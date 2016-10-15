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




Examples
========

Asynchonous API
---------------
In asynchronous mode, mini_al will request and deliver audio data through callbacks.

```
mal_uint32 on_send_audio_data(mal_device* pDevice, mal_uint32 sampleCount, void* pSamples)
{
    return drwav_read_f32((drwav*)pDevice->pUserData, sampleCount, pSamples);
}

int main()
{
    mal_uint32 channels = 2;
    mal_uint32 sampleRate = 44100;
    mal_uint32 fragmentSizeInFrames = 512;
    mal_uint32 fragmentCount = 2;

    mal_device playbackDevice;
    if (mal_device_init(&playbackDevice, mal_device_type_playback, NULL, mal_format_f32, channels, sampleRate, fragmentSizeInFrames, fragmentCount) != MAL_SUCCESS) {
        return -1;
    }
    
    mal_device_set_send_callback(&playbackDevice, on_send_audio_data);
    mal_device_start(&playbackDevice);

    printf("Press Enter to quit...\n");
    getchar();

    mal_device_uninit(&playbackDevice);
    return 0;
} 
```