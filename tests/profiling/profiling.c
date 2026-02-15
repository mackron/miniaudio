#if 1
#include "../../miniaudio.c"
#else
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio-11.h"
#endif

const char* format_short_name(ma_format format)
{
    switch (format)
    {
        case ma_format_u8:  return "u8";
        case ma_format_s16: return "s16";
        case ma_format_s32: return "s32";
        case ma_format_s24: return "s24";
        case ma_format_f32: return "f32";
        default: return "unknown";
    }
}

void deinterleave_reference(ma_format format, ma_uint32 channels, ma_uint64 frameCount, const void* pInterleavedPCMFrames, void** ppDeinterleavedPCMFrames)
{
    ma_uint32 sampleSizeInBytes = ma_get_bytes_per_sample(format);
    ma_uint64 iPCMFrame;
    for (iPCMFrame = 0; iPCMFrame < frameCount; iPCMFrame += 1) {
        ma_uint32 iChannel;
        for (iChannel = 0; iChannel < channels; iChannel += 1) {
                    void* pDst = ma_offset_ptr(ppDeinterleavedPCMFrames[iChannel], iPCMFrame*sampleSizeInBytes);
            const void* pSrc = ma_offset_ptr(pInterleavedPCMFrames, (iPCMFrame*channels+iChannel)*sampleSizeInBytes);
            memcpy(pDst, pSrc, sampleSizeInBytes);
        }
    }
}

void fill_debug_frames(void* pBuffer, ma_format format, ma_uint32 channels, ma_uint32 frameCount)
{
    ma_uint8 v = 0;
    ma_uint32 bpf = ma_get_bytes_per_frame(format, channels);
    ma_uint32 iByte;

    /* Just fill byte-by-byte. */
    for (iByte = 0; iByte < frameCount * bpf; iByte += 1) {
        ((ma_uint8*)pBuffer)[iByte] = v;
        v += 1; /* Just let this overflow. */
    }
}

ma_bool32 verify_deinterleaving_by_format(ma_format format, ma_uint32 channels)
{
    ma_uint64 frameCount = 1023;    /* <-- Make this odd so we can test that the tail is handled properly from internal loop unrolling. */
    ma_uint32 bpf = ma_get_bytes_per_frame(format, channels);
    void* pInterleavedReference;
    void* pDeinterleavedReference[MA_MAX_CHANNELS];
    void* pInterleavedOptimized;
    void* pDeinterleavedOptimized[MA_MAX_CHANNELS];
    ma_uint32 iChannel;

    pInterleavedReference = ma_malloc(frameCount * bpf, NULL);
    pInterleavedOptimized = ma_malloc(frameCount * bpf, NULL);
    
    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        pDeinterleavedReference[iChannel] = ma_malloc(frameCount * ma_get_bytes_per_sample(format), NULL);
        pDeinterleavedOptimized[iChannel] = ma_malloc(frameCount * ma_get_bytes_per_sample(format), NULL);
    }

    fill_debug_frames(pInterleavedReference, format, channels, frameCount);
    MA_COPY_MEMORY(pInterleavedOptimized, pInterleavedReference, frameCount * bpf);


    deinterleave_reference    (format, channels, frameCount, pInterleavedReference, pDeinterleavedReference);
    ma_deinterleave_pcm_frames(format, channels, frameCount, pInterleavedOptimized, pDeinterleavedOptimized);

    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        if (memcmp(pDeinterleavedReference[iChannel], pDeinterleavedOptimized[iChannel], frameCount * ma_get_bytes_per_sample(format)) != 0) {
            return MA_FALSE;
        }
    }


    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        ma_free(pDeinterleavedReference[iChannel], NULL);
        ma_free(pDeinterleavedOptimized[iChannel], NULL);
    }

    ma_free(pInterleavedReference, NULL);
    ma_free(pInterleavedOptimized, NULL);

    return MA_TRUE;
}

void profile_deinterleaving_by_format(ma_format format, ma_uint32 channels)
{
    ma_uint64 frameCount = 1024 * 1024;
    ma_uint32 iterationCount = 1000;
    ma_uint32 bpf = ma_get_bytes_per_frame(format, channels);
    void* pInterleaved;
    void* pDeinterleaved[MA_MAX_CHANNELS];
    ma_uint32 iChannel;
    ma_timer timer;
    double startTime;

    printf("%s %u: ", format_short_name(format), channels);

    if (verify_deinterleaving_by_format(format, channels) == MA_FALSE) {
        printf("FAILED\n");
        return;
    }


    ma_timer_init(&timer);

    pInterleaved = ma_malloc(frameCount * bpf, NULL);
    
    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        pDeinterleaved[iChannel] = ma_malloc(frameCount * ma_get_bytes_per_sample(format), NULL);
    }

    ma_debug_fill_pcm_frames_with_sine_wave((float*)pInterleaved, frameCount, format, channels, 48000); /* The float* cast is to work around an API bug in miniaudio v0.11. It's harmless. */


    startTime = ma_timer_get_time_in_seconds(&timer);
    {
        ma_uint32 i;
        for (i = 0; i < iterationCount; i += 1) {
            ma_deinterleave_pcm_frames(format, channels, frameCount, pInterleaved, pDeinterleaved);
        }
    }
    printf("%f\n", ma_timer_get_time_in_seconds(&timer) - startTime);


    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        ma_free(pDeinterleaved[iChannel], NULL);
    }

    ma_free(pInterleaved, NULL);
}

void profile_deinterleaving(void)
{
    /* Stereo has an optimized code path. */
    profile_deinterleaving_by_format(ma_format_u8,  2);
    profile_deinterleaving_by_format(ma_format_s16, 2);
    profile_deinterleaving_by_format(ma_format_f32, 2);
    profile_deinterleaving_by_format(ma_format_s32, 2);
    profile_deinterleaving_by_format(ma_format_s24, 2);

    /* We have a special case for mono streams so make sure we have coverage of that case. */
    profile_deinterleaving_by_format(ma_format_u8,  1);
    profile_deinterleaving_by_format(ma_format_s16, 1);
    profile_deinterleaving_by_format(ma_format_f32, 1);
    profile_deinterleaving_by_format(ma_format_s32, 1);
    profile_deinterleaving_by_format(ma_format_s24, 1);

    /* Channels > 2 run on a generic code path. */
    profile_deinterleaving_by_format(ma_format_u8,  3);
    profile_deinterleaving_by_format(ma_format_s16, 3);
    profile_deinterleaving_by_format(ma_format_f32, 3);
    profile_deinterleaving_by_format(ma_format_s32, 3);
    profile_deinterleaving_by_format(ma_format_s24, 3);
}

int main(int argc, char** argv)
{
    profile_deinterleaving();

    (void)argc;
    (void)argv;

    return 0;
}
