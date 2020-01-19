
#define MINIAUDIO_SPEEX_RESAMPLER_IMPLEMENTATION
#include "../../extras/speex_resampler/ma_speex_resampler.h"

#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"
#include "../ma_resampler.h"

int init_resampler(ma_uint32 rateIn, ma_uint32 rateOut, ma_resample_algorithm algorithm, ma_resampler* pResampler)
{
    ma_result result;
    ma_resampler_config config;

    config = ma_resampler_config_init(ma_format_s16, 1, rateIn, rateOut, algorithm);
    result = ma_resampler_init(&config, pResampler);
    if (result != MA_SUCCESS) {
        return (int)result;
    }

    return 0;
}

int do_count_query_test__required_input__fixed_interval(ma_resampler* pResampler, ma_uint64 frameCountPerIteration)
{
    int result = 0;
    ma_int16 input[4096];
    ma_int16 i;

    MA_ASSERT(frameCountPerIteration < ma_countof(input));

    /* Fill the input buffer with sequential numbers so we can get an idea on the state of things. Useful for inspecting the linear backend in particular. */
    for (i = 0; i < ma_countof(input); i += 1) {
        input[i] = i;
    }

    for (i = 0; i < ma_countof(input); i += (ma_int16)frameCountPerIteration) {
        ma_int16 output[4096];
        ma_uint64 outputFrameCount;
        ma_uint64 inputFrameCount;
        ma_uint64 requiredInputFrameCount;

        /* We retrieve the required number of input frames for the specified number of output frames, and then compare with what we actually get when reading. */
        requiredInputFrameCount = ma_resampler_get_required_input_frame_count(pResampler, frameCountPerIteration);

        outputFrameCount = frameCountPerIteration;
        inputFrameCount = ma_countof(input);
        result = ma_resampler_process(pResampler, input, &inputFrameCount, output, &outputFrameCount);
        if (result != MA_SUCCESS) {
            printf("Failed to process frames.");
            return result;
        }

        if (inputFrameCount != requiredInputFrameCount) {
            printf("ERROR: Predicted vs actual input count mismatch: predicted=%d, actual=%d\n", (int)requiredInputFrameCount, (int)inputFrameCount);
            result = -1;
        }
    }

    if (result != 0) {
        printf("FAILED\n");
    } else {
        printf("PASSED\n");
    }

    return result;
}

int do_count_query_test__required_input__by_algorithm_and_rate__fixed_interval(ma_resample_algorithm algorithm, ma_uint32 rateIn, ma_uint32 rateOut, ma_uint64 frameCountPerIteration)
{
    int result;
    ma_resampler resampler;

    result = init_resampler(rateIn, rateOut, algorithm, &resampler);
    if (result != 0) {
        return 0;
    }

    result = do_count_query_test__required_input__fixed_interval(&resampler, frameCountPerIteration);

    ma_resampler_uninit(&resampler);
    return result;
}

int do_count_query_test__required_input__by_algorithm__fixed_interval(ma_resample_algorithm algorithm, ma_uint64 frameCountPerIteration)
{
    int result;

    result = do_count_query_test__required_input__by_algorithm_and_rate__fixed_interval(algorithm, 44100, 48000, frameCountPerIteration);
    if (result != 0) {
        return result;
    }

    result = do_count_query_test__required_input__by_algorithm_and_rate__fixed_interval(algorithm, 48000, 44100, frameCountPerIteration);
    if (result != 0) {
        return result;
    }

    
    result = do_count_query_test__required_input__by_algorithm_and_rate__fixed_interval(algorithm, 44100, 192000, frameCountPerIteration);
    if (result != 0) {
        return result;
    }

    result = do_count_query_test__required_input__by_algorithm_and_rate__fixed_interval(algorithm, 192000, 44100, frameCountPerIteration);
    if (result != 0) {
        return result;
    }

    return result;
}

int do_count_query_tests__required_input__by_algorithm(ma_resample_algorithm algorithm)
{
    int result;

    result = do_count_query_test__required_input__by_algorithm__fixed_interval(algorithm, 1);
    if (result != 0) {
        return result;
    }

    result = do_count_query_test__required_input__by_algorithm__fixed_interval(algorithm, 16);
    if (result != 0) {
        return result;
    }

    result = do_count_query_test__required_input__by_algorithm__fixed_interval(algorithm, 127);
    if (result != 0) {
        return result;
    }
    
    return 0;
}

int do_count_query_tests__required_input()
{
    int result;

    printf("Linear\n");
    result = do_count_query_tests__required_input__by_algorithm(ma_resample_algorithm_linear);
    if (result != 0) {
        return result;
    }

    printf("Speex\n");
    result = do_count_query_tests__required_input__by_algorithm(ma_resample_algorithm_speex);
    if (result != 0) {
        return result;
    }

    return 0;
}



int do_count_query_test__expected_output__fixed_interval(ma_resampler* pResampler, ma_uint64 frameCountPerIteration)
{
    int result = 0;
    ma_int16 input[4096];
    ma_int16 i;

    MA_ASSERT(frameCountPerIteration < ma_countof(input));

    /* Fill the input buffer with sequential numbers so we can get an idea on the state of things. Useful for inspecting the linear backend in particular. */
    for (i = 0; i < ma_countof(input); i += 1) {
        input[i] = i;
    }

    for (i = 0; i < ma_countof(input); i += (ma_int16)frameCountPerIteration) {
        ma_int16 output[4096];
        ma_uint64 outputFrameCount;
        ma_uint64 inputFrameCount;
        ma_uint64 expectedOutputFrameCount;

        /* We retrieve the required number of input frames for the specified number of output frames, and then compare with what we actually get when reading. */
        expectedOutputFrameCount = ma_resampler_get_expected_output_frame_count(pResampler, frameCountPerIteration);

        outputFrameCount = ma_countof(output);
        inputFrameCount = frameCountPerIteration;
        result = ma_resampler_process(pResampler, input, &inputFrameCount, output, &outputFrameCount);
        if (result != MA_SUCCESS) {
            printf("Failed to process frames.");
            return result;
        }

        if (outputFrameCount != expectedOutputFrameCount) {
            printf("ERROR: Predicted vs actual output count mismatch: predicted=%d, actual=%d\n", (int)expectedOutputFrameCount, (int)outputFrameCount);
            result = -1;
        }
    }

    if (result != 0) {
        printf("FAILED\n");
    } else {
        printf("PASSED\n");
    }

    return result;
}

int do_count_query_test__expected_output__by_algorithm_and_rate__fixed_interval(ma_resample_algorithm algorithm, ma_uint32 rateIn, ma_uint32 rateOut, ma_uint64 frameCountPerIteration)
{
    int result;
    ma_resampler resampler;

    result = init_resampler(rateIn, rateOut, algorithm, &resampler);
    if (result != 0) {
        return 0;
    }

    result = do_count_query_test__expected_output__fixed_interval(&resampler, frameCountPerIteration);

    ma_resampler_uninit(&resampler);
    return result;
}

int do_count_query_test__expected_output__by_algorithm__fixed_interval(ma_resample_algorithm algorithm, ma_uint64 frameCountPerIteration)
{
    int result;

    result = do_count_query_test__expected_output__by_algorithm_and_rate__fixed_interval(algorithm, 44100, 48000, frameCountPerIteration);
    if (result != 0) {
        return result;
    }

    result = do_count_query_test__expected_output__by_algorithm_and_rate__fixed_interval(algorithm, 48000, 44100, frameCountPerIteration);
    if (result != 0) {
        return result;
    }

    
    result = do_count_query_test__expected_output__by_algorithm_and_rate__fixed_interval(algorithm, 44100, 192000, frameCountPerIteration);
    if (result != 0) {
        return result;
    }

    result = do_count_query_test__expected_output__by_algorithm_and_rate__fixed_interval(algorithm, 192000, 44100, frameCountPerIteration);
    if (result != 0) {
        return result;
    }

    return result;
}

int do_count_query_tests__expected_output__by_algorithm(ma_resample_algorithm algorithm)
{
    int result;

    result = do_count_query_test__expected_output__by_algorithm__fixed_interval(algorithm, 1);
    if (result != 0) {
        return result;
    }

    result = do_count_query_test__expected_output__by_algorithm__fixed_interval(algorithm, 16);
    if (result != 0) {
        return result;
    }

    result = do_count_query_test__expected_output__by_algorithm__fixed_interval(algorithm, 127);
    if (result != 0) {
        return result;
    }
    
    return 0;
}

int do_count_query_tests__expected_output()
{
    int result;

    printf("Linear\n");
    result = do_count_query_tests__expected_output__by_algorithm(ma_resample_algorithm_linear);
    if (result != 0) {
        return result;
    }

    printf("Speex\n");
    result = do_count_query_tests__expected_output__by_algorithm(ma_resample_algorithm_speex);
    if (result != 0) {
        return result;
    }

    return 0;
}


int do_count_query_tests()
{
    int result;

    result = do_count_query_tests__expected_output();
    if (result != 0) {
        return result;
    }

    result = do_count_query_tests__required_input();
    if (result != 0) {
        return result;
    }

    return 0;
}

int main(int argc, char** argv)
{
    int result;

    (void)argc;
    (void)argv;

    result = do_count_query_tests();
    if (result != 0) {
        return result;
    }

    return 0;
}