/*
Shows how to use the high level engine API with SDL.

By default, miniaudio's engine API will initialize a device internally for audio output. You can
instead use the engine independently of a device. To show this off, this example will use SDL for
audio output instead of miniaudio.

This example will load the sound specified on the command line and rotate it around the listener's
head.
*/
#define MA_NO_DEVICE_IO /* <-- Disables the `ma_device` API. We don't need that in this example since SDL will be doing that part for us. */
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>    /* Change this to your include location. Might be <SDL2/SDL.h>. */

#define CHANNELS    2               /* Must be stereo for this example. */
#define SAMPLE_RATE 48000

static ma_engine g_engine;
static ma_sound g_sound;            /* This example will play only a single sound at once, so we only need one `ma_sound` object. */

void data_callback(void* pUserData, ma_uint8* pBuffer, int bufferSizeInBytes)
{
    /* Reading is just a matter of reading straight from the engine. */
    ma_uint32 bufferSizeInFrames = (ma_uint32)bufferSizeInBytes / ma_get_bytes_per_frame(ma_format_f32, ma_engine_get_channels(&g_engine));
    ma_engine_read_pcm_frames(&g_engine, pBuffer, bufferSizeInFrames, NULL);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_engine_config engineConfig;
    SDL_AudioSpec desiredSpec;
    SDL_AudioSpec obtainedSpec;
    SDL_AudioDeviceID deviceID;

    if (argc < 2) {
        printf("No input file.");
        return -1;
    }

    /*
    We'll initialize the engine first for the purpose of the example, but since the engine and SDL
    are independent of each other you can initialize them in any order. You need only make sure the
    channel count and sample rates are consistent between the two.

    When initializing the engine it's important to make sure we don't initialize a device
    internally because we want SDL to be dealing with that for us instead.
    */
    engineConfig = ma_engine_config_init();
    engineConfig.noDevice   = MA_TRUE;      /* <-- Make sure this is set so that no device is created (we'll deal with that ourselves). */
    engineConfig.channels   = CHANNELS;
    engineConfig.sampleRate = SAMPLE_RATE;

    result = ma_engine_init(&engineConfig, &g_engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio engine.");
        return -1;
    }

    /* Now load our sound. */
    result = ma_sound_init_from_file(&g_engine, argv[1], 0, NULL, NULL, &g_sound);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize sound.");
        return -1;
    }

    /* Loop the sound so we can continuously hear it. */
    ma_sound_set_looping(&g_sound, MA_TRUE);

    /*
    The sound will not be started by default, so start it now. We won't hear anything until the SDL
    audio device has been opened and started.
    */
    ma_sound_start(&g_sound);


    /*
    Now that we have the engine and sound we can initialize SDL. This could have also been done
    first before the engine and sound.
    */
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        printf("Failed to initialize SDL sub-system.");
        return -1;
    }

    MA_ZERO_OBJECT(&desiredSpec);
    desiredSpec.freq     = ma_engine_get_sample_rate(&g_engine);
    desiredSpec.format   = AUDIO_F32;
    desiredSpec.channels = ma_engine_get_channels(&g_engine);
    desiredSpec.samples  = 512;
    desiredSpec.callback = data_callback;
    desiredSpec.userdata = NULL;

    deviceID = SDL_OpenAudioDevice(NULL, 0, &desiredSpec, &obtainedSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (deviceID == 0) {
        printf("Failed to open SDL audio device.");
        return -1;
    }

    /* Start playback. */
    SDL_PauseAudioDevice(deviceID, 0);

#if 1
    {
        /* We'll move the sound around the listener which we'll leave at the origin. */
        float stepAngle = 0.002f;
        float angle = 0;
        float distance = 2;

        for (;;) {
            double x = ma_cosd(angle) - ma_sind(angle);
            double y = ma_sind(angle) + ma_cosd(angle);

            ma_sound_set_position(&g_sound, (float)x * distance, 0, (float)y * distance);

            angle += stepAngle;
            ma_sleep(1);
        }
    }
#else
    printf("Press Enter to quit...");
    getchar();
#endif

    ma_sound_uninit(&g_sound);
    ma_engine_uninit(&g_engine);
    SDL_CloseAudioDevice(deviceID);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    return 0;
}