
#ifndef ma_speex_resampler_h
#define ma_speex_resampler_h

#define OUTSIDE_SPEEX
#define RANDOM_PREFIX   ma_speex
#include "thirdparty/speex_resampler.h"

#endif  /* ma_speex_resampler_h */

#if defined(MINIAUDIO_SPEEX_RESAMPLER_IMPLEMENTATION)
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(push)
    #pragma warning(disable:4244)   /* conversion from 'x' to 'y', possible loss of data */
    #pragma warning(disable:4018)   /* signed/unsigned mismatch */
#else
    #pragma GCC diagnostic push
#endif
#include "thirdparty/resample.c"
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(pop)
#else
    #pragma GCC diagnostic pop
#endif
#endif