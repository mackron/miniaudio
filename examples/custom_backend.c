/*
This example show how a custom backend can be implemented.

This implements a full-featured SDL2 backend. It's intentionally built using the same paradigms as the built-in backends in order to make
it suitable as a solid basis for a custom implementation. The SDL2 backend can be disabled with MA_NO_SDL, exactly like the build-in
backends. It supports both runtime and compile-time linking and respects the MA_NO_RUNTIME_LINKING option. It also works on Emscripten
which requires the `-s USE_SDL=2` option.

There may be times where you want to support more than one custom backend. This example has been designed to make it easy to plug-in extra
custom backends without needing to modify any of the base miniaudio initialization code. A custom context structure is declared called
`ma_context_ex`. The first member of this structure is a `ma_context` object which allows it to be cast between the two. The same is done
for devices, which is called `ma_device_ex`. In these structures there is a section for each custom backend, which in this example is just
SDL. These are only enabled at compile time if `MA_SUPPORT_SDL` is defined, which it always is in this example (you may want to have some
logic which more intelligently enables or disables SDL support).

To use a custom backend, at a minimum you must set the `custom.onContextInit()` callback in the context config. You do not need to set the
other callbacks, but if you don't, you must set them in the implementation of the `onContextInit()` callback which is done via an output
parameter. This is the approach taken by this example because it's the simplest way to support multiple custom backends. The idea is that
the `onContextInit()` callback is set to a generic "loader", which then calls out to a backend-specific implementation which then sets the
remaining callbacks if it is successfully initialized.

Custom backends are identified with the `ma_backend_custom` backend type. For the purpose of demonstration, this example only uses the
`ma_backend_custom` backend type because otherwise the built-in backends would always get chosen first and none of the code for the custom
backends would actually get hit. By default, the `ma_backend_custom` backend is the lowest priority backend, except for `ma_backend_null`.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

void main_loop__em()
{
}
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


typedef struct
{
    ma_context context; /* Make this the first member so we can cast between ma_context and ma_context_ex. */
#if defined(MA_SUPPORT_SDL)
    struct
    {
        ma_handle hSDL; /* A handle to the SDL2 shared object. We dynamically load function pointers at runtime so we can avoid linking. */
        ma_proc SDL_InitSubSystem;
        ma_proc SDL_QuitSubSystem;
        ma_proc SDL_GetNumAudioDevices;
        ma_proc SDL_GetAudioDeviceName;
        ma_proc SDL_CloseAudioDevice;
        ma_proc SDL_OpenAudioDevice;
        ma_proc SDL_PauseAudioDevice;
    } sdl;
#endif
} ma_context_ex;

typedef struct
{
    ma_device device;   /* Make this the first member so we can cast between ma_device and ma_device_ex. */
#if defined(MA_SUPPORT_SDL)
    struct
    {
        int deviceIDPlayback;
        int deviceIDCapture;
    } sdl;
#endif
} ma_device_ex;



#if defined(MA_HAS_SDL)
    /* SDL headers are necessary if using compile-time linking. */
    #ifdef MA_NO_RUNTIME_LINKING
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

static ma_result ma_context_enumerate_devices__sdl(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pUserData)
{
    ma_context_ex* pContextEx = (ma_context_ex*)pContext;
    ma_bool32 isTerminated = MA_FALSE;
    ma_bool32 cbResult;
    int iDevice;

    MA_ASSERT(pContext != NULL);
    MA_ASSERT(callback != NULL);

    /* Playback */
    if (!isTerminated) {
        int deviceCount = ((MA_PFN_SDL_GetNumAudioDevices)pContextEx->sdl.SDL_GetNumAudioDevices)(0);
        for (iDevice = 0; iDevice < deviceCount; ++iDevice) {
            ma_device_info deviceInfo;
            MA_ZERO_OBJECT(&deviceInfo);

            deviceInfo.id.custom.i = iDevice;
            ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), ((MA_PFN_SDL_GetAudioDeviceName)pContextEx->sdl.SDL_GetAudioDeviceName)(iDevice, 0), (size_t)-1);

            if (iDevice == 0) {
                deviceInfo.isDefault = MA_TRUE;
            }

            cbResult = callback(pContext, ma_device_type_playback, &deviceInfo, pUserData);
            if (cbResult == MA_FALSE) {
                isTerminated = MA_TRUE;
                break;
            }
        }
    }

    /* Capture */
    if (!isTerminated) {
        int deviceCount = ((MA_PFN_SDL_GetNumAudioDevices)pContextEx->sdl.SDL_GetNumAudioDevices)(1);
        for (iDevice = 0; iDevice < deviceCount; ++iDevice) {
            ma_device_info deviceInfo;
            MA_ZERO_OBJECT(&deviceInfo);

            deviceInfo.id.custom.i = iDevice;
            ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), ((MA_PFN_SDL_GetAudioDeviceName)pContextEx->sdl.SDL_GetAudioDeviceName)(iDevice, 1), (size_t)-1);

            if (iDevice == 0) {
                deviceInfo.isDefault = MA_TRUE;
            }

            cbResult = callback(pContext, ma_device_type_capture, &deviceInfo, pUserData);
            if (cbResult == MA_FALSE) {
                isTerminated = MA_TRUE;
                break;
            }
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_context_get_device_info__sdl(ma_context* pContext, ma_device_type deviceType, const ma_device_id* pDeviceID, ma_device_info* pDeviceInfo)
{
    ma_context_ex* pContextEx = (ma_context_ex*)pContext;

#if !defined(__EMSCRIPTEN__)
    MA_SDL_AudioSpec desiredSpec;
    MA_SDL_AudioSpec obtainedSpec;
    MA_SDL_AudioDeviceID tempDeviceID;
    const char* pDeviceName;
#endif

    MA_ASSERT(pContext != NULL);

    if (pDeviceID == NULL) {
        if (deviceType == ma_device_type_playback) {
            pDeviceInfo->id.custom.i = 0;
            ma_strncpy_s(pDeviceInfo->name, sizeof(pDeviceInfo->name), MA_DEFAULT_PLAYBACK_DEVICE_NAME, (size_t)-1);
        } else {
            pDeviceInfo->id.custom.i = 0;
            ma_strncpy_s(pDeviceInfo->name, sizeof(pDeviceInfo->name), MA_DEFAULT_CAPTURE_DEVICE_NAME, (size_t)-1);
        }
    } else {
        pDeviceInfo->id.custom.i = pDeviceID->custom.i;
        ma_strncpy_s(pDeviceInfo->name, sizeof(pDeviceInfo->name), ((MA_PFN_SDL_GetAudioDeviceName)pContextEx->sdl.SDL_GetAudioDeviceName)(pDeviceID->custom.i, (deviceType == ma_device_type_playback) ? 0 : 1), (size_t)-1);
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
    MA_ZERO_MEMORY(&desiredSpec, sizeof(desiredSpec));

    pDeviceName = NULL;
    if (pDeviceID != NULL) {
        pDeviceName = ((MA_PFN_SDL_GetAudioDeviceName)pContextEx->sdl.SDL_GetAudioDeviceName)(pDeviceID->custom.i, (deviceType == ma_device_type_playback) ? 0 : 1);
    }

    tempDeviceID = ((MA_PFN_SDL_OpenAudioDevice)pContextEx->sdl.SDL_OpenAudioDevice)(pDeviceName, (deviceType == ma_device_type_playback) ? 0 : 1, &desiredSpec, &obtainedSpec, MA_SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (tempDeviceID == 0) {
        ma_log_postf(ma_context_get_log(pContext), MA_LOG_LEVEL_ERROR, "Failed to open SDL device.");
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    ((MA_PFN_SDL_CloseAudioDevice)pContextEx->sdl.SDL_CloseAudioDevice)(tempDeviceID);

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
    ma_device_ex* pDeviceEx = (ma_device_ex*)pUserData;

    MA_ASSERT(pDeviceEx != NULL);

    ma_device_handle_backend_data_callback((ma_device*)pDeviceEx, NULL, pBuffer, (ma_uint32)bufferSizeInBytes / ma_get_bytes_per_frame(pDeviceEx->device.capture.internalFormat, pDeviceEx->device.capture.internalChannels));
}

void ma_audio_callback_playback__sdl(void* pUserData, ma_uint8* pBuffer, int bufferSizeInBytes)
{
    ma_device_ex* pDeviceEx = (ma_device_ex*)pUserData;

    MA_ASSERT(pDeviceEx != NULL);

    ma_device_handle_backend_data_callback((ma_device*)pDeviceEx, pBuffer, NULL, (ma_uint32)bufferSizeInBytes / ma_get_bytes_per_frame(pDeviceEx->device.playback.internalFormat, pDeviceEx->device.playback.internalChannels));
}

static ma_result ma_device_init_internal__sdl(ma_device_ex* pDeviceEx, const ma_device_config* pConfig, ma_device_descriptor* pDescriptor)
{
    ma_context_ex* pContextEx = (ma_context_ex*)pDeviceEx->device.pContext;
    MA_SDL_AudioSpec desiredSpec;
    MA_SDL_AudioSpec obtainedSpec;
    const char* pDeviceName;
    int deviceID;

    MA_ASSERT(pDeviceEx  != NULL);
    MA_ASSERT(pDescriptor != NULL);

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
    MA_ZERO_OBJECT(&desiredSpec);
    desiredSpec.freq     = (int)pDescriptor->sampleRate;
    desiredSpec.format   = ma_format_to_sdl(pDescriptor->format);
    desiredSpec.channels = (ma_uint8)pDescriptor->channels;
    desiredSpec.samples  = (ma_uint16)pDescriptor->periodSizeInFrames;
    desiredSpec.callback = (pConfig->deviceType == ma_device_type_capture) ? ma_audio_callback_capture__sdl : ma_audio_callback_playback__sdl;
    desiredSpec.userdata = pDeviceEx;

    /* We'll fall back to f32 if we don't have an appropriate mapping between SDL and miniaudio. */
    if (desiredSpec.format == 0) {
        desiredSpec.format = MA_AUDIO_F32;
    }

    pDeviceName = NULL;
    if (pDescriptor->pDeviceID != NULL) {
        pDeviceName = ((MA_PFN_SDL_GetAudioDeviceName)pContextEx->sdl.SDL_GetAudioDeviceName)(pDescriptor->pDeviceID->custom.i, (pConfig->deviceType == ma_device_type_playback) ? 0 : 1);
    }

    deviceID = ((MA_PFN_SDL_OpenAudioDevice)pContextEx->sdl.SDL_OpenAudioDevice)(pDeviceName, (pConfig->deviceType == ma_device_type_playback) ? 0 : 1, &desiredSpec, &obtainedSpec, MA_SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (deviceID == 0) {
        ma_log_postf(ma_device_get_log((ma_device*)pDeviceEx), MA_LOG_LEVEL_ERROR, "Failed to open SDL2 device.");
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    if (pConfig->deviceType == ma_device_type_playback) {
        pDeviceEx->sdl.deviceIDPlayback = deviceID;
    } else {
        pDeviceEx->sdl.deviceIDCapture = deviceID;
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

static ma_result ma_device_init__sdl(ma_device* pDevice, const ma_device_config* pConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture)
{
    ma_device_ex* pDeviceEx = (ma_device_ex*)pDevice;
    ma_context_ex* pContextEx = (ma_context_ex*)pDevice->pContext;
    ma_result result;

    MA_ASSERT(pDevice != NULL);
    
    /* SDL does not support loopback mode, so must return MA_DEVICE_TYPE_NOT_SUPPORTED if it's requested. */
    if (pConfig->deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    if (pConfig->deviceType == ma_device_type_capture || pConfig->deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__sdl(pDeviceEx, pConfig, pDescriptorCapture);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    if (pConfig->deviceType == ma_device_type_playback || pConfig->deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__sdl(pDeviceEx, pConfig, pDescriptorPlayback);
        if (result != MA_SUCCESS) {
            if (pConfig->deviceType == ma_device_type_duplex) {
                ((MA_PFN_SDL_CloseAudioDevice)pContextEx->sdl.SDL_CloseAudioDevice)(pDeviceEx->sdl.deviceIDCapture);
            }

            return result;
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_device_uninit__sdl(ma_device* pDevice)
{
    ma_device_ex* pDeviceEx = (ma_device_ex*)pDevice;
    ma_context_ex* pContextEx = (ma_context_ex*)pDevice->pContext;

    MA_ASSERT(pDevice != NULL);

    if (pDevice->type == ma_device_type_capture || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_CloseAudioDevice)pContextEx->sdl.SDL_CloseAudioDevice)(pDeviceEx->sdl.deviceIDCapture);
    }

    if (pDevice->type == ma_device_type_playback || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_CloseAudioDevice)pContextEx->sdl.SDL_CloseAudioDevice)(pDeviceEx->sdl.deviceIDCapture);
    }

    return MA_SUCCESS;
}

static ma_result ma_device_start__sdl(ma_device* pDevice)
{
    ma_device_ex* pDeviceEx = (ma_device_ex*)pDevice;
    ma_context_ex* pContextEx = (ma_context_ex*)pDevice->pContext;

    MA_ASSERT(pDevice != NULL);

    if (pDevice->type == ma_device_type_capture || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_PauseAudioDevice)pContextEx->sdl.SDL_PauseAudioDevice)(pDeviceEx->sdl.deviceIDCapture, 0);
    }

    if (pDevice->type == ma_device_type_playback || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_PauseAudioDevice)pContextEx->sdl.SDL_PauseAudioDevice)(pDeviceEx->sdl.deviceIDPlayback, 0);
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stop__sdl(ma_device* pDevice)
{
    ma_device_ex* pDeviceEx = (ma_device_ex*)pDevice;
    ma_context_ex* pContextEx = (ma_context_ex*)pDevice->pContext;

    MA_ASSERT(pDevice != NULL);

    if (pDevice->type == ma_device_type_capture || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_PauseAudioDevice)pContextEx->sdl.SDL_PauseAudioDevice)(pDeviceEx->sdl.deviceIDCapture, 1);
    }

    if (pDevice->type == ma_device_type_playback || pDevice->type == ma_device_type_duplex) {
        ((MA_PFN_SDL_PauseAudioDevice)pContextEx->sdl.SDL_PauseAudioDevice)(pDeviceEx->sdl.deviceIDPlayback, 1);
    }

    return MA_SUCCESS;
}

static ma_result ma_context_uninit__sdl(ma_context* pContext)
{
    ma_context_ex* pContextEx = (ma_context_ex*)pContext;

    MA_ASSERT(pContext != NULL);

    ((MA_PFN_SDL_QuitSubSystem)pContextEx->sdl.SDL_QuitSubSystem)(MA_SDL_INIT_AUDIO);

    /* Close the handle to the SDL shared object last. */
    ma_dlclose(ma_context_get_log(pContext), pContextEx->sdl.hSDL);
    pContextEx->sdl.hSDL = NULL;

    return MA_SUCCESS;
}

static ma_result ma_context_init__sdl(ma_context* pContext, const ma_context_config* pConfig, ma_backend_callbacks* pCallbacks)
{
    ma_context_ex* pContextEx = (ma_context_ex*)pContext;
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

    MA_ASSERT(pContext != NULL);

    (void)pConfig;

    /* Check if we have SDL2 installed somewhere. If not it's not usable and we need to abort. */
    for (iName = 0; iName < ma_countof(pSDLNames); iName += 1) {
        pContextEx->sdl.hSDL = ma_dlopen(ma_context_get_log(pContext), pSDLNames[iName]);
        if (pContextEx->sdl.hSDL != NULL) {
            break;
        }
    }

    if (pContextEx->sdl.hSDL == NULL) {
        return MA_NO_BACKEND;   /* SDL2 could not be loaded. */
    }

    /* Now that we have the handle to the shared object we can go ahead and load some function pointers. */
    pContextEx->sdl.SDL_InitSubSystem      = ma_dlsym(ma_context_get_log(pContext), pContextEx->sdl.hSDL, "SDL_InitSubSystem");
    pContextEx->sdl.SDL_QuitSubSystem      = ma_dlsym(ma_context_get_log(pContext), pContextEx->sdl.hSDL, "SDL_QuitSubSystem");
    pContextEx->sdl.SDL_GetNumAudioDevices = ma_dlsym(ma_context_get_log(pContext), pContextEx->sdl.hSDL, "SDL_GetNumAudioDevices");
    pContextEx->sdl.SDL_GetAudioDeviceName = ma_dlsym(ma_context_get_log(pContext), pContextEx->sdl.hSDL, "SDL_GetAudioDeviceName");
    pContextEx->sdl.SDL_CloseAudioDevice   = ma_dlsym(ma_context_get_log(pContext), pContextEx->sdl.hSDL, "SDL_CloseAudioDevice");
    pContextEx->sdl.SDL_OpenAudioDevice    = ma_dlsym(ma_context_get_log(pContext), pContextEx->sdl.hSDL, "SDL_OpenAudioDevice");
    pContextEx->sdl.SDL_PauseAudioDevice   = ma_dlsym(ma_context_get_log(pContext), pContextEx->sdl.hSDL, "SDL_PauseAudioDevice");
#else
    pContextEx->sdl.SDL_InitSubSystem      = (ma_proc)SDL_InitSubSystem;
    pContextEx->sdl.SDL_QuitSubSystem      = (ma_proc)SDL_QuitSubSystem;
    pContextEx->sdl.SDL_GetNumAudioDevices = (ma_proc)SDL_GetNumAudioDevices;
    pContextEx->sdl.SDL_GetAudioDeviceName = (ma_proc)SDL_GetAudioDeviceName;
    pContextEx->sdl.SDL_CloseAudioDevice   = (ma_proc)SDL_CloseAudioDevice;
    pContextEx->sdl.SDL_OpenAudioDevice    = (ma_proc)SDL_OpenAudioDevice;
    pContextEx->sdl.SDL_PauseAudioDevice   = (ma_proc)SDL_PauseAudioDevice;
#endif  /* MA_NO_RUNTIME_LINKING */

    resultSDL = ((MA_PFN_SDL_InitSubSystem)pContextEx->sdl.SDL_InitSubSystem)(MA_SDL_INIT_AUDIO);
    if (resultSDL != 0) {
        ma_dlclose(ma_context_get_log(pContext), pContextEx->sdl.hSDL);
        return MA_ERROR;
    }

    /*
    The last step is to make sure the callbacks are set properly in `pCallbacks`. Internally, miniaudio will copy these callbacks into the
    context object and then use them for then on for calling into our custom backend.
    */
    pCallbacks->onContextInit             = ma_context_init__sdl;
    pCallbacks->onContextUninit           = ma_context_uninit__sdl;
    pCallbacks->onContextEnumerateDevices = ma_context_enumerate_devices__sdl;
    pCallbacks->onContextGetDeviceInfo    = ma_context_get_device_info__sdl;
    pCallbacks->onDeviceInit              = ma_device_init__sdl;
    pCallbacks->onDeviceUninit            = ma_device_uninit__sdl;
    pCallbacks->onDeviceStart             = ma_device_start__sdl;
    pCallbacks->onDeviceStop              = ma_device_stop__sdl;

    return MA_SUCCESS;
}
#endif  /* MA_HAS_SDL */


/*
This is our custom backend "loader". All this does is attempts to initialize our custom backends in the order they are listed. The first
one to successfully initialize is the one that's chosen. In this example we're just listing them statically, but you can use whatever logic
you want to handle backend selection.

This is used as the onContextInit() callback in the context config.
*/
static ma_result ma_context_init__custom_loader(ma_context* pContext, const ma_context_config* pConfig, ma_backend_callbacks* pCallbacks)
{
    ma_result result = MA_NO_BACKEND;

    /* Silence some unused parameter warnings just in case no custom backends are enabled. */
    (void)pContext;
    (void)pCallbacks;

    /* SDL. */
#if !defined(MA_NO_SDL)
    if (result != MA_SUCCESS) {
        result = ma_context_init__sdl(pContext, pConfig, pCallbacks);
    }
#endif

    /* ... plug in any other custom backends here ... */

    /* If we have a success result we have initialized a backend. Otherwise we need to tell miniaudio about the error so it can skip over our custom backends. */
    return result;
}


/*
Main program starts here.
*/
#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);

    if (pDevice->type == ma_device_type_playback)  {
        ma_waveform* pSineWave;

        pSineWave = (ma_waveform*)pDevice->pUserData;
        MA_ASSERT(pSineWave != NULL);

        ma_waveform_read_pcm_frames(pSineWave, pOutput, frameCount, NULL);
    }

    if (pDevice->type == ma_device_type_duplex) {
        ma_copy_pcm_frames(pOutput, pInput, frameCount, pDevice->playback.format, pDevice->playback.channels);
    }
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_context_config contextConfig;
    ma_context_ex context;
    ma_device_config deviceConfig;
    ma_device_ex device;
    ma_waveform_config sineWaveConfig;
    ma_waveform sineWave;

    /*
    We're just using ma_backend_custom in this example for demonstration purposes, but a more realistic use case would probably want to include
    other backends as well for robustness.
    */
    ma_backend backends[] = {
        ma_backend_custom
    };

    /*
    To implement a custom backend you need to implement the callbacks in the "custom" member of the context config. The only mandatory
    callback required at this point is the onContextInit() callback. If you do not set the other callbacks, you must set them in
    onContextInit() by setting them on the `pCallbacks` parameter.

    The way we're doing it in this example enables us to easily plug in multiple custom backends. What we do is set the onContextInit()
    callback to a generic "loader" function (ma_context_init__custom_loader() in this example), which then calls out to backend-specific
    context initialization routines, one of which will be for SDL. That way, if for example we wanted to add support for another backend,
    we don't need to touch this part of the code. Instead we add logic to ma_context_init__custom_loader() to choose the most appropriate
    custom backend. That will then fill out the other callbacks appropriately.
    */
    contextConfig = ma_context_config_init();
    contextConfig.custom.onContextInit = ma_context_init__custom_loader;
    
    result = ma_context_init(backends, sizeof(backends)/sizeof(backends[0]), &contextConfig, (ma_context*)&context);
    if (result != MA_SUCCESS) {
        return -1;
    }

    /* In playback mode we're just going to play a sine wave. */
    sineWaveConfig = ma_waveform_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE, ma_waveform_type_sine, 0.2, 220);
    ma_waveform_init(&sineWaveConfig, &sineWave);

    /* The device is created exactly as per normal. */
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.capture.format    = DEVICE_FORMAT;
    deviceConfig.capture.channels  = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &sineWave;

    result = ma_device_init((ma_context*)&context, &deviceConfig, (ma_device*)&device);
    if (result != MA_SUCCESS) {
        ma_context_uninit((ma_context*)&context);
        return -1;
    }


    printf("Device Name: %s\n", ((ma_device*)&device)->playback.name);

    if (ma_device_start((ma_device*)&device) != MA_SUCCESS) {
        ma_device_uninit((ma_device*)&device);
        ma_context_uninit((ma_context*)&context);
        return -5;
    }
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop__em, 0, 1);
#else
    printf("Press Enter to quit...\n");
    getchar();
#endif
    
    ma_device_uninit((ma_device*)&device);
    ma_context_uninit((ma_context*)&context);

    (void)argc;
    (void)argv;

    return 0;
}