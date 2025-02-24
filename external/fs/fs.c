#ifndef fs_c
#define fs_c

#include "fs.h"

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>    /* <-- Just can't get away from this darn thing... Needed for mutexes and file iteration. */

static fs_result fs_result_from_GetLastError(DWORD error)
{
    switch (error)
    {
        case ERROR_SUCCESS:           return FS_SUCCESS;
        case ERROR_NOT_ENOUGH_MEMORY: return FS_OUT_OF_MEMORY;
        case ERROR_BUSY:              return FS_BUSY;
        case ERROR_SEM_TIMEOUT:       return FS_TIMEOUT;
        default: break;
    }

    return FS_ERROR;
}
#endif


/*
This is the maximum number of ureferenced opened archive files that will be kept in memory
before garbage collection of those archives is triggered.
*/
#ifndef FS_DEFAULT_ARCHIVE_GC_THRESHOLD
#define FS_DEFAULT_ARCHIVE_GC_THRESHOLD 10
#endif


#define FS_UNUSED(x) (void)x

#ifndef FS_MALLOC
#define FS_MALLOC(sz) malloc((sz))
#endif

#ifndef FS_REALLOC
#define FS_REALLOC(p, sz) realloc((p), (sz))
#endif

#ifndef FS_FREE
#define FS_FREE(p) free((p))
#endif

static void fs_zero_memory_default(void* p, size_t sz)
{
    if (sz > 0) {
        memset(p, 0, sz);
    }
}

#ifndef FS_ZERO_MEMORY
#define FS_ZERO_MEMORY(p, sz) fs_zero_memory_default((p), (sz))
#endif

#ifndef FS_COPY_MEMORY
#define FS_COPY_MEMORY(dst, src, sz) memcpy((dst), (src), (sz))
#endif

#ifndef FS_MOVE_MEMORY
#define FS_MOVE_MEMORY(dst, src, sz) memmove((dst), (src), (sz))
#endif

#ifndef FS_ASSERT
#define FS_ASSERT(condition) assert(condition)
#endif

#define FS_ZERO_OBJECT(p)           FS_ZERO_MEMORY((p), sizeof(*(p)))
#define FS_COUNTOF(x)               (sizeof(x) / sizeof(x[0]))
#define FS_MAX(x, y)                (((x) > (y)) ? (x) : (y))
#define FS_MIN(x, y)                (((x) < (y)) ? (x) : (y))
#define FS_ABS(x)                   (((x) > 0) ? (x) : -(x))
#define FS_CLAMP(x, lo, hi)         (FS_MAX((lo), FS_MIN((x), (hi))))
#define FS_OFFSET_PTR(p, offset)    (((unsigned char*)(p)) + (offset))
#define FS_ALIGN(x, a)              ((x + (a-1)) & ~(a-1))


FS_API char* fs_strcpy(char* dst, const char* src)
{
    char* dstorig;

    FS_ASSERT(dst != NULL);
    FS_ASSERT(src != NULL);

    dstorig = dst;

    /* No, we're not using this garbage: while (*dst++ = *src++); */
    for (;;) {
        *dst = *src;
        if (*src == '\0') {
            break;
        }

        dst += 1;
        src += 1;
    }

    return dstorig;
}

FS_API int fs_strncpy(char* dst, const char* src, size_t count)
{
    size_t maxcount;
    size_t i;

    if (dst == 0) {
        return EINVAL;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    maxcount = count;

    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
}

FS_API int fs_strcpy_s(char* dst, size_t dstCap, const char* src)
{
    size_t i;

    if (dst == 0) {
        return EINVAL;
    }
    if (dstCap == 0) {
        return ERANGE;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    for (i = 0; i < dstCap && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (i < dstCap) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
}

FS_API int fs_strncpy_s(char* dst, size_t dstCap, const char* src, size_t count)
{
    size_t maxcount;
    size_t i;

    if (dst == 0) {
        return EINVAL;
    }
    if (dstCap == 0) {
        return EINVAL;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    maxcount = count;
    if (count == ((size_t)-1) || count >= dstCap) {        /* -1 = _TRUNCATE */
        maxcount = dstCap - 1;
    }

    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
}

FS_API int fs_strncmp(const char* str1, const char* str2, size_t maxLen)
{
    if (str1 == str2) return  0;

    /* These checks differ from the standard implementation. It's not important, but I prefer it just for sanity. */
    if (str1 == NULL) return -1;
    if (str2 == NULL) return  1;

    /* This function still needs to check for null terminators even though the length has been specified. */
    for (;;) {
        if (maxLen == 0) {
            break;
        }

        if (str1[0] == '\0') {
            break;
        }

        if (str1[0] != str2[0]) {
            break;
        }

        str1 += 1;
        str2 += 1;
        maxLen -= 1;
    }

    if (maxLen == 0) {
        return 0;
    }

    return ((unsigned char*)str1)[0] - ((unsigned char*)str2)[0];
}


FS_API int fs_strnicmp_ascii(const char* str1, const char* str2, size_t count)
{
    if (str1 == NULL || str2 == NULL) {
        return 0;
    }

    while (*str1 != '\0' && *str2 != '\0' && count > 0) {
        int c1 = (int)*str1;
        int c2 = (int)*str2;
    
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 += 'a' - 'A';
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 += 'a' - 'A';
        }
       
        if (c1 != c2) {
            return c1 - c2;
        }
       
        str1  += 1;
        str2  += 1;
        count -= 1;
    }

    if (count == 0) {
        return 0;
    } else if (*str1 == '\0' && *str2 == '\0') {
        return 0;
    } else if (*str1 == '\0') {
        return -1;
    } else {
        return 1;
    }
}

FS_API int fs_strnicmp(const char* str1, const char* str2, size_t count)
{
    /* We will use the standard implementations of strnicmp() and strncasecmp() if they are available. */
#if defined(_MSC_VER) && _MSC_VER >= 1400
    return _strnicmp(str1, str2, count);
#elif defined(__GNUC__) && defined(__USE_GNU)
    return strncasecmp(str1, str2, count);
#else
    /* It would be good if we could use a custom implementation based on the Unicode standard here. Would require a lot of work to get that right, however. */
    return fs_strnicmp_ascii(str1, str2, count);
#endif
}




/* BEG fs_allocation_callbacks.c */
/* Default allocation callbacks. */
static void* fs_malloc_default(size_t sz, void* pUserData)
{
    FS_UNUSED(pUserData);
    return FS_MALLOC(sz);
}

static void* fs_realloc_default(void* p, size_t sz, void* pUserData)
{
    FS_UNUSED(pUserData);
    return FS_REALLOC(p, sz);
}

static void fs_free_default(void* p, void* pUserData)
{
    FS_UNUSED(pUserData);
    FS_FREE(p);
}


static fs_allocation_callbacks fs_allocation_callbacks_init_default(void)
{
    fs_allocation_callbacks allocationCallbacks;

    allocationCallbacks.pUserData = NULL;
    allocationCallbacks.onMalloc  = fs_malloc_default;
    allocationCallbacks.onRealloc = fs_realloc_default;
    allocationCallbacks.onFree    = fs_free_default;

    return allocationCallbacks;
}

static fs_allocation_callbacks fs_allocation_callbacks_init_copy(const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (pAllocationCallbacks != NULL) {
        return *pAllocationCallbacks;
    } else {
        return fs_allocation_callbacks_init_default();
    }
}


FS_API void* fs_malloc(size_t sz, const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (pAllocationCallbacks != NULL) {
        if (pAllocationCallbacks->onMalloc != NULL) {
            return pAllocationCallbacks->onMalloc(sz, pAllocationCallbacks->pUserData);
        } else {
            return NULL;    /* Do not fall back to the default implementation. */
        }
    } else {
        return fs_malloc_default(sz, NULL);
    }
}

FS_API void* fs_calloc(size_t sz, const fs_allocation_callbacks* pAllocationCallbacks)
{
    void* p = fs_malloc(sz, pAllocationCallbacks);
    if (p != NULL) {
        FS_ZERO_MEMORY(p, sz);
    }

    return p;
}

FS_API void* fs_realloc(void* p, size_t sz, const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (pAllocationCallbacks != NULL) {
        if (pAllocationCallbacks->onRealloc != NULL) {
            return pAllocationCallbacks->onRealloc(p, sz, pAllocationCallbacks->pUserData);
        } else {
            return NULL;    /* Do not fall back to the default implementation. */
        }
    } else {
        return fs_realloc_default(p, sz, NULL);
    }
}

FS_API void fs_free(void* p, const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (p == NULL) {
        return;
    }

    if (pAllocationCallbacks != NULL) {
        if (pAllocationCallbacks->onFree != NULL) {
            pAllocationCallbacks->onFree(p, pAllocationCallbacks->pUserData);
        } else {
            return; /* Do no fall back to the default implementation. */
        }
    } else {
        fs_free_default(p, NULL);
    }
}
/* END fs_allocation_callbacks.c */



/* BEG fs_thread.c */
/*
This section has been designed to be mostly compatible with c89thread with only a few minor changes
if you wanted to amalgamate this into another project which uses c89thread and want to avoid duplicate
code. These are the differences:

    * The c89 namespace is replaced with "fs_".
    * There is no c89mtx_timedlock() equivalent.
    * `fs_mtx_plain`, etc. have been capitalized and taken out of the enum.
    * c89thread_success is FS_SUCCESS
    * c89thrd_error is EINVAL
    * c89thrd_busy is EBUSY
    * c89thrd_pthread_* is fs_pthread_*

Parameter ordering is the same as c89thread to make amalgamation easier.
*/

#if defined(_WIN32) && !defined(FS_USE_PTHREAD)
    /* Win32. Don't include windows.h here. */
#else
    #ifndef _XOPEN_SOURCE
    #define _XOPEN_SOURCE   700
    #else
        #if _XOPEN_SOURCE < 500
        #error _XOPEN_SOURCE must be >= 500. c89thread is not usable.
        #endif
    #endif

    #include <pthread.h>
    typedef pthread_t           fs_pthread;
    typedef pthread_mutex_t     fs_pthread_mutex;
    typedef pthread_cond_t      fs_pthread_cond;
#endif

#if defined(_WIN32)
typedef struct
{
    void* handle;    /* HANDLE, CreateMutex(), CreateEvent() */
    int type;
} fs_mtx;
#else
typedef fs_pthread_mutex fs_mtx;
#endif

#define FS_MTX_PLAIN        0
#define FS_MTX_TIMED        1
#define FS_MTX_RECURSIVE    2

enum
{
    fs_mtx_plain     = 0x00000000,
    fs_mtx_timed     = 0x00000001,
    fs_mtx_recursive = 0x00000002
};

#if defined(_WIN32)
FS_API int fs_mtx_init(fs_mtx* mutex, int type)
{
    HANDLE hMutex;

    if (mutex == NULL) {
        return EINVAL;
    }

    /* Initialize the object to zero for safety. */
    mutex->handle = NULL;
    mutex->type   = 0;

    /*
    CreateMutex() will create a thread-aware mutex (allowing recursiveness), whereas an auto-reset
    event (CreateEvent()) is not thread-aware and will deadlock (will not allow recursiveness). In
    Win32 I'm making all mutex's timeable.
    */
    if ((type & FS_MTX_RECURSIVE) != 0) {
        hMutex = CreateMutexA(NULL, FALSE, NULL);
    } else {
        hMutex = CreateEventA(NULL, FALSE, TRUE, NULL);
    }

    if (hMutex == NULL) {
        return fs_result_from_GetLastError(GetLastError());
    }

    mutex->handle = (void*)hMutex;
    mutex->type   = type;

    return FS_SUCCESS;
}

FS_API void fs_mtx_destroy(fs_mtx* mutex)
{
    if (mutex == NULL) {
        return;
    }

    CloseHandle((HANDLE)mutex->handle);
}

FS_API int fs_mtx_lock(fs_mtx* mutex)
{
    DWORD result;

    if (mutex == NULL) {
        return EINVAL;
    }

    result = WaitForSingleObject((HANDLE)mutex->handle, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return EINVAL;
    }

    return FS_SUCCESS;
}

FS_API int fs_mtx_trylock(fs_mtx* mutex)
{
    DWORD result;

    if (mutex == NULL) {
        return EINVAL;
    }

    result = WaitForSingleObject((HANDLE)mutex->handle, 0);
    if (result != WAIT_OBJECT_0) {
        return EBUSY;
    }

    return FS_SUCCESS;
}

FS_API int fs_mtx_unlock(fs_mtx* mutex)
{
    BOOL result;

    if (mutex == NULL) {
        return EINVAL;
    }

    if ((mutex->type & FS_MTX_RECURSIVE) != 0) {
        result = ReleaseMutex((HANDLE)mutex->handle);
    } else {
        result = SetEvent((HANDLE)mutex->handle);
    }

    if (!result) {
        return EINVAL;
    }

    return FS_SUCCESS;
}
#else
FS_API int fs_mtx_init(fs_mtx* mutex, int type)
{
    int result;
    pthread_mutexattr_t attr;   /* For specifying whether or not the mutex is recursive. */

    if (mutex == NULL) {
        return EINVAL;
    }

    pthread_mutexattr_init(&attr);
    if ((type & FS_MTX_RECURSIVE) != 0) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    } else {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);     /* Will deadlock. Consistent with Win32. */
    }

    result = pthread_mutex_init((pthread_mutex_t*)mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    if (result != 0) {
        return EINVAL;
    }

    return FS_SUCCESS;
}

FS_API void fs_mtx_destroy(fs_mtx* mutex)
{
    if (mutex == NULL) {
        return;
    }

    pthread_mutex_destroy((pthread_mutex_t*)mutex);
}

FS_API int fs_mtx_lock(fs_mtx* mutex)
{
    int result;

    if (mutex == NULL) {
        return EINVAL;
    }

    result = pthread_mutex_lock((pthread_mutex_t*)mutex);
    if (result != 0) {
        return EINVAL;
    }

    return FS_SUCCESS;
}

FS_API int fs_mtx_trylock(fs_mtx* mutex)
{
    int result;

    if (mutex == NULL) {
        return EINVAL;
    }

    result = pthread_mutex_trylock((pthread_mutex_t*)mutex);
    if (result != 0) {
        if (result == EBUSY) {
            return EBUSY;
        }

        return EINVAL;
    }

    return FS_SUCCESS;
}

FS_API int fs_mtx_unlock(fs_mtx* mutex)
{
    int result;

    if (mutex == NULL) {
        return EINVAL;
    }

    result = pthread_mutex_unlock((pthread_mutex_t*)mutex);
    if (result != 0) {
        return EINVAL;
    }

    return FS_SUCCESS;
}
#endif
/* END fs_thread.c */



/* BEG fs_stream.c */
FS_API fs_result fs_stream_init(const fs_stream_vtable* pVTable, fs_stream* pStream)
{
    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    pStream->pVTable = pVTable;

    if (pVTable == NULL) {
        return FS_INVALID_ARGS;
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_stream_read(fs_stream* pStream, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    size_t bytesRead;
    fs_result result;

    if (pBytesRead != NULL) {
        *pBytesRead = 0;
    }

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pStream->pVTable->read == NULL) {
        return FS_NOT_IMPLEMENTED;
    }

    bytesRead = 0;
    result = pStream->pVTable->read(pStream, pDst, bytesToRead, &bytesRead);

    if (pBytesRead != NULL) {
        *pBytesRead = bytesRead;
    }

    return result;
}

FS_API fs_result fs_stream_write(fs_stream* pStream, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    size_t bytesWritten;
    fs_result result;

    if (pBytesWritten != NULL) {
        *pBytesWritten = 0;
    }

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pStream->pVTable->write == NULL) {
        return FS_NOT_IMPLEMENTED;
    }

    bytesWritten = 0;
    result = pStream->pVTable->write(pStream, pSrc, bytesToWrite, &bytesWritten);

    if (pBytesWritten != NULL) {
        *pBytesWritten = bytesWritten;
    }

    return result;
}

FS_API fs_result fs_stream_writef(fs_stream* pStream, const char* fmt, ...)
{
    va_list args;
    fs_result result;

    va_start(args, fmt);
    result = fs_stream_writefv(pStream, fmt, args);
    va_end(args);

    return result;
}

FS_API fs_result fs_stream_writef_ex(fs_stream* pStream, const fs_allocation_callbacks* pAllocationCallbacks, const char* fmt, ...)
{
    va_list args;
    fs_result result;

    va_start(args, fmt);
    result = fs_stream_writefv_ex(pStream, pAllocationCallbacks, fmt, args);
    va_end(args);

    return result;
}

FS_API fs_result fs_stream_writefv(fs_stream* pStream, const char* fmt, va_list args)
{
    return fs_stream_writefv_ex(pStream, NULL, fmt, args);
}

FS_API fs_result fs_stream_writefv_ex(fs_stream* pStream, const fs_allocation_callbacks* pAllocationCallbacks, const char* fmt, va_list args)
{
    fs_result result;
    int strLen;
    char pStrStack[1024];

    if (pStream == NULL || fmt == NULL) {
        return FS_INVALID_ARGS;
    }

    strLen = fs_vsnprintf(pStrStack, sizeof(pStrStack), fmt, args);
    if (strLen < 0) {
        return FS_ERROR;    /* Encoding error. */
    }

    if (strLen < (int)sizeof(pStrStack)) {
        /* Stack buffer is big enough. Output straight to the file. */
        result = fs_stream_write(pStream, pStrStack, strLen, NULL);
    } else {
        /* Stack buffer is not big enough. Allocate space on the heap. */
        char* pStrHeap = NULL;

        pStrHeap = (char*)fs_malloc(strLen + 1, pAllocationCallbacks);
        if (pStrHeap == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        fs_vsnprintf(pStrHeap, strLen + 1, fmt, args);
        result = fs_stream_write(pStream, pStrHeap, strLen, NULL);

        fs_free(pStrHeap, pAllocationCallbacks);
    }

    return result;
}

FS_API fs_result fs_stream_seek(fs_stream* pStream, fs_int64 offset, fs_seek_origin origin)
{
    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pStream->pVTable->seek == NULL) {
        return FS_NOT_IMPLEMENTED;
    }

    return pStream->pVTable->seek(pStream, offset, origin);
}

FS_API fs_result fs_stream_tell(fs_stream* pStream, fs_int64* pCursor)
{
    if (pCursor == NULL) {
        return FS_INVALID_ARGS;  /* It does not make sense to call this without a variable to receive the cursor position. */
    }

    *pCursor = 0;   /* <-- In case an error happens later. */

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pStream->pVTable->tell == NULL) {
        return FS_NOT_IMPLEMENTED;
    }

    return pStream->pVTable->tell(pStream, pCursor);
}

FS_API fs_result fs_stream_duplicate(fs_stream* pStream, const fs_allocation_callbacks* pAllocationCallbacks, fs_stream** ppDuplicatedStream)
{
    fs_result result;
    fs_stream* pDuplicatedStream;

    if (ppDuplicatedStream == NULL) {
        return FS_INVALID_ARGS;
    }

    *ppDuplicatedStream = NULL;

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pStream->pVTable->duplicate_alloc_size == NULL || pStream->pVTable->duplicate == NULL) {
        return FS_NOT_IMPLEMENTED;
    }

    pDuplicatedStream = (fs_stream*)fs_calloc(pStream->pVTable->duplicate_alloc_size(pStream), pAllocationCallbacks);
    if (pDuplicatedStream == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    result = fs_stream_init(pStream->pVTable, pDuplicatedStream);
    if (result != FS_SUCCESS) {
        fs_free(pDuplicatedStream, pAllocationCallbacks);
        return result;
    }

    result = pStream->pVTable->duplicate(pStream, pDuplicatedStream);
    if (result != FS_SUCCESS) {
        fs_free(pDuplicatedStream, pAllocationCallbacks);
        return result;
    }

    *ppDuplicatedStream = pDuplicatedStream;

    return FS_SUCCESS;
}

FS_API void fs_stream_delete_duplicate(fs_stream* pDuplicatedStream, const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (pDuplicatedStream == NULL) {
        return;
    }

    if (pDuplicatedStream->pVTable->uninit != NULL) {
        pDuplicatedStream->pVTable->uninit(pDuplicatedStream);
    }

    fs_free(pDuplicatedStream, pAllocationCallbacks);
}


FS_API fs_result fs_stream_read_to_end(fs_stream* pStream, fs_format format, const fs_allocation_callbacks* pAllocationCallbacks, void** ppData, size_t* pDataSize)
{
    fs_result result = FS_SUCCESS;
    size_t dataSize = 0;
    size_t dataCap  = 0;
    void* pData = NULL;

    if (ppData != NULL) {
        *ppData = NULL;
    }
    if (pDataSize != NULL) {
        *pDataSize = 0;
    }

    if (pStream == NULL || ppData == NULL) {
        return FS_INVALID_ARGS;
    }

    /* Read in a loop into a dynamically increasing buffer. */
    for (;;) {
        size_t chunkSize = 4096;
        size_t bytesRead;

        if (dataSize + chunkSize > dataCap) {
            void* pNewData;
            size_t newCap = dataCap * 2;
            if (newCap == 0) {
                newCap = chunkSize;
            }

            pNewData = fs_realloc(pData, newCap, pAllocationCallbacks);
            if (pNewData == NULL) {
                fs_free(pData, pAllocationCallbacks);
                return FS_OUT_OF_MEMORY;
            }

            pData = pNewData;
            dataCap = newCap;
        }

        /* At this point there should be enough data in the buffer for the next chunk. */
        result = fs_stream_read(pStream, FS_OFFSET_PTR(pData, dataSize), chunkSize, &bytesRead);
        dataSize += bytesRead;

        if (result != FS_SUCCESS || bytesRead < chunkSize) {
            break;
        }
    }

    /* If we're opening in text mode, we need to append a null terminator. */
    if (format == FS_FORMAT_TEXT) {
        if (dataSize >= dataCap) {
            void* pNewData;
            pNewData = fs_realloc(pData, dataSize + 1, pAllocationCallbacks);
            if (pNewData == NULL) {
                fs_free(pData, pAllocationCallbacks);
                return FS_OUT_OF_MEMORY;
            }

            pData = pNewData;
        }

        ((char*)pData)[dataSize] = '\0';
    }

    *ppData = pData;

    if (pDataSize != NULL) {
        *pDataSize = dataSize;
    }

    /* Make sure the caller is aware of any errors. */
    if (result != FS_SUCCESS && result != FS_AT_END) {
        return result;
    } else {
        return FS_SUCCESS;
    }
}
/* END fs_stream.c */


/* BEG fs_backend.c */
static size_t fs_backend_alloc_size(const fs_backend* pBackend, const void* pBackendConfig)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->alloc_size == NULL) {
        return 0;
    } else {
        return pBackend->alloc_size(pBackendConfig);
    }
}

static fs_result fs_backend_init(const fs_backend* pBackend, fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->init == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->init(pFS, pBackendConfig, pStream);
    }
}

static void fs_backend_uninit(const fs_backend* pBackend, fs* pFS)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->uninit == NULL) {
        return;
    } else {
        pBackend->uninit(pFS);
    }
}

static fs_result fs_backend_ioctl(const fs_backend* pBackend, fs* pFS, int command, void* pArgs)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->ioctl == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->ioctl(pFS, command, pArgs);
    }
}

static fs_result fs_backend_remove(const fs_backend* pBackend, fs* pFS, const char* pFilePath)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->remove == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->remove(pFS, pFilePath);
    }
}

static fs_result fs_backend_rename(const fs_backend* pBackend, fs* pFS, const char* pOldName, const char* pNewName)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->remove == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->rename(pFS, pOldName, pNewName);
    }
}

static fs_result fs_backend_mkdir(const fs_backend* pBackend, fs* pFS, const char* pPath)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->remove == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->mkdir(pFS, pPath);
    }
}

static fs_result fs_backend_info(const fs_backend* pBackend, fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->info == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->info(pFS, pPath, openMode, pInfo);
    }
}

static size_t fs_backend_file_alloc_size(const fs_backend* pBackend, fs* pFS)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_alloc_size == NULL) {
        return 0;
    } else {
        return pBackend->file_alloc_size(pFS);
    }
}

static fs_result fs_backend_file_open(const fs_backend* pBackend, fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_open == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_open(pFS, pStream, pFilePath, openMode, pFile);
    }
}

static fs_result fs_backend_file_open_handle(const fs_backend* pBackend, fs* pFS, void* hBackendFile, fs_file* pFile)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_open_handle == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_open_handle(pFS, hBackendFile, pFile);
    }
}

static void fs_backend_file_close(const fs_backend* pBackend, fs_file* pFile)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_close == NULL) {
        return;
    } else {
        pBackend->file_close(pFile);
    }
}

static fs_result fs_backend_file_read(const fs_backend* pBackend, fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_read == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_read(pFile, pDst, bytesToRead, pBytesRead);
    }
}

static fs_result fs_backend_file_write(const fs_backend* pBackend, fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_write == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_write(pFile, pSrc, bytesToWrite, pBytesWritten);
    }
}

static fs_result fs_backend_file_seek(const fs_backend* pBackend, fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_seek == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_seek(pFile, offset, origin);
    }
}

static fs_result fs_backend_file_tell(const fs_backend* pBackend, fs_file* pFile, fs_int64* pCursor)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_tell == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_tell(pFile, pCursor);
    }
}

static fs_result fs_backend_file_flush(const fs_backend* pBackend, fs_file* pFile)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_flush == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_flush(pFile);
    }
}

static fs_result fs_backend_file_info(const fs_backend* pBackend, fs_file* pFile, fs_file_info* pInfo)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_info == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_info(pFile, pInfo);
    }
}

static fs_result fs_backend_file_duplicate(const fs_backend* pBackend, fs_file* pFile, fs_file* pDuplicatedFile)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_duplicate == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_duplicate(pFile, pDuplicatedFile);
    }
}

static fs_iterator* fs_backend_first(const fs_backend* pBackend, fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->first == NULL) {
        return NULL;
    } else {
        fs_iterator* pIterator;
        
        pIterator = pBackend->first(pFS, pDirectoryPath, directoryPathLen);
        
        /* Just make double sure the FS information is set in case the backend doesn't do it. */
        if (pIterator != NULL) {
            pIterator->pFS = pFS;
        }

        return pIterator;
    }
}

static fs_iterator* fs_backend_next(const fs_backend* pBackend, fs_iterator* pIterator)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->next == NULL) {
        return NULL;
    } else {
        return pBackend->next(pIterator);
    }
}

static void fs_backend_free_iterator(const fs_backend* pBackend, fs_iterator* pIterator)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->free_iterator == NULL) {
        return;
    } else {
        pBackend->free_iterator(pIterator);
    }
}
/* END fs_backend.c */



/* BEG fs_proxy.c */
/*
This is a special backend that we use for archives so we can intercept opening and closing of files within those archives
and do any necessary reference counting.
*/

/* Forward declarations. */
static size_t fs_increment_opened_archive_ref_count(fs* pFS, fs* pArchive);
static size_t fs_decrement_opened_archive_ref_count(fs* pFS, fs* pArchive);

typedef struct fs_proxy
{
    const fs_backend* pBackend;
    fs_file* pArchiveFile;
} fs_proxy;

typedef struct fs_proxy_config
{
    const fs_backend* pBackend;
    const void* pBackendConfig;
} fs_proxy_config;

typedef struct fs_file_proxy
{
    fs_bool32 unrefArchiveOnClose;
} fs_file_proxy;

static fs_proxy* fs_proxy_get_backend_data(fs* pFS)
{
    return (fs_proxy*)FS_OFFSET_PTR(fs_get_backend_data(pFS), fs_get_backend_data_size(pFS) - sizeof(fs_proxy));
}

static const fs_backend* fs_proxy_get_backend(fs* pFS)
{
    return fs_proxy_get_backend_data(pFS)->pBackend;
}

static fs_file* fs_proxy_get_archive_file(fs* pFS)
{
    fs_proxy* pProxy;

    pProxy = fs_proxy_get_backend_data(pFS);
    FS_ASSERT(pProxy != NULL);

    return pProxy->pArchiveFile;
}

static fs* fs_proxy_get_owner_fs(fs* pFS)
{
    return fs_file_get_fs(fs_proxy_get_archive_file(pFS));
}


static fs_file_proxy* fs_file_proxy_get_backend_data(fs_file* pFile)
{
    return (fs_file_proxy*)FS_OFFSET_PTR(fs_file_get_backend_data(pFile), fs_file_get_backend_data_size(pFile) - sizeof(fs_file_proxy));
}

static fs_bool32 fs_file_proxy_get_unref_archive_on_close(fs_file* pFile)
{
    return fs_file_proxy_get_backend_data(pFile)->unrefArchiveOnClose;
}

static void fs_file_proxy_set_unref_archive_on_close(fs_file* pFile, fs_bool32 unrefArchiveOnClose)
{
    fs_file_proxy_get_backend_data(pFile)->unrefArchiveOnClose = unrefArchiveOnClose;
}


static size_t fs_alloc_size_proxy(const void* pBackendConfig)
{
    const fs_proxy_config* pProxyConfig = (const fs_proxy_config*)pBackendConfig;
    FS_ASSERT(pProxyConfig != NULL);    /* <-- We must have a config since that's where the backend is specified. */

    return fs_backend_alloc_size(pProxyConfig->pBackend, pProxyConfig->pBackendConfig) + sizeof(fs_proxy);
}

static fs_result fs_init_proxy(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    const fs_proxy_config* pProxyConfig = (const fs_proxy_config*)pBackendConfig;
    fs_proxy* pProxy;

    FS_ASSERT(pProxyConfig != NULL);    /* <-- We must have a config since that's where the backend is specified. */
    FS_ASSERT(pStream      != NULL);    /* <-- This backend is only used with archives which means we must have a stream. */

    pProxy = fs_proxy_get_backend_data(pFS);
    FS_ASSERT(pProxy != NULL);

    pProxy->pBackend     = pProxyConfig->pBackend;
    pProxy->pArchiveFile = (fs_file*)pStream; /* The stream will always be a fs_file when using this backend. */

    return fs_backend_init(pProxyConfig->pBackend, pFS, pProxyConfig->pBackendConfig, pStream);
}

static void fs_uninit_proxy(fs* pFS)
{
    fs_backend_uninit(fs_proxy_get_backend(pFS), pFS);
}

static fs_result fs_ioctl_proxy(fs* pFS, int command, void* pArgs)
{
    return fs_backend_ioctl(fs_proxy_get_backend(pFS), pFS, command, pArgs);
}

static fs_result fs_remove_proxy(fs* pFS, const char* pFilePath)
{
    return fs_backend_remove(fs_proxy_get_backend(pFS), pFS, pFilePath);
}

static fs_result fs_rename_proxy(fs* pFS, const char* pOldName, const char* pNewName)
{
    return fs_backend_rename(fs_proxy_get_backend(pFS), pFS, pOldName, pNewName);
}

static fs_result fs_mkdir_proxy(fs* pFS, const char* pPath)
{
    return fs_backend_mkdir(fs_proxy_get_backend(pFS), pFS, pPath);
}

static fs_result fs_info_proxy(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    return fs_backend_info(fs_proxy_get_backend(pFS), pFS, pPath, openMode, pInfo);
}

static size_t fs_file_alloc_size_proxy(fs* pFS)
{
    return fs_backend_file_alloc_size(fs_proxy_get_backend(pFS), pFS);
}

static fs_result fs_file_open_proxy(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    return fs_backend_file_open(fs_proxy_get_backend(pFS), pFS, pStream, pFilePath, openMode, pFile);
}

static fs_result fs_file_open_handle_proxy(fs* pFS, void* hBackendFile, fs_file* pFile)
{
    return fs_backend_file_open_handle(fs_proxy_get_backend(pFS), pFS, hBackendFile, pFile);
}

static void fs_file_close_proxy(fs_file* pFile)
{
    fs_backend_file_close(fs_proxy_get_backend(fs_file_get_fs(pFile)), pFile);

    /*
    This right here is the entire reason the backend needs to be proxied. We need to intercept
    calls to fs_file_close() so we can then tell the FS that owns the archive to decrement the
    reference count and potentially put the archive FS up for garbage collection.

    You'll note that we don't have a corresponding call to fs_open_archive() in the function
    fs_file_open_proxy() above. The reason is that fs_open_archive() is called at a higher
    level when the archive FS is being acquired in the first place.
    */
    if (fs_file_proxy_get_unref_archive_on_close(pFile)) {
        fs_close_archive(fs_file_get_fs(pFile));
    }
}

static fs_result fs_file_read_proxy(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    return fs_backend_file_read(fs_proxy_get_backend(fs_file_get_fs(pFile)), pFile, pDst, bytesToRead, pBytesRead);
}

static fs_result fs_file_write_proxy(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    return fs_backend_file_write(fs_proxy_get_backend(fs_file_get_fs(pFile)), pFile, pSrc, bytesToWrite, pBytesWritten);
}

static fs_result fs_file_seek_proxy(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    return fs_backend_file_seek(fs_proxy_get_backend(fs_file_get_fs(pFile)), pFile, offset, origin);
}

static fs_result fs_file_tell_proxy(fs_file* pFile, fs_int64* pCursor)
{
    return fs_backend_file_tell(fs_proxy_get_backend(fs_file_get_fs(pFile)), pFile, pCursor);
}

static fs_result fs_file_flush_proxy(fs_file* pFile)
{
    return fs_backend_file_flush(fs_proxy_get_backend(fs_file_get_fs(pFile)), pFile);
}

static fs_result fs_file_info_proxy(fs_file* pFile, fs_file_info* pInfo)
{
    return fs_backend_file_info(fs_proxy_get_backend(fs_file_get_fs(pFile)), pFile, pInfo);
}

static fs_result fs_file_duplicate_proxy(fs_file* pFile, fs_file* pDuplicatedFile)
{
    fs_result result;
    fs* pFS;

    pFS = fs_file_get_fs(pFile);
    
    result = fs_backend_file_duplicate(fs_proxy_get_backend(pFS), pFile, pDuplicatedFile);
    if (result != FS_SUCCESS) {
        return result;
    }

    /* Increment the reference count of the opened archive if necessary. */
    if (fs_file_proxy_get_unref_archive_on_close(pFile)) {
        fs* pOwnerFS;

        fs_file_proxy_set_unref_archive_on_close(pDuplicatedFile, FS_TRUE);

        pOwnerFS = fs_proxy_get_owner_fs(pFS);
        if (pOwnerFS != NULL) {
            fs_increment_opened_archive_ref_count(pOwnerFS, pFS);
        }
    }

    return FS_SUCCESS;
}

static fs_iterator* fs_first_proxy(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    return fs_backend_first(fs_proxy_get_backend(pFS), pFS, pDirectoryPath, directoryPathLen);
}

static fs_iterator* fs_next_proxy(fs_iterator* pIterator)
{
    return fs_backend_next(fs_proxy_get_backend(pIterator->pFS), pIterator);
}

static void fs_free_iterator_proxy(fs_iterator* pIterator)
{
    fs_backend_free_iterator(fs_proxy_get_backend(pIterator->pFS), pIterator);
}

static fs_backend fs_proxy_backend =
{
    fs_alloc_size_proxy,
    fs_init_proxy,
    fs_uninit_proxy,
    fs_ioctl_proxy,
    fs_remove_proxy,
    fs_rename_proxy,
    fs_mkdir_proxy,
    fs_info_proxy,
    fs_file_alloc_size_proxy,
    fs_file_open_proxy,
    fs_file_open_handle_proxy,
    fs_file_close_proxy,
    fs_file_read_proxy,
    fs_file_write_proxy,
    fs_file_seek_proxy,
    fs_file_tell_proxy,
    fs_file_flush_proxy,
    fs_file_info_proxy,
    fs_file_duplicate_proxy,
    fs_first_proxy,
    fs_next_proxy,
    fs_free_iterator_proxy
};
const fs_backend* FS_PROXY = &fs_proxy_backend;
/* END fs_proxy.c */



/* BEG fs.c */
#define FS_IS_OPAQUE(mode)      ((mode & FS_OPAQUE) != 0)
#define FS_IS_VERBOSE(mode)     ((mode & FS_VERBOSE) != 0)
#define FS_IS_TRANSPARENT(mode) ((mode & (FS_OPAQUE | FS_VERBOSE)) == 0)

FS_API fs_config fs_config_init_default(void)
{
    fs_config config;

    FS_ZERO_OBJECT(&config);

    return config;
}

FS_API fs_config fs_config_init(const fs_backend* pBackend, void* pBackendConfig, fs_stream* pStream)
{
    fs_config config = fs_config_init_default();
    config.pBackend       = pBackend;
    config.pBackendConfig = pBackendConfig;
    config.pStream        = pStream;

    return config;
}

typedef struct fs_opened_archive
{
    fs* pArchive;
    size_t refCount;
    char pPath[1];
} fs_opened_archive;

typedef struct fs_mount_point
{
    size_t pathOff;                     /* Points to a null terminated string containing the mounted path starting from the first byte after this struct. */
    size_t pathLen;
    size_t mountPointOff;               /* Points to a null terminated string containing the mount point starting from the first byte after this struct. */
    size_t mountPointLen;
    fs* pArchive;                       /* Can be null in which case the mounted path is a directory. */
    fs_bool32 closeArchiveOnUnmount;    /* If set to true, the archive FS will be closed when the mount point is unmounted. */
    fs_bool32 padding;
} fs_mount_point;

typedef struct fs_mount_list fs_mount_list;

struct fs
{
    const fs_backend* pBackend;
    fs_stream* pStream;
    fs_allocation_callbacks allocationCallbacks;
    void* pArchiveTypes;    /* One heap allocation containing all extension registrations. Needs to be parsed in order to enumerate them. Structure is [const fs_backend*][extension][null-terminator][padding (aligned to FS_SIZEOF_PTR)] */
    size_t archiveTypesAllocSize;
    fs_bool32 isOwnerOfArchiveTypes;
    size_t backendDataSize;
    fs_mtx archiveLock;     /* For use with fs_open_archive() and fs_close_archive(). */
    void* pOpenedArchives;  /* One heap allocation. Structure is [fs*][refcount (size_t)][path][null-terminator][padding (aligned to FS_SIZEOF_PTR)] */
    size_t openedArchivesSize;
    size_t openedArchivesCap;
    size_t archiveGCThreshold;
    fs_mount_list* pReadMountPoints;
    fs_mount_list* pWriteMountPoints;
};

typedef struct fs_file
{
    fs_stream stream; /* Files are streams. This must be the first member so it can be cast. */
    fs* pFS;
    fs_stream* pStreamForBackend;   /* The stream for use by the backend. Different to `stream`. This is a duplicate of the stream used by `pFS` so the backend can do reading. */
    size_t backendDataSize;
} fs_file;


static void fs_gc_archives_nolock(fs* pFS, int policy); /* Defined further down in the file. */


static size_t fs_mount_point_size(size_t pathLen, size_t mountPointLen)
{
    return FS_ALIGN(sizeof(fs_mount_point) + pathLen + 1 + mountPointLen + 1, FS_SIZEOF_PTR);
}



static size_t fs_mount_list_get_header_size(void)
{
    return sizeof(size_t)*2;
}

static size_t fs_mount_list_get_alloc_size(const fs_mount_list* pList)
{
    if (pList == NULL) {
        return 0;
    }

    return *(size_t*)FS_OFFSET_PTR(pList, 0);
}

static size_t fs_mount_list_get_alloc_cap(const fs_mount_list* pList)
{
    if (pList == NULL) {
        return 0;
    }

    return *(size_t*)FS_OFFSET_PTR(pList, 1 * sizeof(size_t));
}

static void fs_mount_list_set_alloc_size(fs_mount_list* pList, size_t newSize)
{
    FS_ASSERT(pList != NULL);
    *(size_t*)FS_OFFSET_PTR(pList, 0) = newSize;
}

static void fs_mount_list_set_alloc_cap(fs_mount_list* pList, size_t newCap)
{
    FS_ASSERT(pList != NULL);
    *(size_t*)FS_OFFSET_PTR(pList, 1 * sizeof(size_t)) = newCap;
}


typedef struct fs_mount_list_iterator
{
    const char* pPath;
    const char* pMountPointPath;
    fs* pArchive; /* Can be null. */
    struct
    {
        fs_mount_list* pList;
        fs_mount_point* pMountPoint;
        size_t cursor;
    } internal;
} fs_mount_list_iterator;

static fs_result fs_mount_list_iterator_resolve_members(fs_mount_list_iterator* pIterator, size_t cursor)
{
    FS_ASSERT(pIterator != NULL);

    if (cursor >= fs_mount_list_get_alloc_size(pIterator->internal.pList)) {
        return FS_AT_END;
    }

    pIterator->internal.cursor      = cursor;
    pIterator->internal.pMountPoint = (fs_mount_point*)FS_OFFSET_PTR(pIterator->internal.pList, fs_mount_list_get_header_size() + pIterator->internal.cursor);
    FS_ASSERT(pIterator->internal.pMountPoint != NULL);

    /* The content of the paths are stored at the end of the structure. */
    pIterator->pPath           = (const char*)FS_OFFSET_PTR(pIterator->internal.pMountPoint, sizeof(fs_mount_point) + pIterator->internal.pMountPoint->pathOff);
    pIterator->pMountPointPath = (const char*)FS_OFFSET_PTR(pIterator->internal.pMountPoint, sizeof(fs_mount_point) + pIterator->internal.pMountPoint->mountPointOff);
    pIterator->pArchive        = pIterator->internal.pMountPoint->pArchive;

    return FS_SUCCESS;
}

static fs_result fs_mount_list_first(fs_mount_list* pList, fs_mount_list_iterator* pIterator)
{
    FS_ASSERT(pIterator != NULL);

    FS_ZERO_OBJECT(pIterator);
    pIterator->internal.pList = pList;

    if (fs_mount_list_get_alloc_size(pList) == 0) {
        return FS_AT_END;   /* No mount points. */
    }

    return fs_mount_list_iterator_resolve_members(pIterator, 0);
}

static fs_result fs_mount_list_next(fs_mount_list_iterator* pIterator)
{
    size_t newCursor;

    FS_ASSERT(pIterator != NULL);

    /* For a bit of safety, lets go ahead and check if the cursor is already at the end and if so just abort early. */
    if (pIterator->internal.cursor >= fs_mount_list_get_alloc_size(pIterator->internal.pList)) {
        return FS_AT_END;
    }

    /* Move the cursor forward. If after advancing the cursor we are at the end we're done and we can free the mount point iterator and return. */
    newCursor = pIterator->internal.cursor + fs_mount_point_size(pIterator->internal.pMountPoint->pathLen, pIterator->internal.pMountPoint->mountPointLen);
    FS_ASSERT(newCursor <= fs_mount_list_get_alloc_size(pIterator->internal.pList)); /* <-- If this assert fails, there's a bug in the packing of the structure.*/

    return fs_mount_list_iterator_resolve_members(pIterator, newCursor);
}

static fs_mount_list* fs_mount_list_alloc(fs_mount_list* pList, const char* pPathToMount, const char* pMountPoint, fs_mount_priority priority, const fs_allocation_callbacks* pAllocationCallbacks, fs_mount_point** ppMountPoint)
{
    fs_mount_point* pNewMountPoint = NULL;
    size_t pathToMountLen;
    size_t mountPointLen;
    size_t mountPointAllocSize;

    FS_ASSERT(ppMountPoint != NULL);
    *ppMountPoint = NULL;

    pathToMountLen = strlen(pPathToMount);
    mountPointLen  = strlen(pMountPoint);
    mountPointAllocSize = fs_mount_point_size(pathToMountLen, mountPointLen);

    if (fs_mount_list_get_alloc_cap(pList) < fs_mount_list_get_alloc_size(pList) + mountPointAllocSize) {
        size_t newCap;
        fs_mount_list* pNewList;

        newCap = fs_mount_list_get_alloc_cap(pList) * 2;
        if (newCap < fs_mount_list_get_alloc_size(pList) + mountPointAllocSize) {
            newCap = fs_mount_list_get_alloc_size(pList) + mountPointAllocSize;
        }

        pNewList = (fs_mount_list*)fs_realloc(pList, fs_mount_list_get_header_size() + newCap, pAllocationCallbacks); /* Need room for leading size and cap variables. */
        if (pNewList == NULL) {
            return NULL;
        }

        /* Little bit awkward, but if the list is fresh we'll want to clear everything to zero. */
        if (pList == NULL) {
            FS_ZERO_MEMORY(pNewList, fs_mount_list_get_header_size());
        }

        pList = (fs_mount_list*)pNewList;
        fs_mount_list_set_alloc_cap(pList, newCap);
    }

    /*
    Getting here means we should have enough room in the buffer. Now we need to use the priority to determine where
    we're going to place the new entry within the buffer.
    */
    if (priority == FS_MOUNT_PRIORITY_LOWEST) {
        /* The new entry goes to the end of the list. */
        pNewMountPoint = (fs_mount_point*)FS_OFFSET_PTR(pList, fs_mount_list_get_header_size() + fs_mount_list_get_alloc_size(pList));
    } else if (priority == FS_MOUNT_PRIORITY_HIGHEST) {
        /* The new entry goes to the start of the list. We'll need to move everything down. */
        FS_MOVE_MEMORY(FS_OFFSET_PTR(pList, fs_mount_list_get_header_size() + mountPointAllocSize), FS_OFFSET_PTR(pList, fs_mount_list_get_header_size()), fs_mount_list_get_alloc_size(pList));
        pNewMountPoint = (fs_mount_point*)FS_OFFSET_PTR(pList, fs_mount_list_get_header_size());
    } else {
        FS_ASSERT(!"Unknown mount priority.");
        return NULL;
    }

    fs_mount_list_set_alloc_size(pList, fs_mount_list_get_alloc_size(pList) + mountPointAllocSize);

    /* Now we can fill out the details of the new mount point. */
    pNewMountPoint->pathOff       = 0;                  /* The path is always the first byte after the struct. */
    pNewMountPoint->pathLen       = pathToMountLen;
    pNewMountPoint->mountPointOff = pathToMountLen + 1; /* The mount point is always the first byte after the path to mount. */
    pNewMountPoint->mountPointLen = mountPointLen;

    memcpy(FS_OFFSET_PTR(pNewMountPoint, sizeof(fs_mount_point) + pNewMountPoint->pathOff),       pPathToMount, pathToMountLen + 1);
    memcpy(FS_OFFSET_PTR(pNewMountPoint, sizeof(fs_mount_point) + pNewMountPoint->mountPointOff), pMountPoint,  mountPointLen  + 1);

    *ppMountPoint = pNewMountPoint;
    return pList;
}

static fs_result fs_mount_list_remove(fs_mount_list* pList, fs_mount_point* pMountPoint)
{
    size_t mountPointAllocSize = fs_mount_point_size(pMountPoint->pathLen, pMountPoint->mountPointLen);
    size_t newMountPointsAllocSize = fs_mount_list_get_alloc_size(pList) - mountPointAllocSize;

    FS_MOVE_MEMORY
    (
        pMountPoint,
        FS_OFFSET_PTR(pList, fs_mount_list_get_header_size() + mountPointAllocSize),
        fs_mount_list_get_alloc_size(pList) - ((fs_uintptr)pMountPoint - (fs_uintptr)FS_OFFSET_PTR(pList, fs_mount_list_get_header_size())) - mountPointAllocSize
    );

    fs_mount_list_set_alloc_size(pList, newMountPointsAllocSize);

    return FS_SUCCESS;
}



static const fs_backend* fs_get_backend_or_default(const fs* pFS)
{
    if (pFS == NULL) {
        return FS_STDIO;
    } else {
        return pFS->pBackend;
    }
}

typedef struct fs_registered_backend_iterator
{
    const fs* pFS;
    size_t cursor;
    const fs_backend* pBackend;
    void* pBackendConfig;
    const char* pExtension;
    size_t extensionLen;
} fs_registered_backend_iterator;

FS_API fs_result fs_file_open_or_info(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile, fs_file_info* pInfo);
static fs_result fs_next_registered_backend(fs_registered_backend_iterator* pIterator);

static fs_result fs_first_registered_backend(fs* pFS, fs_registered_backend_iterator* pIterator)
{
    FS_ASSERT(pFS       != NULL);
    FS_ASSERT(pIterator != NULL);

    FS_ZERO_OBJECT(pIterator);
    pIterator->pFS = pFS;

    return fs_next_registered_backend(pIterator);
}

static fs_result fs_next_registered_backend(fs_registered_backend_iterator* pIterator)
{
    FS_ASSERT(pIterator != NULL);

    if (pIterator->cursor >= pIterator->pFS->archiveTypesAllocSize) {
        return FS_AT_END;
    }

    pIterator->pBackend       = *(const fs_backend**)FS_OFFSET_PTR(pIterator->pFS->pArchiveTypes, pIterator->cursor);
    pIterator->pBackendConfig =  NULL;   /* <-- I'm not sure how to deal with backend configs with this API. Putting this member in the iterator in case I want to support this later. */
    pIterator->pExtension     =  (const char*       )FS_OFFSET_PTR(pIterator->pFS->pArchiveTypes, pIterator->cursor + sizeof(fs_backend*));
    pIterator->extensionLen   =  strlen(pIterator->pExtension);

    pIterator->cursor += FS_ALIGN(sizeof(fs_backend*) + pIterator->extensionLen + 1, FS_SIZEOF_PTR);

    return FS_SUCCESS;
}



static fs_opened_archive* fs_find_opened_archive(fs* pFS, const char* pArchivePath, size_t archivePathLen)
{
    size_t cursor;

    if (pFS == NULL) {
        return NULL;
    }

    FS_ASSERT(pArchivePath != NULL);
    FS_ASSERT(archivePathLen > 0);

    cursor = 0;
    while (cursor < pFS->openedArchivesSize) {
        fs_opened_archive* pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, cursor);

        if (fs_strncmp(pOpenedArchive->pPath, pArchivePath, archivePathLen) == 0) {
            return pOpenedArchive;
        }

        /* Getting here means this archive is not the one we're looking for. */
        cursor += FS_ALIGN(sizeof(fs*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);
    }

    /* If we get here it means we couldn't find the archive by it's name. */
    return NULL;
}

static fs_opened_archive* fs_find_opened_archive_by_fs(fs* pFS, fs* pArchive)
{
    size_t cursor;

    if (pFS == NULL) {
        return NULL;
    }

    FS_ASSERT(pArchive != NULL);

    cursor = 0;
    while (cursor < pFS->openedArchivesSize) {
        fs_opened_archive* pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, cursor);

        if (pOpenedArchive->pArchive == pArchive) {
            return pOpenedArchive;
        }

        /* Getting here means this archive is not the one we're looking for. */
        cursor += FS_ALIGN(sizeof(fs*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);
    }

    /* If we get here it means we couldn't find the archive. */
    return NULL;
}

static fs_result fs_add_opened_archive(fs* pFS, fs* pArchive, const char* pArchivePath, size_t archivePathLen)
{
    size_t openedArchiveSize;
    fs_opened_archive* pOpenedArchive;

    FS_ASSERT(pFS          != NULL);
    FS_ASSERT(pArchive     != NULL);
    FS_ASSERT(pArchivePath != NULL);

    if (archivePathLen == FS_NULL_TERMINATED) {
        archivePathLen = strlen(pArchivePath);
    }

    openedArchiveSize = FS_ALIGN(sizeof(fs*) + sizeof(size_t) + archivePathLen + 1, FS_SIZEOF_PTR);

    if (pFS->openedArchivesSize + openedArchiveSize > pFS->openedArchivesCap) {
        size_t newOpenedArchivesCap;
        void* pNewOpenedArchives;

        newOpenedArchivesCap = pFS->openedArchivesCap * 2;
        if (newOpenedArchivesCap < pFS->openedArchivesSize + openedArchiveSize) {
            newOpenedArchivesCap = pFS->openedArchivesSize + openedArchiveSize;
        }

        pNewOpenedArchives = fs_realloc(pFS->pOpenedArchives, newOpenedArchivesCap, fs_get_allocation_callbacks(pFS));
        if (pNewOpenedArchives == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        pFS->pOpenedArchives   = pNewOpenedArchives;
        pFS->openedArchivesCap = newOpenedArchivesCap;
    }

    /* If we get here we should have enough room in the buffer to store the new archive details. */
    FS_ASSERT(pFS->openedArchivesSize + openedArchiveSize <= pFS->openedArchivesCap);

    pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, pFS->openedArchivesSize);
    pOpenedArchive->pArchive = pArchive;
    pOpenedArchive->refCount = 0;
    fs_strncpy(pOpenedArchive->pPath, pArchivePath, archivePathLen);

    pFS->openedArchivesSize += openedArchiveSize;

    return FS_SUCCESS;
}

static fs_result fs_remove_opened_archive(fs* pFS, fs_opened_archive* pOpenedArchive)
{
    /* This is a simple matter of doing a memmove() to move memory down. pOpenedArchive should be an offset of pFS->pOpenedArchives. */
    size_t openedArchiveSize;

    openedArchiveSize = FS_ALIGN(sizeof(fs_opened_archive*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);

    FS_ASSERT(((fs_uintptr)pOpenedArchive + openedArchiveSize) >  ((fs_uintptr)pFS->pOpenedArchives));
    FS_ASSERT(((fs_uintptr)pOpenedArchive + openedArchiveSize) <= ((fs_uintptr)pFS->pOpenedArchives + pFS->openedArchivesSize));

    FS_MOVE_MEMORY(pOpenedArchive, FS_OFFSET_PTR(pOpenedArchive, openedArchiveSize), (size_t)((((fs_uintptr)pFS->pOpenedArchives + pFS->openedArchivesSize)) - ((fs_uintptr)pOpenedArchive + openedArchiveSize)));
    pFS->openedArchivesSize -= openedArchiveSize;

    return FS_SUCCESS;
}

static size_t fs_increment_opened_archive_ref_count(fs* pFS, fs* pArchive)
{
    fs_opened_archive* pOpenedArchive = fs_find_opened_archive_by_fs(pFS, pArchive);
    if (pOpenedArchive != NULL) {
        pOpenedArchive->refCount += 1;

        /* If the owner FS is also an archive, increment it's reference counter as well. */
        if (pFS->pBackend == FS_PROXY) {
            fs_increment_opened_archive_ref_count(fs_proxy_get_owner_fs(pFS), pFS);
        }

        return pOpenedArchive->refCount;
    }

    return 0;
}

static size_t fs_decrement_opened_archive_ref_count(fs* pFS, fs* pArchive)
{
    fs_opened_archive* pOpenedArchive = fs_find_opened_archive_by_fs(pFS, pArchive);
    if (pOpenedArchive != NULL) {
        FS_ASSERT(pOpenedArchive->refCount > 0);    /* <-- If this fails it means there's a bug in the library. Please report. */
        pOpenedArchive->refCount -= 1;

        /* If the owner FS is also an archive, decrement it's reference counter as well. */
        if (pFS->pBackend == FS_PROXY) {
            fs_decrement_opened_archive_ref_count(fs_proxy_get_owner_fs(pFS), pFS);
        }

        return pOpenedArchive->refCount;
    }

    return 0;
}


static size_t fs_archive_type_sizeof(const fs_archive_type* pArchiveType)
{
    return FS_ALIGN(sizeof(pArchiveType->pBackend) + strlen(pArchiveType->pExtension) + 1, FS_SIZEOF_PTR);
}


FS_API fs_result fs_init(const fs_config* pConfig, fs** ppFS)
{
    fs* pFS;
    fs_config defaultConfig;
    const fs_backend* pBackend = NULL;
    size_t backendDataSizeInBytes = 0;
    fs_int64 initialStreamCursor = -1;
    size_t archiveTypesAllocSize = 0;
    size_t iArchiveType;

    if (ppFS == NULL) {
        return FS_INVALID_ARGS;
    }

    *ppFS = NULL;

    if (pConfig == NULL) {
        defaultConfig = fs_config_init_default();
        pConfig = &defaultConfig;
    }

    pBackend = pConfig->pBackend;
    if (pBackend == NULL) {
        pBackend = FS_STDIO;
    }

    /* If the backend is still null at this point it means the default backend has been disabled. */
    if (pBackend == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pBackend->alloc_size != NULL) {
        backendDataSizeInBytes = pBackend->alloc_size(pConfig->pBackendConfig);
    } else {
        backendDataSizeInBytes = 0;
    }

    /* We need to allocate space for the archive types which we place just after the "fs" struct. After that will be the backend data. */
    for (iArchiveType = 0; iArchiveType < pConfig->archiveTypeCount; iArchiveType += 1) {
        archiveTypesAllocSize += fs_archive_type_sizeof(&pConfig->pArchiveTypes[iArchiveType]);
    }

    pFS = (fs*)fs_calloc(sizeof(fs) + archiveTypesAllocSize + backendDataSizeInBytes, pConfig->pAllocationCallbacks);
    if (pFS == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pFS->pBackend              = pBackend;
    pFS->pStream               = pConfig->pStream; /* <-- This is allowed to be null, which will be the case for standard OS file system APIs like stdio. Streams are used for things like archives like Zip files, or in-memory file systems. */
    pFS->allocationCallbacks   = fs_allocation_callbacks_init_copy(pConfig->pAllocationCallbacks);
    pFS->backendDataSize       = backendDataSizeInBytes;
    pFS->isOwnerOfArchiveTypes = FS_TRUE;
    pFS->archiveGCThreshold    = FS_DEFAULT_ARCHIVE_GC_THRESHOLD;
    pFS->archiveTypesAllocSize = archiveTypesAllocSize;
    pFS->pArchiveTypes         = (void*)FS_OFFSET_PTR(pFS, sizeof(fs));

    /* Archive types. */
    if (pConfig->archiveTypeCount > 0) {
        size_t cursor = 0;

        for (iArchiveType = 0; iArchiveType < pConfig->archiveTypeCount; iArchiveType += 1) {
            size_t extensionLength = strlen(pConfig->pArchiveTypes[iArchiveType].pExtension);

            FS_COPY_MEMORY(FS_OFFSET_PTR(pFS->pArchiveTypes, cursor                      ), &pConfig->pArchiveTypes[iArchiveType].pBackend,   sizeof(fs_backend*));
            FS_COPY_MEMORY(FS_OFFSET_PTR(pFS->pArchiveTypes, cursor + sizeof(fs_backend*)),  pConfig->pArchiveTypes[iArchiveType].pExtension, extensionLength + 1);

            cursor += fs_archive_type_sizeof(&pConfig->pArchiveTypes[iArchiveType]);
        }
    } else {
        pFS->pArchiveTypes = NULL;
        pFS->archiveTypesAllocSize = 0;
    }

    /*
    If we were initialized with a stream we need to make sure we have a lock for it. This is needed for
    archives which might have multiple files accessing a stream across different threads. The archive
    will need to lock the stream so it doesn't get all mixed up between threads.
    */
    if (pConfig->pStream != NULL) {
        /* We want to grab the initial cursor of the stream so we can restore it in the case of an error. */
        if (fs_stream_tell(pConfig->pStream, &initialStreamCursor) != FS_SUCCESS) {
            initialStreamCursor = -1;
        }
    }

    /*
    We need a mutex for fs_open_archive() and fs_close_archive(). This needs to be recursive because
    during garbage collection we may end up closing archives in archives.
    */
    fs_mtx_init(&pFS->archiveLock, FS_MTX_RECURSIVE);

    /* We're now ready to initialize the backend. */
    if (pBackend->init != NULL) {
        fs_result result = pBackend->init(pFS, pConfig->pBackendConfig, pConfig->pStream);
        if (result != FS_SUCCESS) {
            /*
            If we have a stream and the backend failed to initialize, it's possible that the cursor of the stream
            was moved as a result. To keep this as clean as possible, we're going to seek the cursor back to the
            initial position.
            */
            if (pConfig->pStream != NULL && initialStreamCursor != -1) {
                fs_stream_seek(pConfig->pStream, initialStreamCursor, FS_SEEK_SET);
            }

            fs_free(pFS, fs_get_allocation_callbacks(pFS));
            return result;
        }
    } else {
        /* Getting here means the backend does not implement an init() function. This is not mandatory so we just assume successful.*/
    }

    *ppFS = pFS;
    return FS_SUCCESS;
}

FS_API void fs_uninit(fs* pFS)
{
    if (pFS == NULL) {
        return;
    }

    /*
    We'll first garbage collect all archives. This should uninitialize any archives that are
    still open but have no references. After this call any archives that are still being
    referenced will remain open. Not quite sure what to do in this situation, but for now
    I'll check if any archives are still open and throw an assert. Not sure if this is
    overly aggressive - feedback welcome.
    */
    fs_gc_archives(pFS, FS_GC_POLICY_FULL);

    /* The caller has a bug if there are still outstanding archives. */
    #if !defined(FS_NO_OPENED_FILES_ASSERT)
    {
        if (pFS->openedArchivesSize > 0) {
            FS_ASSERT(!"You have outstanding opened files. You must close all files before uninitializing the fs object.");    /* <-- If you hit this assert but you're absolutely sure you've closed all your files, please submit a bug report with a reproducible test case. Define `FS_NO_OPENED_FILES_ASSERT` to workaround the assert. */
        }
    }
    #endif


    if (pFS->pBackend->uninit != NULL) {
        pFS->pBackend->uninit(pFS);
    }

    fs_free(pFS->pReadMountPoints, &pFS->allocationCallbacks);
    pFS->pReadMountPoints = NULL;

    fs_free(pFS->pWriteMountPoints, &pFS->allocationCallbacks);
    pFS->pWriteMountPoints = NULL;

    fs_free(pFS->pOpenedArchives, &pFS->allocationCallbacks);
    pFS->pOpenedArchives = NULL;

    fs_mtx_destroy(&pFS->archiveLock);

    fs_free(pFS, &pFS->allocationCallbacks);
}

FS_API fs_result fs_ioctl(fs* pFS, int request, void* pArg)
{
    if (pFS == NULL) {
        return FS_INVALID_ARGS;
    }

    return fs_backend_ioctl(pFS->pBackend, pFS, request, pArg);
}

FS_API fs_result fs_remove(fs* pFS, const char* pFilePath)
{
    if (pFS == NULL || pFilePath == NULL) {
        return FS_INVALID_ARGS;
    }

    return fs_backend_remove(pFS->pBackend, pFS, pFilePath);
}

FS_API fs_result fs_rename(fs* pFS, const char* pOldName, const char* pNewName)
{
    if (pFS == NULL || pOldName == NULL || pNewName == NULL) {
        return FS_INVALID_ARGS;
    }

    return fs_backend_rename(pFS->pBackend, pFS, pOldName, pNewName);
}

FS_API fs_result fs_mkdir(fs* pFS, const char* pPath)
{
    char pRunningPathStack[1024];
    char* pRunningPathHeap = NULL;
    char* pRunningPath = pRunningPathStack;
    size_t runningPathLen = 0;
    fs_path_iterator iSegment;
    const fs_backend* pBackend;

    pBackend = fs_get_backend_or_default(pFS);

    if (pBackend == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pPath == NULL) {
        return FS_INVALID_ARGS;
    }

    /* We need to iterate over each segment and create the directory. If any of these fail we'll need to abort. */
    if (fs_path_first(pPath, FS_NULL_TERMINATED, &iSegment) != FS_SUCCESS) {
        return FS_SUCCESS;  /* It's an empty path. */
    }

    for (;;) {
        fs_result result;

        if (runningPathLen + iSegment.segmentLength + 1 >= sizeof(pRunningPathStack)) {
            if (pRunningPath == pRunningPathStack) {
                pRunningPathHeap = (char*)fs_malloc(runningPathLen + iSegment.segmentLength + 1, fs_get_allocation_callbacks(pFS));
                if (pRunningPathHeap == NULL) {
                    return FS_OUT_OF_MEMORY;
                }

                FS_COPY_MEMORY(pRunningPathHeap, pRunningPathStack, runningPathLen);
                pRunningPath = pRunningPathHeap;
            } else {
                char* pNewRunningPathHeap;

                pNewRunningPathHeap = (char*)fs_realloc(pRunningPathHeap, runningPathLen + iSegment.segmentLength + 1, fs_get_allocation_callbacks(pFS));
                if (pNewRunningPathHeap == NULL) {
                    fs_free(pRunningPathHeap, fs_get_allocation_callbacks(pFS));
                    return FS_OUT_OF_MEMORY;
                }

                pRunningPath = pNewRunningPathHeap;
            }
        }

        FS_COPY_MEMORY(pRunningPath + runningPathLen, iSegment.pFullPath + iSegment.segmentOffset, iSegment.segmentLength);
        runningPathLen += iSegment.segmentLength;
        pRunningPath[runningPathLen] = '\0';

        result = fs_backend_mkdir(pBackend, pFS, pRunningPath);

        /* We just pretend to be successful if the directory already exists. */
        if (result == FS_ALREADY_EXISTS) {
            result = FS_SUCCESS;
        }

        if (result != FS_SUCCESS) {
            if (pRunningPathHeap != NULL) {
                fs_free(pRunningPathHeap, fs_get_allocation_callbacks(pFS));
            }

            return result;
        }

        pRunningPath[runningPathLen] = '/';
        runningPathLen += 1;

        result = fs_path_next(&iSegment);
        if (result != FS_SUCCESS) {
            break;
        }
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_info(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    if (pInfo == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_ZERO_OBJECT(pInfo);

    return fs_file_open_or_info(pFS, pPath, openMode, NULL, pInfo);
}

FS_API fs_stream* fs_get_stream(fs* pFS)
{
    if (pFS == NULL) {
        return NULL;
    }

    return pFS->pStream;
}

FS_API const fs_allocation_callbacks* fs_get_allocation_callbacks(fs* pFS)
{
    if (pFS == NULL) {
        return NULL;
    }

    return &pFS->allocationCallbacks;
}

FS_API void* fs_get_backend_data(fs* pFS)
{
    size_t offset = sizeof(fs);

    if (pFS == NULL) {
        return NULL;
    }

    if (pFS->isOwnerOfArchiveTypes) {
        offset += pFS->archiveTypesAllocSize;
    }

    return FS_OFFSET_PTR(pFS, offset);
}

FS_API size_t fs_get_backend_data_size(fs* pFS)
{
    if (pFS == NULL) {
        return 0;
    }

    return pFS->backendDataSize;
}



static fs_result fs_open_archive_nolock(fs* pFS, const fs_backend* pBackend, void* pBackendConfig, const char* pArchivePath, size_t archivePathLen, int openMode, fs** ppArchive)
{
    fs_result result;
    fs* pArchive;
    fs_config archiveConfig;
    fs_file* pArchiveFile;
    char pArchivePathNTStack[1024];
    char* pArchivePathNTHeap = NULL;    /* <-- Must be initialized to null. */
    char* pArchivePathNT;
    fs_proxy_config proxyConfig;
    fs_opened_archive* pOpenedArchive;

    /*
    The first thing to do is check if the archive has already been opened. If so, we just increment
    the reference count and return the already-loaded fs object.
    */
    pOpenedArchive = fs_find_opened_archive(pFS, pArchivePath, archivePathLen);
    if (pOpenedArchive != NULL) {
        if (pOpenedArchive->refCount == 0) {
            pOpenedArchive->refCount += 1;
        } else {
            fs_increment_opened_archive_ref_count(pFS, pOpenedArchive->pArchive);
        }

        *ppArchive = pOpenedArchive->pArchive;
        return FS_SUCCESS;
    }

    /*
    Getting here means the archive is not cached. We'll need to open it. Unfortunately our path is
    not null terminated so we'll need to do that now. We'll try to avoid a heap allocation if we
    can.
    */
    if (archivePathLen == FS_NULL_TERMINATED) {
        pArchivePathNT = (char*)pArchivePath;   /* <-- Safe cast. We won't be modifying this. */
    } else {
        if (archivePathLen >= sizeof(pArchivePathNTStack)) {
            pArchivePathNTHeap = (char*)fs_malloc(archivePathLen + 1, fs_get_allocation_callbacks(pFS));
            if (pArchivePathNTHeap == NULL) {
                return FS_OUT_OF_MEMORY;
            }

            pArchivePathNT = pArchivePathNTHeap;
        } else {
            pArchivePathNT = pArchivePathNTStack;
        }

        FS_COPY_MEMORY(pArchivePathNT, pArchivePath, archivePathLen);
        pArchivePathNT[archivePathLen] = '\0';
    }

    result = fs_file_open(pFS, pArchivePathNT, openMode, &pArchiveFile);
    if (result != FS_SUCCESS) {
        fs_free(pArchivePathNTHeap, fs_get_allocation_callbacks(pFS));
        return result;
    }

    proxyConfig.pBackend = pBackend;
    proxyConfig.pBackendConfig = pBackendConfig;

    archiveConfig = fs_config_init(FS_PROXY, &proxyConfig, fs_file_get_stream(pArchiveFile));
    archiveConfig.pAllocationCallbacks = fs_get_allocation_callbacks(pFS);

    result = fs_init(&archiveConfig, &pArchive);
    fs_free(pArchivePathNTHeap, fs_get_allocation_callbacks(pFS));

    if (result != FS_SUCCESS) { /* <-- This is the result of fs_init().*/
        fs_file_close(pArchiveFile);
        return result;
    }

    /*
    We need to support the ability to open archives within archives. To do this, the archive fs
    object needs to inherit the registered archive types. Fortunately this is easy because we do
    this as one single allocation which means we can just reference it directly. The API has a
    restriction that archive type registration cannot be modified after a file has been opened.
    */
    pArchive->pArchiveTypes         = pFS->pArchiveTypes;
    pArchive->archiveTypesAllocSize = pFS->archiveTypesAllocSize;
    pArchive->isOwnerOfArchiveTypes = FS_FALSE;

    /* Add the new archive to the cache. */
    result = fs_add_opened_archive(pFS, pArchive, pArchivePath, archivePathLen);
    if (result != FS_SUCCESS) {
        fs_uninit(pArchive);
        fs_file_close(pArchiveFile);
        return result;
    }

    fs_increment_opened_archive_ref_count(pFS, pArchive);

    *ppArchive = pArchive;
    return FS_SUCCESS;
}

FS_API fs_result fs_open_archive_ex(fs* pFS, const fs_backend* pBackend, void* pBackendConfig, const char* pArchivePath, size_t archivePathLen, int openMode, fs** ppArchive)
{
    fs_result result;

    if (ppArchive == NULL) {
        return FS_INVALID_ARGS;
    }

    *ppArchive = NULL;

    if (pFS == NULL || pBackend == NULL || pArchivePath == NULL || archivePathLen == 0) {
        return FS_INVALID_ARGS;
    }

    /*
    It'd be nice to be able to resolve the path here to eliminate any "." and ".." segments thereby
    making the path always consistent for a given archive. However, I cannot think of a way to do
    this robustly without having a backend-specific function like a `resolve()` or whatnot. The
    problem is that the path might be absolute, or it might be relative, and to get it right,
    parcticularly when dealing with different operating systems' ways of specifying an absolute
    path, you really need to have the support of the backend. I might add support for this later.
    */

    fs_mtx_lock(&pFS->archiveLock);
    {
        result = fs_open_archive_nolock(pFS, pBackend, pBackendConfig, pArchivePath, archivePathLen, openMode, ppArchive);
    }
    fs_mtx_unlock(&pFS->archiveLock);

    return result;
}

FS_API fs_result fs_open_archive(fs* pFS, const char* pArchivePath, int openMode, fs** ppArchive)
{
    fs_result backendIteratorResult;
    fs_registered_backend_iterator iBackend;
    fs_result result;

    if (ppArchive == NULL) {
        return FS_INVALID_ARGS;
    }

    *ppArchive = NULL;  /* Safety. */

    if (pFS == NULL || pArchivePath == NULL) {
        return FS_INVALID_ARGS;
    }

    /*
    There can be multiple backends registered to the same extension. We just iterate over each one in order
    and use the first that works.
    */
    result = FS_NO_BACKEND;
    for (backendIteratorResult = fs_first_registered_backend(pFS, &iBackend); backendIteratorResult == FS_SUCCESS; backendIteratorResult = fs_next_registered_backend(&iBackend)) {
        if (fs_path_extension_equal(pArchivePath, FS_NULL_TERMINATED, iBackend.pExtension, iBackend.extensionLen)) {
            result = fs_open_archive_ex(pFS, iBackend.pBackend, iBackend.pBackendConfig, pArchivePath, FS_NULL_TERMINATED, openMode, ppArchive);
            if (result == FS_SUCCESS) {
                return FS_SUCCESS;
            }
        }
    }

    /* Failed to open from any archive backend. */
    return result;
}

FS_API void fs_close_archive(fs* pArchive)
{
    fs* pOwnerFS;

    /* This function should only ever be called for archives that were opened with fs_open_archive(). */
    FS_ASSERT(pArchive->pBackend == FS_PROXY);

    pOwnerFS = fs_proxy_get_owner_fs(pArchive);
    FS_ASSERT(pOwnerFS != NULL);

    fs_mtx_lock(&pOwnerFS->archiveLock);
    {
        fs_decrement_opened_archive_ref_count(pOwnerFS, pArchive);
    }
    fs_mtx_unlock(&pOwnerFS->archiveLock);

    /*
    Now we need to do a bit of garbage collection of opened archives. When files are opened sequentially
    from a single archive, it's more efficient to keep the archive open for a bit rather than constantly
    loading and unloading the archive for every single file. Zip, for example, can be inefficient because
    it requires re-reading and processing the central directory every time you open the archive. The
    issue is that we probably don't want to have too many archives open at a time since it's a bit
    wasteful on memory.

    What we're going to do is use a threshold of how many archives we want to keep open at any given
    time. If we're over this threshold, we'll unload any archives that are no longer in use until we
    get below the threshold.
    */
    fs_gc_archives(pOwnerFS, FS_GC_POLICY_THRESHOLD);
}

static void fs_gc_archives_nolock(fs* pFS, int policy)
{
    size_t unreferencedCount = 0;
    size_t collectionCount = 0;
    size_t cursor = 0;

    FS_ASSERT(pFS != NULL);

    /*
    If we're doing a full garbage collection we need to recursively run the garbage collection process
    on opened archives.
    */
    if ((policy & FS_GC_POLICY_FULL) != 0) {
        cursor = 0;
        while (cursor < pFS->openedArchivesSize) {
            fs_opened_archive* pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, cursor);
            FS_ASSERT(pOpenedArchive != NULL);

            fs_gc_archives(pOpenedArchive->pArchive, policy);
            cursor += FS_ALIGN(sizeof(fs*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);
        }
    }


    /* The first thing to do is count how many unreferenced archives there are. */
    cursor = 0;
    while (cursor < pFS->openedArchivesSize) {
        fs_opened_archive* pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, cursor);

        if (pOpenedArchive->refCount == 0) {
            unreferencedCount += 1;
        }

        /* Getting here means this archive is not the one we're looking for. */
        cursor += FS_ALIGN(sizeof(fs*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);
    }

    /* Now we need to determine how many archives we should unload. */
    if ((policy & FS_GC_POLICY_THRESHOLD) != 0) {
        if (unreferencedCount > fs_get_archive_gc_threshold(pFS)) {
            collectionCount = unreferencedCount - fs_get_archive_gc_threshold(pFS);
        } else {
            collectionCount = 0;    /* We're below the threshold. Don't collect anything. */
        }
    } else if ((policy & FS_GC_POLICY_FULL) != 0) {
        collectionCount = unreferencedCount;
    } else {
        FS_ASSERT(!"Invalid GC policy.");
    }

    /* Now we need to unload the archives. */
    cursor = 0;
    while (collectionCount > 0 && cursor < pFS->openedArchivesSize) {
        fs_opened_archive* pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, cursor);
        if (pOpenedArchive->refCount == 0) {
            fs_file* pArchiveFile;

            pArchiveFile = fs_proxy_get_archive_file(pOpenedArchive->pArchive);
            FS_ASSERT(pArchiveFile != NULL);

            fs_uninit(pOpenedArchive->pArchive);
            fs_file_close(pArchiveFile);

            /* We can remove the archive from the list only after it's been closed. */
            fs_remove_opened_archive(pFS, pOpenedArchive);

            collectionCount -= 1;

            /* Note that we're not advancing the cursor here because we just removed this entry. */
        } else {
            cursor += FS_ALIGN(sizeof(fs*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);
        }
    }
}

FS_API void fs_gc_archives(fs* pFS, int policy)
{
    if (pFS == NULL) {
        return;
    }

    if (policy == 0 || ((policy & FS_GC_POLICY_THRESHOLD) != 0 && (policy & FS_GC_POLICY_FULL) != 0)) {
        return; /* Invalid policy. Must specify FS_GC_POLICY_THRESHOLD or FS_GC_POLICY_FULL, but not both. */
    }

    fs_mtx_lock(&pFS->archiveLock);
    {
        fs_gc_archives_nolock(pFS, policy);
    }
    fs_mtx_unlock(&pFS->archiveLock);
}

FS_API void fs_set_archive_gc_threshold(fs* pFS, size_t threshold)
{
    if (pFS == NULL) {
        return;
    }

    pFS->archiveGCThreshold = threshold;
}

FS_API size_t fs_get_archive_gc_threshold(fs* pFS)
{
    if (pFS == NULL) {
        return 0;
    }

    return pFS->archiveGCThreshold;
}


static size_t fs_file_duplicate_alloc_size(fs* pFS)
{
    return sizeof(fs_file) + fs_backend_file_alloc_size(fs_get_backend_or_default(pFS), pFS);
}

static void fs_file_preinit_no_stream(fs_file* pFile, fs* pFS, size_t backendDataSize)
{
    FS_ASSERT(pFile != NULL);

    pFile->pFS = pFS;
    pFile->backendDataSize = backendDataSize;
}

static void fs_file_uninit(fs_file* pFile);


static fs_result fs_file_stream_read(fs_stream* pStream, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    return fs_file_read((fs_file*)pStream, pDst, bytesToRead, pBytesRead);
}

static fs_result fs_file_stream_write(fs_stream* pStream, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    return fs_file_write((fs_file*)pStream, pSrc, bytesToWrite, pBytesWritten);
}

static fs_result fs_file_stream_seek(fs_stream* pStream, fs_int64 offset, fs_seek_origin origin)
{
    return fs_file_seek((fs_file*)pStream, offset, origin);
}

static fs_result fs_file_stream_tell(fs_stream* pStream, fs_int64* pCursor)
{
    return fs_file_tell((fs_file*)pStream, pCursor);
}

static size_t fs_file_stream_alloc_size(fs_stream* pStream)
{
    return fs_file_duplicate_alloc_size(fs_file_get_fs((fs_file*)pStream));
}

static fs_result fs_file_stream_duplicate(fs_stream* pStream, fs_stream* pDuplicatedStream)
{
    fs_result result;
    fs_file* pStreamFile = (fs_file*)pStream;
    fs_file* pDuplicatedStreamFile = (fs_file*)pDuplicatedStream;

    FS_ASSERT(pStreamFile != NULL);
    FS_ASSERT(pDuplicatedStreamFile != NULL);

    /* The stream will already have been initialized at a higher level in fs_stream_duplicate(). */
    fs_file_preinit_no_stream(pDuplicatedStreamFile, fs_file_get_fs(pStreamFile), pStreamFile->backendDataSize);

    result = fs_backend_file_duplicate(fs_get_backend_or_default(pStreamFile->pFS), pStreamFile, pDuplicatedStreamFile);
    if (result != FS_SUCCESS) {
        return result;
    }

    return FS_SUCCESS;
}

static void fs_file_stream_uninit(fs_stream* pStream)
{
    /* We need to uninitialize the file, but *not* free it. Freeing will be done at a higher level in fs_stream_delete_duplicate(). */
    fs_file_uninit((fs_file*)pStream);
}

static fs_stream_vtable fs_file_stream_vtable =
{
    fs_file_stream_read,
    fs_file_stream_write,
    fs_file_stream_seek,
    fs_file_stream_tell,
    fs_file_stream_alloc_size,
    fs_file_stream_duplicate,
    fs_file_stream_uninit
};




static const fs_backend* fs_file_get_backend(fs_file* pFile)
{
    return fs_get_backend_or_default(fs_file_get_fs(pFile));
}

static fs_result fs_open_or_info_from_archive(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile, fs_file_info* pInfo)
{
    /*
    NOTE: A lot of return values are FS_DOES_NOT_EXIST. This is because this function will only be called
    in response to a FS_DOES_NOT_EXIST in the first call to fs_file_open() which makes this a logical result.
    */
    fs_result result;
    fs_path_iterator iFilePathSeg;
    fs_path_iterator iFilePathSegLast;

    FS_ASSERT(pFS != NULL);

    /*
    We can never open from an archive if we're opening in opaque mode. This mode is intended to
    be used such that only files exactly represented in the file system can be opened.
    */
    if (FS_IS_OPAQUE(openMode)) {    
        return FS_DOES_NOT_EXIST;
    }

    /* If no archive types have been configured we can abort early. */
    if (pFS->archiveTypesAllocSize == 0) {
        return FS_DOES_NOT_EXIST;
    }

    /*
    We need to iterate over each segment in the path and then iterate over each file in that folder and
    check for archives. If we find an archive we simply try loading from that. Note that if the path
    segment itself points to an archive, we *must* open it from that archive because the caller has
    explicitly asked for that archive.
    */
    if (fs_path_first(pFilePath, FS_NULL_TERMINATED, &iFilePathSeg) != FS_SUCCESS) {
        return FS_DOES_NOT_EXIST;
    }

    /* Grab the last part of the file path so we can check if we're up to the file name. */
    fs_path_last(pFilePath, FS_NULL_TERMINATED, &iFilePathSegLast);

    do
    {
        fs_result backendIteratorResult;
        fs_registered_backend_iterator iBackend;
        fs_bool32 isArchive = FS_FALSE;

        /* Skip over "." and ".." segments. */
        if (fs_strncmp(iFilePathSeg.pFullPath, ".", iFilePathSeg.segmentLength) == 0) {
            continue;
        }
        if (fs_strncmp(iFilePathSeg.pFullPath, "..", iFilePathSeg.segmentLength) == 0) {
            continue;
        }

        /* If an archive has been explicitly listed in the path, we must try loading from that. */
        for (backendIteratorResult = fs_first_registered_backend(pFS, &iBackend); backendIteratorResult == FS_SUCCESS; backendIteratorResult = fs_next_registered_backend(&iBackend)) {
            if (fs_path_extension_equal(iFilePathSeg.pFullPath + iFilePathSeg.segmentOffset, iFilePathSeg.segmentLength, iBackend.pExtension, iBackend.extensionLen)) {
                isArchive = FS_TRUE;

                /* This path points to an explicit archive. If this is the file we're trying to actually load, we'll want to handle that too. */
                if (fs_path_iterators_compare(&iFilePathSeg, &iFilePathSegLast) == 0) {
                    /*
                    The archive file itself is the last segment in the path which means that's the file
                    we're actually trying to load. We shouldn't need to try opening this here because if
                    it existed and was able to be opened, it should have been done so at a higher level.
                    */
                    return FS_DOES_NOT_EXIST;
                } else {
                    fs* pArchive;

                    result = fs_open_archive_ex(pFS, iBackend.pBackend, iBackend.pBackendConfig, iFilePathSeg.pFullPath, iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength, FS_OPAQUE | openMode, &pArchive);
                    if (result != FS_SUCCESS) {
                        /*
                        We failed to open the archive. If it's due to the archive not existing we just continue searching. Otherwise
                        a proper error code needs to be returned.
                        */
                        if (result != FS_DOES_NOT_EXIST) {
                            return result;
                        } else {
                            continue;
                        }
                    }

                    result = fs_file_open_or_info(pArchive, iFilePathSeg.pFullPath + iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength + 1, openMode, ppFile, pInfo);
                    if (result != FS_SUCCESS) {
                        fs_close_archive(pArchive);
                        return result;
                    }

                    if (ppFile != NULL) {
                        /* The archive must be unreferenced when closing the file. */
                        fs_file_proxy_set_unref_archive_on_close(*ppFile, FS_TRUE);
                    } else {
                        /* We were only grabbing file info. We can close the archive straight away. */
                        fs_close_archive(pArchive);
                    }

                    return FS_SUCCESS;
                }
            }
        }

        /* If the path has an extension of an archive, but we still manage to get here, it means the archive doesn't exist. */
        if (isArchive) {
            return FS_DOES_NOT_EXIST;
        }

        /*
        Getting here means this part of the path does not look like an archive. We will assume it's a folder and try
        iterating it using opaque mode to get the contents.
        */
        if (FS_IS_VERBOSE(openMode)) {
            /*
            The caller has requested opening in verbose mode. In this case we don't want to be scanning for
            archives. Instead, any archives will be explicitly listed in the path. We just skip this path in
            this case.
            */
            continue;
        } else {
            /*
            Getting here means we're opening in transparent mode. We'll need to search for archives and check
            them one by one. This is the slow path.

            To do this we opaquely iterate over each file in the currently iterated file path. If any of these
            files are recognized as archives, we'll load up that archive and then try opening the file from
            there. If it works we return, otherwise we unload that archive and keep trying.
            */
            fs_iterator* pIterator;

            for (pIterator = fs_backend_first(fs_get_backend_or_default(pFS), pFS, iFilePathSeg.pFullPath, iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength); pIterator != NULL; pIterator = fs_backend_next(fs_get_backend_or_default(pFS), pIterator)) {
                for (backendIteratorResult = fs_first_registered_backend(pFS, &iBackend); backendIteratorResult == FS_SUCCESS; backendIteratorResult = fs_next_registered_backend(&iBackend)) {
                    if (fs_path_extension_equal(pIterator->pName, pIterator->nameLen, iBackend.pExtension, iBackend.extensionLen)) {
                        /* Looks like an archive. We can load this one up and try opening from it. */
                        fs* pArchive;
                        char pArchivePathNTStack[1024];
                        char* pArchivePathNTHeap = NULL;    /* <-- Must be initialized to null. */
                        char* pArchivePathNT;
                        size_t archivePathLen;

                        archivePathLen = iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength + 1 + pIterator->nameLen;
                        if (archivePathLen >= sizeof(pArchivePathNTStack)) {
                            pArchivePathNTHeap = (char*)fs_malloc(archivePathLen + 1, fs_get_allocation_callbacks(pFS));
                            if (pArchivePathNTHeap == NULL) {
                                fs_backend_free_iterator(fs_get_backend_or_default(pFS), pIterator);
                                return FS_OUT_OF_MEMORY;
                            }

                            pArchivePathNT = pArchivePathNTHeap;
                        } else {
                            pArchivePathNT = pArchivePathNTStack;
                        }

                        FS_COPY_MEMORY(pArchivePathNT, iFilePathSeg.pFullPath, iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength);
                        pArchivePathNT[iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength] = '/';
                        FS_COPY_MEMORY(pArchivePathNT + iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength + 1, pIterator->pName, pIterator->nameLen);
                        pArchivePathNT[archivePathLen] = '\0';

                        /* At this point we've constructed the archive name and we can now open it. */
                        result = fs_open_archive_ex(pFS, iBackend.pBackend, iBackend.pBackendConfig, pArchivePathNT, FS_NULL_TERMINATED, FS_OPAQUE | openMode, &pArchive);
                        fs_free(pArchivePathNTHeap, fs_get_allocation_callbacks(pFS));

                        if (result != FS_SUCCESS) { /* <-- This is checking the result of fs_open_archive_ex(). */
                            continue;   /* Failed to open this archive. Keep looking. */
                        }

                        /*
                        Getting here means we've successfully opened the archive. We can now try opening the file
                        from there. The path we load from will be the next segment in the path.
                        */
                        result = fs_file_open_or_info(pArchive, iFilePathSeg.pFullPath + iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength + 1, openMode, ppFile, pInfo);  /* +1 to skip the separator. */
                        if (result != FS_SUCCESS) {
                            fs_close_archive(pArchive);
                            continue;  /* Failed to open the file. Keep looking. */
                        }

                        /* The iterator is no longer required. */
                        fs_backend_free_iterator(fs_get_backend_or_default(pFS), pIterator);
                        pIterator = NULL;

                        if (ppFile != NULL) {
                            /* The archive must be unreferenced when closing the file. */
                            fs_file_proxy_set_unref_archive_on_close(*ppFile, FS_TRUE);
                        } else {
                            /* We were only grabbing file info. We can close the archive straight away. */
                            fs_close_archive(pArchive);
                        }

                        /* Getting here means we successfully opened the file. We're done. */
                        return FS_SUCCESS;
                    }
                }

                /*
                Getting here means this file could not be loaded from any registered archive types. Just move on
                to the next file.
                */
            }

            /*
            Getting here means we couldn't find the file within any known archives in this directory. From here
            we just move onto the segment segment in the path and keep looking.
            */
        }
    } while (fs_path_next(&iFilePathSeg) == FS_SUCCESS);
    
    /* Getting here means we reached the end of the path and never did find the file. */
    return FS_DOES_NOT_EXIST;
}

static fs_result fs_file_alloc(fs* pFS, fs_file** ppFile)
{
    fs_file* pFile;
    fs_result result;
    const fs_backend* pBackend;
    size_t backendDataSizeInBytes = 0;

    FS_ASSERT(ppFile != NULL);
    FS_ASSERT(*ppFile == NULL);  /* <-- File must not already be allocated when calling this. */

    pBackend = fs_get_backend_or_default(pFS);
    FS_ASSERT(pBackend != NULL);

    backendDataSizeInBytes = fs_backend_file_alloc_size(pBackend, pFS);

    pFile = (fs_file*)fs_calloc(sizeof(fs_file) + backendDataSizeInBytes, fs_get_allocation_callbacks(pFS));
    if (pFile == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    /* A file is a stream. */
    result = fs_stream_init(&fs_file_stream_vtable, &pFile->stream);
    if (result != 0) {
        fs_free(pFile, fs_get_allocation_callbacks(pFS));
        return result;
    }

    pFile->pFS = pFS;
    pFile->backendDataSize = backendDataSizeInBytes;

    *ppFile = pFile;
    return FS_SUCCESS;
}

static fs_result fs_file_alloc_if_necessary(fs* pFS, fs_file** ppFile)
{
    FS_ASSERT(ppFile != NULL);

    if (*ppFile == NULL) {
        return fs_file_alloc(pFS, ppFile);
    } else {
        return FS_SUCCESS;
    }
}

static fs_result fs_file_alloc_if_necessary_and_open_or_info(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile, fs_file_info* pInfo)
{
    fs_result result;
    const fs_backend* pBackend;

    if (ppFile != NULL) {
        result = fs_file_alloc_if_necessary(pFS, ppFile);
        if (result != FS_SUCCESS) {
            *ppFile = NULL;
            return result;
        }
    }

    pBackend = fs_get_backend_or_default(pFS);

    if (pBackend == NULL) {
        return FS_INVALID_ARGS;
    }

    /*
    Take a copy of the file system's stream if necessary. We only need to do this if we're opening the file, and if
    the owner `fs` object `pFS` has itself has a stream.
    */
    if (pFS != NULL && ppFile != NULL) {
        fs_stream* pFSStream = pFS->pStream;
        if (pFSStream != NULL) {
            result = fs_stream_duplicate(pFSStream, fs_get_allocation_callbacks(pFS), &(*ppFile)->pStreamForBackend);
            if (result != FS_SUCCESS) {
                return result;
            }
        }
    }

    /*
    This is the lowest level opening function. We never want to look at mounts when opening from here. The input
    file path should already be prefixed with the mount point.

    UPDATE: Actually don't want to explicitly append FS_IGNORE_MOUNTS here because it can affect the behavior of
    proxy and passthrough style backends. Some backends, particularly FS_SUBFS, will call straight into the owner
    `fs` object which might depend on those mounts being handled for correct behaviour.
    */
    /*openMode |= FS_IGNORE_MOUNTS;*/

    if (ppFile != NULL) {
        /* Create the directory structure if necessary. */
        if ((openMode & FS_WRITE) != 0 && (openMode & FS_NO_CREATE_DIRS) == 0) {
            char pDirPathStack[1024];
            char* pDirPathHeap = NULL;
            char* pDirPath;
            int dirPathLen;

            dirPathLen = fs_path_directory(pDirPathStack, sizeof(pDirPathStack), pFilePath, FS_NULL_TERMINATED);
            if (dirPathLen >= (int)sizeof(pDirPathStack)) {
                pDirPathHeap = (char*)fs_malloc(dirPathLen + 1, fs_get_allocation_callbacks(pFS));
                if (pDirPathHeap == NULL) {
                    return FS_OUT_OF_MEMORY;
                }

                dirPathLen = fs_path_directory(pDirPathHeap, dirPathLen + 1, pFilePath, FS_NULL_TERMINATED);
                if (dirPathLen < 0) {
                    fs_stream_delete_duplicate((*ppFile)->pStreamForBackend, fs_get_allocation_callbacks(pFS));
                    fs_free(pDirPathHeap, fs_get_allocation_callbacks(pFS));
                    return FS_ERROR;    /* Should never hit this. */
                }

                pDirPath = pDirPathHeap;
            } else {
                pDirPath = pDirPathStack;
            }

            result = fs_mkdir(pFS, pDirPath);
            if (result != FS_SUCCESS) {
                fs_stream_delete_duplicate((*ppFile)->pStreamForBackend, fs_get_allocation_callbacks(pFS));
                return result;
            }
        }

        result = fs_backend_file_open(pBackend, pFS, (*ppFile)->pStreamForBackend, pFilePath, openMode, *ppFile);

        if (result != FS_SUCCESS) {
            fs_stream_delete_duplicate((*ppFile)->pStreamForBackend, fs_get_allocation_callbacks(pFS));
        }

        /* Grab the info from the opened file if we're also grabbing that. */
        if (result == FS_SUCCESS && pInfo != NULL) {
            fs_backend_file_info(pBackend, *ppFile, pInfo);
        }
    } else {
        if (pInfo != NULL) {
            result = fs_backend_info(pBackend, pFS, pFilePath, openMode, pInfo);
        } else {
            result = FS_INVALID_ARGS;
        }
    }

    if (!FS_IS_OPAQUE(openMode) && (openMode & FS_WRITE) == 0) {
        /*
        If we failed to open the file because it doesn't exist we need to try loading it from an
        archive. We can only do this if the file is being loaded by an explicitly initialized fs
        object.
        */
        if (pFS != NULL && (result == FS_DOES_NOT_EXIST || result == FS_NOT_DIRECTORY)) {
            if (ppFile != NULL) {
                fs_free(*ppFile, fs_get_allocation_callbacks(pFS));
                *ppFile = NULL;
            }

            result = fs_open_or_info_from_archive(pFS, pFilePath, openMode, ppFile, pInfo);
        }
    }

    return result;
}

static fs_result fs_validate_path(const char* pPath, size_t pathLen, int mode)
{
    if ((mode & FS_NO_SPECIAL_DIRS) != 0) {
        fs_path_iterator iPathSeg;
        fs_result result;

        for (result = fs_path_first(pPath, pathLen, &iPathSeg); result == FS_SUCCESS; result = fs_path_next(&iPathSeg)) {
            if (fs_strncmp(iPathSeg.pFullPath, ".", iPathSeg.segmentLength) == 0) {
                return FS_INVALID_ARGS;
            }

            if (fs_strncmp(iPathSeg.pFullPath, "..", iPathSeg.segmentLength) == 0) {
                return FS_INVALID_ARGS;
            }
        }
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_file_open_or_info(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile, fs_file_info* pInfo)
{
    fs_result result;
    fs_result mountPointIerationResult;

    if (ppFile == NULL && pInfo == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pFilePath == NULL) {
        return FS_INVALID_ARGS;
    }

    /* The open mode cannot be 0 when opening a file. It can only be 0 when retrieving info. */
    if (ppFile != NULL && openMode == 0) {
        return FS_INVALID_ARGS;
    }

    result = fs_validate_path(pFilePath, FS_NULL_TERMINATED, openMode);
    if (result != FS_SUCCESS) {
        return result;
    }

    if ((openMode & FS_WRITE) != 0) {
        /*
        Opening in write mode. We need to open from a mount point. This is a bit different from opening
        in read mode because we want to use the mount point that most closely matches the start of the
        file path. Consider, for example, the following mount points:

            - config
            - config/global

        If we're trying to open "config/global/settings.cfg" we want to use the "config/global" mount
        point, not the "config" mount point. This is because the "config/global" mount point is more
        specific and therefore more likely to be the correct one.

        We'll need to iterate over every mount point and keep track of the mount point with the longest
        prefix that matches the start of the file path.
        */
        if (pFS != NULL) {
            fs_mount_list_iterator iMountPoint;
            fs_mount_point* pBestMountPoint = NULL;
            const char* pBestMountPointPath = NULL;
            const char* pBestMountPointFileSubPath = NULL;
            
            for (mountPointIerationResult = fs_mount_list_first(pFS->pWriteMountPoints, &iMountPoint); mountPointIerationResult == FS_SUCCESS; mountPointIerationResult = fs_mount_list_next(&iMountPoint)) {
                const char* pFileSubPath = fs_path_trim_base(pFilePath, FS_NULL_TERMINATED, iMountPoint.pMountPointPath, FS_NULL_TERMINATED);
                if (pFileSubPath == NULL) {
                    continue;   /* The file path doesn't start with this mount point so skip. */
                }

                if (pBestMountPointFileSubPath == NULL || strlen(pFileSubPath) < strlen(pBestMountPointFileSubPath)) {
                    pBestMountPoint = iMountPoint.internal.pMountPoint;
                    pBestMountPointPath = iMountPoint.pPath;
                    pBestMountPointFileSubPath = pFileSubPath;
                }
            }

            if (pBestMountPoint != NULL) {
                char pActualPathStack[1024];
                char* pActualPathHeap = NULL;
                char* pActualPath;
                int actualPathLen;
                char pActualPathCleanStack[1024];
                char* pActualPathCleanHeap = NULL;
                char* pActualPathClean;
                int actualPathCleanLen;
                unsigned int cleanOptions = (openMode & FS_NO_ABOVE_ROOT_NAVIGATION);            

                /* If the mount point starts with a root segment, i.e. "/", we cannot allow navigation above that. */
                if (pBestMountPointPath[0] == '/' || pBestMountPointPath[0] == '\\') {
                    cleanOptions |= FS_NO_ABOVE_ROOT_NAVIGATION;
                }


                /* Here is where we append the cleaned sub-path to the mount points actual path. */
                actualPathLen = fs_path_append(pActualPathStack, sizeof(pActualPathStack), pBestMountPointPath, pBestMountPoint->pathLen, pBestMountPointFileSubPath, FS_NULL_TERMINATED);
                if (actualPathLen > 0 && (size_t)actualPathLen >= sizeof(pActualPathStack)) {
                    /* Not enough room on the stack. Allocate on the heap. */
                    pActualPathHeap = (char*)fs_malloc(actualPathLen + 1, fs_get_allocation_callbacks(pFS));
                    if (pActualPathHeap == NULL) {
                        return FS_OUT_OF_MEMORY;
                    }

                    fs_path_append(pActualPathHeap, actualPathLen + 1, pBestMountPointPath, pBestMountPoint->pathLen, pBestMountPointFileSubPath, FS_NULL_TERMINATED);  /* <-- This should never fail. */
                    pActualPath = pActualPathHeap;
                } else {
                    pActualPath = pActualPathStack;
                }


                /* Now we need to clean the path. */
                actualPathCleanLen = fs_path_normalize(pActualPathCleanStack, sizeof(pActualPathCleanStack), pActualPath, FS_NULL_TERMINATED, cleanOptions);
                if (actualPathCleanLen < 0) {
                    fs_free(pActualPathHeap, fs_get_allocation_callbacks(pFS));
                    return FS_INVALID_OPERATION;    /* Most likely violating FS_NO_ABOVE_ROOT_NAVIGATION. */
                }

                if (actualPathCleanLen >= (int)sizeof(pActualPathCleanStack)) {
                    pActualPathCleanHeap = (char*)fs_malloc(actualPathCleanLen + 1, fs_get_allocation_callbacks(pFS));
                    if (pActualPathCleanHeap == NULL) {
                        fs_free(pActualPathHeap, fs_get_allocation_callbacks(pFS));
                        return FS_OUT_OF_MEMORY;
                    }

                    fs_path_normalize(pActualPathCleanHeap, actualPathCleanLen + 1, pActualPath, FS_NULL_TERMINATED, cleanOptions);    /* <-- This should never fail. */
                    pActualPathClean = pActualPathCleanHeap;
                } else {
                    pActualPathClean = pActualPathCleanStack;
                }

                fs_free(pActualPathHeap, fs_get_allocation_callbacks(pFS));
                pActualPathHeap = NULL;


                /* We now have enough information to open the file. */
                result = fs_file_alloc_if_necessary_and_open_or_info(pFS, pActualPathClean, openMode, ppFile, pInfo);

                fs_free(pActualPathCleanHeap, fs_get_allocation_callbacks(pFS));
                pActualPathCleanHeap = NULL;

                if (result == FS_SUCCESS) {
                    return FS_SUCCESS;
                } else {
                    return FS_DOES_NOT_EXIST;   /* Couldn't find the file from the best mount point. */
                }
            } else {
                return FS_DOES_NOT_EXIST;   /* Couldn't find an appropriate mount point. */
            }
        } else {
            /*
            No "fs" object was supplied. Open using the default backend without using mount points. This is as if you were
            opening a file using `fopen()`.
            */
            if ((openMode & FS_ONLY_MOUNTS) == 0) {
                return fs_file_alloc_if_necessary_and_open_or_info(pFS, pFilePath, openMode, ppFile, pInfo);
            } else {
                /*
                Getting here means only the mount points can be used to open the file (cannot open straight from
                the file system natively).
                */
                return FS_DOES_NOT_EXIST;
            }
        }
    } else {
        /* Opening in read mode. */
        fs_mount_list_iterator iMountPoint;

        if (pFS != NULL && (openMode & FS_IGNORE_MOUNTS) == 0) {
            for (mountPointIerationResult = fs_mount_list_first(pFS->pReadMountPoints, &iMountPoint); mountPointIerationResult == FS_SUCCESS; mountPointIerationResult = fs_mount_list_next(&iMountPoint)) {
                /*
                The first thing to do is check if the start of our file path matches the mount point. If it
                doesn't match we just skip to the next mount point.
                */
                char  pFileSubPathCleanStack[1024];
                char* pFileSubPathCleanHeap = NULL;
                char* pFileSubPathClean;
                int fileSubPathCleanLen;
                unsigned int cleanOptions = (openMode & FS_NO_ABOVE_ROOT_NAVIGATION);

                const char* pFileSubPath = fs_path_trim_base(pFilePath, FS_NULL_TERMINATED, iMountPoint.pMountPointPath, FS_NULL_TERMINATED);
                if (pFileSubPath == NULL) {
                    continue;
                }


                /* If the mount point starts with a root segment, i.e. "/", we cannot allow navigation above that. */
                if (iMountPoint.pMountPointPath[0] == '/' || iMountPoint.pMountPointPath[0] == '\\') {
                    cleanOptions |= FS_NO_ABOVE_ROOT_NAVIGATION;
                }

                /* We need to clean the file sub-path, but can skip it if FS_NO_SPECIAL_DIRS is specified since it's implied. */
                if ((openMode & FS_NO_SPECIAL_DIRS) == 0) {
                    fileSubPathCleanLen = fs_path_normalize(pFileSubPathCleanStack, sizeof(pFileSubPathCleanStack), pFileSubPath, FS_NULL_TERMINATED, cleanOptions);
                    if (fileSubPathCleanLen < 0) {
                        continue;    /* Most likely violating FS_NO_ABOVE_ROOT_NAVIGATION. Keep looking. */
                    }

                    if (fileSubPathCleanLen >= (int)sizeof(pFileSubPathCleanStack)) {
                        pFileSubPathCleanHeap = (char*)fs_malloc(fileSubPathCleanLen + 1, fs_get_allocation_callbacks(pFS));
                        if (pFileSubPathCleanHeap == NULL) {
                            return FS_OUT_OF_MEMORY;
                        }

                        fs_path_normalize(pFileSubPathCleanHeap, fileSubPathCleanLen + 1, pFileSubPath, FS_NULL_TERMINATED, cleanOptions);    /* <-- This should never fail. */
                        pFileSubPathClean = pFileSubPathCleanHeap;
                    } else {
                        pFileSubPathClean = pFileSubPathCleanStack;
                    }
                } else {
                    pFileSubPathClean = (char*)pFileSubPath;  /* Safe cast. Will not be modified past this point. */
                    fileSubPathCleanLen = (int)strlen(pFileSubPathClean);
                }


                /* The mount point could either be a directory or an archive. Both of these require slightly different handling. */
                if (iMountPoint.pArchive != NULL) {
                    /* The mount point is an archive. This is the simpler case. We just load the file directly from the archive. */
                    result = fs_file_open_or_info(iMountPoint.pArchive, pFileSubPathClean, openMode, ppFile, pInfo);
                    if (result == FS_SUCCESS) {
                        /*
                        The reference count of the archive must be incremented or else it'll get prematurely garbage collected. We only
                        need to do this if we're opening the file. It's not necessary if we're just grabbing file info.
                        */
                        if (ppFile != NULL) {
                            fs_increment_opened_archive_ref_count(pFS, iMountPoint.pArchive);
                        }

                        return FS_SUCCESS;
                    } else {
                        /* Failed to load from this archive. Keep looking. */
                    }
                } else {
                    /* The mount point is a directory. We need to combine the sub-path with the mount point's original path and then load the file. */
                    char  pActualPathStack[1024];
                    char* pActualPathHeap = NULL;
                    char* pActualPath;
                    int actualPathLen;

                    actualPathLen = fs_path_append(pActualPathStack, sizeof(pActualPathStack), iMountPoint.pPath, FS_NULL_TERMINATED, pFileSubPathClean, fileSubPathCleanLen);
                    if (actualPathLen > 0 && (size_t)actualPathLen >= sizeof(pActualPathStack)) {
                        /* Not enough room on the stack. Allocate on the heap. */
                        pActualPathHeap = (char*)fs_malloc(actualPathLen + 1, fs_get_allocation_callbacks(pFS));
                        if (pActualPathHeap == NULL) {
                            return FS_OUT_OF_MEMORY;
                        }

                        fs_path_append(pActualPathHeap, actualPathLen + 1, iMountPoint.pPath, FS_NULL_TERMINATED, pFileSubPathClean, fileSubPathCleanLen);   /* <-- This should never fail. */
                        pActualPath = pActualPathHeap;
                    } else {
                        pActualPath = pActualPathStack;
                    }

                    result = fs_file_alloc_if_necessary_and_open_or_info(pFS, pActualPath, openMode, ppFile, pInfo);

                    if (pActualPathHeap != NULL) {
                        fs_free(pActualPathHeap, fs_get_allocation_callbacks(pFS));
                        pActualPathHeap = NULL;
                    }

                    if (result == FS_SUCCESS) {
                        return FS_SUCCESS;
                    } else {
                        /* Failed to load from this directory. Keep looking. */
                    }
                }
            }
        }

        /* If we get here it means we couldn't find the file from our search paths. Try opening directly. */
        if ((openMode & FS_ONLY_MOUNTS) == 0) {
            result = fs_file_alloc_if_necessary_and_open_or_info(pFS, pFilePath, openMode, ppFile, pInfo);
            if (result == FS_SUCCESS) {
                return FS_SUCCESS;
            }
        } else {
            /*
            Getting here means only the mount points can be used to open the file (cannot open straight from
            the file system natively) and the file was unable to be opened from any of them. We need to
            return an error in this case.
            */
            result = FS_DOES_NOT_EXIST;
        }

        /* Getting here means we couldn't open the file from any mount points, nor could we open it directly. */
        if (ppFile != NULL) {
            fs_free(*ppFile, fs_get_allocation_callbacks(pFS));
            *ppFile = NULL;
        }
    }

    FS_ASSERT(result != FS_SUCCESS);
    return result;
}

FS_API fs_result fs_file_open(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile)
{
    if (ppFile == NULL) {
        return FS_INVALID_ARGS;
    }

    *ppFile = NULL;

    return fs_file_open_or_info(pFS, pFilePath, openMode, ppFile, NULL);
}

FS_API fs_result fs_file_open_from_handle(fs* pFS, void* hBackendFile, fs_file** ppFile)
{
    fs_result result;

    if (ppFile == NULL) {
        return FS_INVALID_ARGS;
    }

    *ppFile = NULL;

    result = fs_file_alloc_if_necessary(pFS, ppFile);
    if (result != FS_SUCCESS) {
        *ppFile = NULL;
        return result;
    }

    result = fs_backend_file_open_handle(fs_get_backend_or_default(pFS), pFS, hBackendFile, *ppFile);
    if (result != FS_SUCCESS) {
        fs_free(*ppFile, fs_get_allocation_callbacks(pFS));
        *ppFile = NULL;
    }

    return FS_SUCCESS;
}

static void fs_file_uninit(fs_file* pFile)
{
    fs_backend_file_close(fs_get_backend_or_default(fs_file_get_fs(pFile)), pFile);
}

FS_API void fs_file_close(fs_file* pFile)
{
    const fs_backend* pBackend = fs_file_get_backend(pFile);

    FS_ASSERT(pBackend != NULL);
    (void)pBackend;

    if (pFile == NULL) {
        return;
    }

    fs_file_uninit(pFile);

    if (pFile->pStreamForBackend != NULL) {
        fs_stream_delete_duplicate(pFile->pStreamForBackend, fs_get_allocation_callbacks(pFile->pFS));
    }

    fs_free(pFile, fs_get_allocation_callbacks(pFile->pFS));
}

FS_API fs_result fs_file_read(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_result result;
    size_t bytesRead;
    const fs_backend* pBackend;

    if (pFile == NULL || pDst == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_file_get_backend(pFile);
    FS_ASSERT(pBackend != NULL);

    bytesRead = 0;  /* <-- Just in case the backend doesn't clear this to zero. */
    result = fs_backend_file_read(pBackend, pFile, pDst, bytesToRead, &bytesRead);

    if (pBytesRead != NULL) {
        *pBytesRead = bytesRead;
    }

    if (result != FS_SUCCESS) {
        /* We can only return FS_AT_END if the number of bytes read was 0. */
        if (result == FS_AT_END) {
            if (bytesRead > 0) {
                result = FS_SUCCESS;
            }
        }

        return result;
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_file_write(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_result result;
    size_t bytesWritten;
    const fs_backend* pBackend;

    if (pFile == NULL || pSrc == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_file_get_backend(pFile);
    FS_ASSERT(pBackend != NULL);

    bytesWritten = 0;  /* <-- Just in case the backend doesn't clear this to zero. */
    result = fs_backend_file_write(pBackend, pFile, pSrc, bytesToWrite, &bytesWritten);

    if (pBytesWritten != NULL) {
        *pBytesWritten = bytesWritten;
    }

    return result;
}

FS_API fs_result fs_file_writef(fs_file* pFile, const char* fmt, ...)
{
    va_list args;
    fs_result result;

    va_start(args, fmt);
    result = fs_file_writefv(pFile, fmt, args);
    va_end(args);

    return result;
}

FS_API fs_result fs_file_writefv(fs_file* pFile, const char* fmt, va_list args)
{
    return fs_stream_writefv(fs_file_get_stream(pFile), fmt, args);
}

FS_API fs_result fs_file_seek(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    const fs_backend* pBackend;

    if (pFile == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_file_get_backend(pFile);
    FS_ASSERT(pBackend != NULL);

    return fs_backend_file_seek(pBackend, pFile, offset, origin);
}

FS_API fs_result fs_file_tell(fs_file* pFile, fs_int64* pOffset)
{
    const fs_backend* pBackend;

    if (pOffset == NULL) {
        return FS_INVALID_ARGS;  /* Doesn't make sense to be calling this without an output parameter. */
    }

    *pOffset = 0;

    if (pFile == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_file_get_backend(pFile);
    FS_ASSERT(pBackend != NULL);

    return fs_backend_file_tell(pBackend, pFile, pOffset);
}

FS_API fs_result fs_file_flush(fs_file* pFile)
{
    const fs_backend* pBackend;

    if (pFile == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_file_get_backend(pFile);
    FS_ASSERT(pBackend != NULL);

    return fs_backend_file_flush(pBackend, pFile);
}

FS_API fs_result fs_file_get_info(fs_file* pFile, fs_file_info* pInfo)
{
    const fs_backend* pBackend;

    if (pInfo == NULL) {
        return FS_INVALID_ARGS;  /* It doesn't make sense to call this without an info parameter. */
    }

    memset(pInfo, 0, sizeof(*pInfo));

    if (pFile == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_file_get_backend(pFile);
    FS_ASSERT(pBackend != NULL);

    return fs_backend_file_info(pBackend, pFile, pInfo);
}

FS_API fs_result fs_file_duplicate(fs_file* pFile, fs_file** ppDuplicate)
{
    fs_result result;

    if (ppDuplicate == NULL) {
        return FS_INVALID_ARGS;
    }

    *ppDuplicate = NULL;

    if (pFile == NULL) {
        return FS_INVALID_ARGS;
    }

    result = fs_file_alloc(pFile->pFS, ppDuplicate);
    if (result != FS_SUCCESS) {
        return result;
    }

    return fs_backend_file_duplicate(fs_get_backend_or_default(fs_file_get_fs(pFile)), pFile, *ppDuplicate);
}

FS_API void* fs_file_get_backend_data(fs_file* pFile)
{
    if (pFile == NULL) {
        return NULL;
    }

    return FS_OFFSET_PTR(pFile, sizeof(fs_file));
}

FS_API size_t fs_file_get_backend_data_size(fs_file* pFile)
{
    if (pFile == NULL) {
        return 0;
    }

    return pFile->backendDataSize;
}

FS_API fs_stream* fs_file_get_stream(fs_file* pFile)
{
    return (fs_stream*)pFile;
}

FS_API fs* fs_file_get_fs(fs_file* pFile)
{
    if (pFile == NULL) {
        return NULL;
    }

    return pFile->pFS;
}


typedef struct fs_iterator_item
{
    size_t nameLen;
    fs_file_info info;
} fs_iterator_item;

typedef struct fs_iterator_internal
{
    fs_iterator base;
    size_t itemIndex;   /* The index of the current item we're iterating. */
    size_t itemCount;
    size_t itemDataSize;
    size_t dataSize;
    size_t allocSize;
    fs_iterator_item** ppItems;
} fs_iterator_internal;

static size_t fs_iterator_item_sizeof(size_t nameLen)
{
    return FS_ALIGN(sizeof(fs_iterator_item) + nameLen + 1, FS_SIZEOF_PTR);   /* +1 for the null terminator. */
}

static char* fs_iterator_item_name(fs_iterator_item* pItem)
{
    return (char*)pItem + sizeof(*pItem);
}

static void fs_iterator_internal_resolve_public_members(fs_iterator_internal* pIterator)
{
    FS_ASSERT(pIterator != NULL);

    pIterator->base.pName   = fs_iterator_item_name(pIterator->ppItems[pIterator->itemIndex]);
    pIterator->base.nameLen = pIterator->ppItems[pIterator->itemIndex]->nameLen;
    pIterator->base.info    = pIterator->ppItems[pIterator->itemIndex]->info;
}

static fs_iterator_item* fs_iterator_internal_find(fs_iterator_internal* pIterator, const char* pName)
{
    /*
    We cannot use ppItems here because this function will be called before that has been set up. Instead we need
    to use a cursor and run through each item linearly. This is unsorted.
    */
    size_t iItem;
    size_t cursor = 0;

    for (iItem = 0; iItem < pIterator->itemCount; iItem += 1) {
        fs_iterator_item* pItem = (fs_iterator_item*)FS_OFFSET_PTR(pIterator, sizeof(fs_iterator_internal) + cursor);
        if (fs_strncmp(fs_iterator_item_name(pItem), pName, pItem->nameLen) == 0) {
            return pItem;
        }

        cursor += fs_iterator_item_sizeof(pItem->nameLen);
    }

    return NULL;
}

static fs_iterator_internal* fs_iterator_internal_append(fs_iterator_internal* pIterator, fs_iterator* pOther, fs* pFS, int mode)
{
    size_t newItemSize;
    fs_iterator_item* pNewItem;

    FS_ASSERT(pOther != NULL);

    /* Skip over any "." and ".." entries. */
    if ((pOther->pName[0] == '.' && pOther->pName[1] == 0) || (pOther->pName[0] == '.' && pOther->pName[1] == '.' && pOther->pName[2] == 0)) {
        return pIterator;
    }


    /* If we're in transparent mode, we don't want to add any archives. Instead we want to open them and iterate them recursively. */
    (void)mode;


    /* Check if the item already exists. If so, skip it. */
    if (pIterator != NULL) {
        pNewItem = fs_iterator_internal_find(pIterator, pOther->pName);
        if (pNewItem != NULL) {
            return pIterator;   /* Already exists. Skip it. */
        }
    }

    /* At this point we're ready to append the item. */
    newItemSize = fs_iterator_item_sizeof(pOther->nameLen);
    if (pIterator == NULL || pIterator->dataSize + newItemSize + sizeof(fs_iterator_item*) > pIterator->allocSize) {
        fs_iterator_internal* pNewIterator;
        size_t newAllocSize;

        if (pIterator == NULL) {
            newAllocSize = 4096;
            if (newAllocSize < (sizeof(*pIterator) + newItemSize + sizeof(fs_iterator_item*))) {
                newAllocSize = (sizeof(*pIterator) + newItemSize + sizeof(fs_iterator_item*));
            }
        } else {
            newAllocSize = pIterator->allocSize * 2;
            if (newAllocSize < (pIterator->dataSize + newItemSize + sizeof(fs_iterator_item*))) {
                newAllocSize = (pIterator->dataSize + newItemSize + sizeof(fs_iterator_item*));
            }
        }

        pNewIterator = (fs_iterator_internal*)fs_realloc(pIterator, newAllocSize, fs_get_allocation_callbacks(pFS));
        if (pNewIterator == NULL) {
            return pIterator;
        }

        if (pIterator == NULL) {
            FS_ZERO_MEMORY(pNewIterator, sizeof(fs_iterator_internal));
            pNewIterator->dataSize = sizeof(fs_iterator_internal);
        }

        pIterator = pNewIterator;
        pIterator->allocSize = newAllocSize;
    }

    /* We can now copy the information over to the information. */
    pNewItem = (fs_iterator_item*)FS_OFFSET_PTR(pIterator, sizeof(fs_iterator_internal) + pIterator->itemDataSize);
    FS_COPY_MEMORY(fs_iterator_item_name(pNewItem), pOther->pName, pOther->nameLen + 1);   /* +1 for the null terminator. */
    pNewItem->nameLen = pOther->nameLen;
    pNewItem->info    = pOther->info;

    pIterator->itemDataSize += newItemSize;
    pIterator->dataSize     += newItemSize + sizeof(fs_iterator_item*);
    pIterator->itemCount    += 1;

    return pIterator;
}


static int fs_iterator_item_compare(void* pUserData, const void* pA, const void* pB)
{
    fs_iterator_item* pItemA = *(fs_iterator_item**)pA;
    fs_iterator_item* pItemB = *(fs_iterator_item**)pB;
    const char* pNameA = fs_iterator_item_name(pItemA);
    const char* pNameB = fs_iterator_item_name(pItemB);
    int compareResult;

    (void)pUserData;

    compareResult = fs_strncmp(pNameA, pNameB, FS_MIN(pItemA->nameLen, pItemB->nameLen));
    if (compareResult == 0) {
        if (pItemA->nameLen < pItemB->nameLen) {
            compareResult = -1;
        } else if (pItemA->nameLen > pItemB->nameLen) {
            compareResult =  1;
        }
    }

    return compareResult;
}

static void fs_iterator_internal_sort(fs_iterator_internal* pIterator)
{
    fs_sort(pIterator->ppItems, pIterator->itemCount, sizeof(fs_iterator_item*), fs_iterator_item_compare, NULL);
}

static fs_iterator_internal* fs_iterator_internal_gather(fs_iterator_internal* pIterator, const fs_backend* pBackend, fs* pFS, const char* pDirectoryPath, size_t directoryPathLen, int mode)
{
    fs_result result;
    fs_iterator* pInnerIterator;

    FS_ASSERT(pBackend != NULL);

    /* Regular files take priority. */
    for (pInnerIterator = fs_backend_first(pBackend, pFS, pDirectoryPath, directoryPathLen); pInnerIterator != NULL; pInnerIterator = fs_backend_next(pBackend, pInnerIterator)) {
        pIterator = fs_iterator_internal_append(pIterator, pInnerIterator, pFS, mode);
    }

    /* Now we need to gather from archives, but only if we're not in opaque mode. */
    if (pFS != NULL && !FS_IS_OPAQUE(mode)) {
        fs_path_iterator iDirPathSeg;

        /* If no archive types have been configured we can abort early. */
        if (pFS->archiveTypesAllocSize == 0) {
            return pIterator;
        }

        /*
        Just like when opening a file we need to inspect each segment of the path. For each segment
        we need to check for archives. This is where transparent mode becomes very slow because it
        needs to scan every archive. For opaque mode we need only check for explicitly listed archives
        in the search path.
        */
        if (fs_path_first(pDirectoryPath, directoryPathLen, &iDirPathSeg) != FS_SUCCESS) {
            return pIterator;
        }

        do
        {
            fs_result backendIteratorResult;
            fs_registered_backend_iterator iBackend;
            fs_bool32 isArchive = FS_FALSE;
            size_t dirPathRemainingLen;

            /* Skip over "." and ".." segments. */
            if (fs_strncmp(iDirPathSeg.pFullPath, ".", iDirPathSeg.segmentLength) == 0) {
                continue;
            }
            if (fs_strncmp(iDirPathSeg.pFullPath, "..", iDirPathSeg.segmentLength) == 0) {
                continue;
            }

            if (fs_path_is_last(&iDirPathSeg)) {
                dirPathRemainingLen = 0;
            } else {
                if (directoryPathLen == FS_NULL_TERMINATED) {
                    dirPathRemainingLen = FS_NULL_TERMINATED;
                } else {
                    dirPathRemainingLen = directoryPathLen - (iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength + 1);
                }
            }

            /* If an archive has been explicitly listed in the path, we must try iterating from that. */
            for (backendIteratorResult = fs_first_registered_backend(pFS, &iBackend); backendIteratorResult == FS_SUCCESS; backendIteratorResult = fs_next_registered_backend(&iBackend)) {
                if (fs_path_extension_equal(iDirPathSeg.pFullPath + iDirPathSeg.segmentOffset, iDirPathSeg.segmentLength, iBackend.pExtension, iBackend.extensionLen)) {
                    fs* pArchive;
                    fs_iterator* pArchiveIterator;

                    isArchive = FS_TRUE;

                    result = fs_open_archive_ex(pFS, iBackend.pBackend, iBackend.pBackendConfig, iDirPathSeg.pFullPath, iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength, FS_READ | FS_IGNORE_MOUNTS | mode, &pArchive);
                    if (result != FS_SUCCESS) {
                        /*
                        We failed to open the archive. If it's due to the archive not existing we just continue searching. Otherwise
                        we just bomb out.
                        */
                        if (result != FS_DOES_NOT_EXIST) {
                            fs_close_archive(pArchive);
                            return pIterator;
                        } else {
                            continue;
                        }
                    }

                    if (dirPathRemainingLen == 0) {
                        pArchiveIterator = fs_first_ex(pArchive, "", 0, mode);
                    } else {
                        pArchiveIterator = fs_first_ex(pArchive, iDirPathSeg.pFullPath + iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength + 1, dirPathRemainingLen, mode);
                    }

                    while (pArchiveIterator != NULL) {
                        pIterator = fs_iterator_internal_append(pIterator, pArchiveIterator, pFS, mode);
                        pArchiveIterator = fs_next(pArchiveIterator);
                    }

                    fs_close_archive(pArchive);
                    break;
                }
            }

            /* If the path has an extension of an archive, but we still manage to get here, it means the archive doesn't exist. */
            if (isArchive) {
                return pIterator;
            }

            /*
            Getting here means this part of the path does not look like an archive. We will assume it's a folder and try
            iterating it using opaque mode to get the contents.
            */
            if (FS_IS_VERBOSE(mode)) {
                /*
                The caller has requested opening in verbose mode. In this case we don't want to be scanning for
                archives. Instead, any archives will be explicitly listed in the path. We just skip this path in
                this case.
                */
                continue;
            } else {
                /*
                Getting here means we're in transparent mode. We'll need to search for archives and check them one
                by one. This is the slow path.

                To do this we opaquely iterate over each file in the currently iterated file path. If any of these
                files are recognized as archives, we'll load up that archive and then try iterating from there.
                */
                for (pInnerIterator = fs_backend_first(pBackend, pFS, iDirPathSeg.pFullPath, iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength); pInnerIterator != NULL; pInnerIterator = fs_backend_next(pBackend, pInnerIterator)) {
                    for (backendIteratorResult = fs_first_registered_backend(pFS, &iBackend); backendIteratorResult == FS_SUCCESS; backendIteratorResult = fs_next_registered_backend(&iBackend)) {
                        if (fs_path_extension_equal(pInnerIterator->pName, pInnerIterator->nameLen, iBackend.pExtension, iBackend.extensionLen)) {
                            /* Looks like an archive. We can load this one up and try iterating from it. */
                            fs* pArchive;
                            fs_iterator* pArchiveIterator;
                            char pArchivePathNTStack[1024];
                            char* pArchivePathNTHeap = NULL;    /* <-- Must be initialized to null. */
                            char* pArchivePathNT;
                            size_t archivePathLen;

                            archivePathLen = iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength + 1 + pInnerIterator->nameLen;
                            if (archivePathLen >= sizeof(pArchivePathNTStack)) {
                                pArchivePathNTHeap = (char*)fs_malloc(archivePathLen + 1, fs_get_allocation_callbacks(pFS));
                                if (pArchivePathNTHeap == NULL) {
                                    fs_backend_free_iterator(pBackend, pInnerIterator);
                                    return pIterator;
                                }

                                pArchivePathNT = pArchivePathNTHeap;
                            } else {
                                pArchivePathNT = pArchivePathNTStack;
                            }

                            FS_COPY_MEMORY(pArchivePathNT, iDirPathSeg.pFullPath, iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength);
                            pArchivePathNT[iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength] = '/';
                            FS_COPY_MEMORY(pArchivePathNT + iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength + 1, pInnerIterator->pName, pInnerIterator->nameLen);
                            pArchivePathNT[archivePathLen] = '\0';

                            /* At this point we've constructed the archive name and we can now open it. */
                            result = fs_open_archive_ex(pFS, iBackend.pBackend, iBackend.pBackendConfig, pArchivePathNT, FS_NULL_TERMINATED, FS_READ | FS_IGNORE_MOUNTS | mode, &pArchive);
                            fs_free(pArchivePathNTHeap, fs_get_allocation_callbacks(pFS));

                            if (result != FS_SUCCESS) { /* <-- This is checking the result of fs_open_archive_ex(). */
                                continue;   /* Failed to open this archive. Keep looking. */
                            }


                            if (dirPathRemainingLen == 0) {
                                pArchiveIterator = fs_first_ex(pArchive, "", 0, mode);
                            } else {
                                pArchiveIterator = fs_first_ex(pArchive, iDirPathSeg.pFullPath + iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength + 1, dirPathRemainingLen, mode);
                            }

                            while (pArchiveIterator != NULL) {
                                pIterator = fs_iterator_internal_append(pIterator, pArchiveIterator, pFS, mode);
                                pArchiveIterator = fs_next(pArchiveIterator);
                            }

                            fs_close_archive(pArchive);
                            break;
                        }
                    }
                }
            }
        } while (fs_path_next(&iDirPathSeg) == FS_SUCCESS);
    }

    return pIterator;
}

FS_API fs_iterator* fs_first_ex(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen, int mode)
{
    fs_iterator_internal* pIterator = NULL;  /* This is the iterator we'll eventually be returning. */
    const fs_backend* pBackend;
    fs_iterator* pBackendIterator;
    fs_result result;
    size_t cursor;
    size_t iItem;
    
    if (pDirectoryPath == NULL) {
        pDirectoryPath = "";
    }

    result = fs_validate_path(pDirectoryPath, directoryPathLen, mode);
    if (result != FS_SUCCESS) {
        return NULL;    /* Invalid path. */
    }

    pBackend = fs_get_backend_or_default(pFS);
    if (pBackend == NULL) {
        return NULL;
    }

    if (directoryPathLen == FS_NULL_TERMINATED) {
        directoryPathLen = strlen(pDirectoryPath);
    }

    /*
    The first thing we need to do is gather files and directories from the backend. This needs to be done in the
    same order that we attempt to load files for reading:

        1) From all mounts.
        2) Directly from the file system.

    With each of the steps above, the relevant open mode flags must be respected as well because we want iteration
    to be consistent with what would happen when opening files.
    */
    /* Gather files. */
    {
        fs_result mountPointIerationResult;
        fs_mount_list_iterator iMountPoint;

        /* Check mount points. */
        if (pFS != NULL && (mode & FS_IGNORE_MOUNTS) == 0) {
            for (mountPointIerationResult = fs_mount_list_first(pFS->pReadMountPoints, &iMountPoint); mountPointIerationResult == FS_SUCCESS; mountPointIerationResult = fs_mount_list_next(&iMountPoint)) {
                /*
                Just like when opening a file, we need to check that the directory path starts with the mount point. If it
                doesn't match we just skip to the next mount point.
                */
                char  pDirSubPathCleanStack[1024];
                char* pDirSubPathCleanHeap = NULL;
                char* pDirSubPathClean;
                int dirSubPathCleanLen;
                unsigned int cleanOptions = (mode & FS_NO_ABOVE_ROOT_NAVIGATION);

                size_t dirSubPathLen;
                const char* pDirSubPath = fs_path_trim_base(pDirectoryPath, directoryPathLen, iMountPoint.pMountPointPath, FS_NULL_TERMINATED);
                if (pDirSubPath == NULL) {
                    continue;
                }

                dirSubPathLen = directoryPathLen - (size_t)(pDirSubPath - pDirectoryPath); 


                /* If the mount point starts with a root segment, i.e. "/", we cannot allow navigation above that. */
                if (iMountPoint.pMountPointPath[0] == '/' || iMountPoint.pMountPointPath[0] == '\\') {
                    cleanOptions |= FS_NO_ABOVE_ROOT_NAVIGATION;
                }

                /* We need to clean the file sub-path, but can skip it if FS_NO_SPECIAL_DIRS is specified since it's implied. */
                if ((mode & FS_NO_SPECIAL_DIRS) == 0) {
                    dirSubPathCleanLen = fs_path_normalize(pDirSubPathCleanStack, sizeof(pDirSubPathCleanStack), pDirSubPath, dirSubPathLen, cleanOptions);
                    if (dirSubPathCleanLen < 0) {
                        continue;    /* Most likely violating FS_NO_ABOVE_ROOT_NAVIGATION. */
                    }

                    if (dirSubPathCleanLen >= (int)sizeof(pDirSubPathCleanStack)) {
                        pDirSubPathCleanHeap = (char*)fs_malloc(dirSubPathCleanLen + 1, fs_get_allocation_callbacks(pFS));
                        if (pDirSubPathCleanHeap == NULL) {
                            return NULL;    /* Out of memory. */
                        }

                        fs_path_normalize(pDirSubPathCleanHeap, dirSubPathCleanLen + 1, pDirSubPath, dirSubPathLen, cleanOptions);    /* <-- This should never fail. */
                        pDirSubPathClean = pDirSubPathCleanHeap;
                    } else {
                        pDirSubPathClean = pDirSubPathCleanStack;
                    }
                } else {
                    pDirSubPathClean = (char*)pDirSubPath;  /* Safe cast. Will not be modified past this point. */
                    dirSubPathCleanLen = (int)dirSubPathLen;
                }

                
                if (iMountPoint.pArchive != NULL) {
                    /* The mount point is an archive. We need to iterate over the contents of the archive. */
                    pBackendIterator = fs_first_ex(iMountPoint.pArchive, pDirSubPathClean, dirSubPathCleanLen, mode);
                    while (pBackendIterator != NULL) {
                        pIterator = fs_iterator_internal_append(pIterator, pBackendIterator, pFS, mode);
                        pBackendIterator = fs_next(pBackendIterator);
                    }
                } else {
                    /*
                    The mount point is a directory. We need to construct a path that is the concatenation of the mount point's internal path and
                    our input directory. This is the path we'll be using to iterate over the contents of the directory.
                    */
                    char pInterpolatedPathStack[1024];
                    char* pInterpolatedPathHeap = NULL;
                    char* pInterpolatedPath;
                    int interpolatedPathLen;

                    interpolatedPathLen = fs_path_append(pInterpolatedPathStack, sizeof(pInterpolatedPathStack), iMountPoint.pPath, FS_NULL_TERMINATED, pDirSubPathClean, dirSubPathCleanLen);
                    if (interpolatedPathLen > 0 && (size_t)interpolatedPathLen >= sizeof(pInterpolatedPathStack)) {
                        /* Not enough room on the stack. Allocate on the heap. */
                        pInterpolatedPathHeap = (char*)fs_malloc(interpolatedPathLen + 1, fs_get_allocation_callbacks(pFS));
                        if (pInterpolatedPathHeap == NULL) {
                            fs_free_iterator((fs_iterator*)pIterator);
                            return NULL;    /* Out of memory. */
                        }

                        fs_path_append(pInterpolatedPathHeap, interpolatedPathLen + 1, iMountPoint.pPath, FS_NULL_TERMINATED, pDirSubPathClean, dirSubPathCleanLen);    /* <-- This should never fail. */
                        pInterpolatedPath = pInterpolatedPathHeap;
                    } else {
                        pInterpolatedPath = pInterpolatedPathStack;
                    }

                    pIterator = fs_iterator_internal_gather(pIterator, pBackend, pFS, pInterpolatedPath, FS_NULL_TERMINATED, mode);

                    if (pInterpolatedPathHeap != NULL) {
                        fs_free(pInterpolatedPathHeap, fs_get_allocation_callbacks(pFS));
                        pInterpolatedPathHeap = NULL;
                    }
                }
            }
        }

        /* Check for files directly in the file system. */
        if ((mode & FS_ONLY_MOUNTS) == 0) {
            pIterator = fs_iterator_internal_gather(pIterator, pBackend, pFS, pDirectoryPath, directoryPathLen, mode);
        }
    }

    /* If after the gathering step we don't have an iterator we can just return null. It just means nothing was found. */
    if (pIterator == NULL) {
        return NULL;
    }

    /* Set up pointers. The list of pointers is located at the end of the array. */
    pIterator->ppItems = (fs_iterator_item**)FS_OFFSET_PTR(pIterator, pIterator->dataSize - (pIterator->itemCount * sizeof(fs_iterator_item*)));

    cursor = 0;
    for (iItem = 0; iItem < pIterator->itemCount; iItem += 1) {
        pIterator->ppItems[iItem] = (fs_iterator_item*)FS_OFFSET_PTR(pIterator, sizeof(fs_iterator_internal) + cursor);
        cursor += fs_iterator_item_sizeof(pIterator->ppItems[iItem]->nameLen);
    }

    /* We want to sort items in the iterator to make it consistent across platforms. */
    fs_iterator_internal_sort(pIterator);

    /* Post-processing setup. */
    pIterator->base.pFS  = pFS;
    pIterator->itemIndex = 0;
    fs_iterator_internal_resolve_public_members(pIterator);

    return (fs_iterator*)pIterator;
}

FS_API fs_iterator* fs_first(fs* pFS, const char* pDirectoryPath, int mode)
{
    return fs_first_ex(pFS, pDirectoryPath, FS_NULL_TERMINATED, mode);
}

FS_API fs_iterator* fs_next(fs_iterator* pIterator)
{
    fs_iterator_internal* pIteratorInternal = (fs_iterator_internal*)pIterator;

    if (pIteratorInternal == NULL) {
        return NULL;
    }

    pIteratorInternal->itemIndex += 1;

    if (pIteratorInternal->itemIndex == pIteratorInternal->itemCount) {
        fs_free_iterator(pIterator);
        return NULL;
    }

    fs_iterator_internal_resolve_public_members(pIteratorInternal);

    return pIterator;
}

FS_API void fs_free_iterator(fs_iterator* pIterator)
{
    if (pIterator == NULL) {
        return;
    }

    fs_free(pIterator, fs_get_allocation_callbacks(pIterator->pFS));
}


FS_API fs_result fs_mount(fs* pFS, const char* pPathToMount, const char* pMountPoint, fs_mount_priority priority)
{
    fs_result result;
    fs_mount_list_iterator iterator;
    fs_result iteratorResult;
    fs_mount_list* pMountPoints;
    fs_mount_point* pNewMountPoint;
    fs_file_info fileInfo;
    int openMode;

    if (pFS == NULL || pPathToMount == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pMountPoint == NULL) {
        pMountPoint = "";
    }

    /*
    The first thing we're going to do is check for duplicates. We allow for the same path to be mounted
    to different mount points, and different paths to be mounted to the same mount point, but we don't
    want to have any duplicates where the same path is mounted to the same mount point.
    */
    for (iteratorResult = fs_mount_list_first(pFS->pReadMountPoints, &iterator); iteratorResult == FS_SUCCESS; iteratorResult = fs_mount_list_next(&iterator)) {
        if (strcmp(pPathToMount, iterator.pPath) == 0 && strcmp(pMountPoint, iterator.pMountPointPath) == 0) {
            return FS_SUCCESS;  /* Just pretend we're successful. */
        }
    }

    /*
    Getting here means we're not mounting a duplicate so we can now add it. We'll be either adding it to
    the end of the list, or to the beginning of the list depending on the priority.
    */
    pMountPoints = fs_mount_list_alloc(pFS->pReadMountPoints, pPathToMount, pMountPoint, priority, fs_get_allocation_callbacks(pFS), &pNewMountPoint);
    if (pMountPoints == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pFS->pReadMountPoints = pMountPoints;

    /*
    We need to determine if we're mounting a directory or an archive. If it's an archive, we need to
    open it.
    */
    openMode = FS_READ | FS_VERBOSE;

    /* Must use fs_backend_info() instead of fs_info() because otherwise fs_info() will attempt to read from mounts when we're in the process of trying to add one (this function). */
    result = fs_backend_info(fs_get_backend_or_default(pFS), pFS, (pPathToMount[0] != '\0') ? pPathToMount : ".", FS_IGNORE_MOUNTS, &fileInfo);
    if (result != FS_SUCCESS) {
        return result;
    }

    if (fileInfo.directory) {
        pNewMountPoint->pArchive = NULL;
        pNewMountPoint->closeArchiveOnUnmount = FS_FALSE;
    } else {
        result = fs_open_archive(pFS, pPathToMount, openMode, &pNewMountPoint->pArchive);
        if (result != FS_SUCCESS) {
            return result;
        }

        pNewMountPoint->closeArchiveOnUnmount = FS_TRUE;
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_unmount(fs* pFS, const char* pPathToMount_NotMountPoint)
{
    fs_result iteratorResult;
    fs_mount_list_iterator iterator;

    if (pFS == NULL || pPathToMount_NotMountPoint == NULL) {
        return FS_INVALID_ARGS;
    }

    for (iteratorResult = fs_mount_list_first(pFS->pReadMountPoints, &iterator); iteratorResult == FS_SUCCESS; /*iteratorResult = fs_mount_list_next(&iterator)*/) {
        if (strcmp(pPathToMount_NotMountPoint, iterator.pPath) == 0) {
            if (iterator.internal.pMountPoint->closeArchiveOnUnmount) {
                fs_close_archive(iterator.pArchive);
            }

            fs_mount_list_remove(pFS->pReadMountPoints, iterator.internal.pMountPoint);

            /*
            Since we just removed this item we don't want to advance the cursor. We do, however, need to re-resolve
            the members in preparation for the next iteration.
            */
            fs_mount_list_iterator_resolve_members(&iterator, iterator.internal.cursor);
        } else {
            iteratorResult = fs_mount_list_next(&iterator);
        }
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_mount_fs(fs* pFS, fs* pOtherFS, const char* pMountPoint, fs_mount_priority priority)
{
    fs_result iteratorResult;
    fs_mount_list_iterator iterator;
    fs_mount_list* pMountPoints;
    fs_mount_point* pNewMountPoint;

    if (pFS == NULL || pOtherFS == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pMountPoint == NULL) {
        pMountPoint = "";
    }

    /*
    We don't allow duplicates. An archive can be bound to multiple mount points, but we don't want to have the same
    archive mounted to the same mount point multiple times.
    */
    for (iteratorResult = fs_mount_list_first(pFS->pReadMountPoints, &iterator); iteratorResult == FS_SUCCESS; iteratorResult = fs_mount_list_next(&iterator)) {
        if (pOtherFS == iterator.pArchive && strcmp(pMountPoint, iterator.pMountPointPath) == 0) {
            return FS_SUCCESS;  /* Just pretend we're successful. */
        }
    }

    /*
    Getting here means we're not mounting a duplicate so we can now add it. We'll be either adding it to
    the end of the list, or to the beginning of the list depending on the priority.
    */
    pMountPoints = fs_mount_list_alloc(pFS->pReadMountPoints, "", pMountPoint, priority, fs_get_allocation_callbacks(pFS), &pNewMountPoint);
    if (pMountPoints == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pFS->pReadMountPoints = pMountPoints;

    pNewMountPoint->pArchive = pOtherFS;
    pNewMountPoint->closeArchiveOnUnmount = FS_FALSE;

    return FS_SUCCESS;
}

FS_API fs_result fs_unmount_fs(fs* pFS, fs* pOtherFS)
{
    fs_result iteratorResult;
    fs_mount_list_iterator iterator;

    if (pFS == NULL || pOtherFS == NULL) {
        return FS_INVALID_ARGS;
    }

    for (iteratorResult = fs_mount_list_first(pFS->pReadMountPoints, &iterator); iteratorResult == FS_SUCCESS; iteratorResult = fs_mount_list_next(&iterator)) {
        if (iterator.pArchive == pOtherFS) {
            fs_mount_list_remove(pFS->pReadMountPoints, iterator.internal.pMountPoint);
            return FS_SUCCESS;
        }
    }

    return FS_SUCCESS;
}


FS_API fs_result fs_mount_write(fs* pFS, const char* pPathToMount, const char* pMountPoint, fs_mount_priority priority)
{
    fs_mount_list_iterator iterator;
    fs_result iteratorResult;
    fs_mount_point* pNewMountPoint;
    fs_mount_list* pMountList;

    if (pFS == NULL || pPathToMount == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pMountPoint == NULL) {
        pMountPoint = "";
    }

    /* Like with regular read mount points we'll want to check for duplicates. */
    for (iteratorResult = fs_mount_list_first(pFS->pWriteMountPoints, &iterator); iteratorResult == FS_SUCCESS; iteratorResult = fs_mount_list_next(&iterator)) {
        if (strcmp(pPathToMount, iterator.pPath) == 0 && strcmp(pMountPoint, iterator.pMountPointPath) == 0) {
            return FS_SUCCESS;  /* Just pretend we're successful. */
        }
    }

    /* Getting here means we're not mounting a duplicate so we can now add it. */
    pMountList = fs_mount_list_alloc(pFS->pWriteMountPoints, pPathToMount, pMountPoint, priority, fs_get_allocation_callbacks(pFS), &pNewMountPoint);
    if (pMountList == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pFS->pWriteMountPoints = pMountList;
    
    /* We don't support mounting archives. Explicitly disable this. */
    pNewMountPoint->pArchive = NULL;
    pNewMountPoint->closeArchiveOnUnmount = FS_FALSE;

    return FS_SUCCESS;
}

FS_API fs_result fs_unmount_write(fs* pFS, const char* pPathToMount_NotMountPoint)
{
    fs_result iteratorResult;
    fs_mount_list_iterator iterator;

    if (pFS == NULL || pPathToMount_NotMountPoint == NULL) {
        return FS_INVALID_ARGS;
    }

    for (iteratorResult = fs_mount_list_first(pFS->pWriteMountPoints, &iterator); iteratorResult == FS_SUCCESS; /*iteratorResult = fs_mount_list_next(&iterator)*/) {
        if (strcmp(pPathToMount_NotMountPoint, iterator.pPath) == 0) {
            fs_mount_list_remove(pFS->pWriteMountPoints, iterator.internal.pMountPoint);

            /*
            Since we just removed this item we don't want to advance the cursor. We do, however, need to re-resolve
            the members in preparation for the next iteration.
            */
            fs_mount_list_iterator_resolve_members(&iterator, iterator.internal.cursor);
        } else {
            iteratorResult = fs_mount_list_next(&iterator);
        }
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_file_read_to_end(fs_file* pFile, fs_format format, void** ppData, size_t* pDataSize)
{
    return fs_stream_read_to_end(fs_file_get_stream(pFile), format, fs_get_allocation_callbacks(fs_file_get_fs(pFile)), ppData, pDataSize);
}

FS_API fs_result fs_file_open_and_read(fs* pFS, const char* pFilePath, fs_format format, void** ppData, size_t* pDataSize)
{
    fs_result result;
    fs_file* pFile;

    if (pFS == NULL || pFilePath == NULL || ppData == NULL || pDataSize == NULL) {
        return FS_INVALID_ARGS;
    }

    result = fs_file_open(pFS, pFilePath, FS_READ, &pFile);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_file_read_to_end(pFile, format, ppData, pDataSize);

    fs_file_close(pFile);

    return result;
}

FS_API fs_result fs_file_open_and_write(fs* pFS, const char* pFilePath, void* pData, size_t dataSize)
{
    fs_result result;
    fs_file* pFile;

    if (pFS == NULL || pFilePath == NULL || pData == NULL) {
        return FS_INVALID_ARGS;
    }

    result = fs_file_open(pFS, pFilePath, FS_TRUNCATE, &pFile);
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_file_write(pFile, pData, dataSize, NULL);

    fs_file_close(pFile);

    return result;
}



/******************************************************************************
*
*
* stdio Backend
*
*
******************************************************************************/
#ifndef FS_NO_STDIO
#include <stdio.h>
#include <wchar.h>     /* For wcstombs(). */
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>     /* For _mkdir() */
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif

#ifndef S_ISLNK
    #if defined(_WIN32)
        #define S_ISLNK(m) (0)
    #else
        #define S_ISLNK(m) (((m) & _S_IFMT) == _S_IFLNK)
    #endif
#endif

static int fs_fopen(FILE** ppFile, const char* pFilePath, const char* pOpenMode)
{
#if defined(_MSC_VER) && _MSC_VER >= 1400
    int err;
#endif

    if (ppFile != NULL) {
        *ppFile = NULL;  /* Safety. */
    }

    if (pFilePath == NULL || pOpenMode == NULL || ppFile == NULL) {
        return EINVAL;
    }

#if defined(_MSC_VER) && _MSC_VER >= 1400
    err = fopen_s(ppFile, pFilePath, pOpenMode);
    if (err != 0) {
        return err;
    }
#else
#if defined(_WIN32) || defined(__APPLE__)
    *ppFile = fopen(pFilePath, pOpenMode);
#else
    #if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64 && defined(_LARGEFILE64_SOURCE)
        *ppFile = fopen64(pFilePath, pOpenMode);
    #else
        *ppFile = fopen(pFilePath, pOpenMode);
    #endif
#endif
    if (*ppFile == NULL) {
        int result = errno;
        if (result == 0) {
            result = ENOENT;   /* Just a safety check to make sure we never ever return success when pFile == NULL. */
        }

        return result;
    }
#endif

    return FS_SUCCESS;
}

/*
_wfopen() isn't always available in all compilation environments.

    * Windows only.
    * MSVC seems to support it universally as far back as VC6 from what I can tell (haven't checked further back).
    * MinGW-64 (both 32- and 64-bit) seems to support it.
    * MinGW wraps it in !defined(__STRICT_ANSI__).
    * OpenWatcom wraps it in !defined(_NO_EXT_KEYS).

This can be reviewed as compatibility issues arise. The preference is to use _wfopen_s() and _wfopen() as opposed to the wcsrtombs()
fallback, so if you notice your compiler not detecting this properly I'm happy to look at adding support.
*/
#if defined(_WIN32)
    #if defined(_MSC_VER) || defined(__MINGW64__) || (!defined(__STRICT_ANSI__) && !defined(_NO_EXT_KEYS))
        #define FS_HAS_WFOPEN
    #endif
#endif

int fs_wfopen(FILE** ppFile, const wchar_t* pFilePath, const wchar_t* pOpenMode)
{
    if (ppFile != NULL) {
        *ppFile = NULL;  /* Safety. */
    }

    if (pFilePath == NULL || pOpenMode == NULL || ppFile == NULL) {
        return FS_INVALID_ARGS;
    }

    #if defined(FS_HAS_WFOPEN)
    {
        /* Use _wfopen() on Windows. */
        #if defined(_MSC_VER) && _MSC_VER >= 1400
        {
            errno_t err = _wfopen_s(ppFile, pFilePath, pOpenMode);
            if (err != 0) {
                return err;
            }
        }
        #else
        {
            *ppFile = _wfopen(pFilePath, pOpenMode);
            if (*ppFile == NULL) {
                return errno;
            }
        }
        #endif
    }
    #else
    {
        /*
        Use fopen() on anything other than Windows. Requires a conversion. This is annoying because fopen() is locale specific. The only real way I can
        think of to do this is with wcsrtombs(). Note that wcstombs() is apparently not thread-safe because it uses a static global mbstate_t object for
        maintaining state. I've checked this with -std=c89 and it works, but if somebody get's a compiler error I'll look into improving compatibility.
        */
        mbstate_t mbs;
        size_t lenMB;
        const wchar_t* pFilePathTemp = pFilePath;
        char* pFilePathMB = NULL;
        char pOpenModeMB[32] = {0};

        /* Get the length first. */
        FS_ZERO_OBJECT(&mbs);
        lenMB = wcsrtombs(NULL, &pFilePathTemp, 0, &mbs);
        if (lenMB == (size_t)-1) {
            return errno;
        }

        pFilePathMB = (char*)fs_malloc(lenMB + 1, NULL);
        if (pFilePathMB == NULL) {
            return ENOMEM;
        }

        pFilePathTemp = pFilePath;
        FS_ZERO_OBJECT(&mbs);
        wcsrtombs(pFilePathMB, &pFilePathTemp, lenMB + 1, &mbs);

        /* The open mode should always consist of ASCII characters so we should be able to do a trivial conversion. */
        {
            size_t i = 0;
            for (;;) {
                if (pOpenMode[i] == 0) {
                    pOpenModeMB[i] = '\0';
                    break;
                }

                pOpenModeMB[i] = (char)pOpenMode[i];
                i += 1;
            }
        }

        *ppFile = fopen(pFilePathMB, pOpenModeMB);

        fs_free(pFilePathMB, NULL);

        if (*ppFile == NULL) {
            return errno;
        }
    }
    #endif

    return 0;
}


static fs_file_info fs_file_info_from_stat(struct stat* pStat)
{
    fs_file_info info;

    FS_ZERO_OBJECT(&info);
    info.size             = pStat->st_size;
    info.lastAccessTime   = pStat->st_atime;
    info.lastModifiedTime = pStat->st_mtime;
    info.directory        = S_ISDIR(pStat->st_mode) != 0;
    info.symlink          = S_ISLNK(pStat->st_mode) != 0;

    return info;
}

#if defined(_WIN32)
static fs_uint64 fs_FILETIME_to_unix(const FILETIME* pFT)
{
    ULARGE_INTEGER li;

    li.HighPart = pFT->dwHighDateTime;
    li.LowPart  = pFT->dwLowDateTime;

    return (fs_uint64)(li.QuadPart / 10000000UL - 11644473600UL);   /* Convert from Windows epoch to Unix epoch. */
}

static fs_file_info fs_file_info_from_WIN32_FIND_DATAW(const WIN32_FIND_DATAW* pFD)
{
    fs_file_info info;

    FS_ZERO_OBJECT(&info);
    info.size             = ((fs_uint64)pFD->nFileSizeHigh << 32) | (fs_uint64)pFD->nFileSizeLow;
    info.lastModifiedTime = fs_FILETIME_to_unix(&pFD->ftLastWriteTime);
    info.lastAccessTime   = fs_FILETIME_to_unix(&pFD->ftLastAccessTime);
    info.directory        = (pFD->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)     != 0;
    info.symlink          = (pFD->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;

    return info;
}
#endif


typedef struct fs_stdio_registered_file
{
    size_t pathLen;
    FILE* pFile;
} fs_stdio_registered_file;

typedef struct fs_stdio
{
    int _unused;
} fs_stdio;

static size_t fs_alloc_size_stdio(const void* pBackendConfig)
{
    FS_UNUSED(pBackendConfig);

    return sizeof(fs_stdio);
}

static fs_result fs_init_stdio(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    FS_UNUSED(pFS);
    FS_UNUSED(pBackendConfig);
    FS_UNUSED(pStream);

    return FS_SUCCESS;
}

static void fs_uninit_stdio(fs* pFS)
{
    FS_UNUSED(pFS);
    return;
}

static fs_result fs_ioctl_stdio(fs* pFS, int op, void* pArgs)
{
    FS_UNUSED(pFS);
    FS_UNUSED(op);
    FS_UNUSED(pArgs);

    /* Not used by the stdio backend. */
    return FS_INVALID_OPERATION;
}

static fs_result fs_remove_stdio(fs* pFS, const char* pFilePath)
{
    int result = remove(pFilePath);
    if (result != 0) {
        return fs_result_from_errno(errno);
    }

    FS_UNUSED(pFS);

    return FS_SUCCESS;
}

static fs_result fs_rename_stdio(fs* pFS, const char* pOldName, const char* pNewName)
{
    int result = rename(pOldName, pNewName);
    if (result != 0) {
        return fs_result_from_errno(errno);
    }

    FS_UNUSED(pFS);

    return FS_SUCCESS;
}


#if defined(_WIN32)
static fs_result fs_mkdir_stdio_win32(const char* pPath)
{
    int result = _mkdir(pPath);
    if (result != 0) {
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}
#else
static fs_result fs_mkdir_stdio_posix(const char* pPath)
{
    int result = mkdir(pPath, S_IRWXU);
    if (result != 0) {
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}
#endif

static fs_result fs_mkdir_stdio(fs* pFS, const char* pPath)
{
    fs_result result;

    FS_UNUSED(pFS);

#if defined(_WIN32)
    result = fs_mkdir_stdio_win32(pPath);
#else
    result = fs_mkdir_stdio_posix(pPath);
#endif

    if (result == FS_DOES_NOT_EXIST) {
        result =  FS_SUCCESS;
    }

    return result;
}

static fs_result fs_info_stdio(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    /* We don't want to use stat() with Win32 because, from what I can tell, there's no way to determine if it's a symbolic link. S_IFLNK does not seem to be defined. */
    #if defined(_WIN32)
    {
        int pathLen;
        wchar_t  pPathWStack[1024];
        wchar_t* pPathWHeap = NULL;
        wchar_t* pPathW;
        HANDLE hFind;
        WIN32_FIND_DATAW fd;

        /* Use Win32 to convert from UTF-8 to wchar_t. */
        pathLen = MultiByteToWideChar(CP_UTF8, 0, pPath, -1, NULL, 0);
        if (pathLen == 0) {
            return fs_result_from_errno(GetLastError());
        }

        if (pathLen <= (int)FS_COUNTOF(pPathWStack)) {
            pPathW = pPathWStack;
        } else {
            pPathWHeap = (wchar_t*)fs_malloc(pathLen * sizeof(wchar_t), fs_get_allocation_callbacks(pFS));  /* pathLen includes the null terminator. */
            if (pPathWHeap == NULL) {
                return FS_OUT_OF_MEMORY;
            }

            pPathW = pPathWHeap;
        }

        MultiByteToWideChar(CP_UTF8, 0, pPath, -1, pPathW, pathLen);

        hFind = FindFirstFileW(pPathW, &fd);

        fs_free(pPathWHeap, fs_get_allocation_callbacks(pFS));
        pPathWHeap = NULL;

        *pInfo = fs_file_info_from_WIN32_FIND_DATAW(&fd);
    }
    #else
    {
        struct stat info;

        FS_UNUSED(pFS);

        if (stat(pPath, &info) != 0) {
            return fs_result_from_errno(errno);
        }

        *pInfo = fs_file_info_from_stat(&info);
    }
    #endif

    (void)openMode;

    return FS_SUCCESS;
}


typedef struct fs_file_stdio
{
    FILE* pFile;
    char openMode[4];   /* For duplication. */
    fs_bool32 isRegisteredOrHandle; /* When set to true, will not be closed with fs_file_close(). */
} fs_file_stdio;

static size_t fs_file_alloc_size_stdio(fs* pFS)
{
    FS_UNUSED(pFS);
    return sizeof(fs_file_stdio);
}

static fs_result fs_file_open_stdio(fs* pFS, fs_stream* pStream, const char* pPath, int openMode, fs_file* pFile)
{
    fs_file_stdio* pFileStdio;
    int result;

    FS_UNUSED(pFS);
    FS_UNUSED(pStream);

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    if (pFileStdio == NULL) {
        return FS_INVALID_ARGS;
    }
    
    if ((openMode & FS_WRITE) != 0) {
        if ((openMode & FS_READ) != 0) {
            /* Read and write. */
            if ((openMode & FS_APPEND) == FS_APPEND) {
                pFileStdio->openMode[0] = 'a'; pFileStdio->openMode[1] = '+'; pFileStdio->openMode[2] = 'b'; pFileStdio->openMode[3] = 0;   /* Read-and-write, appending. */
            } else if ((openMode & FS_OVERWRITE) == FS_OVERWRITE) {
                pFileStdio->openMode[0] = 'r'; pFileStdio->openMode[1] = '+'; pFileStdio->openMode[2] = 'b'; pFileStdio->openMode[3] = 0;   /* Read-and-write, overwriting. */
            } else {
                pFileStdio->openMode[0] = 'w'; pFileStdio->openMode[1] = '+'; pFileStdio->openMode[2] = 'b'; pFileStdio->openMode[3] = 0;   /* Read-and-write, truncating. */
            }
        } else {
            /* Write-only. */
            if ((openMode & FS_APPEND) == FS_APPEND) {
                pFileStdio->openMode[0] = 'a'; pFileStdio->openMode[1] = 'b'; pFileStdio->openMode[2] = 0; /* Write-only, appending. */
            } else if ((openMode & FS_OVERWRITE) == FS_OVERWRITE) {
                pFileStdio->openMode[0] = 'r'; pFileStdio->openMode[1] = '+'; pFileStdio->openMode[2] = 'b'; pFileStdio->openMode[3] = 0;   /* Write-only, overwriting. Need to use the "+" option here because there does not appear to be an option for a write-only overwrite mode. */
            } else {
                pFileStdio->openMode[0] = 'w'; pFileStdio->openMode[1] = 'b'; pFileStdio->openMode[2] = 0; /* Write-only, truncating. */
            }
        }
    } else {
        if ((openMode & FS_READ) != 0) {
            pFileStdio->openMode[0] = 'r'; pFileStdio->openMode[1] = 'b'; pFileStdio->openMode[2] = 0;    /* Read-only. */
        } else {
            return FS_INVALID_ARGS;
        }
    }

    #if defined(_WIN32) && defined(FS_HAS_WFOPEN)
    {
        size_t i;
        int pathLen;
        wchar_t  pOpenModeW[4];
        wchar_t  pFilePathWStack[1024];
        wchar_t* pFilePathWHeap = NULL;
        wchar_t* pFilePathW;

        /* Use Win32 to convert from UTF-8 to wchar_t. */
        pathLen = MultiByteToWideChar(CP_UTF8, 0, pPath, -1, NULL, 0);
        if (pathLen > 0) {
            if (pathLen <= (int)FS_COUNTOF(pFilePathWStack)) {
                pFilePathW = pFilePathWStack;
            } else {
                pFilePathWHeap = (wchar_t*)fs_malloc(pathLen * sizeof(wchar_t), fs_get_allocation_callbacks(pFS));
                if (pFilePathWHeap == NULL) {
                    return FS_OUT_OF_MEMORY;
                }

                pFilePathW = pFilePathWHeap;
            }

            MultiByteToWideChar(CP_UTF8, 0, pPath, -1, pFilePathW, pathLen);
            
            for (i = 0; i < FS_COUNTOF(pOpenModeW); i += 1) {
                pOpenModeW[i] = (wchar_t)pFileStdio->openMode[i];
            }

            result = fs_wfopen(&pFileStdio->pFile, pFilePathW, pOpenModeW);

            fs_free(pFilePathWHeap, fs_get_allocation_callbacks(pFS));
            pFilePathWHeap = NULL;

            if (result == 0) {
                return FS_SUCCESS;
            }
        }
    }
    #endif

    /* Getting here means we're either not opening with wfopen(), or wfopen() failed (or the conversion from char to wchar_t). */
    result = fs_fopen(&pFileStdio->pFile, pPath, pFileStdio->openMode);
    if (result != 0) {
        return fs_result_from_errno(result);
    }

    return FS_SUCCESS;
}

static fs_result fs_file_open_handle_stdio(fs* pFS, void* hBackendFile, fs_file* pFile)
{
    fs_file_stdio* pFileStdio;

    FS_UNUSED(pFS);

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    if (pFileStdio == NULL) {
        return FS_INVALID_ARGS;
    }
    
    pFileStdio->pFile = (FILE*)hBackendFile;
    pFileStdio->isRegisteredOrHandle = FS_TRUE;

    return FS_SUCCESS;
}

static void fs_file_close_stdio(fs_file* pFile)
{
    fs_file_stdio* pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    if (pFileStdio == NULL) {
        return;
    }

    if (!pFileStdio->isRegisteredOrHandle) {
        fclose(pFileStdio->pFile);
    }
}

static fs_result fs_file_read_stdio(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    size_t bytesRead;
    fs_file_stdio* pFileStdio;

    /* These were all validated at a higher level. */
    FS_ASSERT(pFile      != NULL);
    FS_ASSERT(pDst       != NULL);
    FS_ASSERT(pBytesRead != NULL);

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    FS_ASSERT(pFileStdio != NULL);

    bytesRead = fread(pDst, 1, bytesToRead, pFileStdio->pFile);

    *pBytesRead = bytesRead;
    
    /* If the value returned by fread is less than the bytes requested, it was either EOF or an error. We don't return EOF unless the number of bytes read is 0. */
    if (bytesRead != bytesToRead) {
        if (feof(pFileStdio->pFile)) {
            if (bytesRead == 0) {
                return FS_AT_END;
            }
        } else {
            return fs_result_from_errno(ferror(pFileStdio->pFile));
        }
    }

    return FS_SUCCESS;
}

static fs_result fs_file_write_stdio(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    size_t bytesWritten;
    fs_file_stdio* pFileStdio;

    /* These were all validated at a higher level. */
    FS_ASSERT(pFile         != NULL);
    FS_ASSERT(pSrc          != NULL);
    FS_ASSERT(pBytesWritten != NULL);

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    FS_ASSERT(pFileStdio != NULL);

    bytesWritten = fwrite(pSrc, 1, bytesToWrite, pFileStdio->pFile);

    *pBytesWritten = bytesWritten;

    if (bytesWritten != bytesToWrite) {
        return fs_result_from_errno(ferror(pFileStdio->pFile));
    }

    return FS_SUCCESS;
}

static fs_result fs_file_seek_stdio(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_stdio* pFileStdio;
    int result;
    int whence;

    /* These were all validated at a higher level. */
    FS_ASSERT(pFile != NULL);
    
    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    FS_ASSERT(pFileStdio != NULL);

    if (origin == FS_SEEK_SET) {
        whence = SEEK_SET;
    } else if (origin == FS_SEEK_END) {
        whence = SEEK_END;
    } else {
        whence = SEEK_CUR;
    }

#if defined(_WIN32)
    #if defined(_MSC_VER) && _MSC_VER > 1200
        result = _fseeki64(pFileStdio->pFile, offset, whence);
    #else
        /* No _fseeki64() so restrict to 31 bits. */
        if (origin > 0x7FFFFFFF) {
            return ERANGE;
        }

        result = fseek(pFileStdio->pFile, (int)offset, whence);
    #endif
#else
    result = fseek(pFileStdio->pFile, (long int)offset, whence);
#endif
    if (result != 0) {
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}

static fs_result fs_file_tell_stdio(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_stdio* pFileStdio;
    fs_int64 result;

    /* These were all validated at a higher level. */
    FS_ASSERT(pFile   != NULL);
    FS_ASSERT(pCursor != NULL);

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    FS_ASSERT(pFileStdio != NULL);

#if defined(_WIN32)
    #if defined(_MSC_VER) && _MSC_VER > 1200
        result = _ftelli64(pFileStdio->pFile);
    #else
        result = ftell(pFileStdio->pFile);
    #endif
#else
    result = ftell(pFileStdio->pFile);
#endif

    *pCursor = result;

    return FS_SUCCESS;
}

static fs_result fs_file_flush_stdio(fs_file* pFile)
{
    fs_file_stdio* pFileStdio;
    int result;

    /* These were all validated at a higher level. */
    FS_ASSERT(pFile != NULL);

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    FS_ASSERT(pFileStdio != NULL);

    result = fflush(pFileStdio->pFile);
    if (result != 0) {
        return fs_result_from_errno(ferror(pFileStdio->pFile));
    }

    return FS_SUCCESS;
}


/* Please submit a bug report if you get an error about fileno(). */
#if !defined(_MSC_VER) && !((defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 1) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE)) && !(defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
int fileno(FILE *stream);
#endif

static fs_result fs_file_info_stdio(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_stdio* pFileStdio;
    int fd;
    struct stat info;

    /* These were all validated at a higher level. */
    FS_ASSERT(pFile != NULL);
    FS_ASSERT(pInfo != NULL);

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    FS_ASSERT(pFileStdio != NULL);

#if defined(_MSC_VER)
    fd = _fileno(pFileStdio->pFile);
#else
    fd =  fileno(pFileStdio->pFile);
#endif

    if (fstat(fd, &info) != 0) {
        return fs_result_from_errno(ferror(pFileStdio->pFile));
    }

    *pInfo = fs_file_info_from_stat(&info);

    return FS_SUCCESS;
}

/* Iteration is platform-specific. */
#define FS_STDIO_MIN_ITERATOR_ALLOCATION_SIZE 1024

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>

FS_API fs_result fs_file_duplicate_stdio(fs_file* pFile, fs_file* pDuplicatedFile)
{
    fs_file_stdio* pFileStdio;
    fs_file_stdio* pDuplicatedFileStdio;
    int fd;
    int fdDuplicate;
    HANDLE hFile;
    HANDLE hFileDuplicate;

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    FS_ASSERT(pFileStdio != NULL);

    pDuplicatedFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pDuplicatedFile);
    FS_ASSERT(pDuplicatedFileStdio != NULL);

    fd = _fileno(pFileStdio->pFile);
    if (fd == -1) {
        return fs_result_from_errno(errno);
    }

    hFile = (HANDLE)_get_osfhandle(fd);
    if (hFile == INVALID_HANDLE_VALUE) {
        return fs_result_from_errno(errno);
    }

    if (!DuplicateHandle(GetCurrentProcess(), hFile, GetCurrentProcess(), &hFileDuplicate, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        return fs_result_from_errno(GetLastError());
    }

    fdDuplicate = _open_osfhandle((fs_intptr)hFileDuplicate, _O_RDONLY);
    if (fdDuplicate == -1) {
        CloseHandle(hFileDuplicate);
        return fs_result_from_errno(errno);
    }

    pDuplicatedFileStdio->pFile = _fdopen(fdDuplicate, pFileStdio->openMode);
    if (pDuplicatedFileStdio->pFile == NULL) {
        _close(fdDuplicate);
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}


typedef struct fs_iterator_stdio
{
    fs_iterator iterator;
    HANDLE hFind;
} fs_iterator_stdio;

FS_API void fs_free_iterator_stdio(fs_iterator* pIterator)
{
    fs_iterator_stdio* pIteratorStdio = (fs_iterator_stdio*)pIterator;

    FindClose(pIteratorStdio->hFind);
    fs_free(pIteratorStdio, fs_get_allocation_callbacks(pIterator->pFS));
}

static fs_iterator* fs_iterator_stdio_resolve(fs_iterator_stdio* pIteratorStdio, fs* pFS, HANDLE hFind, const WIN32_FIND_DATAW* pFD)
{
    fs_iterator_stdio* pNewIteratorStdio;
    size_t allocSize;
    int nameLen;

    /*
    The name is stored at the end of the struct. In order to know how much memory to allocate we'll
    need to calculate the length of the name.
    */
    nameLen = WideCharToMultiByte(CP_UTF8, 0, pFD->cFileName, -1, NULL, 0, NULL, NULL);
    if (nameLen == 0) {
        fs_free_iterator_stdio((fs_iterator*)pIteratorStdio);
        return NULL;
    }

    allocSize = FS_MAX(sizeof(fs_iterator_stdio) + nameLen, FS_STDIO_MIN_ITERATOR_ALLOCATION_SIZE);    /* "nameLen" includes the null terminator. 1KB just to try to avoid excessive internal reallocations inside realloc(). */

    pNewIteratorStdio = (fs_iterator_stdio*)fs_realloc(pIteratorStdio, allocSize, fs_get_allocation_callbacks(pFS));
    if (pNewIteratorStdio == NULL) {
        fs_free_iterator_stdio((fs_iterator*)pIteratorStdio);
        return NULL;
    }

    pNewIteratorStdio->iterator.pFS = pFS;
    pNewIteratorStdio->hFind        = hFind;

    /* Name. */
    pNewIteratorStdio->iterator.pName   = (char*)pNewIteratorStdio + sizeof(fs_iterator_stdio);
    pNewIteratorStdio->iterator.nameLen = (size_t)nameLen - 1;  /* nameLen includes the null terminator. */
    WideCharToMultiByte(CP_UTF8, 0, pFD->cFileName, -1, (char*)pNewIteratorStdio->iterator.pName, nameLen, NULL, NULL);  /* const-cast is safe here. */

    /* Info. */
    pNewIteratorStdio->iterator.info = fs_file_info_from_WIN32_FIND_DATAW(pFD);

    return (fs_iterator*)pNewIteratorStdio;
}

FS_API fs_iterator* fs_first_stdio(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    size_t i;
    int queryLen;
    int cbMultiByte;
    wchar_t  pQueryStack[1024];
    wchar_t* pQueryHeap = NULL;
    wchar_t* pQuery;
    HANDLE hFind;
    WIN32_FIND_DATAW fd;

    /* An empty path means the current directory. Win32 will want us to specify "." in this case. */
    if (pDirectoryPath == NULL || pDirectoryPath[0] == '\0') {
        pDirectoryPath = ".";
        directoryPathLen = 1;
    }

    if (directoryPathLen == FS_NULL_TERMINATED) {
        cbMultiByte = -1;
    } else {
        if (directoryPathLen > 0xFFFFFFFF) {
            return NULL;
        }

        cbMultiByte = (int)directoryPathLen;
    }

    /* When iterating over files using Win32 you specify a wildcard pattern. The "+ 3" you see in the code below is for the wildcard pattern. We also need to make everything a backslash. */
    queryLen = MultiByteToWideChar(CP_UTF8, 0, pDirectoryPath, cbMultiByte, NULL, 0);
    if (queryLen == 0) {
        return NULL;
    }

    if ((queryLen + 3) > (int)FS_COUNTOF(pQueryStack)) {
        pQueryHeap = (wchar_t*)fs_malloc((queryLen + 3) * sizeof(wchar_t), fs_get_allocation_callbacks(pFS));
        if (pQueryHeap == NULL) {
            return NULL;
        }

        pQuery = pQueryHeap;
    }
    else {
        pQuery = pQueryStack;
    }

    MultiByteToWideChar(CP_UTF8, 0, pDirectoryPath, cbMultiByte, pQuery, queryLen);

    if (directoryPathLen == FS_NULL_TERMINATED) {
        queryLen -= 1;  /* Remove the null terminator. Will not include the null terminator if the input string is not null terminated, hence why this is inside the conditional. */
    }

    /* Remove the trailing slash, if any. */
    if (pQuery[queryLen - 1] == L'\\' || pQuery[queryLen - 1] == L'/') {
        queryLen -= 1;
    }

    pQuery[queryLen + 0] = L'\\';
    pQuery[queryLen + 1] = L'*';
    pQuery[queryLen + 2] = L'\0';

    /* Convert to backslashes. */
    for (i = 0; i < (size_t)queryLen; i += 1) {
        if (pQuery[i] == L'/') {
            pQuery[i] = L'\\';
        }
    }

    hFind = FindFirstFileW(pQuery, &fd);
    fs_free(pQueryHeap, fs_get_allocation_callbacks(pFS));

    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    return fs_iterator_stdio_resolve(NULL, pFS, hFind, &fd);
}

FS_API fs_iterator* fs_next_stdio(fs_iterator* pIterator)
{
    fs_iterator_stdio* pIteratorStdio = (fs_iterator_stdio*)pIterator;
    WIN32_FIND_DATAW fd;

    if (!FindNextFileW(pIteratorStdio->hFind, &fd)) {
        fs_free_iterator_stdio(pIterator);
        return NULL;
    }

    return fs_iterator_stdio_resolve(pIteratorStdio, pIterator->pFS, pIteratorStdio->hFind, &fd);
}
#else
#include <unistd.h>
#include <dirent.h>

FS_API fs_result fs_file_duplicate_stdio(fs_file* pFile, fs_file* pDuplicatedFile)
{
    fs_file_stdio* pFileStdio;
    fs_file_stdio* pDuplicatedFileStdio;
    FILE* pDuplicatedFileHandle;

    /* These were all validated at a higher level. */
    FS_ASSERT(pFile           != NULL);
    FS_ASSERT(pDuplicatedFile != NULL);

    pFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pFile);
    FS_ASSERT(pFileStdio != NULL);

    pDuplicatedFileStdio = (fs_file_stdio*)fs_file_get_backend_data(pDuplicatedFile);
    FS_ASSERT(pDuplicatedFileStdio != NULL);

    pDuplicatedFileHandle = fdopen(dup(fileno(pFileStdio->pFile)), pFileStdio->openMode);
    if (pDuplicatedFileHandle == NULL) {
        return fs_result_from_errno(errno);
    }

    pDuplicatedFileStdio->pFile = pDuplicatedFileHandle;
    FS_COPY_MEMORY(pDuplicatedFileStdio->openMode, pFileStdio->openMode, sizeof(pFileStdio->openMode));

    return FS_SUCCESS;
}


typedef struct fs_iterator_stdio
{
    fs_iterator iterator;
    DIR* pDir;
    char* pFullFilePath;        /* Points to the end of the structure. */
    size_t directoryPathLen;    /* The length of the directory section. */
} fs_iterator_stdio;

FS_API void fs_free_iterator_stdio(fs_iterator* pIterator)
{
    fs_iterator_stdio* pIteratorStdio = (fs_iterator_stdio*)pIterator;

    FS_ASSERT(pIteratorStdio != NULL);

    closedir(pIteratorStdio->pDir);
    fs_free(pIteratorStdio, fs_get_allocation_callbacks(pIterator->pFS));
}

FS_API fs_iterator* fs_first_stdio(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    fs_iterator_stdio* pIteratorStdio;
    struct dirent* info;
    struct stat statInfo;
    size_t fileNameLen;

    FS_ASSERT(pDirectoryPath != NULL);

    /*
    Our input string isn't necessarily null terminated so we'll need to make a copy. This isn't
    the end of the world because we need to keep a copy of it anyway for when we need to stat
    the file for information like it's size.

    To do this we're going to allocate memory for our iterator which will include space for the
    directory path. Then we copy the directory path into the allocated memory and point the
    pFullFilePath member of the iterator to it. Then we call opendir(). Once that's done we
    can go to the first file and reallocate the iterator to make room for the file name portion,
    including the separating slash. Then we copy the file name portion over to the buffer.
    */

    if (directoryPathLen == 0 || pDirectoryPath[0] == '\0') {
        directoryPathLen = 1;
        pDirectoryPath = ".";
    }

    /* The first step is to calculate the length of the path if we need to. */
    if (directoryPathLen == (size_t)-1) {
        directoryPathLen = strlen(pDirectoryPath);
    }


    /*
    Now that we know the length of the directory we can allocate space for the iterator. The
    directory path will be placed at the end of the structure.
    */
    pIteratorStdio = (fs_iterator_stdio*)fs_malloc(FS_MAX(sizeof(*pIteratorStdio) + directoryPathLen + 1, FS_STDIO_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pFS));    /* +1 for null terminator. */
    if (pIteratorStdio == NULL) {
        return NULL;
    }

    /* Point pFullFilePath to the end of structure to where the path is located. */
    pIteratorStdio->pFullFilePath = (char*)pIteratorStdio + sizeof(*pIteratorStdio);
    pIteratorStdio->directoryPathLen = directoryPathLen;

    /* We can now copy over the directory path. This will null terminate the path which will allow us to call opendir(). */
    fs_strncpy_s(pIteratorStdio->pFullFilePath, directoryPathLen + 1, pDirectoryPath, directoryPathLen);

    /* We can now open the directory. */
    pIteratorStdio->pDir = opendir(pIteratorStdio->pFullFilePath);
    if (pIteratorStdio->pDir == NULL) {
        fs_free(pIteratorStdio, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    /* We now need to get information about the first file. */
    info = readdir(pIteratorStdio->pDir);
    if (info == NULL) {
        closedir(pIteratorStdio->pDir);
        fs_free(pIteratorStdio, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    fileNameLen = strlen(info->d_name);

    /*
    Now that we have the file name we need to append it to the full file path in the iterator. To do
    this we need to reallocate the iterator to account for the length of the file name, including the
    separating slash.
    */
    {
        fs_iterator_stdio* pNewIteratorStdio= (fs_iterator_stdio*)fs_realloc(pIteratorStdio, FS_MAX(sizeof(*pIteratorStdio) + directoryPathLen + 1 + fileNameLen + 1, FS_STDIO_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pFS));    /* +1 for null terminator. */
        if (pNewIteratorStdio == NULL) {
            closedir(pIteratorStdio->pDir);
            fs_free(pIteratorStdio, fs_get_allocation_callbacks(pFS));
            return NULL;
        }

        pIteratorStdio = pNewIteratorStdio;
    }

    /* Memory has been allocated. Copy over the separating slash and file name. */
    pIteratorStdio->pFullFilePath = (char*)pIteratorStdio + sizeof(*pIteratorStdio);
    pIteratorStdio->pFullFilePath[directoryPathLen] = '/';
    fs_strcpy(pIteratorStdio->pFullFilePath + directoryPathLen + 1, info->d_name);

    /* The pFileName member of the base iterator needs to be set to the file name. */
    pIteratorStdio->iterator.pName   = pIteratorStdio->pFullFilePath + directoryPathLen + 1;
    pIteratorStdio->iterator.nameLen = fileNameLen;

    /* We can now get the file information. */
    if (stat(pIteratorStdio->pFullFilePath, &statInfo) != 0) {
        closedir(pIteratorStdio->pDir);
        fs_free(pIteratorStdio, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    pIteratorStdio->iterator.info = fs_file_info_from_stat(&statInfo);

    return (fs_iterator*)pIteratorStdio;
}

FS_API fs_iterator* fs_next_stdio(fs_iterator* pIterator)
{
    fs_iterator_stdio* pIteratorStdio = (fs_iterator_stdio*)pIterator;
    struct dirent* info;
    struct stat statInfo;
    size_t fileNameLen;

    FS_ASSERT(pIteratorStdio != NULL);

    /* We need to get information about the next file. */
    info = readdir(pIteratorStdio->pDir);
    if (info == NULL) {
        fs_free_iterator_stdio((fs_iterator*)pIteratorStdio);
        return NULL;    /* The end of the directory. */
    }

    fileNameLen = strlen(info->d_name);

    /* We need to reallocate the iterator to account for the new file name. */
    {
        fs_iterator_stdio* pNewIteratorStdio = (fs_iterator_stdio*)fs_realloc(pIteratorStdio, FS_MAX(sizeof(*pIteratorStdio) + pIteratorStdio->directoryPathLen + 1 + fileNameLen + 1, FS_STDIO_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pIterator->pFS));    /* +1 for null terminator. */
        if (pNewIteratorStdio == NULL) {
            fs_free_iterator_stdio((fs_iterator*)pIteratorStdio);
            return NULL;
        }

        pIteratorStdio = pNewIteratorStdio;
    }

    /* Memory has been allocated. Copy over the file name. */
    pIteratorStdio->pFullFilePath = (char*)pIteratorStdio + sizeof(*pIteratorStdio);
    fs_strcpy(pIteratorStdio->pFullFilePath + pIteratorStdio->directoryPathLen + 1, info->d_name);

    /* The pFileName member of the base iterator needs to be set to the file name. */
    pIteratorStdio->iterator.pName   = pIteratorStdio->pFullFilePath + pIteratorStdio->directoryPathLen + 1;
    pIteratorStdio->iterator.nameLen = fileNameLen;

    /* We can now get the file information. */
    if (stat(pIteratorStdio->pFullFilePath, &statInfo) != 0) {
        fs_free_iterator_stdio((fs_iterator*)pIteratorStdio);
        return NULL;
    }

    pIteratorStdio->iterator.info = fs_file_info_from_stat(&statInfo);

    return (fs_iterator*)pIteratorStdio;
}
#endif

fs_backend fs_stdio_backend =
{
    fs_alloc_size_stdio,
    fs_init_stdio,
    fs_uninit_stdio,
    fs_ioctl_stdio,
    fs_remove_stdio,
    fs_rename_stdio,
    fs_mkdir_stdio,
    fs_info_stdio,
    fs_file_alloc_size_stdio,
    fs_file_open_stdio,
    fs_file_open_handle_stdio,
    fs_file_close_stdio,
    fs_file_read_stdio,
    fs_file_write_stdio,
    fs_file_seek_stdio,
    fs_file_tell_stdio,
    fs_file_flush_stdio,
    fs_file_info_stdio,
    fs_file_duplicate_stdio,
    fs_first_stdio,
    fs_next_stdio,
    fs_free_iterator_stdio
};
const fs_backend* FS_STDIO = &fs_stdio_backend;
#else
const fs_backend* FS_STDIO = NULL;
#endif
/* END fs.c */


/* BEG fs_helpers.c */
FS_API fs_result fs_result_from_errno(int error)
{
    switch (error)
    {
        case 0:       return FS_SUCCESS;
        case ENOENT:  return FS_DOES_NOT_EXIST;
        case EEXIST:  return FS_ALREADY_EXISTS;
        case ENOTDIR: return FS_NOT_DIRECTORY;
        case ENOMEM:  return FS_OUT_OF_MEMORY;
        case EINVAL:  return FS_INVALID_ARGS;
        default: break;
    }

    /* Fall back to a generic error. */
    return FS_ERROR;
}
/* END fs_helpers.c */



/* BEG fs_path.c */
FS_API fs_result fs_path_first(const char* pPath, size_t pathLen, fs_path_iterator* pIterator)
{
    if (pIterator == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_ZERO_OBJECT(pIterator);

    if (pPath == NULL || pPath[0] == '\0' || pathLen == 0) {
        return FS_INVALID_ARGS;
    }

    pIterator->pFullPath      = pPath;
    pIterator->fullPathLength = pathLen;
    pIterator->segmentOffset  = 0;
    pIterator->segmentLength  = 0;

    /* We need to find the first separator, or the end of the string. */
    while (pIterator->segmentLength < pathLen && pPath[pIterator->segmentLength] != '\0' && (pPath[pIterator->segmentLength] != '\\' && pPath[pIterator->segmentLength] != '/')) {
        pIterator->segmentLength += 1;
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_path_last(const char* pPath, size_t pathLen, fs_path_iterator* pIterator)
{
    if (pIterator == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_ZERO_OBJECT(pIterator);

    if (pathLen == 0 || pPath == NULL || pPath[0] == '\0') {
        return FS_INVALID_ARGS;
    }

    if (pathLen == (size_t)-1) {
        pathLen = strlen(pPath);
    }

    /* Little trick here. Not *quite* as optimal as it could be, but just go to the end of the string, and then go to the previous segment. */
    pIterator->pFullPath      = pPath;
    pIterator->fullPathLength = pathLen;
    pIterator->segmentOffset  = pathLen;
    pIterator->segmentLength  = 0;

    /* We need to find the last separator, or the beginning of the string. */
    while (pIterator->segmentLength < pathLen && pPath[pIterator->segmentOffset - 1] != '\0' && (pPath[pIterator->segmentOffset - 1] != '\\' && pPath[pIterator->segmentOffset - 1] != '/')) {
        pIterator->segmentOffset -= 1;
        pIterator->segmentLength += 1;
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_path_next(fs_path_iterator* pIterator)
{
    if (pIterator == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_ASSERT(pIterator->pFullPath != NULL);

    /* Move the offset to the end of the previous segment and reset the length. */
    pIterator->segmentOffset = pIterator->segmentOffset + pIterator->segmentLength;
    pIterator->segmentLength = 0;

    /* If we're at the end of the string, we're done. */
    if (pIterator->segmentOffset >= pIterator->fullPathLength || pIterator->pFullPath[pIterator->segmentOffset] == '\0') {
        return FS_AT_END;
    }

    /* At this point we should be sitting on a separator. The next character starts the next segment. */
    pIterator->segmentOffset += 1;

    /* Now we need to find the next separator or the end of the path. This will be the end of the segment. */
    for (;;) {
        if (pIterator->segmentOffset + pIterator->segmentLength >= pIterator->fullPathLength || pIterator->pFullPath[pIterator->segmentOffset + pIterator->segmentLength] == '\0') {
            break;  /* Reached the end of the path. */
        }

        if (pIterator->pFullPath[pIterator->segmentOffset + pIterator->segmentLength] == '\\' || pIterator->pFullPath[pIterator->segmentOffset + pIterator->segmentLength] == '/') {
            break;  /* Found a separator. This marks the end of the next segment. */
        }

        pIterator->segmentLength += 1;
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_path_prev(fs_path_iterator* pIterator)
{
    if (pIterator == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_ASSERT(pIterator->pFullPath != NULL);

    if (pIterator->segmentOffset == 0) {
        return FS_AT_END;  /* If we're already at the start it must mean we're finished iterating. */
    }

    pIterator->segmentLength = 0;

    /*
    The start of the segment of the current iterator should be sitting just before a separator. We
    need to move backwards one step. This will become the end of the segment we'll be returning.
    */
    pIterator->segmentOffset = pIterator->segmentOffset - 1;
    pIterator->segmentLength = 0;

    /* Just keep scanning backwards until we find a separator or the start of the path. */
    for (;;) {
        if (pIterator->segmentOffset == 0) {
            break;
        }

        if (pIterator->pFullPath[pIterator->segmentOffset - 1] == '\\' || pIterator->pFullPath[pIterator->segmentOffset - 1] == '/') {
            break;
        }

        pIterator->segmentOffset -= 1;
        pIterator->segmentLength += 1;
    }

    return FS_SUCCESS;
}

FS_API fs_bool32 fs_path_is_first(const fs_path_iterator* pIterator)
{
    if (pIterator == NULL) {
        return FS_FALSE;
    }

    return pIterator->segmentOffset == 0;

}

FS_API fs_bool32 fs_path_is_last(const fs_path_iterator* pIterator)
{
    if (pIterator == NULL) {
        return FS_FALSE;
    }

    if (pIterator->fullPathLength == FS_NULL_TERMINATED) {
        return pIterator->pFullPath[pIterator->segmentOffset + pIterator->segmentLength] == '\0';
    } else {
        return pIterator->segmentOffset + pIterator->segmentLength == pIterator->fullPathLength;
    }
}

FS_API int fs_path_iterators_compare(const fs_path_iterator* pIteratorA, const fs_path_iterator* pIteratorB)
{
    FS_ASSERT(pIteratorA != NULL);
    FS_ASSERT(pIteratorB != NULL);

    if (pIteratorA->pFullPath == pIteratorB->pFullPath && pIteratorA->segmentOffset == pIteratorB->segmentOffset && pIteratorA->segmentLength == pIteratorB->segmentLength) {
        return 0;
    }

    return fs_strncmp(pIteratorA->pFullPath + pIteratorA->segmentOffset, pIteratorB->pFullPath + pIteratorB->segmentOffset, FS_MIN(pIteratorA->segmentLength, pIteratorB->segmentLength));
}

FS_API const char* fs_path_file_name(const char* pPath, size_t pathLen)
{
    /* The file name is just the last segment. */
    fs_result result;
    fs_path_iterator last;

    result = fs_path_last(pPath, pathLen, &last);
    if (result != FS_SUCCESS) {
        return NULL;
    }

    if (last.segmentLength == 0) {
        return NULL;
    }
    
    return last.pFullPath + last.segmentOffset;
}

FS_API int fs_path_directory(char* pDst, size_t dstCap, const char* pPath, size_t pathLen)
{
    const char* pFileName;

    pFileName = fs_path_file_name(pPath, pathLen);
    if (pFileName == NULL) {
        return -1;
    }

    if (pFileName == pPath) {
        if (pDst != NULL && dstCap > 0) {
            pDst[0] = '\0';
        }

        return 0;   /* The path is just a file name. */
    } else {
        const char* pDirEnd = pFileName - 1;
        size_t dirLen = (size_t)(pDirEnd - pPath);

        if (pDst != NULL && dstCap > 0) {
            size_t bytesToCopy = FS_MIN(dstCap - 1, dirLen);
            if (bytesToCopy > 0) {
                FS_COPY_MEMORY(pDst, pPath, bytesToCopy);
            }

            pDst[bytesToCopy] = '\0';
        }

        if (dirLen > (size_t)-1) {
            return -1;  /* Too long. */
        }

        return (int)dirLen;
    }
}

FS_API const char* fs_path_extension(const char* pPath, size_t pathLen)
{
    const char* pDot = NULL;
    const char* pLastSlash = NULL;
    size_t i;

    if (pPath == NULL) {
        return NULL;
    }

    /* We need to find the last dot after the last slash. */
    for (i = 0; i < pathLen; ++i) {
        if (pPath[i] == '\0') {
            break;
        }

        if (pPath[i] == '.') {
            pDot = pPath + i;
        } else if (pPath[i] == '\\' || pPath[i] == '/') {
            pLastSlash = pPath + i;
        }
    }

    /* If the last dot is after the last slash, we've found it. Otherwise, it's not there and we need to return null. */
    if (pDot != NULL && pDot > pLastSlash) {
        return pDot + 1;
    } else {
        return NULL;
    }
}

FS_API fs_bool32 fs_path_extension_equal(const char* pPath, size_t pathLen, const char* pExtension, size_t extensionLen)
{
    if (pPath == NULL || pExtension == NULL) {
        return FS_FALSE;
    }

    if (extensionLen == FS_NULL_TERMINATED) {
        extensionLen = strlen(pExtension);
    }

    if (pathLen == FS_NULL_TERMINATED) {
        pathLen = strlen(pPath);
    }

    if (extensionLen >= pathLen) {
        return FS_FALSE;
    }

    if (pPath[pathLen - extensionLen - 1] != '.') {
        return FS_FALSE;
    }

    return fs_strnicmp(pPath + pathLen - extensionLen, pExtension, extensionLen) == 0;
}

FS_API const char* fs_path_trim_base(const char* pPath, size_t pathLen, const char* pBasePath, size_t basePathLen)
{
    fs_path_iterator iPath;
    fs_path_iterator iBase;
    fs_result result;

    if (basePathLen != FS_NULL_TERMINATED && pathLen < basePathLen) {
        return NULL;
    }

    if (basePathLen == 0 || pBasePath == NULL || pBasePath[0] == '\0') {
        return pPath;
    }

    result = fs_path_first(pPath, pathLen, &iPath);
    if (result != FS_SUCCESS) {
        return NULL;
    }

    result = fs_path_first(pBasePath, basePathLen, &iBase);
    if (result != FS_SUCCESS) {
        return NULL;
    }

    /* We just keep iterating until we find a mismatch or reach the end of the base path. */
    for (;;) {
        if (iPath.segmentLength != iBase.segmentLength) {
            return NULL;
        }

        if (fs_strncmp(iPath.pFullPath + iPath.segmentOffset, iBase.pFullPath + iBase.segmentOffset, iPath.segmentLength) != 0) {
            return NULL;
        }

        result = fs_path_next(&iBase);
        if (result != FS_SUCCESS) {
            fs_path_next(&iPath);   /* Move to the next segment in the path to ensure our iterators are in sync. */
            break;
        }

        result = fs_path_next(&iPath);
        if (result != FS_SUCCESS) {
            return NULL;    /* If we hit this it means the we've reached the end of the path before the base and therefore we don't match. */
        }
    }

    /* Getting here means we got to the end of the base path without finding a mismatched segment which means the path begins with the base. */
    return iPath.pFullPath + iPath.segmentOffset;
}

FS_API int fs_path_append(char* pDst, size_t dstCap, const char* pBasePath, size_t basePathLen, const char* pPathToAppend, size_t pathToAppendLen)
{
    size_t dstLen = 0;

    if (pBasePath == NULL) {
        pBasePath = "";
        basePathLen = 0;
    }

    if (pPathToAppend == NULL) {
        pPathToAppend = "";
        pathToAppendLen = 0;
    }

    if (basePathLen == FS_NULL_TERMINATED) {
        basePathLen = strlen(pBasePath);
    }

    if (pathToAppendLen == FS_NULL_TERMINATED) {
        pathToAppendLen = strlen(pPathToAppend);
    }


    /* Do not include the separator if we have one. */
    if (basePathLen > 0 && (pBasePath[basePathLen - 1] == '\\' || pBasePath[basePathLen - 1] == '/')) {
        basePathLen -= 1;
    }


    /*
    We don't want to be appending a separator if the base path is empty. Otherwise we'll end up with
    a leading slash.
    */
    if (basePathLen > 0) {
        /* Base path. */
        if (pDst != NULL) {
            size_t bytesToCopy = FS_MIN(basePathLen, dstCap);
            
            if (bytesToCopy > 0) {
                if (bytesToCopy == dstCap) {
                    bytesToCopy -= 1;   /* Need to leave room for the null terminator. */
                }

                /* Don't move the base path if we're appending in-place. */
                if (pDst != pBasePath) {
                    FS_COPY_MEMORY(pDst, pBasePath, FS_MIN(basePathLen, dstCap));
                }
            }

            pDst   += bytesToCopy;
            dstCap -= bytesToCopy;
        }
        dstLen += basePathLen;

        /* Separator. */
        if (pDst != NULL) {
            if (dstCap > 1) {   /* Need to leave room for the separator. */
                pDst[0] = '/';
                pDst += 1;
                dstCap -= 1;
            }
        }
        dstLen += 1;    
    }
    

    /* Path to append. */
    if (pDst != NULL) {
        size_t bytesToCopy = FS_MIN(pathToAppendLen, dstCap);
        
        if (bytesToCopy > 0) {
            if (bytesToCopy == dstCap) {
                bytesToCopy -= 1;   /* Need to leave room for the null terminator. */
            }

            FS_COPY_MEMORY(pDst, pPathToAppend, bytesToCopy);
            pDst[bytesToCopy] = '\0';
        }
    }
    dstLen += pathToAppendLen;


    if (dstLen > 0x7FFFFFFF) {
        return -1;  /* Path is too long to convert to an int. */
    }

    return (int)dstLen;
}

FS_API int fs_path_normalize(char* pDst, size_t dstCap, const char* pPath, size_t pathLen, unsigned int options)
{
    fs_path_iterator iPath;
    fs_result result;
    fs_bool32 allowLeadingBackNav = FS_TRUE;
    fs_path_iterator stack[256];    /* The size of this array controls the maximum number of components supported by this function. We're not doing any heap allocations here. Might add this later if necessary. */
    int top = 0;    /* Acts as a counter for the number of valid items in the stack. */
    int leadingBackNavCount = 0;
    int dstLen = 0;

    if (pPath == NULL) {
        pPath = "";
        pathLen = 0;
    }

    if (pDst != NULL && dstCap > 0) {
        pDst[0] = '\0';
    }

    /* Get rid of the empty case just to make our life easier below. */
    if (pathLen == 0 || pPath[0] == '\0') {
        return 0;
    }

    result = fs_path_first(pPath, pathLen, &iPath);
    if (result != FS_SUCCESS) {
        return -1;  /* Should never hit this because we did an empty string test above. */
    }

    /* We have a special case for when the result starts with "/". */
    if (iPath.segmentLength == 0) {
        allowLeadingBackNav = FS_FALSE; /* When the path starts with "/" we cannot allow a leading ".." in the output path. */

        if (pDst != NULL && dstCap > 0) {
            pDst[0] = '/';
            pDst   += 1;
            dstCap -= 1;
        }
        dstLen += 1;

        /* Get past the root. */
        result = fs_path_next(&iPath);
        if (result != FS_SUCCESS) {
            return dstLen;
        }
    }

    if ((options & FS_NO_ABOVE_ROOT_NAVIGATION) != 0) {
        allowLeadingBackNav = FS_FALSE;
    }

    for (;;) {
        /* Everything in this control block should goto a section below or abort early. */
        {
            if (iPath.segmentLength == 0 || (iPath.segmentLength == 1 && iPath.pFullPath[iPath.segmentOffset] == '.')) {
                /* It's either an empty segment or ".". These are ignored. */
                goto next_segment;
            } else if (iPath.segmentLength == 2 && iPath.pFullPath[iPath.segmentOffset] == '.' && iPath.pFullPath[iPath.segmentOffset + 1] == '.') {
                /* It's a ".." segment. We need to either pop an entry from the stack, or if there is no way to go further back, push the "..". */
                if (top > leadingBackNavCount) {
                    top -= 1;
                    goto next_segment;
                } else {
                    /* In this case the path is trying to navigate above the root. This is not always allowed. */
                    if (!allowLeadingBackNav) {
                        return -1;
                    }

                    leadingBackNavCount += 1;
                    goto push_segment;
                }
            } else {
                /* It's a regular segment. These always need to be pushed onto the stack. */
                goto push_segment;
            }
        }

    push_segment:
        if (top < (int)FS_COUNTOF(stack)) {
            stack[top] = iPath;
            top += 1;
        } else {
            return -1;  /* Ran out of room in "stack". */
        }

    next_segment:
        result = fs_path_next(&iPath);
        if (result != FS_SUCCESS) {
            break;
        }
    }

    /* At this point we should have a stack of items. Now we can construct the output path. */
    {
        int i = 0;
        for (i = 0; i < top; i += 1) {
            size_t segLen = stack[i].segmentLength;

            if (pDst != NULL && dstCap > segLen) {
                FS_COPY_MEMORY(pDst, stack[i].pFullPath + stack[i].segmentOffset, segLen);
                pDst   += segLen;
                dstCap -= segLen;
            }
            dstLen += (int)segLen;

            /* Separator. */
            if (i + 1 < top) {
                if (pDst != NULL && dstCap > 0) {
                    pDst[0] = '/';
                    pDst   += 1;
                    dstCap -= 1;
                }
                dstLen += 1;
            }
        }
    }

    /* Null terminate. */
    if (pDst != NULL && dstCap > 0) {
        pDst[0] = '\0';
    }

    return dstLen;
}
/* END fs_path.c */



/* BEG fs_memory_stream.c */
static fs_result fs_memory_stream_read_internal(fs_stream* pStream, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    return fs_memory_stream_read((fs_memory_stream*)pStream, pDst, bytesToRead, pBytesRead);
}

static fs_result fs_memory_stream_write_internal(fs_stream* pStream, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    return fs_memory_stream_write((fs_memory_stream*)pStream, pSrc, bytesToWrite, pBytesWritten);
}

static fs_result fs_memory_stream_seek_internal(fs_stream* pStream, fs_int64 offset, fs_seek_origin origin)
{
    return fs_memory_stream_seek((fs_memory_stream*)pStream, offset, origin);
}

static fs_result fs_memory_stream_tell_internal(fs_stream* pStream, fs_int64* pCursor)
{
    fs_result result;
    size_t cursor;

    result = fs_memory_stream_tell((fs_memory_stream*)pStream, &cursor);
    if (result != FS_SUCCESS) {
        return result;
    }

    if (cursor > FS_INT64_MAX) {    /* <-- INT64_MAX may not be defined on some compilers. Need to check this. Can easily define this ourselves. */
        return FS_ERROR;
    }

    *pCursor = (fs_int64)cursor;

    return FS_SUCCESS;
}

static size_t fs_memory_stream_duplicate_alloc_size_internal(fs_stream* pStream)
{
    (void)pStream;
    return sizeof(fs_memory_stream);
}

static fs_result fs_memory_stream_duplicate_internal(fs_stream* pStream, fs_stream* pDuplicatedStream)
{
    fs_memory_stream* pMemoryStream;

    pMemoryStream = (fs_memory_stream*)pStream;
    FS_ASSERT(pMemoryStream != NULL);

    *pDuplicatedStream = *pStream;

    /* Slightly special handling for write mode. Need to make a copy of the output buffer. */
    if (pMemoryStream->write.pData != NULL) {
        void* pNewData = fs_malloc(pMemoryStream->write.dataCap, &pMemoryStream->allocationCallbacks);
        if (pNewData == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        FS_COPY_MEMORY(pNewData, pMemoryStream->write.pData, pMemoryStream->write.dataSize);

        pMemoryStream->write.pData = pNewData;

        pMemoryStream->ppData    = &pMemoryStream->write.pData;
        pMemoryStream->pDataSize = &pMemoryStream->write.dataSize;
    } else {
        pMemoryStream->ppData    = (void**)&pMemoryStream->readonly.pData;
        pMemoryStream->pDataSize = &pMemoryStream->readonly.dataSize;
    }

    return FS_SUCCESS;
}

static void fs_memory_stream_uninit_internal(fs_stream* pStream)
{
    fs_memory_stream_uninit((fs_memory_stream*)pStream);
}

static fs_stream_vtable fs_gStreamVTableMemory =
{
    fs_memory_stream_read_internal,
    fs_memory_stream_write_internal,
    fs_memory_stream_seek_internal,
    fs_memory_stream_tell_internal,
    fs_memory_stream_duplicate_alloc_size_internal,
    fs_memory_stream_duplicate_internal,
    fs_memory_stream_uninit_internal
};


FS_API fs_result fs_memory_stream_init_write(const fs_allocation_callbacks* pAllocationCallbacks, fs_memory_stream* pStream)
{
    fs_result result;

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_ZERO_OBJECT(pStream);

    result = fs_stream_init(&fs_gStreamVTableMemory, &pStream->base);
    if (result != FS_SUCCESS) {
        return result;
    }

    pStream->write.pData    = NULL;
    pStream->write.dataSize = 0;
    pStream->write.dataCap  = 0;
    pStream->allocationCallbacks = fs_allocation_callbacks_init_copy(pAllocationCallbacks);

    pStream->ppData    = &pStream->write.pData;
    pStream->pDataSize = &pStream->write.dataSize;

    return FS_SUCCESS;
}

FS_API fs_result fs_memory_stream_init_readonly(const void* pData, size_t dataSize, fs_memory_stream* pStream)
{
    fs_result result;

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_ZERO_OBJECT(pStream);

    if (pData == NULL) {
        return FS_INVALID_ARGS;
    }

    result = fs_stream_init(&fs_gStreamVTableMemory, &pStream->base);
    if (result != FS_SUCCESS) {
        return result;
    }

    pStream->readonly.pData    = pData;
    pStream->readonly.dataSize = dataSize;

    pStream->ppData    = (void**)&pStream->readonly.pData;
    pStream->pDataSize = &pStream->readonly.dataSize;

    return FS_SUCCESS;
}

FS_API void fs_memory_stream_uninit(fs_memory_stream* pStream)
{
    if (pStream == NULL) {
        return;
    }

    if (pStream->write.pData != NULL) {
        fs_free(pStream->write.pData, &pStream->allocationCallbacks);
    }
}

FS_API fs_result fs_memory_stream_read(fs_memory_stream* pStream, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    size_t bytesAvailable;
    size_t bytesRead;

    if (pBytesRead != NULL) {
        *pBytesRead = 0;
    }

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_ASSERT(pStream->cursor <= *pStream->pDataSize); /* If this is triggered it means there a bug in the stream reader. The cursor has gone beyong the end of the buffer. */

    bytesAvailable = *pStream->pDataSize - pStream->cursor;
    if (bytesAvailable == 0) {
        return FS_AT_END;    /* Must return FS_AT_END if we're sitting at the end of the file, even when bytesToRead is 0. */
    }

    bytesRead = FS_MIN(bytesAvailable, bytesToRead);

    /* The destination can be null in which case this acts as a seek. */
    if (pDst != NULL) {
        FS_COPY_MEMORY(pDst, FS_OFFSET_PTR(*pStream->ppData, pStream->cursor), bytesRead);
    }

    pStream->cursor += bytesRead;
    
    if (pBytesRead != NULL) {
        *pBytesRead = bytesRead;
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_memory_stream_write(fs_memory_stream* pStream, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    size_t newSize;

    if (pBytesWritten != NULL) {
        *pBytesWritten = 0;
    }

    if (pStream == NULL || pSrc == NULL) {
        return FS_INVALID_ARGS;
    }

    /* Cannot write in read-only mode. */
    if (pStream->readonly.pData != NULL) {
        return FS_INVALID_OPERATION;
    }

    newSize = *pStream->pDataSize + bytesToWrite;
    if (newSize > pStream->write.dataCap) {
        /* Need to resize. */
        void* pNewBuffer;
        size_t newCap;

        newCap = FS_MAX(newSize, pStream->write.dataCap * 2);
        pNewBuffer = fs_realloc(*pStream->ppData, newCap, &pStream->allocationCallbacks);
        if (pNewBuffer == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        *pStream->ppData = pNewBuffer;
        pStream->write.dataCap = newCap;
    }

    FS_ASSERT(newSize <= pStream->write.dataCap);

    FS_COPY_MEMORY(FS_OFFSET_PTR(*pStream->ppData, *pStream->pDataSize), pSrc, bytesToWrite);
    *pStream->pDataSize = newSize;

    if (pBytesWritten != NULL) {
        *pBytesWritten = bytesToWrite;  /* We always write all or nothing here. */
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_memory_stream_seek(fs_memory_stream* pStream, fs_int64 offset, int origin)
{
    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    if ((fs_uint64)FS_ABS(offset) > FS_SIZE_MAX) {
        return FS_INVALID_ARGS;  /* Trying to seek too far. This will never happen on 64-bit builds. */
    }

    /*
    The seek binary - it works or it doesn't. There's no clamping to the end or anything like that. The
    seek point is either valid or invalid.
    */
    if (origin == FS_SEEK_CUR) {
        if (offset > 0) {
            /* Moving forward. */
            size_t bytesRemaining = *pStream->pDataSize - pStream->cursor;
            if (bytesRemaining < (size_t)offset) {
                return FS_BAD_SEEK;  /* Trying to seek beyond the end of the buffer. */
            }

            pStream->cursor += (size_t)offset;
        } else {
            /* Moving backwards. */
            size_t absoluteOffset = (size_t)FS_ABS(offset); /* Safe cast because it was checked above. */
            if (absoluteOffset > pStream->cursor) {
                return FS_BAD_SEEK;  /* Trying to seek prior to the start of the buffer. */
            }

            pStream->cursor -= absoluteOffset;
        }
    } else if (origin == FS_SEEK_SET) {
        if (offset < 0) {
            return FS_BAD_SEEK;  /* Trying to seek prior to the start of the buffer.. */
        }

        if ((size_t)offset > *pStream->pDataSize) {
            return FS_BAD_SEEK;
        }

        pStream->cursor = (size_t)offset;
    } else if (origin == FS_SEEK_END) {
        if (offset > 0) {
            return FS_BAD_SEEK;  /* Trying to seek beyond the end of the buffer. */
        }

        if ((size_t)FS_ABS(offset) > *pStream->pDataSize) {
            return FS_BAD_SEEK;
        }

        pStream->cursor = *pStream->pDataSize - (size_t)FS_ABS(offset);
    } else {
        return FS_INVALID_ARGS;
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_memory_stream_tell(fs_memory_stream* pStream, size_t* pCursor)
{
    if (pCursor == NULL) {
        return FS_INVALID_ARGS;
    }

    *pCursor = 0;

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    *pCursor = pStream->cursor;

    return FS_SUCCESS;
}

FS_API fs_result fs_memory_stream_remove(fs_memory_stream* pStream, size_t offset, size_t size)
{
    void* pDst;
    void* pSrc;
    size_t tailSize;

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    if ((offset + size) > *pStream->pDataSize) {
        return FS_INVALID_ARGS;
    }

    /* The cursor needs to be moved. */
    if (pStream->cursor > offset) {
        if (pStream->cursor >= (offset + size)) {
            pStream->cursor -= size;
        } else {
            pStream->cursor  = offset;
        }
    }

    pDst = FS_OFFSET_PTR(*pStream->ppData, offset);
    pSrc = FS_OFFSET_PTR(*pStream->ppData, offset + size);
    tailSize = *pStream->pDataSize - (offset + size);

    FS_MOVE_MEMORY(pDst, pSrc, tailSize);
    *pStream->pDataSize -= size;

    return FS_SUCCESS;
}

FS_API fs_result fs_memory_stream_truncate(fs_memory_stream* pStream)
{
    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    return fs_memory_stream_remove(pStream, pStream->cursor, (*pStream->pDataSize - pStream->cursor));
}

FS_API void* fs_memory_stream_take_ownership(fs_memory_stream* pStream, size_t* pSize)
{
    void* pData;

    if (pStream == NULL) {
        return NULL;
    }

    pData = *pStream->ppData;
    if (pSize != NULL) {
        *pSize = *pStream->pDataSize;
    }

    pStream->write.pData    = NULL;
    pStream->write.dataSize = 0;
    pStream->write.dataCap  = 0;

    return pData;

}
/* END fs_memory_stream.c */



/* BEG fs_utils.c */
static FS_INLINE void fs_swap(void* a, void* b, size_t sz)
{
    char* _a = (char*)a;
    char* _b = (char*)b;

    while (sz > 0) {
        char temp = *_a;
        *_a++ = *_b;
        *_b++ = temp;
        sz -= 1;
    }
}

FS_API void fs_sort(void* pBase, size_t count, size_t stride, int (*compareProc)(void*, const void*, const void*), void* pUserData)
{
    /* Simple insert sort for now. Will improve on this later. */
    size_t i;
    size_t j;

    for (i = 1; i < count; i += 1) {
        for (j = i; j > 0; j -= 1) {
            void* pA = (char*)pBase + (j - 1) * stride;
            void* pB = (char*)pBase + j * stride;

            if (compareProc(pUserData, pA, pB) <= 0) {
                break;
            }

            fs_swap(pA, pB, stride);
        }
    }
}

FS_API void* fs_binary_search(const void* pKey, const void* pList, size_t count, size_t stride, int (*compareProc)(void*, const void*, const void*), void* pUserData)
{
    size_t iStart;
    size_t iEnd;
    size_t iMid;

    if (count == 0) {
        return NULL;
    }

    iStart = 0;
    iEnd = count - 1;

    while (iStart <= iEnd) {
        int compareResult;

        iMid = iStart + (iEnd - iStart) / 2;

        compareResult = compareProc(pUserData, pKey, (char*)pList + (iMid * stride));
        if (compareResult < 0) {
            iEnd = iMid - 1;
        } else if (compareResult > 0) {
            iStart = iMid + 1;
        } else {
            return (void*)((char*)pList + (iMid * stride));
        }
    }

    return NULL;
}

FS_API void* fs_linear_search(const void* pKey, const void* pList, size_t count, size_t stride, int (*compareProc)(void*, const void*, const void*), void* pUserData)
{
    size_t i;

    for (i = 0; i < count; i+= 1) {
        int compareResult = compareProc(pUserData, pKey, (char*)pList + (i * stride));
        if (compareResult == 0) {
            return (void*)((char*)pList + (i * stride));
        }
    }

    return NULL;
}

FS_API void* fs_sorted_search(const void* pKey, const void* pList, size_t count, size_t stride, int (*compareProc)(void*, const void*, const void*), void* pUserData)
{
    const size_t threshold = 10;

    if (count < threshold) {
        return fs_linear_search(pKey, pList, count, stride, compareProc, pUserData);
    } else {
        return fs_binary_search(pKey, pList, count, stride, compareProc, pUserData);
    }
}
/* END fs_utils.c */




/* ==== Amalgamations Below ==== */

/* BEG fs_snprintf.c */
typedef char* fs_sprintf_callback(const char* buf, void* user, size_t len);

/*
Disabling unaligned access for safety. TODO: Look at a way to make this configurable. Will require reversing the
logic in stb_sprintf() which we might be able to do via the amalgamator.
*/
#ifndef FS_SPRINTF_NOUNALIGNED
#define FS_SPRINTF_NOUNALIGNED
#endif

/* We need to disable the implicit-fallthrough warning on GCC. */
#if defined(__GNUC__) && (__GNUC__ >= 7 || (__GNUC__ == 6 && __GNUC_MINOR__ >= 1))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

/* BEG stb_sprintf.c */
#if defined(__clang__)
 #if defined(__has_feature) && defined(__has_attribute)
  #if __has_feature(address_sanitizer)
   #if __has_attribute(__no_sanitize__)
    #define FS_ASAN __attribute__((__no_sanitize__("address")))
   #elif __has_attribute(__no_sanitize_address__)
    #define FS_ASAN __attribute__((__no_sanitize_address__))
   #elif __has_attribute(__no_address_safety_analysis__)
    #define FS_ASAN __attribute__((__no_address_safety_analysis__))
   #endif
  #endif
 #endif
#elif defined(__GNUC__) && (__GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
 #if defined(__SANITIZE_ADDRESS__) && __SANITIZE_ADDRESS__
  #define FS_ASAN __attribute__((__no_sanitize_address__))
 #endif
#elif defined(_MSC_VER)
 #if defined(__SANITIZE_ADDRESS__) && __SANITIZE_ADDRESS__
  #define FS_ASAN __declspec(no_sanitize_address)
 #endif
#endif

#ifndef FS_ASAN
#define FS_ASAN
#endif

#ifndef FS_API_SPRINTF_DEF
#define FS_API_SPRINTF_DEF FS_API FS_ASAN
#endif

#ifndef FS_SPRINTF_MIN
#define FS_SPRINTF_MIN 512 
#endif

#ifndef FS_SPRINTF_MSVC_MODE 
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define FS_SPRINTF_MSVC_MODE
#endif
#endif

#ifdef FS_SPRINTF_NOUNALIGNED 
#define FS_UNALIGNED(code)
#else
#define FS_UNALIGNED(code) code
#endif

#ifndef FS_SPRINTF_NOFLOAT

static fs_int32 fs_real_to_str(char const* *start, fs_uint32 *len, char* out, fs_int32 *decimal_pos, double value, fs_uint32 frac_digits);
static fs_int32 fs_real_to_parts(fs_int64 *bits, fs_int32 *expo, double value);
#define FS_SPECIAL 0x7000
#endif

static char fs_period = '.';
static char fs_comma = ',';
static struct
{
   short temp; 
   char pair[201];
} fs_digitpair =
{
  0,
   "00010203040506070809101112131415161718192021222324"
   "25262728293031323334353637383940414243444546474849"
   "50515253545556575859606162636465666768697071727374"
   "75767778798081828384858687888990919293949596979899"
};

FS_API_SPRINTF_DEF void fs_set_sprintf_separators(char pcomma, char pperiod)
{
   fs_period = pperiod;
   fs_comma = pcomma;
}

#define FS_LEFTJUST 1
#define FS_LEADINGPLUS 2
#define FS_LEADINGSPACE 4
#define FS_LEADING_0X 8
#define FS_LEADINGZERO 16
#define FS_INTMAX 32
#define FS_TRIPLET_COMMA 64
#define FS_NEGATIVE 128
#define FS_METRIC_SUFFIX 256
#define FS_HALFWIDTH 512
#define FS_METRIC_NOSPACE 1024
#define FS_METRIC_1024 2048
#define FS_METRIC_JEDEC 4096

static void fs_lead_sign(fs_uint32 fl, char* sign)
{
   sign[0] = 0;
   if (fl & FS_NEGATIVE) {
      sign[0] = 1;
      sign[1] = '-';
   } else if (fl & FS_LEADINGSPACE) {
      sign[0] = 1;
      sign[1] = ' ';
   } else if (fl & FS_LEADINGPLUS) {
      sign[0] = 1;
      sign[1] = '+';
   }
}

static FS_ASAN fs_uint32 fs_strlen_limited(char const* s, fs_uint32 limit)
{
   char const*  sn = s;

   
   for (;;) {
      if (((fs_uintptr)sn & 3) == 0)
         break;

      if (!limit || *sn == 0)
         return (fs_uint32)(sn - s);

      ++sn;
      --limit;
   }

   
   
   
   
   
   while (limit >= 4) {
      fs_uint32 v = *(fs_uint32 *)sn;
      
      if ((v - 0x01010101) & (~v) & 0x80808080UL)
         break;

      sn += 4;
      limit -= 4;
   }

   
   while (limit && *sn) {
      ++sn;
      --limit;
   }

   return (fs_uint32)(sn - s);
}

FS_API_SPRINTF_DEF int fs_vsprintfcb(fs_sprintf_callback* callback, void* user, char* buf, char const* fmt, va_list va)
{
   static char hex[] = "0123456789abcdefxp";
   static char hexu[] = "0123456789ABCDEFXP";
   char* bf;
   char const* f;
   int tlen = 0;

   bf = buf;
   f = fmt;
   for (;;) {
      fs_int32 fw, pr, tz;
      fs_uint32 fl;

      
      #define fs_chk_cb_bufL(bytes)                        \
         {                                                     \
            int len = (int)(bf - buf);                         \
            if ((len + (bytes)) >= FS_SPRINTF_MIN) {          \
               tlen += len;                                    \
               if (0 == (bf = buf = callback(buf, user, len))) \
                  goto done;                                   \
            }                                                  \
         }
      #define fs_chk_cb_buf(bytes)    \
         {                                \
            if (callback) {               \
               fs_chk_cb_bufL(bytes); \
            }                             \
         }
      #define fs_flush_cb()                      \
         {                                           \
            fs_chk_cb_bufL(FS_SPRINTF_MIN - 1); \
         } 
      #define fs_cb_buf_clamp(cl, v)                \
         cl = v;                                        \
         if (callback) {                                \
            int lg = FS_SPRINTF_MIN - (int)(bf - buf); \
            if (cl > lg)                                \
               cl = lg;                                 \
         }

      
      for (;;) {
         while (((fs_uintptr)f) & 3) {
         schk1:
            if (f[0] == '%')
               goto scandd;
         schk2:
            if (f[0] == 0)
               goto endfmt;
            fs_chk_cb_buf(1);
            *bf++ = f[0];
            ++f;
         }
         for (;;) {
            
            
            
            fs_uint32 v, c;
            v = *(fs_uint32 *)f;
            c = (~v) & 0x80808080;
            if (((v ^ 0x25252525) - 0x01010101) & c)
               goto schk1;
            if ((v - 0x01010101) & c)
               goto schk2;
            if (callback)
               if ((FS_SPRINTF_MIN - (int)(bf - buf)) < 4)
                  goto schk1;
            #ifdef FS_SPRINTF_NOUNALIGNED
                if(((fs_uintptr)bf) & 3) {
                    bf[0] = f[0];
                    bf[1] = f[1];
                    bf[2] = f[2];
                    bf[3] = f[3];
                } else
            #endif
            {
                *(fs_uint32 *)bf = v;
            }
            bf += 4;
            f += 4;
         }
      }
   scandd:

      ++f;

      
      fw = 0;
      pr = -1;
      fl = 0;
      tz = 0;

      
      for (;;) {
         switch (f[0]) {
         
         case '-':
            fl |= FS_LEFTJUST;
            ++f;
            continue;
         
         case '+':
            fl |= FS_LEADINGPLUS;
            ++f;
            continue;
         
         case ' ':
            fl |= FS_LEADINGSPACE;
            ++f;
            continue;
         
         case '#':
            fl |= FS_LEADING_0X;
            ++f;
            continue;
         
         case '\'':
            fl |= FS_TRIPLET_COMMA;
            ++f;
            continue;
         
         case '$':
            if (fl & FS_METRIC_SUFFIX) {
               if (fl & FS_METRIC_1024) {
                  fl |= FS_METRIC_JEDEC;
               } else {
                  fl |= FS_METRIC_1024;
               }
            } else {
               fl |= FS_METRIC_SUFFIX;
            }
            ++f;
            continue;
         
         case '_':
            fl |= FS_METRIC_NOSPACE;
            ++f;
            continue;
         
         case '0':
            fl |= FS_LEADINGZERO;
            ++f;
            goto flags_done;
         default: goto flags_done;
         }
      }
   flags_done:

      
      if (f[0] == '*') {
         fw = va_arg(va, fs_uint32);
         ++f;
      } else {
         while ((f[0] >= '0') && (f[0] <= '9')) {
            fw = fw * 10 + f[0] - '0';
            f++;
         }
      }
      
      if (f[0] == '.') {
         ++f;
         if (f[0] == '*') {
            pr = va_arg(va, fs_uint32);
            ++f;
         } else {
            pr = 0;
            while ((f[0] >= '0') && (f[0] <= '9')) {
               pr = pr * 10 + f[0] - '0';
               f++;
            }
         }
      }

      
      switch (f[0]) {
      
      case 'h':
         fl |= FS_HALFWIDTH;
         ++f;
         if (f[0] == 'h')
            ++f;  
         break;
      
      case 'l':
         fl |= ((sizeof(long) == 8) ? FS_INTMAX : 0);
         ++f;
         if (f[0] == 'l') {
            fl |= FS_INTMAX;
            ++f;
         }
         break;
      
      case 'j':
         fl |= (sizeof(size_t) == 8) ? FS_INTMAX : 0;
         ++f;
         break;
      
      case 'z':
         fl |= (sizeof(ptrdiff_t) == 8) ? FS_INTMAX : 0;
         ++f;
         break;
      case 't':
         fl |= (sizeof(ptrdiff_t) == 8) ? FS_INTMAX : 0;
         ++f;
         break;
      
      case 'I':
         if ((f[1] == '6') && (f[2] == '4')) {
            fl |= FS_INTMAX;
            f += 3;
         } else if ((f[1] == '3') && (f[2] == '2')) {
            f += 3;
         } else {
            fl |= ((sizeof(void* ) == 8) ? FS_INTMAX : 0);
            ++f;
         }
         break;
      default: break;
      }

      
      switch (f[0]) {
         #define FS_NUMSZ 512 
         char num[FS_NUMSZ];
         char lead[8];
         char tail[8];
         char* s;
         char const* h;
         fs_uint32 l, n, cs;
         fs_uint64 n64;
#ifndef FS_SPRINTF_NOFLOAT
         double fv;
#endif
         fs_int32 dp;
         char const* sn;

      case 's':
         
         s = va_arg(va, char* );
         if (s == 0)
            s = (char* )"null";
         
         
         l = fs_strlen_limited(s, (pr >= 0) ? (fs_uint32)pr : ~0u);
         lead[0] = 0;
         tail[0] = 0;
         pr = 0;
         dp = 0;
         cs = 0;
         
         goto scopy;

      case 'c': 
         
         s = num + FS_NUMSZ - 1;
         *s = (char)va_arg(va, int);
         l = 1;
         lead[0] = 0;
         tail[0] = 0;
         pr = 0;
         dp = 0;
         cs = 0;
         goto scopy;

      case 'n': 
      {
         int *d = va_arg(va, int *);
         *d = tlen + (int)(bf - buf);
      } break;

#ifdef FS_SPRINTF_NOFLOAT
      case 'A':              
      case 'a':              
      case 'G':              
      case 'g':              
      case 'E':              
      case 'e':              
      case 'f':              
         va_arg(va, double); 
         s = (char* )"No float";
         l = 8;
         lead[0] = 0;
         tail[0] = 0;
         pr = 0;
         cs = 0;
         FS_UNUSED(dp);
         goto scopy;
#else
      case 'A': 
      case 'a': 
         h = (f[0] == 'A') ? hexu : hex;
         fv = va_arg(va, double);
         if (pr == -1)
            pr = 6; 
         
         if (fs_real_to_parts((fs_int64 *)&n64, &dp, fv))
            fl |= FS_NEGATIVE;

         s = num + 64;

         fs_lead_sign(fl, lead);

         if (dp == -1023)
            dp = (n64) ? -1022 : 0;
         else
            n64 |= (((fs_uint64)1) << 52);
         n64 <<= (64 - 56);
         if (pr < 15)
            n64 += ((((fs_uint64)8) << 56) >> (pr * 4));


#ifdef FS_SPRINTF_MSVC_MODE
         *s++ = '0';
         *s++ = 'x';
#else
         lead[1 + lead[0]] = '0';
         lead[2 + lead[0]] = 'x';
         lead[0] += 2;
#endif
         *s++ = h[(n64 >> 60) & 15];
         n64 <<= 4;
         if (pr)
            *s++ = fs_period;
         sn = s;

         
         n = pr;
         if (n > 13)
            n = 13;
         if (pr > (fs_int32)n)
            tz = pr - n;
         pr = 0;
         while (n--) {
            *s++ = h[(n64 >> 60) & 15];
            n64 <<= 4;
         }

         
         tail[1] = h[17];
         if (dp < 0) {
            tail[2] = '-';
            dp = -dp;
         } else
            tail[2] = '+';
         n = (dp >= 1000) ? 6 : ((dp >= 100) ? 5 : ((dp >= 10) ? 4 : 3));
         tail[0] = (char)n;
         for (;;) {
            tail[n] = '0' + dp % 10;
            if (n <= 3)
               break;
            --n;
            dp /= 10;
         }

         dp = (int)(s - sn);
         l = (int)(s - (num + 64));
         s = num + 64;
         cs = 1 + (3 << 24);
         goto scopy;

      case 'G': 
      case 'g': 
         h = (f[0] == 'G') ? hexu : hex;
         fv = va_arg(va, double);
         if (pr == -1)
            pr = 6;
         else if (pr == 0)
            pr = 1; 
         
         if (fs_real_to_str(&sn, &l, num, &dp, fv, (pr - 1) | 0x80000000))
            fl |= FS_NEGATIVE;

         
         n = pr;
         if (l > (fs_uint32)pr)
            l = pr;
         while ((l > 1) && (pr) && (sn[l - 1] == '0')) {
            --pr;
            --l;
         }

         
         if ((dp <= -4) || (dp > (fs_int32)n)) {
            if (pr > (fs_int32)l)
               pr = l - 1;
            else if (pr)
               --pr; 
            goto doexpfromg;
         }
         
         if (dp > 0) {
            pr = (dp < (fs_int32)l) ? l - dp : 0;
         } else {
            pr = -dp + ((pr > (fs_int32)l) ? (fs_int32) l : pr);
         }
         goto dofloatfromg;

      case 'E': 
      case 'e': 
         h = (f[0] == 'E') ? hexu : hex;
         fv = va_arg(va, double);
         if (pr == -1)
            pr = 6; 
         
         if (fs_real_to_str(&sn, &l, num, &dp, fv, pr | 0x80000000))
            fl |= FS_NEGATIVE;
      doexpfromg:
         tail[0] = 0;
         fs_lead_sign(fl, lead);
         if (dp == FS_SPECIAL) {
            s = (char* )sn;
            cs = 0;
            pr = 0;
            goto scopy;
         }
         s = num + 64;
         
         *s++ = sn[0];

         if (pr)
            *s++ = fs_period;

         
         if ((l - 1) > (fs_uint32)pr)
            l = pr + 1;
         for (n = 1; n < l; n++)
            *s++ = sn[n];
         
         tz = pr - (l - 1);
         pr = 0;
         
         tail[1] = h[0xe];
         dp -= 1;
         if (dp < 0) {
            tail[2] = '-';
            dp = -dp;
         } else
            tail[2] = '+';
#ifdef FS_SPRINTF_MSVC_MODE
         n = 5;
#else
         n = (dp >= 100) ? 5 : 4;
#endif
         tail[0] = (char)n;
         for (;;) {
            tail[n] = '0' + dp % 10;
            if (n <= 3)
               break;
            --n;
            dp /= 10;
         }
         cs = 1 + (3 << 24); 
         goto flt_lead;

      case 'f': 
         fv = va_arg(va, double);
      doafloat:
         
         if (fl & FS_METRIC_SUFFIX) {
            double divisor;
            divisor = 1000.0f;
            if (fl & FS_METRIC_1024)
               divisor = 1024.0;
            while (fl < 0x4000000) {
               if ((fv < divisor) && (fv > -divisor))
                  break;
               fv /= divisor;
               fl += 0x1000000;
            }
         }
         if (pr == -1)
            pr = 6; 
         
         if (fs_real_to_str(&sn, &l, num, &dp, fv, pr))
            fl |= FS_NEGATIVE;
      dofloatfromg:
         tail[0] = 0;
         fs_lead_sign(fl, lead);
         if (dp == FS_SPECIAL) {
            s = (char* )sn;
            cs = 0;
            pr = 0;
            goto scopy;
         }
         s = num + 64;

         
         if (dp <= 0) {
            fs_int32 i;
            
            *s++ = '0';
            if (pr)
               *s++ = fs_period;
            n = -dp;
            if ((fs_int32)n > pr)
               n = pr;
            i = n;
            while (i) {
               if ((((fs_uintptr)s) & 3) == 0)
                  break;
               *s++ = '0';
               --i;
            }
            while (i >= 4) {
               *(fs_uint32 *)s = 0x30303030;
               s += 4;
               i -= 4;
            }
            while (i) {
               *s++ = '0';
               --i;
            }
            if ((fs_int32)(l + n) > pr)
               l = pr - n;
            i = l;
            while (i) {
               *s++ = *sn++;
               --i;
            }
            tz = pr - (n + l);
            cs = 1 + (3 << 24); 
         } else {
            cs = (fl & FS_TRIPLET_COMMA) ? ((600 - (fs_uint32)dp) % 3) : 0;
            if ((fs_uint32)dp >= l) {
               
               n = 0;
               for (;;) {
                  if ((fl & FS_TRIPLET_COMMA) && (++cs == 4)) {
                     cs = 0;
                     *s++ = fs_comma;
                  } else {
                     *s++ = sn[n];
                     ++n;
                     if (n >= l)
                        break;
                  }
               }
               if (n < (fs_uint32)dp) {
                  n = dp - n;
                  if ((fl & FS_TRIPLET_COMMA) == 0) {
                     while (n) {
                        if ((((fs_uintptr)s) & 3) == 0)
                           break;
                        *s++ = '0';
                        --n;
                     }
                     while (n >= 4) {
                        *(fs_uint32 *)s = 0x30303030;
                        s += 4;
                        n -= 4;
                     }
                  }
                  while (n) {
                     if ((fl & FS_TRIPLET_COMMA) && (++cs == 4)) {
                        cs = 0;
                        *s++ = fs_comma;
                     } else {
                        *s++ = '0';
                        --n;
                     }
                  }
               }
               cs = (int)(s - (num + 64)) + (3 << 24); 
               if (pr) {
                  *s++ = fs_period;
                  tz = pr;
               }
            } else {
               
               n = 0;
               for (;;) {
                  if ((fl & FS_TRIPLET_COMMA) && (++cs == 4)) {
                     cs = 0;
                     *s++ = fs_comma;
                  } else {
                     *s++ = sn[n];
                     ++n;
                     if (n >= (fs_uint32)dp)
                        break;
                  }
               }
               cs = (int)(s - (num + 64)) + (3 << 24); 
               if (pr)
                  *s++ = fs_period;
               if ((l - dp) > (fs_uint32)pr)
                  l = pr + dp;
               while (n < l) {
                  *s++ = sn[n];
                  ++n;
               }
               tz = pr - (l - dp);
            }
         }
         pr = 0;

         
         if (fl & FS_METRIC_SUFFIX) {
            char idx;
            idx = 1;
            if (fl & FS_METRIC_NOSPACE)
               idx = 0;
            tail[0] = idx;
            tail[1] = ' ';
            {
               if (fl >> 24) { 
                  if (fl & FS_METRIC_1024)
                     tail[idx + 1] = "_KMGT"[fl >> 24];
                  else
                     tail[idx + 1] = "_kMGT"[fl >> 24];
                  idx++;
                  
                  if (fl & FS_METRIC_1024 && !(fl & FS_METRIC_JEDEC)) {
                     tail[idx + 1] = 'i';
                     idx++;
                  }
                  tail[0] = idx;
               }
            }
         };

      flt_lead:
         
         l = (fs_uint32)(s - (num + 64));
         s = num + 64;
         goto scopy;
#endif

      case 'B': 
      case 'b': 
         h = (f[0] == 'B') ? hexu : hex;
         lead[0] = 0;
         if (fl & FS_LEADING_0X) {
            lead[0] = 2;
            lead[1] = '0';
            lead[2] = h[0xb];
         }
         l = (8 << 4) | (1 << 8);
         goto radixnum;

      case 'o': 
         h = hexu;
         lead[0] = 0;
         if (fl & FS_LEADING_0X) {
            lead[0] = 1;
            lead[1] = '0';
         }
         l = (3 << 4) | (3 << 8);
         goto radixnum;

      case 'p': 
         fl |= (sizeof(void* ) == 8) ? FS_INTMAX : 0;
         pr = sizeof(void* ) * 2;
         fl &= ~FS_LEADINGZERO; 
                                    

      case 'X': 
      case 'x': 
         h = (f[0] == 'X') ? hexu : hex;
         l = (4 << 4) | (4 << 8);
         lead[0] = 0;
         if (fl & FS_LEADING_0X) {
            lead[0] = 2;
            lead[1] = '0';
            lead[2] = h[16];
         }
      radixnum:
         
         if (fl & FS_INTMAX)
            n64 = va_arg(va, fs_uint64);
         else
            n64 = va_arg(va, fs_uint32);

         s = num + FS_NUMSZ;
         dp = 0;
         
         tail[0] = 0;
         if (n64 == 0) {
            lead[0] = 0;
            if (pr == 0) {
               l = 0;
               cs = 0;
               goto scopy;
            }
         }
         
         for (;;) {
            *--s = h[n64 & ((1 << (l >> 8)) - 1)];
            n64 >>= (l >> 8);
            if (!((n64) || ((fs_int32)((num + FS_NUMSZ) - s) < pr)))
               break;
            if (fl & FS_TRIPLET_COMMA) {
               ++l;
               if ((l & 15) == ((l >> 4) & 15)) {
                  l &= ~15;
                  *--s = fs_comma;
               }
            }
         };
         
         cs = (fs_uint32)((num + FS_NUMSZ) - s) + ((((l >> 4) & 15)) << 24);
         
         l = (fs_uint32)((num + FS_NUMSZ) - s);
         
         goto scopy;

      case 'u': 
      case 'i':
      case 'd': 
         
         if (fl & FS_INTMAX) {
            fs_int64 i64 = va_arg(va, fs_int64);
            n64 = (fs_uint64)i64;
            if ((f[0] != 'u') && (i64 < 0)) {
               n64 = (fs_uint64)-i64;
               fl |= FS_NEGATIVE;
            }
         } else {
            fs_int32 i = va_arg(va, fs_int32);
            n64 = (fs_uint32)i;
            if ((f[0] != 'u') && (i < 0)) {
               n64 = (fs_uint32)-i;
               fl |= FS_NEGATIVE;
            }
         }

#ifndef FS_SPRINTF_NOFLOAT
         if (fl & FS_METRIC_SUFFIX) {
            if (n64 < 1024)
               pr = 0;
            else if (pr == -1)
               pr = 1;
            fv = (double)(fs_int64)n64;
            goto doafloat;
         }
#endif

         
         s = num + FS_NUMSZ;
         l = 0;

         for (;;) {
            
            char* o = s - 8;
            if (n64 >= 100000000) {
               n = (fs_uint32)(n64 % 100000000);
               n64 /= 100000000;
            } else {
               n = (fs_uint32)n64;
               n64 = 0;
            }
            if ((fl & FS_TRIPLET_COMMA) == 0) {
               do {
                  s -= 2;
                  *(fs_uint16 *)s = *(fs_uint16 *)&fs_digitpair.pair[(n % 100) * 2];
                  n /= 100;
               } while (n);
            }
            while (n) {
               if ((fl & FS_TRIPLET_COMMA) && (l++ == 3)) {
                  l = 0;
                  *--s = fs_comma;
                  --o;
               } else {
                  *--s = (char)(n % 10) + '0';
                  n /= 10;
               }
            }
            if (n64 == 0) {
               if ((s[0] == '0') && (s != (num + FS_NUMSZ)))
                  ++s;
               break;
            }
            while (s != o)
               if ((fl & FS_TRIPLET_COMMA) && (l++ == 3)) {
                  l = 0;
                  *--s = fs_comma;
                  --o;
               } else {
                  *--s = '0';
               }
         }

         tail[0] = 0;
         fs_lead_sign(fl, lead);

         
         l = (fs_uint32)((num + FS_NUMSZ) - s);
         if (l == 0) {
            *--s = '0';
            l = 1;
         }
         cs = l + (3 << 24);
         if (pr < 0)
            pr = 0;

      scopy:
         
         if (pr < (fs_int32)l)
            pr = l;
         n = pr + lead[0] + tail[0] + tz;
         if (fw < (fs_int32)n)
            fw = n;
         fw -= n;
         pr -= l;

         
         if ((fl & FS_LEFTJUST) == 0) {
            if (fl & FS_LEADINGZERO) 
            {
               pr = (fw > pr) ? fw : pr;
               fw = 0;
            } else {
               fl &= ~FS_TRIPLET_COMMA; 
            }
         }

         
         if (fw + pr) {
            fs_int32 i;
            fs_uint32 c;

            
            if ((fl & FS_LEFTJUST) == 0)
               while (fw > 0) {
                  fs_cb_buf_clamp(i, fw);
                  fw -= i;
                  while (i) {
                     if ((((fs_uintptr)bf) & 3) == 0)
                        break;
                     *bf++ = ' ';
                     --i;
                  }
                  while (i >= 4) {
                     *(fs_uint32 *)bf = 0x20202020;
                     bf += 4;
                     i -= 4;
                  }
                  while (i) {
                     *bf++ = ' ';
                     --i;
                  }
                  fs_chk_cb_buf(1);
               }

            
            sn = lead + 1;
            while (lead[0]) {
               fs_cb_buf_clamp(i, lead[0]);
               lead[0] -= (char)i;
               while (i) {
                  *bf++ = *sn++;
                  --i;
               }
               fs_chk_cb_buf(1);
            }

            
            c = cs >> 24;
            cs &= 0xffffff;
            cs = (fl & FS_TRIPLET_COMMA) ? ((fs_uint32)(c - ((pr + cs) % (c + 1)))) : 0;
            while (pr > 0) {
               fs_cb_buf_clamp(i, pr);
               pr -= i;
               if ((fl & FS_TRIPLET_COMMA) == 0) {
                  while (i) {
                     if ((((fs_uintptr)bf) & 3) == 0)
                        break;
                     *bf++ = '0';
                     --i;
                  }
                  while (i >= 4) {
                     *(fs_uint32 *)bf = 0x30303030;
                     bf += 4;
                     i -= 4;
                  }
               }
               while (i) {
                  if ((fl & FS_TRIPLET_COMMA) && (cs++ == c)) {
                     cs = 0;
                     *bf++ = fs_comma;
                  } else
                     *bf++ = '0';
                  --i;
               }
               fs_chk_cb_buf(1);
            }
         }

         
         sn = lead + 1;
         while (lead[0]) {
            fs_int32 i;
            fs_cb_buf_clamp(i, lead[0]);
            lead[0] -= (char)i;
            while (i) {
               *bf++ = *sn++;
               --i;
            }
            fs_chk_cb_buf(1);
         }

         
         n = l;
         while (n) {
            fs_int32 i;
            fs_cb_buf_clamp(i, n);
            n -= i;
            FS_UNALIGNED(while (i >= 4) {
               *(fs_uint32 volatile *)bf = *(fs_uint32 volatile *)s;
               bf += 4;
               s += 4;
               i -= 4;
            })
            while (i) {
               *bf++ = *s++;
               --i;
            }
            fs_chk_cb_buf(1);
         }

         
         while (tz) {
            fs_int32 i;
            fs_cb_buf_clamp(i, tz);
            tz -= i;
            while (i) {
               if ((((fs_uintptr)bf) & 3) == 0)
                  break;
               *bf++ = '0';
               --i;
            }
            while (i >= 4) {
               *(fs_uint32 *)bf = 0x30303030;
               bf += 4;
               i -= 4;
            }
            while (i) {
               *bf++ = '0';
               --i;
            }
            fs_chk_cb_buf(1);
         }

         
         sn = tail + 1;
         while (tail[0]) {
            fs_int32 i;
            fs_cb_buf_clamp(i, tail[0]);
            tail[0] -= (char)i;
            while (i) {
               *bf++ = *sn++;
               --i;
            }
            fs_chk_cb_buf(1);
         }

         
         if (fl & FS_LEFTJUST)
            if (fw > 0) {
               while (fw) {
                  fs_int32 i;
                  fs_cb_buf_clamp(i, fw);
                  fw -= i;
                  while (i) {
                     if ((((fs_uintptr)bf) & 3) == 0)
                        break;
                     *bf++ = ' ';
                     --i;
                  }
                  while (i >= 4) {
                     *(fs_uint32 *)bf = 0x20202020;
                     bf += 4;
                     i -= 4;
                  }
                  while (i--)
                     *bf++ = ' ';
                  fs_chk_cb_buf(1);
               }
            }
         break;

      default: 
         s = num + FS_NUMSZ - 1;
         *s = f[0];
         l = 1;
         fw = fl = 0;
         lead[0] = 0;
         tail[0] = 0;
         pr = 0;
         dp = 0;
         cs = 0;
         goto scopy;
      }
      ++f;
   }
endfmt:

   if (!callback)
      *bf = 0;
   else
      fs_flush_cb();

done:
   return tlen + (int)(bf - buf);
}


#undef FS_LEFTJUST
#undef FS_LEADINGPLUS
#undef FS_LEADINGSPACE
#undef FS_LEADING_0X
#undef FS_LEADINGZERO
#undef FS_INTMAX
#undef FS_TRIPLET_COMMA
#undef FS_NEGATIVE
#undef FS_METRIC_SUFFIX
#undef FS_NUMSZ
#undef fs_chk_cb_bufL
#undef fs_chk_cb_buf
#undef fs_flush_cb
#undef fs_cb_buf_clamp




FS_API_SPRINTF_DEF int fs_sprintf(char* buf, char const* fmt, ...)
{
   int result;
   va_list va;
   va_start(va, fmt);
   result = fs_vsprintfcb(0, 0, buf, fmt, va);
   va_end(va);
   return result;
}

typedef struct fs_sprintf_context {
   char* buf;
   size_t count;
   size_t length;
   char tmp[FS_SPRINTF_MIN];
} fs_sprintf_context;

static char* fs_clamp_callback(const char* buf, void* user, size_t len)
{
   fs_sprintf_context *c = (fs_sprintf_context *)user;
   c->length += len;

   if (len > c->count)
      len = c->count;

   if (len) {
      if (buf != c->buf) {
         const char* s, *se;
         char* d;
         d = c->buf;
         s = buf;
         se = buf + len;
         do {
            *d++ = *s++;
         } while (s < se);
      }
      c->buf += len;
      c->count -= len;
   }

   if (c->count <= 0)
      return c->tmp;
   return (c->count >= FS_SPRINTF_MIN) ? c->buf : c->tmp; 
}

static char*  fs_count_clamp_callback( const char*  buf, void*  user, size_t len )
{
   fs_sprintf_context * c = (fs_sprintf_context*)user;
   (void) sizeof(buf);

   c->length += len;
   return c->tmp; 
}

FS_API_SPRINTF_DEF int fs_vsnprintf( char*  buf, size_t count, char const*  fmt, va_list va )
{
   fs_sprintf_context c;

   if ( (count == 0) && !buf )
   {
      c.length = 0;

      fs_vsprintfcb( fs_count_clamp_callback, &c, c.tmp, fmt, va );
   }
   else
   {
      size_t l;

      c.buf = buf;
      c.count = count;
      c.length = 0;

      fs_vsprintfcb( fs_clamp_callback, &c, fs_clamp_callback(0,&c,0), fmt, va );

      
      l = (size_t)( c.buf - buf );
      if ( l >= count ) 
         l = count - 1;
      buf[l] = 0;
   }

   return (int)c.length;
}

FS_API_SPRINTF_DEF int fs_snprintf(char* buf, size_t count, char const* fmt, ...)
{
   int result;
   va_list va;
   va_start(va, fmt);

   result = fs_vsnprintf(buf, count, fmt, va);
   va_end(va);

   return result;
}

FS_API_SPRINTF_DEF int fs_vsprintf(char* buf, char const* fmt, va_list va)
{
   return fs_vsprintfcb(0, 0, buf, fmt, va);
}




#ifndef FS_SPRINTF_NOFLOAT


#define FS_COPYFP(dest, src)                   \
   {                                               \
      int cn;                                      \
      for (cn = 0; cn < 8; cn++)                   \
         ((char* )&dest)[cn] = ((char* )&src)[cn]; \
   }


static fs_int32 fs_real_to_parts(fs_int64 *bits, fs_int32 *expo, double value)
{
   double d;
   fs_int64 b = 0;

   
   d = value;

   FS_COPYFP(b, d);

   *bits = b & ((((fs_uint64)1) << 52) - 1);
   *expo = (fs_int32)(((b >> 52) & 2047) - 1023);

   return (fs_int32)((fs_uint64) b >> 63);
}

static double const fs_bot[23] = {
   1e+000, 1e+001, 1e+002, 1e+003, 1e+004, 1e+005, 1e+006, 1e+007, 1e+008, 1e+009, 1e+010, 1e+011,
   1e+012, 1e+013, 1e+014, 1e+015, 1e+016, 1e+017, 1e+018, 1e+019, 1e+020, 1e+021, 1e+022
};
static double const fs_negbot[22] = {
   1e-001, 1e-002, 1e-003, 1e-004, 1e-005, 1e-006, 1e-007, 1e-008, 1e-009, 1e-010, 1e-011,
   1e-012, 1e-013, 1e-014, 1e-015, 1e-016, 1e-017, 1e-018, 1e-019, 1e-020, 1e-021, 1e-022
};
static double const fs_negboterr[22] = {
   -5.551115123125783e-018,  -2.0816681711721684e-019, -2.0816681711721686e-020, -4.7921736023859299e-021, -8.1803053914031305e-022, 4.5251888174113741e-023,
   4.5251888174113739e-024,  -2.0922560830128471e-025, -6.2281591457779853e-026, -3.6432197315497743e-027, 6.0503030718060191e-028,  2.0113352370744385e-029,
   -3.0373745563400371e-030, 1.1806906454401013e-032,  -7.7705399876661076e-032, 2.0902213275965398e-033,  -7.1542424054621921e-034, -7.1542424054621926e-035,
   2.4754073164739869e-036,  5.4846728545790429e-037,  9.2462547772103625e-038,  -4.8596774326570872e-039
};
static double const fs_top[13] = {
   1e+023, 1e+046, 1e+069, 1e+092, 1e+115, 1e+138, 1e+161, 1e+184, 1e+207, 1e+230, 1e+253, 1e+276, 1e+299
};
static double const fs_negtop[13] = {
   1e-023, 1e-046, 1e-069, 1e-092, 1e-115, 1e-138, 1e-161, 1e-184, 1e-207, 1e-230, 1e-253, 1e-276, 1e-299
};
static double const fs_toperr[13] = {
   8388608,
   6.8601809640529717e+028,
   -7.253143638152921e+052,
   -4.3377296974619174e+075,
   -1.5559416129466825e+098,
   -3.2841562489204913e+121,
   -3.7745893248228135e+144,
   -1.7356668416969134e+167,
   -3.8893577551088374e+190,
   -9.9566444326005119e+213,
   6.3641293062232429e+236,
   -5.2069140800249813e+259,
   -5.2504760255204387e+282
};
static double const fs_negtoperr[13] = {
   3.9565301985100693e-040,  -2.299904345391321e-063,  3.6506201437945798e-086,  1.1875228833981544e-109,
   -5.0644902316928607e-132, -6.7156837247865426e-155, -2.812077463003139e-178,  -5.7778912386589953e-201,
   7.4997100559334532e-224,  -4.6439668915134491e-247, -6.3691100762962136e-270, -9.436808465446358e-293,
   8.0970921678014997e-317
};

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
static fs_uint64 const fs_powten[20] = {
   1,
   10,
   100,
   1000,
   10000,
   100000,
   1000000,
   10000000,
   100000000,
   1000000000,
   10000000000,
   100000000000,
   1000000000000,
   10000000000000,
   100000000000000,
   1000000000000000,
   10000000000000000,
   100000000000000000,
   1000000000000000000,
   10000000000000000000U
};
#define fs_tento19th ((fs_uint64)1000000000000000000)
#else
static fs_uint64 const fs_powten[20] = {
   1,
   10,
   100,
   1000,
   10000,
   100000,
   1000000,
   10000000,
   100000000,
   1000000000,
   10000000000ULL,
   100000000000ULL,
   1000000000000ULL,
   10000000000000ULL,
   100000000000000ULL,
   1000000000000000ULL,
   10000000000000000ULL,
   100000000000000000ULL,
   1000000000000000000ULL,
   10000000000000000000ULL
};
#define fs_tento19th (1000000000000000000ULL)
#endif

#define fs_ddmulthi(oh, ol, xh, yh)                            \
   {                                                               \
      double ahi = 0, alo, bhi = 0, blo;                           \
      fs_int64 bt;                                             \
      oh = xh * yh;                                                \
      FS_COPYFP(bt, xh);                                       \
      bt &= ((~(fs_uint64)0) << 27);                           \
      FS_COPYFP(ahi, bt);                                      \
      alo = xh - ahi;                                              \
      FS_COPYFP(bt, yh);                                       \
      bt &= ((~(fs_uint64)0) << 27);                           \
      FS_COPYFP(bhi, bt);                                      \
      blo = yh - bhi;                                              \
      ol = ((ahi * bhi - oh) + ahi * blo + alo * bhi) + alo * blo; \
   }

#define fs_ddtoS64(ob, xh, xl)          \
   {                                        \
      double ahi = 0, alo, vh, t;           \
      ob = (fs_int64)xh;                \
      vh = (double)ob;                      \
      ahi = (xh - vh);                      \
      t = (ahi - xh);                       \
      alo = (xh - (ahi - t)) - (vh + t);    \
      ob += (fs_int64)(ahi + alo + xl); \
   }

#define fs_ddrenorm(oh, ol) \
   {                            \
      double s;                 \
      s = oh + ol;              \
      ol = ol - (s - oh);       \
      oh = s;                   \
   }

#define fs_ddmultlo(oh, ol, xh, xl, yh, yl) ol = ol + (xh * yl + xl * yh);

#define fs_ddmultlos(oh, ol, xh, yl) ol = ol + (xh * yl);

static void fs_raise_to_power10(double *ohi, double *olo, double d, fs_int32 power) 
{
   double ph, pl;
   if ((power >= 0) && (power <= 22)) {
      fs_ddmulthi(ph, pl, d, fs_bot[power]);
   } else {
      fs_int32 e, et, eb;
      double p2h, p2l;

      e = power;
      if (power < 0)
         e = -e;
      et = (e * 0x2c9) >> 14; 
      if (et > 13)
         et = 13;
      eb = e - (et * 23);

      ph = d;
      pl = 0.0;
      if (power < 0) {
         if (eb) {
            --eb;
            fs_ddmulthi(ph, pl, d, fs_negbot[eb]);
            fs_ddmultlos(ph, pl, d, fs_negboterr[eb]);
         }
         if (et) {
            fs_ddrenorm(ph, pl);
            --et;
            fs_ddmulthi(p2h, p2l, ph, fs_negtop[et]);
            fs_ddmultlo(p2h, p2l, ph, pl, fs_negtop[et], fs_negtoperr[et]);
            ph = p2h;
            pl = p2l;
         }
      } else {
         if (eb) {
            e = eb;
            if (eb > 22)
               eb = 22;
            e -= eb;
            fs_ddmulthi(ph, pl, d, fs_bot[eb]);
            if (e) {
               fs_ddrenorm(ph, pl);
               fs_ddmulthi(p2h, p2l, ph, fs_bot[e]);
               fs_ddmultlos(p2h, p2l, fs_bot[e], pl);
               ph = p2h;
               pl = p2l;
            }
         }
         if (et) {
            fs_ddrenorm(ph, pl);
            --et;
            fs_ddmulthi(p2h, p2l, ph, fs_top[et]);
            fs_ddmultlo(p2h, p2l, ph, pl, fs_top[et], fs_toperr[et]);
            ph = p2h;
            pl = p2l;
         }
      }
   }
   fs_ddrenorm(ph, pl);
   *ohi = ph;
   *olo = pl;
}





static fs_int32 fs_real_to_str(char const* *start, fs_uint32 *len, char* out, fs_int32 *decimal_pos, double value, fs_uint32 frac_digits)
{
   double d;
   fs_int64 bits = 0;
   fs_int32 expo, e, ng, tens;

   d = value;
   FS_COPYFP(bits, d);
   expo = (fs_int32)((bits >> 52) & 2047);
   ng = (fs_int32)((fs_uint64) bits >> 63);
   if (ng)
      d = -d;

   if (expo == 2047) 
   {
      *start = (bits & ((((fs_uint64)1) << 52) - 1)) ? "NaN" : "Inf";
      *decimal_pos = FS_SPECIAL;
      *len = 3;
      return ng;
   }

   if (expo == 0) 
   {
      if (((fs_uint64) bits << 1) == 0) 
      {
         *decimal_pos = 1;
         *start = out;
         out[0] = '0';
         *len = 1;
         return ng;
      }
      
      {
         fs_int64 v = ((fs_uint64)1) << 51;
         while ((bits & v) == 0) {
            --expo;
            v >>= 1;
         }
      }
   }

   
   {
      double ph, pl;

      
      tens = expo - 1023;
      tens = (tens < 0) ? ((tens * 617) / 2048) : (((tens * 1233) / 4096) + 1);

      
      fs_raise_to_power10(&ph, &pl, d, 18 - tens);

      
      fs_ddtoS64(bits, ph, pl);

      
      if (((fs_uint64)bits) >= fs_tento19th)
         ++tens;
   }

   
   frac_digits = (frac_digits & 0x80000000) ? ((frac_digits & 0x7ffffff) + 1) : (tens + frac_digits);
   if ((frac_digits < 24)) {
      fs_uint32 dg = 1;
      if ((fs_uint64)bits >= fs_powten[9])
         dg = 10;
      while ((fs_uint64)bits >= fs_powten[dg]) {
         ++dg;
         if (dg == 20)
            goto noround;
      }
      if (frac_digits < dg) {
         fs_uint64 r;
         
         e = dg - frac_digits;
         if ((fs_uint32)e >= 24)
            goto noround;
         r = fs_powten[e];
         bits = bits + (r / 2);
         if ((fs_uint64)bits >= fs_powten[dg])
            ++tens;
         bits /= r;
      }
   noround:;
   }

   
   if (bits) {
      fs_uint32 n;
      for (;;) {
         if (bits <= 0xffffffff)
            break;
         if (bits % 1000)
            goto donez;
         bits /= 1000;
      }
      n = (fs_uint32)bits;
      while ((n % 1000) == 0)
         n /= 1000;
      bits = n;
   donez:;
   }

   
   out += 64;
   e = 0;
   for (;;) {
      fs_uint32 n;
      char* o = out - 8;
      
      if (bits >= 100000000) {
         n = (fs_uint32)(bits % 100000000);
         bits /= 100000000;
      } else {
         n = (fs_uint32)bits;
         bits = 0;
      }
      while (n) {
         out -= 2;
         *(fs_uint16 *)out = *(fs_uint16 *)&fs_digitpair.pair[(n % 100) * 2];
         n /= 100;
         e += 2;
      }
      if (bits == 0) {
         if ((e) && (out[0] == '0')) {
            ++out;
            --e;
         }
         break;
      }
      while (out != o) {
         *--out = '0';
         ++e;
      }
   }

   *decimal_pos = tens;
   *start = out;
   *len = e;
   return ng;
}

#undef fs_ddmulthi
#undef fs_ddrenorm
#undef fs_ddmultlo
#undef fs_ddmultlos
#undef FS_SPECIAL
#undef FS_COPYFP

#endif 


#undef FS_UNALIGNED
/* END stb_sprintf.c */

#if defined(__GNUC__) && (__GNUC__ >= 7 || (__GNUC__ == 6 && __GNUC_MINOR__ >= 1))
    #pragma GCC diagnostic pop
#endif
/* END fs_snprintf.c */


#endif  /* fs_c */

/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2025 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
