// Consider this code public domain.
//
// This is some research into a ring buffer implementation. Requirements:
// - Lock free (assuming single producer, single consumer)
// - Support for interleaved and deinterleaved streams
// - Must allow the caller to allocate their own block of memory
//   - Buffers allocated internally must be aligned to MA_SIMD_ALIGNMENT

// USAGE
//
// - Call ma_rb_init() to initialize a simple buffer, with an optional pre-allocated buffer. If you pass in NULL
//   for the pre-allocated buffer, it will be allocated for you and free()'d in ma_rb_uninit(). If you pass in
//   your own pre-allocated buffer, free()-ing is left to you.
//
// - Call ma_rb_init_ex() if you need a deinterleaved buffer. The data for each sub-buffer is offset from each
//   other based on the stride. Use ma_rb_get_subbuffer_stride(), ma_rb_get_subbuffer_offset() and
//   ma_rb_get_subbuffer_ptr() to manage your sub-buffers.
//
// - Use ma_rb_acquire_read() and ma_rb_acquire_write() to retrieve a pointer to a section of the ring buffer.
//   You specify the number of bytes you need, and on output it will set to what was actually acquired. If the
//   read or write pointer is positioned such that the number of bytes requested will require a loop, it will be
//   clamped to the end of the buffer. Therefore, the number of bytes you're given may be less than the number
//   you requested.
//
// - After calling ma_rb_acquire_read/write(), you do your work on the buffer and then "commit" it with
//   ma_rb_commit_read/write(). This is where the read/write pointers are updated. When you commit you need to
//   pass in the buffer that was returned by the earlier call to ma_rb_acquire_read/write() and is only used
//   for validation. The number of bytes passed to ma_rb_commit_read/write() is what's used to increment the
//   pointers.
//
// - If you want to correct for drift between the write pointer and the read pointer you can use a combination
//   of ma_rb_pointer_distance(), ma_rb_seek_read() and ma_rb_seek_write(). Note that you can only move the
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
// - Maximum buffer size is 0x7FFFFFFF-(MA_SIMD_ALIGNMENT-1) because of reasons.

#ifndef ma_ring_buffer_h
#define ma_ring_buffer_h

typedef struct
{
    void* pBuffer;
    ma_uint32 subbufferSizeInBytes;
    ma_uint32 subbufferCount;
    ma_uint32 subbufferStrideInBytes;
    volatile ma_uint32 encodedReadOffset;  /* Most significant bit is the loop flag. Lower 31 bits contains the actual offset in bytes. */
    volatile ma_uint32 encodedWriteOffset; /* Most significant bit is the loop flag. Lower 31 bits contains the actual offset in bytes. */
    ma_bool32 ownsBuffer          : 1;     /* Used to know whether or not miniaudio is responsible for free()-ing the buffer. */
    ma_bool32 clearOnWriteAcquire : 1;     /* When set, clears the acquired write buffer before returning from ma_rb_acquire_write(). */
} ma_rb;

ma_result ma_rb_init_ex(size_t subbufferSizeInBytes, size_t subbufferCount, size_t subbufferStrideInBytes, void* pOptionalPreallocatedBuffer, ma_rb* pRB);
ma_result ma_rb_init(size_t bufferSizeInBytes, void* pOptionalPreallocatedBuffer, ma_rb* pRB);
void ma_rb_uninit(ma_rb* pRB);
ma_result ma_rb_acquire_read(ma_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut);
ma_result ma_rb_commit_read(ma_rb* pRB, size_t sizeInBytes, void* pBufferOut);
ma_result ma_rb_acquire_write(ma_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut);
ma_result ma_rb_commit_write(ma_rb* pRB, size_t sizeInBytes, void* pBufferOut);
ma_result ma_rb_seek_read(ma_rb* pRB, size_t offsetInBytes);
ma_result ma_rb_seek_write(ma_rb* pRB, size_t offsetInBytes);
ma_int32 ma_rb_pointer_distance(ma_rb* pRB);    /* Returns the distance between the write pointer and the read pointer. Should never be negative for a correct program. */
size_t ma_rb_get_subbuffer_stride(ma_rb* pRB);
size_t ma_rb_get_subbuffer_offset(ma_rb* pRB, size_t subbufferIndex);
void* ma_rb_get_subbuffer_ptr(ma_rb* pRB, size_t subbufferIndex, void* pBuffer);


typedef struct
{
    ma_rb rb;
    ma_format format;
    ma_uint32 channels;
} ma_pcm_rb;

ma_result ma_pcm_rb_init_ex(ma_format format, ma_uint32 channels, size_t subbufferSizeInFrames, size_t subbufferCount, size_t subbufferStrideInFrames, void* pOptionalPreallocatedBuffer, ma_pcm_rb* pRB);
ma_result ma_pcm_rb_init(ma_format format, ma_uint32 channels, size_t bufferSizeInFrames, void* pOptionalPreallocatedBuffer, ma_pcm_rb* pRB);
void ma_pcm_rb_uninit(ma_pcm_rb* pRB);
ma_result ma_pcm_rb_acquire_read(ma_pcm_rb* pRB, size_t* pSizeInFrames, void** ppBufferOut);
ma_result ma_pcm_rb_commit_read(ma_pcm_rb* pRB, size_t sizeInFrames, void* pBufferOut);
ma_result ma_pcm_rb_acquire_write(ma_pcm_rb* pRB, size_t* pSizeInFrames, void** ppBufferOut);
ma_result ma_pcm_rb_commit_write(ma_pcm_rb* pRB, size_t sizeInFrames, void* pBufferOut);
ma_result ma_pcm_rb_seek_read(ma_pcm_rb* pRB, size_t offsetInFrames);
ma_result ma_pcm_rb_seek_write(ma_pcm_rb* pRB, size_t offsetInFrames);
ma_int32 ma_pcm_rb_pointer_disance(ma_pcm_rb* pRB); /* Return value is in frames. */
size_t ma_pcm_rb_get_subbuffer_stride(ma_pcm_rb* pRB);
size_t ma_pcm_rb_get_subbuffer_offset(ma_pcm_rb* pRB, size_t subbufferIndex);
void* ma_pcm_rb_get_subbuffer_ptr(ma_pcm_rb* pRB, size_t subbufferIndex, void* pBuffer);


#endif  // ma_ring_buffer_h

#ifdef MINIAUDIO_IMPLEMENTATION
MA_INLINE ma_uint32 ma_rb__extract_offset_in_bytes(ma_uint32 encodedOffset)
{
    return encodedOffset & 0x7FFFFFFF;
}

MA_INLINE ma_uint32 ma_rb__extract_offset_loop_flag(ma_uint32 encodedOffset)
{
    return encodedOffset & 0x80000000;
}

MA_INLINE void* ma_rb__get_read_ptr(ma_rb* pRB)
{
    ma_assert(pRB != NULL);
    return ma_offset_ptr(pRB->pBuffer, ma_rb__extract_offset_in_bytes(pRB->encodedReadOffset));
}

MA_INLINE void* ma_rb__get_write_ptr(ma_rb* pRB)
{
    ma_assert(pRB != NULL);
    return ma_offset_ptr(pRB->pBuffer, ma_rb__extract_offset_in_bytes(pRB->encodedWriteOffset));
}

MA_INLINE ma_uint32 ma_rb__construct_offset(ma_uint32 offsetInBytes, ma_uint32 offsetLoopFlag)
{
    return offsetLoopFlag | offsetInBytes;
}

MA_INLINE void ma_rb__deconstruct_offset(ma_uint32 encodedOffset, ma_uint32* pOffsetInBytes, ma_uint32* pOffsetLoopFlag)
{
    ma_assert(pOffsetInBytes != NULL);
    ma_assert(pOffsetLoopFlag != NULL);

    *pOffsetInBytes  = ma_rb__extract_offset_in_bytes(encodedOffset);
    *pOffsetLoopFlag = ma_rb__extract_offset_loop_flag(encodedOffset);
}


ma_result ma_rb_init_ex(size_t subbufferSizeInBytes, size_t subbufferCount, size_t subbufferStrideInBytes, void* pOptionalPreallocatedBuffer, ma_rb* pRB)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    if (subbufferSizeInBytes == 0 || subbufferCount == 0) {
        return MA_INVALID_ARGS;
    }

    const ma_uint32 maxSubBufferSize = 0x7FFFFFFF - (MA_SIMD_ALIGNMENT-1);
    if (subbufferSizeInBytes > maxSubBufferSize) {
        return MA_INVALID_ARGS;    // Maximum buffer size is ~2GB. The most significant bit is a flag for use internally.
    }


    ma_zero_object(pRB);
    pRB->subbufferSizeInBytes = (ma_uint32)subbufferSizeInBytes;
    pRB->subbufferCount = (ma_uint32)subbufferCount;

    if (pOptionalPreallocatedBuffer != NULL) {
        pRB->subbufferStrideInBytes = (ma_uint32)subbufferStrideInBytes;
        pRB->pBuffer = pOptionalPreallocatedBuffer;
    } else {
        // Here is where we allocate our own buffer. We always want to align this to MA_SIMD_ALIGNMENT for future SIMD optimization opportunity. To do this
        // we need to make sure the stride is a multiple of MA_SIMD_ALIGNMENT.
        pRB->subbufferStrideInBytes = (pRB->subbufferSizeInBytes + (MA_SIMD_ALIGNMENT-1)) & ~MA_SIMD_ALIGNMENT;

        size_t bufferSizeInBytes = (size_t)pRB->subbufferCount*pRB->subbufferStrideInBytes;
        pRB->pBuffer = ma_aligned_malloc(bufferSizeInBytes, MA_SIMD_ALIGNMENT);
        if (pRB->pBuffer == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        ma_zero_memory(pRB->pBuffer, bufferSizeInBytes);
        pRB->ownsBuffer = MA_TRUE;
    }

    return MA_SUCCESS;
}

ma_result ma_rb_init(size_t bufferSizeInBytes, void* pOptionalPreallocatedBuffer, ma_rb* pRB)
{
    return ma_rb_init_ex(bufferSizeInBytes, 1, 0, pOptionalPreallocatedBuffer, pRB);
}

void ma_rb_uninit(ma_rb* pRB)
{
    if (pRB == NULL) {
        return;
    }

    if (pRB->ownsBuffer) {
        ma_free(pRB->pBuffer);
    }
}

ma_result ma_rb_acquire_read(ma_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut)
{
    if (pRB == NULL || pSizeInBytes == NULL || ppBufferOut == NULL) {
        return MA_INVALID_ARGS;
    }

    // The returned buffer should never move ahead of the write pointer.
    ma_uint32 writeOffset = pRB->encodedWriteOffset;
    ma_uint32 writeOffsetInBytes;
    ma_uint32 writeOffsetLoopFlag;
    ma_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    ma_uint32 readOffset = pRB->encodedReadOffset;
    ma_uint32 readOffsetInBytes;
    ma_uint32 readOffsetLoopFlag;
    ma_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

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
    (*ppBufferOut) = ma_rb__get_read_ptr(pRB);

    return MA_SUCCESS;
}

ma_result ma_rb_commit_read(ma_rb* pRB, size_t sizeInBytes, void* pBufferOut)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    // Validate the buffer.
    if (pBufferOut != ma_rb__get_read_ptr(pRB)) {
        return MA_INVALID_ARGS;
    }

    ma_uint32 readOffset = pRB->encodedReadOffset;
    ma_uint32 readOffsetInBytes;
    ma_uint32 readOffsetLoopFlag;
    ma_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    // Check that sizeInBytes is correct. It should never go beyond the end of the buffer.
    ma_uint32 newReadOffsetInBytes = (ma_uint32)(readOffsetInBytes + sizeInBytes);
    if (newReadOffsetInBytes > pRB->subbufferSizeInBytes) {
        return MA_INVALID_ARGS;    // <-- sizeInBytes will cause the read offset to overflow.
    }

    // Move the read pointer back to the start if necessary.
    ma_uint32 newReadOffsetLoopFlag = readOffsetLoopFlag;
    if (newReadOffsetInBytes == pRB->subbufferSizeInBytes) {
        newReadOffsetInBytes = 0;
        newReadOffsetLoopFlag ^= 0x80000000;
    }

    ma_atomic_exchange_32(&pRB->encodedReadOffset, ma_rb__construct_offset(newReadOffsetLoopFlag, newReadOffsetInBytes));
    return MA_SUCCESS;
}

ma_result ma_rb_acquire_write(ma_rb* pRB, size_t* pSizeInBytes, void** ppBufferOut)
{
    if (pRB == NULL || pSizeInBytes == NULL || ppBufferOut == NULL) {
        return MA_INVALID_ARGS;
    }

    // The returned buffer should never overtake the read buffer.
    ma_uint32 readOffset = pRB->encodedReadOffset;
    ma_uint32 readOffsetInBytes;
    ma_uint32 readOffsetLoopFlag;
    ma_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    ma_uint32 writeOffset = pRB->encodedWriteOffset;
    ma_uint32 writeOffsetInBytes;
    ma_uint32 writeOffsetLoopFlag;
    ma_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

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
    *ppBufferOut  = ma_rb__get_write_ptr(pRB);

    // Clear the buffer if desired.
    if (pRB->clearOnWriteAcquire) {
        ma_zero_memory(*ppBufferOut, *pSizeInBytes);
    }

    return MA_SUCCESS;
}

ma_result ma_rb_commit_write(ma_rb* pRB, size_t sizeInBytes, void* pBufferOut)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    // Validate the buffer.
    if (pBufferOut != ma_rb__get_write_ptr(pRB)) {
        return MA_INVALID_ARGS;
    }

    ma_uint32 writeOffset = pRB->encodedWriteOffset;
    ma_uint32 writeOffsetInBytes;
    ma_uint32 writeOffsetLoopFlag;
    ma_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    // Check that sizeInBytes is correct. It should never go beyond the end of the buffer.
    ma_uint32 newWriteOffsetInBytes = (ma_uint32)(writeOffsetInBytes + sizeInBytes);
    if (newWriteOffsetInBytes > pRB->subbufferSizeInBytes) {
        return MA_INVALID_ARGS;    // <-- sizeInBytes will cause the read offset to overflow.
    }

    // Move the read pointer back to the start if necessary.
    ma_uint32 newWriteOffsetLoopFlag = writeOffsetLoopFlag;
    if (newWriteOffsetInBytes == pRB->subbufferSizeInBytes) {
        newWriteOffsetInBytes = 0;
        newWriteOffsetLoopFlag ^= 0x80000000;
    }

    ma_atomic_exchange_32(&pRB->encodedWriteOffset, ma_rb__construct_offset(newWriteOffsetLoopFlag, newWriteOffsetInBytes));
    return MA_SUCCESS;
}

ma_result ma_rb_seek_read(ma_rb* pRB, size_t offsetInBytes)
{
    if (pRB == NULL || offsetInBytes > pRB->subbufferSizeInBytes) {
        return MA_INVALID_ARGS;
    }

    ma_uint32 readOffset = pRB->encodedReadOffset;
    ma_uint32 readOffsetInBytes;
    ma_uint32 readOffsetLoopFlag;
    ma_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    ma_uint32 writeOffset = pRB->encodedWriteOffset;
    ma_uint32 writeOffsetInBytes;
    ma_uint32 writeOffsetLoopFlag;
    ma_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    ma_uint32 newReadOffsetInBytes = readOffsetInBytes;
    ma_uint32 newReadOffsetLoopFlag = readOffsetLoopFlag;

    // We cannot go past the write buffer.
    if (readOffsetLoopFlag == writeOffsetLoopFlag) {
        if ((readOffsetInBytes + offsetInBytes) > writeOffsetInBytes) {
            newReadOffsetInBytes = writeOffsetInBytes;
        } else {
            newReadOffsetInBytes = (ma_uint32)(readOffsetInBytes + offsetInBytes);
        }
    } else {
        // May end up looping.
        if ((readOffsetInBytes + offsetInBytes) >= pRB->subbufferSizeInBytes) {
            newReadOffsetInBytes = (ma_uint32)(readOffsetInBytes + offsetInBytes) - pRB->subbufferSizeInBytes;
            newReadOffsetLoopFlag ^= 0x80000000;    /* <-- Looped. */
        } else {
            newReadOffsetInBytes = (ma_uint32)(readOffsetInBytes + offsetInBytes);
        }
    }

    ma_atomic_exchange_32(&pRB->encodedReadOffset, ma_rb__construct_offset(newReadOffsetInBytes, newReadOffsetLoopFlag));
    return MA_SUCCESS;
}

ma_result ma_rb_seek_write(ma_rb* pRB, size_t offsetInBytes)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_uint32 readOffset = pRB->encodedReadOffset;
    ma_uint32 readOffsetInBytes;
    ma_uint32 readOffsetLoopFlag;
    ma_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    ma_uint32 writeOffset = pRB->encodedWriteOffset;
    ma_uint32 writeOffsetInBytes;
    ma_uint32 writeOffsetLoopFlag;
    ma_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    ma_uint32 newWriteOffsetInBytes = writeOffsetInBytes;
    ma_uint32 newWriteOffsetLoopFlag = writeOffsetLoopFlag;

    // We cannot go past the write buffer.
    if (readOffsetLoopFlag == writeOffsetLoopFlag) {
        // May end up looping.
        if ((writeOffsetInBytes + offsetInBytes) >= pRB->subbufferSizeInBytes) {
            newWriteOffsetInBytes = (ma_uint32)(writeOffsetInBytes + offsetInBytes) - pRB->subbufferSizeInBytes;
            newWriteOffsetLoopFlag ^= 0x80000000;    /* <-- Looped. */
        } else {
            newWriteOffsetInBytes = (ma_uint32)(writeOffsetInBytes + offsetInBytes);
        }
    } else {
        if ((writeOffsetInBytes + offsetInBytes) > readOffsetInBytes) {
            newWriteOffsetInBytes = readOffsetInBytes;
        } else {
            newWriteOffsetInBytes = (ma_uint32)(writeOffsetInBytes + offsetInBytes);
        }
    }

    ma_atomic_exchange_32(&pRB->encodedWriteOffset, ma_rb__construct_offset(newWriteOffsetInBytes, newWriteOffsetLoopFlag));
    return MA_SUCCESS;
}

ma_int32 ma_rb_pointer_distance(ma_rb* pRB)
{
    if (pRB == NULL) {
        return 0;
    }

    ma_uint32 readOffset = pRB->encodedReadOffset;
    ma_uint32 readOffsetInBytes;
    ma_uint32 readOffsetLoopFlag;
    ma_rb__deconstruct_offset(readOffset, &readOffsetInBytes, &readOffsetLoopFlag);

    ma_uint32 writeOffset = pRB->encodedWriteOffset;
    ma_uint32 writeOffsetInBytes;
    ma_uint32 writeOffsetLoopFlag;
    ma_rb__deconstruct_offset(writeOffset, &writeOffsetInBytes, &writeOffsetLoopFlag);

    if (readOffsetLoopFlag == writeOffsetLoopFlag) {
        return writeOffsetInBytes - readOffsetInBytes;
    } else {
        return writeOffsetInBytes + (pRB->subbufferSizeInBytes - readOffsetInBytes);
    }
}

size_t ma_rb_get_subbuffer_stride(ma_rb* pRB)
{
    if (pRB == NULL) {
        return 0;
    }

    if (pRB->subbufferStrideInBytes == 0) {
        return (size_t)pRB->subbufferSizeInBytes;
    }

    return (size_t)pRB->subbufferStrideInBytes;
}

size_t ma_rb_get_subbuffer_offset(ma_rb* pRB, size_t subbufferIndex)
{
    if (pRB == NULL) {
        return 0;
    }

    return subbufferIndex * ma_rb_get_subbuffer_stride(pRB);
}

void* ma_rb_get_subbuffer_ptr(ma_rb* pRB, size_t subbufferIndex, void* pBuffer)
{
    if (pRB == NULL) {
        return NULL;
    }

    return ma_offset_ptr(pBuffer, ma_rb_get_subbuffer_offset(pRB, subbufferIndex));
}


/* ma_pcm_rb */

ma_uint32 ma_pcm_rb_get_bpf(ma_pcm_rb* pRB)
{
    ma_assert(pRB != NULL);

    return ma_get_bytes_per_frame(pRB->format, pRB->channels);
}

ma_result ma_pcm_rb_init_ex(ma_format format, ma_uint32 channels, size_t subbufferSizeInFrames, size_t subbufferCount, size_t subbufferStrideInFrames, void* pOptionalPreallocatedBuffer, ma_pcm_rb* pRB)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_zero_object(pRB);

    ma_uint32 bpf = ma_get_bytes_per_frame(format, channels);
    if (bpf == 0) {
        return MA_INVALID_ARGS;
    }

    ma_result result = ma_rb_init_ex(subbufferSizeInFrames*bpf, subbufferCount, subbufferStrideInFrames*bpf, pOptionalPreallocatedBuffer, &pRB->rb);
    if (result != MA_SUCCESS) {
        return result;
    }

    pRB->format   = format;
    pRB->channels = channels;

    return MA_SUCCESS;
}

ma_result ma_pcm_rb_init(ma_format format, ma_uint32 channels, size_t bufferSizeInFrames, void* pOptionalPreallocatedBuffer, ma_pcm_rb* pRB)
{
    return ma_pcm_rb_init_ex(format, channels, bufferSizeInFrames, 1, 0, pOptionalPreallocatedBuffer, pRB);
}

void ma_pcm_rb_uninit(ma_pcm_rb* pRB)
{
    if (pRB == NULL) {
        return;
    }

    ma_rb_uninit(&pRB->rb);
}

ma_result ma_pcm_rb_acquire_read(ma_pcm_rb* pRB, size_t* pSizeInFrames, void** ppBufferOut)
{
    size_t sizeInBytes;
    ma_result result;

    if (pRB == NULL || pSizeInFrames == NULL) {
        return MA_INVALID_ARGS;
    }

    sizeInBytes = *pSizeInFrames * ma_pcm_rb_get_bpf(pRB);

    result = ma_rb_acquire_read(&pRB->rb, &sizeInBytes, ppBufferOut);
    if (result != MA_SUCCESS) {
        return result;
    }

    *pSizeInFrames = sizeInBytes / ma_pcm_rb_get_bpf(pRB);
    return MA_SUCCESS;
}

ma_result ma_pcm_rb_commit_read(ma_pcm_rb* pRB, size_t sizeInFrames, void* pBufferOut)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_rb_commit_read(&pRB->rb, sizeInFrames * ma_pcm_rb_get_bpf(pRB), pBufferOut);
}

ma_result ma_pcm_rb_acquire_write(ma_pcm_rb* pRB, size_t* pSizeInFrames, void** ppBufferOut)
{
    size_t sizeInBytes;
    ma_result result;

    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    sizeInBytes = *pSizeInFrames * ma_pcm_rb_get_bpf(pRB);

    result = ma_rb_acquire_write(&pRB->rb, &sizeInBytes, ppBufferOut);
    if (result != MA_SUCCESS) {
        return result;
    }

    *pSizeInFrames = sizeInBytes / ma_pcm_rb_get_bpf(pRB);
    return MA_SUCCESS;
}

ma_result ma_pcm_rb_commit_write(ma_pcm_rb* pRB, size_t sizeInFrames, void* pBufferOut)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_rb_commit_write(&pRB->rb, sizeInFrames * ma_pcm_rb_get_bpf(pRB), pBufferOut);
}

ma_result ma_pcm_rb_seek_read(ma_pcm_rb* pRB, size_t offsetInFrames)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_rb_seek_read(&pRB->rb, offsetInFrames * ma_pcm_rb_get_bpf(pRB));
}

ma_result ma_pcm_rb_seek_write(ma_pcm_rb* pRB, size_t offsetInFrames)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_rb_seek_write(&pRB->rb, offsetInFrames * ma_pcm_rb_get_bpf(pRB));
}

ma_int32 ma_pcm_rb_pointer_disance(ma_pcm_rb* pRB)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_rb_pointer_distance(&pRB->rb) / ma_pcm_rb_get_bpf(pRB);
}

size_t ma_pcm_rb_get_subbuffer_stride(ma_pcm_rb* pRB)
{
    if (pRB == NULL) {
        return 0;
    }

    return ma_rb_get_subbuffer_stride(&pRB->rb) / ma_pcm_rb_get_bpf(pRB);
}

size_t ma_pcm_rb_get_subbuffer_offset(ma_pcm_rb* pRB, size_t subbufferIndex)
{
    if (pRB == NULL) {
        return 0;
    }

    return ma_rb_get_subbuffer_offset(&pRB->rb, subbufferIndex) / ma_pcm_rb_get_bpf(pRB);
}

void* ma_pcm_rb_get_subbuffer_ptr(ma_pcm_rb* pRB, size_t subbufferIndex, void* pBuffer)
{
    if (pRB == NULL) {
        return NULL;
    }

    return ma_rb_get_subbuffer_ptr(&pRB->rb, subbufferIndex, pBuffer);
}

#endif  // MINIAUDIO_IMPLEMENTATION
