// This test just plays a constant sine wave tone. Mainly intended to check how physically
// unplugging a device while it's playing works.
//#define MAL_DEBUG_OUTPUT
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

    //printf("TESTING: %d\n", frameCount);
    return (mal_uint32)mal_sine_wave_read_ex(&g_sineWave, frameCount, pDevice->channels, mal_stream_layout_interleaved, (float**)&pFramesOut);
}

int main()
{
    mal_backend backend = mal_backend_wasapi;

    mal_result result = mal_sine_wave_init(0.25f, 400, 48000, &g_sineWave);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize sine wave.\n");
        return -1;
    }

    mal_context_config contextConfig = mal_context_config_init(on_log);
    mal_device_config deviceConfig = mal_device_config_init_playback(mal_format_f32, 0, 48000, on_send);
    deviceConfig.onStopCallback = on_stop;
    //deviceConfig.shareMode = mal_share_mode_exclusive;

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

    printf("Press Enter to quit...\n");
    getchar();

    mal_device_uninit(&device);
    return 0;
}