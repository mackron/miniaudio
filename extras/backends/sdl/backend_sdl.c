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
#include <assert.h>

#ifndef MA_SDL_ASSERT
#define MA_SDL_ASSERT(cond) assert(cond)
#endif

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

/* If we are linking at compile time we'll just #include SDL.h. Otherwise we can just redeclare some stuff to avoid the need for development packages to be installed. */
#ifdef MA_NO_RUNTIME_LINKING
    #define SDL_MAIN_HANDLED
    #ifdef MA_EMSCRIPTEN
        #include <SDL/SDL.h>
    #else
        #include <SDL2/SDL.h>
    #endif

    typedef SDL_AudioCallback MA_SDL_AudioCallback;
    typedef SDL_AudioSpec     MA_SDL_AudioSpec;
    typedef SDL_AudioFormat   MA_SDL_AudioFormat;
    typedef SDL_AudioDeviceID MA_SDL_AudioDeviceID;
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

typedef int                  (* MA_PFN_SDL_InitSubSystem      )(ma_uint32 flags);
typedef void                 (* MA_PFN_SDL_QuitSubSystem      )(ma_uint32 flags);
typedef int                  (* MA_PFN_SDL_GetNumAudioDevices )(int iscapture);
typedef int                  (* MA_PFN_SDL_GetDefaultAudioInfo)(char** name, MA_SDL_AudioSpec* spec, int iscapture);
typedef int                  (* MA_PFN_SDL_GetAudioDeviceSpec )(int index, int iscapture, MA_SDL_AudioSpec* spec);
typedef const char*          (* MA_PFN_SDL_GetAudioDeviceName )(int index, int iscapture);
typedef void                 (* MA_PFN_SDL_CloseAudioDevice   )(MA_SDL_AudioDeviceID dev);
typedef MA_SDL_AudioDeviceID (* MA_PFN_SDL_OpenAudioDevice    )(const char* device, int iscapture, const MA_SDL_AudioSpec* desired, MA_SDL_AudioSpec* obtained, int allowed_changes);
typedef void                 (* MA_PFN_SDL_PauseAudioDevice   )(MA_SDL_AudioDeviceID dev, int pause_on);

typedef struct
{
    ma_handle hSDL; /* A handle to the SDL2 shared object. We dynamically load function pointers at runtime so we can avoid linking. */
    MA_PFN_SDL_InitSubSystem       SDL_InitSubSystem;
    MA_PFN_SDL_QuitSubSystem       SDL_QuitSubSystem;
    MA_PFN_SDL_GetNumAudioDevices  SDL_GetNumAudioDevices;
    MA_PFN_SDL_GetDefaultAudioInfo SDL_GetDefaultAudioInfo;
    MA_PFN_SDL_GetAudioDeviceSpec  SDL_GetAudioDeviceSpec;
    MA_PFN_SDL_GetAudioDeviceName  SDL_GetAudioDeviceName;
    MA_PFN_SDL_CloseAudioDevice    SDL_CloseAudioDevice;
    MA_PFN_SDL_OpenAudioDevice     SDL_OpenAudioDevice;
    MA_PFN_SDL_PauseAudioDevice    SDL_PauseAudioDevice;
} ma_context_state_sdl;

typedef struct
{
    ma_device_state_async async;
    struct
    {
        int deviceID;
    } capture;
    struct
    {
        int deviceID;
    } playback;
} ma_device_state_sdl;


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


static ma_context_state_sdl* ma_context_get_backend_state__sdl(ma_context* pContext)
{
    return (ma_context_state_sdl*)ma_context_get_backend_state(pContext);
}

static ma_device_state_sdl* ma_device_get_backend_state__sdl(ma_device* pDevice)
{
    return (ma_device_state_sdl*)ma_device_get_backend_state(pDevice);
}


static ma_result ma_device_step__sdl(ma_device* pDevice);


static void ma_backend_info__sdl(ma_device_backend_info* pBackendInfo)
{
    MA_SDL_ASSERT(pBackendInfo != NULL);
    pBackendInfo->pName = "SDL2";
}

static ma_result ma_context_init__sdl(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    ma_context_state_sdl* pContextStateSDL;
    const ma_context_config_sdl* pContextConfigSDL = (ma_context_config_sdl*)pContextBackendConfig;
    ma_log* pLog = ma_context_get_log(pContext);
    int resultSDL;

    /* The context config is not currently being used for this backend. */
    (void)pContextConfigSDL;


    /* Allocate our SDL-specific context data. */
    pContextStateSDL = (ma_context_state_sdl*)ma_calloc(sizeof(*pContextStateSDL), ma_context_get_allocation_callbacks(pContext));
    if (pContextStateSDL == NULL) {
        return MA_OUT_OF_MEMORY;
    }
    
    #ifndef MA_NO_RUNTIME_LINKING
    {
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

        /* Check if we have SDL2 installed somewhere. If not it's not usable and we need to abort. */
        for (iName = 0; iName < ma_countof(pSDLNames); iName += 1) {
            pContextStateSDL->hSDL = ma_dlopen(pLog, pSDLNames[iName]);
            if (pContextStateSDL->hSDL != NULL) {
                break;
            }
        }

        if (pContextStateSDL->hSDL == NULL) {
            ma_free(pContextStateSDL, ma_context_get_allocation_callbacks(pContext));
            return MA_NO_BACKEND;   /* SDL2 could not be loaded. */
        }

        /* Now that we have the handle to the shared object we can go ahead and load some function pointers. */
        pContextStateSDL->SDL_InitSubSystem       = (MA_PFN_SDL_InitSubSystem      )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_InitSubSystem");
        pContextStateSDL->SDL_QuitSubSystem       = (MA_PFN_SDL_QuitSubSystem      )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_QuitSubSystem");
        pContextStateSDL->SDL_GetNumAudioDevices  = (MA_PFN_SDL_GetNumAudioDevices )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetNumAudioDevices");
        pContextStateSDL->SDL_GetDefaultAudioInfo = (MA_PFN_SDL_GetDefaultAudioInfo)ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetDefaultAudioInfo");
        pContextStateSDL->SDL_GetAudioDeviceSpec  = (MA_PFN_SDL_GetAudioDeviceSpec )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioDeviceSpec");
        pContextStateSDL->SDL_GetAudioDeviceName  = (MA_PFN_SDL_GetAudioDeviceName )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioDeviceName");
        pContextStateSDL->SDL_CloseAudioDevice    = (MA_PFN_SDL_CloseAudioDevice   )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_CloseAudioDevice");
        pContextStateSDL->SDL_OpenAudioDevice     = (MA_PFN_SDL_OpenAudioDevice    )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_OpenAudioDevice");
        pContextStateSDL->SDL_PauseAudioDevice    = (MA_PFN_SDL_PauseAudioDevice   )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_PauseAudioDevice");
    }
    #else
    {
        pContextStateSDL->SDL_InitSubSystem       = SDL_InitSubSystem;
        pContextStateSDL->SDL_QuitSubSystem       = SDL_QuitSubSystem;
        pContextStateSDL->SDL_GetNumAudioDevices  = SDL_GetNumAudioDevices;
        #ifndef __EMSCRIPTEN__
        pContextStateSDL->SDL_GetDefaultAudioInfo = SDL_GetDefaultAudioInfo;
        pContextStateSDL->SDL_GetAudioDeviceSpec  = SDL_GetAudioDeviceSpec;
        #else
        pContextStateSDL->SDL_GetDefaultAudioInfo = NULL;
        pContextStateSDL->SDL_GetAudioDeviceSpec  = NULL;
        #endif
        pContextStateSDL->SDL_GetAudioDeviceName  = SDL_GetAudioDeviceName;
        pContextStateSDL->SDL_CloseAudioDevice    = SDL_CloseAudioDevice;
        pContextStateSDL->SDL_OpenAudioDevice     = SDL_OpenAudioDevice;
        pContextStateSDL->SDL_PauseAudioDevice    = SDL_PauseAudioDevice;
    }
    #endif  /* MA_NO_RUNTIME_LINKING */

    resultSDL = pContextStateSDL->SDL_InitSubSystem(MA_SDL_INIT_AUDIO);
    if (resultSDL != 0) {
        ma_dlclose(pLog, pContextStateSDL->hSDL);
        ma_free(pContextStateSDL, ma_context_get_allocation_callbacks(pContext));
        return MA_ERROR;
    }

    *ppContextState = pContextStateSDL;

    return MA_SUCCESS;
}

static void ma_context_uninit__sdl(ma_context* pContext)
{
    ma_context_state_sdl* pContextStateSDL = ma_context_get_backend_state__sdl(pContext);

    MA_SDL_ASSERT(pContextStateSDL != NULL);

    pContextStateSDL->SDL_QuitSubSystem(MA_SDL_INIT_AUDIO);

    /* Close the handle to the SDL shared object last. */
    ma_dlclose(ma_context_get_log(pContext), pContextStateSDL->hSDL);
    pContextStateSDL->hSDL = NULL;

    ma_free(pContextStateSDL, ma_context_get_allocation_callbacks(pContext));
}

static void ma_add_native_format_from_AudioSpec__sdl(ma_device_info* pDeviceInfo, const MA_SDL_AudioSpec* pAudioSpec)
{
    pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].format     = ma_format_from_sdl(pAudioSpec->format);
    pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].channels   = pAudioSpec->channels;
    pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].sampleRate = pAudioSpec->freq;
    pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].flags      = 0;

    /* If miniaudio does not support the format, just use f32 as the native format (SDL will do the necessary conversions for us). */
    if (pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].format == ma_format_unknown) {
        pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].format = ma_format_f32;
    }

    pDeviceInfo->nativeDataFormatCount = 1;
}

static ma_result ma_context_enumerate_devices__sdl(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    ma_context_state_sdl* pContextStateSDL = ma_context_get_backend_state__sdl(pContext);
    ma_device_enumeration_result cbResult = MA_DEVICE_ENUMERATION_CONTINUE;
    int iDevice;

    MA_SDL_ASSERT(pContextStateSDL != NULL);

    /* Playback */
    if (cbResult == MA_DEVICE_ENUMERATION_CONTINUE) {
        int deviceCount = pContextStateSDL->SDL_GetNumAudioDevices(0);
        for (iDevice = 0; iDevice < deviceCount; ++iDevice) {
            ma_device_info deviceInfo;
            MA_SDL_AudioSpec audioSpec;

            memset(&deviceInfo, 0, sizeof(deviceInfo));

            /* Default. */
            if (iDevice == 0) { /* <-- Is this correct? Should we instead compare against the name from SDL_GetDefaultAudioInfo()? */
                deviceInfo.isDefault = MA_TRUE;
            }

            /* ID. */
            deviceInfo.id.custom.i = iDevice;

            /* Name. */
            ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), pContextStateSDL->SDL_GetAudioDeviceName(iDevice, 0), (size_t)-1);

            /* Data Format. */
            if (pContextStateSDL->SDL_GetAudioDeviceSpec != NULL) {
                if (pContextStateSDL->SDL_GetAudioDeviceSpec(iDevice, 0, &audioSpec) == 0) {
                    ma_add_native_format_from_AudioSpec__sdl(&deviceInfo, &audioSpec);
                }
            } else {
                /* No way to retrieve the data format. Just report support for everything. */
                deviceInfo.nativeDataFormatCount = 1;
            }

            cbResult = callback(ma_device_type_playback, &deviceInfo, pCallbackUserData);
            if (cbResult == MA_DEVICE_ENUMERATION_ABORT) {
                break;
            }
        }
    }

    /* Capture */
    if (cbResult == MA_DEVICE_ENUMERATION_CONTINUE) {
        int deviceCount = pContextStateSDL->SDL_GetNumAudioDevices(1);
        for (iDevice = 0; iDevice < deviceCount; ++iDevice) {
            ma_device_info deviceInfo;
            MA_SDL_AudioSpec audioSpec;

            memset(&deviceInfo, 0, sizeof(deviceInfo));

            /* Default. */
            if (iDevice == 0) { /* <-- Is this correct? Should we instead compare against the name from SDL_GetDefaultAudioInfo()? */
                deviceInfo.isDefault = MA_TRUE;
            }

            /* ID. */
            deviceInfo.id.custom.i = iDevice;

            /* Name. */
            ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), pContextStateSDL->SDL_GetAudioDeviceName(iDevice, 1), (size_t)-1);

            /* Data Format. */
            if (pContextStateSDL->SDL_GetAudioDeviceSpec != NULL) {
                if (pContextStateSDL->SDL_GetAudioDeviceSpec(iDevice, 1, &audioSpec) == 0) {
                    ma_add_native_format_from_AudioSpec__sdl(&deviceInfo, &audioSpec);
                }
            } else {
                /* No way to retrieve the data format. Just report support for everything. */
                deviceInfo.nativeDataFormatCount = 1;
            }

            cbResult = callback(ma_device_type_capture, &deviceInfo, pCallbackUserData);
            if (cbResult == MA_DEVICE_ENUMERATION_ABORT) {
                break;
            }
        }
    }

    return MA_SUCCESS;
}


void ma_audio_callback_capture__sdl(void* pUserData, ma_uint8* pBuffer, int bufferSizeInBytes)
{
    ma_device* pDevice = (ma_device*)pUserData;
    ma_device_state_sdl* pDeviceStateSDL = ma_device_get_backend_state__sdl(pDevice);
    ma_device_state_async_process(&pDeviceStateSDL->async, pDevice, NULL, pBuffer, (ma_uint32)bufferSizeInBytes / ma_get_bytes_per_frame(pDeviceStateSDL->async.capture.format, pDeviceStateSDL->async.capture.channels));
}

void ma_audio_callback_playback__sdl(void* pUserData, ma_uint8* pBuffer, int bufferSizeInBytes)
{
    ma_device* pDevice = (ma_device*)pUserData;
    ma_device_state_sdl* pDeviceStateSDL = ma_device_get_backend_state__sdl(pDevice);
    ma_device_state_async_process(&pDeviceStateSDL->async, pDevice, pBuffer, NULL, (ma_uint32)bufferSizeInBytes / ma_get_bytes_per_frame(pDeviceStateSDL->async.playback.format, pDeviceStateSDL->async.playback.channels));
}

static ma_result ma_device_init_internal__sdl(ma_device* pDevice, ma_context_state_sdl* pContextStateSDL, ma_device_state_sdl* pDeviceStateSDL, const ma_device_config_sdl* pDeviceConfigSDL, ma_device_type deviceType, ma_device_descriptor* pDescriptor)
{
    MA_SDL_AudioSpec desiredSpec;
    MA_SDL_AudioSpec obtainedSpec;
    const char* pDeviceName;
    int deviceID;

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
           buffer size, use a default value of MA_DEFAULT_PERIOD_SIZE_IN_MILLISECONDS.

    Note that options 2 and 3 require knowledge of the sample rate in order to convert it to a frame count. You should try to keep the
    calculation of the period size as accurate as possible, but sometimes it's just not practical so just use whatever you can.

    A helper function called ma_calculate_buffer_size_in_frames_from_descriptor() is available to do all of this for you which is what
    we'll be using here.
    */
    pDescriptor->periodSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDescriptor, pDescriptor->sampleRate);

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
    desiredSpec.callback = (deviceType == ma_device_type_capture) ? ma_audio_callback_capture__sdl : ma_audio_callback_playback__sdl;
    desiredSpec.userdata = pDevice;

    /* We'll fall back to f32 if we don't have an appropriate mapping between SDL and miniaudio. */
    if (desiredSpec.format == 0) {
        desiredSpec.format = MA_AUDIO_F32;
    }

    pDeviceName = NULL;
    if (pDescriptor->pDeviceID != NULL) {
        pDeviceName = pContextStateSDL->SDL_GetAudioDeviceName(pDescriptor->pDeviceID->custom.i, (deviceType == ma_device_type_playback) ? 0 : 1);
    }

    deviceID = pContextStateSDL->SDL_OpenAudioDevice(pDeviceName, (deviceType == ma_device_type_playback) ? 0 : 1, &desiredSpec, &obtainedSpec, MA_SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (deviceID == 0) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to open SDL2 device.");
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    /* The descriptor needs to be updated with our actual settings. */
    pDescriptor->format             = ma_format_from_sdl(obtainedSpec.format);
    pDescriptor->channels           = obtainedSpec.channels;
    pDescriptor->sampleRate         = (ma_uint32)obtainedSpec.freq;
    ma_channel_map_init_standard(ma_standard_channel_map_default, pDescriptor->channelMap, ma_countof(pDescriptor->channelMap), pDescriptor->channels);
    pDescriptor->periodSizeInFrames = obtainedSpec.samples;
    pDescriptor->periodCount        = 1;    /* SDL doesn't use the notion of period counts, so just set to 1. */

    if (deviceType == ma_device_type_playback) {
        pDeviceStateSDL->playback.deviceID = deviceID;
    } else {
        pDeviceStateSDL->capture.deviceID = deviceID;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_init__sdl(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    ma_device_state_sdl* pDeviceStateSDL;
    ma_device_config_sdl* pDeviceConfigSDL = (ma_device_config_sdl*)pDeviceBackendConfig;
    ma_context_state_sdl* pContextStateSDL = ma_context_get_backend_state__sdl(ma_device_get_context(pDevice));
    ma_device_type deviceType = ma_device_get_type(pDevice);
    ma_result result;
    
    /* SDL does not support loopback mode, so must return MA_DEVICE_TYPE_NOT_SUPPORTED if it's requested. */
    if (deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    /* We need to allocate our backend-specific data. */
    pDeviceStateSDL = (ma_device_state_sdl*)ma_calloc(sizeof(*pDeviceStateSDL), ma_device_get_allocation_callbacks(pDevice));
    if (pDeviceStateSDL == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__sdl(pDevice, pContextStateSDL, pDeviceStateSDL, pDeviceConfigSDL, ma_device_type_capture, pDescriptorCapture);
        if (result != MA_SUCCESS) {
            ma_free(pDeviceStateSDL, ma_device_get_allocation_callbacks(pDevice));
            return result;
        }
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__sdl(pDevice, pContextStateSDL, pDeviceStateSDL, pDeviceConfigSDL, ma_device_type_playback, pDescriptorPlayback);
        if (result != MA_SUCCESS) {
            if (deviceType == ma_device_type_duplex) {
                pContextStateSDL->SDL_CloseAudioDevice(pDeviceStateSDL->capture.deviceID);
            }

            ma_free(pDeviceStateSDL, ma_device_get_allocation_callbacks(pDevice));
            return result;
        }
    }

    result = ma_device_state_async_init(deviceType, pDescriptorPlayback, pDescriptorCapture, ma_device_get_allocation_callbacks(pDevice), &pDeviceStateSDL->async);
    if (result != MA_SUCCESS) {
        if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
            pContextStateSDL->SDL_CloseAudioDevice(pDeviceStateSDL->capture.deviceID);
        }
        if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
            pContextStateSDL->SDL_CloseAudioDevice(pDeviceStateSDL->playback.deviceID);
        }

        ma_free(pDeviceStateSDL, ma_device_get_allocation_callbacks(pDevice));
        return result;
    }

    *ppDeviceState = pDeviceStateSDL;

    return MA_SUCCESS;
}

static void ma_device_uninit__sdl(ma_device* pDevice)
{
    ma_device_state_sdl* pDeviceStateSDL = ma_device_get_backend_state__sdl(pDevice);
    ma_context_state_sdl* pContextStateSDL = ma_context_get_backend_state__sdl(ma_device_get_context(pDevice));
    ma_device_type deviceType = ma_device_get_type(pDevice);

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
        pContextStateSDL->SDL_CloseAudioDevice(pDeviceStateSDL->capture.deviceID);
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        pContextStateSDL->SDL_CloseAudioDevice(pDeviceStateSDL->playback.deviceID);
    }

    ma_device_state_async_uninit(&pDeviceStateSDL->async, ma_device_get_allocation_callbacks(pDevice));

    ma_free(pDeviceStateSDL, ma_device_get_allocation_callbacks(pDevice));
}

static ma_result ma_device_start__sdl(ma_device* pDevice)
{
    ma_device_state_sdl* pDeviceStateSDL = ma_device_get_backend_state__sdl(pDevice);
    ma_context_state_sdl* pContextStateSDL = ma_context_get_backend_state__sdl(ma_device_get_context(pDevice));
    ma_device_type deviceType = ma_device_get_type(pDevice);

    /* Step the device once to ensure buffers are pre-filled before starting. */
    ma_device_step__sdl(pDevice);

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
        pContextStateSDL->SDL_PauseAudioDevice(pDeviceStateSDL->capture.deviceID, 0);
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        pContextStateSDL->SDL_PauseAudioDevice(pDeviceStateSDL->playback.deviceID, 0);
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stop__sdl(ma_device* pDevice)
{
    ma_device_state_sdl* pDeviceStateSDL = ma_device_get_backend_state__sdl(pDevice);
    ma_context_state_sdl* pContextStateSDL = ma_context_get_backend_state__sdl(ma_device_get_context(pDevice));
    ma_device_type deviceType = ma_device_get_type(pDevice);

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
        pContextStateSDL->SDL_PauseAudioDevice(pDeviceStateSDL->capture.deviceID, 1);
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        pContextStateSDL->SDL_PauseAudioDevice(pDeviceStateSDL->playback.deviceID, 1);
    }

    return MA_SUCCESS;
}


static ma_result ma_device_wait__sdl(ma_device* pDevice)
{
    ma_device_state_sdl* pDeviceStateSDL = ma_device_get_backend_state__sdl(pDevice);
    ma_device_state_async_wait(&pDeviceStateSDL->async);

    return MA_SUCCESS;
}

static ma_result ma_device_step__sdl(ma_device* pDevice)
{
    ma_device_state_sdl* pDeviceStateSDL = ma_device_get_backend_state__sdl(pDevice);
    ma_device_state_async_step(&pDeviceStateSDL->async, pDevice);

    return MA_SUCCESS;
}

static void ma_device_loop__sdl(ma_device* pDevice)
{
    for (;;) {
        ma_result result = ma_device_wait__sdl(pDevice);
        if (result != MA_SUCCESS) {
            break;
        }

        /* If the wait terminated due to the device being stopped, abort now. */
        if (!ma_device_is_started(pDevice)) {
            break;
        }

        result = ma_device_step__sdl(pDevice);
        if (result != MA_SUCCESS) {
            break;
        }
    }
}


static ma_device_backend_vtable ma_gDeviceBackendVTable_SDL =
{
    ma_backend_info__sdl,
    ma_context_init__sdl,
    ma_context_uninit__sdl,
    ma_context_enumerate_devices__sdl,
    ma_device_init__sdl,
    ma_device_uninit__sdl,
    ma_device_start__sdl,
    ma_device_stop__sdl,
    NULL,   /* onDeviceRead */
    NULL,   /* onDeviceWrite */
    ma_device_loop__sdl,
    NULL    /* onDeviceWakeup */
};

ma_device_backend_vtable* ma_device_backend_sdl = &ma_gDeviceBackendVTable_SDL;
#else
ma_device_backend_vtable* ma_device_backend_sdl = NULL;
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
