#define MA_NO_DEVICE_IO
#include "../common/common.c"

#include "resource_manager_stream_fences.c"

int main(int argc, char** argv)
{
    ma_register_test("Stream Fences", test_entry__resource_manager_stream_fences);

    return ma_run_tests(argc, argv);
}
