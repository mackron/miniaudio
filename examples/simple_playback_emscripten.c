#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

void main_loop__em()
{
}
#endif

#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     1
#define DEVICE_SAMPLE_RATE  48000

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_sine_wave* pSineWave;

    ma_assert(pDevice->playback.channels == DEVICE_CHANNELS);

    pSineWave = (ma_sine_wave*)pDevice->pUserData;
    ma_assert(pSineWave != NULL);

    ma_sine_wave_read_f32(pSineWave, frameCount, (float*)pOutput);

    (void)pInput;   /* Unused. */
}

int main(int argc, char** argv)
{
    ma_sine_wave sineWave;
    ma_device_config deviceConfig;
    ma_device device;

    ma_sine_wave_init(0.2, 400, DEVICE_SAMPLE_RATE, &sineWave);
    
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &sineWave;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    printf("Device Name: %s\n", device.playback.name);

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return -5;
    }
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop__em, 0, 1);
#else
    printf("Press Enter to quit...\n");
    getchar();
#endif
    
    ma_device_uninit(&device);
    
    (void)argc;
    (void)argv;
    return 0;
}
