v0.11.21 - 2023-11-15
=====================
* Add new ma_device_notification_type_unlocked notification. This is used on Web and will be fired after the user has performed a gesture and thus unlocked the ability to play audio.
* Web: Fix an error where the buffer size is incorrectly calculated.
* Core Audio: Fix a -Wshadow warning.


v0.11.20 - 2023-11-10
=====================
* Fix a compilation error with iOS.
* Fix an error when dynamically linking libraries when forcing the UWP build on desktop.


v0.11.19 - 2023-11-04
=====================
* Fix a bug where `ma_decoder_init_file()` can incorrectly return successfully.
* Fix a crash when using a node with more than 2 outputs.
* Fix a bug where `ma_standard_sample_rate_11025` uses the incorrect rate.
* Fix a bug in `ma_noise` where only white noise would be generated even when specifying pink or Brownian.
* Fix an SSE related bug when converting from mono streams.
* Documentation fixes.
* Remove the use of some deprecated functions.
* Improvements to runtime linking on Apple platforms.
* Web / Emscripten: Audio will no longer attempt to unlock in response to the "touchstart" event. This addresses an issue with iOS and Safari. This results in a change of behavior if you were previously depending on starting audio when the user's finger first touches the screen. Audio will now only unlock when the user's finger is lifted. See this discussion for details: https://github.com/mackron/miniaudio/issues/759
* Web / Emscripten: Fix an error when using a sample rate of 0 in the device config.


v0.11.18 - 2023-08-07
=====================
* Fix some AIFF compatibility issues.
* Fix an error where the cursor of a Vorbis stream is incorrectly incremented.
* Add support for setting a callback on an `ma_engine` object that get's fired after it processes a chunk of audio. This allows applications to do things such as apply a post-processing effect or output the audio to a file.
* Add `ma_engine_get_volume()`.
* Add `ma_sound_get_time_in_milliseconds()`.
* Decouple `MA_API` and `MA_PRIVATE`. This relaxes applications from needing to define both of them if they're only wanting to redefine one.
* Decoding backends will now have their onInitFile/W and onInitMemory initialization routines used where appropriate if they're defined.
* Increase the accuracy of the linear resampler when setting the ratio with `ma_linear_resampler_set_rate_ratio()`.
* Fix erroneous output with the linear resampler when in/out rates are the same.
* AAudio: Fix an error where the buffer size is not configured correctly which sometimes results in excessively high latency.
* ALSA: Fix a possible error when stopping and restarting a device.
* PulseAudio: Minor changes to stream flags.
* Win32: Fix an error where `CoUninialize()` is being called when the corresponding `CoInitializeEx()` fails.
* Web / Emscripten: Add support for AudioWorklets. This is opt-in and can be enabled by defining `MA_ENABLE_AUDIO_WORKLETS`. You must compile with `-sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -sASYNCIFY` for this to work. Requires at least Emscripten v3.1.32.


v0.11.17 - 2023-05-27
=====================
* Fix compilation errors with MA_USE_STDINT.
* Fix a possible runtime error with Windows 95/98.
* Fix a very minor linting warning in VS2022.
* Add support for AIFF/AIFC decoding.
* Add support for RIFX decoding.
* Work around some bad code generation by Clang.
* Amalgamations of dr_wav, dr_flac, dr_mp3 and c89atomic have been updated so that they're now fully namespaced. This allows each of these libraries to be able to be used alongside miniaudio without any conflicts. In addition, some duplicate code, such as sized type declarations, result codes, etc. has been removed.


v0.11.16 - 2023-05-15
=====================
* Fix a memory leak with `ma_sound_init_copy()`.
* Improve performance of `ma_sound_init_*()` when using the `ASYNC | DECODE` flag combination.


v0.11.15 - 2023-04-30
=====================
* Fix a bug where initialization of a duplex device fails on some backends.
* Fix a bug in ma_gainer where smoothing isn't applied correctly thus resulting in glitching.
* Add support for volume smoothing to sounds when changing the volume with `ma_sound_set_volume()`. To use this, you must configure it via the `volumeSmoothTimeInPCMFrames` member of ma_sound_config and use `ma_sound_init_ex()` to initialize your sound. Smoothing is disabled by default.
* WASAPI: Fix a possible buffer overrun when initializing a device.
* WASAPI: Make device initialization more robust by improving the handling of the querying of the internal data format.


v0.11.14 - 2023-03-29
=====================
* Fix some pedantic warnings when compiling with GCC.
* Fix some crashes with the WAV decoder when loading an invalid file.
* Fix a channel mapping error with PipeWire which results in no audio being output.
* Add support for using `ma_pcm_rb` as a data source.
* Silence some C89 compatibility warnings with Clang.
* The `pBytesRead` parameter of the VFS onRead callback is now pre-initialized to zero.


v0.11.13 - 2023-03-23
=====================
* Fix compilation errors with the C++ build.
* Fix compilation errors when WIN32_LEAN_AND_MEAN is defined.


v0.11.12 - 2023-03-19
=====================
* Fix a bug with data source ranges which resulted in data being read from outside the range.
* Fix a crash due to a race condition in the resource manager.
* Fix a crash with some backends when rerouting the playback side of a duplex device.
* Fix some bugs with initialization of POSIX threads.
* Fix a bug where sounds are not resampled when `MA_SOUND_NO_PITCH` is used.
* Fix a bug where changing the range of a data source would result in no audio being read.
* Fix a bug where asynchronously loaded data sources via the resources manager would reset ranges and loop points.
* Fix some Wimplicit-fallthrough warnings.
* Add support for Windows 95/98.
* Add support for configuring the stack size of resource manager job threads.
* Add support for callback notifications when a sound reaches the end.
* Optimizations to the high level API.
* Remove the old runtime linking system for pthread. The `MA_USE_RUNTIME_LINKING_FOR_PTHREAD` option is no longer used.
* WASAPI: Fix a crash when starting a device while it's in the process of rerouting.
* Windows: Remove the Windows-specific default memcpy(), malloc(), etc.


v0.11.11 - 2022-11-04
=====================
* Silence an unused variable warning.
* Remove references to ccall() from the Empscripten build.
* Improve Android detection.
* WASAPI: Some minor improvements to overrun recovery for capture and duplex modes.


v0.11.10 - 2022-10-20
=====================
* Add support for setting the device notification callback when initializing an engine object.
* Add support for more than 2 outputs to splitter nodes.
* Fix a crash when initializing a channel converter.
* Fix a channel mapping error where weights are calculated incorrectly.
* Fix an unaligned access error.
* Fix logging with the C++ build.
* Fix some undefined behavior errors, including some memset()'s to null pointers of 0 bytes.
* Fix logging of device info for loopback devices.
* WASAPI: Fix an error where 32-bit formats are not properly detected.
* WASAPI: Fix a bug where the device is not drained when stopped.
* WASAPI: Fix an issue with loopback mode that results in waiting indefinitely and the callback never getting fired.
* WASAPI: Add support for the Avrt API to specify the audio thread's latency sensitivity requirements. Use the `deviceConfig.wasapi.usage` configuration option.
* PulseAudio: Pass the requested sample rate, if set, to PulseAudio so that it uses the requested sample rate internally rather than always using miniaudio's resampler.
* PulseAudio: Fix a rare null pointer dereference.
* ALSA: Fix a potential crash on older versions of Linux.
* Core Audio: Fix a very unlikely memory leak.
* Core Audio: Update a deprecated symbol.
* AAudio: Fix an error where the wrong tokens are being used for usage, content types and input preset hints.
* WebAudio: Do some cleanup of the internal global JavaScript object when the last context has been uninitialized.
* Win32: Fix an error when the channel mask reported by Windows is all zero.
* Various documentation fixes.
* Bring dr_wav, dr_flac and dr_mp3 up-to-date with latest versions.


v0.11.9 - 2022-04-20
====================
* Fix some bugs where looping doesn't work with the resource manager.
* Fix a crash when seeking a sound.
* Fix a subtle bug the results in a glitch when looping a decoder when resampling is being applied.
* Fix an issue where chaining streams would not result in a seamless transition.
* Add a new flag called MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_UNKNOWN_LENGTH for use with resource managed data sources. This flag is used as a hint to the resource manager that the length of the data source is unknown and calling ma_data_source_get_length_in_pcm_frames() should be avoided.
* Add support for resetting a resampler. This is useful for resetting the internal timer and clearing the internal cache for when you want to seek the input sound source back to the start.
* Add support for clearing the cache from biquads and low-pass filters.


v0.11.8 - 2022-02-12
====================
* PulseAudio: Work around bugs in PipeWire:
  - PipeWire is returning AUX channels for stereo streams instead of FL/FR. This workaround forces FL/FR for stereo streams.
  - PipeWire will glitch when the buffer size is too small, but still well within reasonable limits. To work around this bug, the default buffer size on PulseAudio backends is now 25ms. You can override this in the device config. This bug does not exist with regular PulseAudio, but the new default buffer size will still apply because I'm not aware of a good way to detect if PipeWire is being used. If anybody has advice on how to detect this, I'm happy to listen!
* DirectSound: Increase the minimum period size from 20ms to 30ms.
* Return `MA_SUCCESS` from `ma_device_start()` and `ma_device_stopped()` if the device is already started or stopped respectively.
* Fix an incorrect assertion in the data converter.
* Fix a compilation error with ARM builds.


v0.11.7 - 2022-02-06
====================
* Fix an error when seeking to the end of a WAV file.
* Fix a memory leak with low-pass, high-pass and band-pass filters.
* Fix some bugs in the FLAC decoder.
* Fix a -Wundef warning


v0.11.6 - 2022-01-22
====================
* WASAPI: Fix a bug where the device is not stopped when an error occurrs when writing to a playback device.
* PulseAudio: Fix a rare crash due to a division by zero.
* The node graph can now be used as a node. This allows node graphs to be connected to other node graphs.
* Fix a crash with high-pass and band-pass filters.
* Fix an audio glitch when mixing engine nodes (ma_sound and ma_sound_group).
* Add some new helper APIs for cursor and length retrieval:
  - ma_data_source_get_cursor_in_seconds()
  - ma_data_source_get_length_in_seconds()
  - ma_sound_get_cursor_in_seconds()
  - ma_sound_get_length_in_seconds()


v0.11.5 - 2022-01-16
====================
* WASAPI: Fix a bug in duplex mode when the capture and playback devices have different native sample rates.
* AAudio: Add support for automatic stream routing.
* iOS: The interruption_began notification now automatically calls `ma_device_stop()`. This allows `ma_device_start()` to work as expected when called from interruption_ended.
* iOS: Fix a bug that results in a deadlock when stopping the device in response to the interruption_begain or interruption_ended notifications.
* Fix a bug with fixed sized callbacks that results in glitches in duplex mode.
* Fix a bug that results in a deadlock when starting a device.
* ma_engine_play_sound_ex() is now publicly visible.
* Add validation to ma_sound_set_pitch() to prevent negative pitches.
* Add validation to resamplers to prevent negative ratios.


---------------------------------------------------------------------------------------------------

v0.11.4 - 2022-01-12
  - AAudio: Add initial support for automatic stream routing.
  - Add support for initializing an encoder from a VFS.
  - Fix a bug when initializing an encoder from a file where the file handle does not get closed in
    the event of an error.

v0.11.3 - 2022-01-07
  - Add a new flag for nodes called MA_NODE_FLAG_SILENT_OUTPUT which tells miniaudio that the
    output of the node should always be treated as silence. This gives miniaudio an optimization
    opportunity by skipping mixing of those nodes. Useful for special nodes that need to have
    their outputs wired up to the graph so they're processed, but don't want the output to
    contribute to the final mix.
  - Add support for fixed sized callbacks. With this change, the data callback will be fired with
    a consistent frame count based on the periodSizeInFrames or periodSizeInMilliseconds config
    variable (depending on which one is used). If the period size is not specified, the backend's
    internal period size will be used. Under the hood this uses an intermediary buffer which
    introduces a small inefficiency. To avoid this you can use the `noFixedSizedCallback` config
    variable and set it to true. This will make the callback equivalent to the way it was before
    this change and will avoid the intermediary buffer, but the data callback could get fired with
    an inconsistent frame count which might cause problems where certain operations need to operate
    on fixed sized chunks.
  - Change the logging system to always process debug log messages. This is useful for allowing
    debug and test builds of applications to output debug information that can later be passed on
    for debugging in miniaudio. To filter out these messages, just filter against the log level
    which will be MA_LOG_LEVEL_DEBUG.
  - Change the wav decoder to pick the closest format to the source file by default if no preferred
    format is specified.
  - Fix a bug where ma_device_get_info() and ma_device_get_name() return an error.
  - Fix a bug where ma_data_source_read_pcm_frames() can return MA_AT_END even when some data has
    been read. MA_AT_END should only be returned when nothing has been read.
  - PulseAudio: Fix some bugs where starting and stopping a device can result in a deadlock.

v0.11.2 - 2021-12-31
  - Add a new device notification system to replace the stop callback. The stop callback is still
    in place, but will be removed in version 0.12. New code should use the notificationCallback
    member in the device config instead of stopCallback.
  - Fix a bug where the stopped notification doesn't get fired.
  - iOS: The IO buffer size is now configured based on the device's configured period size.
  - WebAudio: Optimizations to some JavaScript code.

v0.11.1 - 2021-12-27
  - Result codes are now declared as an enum rather than #defines.
  - Channel positions (MA_CHANNEL_*) are now declared as an enum rather than #defines.
  - Add ma_device_get_info() for retrieving device information from an initialized device.
  - Add ma_device_get_name() for retrieving the name of an initialized device.
  - Add support for setting the directional attenuation factor to sounds and groups.
  - Fix a crash when passing in NULL for the pEngine parameter of ma_engine_init().
  - Fix a bug where the node graph will output silence if a node has zero input connections.
  - Fix a bug in the engine where sounds in front of the listener are too loud.
  - AAudio: Fix an incorrect assert.
  - AAudio: Fix a bug that resulted in exclusive mode always resulting in initialization failure.
  - AAudio: Fix a bug that resulted in a capture device incorrectly being detected as disconnected.
  - OpenSL: Fix an error when initializing a device with a non-NULL device ID.
  - OpenSL: Fix some bugs with device initialization.

v0.11.0 - 2021-12-18
  - Add a node graph system for advanced mixing and effect processing.
  - Add a resource manager for loading and streaming sounds.
  - Add a high level engine API for sound management and mixing. This wraps around the node graph
    and resource manager.
  - Add support for custom resmplers.
  - Add ma_decoder_get_data_format().
  - Add support for disabling denormals on the audio thread.
  - Add a delay/echo effect called ma_delay.
  - Add a stereo pan effect called ma_panner.
  - Add a spataializer effect called ma_spatializer.
  - Add support for amplification for device master volume.
  - Remove dependency on MA_MAX_CHANNELS from filters and data conversion.
  - Increase MA_MAX_CHANNELS from 32 to 254.
  - API CHANGE: Changes have been made to the way custom data sources are made. See documentation
    on how to implement custom data sources.
  - API CHANGE: Remove ma_data_source_map() and ma_data_source_unmap()
  - API CHANGE: Remove the `loop` parameter from ma_data_source_read_pcm_frames(). Use
    ma_data_source_set_looping() to enable or disable looping.
  - API CHANGE: Remove ma_channel_mix_mode_planar_blend. Use ma_channel_mix_mode_rectangular instead.
  - API CHANGE: Remove MA_MIN_SAMPLE_RATE and MA_MAX_SAMPLE_RATE. Use ma_standard_sample_rate_min
    and ma_standard_sample_rate_max instead.
  - API CHANGE: Changes have been made to the ma_device_info structure. See documentation for
    details of these changes.
  - API CHANGE: Remove the `shareMode` parameter from ma_context_get_device_info().
  - API CHANGE: Rename noPreZeroedOutputBuffer to noPreSilencedOutputBuffer in the device config.
  - API CHANGE: Remove pBufferOut parameter from ring buffer commit functions.
  - API CHANGE: Remove ma_zero_pcm_frames(). Use ma_silence_pcm_frames() instead.
  - API CHANGE: Change ma_clip_samples_f32() to take input and output buffers rather than working
    exclusively in-place.
  - API CHANGE: Remove ma_clip_pcm_frames_f32(). Use ma_clip_samples_f32() or ma_clip_pcm_frames()
    instead.
  - API CHANGE: Remove the onLog callback from the context config and replaced with a more
    flexible system. See the documentation for how to use logging.
  - API CHANGE: Remove MA_LOG_LEVEL_VERBOSE and add MA_LOG_LEVEL_DEBUG. Logs using the
    MA_LOG_LEVEL_DEBUG logging level will only be output when miniaudio is compiled with the
    MA_DEBUG_OUTPUT option.
  - API CHANGE: MA_LOG_LEVEL has been removed. All log levels will be posted, except for
    MA_LOG_LEVEL_DEBUG which will only be output when MA_DEBUG_OUTPUT is enabled.
  - API CHANGE: Rename ma_resource_format to ma_encoding_format.
  - API CHANGE: Remove all encoding-specific initialization routines for decoders. Use the
    encodingFormat properties in the decoder config instead.
  - API CHANGE: Change ma_decoder_get_length_in_pcm_frames() to return a result code and output the
    number of frames read via an output parameter.
  - API CHANGE: Allocation callbacks must now implement the onRealloc() callback.
  - API CHANGE: Remove ma_get_standard_channel_map() and add ma_channel_map_init_standard().
  - API CHANGE: Rename ma_channel_map_valid() to ma_channel_map_is_valid().
  - API CHANGE: Rename ma_channel_map_equal() to ma_channel_map_is_equal().
  - API CHANGE: Rename ma_channel_map_blank() to ma_channel_map_is_blank().
  - API CHANGE: Remove the Speex resampler. Use a custom resampler instead.
  - API CHANGE: Change the following resampler APIs to return a result code and output their result
    via an output parameter:
    - ma_linear_resampler_get_required_input_frame_count()
    - ma_linear_resampler_get_expected_output_frame_count()
    - ma_resampler_get_required_input_frame_count()
    - ma_resampler_get_expected_output_frame_count()
  - API CHANGE: Update relevant init/uninit functions to take a pointer to allocation callbacks.
  - API CHANGE: Remove ma_scale_buffer_size()
  - API CHANGE: Update ma_encoder_write_pcm_frames() to return a result code and output the number
    of frames written via an output parameter.
  - API CHANGE: Update ma_noise_read_pcm_frames() to return a result code and output the number of
    frames read via an output parameter.
  - API CHANGE: Update ma_waveform_read_pcm_frames() to return a result code and output the number
    of frames read via an output parameter.
  - API CHANGE: Remove The MA_STATE_* and add ma_device_state_* enums.
  - API CHANGE: Rename ma_factor_to_gain_db() to ma_volume_linear_to_db().
  - API CHANGE: Rename ma_gain_db_to_factor() to ma_volume_db_to_linear().
  - API CHANGE: Rename ma_device_set_master_gain_db() to ma_device_set_master_volume_db().
  - API CHANGE: Rename ma_device_get_master_gain_db() to ma_device_get_master_volume_db()

v0.10.43 - 2021-12-10
  - ALSA: Fix use of uninitialized variables.
  - ALSA: Fix enumeration of devices that support both playback and capture.
  - PulseAudio: Fix a possible division by zero.
  - WebAudio: Fix errors in strict mode.

v0.10.42 - 2021-08-22
  - Fix a possible deadlock when stopping devices.

v0.10.41 - 2021-08-15
  - Core Audio: Fix some deadlock errors.

v0.10.40 - 2021-07-23
  - Fix a bug when converting from stereo to mono.
  - PulseAudio: Fix a glitch when pausing and resuming a device.

v0.10.39 - 2021-07-20
  - Core Audio: Fix a deadlock when the default device is changed.
  - Core Audio: Fix compilation errors on macOS and iOS.
  - PulseAudio: Fix a bug where the stop callback is not fired when a device is unplugged.
  - PulseAudio: Fix a null pointer dereference.

v0.10.38 - 2021-07-14
  - Fix a linking error when MA_DEBUG_OUTPUT is not enabled.
  - Fix an error where ma_log_postv() does not return a value.
  - OpenSL: Fix a bug with setting of stream types and recording presets.

0.10.37 - 2021-07-06
  - Fix a bug with log message formatting.
  - Fix build when compiling with MA_NO_THREADING.
  - Minor updates to channel mapping.

0.10.36 - 2021-07-03
  - Add support for custom decoding backends.
  - Fix some bugs with the Vorbis decoder.
  - PulseAudio: Fix a bug with channel mapping.
  - PulseAudio: Fix a bug where miniaudio does not fall back to a supported format when PulseAudio
    defaults to a format not known to miniaudio.
  - OpenSL: Fix a crash when initializing a capture device when a recording preset other than the
    default is specified.
  - Silence some warnings when compiling with MA_DEBUG_OUTPUT
  - Improvements to logging. See the `ma_log` API for details. The logCallback variable used by
    ma_context has been deprecated and will be replaced with the new system in version 0.11.
    - Initialize an `ma_log` object with `ma_log_init()`.
    - Register a callback with `ma_log_register_callback()`.
    - In the context config, set `pLog` to your `ma_log` object and stop using `logCallback`.
  - Prep work for some upcoming changes to data sources. These changes are still compatible with
    existing code, however code will need to be updated in preparation for version 0.11 which will
    be breaking. You should make these changes now for any custom data sources:
    - Change your base data source object from `ma_data_source_callbacks` to `ma_data_source_base`.
    - Call `ma_data_source_init()` for your base object in your custom data source's initialization
      routine. This takes a config object which includes a pointer to a vtable which is now where
      your custom callbacks are defined.
    - Call `ma_data_source_uninit()` in your custom data source's uninitialization routine. This
      doesn't currently do anything, but it placeholder in case some future uninitialization code
      is required to be added at a later date.

v0.10.35 - 2021-04-27
  - Fix the C++ build.

v0.10.34 - 2021-04-26
  - WASAPI: Fix a bug where a result code is not getting checked at initialization time.
  - WASAPI: Bug fixes for loopback mode.
  - ALSA: Fix a possible deadlock when stopping devices.
  - Mark devices as default on the null backend.

v0.10.33 - 2021-04-04
  - Core Audio: Fix a memory leak.
  - Core Audio: Fix a bug where the performance profile is not being used by playback devices.
  - JACK: Fix loading of 64-bit JACK on Windows.
  - Fix a calculation error and add a safety check to the following APIs to prevent a division by zero:
    - ma_calculate_buffer_size_in_milliseconds_from_frames()
    - ma_calculate_buffer_size_in_frames_from_milliseconds()
  - Fix compilation errors relating to c89atomic.
  - Update FLAC decoder.

v0.10.32 - 2021-02-23
  - WASAPI: Fix a deadlock in exclusive mode.
  - WASAPI: No longer return an error from ma_context_get_device_info() when an exclusive mode format
    cannot be retrieved.
  - WASAPI: Attempt to fix some bugs with device uninitialization.
  - PulseAudio: Yet another refactor, this time to remove the dependency on `pa_threaded_mainloop`.
  - Web Audio: Fix a bug on Chrome and any other browser using the same engine.
  - Web Audio: Automatically start the device on some user input if the device has been started. This
    is to work around Google's policy of not starting audio if no user input has yet been performed.
  - Fix a bug where thread handles are not being freed.
  - Fix some static analysis warnings in FLAC, WAV and MP3 decoders.
  - Fix a warning due to referencing _MSC_VER when it is undefined.
  - Update to latest version of c89atomic.
  - Internal refactoring to migrate over to the new backend callback system for the following backends:
    - PulseAudio
    - ALSA
    - Core Audio
    - AAudio
    - OpenSL|ES
    - OSS
    - audio(4)
    - sndio

v0.10.31 - 2021-01-17
  - Make some functions const correct.
  - Update ma_data_source_read_pcm_frames() to initialize pFramesRead to 0 for safety.
  - Add the MA_ATOMIC annotation for use with variables that should be used atomically and remove unnecessary volatile qualifiers.
  - Add support for enabling only specific backends at compile time. This is the reverse of the pre-existing system. With the new
    system, all backends are first disabled with `MA_ENABLE_ONLY_SPECIFIC_BACKENDS`, which is then followed with `MA_ENABLE_*`. The
    old system where you disable backends with `MA_NO_*` still exists and is still the default.

v0.10.30 - 2021-01-10
  - Fix a crash in ma_audio_buffer_read_pcm_frames().
  - Update spinlock APIs to take a volatile parameter as input.
  - Silence some unused parameter warnings.
  - Fix a warning on GCC when compiling as C++.

v0.10.29 - 2020-12-26
  - Fix some subtle multi-threading bugs on non-x86 platforms.
  - Fix a bug resulting in superfluous memory allocations when enumerating devices.
  - Core Audio: Fix a compilation error when compiling for iOS.

v0.10.28 - 2020-12-16
  - Fix a crash when initializing a POSIX thread.
  - OpenSL|ES: Respect the MA_NO_RUNTIME_LINKING option.

v0.10.27 - 2020-12-04
  - Add support for dynamically configuring some properties of `ma_noise` objects post-initialization.
  - Add support for configuring the channel mixing mode in the device config.
  - Fix a bug with simple channel mixing mode (drop or silence excess channels).
  - Fix some bugs with trying to access uninitialized variables.
  - Fix some errors with stopping devices for synchronous backends where the backend's stop callback would get fired twice.
  - Fix a bug in the decoder due to using an uninitialized variable.
  - Fix some data race errors.

v0.10.26 - 2020-11-24
  - WASAPI: Fix a bug where the exclusive mode format may not be retrieved correctly due to accessing freed memory.
  - Fix a bug with ma_waveform where glitching occurs after changing frequency.
  - Fix compilation with OpenWatcom.
  - Fix compilation with TCC.
  - Fix compilation with Digital Mars.
  - Fix compilation warnings.
  - Remove bitfields from public structures to aid in binding maintenance.

v0.10.25 - 2020-11-15
  - PulseAudio: Fix a bug where the stop callback isn't fired.
  - WebAudio: Fix an error that occurs when Emscripten increases the size of it's heap.
  - Custom Backends: Change the onContextInit and onDeviceInit callbacks to take a parameter which is a pointer to the config that was
    passed into ma_context_init() and ma_device_init(). This replaces the deviceType parameter of onDeviceInit.
  - Fix compilation warnings on older versions of GCC.

v0.10.24 - 2020-11-10
  - Fix a bug where initialization of a backend can fail due to some bad state being set from a prior failed attempt at initializing a
    lower priority backend.

v0.10.23 - 2020-11-09
  - AAudio: Add support for configuring a playback stream's usage.
  - Fix a compilation error when all built-in asynchronous backends are disabled at compile time.
  - Fix compilation errors when compiling as C++.

v0.10.22 - 2020-11-08
  - Add support for custom backends.
  - Add support for detecting default devices during device enumeration and with `ma_context_get_device_info()`.
  - Refactor to the PulseAudio backend. This simplifies the implementation and fixes a capture bug.
  - ALSA: Fix a bug in `ma_context_get_device_info()` where the PCM handle is left open in the event of an error.
  - Core Audio: Further improvements to sample rate selection.
  - Core Audio: Fix some bugs with capture mode.
  - OpenSL: Add support for configuring stream types and recording presets.
  - AAudio: Add support for configuring content types and input presets.
  - Fix bugs in `ma_decoder_init_file*()` where the file handle is not closed after a decoding error.
  - Fix some compilation warnings on GCC and Clang relating to the Speex resampler.
  - Fix a compilation error for the Linux build when the ALSA and JACK backends are both disabled.
  - Fix a compilation error for the BSD build.
  - Fix some compilation errors on older versions of GCC.
  - Add documentation for `MA_NO_RUNTIME_LINKING`.

v0.10.21 - 2020-10-30
  - Add ma_is_backend_enabled() and ma_get_enabled_backends() for retrieving enabled backends at run-time.
  - WASAPI: Fix a copy and paste bug relating to loopback mode.
  - Core Audio: Fix a bug when using multiple contexts.
  - Core Audio: Fix a compilation warning.
  - Core Audio: Improvements to sample rate selection.
  - Core Audio: Improvements to format/channels/rate selection when requesting defaults.
  - Core Audio: Add notes regarding the Apple notarization process.
  - Fix some bugs due to null pointer dereferences.

v0.10.20 - 2020-10-06
  - Fix build errors with UWP.
  - Minor documentation updates.

v0.10.19 - 2020-09-22
  - WASAPI: Return an error when exclusive mode is requested, but the native format is not supported by miniaudio.
  - Fix a bug where ma_decoder_seek_to_pcm_frames() never returns MA_SUCCESS even though it was successful.
  - Store the sample rate in the `ma_lpf` and `ma_hpf` structures.

v0.10.18 - 2020-08-30
  - Fix build errors with VC6.
  - Fix a bug in channel converter for s32 format.
  - Change channel converter configs to use the default channel map instead of a blank channel map when no channel map is specified when initializing the
    config. This fixes an issue where the optimized mono expansion path would never get used.
  - Use a more appropriate default format for FLAC decoders. This will now use ma_format_s16 when the FLAC is encoded as 16-bit.
  - Update FLAC decoder.
  - Update links to point to the new repository location (https://github.com/mackron/miniaudio).

v0.10.17 - 2020-08-28
  - Fix an error where the WAV codec is incorrectly excluded from the build depending on which compile time options are set.
  - Fix a bug in ma_audio_buffer_read_pcm_frames() where it isn't returning the correct number of frames processed.
  - Fix compilation error on Android.
  - Core Audio: Fix a bug with full-duplex mode.
  - Add ma_decoder_get_cursor_in_pcm_frames().
  - Update WAV codec.

v0.10.16 - 2020-08-14
  - WASAPI: Fix a potential crash due to using an uninitialized variable.
  - OpenSL: Enable runtime linking.
  - OpenSL: Fix a multithreading bug when initializing and uninitializing multiple contexts at the same time.
  - iOS: Improvements to device enumeration.
  - Fix a crash in ma_data_source_read_pcm_frames() when the output frame count parameter is NULL.
  - Fix a bug in ma_data_source_read_pcm_frames() where looping doesn't work.
  - Fix some compilation warnings on Windows when both DirectSound and WinMM are disabled.
  - Fix some compilation warnings when no decoders are enabled.
  - Add ma_audio_buffer_get_available_frames().
  - Add ma_decoder_get_available_frames().
  - Add sample rate to ma_data_source_get_data_format().
  - Change volume APIs to take 64-bit frame counts.
  - Updates to documentation.

v0.10.15 - 2020-07-15
  - Fix a bug when converting bit-masked channel maps to miniaudio channel maps. This affects the WASAPI and OpenSL backends.

v0.10.14 - 2020-07-14
  - Fix compilation errors on Android.
  - Fix compilation errors with -march=armv6.
  - Updates to the documentation.

v0.10.13 - 2020-07-11
  - Fix some potential buffer overflow errors with channel maps when channel counts are greater than MA_MAX_CHANNELS.
  - Fix compilation error on Emscripten.
  - Silence some unused function warnings.
  - Increase the default buffer size on the Web Audio backend. This fixes glitching issues on some browsers.
  - Bring FLAC decoder up-to-date with dr_flac.
  - Bring MP3 decoder up-to-date with dr_mp3.

v0.10.12 - 2020-07-04
  - Fix compilation errors on the iOS build.

v0.10.11 - 2020-06-28
  - Fix some bugs with device tracking on Core Audio.
  - Updates to documentation.

v0.10.10 - 2020-06-26
  - Add include guard for the implementation section.
  - Mark ma_device_sink_info_callback() as static.
  - Fix compilation errors with MA_NO_DECODING and MA_NO_ENCODING.
  - Fix compilation errors with MA_NO_DEVICE_IO

v0.10.9 - 2020-06-24
  - Amalgamation of dr_wav, dr_flac and dr_mp3. With this change, including the header section of these libraries before the implementation of miniaudio is no
    longer required. Decoding of WAV, FLAC and MP3 should be supported seamlessly without any additional libraries. Decoders can be excluded from the build
    with the following options:
    - MA_NO_WAV
    - MA_NO_FLAC
    - MA_NO_MP3
    If you get errors about multiple definitions you need to either enable the options above, move the implementation of dr_wav, dr_flac and/or dr_mp3 to before
    the implementation of miniaudio, or update dr_wav, dr_flac and/or dr_mp3.
  - Changes to the internal atomics library. This has been replaced with c89atomic.h which is embedded within this file.
  - Fix a bug when a decoding backend reports configurations outside the limits of miniaudio's decoder abstraction.
  - Fix the UWP build.
  - Fix the Core Audio build.
  - Fix the -std=c89 build on GCC.

v0.10.8 - 2020-06-22
  - Remove dependency on ma_context from mutexes.
  - Change ma_data_source_read_pcm_frames() to return a result code and output the frames read as an output parameter.
  - Change ma_data_source_seek_pcm_frames() to return a result code and output the frames seeked as an output parameter.
  - Change ma_audio_buffer_unmap() to return MA_AT_END when the end has been reached. This should be considered successful.
  - Change playback.pDeviceID and capture.pDeviceID to constant pointers in ma_device_config.
  - Add support for initializing decoders from a virtual file system object. This is achieved via the ma_vfs API and allows the application to customize file
    IO for the loading and reading of raw audio data. Passing in NULL for the VFS will use defaults. New APIs:
    - ma_decoder_init_vfs()
    - ma_decoder_init_vfs_wav()
    - ma_decoder_init_vfs_flac()
    - ma_decoder_init_vfs_mp3()
    - ma_decoder_init_vfs_vorbis()
    - ma_decoder_init_vfs_w()
    - ma_decoder_init_vfs_wav_w()
    - ma_decoder_init_vfs_flac_w()
    - ma_decoder_init_vfs_mp3_w()
    - ma_decoder_init_vfs_vorbis_w()
  - Add support for memory mapping to ma_data_source.
    - ma_data_source_map()
    - ma_data_source_unmap()
  - Add ma_offset_pcm_frames_ptr() and ma_offset_pcm_frames_const_ptr() which can be used for offsetting a pointer by a specified number of PCM frames.
  - Add initial implementation of ma_yield() which is useful for spin locks which will be used in some upcoming work.
  - Add documentation for log levels.
  - The ma_event API has been made public in preparation for some uncoming work.
  - Fix a bug in ma_decoder_seek_to_pcm_frame() where the internal sample rate is not being taken into account for determining the seek location.
  - Fix some bugs with the linear resampler when dynamically changing the sample rate.
  - Fix compilation errors with MA_NO_DEVICE_IO.
  - Fix some warnings with GCC and -std=c89.
  - Fix some formatting warnings with GCC and -Wall and -Wpedantic.
  - Fix some warnings with VC6.
  - Minor optimization to ma_copy_pcm_frames(). This is now a no-op when the input and output buffers are the same.

v0.10.7 - 2020-05-25
  - Fix a compilation error in the C++ build.
  - Silence a warning.

v0.10.6 - 2020-05-24
  - Change ma_clip_samples_f32() and ma_clip_pcm_frames_f32() to take a 64-bit sample/frame count.
  - Change ma_zero_pcm_frames() to clear to 128 for ma_format_u8.
  - Add ma_silence_pcm_frames() which replaces ma_zero_pcm_frames(). ma_zero_pcm_frames() will be removed in version 0.11.
  - Add support for u8, s24 and s32 formats to ma_channel_converter.
  - Add compile-time and run-time version querying.
    - MA_VERSION_MINOR
    - MA_VERSION_MAJOR
    - MA_VERSION_REVISION
    - MA_VERSION_STRING
    - ma_version()
    - ma_version_string()
  - Add ma_audio_buffer for reading raw audio data directly from memory.
  - Fix a bug in shuffle mode in ma_channel_converter.
  - Fix compilation errors in certain configurations for ALSA and PulseAudio.
  - The data callback now initializes the output buffer to 128 when the playback sample format is ma_format_u8.

v0.10.5 - 2020-05-05
  - Change ma_zero_pcm_frames() to take a 64-bit frame count.
  - Add ma_copy_pcm_frames().
  - Add MA_NO_GENERATION build option to exclude the `ma_waveform` and `ma_noise` APIs from the build.
  - Add support for formatted logging to the VC6 build.
  - Fix a crash in the linear resampler when LPF order is 0.
  - Fix compilation errors and warnings with older versions of Visual Studio.
  - Minor documentation updates.

v0.10.4 - 2020-04-12
  - Fix a data conversion bug when converting from the client format to the native device format.

v0.10.3 - 2020-04-07
  - Bring up to date with breaking changes to dr_mp3.
  - Remove MA_NO_STDIO. This was causing compilation errors and the maintenance cost versus practical benefit is no longer worthwhile.
  - Fix a bug with data conversion where it was unnecessarily converting to s16 or f32 and then straight back to the original format.
  - Fix compilation errors and warnings with Visual Studio 2005.
  - ALSA: Disable ALSA's automatic data conversion by default and add configuration options to the device config:
    - alsa.noAutoFormat
    - alsa.noAutoChannels
    - alsa.noAutoResample
  - WASAPI: Add some overrun recovery for ma_device_type_capture devices.

v0.10.2 - 2020-03-22
  - Decorate some APIs with MA_API which were missed in the previous version.
  - Fix a bug in ma_linear_resampler_set_rate() and ma_linear_resampler_set_rate_ratio().

v0.10.1 - 2020-03-17
  - Add MA_API decoration. This can be customized by defining it before including miniaudio.h.
  - Fix a bug where opening a file would return a success code when in fact it failed.
  - Fix compilation errors with Visual Studio 6 and 2003.
  - Fix warnings on macOS.

v0.10.0 - 2020-03-07
  - API CHANGE: Refactor data conversion APIs
    - ma_format_converter has been removed. Use ma_convert_pcm_frames_format() instead.
    - ma_channel_router has been replaced with ma_channel_converter.
    - ma_src has been replaced with ma_resampler
    - ma_pcm_converter has been replaced with ma_data_converter
  - API CHANGE: Add support for custom memory allocation callbacks. The following APIs have been updated to take an extra parameter for the allocation
    callbacks:
    - ma_malloc()
    - ma_realloc()
    - ma_free()
    - ma_aligned_malloc()
    - ma_aligned_free()
    - ma_rb_init() / ma_rb_init_ex()
    - ma_pcm_rb_init() / ma_pcm_rb_init_ex()
  - API CHANGE: Simplify latency specification in device configurations. The bufferSizeInFrames and bufferSizeInMilliseconds parameters have been replaced with
    periodSizeInFrames and periodSizeInMilliseconds respectively. The previous variables defined the size of the entire buffer, whereas the new ones define the
    size of a period. The following APIs have been removed since they are no longer relevant:
    - ma_get_default_buffer_size_in_milliseconds()
    - ma_get_default_buffer_size_in_frames()
  - API CHANGE: ma_device_set_stop_callback() has been removed. If you require a stop callback, you must now set it via the device config just like the data
    callback.
  - API CHANGE: The ma_sine_wave API has been replaced with ma_waveform. The following APIs have been removed:
    - ma_sine_wave_init()
    - ma_sine_wave_read_f32()
    - ma_sine_wave_read_f32_ex()
  - API CHANGE: ma_convert_frames() has been updated to take an extra parameter which is the size of the output buffer in PCM frames. Parameters have also been
    reordered.
  - API CHANGE: ma_convert_frames_ex() has been changed to take a pointer to a ma_data_converter_config object to specify the input and output formats to
    convert between.
  - API CHANGE: ma_calculate_frame_count_after_src() has been renamed to ma_calculate_frame_count_after_resampling().
  - Add support for the following filters:
    - Biquad (ma_biquad)
    - First order low-pass (ma_lpf1)
    - Second order low-pass (ma_lpf2)
    - Low-pass with configurable order (ma_lpf)
    - First order high-pass (ma_hpf1)
    - Second order high-pass (ma_hpf2)
    - High-pass with configurable order (ma_hpf)
    - Second order band-pass (ma_bpf2)
    - Band-pass with configurable order (ma_bpf)
    - Second order peaking EQ (ma_peak2)
    - Second order notching (ma_notch2)
    - Second order low shelf (ma_loshelf2)
    - Second order high shelf (ma_hishelf2)
  - Add waveform generation API (ma_waveform) with support for the following:
    - Sine
    - Square
    - Triangle
    - Sawtooth
  - Add noise generation API (ma_noise) with support for the following:
    - White
    - Pink
    - Brownian
  - Add encoding API (ma_encoder). This only supports outputting to WAV files via dr_wav.
  - Add ma_result_description() which is used to retrieve a human readable description of a given result code.
  - Result codes have been changed. Binding maintainers will need to update their result code constants.
  - More meaningful result codes are now returned when a file fails to open.
  - Internal functions have all been made static where possible.
  - Fix potential crash when ma_device object's are not aligned to MA_SIMD_ALIGNMENT.
  - Fix a bug in ma_decoder_get_length_in_pcm_frames() where it was returning the length based on the internal sample rate rather than the output sample rate.
  - Fix bugs in some backends where the device is not drained properly in ma_device_stop().
  - Improvements to documentation.

v0.9.10 - 2020-01-15
  - Fix compilation errors due to #if/#endif mismatches.
  - WASAPI: Fix a bug where automatic stream routing is being performed for devices that are initialized with an explicit device ID.
  - iOS: Fix a crash on device uninitialization.

v0.9.9 - 2020-01-09
  - Fix compilation errors with MinGW.
  - Fix compilation errors when compiling on Apple platforms.
  - WASAPI: Add support for disabling hardware offloading.
  - WASAPI: Add support for disabling automatic stream routing.
  - Core Audio: Fix bugs in the case where the internal device uses deinterleaved buffers.
  - Core Audio: Add support for controlling the session category (AVAudioSessionCategory) and options (AVAudioSessionCategoryOptions).
  - JACK: Fix bug where incorrect ports are connected.

v0.9.8 - 2019-10-07
  - WASAPI: Fix a potential deadlock when starting a full-duplex device.
  - WASAPI: Enable automatic resampling by default. Disable with config.wasapi.noAutoConvertSRC.
  - Core Audio: Fix bugs with automatic stream routing.
  - Add support for controlling whether or not the content of the output buffer passed in to the data callback is pre-initialized
    to zero. By default it will be initialized to zero, but this can be changed by setting noPreZeroedOutputBuffer in the device
    config. Setting noPreZeroedOutputBuffer to true will leave the contents undefined.
  - Add support for clipping samples after the data callback has returned. This only applies when the playback sample format is
    configured as ma_format_f32. If you are doing clipping yourself, you can disable this overhead by setting noClip to true in
    the device config.
  - Add support for master volume control for devices.
    - Use ma_device_set_master_volume() to set the volume to a factor between 0 and 1, where 0 is silence and 1 is full volume.
    - Use ma_device_set_master_volume_db() to set the volume in decibels where 0 is full volume and < 0 reduces the volume.
  - Fix warnings emitted by GCC when `__inline__` is undefined or defined as nothing.

v0.9.7 - 2019-08-28
  - Add support for loopback mode (WASAPI only).
    - To use this, set the device type to ma_device_type_loopback, and then fill out the capture section of the device config.
    - If you need to capture from a specific output device, set the capture device ID to that of a playback device.
  - Fix a crash when an error is posted in ma_device_init().
  - Fix a compilation error when compiling for ARM architectures.
  - Fix a bug with the audio(4) backend where the device is incorrectly being opened in non-blocking mode.
  - Fix memory leaks in the Core Audio backend.
  - Minor refactoring to the WinMM, ALSA, PulseAudio, OSS, audio(4), sndio and null backends.

v0.9.6 - 2019-08-04
  - Add support for loading decoders using a wchar_t string for file paths.
  - Don't trigger an assert when ma_device_start() is called on a device that is already started. This will now log a warning
    and return MA_INVALID_OPERATION. The same applies for ma_device_stop().
  - Try fixing an issue with PulseAudio taking a long time to start playback.
  - Fix a bug in ma_convert_frames() and ma_convert_frames_ex().
  - Fix memory leaks in the WASAPI backend.
  - Fix a compilation error with Visual Studio 2010.

v0.9.5 - 2019-05-21
  - Add logging to ma_dlopen() and ma_dlsym().
  - Add ma_decoder_get_length_in_pcm_frames().
  - Fix a bug with capture on the OpenSL|ES backend.
  - Fix a bug with the ALSA backend where a device would not restart after being stopped.

v0.9.4 - 2019-05-06
  - Add support for C89. With this change, miniaudio should compile clean with GCC/Clang with "-std=c89 -ansi -pedantic" and
    Microsoft compilers back to VC6. Other compilers should also work, but have not been tested.

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
  - Fix a bug where an incorrect value is returned from mal_device_stop().

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
  - Changed MAL_IMPLEMENTATION to MINI_AL_IMPLEMENTATION for consistency with other libraries. The old
    way is still supported for now, but you should update as it may be removed in the future.
  - API CHANGE: Replace device enumeration APIs. mal_enumerate_devices() has been replaced with
    mal_context_get_devices(). An additional low-level device enumration API has been introduced called
    mal_context_enumerate_devices() which uses a callback to report devices.
  - API CHANGE: Rename mal_get_sample_size_in_bytes() to mal_get_bytes_per_sample() and add
    mal_get_bytes_per_frame().
  - API CHANGE: Replace mal_device_config.preferExclusiveMode with mal_device_config.shareMode.
    - This new config can be set to mal_share_mode_shared (default) or mal_share_mode_exclusive.
  - API CHANGE: Remove excludeNullDevice from mal_context_config.alsa.
  - API CHANGE: Rename MAL_MAX_SAMPLE_SIZE_IN_BYTES to MAL_MAX_PCM_SAMPLE_SIZE_IN_BYTES.
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
  - API CHANGE: SRC and DSP callbacks now take a pointer to a mal_src and mal_dsp object respectively.
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
  - API CHANGE: The mal_context_init() function now takes a pointer to a mal_context_config object for
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