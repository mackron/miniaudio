#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"

#include <stdio.h>

#define MAX_TESTS       64
#define TEST_OUTPUT_DIR "res/output"

typedef int (* ma_test_entry_proc)(int argc, char** argv);

typedef struct
{
    const char* pName;
    ma_test_entry_proc onEntry;
} ma_test;

static struct
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

drwav_data_format drwav_data_format_from_minaudio_format(ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
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
