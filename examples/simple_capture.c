// This example simply captures data from your default microphone until you press Enter. The output is saved to the file specified on the command line.

#define MINI_AL_IMPLEMENTATION
#include "../miniaudio.h"

#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

#include <stdlib.h>
#include <stdio.h>

void data_callback(mal_device* pDevice, void* pOutput, const void* pInput, mal_uint32 frameCount)
{
    (void)pOutput;
    
    drwav* pWav = (drwav*)pDevice->pUserData;
    mal_assert(pWav != NULL);

    drwav_write_pcm_frames(pWav, frameCount, pInput);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    mal_result result;

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

    mal_device_config config = mal_device_config_init(mal_device_type_capture);
    config.capture.format   = mal_format_f32;
    config.capture.channels = wavFormat.channels;
    config.sampleRate       = wavFormat.sampleRate;
    config.dataCallback     = data_callback;
    config.pUserData        = &wav;

    mal_device device;
    result = mal_device_init(NULL, &config, &device);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize capture device.\n");
        return -2;
    }

    result = mal_device_start(&device);
    if (result != MAL_SUCCESS) {
        mal_device_uninit(&device);
        printf("Failed to start device.\n");
        return -3;
    }

    printf("Press Enter to stop recording...\n");
    getchar();
    
    mal_device_uninit(&device);
    drwav_uninit(&wav);

    return 0;
}