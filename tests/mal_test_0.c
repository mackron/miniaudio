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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
void main_loop__em()
{
}
#endif


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

FILE* mal_fopen(const char* filePath, const char* openMode)
{
    FILE* pFile;
#if _MSC_VER
    if (fopen_s(&pFile, filePath, openMode) != 0) {
        return NULL;
    }
#else
    pFile = fopen(filePath, openMode);
    if (pFile == NULL) {
        return NULL;
    }
#endif

    return pFile;
}

void* open_and_read_file_data(const char* filePath, size_t* pSizeOut)
{
    // Safety.
    if (pSizeOut) *pSizeOut = 0;

    if (filePath == NULL) {
        return NULL;
    }

    FILE* pFile = mal_fopen(filePath, "rb");
    if (pFile == NULL) {
        return NULL;
    }

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


int do_types_tests()
{
    int result = 0;

    int sizeof_int8 = sizeof(mal_int8);
    int sizeof_uint8 = sizeof(mal_uint8);
    int sizeof_int16 = sizeof(mal_int16);
    int sizeof_uint16 = sizeof(mal_uint16);
    int sizeof_int32 = sizeof(mal_int32);
    int sizeof_uint32 = sizeof(mal_uint32);
    int sizeof_int64 = sizeof(mal_int64);
    int sizeof_uint64 = sizeof(mal_uint64);
    int sizeof_float32 = sizeof(float);
    int sizeof_float64 = sizeof(double);
    int sizeof_uintptr = sizeof(mal_uintptr);

    printf("sizeof(mal_int8)    1 = %d", sizeof_int8);
    if (sizeof_int8 != 1) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(mal_uint8)   1 = %d", sizeof_uint8);
    if (sizeof_uint8 != 1) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    printf("sizeof(mal_int16)   2 = %d", sizeof_int16);
    if (sizeof_int16 != 2) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(mal_uint16)  2 = %d", sizeof_uint16);
    if (sizeof_uint16 != 2) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    printf("sizeof(mal_int32)   4 = %d", sizeof_int32);
    if (sizeof_int32 != 4) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(mal_uint32)  4 = %d", sizeof_uint32);
    if (sizeof_uint32 != 4) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    printf("sizeof(mal_int64)   8 = %d", sizeof_int64);
    if (sizeof_int64 != 8) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(mal_uint64)  8 = %d", sizeof_uint64);
    if (sizeof_uint64 != 8) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    printf("sizeof(float)       4 = %d", sizeof_float32);
    if (sizeof_float32 != 4) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(double)      8 = %d", sizeof_float64);
    if (sizeof_float64 != 8) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    printf("sizeof(mal_uintptr) %d = %d", (int)sizeof(void*), sizeof_uintptr);
    if (sizeof_uintptr != sizeof(void*)) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    return result;
}

int do_aligned_malloc_tests()
{
    int result = 0;

    // We just do a whole bunch of malloc's and check them. This can probably be made more exhaustive.
    void* p[1024];
    for (mal_uint32 i = 0; i < mal_countof(p); ++i) {
        mal_uintptr alignment = MAL_SIMD_ALIGNMENT;

        p[i] = mal_aligned_malloc(1024, alignment);
        if (((mal_uintptr)p[i] & (alignment-1)) != 0) {
            printf("FAILED\n");
            result = -1;
        }
    }

    // Free.
    for (mal_uint32 i = 0; i < mal_countof(p); ++i) {
        mal_aligned_free(p[i]);
    }

    if (result == 0) {
        printf("PASSED\n");
    }

    return result;
}

int do_core_tests()
{
    int result = 0;

    printf("Types...\n");
    if (do_types_tests() != 0) {
        printf("FAILED\n");
        result = -1;
    } else {
        printf("PASSED\n");
    }

    printf("Aligned malloc... ");
    if (do_aligned_malloc_tests() != 0) {
        result = -1;
    }

    return result;
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

    *pBenchmarkFrameCount = fileSize / mal_get_bytes_per_sample(format);
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
                        printf("Sample %u not equal. %d != %d (diff: %d)\n", (mal_int32)i, sampleA, sampleB, sampleA - sampleB);
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
                        printf("Sample %u not equal. %d != %d (diff: %d)\n", (mal_int32)i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case mal_format_s24:
            {
                mal_int32 sampleA = ((mal_int32)(((mal_uint32)(a_u8[i*3+0]) << 8) | ((mal_uint32)(a_u8[i*3+1]) << 16) | ((mal_uint32)(a_u8[i*3+2])) << 24)) >> 8;
                mal_int32 sampleB = ((mal_int32)(((mal_uint32)(b_u8[i*3+0]) << 8) | ((mal_uint32)(b_u8[i*3+1]) << 16) | ((mal_uint32)(b_u8[i*3+2])) << 24)) >> 8;
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %u not equal. %d != %d (diff: %d)\n", (mal_int32)i, sampleA, sampleB, sampleA - sampleB);
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
                        printf("Sample %u not equal. %d != %d (diff: %d)\n", (mal_int32)i, sampleA, sampleB, sampleA - sampleB);
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
                        printf("Sample %u not equal. %.8f != %.8f (diff: %.8f)\n", (mal_int32)i, sampleA, sampleB, sampleA - sampleB);
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
            void* pConvertedData = (void*)mal_malloc((size_t)benchmarkFrameCount * mal_get_bytes_per_sample(formatOut));
            if (pConvertedData != NULL) {
                onConvertPCM(pConvertedData, pBaseData, (mal_uint32)benchmarkFrameCount, mal_dither_mode_none);
                result = mal_pcm_compare(pBenchmarkData, pConvertedData, benchmarkFrameCount, formatOut, allowedDifference);
                if (result == 0) {
                    printf("PASSED\n");
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


int compare_interleaved_and_deinterleaved_buffers(const void* interleaved, const void** deinterleaved, mal_uint32 frameCount, mal_uint32 channels, mal_format format)
{
    mal_uint32 bytesPerSample = mal_get_bytes_per_sample(format);

    const mal_uint8* interleaved8 = (const mal_uint8*)interleaved;
    const mal_uint8** deinterleaved8 = (const mal_uint8**)deinterleaved;

    for (mal_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
        const mal_uint8* interleavedFrame = interleaved8 + iFrame*channels*bytesPerSample;

        for (mal_uint32 iChannel = 0; iChannel < channels; iChannel += 1) {
            const mal_uint8* deinterleavedFrame = deinterleaved8[iChannel] + iFrame*bytesPerSample;

            int result = memcmp(interleavedFrame + iChannel*bytesPerSample, deinterleavedFrame, bytesPerSample);
            if (result != 0) {
                return -1;
            }
        }
    }

    // Getting here means nothing failed.
    return 0;
}

int do_interleaving_test(mal_format format)
{
    // This test is simple. We start with a deinterleaved buffer. We then test interleaving. Then we deinterleave the interleaved buffer
    // and compare that the original. It should be bit-perfect. We do this for all channel counts.
    
    int result = 0;

    switch (format)
    {
        case mal_format_u8:
        {
            mal_uint8 src [MAL_MAX_CHANNELS][64];
            mal_uint8 dst [MAL_MAX_CHANNELS][64];
            mal_uint8 dsti[MAL_MAX_CHANNELS*64];
            void* ppSrc[MAL_MAX_CHANNELS];
            void* ppDst[MAL_MAX_CHANNELS];

            mal_uint32 frameCount = mal_countof(src[0]);
            mal_uint32 channelCount = mal_countof(src);
            for (mal_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (mal_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame] = (mal_uint8)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (mal_uint32 i = 0; i < channelCount; ++i) {
                mal_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                mal_pcm_interleave_u8__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }

                // Deinterleave.
                mal_pcm_deinterleave_u8__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }
            }
        } break;

        case mal_format_s16:
        {
            mal_int16 src [MAL_MAX_CHANNELS][64];
            mal_int16 dst [MAL_MAX_CHANNELS][64];
            mal_int16 dsti[MAL_MAX_CHANNELS*64];
            void* ppSrc[MAL_MAX_CHANNELS];
            void* ppDst[MAL_MAX_CHANNELS];

            mal_uint32 frameCount = mal_countof(src[0]);
            mal_uint32 channelCount = mal_countof(src);
            for (mal_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (mal_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame] = (mal_int16)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (mal_uint32 i = 0; i < channelCount; ++i) {
                mal_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                mal_pcm_interleave_s16__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }

                // Deinterleave.
                mal_pcm_deinterleave_s16__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }
            }
        } break;

        case mal_format_s24:
        {
            mal_uint8 src [MAL_MAX_CHANNELS][64*3];
            mal_uint8 dst [MAL_MAX_CHANNELS][64*3];
            mal_uint8 dsti[MAL_MAX_CHANNELS*64*3];
            void* ppSrc[MAL_MAX_CHANNELS];
            void* ppDst[MAL_MAX_CHANNELS];

            mal_uint32 frameCount = mal_countof(src[0])/3;
            mal_uint32 channelCount = mal_countof(src);
            for (mal_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (mal_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame*3 + 0] = (mal_uint8)iChannel;
                    src[iChannel][iFrame*3 + 1] = (mal_uint8)iChannel;
                    src[iChannel][iFrame*3 + 2] = (mal_uint8)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (mal_uint32 i = 0; i < channelCount; ++i) {
                mal_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                mal_pcm_interleave_s24__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", channelCountForThisIteration);
                    result = -1;
                    break;
                }

                // Deinterleave.
                mal_pcm_deinterleave_s24__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", channelCountForThisIteration);
                    result = -1;
                    break;
                }
            }
        } break;

        case mal_format_s32:
        {
            mal_int32 src [MAL_MAX_CHANNELS][64];
            mal_int32 dst [MAL_MAX_CHANNELS][64];
            mal_int32 dsti[MAL_MAX_CHANNELS*64];
            void* ppSrc[MAL_MAX_CHANNELS];
            void* ppDst[MAL_MAX_CHANNELS];

            mal_uint32 frameCount = mal_countof(src[0]);
            mal_uint32 channelCount = mal_countof(src);
            for (mal_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (mal_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame] = (mal_int32)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (mal_uint32 i = 0; i < channelCount; ++i) {
                mal_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                mal_pcm_interleave_s32__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }

                // Deinterleave.
                mal_pcm_deinterleave_s32__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }
            }
        } break;

        case mal_format_f32:
        {
            float src [MAL_MAX_CHANNELS][64];
            float dst [MAL_MAX_CHANNELS][64];
            float dsti[MAL_MAX_CHANNELS*64];
            void* ppSrc[MAL_MAX_CHANNELS];
            void* ppDst[MAL_MAX_CHANNELS];

            mal_uint32 frameCount = mal_countof(src[0]);
            mal_uint32 channelCount = mal_countof(src);
            for (mal_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (mal_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame] = (float)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (mal_uint32 i = 0; i < channelCount; ++i) {
                mal_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                mal_pcm_interleave_f32__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }

                // Deinterleave.
                mal_pcm_deinterleave_f32__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }
            }
        } break;

        default:
        {
            printf("Unknown format.");
            result = -1;
        } break;
    }


    if (result == 0) {
        printf("PASSED\n");
    }

    return result;
}

int do_interleaving_tests()
{
    int result = 0;

    printf("u8... ");
    if (do_interleaving_test(mal_format_u8) != 0) {
        result = -1;
    }

    printf("s16... ");
    if (do_interleaving_test(mal_format_s16) != 0) {
        result = -1;
    }

    printf("s24... ");
    if (do_interleaving_test(mal_format_s24) != 0) {
        result = -1;
    }

    printf("s32... ");
    if (do_interleaving_test(mal_format_s32) != 0) {
        result = -1;
    }

    printf("f32... ");
    if (do_interleaving_test(mal_format_f32) != 0) {
        result = -1;
    }

    return result;
}


mal_uint32 converter_test_interleaved_callback(mal_format_converter* pConverter, mal_uint32 frameCount, void* pFramesOut, void* pUserData)
{
    mal_sine_wave* pSineWave = (mal_sine_wave*)pUserData;
    mal_assert(pSineWave != NULL);

    float* pFramesOutF32 = (float*)pFramesOut;

    for (mal_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
        float sample;
        mal_sine_wave_read(pSineWave, 1, &sample);

        for (mal_uint32 iChannel = 0; iChannel < pConverter->config.channels; iChannel += 1) {
            pFramesOutF32[iFrame*pConverter->config.channels + iChannel] = sample;
        }
    }

    return frameCount;
}

mal_uint32 converter_test_deinterleaved_callback(mal_format_converter* pConverter, mal_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    mal_sine_wave* pSineWave = (mal_sine_wave*)pUserData;
    mal_assert(pSineWave != NULL);

    mal_sine_wave_read(pSineWave, frameCount, (float*)ppSamplesOut[0]);

    // Copy everything from the first channel over the others.
    for (mal_uint32 iChannel = 1; iChannel < pConverter->config.channels; iChannel += 1) {
        mal_copy_memory(ppSamplesOut[iChannel], ppSamplesOut[0], frameCount * sizeof(float));
    }

    return frameCount;
}

int do_format_converter_tests()
{
    double amplitude = 1;
    double periodsPerSecond = 400;
    mal_uint32 sampleRate = 48000;

    mal_result result = MAL_SUCCESS;

    mal_sine_wave sineWave;
    mal_format_converter converter;

    mal_format_converter_config config;
    mal_zero_object(&config);
    config.formatIn = mal_format_f32;
    config.formatOut = mal_format_s16;
    config.channels = 2;
    config.streamFormatIn = mal_stream_format_pcm;
    config.streamFormatOut = mal_stream_format_pcm;
    config.ditherMode = mal_dither_mode_none;
    config.pUserData = &sineWave;


    config.onRead = converter_test_interleaved_callback;
    config.onReadDeinterleaved = NULL;

    // Interleaved/Interleaved f32 to s16.
    {
        mal_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = mal_format_converter_init(&config, &converter);
        if (result != MAL_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        mal_int16 interleavedFrames[MAL_MAX_CHANNELS * 1024];
        mal_uint64 framesRead = mal_format_converter_read(&converter, 1024, interleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        FILE* pFile = mal_fopen("res/output/converter_f32_to_s16_interleaved_interleaved__stereo_48000.raw", "wb");
        if (pFile == NULL) {
            printf("Failed to open output file.\n");
            return -1;
        }

        fwrite(interleavedFrames, sizeof(mal_int16), (size_t)framesRead * converter.config.channels, pFile);
        fclose(pFile);
    }

    // Interleaved/Deinterleaved f32 to s16.
    {
        mal_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = mal_format_converter_init(&config, &converter);
        if (result != MAL_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        mal_int16 deinterleavedFrames[MAL_MAX_CHANNELS][1024];
        void* ppDeinterleavedFrames[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            ppDeinterleavedFrames[iChannel] = &deinterleavedFrames[iChannel];
        }

        mal_uint64 framesRead = mal_format_converter_read_deinterleaved(&converter, 1024, ppDeinterleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        // Write a separate file for each channel.
        for (mal_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "res/output/converter_f32_to_s16_interleaved_deinterleaved__stereo_48000.raw.%d", iChannel);

            FILE* pFile = mal_fopen(filePath, "wb");
            if (pFile == NULL) {
                printf("Failed to open output file.\n");
                return -1;
            }

            fwrite(ppDeinterleavedFrames[iChannel], sizeof(mal_int16), (size_t)framesRead, pFile);
            fclose(pFile);
        }
    }


    config.onRead = NULL;
    config.onReadDeinterleaved = converter_test_deinterleaved_callback;

    // Deinterleaved/Interleaved f32 to s16.
    {
        mal_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = mal_format_converter_init(&config, &converter);
        if (result != MAL_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        mal_int16 interleavedFrames[MAL_MAX_CHANNELS * 1024];
        mal_uint64 framesRead = mal_format_converter_read(&converter, 1024, interleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        FILE* pFile = mal_fopen("res/output/converter_f32_to_s16_deinterleaved_interleaved__stereo_48000.raw", "wb");
        if (pFile == NULL) {
            printf("Failed to open output file.\n");
            return -1;
        }

        fwrite(interleavedFrames, sizeof(mal_int16), (size_t)framesRead * converter.config.channels, pFile);
        fclose(pFile);
    }

    // Deinterleaved/Deinterleaved f32 to s16.
    {
        mal_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = mal_format_converter_init(&config, &converter);
        if (result != MAL_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        mal_int16 deinterleavedFrames[MAL_MAX_CHANNELS][1024];
        void* ppDeinterleavedFrames[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            ppDeinterleavedFrames[iChannel] = &deinterleavedFrames[iChannel];
        }

        mal_uint64 framesRead = mal_format_converter_read_deinterleaved(&converter, 1024, ppDeinterleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        // Write a separate file for each channel.
        for (mal_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "res/output/converter_f32_to_s16_deinterleaved_deinterleaved__stereo_48000.raw.%d", iChannel);

            FILE* pFile = mal_fopen(filePath, "wb");
            if (pFile == NULL) {
                printf("Failed to open output file.\n");
                return -1;
            }

            fwrite(ppDeinterleavedFrames[iChannel], sizeof(mal_int16), (size_t)framesRead, pFile);
            fclose(pFile);
        }
    }


    config.onRead = converter_test_interleaved_callback;
    config.onReadDeinterleaved = NULL;
    config.formatOut = mal_format_f32;

    // Interleaved/Interleaved f32 to f32.
    {
        mal_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = mal_format_converter_init(&config, &converter);
        if (result != MAL_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        float interleavedFrames[MAL_MAX_CHANNELS * 1024];
        mal_uint64 framesRead = mal_format_converter_read(&converter, 1024, interleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        FILE* pFile = mal_fopen("res/output/converter_f32_to_f32_interleaved_interleaved__stereo_48000.raw", "wb");
        if (pFile == NULL) {
            printf("Failed to open output file.\n");
            return -1;
        }

        fwrite(interleavedFrames, sizeof(float), (size_t)framesRead * converter.config.channels, pFile);
        fclose(pFile);
    }

    // Interleaved/Deinterleaved f32 to f32.
    {
        mal_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = mal_format_converter_init(&config, &converter);
        if (result != MAL_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        float deinterleavedFrames[MAL_MAX_CHANNELS][1024];
        void* ppDeinterleavedFrames[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            ppDeinterleavedFrames[iChannel] = &deinterleavedFrames[iChannel];
        }

        mal_uint64 framesRead = mal_format_converter_read_deinterleaved(&converter, 1024, ppDeinterleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        // Write a separate file for each channel.
        for (mal_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "res/output/converter_f32_to_f32_interleaved_deinterleaved__stereo_48000.raw.%d", iChannel);

            FILE* pFile = mal_fopen(filePath, "wb");
            if (pFile == NULL) {
                printf("Failed to open output file.\n");
                return -1;
            }

            fwrite(ppDeinterleavedFrames[iChannel], sizeof(float), (size_t)framesRead, pFile);
            fclose(pFile);
        }
    }


    config.onRead = NULL;
    config.onReadDeinterleaved = converter_test_deinterleaved_callback;

    // Deinterleaved/Interleaved f32 to f32.
    {
        mal_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = mal_format_converter_init(&config, &converter);
        if (result != MAL_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        float interleavedFrames[MAL_MAX_CHANNELS * 1024];
        mal_uint64 framesRead = mal_format_converter_read(&converter, 1024, interleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        FILE* pFile = mal_fopen("res/output/converter_f32_to_f32_deinterleaved_interleaved__stereo_48000.raw", "wb");
        if (pFile == NULL) {
            printf("Failed to open output file.\n");
            return -1;
        }

        fwrite(interleavedFrames, sizeof(float), (size_t)framesRead * converter.config.channels, pFile);
        fclose(pFile);
    }

    // Deinterleaved/Deinterleaved f32 to f32.
    {
        mal_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = mal_format_converter_init(&config, &converter);
        if (result != MAL_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        float deinterleavedFrames[MAL_MAX_CHANNELS][1024];
        void* ppDeinterleavedFrames[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            ppDeinterleavedFrames[iChannel] = &deinterleavedFrames[iChannel];
        }

        mal_uint64 framesRead = mal_format_converter_read_deinterleaved(&converter, 1024, ppDeinterleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        // Write a separate file for each channel.
        for (mal_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "res/output/converter_f32_to_f32_deinterleaved_deinterleaved__stereo_48000.raw.%d", iChannel);

            FILE* pFile = mal_fopen(filePath, "wb");
            if (pFile == NULL) {
                printf("Failed to open output file.\n");
                return -1;
            }

            fwrite(ppDeinterleavedFrames[iChannel], sizeof(float), (size_t)framesRead, pFile);
            fclose(pFile);
        }
    }



    return 0;
}



mal_uint32 channel_router_callback__passthrough_test(mal_channel_router* pRouter, mal_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    float** ppSamplesIn = (float**)pUserData;

    for (mal_uint32 iChannel = 0; iChannel < pRouter->config.channelsIn; ++iChannel) {
        mal_copy_memory(ppSamplesOut[iChannel], ppSamplesIn[iChannel], frameCount*sizeof(float));
    }

    return frameCount;
}

int do_channel_routing_tests()
{
    mal_bool32 hasError = MAL_FALSE;

    printf("Passthrough... ");
    {
        mal_channel_router_config routerConfig;
        mal_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = mal_channel_mix_mode_planar_blend;
        routerConfig.channelsIn = 6;
        routerConfig.channelsOut = routerConfig.channelsIn;
        routerConfig.noSSE2 = MAL_TRUE;
        routerConfig.noAVX = MAL_TRUE;
        routerConfig.noAVX512 = MAL_TRUE;
        routerConfig.noNEON = MAL_TRUE;
        mal_get_standard_channel_map(mal_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        mal_get_standard_channel_map(mal_standard_channel_map_microsoft, routerConfig.channelsOut, routerConfig.channelMapOut);
        
        mal_channel_router router;
        mal_result result = mal_channel_router_init(&routerConfig, &router);
        if (result == MAL_SUCCESS) {
            if (!router.isPassthrough) {
                printf("Failed to init router as passthrough.\n");
                hasError = MAL_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (mal_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0;
                    if (iChannelIn == iChannelOut) {
                        expectedWeight = 1;
                    }

                    if (router.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MAL_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MAL_TRUE;
        }


        // Here is where we check that the passthrough optimization works correctly. What we do is compare the output of the passthrough
        // optimization with the non-passthrough output. We don't use a real sound here, but instead use values that makes it easier for
        // us to check results. Each channel is given a value equal to it's index, plus 1.
        float testData[MAL_MAX_CHANNELS][MAL_SIMD_ALIGNMENT * 2];
        float* ppTestData[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            ppTestData[iChannel] = testData[iChannel];
            for (mal_uint32 iFrame = 0; iFrame < mal_countof(testData[0]); ++iFrame) {
                ppTestData[iChannel][iFrame] = (float)(iChannel + 1);
            }
        }

        routerConfig.pUserData = ppTestData;
        mal_channel_router_init(&routerConfig, &router);

        MAL_ALIGN(MAL_SIMD_ALIGNMENT) float outputA[MAL_MAX_CHANNELS][MAL_SIMD_ALIGNMENT * 2];
        MAL_ALIGN(MAL_SIMD_ALIGNMENT) float outputB[MAL_MAX_CHANNELS][MAL_SIMD_ALIGNMENT * 2];
        float* ppOutputA[MAL_MAX_CHANNELS];
        float* ppOutputB[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            ppOutputA[iChannel] = outputA[iChannel];
            ppOutputB[iChannel] = outputB[iChannel];
        }

        // With optimizations.
        mal_uint64 framesRead = mal_channel_router_read_deinterleaved(&router, mal_countof(outputA[0]), (void**)ppOutputA, router.config.pUserData);
        if (framesRead != mal_countof(outputA[0])) {
            printf("Returned frame count for optimized incorrect.");
            hasError = MAL_TRUE;
        }

        // Without optimizations.
        router.isPassthrough = MAL_FALSE;
        router.isSimpleShuffle = MAL_FALSE;
        framesRead = mal_channel_router_read_deinterleaved(&router, mal_countof(outputA[0]), (void**)ppOutputB, router.config.pUserData);
        if (framesRead != mal_countof(outputA[0])) {
            printf("Returned frame count for unoptimized path incorrect.");
            hasError = MAL_TRUE;
        }

        // Compare.
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            for (mal_uint32 iFrame = 0; iFrame < mal_countof(outputA[0]); ++iFrame) {
                if (ppOutputA[iChannel][iFrame] != ppOutputB[iChannel][iFrame]) {
                    printf("Sample incorrect [%d][%d]\n", iChannel, iFrame);
                    hasError = MAL_TRUE;
                }
            }
        }


        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Shuffle... ");
    {
        // The shuffle is tested by simply reversing the order of the channels. Doing a reversal just makes it easier to
        // check that everything is working.

        mal_channel_router_config routerConfig;
        mal_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = mal_channel_mix_mode_planar_blend;
        routerConfig.channelsIn = 6;
        routerConfig.channelsOut = routerConfig.channelsIn;
        routerConfig.noSSE2 = MAL_TRUE;
        routerConfig.noAVX = MAL_TRUE;
        routerConfig.noAVX512 = MAL_TRUE;
        routerConfig.noNEON = MAL_TRUE;
        mal_get_standard_channel_map(mal_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            routerConfig.channelMapOut[iChannel] = routerConfig.channelMapIn[routerConfig.channelsIn - iChannel - 1];
        }

        mal_channel_router router;
        mal_result result = mal_channel_router_init(&routerConfig, &router);
        if (result == MAL_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MAL_TRUE;
            }
            if (!router.isSimpleShuffle) {
                printf("Router not configured as a simple shuffle.\n");
                hasError = MAL_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (mal_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0;
                    if (iChannelIn == (routerConfig.channelsOut - iChannelOut - 1)) {
                        expectedWeight = 1;
                    }

                    if (router.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MAL_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MAL_TRUE;
        }


        // Here is where we check that the shuffle optimization works correctly. What we do is compare the output of the shuffle
        // optimization with the non-shuffle output. We don't use a real sound here, but instead use values that makes it easier
        // for us to check results. Each channel is given a value equal to it's index, plus 1.
        float testData[MAL_MAX_CHANNELS][100];
        float* ppTestData[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            ppTestData[iChannel] = testData[iChannel];
            for (mal_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
                ppTestData[iChannel][iFrame] = (float)(iChannel + 1);
            }
        }

        routerConfig.pUserData = ppTestData;
        mal_channel_router_init(&routerConfig, &router);

        float outputA[MAL_MAX_CHANNELS][100];
        float outputB[MAL_MAX_CHANNELS][100];
        float* ppOutputA[MAL_MAX_CHANNELS];
        float* ppOutputB[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            ppOutputA[iChannel] = outputA[iChannel];
            ppOutputB[iChannel] = outputB[iChannel];
        }

        // With optimizations.
        mal_uint64 framesRead = mal_channel_router_read_deinterleaved(&router, 100, (void**)ppOutputA, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for optimized incorrect.");
            hasError = MAL_TRUE;
        }

        // Without optimizations.
        router.isPassthrough = MAL_FALSE;
        router.isSimpleShuffle = MAL_FALSE;
        framesRead = mal_channel_router_read_deinterleaved(&router, 100, (void**)ppOutputB, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for unoptimized path incorrect.");
            hasError = MAL_TRUE;
        }

        // Compare.
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            for (mal_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
                if (ppOutputA[iChannel][iFrame] != ppOutputB[iChannel][iFrame]) {
                    printf("Sample incorrect [%d][%d]\n", iChannel, iFrame);
                    hasError = MAL_TRUE;
                }
            }
        }


        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Simple Conversion (Stereo -> 5.1)... ");
    {
        // This tests takes a Stereo to 5.1 conversion using the simple mixing mode. We should expect 0 and 1 (front/left, front/right) to have
        // weights of 1, and the others to have a weight of 0.

        mal_channel_router_config routerConfig;
        mal_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = mal_channel_mix_mode_simple;
        routerConfig.channelsIn = 2;
        routerConfig.channelsOut = 6;
        routerConfig.noSSE2 = MAL_TRUE;
        routerConfig.noAVX = MAL_TRUE;
        routerConfig.noAVX512 = MAL_TRUE;
        routerConfig.noNEON = MAL_TRUE;
        mal_get_standard_channel_map(mal_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        mal_get_standard_channel_map(mal_standard_channel_map_microsoft, routerConfig.channelsOut, routerConfig.channelMapOut);

        mal_channel_router router;
        mal_result result = mal_channel_router_init(&routerConfig, &router);
        if (result == MAL_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MAL_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MAL_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (mal_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0;
                    if (routerConfig.channelMapIn[iChannelIn] == routerConfig.channelMapOut[iChannelOut]) {
                        expectedWeight = 1;
                    }

                    if (router.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MAL_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MAL_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Simple Conversion (5.1 -> Stereo)... ");
    {
        mal_channel_router_config routerConfig;
        mal_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = mal_channel_mix_mode_simple;
        routerConfig.channelsIn = 6;
        routerConfig.channelsOut = 2;
        routerConfig.noSSE2 = MAL_TRUE;
        routerConfig.noAVX = MAL_TRUE;
        routerConfig.noAVX512 = MAL_TRUE;
        routerConfig.noNEON = MAL_TRUE;
        mal_get_standard_channel_map(mal_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        mal_get_standard_channel_map(mal_standard_channel_map_microsoft, routerConfig.channelsOut, routerConfig.channelMapOut);

        mal_channel_router router;
        mal_result result = mal_channel_router_init(&routerConfig, &router);
        if (result == MAL_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MAL_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MAL_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (mal_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0;
                    if (routerConfig.channelMapIn[iChannelIn] == routerConfig.channelMapOut[iChannelOut]) {
                        expectedWeight = 1;
                    }

                    if (router.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MAL_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MAL_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Planar Blend Conversion (Stereo -> 5.1)... ");
    {
        mal_channel_router_config routerConfig;
        mal_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = mal_channel_mix_mode_planar_blend;
        routerConfig.noSSE2 = MAL_TRUE;
        routerConfig.noAVX = MAL_TRUE;
        routerConfig.noAVX512 = MAL_TRUE;
        routerConfig.noNEON = MAL_TRUE;
        
        // Use very specific mappings for this test.
        routerConfig.channelsIn = 2;
        routerConfig.channelMapIn[0] = MAL_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapIn[1] = MAL_CHANNEL_FRONT_RIGHT;

        routerConfig.channelsOut = 8;
        routerConfig.channelMapOut[0] = MAL_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapOut[1] = MAL_CHANNEL_FRONT_RIGHT;
        routerConfig.channelMapOut[2] = MAL_CHANNEL_FRONT_CENTER;
        routerConfig.channelMapOut[3] = MAL_CHANNEL_LFE;
        routerConfig.channelMapOut[4] = MAL_CHANNEL_BACK_LEFT;
        routerConfig.channelMapOut[5] = MAL_CHANNEL_BACK_RIGHT;
        routerConfig.channelMapOut[6] = MAL_CHANNEL_SIDE_LEFT;
        routerConfig.channelMapOut[7] = MAL_CHANNEL_SIDE_RIGHT;

        mal_channel_router router;
        mal_result result = mal_channel_router_init(&routerConfig, &router);
        if (result == MAL_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MAL_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MAL_TRUE;
            }
            
            float expectedWeights[MAL_MAX_CHANNELS][MAL_MAX_CHANNELS];
            mal_zero_memory(expectedWeights, sizeof(expectedWeights));
            expectedWeights[0][0] = 1.0f;   // FRONT_LEFT  -> FRONT_LEFT
            expectedWeights[0][1] = 0.0f;   // FRONT_LEFT  -> FRONT_RIGHT
            expectedWeights[0][2] = 0.5f;   // FRONT_LEFT  -> FRONT_CENTER
            expectedWeights[0][3] = 0.0f;   // FRONT_LEFT  -> LFE
            expectedWeights[0][4] = 0.25f;  // FRONT_LEFT  -> BACK_LEFT
            expectedWeights[0][5] = 0.0f;   // FRONT_LEFT  -> BACK_RIGHT
            expectedWeights[0][6] = 0.5f;   // FRONT_LEFT  -> SIDE_LEFT
            expectedWeights[0][7] = 0.0f;   // FRONT_LEFT  -> SIDE_RIGHT
            expectedWeights[1][0] = 0.0f;   // FRONT_RIGHT -> FRONT_LEFT
            expectedWeights[1][1] = 1.0f;   // FRONT_RIGHT -> FRONT_RIGHT
            expectedWeights[1][2] = 0.5f;   // FRONT_RIGHT -> FRONT_CENTER
            expectedWeights[1][3] = 0.0f;   // FRONT_RIGHT -> LFE
            expectedWeights[1][4] = 0.0f;   // FRONT_RIGHT -> BACK_LEFT
            expectedWeights[1][5] = 0.25f;  // FRONT_RIGHT -> BACK_RIGHT
            expectedWeights[1][6] = 0.0f;   // FRONT_RIGHT -> SIDE_LEFT
            expectedWeights[1][7] = 0.5f;   // FRONT_RIGHT -> SIDE_RIGHT

            for (mal_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    if (router.weights[iChannelIn][iChannelOut] != expectedWeights[iChannelIn][iChannelOut]) {
                        printf("Failed. Channel weight incorrect for [%d][%d]. Expected %f, got %f\n", iChannelIn, iChannelOut, expectedWeights[iChannelIn][iChannelOut], router.weights[iChannelIn][iChannelOut]);
                        hasError = MAL_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MAL_TRUE;
        }


        // Test the actual conversion. The test data is set to +1 for the left channel, and -1 for the right channel.
        float testData[MAL_MAX_CHANNELS][100];
        float* ppTestData[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            ppTestData[iChannel] = testData[iChannel];
        }

        for (mal_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
            ppTestData[0][iFrame] = -1;
            ppTestData[1][iFrame] = +1;
        }

        routerConfig.pUserData = ppTestData;
        mal_channel_router_init(&routerConfig, &router);

        float output[MAL_MAX_CHANNELS][100];
        float* ppOutput[MAL_MAX_CHANNELS];
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            ppOutput[iChannel] = output[iChannel];
        }

        mal_uint64 framesRead = mal_channel_router_read_deinterleaved(&router, 100, (void**)ppOutput, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for optimized incorrect.\n");
            hasError = MAL_TRUE;
        }

        float expectedOutput[MAL_MAX_CHANNELS];
        expectedOutput[0] = -1.0f;  // FRONT_LEFT
        expectedOutput[1] = +1.0f;  // FRONT_RIGHT
        expectedOutput[2] =  0.0f;  // FRONT_CENTER (left and right should cancel out, totalling 0).
        expectedOutput[3] =  0.0f;  // LFE
        expectedOutput[4] = -0.25f; // BACK_LEFT
        expectedOutput[5] = +0.25f; // BACK_RIGHT
        expectedOutput[6] = -0.5f;  // SIDE_LEFT
        expectedOutput[7] = +0.5f;  // SIDE_RIGHT
        for (mal_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            for (mal_uint32 iFrame = 0; iFrame < framesRead; ++iFrame) {
                if (output[iChannel][iFrame] != expectedOutput[iChannel]) {
                    printf("Incorrect sample [%d][%d]. Expecting %f, got %f\n", iChannel, iFrame, expectedOutput[iChannel], output[iChannel][iFrame]);
                    hasError = MAL_TRUE;
                }
            }
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Planar Blend Conversion (5.1 -> Stereo)... ");
    {
        mal_channel_router_config routerConfig;
        mal_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = mal_channel_mix_mode_planar_blend;
        routerConfig.noSSE2 = MAL_TRUE;
        routerConfig.noAVX = MAL_TRUE;
        routerConfig.noAVX512 = MAL_TRUE;
        routerConfig.noNEON = MAL_TRUE;
        
        // Use very specific mappings for this test.
        routerConfig.channelsIn = 8;
        routerConfig.channelMapIn[0] = MAL_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapIn[1] = MAL_CHANNEL_FRONT_RIGHT;
        routerConfig.channelMapIn[2] = MAL_CHANNEL_FRONT_CENTER;
        routerConfig.channelMapIn[3] = MAL_CHANNEL_LFE;
        routerConfig.channelMapIn[4] = MAL_CHANNEL_BACK_LEFT;
        routerConfig.channelMapIn[5] = MAL_CHANNEL_BACK_RIGHT;
        routerConfig.channelMapIn[6] = MAL_CHANNEL_SIDE_LEFT;
        routerConfig.channelMapIn[7] = MAL_CHANNEL_SIDE_RIGHT;

        routerConfig.channelsOut = 2;
        routerConfig.channelMapOut[0] = MAL_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapOut[1] = MAL_CHANNEL_FRONT_RIGHT;

        mal_channel_router router;
        mal_result result = mal_channel_router_init(&routerConfig, &router);
        if (result == MAL_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MAL_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MAL_TRUE;
            }
            
            float expectedWeights[MAL_MAX_CHANNELS][MAL_MAX_CHANNELS];
            mal_zero_memory(expectedWeights, sizeof(expectedWeights));
            expectedWeights[0][0] = 1.0f;   // FRONT_LEFT   -> FRONT_LEFT
            expectedWeights[1][0] = 0.0f;   // FRONT_RIGHT  -> FRONT_LEFT
            expectedWeights[2][0] = 0.5f;   // FRONT_CENTER -> FRONT_LEFT
            expectedWeights[3][0] = 0.0f;   // LFE          -> FRONT_LEFT
            expectedWeights[4][0] = 0.25f;  // BACK_LEFT    -> FRONT_LEFT
            expectedWeights[5][0] = 0.0f;   // BACK_RIGHT   -> FRONT_LEFT
            expectedWeights[6][0] = 0.5f;   // SIDE_LEFT    -> FRONT_LEFT
            expectedWeights[7][0] = 0.0f;   // SIDE_RIGHT   -> FRONT_LEFT
            expectedWeights[0][1] = 0.0f;   // FRONT_LEFT   -> FRONT_RIGHT
            expectedWeights[1][1] = 1.0f;   // FRONT_RIGHT  -> FRONT_RIGHT
            expectedWeights[2][1] = 0.5f;   // FRONT_CENTER -> FRONT_RIGHT
            expectedWeights[3][1] = 0.0f;   // LFE          -> FRONT_RIGHT
            expectedWeights[4][1] = 0.0f;   // BACK_LEFT    -> FRONT_RIGHT
            expectedWeights[5][1] = 0.25f;  // BACK_RIGHT   -> FRONT_RIGHT
            expectedWeights[6][1] = 0.0f;   // SIDE_LEFT    -> FRONT_RIGHT
            expectedWeights[7][1] = 0.5f;   // SIDE_RIGHT   -> FRONT_RIGHT

            for (mal_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    if (router.weights[iChannelIn][iChannelOut] != expectedWeights[iChannelIn][iChannelOut]) {
                        printf("Failed. Channel weight incorrect for [%d][%d]. Expected %f, got %f\n", iChannelIn, iChannelOut, expectedWeights[iChannelIn][iChannelOut], router.weights[iChannelIn][iChannelOut]);
                        hasError = MAL_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MAL_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Mono -> 2.1 + None... ");
    {
        mal_channel_router_config routerConfig;
        mal_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = mal_channel_mix_mode_planar_blend;
        routerConfig.noSSE2 = MAL_TRUE;
        routerConfig.noAVX = MAL_TRUE;
        routerConfig.noAVX512 = MAL_TRUE;
        routerConfig.noNEON = MAL_TRUE;
        
        // Use very specific mappings for this test.
        routerConfig.channelsIn = 1;
        routerConfig.channelMapIn[0] = MAL_CHANNEL_MONO;

        routerConfig.channelsOut = 4;
        routerConfig.channelMapOut[0] = MAL_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapOut[1] = MAL_CHANNEL_FRONT_RIGHT;
        routerConfig.channelMapOut[2] = MAL_CHANNEL_NONE;
        routerConfig.channelMapOut[3] = MAL_CHANNEL_LFE;

        mal_channel_router router;
        mal_result result = mal_channel_router_init(&routerConfig, &router);
        if (result == MAL_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MAL_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MAL_TRUE;
            }
            
            float expectedWeights[MAL_MAX_CHANNELS][MAL_MAX_CHANNELS];
            mal_zero_memory(expectedWeights, sizeof(expectedWeights));
            expectedWeights[0][0] = 1.0f;   // MONO -> FRONT_LEFT
            expectedWeights[0][1] = 1.0f;   // MONO -> FRONT_RIGHT
            expectedWeights[0][2] = 0.0f;   // MONO -> NONE
            expectedWeights[0][3] = 0.0f;   // MONO -> LFE

            for (mal_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    if (router.weights[iChannelIn][iChannelOut] != expectedWeights[iChannelIn][iChannelOut]) {
                        printf("Failed. Channel weight incorrect for [%d][%d]. Expected %f, got %f\n", iChannelIn, iChannelOut, expectedWeights[iChannelIn][iChannelOut], router.weights[iChannelIn][iChannelOut]);
                        hasError = MAL_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MAL_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("2.1 + None -> Mono... ");
    {
        mal_channel_router_config routerConfig;
        mal_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = mal_channel_mix_mode_planar_blend;
        routerConfig.noSSE2 = MAL_TRUE;
        routerConfig.noAVX = MAL_TRUE;
        routerConfig.noAVX512 = MAL_TRUE;
        routerConfig.noNEON = MAL_TRUE;
        
        // Use very specific mappings for this test.
        routerConfig.channelsIn = 4;
        routerConfig.channelMapIn[0] = MAL_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapIn[1] = MAL_CHANNEL_FRONT_RIGHT;
        routerConfig.channelMapIn[2] = MAL_CHANNEL_NONE;
        routerConfig.channelMapIn[3] = MAL_CHANNEL_LFE;

        routerConfig.channelsOut = 1;
        routerConfig.channelMapOut[0] = MAL_CHANNEL_MONO;

        mal_channel_router router;
        mal_result result = mal_channel_router_init(&routerConfig, &router);
        if (result == MAL_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MAL_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MAL_TRUE;
            }
            
            float expectedWeights[MAL_MAX_CHANNELS][MAL_MAX_CHANNELS];
            mal_zero_memory(expectedWeights, sizeof(expectedWeights));
            expectedWeights[0][0] = 0.5f;   // FRONT_LEFT  -> MONO
            expectedWeights[1][0] = 0.5f;   // FRONT_RIGHT -> MONO
            expectedWeights[2][0] = 0.0f;   // NONE        -> MONO
            expectedWeights[3][0] = 0.0f;   // LFE         -> MONO

            for (mal_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    if (router.weights[iChannelIn][iChannelOut] != expectedWeights[iChannelIn][iChannelOut]) {
                        printf("Failed. Channel weight incorrect for [%d][%d]. Expected %f, got %f\n", iChannelIn, iChannelOut, expectedWeights[iChannelIn][iChannelOut], router.weights[iChannelIn][iChannelOut]);
                        hasError = MAL_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MAL_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }


    if (hasError) {
        return -1;
    } else {
        return 0;
    }
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
                printf("        Name:            %s\n", pPlaybackDeviceInfos[iDevice].name);
                printf("        Min Channels:    %d\n", pPlaybackDeviceInfos[iDevice].minChannels);
                printf("        Max Channels:    %d\n", pPlaybackDeviceInfos[iDevice].maxChannels);
                printf("        Min Sample Rate: %d\n", pPlaybackDeviceInfos[iDevice].minSampleRate);
                printf("        Max Sample Rate: %d\n", pPlaybackDeviceInfos[iDevice].maxSampleRate);
                printf("        Format Count:    %d\n", pPlaybackDeviceInfos[iDevice].formatCount);
                for (mal_uint32 iFormat = 0; iFormat < pPlaybackDeviceInfos[iDevice].formatCount; ++iFormat) {
                    printf("          %s\n", mal_get_format_name(pPlaybackDeviceInfos[iDevice].formats[iFormat]));
                }
            } else {
                printf("        ERROR\n");
            }
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
            
            result = mal_context_get_device_info(&context, mal_device_type_capture, &pCaptureDeviceInfos[iDevice].id, mal_share_mode_shared, &pCaptureDeviceInfos[iDevice]);
            if (result == MAL_SUCCESS) {
                printf("        Name:            %s\n", pCaptureDeviceInfos[iDevice].name);
                printf("        Min Channels:    %d\n", pCaptureDeviceInfos[iDevice].minChannels);
                printf("        Max Channels:    %d\n", pCaptureDeviceInfos[iDevice].maxChannels);
                printf("        Min Sample Rate: %d\n", pCaptureDeviceInfos[iDevice].minSampleRate);
                printf("        Max Sample Rate: %d\n", pCaptureDeviceInfos[iDevice].maxSampleRate);
                printf("        Format Count:    %d\n", pCaptureDeviceInfos[iDevice].formatCount);
                for (mal_uint32 iFormat = 0; iFormat < pCaptureDeviceInfos[iDevice].formatCount; ++iFormat) {
                    printf("          %s\n", mal_get_format_name(pCaptureDeviceInfos[iDevice].formats[iFormat]));
                }
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
    mal_sine_wave* pSineWave;
    mal_event endOfPlaybackEvent;
} playback_test_callback_data;

mal_uint32 on_send__playback_test(mal_device* pDevice, mal_uint32 frameCount, void* pFrames)
{
    playback_test_callback_data* pData = (playback_test_callback_data*)pDevice->pUserData;
    mal_assert(pData != NULL);

#if !defined(__EMSCRIPTEN__)
    mal_uint64 framesRead = mal_decoder_read(pData->pDecoder, frameCount, pFrames);
    if (framesRead == 0) {
        mal_event_signal(&pData->endOfPlaybackEvent);
    }

    return (mal_uint32)framesRead;
#else
    if (pDevice->format == mal_format_f32) {
        for (mal_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
            float sample;
            mal_sine_wave_read(pData->pSineWave, 1, &sample);

            for (mal_uint32 iChannel = 0; iChannel < pDevice->channels; ++iChannel) {
                ((float*)pFrames)[iFrame*pDevice->channels + iChannel] = sample;
            }
        }

        return frameCount;
    } else {
        return 0;
    }
#endif
}

int do_playback_test(mal_backend backend)
{
    mal_result result = MAL_SUCCESS;
    mal_device device;
    mal_decoder decoder;
    mal_sine_wave sineWave;
    mal_bool32 haveDevice = MAL_FALSE;
    mal_bool32 haveDecoder = MAL_FALSE;

    playback_test_callback_data callbackData;
    callbackData.pDecoder = &decoder;
    callbackData.pSineWave = &sineWave;

    printf("--- %s ---\n", mal_get_backend_name(backend));

    // Device.
    printf("  Opening Device... ");
    {
        mal_context_config contextConfig = mal_context_config_init(on_log);
        mal_device_config deviceConfig = mal_device_config_init_default_playback(on_send__playback_test);

    #if defined(__EMSCRIPTEN__)
        deviceConfig.format = mal_format_f32;
    #endif

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
        
#if !defined(__EMSCRIPTEN__)
        mal_decoder_config decoderConfig = mal_decoder_config_init(device.format, device.channels, device.sampleRate);
        result = mal_decoder_init_file("res/sine_s16_mono_48000.wav", &decoderConfig, &decoder);
        if (result == MAL_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed to init decoder.\n");
            goto done;
        }
        haveDecoder = MAL_TRUE;
#else
        result = mal_sine_wave_init(0.5f, 400, device.sampleRate, &sineWave);
        if (result == MAL_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed to init sine wave.\n");
            goto done;
        }
#endif

        
    }

    printf("  Press Enter to start playback... ");
    getchar();
    {
        result = mal_device_start(&device);
        if (result != MAL_SUCCESS) {
            printf("Failed to start device.\n");
            goto done;
        }

#if defined(__EMSCRIPTEN__)
        emscripten_set_main_loop(main_loop__em, 0, 1);
#endif

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

    // Print the compiler.
#if defined(_MSC_VER) && !defined(__clang__)
    printf("Compiler:     VC++\n");
#endif
#if defined(__GNUC__) && !defined(__clang__)
    printf("Compiler:     GCC\n");
#endif
#if defined(__clang__)
    printf("Compiler:     Clang\n");
#endif
#if defined(__TINYC__)
    printf("Compiler:     TCC\n");
#endif

    // Print CPU features.
    if (mal_has_sse2()) {
        printf("Has SSE:      YES\n");
    } else {
        printf("Has SSE:      NO\n");
    }
    if (mal_has_avx()) {
        printf("Has AVX:      YES\n");
    } else {
        printf("Has AVX:      NO\n");
    }
    if (mal_has_avx512f()) {
        printf("Has AVX-512F: YES\n");
    } else {
        printf("Has AVX-512F: NO\n");
    }
    if (mal_has_neon()) {
        printf("Has NEON:     YES\n");
    } else {
        printf("Has NEON:     NO\n");
    }

    // Aligned malloc/free
    printf("=== TESTING CORE ===\n");
    result = do_core_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING CORE ===\n");
    
    printf("\n");

    // Format Conversion
    printf("=== TESTING FORMAT CONVERSION ===\n");
    result = do_format_conversion_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING FORMAT CONVERSION ===\n");
    
    printf("\n");

    // Interleaving / Deinterleaving
    printf("=== TESTING INTERLEAVING/DEINTERLEAVING ===\n");
    result = do_interleaving_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING INTERLEAVING/DEINTERLEAVING ===\n");

    printf("\n");

    // mal_format_converter
    printf("=== TESTING FORMAT CONVERTER ===\n");
    result = do_format_converter_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING FORMAT CONVERTER ===\n");
    
    printf("\n");

    // Channel Routing
    printf("=== TESTING CHANNEL ROUTING ===\n");
    result = do_channel_routing_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING CHANNEL ROUTING ===\n");
    
    printf("\n");
    
    // Backends
    printf("=== TESTING BACKENDS ===\n");
    result = do_backend_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING BACKENDS ===\n");

    printf("\n");

    // Default Playback Devices
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