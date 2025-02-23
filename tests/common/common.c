#include "../../miniaudio.c"
#include "../../external/fs/fs.c"

#include <stdio.h>

#define MAX_TESTS       64
#define TEST_OUTPUT_DIR "output"

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

int ma_run_tests(int argc, char** argv)
{
    int result;
    ma_bool32 hasError = MA_FALSE;
    size_t iTest;

    fs_mkdir(NULL, TEST_OUTPUT_DIR);

    for (iTest = 0; iTest < g_Tests.count; iTest += 1) {
        printf("=== BEGIN %s ===\n", g_Tests.pTests[iTest].pName);
        result = g_Tests.pTests[iTest].onEntry(argc, argv);
        printf("=== END %s : %s ===\n", g_Tests.pTests[iTest].pName, (result == 0) ? "PASSED" : "FAILED");

        if (result != 0) {
            hasError = MA_TRUE;
        }
    }

    if (hasError) {
        return 1;  /* Something failed. */
    } else {
        return 0;   /* Everything passed. */
    }
}

