/* Include ma_vocoder_node.h after miniaudio.h */
#ifndef ma_vocoder_node_h
#define ma_vocoder_node_h

#include "voclib.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
The vocoder node has two inputs and one output. Inputs:

    Input Bus 0: The source/carrier stream.
    Input Bus 1: The excite/modulator stream.

The source (input bus 0) and output must have the same channel count, and is restricted to 1 or 2.
The excite (input bus 1) is restricted to 1 channel.
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channels;         /* The number of channels of the source, which will be the same as the output. Must be 1 or 2. The excite bus must always have one channel. */
    ma_uint32 sampleRate;
    ma_uint32 bands;            /* Defaults to 16. */
    ma_uint32 filtersPerBand;   /* Defaults to 6. */
} ma_vocoder_node_config;

MA_API ma_vocoder_node_config ma_vocoder_node_config_init(ma_uint32 channels, ma_uint32 sampleRate);


typedef struct
{
    ma_node_base baseNode;
    voclib_instance voclib;
} ma_vocoder_node;

MA_API ma_result ma_vocoder_node_init(ma_node_graph* pNodeGraph, const ma_vocoder_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_vocoder_node* pVocoderNode);
MA_API void ma_vocoder_node_uninit(ma_vocoder_node* pVocoderNode, const ma_allocation_callbacks* pAllocationCallbacks);

#ifdef __cplusplus
}
#endif
#endif  /* ma_vocoder_node_h */
