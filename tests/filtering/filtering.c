#include "../test_common/ma_test_common.c"

ma_result filtering_init_decoder_and_encoder(const char* pInputFilePath, const char* pOutputFilePath, ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_decoder* pDecoder, ma_encoder* pEncoder)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_encoder_config encoderConfig;

    decoderConfig = ma_decoder_config_init(format, channels, sampleRate);
    result = ma_decoder_init_file(pInputFilePath, &decoderConfig, pDecoder);
    if (result != MA_SUCCESS) {
        printf("Failed to open \"%s\" for decoding. %s\n", pInputFilePath, ma_result_description(result));
        return result;
    }

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, pDecoder->outputFormat, pDecoder->outputChannels, pDecoder->outputSampleRate);
    result = ma_encoder_init_file(pOutputFilePath, &encoderConfig, pEncoder);
    if (result != MA_SUCCESS) {
        printf("Failed to open \"%s\" for encoding. %s\n", pOutputFilePath, ma_result_description(result));
        ma_decoder_uninit(pDecoder);
        return result;
    }

    return MA_SUCCESS;
}

#include "ma_test_filtering_dithering.c"
#include "ma_test_filtering_lpf.c"
#include "ma_test_filtering_hpf.c"
#include "ma_test_filtering_bpf.c"
#include "ma_test_filtering_notch.c"
#include "ma_test_filtering_peak.c"
#include "ma_test_filtering_loshelf.c"
#include "ma_test_filtering_hishelf.c"

int main(int argc, char** argv)
{
    ma_register_test("Dithering",            test_entry__dithering);
    ma_register_test("Low-Pass Filtering",   test_entry__lpf);
    ma_register_test("High-Pass Filtering",  test_entry__hpf);
    ma_register_test("Band-Pass Filtering",  test_entry__bpf);
    ma_register_test("Notching Filtering",   test_entry__notch);
    ma_register_test("Peaking EQ Filtering", test_entry__peak);
    ma_register_test("Low Shelf Filtering",  test_entry__loshelf);
    ma_register_test("High Shelf Filtering", test_entry__hishelf);

    return ma_run_tests(argc, argv);
}
