/*
Consider this a reference implementation of osaudio. It uses miniaudio under the hood. You can add
this file directly to your source tree, but you may need to update the miniaudio path.

This will use a mutex in osaudio_read() and osaudio_write(). It's a low-contention lock that's only
used for the purpose of osaudio_drain(), but it's still a lock nonetheless. I'm not worrying about
this too much right now because this is just an example implementation, but I might improve on this
at a later date.
*/
#ifndef osaudio_miniaudio_c
#define osaudio_miniaudio_c

#include "osaudio.h"

/*
If you would rather define your own implementation of miniaudio, define OSAUDIO_NO_MINIAUDIO_IMPLEMENTATION. If you do this,
you need to make sure you include the implmeentation before osaudio.c. This would only really be useful if you are wanting
to do a unity build which uses other parts of miniaudio that this file is currently excluding.
*/
#ifndef OSAUDIO_NO_MINIAUDIO_IMPLEMENTATION
#define MA_API static
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"
#endif

struct _osaudio_t
{
    ma_device device;
    osaudio_info_t info;
    osaudio_config_t config;            /* info.configs will point to this. */
    ma_pcm_rb buffer;
    ma_semaphore bufferSemaphore;       /* The semaphore for controlling access to the buffer. The audio thread will release the semaphore. The read and write functions will wait on it. */
    ma_atomic_bool32 isActive;          /* Starts off as false. Set to true when config.buffer_size data has been written in the case of playback, or as soon as osaudio_read() is called in the case of capture. */
    ma_atomic_bool32 isPaused;
    ma_atomic_bool32 isFlushed;         /* When set, activation of the device will flush any data that's currently in the buffer. Defaults to false, and will be set to true in osaudio_drain() and osaudio_flush(). */
    ma_atomic_bool32 xrunDetected;      /* Used for detecting when an xrun has occurred and returning from osaudio_read/write() when OSAUDIO_FLAG_REPORT_XRUN is enabled. */
    ma_spinlock activateLock;           /* Used for starting and stopping the device. Needed because two variables control this - isActive and isPaused. */
    ma_mutex drainLock;                 /* Used for osaudio_drain(). For mutal exclusion between drain() and read()/write(). Technically results in a lock in read()/write(), but not overthinking that since this is just a reference for now. */
};


static ma_bool32   osaudio_g_is_backend_known = MA_FALSE;
static ma_backend  osaudio_g_backend = ma_backend_wasapi;
static ma_context  osaudio_g_context;
static ma_mutex    osaudio_g_context_lock;  /* Only used for device enumeration. Created and destroyed with our context. */
static ma_uint32   osaudio_g_refcount = 0;
static ma_spinlock osaudio_g_lock = 0;


static osaudio_result_t osaudio_result_from_miniaudio(ma_result result)
{
    switch (result)
    {
        case MA_SUCCESS:           return OSAUDIO_SUCCESS;
        case MA_INVALID_ARGS:      return OSAUDIO_INVALID_ARGS;
        case MA_INVALID_OPERATION: return OSAUDIO_INVALID_OPERATION;
        case MA_OUT_OF_MEMORY:     return OSAUDIO_OUT_OF_MEMORY;
        default: return OSAUDIO_ERROR;
    }
}

static ma_format osaudio_format_to_miniaudio(osaudio_format_t format)
{
    switch (format)
    {
        case OSAUDIO_FORMAT_F32: return ma_format_f32;
        case OSAUDIO_FORMAT_U8:  return ma_format_u8;
        case OSAUDIO_FORMAT_S16: return ma_format_s16;
        case OSAUDIO_FORMAT_S24: return ma_format_s24;
        case OSAUDIO_FORMAT_S32: return ma_format_s32;
        default: return ma_format_unknown;
    }
}

static osaudio_format_t osaudio_format_from_miniaudio(ma_format format)
{
    switch (format)
    {
        case ma_format_f32: return OSAUDIO_FORMAT_F32;
        case ma_format_u8:  return OSAUDIO_FORMAT_U8;
        case ma_format_s16: return OSAUDIO_FORMAT_S16;
        case ma_format_s24: return OSAUDIO_FORMAT_S24;
        case ma_format_s32: return OSAUDIO_FORMAT_S32;
        default: return OSAUDIO_FORMAT_UNKNOWN;
    }
}


static osaudio_channel_t osaudio_channel_from_miniaudio(ma_channel channel)
{
    /* Channel positions between here and miniaudio will remain in sync. */
    return (osaudio_channel_t)channel;
}

static ma_channel osaudio_channel_to_miniaudio(osaudio_channel_t channel)
{
    /* Channel positions between here and miniaudio will remain in sync. */
    return (ma_channel)channel;
}


static void osaudio_dummy_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    (void)pDevice;
    (void)pOutput;
    (void)pInput;
    (void)frameCount;
}

static osaudio_result_t osaudio_determine_miniaudio_backend(ma_backend* pBackend, ma_device* pDummyDevice)
{
    ma_device dummyDevice;
    ma_device_config dummyDeviceConfig;
    ma_result result;

    /*
    To do this we initialize a dummy device. We allow the caller to make use of this device as an optimization. This is
    only used by osaudio_enumerate_devices() because that can make use of the context from the dummy device rather than
    having to create it's own. pDummyDevice can be null.
    */
    if (pDummyDevice == NULL) {
        pDummyDevice = &dummyDevice;
    }

    dummyDeviceConfig = ma_device_config_init(ma_device_type_playback);
    dummyDeviceConfig.dataCallback = osaudio_dummy_data_callback;

    result = ma_device_init(NULL, &dummyDeviceConfig, pDummyDevice);
    if (result != MA_SUCCESS || pDummyDevice->pContext->backend == ma_backend_null) {
        /* Failed to open a default playback device. Try capture. */
        if (result == MA_SUCCESS) {
            /* This means we successfully initialize a device, but it's backend is null. It could be that there's no playback devices attached. Try capture. */
            ma_device_uninit(pDummyDevice);
        }

        dummyDeviceConfig = ma_device_config_init(ma_device_type_capture);
        result = ma_device_init(NULL, &dummyDeviceConfig, pDummyDevice);
    }

    if (result != MA_SUCCESS) {
        return osaudio_result_from_miniaudio(result);
    }

    *pBackend = pDummyDevice->pContext->backend;

    /* We're done. */
    if (pDummyDevice == &dummyDevice) {
        ma_device_uninit(&dummyDevice);
    }

    return OSAUDIO_SUCCESS;
}

static osaudio_result_t osaudio_ref_context_nolock()
{
    /* Initialize the global context if necessary. */
    if (osaudio_g_refcount == 0) {
        osaudio_result_t result;

        /* If we haven't got a known context, we'll need to determine it here. */
        if (osaudio_g_is_backend_known == MA_FALSE) {
            result = osaudio_determine_miniaudio_backend(&osaudio_g_backend, NULL);
            if (result != OSAUDIO_SUCCESS) {
                return result;
            }
        }

        result = osaudio_result_from_miniaudio(ma_context_init(&osaudio_g_backend, 1, NULL, &osaudio_g_context));
        if (result != OSAUDIO_SUCCESS) {
            return result;
        }

        /* Need a mutex for device enumeration. */
        ma_mutex_init(&osaudio_g_context_lock);
    }

    osaudio_g_refcount += 1;

    return OSAUDIO_SUCCESS;
}

static osaudio_result_t osaudio_unref_context_nolock()
{
    if (osaudio_g_refcount == 0) {
        return OSAUDIO_INVALID_OPERATION;
    }

    osaudio_g_refcount -= 1;

    /* Uninitialize the context if we don't have any more references. */
    if (osaudio_g_refcount == 0) {
       ma_context_uninit(&osaudio_g_context);
       ma_mutex_uninit(&osaudio_g_context_lock);
    }

    return OSAUDIO_SUCCESS;
}

static ma_context* osaudio_ref_context()
{
    osaudio_result_t result;

    ma_spinlock_lock(&osaudio_g_lock);
    {
        result = osaudio_ref_context_nolock();
    }
    ma_spinlock_unlock(&osaudio_g_lock);

    if (result != OSAUDIO_SUCCESS) {
        return NULL;
    }

    return &osaudio_g_context;
}

static osaudio_result_t osaudio_unref_context()
{
    osaudio_result_t result;

    ma_spinlock_lock(&osaudio_g_lock);
    {
        result = osaudio_unref_context_nolock();
    }
    ma_spinlock_unlock(&osaudio_g_lock);

    return result;
}


static void osaudio_info_from_miniaudio(osaudio_info_t* info, const ma_device_info* infoMA)
{
    unsigned int iNativeConfig;

    /* It just so happens, by absolutely total coincidence, that the size of the ID and name are the same between here and miniaudio. What are the odds?! */
    memcpy(info->id.data, &infoMA->id, sizeof(info->id.data));
    memcpy(info->name, infoMA->name, sizeof(info->name));
        
    info->config_count = (unsigned int)infoMA->nativeDataFormatCount;
    for (iNativeConfig = 0; iNativeConfig < info->config_count; iNativeConfig += 1) {
        unsigned int iChannel;

        info->configs[iNativeConfig].device_id = &info->id;
        info->configs[iNativeConfig].direction = info->direction;
        info->configs[iNativeConfig].format    = osaudio_format_from_miniaudio(infoMA->nativeDataFormats[iNativeConfig].format);
        info->configs[iNativeConfig].channels  = (unsigned int)infoMA->nativeDataFormats[iNativeConfig].channels;
        info->configs[iNativeConfig].rate      = (unsigned int)infoMA->nativeDataFormats[iNativeConfig].sampleRate;

        /* Apparently miniaudio does not report channel positions. I don't know why I'm not doing that. */
        for (iChannel = 0; iChannel < info->configs[iNativeConfig].channels; iChannel += 1) {
            info->configs[iNativeConfig].channel_map[iChannel] = OSAUDIO_CHANNEL_NONE;
        }
    }
}

static osaudio_result_t osaudio_enumerate_nolock(unsigned int* count, osaudio_info_t** info, ma_context* pContext)
{
    osaudio_result_t result;
    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    ma_uint32 iInfo;
    size_t allocSize;
    osaudio_info_t* pRunningInfo;
    osaudio_config_t* pRunningConfig;

    /* We now need to retrieve the device information from miniaudio. */
    result = osaudio_result_from_miniaudio(ma_context_get_devices(pContext, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount));
    if (result != OSAUDIO_SUCCESS) {
        osaudio_unref_context();
        return result;
    }

    /*
    Because the caller needs to free the returned pointer it's important that we keep it all in one allocation. Because there can be
    a variable number of native configs we'll have to compute the size of the allocation first, and then do a second pass to fill
    out the data.
    */
    allocSize = ((size_t)playbackCount + (size_t)captureCount) * sizeof(osaudio_info_t);

    /* Now we need to iterate over each playback and capture device and add up the number of native configs. */
    for (iInfo = 0; iInfo < playbackCount; iInfo += 1) {
        ma_context_get_device_info(pContext, ma_device_type_playback, &pPlaybackInfos[iInfo].id, &pPlaybackInfos[iInfo]);
        allocSize += pPlaybackInfos[iInfo].nativeDataFormatCount * sizeof(osaudio_config_t);
    }
    for (iInfo = 0; iInfo < captureCount; iInfo += 1) {
        ma_context_get_device_info(pContext, ma_device_type_capture, &pCaptureInfos[iInfo].id, &pCaptureInfos[iInfo]);
        allocSize += pCaptureInfos[iInfo].nativeDataFormatCount * sizeof(osaudio_config_t);
    }

    /* Now that we know the size of the allocation we can allocate it. */
    *info = (osaudio_info_t*)calloc(1, allocSize);
    if (*info == NULL) {
        osaudio_unref_context();
        return OSAUDIO_OUT_OF_MEMORY;
    }

    pRunningInfo   = *info;
    pRunningConfig = (osaudio_config_t*)(((unsigned char*)*info) + (((size_t)playbackCount + (size_t)captureCount) * sizeof(osaudio_info_t)));

    for (iInfo = 0; iInfo < playbackCount; iInfo += 1) {
        pRunningInfo->direction = OSAUDIO_OUTPUT;
        pRunningInfo->configs   = pRunningConfig;
        osaudio_info_from_miniaudio(pRunningInfo, &pPlaybackInfos[iInfo]);

        pRunningConfig += pRunningInfo->config_count;
        pRunningInfo   += 1;
    }

    for (iInfo = 0; iInfo < captureCount; iInfo += 1) {
        pRunningInfo->direction = OSAUDIO_INPUT;
        pRunningInfo->configs   = pRunningConfig;
        osaudio_info_from_miniaudio(pRunningInfo, &pPlaybackInfos[iInfo]);

        pRunningConfig += pRunningInfo->config_count;
        pRunningInfo   += 1;
    }

    *count = (unsigned int)(playbackCount + captureCount);

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_enumerate(unsigned int* count, osaudio_info_t** info)
{
    osaudio_result_t result;
    ma_context* pContext = NULL;
    
    if (count != NULL) {
        *count = 0;
    }
    if (info != NULL) {
        *info = NULL;
    }

    if (count == NULL || info == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    pContext = osaudio_ref_context();
    if (pContext == NULL) {
        return OSAUDIO_ERROR;
    }

    ma_mutex_lock(&osaudio_g_context_lock);
    {
        result = osaudio_enumerate_nolock(count, info, pContext);
    }
    ma_mutex_unlock(&osaudio_g_context_lock);

    /* We're done. We can now return. */
    osaudio_unref_context();
    return result;
}


void osaudio_config_init(osaudio_config_t* config, osaudio_direction_t direction)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->direction = direction;
}


static void osaudio_data_callback_playback(osaudio_t audio, void* pOutput, ma_uint32 frameCount)
{
    /*
    If there's content in the buffer, read from it and release the semaphore. There needs to be a whole frameCount chunk
    in the buffer so we can keep everything in nice clean chunks. When we read from the buffer, we release a semaphore
    which will allow the main thread to write more data to the buffer.
    */
    ma_uint32 framesToRead;
    ma_uint32 framesProcessed;
    void* pBuffer;

    framesToRead = ma_pcm_rb_available_read(&audio->buffer);
    if (framesToRead > frameCount) {
        framesToRead = frameCount;
    }

    framesProcessed = framesToRead;

    /* For robustness we should run this in a loop in case the buffer wraps around. */
    while (frameCount > 0) {
        framesToRead = frameCount;

        ma_pcm_rb_acquire_read(&audio->buffer, &framesToRead, &pBuffer);
        if (framesToRead == 0) {
            break;
        }

        memcpy(pOutput, pBuffer, framesToRead * ma_get_bytes_per_frame(audio->device.playback.format, audio->device.playback.channels));
        ma_pcm_rb_commit_read(&audio->buffer, framesToRead);

        frameCount -= framesToRead;
        pOutput = ((unsigned char*)pOutput) + (framesToRead * ma_get_bytes_per_frame(audio->device.playback.format, audio->device.playback.channels));
    }

    /* Make sure we release the semaphore if we ended up reading anything. */
    if (framesProcessed > 0) {
        ma_semaphore_release(&audio->bufferSemaphore);
    }

    if (frameCount > 0) {
        /* Underrun. Pad with silence. */
        ma_silence_pcm_frames(pOutput, frameCount, audio->device.playback.format, audio->device.playback.channels);
        ma_atomic_bool32_set(&audio->xrunDetected, MA_TRUE);
    }
}

static void osaudio_data_callback_capture(osaudio_t audio, const void* pInput, ma_uint32 frameCount)
{
    /* If there's space in the buffer, write to it and release the semaphore. The semaphore is only released on full-chunk boundaries. */
    ma_uint32 framesToWrite;
    ma_uint32 framesProcessed;
    void* pBuffer;

    framesToWrite = ma_pcm_rb_available_write(&audio->buffer);
    if (framesToWrite > frameCount) {
        framesToWrite = frameCount;
    }

    framesProcessed = framesToWrite;

    while (frameCount > 0) {
        framesToWrite = frameCount;

        ma_pcm_rb_acquire_write(&audio->buffer, &framesToWrite, &pBuffer);
        if (framesToWrite == 0) {
            break;
        }

        memcpy(pBuffer, pInput, framesToWrite * ma_get_bytes_per_frame(audio->device.capture.format, audio->device.capture.channels));
        ma_pcm_rb_commit_write(&audio->buffer, framesToWrite);

        frameCount -= framesToWrite;
        pInput = ((unsigned char*)pInput) + (framesToWrite * ma_get_bytes_per_frame(audio->device.capture.format, audio->device.capture.channels));
    }

    /* Make sure we release the semaphore if we ended up reading anything. */
    if (framesProcessed > 0) {
        ma_semaphore_release(&audio->bufferSemaphore);
    }

    if (frameCount > 0) {
        /* Overrun. Not enough room to move our input data into the buffer. */
        ma_atomic_bool32_set(&audio->xrunDetected, MA_TRUE);
    }
}

static void osaudio_nofication_callback(const ma_device_notification* pNotification)
{
    osaudio_t audio = (osaudio_t)pNotification->pDevice->pUserData;

    if (audio->config.notification != NULL) {
        osaudio_notification_t notification;

        switch (pNotification->type)
        {
            case ma_device_notification_type_started:
            {
                notification.type = OSAUDIO_NOTIFICATION_STARTED;
            } break;
            case ma_device_notification_type_stopped:
            {
                notification.type = OSAUDIO_NOTIFICATION_STOPPED;
            } break;
            case ma_device_notification_type_rerouted:
            {
                notification.type = OSAUDIO_NOTIFICATION_REROUTED;
            } break;
            case ma_device_notification_type_interruption_began:
            {
                notification.type = OSAUDIO_NOTIFICATION_INTERRUPTION_BEGIN;
            } break;
            case ma_device_notification_type_interruption_ended:
            {
                notification.type = OSAUDIO_NOTIFICATION_INTERRUPTION_END;
            } break;
        }

        audio->config.notification(audio->config.user_data, &notification);
    }
}

static void osaudio_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    osaudio_t audio = (osaudio_t)pDevice->pUserData;

    if (audio->info.direction == OSAUDIO_OUTPUT) {
        osaudio_data_callback_playback(audio, pOutput, frameCount);
    } else {
        osaudio_data_callback_capture(audio, pInput, frameCount);
    }
}

osaudio_result_t osaudio_open(osaudio_t* audio, osaudio_config_t* config)
{
    osaudio_result_t result;
    ma_context* pContext = NULL;
    ma_device_config deviceConfig;
    ma_device_info deviceInfo;
    int periodCount = 2;
    unsigned int iChannel;

    if (audio != NULL) {
        *audio = NULL;  /* Safety. */
    }

    if (audio == NULL || config == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    pContext = osaudio_ref_context();   /* Will be unreferenced in osaudio_close(). */
    if (pContext == NULL) {
        return OSAUDIO_ERROR;
    }

    *audio = (osaudio_t)calloc(1, sizeof(**audio));
    if (*audio == NULL) {
        osaudio_unref_context();
        return OSAUDIO_OUT_OF_MEMORY;
    }

    if (config->direction == OSAUDIO_OUTPUT) {
        deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format   = osaudio_format_to_miniaudio(config->format);
        deviceConfig.playback.channels = (ma_uint32)config->channels;

        if (config->channel_map[0] != OSAUDIO_CHANNEL_NONE) {
            for (iChannel = 0; iChannel < config->channels; iChannel += 1) {
                deviceConfig.playback.pChannelMap[iChannel] = osaudio_channel_to_miniaudio(config->channel_map[iChannel]);
            }
        }
    } else {
        deviceConfig = ma_device_config_init(ma_device_type_capture);
        deviceConfig.capture.format    = osaudio_format_to_miniaudio(config->format);
        deviceConfig.capture.channels  = (ma_uint32)config->channels;

        if (config->channel_map[0] != OSAUDIO_CHANNEL_NONE) {
            for (iChannel = 0; iChannel < config->channels; iChannel += 1) {
                deviceConfig.capture.pChannelMap[iChannel] = osaudio_channel_to_miniaudio(config->channel_map[iChannel]);
            }
        }
    }

    deviceConfig.sampleRate = (ma_uint32)config->rate;

    /* If the buffer size is 0, we'll default to 10ms. */
    deviceConfig.periodSizeInFrames = (ma_uint32)config->buffer_size;
    if (deviceConfig.periodSizeInFrames == 0) {
        deviceConfig.periodSizeInMilliseconds = 10;
    }

    deviceConfig.dataCallback = osaudio_data_callback;
    deviceConfig.pUserData    = *audio;

    if ((config->flags & OSAUDIO_FLAG_NO_REROUTING) != 0) {
        deviceConfig.wasapi.noAutoStreamRouting = MA_TRUE;
    }

    if (config->notification != NULL) {
        deviceConfig.notificationCallback = osaudio_nofication_callback;
    }

    result = osaudio_result_from_miniaudio(ma_device_init(pContext, &deviceConfig, &((*audio)->device)));
    if (result != OSAUDIO_SUCCESS) {
        free(*audio);
        osaudio_unref_context();
        return result;
    }

    /* The input config needs to be updated with actual values. */
    if (config->direction == OSAUDIO_OUTPUT) {
        config->format = osaudio_format_from_miniaudio((*audio)->device.playback.format);
        config->channels = (unsigned int)(*audio)->device.playback.channels;

        for (iChannel = 0; iChannel < config->channels; iChannel += 1) {
            config->channel_map[iChannel] = osaudio_channel_from_miniaudio((*audio)->device.playback.channelMap[iChannel]);
        }
    } else {
        config->format = osaudio_format_from_miniaudio((*audio)->device.capture.format);
        config->channels = (unsigned int)(*audio)->device.capture.channels;

        for (iChannel = 0; iChannel < config->channels; iChannel += 1) {
            config->channel_map[iChannel] = osaudio_channel_from_miniaudio((*audio)->device.capture.channelMap[iChannel]);
        }
    }

    config->rate = (unsigned int)(*audio)->device.sampleRate;

    if (deviceConfig.periodSizeInFrames == 0) {
        if (config->direction == OSAUDIO_OUTPUT) {
            config->buffer_size = (int)(*audio)->device.playback.internalPeriodSizeInFrames;
        } else {
            config->buffer_size = (int)(*audio)->device.capture.internalPeriodSizeInFrames;
        }
    }


    /* The device object needs to have a it's local info built. We can get the ID and name from miniaudio. */
    result = osaudio_result_from_miniaudio(ma_device_get_info(&(*audio)->device, (*audio)->device.type, &deviceInfo));
    if (result == MA_SUCCESS) {
        memcpy((*audio)->info.id.data, &deviceInfo.id, sizeof((*audio)->info.id.data));
        memcpy((*audio)->info.name, deviceInfo.name, sizeof((*audio)->info.name));
    }

    (*audio)->info.direction    = config->direction;
    (*audio)->info.config_count = 1;
    (*audio)->info.configs      = &(*audio)->config;
    (*audio)->config            = *config;
    (*audio)->config.device_id  = &(*audio)->info.id;


    /* We need a ring buffer. */
    result = osaudio_result_from_miniaudio(ma_pcm_rb_init(osaudio_format_to_miniaudio(config->format), (ma_uint32)config->channels, (ma_uint32)config->buffer_size * periodCount, NULL, NULL, &(*audio)->buffer));
    if (result != OSAUDIO_SUCCESS) {
        ma_device_uninit(&(*audio)->device);
        free(*audio);
        osaudio_unref_context();
        return result;
    }

    /* Now we need a semaphore to control access to the ring buffer to to block read/write when necessary. */
    result = osaudio_result_from_miniaudio(ma_semaphore_init((config->direction == OSAUDIO_OUTPUT) ? periodCount : 0, &(*audio)->bufferSemaphore));
    if (result != OSAUDIO_SUCCESS) {
        ma_pcm_rb_uninit(&(*audio)->buffer);
        ma_device_uninit(&(*audio)->device);
        free(*audio);
        osaudio_unref_context();
        return result;
    }

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_close(osaudio_t audio)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    ma_device_uninit(&audio->device);
    osaudio_unref_context();

    return OSAUDIO_SUCCESS;
}

static void osaudio_activate(osaudio_t audio)
{
    ma_spinlock_lock(&audio->activateLock);
    {
        if (ma_atomic_bool32_get(&audio->isActive) == MA_FALSE) {
            ma_atomic_bool32_set(&audio->isActive, MA_TRUE);

            /* If we need to flush, do so now before starting the device. */
            if (ma_atomic_bool32_get(&audio->isFlushed) == MA_TRUE) {
                ma_pcm_rb_reset(&audio->buffer);
                ma_atomic_bool32_set(&audio->isFlushed, MA_FALSE);
            }

            /* If we're not paused, start the device. */
            if (ma_atomic_bool32_get(&audio->isPaused) == MA_FALSE) {
                ma_device_start(&audio->device);
            }
        }
    }
    ma_spinlock_unlock(&audio->activateLock);
}

osaudio_result_t osaudio_write(osaudio_t audio, const void* data, unsigned int frame_count)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    ma_mutex_lock(&audio->drainLock);
    {
        /* Don't return until everything has been written. */
        while (frame_count > 0) {
            ma_uint32 framesToWrite = frame_count;
            ma_uint32 framesAvailableInBuffer;

            /* There should be enough data available in the buffer now, but check anyway. */
            framesAvailableInBuffer = ma_pcm_rb_available_write(&audio->buffer);
            if (framesAvailableInBuffer > 0) {
                void* pBuffer;

                if (framesToWrite > framesAvailableInBuffer) {
                    framesToWrite = framesAvailableInBuffer;
                }

                ma_pcm_rb_acquire_write(&audio->buffer, &framesToWrite, &pBuffer);
                {
                    ma_copy_pcm_frames(pBuffer, data, framesToWrite, audio->device.playback.format, audio->device.playback.channels);
                }
                ma_pcm_rb_commit_write(&audio->buffer, framesToWrite);

                frame_count -= (unsigned int)framesToWrite;
                data = (const void*)((const unsigned char*)data + (framesToWrite * ma_get_bytes_per_frame(audio->device.playback.format, audio->device.playback.channels)));

                if (framesToWrite > 0) {
                    osaudio_activate(audio);
                }
            } else {
                /* If we get here it means there's not enough data available in the buffer. We need to wait for more. */
                ma_semaphore_wait(&audio->bufferSemaphore);

                /* If we're not active it probably means we've flushed. This write needs to be aborted. */
                if (ma_atomic_bool32_get(&audio->isActive) == MA_FALSE) {
                    break;
                }
            }
        }
    }
    ma_mutex_unlock(&audio->drainLock);

    if ((audio->config.flags & OSAUDIO_FLAG_REPORT_XRUN) != 0) {
        if (ma_atomic_bool32_get(&audio->xrunDetected)) {
            ma_atomic_bool32_set(&audio->xrunDetected, MA_FALSE);
            return OSAUDIO_XRUN;
        }
    }

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_read(osaudio_t audio, void* data, unsigned int frame_count)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    ma_mutex_lock(&audio->drainLock);
    {
        while (frame_count > 0) {
            ma_uint32 framesToRead = frame_count;
            ma_uint32 framesAvailableInBuffer;

            /* There should be enough data available in the buffer now, but check anyway. */
            framesAvailableInBuffer = ma_pcm_rb_available_read(&audio->buffer);
            if (framesAvailableInBuffer > 0) {
                void* pBuffer;

                if (framesToRead > framesAvailableInBuffer) {
                    framesToRead = framesAvailableInBuffer;
                }

                ma_pcm_rb_acquire_read(&audio->buffer, &framesToRead, &pBuffer);
                {
                    ma_copy_pcm_frames(data, pBuffer, framesToRead, audio->device.capture.format, audio->device.capture.channels);
                }
                ma_pcm_rb_commit_read(&audio->buffer, framesToRead);

                frame_count -= (unsigned int)framesToRead;
                data = (void*)((unsigned char*)data + (framesToRead * ma_get_bytes_per_frame(audio->device.capture.format, audio->device.capture.channels)));
            } else {
                /* Activate the device from the get go or else we'll never end up capturing anything. */
                osaudio_activate(audio);

                /* If we get here it means there's not enough data available in the buffer. We need to wait for more. */
                ma_semaphore_wait(&audio->bufferSemaphore);

                /* If we're not active it probably means we've flushed. This read needs to be aborted. */
                if (ma_atomic_bool32_get(&audio->isActive) == MA_FALSE) {
                    break;
                }
            }
        }
    }
    ma_mutex_unlock(&audio->drainLock);

    if ((audio->config.flags & OSAUDIO_FLAG_REPORT_XRUN) != 0) {
        if (ma_atomic_bool32_get(&audio->xrunDetected)) {
            ma_atomic_bool32_set(&audio->xrunDetected, MA_FALSE);
            return OSAUDIO_XRUN;
        }
    }

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_drain(osaudio_t audio)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    /* This cannot be called while the device is in a paused state. */
    if (ma_atomic_bool32_get(&audio->isPaused)) {
        return OSAUDIO_DEVICE_STOPPED;
    }

    /* For capture we want to stop the device immediately or else we won't ever drain the buffer because miniaudio will be constantly filling it. */
    if (audio->info.direction == OSAUDIO_INPUT) {
        ma_device_stop(&audio->device);
    }

    /*
    Mark the device as inactive *before* releasing the semaphore. When read/write completes waiting
    on the semaphore, they'll check this flag and abort.
    */
    ma_atomic_bool32_set(&audio->isActive, MA_FALSE);

    /*
    Again in capture mode, we need to release the semaphore before waiting for the drain lock because
    there's a chance read() will be waiting on the semaphore and will need to be woken up in order for
    it to be given to chance to return.
    */
    if (audio->info.direction == OSAUDIO_INPUT) {
        ma_semaphore_release(&audio->bufferSemaphore);
    }

    /* Now we need to wait for any pending reads or writes to complete. */
    ma_mutex_lock(&audio->drainLock);
    {
        /* No processing should be happening on the buffer at this point. Wait for miniaudio to consume the buffer. */
        while (ma_pcm_rb_available_read(&audio->buffer) > 0) {
            ma_sleep(1);
        }

        /*
        At this point the buffer should be empty, and we shouldn't be in any read or write calls. If
        it's a playback device, we'll want to stop the device. There's no need to release the semaphore.
        */
        if (audio->info.direction == OSAUDIO_OUTPUT) {
            ma_device_stop(&audio->device);
        }
    }
    ma_mutex_unlock(&audio->drainLock);

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_flush(osaudio_t audio)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    /*
    First stop the device. This ensures the miniaudio background thread doesn't try modifying the
    buffer from under us while we're trying to flush it.
    */
    ma_device_stop(&audio->device);

    /*
    Mark the device as inactive *before* releasing the semaphore. When read/write completes waiting
    on the semaphore, they'll check this flag and abort.
    */
    ma_atomic_bool32_set(&audio->isActive, MA_FALSE);

    /*
    Release the semaphore after marking the device as inactive. This needs to be released in order
    to wakeup osaudio_read() and osaudio_write().
    */
    ma_semaphore_release(&audio->bufferSemaphore);

    /*
    The buffer should only be modified by osaudio_read() or osaudio_write(), or the miniaudio
    background thread. Therefore, we don't actually clear the buffer here. Instead we'll clear it
    in osaudio_activate(), depending on whether or not the below flag is set.
    */
    ma_atomic_bool32_set(&audio->isFlushed, MA_TRUE);

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_pause(osaudio_t audio)
{
    osaudio_result_t result = OSAUDIO_SUCCESS;

    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    ma_spinlock_lock(&audio->activateLock);
    {
        if (ma_atomic_bool32_get(&audio->isPaused) == MA_FALSE) {
            ma_atomic_bool32_set(&audio->isPaused, MA_TRUE);

            /* No need to stop the device if it's not active. */
            if (ma_atomic_bool32_get(&audio->isActive)) {
                result = osaudio_result_from_miniaudio(ma_device_stop(&audio->device));
            }
        }
    }
    ma_spinlock_unlock(&audio->activateLock);

    return result;
}

osaudio_result_t osaudio_resume(osaudio_t audio)
{
    osaudio_result_t result = OSAUDIO_SUCCESS;

    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    ma_spinlock_lock(&audio->activateLock);
    {
        if (ma_atomic_bool32_get(&audio->isPaused)) {
            ma_atomic_bool32_set(&audio->isPaused, MA_FALSE);

            /* Don't start the device unless it's active. */
            if (ma_atomic_bool32_get(&audio->isActive)) {
                result = osaudio_result_from_miniaudio(ma_device_start(&audio->device));
            }
        }
    }
    ma_spinlock_unlock(&audio->activateLock);

    return result;
}

unsigned int osaudio_get_avail(osaudio_t audio)
{
    if (audio == NULL) {
        return 0;
    }

    if (audio->info.direction == OSAUDIO_OUTPUT) {
        return ma_pcm_rb_available_write(&audio->buffer);
    } else {
        return ma_pcm_rb_available_read(&audio->buffer);
    }
}

const osaudio_info_t* osaudio_get_info(osaudio_t audio)
{
    if (audio == NULL) {
        return NULL;
    }

    return &audio->info;
}

#endif  /* osaudio_miniaudio_c */
