// Uncomment this to include Vorbis decoding tests, albeit with some annoying warnings with MinGW.
//#define MAL_INCLUDE_VORBIS_TESTS

#include "../extras/dr_flac.h"
#include "../extras/dr_mp3.h"
#include "../extras/dr_wav.h"

#ifdef MAL_INCLUDE_VORBIS_TESTS
    #define STB_VORBIS_HEADER_ONLY
    #include "../extras/stb_vorbis.c"
#endif

#define MAL_IMPLEMENTATION
#include "../mini_al.h"

mal_backend g_Backends[] = {
    mal_backend_wasapi,
    mal_backend_dsound,
    mal_backend_winmm,
    mal_backend_oss,
    mal_backend_pulseaudio,
    mal_backend_alsa,
    mal_backend_jack,
    mal_backend_opensl,
    mal_backend_openal,
    mal_backend_sdl,
    mal_backend_null
};

void on_log(mal_context* pContext, mal_device* pDevice, const char* message)
{
    (void)pContext;
    (void)pDevice;
    printf("%s\n", message);
}


int do_backend_test(mal_backend backend)
{
    mal_result result = MAL_SUCCESS;
    mal_context context;
    mal_device_info* pPlaybackDeviceInfos;
    mal_uint32 playbackDeviceCount;
    mal_device_info* pCaptureDeviceInfos;
    mal_uint32 captureDeviceCount;

    printf("--- %s ---\n", mal_get_backend_name(backend));

    // Context.
    printf("  Creating Context... ");
    {
        mal_context_config contextConfig = mal_context_config_init(on_log);

        result = mal_context_init(&backend, 1, &contextConfig, &context);
        if (result == MAL_SUCCESS) {
            printf(" Done\n");
        } else {
            if (result == MAL_NO_BACKEND) {
                printf(" Not supported\n");
                printf("--- End %s ---\n", mal_get_backend_name(backend));
                printf("\n");
                return 0;
            } else {
                printf(" Failed\n");
                goto done;
            }
        }
    }

    // Enumeration.
    printf("  Enumerating Devices... ");
    {
        result = mal_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
        if (result == MAL_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed\n");
            goto done;
        }

        printf("    Playback Devices (%d)\n", playbackDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
        }
    }

    // Device Information.
    printf("  Getting Device Information...\n");
    {
        printf("    Playback Devices (%d)\n", playbackDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
            
            result = mal_context_get_device_info(&context, mal_device_type_playback, &pPlaybackDeviceInfos[iDevice].id, mal_share_mode_shared, &pPlaybackDeviceInfos[iDevice]);
            if (result == MAL_SUCCESS) {
                printf("        Name: %s\n", pPlaybackDeviceInfos[iDevice].name);
            } else {
                printf("        ERROR\n");
            }
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
            
            result = mal_context_get_device_info(&context, mal_device_type_capture, &pCaptureDeviceInfos[iDevice].id, mal_share_mode_shared, &pCaptureDeviceInfos[iDevice]);
            if (result == MAL_SUCCESS) {
                printf("        Name: %s\n", pCaptureDeviceInfos[iDevice].name);
            } else {
                printf("        ERROR\n");
            }
        }
    }

done:
    printf("--- End %s ---\n", mal_get_backend_name(backend));
    printf("\n");
    
    mal_context_uninit(&context);
    return (result == MAL_SUCCESS) ? 0 : -1;
}

int do_backend_tests()
{
    mal_bool32 hasErrorOccurred = MAL_FALSE;

    // Tests are performed on a per-backend basis.
    for (size_t iBackend = 0; iBackend < mal_countof(g_Backends); ++iBackend) {
        int result = do_backend_test(g_Backends[iBackend]);
        if (result < 0) {
            hasErrorOccurred = MAL_TRUE;
        }
    }

    return (hasErrorOccurred) ? -1 : 0;
}


typedef struct
{
    mal_decoder* pDecoder;
    mal_event endOfPlaybackEvent;
} playback_test_callback_data;

mal_uint32 on_send__playback_test(mal_device* pDevice, mal_uint32 frameCount, void* pFrames)
{
    playback_test_callback_data* pData = (playback_test_callback_data*)pDevice->pUserData;
    mal_assert(pData != NULL);

    mal_uint64 framesRead = mal_decoder_read(pData->pDecoder, frameCount, pFrames);
    if (framesRead == 0) {
        mal_event_signal(&pData->endOfPlaybackEvent);
    }

    return (mal_uint32)framesRead;
}

int do_playback_test(mal_backend backend)
{
    mal_result result = MAL_SUCCESS;
    mal_device device;
    mal_decoder decoder;
    mal_bool32 haveDevice = MAL_FALSE;
    mal_bool32 haveDecoder = MAL_FALSE;

    playback_test_callback_data callbackData;
    callbackData.pDecoder = &decoder;

    printf("--- %s ---\n", mal_get_backend_name(backend));

    // Device.
    printf("  Opening Device... ");
    {
        mal_context_config contextConfig = mal_context_config_init(on_log);
        mal_device_config deviceConfig = mal_device_config_init_default_playback(on_send__playback_test);

        result = mal_device_init_ex(&backend, 1, &contextConfig, mal_device_type_playback, NULL, &deviceConfig, &callbackData, &device);
        if (result == MAL_SUCCESS) {
            printf("Done\n");
        } else {
            if (result == MAL_NO_BACKEND) {
                printf(" Not supported\n");
                printf("--- End %s ---\n", mal_get_backend_name(backend));
                printf("\n");
                return 0;
            } else {
                printf(" Failed\n");
                goto done;
            }
        }
        haveDevice = MAL_TRUE;
    }

    printf("  Opening Decoder... ");
    {
        result = mal_event_init(device.pContext, &callbackData.endOfPlaybackEvent);
        if (result != MAL_SUCCESS) {
            printf("Failed to init event.\n");
            goto done;
        }
        
        mal_decoder_config decoderConfig = mal_decoder_config_init(device.format, device.channels, device.sampleRate);
        result = mal_decoder_init_file("res/sine_s16_mono_48000.wav", &decoderConfig, &decoder);
        if (result == MAL_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed to init decoder.\n");
            goto done;
        }
        haveDecoder = MAL_TRUE;
    }

    printf("  Press Enter to start playback... ");
    getchar();
    {
        result = mal_device_start(&device);
        if (result != MAL_SUCCESS) {
            printf("Failed to start device.\n");
            goto done;
        }

        mal_event_wait(&callbackData.endOfPlaybackEvent);   // Wait for the sound to finish.
        printf("Done\n");
    }

done:
    printf("--- End %s ---\n", mal_get_backend_name(backend));
    printf("\n");
    
    if (haveDevice) {
        mal_device_uninit(&device);
    }
    if (haveDecoder) {
        mal_decoder_uninit(&decoder);
    }
    return (result == MAL_SUCCESS) ? 0 : -1;
}

int do_playback_tests()
{
    mal_bool32 hasErrorOccurred = MAL_FALSE;

    for (size_t iBackend = 0; iBackend < mal_countof(g_Backends); ++iBackend) {
        int result = do_playback_test(g_Backends[iBackend]);
        if (result < 0) {
            hasErrorOccurred = MAL_TRUE;
        }
    }

    return (hasErrorOccurred) ? -1 : 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    mal_bool32 hasErrorOccurred = MAL_FALSE;
    int result = 0;
    
    printf("=== TESTING BACKENDS ===\n");
    result = do_backend_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING BACKENDS ===\n");

    printf("\n");

    printf("=== TESTING DEFAULT PLAYBACK DEVICES ===\n");
    result = do_playback_tests();
    if (result < 0) {
        hasErrorOccurred = MAL_TRUE;
    }
    printf("=== END TESTING DEFAULT PLAYBACK DEVICES ===\n");

    return (hasErrorOccurred) ? -1 : 0;
}

#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

#ifdef MAL_INCLUDE_VORBIS_TESTS
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
#endif