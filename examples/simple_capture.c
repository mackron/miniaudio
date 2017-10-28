// This example simply captures data from your default microphone until you press Enter, after
// which it plays back the captured audio.

#define MAL_IMPLEMENTATION
#include "../mini_al.h"

#include <stdlib.h>
#include <stdio.h>


mal_uint32 capturedSampleCount = 0;
mal_int16* pCapturedSamples = NULL;
mal_uint32 playbackSample = 0;

void on_recv_frames(mal_device* pDevice, mal_uint32 frameCount, const void* pSamples)
{
    mal_uint32 sampleCount = frameCount * pDevice->channels;
    
    mal_uint32 newCapturedSampleCount = capturedSampleCount + sampleCount;
    mal_int16* pNewCapturedSamples = (mal_int16*)realloc(pCapturedSamples, newCapturedSampleCount * sizeof(mal_int16));
    if (pNewCapturedSamples == NULL) {
        return;
    }

    memcpy(pNewCapturedSamples + capturedSampleCount, pSamples, sampleCount * sizeof(mal_int16));

    pCapturedSamples = pNewCapturedSamples;
    capturedSampleCount = newCapturedSampleCount;
}

mal_uint32 on_send_frames(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    mal_uint32 samplesToRead = frameCount * pDevice->channels;
    if (samplesToRead > capturedSampleCount-playbackSample) {
        samplesToRead = capturedSampleCount-playbackSample;
    }

    if (samplesToRead == 0) {
        return 0;
    }

    memcpy(pSamples, pCapturedSamples + playbackSample, samplesToRead * sizeof(mal_int16));
    playbackSample += samplesToRead;

    return samplesToRead / pDevice->channels;
}

int main()
{
    mal_context context;
    if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.");
        return -1;
    }

    mal_device_config config = mal_device_config_init(mal_format_s16, 2, 48000, on_recv_frames, on_send_frames);

    printf("Recording...\n");
    mal_device captureDevice;
    if (mal_device_init(&context, mal_device_type_capture, NULL, &config, NULL, &captureDevice) != MAL_SUCCESS) {
        mal_context_uninit(&context);
        printf("Failed to initialize capture device.\n");
        return -2;
    }
    mal_device_start(&captureDevice);

    printf("Press Enter to stop recording...\n");
    getchar();
    mal_device_uninit(&captureDevice);
    
    

    printf("Playing...\n");
    mal_device playbackDevice;
    if (mal_device_init(&context, mal_device_type_playback, NULL, &config, NULL, &playbackDevice) != MAL_SUCCESS) {
        mal_context_uninit(&context);
        printf("Failed to initialize playback device.\n");
        return -3;
    }
    mal_device_start(&playbackDevice);

    printf("Press Enter to quit...\n");
    getchar();
    mal_device_uninit(&playbackDevice);

    mal_context_uninit(&context);
    return 0;
}