/*
Demonstrates how to implement a custom decoder.

This example implements two custom decoders:

  * Vorbis via libvorbis
  * Opus via libopus

A custom decoder must implement a data source. In this example, the libvorbis data source is called
`ma_libvorbis` and the Opus data source is called `ma_libopus`. These two objects are compatible
with the `ma_data_source` APIs and can be taken straight from this example and used in real code.

The custom decoding data sources (`ma_libvorbis` and `ma_libopus` in this example) are connected to
the decoder via the decoder config (`ma_decoder_config`). You need to implement a vtable for each
of your custom decoders. See `ma_decoding_backend_vtable` for the functions you need to implement.
The `onInitFile`, `onInitFileW` and `onInitMemory` functions are optional.
*/
#define MA_NO_VORBIS    /* Disable the built-in Vorbis decoder to ensure the libvorbis decoder is picked. */
#define MA_NO_OPUS      /* Disable the (not yet implemented) built-in Opus decoder to ensure the libopus decoder is picked. */
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>


#if !defined(MA_NO_LIBVORBIS)
#define OV_EXCLUDE_STATIC_CALLBACKS
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


static ma_result ma_libvorbis_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_libvorbis_read_pcm_frames((ma_libvorbis*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_libvorbis_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_libvorbis_seek_to_pcm_frame((ma_libvorbis*)pDataSource, frameIndex);
}

static ma_result ma_libvorbis_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    return ma_libvorbis_get_data_format((ma_libvorbis*)pDataSource, pFormat, pChannels, pSampleRate, NULL, 0);
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
    NULL,   /* onMap() */
    NULL,   /* onUnmap() */
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

    bytesToRead = size * count;
    result = pVorbis->onRead(pVorbis->pReadSeekTellUserData, pBufferOut, bytesToRead, &bytesRead);

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
                    ma_interleave_pcm_frames(format, channels, libvorbisResult, ppFramesF32, ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, format, channels));
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
            ma_get_standard_channel_map(ma_standard_channel_map_vorbis, (ma_uint32)ma_min(pInfo->channels, channelMapCap), pChannelMap);
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

MA_API ma_result ma_libopus_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libopus* pVorbis);
MA_API ma_result ma_libopus_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libopus* pVorbis);
MA_API void ma_libopus_uninit(ma_libopus* pVorbis, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_libopus_read_pcm_frames(ma_libopus* pVorbis, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_libopus_seek_to_pcm_frame(ma_libopus* pVorbis, ma_uint64 frameIndex);
MA_API ma_result ma_libopus_get_data_format(ma_libopus* pVorbis, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_libopus_get_cursor_in_pcm_frames(ma_libopus* pVorbis, ma_uint64* pCursor);
MA_API ma_result ma_libopus_get_length_in_pcm_frames(ma_libopus* pVorbis, ma_uint64* pLength);


static ma_result ma_libopus_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_libopus_read_pcm_frames((ma_libopus*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_libopus_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_libopus_seek_to_pcm_frame((ma_libopus*)pDataSource, frameIndex);
}

static ma_result ma_libopus_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    return ma_libopus_get_data_format((ma_libopus*)pDataSource, pFormat, pChannels, pSampleRate, NULL, 0);
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
    NULL,   /* onMap() */
    NULL,   /* onUnmap() */
    ma_libopus_ds_get_data_format,
    ma_libopus_ds_get_cursor,
    ma_libopus_ds_get_length
};


#if !defined(MA_NO_LIBOPUS)
static int ma_libopus_of_callback__read(void* pUserData, void* pBufferOut, int bytesToRead)
{
    ma_libopus* pOpus = (ma_libopus*)pUserData;
    ma_result result;
    size_t bytesRead;

    result = pOpus->onRead(pOpus->pReadSeekTellUserData, pBufferOut, bytesToRead, &bytesRead);

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

        /* We can now initialize the vorbis decoder. This must be done after we've set up the callbacks. */
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
                libopusResult = op_read_float(pOpus->of, ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, format, channels), framesToRead * channels, NULL);
            } else {
                libopusResult = op_read      (pOpus->of, ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, format, channels), framesToRead * channels, NULL);
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
            ma_get_standard_channel_map(ma_standard_channel_map_vorbis, (ma_uint32)ma_min(channels, channelMapCap), pChannelMap);
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


/*
In this example we're going to be implementing our custom decoders as an extension to the ma_decoder
object. We declare our decoding backends after the ma_decoder object which allows us to avoid a
memory allocation. There are many ways to manage the backend objects so use whatever works best for
your particular scenario.
*/
typedef struct
{
    ma_decoder base;    /* Must be the first member so we can cast between ma_decoder_ex and ma_decoder. */
    union
    {
        ma_libvorbis libvorbis;
        ma_libopus   libopus;
    } backends;
} ma_decoder_ex;

static ma_result ma_decoder_ex_init__libvorbis(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_decoder_ex* pDecoderEx = (ma_decoder_ex*)pUserData;
    ma_result result;

    /*
    NOTE: We don't need to allocate any memory for the libvorbis object here because our backend
    data is just extended off the ma_decoder object (ma_decode_ex) which is passed in as a
    parameter to this function. We therefore need only cast to ma_decoder_ex and reference data
    directly from that structure.
    */
    *ppBackend = (ma_data_source*)&pDecoderEx->backends.libvorbis;

    result = ma_libvorbis_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, &pDecoderEx->backends.libvorbis);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoder_ex_init_file__libvorbis(void* pUserData, const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_decoder_ex* pDecoderEx = (ma_decoder_ex*)pUserData;
    ma_result result;

    *ppBackend = (ma_data_source*)&pDecoderEx->backends.libvorbis;

    result = ma_libvorbis_init_file(pFilePath, pConfig, pAllocationCallbacks, &pDecoderEx->backends.libvorbis);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_decoder_ex_uninit__libvorbis(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_libvorbis* pVorbis = (ma_libvorbis*)pBackend;
    ma_libvorbis_uninit(pVorbis, pAllocationCallbacks);

    /* No need to free the pVorbis object because it is sitting in the containing ma_decoder_ex object. */

    (void)pUserData;
}

static ma_result ma_decoder_ex_get_channel_map__libvorbis(void* pUserData, ma_data_source* pBackend, ma_channel* pChannelMap, size_t channelMapCap)
{
    ma_libvorbis* pVorbis = (ma_libvorbis*)pBackend;

    (void)pUserData;

    return ma_libvorbis_get_data_format(pVorbis, NULL, NULL, NULL, pChannelMap, channelMapCap);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_libvorbis =
{
    ma_decoder_ex_init__libvorbis,
    ma_decoder_ex_init_file__libvorbis,
    NULL, /* onInitFileW() */
    NULL, /* onInitMemory() */
    ma_decoder_ex_uninit__libvorbis,
    ma_decoder_ex_get_channel_map__libvorbis
};



static ma_result ma_decoder_ex_init__libopus(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_decoder_ex* pDecoderEx = (ma_decoder_ex*)pUserData;
    ma_result result;

    /*
    NOTE: We don't need to allocate any memory for the libopus object here because our backend
    data is just extended off the ma_decoder object (ma_decode_ex) which is passed in as a
    parameter to this function. We therefore need only cast to ma_decoder_ex and reference data
    directly from that structure.
    */
    *ppBackend = (ma_data_source*)&pDecoderEx->backends.libopus;

    result = ma_libopus_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, &pDecoderEx->backends.libopus);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoder_ex_init_file__libopus(void* pUserData, const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_decoder_ex* pDecoderEx = (ma_decoder_ex*)pUserData;
    ma_result result;

    *ppBackend = (ma_data_source*)&pDecoderEx->backends.libopus;

    result = ma_libopus_init_file(pFilePath, pConfig, pAllocationCallbacks, &pDecoderEx->backends.libopus);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_decoder_ex_uninit__libopus(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_libopus* pOpus = (ma_libopus*)pBackend;
    ma_libopus_uninit(pOpus, pAllocationCallbacks);

    /* No need to free the pOpus object because it is sitting in the containing ma_decoder_ex object. */

    (void)pUserData;
}

static ma_result ma_decoder_ex_get_channel_map__libopus(void* pUserData, ma_data_source* pBackend, ma_channel* pChannelMap, size_t channelMapCap)
{
    ma_libopus* pOpus = (ma_libopus*)pBackend;

    (void)pUserData;

    return ma_libopus_get_data_format(pOpus, NULL, NULL, NULL, pChannelMap, channelMapCap);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_libopus =
{
    ma_decoder_ex_init__libopus,
    ma_decoder_ex_init_file__libopus,
    NULL, /* onInitFileW() */
    NULL, /* onInitMemory() */
    ma_decoder_ex_uninit__libopus,
    ma_decoder_ex_get_channel_map__libopus
};




void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_data_source* pDataSource = (ma_data_source*)pDevice->pUserData;
    if (pDataSource == NULL) {
        return;
    }

    ma_data_source_read_pcm_frames(pDataSource, pOutput, frameCount, NULL, MA_TRUE);

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder_ex decoder;
    ma_device_config deviceConfig;
    ma_device device;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    /*
    Add your custom backend vtables here. The order in the array defines the order of priority. The
    vtables will be passed in via the decoder config.
    */
    ma_decoding_backend_vtable* pCustomBackendVTables[] =
    {
        &g_ma_decoding_backend_vtable_libvorbis,
        &g_ma_decoding_backend_vtable_libopus
    };


    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    
    /* Initialize the decoder. */
    decoderConfig = ma_decoder_config_init_default();
    decoderConfig.pCustomBackendUserData   = &decoder;  /* In this example our backend objects are contained within a ma_decoder_ex object to avoid a malloc. Our vtables need to know about this. */
    decoderConfig.ppCustomBackendVTables   = pCustomBackendVTables;
    decoderConfig.customBackendVTableCount = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
    
    result = ma_decoder_init_file(argv[1], &decoderConfig, &decoder.base);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.");
        return -1;
    }


    /* Initialize the device. */
    result = ma_data_source_get_data_format(&decoder, &format, &channels, &sampleRate);
    if (result != MA_SUCCESS) {
        printf("Failed to retrieve decoder data format.");
        ma_decoder_uninit(&decoder.base);
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = format;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate        = sampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &decoder;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        ma_decoder_uninit(&decoder.base);
        return -1;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder.base);
        return -1;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder.base);

    return 0;
}