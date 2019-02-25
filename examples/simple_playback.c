#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"  // Enables FLAC decoding.
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"   // Enables MP3 decoding.
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"   // Enables WAV decoding.

#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

#include <stdio.h>

void data_callback(mal_device* pDevice, void* pOutput, const void* pInput, mal_uint32 frameCount)
{
    mal_decoder* pDecoder = (mal_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return;
    }

    mal_decoder_read_pcm_frames(pDecoder, pOutput, frameCount);

    (void)pInput;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    mal_decoder decoder;
    mal_result result = mal_decoder_init_file(argv[1], NULL, &decoder);
    if (result != MAL_SUCCESS) {
        return -2;
    }

    mal_device_config config = mal_device_config_init(mal_device_type_playback);
    config.playback.pDeviceID = NULL;
    config.playback.format    = decoder.outputFormat;
    config.playback.channels  = decoder.outputChannels;
    config.sampleRate         = decoder.outputSampleRate;
    config.dataCallback       = data_callback;
    config.pUserData          = &decoder;

    mal_device device;
    if (mal_device_init(NULL, &config, &device) != MAL_SUCCESS) {
        printf("Failed to open playback device.\n");
        mal_decoder_uninit(&decoder);
        return -3;
    }

    if (mal_device_start(&device) != MAL_SUCCESS) {
        printf("Failed to start playback device.\n");
        mal_device_uninit(&device);
        mal_decoder_uninit(&decoder);
        return -4;
    }

    printf("Press Enter to quit...");
    getchar();

    mal_device_uninit(&device);
    mal_decoder_uninit(&decoder);

    return 0;
}
