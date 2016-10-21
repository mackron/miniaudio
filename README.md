mini_al - Mini Audio Library
============================
mini_al is a simple library for playing and recording audio. It's focused on simplicity and has
a very small number of APIs. This is not a full-featured audio library, nor will it ever be. It
is intended to be used as a quick and easy way to connect to an audio device and deliver and
capture audio data from speakers and microphones.

C/C++, single file, public domain.


Features
========
- Public domain
- Single file
- A very simple API




Example
=======
mini_al will request and deliver audio data through callbacks.

```c
mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    drwav* pWav = (drwav*)pDevice->pUserData; 
    return (mal_uint32)drwav_read_f32(pWav, frameCount * pDevice->channels, (float*)pSamples) / pDevice->channels;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("No input file.");
        return -1;
    }

    drwav wav;
    if (!drwav_init_file(&wav, argv[1])) {
        printf("Not a valid WAV file.");
        return -2;
    }
    
    // In this example we use the default playback device with a default buffer size and period count.
    mal_device device;
    if (mal_device_init(&device, mal_device_type_playback, NULL, mal_format_f32, wav.channels, wav.sampleRate, 0, 0, NULL) != MAL_SUCCESS) {
        printf("Failed to open playback device.");
        drwav_uninit(&wav);
        return -3;
    }
    
    // The pUserData member of mal_device is reserved for you.
    device.pUserData = &wav;
    
    // This is the callback for sending data to a playback device when it needs more. Make sure
    // it's set before playing the device otherwise you'll end up with silence for the first
    // bunch of frames.
    mal_device_set_send_callback(&device, on_send_frames_to_device);
    mal_device_start(&device);
    
    printf("Press Enter to quit...");
    getchar();
    
    mal_device_uninit(&device);
    drwav_uninit(&wav);
    
    return 0;
}
```