#define MA_NO_DEVICE_IO
#include "../../miniaudio.c"

#include <stdio.h>

#define CHECK_RESULT(call, expected)                                             \
    do {                                                                         \
        ma_result testResult = (call);                                            \
        if (testResult != (expected)) {                                           \
            printf("FAIL: %s returned %d, expected %d\n", #call, testResult, (expected)); \
            return 1;                                                            \
        }                                                                        \
    } while (0)

int main(void)
{
    ma_resource_manager_config config = ma_resource_manager_config_init();
    ma_resource_manager manager;
    float samples[16] = {0};
    unsigned int i;

    CHECK_RESULT(ma_resource_manager_init(&config, &manager), MA_SUCCESS);
    CHECK_RESULT(ma_resource_manager_unregister_file(NULL, "missing"), MA_INVALID_ARGS);
    CHECK_RESULT(ma_resource_manager_unregister_file(&manager, NULL), MA_INVALID_ARGS);

    printf("Testing missing and repeated unregisters, followed by valid registrations.\n");
    fflush(stdout);

    for (i = 0; i < 100; i += 1) {
        CHECK_RESULT(ma_resource_manager_unregister_file(&manager, "missing"), MA_DOES_NOT_EXIST);
        CHECK_RESULT(ma_resource_manager_register_decoded_data(&manager, "sound", samples, 16, ma_format_f32, 1, 48000), MA_SUCCESS);
        CHECK_RESULT(ma_resource_manager_register_decoded_data(&manager, "sound", samples, 16, ma_format_f32, 1, 48000), MA_SUCCESS);
        CHECK_RESULT(ma_resource_manager_unregister_file(&manager, "missing"), MA_DOES_NOT_EXIST);
        CHECK_RESULT(ma_resource_manager_unregister_file(&manager, "sound"), MA_SUCCESS);
        CHECK_RESULT(ma_resource_manager_unregister_data(&manager, "sound"), MA_SUCCESS);
        CHECK_RESULT(ma_resource_manager_unregister_file(&manager, "sound"), MA_DOES_NOT_EXIST);

        CHECK_RESULT(ma_resource_manager_unregister_file_w(&manager, L"missing-wide"), MA_DOES_NOT_EXIST);
        CHECK_RESULT(ma_resource_manager_register_decoded_data_w(&manager, L"sound-wide", samples, 16, ma_format_f32, 1, 48000), MA_SUCCESS);
        CHECK_RESULT(ma_resource_manager_unregister_file_w(&manager, L"sound-wide"), MA_SUCCESS);
        CHECK_RESULT(ma_resource_manager_unregister_data_w(&manager, L"sound-wide"), MA_DOES_NOT_EXIST);

        CHECK_RESULT(ma_resource_manager_register_decoded_data(&manager, "next", samples, 16, ma_format_f32, 1, 48000), MA_SUCCESS);
        CHECK_RESULT(ma_resource_manager_unregister_data(&manager, "next"), MA_SUCCESS);
    }

    ma_resource_manager_uninit(&manager);
    printf("PASS: missing/repeated unregisters preserve subsequent resource operations (100 rounds).\n");
    return 0;
}
