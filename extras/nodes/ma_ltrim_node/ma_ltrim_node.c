
#include "ma_ltrim_node.h"

MA_API ma_ltrim_node_config ma_ltrim_node_config_init(ma_uint32 channels, float threshold)
{
    ma_ltrim_node_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_node_config_init();  /* Input and output channels will be set in ma_ltrim_node_init(). */
    config.channels   = channels;
    config.threshold  = threshold;

    return config;
}


static void ma_ltrim_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_ltrim_node* pTrimNode = (ma_ltrim_node*)pNode;
    ma_uint32 framesProcessedIn  = 0;
    ma_uint32 framesProcessedOut = 0;
    ma_uint32 channelCount       = ma_node_get_input_channels(pNode, 0);
    
    /*
    If we haven't yet found the start, skip over every input sample until we find a frame outside
    of the threshold.
    */
    if (pTrimNode->foundStart == MA_FALSE) {
        while (framesProcessedIn < *pFrameCountIn) {
            ma_uint32 iChannel = 0;
            for (iChannel = 0; iChannel < channelCount; iChannel += 1) {
                float sample = ppFramesIn[0][framesProcessedIn*channelCount + iChannel];
                if (sample < -pTrimNode->threshold || sample > pTrimNode->threshold) {
                    pTrimNode->foundStart = MA_TRUE;
                    break;
                }
            }

            if (pTrimNode->foundStart) {
                break;  /* The start has been found. Get out of this loop and finish off processing. */
            } else {
                framesProcessedIn += 1;
            }
        }
    }

    /* If there's anything left, just copy it over. */
    framesProcessedOut = ma_min(*pFrameCountOut, *pFrameCountIn - framesProcessedIn);
    ma_copy_pcm_frames(ppFramesOut[0], &ppFramesIn[0][framesProcessedIn], framesProcessedOut, ma_format_f32, channelCount);

    framesProcessedIn += framesProcessedOut;

    /* We always "process" every input frame, but we may only done a partial output. */
    *pFrameCountIn  = framesProcessedIn;
    *pFrameCountOut = framesProcessedOut;
}

static ma_node_vtable g_ma_ltrim_node_vtable =
{
    ma_ltrim_node_process_pcm_frames,
    NULL,
    1,  /* 1 input channel. */
    1,  /* 1 output channel. */
    MA_NODE_FLAG_DIFFERENT_PROCESSING_RATES
};

MA_API ma_result ma_ltrim_node_init(ma_node_graph* pNodeGraph, const ma_ltrim_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_ltrim_node* pTrimNode)
{
    ma_result result;
    ma_node_config baseConfig;

    if (pTrimNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pTrimNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pTrimNode->threshold  = pConfig->threshold;
    pTrimNode->foundStart = MA_FALSE;

    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable          = &g_ma_ltrim_node_vtable;
    baseConfig.pInputChannels  = &pConfig->channels;
    baseConfig.pOutputChannels = &pConfig->channels;

    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pTrimNode->baseNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_ltrim_node_uninit(ma_ltrim_node* pTrimNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    /* The base node is always uninitialized first. */
    ma_node_uninit(pTrimNode, pAllocationCallbacks);
}
