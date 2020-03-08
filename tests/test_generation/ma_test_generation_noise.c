
ma_result test_noise__by_format_and_type(ma_format format, ma_noise_type type, const char* pFileName)
{
    ma_result result;
    ma_noise_config noiseConfig;
    ma_noise noise;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    ma_uint32 iFrame;

    printf("    %s\n", pFileName);

    noiseConfig = ma_noise_config_init(format, 1, type, 0, 0.1);
    result = ma_noise_init(&noiseConfig, &noise);
    if (result != MA_SUCCESS) {
        return result;
    }

    encoderConfig = ma_encoder_config_init(ma_resource_format_wav, format, noiseConfig.channels, 48000);
    result = ma_encoder_init_file(pFileName, &encoderConfig, &encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* We'll do a few seconds of data. */
    for (iFrame = 0; iFrame < encoder.config.sampleRate * 10; iFrame += 1) {
        ma_uint8 temp[1024];
        ma_noise_read_pcm_frames(&noise, temp, 1);
        ma_encoder_write_pcm_frames(&encoder, temp, 1);
    }

    ma_encoder_uninit(&encoder);
    return MA_SUCCESS;
}

ma_result test_noise__f32()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_noise__by_format_and_type(ma_format_f32, ma_noise_type_white, TEST_OUTPUT_DIR"/noise_f32_white.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_noise__by_format_and_type(ma_format_f32, ma_noise_type_pink, TEST_OUTPUT_DIR"/noise_f32_pink.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_noise__by_format_and_type(ma_format_f32, ma_noise_type_brownian, TEST_OUTPUT_DIR"/noise_f32_brownian.wav");
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

    result = test_noise__by_format_and_type(ma_format_s16, ma_noise_type_white, TEST_OUTPUT_DIR"/output/noise_s16_white.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_noise__by_format_and_type(ma_format_s16, ma_noise_type_pink, TEST_OUTPUT_DIR"/output/noise_s16_pink.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_noise__by_format_and_type(ma_format_s16, ma_noise_type_brownian, TEST_OUTPUT_DIR"/output/noise_s16_brownian.wav");
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

    result = test_noise__by_format_and_type(ma_format_u8, ma_noise_type_white, TEST_OUTPUT_DIR"/noise_u8_white.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_noise__by_format_and_type(ma_format_u8, ma_noise_type_pink, TEST_OUTPUT_DIR"/noise_u8_pink.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_noise__by_format_and_type(ma_format_u8, ma_noise_type_brownian, TEST_OUTPUT_DIR"/noise_u8_brownian.wav");
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
