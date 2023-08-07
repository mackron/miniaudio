#define MA_DEBUG_OUTPUT
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"

#include <stdio.h>

#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000

ma_bool32 isRunning = MA_FALSE;
ma_device device;
ma_waveform sineWave;   /* For playback example. */


void main_loop__em()
{
}



void data_callback_playback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);

    ma_waveform_read_pcm_frames(&sineWave, pOutput, frameCount, NULL);

    (void)pInput;   /* Unused. */
}

static void do_playback()
{
    ma_device_config deviceConfig;
    ma_waveform_config sineWaveConfig;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback_playback;
    deviceConfig.pUserData         = &sineWave;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return;
    }

    sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.2, 220);
    ma_waveform_init(&sineWaveConfig, &sineWave);

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start device.");
        return;
    }
}



void data_callback_duplex(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MA_ASSERT(pDevice->capture.format == pDevice->playback.format);
    MA_ASSERT(pDevice->capture.channels == pDevice->playback.channels);

    /* In this example the format and channel count are the same for both input and output which means we can just memcpy(). */
    MA_COPY_MEMORY(pOutput, pInput, frameCount * ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels));
}

static void do_duplex()
{
    ma_result result;
    ma_device_config deviceConfig;

    deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.pDeviceID  = NULL;
    deviceConfig.capture.format     = DEVICE_FORMAT;
    deviceConfig.capture.channels   = 2;
    deviceConfig.capture.shareMode  = ma_share_mode_shared;
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format    = DEVICE_FORMAT;
    deviceConfig.playback.channels  = 2;
    deviceConfig.sampleRate         = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback       = data_callback_duplex;
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        return;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start device.");
        return;
    }
}


static EM_BOOL on_canvas_click(int eventType, const EmscriptenMouseEvent* pMouseEvent, void* pUserData)
{
    if (isRunning == MA_FALSE) {
        if (pMouseEvent->button == 0) { /* Left click. */
            do_playback();
        } else if (pMouseEvent->button == 2) { /* Right click. */
            do_duplex();
        }

        isRunning = MA_TRUE;
    } else {
        if (ma_device_get_state(&device) == ma_device_state_started) {
            ma_device_stop(&device);
        } else {
            ma_device_start(&device);
        }
    }

    (void)eventType;
    (void)pUserData;

    return EM_FALSE;
}




int main(int argc, char** argv)
{
    printf("Click inside canvas to start playing:\n");
    printf("    Left click for playback\n");
    printf("    Right click for duplex\n");

    /* The device must be started in response to an input event. */
    emscripten_set_mouseup_callback("canvas", &device, 0, on_canvas_click);
    
    emscripten_set_main_loop(main_loop__em, 0, 1);
    
    (void)argc;
    (void)argv;
    return 0;
}
