/* Include ma_ltrim_node.h after miniaudio.h */
#ifndef miniaudio_ltrim_node_h
#define miniaudio_ltrim_node_h

#include "../../../miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
The trim node has one input and one output.
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channels;
    float threshold;
} ma_ltrim_node_config;

MA_API ma_ltrim_node_config ma_ltrim_node_config_init(ma_uint32 channels, float threshold);


typedef struct
{
    ma_node_base baseNode;
    float threshold;
    ma_bool32 foundStart;
} ma_ltrim_node;

MA_API ma_result ma_ltrim_node_init(ma_node_graph* pNodeGraph, const ma_ltrim_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_ltrim_node* pTrimNode);
MA_API void ma_ltrim_node_uninit(ma_ltrim_node* pTrimNode, const ma_allocation_callbacks* pAllocationCallbacks);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_ltrim_node_h */
