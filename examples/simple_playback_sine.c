#define MA_NO_DECODING
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
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_waveform* pSineWave;

    MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);

    pSineWave = (ma_waveform*)pDevice->pUserData;
    MA_ASSERT(pSineWave != NULL);

    ma_waveform_read_pcm_frames(pSineWave, pOutput, frameCount);

    (void)pInput;   /* Unused. */
}

int main(int argc, char** argv)
{
    ma_waveform sineWave;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform_config sineWaveConfig;

    sineWaveConfig = ma_waveform_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE, ma_waveform_type_sine, 0.2, 220);
    ma_waveform_init(&sineWaveConfig, &sineWave);

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
