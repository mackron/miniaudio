// This example simply captures data from your default microphone until you press Enter, after
// which it plays back the captured audio.

#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

#include <stdlib.h>
#include <stdio.h>


mal_uint32 capturedSampleCount = 0;
mal_int16* pCapturedSamples = NULL;
mal_uint32 playbackSample = 0;

void on_recv_frames(mal_device* pDevice, const void* pInput, void* pOutput, mal_uint32 frameCount)
{
    mal_uint32 sampleCount = frameCount * pDevice->channels;

    mal_uint32 newCapturedSampleCount = capturedSampleCount + sampleCount;
    mal_int16* pNewCapturedSamples = (mal_int16*)realloc(pCapturedSamples, newCapturedSampleCount * sizeof(mal_int16));
    if (pNewCapturedSamples == NULL) {
        return;
    }

    memcpy(pNewCapturedSamples + capturedSampleCount, pInput, sampleCount * sizeof(mal_int16));

    pCapturedSamples = pNewCapturedSamples;
    capturedSampleCount = newCapturedSampleCount;

    (void)pOutput;
}

void on_send_frames(mal_device* pDevice, const void* pInput, void* pOutput, mal_uint32 frameCount)
{
    mal_uint32 samplesToRead = frameCount * pDevice->channels;
    if (samplesToRead > capturedSampleCount-playbackSample) {
        samplesToRead = capturedSampleCount-playbackSample;
    }

    if (samplesToRead == 0) {
        return;
    }

    memcpy(pOutput, pCapturedSamples + playbackSample, samplesToRead * sizeof(mal_int16));
    playbackSample += samplesToRead;

    (void)pInput;
}

int main()
{
    mal_device_config config;

    mal_context context;
    if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.");
        return -1;
    }

    printf("Recording...\n");
    config = mal_device_config_init(mal_format_s16, 2, 48000, on_recv_frames, NULL);
    mal_device captureDevice;
    if (mal_device_init(&context, mal_device_type_capture, NULL, &config, &captureDevice) != MAL_SUCCESS) {
        mal_context_uninit(&context);
        printf("Failed to initialize capture device.\n");
        return -2;
    }

    if (mal_device_start(&captureDevice) != MAL_SUCCESS) {
        mal_device_uninit(&captureDevice);
        mal_context_uninit(&context);
        printf("Failed to start capture device.\n");
        return -3;
    }

    printf("Press Enter to stop recording...\n");
    getchar();

    mal_device_uninit(&captureDevice);



    printf("Playing...\n");
    config = mal_device_config_init(mal_format_s16, 2, 48000, on_send_frames, NULL);
    mal_device playbackDevice;
    if (mal_device_init(&context, mal_device_type_playback, NULL, &config, &playbackDevice) != MAL_SUCCESS) {
        mal_context_uninit(&context);
        printf("Failed to initialize playback device.\n");
        return -4;
    }

    if (mal_device_start(&playbackDevice) != MAL_SUCCESS) {
        mal_device_uninit(&playbackDevice);
        mal_context_uninit(&context);
        printf("Failed to start playback device.\n");
        return -5;
    }

    printf("Press Enter to quit...\n");
    getchar();

    mal_device_uninit(&playbackDevice);

    mal_context_uninit(&context);
    return 0;
}