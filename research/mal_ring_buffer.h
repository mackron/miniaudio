// Consider this code public domain.
//
// This is some research into a ring buffer implementation. Requirements:
// - Lock free (assuming single producer, single consumer)
// - Support for interleaved and deinterleaved streams
// - Must allow the caller to allocate their own block of memory
//   - Buffers allocated internally must be aligned to MAL_SIMD_ALIGNMENT

// USAGE
//
// - Call mal_rb_init() to initialize a simple buffer, with an optional pre-allocated buffer. If you pass in NULL
//   for the pre-allocated buffer, it will be allocated for you and free()'d in mal_rb_uninit(). If you pass in
//   your own pre-allocated buffer, free()-ing is left to you.
//
// - Call mal_rb_init_ex() if you need a deinterleaved buffer. The data for each sub-buffer is offset from each
//   other based on the stride. Use mal_rb_get_subbuffer_stride(), mal_rb_get_subbuffer_offset() and
//   mal_rb_get_subbuffer_ptr() to manage your sub-buffers.
//
// - Use mal_rb_acquire_read() and mal_rb_acquire_write() to retrieve a pointer to a section of the ring buffer.
//   You specify the number of bytes you need, and on output it will set to what was actually acquired. If the
//   read or write pointer is positioned such that the number of bytes requested will require a loop, it will be
//   clamped to the end of the buffer. Therefore, the number of bytes you're given may be less than the number
//   you requested.
//
// - After calling mal_rb_acquire_read/write(), you do your work on the buffer and then "commit" it with
//   mal_rb_commit_read/write(). This is where the read/write pointers are updated. When you commit you need to
//   pass in the buffer that was returned by the earlier call to mal_rb_acquire_read/write() and is only used
//   for validation. The number of bytes passed to mal_rb_commit_read/write() is what's used to increment the
//   pointers.
//
// - If you want to correct for drift between the write pointer and the read pointer you can use a combination
//   of mal_rb_pointer_distance(), mal_rb_seek_read() and mal_rb_seek_write(). Note that you can only move the
//   pointers forward, and you should only move the read pointer forward via the consumer thread, and the write
//   pointer forward by the producer thread. If there is too much space between the pointers, move the read
//   pointer forward. If there is too little space between the pointers, move the write pointer forward.

// NOTES
//
// - Probably buggy.
// - Still experimenting with the API. Open to suggestions.
// - Thread safety depends on a single producer, single consumer model. Only one thread is allowed to write, and
//   only one thread is allowed to read. The producer is the only one allowed to move the write pointer, and the
//   consumer is the only one allowed to move the read pointer.
// - Thread safety not fully tested - may even be completely broken.
// - Operates on bytes. May end up adding to higher level helpers for doing everything per audio frame.
// - Maximum buffer size is 0x7FFFFFFF-(MAL_SIMD_ALIGNMENT-1) because of reasons.

#ifndef mal_ring_buffer_h
#define mal_ring_buffer_h

typedef struct
{
    void* pBuffer;
    mal_uint32 subbufferSizeInBytes;
    mal_uint32 subbufferCount;
    mal_uint32 subbufferStrideInBytes;
    volatile mal_uint32 readOffset;     /* Most significant bit is the loop flag. Lower 31 bits contains the actual offset in bytes. */
    volatile mal_uint32 writeOffset;    /* Most significant bit is the loop flag. Lower 31 bits contains the actual offset in bytes. */
    mal_bool32 ownsBuffer          : 1; /* Used to know whether or not mini_al is responsible for free()-ing the buffer. */
    mal_bool32 clearOnWriteAcquire : 1; /* When set, clears the acquired write buffer before returning from mal_rb_acquire_write(). */
} mal_rb;

mal_result mal_rb_init_ex(size_t subbufferSizeInBytes, size_t subbufferCount, size_t subbufferStrideInBytes, void* pOptionalPreallocatedBuffer, mal_rb* pRB);
mal_result mal_rb_init(size_t bufferSizeInBytes, void* pOptionalPreallocatedBuffer, mal_rb* pRB);
void mal_rb_uninit(mal_rb* pRB);
mal_result mal_rb_acquire_read(mal_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut);
mal_result mal_rb_commit_read(mal_rb* pRB, size_t sizeInBytes, void* pBufferOut);
mal_result mal_rb_acquire_write(mal_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut);
mal_result mal_rb_commit_write(mal_rb* pRB, size_t sizeInBytes, void* pBufferOut);
mal_result mal_rb_seek_read(mal_rb* pRB, size_t offsetInBytes);
mal_result mal_rb_seek_write(mal_rb* pRB, size_t offsetInBytes);
mal_int32 mal_rb_pointer_distance(mal_rb* pRB);    /* Returns the distance between the write pointer and the read pointer. Should never be negative for a correct program. */
size_t mal_rb_get_subbuffer_stride(mal_rb* pRB);
size_t mal_rb_get_subbuffer_offset(mal_rb* pRB, size_t subbufferIndex);
void* mal_rb_get_subbuffer_ptr(mal_rb* pRB, size_t subbufferIndex, void* pBuffer);

#endif  // mal_ring_buffer_h

#ifdef MINI_AL_IMPLEMENTATION
void* mal_rb__get_read_ptr(mal_rb* pRB)
{
    mal_assert(pRB != NULL);
    return mal_offset_ptr(pRB->pBuffer, (pRB->readOffset & 0x7FFFFFFF));
}

void* mal_rb__get_write_ptr(mal_rb* pRB)
{
    mal_assert(pRB != NULL);
    return mal_offset_ptr(pRB->pBuffer, (pRB->writeOffset & 0x7FFFFFFF));
}

mal_uint32 mal_rb__construct_offset(mal_uint32 offsetInBytes, mal_uint32 offsetLoopFlag)
{
    return offsetLoopFlag | offsetInBytes;
}

void mal_rb__deconstruct_offset(mal_uint32 offset, mal_uint32* pOffsetInBytes, mal_uint32* pOffsetLoopFlag)
{
    mal_assert(pOffsetInBytes != NULL);
    mal_assert(pOffsetLoopFlag != NULL);

    *pOffsetInBytes  = offset & 0x7FFFFFFF;
    *pOffsetLoopFlag = offset & 0x80000000;
}


mal_result mal_rb_init_ex(size_t subbufferSizeInBytes, size_t subbufferCount, size_t subbufferStrideInBytes, void* pOptionalPreallocatedBuffer, mal_rb* pRB)
{
    if (pRB == NULL) {
        return MAL_INVALID_ARGS;
    }

    if (subbufferSizeInBytes == 0 || subbufferCount == 0) {
        return MAL_INVALID_ARGS;
    }

    const mal_uint32 maxSubBufferSize = 0x7FFFFFFF - (MAL_SIMD_ALIGNMENT-1);
    if (subbufferSizeInBytes > maxSubBufferSize) {
        return MAL_INVALID_ARGS;    // Maximum buffer size is ~2GB. The most significant bit is a flag for use internally.
    }


    mal_zero_object(pRB);
    pRB->subbufferSizeInBytes = (mal_uint32)subbufferSizeInBytes;
    pRB->subbufferCount = (mal_uint32)subbufferCount;

    if (pOptionalPreallocatedBuffer != NULL) {
        pRB->subbufferStrideInBytes = (mal_uint32)subbufferStrideInBytes;
        pRB->pBuffer = pOptionalPreallocatedBuffer;
    } else {
        // Here is where we allocate our own buffer. We always want to align this to MAL_SIMD_ALIGNMENT for future SIMD optimization opportunity. To do this
        // we need to make sure the stride is a multiple of MAL_SIMD_ALIGNMENT.
        pRB->subbufferStrideInBytes = (pRB->subbufferSizeInBytes + (MAL_SIMD_ALIGNMENT-1)) & ~MAL_SIMD_ALIGNMENT;

        size_t bufferSizeInBytes = (size_t)pRB->subbufferCount*pRB->subbufferStrideInBytes;
        pRB->pBuffer = mal_aligned_malloc(bufferSizeInBytes, MAL_SIMD_ALIGNMENT);
        if (pRB->pBuffer == NULL) {
            return MAL_OUT_OF_MEMORY;
        }

        mal_zero_memory(pRB->pBuffer, bufferSizeInBytes);
        pRB->ownsBuffer = MAL_TRUE;
    }

    return MAL_SUCCESS;
}

mal_result mal_rb_init(size_t bufferSizeInBytes, void* pOptionalPreallocatedBuffer, mal_rb* pRB)
{
    return mal_rb_init_ex(bufferSizeInBytes, 1, 0, pOptionalPreallocatedBuffer, pRB);
}

void mal_rb_uninit(mal_rb* pRB)
{
    if (pRB == NULL) {
        return;
    }

    if (pRB->ownsBuffer) {
        mal_free(pRB->pBuffer);
    }
}

mal_result mal_rb_acquire_read(mal_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut)
{
    if (pRB == NULL || pSizeInBytes == NULL || ppBufferOut == NULL) {
        return MAL_INVALID_ARGS;
    }

    // The returned buffer should never move ahead of the write pointer.
    mal_uint32 writeOffset = pRB->writeOffset;
    mal_uint32 writeOffsetInBytes;
    mal_uint32 writeOffsetLoopFlag;
    mal_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    mal_uint32 readOffset = pRB->readOffset;
    mal_uint32 readOffsetInBytes;
    mal_uint32 readOffsetLoopFlag;
    mal_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    // The number of bytes available depends on whether or not the read and write pointers are on the same loop iteration. If so, we
    // can only read up to the write pointer. If not, we can only read up to the end of the buffer.
    size_t bytesAvailable;
    if (readOffsetLoopFlag == writeOffsetLoopFlag) {
        bytesAvailable = writeOffsetInBytes - readOffsetInBytes;
    } else {
        bytesAvailable = pRB->subbufferSizeInBytes - readOffsetInBytes;
    }

    size_t bytesRequested = *pSizeInBytes;
    if (bytesRequested > bytesAvailable) {
        bytesRequested = bytesAvailable;
    }

    *pSizeInBytes = bytesRequested;
    (*ppBufferOut) = mal_rb__get_read_ptr(pRB);

    return MAL_SUCCESS;
}

mal_result mal_rb_commit_read(mal_rb* pRB, size_t sizeInBytes, void* pBufferOut)
{
    if (pRB == NULL) {
        return MAL_INVALID_ARGS;
    }

    // Validate the buffer.
    if (pBufferOut != mal_rb__get_read_ptr(pRB)) {
        return MAL_INVALID_ARGS;
    }

    mal_uint32 readOffset = pRB->readOffset;
    mal_uint32 readOffsetInBytes;
    mal_uint32 readOffsetLoopFlag;
    mal_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    // Check that sizeInBytes is correct. It should never go beyond the end of the buffer.
    mal_uint32 newReadOffsetInBytes = (mal_uint32)(readOffsetInBytes + sizeInBytes);
    if (newReadOffsetInBytes > pRB->subbufferSizeInBytes) {
        return MAL_INVALID_ARGS;    // <-- sizeInBytes will cause the read offset to overflow.
    }

    // Move the read pointer back to the start if necessary.
    mal_uint32 newReadOffsetLoopFlag = readOffsetLoopFlag;
    if (newReadOffsetInBytes == pRB->subbufferSizeInBytes) {
        newReadOffsetInBytes = 0;
        newReadOffsetLoopFlag ^= 0x80000000;
    }

    mal_atomic_exchange_32(&pRB->readOffset, mal_rb__construct_offset(newReadOffsetLoopFlag, newReadOffsetInBytes));
    return MAL_SUCCESS;
}

mal_result mal_rb_acquire_write(mal_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut)
{
    if (pRB == NULL || pSizeInBytes == NULL || ppBufferOut == NULL) {
        return MAL_INVALID_ARGS;
    }

    // The returned buffer should never overtake the read buffer.
    mal_uint32 readOffset = pRB->readOffset;
    mal_uint32 readOffsetInBytes;
    mal_uint32 readOffsetLoopFlag;
    mal_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    mal_uint32 writeOffset = pRB->writeOffset;
    mal_uint32 writeOffsetInBytes;
    mal_uint32 writeOffsetLoopFlag;
    mal_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    // In the case of writing, if the write pointer and the read pointer are on the same loop iteration we can only
    // write up to the end of the buffer. Otherwise we can only write up to the read pointer. The write pointer should
    // never overtake the read pointer.
    size_t bytesAvailable;
    if (writeOffsetLoopFlag == readOffsetLoopFlag) {
        bytesAvailable = pRB->subbufferSizeInBytes - writeOffsetInBytes;
    } else {
        bytesAvailable = readOffsetInBytes - writeOffsetInBytes;
    }

    size_t bytesRequested = *pSizeInBytes;
    if (bytesRequested > bytesAvailable) {
        bytesRequested = bytesAvailable;
    }

    *pSizeInBytes = bytesRequested;
    *ppBufferOut  = mal_rb__get_write_ptr(pRB);

    // Clear the buffer if desired.
    if (pRB->clearOnWriteAcquire) {
        mal_zero_memory(*ppBufferOut, *pSizeInBytes);
    }

    return MAL_SUCCESS;
}

mal_result mal_rb_commit_write(mal_rb* pRB, size_t sizeInBytes, void* pBufferOut)
{
    if (pRB == NULL) {
        return MAL_INVALID_ARGS;
    }

    // Validate the buffer.
    if (pBufferOut != mal_rb__get_write_ptr(pRB)) {
        return MAL_INVALID_ARGS;
    }

    mal_uint32 writeOffset = pRB->writeOffset;
    mal_uint32 writeOffsetInBytes;
    mal_uint32 writeOffsetLoopFlag;
    mal_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    // Check that sizeInBytes is correct. It should never go beyond the end of the buffer.
    mal_uint32 newWriteOffsetInBytes = (mal_uint32)(writeOffsetInBytes + sizeInBytes);
    if (newWriteOffsetInBytes > pRB->subbufferSizeInBytes) {
        return MAL_INVALID_ARGS;    // <-- sizeInBytes will cause the read offset to overflow.
    }

    // Move the read pointer back to the start if necessary.
    mal_uint32 newWriteOffsetLoopFlag = writeOffsetLoopFlag;
    if (newWriteOffsetInBytes == pRB->subbufferSizeInBytes) {
        newWriteOffsetInBytes = 0;
        newWriteOffsetLoopFlag ^= 0x80000000;
    }

    mal_atomic_exchange_32(&pRB->writeOffset, mal_rb__construct_offset(newWriteOffsetLoopFlag, newWriteOffsetInBytes));
    return MAL_SUCCESS;
}

mal_result mal_rb_seek_read(mal_rb* pRB, size_t offsetInBytes)
{
    if (pRB == NULL || offsetInBytes > pRB->subbufferSizeInBytes) {
        return MAL_INVALID_ARGS;
    }

    mal_uint32 readOffset = pRB->readOffset;
    mal_uint32 readOffsetInBytes;
    mal_uint32 readOffsetLoopFlag;
    mal_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    mal_uint32 writeOffset = pRB->writeOffset;
    mal_uint32 writeOffsetInBytes;
    mal_uint32 writeOffsetLoopFlag;
    mal_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    mal_uint32 newReadOffsetInBytes = readOffsetInBytes;
    mal_uint32 newReadOffsetLoopFlag = readOffsetLoopFlag;

    // We cannot go past the write buffer.
    if (readOffsetLoopFlag == writeOffsetLoopFlag) {
        if ((readOffsetInBytes + offsetInBytes) > writeOffsetInBytes) {
            newReadOffsetInBytes = writeOffsetInBytes;
        } else {
            newReadOffsetInBytes = (mal_uint32)(readOffsetInBytes + offsetInBytes);
        }
    } else {
        // May end up looping.
        if ((readOffsetInBytes + offsetInBytes) >= pRB->subbufferSizeInBytes) {
            newReadOffsetInBytes = (mal_uint32)(readOffsetInBytes + offsetInBytes) - pRB->subbufferSizeInBytes;
            newReadOffsetLoopFlag ^= 0x80000000;    /* <-- Looped. */
        } else {
            newReadOffsetInBytes = (mal_uint32)(readOffsetInBytes + offsetInBytes);
        }
    }

    mal_atomic_exchange_32(&pRB->readOffset, mal_rb__construct_offset(newReadOffsetInBytes, newReadOffsetLoopFlag));
    return MAL_SUCCESS;
}

mal_result mal_rb_seek_write(mal_rb* pRB, size_t offsetInBytes)
{
    if (pRB == NULL) {
        return MAL_INVALID_ARGS;
    }

    mal_uint32 readOffset = pRB->readOffset;
    mal_uint32 readOffsetInBytes;
    mal_uint32 readOffsetLoopFlag;
    mal_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    mal_uint32 writeOffset = pRB->writeOffset;
    mal_uint32 writeOffsetInBytes;
    mal_uint32 writeOffsetLoopFlag;
    mal_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    mal_uint32 newWriteOffsetInBytes = writeOffsetInBytes;
    mal_uint32 newWriteOffsetLoopFlag = writeOffsetLoopFlag;

    // We cannot go past the write buffer.
    if (readOffsetLoopFlag == writeOffsetLoopFlag) {
        // May end up looping.
        if ((writeOffsetInBytes + offsetInBytes) >= pRB->subbufferSizeInBytes) {
            newWriteOffsetInBytes = (mal_uint32)(writeOffsetInBytes + offsetInBytes) - pRB->subbufferSizeInBytes;
            newWriteOffsetLoopFlag ^= 0x80000000;    /* <-- Looped. */
        } else {
            newWriteOffsetInBytes = (mal_uint32)(writeOffsetInBytes + offsetInBytes);
        }
    } else {
        if ((writeOffsetInBytes + offsetInBytes) > readOffsetInBytes) {
            newWriteOffsetInBytes = readOffsetInBytes;
        } else {
            newWriteOffsetInBytes = (mal_uint32)(writeOffsetInBytes + offsetInBytes);
        }
    }

    mal_atomic_exchange_32(&pRB->writeOffset, mal_rb__construct_offset(newWriteOffsetInBytes, newWriteOffsetLoopFlag));
    return MAL_SUCCESS;
}

mal_int32 mal_rb_pointer_distance(mal_rb* pRB)
{
    if (pRB == NULL) {
        return 0;
    }

    mal_uint32 readOffset = pRB->readOffset;
    mal_uint32 readOffsetInBytes;
    mal_uint32 readOffsetLoopFlag;
    mal_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    mal_uint32 writeOffset = pRB->writeOffset;
    mal_uint32 writeOffsetInBytes;
    mal_uint32 writeOffsetLoopFlag;
    mal_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    if (readOffsetLoopFlag == writeOffsetLoopFlag) {
        return writeOffsetInBytes - readOffsetInBytes;
    } else {
        return writeOffsetInBytes + (pRB->subbufferSizeInBytes - readOffsetInBytes);
    }
}

size_t mal_rb_get_subbuffer_stride(mal_rb* pRB)
{
    if (pRB == NULL) {
        return 0;
    }

    if (pRB->subbufferStrideInBytes == 0) {
        return (size_t)pRB->subbufferSizeInBytes;
    }

    return (size_t)pRB->subbufferStrideInBytes;
}

size_t mal_rb_get_subbuffer_offset(mal_rb* pRB, size_t subbufferIndex)
{
    if (pRB == NULL) {
        return 0;
    }

    return subbufferIndex * mal_rb_get_subbuffer_stride(pRB);
}

void* mal_rb_get_subbuffer_ptr(mal_rb* pRB, size_t subbufferIndex, void* pBuffer)
{
    if (pRB == NULL) {
        return NULL;
    }

    return mal_offset_ptr(pBuffer, mal_rb_get_subbuffer_offset(pRB, subbufferIndex));
}
#endif  // MINI_AL_IMPLEMENTATION
