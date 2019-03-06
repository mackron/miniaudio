#define MA_DEBUG_OUTPUT
#define MA_USE_REFERENCE_CONVERSION_APIS
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

// Two converters are needed here. One for converting f32 samples from the sine wave generator to the input format,
// and another for converting the input format to the output format for device output.
ma_sine_wave sineWave;
ma_format_converter converterIn;
ma_format_converter converterOut;

ma_uint32 on_convert_samples_in(ma_format_converter* pConverter, ma_uint32 frameCount, void* pFrames, void* pUserData)
{
    (void)pUserData;
    ma_assert(pConverter->config.formatIn == ma_format_f32);

    ma_sine_wave* pSineWave = (ma_sine_wave*)pConverter->config.pUserData;
    ma_assert(pSineWave);

    return (ma_uint32)ma_sine_wave_read_f32(pSineWave, frameCount, (float*)pFrames);
}

ma_uint32 on_convert_samples_out(ma_format_converter* pConverter, ma_uint32 frameCount, void* pFrames, void* pUserData)
{
    (void)pUserData;

    ma_format_converter* pConverterIn = (ma_format_converter*)pConverter->config.pUserData;
    ma_assert(pConverterIn != NULL);

    return (ma_uint32)ma_format_converter_read(pConverterIn, frameCount, pFrames, NULL);
}

void on_send_to_device__original(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_assert(pDevice->playback.format == ma_format_f32);
    ma_assert(pDevice->playback.channels == 1);

    ma_sine_wave_read_f32(&sineWave, frameCount, (float*)pOutput);

    (void)pDevice;
    (void)pInput;
}

void on_send_to_device__dithered(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_assert(pDevice->playback.channels == 1);

    ma_format_converter* pConverter = (ma_format_converter*)pDevice->pUserData;
    ma_assert(pConverter != NULL);
    ma_assert(pDevice->playback.format == pConverter->config.formatOut);

    ma_format_converter_read(pConverter, frameCount, pOutput, NULL);

    (void)pInput;
}

int do_dithering_test()
{
    ma_device_config config;
    ma_device device;
    ma_result result;

    config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate = 0;
    config.dataCallback = on_send_to_device__original;

    // We first play the sound the way it's meant to be played.
    result = ma_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {
        return -1;
    }

    ma_sine_wave_init(0.5, 400, device.sampleRate, &sineWave);

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        return -2;
    }

    printf("Press Enter to play enable dithering.\n");
    getchar();
    ma_device_uninit(&device);


    ma_format srcFormat = ma_format_s24;
    ma_format dstFormat = ma_format_u8;
    ma_dither_mode ditherMode = ma_dither_mode_triangle;

    ma_format_converter_config converterInConfig = ma_format_converter_config_init_new();
    converterInConfig.formatIn = ma_format_f32;  // <-- From the sine wave generator.
    converterInConfig.formatOut = srcFormat;
    converterInConfig.channels = config.playback.channels;
    converterInConfig.ditherMode = ma_dither_mode_none;
    converterInConfig.onRead = on_convert_samples_in;
    converterInConfig.pUserData = &sineWave;
    result = ma_format_converter_init(&converterInConfig, &converterIn);
    if (result != MA_SUCCESS) {
        return -3;
    }

    ma_format_converter_config converterOutConfig = ma_format_converter_config_init_new();
    converterOutConfig.formatIn = srcFormat;
    converterOutConfig.formatOut = dstFormat;
    converterOutConfig.channels = config.playback.channels;
    converterOutConfig.ditherMode = ditherMode;
    converterOutConfig.onRead = on_convert_samples_out;
    converterOutConfig.pUserData = &converterIn;
    result = ma_format_converter_init(&converterOutConfig, &converterOut);
    if (result != MA_SUCCESS) {
        return -3;
    }

    config.playback.format = dstFormat;
    config.dataCallback = on_send_to_device__dithered;
    config.pUserData = &converterOut;

    result = ma_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {
        return -1;
    }

    // Now we play the sound after it's run through a dithered format converter.
    ma_sine_wave_init(0.5, 400, device.sampleRate, &sineWave);

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        return -2;
    }

    printf("Press Enter to stop.\n");
    getchar();

    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;


    do_dithering_test();


    return 0;
}