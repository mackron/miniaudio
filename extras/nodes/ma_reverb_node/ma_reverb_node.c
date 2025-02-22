#ifndef miniaudio_reverb_node_c
#define miniaudio_reverb_node_c

#define VERBLIB_IMPLEMENTATION
#include "ma_reverb_node.h"

#include <string.h> /* For memset(). */

MA_API ma_reverb_node_config ma_reverb_node_config_init(ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_reverb_node_config config;

    memset(&config, 0, sizeof(config));
    config.nodeConfig = ma_node_config_init();  /* Input and output channels will be set in ma_reverb_node_init(). */
    config.channels   = channels;
    config.sampleRate = sampleRate;
    config.roomSize   = verblib_initialroom;
    config.damping    = verblib_initialdamp;
    config.width      = verblib_initialwidth;
    config.wetVolume  = verblib_initialwet;
    config.dryVolume  = verblib_initialdry;
    config.mode       = verblib_initialmode;

    return config;
}


static void ma_reverb_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_reverb_node* pReverbNode = (ma_reverb_node*)pNode;

    (void)pFrameCountIn;

    verblib_process(&pReverbNode->reverb, ppFramesIn[0], ppFramesOut[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_reverb_node_vtable =
{
    ma_reverb_node_process_pcm_frames,
    NULL,
    1,  /* 1 input bus. */
    1,  /* 1 output bus. */
    MA_NODE_FLAG_CONTINUOUS_PROCESSING  /* Reverb requires continuous processing to ensure the tail get's processed. */
};

MA_API ma_result ma_reverb_node_init(ma_node_graph* pNodeGraph, const ma_reverb_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_reverb_node* pReverbNode)
{
    ma_result result;
    ma_node_config baseConfig;

    if (pReverbNode == NULL) {
        return MA_INVALID_ARGS;
    }

    memset(pReverbNode, 0, sizeof(*pReverbNode));

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (verblib_initialize(&pReverbNode->reverb, (unsigned long)pConfig->sampleRate, (unsigned int)pConfig->channels) == 0) {
        return MA_INVALID_ARGS;
    }

    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable          = &g_ma_reverb_node_vtable;
    baseConfig.pInputChannels  = &pConfig->channels;
    baseConfig.pOutputChannels = &pConfig->channels;

    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pReverbNode->baseNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_reverb_node_uninit(ma_reverb_node* pReverbNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    /* The base node is always uninitialized first. */
    ma_node_uninit(pReverbNode, pAllocationCallbacks);
}

#endif  /* miniaudio_reverb_node_c */
