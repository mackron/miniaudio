/* Data converter. Public domain. */

#ifndef ma_data_converter_h
#define ma_data_converter_h

#include "ma_resampler.h"

typedef struct
{
    ma_format format;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_channel channelMapIn[MA_MAX_CHANNELS];
    ma_channel channelMapOut[MA_MAX_CHANNELS];
    ma_channel_mix_mode mixingMode;
    float weights[MA_MAX_CHANNELS][MA_MAX_CHANNELS];  /* [in][out]. Only used when mixingMode is set to ma_channel_mix_mode_custom_weights. */
} ma_channel_converter_config;

ma_channel_converter_config ma_channel_converter_config_init(ma_format format, ma_uint32 channelsIn, const ma_channel channelMapIn[MA_MAX_CHANNELS], ma_uint32 channelsOut, const ma_channel channelMapOut[MA_MAX_CHANNELS], ma_channel_mix_mode mixingMode);

typedef struct
{
    ma_format format;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_channel channelMapIn[MA_MAX_CHANNELS];
    ma_channel channelMapOut[MA_MAX_CHANNELS];
    ma_channel_mix_mode mixingMode;
    union
    {
        float    f32[MA_MAX_CHANNELS][MA_MAX_CHANNELS];
        ma_int32 s16[MA_MAX_CHANNELS][MA_MAX_CHANNELS];
    } weights;
    ma_bool32 isPassthrough         : 1;
    ma_bool32 isSimpleShuffle       : 1;
    ma_bool32 isSimpleMonoExpansion : 1;
    ma_bool32 isStereoToMono        : 1;
    ma_uint8  shuffleTable[MA_MAX_CHANNELS];
} ma_channel_converter;

ma_result ma_channel_converter_init(const ma_channel_converter_config* pConfig, ma_channel_converter* pConverter);
void ma_channel_converter_uninit(ma_channel_converter* pConverter);
ma_result ma_channel_converter_process_pcm_frames(ma_channel_converter* pConverter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);


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

#ifndef MA_CHANNEL_CONVERTER_FIXED_POINT_SHIFT
#define MA_CHANNEL_CONVERTER_FIXED_POINT_SHIFT  12
#endif

ma_channel_converter_config ma_channel_converter_config_init(ma_format format, ma_uint32 channelsIn, const ma_channel channelMapIn[MA_MAX_CHANNELS], ma_uint32 channelsOut, const ma_channel channelMapOut[MA_MAX_CHANNELS], ma_channel_mix_mode mixingMode)
{
    ma_channel_converter_config config;
    MA_ZERO_OBJECT(&config);
    config.format      = format;
    config.channelsIn  = channelsIn;
    config.channelsOut = channelsOut;
    ma_channel_map_copy(config.channelMapIn,  channelMapIn,  channelsIn);
    ma_channel_map_copy(config.channelMapOut, channelMapOut, channelsOut);
    config.mixingMode  = mixingMode;

    return config;
}

static ma_int32 ma_channel_converter_float_to_fp(float x)
{
    return (ma_int32)(x * (1<<MA_CHANNEL_CONVERTER_FIXED_POINT_SHIFT));
}

static ma_bool32 ma_is_spatial_channel_position(ma_channel channelPosition)
{
    int i;

    if (channelPosition == MA_CHANNEL_NONE || channelPosition == MA_CHANNEL_MONO || channelPosition == MA_CHANNEL_LFE) {
        return MA_FALSE;
    }

    for (i = 0; i < 6; ++i) {   /* Each side of a cube. */
        if (g_maChannelPlaneRatios[channelPosition][i] != 0) {
            return MA_TRUE;
        }
    }

    return MA_FALSE;
}

ma_result ma_channel_converter_init(const ma_channel_converter_config* pConfig, ma_channel_converter* pConverter)
{
    ma_uint32 iChannelIn;
    ma_uint32 iChannelOut;

    if (pConverter == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pConverter);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (!ma_channel_map_valid(pConfig->channelsIn, pConfig->channelMapIn)) {
        return MA_INVALID_ARGS; /* Invalid input channel map. */
    }
    if (!ma_channel_map_valid(pConfig->channelsOut, pConfig->channelMapOut)) {
        return MA_INVALID_ARGS; /* Invalid output channel map. */
    }

    if (pConfig->format != ma_format_s16 && pConfig->format != ma_format_f32) {
        return MA_INVALID_ARGS; /* Invalid format. */
    }

    pConverter->format      = pConfig->format;
    pConverter->channelsIn  = pConfig->channelsIn;
    pConverter->channelsOut = pConfig->channelsOut;
    ma_channel_map_copy(pConverter->channelMapIn,  pConfig->channelMapIn,  pConfig->channelsIn);
    ma_channel_map_copy(pConverter->channelMapOut, pConfig->channelMapOut, pConfig->channelsOut);
    pConverter->mixingMode  = pConfig->mixingMode;

    for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; iChannelIn += 1) {
        for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
            if (pConverter->format == ma_format_s16) {
                pConverter->weights.f32[iChannelIn][iChannelOut] = pConfig->weights[iChannelIn][iChannelOut];
            } else {
                pConverter->weights.s16[iChannelIn][iChannelOut] = ma_channel_converter_float_to_fp(pConfig->weights[iChannelIn][iChannelOut]);
            }
        }
    }
    


    /* If the input and output channels and channel maps are the same we should use a passthrough. */
    if (pConverter->channelsIn == pConverter->channelsOut) {
        if (ma_channel_map_equal(pConverter->channelsIn, pConverter->channelMapIn, pConverter->channelMapOut)) {
            pConverter->isPassthrough = MA_TRUE;
        }
        if (ma_channel_map_blank(pConverter->channelsIn, pConverter->channelMapIn) || ma_channel_map_blank(pConverter->channelsOut, pConverter->channelMapOut)) {
            pConverter->isPassthrough = MA_TRUE;
        }
    }


    /*
    We can use a simple case for expanding the mono channel. This will used when expanding a mono input into any output so long
    as no LFE is present in the output.
    */
    if (!pConverter->isPassthrough) {
        if (pConverter->channelsIn == 1 && pConverter->channelMapIn[0] == MA_CHANNEL_MONO) {
            /* Optimal case if no LFE is in the output channel map. */
            pConverter->isSimpleMonoExpansion = MA_TRUE;
            if (ma_channel_map_contains_channel_position(pConverter->channelsOut, pConverter->channelMapOut, MA_CHANNEL_LFE)) {
                pConverter->isSimpleMonoExpansion = MA_FALSE;
            }
        }
    }

    /* Another optimized case is stereo to mono. */
    if (!pConverter->isPassthrough) {
        if (pConverter->channelsOut == 1 && pConverter->channelMapOut[0] == MA_CHANNEL_MONO && pConverter->channelsIn == 2) {
            /* Optimal case if no LFE is in the input channel map. */
            pConverter->isStereoToMono = MA_TRUE;
            if (ma_channel_map_contains_channel_position(pConverter->channelsIn, pConverter->channelMapIn, MA_CHANNEL_LFE)) {
                pConverter->isStereoToMono = MA_FALSE;
            }
        }
    }


    /*
    Here is where we do a bit of pre-processing to know how each channel should be combined to make up the output. Rules:
    
        1) If it's a passthrough, do nothing - it's just a simple memcpy().
        2) If the channel counts are the same and every channel position in the input map is present in the output map, use a
           simple shuffle. An example might be different 5.1 channel layouts.
        3) Otherwise channels are blended based on spatial locality.
    */
    if (!pConverter->isPassthrough) {
        if (pConverter->channelsIn == pConverter->channelsOut) {
            ma_bool32 areAllChannelPositionsPresent = MA_TRUE;
            for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                ma_bool32 isInputChannelPositionInOutput = MA_FALSE;
                for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
                    if (pConverter->channelMapIn[iChannelIn] == pConverter->channelMapOut[iChannelOut]) {
                        isInputChannelPositionInOutput = MA_TRUE;
                        break;
                    }
                }

                if (!isInputChannelPositionInOutput) {
                    areAllChannelPositionsPresent = MA_FALSE;
                    break;
                }
            }

            if (areAllChannelPositionsPresent) {
                pConverter->isSimpleShuffle = MA_TRUE;

                /*
                All the router will be doing is rearranging channels which means all we need to do is use a shuffling table which is just
                a mapping between the index of the input channel to the index of the output channel.
                */
                for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                    for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
                        if (pConverter->channelMapIn[iChannelIn] == pConverter->channelMapOut[iChannelOut]) {
                            pConverter->shuffleTable[iChannelIn] = (ma_uint8)iChannelOut;
                            break;
                        }
                    }
                }
            }
        }
    }


    /*
    Here is where weights are calculated. Note that we calculate the weights at all times, even when using a passthrough and simple
    shuffling. We use different algorithms for calculating weights depending on our mixing mode.
    
    In simple mode we don't do any blending (except for converting between mono, which is done in a later step). Instead we just
    map 1:1 matching channels. In this mode, if no channels in the input channel map correspond to anything in the output channel
    map, nothing will be heard!
    */

    /* In all cases we need to make sure all channels that are present in both channel maps have a 1:1 mapping. */
    for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
        ma_channel channelPosIn = pConverter->channelMapIn[iChannelIn];

        for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
            ma_channel channelPosOut = pConverter->channelMapOut[iChannelOut];

            if (channelPosIn == channelPosOut) {
                if (pConverter->format == ma_format_s16) {
                    pConverter->weights.s16[iChannelIn][iChannelOut] = (1 << MA_CHANNEL_CONVERTER_FIXED_POINT_SHIFT);
                } else {
                    pConverter->weights.f32[iChannelIn][iChannelOut] = 1;
                }
            }
        }
    }

    /*
    The mono channel is accumulated on all other channels, except LFE. Make sure in this loop we exclude output mono channels since
    they were handled in the pass above.
    */
    for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
        ma_channel channelPosIn = pConverter->channelMapIn[iChannelIn];

        if (channelPosIn == MA_CHANNEL_MONO) {
            for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
                ma_channel channelPosOut = pConverter->channelMapOut[iChannelOut];

                if (channelPosOut != MA_CHANNEL_NONE && channelPosOut != MA_CHANNEL_MONO && channelPosOut != MA_CHANNEL_LFE) {
                    if (pConverter->format == ma_format_s16) {
                        pConverter->weights.s16[iChannelIn][iChannelOut] = (1 << MA_CHANNEL_CONVERTER_FIXED_POINT_SHIFT);
                    } else {
                        pConverter->weights.f32[iChannelIn][iChannelOut] = 1;
                    }
                }
            }
        }
    }

    /* The output mono channel is the average of all non-none, non-mono and non-lfe input channels. */
    {
        ma_uint32 len = 0;
        for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
            ma_channel channelPosIn = pConverter->channelMapIn[iChannelIn];

            if (channelPosIn != MA_CHANNEL_NONE && channelPosIn != MA_CHANNEL_MONO && channelPosIn != MA_CHANNEL_LFE) {
                len += 1;
            }
        }

        if (len > 0) {
            float monoWeight = 1.0f / len;

            for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
                ma_channel channelPosOut = pConverter->channelMapOut[iChannelOut];

                if (channelPosOut == MA_CHANNEL_MONO) {
                    for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                        ma_channel channelPosIn = pConverter->channelMapIn[iChannelIn];

                        if (channelPosIn != MA_CHANNEL_NONE && channelPosIn != MA_CHANNEL_MONO && channelPosIn != MA_CHANNEL_LFE) {
                            if (pConverter->format == ma_format_s16) {
                                pConverter->weights.s16[iChannelIn][iChannelOut] = ma_channel_converter_float_to_fp(monoWeight);
                            } else {
                                pConverter->weights.f32[iChannelIn][iChannelOut] = monoWeight;
                            }
                        }
                    }
                }
            }
        }
    }


    /* Input and output channels that are not present on the other side need to be blended in based on spatial locality. */
    switch (pConverter->mixingMode)
    {
        case ma_channel_mix_mode_rectangular:
        {
            /* Unmapped input channels. */
            for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                ma_channel channelPosIn = pConverter->channelMapIn[iChannelIn];

                if (ma_is_spatial_channel_position(channelPosIn)) {
                    if (!ma_channel_map_contains_channel_position(pConverter->channelsOut, pConverter->channelMapOut, channelPosIn)) {
                        for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
                            ma_channel channelPosOut = pConverter->channelMapOut[iChannelOut];

                            if (ma_is_spatial_channel_position(channelPosOut)) {
                                float weight = 0;
                                if (pConverter->mixingMode == ma_channel_mix_mode_rectangular) {
                                    weight = ma_calculate_channel_position_rectangular_weight(channelPosIn, channelPosOut);
                                }

                                /* Only apply the weight if we haven't already got some contribution from the respective channels. */
                                if (pConverter->format == ma_format_s16) {
                                    if (pConverter->weights.s16[iChannelIn][iChannelOut] == 0) {
                                        pConverter->weights.s16[iChannelIn][iChannelOut] = ma_channel_converter_float_to_fp(weight);
                                    }
                                } else {
                                    if (pConverter->weights.f32[iChannelIn][iChannelOut] == 0) {
                                        pConverter->weights.f32[iChannelIn][iChannelOut] = weight;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            /* Unmapped output channels. */
            for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
                ma_channel channelPosOut = pConverter->channelMapOut[iChannelOut];

                if (ma_is_spatial_channel_position(channelPosOut)) {
                    if (!ma_channel_map_contains_channel_position(pConverter->channelsIn, pConverter->channelMapIn, channelPosOut)) {
                        for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                            ma_channel channelPosIn = pConverter->channelMapIn[iChannelIn];

                            if (ma_is_spatial_channel_position(channelPosIn)) {
                                float weight = 0;
                                if (pConverter->mixingMode == ma_channel_mix_mode_rectangular) {
                                    weight = ma_calculate_channel_position_rectangular_weight(channelPosIn, channelPosOut);
                                }

                                /* Only apply the weight if we haven't already got some contribution from the respective channels. */
                                if (pConverter->format == ma_format_s16) {
                                    if (pConverter->weights.s16[iChannelIn][iChannelOut] == 0) {
                                        pConverter->weights.s16[iChannelIn][iChannelOut] = ma_channel_converter_float_to_fp(weight);
                                    }
                                } else {
                                    if (pConverter->weights.f32[iChannelIn][iChannelOut] == 0) {
                                        pConverter->weights.f32[iChannelIn][iChannelOut] = weight;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } break;

        case ma_channel_mix_mode_custom_weights:
        case ma_channel_mix_mode_simple:
        default:
        {
            /* Fallthrough. */
        } break;
    }


    return MA_SUCCESS;
}

void ma_channel_converter_uninit(ma_channel_converter* pConverter)
{
    if (pConverter == NULL) {
        return;
    }
}

static ma_result ma_channel_converter_process_pcm_frames__passthrough(ma_channel_converter* pConverter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    MA_ASSERT(pConverter != NULL);
    MA_ASSERT(pFramesOut != NULL);
    MA_ASSERT(pFramesIn  != NULL);

    ma_copy_memory_64(pFramesOut, pFramesIn, frameCount * ma_get_bytes_per_frame(pConverter->format, pConverter->channelsOut));
    return MA_SUCCESS;
}

static ma_result ma_channel_converter_process_pcm_frames__simple_shuffle(ma_channel_converter* pConverter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_uint32 iFrame;
    ma_uint32 iChannelIn;

    MA_ASSERT(pConverter != NULL);
    MA_ASSERT(pFramesOut != NULL);
    MA_ASSERT(pFramesIn  != NULL);
    MA_ASSERT(pConverter->channelsIn == pConverter->channelsOut);

    if (pConverter->format == ma_format_s16) {
        /* */ ma_int16* pFramesOutS16 = (      ma_int16*)pFramesOut;
        const ma_int16* pFramesInS16  = (const ma_int16*)pFramesIn;

        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                pFramesOutS16[pConverter->shuffleTable[iChannelIn]] = pFramesInS16[iChannelIn];
            }
        }
    } else {
        /* */ float* pFramesOutF32 = (      float*)pFramesOut;
        const float* pFramesInF32  = (const float*)pFramesIn;

        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                pFramesOutF32[pConverter->shuffleTable[iChannelIn]] = pFramesInF32[iChannelIn];
            }
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_channel_converter_process_pcm_frames__simple_mono_expansion(ma_channel_converter* pConverter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_uint64 iFrame;

    MA_ASSERT(pConverter != NULL);
    MA_ASSERT(pFramesOut != NULL);
    MA_ASSERT(pFramesIn  != NULL);

    if (pConverter->format == ma_format_s16) {
        /* */ ma_int16* pFramesOutS16 = (      ma_int16*)pFramesOut;
        const ma_int16* pFramesInS16  = (const ma_int16*)pFramesIn;

        if (pConverter->channelsOut == 2) {
            for (iFrame = 0; iFrame < frameCount; ++iFrame) {
                pFramesOutS16[iFrame*2 + 0] = pFramesInS16[iFrame];
                pFramesOutS16[iFrame*2 + 1] = pFramesInS16[iFrame];
            }
        } else {
            for (iFrame = 0; iFrame < frameCount; ++iFrame) {
                ma_uint32 iChannel;
                for (iChannel = 0; iChannel < pConverter->channelsOut; iChannel += 1) {
                    pFramesOutS16[iFrame*pConverter->channelsOut + iChannel] = pFramesInS16[iFrame];
                }
            }
        }
    } else {
        /* */ float* pFramesOutF32 = (      float*)pFramesOut;
        const float* pFramesInF32  = (const float*)pFramesIn;

        if (pConverter->channelsOut == 2) {
            for (iFrame = 0; iFrame < frameCount; ++iFrame) {
                pFramesOutF32[iFrame*2 + 0] = pFramesInF32[iFrame];
                pFramesOutF32[iFrame*2 + 1] = pFramesInF32[iFrame];
            }
        } else {
            for (iFrame = 0; iFrame < frameCount; ++iFrame) {
                ma_uint32 iChannel;
                for (iChannel = 0; iChannel < pConverter->channelsOut; iChannel += 1) {
                    pFramesOutF32[iFrame*pConverter->channelsOut + iChannel] = pFramesInF32[iFrame];
                }
            }
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_channel_converter_process_pcm_frames__stereo_to_mono(ma_channel_converter* pConverter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_uint64 iFrame;

    MA_ASSERT(pConverter != NULL);
    MA_ASSERT(pFramesOut != NULL);
    MA_ASSERT(pFramesIn  != NULL);
    MA_ASSERT(pConverter->channelsIn  == 2);
    MA_ASSERT(pConverter->channelsOut == 1);

    if (pConverter->format == ma_format_s16) {
        /* */ ma_int16* pFramesOutS16 = (      ma_int16*)pFramesOut;
        const ma_int16* pFramesInS16  = (const ma_int16*)pFramesIn;

        for (iFrame = 0; iFrame < frameCount; ++iFrame) {
            pFramesOutS16[iFrame] = (ma_int16)(((ma_int32)pFramesInS16[iFrame*2+0] + (ma_int32)pFramesInS16[iFrame*2+1]) / 2);
        }
    } else {
        /* */ float* pFramesOutF32 = (      float*)pFramesOut;
        const float* pFramesInF32  = (const float*)pFramesIn;

        for (iFrame = 0; iFrame < frameCount; ++iFrame) {
            pFramesOutF32[iFrame] = (pFramesInF32[iFrame*2+0] + pFramesInF32[iFrame*2+0]) * 0.5f;
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_channel_converter_process_pcm_frames__weights(ma_channel_converter* pConverter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_uint32 iFrame;
    ma_uint32 iChannelIn;
    ma_uint32 iChannelOut;

    MA_ASSERT(pConverter != NULL);
    MA_ASSERT(pFramesOut != NULL);
    MA_ASSERT(pFramesIn  != NULL);

    /* This is the more complicated case. Each of the output channels is accumulated with 0 or more input channels. */

    /* Clear. */
    ma_zero_memory_64(pFramesOut, frameCount * ma_get_bytes_per_frame(pConverter->format, pConverter->channelsOut));

    /* Accumulate. */
    if (pConverter->format == ma_format_s16) {
        /* */ ma_int16* pFramesOutS16 = (      ma_int16*)pFramesOut;
        const ma_int16* pFramesInS16  = (const ma_int16*)pFramesIn;

        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
                    ma_int32 s = pFramesOutS16[iFrame*pConverter->channelsOut + iChannelOut];
                    s += (pFramesInS16[iFrame*pConverter->channelsIn + iChannelIn] * pConverter->weights.s16[iChannelIn][iChannelOut]) >> MA_CHANNEL_CONVERTER_FIXED_POINT_SHIFT;

                    pFramesOutS16[iFrame*pConverter->channelsOut + iChannelOut] = (ma_int16)ma_clamp(s, -32768, 32767);
                }
            }
        }
    } else {
        /* */ float* pFramesOutF32 = (      float*)pFramesOut;
        const float* pFramesInF32  = (const float*)pFramesIn;

        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            for (iChannelIn = 0; iChannelIn < pConverter->channelsIn; ++iChannelIn) {
                for (iChannelOut = 0; iChannelOut < pConverter->channelsOut; ++iChannelOut) {
                    pFramesOutF32[iFrame*pConverter->channelsOut + iChannelOut] += pFramesInF32[iFrame*pConverter->channelsIn + iChannelIn] * pConverter->weights.f32[iChannelIn][iChannelOut];
                }
            }
        }
    }
    
    return MA_SUCCESS;
}

ma_result ma_channel_converter_process_pcm_frames(ma_channel_converter* pConverter, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    if (pConverter == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pFramesOut == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pFramesIn == NULL) {
        ma_zero_memory_64(pFramesOut, frameCount * ma_get_bytes_per_frame(pConverter->format, pConverter->channelsOut));
        return MA_SUCCESS;
    }

    if (pConverter->isPassthrough) {
        return ma_channel_converter_process_pcm_frames__passthrough(pConverter, pFramesOut, pFramesIn, frameCount);
    } else if (pConverter->isSimpleShuffle) {
        return ma_channel_converter_process_pcm_frames__simple_shuffle(pConverter, pFramesOut, pFramesIn, frameCount);
    } else if (pConverter->isSimpleMonoExpansion) {
        return ma_channel_converter_process_pcm_frames__simple_mono_expansion(pConverter, pFramesOut, pFramesIn, frameCount);
    } else if (pConverter->isStereoToMono) {
        return ma_channel_converter_process_pcm_frames__stereo_to_mono(pConverter, pFramesOut, pFramesIn, frameCount);
    } else {
        return ma_channel_converter_process_pcm_frames__weights(pConverter, pFramesOut, pFramesIn, frameCount);
    }

    return MA_SUCCESS;
}



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
        pConverter->hasChannelRouter        == MA_FALSE &&
        pConverter->hasResampler            == MA_FALSE) {
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
    ma_result result = MA_SUCCESS;
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
                break;
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
                break;
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

    return result;
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
