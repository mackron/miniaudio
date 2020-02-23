
ma_result test_lpf1__f32(const char* pInputFilePath)
{
    const char* pOutputFilePath = "output/lpf1_f32.wav";
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    drwav_data_format wavFormat;
    drwav wav;
    ma_lpf1_config lpfConfig;
    ma_lpf1 lpf;
    
    decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    result = ma_decoder_init_file(pInputFilePath, &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    lpfConfig = ma_lpf_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000);
    result = ma_lpf1_init(&lpfConfig, &lpf);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return result;
    }

    wavFormat = drwav_data_format_from_minaudio_format(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate);
    if (!drwav_init_file_write(&wav, pOutputFilePath, &wavFormat, NULL)) {
        ma_decoder_uninit(&decoder);
        return MA_ERROR;
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
        ma_lpf1_process_pcm_frames(&lpf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        drwav_write_pcm_frames(&wav, framesJustRead, tempOut);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    drwav_uninit(&wav);
    return MA_SUCCESS;
}

ma_result test_lpf1__s16(const char* pInputFilePath)
{
    const char* pOutputFilePath = "output/lpf1_s16.wav";
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    drwav_data_format wavFormat;
    drwav wav;
    ma_lpf1_config lpfConfig;
    ma_lpf1 lpf;
    
    decoderConfig = ma_decoder_config_init(ma_format_s16, 0, 0);
    result = ma_decoder_init_file(pInputFilePath, &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    lpfConfig = ma_lpf_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000);
    result = ma_lpf1_init(&lpfConfig, &lpf);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return result;
    }

    wavFormat = drwav_data_format_from_minaudio_format(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate);
    if (!drwav_init_file_write(&wav, pOutputFilePath, &wavFormat, NULL)) {
        ma_decoder_uninit(&decoder);
        return MA_ERROR;
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
        ma_lpf1_process_pcm_frames(&lpf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        drwav_write_pcm_frames(&wav, framesJustRead, tempOut);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    drwav_uninit(&wav);
    return MA_SUCCESS;
}

ma_result test_lpf2__f32(const char* pInputFilePath)
{
    const char* pOutputFilePath = "output/lpf2_f32.wav";
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    drwav_data_format wavFormat;
    drwav wav;
    ma_lpf_config lpfConfig;
    ma_lpf lpf;
    
    decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    result = ma_decoder_init_file(pInputFilePath, &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    lpfConfig = ma_lpf_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000);
    result = ma_lpf_init(&lpfConfig, &lpf);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return result;
    }

    wavFormat = drwav_data_format_from_minaudio_format(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate);
    if (!drwav_init_file_write(&wav, pOutputFilePath, &wavFormat, NULL)) {
        ma_decoder_uninit(&decoder);
        return MA_ERROR;
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
        ma_lpf_process_pcm_frames(&lpf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        drwav_write_pcm_frames(&wav, framesJustRead, tempOut);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    drwav_uninit(&wav);
    return MA_SUCCESS;
}

int test_entry__lpf(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    const char* pInputFilePath;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    pInputFilePath = argv[1];
    
    result = test_lpf1__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_lpf1__s16(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_lpf2__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}
