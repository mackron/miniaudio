#ifndef miniaudio_backend_sdl3_c
#define miniaudio_backend_sdl3_c

#if !defined(MINIAUDIO_IMPLEMENTATION) && !defined(MA_IMPLEMENTATION)
#include "../../../miniaudio.h"
#endif

#include "miniaudio_sdl3.h"

#include <string.h>
#include <assert.h>
#include <limits.h>

#ifndef MA_SDL3_ASSERT
#define MA_SDL3_ASSERT(cond) assert(cond)
#endif

#define MA_SUPPORT_SDL3
#if defined(MA_SUPPORT_SDL3) && !defined(MA_NO_SDL3) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_SDL3))
    #define MA_HAS_SDL3
#endif

#if defined(MA_HAS_SDL3) && (defined(MA_NO_RUNTIME_LINKING) || defined(MA_EMSCRIPTEN))
    #ifdef __has_include
        #if !__has_include(<SDL3/SDL_audio.h>)
            #undef MA_HAS_SDL3
        #endif
    #endif
#endif

#if defined(MA_HAS_SDL3)
#define MA_SDL3_INIT_AUDIO          0x00000010
#define MA_SDL3_AUDIO_U8            0x0008
#define MA_SDL3_AUDIO_S16LSB        0x8010
#define MA_SDL3_AUDIO_S16MSB        0x9010
#define MA_SDL3_AUDIO_S32LSB        0x8020
#define MA_SDL3_AUDIO_S32MSB        0x9020
#define MA_SDL3_AUDIO_F32LSB        0x8120
#define MA_SDL3_AUDIO_F32MSB        0x9120
#define MA_SDL3_DEFAULT_PLAYBACK    ((ma_uint32)0xFFFFFFFF)
#define MA_SDL3_DEFAULT_CAPTURE     ((ma_uint32)0xFFFFFFFE)

#if defined(MA_NO_RUNTIME_LINKING) || defined(MA_EMSCRIPTEN)
    #include <SDL3/SDL_audio.h>
    #include <SDL3/SDL_init.h>
    #include <SDL3/SDL_error.h>
    #include <SDL3/SDL_hints.h>

    #define MA_SDL3_CALL SDLCALL

    typedef bool                    MA_SDL3_bool;
    typedef SDL_AudioFormat         MA_SDL3_AudioFormat;
    typedef SDL_AudioDeviceID       MA_SDL3_AudioDeviceID;
    typedef SDL_AudioSpec           MA_SDL3_AudioSpec;
    typedef SDL_AudioStream         MA_SDL3_AudioStream;
    typedef SDL_AudioStreamCallback MA_SDL3_AudioStreamCallback;
#else
    #if defined(_WIN32) && !defined(__GNUC__)
        #define MA_SDL3_CALL __cdecl
    #else
        #define MA_SDL3_CALL
    #endif

    /* Match SDL3's bool ABI, including its fallback for older C compilers. */
    #if defined(__cplusplus)
        typedef bool MA_SDL3_bool;
    #elif defined(__GNUC__) || defined(__clang__)
        __extension__ typedef _Bool MA_SDL3_bool;
    #elif (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || (defined(_MSC_VER) && _MSC_VER >= 1910)
        #include <stdbool.h>
        typedef bool MA_SDL3_bool;
    #else
        typedef unsigned char MA_SDL3_bool;
    #endif

    /* SDL3 uses an enum for formats, unlike SDL2's 16-bit integer. */
    typedef enum
    {
        MA_SDL3_AUDIO_FORMAT_UNKNOWN = 0,
        MA_SDL3_AUDIO_FORMAT_U8      = MA_SDL3_AUDIO_U8,
        MA_SDL3_AUDIO_FORMAT_S16LSB  = MA_SDL3_AUDIO_S16LSB,
        MA_SDL3_AUDIO_FORMAT_S16MSB  = MA_SDL3_AUDIO_S16MSB,
        MA_SDL3_AUDIO_FORMAT_S32LSB  = MA_SDL3_AUDIO_S32LSB,
        MA_SDL3_AUDIO_FORMAT_S32MSB  = MA_SDL3_AUDIO_S32MSB,
        MA_SDL3_AUDIO_FORMAT_F32LSB  = MA_SDL3_AUDIO_F32LSB,
        MA_SDL3_AUDIO_FORMAT_F32MSB  = MA_SDL3_AUDIO_F32MSB
    } MA_SDL3_AudioFormat;

    typedef ma_uint32 MA_SDL3_AudioDeviceID;

    typedef struct
    {
        MA_SDL3_AudioFormat format;
        int channels;
        int freq;
    } MA_SDL3_AudioSpec;

    typedef struct SDL_AudioStream MA_SDL3_AudioStream;

    typedef void (MA_SDL3_CALL * MA_SDL3_AudioStreamCallback)(void* pUserData, MA_SDL3_AudioStream* pStream, int additionalAmount, int totalAmount);
#endif

typedef MA_SDL3_bool           (MA_SDL3_CALL * MA_PFN_SDL3_SetHint                 )(const char* pName, const char* pValue);
typedef MA_SDL3_bool           (MA_SDL3_CALL * MA_PFN_SDL3_InitSubSystem           )(ma_uint32 flags);
typedef void                   (MA_SDL3_CALL * MA_PFN_SDL3_QuitSubSystem           )(ma_uint32 flags);
typedef MA_SDL3_AudioDeviceID* (MA_SDL3_CALL * MA_PFN_SDL3_GetAudioPlaybackDevices )(int* pCount);
typedef MA_SDL3_AudioDeviceID* (MA_SDL3_CALL * MA_PFN_SDL3_GetAudioRecordingDevices)(int* pCount);
typedef const char*            (MA_SDL3_CALL * MA_PFN_SDL3_GetAudioDeviceName      )(MA_SDL3_AudioDeviceID deviceID);
typedef MA_SDL3_bool           (MA_SDL3_CALL * MA_PFN_SDL3_GetAudioDeviceFormat    )(MA_SDL3_AudioDeviceID deviceID, MA_SDL3_AudioSpec* pSpec, int* pSampleFrames);
typedef MA_SDL3_bool           (MA_SDL3_CALL * MA_PFN_SDL3_IsAudioDevicePlayback   )(MA_SDL3_AudioDeviceID deviceID);
typedef MA_SDL3_AudioStream*   (MA_SDL3_CALL * MA_PFN_SDL3_OpenAudioDeviceStream   )(MA_SDL3_AudioDeviceID deviceID, const MA_SDL3_AudioSpec* pSpec, MA_SDL3_AudioStreamCallback callback, void* pUserData);
typedef MA_SDL3_AudioDeviceID  (MA_SDL3_CALL * MA_PFN_SDL3_GetAudioStreamDevice    )(MA_SDL3_AudioStream* pStream);
typedef void                   (MA_SDL3_CALL * MA_PFN_SDL3_DestroyAudioStream      )(MA_SDL3_AudioStream* pStream);
typedef MA_SDL3_bool           (MA_SDL3_CALL * MA_PFN_SDL3_ResumeAudioStreamDevice )(MA_SDL3_AudioStream* pStream);
typedef MA_SDL3_bool           (MA_SDL3_CALL * MA_PFN_SDL3_PauseAudioStreamDevice  )(MA_SDL3_AudioStream* pStream);
typedef MA_SDL3_bool           (MA_SDL3_CALL * MA_PFN_SDL3_PutAudioStreamData      )(MA_SDL3_AudioStream* pStream, const void* pBuffer, int sizeInBytes);
typedef int                    (MA_SDL3_CALL * MA_PFN_SDL3_GetAudioStreamData      )(MA_SDL3_AudioStream* pStream, void* pBuffer, int sizeInBytes);
typedef int                    (MA_SDL3_CALL * MA_PFN_SDL3_GetAudioStreamQueued    )(MA_SDL3_AudioStream* pStream);
typedef int                    (MA_SDL3_CALL * MA_PFN_SDL3_GetAudioStreamAvailable )(MA_SDL3_AudioStream* pStream);
typedef MA_SDL3_bool           (MA_SDL3_CALL * MA_PFN_SDL3_ClearAudioStream        )(MA_SDL3_AudioStream* pStream);
typedef const char*            (MA_SDL3_CALL * MA_PFN_SDL3_GetError                )(void);
typedef void                   (MA_SDL3_CALL * MA_PFN_SDL3_free                    )(void* pMemory);

typedef struct
{
    ma_handle hSDL;
    MA_PFN_SDL3_SetHint                  SDL_SetHint;
    MA_PFN_SDL3_InitSubSystem            SDL_InitSubSystem;
    MA_PFN_SDL3_QuitSubSystem            SDL_QuitSubSystem;
    MA_PFN_SDL3_GetAudioPlaybackDevices  SDL_GetAudioPlaybackDevices;
    MA_PFN_SDL3_GetAudioRecordingDevices SDL_GetAudioRecordingDevices;
    MA_PFN_SDL3_GetAudioDeviceName       SDL_GetAudioDeviceName;
    MA_PFN_SDL3_GetAudioDeviceFormat     SDL_GetAudioDeviceFormat;
    MA_PFN_SDL3_IsAudioDevicePlayback    SDL_IsAudioDevicePlayback;
    MA_PFN_SDL3_OpenAudioDeviceStream    SDL_OpenAudioDeviceStream;
    MA_PFN_SDL3_GetAudioStreamDevice     SDL_GetAudioStreamDevice;
    MA_PFN_SDL3_DestroyAudioStream       SDL_DestroyAudioStream;
    MA_PFN_SDL3_ResumeAudioStreamDevice  SDL_ResumeAudioStreamDevice;
    MA_PFN_SDL3_PauseAudioStreamDevice   SDL_PauseAudioStreamDevice;
    MA_PFN_SDL3_PutAudioStreamData       SDL_PutAudioStreamData;
    MA_PFN_SDL3_GetAudioStreamData       SDL_GetAudioStreamData;
    MA_PFN_SDL3_GetAudioStreamQueued     SDL_GetAudioStreamQueued;
    MA_PFN_SDL3_GetAudioStreamAvailable  SDL_GetAudioStreamAvailable;
    MA_PFN_SDL3_ClearAudioStream         SDL_ClearAudioStream;
    MA_PFN_SDL3_GetError                 SDL_GetError;
    MA_PFN_SDL3_free                     SDL_free;
} ma_context_state_sdl3;

typedef struct
{
    MA_SDL3_AudioStream* pStream;
    void* pBuffer;
    ma_uint32 bufferSizeInBytes;
    ma_uint32 bytesPerFrame;
} ma_device_stream_sdl3;

typedef struct
{
    ma_semaphore semaphore;
    ma_device_stream_sdl3 capture;
    ma_device_stream_sdl3 playback;
} ma_device_state_sdl3;

static ma_result ma_sdl_uint_to_string(ma_uint32 value, char* pBuffer, size_t bufferSize)
{
    char digits[10];
    size_t digitCount = 0;
    size_t iDigit;

    if (pBuffer == NULL || bufferSize == 0) {
        return MA_INVALID_ARGS;
    }

    do {
        digits[digitCount] = (char)('0' + value % 10);
        digitCount += 1;
        value /= 10;
    } while (value != 0);

    if (bufferSize <= digitCount) {
        pBuffer[0] = '\0';
        return MA_OUT_OF_RANGE;
    }

    for (iDigit = 0; iDigit < digitCount; iDigit += 1) {
        pBuffer[iDigit] = digits[digitCount - iDigit - 1];
    }

    pBuffer[digitCount] = '\0';
    return MA_SUCCESS;
}

static MA_SDL3_AudioFormat ma_format_to_sdl3(ma_format format)
{
    if (ma_is_little_endian()) {
        switch (format) {
            case ma_format_s16: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_S16LSB;
            case ma_format_s24: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_S32LSB;  /* Closest match. */
            case ma_format_s32: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_S32LSB;
            case ma_format_f32: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_F32LSB;
            default: break;
        }
    } else {
        switch (format) {
            case ma_format_s16: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_S16MSB;
            case ma_format_s24: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_S32MSB;  /* Closest match. */
            case ma_format_s32: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_S32MSB;
            case ma_format_f32: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_F32MSB;
            default: break;
        }
    }

    /* Endian agnostic. */
    switch (format) {
        case ma_format_u8: return (MA_SDL3_AudioFormat)MA_SDL3_AUDIO_U8;
        default: return (MA_SDL3_AudioFormat)0;
    }
}

static ma_format ma_format_from_sdl3(MA_SDL3_AudioFormat format)
{
    if (ma_is_little_endian()) {
        switch (format) {
            case MA_SDL3_AUDIO_S16LSB: return ma_format_s16;
            case MA_SDL3_AUDIO_S32LSB: return ma_format_s32;
            case MA_SDL3_AUDIO_F32LSB: return ma_format_f32;
            default: break;
        }
    } else {
        switch (format) {
            case MA_SDL3_AUDIO_S16MSB: return ma_format_s16;
            case MA_SDL3_AUDIO_S32MSB: return ma_format_s32;
            case MA_SDL3_AUDIO_F32MSB: return ma_format_f32;
            default: break;
        }
    }

    /* Endian agnostic. */
    switch (format) {
        case MA_SDL3_AUDIO_U8: return ma_format_u8;
        default: return ma_format_unknown;
    }
}


static ma_context_state_sdl3* ma_context_get_backend_state__sdl3(ma_context* pContext)
{
    return (ma_context_state_sdl3*)ma_context_get_backend_state(pContext);
}

static ma_device_state_sdl3* ma_device_get_backend_state__sdl3(ma_device* pDevice)
{
    return (ma_device_state_sdl3*)ma_device_get_backend_state(pDevice);
}


static void ma_backend_info__sdl3(ma_device_backend_info* pBackendInfo)
{
    MA_SDL3_ASSERT(pBackendInfo != NULL);
    pBackendInfo->pName = "SDL3";
}

static ma_result ma_context_init__sdl3(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    ma_context_state_sdl3* pContextStateSDL;
    ma_log* pLog;

    (void)pContextBackendConfig;

    *ppContextState = NULL;
    
    pLog = ma_context_get_log(pContext);

    pContextStateSDL = (ma_context_state_sdl3*)ma_calloc(sizeof(*pContextStateSDL), ma_context_get_allocation_callbacks(pContext));
    if (pContextStateSDL == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    #if !defined(MA_NO_RUNTIME_LINKING) && !defined(MA_EMSCRIPTEN)
    {
        size_t iName;
        const char* pSDLNames[] = {
        #if defined(_WIN32)
            "SDL3.dll"
        #elif defined(__APPLE__)
            "SDL3.framework/SDL3",
            "libSDL3.0.dylib",
            "libSDL3.dylib"
        #else
            "libSDL3.so.0",
            "libSDL3.so"
        #endif
        };

        for (iName = 0; iName < ma_countof(pSDLNames); iName += 1) {
            pContextStateSDL->hSDL = ma_dlopen(pLog, pSDLNames[iName]);
            if (pContextStateSDL->hSDL != NULL) {
                break;
            }
        }

        if (pContextStateSDL->hSDL == NULL) {
            ma_free(pContextStateSDL, ma_context_get_allocation_callbacks(pContext));
            return MA_NO_BACKEND;
        }

        pContextStateSDL->SDL_SetHint                   = (MA_PFN_SDL3_SetHint                 )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_SetHint");
        pContextStateSDL->SDL_InitSubSystem             = (MA_PFN_SDL3_InitSubSystem           )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_InitSubSystem");
        pContextStateSDL->SDL_QuitSubSystem             = (MA_PFN_SDL3_QuitSubSystem           )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_QuitSubSystem");
        pContextStateSDL->SDL_GetAudioPlaybackDevices   = (MA_PFN_SDL3_GetAudioPlaybackDevices )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioPlaybackDevices");
        pContextStateSDL->SDL_GetAudioRecordingDevices  = (MA_PFN_SDL3_GetAudioRecordingDevices)ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioRecordingDevices");
        pContextStateSDL->SDL_GetAudioDeviceName        = (MA_PFN_SDL3_GetAudioDeviceName      )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioDeviceName");
        pContextStateSDL->SDL_GetAudioDeviceFormat      = (MA_PFN_SDL3_GetAudioDeviceFormat    )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioDeviceFormat");
        pContextStateSDL->SDL_IsAudioDevicePlayback     = (MA_PFN_SDL3_IsAudioDevicePlayback   )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_IsAudioDevicePlayback");
        pContextStateSDL->SDL_OpenAudioDeviceStream     = (MA_PFN_SDL3_OpenAudioDeviceStream   )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_OpenAudioDeviceStream");
        pContextStateSDL->SDL_GetAudioStreamDevice      = (MA_PFN_SDL3_GetAudioStreamDevice    )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioStreamDevice");
        pContextStateSDL->SDL_DestroyAudioStream        = (MA_PFN_SDL3_DestroyAudioStream      )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_DestroyAudioStream");
        pContextStateSDL->SDL_ResumeAudioStreamDevice   = (MA_PFN_SDL3_ResumeAudioStreamDevice )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_ResumeAudioStreamDevice");
        pContextStateSDL->SDL_PauseAudioStreamDevice    = (MA_PFN_SDL3_PauseAudioStreamDevice  )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_PauseAudioStreamDevice");
        pContextStateSDL->SDL_PutAudioStreamData        = (MA_PFN_SDL3_PutAudioStreamData      )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_PutAudioStreamData");
        pContextStateSDL->SDL_GetAudioStreamData        = (MA_PFN_SDL3_GetAudioStreamData      )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioStreamData");
        pContextStateSDL->SDL_GetAudioStreamQueued      = (MA_PFN_SDL3_GetAudioStreamQueued    )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioStreamQueued");
        pContextStateSDL->SDL_GetAudioStreamAvailable   = (MA_PFN_SDL3_GetAudioStreamAvailable )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetAudioStreamAvailable");
        pContextStateSDL->SDL_ClearAudioStream          = (MA_PFN_SDL3_ClearAudioStream        )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_ClearAudioStream");
        pContextStateSDL->SDL_GetError                  = (MA_PFN_SDL3_GetError                )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_GetError");
        pContextStateSDL->SDL_free                      = (MA_PFN_SDL3_free                    )ma_dlsym(pLog, pContextStateSDL->hSDL, "SDL_free");
    }
    #else
    {
        pContextStateSDL->SDL_SetHint                   = SDL_SetHint;
        pContextStateSDL->SDL_InitSubSystem             = SDL_InitSubSystem;
        pContextStateSDL->SDL_QuitSubSystem             = SDL_QuitSubSystem;
        pContextStateSDL->SDL_GetAudioPlaybackDevices   = SDL_GetAudioPlaybackDevices;
        pContextStateSDL->SDL_GetAudioRecordingDevices  = SDL_GetAudioRecordingDevices;
        pContextStateSDL->SDL_GetAudioDeviceName        = SDL_GetAudioDeviceName;
        pContextStateSDL->SDL_GetAudioDeviceFormat      = SDL_GetAudioDeviceFormat;
        pContextStateSDL->SDL_IsAudioDevicePlayback     = SDL_IsAudioDevicePlayback;
        pContextStateSDL->SDL_OpenAudioDeviceStream     = SDL_OpenAudioDeviceStream;
        pContextStateSDL->SDL_GetAudioStreamDevice      = SDL_GetAudioStreamDevice;
        pContextStateSDL->SDL_DestroyAudioStream        = SDL_DestroyAudioStream;
        pContextStateSDL->SDL_ResumeAudioStreamDevice   = SDL_ResumeAudioStreamDevice;
        pContextStateSDL->SDL_PauseAudioStreamDevice    = SDL_PauseAudioStreamDevice;
        pContextStateSDL->SDL_PutAudioStreamData        = SDL_PutAudioStreamData;
        pContextStateSDL->SDL_GetAudioStreamData        = SDL_GetAudioStreamData;
        pContextStateSDL->SDL_GetAudioStreamQueued      = SDL_GetAudioStreamQueued;
        pContextStateSDL->SDL_GetAudioStreamAvailable   = SDL_GetAudioStreamAvailable;
        pContextStateSDL->SDL_ClearAudioStream          = SDL_ClearAudioStream;
        pContextStateSDL->SDL_GetError                  = SDL_GetError;
        pContextStateSDL->SDL_free                      = SDL_free;
    }
    #endif

    if (!pContextStateSDL->SDL_InitSubSystem(MA_SDL3_INIT_AUDIO)) {
        ma_log_postf(pLog, MA_LOG_LEVEL_ERROR, "[SDL3] Audio initialization failed: %s\n", pContextStateSDL->SDL_GetError());
        ma_dlclose(pLog, pContextStateSDL->hSDL);
        ma_free(pContextStateSDL, ma_context_get_allocation_callbacks(pContext));
        return MA_ERROR;
    }

    *ppContextState = pContextStateSDL;
    return MA_SUCCESS;
}

static void ma_context_uninit__sdl3(ma_context* pContext)
{
    ma_context_state_sdl3* pContextStateSDL = ma_context_get_backend_state__sdl3(pContext);

    pContextStateSDL->SDL_QuitSubSystem(MA_SDL3_INIT_AUDIO);

    ma_dlclose(ma_context_get_log(pContext), pContextStateSDL->hSDL);
    ma_free(pContextStateSDL, ma_context_get_allocation_callbacks(pContext));
}

static void ma_device_info_init__sdl3(ma_context_state_sdl3* pContextStateSDL, MA_SDL3_AudioDeviceID deviceID, const char* pName, ma_bool32 isDefault, ma_device_info* pInfo)
{
    MA_SDL3_AudioSpec spec;
    ma_format format;

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->id.custom.u = deviceID;
    ma_strncpy_s(pInfo->name, sizeof(pInfo->name), pName, (size_t)-1);
    pInfo->isDefault = isDefault;

    if (pContextStateSDL->SDL_GetAudioDeviceFormat(deviceID, &spec, NULL)) {
        format = ma_format_from_sdl3(spec.format);
        if (format != ma_format_unknown && spec.channels > 0 && spec.freq > 0) {
            ma_device_info_add_native_data_format(pInfo, format, (ma_uint32)spec.channels, (ma_uint32)spec.channels, (ma_uint32)spec.freq, (ma_uint32)spec.freq);
        }
    }
}

static ma_result ma_context_enumerate_devices__sdl3(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    ma_context_state_sdl3* pContextStateSDL = ma_context_get_backend_state__sdl3(pContext);
    ma_device_info info;
    MA_SDL3_AudioDeviceID* pDeviceIDs;
    const char* pName;
    int deviceCount;
    int iDevice;

    /* Playback */
    {
        pDeviceIDs = pContextStateSDL->SDL_GetAudioPlaybackDevices(&deviceCount);
        if (pDeviceIDs == NULL) {
            return MA_ERROR;
        }

        ma_device_info_init__sdl3(pContextStateSDL, MA_SDL3_DEFAULT_PLAYBACK, "Default Playback Device", MA_TRUE, &info);
        if (callback(ma_device_type_playback, &info, pCallbackUserData) == MA_DEVICE_ENUMERATION_ABORT) {
            pContextStateSDL->SDL_free(pDeviceIDs);
            return MA_SUCCESS;
        }

        for (iDevice = 0; iDevice < deviceCount; iDevice += 1) {
            pName = pContextStateSDL->SDL_GetAudioDeviceName(pDeviceIDs[iDevice]);
            if (pName == NULL) {
                continue;   /* The device may have disconnected since enumeration. */
            }

            ma_device_info_init__sdl3(pContextStateSDL, pDeviceIDs[iDevice], pName, MA_FALSE, &info);
            if (callback(ma_device_type_playback, &info, pCallbackUserData) == MA_DEVICE_ENUMERATION_ABORT) {
                pContextStateSDL->SDL_free(pDeviceIDs);
                return MA_SUCCESS;
            }
        }

        pContextStateSDL->SDL_free(pDeviceIDs);
    }

    /* Capture */
    {
        pDeviceIDs = pContextStateSDL->SDL_GetAudioRecordingDevices(&deviceCount);
        if (pDeviceIDs == NULL) {
            return MA_ERROR;
        }

        ma_device_info_init__sdl3(pContextStateSDL, MA_SDL3_DEFAULT_CAPTURE, "Default Capture Device", MA_TRUE, &info);
        if (callback(ma_device_type_capture, &info, pCallbackUserData) == MA_DEVICE_ENUMERATION_ABORT) {
            pContextStateSDL->SDL_free(pDeviceIDs);
            return MA_SUCCESS;
        }

        for (iDevice = 0; iDevice < deviceCount; iDevice += 1) {
            pName = pContextStateSDL->SDL_GetAudioDeviceName(pDeviceIDs[iDevice]);
            if (pName == NULL) {
                continue;   /* The device may have disconnected since enumeration. */
            }

            ma_device_info_init__sdl3(pContextStateSDL, pDeviceIDs[iDevice], pName, MA_FALSE, &info);
            if (callback(ma_device_type_capture, &info, pCallbackUserData) == MA_DEVICE_ENUMERATION_ABORT) {
                pContextStateSDL->SDL_free(pDeviceIDs);
                return MA_SUCCESS;
            }
        }

        pContextStateSDL->SDL_free(pDeviceIDs);
    }

    return MA_SUCCESS;
}

static ma_result ma_device_process_playback__sdl3(ma_device* pDevice)
{
    ma_device_state_sdl3* pState = ma_device_get_backend_state__sdl3(pDevice);
    ma_context_state_sdl3* pContextState = ma_context_get_backend_state__sdl3(ma_device_get_context(pDevice));
    ma_device_stream_sdl3* pPlayback = &pState->playback;
    ma_uint32 bytesRemaining;
    ma_uint32 bytesToWrite;
    int bytesQueued;
    ma_result result;

    bytesQueued = pContextState->SDL_GetAudioStreamQueued(pPlayback->pStream);
    if (bytesQueued < 0) {
        return MA_ERROR;
    }

    if ((ma_uint32)bytesQueued >= pPlayback->bufferSizeInBytes*2) {
        return MA_NO_DATA_AVAILABLE;
    }

    bytesRemaining = pPlayback->bufferSizeInBytes*2 - (ma_uint32)bytesQueued;
    bytesRemaining -= bytesRemaining % pPlayback->bytesPerFrame;
    if (bytesRemaining == 0) {
        return MA_NO_DATA_AVAILABLE;
    }

    while (bytesRemaining > 0) {
        bytesToWrite = ma_min(bytesRemaining, pPlayback->bufferSizeInBytes);

        result = ma_device_handle_backend_data_callback(pDevice, pPlayback->pBuffer, NULL, bytesToWrite / pPlayback->bytesPerFrame);
        if (result != MA_SUCCESS) {
            return result;
        }

        if (!pContextState->SDL_PutAudioStreamData(pPlayback->pStream, pPlayback->pBuffer, (int)bytesToWrite)) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[SDL3] Playback transfer failed: %s\n", pContextState->SDL_GetError());
            return MA_ERROR;
        }

        bytesRemaining -= bytesToWrite;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_process_capture__sdl3(ma_device* pDevice)
{
    ma_device_state_sdl3* pState = ma_device_get_backend_state__sdl3(pDevice);
    ma_context_state_sdl3* pContextState = ma_context_get_backend_state__sdl3(ma_device_get_context(pDevice));
    ma_device_stream_sdl3* pCapture = &pState->capture;
    ma_uint32 bytesRemaining;
    ma_uint32 bytesToRead;
    int bytesAvailable;
    int bytesRead;
    ma_result result;

    bytesAvailable = pContextState->SDL_GetAudioStreamAvailable(pCapture->pStream);
    if (bytesAvailable < 0) {
        return MA_ERROR;
    }

    bytesRemaining = (ma_uint32)bytesAvailable;
    bytesRemaining -= bytesRemaining % pCapture->bytesPerFrame;
    if (bytesRemaining == 0) {
        return MA_NO_DATA_AVAILABLE;
    }

    /* Read only the data available at entry so capture cannot delay playback indefinitely. */
    while (bytesRemaining > 0) {
        bytesToRead = ma_min(bytesRemaining, pCapture->bufferSizeInBytes);

        bytesRead = pContextState->SDL_GetAudioStreamData(pCapture->pStream, pCapture->pBuffer, (int)bytesToRead);
        if (bytesRead < 0) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[SDL3] Capture transfer failed: %s\n", pContextState->SDL_GetError());
            return MA_ERROR;
        }

        if (bytesRead == 0) {
            break;
        }

        result = ma_device_handle_backend_data_callback(pDevice, NULL, pCapture->pBuffer, (ma_uint32)bytesRead / pCapture->bytesPerFrame);
        if (result != MA_SUCCESS) {
            return result;
        }

        bytesRemaining -= (ma_uint32)bytesRead;
    }

    return MA_SUCCESS;
}

static void MA_SDL3_CALL ma_audio_callback__sdl3(void* pUserData, MA_SDL3_AudioStream* pStream, int additionalAmount, int totalAmount)
{
    ma_semaphore* pSemaphore = (ma_semaphore*)pUserData;

    (void)pStream;
    (void)additionalAmount;
    (void)totalAmount;

    ma_semaphore_release(pSemaphore);
}

static void ma_device_stream_uninit__sdl3(ma_device* pDevice, ma_context_state_sdl3* pContextState, ma_device_stream_sdl3* pStream)
{
    if (pStream->pStream != NULL) {
        pContextState->SDL_DestroyAudioStream(pStream->pStream);
        pStream->pStream = NULL;
    }

    ma_free(pStream->pBuffer, ma_device_get_allocation_callbacks(pDevice));
    pStream->pBuffer = NULL;
}

static ma_result ma_device_init_internal__sdl3(ma_device* pDevice, ma_context_state_sdl3* pContextState, ma_device_stream_sdl3* pStream, ma_semaphore* pSemaphore, ma_device_type deviceType, ma_device_descriptor* pDescriptor)
{
    MA_SDL3_AudioDeviceID deviceID = 0;
    MA_SDL3_AudioSpec spec;
    MA_SDL3_AudioSpec nativeSpec;
    ma_uint64 periodSizeInFrames;
    char periodSizeInFramesStr[16];
    int sampleFrames;
    ma_result result;

    if (pDescriptor->shareMode == ma_share_mode_exclusive) {
        return MA_SHARE_MODE_NOT_SUPPORTED;
    }

    if (pDescriptor->pDeviceID != NULL) {
        deviceID = pDescriptor->pDeviceID->custom.u;
    }

    if (deviceType == ma_device_type_playback) {
        if (deviceID == 0) {
            deviceID = MA_SDL3_DEFAULT_PLAYBACK;
        }
    } else {
        if (deviceID == 0) {
            deviceID = MA_SDL3_DEFAULT_CAPTURE;
        }
    }

    if (deviceType == ma_device_type_playback) {
        if (!pContextState->SDL_IsAudioDevicePlayback(deviceID)) {
            return MA_INVALID_DEVICE_CONFIG;
        }
    } else {
        if (pContextState->SDL_IsAudioDevicePlayback(deviceID)) {
            return MA_INVALID_DEVICE_CONFIG;
        }
    }

    if (!pContextState->SDL_GetAudioDeviceFormat(deviceID, &nativeSpec, NULL)) {
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    if (pDescriptor->channels > MA_MAX_CHANNELS || pDescriptor->channels > INT_MAX || pDescriptor->sampleRate > INT_MAX) {
        return MA_INVALID_DEVICE_CONFIG;
    }

    spec.format = ma_format_to_sdl3(pDescriptor->format);
    if (spec.format == 0) {
        spec.format = nativeSpec.format;
        if (ma_format_from_sdl3(spec.format) == ma_format_unknown) {
            spec.format = ma_format_to_sdl3(ma_format_f32);
        }
    }

    spec.channels = (int)pDescriptor->channels;
    if (spec.channels == 0) {
        spec.channels = nativeSpec.channels;
    }

    spec.freq = (int)pDescriptor->sampleRate;
    if (spec.freq == 0) {
        spec.freq = nativeSpec.freq;
    }

    if (spec.channels <= 0 || spec.channels > MA_MAX_CHANNELS || spec.freq <= 0) {
        return MA_INVALID_DEVICE_CONFIG;
    }

    /*
    SDL has an especially awkward way of setting the period size (latency). You have to set it via a
    hint called "SDL_AUDIO_DEVICE_SAMPLE_FRAMES", but it's a global setting rather than per-device.
    I don't recall ever seeing this before. And to make it better you need to pass it in as a
    string.

    From what I can tell, it is the size of the buffer before SDL does it's resampling. What this
    means, if my understanding is correct, is we need to specify the size based on that of the
    native sample rate of SDL's internal device instead of the front-facing rate of the stream. The
    calcualtion below performs the conversion. Advice welcome if I'm incorrect about this.
    */
    periodSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDescriptor, (ma_uint32)spec.freq);
    periodSizeInFrames = (periodSizeInFrames * (ma_uint32)nativeSpec.freq + (ma_uint32)spec.freq - 1) / (ma_uint32)spec.freq;

    /* The period size is set with SDL_SetHint() which means we need to convert to a string. Fun. */
    result = ma_sdl_uint_to_string((ma_uint32)periodSizeInFrames, periodSizeInFramesStr, sizeof(periodSizeInFramesStr));
    if (result != MA_SUCCESS) {
        return result;
    }

    pContextState->SDL_SetHint("SDL_AUDIO_DEVICE_SAMPLE_FRAMES", periodSizeInFramesStr);

    pStream->pStream = pContextState->SDL_OpenAudioDeviceStream(deviceID, &spec, ma_audio_callback__sdl3, pSemaphore);
    if (pStream->pStream == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[SDL3] Device open failed: %s\n", pContextState->SDL_GetError());
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    /* Get the negotiated format in case it differs from what we requested. */
    if (!pContextState->SDL_GetAudioDeviceFormat(pContextState->SDL_GetAudioStreamDevice(pStream->pStream), &nativeSpec, &sampleFrames) || nativeSpec.freq <= 0 || sampleFrames <= 0) {
        ma_device_stream_uninit__sdl3(pDevice, pContextState, pStream);
        return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
    }

    periodSizeInFrames = ((ma_uint64)sampleFrames * (ma_uint32)spec.freq + (ma_uint32)nativeSpec.freq - 1) / (ma_uint32)nativeSpec.freq;

    pStream->bytesPerFrame     = ma_get_bytes_per_frame(ma_format_from_sdl3(spec.format), (ma_uint32)spec.channels);
    pStream->bufferSizeInBytes = (ma_uint32)periodSizeInFrames * pStream->bytesPerFrame;

    pStream->pBuffer = ma_malloc(pStream->bufferSizeInBytes, ma_device_get_allocation_callbacks(pDevice));
    if (pStream->pBuffer == NULL) {
        ma_device_stream_uninit__sdl3(pDevice, pContextState, pStream);
        return MA_OUT_OF_MEMORY;
    }

    pDescriptor->format                   = ma_format_from_sdl3(spec.format);
    pDescriptor->channels                 = (ma_uint32)spec.channels;
    pDescriptor->sampleRate               = (ma_uint32)spec.freq;
    pDescriptor->periodSizeInFrames       = (ma_uint32)periodSizeInFrames;
    pDescriptor->periodSizeInMilliseconds = 0;
    pDescriptor->periodCount              = 2;

    ma_channel_map_init_standard(ma_standard_channel_map_sdl, pDescriptor->channelMap, ma_countof(pDescriptor->channelMap), pDescriptor->channels);

    return MA_SUCCESS;
}

static ma_result ma_device_init__sdl3(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    ma_context_state_sdl3* pContextState = ma_context_get_backend_state__sdl3(ma_device_get_context(pDevice));
    ma_device_state_sdl3* pState;
    ma_device_type deviceType;
    ma_result result;

    (void)pDeviceBackendConfig;

    *ppDeviceState = NULL;
    
    deviceType = ma_device_get_type(pDevice);
    if (deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    pState = (ma_device_state_sdl3*)ma_calloc(sizeof(*pState), ma_device_get_allocation_callbacks(pDevice));
    if (pState == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_semaphore_init(0, &pState->semaphore);
    if (result != MA_SUCCESS) {
        ma_free(pState, ma_device_get_allocation_callbacks(pDevice));
        return result;
    }

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__sdl3(pDevice, pContextState, &pState->capture, &pState->semaphore, ma_device_type_capture, pDescriptorCapture);
        if (result != MA_SUCCESS) {
            goto error;
        }
    }

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__sdl3(pDevice, pContextState, &pState->playback, &pState->semaphore, ma_device_type_playback, pDescriptorPlayback);
        if (result != MA_SUCCESS) {
            goto error;
        }
    }

    *ppDeviceState = pState;
    return MA_SUCCESS;

error:
    ma_device_stream_uninit__sdl3(pDevice, pContextState, &pState->capture);
    ma_device_stream_uninit__sdl3(pDevice, pContextState, &pState->playback);
    ma_semaphore_uninit(&pState->semaphore);
    ma_free(pState, ma_device_get_allocation_callbacks(pDevice));
    return result;
}

static void ma_device_uninit__sdl3(ma_device* pDevice)
{
    ma_device_state_sdl3* pState = ma_device_get_backend_state__sdl3(pDevice);
    ma_context_state_sdl3* pContextState = ma_context_get_backend_state__sdl3(ma_device_get_context(pDevice));

    ma_device_stream_uninit__sdl3(pDevice, pContextState, &pState->capture);
    ma_device_stream_uninit__sdl3(pDevice, pContextState, &pState->playback);
    ma_semaphore_uninit(&pState->semaphore);
    ma_free(pState, ma_device_get_allocation_callbacks(pDevice));
}

static ma_result ma_device_stop__sdl3(ma_device* pDevice)
{
    ma_device_state_sdl3* pState = ma_device_get_backend_state__sdl3(pDevice);
    ma_context_state_sdl3* pContextState = ma_context_get_backend_state__sdl3(ma_device_get_context(pDevice));
    ma_result result = MA_SUCCESS;

    if (pState->capture.pStream != NULL) {
        if (!pContextState->SDL_PauseAudioStreamDevice(pState->capture.pStream)) {
            result = MA_ERROR;
        }

        if (!pContextState->SDL_ClearAudioStream(pState->capture.pStream)) {
            result = MA_ERROR;
        }
    }

    if (pState->playback.pStream != NULL) {
        if (!pContextState->SDL_PauseAudioStreamDevice(pState->playback.pStream)) {
            result = MA_ERROR;
        }

        if (!pContextState->SDL_ClearAudioStream(pState->playback.pStream)) {
            result = MA_ERROR;
        }
    }

    return result;
}

static ma_result ma_device_start__sdl3(ma_device* pDevice)
{
    ma_device_state_sdl3* pState = ma_device_get_backend_state__sdl3(pDevice);
    ma_context_state_sdl3* pContextState = ma_context_get_backend_state__sdl3(ma_device_get_context(pDevice));
    ma_result result;

    /* Duplex playback is filled after the device enters the started state. */
    if (ma_device_get_type(pDevice) == ma_device_type_playback) {
        result = ma_device_process_playback__sdl3(pDevice);
        if (result != MA_SUCCESS) {
            goto error;
        }
    }

    if (pState->capture.pStream != NULL) {
        if (!pContextState->SDL_ResumeAudioStreamDevice(pState->capture.pStream)) {
            goto error;
        }
    }

    if (pState->playback.pStream != NULL) {
        if (!pContextState->SDL_ResumeAudioStreamDevice(pState->playback.pStream)) {
            goto error;
        }
    }

    return MA_SUCCESS;

error:
    ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[SDL3] Device start failed: %s\n", pContextState->SDL_GetError());
    ma_device_stop__sdl3(pDevice);
    return MA_FAILED_TO_START_BACKEND_DEVICE;
}

static ma_result ma_device_step__sdl3(ma_device* pDevice, ma_blocking_mode blockingMode)
{
    ma_device_state_sdl3* pState = ma_device_get_backend_state__sdl3(pDevice);
    ma_result result;
    ma_bool32 hasProcessedData = MA_FALSE;

    /* We should not be blocking on web. We will force non-blocking mode with the Emscripten build. */
    #if defined(MA_EMSCRIPTEN)
    if (ma_device_get_threading_mode(pDevice) == MA_THREADING_MODE_SINGLE_THREADED) {
        blockingMode = MA_BLOCKING_MODE_NON_BLOCKING;
    }
    #endif

    while (ma_device_is_started(pDevice)) {
        if (pState->capture.pStream != NULL) {
            result = ma_device_process_capture__sdl3(pDevice);
            if (result == MA_SUCCESS) {
                hasProcessedData = MA_TRUE;
            } else if (result != MA_NO_DATA_AVAILABLE) {
                return result;
            }
        }

        if (pState->playback.pStream != NULL) {
            result = ma_device_process_playback__sdl3(pDevice);
            if (result == MA_SUCCESS) {
                hasProcessedData = MA_TRUE;
            } else if (result != MA_NO_DATA_AVAILABLE) {
                return result;
            }
        }

        if (hasProcessedData || blockingMode == MA_BLOCKING_MODE_NON_BLOCKING) {
            break;
        }

        result = ma_semaphore_wait(&pState->semaphore);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    return MA_SUCCESS;
}

static void ma_device_wakeup__sdl3(ma_device* pDevice)
{
    ma_device_state_sdl3* pState = ma_device_get_backend_state__sdl3(pDevice);
    ma_semaphore_release(&pState->semaphore);
}

static ma_device_backend_vtable ma_gDeviceBackendVTable_SDL3 =
{
    ma_backend_info__sdl3,
    ma_context_init__sdl3,
    ma_context_uninit__sdl3,
    ma_context_enumerate_devices__sdl3,
    ma_device_init__sdl3,
    ma_device_uninit__sdl3,
    ma_device_start__sdl3,
    ma_device_stop__sdl3,
    ma_device_step__sdl3,
    ma_device_wakeup__sdl3
};

ma_device_backend_vtable* ma_device_backend_sdl3 = &ma_gDeviceBackendVTable_SDL3;
#else
ma_device_backend_vtable* ma_device_backend_sdl3 = NULL;
#endif  /* MA_HAS_SDL3 */

MA_API ma_device_backend_vtable* ma_sdl3_get_vtable(void)
{
    return ma_device_backend_sdl3;
}


MA_API ma_context_config_sdl3 ma_context_config_sdl3_init(void)
{
    ma_context_config_sdl3 config;

    memset(&config, 0, sizeof(config));

    return config;
}

MA_API ma_device_config_sdl3 ma_device_config_sdl3_init(void)
{
    ma_device_config_sdl3 config;

    memset(&config, 0, sizeof(config));

    return config;
}


#endif  /* miniaudio_backend_sdl3_c */
