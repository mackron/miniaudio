/*
Demonstrates how to apply an effect to a duplex stream using the node graph system.

This example applies a vocoder effect to the input stream before outputting it. A custom node
called `ma_vocoder_node` is used to achieve the effect which can be found in the extras folder in
the miniaudio repository. The vocoder node uses https://github.com/blastbay/voclib to achieve the
effect.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"
#include "../extras/nodes/ma_vocoder_node/ma_vocoder_node.c"

#include <stdio.h>

#define DEVICE_FORMAT      ma_format_f32;   /* Must always be f32 for this example because the node graph system only works with this. */
#define DEVICE_CHANNELS    1                /* For this example, always set to 1. */

static ma_waveform         g_sourceData;    /* The underlying data source of the source node. */
static ma_audio_buffer_ref g_exciteData;    /* The underlying data source of the excite node. */
static ma_data_source_node g_sourceNode;    /* A data source node containing the source data we'll be sending through to the vocoder. This will be routed into the first bus of the vocoder node. */
static ma_data_source_node g_exciteNode;    /* A data source node containing the excite data we'll be sending through to the vocoder. This will be routed into the second bus of the vocoder node. */
static ma_vocoder_node     g_vocoderNode;   /* The vocoder node. */
static ma_node_graph       g_nodeGraph;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MA_ASSERT(pDevice->capture.format == pDevice->playback.format);
    MA_ASSERT(pDevice->capture.channels == pDevice->playback.channels);

    /*
    The node graph system is a pulling style of API. At the lowest level of the chain will be a 
    node acting as a data source for the purpose of delivering the initial audio data. In our case,
    the data source is our `pInput` buffer. We need to update the underlying data source so that it
    read data from `pInput`.
    */
    ma_audio_buffer_ref_set_data(&g_exciteData, pInput, frameCount);

    /* With the source buffer configured we can now read directly from the node graph. */
    ma_node_graph_read_pcm_frames(&g_nodeGraph, pOutput, frameCount, NULL);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
    ma_node_graph_config nodeGraphConfig;
    ma_vocoder_node_config vocoderNodeConfig;
    ma_data_source_node_config sourceNodeConfig;
    ma_data_source_node_config exciteNodeConfig;
    ma_waveform_config waveformConfig;

    deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.pDeviceID  = NULL;
    deviceConfig.capture.format     = DEVICE_FORMAT;
    deviceConfig.capture.channels   = DEVICE_CHANNELS;
    deviceConfig.capture.shareMode  = ma_share_mode_shared;
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format    = DEVICE_FORMAT;
    deviceConfig.playback.channels  = DEVICE_CHANNELS;
    deviceConfig.dataCallback       = data_callback;
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        return result;
    }


    /* Now we can setup our node graph. */
    nodeGraphConfig = ma_node_graph_config_init(device.capture.channels);

    result = ma_node_graph_init(&nodeGraphConfig, NULL, &g_nodeGraph);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize node graph.");
        goto done0;
    }


    /* Vocoder. Attached straight to the endpoint. */
    vocoderNodeConfig = ma_vocoder_node_config_init(device.capture.channels, device.sampleRate);

    result = ma_vocoder_node_init(&g_nodeGraph, &vocoderNodeConfig, NULL, &g_vocoderNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize vocoder node.");
        goto done1;
    }

    ma_node_attach_output_bus(&g_vocoderNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);

    /* Amplify the volume of the vocoder output because in my testing it is a bit quiet. */
    ma_node_set_output_bus_volume(&g_vocoderNode, 0, 4);


    /* Source/carrier. Attached to input bus 0 of the vocoder node. */
    waveformConfig = ma_waveform_config_init(device.capture.format, device.capture.channels, device.sampleRate, ma_waveform_type_sawtooth, 1.0, 50);

    result = ma_waveform_init(&waveformConfig, &g_sourceData);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize waveform for excite node.");
        goto done3;
    }

    sourceNodeConfig = ma_data_source_node_config_init(&g_sourceData);

    result = ma_data_source_node_init(&g_nodeGraph, &sourceNodeConfig, NULL, &g_sourceNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize excite node.");
        goto done3;
    }

    ma_node_attach_output_bus(&g_sourceNode, 0, &g_vocoderNode, 0);


    /* Excite/modulator. Attached to input bus 1 of the vocoder node. */
    result = ma_audio_buffer_ref_init(device.capture.format, device.capture.channels, NULL, 0, &g_exciteData);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio buffer for source.");
        goto done2;
    }

    exciteNodeConfig = ma_data_source_node_config_init(&g_exciteData);

    result = ma_data_source_node_init(&g_nodeGraph, &exciteNodeConfig, NULL, &g_exciteNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize source node.");
        goto done2;
    }

    ma_node_attach_output_bus(&g_exciteNode, 0, &g_vocoderNode, 1);


    ma_device_start(&device);

    printf("Press Enter to quit...\n");
    getchar();

    /* It's important that we stop the device first or else we'll uninitialize the graph from under the device. */
    ma_device_stop(&device);

/*done4:*/ ma_data_source_node_uninit(&g_exciteNode, NULL);
done3: ma_data_source_node_uninit(&g_sourceNode, NULL);
done2: ma_vocoder_node_uninit(&g_vocoderNode, NULL);
done1: ma_node_graph_uninit(&g_nodeGraph, NULL);
done0: ma_device_uninit(&device);

    (void)argc;
    (void)argv;
    return 0;
}
