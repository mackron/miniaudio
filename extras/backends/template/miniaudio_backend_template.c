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

static void ma_backend_info__template(ma_device_backend_info* pBackendInfo)
{
    (void)pBackendInfo;
}

static ma_result ma_context_init__template(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    (void)pContext;
    (void)pContextBackendConfig;
    (void)ppContextState;

    return MA_NOT_IMPLEMENTED;
}

static void ma_context_uninit__template(ma_context* pContext)
{
    (void)pContext;
}

static ma_result ma_context_enumerate_devices__template(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    (void)pContext;
    (void)callback;
    (void)pCallbackUserData;

    return MA_NOT_IMPLEMENTED;
}

static ma_result ma_device_init__template(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    (void)pDevice;
    (void)pDeviceBackendConfig;
    (void)pDescriptorPlayback;
    (void)pDescriptorCapture;
    (void)ppDeviceState;

    return MA_NOT_IMPLEMENTED;
}

static void ma_device_uninit__template(ma_device* pDevice)
{
    (void)pDevice;
}

static ma_result ma_device_start__template(ma_device* pDevice)
{
    (void)pDevice;

    return MA_NOT_IMPLEMENTED;
}

static ma_result ma_device_stop__template(ma_device* pDevice)
{
    (void)pDevice;

    return MA_NOT_IMPLEMENTED;
}

static ma_result ma_device_step__template(ma_device* pDevice, ma_blocking_mode blockingMode)
{
    (void)pDevice;
    (void)blockingMode;

    return MA_NOT_IMPLEMENTED;
}

static void ma_device_wakeup__template(ma_device* pDevice)
{
    (void)pDevice;
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
