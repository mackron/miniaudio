
ma_result test_dithering__u8(const char* pInputFilePath)
{
    const char* pOutputFilePath = "output/dithering_u8.wav";
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    drwav_data_format wavFormat;
    drwav wav;
    
    decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    result = ma_decoder_init_file(pInputFilePath, &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    wavFormat = drwav_data_format_from_minaudio_format(ma_format_u8, decoder.outputChannels, decoder.outputSampleRate);
    if (!drwav_init_file_write(&wav, pOutputFilePath, &wavFormat, NULL)) {
        ma_decoder_uninit(&decoder);
        return MA_ERROR;
    }

    for (;;) {
        ma_uint8 tempIn[4096];
        ma_uint8 tempOut[4096];
        ma_uint64 tempCapIn  = sizeof(tempIn)  / ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        ma_uint64 tempCapOut = sizeof(tempOut) / ma_get_bytes_per_frame(ma_format_u8, decoder.outputChannels);
        ma_uint64 framesToRead;
        ma_uint64 framesJustRead;

        framesToRead = ma_min(tempCapIn, tempCapOut);
        framesJustRead = ma_decoder_read_pcm_frames(&decoder, tempIn, framesToRead);

        /* Convert, with dithering. */
        ma_convert_pcm_frames_format(tempOut, ma_format_u8, tempIn, decoder.outputFormat, framesJustRead, decoder.outputChannels, ma_dither_mode_triangle);

        /* Write to the WAV file. */
        drwav_write_pcm_frames(&wav, framesJustRead, tempOut);

        if (framesJustRead < framesToRead) {
            break;
        }
    }

    drwav_uninit(&wav);
    return MA_SUCCESS;
}

int test_entry__dithering(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    const char* pInputFilePath;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    pInputFilePath = argv[1];
    
    result = test_dithering__u8(pInputFilePath);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}
