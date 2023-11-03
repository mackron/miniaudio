/*
USAGE: audioconverter [input file] [output file] [format] [channels] [rate]

EXAMPLES:
    audioconverter my_file.flac my_file.wav
    audioconverter my_file.flac my_file.wav f32 44100 linear --linear-order 8
*/
#define _CRT_SECURE_NO_WARNINGS /* For stb_vorbis' usage of fopen() instead of fopen_s(). */

#define STB_VORBIS_HEADER_ONLY
#include "../../extras/stb_vorbis.c"    /* Enables Vorbis decoding. */

#define MA_NO_DEVICE_IO
#define MA_NO_THREADING
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"

#include <stdio.h>

void print_usage()
{
    printf("USAGE: audioconverter [input file] [output file] [format] [channels] [rate]\n");
    printf("  [format] is optional and can be one of the following:\n");
    printf("    u8  8-bit unsigned integer\n");
    printf("    s16 16-bit signed integer\n");
    printf("    s24 24-bit signed integer (tightly packed)\n");
    printf("    s32 32-bit signed integer\n");
    printf("    f32 32-bit floating point\n");
    printf("  [channels] is optional and in the range of %d and %d\n", MA_MIN_CHANNELS, MA_MAX_CHANNELS);
    printf("  [rate] is optional and in the range of %d and %d\n", ma_standard_sample_rate_min, ma_standard_sample_rate_max);
    printf("\n");
    printf("PARAMETERS:\n");
    printf("  --linear-order [0..%d]\n", MA_MAX_FILTER_ORDER);
}

ma_result do_conversion(ma_decoder* pDecoder, ma_encoder* pEncoder)
{
    ma_result result = MA_SUCCESS;

    MA_ASSERT(pDecoder != NULL);
    MA_ASSERT(pEncoder != NULL);

    /*
    All we do is read from the decoder and then write straight to the encoder. All of the necessary data conversion
    will happen internally.
    */
    for (;;) {
        ma_uint8 pRawData[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
        ma_uint64 framesReadThisIteration;
        ma_uint64 framesToReadThisIteration;

        framesToReadThisIteration = sizeof(pRawData) / ma_get_bytes_per_frame(pDecoder->outputFormat, pDecoder->outputChannels);
        result = ma_decoder_read_pcm_frames(pDecoder, pRawData, framesToReadThisIteration, &framesReadThisIteration);
        if (result != MA_SUCCESS) {
            break;  /* Reached the end, or an error occurred. */
        }

        /* At this point we have the raw data from the decoder. We now just need to write it to the encoder. */
        ma_encoder_write_pcm_frames(pEncoder, pRawData, framesReadThisIteration, NULL);

        /* Get out of the loop if we've reached the end. */
        if (framesReadThisIteration < framesToReadThisIteration) {
            break;
        }
    }

    return result;
}

ma_bool32 is_number(const char* str)
{
    if (str == NULL || str[0] == '\0') {
        return MA_FALSE;
    }

    while (str[0] != '\0') {
        if (str[0] < '0' || str[0] > '9') {
            return MA_FALSE;
        }

        str += 1;
    }

    return MA_TRUE;
}

ma_bool32 try_parse_uint32_in_range(const char* str, ma_uint32* pValue, ma_uint32 lo, ma_uint32 hi)
{
    ma_uint32 x;

    if (!is_number(str)) {
        return MA_FALSE;    /* Not an integer. */
    }

    x = (ma_uint32)atoi(str);
    if (x < lo || x > hi) {
        return MA_FALSE;    /* Out of range. */
    }

    if (pValue != NULL) {
        *pValue = x;
    }
    
    return MA_TRUE;
}

ma_bool32 try_parse_format(const char* str, ma_format* pValue)
{
    ma_format format;

    /*  */ if (strcmp(str, "u8") == 0) {
        format = ma_format_u8;
    } else if (strcmp(str, "s16") == 0) {
        format = ma_format_s16;
    } else if (strcmp(str, "s24") == 0) {
        format = ma_format_s24;
    } else if (strcmp(str, "s32") == 0) {
        format = ma_format_s32;
    } else if (strcmp(str, "f32") == 0) {
        format = ma_format_f32;
    } else {
        return MA_FALSE;    /* Not a format. */
    }

    if (pValue != NULL) {
        *pValue = format;
    }

    return MA_TRUE;
}

ma_bool32 try_parse_channels(const char* str, ma_uint32* pValue)
{
    return try_parse_uint32_in_range(str, pValue, MA_MIN_CHANNELS, MA_MAX_CHANNELS);
}

ma_bool32 try_parse_sample_rate(const char* str, ma_uint32* pValue)
{
    return try_parse_uint32_in_range(str, pValue, ma_standard_sample_rate_min, ma_standard_sample_rate_max);
}

ma_bool32 try_parse_resample_algorithm(const char* str, ma_resample_algorithm* pValue)
{
    ma_resample_algorithm algorithm;

    /*  */ if (strcmp(str, "linear") == 0) {
        algorithm = ma_resample_algorithm_linear;
    } else {
        return MA_FALSE;    /* Not a valid algorithm */
    }

    if (pValue != NULL) {
        *pValue = algorithm;
    }

    return MA_TRUE;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    ma_encoding_format outputEncodingFormat;
    ma_format format = ma_format_unknown;
    ma_uint32 channels = 0;
    ma_uint32 rate = 0;
    ma_resample_algorithm resampleAlgorithm;
    ma_uint32 linearOrder = 8;
    int iarg;
    const char* pOutputFilePath;

    print_usage();

    /* Print help if requested. */
    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage();
            return 0;
        }
    }

    if (argc < 3) {
        print_usage();
        return -1;
    }

    resampleAlgorithm = ma_resample_algorithm_linear;

    /*
    The fourth and fifth arguments can be a format and/or rate specifier. It doesn't matter which order they are in as we can identify them by whether or
    not it's a number. If it's a number we assume it's a sample rate, otherwise we assume it's a format specifier.
    */
    for (iarg = 3; iarg < argc; iarg += 1) {
        if (strcmp(argv[iarg], "--linear-order") == 0) {
            iarg += 1;
            if (iarg >= argc) {
                break;
            }

            if (!try_parse_uint32_in_range(argv[iarg], &linearOrder, 0, 8)) {
                printf("Expecting a number between 0 and %d for --linear-order.\n", MA_MAX_FILTER_ORDER);
                return -1;
            }
            
            continue;
        }

        if (try_parse_resample_algorithm(argv[iarg], &resampleAlgorithm)) {
            continue;
        }

        if (try_parse_format(argv[iarg], &format)) {
            continue;
        }

        if (try_parse_channels(argv[iarg], &channels)) {
            continue;
        }

        if (try_parse_sample_rate(argv[iarg], &rate)) {
            continue;
        }

        /* Getting here means we have an unknown parameter. */
        printf("Warning: Unknown parameter \"%s\"", argv[iarg]);
    }

    /* Initialize a decoder for the input file. */
    decoderConfig = ma_decoder_config_init(format, channels, rate);
    decoderConfig.resampling.algorithm = resampleAlgorithm;
    decoderConfig.resampling.linear.lpfOrder = linearOrder;

    result = ma_decoder_init_file(argv[1], &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        printf("Failed to open input file. Check the file exists and the format is supported. Supported input formats:\n");
    #if defined(dr_opus_h)
        printf("    Opus\n");
    #endif
    #if defined(dr_mp3_h)
        printf("    MP3\n");
    #endif    
    #if defined(dr_flac_h)
        printf("    FLAC\n");
    #endif    
    #if defined(STB_VORBIS_INCLUDE_STB_VORBIS_H)
        printf("    Vorbis\n");
    #endif
    #if defined(dr_wav_h)
        printf("    WAV\n");
    #endif
        return (int)result;
    }


    pOutputFilePath = argv[2];

    outputEncodingFormat = ma_encoding_format_wav;  /* Wave by default in case we don't know the file extension. */
    if (ma_path_extension_equal(pOutputFilePath, "wav")) {
        outputEncodingFormat = ma_encoding_format_wav;
    } else {
        printf("Warning: Unknown file extension \"%s\". Encoding as WAV.\n", ma_path_extension(pOutputFilePath));
    }

    /* Initialize the encoder for the output file. */
    encoderConfig = ma_encoder_config_init(outputEncodingFormat, decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate);
    result = ma_encoder_init_file(pOutputFilePath, &encoderConfig, &encoder);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        printf("Failed to open output file. Check that the directory exists and that the file is not already opened by another process. %s", ma_result_description(result));
        return -1;
    }


    /* We have our decoder and encoder ready, so now we can do the conversion. */
    result = do_conversion(&decoder, &encoder);
    
    
    /* Done. */
    ma_encoder_uninit(&encoder);
    ma_decoder_uninit(&decoder);

    return (int)result;
}


/* stb_vorbis implementation must come after the implementation of miniaudio. */
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(push)
    #pragma warning(disable:4100)   /* unreferenced formal parameter */
    #pragma warning(disable:4244)   /* '=': conversion from '' to '', possible loss of data */
    #pragma warning(disable:4245)   /* '=': conversion from '' to '', signed/unsigned mismatch */
    #pragma warning(disable:4456)   /* declaration of '' hides previous local declaration */
    #pragma warning(disable:4457)   /* declaration of '' hides function parameter */
    #pragma warning(disable:4701)   /* potentially uninitialized local variable '' used */
#else
#endif
#undef STB_VORBIS_HEADER_ONLY
#include "../../extras/stb_vorbis.c"
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(pop)
#else
#endif
