/*
Consider this code public domain.

This is research into a new resampler for mini_al. Not yet complete.

Requirements:
- Selection of different algorithms. The following at a minimum:
  - Passthrough
  - Linear with optional filtering
  - Sinc
- Floating point pipeline for f32 and fixed point integer pipeline for s16
  - Specify a mal_format enum as a config at initialization time, but fail if it's anything other than f32 or s16
- Need ability to move time forward without processing any samples
  - Needs an option to handle the cache as if silent samples of 0 have been passed as input
  - Needs option to move time forward by output sample rate _or_ input sample rate
- Need to be able to do the equivalent to a seek by passing in NULL to the read API()
  - mal_resampler_read(pResampler, frameCount, NULL) = mal_resampler_seek(pResampler, frameCount, 0)
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
- Pointers passed into the onRead() callback must be guaranteed to be aligned to MAL_SIMD_ALIGNMENT.


Other Notes:
- I've had a bug in the past where a single call to read() returns too many samples. It essentially computes more
  samples than the input data would allow. The input data would get consumed, but output samples would continue to
  get computed up to the requested frame count, filling in the end with zeroes. This is completely wrong because
  the return value needs to be used to know whether or not the end of the input has been reached.


Random Notes:
- You cannot change the algorithm after initialization.
*/
#ifndef mal_resampler_h
#define mal_resampler_h

#define MAL_RESAMPLER_SEEK_SILENT_INPUT (1 << 0)    /* When set, does not read anything from the client when seeking. This does _not_ call onRead(). */
#define MAL_RESAMPLER_SEEK_INPUT_RATE   (1 << 1)    /* When set, treats the specified frame count based on the input sample rate rather than the output sample rate. */

typedef struct mal_resampler mal_resampler;

/* Client callbacks. */
typedef mal_uint32 (* mal_resampler_read_from_client_proc)               (mal_resampler* pResampler, mal_uint32 frameCount, void** ppFrames);

/* Backend functions. */
typedef mal_uint64 (* mal_resampler_read_proc)                           (mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames);
typedef mal_uint64 (* mal_resampler_seek_proc)                           (mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options);
typedef mal_result (* mal_resampler_get_cached_frame_counts_proc)        (mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput);
typedef mal_uint64 (* mal_resampler_get_required_input_frame_count_proc) (mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput);
typedef mal_uint64 (* mal_resampler_get_expected_output_frame_count_proc)(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput);

typedef enum
{
    mal_resample_algorithm_sinc = 0,    /* Default. */
    mal_resample_algorithm_linear,      /* Fastest. */
    mal_resample_algorithm_passthrough  /* No resampling. */
} mal_resample_algorithm;

typedef enum
{
    mal_resampler_end_of_input_mode_consume = 0,    /* When the end of the input stream is reached, consume the last input PCM frames (do not leave them in the internal cache). Default. */
    mal_resampler_end_of_input_mode_no_consume      /* When the end of the input stream is reached, do _not_ consume the last input PCM frames (leave them in the internal cache). Use this in streaming situations. */
} mal_resampler_end_of_input_mode;

typedef struct
{
    mal_format format;
    mal_uint32 channels;
    mal_uint32 sampleRateIn;
    mal_uint32 sampleRateOut;
    double ratio;    /* ratio = in/out */
    mal_resample_algorithm algorithm;
    mal_resampler_end_of_input_mode endOfInputMode;
    mal_resampler_read_from_client_proc onRead;
    void* pUserData;
} mal_resampler_config;

struct mal_resampler
{
    mal_resampler_config config;
    mal_resampler_read_proc read;
    mal_resampler_seek_proc seek;
    mal_resampler_get_cached_frame_counts_proc getCachedFrameCounts;
    mal_resampler_get_required_input_frame_count_proc getRequiredInputFrameCount;
    mal_resampler_get_expected_output_frame_count_proc getExpectedOutputFrameCount;
};

/*
Initializes a new resampler object from a config.
*/
mal_result mal_resampler_init(const mal_resampler_config* pConfig, mal_resampler* pResampler);

/*
Uninitializes the given resampler.
*/
void mal_resampler_uninit(mal_resampler* pResampler);

/*
Dynamically adjusts the sample rate.
*/
mal_result mal_resampler_set_rate(mal_resampler* pResampler, mal_uint32 sampleRateIn, mal_uint32 sampleRateOut);

/*
Dynamically adjusts the sample rate by a ratio.
*/
mal_result mal_resampler_set_rate_ratio(mal_resampler* pResampler, double ratio);

/*
Reads a number of PCM frames from the resampler.

Passing in NULL for ppFrames is equivalent to calling mal_resampler_seek(pResampler, frameCount, 0).
*/
mal_uint64 mal_resampler_read(mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames);

/*
Seeks forward by the specified number of PCM frames.

"options" can be a cobination of the following:
    MAL_RESAMPLER_SEEK_SILENT_INPUT
        Reads in silence instead of reading in data from the onRead callback.
    MAL_RESAMPLER_SEEK_INPUT_RATE
        Treats "frameCount" as input samples instead of output samples.
*/
mal_uint64 mal_resampler_seek(mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options);

/*
Retrieves detailed information about the number of PCM frames that are current sitting inside the cache.

Since the ratio may be fractional, returning whole values for the input and output count does not quite provide enough
information for complete accuracy. When an input sample is only partially consumed, it is still required to compute the
next output frame which means it needs to be included in pInputPCMFrameCount. That little bit of excess is returned in
pExcessInput. Likewise, when computing the number of cached output frames, there may be some leftover input frames which
are returned in pLeftoverInput.

When the end of input mode is set to mal_resampler_end_of_input_mode_no_consume, the input frames currently sitting in the
window are not included in the calculation.

Consider using mal_resampler_get_cached_input_frame_count() and mal_resampler_get_cached_output_frame_count() for a simpler
and easier to user API.
*/
mal_result mal_resampler_get_cached_frame_counts(mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput);

/*
A helper API for retrieving the number of cached input frames.

Sine this function returns whole input frames and the ratio may be fractional, there is a situation where part of the last
input sample may be required in the calculation of the next output sample. This excess is returned in pExcessInput.

See also: mal_resampler_get_cached_frame_counts()
*/
mal_uint64 mal_resampler_get_cached_input_frame_count(mal_resampler* pResampler, double* pExcessInput);

/*
A helper API for retrieving the number of whole output frames that can be calculated from the currently cached input frames.

Since this function returns whole output frames and the ratio may be fractional, there is a situation where the very last
input frame cannot be fully process because in order to do so the resampler would need part of the next input frame after
that. In this case the last input frames may have only been partially processed. The leftover input frames are returned
in pLeftoverInput which may be fractional and may be greater than 1, depending on the ratio.

See also: mal_resampler_get_cached_frame_counts()
*/
mal_uint64 mal_resampler_get_cached_output_frame_count(mal_resampler* pResampler, double* pLeftoverInput);

/*
Calculates the number of whole input frames that would need to be read from the client in order to output the specified
number of output frames.

The returned value does not include cached input frames. It only returns the number of extra frames that would need to be
read from the client in order to output the specified number of output frames.

Sine this function returns whole input frames and the ratio may be fractional, there is a situation where part of the last
input sample may be required in the calculation of the next output sample. This excess is returned in pExcessInput.

When the end of input mode is set to mal_resampler_end_of_input_mode_no_consume, the input frames sitting in the filter
window are not included in the calculation.
*/
mal_uint64 mal_resampler_get_required_input_frame_count(mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput);

/*
Calculates the number of output PCM frames that would be output after fully reading and consuming the specified number of
input PCM frames from the client.

A detail to keep in mind is how cached input frames are handled. This function calculates the output frame count based on
inputFrameCount + mal_resampler_get_cached_input_frame_count(). It essentially calcualtes how many output frames will be
returned if an additional inputFrameCount frames were read from the client and consumed by the resampler. You can adjust
the return value by mal_resampler_get_cached_output_frame_count() which calculates the number of output frames that can be
output from the currently cached input.

Since this function returns whole output frames and the ratio may be fractional, there is a situation where the very last
input frame cannot be fully process because in order to do so the resampler would need part of the next input frame after
that. In this case the last input frames may have only been partially processed. The leftover input frames are returned
in pLeftoverInput which may be fractional and may be greater than 1, depending on the ratio.

When the end of input mode is set to mal_resampler_end_of_input_mode_no_consume, the input frames sitting in the filter
window are not included in the calculation.
*/
mal_uint64 mal_resampler_get_expected_output_frame_count(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput);
#endif

#ifdef MINI_AL_IMPLEMENTATION
mal_uint64 mal_resampler_read__passthrough(mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames);
mal_uint64 mal_resampler_seek__passthrough(mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options);
mal_result mal_resampler_get_cached_frame_counts__passthrough(mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput);
mal_uint64 mal_resampler_get_required_input_frame_count__passthrough(mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput);
mal_uint64 mal_resampler_get_expected_output_frame_count__passthrough(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput);

mal_uint64 mal_resampler_read__linear(mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames);
mal_uint64 mal_resampler_seek__linear(mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options);
mal_result mal_resampler_get_cached_frame_counts__linear(mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput);
mal_uint64 mal_resampler_get_required_input_frame_count__linear(mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput);
mal_uint64 mal_resampler_get_expected_output_frame_count__linear(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput);

mal_uint64 mal_resampler_read__sinc(mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames);
mal_uint64 mal_resampler_seek__sinc(mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options);
mal_result mal_resampler_get_cached_frame_counts__sinc(mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput);
mal_uint64 mal_resampler_get_required_input_frame_count__sinc(mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput);
mal_uint64 mal_resampler_get_expected_output_frame_count__sinc(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput);

/* TODO: Add this to mini_al.h */
#define MAL_ALIGN_INT(val, alignment) (((val) + ((alignment-1))) & ~((alignment)-1))
#define MAL_ALIGN_PTR(ptr, alignment) (void*)MAL_ALIGN_INT(((mal_uintptr)(ptr)), (alignment))

/*
This macro declares a set of variables on the stack of a given size in bytes. The variables it creates are:
  - mal_uint8 <name>Unaligned[size + MAL_SIMD_ALIGNMENT];
  - <type>* <name>[MAL_MAX_CHANNELS];
  - size_t <name>FrameCount;    <-- This is the number of samples contained within each sub-buffer of <name>

This does not work for formats that do not have a clean mapping to a primitive C type. s24 will not work here.
*/
#define MAL_DECLARE_ALIGNED_STACK_BUFFER(type, name, size, channels) \
    mal_uint8 name##Unaligned[(size) + MAL_SIMD_ALIGNMENT]; \
    type* name[MAL_MAX_CHANNELS]; \
    size_t name##FrameCount = ((size) & ~((MAL_SIMD_ALIGNMENT)-1)) / sizeof(type); \
    do { \
        mal_uint32 iChannel; \
        for (iChannel = 0; iChannel < channels; ++iChannel) { \
            name[iChannel] = (type*)((mal_uint8*)MAL_ALIGN_PTR(name##Unaligned, MAL_SIMD_ALIGNMENT) + (iChannel*((size) & ~((MAL_SIMD_ALIGNMENT)-1)))); \
        } \
    } while (0)
    

mal_result mal_resampler_init(const mal_resampler_config* pConfig, mal_resampler* pResampler)
{
    if (pResampler == NULL) {
        return MAL_INVALID_ARGS;
    }
    mal_zero_object(pResampler);

    if (pConfig == NULL) {
        return MAL_INVALID_ARGS;
    }

    pResampler->config = *pConfig;
    if (pResampler->config.format != mal_format_f32 && pResampler->config.format != mal_format_s16) {
        return MAL_INVALID_ARGS;        /* Unsupported format. */
    }
    if (pResampler->config.channels == 0) {
        return MAL_INVALID_ARGS;        /* Unsupported channel count. */
    }
    if (pResampler->config.ratio == 0) {
        if (pResampler->config.sampleRateIn == 0 || pResampler->config.sampleRateOut == 0) {
            return MAL_INVALID_ARGS;    /* Unsupported sample rate. */
        }
        pResampler->config.ratio = (double)pResampler->config.sampleRateIn / (double)pResampler->config.sampleRateOut;
    }
    if (pResampler->config.onRead == NULL) {
        return MAL_INVALID_ARGS;        /* No input callback specified. */
    }

    switch (pResampler->config.algorithm) {
        case mal_resample_algorithm_passthrough:
        {
            pResampler->read                        = mal_resampler_read__passthrough;
            pResampler->seek                        = mal_resampler_seek__passthrough;
            pResampler->getCachedFrameCounts        = mal_resampler_get_cached_frame_counts__passthrough;
            pResampler->getRequiredInputFrameCount  = mal_resampler_get_required_input_frame_count__passthrough;
            pResampler->getExpectedOutputFrameCount = mal_resampler_get_expected_output_frame_count__passthrough;
        } break;

        case mal_resample_algorithm_linear:
        {
            pResampler->read                        = mal_resampler_read__linear;
            pResampler->seek                        = mal_resampler_seek__linear;
            pResampler->getCachedFrameCounts        = mal_resampler_get_cached_frame_counts__linear;
            pResampler->getRequiredInputFrameCount  = mal_resampler_get_required_input_frame_count__linear;
            pResampler->getExpectedOutputFrameCount = mal_resampler_get_expected_output_frame_count__linear;
        } break;

        case mal_resample_algorithm_sinc:
        {
            pResampler->read                        = mal_resampler_read__sinc;
            pResampler->seek                        = mal_resampler_seek__sinc;
            pResampler->getCachedFrameCounts        = mal_resampler_get_cached_frame_counts__sinc;
            pResampler->getRequiredInputFrameCount  = mal_resampler_get_required_input_frame_count__sinc;
            pResampler->getExpectedOutputFrameCount = mal_resampler_get_expected_output_frame_count__sinc;
        } break;
    }

    return MAL_SUCCESS;
}

void mal_resampler_uninit(mal_resampler* pResampler)
{
    (void)pResampler;
}

mal_result mal_resampler_set_rate(mal_resampler* pResampler, mal_uint32 sampleRateIn, mal_uint32 sampleRateOut)
{
    if (pResampler == NULL) {
        return MAL_INVALID_ARGS;
    }

    if (sampleRateIn == 0 || sampleRateOut == 0) {
        return MAL_INVALID_ARGS;
    }

    pResampler->config.sampleRateIn  = sampleRateIn;
    pResampler->config.sampleRateOut = sampleRateOut;
    pResampler->config.ratio         = (double)pResampler->config.sampleRateIn / (double)pResampler->config.sampleRateOut;

    return MAL_SUCCESS;
}

mal_result mal_resampler_set_rate_ratio(mal_resampler* pResampler, double ratio)
{
    if (pResampler == NULL) {
        return MAL_INVALID_ARGS;
    }

    if (ratio == 0) {
        return MAL_INVALID_ARGS;
    }

    pResampler->config.ratio = ratio;

    return MAL_SUCCESS;
}

mal_uint64 mal_resampler_read(mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames)
{
    if (pResampler == NULL || pResampler->read == NULL) {
        return 0;   /* Invalid arguments. */
    }

    if (frameCount == 0) {
        return 0;   /* Nothing to do, so return early. */
    }

    /* When ppFrames is NULL, reading is equivalent to seeking with default options. */
    if (ppFrames == NULL) {
        return mal_resampler_seek(pResampler, frameCount, 0);
    }

    return pResampler->read(pResampler, frameCount, ppFrames);
}

mal_uint64 mal_resampler_seek(mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options)
{
    if (pResampler == NULL || pResampler->seek == NULL) {
        return 0;
    }

    if (frameCount == 0) {
        return 0;   /* Nothing to do, so return early. */
    }

    return pResampler->seek(pResampler, frameCount, options);
}

mal_result mal_resampler_get_cached_frame_counts(mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput)
{
    /* For safety in case a backend fails to set the frame counts. */
    if (pInputPCMFrameCount  != NULL) { *pInputPCMFrameCount  = 0; }
    if (pOutputPCMFrameCount != NULL) { *pOutputPCMFrameCount = 0; }

    if (pResampler == NULL || pResampler->getCachedFrameCounts == NULL) {
        return MAL_INVALID_ARGS;
    }

    /* For safety we will ensure NULL is never passed to this callback. It also simplifies the implementation of each backend. */
    mal_uint64 inputPCMFrameCount = 0;
    double excessInput = 0;
    mal_uint64 outputPCMFrameCount = 0;
    double leftoverInput = 0;
    mal_result result = pResampler->getCachedFrameCounts(pResampler, &inputPCMFrameCount, &excessInput, &outputPCMFrameCount, &leftoverInput);
    if (result != MAL_SUCCESS) {
        return result;
    }

    if (pInputPCMFrameCount  != NULL) { *pInputPCMFrameCount  = inputPCMFrameCount;  }
    if (pExcessInput         != NULL) { *pExcessInput         = excessInput;         }
    if (pOutputPCMFrameCount != NULL) { *pOutputPCMFrameCount = outputPCMFrameCount; }
    if (pLeftoverInput       != NULL) { *pLeftoverInput       = leftoverInput;       }

    return result;
}

mal_uint64 mal_resampler_get_cached_input_frame_count(mal_resampler* pResampler, double* pExcessInput)
{
    if (pResampler == NULL || pResampler->getCachedFrameCounts == NULL) {
        return 0;
    }

    mal_uint64 inputPCMFrameCount = 0;
    double excessInput = 0;
    mal_uint64 unused;
    double unused2;
    mal_result result = pResampler->getCachedFrameCounts(pResampler, &inputPCMFrameCount, &excessInput, &unused, &unused2);
    if (result != MAL_SUCCESS) {
        return 0;
    }

    if (pExcessInput != NULL) {
        *pExcessInput = excessInput;
    }
    return inputPCMFrameCount;
}

mal_uint64 mal_resampler_get_cached_output_frame_count(mal_resampler* pResampler, double* pLeftoverInput)
{
    if (pResampler == NULL || pResampler->getCachedFrameCounts == NULL) {
        return 0;
    }

    mal_uint64 unused;
    double unused2;
    mal_uint64 outputPCMFrameCount = 0;
    double leftoverInput = 0;
    mal_result result = pResampler->getCachedFrameCounts(pResampler, &unused, &unused2, &outputPCMFrameCount, &leftoverInput);
    if (result != MAL_SUCCESS) {
        return 0;
    }

    if (pLeftoverInput != NULL) {
        *pLeftoverInput = leftoverInput;
    }
    return outputPCMFrameCount;
}

mal_uint64 mal_resampler_get_required_input_frame_count(mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput)
{
    if (pResampler == NULL || pResampler->getRequiredInputFrameCount == NULL) {
        return 0;   /* Invalid args. */
    }

    if (outputFrameCount == 0) {
        return 0;
    }

    return pResampler->getRequiredInputFrameCount(pResampler, outputFrameCount, pExcessInput);
}

mal_uint64 mal_resampler_get_expected_output_frame_count(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput)
{
    if (pResampler == NULL || pResampler->getExpectedOutputFrameCount == NULL) {
        return 0; /* Invalid args. */
    }

    if (inputFrameCount == 0) {
        return 0;
    }

    return pResampler->getExpectedOutputFrameCount(pResampler, inputFrameCount, pLeftoverInput);
}


/*
Passthrough
*/
mal_uint64 mal_resampler_read__passthrough(mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames)
{
    mal_assert(pResampler != NULL);
    mal_assert(pResampler->config.onRead != NULL);
    mal_assert(frameCount > 0);
    mal_assert(ppFrames != NULL);

    /*
    It's tempting to to just call pResampler->config.onRead() and pass in ppFrames directly, however this violates
    our requirement that all buffers passed into onRead() are aligned to MAL_SIMD_ALIGNMENT. If any of the ppFrames
    buffers are misaligned we need to read into a temporary buffer.
    */
    mal_bool32 isOutputBufferAligned = MAL_TRUE;
    for (mal_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
        if (((mal_uintptr)ppFrames[iChannel] & (MAL_SIMD_ALIGNMENT-1)) != 0) {
            isOutputBufferAligned = MAL_FALSE;
            break;
        }
    }

    if (frameCount <= 0xFFFFFFFF && isOutputBufferAligned) {
        return pResampler->config.onRead(pResampler, (mal_uint32)frameCount, ppFrames); /* Fast path. */
    } else {
        MAL_DECLARE_ALIGNED_STACK_BUFFER(float, ppRunningFrames, 4096, pResampler->config.channels);

        mal_uint64 totalFramesRead = 0;
        while (frameCount > 0) {
            mal_uint64 framesToReadNow = (pResampler->config.format == mal_format_f32) ? ppRunningFramesFrameCount : ppRunningFramesFrameCount*2;   /* x2 for the s16 frame count because ppRunningFramesFrameCount is based on f32. */
            if (framesToReadNow > frameCount) {
                framesToReadNow = frameCount;
            }

            mal_uint32 framesJustRead = pResampler->config.onRead(pResampler, (mal_uint32)framesToReadNow, ppRunningFrames);
            if (framesJustRead == 0) {
                break;
            }

            totalFramesRead += framesJustRead;
            frameCount -= framesJustRead;

            mal_uint32 bytesJustRead = framesJustRead * mal_get_bytes_per_sample(pResampler->config.format);
            for (mal_uint32 iChannel = 0; iChannel < pResampler->config.channels; ++iChannel) {
                mal_copy_memory(ppFrames[iChannel], ppRunningFrames[iChannel], bytesJustRead);
                ppFrames[iChannel] = mal_offset_ptr(ppFrames[iChannel], bytesJustRead);
            }
            
            if (framesJustRead < framesToReadNow) {
                break;
            }
        }

        return totalFramesRead;
    }
}

mal_uint64 mal_resampler_seek__passthrough(mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options)
{
    mal_assert(pResampler != NULL);
    mal_assert(pResampler->config.onRead != NULL);
    mal_assert(frameCount > 0);

    if ((options & MAL_RESAMPLER_SEEK_SILENT_INPUT) != 0) {
        return frameCount;  /* No input from onRead(), so just return immediately. */
    }

    /* Getting here means we need to read from onRead(). In this case we just read into a trash buffer. */
    MAL_DECLARE_ALIGNED_STACK_BUFFER(float, trash, 4096, pResampler->config.channels);

    mal_uint64 totalFramesRead = 0;
    while (frameCount > 0) {
        mal_uint64 framesToRead = trashFrameCount;
        if (framesToRead > frameCount) {
            framesToRead = frameCount;
        }

        mal_uint64 framesRead = pResampler->config.onRead(pResampler, (mal_uint32)framesToRead, trash);
        totalFramesRead += framesRead;
        frameCount -= framesRead;

        /* Don't get stuck in a loop if the client returns no samples. */
        if (framesRead < framesToRead) {
            break;
        }
    }

    return totalFramesRead;
}

mal_result mal_resampler_get_cached_frame_counts__passthrough(mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(pInputPCMFrameCount != NULL);
    mal_assert(pExcessInput != NULL);
    mal_assert(pOutputPCMFrameCount != NULL);
    mal_assert(pLeftoverInput != NULL);

    /* The passthrough implementation never caches, so this is always 0. */
    *pInputPCMFrameCount = 0;
    *pExcessInput = 0;
    *pOutputPCMFrameCount = 0;
    *pLeftoverInput = 0;

    return MAL_SUCCESS;
}

mal_uint64 mal_resampler_get_required_input_frame_count__passthrough(mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(outputFrameCount > 0);
    mal_assert(pExcessInput != NULL);

    /* For passthrough input and output is the same. */
    (void)pResampler;
    *pExcessInput = 0;
    return outputFrameCount;
}

mal_uint64 mal_resampler_get_expected_output_frame_count__passthrough(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(inputFrameCount > 0);
    mal_assert(pLeftoverInput != NULL);

    /* For passthrough input and output is the same. */
    (void)pResampler;
    *pLeftoverInput = 0;
    return inputFrameCount;
}


/*
Linear
*/
mal_uint64 mal_resampler_read__linear(mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames)
{
    mal_assert(pResampler != NULL);
    mal_assert(pResampler->config.onRead != NULL);
    mal_assert(frameCount > 0);
    mal_assert(ppFrames != NULL);

    /* TODO: Implement me. */
    (void)pResampler;
    (void)frameCount;
    (void)ppFrames;
    return 0;
}

mal_uint64 mal_resampler_seek__linear(mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options)
{
    mal_assert(pResampler != NULL);
    mal_assert(pResampler->config.onRead != NULL);
    mal_assert(frameCount > 0);

    /* TODO: Implement me. */
    (void)pResampler;
    (void)frameCount;
    (void)options;
    return 0;
}

mal_result mal_resampler_get_cached_frame_counts__linear(mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(pInputPCMFrameCount != NULL);
    mal_assert(pExcessInput != NULL);
    mal_assert(pOutputPCMFrameCount != NULL);
    mal_assert(pLeftoverInput != NULL);

    /* TODO: Implement me. */
    return MAL_ERROR;
}

mal_uint64 mal_resampler_get_required_input_frame_count__linear(mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(outputFrameCount > 0);
    mal_assert(pExcessInput != NULL);

    /* TODO: Implement me. */
    (void)pResampler;
    *pExcessInput = 0;
    return 0;
}

mal_uint64 mal_resampler_get_expected_output_frame_count__linear(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(inputFrameCount > 0);
    mal_assert(pLeftoverInput != NULL);

    /* TODO: Implement me. */
    (void)pResampler;
    *pLeftoverInput = 0;
    return 0;
}


/*
Sinc
*/
mal_uint64 mal_resampler_read__sinc(mal_resampler* pResampler, mal_uint64 frameCount, void** ppFrames)
{
    mal_assert(pResampler != NULL);
    mal_assert(pResampler->config.onRead != NULL);
    mal_assert(frameCount > 0);
    mal_assert(ppFrames != NULL);

    /* TODO: Implement me. */
    (void)pResampler;
    (void)frameCount;
    (void)ppFrames;
    return 0;
}

mal_uint64 mal_resampler_seek__sinc(mal_resampler* pResampler, mal_uint64 frameCount, mal_uint32 options)
{
    mal_assert(pResampler != NULL);
    mal_assert(pResampler->config.onRead != NULL);
    mal_assert(frameCount > 0);

    /* TODO: Implement me. */
    (void)pResampler;
    (void)frameCount;
    (void)options;
    return 0;
}

mal_result mal_resampler_get_cached_frame_counts__sinc(mal_resampler* pResampler, mal_uint64* pInputPCMFrameCount, double* pExcessInput, mal_uint64* pOutputPCMFrameCount, double* pLeftoverInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(pInputPCMFrameCount != NULL);
    mal_assert(pExcessInput != NULL);
    mal_assert(pOutputPCMFrameCount != NULL);
    mal_assert(pLeftoverInput != NULL);

    /* TODO: Implement me. */
    return MAL_ERROR;
}

mal_uint64 mal_resampler_get_required_input_frame_count__sinc(mal_resampler* pResampler, mal_uint64 outputFrameCount, double* pExcessInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(outputFrameCount > 0);
    mal_assert(pExcessInput != NULL);

    /* TODO: Implement me. */
    (void)pResampler;
    *pExcessInput = 0;
    return 0;
}

mal_uint64 mal_resampler_get_expected_output_frame_count__sinc(mal_resampler* pResampler, mal_uint64 inputFrameCount, double* pLeftoverInput)
{
    mal_assert(pResampler != NULL);
    mal_assert(inputFrameCount > 0);
    mal_assert(pLeftoverInput != NULL);

    /* TODO: Implement me. */
    (void)pResampler;
    *pLeftoverInput = 0;
    return 0;
}

#endif
