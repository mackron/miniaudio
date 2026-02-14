#define MA_NO_THREADING
#define MA_NO_DEVICE_IO
#include "../common/common.c"

ma_result init_data_converter(ma_uint32 rateIn, ma_uint32 rateOut, ma_resample_algorithm algorithm, ma_data_converter* pDataConverter)
{
    ma_result result;
    ma_data_converter_config config;

    config = ma_data_converter_config_init(ma_format_s16, ma_format_s16, 1, 1, rateIn, rateOut);
    config.resampling.algorithm = algorithm;

    result = ma_data_converter_init(&config, NULL, pDataConverter);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

#if 0
ma_result test_data_converter__passthrough()
{
    /*
    Notes:
      - The isPassthrough flag should be set to true. Both the positive and negative cases need to be tested.
      - ma_data_converter_set_rate() should fail with MA_INVALID_OPERATION.
      - The output should be identical to the input.
    */
    printf("Passthrough\n");

    

    return MA_SUCCESS;
}
#endif


int test_entry__data_converter(int argc, char** argv)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    (void)argc;
    (void)argv;

#if 0
    result = test_data_converter__passthrough();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }
#endif

    (void)result;

    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}



int main(int argc, char** argv)
{
    ma_register_test("Data Conversion", test_entry__data_converter);

    return ma_run_tests(argc, argv);
}
