/* Uncomment this and update the path to enable the debug screen. */
/*#include "/opt/toolchains/vitasdk/samples/common/debugScreen.c"*/

#if defined(DEBUG_SCREEN_H)
#define HAS_DEBUG_SCREEN
#endif

#ifdef HAS_DEBUG_SCREEN
#define printf psvDebugScreenPrintf
#endif

#define MA_DEBUG_OUTPUT
#include "../../miniaudio.c"

#include <psp2/ctrl.h>

static void data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    static int c = 0;

    ma_data_source* pDataSource = (ma_data_source*)ma_device_get_user_data(pDevice);
    ma_data_source_read_pcm_frames(pDataSource, pFramesOut, frameCount, NULL);

    (void)pFramesIn;
    (void)pDevice;
}

int main()
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform waveform;
    ma_decoder decoder;
    SceCtrlData ctrlPeek, ctrlPress;

    #ifdef HAS_DEBUG_SCREEN
    psvDebugScreenInit();
    #endif

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    //deviceConfig.threadingMode      = MA_THREADING_MODE_SINGLE_THREADED;
    deviceConfig.playback.format    = ma_format_s16;
    deviceConfig.playback.channels  = 2;
    deviceConfig.sampleRate         = 48000;
    deviceConfig.dataCallback       = data_callback;
    deviceConfig.pUserData          = &waveform;
    deviceConfig.periodSizeInFrames = 1024;

    #if 0
    ma_device_backend_config backend = ma_device_backend_config_init(ma_device_backend_vita, NULL);
    result = ma_device_init_ex(&backend, 1, NULL, &deviceConfig, &device);
    #else
    result = ma_device_init(NULL, &deviceConfig, &device);
    #endif
    if (result != MA_SUCCESS) {
        return 1;
    }

    /* Initialize the waveform before starting the device. */
    {
        ma_waveform_config waveformConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.1f, 400);
        ma_waveform_init(&waveformConfig, &waveform);
    }

    /* Decoder. */
    #if 0
    {
        ma_decoder_config decoderConfig = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);
        ma_decoder_init_file("test.mp3", &decoderConfig, &decoder);
    }
    #endif

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        return 1;
    }

    for (;;) {
        ctrlPress = ctrlPeek;
		sceCtrlPeekBufferPositive(0, &ctrlPeek, 1);
		ctrlPress.buttons = ctrlPeek.buttons & ~ctrlPress.buttons;

        if(ctrlPress.buttons == SCE_CTRL_SQUARE) {
            if (ma_device_is_started(&device)) {
                printf("STOPPING\n");
                ma_device_stop(&device);
            } else {
                printf("STARTING\n");
                ma_device_start(&device);
            }
        }

        if(ctrlPress.buttons == SCE_CTRL_CROSS) {
            break;
        }

        if (ma_device_get_threading_mode(&device) == MA_THREADING_MODE_SINGLE_THREADED) {
            ma_device_step(&device, MA_BLOCKING_MODE_NON_BLOCKING);

            /*
            I found that if I don't relax the CPU a bit I'll get glitching when running in vita3k. Maybe the loop is pinning the CPU
            at 100% and starving the audio system?
            */
            ma_sleep(1);
        } else {
            ma_sleep(1);
        }
    }

    ma_waveform_uninit(&waveform);
    ma_device_uninit(&device);
    return 0;
}
