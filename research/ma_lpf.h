#ifndef ma_lpf_h
#define ma_lpf_h

/*
TODO:
  - Document passthrough behaviour of the biquad filter and how it doesn't update previous inputs and outputs.
  - Document how changing biquad constants requires reinitialization of the filter (due to issue above).
*/

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    float a0;
    float a1;
    float a2;
    float b0;
    float b1;
    float b2;
} ma_biquad_config;

ma_biquad_config ma_biquad_config_init(ma_format format, ma_uint32 channels, float a0, float a1, float a2, float b0, float b1, float b2);

typedef struct
{
    ma_biquad_config config;
    ma_bool32 isPassthrough;
    ma_uint32 prevFrameCount;
    float x1[MA_MAX_CHANNELS];   /* x[n-1] */
    float x2[MA_MAX_CHANNELS];   /* x[n-2] */
    float y1[MA_MAX_CHANNELS];   /* y[n-1] */
    float y2[MA_MAX_CHANNELS];   /* y[n-2] */
} ma_biquad;

ma_result ma_biquad_init(const ma_biquad_config* pConfig, ma_biquad* pBQ);
ma_result ma_biquad_process(ma_biquad* pBQ, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);


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
ma_result ma_lpf_process(ma_lpf* pLPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);

#endif  /* ma_lpf_h */




#if defined(MINIAUDIO_IMPLEMENTATION)

ma_biquad_config ma_biquad_config_init(ma_format format, ma_uint32 channels, float a0, float a1, float a2, float b0, float b1, float b2)
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

    if (pConfig->a0 == 0) {
        return MA_INVALID_ARGS; /* Division by zero. */
    }

    /* Currently only supporting f32, but support for other formats will be added later. */
    if (pConfig->format != ma_format_f32) {
        return MA_INVALID_ARGS;
    }

    pBQ->config = *pConfig;

    if (pConfig->a0 == 1 && pConfig->a1 == 0 && pConfig->a2 == 0 &&
        pConfig->b0 == 1 && pConfig->b1 == 0 && pConfig->b2 == 0) {
        pBQ->isPassthrough = MA_TRUE;
    }

    /* Normalize. */
    pBQ->config.a1 /= pBQ->config.a0;
    pBQ->config.a2 /= pBQ->config.a0;
    pBQ->config.b0 /= pBQ->config.a0;
    pBQ->config.b1 /= pBQ->config.a0;
    pBQ->config.b2 /= pBQ->config.a0;

    return MA_SUCCESS;
}

ma_result ma_biquad_process(ma_biquad* pBQ, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_uint32 n;
    ma_uint32 c;
    float a1 = pBQ->config.a1;
    float a2 = pBQ->config.a2;
    float b0 = pBQ->config.b0;
    float b1 = pBQ->config.b1;
    float b2 = pBQ->config.b2;

    if (pBQ == NULL || pFramesOut == NULL || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Fast path for passthrough. */
    if (pBQ->isPassthrough) {
        ma_copy_memory_64(pFramesOut, pFramesIn, frameCount * ma_get_bytes_per_frame(pBQ->config.format, pBQ->config.channels));
        return MA_SUCCESS;
    }

    /* Currently only supporting f32. */
    if (pBQ->config.format == ma_format_f32) {
              float* pY = (      float*)pFramesOut;
        const float* pX = (const float*)pFramesIn;

        for (n = 0; n < frameCount; n += 1) {
            for (c = 0; c < pBQ->config.channels; c += 1) {
                float x2 = pBQ->x2[c];
                float x1 = pBQ->x1[c];
                float x0 = pX[n*pBQ->config.channels + c];
                float y2 = pBQ->y2[c];
                float y1 = pBQ->y1[c];
                float y0 = b0*x0 + b1*x1 + b2*x2 - a1*y1 - a2*y2;
                
                pY[n*pBQ->config.channels + c] = y0;
                pBQ->x2[c] = x1;
                pBQ->x1[c] = x0;
                pBQ->y2[c] = y1;
                pBQ->y1[c] = y0;
            }
        }
    } else {
        return MA_INVALID_ARGS; /* Format not supported. Should never hit this because it's checked in ma_biquad_init(). */
    }

    return MA_SUCCESS;
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

ma_result ma_lpf_init(const ma_lpf_config* pConfig, ma_lpf* pLPF)
{
    ma_result result;
    ma_biquad_config bqConfig;
    double q;
    double w;
    double s;
    double c;
    double a;

    if (pLPF == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pLPF);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pLPF->config = *pConfig;

    q = 1 / sqrt(2);
    w = 2 * MA_PI_D * pConfig->cutoffFrequency / pConfig->sampleRate;
    s = sin(w);
    c = cos(w);
    a = s / (2*q);

    bqConfig.a0 = (float)( 1 + a);
    bqConfig.a1 = (float)(-2 * c);
    bqConfig.a2 = (float)( 1 - a);
    bqConfig.b0 = (float)((1 - c) / 2);
    bqConfig.b1 = (float)( 1 - c);
    bqConfig.b2 = (float)((1 - c) / 2);

    bqConfig.format = pConfig->format;
    bqConfig.channels = pConfig->channels;

    result = ma_biquad_init(&bqConfig, &pLPF->bq);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

ma_result ma_lpf_process(ma_lpf* pLPF, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    if (pLPF == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_biquad_process(&pLPF->bq, pFramesOut, pFramesIn, frameCount);
}

#endif