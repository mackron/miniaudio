#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"
#include "miniaudio_engine.h"

ma_node_graph g_nodeGraph;
ma_data_source_node g_dataSourceNode;
ma_splitter_node g_splitterNode;
ma_splitter_node g_loopNode;    /* For testing loop detection. We're going to route one of these endpoints back to g_splitterNode to form a loop. */

void data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    /* Read straight from our node graph. */
    ma_node_graph_read_pcm_frames(&g_nodeGraph, pFramesOut, frameCount, NULL);

    (void)pDevice;      /* Unused. */
    (void)pFramesIn;    /* Unused. */
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    ma_node_graph_config nodeGraphConfig;
    ma_data_source_node_config dataSourceNodeConfig;
    ma_splitter_node_config splitterNodeConfig;

    if (argc <= 1) {
        printf("No input file.");
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_f32; /* The node graph API only supports f32. */
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate        = 48000;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = NULL;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize device.");
        return -1;
    }


    /*
    Set up the new graph before starting the device so that we have something to read from as soon
    as the device requests data. It doesn't matter what order we do this, but I'm starting with the
    data source node since it makes more logical sense to me to start with the start of the chain.
    */
    nodeGraphConfig = ma_node_graph_config_init(device.playback.channels);

    result = ma_node_graph_init(&nodeGraphConfig, NULL, &g_nodeGraph);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize node graph.");
        return -1;
    }


    /*
    We want the decoder to use the same format as the device. This way we can keep the entire node
    graph using the same format/channels/rate to avoid the need to do data conversion.
    */
    decoderConfig = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);

    result = ma_decoder_init_file(argv[1], &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.");
        ma_device_uninit(&device);
        return -1;
    }

    dataSourceNodeConfig = ma_data_source_node_config_init(&decoder, MA_TRUE);

    result = ma_data_source_node_init(&g_nodeGraph, &dataSourceNodeConfig, NULL, &g_dataSourceNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize data source node.");
        ma_decoder_uninit(&decoder);
        ma_device_uninit(&device);
        return -1;
    }

    /*result = ma_node_attach_output_bus(&g_dataSourceNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);
    if (result != MA_SUCCESS) {
        printf("Failed to attach node.");
        return -1;
    }*/

    /*ma_node_set_state_time(&g_dataSourceNode, ma_node_state_started, 48000*1);
    ma_node_set_state_time(&g_dataSourceNode, ma_node_state_stopped, 48000*5);*/


#if 1
    /*
    Splitter node. Note that we've already attached the data source node to another, so this section
    will test that changing of attachments works as expected.
    */
    splitterNodeConfig = ma_splitter_node_config_init(device.playback.channels);



    /* Loop detection testing. */
    result = ma_splitter_node_init(&g_nodeGraph, &splitterNodeConfig, NULL, &g_loopNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize loop node.");
        return -1;
    }

    /* Adjust the volume of the splitter node's endpoints. We'll just do it 50/50 so that both of them combine to reproduce the original signal at the endpoint. */
    ma_node_set_output_bus_volume(&g_loopNode, 0, 1.0f);
    ma_node_set_output_bus_volume(&g_loopNode, 1, 1.0f);


    

    result = ma_splitter_node_init(&g_nodeGraph, &splitterNodeConfig, NULL, &g_splitterNode);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize splitter node.");
        return -1;
    }


#if 0
    /* Connect both outputs of the splitter to the endpoint for now. Later on we'll test effects and whatnot. */
    ma_node_attach_output_bus(&g_splitterNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);
    ma_node_attach_output_bus(&g_splitterNode, 1, ma_node_graph_get_endpoint(&g_nodeGraph), 0);

    /* Adjust the volume of the splitter node's endpoints. We'll just do it 50/50 so that both of them combine to reproduce the original signal at the endpoint. */
    ma_node_set_output_bus_volume(&g_splitterNode, 0, 0.5f);
    ma_node_set_output_bus_volume(&g_splitterNode, 1, 0.5f);

    /* The data source needs to have it's connection changed from the endpoint to the splitter. */
    ma_node_attach_output_bus(&g_dataSourceNode, 0, &g_splitterNode, 0);
#else
    /* Connect the loop node directly to the output. */
    ma_node_attach_output_bus(&g_loopNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);
    ma_node_attach_output_bus(&g_loopNode, 1, ma_node_graph_get_endpoint(&g_nodeGraph), 0);

    /* Connect the splitter node directly to the loop node. */
    ma_node_attach_output_bus(&g_splitterNode, 0, &g_loopNode, 0);
    ma_node_attach_output_bus(&g_splitterNode, 1, &g_loopNode, 1);

    /* Connect the data source node to the splitter node. */
    ma_node_attach_output_bus(&g_dataSourceNode, 0, &g_splitterNode, 0);

    /* Now loop back to the splitter node to form a loop. */
    ma_node_attach_output_bus(&g_loopNode, 1, &g_splitterNode, 0);
#endif

    
#endif

    


    /* Stop the splitter node for testing. */
    /*ma_node_set_state(&g_splitterNode, ma_node_state_stopped);*/


    /*
    Only start the device after our nodes have been set up. We passed in `deviceNode` as the user
    data to the data callback so we need to make sure it's initialized before we start the device.
    */
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        return -1;
    }

    printf("Press Enter to quit...");
    getchar();


    /* Teardown. These are uninitialized in a weird order here just for demonstration. */

    /* We should be able to safely destroy the node while the device is still running. */
    ma_data_source_node_uninit(&g_dataSourceNode, NULL);
    
    /* The device needs to be stopped before we uninitialize the node graph or else the device's callback will try referencing the node graph. */
    ma_device_uninit(&device);
    
    /* The node graph will be referenced by the device's data called so it needs to be uninitialized after the device has stopped. */
    ma_node_graph_uninit(&g_nodeGraph, NULL);

    return 0;
}
