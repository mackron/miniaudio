#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

void main_loop__em()
{
}
#endif

#define DEVICE_FORMAT       mal_format_f32
#define DEVICE_CHANNELS     1
#define DEVICE_SAMPLE_RATE  48000

void data_callback(mal_device* pDevice, void* pOutput, const void* pInput, mal_uint32 frameCount)
{
    (void)pInput;   /* Unused. */
    mal_assert(pDevice->playback.channels == DEVICE_CHANNELS);

    mal_sine_wave* pSineWave = (mal_sine_wave*)pDevice->pUserData;
    mal_assert(pSineWave != NULL);

    mal_sine_wave_read_f32(pSineWave, frameCount, (float*)pOutput);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    mal_sine_wave sineWave;
    mal_sine_wave_init(0.2, 400, DEVICE_SAMPLE_RATE, &sineWave);
    
    mal_device_config config = mal_device_config_init(mal_device_type_playback);
    config.playback.format   = DEVICE_FORMAT;
    config.playback.channels = DEVICE_CHANNELS;
    config.sampleRate        = DEVICE_SAMPLE_RATE;
    config.dataCallback      = data_callback;
    config.pUserData         = &sineWave;

    mal_device device;
    if (mal_device_init(NULL, &config, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    printf("Device Name: %s\n", device.playback.name);

    if (mal_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        mal_device_uninit(&device);
        return -5;
    }
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop__em, 0, 1);
#else
    printf("Press Enter to quit...\n");
    getchar();
#endif
    
    mal_device_uninit(&device);
    
    return 0;
}
