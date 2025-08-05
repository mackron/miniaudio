/*
Demonstrates how to implement a custom decoder and use it with the high level API.

This is the same as the custom_decoder example, only it's used with the high level engine API
rather than the low level decoding API. You can use this to add support for Opus to your games, for
example (via libopus).
*/
#include "../miniaudio.c"
#include "../extras/decoders/libvorbis/miniaudio_libvorbis.c"
#include "../extras/decoders/libopus/miniaudio_libopus.c"

#include <stdio.h>


int main(int argc, char** argv)
{
    ma_result result;
    ma_resource_manager_config resourceManagerConfig;
    ma_resource_manager resourceManager;
    ma_engine_config engineConfig;
    ma_engine engine;

    /*
    Add your custom backend vtables here. The order in the array defines the order of priority. The
    vtables will be passed in to the resource manager config.
    */
    ma_decoding_backend_vtable* pCustomBackendVTables[] =
    {
        ma_decoding_backend_libvorbis,
        ma_decoding_backend_libopus
    };


    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }


    /* Using custom decoding backends requires a resource manager. */
    resourceManagerConfig = ma_resource_manager_config_init();
    resourceManagerConfig.ppCustomDecodingBackendVTables = pCustomBackendVTables;
    resourceManagerConfig.customDecodingBackendCount     = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
    resourceManagerConfig.pCustomDecodingBackendUserData = NULL;  /* <-- This will be passed in to the pUserData parameter of each function in the decoding backend vtables. */

    result = ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize resource manager.");
        return -1;
    }


    /* Once we have a resource manager we can create the engine. */
    engineConfig = ma_engine_config_init();
    engineConfig.pResourceManager = &resourceManager;

    result = ma_engine_init(&engineConfig, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize engine.");
        return -1;
    }


    /* Now we can play our sound. */
    result = ma_engine_play_sound(&engine, argv[1], NULL);
    if (result != MA_SUCCESS) {
        printf("Failed to play sound.");
        return -1;
    }


    printf("Press Enter to quit...");
    getchar();

    ma_engine_uninit(&engine);
    ma_resource_manager_uninit(&resourceManager);

    return 0;
}
