
ma_result test_dithering__u8(const char* pInputFilePath)
{
    const char* pOutputFilePath = TEST_OUTPUT_DIR"/dithering_u8.wav";
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    
    decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
    result = ma_decoder_init_file(pInputFilePath, &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_u8, decoder.outputChannels, decoder.outputSampleRate);
    result = ma_encoder_init_file(pOutputFilePath, &encoderConfig, &encoder);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return result;
    }

    for (;;) {
        ma_uint8 tempIn[4096];
        ma_uint8 tempOut[4096];
        ma_uint64 tempCapIn  = sizeof(tempIn)  / ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
        ma_uint64 tempCapOut = sizeof(tempOut) / ma_get_bytes_per_frame(ma_format_u8, decoder.outputChannels);
        ma_uint64 framesToRead;
        ma_uint64 framesJustRead;

        framesToRead = ma_min(tempCapIn, tempCapOut);
        ma_decoder_read_pcm_frames(&decoder, tempIn, framesToRead, &framesJustRead);

        /* Convert, with dithering. */
        ma_convert_pcm_frames_format(tempOut, ma_format_u8, tempIn, decoder.outputFormat, framesJustRead, decoder.outputChannels, ma_dither_mode_triangle);

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
