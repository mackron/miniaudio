ma_result hishelf_init_decoder_and_encoder(const char* pInputFilePath, const char* pOutputFilePath, ma_format format, ma_decoder* pDecoder, ma_encoder* pEncoder)
{
    return filtering_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, 0, 0, pDecoder, pEncoder);
}


ma_result test_hishelf2__by_format(const char* pInputFilePath, const char* pOutputFilePath, ma_format format)
{
    ma_result result;
    ma_decoder decoder;
    ma_encoder encoder;
    ma_hishelf2_config hishelfConfig;
    ma_hishelf2 hishelf;

    printf("    %s\n", pOutputFilePath);

    result = hishelf_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, &decoder, &encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    hishelfConfig = ma_hishelf2_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 18, 1, 16000);
    result = ma_hishelf2_init(&hishelfConfig, &hishelf);
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
        framesJustRead = ma_decoder_read_pcm_frames(&decoder, tempIn, framesToRead);

        /* Filter */
        ma_hishelf2_process_pcm_frames(&hishelf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        ma_encoder_write_pcm_frames(&encoder, tempOut, framesJustRead);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    ma_decoder_uninit(&decoder);
    ma_encoder_uninit(&encoder);
    return MA_SUCCESS;
}

ma_result test_hishelf2__f32(const char* pInputFilePath)
{
    return test_hishelf2__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hishelf2_f32.wav", ma_format_f32);
}

ma_result test_hishelf2__s16(const char* pInputFilePath)
{
    return test_hishelf2__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hishelf2_s16.wav", ma_format_s16);
}

#if 0
ma_result test_hishelf4__by_format(const char* pInputFilePath, const char* pOutputFilePath, ma_format format)
{
    ma_result result;
    ma_decoder decoder;
    ma_encoder encoder;
    ma_hishelf_config hishelfConfig;
    ma_hishelf hishelf;

    printf("    %s\n", pOutputFilePath);

    result = hishelf_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, &decoder, &encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    hishelfConfig = ma_hishelf_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000, 4);
    result = ma_hishelf_init(&hishelfConfig, &hishelf);
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
        framesJustRead = ma_decoder_read_pcm_frames(&decoder, tempIn, framesToRead);

        /* Filter */
        ma_hishelf_process_pcm_frames(&hishelf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        ma_encoder_write_pcm_frames(&encoder, tempOut, framesJustRead);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    ma_decoder_uninit(&decoder);
    ma_encoder_uninit(&encoder);
    return MA_SUCCESS;
}

ma_result test_hishelf4__f32(const char* pInputFilePath)
{
    return test_hishelf4__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hishelf4_f32.wav", ma_format_f32);
}

ma_result test_hishelf4__s16(const char* pInputFilePath)
{
    return test_hishelf4__by_format(pInputFilePath, TEST_OUTPUT_DIR"/hishelf4_s16.wav", ma_format_s16);
}
#endif

int test_entry__hishelf(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    const char* pInputFilePath;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    pInputFilePath = argv[1];
    

    result = test_hishelf2__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_hishelf2__s16(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

#if 0
    result = test_hishelf4__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_hishelf4__s16(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }
#endif

    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}
