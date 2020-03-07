
#ifndef ma_speex_resampler_h
#define ma_speex_resampler_h

#define OUTSIDE_SPEEX
#define RANDOM_PREFIX   ma_speex
#include "thirdparty/speex_resampler.h"

#ifdef _MSC_VER
    #if defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wlanguage-extension-token"
        #pragma GCC diagnostic ignored "-Wlong-long"        
        #pragma GCC diagnostic ignored "-Wc++11-long-long"
    #endif
    #define spx_uint64_t unsigned __int64
    #if defined(__clang__)
        #pragma GCC diagnostic pop
    #endif
#else
    #define MA_HAS_STDINT
    #include <stdint.h>
    #define spx_uint64_t uint64_t
#endif

int ma_speex_resampler_get_required_input_frame_count(SpeexResamplerState* st, spx_uint64_t out_len, spx_uint64_t* in_len);
int ma_speex_resampler_get_expected_output_frame_count(SpeexResamplerState* st, spx_uint64_t in_len, spx_uint64_t* out_len);

#endif  /* ma_speex_resampler_h */

#if defined(MINIAUDIO_SPEEX_RESAMPLER_IMPLEMENTATION)
/* The Speex resampler uses "inline", which is not defined for C89. We need to define it here. */
#if defined(__GNUC__) && !defined(_MSC_VER)
    #if defined(__STRICT_ANSI__)
        #if !defined(inline)
            #define inline __inline__ __attribute__((always_inline))
            #define MA_SPEEX_INLINE_DEFINED
        #endif
    #endif
#endif

#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(push)
    #pragma warning(disable:4244)   /* conversion from 'x' to 'y', possible loss of data */
    #pragma warning(disable:4018)   /* signed/unsigned mismatch */
    #pragma warning(disable:4706)   /* assignment within conditional expression */
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wsign-compare" /* comparison between signed and unsigned integer expressions */
#endif
#include "thirdparty/resample.c"
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(pop)
#else
    #pragma GCC diagnostic pop
#endif

#if defined(MA_SPEEX_INLINE_DEFINED)
    #undef inline
    #undef MA_SPEEX_INLINE_DEFINED
#endif

EXPORT int ma_speex_resampler_get_required_input_frame_count(SpeexResamplerState* st, spx_uint64_t out_len, spx_uint64_t* in_len)
{
    spx_uint64_t count;

    if (st == NULL || in_len == NULL) {
        return RESAMPLER_ERR_INVALID_ARG;
    }

    *in_len = 0;

    if (out_len == 0) {
        return RESAMPLER_ERR_SUCCESS;   /* Nothing to do. */
    }

    /* miniaudio only uses interleaved APIs so we can safely just use channel index 0 for the calculations. */
    if (st->nb_channels == 0) {
        return RESAMPLER_ERR_BAD_STATE;
    }

    count  = out_len * st->int_advance;
    count += (st->samp_frac_num[0] + (out_len * st->frac_advance)) / st->den_rate;

    *in_len = count;

    return RESAMPLER_ERR_SUCCESS;
}

EXPORT int ma_speex_resampler_get_expected_output_frame_count(SpeexResamplerState* st, spx_uint64_t in_len, spx_uint64_t* out_len)
{
    spx_uint64_t count;
    spx_uint64_t last_sample;
    spx_uint32_t samp_frac_num;

    if (st == NULL || out_len == NULL) {
        return RESAMPLER_ERR_INVALID_ARG;
    }

    *out_len = 0;

    if (out_len == 0) {
        return RESAMPLER_ERR_SUCCESS;   /* Nothing to do. */
    }

    /* miniaudio only uses interleaved APIs so we can safely just use channel index 0 for the calculations. */
    if (st->nb_channels == 0) {
        return RESAMPLER_ERR_BAD_STATE;
    }

    count         = 0;
    last_sample   = st->last_sample[0];
    samp_frac_num = st->samp_frac_num[0];

    while (!(last_sample >= in_len)) {
        count += 1;

        last_sample   += st->int_advance;
        samp_frac_num += st->frac_advance;
        if (samp_frac_num >= st->den_rate) {
            samp_frac_num -= st->den_rate;
            last_sample += 1;
        }
    }

    *out_len = count;

    return RESAMPLER_ERR_SUCCESS;
}
#endif
