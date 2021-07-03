/* Include ma_reverb_node.h after miniaudio.h */
#ifndef ma_channel_combiner_node_h
#define ma_channel_combiner_node_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channels;         /* The number of channels of the source, which will be the same as the output. Must be 1 or 2. The excite bus must always have one channel. */
} ma_channel_combiner_node_config;

MA_API ma_channel_combiner_node_config ma_channel_combiner_node_config_init(ma_uint32 channels);


typedef struct
{
    ma_node_base baseNode;
} ma_channel_combiner_node;

MA_API ma_result ma_channel_combiner_node_init(ma_node_graph* pNodeGraph, const ma_channel_combiner_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_channel_combiner_node* pSeparatorNode);
MA_API void ma_channel_combiner_node_uninit(ma_channel_combiner_node* pSeparatorNode, const ma_allocation_callbacks* pAllocationCallbacks);


#ifdef __cplusplus
}
#endif
#endif  /* ma_reverb_node_h */
