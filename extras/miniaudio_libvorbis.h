/*
This implements a data source that decodes Vorbis streams via libvorbis + libvorbisfile

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.

You need to include this file after miniaudio.h.
*/
#ifndef miniaudio_libvorbis_h
#define miniaudio_libvorbis_h

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MA_NO_LIBVORBIS)
#ifndef OV_EXCLUDE_STATIC_CALLBACKS
#define OV_EXCLUDE_STATIC_CALLBACKS
#endif
#include <vorbis/vorbisfile.h>
#endif

typedef struct
{
    ma_data_source_base ds;     /* The libvorbis decoder can be used independently as a data source. */
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    void* pReadSeekTellUserData;
    ma_format format;           /* Will be either f32 or s16. */
#if !defined(MA_NO_LIBVORBIS)
    OggVorbis_File vf;
#endif
} ma_libvorbis;

MA_API ma_result ma_libvorbis_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libvorbis* pVorbis);
MA_API ma_result ma_libvorbis_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libvorbis* pVorbis);
MA_API void ma_libvorbis_uninit(ma_libvorbis* pVorbis, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_libvorbis_read_pcm_frames(ma_libvorbis* pVorbis, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_libvorbis_seek_to_pcm_frame(ma_libvorbis* pVorbis, ma_uint64 frameIndex);
MA_API ma_result ma_libvorbis_get_data_format(ma_libvorbis* pVorbis, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_libvorbis_get_cursor_in_pcm_frames(ma_libvorbis* pVorbis, ma_uint64* pCursor);
MA_API ma_result ma_libvorbis_get_length_in_pcm_frames(ma_libvorbis* pVorbis, ma_uint64* pLength);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

static ma_result ma_libvorbis_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_libvorbis_read_pcm_frames((ma_libvorbis*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_libvorbis_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_libvorbis_seek_to_pcm_frame((ma_libvorbis*)pDataSource, frameIndex);
}

static ma_result ma_libvorbis_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_libvorbis_get_data_format((ma_libvorbis*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_libvorbis_ds_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_libvorbis_get_cursor_in_pcm_frames((ma_libvorbis*)pDataSource, pCursor);
}

static ma_result ma_libvorbis_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_libvorbis_get_length_in_pcm_frames((ma_libvorbis*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_libvorbis_ds_vtable =
{
    ma_libvorbis_ds_read,
    ma_libvorbis_ds_seek,
    ma_libvorbis_ds_get_data_format,
    ma_libvorbis_ds_get_cursor,
    ma_libvorbis_ds_get_length
};


#if !defined(MA_NO_LIBVORBIS)
static size_t ma_libvorbis_vf_callback__read(void* pBufferOut, size_t size, size_t count, void* pUserData)
{
    ma_libvorbis* pVorbis = (ma_libvorbis*)pUserData;
    ma_result result;
    size_t bytesToRead;
    size_t bytesRead;

    /* For consistency with fread(). If `size` of `count` is 0, return 0 immediately without changing anything. */
    if (size == 0 || count == 0) {
        return 0;
    }

    bytesToRead = size * count;
    result = pVorbis->onRead(pVorbis->pReadSeekTellUserData, pBufferOut, bytesToRead, &bytesRead);
    if (result != MA_SUCCESS) {
        /* Not entirely sure what to return here. What if an error occurs, but some data was read and bytesRead is > 0? */
        return 0;
    }

    return bytesRead / size;
}

static int ma_libvorbis_vf_callback__seek(void* pUserData, ogg_int64_t offset, int whence)
{
    ma_libvorbis* pVorbis = (ma_libvorbis*)pUserData;
    ma_result result;
    ma_seek_origin origin;

    if (whence == SEEK_SET) {
        origin = ma_seek_origin_start;
    } else if (whence == SEEK_END) {
        origin = ma_seek_origin_end;
    } else {
        origin = ma_seek_origin_current;
    }

    result = pVorbis->onSeek(pVorbis->pReadSeekTellUserData, offset, origin);
    if (result != MA_SUCCESS) {
        return -1;
    }

    return 0;
}

static long ma_libvorbis_vf_callback__tell(void* pUserData)
{
    ma_libvorbis* pVorbis = (ma_libvorbis*)pUserData;
    ma_result result;
    ma_int64 cursor;
    
    result = pVorbis->onTell(pVorbis->pReadSeekTellUserData, &cursor);
    if (result != MA_SUCCESS) {
        return -1;
    }

    return (long)cursor;
}
#endif

static ma_result ma_libvorbis_init_internal(const ma_decoding_backend_config* pConfig, ma_libvorbis* pVorbis)
{
    ma_result result;
    ma_data_source_config dataSourceConfig;

    if (pVorbis == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pVorbis);
    pVorbis->format = ma_format_f32;    /* f32 by default. */

    if (pConfig != NULL && (pConfig->preferredFormat == ma_format_f32 || pConfig->preferredFormat == ma_format_s16)) {
        pVorbis->format = pConfig->preferredFormat;
    } else {
        /* Getting here means something other than f32 and s16 was specified. Just leave this unset to use the default format. */
    }

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_libvorbis_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pVorbis->ds);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_libvorbis_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libvorbis* pVorbis)
{
    ma_result result;

    (void)pAllocationCallbacks; /* Can't seem to find a way to configure memory allocations in libvorbis. */
    
    result = ma_libvorbis_init_internal(pConfig, pVorbis);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (onRead == NULL || onSeek == NULL) {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pVorbis->onRead = onRead;
    pVorbis->onSeek = onSeek;
    pVorbis->onTell = onTell;
    pVorbis->pReadSeekTellUserData = pReadSeekTellUserData;

    #if !defined(MA_NO_LIBVORBIS)
    {
        int libvorbisResult;
        ov_callbacks libvorbisCallbacks;

        /* We can now initialize the vorbis decoder. This must be done after we've set up the callbacks. */
        libvorbisCallbacks.read_func  = ma_libvorbis_vf_callback__read;
        libvorbisCallbacks.seek_func  = ma_libvorbis_vf_callback__seek;
        libvorbisCallbacks.close_func = NULL;
        libvorbisCallbacks.tell_func  = ma_libvorbis_vf_callback__tell;

        libvorbisResult = ov_open_callbacks(pVorbis, &pVorbis->vf, NULL, 0, libvorbisCallbacks);
        if (libvorbisResult < 0) {
            return MA_INVALID_FILE;
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libvorbis is disabled. */
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libvorbis_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libvorbis* pVorbis)
{
    ma_result result;

    (void)pAllocationCallbacks; /* Can't seem to find a way to configure memory allocations in libvorbis. */

    result = ma_libvorbis_init_internal(pConfig, pVorbis);
    if (result != MA_SUCCESS) {
        return result;
    }

    #if !defined(MA_NO_LIBVORBIS)
    {
        int libvorbisResult;

        libvorbisResult = ov_fopen(pFilePath, &pVorbis->vf);
        if (libvorbisResult < 0) {
            return MA_INVALID_FILE;
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libvorbis is disabled. */
        (void)pFilePath;
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API void ma_libvorbis_uninit(ma_libvorbis* pVorbis, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pVorbis == NULL) {
        return;
    }

    (void)pAllocationCallbacks;

    #if !defined(MA_NO_LIBVORBIS)
    {
        ov_clear(&pVorbis->vf);
    }
    #else
    {
        /* libvorbis is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
    }
    #endif

    ma_data_source_uninit(&pVorbis->ds);
}

MA_API ma_result ma_libvorbis_read_pcm_frames(ma_libvorbis* pVorbis, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (frameCount == 0) {
        return MA_INVALID_ARGS;
    }

    if (pVorbis == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_LIBVORBIS)
    {
        /* We always use floating point format. */
        ma_result result = MA_SUCCESS;  /* Must be initialized to MA_SUCCESS. */
        ma_uint64 totalFramesRead;
        ma_format format;
        ma_uint32 channels;

        ma_libvorbis_get_data_format(pVorbis, &format, &channels, NULL, NULL, 0);

        totalFramesRead = 0;
        while (totalFramesRead < frameCount) {
            long libvorbisResult;
            int framesToRead;
            ma_uint64 framesRemaining;

            framesRemaining = (frameCount - totalFramesRead);
            framesToRead = 1024;
            if (framesToRead > framesRemaining) {
                framesToRead = (int)framesRemaining;
            }

            if (format == ma_format_f32) {
                float** ppFramesF32;

                libvorbisResult = ov_read_float(&pVorbis->vf, &ppFramesF32, framesToRead, NULL);
                if (libvorbisResult < 0) {
                    result = MA_ERROR;  /* Error while decoding. */
                    break;
                } else {
                    /* Frames need to be interleaved. */
                    ma_interleave_pcm_frames(format, channels, libvorbisResult, (const void**)ppFramesF32, ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, format, channels));
                    totalFramesRead += libvorbisResult;

                    if (libvorbisResult == 0) {
                        result = MA_AT_END;
                        break;
                    }
                }
            } else {
                libvorbisResult = ov_read(&pVorbis->vf, ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, format, channels), framesToRead * ma_get_bytes_per_frame(format, channels), 0, 2, 1, NULL);
                if (libvorbisResult < 0) {
                    result = MA_ERROR;  /* Error while decoding. */
                    break;
                } else {
                    /* Conveniently, there's no need to interleaving when using ov_read(). I'm not sure why ov_read_float() is different in that regard... */
                    totalFramesRead += libvorbisResult / ma_get_bytes_per_frame(format, channels);

                    if (libvorbisResult == 0) {
                        result = MA_AT_END;
                        break;
                    }
                }
            }
        }

        if (pFramesRead != NULL) {
            *pFramesRead = totalFramesRead;
        }

        if (result == MA_SUCCESS && totalFramesRead == 0) {
            result = MA_AT_END;
        }

        return result;
    }
    #else
    {
        /* libvorbis is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);

        (void)pFramesOut;
        (void)frameCount;
        (void)pFramesRead;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libvorbis_seek_to_pcm_frame(ma_libvorbis* pVorbis, ma_uint64 frameIndex)
{
    if (pVorbis == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_LIBVORBIS)
    {
        int libvorbisResult = ov_pcm_seek(&pVorbis->vf, (ogg_int64_t)frameIndex);
        if (libvorbisResult != 0) {
            if (libvorbisResult == OV_ENOSEEK) {
                return MA_INVALID_OPERATION;    /* Not seekable. */
            } else if (libvorbisResult == OV_EINVAL) {
                return MA_INVALID_ARGS;
            } else {
                return MA_ERROR;
            }
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libvorbis is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);

        (void)frameIndex;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libvorbis_get_data_format(ma_libvorbis* pVorbis, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    /* Defaults for safety. */
    if (pFormat != NULL) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels != NULL) {
        *pChannels = 0;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = 0;
    }
    if (pChannelMap != NULL) {
        MA_ZERO_MEMORY(pChannelMap, sizeof(*pChannelMap) * channelMapCap);
    }

    if (pVorbis == NULL) {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL) {
        *pFormat = pVorbis->format;
    }

    #if !defined(MA_NO_LIBVORBIS)
    {
        vorbis_info* pInfo = ov_info(&pVorbis->vf, 0);
        if (pInfo == NULL) {
            return MA_INVALID_OPERATION;
        }

        if (pChannels != NULL) {
            *pChannels = pInfo->channels;
        }

        if (pSampleRate != NULL) {
            *pSampleRate = pInfo->rate;
        }

        if (pChannelMap != NULL) {
            ma_channel_map_init_standard(ma_standard_channel_map_vorbis, pChannelMap, channelMapCap, pInfo->channels);
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libvorbis is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libvorbis_get_cursor_in_pcm_frames(ma_libvorbis* pVorbis, ma_uint64* pCursor)
{
    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;   /* Safety. */

    if (pVorbis == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_LIBVORBIS)
    {
        ogg_int64_t offset = ov_pcm_tell(&pVorbis->vf);
        if (offset < 0) {
            return MA_INVALID_FILE;
        }

        *pCursor = (ma_uint64)offset;

        return MA_SUCCESS;
    }
    #else
    {
        /* libvorbis is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libvorbis_get_length_in_pcm_frames(ma_libvorbis* pVorbis, ma_uint64* pLength)
{
    if (pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    *pLength = 0;   /* Safety. */

    if (pVorbis == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_LIBVORBIS)
    {
        /* I don't know how to reliably retrieve the length in frames using libvorbis, so returning 0 for now. */
        *pLength = 0;

        return MA_SUCCESS;
    }
    #else
    {
        /* libvorbis is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

#endif
