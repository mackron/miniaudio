#define MAL_IMPLEMENTATION
#include "../mini_al.h"

float g_ChannelRouterProfilingOutputBenchmark[8][480000];
float g_ChannelRouterProfilingOutput[8][480000];
double g_ChannelRouterTime_Reference = 0;
double g_ChannelRouterTime_SSE2 = 0;
double g_ChannelRouterTime_AVX = 0;
double g_ChannelRouterTime_AVX512 = 0;
double g_ChannelRouterTime_NEON = 0;

mal_sine_wave sineWave;

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
        mal_sine_wave_init(1/(iChannel+1), 400, 48000, &sineWave);
        mal_sine_wave_read(&sineWave, frameCount, ppSamplesOutF[iChannel]);
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

    mal_sine_wave_init(1, 400, 48000, &sineWave);
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

    return 1;
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

    getchar();

    return 0;
}