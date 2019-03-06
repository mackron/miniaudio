#include <stdio.h>

#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

mal_sine_wave sineWave;
mal_uint32 framesWritten;
mal_event stopEvent;
mal_bool32 isInitialRun = MAL_TRUE;

void on_stop(mal_device* pDevice)
{
    (void)pDevice;
    printf("STOPPED\n");
}

void on_data(mal_device* pDevice, void* pOutput, const void* pInput, mal_uint32 frameCount)
{
    (void)pInput;   /* Not used yet. */

    /* Output exactly one second of data. Pad the end with silence. */
    mal_uint32 framesRemaining = pDevice->sampleRate - framesWritten;
    mal_uint32 framesToProcess = frameCount;
    if (framesToProcess > framesRemaining && isInitialRun) {
        framesToProcess = framesRemaining;
    }

    mal_sine_wave_read_f32_ex(&sineWave, framesToProcess, pDevice->playback.channels, mal_stream_layout_interleaved, (float**)&pOutput);
    if (isInitialRun) {
        framesWritten += framesToProcess;
    }

    mal_assert(framesWritten <= pDevice->sampleRate);
    if (framesWritten >= pDevice->sampleRate) {
        if (isInitialRun) {
            printf("STOPPING [AUDIO THREAD]...\n");
            mal_event_signal(&stopEvent);
            isInitialRun = MAL_FALSE;
        }
    }
}

int main(int argc, char** argv)
{
    mal_result result;

    (void)argc;
    (void)argv;

    mal_backend backend = mal_backend_wasapi;

    mal_sine_wave_init(0.25, 400, 44100, &sineWave);

    mal_device_config config = mal_device_config_init(mal_device_type_playback);
    config.playback.format = mal_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 44100;
    config.dataCallback = on_data;
    config.stopCallback = on_stop;
    config.bufferSizeInFrames = 16384;

    mal_device device;
    result = mal_device_init_ex(&backend, 1, NULL, &config, &device);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize device.\n");
        return result;
    }

    result = mal_event_init(device.pContext, &stopEvent);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize stop event.\n");
        return result;
    }

    mal_device_start(&device);

    /* We wait for the stop event, stop the device, then ask the user to press any key to restart. This checks that the device can restart after stopping. */
    mal_event_wait(&stopEvent);

    printf("STOPPING [MAIN THREAD]...\n");
    mal_device_stop(&device);

    printf("Press Enter to restart...\n");
    getchar();

    result = mal_device_start(&device);
    if (result != MAL_SUCCESS) {
        printf("Failed to restart the device.\n");
        mal_device_uninit(&device);
        return -1;
    }

    printf("Press Enter to quit...\n");
    getchar();

    mal_device_uninit(&device);
    return 0;
}