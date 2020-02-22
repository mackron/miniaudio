#include "../test_common/ma_test_common.c"

#include "ma_test_filtering_dithering.c"
#include "ma_test_filtering_lpf.c"
#include "ma_test_filtering_hpf.c"
#include "ma_test_filtering_bpf.c"

int main(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    size_t iTest;

    (void)argc;
    (void)argv;

    result = ma_register_test("Dithering", test_entry__dithering);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = ma_register_test("Low-Pass Filtering", test_entry__lpf);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = ma_register_test("High-Pass Filtering", test_entry__hpf);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = ma_register_test("Band-Pass Filtering", test_entry__bpf);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
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
