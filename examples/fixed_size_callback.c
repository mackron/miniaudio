/*
The example demonstrates how to implement a fixed sized callback. miniaudio does not have built-in support for
firing the data callback with fixed sized buffers. In order to support this you need to implement a layer that
sits on top of the normal data callback. This example demonstrates one way of doing this.

This example uses a ring buffer to act as the intermediary buffer between the low-level device callback and the
fixed sized callback. You do not need to use a ring buffer here, but it's a good opportunity to demonstrate how
to use miniaudio's ring buffer API. The ring buffer in this example is in global scope for simplicity, but you
can pass it around as user data for the device (device.pUserData).

This only works for output devices, but can be implemented for input devices by simply swapping the direction
of data movement.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>

#define DEVICE_FORMAT           ma_format_f32
#define DEVICE_CHANNELS         1
#define DEVICE_SAMPLE_RATE      48000

#define PCM_FRAME_CHUNK_SIZE    1234    /* <-- Play around with this to control your fixed sized buffer. */

ma_waveform g_sineWave;
ma_pcm_rb g_rb; /* The ring buffer. */

void data_callback_fixed(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    /*
    This callback will have a guaranteed and consistent size for frameCount. In this example we just fill the output buffer with a sine wave. This
    is where you would handle the callback just like normal, only now you can assume frameCount is a fixed size.
    */
    printf("frameCount=%d\n", frameCount);

    ma_waveform_read_pcm_frames(&g_sineWave, pOutput, frameCount);

    /* Unused in this example. */
    (void)pDevice;
    (void)pInput;
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    /*
    This is the device's main data callback. This will handle all of the fixed sized buffer management for you and will call data_callback_fixed()
    for you. You should do all of your normal callback stuff in data_callback_fixed().
    */
    ma_uint32 pcmFramesAvailableInRB;
    ma_uint32 pcmFramesProcessed = 0;
    ma_uint8* pRunningOutput = (ma_uint8*)pOutput;

    MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);

    /*
    The first thing to do is check if there's enough data available in the ring buffer. If so we can read from it. Otherwise we need to keep filling
    the ring buffer until there's enough, making sure we only fill the ring buffer in chunks of PCM_FRAME_CHUNK_SIZE.
    */
    while (pcmFramesProcessed < frameCount) {    /* Keep going until we've filled the output buffer. */
        ma_uint32 framesRemaining = frameCount - pcmFramesProcessed;

        pcmFramesAvailableInRB = ma_pcm_rb_available_read(&g_rb);
        if (pcmFramesAvailableInRB > 0) {
            ma_uint32 framesToRead = (framesRemaining < pcmFramesAvailableInRB) ? framesRemaining : pcmFramesAvailableInRB;
            void* pReadBuffer;

            ma_pcm_rb_acquire_read(&g_rb, &framesToRead, &pReadBuffer);
            {
                memcpy(pRunningOutput, pReadBuffer, framesToRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
            }
            ma_pcm_rb_commit_read(&g_rb, framesToRead, pReadBuffer);

            pRunningOutput += framesToRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels);
            pcmFramesProcessed += framesToRead;
        } else {
            /*
            There's nothing in the buffer. Fill it with more data from the callback. We reset the buffer first so that the read and write pointers
            are reset back to the start so we can fill the ring buffer in chunks of PCM_FRAME_CHUNK_SIZE which is what we initialized it with. Note
            that this is not how you would want to do it in a multi-threaded environment. In this case you would want to seek the write pointer
            forward via the producer thread and the read pointer forward via the consumer thread (this thread).
            */
            ma_uint32 framesToWrite = PCM_FRAME_CHUNK_SIZE;
            void* pWriteBuffer;

            ma_pcm_rb_reset(&g_rb);
            ma_pcm_rb_acquire_write(&g_rb, &framesToWrite, &pWriteBuffer);
            {
                MA_ASSERT(framesToWrite == PCM_FRAME_CHUNK_SIZE);   /* <-- This should always work in this example because we just reset the ring buffer. */
                data_callback_fixed(pDevice, pWriteBuffer, NULL, framesToWrite);
            }
            ma_pcm_rb_commit_write(&g_rb, framesToWrite, pWriteBuffer);
        }
    }

    /* Unused in this example. */
    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_waveform_config waveformConfig;
    ma_device_config deviceConfig;
    ma_device device;

    waveformConfig = ma_waveform_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE, ma_waveform_type_sine, 0.1, 220);
    ma_waveform_init(&waveformConfig, &g_sineWave);

    ma_pcm_rb_init(DEVICE_FORMAT, DEVICE_CHANNELS, PCM_FRAME_CHUNK_SIZE, NULL, NULL, &g_rb);
    
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = NULL;  /* <-- Set this to a pointer to the ring buffer if you don't want it in global scope. */

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        ma_pcm_rb_uninit(&g_rb);
        return -4;
    }

    printf("Device Name: %s\n", device.playback.name);

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_pcm_rb_uninit(&g_rb);
        ma_device_uninit(&device);
        return -5;
    }
    
    printf("Press Enter to quit...\n");
    getchar();

    ma_pcm_rb_uninit(&g_rb);
    ma_device_uninit(&device);
    
    (void)argc;
    (void)argv;
    return 0;
}
