#define MA_NO_DEVICE_IO
#include "../test_common/ma_test_common.c"

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
