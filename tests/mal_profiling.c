#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

typedef enum
{
    simd_mode_scalar = 0,
    simd_mode_sse2,
    simd_mode_avx,
    simd_mode_avx512,
    simd_mode_neon
} simd_mode;

const char* simd_mode_to_string(simd_mode mode)
{
    switch (mode) {
        case simd_mode_scalar: return "Reference";
        case simd_mode_sse2:    return "SSE2";
        case simd_mode_avx:    return "AVX";
        case simd_mode_avx512: return "AVX-512";
        case simd_mode_neon:   return "NEON";
    }

    return "Unknown";
}

const char* mal_src_algorithm_to_string(mal_src_algorithm algorithm)
{
    switch (algorithm) {
        case mal_src_algorithm_none:   return "Passthrough";
        case mal_src_algorithm_linear: return "Linear";
        case mal_src_algorithm_sinc:   return "Sinc";
    }

    return "Unknown";
}


float g_ChannelRouterProfilingOutputBenchmark[8][48000];
float g_ChannelRouterProfilingOutput[8][48000];
double g_ChannelRouterTime_Reference = 0;
double g_ChannelRouterTime_SSE2 = 0;
double g_ChannelRouterTime_AVX = 0;
double g_ChannelRouterTime_AVX512 = 0;
double g_ChannelRouterTime_NEON = 0;

mal_sine_wave g_sineWave;

mal_bool32 channel_router_test(mal_uint32 channels, mal_uint64 frameCount, float** ppFramesA, float** ppFramesB)
{
    for (mal_uint32 iChannel = 0; iChannel < channels; ++iChannel) {
        for (mal_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
            if (ppFramesA[iChannel][iFrame] != ppFramesB[iChannel][iFrame]) {
                return MAL_FALSE;
            }
        }
    }

    return MAL_TRUE;
}

mal_uint32 channel_router_on_read(mal_channel_router* pRouter, mal_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    (void)pUserData;
    (void)pRouter;

    float** ppSamplesOutF = (float**)ppSamplesOut;

    for (mal_uint32 iChannel = 0; iChannel < pRouter->config.channelsIn; ++iChannel) {
        mal_sine_wave_init(1/(iChannel+1), 400, 48000, &g_sineWave);
        mal_sine_wave_read(&g_sineWave, frameCount, ppSamplesOutF[iChannel]);
    }

    return frameCount;
}

int do_profiling__channel_routing()
{
    mal_result result;

    // When profiling we need to compare against a benchmark to ensure the optimization is implemented correctly. We always
    // use the reference implementation for our benchmark.
    mal_uint32 channels = mal_countof(g_ChannelRouterProfilingOutputBenchmark);
    mal_channel channelMapIn[MAL_MAX_CHANNELS];
    mal_get_standard_channel_map(mal_standard_channel_map_default, channels, channelMapIn);
    mal_channel channelMapOut[MAL_MAX_CHANNELS];
    mal_get_standard_channel_map(mal_standard_channel_map_default, channels, channelMapOut);

    mal_channel_router_config routerConfig = mal_channel_router_config_init(channels, channelMapIn, channels, channelMapOut, mal_channel_mix_mode_planar_blend, channel_router_on_read, NULL);

    mal_channel_router router;
    result = mal_channel_router_init(&routerConfig, &router);
    if (result != MAL_SUCCESS) {
        return -1;
    }

    // Disable optimizations for our tests.
    router.isPassthrough = MAL_FALSE;
    router.isSimpleShuffle = MAL_FALSE;
    router.useSSE2 = MAL_FALSE;
    router.useAVX = MAL_FALSE;
    router.useAVX512 = MAL_FALSE;
    router.useNEON = MAL_FALSE;

    mal_uint64 framesToRead = mal_countof(g_ChannelRouterProfilingOutputBenchmark[0]);

    // Benchmark
    void* ppOutBenchmark[8];
    for (int i = 0; i < 8; ++i) {
        ppOutBenchmark[i] = (void*)g_ChannelRouterProfilingOutputBenchmark[i];
    }

    mal_sine_wave_init(1, 400, 48000, &g_sineWave);
    mal_uint64 framesRead = mal_channel_router_read_deinterleaved(&router, framesToRead, ppOutBenchmark, NULL);
    if (framesRead != framesToRead) {
        printf("Channel Router: An error occurred while reading benchmark data.\n");
    }

    void* ppOut[8];
    for (int i = 0; i < 8; ++i) {
        ppOut[i] = (void*)g_ChannelRouterProfilingOutput[i];
    }


    printf("Channel Routing\n");
    printf("===============\n");

    // Reference
    {
        mal_timer timer;
        mal_timer_init(&timer);
        double startTime = mal_timer_get_time_in_seconds(&timer);

        framesRead = mal_channel_router_read_deinterleaved(&router, framesToRead, ppOut, NULL);
        if (framesRead != framesToRead) {
            printf("Channel Router: An error occurred while reading reference data.\n");
        }

        if (!channel_router_test(channels, framesRead, (float**)ppOutBenchmark, (float**)ppOut)) {
            printf("  [ERROR] ");
        } else {
            printf("  [PASSED] ");
        }

        g_ChannelRouterTime_Reference = mal_timer_get_time_in_seconds(&timer) - startTime;
        printf("Reference: %.4fms (%.2f%%)\n", g_ChannelRouterTime_Reference*1000, g_ChannelRouterTime_Reference/g_ChannelRouterTime_Reference*100);
    }

    // SSE2
    if (mal_has_sse2()) {
        router.useSSE2 = MAL_TRUE;
        mal_timer timer;
        mal_timer_init(&timer);
        double startTime = mal_timer_get_time_in_seconds(&timer);

        framesRead = mal_channel_router_read_deinterleaved(&router, framesToRead, ppOut, NULL);
        if (framesRead != framesToRead) {
            printf("Channel Router: An error occurred while reading SSE2 data.\n");
        }

        g_ChannelRouterTime_SSE2 = mal_timer_get_time_in_seconds(&timer) - startTime;
        router.useSSE2 = MAL_FALSE;

        if (!channel_router_test(channels, framesRead, (float**)ppOutBenchmark, (float**)ppOut)) {
            printf("  [ERROR] ");
        } else {
            printf("  [PASSED] ");
        }

        printf("SSE2: %.4fms (%.2f%%)\n", g_ChannelRouterTime_SSE2*1000, g_ChannelRouterTime_Reference/g_ChannelRouterTime_SSE2*100);
    }

    // AVX
    if (mal_has_avx()) {
        router.useAVX = MAL_TRUE;
        mal_timer timer;
        mal_timer_init(&timer);
        double startTime = mal_timer_get_time_in_seconds(&timer);

        framesRead = mal_channel_router_read_deinterleaved(&router, framesToRead, ppOut, NULL);
        if (framesRead != framesToRead) {
            printf("Channel Router: An error occurred while reading AVX data.\n");
        }

        g_ChannelRouterTime_AVX = mal_timer_get_time_in_seconds(&timer) - startTime;
        router.useAVX = MAL_FALSE;

        if (!channel_router_test(channels, framesRead, (float**)ppOutBenchmark, (float**)ppOut)) {
            printf("  [ERROR] ");
        } else {
            printf("  [PASSED] ");
        }

        printf("AVX: %.4fms (%.2f%%)\n", g_ChannelRouterTime_AVX*1000, g_ChannelRouterTime_Reference/g_ChannelRouterTime_AVX*100);
    }

    // NEON
    if (mal_has_neon()) {
        router.useNEON = MAL_TRUE;
        mal_timer timer;
        mal_timer_init(&timer);
        double startTime = mal_timer_get_time_in_seconds(&timer);

        framesRead = mal_channel_router_read_deinterleaved(&router, framesToRead, ppOut, NULL);
        if (framesRead != framesToRead) {
            printf("Channel Router: An error occurred while reading NEON data.\n");
        }

        g_ChannelRouterTime_NEON = mal_timer_get_time_in_seconds(&timer) - startTime;
        router.useNEON = MAL_FALSE;

        if (!channel_router_test(channels, framesRead, (float**)ppOutBenchmark, (float**)ppOut)) {
            printf("  [ERROR] ");
        } else {
            printf("  [PASSED] ");
        }

        printf("NEON: %.4fms (%.2f%%)\n", g_ChannelRouterTime_NEON*1000, g_ChannelRouterTime_Reference/g_ChannelRouterTime_NEON*100);
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//
// SRC
//
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
    float* pFrameData[MAL_MAX_CHANNELS];
    mal_uint64 frameCount;
    mal_uint32 channels;
    double timeTaken;
} src_reference_data;

typedef struct
{
    float* pFrameData[MAL_MAX_CHANNELS];
    mal_uint64 frameCount;
    mal_uint64 iNextFrame;
    mal_uint32 channels;
} src_data;

mal_uint32 do_profiling__src__on_read(mal_src* pSRC, mal_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    src_data* pBaseData = (src_data*)pUserData;
    mal_assert(pBaseData != NULL);
    mal_assert(pBaseData->iNextFrame <= pBaseData->frameCount);

    mal_uint64 framesToRead = frameCount;

    mal_uint64 framesAvailable = pBaseData->frameCount - pBaseData->iNextFrame;
    if (framesToRead > framesAvailable) {
        framesToRead = framesAvailable;
    }

    if (framesToRead > 0) {
        for (mal_uint32 iChannel = 0; iChannel < pSRC->config.channels; iChannel += 1) {
            mal_copy_memory(ppSamplesOut[iChannel], pBaseData->pFrameData[iChannel], (size_t)(framesToRead * sizeof(float)));
        }
    }

    pBaseData->iNextFrame += framesToRead;
    return (mal_uint32)framesToRead;
}

mal_result init_src(src_data* pBaseData, mal_uint32 sampleRateIn, mal_uint32 sampleRateOut, mal_src_algorithm algorithm, simd_mode mode, mal_src* pSRC)
{
    mal_assert(pBaseData != NULL);
    mal_assert(pSRC != NULL);

    mal_src_config srcConfig = mal_src_config_init(sampleRateIn, sampleRateOut, pBaseData->channels, do_profiling__src__on_read, pBaseData);
    srcConfig.sinc.windowWidth = 17;    // <-- Make this an odd number to test unaligned section in the SIMD implementations.
    srcConfig.algorithm = algorithm;
    srcConfig.noSSE2    = MAL_TRUE;
    srcConfig.noAVX     = MAL_TRUE;
    srcConfig.noAVX512  = MAL_TRUE;
    srcConfig.noNEON    = MAL_TRUE;
    switch (mode) {
        case simd_mode_sse2:   srcConfig.noSSE2   = MAL_FALSE; break;
        case simd_mode_avx:    srcConfig.noAVX    = MAL_FALSE; break;
        case simd_mode_avx512: srcConfig.noAVX512 = MAL_FALSE; break;
        case simd_mode_neon:   srcConfig.noNEON   = MAL_FALSE; break;
        case simd_mode_scalar:
        default: break;
    }

    mal_result result = mal_src_init(&srcConfig, pSRC);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize sample rate converter.\n");
        return (int)result;
    }

    return result;
}

int do_profiling__src__profile_individual(src_data* pBaseData, mal_uint32 sampleRateIn, mal_uint32 sampleRateOut, mal_src_algorithm algorithm, simd_mode mode, src_reference_data* pReferenceData)
{
    mal_assert(pBaseData != NULL);
    mal_assert(pReferenceData != NULL);

    mal_result result = MAL_ERROR;

    // Make sure the base data is moved back to the start.
    pBaseData->iNextFrame = 0;

    mal_src src;
    result = init_src(pBaseData, sampleRateIn, sampleRateOut, algorithm, mode, &src);
    if (result != MAL_SUCCESS) {
        return (int)result;
    }


    // Profiling.
    mal_uint64 sz = pReferenceData->frameCount * sizeof(float);
    mal_assert(sz <= SIZE_MAX);

    float* pFrameData[MAL_MAX_CHANNELS];
    for (mal_uint32 iChannel = 0; iChannel < pBaseData->channels; iChannel += 1) {
        pFrameData[iChannel] = (float*)mal_aligned_malloc((size_t)sz, MAL_SIMD_ALIGNMENT);
        if (pFrameData[iChannel] == NULL) {
            printf("Out of memory.\n");
            return -2;
        }
        mal_zero_memory(pFrameData[iChannel], (size_t)sz);
    }

    mal_timer timer;
    mal_timer_init(&timer);
    double startTime = mal_timer_get_time_in_seconds(&timer);
    {
        mal_src_read_deinterleaved(&src, pReferenceData->frameCount, (void**)pFrameData, pBaseData);
    }
    double timeTaken = mal_timer_get_time_in_seconds(&timer) - startTime;


    // Correctness test.
    mal_bool32 passed = MAL_TRUE;
    for (mal_uint32 iChannel = 0; iChannel < pReferenceData->channels; iChannel += 1) {
        for (mal_uint32 iFrame = 0; iFrame < pReferenceData->frameCount; iFrame += 1) {
            float s0 = pReferenceData->pFrameData[iChannel][iFrame];
            float s1 =                 pFrameData[iChannel][iFrame];
            if (s0 != s1) {
                printf("(Channel %d, Sample %d) %f != %f\n", iChannel, iFrame, s0, s1);
                passed = MAL_FALSE;
            }
        }
    }


    // Print results.
    if (passed) {
        printf("  [PASSED] ");
    } else {
        printf("  [FAILED] ");
    }
    printf("%s %d -> %d (%s): %.4fms (%.2f%%)\n", mal_src_algorithm_to_string(algorithm), sampleRateIn, sampleRateOut, simd_mode_to_string(mode), timeTaken*1000, pReferenceData->timeTaken/timeTaken*100);


    for (mal_uint32 iChannel = 0; iChannel < pBaseData->channels; iChannel += 1) {
        mal_aligned_free(pFrameData[iChannel]);
    }

    return (int)result;
}

int do_profiling__src__profile_set(src_data* pBaseData, mal_uint32 sampleRateIn, mal_uint32 sampleRateOut, mal_src_algorithm algorithm)
{
    mal_assert(pBaseData != NULL);

    // Make sure the base data is back at the start.
    pBaseData->iNextFrame = 0;

    src_reference_data referenceData;
    mal_zero_object(&referenceData);
    referenceData.channels = pBaseData->channels;

    // The first thing to do is to perform a sample rate conversion using the scalar/reference implementation. This reference is used to compare
    // the results of the optimized implementation.
    referenceData.frameCount = mal_calculate_frame_count_after_src(sampleRateOut, sampleRateIn, pBaseData->frameCount);
    if (referenceData.frameCount == 0) {
        printf("Failed to calculate output frame count.\n");
        return -1;
    }

    mal_uint64 sz = referenceData.frameCount * sizeof(float);
    mal_assert(sz <= SIZE_MAX);

    for (mal_uint32 iChannel = 0; iChannel < referenceData.channels; iChannel += 1) {
        referenceData.pFrameData[iChannel] = (float*)mal_aligned_malloc((size_t)sz, MAL_SIMD_ALIGNMENT);
        if (referenceData.pFrameData[iChannel] == NULL) {
            printf("Out of memory.\n");
            return -2;
        }
        mal_zero_memory(referenceData.pFrameData[iChannel], (size_t)sz);
    }


    // Generate the reference data.
    mal_src src;
    mal_result result = init_src(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_scalar, &src);
    if (result != MAL_SUCCESS) {
        return (int)result;
    }

    mal_timer timer;
    mal_timer_init(&timer);
    double startTime = mal_timer_get_time_in_seconds(&timer);
    {
        mal_src_read_deinterleaved(&src, referenceData.frameCount, (void**)referenceData.pFrameData, pBaseData);
    }
    referenceData.timeTaken = mal_timer_get_time_in_seconds(&timer) - startTime;


    // Now that we have the reference data to compare against we can go ahead and measure the SIMD optimizations.
    if (mal_has_sse2()) {
        do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_sse2, &referenceData);
    }
    if (mal_has_avx()) {
        do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_avx, &referenceData);
    }
    if (mal_has_avx512f()) {
        do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_avx512, &referenceData);
    }
    if (mal_has_neon()) {
        do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_neon, &referenceData);
    }
    

    for (mal_uint32 iChannel = 0; iChannel < referenceData.channels; iChannel += 1) {
        mal_aligned_free(referenceData.pFrameData[iChannel]);
    }

    return 0;
}

int do_profiling__src()
{
    printf("Sample Rate Conversion\n");
    printf("======================\n");

    // Set up base data.
    src_data baseData;
    mal_zero_object(&baseData);
    baseData.channels = 8;
    baseData.frameCount = 10000;
    for (mal_uint32 iChannel = 0; iChannel < baseData.channels; ++iChannel) {
        baseData.pFrameData[iChannel] = (float*)mal_aligned_malloc((size_t)(baseData.frameCount * sizeof(float)), MAL_SIMD_ALIGNMENT);
        if (baseData.pFrameData[iChannel] == NULL) {
            printf("Out of memory.\n");
            return -1;
        }

        mal_sine_wave sineWave;
        mal_sine_wave_init(1.0f, 400 + (iChannel*50), 48000, &sineWave);
        mal_sine_wave_read(&sineWave, baseData.frameCount, baseData.pFrameData[iChannel]);
    }
    

    // Upsampling.
    do_profiling__src__profile_set(&baseData, 44100, 48000, mal_src_algorithm_sinc);

    // Downsampling.
    do_profiling__src__profile_set(&baseData, 48000, 44100, mal_src_algorithm_sinc);


    for (mal_uint32 iChannel = 0; iChannel < baseData.channels; iChannel += 1) {
        mal_aligned_free(baseData.pFrameData[iChannel]);
    }

    return 0;
}


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Summary.
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


    printf("\n");

    // Channel routing.
    do_profiling__channel_routing();
    printf("\n\n");

    // Sample rate conversion.
    do_profiling__src();
    printf("\n\n");


    printf("Press any key to quit...\n");
    getchar();

    return 0;
}