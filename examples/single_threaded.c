/*
Demonstrates the use of single-threaded mode.

By default, miniaudio runs in multi-threaded mode where audio processing is done on a separate
thread that's managed internally by miniaudio. Sometimes this is undesireable, such as when trying
to get miniaudio working on low-end systems where an extra thread is too costly, or when trying to
get it working on extremely old platforms that don't support threading at all, or simply when you
want to have more control over threading in your application.

To enable single-threaded mode, set the `threadingMode` member of the `ma_device_config` structure
to `MA_THREADING_MODE_SINGLE_THREADED`. To process audio, you need to regularly call
`ma_device_step()`, usually from your main application loop. It is from this function that the
data callback will get fired. You should only call `ma_device_step()` when the device is started,
so typically you would do this between your `ma_device_start()` and `ma_device_stop()` calls.

The `ma_device_step()` function lets you control whether or not it should block while waiting for
audio to be processed. This is controlled via the `blockingMode` parameter. You would typically
use `MA_BLOCKING_MODE_BLOCKING` if you want to relax the CPU. For a game you would probably want to
use `MA_BLOCKING_MODE_NON_BLOCKING`.

You should only ever call `ma_device_step()` in single-threaded mode. In multi-threaded mode (the
default), you should never call this function manually. You can query whether or not the device is
in single-threaded mode via `ma_device_get_threading_mode()`.
*/
#include "../miniaudio.c"

#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

void main_loop__em(void* pUserData)
{
    ma_device* pDevice = (ma_device*)pUserData;
    ma_device_step(pDevice, MA_BLOCKING_MODE_NON_BLOCKING);
}
#endif

#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_waveform_read_pcm_frames((ma_waveform*)pDevice->pUserData, pOutput, frameCount, NULL);

    (void)pInput;   /* Unused. */
}

int main(int argc, char** argv)
{
    ma_waveform sineWave;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform_config sineWaveConfig;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.threadingMode     = MA_THREADING_MODE_SINGLE_THREADED; /* <-- This is what enables single-threaded mode. */
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &sineWave;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.2, 220);
    ma_waveform_init(&sineWaveConfig, &sineWave);

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return -5;
    }

    printf("Running in single-threaded mode. Press Ctrl+C to quit.\n");
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop__em, &device, 0, 1);
#else
    /*
    We're putting this in an infinite loop for the sake of this example, but in a real application you
    would probably integrate this into your normal application loop.

    Using blocking mode makes it so the CPU is relaxed. For a game you would probably want to use
    non-blocking mode which you can do with `MA_BLOCKING_MODE_NON_BLOCKING`.

    If the device is stopped, `ma_device_step()` will return `MA_DEVICE_NOT_STARTED` which means you
    need not explicitly check if the device is started before calling this function.
    */
    for (;;) {
        ma_result result = ma_device_step(&device, MA_BLOCKING_MODE_BLOCKING);
        if (result != MA_SUCCESS) {
            break;
        }
    }
#endif
    
    ma_device_uninit(&device);
    ma_waveform_uninit(&sineWave);  /* Uninitialize the waveform after the device so we don't pull it from under the device while it's being reference in the data callback. */
    
    (void)argc;
    (void)argv;
    return 0;
}
