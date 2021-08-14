
#include "ma_channel_combiner_node.h"

MA_API ma_channel_combiner_node_config ma_channel_combiner_node_config_init(ma_uint32 channels)
{
    ma_channel_combiner_node_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_node_config_init();  /* Input and output channels will be set in ma_channel_combiner_node_init(). */
    config.channels   = channels;

    return config;
}


static void ma_channel_combiner_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_channel_combiner_node* pCombinerNode = (ma_channel_combiner_node*)pNode;

    (void)pFrameCountIn;

    ma_interleave_pcm_frames(ma_format_f32, ma_node_get_output_channels(pCombinerNode, 0), *pFrameCountOut, (const void**)ppFramesIn, (void*)ppFramesOut[0]);
}

static ma_node_vtable g_ma_channel_combiner_node_vtable =
{
    ma_channel_combiner_node_process_pcm_frames,
    NULL,
    MA_NODE_BUS_COUNT_UNKNOWN,  /* Input bus count is determined by the channel count and is unknown until the node instance is initialized. */
    1,  /* 1 output bus. */
    0   /* Default flags. */
};

MA_API ma_result ma_channel_combiner_node_init(ma_node_graph* pNodeGraph, const ma_channel_combiner_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_channel_combiner_node* pCombinerNode)
{
    ma_result result;
    ma_node_config baseConfig;
    ma_uint32 inputChannels[MA_MAX_NODE_BUS_COUNT];
    ma_uint32 outputChannels[1];
    ma_uint32 iChannel;

    if (pCombinerNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pCombinerNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    /* All input channels are mono. */
    for (iChannel = 0; iChannel < pConfig->channels; iChannel += 1) {
        inputChannels[iChannel] = 1;
    }

    outputChannels[0] = pConfig->channels;

    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable          = &g_ma_channel_combiner_node_vtable;
    baseConfig.inputBusCount   = pConfig->channels; /* The vtable has an unknown channel count, so must specify it here. */
    baseConfig.pInputChannels  = inputChannels;
    baseConfig.pOutputChannels = outputChannels;

    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pCombinerNode->baseNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_channel_combiner_node_uninit(ma_channel_combiner_node* pCombinerNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    /* The base node is always uninitialized first. */
    ma_node_uninit(pCombinerNode, pAllocationCallbacks);
}