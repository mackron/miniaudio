/*
This example shows how to use the node graph system.

The node graph system can be used for doing complex mixing and effect processing. The idea is that
you have a number of nodes that are connected to each other to form a graph. At the end of the
graph is an endpoint which all nodes eventually connect to.

A node is used to do some kind of processing on zero or more input streams and produce one or more
output streams. Each node can have a number of inputs and outputs. Each of these is called a bus in
miniaudio. Some nodes, particularly data source nodes, have no inputs and instead generate their
outputs dynamically. All nodes will have at least one output or else it'll be disconnected from the
graph and will never get processed. Each output bus of a node will be connected to an input bus of
another node, but they don't all need to connect to the same input node. For example, a splitter
node has 1 input bus and 2 output buses and is used to duplicate a signal. You could then branch
off and have one output bus connected to one input node and the other connected to a different
input node, and then have two different effects process for each of the duplicated branches.

Any number of output buses can be connected to an input bus in which case the output buses will be
mixed before processing by the input node. This is how you would achieve the mixing part of the
node graph.

This example will be using the following node graph set up:

    ```
    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Data flows left to right >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    +---------------+                              +-----------------+
    | Data Source 1 =----+    +----------+    +----= Low Pass Filter =----+
    +---------------+    |    |          =----+    +-----------------+    |    +----------+
                         +----= Splitter |                                +----= ENDPOINT |
    +---------------+    |    |          =----+    +-----------------+    |    +----------+
    | Data Source 2 =----+    +----------+    +----=  Echo / Delay   =----+
    +---------------+                              +-----------------+
    ```

This does not represent a realistic real-world scenario, but it demonstrates how to make use of
mixing, multiple outputs and multiple effects.

The data source nodes are connected to the input of the splitter. They'll be mixed before being
processed by the splitter. The splitter has two output buses. In the graph above, one bus will be
routed to a low pass filter, whereas the other bus will be routed to an echo effect. Then, the
outputs of these two effects will be connected to the input bus of the endpoint. Because both of
the outputs are connected to the same input bus, they'll be mixed at that point.

The two data sources at the start of the graph have no inputs. They'll instead generate their
output by reading from a data source. The data source in this case will be one `ma_decoder` for
each input file specified on the command line.

You can also control the volume of an output bus. In this example, we set the volumes of the low
pass and echo effects so that one of them becomes more obvious than the other.

When you want to read from the graph, you simply call `ma_node_graph_read_pcm_frames()`.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

/* Data Format */
#define FORMAT              ma_format_f32   /* Must always be f32. */
#define CHANNELS            2
#define SAMPLE_RATE         48000

/* Effect Properties */
#define LPF_BIAS            0.9f    /* Higher values means more bias towards the low pass filter (the low pass filter will be more audible). Lower values means more bias towards the echo. Must be between 0 and 1. */
#define LPF_CUTOFF_FACTOR   80      /* High values = more filter. */
#define LPF_ORDER           8
#define DELAY_IN_SECONDS    0.2f
#define DECAY               0.5f    /* Volume falloff for each echo. */

typedef struct
{
    ma_data_source_node node;   /* If you make this the first member, you can pass a pointer to this struct into any `ma_node_*` API and it will "Just Work". */
    ma_decoder decoder;
} sound_node;

static ma_node_graph    g_nodeGraph;
static ma_lpf_node      g_lpfNode;
static ma_delay_node    g_delayNode;
static ma_splitter_node g_splitterNode;
static sound_node*      g_pSoundNodes;
static int              g_soundNodeCount;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MA_ASSERT(pDevice->playback.channels == CHANNELS);

    /*
    Hearing the output of the node graph is as easy as reading straight into the output buffer. You just need to
    make sure you use a consistent data format or else you'll need to do your own conversion.
    */
    ma_node_graph_read_pcm_frames(&g_nodeGraph, pOutput, frameCount, NULL);

    (void)pInput;   /* Unused. */
}

int main(int argc, char** argv)
{
    int iarg;
    ma_result result;

    /* We'll set up our nodes starting from the end and working our way back to the start. We'll need to set up the graph first. */
    {
        ma_node_graph_config nodeGraphConfig = ma_node_graph_config_init(CHANNELS);

        result = ma_node_graph_init(&nodeGraphConfig, NULL, &g_nodeGraph);
        if (result != MA_SUCCESS) {
            printf("ERROR: Failed to initialize node graph.");
            return -1;
        }
    }


    /* Low Pass Filter. */
    {
        ma_lpf_node_config lpfNodeConfig = ma_lpf_node_config_init(CHANNELS, SAMPLE_RATE, SAMPLE_RATE / LPF_CUTOFF_FACTOR, LPF_ORDER);

        result = ma_lpf_node_init(&g_nodeGraph, &lpfNodeConfig, NULL, &g_lpfNode);
        if (result != MA_SUCCESS) {
            printf("ERROR: Failed to initialize low pass filter node.");
            return -1;
        }

        /* Connect the output bus of the low pass filter node to the input bus of the endpoint. */
        ma_node_attach_output_bus(&g_lpfNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);

        /* Set the volume of the low pass filter to make it more of less impactful. */
        ma_node_set_output_bus_volume(&g_lpfNode, 0, LPF_BIAS);
    }


    /* Echo / Delay. */
    {
        ma_delay_node_config delayNodeConfig = ma_delay_node_config_init(CHANNELS, SAMPLE_RATE, (ma_uint32)(SAMPLE_RATE * DELAY_IN_SECONDS), DECAY);

        result = ma_delay_node_init(&g_nodeGraph, &delayNodeConfig, NULL, &g_delayNode);
        if (result != MA_SUCCESS) {
            printf("ERROR: Failed to initialize delay node.");
            return -1;
        }

        /* Connect the output bus of the delay node to the input bus of the endpoint. */
        ma_node_attach_output_bus(&g_delayNode, 0, ma_node_graph_get_endpoint(&g_nodeGraph), 0);

        /* Set the volume of the delay filter to make it more of less impactful. */
        ma_node_set_output_bus_volume(&g_delayNode, 0, 1 - LPF_BIAS);
    }


    /* Splitter. */
    {
        ma_splitter_node_config splitterNodeConfig = ma_splitter_node_config_init(CHANNELS);

        result = ma_splitter_node_init(&g_nodeGraph, &splitterNodeConfig, NULL, &g_splitterNode);
        if (result != MA_SUCCESS) {
            printf("ERROR: Failed to initialize splitter node.");
            return -1;
        }

        /* Connect output bus 0 to the input bus of the low pass filter node, and output bus 1 to the input bus of the delay node. */
        ma_node_attach_output_bus(&g_splitterNode, 0, &g_lpfNode,   0);
        ma_node_attach_output_bus(&g_splitterNode, 1, &g_delayNode, 0);
    }


    /* Data sources. Ignore any that cannot be loaded. */
    g_pSoundNodes = (sound_node*)ma_malloc(sizeof(*g_pSoundNodes) * argc-1, NULL);
    if (g_pSoundNodes == NULL) {
        printf("Failed to allocate memory for sounds.");
        return -1;
    }

    g_soundNodeCount = 0;
    for (iarg = 1; iarg < argc; iarg += 1) {
        ma_decoder_config decoderConfig = ma_decoder_config_init(FORMAT, CHANNELS, SAMPLE_RATE);

        result = ma_decoder_init_file(argv[iarg], &decoderConfig, &g_pSoundNodes[g_soundNodeCount].decoder);
        if (result == MA_SUCCESS) {
            ma_data_source_node_config dataSourceNodeConfig = ma_data_source_node_config_init(&g_pSoundNodes[g_soundNodeCount].decoder);

            result = ma_data_source_node_init(&g_nodeGraph, &dataSourceNodeConfig, NULL, &g_pSoundNodes[g_soundNodeCount].node);
            if (result == MA_SUCCESS) {
                /* The data source node has been created successfully. Attach it to the splitter. */
                ma_node_attach_output_bus(&g_pSoundNodes[g_soundNodeCount].node, 0, &g_splitterNode, 0);
                g_soundNodeCount += 1;
            } else {
                printf("WARNING: Failed to init data source node for sound \"%s\". Ignoring.", argv[iarg]);
                ma_decoder_uninit(&g_pSoundNodes[g_soundNodeCount].decoder);
            }
        } else {
            printf("WARNING: Failed to load sound \"%s\". Ignoring.", argv[iarg]);
        }
    }

    /* Everything has been initialized successfully so now we can set up a playback device so we can listen to the result. */
    {
        ma_device_config deviceConfig;
        ma_device device;

        deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format   = FORMAT;
        deviceConfig.playback.channels = CHANNELS;
        deviceConfig.sampleRate        = SAMPLE_RATE;
        deviceConfig.dataCallback      = data_callback;
        deviceConfig.pUserData         = NULL;

        result = ma_device_init(NULL, &deviceConfig, &device);
        if (result != MA_SUCCESS) {
            printf("ERROR: Failed to initialize device.");
            goto cleanup_graph;
        }

        result = ma_device_start(&device);
        if (result != MA_SUCCESS) {
            ma_device_uninit(&device);
            goto cleanup_graph;
        }

        printf("Press Enter to quit...\n");
        getchar();

        /* We're done. Clean up the device. */
        ma_device_uninit(&device);
    }


cleanup_graph:
    {
        /* It's good practice to tear down the graph from the lowest level nodes first. */
        int iSound;

        /* Sounds. */
        for (iSound = 0; iSound < g_soundNodeCount; iSound += 1) {
            ma_data_source_node_uninit(&g_pSoundNodes[iSound].node, NULL);
            ma_decoder_uninit(&g_pSoundNodes[iSound].decoder);
        }

        /* Splitter. */
        ma_splitter_node_uninit(&g_splitterNode, NULL);
        
        /* Echo / Delay */
        ma_delay_node_uninit(&g_delayNode, NULL);

        /* Low Pass Filter */
        ma_lpf_node_uninit(&g_lpfNode, NULL);

        /* Node Graph */
        ma_node_graph_uninit(&g_nodeGraph, NULL);
    }    

    return 0;
}
