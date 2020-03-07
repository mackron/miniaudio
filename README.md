![miniaudio](https://miniaud.io/img/miniaudio_wide.png)

miniaudio is a single file library for audio playback and capture. It's written in C (compilable as C++) and
released into the public domain.


Features
========
- Liberally licensed, with your choice of either public domain or MIT No Attribution for those regions who don't
  recognize public domain.
- Everything is implemented in a single file for easy integration into your source tree.
- No external dependencies except for the C standard library and backend APIs.
- Written in C89 and compilable as C++ which should enable miniaudio to work with almost all compilers.
- Supports all major desktop and mobile platforms, with multiple backends for maximum compatibility.
- Supports playback, capture, full-duplex and loopback (WASAPI only).
- Device enumeration for connecting to specific devices, not just defaults.
- Connect to multiple devices at once.
- Shared and exclusive mode on supported backends.
- Backend-specific configuration options.
- Device capability querying.
- Automatic data conversion between your application and the internal device.
- Sample format conversion with optional dithering.
- Channel conversion and channel mapping.
- Resampling with support for multiple algorithms.
  - Simple linear resampling with anti-aliasing.
  - Optional Speex resampling (must opt-in).
- Filters.
  - Biquad
  - Low-pass (first, second and high order)
  - High-pass (first, second and high order)
  - Second order band-pass
  - Second order notch
  - Second order peaking
  - Second order low shelf
  - Second order high shelf
- Waveform generation.
  - Sine
  - Square
  - Triangle
  - Sawtooth
- Noise generation.
  - White
  - Pink
  - Brownian
- Decoding (requires external single-file libraries).
  - WAV via dr_wav
  - FLAC via dr_flac
  - MP3 via dr_mp3
  - Vorbis via stb_vorbis
- Encoding (requires external single-file libraries).
  - WAV via dr_wav
- Lock free ring buffer (single producer, single consumer).


Supported Platforms
===================
- Windows (XP+), UWP
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
- audio(4) (NetBSD and OpenBSD)
- OSS (FreeBSD)
- AAudio (Android 8.0+)
- OpenSL|ES (Android only)
- Web Audio (Emscripten)
- Null (Silence)


Building
======
Do the following in one source file:
```c
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```
Then just compile. There's no need to install any dependencies. On Windows and macOS there's no need to link
to anything. On Linux just link to -lpthread, -lm and -ldl. On BSD just link to -lpthread and -lm.


Simple Playback Example
=======================

```c
#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"  /* Enables FLAC decoding. */
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"   /* Enables MP3 decoding. */
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"   /* Enables WAV decoding. */

#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <stdio.h>

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return;
    }

    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount);

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder decoder;
    ma_device_config deviceConfig;
    ma_device device;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    result = ma_decoder_init_file(argv[1], NULL, &decoder);
    if (result != MA_SUCCESS) {
        return -2;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = decoder.outputFormat;
    deviceConfig.playback.channels = decoder.outputChannels;
    deviceConfig.sampleRate        = decoder.outputSampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &decoder;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        ma_decoder_uninit(&decoder);
        return -3;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
        return -4;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);

    return 0;
}
```


MP3/Vorbis/FLAC/WAV Decoding
============================
miniaudio includes a decoding API which supports the following backends:
- FLAC via [dr_flac](https://github.com/mackron/dr_libs/blob/master/dr_flac.h)
- MP3 via [dr_mp3](https://github.com/mackron/dr_libs/blob/master/dr_mp3.h)
- WAV via [dr_wav](https://github.com/mackron/dr_libs/blob/master/dr_wav.h)
- Vorbis via [stb_vorbis](https://github.com/nothings/stb/blob/master/stb_vorbis.c)

Copies of these libraries can be found in the "extras" folder.

To enable support for a decoding backend, all you need to do is #include the header section of the
relevant backend library before the implementation of miniaudio, like so:

```c
#include "dr_flac.h"    // Enables FLAC decoding.
#include "dr_mp3.h"     // Enables MP3 decoding.
#include "dr_wav.h"     // Enables WAV decoding.

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

A decoder can be initialized from a file with `ma_decoder_init_file()`, a block of memory with
`ma_decoder_init_memory()`, or from data delivered via callbacks with `ma_decoder_init()`. Here
is an example for loading a decoder from a file:

```c
ma_decoder decoder;
ma_result result = ma_decoder_init_file("MySong.mp3", NULL, &decoder);
if (result != MA_SUCCESS) {
    return false;   // An error occurred.
}

...

ma_decoder_uninit(&decoder);
```

When initializing a decoder, you can optionally pass in a pointer to a `ma_decoder_config` object
(the `NULL` argument in the example above) which allows you to configure the output format, channel
count, sample rate and channel map:

```c
ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, 48000);
```

When passing in NULL for this parameter, the output format will be the same as that defined by the
decoding backend.

Data is read from the decoder as PCM frames:

```c
ma_uint64 framesRead = ma_decoder_read_pcm_frames(pDecoder, pFrames, framesToRead);
```

You can also seek to a specific frame like so:

```c
ma_result result = ma_decoder_seek_to_pcm_frame(pDecoder, targetFrame);
if (result != MA_SUCCESS) {
    return false;   // An error occurred.
}
```

When loading a decoder, miniaudio uses a trial and error technique to find the appropriate decoding
backend. This can be unnecessarily inefficient if the type is already known. In this case you can
use the `_wav`, `_mp3`, etc. varients of the aforementioned initialization APIs:

```
ma_decoder_init_wav()
ma_decoder_init_mp3()
ma_decoder_init_memory_wav()
ma_decoder_init_memory_mp3()
ma_decoder_init_file_wav()
ma_decoder_init_file_mp3()
etc.
```

The `ma_decoder_init_file()` API will try using the file extension to determine which decoding
backend to prefer.


Unofficial Bindings
===================
The projects below offer bindings for other languages which you may be interested in. Note that these
are unofficial and are not maintained as part of this repository. If you encounter a binding-specific
bug, please post a bug report to the specific project. If you've written your own bindings let me know
and I'll consider adding it to this list.

Language | Project
---------|--------
Python   | [pyminiaudio](https://github.com/irmen/pyminiaudio)
Go       | [malgo](https://github.com/gen2brain/malgo)

