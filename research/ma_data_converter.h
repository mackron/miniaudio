/* Data converter. Public domain. */

#ifndef ma_data_converter_h
#define ma_data_converter_h

#include "ma_resampler.h"

typedef struct
{
    ma_format formatIn;
    ma_format formatOut;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    ma_dither_mode ditherMode;
    ma_resample_algorithm resampleAlgorithm;
    ma_bool32 dynamicSampleRate;
    struct
    {
        struct
        {
            ma_uint32 lpfCount;
            double lpfNyquistFactor;
        } linear;
        struct
        {
            int quality;
        } speex;
    } resampling;
} ma_data_converter_config;

ma_data_converter_config ma_data_converter_config_init(ma_format formatIn, ma_format formatOut, ma_uint32 channelsIn, ma_uint32 channelsOut, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);

typedef struct
{
    ma_data_converter_config config;
    ma_resampler resampler;
    ma_bool32 hasPreFormatConversion  : 1;
    ma_bool32 hasPostFormatConversion : 1;
    ma_bool32 hasChannelRouter        : 1;
    ma_bool32 hasResampler            : 1;
    ma_bool32 isPassthrough           : 1;
} ma_data_converter;

ma_result ma_data_converter_init(const ma_data_converter_config* pConfig, ma_data_converter* pConverter);
void ma_data_converter_uninit(ma_data_converter* pConverter);
ma_result ma_data_converter_process_pcm_frames(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
ma_result ma_data_converter_set_rate(ma_data_converter* pConverter, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);
ma_result ma_data_converter_set_rate_ratio(ma_data_converter* pConverter, float ratioInOut);
ma_uint64 ma_data_converter_get_required_input_frame_count(ma_data_converter* pConverter, ma_uint64 outputFrameCount);
ma_uint64 ma_data_converter_get_expected_output_frame_count(ma_data_converter* pConverter, ma_uint64 inputFrameCount);
ma_uint64 ma_data_converter_get_input_latency(ma_data_converter* pConverter);
ma_uint64 ma_data_converter_get_output_latency(ma_data_converter* pConverter);

#endif  /* ma_data_converter_h */

#if defined(MINIAUDIO_IMPLEMENTATION)

#ifndef MA_DATA_CONVERTER_STACK_BUFFER_SIZE
#define MA_DATA_CONVERTER_STACK_BUFFER_SIZE 4096
#endif

ma_data_converter_config ma_data_converter_config_init(ma_format formatIn, ma_format formatOut, ma_uint32 channelsIn, ma_uint32 channelsOut, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut)
{
    ma_data_converter_config config;
    MA_ZERO_OBJECT(&config);
    config.formatIn          = formatIn;
    config.formatOut         = formatOut;
    config.channelsIn        = channelsIn;
    config.channelsOut       = channelsOut;
    config.sampleRateIn      = sampleRateIn;
    config.sampleRateOut     = sampleRateOut;
    config.ditherMode        = ma_dither_mode_none;
    config.resampleAlgorithm = ma_resample_algorithm_linear;
    config.dynamicSampleRate = MA_TRUE; /* Enable dynamic sample rates by default. An optimization is to disable this when the sample rate is the same, but that will disable ma_data_converter_set_rate(). */

    /* Linear resampling defaults. */
    config.resampling.linear.lpfCount = 1;
    config.resampling.linear.lpfNyquistFactor = 1;

    /* Speex resampling defaults. */
    config.resampling.speex.quality = 3;

    return config;
}

ma_result ma_data_converter_init(const ma_data_converter_config* pConfig, ma_data_converter* pConverter)
{
    ma_result result;

    if (pConverter == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pConverter);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pConverter->config = *pConfig;

    /* All of our data conversion stages support s16 and f32 natively. Anything else will require a pre- and/or post-conversion step. */
    if (pConverter->config.formatIn != ma_format_s16 && pConverter->config.formatIn != ma_format_f32) {
        pConverter->hasPreFormatConversion = MA_TRUE;
    }
    if (pConverter->config.formatOut != ma_format_s16 && pConverter->config.formatOut != ma_format_f32) {
        pConverter->hasPostFormatConversion = MA_TRUE;
    }


    /* Channel router. */
    if (pConverter->config.channelsIn != pConverter->config.channelsOut) {
        pConverter->hasChannelRouter = MA_TRUE;
    }


    /* Always enable dynamic sample rates if the input sample rate is different because we're always going to need a resampler in this case anyway. */
    if (pConverter->config.dynamicSampleRate == MA_FALSE) {
        pConverter->config.dynamicSampleRate = pConverter->config.sampleRateIn != pConverter->config.sampleRateOut;
    }

    /* Resampler. */
    if (pConverter->config.dynamicSampleRate) {
        ma_resampler_config resamplerConfig;
        ma_format resamplerFormat;
        ma_uint32 resamplerChannels;

        if (pConverter->config.formatIn == ma_format_s16) {
            resamplerFormat = ma_format_s16;
        } else {
            resamplerFormat = ma_format_f32;
        }

        /* The resampler is the most expensive part of the conversion process, so we need to do it at the stage where the channel count is at it's lowest. */
        if (pConverter->config.channelsIn < pConverter->config.channelsOut) {
            resamplerChannels = pConverter->config.channelsIn;
        } else {
            resamplerChannels = pConverter->config.channelsOut;
        }

        resamplerConfig = ma_resampler_config_init(resamplerFormat, resamplerChannels, pConverter->config.sampleRateIn, pConverter->config.sampleRateOut, pConverter->config.resampleAlgorithm);
        resamplerConfig.linear.lpfCount         = pConverter->config.resampling.linear.lpfCount;
        resamplerConfig.linear.lpfNyquistFactor = pConverter->config.resampling.linear.lpfNyquistFactor;
        resamplerConfig.speex.quality           = pConverter->config.resampling.speex.quality;

        result = ma_resampler_init(&resamplerConfig, &pConverter->resampler);
        if (result != MA_SUCCESS) {
            return result;
        }

        pConverter->hasResampler = MA_TRUE;
    }

    /* We can enable passthrough optimizations if applicable. Note that we'll only be able to do this if the sample rate is static. */
    if (pConverter->hasPreFormatConversion  == MA_FALSE &&
        pConverter->hasPostFormatConversion == MA_FALSE &&
        pConverter->hasChannelRouter       == MA_FALSE &&
        pConverter->hasResampler           == MA_FALSE) {
        pConverter->isPassthrough = MA_TRUE;
    }

    return MA_SUCCESS;
}

void ma_data_converter_uninit(ma_data_converter* pConverter)
{
    if (pConverter == NULL) {
        return;
    }

    if (pConverter->hasResampler) {
        ma_resampler_uninit(&pConverter->resampler);
    }
}

static ma_result ma_data_converter_process_pcm_frames__passthrough(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut;
    ma_uint64 frameCount;

    MA_ASSERT(pConverter != NULL);
    
    frameCountIn = 0;
    if (pFrameCountIn != NULL) {
        frameCountIn = *pFrameCountIn;
    }

    frameCountOut = 0;
    if (pFrameCountOut != NULL) {
        frameCountOut = *pFrameCountOut;
    }

    frameCount = ma_min(frameCountIn, frameCountOut);

    if (pFramesOut != NULL) {
        if (pFramesIn != NULL) {
            ma_copy_memory_64(pFramesOut, pFramesIn, frameCount * ma_get_bytes_per_frame(pConverter->config.formatOut, pConverter->config.channelsOut));
        } else {
            ma_zero_memory_64(pFramesOut,            frameCount * ma_get_bytes_per_frame(pConverter->config.formatOut, pConverter->config.channelsOut));
        }
    }

    if (pFrameCountIn != NULL) {
        *pFrameCountIn = frameCount;
    }
    if (pFrameCountOut != NULL) {
        *pFrameCountOut = frameCount;
    }

    return MA_SUCCESS;
}

static ma_result ma_data_converter_process_pcm_frames__format_only(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut;
    ma_uint64 frameCount;

    MA_ASSERT(pConverter != NULL);
    
    frameCountIn = 0;
    if (pFrameCountIn != NULL) {
        frameCountIn = *pFrameCountIn;
    }

    frameCountOut = 0;
    if (pFrameCountOut != NULL) {
        frameCountOut = *pFrameCountOut;
    }

    frameCount = ma_min(frameCountIn, frameCountOut);

    if (pFramesOut != NULL) {
        if (pFramesIn != NULL) {
            ma_pcm_convert(pFramesOut, pConverter->config.formatOut, pFramesIn, pConverter->config.formatIn, frameCount * pConverter->config.channelsIn, pConverter->config.ditherMode);
        } else {
            ma_zero_memory_64(pFramesOut, frameCount * ma_get_bytes_per_frame(pConverter->config.formatOut, pConverter->config.channelsOut));
        }
    }

    if (pFrameCountIn != NULL) {
        *pFrameCountIn = frameCount;
    }
    if (pFrameCountOut != NULL) {
        *pFrameCountOut = frameCount;
    }

    return MA_SUCCESS;
}


static ma_result ma_data_converter_process_pcm_frames__resample_with_format_conversion(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_result result;
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut;
    ma_uint64 framesProcessedIn;
    ma_uint64 framesProcessedOut;

    MA_ASSERT(pConverter != NULL);

    frameCountIn = 0;
    if (pFrameCountIn != NULL) {
        frameCountIn = *pFrameCountIn;
    }

    frameCountOut = 0;
    if (pFrameCountOut != NULL) {
        frameCountOut = *pFrameCountOut;
    }

    framesProcessedIn  = 0;
    framesProcessedOut = 0;

    while (framesProcessedIn < frameCountIn && framesProcessedOut < frameCountOut) {
        ma_uint8 pTempBufferOut[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
        const ma_uint32 tempBufferOutCap = sizeof(pTempBufferOut) / ma_get_bytes_per_frame(pConverter->resampler.config.format, pConverter->resampler.config.channels);
        const void* pFramesInThisIteration;
        /* */ void* pFramesOutThisIteration;
        ma_uint64 frameCountInThisIteration;
        ma_uint64 frameCountOutThisIteration;

        if (pFramesIn != NULL) {
            pFramesInThisIteration = ma_offset_ptr(pFramesIn, framesProcessedIn * ma_get_bytes_per_frame(pConverter->resampler.config.format, pConverter->resampler.config.channels));
        } else {
            pFramesInThisIteration = NULL;
        }

        if (pFramesOut != NULL) {
            pFramesOutThisIteration = ma_offset_ptr(pFramesOut, framesProcessedOut * ma_get_bytes_per_frame(pConverter->config.formatOut, pConverter->config.channelsOut));
        } else {
            pFramesOutThisIteration = NULL;
        }

        /* Do a pre format conversion if necessary. */
        if (pConverter->hasPreFormatConversion) {
            ma_uint8 pTempBufferIn[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            const ma_uint32 tempBufferInCap = sizeof(pTempBufferIn) / ma_get_bytes_per_frame(pConverter->resampler.config.format, pConverter->resampler.config.channels);

            frameCountInThisIteration  = (frameCountIn - framesProcessedIn);
            if (frameCountInThisIteration > tempBufferInCap) {
                frameCountInThisIteration = tempBufferInCap;
            }

            if (pFramesInThisIteration != NULL) {
                ma_pcm_convert(pTempBufferIn, pConverter->resampler.config.format, pFramesInThisIteration, pConverter->config.formatIn, frameCountInThisIteration, pConverter->config.ditherMode);
            } else {
                MA_ZERO_MEMORY(pTempBufferIn, sizeof(pTempBufferIn));
            }

            frameCountOutThisIteration = (frameCountOut - framesProcessedOut);

            if (pConverter->hasPostFormatConversion) {
                /* Both input and output conversion required. Output to the temp buffer. */
                if (frameCountOutThisIteration > tempBufferOutCap) {
                    frameCountOutThisIteration = tempBufferOutCap;
                }

                result = ma_resampler_process_pcm_frames(&pConverter->resampler, pTempBufferIn, &frameCountInThisIteration, pTempBufferOut, &frameCountOutThisIteration);
            } else {
                /* Only pre-format required. Output straight to the output buffer. */
                result = ma_resampler_process_pcm_frames(&pConverter->resampler, pTempBufferIn, &frameCountInThisIteration, pFramesOutThisIteration, &frameCountOutThisIteration);
            }

            if (result != MA_SUCCESS) {
                return result;
            }
        } else {
            /* No pre-format required. Just read straight from the input buffer. */
            MA_ASSERT(pConverter->hasPostFormatConversion == MA_TRUE);

            frameCountInThisIteration  = (frameCountIn  - framesProcessedIn);
            frameCountOutThisIteration = (frameCountOut - framesProcessedOut);
            if (frameCountOutThisIteration > tempBufferOutCap) {
                frameCountOutThisIteration = tempBufferOutCap;
            }

            result = ma_resampler_process_pcm_frames(&pConverter->resampler, pFramesInThisIteration, &frameCountInThisIteration, pTempBufferOut, &frameCountOutThisIteration);
            if (result != MA_SUCCESS) {
                return result;
            }
        }

        /* If we are doing a post format conversion we need to do that now. */
        if (pConverter->hasPostFormatConversion) {
            if (pFramesOutThisIteration != NULL) {
                ma_pcm_convert(pFramesOutThisIteration, pConverter->config.formatOut, pTempBufferOut, pConverter->resampler.config.format, frameCountOutThisIteration * pConverter->resampler.config.channels, pConverter->config.ditherMode);
            }
        }

        framesProcessedIn  += frameCountInThisIteration;
        framesProcessedOut += frameCountOutThisIteration;
    }

    if (pFrameCountIn != NULL) {
        *pFrameCountIn = framesProcessedIn;
    }
    if (pFrameCountOut != NULL) {
        *pFrameCountOut = framesProcessedOut;
    }

    return MA_SUCCESS;
}

static ma_result ma_data_converter_process_pcm_frames__resample_only(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pConverter != NULL);

    if (pConverter->hasPreFormatConversion == MA_FALSE && pConverter->hasPostFormatConversion == MA_FALSE) {
        /* Neither pre- nor post-format required. This is simple case where only resampling is required. */
        return ma_resampler_process_pcm_frames(&pConverter->resampler, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        /* Format conversion required. */
        return ma_data_converter_process_pcm_frames__resample_with_format_conversion(pConverter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    }
}

static ma_result ma_data_converter_process_pcm_frames__channels_only(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pConverter != NULL);

    (void)pConverter;
    (void)pFramesIn;
    (void)pFrameCountIn;
    (void)pFramesOut;
    (void)pFrameCountOut;
    return MA_INVALID_OPERATION;
}

static ma_result ma_data_converter_process_pcm_frames__resampling_first(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pConverter != NULL);

    (void)pConverter;
    (void)pFramesIn;
    (void)pFrameCountIn;
    (void)pFramesOut;
    (void)pFrameCountOut;
    return MA_INVALID_OPERATION;
}

static ma_result ma_data_converter_process_pcm_frames__channels_first(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    MA_ASSERT(pConverter != NULL);

    (void)pConverter;
    (void)pFramesIn;
    (void)pFrameCountIn;
    (void)pFramesOut;
    (void)pFrameCountOut;
    return MA_INVALID_OPERATION;
}

ma_result ma_data_converter_process_pcm_frames(ma_data_converter* pConverter, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    if (pConverter == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConverter->isPassthrough) {
        return ma_data_converter_process_pcm_frames__passthrough(pConverter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    }

    /*
    Here is where the real work is done. Getting here means we're not using a passthrough and we need to move the data through each of the relevant stages. The order
    of our stages depends on the input and output channel count. If the input channels is less than the output channels we want to do sample rate conversion first so
    that it has less work (resampling is the most expensive part of format conversion).
    */
    if (pConverter->config.channelsIn < pConverter->config.channelsOut) {
        /* Do resampling first, if necessary. */
        MA_ASSERT(pConverter->hasChannelRouter == MA_TRUE);

        if (pConverter->hasResampler) {
            /* Channel routing first. */
            return ma_data_converter_process_pcm_frames__channels_first(pConverter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
        } else {
            /* Resampling not required. */
            return ma_data_converter_process_pcm_frames__channels_only(pConverter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
        }
    } else {
        /* Do channel conversion first, if necessary. */
        if (pConverter->hasChannelRouter) {
            if (pConverter->hasResampler) {
                /* Resampling first. */
                return ma_data_converter_process_pcm_frames__resampling_first(pConverter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
            } else {
                /* Resampling not required. */
                return ma_data_converter_process_pcm_frames__channels_only(pConverter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
            }
        } else {
            /* Channel routing not required. */
            if (pConverter->hasResampler) {
                /* Resampling only. */
                return ma_data_converter_process_pcm_frames__resample_only(pConverter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
            } else {
                /* No channel routing nor resampling required. Just format conversion. */
                return ma_data_converter_process_pcm_frames__format_only(pConverter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
            }
        }
    }
}

ma_result ma_data_converter_set_rate(ma_data_converter* pConverter, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut)
{
    if (pConverter == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConverter->hasResampler == MA_FALSE) {
        return MA_INVALID_OPERATION;    /* Dynamic resampling not enabled. */
    }

    return ma_resampler_set_rate(&pConverter->resampler, sampleRateIn, sampleRateOut);
}

ma_result ma_data_converter_set_rate_ratio(ma_data_converter* pConverter, float ratioInOut)
{
    if (pConverter == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConverter->hasResampler == MA_FALSE) {
        return MA_INVALID_OPERATION;    /* Dynamic resampling not enabled. */
    }

    return ma_resampler_set_rate_ratio(&pConverter->resampler, ratioInOut);
}

ma_uint64 ma_data_converter_get_required_input_frame_count(ma_data_converter* pConverter, ma_uint64 outputFrameCount)
{
    if (pConverter == NULL) {
        return 0;
    }

    if (pConverter->hasResampler) {
        return ma_resampler_get_required_input_frame_count(&pConverter->resampler, outputFrameCount);
    } else {
        return outputFrameCount;    /* 1:1 */
    }
}

ma_uint64 ma_data_converter_get_expected_output_frame_count(ma_data_converter* pConverter, ma_uint64 inputFrameCount)
{
    if (pConverter == NULL) {
        return 0;
    }

    if (pConverter->hasResampler) {
        return ma_resampler_get_expected_output_frame_count(&pConverter->resampler, inputFrameCount);
    } else {
        return inputFrameCount;     /* 1:1 */
    }
}

ma_uint64 ma_data_converter_get_input_latency(ma_data_converter* pConverter)
{
    if (pConverter == NULL) {
        return 0;
    }

    if (pConverter->hasResampler) {
        return ma_resampler_get_input_latency(&pConverter->resampler);
    }

    return 0;   /* No latency without a resampler. */
}

ma_uint64 ma_data_converter_get_output_latency(ma_data_converter* pConverter)
{
    if (pConverter == NULL) {
        return 0;
    }

    if (pConverter->hasResampler) {
        return ma_resampler_get_output_latency(&pConverter->resampler);
    }

    return 0;   /* No latency without a resampler. */
}

#endif
