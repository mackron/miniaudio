#define MINI_AL_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    mal_context context;
    if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.\n");
        return -2;
    }

    mal_device_info* pPlaybackDeviceInfos;
    mal_uint32 playbackDeviceCount;
    mal_device_info* pCaptureDeviceInfos;
    mal_uint32 captureDeviceCount;
    mal_result result = mal_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
    if (result != MAL_SUCCESS) {
        printf("Failed to retrieve device information.\n");
        return -3;
    }

    printf("Playback Devices\n");
    for (mal_uint32 iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
    }

    printf("\n");

    printf("Capture Devices\n");
    for (mal_uint32 iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
    }


    mal_context_uninit(&context);
    return 0;
}
