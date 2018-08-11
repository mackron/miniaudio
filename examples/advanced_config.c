#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

#include <stdio.h>

void on_log(mal_context* pContext, mal_device* pDevice, const char* message)
{
    (void)pContext;
    (void)pDevice;
    printf("mini_al: %s\n", message);
}

mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    (void)pDevice;
    (void)frameCount;
    (void)pSamples;
    return 0;   // Just output silence for this example.
}

void on_device_stop(mal_device* pDevice)
{
    (void)pDevice;
    printf("Device stopped\n");
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // When initializing a context, you can pass in an optional configuration object that allows you to control
    // context-level configuration. The mal_context_config_init() function will initialize a config object with
    // common configuration settings, but you can set other members for more detailed control.
    mal_context_config contextConfig = mal_context_config_init(on_log);

    // The priority of the worker thread can be set with the following. The default priority is
    // mal_thread_priority_highest.
    contextConfig.threadPriority = mal_thread_priority_normal;


    // PulseAudio
    // ----------
    
    // PulseAudio allows you to set the name of the application. mini_al exposes this through the following
    // config.
    contextConfig.pulse.pApplicationName = "My Application";

    // PulseAudio also allows you to control the server you want to connect to, in which case you can specify
    // it with the config below.
    contextConfig.pulse.pServerName = "my_server";

    // During initialization, PulseAudio can try to automatically start the PulseAudio daemon. This does not
    // suit mini_al's trial and error backend initialization architecture so it's disabled by default, but you
    // can enable it like so:
    contextConfig.pulse.tryAutoSpawn = MAL_TRUE;


    // ALSA
    // ----

    // Typically, ALSA enumerates many devices, which unfortunately is not very friendly for the end user. To
    // combat this, mini_al will include only unique card/device pairs by default. The problem with this is that
    // you lose a bit of flexibility and control. Setting alsa.useVerboseDeviceEnumeration makes it so the ALSA
    // backend includes all devices (and there's a lot of them!).
    contextConfig.alsa.useVerboseDeviceEnumeration = MAL_TRUE;


    // JACK
    // ----

    // Like PulseAudio, JACK allows you to specify the name of your application, which you can set like so:
    contextConfig.jack.pClientName = "My Application";

    // Also like PulseAudio, you can have JACK try to automatically start using the following:
    contextConfig.jack.tryStartServer = MAL_TRUE;



    // The prioritization of backends can be controlled by the application. You need only specify the backends
    // you care about. If the context cannot be initialized for any of the specified backends mal_context_init()
    // will fail.
    mal_backend backends[] = {
        mal_backend_wasapi, // Higest priority.
        mal_backend_dsound,
        mal_backend_winmm,
        mal_backend_coreaudio,
        mal_backend_sndio,
        mal_backend_audio4,
        mal_backend_oss,
        mal_backend_pulseaudio,
        mal_backend_alsa,
        mal_backend_jack,
        mal_backend_opensl,
        mal_backend_openal,
        mal_backend_sdl,
        mal_backend_null    // Lowest priority.
    };

    mal_context context;
    if (mal_context_init(backends, sizeof(backends)/sizeof(backends[0]), &contextConfig, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.");
        return -2;
    }

    
    // Enumerate devices.
    mal_device_info* pPlaybackDeviceInfos;
    mal_uint32 playbackDeviceCount;
    mal_device_info* pCaptureDeviceInfos;
    mal_uint32 captureDeviceCount;
    mal_result result = mal_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
    if (result != MAL_SUCCESS) {
        printf("Failed to retrieve device information.\n");
        return -3;
    }

    printf("Playback Devices (%d)\n", playbackDeviceCount);
    for (mal_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
    }

    printf("\n");

    printf("Capture Devices (%d)\n", captureDeviceCount);
    for (mal_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
    }


    // Open the device.
    //
    // Unlike context configs, device configs are required. Similar to context configs, an API exists to help you
    // initialize a config object called mal_device_config_init(). There are an additional two helper APIs to make
    // it easy for you to initialize playback or capture configs specifically: mal_device_config_init_playback()
    // and mal_device_config_init_capture().
    mal_device_config deviceConfig = mal_device_config_init(mal_format_s16, 2, 48000, NULL, on_send_frames_to_device);

    // Applications can specify a callback for when a device is stopped.
    deviceConfig.onStopCallback = on_device_stop;

    // Applications can request exclusive control of the device using the config variable below. Note that not all
    // backends support this feature, so this is actually just a hint.
    deviceConfig.shareMode = mal_share_mode_exclusive;

    // mini_al allows applications to control the mapping of channels. The config below swaps the left and right
    // channels. Normally in an interleaved audio stream, the left channel comes first, but we can change that
    // like the following:
    deviceConfig.channelMap[0] = MAL_CHANNEL_FRONT_RIGHT;
    deviceConfig.channelMap[1] = MAL_CHANNEL_FRONT_LEFT;

    // The ALSA backend has two ways of delivering data to and from a device: memory mapping and read/write. By
    // default memory mapping will be used over read/write because it avoids a single point of data movement
    // internally and is thus, theoretically, more efficient. In testing, however, this has been less stable than
    // read/write mode so an option exists to disable it if need be. This is mainly for debugging, but is left
    // here in case it might be useful for others. If you find a bug specific to mmap mode, please report it!
    deviceConfig.alsa.noMMap = MAL_TRUE;

    // This is not used in this example, but mini_al allows you to directly control the device ID that's used
    // for device selection by mal_device_init(). Below is an example for ALSA. In this example it forces
    // mal_device_init() to try opening the "hw:0,0" device. This is useful for debugging in case you have
    // audio glitches or whatnot with specific devices.
#ifdef MAL_SUPPORT_ALSA
    mal_device_id customDeviceID;
    if (context.backend == mal_backend_alsa) {
        strcpy(customDeviceID.alsa, "hw:0,0");

        // The ALSA backend also supports a mini_al-specific format which looks like this: ":0,0". In this case,
        // mini_al will try different plugins depending on the shareMode setting. When using shared mode it will
        // convert ":0,0" to "dmix:0,0"/"dsnoop:0,0". For exclusive mode (or if dmix/dsnoop fails) it will convert
        // it to "hw:0,0". This is how the ALSA backend honors the shareMode hint.
        strcpy(customDeviceID.alsa, ":0,0");
    }
#endif

    mal_device playbackDevice;
    if (mal_device_init(&context, mal_device_type_playback, NULL, &deviceConfig, NULL, &playbackDevice) != MAL_SUCCESS) {
        printf("Failed to initialize playback device.\n");
        mal_context_uninit(&context);
        return -7;
    }

    if (mal_device_start(&playbackDevice) != MAL_SUCCESS) {
        printf("Failed to start playback device.\n");
        mal_device_uninit(&playbackDevice);
        mal_context_uninit(&context);
        return -8;
    }

    printf("Press Enter to quit...");
    getchar();

    mal_device_uninit(&playbackDevice);


    mal_context_uninit(&context);
    return 0;
}
