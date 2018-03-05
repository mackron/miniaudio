![mini_al](http://dred.io/img/minial_wide.png)

mini_al is a simple library for playing and recording audio. It's focused on simplicity and has
a very small number of APIs.

C/C++, single file, public domain.


Features
========
- Public domain.
- Single file.
- Compilable as both C and C++.
- Easy to build.
  - Does not require linking to anything on the Windows build and only -ldl on Linux.
  - The Windows build compiles clean with the default installations of modern versions of MSVC, GCC
    and Clang. There is no need to download any dependencies, configure include paths nor link to
    any libraries. It should Just Work.
  - The header section does not include any platform specific headers.
- A very simple API.
- Transparent data structures with direct access to internal data.
- Supports both playback and capture on all backends.
- Automatic format conversion.
  - Sample format conversion.
  - Sample rate conversion.
    - Sample rate conversion is currently low quality, but a higher quality implementation is planned.
  - Channel mapping/layout.
  - Channel mixing (converting mono to 5.1, etc.)
- MP3, Vorbis, FLAC and WAV decoding
  - This depends on external single file libraries which can be found in the "extras" folder.


Supported Platforms
===================
- Windows (XP+)
- Linux
- BSD
- Android
- Emscripten / HTML5

macOS and iOS support is coming soon(ish) via Core Audio. Unofficial support is enabled via the
PulseAudio, JACK, OpenAL and SDL backends, however I have not tested these personally.


Backends
========
- WASAPI
- DirectSound
- WinMM
- ALSA
- PulseAudio
- JACK
- OSS
- OpenSL|ES (Android only)
- OpenAL
- SDL
- Null (Silence)



Simple Playback Example
=======================

```c
#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"  // Enables FLAC decoding.
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"   // Enables MP3 decoding.
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"   // Enables WAV decoding.

#define MAL_IMPLEMENTATION
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

    mal_device_config config = mal_device_config_init_playback(decoder.outputFormat, decoder.outputChannels, decoder.outputSampleRate, on_send_frames_to_device);

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

To enable support for a decoder backend, all you need to do is #include the header section of the
relevant backend library before the implementation of mini_al, like so:

```
#include "dr_flac.h"    // Enables FLAC decoding.
#include "dr_mp3.h"     // Enables MP3 decoding.
#include "dr_wav.h"     // Enables WAV decoding.

#define MAL_IMPLEMENTATION
#include "mini_al.h"
```