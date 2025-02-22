#ifndef miniaudio_vocoder_node_c
#define miniaudio_vocoder_node_c

#define VOCLIB_IMPLEMENTATION
#include "ma_vocoder_node.h"

#include <string.h> /* For memset(). */

MA_API ma_vocoder_node_config ma_vocoder_node_config_init(ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_vocoder_node_config config;

    memset(&config, 0, sizeof(config));
    config.nodeConfig     = ma_node_config_init();   /* Input and output channels will be set in ma_vocoder_node_init(). */
    config.channels       = channels;
    config.sampleRate     = sampleRate;
    config.bands          = 16;
    config.filtersPerBand = 6;

    return config;
}


static void ma_vocoder_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_vocoder_node* pVocoderNode = (ma_vocoder_node*)pNode;

    (void)pFrameCountIn;

    voclib_process(&pVocoderNode->voclib, ppFramesIn[0], ppFramesIn[1], ppFramesOut[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_vocoder_node_vtable =
{
    ma_vocoder_node_process_pcm_frames,
    NULL,
    2,  /* 2 input buses. */
    1,  /* 1 output bus. */
    0
};

MA_API ma_result ma_vocoder_node_init(ma_node_graph* pNodeGraph, const ma_vocoder_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_vocoder_node* pVocoderNode)
{
    ma_result result;
    ma_node_config baseConfig;
    ma_uint32 inputChannels[2];
    ma_uint32 outputChannels[1];

    if (pVocoderNode == NULL) {
        return MA_INVALID_ARGS;
    }

    memset(pVocoderNode, 0, sizeof(*pVocoderNode));

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (voclib_initialize(&pVocoderNode->voclib, (unsigned char)pConfig->bands, (unsigned char)pConfig->filtersPerBand, (unsigned int)pConfig->sampleRate, (unsigned char)pConfig->channels) == 0) {
        return MA_INVALID_ARGS;
    }

    inputChannels [0] = pConfig->channels;   /* Source/carrier. */
    inputChannels [1] = 1;                   /* Excite/modulator. Must always be single channel. */
    outputChannels[0] = pConfig->channels;   /* Output channels is always the same as the source/carrier. */

    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable          = &g_ma_vocoder_node_vtable;
    baseConfig.pInputChannels  = inputChannels;
    baseConfig.pOutputChannels = outputChannels;

    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pVocoderNode->baseNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_vocoder_node_uninit(ma_vocoder_node* pVocoderNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    /* The base node must always be initialized first. */
    ma_node_uninit(pVocoderNode, pAllocationCallbacks);
}

#endif  /* miniaudio_vocoder_node_c */
