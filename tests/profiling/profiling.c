#if 1
#include "../../miniaudio.c"
#else
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio-11.h"
#endif


void profile_sizeof()
{
    printf("sizeof(ma_context):           %lu\n", sizeof(ma_context));
    printf("sizeof(ma_device):            %lu\n", sizeof(ma_device));
    printf("sizeof(ma_device.playback):   %lu\n", sizeof(((ma_device*)0)->playback));
    printf("sizeof(ma_duplex_rb):         %lu\n", sizeof(ma_duplex_rb));
    printf("sizeof(ma_data_converter):    %lu\n", sizeof(ma_data_converter));
    printf("sizeof(ma_channel_converter): %lu\n", sizeof(ma_channel_converter));
    printf("sizeof(ma_resampler):         %lu\n", sizeof(ma_resampler));
    printf("sizeof(ma_lpf):               %lu\n", sizeof(ma_lpf));
    printf("sizeof(ma_mutex):             %lu\n", sizeof(ma_mutex));
    printf("sizeof(ma_event):             %lu\n", sizeof(ma_event));
    printf("\n");
}


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

void fill_debug_frames(void* pBuffer, ma_format format, ma_uint32 channels, ma_uint32 frameCount, ma_uint8 valueOffset)
{
    ma_uint8 v = valueOffset;
    ma_uint32 bpf = ma_get_bytes_per_frame(format, channels);
    ma_uint32 iByte;

    /* Just fill byte-by-byte. */
    for (iByte = 0; iByte < frameCount * bpf; iByte += 1) {
        ((ma_uint8*)pBuffer)[iByte] = v;
        v += 1; /* Just let this overflow. */
    }
}

ma_bool32 compare_interleaved_deinterleaved(ma_format format, ma_uint32 channels, ma_uint32 frameCount, void* pInterleaved, const void** ppDeinterleaved)
{
    ma_uint32 sampleSizeInBytes = ma_get_bytes_per_sample(format);
    ma_uint32 iPCMFrame;
    
    for (iPCMFrame = 0; iPCMFrame < frameCount; iPCMFrame += 1) {
        ma_uint32 iChannel;
        for (iChannel = 0; iChannel < channels; iChannel += 1) {
            const void* pDeinterleavedSample = ma_offset_ptr(ppDeinterleaved[iChannel], iPCMFrame * sampleSizeInBytes);
            const void* pInterleavedSample   = ma_offset_ptr(pInterleaved, (iPCMFrame * channels + iChannel) * sampleSizeInBytes);
            
            if (memcmp(pDeinterleavedSample, pInterleavedSample, sampleSizeInBytes) != 0) {
                return MA_FALSE;
            }
        }
    }
    
    return MA_TRUE;
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

    fill_debug_frames(pInterleavedReference, format, channels, frameCount, 0);
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

    printf("Deinterleave: %s %u: ", format_short_name(format), channels);

    if (verify_deinterleaving_by_format(format, channels) == MA_FALSE) {
        printf("FAILED\n");
        return;
    }


    ma_timer_init(&timer);

    pInterleaved = ma_malloc(frameCount * bpf, NULL);
    
    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        pDeinterleaved[iChannel] = ma_malloc(frameCount * ma_get_bytes_per_sample(format), NULL);
    }

    fill_debug_frames(pInterleaved, format, channels, frameCount, 0);


    startTime = ma_timer_get_time_in_seconds(&timer);
    {
        ma_uint32 i;
        for (i = 0; i < iterationCount; i += 1) {
            ma_deinterleave_pcm_frames(format, channels, frameCount, pInterleaved, pDeinterleaved);
        }
    }
    printf("%f\n", ma_timer_get_time_in_seconds(&timer) - startTime);

    /*
    I think Clang can recognize that we're not actually doing anything with the output data of our tests and then
    optimizes out the entire thing. We'll do a simple comparision here.
    */
    if (compare_interleaved_deinterleaved(format, channels, frameCount, pInterleaved, (const void**)pDeinterleaved) == MA_FALSE) {
        printf("FAILED VERIFICATION\n");
    }


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


void interleave_reference(ma_format format, ma_uint32 channels, ma_uint64 frameCount, const void** ppDeinterleavedPCMFrames, void* pInterleavedPCMFrames)
{
    ma_uint32 sampleSizeInBytes = ma_get_bytes_per_sample(format);
    ma_uint64 iPCMFrame;
    for (iPCMFrame = 0; iPCMFrame < frameCount; ++iPCMFrame) {
        ma_uint32 iChannel;
        for (iChannel = 0; iChannel < channels; ++iChannel) {
                  void* pDst = ma_offset_ptr(pInterleavedPCMFrames, (iPCMFrame*channels+iChannel)*sampleSizeInBytes);
            const void* pSrc = ma_offset_ptr(ppDeinterleavedPCMFrames[iChannel], iPCMFrame*sampleSizeInBytes);
            memcpy(pDst, pSrc, sampleSizeInBytes);
        }
    }
}

ma_bool32 verify_interleaving_by_format(ma_format format, ma_uint32 channels)
{
    ma_uint64 frameCount = 1023;    /* <-- Make this odd so we can test that the tail is handled properly from internal loop unrolling. */
    ma_uint32 bpf = ma_get_bytes_per_frame(format, channels);
    void* pDeinterleavedReference[MA_MAX_CHANNELS];
    void* pInterleavedReference;
    void* pDeinterleavedOptimized[MA_MAX_CHANNELS];
    void* pInterleavedOptimized;
    ma_uint32 iChannel;

    pInterleavedReference = ma_malloc(frameCount * bpf, NULL);
    pInterleavedOptimized = ma_malloc(frameCount * bpf, NULL);
    
    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        pDeinterleavedReference[iChannel] = ma_malloc(frameCount * ma_get_bytes_per_sample(format), NULL);
        pDeinterleavedOptimized[iChannel] = ma_malloc(frameCount * ma_get_bytes_per_sample(format), NULL);
    }

    /* Fill deinterleaved buffers with test data. */
    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        fill_debug_frames(pDeinterleavedReference[iChannel], format, 1, frameCount, (ma_uint8)iChannel);    /* Last parameter is to ensure each channel position has different values. */
        MA_COPY_MEMORY(pDeinterleavedOptimized[iChannel], pDeinterleavedReference[iChannel], frameCount * ma_get_bytes_per_sample(format));
    }


    interleave_reference    (format, channels, frameCount, (const void**)pDeinterleavedReference, pInterleavedReference);
    ma_interleave_pcm_frames(format, channels, frameCount, (const void**)pDeinterleavedOptimized, pInterleavedOptimized);

    if (memcmp(pInterleavedReference, pInterleavedOptimized, frameCount * bpf) != 0) {
        return MA_FALSE;
    }


    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        ma_free(pDeinterleavedReference[iChannel], NULL);
        ma_free(pDeinterleavedOptimized[iChannel], NULL);
    }

    ma_free(pInterleavedReference, NULL);
    ma_free(pInterleavedOptimized, NULL);

    return MA_TRUE;
}

void profile_interleaving_by_format(ma_format format, ma_uint32 channels)
{
    ma_uint64 frameCount = 1024 * 1024;
    ma_uint32 iterationCount = 1000;
    ma_uint32 bpf = ma_get_bytes_per_frame(format, channels);
    void* pDeinterleaved[MA_MAX_CHANNELS];
    void* pInterleaved;
    ma_uint32 iChannel;
    ma_timer timer;
    double startTime;

    printf("Interleave: %s %u: ", format_short_name(format), channels);

    if (verify_interleaving_by_format(format, channels) == MA_FALSE) {
        printf("FAILED\n");
        return;
    }


    ma_timer_init(&timer);

    pInterleaved = ma_malloc(frameCount * bpf, NULL);
    
    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        pDeinterleaved[iChannel] = ma_malloc(frameCount * ma_get_bytes_per_sample(format), NULL);
        fill_debug_frames(pDeinterleaved[iChannel], format, 1, frameCount, (ma_uint8)iChannel);
    }


    startTime = ma_timer_get_time_in_seconds(&timer);
    {
        ma_uint32 i;
        for (i = 0; i < iterationCount; i += 1) {
            ma_interleave_pcm_frames(format, channels, frameCount, (const void**)pDeinterleaved, pInterleaved);
        }
    }
    printf("%f\n", ma_timer_get_time_in_seconds(&timer) - startTime);

    /*
    I think Clang can recognize that we're not actually doing anything with the output data of our tests and then
    optimizes out the entire thing. We'll do a simple comparision here.
    */
    if (compare_interleaved_deinterleaved(format, channels, frameCount, pInterleaved, (const void**)pDeinterleaved) == MA_FALSE) {
        printf("FAILED VERIFICATION\n");
    }


    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        ma_free(pDeinterleaved[iChannel], NULL);
    }

    ma_free(pInterleaved, NULL);
}

void profile_interleaving(void)
{
    /* Stereo has an optimized code path. */
    profile_interleaving_by_format(ma_format_u8,  2);
    profile_interleaving_by_format(ma_format_s16, 2);
    profile_interleaving_by_format(ma_format_f32, 2);
    profile_interleaving_by_format(ma_format_s32, 2);
    profile_interleaving_by_format(ma_format_s24, 2);

    /* We have a special case for mono streams so make sure we have coverage of that case. */
    profile_interleaving_by_format(ma_format_u8,  1);
    profile_interleaving_by_format(ma_format_s16, 1);
    profile_interleaving_by_format(ma_format_f32, 1);
    profile_interleaving_by_format(ma_format_s32, 1);
    profile_interleaving_by_format(ma_format_s24, 1);

    /* Channels > 2 run on a generic code path. */
    profile_interleaving_by_format(ma_format_u8,  3);
    profile_interleaving_by_format(ma_format_s16, 3);
    profile_interleaving_by_format(ma_format_f32, 3);
    profile_interleaving_by_format(ma_format_s32, 3);
    profile_interleaving_by_format(ma_format_s24, 3);
}


int main(int argc, char** argv)
{
    profile_sizeof();
    profile_deinterleaving();
    profile_interleaving();

    (void)argc;
    (void)argv;

    return 0;
}
