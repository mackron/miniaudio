// This example simply captures data from your default microphone until you press Enter, after
// which it plays back the captured audio.

#define MAL_IMPLEMENTATION
#include "../mini_al.h"

#include <stdlib.h>
#include <stdio.h>


mal_uint32 capturedSampleCount = 0;
float* pCapturedSamples = NULL;
mal_uint32 playbackSample = 0;

void on_recv_frames(mal_device* pDevice, mal_uint32 frameCount, const void* pSamples)
{
    mal_uint32 sampleCount = frameCount * pDevice->channels;
	
    mal_uint32 newCapturedSampleCount = capturedSampleCount + sampleCount;
    float* pNewCapturedSamples = (float*)realloc(pCapturedSamples, newCapturedSampleCount * sizeof(float));
    if (pNewCapturedSamples == NULL) {
        return;
    }

    memcpy(pNewCapturedSamples + capturedSampleCount, pSamples, sampleCount * sizeof(float));

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

    memcpy(pSamples, pCapturedSamples + playbackSample, samplesToRead * sizeof(float));
    playbackSample += samplesToRead;

    return samplesToRead / pDevice->channels;
}

int main()
{
    printf("Recording...\n");
    mal_device captureDevice;
    if (mal_device_init(&captureDevice, mal_device_type_capture, NULL, mal_format_f32, 2, 48000, 0, 0, NULL)) {
        printf("Failed to initialize capture device.\n");
        return -2;
    }
    mal_device_set_recv_callback(&captureDevice, on_recv_frames);
    mal_device_start(&captureDevice);

    printf("Press Enter to stop recording...\n");
    getchar();
    mal_device_uninit(&captureDevice);
    
    

    printf("Playing...\n");
    mal_device playbackDevice;
    if (mal_device_init(&playbackDevice, mal_device_type_playback, NULL, mal_format_f32, 2, 48000, 0, 0, NULL)) {
        printf("Failed to initialize playback device.\n");
        return -3;
    }
    mal_device_set_send_callback(&playbackDevice, on_send_frames);
    mal_device_start(&playbackDevice);

    printf("Press Enter to quit...\n");
	getchar();
    mal_device_uninit(&playbackDevice);

	return 0;
}