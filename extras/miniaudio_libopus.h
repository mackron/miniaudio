/*
This implements a data source that decodes Opus streams via libopus + libopusfile

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.

You need to include this file after miniaudio.h.
*/
#ifndef miniaudio_libopus_h
#define miniaudio_libopus_h

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MA_NO_LIBOPUS)
#include <opusfile.h>
#endif

typedef struct
{
    ma_data_source_base ds;     /* The libopus decoder can be used independently as a data source. */
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    void* pReadSeekTellUserData;
    ma_format format;           /* Will be either f32 or s16. */
#if !defined(MA_NO_LIBOPUS)
    OggOpusFile* of;
#endif
} ma_libopus;

MA_API ma_result ma_libopus_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libopus* pOpus);
MA_API ma_result ma_libopus_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libopus* pOpus);
MA_API void ma_libopus_uninit(ma_libopus* pOpus, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_libopus_read_pcm_frames(ma_libopus* pOpus, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_libopus_seek_to_pcm_frame(ma_libopus* pOpus, ma_uint64 frameIndex);
MA_API ma_result ma_libopus_get_data_format(ma_libopus* pOpus, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_libopus_get_cursor_in_pcm_frames(ma_libopus* pOpus, ma_uint64* pCursor);
MA_API ma_result ma_libopus_get_length_in_pcm_frames(ma_libopus* pOpus, ma_uint64* pLength);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

static ma_result ma_libopus_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_libopus_read_pcm_frames((ma_libopus*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_libopus_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_libopus_seek_to_pcm_frame((ma_libopus*)pDataSource, frameIndex);
}

static ma_result ma_libopus_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_libopus_get_data_format((ma_libopus*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_libopus_ds_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_libopus_get_cursor_in_pcm_frames((ma_libopus*)pDataSource, pCursor);
}

static ma_result ma_libopus_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_libopus_get_length_in_pcm_frames((ma_libopus*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_libopus_ds_vtable =
{
    ma_libopus_ds_read,
    ma_libopus_ds_seek,
    ma_libopus_ds_get_data_format,
    ma_libopus_ds_get_cursor,
    ma_libopus_ds_get_length
};


#if !defined(MA_NO_LIBOPUS)
static int ma_libopus_of_callback__read(void* pUserData, unsigned char* pBufferOut, int bytesToRead)
{
    ma_libopus* pOpus = (ma_libopus*)pUserData;
    ma_result result;
    size_t bytesRead;

    result = pOpus->onRead(pOpus->pReadSeekTellUserData, (void*)pBufferOut, bytesToRead, &bytesRead);

    if (result != MA_SUCCESS) {
        return -1;
    }

    return (int)bytesRead;
}

static int ma_libopus_of_callback__seek(void* pUserData, ogg_int64_t offset, int whence)
{
    ma_libopus* pOpus = (ma_libopus*)pUserData;
    ma_result result;
    ma_seek_origin origin;

    if (whence == SEEK_SET) {
        origin = ma_seek_origin_start;
    } else if (whence == SEEK_END) {
        origin = ma_seek_origin_end;
    } else {
        origin = ma_seek_origin_current;
    }

    result = pOpus->onSeek(pOpus->pReadSeekTellUserData, offset, origin);
    if (result != MA_SUCCESS) {
        return -1;
    }

    return 0;
}

static opus_int64 ma_libopus_of_callback__tell(void* pUserData)
{
    ma_libopus* pOpus = (ma_libopus*)pUserData;
    ma_result result;
    ma_int64 cursor;

    if (pOpus->onTell == NULL) {
        return -1;
    }

    result = pOpus->onTell(pOpus->pReadSeekTellUserData, &cursor);
    if (result != MA_SUCCESS) {
        return -1;
    }

    return cursor;
}
#endif

static ma_result ma_libopus_init_internal(const ma_decoding_backend_config* pConfig, ma_libopus* pOpus)
{
    ma_result result;
    ma_data_source_config dataSourceConfig;

    if (pOpus == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pOpus);
    pOpus->format = ma_format_f32;    /* f32 by default. */

    if (pConfig != NULL && (pConfig->preferredFormat == ma_format_f32 || pConfig->preferredFormat == ma_format_s16)) {
        pOpus->format = pConfig->preferredFormat;
    } else {
        /* Getting here means something other than f32 and s16 was specified. Just leave this unset to use the default format. */
    }

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_libopus_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pOpus->ds);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the base data source. */
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_libopus_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libopus* pOpus)
{
    ma_result result;

    (void)pAllocationCallbacks; /* Can't seem to find a way to configure memory allocations in libopus. */
    
    result = ma_libopus_init_internal(pConfig, pOpus);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (onRead == NULL || onSeek == NULL) {
        return MA_INVALID_ARGS; /* onRead and onSeek are mandatory. */
    }

    pOpus->onRead = onRead;
    pOpus->onSeek = onSeek;
    pOpus->onTell = onTell;
    pOpus->pReadSeekTellUserData = pReadSeekTellUserData;

    #if !defined(MA_NO_LIBOPUS)
    {
        int libopusResult;
        OpusFileCallbacks libopusCallbacks;

        /* We can now initialize the Opus decoder. This must be done after we've set up the callbacks. */
        libopusCallbacks.read  = ma_libopus_of_callback__read;
        libopusCallbacks.seek  = ma_libopus_of_callback__seek;
        libopusCallbacks.close = NULL;
        libopusCallbacks.tell  = ma_libopus_of_callback__tell;

        pOpus->of = op_open_callbacks(pOpus, &libopusCallbacks, NULL, 0, &libopusResult);
        if (pOpus->of == NULL) {
            return MA_INVALID_FILE;
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libopus is disabled. */
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libopus_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libopus* pOpus)
{
    ma_result result;

    (void)pAllocationCallbacks; /* Can't seem to find a way to configure memory allocations in libopus. */

    result = ma_libopus_init_internal(pConfig, pOpus);
    if (result != MA_SUCCESS) {
        return result;
    }

    #if !defined(MA_NO_LIBOPUS)
    {
        int libopusResult;

        pOpus->of = op_open_file(pFilePath, &libopusResult);
        if (pOpus->of == NULL) {
            return MA_INVALID_FILE;
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libopus is disabled. */
        (void)pFilePath;
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API void ma_libopus_uninit(ma_libopus* pOpus, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pOpus == NULL) {
        return;
    }

    (void)pAllocationCallbacks;

    #if !defined(MA_NO_LIBOPUS)
    {
        op_free(pOpus->of);
    }
    #else
    {
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
    }
    #endif

    ma_data_source_uninit(&pOpus->ds);
}

MA_API ma_result ma_libopus_read_pcm_frames(ma_libopus* pOpus, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (frameCount == 0) {
        return MA_INVALID_ARGS;
    }

    if (pOpus == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_LIBOPUS)
    {
        /* We always use floating point format. */
        ma_result result = MA_SUCCESS;  /* Must be initialized to MA_SUCCESS. */
        ma_uint64 totalFramesRead;
        ma_format format;
        ma_uint32 channels;

        ma_libopus_get_data_format(pOpus, &format, &channels, NULL, NULL, 0);

        totalFramesRead = 0;
        while (totalFramesRead < frameCount) {
            long libopusResult;
            int framesToRead;
            ma_uint64 framesRemaining;

            framesRemaining = (frameCount - totalFramesRead);
            framesToRead = 1024;
            if (framesToRead > framesRemaining) {
                framesToRead = (int)framesRemaining;
            }

            if (format == ma_format_f32) {
                libopusResult = op_read_float(pOpus->of, (float*)ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, format, channels), framesToRead * channels, NULL);
            } else {
                libopusResult = op_read      (pOpus->of, (opus_int16*)ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, format, channels), framesToRead * channels, NULL);
            }

            if (libopusResult < 0) {
                result = MA_ERROR;  /* Error while decoding. */
                break;
            } else {
                totalFramesRead += libopusResult;

                if (libopusResult == 0) {
                    result = MA_AT_END;
                    break;
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
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);

        (void)pFramesOut;
        (void)frameCount;
        (void)pFramesRead;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libopus_seek_to_pcm_frame(ma_libopus* pOpus, ma_uint64 frameIndex)
{
    if (pOpus == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_LIBOPUS)
    {
        int libopusResult = op_pcm_seek(pOpus->of, (ogg_int64_t)frameIndex);
        if (libopusResult != 0) {
            if (libopusResult == OP_ENOSEEK) {
                return MA_INVALID_OPERATION;    /* Not seekable. */
            } else if (libopusResult == OP_EINVAL) {
                return MA_INVALID_ARGS;
            } else {
                return MA_ERROR;
            }
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);

        (void)frameIndex;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libopus_get_data_format(ma_libopus* pOpus, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
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

    if (pOpus == NULL) {
        return MA_INVALID_OPERATION;
    }

    if (pFormat != NULL) {
        *pFormat = pOpus->format;
    }

    #if !defined(MA_NO_LIBOPUS)
    {
        ma_uint32 channels = op_channel_count(pOpus->of, -1);

        if (pChannels != NULL) {
            *pChannels = channels;
        }

        if (pSampleRate != NULL) {
            *pSampleRate = 48000;
        }

        if (pChannelMap != NULL) {
            ma_channel_map_init_standard(ma_standard_channel_map_vorbis, pChannelMap, channelMapCap, channels);
        }

        return MA_SUCCESS;
    }
    #else
    {
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libopus_get_cursor_in_pcm_frames(ma_libopus* pOpus, ma_uint64* pCursor)
{
    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;   /* Safety. */

    if (pOpus == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_LIBOPUS)
    {
        ogg_int64_t offset = op_pcm_tell(pOpus->of);
        if (offset < 0) {
            return MA_INVALID_FILE;
        }

        *pCursor = (ma_uint64)offset;

        return MA_SUCCESS;
    }
    #else
    {
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

MA_API ma_result ma_libopus_get_length_in_pcm_frames(ma_libopus* pOpus, ma_uint64* pLength)
{
    if (pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    *pLength = 0;   /* Safety. */

    if (pOpus == NULL) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_LIBOPUS)
    {
        ogg_int64_t length = op_pcm_total(pOpus->of, -1);
        if (length < 0) {
            return MA_ERROR;
        }

        *pLength = (ma_uint64)length;

        return MA_SUCCESS;
    }
    #else
    {
        /* libopus is disabled. Should never hit this since initialization would have failed. */
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

#endif
