#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"

#define DR_WAV_IMPLEMENTATION
#include "../../extras/dr_wav.h"

#include <stdio.h>

static drwav_data_format drwav_data_format_from_minaudio_format(ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    drwav_data_format wavFormat;

    wavFormat.container     = drwav_container_riff;
    wavFormat.channels      = channels;
    wavFormat.sampleRate    = sampleRate;
    wavFormat.bitsPerSample = ma_get_bytes_per_sample(format) * 8;

    if (format == ma_format_f32) {
        wavFormat.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    } else {
        wavFormat.format = DR_WAVE_FORMAT_PCM;
    }

    return wavFormat;
}

#include "ma_test_generation_noise.c"
#include "ma_test_generation_waveform.c"

#define MAX_TESTS   64

typedef int (* ma_test_entry_proc)(int argc, char** argv);

typedef struct
{
    const char* pName;
    ma_test_entry_proc onEntry;
} ma_test;

struct
{
    ma_test pTests[MAX_TESTS];
    size_t count;
} g_Tests;

ma_result ma_register_test(const char* pName, ma_test_entry_proc onEntry)
{
    MA_ASSERT(pName != NULL);
    MA_ASSERT(onEntry != NULL);

    if (g_Tests.count >= MAX_TESTS) {
        printf("Failed to register test %s because there are too many tests already registered. Increase the value of MAX_TESTS\n", pName);
        return MA_INVALID_OPERATION;
    }

    g_Tests.pTests[g_Tests.count].pName   = pName;
    g_Tests.pTests[g_Tests.count].onEntry = onEntry;
    g_Tests.count += 1;

    return MA_SUCCESS;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    size_t iTest;

    (void)argc;
    (void)argv;

    result = ma_register_test("Noise", test_entry__noise);
    if (result != MA_SUCCESS) {
        return result;
    }

    result = ma_register_test("Waveform", test_entry__waveform);
    if (result != MA_SUCCESS) {
        return result;
    }

    for (iTest = 0; iTest < g_Tests.count; iTest += 1) {
        printf("=== BEGIN %s ===\n", g_Tests.pTests[iTest].pName);
        result = g_Tests.pTests[iTest].onEntry(argc, argv);
        printf("=== END %s : %s ===\n", g_Tests.pTests[iTest].pName, (result == 0) ? "PASSED" : "FAILED");

        if (result != 0) {
            hasError = MA_TRUE;
        }
    }

    if (hasError) {
        return -1;  /* Something failed. */
    } else {
        return 0;   /* Everything passed. */
    }
}
