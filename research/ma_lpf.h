#ifndef ma_lpf_h
#define ma_lpf_h

/*
TODO:
  - Document how changing biquad constants requires reinitialization of the filter. ma_biquad_reinit().
  - Document how ma_biquad_process_pcm_frames() and ma_lpf_process_pcm_frames() supports in-place filtering by passing in the same buffer for both the input and output.
*/

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
    ma_biquad_config config;
    float x1[MA_MAX_CHANNELS];   /* x[n-1] */
    float x2[MA_MAX_CHANNELS];   /* x[n-2] */
    float y1[MA_MAX_CHANNELS];   /* y[n-1] */
    float y2[MA_MAX_CHANNELS];   /* y[n-2] */
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
    ma_uint32 cutoffFrequency;
} ma_lpf_config;

ma_lpf_config ma_lpf_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 cutoffFrequency);

typedef struct
{
    ma_biquad bq;   /* The low-pass filter is implemented as a biquad filter. */
    ma_lpf_config config;
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

    pBQ->config = *pConfig;

    /* Normalize. */
    pBQ->config.a1 /= pBQ->config.a0;
    pBQ->config.a2 /= pBQ->config.a0;
    pBQ->config.b0 /= pBQ->config.a0;
    pBQ->config.b1 /= pBQ->config.a0;
    pBQ->config.b2 /= pBQ->config.a0;

    return MA_SUCCESS;
}

static MA_INLINE void ma_biquad_process_pcm_frame_f32(ma_biquad* pBQ, float* pY, const float* pX)
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

static MA_INLINE void ma_biquad_process_pcm_frame_s16(ma_biquad* pBQ, ma_int16* pY, const ma_int16* pX)
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

ma_result ma_biquad_process_pcm_frames(ma_biquad* pBQ, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_uint32 n;

    if (pBQ == NULL || pFramesOut == NULL || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Note that the logic below needs to support in-place filtering. That is, it must support the case where pFramesOut and pFramesIn are the same. */

    /* Currently only supporting f32. */
    if (pBQ->config.format == ma_format_f32) {
        /* */ float* pY = (      float*)pFramesOut;
        const float* pX = (const float*)pFramesIn;

        for (n = 0; n < frameCount; n += 1) {
            ma_biquad_process_pcm_frame_f32(pBQ, pY + n*pBQ->config.channels, pX + n*pBQ->config.channels);
            pY += pBQ->config.channels;
            pX += pBQ->config.channels;
        }
    } else if (pBQ->config.format == ma_format_s16) {
        /* */ ma_int16* pY = (      ma_int16*)pFramesOut;
        const ma_int16* pX = (const ma_int16*)pFramesIn;

        for (n = 0; n < frameCount; n += 1) {
            ma_biquad_process_pcm_frame_s16(pBQ, pY, pX);
            pY += pBQ->config.channels;
            pX += pBQ->config.channels;
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


ma_lpf_config ma_lpf_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 cutoffFrequency)
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

    pLPF->config = *pConfig;

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

    pLPF->config = *pConfig;

    bqConfig = ma_lpf__get_biquad_config(pConfig);
    result = ma_biquad_reinit(&bqConfig, &pLPF->bq);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static MA_INLINE void ma_lpf_process_pcm_frame_s16(ma_lpf* pLPF, ma_int16* pFrameOut, const ma_int16* pFrameIn)
{
    ma_biquad_process_pcm_frame_s16(&pLPF->bq, pFrameOut, pFrameIn);
}

static MA_INLINE void ma_lpf_process_pcm_frame_f32(ma_lpf* pLPF, float* pFrameOut, const float* pFrameIn)
{
    ma_biquad_process_pcm_frame_f32(&pLPF->bq, pFrameOut, pFrameIn);
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