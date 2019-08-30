// Uncomment this to include Vorbis decoding tests, albeit with some annoying warnings with MinGW.
//#define MA_INCLUDE_VORBIS_TESTS

#include "../extras/dr_flac.h"
#include "../extras/dr_mp3.h"
#include "../extras/dr_wav.h"

#ifdef MA_INCLUDE_VORBIS_TESTS
    #define STB_VORBIS_HEADER_ONLY
    #include "../extras/stb_vorbis.c"
#endif

//#define MA_DEBUG_OUTPUT
#define MA_LOG_LEVEL MA_LOG_LEVEL_VERBOSE
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
void main_loop__em()
{
}
#endif


ma_backend g_Backends[] = {
    ma_backend_wasapi,
    ma_backend_dsound,
    ma_backend_winmm,
    ma_backend_coreaudio,
    ma_backend_sndio,
    ma_backend_audio4,
    ma_backend_oss,
    ma_backend_pulseaudio,
    ma_backend_alsa,
    ma_backend_jack,
    ma_backend_aaudio,
    ma_backend_opensl,
    ma_backend_webaudio,
    ma_backend_null
};

void on_log(ma_context* pContext, ma_device* pDevice, ma_uint32 logLevel, const char* message)
{
    (void)pContext;
    (void)pDevice;
    (void)logLevel;
    printf("%s\n", message);
}

void on_stop(ma_device* pDevice)
{
    (void)pDevice;
    printf("Device Stopped.\n");
}

FILE* ma_fopen(const char* filePath, const char* openMode)
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

    FILE* pFile = ma_fopen(filePath, "rb");
    if (pFile == NULL) {
        return NULL;
    }

    fseek(pFile, 0, SEEK_END);
    ma_uint64 fileSize = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    if (fileSize > MA_SIZE_MAX) {
        fclose(pFile);
        return NULL;
    }

    void* pFileData = ma_malloc((size_t)fileSize);    // <-- Safe cast due to the check above.
    if (pFileData == NULL) {
        fclose(pFile);
        return NULL;
    }

    size_t bytesRead = fread(pFileData, 1, (size_t)fileSize, pFile);
    if (bytesRead != fileSize) {
        ma_free(pFileData);
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

    int sizeof_int8 = sizeof(ma_int8);
    int sizeof_uint8 = sizeof(ma_uint8);
    int sizeof_int16 = sizeof(ma_int16);
    int sizeof_uint16 = sizeof(ma_uint16);
    int sizeof_int32 = sizeof(ma_int32);
    int sizeof_uint32 = sizeof(ma_uint32);
    int sizeof_int64 = sizeof(ma_int64);
    int sizeof_uint64 = sizeof(ma_uint64);
    int sizeof_float32 = sizeof(float);
    int sizeof_float64 = sizeof(double);
    int sizeof_uintptr = sizeof(ma_uintptr);

    printf("sizeof(ma_int8)    1 = %d", sizeof_int8);
    if (sizeof_int8 != 1) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(ma_uint8)   1 = %d", sizeof_uint8);
    if (sizeof_uint8 != 1) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    printf("sizeof(ma_int16)   2 = %d", sizeof_int16);
    if (sizeof_int16 != 2) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(ma_uint16)  2 = %d", sizeof_uint16);
    if (sizeof_uint16 != 2) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    printf("sizeof(ma_int32)   4 = %d", sizeof_int32);
    if (sizeof_int32 != 4) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(ma_uint32)  4 = %d", sizeof_uint32);
    if (sizeof_uint32 != 4) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }

    printf("sizeof(ma_int64)   8 = %d", sizeof_int64);
    if (sizeof_int64 != 8) {
        printf(" - FAILED\n");
        result = -1;
    } else {
        printf(" - PASSED\n");
    }
    printf("sizeof(ma_uint64)  8 = %d", sizeof_uint64);
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

    printf("sizeof(ma_uintptr) %d = %d", (int)sizeof(void*), sizeof_uintptr);
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
    for (ma_uint32 i = 0; i < ma_countof(p); ++i) {
        ma_uintptr alignment = MA_SIMD_ALIGNMENT;

        p[i] = ma_aligned_malloc(1024, alignment);
        if (((ma_uintptr)p[i] & (alignment-1)) != 0) {
            printf("FAILED\n");
            result = -1;
        }
    }

    // Free.
    for (ma_uint32 i = 0; i < ma_countof(p); ++i) {
        ma_aligned_free(p[i]);
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


void* load_raw_audio_data(const char* filePath, ma_format format, ma_uint64* pBenchmarkFrameCount)
{
    ma_assert(pBenchmarkFrameCount != NULL);
    *pBenchmarkFrameCount = 0;

    size_t fileSize;
    void* pFileData = open_and_read_file_data(filePath, &fileSize);
    if (pFileData == NULL) {
        printf("Could not open file %s\n", filePath);
        return NULL;
    }

    *pBenchmarkFrameCount = fileSize / ma_get_bytes_per_sample(format);
    return pFileData;
}

void* load_benchmark_base_data(ma_format format, ma_uint32* pChannelsOut, ma_uint32* pSampleRateOut, ma_uint64* pBenchmarkFrameCount)
{
    ma_assert(pChannelsOut != NULL);
    ma_assert(pSampleRateOut != NULL);
    ma_assert(pBenchmarkFrameCount != NULL);

    *pChannelsOut = 1;
    *pSampleRateOut = 8000;
    *pBenchmarkFrameCount = 0;

    const char* filePath = NULL;
    switch (format) {
        case ma_format_u8:  filePath = "res/benchmarks/pcm_u8_to_u8__mono_8000.raw";   break;
        case ma_format_s16: filePath = "res/benchmarks/pcm_s16_to_s16__mono_8000.raw"; break;
        case ma_format_s24: filePath = "res/benchmarks/pcm_s24_to_s24__mono_8000.raw"; break;
        case ma_format_s32: filePath = "res/benchmarks/pcm_s32_to_s32__mono_8000.raw"; break;
        case ma_format_f32: filePath = "res/benchmarks/pcm_f32_to_f32__mono_8000.raw"; break;
        default: return NULL;
    }

    return load_raw_audio_data(filePath, format, pBenchmarkFrameCount);
}

int ma_pcm_compare(const void* a, const void* b, ma_uint64 count, ma_format format, float allowedDifference)
{
    int result = 0;

    const ma_uint8* a_u8  = (const ma_uint8*)a;
    const ma_uint8* b_u8  = (const ma_uint8*)b;
    const ma_int16* a_s16 = (const ma_int16*)a;
    const ma_int16* b_s16 = (const ma_int16*)b;
    const ma_int32* a_s32 = (const ma_int32*)a;
    const ma_int32* b_s32 = (const ma_int32*)b;
    const float*     a_f32 = (const float*    )a;
    const float*     b_f32 = (const float*    )b;

    for (ma_uint64 i = 0; i < count; ++i) {
        switch (format) {
            case ma_format_u8:
            {
                ma_uint8 sampleA = a_u8[i];
                ma_uint8 sampleB = b_u8[i];
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %u not equal. %d != %d (diff: %d)\n", (ma_int32)i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case ma_format_s16:
            {
                ma_int16 sampleA = a_s16[i];
                ma_int16 sampleB = b_s16[i];
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %u not equal. %d != %d (diff: %d)\n", (ma_int32)i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case ma_format_s24:
            {
                ma_int32 sampleA = ((ma_int32)(((ma_uint32)(a_u8[i*3+0]) << 8) | ((ma_uint32)(a_u8[i*3+1]) << 16) | ((ma_uint32)(a_u8[i*3+2])) << 24)) >> 8;
                ma_int32 sampleB = ((ma_int32)(((ma_uint32)(b_u8[i*3+0]) << 8) | ((ma_uint32)(b_u8[i*3+1]) << 16) | ((ma_uint32)(b_u8[i*3+2])) << 24)) >> 8;
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %u not equal. %d != %d (diff: %d)\n", (ma_int32)i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case ma_format_s32:
            {
                ma_int32 sampleA = a_s32[i];
                ma_int32 sampleB = b_s32[i];
                if (sampleA != sampleB) {
                    if (abs(sampleA - sampleB) > allowedDifference) {   // Allow a difference of 1.
                        printf("Sample %u not equal. %d != %d (diff: %d)\n", (ma_int32)i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            case ma_format_f32:
            {
                float sampleA = a_f32[i];
                float sampleB = b_f32[i];
                if (sampleA != sampleB) {
                    float difference = sampleA - sampleB;
                    difference = (difference < 0) ? -difference : difference;

                    if (difference > allowedDifference) {
                        printf("Sample %u not equal. %.8f != %.8f (diff: %.8f)\n", (ma_int32)i, sampleA, sampleB, sampleA - sampleB);
                        result = -1;
                    }
                }
            } break;

            default: return -1;
        }
    }

    return result;
}

int do_format_conversion_test(ma_format formatIn, ma_format formatOut)
{
    int result = 0;

    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint64 baseFrameCount;
    ma_int16* pBaseData = (ma_int16*)load_benchmark_base_data(formatIn, &channels, &sampleRate, &baseFrameCount);
    if (pBaseData == NULL) {
        return -1;  // Failed to load file.
    }

    void (* onConvertPCM)(void* dst, const void* src, ma_uint64 count, ma_dither_mode ditherMode) = NULL;
    const char* pBenchmarkFilePath = NULL;

    switch (formatIn) {
        case ma_format_u8:
        {
            switch (formatOut) {
                case ma_format_u8:
                {
                    onConvertPCM = ma_pcm_u8_to_u8;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_u8__mono_8000.raw";
                } break;
                case ma_format_s16:
                {
                    onConvertPCM = ma_pcm_u8_to_s16__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_s16__mono_8000.raw";
                } break;
                case ma_format_s24:
                {
                    onConvertPCM = ma_pcm_u8_to_s24__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_s24__mono_8000.raw";
                } break;
                case ma_format_s32:
                {
                    onConvertPCM = ma_pcm_u8_to_s32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_s32__mono_8000.raw";
                } break;
                case ma_format_f32:
                {
                    onConvertPCM = ma_pcm_u8_to_f32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_u8_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        case ma_format_s16:
        {
            switch (formatOut) {
                case ma_format_u8:
                {
                    onConvertPCM = ma_pcm_s16_to_u8__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_u8__mono_8000.raw";
                } break;
                case ma_format_s16:
                {
                    onConvertPCM = ma_pcm_s16_to_s16;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_s16__mono_8000.raw";
                } break;
                case ma_format_s24:
                {
                    onConvertPCM = ma_pcm_s16_to_s24__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_s24__mono_8000.raw";
                } break;
                case ma_format_s32:
                {
                    onConvertPCM = ma_pcm_s16_to_s32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_s32__mono_8000.raw";
                } break;
                case ma_format_f32:
                {
                    onConvertPCM = ma_pcm_s16_to_f32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s16_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        case ma_format_s24:
        {
            switch (formatOut) {
                case ma_format_u8:
                {
                    onConvertPCM = ma_pcm_s24_to_u8__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_u8__mono_8000.raw";
                } break;
                case ma_format_s16:
                {
                    onConvertPCM = ma_pcm_s24_to_s16__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_s16__mono_8000.raw";
                } break;
                case ma_format_s24:
                {
                    onConvertPCM = ma_pcm_s24_to_s24;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_s24__mono_8000.raw";
                } break;
                case ma_format_s32:
                {
                    onConvertPCM = ma_pcm_s24_to_s32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_s32__mono_8000.raw";
                } break;
                case ma_format_f32:
                {
                    onConvertPCM = ma_pcm_s24_to_f32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s24_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        case ma_format_s32:
        {
            switch (formatOut) {
                case ma_format_u8:
                {
                    onConvertPCM = ma_pcm_s32_to_u8__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_u8__mono_8000.raw";
                } break;
                case ma_format_s16:
                {
                    onConvertPCM = ma_pcm_s32_to_s16__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_s16__mono_8000.raw";
                } break;
                case ma_format_s24:
                {
                    onConvertPCM = ma_pcm_s32_to_s24__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_s24__mono_8000.raw";
                } break;
                case ma_format_s32:
                {
                    onConvertPCM = ma_pcm_s32_to_s32;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_s32__mono_8000.raw";
                } break;
                case ma_format_f32:
                {
                    onConvertPCM = ma_pcm_s32_to_f32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_s32_to_f32__mono_8000.raw";
                } break;
                default:
                {
                    result = -1;
                } break;
            }
        } break;

        case ma_format_f32:
        {
            switch (formatOut) {
                case ma_format_u8:
                {
                    onConvertPCM = ma_pcm_f32_to_u8__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_u8__mono_8000.raw";
                } break;
                case ma_format_s16:
                {
                    onConvertPCM = ma_pcm_f32_to_s16__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_s16__mono_8000.raw";
                } break;
                case ma_format_s24:
                {
                    onConvertPCM = ma_pcm_f32_to_s24__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_s24__mono_8000.raw";
                } break;
                case ma_format_s32:
                {
                    onConvertPCM = ma_pcm_f32_to_s32__reference;
                    pBenchmarkFilePath = "res/benchmarks/pcm_f32_to_s32__mono_8000.raw";
                } break;
                case ma_format_f32:
                {
                    onConvertPCM = ma_pcm_f32_to_f32;
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
        ma_free(pBaseData);
        return result;
    }

    // We need to allow a very small amount of difference to each sample because the software that generated our testing benchmarks can use slightly
    // different (but still correct) algorithms which produce slightly different results. I'm allowing for this variability in my basic comparison
    // tests, but testing things like dithering will require more detailed testing which I'll probably do separate to this test project.
    ma_bool32 allowSmallDifference = MA_TRUE;
    float allowedDifference = 0;
    if (allowSmallDifference) {
        if (formatOut == ma_format_f32) {
            switch (formatIn) {
                case ma_format_u8:  allowedDifference = 1.0f / 255        * 2; break;
                case ma_format_s16: allowedDifference = 1.0f / 32767      * 2; break;
                case ma_format_s24: allowedDifference = 1.0f / 8388608    * 2; break;
                case ma_format_s32: allowedDifference = 1.0f / 2147483647 * 2; break;
                case ma_format_f32: allowedDifference = 0; break;
                default: break;
            }
        } else {
            allowedDifference = 1;
        }
    }

    ma_uint64 benchmarkFrameCount;
    void* pBenchmarkData = load_raw_audio_data(pBenchmarkFilePath, formatOut, &benchmarkFrameCount);
    if (pBenchmarkData != NULL) {
        if (benchmarkFrameCount == baseFrameCount) {
            void* pConvertedData = (void*)ma_malloc((size_t)benchmarkFrameCount * ma_get_bytes_per_sample(formatOut));
            if (pConvertedData != NULL) {
                onConvertPCM(pConvertedData, pBaseData, (ma_uint32)benchmarkFrameCount, ma_dither_mode_none);
                result = ma_pcm_compare(pBenchmarkData, pConvertedData, benchmarkFrameCount, formatOut, allowedDifference);
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


    ma_free(pBaseData);
    ma_free(pBenchmarkData);
    return result;
}

int do_format_conversion_tests_u8()
{
    int result = 0;

    printf("PCM u8 -> u8... ");
    if (do_format_conversion_test(ma_format_u8, ma_format_u8) != 0) {
        result = -1;
    }

    printf("PCM u8 -> s16... ");
    if (do_format_conversion_test(ma_format_u8, ma_format_s16) != 0) {
        result = -1;
    }

    printf("PCM u8 -> s24... ");
    if (do_format_conversion_test(ma_format_u8, ma_format_s24) != 0) {
        result = -1;
    }

    printf("PCM u8 -> s32... ");
    if (do_format_conversion_test(ma_format_u8, ma_format_s32) != 0) {
        result = -1;
    }

    printf("PCM u8 -> f32... ");
    if (do_format_conversion_test(ma_format_u8, ma_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests_s16()
{
    int result = 0;

    printf("PCM s16 -> u8... ");
    if (do_format_conversion_test(ma_format_s16, ma_format_u8) != 0) {
        result = -1;
    }

    printf("PCM s16 -> s16... ");
    if (do_format_conversion_test(ma_format_s16, ma_format_s16) != 0) {
        result = -1;
    }

    printf("PCM s16 -> s24... ");
    if (do_format_conversion_test(ma_format_s16, ma_format_s24) != 0) {
        result = -1;
    }

    printf("PCM s16 -> s32... ");
    if (do_format_conversion_test(ma_format_s16, ma_format_s32) != 0) {
        result = -1;
    }

    printf("PCM s16 -> f32... ");
    if (do_format_conversion_test(ma_format_s16, ma_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests_s24()
{
    int result = 0;

    printf("PCM s24 -> u8... ");
    if (do_format_conversion_test(ma_format_s24, ma_format_u8) != 0) {
        result = -1;
    }

    printf("PCM s24 -> s16... ");
    if (do_format_conversion_test(ma_format_s24, ma_format_s16) != 0) {
        result = -1;
    }

    printf("PCM s24 -> s24... ");
    if (do_format_conversion_test(ma_format_s24, ma_format_s24) != 0) {
        result = -1;
    }

    printf("PCM s24 -> s32... ");
    if (do_format_conversion_test(ma_format_s24, ma_format_s32) != 0) {
        result = -1;
    }

    printf("PCM s24 -> f32... ");
    if (do_format_conversion_test(ma_format_s24, ma_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests_s32()
{
    int result = 0;

    printf("PCM s32 -> u8... ");
    if (do_format_conversion_test(ma_format_s32, ma_format_u8) != 0) {
        result = -1;
    }

    printf("PCM s32 -> s16... ");
    if (do_format_conversion_test(ma_format_s32, ma_format_s16) != 0) {
        result = -1;
    }

    printf("PCM s32 -> s24... ");
    if (do_format_conversion_test(ma_format_s32, ma_format_s24) != 0) {
        result = -1;
    }

    printf("PCM s32 -> s32... ");
    if (do_format_conversion_test(ma_format_s32, ma_format_s32) != 0) {
        result = -1;
    }

    printf("PCM s32 -> f32... ");
    if (do_format_conversion_test(ma_format_s32, ma_format_f32) != 0) {
        result = -1;
    }

    return result;
}

int do_format_conversion_tests_f32()
{
    int result = 0;

    printf("PCM f32 -> u8... ");
    if (do_format_conversion_test(ma_format_f32, ma_format_u8) != 0) {
        result = -1;
    }

    printf("PCM f32 -> s16... ");
    if (do_format_conversion_test(ma_format_f32, ma_format_s16) != 0) {
        result = -1;
    }

    printf("PCM f32 -> s24... ");
    if (do_format_conversion_test(ma_format_f32, ma_format_s24) != 0) {
        result = -1;
    }

    printf("PCM f32 -> s32... ");
    if (do_format_conversion_test(ma_format_f32, ma_format_s32) != 0) {
        result = -1;
    }

    printf("PCM f32 -> f32... ");
    if (do_format_conversion_test(ma_format_f32, ma_format_f32) != 0) {
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


int compare_interleaved_and_deinterleaved_buffers(const void* interleaved, const void** deinterleaved, ma_uint32 frameCount, ma_uint32 channels, ma_format format)
{
    ma_uint32 bytesPerSample = ma_get_bytes_per_sample(format);

    const ma_uint8* interleaved8 = (const ma_uint8*)interleaved;
    const ma_uint8** deinterleaved8 = (const ma_uint8**)deinterleaved;

    for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
        const ma_uint8* interleavedFrame = interleaved8 + iFrame*channels*bytesPerSample;

        for (ma_uint32 iChannel = 0; iChannel < channels; iChannel += 1) {
            const ma_uint8* deinterleavedFrame = deinterleaved8[iChannel] + iFrame*bytesPerSample;

            int result = memcmp(interleavedFrame + iChannel*bytesPerSample, deinterleavedFrame, bytesPerSample);
            if (result != 0) {
                return -1;
            }
        }
    }

    // Getting here means nothing failed.
    return 0;
}

int do_interleaving_test(ma_format format)
{
    // This test is simple. We start with a deinterleaved buffer. We then test interleaving. Then we deinterleave the interleaved buffer
    // and compare that the original. It should be bit-perfect. We do this for all channel counts.
    
    int result = 0;

    switch (format)
    {
        case ma_format_u8:
        {
            ma_uint8 src [MA_MAX_CHANNELS][64];
            ma_uint8 dst [MA_MAX_CHANNELS][64];
            ma_uint8 dsti[MA_MAX_CHANNELS*64];
            void* ppSrc[MA_MAX_CHANNELS];
            void* ppDst[MA_MAX_CHANNELS];

            ma_uint32 frameCount = ma_countof(src[0]);
            ma_uint32 channelCount = ma_countof(src);
            for (ma_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame] = (ma_uint8)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (ma_uint32 i = 0; i < channelCount; ++i) {
                ma_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                ma_pcm_interleave_u8__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }

                // Deinterleave.
                ma_pcm_deinterleave_u8__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }
            }
        } break;

        case ma_format_s16:
        {
            ma_int16 src [MA_MAX_CHANNELS][64];
            ma_int16 dst [MA_MAX_CHANNELS][64];
            ma_int16 dsti[MA_MAX_CHANNELS*64];
            void* ppSrc[MA_MAX_CHANNELS];
            void* ppDst[MA_MAX_CHANNELS];

            ma_uint32 frameCount = ma_countof(src[0]);
            ma_uint32 channelCount = ma_countof(src);
            for (ma_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame] = (ma_int16)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (ma_uint32 i = 0; i < channelCount; ++i) {
                ma_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                ma_pcm_interleave_s16__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }

                // Deinterleave.
                ma_pcm_deinterleave_s16__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }
            }
        } break;

        case ma_format_s24:
        {
            ma_uint8 src [MA_MAX_CHANNELS][64*3];
            ma_uint8 dst [MA_MAX_CHANNELS][64*3];
            ma_uint8 dsti[MA_MAX_CHANNELS*64*3];
            void* ppSrc[MA_MAX_CHANNELS];
            void* ppDst[MA_MAX_CHANNELS];

            ma_uint32 frameCount = ma_countof(src[0])/3;
            ma_uint32 channelCount = ma_countof(src);
            for (ma_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame*3 + 0] = (ma_uint8)iChannel;
                    src[iChannel][iFrame*3 + 1] = (ma_uint8)iChannel;
                    src[iChannel][iFrame*3 + 2] = (ma_uint8)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (ma_uint32 i = 0; i < channelCount; ++i) {
                ma_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                ma_pcm_interleave_s24__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", channelCountForThisIteration);
                    result = -1;
                    break;
                }

                // Deinterleave.
                ma_pcm_deinterleave_s24__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", channelCountForThisIteration);
                    result = -1;
                    break;
                }
            }
        } break;

        case ma_format_s32:
        {
            ma_int32 src [MA_MAX_CHANNELS][64];
            ma_int32 dst [MA_MAX_CHANNELS][64];
            ma_int32 dsti[MA_MAX_CHANNELS*64];
            void* ppSrc[MA_MAX_CHANNELS];
            void* ppDst[MA_MAX_CHANNELS];

            ma_uint32 frameCount = ma_countof(src[0]);
            ma_uint32 channelCount = ma_countof(src);
            for (ma_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame] = (ma_int32)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (ma_uint32 i = 0; i < channelCount; ++i) {
                ma_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                ma_pcm_interleave_s32__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }

                // Deinterleave.
                ma_pcm_deinterleave_s32__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppDst, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Interleaved to Deinterleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }
            }
        } break;

        case ma_format_f32:
        {
            float src [MA_MAX_CHANNELS][64];
            float dst [MA_MAX_CHANNELS][64];
            float dsti[MA_MAX_CHANNELS*64];
            void* ppSrc[MA_MAX_CHANNELS];
            void* ppDst[MA_MAX_CHANNELS];

            ma_uint32 frameCount = ma_countof(src[0]);
            ma_uint32 channelCount = ma_countof(src);
            for (ma_uint32 iChannel = 0; iChannel < channelCount; iChannel += 1) {
                for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    src[iChannel][iFrame] = (float)iChannel;
                }

                ppSrc[iChannel] = &src[iChannel];
                ppDst[iChannel] = &dst[iChannel];
            }

            // Now test every channel count.
            for (ma_uint32 i = 0; i < channelCount; ++i) {
                ma_uint32 channelCountForThisIteration = i + 1;

                // Interleave.
                ma_pcm_interleave_f32__reference(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration);
                if (compare_interleaved_and_deinterleaved_buffers(dsti, (const void**)ppSrc, frameCount, channelCountForThisIteration, format) != 0) {
                    printf("FAILED. Deinterleaved to Interleaved (Channels = %u)\n", i);
                    result = -1;
                    break;
                }

                // Deinterleave.
                ma_pcm_deinterleave_f32__reference((void**)ppDst, dsti, frameCount, channelCountForThisIteration);
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
    if (do_interleaving_test(ma_format_u8) != 0) {
        result = -1;
    }

    printf("s16... ");
    if (do_interleaving_test(ma_format_s16) != 0) {
        result = -1;
    }

    printf("s24... ");
    if (do_interleaving_test(ma_format_s24) != 0) {
        result = -1;
    }

    printf("s32... ");
    if (do_interleaving_test(ma_format_s32) != 0) {
        result = -1;
    }

    printf("f32... ");
    if (do_interleaving_test(ma_format_f32) != 0) {
        result = -1;
    }

    return result;
}


ma_uint32 converter_test_interleaved_callback(ma_format_converter* pConverter, ma_uint32 frameCount, void* pFramesOut, void* pUserData)
{
    ma_sine_wave* pSineWave = (ma_sine_wave*)pUserData;
    ma_assert(pSineWave != NULL);

    float* pFramesOutF32 = (float*)pFramesOut;

    for (ma_uint32 iFrame = 0; iFrame < frameCount; iFrame += 1) {
        float sample;
        ma_sine_wave_read_f32(pSineWave, 1, &sample);

        for (ma_uint32 iChannel = 0; iChannel < pConverter->config.channels; iChannel += 1) {
            pFramesOutF32[iFrame*pConverter->config.channels + iChannel] = sample;
        }
    }

    return frameCount;
}

ma_uint32 converter_test_deinterleaved_callback(ma_format_converter* pConverter, ma_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    ma_sine_wave* pSineWave = (ma_sine_wave*)pUserData;
    ma_assert(pSineWave != NULL);

    ma_sine_wave_read_f32(pSineWave, frameCount, (float*)ppSamplesOut[0]);

    // Copy everything from the first channel over the others.
    for (ma_uint32 iChannel = 1; iChannel < pConverter->config.channels; iChannel += 1) {
        ma_copy_memory(ppSamplesOut[iChannel], ppSamplesOut[0], frameCount * sizeof(float));
    }

    return frameCount;
}

int do_format_converter_tests()
{
    double amplitude = 1;
    double periodsPerSecond = 400;
    ma_uint32 sampleRate = 48000;

    ma_result result = MA_SUCCESS;

    ma_sine_wave sineWave;
    ma_format_converter converter;

    ma_format_converter_config config;
    ma_zero_object(&config);
    config.formatIn = ma_format_f32;
    config.formatOut = ma_format_s16;
    config.channels = 2;
    config.streamFormatIn = ma_stream_format_pcm;
    config.streamFormatOut = ma_stream_format_pcm;
    config.ditherMode = ma_dither_mode_none;
    config.pUserData = &sineWave;


    config.onRead = converter_test_interleaved_callback;
    config.onReadDeinterleaved = NULL;

    // Interleaved/Interleaved f32 to s16.
    {
        ma_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = ma_format_converter_init(&config, &converter);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        ma_int16 interleavedFrames[MA_MAX_CHANNELS * 1024];
        ma_uint64 framesRead = ma_format_converter_read(&converter, 1024, interleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        FILE* pFile = ma_fopen("res/output/converter_f32_to_s16_interleaved_interleaved__stereo_48000.raw", "wb");
        if (pFile == NULL) {
            printf("Failed to open output file.\n");
            return -1;
        }

        fwrite(interleavedFrames, sizeof(ma_int16), (size_t)framesRead * converter.config.channels, pFile);
        fclose(pFile);
    }

    // Interleaved/Deinterleaved f32 to s16.
    {
        ma_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = ma_format_converter_init(&config, &converter);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        ma_int16 deinterleavedFrames[MA_MAX_CHANNELS][1024];
        void* ppDeinterleavedFrames[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            ppDeinterleavedFrames[iChannel] = &deinterleavedFrames[iChannel];
        }

        ma_uint64 framesRead = ma_format_converter_read_deinterleaved(&converter, 1024, ppDeinterleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        // Write a separate file for each channel.
        for (ma_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "res/output/converter_f32_to_s16_interleaved_deinterleaved__stereo_48000.raw.%d", iChannel);

            FILE* pFile = ma_fopen(filePath, "wb");
            if (pFile == NULL) {
                printf("Failed to open output file.\n");
                return -1;
            }

            fwrite(ppDeinterleavedFrames[iChannel], sizeof(ma_int16), (size_t)framesRead, pFile);
            fclose(pFile);
        }
    }


    config.onRead = NULL;
    config.onReadDeinterleaved = converter_test_deinterleaved_callback;

    // Deinterleaved/Interleaved f32 to s16.
    {
        ma_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = ma_format_converter_init(&config, &converter);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        ma_int16 interleavedFrames[MA_MAX_CHANNELS * 1024];
        ma_uint64 framesRead = ma_format_converter_read(&converter, 1024, interleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        FILE* pFile = ma_fopen("res/output/converter_f32_to_s16_deinterleaved_interleaved__stereo_48000.raw", "wb");
        if (pFile == NULL) {
            printf("Failed to open output file.\n");
            return -1;
        }

        fwrite(interleavedFrames, sizeof(ma_int16), (size_t)framesRead * converter.config.channels, pFile);
        fclose(pFile);
    }

    // Deinterleaved/Deinterleaved f32 to s16.
    {
        ma_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = ma_format_converter_init(&config, &converter);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        ma_int16 deinterleavedFrames[MA_MAX_CHANNELS][1024];
        void* ppDeinterleavedFrames[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            ppDeinterleavedFrames[iChannel] = &deinterleavedFrames[iChannel];
        }

        ma_uint64 framesRead = ma_format_converter_read_deinterleaved(&converter, 1024, ppDeinterleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        // Write a separate file for each channel.
        for (ma_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "res/output/converter_f32_to_s16_deinterleaved_deinterleaved__stereo_48000.raw.%d", iChannel);

            FILE* pFile = ma_fopen(filePath, "wb");
            if (pFile == NULL) {
                printf("Failed to open output file.\n");
                return -1;
            }

            fwrite(ppDeinterleavedFrames[iChannel], sizeof(ma_int16), (size_t)framesRead, pFile);
            fclose(pFile);
        }
    }


    config.onRead = converter_test_interleaved_callback;
    config.onReadDeinterleaved = NULL;
    config.formatOut = ma_format_f32;

    // Interleaved/Interleaved f32 to f32.
    {
        ma_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = ma_format_converter_init(&config, &converter);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        float interleavedFrames[MA_MAX_CHANNELS * 1024];
        ma_uint64 framesRead = ma_format_converter_read(&converter, 1024, interleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        FILE* pFile = ma_fopen("res/output/converter_f32_to_f32_interleaved_interleaved__stereo_48000.raw", "wb");
        if (pFile == NULL) {
            printf("Failed to open output file.\n");
            return -1;
        }

        fwrite(interleavedFrames, sizeof(float), (size_t)framesRead * converter.config.channels, pFile);
        fclose(pFile);
    }

    // Interleaved/Deinterleaved f32 to f32.
    {
        ma_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = ma_format_converter_init(&config, &converter);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        float deinterleavedFrames[MA_MAX_CHANNELS][1024];
        void* ppDeinterleavedFrames[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            ppDeinterleavedFrames[iChannel] = &deinterleavedFrames[iChannel];
        }

        ma_uint64 framesRead = ma_format_converter_read_deinterleaved(&converter, 1024, ppDeinterleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        // Write a separate file for each channel.
        for (ma_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "res/output/converter_f32_to_f32_interleaved_deinterleaved__stereo_48000.raw.%d", iChannel);

            FILE* pFile = ma_fopen(filePath, "wb");
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
        ma_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = ma_format_converter_init(&config, &converter);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        float interleavedFrames[MA_MAX_CHANNELS * 1024];
        ma_uint64 framesRead = ma_format_converter_read(&converter, 1024, interleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        FILE* pFile = ma_fopen("res/output/converter_f32_to_f32_deinterleaved_interleaved__stereo_48000.raw", "wb");
        if (pFile == NULL) {
            printf("Failed to open output file.\n");
            return -1;
        }

        fwrite(interleavedFrames, sizeof(float), (size_t)framesRead * converter.config.channels, pFile);
        fclose(pFile);
    }

    // Deinterleaved/Deinterleaved f32 to f32.
    {
        ma_sine_wave_init(amplitude, periodsPerSecond, sampleRate, &sineWave);
        result = ma_format_converter_init(&config, &converter);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize converter.\n");
            return -1;
        }

        float deinterleavedFrames[MA_MAX_CHANNELS][1024];
        void* ppDeinterleavedFrames[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            ppDeinterleavedFrames[iChannel] = &deinterleavedFrames[iChannel];
        }

        ma_uint64 framesRead = ma_format_converter_read_deinterleaved(&converter, 1024, ppDeinterleavedFrames, converter.config.pUserData);
        if (framesRead != 1024) {
            printf("Failed to read interleaved data from converter.\n");
            return -1;
        }

        // Write a separate file for each channel.
        for (ma_uint32 iChannel = 0; iChannel < converter.config.channels; iChannel += 1) {
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "res/output/converter_f32_to_f32_deinterleaved_deinterleaved__stereo_48000.raw.%d", iChannel);

            FILE* pFile = ma_fopen(filePath, "wb");
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



ma_uint32 channel_router_callback__passthrough_test(ma_channel_router* pRouter, ma_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    float** ppSamplesIn = (float**)pUserData;

    for (ma_uint32 iChannel = 0; iChannel < pRouter->config.channelsIn; ++iChannel) {
        ma_copy_memory(ppSamplesOut[iChannel], ppSamplesIn[iChannel], frameCount*sizeof(float));
    }

    return frameCount;
}

int do_channel_routing_tests()
{
    ma_bool32 hasError = MA_FALSE;

    printf("Passthrough... ");
    {
        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_planar_blend;
        routerConfig.channelsIn = 6;
        routerConfig.channelsOut = routerConfig.channelsIn;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsOut, routerConfig.channelMapOut);
        
        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (!router.isPassthrough) {
                printf("Failed to init router as passthrough.\n");
                hasError = MA_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0;
                    if (iChannelIn == iChannelOut) {
                        expectedWeight = 1;
                    }

                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }


        // Here is where we check that the passthrough optimization works correctly. What we do is compare the output of the passthrough
        // optimization with the non-passthrough output. We don't use a real sound here, but instead use values that makes it easier for
        // us to check results. Each channel is given a value equal to it's index, plus 1.
        float testData[MA_MAX_CHANNELS][MA_SIMD_ALIGNMENT * 2];
        float* ppTestData[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            ppTestData[iChannel] = testData[iChannel];
            for (ma_uint32 iFrame = 0; iFrame < ma_countof(testData[0]); ++iFrame) {
                ppTestData[iChannel][iFrame] = (float)(iChannel + 1);
            }
        }

        routerConfig.pUserData = ppTestData;
        ma_channel_router_init(&routerConfig, &router);

        MA_ALIGN(MA_SIMD_ALIGNMENT) float outputA[MA_MAX_CHANNELS][MA_SIMD_ALIGNMENT * 2];
        MA_ALIGN(MA_SIMD_ALIGNMENT) float outputB[MA_MAX_CHANNELS][MA_SIMD_ALIGNMENT * 2];
        float* ppOutputA[MA_MAX_CHANNELS];
        float* ppOutputB[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            ppOutputA[iChannel] = outputA[iChannel];
            ppOutputB[iChannel] = outputB[iChannel];
        }

        // With optimizations.
        ma_uint64 framesRead = ma_channel_router_read_deinterleaved(&router, ma_countof(outputA[0]), (void**)ppOutputA, router.config.pUserData);
        if (framesRead != ma_countof(outputA[0])) {
            printf("Returned frame count for optimized incorrect.");
            hasError = MA_TRUE;
        }

        // Without optimizations.
        router.isPassthrough = MA_FALSE;
        router.isSimpleShuffle = MA_FALSE;
        framesRead = ma_channel_router_read_deinterleaved(&router, ma_countof(outputA[0]), (void**)ppOutputB, router.config.pUserData);
        if (framesRead != ma_countof(outputA[0])) {
            printf("Returned frame count for unoptimized path incorrect.");
            hasError = MA_TRUE;
        }

        // Compare.
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            for (ma_uint32 iFrame = 0; iFrame < ma_countof(outputA[0]); ++iFrame) {
                if (ppOutputA[iChannel][iFrame] != ppOutputB[iChannel][iFrame]) {
                    printf("Sample incorrect [%d][%d]\n", iChannel, iFrame);
                    hasError = MA_TRUE;
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

        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_planar_blend;
        routerConfig.channelsIn = 6;
        routerConfig.channelsOut = routerConfig.channelsIn;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            routerConfig.channelMapOut[iChannel] = routerConfig.channelMapIn[routerConfig.channelsIn - iChannel - 1];
        }

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (!router.isSimpleShuffle) {
                printf("Router not configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0;
                    if (iChannelIn == (routerConfig.channelsOut - iChannelOut - 1)) {
                        expectedWeight = 1;
                    }

                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }


        // Here is where we check that the shuffle optimization works correctly. What we do is compare the output of the shuffle
        // optimization with the non-shuffle output. We don't use a real sound here, but instead use values that makes it easier
        // for us to check results. Each channel is given a value equal to it's index, plus 1.
        float testData[MA_MAX_CHANNELS][100];
        float* ppTestData[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            ppTestData[iChannel] = testData[iChannel];
            for (ma_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
                ppTestData[iChannel][iFrame] = (float)(iChannel + 1);
            }
        }

        routerConfig.pUserData = ppTestData;
        ma_channel_router_init(&routerConfig, &router);

        float outputA[MA_MAX_CHANNELS][100];
        float outputB[MA_MAX_CHANNELS][100];
        float* ppOutputA[MA_MAX_CHANNELS];
        float* ppOutputB[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            ppOutputA[iChannel] = outputA[iChannel];
            ppOutputB[iChannel] = outputB[iChannel];
        }

        // With optimizations.
        ma_uint64 framesRead = ma_channel_router_read_deinterleaved(&router, 100, (void**)ppOutputA, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for optimized incorrect.");
            hasError = MA_TRUE;
        }

        // Without optimizations.
        router.isPassthrough = MA_FALSE;
        router.isSimpleShuffle = MA_FALSE;
        framesRead = ma_channel_router_read_deinterleaved(&router, 100, (void**)ppOutputB, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for unoptimized path incorrect.");
            hasError = MA_TRUE;
        }

        // Compare.
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            for (ma_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
                if (ppOutputA[iChannel][iFrame] != ppOutputB[iChannel][iFrame]) {
                    printf("Sample incorrect [%d][%d]\n", iChannel, iFrame);
                    hasError = MA_TRUE;
                }
            }
        }


        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Simple Mono Expansion (Mono -> Stereo)... ");
    {
        // The simple mono expansion case will be activated when a mono channel map is converted to anything without an LFE. 
        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_planar_blend;
        routerConfig.channelsIn = 1;
        routerConfig.channelsOut = 2;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsOut, routerConfig.channelMapOut);

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            if (!router.isSimpleMonoExpansion) {
                printf("Router not configured as simple mono expansion.\n");
                hasError = MA_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 1;

                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }


        // Here is where we check that the shuffle optimization works correctly. What we do is compare the output of the shuffle
        // optimization with the non-shuffle output. We don't use a real sound here, but instead use values that makes it easier
        // for us to check results. Each channel is given a value equal to it's index, plus 1.
        float testData[MA_MAX_CHANNELS][100];
        float* ppTestData[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            ppTestData[iChannel] = testData[iChannel];
            for (ma_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
                ppTestData[iChannel][iFrame] = (float)(iChannel + 1);
            }
        }

        routerConfig.pUserData = ppTestData;
        ma_channel_router_init(&routerConfig, &router);

        float outputA[MA_MAX_CHANNELS][100];
        float outputB[MA_MAX_CHANNELS][100];
        float* ppOutputA[MA_MAX_CHANNELS];
        float* ppOutputB[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            ppOutputA[iChannel] = outputA[iChannel];
            ppOutputB[iChannel] = outputB[iChannel];
        }

        // With optimizations.
        ma_uint64 framesRead = ma_channel_router_read_deinterleaved(&router, 100, (void**)ppOutputA, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for optimized incorrect.");
            hasError = MA_TRUE;
        }

        // Without optimizations.
        router.isPassthrough = MA_FALSE;
        router.isSimpleShuffle = MA_FALSE;
        framesRead = ma_channel_router_read_deinterleaved(&router, 100, (void**)ppOutputB, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for unoptimized path incorrect.");
            hasError = MA_TRUE;
        }

        // Compare.
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            for (ma_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
                if (ppOutputA[iChannel][iFrame] != ppOutputB[iChannel][iFrame]) {
                    printf("Sample incorrect [%d][%d]\n", iChannel, iFrame);
                    hasError = MA_TRUE;
                }
            }
        }


        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Simple Stereo to Mono... ");
    {
        // The simple mono expansion case will be activated when a mono channel map is converted to anything without an LFE. 
        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_planar_blend;
        routerConfig.channelsIn = 2;
        routerConfig.channelsOut = 1;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsOut, routerConfig.channelMapOut);

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleMonoExpansion) {
                printf("Router incorrectly configured as simple mono expansion.\n");
                hasError = MA_TRUE;
            }
            if (!router.isStereoToMono) {
                printf("Router not configured as stereo to mono.\n");
                hasError = MA_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0.5f;

                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }


        // Here is where we check that the shuffle optimization works correctly. What we do is compare the output of the shuffle
        // optimization with the non-shuffle output. We don't use a real sound here, but instead use values that makes it easier
        // for us to check results. Each channel is given a value equal to it's index, plus 1.
        float testData[MA_MAX_CHANNELS][100];
        float* ppTestData[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            ppTestData[iChannel] = testData[iChannel];
            for (ma_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
                ppTestData[iChannel][iFrame] = (float)(iChannel + 1);
            }
        }

        routerConfig.pUserData = ppTestData;
        ma_channel_router_init(&routerConfig, &router);

        float outputA[MA_MAX_CHANNELS][100];
        float outputB[MA_MAX_CHANNELS][100];
        float* ppOutputA[MA_MAX_CHANNELS];
        float* ppOutputB[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            ppOutputA[iChannel] = outputA[iChannel];
            ppOutputB[iChannel] = outputB[iChannel];
        }

        // With optimizations.
        ma_uint64 framesRead = ma_channel_router_read_deinterleaved(&router, 100, (void**)ppOutputA, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for optimized incorrect.");
            hasError = MA_TRUE;
        }

        // Without optimizations.
        router.isPassthrough = MA_FALSE;
        router.isSimpleShuffle = MA_FALSE;
        framesRead = ma_channel_router_read_deinterleaved(&router, 100, (void**)ppOutputB, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for unoptimized path incorrect.");
            hasError = MA_TRUE;
        }

        // Compare.
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            for (ma_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
                if (ppOutputA[iChannel][iFrame] != ppOutputB[iChannel][iFrame]) {
                    printf("Sample incorrect [%d][%d]\n", iChannel, iFrame);
                    hasError = MA_TRUE;
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

        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_simple;
        routerConfig.channelsIn = 2;
        routerConfig.channelsOut = 6;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsOut, routerConfig.channelMapOut);

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0;
                    if (routerConfig.channelMapIn[iChannelIn] == routerConfig.channelMapOut[iChannelOut]) {
                        expectedWeight = 1;
                    }

                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Simple Conversion (5.1 -> Stereo)... ");
    {
        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_simple;
        routerConfig.channelsIn = 6;
        routerConfig.channelsOut = 2;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsIn, routerConfig.channelMapIn);
        ma_get_standard_channel_map(ma_standard_channel_map_microsoft, routerConfig.channelsOut, routerConfig.channelMapOut);

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            
            // Expecting the weights to all be equal to 1 for each channel.
            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    float expectedWeight = 0;
                    if (routerConfig.channelMapIn[iChannelIn] == routerConfig.channelMapOut[iChannelOut]) {
                        expectedWeight = 1;
                    }

                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeight) {
                        printf("Failed. Channel weight incorrect: %f\n", expectedWeight);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Planar Blend Conversion (Stereo -> 5.1)... ");
    {
        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_planar_blend;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        
        // Use very specific mappings for this test.
        routerConfig.channelsIn = 2;
        routerConfig.channelMapIn[0] = MA_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapIn[1] = MA_CHANNEL_FRONT_RIGHT;

        routerConfig.channelsOut = 8;
        routerConfig.channelMapOut[0] = MA_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapOut[1] = MA_CHANNEL_FRONT_RIGHT;
        routerConfig.channelMapOut[2] = MA_CHANNEL_FRONT_CENTER;
        routerConfig.channelMapOut[3] = MA_CHANNEL_LFE;
        routerConfig.channelMapOut[4] = MA_CHANNEL_BACK_LEFT;
        routerConfig.channelMapOut[5] = MA_CHANNEL_BACK_RIGHT;
        routerConfig.channelMapOut[6] = MA_CHANNEL_SIDE_LEFT;
        routerConfig.channelMapOut[7] = MA_CHANNEL_SIDE_RIGHT;

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            
            float expectedWeights[MA_MAX_CHANNELS][MA_MAX_CHANNELS];
            ma_zero_memory(expectedWeights, sizeof(expectedWeights));
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

            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeights[iChannelIn][iChannelOut]) {
                        printf("Failed. Channel weight incorrect for [%d][%d]. Expected %f, got %f\n", iChannelIn, iChannelOut, expectedWeights[iChannelIn][iChannelOut], router.config.weights[iChannelIn][iChannelOut]);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }


        // Test the actual conversion. The test data is set to +1 for the left channel, and -1 for the right channel.
        float testData[MA_MAX_CHANNELS][100];
        float* ppTestData[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsIn; ++iChannel) {
            ppTestData[iChannel] = testData[iChannel];
        }

        for (ma_uint32 iFrame = 0; iFrame < 100; ++iFrame) {
            ppTestData[0][iFrame] = -1;
            ppTestData[1][iFrame] = +1;
        }

        routerConfig.pUserData = ppTestData;
        ma_channel_router_init(&routerConfig, &router);

        float output[MA_MAX_CHANNELS][100];
        float* ppOutput[MA_MAX_CHANNELS];
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            ppOutput[iChannel] = output[iChannel];
        }

        ma_uint64 framesRead = ma_channel_router_read_deinterleaved(&router, 100, (void**)ppOutput, router.config.pUserData);
        if (framesRead != 100) {
            printf("Returned frame count for optimized incorrect.\n");
            hasError = MA_TRUE;
        }

        float expectedOutput[MA_MAX_CHANNELS];
        expectedOutput[0] = -1.0f;  // FRONT_LEFT
        expectedOutput[1] = +1.0f;  // FRONT_RIGHT
        expectedOutput[2] =  0.0f;  // FRONT_CENTER (left and right should cancel out, totalling 0).
        expectedOutput[3] =  0.0f;  // LFE
        expectedOutput[4] = -0.25f; // BACK_LEFT
        expectedOutput[5] = +0.25f; // BACK_RIGHT
        expectedOutput[6] = -0.5f;  // SIDE_LEFT
        expectedOutput[7] = +0.5f;  // SIDE_RIGHT
        for (ma_uint32 iChannel = 0; iChannel < routerConfig.channelsOut; ++iChannel) {
            for (ma_uint32 iFrame = 0; iFrame < framesRead; ++iFrame) {
                if (output[iChannel][iFrame] != expectedOutput[iChannel]) {
                    printf("Incorrect sample [%d][%d]. Expecting %f, got %f\n", iChannel, iFrame, expectedOutput[iChannel], output[iChannel][iFrame]);
                    hasError = MA_TRUE;
                }
            }
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Planar Blend Conversion (5.1 -> Stereo)... ");
    {
        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_planar_blend;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        
        // Use very specific mappings for this test.
        routerConfig.channelsIn = 8;
        routerConfig.channelMapIn[0] = MA_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapIn[1] = MA_CHANNEL_FRONT_RIGHT;
        routerConfig.channelMapIn[2] = MA_CHANNEL_FRONT_CENTER;
        routerConfig.channelMapIn[3] = MA_CHANNEL_LFE;
        routerConfig.channelMapIn[4] = MA_CHANNEL_BACK_LEFT;
        routerConfig.channelMapIn[5] = MA_CHANNEL_BACK_RIGHT;
        routerConfig.channelMapIn[6] = MA_CHANNEL_SIDE_LEFT;
        routerConfig.channelMapIn[7] = MA_CHANNEL_SIDE_RIGHT;

        routerConfig.channelsOut = 2;
        routerConfig.channelMapOut[0] = MA_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapOut[1] = MA_CHANNEL_FRONT_RIGHT;

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            
            float expectedWeights[MA_MAX_CHANNELS][MA_MAX_CHANNELS];
            ma_zero_memory(expectedWeights, sizeof(expectedWeights));
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

            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeights[iChannelIn][iChannelOut]) {
                        printf("Failed. Channel weight incorrect for [%d][%d]. Expected %f, got %f\n", iChannelIn, iChannelOut, expectedWeights[iChannelIn][iChannelOut], router.config.weights[iChannelIn][iChannelOut]);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("Mono -> 2.1 + None... ");
    {
        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_planar_blend;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        
        // Use very specific mappings for this test.
        routerConfig.channelsIn = 1;
        routerConfig.channelMapIn[0] = MA_CHANNEL_MONO;

        routerConfig.channelsOut = 4;
        routerConfig.channelMapOut[0] = MA_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapOut[1] = MA_CHANNEL_FRONT_RIGHT;
        routerConfig.channelMapOut[2] = MA_CHANNEL_NONE;
        routerConfig.channelMapOut[3] = MA_CHANNEL_LFE;

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            
            float expectedWeights[MA_MAX_CHANNELS][MA_MAX_CHANNELS];
            ma_zero_memory(expectedWeights, sizeof(expectedWeights));
            expectedWeights[0][0] = 1.0f;   // MONO -> FRONT_LEFT
            expectedWeights[0][1] = 1.0f;   // MONO -> FRONT_RIGHT
            expectedWeights[0][2] = 0.0f;   // MONO -> NONE
            expectedWeights[0][3] = 0.0f;   // MONO -> LFE

            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeights[iChannelIn][iChannelOut]) {
                        printf("Failed. Channel weight incorrect for [%d][%d]. Expected %f, got %f\n", iChannelIn, iChannelOut, expectedWeights[iChannelIn][iChannelOut], router.config.weights[iChannelIn][iChannelOut]);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
        }

        if (!hasError) {
            printf("PASSED\n");
        }
    }

    printf("2.1 + None -> Mono... ");
    {
        ma_channel_router_config routerConfig;
        ma_zero_object(&routerConfig);
        routerConfig.onReadDeinterleaved = channel_router_callback__passthrough_test;
        routerConfig.pUserData = NULL;
        routerConfig.mixingMode = ma_channel_mix_mode_planar_blend;
        routerConfig.noSSE2 = MA_TRUE;
        routerConfig.noAVX2 = MA_TRUE;
        routerConfig.noAVX512 = MA_TRUE;
        routerConfig.noNEON = MA_TRUE;
        
        // Use very specific mappings for this test.
        routerConfig.channelsIn = 4;
        routerConfig.channelMapIn[0] = MA_CHANNEL_FRONT_LEFT;
        routerConfig.channelMapIn[1] = MA_CHANNEL_FRONT_RIGHT;
        routerConfig.channelMapIn[2] = MA_CHANNEL_NONE;
        routerConfig.channelMapIn[3] = MA_CHANNEL_LFE;

        routerConfig.channelsOut = 1;
        routerConfig.channelMapOut[0] = MA_CHANNEL_MONO;

        ma_channel_router router;
        ma_result result = ma_channel_router_init(&routerConfig, &router);
        if (result == MA_SUCCESS) {
            if (router.isPassthrough) {
                printf("Router incorrectly configured as a passthrough.\n");
                hasError = MA_TRUE;
            }
            if (router.isSimpleShuffle) {
                printf("Router incorrectly configured as a simple shuffle.\n");
                hasError = MA_TRUE;
            }
            
            float expectedWeights[MA_MAX_CHANNELS][MA_MAX_CHANNELS];
            ma_zero_memory(expectedWeights, sizeof(expectedWeights));
            expectedWeights[0][0] = 0.5f;   // FRONT_LEFT  -> MONO
            expectedWeights[1][0] = 0.5f;   // FRONT_RIGHT -> MONO
            expectedWeights[2][0] = 0.0f;   // NONE        -> MONO
            expectedWeights[3][0] = 0.0f;   // LFE         -> MONO

            for (ma_uint32 iChannelIn = 0; iChannelIn < routerConfig.channelsIn; ++iChannelIn) {
                for (ma_uint32 iChannelOut = 0; iChannelOut < routerConfig.channelsOut; ++iChannelOut) {
                    if (router.config.weights[iChannelIn][iChannelOut] != expectedWeights[iChannelIn][iChannelOut]) {
                        printf("Failed. Channel weight incorrect for [%d][%d]. Expected %f, got %f\n", iChannelIn, iChannelOut, expectedWeights[iChannelIn][iChannelOut], router.config.weights[iChannelIn][iChannelOut]);
                        hasError = MA_TRUE;
                    }
                }
            }
        } else {
            printf("Failed to init router.\n");
            hasError = MA_TRUE;
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


int do_backend_test(ma_backend backend)
{
    ma_result result = MA_SUCCESS;
    ma_context context;
    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_device_info* pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;

    printf("--- %s ---\n", ma_get_backend_name(backend));

    // Context.
    printf("  Creating Context... ");
    {
        ma_context_config contextConfig = ma_context_config_init();
        contextConfig.logCallback = on_log;

        result = ma_context_init(&backend, 1, &contextConfig, &context);
        if (result == MA_SUCCESS) {
            printf(" Done\n");
        } else {
            if (result == MA_NO_BACKEND) {
                printf(" Not supported\n");
                printf("--- End %s ---\n", ma_get_backend_name(backend));
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
        result = ma_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
        if (result == MA_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed\n");
            goto done;
        }

        printf("    Playback Devices (%d)\n", playbackDeviceCount);
        for (ma_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (ma_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
        }
    }

    // Device Information.
    printf("  Getting Device Information...\n");
    {
        printf("    Playback Devices (%d)\n", playbackDeviceCount);
        for (ma_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
            
            result = ma_context_get_device_info(&context, ma_device_type_playback, &pPlaybackDeviceInfos[iDevice].id, ma_share_mode_shared, &pPlaybackDeviceInfos[iDevice]);
            if (result == MA_SUCCESS) {
                printf("        Name:            %s\n", pPlaybackDeviceInfos[iDevice].name);
                printf("        Min Channels:    %d\n", pPlaybackDeviceInfos[iDevice].minChannels);
                printf("        Max Channels:    %d\n", pPlaybackDeviceInfos[iDevice].maxChannels);
                printf("        Min Sample Rate: %d\n", pPlaybackDeviceInfos[iDevice].minSampleRate);
                printf("        Max Sample Rate: %d\n", pPlaybackDeviceInfos[iDevice].maxSampleRate);
                printf("        Format Count:    %d\n", pPlaybackDeviceInfos[iDevice].formatCount);
                for (ma_uint32 iFormat = 0; iFormat < pPlaybackDeviceInfos[iDevice].formatCount; ++iFormat) {
                    printf("          %s\n", ma_get_format_name(pPlaybackDeviceInfos[iDevice].formats[iFormat]));
                }
            } else {
                printf("        ERROR\n");
            }
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (ma_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
            
            result = ma_context_get_device_info(&context, ma_device_type_capture, &pCaptureDeviceInfos[iDevice].id, ma_share_mode_shared, &pCaptureDeviceInfos[iDevice]);
            if (result == MA_SUCCESS) {
                printf("        Name:            %s\n", pCaptureDeviceInfos[iDevice].name);
                printf("        Min Channels:    %d\n", pCaptureDeviceInfos[iDevice].minChannels);
                printf("        Max Channels:    %d\n", pCaptureDeviceInfos[iDevice].maxChannels);
                printf("        Min Sample Rate: %d\n", pCaptureDeviceInfos[iDevice].minSampleRate);
                printf("        Max Sample Rate: %d\n", pCaptureDeviceInfos[iDevice].maxSampleRate);
                printf("        Format Count:    %d\n", pCaptureDeviceInfos[iDevice].formatCount);
                for (ma_uint32 iFormat = 0; iFormat < pCaptureDeviceInfos[iDevice].formatCount; ++iFormat) {
                    printf("          %s\n", ma_get_format_name(pCaptureDeviceInfos[iDevice].formats[iFormat]));
                }
            } else {
                printf("        ERROR\n");
            }
        }
    }

done:
    printf("--- End %s ---\n", ma_get_backend_name(backend));
    printf("\n");
    
    ma_context_uninit(&context);
    return (result == MA_SUCCESS) ? 0 : -1;
}

int do_backend_tests()
{
    ma_bool32 hasErrorOccurred = MA_FALSE;

    // Tests are performed on a per-backend basis.
    for (size_t iBackend = 0; iBackend < ma_countof(g_Backends); ++iBackend) {
        int result = do_backend_test(g_Backends[iBackend]);
        if (result < 0) {
            hasErrorOccurred = MA_TRUE;
        }
    }

    return (hasErrorOccurred) ? -1 : 0;
}


typedef struct
{
    ma_decoder* pDecoder;
    ma_sine_wave* pSineWave;
    ma_event endOfPlaybackEvent;
} playback_test_callback_data;

void on_send__playback_test(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    playback_test_callback_data* pData = (playback_test_callback_data*)pDevice->pUserData;
    ma_assert(pData != NULL);

#if !defined(__EMSCRIPTEN__)
    ma_uint64 framesRead = ma_decoder_read_pcm_frames(pData->pDecoder, pOutput, frameCount);
    if (framesRead == 0) {
        ma_event_signal(&pData->endOfPlaybackEvent);
    }
#else
    if (pDevice->playback.format == ma_format_f32) {
        for (ma_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
            float sample;
            ma_sine_wave_read_f32(pData->pSineWave, 1, &sample);

            for (ma_uint32 iChannel = 0; iChannel < pDevice->playback.channels; ++iChannel) {
                ((float*)pOutput)[iFrame*pDevice->playback.channels + iChannel] = sample;
            }
        }
    }
#endif

    (void)pInput;
}

void on_stop__playback_test(ma_device* pDevice)
{
    playback_test_callback_data* pData = (playback_test_callback_data*)pDevice->pUserData;
    ma_assert(pData != NULL);
    
    printf("Device Stopped.\n");
    ma_event_signal(&pData->endOfPlaybackEvent);
}

int do_playback_test(ma_backend backend)
{
    ma_result result = MA_SUCCESS;
    ma_device device;
    ma_decoder decoder;
    ma_sine_wave sineWave;
    ma_bool32 haveDevice = MA_FALSE;
    ma_bool32 haveDecoder = MA_FALSE;

    playback_test_callback_data callbackData;
    callbackData.pDecoder = &decoder;
    callbackData.pSineWave = &sineWave;

    printf("--- %s ---\n", ma_get_backend_name(backend));

    // Device.
    printf("  Opening Device... ");
    {
        ma_context_config contextConfig = ma_context_config_init();
        contextConfig.logCallback = on_log;

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.pUserData = &callbackData;
        deviceConfig.dataCallback = on_send__playback_test;
        deviceConfig.stopCallback = on_stop__playback_test;
        

    #if defined(__EMSCRIPTEN__)
        deviceConfig.playback.format = ma_format_f32;
    #endif

        result = ma_device_init_ex(&backend, 1, &contextConfig, &deviceConfig, &device);
        if (result == MA_SUCCESS) {
            printf("Done\n");
        } else {
            if (result == MA_NO_BACKEND) {
                printf(" Not supported\n");
                printf("--- End %s ---\n", ma_get_backend_name(backend));
                printf("\n");
                return 0;
            } else {
                printf(" Failed\n");
                goto done;
            }
        }
        haveDevice = MA_TRUE;

        printf("    Is Passthrough: %s\n", (device.playback.converter.isPassthrough) ? "YES" : "NO");
        printf("    Buffer Size in Frames: %d\n", device.playback.internalBufferSizeInFrames);
    }

    printf("  Opening Decoder... ");
    {
        result = ma_event_init(device.pContext, &callbackData.endOfPlaybackEvent);
        if (result != MA_SUCCESS) {
            printf("Failed to init event.\n");
            goto done;
        }
        
#if !defined(__EMSCRIPTEN__)
        ma_decoder_config decoderConfig = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);
        result = ma_decoder_init_file("res/sine_s16_mono_48000.wav", &decoderConfig, &decoder);
        if (result == MA_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed to init decoder.\n");
            goto done;
        }
        haveDecoder = MA_TRUE;
#else
        result = ma_sine_wave_init(0.5f, 400, device.sampleRate, &sineWave);
        if (result == MA_SUCCESS) {
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
        result = ma_device_start(&device);
        if (result != MA_SUCCESS) {
            printf("Failed to start device.\n");
            goto done;
        }

#if defined(__EMSCRIPTEN__)
        emscripten_set_main_loop(main_loop__em, 0, 1);
#endif

        // Test rapid stopping and restarting.
        //ma_device_stop(&device);
        //ma_device_start(&device);

        ma_event_wait(&callbackData.endOfPlaybackEvent);   // Wait for the sound to finish.
        printf("Done\n");
    }

done:
    printf("--- End %s ---\n", ma_get_backend_name(backend));
    printf("\n");
    
    if (haveDevice) {
        ma_device_uninit(&device);
    }
    if (haveDecoder) {
        ma_decoder_uninit(&decoder);
    }
    return (result == MA_SUCCESS) ? 0 : -1;
}

int do_playback_tests()
{
    ma_bool32 hasErrorOccurred = MA_FALSE;

    for (size_t iBackend = 0; iBackend < ma_countof(g_Backends); ++iBackend) {
        int result = do_playback_test(g_Backends[iBackend]);
        if (result < 0) {
            hasErrorOccurred = MA_TRUE;
        }
    }

    return (hasErrorOccurred) ? -1 : 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    ma_bool32 hasErrorOccurred = MA_FALSE;
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
#if defined(__DMC__)
    printf("Compiler:     Digital Mars C++\n");
#endif

    // Print CPU features.
    if (ma_has_sse2()) {
        printf("Has SSE:      YES\n");
    } else {
        printf("Has SSE:      NO\n");
    }
    if (ma_has_avx2()) {
        printf("Has AVX2:     YES\n");
    } else {
        printf("Has AVX2:     NO\n");
    }
    if (ma_has_avx512f()) {
        printf("Has AVX-512F: YES\n");
    } else {
        printf("Has AVX-512F: NO\n");
    }
    if (ma_has_neon()) {
        printf("Has NEON:     YES\n");
    } else {
        printf("Has NEON:     NO\n");
    }


    // Aligned malloc/free
    printf("=== TESTING CORE ===\n");
    result = do_core_tests();
    if (result < 0) {
        hasErrorOccurred = MA_TRUE;
    }
    printf("=== END TESTING CORE ===\n");
    
    printf("\n");

    // Format Conversion
    printf("=== TESTING FORMAT CONVERSION ===\n");
    result = do_format_conversion_tests();
    if (result < 0) {
        hasErrorOccurred = MA_TRUE;
    }
    printf("=== END TESTING FORMAT CONVERSION ===\n");
    
    printf("\n");

    // Interleaving / Deinterleaving
    printf("=== TESTING INTERLEAVING/DEINTERLEAVING ===\n");
    result = do_interleaving_tests();
    if (result < 0) {
        hasErrorOccurred = MA_TRUE;
    }
    printf("=== END TESTING INTERLEAVING/DEINTERLEAVING ===\n");

    printf("\n");

    // ma_format_converter
    printf("=== TESTING FORMAT CONVERTER ===\n");
    result = do_format_converter_tests();
    if (result < 0) {
        hasErrorOccurred = MA_TRUE;
    }
    printf("=== END TESTING FORMAT CONVERTER ===\n");
    
    printf("\n");

    // Channel Routing
    printf("=== TESTING CHANNEL ROUTING ===\n");
    result = do_channel_routing_tests();
    if (result < 0) {
        hasErrorOccurred = MA_TRUE;
    }
    printf("=== END TESTING CHANNEL ROUTING ===\n");
    
    printf("\n");
    
    // Backends
    printf("=== TESTING BACKENDS ===\n");
    result = do_backend_tests();
    if (result < 0) {
        hasErrorOccurred = MA_TRUE;
    }
    printf("=== END TESTING BACKENDS ===\n");

    printf("\n");

    // Default Playback Devices
    printf("=== TESTING DEFAULT PLAYBACK DEVICES ===\n");
    result = do_playback_tests();
    if (result < 0) {
        hasErrorOccurred = MA_TRUE;
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

#ifdef MA_INCLUDE_VORBIS_TESTS
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
