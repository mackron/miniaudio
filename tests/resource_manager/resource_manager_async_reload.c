/*
Loads, waits for and unloads the same file asynchronously in a tight loop.

If a job signals a notification or fence before it has finished with the data buffer, the owner can
uninitialize the data source and initialize a new one at the same address before the job thread gets to
the `executionPointer` increment. The new data buffer then starts with the wrong execution pointer, its
load job is re-posted as out of order forever, and the fence is never released. A watchdog thread fails
the test instead of letting it hang.
*/
#define RM_ASYNC_RELOAD_FILE        TEST_OUTPUT_DIR"/resource_manager_async_reload.wav"
#define RM_ASYNC_RELOAD_ITERATIONS  5000
#define RM_ASYNC_RELOAD_TIMEOUT_MS  5000

static ma_uint32 g_rmAsyncReloadProgress;
static ma_uint32 g_rmAsyncReloadFinished;

static ma_thread_result MA_THREADCALL rm_async_reload_watchdog(void* pUserData)
{
    ma_uint32 lastProgress = (ma_uint32)-1;
    ma_uint32 idleTime = 0;

    (void)pUserData;

    while (ma_atomic_load_32(&g_rmAsyncReloadFinished) == 0) {
        ma_uint32 progress = ma_atomic_load_32(&g_rmAsyncReloadProgress);
        if (progress != lastProgress) {
            lastProgress = progress;
            idleTime = 0;
        } else {
            idleTime += 10;
            if (idleTime >= RM_ASYNC_RELOAD_TIMEOUT_MS) {
                printf("      FAILED: iteration %u did not complete within %u ms.\n", (unsigned int)progress, (unsigned int)RM_ASYNC_RELOAD_TIMEOUT_MS);
                printf("=== END Async Reload : FAILED ===\n");
                fflush(stdout);
                exit(1);    /* The main thread is stuck in ma_fence_wait(). */
            }
        }

        ma_sleep(10);
    }

    return (ma_thread_result)0;
}

static ma_result rm_async_reload_create_file(void)
{
    ma_result result;
    ma_waveform_config waveformConfig;
    ma_waveform waveform;
    ma_encoder_config encoderConfig;
    ma_encoder encoder;
    static float frames[4800 * 2];  /* 100ms of stereo audio. Short files make the window easier to hit. */

    waveformConfig = ma_waveform_config_init(ma_format_f32, 2, 48000, ma_waveform_type_sine, 0.2, 440);
    result = ma_waveform_init(&waveformConfig, &waveform);
    if (result != MA_SUCCESS) {
        return result;
    }

    ma_waveform_read_pcm_frames(&waveform, frames, 4800, NULL);
    ma_waveform_uninit(&waveform);

    encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 48000);
    result = ma_encoder_init_file(RM_ASYNC_RELOAD_FILE, &encoderConfig, &encoder);
    if (result != MA_SUCCESS) {
        printf("Failed to open \"%s\" for writing. %s\n", RM_ASYNC_RELOAD_FILE, ma_result_description(result));
        return result;
    }

    ma_encoder_write_pcm_frames(&encoder, frames, 4800, NULL);
    ma_encoder_uninit(&encoder);

    return MA_SUCCESS;
}

static ma_result test_resource_manager_async_reload__by_flags(const char* pName, ma_uint32 jobThreadCount, ma_uint32 flags, ma_bool32 waitForInit)
{
    ma_result result;
    ma_resource_manager_config resourceManagerConfig;
    ma_resource_manager resourceManager;
    ma_resource_manager_data_source_config dataSourceConfig;
    ma_resource_manager_pipeline_notifications notifications;
    ma_resource_manager_data_source dataSource; /* Same address every iteration. Memory reuse is what triggers the bug. */
    ma_fence fence;
    ma_thread watchdog;
    ma_uint32 iIteration;

    printf("    %s (%u job thread%s)\n", pName, (unsigned int)jobThreadCount, (jobThreadCount == 1) ? "" : "s");

    resourceManagerConfig = ma_resource_manager_config_init();
    resourceManagerConfig.jobThreadCount = jobThreadCount;
    resourceManagerConfig.decodedFormat  = ma_format_f32;

    result = ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
    if (result != MA_SUCCESS) {
        printf("      Failed to initialize resource manager. %s\n", ma_result_description(result));
        return result;
    }

    result = ma_fence_init(&fence);
    if (result != MA_SUCCESS) {
        ma_resource_manager_uninit(&resourceManager);
        return result;
    }

    notifications = ma_resource_manager_pipeline_notifications_init();
    if (waitForInit) {
        notifications.init.pFence = &fence;
    } else {
        notifications.done.pFence = &fence;
    }

    dataSourceConfig = ma_resource_manager_data_source_config_init();
    dataSourceConfig.pFilePath      = RM_ASYNC_RELOAD_FILE;
    dataSourceConfig.pNotifications = &notifications;
    dataSourceConfig.flags          = flags | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;

    ma_atomic_exchange_32(&g_rmAsyncReloadProgress, 0);
    ma_atomic_exchange_32(&g_rmAsyncReloadFinished, 0);

    result = ma_thread_create(&watchdog, ma_thread_priority_default, 0, rm_async_reload_watchdog, NULL, NULL);
    if (result != MA_SUCCESS) {
        ma_fence_uninit(&fence);
        ma_resource_manager_uninit(&resourceManager);
        return result;
    }

    for (iIteration = 0; iIteration < RM_ASYNC_RELOAD_ITERATIONS; iIteration += 1) {
        result = ma_resource_manager_data_source_init_ex(&resourceManager, &dataSourceConfig, &dataSource);
        if (result != MA_SUCCESS) {
            printf("      Failed to initialize data source on iteration %u. %s\n", (unsigned int)iIteration, ma_result_description(result));
            break;
        }

        ma_fence_wait(&fence);

        result = ma_resource_manager_data_source_uninit(&dataSource);
        if (result != MA_SUCCESS) {
            printf("      Failed to uninitialize data source on iteration %u. %s\n", (unsigned int)iIteration, ma_result_description(result));
            break;
        }

        ma_atomic_fetch_add_32(&g_rmAsyncReloadProgress, 1);
    }

    ma_atomic_exchange_32(&g_rmAsyncReloadFinished, 1);
    ma_thread_wait(&watchdog);

    ma_fence_uninit(&fence);
    ma_resource_manager_uninit(&resourceManager);

    return result;
}

int test_entry__resource_manager_async_reload(int argc, char** argv)
{
    ma_bool32 hasError = MA_FALSE;
    ma_uint32 jobThreadCounts[2];
    size_t iJobThreadCount;

    (void)argc;
    (void)argv;

    jobThreadCounts[0] = 1;
    jobThreadCounts[1] = 4;

    if (rm_async_reload_create_file() != MA_SUCCESS) {
        return -1;
    }

    for (iJobThreadCount = 0; iJobThreadCount < ma_countof(jobThreadCounts); iJobThreadCount += 1) {
        ma_uint32 jobThreadCount = jobThreadCounts[iJobThreadCount];

        if (test_resource_manager_async_reload__by_flags("Decoded buffer, done fence", jobThreadCount, MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE, MA_FALSE) != MA_SUCCESS) {
            hasError = MA_TRUE;
        }
        if (test_resource_manager_async_reload__by_flags("Encoded buffer, done fence", jobThreadCount, 0, MA_FALSE) != MA_SUCCESS) {
            hasError = MA_TRUE;
        }
        if (test_resource_manager_async_reload__by_flags("Decoded buffer, init fence", jobThreadCount, MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE, MA_TRUE) != MA_SUCCESS) {
            hasError = MA_TRUE;
        }
        if (test_resource_manager_async_reload__by_flags("Stream, init fence", jobThreadCount, MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM, MA_TRUE) != MA_SUCCESS) {
            hasError = MA_TRUE;
        }
    }

    if (hasError) {
        return -1;
    }

    return 0;
}
