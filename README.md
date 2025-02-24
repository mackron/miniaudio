<h1 align="center">
    <a href="https://miniaud.io"><img src="https://miniaud.io/img/miniaudio_wide.png" alt="miniaudio" width="1280"></a>
    <br>
</h1>

<h4 align="center">An audio playback and capture library in a single source file.</h4>

<p align="center">
    <a href="https://discord.gg/9vpqbjU"><img src="https://img.shields.io/discord/712952679415939085?label=discord&logo=discord&style=flat-square" alt="discord"></a>
    <a href="https://fosstodon.org/@mackron"><img src="https://img.shields.io/mastodon/follow/109293691403797709?color=blue&domain=https%3A%2F%2Ffosstodon.org&label=mastodon&logo=mastodon&style=flat-square" alt="mastodon"></a>
</p>

<p align="center">
    <a href="#features">Features</a> -
    <a href="#examples">Examples</a> -
    <a href="#building">Building</a> -
    <a href="#documentation">Documentation</a> -
    <a href="#supported-platforms">Supported Platforms</a> -
    <a href="#security">Security</a> -
    <a href="#license">License</a>
</p>

miniaudio is written in C with no dependencies except the standard library and should compile clean on all major
compilers without the need to install any additional development packages. All major desktop and mobile platforms
are supported.


Features
========
- Simple build system with no external dependencies.
- Simple and flexible API.
- Low-level API for direct access to raw audio data.
- High-level API for sound management, mixing, effects and optional 3D spatialization.
- Flexible node graph system for advanced mixing and effect processing.
- Resource management for loading sound files.
- Decoding, with built-in support for WAV, FLAC and MP3, in addition to being able to plug in custom decoders.
- Encoding (WAV only).
- Data conversion.
- Resampling, including custom resamplers.
- Channel mapping.
- Basic generation of waveforms and noise.
- Basic effects and filters.

Refer to the [Programming Manual](https://miniaud.io/docs/manual/) for a more complete description of
available features in miniaudio.


Examples
========

This example shows one way to play a sound using the high level API.

```c
#include "miniaudio.h"

#include <stdio.h>

int main()
{
    ma_result result;
    ma_engine engine;

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        return -1;
    }

    ma_engine_play_sound(&engine, "sound.wav", NULL);

    printf("Press Enter to quit...");
    getchar();

    ma_engine_uninit(&engine);

    return 0;
}
```

This example shows how to decode and play a sound using the low level API.

```c
#include "miniaudio.h"

#include <stdio.h>

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return;
    }

    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, NULL);

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

More examples can be found in the [examples](examples) folder or online here: https://miniaud.io/docs/examples/


Building
========
Just compile miniaudio.c like any other source file and include miniaudio.h like a normal header. There's no need
to install any dependencies. On Windows and macOS there's no need to link to anything. On Linux and BSD just link
to `-lpthread` and `-lm`. On iOS you need to compile as Objective-C. Link to `-ldl` if you get errors about
`dlopen()`, etc.

If you get errors about undefined references to `__sync_val_compare_and_swap_8`, `__atomic_load_8`, etc. you
need to link with `-latomic`.

ABI compatibility is not guaranteed between versions so take care if compiling as a DLL/SO. The suggested way
to integrate miniaudio is by adding it directly to your source tree.

You can also use CMake if that's your preference.


Documentation
=============
Online documentation can be found here: https://miniaud.io/docs/

Documentation can also be found at the top of [miniaudio.h](https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h)
which is always the most up-to-date and authoritative source of information on how to use miniaudio. All other
documentation is generated from this in-code documentation.


Supported Platforms
===================
- Windows
- macOS, iOS
- Linux
- FreeBSD / OpenBSD / NetBSD
- Android
- Raspberry Pi
- Emscripten / HTML5

miniaudio should compile clean on other platforms, but it will not include any support for playback or capture
by default. To support that, you would need to implement a custom backend. You can do this without needing to
modify the miniaudio source code. See the [custom_backend](examples/custom_backend.c) example.

Backends
--------
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
- Custom


Security
========
I deal with all security related issues publicly and transparently, and it can sometimes take a while before I
get a chance to address it. If this is an issue for you, you need to use another library. The fastest way to get
a bug fixed is to submit a pull request, but if this is unpractical for you please post a ticket to the public
GitHub issue tracker.


License
=======
Your choice of either public domain or [MIT No Attribution](https://github.com/aws/mit-0).
