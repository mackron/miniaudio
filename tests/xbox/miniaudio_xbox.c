#include "../../miniaudio.c"

#include <hal/debug.h>
#include <hal/video.h>

static void data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
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

    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    /*deviceConfig.threadingMode      = MA_THREADING_MODE_SINGLE_THREADED;*/
    deviceConfig.playback.format    = ma_format_s16;
    deviceConfig.playback.channels  = 2;
    deviceConfig.sampleRate         = 0;
    deviceConfig.dataCallback       = data_callback;
    deviceConfig.pUserData          = &decoder;
    deviceConfig.periodSizeInFrames = 1024;
    
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        debugPrint("Failed to initialize device.\n");
        return 1;
    }

    /* Initialize the waveform before starting the device. */
    {
        ma_waveform_config waveformConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.1f, 400);
        ma_waveform_init(&waveformConfig, &waveform);
    }

    /* Decoder. */
    {
        ma_decoder_config decoderConfig = ma_decoder_config_init(device.playback.format, device.playback.channels, device.sampleRate);
        ma_decoder_init_file("D:\\test.mp3", &decoderConfig, &decoder);
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        debugPrint("Failed to start device.\n");
        return 1;
    }

    for (;;) {
        /*debugPrint("Looping...\n");*/
        if (ma_device_get_threading_mode(&device) == MA_THREADING_MODE_SINGLE_THREADED) {
            ma_device_step(&device, MA_BLOCKING_MODE_NON_BLOCKING);
        } else {
            ma_sleep(500);
        }
    }

    ma_waveform_uninit(&waveform);
    ma_device_uninit(&device);
    return 0;
}
