/*
This implements a full-featured SDL2 backend. It's intentionally built using the same paradigms as the built-in backends in order to make
it suitable as a solid basis for a custom implementation. The SDL2 backend can be disabled with MA_NO_SDL, exactly like the built-in
backends. It supports both runtime and compile-time linking and respects the MA_NO_RUNTIME_LINKING option. It also works on Emscripten
which requires the `-s USE_SDL=2` option.
*/
#ifndef miniaudio_backend_sdl_c
#define miniaudio_backend_sdl_c

/* Include miniaudio.h if we're not including this file after the implementation. */
#if !defined(MINIAUDIO_IMPLEMENTATION) && !defined(MA_IMPLEMENTATION)
#include "../../../miniaudio.h"
#endif

#include "backend_sdl.h"

#include <string.h> /* memset() */

/* Support SDL on everything. */
#define MA_SUPPORT_SDL

/*
Only enable SDL if it's hasn't been explicitly disabled (MA_NO_SDL) or enabled (MA_ENABLE_SDL with
MA_ENABLE_ONLY_SPECIFIC_BACKENDS) and it's supported at compile time (MA_SUPPORT_SDL).
*/
#if defined(MA_SUPPORT_SDL) && !defined(MA_NO_SDL) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_SDL))
    #define MA_HAS_SDL
#endif

/* SDL headers are necessary if using compile-time linking. Necessary for Emscripten. */
#if defined(MA_HAS_SDL)
    #if defined(MA_NO_RUNTIME_LINKING) || defined(MA_EMSCRIPTEN)
        #ifdef __has_include
            #ifdef MA_EMSCRIPTEN
                #if !__has_include(<SDL/SDL_audio.h>)
                    #undef MA_HAS_SDL
                #endif
            #else
                #if !__has_include(<SDL2/SDL_audio.h>)
                    #undef MA_HAS_SDL
                #endif
            #endif
        #endif
    #endif
#endif

/* Don't compile in the SDL backend if it's been disabled. */
#if defined(MA_HAS_SDL)
#define MA_SDL_INIT_AUDIO                       0x00000010
#define MA_AUDIO_U8                             0x0008
#define MA_AUDIO_S16                            0x8010
#define MA_AUDIO_S32                            0x8020
#define MA_AUDIO_F32                            0x8120
#define MA_SDL_AUDIO_ALLOW_FREQUENCY_CHANGE     0x00000001
#define MA_SDL_AUDIO_ALLOW_FORMAT_CHANGE        0x00000002
#define MA_SDL_AUDIO_ALLOW_CHANNELS_CHANGE      0x00000004
#define MA_SDL_AUDIO_ALLOW_ANY_CHANGE           (MA_SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | MA_SDL_AUDIO_ALLOW_FORMAT_CHANGE | MA_SDL_AUDIO_ALLOW_CHANNELS_CHANGE)

typedef struct
{
    ma_handle hSDL; /* A handle to the SDL2 shared object. We dynamically load function pointers at runtime so we can avoid linking. */
    ma_proc SDL_InitSubSystem;
    ma_proc SDL_QuitSubSystem;
    ma_proc SDL_GetNumAudioDevices;
    ma_proc SDL_GetAudioDeviceName;
    ma_proc SDL_CloseAudioDevice;
    ma_proc SDL_OpenAudioDevice;
    ma_proc SDL_PauseAudioDevice;
} ma_context_data_sdl;

typedef struct
{
    int deviceIDPlayback;
    int deviceIDCapture;
} ma_device_data_sdl;

/* If we are linking at compile time we'll just #include SDL.h. Otherwise we can just redeclare some stuff to avoid the need for development packages to be installed. */
#ifdef MA_NO_RUNTIME_LINKING
    #define SDL_MAIN_HANDLED
    #ifdef MA_EMSCRIPTEN
        #include <SDL/SDL.h>
    #else
        #include <SDL2/SDL.h>
    #endif

    typedef SDL_AudioCallback   MA_SDL_AudioCallback;
    typedef SDL_AudioSpec       MA_SDL_AudioSpec;
    typedef SDL_AudioFormat     MA_SDL_AudioFormat;
    typedef SDL_AudioDeviceID   MA_SDL_AudioDeviceID;
#else
    typedef void (* MA_SDL_AudioCallback)(void* userdata, ma_uint8* stream, int len);
    typedef ma_uint16 MA_SDL_AudioFormat;
    typedef ma_uint32 MA_SDL_AudioDeviceID;

    typedef struct MA_SDL_AudioSpec
    {
        int freq;
        MA_SDL_AudioFormat format;
        ma_uint8 channels;
        ma_uint8 silence;
        ma_uint16 samples;
        ma_uint16 padding;
        ma_uint32 size;
        MA_SDL_AudioCallback callback;
        void* userdata;
    } MA_SDL_AudioSpec;
#endif

typedef int                  (* MA_PFN_SDL_InitSubSystem)(ma_uint32 flags);
typedef void                 (* MA_PFN_SDL_QuitSubSystem)(ma_uint32 flags);
typedef int                  (* MA_PFN_SDL_GetNumAudioDevices)(int iscapture);
typedef const char*          (* MA_PFN_SDL_GetAudioDeviceName)(int index, int iscapture);
typedef void                 (* MA_PFN_SDL_CloseAudioDevice)(MA_SDL_AudioDeviceID dev);
typedef MA_SDL_AudioDeviceID (* MA_PFN_SDL_OpenAudioDevice)(const char* device, int iscapture, const MA_SDL_AudioSpec* desired, MA_SDL_AudioSpec* obtained, int allowed_changes);
typedef void                 (* MA_PFN_SDL_PauseAudioDevice)(MA_SDL_AudioDeviceID dev, int pause_on);

MA_SDL_AudioFormat ma_format_to_sdl(ma_format format)
{
    switch (format)
    {
        case ma_format_unknown: return 0;
        case ma_format_u8:      return MA_AUDIO_U8;
        case ma_format_s16:     return MA_AUDIO_S16;
        case ma_format_s24:     return MA_AUDIO_S32;  /* Closest match. */
        case ma_format_s32:     return MA_AUDIO_S32;
        case ma_format_f32:     return MA_AUDIO_F32;
        default:                return 0;
    }
}

ma_format ma_format_from_sdl(MA_SDL_AudioFormat format)
{
    switch (format)
    {
        case MA_AUDIO_U8:  return ma_format_u8;
        case MA_AUDIO_S16: return ma_format_s16;
        case MA_AUDIO_S32: return ma_format_s32;
        case MA_AUDIO_F32: return ma_format_f32;
        default:           return ma_format_unknown;
    }
}

static ma_result ma_context_enumerate_devices__sdl(void* pUserData, ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    ma_context_data_sdl* pContextDataSDL;
    ma_bool32 isTerminated = MA_FALSE;
    ma_bool32 cbResult;
    int iDevice;

    (void)pUserData;

    pContextDataSDL = (ma_context_data_sdl*)pContext->pBackendData;

    /* Playback */
    if (!isTerminated) {
        int deviceCount = ((MA_PFN_SDL_GetNumAudioDevices)pContextDataSDL->SDL_GetNumAudioDevices)(0);
        for (iDevice = 0; iDevice < deviceCount; ++iDevice) {
            ma_device_info deviceInfo;
            memset(&deviceInfo, 0, sizeof(deviceInfo));

            deviceInfo.id.custom.i = iDevice;
            ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), ((MA_PFN_SDL_GetAudioDeviceName)pContextDataSDL->SDL_GetAudioDeviceName)(iDevice, 0), (size_t)-1);

            if (iDevice == 0) {
                deviceInfo.isDefault = MA_TRUE;
            }

            cbResult = callback(pContext, ma_device_type_playback, &deviceInfo, pCallbackUserData);
            if (cbResult == MA_FALSE) {
                isTerminated = MA_TRUE;
                break;
            }
        }
    }

    /* Capture */
    if (!isTerminated) {
        int deviceCount = ((MA_PFN_SDL_GetNumAudioDevices)pContextDataSDL->SDL_GetNumAudioDevices)(1);
        for (iDevice = 0; iDevice < deviceCount; ++iDevice) {
            ma_device_info deviceInfo;
            memset(&deviceInfo, 0, sizeof(deviceInfo));

            deviceInfo.id.custom.i = iDevice;
            ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), ((MA_PFN_SDL_GetAudioDeviceName)pContextDataSDL->SDL_GetAudioDeviceName)(iDevice, 1), (size_t)-1);

            if (iDevice == 0) {
                deviceInfo.isDefault = MA_TRUE;
            }

            cbResult = callback(pContext, ma_device_type_capture, &deviceInfo, pCallbackUserData);
            if (cbResult == MA_FALSE) {
                isTerminated = MA_TRUE;
                break;
            }
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_context_get_device_info__sdl(void* pUserData, ma_context* pContext, ma_device_type deviceType, const ma_device_id* pDeviceID, ma_device_info* pDeviceInfo)
{
    ma_context_data_sdl* pContextDataSDL;

#if !defined(__EMSCRIPTEN__)
    MA_SDL_AudioSpec desiredSpec;
    MA_SDL_AudioSpec obtainedSpec;
    MA_SDL_AudioDeviceID tempDeviceID;
    const char* pDeviceName;
#endif

    (void)pUserData;

    pContextDataSDL = (ma_context_data_sdl*)pContext->pBackendData;

    if (pDeviceID == NULL) {
        if (deviceType == ma_device_type_playback) {
            pDeviceInfo->id.custom.i = 0;
            ma_strncpy_s(pDeviceInfo->name, sizeof(pDeviceInfo->name), "Default Playback Device", (size_t)-1);
        } else {
            pDeviceInfo->id.custom.i = 0;
            ma_strncpy_s(pDeviceInfo->name, sizeof(pDeviceInfo->name), "Default Capture Device", (size_t)-1);
        }
    } else {
        pDeviceInfo->id.custom.i = pDeviceID->custom.i;
        ma_strncpy_s(pDeviceInfo->name, sizeof(pDeviceInfo->name), ((MA_PFN_SDL_GetAudioDeviceName)pContextDataSDL->SDL_GetAudioDeviceName)(pDeviceID->custom.i, (deviceType == ma_device_type_playback) ? 0 : 1), (size_t)-1);
    }

    if (pDeviceInfo->id.custom.i == 0) {
        pDeviceInfo->isDefault = MA_TRUE;
    }

    /*
    To get an accurate idea on the backend's native format we need to open the device. Not ideal, but it's the only way. An
    alternative to this is to report all channel counts, sample rates and formats, but that doesn't offer a good representation
    of the device's _actual_ ideal format.
    
    Note: With Emscripten, it looks like non-zero values need to be specified for desiredSpec. Whatever is specified in
    desiredSpec will be used by SDL since it uses it just does it's own format conversion internally. Therefore, from what
    I can tell, there's no real way to know the device's actual format which means I'm just going to fall back to the full
    range of channels and sample rates on Emscripten builds.
    */
#if defined(__EMSCRIPTEN__)
    /* Good practice to prioritize the best format first so that the application can use the first data format as their chosen one if desired. */
    pDeviceInfo->nativeDataFormatCount = 3;
    pDeviceInfo->nativeDataFormats[0].format     = ma_format_s16;
    pDeviceInfo->nativeDataFormats[0].channels   = 0;   /* All channel counts supported. */
    pDeviceInfo->nativeDataFormats[0].sampleRate = 0;   /* All sample rates supported. */
    pDeviceInfo->nativeDataFormats[0].flags      = 0;
    pDeviceInfo->nativeDataFormats[1].format     = ma_format_s32;
    pDeviceInfo->nativeDataFormats[1].channels   = 0;   /* All channel counts supported. */
    pDeviceInfo->nativeDataFormats[1].sampleRate = 0;   /* All sample rates supported. */
    pDeviceInfo->nativeDataFormats[1].flags      = 0;
    pDeviceInfo->nativeDataFormats[2].format     = ma_format_u8;
    pDeviceInfo->nativeDataFormats[2].channels   = 0;   /* All channel counts supported. */
    pDeviceInfo->nativeDataFormats[2].sampleRate = 0;   /* All sample rates supported. */
    pDeviceInfo->nativeDataFormats[2].flags      = 0;
#else
    memset(&desiredSpec, 0, sizeof(desiredSpec));

    pDeviceName = NULL;
    if (pDeviceID != NULL) {
        pDeviceName = ((MA_PFN_SDL_GetAudioDeviceName)pContextDataSDL->SDL_GetAudioDeviceName)(pDeviceID->custom.i, (deviceType == ma_device_type_playback) ? 0 : 1);
    }

    tempDeviceID = ((MA_PFN_SDL_OpenAudioDevice)pContextDataSDL->SDL_OpenAudioDevice)(pDeviceName, (deviceType == ma_device_type_playback) ? 0 : 1, &desiredSpec, &obtainedSpec, MA_SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (tempDeviceID == 0) {
        ma_log_postf(ma_context_get_log(pContext), MA_LOG_LEVEL_ERROR, "Failed to open SDL device.");
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    ((MA_PFN_SDL_CloseAudioDevice)pContextDataSDL->SDL_CloseAudioDevice)(tempDeviceID);

    /* Only reporting a single native data format. It'll be whatever SDL decides is the best. */
    pDeviceInfo->nativeDataFormatCount = 1;
    pDeviceInfo->nativeDataFormats[0].format     = ma_format_from_sdl(obtainedSpec.format);
    pDeviceInfo->nativeDataFormats[0].channels   = obtainedSpec.channels;
    pDeviceInfo->nativeDataFormats[0].sampleRate = obtainedSpec.freq;
    pDeviceInfo->nativeDataFormats[0].flags      = 0;

    /* If miniaudio does not support the format, just use f32 as the native format (SDL will do the necessary conversions for us). */
    if (pDeviceInfo->nativeDataFormats[0].format == ma_format_unknown) {
        pDeviceInfo->nativeDataFormats[0].format = ma_format_f32;
    }
#endif  /* __EMSCRIPTEN__ */

    return MA_SUCCESS;
}


void ma_audio_callback_capture__sdl(void* pUserData, ma_uint8* pBuffer, int bufferSizeInBytes)
{
    ma_device* pDevice = (ma_device*)pUserData;
    ma_device_handle_backend_data_callback(pDevice, NULL, pBuffer, (ma_uint32)bufferSizeInBytes / ma_get_bytes_per_frame(pDevice->capture.internalFormat, pDevice->capture.internalChannels));
}

void ma_audio_callback_playback__sdl(void* pUserData, ma_uint8* pBuffer, int bufferSizeInBytes)
{
    ma_device* pDevice = (ma_device*)pUserData;
    ma_device_handle_backend_data_callback(pDevice, pBuffer, NULL, (ma_uint32)bufferSizeInBytes / ma_get_bytes_per_frame(pDevice->playback.internalFormat, pDevice->playback.internalChannels));
}

static ma_result ma_device_init_internal__sdl(ma_device* pDevice, const ma_device_config* pConfig, ma_device_descriptor* pDescriptor)
{
    ma_context_data_sdl* pContextDataSDL;
    ma_device_data_sdl* pDeviceDataSDL;
    const ma_device_config_sdl* pDeviceConfigSDL;
    MA_SDL_AudioSpec desiredSpec;
    MA_SDL_AudioSpec obtainedSpec;
    const char* pDeviceName;
    int deviceID;

    pContextDataSDL = (ma_context_data_sdl*)pDevice->pContext->pBackendData;
    pDeviceDataSDL = (ma_device_data_sdl*)pDevice->pBackendData;

    /* Grab the SDL backend config. This is not currently used. */
    pDeviceConfigSDL = (const ma_device_config_sdl*)ma_device_config_find_custom_backend_config(pConfig, MA_DEVICE_BACKEND_VTABLE_SDL);
    (void)pDeviceConfigSDL;


    /*
    SDL is a little bit awkward with specifying the buffer size, You need to specify the size of the buffer in frames, but since we may
    have requested a period size in milliseconds we'll need to convert, which depends on the sample rate. But there's a possibility that
    the sample rate just set to 0, which indicates that the native sample rate should be used. There's no practical way to calculate this
    that I can think of right now so I'm just using MA_DEFAULT_SAMPLE_RATE.
    */
    if (pDescriptor->sampleRate == 0) {
        pDescriptor->sampleRate = MA_DEFAULT_SAMPLE_RATE;
    }

    /*
    When determining the period size, you need to take defaults into account. This is how the size of the period should be determined.

        1) If periodSizeInFrames is not 0, use periodSizeInFrames; else
        2) If periodSizeInMilliseconds is not 0, use periodSizeInMilliseconds; else
        3) If both periodSizeInFrames and periodSizeInMilliseconds is 0, use the backend's default. If the backend does not allow a default
           buffer size, use a default value of MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS_LOW_LATENCY or 
           MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS_CONSERVATIVE depending on the value of pConfig->performanceProfile.

    Note that options 2 and 3 require knowledge of the sample rate in order to convert it to a frame count. You should try to keep the
    calculation of the period size as accurate as possible, but sometimes it's just not practical so just use whatever you can.

    A helper function called ma_calculate_buffer_size_in_frames_from_descriptor() is available to do all of this for you which is what
    we'll be using here.
    */
    pDescriptor->periodSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDescriptor, pDescriptor->sampleRate, pConfig->performanceProfile);

    /* SDL wants the buffer size to be a power of 2 for some reason. */
    if (pDescriptor->periodSizeInFrames > 32768) {
        pDescriptor->periodSizeInFrames = 32768;
    } else {
        pDescriptor->periodSizeInFrames = ma_next_power_of_2(pDescriptor->periodSizeInFrames);
    }


    /* We now have enough information to set up the device. */
    memset(&desiredSpec, 0, sizeof(desiredSpec));
    desiredSpec.freq     = (int)pDescriptor->sampleRate;
    desiredSpec.format   = ma_format_to_sdl(pDescriptor->format);
    desiredSpec.channels = (ma_uint8)pDescriptor->channels;
    desiredSpec.samples  = (ma_uint16)pDescriptor->periodSizeInFrames;
    desiredSpec.callback = (pConfig->deviceType == ma_device_type_capture) ? ma_audio_callback_capture__sdl : ma_audio_callback_playback__sdl;
    desiredSpec.userdata = pDevice;

    /* We'll fall back to f32 if we don't have an appropriate mapping between SDL and miniaudio. */
    if (desiredSpec.format == 0) {
        desiredSpec.format = MA_AUDIO_F32;
    }

    pDeviceName = NULL;
    if (pDescriptor->pDeviceID != NULL) {
        pDeviceName = ((MA_PFN_SDL_GetAudioDeviceName)pContextDataSDL->SDL_GetAudioDeviceName)(pDescriptor->pDeviceID->custom.i, (pConfig->deviceType == ma_device_type_playback) ? 0 : 1);
    }

    deviceID = ((MA_PFN_SDL_OpenAudioDevice)pContextDataSDL->SDL_OpenAudioDevice)(pDeviceName, (pConfig->deviceType == ma_device_type_playback) ? 0 : 1, &desiredSpec, &obtainedSpec, MA_SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (deviceID == 0) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to open SDL2 device.");
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    if (pConfig->deviceType == ma_device_type_playback) {
        pDeviceDataSDL->deviceIDPlayback = deviceID;
    } else {
        pDeviceDataSDL->deviceIDCapture = deviceID;
    }

    /* The descriptor needs to be updated with our actual settings. */
    pDescriptor->format             = ma_format_from_sdl(obtainedSpec.format);
    pDescriptor->channels           = obtainedSpec.channels;
    pDescriptor->sampleRate         = (ma_uint32)obtainedSpec.freq;
    ma_channel_map_init_standard(ma_standard_channel_map_default, pDescriptor->channelMap, ma_countof(pDescriptor->channelMap), pDescriptor->channels);
    pDescriptor->periodSizeInFrames = obtainedSpec.samples;
    pDescriptor->periodCount        = 1;    /* SDL doesn't use the notion of period counts, so just set to 1. */

    return MA_SUCCESS;
}

static ma_result ma_device_init__sdl(void* pUserData, ma_device* pDevice, const ma_device_config* pConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture)
{
    ma_context_data_sdl* pContextDataSDL;
    ma_device_data_sdl* pDeviceDataSDL;
    ma_result result;

    (void)pUserData;

    pContextDataSDL = (ma_context_data_sdl*)pDevice->pContext->pBackendData;
    
    /* SDL does not support loopback mode, so must return MA_DEVICE_TYPE_NOT_SUPPORTED if it's requested. */
    if (pConfig->deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    /* We need to allocate our backend-specific data. */
    pDeviceDataSDL = (ma_device_data_sdl*)ma_calloc(sizeof(*pDeviceDataSDL), &pDevice->pContext->allocationCallbacks);
    if (pDeviceDataSDL == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    pDevice->pBackendData = pDeviceDataSDL;

    if (pConfig->deviceType == ma_device_type_capture || pConfig->deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__sdl(pDevice, pConfig, pDescriptorCapture);
        if (result != MA_SUCCESS) {
            ma_free(pDeviceDataSDL, &pDevice->pContext->allocationCallbacks);
            return result;
        }
    }

    if (pConfig->deviceType == ma_device_type_playback || pConfig->deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__sdl(pDevice, pConfig, pDescriptorPlayback);
        if (result != MA_SUCCESS) {
            if (pConfig->deviceType == ma_device_type_duplex) {
                ((MA_PFN_SDL_CloseAudioDevice)pContextDataSDL->SDL_CloseAudioDevice)(pDeviceDataSDL->deviceIDCapture);
            }

            ma_free(pDeviceDataSDL, &pDevice->pContext->allocationCallbacks);
            return result;
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_device_uninit__sdl(void* pUserData, ma_device* pDevice)
{
    ma_context_data_sdl* pContextDataSDL;
    ma_device_data_sdl* pDeviceDataSDL;

    (void)pUserData;

    pContextDataSDL = (ma_context_data_sdl*)pDevice->pContext->pBackendData;
    pDeviceDataSDL  = (ma_device_data_sdl*)pDevice->pBackendData;

    if (pDevice->type == ma_device_type_capture || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_CloseAudioDevice)pContextDataSDL->SDL_CloseAudioDevice)(pDeviceDataSDL->deviceIDCapture);
    }

    if (pDevice->type == ma_device_type_playback || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_CloseAudioDevice)pContextDataSDL->SDL_CloseAudioDevice)(pDeviceDataSDL->deviceIDCapture);
    }

    ma_free(pDeviceDataSDL, &pDevice->pContext->allocationCallbacks);

    return MA_SUCCESS;
}

static ma_result ma_device_start__sdl(void* pUserData, ma_device* pDevice)
{
    ma_context_data_sdl* pContextDataSDL;
    ma_device_data_sdl* pDeviceDataSDL;

    (void)pUserData;

    pContextDataSDL = (ma_context_data_sdl*)pDevice->pContext->pBackendData;
    pDeviceDataSDL  = (ma_device_data_sdl*)pDevice->pBackendData;

    if (pDevice->type == ma_device_type_capture || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_PauseAudioDevice)pContextDataSDL->SDL_PauseAudioDevice)(pDeviceDataSDL->deviceIDCapture, 0);
    }

    if (pDevice->type == ma_device_type_playback || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_PauseAudioDevice)pContextDataSDL->SDL_PauseAudioDevice)(pDeviceDataSDL->deviceIDPlayback, 0);
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stop__sdl(void* pUserData, ma_device* pDevice)
{
    ma_context_data_sdl* pContextDataSDL;
    ma_device_data_sdl* pDeviceDataSDL;

    (void)pUserData;

    pContextDataSDL = (ma_context_data_sdl*)pDevice->pContext->pBackendData;
    pDeviceDataSDL  = (ma_device_data_sdl*)pDevice->pBackendData;

    if (pDevice->type == ma_device_type_capture || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_PauseAudioDevice)pContextDataSDL->SDL_PauseAudioDevice)(pDeviceDataSDL->deviceIDCapture, 1);
    }

    if (pDevice->type == ma_device_type_playback || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_PauseAudioDevice)pContextDataSDL->SDL_PauseAudioDevice)(pDeviceDataSDL->deviceIDPlayback, 1);
    }

    return MA_SUCCESS;
}

static ma_result ma_context_uninit__sdl(void* pUserData, ma_context* pContext)
{
    ma_context_data_sdl* pContextDataSDL;

    (void)pUserData;

    pContextDataSDL = (ma_context_data_sdl*)pContext->pBackendData;

    ((MA_PFN_SDL_QuitSubSystem)pContextDataSDL->SDL_QuitSubSystem)(MA_SDL_INIT_AUDIO);

    /* Close the handle to the SDL shared object last. */
    ma_dlclose(ma_context_get_log(pContext), pContextDataSDL->hSDL);
    pContextDataSDL->hSDL = NULL;

    ma_free(pContextDataSDL, &pContext->allocationCallbacks);

    return MA_SUCCESS;
}

static ma_result ma_context_init__sdl(void* pUserData, ma_context* pContext, const ma_context_config* pConfig)
{
    ma_context_data_sdl* pContextDataSDL;
    const ma_context_config_sdl* pContextConfigSDL;
    int resultSDL;
    
#ifndef MA_NO_RUNTIME_LINKING
    /* We'll use a list of possible shared object names for easier extensibility. */
    size_t iName;
    const char* pSDLNames[] = {
#if defined(_WIN32)
        "SDL2.dll"
#elif defined(__APPLE__)
        "SDL2.framework/SDL2"
#else
        "libSDL2-2.0.so.0"
#endif
    };

    (void)pUserData;

    /* Grab the config. This is not currently being used. */
    pContextConfigSDL = (const ma_context_config_sdl*)ma_context_config_find_custom_backend_config(pConfig, MA_DEVICE_BACKEND_VTABLE_SDL);
    (void)pContextConfigSDL;


    /* Allocate our SDL-specific context data. */
    pContextDataSDL = (ma_context_data_sdl*)ma_calloc(sizeof(*pContextDataSDL), &pContext->allocationCallbacks);
    if (pContextDataSDL == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    pContext->pBackendData = pContextDataSDL;


    /* Check if we have SDL2 installed somewhere. If not it's not usable and we need to abort. */
    for (iName = 0; iName < ma_countof(pSDLNames); iName += 1) {
        pContextDataSDL->hSDL = ma_dlopen(ma_context_get_log(pContext), pSDLNames[iName]);
        if (pContextDataSDL->hSDL != NULL) {
            break;
        }
    }

    if (pContextDataSDL->hSDL == NULL) {
        ma_free(pContextDataSDL, &pContext->allocationCallbacks);
        return MA_NO_BACKEND;   /* SDL2 could not be loaded. */
    }

    /* Now that we have the handle to the shared object we can go ahead and load some function pointers. */
    pContextDataSDL->SDL_InitSubSystem      = ma_dlsym(ma_context_get_log(pContext), pContextDataSDL->hSDL, "SDL_InitSubSystem");
    pContextDataSDL->SDL_QuitSubSystem      = ma_dlsym(ma_context_get_log(pContext), pContextDataSDL->hSDL, "SDL_QuitSubSystem");
    pContextDataSDL->SDL_GetNumAudioDevices = ma_dlsym(ma_context_get_log(pContext), pContextDataSDL->hSDL, "SDL_GetNumAudioDevices");
    pContextDataSDL->SDL_GetAudioDeviceName = ma_dlsym(ma_context_get_log(pContext), pContextDataSDL->hSDL, "SDL_GetAudioDeviceName");
    pContextDataSDL->SDL_CloseAudioDevice   = ma_dlsym(ma_context_get_log(pContext), pContextDataSDL->hSDL, "SDL_CloseAudioDevice");
    pContextDataSDL->SDL_OpenAudioDevice    = ma_dlsym(ma_context_get_log(pContext), pContextDataSDL->hSDL, "SDL_OpenAudioDevice");
    pContextDataSDL->SDL_PauseAudioDevice   = ma_dlsym(ma_context_get_log(pContext), pContextDataSDL->hSDL, "SDL_PauseAudioDevice");
#else
    pContextDataSDL->SDL_InitSubSystem      = (ma_proc)SDL_InitSubSystem;
    pContextDataSDL->SDL_QuitSubSystem      = (ma_proc)SDL_QuitSubSystem;
    pContextDataSDL->SDL_GetNumAudioDevices = (ma_proc)SDL_GetNumAudioDevices;
    pContextDataSDL->SDL_GetAudioDeviceName = (ma_proc)SDL_GetAudioDeviceName;
    pContextDataSDL->SDL_CloseAudioDevice   = (ma_proc)SDL_CloseAudioDevice;
    pContextDataSDL->SDL_OpenAudioDevice    = (ma_proc)SDL_OpenAudioDevice;
    pContextDataSDL->SDL_PauseAudioDevice   = (ma_proc)SDL_PauseAudioDevice;
#endif  /* MA_NO_RUNTIME_LINKING */

    resultSDL = ((MA_PFN_SDL_InitSubSystem)pContextDataSDL->SDL_InitSubSystem)(MA_SDL_INIT_AUDIO);
    if (resultSDL != 0) {
        ma_dlclose(ma_context_get_log(pContext), pContextDataSDL->hSDL);
        ma_free(pContextDataSDL, &pContext->allocationCallbacks);
        return MA_ERROR;
    }

    return MA_SUCCESS;
}


static ma_device_backend_vtable ma_gDeviceBackendVTable_SDL =
{
    ma_context_init__sdl,
    ma_context_uninit__sdl,
    ma_context_enumerate_devices__sdl,
    ma_context_get_device_info__sdl,
    ma_device_init__sdl,
    ma_device_uninit__sdl,
    ma_device_start__sdl,
    ma_device_stop__sdl,
    NULL,   /* onDeviceRead */
    NULL,   /* onDeviceWrite */
    NULL,   /* onDeviceDataLoop */
    NULL,   /* onDeviceDataLoopWakeup */
    NULL    /* onDeviceGetInfo */
};

const ma_device_backend_vtable* MA_DEVICE_BACKEND_VTABLE_SDL = &ma_gDeviceBackendVTable_SDL;
#else
const ma_device_backend_vtable* MA_DEVICE_BACKEND_VTABLE_SDL = NULL;
#endif  /* MA_HAS_SDL */


MA_API ma_context_config_sdl ma_context_config_sdl_init(void)
{
    ma_context_config_sdl config;

    memset(&config, 0, sizeof(config));

    return config;
}

MA_API ma_device_config_sdl ma_device_config_sdl_init(void)
{
    ma_device_config_sdl config;

    memset(&config, 0, sizeof(config));

    return config;
}


#endif  /* miniaudio_backend_sdl_c */
