/*
Cannot use SPA_AUDIO_INFO_RAW_INIT because it's a define with varargs.
*/
#ifndef miniaudio_backend_pipewire_c
#define miniaudio_backend_pipewire_c

#include "miniaudio_pipewire.h"

#include <string.h> /* memset() */
#include <assert.h> /* assert() */

#ifndef MA_PIPEWIRE_ASSERT
#define MA_PIPEWIRE_ASSERT(x) assert(x)
#endif

#ifndef MA_PIPEWIRE_ZERO_OBJECT
#define MA_PIPEWIRE_ZERO_OBJECT(x) memset((x), 0, sizeof(*(x)))
#endif


/*
NOTES FOR NEW BACKEND SYSTEM

There are currently a few problems with the existing backend system:

    - Backends depend on knowledge of `ma_device` and `ma_context`. It would be better if they had a clearer separation of concerns.

    - For backends that require explicit handling of automatic stream routing, things can get annoyingly awkward when the backend
      updates it's internal state and needs to tell the device about it's new properties, such as the new sample rate, etc.

    - There is a non-trivial burden put onto backends when it comes to duplex mode. It would be good if there was a way where the
      backend could just concern itself with playback or capture, and then have miniaudio deal with the duplex part of it. Backends
      should still be able to support duplex mode themselves in case they can do something more optimal.

    - Due to the combination of tight coupling with `ma_device`, and the application being in control of when the `ma_device` object
      is destroyed, things can get a bit messy because the backend may be in the middle of something, such as a device reroute, when
      the application decides to destroy the device. By decoupling the backend state from the `ma_device` object, it could give the
      backend more flexibility to handle this gracefully.
      


Some random thoughts on how to improve things follow.

One idea is to introduce the concept of a "backend state". The backend will concern itself only with this state. It will have no
knowledge of `ma_device` or `ma_context`. It will only care or know about it's own state.

When a backend initializes a context or device, it'll allocate a state object containing only backend-specific information. This state
object can then be installed into the `ma_context` or `ma_device` object. When creating the state object, miniaudio will provide the
necessary information via a configuration structure. When installing the state object, the backend will provide the necessary information
back to miniaudio via a descriptor structure.

There can be two different state objects for a device: one for playback and one for capture. When a device is initialized as duplex,
but is not supported by the backend, the backend can return `MA_DEVICE_TYPE_NOT_SUPPORTED`. When this happens, miniaudio can fall back
to creating a separate playback and capture device and then deal with the duplex part itself through the use of a proxy data callback.

Decoupling the backend state from the `ma_device` object will allow the backend to keep the state object alive while something like a
device reroute is still in progress. This is important because applications can destroy the `ma_device` object at any time.

When a backend state is installed, it is always accompanied by a `ma_device_descriptor` object. In device initialization, these are
passed into the initialization routine as both an input and output parameter. For installing a device state after the device has
been initialized, such as when a device reroute occurs, a function called something like `ma_device_install_backend_state()` can be
used:

    ma_result ma_device_install_backend_state(ma_device* pDevice, ma_device_type deviceType, const ma_device_descriptor* pDescriptor, void* pBackendState);

This function would replace `ma_device_post_init()`.

Need a way for backends to process audio data easily. Maybe pass in a `ma_device_data_callback` object to the initialization function
which can then be stored in the backend state?

    ma_device_data_callback_process(pDeviceDataCallback, pFramesOut, pFramesIn, frameCount);


*/



#if defined(MA_LINUX)
    #define MA_SUPPORT_PIPEWIRE
#endif

#if defined(MA_SUPPORT_PIPEWIRE) && !defined(MA_NO_PIPEWIRE) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_PIPEWIRE))
    #define MA_HAS_PIPEWIRE
#endif

#if defined(MA_HAS_PIPEWIRE)
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wc99-extensions"
    #pragma GCC diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
    #pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
/*#include <pipewire/pipewire.h>*/
#include <spa/param/audio/format-utils.h>   /* For spa_format_audio_raw_build() */
#include <spa/param/audio/raw.h>
#include <spa/utils/dict.h>
#include <spa/pod/command.h>
#include <spa/buffer/buffer.h>

#define MA_PW_KEY_MEDIA_TYPE        "media.type"
#define MA_PW_KEY_MEDIA_CATEGORY    "media.category"
#define MA_PW_KEY_MEDIA_ROLE        "media.role"

#define MA_PW_ID_ANY                0xFFFFFFFF

struct ma_pw_thread_loop;
struct ma_pw_loop;
struct ma_pw_context;
struct ma_pw_core;
struct ma_pw_properties;
struct ma_pw_stream;
struct ma_pw_stream_control;

enum ma_pw_stream_state
{
    MA_PW_STREAM_STATE_ERROR       = -1,
    MA_PW_STREAM_STATE_UNCONNECTED =  0,
    MA_PW_STREAM_STATE_CONNECTING  =  1,
    MA_PW_STREAM_STATE_PAUSED      =  2,
    MA_PW_STREAM_STATE_STREAMING   =  3
};

enum ma_pw_stream_flags
{
    MA_PW_STREAM_FLAG_NONE           = 0,
    MA_PW_STREAM_FLAG_AUTOCONNECT    = (1 << 0),
    MA_PW_STREAM_FLAG_INACTIVE       = (1 << 1),
    MA_PW_STREAM_FLAG_MAP_BUFFERS    = (1 << 2),
    MA_PW_STREAM_FLAG_DRIVER         = (1 << 3),
    MA_PW_STREAM_FLAG_RT_PROCESS     = (1 << 4),
    MA_PW_STREAM_FLAG_NO_CONVERT     = (1 << 5),
    MA_PW_STREAM_FLAG_EXCLUSIVE      = (1 << 6),
    MA_PW_STREAM_FLAG_DONT_RECONNECT = (1 << 7),
    MA_PW_STREAM_FLAG_ALLOC_BUFFERS  = (1 << 8),
    MA_PW_STREAM_FLAG_TRIGGER        = (1 << 9),
    MA_PW_STREAM_FLAG_ASYNC          = (1 << 10)
};

struct ma_pw_buffer
{
    struct spa_buffer* buffer;
    void* user_data;
    ma_uint64 size;
    ma_uint64 requested;
};


#define MA_PW_VERSION_STREAM_EVENTS 2

struct ma_pw_stream_events
{
    ma_uint32 version;
    void (* destroy      )(void* data);
    void (* state_changed)(void* data, enum ma_pw_stream_state oldState, enum ma_pw_stream_state newState, const char* error);
    void (* control_info )(void* data, ma_uint32 id, const struct ma_pw_stream_control* control);
    void (* io_changed   )(void* data, ma_uint32 id, void* area, ma_uint32 size);
    void (* param_changed)(void* data, ma_uint32 id, const struct spa_pod* param);
    void (* add_buffer   )(void* data, struct ma_pw_buffer* buffer);
    void (* remove_buffer)(void* data, struct ma_pw_buffer* buffer);
    void (* process      )(void* data);
    void (* drained      )(void* data);
    void (* command      )(void* data, const struct spa_command* command);
    void (* trigger_done )(void* data);
};

typedef void                      (* ma_pw_init_proc                 )(int* argc, char*** argv);
typedef void                      (* ma_pw_deinit_proc               )(void);
typedef struct ma_pw_thread_loop* (* ma_pw_thread_loop_new_proc      )(const char* name, const struct spa_dict* props);
typedef void                      (* ma_pw_thread_loop_destroy_proc  )(struct ma_pw_thread_loop* loop);
typedef struct ma_pw_loop*        (* ma_pw_thread_loop_get_loop_proc )(struct ma_pw_thread_loop* loop);
typedef int                       (* ma_pw_thread_loop_start_proc    )(struct ma_pw_thread_loop* loop);
typedef void                      (* ma_pw_thread_loop_lock_proc     )(struct ma_pw_thread_loop* loop);
typedef void                      (* ma_pw_thread_loop_unlock_proc   )(struct ma_pw_thread_loop* loop);
typedef struct ma_pw_context*     (* ma_pw_context_new_proc          )(struct ma_pw_loop* loop, const char* name, const struct spa_dict* props);
typedef void                      (* ma_pw_context_destroy_proc      )(struct ma_pw_context* context);
typedef struct ma_pw_core*        (* ma_pw_context_connect_proc      )(struct ma_pw_context* context, struct ma_pw_properties* properties, size_t user_data_size);
typedef void                      (* ma_pw_core_disconnect_proc      )(struct ma_pw_core* core);
typedef struct ma_pw_properties*  (* ma_pw_properties_new_proc       )(const char* key, ...);
typedef void                      (* ma_pw_properties_free_proc      )(struct ma_pw_properties* properties);
typedef struct ma_pw_stream*      (* ma_pw_stream_new_proc           )(struct ma_pw_core* core, const char* name, struct ma_pw_properties* props);
typedef void                      (* ma_pw_stream_destroy_proc       )(struct ma_pw_stream* stream);
typedef void                      (* ma_pw_stream_add_listener_proc  )(struct ma_pw_stream* stream, struct spa_hook* listener, const struct ma_pw_stream_events* events, void* data);
typedef int                       (* ma_pw_stream_connect_proc       )(struct ma_pw_stream* stream, enum spa_direction direction, ma_uint32 target_id, enum ma_pw_stream_flags flags, const struct spa_pod** params, ma_uint32 paramCount);
typedef int                       (* ma_pw_stream_set_active_proc    )(struct ma_pw_stream* stream, bool active);
typedef struct ma_pw_buffer*      (* ma_pw_stream_dequeue_buffer_proc)(struct ma_pw_stream* stream);
typedef int                       (* ma_pw_stream_queue_buffer_proc  )(struct ma_pw_stream* stream, struct ma_pw_buffer* buffer);
typedef int                       (* ma_pw_stream_update_params_proc )(struct ma_pw_stream* stream, const struct spa_pod** params, ma_uint32 paramCount);

typedef struct
{
    ma_handle hPipeWire;
    ma_pw_init_proc                  pw_init;
    ma_pw_deinit_proc                pw_deinit;
    ma_pw_thread_loop_new_proc       pw_thread_loop_new;
    ma_pw_thread_loop_destroy_proc   pw_thread_loop_destroy;
    ma_pw_thread_loop_get_loop_proc  pw_thread_loop_get_loop;
    ma_pw_thread_loop_start_proc     pw_thread_loop_start;
    ma_pw_thread_loop_lock_proc      pw_thread_loop_lock;
    ma_pw_thread_loop_unlock_proc    pw_thread_loop_unlock;
    ma_pw_context_new_proc           pw_context_new;
    ma_pw_context_destroy_proc       pw_context_destroy;
    ma_pw_context_connect_proc       pw_context_connect;
    ma_pw_core_disconnect_proc       pw_core_disconnect;
    ma_pw_properties_new_proc        pw_properties_new;
    ma_pw_properties_free_proc       pw_properties_free;
    ma_pw_stream_new_proc            pw_stream_new;
    ma_pw_stream_destroy_proc        pw_stream_destroy;
    ma_pw_stream_add_listener_proc   pw_stream_add_listener;
    ma_pw_stream_connect_proc        pw_stream_connect;
    ma_pw_stream_set_active_proc     pw_stream_set_active;
    ma_pw_stream_dequeue_buffer_proc pw_stream_dequeue_buffer;
    ma_pw_stream_queue_buffer_proc   pw_stream_queue_buffer;
    ma_pw_stream_update_params_proc  pw_stream_update_params;
} ma_context_state_pipewire;

typedef struct
{
    struct ma_pw_thread_loop* pThreadLoop;
    struct ma_pw_context* pContext;
    struct ma_pw_core* pCore;
    struct ma_pw_stream* pStreamPlayback;
    struct ma_pw_stream* pStreamCapture;
    struct spa_hook eventListener;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_bool32 isInitialized;
    ma_bool32 isInternalFormatFinalised;
    struct
    {
        ma_device_descriptor* pDescriptor;      /* This is only used for setting up internal format. It's needed here because it looks like the only way to get the internal format is via a stupid callback. */
        struct ma_pw_stream* pStream;           /* The stream that's being configured. One of either pStreamPlayback or pStreamCapture. Used for the stupid param_changed callback. TODO: I think this can be removed. */
    } paramChangedCallbackData;
} ma_device_state_pipewire;


static enum spa_audio_format ma_format_to_pipewire(ma_format format)
{
    switch (format)
    {
        case ma_format_u8:  return SPA_AUDIO_FORMAT_U8;
        case ma_format_s16: return SPA_AUDIO_FORMAT_S16;
        case ma_format_s24: return SPA_AUDIO_FORMAT_S24;
        case ma_format_s32: return SPA_AUDIO_FORMAT_S32;
        case ma_format_f32: return SPA_AUDIO_FORMAT_F32;
        default: return SPA_AUDIO_FORMAT_UNKNOWN;
    }
}

static ma_format ma_format_from_pipewire(enum spa_audio_format format)
{
    switch (format)
    {
        case SPA_AUDIO_FORMAT_U8:  return ma_format_u8;
        case SPA_AUDIO_FORMAT_S16: return ma_format_s16;
        case SPA_AUDIO_FORMAT_S24: return ma_format_s24;
        case SPA_AUDIO_FORMAT_S32: return ma_format_s32;
        case SPA_AUDIO_FORMAT_F32: return ma_format_f32;
        default: return ma_format_unknown;
    }
}


static ma_context_state_pipewire* ma_context_get_backend_state__pipewire(ma_context* pContext)
{
    return (ma_context_state_pipewire*)ma_context_get_backend_state(pContext);
}

static ma_device_state_pipewire* ma_device_get_backend_state__pipewire(ma_device* pDevice)
{
    return (ma_device_state_pipewire*)ma_device_get_backend_state(pDevice);
}


static void ma_backend_info__pipewire(ma_device_backend_info* pBackendInfo)
{
    MA_PIPEWIRE_ASSERT(pBackendInfo != NULL);
    pBackendInfo->pName = "PipeWire";
}

static ma_result ma_context_init__pipewire(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    /* We'll use a list of possible shared object names for easier extensibility. */
    ma_context_state_pipewire* pContextStatePipeWire;
    ma_context_config_pipewire* pContextConfigPipeWire = (ma_context_config_pipewire*)pContextBackendConfig;
    ma_log* pLog = ma_context_get_log(pContext);
    ma_handle hPipeWire;
    size_t iName;
    const char* pSONames[] = {
        "libpipewire-0.3.so.0",
        "libpipewire.so"
    };

    (void)pContextConfigPipeWire;

    pContextStatePipeWire = (ma_context_state_pipewire*)ma_calloc(sizeof(*pContextStatePipeWire), ma_context_get_allocation_callbacks(pContext));
    if (pContextStatePipeWire == NULL) {
        return MA_OUT_OF_MEMORY;
    }


    /* Check if we have a PipeWire SO. If we can't find this we need to abort. */
    for (iName = 0; iName < ma_countof(pSONames); iName += 1) {
        hPipeWire = ma_dlopen(pLog, pSONames[iName]);
        if (hPipeWire != NULL) {
            break;
        }
    }

    if (hPipeWire == NULL) {
        ma_free(pContextStatePipeWire, ma_context_get_allocation_callbacks(pContext));
        return MA_NO_BACKEND;   /* PipeWire could not be loaded. */
    }

    /* Now that we have the handle to the shared object we can go ahead and load some function pointers. */
    pContextStatePipeWire->hPipeWire = hPipeWire;
    pContextStatePipeWire->pw_init                  = (ma_pw_init_proc                 )ma_dlsym(pLog, hPipeWire, "pw_init");
    pContextStatePipeWire->pw_deinit                = (ma_pw_deinit_proc               )ma_dlsym(pLog, hPipeWire, "pw_deinit");
    pContextStatePipeWire->pw_thread_loop_new       = (ma_pw_thread_loop_new_proc      )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_new");
    pContextStatePipeWire->pw_thread_loop_destroy   = (ma_pw_thread_loop_destroy_proc  )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_destroy");
    pContextStatePipeWire->pw_thread_loop_get_loop  = (ma_pw_thread_loop_get_loop_proc )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_get_loop");
    pContextStatePipeWire->pw_thread_loop_start     = (ma_pw_thread_loop_start_proc    )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_start");
    pContextStatePipeWire->pw_thread_loop_lock      = (ma_pw_thread_loop_lock_proc     )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_lock");
    pContextStatePipeWire->pw_thread_loop_unlock    = (ma_pw_thread_loop_unlock_proc   )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_unlock");
    pContextStatePipeWire->pw_context_new           = (ma_pw_context_new_proc          )ma_dlsym(pLog, hPipeWire, "pw_context_new");
    pContextStatePipeWire->pw_context_destroy       = (ma_pw_context_destroy_proc      )ma_dlsym(pLog, hPipeWire, "pw_context_destroy");
    pContextStatePipeWire->pw_context_connect       = (ma_pw_context_connect_proc      )ma_dlsym(pLog, hPipeWire, "pw_context_connect");
    pContextStatePipeWire->pw_core_disconnect       = (ma_pw_core_disconnect_proc      )ma_dlsym(pLog, hPipeWire, "pw_core_disconnect");
    pContextStatePipeWire->pw_properties_new        = (ma_pw_properties_new_proc       )ma_dlsym(pLog, hPipeWire, "pw_properties_new");
    pContextStatePipeWire->pw_properties_free       = (ma_pw_properties_free_proc      )ma_dlsym(pLog, hPipeWire, "pw_properties_free");
    pContextStatePipeWire->pw_stream_new            = (ma_pw_stream_new_proc           )ma_dlsym(pLog, hPipeWire, "pw_stream_new");
    pContextStatePipeWire->pw_stream_destroy        = (ma_pw_stream_destroy_proc       )ma_dlsym(pLog, hPipeWire, "pw_stream_destroy");
    pContextStatePipeWire->pw_stream_add_listener   = (ma_pw_stream_add_listener_proc  )ma_dlsym(pLog, hPipeWire, "pw_stream_add_listener");
    pContextStatePipeWire->pw_stream_connect        = (ma_pw_stream_connect_proc       )ma_dlsym(pLog, hPipeWire, "pw_stream_connect");
    pContextStatePipeWire->pw_stream_set_active     = (ma_pw_stream_set_active_proc    )ma_dlsym(pLog, hPipeWire, "pw_stream_set_active");
    pContextStatePipeWire->pw_stream_dequeue_buffer = (ma_pw_stream_dequeue_buffer_proc)ma_dlsym(pLog, hPipeWire, "pw_stream_dequeue_buffer");
    pContextStatePipeWire->pw_stream_queue_buffer   = (ma_pw_stream_queue_buffer_proc  )ma_dlsym(pLog, hPipeWire, "pw_stream_queue_buffer");
    pContextStatePipeWire->pw_stream_update_params  = (ma_pw_stream_update_params_proc )ma_dlsym(pLog, hPipeWire, "pw_stream_update_params");

    if (pContextStatePipeWire->pw_init != NULL) {
        pContextStatePipeWire->pw_init(NULL, NULL);
    }

    *ppContextState = pContextStatePipeWire;

    return MA_SUCCESS;
}

static void ma_context_uninit__pipewire(ma_context* pContext)
{
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(pContext);

    MA_PIPEWIRE_ASSERT(pContextStatePipeWire != NULL);

    if (pContextStatePipeWire->pw_deinit != NULL) {
        pContextStatePipeWire->pw_deinit();
    }

    /* Close the handle to the PipeWire shared object last. */
    ma_dlclose(ma_context_get_log(pContext), pContextStatePipeWire->hPipeWire);
    pContextStatePipeWire->hPipeWire = NULL;

    ma_free(pContextStatePipeWire, ma_context_get_allocation_callbacks(pContext));
}

static ma_result ma_context_enumerate_devices__pipewire(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(pContext);
    ma_bool32 cbResult = MA_TRUE;

    MA_PIPEWIRE_ASSERT(pContextStatePipeWire != NULL);
    MA_PIPEWIRE_ASSERT(callback != NULL);

    /* TODO: Proper device enumeration. */

    /* Playback. */
    if (cbResult) {
        ma_device_info deviceInfo;
        MA_PIPEWIRE_ZERO_OBJECT(&deviceInfo);
        deviceInfo.id.custom.i = 0;
        ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), "Default Playback Device", (size_t)-1);

        cbResult = callback(ma_device_type_playback, &deviceInfo, pCallbackUserData);
    }

    /* Capture. */
    if (cbResult) {
        ma_device_info deviceInfo;
        MA_PIPEWIRE_ZERO_OBJECT(&deviceInfo);
        deviceInfo.id.custom.i = 0;
        ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), "Default Capture Device", (size_t)-1);

        cbResult = callback(ma_device_type_capture, &deviceInfo, pCallbackUserData);
    }

    return MA_SUCCESS;
}

static ma_result ma_context_get_device_info__pipewire(ma_context* pContext, ma_device_type deviceType, const ma_device_id* pDeviceID, ma_device_info* pDeviceInfo)
{
    /* TODO: Implement this properly. */

    (void)pContext;
    (void)pDeviceID;

    MA_PIPEWIRE_ZERO_OBJECT(pDeviceInfo);

    /* ID */
    pDeviceInfo->id.custom.i = 0;

    /* Name */
    if (deviceType == ma_device_type_playback) {
        ma_strncpy_s(pDeviceInfo->name, sizeof(pDeviceInfo->name), "Default Playback Device", (size_t)-1);
    } else {
        ma_strncpy_s(pDeviceInfo->name, sizeof(pDeviceInfo->name), "Default Capture Device", (size_t)-1);
    }

    pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].format     = ma_format_unknown;
    pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].channels   = 0;
    pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].sampleRate = 0;
    pDeviceInfo->nativeDataFormats[pDeviceInfo->nativeDataFormatCount].flags      = 0;
    pDeviceInfo->nativeDataFormatCount += 1;

    return MA_SUCCESS;
}


static void ma_stream_event_param_changed__pipewire(void* pUserData, ma_uint32 id, const struct spa_pod* pParam)
{
    ma_device* pDevice = (ma_device*)pUserData;
    ma_device_state_pipewire*  pDeviceStatePipeWire  = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));

    if (pParam != NULL && id == SPA_PARAM_Format) {
        struct spa_audio_info_raw audioInfo;
        char podBuilderBuffer[1024];
        struct spa_pod_builder podBuilder;
        const struct spa_pod* pBufferParameters[1];
        ma_uint32 bufferSizeInFrames;
        ma_uint32 bytesPerFrame;

        if (pDeviceStatePipeWire->isInternalFormatFinalised) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_WARNING, "PipeWire format parameter changed after device has been initialized.");
            return;
        }

        printf("Param Changed: %d\n", id);

        /*
        We can now determine the format/channels/rate which will let us configure the size of the buffer and set the
        internal format of the device.
        */
        spa_format_audio_raw_parse(pParam, &audioInfo);

        printf("Format:   %d\n", audioInfo.format);
        printf("Channels: %d\n", audioInfo.channels);
        printf("Rate:     %d\n", audioInfo.rate);
        printf("Channel Map: {");
        for (ma_uint32 iChannel = 0; iChannel < audioInfo.channels; iChannel += 1) {
            printf("%d", audioInfo.position[iChannel]);
            if (iChannel < audioInfo.channels - 1) {
                printf(", ");
            }
        }
        printf("}\n");


        /* Now that we definitely know the sample rate, we can reliable configure the size of the buffer. */
        bufferSizeInFrames = pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->periodSizeInFrames;
        if (bufferSizeInFrames == 0) {
            bufferSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor, (ma_uint32)audioInfo.rate);
        }

        /* Update the descriptor. This is where the internal format/channels/rate is set. */
        pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->format             = ma_format_from_pipewire(audioInfo.format);
        pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->channels           = audioInfo.channels;
        pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->sampleRate         = audioInfo.rate;
        pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->periodSizeInFrames = bufferSizeInFrames;
        pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->periodCount        = 2;


        bytesPerFrame = ma_get_bytes_per_frame(pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->format, pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->channels);
        
        /* Now update the PipeWire buffer properties. */
        podBuilder = SPA_POD_BUILDER_INIT(podBuilderBuffer, sizeof(podBuilderBuffer));

        pBufferParameters[0] = (const spa_pod*)spa_pod_builder_add_object(&podBuilder,
            SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
            SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor->periodCount, 2, 8),
            SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
            SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(bytesPerFrame),
            SPA_PARAM_BUFFERS_size,    SPA_POD_Int(bytesPerFrame * bufferSizeInFrames));

        pContextStatePipeWire->pw_stream_update_params(pDeviceStatePipeWire->paramChangedCallbackData.pStream, pBufferParameters, sizeof(pBufferParameters) / sizeof(pBufferParameters[0]));

        pDeviceStatePipeWire->isInternalFormatFinalised = MA_TRUE;
    }
}

static void ma_stream_event_process__pipewire(void* pUserData)
{
    ma_device* pDevice = (ma_device*)pUserData;
    ma_device_state_pipewire*  pDeviceStatePipeWire  = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));
    struct ma_pw_stream* pStream;
    struct ma_pw_buffer* pBuffer;
    ma_uint32 bytesPerFrame;
    ma_uint32 frameCount;
    ma_device_type deviceType;

    if (!pDeviceStatePipeWire->isInitialized) {
        printf("Not initialized\n");
        return;
    }

    deviceType = ma_device_get_type(pDevice);

    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        pStream = pDeviceStatePipeWire->pStreamPlayback;
        bytesPerFrame = ma_get_bytes_per_frame(pDeviceStatePipeWire->format, pDeviceStatePipeWire->channels);
    } else {
        pStream = pDeviceStatePipeWire->pStreamCapture;
        bytesPerFrame = ma_get_bytes_per_frame(pDeviceStatePipeWire->format, pDeviceStatePipeWire->channels);
    }

    for (;;) {
        pBuffer = pContextStatePipeWire->pw_stream_dequeue_buffer(pStream);
        if (pBuffer == NULL) {
            return;
        }

        if (pBuffer->buffer->datas[0].data == NULL) {
            return;
        }

        //frameCount = (ma_uint32)ma_min(pBuffer->requested, pBuffer->buffer->datas[0].maxsize / bytesPerFrame);
        frameCount = (ma_uint32)(pBuffer->buffer->datas[0].maxsize / bytesPerFrame);
        if (frameCount > 0) {
            if (pStream == pDeviceStatePipeWire->pStreamPlayback) {
                printf("(Playback) Processing %d frames... %d %d\n", (int)frameCount, (int)pBuffer->requested, pBuffer->buffer->datas[0].maxsize / bytesPerFrame);
                ma_device_handle_backend_data_callback(pDevice, pBuffer->buffer->datas[0].data, NULL, frameCount);
            } else {
                printf("(Capture) Processing %d frames...\n", (int)frameCount);
                ma_device_handle_backend_data_callback(pDevice, NULL, pBuffer->buffer->datas[0].data, frameCount);
            }

            //printf("Done...\n");
        } else {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_WARNING, "(PipeWire) No frames to process.\n");
        }

        pBuffer->buffer->datas[0].chunk->offset = 0;
        pBuffer->buffer->datas[0].chunk->stride = bytesPerFrame;
        pBuffer->buffer->datas[0].chunk->size   = frameCount * bytesPerFrame;

        pContextStatePipeWire->pw_stream_queue_buffer(pStream, pBuffer);
    }
}

static const struct ma_pw_stream_events ma_gStreamEventsPipeWire =
{
    MA_PW_VERSION_STREAM_EVENTS,
    NULL, /* destroy       */
    NULL, /* state_changed */
    NULL, /* control_info  */
    NULL, /* io_changed    */
    ma_stream_event_param_changed__pipewire,
    NULL, /* add_buffer    */
    NULL, /* remove_buffer */
    ma_stream_event_process__pipewire,
    NULL, /* drained       */
    NULL, /* command       */
    NULL, /* trigger_done  */
};

static ma_result ma_device_init_internal__pipewire(ma_device* pDevice, ma_context_state_pipewire* pContextStatePipeWire, ma_device_state_pipewire* pDeviceStatePipeWire, const ma_device_config_pipewire* pDeviceConfigPipeWire, ma_device_type deviceType, ma_device_descriptor* pDescriptor)
{
    struct spa_audio_info_raw audioInfo;
    struct ma_pw_stream* pStream;
    struct ma_pw_properties* pProperties;
    char podBuilderBuffer[1024];    /* A random buffer for use by the POD builder. I have no idea what the purpose of this is and what an appropriate size it should be set to. Why is this even a thing? */
    struct spa_pod_builder podBuilder;
    const struct spa_pod* pConnectionParameters[1];
    enum ma_pw_stream_flags streamFlags;
    int connectResult;
    //ma_uint32 bufferSizeInFrames;

    /* This function can only be called for playback or capture sides. */
    if (deviceType != ma_device_type_playback && deviceType != ma_device_type_capture) {
        return MA_INVALID_ARGS;
    }

    pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor = pDescriptor; /* <-- This is only for the param_changed callback. We'll clear this to NULL when we're done initializing. */


    pProperties = pContextStatePipeWire->pw_properties_new(
        MA_PW_KEY_MEDIA_TYPE,     "Audio",
        MA_PW_KEY_MEDIA_CATEGORY, "Playback",
        MA_PW_KEY_MEDIA_ROLE,     (pDeviceConfigPipeWire->pMediaRole != NULL) ? pDeviceConfigPipeWire->pMediaRole : "Game",
        NULL);

    pContextStatePipeWire->pw_thread_loop_lock(pDeviceStatePipeWire->pThreadLoop);
    {
        pStream = pContextStatePipeWire->pw_stream_new(pDeviceStatePipeWire->pCore, (pDeviceConfigPipeWire->pStreamName != NULL) ? pDeviceConfigPipeWire->pStreamName : "miniaudio", pProperties);
        if (pStream == NULL) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to create PipeWire stream.");
            pContextStatePipeWire->pw_thread_loop_unlock(pDeviceStatePipeWire->pThreadLoop);
            return MA_ERROR;
        }

        /* This installs callbacks for process and param_changed. "process" is for queuing audio data, and "param_changed" is for getting the internal format/channels/rate. */
        pContextStatePipeWire->pw_stream_add_listener(pStream, &pDeviceStatePipeWire->eventListener, &ma_gStreamEventsPipeWire, pDevice);

        /* Set the stream in the device data so that we can use it in the param_changed callback. This will be cleared later. */
        pDeviceStatePipeWire->paramChangedCallbackData.pStream = pStream;



        podBuilder = SPA_POD_BUILDER_INIT(podBuilderBuffer, sizeof(podBuilderBuffer));

        memset(&audioInfo, 0, sizeof(audioInfo));
        audioInfo.format   = ma_format_to_pipewire(pDescriptor->format);
        audioInfo.channels = pDescriptor->channels;
        audioInfo.rate     = pDescriptor->sampleRate;
        /* We're going to leave the channel map alone and just do a conversion ourselves if it differs from the native map. */
        /* TODO: It looks like the params_changed callback does not have a filled out channel map? We might need to do a miniaudio-to-PipeWire channel map conversion and do it that way. Was hoping to just use the native channel map. */

        pConnectionParameters[0] = spa_format_audio_raw_build(&podBuilder, SPA_PARAM_EnumFormat, &audioInfo);

        /*
        pConnectionParameters[1] = spa_pod_builder_add_object(&podBuilder,
                    SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                    SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(pDescriptor->periodCount, 2, 8),
                    SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
                    SPA_PARAM_BUFFERS_size,    SPA_POD_Int(bufferSizeInFrames * ma_get_bytes_per_frame(pDescriptor->format, pDescriptor->channels)));
        */

        /*
        I'm just using MAP_BUFFERS because it's what the PipeWire examples do. I don't know what this does. Also, what's the
        point in the AUTOCONNECT flag? Is that not what we're already doing by calling a function called "connect"?!

        We also can't use INACTIVE because without it, the param_changed callback will not get called, but we depend on that
        so we can get access to the internal format/channels/rate.
        */
        streamFlags = (ma_pw_stream_flags)(MA_PW_STREAM_FLAG_AUTOCONNECT | /*MA_PW_STREAM_FLAG_INACTIVE |*/ MA_PW_STREAM_FLAG_MAP_BUFFERS);

        connectResult = pContextStatePipeWire->pw_stream_connect(pStream, (deviceType == ma_device_type_playback) ? SPA_DIRECTION_OUTPUT : SPA_DIRECTION_INPUT, MA_PW_ID_ANY, streamFlags, pConnectionParameters, ma_countof(pConnectionParameters));
        if (connectResult < 0) {
            ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to connect PipeWire stream.");
            pContextStatePipeWire->pw_stream_destroy(pStream);
            pContextStatePipeWire->pw_thread_loop_unlock(pDeviceStatePipeWire->pThreadLoop);
            return MA_ERROR;
        }
    
        if (deviceType == ma_device_type_playback) {
            pDeviceStatePipeWire->pStreamPlayback = pStream;
        } else {
            pDeviceStatePipeWire->pStreamCapture = pStream;
        }
    }
    pContextStatePipeWire->pw_thread_loop_unlock(pDeviceStatePipeWire->pThreadLoop);

    /* TODO: Wait on a mutex or spinlock. */
    while (pDeviceStatePipeWire->isInternalFormatFinalised == MA_FALSE) {
        //ma_sleep(10);
    }

    /* Devices are in a stopped state by default in miniaudio. */
    pContextStatePipeWire->pw_thread_loop_lock(pDeviceStatePipeWire->pThreadLoop);
    {
        pContextStatePipeWire->pw_stream_set_active(pStream, MA_FALSE);
    }
    pContextStatePipeWire->pw_thread_loop_unlock(pDeviceStatePipeWire->pThreadLoop);


    printf("STREAM INIT DONE\n");
    pDeviceStatePipeWire->paramChangedCallbackData.pDescriptor = NULL;
    pDeviceStatePipeWire->paramChangedCallbackData.pStream     = NULL;
    pDeviceStatePipeWire->isInitialized = MA_TRUE;
    return MA_SUCCESS;
}

static ma_result ma_device_init__pipewire(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    ma_result result;
    struct ma_pw_thread_loop* pThreadLoop;
    struct ma_pw_context* pPipeWireContext;
    struct ma_pw_core* pCore;
    ma_context_state_pipewire* pContextStatePipeWire;
    ma_device_state_pipewire* pDeviceStatePipeWire;
    const ma_device_config_pipewire* pDeviceConfigPipeWire;
    ma_device_config_pipewire defaultDeviceConfigPipeWire;
    ma_device_type deviceType;

    pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));
    MA_PIPEWIRE_ASSERT(pContextStatePipeWire != NULL);

    /* Grab the config. This can be null in which case we'll use a default. */
    pDeviceConfigPipeWire = (const ma_device_config_pipewire*)pDeviceBackendConfig;
    if (pDeviceConfigPipeWire == NULL) {
        defaultDeviceConfigPipeWire = ma_device_config_pipewire_init();
        pDeviceConfigPipeWire = &defaultDeviceConfigPipeWire;
    }

    deviceType = ma_device_get_type(pDevice);
    
    /* Not sure how to do loopback with PipeWire, but it feels like something PipeWire would support. Look into this. */
    if (deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    pThreadLoop = pContextStatePipeWire->pw_thread_loop_new((pDeviceConfigPipeWire->pThreadName != NULL) ? pDeviceConfigPipeWire->pThreadName : "miniaudio (PipeWire)", NULL);
    if (pThreadLoop == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to create PipeWire thread loop.");
        return MA_ERROR;
    }

    pPipeWireContext = pContextStatePipeWire->pw_context_new(pContextStatePipeWire->pw_thread_loop_get_loop(pThreadLoop), NULL, 0);
    if (pPipeWireContext == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to create PipeWire context.");
        pContextStatePipeWire->pw_thread_loop_destroy(pThreadLoop);
        return MA_ERROR;
    }

    pCore = pContextStatePipeWire->pw_context_connect(pPipeWireContext, NULL, 0);
    if (pCore == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to connect PipeWire context.");
        pContextStatePipeWire->pw_context_destroy(pPipeWireContext);
        pContextStatePipeWire->pw_thread_loop_destroy(pThreadLoop);
        return MA_ERROR;
    }

    /* We can now allocate our per-device PipeWire-specific data. */
    pDeviceStatePipeWire = (ma_device_state_pipewire*)ma_calloc(sizeof(*pDeviceStatePipeWire), ma_device_get_allocation_callbacks(pDevice));
    if (pDeviceStatePipeWire == NULL) {
        pContextStatePipeWire->pw_thread_loop_destroy(pThreadLoop);
        return MA_OUT_OF_MEMORY;
    }

    pDeviceStatePipeWire->pThreadLoop = pThreadLoop;
    pDeviceStatePipeWire->pContext    = pPipeWireContext;
    pDeviceStatePipeWire->pCore       = pCore;

    /* If I start the loop right after creating it, I get errors from PipeWire. */
    pContextStatePipeWire->pw_thread_loop_start(pThreadLoop);
    
    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__pipewire(pDevice, pContextStatePipeWire, pDeviceStatePipeWire, pDeviceConfigPipeWire, ma_device_type_capture, pDescriptorCapture);
    }
    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__pipewire(pDevice, pContextStatePipeWire, pDeviceStatePipeWire, pDeviceConfigPipeWire, ma_device_type_playback, pDescriptorPlayback);
    }

    if (result != MA_SUCCESS) {
        ma_free(pDeviceStatePipeWire, ma_device_get_allocation_callbacks(pDevice));
        return result;
    }

    pDeviceStatePipeWire->format     = pDescriptorPlayback->format;
    pDeviceStatePipeWire->channels   = pDescriptorPlayback->channels;
    pDeviceStatePipeWire->sampleRate = pDescriptorPlayback->sampleRate;

    *ppDeviceState = pDeviceStatePipeWire;

    return MA_SUCCESS;
}



static void ma_device_uninit__pipewire(ma_device* pDevice)
{
    ma_device_state_pipewire* pDeviceStatePipeWire = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));

    pContextStatePipeWire->pw_thread_loop_lock(pDeviceStatePipeWire->pThreadLoop);
    {
        if (pDeviceStatePipeWire->pStreamCapture != NULL) {
            pContextStatePipeWire->pw_stream_destroy(pDeviceStatePipeWire->pStreamCapture);
            pDeviceStatePipeWire->pStreamCapture = NULL;
        }

        if (pDeviceStatePipeWire->pStreamPlayback != NULL) {
            pContextStatePipeWire->pw_stream_destroy(pDeviceStatePipeWire->pStreamPlayback);
            pDeviceStatePipeWire->pStreamPlayback = NULL;
        }
    }
    pContextStatePipeWire->pw_thread_loop_unlock(pDeviceStatePipeWire->pThreadLoop);

    ma_free(pDeviceStatePipeWire, ma_device_get_allocation_callbacks(pDevice));
}



static ma_result ma_device_start__pipewire(ma_device* pDevice)
{
    ma_device_state_pipewire* pDeviceStatePipeWire = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));

    pContextStatePipeWire->pw_thread_loop_lock(pDeviceStatePipeWire->pThreadLoop);
    {
        if (pDeviceStatePipeWire->pStreamCapture != NULL) {
            pContextStatePipeWire->pw_stream_set_active(pDeviceStatePipeWire->pStreamCapture, MA_TRUE);
        }

        if (pDeviceStatePipeWire->pStreamPlayback != NULL) {
            pContextStatePipeWire->pw_stream_set_active(pDeviceStatePipeWire->pStreamPlayback, MA_TRUE);
        }
    }
    pContextStatePipeWire->pw_thread_loop_unlock(pDeviceStatePipeWire->pThreadLoop);

    return MA_SUCCESS;
}


static ma_result ma_device_stop__pipewire(ma_device* pDevice)
{
    ma_device_state_pipewire* pDeviceStatePipeWire = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));

    pContextStatePipeWire->pw_thread_loop_lock(pDeviceStatePipeWire->pThreadLoop);
    {
        if (pDeviceStatePipeWire->pStreamCapture != NULL) {
            pContextStatePipeWire->pw_stream_set_active(pDeviceStatePipeWire->pStreamCapture, MA_FALSE);
        }

        if (pDeviceStatePipeWire->pStreamPlayback != NULL) {
            pContextStatePipeWire->pw_stream_set_active(pDeviceStatePipeWire->pStreamPlayback, MA_FALSE);
        }
    }
    pContextStatePipeWire->pw_thread_loop_unlock(pDeviceStatePipeWire->pThreadLoop);

    return MA_SUCCESS;
}

static ma_device_backend_vtable ma_gDeviceBackendVTable_PipeWire =
{
    ma_backend_info__pipewire,
    ma_context_init__pipewire,
    ma_context_uninit__pipewire,
    ma_context_enumerate_devices__pipewire,
    ma_context_get_device_info__pipewire,
    ma_device_init__pipewire,
    ma_device_uninit__pipewire,
    ma_device_start__pipewire,
    ma_device_stop__pipewire,
    NULL,   /* onDeviceRead */
    NULL,   /* onDeviceWrite */
    NULL,   /* onDeviceLoop */
    NULL    /* onDeviceWakeup */
};

ma_device_backend_vtable* ma_device_backend_pipewire = &ma_gDeviceBackendVTable_PipeWire;

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic pop
#endif
#else
ma_device_backend_vtable* ma_device_backend_pipewire = NULL;
#endif  /* MA_HAS_PIPEWIRE */

MA_API ma_context_config_pipewire ma_context_config_pipewire_init(void)
{
    ma_context_config_pipewire config;

    memset(&config, 0, sizeof(config));

    return config;
}

MA_API ma_device_config_pipewire ma_device_config_pipewire_init(void)
{
    ma_device_config_pipewire config;

    memset(&config, 0, sizeof(config));

    return config;
}
#endif /* miniaudio_backend_pipewire_c */