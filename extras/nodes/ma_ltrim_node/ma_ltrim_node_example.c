#define MINIAUDIO_IMPLEMENTATION
#include "../../../miniaudio.h"
#include "ma_ltrim_node.c"

#include <stdio.h>

#define DEVICE_FORMAT       ma_format_f32       /* Must always be f32 for this example because the node graph system only works with this. */
#define DEVICE_CHANNELS     0                   /* The input file will determine the channel count. */
#define DEVICE_SAMPLE_RATE  0                   /* The input file will determine the sample rate. */

static ma_decoder          g_decoder;         /* The decoder that we'll read data from. */
static ma_data_source_node g_dataSupplyNode;  /* The node that will sit at the root level. Will be reading data from g_dataSupply. */
static ma_ltrim_node       g_trimNode;        /* The trim node. */
static ma_node_graph       g_nodeGraph;       /* The main node graph that we'll be feeding data through. */

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
    ma_ltrim_node_config trimNodeConfig;
    ma_data_source_node_config dataSupplyNodeConfig;

    if (argc < 1) {
        printf("No input file.\n");
        return -1;
    }


    /* Decoder. */
    decoderConfig = ma_decoder_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE);

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


    /* Trimmer. Attached straight to the endpoint. Input will be the data source node. */
    trimNodeConfig = ma_ltrim_node_config_init(device.playback.channels, 0);

    result = ma_ltrim_node_init(&g_nodeGraph, &trimNodeConfig, NULL, &g_trimNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize ltrim node.");
        goto done1;
    }

    ma_node_attach_output_bus(&g_trimNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);


    /* Data supply. */
    dataSupplyNodeConfig = ma_data_source_node_config_init(&g_decoder);

    result = ma_data_source_node_init(&g_nodeGraph, &dataSupplyNodeConfig, NULL, &g_dataSupplyNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize data source node.");
        goto done2;
    }

    ma_node_attach_output_bus(&g_dataSupplyNode, 0, &g_trimNode, 0);



    /* Now we just start the device and wait for the user to terminate the program. */
    ma_device_start(&device);

    printf("Press Enter to quit...\n");
    getchar();

    /* It's important that we stop the device first or else we'll uninitialize the graph from under the device. */
    ma_device_stop(&device);


/*done3:*/ ma_data_source_node_uninit(&g_dataSupplyNode, NULL);
done2: ma_ltrim_node_uninit(&g_trimNode, NULL);
done1: ma_node_graph_uninit(&g_nodeGraph, NULL);
done0: ma_device_uninit(&device);

    return 0;
}