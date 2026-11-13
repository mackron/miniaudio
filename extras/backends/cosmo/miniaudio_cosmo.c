#ifndef miniaudio_backend_cosmo_c
#define miniaudio_backend_cosmo_c

#include "miniaudio_cosmo.h"

/* TODO: Replace with MA_ZERO_MEMORY() when this backend is integrated into miniaudio. */
#include <string.h>
#define MA_COSMO_ZERO_MEMORY(p, sz) memset((p), 0, (sz))

#if defined(__COSMOPOLITAN__)
    #define MA_SUPPORT_COSMO
#endif

#if defined(MA_SUPPORT_COSMO) && !defined(MA_NO_COSMO) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_COSMO))
    #define MA_HAS_COSMO
#endif

#if defined(MA_HAS_COSMO)
#include <cosmoaudio.h>
#include <limits.h>
#include <time.h>

typedef struct
{
    struct CosmoAudio* pAudio;
    float* pBuffer;
    ma_uint32 periodSizeInFrames;
} ma_device_stream_cosmo;

typedef struct
{
    ma_device_stream_cosmo playback;
    ma_device_stream_cosmo capture;
} ma_device_state_cosmo;


static ma_device_state_cosmo* ma_device_get_backend_state__cosmo(ma_device* pDevice)
{
    return (ma_device_state_cosmo*)ma_device_get_backend_state(pDevice);
}

static void ma_backend_info__cosmo(ma_device_backend_info* pBackendInfo)
{
    pBackendInfo->pName = "Cosmo";
    pBackendInfo->noAudioThread = MA_TRUE;
}

static ma_result ma_context_init__cosmo(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    (void)pContext;
    (void)pContextBackendConfig;

    *ppContextState = NULL;
    return MA_SUCCESS;
}

static void ma_context_uninit__cosmo(ma_context* pContext)
{
    (void)pContext;
}

static ma_result ma_context_enumerate_devices__cosmo(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    ma_device_info info;

    (void)pContext;

    MA_COSMO_ZERO_MEMORY(&info, sizeof(info));
    info.isDefault = MA_TRUE;
    ma_strncpy_s(info.name, sizeof(info.name), "Default Playback Device", (size_t)-1);
    ma_device_info_add_native_data_format(&info, ma_format_f32, 1, 2, ma_standard_sample_rate_min, ma_standard_sample_rate_max);

    if (callback(ma_device_type_playback, &info, pCallbackUserData) == MA_DEVICE_ENUMERATION_ABORT) {
        return MA_SUCCESS;
    }


    MA_COSMO_ZERO_MEMORY(&info, sizeof(info));
    info.isDefault = MA_TRUE;
    ma_strncpy_s(info.name, sizeof(info.name), "Default Capture Device", (size_t)-1);
    ma_device_info_add_native_data_format(&info, ma_format_f32, 1, 2, ma_standard_sample_rate_min, ma_standard_sample_rate_max);

    if (callback(ma_device_type_capture, &info, pCallbackUserData) == MA_DEVICE_ENUMERATION_ABORT) {
        return MA_SUCCESS;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stream_close__cosmo(ma_device* pDevice, ma_device_stream_cosmo* pStream)
{
    int result = COSMOAUDIO_SUCCESS;

    if (pStream->pAudio != NULL) {
        result = cosmoaudio_close(pStream->pAudio);
        pStream->pAudio = NULL;
    }

    if (result != COSMOAUDIO_SUCCESS) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Failed to close stream (%d).\n", result);
        return MA_ERROR;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stream_init__cosmo(ma_device* pDevice, ma_device_stream_cosmo* pStream, ma_device_type deviceType, const ma_device_config_cosmo* pConfig, ma_device_descriptor* pDescriptor)
{
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 periodCount;
    ma_uint32 bufferSizeInFrames;
    struct CosmoAudioOpenOptions options;
    int writableFrames = 0;
    int resultCosmo;

    if (pDescriptor->shareMode == ma_share_mode_exclusive) {
        return MA_SHARE_MODE_NOT_SUPPORTED;
    }

    if (pDescriptor->pDeviceID != NULL && pDescriptor->pDeviceID->custom.i != 0) {
        return MA_NO_DEVICE;
    }

    channels = pDescriptor->channels;
    if (channels != 1 && channels != 2) {
        channels = 2;
    }

    sampleRate = pDescriptor->sampleRate;
    if (sampleRate == 0) {
        sampleRate = MA_DEFAULT_SAMPLE_RATE;
    }

    /* Cosmo explicitly says that sample rates below 8000 are not supported. Clamp. */
    if (sampleRate < 8000) {
        sampleRate = 8000;
    }
    if (sampleRate > 384000) {
        sampleRate = 384000;
    }

    /*
    Cosmo rejects writes equal to the ring capacity. To enforce this we just use a period count of 2 so that
    buffer updates are split in half.
    */
    periodCount = pDescriptor->periodCount;
    if (periodCount < 2) {
        periodCount = 2;
    }

    pStream->periodSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDescriptor, sampleRate);

    bufferSizeInFrames = pStream->periodSizeInFrames * periodCount;
    if (bufferSizeInFrames == 0 || bufferSizeInFrames >= INT_MAX) {
        return MA_INVALID_DEVICE_CONFIG;
    }

    MA_COSMO_ZERO_MEMORY(&options, sizeof(options));
    options.sizeofThis = sizeof(options);

    if (deviceType == ma_device_type_capture) {
        options.deviceType = kCosmoAudioDeviceTypeCapture;
    } else {
        options.deviceType = kCosmoAudioDeviceTypePlayback;
    }

    options.channels     = (int)channels;
    options.sampleRate   = (int)sampleRate;
    options.bufferFrames = (int)bufferSizeInFrames;
    options.debugLog     = pConfig->enableDebugLog;

    resultCosmo = cosmoaudio_open(&pStream->pAudio, &options);
    if (resultCosmo != COSMOAUDIO_SUCCESS) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Failed to open stream (%d).\n", resultCosmo);
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    /*
    For playback, we can get a more accurate period size by querying the writable frames with
    cosmoaudio_poll(). No such mechanic is available for capture so we'll just assume our requested
    period size.
    */
    if (deviceType == ma_device_type_playback) {
        resultCosmo = cosmoaudio_poll(pStream->pAudio, NULL, &writableFrames);
        if (resultCosmo != COSMOAUDIO_SUCCESS) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Failed to query playback buffer (%d).\n", resultCosmo);
            return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
        }

        if (writableFrames > options.bufferFrames) {
            pStream->periodSizeInFrames = (ma_uint32)writableFrames / periodCount;
            if ((ma_uint32)writableFrames % periodCount != 0) {
                pStream->periodSizeInFrames += 1;
            }
        }
    }

    pDescriptor->format                   = ma_format_f32;
    pDescriptor->channels                 = channels;
    pDescriptor->sampleRate               = sampleRate;
    pDescriptor->periodSizeInFrames       = pStream->periodSizeInFrames;
    pDescriptor->periodSizeInMilliseconds = 0;
    pDescriptor->periodCount              = periodCount;
    ma_channel_map_init_standard(ma_standard_channel_map_default, pDescriptor->channelMap, ma_countof(pDescriptor->channelMap), channels);

    return MA_SUCCESS;
}

static ma_result ma_device_init__cosmo(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    const ma_device_config_cosmo* pConfig = (const ma_device_config_cosmo*)pDeviceBackendConfig;
    ma_device_config_cosmo defaultConfig;
    ma_device_stream_cosmo playbackStream;
    ma_device_stream_cosmo captureStream;
    ma_device_state_cosmo* pState;
    size_t bufferSizeInBytesCapture = 0;
    size_t bufferSizeInBytesPlayback = 0;
    ma_device_type deviceType;
    ma_result result;

    *ppDeviceState = NULL;

    deviceType = ma_device_get_type(pDevice);
    if (deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    if (pConfig == NULL) {
        defaultConfig = ma_device_config_cosmo_init();
        pConfig = &defaultConfig;
    }

    MA_COSMO_ZERO_MEMORY(&playbackStream, sizeof(playbackStream));
    MA_COSMO_ZERO_MEMORY(&captureStream,  sizeof(captureStream));

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
        result = ma_device_stream_init__cosmo(pDevice, &captureStream, ma_device_type_capture, pConfig, pDescriptorCapture);
        if (result != MA_SUCCESS) {
            goto error;
        }

        bufferSizeInBytesCapture = ma_align_64((size_t)pDescriptorCapture->periodSizeInFrames * pDescriptorCapture->channels * sizeof(float));
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        result = ma_device_stream_init__cosmo(pDevice, &playbackStream, ma_device_type_playback, pConfig, pDescriptorPlayback);
        if (result != MA_SUCCESS) {
            goto error;
        }

        bufferSizeInBytesPlayback = ma_align_64((size_t)pDescriptorPlayback->periodSizeInFrames * pDescriptorPlayback->channels * sizeof(float));
    }

    pState = (ma_device_state_cosmo*)ma_malloc(ma_align_64(sizeof(*pState)) + bufferSizeInBytesCapture + bufferSizeInBytesPlayback, ma_device_get_allocation_callbacks(pDevice));
    if (pState == NULL) {
        result = MA_OUT_OF_MEMORY;
        goto error;
    }

    pState->playback = playbackStream;
    pState->capture  = captureStream;

    if (bufferSizeInBytesCapture > 0) {
        pState->capture.pBuffer = (float*)ma_offset_ptr(pState, ma_align_64(sizeof(*pState)));
    }

    if (bufferSizeInBytesPlayback > 0) {
        pState->playback.pBuffer = (float*)ma_offset_ptr(pState, ma_align_64(sizeof(*pState)) + bufferSizeInBytesCapture);
    }

    *ppDeviceState = pState;
    return MA_SUCCESS;

error:
    ma_device_stream_close__cosmo(pDevice, &captureStream);
    ma_device_stream_close__cosmo(pDevice, &playbackStream);
    return result;
}

static void ma_device_uninit__cosmo(ma_device* pDevice)
{
    ma_device_state_cosmo* pState = ma_device_get_backend_state__cosmo(pDevice);

    ma_device_stream_close__cosmo(pDevice, &pState->capture);
    ma_device_stream_close__cosmo(pDevice, &pState->playback);
    
    ma_free(pState, ma_device_get_allocation_callbacks(pDevice));
}

static ma_result ma_device_process_playback__cosmo(ma_device* pDevice)
{
    ma_device_state_cosmo* pState = ma_device_get_backend_state__cosmo(pDevice);
    ma_device_stream_cosmo* pStream = &pState->playback;
    ma_uint32 framesToWrite;
    ma_uint32 totalFramesWritten = 0;
    int writableFrames = 0;
    int framesWritten;
    int resultCosmo;
    ma_result result;

    resultCosmo = cosmoaudio_poll(pStream->pAudio, NULL, &writableFrames);
    if (resultCosmo != COSMOAUDIO_SUCCESS) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Playback poll failed (%d).\n", resultCosmo);
        return MA_ERROR;
    }

    while (writableFrames > 0) {
        framesToWrite = ma_min((ma_uint32)writableFrames, pStream->periodSizeInFrames);

        result = ma_device_handle_backend_data_callback(pDevice, pStream->pBuffer, NULL, framesToWrite);
        if (result != MA_SUCCESS) {
            return result;
        }

        framesWritten = cosmoaudio_write(pStream->pAudio, pStream->pBuffer, (int)framesToWrite);
        if (framesWritten < 0) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Playback write failed (%d).\n", framesWritten);
            return MA_ERROR;
        }

        if (framesWritten != (int)framesToWrite) {
            ma_log_post(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Playback write did not accept all requested frames.\n");
            return MA_ERROR;
        }

        writableFrames     -= framesWritten;
        totalFramesWritten += (ma_uint32)framesWritten;
    }

    if (totalFramesWritten == 0) {
        return MA_NO_DATA_AVAILABLE;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_process_capture__cosmo(ma_device* pDevice)
{
    ma_device_state_cosmo* pState = ma_device_get_backend_state__cosmo(pDevice);
    ma_device_stream_cosmo* pStream = &pState->capture;
    ma_uint32 totalFramesRead = 0;
    int readableFrames = 0;
    int framesRead;
    int resultCosmo;
    ma_result result;

    resultCosmo = cosmoaudio_poll(pStream->pAudio, &readableFrames, NULL);
    if (resultCosmo != COSMOAUDIO_SUCCESS) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Capture poll failed (%d).\n", resultCosmo);
        return MA_ERROR;
    }

    while (readableFrames > 0) {
        framesRead = cosmoaudio_read(pStream->pAudio, pStream->pBuffer, (int)ma_min((ma_uint32)readableFrames, pStream->periodSizeInFrames));
        if (framesRead < 0) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Capture read failed (%d).\n", framesRead);
            return MA_ERROR;
        }

        if (framesRead == 0) {
            break;
        }

        result = ma_device_handle_backend_data_callback(pDevice, NULL, pStream->pBuffer, (ma_uint32)framesRead);
        if (result != MA_SUCCESS) {
            return result;
        }

        readableFrames  -= framesRead;
        totalFramesRead += (ma_uint32)framesRead;
    }

    if (totalFramesRead == 0) {
        return MA_NO_DATA_AVAILABLE;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stop__cosmo(ma_device* pDevice)
{
    ma_device_state_cosmo* pState = ma_device_get_backend_state__cosmo(pDevice);
    int resultCosmo;

    if (pState->playback.pAudio != NULL) {
        resultCosmo = cosmoaudio_flush(pState->playback.pAudio);
        if (resultCosmo != COSMOAUDIO_SUCCESS) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Failed to drain playback (%d).\n", resultCosmo);
            return MA_ERROR;
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_device_start__cosmo(ma_device* pDevice)
{
    ma_device_state_cosmo* pState = ma_device_get_backend_state__cosmo(pDevice);
    ma_device_stream_cosmo* pStream = &pState->capture;
    int readableFrames = 0;
    int framesRead;
    int resultCosmo;

    if (pStream->pAudio != NULL) {
        /*
        The Cosmo audio API has no explicit start or stop operation. This means that capture is
        running under the hood at all times. When we start the device we need to discard any
        background-recorded audio data that is still sitting in the internal buffer.
        */
        resultCosmo = cosmoaudio_poll(pStream->pAudio, &readableFrames, NULL);
        if (resultCosmo != COSMOAUDIO_SUCCESS) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[Cosmo] Capture poll failed (%d).\n", resultCosmo);
            return MA_ERROR;
        }

        while (readableFrames > 0) {
            framesRead = cosmoaudio_read(pStream->pAudio, pStream->pBuffer, (int)ma_min((ma_uint32)readableFrames, pStream->periodSizeInFrames));
            if (framesRead <= 0) {
                break;
            }

            readableFrames -= framesRead;
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_device_step__cosmo(ma_device* pDevice, ma_blocking_mode blockingMode)
{
    ma_device_state_cosmo* pState = ma_device_get_backend_state__cosmo(pDevice);
    ma_bool32 hasProcessedData = MA_FALSE;
    ma_result result;
    struct timespec delay;

    while (ma_device_is_started(pDevice)) {
        if (pState->capture.pAudio != NULL) {
            result = ma_device_process_capture__cosmo(pDevice);
            if (result == MA_SUCCESS) {
                hasProcessedData = MA_TRUE;
            } else if (result != MA_NO_DATA_AVAILABLE) {
                return result;
            }
        }

        if (pState->playback.pAudio != NULL) {
            result = ma_device_process_playback__cosmo(pDevice);
            if (result == MA_SUCCESS) {
                hasProcessedData = MA_TRUE;
            } else if (result != MA_NO_DATA_AVAILABLE) {
                return result;
            }
        }

        if (hasProcessedData || blockingMode == MA_BLOCKING_MODE_NON_BLOCKING) {
            break;
        }

        /* CosmoAudio's blocking poll has no timeout or wakeup. Sleep for a bit. TODO: Replace this with ma_sleep() when integrated into miniaudio proper. */
        delay.tv_sec = 0;
        delay.tv_nsec = 1000000;
        nanosleep(&delay, NULL);
    }

    return MA_SUCCESS;
}

static ma_device_backend_vtable ma_gDeviceBackendVTable_Cosmo =
{
    ma_backend_info__cosmo,
    ma_context_init__cosmo,
    ma_context_uninit__cosmo,
    ma_context_enumerate_devices__cosmo,
    ma_device_init__cosmo,
    ma_device_uninit__cosmo,
    ma_device_start__cosmo,
    ma_device_stop__cosmo,
    ma_device_step__cosmo,
    NULL
};

ma_device_backend_vtable* ma_device_backend_cosmo = &ma_gDeviceBackendVTable_Cosmo;
#else
ma_device_backend_vtable* ma_device_backend_cosmo = NULL;
#endif /* MA_HAS_COSMO */

MA_API ma_device_backend_vtable* ma_cosmo_get_vtable(void)
{
    return ma_device_backend_cosmo;
}

MA_API ma_context_config_cosmo ma_context_config_cosmo_init(void)
{
    ma_context_config_cosmo config;

    MA_COSMO_ZERO_MEMORY(&config, sizeof(config));

    return config;
}

MA_API ma_device_config_cosmo ma_device_config_cosmo_init(void)
{
    ma_device_config_cosmo config;

    MA_COSMO_ZERO_MEMORY(&config, sizeof(config));

    return config;
}

#endif /* miniaudio_backend_cosmo_c */
