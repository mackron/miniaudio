![mini_al](http://dred.io/img/minial_wide.png)

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




Simple Playback Example
=======================

```c
mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    drwav* pWav = (drwav*)pDevice->pUserData;
    if (pWav == NULL) {
        return 0;
    }
    
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
	mal_device_config config;
	config.format = mal_format_f32;
	config.channels = wav.channels;
	config.sampleRate = wav.sampleRate;
	config.bufferSizeInFrames = 0;	// Use default.
	config.periods = 0;				// Use default.
	config.onRecvCallback = NULL;	// Not used for playback.
	config.onSendCallback = on_send_frames_to_device;
	config.onStopCallback = NULL;
    config.onLogCallback  = NULL;
	
    mal_device device;
    if (mal_device_init(&device, mal_device_type_playback, NULL, &config, &wav) != MAL_SUCCESS) {
        printf("Failed to open playback device.");
        drwav_uninit(&wav);
        return -3;
    }
    mal_device_start(&device);
    
    printf("Press Enter to quit...");
    getchar();
    
    mal_device_uninit(&device);
    drwav_uninit(&wav);
    
    return 0;
}
```