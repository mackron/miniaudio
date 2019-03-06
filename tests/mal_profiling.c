#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

typedef enum
{
    simd_mode_scalar = 0,
    simd_mode_sse2,
    simd_mode_avx2,
    simd_mode_avx512,
    simd_mode_neon
} simd_mode;

const char* simd_mode_to_string(simd_mode mode)
{
    switch (mode) {
        case simd_mode_scalar: return "Reference";
        case simd_mode_sse2:   return "SSE2";
        case simd_mode_avx2:   return "AVX2";
        case simd_mode_avx512: return "AVX-512";
        case simd_mode_neon:   return "NEON";
    }

    return "Unknown";
}

const char* ma_src_algorithm_to_string(ma_src_algorithm algorithm)
{
    switch (algorithm) {
        case ma_src_algorithm_none:   return "Passthrough";
        case ma_src_algorithm_linear: return "Linear";
        case ma_src_algorithm_sinc:   return "Sinc";
    }

    return "Unknown";
}

const char* ma_dither_mode_to_string(ma_dither_mode ditherMode)
{
    switch (ditherMode) {
        case ma_dither_mode_none:      return "None";
        case ma_dither_mode_rectangle: return "Rectangle";
        case ma_dither_mode_triangle:  return "Triangle";
    }

    return "Unkown";
}




///////////////////////////////////////////////////////////////////////////////
//
// Format Conversion
//
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    void* pBaseData;
    ma_uint64 sampleCount;
    ma_uint64 iNextSample;
} format_conversion_data;

void pcm_convert__reference(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 sampleCount, ma_dither_mode ditherMode)
{
    switch (formatIn)
    {
        case ma_format_u8:
        {
            switch (formatOut)
            {
                case ma_format_s16: ma_pcm_u8_to_s16__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_u8_to_s24__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_u8_to_s32__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_u8_to_f32__reference(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s16:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s16_to_u8__reference( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s16_to_s24__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s16_to_s32__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s16_to_f32__reference(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s24:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s24_to_u8__reference( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s24_to_s16__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s24_to_s32__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s24_to_f32__reference(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s32_to_u8__reference( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s32_to_s16__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s32_to_s24__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s32_to_f32__reference(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_f32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_f32_to_u8__reference( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_f32_to_s16__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_f32_to_s24__reference(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_f32_to_s32__reference(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        default: break;
    }
}

void pcm_convert__optimized(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 sampleCount, ma_dither_mode ditherMode)
{
    switch (formatIn)
    {
        case ma_format_u8:
        {
            switch (formatOut)
            {
                case ma_format_s16: ma_pcm_u8_to_s16__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_u8_to_s24__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_u8_to_s32__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_u8_to_f32__optimized(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s16:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s16_to_u8__optimized( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s16_to_s24__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s16_to_s32__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s16_to_f32__optimized(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s24:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s24_to_u8__optimized( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s24_to_s16__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s24_to_s32__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s24_to_f32__optimized(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s32_to_u8__optimized( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s32_to_s16__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s32_to_s24__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s32_to_f32__optimized(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_f32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_f32_to_u8__optimized( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_f32_to_s16__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_f32_to_s24__optimized(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_f32_to_s32__optimized(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        default: break;
    }
}

#if defined(MA_SUPPORT_SSE2)
void pcm_convert__sse2(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 sampleCount, ma_dither_mode ditherMode)
{
    switch (formatIn)
    {
        case ma_format_u8:
        {
            switch (formatOut)
            {
                case ma_format_s16: ma_pcm_u8_to_s16__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_u8_to_s24__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_u8_to_s32__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_u8_to_f32__sse2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s16:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s16_to_u8__sse2( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s16_to_s24__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s16_to_s32__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s16_to_f32__sse2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s24:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s24_to_u8__sse2( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s24_to_s16__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s24_to_s32__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s24_to_f32__sse2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s32_to_u8__sse2( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s32_to_s16__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s32_to_s24__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s32_to_f32__sse2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_f32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_f32_to_u8__sse2( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_f32_to_s16__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_f32_to_s24__sse2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_f32_to_s32__sse2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        default: break;
    }
}
#endif

#if defined(MA_SUPPORT_AVX2)
void pcm_convert__avx(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 sampleCount, ma_dither_mode ditherMode)
{
    switch (formatIn)
    {
        case ma_format_u8:
        {
            switch (formatOut)
            {
                case ma_format_s16: ma_pcm_u8_to_s16__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_u8_to_s24__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_u8_to_s32__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_u8_to_f32__avx2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s16:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s16_to_u8__avx2( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s16_to_s24__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s16_to_s32__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s16_to_f32__avx2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s24:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s24_to_u8__avx2( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s24_to_s16__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s24_to_s32__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s24_to_f32__avx2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s32_to_u8__avx2( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s32_to_s16__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s32_to_s24__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s32_to_f32__avx2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_f32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_f32_to_u8__avx2( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_f32_to_s16__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_f32_to_s24__avx2(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_f32_to_s32__avx2(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        default: break;
    }
}
#endif

#if defined(MA_SUPPORT_AVX512)
void pcm_convert__avx512(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 sampleCount, ma_dither_mode ditherMode)
{
    switch (formatIn)
    {
        case ma_format_u8:
        {
            switch (formatOut)
            {
                case ma_format_s16: ma_pcm_u8_to_s16__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_u8_to_s24__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_u8_to_s32__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_u8_to_f32__avx512(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s16:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s16_to_u8__avx512( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s16_to_s24__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s16_to_s32__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s16_to_f32__avx512(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s24:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s24_to_u8__avx512( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s24_to_s16__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s24_to_s32__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s24_to_f32__avx512(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s32_to_u8__avx512( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s32_to_s16__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s32_to_s24__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s32_to_f32__avx512(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_f32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_f32_to_u8__avx512( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_f32_to_s16__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_f32_to_s24__avx512(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_f32_to_s32__avx512(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        default: break;
    }
}
#endif

#if defined(MA_SUPPORT_NEON)
void pcm_convert__neon(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 sampleCount, ma_dither_mode ditherMode)
{
    switch (formatIn)
    {
        case ma_format_u8:
        {
            switch (formatOut)
            {
                case ma_format_s16: ma_pcm_u8_to_s16__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_u8_to_s24__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_u8_to_s32__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_u8_to_f32__neon(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s16:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s16_to_u8__neon( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s16_to_s24__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s16_to_s32__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s16_to_f32__neon(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s24:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s24_to_u8__neon( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s24_to_s16__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_s24_to_s32__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s24_to_f32__neon(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_s32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_s32_to_u8__neon( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_s32_to_s16__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_s32_to_s24__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_f32: ma_pcm_s32_to_f32__neon(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        case ma_format_f32:
        {
            switch (formatOut)
            {
                case ma_format_u8:  ma_pcm_f32_to_u8__neon( pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s16: ma_pcm_f32_to_s16__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s24: ma_pcm_f32_to_s24__neon(pOut, pIn, sampleCount, ditherMode); return;
                case ma_format_s32: ma_pcm_f32_to_s32__neon(pOut, pIn, sampleCount, ditherMode); return;
                default: break;
            }
        } break;

        default: break;
    }
}
#endif

void pcm_convert(void* pOut, ma_format formatOut, const void* pIn, ma_format formatIn, ma_uint64 sampleCount, ma_dither_mode ditherMode, simd_mode mode)
{
    // For testing, we always reset the seed for dithering so we can get consistent results for comparisons.
    ma_seed(1234);

    switch (mode)
    {
        case simd_mode_scalar:
        {
            pcm_convert__optimized(pOut, formatOut, pIn, formatIn, sampleCount, ditherMode);
        } break;

#if defined(MA_SUPPORT_SSE2)
        case simd_mode_sse2:
        {
            pcm_convert__sse2(pOut, formatOut, pIn, formatIn, sampleCount, ditherMode);
        } break;
#endif
        
#if defined(MA_SUPPORT_AVX2)
        case simd_mode_avx2:
        {
            pcm_convert__avx(pOut, formatOut, pIn, formatIn, sampleCount, ditherMode);
        } break;
#endif

#if defined(MA_SUPPORT_AVX512)
        case simd_mode_avx512:
        {
            pcm_convert__avx512(pOut, formatOut, pIn, formatIn, sampleCount, ditherMode);
        } break;
#endif
        
#if defined(MA_SUPPORT_NEON)
        case simd_mode_neon:
        {
            pcm_convert__neon(pOut, formatOut, pIn, formatIn, sampleCount, ditherMode);
        } break;
#endif

        default: break;
    }
}


int do_profiling__format_conversion__profile_individual(ma_format formatIn, ma_format formatOut, ma_dither_mode ditherMode, const void* pBaseData, ma_uint64 sampleCount, simd_mode mode, const void* pReferenceData, double referenceTime)
{
    void* pTestData = ma_aligned_malloc((size_t)(sampleCount * ma_get_bytes_per_sample(formatOut)), MA_SIMD_ALIGNMENT);
    if (pTestData == NULL) {
        printf("Out of memory.\n");
        return -1;
    }

    ma_timer timer;
    ma_timer_init(&timer);
    double timeTaken = ma_timer_get_time_in_seconds(&timer);
    {
        pcm_convert(pTestData, formatOut, pBaseData, formatIn, sampleCount, ditherMode, mode);
    }
    timeTaken = ma_timer_get_time_in_seconds(&timer) - timeTaken;


    // Compare with the reference for correctness.
    ma_bool32 passed = MA_TRUE;
    for (ma_uint64 iSample = 0; iSample < sampleCount; ++iSample) {
        ma_uint32 bps = ma_get_bytes_per_sample(formatOut);

        // We need to compare on a format by format basis because we allow for very slight deviations in results depending on the output format.
        switch (formatOut)
        {
            case ma_format_s16:
            {
                ma_int16 a = ((const ma_int16*)pReferenceData)[iSample];
                ma_int16 b = ((const ma_int16*)pTestData)[iSample];
                if (abs(a-b) > 0) {
                    printf("Incorrect Sample: (%d) %d != %d\n", (int)iSample, a, b);
                    passed = MA_FALSE;
                }
            } break;

            default:
            {
                if (memcmp(ma_offset_ptr(pReferenceData, iSample*bps), ma_offset_ptr(pTestData, iSample*bps), bps) != 0) {
                    printf("Incorrect Sample: (%d)\n", (int)iSample);
                    passed = MA_FALSE;
                }
            } break;
        }
    }

    if (passed) {
        printf("  [PASSED] ");
    } else {
        printf("  [FAILED] ");
    }
    printf("(Dither = %s) %s -> %s (%s): %.4fms (%.2f%%)\n", ma_dither_mode_to_string(ditherMode), ma_get_format_name(formatIn), ma_get_format_name(formatOut), simd_mode_to_string(mode), timeTaken*1000, referenceTime/timeTaken*100);

    ma_aligned_free(pTestData);
    return 0;
}

int do_profiling__format_conversion__profile_set(ma_format formatIn, ma_format formatOut, ma_dither_mode ditherMode)
{
    // Generate our base data to begin with. This is generated from an f32 sine wave which is converted to formatIn. That then becomes our base data.
    ma_uint32 sampleCount = 10000000;

    float* pSourceData = (float*)ma_aligned_malloc(sampleCount*sizeof(*pSourceData), MA_SIMD_ALIGNMENT);
    if (pSourceData == NULL) {
        printf("Out of memory.\n");
        return -1;
    }

    ma_sine_wave sineWave;
    ma_sine_wave_init(1.0, 400, 48000, &sineWave);
    ma_sine_wave_read_f32(&sineWave, sampleCount, pSourceData);

    void* pBaseData = ma_aligned_malloc(sampleCount * ma_get_bytes_per_sample(formatIn), MA_SIMD_ALIGNMENT);
    ma_pcm_convert(pBaseData, formatIn, pSourceData, ma_format_f32, sampleCount, ma_dither_mode_none);


    // Reference first so we can get a benchmark.
    void* pReferenceData = ma_aligned_malloc(sampleCount * ma_get_bytes_per_sample(formatOut), MA_SIMD_ALIGNMENT);
    ma_timer timer;
    ma_timer_init(&timer);
    double referenceTime = ma_timer_get_time_in_seconds(&timer);
    {
        pcm_convert__reference(pReferenceData, formatOut, pBaseData, formatIn, sampleCount, ditherMode);
    }
    referenceTime = ma_timer_get_time_in_seconds(&timer) - referenceTime;

    
    // Here is where each optimized implementation is profiled.
    do_profiling__format_conversion__profile_individual(formatIn, formatOut, ditherMode, pBaseData, sampleCount, simd_mode_scalar, pReferenceData, referenceTime);

    if (ma_has_sse2()) {
        do_profiling__format_conversion__profile_individual(formatIn, formatOut, ditherMode, pBaseData, sampleCount, simd_mode_sse2, pReferenceData, referenceTime);
    }
    if (ma_has_avx2()) {
        do_profiling__format_conversion__profile_individual(formatIn, formatOut, ditherMode, pBaseData, sampleCount, simd_mode_avx2, pReferenceData, referenceTime);
    }
    if (ma_has_avx512f()) {
        do_profiling__format_conversion__profile_individual(formatIn, formatOut, ditherMode, pBaseData, sampleCount, simd_mode_avx512, pReferenceData, referenceTime);
    }
    if (ma_has_neon()) {
        do_profiling__format_conversion__profile_individual(formatIn, formatOut, ditherMode, pBaseData, sampleCount, simd_mode_neon, pReferenceData, referenceTime);
    }



    ma_aligned_free(pReferenceData);
    ma_aligned_free(pBaseData);
    ma_aligned_free(pSourceData);
    return 0;
}

int do_profiling__format_conversion()
{
    // First we need to generate our base data.


    do_profiling__format_conversion__profile_set(ma_format_f32, ma_format_s16, ma_dither_mode_none);

    return 0;
}



///////////////////////////////////////////////////////////////////////////////
//
// Channel Routing
//
///////////////////////////////////////////////////////////////////////////////

float g_ChannelRouterProfilingOutputBenchmark[8][48000];
float g_ChannelRouterProfilingOutput[8][48000];
double g_ChannelRouterTime_Reference = 0;
double g_ChannelRouterTime_SSE2 = 0;
double g_ChannelRouterTime_AVX2 = 0;
double g_ChannelRouterTime_AVX512 = 0;
double g_ChannelRouterTime_NEON = 0;

ma_sine_wave g_sineWave;

ma_bool32 channel_router_test(ma_uint32 channels, ma_uint64 frameCount, float** ppFramesA, float** ppFramesB)
{
    for (ma_uint32 iChannel = 0; iChannel < channels; ++iChannel) {
        for (ma_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
            if (ppFramesA[iChannel][iFrame] != ppFramesB[iChannel][iFrame]) {
                return MA_FALSE;
            }
        }
    }

    return MA_TRUE;
}

ma_uint32 channel_router_on_read(ma_channel_router* pRouter, ma_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    (void)pUserData;
    (void)pRouter;

    float** ppSamplesOutF = (float**)ppSamplesOut;

    for (ma_uint32 iChannel = 0; iChannel < pRouter->config.channelsIn; ++iChannel) {
        ma_sine_wave_init(1/(iChannel+1), 400, 48000, &g_sineWave);
        ma_sine_wave_read_f32(&g_sineWave, frameCount, ppSamplesOutF[iChannel]);
    }

    return frameCount;
}

int do_profiling__channel_routing()
{
    ma_result result;

    // When profiling we need to compare against a benchmark to ensure the optimization is implemented correctly. We always
    // use the reference implementation for our benchmark.
    ma_uint32 channels = ma_countof(g_ChannelRouterProfilingOutputBenchmark);
    ma_channel channelMapIn[MA_MAX_CHANNELS];
    ma_get_standard_channel_map(ma_standard_channel_map_default, channels, channelMapIn);
    ma_channel channelMapOut[MA_MAX_CHANNELS];
    ma_get_standard_channel_map(ma_standard_channel_map_default, channels, channelMapOut);

    ma_channel_router_config routerConfig = ma_channel_router_config_init(channels, channelMapIn, channels, channelMapOut, ma_channel_mix_mode_planar_blend, channel_router_on_read, NULL);

    ma_channel_router router;
    result = ma_channel_router_init(&routerConfig, &router);
    if (result != MA_SUCCESS) {
        return -1;
    }

    // Disable optimizations for our tests.
    router.isPassthrough = MA_FALSE;
    router.isSimpleShuffle = MA_FALSE;
    router.useSSE2 = MA_FALSE;
    router.useAVX2 = MA_FALSE;
    router.useAVX512 = MA_FALSE;
    router.useNEON = MA_FALSE;

    ma_uint64 framesToRead = ma_countof(g_ChannelRouterProfilingOutputBenchmark[0]);

    // Benchmark
    void* ppOutBenchmark[8];
    for (int i = 0; i < 8; ++i) {
        ppOutBenchmark[i] = (void*)g_ChannelRouterProfilingOutputBenchmark[i];
    }

    ma_sine_wave_init(1, 400, 48000, &g_sineWave);
    ma_uint64 framesRead = ma_channel_router_read_deinterleaved(&router, framesToRead, ppOutBenchmark, NULL);
    if (framesRead != framesToRead) {
        printf("Channel Router: An error occurred while reading benchmark data.\n");
    }

    void* ppOut[8];
    for (int i = 0; i < 8; ++i) {
        ppOut[i] = (void*)g_ChannelRouterProfilingOutput[i];
    }


    printf("Channel Routing\n");
    printf("===============\n");

    // Reference
    {
        ma_timer timer;
        ma_timer_init(&timer);
        double startTime = ma_timer_get_time_in_seconds(&timer);

        framesRead = ma_channel_router_read_deinterleaved(&router, framesToRead, ppOut, NULL);
        if (framesRead != framesToRead) {
            printf("Channel Router: An error occurred while reading reference data.\n");
        }

        if (!channel_router_test(channels, framesRead, (float**)ppOutBenchmark, (float**)ppOut)) {
            printf("  [ERROR] ");
        } else {
            printf("  [PASSED] ");
        }

        g_ChannelRouterTime_Reference = ma_timer_get_time_in_seconds(&timer) - startTime;
        printf("Reference: %.4fms (%.2f%%)\n", g_ChannelRouterTime_Reference*1000, g_ChannelRouterTime_Reference/g_ChannelRouterTime_Reference*100);
    }

    // SSE2
    if (ma_has_sse2()) {
        router.useSSE2 = MA_TRUE;
        ma_timer timer;
        ma_timer_init(&timer);
        double startTime = ma_timer_get_time_in_seconds(&timer);

        framesRead = ma_channel_router_read_deinterleaved(&router, framesToRead, ppOut, NULL);
        if (framesRead != framesToRead) {
            printf("Channel Router: An error occurred while reading SSE2 data.\n");
        }

        g_ChannelRouterTime_SSE2 = ma_timer_get_time_in_seconds(&timer) - startTime;
        router.useSSE2 = MA_FALSE;

        if (!channel_router_test(channels, framesRead, (float**)ppOutBenchmark, (float**)ppOut)) {
            printf("  [ERROR] ");
        } else {
            printf("  [PASSED] ");
        }

        printf("SSE2: %.4fms (%.2f%%)\n", g_ChannelRouterTime_SSE2*1000, g_ChannelRouterTime_Reference/g_ChannelRouterTime_SSE2*100);
    }

    // AVX2
    if (ma_has_avx2()) {
        router.useAVX2 = MA_TRUE;
        ma_timer timer;
        ma_timer_init(&timer);
        double startTime = ma_timer_get_time_in_seconds(&timer);

        framesRead = ma_channel_router_read_deinterleaved(&router, framesToRead, ppOut, NULL);
        if (framesRead != framesToRead) {
            printf("Channel Router: An error occurred while reading AVX2 data.\n");
        }

        g_ChannelRouterTime_AVX2 = ma_timer_get_time_in_seconds(&timer) - startTime;
        router.useAVX2 = MA_FALSE;

        if (!channel_router_test(channels, framesRead, (float**)ppOutBenchmark, (float**)ppOut)) {
            printf("  [ERROR] ");
        } else {
            printf("  [PASSED] ");
        }

        printf("AVX2: %.4fms (%.2f%%)\n", g_ChannelRouterTime_AVX2*1000, g_ChannelRouterTime_Reference/g_ChannelRouterTime_AVX2*100);
    }

    // NEON
    if (ma_has_neon()) {
        router.useNEON = MA_TRUE;
        ma_timer timer;
        ma_timer_init(&timer);
        double startTime = ma_timer_get_time_in_seconds(&timer);

        framesRead = ma_channel_router_read_deinterleaved(&router, framesToRead, ppOut, NULL);
        if (framesRead != framesToRead) {
            printf("Channel Router: An error occurred while reading NEON data.\n");
        }

        g_ChannelRouterTime_NEON = ma_timer_get_time_in_seconds(&timer) - startTime;
        router.useNEON = MA_FALSE;

        if (!channel_router_test(channels, framesRead, (float**)ppOutBenchmark, (float**)ppOut)) {
            printf("  [ERROR] ");
        } else {
            printf("  [PASSED] ");
        }

        printf("NEON: %.4fms (%.2f%%)\n", g_ChannelRouterTime_NEON*1000, g_ChannelRouterTime_Reference/g_ChannelRouterTime_NEON*100);
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//
// SRC
//
///////////////////////////////////////////////////////////////////////////////

typedef struct
{
    float* pFrameData[MA_MAX_CHANNELS];
    ma_uint64 frameCount;
    ma_uint32 channels;
    double timeTaken;
} src_reference_data;

typedef struct
{
    float* pFrameData[MA_MAX_CHANNELS];
    ma_uint64 frameCount;
    ma_uint64 iNextFrame;
    ma_uint32 channels;
} src_data;

ma_uint32 do_profiling__src__on_read(ma_src* pSRC, ma_uint32 frameCount, void** ppSamplesOut, void* pUserData)
{
    src_data* pBaseData = (src_data*)pUserData;
    ma_assert(pBaseData != NULL);
    ma_assert(pBaseData->iNextFrame <= pBaseData->frameCount);

    ma_uint64 framesToRead = frameCount;

    ma_uint64 framesAvailable = pBaseData->frameCount - pBaseData->iNextFrame;
    if (framesToRead > framesAvailable) {
        framesToRead = framesAvailable;
    }

    if (framesToRead > 0) {
        for (ma_uint32 iChannel = 0; iChannel < pSRC->config.channels; iChannel += 1) {
            ma_copy_memory(ppSamplesOut[iChannel], pBaseData->pFrameData[iChannel], (size_t)(framesToRead * sizeof(float)));
        }
    }

    pBaseData->iNextFrame += framesToRead;
    return (ma_uint32)framesToRead;
}

ma_result init_src(src_data* pBaseData, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_src_algorithm algorithm, simd_mode mode, ma_src* pSRC)
{
    ma_assert(pBaseData != NULL);
    ma_assert(pSRC != NULL);

    ma_src_config srcConfig = ma_src_config_init(sampleRateIn, sampleRateOut, pBaseData->channels, do_profiling__src__on_read, pBaseData);
    srcConfig.sinc.windowWidth = 17;    // <-- Make this an odd number to test unaligned section in the SIMD implementations.
    srcConfig.algorithm = algorithm;
    srcConfig.noSSE2    = MA_TRUE;
    srcConfig.noAVX2    = MA_TRUE;
    srcConfig.noAVX512  = MA_TRUE;
    srcConfig.noNEON    = MA_TRUE;
    switch (mode) {
        case simd_mode_sse2:   srcConfig.noSSE2   = MA_FALSE; break;
        case simd_mode_avx2:   srcConfig.noAVX2   = MA_FALSE; break;
        case simd_mode_avx512: srcConfig.noAVX512 = MA_FALSE; break;
        case simd_mode_neon:   srcConfig.noNEON   = MA_FALSE; break;
        case simd_mode_scalar:
        default: break;
    }

    ma_result result = ma_src_init(&srcConfig, pSRC);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize sample rate converter.\n");
        return (int)result;
    }

    return result;
}

int do_profiling__src__profile_individual(src_data* pBaseData, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_src_algorithm algorithm, simd_mode mode, src_reference_data* pReferenceData)
{
    ma_assert(pBaseData != NULL);
    ma_assert(pReferenceData != NULL);

    ma_result result = MA_ERROR;

    // Make sure the base data is moved back to the start.
    pBaseData->iNextFrame = 0;

    ma_src src;
    result = init_src(pBaseData, sampleRateIn, sampleRateOut, algorithm, mode, &src);
    if (result != MA_SUCCESS) {
        return (int)result;
    }


    // Profiling.
    ma_uint64 sz = pReferenceData->frameCount * sizeof(float);
    ma_assert(sz <= SIZE_MAX);

    float* pFrameData[MA_MAX_CHANNELS];
    for (ma_uint32 iChannel = 0; iChannel < pBaseData->channels; iChannel += 1) {
        pFrameData[iChannel] = (float*)ma_aligned_malloc((size_t)sz, MA_SIMD_ALIGNMENT);
        if (pFrameData[iChannel] == NULL) {
            printf("Out of memory.\n");
            return -2;
        }
        ma_zero_memory(pFrameData[iChannel], (size_t)sz);
    }

    ma_timer timer;
    ma_timer_init(&timer);
    double startTime = ma_timer_get_time_in_seconds(&timer);
    {
        ma_src_read_deinterleaved(&src, pReferenceData->frameCount, (void**)pFrameData, pBaseData);
    }
    double timeTaken = ma_timer_get_time_in_seconds(&timer) - startTime;


    // Correctness test.
    ma_bool32 passed = MA_TRUE;
    for (ma_uint32 iChannel = 0; iChannel < pReferenceData->channels; iChannel += 1) {
        for (ma_uint32 iFrame = 0; iFrame < pReferenceData->frameCount; iFrame += 1) {
            float s0 = pReferenceData->pFrameData[iChannel][iFrame];
            float s1 =                 pFrameData[iChannel][iFrame];
            //if (s0 != s1) {
            if (fabs(s0 - s1) > 0.000001) {
                printf("(Channel %d, Sample %d) %f != %f\n", iChannel, iFrame, s0, s1);
                passed = MA_FALSE;
            }
        }
    }


    // Print results.
    if (passed) {
        printf("  [PASSED] ");
    } else {
        printf("  [FAILED] ");
    }
    printf("%s %d -> %d (%s): %.4fms (%.2f%%)\n", ma_src_algorithm_to_string(algorithm), sampleRateIn, sampleRateOut, simd_mode_to_string(mode), timeTaken*1000, pReferenceData->timeTaken/timeTaken*100);


    for (ma_uint32 iChannel = 0; iChannel < pBaseData->channels; iChannel += 1) {
        ma_aligned_free(pFrameData[iChannel]);
    }

    return (int)result;
}

int do_profiling__src__profile_set(src_data* pBaseData, ma_uint32 sampleRateIn, ma_uint32 sampleRateOut, ma_src_algorithm algorithm)
{
    ma_assert(pBaseData != NULL);

    // Make sure the base data is back at the start.
    pBaseData->iNextFrame = 0;

    src_reference_data referenceData;
    ma_zero_object(&referenceData);
    referenceData.channels = pBaseData->channels;

    // The first thing to do is to perform a sample rate conversion using the scalar/reference implementation. This reference is used to compare
    // the results of the optimized implementation.
    referenceData.frameCount = ma_calculate_frame_count_after_src(sampleRateOut, sampleRateIn, pBaseData->frameCount);
    if (referenceData.frameCount == 0) {
        printf("Failed to calculate output frame count.\n");
        return -1;
    }

    ma_uint64 sz = referenceData.frameCount * sizeof(float);
    ma_assert(sz <= SIZE_MAX);

    for (ma_uint32 iChannel = 0; iChannel < referenceData.channels; iChannel += 1) {
        referenceData.pFrameData[iChannel] = (float*)ma_aligned_malloc((size_t)sz, MA_SIMD_ALIGNMENT);
        if (referenceData.pFrameData[iChannel] == NULL) {
            printf("Out of memory.\n");
            return -2;
        }
        ma_zero_memory(referenceData.pFrameData[iChannel], (size_t)sz);
    }


    // Generate the reference data.
    ma_src src;
    ma_result result = init_src(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_scalar, &src);
    if (result != MA_SUCCESS) {
        return (int)result;
    }

    ma_timer timer;
    ma_timer_init(&timer);
    double startTime = ma_timer_get_time_in_seconds(&timer);
    {
        ma_src_read_deinterleaved(&src, referenceData.frameCount, (void**)referenceData.pFrameData, pBaseData);
    }
    referenceData.timeTaken = ma_timer_get_time_in_seconds(&timer) - startTime;


    // Now that we have the reference data to compare against we can go ahead and measure the SIMD optimizations.
    do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_scalar, &referenceData);
    if (ma_has_sse2()) {
        do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_sse2, &referenceData);
    }
    if (ma_has_avx2()) {
        do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_avx2, &referenceData);
    }
    if (ma_has_avx512f()) {
        do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_avx512, &referenceData);
    }
    if (ma_has_neon()) {
        do_profiling__src__profile_individual(pBaseData, sampleRateIn, sampleRateOut, algorithm, simd_mode_neon, &referenceData);
    }
    

    for (ma_uint32 iChannel = 0; iChannel < referenceData.channels; iChannel += 1) {
        ma_aligned_free(referenceData.pFrameData[iChannel]);
    }

    return 0;
}

int do_profiling__src()
{
    printf("Sample Rate Conversion\n");
    printf("======================\n");

    // Set up base data.
    src_data baseData;
    ma_zero_object(&baseData);
    baseData.channels = 8;
    baseData.frameCount = 100000;
    for (ma_uint32 iChannel = 0; iChannel < baseData.channels; ++iChannel) {
        baseData.pFrameData[iChannel] = (float*)ma_aligned_malloc((size_t)(baseData.frameCount * sizeof(float)), MA_SIMD_ALIGNMENT);
        if (baseData.pFrameData[iChannel] == NULL) {
            printf("Out of memory.\n");
            return -1;
        }

        ma_sine_wave sineWave;
        ma_sine_wave_init(1.0f, 400 + (iChannel*50), 48000, &sineWave);
        ma_sine_wave_read_f32(&sineWave, baseData.frameCount, baseData.pFrameData[iChannel]);
    }
    

    // Upsampling.
    do_profiling__src__profile_set(&baseData, 44100, 48000, ma_src_algorithm_sinc);

    // Downsampling.
    do_profiling__src__profile_set(&baseData, 48000, 44100, ma_src_algorithm_sinc);


    for (ma_uint32 iChannel = 0; iChannel < baseData.channels; iChannel += 1) {
        ma_aligned_free(baseData.pFrameData[iChannel]);
    }

    return 0;
}

#if 0
// Converts two 4xf32 vectors to one 8xi16 vector with signed saturation.
__m128i drmath_vf32_to_vi16__sse2(__m128 f32_0, __m128 f32_1)
{    
    return _mm_packs_epi32(_mm_cvttps_epi32(f32_0), _mm_cvttps_epi32(f32_1));
}

__m256i drmath_vf32_to_vi16__avx(__m256 f32_0, __m256 f32_1)
{
    __m256i i0 = _mm256_cvttps_epi32(f32_0);
    __m256i i1 = _mm256_cvttps_epi32(f32_1);
    __m256i p0 = _mm256_permute2x128_si256(i0, i1, 32);
    __m256i p1 = _mm256_permute2x128_si256(i0, i1, 49);
    __m256i r  = _mm256_packs_epi32(p0, p1);
    return r;
}
#endif

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;


    {
        //__m128  f0 = _mm_set_ps(32780, 2, 1, 0);
        //__m128  f1 = _mm_set_ps(-32780, 6, 5, 4);
        //__m128i r = drmath_vf32_to_vi16__sse2(f0, f1);

        //__m256  f0 = _mm256_set_ps(7,  6,  5,  4,  3,  2,  1, 0);
        //__m256  f1 = _mm256_set_ps(15, 14, 13, 12, 11, 10, 9, 8);
        //__m256i r = drmath_vf32_to_vi16__avx(f0, f1);
        //
        //int a = 5;
    }



    // Summary.
    if (ma_has_sse2()) {
        printf("Has SSE2:     YES\n");
    } else {
        printf("Has SSE2:     NO\n");
    }
    if (ma_has_avx2()) {
        printf("Has AVX2:     YES\n");
    } else {
        printf("Has AVX2:     NO\n");
    }
    if (ma_has_avx512f()) {
        printf("Has AVX-512F: YES\n");
    } else {
        printf("Has AVX-512F: NO\n");
    }
    if (ma_has_neon()) {
        printf("Has NEON:     YES\n");
    } else {
        printf("Has NEON:     NO\n");
    }


    printf("\n");

    // Format conversion.
    do_profiling__format_conversion();
    printf("\n\n");

    // Channel routing.
    do_profiling__channel_routing();
    printf("\n\n");

    // Sample rate conversion.
    do_profiling__src();
    printf("\n\n");


    printf("Press any key to quit...\n");
    getchar();

    return 0;
}