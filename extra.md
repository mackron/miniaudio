Audio playback and capture library. Choice of public domain or MIT-0. See license statements at the end of this file.
miniaudio (formerly mini_al) - v0.9.5 - 2019-05-21

David Reid - davidreidsoftware@gmail.com

MAJOR CHANGES IN VERSION 0.9
============================
Version 0.9 includes major API changes, centered mostly around full-duplex and the rebrand to "miniaudio". Before I go into
detail about the major changes I would like to apologize. I know it's annoying dealing with breaking API changes, but I think
it's best to get these changes out of the way now while the library is still relatively young and unknown.

There's been a lot of refactoring with this release so there's a good chance a few bugs have been introduced. I apologize in
advance for this. You may want to hold off on upgrading for the short term if you're worried. If mini_al v0.8.14 works for
you, and you don't need full-duplex support, you can avoid upgrading (though you won't be getting future bug fixes).


Rebranding to "miniaudio"
-------------------------
The decision was made to rename mini_al to miniaudio. Don't worry, it's the same project. The reason for this is simple:

1) Having the word "audio" in the title makes it immediately clear that the library is related to audio; and
2) I don't like the look of the underscore.

This rebrand has necessitated a change in namespace from "mal" to "ma". I know this is annoying, and I apologize, but it's
better to get this out of the road now rather than later. Also, since there are necessary API changes for full-duplex support
I think it's better to just get the namespace change over and done with at the same time as the full-duplex changes. I'm hoping
this will be the last of the major API changes. Fingers crossed!

The implementation define is now "#define MINIAUDIO_IMPLEMENTATION". You can also use "#define MA_IMPLEMENTATION" if that's
your preference.


Full-Duplex Support
-------------------
The major feature added to version 0.9 is full-duplex. This has necessitated a few API changes.

1) The data callback has now changed. Previously there was one type of callback for playback and another for capture. I wanted
   to avoid a third callback just for full-duplex so the decision was made to break this API and unify the callbacks. Now,
   there is just one callback which is the same for all three modes (playback, capture, duplex). The new callback looks like
   the following:

       void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

   This callback allows you to move data straight out of the input buffer and into the output buffer in full-duplex mode. In
   playback-only mode, pInput will be null. Likewise, pOutput will be null in capture-only mode. The sample count is no longer
   returned from the callback since it's not necessary for miniaudio anymore.

2) The device config needed to change in order to support full-duplex. Full-duplex requires the ability to allow the client
   to choose a different PCM format for the playback and capture sides. The old ma_device_config object simply did not allow
   this and needed to change. With these changes you now specify the device ID, format, channels, channel map and share mode
   on a per-playback and per-capture basis (see example below). The sample rate must be the same for playback and capture.

   Since the device config API has changed I have also decided to take the opportunity to simplify device initialization. Now,
   the device ID, device type and callback user data are set in the config. ma_device_init() is now simplified down to taking
   just the context, device config and a pointer to the device object being initialized. The rationale for this change is that
   it just makes more sense to me that these are set as part of the config like everything else.

   Example device initialization:

       ma_device_config config = ma_device_config_init(ma_device_type_duplex);   // Or ma_device_type_playback or ma_device_type_capture.
       config.playback.pDeviceID = &myPlaybackDeviceID; // Or NULL for the default playback device.
       config.playback.format    = ma_format_f32;
       config.playback.channels  = 2;
       config.capture.pDeviceID  = &myCaptureDeviceID;  // Or NULL for the default capture device.
       config.capture.format     = ma_format_s16;
       config.capture.channels   = 1;
       config.sampleRate         = 44100;
       config.dataCallback       = data_callback;
       config.pUserData          = &myUserData;

       result = ma_device_init(&myContext, &config, &device);
       if (result != MA_SUCCESS) {
           ... handle error ...
       }

   Note that the "onDataCallback" member of ma_device_config has been renamed to "dataCallback". Also, "onStopCallback" has
   been renamed to "stopCallback".

This is the first pass for full-duplex and there is a known bug. You will hear crackling on the following backends when sample
rate conversion is required for the playback device:

  - Core Audio
  - JACK
  - AAudio
  - OpenSL
  - WebAudio

In addition to the above, not all platforms have been absolutely thoroughly tested simply because I lack the hardware for such
thorough testing. If you experience a bug, an issue report on GitHub or an email would be greatly appreciated (and a sample
program that reproduces the issue if possible).


Other API Changes
-----------------
In addition to the above, the following API changes have been made:

- The log callback is no longer passed to ma_context_config_init(). Instead you need to set it manually after initialization.
- The onLogCallback member of ma_context_config has been renamed to "logCallback".
- The log callback now takes a logLevel parameter. The new callback looks like: void log_callback(ma_context* pContext, ma_device* pDevice, ma_uint32 logLevel, const char* message)
  - You can use ma_log_level_to_string() to convert the logLevel to human readable text if you want to log it.
- Some APIs have been renamed:
  - mal_decoder_read()          -> ma_decoder_read_pcm_frames()
  - mal_decoder_seek_to_frame() -> ma_decoder_seek_to_pcm_frame()
  - mal_sine_wave_read()        -> ma_sine_wave_read_f32()
  - mal_sine_wave_read_ex()     -> ma_sine_wave_read_f32_ex()
- Some APIs have been removed:
  - mal_device_get_buffer_size_in_bytes()
  - mal_device_set_recv_callback()
  - mal_device_set_send_callback()
  - mal_src_set_input_sample_rate()
  - mal_src_set_output_sample_rate()
- Error codes have been rearranged. If you're a binding maintainer you will need to update.
- The ma_backend enums have been rearranged to priority order. The rationale for this is to simplify automatic backend selection
  and to make it easier to see the priority. If you're a binding maintainer you will need to update.
- ma_dsp has been renamed to ma_pcm_converter. The rationale for this change is that I'm expecting "ma_dsp" to conflict with
  some future planned high-level APIs.
- For functions that take a pointer/count combo, such as ma_decoder_read_pcm_frames(), the parameter order has changed so that
  the pointer comes before the count. The rationale for this is to keep it consistent with things like memcpy().


Miscellaneous Changes
---------------------
The following miscellaneous changes have also been made.

- The AAudio backend has been added for Android 8 and above. This is Android's new "High-Performance Audio" API. (For the
  record, this is one of the nicest audio APIs out there, just behind the BSD audio APIs).
- The WebAudio backend has been added. This is based on ScriptProcessorNode. This removes the need for SDL.
- The SDL and OpenAL backends have been removed. These were originally implemented to add support for platforms for which miniaudio
  was not explicitly supported. These are no longer needed and have therefore been removed.
- Device initialization now fails if the requested share mode is not supported. If you ask for exclusive mode, you either get an
  exclusive mode device, or an error. The rationale for this change is to give the client more control over how to handle cases
  when the desired shared mode is unavailable.
- A lock-free ring buffer API has been added. There are two varients of this. "ma_rb" operates on bytes, whereas "ma_pcm_rb"
  operates on PCM frames.
- The library is now licensed as a choice of Public Domain (Unlicense) _or_ MIT-0 (No Attribution) which is the same as MIT, but
  removes the attribution requirement. The rationale for this is to support countries that don't recognize public domain.
*/

/*
ABOUT
=====
miniaudio is a single file library for audio playback and capture. It's written in C (compilable as
C++) and released into the public domain.

Supported Backends:
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

Supported Formats:
  - Unsigned 8-bit PCM
  - Signed 16-bit PCM
  - Signed 24-bit PCM (tightly packed)
  - Signed 32-bit PCM
  - IEEE 32-bit floating point PCM


USAGE
=====
miniaudio is a single-file library. To use it, do something like the following in one .c file.
  #define MINIAUDIO_IMPLEMENTATION
  #include "miniaudio.h"

You can then #include this file in other parts of the program as you would with any other header file.

miniaudio uses an asynchronous, callback based API. You initialize a device with a configuration (sample rate,
channel count, etc.) which includes the callback you want to use to handle data transmission to/from the
device. In the callback you either read from a data pointer in the case of playback or write to it in the case
of capture.

Playback Example
----------------
```c
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return;
    }

    ma_decoder_read_pcm_frames(pDecoder, frameCount, pOutput);
}

...

ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.playback.format   = decoder.outputFormat;
config.playback.channels = decoder.outputChannels;
config.sampleRate        = decoder.outputSampleRate;
config.dataCallback      = data_callback;
config.pUserData         = &decoder;

ma_device device;
if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
    ... An error occurred ...
}

ma_device_start(&device);     // The device is sleeping by default so you'll need to start it manually.

...

ma_device_uninit(&device);    // This will stop the device so no need to do that manually.
```

BUILDING
========
miniaudio should Just Work by adding it to your project's source tree. You do not need to download or install
any dependencies. See below for platform-specific details.

If you want to disable a specific backend, #define the appropriate MA_NO_* option before the implementation.

Note that GCC and Clang requires "-msse2", "-mavx2", etc. for SIMD optimizations.


Building for Windows
--------------------
The Windows build should compile clean on all popular compilers without the need to configure any include paths
nor link to any libraries.

Building for macOS and iOS
--------------------------
The macOS build should compile clean without the need to download any dependencies or link to any libraries or
frameworks. The iOS build needs to be compiled as Objective-C (sorry) and will need to link the relevant frameworks
but should Just Work with Xcode.

Building for Linux
------------------
The Linux build only requires linking to -ldl, -lpthread and -lm. You do not need any development packages.

Building for BSD
----------------
The BSD build only requires linking to -ldl, -lpthread and -lm. NetBSD uses audio(4), OpenBSD uses sndio and
FreeBSD uses OSS.

Building for Android
--------------------
AAudio is the highest priority backend on Android. This should work out out of the box without needing any kind of
compiler configuration. Support for AAudio starts with Android 8 which means older versions will fall back to
OpenSL|ES which requires API level 16+.

Building for Emscripten
-----------------------
The Emscripten build emits Web Audio JavaScript directly and should Just Work without any configuration.


NOTES
=====
- This library uses an asynchronous API for delivering and requesting audio data. Each device will have
  it's own worker thread which is managed by the library.
- If ma_device_init() is called with a device that's not aligned to the platform's natural alignment
  boundary (4 bytes on 32-bit, 8 bytes on 64-bit), it will _not_ be thread-safe. The reason for this
  is that it depends on members of ma_device being correctly aligned for atomic assignments.
- Sample data is always native-endian and interleaved. For example, ma_format_s16 means signed 16-bit
  integer samples, interleaved. Let me know if you need non-interleaved and I'll look into it.
- The sndio backend is currently only enabled on OpenBSD builds.
- The audio(4) backend is supported on OpenBSD, but you may need to disable sndiod before you can use it.
- Automatic stream routing is enabled on a per-backend basis. Support is explicitly enabled for WASAPI
  and Core Audio, however other backends such as PulseAudio may naturally support it, though not all have
  been tested.


BACKEND NUANCES
===============

PulseAudio
----------
- If you experience bad glitching/noise on Arch Linux, consider this fix from the Arch wiki:
    https://wiki.archlinux.org/index.php/PulseAudio/Troubleshooting#Glitches,_skips_or_crackling
  Alternatively, consider using a different backend such as ALSA.

Android
-------
- To capture audio on Android, remember to add the RECORD_AUDIO permission to your manifest:
    <uses-permission android:name="android.permission.RECORD_AUDIO" />
- With OpenSL|ES, only a single ma_context can be active at any given time. This is due to a limitation with OpenSL|ES.
- With AAudio, only default devices are enumerated. This is due to AAudio not having an enumeration API (devices are
  enumerated through Java). You can however perform your own device enumeration through Java and then set the ID in the
  ma_device_id structure (ma_device_id.aaudio) and pass it to ma_device_init().
- The backend API will perform resampling where possible. The reason for this as opposed to using miniaudio's built-in
  resampler is to take advantage of any potential device-specific optimizations the driver may implement.

UWP
---
- UWP only supports default playback and capture devices.
- UWP requires the Microphone capability to be enabled in the application's manifest (Package.appxmanifest):
      <Package ...>
          ...
          <Capabilities>
              <DeviceCapability Name="microphone" />
          </Capabilities>
      </Package>

Web Audio / Emscripten
----------------------
- The first time a context is initialized it will create a global object called "mal" whose primary purpose is to act
  as a factory for device objects.
- Currently the Web Audio backend uses ScriptProcessorNode's, but this may need to change later as they've been deprecated.
- Google is implementing a policy in their browsers that prevent automatic media output without first receiving some kind
  of user input. See here for details: https://developers.google.com/web/updates/2017/09/autoplay-policy-changes. Starting
  the device may fail if you try to start playback without first handling some kind of user input.


OPTIONS
=======
Define these options before including this file.

```c
#define MA_NO_WASAPI
  Disables the WASAPI backend.

#define MA_NO_DSOUND
  Disables the DirectSound backend.

#define MA_NO_WINMM
  Disables the WinMM backend.

#define MA_NO_ALSA
  Disables the ALSA backend.

#define MA_NO_PULSEAUDIO
  Disables the PulseAudio backend.

#define MA_NO_JACK
  Disables the JACK backend.

#define MA_NO_COREAUDIO
  Disables the Core Audio backend.

#define MA_NO_SNDIO
  Disables the sndio backend.

#define MA_NO_AUDIO4
  Disables the audio(4) backend.

#define MA_NO_OSS
  Disables the OSS backend.

#define MA_NO_AAUDIO
  Disables the AAudio backend.

#define MA_NO_OPENSL
  Disables the OpenSL|ES backend.

#define MA_NO_WEBAUDIO
  Disables the Web Audio backend.

#define MA_NO_NULL
  Disables the null backend.

#define MA_DEFAULT_PERIODS
  When a period count of 0 is specified when a device is initialized, it will default to this.

#define MA_BASE_BUFFER_SIZE_IN_MILLISECONDS_LOW_LATENCY
#define MA_BASE_BUFFER_SIZE_IN_MILLISECONDS_CONSERVATIVE
  When a buffer size of 0 is specified when a device is initialized it will default to a buffer of this size, depending
  on the chosen performance profile. These can be increased or decreased depending on your specific requirements.

#define MA_NO_DECODING
  Disables the decoding APIs.

#define MA_NO_DEVICE_IO
  Disables playback and recording. This will disable ma_context and ma_device APIs. This is useful if you only want to
  use miniaudio's data conversion and/or decoding APIs. 

#define MA_NO_STDIO
  Disables file IO APIs.

#define MA_NO_SSE2
  Disables SSE2 optimizations.

#define MA_NO_AVX2
  Disables AVX2 optimizations.

#define MA_NO_AVX512
  Disables AVX-512 optimizations.

#define MA_NO_NEON
  Disables NEON optimizations.

#define MA_LOG_LEVEL <Level>
  Sets the logging level. Set level to one of the following:
    MA_LOG_LEVEL_VERBOSE
    MA_LOG_LEVEL_INFO
    MA_LOG_LEVEL_WARNING
    MA_LOG_LEVEL_ERROR

#define MA_DEBUG_OUTPUT
  Enable printf() debug output.

#define MA_COINIT_VALUE
  Windows only. The value to pass to internal calls to CoInitializeEx(). Defaults to COINIT_MULTITHREADED.
```

DEFINITIONS
===========
This section defines common terms used throughout miniaudio. Unfortunately there is often ambiguity in the use of terms
throughout the audio space, so this section is intended to clarify how miniaudio uses each term.

Sample
------
A sample is a single unit of audio data. If the sample format is f32, then one sample is one 32-bit floating point number.

Frame / PCM Frame
-----------------
A frame is a groups of samples equal to the number of channels. For a stereo stream a frame is 2 samples, a mono frame
is 1 sample, a 5.1 surround sound frame is 6 samples, etc. The terms "frame" and "PCM frame" are the same thing in
miniaudio. Note that this is different to a compressed frame. If ever miniaudio needs to refer to a compressed frame, such
as a FLAC frame, it will always clarify what it's referring to with something like "FLAC frame" or whatnot.

Channel
-------
A stream of monaural audio that is emitted from an individual speaker in a speaker system, or received from an individual
microphone in a microphone system. A stereo stream has two channels (a left channel, and a right channel), a 5.1 surround
sound system has 6 channels, etc. Some audio systems refer to a channel as a complex audio stream that's mixed with other
channels to produce the final mix - this is completely different to miniaudio's use of the term "channel" and should not be
confused.

Sample Rate
-----------
The sample rate in miniaudio is always expressed in Hz, such as 44100, 48000, etc. It's the number of PCM frames that are
processed per second.

Formats
-------
Throughout miniaudio you will see references to different sample formats:

    u8  - Unsigned 8-bit integer
    s16 - Signed 16-bit integer
    s24 - Signed 24-bit integer (tightly packed).
    s32 - Signed 32-bit integer
    f32 - 32-bit floating point
