#ifndef miniaudio_vfs_debugging_c
#define miniaudio_vfs_debugging_c

#include "miniaudio_vfs_debugging.h"

MA_API ma_vfs_debugging_config ma_vfs_debugging_config_init(ma_vfs* pUnderlyingVFS, ma_uint32 latencyInMilliseconds)
{
    ma_vfs_debugging_config config;

    MA_ZERO_OBJECT(&config);
    config.pUnderlyingVFS        = pUnderlyingVFS;
    config.latencyInMilliseconds = latencyInMilliseconds;

    return config;
}


static ma_vfs_debugging* ma_vfs_debugging_cast(ma_vfs* pVFS)
{
    return (ma_vfs_debugging*)pVFS;
}

static ma_vfs* ma_vfs_debugging_get_underlying_vfs(ma_vfs* pVFS)
{
    return ma_vfs_debugging_cast(pVFS)->config.pUnderlyingVFS;
}


static ma_result ma_vfs_debugging_open(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
    return ma_vfs_open(ma_vfs_debugging_get_underlying_vfs(pVFS), pFilePath, openMode, pFile);
}

static ma_result ma_vfs_debugging_open_w(ma_vfs* pVFS, const wchar_t* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
    return ma_vfs_open_w(ma_vfs_debugging_get_underlying_vfs(pVFS), pFilePath, openMode, pFile);
}

static ma_result ma_vfs_debugging_close(ma_vfs* pVFS, ma_vfs_file file)
{
    return ma_vfs_close(ma_vfs_debugging_get_underlying_vfs(pVFS), file);
}

static ma_result ma_vfs_debugging_read(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
{
    ma_vfs_debugging* pDebuggingVFS = ma_vfs_debugging_cast(pVFS);
    ma_result result;

    /* Introduce artificial latency if requested. Ignored on Emscripten. */
    #ifndef __EMSCRIPTEN__
    {
        if (pDebuggingVFS->config.latencyInMilliseconds > 0) {
            ma_sleep(pDebuggingVFS->config.latencyInMilliseconds);
        }
    }
    #endif

    /*printf("READING\n");*/

    result = ma_vfs_read(ma_vfs_debugging_get_underlying_vfs(pVFS), file, pDst, sizeInBytes, pBytesRead);
    return result;
}

static ma_result ma_vfs_debugging_write(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten)
{
    return ma_vfs_write(ma_vfs_debugging_get_underlying_vfs(pVFS), file, pSrc, sizeInBytes, pBytesWritten);
}

static ma_result ma_vfs_debugging_seek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
    return ma_vfs_seek(ma_vfs_debugging_get_underlying_vfs(pVFS), file, offset, origin);
}

static ma_result ma_vfs_debugging_tell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor)
{
    return ma_vfs_tell(ma_vfs_debugging_get_underlying_vfs(pVFS), file, pCursor);
}

static ma_result ma_vfs_debugging_info(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo)
{
    return ma_vfs_info(ma_vfs_debugging_get_underlying_vfs(pVFS), file, pInfo);
}

MA_API ma_result ma_vfs_debugging_init(const ma_vfs_debugging_config* pConfig, ma_vfs_debugging* pVFS)
{
    if (pVFS == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pVFS);

    if (pConfig == NULL) {
        pVFS->config = ma_vfs_debugging_config_init(NULL, 0);
    } else {
        pVFS->config = *pConfig;
    }

    pVFS->cb.onOpen  = ma_vfs_debugging_open;
    pVFS->cb.onOpenW = ma_vfs_debugging_open_w;
    pVFS->cb.onClose = ma_vfs_debugging_close;
    pVFS->cb.onRead  = ma_vfs_debugging_read;
    pVFS->cb.onWrite = ma_vfs_debugging_write;
    pVFS->cb.onSeek  = ma_vfs_debugging_seek;
    pVFS->cb.onTell  = ma_vfs_debugging_tell;
    pVFS->cb.onInfo  = ma_vfs_debugging_info;

    return MA_SUCCESS;
}

#endif  /* miniaudio_vfs_debugging_c */
