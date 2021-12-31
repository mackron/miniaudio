/*
Demonstrates one way to load multiple files and play them all back at the same time.

When mixing multiple sounds together, you should not create multiple devices. Instead you should create only a single
device and then mix your sounds together which you can do by simply summing their samples together. The simplest way to
do this is to use floating point samples and use miniaudio's built-in clipper to handling clipping for you. (Clipping
is when sample are clampled to their minimum and maximum range, which for floating point is -1..1.)

```
Usage:   simple_mixing [input file 0] [input file 1] ... [input file n]
Example: simple_mixing file1.wav file2.flac
```
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>

/*
For simplicity, this example requires the device to use floating point samples.
*/
#define SAMPLE_FORMAT   ma_format_f32
#define CHANNEL_COUNT   2
#define SAMPLE_RATE     48000

ma_uint32   g_decoderCount;
ma_decoder* g_pDecoders;
ma_bool32*  g_pDecodersAtEnd;

ma_event g_stopEvent; /* <-- Signaled by the audio thread, waited on by the main thread. */

ma_bool32 are_all_decoders_at_end()
{
    ma_uint32 iDecoder;
    for (iDecoder = 0; iDecoder < g_decoderCount; ++iDecoder) {
        if (g_pDecodersAtEnd[iDecoder] == MA_FALSE) {
            return MA_FALSE;
        }
    }

    return MA_TRUE;
}

ma_uint32 read_and_mix_pcm_frames_f32(ma_decoder* pDecoder, float* pOutputF32, ma_uint32 frameCount)
{
    /*
    The way mixing works is that we just read into a temporary buffer, then take the contents of that buffer and mix it with the
    contents of the output buffer by simply adding the samples together. You could also clip the samples to -1..+1, but I'm not
    doing that in this example.
    */
    ma_result result;
    float temp[4096];
    ma_uint32 tempCapInFrames = ma_countof(temp) / CHANNEL_COUNT;
    ma_uint32 totalFramesRead = 0;

    while (totalFramesRead < frameCount) {
        ma_uint64 iSample;
        ma_uint64 framesReadThisIteration;
        ma_uint32 totalFramesRemaining = frameCount - totalFramesRead;
        ma_uint32 framesToReadThisIteration = tempCapInFrames;
        if (framesToReadThisIteration > totalFramesRemaining) {
            framesToReadThisIteration = totalFramesRemaining;
        }

        result = ma_decoder_read_pcm_frames(pDecoder, temp, framesToReadThisIteration, &framesReadThisIteration);
        if (result != MA_SUCCESS || framesReadThisIteration == 0) {
            break;
        }

        /* Mix the frames together. */
        for (iSample = 0; iSample < framesReadThisIteration*CHANNEL_COUNT; ++iSample) {
            pOutputF32[totalFramesRead*CHANNEL_COUNT + iSample] += temp[iSample];
        }

        totalFramesRead += (ma_uint32)framesReadThisIteration;

        if (framesReadThisIteration < (ma_uint32)framesToReadThisIteration) {
            break;  /* Reached EOF. */
        }
    }
    
    return totalFramesRead;
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    float* pOutputF32 = (float*)pOutput;
    ma_uint32 iDecoder;

    MA_ASSERT(pDevice->playback.format == SAMPLE_FORMAT);   /* <-- Important for this example. */

    for (iDecoder = 0; iDecoder < g_decoderCount; ++iDecoder) {
        if (!g_pDecodersAtEnd[iDecoder]) {
            ma_uint32 framesRead = read_and_mix_pcm_frames_f32(&g_pDecoders[iDecoder], pOutputF32, frameCount);
            if (framesRead < frameCount) {
                g_pDecodersAtEnd[iDecoder] = MA_TRUE;
            }
        }
    }

    /*
    If at the end all of our decoders are at the end we need to stop. We cannot stop the device in the callback. Instead we need to
    signal an event to indicate that it's stopped. The main thread will be waiting on the event, after which it will stop the device.
    */
    if (are_all_decoders_at_end()) {
        ma_event_signal(&g_stopEvent);
    }

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_device_config deviceConfig;
    ma_device device;
    ma_uint32 iDecoder;

    if (argc < 2) {
        printf("No input files.\n");
        return -1;
    }

    g_decoderCount   = argc-1;
    g_pDecoders      = (ma_decoder*)malloc(sizeof(*g_pDecoders)      * g_decoderCount);
    g_pDecodersAtEnd = (ma_bool32*) malloc(sizeof(*g_pDecodersAtEnd) * g_decoderCount);

    /* In this example, all decoders need to have the same output format. */
    decoderConfig = ma_decoder_config_init(SAMPLE_FORMAT, CHANNEL_COUNT, SAMPLE_RATE);
    for (iDecoder = 0; iDecoder < g_decoderCount; ++iDecoder) {
        result = ma_decoder_init_file(argv[1+iDecoder], &decoderConfig, &g_pDecoders[iDecoder]);
        if (result != MA_SUCCESS) {
            ma_uint32 iDecoder2;
            for (iDecoder2 = 0; iDecoder2 < iDecoder; ++iDecoder2) {
                ma_decoder_uninit(&g_pDecoders[iDecoder2]);
            }
            free(g_pDecoders);
            free(g_pDecodersAtEnd);

            printf("Failed to load %s.\n", argv[1+iDecoder]);
            return -3;
        }
        g_pDecodersAtEnd[iDecoder] = MA_FALSE;
    }

    /* Create only a single device. The decoders will be mixed together in the callback. In this example the data format needs to be the same as the decoders. */
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = SAMPLE_FORMAT;
    deviceConfig.playback.channels = CHANNEL_COUNT;
    deviceConfig.sampleRate        = SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = NULL;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        for (iDecoder = 0; iDecoder < g_decoderCount; ++iDecoder) {
            ma_decoder_uninit(&g_pDecoders[iDecoder]);
        }
        free(g_pDecoders);
        free(g_pDecodersAtEnd);

        printf("Failed to open playback device.\n");
        return -3;
    }

    /*
    We can't stop in the audio thread so we instead need to use an event. We wait on this thread in the main thread, and signal it in the audio thread. This
    needs to be done before starting the device. We need a context to initialize the event, which we can get from the device. Alternatively you can initialize
    a context separately, but we don't need to do that for this example.
    */
    ma_event_init(&g_stopEvent);

    /* Now we start playback and wait for the audio thread to tell us to stop. */
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        for (iDecoder = 0; iDecoder < g_decoderCount; ++iDecoder) {
            ma_decoder_uninit(&g_pDecoders[iDecoder]);
        }
        free(g_pDecoders);
        free(g_pDecodersAtEnd);

        printf("Failed to start playback device.\n");
        return -4;
    }

    printf("Waiting for playback to complete...\n");
    ma_event_wait(&g_stopEvent);
    
    /* Getting here means the audio thread has signaled that the device should be stopped. */
    ma_device_uninit(&device);
    
    for (iDecoder = 0; iDecoder < g_decoderCount; ++iDecoder) {
        ma_decoder_uninit(&g_pDecoders[iDecoder]);
    }
    free(g_pDecoders);
    free(g_pDecodersAtEnd);

    return 0;
}
