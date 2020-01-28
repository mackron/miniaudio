#ifndef ma_lpf_h
#define ma_lpf_h

/**************************************************************************************************************************************************************

Biquad Filter
=============
Biquad filtering is achieved with the `ma_biquad` API. Example:

    ```c
    ma_biquad_config config = ma_biquad_config_init(ma_format_f32, channels, a0, a1, a2, b0, b1, b2);
    ma_result result = ma_biquad_init(&config, &biquad);
    if (result != MA_SUCCESS) {
        // Error.
    }

    ...

    ma_biquad_process_pcm_frames(&biquad, pFramesOut, pFramesIn, frameCount);
    ```

Biquad filtering is implemented using transposed direct form 2. The denominator coefficients are a0, a1 and a2, and the numerator coefficients are b0, b1 and
b2. The a0 coefficient is required and coefficients must not be pre-normalized.

Supported formats are ma_format_s16 and ma_format_f32. If you need to use a different format you need to convert it yourself beforehand. Input and output
frames are always interleaved.

Filtering can be applied in-place by passing in the same pointer for both the input and output buffers, like so:

    ```c
    ma_biquad_process_pcm_frames(&biquad, pMyData, pMyData, frameCount);
    ```

If you need to change the values of the coefficients, but maintain the values in the registers you can do so with `ma_biquad_reinit()`. This is useful if you
need to change the properties of the filter while keeping the values of registers valid to avoid glitching or whatnot. Do not use `ma_biquad_init()` for this
as it will do a full initialization which involves clearing the registers to 0.

**************************************************************************************************************************************************************/

typedef union
{
    float    f32;
    ma_int32 s32;
} ma_biquad_coefficient;

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    double a0;
    double a1;
    double a2;
    double b0;
    double b1;
    double b2;
} ma_biquad_config;

ma_biquad_config ma_biquad_config_init(ma_format format, ma_uint32 channels, double a0, double a1, double a2, double b0, double b1, double b2);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_biquad_coefficient a1;
    ma_biquad_coefficient a2;
    ma_biquad_coefficient b0;
    ma_biquad_coefficient b1;
    ma_biquad_coefficient b2;
    ma_biquad_coefficient r1[MA_MAX_CHANNELS];
    ma_biquad_coefficient r2[MA_MAX_CHANNELS];
} ma_biquad;

ma_result ma_biquad_init(const ma_biquad_config* pConfig, ma_biquad* pBQ);
ma_result ma_biquad_reinit(const ma_biquad_config* pConfig, ma_biquad* pBQ);
ma_result ma_biquad_process_pcm_frames(ma_biquad* pBQ, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
ma_uint32 ma_biquad_get_latency(ma_biquad* pBQ);


typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    double cutoffFrequency;
} ma_lpf_config;

ma_lpf_config ma_lpf_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency);

typedef struct
{
    ma_biquad bq;   /* The low-pass filter is implemented as a biquad filter. */
} ma_lpf;

ma_result ma_lpf_init(const ma_lpf_config* pConfig, ma_lpf* pLPF);
ma_result ma_lpf_reinit(const ma_lpf_config* pConfig, ma_lpf* pLPF);
ma_result ma_lpf_process_pcm_frames(ma_lpf* pLPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
ma_uint32 ma_lpf_get_latency(ma_lpf* pLPF);

#endif  /* ma_lpf_h */




#if defined(MINIAUDIO_IMPLEMENTATION)

ma_biquad_config ma_biquad_config_init(ma_format format, ma_uint32 channels, double a0, double a1, double a2, double b0, double b1, double b2)
{
    ma_biquad_config config;

    MA_ZERO_OBJECT(&config);
    config.format = format;
    config.channels = channels;
    config.a0 = a0;
    config.a1 = a1;
    config.a2 = a2;
    config.b0 = b0;
    config.b1 = b1;
    config.b2 = b2;

    return config;
}

ma_result ma_biquad_init(const ma_biquad_config* pConfig, ma_biquad* pBQ)
{
    if (pBQ == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pBQ);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_biquad_reinit(pConfig, pBQ);
}

ma_result ma_biquad_reinit(const ma_biquad_config* pConfig, ma_biquad* pBQ)
{
    if (pBQ == NULL || pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->a0 == 0) {
        return MA_INVALID_ARGS; /* Division by zero. */
    }

    /* Currently only supporting f32 and s16, but support for other formats will be added later. */
    if (pConfig->format != ma_format_f32 && pConfig->format != ma_format_s16) {
        return MA_INVALID_ARGS;
    }

    pBQ->format   = pConfig->format;
    pBQ->channels = pConfig->channels;

    /* Normalize. */
    pBQ->a1.f32 = (float)(pConfig->a1 / pConfig->a0);
    pBQ->a2.f32 = (float)(pConfig->a2 / pConfig->a0);
    pBQ->b0.f32 = (float)(pConfig->b0 / pConfig->a0);
    pBQ->b1.f32 = (float)(pConfig->b1 / pConfig->a0);
    pBQ->b2.f32 = (float)(pConfig->b2 / pConfig->a0);

    return MA_SUCCESS;
}

#if 0
static MA_INLINE void ma_biquad_process_pcm_frame_f32__direct_form_1(ma_biquad* pBQ, float* pY, const float* pX)
{
    ma_uint32 c;
    const double a1 = pBQ->config.a1;
    const double a2 = pBQ->config.a2;
    const double b0 = pBQ->config.b0;
    const double b1 = pBQ->config.b1;
    const double b2 = pBQ->config.b2;

    for (c = 0; c < pBQ->config.channels; c += 1) {
        double x2 = pBQ->x2[c];
        double x1 = pBQ->x1[c];
        double x0 = pX[c];
        double y2 = pBQ->y2[c];
        double y1 = pBQ->y1[c];
        double y0 = b0*x0 + b1*x1 + b2*x2 - a1*y1 - a2*y2;
                
        pY[c] = (float)y0;
        pBQ->x2[c] = (float)x1;
        pBQ->x1[c] = (float)x0;
        pBQ->y2[c] = (float)y1;
        pBQ->y1[c] = (float)y0;
    }
}
#endif

static MA_INLINE void ma_biquad_process_pcm_frame_f32__direct_form_2_transposed(ma_biquad* pBQ, float* pY, const float* pX)
{
    ma_uint32 c;
    const float a1 = pBQ->a1.f32;
    const float a2 = pBQ->a2.f32;
    const float b0 = pBQ->b0.f32;
    const float b1 = pBQ->b1.f32;
    const float b2 = pBQ->b2.f32;
    
    for (c = 0; c < pBQ->channels; c += 1) {
        float r1 = pBQ->r1[c].f32;
        float r2 = pBQ->r2[c].f32;
        float x  = pX[c];
        float y;

        y  = b0*x        + r1;
        r1 = b1*x - a1*y + r2;
        r2 = b2*x - a2*y;

        pY[c]          = y;
        pBQ->r1[c].f32 = r1;
        pBQ->r2[c].f32 = r2;
    }
}

#if 0
static MA_INLINE void ma_biquad_process_pcm_frame_s16__direct_form_1(ma_biquad* pBQ, ma_int16* pY, const ma_int16* pX)
{
    ma_uint32 c;
    const double a1 = pBQ->config.a1;
    const double a2 = pBQ->config.a2;
    const double b0 = pBQ->config.b0;
    const double b1 = pBQ->config.b1;
    const double b2 = pBQ->config.b2;

    for (c = 0; c < pBQ->config.channels; c += 1) {
        double x2 = pBQ->x2[c];
        double x1 = pBQ->x1[c];
        double x0 = pX[c] * 0.000030517578125;  /* s16 -> f32 */
        double y2 = pBQ->y2[c];
        double y1 = pBQ->y1[c];
        double y0 = b0*x0 + b1*x1 + b2*x2 - a1*y1 - a2*y2;
                
        pY[c] = (ma_int16)(y0 * 32767.0);       /* f32 -> s16 */
        pBQ->x2[c] = (float)x1;
        pBQ->x1[c] = (float)x0;
        pBQ->y2[c] = (float)y1;
        pBQ->y1[c] = (float)y0;
    }
}
#endif

static MA_INLINE void ma_biquad_process_pcm_frame_s16__direct_form_2_transposed(ma_biquad* pBQ, ma_int16* pY, const ma_int16* pX)
{
    ma_uint32 c;
    const float a1 = pBQ->a1.f32;
    const float a2 = pBQ->a2.f32;
    const float b0 = pBQ->b0.f32;
    const float b1 = pBQ->b1.f32;
    const float b2 = pBQ->b2.f32;
    
    for (c = 0; c < pBQ->channels; c += 1) {
        float r1 = pBQ->r1[c].f32;
        float r2 = pBQ->r2[c].f32;
        float x  = pX[c] / 32767.0f;            /* s16 -> f32 */
        float y;

        y  = b0*x        + r1;
        r1 = b1*x - a1*y + r2;
        r2 = b2*x - a2*y;

        pY[c]          = (ma_int16)(y * 32767.0f);  /* f32 -> s16 */
        pBQ->r1[c].f32 = r1;
        pBQ->r2[c].f32 = r2;
    }
}

ma_result ma_biquad_process_pcm_frames(ma_biquad* pBQ, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_uint32 n;

    if (pBQ == NULL || pFramesOut == NULL || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Note that the logic below needs to support in-place filtering. That is, it must support the case where pFramesOut and pFramesIn are the same. */

    /* Currently only supporting f32. */
    if (pBQ->format == ma_format_f32) {
        /* */ float* pY = (      float*)pFramesOut;
        const float* pX = (const float*)pFramesIn;

        for (n = 0; n < frameCount; n += 1) {
            ma_biquad_process_pcm_frame_f32__direct_form_2_transposed(pBQ, pY + n*pBQ->channels, pX + n*pBQ->channels);
            pY += pBQ->channels;
            pX += pBQ->channels;
        }
    } else if (pBQ->format == ma_format_s16) {
        /* */ ma_int16* pY = (      ma_int16*)pFramesOut;
        const ma_int16* pX = (const ma_int16*)pFramesIn;

        for (n = 0; n < frameCount; n += 1) {
            ma_biquad_process_pcm_frame_s16__direct_form_2_transposed(pBQ, pY, pX);
            pY += pBQ->channels;
            pX += pBQ->channels;
        }
    } else {
        MA_ASSERT(MA_FALSE);
        return MA_INVALID_ARGS; /* Format not supported. Should never hit this because it's checked in ma_biquad_init(). */
    }

    return MA_SUCCESS;
}

ma_uint32 ma_biquad_get_latency(ma_biquad* pBQ)
{
    if (pBQ == NULL) {
        return 0;
    }

    return 2;
}


ma_lpf_config ma_lpf_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency)
{
    ma_lpf_config config;
    
    MA_ZERO_OBJECT(&config);
    config.format = format;
    config.channels = channels;
    config.sampleRate = sampleRate;
    config.cutoffFrequency = cutoffFrequency;

    return config;
}

static MA_INLINE ma_biquad_config ma_lpf__get_biquad_config(const ma_lpf_config* pConfig)
{
    ma_biquad_config bqConfig;
    double q;
    double w;
    double s;
    double c;
    double a;

    MA_ASSERT(pConfig != NULL);

    q = 1 / sqrt(2);
    w = 2 * MA_PI_D * pConfig->cutoffFrequency / pConfig->sampleRate;
    s = sin(w);
    c = cos(w);
    a = s / (2*q);

    bqConfig.a0 = (double)( 1 + a);
    bqConfig.a1 = (double)(-2 * c);
    bqConfig.a2 = (double)( 1 - a);
    bqConfig.b0 = (double)((1 - c) / 2);
    bqConfig.b1 = (double)( 1 - c);
    bqConfig.b2 = (double)((1 - c) / 2);

    bqConfig.format   = pConfig->format;
    bqConfig.channels = pConfig->channels;

    return bqConfig;
}

ma_result ma_lpf_init(const ma_lpf_config* pConfig, ma_lpf* pLPF)
{
    ma_result result;
    ma_biquad_config bqConfig;

    if (pLPF == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pLPF);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    bqConfig = ma_lpf__get_biquad_config(pConfig);
    result = ma_biquad_init(&bqConfig, &pLPF->bq);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

ma_result ma_lpf_reinit(const ma_lpf_config* pConfig, ma_lpf* pLPF)
{
    ma_result result;
    ma_biquad_config bqConfig;

    if (pLPF == NULL || pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    bqConfig = ma_lpf__get_biquad_config(pConfig);
    result = ma_biquad_reinit(&bqConfig, &pLPF->bq);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static MA_INLINE void ma_lpf_process_pcm_frame_s16(ma_lpf* pLPF, ma_int16* pFrameOut, const ma_int16* pFrameIn)
{
    ma_biquad_process_pcm_frame_s16__direct_form_2_transposed(&pLPF->bq, pFrameOut, pFrameIn);
}

static MA_INLINE void ma_lpf_process_pcm_frame_f32(ma_lpf* pLPF, float* pFrameOut, const float* pFrameIn)
{
    ma_biquad_process_pcm_frame_f32__direct_form_2_transposed(&pLPF->bq, pFrameOut, pFrameIn);
}

ma_result ma_lpf_process_pcm_frames(ma_lpf* pLPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    if (pLPF == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_biquad_process_pcm_frames(&pLPF->bq, pFramesOut, pFramesIn, frameCount);
}

ma_uint32 ma_lpf_get_latency(ma_lpf* pLPF)
{
    if (pLPF == NULL) {
        return 0;
    }

    return ma_biquad_get_latency(&pLPF->bq);
}

#endif