/*
Demonstrates how to apply an effect to sounds using the high level engine API.

This example will load a file from the command line and apply an echo/delay effect to it. It will
show you how to manage `ma_sound` objects and how to insert an effect into the graph.

The `ma_engine` object is a node graph and is compatible with the `ma_node_graph` API. The
`ma_sound` object is a node within the node and is compatible with the `ma_node` API. This means
that applying an effect is as simple as inserting an effect node into the graph and plugging in the
sound's output into the effect's input. See the Node Graph example for how to use the node graph.

This example is playing only a single sound at a time which means only a single `ma_sound` object
it being used. If you want to play multiple sounds at the same time, even if they're for the same
sound file, you need multiple `ma_sound` objects.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#define DELAY_IN_SECONDS    0.2f
#define DECAY               0.25f   /* Volume falloff for each echo. */

static ma_engine g_engine;
static ma_sound g_sound;            /* This example will play only a single sound at once, so we only need one `ma_sound` object. */
static ma_delay_node g_delayNode;   /* The echo effect is achieved using a delay node. */

int main(int argc, char** argv)
{
    ma_result result;

    if (argc < 2) {
        printf("No input file.");
        return -1;
    }

    /* The engine needs to be initialized first. */
    result = ma_engine_init(NULL, &g_engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio engine.");
        return -1;
    }


    /*
    We'll build our graph starting from the end so initialize the delay node now. The output of
    this node will be connected straight to the output. You could also attach it to a sound group
    or any other node that accepts an input.

    Creating a node requires a pointer to the node graph that owns it. The engine itself is a node
    graph. In the code below we can get a pointer to the node graph with `ma_engine_get_node_graph()`
    or we could simple cast the engine to a ma_node_graph* like so:
    
        (ma_node_graph*)&g_engine

    The endpoint of the graph can be retrieved with `ma_engine_get_endpoint()`.
    */
    {
        ma_delay_node_config delayNodeConfig;
        ma_uint32 channels;
        ma_uint32 sampleRate;

        channels   = ma_engine_get_channels(&g_engine);
        sampleRate = ma_engine_get_sample_rate(&g_engine);

        delayNodeConfig = ma_delay_node_config_init(channels, sampleRate, (ma_uint32)(sampleRate * DELAY_IN_SECONDS), DECAY);

        result = ma_delay_node_init(ma_engine_get_node_graph(&g_engine), &delayNodeConfig, NULL, &g_delayNode);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize delay node.");
            return -1;
        }

        /* Connect the output of the delay node to the input of the endpoint. */
        ma_node_attach_output_bus(&g_delayNode, 0, ma_engine_get_endpoint(&g_engine), 0);
    }


    /* Now we can load the sound and connect it to the delay node. */
    {
        result = ma_sound_init_from_file(&g_engine, argv[1], 0, NULL, NULL, &g_sound);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize sound \"%s\".", argv[1]);
            return -1;
        }

        /* Connect the output of the sound to the input of the effect. */
        ma_node_attach_output_bus(&g_sound, 0, &g_delayNode, 0);

        /*
        Start the sound after it's applied to the sound. Otherwise there could be a scenario where
        the very first part of it is read before the attachment to the effect is made.
        */
        ma_sound_start(&g_sound);
    }


    printf("Press Enter to quit...");
    getchar();

    ma_sound_uninit(&g_sound);
    ma_delay_node_uninit(&g_delayNode, NULL);
    ma_engine_uninit(&g_engine);

    return 0;
}