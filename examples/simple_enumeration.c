/*
Demonstrates how to enumerate over devices.

Device enumaration requires a `ma_context` object which is initialized with `ma_context_init()`. Conceptually, the
context sits above a device. You can have many devices to one context.

If you use device enumeration, you should explicitly specify the same context you used for enumeration in the call to
`ma_device_init()` when you initialize your devices.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    ma_result result;
    ma_context context;
    ma_device_info* pPlaybackDeviceInfos;
    ma_uint32 playbackDeviceCount;
    ma_device_info* pCaptureDeviceInfos;
    ma_uint32 captureDeviceCount;
    ma_uint32 iDevice;

    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        printf("Failed to initialize context.\n");
        return -2;
    }

    result = ma_context_get_devices(&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount);
    if (result != MA_SUCCESS) {
        printf("Failed to retrieve device information.\n");
        return -3;
    }

    printf("Playback Devices\n");
    for (iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, pPlaybackDeviceInfos[iDevice].name);
    }

    printf("\n");

    printf("Capture Devices\n");
    for (iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
    }


    ma_context_uninit(&context);

    (void)argc;
    (void)argv;
    return 0;
}
