
#ifndef ma_speex_resampler_h
#define ma_speex_resampler_h

#define OUTSIDE_SPEEX
#define RANDOM_PREFIX   ma_speex
#include "thirdparty/speex_resampler.h"

#define spx_uint64_t unsigned long long

int ma_speex_resampler_get_required_input_frame_count(SpeexResamplerState* st, spx_uint64_t out_len, spx_uint64_t* in_len);

#endif  /* ma_speex_resampler_h */

#if defined(MINIAUDIO_SPEEX_RESAMPLER_IMPLEMENTATION)
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(push)
    #pragma warning(disable:4244)   /* conversion from 'x' to 'y', possible loss of data */
    #pragma warning(disable:4018)   /* signed/unsigned mismatch */
    #pragma warning(disable:4706)   /* assignment within conditional expression */
#else
    #pragma GCC diagnostic push
#endif
#include "thirdparty/resample.c"
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(pop)
#else
    #pragma GCC diagnostic pop
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
#endif