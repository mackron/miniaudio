// This example simply captures data from your default microphone until you press Enter. The output is saved to the file specified on the command line.

#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

#include <stdlib.h>
#include <stdio.h>

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    (void)pOutput;
    
    drwav* pWav = (drwav*)pDevice->pUserData;
    ma_assert(pWav != NULL);

    drwav_write_pcm_frames(pWav, frameCount, pInput);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    ma_result result;

    drwav_data_format wavFormat;
    wavFormat.container     = drwav_container_riff;
    wavFormat.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
    wavFormat.channels      = 2;
    wavFormat.sampleRate    = 44100;
    wavFormat.bitsPerSample = 32;

    drwav wav;
    if (drwav_init_file_write(&wav, argv[1], &wavFormat) == DRWAV_FALSE) {
        printf("Failed to initialize output file.\n");
        return -1;
    }

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format   = ma_format_f32;
    config.capture.channels = wavFormat.channels;
    config.sampleRate       = wavFormat.sampleRate;
    config.dataCallback     = data_callback;
    config.pUserData        = &wav;

    ma_device device;
    result = ma_device_init(NULL, &config, &device);
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
    drwav_uninit(&wav);

    return 0;
}