#define MAL_IMPLEMENTATION
#include "../mini_al.h"

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

// This is the function that's used for sending more data to the device for playback.
mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    mal_assert(pDevice->channels == DEVICE_CHANNELS);

    mal_sine_wave* pSineWave = (mal_sine_wave*)pDevice->pUserData;
    mal_assert(pSineWave != NULL);

    return mal_sine_wave_read(pSineWave, frameCount, (float*)pSamples);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    mal_sine_wave sineWave;
    mal_sine_wave_init(0.2, 400, DEVICE_SAMPLE_RATE, &sineWave);
    
    mal_device_config config = mal_device_config_init_playback(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE, on_send_frames_to_device);
    mal_device device;
    if (mal_device_init(NULL, mal_device_type_playback, NULL, &config, &sineWave, &device) != MAL_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    printf("Device Name: %s\n", device.name);

    if (mal_device_start(&device) != MAL_SUCCESS) {
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
