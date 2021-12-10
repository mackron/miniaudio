#define MINIAUDIO_IMPLEMENTATION
#include "../../../miniaudio.h"
#include "ma_channel_separator_node.c"
#include "../ma_channel_combiner_node/ma_channel_combiner_node.c"

#include <stdio.h>

#define DEVICE_FORMAT       ma_format_f32       /* Must always be f32 for this example because the node graph system only works with this. */
#define DEVICE_CHANNELS     0                   /* The input file will determine the channel count. */
#define DEVICE_SAMPLE_RATE  48000

/*
In this example we're just separating out the channels with a `ma_channel_separator_node`, and then
combining them back together with a `ma_channel_combiner_node` before playing them back.
*/
static ma_decoder                g_decoder;         /* The decoder that we'll read data from. */
static ma_data_source_node       g_dataSupplyNode;  /* The node that will sit at the root level. Will be reading data from g_dataSupply. */
static ma_channel_separator_node g_separatorNode;   /* The separator node. */
static ma_channel_combiner_node  g_combinerNode;    /* The combiner node. */
static ma_node_graph             g_nodeGraph;       /* The main node graph that we'll be feeding data through. */

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    (void)pInput;
    (void)pDevice;

    /* All we need to do is read from the node graph. */
    ma_node_graph_read_pcm_frames(&g_nodeGraph, pOutput, frameCount, NULL);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_device_config deviceConfig;
    ma_device device;
    ma_node_graph_config nodeGraphConfig;
    ma_channel_separator_node_config separatorNodeConfig;
    ma_channel_combiner_node_config combinerNodeConfig;
    ma_data_source_node_config dataSupplyNodeConfig;
    ma_uint32 iChannel;

    if (argc < 1) {
        printf("No input file.\n");
        return -1;
    }


    /* Decoder. */
    decoderConfig = ma_decoder_config_init(DEVICE_FORMAT, 0, DEVICE_SAMPLE_RATE);

    result = ma_decoder_init_file(argv[1], &decoderConfig, &g_decoder);
    if (result != MA_SUCCESS) {
        printf("Failed to load decoder.\n");
        return -1;
    }


    /* Device. */
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format    = g_decoder.outputFormat;
    deviceConfig.playback.channels  = g_decoder.outputChannels;
    deviceConfig.sampleRate         = g_decoder.outputSampleRate;
    deviceConfig.dataCallback       = data_callback;
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        return result;
    }


    /* Node graph. */
    nodeGraphConfig = ma_node_graph_config_init(device.playback.channels);

    result = ma_node_graph_init(&nodeGraphConfig, NULL, &g_nodeGraph);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize node graph.");
        goto done0;
    }


    /* Combiner. Attached straight to the endpoint. Input will be the separator node. */
    combinerNodeConfig = ma_channel_combiner_node_config_init(device.playback.channels);

    result = ma_channel_combiner_node_init(&g_nodeGraph, &combinerNodeConfig, NULL, &g_combinerNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize channel combiner node.");
        goto done1;
    }

    ma_node_attach_output_bus(&g_combinerNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);


    /*
    Separator. Attached to the combiner. We need to attach each of the outputs of the
    separator to each of the inputs of the combiner.
    */
    separatorNodeConfig = ma_channel_separator_node_config_init(device.playback.channels);

    result = ma_channel_separator_node_init(&g_nodeGraph, &separatorNodeConfig, NULL, &g_separatorNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize channel separator node.");
        goto done2;
    }

    /* The separator and combiner must have the same number of output and input buses respectively. */
    MA_ASSERT(ma_node_get_output_bus_count(&g_separatorNode) == ma_node_get_input_bus_count(&g_combinerNode));

    /* Each of the separator's outputs need to be attached to the corresponding input of the combiner. */
    for (iChannel = 0; iChannel < ma_node_get_output_bus_count(&g_separatorNode); iChannel += 1) {
        ma_node_attach_output_bus(&g_separatorNode, iChannel, &g_combinerNode, iChannel);
    }



    /* Data supply. Attached to input bus 0 of the reverb node. */
    dataSupplyNodeConfig = ma_data_source_node_config_init(&g_decoder);

    result = ma_data_source_node_init(&g_nodeGraph, &dataSupplyNodeConfig, NULL, &g_dataSupplyNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize source node.");
        goto done3;
    }

    ma_node_attach_output_bus(&g_dataSupplyNode, 0, &g_separatorNode, 0);



    /* Now we just start the device and wait for the user to terminate the program. */
    ma_device_start(&device);

    printf("Press Enter to quit...\n");
    getchar();

    /* It's important that we stop the device first or else we'll uninitialize the graph from under the device. */
    ma_device_stop(&device);


/*done4:*/ ma_data_source_node_uninit(&g_dataSupplyNode, NULL);
done3: ma_channel_separator_node_uninit(&g_separatorNode, NULL);
done2: ma_channel_combiner_node_uninit(&g_combinerNode, NULL);
done1: ma_node_graph_uninit(&g_nodeGraph, NULL);
done0: ma_device_uninit(&device);
    
    (void)argc;
    (void)argv;

    return 0;
}