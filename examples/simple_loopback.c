/*
This example simply captures data from your default playback device until you press Enter. The output is saved to the file
specified on the command line.

The use loopback mode you just need to set the device type to ma_device_type_loopback and set the capture device config
properties. The output buffer in the callback will be null whereas the input buffer will be valid.
*/

#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

#include <stdlib.h>
#include <stdio.h>

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    drwav* pWav = (drwav*)pDevice->pUserData;
    MA_ASSERT(pWav != NULL);

    drwav_write_pcm_frames(pWav, frameCount, pInput);

    (void)pOutput;
}

int main(int argc, char** argv)
{
    ma_result result;
    drwav_data_format wavFormat;
    drwav wav;
    ma_device_config deviceConfig;
    ma_device device;

    /* Loopback mode is currently only supported on WASAPI. */
    ma_backend backends[] = {
        ma_backend_wasapi
    };

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    wavFormat.container     = drwav_container_riff;
    wavFormat.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
    wavFormat.channels      = 2;
    wavFormat.sampleRate    = 44100;
    wavFormat.bitsPerSample = 32;

    if (drwav_init_file_write(&wav, argv[1], &wavFormat, NULL) == DRWAV_FALSE) {
        printf("Failed to initialize output file.\n");
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.pDeviceID = NULL; /* Use default device for this example. Set this to the ID of a _playback_ device if you want to capture from a specific device. */
    deviceConfig.capture.format    = ma_format_f32;
    deviceConfig.capture.channels  = wavFormat.channels;
    deviceConfig.sampleRate        = wavFormat.sampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &wav;

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
    drwav_uninit(&wav);

    return 0;
}
