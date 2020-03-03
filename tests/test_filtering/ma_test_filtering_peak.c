ma_result peak_init_decoder_and_encoder(const char* pInputFilePath, const char* pOutputFilePath, ma_format format, ma_decoder* pDecoder, ma_encoder* pEncoder)
{
    return filtering_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, 0, 0, pDecoder, pEncoder);
}


ma_result test_peak2__by_format(const char* pInputFilePath, const char* pOutputFilePath, ma_format format)
{
    ma_result result;
    ma_decoder decoder;
    ma_encoder encoder;
    ma_peak2_config peakConfig;
    ma_peak2 peak;

    printf("    %s\n", pOutputFilePath);

    result = peak_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, &decoder, &encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    peakConfig = ma_peak2_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 24, 0, 16000);
    result = ma_peak2_init(&peakConfig, &peak);
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
        ma_peak2_process_pcm_frames(&peak, tempOut, tempIn, framesJustRead);

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

ma_result test_peak2__f32(const char* pInputFilePath)
{
    return test_peak2__by_format(pInputFilePath, TEST_OUTPUT_DIR"/peak2_f32.wav", ma_format_f32);
}

ma_result test_peak2__s16(const char* pInputFilePath)
{
    return test_peak2__by_format(pInputFilePath, TEST_OUTPUT_DIR"/peak2_s16.wav", ma_format_s16);
}

#if 0
ma_result test_peak4__by_format(const char* pInputFilePath, const char* pOutputFilePath, ma_format format)
{
    ma_result result;
    ma_decoder decoder;
    ma_encoder encoder;
    ma_peak_config peakConfig;
    ma_peak peak;

    printf("    %s\n", pOutputFilePath);

    result = peak_init_decoder_and_encoder(pInputFilePath, pOutputFilePath, format, &decoder, &encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    peakConfig = ma_peak_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000, 4);
    result = ma_peak_init(&peakConfig, &peak);
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
        ma_peak_process_pcm_frames(&peak, tempOut, tempIn, framesJustRead);

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

ma_result test_peak4__f32(const char* pInputFilePath)
{
    return test_peak4__by_format(pInputFilePath, TEST_OUTPUT_DIR"/peak4_f32.wav", ma_format_f32);
}

ma_result test_peak4__s16(const char* pInputFilePath)
{
    return test_peak4__by_format(pInputFilePath, TEST_OUTPUT_DIR"/peak4_s16.wav", ma_format_s16);
}
#endif

int test_entry__peak(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    const char* pInputFilePath;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    pInputFilePath = argv[1];
    

    result = test_peak2__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_peak2__s16(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

#if 0
    result = test_peak4__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_peak4__s16(pInputFilePath);
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
