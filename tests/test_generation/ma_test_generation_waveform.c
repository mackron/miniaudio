
ma_result test_waveform__by_format_and_type(ma_format format, ma_waveform_type type, double amplitude, const char* pFileName)
{
    ma_result result;
    ma_waveform_config waveformConfig;
    ma_waveform waveform;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    ma_uint32 iFrame;

    printf("    %s\n", pFileName);

    waveformConfig = ma_waveform_config_init(format, 2, 48000, type, amplitude, 220);
    result = ma_waveform_init(&waveformConfig, &waveform);
    if (result != MA_SUCCESS) {
        return result;
    }

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, waveformConfig.format, waveformConfig.channels, waveformConfig.sampleRate);
    result = ma_encoder_init_file(pFileName, &encoderConfig, &encoder);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize encoder. */
    }

    /* We'll do a few seconds of data. */
    for (iFrame = 0; iFrame < waveformConfig.sampleRate * 10; iFrame += 1) {
        float temp[MA_MAX_CHANNELS];
        ma_waveform_read_pcm_frames(&waveform, temp, 1, NULL);
        ma_encoder_write_pcm_frames(&encoder, temp, 1, NULL);
    }

    ma_encoder_uninit(&encoder);
    ma_waveform_uninit(&waveform);

    return MA_SUCCESS;
}

ma_result test_waveform__f32()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    double amplitude = 0.2;

    /* Sine */
    result = test_waveform__by_format_and_type(ma_format_f32, ma_waveform_type_sine, +amplitude, TEST_OUTPUT_DIR"/waveform_f32_sine.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_f32, ma_waveform_type_sine, -amplitude, TEST_OUTPUT_DIR"/waveform_f32_sine_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Square */
    result = test_waveform__by_format_and_type(ma_format_f32, ma_waveform_type_square, +amplitude, TEST_OUTPUT_DIR"/waveform_f32_square.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_f32, ma_waveform_type_square, -amplitude, TEST_OUTPUT_DIR"/waveform_f32_square_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Triangle */
    result = test_waveform__by_format_and_type(ma_format_f32, ma_waveform_type_triangle, +amplitude, TEST_OUTPUT_DIR"/waveform_f32_triangle.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_f32, ma_waveform_type_triangle, -amplitude, TEST_OUTPUT_DIR"/waveform_f32_triangle_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Sawtooth */
    result = test_waveform__by_format_and_type(ma_format_f32, ma_waveform_type_sawtooth, +amplitude, TEST_OUTPUT_DIR"/waveform_f32_sawtooth.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_f32, ma_waveform_type_sawtooth, -amplitude, TEST_OUTPUT_DIR"/waveform_f32_sawtooth_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }


    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_waveform__s16()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    double amplitude = 0.2;

    /* Sine */
    result = test_waveform__by_format_and_type(ma_format_s16, ma_waveform_type_sine, +amplitude, TEST_OUTPUT_DIR"/waveform_s16_sine.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_s16, ma_waveform_type_sine, -amplitude, TEST_OUTPUT_DIR"/waveform_s16_sine_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Square */
    result = test_waveform__by_format_and_type(ma_format_s16, ma_waveform_type_square, +amplitude, TEST_OUTPUT_DIR"/waveform_s16_square.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_s16, ma_waveform_type_square, -amplitude, TEST_OUTPUT_DIR"/waveform_s16_square_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Triangle */
    result = test_waveform__by_format_and_type(ma_format_s16, ma_waveform_type_triangle, +amplitude, TEST_OUTPUT_DIR"/waveform_s16_triangle.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_s16, ma_waveform_type_triangle, -amplitude, TEST_OUTPUT_DIR"/waveform_s16_triangle_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Sawtooth */
    result = test_waveform__by_format_and_type(ma_format_s16, ma_waveform_type_sawtooth, +amplitude, TEST_OUTPUT_DIR"/waveform_s16_sawtooth.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_s16, ma_waveform_type_sawtooth, -amplitude, TEST_OUTPUT_DIR"/waveform_s16_sawtooth_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }


    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_waveform__u8()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    double amplitude = 0.2;

    /* Sine */
    result = test_waveform__by_format_and_type(ma_format_u8, ma_waveform_type_sine, +amplitude, TEST_OUTPUT_DIR"/waveform_u8_sine.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_u8, ma_waveform_type_sine, -amplitude, TEST_OUTPUT_DIR"/waveform_u8_sine_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Square */
    result = test_waveform__by_format_and_type(ma_format_u8, ma_waveform_type_square, +amplitude, TEST_OUTPUT_DIR"/waveform_u8_square.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_u8, ma_waveform_type_square, -amplitude, TEST_OUTPUT_DIR"/waveform_u8_square_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Triangle */
    result = test_waveform__by_format_and_type(ma_format_u8, ma_waveform_type_triangle, +amplitude, TEST_OUTPUT_DIR"/waveform_u8_triangle.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_u8, ma_waveform_type_triangle, -amplitude, TEST_OUTPUT_DIR"/waveform_u8_triangle_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    /* Sawtooth */
    result = test_waveform__by_format_and_type(ma_format_u8, ma_waveform_type_sawtooth, +amplitude, TEST_OUTPUT_DIR"/waveform_u8_sawtooth.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__by_format_and_type(ma_format_u8, ma_waveform_type_sawtooth, -amplitude, TEST_OUTPUT_DIR"/waveform_u8_sawtooth_neg.wav");
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }


    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

int test_entry__waveform(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    (void)argc;
    (void)argv;

    result = test_waveform__f32();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__s16();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_waveform__u8();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}