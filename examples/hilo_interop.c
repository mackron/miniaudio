/*
Demonstrates interop between the high-level and the low-level API.

In this example we are using `ma_device` (the low-level API) to capture data from the microphone
which we then play back through the engine as a sound. We use a ring buffer to act as the data
source for the sound.

This is just a very basic example to show the general idea on how this might be achieved. In
this example a ring buffer is being used as the intermediary data source, but you can use anything
that works best for your situation. So long as the data is captured from the microphone, and then
delivered to the sound (via a data source), you should be good to go.

A more robust example would probably not want to use a ring buffer directly as the data source.
Instead you would probably want to do a custom data source that handles underruns and overruns of
the ring buffer and deals with desyncs between capture and playback. In the future this example
may be updated to make use of a more advanced data source that handles all of this.
*/
#include "../miniaudio.c"

static ma_audio_ring_buffer rb;
static ma_device device;
static ma_engine engine;
static ma_sound sound;  /* The sound will be the playback of the capture side. */

void capture_data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    (void)pFramesOut;
    (void)pDevice;

    /* We need to write to the ring buffer. */
    ma_audio_ring_buffer_write_pcm_frames(&rb, pFramesIn, frameCount, NULL);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_audio_ring_buffer_config ringBufferConfig;

    /*
    The first thing we'll do is set up the capture side. There are two parts to this. The first is
    the device itself, and the other is the ring buffer. It doesn't matter what order we initialize
    these in, so long as the ring buffer is created before the device is started so that the
    callback can be guaranteed to have a valid destination. We'll initialize the device first, and
    then use the format, channels and sample rate to initialize the ring buffer.

    It's important that the sample format of the device is set to f32 because that's what the engine
    uses internally.
    */

    /* Initialize the capture device. */
    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.dataCallback = capture_data_callback;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize capture device.");
        return -1;
    }

    /* Initialize the ring buffer. Make sure the sample rate is set so the engine can resample it if necessary. */
    ringBufferConfig = ma_audio_ring_buffer_config_init(device.capture.format, device.capture.channels, device.sampleRate, device.capture.internalPeriodSizeInFrames * 3);
    
    result = ma_audio_ring_buffer_init(&ringBufferConfig, &rb);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize the ring buffer.");
        return -1;
    }


    /*
    At this point the capture side is set up and we can now set up the playback side. Here we are
    using `ma_engine` and linking the captured data to a sound so it can be manipulated just like
    any other sound in the world.

    Note that we have not yet started the capture device. Since the captured data is tied to a
    sound, we'll link the starting and stopping of the capture device to the starting and stopping
    of the sound.
    */

    /* We'll get the engine up and running before we start the capture device. */
    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize the engine.");
        return -1;
    }

    /*
    We can now create our sound. This is created from a data source, which in this example is a
    ring buffer. The capture side will be writing data into the ring buffer, whereas the sound
    will be reading from it.
    */
    result = ma_sound_init_from_data_source(&engine, &rb, 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize the sound.");
        return -1;
    }

    /* Link the starting of the device and sound together. */
    ma_device_start(&device);
    ma_sound_start(&sound);


    printf("Press Enter to quit...\n");
    getchar();

    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
    ma_device_uninit(&device);
    ma_audio_ring_buffer_uninit(&rb);


    (void)argc;
    (void)argv;
    return 0;
}
