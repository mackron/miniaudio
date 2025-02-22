/* Include ma_reverb_node.h after miniaudio.h */
#ifndef miniaudio_reverb_node_h
#define miniaudio_reverb_node_h

#include "../../../miniaudio.h"
#include "verblib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
The reverb node has one input and one output.
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channels;         /* The number of channels of the source, which will be the same as the output. Must be 1 or 2. */
    ma_uint32 sampleRate;
    float roomSize;
    float damping;
    float width;
    float wetVolume;
    float dryVolume;
    float mode;
} ma_reverb_node_config;

MA_API ma_reverb_node_config ma_reverb_node_config_init(ma_uint32 channels, ma_uint32 sampleRate);


typedef struct
{
    ma_node_base baseNode;
    verblib reverb;
} ma_reverb_node;

MA_API ma_result ma_reverb_node_init(ma_node_graph* pNodeGraph, const ma_reverb_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_reverb_node* pReverbNode);
MA_API void ma_reverb_node_uninit(ma_reverb_node* pReverbNode, const ma_allocation_callbacks* pAllocationCallbacks);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_reverb_node_h */
