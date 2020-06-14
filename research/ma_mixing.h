
#ifndef miniaudio_mixing_h
#define miniaudio_mixing_h

#ifdef __cplusplus
extern "C" {
#endif

/*
Open Questions:
  - Should the effect chain automatically convert data between effects, or should it require the format to always be compatible with a data converter effect in
    places where it's required?


Effects
=======
The `ma_effect` API is a mid-level API for chaining together effects. This is a wrapper around lower level APIs which you can continue to use by themselves if
this API does not work for you.

Effects can be linked together as a chain, with one input and one output. When processing audio data through an effect, it starts at the top of the chain and
works it's way down.


Usage
-----
To apply the effect to some audio data, do something like the following:

    ```c
    ma_uint64 framesToProcessIn  = availableInputFrameCount;
    ma_uint64 framesToProcessOut = frameCountOut;
    ma_result result = ma_effect_process_pcm_frames(pEffect, pFramesIn, &framesToProcessIn, pFramesOut, &framesToProcessOut);
    if (result != MA_SUCCESS) {
        // Error.
    }

    // At this point framesToProcessIn contains the number of input frames that were consumed and framesToProcessOut contains the number of output frames that
    // were processed.
    ```

Some effects can change the sample rate, which means the number of output frames may be different to the number of input frames consumed. Therefore they both
need to be specified when processing a chunk of audio data.


*/
typedef void ma_effect;

typedef struct ma_effect_base ma_effect_base;
struct ma_effect_base
{
    ma_result (* onProcessPCMFrames)(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
    ma_uint64 (* onGetRequiredInputFrameCount)(ma_effect* pEffect, ma_uint64 outputFrameCount);
    ma_uint64 (* onGetExpectedOutputFrameCount)(ma_effect* pEffect, ma_uint64 inputFrameCount);
    ma_result (* onGetInputDataFormat)(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
    ma_result (* onGetOutputDataFormat)(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
    ma_effect_base* pPrev;
    ma_effect_base* pNext;
};

MA_API ma_result ma_effect_process_pcm_frames(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
MA_API ma_result ma_effect_process_pcm_frames_ex(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut, ma_format formatIn, ma_uint32 channelsIn, ma_format formatOut, ma_uint32 channelsOut);
MA_API ma_uint64 ma_effect_get_required_input_frame_count(ma_effect* pEffect, ma_uint64 outputFrameCount);
MA_API ma_uint64 ma_effect_get_expected_output_frame_count(ma_effect* pEffect, ma_uint64 inputFrameCount);
MA_API ma_result ma_effect_append(ma_effect* pEffect, ma_effect* pParent);
MA_API ma_result ma_effect_prepend(ma_effect* pEffect, ma_effect* pChild);
MA_API ma_result ma_effect_detach(ma_effect* pEffect);
MA_API ma_result ma_effect_get_output_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API ma_result ma_effect_get_input_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);



/*
Mixing
======
Mixing is done via the ma_mixer API. You can use this if you want to mix multiple sources of audio together and play them all at the same time, layered on top
of each other. This is a mid-level procedural API. Do not confuse this with a high-level data-driven API. You do not "attach" and "detach" sounds, but instead
write raw audio data directly into an accumulation buffer procedurally. High-level data-driven APIs will be coming at a later date.

Below are the features of the ma_mixer API:

    * Mixing to and from any data format with seamless conversion when necessary.
      * Initialize the `ma_mixer` object using whatever format is convenient, and then mix audio in any other format with seamless data conversion.
    * Submixing (mix one `ma_mixer` directly into another `ma_mixer`, with volume and effect control).
    * Volume control.
    * Effects (via the `ma_effect` API).
    * Mix directly from raw audio data in addition to `ma_decoder`, `ma_waveform`, `ma_noise`, `ma_pcm_rb` and `ma_rb` objects.

Mixing sounds together is as simple as summing their samples. As samples are summed together they are stored in a buffer called the accumulation buffer. In
order to ensure there is enough precision to store the intermediary results, the accumulation buffer needs to be at a higher bit depth than the sample format
being mixed, with the exception of floating point. Below is a mapping of the sample format and the data type of the accumulation buffer:

    +---------------+------------------------+
    | Sample Format | Accumulation Data Type |
    +---------------+------------------------+
    | ma_format_u8  | ma_int16               |
    | ma_format_s16 | ma_int32               |
    | ma_format_s24 | ma_int64               |
    | ma_format_s32 | ma_int64               |
    | ma_format_f32 | float                  |
    +---------------+------------------------+

The size of the accumulation buffer is fixed and must be specified at initialization time. When you initialize a mixer you need to also specify a sample format
which will be the format of the returned data after mixing. The format is also what's used to determine the bit depth to use for the accumulation buffer and
how to interpret the data contained within it. You must also specify a channel count in order to support interleaved multi-channel data. The sample rate is not
required by the mixer as it only cares about raw sample data.

The mixing process involves three main steps:

    1) Clearing the accumulation buffer to zero
         ma_mixer_begin()

    2) Accumulating all audio sources
         ma_mixer_mix_pcm_frames()
         ma_mixer_mix_data_source()
         ma_mixer_mix_rb()
         ma_mixer_mix_pcm_rb()

    3) Volume, clipping, effects and final output
         ma_mixer_end()

At the beginning of mixing the accumulation buffer will be cleared to zero. When you begin mixing you need to specify the number of PCM frames you want to
output at the end of mixing. If the requested number of output frames exceeds the capacity of the internal accumulation buffer, it will be clamped and returned
back to the caller. An effect can be applied at the end of mixing (after volume and clipping). Effects can do resampling which means the number of input frames
required to generate the requested number of output frames may be different. Therefore, another parameter is required which will receive the input frame count.
When mixing audio sources, you must do so based on the input frame count, not the output frame count (usage examples are in the next section).

After the accumulation buffer has been cleared to zero (the first step), you can start mixing audio data. When you mix audio data you should do so based on the
required number of input frames returned by ma_mixer_begin() or ma_mixer_begin_submix(). You can specify audio data in any data format in which case the data
will be automatically converted to the format required by the accumulation buffer. Input data can be specified in multiple ways:

    - A pointer to raw PCM data
    - A data source (ma_data_source, ma_decoder, ma_audio_buffer, ma_waveform, ma_noise)
    - A ring buffer (ma_rb, ma_pcm_rb)

Once you've finished accumulating all of your audio sources you need to perform a post process step which performs the final volume adjustment, clipping,
effects and copying to the specified output buffer in the format specified when the mixer was initialized. Volume is applied before clipping, which is applied
before the effect, which is done before final output. In between these steps is all of the necessary data conversion, so for performance it's important to be
mindful of where and when data will be converted.

The mixing API in miniaudio supports seamless data conversion at all stages of the mixing pipeline. If you're not mindful about the data formats used by each
of the different stages of the mixing pipeline you may introduce unnecessary inefficiency. For maximum performance you should use a consistent sample format,
channel count and sample rate for as much of the mixing pipeline as possible. As soon as you introduce a different format, the mixing pipeline will perform the
necessary data conversion.



Usage
-----
Initialize a mixer like the following:

    ```c
    ma_mixer_config config = ma_mixer_config_init(ma_format_f32, 2, 1024, NULL);

    ma_mixer mixer;
    result = ma_mixer_init(&config, &mixer);
    if (result != MA_SUCCESS) {
        // An error occurred.
    }
    ```

Before you can initialize a mixer you need to specify it's configuration via a `ma_mixer_config` object. This can be created with `ma_mixer_config_init()`
which requires the mixing format, channel count, size of the intermediary buffer in PCM frames and an optional pointer to a pre-allocated accumulation buffer.
Once you have the configuration set up, you can call `ma_mixer_init()` to initialize the mixer. If you passed in NULL for the pre-allocated accumulation buffer
this will allocate it on the stack for you, using custom allocation callbacks specified in the `allocationCallbacks` member of the mixer config.

Below is an example for mixing two decoders together:

    ```c
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut = desiredOutputFrameCount;
    ma_mixer_begin(&mixer, NULL, &frameCountOut, &frameCountIn);
    {
        // At this point, frameCountIn contains the number of frames we should be mixing in this iteration, whereas frameCountOut contains the number of output
        // frames we'll be outputting in ma_mixer_end().
        ma_mixer_mix_data_source(&mixer, &decoder1, frameCountIn, isLooping1);
        ma_mixer_mix_data_source(&mixer, &decoder2, frameCountIn, isLooping2);
    }
    ma_mixer_end(&mixer, NULL, pFinalMix); // pFinalMix must be large enough to store frameCountOut frames in the mixer's format (specified at initialization time).
    ```

When you want to mix sounds together, you need to specify how many output frames you would like to end up with by the end. This depends on the size of the
accumulation buffer, however, which is of a fixed size. Therefore, the number of output frames you ask for is not necessarily what you'll get. In addition, an
effect can be applied at the end of mixing, and since that may perform resampling, the number of input frames required to generate the desired number of output
frames may differ which means you must also specify a pointer to a variable which will receive the required input frame count. In order to avoid glitching you
should write all of these input frames if they're available.

The ma_mixer API uses a sort of "immediate mode" design. The idea is that you "begin" and "end" mixing. When you begin mixing a number of frames you need to
call `ma_mixer_begin()`. This will initialize the accumulation buffer to zero (silence) in preparation for mixing. Next, you can start mixing audio data which
can be done in several ways, depending on the source of the audio data. In the example above we are using a `ma_decoder` as the input data source. This will
automatically convert the input data to an appropriate format for mixing.

Each call to ma_mixer_mix_*() accumulates from the beginning of the accumulation buffer.

Once all of your input data has been mixed you need to call `ma_mixer_end()`. This is where the data in the accumulation buffer has volume applied, is clipped
and has the effect applied, in that order. Finally, the data is output to the specified buffer in the format specified when the mixer was first initialized,
overwriting anything that was previously contained within the buffer, unless it's a submix in which case it will be mixed with the parent mixer. See section
below for more details.

The mixing API also supports submixing. This is where the final output of one mixer is mixed directly into the accumulation buffer of another mixer. A common
example is a game with a music submix and an effects submix, which are then combined to form the master mix. Example:

    ```c
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut = desiredOutputFrameCount;  // <-- Must be set to the desired number of output frames. Upon returning, will contain the actual number of output frames.
    ma_mixer_begin(&masterMixer, NULL, &frameCountOut, &frameCountIn);
    {
        ma_uint64 submixFrameCountIn;
        ma_uint64 submixFrameCountOut;  // <-- No pre-initialization required for a submix as it's derived from the parent mix's input frame count.

        // Music submix.
        ma_mixer_begin(&musicMixer, &masterMixer, &submixFrameCountIn, &submixFrameCountOut);
        {
            ma_mixer_mix_data_source(&musicMixer, &musicDecoder, submixFrameCountIn, isMusicLooping);
        }
        ma_mixer_end(&musicMixer, &masterMixer, NULL);

        // Effects submix.
        ma_mixer_begin(&effectsMixer, &masterMixer, &submixFrameCountIn, &submixFrameCountOut);
        {
            ma_mixer_mix_data_source(&effectsMixer, &decoder1, frameCountIn, isLooping1);
            ma_mixer_mix_data_source(&effectsMixer, &decoder2, frameCountIn, isLooping2);
        }
        ma_mixer_end(&effectsMixer, &masterMixer, NULL);
    }
    ma_mixer_end(&masterMixer, NULL, pFinalMix); // pFinalMix must be large enough to store frameCountOut frames in the mixer's format (specified at initialization time).
    ```

If you want to use submixing, you need to ensure the accumulation buffers of each mixer is large enough to accomodate each other. That is, the accumulation
buffer of the sub-mixer needs to be large enough to store the required number of input frames returned by the parent call to `ma_mixer_begin()`. If you are not
doing any resampling you can just make the accumulation buffers the same size and you will fine. If you want to submix, you can only call `ma_mixer_begin()`
between the begin and end pairs of the parent mixer, which can be a master mix or another submix.



Implementation Details and Performance Guidelines
-------------------------------------------------
There are two main factors which affect mixing performance: data conversion and data movement. This section will detail the implementation of the ma_mixer API
and hopefully give you a picture on how best to identify and avoid potential performance pitfalls.

TODO: Write me.

Below a summary of some things to keep in mind for high performance mixing:

    * Choose a sample format at compile time and use it for everything. Optimized pipelines will be implemented for ma_format_s16 and ma_format_f32. The most
      common format is ma_format_f32 which will work in almost all cases. If you're building a game, ma_format_s16 may also work. Professional audio work will
      likely require ma_format_f32 for the added precision for authoring work. Do not use ma_format_s24 if you have high performance requirements as it is not
      nicely aligned and thus requires an inefficient conversion to 32-bit.

    * If you're building a game, try to use a consistent sample format, channel count and sample rate for all of your audio files, or at least all of your
      audio files for a specific category (same format for all sfx, same format for all music, same format for all voices, etc.)

    * Be mindful of when you perform resampling. Most desktop platforms output at a sample rate of 48000Hz or 44100Hz. If your input data is, for example,
      22050Hz, consider doing your mixing at 22050Hz, and then doing a final resample to the playback device's output format. In this example, resampling all
      of your data sources to 48000Hz before mixing may be unnecessarily inefficient because it'll need to perform mixing on a greater number of samples.
*/

MA_API size_t ma_get_accumulation_bytes_per_sample(ma_format format);
MA_API size_t ma_get_accumulation_bytes_per_frame(ma_format format, ma_uint32 channels);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint64 accumulationBufferSizeInFrames;
    void* pPreAllocatedAccumulationBuffer;
    ma_allocation_callbacks allocationCallbacks;
    float volume;
} ma_mixer_config;

MA_API ma_mixer_config ma_mixer_config_init(ma_format format, ma_uint32 channels, ma_uint64 accumulationBufferSizeInFrames, void* pPreAllocatedAccumulationBuffer, const ma_allocation_callbacks* pAllocationCallbacks);


typedef struct
{
    ma_format format;   /* This will be the format output by ma_mixer_end(). */
    ma_uint32 channels;
    ma_uint64 accumulationBufferSizeInFrames;
    void* pAccumulationBuffer;  /* In the accumulation format. */
    ma_allocation_callbacks allocationCallbacks;
    ma_bool32 ownsAccumulationBuffer;
    float volume;
    ma_effect* pEffect; /* The effect to apply after mixing input sources. */
    struct
    {
        ma_uint64 frameCountIn;
        ma_uint64 frameCountOut;
        ma_bool32 isInsideBeginEnd;
    } mixingState;
} ma_mixer;

/*
Initialize a mixer.

A mixer is used to mix/layer/blend sounds together.


Parameters
----------
pConfig (in)
    A pointer to the mixer's configuration. Cannot be NULL. See remarks.

pMixer (out)
    A pointer to the mixer object being initialized.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Thread Safety
-------------
Unsafe. You should not be trying to initialize a mixer from one thread, while at the same time trying to use it on another.


Callback Safety
---------------
This is safe to call in the data callback, but do if you do so, keep in mind that if you do not supply a pre-allocated accumulation buffer it will allocate
memory on the heap for you.


Remarks
-------
The mixer can be configured via the `pConfig` argument. The config object is initialized with `ma_mixer_config_init()`. Individual configuration settings can
then be set directly on the structure. Below are the members of the `ma_mixer_config` object.

    format
        The sample format to use for mixing. This is the format that will be output by `ma_mixer_end()`.

    channels
        The channel count to use for mixing. This is the number of channels that will be output by `ma_mixer_end()`.

    accumulationBufferSizeInFrames
        A mixer uses a fixed sized buffer for it's entire life time. This specifies the size in PCM frames of the accumulation buffer. When calling
        `ma_mixer_begin()`, the requested output frame count will be clamped based on the value of this property. You should not use this propertry to
        determine how many frames to mix at a time with `ma_mixer_mix_*()` - use the value returned by `ma_mixer_begin()`.

    pPreAllocatedAccumulationBuffer
        A pointer to a pre-allocated buffer to use for the accumulation buffer. This can be null in which case a buffer will be allocated for you using the
        specified allocation callbacks, if any. You can calculate the size in bytes of the accumulation buffer like so:

            ```c
            sizeInBytes = config.accumulationBufferSizeInFrames * ma_get_accumulation_bytes_per_frame(config.format, config.channels)
            ```

        Note that you should _not_ use `ma_get_bytes_per_frame()` when calculating the size of the buffer because the accumulation buffer requires a higher bit
        depth for accumulation in order to avoid wrapping.

    allocationCallbacks
        Memory allocation callbacks to use for allocating memory for the accumulation buffer. If all callbacks in this object are NULL, `MA_MALLOC()` and
        `MA_FREE()` will be used.

    volume
        The default output volume in linear scale. Defaults to 1. This can be changed after initialization with `ma_mixer_set_volume()`.
*/
MA_API ma_result ma_mixer_init(ma_mixer_config* pConfig, ma_mixer* pMixer);

/*
Uninitializes a mixer.


Parameters:
-----------
pMixer (in)
    A pointer to the mixer being unintialized.


Thread Safety
-------------
Unsafe. You should not be uninitializing a mixer while using it on another thread.


Callback Safety
---------------
If you did not specify a pre-allocated accumulation buffer, this will free it.


Remarks
-------
If you specified a pre-allocated buffer it will be left as-is. Otherwise it will be freed using the allocation callbacks specified in the config when the mixer
was initialized.
*/
MA_API void ma_mixer_uninit(ma_mixer* pMixer);

/*
Marks the beginning of a mix of a specified number of frames.

When you begin mixing, you must specify how many frames you want to mix. You specify the number of output frames you want, and upon returning you will receive
the number of output frames you'll actually get. When an effect is attached, there may be a chance that the number of input frames required to output the given
output frame count differs. The input frame count is also returned, and this is number of frames you must use with the `ma_mixer_mix_*()` APIs, provided that
number of input frames are available to you at mixing time.

Each call to `ma_mixer_begin()` must be matched with a call to `ma_mixer_end()`. In between these you mix audio data using the `ma_mixer_mix_*()` APIs. When
you call `ma_mixer_end()`, the number of frames that are output will be equal to the output frame count. When you call `ma_mixer_mix_*()`, you specify a frame
count based on the input frame count.


Parameters
----------
pMixer (in)
    A pointer to the relevant mixer.

pParentMixer (in, optional)
    A pointer to the parent mixer. Set this to non-NULL if you want the output of `pMixer` to be mixed with `pParentMixer`. Otherwise, if you want to output
    directly to a buffer, set this to NULL. You would set this to NULL for a master mixer, and non-NULL for a submix. See remarks.

pFrameCountOut (in, out)
    On input, specifies the desired number of output frames to mix in this iteration. The requested number of output frames may not be able to fit in the
    internal accumulation buffer which means on output this variable will receive the actual number of output frames. On input, this will be ignored if
    `pParentMixer` is non-NULL because the output frame count of a submix must be compatible with the parent mixer.

pFramesCountIn (out)
    A pointer to the variable that will receive the number of input frames to mix with each call to `ma_mixer_mix_*()`. This will usually always equal the
    output frame count, but will be different if an effect is applied and that effect performs resampling. See remarks.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Thread Safety
-------------
This can be called from any thread so long as you perform your own synchronization against the `pMixer` and `pParentMixer` object.


Callback Safety
---------------
Safe.


Remarks
-------
When you call `ma_mixer_begin()`, you need to specify how many output frames you want. The number of input frames required to generate those output frames can
differ, however. This will only happen if you have an effect attached (see `ma_mixer_set_effect()`) and if one of the effects in the chain performs resampling.
The input frame count will be returned by the `pFrameCountIn` parameter, and this is how many frames should be used when mixing with `ma_mixer_mix_*()`. See
examples below.

The mixer API supports the concept of submixing which is where the output of one mixer is mixed with that of another. A common example from a game:

    Master
        SFX
        Music
        Voices

In the example above, "Master" is the master mix and "SFX", "Music" and "Voices" are submixes. When you call `ma_mixer_begin()` for the "Master" mix, you would
set `pParentMixer` to NULL. For the "SFX", "Music" and "Voices" you would set it to a pointer to the master mixer, and you must call `ma_mixer_begin()` and
`ma_mixer_end()` between the begin and end pairs of the parent mixer. If you want to perform submixing, you need to pass the same parent mixer (`pParentMixer`)
to `ma_mixer_end()`. See example 2 for an example on how to do submixing.


Example 1
---------
This example shows a basic mixer without any submixing.

```c
ma_uint64 frameCountIn;
ma_uint64 frameCountOut = desiredFrameCount;    // <-- On input specifies what you want, on output receives what you actually got.
ma_mixer_begin(&mixer, NULL, &frameCountOut, &frameCountIn);
{
    ma_mixer_mix_data_source(&mixer, &decoder1, frameCountIn, isLooping1);
    ma_mixer_mix_data_source(&mixer, &decoder2, frameCountIn, isLooping2);
}
ma_mixer_end(&mixer, NULL, pFramesOut); // <-- pFramesOut must be large enough to receive frameCountOut frames in mixer.format/mixer.channels format.
```


Example 2
---------
This example shows how you can do submixing.

```c
ma_uint64 frameCountIn;
ma_uint64 frameCountOut = desiredFrameCount;    // <-- On input specifies what you want, on output receives what you actually got.
ma_mixer_begin(&masterMixer, NULL, &frameCountOut, &frameCountIn);
{
    ma_uint64 submixFrameCountIn;

    // SFX submix.
    ma_mixer_begin(&sfxMixer, &masterMixer, &submixFrameCountIn, NULL);     // Output frame count not required for submixing.
    {
        ma_mixer_mix_data_source(&sfxMixer, &sfxDecoder1, submixFrameCountIn, isSFXLooping1);
        ma_mixer_mix_data_source(&sfxMixer, &sfxDecoder2, submixFrameCountIn, isSFXLooping2);
    }
    ma_mixer_end(&sfxMixer, &masterMixer, NULL);

    // Voice submix.
    ma_mixer_begin(&voiceMixer, &masterMixer, &submixFrameCountIn, NULL);
    {
        ma_mixer_mix_data_source(&voiceMixer, &voiceDecoder1, submixFrameCountIn, isVoiceLooping1);
    }
    ma_mixer_end(&voiceMixer, &masterMixer, NULL);
    
    // Music submix.
    ma_mixer_begin(&musicMixer, &masterMixer, &submixFrameCountIn, NULL);
    {
        ma_mixer_mix_data_source(&musicMixer, &musicDecoder1, submixFrameCountIn, isMusicLooping1);
    }
    ma_mixer_end(&musicMixer, &masterMixer, NULL);
}
ma_mixer_end(&masterMixer, NULL, pFramesOut); // <-- pFramesOut must be large enough to receive frameCountOut frames in mixer.format/mixer.channels format.
```


See Also
--------
ma_mixer_end()
ma_mixer_set_effect()
ma_mixer_get_effect()
*/
MA_API ma_result ma_mixer_begin(ma_mixer* pMixer, ma_mixer* pParentMixer, ma_uint64* pFrameCountOut, ma_uint64* pFrameCountIn);

/*
Applies volume, performs clipping, applies the effect (if any) and outputs the final mix to the specified output buffer or mixed with another mixer.


Parameters
----------
pMixer (in)
    A pointer to the mixer.

pParentMixer (in, optional)
    A pointer to the parent mixer. If this is non-NULL, the output of `pMixer` will be mixed with `pParentMixer`. It is an error for `pParentMixer` and
    `pFramesOut` to both be non-NULL. If this is non-NULL, it must have also been specified as the parent mixer in the prior call to `ma_mixer_begin()`.

pFramesOut (in, optional)
    A pointer to the buffer that will receive the final mixed output. The output buffer must be in the format specified by the mixer's configuration that was
    used to initialized it. The required size in frames is defined by the output frame count returned by `ma_mixer_begin()`. It is an error for `pFramesOut`
    and `pParentMixer` to both be non-NULL.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Remarks
-------
It is an error both both `pParentMixer` and `pFramesOut` to both be NULL or non-NULL. You must specify one or the other.

When outputting to a parent mixer (`pParentMixer` is non-NULL), the output is mixed with the parent mixer. Otherwise (`pFramesOut` is non-NULL), the output
will overwrite anything already in the output buffer.

When calculating the final output, the volume will be applied before clipping, which is done before applying the effect (if any).

See documentation for `ma_mixer_begin()` for an example on how to use `ma_mixer_end()`.


See Also
--------
ma_mixer_begin()
ma_mixer_set_volume()
ma_mixer_get_volume()
ma_mixer_set_effect()
ma_mixer_get_effect()
*/
MA_API ma_result ma_mixer_end(ma_mixer* pMixer, ma_mixer* pParentMixer, void* pFramesOut);


/*
Mixes audio data from a buffer containing raw PCM data.


Parameters
----------
pMixer (in)
    A pointer to the mixer.

pFramesIn (in)
    A pointer to the buffer containing the raw PCM data to mix with the mixer. The data contained within this buffer is assumed to be of the same format as the
    mixer, which was specified when the mixer was initialized. Use `ma_mixer_mix_pcm_frames_ex()` to mix data of a different format.

frameCountIn (in)
    The number of frames to mix. This cannot exceed the number of input frames returned by `ma_mixer_begin()`. If it does, an error will be returned. If it is
    less, silence will be mixed to make up the excess.

formatIn (in)
    The sample format of the input data.

channelsIn (in)
    The channel count of the input data.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Remarks
-------
Each call to this function will start mixing from the start of the internal accumulation buffer.

This will automatically convert the data to the mixer's native format. The sample format will be converted without dithering. Channels will be converted based
on the default channel map.


See Also
--------
ma_mixer_mix_pcm_frames()
ma_mixer_begin()
ma_mixer_end()
*/
MA_API ma_result ma_mixer_mix_pcm_frames(ma_mixer* pMixer, const void* pFramesIn, ma_uint64 frameCountIn, float volume, ma_format formatIn, ma_uint32 channelsIn);

/*
Mixes audio data from a data source


Parameters
----------
pMixer (in)
    A pointer to the mixer.

pDataSource (in)
    A pointer to the data source to read input data from.

frameCountIn (in)
    The number of frames to mix. This cannot exceed the number of input frames returned by `ma_mixer_begin()`. If it does, an error will be returned. If it is
    less, silence will be mixed to make up the excess.

formatIn (in)
    The sample format of the input data.

channelsIn (in)
    The channel count of the input data.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Remarks
-------
Each call to this function will start mixing from the start of the internal accumulation buffer.

This will automatically convert the data to the mixer's native format. The sample format will be converted without dithering. Channels will be converted based
on the default channel map.


See Also
--------
ma_mixer_begin()
ma_mixer_end()
*/
MA_API ma_result ma_mixer_mix_data_source(ma_mixer* pMixer, ma_data_source* pDataSource, ma_uint64 frameCountIn, float volume, ma_effect* pEffect, ma_bool32 loop);
MA_API ma_result ma_mixer_mix_rb(ma_mixer* pMixer, ma_rb* pRB, ma_uint64 frameCountIn, float volume, ma_effect* pEffect, ma_format formatIn, ma_uint32 channelsIn); /* Caller is the consumer. */
MA_API ma_result ma_mixer_mix_pcm_rb(ma_mixer* pMixer, ma_pcm_rb* pRB, ma_uint64 frameCountIn, float volume, ma_effect* pEffect);                                   /* Caller is the consumer. */

MA_API ma_result ma_mixer_set_volume(ma_mixer* pMixer, float volume);
MA_API ma_result ma_mixer_get_volume(ma_mixer* pMixer, float* pVolume);
MA_API ma_result ma_mixer_set_gain_db(ma_mixer* pMixer, float gainDB);
MA_API ma_result ma_mixer_get_gain_db(ma_mixer* pMixer, float* pGainDB);
MA_API ma_result ma_mixer_set_effect(ma_mixer* pMixer, ma_effect* pEffect);
MA_API ma_result ma_mixer_get_effect(ma_mixer* pMixer, ma_effect** ppEffect);
MA_API ma_result ma_mixer_get_output_data_format(ma_mixer* pMixer, ma_format* pFormat, ma_uint32* pChannels);
MA_API ma_result ma_mixer_get_input_data_format(ma_mixer* pMixer, ma_format* pFormat, ma_uint32* pChannels);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_mixing_h */


#if defined(MA_IMPLEMENTATION) || defined(MINIAUDIO_IMPLEMENTATION)

static void ma_convert_pcm_frames_format_and_channels(void* pDst, ma_format formatOut, ma_uint32 channelsOut, const void* pSrc, ma_format formatIn, ma_uint32 channelsIn, ma_uint64 frameCount, ma_dither_mode ditherMode)
{
    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    if (channelsOut == channelsIn) {
        /* Only format conversion required. */
        if (formatOut == formatIn) {
            /* No data conversion required at all - just copy. */
            if (pDst == pSrc) {
                /* No-op. */
            } else {
                ma_copy_pcm_frames(pDst, pSrc, frameCount, formatOut, channelsOut);
            }
        } else {
            /* Simple format conversion. */
            ma_convert_pcm_frames_format(pDst, formatOut, pSrc, formatIn, frameCount, channelsOut, ditherMode);
        }
    } else {
        /* Getting here means we require a channel converter. We do channel conversion in the input format, and then format convert as a post process step if required. */
        ma_result result;
        ma_channel_converter_config channelConverterConfig;
        ma_channel_converter channelConverter;

        channelConverterConfig = ma_channel_converter_config_init(formatIn, channelsIn, NULL, channelsOut, NULL, ma_channel_mix_mode_default);
        result = ma_channel_converter_init(&channelConverterConfig, &channelConverter);
        if (result != MA_SUCCESS) {
            return; /* Failed to initialize channel converter for some reason. Should never fail. */
        }

        /* If we don't require any format conversion we can output straight into the output buffer. Otherwise we need to use an intermediary. */
        if (formatOut == formatIn) {
            /* No format conversion required. Output straight to the output buffer. */
            ma_channel_converter_process_pcm_frames(&channelConverter, pDst, pSrc, frameCount);
        } else {
            /* Format conversion required. We need to use an intermediary buffer. */
            ma_uint8  buffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE]; /* formatIn, channelsOut */
            ma_uint32 bufferCap = sizeof(buffer) / ma_get_bytes_per_frame(formatIn, channelsOut);
            ma_uint64 totalFramesProcessed = 0;
            
            while (totalFramesProcessed < frameCount) {
                ma_uint64 framesToProcess = frameCount - totalFramesProcessed;
                if (framesToProcess > bufferCap) {
                    framesToProcess = bufferCap;
                }

                result = ma_channel_converter_process_pcm_frames(&channelConverter, buffer, ma_offset_ptr(pSrc, totalFramesProcessed * ma_get_bytes_per_frame(formatIn, channelsIn)), framesToProcess);
                if (result != MA_SUCCESS) {
                    break;
                }

                /* Channel conversion is done, now format conversion straight into the output buffer. */
                ma_convert_pcm_frames_format(ma_offset_ptr(pDst, totalFramesProcessed * ma_get_bytes_per_frame(formatOut, channelsOut)), formatOut, buffer, formatIn, framesToProcess, channelsOut, ditherMode);

                totalFramesProcessed += framesToProcess;
            }
        }
    }
}


static ma_effect_base* ma_effect_get_root(ma_effect* pEffect)
{
    ma_effect_base* pRootEffect;

    if (pEffect == NULL) {
        return NULL;
    }

    pRootEffect = (ma_effect_base*)pEffect;
    for (;;) {
        if (pRootEffect->pPrev == NULL) {
            return pRootEffect;
        } else {
            pRootEffect = pRootEffect->pPrev;
        }
    }

    /* Should never hit this. */
    /*return NULL;*/
}

MA_API ma_result ma_effect_process_pcm_frames(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_result result = MA_SUCCESS;
    ma_effect_base* pBase = (ma_effect_base*)pEffect;
    ma_effect_base* pFirstEffect;
    ma_effect_base* pRunningEffect;
    ma_uint32 iTempBuffer = 0;
    ma_uint8  tempFrames[2][MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
    ma_uint64 tempFrameCount[2];
    ma_uint64 frameCountIn;
    ma_uint64 frameCountInConsumed;
    ma_uint64 frameCountOut;
    ma_uint64 frameCountOutConsumed;

    if (pEffect == NULL || pBase->onProcessPCMFrames == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We need to start at the top and work our way down. */
    pFirstEffect = pEffect;
    while (pFirstEffect->pPrev != NULL) {
        pFirstEffect = pFirstEffect->pPrev;
    }

    pRunningEffect = pFirstEffect;

    /* Optimized path if this is the only effect in the chain. */
    if (pFirstEffect == pBase) {
        return pBase->onProcessPCMFrames(pRunningEffect, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    }

    frameCountIn          = *pFrameCountIn;
    frameCountInConsumed  = 0;
    frameCountOut         = *pFrameCountOut;
    frameCountOutConsumed = 0;

    /*
    We need to output into a temp buffer which will become our new input buffer. We will allocate this on the stack which means we will need to do
    several iterations to process as much data as possible available in the input buffer, or can fit in the output buffer.
    */
    while (frameCountIn < frameCountInConsumed && frameCountOut < frameCountOutConsumed) {
        for (;;) {
            MA_ASSERT(pRunningEffect != NULL);

            const void* pRunningFramesIn;
            /* */ void* pRunningFramesOut;
            ma_uint64 frameCountInThisIteration;
            ma_uint64 frameCountOutThisIteration;
            ma_format runningEffectFormatIn;
            ma_uint32 runningEffectChannelsIn;

            if (pRunningEffect->onGetInputDataFormat == NULL) {
                result = MA_INVALID_ARGS;   /* Don't have a way to retrieve the input format. */
                break;
            }

            result = pRunningEffect->onGetInputDataFormat(pRunningEffect, &runningEffectFormatIn, &runningEffectChannelsIn, NULL);
            if (result != MA_SUCCESS) {
                return result;  /* Failed to retrieve the input data format. Abort. */
            }


            if (pRunningEffect == pFirstEffect) {
                /* It's the first effect which means we need to read directly from the input buffer. */
                pRunningFramesIn = ma_offset_ptr(pFramesIn, frameCountInConsumed * ma_get_bytes_per_frame(runningEffectFormatIn, runningEffectChannelsIn));
                frameCountInThisIteration = frameCountIn - frameCountInConsumed;
            } else {
                /* It's not the first item. We need to read from a temp buffer. */
                pRunningFramesIn = tempFrames[iTempBuffer];
                frameCountInThisIteration = tempFrameCount[iTempBuffer];
                iTempBuffer = (iTempBuffer + 1) & 0x01; /* Toggle between 0 and 1. */
            }

            if (pRunningEffect == pEffect) {
                /* It's the last item in the chain so we need to output directly to the output buffer. */
                pRunningFramesOut = ma_offset_ptr(pFramesOut, frameCountOutConsumed * ma_get_bytes_per_frame(runningEffectFormatIn, runningEffectChannelsIn));
                frameCountOutThisIteration = frameCountOut - frameCountOutConsumed;
            } else {
                /* It's not the last item in the chain. We need to output to a temp buffer so that it becomes our input buffer in the next iteration. */
                pRunningFramesOut = tempFrames[iTempBuffer];
                frameCountOutThisIteration = sizeof(tempFrames[iTempBuffer]) / ma_get_bytes_per_frame(runningEffectFormatIn, runningEffectChannelsIn);
            }

            result = pRunningEffect->onProcessPCMFrames(pRunningEffect, pRunningFramesIn, &frameCountInThisIteration, pRunningFramesOut, &frameCountOutThisIteration);
            if (result != MA_SUCCESS) {
                break;
            }

            /*
            We need to increment our input and output frame counters. This depends on whether or not we read directly from the input buffer or wrote directly
            to the output buffer.
            */
            if (pRunningEffect == pFirstEffect) {
                frameCountInConsumed += frameCountInThisIteration;
            }
            if (pRunningEffect == pBase) {
                frameCountOutConsumed += frameCountOutThisIteration;
            }

            tempFrameCount[iTempBuffer] = frameCountOutThisIteration;


            /* If we just processed the input effect we need to abort. */
            if (pRunningEffect == pBase) {
                break;
            }

            pRunningEffect = pRunningEffect->pNext;
        }
    }


    if (pFrameCountIn != NULL) {
        *pFrameCountIn = frameCountInConsumed;
    }
    if (pFrameCountOut != NULL) {
        *pFrameCountOut = frameCountOutConsumed;
    }

    return result;
}

MA_API ma_result ma_effect_process_pcm_frames_ex(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut, ma_format formatIn, ma_uint32 channelsIn, ma_format formatOut, ma_uint32 channelsOut)
{
    ma_result result;
    ma_format effectFormatIn;
    ma_uint32 effectChannelsIn;
    ma_format effectFormatOut;
    ma_uint32 effectChannelsOut;

    /* First thing to retrieve the effect's input and output format to determine if conversion is necessary. */
    result = ma_effect_get_input_data_format(pEffect, &effectFormatIn, &effectChannelsIn, NULL);
    if (result != MA_SUCCESS) {
        return result;
    }

    result = ma_effect_get_output_data_format(pEffect, &effectFormatOut, &effectChannelsOut, NULL);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (effectFormatIn == formatIn && effectChannelsIn == channelsIn && effectFormatOut == formatOut && effectChannelsOut) {
        /* Fast path. No need for any data conversion. */
        return ma_effect_process_pcm_frames(pEffect, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        /* Slow path. Getting here means we need to do pre- and/or post-data conversion. */
        ma_uint8  effectInBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
        ma_uint32 effectInBufferCap = sizeof(effectInBuffer) / ma_get_bytes_per_frame(effectFormatIn, effectChannelsIn);
        ma_uint8  effectOutBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
        ma_uint32 effectOutBufferCap = sizeof(effectOutBuffer) / ma_get_bytes_per_frame(effectFormatOut, effectChannelsOut);
        ma_uint64 totalFramesProcessedIn  = 0;
        ma_uint64 totalFramesProcessedOut = 0;
        ma_uint64 frameCountIn  = *pFrameCountIn;
        ma_uint64 frameCountOut = *pFrameCountOut;
        
        while (totalFramesProcessedIn < frameCountIn && totalFramesProcessedOut < frameCountOut) {
            ma_uint64 framesToProcessThisIterationIn;
            ma_uint64 framesToProcessThisIterationOut;
            const void* pRunningFramesIn  = ma_offset_ptr(pFramesIn,  ma_get_bytes_per_frame(formatIn,  channelsIn ));
            /* */ void* pRunningFramesOut = ma_offset_ptr(pFramesOut, ma_get_bytes_per_frame(formatOut, channelsOut));

            framesToProcessThisIterationOut = frameCountOut - totalFramesProcessedOut;
            if (framesToProcessThisIterationOut > effectOutBufferCap) {
                framesToProcessThisIterationOut = effectOutBufferCap;
            }

            framesToProcessThisIterationIn = ma_effect_get_required_input_frame_count(pEffect, framesToProcessThisIterationOut);
            if (framesToProcessThisIterationIn > (frameCountIn - totalFramesProcessedIn)) {
                framesToProcessThisIterationIn = (frameCountIn - totalFramesProcessedIn);
            }
            if (framesToProcessThisIterationIn > effectInBufferCap) {
                framesToProcessThisIterationIn = effectInBufferCap;
            }

            /* At this point our input data has been converted to the effect's input format, so now we need to run the effect, making sure we output to the intermediary buffer. */
            if (effectFormatIn == formatIn && effectChannelsIn == channelsIn) {
                /* Fast path. No input conversion required. */
                if (effectFormatOut == formatOut && effectChannelsOut == channelsOut) {
                    /* Fast path. Neither input nor output data conversion required. */
                    ma_effect_process_pcm_frames(pEffect, pRunningFramesIn, &framesToProcessThisIterationIn, pRunningFramesOut, &framesToProcessThisIterationOut);
                } else {
                    /* Slow path. Output conversion required. */
                    ma_effect_process_pcm_frames(pEffect, pRunningFramesIn, &framesToProcessThisIterationIn, effectOutBuffer, &framesToProcessThisIterationOut);
                    ma_convert_pcm_frames_format_and_channels(pRunningFramesOut, formatOut, channelsOut, effectOutBuffer, effectFormatOut, effectChannelsOut, framesToProcessThisIterationOut, ma_dither_mode_none);
                }
            } else {
                /* Slow path. Input conversion required. */
                ma_convert_pcm_frames_format_and_channels(effectInBuffer, effectFormatIn, effectChannelsIn, pRunningFramesIn, formatIn, channelsIn, framesToProcessThisIterationIn, ma_dither_mode_none);

                if (effectFormatOut == formatOut && effectChannelsOut == channelsOut) {
                    /* Fast path. No output format conversion required. */
                    ma_effect_process_pcm_frames(pEffect, effectInBuffer, &framesToProcessThisIterationIn, pRunningFramesOut, &framesToProcessThisIterationOut);
                } else {
                    /* Slow path. Output format conversion required. */
                    ma_effect_process_pcm_frames(pEffect, effectInBuffer, &framesToProcessThisIterationIn, effectOutBuffer, &framesToProcessThisIterationOut);
                    ma_convert_pcm_frames_format_and_channels(pRunningFramesOut, formatOut, channelsOut, effectOutBuffer, effectFormatOut, effectChannelsOut, framesToProcessThisIterationOut, ma_dither_mode_none);
                }
            }

            totalFramesProcessedIn  += framesToProcessThisIterationIn;
            totalFramesProcessedOut += framesToProcessThisIterationOut;
        }
    }

    return MA_SUCCESS;
}

static ma_uint64 ma_effect_get_required_input_frame_count_local(ma_effect* pEffect, ma_uint64 outputFrameCount)
{
    ma_effect_base* pBase = (ma_effect_base*)pEffect;

    MA_ASSERT(pEffect != NULL);

    if (pBase->onGetRequiredInputFrameCount) {
        return pBase->onGetRequiredInputFrameCount(pEffect, outputFrameCount);
    } else {
        /* If there is no callback, assume a 1:1 mapping. */
        return outputFrameCount;
    }
}

MA_API ma_uint64 ma_effect_get_required_input_frame_count(ma_effect* pEffect, ma_uint64 outputFrameCount)
{
    ma_effect_base* pBase = (ma_effect_base*)pEffect;
    ma_uint64 localInputFrameCount;

    if (pEffect == NULL) {
        return 0;
    }

    localInputFrameCount = ma_effect_get_required_input_frame_count_local(pEffect, outputFrameCount);

    if (pBase->pPrev == NULL) {
        return localInputFrameCount;
    } else {
        ma_uint64 parentInputFrameCount = ma_effect_get_required_input_frame_count(pBase->pPrev, outputFrameCount);
        if (parentInputFrameCount > localInputFrameCount) {
            return parentInputFrameCount;
        } else {
            return localInputFrameCount;
        }
    }
}

static ma_uint64 ma_effect_get_expected_output_frame_count_local(ma_effect* pEffect, ma_uint64 inputFrameCount)
{
    ma_effect_base* pBase = (ma_effect_base*)pEffect;

    MA_ASSERT(pEffect != NULL);

    if (pBase->onGetExpectedOutputFrameCount) {
        return pBase->onGetExpectedOutputFrameCount(pEffect, inputFrameCount);
    } else {
        /* If there is no callback, assume a 1:1 mapping. */
        return inputFrameCount;
    }
}

MA_API ma_uint64 ma_effect_get_expected_output_frame_count(ma_effect* pEffect, ma_uint64 inputFrameCount)
{
    ma_effect_base* pBase = (ma_effect_base*)pEffect;
    ma_uint64 localOutputFrameCount;

    if (pEffect == NULL) {
        return 0;
    }

    localOutputFrameCount = ma_effect_get_expected_output_frame_count_local(pEffect, inputFrameCount);

    if (pBase->pPrev == NULL) {
        return localOutputFrameCount;
    } else {
        ma_uint64 parentOutputFrameCount = ma_effect_get_expected_output_frame_count(pBase->pPrev, inputFrameCount);
        if (parentOutputFrameCount < localOutputFrameCount) {
            return parentOutputFrameCount;
        } else {
            return localOutputFrameCount;
        }
    }
}

ma_result ma_effect_append(ma_effect* pEffect, ma_effect* pParent)
{
    ma_effect_base* pBaseEffect = (ma_effect_base*)pEffect;
    ma_effect_base* pBaseParent = (ma_effect_base*)pParent;

    if (pEffect == NULL || pParent == NULL || pEffect == pParent) {
        return MA_INVALID_ARGS;
    }

    /* The effect must be detached before reinserting into the list. */
    if (pBaseEffect->pPrev != NULL || pBaseEffect->pNext != NULL) {
        return MA_INVALID_OPERATION;
    }

    MA_ASSERT(pBaseEffect->pPrev == NULL);
    MA_ASSERT(pBaseEffect->pNext == NULL);

    /* Update the effect first. */
    pBaseEffect->pPrev = pBaseParent;
    pBaseEffect->pNext = pBaseParent->pNext;

    /* Now update the parent. Slot the effect between the parent and the parent's next item, if it has one. */
    if (pBaseParent->pNext != NULL) {
        pBaseParent->pNext->pPrev = pEffect;
    }
    pBaseParent->pNext = pEffect;

    return MA_SUCCESS;
}

ma_result ma_effect_prepend(ma_effect* pEffect, ma_effect* pChild)
{
    ma_effect_base* pBaseEffect = (ma_effect_base*)pEffect;
    ma_effect_base* pBaseChild  = (ma_effect_base*)pChild;

    if (pChild == NULL || pChild == NULL || pEffect == pChild) {
        return MA_INVALID_ARGS;
    }

    /* The effect must be detached before reinserting into the list. */
    if (pBaseEffect->pPrev != NULL || pBaseEffect->pNext != NULL) {
        return MA_INVALID_OPERATION;
    }

    MA_ASSERT(pBaseEffect->pPrev == NULL);
    MA_ASSERT(pBaseEffect->pNext == NULL);

    /* Update the effect first. */
    pBaseEffect->pNext = pBaseChild;
    pBaseEffect->pPrev = pBaseChild->pPrev;

    /* Now update the child. Slot the effect between the child and the child's previous item, if it has one. */
    if (pBaseChild->pPrev != NULL) {
        pBaseChild->pPrev->pNext = pEffect;
    }
    pBaseChild->pPrev = pEffect;

    return MA_SUCCESS;
}

ma_result ma_effect_detach(ma_effect* pEffect)
{
    ma_effect_base* pBaseEffect = (ma_effect_base*)pEffect;

    if (pBaseEffect == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pBaseEffect->pPrev != NULL) {
        pBaseEffect->pPrev->pNext = pBaseEffect->pNext;
        pBaseEffect->pPrev = NULL;
    }

    if (pBaseEffect->pNext != NULL) {
        pBaseEffect->pNext->pPrev = pBaseEffect->pPrev;
        pBaseEffect->pNext = NULL;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_effect_get_output_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_effect_base* pBaseEffect = (ma_effect_base*)pEffect;
    ma_result result;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    if (pFormat != NULL) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels != NULL) {
        *pChannels = 0;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = 0;
    }

    if (pBaseEffect == NULL || pBaseEffect->onGetOutputDataFormat == NULL) {
        return MA_INVALID_ARGS;
    }

    result = pBaseEffect->onGetOutputDataFormat(pEffect, &format, &channels, &sampleRate);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (pFormat != NULL) {
        *pFormat = format;
    }
    if (pChannels != NULL) {
        *pChannels = channels;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = sampleRate;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_effect_get_input_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_effect_base* pRootEffect;
    ma_result result;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    if (pFormat != NULL) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels != NULL) {
        *pChannels = 0;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = 0;
    }

    pRootEffect = ma_effect_get_root(pEffect);
    if (pRootEffect == NULL || pRootEffect->onGetInputDataFormat == NULL) {
        return MA_INVALID_ARGS;
    }

    result = pRootEffect->onGetInputDataFormat(pRootEffect, &format, &channels, &sampleRate);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (pFormat != NULL) {
        *pFormat = format;
    }
    if (pChannels != NULL) {
        *pChannels = channels;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = sampleRate;
    }

    return MA_SUCCESS;
}



MA_API size_t ma_get_accumulation_bytes_per_sample(ma_format format)
{
    size_t bytesPerSample[ma_format_count] = {
        0,                  /* ma_format_unknown */
        sizeof(ma_int16),   /* ma_format_u8  */
        sizeof(ma_int32),   /* ma_format_s16 */
        sizeof(ma_int64),   /* ma_format_s24 */
        sizeof(ma_int64),   /* ma_format_s32 */
        sizeof(float)       /* ma_format_f32 */
    };

    return bytesPerSample[format];
}

MA_API size_t ma_get_accumulation_bytes_per_frame(ma_format format, ma_uint32 channels)
{
    return ma_get_accumulation_bytes_per_sample(format) * channels;
}



static MA_INLINE ma_int16 ma_float_to_fixed_16(float x)
{
    return (ma_int16)(x * (1 << 8));
}



static MA_INLINE ma_int16 ma_apply_volume_unclipped_u8(ma_int16 x, ma_int16 volume)
{
    return (ma_int16)(((ma_int32)x * (ma_int32)volume) >> 8);
}

static MA_INLINE ma_int32 ma_apply_volume_unclipped_s16(ma_int32 x, ma_int16 volume)
{
    return (ma_int32)((x * volume) >> 8);
}

static MA_INLINE ma_int64 ma_apply_volume_unclipped_s24(ma_int64 x, ma_int16 volume)
{
    return (ma_int64)((x * volume) >> 8);
}

static MA_INLINE ma_int64 ma_apply_volume_unclipped_s32(ma_int64 x, ma_int16 volume)
{
    return (ma_int64)((x * volume) >> 8);
}

static MA_INLINE float ma_apply_volume_unclipped_f32(float x, float volume)
{
    return x * volume;
}


#if 0
static void ma_accumulate_and_clip_u8(ma_uint8* pDst, const ma_int16* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_u8(ma_pcm_sample_u8_to_s16_no_scale(pDst[iSample]) + pSrc[iSample]);
    }
}

static void ma_accumulate_and_clip_s16(ma_int16* pDst, const ma_int32* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s16(pDst[iSample] + pSrc[iSample]);
    }
}

static void ma_accumulate_and_clip_s24(ma_uint8* pDst, const ma_int64* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        ma_int64 s = ma_clip_s24(ma_pcm_sample_s24_to_s32_no_scale(&pDst[iSample*3]) + pSrc[iSample]);
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_accumulate_and_clip_s32(ma_int32* pDst, const ma_int64* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s32(pDst[iSample] + pSrc[iSample]);
    }
}

static void ma_accumulate_and_clip_f32(float* pDst, const float* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_f32(pDst[iSample] + pSrc[iSample]);
    }
}
#endif


static void ma_clip_samples_u8(ma_uint8* pDst, const ma_int16* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_u8(pSrc[iSample]);
    }
}

static void ma_clip_samples_s16(ma_int16* pDst, const ma_int32* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s16(pSrc[iSample]);
    }
}

static void ma_clip_samples_s24(ma_uint8* pDst, const ma_int64* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        ma_int64 s = ma_clip_s24(pSrc[iSample]);
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_clip_samples_s32(ma_int32* pDst, const ma_int64* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s32(pSrc[iSample]);
    }
}

static void ma_clip_samples_f32_ex(float* pDst, const float* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_f32(pSrc[iSample]);
    }
}



static void ma_volume_and_clip_samples_u8(ma_uint8* pDst, const ma_int16* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_u8(ma_apply_volume_unclipped_u8(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_clip_samples_s16(ma_int16* pDst, const ma_int32* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s16(ma_apply_volume_unclipped_s16(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_clip_samples_s24(ma_uint8* pDst, const ma_int64* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        ma_int64 s = ma_clip_s24(ma_apply_volume_unclipped_s24(pSrc[iSample], volumeFixed));
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_volume_and_clip_samples_s32(ma_int32* pDst, const ma_int64* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s32(ma_apply_volume_unclipped_s32(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_clip_samples_f32(float* pDst, const float* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    /* For the f32 case we need to make sure this supports in-place processing where the input and output buffers are the same. */

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_f32(ma_apply_volume_unclipped_f32(pSrc[iSample], volume));
    }
}

static void ma_clip_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels)
{
    ma_uint64 sampleCount;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    sampleCount = frameCount * channels;

    switch (format) {
        case ma_format_u8:  ma_clip_samples_u8( pDst, pSrc, sampleCount); break;
        case ma_format_s16: ma_clip_samples_s16(pDst, pSrc, sampleCount); break;
        case ma_format_s24: ma_clip_samples_s24(pDst, pSrc, sampleCount); break;
        case ma_format_s32: ma_clip_samples_s32(pDst, pSrc, sampleCount); break;
        case ma_format_f32: ma_clip_samples_f32_ex(pDst, pSrc, sampleCount); break;
        
        /* Do nothing if we don't know the format. We're including these here to silence a compiler warning about enums not being handled by the switch. */
        case ma_format_unknown:
        case ma_format_count:
            break;
    }
}

static void ma_volume_and_clip_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels, float volume)
{
    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    if (volume == 1) {
        ma_clip_pcm_frames(pDst, pSrc, frameCount, format, channels);   /* Optimized case for volume = 1. */
    } else if (volume == 0) {
        ma_silence_pcm_frames(pDst, frameCount, format, channels);      /* Optimized case for volume = 0. */
    } else {
        ma_uint64 sampleCount = frameCount * channels;

        switch (format) {
            case ma_format_u8:  ma_volume_and_clip_samples_u8( pDst, pSrc, sampleCount, volume); break;
            case ma_format_s16: ma_volume_and_clip_samples_s16(pDst, pSrc, sampleCount, volume); break;
            case ma_format_s24: ma_volume_and_clip_samples_s24(pDst, pSrc, sampleCount, volume); break;
            case ma_format_s32: ma_volume_and_clip_samples_s32(pDst, pSrc, sampleCount, volume); break;
            case ma_format_f32: ma_volume_and_clip_samples_f32(pDst, pSrc, sampleCount, volume); break;
            
            /* Do nothing if we don't know the format. We're including these here to silence a compiler warning about enums not being handled by the switch. */
            case ma_format_unknown:
            case ma_format_count:
                break;
        }
    }
}


static void ma_clipped_accumulate_u8(ma_uint8* pDst, const ma_uint8* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = ma_clip_u8(ma_pcm_sample_u8_to_s16_no_scale(pDst[iSample]) + ma_pcm_sample_u8_to_s16_no_scale(pSrc[iSample]));
    }
}

static void ma_clipped_accumulate_s16(ma_int16* pDst, const ma_int16* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = ma_clip_s16((ma_int32)pDst[iSample] + (ma_int32)pSrc[iSample]);
    }
}

static void ma_clipped_accumulate_s24(ma_uint8* pDst, const ma_uint8* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        ma_int64 s = ma_clip_s24(ma_pcm_sample_s24_to_s32_no_scale(&pDst[iSample*3]) + ma_pcm_sample_s24_to_s32_no_scale(&pSrc[iSample*3]));
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_clipped_accumulate_s32(ma_int32* pDst, const ma_int32* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = ma_clip_s32((ma_int64)pDst[iSample] + (ma_int64)pSrc[iSample]);
    }
}

static void ma_clipped_accumulate_f32(float* pDst, const float* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = ma_clip_f32(pDst[iSample] + pSrc[iSample]);
    }
}

static void ma_clipped_accumulate_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels)
{
    ma_uint64 sampleCount;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    sampleCount = frameCount * channels;

    switch (format) {
        case ma_format_u8:  ma_clipped_accumulate_u8( pDst, pSrc, sampleCount); break;
        case ma_format_s16: ma_clipped_accumulate_s16(pDst, pSrc, sampleCount); break;
        case ma_format_s24: ma_clipped_accumulate_s24(pDst, pSrc, sampleCount); break;
        case ma_format_s32: ma_clipped_accumulate_s32(pDst, pSrc, sampleCount); break;
        case ma_format_f32: ma_clipped_accumulate_f32(pDst, pSrc, sampleCount); break;

        /* Do nothing if we don't know the format. We're including these here to silence a compiler warning about enums not being handled by the switch. */
        case ma_format_unknown:
        case ma_format_count:
            break;
    }
}



static void ma_unclipped_accumulate_u8(ma_int16* pDst, const ma_uint8* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = pDst[iSample] + ma_pcm_sample_u8_to_s16_no_scale(pSrc[iSample]);
    }
}

static void ma_unclipped_accumulate_s16(ma_int32* pDst, const ma_int16* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = (ma_int32)pDst[iSample] + (ma_int32)pSrc[iSample];
    }
}

static void ma_unclipped_accumulate_s24(ma_int64* pDst, const ma_uint8* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = pDst[iSample] + ma_pcm_sample_s24_to_s32_no_scale(&pSrc[iSample*3]);
    }
}

static void ma_unclipped_accumulate_s32(ma_int64* pDst, const ma_int32* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = (ma_int64)pDst[iSample] + (ma_int64)pSrc[iSample];
    }
}

static void ma_unclipped_accumulate_f32(float* pDst, const float* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = pDst[iSample] + pSrc[iSample];
    }
}

static void ma_unclipped_accumulate_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels)
{
    ma_uint64 sampleCount;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    sampleCount = frameCount * channels;

    switch (format) {
        case ma_format_u8:  ma_unclipped_accumulate_u8( pDst, pSrc, sampleCount); break;
        case ma_format_s16: ma_unclipped_accumulate_s16(pDst, pSrc, sampleCount); break;
        case ma_format_s24: ma_unclipped_accumulate_s24(pDst, pSrc, sampleCount); break;
        case ma_format_s32: ma_unclipped_accumulate_s32(pDst, pSrc, sampleCount); break;
        case ma_format_f32: ma_unclipped_accumulate_f32(pDst, pSrc, sampleCount); break;

        /* Do nothing if we don't know the format. We're including these here to silence a compiler warning about enums not being handled by the switch. */
        case ma_format_unknown:
        case ma_format_count:
            break;
    }
}


#if 0
static void ma_volume_and_accumulate_and_clip_u8(ma_uint8* pDst, const ma_int16* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_u8(ma_pcm_sample_u8_to_s16_no_scale(pDst[iSample]) + ma_apply_volume_unclipped_u8(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_accumulate_and_clip_s16(ma_int16* pDst, const ma_int32* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s16(pDst[iSample] + ma_apply_volume_unclipped_s16(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_accumulate_and_clip_s24(ma_uint8* pDst, const ma_int64* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        ma_int64 s = ma_clip_s24(ma_pcm_sample_s24_to_s32_no_scale(&pDst[iSample*3]) + ma_apply_volume_unclipped_s24(pSrc[iSample], volumeFixed));
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_volume_and_accumulate_and_clip_s32(ma_int32* dst, const ma_int64* src, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(dst != NULL);
    MA_ASSERT(src != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        dst[iSample] = ma_clip_s32(dst[iSample] + ma_apply_volume_unclipped_s32(src[iSample], volumeFixed));
    }
}

static void ma_volume_and_accumulate_and_clip_f32(float* pDst, const float* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_f32(pDst[iSample] + ma_apply_volume_unclipped_f32(pSrc[iSample], volume));
    }
}

static ma_result ma_volume_and_accumulate_and_clip_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels, float volume)
{
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The output buffer cannot be the same as the accumulation buffer. */
    if (pDst == pSrc) {
        return MA_INVALID_OPERATION;
    }

    /* No-op if there's no volume. */
    if (volume == 0) {
        return MA_SUCCESS;
    }

    sampleCount = frameCount * channels;

    /* No need for volume control if the volume is 1. */
    if (volume == 1) {
        switch (format) {
            case ma_format_u8:  ma_accumulate_and_clip_u8( pDst, pSrc, sampleCount); break;
            case ma_format_s16: ma_accumulate_and_clip_s16(pDst, pSrc, sampleCount); break;
            case ma_format_s24: ma_accumulate_and_clip_s24(pDst, pSrc, sampleCount); break;
            case ma_format_s32: ma_accumulate_and_clip_s32(pDst, pSrc, sampleCount); break;
            case ma_format_f32: ma_accumulate_and_clip_f32(pDst, pSrc, sampleCount); break;
            default: return MA_INVALID_ARGS;    /* Unknown format. */
        }
    } else {
        /* Getting here means the volume is not 0 nor 1. */
        MA_ASSERT(volume != 0 && volume != 1);

        switch (format) {
            case ma_format_u8:  ma_volume_and_accumulate_and_clip_u8( pDst, pSrc, sampleCount, volume); break;
            case ma_format_s16: ma_volume_and_accumulate_and_clip_s16(pDst, pSrc, sampleCount, volume); break;
            case ma_format_s24: ma_volume_and_accumulate_and_clip_s24(pDst, pSrc, sampleCount, volume); break;
            case ma_format_s32: ma_volume_and_accumulate_and_clip_s32(pDst, pSrc, sampleCount, volume); break;
            case ma_format_f32: ma_volume_and_accumulate_and_clip_f32(pDst, pSrc, sampleCount, volume); break;
            default: return MA_INVALID_ARGS;        /* Unknown format. */
        }
    }

    return MA_SUCCESS;
}
#endif

static ma_result ma_volume_and_clip_and_effect_pcm_frames(void* pDst, ma_format formatOut, ma_uint32 channelsOut, ma_uint64 frameCountOut, const void* pSrc, ma_format formatIn, ma_uint32 channelsIn, ma_uint64 frameCountIn, float volume, ma_effect* pEffect, ma_bool32 isAccumulation)
{
    ma_result result;
    ma_uint8  effectBufferIn[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
    ma_uint32 effectBufferInCapInFrames;
    ma_uint8  effectBufferOut[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
    ma_uint32 effectBufferOutCapInFrames;
    ma_format effectFormatIn;
    ma_uint32 effectChannelsIn;
    ma_format effectFormatOut;
    ma_uint32 effectChannelsOut;
    ma_uint64 totalFramesProcessedOut = 0;
    ma_uint64 totalFramesProcessedIn  = 0;
    /* */ void* pRunningDst = pDst;
    const void* pRunningSrc = pSrc;

    if (pDst == NULL || pSrc == NULL || pEffect == NULL) {
        return MA_INVALID_ARGS;
    }

    /* No op if silent. */
    if (volume == 0) {
        return MA_SUCCESS;
    }

    /* We need to know the effect's input and output formats so we can do pre- and post-effect data conversion if necessary. */
    ma_effect_get_input_data_format( pEffect, &effectFormatIn,  &effectChannelsIn,  NULL);
    ma_effect_get_output_data_format(pEffect, &effectFormatOut, &effectChannelsOut, NULL);

    effectBufferInCapInFrames  = sizeof(effectBufferIn ) / ma_get_bytes_per_frame(effectFormatIn,  effectChannelsIn );
    effectBufferOutCapInFrames = sizeof(effectBufferOut) / ma_get_bytes_per_frame(effectFormatOut, effectChannelsOut);

    while (totalFramesProcessedOut < frameCountOut && totalFramesProcessedIn < frameCountIn) {
        ma_uint64 effectFrameCountIn;
        ma_uint64 effectFrameCountOut;

        effectFrameCountOut = frameCountOut - totalFramesProcessedOut;
        if (effectFrameCountOut > effectBufferOutCapInFrames) {
            effectFrameCountOut = effectBufferOutCapInFrames;
        }

        effectFrameCountIn = ma_effect_get_required_input_frame_count(pEffect, effectFrameCountOut);
        if (effectFrameCountIn > frameCountIn - totalFramesProcessedIn) {
            effectFrameCountIn = frameCountIn - totalFramesProcessedIn;
        }
        if (effectFrameCountIn > effectBufferInCapInFrames) {
            effectFrameCountIn = effectBufferInCapInFrames;
        }

        /*
        The first step is to get the data ready for the effect. If the effect's input format and channels are the same as the source buffer, we just
        clip the accumulation buffer straight input the effect's input buffer. Otherwise need to do a conversion.
        */
        if (effectFormatIn == formatIn && effectChannelsIn == channelsIn) {
            /* Fast path. No data conversion required for the input data except clipping. */
            ma_volume_and_clip_pcm_frames(effectBufferIn, pRunningSrc, effectBufferInCapInFrames, formatIn, channelsIn, volume);
        } else {
            /* Slow path. Data conversion required between the input data and the effect input data. */
            ma_uint8  clippedSrcBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint32 clippedSrcBufferCapInFrames = sizeof(clippedSrcBuffer) / ma_get_bytes_per_frame(formatIn, channelsIn);

            if (effectFrameCountIn > clippedSrcBufferCapInFrames) {
                effectFrameCountIn = clippedSrcBufferCapInFrames;
            }

            ma_volume_and_clip_pcm_frames(clippedSrcBuffer, pRunningSrc, effectFrameCountIn, formatIn, channelsIn, volume);

            /* At this point the input data has had volume and clipping applied. We can now convert this to the effect's input format. */
            ma_convert_pcm_frames_format_and_channels(effectBufferIn, effectFormatIn, effectChannelsIn, clippedSrcBuffer, formatIn, channelsIn, effectFrameCountIn, ma_dither_mode_none);
        }

        /* At this point we have our input data in the effect's input format and we can now apply it. */
        result = ma_effect_process_pcm_frames(pEffect, effectBufferIn, &effectFrameCountIn, effectBufferOut, &effectFrameCountOut);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to process the effect. */
        }

        /*
        The effect has been applied. If the effect's output format is the same as the final output we can just accumulate straight into the output buffer,
        otherwise we need to convert.
        */
        if (effectFormatOut == formatOut && effectChannelsOut == channelsOut) {
            /* Fast path. No data conversion required for output data. Just accumulate. */
            if (isAccumulation) {
                ma_unclipped_accumulate_pcm_frames(pRunningDst, effectBufferOut, effectFrameCountOut, effectFormatOut, effectChannelsOut);
            } else {
                ma_clipped_accumulate_pcm_frames(pRunningDst, effectBufferOut, effectFrameCountOut, effectFormatOut, effectChannelsOut);
            }
        } else {
            /* Slow path. Data conversion required before accumulating. */
            ma_uint8  accumulationInBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint32 accumulationInBufferCapInFrames = sizeof(accumulationInBuffer) / ma_get_bytes_per_frame(formatOut, channelsOut);
            ma_uint64 totalFramesAccumulated = 0;
            ma_uint8* pRunningEffectBufferOut = effectBufferOut;

            while (totalFramesAccumulated < effectFrameCountOut) {
                ma_uint64 framesToAccumulate = effectFrameCountOut - totalFramesAccumulated;
                if (framesToAccumulate > accumulationInBufferCapInFrames) {
                    framesToAccumulate = accumulationInBufferCapInFrames;
                }

                /* We know how many frames to process in this iteration, so first of all do the conversion from the effect's output to the final output format.*/
                ma_convert_pcm_frames_format_and_channels(accumulationInBuffer, formatOut, channelsOut, pRunningEffectBufferOut, effectFormatOut, effectChannelsOut, framesToAccumulate, ma_dither_mode_none);

                /* We have the data in the final output format, so now we just accumulate or overwrite. */
                if (isAccumulation) {
                    ma_unclipped_accumulate_pcm_frames(ma_offset_ptr(pRunningDst, totalFramesAccumulated * ma_get_accumulation_bytes_per_frame(formatOut, channelsOut)), accumulationInBuffer, framesToAccumulate, formatOut, channelsOut);
                } else {
                    ma_clip_pcm_frames(ma_offset_ptr(pRunningDst, totalFramesAccumulated * ma_get_bytes_per_frame(formatOut, channelsOut)), accumulationInBuffer, framesToAccumulate, formatOut, channelsOut);
                }
                
                totalFramesAccumulated += framesToAccumulate;
                pRunningEffectBufferOut = ma_offset_ptr(pRunningEffectBufferOut, framesToAccumulate * ma_get_bytes_per_frame(formatOut, channelsOut));
            }
        }

        totalFramesProcessedIn  += effectFrameCountIn;
        totalFramesProcessedOut += effectFrameCountOut;

        pRunningSrc = ma_offset_ptr(pRunningSrc, effectFrameCountIn * ma_get_accumulation_bytes_per_frame(formatIn, channelsIn));
        if (isAccumulation) {
            pRunningDst = ma_offset_ptr(pRunningDst, effectFrameCountOut * ma_get_accumulation_bytes_per_frame(formatIn, channelsIn));
        } else {
            pRunningDst = ma_offset_ptr(pRunningDst, effectFrameCountOut * ma_get_bytes_per_frame(formatOut, channelsOut));
        }
    }

    return MA_SUCCESS;
}


static ma_result ma_mix_pcm_frames_u8(ma_int16* pDst, const ma_uint8* pSrc, ma_uint32 channels, ma_uint64 frameCount, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }
    
    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_pcm_sample_u8_to_s16_no_scale(pSrc[iSample]);
        }
    } else {
        ma_int16 volumeFixed = ma_float_to_fixed_16(volume);
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_u8(ma_pcm_sample_u8_to_s16_no_scale(pSrc[iSample]), volumeFixed);
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames_s16(ma_int32* pDst, const ma_int16* pSrc, ma_uint32 channels, ma_uint64 frameCount, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }
    
    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += pSrc[iSample];
        }
    } else {
        ma_int16 volumeFixed = ma_float_to_fixed_16(volume);
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_s16(pSrc[iSample], volumeFixed);
        }
    }
    
    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames_s24(ma_int64* pDst, const ma_uint8* pSrc, ma_uint32 channels, ma_uint64 frameCount, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }
    
    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_pcm_sample_s24_to_s32_no_scale(&pSrc[iSample*3]);
        }
    } else {
        ma_int16 volumeFixed = ma_float_to_fixed_16(volume);
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_s24(ma_pcm_sample_s24_to_s32_no_scale(&pSrc[iSample*3]), volumeFixed);
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames_s32(ma_int64* pDst, const ma_int32* pSrc, ma_uint32 channels, ma_uint64 frameCount, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }
    
    
    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += pSrc[iSample];
        }
    } else {
        ma_int16 volumeFixed = ma_float_to_fixed_16(volume);
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_s32(pSrc[iSample], volumeFixed);
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames_f32(float* pDst, const float* pSrc, ma_uint32 channels, ma_uint64 frameCount, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }

    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += pSrc[iSample];
        }
    } else {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_f32(pSrc[iSample], volume);
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels, float volume)
{
    ma_result result;

    switch (format)
    {
        case ma_format_u8:  result = ma_mix_pcm_frames_u8( pDst, pSrc, channels, frameCount, volume); break;
        case ma_format_s16: result = ma_mix_pcm_frames_s16(pDst, pSrc, channels, frameCount, volume); break;
        case ma_format_s24: result = ma_mix_pcm_frames_s24(pDst, pSrc, channels, frameCount, volume); break;
        case ma_format_s32: result = ma_mix_pcm_frames_s32(pDst, pSrc, channels, frameCount, volume); break;
        case ma_format_f32: result = ma_mix_pcm_frames_f32(pDst, pSrc, channels, frameCount, volume); break;
        default: return MA_INVALID_ARGS;    /* Unknown format. */
    }

    return result;
}

static ma_result ma_mix_pcm_frames_ex(void* pDst, ma_format formatOut, ma_uint32 channelsOut, const void* pSrc, ma_format formatIn, ma_uint32 channelsIn, ma_uint64 frameCount, float volume)
{
    if (pDst == NULL || pSrc == NULL) {
        return MA_INVALID_ARGS;
    }

    if (formatOut == formatIn && channelsOut == channelsIn) {
        /* Fast path. */
        return ma_mix_pcm_frames(pDst, pSrc, frameCount, formatOut, channelsOut, volume);
    } else {
        /* Slow path. Data conversion required. */
        ma_uint8  buffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
        ma_uint32 bufferCapInFrames = sizeof(buffer) / ma_get_bytes_per_frame(formatOut, channelsOut);
        ma_uint64 totalFramesProcessed = 0;
        /* */ void* pRunningDst = pDst;
        const void* pRunningSrc = pSrc;

        while (totalFramesProcessed < frameCount) {
            ma_uint64 framesToProcess = frameCount - totalFramesProcessed;
            if (framesToProcess > bufferCapInFrames) {
                framesToProcess = bufferCapInFrames;
            }

            /* Conversion. */
            ma_convert_pcm_frames_format_and_channels(buffer, formatOut, channelsOut, pRunningSrc, formatIn, channelsIn, framesToProcess, ma_dither_mode_none);

            /* Mixing. */
            ma_mix_pcm_frames(pRunningDst, buffer, framesToProcess, formatOut, channelsOut, volume);

            totalFramesProcessed += framesToProcess;
            pRunningDst = ma_offset_ptr(pRunningDst, framesToProcess * ma_get_accumulation_bytes_per_frame(formatOut, channelsOut));
            pRunningSrc = ma_offset_ptr(pRunningSrc, framesToProcess * ma_get_bytes_per_frame(formatIn, channelsIn));
        }
    }

    return MA_SUCCESS;
}


static void ma_mix_accumulation_buffers_u8(ma_int16* pDst, const ma_int16* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_u8(pSrc[iSample], volumeFixed);
    }
}

static void ma_mix_accumulation_buffers_s16(ma_int32* pDst, const ma_int32* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_s16(pSrc[iSample], volumeFixed);
    }
}

static void ma_mix_accumulation_buffers_s24(ma_int64* pDst, const ma_int64* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_s24(pSrc[iSample], volumeFixed);
    }
}

static void ma_mix_accumulation_buffers_s32(ma_int64* pDst, const ma_int64* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_s32(pSrc[iSample], volumeFixed);
    }
}

static void ma_mix_accumulation_buffers_f32(float* pDst, const float* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_f32(pSrc[iSample], volume);
    }
}

static void ma_mix_accumulation_buffers(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format formatIn, ma_uint32 channelsIn, float volume)
{
    ma_uint64 sampleCount;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    sampleCount = frameCount * channelsIn;

    switch (formatIn)
    {
        case ma_format_u8:  ma_mix_accumulation_buffers_u8( pDst, pSrc, sampleCount, volume); break;
        case ma_format_s16: ma_mix_accumulation_buffers_s16(pDst, pSrc, sampleCount, volume); break;
        case ma_format_s24: ma_mix_accumulation_buffers_s24(pDst, pSrc, sampleCount, volume); break;
        case ma_format_s32: ma_mix_accumulation_buffers_s32(pDst, pSrc, sampleCount, volume); break;
        case ma_format_f32: ma_mix_accumulation_buffers_f32(pDst, pSrc, sampleCount, volume); break;
        default: break;
    }
}

static void ma_mix_accumulation_buffers_ex(void* pDst, ma_format formatOut, ma_uint32 channelsOut, const void* pSrc, ma_format formatIn, ma_uint32 channelsIn, ma_uint64 frameCount, float volume)
{
    if (formatOut == formatIn && channelsOut == channelsIn) {
        /* Fast path. No conversion required. */
        ma_mix_accumulation_buffers(pDst, pSrc, frameCount, formatIn, channelsIn, volume);
    } else {
        /* Slow path. Conversion required. The way we're going to do this is clip the input buffer, and then use existing mixing infrastructure to mix as if it were regular input. */
        ma_uint8  clippedSrcBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE]; /* formatIn, channelsIn */
        ma_uint32 clippedSrcBufferCapInFrames = sizeof(clippedSrcBuffer) / ma_get_bytes_per_frame(formatIn, channelsIn);
        ma_uint64 totalFramesProcessed = 0;
        /* */ void* pRunningDst = pDst;
        const void* pRunningSrc = pSrc;

        while (totalFramesProcessed < frameCount) {
            ma_uint64 framesToProcess = frameCount - totalFramesProcessed;
            if (framesToProcess > clippedSrcBufferCapInFrames) {
                framesToProcess = clippedSrcBufferCapInFrames;
            }

            /* Volume and clip. */
            ma_volume_and_clip_pcm_frames(clippedSrcBuffer, pRunningSrc, framesToProcess, formatIn, channelsIn, volume);

            /* Mix. */
            ma_mix_pcm_frames_ex(pRunningDst, formatOut, channelsOut, clippedSrcBuffer, formatIn, channelsIn, framesToProcess, 1);
            
            totalFramesProcessed += framesToProcess;
            pRunningDst = ma_offset_ptr(pRunningDst, framesToProcess * ma_get_accumulation_bytes_per_frame(formatOut, channelsOut));
            pRunningSrc = ma_offset_ptr(pRunningSrc, framesToProcess * ma_get_accumulation_bytes_per_frame(formatIn,  channelsIn ));
        }
    }
}




MA_API ma_mixer_config ma_mixer_config_init(ma_format format, ma_uint32 channels, ma_uint64 accumulationBufferSizeInFrames, void* pPreAllocatedAccumulationBuffer, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_mixer_config config;
    
    MA_ZERO_OBJECT(&config);
    config.format = format;
    config.channels = channels;
    config.accumulationBufferSizeInFrames = accumulationBufferSizeInFrames;
    config.pPreAllocatedAccumulationBuffer = pPreAllocatedAccumulationBuffer;
    config.volume = 1;
    ma_allocation_callbacks_init_copy(&config.allocationCallbacks, pAllocationCallbacks);

    return config;
}


MA_API ma_result ma_mixer_init(ma_mixer_config* pConfig, ma_mixer* pMixer)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pMixer);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->accumulationBufferSizeInFrames == 0) {
        return MA_INVALID_ARGS; /* Must have an accumulation buffer. */
    }

    pMixer->format                         = pConfig->format;
    pMixer->channels                       = pConfig->channels;
    pMixer->accumulationBufferSizeInFrames = pConfig->accumulationBufferSizeInFrames;
    pMixer->pAccumulationBuffer            = pConfig->pPreAllocatedAccumulationBuffer;
    ma_allocation_callbacks_init_copy(&pMixer->allocationCallbacks, &pConfig->allocationCallbacks);
    pMixer->volume                         = pConfig->volume;

    if (pMixer->pAccumulationBuffer == NULL) {
        ma_uint64 accumulationBufferSizeInBytes = pConfig->accumulationBufferSizeInFrames * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels);
        if (accumulationBufferSizeInBytes > MA_SIZE_MAX) {
            return MA_OUT_OF_MEMORY;
        }

        pMixer->pAccumulationBuffer = ma__malloc_from_callbacks((size_t)accumulationBufferSizeInBytes, &pMixer->allocationCallbacks);   /* Safe cast. */
        if (pMixer->pAccumulationBuffer == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        pMixer->ownsAccumulationBuffer = MA_TRUE;
    } else {
        pMixer->ownsAccumulationBuffer = MA_FALSE;
    }

    return MA_SUCCESS;
}

MA_API void ma_mixer_uninit(ma_mixer* pMixer)
{
    if (pMixer == NULL) {
        return;
    }

    if (pMixer->ownsAccumulationBuffer) {
        ma__free_from_callbacks(pMixer->pAccumulationBuffer, &pMixer->allocationCallbacks);
    }
}

MA_API ma_result ma_mixer_begin(ma_mixer* pMixer, ma_mixer* pParentMixer, ma_uint64* pFrameCountOut, ma_uint64* pFrameCountIn)
{
    ma_uint64 frameCountOut;
    ma_uint64 frameCountIn;

    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pMixer->mixingState.isInsideBeginEnd == MA_TRUE) {
        return MA_INVALID_OPERATION;    /* Cannot call this while already inside a begin/end pair. */
    }

    /* If we're submixing we need to make the frame counts compatible with the parent mixer. */
    if (pParentMixer != NULL) {
        /* The output frame count must match the input frame count of the parent. If this cannot be accommodated we need to fail. */
        frameCountOut = pParentMixer->mixingState.frameCountIn;
        frameCountIn  = frameCountOut;
    } else {
        if (pFrameCountOut == NULL) {
            return MA_INVALID_ARGS; /* The desired output frame count is required for a root level mixer. */
        }

        frameCountOut = *pFrameCountOut;
    }

    if (pMixer->pEffect != NULL) {
        frameCountIn = ma_effect_get_required_input_frame_count(pMixer->pEffect, frameCountOut);
        if (frameCountIn > pMixer->accumulationBufferSizeInFrames) {
            /*
            The required number of input frames for the requested number of output frames is too much to fit in the accumulation buffer. We need
            to reduce the output frame count to accommodate.
            */
            ma_uint64 newFrameCountOut;
            newFrameCountOut = ma_effect_get_expected_output_frame_count(pMixer->pEffect, pMixer->accumulationBufferSizeInFrames);
            MA_ASSERT(newFrameCountOut < frameCountOut);

            frameCountOut = newFrameCountOut;
            frameCountIn  = pMixer->accumulationBufferSizeInFrames;
        }
    } else {
        frameCountIn = frameCountOut;
    }

    /* If the output frame count cannot match the parent's input frame count we need to fail. */
    if (pParentMixer != NULL && frameCountOut != pParentMixer->mixingState.frameCountIn) {
        return MA_INVALID_OPERATION;    /* Not compatible with the parent mixer. */
    }

    pMixer->mixingState.isInsideBeginEnd = MA_TRUE;
    pMixer->mixingState.frameCountOut    = frameCountOut;
    pMixer->mixingState.frameCountIn     = frameCountIn;

    ma_zero_memory_64(pMixer->pAccumulationBuffer, frameCountIn * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels));

    if (pFrameCountOut != NULL) {
        *pFrameCountOut = frameCountOut;
    }
    if (pFrameCountIn != NULL) {
        *pFrameCountIn  = frameCountIn;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_end(ma_mixer* pMixer, ma_mixer* pParentMixer, void* pFramesOut)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    /* It's an error for both pParentMixer and pFramesOut to be NULL. */
    if (pParentMixer == NULL && pFramesOut == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If both pParentMixer and pFramesOut are both non-NULL, it indicates an error on the callers side. Make sure they're aware of it. */
    if (pParentMixer != NULL && pFramesOut != NULL) {
        MA_ASSERT(MA_FALSE);
        return MA_INVALID_ARGS;
    }

    if (pMixer->mixingState.isInsideBeginEnd == MA_FALSE) {
        return MA_INVALID_OPERATION;    /* No matching begin. */
    }

    /* Completely different paths if we're outputting to a parent mixer rather than directly to an output buffer. */
    if (pParentMixer != NULL) {
        ma_format localFormatOut;
        ma_uint32 localChannelsOut;
        ma_format parentFormatIn;
        ma_uint32 parentChannelsIn;

        /*
        We need to accumulate the output of pMixer straight into the accumulation buffer of pParentMixer. If the output format of pMixer is different
        to the input format of pParentMixer it needs to be converted.
        */
        ma_mixer_get_output_data_format(pMixer, &localFormatOut, &localChannelsOut);
        ma_mixer_get_input_data_format(pParentMixer, &parentFormatIn, &parentChannelsIn);

        /* A reminder that the output frame count of pMixer must match the input frame count of pParentMixer. */
        MA_ASSERT(pMixer->mixingState.frameCountOut == pParentMixer->mixingState.frameCountIn);

        if (pMixer->pEffect == NULL) {
            /* No effect. Input needs to come straight from the accumulation buffer. */
            ma_mix_accumulation_buffers_ex(pParentMixer->pAccumulationBuffer, parentFormatIn, parentChannelsIn, pMixer->pAccumulationBuffer, localFormatOut, localChannelsOut, pMixer->mixingState.frameCountOut, pMixer->volume);
        } else {
            /* With effect. Input needs to be pre-processed from the effect. */
            ma_volume_and_clip_and_effect_pcm_frames(pParentMixer->pAccumulationBuffer, parentFormatIn, parentChannelsIn, pParentMixer->mixingState.frameCountIn, pMixer->pAccumulationBuffer, pMixer->format, pMixer->channels, pMixer->mixingState.frameCountIn, pMixer->volume, pMixer->pEffect, /*isAccumulation*/ MA_TRUE);
        }
    } else {
        /* We're not submixing so we can overwite. */
        if (pMixer->pEffect == NULL) {
            /* All we need to do is convert the accumulation buffer to the output format. */
            ma_volume_and_clip_pcm_frames(pFramesOut, pMixer->pAccumulationBuffer, pMixer->mixingState.frameCountOut, pMixer->format, pMixer->channels, pMixer->volume);
        } else {
            /* We need to run our accumulation through the effect. */
            ma_volume_and_clip_and_effect_pcm_frames(pFramesOut, pMixer->format, pMixer->channels, pMixer->mixingState.frameCountOut, pMixer->pAccumulationBuffer, pMixer->format, pMixer->channels, pMixer->mixingState.frameCountIn, pMixer->volume, pMixer->pEffect, /*isAccumulation*/ MA_FALSE);
        }
    }

    pMixer->mixingState.isInsideBeginEnd = MA_FALSE;
    pMixer->mixingState.frameCountOut    = 0;
    pMixer->mixingState.frameCountIn     = 0;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_mix_pcm_frames(ma_mixer* pMixer, const void* pFramesIn, ma_uint64 frameCountIn, float volume, ma_format formatIn, ma_uint32 channelsIn)
{
    if (pMixer == NULL || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    if (frameCountIn > pMixer->mixingState.frameCountIn) {
        return MA_INVALID_ARGS; /* Passing in too many input frames. */
    }

    ma_mix_pcm_frames(pMixer->pAccumulationBuffer, pFramesIn, frameCountIn, formatIn, channelsIn, volume);

    return MA_SUCCESS;
}

static ma_result ma_mixer_mix_data_source_mmap(ma_mixer* pMixer, ma_data_source* pDataSource, ma_uint64 frameCount, float volume, ma_effect* pEffect, ma_format formatIn, ma_uint32 channelsIn, ma_bool32 loop)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 totalFramesProcessed = 0;
    void* pRunningAccumulationBuffer = pMixer->pAccumulationBuffer;
    ma_bool32 preEffectConversionRequired = MA_FALSE;
    ma_format effectFormatIn = ma_format_unknown;
    ma_uint32 effectChannelsIn = 0;
    ma_format effectFormatOut = ma_format_unknown;
    ma_uint32 effectChannelsOut = 0;

    MA_ASSERT(pMixer      != NULL);
    MA_ASSERT(pDataSource != NULL);

    if (frameCount > pMixer->mixingState.frameCountIn) {
        return MA_INVALID_ARGS; /* Passing in too many input frames. */
    }

    if (pEffect != NULL) {
        /* We need to know the effect's input and output data format before we'll be able to apply it properly. */
        result = ma_effect_get_input_data_format(pEffect, &effectFormatIn, &effectChannelsIn, NULL);
        if (result != MA_SUCCESS) {
            return result;
        }

        result = ma_effect_get_output_data_format(pEffect, &effectFormatOut, &effectChannelsOut, NULL);
        if (result != MA_SUCCESS) {
            return result;
        }

        preEffectConversionRequired = (formatIn != effectFormatIn || channelsIn != effectChannelsIn);
    }

    while (totalFramesProcessed < frameCount) {
        void* pMappedBuffer;
        ma_uint64 framesToProcess = frameCount - totalFramesProcessed;

        if (pEffect == NULL) {
            /* Fast path. Mix directly from the data source and don't bother applying an effect. */
            result = ma_data_source_map(pDataSource, &pMappedBuffer, &framesToProcess);
            if (framesToProcess == 0) {
                break;  /* Wasn't able to map any data. Abort. */
            }

            ma_mix_pcm_frames_ex(pRunningAccumulationBuffer, pMixer->format, pMixer->channels, pMappedBuffer, formatIn, channelsIn, framesToProcess, volume);

            result = ma_data_source_unmap(pDataSource, framesToProcess);
        } else {
            /* Slow path. Need to apply an effect. This requires the use of an intermediary buffer. */
            ma_uint8 effectInBuffer [MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint8 effectOutBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint32 effectInBufferCap  = sizeof(effectInBuffer)  / ma_get_bytes_per_frame(effectFormatIn,  effectChannelsIn);
            ma_uint32 effectOutBufferCap = sizeof(effectOutBuffer) / ma_get_bytes_per_frame(effectFormatOut, effectChannelsOut);
            ma_uint64 framesMapped;

            if (framesToProcess > effectOutBufferCap) {
                framesToProcess = effectOutBufferCap;
            }

            framesMapped = ma_effect_get_required_input_frame_count(pEffect, framesToProcess);
            if (framesMapped > effectInBufferCap) {
                framesMapped = effectInBufferCap;
            }

            /* We need to map our input data first. The input data will be either fed directly into the effect, or will be converted first. */
            result = ma_data_source_map(pDataSource, &pMappedBuffer, &framesMapped);
            if (result != MA_SUCCESS) {
                return result;
            }

            /* We have the data from the data source so no we can apply the effect. */
            if (preEffectConversionRequired == MA_FALSE) {
                /* Fast path. No format required before applying the effect. */
                ma_effect_process_pcm_frames(pEffect, pMappedBuffer, &framesMapped, effectOutBuffer, &framesToProcess);
            } else {
                /* Slow path. Need to convert the data before applying the effect. */
                ma_convert_pcm_frames_format_and_channels(effectInBuffer, effectFormatIn, effectChannelsIn, pMappedBuffer, formatIn, channelsIn, framesMapped, ma_dither_mode_none);
                ma_effect_process_pcm_frames(pEffect, effectInBuffer, &framesMapped, effectOutBuffer, &framesToProcess);
            }

            /* The effect has been applied so now we can mix it. */
            ma_mix_pcm_frames_ex(pRunningAccumulationBuffer, pMixer->format, pMixer->channels, effectOutBuffer, effectFormatOut, effectChannelsOut, framesToProcess, volume);

            /* We're finished with the input data. */
            result = ma_data_source_unmap(pDataSource, framesMapped);   /* Do this last because the result code is used below to determine whether or not we need to loop. */
        }

        totalFramesProcessed += framesToProcess;
        pRunningAccumulationBuffer = ma_offset_ptr(pRunningAccumulationBuffer, framesToProcess * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels));

        if (result != MA_SUCCESS) {
            if (result == MA_AT_END) {
                if (loop) {
                    ma_data_source_seek_to_pcm_frame(pDataSource, 0);
                    result = MA_SUCCESS;    /* Make sure we don't return MA_AT_END which will happen if we conicidentally hit the end of the data source at the same time as we finish outputting. */
                } else {
                    break;  /* We've reached the end and we're not looping. */
                }
            } else {
                return result;  /* An error occurred. */
            }
        }
    }

    return result;
}

static ma_result ma_mixer_mix_data_source_read(ma_mixer* pMixer, ma_data_source* pDataSource, ma_uint64 frameCount, float volume, ma_effect* pEffect, ma_format formatIn, ma_uint32 channelsIn, ma_bool32 loop)
{
    ma_result result = MA_SUCCESS;
    ma_uint8  preMixBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
    ma_uint32 preMixBufferCap;
    ma_uint64 totalFramesProcessed = 0;
    void* pRunningAccumulationBuffer = pMixer->pAccumulationBuffer;
    ma_format effectFormatIn = ma_format_unknown;
    ma_uint32 effectChannelsIn = 0;
    ma_format preMixFormat = ma_format_unknown;
    ma_uint32 preMixChannels = 0;
    ma_bool32 preEffectConversionRequired = MA_FALSE;

    MA_ASSERT(pMixer      != NULL);
    MA_ASSERT(pDataSource != NULL);

    if (frameCount > pMixer->mixingState.frameCountIn) {
        return MA_INVALID_ARGS; /* Passing in too many input frames. */
    }

    if (pEffect == NULL) {
        preMixFormat   = formatIn;
        preMixChannels = channelsIn;
    } else {
        /* We need to know the effect's input and output data format before we'll be able to apply it properly. */
        result = ma_effect_get_input_data_format(pEffect, &effectFormatIn, &effectChannelsIn, NULL);
        if (result != MA_SUCCESS) {
            return result;
        }

        result = ma_effect_get_output_data_format(pEffect, &preMixFormat, &preMixChannels, NULL);
        if (result != MA_SUCCESS) {
            return result;
        }

        preEffectConversionRequired = (formatIn != effectFormatIn || channelsIn != effectChannelsIn);
    }

    preMixBufferCap = sizeof(preMixBuffer) / ma_get_bytes_per_frame(preMixFormat, preMixChannels);

    totalFramesProcessed = 0;
    pRunningAccumulationBuffer = pMixer->pAccumulationBuffer;

    while (totalFramesProcessed < frameCount) {
        ma_uint64 framesRead;
        ma_uint64 framesToRead = frameCount - totalFramesProcessed;
        if (framesToRead > preMixBufferCap) {
            framesToRead = preMixBufferCap;
        }

        if (pEffect == NULL) {
            result = ma_data_source_read_pcm_frames(pDataSource, preMixBuffer, framesToRead, &framesRead, loop);
            ma_mix_pcm_frames_ex(pRunningAccumulationBuffer, pMixer->format, pMixer->channels, preMixBuffer, formatIn, channelsIn, framesRead, volume);
        } else {
            ma_uint8 callbackBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint8 effectInBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint32 callbackBufferCap = sizeof(callbackBuffer) / ma_get_bytes_per_frame(formatIn, channelsIn);
            ma_uint32 effectInBufferCap = sizeof(effectInBuffer) / ma_get_bytes_per_frame(effectFormatIn, effectChannelsIn);
            ma_uint64 effectFrameCountOut;
            ma_uint64 effectFrameCountIn;
            ma_uint64 framesReadFromCallback;
            ma_uint64 framesToReadFromCallback = ma_effect_get_required_input_frame_count(pEffect, framesToRead);
            if (framesToReadFromCallback > callbackBufferCap) {
                framesToReadFromCallback = callbackBufferCap;
            }
            if (framesToReadFromCallback > effectInBufferCap) {
                framesToReadFromCallback = effectInBufferCap;
            }

            /*
            We can now read some data from the callback. We should never read more input frame than will be consumed. If the format of the callback is the same as the effect's input
            format we can save ourselves a copy and run on a slightly faster path.
            */
            if (preEffectConversionRequired == MA_FALSE) {
                /* Fast path. No need for conversion between the callback and the  */
                result = ma_data_source_read_pcm_frames(pDataSource, effectInBuffer, framesToReadFromCallback, &framesReadFromCallback, loop);
            } else {
                /* Slow path. Conversion between the callback and the effect required. */
                result = ma_data_source_read_pcm_frames(pDataSource, callbackBuffer, framesToReadFromCallback, &framesReadFromCallback, loop);
                ma_convert_pcm_frames_format_and_channels(effectInBuffer, effectFormatIn, effectChannelsIn, callbackBuffer, formatIn, channelsIn, framesReadFromCallback, ma_dither_mode_none);
            }

            /* We have our input data for the effect so now we just process as much as we can based on our input and output frame counts. */
            effectFrameCountIn  = framesReadFromCallback;
            effectFrameCountOut = framesToRead;
            ma_effect_process_pcm_frames(pEffect, effectInBuffer, &effectFrameCountIn, preMixBuffer, &effectFrameCountOut);

            /* At this point the effect should be applied and we can mix it. */
            framesRead = (ma_uint32)effectFrameCountOut;    /* Safe cast. */
            ma_mix_pcm_frames_ex(pRunningAccumulationBuffer, pMixer->format, pMixer->channels, preMixBuffer, preMixFormat, preMixChannels, framesRead, volume);

            /* An emergency failure case. Abort if we didn't consume any input nor any output frames. */
            if (framesRead == 0 && framesReadFromCallback == 0) {
                break;
            }
        }

        totalFramesProcessed += framesRead;
        pRunningAccumulationBuffer = ma_offset_ptr(pRunningAccumulationBuffer, framesRead * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels));

        /* If the data source is busy we need to end mixing now. */
        if (result == MA_BUSY || result == MA_AT_END) {
            break;
        }
    }

    return result;
}

MA_API ma_result ma_mixer_mix_data_source(ma_mixer* pMixer, ma_data_source* pDataSource, ma_uint64 frameCountIn, float volume, ma_effect* pEffect, ma_bool32 loop)
{
    ma_result result;
    ma_format formatIn;
    ma_uint32 channelsIn;
    ma_bool32 supportsMMap = MA_FALSE;
    ma_data_source_callbacks* pDataSourceCallbacks = (ma_data_source_callbacks*)pDataSource;

    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }
    
    result = ma_data_source_get_data_format(pDataSource, &formatIn, &channelsIn);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* Use memory mapping if it's available. */
    if (pDataSourceCallbacks->onMap != NULL && pDataSourceCallbacks->onUnmap != NULL) {
        supportsMMap = MA_TRUE;
    }

    if (supportsMMap) {
        /* Fast path. This is memory mapping mode. */
        return ma_mixer_mix_data_source_mmap(pMixer, pDataSourceCallbacks, frameCountIn, volume, pEffect, formatIn, channelsIn, loop);
    } else {
        /* Slow path. This is reading mode. */
        return ma_mixer_mix_data_source_read(pMixer, pDataSourceCallbacks, frameCountIn, volume, pEffect, formatIn, channelsIn, loop);
    }
}


typedef struct
{
    ma_data_source_callbacks ds;
    ma_rb* pRB;
    ma_format format;
    ma_uint32 channels;
    void* pMappedBuffer;
} ma_rb_data_source;

static ma_result ma_rb_data_source__on_map(ma_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount)
{
    ma_rb_data_source* pRB = (ma_rb_data_source*)pDataSource;
    ma_result result;
    ma_uint32 bpf = ma_get_bytes_per_frame(pRB->format, pRB->channels);
    size_t sizeInBytes;

    sizeInBytes = (size_t)(*pFrameCount * bpf);
    result = ma_rb_acquire_read(pRB->pRB, &sizeInBytes, ppFramesOut);
    *pFrameCount = sizeInBytes / bpf;

    pRB->pMappedBuffer = *ppFramesOut;

    return result;
}

static ma_result ma_rb_data_source__on_unmap(ma_data_source* pDataSource, ma_uint64 frameCount)
{
    ma_rb_data_source* pRB = (ma_rb_data_source*)pDataSource;
    ma_result result;
    ma_uint32 bpf = ma_get_bytes_per_frame(pRB->format, pRB->channels);
    size_t sizeInBytes;

    sizeInBytes = (size_t)(frameCount * bpf);
    result = ma_rb_commit_read(pRB->pRB, sizeInBytes, pRB->pMappedBuffer);

    pRB->pMappedBuffer = NULL;

    return result;  /* We never actually return MA_AT_END here because a ring buffer doesn't have any notion of an end. */
}

static ma_result ma_rb_data_source__on_get_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels)
{
    ma_rb_data_source* pRB = (ma_rb_data_source*)pDataSource;

    *pFormat   = pRB->format;
    *pChannels = pRB->channels;

    return MA_SUCCESS;
}

static ma_result ma_rb_data_source_init(ma_rb* pRB, ma_format format, ma_uint32 channels, ma_rb_data_source* pDataSource)
{
    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    pDataSource->ds.onRead          = NULL;
    pDataSource->ds.onSeek          = NULL;  /* We can't really seek in a ring buffer - there's no notion of a beginning and an end in a ring buffer. */
    pDataSource->ds.onMap           = ma_rb_data_source__on_map;
    pDataSource->ds.onUnmap         = ma_rb_data_source__on_unmap;
    pDataSource->ds.onGetDataFormat = ma_rb_data_source__on_get_format;
    pDataSource->pRB                = pRB;
    pDataSource->format             = format;
    pDataSource->channels           = channels;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_mix_rb(ma_mixer* pMixer, ma_rb* pRB, ma_uint64 frameCountIn, float volume, ma_effect* pEffect, ma_format formatIn, ma_uint32 channelsIn)
{
    /* Ring buffer mixing can be implemented in terms of a memory mapped data source. */
    ma_rb_data_source ds;
    ma_rb_data_source_init(pRB, formatIn, channelsIn, &ds); /* Will never fail and does not require an uninit() implementation. */

    return ma_mixer_mix_data_source(pMixer, &ds, frameCountIn, volume, pEffect, MA_TRUE);   /* Ring buffers always loop, but the loop parameter will never actually be used because ma_rb_data_source__on_unmap() will never return MA_AT_END. */
}

MA_API ma_result ma_mixer_mix_pcm_rb(ma_mixer* pMixer, ma_pcm_rb* pRB, ma_uint64 frameCountIn, float volume, ma_effect* pEffect)
{
    return ma_mixer_mix_rb(pMixer, &pRB->rb, frameCountIn, volume, pEffect, pRB->format, pRB->channels);
}


MA_API ma_result ma_mixer_set_volume(ma_mixer* pMixer, float volume)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (volume < 0.0f || volume > 1.0f) {
        return MA_INVALID_ARGS;
    }

    pMixer->volume = volume;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_get_volume(ma_mixer* pMixer, float* pVolume)
{
    if (pVolume == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pMixer == NULL) {
        *pVolume = 0;
        return MA_INVALID_ARGS;
    }

    *pVolume = pMixer->volume;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_set_gain_db(ma_mixer* pMixer, float gainDB)
{
    if (gainDB > 0) {
        return MA_INVALID_ARGS;
    }

    return ma_mixer_set_volume(pMixer, ma_gain_db_to_factor(gainDB));
}

MA_API ma_result ma_mixer_get_gain_db(ma_mixer* pMixer, float* pGainDB)
{
    float factor;
    ma_result result;

    if (pGainDB == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_mixer_get_volume(pMixer, &factor);
    if (result != MA_SUCCESS) {
        *pGainDB = 0;
        return result;
    }

    *pGainDB = ma_factor_to_gain_db(factor);

    return MA_SUCCESS;
}


MA_API ma_result ma_mixer_set_effect(ma_mixer* pMixer, ma_effect* pEffect)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pMixer->pEffect == pEffect) {
        return MA_SUCCESS;  /* No-op. */
    }

    /* The effect cannot be changed if we're in the middle of a begin/end pair. */
    if (pMixer->mixingState.isInsideBeginEnd) {
        return MA_INVALID_OPERATION;
    }

    pMixer->pEffect = pEffect;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_get_effect(ma_mixer* pMixer, ma_effect** ppEffect)
{
    if (ppEffect == NULL) {
        return MA_INVALID_ARGS;
    }

    *ppEffect = NULL;   /* Safety. */

    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    *ppEffect = pMixer->pEffect;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_get_output_data_format(ma_mixer* pMixer, ma_format* pFormat, ma_uint32* pChannels)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If we have an effect, the output data format will be the effect's output data format. */
    if (pMixer->pEffect != NULL) {
        return ma_effect_get_output_data_format(pMixer->pEffect, pFormat, pChannels, NULL);
    } else {
        if (pFormat != NULL) {
            *pFormat = pMixer->format;
        }

        if (pChannels != NULL) {
            *pChannels = pMixer->channels;
        }

        return MA_SUCCESS;
    }
}

MA_API ma_result ma_mixer_get_input_data_format(ma_mixer* pMixer, ma_format* pFormat, ma_uint32* pChannels)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pFormat != NULL) {
        *pFormat = pMixer->format;
    }

    if (pChannels != NULL) {
        *pChannels = pMixer->channels;
    }

    return MA_SUCCESS;
}

#endif