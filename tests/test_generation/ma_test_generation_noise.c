
static drwav_data_format drwav_data_format_from_noise_config(const ma_noise_config* pNoiseConfig)
{
    MA_ASSERT(pNoiseConfig != NULL);

    return drwav_data_format_from_minaudio_format(pNoiseConfig->format, pNoiseConfig->channels, 48000);
}

ma_result test_noise__by_format_and_type(ma_format format, ma_waveform_type type, const char* pFileName)
{
    ma_result result;
    ma_noise_config noiseConfig;
    ma_noise noise;
    drwav_data_format wavFormat;
    drwav wav;
    ma_uint32 iFrame;

    printf("    %s\n", pFileName);

    noiseConfig = ma_noise_config_init(format, 2, type, 0, 0.1);
    result = ma_noise_init(&noiseConfig, &noise);
    if (result != MA_SUCCESS) {
        return result;
    }

    wavFormat = drwav_data_format_from_noise_config(&noiseConfig);
    if (!drwav_init_file_write(&wav, pFileName, &wavFormat, NULL)) {
        return MA_ERROR;    /* Could not open file for writing. */
    }

    /* We'll do a few seconds of data. */
    for (iFrame = 0; iFrame < wavFormat.sampleRate * 10; iFrame += 1) {
        ma_uint8 temp[1024];
        ma_noise_read_pcm_frames(&noise, temp, 1);
        drwav_write_pcm_frames(&wav, 1, temp);
    }

    drwav_uninit(&wav);
    return MA_SUCCESS;
}

ma_result test_noise__f32()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_noise__by_format_and_type(ma_format_f32, ma_noise_type_white, "output/noise_f32_white.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_noise__s16()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_noise__by_format_and_type(ma_format_s16, ma_noise_type_white, "output/noise_s16_white.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_noise__u8()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_noise__by_format_and_type(ma_format_u8, ma_noise_type_white, "output/noise_u8_white.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

int test_entry__noise(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    (void)argc;
    (void)argv;

    result = test_noise__f32();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_noise__s16();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_noise__u8();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}
