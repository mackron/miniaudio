#define MA_NO_DEVICE_IO
#include "../common/common.c"

#include "generation_noise.c"
#include "generation_waveform.c"

int main(int argc, char** argv)
{
    ma_register_test("Noise",    test_entry__noise);
    ma_register_test("Waveform", test_entry__waveform);

    return ma_run_tests(argc, argv);
}
