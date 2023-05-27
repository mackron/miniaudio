
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

ma_result test_data_converter__resampling_expected_output_fixed_interval(ma_data_converter* pDataConverter, ma_uint64 frameCountPerIteration)
{
    ma_result result = MA_SUCCESS;
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
        ma_data_converter_get_expected_output_frame_count(pDataConverter, frameCountPerIteration, &expectedOutputFrameCount);

        outputFrameCount = ma_countof(output);
        inputFrameCount = frameCountPerIteration;
        result = ma_data_converter_process_pcm_frames(pDataConverter, input, &inputFrameCount, output, &outputFrameCount);
        if (result != MA_SUCCESS) {
            printf("Failed to process frames.");
            break;
        }

        if (outputFrameCount != expectedOutputFrameCount) {
            printf("ERROR: Predicted vs actual output count mismatch: predicted=%d, actual=%d\n", (int)expectedOutputFrameCount, (int)outputFrameCount);
            result = MA_ERROR;
        }
    }

    if (result != MA_SUCCESS) {
        printf("FAILED\n");
    } else {
        printf("PASSED\n");
    }

    return result;
}

ma_result test_data_converter__resampling_expected_output_by_algorithm_and_rate_fixed_interval(ma_resample_algorithm algorithm, ma_uint32 rateIn, ma_uint32 rateOut, ma_uint64 frameCountPerIteration)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    ma_data_converter converter;

    result = init_data_converter(rateIn, rateOut, algorithm, &converter);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_expected_output_fixed_interval(&converter, frameCountPerIteration);

    ma_data_converter_uninit(&converter, NULL);

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_data_converter__resampling_expected_output_by_algorithm_fixed_interval(ma_resample_algorithm algorithm, ma_uint64 frameCountPerIteration)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_data_converter__resampling_expected_output_by_algorithm_and_rate_fixed_interval(algorithm, 44100, 48000, frameCountPerIteration);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_expected_output_by_algorithm_and_rate_fixed_interval(algorithm, 48000, 44100, frameCountPerIteration);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    
    result = test_data_converter__resampling_expected_output_by_algorithm_and_rate_fixed_interval(algorithm, 44100, 192000, frameCountPerIteration);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_expected_output_by_algorithm_and_rate_fixed_interval(algorithm, 192000, 44100, frameCountPerIteration);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_data_converter__resampling_expected_output_by_algorithm(ma_resample_algorithm algorithm)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_data_converter__resampling_expected_output_by_algorithm_fixed_interval(algorithm, 1);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_expected_output_by_algorithm_fixed_interval(algorithm, 16);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_expected_output_by_algorithm_fixed_interval(algorithm, 127);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }
    
    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_data_converter__resampling_expected_output()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    printf("Linear\n");
    result = test_data_converter__resampling_expected_output_by_algorithm(ma_resample_algorithm_linear);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}



ma_result test_data_converter__resampling_required_input_fixed_interval(ma_data_converter* pDataConverter, ma_uint64 frameCountPerIteration)
{
    ma_result result = MA_SUCCESS;
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
        ma_data_converter_get_required_input_frame_count(pDataConverter, frameCountPerIteration, &requiredInputFrameCount);

        outputFrameCount = frameCountPerIteration;
        inputFrameCount = ma_countof(input);
        result = ma_data_converter_process_pcm_frames(pDataConverter, input, &inputFrameCount, output, &outputFrameCount);
        if (result != MA_SUCCESS) {
            printf("Failed to process frames.");
            break;
        }

        if (inputFrameCount != requiredInputFrameCount) {
            printf("ERROR: Predicted vs actual input count mismatch: predicted=%d, actual=%d\n", (int)requiredInputFrameCount, (int)inputFrameCount);
            result = MA_ERROR;
        }
    }

    if (result != MA_SUCCESS) {
        printf("FAILED\n");
    } else {
        printf("PASSED\n");
    }

    return result;
}

ma_result test_data_converter__resampling_required_input_by_algorithm_and_rate_fixed_interval(ma_resample_algorithm algorithm, ma_uint32 rateIn, ma_uint32 rateOut, ma_uint64 frameCountPerIteration)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;
    ma_data_converter converter;

    result = init_data_converter(rateIn, rateOut, algorithm, &converter);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_required_input_fixed_interval(&converter, frameCountPerIteration);

    ma_data_converter_uninit(&converter, NULL);

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_data_converter__resampling_required_input_by_algorithm_fixed_interval(ma_resample_algorithm algorithm, ma_uint64 frameCountPerIteration)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_data_converter__resampling_required_input_by_algorithm_and_rate_fixed_interval(algorithm, 44100, 48000, frameCountPerIteration);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_required_input_by_algorithm_and_rate_fixed_interval(algorithm, 48000, 44100, frameCountPerIteration);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    
    result = test_data_converter__resampling_required_input_by_algorithm_and_rate_fixed_interval(algorithm, 44100, 192000, frameCountPerIteration);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_required_input_by_algorithm_and_rate_fixed_interval(algorithm, 192000, 44100, frameCountPerIteration);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_data_converter__resampling_required_input_by_algorithm(ma_resample_algorithm algorithm)
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_data_converter__resampling_required_input_by_algorithm_fixed_interval(algorithm, 1);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_required_input_by_algorithm_fixed_interval(algorithm, 16);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_required_input_by_algorithm_fixed_interval(algorithm, 127);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }
    
    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}

ma_result test_data_converter__resampling_required_input()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    printf("Linear\n");
    result = test_data_converter__resampling_required_input_by_algorithm(ma_resample_algorithm_linear);
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}




ma_result test_data_converter__resampling()
{
    ma_result result;
    ma_bool32 hasError = MA_FALSE;

    result = test_data_converter__resampling_expected_output();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    result = test_data_converter__resampling_required_input();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return MA_ERROR;
    } else {
        return MA_SUCCESS;
    }
}


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

    result = test_data_converter__resampling();
    if (result != MA_SUCCESS) {
        hasError = MA_TRUE;
    }


    if (hasError) {
        return -1;
    } else {
        return 0;
    }
}
