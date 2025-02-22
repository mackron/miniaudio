
ma_result hpf_init_decoder_and_encoder(const char* pInputFilePath, const char* pOutputFilePath, ma_format format, ma_decoder* pDecoder, ma_encoder* pEncoder)
{
    return filtering_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, 0, 0, pDecoder, pEncoder);
}

ma_result test_hpf1__by_format(const char* pInputFilePath, const char* pOutputFilePath, ma_format format)
{
    ma_result result;
    ma_decoder decoder;
    ma_encoder encoder;
    ma_hpf1_config hpfConfig;
    ma_hpf1 hpf;

    printf("    %s\n", pOutputFilePath);

    result = hpf_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, &decoder, &encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    hpfConfig = ma_hpf1_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000);
    result = ma_hpf1_init(&hpfConfig, NULL, &hpf);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        ma_encoder_uninit(&encoder);
        return result;
    }

    for (;;) {
        ma_uint8 tempIn[4096];
        ma_uint8 tempOut[4096];
        ma_uint64 tempCapIn  = sizeof(tempIn)  / ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        ma_uint64 tempCapOut = sizeof(tempOut) / ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        ma_uint64 framesToRead;
        ma_uint64 framesJustRead;

        framesToRead = ma_min(tempCapIn, tempCapOut);
        ma_decoder_read_pcm_frames(&decoder, tempIn, framesToRead, &framesJustRead);

        /* Filter */
        ma_hpf1_process_pcm_frames(&hpf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        ma_encoder_write_pcm_frames(&encoder, tempOut, framesJustRead, NULL);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    ma_decoder_uninit(&decoder);
    ma_encoder_uninit(&encoder);
    return MA_SUCCESS;
}

ma_result test_hpf1__f32(const char* pInputFilePath)
{
    return test_hpf1__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hpf1_f32.wav", ma_format_f32);
}

ma_result test_hpf1__s16(const char* pInputFilePath)
{
    return test_hpf1__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hpf1_s16.wav", ma_format_s16);
}


ma_result test_hpf2__by_format(const char* pInputFilePath, const char* pOutputFilePath, ma_format format)
{
    ma_result result;
    ma_decoder decoder;
    ma_encoder encoder;
    ma_hpf2_config hpfConfig;
    ma_hpf2 hpf;

    printf("    %s\n", pOutputFilePath);

    result = hpf_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, &decoder, &encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    hpfConfig = ma_hpf2_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000, 0);
    result = ma_hpf2_init(&hpfConfig, NULL, &hpf);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        ma_encoder_uninit(&encoder);
        return result;
    }

    for (;;) {
        ma_uint8 tempIn[4096];
        ma_uint8 tempOut[4096];
        ma_uint64 tempCapIn  = sizeof(tempIn)  / ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        ma_uint64 tempCapOut = sizeof(tempOut) / ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        ma_uint64 framesToRead;
        ma_uint64 framesJustRead;

        framesToRead = ma_min(tempCapIn, tempCapOut);
        ma_decoder_read_pcm_frames(&decoder, tempIn, framesToRead, &framesJustRead);

        /* Filter */
        ma_hpf2_process_pcm_frames(&hpf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        ma_encoder_write_pcm_frames(&encoder, tempOut, framesJustRead, NULL);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    ma_decoder_uninit(&decoder);
    ma_encoder_uninit(&encoder);
    return MA_SUCCESS;
}

ma_result test_hpf2__f32(const char* pInputFilePath)
{
    return test_hpf2__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hpf2_f32.wav", ma_format_f32);
}

ma_result test_hpf2__s16(const char* pInputFilePath)
{
    return test_hpf2__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hpf2_s16.wav", ma_format_s16);
}


ma_result test_hpf3__by_format(const char* pInputFilePath, const char* pOutputFilePath, ma_format format)
{
    ma_result result;
    ma_decoder decoder;
    ma_encoder encoder;
    ma_hpf_config hpfConfig;
    ma_hpf hpf;

    printf("    %s\n", pOutputFilePath);

    result = hpf_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, &decoder, &encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    hpfConfig = ma_hpf_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000, 3);
    result = ma_hpf_init(&hpfConfig, NULL, &hpf);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        ma_encoder_uninit(&encoder);
        return result;
    }

    for (;;) {
        ma_uint8 tempIn[4096];
        ma_uint8 tempOut[4096];
        ma_uint64 tempCapIn  = sizeof(tempIn)  / ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        ma_uint64 tempCapOut = sizeof(tempOut) / ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        ma_uint64 framesToRead;
        ma_uint64 framesJustRead;

        framesToRead = ma_min(tempCapIn, tempCapOut);
        ma_decoder_read_pcm_frames(&decoder, tempIn, framesToRead, &framesJustRead);

        /* Filter */
        ma_hpf_process_pcm_frames(&hpf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        ma_encoder_write_pcm_frames(&encoder, tempOut, framesJustRead, NULL);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    ma_decoder_uninit(&decoder);
    ma_encoder_uninit(&encoder);
    return MA_SUCCESS;
}

ma_result test_hpf3__f32(const char* pInputFilePath)
{
    return test_hpf3__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hpf3_f32.wav", ma_format_f32);
}

ma_result test_hpf3__s16(const char* pInputFilePath)
{
    return test_hpf3__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hpf3_s16.wav", ma_format_s16);
}


int test_entry__hpf(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    const char* pInputFilePath;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    pInputFilePath = argv[1];
    

    result = test_hpf1__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_hpf1__s16(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }


    result = test_hpf2__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_hpf2__s16(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }


    result = test_hpf3__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_hpf3__s16(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }


    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}
