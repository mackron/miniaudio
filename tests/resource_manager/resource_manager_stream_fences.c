/*
Checks that every fence a data stream acquires is released again.

A stream has no "done" stage of its own. If `ma_resource_manager_data_stream_init_ex()` acquires the done fence but
the load job only releases the init fence, the done fence never reaches zero and `ma_fence_wait()` on it blocks
forever. The fence counter is polled with a timeout instead of waiting on it, so a failure doesn't hang the test.
*/
#define RM_STREAM_FENCES_FILE       TEST_OUTPUT_DIR"/resource_manager_stream_fences.wav"
#define RM_STREAM_FENCES_TIMEOUT_MS 5000

static ma_result rm_stream_fences_create_file(void)
{
    ma_result result;
    ma_waveform_config waveformConfig;
    ma_waveform waveform;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    static float frames[48000 * 2]; /* 1 second of stereo audio, longer than the stream's two pages. */

    waveformConfig = ma_waveform_config_init(ma_format_f32, 2, 48000, ma_waveform_type_sine, 0.2, 440);
    result = ma_waveform_init(&waveformConfig, &waveform);
    if (result != MA_SUCCESS) {
        return result;
    }

    ma_waveform_read_pcm_frames(&waveform, frames, 48000, NULL);
    ma_waveform_uninit(&waveform);

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 48000);
    result = ma_encoder_init_file(RM_STREAM_FENCES_FILE, &encoderConfig, &encoder);
    if (result != MA_SUCCESS) {
        printf("Failed to open \"%s\" for writing. %s\n", RM_STREAM_FENCES_FILE, ma_result_description(result));
        return result;
    }

    ma_encoder_write_pcm_frames(&encoder, frames, 48000, NULL);
    ma_encoder_uninit(&encoder);

    return MA_SUCCESS;
}

static ma_bool32 rm_stream_fences_wait_released(ma_fence* pFence)
{
    ma_uint32 waited = 0;

    while (ma_atomic_load_32(&pFence->counter) != 0) {
        if (waited >= RM_STREAM_FENCES_TIMEOUT_MS) {
            return MA_FALSE;
        }

        ma_sleep(10);
        waited += 10;
    }

    return MA_TRUE;
}

static ma_result test_resource_manager_stream_fences__by_stage(const char* pName, ma_bool32 useInitFence, ma_bool32 useDoneFence)
{
    ma_result result;
    ma_resource_manager_config resourceManagerConfig;
    ma_resource_manager resourceManager;
    ma_resource_manager_data_source_config dataSourceConfig;
    ma_resource_manager_pipeline_notifications notifications;
    ma_resource_manager_data_source dataSource;
    ma_fence initFence;
    ma_fence doneFence;
    ma_bool32 released = MA_TRUE;

    printf("    %s\n", pName);

    resourceManagerConfig = ma_resource_manager_config_init();
    resourceManagerConfig.decodedFormat = ma_format_f32;

    result = ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
    if (result != MA_SUCCESS) {
        printf("      Failed to initialize resource manager. %s\n", ma_result_description(result));
        return result;
    }

    ma_fence_init(&initFence);
    ma_fence_init(&doneFence);

    notifications = ma_resource_manager_pipeline_notifications_init();
    if (useInitFence) {
        notifications.init.pFence = &initFence;
    }
    if (useDoneFence) {
        notifications.done.pFence = &doneFence;
    }

    dataSourceConfig = ma_resource_manager_data_source_config_init();
    dataSourceConfig.pFilePath      = RM_STREAM_FENCES_FILE;
    dataSourceConfig.pNotifications = &notifications;
    dataSourceConfig.flags          = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;

    result = ma_resource_manager_data_source_init_ex(&resourceManager, &dataSourceConfig, &dataSource);
    if (result != MA_SUCCESS) {
        printf("      Failed to initialize data source. %s\n", ma_result_description(result));
    } else {
        if (!rm_stream_fences_wait_released(&initFence)) {
            printf("      FAILED: init fence still held after %u ms (counter = %u).\n", (unsigned int)RM_STREAM_FENCES_TIMEOUT_MS, (unsigned int)ma_atomic_load_32(&initFence.counter));
            released = MA_FALSE;
        }
        if (!rm_stream_fences_wait_released(&doneFence)) {
            printf("      FAILED: done fence still held after %u ms (counter = %u).\n", (unsigned int)RM_STREAM_FENCES_TIMEOUT_MS, (unsigned int)ma_atomic_load_32(&doneFence.counter));
            released = MA_FALSE;
        }

        ma_resource_manager_data_source_uninit(&dataSource);

        if (!released) {
            result = MA_ERROR;
        }
    }

    /* The fences are not uninitialized on failure: one of them could still be referenced by a pending job. */
    if (released) {
        ma_fence_uninit(&initFence);
        ma_fence_uninit(&doneFence);
    }
    ma_resource_manager_uninit(&resourceManager);

    return result;
}

int test_entry__resource_manager_stream_fences(int argc, char** argv)
{
    ma_bool32 hasError = MA_FALSE;

    (void)argc;
    (void)argv;

    if (rm_stream_fences_create_file() != MA_SUCCESS) {
        return -1;
    }

    if (test_resource_manager_stream_fences__by_stage("Stream, init fence", MA_TRUE, MA_FALSE) != MA_SUCCESS) {
        hasError = MA_TRUE;
    }
    if (test_resource_manager_stream_fences__by_stage("Stream, done fence", MA_FALSE, MA_TRUE) != MA_SUCCESS) {
        hasError = MA_TRUE;
    }
    if (test_resource_manager_stream_fences__by_stage("Stream, init and done fences", MA_TRUE, MA_TRUE) != MA_SUCCESS) {
        hasError = MA_TRUE;
    }

    if (hasError) {
        return -1;
    }

    return 0;
}
