/*
This example demonstrates some of the advanced features of the high level engine API.

The following features are demonstrated:

  * Initialization of the engine from a pre-initialized device.
  * Self-managed resource managers.
  * Multiple engines with a shared resource manager.
  * Creation and management of `ma_sound` objects.

This example will play the sound that's passed in on the command line.

Using a shared resource manager, as we do in this example, is useful for when you want to user
multiple engines so that you can output to multiple playback devices simultaneoulys. An example
might be a local co-op multiplayer game where each player has their own headphones.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#define MAX_DEVICES 2
#define MAX_SOUNDS  32

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    (void)pInput;

    /*
    Since we're managing the underlying device ourselves, we need to read from the engine directly.
    To do this we need access to the `ma_engine` object which we passed in to the user data. One
    advantage of this is that you could do your own audio processing in addition to the engine's
    standard processing.
    */
    ma_engine_read_pcm_frames((ma_engine*)pDevice->pUserData, pOutput, frameCount, NULL);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_context context;
    ma_resource_manager_config resourceManagerConfig;
    ma_resource_manager resourceManager;
    ma_engine engines[MAX_DEVICES];
    ma_device devices[MAX_DEVICES];
    ma_uint32 engineCount = 0;
    ma_uint32 iEngine;
    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_uint32 iAvailableDevice;
    ma_uint32 iChosenDevice;
    ma_sound sounds[MAX_SOUNDS];
    ma_uint32 soundCount;
    ma_uint32 iSound;

    if (argc < 2) {
        printf("No input file.");
        return -1;
    }


    /*
    We are going to be initializing multiple engines. In order to save on memory usage we can use a self managed
    resource manager so we can share a single resource manager across multiple engines.
    */
    resourceManagerConfig = ma_resource_manager_config_init();
    resourceManagerConfig.decodedFormat     = ma_format_f32;    /* ma_format_f32 should almost always be used as that's what the engine (and most everything else) uses for mixing. */
    resourceManagerConfig.decodedChannels   = 0;                /* Setting the channel count to 0 will cause sounds to use their native channel count. */
    resourceManagerConfig.decodedSampleRate = 48000;            /* Using a consistent sample rate is useful for avoiding expensive resampling in the audio thread. This will result in resampling being performed by the loading thread(s). */

    result = ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize resource manager.");
        return -1;
    }


    /* We're going to want a context so we can enumerate our playback devices. */
    result = ma_context_init(NULL, 0, NULL, &context);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize context.");
        return -1;
    }

    /*
    Now that we have a context we will want to enumerate over each device so we can display them to the user and give
    them a chance to select the output devices they want to use.
    */
    result = ma_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, NULL, NULL);
    if (result != MA_SUCCESS) {
        printf("Failed to enumerate playback devices.");
        ma_context_uninit(&context);
        return -1;
    }


    /* We have our devices, so now we want to get the user to select the devices they want to output to. */
    engineCount = 0;

    for (iChosenDevice = 0; iChosenDevice < MAX_DEVICES; iChosenDevice += 1) {
        int c = 0;
        for (;;) {
            printf("Select playback device %d ([%d - %d], Q to quit):\n", iChosenDevice+1, 0, ma_min((int)playbackDeviceCount, 9));

            for (iAvailableDevice = 0; iAvailableDevice < playbackDeviceCount; iAvailableDevice += 1) {
                printf("    %d: %s\n", iAvailableDevice, pPlaybackDeviceInfos[iAvailableDevice].name);
            }

            for (;;) {
                c = getchar();
                if (c != '\n') {
                    break;
                }
            }
        
            if (c == 'q' || c == 'Q') {
                return 0;   /* User aborted. */
            }

            if (c >= '0' && c <= '9') {
                c -= '0';

                if (c < (int)playbackDeviceCount) {
                    ma_device_config deviceConfig;
                    ma_engine_config engineConfig;

                    /*
                    Create the device first before the engine. We'll specify the device in the engine's config. This is optional. When a device is
                    not pre-initialized the engine will create one for you internally. The device does not need to be started here - the engine will
                    do that for us in `ma_engine_start()`. The device's format is derived from the resource manager, but can be whatever you want.
                    It's useful to keep the format consistent with the resource manager to avoid data conversions costs in the audio callback. In
                    this example we're using the resource manager's sample format and sample rate, but leaving the channel count set to the device's
                    native channels. You can use whatever format/channels/rate you like.
                    */
                    deviceConfig = ma_device_config_init(ma_device_type_playback);
                    deviceConfig.playback.pDeviceID = &pPlaybackDeviceInfos[c].id;
                    deviceConfig.playback.format    = resourceManager.config.decodedFormat;
                    deviceConfig.playback.channels  = 0;
                    deviceConfig.sampleRate         = resourceManager.config.decodedSampleRate;
                    deviceConfig.dataCallback       = data_callback;
                    deviceConfig.pUserData          = &engines[engineCount];

                    result = ma_device_init(&context, &deviceConfig, &devices[engineCount]);
                    if (result != MA_SUCCESS) {
                        printf("Failed to initialize device for %s.\n", pPlaybackDeviceInfos[c].name);
                        return -1;
                    }

                    /* Now that we have the device we can initialize the engine. The device is passed into the engine's config. */
                    engineConfig = ma_engine_config_init();
                    engineConfig.pDevice          = &devices[engineCount];
                    engineConfig.pResourceManager = &resourceManager;
                    engineConfig.noAutoStart      = MA_TRUE;    /* Don't start the engine by default - we'll do that manually below. */

                    result = ma_engine_init(&engineConfig, &engines[engineCount]);
                    if (result != MA_SUCCESS) {
                        printf("Failed to initialize engine for %s.\n", pPlaybackDeviceInfos[c].name);
                        ma_device_uninit(&devices[engineCount]);
                        return -1;
                    }
                    
                    engineCount += 1;
                    break;
                } else {
                    printf("Invalid device number.\n");
                }
            } else {
                printf("Invalid device number.\n");
            }
        }

        printf("Device %d: %s\n", iChosenDevice+1, pPlaybackDeviceInfos[c].name);
    }


    /* We should now have our engine's initialized. We can now start them. */
    for (iEngine = 0; iEngine < engineCount; iEngine += 1) {
        result = ma_engine_start(&engines[iEngine]);
        if (result != MA_SUCCESS) {
            printf("WARNING: Failed to start engine %d.\n", iEngine);
        }
    }


    /*
    At this point our engine's are running and outputting nothing but silence. To get them playing something we'll need
    some sounds. In this example we're just using one sound per engine, but you can create as many as you like. Since
    we're using a shared resource manager, the sound data will only be loaded once. This is how you would implement
    multiple listeners.
    */
    soundCount = 0;
    for (iEngine = 0; iEngine < engineCount; iEngine += 1) {
        /* Just one sound per engine in this example. We're going to be loading this asynchronously. */
        result = ma_sound_init_from_file(&engines[iEngine], argv[1], MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM, NULL, NULL, &sounds[iEngine]);
        if (result != MA_SUCCESS) {
            printf("WARNING: Failed to load sound \"%s\"", argv[1]);
            break;
        }

        /*
        The sound can be started as soon as ma_sound_init_from_file() returns, even for sounds that are initialized
        with MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC. The sound will start playing while it's being loaded. Note that if the
        asynchronous loading process cannot keep up with the rate at which you try reading you'll end up glitching.
        If this is an issue, you need to not load sounds asynchronously.
        */
        result = ma_sound_start(&sounds[iEngine]);
        if (result != MA_SUCCESS) {
            printf("WARNING: Failed to start sound.");
        }

        soundCount += 1;
    }


    printf("Press Enter to quit...");
    getchar();
    
    for (;;) {
        int c = getchar();
        if (c == '\n') {
            break;
        }
    }

    /* Teardown. */

    /* The application owns the `ma_sound` object which means you're responsible for uninitializing them. */
    for (iSound = 0; iSound < soundCount; iSound += 1) {
        ma_sound_uninit(&sounds[iSound]);
    }

    /* We can now uninitialize each engine. */
    for (iEngine = 0; iEngine < engineCount; iEngine += 1) {
        ma_engine_uninit(&engines[iEngine]);

        /*
        The engine has been uninitialized so now lets uninitialize the device. Do this first to ensure we don't
        uninitialize the resource manager from under the device while the data callback is running.
        */
        ma_device_uninit(&devices[iEngine]);
    }

    /*
    Do the resource manager last. This way we can guarantee the data callbacks of each device aren't trying to access
    and data managed by the resource manager.
    */
    ma_resource_manager_uninit(&resourceManager);

    return 0;
}
