#define MINIAUDIO_IMPLEMENTATION
#include "../../../miniaudio.h"
#include "ma_reverb_node.c"

#include <stdio.h>

#define DEVICE_FORMAT       ma_format_f32       /* Must always be f32 for this example because the node graph system only works with this. */
#define DEVICE_CHANNELS     1                   /* For this example, always set to 1. */
#define DEVICE_SAMPLE_RATE  48000               /* Cannot be less than 22050 for this example. */

static ma_audio_buffer_ref g_dataSupply;        /* The underlying data source of the source node. */
static ma_data_source_node g_dataSupplyNode;    /* The node that will sit at the root level. Will be reading data from g_dataSupply. */
static ma_reverb_node      g_reverbNode;        /* The reverb node. */
static ma_node_graph       g_nodeGraph;         /* The main node graph that we'll be feeding data through. */

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MA_ASSERT(pDevice->capture.format == pDevice->playback.format && pDevice->capture.format == ma_format_f32);
    MA_ASSERT(pDevice->capture.channels == pDevice->playback.channels);

    /*
    The node graph system is a pulling style of API. At the lowest level of the chain will be a 
    node acting as a data source for the purpose of delivering the initial audio data. In our case,
    the data source is our `pInput` buffer. We need to update the underlying data source so that it
    read data from `pInput`.
    */
    ma_audio_buffer_ref_set_data(&g_dataSupply, pInput, frameCount);

    /* With the source buffer configured we can now read directly from the node graph. */
    ma_node_graph_read_pcm_frames(&g_nodeGraph, pOutput, frameCount, NULL);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
    ma_node_graph_config nodeGraphConfig;
    ma_reverb_node_config reverbNodeConfig;
    ma_data_source_node_config dataSupplyNodeConfig;

    deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.pDeviceID  = NULL;
    deviceConfig.capture.format     = DEVICE_FORMAT;
    deviceConfig.capture.channels   = DEVICE_CHANNELS;
    deviceConfig.capture.shareMode  = ma_share_mode_shared;
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format    = DEVICE_FORMAT;
    deviceConfig.playback.channels  = DEVICE_CHANNELS;
    deviceConfig.sampleRate         = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback       = data_callback;
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        return result;
    }


    /* Node graph. */
    nodeGraphConfig = ma_node_graph_config_init(device.capture.channels);

    result = ma_node_graph_init(&nodeGraphConfig, NULL, &g_nodeGraph);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize node graph.");
        goto done0;
    }


    /* Reverb. Attached straight to the endpoint. */
    reverbNodeConfig = ma_reverb_node_config_init(device.capture.channels, device.sampleRate);

    result = ma_reverb_node_init(&g_nodeGraph, &reverbNodeConfig, NULL, &g_reverbNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize reverb node.");
        goto done1;
    }

    ma_node_attach_output_bus(&g_reverbNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);


    /* Data supply. Attached to input bus 0 of the reverb node. */
    result = ma_audio_buffer_ref_init(device.capture.format, device.capture.channels, NULL, 0, &g_dataSupply);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio buffer for source.");
        goto done2;
    }

    dataSupplyNodeConfig = ma_data_source_node_config_init(&g_dataSupply);

    result = ma_data_source_node_init(&g_nodeGraph, &dataSupplyNodeConfig, NULL, &g_dataSupplyNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize source node.");
        goto done2;
    }

    ma_node_attach_output_bus(&g_dataSupplyNode, 0, &g_reverbNode, 0);



    /* Now we just start the device and wait for the user to terminate the program. */
    ma_device_start(&device);

    printf("Press Enter to quit...\n");
    getchar();

    /* It's important that we stop the device first or else we'll uninitialize the graph from under the device. */
    ma_device_stop(&device);


/*done3:*/ ma_data_source_node_uninit(&g_dataSupplyNode, NULL);
done2: ma_reverb_node_uninit(&g_reverbNode, NULL);
done1: ma_node_graph_uninit(&g_nodeGraph, NULL);
done0: ma_device_uninit(&device);
    
    (void)argc;
    (void)argv;

    return 0;
}