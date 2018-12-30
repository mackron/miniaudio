#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"  // Enables FLAC decoding.
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"   // Enables MP3 decoding.
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"   // Enables WAV decoding.

#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

#include <stdio.h>

// This is the function that's used for sending more data to the device for playback.
mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    mal_decoder* pDecoder = (mal_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return 0;
    }

    return (mal_uint32)mal_decoder_read_pcm_frames(pDecoder, frameCount, pSamples);
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

    mal_device_config config = mal_device_config_init_playback(
        decoder.outputFormat,
        decoder.outputChannels,
        decoder.outputSampleRate,
        on_send_frames_to_device,
        &decoder);

    mal_device device;
    if (mal_device_init(NULL, mal_device_type_playback, NULL, &config, &device) != MAL_SUCCESS) {
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
