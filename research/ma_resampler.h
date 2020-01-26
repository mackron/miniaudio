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

Because Speex is not public domain it is strictly opt-in and the code is stored in separate files. if you opt-in to the Speex backend you will need to consider
it's license, the text of which can be found in it's source files in "extras/speex_resampler". Details on how to opt-in to the Speex resampler is explained in
the Speex Resampler section below.

The algorithm cannot be changed after initialization.

Processing always happens on a per PCM frame basis and always assumed interleaved input and output. De-interleaved processing is not supported. To process
frames, use `ma_resampler_process_pcm_frames()`. On input, this function takes the number of output frames you can fit in the output buffer and the number of
input frames contained in the input buffer. On output these variables contain the number of output frames that were written to the output buffer and the
number of input frames that were consumed in the process. You can pass in NULL for the input buffer in which case it will be treated as an infinitely large
buffer of zeros. The output buffer can also be NULL, in which case the processing will be treated as seek.

The sample rate can be changed dynamically on the fly. You can change this with explicit sample rates with `ma_resampler_set_rate()` and also with a decimal
ratio with `ma_resampler_set_rate_ratio()`. The ratio is in/out.

Sometimes it's useful to know exactly how many input frames will be required to output a specific number of frames. You can calculate this with
`ma_resampler_get_required_input_frame_count()`. Likewise, it's sometimes useful to know exactly how many frames would be output given a certain number of
input frames. You can do this with `ma_resampler_get_expected_output_frame_count()`.

Due to the nature of how resampling works, the resampler introduces some latency. This can be retrieved in terms of both the input rate and the output rate
with `ma_resampler_get_input_latency()` and `ma_resampler_get_output_latency()`.


Resampling Algorithms
---------------------
The choice of resampling algorithm depends on your situation and requirements. The linear resampler is the most efficient and has the least amount of latency,
but at the expense of poorer quality. The Speex resampler is higher quality, but slower with more latency. It also performs several heap applications
internally for memory management.


Linear Resampling
-----------------
The linear resampler is the fastest, but comes at the expense of poorer quality. There is, however, some control over the quality of the linear resampler which
may make it a suitable option depending on your requirements.

The linear resampler performs low-pass filtering before or after downsampling or upsampling, depending on the sample rates you're converting between. When
decreasing the sample rate, the low-pass filter will be applied before downsampling. When increasing the rate it will be performed after upsampling. By default
a second order low-pass filter will be applied. To improve quality you can chain low-pass filters together, up to a maximum of `MA_MAX_RESAMPLER_LPF_FILTERS`.
This comes at the expense of increased computational cost and latency. You can also disable filtering altogether by setting the filter count to 0. The filter
count is controlled with the `lpfCount` config variable.

The low-pass filter has a cutoff frequency which defaults to half the sample rate of the lowest of the input and output sample rates (Nyquist Frequency). This
can be controlled with the `lpfNyquistFactor` config variable. This defaults to 1, and should be in the range of 0..1, although a value of 0 does not make
sense and should be avoided. A value of 1 will use the Nyquist Frequency as the cutoff. A value of 0.5 will use half the Nyquist Frequency as the cutoff, etc.
Values less than 1 will result in more washed out sound due to more of the higher frequencies being removed. This config variable has no impact on performance
and is a purely perceptual configuration.

The API for the linear resampler is the same as the main resampler API, only it's called `ma_linear_resampler`.


Speex Resampling
----------------
The Speex resampler is made up of third party code which is released under the BSD license. Because it is licensed differently to miniaudio, which is public
domain, it is strictly opt-in and all of it's code is stored in separate files. If you opt-in to the Speex resampler you must consider the license text in it's
source files. To opt-in, you must first #include the following file before the implementation of miniaudio.h:

    #include "extras/speex_resampler/ma_speex_resampler.h"

Both the header and implementation is contained within the same file. To implementation can be included in your program like so:

    #define MINIAUDIO_SPEEX_RESAMPLER_IMPLEMENTATION
    #include "extras/speex_resampler/ma_speex_resampler.h"

Note that even if you opt-in to the Speex backend, miniaudio won't use it unless you explicitly ask for it in the respective config of the object you are
initializing. If you try to use the Speex resampler without opting in, initialization of the `ma_resampler` object will fail with `MA_NO_BACKEND`.

The only configuration option to consider with the Speex resampler is the `speex.quality` config variable. This is a value between 0 and 10, with 0 being
the worst/fastest and 10 being the best/slowest. The default value is 3.

**************************************************************************************************************************************************************/

#ifndef MA_MAX_RESAMPLER_LPF_FILTERS
#define MA_MAX_RESAMPLER_LPF_FILTERS 4
#endif

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    ma_uint32 lpfCount;         /* How many low-pass filters to chain together. A single low-pass filter is second order. Setting this to 0 will disable low-pass filtering. */
    double    lpfNyquistFactor; /* 0..1. Defaults to 1. 1 = Half the sampling frequency (Nyquist Frequency), 0.5 = Quarter the sampling frequency (half Nyquest Frequency), etc. */
} ma_linear_resampler_config;

ma_linear_resampler_config ma_linear_resampler_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);

typedef struct
{
    ma_linear_resampler_config config;
    ma_uint32 inAdvanceInt;
    ma_uint32 inAdvanceFrac;
    ma_uint32 inTimeInt;
    ma_uint32 inTimeFrac;
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
    ma_lpf lpf[MA_MAX_RESAMPLER_LPF_FILTERS];
} ma_linear_resampler;

ma_result ma_linear_resampler_init(const ma_linear_resampler_config* pConfig, ma_linear_resampler* pResampler);
void ma_linear_resampler_uninit(ma_linear_resampler* pResampler);
ma_result ma_linear_resampler_process_pcm_frames(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
ma_result ma_linear_resampler_set_rate(ma_linear_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);
ma_uint64 ma_linear_resampler_get_required_input_frame_count(ma_linear_resampler* pResampler, ma_uint64 outputFrameCount);
ma_uint64 ma_linear_resampler_get_expected_output_frame_count(ma_linear_resampler* pResampler, ma_uint64 inputFrameCount);
ma_uint64 ma_linear_resampler_get_input_latency(ma_linear_resampler* pResampler);
ma_uint64 ma_linear_resampler_get_output_latency(ma_linear_resampler* pResampler);

typedef enum
{
    ma_resample_algorithm_linear,   /* Fastest, lowest quality. Optional low-pass filtering. Default. */
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
        ma_uint32 lpfCount;
        double lpfNyquistFactor;
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
        ma_linear_resampler linear;
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

ma_linear_resampler_config ma_linear_resampler_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut)
{
    ma_linear_resampler_config config;
    MA_ZERO_OBJECT(&config);
    config.format           = format;
    config.channels         = channels;
    config.sampleRateIn     = sampleRateIn;
    config.sampleRateOut    = sampleRateOut;
    config.lpfCount         = 1;
    config.lpfNyquistFactor = 1;

    return config;
}

static ma_result ma_linear_resampler_set_rate_internal(ma_linear_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_bool32 isResamplerAlreadyInitialized)
{
    ma_uint32 gcf;

    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    if (sampleRateIn == 0 || sampleRateOut == 0) {
        return MA_INVALID_ARGS;
    }

    /* Simplify the sample rate. */
    gcf = ma_gcf_u32(pResampler->config.sampleRateIn, pResampler->config.sampleRateOut);
    pResampler->config.sampleRateIn  /= gcf;
    pResampler->config.sampleRateOut /= gcf;

    if (pResampler->config.lpfCount > 0) {
        ma_result result;
        ma_uint32 iFilter;
        ma_uint32 lpfSampleRate;
        ma_uint32 lpfCutoffFrequency;
        ma_lpf_config lpfConfig;

        if (pResampler->config.lpfCount > MA_MAX_RESAMPLER_LPF_FILTERS) {
            return MA_INVALID_ARGS;
        }

        lpfSampleRate      = (ma_uint32)(ma_max(pResampler->config.sampleRateIn, pResampler->config.sampleRateOut));
        lpfCutoffFrequency = (ma_uint32)(ma_min(pResampler->config.sampleRateIn, pResampler->config.sampleRateOut) * 0.5 * pResampler->config.lpfNyquistFactor);

        lpfConfig = ma_lpf_config_init(pResampler->config.format, pResampler->config.channels, lpfSampleRate, lpfCutoffFrequency);

        /*
        If the resampler is alreay initialized we don't want to do a fresh initialization of the low-pass filter because it will result in the cached frames
        getting cleared. Instead we re-initialize the filter which will maintain any cached frames.
        */
        for (iFilter = 0; iFilter < pResampler->config.lpfCount; iFilter += 1) {
            if (isResamplerAlreadyInitialized) {
                result = ma_lpf_reinit(&lpfConfig, &pResampler->lpf[iFilter]);
            } else {
                result = ma_lpf_init(&lpfConfig, &pResampler->lpf[iFilter]);
            }

            if (result != MA_SUCCESS) {
                break;
            }
        }
        
        if (result != MA_SUCCESS) {
            return result;  /* Failed to initialize the low-pass filter. */
        }
    }

    pResampler->inAdvanceInt  = pResampler->config.sampleRateIn / pResampler->config.sampleRateOut;
    pResampler->inAdvanceFrac = pResampler->config.sampleRateIn % pResampler->config.sampleRateOut;

    /* Make sure the fractional part is less than the output sample rate. */
    pResampler->inTimeInt += pResampler->inTimeFrac / pResampler->config.sampleRateOut;
    pResampler->inTimeFrac = pResampler->inTimeFrac % pResampler->config.sampleRateOut;

    return MA_SUCCESS;
}

ma_result ma_linear_resampler_init(const ma_linear_resampler_config* pConfig, ma_linear_resampler* pResampler)
{
    ma_result result;

    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pResampler);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pResampler->config = *pConfig;

    /* Setting the rate will set up the filter and time advances for us. */
    result = ma_linear_resampler_set_rate_internal(pResampler, pConfig->sampleRateIn, pConfig->sampleRateOut, /* isResamplerAlreadyInitialized = */ MA_FALSE);
    if (result != MA_SUCCESS) {
        return result;
    }

    pResampler->inTimeInt  = 1;  /* Set this to one to force an input sample to always be loaded for the first output frame. */
    pResampler->inTimeFrac = 0;

    return MA_SUCCESS;
}

void ma_linear_resampler_uninit(ma_linear_resampler* pResampler)
{
    if (pResampler == NULL) {
        return;
    }
}

static MA_INLINE ma_int16 ma_linear_resampler_mix_s16(ma_int16 x, ma_int16 y, ma_int32 a, const ma_int32 shift)
{
    ma_int32 b;
    ma_int32 c;
    ma_int32 r;

    MA_ASSERT(a <= (1<<shift));

    b = x * ((1<<shift) - a);
    c = y * a;
    r = b + c;
    
    return (ma_int16)(r >> shift);
}

static void ma_linear_resampler_interpolate_frame_s16(ma_linear_resampler* pResampler, ma_int16* pFrameOut)
{
    ma_uint32 c;
    ma_uint32 a;
    const ma_uint32 shift = 12;

    MA_ASSERT(pResampler != NULL);
    MA_ASSERT(pFrameOut  != NULL);

    a = (pResampler->inTimeFrac << shift) / pResampler->config.sampleRateOut;

    for (c = 0; c < pResampler->config.channels; c += 1) {
        ma_int16 s = ma_linear_resampler_mix_s16(pResampler->x0.s16[c], pResampler->x1.s16[c], a, shift);
        pFrameOut[c] = s;
    }
}


static void ma_linear_resampler_interpolate_frame_f32(ma_linear_resampler* pResampler, float* pFrameOut)
{
    ma_uint32 c;
    float a;

    MA_ASSERT(pResampler != NULL);
    MA_ASSERT(pFrameOut  != NULL);

    a = (float)pResampler->inTimeFrac / pResampler->config.sampleRateOut;

    for (c = 0; c < pResampler->config.channels; c += 1) {
        float s = ma_mix_f32_fast(pResampler->x0.f32[c], pResampler->x1.f32[c], a);
        pFrameOut[c] = s;
    }
}

static ma_result ma_linear_resampler_process_pcm_frames_s16_downsample(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    const ma_int16* pFramesInS16;
    /* */ ma_int16* pFramesOutS16;
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut;
    ma_uint64 framesProcessedIn;
    ma_uint64 framesProcessedOut;

    MA_ASSERT(pResampler     != NULL);
    MA_ASSERT(pFrameCountIn  != NULL);
    MA_ASSERT(pFrameCountOut != NULL);

    pFramesInS16       = (const ma_int16*)pFramesIn;
    pFramesOutS16      = (      ma_int16*)pFramesOut;
    frameCountIn       = *pFrameCountIn;
    frameCountOut      = *pFrameCountOut;
    framesProcessedIn  = 0;
    framesProcessedOut = 0;

    for (;;) {
        if (framesProcessedOut >= *pFrameCountOut) {
            break;
        }

        /* Before interpolating we need to load the buffers. When doing this we need to ensure we run every input sample through the filter. */
        while (pResampler->inTimeInt > 0 && frameCountIn > 0) {
            ma_uint32 iFilter;
            ma_uint32 iChannel;

            if (pFramesInS16 != NULL) {
                for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                    pResampler->x0.s16[iChannel] = pResampler->x1.s16[iChannel];
                    pResampler->x1.s16[iChannel] = pFramesInS16[iChannel];
                }
                pFramesInS16 += pResampler->config.channels;
            } else {
                for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                    pResampler->x0.s16[iChannel] = pResampler->x1.s16[iChannel];
                    pResampler->x1.s16[iChannel] = 0;
                }
            }

            /* Filter. */
            for (iFilter = 0; iFilter < pResampler->config.lpfCount; iFilter += 1) {
                ma_lpf_process_pcm_frame_s16(&pResampler->lpf[iFilter], pResampler->x1.s16, pResampler->x1.s16);
            }

            frameCountIn          -= 1;
            framesProcessedIn     += 1;
            pResampler->inTimeInt -= 1;
        }

        if (pResampler->inTimeInt > 0) {
            break;  /* Ran out of input data. */
        }

        /* Getting here means the frames have been loaded and filtered and we can generate the next output frame. */
        if (pFramesOutS16 != NULL) {
            MA_ASSERT(pResampler->inTimeInt == 0);
            ma_linear_resampler_interpolate_frame_s16(pResampler, pFramesOutS16);

            pFramesOutS16 += 1;
        }

        framesProcessedOut += 1;

        /* Advance time forward. */
        pResampler->inTimeInt  += pResampler->inAdvanceInt;
        pResampler->inTimeFrac += pResampler->inAdvanceFrac;
        if (pResampler->inTimeFrac >= pResampler->config.sampleRateOut) {
            pResampler->inTimeFrac -= pResampler->config.sampleRateOut;
            pResampler->inTimeInt  += 1;
        }
    }

    *pFrameCountIn  = framesProcessedIn;
    *pFrameCountOut = framesProcessedOut;

    return MA_SUCCESS;
}

static ma_result ma_linear_resampler_process_pcm_frames_s16_upsample(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    const ma_int16* pFramesInS16;
    /* */ ma_int16* pFramesOutS16;
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut;
    ma_uint64 framesProcessedIn;
    ma_uint64 framesProcessedOut;

    MA_ASSERT(pResampler     != NULL);
    MA_ASSERT(pFrameCountIn  != NULL);
    MA_ASSERT(pFrameCountOut != NULL);

    pFramesInS16       = (const ma_int16*)pFramesIn;
    pFramesOutS16      = (      ma_int16*)pFramesOut;
    frameCountIn       = *pFrameCountIn;
    frameCountOut      = *pFrameCountOut;
    framesProcessedIn  = 0;
    framesProcessedOut = 0;

    for (;;) {
        ma_uint32 iFilter;

        if (framesProcessedOut >= *pFrameCountOut) {
            break;
        }

        /* Before interpolating we need to load the buffers. */
        while (pResampler->inTimeInt > 0 && frameCountIn > 0) {
            ma_uint32 iChannel;

            if (pFramesInS16 != NULL) {
                for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                    pResampler->x0.s16[iChannel] = pResampler->x1.s16[iChannel];
                    pResampler->x1.s16[iChannel] = pFramesInS16[iChannel];
                }
                pFramesInS16 += pResampler->config.channels;
            } else {
                for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                    pResampler->x0.s16[iChannel] = pResampler->x1.s16[iChannel];
                    pResampler->x1.s16[iChannel] = 0;
                }
            }

            frameCountIn          -= 1;
            framesProcessedIn     += 1;
            pResampler->inTimeInt -= 1;
        }

        if (pResampler->inTimeInt > 0) {
            break;  /* Ran out of input data. */
        }

        /* Getting here means the frames have been loaded and we can generate the next output frame. */
        if (pFramesOutS16 != NULL) {
            MA_ASSERT(pResampler->inTimeInt == 0);
            ma_linear_resampler_interpolate_frame_s16(pResampler, pFramesOutS16);

            /* Filter. */
            for (iFilter = 0; iFilter < pResampler->config.lpfCount; iFilter += 1) {
                ma_lpf_process_pcm_frame_s16(&pResampler->lpf[iFilter], pFramesOutS16, pFramesOutS16);
            }

            pFramesOutS16 += 1;
        }

        framesProcessedOut += 1;

        /* Advance time forward. */
        pResampler->inTimeInt  += pResampler->inAdvanceInt;
        pResampler->inTimeFrac += pResampler->inAdvanceFrac;
        if (pResampler->inTimeFrac >= pResampler->config.sampleRateOut) {
            pResampler->inTimeFrac -= pResampler->config.sampleRateOut;
            pResampler->inTimeInt  += 1;
        }
    }

    *pFrameCountIn  = framesProcessedIn;
    *pFrameCountOut = framesProcessedOut;

    return MA_SUCCESS;
}

static ma_result ma_linear_resampler_process_pcm_frames_s16(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pResampler != NULL);

    if (pResampler->config.sampleRateIn > pResampler->config.sampleRateOut) {
        return ma_linear_resampler_process_pcm_frames_s16_downsample(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        return ma_linear_resampler_process_pcm_frames_s16_upsample(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    }

    return MA_SUCCESS;
}


static ma_result ma_linear_resampler_process_pcm_frames_f32_downsample(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    const float* pFramesInF32;
    /* */ float* pFramesOutF32;
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut;
    ma_uint64 framesProcessedIn;
    ma_uint64 framesProcessedOut;

    MA_ASSERT(pResampler     != NULL);
    MA_ASSERT(pFrameCountIn  != NULL);
    MA_ASSERT(pFrameCountOut != NULL);

    pFramesInF32       = (const float*)pFramesIn;
    pFramesOutF32      = (      float*)pFramesOut;
    frameCountIn       = *pFrameCountIn;
    frameCountOut      = *pFrameCountOut;
    framesProcessedIn  = 0;
    framesProcessedOut = 0;

    for (;;) {
        if (framesProcessedOut >= *pFrameCountOut) {
            break;
        }

        /* Before interpolating we need to load the buffers. When doing this we need to ensure we run every input sample through the filter. */
        while (pResampler->inTimeInt > 0 && frameCountIn > 0) {
            ma_uint32 iFilter;
            ma_uint32 iChannel;

            if (pFramesInF32 != NULL) {
                for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                    pResampler->x0.f32[iChannel] = pResampler->x1.f32[iChannel];
                    pResampler->x1.f32[iChannel] = pFramesInF32[iChannel];
                }
                pFramesInF32 += pResampler->config.channels;
            } else {
                for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                    pResampler->x0.f32[iChannel] = pResampler->x1.f32[iChannel];
                    pResampler->x1.f32[iChannel] = 0;
                }
            }

            /* Filter. */
            for (iFilter = 0; iFilter < pResampler->config.lpfCount; iFilter += 1) {
                ma_lpf_process_pcm_frame_f32(&pResampler->lpf[iFilter], pResampler->x1.f32, pResampler->x1.f32);
            }

            frameCountIn          -= 1;
            framesProcessedIn     += 1;
            pResampler->inTimeInt -= 1;
        }

        if (pResampler->inTimeInt > 0) {
            break;  /* Ran out of input data. */
        }

        /* Getting here means the frames have been loaded and filtered and we can generate the next output frame. */
        if (pFramesOutF32 != NULL) {
            MA_ASSERT(pResampler->inTimeInt == 0);
            ma_linear_resampler_interpolate_frame_f32(pResampler, pFramesOutF32);

            pFramesOutF32 += 1;
        }

        framesProcessedOut += 1;

        /* Advance time forward. */
        pResampler->inTimeInt  += pResampler->inAdvanceInt;
        pResampler->inTimeFrac += pResampler->inAdvanceFrac;
        if (pResampler->inTimeFrac >= pResampler->config.sampleRateOut) {
            pResampler->inTimeFrac -= pResampler->config.sampleRateOut;
            pResampler->inTimeInt  += 1;
        }
    }

    *pFrameCountIn  = framesProcessedIn;
    *pFrameCountOut = framesProcessedOut;

    return MA_SUCCESS;
}

static ma_result ma_linear_resampler_process_pcm_frames_f32_upsample(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    const float* pFramesInF32;
    /* */ float* pFramesOutF32;
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut;
    ma_uint64 framesProcessedIn;
    ma_uint64 framesProcessedOut;

    MA_ASSERT(pResampler     != NULL);
    MA_ASSERT(pFrameCountIn  != NULL);
    MA_ASSERT(pFrameCountOut != NULL);

    pFramesInF32       = (const float*)pFramesIn;
    pFramesOutF32      = (      float*)pFramesOut;
    frameCountIn       = *pFrameCountIn;
    frameCountOut      = *pFrameCountOut;
    framesProcessedIn  = 0;
    framesProcessedOut = 0;

    for (;;) {
        ma_uint32 iFilter;

        if (framesProcessedOut >= *pFrameCountOut) {
            break;
        }

        /* Before interpolating we need to load the buffers. */
        while (pResampler->inTimeInt > 0 && frameCountIn > 0) {
            ma_uint32 iChannel;

            if (pFramesInF32 != NULL) {
                for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                    pResampler->x0.f32[iChannel] = pResampler->x1.f32[iChannel];
                    pResampler->x1.f32[iChannel] = pFramesInF32[iChannel];
                }
                pFramesInF32 += pResampler->config.channels;
            } else {
                for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                    pResampler->x0.f32[iChannel] = pResampler->x1.f32[iChannel];
                    pResampler->x1.f32[iChannel] = 0;
                }
            }

            frameCountIn          -= 1;
            framesProcessedIn     += 1;
            pResampler->inTimeInt -= 1;
        }

        if (pResampler->inTimeInt > 0) {
            break;  /* Ran out of input data. */
        }

        /* Getting here means the frames have been loaded and we can generate the next output frame. */
        if (pFramesOutF32 != NULL) {
            MA_ASSERT(pResampler->inTimeInt == 0);
            ma_linear_resampler_interpolate_frame_f32(pResampler, pFramesOutF32);

            /* Filter. */
            for (iFilter = 0; iFilter < pResampler->config.lpfCount; iFilter += 1) {
                ma_lpf_process_pcm_frame_f32(&pResampler->lpf[iFilter], pFramesOutF32, pFramesOutF32);
            }

            pFramesOutF32 += 1;
        }

        framesProcessedOut += 1;

        /* Advance time forward. */
        pResampler->inTimeInt  += pResampler->inAdvanceInt;
        pResampler->inTimeFrac += pResampler->inAdvanceFrac;
        if (pResampler->inTimeFrac >= pResampler->config.sampleRateOut) {
            pResampler->inTimeFrac -= pResampler->config.sampleRateOut;
            pResampler->inTimeInt  += 1;
        }
    }

    *pFrameCountIn  = framesProcessedIn;
    *pFrameCountOut = framesProcessedOut;

    return MA_SUCCESS;
}

static ma_result ma_linear_resampler_process_pcm_frames_f32(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pResampler != NULL);

    if (pResampler->config.sampleRateIn > pResampler->config.sampleRateOut) {
        return ma_linear_resampler_process_pcm_frames_f32_downsample(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        return ma_linear_resampler_process_pcm_frames_f32_upsample(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    }

    return MA_SUCCESS;
}


ma_result ma_linear_resampler_process_pcm_frames(ma_linear_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    /*  */ if (pResampler->config.format == ma_format_s16) {
        return ma_linear_resampler_process_pcm_frames_s16(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else if (pResampler->config.format == ma_format_f32) {
        return ma_linear_resampler_process_pcm_frames_f32(pResampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        /* Should never get here. Getting here means the format is not supported and you didn't check the return value of ma_linear_resampler_init(). */
        MA_ASSERT(MA_FALSE);
        return MA_INVALID_ARGS;
    }
}


ma_result ma_linear_resampler_set_rate(ma_linear_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut)
{
    return ma_linear_resampler_set_rate_internal(pResampler, sampleRateIn, sampleRateOut, /* isResamplerAlreadyInitialized = */ MA_TRUE);
}


ma_uint64 ma_linear_resampler_get_required_input_frame_count(ma_linear_resampler* pResampler, ma_uint64 outputFrameCount)
{
    ma_uint64 count;

    if (pResampler == NULL) {
        return 0;
    }

    count  = outputFrameCount * pResampler->inAdvanceInt;
    count += (pResampler->inTimeFrac + (outputFrameCount * pResampler->inAdvanceFrac)) / pResampler->config.sampleRateOut;

    return count;
}

ma_uint64 ma_linear_resampler_get_expected_output_frame_count(ma_linear_resampler* pResampler, ma_uint64 inputFrameCount)
{
    ma_uint64 outputFrameCount;
    ma_uint64 inTimeInt;
    ma_uint64 inTimeFrac;

    if (pResampler == NULL) {
        return 0;
    }

    /* TODO: Try making this run in constant time. */

    outputFrameCount = 0;
    inTimeInt  = pResampler->inTimeInt;
    inTimeFrac = pResampler->inTimeFrac;

    for (;;) {
        while (inTimeInt > 0 && inputFrameCount > 0) {
            inputFrameCount -= 1;
            inTimeInt       -= 1;
        }

        if (inTimeInt > 0) {
            break;
        }

        outputFrameCount += 1;

        /* Advance time forward. */
        inTimeInt  += pResampler->inAdvanceInt;
        inTimeFrac += pResampler->inAdvanceFrac;
        if (inTimeFrac >= pResampler->config.sampleRateOut) {
            inTimeFrac -= pResampler->config.sampleRateOut;
            inTimeInt  += 1;
        }
    }

    return outputFrameCount;
}

ma_uint64 ma_linear_resampler_get_input_latency(ma_linear_resampler* pResampler)
{
    ma_uint32 latency;
    ma_uint32 iFilter;

    if (pResampler == NULL) {
        return 0;
    }

    latency = 1;
    for (iFilter = 0; iFilter < pResampler->config.lpfCount; iFilter += 1) {
        latency += ma_lpf_get_latency(&pResampler->lpf[iFilter]);
    }

    return latency;
}

ma_uint64 ma_linear_resampler_get_output_latency(ma_linear_resampler* pResampler)
{
    if (pResampler == NULL) {
        return 0;
    }

    return ma_linear_resampler_get_input_latency(pResampler) * pResampler->config.sampleRateOut / pResampler->config.sampleRateIn;
}


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
#endif  /* ma_speex_resampler_h */

ma_resampler_config ma_resampler_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_resample_algorithm algorithm)
{
    ma_resampler_config config;

    MA_ZERO_OBJECT(&config);
    config.format = format;
    config.channels = channels;
    config.sampleRateIn = sampleRateIn;
    config.sampleRateOut = sampleRateOut;
    config.algorithm = algorithm;

    /* Linear. */
    config.linear.lpfCount = 1;
    config.linear.lpfNyquistFactor = 1;

    /* Speex. */
    config.speex.quality = 3;   /* Cannot leave this as 0 as that is actually a valid value for Speex resampling quality. */

    return config;
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
            ma_linear_resampler_config linearConfig;
            linearConfig = ma_linear_resampler_config_init(pConfig->format, pConfig->channels, pConfig->sampleRateIn, pConfig->sampleRateOut);
            linearConfig.lpfCount         = pConfig->linear.lpfCount;
            linearConfig.lpfNyquistFactor = pConfig->linear.lpfNyquistFactor;

            result = ma_linear_resampler_init(&linearConfig, &pResampler->state.linear);
            if (result != MA_SUCCESS) {
                return result;
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

    if (pResampler->config.algorithm == ma_resample_algorithm_linear) {
        ma_linear_resampler_uninit(&pResampler->state.linear);
    }

#if defined(MA_HAS_SPEEX_RESAMPLER)
    if (pResampler->config.algorithm == ma_resample_algorithm_speex) {
        speex_resampler_destroy((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState);
    }
#endif
}

static ma_result ma_resampler_process_pcm_frames__read__linear(ma_resampler* pResampler, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    return ma_linear_resampler_process_pcm_frames(&pResampler->state.linear, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
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
            speexErr = speex_resampler_process_interleaved_float((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState, (const float*)pFramesInThisIteration, &frameCountInThisIteration, (float*)pFramesOutThisIteration, &frameCountOutThisIteration);
        } else if (pResampler->config.format == ma_format_s16) {
            speexErr = speex_resampler_process_interleaved_int((SpeexResamplerState*)pResampler->state.speex.pSpeexResamplerState, (const spx_int16_t*)pFramesInThisIteration, &frameCountInThisIteration, (spx_int16_t*)pFramesOutThisIteration, &frameCountOutThisIteration);
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

    /* Seeking is supported natively by the linear resampler. */
    return ma_linear_resampler_process_pcm_frames(pResampler, pFramesIn, pFrameCountIn, NULL, pFrameCountOut);
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
            return ma_linear_resampler_set_rate(&pResampler->state.linear, sampleRateIn, sampleRateOut);
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
    /* We use up to 6 decimal places. */
    ma_uint32 n;
    ma_uint32 d;

    d = 1000000;
    n = (ma_uint32)(ratio * d);

    if (n == 0) {
        return MA_INVALID_ARGS; /* Ratio too small. */
    }

    MA_ASSERT(n != 0);
    
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
            return ma_linear_resampler_get_required_input_frame_count(&pResampler->state.linear, outputFrameCount);
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
            return ma_linear_resampler_get_expected_output_frame_count(&pResampler->state.linear, inputFrameCount);
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
            return ma_linear_resampler_get_input_latency(&pResampler->state.linear);
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
            return ma_linear_resampler_get_output_latency(&pResampler->state.linear);
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
