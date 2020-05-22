<h1 align="center">
    <a href="https://miniaud.io"><img src="https://miniaud.io/img/miniaudio_wide.png" alt="miniaudio" width="1280"></a>
    <br>
</h1>

<h4 align="center">A single file library for audio playback and capture.</h4>

<p align="center">
    <a href="https://discord.gg/9vpqbjU"><img src="https://img.shields.io/discord/712952679415939085?label=discord&logo=discord" alt="discord"></a>
    <a href="https://twitter.com/mackron"><img src="https://img.shields.io/twitter/follow/mackron?style=flat&label=twitter&color=1da1f2&logo=twitter" alt="twitter"></a>
</p>

<p align="center">
    <a href="#features">Features</a> -
    <a href="#supported-platforms">Supported Platforms</a> -
    <a href="#backends">Backends</a> -
    <a href="#building">Building</a> -
    <a href="#examples">Examples</a> -
    <a href="#documentation">Documentation</a> -
    <a href="#unofficial-bindings">Unofficial Bindings</a>
</p>

Features
========
- Your choice of either public domain or [MIT No Attribution](https://github.com/aws/mit-0).
- Entirely contained within a single file for easy integration into your source tree.
- No external dependencies except for the C standard library and backend libraries.
- Written in C and compilable as C++, enabling miniaudio to work on almost all compilers.
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
========
Do the following in one source file:
```c
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```
Then just compile. There's no need to install any dependencies. On Windows and macOS there's no need to link
to anything. On Linux just link to -lpthread, -lm and -ldl. On BSD just link to -lpthread and -lm. On iOS you
need to compile as Objective-C.


Examples
========
This example shows how to decode and play a sound.

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

More examples can be found in the [examples](examples) folder.


Documentation
=============
Documentation can be found at the top of [miniaudio.h](https://raw.githubusercontent.com/dr-soft/miniaudio/master/miniaudio.h)
which is always the most up-to-date and authoritive source of information on how to use miniaudio.



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

