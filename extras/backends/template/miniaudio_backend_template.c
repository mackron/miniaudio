#ifndef miniaudio_backend_template_c
#define miniaudio_backend_template_c

/* Do not include this in your backend. It's only used to validate the template build. Needed for MA_ZERO_OBJECT(). */
#include "../../../miniaudio.c"

#include "miniaudio_backend_template.h"

/* Wrap this in a #ifdef/#endif depending on the build environment. */
#define MA_SUPPORT_TEMPLATE

#if defined(MA_SUPPORT_TEMPLATE) && !defined(MA_NO_TEMPLATE) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_TEMPLATE))
    #define MA_HAS_TEMPLATE
#endif

#if defined(MA_HAS_TEMPLATE)
typedef struct ma_context_state_template
{
    int _unused;
} ma_context_state_template;

typedef struct ma_device_state_template
{
    int _unused;
} ma_device_state_template;


static ma_context_state_template* ma_context_get_backend_state__template(ma_context* pContext)
{
    return (ma_context_state_template*)ma_context_get_backend_state(pContext);
}

static ma_device_state_template* ma_device_get_backend_state__template(ma_device* pDevice)
{
    return (ma_device_state_template*)ma_device_get_backend_state(pDevice);
}


static void ma_backend_info__template(ma_device_backend_info* pBackendInfo)
{
    pBackendInfo->pName = "Template";
}

static ma_result ma_context_init__template(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    ma_context_state_template* pContextStateTemplate;
    const ma_context_config_template* pContextConfigTemplate = (ma_context_config_template*)pContextBackendConfig;

    pContextStateTemplate = (ma_context_state_template*)ma_calloc(sizeof(*pContextStateTemplate), ma_context_get_allocation_callbacks(pContext));
    if (pContextStateTemplate == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    /* Do any context-level initialization here. */
    (void)pContextConfigTemplate;
    (void)pContext;

    *ppContextState = pContextStateTemplate;

    return MA_SUCCESS;
}

static void ma_context_uninit__template(ma_context* pContext)
{
    ma_context_state_template* pContextStateTemplate = ma_context_get_backend_state__template(pContext);

    /* Do any context-level uninitialization here. */

    ma_free(pContextStateTemplate, ma_context_get_allocation_callbacks(pContext));
}

static ma_result ma_context_enumerate_devices__template(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    ma_context_state_template* pContextStateTemplate = ma_context_get_backend_state__template(pContext);
    ma_device_info deviceInfo;
    ma_device_enumeration_result enumerationResult;

    /* This example is only outputting a default playback and capture device. Modify this as required. */
    (void)pContextStateTemplate;

    /* Playback. */
    MA_ZERO_OBJECT(&deviceInfo);
    deviceInfo.isDefault = MA_TRUE;
    deviceInfo.id.custom.i = 0;
    ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), "Default Playback Device", (size_t)-1);

    /* Add a native format for each natively supported sample format. The channel count and sample rates are ranges. The format is the key. */
    ma_device_info_add_native_data_format(&deviceInfo, ma_format_s16, 1, 2, 44100, 48000);
    ma_device_info_add_native_data_format(&deviceInfo, ma_format_f32, 1, 2, 44100, 48000);

    /* If the callback has requested that we abort enumeration we need to respect it. */
    enumerationResult = callback(ma_device_type_playback, &deviceInfo, pCallbackUserData);
    if (enumerationResult == MA_DEVICE_ENUMERATION_ABORT) {
        return MA_SUCCESS;
    }

    /* Capture. */
    MA_ZERO_OBJECT(&deviceInfo);
    deviceInfo.isDefault = MA_TRUE;
    deviceInfo.id.custom.i = 0;
    ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), "Default Capture Device", (size_t)-1);

    /* Add a native format for each natively supported sample format. The channel count and sample rates are ranges. The format is the key. */
    ma_device_info_add_native_data_format(&deviceInfo, ma_format_s16, 1, 2, 44100, 48000);
    ma_device_info_add_native_data_format(&deviceInfo, ma_format_f32, 1, 2, 44100, 48000);

    /* If the callback has requested that we abort enumeration we need to respect it. */
    enumerationResult = callback(ma_device_type_capture, &deviceInfo, pCallbackUserData);
    if (enumerationResult == MA_DEVICE_ENUMERATION_ABORT) {
        return MA_SUCCESS;
    }

    /* Enumeration done. */
    return MA_SUCCESS;
}

static ma_result ma_device_init__template(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    ma_device_state_template* pDeviceStateTemplate;
    ma_device_config_template* pDeviceConfigTemplate = (ma_device_config_template*)pDeviceBackendConfig;
    ma_context_state_template* pContextStateTemplate = ma_context_get_backend_state__template(ma_device_get_context(pDevice));
    ma_device_config_template defaultConfig;
    ma_device_type deviceType = ma_device_get_type(pDevice);

    /* Use a default config if one was not provided. This is not mandated by miniaudio, but it's good practice. */
    if (pDeviceConfigTemplate == NULL) {
        defaultConfig = ma_device_config_template_init();
        pDeviceConfigTemplate = &defaultConfig;
    }

    /* Return an error for any unsupported device types. */
    if (deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    pDeviceStateTemplate = (ma_device_state_template*)ma_calloc(sizeof(*pDeviceStateTemplate), ma_device_get_allocation_callbacks(pDevice));
    if (pDeviceStateTemplate == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    /* Not using the context state for this example. */
    (void)pContextStateTemplate;

    /* No backend-specific config for this example. */
    (void)pDeviceConfigTemplate;

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex || deviceType == ma_device_type_loopback) {
        /* Do any backend initialization for the capture side here. Update pDescriptorCapture with the actual internal format, channels, rate and period size. */
        ma_format format;
        ma_uint32 channels;
        ma_uint32 sampleRate;
        ma_uint32 periodSizeInFrames;

        /*
        On input pDescriptorCapture contains what miniaudio would like, if possible. On output, you set
        it to what the backend actually supports. You would typically try to make it as close as possible,
        but it's ultimately up to the backend to choose what it gives you.
        */

        /* Negotiate the format. */
        format = pDescriptorCapture->format;
        if (format != ma_format_f32) {
            format = ma_format_f32;
        }

        /* Negotiate the channel count. */
        channels = pDescriptorCapture->channels;
        if (channels != 1 && channels != 2) {
            channels = 2;
        }

        /* Negotiate the sample rate. */
        sampleRate = pDescriptorCapture->sampleRate;
        if (sampleRate != 44100 && sampleRate != 48000) {
            sampleRate = 48000;
        }

        /*
        Negotiate the period size. You will want to use ma_calculate_buffer_size_in_frames_from_descriptor() to
        calculate an appropriate period size based on standardized miniaudio conventions as a base point, and then
        massage it based on the requirements of the backend. This example is just clamping it between 1024 and
        16384 just for demonstration purposes.
        */
        periodSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDescriptorCapture, sampleRate);
        if (periodSizeInFrames < 1024) {
            periodSizeInFrames = 1024;
        }

        /* Initialize any backend-specific stuff here. */

        /* Update the descriptor with the actual internal settings. */
        pDescriptorCapture->format             = format;
        pDescriptorCapture->channels           = channels;
        pDescriptorCapture->sampleRate         = sampleRate;
        pDescriptorCapture->periodSizeInFrames = periodSizeInFrames;
        pDescriptorCapture->periodCount        = 1; /* Just set this 1 if you're unsure. Set it to 2 if you are using a double-buffering technique. */
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        /* This works exactly the same as capture, but for playback. */
        ma_format format;
        ma_uint32 channels;
        ma_uint32 sampleRate;
        ma_uint32 periodSizeInFrames;

        format = pDescriptorPlayback->format;
        if (format != ma_format_f32) {
            format = ma_format_f32;
        }

        channels = pDescriptorPlayback->channels;
        if (channels != 1 && channels != 2) {
            channels = 2;
        }

        sampleRate = pDescriptorPlayback->sampleRate;
        if (sampleRate != 44100 && sampleRate != 48000) {
            sampleRate = 48000;
        }

        periodSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDescriptorPlayback, sampleRate);
        if (periodSizeInFrames < 1024) {
            periodSizeInFrames = 1024;
        }

        /* Initialize any backend-specific stuff here. */

        /* Update the descriptor with the actual internal settings. */
        pDescriptorPlayback->format             = format;
        pDescriptorPlayback->channels           = channels;
        pDescriptorPlayback->sampleRate         = sampleRate;
        pDescriptorPlayback->periodSizeInFrames = periodSizeInFrames;
        pDescriptorPlayback->periodCount        = 1;
    }

    *ppDeviceState = pDeviceStateTemplate;

    return MA_SUCCESS;
}

static void ma_device_uninit__template(ma_device* pDevice)
{
    ma_device_state_template* pDeviceStateTemplate = ma_device_get_backend_state__template(pDevice);

    /* Do any device-level uninitialization here. */

    ma_free(pDeviceStateTemplate, ma_device_get_allocation_callbacks(pDevice));
}

static ma_result ma_device_start__template(ma_device* pDevice)
{
    ma_device_state_template* pDeviceStateTemplate = ma_device_get_backend_state__template(pDevice);
    ma_device_type deviceType = ma_device_get_type(pDevice);

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex || deviceType == ma_device_type_loopback) {
        /* Start the capture side. */
        (void)pDeviceStateTemplate;
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        /* Start the playback side. */
        (void)pDeviceStateTemplate;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stop__template(ma_device* pDevice)
{
    ma_device_state_template* pDeviceStateTemplate = ma_device_get_backend_state__template(pDevice);
    ma_device_type deviceType = ma_device_get_type(pDevice);

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex || deviceType == ma_device_type_loopback) {
        /* Stop the capture side. */
        (void)pDeviceStateTemplate;
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        /* Stop the playback side. */
        (void)pDeviceStateTemplate;
    }

    return MA_NOT_IMPLEMENTED;
}

static ma_result ma_device_step__template(ma_device* pDevice, ma_blocking_mode blockingMode)
{
    ma_device_state_template* pDeviceStateTemplate = ma_device_get_backend_state__template(pDevice);

    /*
    When blockingMode is MA_BLOCKING_MODE_BLOCKING, this function should block until data has been processed. Otherwise
    if it's set to MA_BLOCKING_MODE_NON_BLOCKING it should try processing any data if ready and then immediately return,
    regardless of whether or not any data was processed. Use ma_device_handle_backend_data_callback() to process data.

    The code below is an example of the general idea. You don't have to follow this exact design.
    */
    if (blockingMode == MA_BLOCKING_MODE_BLOCKING) {
        /* Wait here. */
        (void)pDeviceStateTemplate;
    }

    /* If after waiting the device has been stopped don't process any audio data. */
    if (!ma_device_is_started(pDevice)) {
        return MA_DEVICE_NOT_STARTED;
    }

    /* Call ma_device_handle_backend_data_callback() at some point. */

    return MA_SUCCESS;
}

static void ma_device_wakeup__template(ma_device* pDevice)
{
    ma_device_state_template* pDeviceStateTemplate = ma_device_get_backend_state__template(pDevice);

    /* Do whatever needs to be done to wakeup the step function. It's OK to just do nothing here so long as your backend will return from a blocking step in a reasonable amount of time. */
    (void)pDeviceStateTemplate;
}

static ma_device_backend_vtable ma_gDeviceBackendVTable_Template =
{
    ma_backend_info__template,
    ma_context_init__template,
    ma_context_uninit__template,
    ma_context_enumerate_devices__template,
    ma_device_init__template,
    ma_device_uninit__template,
    ma_device_start__template,
    ma_device_stop__template,
    ma_device_step__template,
    ma_device_wakeup__template
};

ma_device_backend_vtable* ma_device_backend_template = &ma_gDeviceBackendVTable_Template;
#else
ma_device_backend_vtable* ma_device_backend_template = NULL;
#endif  /* MA_HAS_TEMPLATE */

MA_API ma_device_backend_vtable* ma_template_get_vtable(void)
{
    return ma_device_backend_template;
}

MA_API ma_context_config_template ma_context_config_template_init(void)
{
    ma_context_config_template config;

    MA_ZERO_OBJECT(&config);

    return config;
}

MA_API ma_device_config_template ma_device_config_template_init(void)
{
    ma_device_config_template config;

    MA_ZERO_OBJECT(&config);

    return config;
}

#endif  /* miniaudio_backend_template_c */
