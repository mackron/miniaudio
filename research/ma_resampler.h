/*
Consider this code public domain.

This is research into a new resampler for miniaudio. Not yet complete.

Requirements:
- Selection of different algorithms. The following at a minimum:
  - Linear with optional filtering
  - Sinc
- Floating point pipeline for f32 and fixed point integer pipeline for s16
  - Specify a ma_format enum as a config at initialization time, but fail if it's anything other than f32 or s16
- Need ability to move time forward without processing any samples
  - Needs an option to handle the cache as if silent samples of 0 have been passed as input
  - Needs option to move time forward by output sample rate _or_ input sample rate
- Need to be able to do the equivalent to a seek by passing in NULL to the read API()
  - ma_resampler_read(pResampler, frameCount, NULL) = ma_resampler_seek(pResampler, frameCount, 0)
- Need to be able to query the number of output PCM frames that can be generated from the currently cached input. The
  returned value must be fractional. Likewise, must be able to query the number of cached input PCM frames and must
  also be fractional.
- Need to be able to query exactly how many output PCM frames the user would get if they requested a certain number
  input frames. Likewise, need to be able to query how many input PCM frames are required for a certain number of
  output frames.
- Must support dynamic changing of the sample rate, both by input/output rate and by ratio
  - Each read and seek function for each algorithm must handle a ratio of 1 in a fast path
- Must have different modes on how to handle the last of the input samples. Certain situations (streaming) requires
  the last input samples to be cached in the internal structure for the windowing algorithm. Other situations require
  all of the input samples to be consumed in order to output the correct total sample count.
- Need to support converting input samples directly passed in as parameters without using a callback.
  - ma_resampler_read(pResampler, &inputFrameCount, pInputFrames, &outputFrameCount, pOutputFrames). Returns a
    result code. inputFrameCount and outputFrameCount are both input and output.
- Need to support using a ring buffer as the backing data.
  - ma_resampler_read_from_pcm_rb(pResampler, frameCount, pFramesOut, &ringBuffer). May need an option to control
    how to handle underruns - should it stop processing or should it pad with zeroes?
- Need to support reading from a callback.
  - ma_resampler_read_from_callback(pResampler, frameCount, pFramesOut, resampler_callback, pUserData)


Other Notes:
- I've had a bug in the past where a single call to read() returns too many samples. It essentially computes more
  samples than the input data would allow. The input data would get consumed, but output samples would continue to
  get computed up to the requested frame count, filling in the end with zeroes. This is completely wrong because
  the return value needs to be used to know whether or not the end of the input has been reached.


Random Notes:
- You cannot change the algorithm after initialization.
- It is recommended to keep the ma_resampler object aligned to MA_SIMD_ALIGNMENT, though it is not necessary.
- Ratios need to be in the range of MA_RESAMPLER_MIN_RATIO and MA_RESAMPLER_MAX_RATIO. This is enough to convert
  to and from 8000 and 384000, which is the smallest and largest standard rates supported by miniaudio. If you need
  extreme ratios then you will need to chain resamplers together.
*/
#ifndef ma_resampler_h
#define ma_resampler_h

#define MA_RESAMPLER_SEEK_NO_CLIENT_READ            (1 << 0)    /* When set, does not read anything from the client when seeking. This does _not_ call onRead(). */
#define MA_RESAMPLER_SEEK_INPUT_RATE                (1 << 1)    /* When set, treats the specified frame count based on the input sample rate rather than the output sample rate. */

#ifndef MA_RESAMPLER_CACHE_SIZE_IN_BYTES
#define MA_RESAMPLER_CACHE_SIZE_IN_BYTES            4096
#endif

#ifndef MA_RESAMPLER_MAX_WINDOW_WIDTH_IN_FRAMES
#define MA_RESAMPLER_MAX_WINDOW_WIDTH_IN_FRAMES     32
#endif

#ifndef MA_RESAMPLER_MAX_WINDOW_WIDTH_IN_BYTES
#define MA_RESAMPLER_MAX_WINDOW_WIDTH_IN_BYTES      (4*MA_MAX_CHANNELS*MA_RESAMPLER_MAX_WINDOW_WIDTH_IN_FRAMES)
#endif

typedef struct ma_resampler ma_resampler;
typedef enum ma_resampler_seek_mode ma_resampler_seek_mode;

/* Client callbacks. */
typedef ma_uint32 (* ma_resampler_read_from_client_proc)(ma_resampler* pResampler, ma_uint32 frameCount, void** ppFrames);

/* Backend functions. */
typedef ma_result (* ma_resampler_init_proc)    (ma_resampler* pResampler);
typedef ma_result (* ma_resampler_process_proc) (ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_uint64* pFrameCountIn, void** ppFramesIn, ma_resampler_seek_mode seekMode);

typedef ma_uint64 (* ma_resampler_read_f32_proc)(ma_resampler* pResampler, ma_uint64 frameCount, float** ppFrames);
typedef ma_uint64 (* ma_resampler_read_s16_proc)(ma_resampler* pResampler, ma_uint64 frameCount, ma_int16** ppFrames);
typedef ma_uint64 (* ma_resampler_seek_proc)    (ma_resampler* pResampler, ma_uint64 frameCount, ma_uint32 options);

typedef enum
{
    ma_resampler_algorithm_linear = 0,  /* Default. Fastest. */
    ma_resampler_algorithm_sinc         /* Slower. */
} ma_resampler_algorithm;

typedef enum
{
    ma_resampler_end_of_input_mode_consume = 0,     /* When the end of the input stream is reached, consume the last input PCM frames (do not leave them in the internal cache). Default. */
    ma_resampler_end_of_input_mode_no_consume       /* When the end of the input stream is reached, do _not_ consume the last input PCM frames (leave them in the internal cache). Use this in streaming situations. */
} ma_resampler_end_of_input_mode;

enum ma_resampler_seek_mode
{
    ma_resampler_seek_mode_none = 0,                /* No seeking (normal read). */
    ma_resampler_seek_mode_output,                  /* Seek by output rate. */
    ma_resampler_seek_mode_input                    /* Seek by input rate. */
};

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRateIn;
    ma_uint32 sampleRateOut;
    double ratio;    /* ratio = in/out */
    ma_resampler_algorithm algorithm;
    ma_resampler_end_of_input_mode endOfInputMode;
    ma_stream_layout layout;   /* Interleaved or deinterleaved. */
    ma_resampler_read_from_client_proc onRead;
    void* pUserData;
} ma_resampler_config;

struct ma_resampler
{
    union
    {
        float    f32[MA_RESAMPLER_MAX_WINDOW_WIDTH_IN_BYTES/sizeof(float)];
        ma_int16 s16[MA_RESAMPLER_MAX_WINDOW_WIDTH_IN_BYTES/sizeof(ma_int16)];
    } window;                       /* Keep this as the first member of this structure for SIMD alignment purposes. */
    /*ma_uint32 cacheStrideInFrames;*/  /* The number of the samples between channels in the cache. The first sample for channel 0 is cacheStrideInFrames*0. The first sample for channel 1 is cacheStrideInFrames*1, etc. */
    /*ma_uint16 cacheLengthInFrames;*/  /* The number of valid frames sitting in the cache, including the filter window. May be less than the cache's capacity. */
    ma_uint16 windowLength;
    double windowTime;              /* By input rate. Relative to the start of the cache. */
    ma_resampler_config config;
    ma_resampler_init_proc init;
    ma_resampler_process_proc process;
    ma_resampler_read_f32_proc readF32;
    ma_resampler_read_s16_proc readS16;
    ma_resampler_seek_proc seek;
};

/*
Initializes a new resampler object from a config.
*/
ma_result ma_resampler_init(const ma_resampler_config* pConfig, ma_resampler* pResampler);

/*
Uninitializes the given resampler.
*/
void ma_resampler_uninit(ma_resampler* pResampler);

/*
Dynamically adjusts the sample rate.
*/
ma_result ma_resampler_set_rate(ma_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut);

/*
Dynamically adjusts the sample rate by a ratio.

The ratio is in/out.
*/
ma_result ma_resampler_set_rate_ratio(ma_resampler* pResampler, double ratio);

/*
Converts the given input data.

On input, [pFrameCountOut] contains the number of output frames to process. On output it contains the number of output frames that
were actually processed, which may be less than the requested amount which will happen if there's not enough input data. You can use
ma_resampler_get_expected_output_frame_count() to know how many output frames will be processed for a given number of input frames.

On input, [pFrameCountIn] contains the number of input frames contained in [ppFramesIn]. On output it contains the number of whole
input frames that were actually processed. You can use ma_resampler_get_required_input_frame_count() to know how many input frames
you should provide for a given number of output frames.

You can pass NULL to [ppFramesOut], in which case a seek will be performed. When [pFrameCountOut] is not NULL, it will seek past that
number of output frames. Otherwise, if [pFrameCountOut] is NULL and [pFrameCountIn] is not NULL, it will seek by the specified number
of input frames. When seeking, [ppFramesIn] is allowed to NULL, in which case the internal timing state will be updated, but no input
will be processed.

It is an error for both [pFrameCountOut] and [pFrameCountIn] to be NULL.
*/
ma_result ma_resampler_process(ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_uint64* pFrameCountIn, void** ppFramesIn);
ma_result ma_resampler_process_callback(ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_resampler_read_from_client_proc onRead, void* pUserData);


/*
Reads a number of PCM frames from the resampler.

Passing in NULL for ppFrames is equivalent to calling ma_resampler_seek(pResampler, frameCount, 0).
*/
/*ma_uint64 ma_resampler_read(ma_resampler* pResampler, ma_uint64 frameCount, void** ppFrames);
ma_uint64 ma_resampler_read_callbacks(ma_resampler* pResampler, ma_uint64 frameCount, void** pFrames, ma_resampler_read_from_client_proc onRead, void* pUserData);*/

/*
Seeks forward by the specified number of PCM frames.

"options" can be a cobination of the following:
    MA_RESAMPLER_SEEK_NO_CLIENT_READ
        Leaves the contents of the internal cached undefined instead of reading in data from the onRead callback.
    MA_RESAMPLER_SEEK_INPUT_RATE
        Treats "frameCount" as input samples instead of output samples.
*/
/*ma_uint64 ma_resampler_seek(ma_resampler* pResampler, ma_uint64 frameCount, ma_uint32 options);*/

/*
Retrieves the number of cached input frames.

This is equivalent to: (ma_uint64)ceil(ma_resampler_get_cached_input_time(pResampler));
*/
ma_uint64 ma_resampler_get_cached_input_frame_count(ma_resampler* pResampler);

/*
Retrieves the number of whole output frames that can be calculated from the currently cached input frames.

This is equivalent to: (ma_uint64)floor(ma_resampler_get_cached_output_time(pResampler));
*/
ma_uint64 ma_resampler_get_cached_output_frame_count(ma_resampler* pResampler);

/*
The same as ma_resampler_get_cached_input_frame_count(), except returns a fractional value representing the exact amount
of time in input rate making up the cached input.

When the end of input mode is set to ma_resampler_end_of_input_mode_no_consume, the input frames currently sitting in the
window are not included in the calculation.

This can return a negative value if nothing has yet been loaded into the internal cache. This will happen if this is called
immediately after initialization, before the first read has been performed. It may also happen if only a few samples have
been read from the client.
*/
double ma_resampler_get_cached_input_time(ma_resampler* pResampler);

/*
The same as ma_resampler_get_cached_output_frame_count(), except returns a fractional value representing the exact amount
of time in output rate making up the cached output.

When the end of input mode is set to ma_resampler_end_of_input_mode_no_consume, the input frames currently sitting in the
window are not included in the calculation.

This can return a negative value. See ma_resampler_get_cached_input_time() for details.
*/
double ma_resampler_get_cached_output_time(ma_resampler* pResampler);

/*
Calculates the number of whole input frames that would need to be read from the client in order to output the specified
number of output frames.

The returned value does not include cached input frames. It only returns the number of extra frames that would need to be
read from the client in order to output the specified number of output frames.

When the end of input mode is set to ma_resampler_end_of_input_mode_no_consume, the input frames sitting in the filter
window are not included in the calculation.
*/
ma_uint64 ma_resampler_get_required_input_frame_count(ma_resampler* pResampler, ma_uint64 outputFrameCount);

/*
Calculates the number of whole output frames that would be output after fully reading and consuming the specified number of
input frames from the client.

A detail to keep in mind is how cached input frames are handled. This function calculates the output frame count based on
inputFrameCount + ma_resampler_get_cached_input_time(). It essentially calcualtes how many output frames will be returned
if an additional inputFrameCount frames were read from the client and consumed by the resampler. You can adjust the return
value by ma_resampler_get_cached_output_frame_count() which calculates the number of output frames that can be output from
the currently cached input.

When the end of input mode is set to ma_resampler_end_of_input_mode_no_consume, the input frames sitting in the filter
window are not included in the calculation.
*/
ma_uint64 ma_resampler_get_expected_output_frame_count(ma_resampler* pResampler, ma_uint64 inputFrameCount);
#endif

#ifdef MINIAUDIO_IMPLEMENTATION

#ifndef MA_RESAMPLER_MIN_RATIO
#define MA_RESAMPLER_MIN_RATIO 0.02083333
#endif
#ifndef MA_RESAMPLER_MAX_RATIO
#define MA_RESAMPLER_MAX_RATIO 48.0
#endif

ma_result ma_resampler_init__linear(ma_resampler* pResampler);
ma_result ma_resampler_process__linear(ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_uint64* pFrameCountIn, void** ppFramesIn, ma_resampler_seek_mode seekMode);
ma_uint64 ma_resampler_read_f32__linear(ma_resampler* pResampler, ma_uint64 frameCount, float** ppFrames);
ma_uint64 ma_resampler_read_s16__linear(ma_resampler* pResampler, ma_uint64 frameCount, ma_int16** ppFrames);
ma_uint64 ma_resampler_seek__linear(ma_resampler* pResampler, ma_uint64 frameCount, ma_uint32 options);

ma_result ma_resampler_init__sinc(ma_resampler* pResampler);
ma_result ma_resampler_process__sinc(ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_uint64* pFrameCountIn, void** ppFramesIn, ma_resampler_seek_mode seekMode);
ma_uint64 ma_resampler_read_f32__sinc(ma_resampler* pResampler, ma_uint64 frameCount, float** ppFrames);
ma_uint64 ma_resampler_read_s16__sinc(ma_resampler* pResampler, ma_uint64 frameCount, ma_int16** ppFrames);
ma_uint64 ma_resampler_seek__sinc(ma_resampler* pResampler, ma_uint64 frameCount, ma_uint32 options);

static MA_INLINE float ma_fractional_part_f32(float x)
{
    return x - ((ma_int32)x);
}

static MA_INLINE double ma_fractional_part_f64(double x)
{
    return x - ((ma_int64)x);
}

#if 0
#define MA_ALIGN_INT(val, alignment) (((val) + ((alignment)-1)) & ~((alignment)-1))
#define MA_ALIGN_PTR(ptr, alignment) (void*)MA_ALIGN_INT(((ma_uintptr)(ptr)), (alignment))

/*
This macro declares a set of variables on the stack of a given size in bytes. The variables it creates are:
  - ma_uint8 <name>Unaligned[size + MA_SIMD_ALIGNMENT];
  - <type>* <name>[MA_MAX_CHANNELS];
  - size_t <name>FrameCount;    <-- This is the number of samples contained within each sub-buffer of <name>

This does not work for formats that do not have a clean mapping to a primitive C type. s24 will not work here.
*/
#define MA_DECLARE_ALIGNED_STACK_BUFFER(type, name, size, channels) \
    ma_uint8 name##Unaligned[(size) + MA_SIMD_ALIGNMENT]; \
    type* name[MA_MAX_CHANNELS]; \
    size_t name##FrameCount = ((size) & ~((MA_SIMD_ALIGNMENT)-1)) / sizeof(type); \
    do { \
        ma_uint32 iChannel; \
        for (iChannel = 0; iChannel < channels; ++iChannel) { \
            name[iChannel] = (type*)((ma_uint8*)MA_ALIGN_PTR(name##Unaligned, MA_SIMD_ALIGNMENT) + (iChannel*((size) & ~((MA_SIMD_ALIGNMENT)-1)))); \
        } \
    } while (0)
#endif

#define ma_filter_window_length_left(length)   ((length) >> 1)
#define ma_filter_window_length_right(length)  ((length) - ma_filter_window_length_left(length))

static MA_INLINE ma_uint16 ma_resampler__window_length_left(const ma_resampler* pResampler)
{
    return ma_filter_window_length_left(pResampler->windowLength);
}

static MA_INLINE ma_uint16 ma_resampler__window_length_right(const ma_resampler* pResampler)
{
    return ma_filter_window_length_right(pResampler->windowLength);
}

static MA_INLINE double ma_resampler__calculate_cached_input_time_by_mode(ma_resampler* pResampler, ma_resampler_end_of_input_mode mode)
{
    /*
    The cached input time depends on whether or not the end of the input is being consumed. If so, it's the difference between the
    last cached frame and the halfway point of the window, rounded down. Otherwise it's between the last cached frame and the end
    of the window.
    */
    double cachedInputTime = pResampler->cacheLengthInFrames;
    if (mode == ma_resampler_end_of_input_mode_consume) {
        cachedInputTime -= (pResampler->windowTime + ma_resampler__window_length_left(pResampler));
    } else {
        cachedInputTime -= (pResampler->windowTime + pResampler->windowLength);
    }

    return cachedInputTime;
}

static MA_INLINE double ma_resampler__calculate_cached_input_time(ma_resampler* pResampler)
{
    return ma_resampler__calculate_cached_input_time_by_mode(pResampler, pResampler->config.endOfInputMode);
}

static MA_INLINE double ma_resampler__calculate_cached_output_time_by_mode(ma_resampler* pResampler, ma_resampler_end_of_input_mode mode)
{
    return ma_resampler__calculate_cached_input_time_by_mode(pResampler, mode) / pResampler->config.ratio;
}

static MA_INLINE double ma_resampler__calculate_cached_output_time(ma_resampler* pResampler)
{
    return ma_resampler__calculate_cached_output_time_by_mode(pResampler, pResampler->config.endOfInputMode);
}



static MA_INLINE ma_result ma_resampler__slide_cache_down(ma_resampler* pResampler)
{
    /* This function moves everything from the start of the window to the last loaded frame in the cache down to the front. */

    ma_uint16 framesToConsume;
    framesToConsume = (ma_uint16)pResampler->windowTime;

    pResampler->windowTime          -= framesToConsume;
    pResampler->cacheLengthInFrames -= framesToConsume;

    if (pResampler->config.format == ma_format_f32) {
        for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
            for (ma_uint32 iFrame = 0; iFrame < pResampler->cacheLengthInFrames; ++iFrame) {
                pResampler->cache.f32[pResampler->cacheStrideInFrames*iChannel + iFrame] = pResampler->cache.f32[pResampler->cacheStrideInFrames*iChannel + iFrame + framesToConsume];
            }
        }
    } else {
        for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
            for (ma_uint32 iFrame = 0; iFrame < pResampler->cacheLengthInFrames; ++iFrame) {
                pResampler->cache.s16[pResampler->cacheStrideInFrames*iChannel + iFrame] = pResampler->cache.s16[pResampler->cacheStrideInFrames*iChannel + iFrame + framesToConsume];
            }
        }
    }

    return MA_SUCCESS;
}

typedef union
{
    float* f32[MA_MAX_CHANNELS];
    ma_int16* s16[MA_MAX_CHANNELS];
} ma_resampler_deinterleaved_pointers;

typedef union
{
    float* f32;
    ma_int16* s16;
} ma_resampler_interleaved_pointers;

ma_result ma_resampler__reload_cache(ma_resampler* pResampler, ma_bool32* pLoadedEndOfInput)
{
    /* When reloading the buffer there's a specific rule to keep into consideration. When the client */

    ma_result result;
    ma_uint32 framesToReadFromClient;
    ma_uint32 framesReadFromClient;
    ma_bool32 loadedEndOfInput = MA_FALSE;
    
    ma_assert(pLoadedEndOfInput != NULL);
    ma_assert(pResampler->windowTime <  65536);
    ma_assert(pResampler->windowTime <= pResampler->cacheLengthInFrames);

    /* Before loading anything from the client we need to move anything left in the cache down the front. */
    result = ma_resampler__slide_cache_down(pResampler);
    if (result != MA_SUCCESS) {
        return result;  /* Should never actually happen. */
    }

    /*
    Here is where we calculate the number of samples to read from the client. We always read in slightly less than the capacity of the
    cache. The reason is that the little bit is filled with zero-padding when the end of input is reached. The amount of padding is equal
    to the size of the right side of the filter window.
    */
    if (pResampler->config.format == ma_format_f32) {
        framesToReadFromClient = pResampler->cacheStrideInFrames - ma_resampler__window_length_right(pResampler) - pResampler->cacheLengthInFrames;
    } else {
        framesToReadFromClient = pResampler->cacheStrideInFrames - ma_resampler__window_length_right(pResampler) - pResampler->cacheLengthInFrames;
    }

    /* Here is where we need to read more data from the client. We need to construct some deinterleaved buffers first, though. */
    ma_resampler_deinterleaved_pointers clientDst;
    if (pResampler->config.format == ma_format_f32) {
        for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
            clientDst.f32[iChannel] = pResampler->cache.f32 + (pResampler->cacheStrideInFrames*iChannel + pResampler->cacheLengthInFrames);
        }
                
        if (pResampler->config.layout == ma_stream_layout_deinterleaved) {
            framesReadFromClient = pResampler->config.onRead(pResampler, framesToReadFromClient, clientDst.f32);
        } else {
            float buffer[ma_countof(pResampler->cache.f32)];
            float* pInterleavedFrames = buffer;
            framesReadFromClient = pResampler->config.onRead(pResampler, framesToReadFromClient, &pInterleavedFrames);
            ma_deinterleave_pcm_frames(pResampler->config.format, pResampler->config.channels, framesReadFromClient, pInterleavedFrames, clientDst.f32);
        }
    } else {
        for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
            clientDst.s16[iChannel] = pResampler->cache.s16 + (pResampler->cacheStrideInFrames*iChannel + pResampler->cacheLengthInFrames);
        }

        if (pResampler->config.layout == ma_stream_layout_deinterleaved) {
            framesReadFromClient = pResampler->config.onRead(pResampler, framesToReadFromClient, clientDst.s16);
        } else {
            ma_int16 buffer[ma_countof(pResampler->cache.s16)];
            ma_int16* pInterleavedFrames = buffer;
            framesReadFromClient = pResampler->config.onRead(pResampler, framesToReadFromClient, &pInterleavedFrames);
            ma_deinterleave_pcm_frames(pResampler->config.format, pResampler->config.channels, framesReadFromClient, pInterleavedFrames, clientDst.s16);
        }
    }
            
    ma_assert(framesReadFromClient <= framesToReadFromClient);
    if (framesReadFromClient < framesToReadFromClient) {
        /* We have reached the end of the input buffer. We do _not_ want to attempt to read any more data from the client in this case. */
        loadedEndOfInput = MA_TRUE;
        *pLoadedEndOfInput = loadedEndOfInput;
    }

    ma_assert(framesReadFromClient <= 65535);
    pResampler->cacheLengthInFrames += (ma_uint16)framesReadFromClient;

    /*
    If we just loaded the end of the input and the resampler is configured to consume it, we need to pad the end of the cache with
    silence. This system ensures the last input samples are processed by the resampler. The amount of padding is equal to the length
    of the right side of the filter window.
    */
    if (loadedEndOfInput && pResampler->config.endOfInputMode == ma_resampler_end_of_input_mode_consume) {
        ma_uint16 paddingLengthInFrames = ma_resampler__window_length_right(pResampler);
        if (paddingLengthInFrames > 0) {
            if (pResampler->config.format == ma_format_f32) {
                for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                    ma_zero_memory(pResampler->cache.f32 + (pResampler->cacheStrideInFrames*iChannel + pResampler->cacheLengthInFrames), paddingLengthInFrames*sizeof(float));
                }
            } else {
                for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                    ma_zero_memory(pResampler->cache.s16 + (pResampler->cacheStrideInFrames*iChannel + pResampler->cacheLengthInFrames), paddingLengthInFrames*sizeof(ma_int16));
                }
            }
            
            pResampler->cacheLengthInFrames += paddingLengthInFrames;
        }
    }

    return MA_SUCCESS;
}


ma_result ma_resampler_init(const ma_resampler_config* pConfig, ma_resampler* pResampler)
{
    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }
    ma_zero_object(pResampler);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pResampler->config = *pConfig;
    if (pResampler->config.format != ma_format_f32 && pResampler->config.format != ma_format_s16) {
        return MA_INVALID_ARGS;        /* Unsupported format. */
    }
    if (pResampler->config.channels == 0) {
        return MA_INVALID_ARGS;        /* Unsupported channel count. */
    }
    if (pResampler->config.ratio == 0) {
        if (pResampler->config.sampleRateIn == 0 || pResampler->config.sampleRateOut == 0) {
            return MA_INVALID_ARGS;    /* Unsupported sample rate. */
        }
        pResampler->config.ratio = (double)pResampler->config.sampleRateIn / (double)pResampler->config.sampleRateOut;
    }
    if (pResampler->config.onRead == NULL) {
        return MA_INVALID_ARGS;        /* No input callback specified. */
    }

    switch (pResampler->config.algorithm) {
        case ma_resampler_algorithm_linear:
        {
            pResampler->init    = ma_resampler_init__linear;
            pResampler->process = ma_resampler_process__linear;
            pResampler->readF32 = ma_resampler_read_f32__linear;
            pResampler->readS16 = ma_resampler_read_s16__linear;
            pResampler->seek    = ma_resampler_seek__linear;
        } break;

        case ma_resampler_algorithm_sinc:
        {
            pResampler->init    = ma_resampler_init__sinc;
            pResampler->process = ma_resampler_process__sinc;
            pResampler->readF32 = ma_resampler_read_f32__sinc;
            pResampler->readS16 = ma_resampler_read_s16__sinc;
            pResampler->seek    = ma_resampler_seek__sinc;
        } break;
    }

    if (pResampler->config.format == ma_format_f32) {
        pResampler->cacheStrideInFrames = ma_countof(pResampler->cache.f32) / pResampler->config.channels;
    } else {
        pResampler->cacheStrideInFrames = ma_countof(pResampler->cache.s16) / pResampler->config.channels;
    }

    if (pResampler->init != NULL) {
        ma_result result = pResampler->init(pResampler);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    /*
    After initializing the backend, we'll need to pre-fill the filter with zeroes. This has already been half done via
    the call to ma_zero_object() at the top of this function, but we need to increment the frame counter to complete it.
    */
    pResampler->cacheLengthInFrames = ma_resampler__window_length_left(pResampler);

    return MA_SUCCESS;
}

void ma_resampler_uninit(ma_resampler* pResampler)
{
    (void)pResampler;
}

ma_result ma_resampler_set_rate(ma_resampler* pResampler, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut)
{
    double ratio;

    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    if (sampleRateIn == 0 || sampleRateOut == 0) {
        return MA_INVALID_ARGS;
    }

    ratio = (double)pResampler->config.sampleRateIn / (double)pResampler->config.sampleRateOut;
    if (ratio < MA_RESAMPLER_MIN_RATIO || ratio > MA_RESAMPLER_MAX_RATIO) {
        return MA_INVALID_ARGS;    /* Ratio is too extreme. */
    }

    pResampler->config.sampleRateIn  = sampleRateIn;
    pResampler->config.sampleRateOut = sampleRateOut;
    pResampler->config.ratio         = ratio;

    return MA_SUCCESS;
}

ma_result ma_resampler_set_rate_ratio(ma_resampler* pResampler, double ratio)
{
    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ratio < MA_RESAMPLER_MIN_RATIO || ratio > MA_RESAMPLER_MAX_RATIO) {
        return MA_INVALID_ARGS;    /* Ratio is too extreme. */
    }

    pResampler->config.ratio = ratio;

    return MA_SUCCESS;
}


ma_result ma_resampler_process(ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_uint64* pFrameCountIn, void** ppFramesIn)
{
    ma_resampler_seek_mode seekMode;

    if (pResampler == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ppFramesOut != NULL) {
        /* Normal processing. */
        if (pFrameCountOut == NULL) {
            return MA_INVALID_ARGS; /* Don't have any output frames to process. */
        }
        if (pFrameCountIn == NULL || ppFramesIn == NULL) {
            return MA_INVALID_ARGS; /* Cannot process without any input data. */
        }

        seekMode = ma_resampler_seek_mode_none;
    } else {
        /* Seeking. */
        if (pFrameCountOut != NULL) {
            /* Seeking by output frames. */
            seekMode = ma_resampler_seek_mode_output;
        } else {
            /* Seeking by input frames. */
            if (pFrameCountIn == NULL) {
                return MA_INVALID_ARGS; /* Don't have any input frames to process. */
            }

            seekMode = ma_resampler_seek_mode_input;
        }
    }

    return pResampler->process(pResampler, pFrameCountOut, ppFramesOut, pFrameCountIn, ppFramesIn, seekMode);
}

ma_result ma_resampler_process_callback(ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_resampler_read_from_client_proc onRead, void* pUserData)
{
    union
    {
        float    f32[1024];
        ma_int16 s16[2048];
    } inputBuffer;
    ma_uint64 inputBufferSizeInFrames;
    ma_uint64 outputFramesRemaining;
    ma_result result;
    void* ppRunningFramesOut[MA_MAX_CHANNELS];
    void* ppInputFrames[MA_MAX_CHANNELS];

    if (onRead == NULL) {
        return MA_INVALID_ARGS; /* Does not make sense to call this API without a callback... */
    }

    /* This API always requires an output frame count. */
    if (pFrameCountOut == NULL) {
        return MA_INVALID_ARGS;
    }

    result = MA_SUCCESS;

    inputBufferSizeInFrames = sizeof(inputBuffer)/ma_get_bytes_per_frame(pResampler->config.format, pResampler->config.channels);
    outputFramesRemaining = *pFrameCountOut;

    if (pResampler->config.layout == ma_stream_layout_interleaved) {
        ppRunningFramesOut[0] = ppFramesOut[0];
        ppInputFrames[0] = (void*)&inputBuffer.f32[0];
    } else {
        ma_uint32 iChannel;
        for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
            ppRunningFramesOut[iChannel] = ppFramesOut[iChannel];
        }
        for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
            ppInputFrames[iChannel] = ma_offset_ptr(&inputBuffer.f32[0], inputBufferSizeInFrames * iChannel);
        }
    }
    
    /* Keep reading until every output frame has been processed. */
    for (;;) {
        ma_uint64 outputFrameCount;
        ma_uint64 inputFrameCount;
        ma_uint64 targetInputFrameCount;

        targetInputFrameCount = ma_resampler_get_required_input_frame_count(pResampler, outputFrameCount);
        if (targetInputFrameCount > inputBufferSizeInFrames) {
            targetInputFrameCount = inputBufferSizeInFrames;
        }

        inputFrameCount = onRead(pResampler, targetInputFrameCount, &ppInputFrames[0]); /* Don't check if inputFrameCount is 0 and break from the loop. May want to extract the last bit that's sitting in the window. */

        result = ma_resampler_process(pResampler, &outputFrameCount, &ppRunningFramesOut[0], &inputFrameCount, &ppInputFrames[0]);
        if (result != MA_SUCCESS) {
            break;
        }

        outputFramesRemaining -= outputFrameCount;
        if (outputFramesRemaining == 0) {
            break;
        }

        if (inputFrameCount < targetInputFrameCount) {
            break;  /* Input data has been exhausted. */
        }

        if (pResampler->config.layout == ma_stream_layout_interleaved) {
            ppRunningFramesOut[0] = ma_offset_ptr(ppRunningFramesOut[0], outputFrameCount * ma_get_bytes_per_sample(pResampler->config.format));
        } else {
            ma_uint32 iChannel;
            for (iChannel = 0; iChannel < pResampler->config.channels; iChannel += 1) {
                ppRunningFramesOut[iChannel] = ma_offset_ptr(ppRunningFramesOut[iChannel], outputFrameCount * ma_get_bytes_per_sample(pResampler->config.format));
            }
        }
    }

    *pFrameCountOut = *pFrameCountOut - outputFramesRemaining;

    return result;
}



#if 0
ma_uint64 ma_resampler_read(ma_resampler* pResampler, ma_uint64 frameCount, void** ppFrames)
{
    ma_result result;
    ma_uint64 totalFramesRead;
    ma_resampler_deinterleaved_pointers runningFramesOutDeinterleaved;
    ma_resampler_interleaved_pointers runningFramesOutInterleaved;
    ma_bool32 loadedEndOfInput = MA_FALSE;

    if (pResampler == NULL) {
        return 0;   /* Invalid arguments. */
    }

    if (frameCount == 0) {
        return 0;   /* Nothing to do, so return early. */
    }

    /* When ppFrames is NULL, reading is equivalent to seeking with default options. */
    if (ppFrames == NULL) {
        return ma_resampler_seek(pResampler, frameCount, 0);
    }


    if (pResampler->config.format == ma_format_f32) {
        ma_assert(pResampler->readF32 != NULL);
    } else {
        ma_assert(pResampler->readS16 != NULL);
    }


    /* Initialization of the running frame pointers. */
    if (pResampler->config.layout == ma_stream_layout_deinterleaved) {
        for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
            runningFramesOutDeinterleaved.f32[iChannel] = (float*)ppFrames[iChannel];
        }
        runningFramesOutInterleaved.f32 = NULL; /* Silences a warning. */
    } else {
        runningFramesOutInterleaved.f32 = (float*)ppFrames[0];
    }
    

    /*
    The backend read callbacks are only called for ranges that can be read entirely from cache. This simplifies each backend
    because they do not need to worry about cache reloading logic. Instead we do all of the cache reloading stuff from here.
    */

    totalFramesRead = 0;
    while (totalFramesRead < frameCount) {
        double cachedOutputTime;
        ma_uint64 framesRemaining = frameCount - totalFramesRead;
        ma_uint64 framesToReadRightNow = framesRemaining;

        /* We need to make sure we don't read more than what's already in the buffer at a time. */
        cachedOutputTime = ma_resampler__calculate_cached_output_time_by_mode(pResampler, ma_resampler_end_of_input_mode_no_consume);
        if (cachedOutputTime > 0) {
            if (framesRemaining > cachedOutputTime) {
                framesToReadRightNow = (ma_uint64)floor(cachedOutputTime);
            }

            /* 
            At this point we should know how many frames can be read this iteration. We need an optimization for when the ratio=1
            and the current time is a whole number. In this case we need to do a direct copy without any processing.
            */
            if (pResampler->config.ratio == 1 && ma_fractional_part_f64(pResampler->windowTime) == 0) {
                /*
                No need to read from the backend - just copy the input straight over without any processing. We start reading from
                the right side of the filter window.
                */
                ma_uint16 iFirstSample = (ma_uint16)pResampler->windowTime + ma_resampler__window_length_left(pResampler);
                if (pResampler->config.format == ma_format_f32) {
                    for (ma_uint16 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                        if (pResampler->config.layout == ma_stream_layout_deinterleaved) {
                            for (ma_uint16 iFrame = 0; iFrame < framesToReadRightNow; ++iFrame) {
                                runningFramesOutDeinterleaved.f32[iChannel][iFrame] = pResampler->cache.f32[pResampler->cacheStrideInFrames*iChannel + iFirstSample + iFrame];
                            }
                        } else {
                            for (ma_uint16 iFrame = 0; iFrame < framesToReadRightNow; ++iFrame) {
                                runningFramesOutInterleaved.f32[iFrame*pResampler->config.channels + iChannel] = pResampler->cache.f32[pResampler->cacheStrideInFrames*iChannel + iFirstSample + iFrame];
                            }
                        }
                    }
                } else {
                    for (ma_uint16 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                        if (pResampler->config.layout == ma_stream_layout_deinterleaved) {
                            for (ma_uint16 iFrame = 0; iFrame < framesToReadRightNow; ++iFrame) {
                                runningFramesOutDeinterleaved.s16[iChannel][iFrame] = pResampler->cache.s16[pResampler->cacheStrideInFrames*iChannel + iFirstSample + iFrame];
                            }
                        } else {
                            for (ma_uint16 iFrame = 0; iFrame < framesToReadRightNow; ++iFrame) {
                                runningFramesOutInterleaved.s16[iFrame*pResampler->config.channels + iChannel] = pResampler->cache.s16[pResampler->cacheStrideInFrames*iChannel + iFirstSample + iFrame];
                            }
                        }
                    }
                }
            } else {
                /*
                Need to read from the backend. Input data is always from the cache. Output data is always to a deinterleaved buffer. When the stream layout
                is set to deinterleaved, we need to read into a temporary buffer and then interleave.
                */
                ma_uint64 framesJustRead;
                if (pResampler->config.format == ma_format_f32) {
                    if (pResampler->config.layout == ma_stream_layout_deinterleaved) {
                        framesJustRead = pResampler->readF32(pResampler, framesToReadRightNow, runningFramesOutDeinterleaved.f32);
                    } else {
                        float buffer[ma_countof(pResampler->cache.f32)];
                        float* ppDeinterleavedFrames[MA_MAX_CHANNELS];
                        for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                            ppDeinterleavedFrames[iChannel] = buffer + (pResampler->cacheStrideInFrames*iChannel);
                        }

                        framesJustRead = pResampler->readF32(pResampler, framesToReadRightNow, ppDeinterleavedFrames);
                        ma_interleave_pcm_frames(pResampler->config.format, pResampler->config.channels, framesJustRead, ppDeinterleavedFrames, runningFramesOutInterleaved.f32);
                    }
                } else {
                    if (pResampler->config.layout == ma_stream_layout_interleaved) {
                        framesJustRead = pResampler->readS16(pResampler, framesToReadRightNow, runningFramesOutDeinterleaved.s16);
                    } else {
                        ma_int16 buffer[ma_countof(pResampler->cache.s16)];
                        ma_int16* ppDeinterleavedFrames[MA_MAX_CHANNELS];
                        for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                            ppDeinterleavedFrames[iChannel] = buffer + (pResampler->cacheStrideInFrames*iChannel);
                        }

                        framesJustRead = pResampler->readS16(pResampler, framesToReadRightNow, ppDeinterleavedFrames);
                        ma_interleave_pcm_frames(pResampler->config.format, pResampler->config.channels, framesJustRead, ppDeinterleavedFrames, runningFramesOutInterleaved.s16);
                    }
                }
            
                if (framesJustRead != framesToReadRightNow) {
                    ma_assert(MA_FALSE);
                    break;  /* Should never hit this. */
                }
            }

            /* Move time forward. */
            pResampler->windowTime += (framesToReadRightNow * pResampler->config.ratio);

            if (pResampler->config.format == ma_format_f32) {
                if (pResampler->config.layout == ma_stream_layout_deinterleaved) {
                    for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                        runningFramesOutDeinterleaved.f32[iChannel] += framesToReadRightNow;
                    }
                } else {
                    runningFramesOutInterleaved.f32 += framesToReadRightNow * pResampler->config.channels;
                }
            } else {
                if (pResampler->config.layout == ma_stream_layout_deinterleaved) {
                    for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                        runningFramesOutDeinterleaved.s16[iChannel] += framesToReadRightNow;
                    }
                } else {
                    runningFramesOutInterleaved.s16 += framesToReadRightNow * pResampler->config.channels;
                }
            }

            /* We don't want to reload the buffer if we've finished reading. */
            totalFramesRead += framesToReadRightNow;
            if (totalFramesRead == frameCount) {
                break;
            }
        }

        /*
        We need to exit if we've reached the end of the input buffer. We do not want to attempt to read more data, nor
        do we want to read in zeroes to fill out the requested frame count (frameCount).
        */
        if (loadedEndOfInput) {
            break;
        }

        /*
        If we get here it means we need to reload the buffer from the client and keep iterating. To reload the buffer we
        need to move the remaining data down to the front of the buffer, adjust the window time, then read more from the
        client. If we have already reached the end of the client's data, we don't want to attempt to read more.
        */
        result = ma_resampler__reload_cache(pResampler, &loadedEndOfInput);
        if (result != MA_SUCCESS) {
            break;  /* An error occurred when trying to reload the cache from the client. This does _not_ indicate that the end of the input has been reached. */
        }
    }

    return totalFramesRead;
}

ma_uint64 ma_resampler_seek(ma_resampler* pResampler, ma_uint64 frameCount, ma_uint32 options)
{
    ma_uint64 totalFramesSeeked = 0;

    if (pResampler == NULL) {
        return 0;   /* Invalid args. */
    }

    if (frameCount == 0) {
        return 0;   /* Nothing to do, so return early. */
    }

    /* Seeking is slightly different depending on whether or not we are seeking by the output or input rate. */
    if ((options & MA_RESAMPLER_SEEK_INPUT_RATE) != 0 || pResampler->config.ratio == 1) {
        /* Seeking by input rate. This is a simpler case because we don't need to care about the ratio. */
        if ((options & MA_RESAMPLER_SEEK_NO_CLIENT_READ) != 0) {
            /*
            Not reading from the client. This is the fast path. We can do this in constant time. Because in this mode the contents
            of the cache are left undefined and the fractional part of the window time is left exactly the same (since we're seeking
            by input rate instead of output rate), all we need to do is change the loaded sample count to the start of the right
            side of the filter window so that a reload is forced in the next read.
            */
            pResampler->cacheLengthInFrames = (ma_uint16)ceil(pResampler->windowTime + ma_resampler__window_length_left(pResampler));
            totalFramesSeeked = frameCount;
        } else {
            /* We need to read from the client which means we need to loop. */
            /*while (totalFramesSeeked < frameCount) {

            }*/
        }
    } else {
        /* Seeking by output rate. */
        if ((options & MA_RESAMPLER_SEEK_NO_CLIENT_READ) != 0) {
            /* Not reading from the client. This is a fast-ish path, though I'm not doing this in constant time like when seeking by input rate. It's easier to just loop. */
        } else {
            /* Reading from the client. This case is basically the same as reading, but without the filtering. */
        }
    }

    return totalFramesSeeked;
}
#endif

ma_uint64 ma_resampler_get_cached_input_frame_count(ma_resampler* pResampler)
{
    return (ma_uint64)ceil(ma_resampler_get_cached_input_time(pResampler));
}

ma_uint64 ma_resampler_get_cached_output_frame_count(ma_resampler* pResampler)
{
    return (ma_uint64)floor(ma_resampler_get_cached_output_time(pResampler));
}


double ma_resampler_get_cached_input_time(ma_resampler* pResampler)
{
    if (pResampler == NULL) {
        return 0;   /* Invalid args. */
    }

    return ma_resampler__calculate_cached_input_time(pResampler);
}

double ma_resampler_get_cached_output_time(ma_resampler* pResampler)
{
    if (pResampler == NULL) {
        return 0;   /* Invalid args. */
    }

    return ma_resampler__calculate_cached_output_time(pResampler);
}


ma_uint64 ma_resampler_get_required_input_frame_count(ma_resampler* pResampler, ma_uint64 outputFrameCount)
{
    if (pResampler == NULL) {
        return 0;   /* Invalid args. */
    }

    if (outputFrameCount == 0) {
        return 0;
    }

    /* First grab the amount of output time sitting in the cache. */
    double cachedOutputTime = ma_resampler__calculate_cached_output_time(pResampler);
    if (cachedOutputTime >= outputFrameCount) {
        return 0;   /* All of the necessary input data is cached. No additional data is required from the client. */
    }

    /*
    Getting here means more input data will be required. A detail to consider here is that we are accepting an unsigned 64-bit integer
    for the output frame count, however we need to consider sub-frame timing which we're doing by using a double. There will not be
    enough precision in the double to represent the whole 64-bit range of the input variable. For now I'm not handling this explicitly
    because I think it's unlikely outputFrameCount will be set to something so huge anyway, but it will be something to think about in
    order to get this working properly for the whole 64-bit range.

    The return value must always be larger than 0 after this point. If it's not we have an error.
    */
    double nonCachedOutputTime = outputFrameCount - cachedOutputTime;
    ma_assert(nonCachedOutputTime > 0);

    ma_uint64 requiredInputFrames = (ma_uint64)ceil(nonCachedOutputTime * pResampler->config.ratio);
    ma_assert(requiredInputFrames > 0);

    return requiredInputFrames;
}

ma_uint64 ma_resampler_get_expected_output_frame_count(ma_resampler* pResampler, ma_uint64 inputFrameCount)
{
    if (pResampler == NULL) {
        return 0; /* Invalid args. */
    }

    if (inputFrameCount == 0) {
        return 0;
    }

    /* What we're actually calculating here is how many whole output frames will be calculated after consuming inputFrameCount + ma_resampler_get_cached_input_time(). */
    return (ma_uint64)floor((ma_resampler__calculate_cached_input_time(pResampler) + inputFrameCount) / pResampler->config.ratio);
}


/*
Linear
*/
ma_result ma_resampler_init__linear(ma_resampler* pResampler)
{
    ma_assert(pResampler != NULL);

    /* The linear implementation always has a window length of 2. */
    pResampler->windowLength = 2;

    return MA_SUCCESS;
}

ma_result ma_resampler_process__linear(ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_uint64* pFrameCountIn, void** ppFramesIn, ma_resampler_seek_mode seekMode)
{
    ma_assert(pResampler != NULL);

    if (seekMode == ma_resampler_seek_mode_none) {
        /* Standard processing. */
        ma_assert(pFrameCountOut != NULL);
        ma_assert(ppFramesOut    != NULL);
        ma_assert(pFrameCountIn  != NULL);
        ma_assert(ppFramesIn     != NULL);


    } else if (seekMode == ma_resampler_seek_mode_output) {
        /* Seek by output rate. */
        ma_assert(pFrameCountOut != NULL);
        
        /* TODO: Implement me. */
    } else if (seekMode == ma_resampler_seek_mode_input) {
        /* Seek by input rate. */
        ma_assert(pFrameCountIn != NULL);

        /* TODO: Implement me. */
    }

    return MA_INVALID_OPERATION;    /* Not yet implemented. */
}

ma_uint64 ma_resampler_read_f32__linear(ma_resampler* pResampler, ma_uint64 frameCount, float** ppFrames)
{
    ma_assert(pResampler != NULL);
    ma_assert(pResampler->config.onRead != NULL);
    ma_assert(frameCount > 0);
    ma_assert(ppFrames != NULL);

    /* TODO: Implement me. */
    (void)pResampler;
    (void)frameCount;
    (void)ppFrames;
    return 0;
}

ma_uint64 ma_resampler_read_s16__linear(ma_resampler* pResampler, ma_uint64 frameCount, ma_int16** ppFrames)
{
    ma_assert(pResampler != NULL);
    ma_assert(pResampler->config.onRead != NULL);
    ma_assert(frameCount > 0);
    ma_assert(ppFrames != NULL);

    /* TODO: Implement an s16 optimized implementation. */

    /* I'm cheating here and just using the f32 implementation and converting to s16. This will be changed later - for now just focusing on getting it working. */
    float bufferF32[ma_countof(pResampler->cache.s16)];
    float* ppFramesF32[MA_MAX_CHANNELS];
    for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
        ppFramesF32[iChannel] = bufferF32 + (pResampler->cacheStrideInFrames*iChannel);
    }

    ma_uint64 framesRead = ma_resampler_read_f32__linear(pResampler, frameCount, ppFramesF32);

    for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
        ma_pcm_f32_to_s16(ppFrames[iChannel], ppFramesF32[iChannel], frameCount, ma_dither_mode_none);    /* No dithering - keep it fast for linear. */
    }

    return framesRead;
}

ma_uint64 ma_resampler_seek__linear(ma_resampler* pResampler, ma_uint64 frameCount, ma_uint32 options)
{
    ma_assert(pResampler != NULL);
    ma_assert(pResampler->config.onRead != NULL);
    ma_assert(frameCount > 0);

    /* TODO: Implement me. */
    (void)pResampler;
    (void)frameCount;
    (void)options;
    return 0;
}


/*
Sinc
*/
ma_result ma_resampler_init__sinc(ma_resampler* pResampler)
{
    ma_assert(pResampler != NULL);

    /* TODO: Implement me. Need to initialize the sinc table. */
    return MA_SUCCESS;
}

ma_result ma_resampler_process__sinc(ma_resampler* pResampler, ma_uint64* pFrameCountOut, void** ppFramesOut, ma_uint64* pFrameCountIn, void** ppFramesIn, ma_resampler_seek_mode seekMode)
{
    ma_assert(pResampler != NULL);

    return MA_INVALID_OPERATION;    /* Not yet implemented. */
}

ma_uint64 ma_resampler_read_f32__sinc(ma_resampler* pResampler, ma_uint64 frameCount, float** ppFrames)
{
    ma_assert(pResampler != NULL);
    ma_assert(pResampler->config.onRead != NULL);
    ma_assert(frameCount > 0);
    ma_assert(ppFrames != NULL);

    /* TODO: Implement me. */
    (void)pResampler;
    (void)frameCount;
    (void)ppFrames;
    return 0;
}

ma_uint64 ma_resampler_read_s16__sinc(ma_resampler* pResampler, ma_uint64 frameCount, ma_int16** ppFrames)
{
    ma_assert(pResampler != NULL);
    ma_assert(pResampler->config.onRead != NULL);
    ma_assert(frameCount > 0);
    ma_assert(ppFrames != NULL);

    /* TODO: Implement an s16 optimized implementation. */

    /* I'm cheating here and just using the f32 implementation and converting to s16. This will be changed later - for now just focusing on getting it working. */
    float bufferF32[ma_countof(pResampler->cache.s16)];
    float* ppFramesF32[MA_MAX_CHANNELS];
    for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
        ppFramesF32[iChannel] = bufferF32 + (pResampler->cacheStrideInFrames*iChannel);
    }

    ma_uint64 framesRead = ma_resampler_read_f32__sinc(pResampler, frameCount, ppFramesF32);

    for (ma_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
        ma_pcm_f32_to_s16(ppFrames[iChannel], ppFramesF32[iChannel], frameCount, ma_dither_mode_triangle);
    }

    return framesRead;
}

ma_uint64 ma_resampler_seek__sinc(ma_resampler* pResampler, ma_uint64 frameCount, ma_uint32 options)
{
    ma_assert(pResampler != NULL);
    ma_assert(pResampler->config.onRead != NULL);
    ma_assert(frameCount > 0);

    /* TODO: Implement me. */
    (void)pResampler;
    (void)frameCount;
    (void)options;
    return 0;
}

#endif
