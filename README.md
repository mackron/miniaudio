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


Supported Platforms
===================
- Windows (XP+)
- Linux
- BSD
- Android
- Emscripten / HTML5

macOS and iOS support is coming soon(ish) via Core Audio. Unofficial support is enabled via the OpenAL
and SDL backends, however I have not tested these personally.


Backends
========
- WASAPI
- DirectSound
- WinMM
- ALSA
- OSS
- OpenSL|ES (Android only)
- OpenAL
- SDL
- Null (Silence)



Simple Playback Example
=======================

```c
#define MAL_IMPLEMENTATION
#include "../mini_al.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include <stdio.h>

// This is the function that's used for sending more data to the device for playback.
mal_uint32 on_send_frames_to_device(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    drwav* pWav = (drwav*)pDevice->pUserData;
    if (pWav == NULL) {
        return 0;
    }
    
    return (mal_uint32)drwav_read_s16(pWav, frameCount * pDevice->channels, (mal_int16*)pSamples) / pDevice->channels;
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

    mal_context context;
    if (mal_context_init(NULL, 0, NULL, &context) != MAL_SUCCESS) {
        printf("Failed to initialize context.");
        drwav_uninit(&wav);
        return -3;
    }
    
    mal_device_config config = mal_device_config_init_playback(mal_format_s16, wav.channels, wav.sampleRate, on_send_frames_to_device);
    
    mal_device device;
    if (mal_device_init(&context, mal_device_type_playback, NULL, &config, &wav, &device) != MAL_SUCCESS) {
        printf("Failed to open playback device.");
        mal_context_uninit(&context);
        drwav_uninit(&wav);
        return -4;
    }
    
    mal_device_start(&device);
    
    printf("Press Enter to quit...");
    getchar();
    
    mal_device_uninit(&device);
    mal_context_uninit(&context);
    drwav_uninit(&wav);
    
    return 0;
}
```

mini_al does not directly support loading of audio files like WAV, FLAC, MP3, etc. You may want to
consider the following single file libraries for this:
- [dr_flac](https://github.com/mackron/dr_libs/blob/master/dr_flac.h)
- [dr_wav](https://github.com/mackron/dr_libs/blob/master/dr_wav.h)
- [stb_vorbis](https://github.com/nothings/stb/blob/master/stb_vorbis.c)
- [jar_mod](https://github.com/kd7tck/jar/blob/master/jar_mod.h)
- [jar_xm](https://github.com/kd7tck/jar/blob/master/jar_xm.h)

Copies of these libraries can be found in the "extras" folder. If you know of any other single file
libraries I can add to this list, let me know. Preferably public domain or MIT.
