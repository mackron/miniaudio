
#define MA_LOG_LEVEL MA_LOG_LEVEL_VERBOSE
#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

int print_context_info(ma_context* pContext)
{
    ma_result result = MA_SUCCESS;
    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_device_info* pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;

    printf("BACKEND: %s\n", ma_get_backend_name(pContext->backend));

    // Enumeration.
    printf("  Enumerating Devices... ");
    {
        result = ma_context_get_devices(pContext, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
        if (result == MA_SUCCESS) {
            printf("Done\n");
        } else {
            printf("Failed\n");
            goto done;
        }

        printf("    Playback Devices (%d)\n", playbackDeviceCount);
        for (ma_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (ma_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
        }
    }

    // Device Information.
    printf("  Getting Device Information...\n");
    {
        printf("    Playback Devices (%d)\n", playbackDeviceCount);
        for (ma_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
            
            result = ma_context_get_device_info(pContext, ma_device_type_playback, &pPlaybackDeviceInfos[iDevice].id, ma_share_mode_shared, &pPlaybackDeviceInfos[iDevice]);
            if (result == MA_SUCCESS) {
                printf("        Name:            %s\n", pPlaybackDeviceInfos[iDevice].name);
                printf("        Min Channels:    %d\n", pPlaybackDeviceInfos[iDevice].minChannels);
                printf("        Max Channels:    %d\n", pPlaybackDeviceInfos[iDevice].maxChannels);
                printf("        Min Sample Rate: %d\n", pPlaybackDeviceInfos[iDevice].minSampleRate);
                printf("        Max Sample Rate: %d\n", pPlaybackDeviceInfos[iDevice].maxSampleRate);
                printf("        Format Count:    %d\n", pPlaybackDeviceInfos[iDevice].formatCount);
                for (ma_uint32 iFormat = 0; iFormat < pPlaybackDeviceInfos[iDevice].formatCount; ++iFormat) {
                    printf("          %s\n", ma_get_format_name(pPlaybackDeviceInfos[iDevice].formats[iFormat]));
                }
            } else {
                printf("        ERROR\n");
            }
        }

        printf("    Capture Devices (%d)\n", captureDeviceCount);
        for (ma_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
            printf("      %d: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
            
            result = ma_context_get_device_info(pContext, ma_device_type_capture, &pCaptureDeviceInfos[iDevice].id, ma_share_mode_shared, &pCaptureDeviceInfos[iDevice]);
            if (result == MA_SUCCESS) {
                printf("        Name:            %s\n", pCaptureDeviceInfos[iDevice].name);
                printf("        Min Channels:    %d\n", pCaptureDeviceInfos[iDevice].minChannels);
                printf("        Max Channels:    %d\n", pCaptureDeviceInfos[iDevice].maxChannels);
                printf("        Min Sample Rate: %d\n", pCaptureDeviceInfos[iDevice].minSampleRate);
                printf("        Max Sample Rate: %d\n", pCaptureDeviceInfos[iDevice].maxSampleRate);
                printf("        Format Count:    %d\n", pCaptureDeviceInfos[iDevice].formatCount);
                for (ma_uint32 iFormat = 0; iFormat < pCaptureDeviceInfos[iDevice].formatCount; ++iFormat) {
                    printf("          %s\n", ma_get_format_name(pCaptureDeviceInfos[iDevice].formats[iFormat]));
                }
            } else {
                printf("        ERROR\n");
            }
        }
    }

done:
    printf("\n");
    return (result == MA_SUCCESS) ? 0 : -1;
}

int print_device_info(ma_device* pDevice)
{
    printf("DEVICE NAME: %s\n", pDevice->name);
    printf("  Format:      %s -> %s\n", ma_get_format_name(pDevice->format), ma_get_format_name(pDevice->internalFormat));
    printf("  Channels:    %d -> %d\n", pDevice->channels, pDevice->internalChannels);
    printf("  Sample Rate: %d -> %d\n", pDevice->sampleRate, pDevice->internalSampleRate);
    printf("  Buffer Size: %d\n",       pDevice->bufferSizeInFrames);
    printf("  Periods:     %d\n",       pDevice->periods);
    
    return 0;
}

ma_uint32 on_send(ma_device* pDevice, ma_uint32 frameCount, void* pFramesOut)
{
    ma_sine_wave* pSineWave = (ma_sine_wave*)pDevice->pUserData;
    ma_assert(pSineWave != NULL);
    
    float* pFramesOutF32 = (float*)pFramesOut;
    
    for (ma_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
        float sample;
        ma_sine_wave_read(pSineWave, 1, &sample);
        for (ma_uint32 iChannel = 0; iChannel < pDevice->channels; ++iChannel) {
            pFramesOutF32[iChannel] = sample;
        }
        
        pFramesOutF32 += pDevice->channels;
    }
    
    return frameCount;
}

int main(int argc, char** argv)
{
    ma_result result;
    
    ma_sine_wave sineWave;
    result = ma_sine_wave_init(0.2, 400, 44100, &sineWave);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize sine wave.\n");
        return -1;
    }

    // Separate context for this test.
    ma_context_config contextConfig = ma_context_config_init(NULL);   // <-- Don't need a log callback because we're using debug output instead.
    ma_context context;
    result = ma_context_init(NULL, 0, &contextConfig, &context);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize context.\n");
        return -1;
    }
    
    print_context_info(&context);
    
    
    // Device.
    ma_device_config deviceConfig = ma_device_config_init_playback(ma_format_f32, 2, 44100, on_send);
    deviceConfig.bufferSizeInFrames = 32768;
    
    ma_device device;
    result = ma_device_init(&context, ma_device_type_playback, NULL, &deviceConfig, &sineWave, &device);
    if (result != MA_SUCCESS) {
        ma_context_uninit(&context);
        printf("Failed to initialize device.\n");
        return -1;
    }
    
    print_device_info(&device);
    
    
    // Start playback.
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        ma_context_uninit(&context);
        printf("Failed to start device.\n");
        return -1;
    }
    
    
    printf("Press Enter to quit...\n");
    getchar();
    
    
    ma_device_uninit(&device);
    ma_context_uninit(&context);
    return 0;
}
