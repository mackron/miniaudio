#define MA_NO_DEVICE_IO
#include "../common/common.c"

#include <stdlib.h>

#include "resource_manager_async_reload.c"

int main(int argc, char** argv)
{
    ma_register_test("Async Reload", test_entry__resource_manager_async_reload);

    return ma_run_tests(argc, argv);
}
