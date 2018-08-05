![mini_al](http://dred.io/img/minial_wide.png)

mini_al is a single file library for audio playback and capture. It's written in C (compilable as C++)
and released into the public domain.


Features
========
- A simple build system.
  - It should Just Work out of the box, without the need to download and install any dependencies.
- A simple API.
- Transparent data structures with direct access to internal data.
- Supports both playback and capture on all backends.
- Automatic data conversion.
  - Sample format conversion, with optional dithering.
  - Sample rate conversion.
  - Channel mapping and channel conversion (stereo to 5.1, etc.)
- MP3, Vorbis, FLAC and WAV decoding.
  - This depends on external single file libraries which can be found in the "extras" folder.


Supported Platforms
===================
- Windows (XP+)
- macOS, iOS
- Linux
- BSD
- Android
- Raspberry Pi
- Emscripten / HTML5


Backends
========
- WASAPI
- DirectSound
- WinMM
- Core Audio (Apple)
- ALSA
- PulseAudio
- JACK
- sndio (OpenBSD)
- audioio (NetBSD)
- OSS (FreeBSD)
- OpenSL|ES (Android only)
- OpenAL
- SDL
- Null (Silence)


Building
======
Do the following in one source file:
```
#define MINI_AL_IMPLEMENTATION
#include "mini_al.h"
```
Then just compile. There's no need to install any dependencies. On Windows and macOS there's no need to link
to anything. On Linux and BSD, just link to -lpthread, -lm and -ldl.


Simple Playback Example
=======================

```c
#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"  // Enables FLAC decoding.
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"   // Enables MP3 decoding.
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"   // Enables WAV decoding.

#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

#include <stdio.h>

// This is the function that's used for sending more data to the device for playback.
mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    mal_decoder* pDecoder = (mal_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return 0;
    }

    return (mal_uint32)mal_decoder_read(pDecoder, frameCount, pSamples);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    mal_decoder decoder;
    mal_result result = mal_decoder_init_file(argv[1], NULL, &decoder);
    if (result != MAL_SUCCESS) {
        return -2;
    }

    mal_device_config config = mal_device_config_init_playback(
        decoder.outputFormat,
        decoder.outputChannels,
        decoder.outputSampleRate,
        on_send_frames_to_device);

    mal_device device;
    if (mal_device_init(NULL, mal_device_type_playback, NULL, &config, &decoder, &device) != MAL_SUCCESS) {
        printf("Failed to open playback device.\n");
        mal_decoder_uninit(&decoder);
        return -3;
    }

    if (mal_device_start(&device) != MAL_SUCCESS) {
        printf("Failed to start playback device.\n");
        mal_device_uninit(&device);
        mal_decoder_uninit(&decoder);
        return -4;
    }

    printf("Press Enter to quit...");
    getchar();

    mal_device_uninit(&device);
    mal_decoder_uninit(&decoder);

    return 0;
}
```


MP3/Vorbis/FLAC/WAV Decoding
============================
mini_al includes a decoding API which supports the following backends:
- FLAC via [dr_flac](https://github.com/mackron/dr_libs/blob/master/dr_flac.h)
- MP3 via [dr_mp3](https://github.com/mackron/dr_libs/blob/master/dr_mp3.h)
- WAV via [dr_wav](https://github.com/mackron/dr_libs/blob/master/dr_wav.h)
- Vorbis via [stb_vorbis](https://github.com/nothings/stb/blob/master/stb_vorbis.c)

Copies of these libraries can be found in the "extras" folder. You may also want to look at the
libraries below, but they are not supported by the mini_al decoder API. If you know of any other
single file libraries I can add to this list, let me know. Preferably public domain or MIT.
- [minimp3](https://github.com/lieff/minimp3)
- [jar_mod](https://github.com/kd7tck/jar/blob/master/jar_mod.h)
- [jar_xm](https://github.com/kd7tck/jar/blob/master/jar_xm.h)

To enable support for a decoding backend, all you need to do is #include the header section of the
relevant backend library before the implementation of mini_al, like so:

```
#include "dr_flac.h"    // Enables FLAC decoding.
#include "dr_mp3.h"     // Enables MP3 decoding.
#include "dr_wav.h"     // Enables WAV decoding.

#define MINI_AL_IMPLEMENTATION
#include "mini_al.h"
```

A decoder can be initialized from a file with `mal_decoder_init_file()`, a block of memory with
`mal_decoder_init_memory()`, or from data delivered via callbacks with `mal_decoder_init()`. Here
is an example for loading a decoder from a file:

```
mal_decoder decoder;
mal_result result = mal_decoder_init_file("MySong.mp3", NULL, &decoder);
if (result != MAL_SUCCESS) {
    return false;   // An error occurred.
}

...

mal_decoder_uninit(&decoder);
```

When initializing a decoder, you can optionally pass in a pointer to a `mal_decoder_config` object
(the `NULL` argument in the example above) which allows you to configure the output format, channel
count, sample rate and channel map:

```
mal_decoder_config config = mal_decoder_config_init(mal_format_f32, 2, 48000);
```

When passing in NULL for this parameter, the output format will be the same as that defined by the
decoding backend.

Data is read from the decoder as PCM frames:

```
mal_uint64 framesRead = mal_decoder_read(pDecoder, framesToRead, pFrames);
```

You can also seek to a specific frame like so:

```
mal_result result = mal_decoder_seek(pDecoder, targetFrame);
if (result != MAL_SUCCESS) {
    return false;   // An error occurred.
}
```

When loading a decoder, mini_al uses a trial and error technique to find the appropriate decoding
backend. This can be unnecessarily inefficient if the type is already known. In this case you can
use the `_wav`, `_mp3`, etc. varients of the aforementioned initialization APIs:

```
mal_decoder_init_wav()
mal_decoder_init_mp3()
mal_decoder_init_memory_wav()
mal_decoder_init_memory_mp3()
mal_decoder_init_file_wav()
mal_decoder_init_file_mp3()
etc.
```

The `mal_decoder_init_file()` API will try using the file extension to determine which decoding
backend to prefer.
