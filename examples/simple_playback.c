#define MAL_IMPLEMENTATION
#include "../mini_al.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include <stdio.h>

// This is the function that's used for sending more data to the device for playback.
mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    drwav* pWav = (drwav*)pDevice->pUserData;
    if (pWav == NULL) {
        return 0;
    }
    
    return (mal_uint32)drwav_read_s16(pWav, frameCount * pDevice->channels, (mal_int16*)pSamples) / pDevice->channels;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("No input file.");
        return -1;
    }

    drwav wav;
    if (!drwav_init_file(&wav, argv[1])) {
        printf("Not a valid WAV file.");
        return -2;
    }

    mal_context context;
    if (mal_context_init(NULL, 0, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.");
        return -3;
    }
    
    mal_device_config config = mal_device_config_init_playback(mal_format_s16, wav.channels, wav.sampleRate, on_send_frames_to_device);
    
    mal_device device;
    if (mal_device_init(&context, mal_device_type_playback, NULL, &config, &wav, &device) != MAL_SUCCESS) {
        printf("Failed to open playback device.");
        mal_context_uninit(&context);
        drwav_uninit(&wav);
        return -4;
    }
    mal_device_start(&device);
    
    printf("Press Enter to quit...");
    getchar();
    
    mal_device_uninit(&device);
    mal_context_uninit(&context);
    drwav_uninit(&wav);
    
    return 0;
}