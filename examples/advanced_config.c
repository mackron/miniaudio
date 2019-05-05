#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>

void log_callback(ma_context* pContext, ma_device* pDevice, ma_uint32 logLevel, const char* message)
{
    (void)pContext;
    (void)pDevice;
    printf("miniaudio: [%s] %s\n", ma_log_level_to_string(logLevel), message);
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    (void)pDevice;
    (void)pOutput;
    (void)pInput;
    (void)frameCount;
    return;   /* Just output silence for this example. */
}

void stop_callback(ma_device* pDevice)
{
    (void)pDevice;
    printf("Device stopped\n");
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_context_config contextConfig;
    ma_context context;
    ma_device_config deviceConfig;
    ma_device playbackDevice;
    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_device_info* pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;
    ma_uint32 iDevice;

    /*
    The prioritization of backends can be controlled by the application. You need only specify the backends
    you care about. If the context cannot be initialized for any of the specified backends ma_context_init()
    will fail.
    */
    ma_backend backends[] = {
        ma_backend_wasapi, /* Higest priority. */
        ma_backend_dsound,
        ma_backend_winmm,
        ma_backend_coreaudio,
        ma_backend_sndio,
        ma_backend_audio4,
        ma_backend_oss,
        ma_backend_pulseaudio,
        ma_backend_alsa,
        ma_backend_jack,
        ma_backend_aaudio,
        ma_backend_opensl,
        ma_backend_webaudio,
        ma_backend_null    /* Lowest priority. */
    };


    /*
    When initializing a context, you can pass in an optional configuration object that allows you to control
    context-level configuration. The ma_context_config_init() function will initialize a config object with
    common configuration settings, but you can set other members for more detailed control.
    */
    contextConfig = ma_context_config_init();
    contextConfig.logCallback = log_callback;

    /*
    The priority of the worker thread can be set with the following. The default priority is
    ma_thread_priority_highest.
    */
    contextConfig.threadPriority = ma_thread_priority_normal;


    /* PulseAudio */
    
    /* PulseAudio allows you to set the name of the application. miniaudio exposes this through the following config. */
    contextConfig.pulse.pApplicationName = "My Application";

    /*
    PulseAudio also allows you to control the server you want to connect to, in which case you can specify
    it with the config below.
    */
    contextConfig.pulse.pServerName = "my_server";

    /*
    During initialization, PulseAudio can try to automatically start the PulseAudio daemon. This does not
    suit miniaudio's trial and error backend initialization architecture so it's disabled by default, but you
    can enable it like so:
    */
    contextConfig.pulse.tryAutoSpawn = MA_TRUE;


    /* ALSA */

    /*
    Typically, ALSA enumerates many devices, which unfortunately is not very friendly for the end user. To
    combat this, miniaudio will include only unique card/device pairs by default. The problem with this is that
    you lose a bit of flexibility and control. Setting alsa.useVerboseDeviceEnumeration makes it so the ALSA
    backend includes all devices (and there's a lot of them!).
    */
    contextConfig.alsa.useVerboseDeviceEnumeration = MA_TRUE;


    /* JACK */

    /* Like PulseAudio, JACK allows you to specify the name of your application, which you can set like so: */
    contextConfig.jack.pClientName = "My Application";

    /* Also like PulseAudio, you can have JACK try to automatically start using the following: */
    contextConfig.jack.tryStartServer = MA_TRUE;


    
    if (ma_context_init(backends, sizeof(backends)/sizeof(backends[0]), &contextConfig, &context) != MA_SUCCESS) {
        printf("Failed to initialize context.");
        return -2;
    }

    
    /* Enumerate devices. */
    result = ma_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
    if (result != MA_SUCCESS) {
        printf("Failed to retrieve device information.\n");
        return -3;
    }

    printf("Playback Devices (%d)\n", playbackDeviceCount);
    for (iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
    }

    printf("\n");

    printf("Capture Devices (%d)\n", captureDeviceCount);
    for (iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
    }


    /*
    Open the device.
    
    Unlike context configs, device configs are required. Similar to context configs, an API exists to help you
    initialize a config object called ma_device_config_init().
    
    When using full-duplex you may want to use a different sample format, channel count and channel map. To
    support this, the device configuration splits these into "playback" and "capture" as shown below.
    */
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_s16;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = 48000;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = NULL;

    /* Applications can specify a callback for when a device is stopped. */
    deviceConfig.stopCallback = stop_callback;

    /*
    Applications can request exclusive control of the device using the config variable below. Note that not all
    backends support this feature, so this is actually just a hint.
    */
    deviceConfig.playback.shareMode = ma_share_mode_exclusive;

    /*
    miniaudio allows applications to control the mapping of channels. The config below swaps the left and right
    channels. Normally in an interleaved audio stream, the left channel comes first, but we can change that
    like the following:
    */
    deviceConfig.playback.channelMap[0] = MA_CHANNEL_FRONT_RIGHT;
    deviceConfig.playback.channelMap[1] = MA_CHANNEL_FRONT_LEFT;

    /*
    The ALSA backend has two ways of delivering data to and from a device: memory mapping and read/write. By
    default memory mapping will be used over read/write because it avoids a single point of data movement
    internally and is thus, theoretically, more efficient. In testing, however, this has been less stable than
    read/write mode so an option exists to disable it if need be. This is mainly for debugging, but is left
    here in case it might be useful for others. If you find a bug specific to mmap mode, please report it!
    */
    deviceConfig.alsa.noMMap = MA_TRUE;

    /*
    This is not used in this example, but miniaudio allows you to directly control the device ID that's used
    for device selection by ma_device_init(). Below is an example for ALSA. In this example it forces
    ma_device_init() to try opening the "hw:0,0" device. This is useful for debugging in case you have
    audio glitches or whatnot with specific devices.
    */
#ifdef MA_SUPPORT_ALSA
    {
        ma_device_id customDeviceID;
        if (context.backend == ma_backend_alsa) {
            strcpy(customDeviceID.alsa, "hw:0,0");

            /*
            The ALSA backend also supports a miniaudio-specific format which looks like this: ":0,0". In this case,
            miniaudio will try different plugins depending on the shareMode setting. When using shared mode it will
            convert ":0,0" to "dmix:0,0"/"dsnoop:0,0". For exclusive mode (or if dmix/dsnoop fails) it will convert
            it to "hw:0,0". This is how the ALSA backend honors the shareMode hint.
            */
            strcpy(customDeviceID.alsa, ":0,0");
        }
    }
#endif

    if (ma_device_init(&context, &deviceConfig, &playbackDevice) != MA_SUCCESS) {
        printf("Failed to initialize playback device.\n");
        ma_context_uninit(&context);
        return -7;
    }

    if (ma_device_start(&playbackDevice) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&playbackDevice);
        ma_context_uninit(&context);
        return -8;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&playbackDevice);


    ma_context_uninit(&context);

    (void)argc;
    (void)argv;
    return 0;
}
