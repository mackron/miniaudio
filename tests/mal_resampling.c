// We're using sigvis for visualizations. This will include miniaudio for us, so no need to include miniaudio in this file.
#define NO_SIGVIS

#define MA_NO_SSE2
#define MA_NO_AVX2

#ifdef NO_SIGVIS
    #define MINIAUDIO_IMPLEMENTATION
    #include "../miniaudio.h"
#else
    #define MINI_SIGVIS_IMPLEMENTATION
    #include "../tools/mini_sigvis/mini_sigvis.h" // <-- Includes miniaudio.
#endif

// There is a usage pattern for resampling that miniaudio does not properly support which is where the client continuously
// reads samples until mal_src_read() returns 0. The problem with this pattern is that is consumes the samples sitting
// in the window which are needed to compute the next samples in future calls to mal_src_read() (assuming the client
// has re-filled the resampler's input data).

/*
for (;;) {
    fill_src_input_data(&src, someData);

    float buffer[4096]
    while ((framesRead = mal_src_read(&src, ...) != 0) {
        do_something_with_resampled_data(buffer);
    }
}
*/

// In the use case above, the very last samples that are read from mal_src_read() will not have future samples to draw
// from in order to calculate the correct interpolation factor which in turn results in crackling.

mal_uint32 sampleRateIn  = 0;
mal_uint32 sampleRateOut = 0;
mal_sine_wave sineWave; // <-- This is the source data.
mal_src src;
float srcInput[1024];
mal_uint32 srcNextSampleIndex = mal_countof(srcInput);

void reload_src_input()
{
    mal_sine_wave_read_f32(&sineWave, mal_countof(srcInput), srcInput);
    srcNextSampleIndex = 0;
}

mal_uint32 on_src(mal_src* pSRC, mal_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    mal_assert(pSRC != NULL);
    mal_assert(pSRC->config.channels == 1);

    (void)pUserData;

    // Only read as much as is available in the input buffer. Do not reload the buffer here.
    mal_uint32 framesAvailable = mal_countof(srcInput) - srcNextSampleIndex;
    mal_uint32 framesToRead = frameCount;
    if (framesToRead > framesAvailable) {
        framesToRead = framesAvailable;
    }

    mal_copy_memory(ppSamplesOut[0], srcInput + srcNextSampleIndex, sizeof(float)*framesToRead);
    srcNextSampleIndex += framesToRead;

    return framesToRead;
}

void on_send_to_device(mal_device* pDevice, void* pOutput, const void* pInput, mal_uint32 frameCount)
{
    (void)pDevice;
    (void)pInput;

    mal_assert(pDevice->playback.format == mal_format_f32);
    mal_assert(pDevice->playback.channels == 1);

    float* pFramesF32 = (float*)pOutput;

    // To reproduce the case we are needing to test, we need to read from the SRC in a very specific way. We keep looping
    // until we've read the requested frame count, however we have an inner loop that keeps running until mal_src_read()
    // returns 0, in which case we need to reload the SRC's input data and keep going.
    mal_uint32 totalFramesRead = 0;
    while (totalFramesRead < frameCount) {
        mal_uint32 framesRemaining = frameCount - totalFramesRead;

        mal_uint32 maxFramesToRead = 128;
        mal_uint32 framesToRead = framesRemaining;
        if (framesToRead > maxFramesToRead) {
            framesToRead = maxFramesToRead;
        }

        mal_uint32 framesRead = (mal_uint32)mal_src_read_deinterleaved(&src, framesToRead, (void**)&pFramesF32, NULL);
        if (framesRead == 0) {
            reload_src_input();
        }

        totalFramesRead += framesRead;
        pFramesF32 += framesRead;
    }

    mal_assert(totalFramesRead == frameCount);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;


    mal_device_config config = mal_device_config_init(mal_device_type_playback);
    config.playback.format = mal_format_f32;
    config.playback.channels = 1;
    config.dataCallback = on_send_to_device;

    mal_device device;
    mal_result result;

    config.bufferSizeInFrames = 8192*1;

    // We first play the sound the way it's meant to be played.
    result = mal_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {
        return -1;
    }


    // For this test, we need the sine wave to be a different format to the device.
    sampleRateOut = device.sampleRate;
    sampleRateIn = (sampleRateOut == 44100) ? 48000 : 44100;
    mal_sine_wave_init(0.2, 400, sampleRateIn, &sineWave);

    mal_src_config srcConfig = mal_src_config_init(sampleRateIn, sampleRateOut, 1, on_src, NULL);
    srcConfig.algorithm = mal_src_algorithm_sinc;
    srcConfig.neverConsumeEndOfInput = MA_TRUE;

    result = mal_src_init(&srcConfig, &src);
    if (result != MA_SUCCESS) {
        printf("Failed to create SRC.\n");
        return -1;
    }



#ifndef NO_SIGVIS
    msigvis_context sigvis;
    result = msigvis_init(&sigvis);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize mini_sigvis context.\n");
        return -1;
    }

    msigvis_screen screen;
    result = msigvis_screen_init(&sigvis, 1280, 720, &screen);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize mini_sigvis screen.\n");
        return -2;
    }
    
    msigvis_screen_show(&screen);


    msigvis_channel channelSineWave;
    result = msigvis_channel_init(&sigvis, mal_format_f32, sampleRateOut, &channelSineWave);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize mini_sigvis channel.\n");
        return -3;
    }

    float testSamples[40960];
    float* pFramesF32 = testSamples;

    // To reproduce the case we are needing to test, we need to read from the SRC in a very specific way. We keep looping
    // until we've read the requested frame count, however we have an inner loop that keeps running until mal_src_read()
    // returns 0, in which case we need to reload the SRC's input data and keep going.
    mal_uint32 totalFramesRead = 0;
    while (totalFramesRead < mal_countof(testSamples)) {
        mal_uint32 maxFramesToRead = 128;
        mal_uint32 framesToRead = mal_countof(testSamples);
        if (framesToRead > maxFramesToRead) {
            framesToRead = maxFramesToRead;
        }

        mal_uint32 framesRead = (mal_uint32)mal_src_read_deinterleaved(&src, framesToRead, (void**)&pFramesF32, NULL);
        if (framesRead == 0) {
            reload_src_input();
        }

        totalFramesRead += framesRead;
        pFramesF32 += framesRead;
    }

    msigvis_channel_push_samples(&channelSineWave, mal_countof(testSamples), testSamples);
    msigvis_screen_add_channel(&screen, &channelSineWave);


    int exitCode = msigvis_run(&sigvis);

    msigvis_screen_uninit(&screen);
    msigvis_uninit(&sigvis);
#else

    result = mal_device_start(&device);
    if (result != MA_SUCCESS) {
        return -2;
    }

    printf("Press Enter to quit...\n");
    getchar();
    mal_device_uninit(&device);
#endif

    return 0;
}
