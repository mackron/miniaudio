/*
Demonstrates how to implement loopback recording.

This example simply captures data from your default playback device until you press Enter. The output is saved to the
file specified on the command line.

Loopback mode is when you record audio that is played from a given speaker. It is only supported on WASAPI, but can be
used indirectly with PulseAudio by choosing the appropriate loopback device after enumeration.

To use loopback mode you just need to set the device type to ma_device_type_loopback and set the capture device config
properties. The output buffer in the callback will be null whereas the input buffer will be valid.
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

    /* Loopback mode is currently only supported on WASAPI. */
    ma_backend backends[] = {
        ma_backend_wasapi
    };

    if (argc < 2) {
        printf("No output file.\n");
        return -1;
    }

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 44100);

    if (ma_encoder_init_file(argv[1], &encoderConfig, &encoder) != MA_SUCCESS) {
        printf("Failed to initialize output file.\n");
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.pDeviceID = NULL; /* Use default device for this example. Set this to the ID of a _playback_ device if you want to capture from a specific device. */
    deviceConfig.capture.format    = encoder.config.format;
    deviceConfig.capture.channels  = encoder.config.channels;
    deviceConfig.sampleRate        = encoder.config.sampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &encoder;

    result = ma_device_init_ex(backends, sizeof(backends)/sizeof(backends[0]), NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize loopback device.\n");
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
