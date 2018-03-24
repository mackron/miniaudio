// Uncomment this to include Vorbis decoding tests, albeit with some annoying warnings with MinGW.
//#define MAL_INCLUDE_VORBIS_TESTS

#include "../extras/dr_flac.h"
#include "../extras/dr_mp3.h"
#include "../extras/dr_wav.h"

#ifdef MAL_INCLUDE_VORBIS_TESTS
    #define STB_VORBIS_HEADER_ONLY
    #include "../extras/stb_vorbis.c"
#endif

#define MAL_IMPLEMENTATION
#include "../mini_al.h"

mal_backend g_Backends[] = {
    mal_backend_wasapi,
    mal_backend_dsound,
    mal_backend_winmm,
    mal_backend_oss,
    mal_backend_pulseaudio,
    mal_backend_alsa,
    mal_backend_jack,
    mal_backend_opensl,
    mal_backend_openal,
    mal_backend_sdl,
    mal_backend_null
};

void on_log(mal_context* pContext, mal_device* pDevice, const char* message)
{
    (void)pContext;
    (void)pDevice;
    printf("%s\n", message);
}


void* open_and_read_file_data(const char* filePath, size_t* pSizeOut)
{
    // Safety.
    if (pSizeOut) *pSizeOut = 0;

    if (filePath == NULL) {
        return NULL;
    }

    FILE* pFile;
#if _MSC_VER
    if (fopen_s(&pFile, filePath, "rb") != 0) {
        return NULL;
    }
#else
    pFile = fopen(filePath, "rb");
    if (pFile == NULL) {
        return NULL;
    }
#endif

    fseek(pFile, 0, SEEK_END);
    mal_uint64 fileSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    if (fileSize > SIZE_MAX) {
        fclose(pFile);
        return NULL;
    }

    void* pFileData = mal_malloc((size_t)fileSize);    // <-- Safe cast due to the check above.
    if (pFileData == NULL) {
        fclose(pFile);
        return NULL;
    }

    size_t bytesRead = fread(pFileData, 1, (size_t)fileSize, pFile);
    if (bytesRead != fileSize) {
        mal_free(pFileData);
        fclose(pFile);
        return NULL;
    }

    fclose(pFile);

    if (pSizeOut) {
        *pSizeOut = (size_t)fileSize;
    }

    return pFileData;
}

void* load_raw_audio_data(const char* filePath, mal_format format, mal_uint64* pBenchmarkFrameCount)
{
    mal_assert(pBenchmarkFrameCount != NULL);
    *pBenchmarkFrameCount = 0;

    size_t fileSize;
    void* pFileData = open_and_read_file_data(filePath, &fileSize);
    if (pFileData == NULL) {
        printf("Cound not open file %s\n", filePath);
        return NULL;
    }

    *pBenchmarkFrameCount = fileSize / mal_get_sample_size_in_bytes(format);
    return pFileData;
}

void* load_benchmark_base_data(mal_format format, mal_uint32* pChannelsOut, mal_uint32* pSampleRateOut, mal_uint64* pBenchmarkFrameCount)
{
    mal_assert(pChannelsOut != NULL);
    mal_assert(pSampleRateOut != NULL);
    mal_assert(pBenchmarkFrameCount != NULL);

    *pChannelsOut = 1;
    *pSampleRateOut = 8000;
    *pBenchmarkFrameCount = 0;

    const char* filePath = NULL;
    switch (format) {
        case mal_format_u8:  filePath = "res/benchmarks/pcm_u8_to_u8__mono_8000.raw";   break;
        case mal_format_s16: filePath = "res/benchmarks/pcm_s16_to_s16__mono_8000.raw"; break;
        case mal_format_s24: filePath = "res/benchmarks/pcm_s24_to_s24__mono_8000.raw"; break;
        case mal_format_s32: filePath = "res/benchmarks/pcm_s32_to_s32__mono_8000.raw"; break;
        case mal_format_f32: filePath = "res/benchmarks/pcm_f32_to_f32__mono_8000.raw"; break;
        default: return NULL;
    }

    return load_raw_audio_data(filePath, format, pBenchmarkFrameCount);
}

int mal_pcm_compare(const void* a, const void* b, mal_uint64 count, mal_format format, float allowedDifference)
{
    int result = 0;

    const mal_uint8* a_u8  = (const mal_uint8*)a;
    const mal_uint8* b_u8  = (const mal_uint8*)b;
    const mal_int16* a_s16 = (const mal_int16*)a;
    const mal_int16* b_s16 = (const mal_int16*)b;
    const mal_int32* a_s32 = (const mal_int32*)a;
    const mal_int32* b_s32 = (const mal_int32*)b;
    const float*     a_f32 = (const float*    )a;
    const float*     b_f32 = (const float*    )b;

    for (mal_uint64 i = 0; i < count; ++i) {
        switch (format) {
            case mal_format_u8:
            {
                mal_uint8 sampleA = a_u8[i];
                mal_uint8 sampleB = b_u8[i];
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %I64u not equal. %d != %d (diff: %d)\n", i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case mal_format_s16:
            {
                mal_int16 sampleA = a_s16[i];
                mal_int16 sampleB = b_s16[i];
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %I64u not equal. %d != %d (diff: %d)\n", i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case mal_format_s24:
            {
                mal_int32 sampleA = ((mal_int32)(((mal_uint32)(a_u8[i*3+0]) << 8) | ((mal_uint32)(a_u8[i*3+1]) << 16) | ((mal_uint32)(a_u8[i*3+2])) << 24)) >> 8;
                mal_int32 sampleB = ((mal_int32)(((mal_uint32)(b_u8[i*3+0]) << 8) | ((mal_uint32)(b_u8[i*3+1]) << 16) | ((mal_uint32)(b_u8[i*3+2])) << 24)) >> 8;;
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %I64u not equal. %d != %d (diff: %d)\n", i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case mal_format_s32:
            {
                mal_int32 sampleA = a_s32[i];
                mal_int32 sampleB = b_s32[i];
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %I64u not equal. %d != %d (diff: %d)\n", i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case mal_format_f32:
            {
                float sampleA = a_f32[i];
                float sampleB = b_f32[i];
                if (sampleA != sampleB) {
                    float difference = sampleA - sampleB;
                    difference = (difference < 0) ? -difference : difference;

                    if (difference > allowedDifference) {
                        printf("Sample %I64u not equal. %.8f != %.8f (diff: %.8f)\n", i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            default: return -1;
        }
    }

    return result;
}

int do_format_conversion_test(mal_format formatIn, mal_format formatOut)
{
    int result = 0;

    mal_uint32 channels;
    mal_uint32 sampleRate;
    mal_uint64 baseFrameCount;
    mal_int16* pBaseData = (mal_int16*)load_benchmark_base_data(formatIn, &channels, &sampleRate, &baseFrameCount);
    if (pBaseData == NULL) {
        return -1;  // Failed to load file.
    }

    void (* onConvertPCM)(void* dst, const void* src, mal_uint64 count, mal_dither_mode ditherMode) = NULL;
    const char* pBenchmarkFilePath = NULL;

    switch (formatIn) {
        case mal_format_u8:
        {
            switch (formatOut) {
                case mal_format_u8:
                {
                    onConvertPCM = mal_pcm_u8_to_u8;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_u8__mono_8000.raw";
                } break;
                case mal_format_s16:
                {
                    onConvertPCM = mal_pcm_u8_to_s16__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_s16__mono_8000.raw";
                } break;
                case mal_format_s24:
                {
                    onConvertPCM = mal_pcm_u8_to_s24__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_s24__mono_8000.raw";
                } break;
                case mal_format_s32:
                {
                    onConvertPCM = mal_pcm_u8_to_s32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_s32__mono_8000.raw";
                } break;
                case mal_format_f32:
                {
                    onConvertPCM = mal_pcm_u8_to_f32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        case mal_format_s16:
        {
            switch (formatOut) {
                case mal_format_u8:
                {
                    onConvertPCM = mal_pcm_s16_to_u8__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_u8__mono_8000.raw";
                } break;
                case mal_format_s16:
                {
                    onConvertPCM = mal_pcm_s16_to_s16;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_s16__mono_8000.raw";
                } break;
                case mal_format_s24:
                {
                    onConvertPCM = mal_pcm_s16_to_s24__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_s24__mono_8000.raw";
                } break;
                case mal_format_s32:
                {
                    onConvertPCM = mal_pcm_s16_to_s32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_s32__mono_8000.raw";
                } break;
                case mal_format_f32:
                {
                    onConvertPCM = mal_pcm_s16_to_f32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        case mal_format_s24:
        {
            switch (formatOut) {
                case mal_format_u8:
                {
                    onConvertPCM = mal_pcm_s24_to_u8__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_u8__mono_8000.raw";
                } break;
                case mal_format_s16:
                {
                    onConvertPCM = mal_pcm_s24_to_s16__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_s16__mono_8000.raw";
                } break;
                case mal_format_s24:
                {
                    onConvertPCM = mal_pcm_s24_to_s24;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_s24__mono_8000.raw";
                } break;
                case mal_format_s32:
                {
                    onConvertPCM = mal_pcm_s24_to_s32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_s32__mono_8000.raw";
                } break;
                case mal_format_f32:
                {
                    onConvertPCM = mal_pcm_s24_to_f32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        case mal_format_s32:
        {
            switch (formatOut) {
                case mal_format_u8:
                {
                    onConvertPCM = mal_pcm_s32_to_u8__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_u8__mono_8000.raw";
                } break;
                case mal_format_s16:
                {
                    onConvertPCM = mal_pcm_s32_to_s16__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_s16__mono_8000.raw";
                } break;
                case mal_format_s24:
                {
                    onConvertPCM = mal_pcm_s32_to_s24__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_s24__mono_8000.raw";
                } break;
                case mal_format_s32:
                {
                    onConvertPCM = mal_pcm_s32_to_s32;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_s32__mono_8000.raw";
                } break;
                case mal_format_f32:
                {
                    onConvertPCM = mal_pcm_s32_to_f32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        case mal_format_f32:
        {
            switch (formatOut) {
                case mal_format_u8:
                {
                    onConvertPCM = mal_pcm_f32_to_u8__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_u8__mono_8000.raw";
                } break;
                case mal_format_s16:
                {
                    onConvertPCM = mal_pcm_f32_to_s16__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_s16__mono_8000.raw";
                } break;
                case mal_format_s24:
                {
                    onConvertPCM = mal_pcm_f32_to_s24__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_s24__mono_8000.raw";
                } break;
                case mal_format_s32:
                {
                    onConvertPCM = mal_pcm_f32_to_s32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_s32__mono_8000.raw";
                } break;
                case mal_format_f32:
                {
                    onConvertPCM = mal_pcm_f32_to_f32;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        default:
        {
            result = -1;
        } break;
    }


    if (result != 0) {
        mal_free(pBaseData);
        return result;
    }

    // We need to allow a very small amount of difference to each sample because the software that generated our testing benchmarks can use slightly
    // different (but still correct) algorithms which produce slightly different results. I'm allowing for this variability in my basic comparison
    // tests, but testing things like dithering will require more detailed testing which I'll probably do separate to this test project.
    mal_bool32 allowSmallDifference = MAL_TRUE;
    float allowedDifference = 0;
    if (allowSmallDifference) {
        if (formatOut == mal_format_f32) {
            switch (formatIn) {
                case mal_format_u8:  allowedDifference = 1.0f / 255        * 2; break;
                case mal_format_s16: allowedDifference = 1.0f / 32767      * 2; break;
                case mal_format_s24: allowedDifference = 1.0f / 8388608    * 2; break;
                case mal_format_s32: allowedDifference = 1.0f / 2147483647 * 2; break;
                case mal_format_f32: allowedDifference = 0; break;
                default: break;
            }
        } else {
            allowedDifference = 1;
        }
    }

    mal_uint64 benchmarkFrameCount;
    void* pBenchmarkData = load_raw_audio_data(pBenchmarkFilePath, formatOut, &benchmarkFrameCount);
    if (pBenchmarkData != NULL) {
        if (benchmarkFrameCount == baseFrameCount) {
            void* pConvertedData = (void*)mal_malloc(benchmarkFrameCount * mal_get_sample_size_in_bytes(formatOut));
            if (pConvertedData != NULL) {
                onConvertPCM(pConvertedData, pBaseData, (mal_uint32)benchmarkFrameCount, mal_dither_mode_none);
                result = mal_pcm_compare(pBenchmarkData, pConvertedData, benchmarkFrameCount, formatOut, allowedDifference);
                if (result == 0) {
                    printf("PASSED.\n");
                }
            } else {
                printf("FAILED. Out of memory.\n");
                result = -3;
            }
        } else {
            printf("FAILED. Frame count mismatch.\n");
            result = -2;
        }
    } else {
        printf("FAILED.");
        result = -1;
    }


    mal_free(pBaseData);
    mal_free(pBenchmarkData);
    return result;
}

int do_format_conversion_tests_u8()
{
    int result = 0;

    printf("PCM u8 -> u8... ");
    if (do_format_conversion_test(mal_format_u8, mal_format_u8) != 0) {
        result = -1;
    }

    printf("PCM u8 -> s16... ");
    if (do_format_conversion_test(mal_format_u8, mal_format_s16) != 0) {
        result = -1;
    }

    printf("PCM u8 -> s24... ");
    if (do_format_conversion_test(mal_format_u8, mal_format_s24) != 0) {
        result = -1;
    }

    printf("PCM u8 -> s32... ");
    if (do_format_conversion_test(mal_format_u8, mal_format_s32) != 0) {
        result = -1;
    }

    printf("PCM u8 -> f32... ");
    if (do_format_conversion_test(mal_format_u8, mal_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests_s16()
{
    int result = 0;

    printf("PCM s16 -> u8... ");
    if (do_format_conversion_test(mal_format_s16, mal_format_u8) != 0) {
        result = -1;
    }

    printf("PCM s16 -> s16... ");
    if (do_format_conversion_test(mal_format_s16, mal_format_s16) != 0) {
        result = -1;
    }

    printf("PCM s16 -> s24... ");
    if (do_format_conversion_test(mal_format_s16, mal_format_s24) != 0) {
        result = -1;
    }

    printf("PCM s16 -> s32... ");
    if (do_format_conversion_test(mal_format_s16, mal_format_s32) != 0) {
        result = -1;
    }

    printf("PCM s16 -> f32... ");
    if (do_format_conversion_test(mal_format_s16, mal_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests_s24()
{
    int result = 0;

    printf("PCM s24 -> u8... ");
    if (do_format_conversion_test(mal_format_s24, mal_format_u8) != 0) {
        result = -1;
    }

    printf("PCM s24 -> s16... ");
    if (do_format_conversion_test(mal_format_s24, mal_format_s16) != 0) {
        result = -1;
    }

    printf("PCM s24 -> s24... ");
    if (do_format_conversion_test(mal_format_s24, mal_format_s24) != 0) {
        result = -1;
    }

    printf("PCM s24 -> s32... ");
    if (do_format_conversion_test(mal_format_s24, mal_format_s32) != 0) {
        result = -1;
    }

    printf("PCM s24 -> f32... ");
    if (do_format_conversion_test(mal_format_s24, mal_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests_s32()
{
    int result = 0;

    printf("PCM s32 -> u8... ");
    if (do_format_conversion_test(mal_format_s32, mal_format_u8) != 0) {
        result = -1;
    }

    printf("PCM s32 -> s16... ");
    if (do_format_conversion_test(mal_format_s32, mal_format_s16) != 0) {
        result = -1;
    }

    printf("PCM s32 -> s24... ");
    if (do_format_conversion_test(mal_format_s32, mal_format_s24) != 0) {
        result = -1;
    }

    printf("PCM s32 -> s32... ");
    if (do_format_conversion_test(mal_format_s32, mal_format_s32) != 0) {
        result = -1;
    }

    printf("PCM s32 -> f32... ");
    if (do_format_conversion_test(mal_format_s32, mal_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests_f32()
{
    int result = 0;

    printf("PCM f32 -> u8... ");
    if (do_format_conversion_test(mal_format_f32, mal_format_u8) != 0) {
        result = -1;
    }

    printf("PCM f32 -> s16... ");
    if (do_format_conversion_test(mal_format_f32, mal_format_s16) != 0) {
        result = -1;
    }

    printf("PCM f32 -> s24... ");
    if (do_format_conversion_test(mal_format_f32, mal_format_s24) != 0) {
        result = -1;
    }

    printf("PCM f32 -> s32... ");
    if (do_format_conversion_test(mal_format_f32, mal_format_s32) != 0) {
        result = -1;
    }

    printf("PCM f32 -> f32... ");
    if (do_format_conversion_test(mal_format_f32, mal_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests()
{
    int result = 0;

    if (do_format_conversion_tests_u8() != 0) {
        result = -1;
    }
    if (do_format_conversion_tests_s16() != 0) {
        result = -1;
    }
    if (do_format_conversion_tests_s24() != 0) {
        result = -1;
    }
    if (do_format_conversion_tests_s32() != 0) {
        result = -1;
    }
    if (do_format_conversion_tests_f32() != 0) {
        result = -1;
    }

    return result;
}


int do_backend_test(mal_backend backend)
{
    mal_result result = MAL_SUCCESS;
    mal_context context;
    mal_device_info* pPlaybackDeviceInfos;
    mal_uint32 playbackDeviceCount;
    mal_device_info* pCaptureDeviceInfos;
    mal_uint32 captureDeviceCount;

    printf("--- %s ---\n", mal_get_backend_name(backend));

    // Context.
    printf("  Creating Context... ");
    {
        mal_context_config contextConfig = mal_context_config_init(on_log);

        result = mal_context_init(&backend, 1, &contextConfig, &context);
        if (result == MAL_SUCCESS) {
            printf(" Done\n");
        } else {
            if (result == MAL_NO_BACKEND) {
                printf(" Not supported\n");
                printf("--- End %s ---\n", mal_get_backend_name(backend));
                printf("\n");
                return 0;
            } else {
                printf(" Failed\n");
                goto done;
            }
        }
    }

    // Enumeration.
    printf("  Enumerating Devices... ");
    {
        result = mal_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
        if (result == MAL_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed\n");
            goto done;
        }

        printf("    Playback Devices (%d)\n", playbackDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
        }
    }

    // Device Information.
    printf("  Getting Device Information...\n");
    {
        printf("    Playback Devices (%d)\n", playbackDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
            
            result = mal_context_get_device_info(&context, mal_device_type_playback, &pPlaybackDeviceInfos[iDevice].id, mal_share_mode_shared, &pPlaybackDeviceInfos[iDevice]);
            if (result == MAL_SUCCESS) {
                printf("        Name: %s\n", pPlaybackDeviceInfos[iDevice].name);
            } else {
                printf("        ERROR\n");
            }
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
            
            result = mal_context_get_device_info(&context, mal_device_type_capture, &pCaptureDeviceInfos[iDevice].id, mal_share_mode_shared, &pCaptureDeviceInfos[iDevice]);
            if (result == MAL_SUCCESS) {
                printf("        Name: %s\n", pCaptureDeviceInfos[iDevice].name);
            } else {
                printf("        ERROR\n");
            }
        }
    }

done:
    printf("--- End %s ---\n", mal_get_backend_name(backend));
    printf("\n");
    
    mal_context_uninit(&context);
    return (result == MAL_SUCCESS) ? 0 : -1;
}

int do_backend_tests()
{
    mal_bool32 hasErrorOccurred = MAL_FALSE;

    // Tests are performed on a per-backend basis.
    for (size_t iBackend = 0; iBackend < mal_countof(g_Backends); ++iBackend) {
        int result = do_backend_test(g_Backends[iBackend]);
        if (result < 0) {
            hasErrorOccurred = MAL_TRUE;
        }
    }

    return (hasErrorOccurred) ? -1 : 0;
}


typedef struct
{
    mal_decoder* pDecoder;
    mal_event endOfPlaybackEvent;
} playback_test_callback_data;

mal_uint32 on_send__playback_test(mal_device* pDevice, mal_uint32 frameCount, void* pFrames)
{
    playback_test_callback_data* pData = (playback_test_callback_data*)pDevice->pUserData;
    mal_assert(pData != NULL);

    mal_uint64 framesRead = mal_decoder_read(pData->pDecoder, frameCount, pFrames);
    if (framesRead == 0) {
        mal_event_signal(&pData->endOfPlaybackEvent);
    }

    return (mal_uint32)framesRead;
}

int do_playback_test(mal_backend backend)
{
    mal_result result = MAL_SUCCESS;
    mal_device device;
    mal_decoder decoder;
    mal_bool32 haveDevice = MAL_FALSE;
    mal_bool32 haveDecoder = MAL_FALSE;

    playback_test_callback_data callbackData;
    callbackData.pDecoder = &decoder;

    printf("--- %s ---\n", mal_get_backend_name(backend));

    // Device.
    printf("  Opening Device... ");
    {
        mal_context_config contextConfig = mal_context_config_init(on_log);
        mal_device_config deviceConfig = mal_device_config_init_default_playback(on_send__playback_test);

        result = mal_device_init_ex(&backend, 1, &contextConfig, mal_device_type_playback, NULL, &deviceConfig, &callbackData, &device);
        if (result == MAL_SUCCESS) {
            printf("Done\n");
        } else {
            if (result == MAL_NO_BACKEND) {
                printf(" Not supported\n");
                printf("--- End %s ---\n", mal_get_backend_name(backend));
                printf("\n");
                return 0;
            } else {
                printf(" Failed\n");
                goto done;
            }
        }
        haveDevice = MAL_TRUE;

        printf("    Is Passthrough: %s\n", (device.dsp.isPassthrough) ? "YES" : "NO");
    }

    printf("  Opening Decoder... ");
    {
        result = mal_event_init(device.pContext, &callbackData.endOfPlaybackEvent);
        if (result != MAL_SUCCESS) {
            printf("Failed to init event.\n");
            goto done;
        }
        
        mal_decoder_config decoderConfig = mal_decoder_config_init(device.format, device.channels, device.sampleRate);
        result = mal_decoder_init_file("res/sine_s16_mono_48000.wav", &decoderConfig, &decoder);
        if (result == MAL_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed to init decoder.\n");
            goto done;
        }
        haveDecoder = MAL_TRUE;
    }

    printf("  Press Enter to start playback... ");
    getchar();
    {
        result = mal_device_start(&device);
        if (result != MAL_SUCCESS) {
            printf("Failed to start device.\n");
            goto done;
        }

        mal_event_wait(&callbackData.endOfPlaybackEvent);   // Wait for the sound to finish.
        printf("Done\n");
    }

done:
    printf("--- End %s ---\n", mal_get_backend_name(backend));
    printf("\n");
    
    if (haveDevice) {
        mal_device_uninit(&device);
    }
    if (haveDecoder) {
        mal_decoder_uninit(&decoder);
    }
    return (result == MAL_SUCCESS) ? 0 : -1;
}

int do_playback_tests()
{
    mal_bool32 hasErrorOccurred = MAL_FALSE;

    for (size_t iBackend = 0; iBackend < mal_countof(g_Backends); ++iBackend) {
        int result = do_playback_test(g_Backends[iBackend]);
        if (result < 0) {
            hasErrorOccurred = MAL_TRUE;
        }
    }

    return (hasErrorOccurred) ? -1 : 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    mal_bool32 hasErrorOccurred = MAL_FALSE;
    int result = 0;

    printf("=== TESTING FORMAT CONVERSION ===\n");
    result = do_format_conversion_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING FORMAT CONVERSION ===\n");

    printf("\n");
    
    printf("=== TESTING BACKENDS ===\n");
    result = do_backend_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING BACKENDS ===\n");

    printf("\n");

    printf("=== TESTING DEFAULT PLAYBACK DEVICES ===\n");
    result = do_playback_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING DEFAULT PLAYBACK DEVICES ===\n");

    return (hasErrorOccurred) ? -1 : 0;
}

#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

#ifdef MAL_INCLUDE_VORBIS_TESTS
    #if defined(_MSC_VER)
        #pragma warning(push)
        #pragma warning(disable:4456)
        #pragma warning(disable:4457)
        #pragma warning(disable:4100)
        #pragma warning(disable:4244)
        #pragma warning(disable:4701)
        #pragma warning(disable:4245)
    #endif
    #if defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wunused-value"
        #pragma GCC diagnostic ignored "-Wunused-parameter"
        #ifndef __clang__
        #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
        #endif
    #endif
    #undef STB_VORBIS_HEADER_ONLY
    #include "../extras/stb_vorbis.c"
    #if defined(_MSC_VER)
        #pragma warning(pop)
    #endif
    #if defined(__GNUC__)
        #pragma GCC diagnostic pop
    #endif
#endif