#include <stdio.h>

#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

ma_sine_wave sineWave;
ma_uint32 framesWritten;
ma_event stopEvent;
ma_bool32 isInitialRun = MA_TRUE;

void on_stop(ma_device* pDevice)
{
    (void)pDevice;
    printf("STOPPED\n");
}

void on_data(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    (void)pInput;   /* Not used yet. */

    /* Output exactly one second of data. Pad the end with silence. */
    ma_uint32 framesRemaining = pDevice->sampleRate - framesWritten;
    ma_uint32 framesToProcess = frameCount;
    if (framesToProcess > framesRemaining && isInitialRun) {
        framesToProcess = framesRemaining;
    }

    ma_sine_wave_read_f32_ex(&sineWave, framesToProcess, pDevice->playback.channels, ma_stream_layout_interleaved, (float**)&pOutput);
    if (isInitialRun) {
        framesWritten += framesToProcess;
    }

    ma_assert(framesWritten <= pDevice->sampleRate);
    if (framesWritten >= pDevice->sampleRate) {
        if (isInitialRun) {
            printf("STOPPING [AUDIO THREAD]...\n");
            ma_event_signal(&stopEvent);
            isInitialRun = MA_FALSE;
        }
    }
}

int main(int argc, char** argv)
{
    ma_result result;

    (void)argc;
    (void)argv;

    ma_backend backend = ma_backend_wasapi;

    ma_sine_wave_init(0.25, 400, 44100, &sineWave);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 44100;
    config.dataCallback = on_data;
    config.stopCallback = on_stop;
    config.bufferSizeInFrames = 16384;

    ma_device device;
    result = ma_device_init_ex(&backend, 1, NULL, &config, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize device.\n");
        return result;
    }

    result = ma_event_init(device.pContext, &stopEvent);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize stop event.\n");
        return result;
    }

    ma_device_start(&device);

    /* We wait for the stop event, stop the device, then ask the user to press any key to restart. This checks that the device can restart after stopping. */
    ma_event_wait(&stopEvent);

    printf("STOPPING [MAIN THREAD]...\n");
    ma_device_stop(&device);

    printf("Press Enter to restart...\n");
    getchar();

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("Failed to restart the device.\n");
        ma_device_uninit(&device);
        return -1;
    }

    printf("Press Enter to quit...\n");
    getchar();

    ma_device_uninit(&device);
    return 0;
}