#include "../test_common/ma_test_common.c"

ma_result filtering_init_decoder_and_encoder(const char* pInputFilePath, const char* pOutputFilePath, ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_decoder* pDecoder, drwav* pEncoder)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    drwav_data_format wavFormat;

    decoderConfig = ma_decoder_config_init(format, 0, 0);
    result = ma_decoder_init_file(pInputFilePath, &decoderConfig, pDecoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    wavFormat = drwav_data_format_from_minaudio_format(pDecoder->outputFormat, pDecoder->outputChannels, pDecoder->outputSampleRate);
    if (!drwav_init_file_write(pEncoder, pOutputFilePath, &wavFormat, NULL)) {
        ma_decoder_uninit(pDecoder);
        return MA_ERROR;
    }

    return MA_SUCCESS;
}

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
