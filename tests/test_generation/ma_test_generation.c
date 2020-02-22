#include "../test_common/ma_test_common.c"

#define DR_WAV_IMPLEMENTATION
#include "../../extras/dr_wav.h"

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
