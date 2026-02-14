#define MA_NO_PIPEWIRE
#if 1
#include "../../miniaudio.c"
#else
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio-11.h"
#endif

#ifdef MA_HAS_LIBSAMPLERATE
#include <samplerate.h>
#endif

#ifdef MA_HAS_SPEEXDSP
#include <speex/speex_resampler.h>
#endif

#ifdef MA_HAS_SOXR
#include <soxr.h>
#endif

#define RESAMPLING_LPF_ORDER            0
#define RESAMPLING_FRAMES_PER_ITERATION 1024 * 1024
#define RESAMPLING_ITERATION_COUNT      100

#define resampling_api_miniaudio     0
#define resampling_api_libsamplerate 1
#define resampling_api_speex         2
#define resampling_api_soxr          4

typedef int resampling_api;

const char* resampling_api_to_string(resampling_api api)
{
    if (api == resampling_api_miniaudio) {
        return "miniaudio";
    }
    if (api == resampling_api_libsamplerate) {
        return "libsamplerate";
    }
    if (api == resampling_api_speex) {
        return "speex";
    }
    if (api == resampling_api_soxr) {
        return "soxr";
    }

    MA_ASSERT(!"Resampling API needs to be added to resampling_api_to_string().");
    return "unknown";
}

/* Modify this to control the resampling API. Comment out to force miniaudio. */
/*#define RESAMPLING_LISTENING_TEST_API resampling_api_libsamplerate*/

#if defined(RESAMPLING_LISTENING_TEST_API)
    #if RESAMPLING_LISTENING_TEST_API == resampling_api_libsamplerate && !defined(MA_HAS_LIBSAMPLERATE)
        #error "libsamplerate is unavailable and cannot be used."
    #endif
#endif

#ifndef RESAMPLING_LISTENING_TEST_API
#define RESAMPLING_LISTENING_TEST_API resampling_api_miniaudio
#endif

typedef struct
{
    ma_resampler resampler;
    ma_waveform waveform;
    ma_uint8 cache[4096];
    ma_uint32 cachedFrameCount;
    ma_uint32 cachedFrameCap;
    float sampleRateStep;
    float sampleRateRatio;
    float minSampleRateRatio;
    float maxSampleRateRatio;
    #ifdef MA_HAS_LIBSAMPLERATE
    SRC_STATE* pLSRState;
    #endif
} resampler_data_callback_data;

static void resampler_data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    resampler_data_callback_data* pResamplerData = (resampler_data_callback_data*)pDevice->pUserData;
    ma_uint32 framesWritten;
    ma_uint32 bpf;

    bpf = ma_get_bytes_per_frame(pResamplerData->resampler.format, pResamplerData->resampler.channels);
    
    framesWritten = 0;
    while (framesWritten < frameCount) {
        /* We resample from the cache. */
        while (pResamplerData->cachedFrameCount > 0 && framesWritten < frameCount) {
            #if RESAMPLING_LISTENING_TEST_API == resampling_api_miniaudio
            {
                ma_uint64 frameCountIn;
                ma_uint64 frameCountOut;

                frameCountIn  = pResamplerData->cachedFrameCount;
                frameCountOut = frameCount - framesWritten;
                ma_resampler_process_pcm_frames(&pResamplerData->resampler, pResamplerData->cache, &frameCountIn, ma_offset_ptr(pFramesOut, framesWritten * bpf), &frameCountOut);

                MA_MOVE_MEMORY(pResamplerData->cache, ma_offset_ptr(pResamplerData->cache, frameCountIn * bpf), (pResamplerData->cachedFrameCount - frameCountIn) * bpf);
                pResamplerData->cachedFrameCount -= frameCountIn;
                framesWritten += frameCountOut;
            }
            #endif

            #if RESAMPLING_LISTENING_TEST_API == resampling_api_libsamplerate
            {
                SRC_DATA data;

                MA_ZERO_OBJECT(&data);
                data.data_in       = (const float*)pResamplerData->cache;
                data.data_out      = (float*)ma_offset_ptr(pFramesOut, framesWritten * bpf);
                data.input_frames  = pResamplerData->cachedFrameCount;
                data.output_frames = frameCount - framesWritten;
                data.src_ratio     = 1 / pResamplerData->sampleRateRatio;
                src_process(pResamplerData->pLSRState, &data);

                MA_MOVE_MEMORY(pResamplerData->cache, ma_offset_ptr(pResamplerData->cache, data.input_frames_used * bpf), (pResamplerData->cachedFrameCount - data.input_frames_used) * bpf);
                pResamplerData->cachedFrameCount -= data.input_frames_used;
                framesWritten += data.output_frames_gen;
            }
            #endif
        }

        /* Reload the cache if necessary. */
        if (pResamplerData->cachedFrameCount == 0) {
            ma_waveform_read_pcm_frames(&pResamplerData->waveform, pResamplerData->cache, pResamplerData->cachedFrameCap, NULL);
            pResamplerData->cachedFrameCount = pResamplerData->cachedFrameCap;
        }
    }

    /* Sweep the sample rate. We'll just adjust the output rate. */
    pResamplerData->sampleRateRatio += pResamplerData->sampleRateStep;
    /*  */ if (pResamplerData->sampleRateRatio > pResamplerData->maxSampleRateRatio) {
        pResamplerData->sampleRateRatio = pResamplerData->maxSampleRateRatio;
        pResamplerData->sampleRateStep = -pResamplerData->sampleRateStep;
    } else if (pResamplerData->sampleRateRatio < pResamplerData->minSampleRateRatio) {
        pResamplerData->sampleRateRatio = pResamplerData->minSampleRateRatio;
        pResamplerData->sampleRateStep = -pResamplerData->sampleRateStep;
    }

    #if RESAMPLING_LISTENING_TEST_API == resampling_api_miniaudio
    {
        ma_resampler_set_rate_ratio(&pResamplerData->resampler, pResamplerData->sampleRateRatio);
    }
    #endif
    #if RESAMPLING_LISTENING_TEST_API == resampling_api_libsamplerate
    {
        src_set_ratio(pResamplerData->pLSRState, 1 / pResamplerData->sampleRateRatio);
    }
    #endif

    (void)pFramesIn;
}

void resampler_listening_test(void)
{
    ma_result result;
    resampler_data_callback_data callbackData;
    ma_resampler_config resamplerConfig;
    ma_waveform_config waveformConfig;
    ma_device_config deviceConfig;
    ma_device device;
    ma_format format;
    ma_uint32 channels = 2;

    printf("Resampling Listening Test API: %s\n", resampling_api_to_string(RESAMPLING_LISTENING_TEST_API));

    #if RESAMPLING_LISTENING_TEST_API == resampling_api_miniaudio
    {
        format = ma_format_f32; /* Switch this between f32 and s16 as required when testing miniaudio. */
    }
    #endif
    #if RESAMPLING_LISTENING_TEST_API == resampling_api_libsamplerate
    {
        format = ma_format_f32; /* Can only use f32 with libsamplerate. */

        callbackData.pLSRState = src_new(SRC_SINC_FASTEST /*SRC_SINC_BEST_QUALITY*/, channels, NULL);
    }
    #endif

    resamplerConfig = ma_resampler_config_init(format, channels, 48000, 48000, ma_resample_algorithm_linear);
    resamplerConfig.linear.lpfOrder = RESAMPLING_LPF_ORDER;

    result = ma_resampler_init(&resamplerConfig, NULL, &callbackData.resampler);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize resampler.");
        return;
    }


    waveformConfig = ma_waveform_config_init(resamplerConfig.format, resamplerConfig.channels, resamplerConfig.sampleRateIn, ma_waveform_type_sine, 0.2f, 440);

    result = ma_waveform_init(&waveformConfig, &callbackData.waveform);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize waveform.");
        return;
    }


    callbackData.cachedFrameCount   = 0;
    callbackData.cachedFrameCap     = sizeof(callbackData.cache) / ma_get_bytes_per_frame(resamplerConfig.format, resamplerConfig.channels);

    
    /* Default. */
    #if 1
    callbackData.sampleRateStep     = 0.01f;
    callbackData.minSampleRateRatio = 0.2f;
    callbackData.maxSampleRateRatio = 5.0f;
    #endif

    /* Low frequency upsample. */
    #if 0
    callbackData.sampleRateStep     = 0.001f;
    callbackData.minSampleRateRatio = 0.01f;
    callbackData.maxSampleRateRatio = 0.09f;
    #endif

    /* High frequency downsample */
    #if 0
    callbackData.sampleRateStep     = 0.01f;
    callbackData.minSampleRateRatio = 4.0f;
    callbackData.maxSampleRateRatio = 6.0f;
    #endif

    #if 1
    callbackData.sampleRateStep     = 0.002f;
    callbackData.minSampleRateRatio = 0.002f;
    callbackData.maxSampleRateRatio = 0.2f;
    #endif

    callbackData.sampleRateRatio    = callbackData.minSampleRateRatio;
    


    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format    = resamplerConfig.format;
    deviceConfig.playback.channels  = resamplerConfig.channels;
    deviceConfig.sampleRate         = resamplerConfig.sampleRateIn;
    deviceConfig.periodSizeInFrames = /*2048; //*/256;
    deviceConfig.dataCallback       = resampler_data_callback;
    deviceConfig.pUserData          = &callbackData;
    

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize device.");
        return;
    }

    ma_device_start(&device);

    ma_sleep(100000);

    ma_device_uninit(&device);
    ma_waveform_uninit(&callbackData.waveform);
    ma_resampler_uninit(&callbackData.resampler, NULL);
}

static void profile_resampler_miniaudio(resampling_api api, ma_format format, ma_uint32 channels, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_uint32 lpfOrder)
{
    ma_result result;
    ma_timer timer;
    double startTime;
    void* pSamplesIn = NULL;
    void* pSamplesOut = NULL;
    ma_uint32 bpf = ma_get_bytes_per_frame(format, channels);
    int i;
    ma_uint32 framesOutCap = 1024;

    printf("%s: %s, %s, LPF %u, %u > %u: ", resampling_api_to_string(api), ((format == ma_format_s16) ? "s16" : "f32"), ((channels == 1) ? "mono" : "stereo"), lpfOrder, sampleRateIn, sampleRateOut);

    ma_timer_init(&timer);

    if (api == resampling_api_libsamplerate && format != ma_format_f32) {
        printf("Format not supported.\n");
        return;
    }

    pSamplesIn  = ma_malloc(RESAMPLING_FRAMES_PER_ITERATION * bpf, NULL);
    pSamplesOut = ma_malloc(framesOutCap                    * bpf, NULL);

    ma_debug_fill_pcm_frames_with_sine_wave((float*)pSamplesIn, RESAMPLING_FRAMES_PER_ITERATION, format, channels, sampleRateIn);   /* The float* cast is to work around an API bug in miniaudio v0.11. It's harmless. */

    if (api == resampling_api_miniaudio) {
        ma_linear_resampler_config resamplerConfig;
        ma_linear_resampler resampler;

        resamplerConfig = ma_linear_resampler_config_init(format, channels, sampleRateIn, sampleRateOut);
        resamplerConfig.lpfOrder = lpfOrder;

        result = ma_linear_resampler_init(&resamplerConfig, NULL, &resampler);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize resampler.\n");
            return;
        }

        startTime = ma_timer_get_time_in_seconds(&timer);
        {
            for (i = 0; i < RESAMPLING_ITERATION_COUNT; i += 1) {
                ma_uint64 totalFramesRead = 0;

                while (totalFramesRead < RESAMPLING_FRAMES_PER_ITERATION) {
                    ma_uint64 framesIn  = RESAMPLING_FRAMES_PER_ITERATION - totalFramesRead;
                    ma_uint64 framesOut = framesOutCap;

                    ma_linear_resampler_process_pcm_frames(&resampler, ma_offset_pcm_frames_ptr(pSamplesIn, totalFramesRead, format, channels), &framesIn, pSamplesOut, &framesOut);
                    totalFramesRead += framesIn;
                }
            }
        }
        printf("%f\n", ma_timer_get_time_in_seconds(&timer) - startTime);
    }
    
    if (api == resampling_api_libsamplerate) {
        #ifdef MA_HAS_LIBSAMPLERATE
        {
            #if 0
            SRC_SINC_BEST_QUALITY
            SRC_SINC_MEDIUM_QUALITY
            SRC_SINC_FASTEST
            SRC_ZERO_ORDER_HOLD
            SRC_LINEAR
            #endif

            SRC_STATE* pState = src_new(SRC_LINEAR, channels, NULL);

            startTime = ma_timer_get_time_in_seconds(&timer);
            {
                for (i = 0; i < RESAMPLING_ITERATION_COUNT; i += 1) {
                    ma_uint64 totalFramesRead = 0;

                    while (totalFramesRead < RESAMPLING_FRAMES_PER_ITERATION) {
                        SRC_DATA data;

                        data.data_in       = ma_offset_pcm_frames_ptr(pSamplesIn, totalFramesRead, format, channels);
                        data.data_out      = pSamplesOut;
                        data.input_frames  = RESAMPLING_FRAMES_PER_ITERATION - totalFramesRead;
                        data.output_frames = framesOutCap;
                        data.src_ratio     = (double)sampleRateOut / sampleRateIn;  /* I think libsamplerate's ratio is out/in, whereas miniaudio is in/out. Advice welcome if I am wrong about this. */
                        src_process(pState, &data);
                        
                        totalFramesRead += data.output_frames_gen;
                    }
                }
            }
            printf("%f\n", ma_timer_get_time_in_seconds(&timer) - startTime);
        }
        #else
        {
            printf("libsamplerate not available.\n");
        }
        #endif
    }

    if (api == resampling_api_speex) {
        #ifdef MA_HAS_SPEEXDSP
        {
            int err;
            SpeexResamplerState* pState = speex_resampler_init(channels, sampleRateIn, sampleRateOut, SPEEX_RESAMPLER_QUALITY_MIN, &err);

            if (err != RESAMPLER_ERR_SUCCESS) {
                printf("Failed to initialize speex resampler.\n");
                return;
            }

            startTime = ma_timer_get_time_in_seconds(&timer);
            {
                for (i = 0; i < RESAMPLING_ITERATION_COUNT; i += 1) {
                    ma_uint64 totalFramesRead = 0;

                    while (totalFramesRead < RESAMPLING_FRAMES_PER_ITERATION) {
                        spx_uint32_t in_len  = (spx_uint32_t)(RESAMPLING_FRAMES_PER_ITERATION - totalFramesRead);
                        spx_uint32_t out_len = (spx_uint32_t)framesOutCap;

                        if (format == ma_format_f32) {
                            speex_resampler_process_interleaved_float(pState, ma_offset_pcm_frames_ptr(pSamplesIn, totalFramesRead, format, channels), &in_len, pSamplesOut, &out_len);
                        } else {
                            speex_resampler_process_interleaved_int(pState, ma_offset_pcm_frames_ptr(pSamplesIn, totalFramesRead, format, channels), &in_len, pSamplesOut, &out_len);
                        }
                        
                        totalFramesRead += in_len;
                    }
                }
            }
            printf("%f\n", ma_timer_get_time_in_seconds(&timer) - startTime);

            speex_resampler_destroy(pState);
        }
        #else
        {
            printf("speex not available.\n");
        }
        #endif
    }

    if (api == resampling_api_soxr) {
        #ifdef MA_HAS_SOXR
        {
            soxr_error_t error;
            soxr_io_spec_t ioSpec;
            soxr_quality_spec_t qualitySpec;
            soxr_t pState;

            ioSpec = soxr_io_spec((format == ma_format_f32) ? SOXR_FLOAT32_I : SOXR_INT16_I, (format == ma_format_f32) ? SOXR_FLOAT32_I : SOXR_INT16_I);
            qualitySpec = soxr_quality_spec(SOXR_QQ, 0);
            pState = soxr_create((double)sampleRateIn, (double)sampleRateOut, channels, &error, &ioSpec, &qualitySpec, NULL);

            if (error != NULL) {
                printf("Failed to initialize soxr resampler.\n");
                return;
            }

            startTime = ma_timer_get_time_in_seconds(&timer);
            {
                for (i = 0; i < RESAMPLING_ITERATION_COUNT; i += 1) {
                    ma_uint64 totalFramesRead = 0;

                    while (totalFramesRead < RESAMPLING_FRAMES_PER_ITERATION) {
                        size_t in_len = RESAMPLING_FRAMES_PER_ITERATION - totalFramesRead;
                        size_t out_len = 0;

                        soxr_process(pState, ma_offset_pcm_frames_ptr(pSamplesIn, totalFramesRead, format, channels), in_len, &in_len, pSamplesOut, framesOutCap, &out_len);
                        
                        totalFramesRead += in_len;
                    }
                }
            }
            printf("%f\n", ma_timer_get_time_in_seconds(&timer) - startTime);

            soxr_delete(pState);
        }
        #else
        {
            printf("soxr not available.\n");
        }
        #endif
    }
}

void profile_resampler(void)
{
    unsigned int denormalsState;

    denormalsState = ma_disable_denormals();
    {
        #if 1
        /* miniaudio */
        profile_resampler_miniaudio(resampling_api_miniaudio, ma_format_s16, 1, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_miniaudio, ma_format_s16, 2, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_miniaudio, ma_format_f32, 1, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_miniaudio, ma_format_f32, 2, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_miniaudio, ma_format_s16, 1, 48000, 44100, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_miniaudio, ma_format_s16, 2, 48000, 44100, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_miniaudio, ma_format_f32, 1, 48000, 44100, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_miniaudio, ma_format_f32, 2, 48000, 44100, RESAMPLING_LPF_ORDER);

        /* libsamplerate */
        profile_resampler_miniaudio(resampling_api_libsamplerate, ma_format_f32, 1, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_libsamplerate, ma_format_f32, 2, 44100, 48000, RESAMPLING_LPF_ORDER);

        /* speex */
        profile_resampler_miniaudio(resampling_api_speex, ma_format_s16, 1, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_speex, ma_format_s16, 2, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_speex, ma_format_f32, 1, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_speex, ma_format_f32, 2, 44100, 48000, RESAMPLING_LPF_ORDER);

        /* soxr */
        profile_resampler_miniaudio(resampling_api_soxr, ma_format_s16, 1, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_soxr, ma_format_s16, 2, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_soxr, ma_format_f32, 1, 44100, 48000, RESAMPLING_LPF_ORDER);
        profile_resampler_miniaudio(resampling_api_soxr, ma_format_f32, 2, 44100, 48000, RESAMPLING_LPF_ORDER);
        #endif
    }
    ma_restore_denormals(denormalsState);


    /* Finish off with a listening test. */
    resampler_listening_test();
}

int main(int argc, char** argv)
{
    profile_resampler();

    (void)argc;
    (void)argv;

    return 0;
}