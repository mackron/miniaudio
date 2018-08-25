#define MAL_DEBUG_OUTPUT
#define MAL_USE_REFERENCE_CONVERSION_APIS
#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

// Two converters are needed here. One for converting f32 samples from the sine wave generator to the input format,
// and another for converting the input format to the output format for device output.
mal_sine_wave sineWave;
mal_format_converter converterIn;
mal_format_converter converterOut;

mal_uint32 on_convert_samples_in(mal_format_converter* pConverter, mal_uint32 frameCount, void* pFrames, void* pUserData)
{
    (void)pUserData;
    mal_assert(pConverter->config.formatIn == mal_format_f32);

    mal_sine_wave* pSineWave = (mal_sine_wave*)pConverter->config.pUserData;
    mal_assert(pSineWave);

    return (mal_uint32)mal_sine_wave_read(pSineWave, frameCount, (float*)pFrames);
}

mal_uint32 on_convert_samples_out(mal_format_converter* pConverter, mal_uint32 frameCount, void* pFrames, void* pUserData)
{
    (void)pUserData;

    mal_format_converter* pConverterIn = (mal_format_converter*)pConverter->config.pUserData;
    mal_assert(pConverterIn != NULL);

    return (mal_uint32)mal_format_converter_read(pConverterIn, frameCount, pFrames, NULL);
}

mal_uint32 on_send_to_device__original(mal_device* pDevice, mal_uint32 frameCount, void* pFrames)
{
    (void)pDevice;
    mal_assert(pDevice->format == mal_format_f32);
    mal_assert(pDevice->channels == 1);

    return (mal_uint32)mal_sine_wave_read(&sineWave, frameCount, (float*)pFrames);
}

mal_uint32 on_send_to_device__dithered(mal_device* pDevice, mal_uint32 frameCount, void* pFrames)
{
    mal_assert(pDevice->channels == 1);

    mal_format_converter* pConverter = (mal_format_converter*)pDevice->pUserData;
    mal_assert(pConverter != NULL);
    mal_assert(pDevice->format == pConverter->config.formatOut);

    return (mal_uint32)mal_format_converter_read(pConverter, frameCount, pFrames, NULL);
}

int do_dithering_test()
{
    mal_device_config config = mal_device_config_init_playback(mal_format_f32, 1, 0, on_send_to_device__original);
    mal_device device;
    mal_result result;

    // We first play the sound the way it's meant to be played.
    result = mal_device_init(NULL, mal_device_type_playback, NULL, &config, NULL, &device);
    if (result != MAL_SUCCESS) {
        return -1;
    }

    mal_sine_wave_init(0.5, 400, device.sampleRate, &sineWave);

    result = mal_device_start(&device);
    if (result != MAL_SUCCESS) {
        return -2;
    }

    printf("Press Enter to play enable dithering.\n");
    getchar();
    mal_device_uninit(&device);


    // Now we play the sound after it's run through a dithered format converter.
    mal_sine_wave_init(0.5, 400, device.sampleRate, &sineWave);

    mal_format srcFormat = mal_format_s24;
    mal_format dstFormat = mal_format_u8;
    mal_dither_mode ditherMode = mal_dither_mode_triangle;

    mal_format_converter_config converterInConfig = mal_format_converter_config_init_new();
    converterInConfig.formatIn = mal_format_f32;  // <-- From the sine wave generator.
    converterInConfig.formatOut = srcFormat;
    converterInConfig.channels = config.channels;
    converterInConfig.ditherMode = mal_dither_mode_none;
    converterInConfig.onRead = on_convert_samples_in;
    converterInConfig.pUserData = &sineWave;
    result = mal_format_converter_init(&converterInConfig, &converterIn);
    if (result != MAL_SUCCESS) {
        return -3;
    }

    mal_format_converter_config converterOutConfig = mal_format_converter_config_init_new();
    converterOutConfig.formatIn = srcFormat;
    converterOutConfig.formatOut = dstFormat;
    converterOutConfig.channels = config.channels;
    converterOutConfig.ditherMode = ditherMode;
    converterOutConfig.onRead = on_convert_samples_out;
    converterOutConfig.pUserData = &converterIn;
    result = mal_format_converter_init(&converterOutConfig, &converterOut);
    if (result != MAL_SUCCESS) {
        return -3;
    }

    config = mal_device_config_init_playback(converterOutConfig.formatOut, 1, 0, on_send_to_device__dithered);

    result = mal_device_init(NULL, mal_device_type_playback, NULL, &config, &converterOut, &device);
    if (result != MAL_SUCCESS) {
        return -1;
    }

    result = mal_device_start(&device);
    if (result != MAL_SUCCESS) {
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