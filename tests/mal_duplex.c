#include <stdio.h>

#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

void on_stop(mal_device* pDevice)
{
    (void)pDevice;
    printf("STOPPED\n");
}

void data_callback(mal_device* pDevice, void* pOutput, const void* pInput, mal_uint32 frameCount)
{
    /* In our this test the format and channel count is the same for both input and output which means we can just memcpy(). */
    mal_copy_memory(pOutput, pInput, frameCount * mal_get_bytes_per_frame(pDevice->format, pDevice->channels));
}

int main(int argc, char** argv)
{
    mal_result result;

    (void)argc;
    (void)argv;

    mal_backend backend = mal_backend_wasapi;

    mal_device_config config = mal_device_config_init_default(data_callback, NULL);
    config.format = mal_format_f32;
    config.channels = 2;
    config.sampleRate = 44100;
    config.onStopCallback = on_stop;
    config.bufferSizeInFrames = 16384*4;

    mal_device device;
    result = mal_device_init_ex(&backend, 1, NULL, mal_device_type_playback, NULL, &config, &device);
    if (result != MAL_SUCCESS) {
        return result;
    }

    mal_device_start(&device);

    printf("Press Enter to quit...\n");
    getchar();

    mal_device_uninit(&device);
    return 0;
}