
ma_result test_hpf__f32(const char* pInputFilePath)
{
    const char* pOutputFilePath = "output/hpf_f32.wav";
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    drwav_data_format wavFormat;
    drwav wav;
    ma_hpf_config hpfConfig;
    ma_hpf hpf;
    
    decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    result = ma_decoder_init_file(pInputFilePath, &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    hpfConfig = ma_hpf_config_init(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, 2000);
    result = ma_hpf_init(&hpfConfig, &hpf);
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
        ma_hpf_process_pcm_frames(&hpf, tempOut, tempIn, framesJustRead);

        /* Write to the WAV file. */
        drwav_write_pcm_frames(&wav, framesJustRead, tempOut);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    drwav_uninit(&wav);
    return MA_SUCCESS;
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
    
    result = test_hpf__f32(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}
