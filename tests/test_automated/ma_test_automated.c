
#include "../test_common/ma_test_common.c"
#include "ma_test_automated_data_converter.c"

int main(int argc, char** argv)
{
    ma_register_test("Data Conversion", test_entry__data_converter);

    return ma_run_tests(argc, argv);
}
