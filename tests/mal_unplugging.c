// This test just plays a constant sine wave tone. Mainly intended to check how physically
// unplugging a device while it's playing works.
#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"
#include <stdio.h>

mal_sine_wave g_sineWave;
mal_event g_stopEvent;

void on_log(mal_context* pContext, mal_device* pDevice, const char* message)
{
    (void)pContext;
    (void)pDevice;
    printf("%s\n", message);
}

void on_stop(mal_device* pDevice)
{
    (void)pDevice;
    printf("STOPPED\n");
    mal_event_signal(&g_stopEvent);
}

mal_uint32 on_send(mal_device* pDevice, mal_uint32 frameCount, void* pFramesOut)
{
    mal_assert(pDevice != NULL);
    mal_assert(pDevice->channels == 1);
    (void)pDevice;

    //printf("TESTING: %d\n", frameCount);
    return (mal_uint32)mal_sine_wave_read(&g_sineWave, frameCount, (float*)pFramesOut);
}

int main()
{
    mal_backend backend = mal_backend_alsa;

    mal_result result = mal_sine_wave_init(0.25f, 400, 48000, &g_sineWave);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize sine wave.\n");
        return -1;
    }

    mal_context_config contextConfig = mal_context_config_init(on_log);
    mal_device_config deviceConfig = mal_device_config_init_playback(mal_format_f32, 1, 48000, on_send);
    deviceConfig.onStopCallback = on_stop;

    mal_device device;
    result = mal_device_init_ex(&backend, 1, &contextConfig, mal_device_type_playback, NULL, &deviceConfig, NULL, &device);
    if (result != MAL_SUCCESS) {
        return result;
    }

    mal_event_init(device.pContext, &g_stopEvent);

    result = mal_device_start(&device);
    if (result != MAL_SUCCESS) {
        mal_device_uninit(&device);
        return result;
    }


    printf("Unplug the device...\n");
    mal_event_wait(&g_stopEvent);

    printf("Plug in the device and hit Enter to attempt to restart the device...\n");
    getchar();

    // To restart the device, first try mal_device_start(). If it fails, re-initialize from the top.
    result = mal_device_start(&device);
    if (result != MAL_SUCCESS) {
        printf("Failed to restart. Attempting to reinitialize...\n");
        mal_device_uninit(&device);
        
        result = mal_device_init_ex(&backend, 1, &contextConfig, mal_device_type_playback, NULL, &deviceConfig, NULL, &device);
        if (result != MAL_SUCCESS) {
            printf("Failed to reinitialize.\n");
            return -1;
        }

        result = mal_device_start(&device);
        if (result != MAL_SUCCESS) {
            printf("Failed to start device.\n");
            mal_device_uninit(&device);
            return -1;
        }
    }

    printf("Press Enter to quit...");
    getchar();

    mal_device_uninit(&device);
    return 0;
}