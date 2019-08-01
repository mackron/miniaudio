REVISION HISTORY
================
v0.9.5 - 2019-05-21

  - Add logging to ma_dlopen() and ma_dlsym().
  - Add ma_decoder_get_length_in_pcm_frames().
  - Fix a bug with capture on the OpenSL|ES backend.
  - Fix a bug with the ALSA backend where a device would not restart after being stopped.

v0.9.4 - 2019-05-06

  - Add support for C89. With this change, miniaudio should compile clean with GCC/Clang with "-std=c89 -ansi -pedantic" and
    Microsoft copmilers back to VC6. Other compilers should also work, but have not been tested.

v0.9.3 - 2019-04-19

  - Fix compiler errors on GCC when compiling with -std=c99.

v0.9.2 - 2019-04-08

  - Add support for per-context user data.
  - Fix a potential bug with context configs.
  - Fix some bugs with PulseAudio.

v0.9.1 - 2019-03-17

  - Fix a bug where the output buffer is not getting zeroed out before calling the data callback. This happens when
    the device is running in passthrough mode (not doing any data conversion).
  - Fix an issue where the data callback is getting called too frequently on the WASAPI and DirectSound backends.
  - Fix error on the UWP build.
  - Fix a build error on Apple platforms.

v0.9 - 2019-03-06

  - Rebranded to "miniaudio". All namespaces have been renamed from "mal" to "ma".
  - API CHANGE: ma_device_init() and ma_device_config_init() have changed significantly:
    - The device type, device ID and user data pointer have moved from ma_device_init() to the config.
    - All variations of ma_device_config_init_*() have been removed in favor of just ma_device_config_init().
    - ma_device_config_init() now takes only one parameter which is the device type. All other properties need
      to be set on the returned object directly.
    - The onDataCallback and onStopCallback members of ma_device_config have been renamed to "dataCallback"
      and "stopCallback".
    - The ID of the physical device is now split into two: one for the playback device and the other for the
      capture device. This is required for full-duplex. These are named "pPlaybackDeviceID" and "pCaptureDeviceID".
  - API CHANGE: The data callback has changed. It now uses a unified callback for all device types rather than
    being separate for each. It now takes two pointers - one containing input data and the other output data. This
    design in required for full-duplex. The return value is now void instead of the number of frames written. The
    new callback looks like the following:
        void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
  - API CHANGE: Remove the log callback parameter from ma_context_config_init(). With this change,
    ma_context_config_init() now takes no parameters and the log callback is set via the structure directly. The
    new policy for config initialization is that only mandatory settings are passed in to *_config_init(). The
    "onLog" member of ma_context_config has been renamed to "logCallback".
  - API CHANGE: Remove ma_device_get_buffer_size_in_bytes().
  - API CHANGE: Rename decoding APIs to "pcm_frames" convention.
    - mal_decoder_read()          -> ma_decoder_read_pcm_frames()
    - mal_decoder_seek_to_frame() -> ma_decoder_seek_to_pcm_frame()
  - API CHANGE: Rename sine wave reading APIs to f32 convention.
    - mal_sine_wave_read()    -> ma_sine_wave_read_f32()
    - mal_sine_wave_read_ex() -> ma_sine_wave_read_f32_ex()
  - API CHANGE: Remove some deprecated APIs
    - mal_device_set_recv_callback()
    - mal_device_set_send_callback()
    - mal_src_set_input_sample_rate()
    - mal_src_set_output_sample_rate()
  - API CHANGE: Add log level to the log callback. New signature:
    - void on_log(ma_context* pContext, ma_device* pDevice, ma_uint32 logLevel, const char* message)
  - API CHANGE: Changes to result codes. Constants have changed and unused codes have been removed. If you're
    a binding mainainer you will need to update your result code constants.
  - API CHANGE: Change the order of the ma_backend enums to priority order. If you are a binding maintainer, you
    will need to update.
  - API CHANGE: Rename mal_dsp to ma_pcm_converter. All functions have been renamed from mal_dsp_*() to
    ma_pcm_converter_*(). All structures have been renamed from mal_dsp* to ma_pcm_converter*.
  - API CHANGE: Reorder parameters of ma_decoder_read_pcm_frames() to be consistent with the new parameter order scheme.
  - The resampling algorithm has been changed from sinc to linear. The rationale for this is that the sinc implementation
    is too inefficient right now. This will hopefully be improved at a later date.
  - Device initialization will no longer fall back to shared mode if exclusive mode is requested but is unusable.
    With this change, if you request an device in exclusive mode, but exclusive mode is not supported, it will not
    automatically fall back to shared mode. The client will need to reinitialize the device in shared mode if that's
    what they want.
  - Add ring buffer API. This is ma_rb and ma_pcm_rb, the difference being that ma_rb operates on bytes and
    ma_pcm_rb operates on PCM frames.
  - Add Web Audio backend. This is used when compiling with Emscripten. The SDL backend, which was previously
    used for web support, will be removed in a future version.
  - Add AAudio backend (Android Audio). This is the new priority backend for Android. Support for AAudio starts
    with Android 8. OpenSL|ES is used as a fallback for older versions of Android.
  - Remove OpenAL and SDL backends.
  - Fix a possible deadlock when rapidly stopping the device after it has started.
  - Update documentation.
  - Change licensing to a choice of public domain _or_ MIT-0 (No Attribution).

v0.8.14 - 2018-12-16

  - Core Audio: Fix a bug where the device state is not set correctly after stopping.
  - Add support for custom weights to the channel router.
  - Update decoders to use updated APIs in dr_flac, dr_mp3 and dr_wav.

v0.8.13 - 2018-12-04

  - Core Audio: Fix a bug with channel mapping.
  - Fix a bug with channel routing where the back/left and back/right channels have the wrong weight.

v0.8.12 - 2018-11-27

  - Drop support for SDL 1.2. The Emscripten build now requires "-s USE_SDL=2".
  - Fix a linking error with ALSA.
  - Fix a bug on iOS where the device name is not set correctly.

v0.8.11 - 2018-11-21

  - iOS bug fixes.
  - Minor tweaks to PulseAudio.

v0.8.10 - 2018-10-21

  - Core Audio: Fix a hang when uninitializing a device.
  - Fix a bug where an incorrect value is returned from ma_device_stop().

v0.8.9 - 2018-09-28

  - Fix a bug with the SDL backend where device initialization fails.

v0.8.8 - 2018-09-14

  - Fix Linux build with the ALSA backend.
  - Minor documentation fix.

v0.8.7 - 2018-09-12

  - Fix a bug with UWP detection.

v0.8.6 - 2018-08-26

  - Automatically switch the internal device when the default device is unplugged. Note that this is still in the
    early stages and not all backends handle this the same way. As of this version, this will not detect a default
    device switch when changed from the operating system's audio preferences (unless the backend itself handles
    this automatically). This is not supported in exclusive mode.
  - WASAPI and Core Audio: Add support for stream routing. When the application is using a default device and the
    user switches the default device via the operating system's audio preferences, miniaudio will automatically switch
    the internal device to the new default. This is not supported in exclusive mode.
  - WASAPI: Add support for hardware offloading via IAudioClient2. Only supported on Windows 8 and newer.
  - WASAPI: Add support for low-latency shared mode via IAudioClient3. Only supported on Windows 10 and newer.
  - Add support for compiling the UWP build as C.
  - mal_device_set_recv_callback() and mal_device_set_send_callback() have been deprecated. You must now set this
    when the device is initialized with mal_device_init*(). These will be removed in version 0.9.0.

v0.8.5 - 2018-08-12

  - Add support for specifying the size of a device's buffer in milliseconds. You can still set the buffer size in
    frames if that suits you. When bufferSizeInFrames is 0, bufferSizeInMilliseconds will be used. If both are non-0
    then bufferSizeInFrames will take priority. If both are set to 0 the default buffer size is used.
  - Add support for the audio(4) backend to OpenBSD.
  - Fix a bug with the ALSA backend that was causing problems on Raspberry Pi. This significantly improves the
    Raspberry Pi experience.
  - Fix a bug where an incorrect number of samples is returned from sinc resampling.
  - Add support for setting the value to be passed to internal calls to CoInitializeEx().
  - WASAPI and WinMM: Stop the device when it is unplugged.

v0.8.4 - 2018-08-06

  - Add sndio backend for OpenBSD.
  - Add audio(4) backend for NetBSD.
  - Drop support for the OSS backend on everything except FreeBSD and DragonFly BSD.
  - Formats are now native-endian (were previously little-endian).
  - Mark some APIs as deprecated:
    - mal_src_set_input_sample_rate() and mal_src_set_output_sample_rate() are replaced with mal_src_set_sample_rate().
    - mal_dsp_set_input_sample_rate() and mal_dsp_set_output_sample_rate() are replaced with mal_dsp_set_sample_rate().
  - Fix a bug when capturing using the WASAPI backend.
  - Fix some aliasing issues with resampling, specifically when increasing the sample rate.
  - Fix warnings.

v0.8.3 - 2018-07-15

  - Fix a crackling bug when resampling in capture mode.
  - Core Audio: Fix a bug where capture does not work.
  - ALSA: Fix a bug where the worker thread can get stuck in an infinite loop.
  - PulseAudio: Fix a bug where mal_context_init() succeeds when PulseAudio is unusable.
  - JACK: Fix a bug where mal_context_init() succeeds when JACK is unusable.

v0.8.2 - 2018-07-07

  - Fix a bug on macOS with Core Audio where the internal callback is not called.

v0.8.1 - 2018-07-06

  - Fix compilation errors and warnings.

v0.8 - 2018-07-05

  - Changed MA_IMPLEMENTATION to MINI_AL_IMPLEMENTATION for consistency with other libraries. The old
    way is still supported for now, but you should update as it may be removed in the future.
  - API CHANGE: Replace device enumeration APIs. mal_enumerate_devices() has been replaced with
    mal_context_get_devices(). An additional low-level device enumration API has been introduced called
    mal_context_enumerate_devices() which uses a callback to report devices.
  - API CHANGE: Rename mal_get_sample_size_in_bytes() to mal_get_bytes_per_sample() and add
    mal_get_bytes_per_frame().
  - API CHANGE: Replace mal_device_config.preferExclusiveMode with ma_device_config.shareMode.
    - This new config can be set to mal_share_mode_shared (default) or ma_share_mode_exclusive.
  - API CHANGE: Remove excludeNullDevice from mal_context_config.alsa.
  - API CHANGE: Rename MA_MAX_SAMPLE_SIZE_IN_BYTES to MA_MAX_PCM_SAMPLE_SIZE_IN_BYTES.
  - API CHANGE: Change the default channel mapping to the standard Microsoft mapping.
  - API CHANGE: Remove backend-specific result codes.
  - API CHANGE: Changes to the format conversion APIs (mal_pcm_f32_to_s16(), etc.)
  - Add support for Core Audio (Apple).
  - Add support for PulseAudio.
    - This is the highest priority backend on Linux (higher priority than ALSA) since it is commonly
      installed by default on many of the popular distros and offer's more seamless integration on
      platforms where PulseAudio is used. In addition, if PulseAudio is installed and running (which
      is extremely common), it's better to just use PulseAudio directly rather than going through the
      "pulse" ALSA plugin (which is what the "default" ALSA device is likely set to).
  - Add support for JACK.
  - Remove dependency on asound.h for the ALSA backend. This means the ALSA development packages are no
    longer required to build miniaudio.
  - Remove dependency on dsound.h for the DirectSound backend. This fixes build issues with some
    distributions of MinGW.
  - Remove dependency on audioclient.h for the WASAPI backend. This fixes build issues with some
    distributions of MinGW.
  - Add support for dithering to format conversion.
  - Add support for configuring the priority of the worker thread.
  - Add a sine wave generator.
  - Improve efficiency of sample rate conversion.
  - Introduce the notion of standard channel maps. Use mal_get_standard_channel_map().
  - Introduce the notion of default device configurations. A default config uses the same configuration
    as the backend's internal device, and as such results in a pass-through data transmission pipeline.
  - Add support for passing in NULL for the device config in mal_device_init(), which uses a default
    config. This requires manually calling mal_device_set_send/recv_callback().
  - Add support for decoding from raw PCM data (mal_decoder_init_raw(), etc.)
  - Make mal_device_init_ex() more robust.
  - Make some APIs more const-correct.
  - Fix errors with SDL detection on Apple platforms.
  - Fix errors with OpenAL detection.
  - Fix some memory leaks.
  - Fix a bug with opening decoders from memory.
  - Early work on SSE2, AVX2 and NEON optimizations.
  - Miscellaneous bug fixes.
  - Documentation updates.

v0.7 - 2018-02-25

  - API CHANGE: Change mal_src_read_frames() and mal_dsp_read_frames() to use 64-bit sample counts.
  - Add decoder APIs for loading WAV, FLAC, Vorbis and MP3 files.
  - Allow opening of devices without a context.
    - In this case the context is created and managed internally by the device.
  - Change the default channel mapping to the same as that used by FLAC.
  - Fix build errors with macOS.

v0.6c - 2018-02-12

  - Fix build errors with BSD/OSS.

v0.6b - 2018-02-03

  - Fix some warnings when compiling with Visual C++.

v0.6a - 2018-01-26

  - Fix errors with channel mixing when increasing the channel count.
  - Improvements to the build system for the OpenAL backend.
  - Documentation fixes.

v0.6 - 2017-12-08

  - API CHANGE: Expose and improve mutex APIs. If you were using the mutex APIs before this version you'll
    need to update.
  - API CHANGE: SRC and DSP callbacks now take a pointer to a mal_src and ma_dsp object respectively.
  - API CHANGE: Improvements to event and thread APIs. These changes make these APIs more consistent.
  - Add support for SDL and Emscripten.
  - Simplify the build system further for when development packages for various backends are not installed.
    With this change, when the compiler supports __has_include, backends without the relevant development
    packages installed will be ignored. This fixes the build for old versions of MinGW.
  - Fixes to the Android build.
  - Add mal_convert_frames(). This is a high-level helper API for performing a one-time, bulk conversion of
    audio data to a different format.
  - Improvements to f32 -> u8/s16/s24/s32 conversion routines.
  - Fix a bug where the wrong value is returned from mal_device_start() for the OpenSL backend.
  - Fixes and improvements for Raspberry Pi.
  - Warning fixes.

v0.5 - 2017-11-11

  - API CHANGE: The mal_context_init() function now takes a pointer to a ma_context_config object for
    configuring the context. The works in the same kind of way as the device config. The rationale for this
    change is to give applications better control over context-level properties, add support for backend-
    specific configurations, and support extensibility without breaking the API.
  - API CHANGE: The alsa.preferPlugHW device config variable has been removed since it's not really useful for
    anything anymore.
  - ALSA: By default, device enumeration will now only enumerate over unique card/device pairs. Applications
    can enable verbose device enumeration by setting the alsa.useVerboseDeviceEnumeration context config
    variable.
  - ALSA: When opening a device in shared mode (the default), the dmix/dsnoop plugin will be prioritized. If
    this fails it will fall back to the hw plugin. With this change the preferExclusiveMode config is now
    honored. Note that this does not happen when alsa.useVerboseDeviceEnumeration is set to true (see above)
    which is by design.
  - ALSA: Add support for excluding the "null" device using the alsa.excludeNullDevice context config variable.
  - ALSA: Fix a bug with channel mapping which causes an assertion to fail.
  - Fix errors with enumeration when pInfo is set to NULL.
  - OSS: Fix a bug when starting a device when the client sends 0 samples for the initial buffer fill.

v0.4 - 2017-11-05

  - API CHANGE: The log callback is now per-context rather than per-device and as is thus now passed to
    mal_context_init(). The rationale for this change is that it allows applications to capture diagnostic
    messages at the context level. Previously this was only available at the device level.
  - API CHANGE: The device config passed to mal_device_init() is now const.
  - Added support for OSS which enables support on BSD platforms.
  - Added support for WinMM (waveOut/waveIn).
  - Added support for UWP (Universal Windows Platform) applications. Currently C++ only.
  - Added support for exclusive mode for selected backends. Currently supported on WASAPI.
  - POSIX builds no longer require explicit linking to libpthread (-lpthread).
  - ALSA: Explicit linking to libasound (-lasound) is no longer required.
  - ALSA: Latency improvements.
  - ALSA: Use MMAP mode where available. This can be disabled with the alsa.noMMap config.
  - ALSA: Use "hw" devices instead of "plughw" devices by default. This can be disabled with the
    alsa.preferPlugHW config.
  - WASAPI is now the highest priority backend on Windows platforms.
  - Fixed an error with sample rate conversion which was causing crackling when capturing.
  - Improved error handling.
  - Improved compiler support.
  - Miscellaneous bug fixes.

v0.3 - 2017-06-19

  - API CHANGE: Introduced the notion of a context. The context is the highest level object and is required for
    enumerating and creating devices. Now, applications must first create a context, and then use that to
    enumerate and create devices. The reason for this change is to ensure device enumeration and creation is
    tied to the same backend. In addition, some backends are better suited to this design.
  - API CHANGE: Removed the rewinding APIs because they're too inconsistent across the different backends, hard
    to test and maintain, and just generally unreliable.
  - Added helper APIs for initializing mal_device_config objects.
  - Null Backend: Fixed a crash when recording.
  - Fixed build for UWP.
  - Added support for f32 formats to the OpenSL|ES backend.
  - Added initial implementation of the WASAPI backend.
  - Added initial implementation of the OpenAL backend.
  - Added support for low quality linear sample rate conversion.
  - Added early support for basic channel mapping.

v0.2 - 2016-10-28

  - API CHANGE: Add user data pointer as the last parameter to mal_device_init(). The rationale for this
    change is to ensure the logging callback has access to the user data during initialization.
  - API CHANGE: Have device configuration properties be passed to mal_device_init() via a structure. Rationale:
    1) The number of parameters is just getting too much.
    2) It makes it a bit easier to add new configuration properties in the future. In particular, there's a
       chance there will be support added for backend-specific properties.
  - Dropped support for f64, A-law and Mu-law formats since they just aren't common enough to justify the
    added maintenance cost.
  - DirectSound: Increased the default buffer size for capture devices.
  - Added initial implementation of the OpenSL|ES backend.

v0.1 - 2016-10-21

  - Initial versioned release.
