/* Include ma_channel_separator_node.h after miniaudio.h */
#ifndef miniaudio_channel_separator_node_h
#define miniaudio_channel_separator_node_h

#include "../../../miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channels;
} ma_channel_separator_node_config;

MA_API ma_channel_separator_node_config ma_channel_separator_node_config_init(ma_uint32 channels);


typedef struct
{
    ma_node_base baseNode;
} ma_channel_separator_node;

MA_API ma_result ma_channel_separator_node_init(ma_node_graph* pNodeGraph, const ma_channel_separator_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_channel_separator_node* pSeparatorNode);
MA_API void ma_channel_separator_node_uninit(ma_channel_separator_node* pSeparatorNode, const ma_allocation_callbacks* pAllocationCallbacks);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_channel_separator_node_h */
