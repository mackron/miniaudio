#define _CRT_SECURE_NO_WARNINGS

// These are implemented at the bottom of this file.
#define STB_VORBIS_HEADER_ONLY
#include "../extras/stb_vorbis.c"
#include "../extras/dr_flac.h"
#include "../extras/dr_wav.h"
#include "../extras/jar_mod.h"
#include "../extras/jar_xm.h"

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

    //mal_decoder_config decoderConfig = mal_decoder_config_init(mal_format_f32, 2, 48000);
    mal_decoder decoder;
    mal_result result = mal_decoder_init_file(argv[1], /*&decoderConfig*/NULL, &decoder);
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

#if 0
#include <stdio.h>

mal_uint32 on_send_flac_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    drflac* pFlac = (drflac*)pDevice->pUserData;
    if (pFlac == NULL) {
        return 0;
    }

    return (mal_uint32)drflac_read_s16(pFlac, frameCount * pDevice->channels, (mal_int16*)pSamples) / pDevice->channels;
}

mal_uint32 on_send_wav_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    drwav* pWav = (drwav*)pDevice->pUserData;
    if (pWav == NULL) {
        return 0;
    }

    return (mal_uint32)drwav_read_s16(pWav, frameCount * pDevice->channels, (mal_int16*)pSamples) / pDevice->channels;
}

mal_uint32 on_send_vorbis_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    stb_vorbis* pVorbis = (stb_vorbis*)pDevice->pUserData;
    if (pVorbis == NULL) {
        return 0;
    }

    return (mal_uint32)stb_vorbis_get_samples_short_interleaved(pVorbis, pDevice->channels, (short*)pSamples, frameCount * pDevice->channels) / pDevice->channels;
}

mal_uint32 on_send_mod_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    jar_mod_context_t* pMod = (jar_mod_context_t*)pDevice->pUserData;
    if (pMod == NULL) {
        return 0;
    }

    jar_mod_fillbuffer(pMod, (mal_int16*)pSamples, frameCount, 0);
    return frameCount;
}

mal_uint32 on_send_xm_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    jar_xm_context_t* pXM = (jar_xm_context_t*)pDevice->pUserData;
    if (pXM == NULL) {
        return 0;
    }

    jar_xm_generate_samples_16bit(pXM, (short*)pSamples, frameCount);
    return frameCount;
}

int main(int argc, char** argv)
{
    int exitcode = 0;

    if (argc < 2) {
        printf("No input file.");
        return -1;
    }

    enum { UNK, FLAC, WAV, VORBIS, MOD, XM } type = UNK;

    jar_mod_context_t mod;
    jar_mod_init(&mod);

    jar_xm_context_t *xm = 0;

    drflac* flac = NULL;
    drwav* wav = NULL;
    stb_vorbis* vorbis = NULL;
    if ( type == UNK && (flac = drflac_open_file(argv[1])) != NULL)                       type = FLAC;
    if ( type == UNK && (wav = drwav_open_file(argv[1])) != NULL)                         type = WAV;
    if ( type == UNK && (vorbis = stb_vorbis_open_filename(argv[1], NULL, NULL)) != NULL) type = VORBIS;
    if ( type == UNK && (jar_xm_create_context_from_file(&xm, 48000, argv[1]) == 0))      type = XM;
    if ( type == UNK && (jar_mod_load_file(&mod, argv[1]) != 0) )                         type = MOD;

    if( type == UNK ) {
        printf("Not a valid input file.");
        exitcode = -2;
        goto end;
    }

    mal_context context;
    if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.");
        exitcode = -3;
        goto end;
    }

    void* pUserData = NULL;
    mal_device_config config;
    switch (type)
    {
        case FLAC:
            config = mal_device_config_init_playback(mal_format_s16, flac->channels, flac->sampleRate, on_send_flac_frames_to_device);
            pUserData = flac;
            break;

        case WAV:
            config = mal_device_config_init_playback(mal_format_s16, wav->channels, wav->sampleRate, on_send_wav_frames_to_device);
            pUserData = wav;
            break;

        case VORBIS:
            config = mal_device_config_init_playback(mal_format_s16, vorbis->channels, vorbis->sample_rate, on_send_vorbis_frames_to_device);
            pUserData = vorbis;
            break;

        case MOD:
            config = mal_device_config_init_playback(mal_format_s16, 2, mod.playrate, on_send_mod_frames_to_device);
            pUserData = &mod;
            break;

        case XM:
            config = mal_device_config_init_playback(mal_format_s16, 2, 48000, on_send_xm_frames_to_device);
            pUserData = xm;
            break;
    }

    mal_device device;
    if (mal_device_init(&context, mal_device_type_playback, NULL, &config, pUserData, &device) != MAL_SUCCESS) {
        printf("Failed to open playback device.");
        mal_context_uninit(&context);
        exitcode = -4;
        goto end;
    }

    if (mal_device_start(&device) != MAL_SUCCESS) {
        printf("Failed to start playback device.\n");
        mal_device_uninit(&device);
        mal_context_uninit(&context);
        exitcode = -4;
        goto end;
    }

    printf("Press Enter to quit...");
    getchar();

    mal_device_uninit(&device);
    mal_context_uninit(&context);

end:;
    drflac_close(flac);
    drwav_close(wav);
    stb_vorbis_close(vorbis);
    jar_mod_unload(&mod);
    if(xm) jar_xm_free_context(xm);

    return exitcode;
}
#endif


#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"

#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

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

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable:4100)
    #pragma warning(disable:4244)
    #pragma warning(disable:4018)
    #pragma warning(disable:4456)
    #pragma warning(disable:4701)
    #pragma warning(disable:4702)
    #pragma warning(disable:4706)
    #pragma warning(disable:4127)
#endif
#define JAR_MOD_IMPLEMENTATION
#include "../extras/jar_mod.h"
#undef DEBUG

#define JAR_XM_IMPLEMENTATION
#include "../extras/jar_xm.h"
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif