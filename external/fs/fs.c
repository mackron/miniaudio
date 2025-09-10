#ifndef fs_c
#define fs_c

#include "fs.h"

/* BEG fs_platform_detection.c */
#if defined(_WIN32)
    #define FS_WIN32
#else
    #define FS_POSIX
#endif
/* END fs_platform_detection.c */

#include <errno.h>

/* BEG fs_common_macros.c */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* BEG fs_va_copy.c */
#ifndef fs_va_copy
    #if !defined(_MSC_VER) || _MSC_VER >= 1800
        #if !defined(__STDC_VERSION__) || (defined(__GNUC__) && __GNUC__ < 3)   /* <-- va_copy() is not available when using `-std=c89`. The `!defined(__STDC_VERSION__)` parts is what checks for this. */
            #if defined(__va_copy)
                #define fs_va_copy(dst, src) __va_copy(dst, src)
            #else
                #define fs_va_copy(dst, src) ((dst) = (src))    /* This is untested. Not sure if this is correct for old GCC. */
            #endif
        #else
            #define fs_va_copy(dst, src) va_copy((dst), (src))
        #endif
    #else
        #define fs_va_copy(dst, src) ((dst) = (src))
    #endif
#endif
/* END fs_va_copy.c */

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
/* END fs_common_macros.c */

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

FS_API int fs_strcat_s(char* dst, size_t dstCap, const char* src)
{
    char* dstorig;

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

    dstorig = dst;

    while (dstCap > 0 && dst[0] != '\0') {
        dst    += 1;
        dstCap -= 1;
    }

    if (dstCap == 0) {
        return EINVAL;  /* Unterminated. */
    }

    while (dstCap > 0 && src[0] != '\0') {
        *dst++ = *src++;
        dstCap -= 1;
    }

    if (dstCap > 0) {
        dst[0] = '\0';
    } else {
        dstorig[0] = '\0';
        return ERANGE;
    }

    return 0;
}

FS_API int fs_strncat_s(char* dst, size_t dstCap, const char* src, size_t count)
{
    char* dstorig;

    if (dst == 0) {
        return EINVAL;
    }
    if (dstCap == 0) {
        return ERANGE;
    }
    if (src == 0) {
        return EINVAL;
    }

    dstorig = dst;

    while (dstCap > 0 && dst[0] != '\0') {
        dst    += 1;
        dstCap -= 1;
    }

    if (dstCap == 0) {
        return EINVAL;  /* Unterminated. */
    }

    if (count == ((size_t)-1)) {        /* _TRUNCATE */
        count = dstCap - 1;
    }

    while (dstCap > 0 && src[0] != '\0' && count > 0) {
        *dst++ = *src++;
        dstCap -= 1;
        count  -= 1;
    }

    if (dstCap > 0) {
        dst[0] = '\0';
    } else {
        dstorig[0] = '\0';
        return ERANGE;
    }

    return 0;
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


/* BEG fs_result.c */
FS_API const char* fs_result_to_string(fs_result result)
{
    switch (result)
    {
        case FS_SUCCESS:                       return "No error";
        case FS_ERROR:                         return "Unknown error";
        case FS_INVALID_ARGS:                  return "Invalid argument";
        case FS_INVALID_OPERATION:             return "Invalid operation";
        case FS_OUT_OF_MEMORY:                 return "Out of memory";
        case FS_OUT_OF_RANGE:                  return "Out of range";
        case FS_ACCESS_DENIED:                 return "Permission denied";
        case FS_DOES_NOT_EXIST:                return "Resource does not exist";
        case FS_ALREADY_EXISTS:                return "Resource already exists";
        case FS_TOO_MANY_OPEN_FILES:           return "Too many open files";
        case FS_INVALID_FILE:                  return "Invalid file";
        case FS_TOO_BIG:                       return "Too large";
        case FS_PATH_TOO_LONG:                 return "Path too long";
        case FS_NAME_TOO_LONG:                 return "Name too long";
        case FS_NOT_DIRECTORY:                 return "Not a directory";
        case FS_IS_DIRECTORY:                  return "Is a directory";
        case FS_DIRECTORY_NOT_EMPTY:           return "Directory not empty";
        case FS_AT_END:                        return "At end";
        case FS_NO_SPACE:                      return "No space available";
        case FS_BUSY:                          return "Device or resource busy";
        case FS_IO_ERROR:                      return "Input/output error";
        case FS_INTERRUPT:                     return "Interrupted";
        case FS_UNAVAILABLE:                   return "Resource unavailable";
        case FS_ALREADY_IN_USE:                return "Resource already in use";
        case FS_BAD_ADDRESS:                   return "Bad address";
        case FS_BAD_SEEK:                      return "Illegal seek";
        case FS_BAD_PIPE:                      return "Broken pipe";
        case FS_DEADLOCK:                      return "Deadlock";
        case FS_TOO_MANY_LINKS:                return "Too many links";
        case FS_NOT_IMPLEMENTED:               return "Not implemented";
        case FS_NO_MESSAGE:                    return "No message of desired type";
        case FS_BAD_MESSAGE:                   return "Invalid message";
        case FS_NO_DATA_AVAILABLE:             return "No data available";
        case FS_INVALID_DATA:                  return "Invalid data";
        case FS_TIMEOUT:                       return "Timeout";
        case FS_NO_NETWORK:                    return "Network unavailable";
        case FS_NOT_UNIQUE:                    return "Not unique";
        case FS_NOT_SOCKET:                    return "Socket operation on non-socket";
        case FS_NO_ADDRESS:                    return "Destination address required";
        case FS_BAD_PROTOCOL:                  return "Protocol wrong type for socket";
        case FS_PROTOCOL_UNAVAILABLE:          return "Protocol not available";
        case FS_PROTOCOL_NOT_SUPPORTED:        return "Protocol not supported";
        case FS_PROTOCOL_FAMILY_NOT_SUPPORTED: return "Protocol family not supported";
        case FS_ADDRESS_FAMILY_NOT_SUPPORTED:  return "Address family not supported";
        case FS_SOCKET_NOT_SUPPORTED:          return "Socket type not supported";
        case FS_CONNECTION_RESET:              return "Connection reset";
        case FS_ALREADY_CONNECTED:             return "Already connected";
        case FS_NOT_CONNECTED:                 return "Not connected";
        case FS_CONNECTION_REFUSED:            return "Connection refused";
        case FS_NO_HOST:                       return "No host";
        case FS_IN_PROGRESS:                   return "Operation in progress";
        case FS_CANCELLED:                     return "Operation cancelled";
        case FS_MEMORY_ALREADY_MAPPED:         return "Memory already mapped";
        case FS_DIFFERENT_DEVICE:              return "Different device";
        default:                               return "Unknown error";
    }
}

#include <errno.h>

FS_API fs_result fs_result_from_errno(int error)
{
    if (error == 0) {
        return FS_SUCCESS;
    }
#ifdef EPERM
    else if (error == EPERM) { return FS_INVALID_OPERATION; }
#endif
#ifdef ENOENT
    else if (error == ENOENT) { return FS_DOES_NOT_EXIST; }
#endif
#ifdef ESRCH
    else if (error == ESRCH) { return FS_DOES_NOT_EXIST; }
#endif
#ifdef EINTR
    else if (error == EINTR) { return FS_INTERRUPT; }
#endif
#ifdef EIO
    else if (error == EIO) { return FS_IO_ERROR; }
#endif
#ifdef ENXIO
    else if (error == ENXIO) { return FS_DOES_NOT_EXIST; }
#endif
#ifdef E2BIG
    else if (error == E2BIG) { return FS_INVALID_ARGS; }
#endif
#ifdef ENOEXEC
    else if (error == ENOEXEC) { return FS_INVALID_FILE; }
#endif
#ifdef EBADF
    else if (error == EBADF) { return FS_INVALID_FILE; }
#endif
#ifdef EAGAIN
    else if (error == EAGAIN) { return FS_UNAVAILABLE; }
#endif
#ifdef ENOMEM
    else if (error == ENOMEM) { return FS_OUT_OF_MEMORY; }
#endif
#ifdef EACCES
    else if (error == EACCES) { return FS_ACCESS_DENIED; }
#endif
#ifdef EFAULT
    else if (error == EFAULT) { return FS_BAD_ADDRESS; }
#endif
#ifdef EBUSY
    else if (error == EBUSY) { return FS_BUSY; }
#endif
#ifdef EEXIST
    else if (error == EEXIST) { return FS_ALREADY_EXISTS; }
#endif
#ifdef EXDEV
    else if (error == EXDEV) { return FS_DIFFERENT_DEVICE; }
#endif
#ifdef ENODEV
    else if (error == ENODEV) { return FS_DOES_NOT_EXIST; }
#endif
#ifdef ENOTDIR
    else if (error == ENOTDIR) { return FS_NOT_DIRECTORY; }
#endif
#ifdef EISDIR
    else if (error == EISDIR) { return FS_IS_DIRECTORY; }
#endif
#ifdef EINVAL
    else if (error == EINVAL) { return FS_INVALID_ARGS; }
#endif
#ifdef ENFILE
    else if (error == ENFILE) { return FS_TOO_MANY_OPEN_FILES; }
#endif
#ifdef EMFILE
    else if (error == EMFILE) { return FS_TOO_MANY_OPEN_FILES; }
#endif
#ifdef ENOTTY
    else if (error == ENOTTY) { return FS_INVALID_OPERATION; }
#endif
#ifdef ETXTBSY
    else if (error == ETXTBSY) { return FS_BUSY; }
#endif
#ifdef EFBIG
    else if (error == EFBIG) { return FS_TOO_BIG; }
#endif
#ifdef ENOSPC
    else if (error == ENOSPC) { return FS_NO_SPACE; }
#endif
#ifdef ESPIPE
    else if (error == ESPIPE) { return FS_BAD_SEEK; }
#endif
#ifdef EROFS
    else if (error == EROFS) { return FS_ACCESS_DENIED; }
#endif
#ifdef EPIPE
    else if (error == EPIPE) { return FS_BAD_PIPE; }
#endif
#ifdef EDOM
    else if (error == EDOM) { return FS_OUT_OF_RANGE; }
#endif
#ifdef ERANGE
    else if (error == ERANGE) { return FS_OUT_OF_RANGE; }
#endif
#ifdef EDEADLK
    else if (error == EDEADLK) { return FS_DEADLOCK; }
#endif
#ifdef ENAMETOOLONG
    else if (error == ENAMETOOLONG) { return FS_PATH_TOO_LONG; }
#endif
#ifdef ENOSYS
    else if (error == ENOSYS) { return FS_NOT_IMPLEMENTED; }
#endif
#ifdef ENOTEMPTY
    else if (error == ENOTEMPTY) { return FS_DIRECTORY_NOT_EMPTY; }
#endif
#ifdef ELNRNG
    else if (error == ELNRNG) { return FS_OUT_OF_RANGE; }
#endif
#ifdef EBFONT
    else if (error == EBFONT) { return FS_INVALID_FILE; }
#endif
#ifdef ENODATA
    else if (error == ENODATA) { return FS_NO_DATA_AVAILABLE; }
#endif
#ifdef ETIME
    else if (error == ETIME) { return FS_TIMEOUT; }
#endif
#ifdef ENOSR
    else if (error == ENOSR) { return FS_NO_DATA_AVAILABLE; }
#endif
#ifdef ENONET
    else if (error == ENONET) { return FS_NO_NETWORK; }
#endif
#ifdef EOVERFLOW
    else if (error == EOVERFLOW) { return FS_TOO_BIG; }
#endif
#ifdef ELIBACC
    else if (error == ELIBACC) { return FS_ACCESS_DENIED; }
#endif
#ifdef ELIBBAD
    else if (error == ELIBBAD) { return FS_INVALID_FILE; }
#endif
#ifdef ELIBSCN
    else if (error == ELIBSCN) { return FS_INVALID_FILE; }
#endif
#ifdef EILSEQ
    else if (error == EILSEQ) { return FS_INVALID_DATA; }
#endif
#ifdef ENOTSOCK
    else if (error == ENOTSOCK) { return FS_NOT_SOCKET; }
#endif
#ifdef EDESTADDRREQ
    else if (error == EDESTADDRREQ) { return FS_NO_ADDRESS; }
#endif
#ifdef EMSGSIZE
    else if (error == EMSGSIZE) { return FS_TOO_BIG; }
#endif
#ifdef EPROTOTYPE
    else if (error == EPROTOTYPE) { return FS_BAD_PROTOCOL; }
#endif
#ifdef ENOPROTOOPT
    else if (error == ENOPROTOOPT) { return FS_PROTOCOL_UNAVAILABLE; }
#endif
#ifdef EPROTONOSUPPORT
    else if (error == EPROTONOSUPPORT) { return FS_PROTOCOL_NOT_SUPPORTED; }
#endif
#ifdef ESOCKTNOSUPPORT
    else if (error == ESOCKTNOSUPPORT) { return FS_SOCKET_NOT_SUPPORTED; }
#endif
#ifdef EOPNOTSUPP
    else if (error == EOPNOTSUPP) { return FS_INVALID_OPERATION; }
#endif
#ifdef EPFNOSUPPORT
    else if (error == EPFNOSUPPORT) { return FS_PROTOCOL_FAMILY_NOT_SUPPORTED; }
#endif
#ifdef EAFNOSUPPORT
    else if (error == EAFNOSUPPORT) { return FS_ADDRESS_FAMILY_NOT_SUPPORTED; }
#endif
#ifdef EADDRINUSE
    else if (error == EADDRINUSE) { return FS_ALREADY_IN_USE; }
#endif
#ifdef ENETDOWN
    else if (error == ENETDOWN) { return FS_NO_NETWORK; }
#endif
#ifdef ENETUNREACH
    else if (error == ENETUNREACH) { return FS_NO_NETWORK; }
#endif
#ifdef ENETRESET
    else if (error == ENETRESET) { return FS_NO_NETWORK; }
#endif
#ifdef ECONNABORTED
    else if (error == ECONNABORTED) { return FS_NO_NETWORK; }
#endif
#ifdef ECONNRESET
    else if (error == ECONNRESET) { return FS_CONNECTION_RESET; }
#endif
#ifdef ENOBUFS
    else if (error == ENOBUFS) { return FS_NO_SPACE; }
#endif
#ifdef EISCONN
    else if (error == EISCONN) { return FS_ALREADY_CONNECTED; }
#endif
#ifdef ENOTCONN
    else if (error == ENOTCONN) { return FS_NOT_CONNECTED; }
#endif
#ifdef ETIMEDOUT
    else if (error == ETIMEDOUT) { return FS_TIMEOUT; }
#endif
#ifdef ECONNREFUSED
    else if (error == ECONNREFUSED) { return FS_CONNECTION_REFUSED; }
#endif
#ifdef EHOSTDOWN
    else if (error == EHOSTDOWN) { return FS_NO_HOST; }
#endif
#ifdef EHOSTUNREACH
    else if (error == EHOSTUNREACH) { return FS_NO_HOST; }
#endif
#ifdef EALREADY
    else if (error == EALREADY) { return FS_IN_PROGRESS; }
#endif
#ifdef EINPROGRESS
    else if (error == EINPROGRESS) { return FS_IN_PROGRESS; }
#endif
#ifdef ESTALE
    else if (error == ESTALE) { return FS_INVALID_FILE; }
#endif
#ifdef EREMOTEIO
    else if (error == EREMOTEIO) { return FS_IO_ERROR; }
#endif
#ifdef EDQUOT
    else if (error == EDQUOT) { return FS_NO_SPACE; }
#endif
#ifdef ENOMEDIUM
    else if (error == ENOMEDIUM) { return FS_DOES_NOT_EXIST; }
#endif
#ifdef ECANCELED
    else if (error == ECANCELED) { return FS_CANCELLED; }
#endif
    
    return FS_ERROR;
}

#if defined(_WIN32)
#include <windows.h> /* For GetLastError, ERROR_* constants. */

FS_API fs_result fs_result_from_GetLastError(void)
{
    switch (GetLastError())
    {
        case ERROR_SUCCESS:                return FS_SUCCESS;
        case ERROR_NOT_ENOUGH_MEMORY:      return FS_OUT_OF_MEMORY;
        case ERROR_OUTOFMEMORY:            return FS_OUT_OF_MEMORY;
        case ERROR_BUSY:                   return FS_BUSY;
        case ERROR_SEM_TIMEOUT:            return FS_TIMEOUT;
        case ERROR_ALREADY_EXISTS:         return FS_ALREADY_EXISTS;
        case ERROR_FILE_EXISTS:            return FS_ALREADY_EXISTS;
        case ERROR_ACCESS_DENIED:          return FS_ACCESS_DENIED;
        case ERROR_WRITE_PROTECT:          return FS_ACCESS_DENIED;
        case ERROR_PRIVILEGE_NOT_HELD:     return FS_ACCESS_DENIED;
        case ERROR_SHARING_VIOLATION:      return FS_ACCESS_DENIED;
        case ERROR_LOCK_VIOLATION:         return FS_ACCESS_DENIED;
        case ERROR_FILE_NOT_FOUND:         return FS_DOES_NOT_EXIST;
        case ERROR_PATH_NOT_FOUND:         return FS_DOES_NOT_EXIST;
        case ERROR_INVALID_NAME:           return FS_INVALID_ARGS;
        case ERROR_BAD_PATHNAME:           return FS_INVALID_ARGS;
        case ERROR_INVALID_PARAMETER:      return FS_INVALID_ARGS;
        case ERROR_INVALID_HANDLE:         return FS_INVALID_ARGS;
        case ERROR_INVALID_FUNCTION:       return FS_INVALID_OPERATION;
        case ERROR_FILENAME_EXCED_RANGE:   return FS_PATH_TOO_LONG;
        case ERROR_DIRECTORY:              return FS_NOT_DIRECTORY;
        case ERROR_DIR_NOT_EMPTY:          return FS_DIRECTORY_NOT_EMPTY;
        case ERROR_FILE_TOO_LARGE:         return FS_TOO_BIG;
        case ERROR_DISK_FULL:              return FS_OUT_OF_RANGE;
        case ERROR_HANDLE_EOF:             return FS_AT_END;
        case ERROR_SEEK:                   return FS_BAD_SEEK;
        case ERROR_OPERATION_ABORTED:      return FS_CANCELLED;
        case ERROR_CANCELLED:              return FS_INTERRUPT;
        case ERROR_TOO_MANY_OPEN_FILES:    return FS_TOO_MANY_OPEN_FILES;
        case ERROR_INVALID_DATA:           return FS_INVALID_DATA;
        case ERROR_NO_DATA:                return FS_NO_DATA_AVAILABLE;
        case ERROR_NOT_SAME_DEVICE:        return FS_DIFFERENT_DEVICE;
        default:                           return FS_ERROR; /* Generic error. */
    }
}
#endif /* _WIN32 */
/* END fs_result.c */


/* BEG fs_allocation_callbacks.c */
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
    * There is no c89mtx_trylock() or c89mtx_timedlock() equivalent.
    * c89thrd_success is FS_SUCCESS
    * c89thrd_error is FS_ERROR
    * c89thrd_busy is FS_BUSY
    * c89thrd_pthread_* is fs_pthread_*

Parameter ordering is the same as c89thread to make amalgamation easier.
*/

/* BEG fs_thread_basic_types.c */
#if defined(FS_POSIX)
    #ifndef FS_USE_PTHREAD
    #define FS_USE_PTHREAD
    #endif

    #ifndef FS_NO_PTHREAD_IN_HEADER
        #include <pthread.h>
        typedef pthread_t       fs_pthread_t;
        typedef pthread_mutex_t fs_pthread_mutex_t;
    #else
        typedef fs_uintptr      fs_pthread_t;
        typedef union           fs_pthread_mutex_t { char __data[40]; fs_uint64 __alignment; } fs_pthread_mutex_t;
    #endif
#endif
/* END fs_thread_basic_types.c */


/* BEG fs_thread_mtx.h */
#if defined(FS_WIN32)
    typedef struct
    {
        void* handle;    /* HANDLE, CreateMutex(), CreateEvent() */
        int type;
    } fs_mtx;
#else
    /*
    We may need to force the use of a manual recursive mutex which will happen when compiling
    on very old compilers, or with `-std=c89`.
    */

    /* If __STDC_VERSION__ is not defined it means we're compiling in C89 mode. */
    #if !defined(FS_USE_MANUAL_RECURSIVE_MUTEX) && !defined(__STDC_VERSION__)
        #define FS_USE_MANUAL_RECURSIVE_MUTEX
    #endif

    /* This is for checking if PTHREAD_MUTEX_RECURSIVE is available. */
    #if !defined(FS_USE_MANUAL_RECURSIVE_MUTEX) && (!defined(__USE_UNIX98) && !defined(__USE_XOPEN2K8))
        #define FS_USE_MANUAL_RECURSIVE_MUTEX
    #endif

    #ifdef FS_USE_MANUAL_RECURSIVE_MUTEX
        typedef struct
        {
            fs_pthread_mutex_t mutex;    /* The underlying pthread mutex. */
            fs_pthread_mutex_t guard;    /* Guard for metadata (owner and recursionCount). */
            fs_pthread_t owner;
            int recursionCount;
            int type;
        } fs_mtx;
    #else
        typedef fs_pthread_mutex_t fs_mtx;
    #endif
#endif

enum
{
    fs_mtx_plain     = 0x00000000,
    fs_mtx_timed     = 0x00000001,
    fs_mtx_recursive = 0x00000002
};
/* END fs_thread_mtx.h */

/* BEG fs_thread_mtx.c */
#if defined(FS_WIN32) && !defined(FS_USE_PTHREAD)
static int fs_mtx_init(fs_mtx* mutex, int type)
{
    HANDLE hMutex;

    if (mutex == NULL) {
        return FS_ERROR;
    }

    /* Initialize the object to zero for safety. */
    mutex->handle = NULL;
    mutex->type   = 0;

    /*
    CreateMutex() will create a thread-aware mutex (allowing recursiveness), whereas an auto-reset
    event (CreateEvent()) is not thread-aware and will deadlock (will not allow recursiveness). In
    Win32 I'm making all mutex's timeable.
    */
    if ((type & fs_mtx_recursive) != 0) {
        hMutex = CreateMutexA(NULL, FALSE, NULL);
    } else {
        hMutex = CreateEventA(NULL, FALSE, TRUE, NULL);
    }

    if (hMutex == NULL) {
        return fs_result_from_GetLastError();
    }

    mutex->handle = (void*)hMutex;
    mutex->type   = type;

    return FS_SUCCESS;
}

static void fs_mtx_destroy(fs_mtx* mutex)
{
    if (mutex == NULL) {
        return;
    }

    CloseHandle((HANDLE)mutex->handle);
}

static int fs_mtx_lock(fs_mtx* mutex)
{
    DWORD result;

    if (mutex == NULL) {
        return FS_ERROR;
    }

    result = WaitForSingleObject((HANDLE)mutex->handle, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return FS_ERROR;
    }

    return FS_SUCCESS;
}

static int fs_mtx_unlock(fs_mtx* mutex)
{
    BOOL result;

    if (mutex == NULL) {
        return FS_ERROR;
    }

    if ((mutex->type & fs_mtx_recursive) != 0) {
        result = ReleaseMutex((HANDLE)mutex->handle);
    } else {
        result = SetEvent((HANDLE)mutex->handle);
    }

    if (!result) {
        return FS_ERROR;
    }

    return FS_SUCCESS;
}
#else
static int fs_mtx_init(fs_mtx* mutex, int type)
{
    int result;

    if (mutex == NULL) {
        return FS_ERROR;
    }

    #ifdef FS_USE_MANUAL_RECURSIVE_MUTEX
    {
        /* Initialize the main mutex */
        result = pthread_mutex_init(&mutex->mutex, NULL);
        if (result != 0) {
            return FS_ERROR;
        }
        
        /* For recursive mutexes, we need the guard mutex and metadata */
        if ((type & fs_mtx_recursive) != 0) {
            if (pthread_mutex_init(&mutex->guard, NULL) != 0) {
                pthread_mutex_destroy(&mutex->mutex);
                return FS_ERROR;
            }

            mutex->owner = 0;  /* No owner initially. */
            mutex->recursionCount = 0;
        }
        
        mutex->type = type;
        
        return FS_SUCCESS;
    }
    #else
    {
        pthread_mutexattr_t attr;   /* For specifying whether or not the mutex is recursive. */

        pthread_mutexattr_init(&attr);
        if ((type & fs_mtx_recursive) != 0) {
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        } else {
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);     /* Will deadlock. Consistent with Win32. */
        }

        result = pthread_mutex_init((pthread_mutex_t*)mutex, &attr);
        pthread_mutexattr_destroy(&attr);

        if (result != 0) {
            return FS_ERROR;
        }

        return FS_SUCCESS;
    }
    #endif
}

static void fs_mtx_destroy(fs_mtx* mutex)
{
    if (mutex == NULL) {
        return;
    }

    #ifdef FS_USE_MANUAL_RECURSIVE_MUTEX
    {
        /* Only destroy the guard mutex if it was initialized (for recursive mutexes) */
        if ((mutex->type & fs_mtx_recursive) != 0) {
            pthread_mutex_destroy(&mutex->guard);
        }

        pthread_mutex_destroy(&mutex->mutex);
    }
    #else
    {
        pthread_mutex_destroy((pthread_mutex_t*)mutex);
    }
    #endif
}

static int fs_mtx_lock(fs_mtx* mutex)
{
    int result;

    if (mutex == NULL) {
        return FS_ERROR;
    }

    #ifdef FS_USE_MANUAL_RECURSIVE_MUTEX
    {
        pthread_t currentThread;

        /* Optimized path for plain mutexes. */
        if ((mutex->type & fs_mtx_recursive) == 0) {
            result = pthread_mutex_lock(&mutex->mutex);
            if (result != 0) {
                return FS_ERROR;
            }

            return FS_SUCCESS;
        }

        /* Getting here means it's a recursive mutex. */
        currentThread = pthread_self();
        
        /* First, lock the guard mutex to safely access the metadata. */
        result = pthread_mutex_lock(&mutex->guard);
        if (result != 0) {
            return FS_ERROR;
        }

        /* We can bomb out early if the current thread already owns this mutex. */
        if (mutex->recursionCount > 0 && pthread_equal(mutex->owner, currentThread)) {
            mutex->recursionCount += 1;
            pthread_mutex_unlock(&mutex->guard);
            return FS_SUCCESS;
        }

        /* The guard mutex needs to be unlocked before locking the main mutex or else we'll deadlock. */
        pthread_mutex_unlock(&mutex->guard);
        
        result = pthread_mutex_lock(&mutex->mutex);
        if (result != 0) {
            return FS_ERROR;
        }
        
        /* Update metadata. */
        result = pthread_mutex_lock(&mutex->guard);
        if (result != 0) {
            pthread_mutex_unlock(&mutex->mutex);
            return FS_ERROR;
        }
        
        mutex->owner = currentThread;
        mutex->recursionCount = 1;
        
        pthread_mutex_unlock(&mutex->guard);

        return FS_SUCCESS;
    }
    #else
    {
        result = pthread_mutex_lock((pthread_mutex_t*)mutex);
        if (result != 0) {
            return FS_ERROR;
        }

        return FS_SUCCESS;
    }
    #endif
}

static int fs_mtx_unlock(fs_mtx* mutex)
{
    int result;

    if (mutex == NULL) {
        return FS_ERROR;
    }

    #ifdef FS_USE_MANUAL_RECURSIVE_MUTEX
    {
        pthread_t currentThread;

        /* Optimized path for plain mutexes. */
        if ((mutex->type & fs_mtx_recursive) == 0) {
            result = pthread_mutex_unlock(&mutex->mutex);
            if (result != 0) {
                return FS_ERROR;
            }

            return FS_SUCCESS;
        }
        
        /* Getting here means it's a recursive mutex. */
        currentThread = pthread_self();
        
        /* Lock the guard mutex to safely access the metadata */
        result = pthread_mutex_lock(&mutex->guard);
        if (result != 0) {
            return FS_ERROR;
        }
        
        /* Check if the current thread owns the mutex */
        if (mutex->recursionCount == 0 || !pthread_equal(mutex->owner, currentThread)) {
            /* Getting here means we are trying to unlock a mutex that is not owned by this thread. Bomb out. */
            pthread_mutex_unlock(&mutex->guard);
            return FS_ERROR;
        }
        
        mutex->recursionCount -= 1;

        if (mutex->recursionCount == 0) {
            /* Last unlock. Clear ownership and unlock the main mutex. */
            mutex->owner = 0;
            pthread_mutex_unlock(&mutex->guard);

            result = pthread_mutex_unlock(&mutex->mutex);
            if (result != 0) {
                return FS_ERROR;
            }
        } else {
            /* Still recursively locked, just unlock the guard mutex. */
            pthread_mutex_unlock(&mutex->guard);
        }
        
        return FS_SUCCESS;
    }
    #else
    {
        result = pthread_mutex_unlock((pthread_mutex_t*)mutex);
        if (result != 0) {
            return FS_ERROR;
        }

        return FS_SUCCESS;
    }
    #endif
}
#endif
/* END fs_thread_mtx.c */
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
    } else {
        /*
        The caller has not specified a destination for the bytes read. If we didn't output the exact
        number of bytes as requested we'll need to report an error.
        */
        if (result == FS_SUCCESS && bytesRead != bytesToRead) {
            result = FS_ERROR;
        }
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
    va_list args2;

    if (pStream == NULL || fmt == NULL) {
        return FS_INVALID_ARGS;
    }

    fs_va_copy(args2, args);
    {
        strLen = fs_vsnprintf(pStrStack, sizeof(pStrStack), fmt, args2);
    }
    va_end(args2);

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


/* BEG fs.c */
const char* FS_STDIN  = "<si>";
const char* FS_STDOUT = "<so>";
const char* FS_STDERR = "<se>";

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

    if (pBackend->mkdir == NULL) {
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

static fs_result fs_backend_file_truncate(const fs_backend* pBackend, fs_file* pFile)
{
    FS_ASSERT(pBackend != NULL);

    if (pBackend->file_truncate == NULL) {
        return FS_NOT_IMPLEMENTED;
    } else {
        return pBackend->file_truncate(pFile);
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


FS_API fs_archive_type fs_archive_type_init(const fs_backend* pBackend, const char* pExtension)
{
    fs_archive_type archiveType;

    archiveType.pBackend   = pBackend;
    archiveType.pExtension = pExtension;

    return archiveType;
}


/*
This is the maximum number of ureferenced opened archive files that will be kept in memory
before garbage collection of those archives is triggered.
*/
#ifndef FS_DEFAULT_ARCHIVE_GC_THRESHOLD
#define FS_DEFAULT_ARCHIVE_GC_THRESHOLD 10
#endif

#define FS_IS_OPAQUE(mode)      ((mode & FS_OPAQUE ) == FS_OPAQUE )
#define FS_IS_VERBOSE(mode)     ((mode & FS_VERBOSE) == FS_VERBOSE)
#define FS_IS_TRANSPARENT(mode) (!FS_IS_OPAQUE(mode) && !FS_IS_VERBOSE(mode))

FS_API fs_config fs_config_init_default(void)
{
    fs_config config;

    FS_ZERO_OBJECT(&config);

    return config;
}

FS_API fs_config fs_config_init(const fs_backend* pBackend, const void* pBackendConfig, fs_stream* pStream)
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
    fs_on_refcount_changed_proc onRefCountChanged;
    void* pRefCountChangedUserData;
    fs_mtx archiveLock;     /* For use with fs_open_archive() and fs_close_archive(). */
    void* pOpenedArchives;  /* One heap allocation. Structure is [fs*][refcount (size_t)][path][null-terminator][padding (aligned to FS_SIZEOF_PTR)] */
    size_t openedArchivesSize;
    size_t openedArchivesCap;
    size_t archiveGCThreshold;
    fs_mount_list* pReadMountPoints;
    fs_mount_list* pWriteMountPoints;
    fs_mtx refLock;
    fs_uint32 refCount;        /* Incremented when a file is opened, decremented when a file is closed. */
};

struct fs_file
{
    fs_stream stream; /* Files are streams. This must be the first member so it can be cast. */
    fs* pFS;
    fs_stream* pStreamForBackend;   /* The stream for use by the backend. Different to `stream`. This is a duplicate of the stream used by `pFS` so the backend can do reading. */
    size_t backendDataSize;
};

typedef enum fs_mount_priority
{
    FS_MOUNT_PRIORITY_HIGHEST = 0,
    FS_MOUNT_PRIORITY_LOWEST  = 1
} fs_mount_priority;


static void fs_gc(fs* pFS, int policy, fs* pSpecificArchive); /* Generic internal GC function. */


static const char* fs_mount_point_real_path(const fs_mount_point* pMountPoint)
{
    FS_ASSERT(pMountPoint != NULL);

    return (const char*)FS_OFFSET_PTR(pMountPoint, sizeof(fs_mount_point) + pMountPoint->pathOff);
}

static size_t fs_mount_point_real_path_len(const fs_mount_point* pMountPoint)
{
    FS_ASSERT(pMountPoint != NULL);

    return pMountPoint->pathLen;
}

static const char* fs_mount_point_virtual_path(const fs_mount_point* pMountPoint)
{
    FS_ASSERT(pMountPoint != NULL);

    return (const char*)FS_OFFSET_PTR(pMountPoint, sizeof(fs_mount_point) + pMountPoint->mountPointOff);
}

static size_t fs_mount_point_virtual_path_len(const fs_mount_point* pMountPoint)
{
    FS_ASSERT(pMountPoint != NULL);

    return pMountPoint->mountPointLen;
}

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

static fs_bool32 fs_mount_list_at_end(const fs_mount_list_iterator* pIterator)
{
    FS_ASSERT(pIterator != NULL);

    return (pIterator->internal.cursor >= fs_mount_list_get_alloc_size(pIterator->internal.pList));
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

    /* We can't continue if the list is at the end or else we'll overrun the cursor. */
    if (fs_mount_list_at_end(pIterator)) {
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



static const fs_backend* fs_get_default_backend(void)
{
    /*  */ if (FS_BACKEND_POSIX != NULL) {
        return FS_BACKEND_POSIX;
    } else if (FS_BACKEND_WIN32 != NULL) {
        return FS_BACKEND_WIN32;
    } else {
        return NULL;
    }
}

static const fs_backend* fs_get_backend_or_default(const fs* pFS)
{
    if (pFS == NULL) {
        return fs_get_default_backend();
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

static fs_result fs_file_open_or_info(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile, fs_file_info* pInfo);
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

#if 0
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
#endif

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


static size_t fs_archive_type_sizeof(const fs_archive_type* pArchiveType)
{
    return FS_ALIGN(sizeof(pArchiveType->pBackend) + strlen(pArchiveType->pExtension) + 1, FS_SIZEOF_PTR);
}


typedef struct
{
    size_t      len;
    char        stack[1024];
    char*       heap;
    const char* ref;
} fs_string;

static fs_string fs_string_new(void)
{
    fs_string str;

    str.len      = 0;
    str.stack[0] = '\0';
    str.heap     = NULL;
    str.ref      = NULL;

    return str;
}

static fs_string fs_string_new_ref(const char* ref, size_t len)
{
    fs_string str = fs_string_new();
    str.ref = ref;
    str.len = len;

    if (str.len == FS_NULL_TERMINATED) {
        str.len = strlen(ref);
    }

    return str;
}

static fs_result fs_string_alloc(size_t len, const fs_allocation_callbacks* pAllocationCallbacks, fs_string* pString)
{
    *pString = fs_string_new();

    if (len < sizeof(pString->stack)) {
        pString->stack[0] = '\0';
    } else {
        pString->heap = (char*)fs_malloc(len + 1, pAllocationCallbacks);
        if (pString->heap == NULL) {
            return FS_OUT_OF_MEMORY;
        }
    
        pString->heap[0] = '\0';
    }

    return FS_SUCCESS;
}

static void fs_string_free(fs_string* pString, const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (pString == NULL) {
        return;
    }

    if (pString->heap != NULL) {
        fs_free(pString->heap, pAllocationCallbacks);
        pString->heap = NULL;
    }
}

static const char* fs_string_cstr(const fs_string* pString)
{
    FS_ASSERT(pString != NULL);

    if (pString->ref != NULL) {
        return pString->ref;
    }

    if (pString->heap != NULL) {
        return pString->heap;
    }

    return pString->stack;
}

static size_t fs_string_len(const fs_string* pString)
{
    return pString->len;
}

static void fs_string_append_preallocated(fs_string* pString, const char* pSrc, size_t srcLen)
{
    char* pDst;

    FS_ASSERT(pSrc         != NULL);
    FS_ASSERT(pString      != NULL);
    FS_ASSERT(pString->ref == NULL); /* Can't concatenate to a reference string. */

    if (srcLen == FS_NULL_TERMINATED) {
        srcLen = strlen(pSrc);
    }

    if (pString->heap != NULL) {
        pDst = pString->heap;
    } else {
        pDst = pString->stack;
    }

    FS_COPY_MEMORY(pDst + pString->len, pSrc, srcLen);
    pString->len += srcLen;
    pDst[pString->len] = '\0';
}


static const char* fs_path_trim_mount_point_base(const char* pPath, size_t pathLen, const char* pMountPoint, size_t mountPointLen)
{
    FS_ASSERT(pPath != NULL && pMountPoint != NULL);

    /*
    Special case here, and why we need to use this function instead of fs_path_trim_base() directly. For mount
    points, the "" mount is *not* considered to match with a path that starts with "/".
    */
    if (pathLen > 0 && pPath[0] == '/' && pMountPoint[0] == '\0') {
        return NULL;    /* Path starts with "/", but the mount point is "". This is not a match. */
    }

    return fs_path_trim_base(pPath, pathLen, pMountPoint, mountPointLen);
}

/*
This will return an error if the path is not prefixed with the mount point, or if it attempts to
navigate above the mount point when disallowed.
*/
static fs_result fs_resolve_sub_path_from_mount_point(fs* pFS, fs_mount_point* pMountPoint, const char* pPath, int openMode, fs_string* pResolvedSubPath)
{
    fs_result result;
    int stringLen;
    int normalizeOptions = (openMode & FS_NO_ABOVE_ROOT_NAVIGATION);
    const char* pSubPath;

    FS_ASSERT(pPath            != NULL);
    FS_ASSERT(pResolvedSubPath != NULL);

    *pResolvedSubPath = fs_string_new();

    if (pFS == NULL || pMountPoint == NULL) {
        return FS_INVALID_ARGS;
    }

    pSubPath = fs_path_trim_mount_point_base(pPath, FS_NULL_TERMINATED, fs_mount_point_virtual_path(pMountPoint), fs_mount_point_virtual_path_len(pMountPoint));
    if (pSubPath == NULL) {
        return FS_DOES_NOT_EXIST;   /* The file path does not start with this mount point. */
    }

    /* A path starting with a slash does not allow for navigating above the root. */
    if (pPath[0] == '/' || pPath[0] == '\\') {
        normalizeOptions |= FS_NO_ABOVE_ROOT_NAVIGATION;
    }

    /*
    The sub-path needs to be cleaned. This is where FS_NO_ABOVE_ROOT_NAVIGATION is validated. We can skip this
    process if special directories have been disabled since a clean path will be implied and therefore there
    should be no possibility of navigating above the root.
    */
    if ((openMode & FS_NO_SPECIAL_DIRS) == 0) {
        stringLen = fs_path_normalize(pResolvedSubPath->stack, sizeof(pResolvedSubPath->stack), pSubPath, FS_NULL_TERMINATED, normalizeOptions);
        if (stringLen < 0) {
            return FS_DOES_NOT_EXIST;    /* Most likely violating FS_NO_ABOVE_ROOT_NAVIGATION. */
        }

        pResolvedSubPath->len = (size_t)stringLen;

        if ((size_t)stringLen >= sizeof(pResolvedSubPath->stack)) {
            result = fs_string_alloc((size_t)stringLen, fs_get_allocation_callbacks(pFS), pResolvedSubPath);
            if (result != FS_SUCCESS) {
                return result;
            }

            fs_path_normalize(pResolvedSubPath->heap, pResolvedSubPath->len + 1, pSubPath, FS_NULL_TERMINATED, normalizeOptions);    /* <-- This should never fail. */
        }
    } else {
        *pResolvedSubPath = fs_string_new_ref(pSubPath, FS_NULL_TERMINATED);
    }

    return FS_SUCCESS;
}

/* This is similar to fs_resolve_sub_path_from_mount_point() but returns the real path instead of the sub-path. */
static fs_result fs_resolve_real_path_from_mount_point(fs* pFS, fs_mount_point* pMountPoint, const char* pPath, int openMode, fs_string* pResolvedRealPath)
{
    fs_result result;
    fs_string subPath;
    int stringLen;

    *pResolvedRealPath = fs_string_new();

    result = fs_resolve_sub_path_from_mount_point(pFS, pMountPoint, pPath, openMode, &subPath);
    if (result != FS_SUCCESS) {
        return result;
    }

    stringLen = fs_path_append(pResolvedRealPath->stack, sizeof(pResolvedRealPath->stack), fs_mount_point_real_path(pMountPoint), fs_mount_point_real_path_len(pMountPoint), fs_string_cstr(&subPath), fs_string_len(&subPath));
    if (stringLen < 0) {
        fs_string_free(&subPath, fs_get_allocation_callbacks(pFS));
        return FS_PATH_TOO_LONG;    /* The only error we would get here is if the path is too long. */
    }

    pResolvedRealPath->len = (size_t)stringLen;

    if ((size_t)stringLen >= sizeof(pResolvedRealPath->stack)) {
        result = fs_string_alloc((size_t)stringLen, fs_get_allocation_callbacks(pFS), pResolvedRealPath);
        if (result != FS_SUCCESS) {
            return result;
        }

        fs_path_append(pResolvedRealPath->heap, pResolvedRealPath->len + 1, fs_mount_point_real_path(pMountPoint), fs_mount_point_real_path_len(pMountPoint), fs_string_cstr(&subPath), fs_string_len(&subPath));    /* <-- This should never fail. */
    }

    /* Now that actual path has been constructed we can discard of our sub-path. */
    fs_string_free(&subPath, fs_get_allocation_callbacks(pFS));

    return FS_SUCCESS;
}

static fs_mount_point* fs_find_best_write_mount_point(fs* pFS, const char* pPath, int openMode, fs_string* pResolvedRealPath)
{
    /*
    This is a bit different from read mounts because we want to use the mount point that most closely
    matches the start of the file path. Consider, for example, the following mount points:

        - config
        - config/global

    If we're trying to open "config/global/settings.cfg" we want to use the "config/global" mount
    point, not the "config" mount point. This is because the "config/global" mount point is more
    specific and therefore more likely to be the correct one.

    We'll need to iterate over every mount point and keep track of the mount point with the longest
    prefix that matches the start of the file path.
    */
    fs_result result;
    fs_mount_list_iterator iMountPoint;
    fs_mount_point* pBestMountPoint = NULL;
    const char* pBestMountPointFileSubPath = NULL;

    for (result = fs_mount_list_first(pFS->pWriteMountPoints, &iMountPoint); result == FS_SUCCESS; result = fs_mount_list_next(&iMountPoint)) {
        const char* pFileSubPath = fs_path_trim_mount_point_base(pPath, FS_NULL_TERMINATED, iMountPoint.pMountPointPath, FS_NULL_TERMINATED);
        if (pFileSubPath == NULL) {
            continue;   /* The file path doesn't start with this mount point so skip. */
        }

        if (pBestMountPointFileSubPath == NULL || strlen(pFileSubPath) < strlen(pBestMountPointFileSubPath)) {
            pBestMountPoint = iMountPoint.internal.pMountPoint;
            pBestMountPointFileSubPath = pFileSubPath;
        }
    }

    if (pBestMountPoint == NULL) {
        return NULL;
    }

    /* At this point we have identified the best mount point. We now need to resolve the absolute path. */
    result = fs_resolve_real_path_from_mount_point(pFS, pBestMountPoint, pPath, openMode, pResolvedRealPath);
    if (result != FS_SUCCESS) {
        return NULL;    /* This probably failed because the path was trying to navigate above the mount point when not allowed to do so. */
    }

    return pBestMountPoint;
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
    fs_result result;

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
        pBackend = fs_get_default_backend();
    }

    /* If the backend is still null at this point it means the default backend has been disabled. */
    if (pBackend == NULL) {
        return FS_INVALID_ARGS;
    }

    backendDataSizeInBytes = fs_backend_alloc_size(pBackend, pConfig->pBackendConfig);

    /* We need to allocate space for the archive types which we place just after the "fs" struct. After that will be the backend data. */
    for (iArchiveType = 0; iArchiveType < pConfig->archiveTypeCount; iArchiveType += 1) {
        archiveTypesAllocSize += fs_archive_type_sizeof(&pConfig->pArchiveTypes[iArchiveType]);
    }

    pFS = (fs*)fs_calloc(sizeof(fs) + archiveTypesAllocSize + backendDataSizeInBytes, pConfig->pAllocationCallbacks);
    if (pFS == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pFS->pBackend              = pBackend;
    pFS->pStream               = pConfig->pStream; /* <-- This is allowed to be null, which will be the case for standard OS file system APIs. Streams are used for things like archives like Zip files, or in-memory file systems. */
    pFS->refCount              = 1;
    pFS->allocationCallbacks   = fs_allocation_callbacks_init_copy(pConfig->pAllocationCallbacks);
    pFS->backendDataSize       = backendDataSizeInBytes;
    pFS->onRefCountChanged     = pConfig->onRefCountChanged;
    pFS->pRefCountChangedUserData = pConfig->pRefCountChangedUserData;
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
    fs_mtx_init(&pFS->archiveLock, fs_mtx_recursive);

    /*
    We need a mutex for the reference counting. This is needed because we may have multiple threads
    opening and closing files at the same time.
    */
    fs_mtx_init(&pFS->refLock, fs_mtx_recursive);

    /* We're now ready to initialize the backend. */
    result = fs_backend_init(pBackend, pFS, pConfig->pBackendConfig, pConfig->pStream);
    if (result != FS_NOT_IMPLEMENTED) {
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
        result = FS_SUCCESS;
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

    /*
    A correct program should explicitly close their files. The reference count should be 1 when
    calling this function if the program is correct.
    */
    #if !defined(FS_ENABLE_OPENED_FILES_ASSERT)
    {
        if (fs_refcount(pFS) > 1) {
            FS_ASSERT(!"You have outstanding opened files. You must close all files before uninitializing the fs object.");    /* <-- If you hit this assert but you're absolutely sure you've closed all your files, please submit a bug report with a reproducible test case. */
        }
    }
    #endif


    fs_backend_uninit(pFS->pBackend, pFS);

    fs_free(pFS->pReadMountPoints, &pFS->allocationCallbacks);
    pFS->pReadMountPoints = NULL;

    fs_free(pFS->pWriteMountPoints, &pFS->allocationCallbacks);
    pFS->pWriteMountPoints = NULL;

    fs_free(pFS->pOpenedArchives, &pFS->allocationCallbacks);
    pFS->pOpenedArchives = NULL;

    fs_mtx_destroy(&pFS->refLock);
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

FS_API fs_result fs_remove(fs* pFS, const char* pFilePath, int options)
{
    fs_result result;
    const fs_backend* pBackend;

    if (pFilePath == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_get_backend_or_default(pFS);
    if (pBackend == NULL) {
        return FS_INVALID_ARGS;
    }

    /* If we're using the default file system, ignore mount points since there's no real notion of them. */
    if (pFS == NULL) {
        options |= FS_IGNORE_MOUNTS;
    }

    if ((options & FS_IGNORE_MOUNTS) != 0) {
        result = fs_backend_remove(pBackend, pFS, pFilePath);
    } else {
        fs_string realFilePath;
        fs_mount_point* pMountPoint;

        pMountPoint = fs_find_best_write_mount_point(pFS, pFilePath, options, &realFilePath);
        if (pMountPoint == NULL) {
            return FS_DOES_NOT_EXIST;   /* Couldn't find a mount point. */
        }

        result = fs_backend_remove(pBackend, pFS, fs_string_cstr(&realFilePath));
        fs_string_free(&realFilePath, fs_get_allocation_callbacks(pFS));
    }

    return result;
}

FS_API fs_result fs_rename(fs* pFS, const char* pOldPath, const char* pNewPath, int options)
{
    fs_result result;
    const fs_backend* pBackend;

    if (pOldPath == NULL || pNewPath == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_get_backend_or_default(pFS);
    if (pBackend == NULL) {
        return FS_INVALID_ARGS;
    }

    /* If we're using the default file system, ignore mount points since there's no real notion of them. */
    if (pFS == NULL) {
        options |= FS_IGNORE_MOUNTS;
    }

    /* If we're ignoring mounts we can just call straight into the backend. */
    if ((options & FS_IGNORE_MOUNTS) != 0) {
        result = fs_backend_rename(pBackend, pFS, pOldPath, pNewPath);
    } else {    
        fs_string realOldPath;
        fs_string realNewPath;
        fs_mount_point* pMountPointOld;
        fs_mount_point* pMountPointNew;

        pMountPointOld = fs_find_best_write_mount_point(pFS, pOldPath, options, &realOldPath);
        if (pMountPointOld == NULL) {
            return FS_DOES_NOT_EXIST;   /* Couldn't find a mount point. */
        }

        pMountPointNew = fs_find_best_write_mount_point(pFS, pNewPath, options, &realNewPath);
        if (pMountPointNew == NULL) {
            fs_string_free(&realNewPath, fs_get_allocation_callbacks(pFS));
            return FS_DOES_NOT_EXIST;   /* Couldn't find a mount point. */
        }

        result = fs_backend_rename(pBackend, pFS, fs_string_cstr(&realOldPath), fs_string_cstr(&realNewPath));
        fs_string_free(&realOldPath, fs_get_allocation_callbacks(pFS));
        fs_string_free(&realNewPath, fs_get_allocation_callbacks(pFS));
    }

    return result;
}

FS_API fs_result fs_mkdir(fs* pFS, const char* pPath, int options)
{
    fs_result result;
    char pRunningPathStack[1024];
    char* pRunningPathHeap = NULL;
    char* pRunningPath = pRunningPathStack;
    size_t runningPathLen = 0;
    fs_path_iterator iSegment;
    const fs_backend* pBackend;
    fs_mount_point* pMountPoint = NULL;
    fs_string realPath;

    pBackend = fs_get_backend_or_default(pFS);

    if (pBackend == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pPath == NULL) {
        return FS_INVALID_ARGS;
    }

    /* If we're using the default file system, ignore mount points since there's no real notion of them. */
    if (pFS == NULL) {
        options |= FS_IGNORE_MOUNTS;
    }

    /* If we're using mount points we'll want to find the best one from our input path. */
    if ((options & FS_IGNORE_MOUNTS) != 0) {
        pMountPoint = NULL;
        realPath = fs_string_new_ref(pPath, FS_NULL_TERMINATED);
    } else {
        pMountPoint = fs_find_best_write_mount_point(pFS, pPath, options, &realPath);
        if (pMountPoint == NULL) {
            return FS_DOES_NOT_EXIST;   /* Couldn't find a mount point. */
        }
    }

    /*
    Before trying to create the directory structure, just try creating it directly on the backend. If this
    fails with FS_DOES_NOT_EXIST, it means one of the parent directories does not exist. In this case we
    need to create the parent directories first, but only if FS_NO_CREATE_DIRS is not set.
    */
    result = fs_backend_mkdir(pBackend, pFS, fs_string_cstr(&realPath));
    if (result != FS_DOES_NOT_EXIST) {
        fs_string_free(&realPath, fs_get_allocation_callbacks(pFS));
        return result; /* Either success or some other error. */
    }

    /*
    Getting here means there is a missing parent directory somewhere. We'll need to try creating it by
    iterating over each segment and creating each directory. We only do this if FS_NO_CREATE_DIRS is not
    set in which case we just return FS_DOES_NOT_EXIST.
    */
    if ((options & FS_NO_CREATE_DIRS) != 0) {
        fs_string_free(&realPath, fs_get_allocation_callbacks(pFS));

        FS_ASSERT(result == FS_DOES_NOT_EXIST);
        return result;
    }

    /* We need to iterate over each segment and create the directory. If any of these fail we'll need to abort. */
    if (fs_path_first(fs_string_cstr(&realPath), FS_NULL_TERMINATED, &iSegment) != FS_SUCCESS) {
        /*
        If we get here it means the path is empty. We should actually never get here because the backend
        should have already handled this in our initial attempt at fs_backend_mkdir(). If this assert is
        getting triggered it means there's a bug in the backend.
        */
        FS_ASSERT(FS_FALSE);
        fs_string_free(&realPath, fs_get_allocation_callbacks(pFS));
        return FS_ALREADY_EXISTS;  /* It's an empty path. */
    }

    pRunningPath[0] = '\0';

    for (;;) {
        if (runningPathLen + iSegment.segmentLength + 1 + 1 >= sizeof(pRunningPathStack)) {
            if (pRunningPath == pRunningPathStack) {
                pRunningPathHeap = (char*)fs_malloc(runningPathLen + iSegment.segmentLength + 1 + 1, fs_get_allocation_callbacks(pFS));
                if (pRunningPathHeap == NULL) {
                    fs_string_free(&realPath, fs_get_allocation_callbacks(pFS));
                    return FS_OUT_OF_MEMORY;
                }

                FS_COPY_MEMORY(pRunningPathHeap, pRunningPathStack, runningPathLen);
                pRunningPath = pRunningPathHeap;
            } else {
                char* pNewRunningPathHeap;

                pNewRunningPathHeap = (char*)fs_realloc(pRunningPathHeap, runningPathLen + iSegment.segmentLength + 1 + 1, fs_get_allocation_callbacks(pFS));
                if (pNewRunningPathHeap == NULL) {
                    fs_free(pRunningPathHeap, fs_get_allocation_callbacks(pFS));
                    fs_string_free(&realPath, fs_get_allocation_callbacks(pFS));
                    return FS_OUT_OF_MEMORY;
                }

                pRunningPath = pNewRunningPathHeap;
            }
        }

        FS_COPY_MEMORY(pRunningPath + runningPathLen, iSegment.pFullPath + iSegment.segmentOffset, iSegment.segmentLength);
        runningPathLen += iSegment.segmentLength;
        pRunningPath[runningPathLen] = '\0';

        /*
        The running path might be an empty string due to the way we parse our path. For example, a path
        such as `/foo/bar` will have an empty segment before the first slash. In this case we want to
        treat the empty segment as a valid already-existing directory.
        */
        if (runningPathLen > 0) {
            result = fs_backend_mkdir(pBackend, pFS, pRunningPath);
        } else {
            result = FS_ALREADY_EXISTS;
        }

        /* We just pretend to be successful if the directory already exists. */
        if (result == FS_ALREADY_EXISTS) {
            result = FS_SUCCESS;
        }

        if (result != FS_SUCCESS) {
            fs_free(pRunningPathHeap, fs_get_allocation_callbacks(pFS));
            fs_string_free(&realPath, fs_get_allocation_callbacks(pFS));
            return result;
        }

        pRunningPath[runningPathLen] = '/';
        runningPathLen += 1;

        result = fs_path_next(&iSegment);
        if (result != FS_SUCCESS) {
            break;
        }
    }

    fs_free(pRunningPathHeap, fs_get_allocation_callbacks(pFS));
    fs_string_free(&realPath, fs_get_allocation_callbacks(pFS));

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


static void fs_on_refcount_changed(fs* pFS, fs_uint32 newRefCount, fs_uint32 oldRefCount)
{
    if (pFS->onRefCountChanged != NULL) {
        pFS->onRefCountChanged(pFS->pRefCountChangedUserData, pFS, newRefCount, oldRefCount);
    }
}

FS_API fs* fs_ref(fs* pFS)
{
    fs_uint32 newRefCount;
    fs_uint32 oldRefCount;

    if (pFS == NULL) {
        return NULL;
    }

    fs_mtx_lock(&pFS->refLock);
    {
        oldRefCount = pFS->refCount;
        newRefCount = pFS->refCount + 1;

        pFS->refCount = newRefCount;

        fs_on_refcount_changed(pFS, newRefCount, oldRefCount);
    }
    fs_mtx_unlock(&pFS->refLock);

    return pFS;
}

FS_API fs_uint32 fs_unref(fs* pFS)
{
    fs_uint32 newRefCount;
    fs_uint32 oldRefCount;

    if (pFS == NULL) {
        return 0;
    }

    if (pFS->refCount == 1) {
        #if !defined(FS_ENABLE_OPENED_FILES_ASSERT)
        {
            FS_ASSERT(!"ref/funref mismatch. Ensure all fs_ref() calls are matched with fs_unref() calls.");
        }
        #endif
        return pFS->refCount;
    }

    fs_mtx_lock(&pFS->refLock);
    {
        oldRefCount = pFS->refCount;
        newRefCount = pFS->refCount - 1;

        pFS->refCount = newRefCount;

        fs_on_refcount_changed(pFS, newRefCount, oldRefCount);
    }
    fs_mtx_unlock(&pFS->refLock);

    return newRefCount;
}

FS_API fs_uint32 fs_refcount(fs* pFS)
{
    fs_uint32 refCount;

    if (pFS == NULL) {
        return 0;
    }

    fs_mtx_lock(&pFS->refLock);
    {
        refCount = pFS->refCount;
    }
    fs_mtx_unlock(&pFS->refLock);

    return refCount;
}



static fs_result fs_find_registered_archive_type_by_path(fs* pFS, const char* pPath, size_t pathLen, const fs_backend** ppBackend, const void** ppBackendConfig)
{
    fs_result result;
    fs_registered_backend_iterator iBackend;

    if (ppBackend != NULL) {
        *ppBackend = NULL;
    }
    if (ppBackendConfig != NULL) {
        *ppBackendConfig = NULL;
    }

    for (result = fs_first_registered_backend(pFS, &iBackend); result == FS_SUCCESS; result = fs_next_registered_backend(&iBackend)) {
        if (fs_path_extension_equal(pPath, pathLen, iBackend.pExtension, iBackend.extensionLen)) {
            if (ppBackend != NULL) {
                *ppBackend = iBackend.pBackend;
            }
            if (ppBackendConfig != NULL) {
                *ppBackendConfig = iBackend.pBackendConfig;
            }
            
            return FS_SUCCESS;
        }
    }

    return FS_DOES_NOT_EXIST;
}

FS_API fs_bool32 fs_path_looks_like_archive(fs* pFS, const char* pPath, size_t pathLen)
{
    fs_result result;

    if (pFS == NULL || pPath == NULL || pathLen == 0 || pPath[0] == '\0') {
        return FS_FALSE;
    }

    result = fs_find_registered_archive_type_by_path(pFS, pPath, pathLen, NULL, NULL);
    if (result == FS_SUCCESS) {
        return FS_TRUE;
    }

    return FS_FALSE;
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
        for (result = fs_first_registered_backend(pFS, &iBackend); result == FS_SUCCESS; result = fs_next_registered_backend(&iBackend)) {
            if (fs_path_extension_equal(iFilePathSeg.pFullPath + iFilePathSeg.segmentOffset, iFilePathSeg.segmentLength, iBackend.pExtension, iBackend.extensionLen)) {
                const fs_backend* pBackend = iBackend.pBackend;
                const void* pBackendConfig = iBackend.pBackendConfig;

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

                    result = fs_open_archive_ex(pFS, pBackend, pBackendConfig, iFilePathSeg.pFullPath, iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength, FS_NO_INCREMENT_REFCOUNT | FS_OPAQUE | openMode, &pArchive);
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
                        if (fs_refcount(pArchive) == 1) { fs_gc_archives(pFS, FS_GC_POLICY_THRESHOLD); }
                        return result;
                    }

                    if (ppFile == NULL) {
                        /* We were only grabbing file info. We can garbage collect the archive straight away if necessary. */
                        if (fs_refcount(pArchive) == 1) { fs_gc_archives(pFS, FS_GC_POLICY_THRESHOLD); }
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
                for (result = fs_first_registered_backend(pFS, &iBackend); result == FS_SUCCESS; result = fs_next_registered_backend(&iBackend)) {
                    if (fs_path_extension_equal(pIterator->pName, pIterator->nameLen, iBackend.pExtension, iBackend.extensionLen)) {
                        /* Looks like an archive. We can load this one up and try opening from it. */
                        const fs_backend* pBackend = iBackend.pBackend;
                        const void* pBackendConfig = iBackend.pBackendConfig;
                        fs* pArchive;
                        fs_string archivePath;

                        result = fs_string_alloc(iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength + 1 + pIterator->nameLen, fs_get_allocation_callbacks(pFS), &archivePath);
                        if (result != FS_SUCCESS) {
                            fs_backend_free_iterator(fs_get_backend_or_default(pFS), pIterator);
                            return result;
                        }

                        fs_string_append_preallocated(&archivePath, iFilePathSeg.pFullPath, iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength);
                        fs_string_append_preallocated(&archivePath, "/", FS_NULL_TERMINATED);
                        fs_string_append_preallocated(&archivePath, pIterator->pName, pIterator->nameLen);

                        /* At this point we've constructed the archive name and we can now open it. */
                        result = fs_open_archive_ex(pFS, pBackend, pBackendConfig, fs_string_cstr(&archivePath), FS_NULL_TERMINATED, FS_NO_INCREMENT_REFCOUNT | FS_OPAQUE | openMode, &pArchive);
                        fs_string_free(&archivePath, fs_get_allocation_callbacks(pFS));

                        if (result != FS_SUCCESS) { /* <-- This is checking the result of fs_open_archive_ex(). */
                            continue;   /* Failed to open this archive. Keep looking. */
                        }

                        /*
                        Getting here means we've successfully opened the archive. We can now try opening the file
                        from there. The path we load from will be the next segment in the path.
                        */
                        result = fs_file_open_or_info(pArchive, iFilePathSeg.pFullPath + iFilePathSeg.segmentOffset + iFilePathSeg.segmentLength + 1, openMode, ppFile, pInfo);  /* +1 to skip the separator. */
                        if (result != FS_SUCCESS) {
                            if (fs_refcount(pArchive) == 1) { fs_gc_archives(pFS, FS_GC_POLICY_THRESHOLD); }
                            continue;  /* Failed to open the file. Keep looking. */
                        }

                        /* The iterator is no longer required. */
                        fs_backend_free_iterator(fs_get_backend_or_default(pFS), pIterator);
                        pIterator = NULL;

                        if (ppFile == NULL) {
                            /* We were only grabbing file info. We can garbage collect the archive straight away if necessary. */
                            if (fs_refcount(pArchive) == 1) { fs_gc_archives(pFS, FS_GC_POLICY_THRESHOLD); }
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

static void fs_file_free(fs_file** ppFile)
{
    fs_file* pFile;

    if (ppFile == NULL) {
        return;
    }

    pFile = *ppFile;
    if (pFile == NULL) {
        return;
    }

    fs_unref(pFile->pFS);
    fs_free(pFile, fs_get_allocation_callbacks(pFile->pFS));

    *ppFile = NULL;
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

    /* The reference count of the fs object needs to be incremented. It'll be decremented in fs_file_free(). */
    fs_ref(pFS);

    *ppFile = pFile;
    return FS_SUCCESS;
}

static fs_result fs_file_alloc_and_open_or_info(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile, fs_file_info* pInfo)
{
    fs_result result;
    const fs_backend* pBackend;
    fs_bool32 isStandardIOFile;

    pBackend = fs_get_backend_or_default(pFS);
    if (pBackend == NULL) {
        return FS_INVALID_ARGS;
    }

    if (ppFile != NULL) {
        FS_ASSERT(*ppFile == NULL);

        result = fs_file_alloc(pFS, ppFile);
        if (result != FS_SUCCESS) {
            return result;
        }
    }

    /*
    Take a copy of the file system's stream if necessary. We only need to do this if we're opening the file, and if
    the owner `fs` object `pFS` itself has a stream.
    */
    if (pFS != NULL && ppFile != NULL && pFS->pStream != NULL) {
        result = fs_stream_duplicate(pFS->pStream, fs_get_allocation_callbacks(pFS), &(*ppFile)->pStreamForBackend);
        if (result != FS_SUCCESS) {
            fs_file_free(ppFile);
            return result;
        }
    }

    isStandardIOFile = (pFilePath == FS_STDIN || pFilePath == FS_STDOUT || pFilePath == FS_STDERR);

    /*
    This is the lowest level opening function. We never want to look at mounts when opening from here. The input
    file path should already be prefixed with the mount point.

    UPDATE: Actually don't want to explicitly append FS_IGNORE_MOUNTS here because it can affect the behavior of
    passthrough style backends. Some backends, particularly FS_SUB, will call straight into the owner `fs` object
    which might depend on those mounts being handled for correct behaviour.
    */
    /*openMode |= FS_IGNORE_MOUNTS;*/

    if (ppFile != NULL) {
        /* Create the directory structure if necessary. */
        if ((openMode & FS_WRITE) != 0 && (openMode & FS_NO_CREATE_DIRS) == 0 && !isStandardIOFile) {
            char pDirPathStack[1024];
            char* pDirPathHeap = NULL;
            char* pDirPath;
            int dirPathLen;

            dirPathLen = fs_path_directory(pDirPathStack, sizeof(pDirPathStack), pFilePath, FS_NULL_TERMINATED);
            if (dirPathLen >= (int)sizeof(pDirPathStack)) {
                pDirPathHeap = (char*)fs_malloc(dirPathLen + 1, fs_get_allocation_callbacks(pFS));
                if (pDirPathHeap == NULL) {
                    fs_stream_delete_duplicate((*ppFile)->pStreamForBackend, fs_get_allocation_callbacks(pFS));
                    fs_file_free(ppFile);
                    return FS_OUT_OF_MEMORY;
                }

                dirPathLen = fs_path_directory(pDirPathHeap, dirPathLen + 1, pFilePath, FS_NULL_TERMINATED);
                if (dirPathLen < 0) {
                    fs_stream_delete_duplicate((*ppFile)->pStreamForBackend, fs_get_allocation_callbacks(pFS));
                    fs_file_free(ppFile);
                    fs_free(pDirPathHeap, fs_get_allocation_callbacks(pFS));
                    return FS_ERROR;
                }

                pDirPath = pDirPathHeap;
            } else {
                pDirPath = pDirPathStack;
            }

            result = fs_mkdir(pFS, pDirPath, FS_IGNORE_MOUNTS);
            if (result != FS_SUCCESS && result != FS_ALREADY_EXISTS) {
                fs_stream_delete_duplicate((*ppFile)->pStreamForBackend, fs_get_allocation_callbacks(pFS));
                fs_file_free(ppFile);
                return result;
            }
        }

        result = fs_backend_file_open(pBackend, pFS, (*ppFile)->pStreamForBackend, pFilePath, openMode, *ppFile);

        if (result != FS_SUCCESS) {
            fs_stream_delete_duplicate((*ppFile)->pStreamForBackend, fs_get_allocation_callbacks(pFS));
            fs_file_free(ppFile);
            goto try_opening_from_archive;
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

try_opening_from_archive:
    if (!FS_IS_OPAQUE(openMode) && (openMode & FS_WRITE) == 0 && !isStandardIOFile) {
        /*
        If we failed to open the file because it doesn't exist we need to try loading it from an
        archive. We can only do this if the file is being loaded by an explicitly initialized fs
        object.
        */
        if (pFS != NULL && (result == FS_DOES_NOT_EXIST || result == FS_NOT_DIRECTORY)) {
            fs_file_free(ppFile);
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

static fs_result fs_file_open_or_info(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile, fs_file_info* pInfo)
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

    /* Special case for standard IO files. */
    if (pFilePath == FS_STDIN || pFilePath == FS_STDOUT || pFilePath == FS_STDERR) {
        return fs_file_alloc_and_open_or_info(pFS, pFilePath, openMode, ppFile, pInfo);
    }

    result = fs_validate_path(pFilePath, FS_NULL_TERMINATED, openMode);
    if (result != FS_SUCCESS) {
        return result;
    }

    if ((openMode & FS_WRITE) != 0) {
        /* Opening in write mode. */
        if (pFS != NULL && (openMode & FS_IGNORE_MOUNTS) == 0) {
            fs_mount_point* pBestMountPoint = NULL;
            fs_string fileRealPath;

            pBestMountPoint = fs_find_best_write_mount_point(pFS, pFilePath, openMode, &fileRealPath);
            if (pBestMountPoint != NULL) {
                /* We now have enough information to open the file. */
                result = fs_file_alloc_and_open_or_info(pFS, fs_string_cstr(&fileRealPath), openMode, ppFile, pInfo);
                fs_string_free(&fileRealPath, fs_get_allocation_callbacks(pFS));

                if (result != FS_SUCCESS) {
                    return result;
                }

                return FS_SUCCESS;
            } else {
                return FS_DOES_NOT_EXIST;   /* Couldn't find an appropriate mount point. */
            }
        } else {
            /*
            No "fs" object was supplied. Open using the default backend without using mount points. This is as if you were
            opening a file using `fopen()`.
            */
            if ((openMode & FS_ONLY_MOUNTS) == 0) {
                return fs_file_alloc_and_open_or_info(pFS, pFilePath, openMode, ppFile, pInfo);
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
                /* We need to run a slightly different code path depending on whether or not the mount point is an archive. */
                if (iMountPoint.pArchive != NULL) {
                    /* The mount point is an archive. In this case we need to grab the file's sub-path and just open that from the archive. */
                    fs_string fileSubPath;

                    result = fs_resolve_sub_path_from_mount_point(pFS, iMountPoint.internal.pMountPoint, pFilePath, openMode, &fileSubPath);
                    if (result != FS_SUCCESS) {
                        continue;   /* Not a valid mount point, or trying to navigate above the root. */
                    }

                    result = fs_file_open_or_info(iMountPoint.pArchive, fs_string_cstr(&fileSubPath), openMode, ppFile, pInfo);
                    fs_string_free(&fileSubPath, fs_get_allocation_callbacks(pFS));
                } else {
                    /* Not loading from an archive. In this case we need to resolve the real path and load the file from that. */
                    fs_string fileRealPath;

                    result = fs_resolve_real_path_from_mount_point(pFS, iMountPoint.internal.pMountPoint, pFilePath, openMode, &fileRealPath);
                    if (result != FS_SUCCESS) {
                        continue;   /* Not a valid mount point, or trying to navigate above the root. */
                    }

                    result = fs_file_alloc_and_open_or_info(pFS, fs_string_cstr(&fileRealPath), openMode, ppFile, pInfo);
                    fs_string_free(&fileRealPath, fs_get_allocation_callbacks(pFS));
                }

                if (result == FS_SUCCESS) {
                    return FS_SUCCESS;
                } else {
                    /* Failed to load from this mount point. Keep looking. */
                }
            }
        }

        /* If we get here it means we couldn't find the file from our search paths. Try opening directly. */
        if ((openMode & FS_ONLY_MOUNTS) == 0) {
            result = fs_file_alloc_and_open_or_info(pFS, pFilePath, openMode, ppFile, pInfo);
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
            fs_file_free(ppFile);
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

    fs_file_free(&pFile);
}

FS_API fs_result fs_file_read(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_result result;
    size_t bytesRead;
    const fs_backend* pBackend;

    if (pBytesRead != NULL) {
        *pBytesRead = 0;
    }

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

    /*
    If pBytesRead is null it means the caller will never be able to tell exactly how many bytes were read. In this
    case, if we didn't read the exact number of bytes that were requested we'll need to return an error.
    */
    if (pBytesRead == NULL) {
        if (bytesRead != bytesToRead) {
            return FS_ERROR;
        }
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_file_write(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_result result;
    size_t bytesWritten;
    const fs_backend* pBackend;

    if (pBytesWritten != NULL) {
        *pBytesWritten = 0;
    }

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

    /*
    As with reading, if the caller passes in null for pBytesWritten we need to return an error if
    the exact number of bytes couldn't be written.
    */
    if (pBytesWritten == NULL) {
        if (bytesWritten != bytesToWrite) {
            return FS_ERROR;
        }
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

FS_API fs_result fs_file_truncate(fs_file* pFile)
{
    const fs_backend* pBackend;

    if (pFile == NULL) {
        return FS_INVALID_ARGS;
    }

    pBackend = fs_file_get_backend(pFile);
    FS_ASSERT(pBackend != NULL);

    return fs_backend_file_truncate(pBackend, pFile);
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

        /*
        When we get here, we are iterating over the actual files in archives that are located in the
        file system `pFS`. There's no real concept of mounts here. In order to make iteration work
        as expected, we need to modify our mode flags to ensure it does not attempt to read from
        mounts.
        */
        mode |=  FS_IGNORE_MOUNTS;
        mode &= ~FS_ONLY_MOUNTS;

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

                    result = fs_open_archive_ex(pFS, iBackend.pBackend, iBackend.pBackendConfig, iDirPathSeg.pFullPath, iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength, FS_READ | (mode & ~FS_WRITE), &pArchive);
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
                            fs_string archivePath;

                            result = fs_string_alloc(iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength + 1 + pInnerIterator->nameLen, fs_get_allocation_callbacks(pFS), &archivePath);
                            if (result != FS_SUCCESS) {
                                fs_backend_free_iterator(pBackend, pInnerIterator);
                                return pIterator;
                            }

                            fs_string_append_preallocated(&archivePath, iDirPathSeg.pFullPath, iDirPathSeg.segmentOffset + iDirPathSeg.segmentLength);
                            fs_string_append_preallocated(&archivePath, "/", FS_NULL_TERMINATED);
                            fs_string_append_preallocated(&archivePath, pInnerIterator->pName, pInnerIterator->nameLen);

                            /* At this point we've constructed the archive name and we can now open it. */
                            result = fs_open_archive_ex(pFS, iBackend.pBackend, iBackend.pBackendConfig, fs_string_cstr(&archivePath), FS_NULL_TERMINATED, FS_READ | (mode & ~FS_WRITE), &pArchive);
                            fs_string_free(&archivePath, fs_get_allocation_callbacks(pFS));

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

    /* Make sure we never try using mounts if we are not using a fs object. */
    if (pFS == NULL) {
        mode |=  FS_IGNORE_MOUNTS;
        mode &= ~FS_ONLY_MOUNTS;
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
    if ((mode & FS_WRITE) != 0) {
        /* Write mode. */
        if (pFS != NULL && (mode & FS_IGNORE_MOUNTS) == 0) {
            fs_mount_point* pBestMountPoint = NULL;
            fs_string fileRealPath;

            pBestMountPoint = fs_find_best_write_mount_point(pFS, pDirectoryPath, mode, &fileRealPath);
            if (pBestMountPoint != NULL) {
                pIterator = fs_iterator_internal_gather(pIterator, pBackend, pFS, fs_string_cstr(&fileRealPath), fs_string_len(&fileRealPath), mode);
                fs_string_free(&fileRealPath, fs_get_allocation_callbacks(pFS));
            }
        } else {
            /* No "fs" object was supplied, or we're ignoring mounts. Need to gather directly from the file system. */
            if ((mode & FS_ONLY_MOUNTS) == 0) {
                pIterator = fs_iterator_internal_gather(pIterator, pBackend, pFS, pDirectoryPath, directoryPathLen, mode);
            }
        }
    } else {
        /* Read mode. */
        fs_result mountPointIerationResult;
        fs_mount_list_iterator iMountPoint;

        /* Check mount points. */
        if (pFS != NULL && (mode & FS_IGNORE_MOUNTS) == 0) {
            for (mountPointIerationResult = fs_mount_list_first(pFS->pReadMountPoints, &iMountPoint); mountPointIerationResult == FS_SUCCESS; mountPointIerationResult = fs_mount_list_next(&iMountPoint)) {
                if (iMountPoint.pArchive != NULL) {
                    fs_string dirSubPath;

                    result = fs_resolve_sub_path_from_mount_point(pFS, iMountPoint.internal.pMountPoint, pDirectoryPath, mode, &dirSubPath);
                    if (result != FS_SUCCESS) {
                        continue;
                    }

                    pBackendIterator = fs_first_ex(iMountPoint.pArchive, fs_string_cstr(&dirSubPath), fs_string_len(&dirSubPath), mode);
                    while (pBackendIterator != NULL) {
                        pIterator = fs_iterator_internal_append(pIterator, pBackendIterator, pFS, mode);
                        pBackendIterator = fs_next(pBackendIterator);
                    }

                    fs_string_free(&dirSubPath, fs_get_allocation_callbacks(pFS));
                } else {
                    fs_string dirRealPath;

                    result = fs_resolve_real_path_from_mount_point(pFS, iMountPoint.internal.pMountPoint, pDirectoryPath, mode, &dirRealPath);
                    if (result != FS_SUCCESS) {
                        continue;
                    }

                    pIterator = fs_iterator_internal_gather(pIterator, pBackend, pFS, fs_string_cstr(&dirRealPath), fs_string_len(&dirRealPath), mode);
                    fs_string_free(&dirRealPath, fs_get_allocation_callbacks(pFS));
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



static void fs_on_refcount_changed_internal(void* pUserData, fs* pFS, fs_uint32 newRefCount, fs_uint32 oldRefCount)
{
    fs* pOwnerFS;

    (void)pUserData;
    (void)pFS;
    (void)newRefCount;
    (void)oldRefCount;

    pOwnerFS = (fs*)pUserData;
    FS_ASSERT(pOwnerFS != NULL);

    if (newRefCount == 1) {
        /* In this case there are no more files referencing this archive. We'll want to do some garbage collection. */
        fs_gc_archives(pOwnerFS, FS_GC_POLICY_THRESHOLD);
    }
}

static fs_result fs_open_archive_nolock(fs* pFS, const fs_backend* pBackend, const void* pBackendConfig, const char* pArchivePath, size_t archivePathLen, int openMode, fs** ppArchive)
{
    fs_result result;
    fs* pArchive;
    fs_config archiveConfig;
    fs_file* pArchiveFile;
    char pArchivePathNTStack[1024];
    char* pArchivePathNTHeap = NULL;    /* <-- Must be initialized to null. */
    char* pArchivePathNT;
    fs_opened_archive* pOpenedArchive;

    /*
    The first thing to do is check if the archive has already been opened. If so, we just increment
    the reference count and return the already-loaded fs object.
    */
    pOpenedArchive = fs_find_opened_archive(pFS, pArchivePath, archivePathLen);
    if (pOpenedArchive != NULL) {
        pArchive = pOpenedArchive->pArchive;
    } else {
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

        archiveConfig = fs_config_init(pBackend, pBackendConfig, fs_file_get_stream(pArchiveFile));
        archiveConfig.pAllocationCallbacks = fs_get_allocation_callbacks(pFS);
        archiveConfig.onRefCountChanged = fs_on_refcount_changed_internal;
        archiveConfig.pRefCountChangedUserData = pFS;   /* The user data is always the fs object that owns this archive. */

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
    }

    FS_ASSERT(pArchive != NULL);

    *ppArchive = ((openMode & FS_NO_INCREMENT_REFCOUNT) == 0) ? fs_ref(pArchive) : pArchive;
    return FS_SUCCESS;
}

FS_API fs_result fs_open_archive_ex(fs* pFS, const fs_backend* pBackend, const void* pBackendConfig, const char* pArchivePath, size_t archivePathLen, int openMode, fs** ppArchive)
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
    fs_uint32 newRefCount;

    if (pArchive == NULL) {
        return;
    }

    /* In fs_open_archive() we incremented the reference count. Now we need to decrement it. */
    newRefCount = fs_unref(pArchive);

    /*
    If the reference count of the archive is 1 it means we don't currently have any files opened. We should
    look at garbage collecting.
    */
    if (newRefCount == 1) {
        /*
        This is a bit hacky and should probably change. When we initialized the archive in fs_open_archive() we set the user
        data of the onRefCountChanged callback to be the fs object that owns this archive. We'll just use that to fire the
        garbage collection process.
        */
        fs* pArchiveOwnerFS = (fs*)pArchive->pRefCountChangedUserData;
        FS_ASSERT(pArchiveOwnerFS != NULL);

        fs_gc(pArchiveOwnerFS, FS_GC_POLICY_FULL, pArchive);
    }
}

static void fs_gc_nolock(fs* pFS, int policy, fs* pSpecificArchive)
{
    size_t unreferencedCount = 0;
    size_t collectionCount = 0;
    size_t cursor = 0;

    FS_ASSERT(pFS != NULL);

    /*
    If we're doing a full garbage collection we need to recursively run the garbage collection process
    on opened archives. For specific GC, we only recursively collect the target archive.
    */
    if ((policy & FS_GC_POLICY_FULL) != 0) {
        /* For full GC, recursively collect all opened archives. */
        if (pSpecificArchive != NULL) {
            fs_gc_archives(pSpecificArchive, FS_GC_POLICY_FULL);
        } else {
            cursor = 0;
            while (cursor < pFS->openedArchivesSize) {
                fs_opened_archive* pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, cursor);
                FS_ASSERT(pOpenedArchive != NULL);

                fs_gc_archives(pOpenedArchive->pArchive, policy);
                cursor += FS_ALIGN(sizeof(fs*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);
            }
        }
    }

    /* Count unreferenced archives. */
    cursor = 0;
    while (cursor < pFS->openedArchivesSize) {
        fs_opened_archive* pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, cursor);

        if (fs_refcount(pOpenedArchive->pArchive) == 1) {
            unreferencedCount += 1;
        }

        /* If this is a specific GC, check if this is the target archive. */
        if (pSpecificArchive != NULL && pSpecificArchive == pOpenedArchive->pArchive) {
            if (fs_refcount(pSpecificArchive) == 1) {
                break;
            } else {
                /* Archive is still referenced, so we can't collect it. */
                return;
            }
        }

        cursor += FS_ALIGN(sizeof(fs*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);
    }

    /* Determine how many archives to collect. */
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

    /* Collect archives. */
    cursor = 0;
    while (collectionCount > 0 && cursor < pFS->openedArchivesSize) {
        fs_opened_archive* pOpenedArchive = (fs_opened_archive*)FS_OFFSET_PTR(pFS->pOpenedArchives, cursor);

        if (fs_refcount(pOpenedArchive->pArchive) == 1 && (pSpecificArchive == NULL || pSpecificArchive == pOpenedArchive->pArchive)) {
            fs_file* pArchiveFile = (fs_file*)pOpenedArchive->pArchive->pStream;
            FS_ASSERT(pArchiveFile != NULL);

            fs_uninit(pOpenedArchive->pArchive);
            fs_file_close(pArchiveFile);
            fs_remove_opened_archive(pFS, pOpenedArchive);

            collectionCount -= 1;
            /* Note that we're not advancing the cursor here because we just removed this entry. */
        } else {
            cursor += FS_ALIGN(sizeof(fs*) + sizeof(size_t) + strlen(pOpenedArchive->pPath) + 1, FS_SIZEOF_PTR);
        }
    }
}

static void fs_gc(fs* pFS, int policy, fs* pSpecificArchive)
{
    if (pFS == NULL) {
        return;
    }

    if (policy == 0 || ((policy & FS_GC_POLICY_THRESHOLD) != 0 && (policy & FS_GC_POLICY_FULL) != 0)) {
        return; /* Invalid policy. Must specify FS_GC_POLICY_THRESHOLD or FS_GC_POLICY_FULL, but not both. */
    }

    fs_mtx_lock(&pFS->archiveLock);
    {
        fs_gc_nolock(pFS, policy, pSpecificArchive);
    }
    fs_mtx_unlock(&pFS->archiveLock);
}

FS_API void fs_gc_archives(fs* pFS, int policy)
{
    fs_gc(pFS, policy, NULL);
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


static fs_result fs_unmount_read(fs* pFS, const char* pActualPath, int options)
{
    fs_result iteratorResult;
    fs_mount_list_iterator iterator;

    if (pFS == NULL || pActualPath == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_UNUSED(options);

    for (iteratorResult = fs_mount_list_first(pFS->pReadMountPoints, &iterator); iteratorResult == FS_SUCCESS && !fs_mount_list_at_end(&iterator); /*iteratorResult = fs_mount_list_next(&iterator)*/) {
        if (strcmp(pActualPath, iterator.pPath) == 0) {
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

static fs_result fs_mount_read(fs* pFS, const char* pActualPath, const char* pVirtualPath, int options)
{
    fs_result result;
    fs_mount_list_iterator iterator;
    fs_result iteratorResult;
    fs_mount_list* pMountPoints;
    fs_mount_point* pNewMountPoint;
    fs_file_info fileInfo;

    FS_ASSERT(pFS != NULL);
    FS_ASSERT(pActualPath != NULL);
    FS_ASSERT(pVirtualPath != NULL);
    FS_ASSERT((options & FS_READ) == FS_READ);

    /*
    The first thing we're going to do is check for duplicates. We allow for the same path to be mounted
    to different mount points, and different paths to be mounted to the same mount point, but we don't
    want to have any duplicates where the same path is mounted to the same mount point.
    */
    for (iteratorResult = fs_mount_list_first(pFS->pReadMountPoints, &iterator); iteratorResult == FS_SUCCESS; iteratorResult = fs_mount_list_next(&iterator)) {
        if (strcmp(pActualPath, iterator.pPath) == 0 && strcmp(pVirtualPath, iterator.pMountPointPath) == 0) {
            return FS_SUCCESS;  /* Just pretend we're successful. */
        }
    }

    /*
    Getting here means we're not mounting a duplicate so we can now add it. We'll be either adding it to
    the end of the list, or to the beginning of the list depending on the priority.
    */
    pMountPoints = fs_mount_list_alloc(pFS->pReadMountPoints, pActualPath, pVirtualPath, ((options & FS_LOWEST_PRIORITY) == FS_LOWEST_PRIORITY) ? FS_MOUNT_PRIORITY_LOWEST : FS_MOUNT_PRIORITY_HIGHEST, fs_get_allocation_callbacks(pFS), &pNewMountPoint);
    if (pMountPoints == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pNewMountPoint->pArchive = NULL;
    pNewMountPoint->closeArchiveOnUnmount = FS_FALSE;

    pFS->pReadMountPoints = pMountPoints;

    /*
    We need to determine if we're mounting a directory or an archive. If it's an archive, we need to
    open it.
    */

    /* Must use fs_backend_info() instead of fs_info() because otherwise fs_info() will attempt to read from mounts when we're in the process of trying to add one (this function). */
    result = fs_backend_info(fs_get_backend_or_default(pFS), pFS, (pActualPath[0] != '\0') ? pActualPath : ".", FS_IGNORE_MOUNTS, &fileInfo);
    if (result != FS_SUCCESS) {
        fs_unmount_read(pFS, pActualPath, options);
        return result;
    }

    /* if the path is not pointing to a directory, assume it's a file, and therefore an archive. */
    if (!fileInfo.directory) {
        result = fs_open_archive(pFS, pActualPath, FS_READ | FS_VERBOSE, &pNewMountPoint->pArchive);
        if (result != FS_SUCCESS) {
            fs_unmount_read(pFS, pActualPath, options);
            return result;
        }

        pNewMountPoint->closeArchiveOnUnmount = FS_TRUE;
    }

    return FS_SUCCESS;
}


static fs_result fs_unmount_write(fs* pFS, const char* pActualPath, int options)
{
    fs_result iteratorResult;
    fs_mount_list_iterator iterator;

    FS_ASSERT(pFS != NULL);
    FS_ASSERT(pActualPath != NULL);

    FS_UNUSED(options);

    for (iteratorResult = fs_mount_list_first(pFS->pWriteMountPoints, &iterator); iteratorResult == FS_SUCCESS && !fs_mount_list_at_end(&iterator); /*iteratorResult = fs_mount_list_next(&iterator)*/) {
        if (strcmp(pActualPath, iterator.pPath) == 0) {
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

static fs_result fs_mount_write(fs* pFS, const char* pActualPath, const char* pVirtualPath, int options)
{
    fs_result result;
    fs_mount_list_iterator iterator;
    fs_result iteratorResult;
    fs_mount_point* pNewMountPoint;
    fs_mount_list* pMountList;
    fs_file_info fileInfo;

    if (pFS == NULL || pActualPath == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pVirtualPath == NULL) {
        pVirtualPath = "";
    }

    /* Like with regular read mount points we'll want to check for duplicates. */
    for (iteratorResult = fs_mount_list_first(pFS->pWriteMountPoints, &iterator); iteratorResult == FS_SUCCESS; iteratorResult = fs_mount_list_next(&iterator)) {
        if (strcmp(pActualPath, iterator.pPath) == 0 && strcmp(pVirtualPath, iterator.pMountPointPath) == 0) {
            return FS_SUCCESS;  /* Just pretend we're successful. */
        }
    }

    /* Getting here means we're not mounting a duplicate so we can now add it. */
    pMountList = fs_mount_list_alloc(pFS->pWriteMountPoints, pActualPath, pVirtualPath, ((options & FS_LOWEST_PRIORITY) == FS_LOWEST_PRIORITY) ? FS_MOUNT_PRIORITY_LOWEST : FS_MOUNT_PRIORITY_HIGHEST, fs_get_allocation_callbacks(pFS), &pNewMountPoint);
    if (pMountList == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pFS->pWriteMountPoints = pMountList;
    
    /*
    We need to determine if we're mounting a directory or an archive. If it's an archive we need
    to fail because we do not support mounting archives in write mode.
    */
    pNewMountPoint->pArchive = NULL;
    pNewMountPoint->closeArchiveOnUnmount = FS_FALSE;

    /* Must use fs_backend_info() instead of fs_info() because otherwise fs_info() will attempt to read from mounts when we're in the process of trying to add one (this function). */
    result = fs_backend_info(fs_get_backend_or_default(pFS), pFS, (pActualPath[0] != '\0') ? pActualPath : ".", FS_IGNORE_MOUNTS, &fileInfo);
    if (result != FS_SUCCESS && result != FS_DOES_NOT_EXIST) {
        fs_unmount_write(pFS, pActualPath, options);
        return result;
    }

    if (!fileInfo.directory && result != FS_DOES_NOT_EXIST) {
        fs_unmount_write(pFS, pActualPath, options);
        return FS_INVALID_ARGS;
    }

    /* Since we'll be wanting to write out files to the mount point we should ensure the folder actually exists. */
    if ((options & FS_NO_CREATE_DIRS) == 0) {
        fs_result result = fs_mkdir(pFS, pActualPath, FS_IGNORE_MOUNTS);
        if (result != FS_SUCCESS && result != FS_ALREADY_EXISTS) {
            fs_unmount_write(pFS, pActualPath, options);
            return result;
        }
    }

    return FS_SUCCESS;
}


FS_API fs_result fs_mount(fs* pFS, const char* pActualPath, const char* pVirtualPath, int options)
{
    if (pFS == NULL || pActualPath == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pVirtualPath == NULL) {
        pVirtualPath = "";
    }

    /* At least READ or WRITE must be specified. */
    if ((options & (FS_READ | FS_WRITE)) == 0) {
        return FS_INVALID_ARGS;
    }

    if ((options & FS_WRITE) == FS_WRITE) {
        fs_result result = fs_mount_write(pFS, pActualPath, pVirtualPath, options);
        if (result != FS_SUCCESS) {
            return result;
        }
    }

    if ((options & FS_READ) == FS_READ) {
        fs_result result = fs_mount_read(pFS, pActualPath, pVirtualPath, options);
        if (result != FS_SUCCESS) {
            return result;
        }
    }

    return FS_SUCCESS;
}

FS_API fs_result fs_unmount(fs* pFS, const char* pActualPath, int options)
{
    fs_result result;

    if (pFS == NULL || pActualPath == NULL) {
        return FS_INVALID_ARGS;
    }

    if ((options & FS_READ) == FS_READ) {
        result = fs_unmount_read(pFS, pActualPath, options);
        if (result != FS_SUCCESS) {
            return result;
        }
    }

    if ((options & FS_WRITE) == FS_WRITE) {
        result = fs_unmount_write(pFS, pActualPath, options);
        if (result != FS_SUCCESS) {
            return result;
        }
    }

    return FS_SUCCESS;
}

static size_t fs_sysdir_append(fs_sysdir_type type, char* pDst, size_t dstCap, const char* pSubDir)
{
    size_t sysDirLen;
    size_t subDirLen;
    size_t totalLen;

    if (pDst == NULL || pSubDir == NULL) {
        return 0;
    }

    sysDirLen = fs_sysdir(type, pDst, dstCap);
    if (sysDirLen == 0) {
        return 0;   /* Failed to retrieve the system directory. */
    }

    subDirLen = strlen(pSubDir);

    totalLen = sysDirLen + 1 + subDirLen;   /* +1 for the separator. */
    if (totalLen < dstCap) {
        pDst[sysDirLen] = '/';
        FS_COPY_MEMORY(pDst + sysDirLen + 1, pSubDir, subDirLen);
        pDst[totalLen] = '\0';
    }

    return totalLen;
}

FS_API fs_result fs_mount_sysdir(fs* pFS, fs_sysdir_type type, const char* pSubDir, const char* pVirtualPath, int options)
{
    char  pPathToMountStack[1024];
    char* pPathToMountHeap = NULL;
    char* pPathToMount;
    size_t pathToMountLen;
    fs_result result;

    if (pFS == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pVirtualPath == NULL) {
        pVirtualPath = "";
    }

    /*
    We're enforcing a sub-directory with this function to encourage applications to use good
    practice with with directory structures.
    */
    if (pSubDir == NULL || pSubDir[0] == '\0') {
        return FS_INVALID_ARGS;
    }

    pathToMountLen = fs_sysdir_append(type, pPathToMountStack, sizeof(pPathToMountStack), pSubDir);
    if (pathToMountLen == 0) {
        return FS_ERROR;    /* Failed to retrieve the system directory. */
    }

    if (pathToMountLen < sizeof(pPathToMountStack)) {
        pPathToMount = pPathToMountStack;
    } else {
        pathToMountLen += 1;    /* +1 for the null terminator. */

        pPathToMountHeap = (char*)fs_malloc(pathToMountLen, fs_get_allocation_callbacks(pFS));
        if (pPathToMountHeap == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        fs_sysdir_append(type, pPathToMountHeap, pathToMountLen, pSubDir);
        pPathToMount = pPathToMountHeap;
    }

    /* At this point we should have the path we want to mount. Now we can do the actual mounting. */
    result = fs_mount(pFS, pPathToMount, pVirtualPath, options);
    fs_free(pPathToMountHeap, fs_get_allocation_callbacks(pFS));

    return result;
}

FS_API fs_result fs_unmount_sysdir(fs* pFS, fs_sysdir_type type, const char* pSubDir, int options)
{
    char  pPathToMountStack[1024];
    char* pPathToMountHeap = NULL;
    char* pPathToMount;
    size_t pathToMountLen;
    fs_result result;

    if (pFS == NULL) {
        return FS_INVALID_ARGS;
    }

    /*
    We're enforcing a sub-directory with this function to encourage applications to use good
    practice with with directory structures.
    */
    if (pSubDir == NULL || pSubDir[0] == '\0') {
        return FS_INVALID_ARGS;
    }

    pathToMountLen = fs_sysdir_append(type, pPathToMountStack, sizeof(pPathToMountStack), pSubDir);
    if (pathToMountLen == 0) {
        return FS_ERROR;    /* Failed to retrieve the system directory. */
    }

    if (pathToMountLen < sizeof(pPathToMountStack)) {
        pPathToMount = pPathToMountStack;
    } else {
        pathToMountLen += 1;    /* +1 for the null terminator. */

        pPathToMountHeap = (char*)fs_malloc(pathToMountLen, fs_get_allocation_callbacks(pFS));
        if (pPathToMountHeap == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        fs_sysdir_append(type, pPathToMountHeap, pathToMountLen, pSubDir);
        pPathToMount = pPathToMountHeap;
    }

    /* At this point we should have the path we want to mount. Now we can do the actual mounting. */
    result = fs_unmount(pFS, pPathToMount, options);

    fs_free(pPathToMountHeap, fs_get_allocation_callbacks(pFS));
    return result;
}

FS_API fs_result fs_mount_fs(fs* pFS, fs* pOtherFS, const char* pVirtualPath, int options)
{
    fs_result iteratorResult;
    fs_mount_list_iterator iterator;
    fs_mount_list* pMountPoints;
    fs_mount_point* pNewMountPoint;

    if (pFS == NULL || pOtherFS == NULL) {
        return FS_INVALID_ARGS;
    }

    if (pVirtualPath == NULL) {
        pVirtualPath = "";
    }

    /* We don't support write mode when mounting an FS. */
    if ((options & FS_WRITE) == FS_WRITE) {
        return FS_INVALID_ARGS;
    }

    /*
    We don't allow duplicates. An archive can be bound to multiple mount points, but we don't want to have the same
    archive mounted to the same mount point multiple times.
    */
    for (iteratorResult = fs_mount_list_first(pFS->pReadMountPoints, &iterator); iteratorResult == FS_SUCCESS; iteratorResult = fs_mount_list_next(&iterator)) {
        if (pOtherFS == iterator.pArchive && strcmp(pVirtualPath, iterator.pMountPointPath) == 0) {
            /* File system is already mounted to the virtual path. Just pretend we're successful. */
            fs_ref(pOtherFS);
            return FS_SUCCESS;
        }
    }

    /*
    Getting here means we're not mounting a duplicate so we can now add it. We'll be either adding it to
    the end of the list, or to the beginning of the list depending on the priority.
    */
    pMountPoints = fs_mount_list_alloc(pFS->pReadMountPoints, "", pVirtualPath, ((options & FS_LOWEST_PRIORITY) == FS_LOWEST_PRIORITY) ? FS_MOUNT_PRIORITY_LOWEST : FS_MOUNT_PRIORITY_HIGHEST, fs_get_allocation_callbacks(pFS), &pNewMountPoint);
    if (pMountPoints == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pFS->pReadMountPoints = pMountPoints;

    pNewMountPoint->pArchive = fs_ref(pOtherFS);
    pNewMountPoint->closeArchiveOnUnmount = FS_FALSE;

    return FS_SUCCESS;
}

FS_API fs_result fs_unmount_fs(fs* pFS, fs* pOtherFS, int options)
{
    fs_result iteratorResult;
    fs_mount_list_iterator iterator;

    if (pFS == NULL || pOtherFS == NULL) {
        return FS_INVALID_ARGS;
    }

    FS_UNUSED(options);

    for (iteratorResult = fs_mount_list_first(pFS->pReadMountPoints, &iterator); iteratorResult == FS_SUCCESS; iteratorResult = fs_mount_list_next(&iterator)) {
        if (iterator.pArchive == pOtherFS) {
            fs_mount_list_remove(pFS->pReadMountPoints, iterator.internal.pMountPoint);
            fs_unref(pOtherFS);
            return FS_SUCCESS;
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

    if (pFilePath == NULL || ppData == NULL || (pDataSize == NULL && format != FS_FORMAT_TEXT)) {
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

FS_API fs_result fs_file_open_and_write(fs* pFS, const char* pFilePath, const void* pData, size_t dataSize)
{
    fs_result result;
    fs_file* pFile;

    if (pFilePath == NULL) {
        return FS_INVALID_ARGS;
    }

    /* The data pointer can be null, but only if the data size is 0. In this case the file is just made empty which is a valid use case. */
    if (pData == NULL && dataSize > 0) {
        return FS_INVALID_ARGS;
    }

    result = fs_file_open(pFS, pFilePath, FS_WRITE | FS_TRUNCATE, &pFile);
    if (result != FS_SUCCESS) {
        return result;
    }

    if (dataSize > 0) {
        result = fs_file_write(pFile, pData, dataSize, NULL);
    }

    fs_file_close(pFile);

    return result;
}


/* BEG fs_backend_posix.c */
#if !defined(_WIN32)    /* <-- Add any platforms that lack POSIX support here. */
    #define FS_SUPPORTS_POSIX
#endif

#if !defined(FS_NO_POSIX) && defined(FS_SUPPORTS_POSIX)
    #define FS_HAS_POSIX
#endif

#if defined(FS_HAS_POSIX)

/* For 64-bit seeks. */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <string.h> /* memset() */
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>


static size_t fs_alloc_size_posix(const void* pBackendConfig)
{
    (void)pBackendConfig;
    return 0;
}

static fs_result fs_init_posix(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    (void)pFS;
    (void)pBackendConfig;
    (void)pStream;

    return FS_SUCCESS;
}

static void fs_uninit_posix(fs* pFS)
{
    (void)pFS;
}

static fs_result fs_ioctl_posix(fs* pFS, int op, void* pArg)
{
    (void)pFS;
    (void)op;
    (void)pArg;

    return FS_NOT_IMPLEMENTED;
}

static fs_result fs_remove_posix(fs* pFS, const char* pFilePath)
{
    int result = remove(pFilePath);
    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    (void)pFS;
    return FS_SUCCESS;
}

static fs_result fs_rename_posix(fs* pFS, const char* pOldPath, const char* pNewPath)
{
    int result = rename(pOldPath, pNewPath);
    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    (void)pFS;
    return FS_SUCCESS;
}

static fs_result fs_mkdir_posix(fs* pFS, const char* pPath)
{
    int result = mkdir(pPath, S_IRWXU);
    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    (void)pFS;
    return FS_SUCCESS;
}


static fs_file_info fs_file_info_from_stat_posix(struct stat* pStat)
{
    fs_file_info info;

    memset(&info, 0, sizeof(info));
    info.size             = pStat->st_size;
    info.lastAccessTime   = pStat->st_atime;
    info.lastModifiedTime = pStat->st_mtime;
    info.directory        = S_ISDIR(pStat->st_mode) != 0;
    info.symlink          = S_ISLNK(pStat->st_mode) != 0;

    return info;
}

static fs_result fs_info_posix(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    struct stat info;
    int result;

    /*  */ if (pPath == FS_STDIN ) {
        result = fstat(STDIN_FILENO,  &info);
    } else if (pPath == FS_STDOUT) {
        result = fstat(STDOUT_FILENO, &info);
    } else if (pPath == FS_STDERR) {
        result = fstat(STDERR_FILENO, &info);
    } else {
        result = stat(pPath, &info);
    }

    if (result != 0) {
        return fs_result_from_errno(errno);
    }

    *pInfo = fs_file_info_from_stat_posix(&info);

    (void)pFS;
    (void)openMode;
    return FS_SUCCESS;
}


typedef struct fs_file_posix
{
    int fd;
    fs_bool32 isStandardHandle;
    int openMode;       /* The original open mode for duplication purposes. */
    char* pFilePath;    /* A copy of the original file path for duplication purposes. */
    char  pFilePathStack[128];
    char* pFilePathHeap;
} fs_file_posix;

static size_t fs_file_alloc_size_posix(fs* pFS)
{
    (void)pFS;
    return sizeof(fs_file_posix);
}

static fs_result fs_file_open_posix(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    int fd;
    int flags = 0;
    size_t filePathLen;

    if ((openMode & FS_READ) != 0) {
        if ((openMode & FS_WRITE) != 0) {
            flags |= O_RDWR | O_CREAT;
        } else {
            flags |= O_RDONLY;
        }
    } else if ((openMode & FS_WRITE) != 0) {
        flags |= O_WRONLY | O_CREAT;
    }

    if ((openMode & FS_WRITE) != 0) {
        if ((openMode & FS_EXCLUSIVE) != 0) {
            flags |= O_EXCL;
        } else if ((openMode & FS_APPEND) != 0) {
            flags |= O_APPEND;
        } else if ((openMode & FS_TRUNCATE) != 0) {
            flags |= O_TRUNC;
        }
    }


    /* For ancient versions of Linux. */
    #if defined(O_LARGEFILE)
    flags |= O_LARGEFILE;
    #endif

    /*  */ if (pFilePath == FS_STDIN ) {
        fd = STDIN_FILENO;
        pFilePosix->isStandardHandle = FS_TRUE;
    } else if (pFilePath == FS_STDOUT) {
        fd = STDOUT_FILENO;
        pFilePosix->isStandardHandle = FS_TRUE;
    } else if (pFilePath == FS_STDERR) {
        fd = STDERR_FILENO;
        pFilePosix->isStandardHandle = FS_TRUE;
    } else {
        fd = open(pFilePath, flags, 0666);
    }

    if (fd < 0) {
        return fs_result_from_errno(errno);
    }

    pFilePosix->fd = fd;

    /*
    In order to support duplication we need to keep track of the original file path and open modes. Using dup() is
    not an option because that results in a shared read/write pointer, whereas we need them to be separate. We need
    not do this for standard handles.
    */
    if (!pFilePosix->isStandardHandle) {  
        pFilePosix->openMode = openMode;

        filePathLen = strlen(pFilePath);

        if (filePathLen < FS_COUNTOF(pFilePosix->pFilePathStack)) {
            pFilePosix->pFilePath = pFilePosix->pFilePathStack;
        } else {
            pFilePosix->pFilePathHeap = (char*)fs_malloc(filePathLen + 1, fs_get_allocation_callbacks(pFS));
            if (pFilePosix->pFilePathHeap == NULL) {
                close(fd);
                return FS_OUT_OF_MEMORY;
            }

            pFilePosix->pFilePath = pFilePosix->pFilePathHeap;
        }

        fs_strcpy(pFilePosix->pFilePath, pFilePath);
    }


    (void)pFS;
    (void)pStream;
    return FS_SUCCESS;
}

static void fs_file_close_posix(fs_file* pFile)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);

    /* No need to do anything if the file was opened from stdin, stdout, or stderr. */
    if (pFilePosix->isStandardHandle) {
        return;
    }

    close(pFilePosix->fd);
    fs_free(pFilePosix->pFilePathHeap, fs_get_allocation_callbacks(fs_file_get_fs(pFile)));
}

static fs_result fs_file_read_posix(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    ssize_t bytesRead;

    bytesRead = read(pFilePosix->fd, pDst, bytesToRead);
    if (bytesRead < 0) {
        return fs_result_from_errno(errno);
    }

    *pBytesRead = (size_t)bytesRead;

    if (*pBytesRead == 0) {
        return FS_AT_END;
    }

    return FS_SUCCESS;
}

static fs_result fs_file_write_posix(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    ssize_t bytesWritten;

    bytesWritten = write(pFilePosix->fd, pSrc, bytesToWrite);
    if (bytesWritten < 0) {
        return fs_result_from_errno(errno);
    }

    *pBytesWritten = (size_t)bytesWritten;
    return FS_SUCCESS;
}

static fs_result fs_file_seek_posix(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    int whence;
    off_t result;

    if (origin == FS_SEEK_SET) {
        whence = SEEK_SET;
    } else if (origin == FS_SEEK_END) {
        whence = SEEK_END;
    } else {
        whence = SEEK_CUR;
    }

    #if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
    {
        result = lseek(pFilePosix->fd, (off_t)offset, whence);
    }
    #else
    {
        if (offset < -2147483648 || offset > 2147483647) {
            return FS_BAD_SEEK;    /* Offset is too large. */
        }

        result = lseek(pFilePosix->fd, (off_t)(int)offset, whence);
    }
    #endif

    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}

static fs_result fs_file_tell_posix(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    fs_int64 cursor;

    cursor = (fs_int64)lseek(pFilePosix->fd, 0, SEEK_CUR);
    if (cursor < 0) {
        return fs_result_from_errno(errno);
    }

    *pCursor = cursor;
    return FS_SUCCESS;
}

static fs_result fs_file_flush_posix(fs_file* pFile)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    int result;

    result = fsync(pFilePosix->fd);
    if (result < 0) {
        return fs_result_from_errno(errno);
    }

    return FS_SUCCESS;
}


/*
The availability of ftruncate() is annoyingly tricky because it is not available with `-std=c89` unless the
application opts into it by defining _POSIX_C_SOURCE or _XOPEN_SOURCE
*/
#if !defined(FS_HAS_FTRUNCATE) && (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 500)  /* Opted in by the application via a feature macro. */
    #define FS_HAS_FTRUNCATE
#endif
#if !defined(FS_HAS_FTRUNCATE) && !defined(__STRICT_ANSI__) /* Not using strict ANSI. Assume available. Might need to massage this later. */
    #define FS_HAS_FTRUNCATE
#endif

static fs_result fs_file_truncate_posix(fs_file* pFile)
{
    #if defined(FS_HAS_FTRUNCATE)
    {
        fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
        off_t currentPos;
        
        /* Our truncation is based on the current write position. */
        currentPos = lseek(pFilePosix->fd, 0, SEEK_CUR);
        if (currentPos < 0) {
            return fs_result_from_errno(errno);
        }

        if (ftruncate(pFilePosix->fd, currentPos) < 0) {
            return fs_result_from_errno(errno);
        }

        return FS_SUCCESS;
    }
    #else
    {
        (void)pFile;
        return FS_NOT_IMPLEMENTED;
    }
    #endif
}

static fs_result fs_file_info_posix(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_posix* pFilePosix = (fs_file_posix*)fs_file_get_backend_data(pFile);
    struct stat info;

    if (fstat(pFilePosix->fd, &info) < 0) {
        return fs_result_from_errno(errno);
    }

    *pInfo = fs_file_info_from_stat_posix(&info);

    return FS_SUCCESS;
}

static fs_result fs_file_duplicate_posix(fs_file* pFile, fs_file* pDuplicate)
{
    fs_file_posix* pFilePosix      = (fs_file_posix*)fs_file_get_backend_data(pFile);
    fs_file_posix* pDuplicatePosix = (fs_file_posix*)fs_file_get_backend_data(pDuplicate);
    fs_result result;
    struct stat st1, st2;

    /* Simple case for standard handles. */
    if (pFilePosix->isStandardHandle) {
        pDuplicatePosix->fd = pFilePosix->fd;
        pDuplicatePosix->isStandardHandle = FS_TRUE;

        return FS_SUCCESS;
    }

    /*
    We cannot duplicate the handle with dup() because that will result in a shared read/write pointer. We
    need to open the file again with the same path and flags. We're not going to allow duplication of files
    that were opened in write mode.
    */
    if ((pFilePosix->openMode & FS_WRITE) != 0) {
        return FS_INVALID_OPERATION;
    }

    result = fs_file_open_posix(fs_file_get_fs(pFile), NULL, pFilePosix->pFilePath, pFilePosix->openMode, pDuplicate);
    if (result != FS_SUCCESS) {
        return result;
    }

    /* Do a quick check that it's still pointing to the same file. */
    if (fstat(pFilePosix->fd, &st1) < 0) {
        fs_file_close_posix(pDuplicate);
        return fs_result_from_errno(errno);
    }

    if (fstat(pDuplicatePosix->fd, &st2) < 0) {
        fs_file_close_posix(pDuplicate);
        return fs_result_from_errno(errno);
    }

    if (st1.st_ino != st2.st_ino || st1.st_dev != st2.st_dev) {
        fs_file_close_posix(pDuplicate);
        return FS_INVALID_OPERATION;    /* It looks like the files have changed. */
    }

    return FS_SUCCESS;
}


#define FS_POSIX_MIN_ITERATOR_ALLOCATION_SIZE 1024

typedef struct fs_iterator_posix
{
    fs_iterator iterator;
    DIR* pDir;
    char* pFullFilePath;        /* Points to the end of the structure. */
    size_t directoryPathLen;    /* The length of the directory section. */
} fs_iterator_posix;

static void fs_free_iterator_posix(fs_iterator* pIterator);

static fs_iterator* fs_first_posix(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    fs_iterator_posix* pIteratorPosix;
    struct dirent* info;
    struct stat statInfo;
    size_t fileNameLen;

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
    pIteratorPosix = (fs_iterator_posix*)fs_malloc(FS_MAX(sizeof(*pIteratorPosix) + directoryPathLen + 1, FS_POSIX_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pFS));    /* +1 for null terminator. */
    if (pIteratorPosix == NULL) {
        return NULL;
    }

    /* Point pFullFilePath to the end of structure to where the path is located. */
    pIteratorPosix->pFullFilePath = (char*)pIteratorPosix + sizeof(*pIteratorPosix);
    pIteratorPosix->directoryPathLen = directoryPathLen;

    /* We can now copy over the directory path. This will null terminate the path which will allow us to call opendir(). */
    fs_strncpy_s(pIteratorPosix->pFullFilePath, directoryPathLen + 1, pDirectoryPath, directoryPathLen);

    /* We can now open the directory. */
    pIteratorPosix->pDir = opendir(pIteratorPosix->pFullFilePath);
    if (pIteratorPosix->pDir == NULL) {
        fs_free(pIteratorPosix, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    /* We now need to get information about the first file. */
    info = readdir(pIteratorPosix->pDir);
    if (info == NULL) {
        closedir(pIteratorPosix->pDir);
        fs_free(pIteratorPosix, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    fileNameLen = strlen(info->d_name);

    /*
    Now that we have the file name we need to append it to the full file path in the iterator. To do
    this we need to reallocate the iterator to account for the length of the file name, including the
    separating slash.
    */
    {
        fs_iterator_posix* pNewIteratorPosix= (fs_iterator_posix*)fs_realloc(pIteratorPosix, FS_MAX(sizeof(*pIteratorPosix) + directoryPathLen + 1 + fileNameLen + 1, FS_POSIX_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pFS));    /* +1 for null terminator. */
        if (pNewIteratorPosix == NULL) {
            closedir(pIteratorPosix->pDir);
            fs_free(pIteratorPosix, fs_get_allocation_callbacks(pFS));
            return NULL;
        }

        pIteratorPosix = pNewIteratorPosix;
    }

    /* Memory has been allocated. Copy over the separating slash and file name. */
    pIteratorPosix->pFullFilePath = (char*)pIteratorPosix + sizeof(*pIteratorPosix);
    pIteratorPosix->pFullFilePath[directoryPathLen] = '/';
    fs_strcpy(pIteratorPosix->pFullFilePath + directoryPathLen + 1, info->d_name);

    /* The pFileName member of the base iterator needs to be set to the file name. */
    pIteratorPosix->iterator.pName   = pIteratorPosix->pFullFilePath + directoryPathLen + 1;
    pIteratorPosix->iterator.nameLen = fileNameLen;

    /* We can now get the file information. */
    if (stat(pIteratorPosix->pFullFilePath, &statInfo) != 0) {
        closedir(pIteratorPosix->pDir);
        fs_free(pIteratorPosix, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    pIteratorPosix->iterator.info = fs_file_info_from_stat_posix(&statInfo);

    return (fs_iterator*)pIteratorPosix;
}

static fs_iterator* fs_next_posix(fs_iterator* pIterator)
{
    fs_iterator_posix* pIteratorPosix = (fs_iterator_posix*)pIterator;
    struct dirent* info;
    struct stat statInfo;
    size_t fileNameLen;

    /* We need to get information about the next file. */
    info = readdir(pIteratorPosix->pDir);
    if (info == NULL) {
        fs_free_iterator_posix((fs_iterator*)pIteratorPosix);
        return NULL;    /* The end of the directory. */
    }

    fileNameLen = strlen(info->d_name);

    /* We need to reallocate the iterator to account for the new file name. */
    {
        fs_iterator_posix* pNewIteratorPosix = (fs_iterator_posix*)fs_realloc(pIteratorPosix, FS_MAX(sizeof(*pIteratorPosix) + pIteratorPosix->directoryPathLen + 1 + fileNameLen + 1, FS_POSIX_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pIterator->pFS));    /* +1 for null terminator. */
        if (pNewIteratorPosix == NULL) {
            fs_free_iterator_posix((fs_iterator*)pIteratorPosix);
            return NULL;
        }

        pIteratorPosix = pNewIteratorPosix;
    }

    /* Memory has been allocated. Copy over the file name. */
    pIteratorPosix->pFullFilePath = (char*)pIteratorPosix + sizeof(*pIteratorPosix);
    fs_strcpy(pIteratorPosix->pFullFilePath + pIteratorPosix->directoryPathLen + 1, info->d_name);

    /* The pFileName member of the base iterator needs to be set to the file name. */
    pIteratorPosix->iterator.pName   = pIteratorPosix->pFullFilePath + pIteratorPosix->directoryPathLen + 1;
    pIteratorPosix->iterator.nameLen = fileNameLen;

    /* We can now get the file information. */
    if (stat(pIteratorPosix->pFullFilePath, &statInfo) != 0) {
        fs_free_iterator_posix((fs_iterator*)pIteratorPosix);
        return NULL;
    }

    pIteratorPosix->iterator.info = fs_file_info_from_stat_posix(&statInfo);

    return (fs_iterator*)pIteratorPosix;
}

static void fs_free_iterator_posix(fs_iterator* pIterator)
{
    fs_iterator_posix* pIteratorPosix = (fs_iterator_posix*)pIterator;

    closedir(pIteratorPosix->pDir);
    fs_free(pIteratorPosix, fs_get_allocation_callbacks(pIterator->pFS));
}

static fs_backend fs_posix_backend =
{
    fs_alloc_size_posix,
    fs_init_posix,
    fs_uninit_posix,
    fs_ioctl_posix,
    fs_remove_posix,
    fs_rename_posix,
    fs_mkdir_posix,
    fs_info_posix,
    fs_file_alloc_size_posix,
    fs_file_open_posix,
    fs_file_close_posix,
    fs_file_read_posix,
    fs_file_write_posix,
    fs_file_seek_posix,
    fs_file_tell_posix,
    fs_file_flush_posix,
    fs_file_truncate_posix,
    fs_file_info_posix,
    fs_file_duplicate_posix,
    fs_first_posix,
    fs_next_posix,
    fs_free_iterator_posix
};

const fs_backend* FS_BACKEND_POSIX = &fs_posix_backend;
#else
const fs_backend* FS_BACKEND_POSIX = NULL;
#endif
/* END fs_backend_posix.c */


/* BEG fs_backend_win32.c */
#if defined(_WIN32)
    #define FS_SUPPORTS_WIN32
#endif

#if !defined(FS_NO_WIN32) && defined(FS_SUPPORTS_WIN32)
    #define FS_HAS_WIN32
#endif

#if defined(_WIN32)
#include <windows.h>

#if defined(UNICODE) || defined(_UNICODE)
#define fs_win32_char wchar_t
#else
#define fs_win32_char char
#endif

typedef struct
{
    size_t len;
    fs_win32_char* path;
    fs_win32_char  pathStack[256];
    fs_win32_char* pathHeap;
} fs_win32_path;

static void fs_win32_path_init_internal(fs_win32_path* pPath)
{
    pPath->len = 0;
    pPath->path = pPath->pathStack;
    pPath->pathStack[0] = '\0';
    pPath->pathHeap = NULL;
}

static fs_result fs_win32_path_init(fs_win32_path* pPath, const char* pPathUTF8, size_t pathUTF8Len, const fs_allocation_callbacks* pAllocationCallbacks)
{
    size_t i;

    fs_win32_path_init_internal(pPath);

    #if defined(UNICODE) || defined(_UNICODE)
    {
        int wideCharLen;
        int cbMultiByte;

        if (pathUTF8Len == (size_t)-1) {
            cbMultiByte = (int)-1;
        } else {
            cbMultiByte = (int)pathUTF8Len + 1;
        }

        wideCharLen = MultiByteToWideChar(CP_UTF8, 0, pPathUTF8, cbMultiByte, NULL, 0);
        if (wideCharLen == 0) {
            return FS_ERROR;
        }

        /* Use the stack if possible. If not, allocate on the heap. */
        if (wideCharLen <= (int)FS_COUNTOF(pPath->pathStack)) {
            pPath->path = pPath->pathStack;
        } else {
            pPath->pathHeap = (fs_win32_char*)fs_malloc(sizeof(fs_win32_char) * wideCharLen, pAllocationCallbacks);
            if (pPath->pathHeap == NULL) {
                return FS_OUT_OF_MEMORY;
            }

            pPath->path = pPath->pathHeap;
        }

        MultiByteToWideChar(CP_UTF8, 0, pPathUTF8, cbMultiByte, pPath->path, wideCharLen);
        pPath->len = wideCharLen - 1;  /* The count returned by MultiByteToWideChar() includes the null terminator, so subtract 1 to compensate. */

        /* Convert forward slashes to back slashes for compatibility. */
        for (i = 0; i < pPath->len; i += 1) {
            if (pPath->path[i] == '/') {
                pPath->path[i] = '\\';
            }
        }

        return FS_SUCCESS;
    }
    #else
    {
        /*
        Not doing any conversion here. Just assuming the path is an ANSI path. We need to copy over the string
        and convert slashes to backslashes.
        */
        if (pathUTF8Len == (size_t)-1) {
            pPath->len = strlen(pPathUTF8);
        } else {
            pPath->len = pathUTF8Len;
        }

        if (pPath->len >= sizeof(pPath->pathStack)) {
            pPath->pathHeap = (fs_win32_char*)fs_malloc(sizeof(fs_win32_char) * (pPath->len + 1), pAllocationCallbacks);
            if (pPath->pathHeap == NULL) {
                return FS_OUT_OF_MEMORY;
            }

            pPath->path = pPath->pathHeap;
        }

        fs_strcpy(pPath->path, pPathUTF8);
        for (i = 0; i < pPath->len; i += 1) {
            if (pPath->path[i] == '/') {
                pPath->path[i] = '\\';
            }
        }

        return FS_SUCCESS;
    }
    #endif
}



static fs_result fs_win32_path_append(fs_win32_path* pPath, const char* pAppendUTF8, const fs_allocation_callbacks* pAllocationCallbacks)
{
    fs_result result;
    fs_win32_path append;
    size_t newLen;

    result = fs_win32_path_init(&append, pAppendUTF8, (size_t)-1, pAllocationCallbacks);
    if (result != FS_SUCCESS) {
        return result;
    }

    newLen = pPath->len + append.len;

    if (pPath->path == pPath->pathHeap) {
        /* It's on the heap. Just realloc. */
        fs_win32_char* pNewHeap = (fs_win32_char*)fs_realloc(pPath->pathHeap, sizeof(fs_win32_char) * (newLen + 1), pAllocationCallbacks);
        if (pNewHeap == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        pPath->pathHeap = pNewHeap;
        pPath->path     = pNewHeap;
    } else {
        /* Getting here means it's on the stack. We may need to transfer to the heap. */
        if (newLen >= FS_COUNTOF(pPath->pathStack)) {
            /* There's not enough room on the stack. We need to move the string from the stack to the heap. */
            pPath->pathHeap = (fs_win32_char*)fs_malloc(sizeof(fs_win32_char) * (newLen + 1), pAllocationCallbacks);
            if (pPath->pathHeap == NULL) {
                return FS_OUT_OF_MEMORY;
            }

            memcpy(pPath->pathHeap, pPath->pathStack, sizeof(fs_win32_char) * (pPath->len + 1));
            pPath->path = pPath->pathHeap;
        } else {
            /* There's enough room on the stack. No modifications needed. */
        }
    }

    /* Now we can append. */
    memcpy(pPath->path + pPath->len, append.path, sizeof(fs_win32_char) * (append.len + 1));  /* Null terminator copied in-place. */
    pPath->len = newLen;

    return FS_SUCCESS;
}



static void fs_win32_path_uninit(fs_win32_path* pPath, const fs_allocation_callbacks* pAllocationCallbacks)
{
    if (pPath->pathHeap) {
        fs_free(pPath->pathHeap, pAllocationCallbacks);
        pPath->pathHeap = NULL;
    }
}


FS_API fs_uint64 fs_FILETIME_to_unix(const FILETIME* pFT)
{
    ULARGE_INTEGER li;

    li.HighPart = pFT->dwHighDateTime;
    li.LowPart  = pFT->dwLowDateTime;

    return (fs_uint64)(li.QuadPart / 10000000UL - ((fs_uint64)116444736UL * 100UL));   /* Convert from Windows epoch to Unix epoch. */
}

static fs_file_info fs_file_info_from_WIN32_FIND_DATA(const WIN32_FIND_DATA* pFD)
{
    fs_file_info info;

    FS_ZERO_OBJECT(&info);
    info.size             = ((fs_uint64)pFD->nFileSizeHigh << 32) | (fs_uint64)pFD->nFileSizeLow;
    info.lastModifiedTime = fs_FILETIME_to_unix(&pFD->ftLastWriteTime);
    info.lastAccessTime   = fs_FILETIME_to_unix(&pFD->ftLastAccessTime);
    info.directory        = (pFD->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    info.symlink          = ((pFD->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) && (pFD->dwReserved0 == 0xA000000C); /* <-- The use of dwReserved0 is documented for WIN32_FIND_DATA. */

    return info;
}

static fs_file_info fs_file_info_from_HANDLE_FILE_INFORMATION(const BY_HANDLE_FILE_INFORMATION* pFileInfo)
{
    fs_file_info info;

    FS_ZERO_OBJECT(&info);
    info.size             = ((fs_uint64)pFileInfo->nFileSizeHigh << 32) | (fs_uint64)pFileInfo->nFileSizeLow;
    info.lastModifiedTime = fs_FILETIME_to_unix(&pFileInfo->ftLastWriteTime);
    info.lastAccessTime   = fs_FILETIME_to_unix(&pFileInfo->ftLastAccessTime);
    info.directory        = (pFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    return info;
}


static size_t fs_alloc_size_win32(const void* pBackendConfig)
{
    (void)pBackendConfig;
    return 0;
}

static fs_result fs_init_win32(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    (void)pFS;
    (void)pBackendConfig;
    (void)pStream;

    return FS_SUCCESS;
}

static void fs_uninit_win32(fs* pFS)
{
    (void)pFS;
}

static fs_result fs_ioctl_win32(fs* pFS, int op, void* pArg)
{
    (void)pFS;
    (void)op;
    (void)pArg;

    return FS_NOT_IMPLEMENTED;
}

static fs_result fs_remove_win32(fs* pFS, const char* pFilePath)
{
    BOOL resultWin32;
    fs_result result;
    fs_win32_path path;

    result = fs_win32_path_init(&path, pFilePath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    resultWin32 = DeleteFile(path.path);
    if (resultWin32 == FS_FALSE) {
        /* It may have been a directory. */
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED || error == ERROR_FILE_NOT_FOUND) {
            DWORD attributes = GetFileAttributes(path.path);
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                resultWin32 = RemoveDirectory(path.path);
                if (resultWin32 == FS_FALSE) {
                    result = fs_result_from_GetLastError();
                    goto done;
                } else {
                    return FS_SUCCESS;
                }
            } else {
                result = fs_result_from_GetLastError();
                goto done;
            }
        } else {
            result = fs_result_from_GetLastError();
            goto done;
        }

        result = fs_result_from_GetLastError();
        goto done;
    } else {
        result = FS_SUCCESS;
    }

done:
    fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));

    (void)pFS;
    return result;
}

static fs_result fs_rename_win32(fs* pFS, const char* pOldPath, const char* pNewPath)
{
    BOOL resultWin32;
    fs_result result;
    fs_win32_path pathOld;
    fs_win32_path pathNew;

    result = fs_win32_path_init(&pathOld, pOldPath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    result = fs_win32_path_init(&pathNew, pNewPath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        fs_win32_path_uninit(&pathOld, fs_get_allocation_callbacks(pFS));
        return result;
    }

    resultWin32 = MoveFile(pathOld.path, pathNew.path);
    if (resultWin32 == FS_FALSE) {
        result = fs_result_from_GetLastError();
    }

    fs_win32_path_uninit(&pathOld, fs_get_allocation_callbacks(pFS));
    fs_win32_path_uninit(&pathNew, fs_get_allocation_callbacks(pFS));

    (void)pFS;
    return result;
}

static fs_result fs_mkdir_win32(fs* pFS, const char* pPath)
{
    BOOL resultWin32;
    fs_result result;
    fs_win32_path path;

    /* If it's a drive letter segment just pretend it's successful. */
    if ((pPath[0] >= 'a' && pPath[0] <= 'z') || (pPath[0] >= 'A' && pPath[0] <= 'Z')) {
        if (pPath[1] == ':' && pPath[2] == '\0') {
            return FS_SUCCESS;
        }
    }

    result = fs_win32_path_init(&path, pPath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    resultWin32 = CreateDirectory(path.path, NULL);
    if (resultWin32 == FS_FALSE) {
        result = fs_result_from_GetLastError();
        goto done;
    }

done:
    fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));

    (void)pFS;
    return result;
}

static fs_result fs_info_from_stdio_win32(HANDLE hFile, fs_file_info* pInfo)
{
    BY_HANDLE_FILE_INFORMATION fileInfo;

    if (GetFileInformationByHandle(hFile, &fileInfo) == FS_FALSE) {
        return fs_result_from_GetLastError();
    }

    *pInfo = fs_file_info_from_HANDLE_FILE_INFORMATION(&fileInfo);

    return FS_SUCCESS;
}

static fs_result fs_info_win32(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    HANDLE hFind;
    WIN32_FIND_DATA fd;
    fs_result result;
    fs_win32_path path;

    /* Special case for standard IO files. */
    /*  */ if (pPath == FS_STDIN ) {
        return fs_info_from_stdio_win32(GetStdHandle(STD_INPUT_HANDLE ), pInfo);
    } else if (pPath == FS_STDOUT) {
        return fs_info_from_stdio_win32(GetStdHandle(STD_OUTPUT_HANDLE), pInfo);
    } else if (pPath == FS_STDERR) {
        return fs_info_from_stdio_win32(GetStdHandle(STD_ERROR_HANDLE ), pInfo);
    }

    result = fs_win32_path_init(&path, pPath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    hFind = FindFirstFile(path.path, &fd);

    fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));

    if (hFind == INVALID_HANDLE_VALUE) {
        result = fs_result_from_GetLastError();
        goto done;
    } else {
        result = FS_SUCCESS;
    }

    FindClose(hFind);
    hFind = NULL;

    *pInfo = fs_file_info_from_WIN32_FIND_DATA(&fd);

done:
    (void)openMode;
    (void)pFS;
    return result;
}


typedef struct fs_file_win32
{
    HANDLE hFile;
    fs_bool32 isStandardHandle;
    int openMode;       /* The original open mode for duplication purposes. */
    char* pFilePath;
    char  pFilePathStack[256];
    char* pFilePathHeap;
} fs_file_win32;

static size_t fs_file_alloc_size_win32(fs* pFS)
{
    (void)pFS;
    return sizeof(fs_file_win32);
}

static fs_result fs_file_open_win32(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    fs_result result;
    fs_win32_path path;
    HANDLE hFile;
    DWORD dwDesiredAccess = 0;
    DWORD dwShareMode = 0;
    DWORD dwCreationDisposition = OPEN_EXISTING;

    /*  */ if (pFilePath == FS_STDIN ) {
        hFile = GetStdHandle(STD_INPUT_HANDLE);
        pFileWin32->isStandardHandle = FS_TRUE;
    } else if (pFilePath == FS_STDOUT) {
        hFile = GetStdHandle(STD_OUTPUT_HANDLE);
        pFileWin32->isStandardHandle = FS_TRUE;
    } else if (pFilePath == FS_STDERR) {
        hFile = GetStdHandle(STD_ERROR_HANDLE);
        pFileWin32->isStandardHandle = FS_TRUE;
    } else {
        pFileWin32->isStandardHandle = FS_FALSE;
    }

    if (pFileWin32->isStandardHandle) {
        pFileWin32->hFile = hFile;
        return FS_SUCCESS;
    }


    if ((openMode & FS_READ) != 0) {
        dwDesiredAccess      |= GENERIC_READ;
        dwShareMode          |= FILE_SHARE_READ;
        dwCreationDisposition = OPEN_EXISTING;  /* In read mode, our default is to open an existing file, and fail if it doesn't exist. This can be overwritten in the write case below. */
    }

    if ((openMode & FS_WRITE) != 0) {
        dwShareMode |= FILE_SHARE_WRITE;

        if ((openMode & FS_EXCLUSIVE) != 0) {
            dwDesiredAccess      |= GENERIC_WRITE;
            dwCreationDisposition = CREATE_NEW;
        } else if ((openMode & FS_APPEND) != 0) {
            dwDesiredAccess      |= FILE_APPEND_DATA;
            dwCreationDisposition = OPEN_ALWAYS;
        } else if ((openMode & FS_TRUNCATE) != 0) {
            dwDesiredAccess      |= GENERIC_WRITE;
            dwCreationDisposition = CREATE_ALWAYS;
        } else {
            dwDesiredAccess      |= GENERIC_WRITE;
            dwCreationDisposition = OPEN_ALWAYS;
        }
    }

    /* As an added safety check, make sure one or both of read and write was specified. */
    if (dwDesiredAccess == 0) {
        return FS_INVALID_ARGS;
    }

    result = fs_win32_path_init(&path, pFilePath, (size_t)-1, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return result;
    }

    hFile = CreateFile(path.path, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        result = fs_result_from_GetLastError();
    } else {
        result = FS_SUCCESS;
        pFileWin32->hFile = hFile;
    }

    if (result != FS_SUCCESS) {
        return result;
    }

    /* We need to keep track of the open mode for duplication purposes. */
    pFileWin32->openMode = openMode;

    /* We need to make a copy of the path for duplication purposes. */
    if (path.len < FS_COUNTOF(pFileWin32->pFilePathStack)) {
        pFileWin32->pFilePath = pFileWin32->pFilePathStack;
    } else {
        pFileWin32->pFilePathHeap = (char*)fs_malloc(path.len + 1, fs_get_allocation_callbacks(pFS));
        if (pFileWin32->pFilePathHeap == NULL) {
            result = FS_OUT_OF_MEMORY;
            if (pFileWin32->isStandardHandle == FS_FALSE) {
                CloseHandle(pFileWin32->hFile);
            }

            fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));
            return result;
        }

        pFileWin32->pFilePath = pFileWin32->pFilePathHeap;
    }

    fs_strcpy(pFileWin32->pFilePath, pFilePath);


    /* All done. */
    fs_win32_path_uninit(&path, fs_get_allocation_callbacks(pFS));

    (void)pFS;
    (void)pStream;
    return FS_SUCCESS;
}

static void fs_file_close_win32(fs_file* pFile)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);

    if (pFileWin32->isStandardHandle == FS_FALSE) {
        CloseHandle(pFileWin32->hFile);
        fs_free(pFileWin32->pFilePathHeap, fs_get_allocation_callbacks(fs_file_get_fs(pFile)));
    }
}

static fs_result fs_file_read_win32(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    BOOL resultWin32;
    size_t bytesRemaining = bytesToRead;
    char* pRunningDst = (char*)pDst;

    /*
    ReadFile() expects a DWORD for the number of bytes to read which means we'll need to run it in a loop in case
    our bytesToRead argument is larger than 4GB.
    */
    while (bytesRemaining > 0) {
        DWORD bytesToReadNow = (DWORD)FS_MIN(bytesRemaining, (size_t)0xFFFFFFFF);
        DWORD bytesReadNow;

        resultWin32 = ReadFile(pFileWin32->hFile, pRunningDst, bytesToReadNow, &bytesReadNow, NULL);
        if (resultWin32 == FS_FALSE) {
            return fs_result_from_GetLastError();
        }

        if (bytesReadNow == 0) {
            break;
        }

        bytesRemaining -= bytesReadNow;
        pRunningDst    += bytesReadNow;
    }

    *pBytesRead = bytesToRead - bytesRemaining;
    
    if (*pBytesRead == 0) {
        return FS_AT_END;
    }

    return FS_SUCCESS;
}

static fs_result fs_file_write_win32(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    BOOL resultWin32;
    size_t bytesRemaining = bytesToWrite;
    const char* pRunningSrc = (const char*)pSrc;

    /*
    WriteFile() expects a DWORD for the number of bytes to write which means we'll need to run it in a loop in case
    our bytesToWrite argument is larger than 4GB.
    */
    while (bytesRemaining > 0) {
        DWORD bytesToWriteNow = (DWORD)FS_MIN(bytesRemaining, (size_t)0xFFFFFFFF);
        DWORD bytesWrittenNow;

        resultWin32 = WriteFile(pFileWin32->hFile, pRunningSrc, bytesToWriteNow, &bytesWrittenNow, NULL);
        if (resultWin32 == FS_FALSE) {
            return fs_result_from_GetLastError();
        }

        if (bytesWrittenNow == 0) {
            break;
        }

        bytesRemaining -= bytesWrittenNow;
        pRunningSrc    += bytesWrittenNow;
    }

    *pBytesWritten = bytesToWrite - bytesRemaining;

    return FS_SUCCESS;
}

static fs_result fs_file_seek_win32(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    LARGE_INTEGER liDistanceToMove;
    DWORD dwMoveMethod;

    switch (origin) {
    case FS_SEEK_SET:
        dwMoveMethod = FILE_BEGIN;
        liDistanceToMove.QuadPart = offset;
        break;
    case FS_SEEK_CUR:
        dwMoveMethod = FILE_CURRENT;
        liDistanceToMove.QuadPart = offset;
        break;
    case FS_SEEK_END:
        dwMoveMethod = FILE_END;
        liDistanceToMove.QuadPart = offset;
        break;
    default:
        return FS_INVALID_ARGS;
    }

    /*
    Use SetFilePointer() instead of SetFilePointerEx() for compatibility with old Windows.

    Note from MSDN:

        If you do not need the high order 32-bits, this pointer must be set to NULL.
    */
    if (SetFilePointer(pFileWin32->hFile, liDistanceToMove.LowPart, (liDistanceToMove.HighPart == 0 ? NULL : &liDistanceToMove.HighPart), dwMoveMethod) == INVALID_SET_FILE_POINTER) {
        return fs_result_from_GetLastError();
    }

    return FS_SUCCESS;
}

static fs_result fs_file_tell_win32(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    LARGE_INTEGER liCursor;

    liCursor.HighPart = 0;
    liCursor.LowPart  = SetFilePointer(pFileWin32->hFile, 0, &liCursor.HighPart, FILE_CURRENT);

    if (liCursor.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        return fs_result_from_GetLastError();
    }

    *pCursor = liCursor.QuadPart;

    return FS_SUCCESS;
}

static fs_result fs_file_flush_win32(fs_file* pFile)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);

    if (FlushFileBuffers(pFileWin32->hFile) == FS_FALSE) {
        return fs_result_from_GetLastError();
    }

    return FS_SUCCESS;
}

static fs_result fs_file_truncate_win32(fs_file* pFile)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);

    if (SetEndOfFile(pFileWin32->hFile) == FS_FALSE) {
        return fs_result_from_GetLastError();
    }

    return FS_SUCCESS;
}

static fs_result fs_file_info_win32(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_win32* pFileWin32 = (fs_file_win32*)fs_file_get_backend_data(pFile);
    BY_HANDLE_FILE_INFORMATION fileInfo;

    if (GetFileInformationByHandle(pFileWin32->hFile, &fileInfo) == FS_FALSE) {
        return fs_result_from_GetLastError();
    }

    *pInfo = fs_file_info_from_HANDLE_FILE_INFORMATION(&fileInfo);

    return FS_SUCCESS;
}

static fs_result fs_file_duplicate_win32(fs_file* pFile, fs_file* pDuplicate)
{
    fs_file_win32* pFileWin32      = (fs_file_win32*)fs_file_get_backend_data(pFile);
    fs_file_win32* pDuplicateWin32 = (fs_file_win32*)fs_file_get_backend_data(pDuplicate);
    fs_result result;
    BY_HANDLE_FILE_INFORMATION info1, info2;

    if (pFileWin32->isStandardHandle) {
        pDuplicateWin32->hFile = pFileWin32->hFile;
        pDuplicateWin32->isStandardHandle = FS_TRUE;
        
        return FS_SUCCESS;
    }

    /*
    We cannot duplicate the handle because that will result in a shared read/write pointer. We need to
    open the file again with the same path and flags. We're not going to allow duplication of files
    that were opened in write mode.
    */
    if ((pFileWin32->openMode & FS_WRITE) != 0) {
        return FS_INVALID_OPERATION;
    }

    result = fs_file_open_win32(fs_file_get_fs(pFile), NULL, pFileWin32->pFilePath, pFileWin32->openMode, pDuplicate);
    if (result != FS_SUCCESS) {
        return result;
    }

    /* Now check the file information in case it got replaced with a different file from under us. */
    if (GetFileInformationByHandle(pFileWin32->hFile, &info1) == FS_FALSE) {
        fs_file_close_win32(pDuplicate);
        return fs_result_from_GetLastError();
    }
    if (GetFileInformationByHandle(pDuplicateWin32->hFile, &info2) == FS_FALSE) {
        fs_file_close_win32(pDuplicate);
        return fs_result_from_GetLastError();
    }

    if ((info1.dwVolumeSerialNumber != info2.dwVolumeSerialNumber) || (info1.nFileIndexLow != info2.nFileIndexLow) || (info1.nFileIndexHigh != info2.nFileIndexHigh)) {
        fs_file_close_win32(pDuplicate);
        return FS_INVALID_OPERATION;
    }

    return FS_SUCCESS;
}


#define FS_WIN32_MIN_ITERATOR_ALLOCATION_SIZE 1024

typedef struct fs_iterator_win32
{
    fs_iterator iterator;
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    char* pFullFilePath;        /* Points to the end of the structure. */
    size_t directoryPathLen;    /* The length of the directory section. */
} fs_iterator_win32;

static void fs_free_iterator_win32(fs_iterator* pIterator);

static fs_iterator* fs_iterator_win32_resolve(fs_iterator_win32* pIteratorWin32, fs* pFS, HANDLE hFind, const WIN32_FIND_DATA* pFD)
{
    fs_iterator_win32* pNewIteratorWin32;
    size_t allocSize;
    int nameLenIncludingNullTerminator;

    /*
    The name is stored at the end of the struct. In order to know how much memory to allocate we'll
    need to calculate the length of the name.
    */
    #if defined(UNICODE) || defined(_UNICODE)
    {
        nameLenIncludingNullTerminator = WideCharToMultiByte(CP_UTF8, 0, pFD->cFileName, -1, NULL, 0, NULL, NULL);
        if (nameLenIncludingNullTerminator == 0) {
            fs_free_iterator_win32((fs_iterator*)pIteratorWin32);
            return NULL;
        }
    }
    #else
    {
        nameLenIncludingNullTerminator = (int)strlen(pFD->cFileName) + 1;  /* +1 for the null terminator. */
    }
    #endif

    allocSize = FS_MAX(sizeof(fs_iterator_win32) + nameLenIncludingNullTerminator, FS_WIN32_MIN_ITERATOR_ALLOCATION_SIZE);    /* 1KB just to try to avoid excessive internal reallocations inside realloc(). */

    pNewIteratorWin32 = (fs_iterator_win32*)fs_realloc(pIteratorWin32, allocSize, fs_get_allocation_callbacks(pFS));
    if (pNewIteratorWin32 == NULL) {
        fs_free_iterator_win32((fs_iterator*)pIteratorWin32);
        return NULL;
    }

    pNewIteratorWin32->iterator.pFS = pFS;
    pNewIteratorWin32->hFind        = hFind;

    /* Name. */
    pNewIteratorWin32->iterator.pName   = (char*)pNewIteratorWin32 + sizeof(fs_iterator_win32);
    pNewIteratorWin32->iterator.nameLen = (size_t)nameLenIncludingNullTerminator - 1;

    #if defined(UNICODE) || defined(_UNICODE)
    {
        WideCharToMultiByte(CP_UTF8, 0, pFD->cFileName, -1, (char*)pNewIteratorWin32->iterator.pName, nameLenIncludingNullTerminator, NULL, NULL);  /* const-cast is safe here. */
    }
    #else
    {
        fs_strcpy((char*)pNewIteratorWin32->iterator.pName, pFD->cFileName);  /* const-cast is safe here. */
    }
    #endif

    /* Info. */
    pNewIteratorWin32->iterator.info = fs_file_info_from_WIN32_FIND_DATA(pFD);

    return (fs_iterator*)pNewIteratorWin32;
}

static fs_iterator* fs_first_win32(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    HANDLE hFind;
    WIN32_FIND_DATA fd;
    fs_result result;
    fs_win32_path query;

    /* An empty path means the current directory. Win32 will want us to specify "." in this case. */
    if (pDirectoryPath == NULL || pDirectoryPath[0] == '\0') {
        pDirectoryPath = ".";
        directoryPathLen = 1;
    }

    result = fs_win32_path_init(&query, pDirectoryPath, directoryPathLen, fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        return NULL;
    }

    /*
    At this point we have converted the first part of the query. Now we need to append "\*" to it. To do this
    properly, we'll first need to remove any trailing slash, if any.
    */
    if (query.len > 0 && query.path[query.len - 1] == '\\') {
        query.len -= 1;
        query.path[query.len] = '\0';
    }

    result = fs_win32_path_append(&query, "\\*", fs_get_allocation_callbacks(pFS));
    if (result != FS_SUCCESS) {
        fs_win32_path_uninit(&query, fs_get_allocation_callbacks(pFS));
        return NULL;
    }

    hFind = FindFirstFile(query.path, &fd);
    fs_win32_path_uninit(&query, fs_get_allocation_callbacks(pFS));

    if (hFind == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    return fs_iterator_win32_resolve(NULL, pFS, hFind, &fd);
}

static fs_iterator* fs_next_win32(fs_iterator* pIterator)
{
    fs_iterator_win32* pIteratorWin32 = (fs_iterator_win32*)pIterator;
    WIN32_FIND_DATA fd;

    if (!FindNextFile(pIteratorWin32->hFind, &fd)) {
        fs_free_iterator_win32(pIterator);
        return NULL;
    }

    return fs_iterator_win32_resolve(pIteratorWin32, pIterator->pFS, pIteratorWin32->hFind, &fd);
}

static void fs_free_iterator_win32(fs_iterator* pIterator)
{
    fs_iterator_win32* pIteratorWin32 = (fs_iterator_win32*)pIterator;

    FindClose(pIteratorWin32->hFind);
    fs_free(pIteratorWin32, fs_get_allocation_callbacks(pIterator->pFS));
}

static fs_backend fs_win32_backend =
{
    fs_alloc_size_win32,
    fs_init_win32,
    fs_uninit_win32,
    fs_ioctl_win32,
    fs_remove_win32,
    fs_rename_win32,
    fs_mkdir_win32,
    fs_info_win32,
    fs_file_alloc_size_win32,
    fs_file_open_win32,
    fs_file_close_win32,
    fs_file_read_win32,
    fs_file_write_win32,
    fs_file_seek_win32,
    fs_file_tell_win32,
    fs_file_flush_win32,
    fs_file_truncate_win32,
    fs_file_info_win32,
    fs_file_duplicate_win32,
    fs_first_win32,
    fs_next_win32,
    fs_free_iterator_win32
};

const fs_backend* FS_BACKEND_WIN32 = &fs_win32_backend;
#else
const fs_backend* FS_BACKEND_WIN32 = NULL;
#endif
/* END fs_backend_win32.c */
/* END fs.c */


/* BEG fs_sysdir.c */
#if defined(_WIN32)
#include <shlobj.h>

#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA 0x001C
#endif
#ifndef CSIDL_PROFILE
#define CSIDL_PROFILE       0x0028
#endif


/*
A helper for retrieving the directory containing the executable. We use this as a fall back for when
a system folder cannot be used (usually ancient versions of Windows).
*/
HRESULT fs_get_executable_directory_win32(char* pPath)
{
    DWORD result;

    result = GetModuleFileNameA(NULL, pPath, 260);
    if (result == 260) {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    fs_path_directory(pPath, 260, pPath, result);

    return ERROR_SUCCESS;
}

/*
A simple wrapper to get a folder path. Mainly used to hide away some messy compatibility workarounds
for different versions of Windows.

The `pPath` pointer must be large enough to store at least 260 characters.
*/
HRESULT fs_get_folder_path_win32(char* pPath, int nFolder)
{
    HRESULT hr;

    FS_ASSERT(pPath != NULL);

    /*
    Using SHGetSpecialFolderPath() here for compatibility with Windows 95/98. This has been deprecated
    and the successor is SHGetFolderPath(), which itself has been deprecated in favour of the Known
    Folder API.

    If something comes up and SHGetSpecialFolderPath() stops working (unlikely), we could instead try
    using SHGetFolderPath(), like this:

        SHGetFolderPathA(NULL, nFolder, NULL, SHGFP_TYPE_CURRENT, pPath);

    If that also stops working, we would need to use the Known Folder API which I'm unfamiliar with.
    */

    hr = SHGetSpecialFolderPathA(NULL, pPath, nFolder, 0);
    if (FAILED(hr)) {
        /*
        If this fails it could be because we're calling this from an old version of Windows. We'll
        check for known folder types and do a fall back.
        */
        if (nFolder == CSIDL_LOCAL_APPDATA) {
            hr = SHGetSpecialFolderPathA(NULL, pPath, CSIDL_APPDATA, 0);
            if (FAILED(hr)) {
                hr = fs_get_executable_directory_win32(pPath);
            }
        } else if (nFolder == CSIDL_PROFILE) {
            /*
            Old versions of Windows don't really have the notion of a user folder. In this case
            we'll just use the executable directory.
            */
            hr = fs_get_executable_directory_win32(pPath);
        }
    }

    return hr;
}
#else
#include <pwd.h>
#include <unistd.h> /* For getuid() */

static const char* fs_sysdir_home(void)
{
    const char* pHome;
    struct passwd* pPasswd;

    pHome = getenv("HOME");
    if (pHome != NULL) {
        return pHome;
    }

    /* Fallback to getpwuid(). */
    pPasswd = getpwuid(getuid());
    if (pPasswd != NULL) {
        return pPasswd->pw_dir;
    }

    return NULL;
}

static size_t fs_sysdir_home_subdir(const char* pSubDir, char* pDst, size_t dstCap)
{
    const char* pHome = fs_sysdir_home();
    if (pHome != NULL) {
        size_t homeLen = strlen(pHome);
        size_t subDirLen = strlen(pSubDir);
        size_t fullLength = homeLen + 1 + subDirLen;

        if (fullLength < dstCap) {
            FS_COPY_MEMORY(pDst, pHome, homeLen);
            pDst[homeLen] = '/';
            FS_COPY_MEMORY(pDst + homeLen + 1, pSubDir, subDirLen);
            pDst[fullLength] = '\0';
        }

        return fullLength;
    }

    return 0;
}
#endif

FS_API size_t fs_sysdir(fs_sysdir_type type, char* pDst, size_t dstCap)
{
    size_t fullLength = 0;

    #if defined(_WIN32)
    {
        HRESULT hr;
        char pPath[260];

        switch (type)
        {
            case FS_SYSDIR_HOME:
            {
                hr = fs_get_folder_path_win32(pPath, CSIDL_PROFILE);
                if (SUCCEEDED(hr)) {
                    fullLength = strlen(pPath);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pPath, fullLength);
                        pDst[fullLength] = '\0';
                    }
                }
            } break;

            case FS_SYSDIR_TEMP:
            {
                fullLength = GetTempPathA(sizeof(pPath), pPath);
                if (fullLength > 0) {
                    fullLength -= 1;  /* Remove the trailing slash. */

                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pPath, fullLength);
                        pDst[fullLength] = '\0';
                    }
                }
            } break;

            case FS_SYSDIR_CONFIG:
            {
                hr = fs_get_folder_path_win32(pPath, CSIDL_APPDATA);
                if (SUCCEEDED(hr)) {
                    fullLength = strlen(pPath);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pPath, fullLength);
                        pDst[fullLength] = '\0';
                    }
                }
            } break;

            case FS_SYSDIR_DATA:
            {
                hr = fs_get_folder_path_win32(pPath, CSIDL_LOCAL_APPDATA);
                if (SUCCEEDED(hr)) {
                    fullLength = strlen(pPath);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pPath, fullLength);
                        pDst[fullLength] = '\0';
                    }
                }
            } break;

            case FS_SYSDIR_CACHE:
            {
                /* There's no proper known folder for caches. We'll just use %LOCALAPPDATA%\Cache. */
                hr = fs_get_folder_path_win32(pPath, CSIDL_LOCAL_APPDATA);
                if (SUCCEEDED(hr)) {
                    const char* pCacheSuffix = "\\Cache";
                    size_t localAppDataLen = strlen(pPath);
                    size_t cacheSuffixLen = strlen(pCacheSuffix);
                    fullLength = localAppDataLen + cacheSuffixLen;

                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pPath, localAppDataLen);
                        FS_COPY_MEMORY(pDst + localAppDataLen, pCacheSuffix, cacheSuffixLen);
                        pDst[fullLength] = '\0';
                    }
                }
            } break;

            default:
            {
                FS_ASSERT(!"Unknown system directory type.");
            } break;
        }

        /* Normalize the path to use forward slashes. */
        if (pDst != NULL && fullLength < dstCap) {
            size_t i;

            for (i = 0; i < fullLength; i += 1) {
                if (pDst[i] == '\\') {
                    pDst[i] = '/';
                }
            }
        }
    }
    #else
    {
        switch (type)
        {
            case FS_SYSDIR_HOME:
            {
                const char* pHome = fs_sysdir_home();
                if (pHome != NULL) {
                    fullLength = strlen(pHome);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pHome, fullLength);
                        pDst[fullLength] = '\0';
                    }
                }
            } break;

            case FS_SYSDIR_TEMP:
            {
                const char* pTemp = getenv("TMPDIR");
                if (pTemp != NULL) {
                    fullLength = strlen(pTemp);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pTemp, fullLength);
                        pDst[fullLength] = '\0';
                    }
                } else {
                    /* Fallback to /tmp. */
                    const char* pTmp = "/tmp";
                    fullLength = strlen(pTmp);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pTmp, fullLength);
                        pDst[fullLength] = '\0';
                    }
                }
            } break;

            case FS_SYSDIR_CONFIG:
            {
                const char* pConfig = getenv("XDG_CONFIG_HOME");
                if (pConfig != NULL) {
                    fullLength = strlen(pConfig);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pConfig, fullLength);
                        pDst[fullLength] = '\0';
                    }
                } else {
                    /* Fallback to ~/.config. */
                    fullLength = fs_sysdir_home_subdir(".config", pDst, dstCap);
                }
            } break;

            case FS_SYSDIR_DATA:
            {
                const char* pData = getenv("XDG_DATA_HOME");
                if (pData != NULL) {
                    fullLength = strlen(pData);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pData, fullLength);
                        pDst[fullLength] = '\0';
                    }
                } else {
                    /* Fallback to ~/.local/share. */
                    fullLength = fs_sysdir_home_subdir(".local/share", pDst, dstCap);
                }
            } break;

            case FS_SYSDIR_CACHE:
            {
                const char* pCache = getenv("XDG_CACHE_HOME");
                if (pCache != NULL) {
                    fullLength = strlen(pCache);
                    if (pDst != NULL && fullLength < dstCap) {
                        FS_COPY_MEMORY(pDst, pCache, fullLength);
                        pDst[fullLength] = '\0';
                    }
                } else {
                    /* Fallback to ~/.cache. */
                    fullLength = fs_sysdir_home_subdir(".cache", pDst, dstCap);
                }
            } break;

            default:
            {
                FS_ASSERT(!"Unknown system directory type.");
            } break;
        }
    }
    #endif

    /* Check if there's a trailing slash, and if so, delete it. */
    if (pDst != NULL && fullLength < dstCap && fullLength > 0) {
        if (pDst[fullLength - 1] == '/' || pDst[fullLength - 1] == '\\') {
            pDst[fullLength - 1] = '\0';
        }
    }

    return fullLength;
}
/* END fs_sysdir.c */


/* BEG fs_mktmp.c */
#ifndef _WIN32

/*
We need to detect whether or not mkdtemp() and mkstemp() are available. If it's not we'll fall back to a
non-optimal implementation. These are the rules:

  - POSIX.1-2008 and later
  - glibc 2.1.91+ (when _GNU_SOURCE is defined)
  - Not available in strict C89 mode
  - Not available on some older systems
*/
#if !defined(FS_USE_MKDTEMP_FALLBACK)
    #if !defined(FS_HAS_MKDTEMP) && (defined(_GNU_SOURCE) || defined(_BSD_SOURCE))
        #define FS_HAS_MKDTEMP
    #endif
    #if !defined(FS_HAS_MKDTEMP) && defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L
        #define FS_HAS_MKDTEMP
    #endif
    #if !defined(FS_HAS_MKDTEMP) && defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700
        #define FS_HAS_MKDTEMP
    #endif
    #if !defined(FS_HAS_MKDTEMP) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L && !defined(__STRICT_ANSI__)
        #define FS_HAS_MKDTEMP
    #endif
#endif

#if !defined(FS_HAS_MKDTEMP)
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>

static int fs_get_random_bytes(unsigned char* pBytes, size_t count)
{
    int fd;
    ssize_t bytesRead;

    /* Try /dev/urandom first. */
    fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        bytesRead = read(fd, pBytes, count);
        close(fd);

        if ((size_t)bytesRead == count) {
            return 0;  /* Success. */
        }

        /* Getting here means reading failed. Fall through to the fallback case. */
    }

    /* Getting here means /dev/urandom failed. We can fall back to a simple time/pid/stack based entropy. */
    {
        unsigned long stackAddress = 0;
        unsigned long seed;
        size_t i;
        
        /* Create a seed using multiple entropy sources. */
        seed  = (unsigned long)time(NULL);
        seed ^= (unsigned long)getpid();
        seed ^= (unsigned long)(size_t)&stackAddress;  /* <-- Stack address entropy. */
        seed ^= (unsigned long)clock();

        /* LCG */
        for (i = 0; i < count; i += 1) {
            seed = seed * 1103515245U + 12345U;
            pBytes[i] = (unsigned char)(seed >> 16);
        }
        
        return 0;
    }
}

static char* fs_mkdtemp_fallback(char* pTemplate)
{
    size_t templateLen;
    char* pXXXXXX;
    int attempts;
    int i;
    
    if (pTemplate == NULL) {
        return NULL;
    }
    
    /* The template must end with XXXXXX. */
    templateLen = strlen(pTemplate);
    if (templateLen < 6) {
        return NULL;
    }
    
    pXXXXXX = pTemplate + templateLen - 6;
    for (i = 0; i < 6; i += 1) {
        if (pXXXXXX[i] != 'X') {
            return NULL; /* Last 6 characters are not "XXXXXX". */
        }
    }

    /* We can now fill out the random part and try creating the directory. */
    for (attempts = 0; attempts < 100; attempts += 1) {
        unsigned char randomBytes[6];
        
        if (fs_get_random_bytes(randomBytes, 6) != 0) {
            return NULL;
        }
        
        /* Fill out the random part using the random bytes */
        for (i = 0; i < 6; i += 1) {
            int r = randomBytes[i] % 62;
            if (r < 10) {
                pXXXXXX[i] = '0' + r;
            } else if (r < 36) {
                pXXXXXX[i] = 'A' + (r - 10);
            } else {
                pXXXXXX[i] = 'a' + (r - 36);
            }
        }

        /* With the random part filled out we're now ready to try creating the directory */
        if (mkdir(pTemplate, S_IRWXU) == 0) {
            return pTemplate; /* Success */
        }

        /*
        Getting here means we failed to create the directory. If it's because the directory already exists (EEXIST)
        we can just continue iterating. Otherwise it was an unexpected error and we need to bomb out.
        */
        if (errno != EEXIST) {
            return NULL;
        }
    }
    
    /* Getting here means we failed after several attempts. */
    return NULL;
}

static int fs_mkstemp_fallback(char* pTemplate)
{
    size_t templateLen;
    char* pXXXXXX;
    int attempts;
    int i;
    int fd;
    
    if (pTemplate == NULL) {
        return -1;
    }
    
    /* The template must end with XXXXXX. */
    templateLen = strlen(pTemplate);
    if (templateLen < 6) {
        return -1;
    }
    
    pXXXXXX = pTemplate + templateLen - 6;
    for (i = 0; i < 6; i += 1) {
        if (pXXXXXX[i] != 'X') {
            return -1; /* Last 6 characters are not "XXXXXX". */
        }
    }

    /* We can now fill out the random part and try creating the file. */
    for (attempts = 0; attempts < 100; attempts += 1) {
        unsigned char randomBytes[6];
        
        if (fs_get_random_bytes(randomBytes, 6) != 0) {
            return -1;
        }
        
        /* Fill out the random part using the random bytes */
        for (i = 0; i < 6; i += 1) {
            int r = randomBytes[i] % 62;
            if (r < 10) {
                pXXXXXX[i] = '0' + r;
            } else if (r < 36) {
                pXXXXXX[i] = 'A' + (r - 10);
            } else {
                pXXXXXX[i] = 'a' + (r - 36);
            }
        }

        /* With the random part filled out we're now ready to try creating the file */
        fd = open(pTemplate, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
        if (fd >= 0) {
            return fd; /* Success */
        }

        /*
        Getting here means we failed to create the file. If it's because the file already exists (EEXIST)
        we can just continue iterating. Otherwise it was an unexpected error and we need to bomb out.
        */
        if (errno != EEXIST) {
            return -1;
        }
    }
    
    /* Getting here means we failed after several attempts. */
    return -1;
}
#endif

static char* fs_mkdtemp(char* pTemplate)
{
    if (pTemplate == NULL) {
        return NULL;
    }
    
    #if defined(FS_HAS_MKDTEMP)
    {
        return mkdtemp(pTemplate);
    }
    #else
    {
        return fs_mkdtemp_fallback(pTemplate);
    }
    #endif
}

static int fs_mkstemp(char* pTemplate)
{
    if (pTemplate == NULL) {
        return -1;
    }
    
    #if defined(FS_HAS_MKDTEMP)
    {
        return mkstemp(pTemplate);
    }
    #else
    {
        return fs_mkstemp_fallback(pTemplate);
    }
    #endif
}
#endif

FS_API fs_result fs_mktmp(const char* pPrefix, char* pTmpPath, size_t tmpPathCap, int options)
{
    size_t baseDirLen;
    const char* pPrefixName;
    const char* pPrefixDir;
    size_t prefixDirLen;

    if (pTmpPath == NULL) {
        return FS_INVALID_ARGS;
    }

    pTmpPath[0] = '\0';  /* Safety. */

    if (tmpPathCap == 0) {
        return FS_INVALID_ARGS;
    }

    if (pPrefix == NULL) {
        pPrefix = "";
    }

    if (pPrefix[0] == '\0') {
        pPrefix = "fs";
    }

    /* The caller must explicitly specify whether or not a file or directory is being created. */
    if ((options & (FS_MKTMP_DIR | FS_MKTMP_FILE)) == 0) {
        return FS_INVALID_ARGS;
    }

    /* It's not allowed for both DIR and FILE to be set. */
    if ((options & FS_MKTMP_DIR) != 0 && (options & FS_MKTMP_FILE) != 0) {
        return FS_INVALID_ARGS;
    }

    /* The prefix is not allowed to have any ".." segments and cannot start with "/". */
    if (strstr(pPrefix, "..") != NULL || pPrefix[0] == '/') {
        return FS_INVALID_ARGS;
    }

    /* We first need to grab the directory of the system's base temp directory. */
    baseDirLen = fs_sysdir(FS_SYSDIR_TEMP, pTmpPath, tmpPathCap);
    if (baseDirLen == 0) {
        return FS_ERROR;    /* Failed to retrieve the base temp directory. Cannot create a temp file. */
    }

    /* Now we need to append the directory part of the prefix. */
    pPrefixName = fs_path_file_name(pPrefix, FS_NULL_TERMINATED);
    FS_ASSERT(pPrefixName != NULL);

    if (pPrefixName == pPrefix) {
        /* No directory. */
        pPrefixDir = "";
        prefixDirLen = 0;
    } else {
        /* We have a directory. */
        pPrefixDir = pPrefix;
        prefixDirLen = (size_t)(pPrefixName - pPrefix);
        prefixDirLen -= 1; /* Remove the trailing slash from the prefix directory. */
    }

    if (prefixDirLen > 0) {
        if (fs_strcat_s(pTmpPath, tmpPathCap, "/") != 0) {
            return FS_PATH_TOO_LONG;
        }
    }

    if (fs_strncat_s(pTmpPath, tmpPathCap, pPrefixDir, prefixDirLen) != 0) {
        return FS_PATH_TOO_LONG;
    }

    /* Create the directory structure if necessary. */
    if ((options & FS_NO_CREATE_DIRS) == 0) {
        fs_mkdir(NULL, pTmpPath, FS_IGNORE_MOUNTS);
    }

    /* Now we can append the between the directory part and the name part. */
    if (fs_strcat_s(pTmpPath, tmpPathCap, "/") != 0) {
        return FS_PATH_TOO_LONG;
    }

    /* We're now ready for the platform specific part. */
    #if defined(_WIN32)
    {
        /*
        We're using GetTempFileName(). This is annoying because of two things. First, it requires that
        path separators be backslashes. Second, it does not take a capacity parameter so we need to
        ensure the output buffer is at least MAX_PATH (260) bytes long.
        */
        char pTmpPathWin[MAX_PATH];
        size_t i;

        for (i = 0; pTmpPath[i] != '\0'; i += 1) {
            if (pTmpPath[i] == '/') {
                pTmpPath[i] = '\\';
            }
        }

        if (GetTempFileNameA(pTmpPath, pPrefixName, 0, pTmpPathWin) == 0) {
            return fs_result_from_GetLastError();
        }

        /*
        NOTE: At this point the operating system will have created the file. If any error occurs from here
        we need to remember to delete it.
        */

        if (fs_strcpy_s(pTmpPath, tmpPathCap, pTmpPathWin) != 0) {
            DeleteFileA(pTmpPathWin);
            return FS_PATH_TOO_LONG;
        }

        /*
        If we're creating a folder the process is to delete the file that the OS just created and create a new
        folder in it's place.
        */
        if ((options & FS_MKTMP_DIR) != 0) {
            /* We're creating a temp directory. Delete the file and create a folder in it's place. */
            DeleteFileA(pTmpPathWin);

            if (CreateDirectoryA(pTmpPathWin, NULL) == 0) {
                return fs_result_from_GetLastError();
            }
        } else {
            /* We're creating a temp file. The OS will have already created the file in GetTempFileNameA() so no need to create it explicitly. */
        }

        /* Finally we need to convert our back slashes to forward slashes. */
        for (i = 0; pTmpPath[i] != '\0'; i += 1) {
            if (pTmpPath[i] == '\\') {
                pTmpPath[i] = '/';
            }
        }
    }
    #else
    {
        /* Append the file name part. */
        if (fs_strcat_s(pTmpPath, tmpPathCap, pPrefixName) != 0) {
            return FS_PATH_TOO_LONG;
        }

        /* Append the random part. */
        if (fs_strcat_s(pTmpPath, tmpPathCap, "XXXXXX") != 0) {
            return FS_PATH_TOO_LONG;
        }

        /* At this point the full path has been constructed. We can now create the file or directory. */
        if ((options & FS_MKTMP_DIR) != 0) {
            /* We're creating a temp directory. */
            if (fs_mkdtemp(pTmpPath) == NULL) {
                return fs_result_from_errno(errno);
            }
        } else {
            /* We're creating a temp file. */
            int fd = fs_mkstemp(pTmpPath);
            if (fd == -1) {
                return fs_result_from_errno(errno);
            }

            close(fd);
        }
    }
    #endif

    return FS_SUCCESS;
}
/* END fs_mktmp.c */



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

FS_API int fs_path_compare(const char* pPathA, size_t pathALen, const char* pPathB, size_t pathBLen)
{
    fs_path_iterator iPathA;
    fs_path_iterator iPathB;
    fs_result result;

    if (pPathA == NULL && pPathB == NULL) {
        return 0;
    }

    if (pPathA == NULL) {
        return -1;
    }
    if (pPathB == NULL) {
        return +1;
    }

    result = fs_path_first(pPathA, pathALen, &iPathA);
    if (result != FS_SUCCESS) {
        return -1;
    }

    result = fs_path_first(pPathB, pathBLen, &iPathB);
    if (result != FS_SUCCESS) {
        return +1;
    }

    /* We just keep iterating until we find a mismatch or reach the end of one of the paths. */
    for (;;) {
        int cmp;

        cmp = fs_path_iterators_compare(&iPathA, &iPathB);
        if (cmp != 0) {
            return cmp;
        }

        if (fs_path_is_last(&iPathA) && fs_path_is_last(&iPathB)) {
            return 0;   /* Both paths are the same. */
        }

        result = fs_path_next(&iPathA);
        if (result != FS_SUCCESS) {
            return -1;
        }

        result = fs_path_next(&iPathB);
        if (result != FS_SUCCESS) {
            return +1;
        }
    }

    return 0;
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
            if (bytesToCopy > 0 && pDst != pPath) {
                FS_MOVE_MEMORY(pDst, pPath, bytesToCopy);
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
        if (iPath.segmentLength != iBase.segmentLength || fs_strncmp(iPath.pFullPath + iPath.segmentOffset, iBase.pFullPath + iBase.segmentOffset, iPath.segmentLength) != 0) {
            return NULL;
        }

        result = fs_path_next(&iBase);
        if (result != FS_SUCCESS || (iBase.segmentLength == 0 && fs_path_is_last(&iBase))) {
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

FS_API fs_bool32 fs_path_begins_with(const char* pPath, size_t pathLen, const char* pBasePath, size_t basePathLen)
{
    return fs_path_trim_base(pPath, pathLen, pBasePath, basePathLen) != NULL;
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
                    FS_COPY_MEMORY(pDst, pBasePath, bytesToCopy);
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
        }

        pDst   += bytesToCopy;
        dstCap -= bytesToCopy;
    }
    dstLen += pathToAppendLen;


    /* Null terminator. */
    if (pDst != NULL) {
        if (dstCap > 0) {
            pDst[0] = '\0';
        }
    }


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

#if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic push
    #if defined(__clang__)
        #pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
    #elif defined(__GNUC__)
        #pragma GCC diagnostic ignored "-Wtype-limits"
    #endif
#endif
    if (cursor > FS_INT64_MAX) {
        return FS_ERROR;
    }
#if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

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
    fs_int64 newCursor;

    if (pStream == NULL) {
        return FS_INVALID_ARGS;
    }

    if ((fs_uint64)FS_ABS(offset) > FS_SIZE_MAX) {
        return FS_INVALID_ARGS;  /* Trying to seek too far. This will never happen on 64-bit builds. */
    }

    newCursor = pStream->cursor;

    if (origin == FS_SEEK_SET) {
        newCursor = 0;
    } else if (origin == FS_SEEK_CUR) {
        newCursor = (fs_int64)pStream->cursor;
    } else if (origin == FS_SEEK_END) {
        newCursor = (fs_int64)*pStream->pDataSize;
    } else {
        FS_ASSERT(!"Invalid seek origin");
        return FS_INVALID_ARGS;
    }

    newCursor += offset;

    if (newCursor < 0) {
        return FS_BAD_SEEK;  /* Trying to seek prior to the start of the buffer. */
    }
    if ((size_t)newCursor > *pStream->pDataSize) {
        return FS_BAD_SEEK;  /* Trying to seek beyond the end of the buffer. */
    }

    pStream->cursor = (size_t)newCursor;

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

/* We'll get -Wlong-long warnings when forcing C89. Just force disable them. */
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wlong-long"
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
    #pragma GCC diagnostic pop  /* Fallthrough warnings. */
#endif
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic pop  /* -Wlong-long */
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
