#define _CRT_SECURE_NO_WARNINGS

// These are implemented at the bottom of this file.
#define STB_VORBIS_HEADER_ONLY
#include "../extras/stb_vorbis.c"
#include "../extras/dr_flac.h"
#include "../extras/dr_wav.h"
#include "../extras/dr_mp3.h"

#define MAL_IMPLEMENTATION
#include "../mini_al.h"

mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pFramesOut)
{
    mal_decoder* pDecoder = (mal_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return 0;
    }

    return (mal_uint32)mal_decoder_read(pDecoder, frameCount, pFramesOut);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("No input file.");
        return -1;
    }

    mal_decoder decoder;
    mal_result result = mal_decoder_init_file(argv[1], NULL, &decoder);
    if (result != MAL_SUCCESS) {
        return -2;
    }


    mal_context context;
    if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.\n");
        mal_decoder_uninit(&decoder);
        return -3;
    }

    mal_device_config deviceConfig = mal_device_config_init_playback(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, on_send_frames_to_device);
    mal_copy_memory(&deviceConfig.channelMap, decoder.outputChannelMap, sizeof(decoder.outputChannelMap));

    mal_device device;
    if (mal_device_init(&context, mal_device_type_playback, NULL, &deviceConfig, &decoder, &device) != MAL_SUCCESS) {
        printf("Failed to initialize device.\n");
        mal_context_uninit(&context);
        mal_decoder_uninit(&decoder);
        return -4;
    }

    if (mal_device_start(&device) != MAL_SUCCESS) {
        printf("Failed to start playback device.\n");
        mal_device_uninit(&device);
        mal_context_uninit(&context);
        mal_decoder_uninit(&decoder);
        return -5;
    }


    printf("Press Enter to quit...");
    getchar();

    mal_device_uninit(&device);
    mal_context_uninit(&context);
    mal_decoder_uninit(&decoder);

    return 0;
}


#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"

#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable:4456)
    #pragma warning(disable:4457)
    #pragma warning(disable:4100)
    #pragma warning(disable:4244)
    #pragma warning(disable:4701)
    #pragma warning(disable:4245)
#endif
#if defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-value"
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #ifndef __clang__
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    #endif
#endif
#undef STB_VORBIS_HEADER_ONLY
#include "../extras/stb_vorbis.c"
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#if defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
