
#define MAL_LOG_LEVEL MAL_LOG_LEVEL_VERBOSE
#define MAL_DEBUG_OUTPUT
#define MINI_AL_IMPLEMENTATION
#include "../miniaudio.h"

int print_context_info(mal_context* pContext)
{
    mal_result result = MAL_SUCCESS;
    mal_device_info* pPlaybackDeviceInfos;
    mal_uint32 playbackDeviceCount;
    mal_device_info* pCaptureDeviceInfos;
    mal_uint32 captureDeviceCount;

    printf("BACKEND: %s\n", mal_get_backend_name(pContext->backend));

    // Enumeration.
    printf("  Enumerating Devices... ");
    {
        result = mal_context_get_devices(pContext, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
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
            
            result = mal_context_get_device_info(pContext, mal_device_type_playback, &pPlaybackDeviceInfos[iDevice].id, mal_share_mode_shared, &pPlaybackDeviceInfos[iDevice]);
            if (result == MAL_SUCCESS) {
                printf("        Name:            %s\n", pPlaybackDeviceInfos[iDevice].name);
                printf("        Min Channels:    %d\n", pPlaybackDeviceInfos[iDevice].minChannels);
                printf("        Max Channels:    %d\n", pPlaybackDeviceInfos[iDevice].maxChannels);
                printf("        Min Sample Rate: %d\n", pPlaybackDeviceInfos[iDevice].minSampleRate);
                printf("        Max Sample Rate: %d\n", pPlaybackDeviceInfos[iDevice].maxSampleRate);
                printf("        Format Count:    %d\n", pPlaybackDeviceInfos[iDevice].formatCount);
                for (mal_uint32 iFormat = 0; iFormat < pPlaybackDeviceInfos[iDevice].formatCount; ++iFormat) {
                    printf("          %s\n", mal_get_format_name(pPlaybackDeviceInfos[iDevice].formats[iFormat]));
                }
            } else {
                printf("        ERROR\n");
            }
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (mal_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
            
            result = mal_context_get_device_info(pContext, mal_device_type_capture, &pCaptureDeviceInfos[iDevice].id, mal_share_mode_shared, &pCaptureDeviceInfos[iDevice]);
            if (result == MAL_SUCCESS) {
                printf("        Name:            %s\n", pCaptureDeviceInfos[iDevice].name);
                printf("        Min Channels:    %d\n", pCaptureDeviceInfos[iDevice].minChannels);
                printf("        Max Channels:    %d\n", pCaptureDeviceInfos[iDevice].maxChannels);
                printf("        Min Sample Rate: %d\n", pCaptureDeviceInfos[iDevice].minSampleRate);
                printf("        Max Sample Rate: %d\n", pCaptureDeviceInfos[iDevice].maxSampleRate);
                printf("        Format Count:    %d\n", pCaptureDeviceInfos[iDevice].formatCount);
                for (mal_uint32 iFormat = 0; iFormat < pCaptureDeviceInfos[iDevice].formatCount; ++iFormat) {
                    printf("          %s\n", mal_get_format_name(pCaptureDeviceInfos[iDevice].formats[iFormat]));
                }
            } else {
                printf("        ERROR\n");
            }
        }
    }

done:
    printf("\n");
    return (result == MAL_SUCCESS) ? 0 : -1;
}

int print_device_info(mal_device* pDevice)
{
    printf("DEVICE NAME: %s\n", pDevice->name);
    printf("  Format:      %s -> %s\n", mal_get_format_name(pDevice->format), mal_get_format_name(pDevice->internalFormat));
    printf("  Channels:    %d -> %d\n", pDevice->channels, pDevice->internalChannels);
    printf("  Sample Rate: %d -> %d\n", pDevice->sampleRate, pDevice->internalSampleRate);
    printf("  Buffer Size: %d\n",       pDevice->bufferSizeInFrames);
    printf("  Periods:     %d\n",       pDevice->periods);
    
    return 0;
}

mal_uint32 on_send(mal_device* pDevice, mal_uint32 frameCount, void* pFramesOut)
{
    mal_sine_wave* pSineWave = (mal_sine_wave*)pDevice->pUserData;
    mal_assert(pSineWave != NULL);
    
    float* pFramesOutF32 = (float*)pFramesOut;
    
    for (mal_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
        float sample;
        mal_sine_wave_read(pSineWave, 1, &sample);
        for (mal_uint32 iChannel = 0; iChannel < pDevice->channels; ++iChannel) {
            pFramesOutF32[iChannel] = sample;
        }
        
        pFramesOutF32 += pDevice->channels;
    }
    
    return frameCount;
}

int main(int argc, char** argv)
{
    mal_result result;
    
    mal_sine_wave sineWave;
    result = mal_sine_wave_init(0.2, 400, 44100, &sineWave);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize sine wave.\n");
        return -1;
    }

    // Separate context for this test.
    mal_context_config contextConfig = mal_context_config_init(NULL);   // <-- Don't need a log callback because we're using debug output instead.
    mal_context context;
    result = mal_context_init(NULL, 0, &contextConfig, &context);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize context.\n");
        return -1;
    }
    
    print_context_info(&context);
    
    
    // Device.
    mal_device_config deviceConfig = mal_device_config_init_playback(mal_format_f32, 2, 44100, on_send);
    deviceConfig.bufferSizeInFrames = 32768;
    
    mal_device device;
    result = mal_device_init(&context, mal_device_type_playback, NULL, &deviceConfig, &sineWave, &device);
    if (result != MAL_SUCCESS) {
        mal_context_uninit(&context);
        printf("Failed to initialize device.\n");
        return -1;
    }
    
    print_device_info(&device);
    
    
    // Start playback.
    result = mal_device_start(&device);
    if (result != MAL_SUCCESS) {
        mal_device_uninit(&device);
        mal_context_uninit(&context);
        printf("Failed to start device.\n");
        return -1;
    }
    
    
    printf("Press Enter to quit...\n");
    getchar();
    
    
    mal_device_uninit(&device);
    mal_context_uninit(&context);
    return 0;
}
