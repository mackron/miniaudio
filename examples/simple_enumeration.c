#define MAL_IMPLEMENTATION
#include "../mini_al.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    mal_context context;
    if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.");
        return -2;
    }

    mal_device_info infos[32];
    mal_uint32 infoCount = sizeof(infos) / sizeof(infos[0]);

    // Playback devices.
    mal_result result = mal_enumerate_devices(&context, mal_device_type_playback, &infoCount, infos);
    if (result != MAL_SUCCESS) {
        printf("Failed to enumerate playback devices.");
        mal_context_uninit(&context);
        return -3;
    }

    printf("Playback Devices\n");
    for (mal_uint32 iDevice = 0; iDevice < infoCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, infos[iDevice].name);
    }


    printf("\n");


    // Capture devices.
    result = mal_enumerate_devices(&context, mal_device_type_capture, &infoCount, infos);
    if (result != MAL_SUCCESS) {
        printf("Failed to enumerate capture devices.");
        mal_context_uninit(&context);
        return -4;
    }

    printf("Capture Devices\n");
    for (mal_uint32 iDevice = 0; iDevice < infoCount; ++iDevice) {
        printf("    %u: %s\n", iDevice, infos[iDevice].name);
    }


    mal_context_uninit(&context);
    return 0;
}
