/* Resampling research. Public domain. */

#ifndef ma_resampler_h
#define ma_resampler_h

#include "ma_lpf.h"


/**************************************************************************************************************************************************************

Resampling
==========
Resampling is achieved with the `ma_resampler` object. To create a resampler object, do something like the following:

    ```c
    ma_resampler_config config = ma_resampler_config_init(ma_format_s16, channels, sampleRateIn, sampleRateOut, ma_resample_algorithm_linear);
    ma_resampler resampler;
    ma_result result = ma_resampler_init(&config, &resampler);
    if (result != MA_SUCCESS) {
        // An error occurred...
    }
    ```

Do the following to uninitialize the resampler:

    ```c
    ma_resampler_uninit(&resampler);
    ```

The following example shows how data can be processed

    ```c
    ma_uint64 frameCountIn  = 1000;
    ma_uint64 frameCountOut = 2000;
    ma_result result = ma_resampler_process_pcm_frames(&resampler, pFramesIn, &frameCountIn, pFramesOut, &frameCountOut);
    if (result != MA_SUCCESS) {
        // An error occurred...
    }

    // At this point, frameCountIn contains the number of input frames that were consumed and frameCountOut contains the number of output frames written.
    ```

To initialize the resampler you first need to set up a config (`ma_resampler_config`) with `ma_resampler_config_init()`. You need to specify the sample format
you want to use, the number of channels, the input and output sample rate, and the algorithm.

The sample format can be one of the following:
  - ma_format_s16
  - ma_format_f32

If you need a different format you will need to perform pre- and post-conversions yourself where necessary. Note that the format is the same for both input
and output. The format cannot be changed after initialization.

The resampler supports multiple channels and is always interleaved (both input and output). The channel count cannot be changed after initialization.

The sample rates can be anything other than zero, and are always specified in hertz. They should be set to something like 44100, etc. The sample rate is the
only configuration property that can be changed after initialization.

The miniaudio resampler supports multiple algorithms:
  - Linear: ma_resample_algorithm_linear
  - Speex:  ma_resample_algorithm_speex

Because Speex is not public domain it is opt-in and the code is stored in separate files. To enable the Speex backend, you must first #include the following
file before the implementation of miniaudio.h:

    #include "extras/speex_resampler/ma_speex_resampler.h"

Both the header and implementation is contained within the same file. To implementation can be included in your program like so:

    #define MINIAUDIO_SPEEX_RESAMPLER_IMPLEMENTATION
    #include "extras/speex_resampler/ma_speex_resampler.h"

Note that if you opt-in to the Speex backend it will be used by default for all internal data conversion by miniaudio. Otherwise the linear implementation will
be used. To use a different backend, even when you have opted in to the Speex resampler, you can change the algorithm in the respective config of the object
you are initializating. If you try to use the Speex resampler without opting in, initialization of the `ma_resampler` object will fail with `MA_NO_BACKEND`.

Note that if you opt-in to the Speex backend you will need to consider it's license, the text of which can be found in the files in "extras/speex_resampler".

The algorithm cannot be changed after initialization.

Processing always happens on a per PCM frame basis and always assumed interleaved input and output. De-interleaved processing is not supported. To process
frames, use `ma_resampler_process_pcm_frames_pcm_frames()`. On input, this function takes the number of output frames you can fit in the output buffer and
the number of input frames contained in the input buffer. On output these variables contain the number of output frames that were written to the output buffer
and the number of input frames that were consumed in the process. You can pass in NULL for the input buffer in which case it will be treated as an infinitely
large buffer of zeros. The output buffer can also be NULL, in which case the processing will be treated as seek.

The sample rate can be changed dynamically on the fly. You can change this with explicit sample rates with `ma_resampler_set_rate()` and also with a decimal
ratio with `ma_resampler_set_rate_ratio()`. The ratio is in/out.

Sometimes it's useful to know exactly how many input frames will be required to output a specific number of frames. You can calculate this with
`ma_resampler_get_required_input_frame_count()`. Likewise, it's sometimes useful to know exactly how many frames would be output given a certain number of
input frames. You can do this with `ma_resampler_get_expected_output_frame_count()`.

Due to the nature of how resampling works, the resampler introduces some latency. This can be retrieved in terms of both the input rate and the output rate
with `ma_resampler_get_input_latency()` and `ma_resampler_get_output_latency()`.


Resampling Algorithms
---------------------
The choice of resampling algorithm depends on your situation and requirements. The linear resampler is poor quality, but fast with extremely low latency. The
Speex resampler is higher quality, but slower with more latency. It also performs a malloc() internally for memory management.

**************************************************************************************************************************************************************/

typedef enum
{
    ma_resample_algorithm_linear,   /* Fastest, lowest quality. Optional low-pass filtering. */
    ma_resample_algorithm_speex
} ma_resample_algorithm;

typedef struct
{
    ma_format format;   /* Must be either ma_format_f32 or ma_format_s16. */
    ma_uint32 channels;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    ma_resample_algorithm algorithm;
    struct
    {
        ma_bool32 enableLPF;
        ma_uint32 lpfCutoffFrequency;
    } linear;
    struct
    {
        int quality;    /* 0 to 10. Defaults to 3. */
    } speex;
} ma_resampler_config;

ma_resampler_config ma_resampler_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_resample_algorithm algorithm);

typedef struct
{
    ma_resampler_config config;
    union
    {
        struct
        {
            float t;    /* Input time, relative to x0. */
            union
            {
                float    f32[MA_MAX_CHANNELS];
                ma_int16 s16[MA_MAX_CHANNELS];
            } x0; /* The previous input frame. */
            union
            {
                float    f32[MA_MAX_CHANNELS];
                ma_int16 s16[MA_MAX_CHANNELS];
            } x1; /* The next input frame. */
            ma_lpf lpf;
        } linear;
        struct
        {
            void* pSpeexResamplerState;   /* SpeexResamplerState* */
        } speex;
    } state;
} ma_resampler;

/*
Initializes a new resampler object from a config.
*/
ma_result ma_resampler_init(const ma_resampler_config* pConfig, ma_resampler* pResampler);

/*
Uninitializes a resampler.
*/
void ma_resampler_uninit(ma_resampler* pResampler);

/*
Converts the given input data.

Both the input and output frames must be in the format specified in the config when the resampler was initilized.

On input, [pFrameCountOut] contains the number of output frames to process. On output it contains the number of output frames that
were actually processed, which may be less than the requested amount which will happen if there's not enough input data. You can use
ma_resampler_get_expected_output_frame_count() to know how many output frames will be processed for a given number of input frames.

On input, [pFrameCountIn] contains the number of input frames contained in [pFramesIn]. On output it contains the number of whole
input frames that were actually processed. You can use ma_resampler_get_required_input_frame_count() to know how many input frames
you should provide for a given number of output frames. [pFramesIn] can be NULL, in which case zeroes will be used instead.

If [pFramesOut] is NULL, a seek is performed. In this case, if [pFrameCountOut] is not NULL it will seek by the specified number of
output frames. Otherwise, if [pFramesCountOut] is NULL and [pFrameCountIn] is not NULL, it will seek by the specified number of input
frames. When seeking, [pFramesIn] is allowed to NULL, in which case the internal timing state will be updated, but no input will be
processed. In this case, any internal filter state will be updated as if zeroes were passed in.

It is an error for [pFramesOut] to be non-NULL and [pFrameCountOut] to be NULL.

It is an error for both [pFrameCountOut] and [pFrameCountIn] to be NULL.
*/
ma_result ma_resampler_process_pcm_frames(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);


/*
Sets the input and output sample sample rate.
*/
ma_result ma_resampler_set_rate(ma_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);

/*
Sets the input and output sample rate as a ratio.

The ration is in/out.
*/
ma_result ma_resampler_set_rate_ratio(ma_resampler* pResampler, float ratio);


/*
Calculates the number of whole input frames that would need to be read from the client in order to output the specified
number of output frames.

The returned value does not include cached input frames. It only returns the number of extra frames that would need to be
read from the input buffer in order to output the specified number of output frames.
*/
ma_uint64 ma_resampler_get_required_input_frame_count(ma_resampler* pResampler, ma_uint64 outputFrameCount);

/*
Calculates the number of whole output frames that would be output after fully reading and consuming the specified number of
input frames.
*/
ma_uint64 ma_resampler_get_expected_output_frame_count(ma_resampler* pResampler, ma_uint64 inputFrameCount);


/*
Retrieves the latency introduced by the resampler in input frames.
*/
ma_uint64 ma_resampler_get_input_latency(ma_resampler* pResampler);

/*
Retrieves the latency introduced by the resampler in output frames.
*/
ma_uint64 ma_resampler_get_output_latency(ma_resampler* pResampler);


#endif  /* ma_resampler_h */

/*
Implementation
*/
#ifdef MINIAUDIO_IMPLEMENTATION

#ifndef MA_RESAMPLER_MIN_RATIO
#define MA_RESAMPLER_MIN_RATIO 0.02083333
#endif
#ifndef MA_RESAMPLER_MAX_RATIO
#define MA_RESAMPLER_MAX_RATIO 48.0
#endif

#if defined(ma_speex_resampler_h)
#define MA_HAS_SPEEX_RESAMPLER

static ma_result ma_result_from_speex_err(int err)
{
    switch (err)
    {
        case RESAMPLER_ERR_SUCCESS:      return MA_SUCCESS;
        case RESAMPLER_ERR_ALLOC_FAILED: return MA_OUT_OF_MEMORY;
        case RESAMPLER_ERR_BAD_STATE:    return MA_ERROR;
        case RESAMPLER_ERR_INVALID_ARG:  return MA_INVALID_ARGS;
        case RESAMPLER_ERR_PTR_OVERLAP:  return MA_INVALID_ARGS;
        case RESAMPLER_ERR_OVERFLOW:     return MA_ERROR;
        default: return MA_ERROR;
    }
}
#endif

ma_resampler_config ma_resampler_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_resample_algorithm algorithm)
{
    ma_resampler_config config;

    MA_ZERO_OBJECT(&config);
    config.format = format;
    config.channels = channels;
    config.sampleRateIn = sampleRateIn;
    config.sampleRateOut = sampleRateOut;
    config.algorithm = algorithm;
    config.speex.quality = 3;   /* Cannot leave this as 0 as that is actually a valid value for Speex resampling quality. */

    return config;
}

static ma_result ma_resampler__init_lpf(ma_resampler* pResampler)
{
    ma_result result;
    ma_lpf_config lpfConfig;

    pResampler->state.linear.t = -1;    /* This must be set to -1 for the linear backend. It's used to indicate that the first frame needs to be loaded. */

    lpfConfig = ma_lpf_config_init(pResampler->config.format, pResampler->config.channels, pResampler->config.sampleRateOut, pResampler->config.linear.lpfCutoffFrequency);
    if (lpfConfig.cutoffFrequency == 0) {
        lpfConfig.cutoffFrequency = ma_min(pResampler->config.sampleRateIn, pResampler->config.sampleRateOut) / 2;
    }

    result = ma_lpf_init(&lpfConfig, &pResampler->state.linear.lpf);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

ma_result ma_resampler_init(const ma_resampler_config* pConfig, ma_resampler* pResampler)
{
    ma_result result;

    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pResampler);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->format != ma_format_f32 && pConfig->format != ma_format_s16) {
        return MA_INVALID_ARGS;
    }

    pResampler->config = *pConfig;

    switch (pConfig->algorithm)
    {
        case ma_resample_algorithm_linear:
        {
            /* We need to initialize the time to -1 so that ma_resampler_process_pcm_frames() can know that it needs to load the buffer with an initial frame. */
            pResampler->state.linear.t = -1;

            if (pConfig->linear.enableLPF) {
                result = ma_resampler__init_lpf(pResampler);
                if (result != MA_SUCCESS) {
                    return result;
                }
            }
        } break;

        case ma_resample_algorithm_speex:
        {
        #if defined(MA_HAS_SPEEX_RESAMPLER)
            int speexErr;
            pResampler->state.speex.pSpeexResamplerState = speex_resampler_init(pConfig->channels, pConfig->sampleRateIn, pConfig->sampleRateOut, pConfig->speex.quality, &speexErr);
            if (pResampler->state.speex.pSpeexResamplerState == NULL) {
                return ma_result_from_speex_err(speexErr);
            }
        #else
            /* Speex resampler not available. */
            return MA_NO_BACKEND;
        #endif
        } break;

        default: return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

void ma_resampler_uninit(ma_resampler* pResampler)
{
    if (pResampler == NULL) {
        return;
    }

#if defined(MA_HAS_SPEEX_RESAMPLER)
    if (pResampler->config.algorithm == ma_resample_algorithm_speex) {
        speex_resampler_destroy(pResampler->state.speex.pSpeexResamplerState);
    }
#endif
}

static ma_result ma_resampler_process_pcm_frames__read__linear(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_uint64 frameCountOut;
    ma_uint64 frameCountIn;
    ma_uint64 iFrameOut;
    ma_uint64 iFrameIn;
    ma_uint64 iChannel;
    float ratioInOut;

    /* */ float* pYF32 = (      float*)pFramesOut;
    const float* pXF32 = (const float*)pFramesIn;

    /* */ ma_int16* pYS16 = (      ma_int16*)pFramesOut;
    const ma_int16* pXS16 = (const ma_int16*)pFramesIn;

    MA_ASSERT(pResampler     != NULL);
    MA_ASSERT(pFramesOut     != NULL);
    MA_ASSERT(pFrameCountOut != NULL);
    MA_ASSERT(pFrameCountIn  != NULL);

    frameCountOut = *pFrameCountOut;
    frameCountIn  = *pFrameCountIn;

    if (frameCountOut == 0) {
        return MA_INVALID_ARGS; /* Nothing to do. */
    }

    ratioInOut = (float)pResampler->config.sampleRateIn / (float)pResampler->config.sampleRateOut;

    iFrameOut = 0;
    iFrameIn  = 0;

    /*
    We need to do an initial load of input data so that the first output frame is the same as the input frame. We can know whether or not to do this by
    checking whether or not the current time is < 0 (it will be initialized to -1).
    */
    if (pResampler->state.linear.t < 0) {
        if (frameCountIn > 0) {
            for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                if (pResampler->config.format == ma_format_f32) {
                    if (pXF32 != NULL) {
                        pResampler->state.linear.x1.f32[iChannel] = pXF32[iChannel];
                    } else {
                        pResampler->state.linear.x1.f32[iChannel] = 0;
                    }
                } else {
                    if (pXS16 != NULL) {
                        pResampler->state.linear.x1.s16[iChannel] = pXS16[iChannel];
                    } else {
                        pResampler->state.linear.x1.s16[iChannel] = 0;
                    }
                }
            }
            iFrameIn += 1;

            pResampler->state.linear.t = 1; /* Important that we set this to 1. This will cause the logic below to load the _second_ frame so we can do correct interpolation. */
        }
    }

    for (;;) {
        /* We can't interpolate if our interpolation factor (time relative to x0) is greater than 1. */
        if (pResampler->state.linear.t > 1) {
            /* Need to load the next input frame. */
            iFrameIn += (ma_uint64)pResampler->state.linear.t;
            if (iFrameIn < frameCountIn) {
                /* We have enough input frames remaining to bring the time down to 0..1. */
                MA_ASSERT(iFrameIn > 0);

                if (pResampler->config.format == ma_format_f32) {
                    for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                        if (pXF32 != NULL) {
                            pResampler->state.linear.x0.f32[iChannel] = pXF32[(iFrameIn-1)*pResampler->config.channels + iChannel];
                            pResampler->state.linear.x1.f32[iChannel] = pXF32[(iFrameIn-0)*pResampler->config.channels + iChannel];
                        } else {
                            pResampler->state.linear.x0.f32[iChannel] = 0;
                            pResampler->state.linear.x1.f32[iChannel] = 0;
                        }
                    }
                } else {
                    for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                        if (pXS16 != NULL) {
                            pResampler->state.linear.x0.s16[iChannel] = pXS16[(iFrameIn-1)*pResampler->config.channels + iChannel];
                            pResampler->state.linear.x1.s16[iChannel] = pXS16[(iFrameIn-0)*pResampler->config.channels + iChannel];
                        } else {
                            pResampler->state.linear.x0.s16[iChannel] = 0;
                            pResampler->state.linear.x1.s16[iChannel] = 0;
                        }
                    }
                }

                /* The time should always be relative to x0, and should not be greater than 1. */
                pResampler->state.linear.t -= floorf(pResampler->state.linear.t);
                MA_ASSERT(pResampler->state.linear.t >= 0 && pResampler->state.linear.t <= 1);
            } else {
                /* Ran out of input frames. Make sure we consume the rest of the input frames by adjusting our input time appropriately. */
                if (pResampler->config.format == ma_format_f32) {
                    if (frameCountIn > 1) {
                        for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                            if (pXF32 != NULL) {
                                pResampler->state.linear.x0.f32[iChannel] = pXF32[(frameCountIn-2)*pResampler->config.channels + iChannel];
                                pResampler->state.linear.x1.f32[iChannel] = pXF32[(frameCountIn-1)*pResampler->config.channels + iChannel];
                            } else {
                                pResampler->state.linear.x0.f32[iChannel] = 0;
                                pResampler->state.linear.x1.f32[iChannel] = 0;
                            }
                        }
                    } else {
                        for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                            if (pXF32 != NULL) {
                                pResampler->state.linear.x0.f32[iChannel] = pResampler->state.linear.x1.f32[iChannel];
                                pResampler->state.linear.x1.f32[iChannel] = pXF32[(frameCountIn-1)*pResampler->config.channels + iChannel];
                            } else {
                                pResampler->state.linear.x0.f32[iChannel] = pResampler->state.linear.x1.f32[iChannel];
                                pResampler->state.linear.x1.f32[iChannel] = 0;
                            }
                        }
                    }
                } else {
                    if (frameCountIn > 1) {
                        for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                            if (pXS16 != NULL) {
                                pResampler->state.linear.x0.s16[iChannel] = pXS16[(frameCountIn-2)*pResampler->config.channels + iChannel];
                                pResampler->state.linear.x1.s16[iChannel] = pXS16[(frameCountIn-1)*pResampler->config.channels + iChannel];
                            } else {
                                pResampler->state.linear.x0.s16[iChannel] = 0;
                                pResampler->state.linear.x1.s16[iChannel] = 0;
                            }
                        }
                    } else {
                        for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                            if (pXS16 != NULL) {
                                pResampler->state.linear.x0.s16[iChannel] = pResampler->state.linear.x1.s16[iChannel];
                                pResampler->state.linear.x1.s16[iChannel] = pXS16[(frameCountIn-1)*pResampler->config.channels + iChannel];
                            } else {
                                pResampler->state.linear.x0.s16[iChannel] = pResampler->state.linear.x1.s16[iChannel];
                                pResampler->state.linear.x1.s16[iChannel] = 0;
                            }
                        }
                    }
                }

                pResampler->state.linear.t -= (iFrameIn - frameCountIn) + 1;
                iFrameIn = frameCountIn;

                break;
            }
        }

        if (pResampler->config.format == ma_format_f32) {
            for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                pYF32[iFrameOut*pResampler->config.channels + iChannel] = ma_mix_f32_fast(pResampler->state.linear.x0.f32[iChannel], pResampler->state.linear.x1.f32[iChannel], pResampler->state.linear.t);
            }
        } else {
            for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                pYS16[iFrameOut*pResampler->config.channels + iChannel] = ma_mix_s16_fast(pResampler->state.linear.x0.s16[iChannel], pResampler->state.linear.x1.s16[iChannel], pResampler->state.linear.t);
            }
        }

        /* Move time forward. */
        pResampler->state.linear.t += ratioInOut;
        iFrameOut += 1;

        if (iFrameOut >= frameCountOut || iFrameIn >= frameCountIn) {
            break;
        }
    }

    /* Here is where we set the number of frames that were consumed. */
    *pFrameCountOut = iFrameOut;
    *pFrameCountIn  = iFrameIn;

    /* Low-pass filter if it's enabled. */
    if (pResampler->config.linear.enableLPF && pResampler->config.sampleRateIn != pResampler->config.sampleRateOut) {
        return ma_lpf_process(&pResampler->state.linear.lpf, pFramesOut, pFramesOut, *pFrameCountOut);
    } else {
        return MA_SUCCESS;
    }
}

#if defined(MA_HAS_SPEEX_RESAMPLER)
static ma_result ma_resampler_process_pcm_frames__read__speex(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    int speexErr;
    ma_uint64 frameCountOut;
    ma_uint64 frameCountIn;
    ma_uint64 framesProcessedOut;
    ma_uint64 framesProcessedIn;
    unsigned int framesPerIteration = UINT_MAX;

    MA_ASSERT(pResampler     != NULL);
    MA_ASSERT(pFramesOut     != NULL);
    MA_ASSERT(pFrameCountOut != NULL);
    MA_ASSERT(pFrameCountIn  != NULL);

    /*
    Reading from the Speex resampler requires a bit of dancing around for a few reasons. The first thing is that it's frame counts
    are in unsigned int's whereas ours is in ma_uint64. We therefore need to run the conversion in a loop. The other, more complicated
    problem, is that we need to keep track of the input time, similar to what we do with the linear resampler. The reason we need to
    do this is for ma_resampler_get_required_input_frame_count() and ma_resampler_get_expected_output_frame_count().
    */
    frameCountOut      = *pFrameCountOut;
    frameCountIn       = *pFrameCountIn;
    framesProcessedOut = 0;
    framesProcessedIn  = 0;

    while (framesProcessedOut < frameCountOut && framesProcessedIn < frameCountIn) {
        unsigned int frameCountInThisIteration;
        unsigned int frameCountOutThisIteration;
        const void* pFramesInThisIteration;
        void* pFramesOutThisIteration;

        frameCountInThisIteration = framesPerIteration;
        if ((ma_uint64)frameCountInThisIteration > (frameCountIn - framesProcessedIn)) {
            frameCountInThisIteration = (unsigned int)(frameCountIn - framesProcessedIn);
        }

        frameCountOutThisIteration = framesPerIteration;
        if ((ma_uint64)frameCountOutThisIteration > (frameCountOut - framesProcessedOut)) {
            frameCountOutThisIteration = (unsigned int)(frameCountOut - framesProcessedOut);
        }

        pFramesInThisIteration  = ma_offset_ptr(pFramesIn,  framesProcessedIn  * ma_get_bytes_per_frame(pResampler->config.format, pResampler->config.channels));
        pFramesOutThisIteration = ma_offset_ptr(pFramesOut, framesProcessedOut * ma_get_bytes_per_frame(pResampler->config.format, pResampler->config.channels));

        if (pResampler->config.format == ma_format_f32) {
            speexErr = speex_resampler_process_interleaved_float((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState, pFramesInThisIteration, &frameCountInThisIteration, pFramesOutThisIteration, &frameCountOutThisIteration);
        } else if (pResampler->config.format == ma_format_s16) {
            speexErr = speex_resampler_process_interleaved_int((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState, pFramesInThisIteration, &frameCountInThisIteration, pFramesOutThisIteration, &frameCountOutThisIteration);
        } else {
            /* Format not supported. Should never get here. */
            MA_ASSERT(MA_FALSE);
            return MA_INVALID_OPERATION;
        }

        if (speexErr != RESAMPLER_ERR_SUCCESS) {
            return ma_result_from_speex_err(speexErr);
        }

        framesProcessedIn  += frameCountInThisIteration;
        framesProcessedOut += frameCountOutThisIteration;
    }

    *pFrameCountOut = framesProcessedOut;
    *pFrameCountIn  = framesProcessedIn;

    return MA_SUCCESS;
}
#endif

static ma_result ma_resampler_process_pcm_frames__read(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pResampler != NULL);
    MA_ASSERT(pFramesOut != NULL);

    /* pFramesOut is not NULL, which means we must have a capacity. */
    if (pFrameCountOut == NULL) {
        return MA_INVALID_ARGS;
    }

    /* It doesn't make sense to not have any input frames to process. */
    if (pFrameCountIn == NULL || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    switch (pResampler->config.algorithm)
    {
        case ma_resample_algorithm_linear:
        {
            return ma_resampler_process_pcm_frames__read__linear(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
        }

        case ma_resample_algorithm_speex:
        {
        #if defined(MA_HAS_SPEEX_RESAMPLER)
            return ma_resampler_process_pcm_frames__read__speex(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
        #else
            break;
        #endif
        }
    
        default: break;
    }

    /* Should never get here. */
    MA_ASSERT(MA_FALSE);
    return MA_INVALID_ARGS;
}


static ma_result ma_resampler_process_pcm_frames__seek__generic(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, ma_uint64* pFrameCountOut)
{
    /* The generic seek method is implemented in on top of ma_resampler_process_pcm_frames__read() by just processing into a dummy buffer. */
    float devnull[8192];
    ma_uint64 totalOutputFramesToProcess;
    ma_uint64 totalOutputFramesProcessed;
    ma_uint64 totalInputFramesProcessed;
    ma_uint32 bpf;
    ma_result result;

    MA_ASSERT(pResampler != NULL);

    totalOutputFramesProcessed = 0;
    totalInputFramesProcessed = 0;
    bpf = ma_get_bytes_per_frame(pResampler->config.format, pResampler->config.channels);

    if (pFrameCountOut != NULL) {
        /* Seek by output frames. */
        totalOutputFramesToProcess = *pFrameCountOut;
    } else {
        /* Seek by input frames. */
        MA_ASSERT(pFrameCountIn != NULL);
        totalOutputFramesToProcess = ma_resampler_get_expected_output_frame_count(pResampler, *pFrameCountIn);
    }

    if (pFramesIn != NULL) {
        /* Process input data. */
        MA_ASSERT(pFrameCountIn != NULL);
        while (totalOutputFramesProcessed < totalOutputFramesToProcess && totalInputFramesProcessed < *pFrameCountIn) {
            ma_uint64 inputFramesToProcessThisIteration  = (*pFrameCountIn - totalInputFramesProcessed);
            ma_uint64 outputFramesToProcessThisIteration = (totalOutputFramesToProcess - totalOutputFramesProcessed);
            if (outputFramesToProcessThisIteration > sizeof(devnull) / bpf) {
                outputFramesToProcessThisIteration = sizeof(devnull) / bpf;
            }

            result = ma_resampler_process_pcm_frames__read(pResampler, ma_offset_ptr(pFramesIn, totalInputFramesProcessed*bpf), &inputFramesToProcessThisIteration, ma_offset_ptr(devnull, totalOutputFramesProcessed*bpf), &outputFramesToProcessThisIteration);
            if (result != MA_SUCCESS) {
                return result;
            }

            totalOutputFramesProcessed += outputFramesToProcessThisIteration;
            totalInputFramesProcessed  += inputFramesToProcessThisIteration;
        }
    } else {
        /* Don't process input data - just update timing and filter state as if zeroes were passed in. */
        while (totalOutputFramesProcessed < totalOutputFramesToProcess) {
            ma_uint64 inputFramesToProcessThisIteration  = 16384;
            ma_uint64 outputFramesToProcessThisIteration = (totalOutputFramesToProcess - totalOutputFramesProcessed);
            if (outputFramesToProcessThisIteration > sizeof(devnull) / bpf) {
                outputFramesToProcessThisIteration = sizeof(devnull) / bpf;
            }

            result = ma_resampler_process_pcm_frames__read(pResampler, NULL, &inputFramesToProcessThisIteration, ma_offset_ptr(devnull, totalOutputFramesProcessed*bpf), &outputFramesToProcessThisIteration);
            if (result != MA_SUCCESS) {
                return result;
            }

            totalOutputFramesProcessed += outputFramesToProcessThisIteration;
            totalInputFramesProcessed  += inputFramesToProcessThisIteration;
        }
    }


    if (pFrameCountIn != NULL) {
        *pFrameCountIn = totalInputFramesProcessed;
    }
    if (pFrameCountOut != NULL) {
        *pFrameCountOut = totalOutputFramesProcessed;
    }

    return MA_SUCCESS;
}

static ma_result ma_resampler_process_pcm_frames__seek__linear(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pResampler != NULL);

    return ma_resampler_process_pcm_frames__seek__generic(pResampler, pFramesIn, pFrameCountIn, pFrameCountOut);
}

#if defined(MA_HAS_SPEEX_RESAMPLER)
static ma_result ma_resampler_process_pcm_frames__seek__speex(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pResampler != NULL);

    return ma_resampler_process_pcm_frames__seek__generic(pResampler, pFramesIn, pFrameCountIn, pFrameCountOut);
}
#endif

static ma_result ma_resampler_process_pcm_frames__seek(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pResampler != NULL);

    switch (pResampler->config.algorithm)
    {
        case ma_resample_algorithm_linear:
        {
            return ma_resampler_process_pcm_frames__seek__linear(pResampler, pFramesIn, pFrameCountIn, pFrameCountOut);
        } break;

        case ma_resample_algorithm_speex:
        {
        #if defined(MA_HAS_SPEEX_RESAMPLER)
            return ma_resampler_process_pcm_frames__seek__speex(pResampler, pFramesIn, pFrameCountIn, pFrameCountOut);
        #else
            break;
        #endif
        };

        default: break;
    }

    /* Should never hit this. */
    MA_ASSERT(MA_FALSE);
    return MA_INVALID_ARGS;
}


ma_result ma_resampler_process_pcm_frames(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pFrameCountOut == NULL && pFrameCountIn == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pFramesOut != NULL) {
        /* Reading. */
        return ma_resampler_process_pcm_frames__read(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        /* Seeking. */
        return ma_resampler_process_pcm_frames__seek(pResampler, pFramesIn, pFrameCountIn, pFrameCountOut);
    }
}

ma_result ma_resampler_set_rate(ma_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut)
{
    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    if (sampleRateIn == 0 || sampleRateOut == 0) {
        return MA_INVALID_ARGS;
    }

    pResampler->config.sampleRateIn  = sampleRateIn;
    pResampler->config.sampleRateOut = sampleRateOut;

    switch (pResampler->config.algorithm)
    {
        case ma_resample_algorithm_linear:
        {
            /* If we are using low-pass filtering we need to reinitialize the filter since it depends on the sample rate. */
            if (pResampler->config.linear.enableLPF) {
                ma_resampler__init_lpf(pResampler);
            }
        } break;

        case ma_resample_algorithm_speex:
        {
        #if defined(MA_HAS_SPEEX_RESAMPLER)
            return ma_result_from_speex_err(speex_resampler_set_rate((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState, sampleRateIn, sampleRateOut));
        #else
            break;
        #endif
        };

        default: break;
    }

    /* Should never get here. */
    MA_ASSERT(MA_FALSE);
    return MA_INVALID_OPERATION;
}

ma_result ma_resampler_set_rate_ratio(ma_resampler* pResampler, float ratio)
{
    /* We use up to 6 decimal places and then simplify the fraction. */
    ma_uint32 n;
    ma_uint32 d;
    ma_uint32 gcf;

    if (ratio < MA_RESAMPLER_MIN_RATIO || ratio > MA_RESAMPLER_MAX_RATIO) {
        return MA_INVALID_ARGS;
    }

    d = 1000000;
    n = (ma_uint32)(ratio * d);

    MA_ASSERT(n != 0);
    
    gcf = ma_gcf_u32(n, d);
    
    n /= gcf;
    d /= gcf;

    return ma_resampler_set_rate(pResampler, n, d);
}

ma_uint64 ma_resampler_get_required_input_frame_count(ma_resampler* pResampler, ma_uint64 outputFrameCount)
{
    double ratioInOut;

    if (pResampler == NULL) {
        return 0;
    }

    if (outputFrameCount == 0) {
        return 0;
    }

    ratioInOut = (double)pResampler->config.sampleRateIn / (double)pResampler->config.sampleRateOut;

    switch (pResampler->config.algorithm)
    {
        case ma_resample_algorithm_linear:
        {
            /*
            The first output frame is treated a little different to the rest because it is never interpolated - the first output frame is always the
            same as the first input frame. We can know if we're loading the first frame by checking if the input time is < 0.
            */
            ma_uint64 count = 0;
            double t = pResampler->state.linear.t;
            if (t < 0) {
                count = 1;
                t = 1;
            }

            /* If the input time is greater than 1 we consume any whole input frames. */
            if (t > 1) {
                count = (ma_uint64)t;
                t -= count;
            }

            /* At this point we are guaranteed to get at least one output frame from the cached input (not requiring an additional input). */
            outputFrameCount -= 1;

            count += (ma_uint64)ceil(t + (outputFrameCount * ratioInOut)) - 1;
            return count;
        }

        case ma_resample_algorithm_speex:
        {
        #if defined(MA_HAS_SPEEX_RESAMPLER)
            ma_uint64 count;
            int speexErr = ma_speex_resampler_get_required_input_frame_count((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState, outputFrameCount, &count);
            if (speexErr != RESAMPLER_ERR_SUCCESS) {
                return 0;
            }

            return count;
        #else
            break;
        #endif
        }

        default: break;
    }

    /* Should never get here. */
    MA_ASSERT(MA_FALSE);
    return 0;
}

ma_uint64 ma_resampler_get_expected_output_frame_count(ma_resampler* pResampler, ma_uint64 inputFrameCount)
{
    double ratioInOut;

    if (pResampler == NULL) {
        return 0;   /* Invalid args. */
    }

    if (inputFrameCount == 0) {
        return 0;
    }

    ratioInOut = (double)pResampler->config.sampleRateIn / (double)pResampler->config.sampleRateOut;

    switch (pResampler->config.algorithm)
    {
        case ma_resample_algorithm_linear:
        {
            ma_uint64 outputFrameCount = 0;
            double t = pResampler->state.linear.t;

            if (t < 0) {
                t = 1;
                inputFrameCount -= 1;
            }

            for (;;) {
                if (t > 1) {
                    if (inputFrameCount  > (ma_uint64)t) {
                        inputFrameCount -= (ma_uint64)t;
                        t -= (ma_uint64)t;
                    } else {
                        inputFrameCount = 0;
                        break;
                    }
                }

                t += ratioInOut;
                outputFrameCount += 1;

                if (inputFrameCount == 0) {
                    break;
                }
            }

            return outputFrameCount;
        }

        case ma_resample_algorithm_speex:
        {
        #if defined(MA_HAS_SPEEX_RESAMPLER)
            ma_uint64 count;
            int speexErr = ma_speex_resampler_get_expected_output_frame_count((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState, inputFrameCount, &count);
            if (speexErr != RESAMPLER_ERR_SUCCESS) {
                return 0;
            }

            return count;
        #else
            break;
        #endif
        }

        default: break;
    }

    /* Should never get here. */
    MA_ASSERT(MA_FALSE);
    return 0;
}

ma_uint64 ma_resampler_get_input_latency(ma_resampler* pResampler)
{
    if (pResampler == NULL) {
        return 0;
    }

    switch (pResampler->config.algorithm)
    {
        case ma_resample_algorithm_linear:
        {
            return 1;
        }

        case ma_resample_algorithm_speex:
        {
        #if defined(MA_HAS_SPEEX_RESAMPLER)
            return (ma_uint64)ma_speex_resampler_get_input_latency((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState);
        #else
            break;
        #endif
        }

        default: break;
    }

    /* Should never get here. */
    MA_ASSERT(MA_FALSE);
    return 0;
}

ma_uint64 ma_resampler_get_output_latency(ma_resampler* pResampler)
{
    if (pResampler == NULL) {
        return 0;
    }

    switch (pResampler->config.algorithm)
    {
        case ma_resample_algorithm_linear:
        {
            return 1;
        }

        case ma_resample_algorithm_speex:
        {
        #if defined(MA_HAS_SPEEX_RESAMPLER)
            return (ma_uint64)ma_speex_resampler_get_output_latency((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState);
        #else
            break;
        #endif
        }

        default: break;
    }

    /* Should never get here. */
    MA_ASSERT(MA_FALSE);
    return 0;
}
#endif  /* MINIAUDIO_IMPLEMENTATION */
