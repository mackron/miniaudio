#include <stdio.h>

#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

void log_callback(mal_context* pContext, mal_device* pDevice, mal_uint32 logLevel, const char* message)
{
    (void)pContext;
    (void)pDevice;
    (void)logLevel;
    printf("%s\n", message);
}

void stop_callback(mal_device* pDevice)
{
    (void)pDevice;
    printf("STOPPED\n");
}

void data_callback(mal_device* pDevice, void* pOutput, const void* pInput, mal_uint32 frameCount)
{
    /* In this test the format and channel count are the same for both input and output which means we can just memcpy(). */
    mal_copy_memory(pOutput, pInput, frameCount * mal_get_bytes_per_frame(pDevice->format, pDevice->channels));
}

int main(int argc, char** argv)
{
    mal_result result;

    (void)argc;
    (void)argv;

    mal_backend backend = mal_backend_wasapi;

    mal_context_config contextConfig = mal_context_config_init(log_callback);
    mal_context context;
    result = mal_context_init(&backend, 1, &contextConfig, &context);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize context.\n");
        return result;
    }

    mal_device_config deviceConfig = mal_device_config_init(mal_device_type_duplex);
    deviceConfig.pDeviceID = NULL;
    deviceConfig.format = mal_format_f32;
    deviceConfig.channels = 2;
    deviceConfig.sampleRate = 44100;
    deviceConfig.bufferSizeInMilliseconds = 50;
    deviceConfig.periods = 3;
    deviceConfig.shareMode = mal_share_mode_shared;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.stopCallback = stop_callback;
    deviceConfig.pUserData = NULL;

    mal_device device;
    result = mal_device_init(&context, &deviceConfig, &device);
    if (result != MAL_SUCCESS) {
        return result;
    }

    mal_device_start(&device);

    printf("Press Enter to quit...\n");
    getchar();

    mal_device_uninit(&device);
    return 0;
}