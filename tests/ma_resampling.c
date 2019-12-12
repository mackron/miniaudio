
#define MA_NO_SSE2
#define MA_NO_AVX2

#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

// There is a usage pattern for resampling that miniaudio does not properly support which is where the client continuously
// reads samples until ma_src_read() returns 0. The problem with this pattern is that is consumes the samples sitting
// in the window which are needed to compute the next samples in future calls to ma_src_read() (assuming the client
// has re-filled the resampler's input data).

/*
for (;;) {
    fill_src_input_data(&src, someData);

    float buffer[4096]
    while ((framesRead = ma_src_read(&src, ...) != 0) {
        do_something_with_resampled_data(buffer);
    }
}
*/

// In the use case above, the very last samples that are read from ma_src_read() will not have future samples to draw
// from in order to calculate the correct interpolation factor which in turn results in crackling.

ma_uint32 sampleRateIn  = 0;
ma_uint32 sampleRateOut = 0;
ma_sine_wave sineWave; // <-- This is the source data.
ma_src src;
float srcInput[1024];
ma_uint32 srcNextSampleIndex = ma_countof(srcInput);

void reload_src_input()
{
    ma_sine_wave_read_f32(&sineWave, ma_countof(srcInput), srcInput);
    srcNextSampleIndex = 0;
}

ma_uint32 on_src(ma_src* pSRC, ma_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    ma_assert(pSRC != NULL);
    ma_assert(pSRC->config.channels == 1);

    (void)pUserData;

    // Only read as much as is available in the input buffer. Do not reload the buffer here.
    ma_uint32 framesAvailable = ma_countof(srcInput) - srcNextSampleIndex;
    ma_uint32 framesToRead = frameCount;
    if (framesToRead > framesAvailable) {
        framesToRead = framesAvailable;
    }

    ma_copy_memory(ppSamplesOut[0], srcInput + srcNextSampleIndex, sizeof(float)*framesToRead);
    srcNextSampleIndex += framesToRead;

    return framesToRead;
}

void on_send_to_device(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    (void)pDevice;
    (void)pInput;

    ma_assert(pDevice->playback.format == ma_format_f32);
    ma_assert(pDevice->playback.channels == 1);

    float* pFramesF32 = (float*)pOutput;

    // To reproduce the case we are needing to test, we need to read from the SRC in a very specific way. We keep looping
    // until we've read the requested frame count, however we have an inner loop that keeps running until ma_src_read()
    // returns 0, in which case we need to reload the SRC's input data and keep going.
    ma_uint32 totalFramesRead = 0;
    while (totalFramesRead < frameCount) {
        ma_uint32 framesRemaining = frameCount - totalFramesRead;

        ma_uint32 maxFramesToRead = 128;
        ma_uint32 framesToRead = framesRemaining;
        if (framesToRead > maxFramesToRead) {
            framesToRead = maxFramesToRead;
        }

        ma_uint32 framesRead = (ma_uint32)ma_src_read_deinterleaved(&src, framesToRead, (void**)&pFramesF32, NULL);
        if (framesRead == 0) {
            reload_src_input();
        }

        totalFramesRead += framesRead;
        pFramesF32 += framesRead;
    }

    ma_assert(totalFramesRead == frameCount);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;


    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.dataCallback = on_send_to_device;

    ma_device device;
    ma_result result;

    config.bufferSizeInFrames = 8192*1;

    // We first play the sound the way it's meant to be played.
    result = ma_device_init(NULL, &config, &device);
    if (result != MA_SUCCESS) {
        return -1;
    }


    // For this test, we need the sine wave to be a different format to the device.
    sampleRateOut = device.sampleRate;
    sampleRateIn = (sampleRateOut == 44100) ? 48000 : 44100;
    ma_sine_wave_init(0.2, 400, sampleRateIn, &sineWave);

    ma_src_config srcConfig = ma_src_config_init(sampleRateIn, sampleRateOut, 1, on_src, NULL);
    srcConfig.algorithm = ma_src_algorithm_sinc;
    srcConfig.neverConsumeEndOfInput = MA_TRUE;

    result = ma_src_init(&srcConfig, &src);
    if (result != MA_SUCCESS) {
        printf("Failed to create SRC.\n");
        return -1;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        return -2;
    }

    printf("Press Enter to quit...\n");
    getchar();
    ma_device_uninit(&device);

    return 0;
}
