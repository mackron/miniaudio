/*
Demonstrates how to capture data from a microphone using the low-level API.

This example simply captures data from your default microphone until you press Enter. The output is saved to the file
specified on the command line.

Capturing works in a very similar way to playback. The only difference is the direction of data movement. Instead of
the application sending data to the device, the device will send data to the application. This example just writes the
data received by the microphone straight to a WAV file.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdlib.h>
#include <stdio.h>

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_encoder* pEncoder = (ma_encoder*)pDevice->pUserData;
    MA_ASSERT(pEncoder != NULL);

    ma_encoder_write_pcm_frames(pEncoder, pInput, frameCount, NULL);

    (void)pOutput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    ma_device_config deviceConfig;
    ma_device device;

    if (argc < 2) {
        printf("No output file.\n");
        return -1;
    }

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 44100);

    if (ma_encoder_init_file(argv[1], &encoderConfig, &encoder) != MA_SUCCESS) {
        printf("Failed to initialize output file.\n");
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format   = encoder.config.format;
    deviceConfig.capture.channels = encoder.config.channels;
    deviceConfig.sampleRate       = encoder.config.sampleRate;
    deviceConfig.dataCallback     = data_callback;
    deviceConfig.pUserData        = &encoder;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize capture device.\n");
        return -2;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to start device.\n");
        return -3;
    }

    printf("Press Enter to stop recording...\n");
    getchar();
    
    ma_device_uninit(&device);
    ma_encoder_uninit(&encoder);

    return 0;
}
