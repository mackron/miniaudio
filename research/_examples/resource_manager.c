/*
Demonstrates how you can use the resource manager to manage loaded sounds.

The resource manager can be used to create a data source whose resources are managed internally by miniaudio. The data
sources can then be read just like any other data source such as decoders and audio buffers.

In this example we use the resource manager independently of the `ma_engine` API so that we can demonstrate how it can
be used by itself without getting it confused with `ma_engine`. Audio data is mixed using the `ma_mixer` API, but you
can also use data sources with `ma_data_source_read_pcm_frames()` in the same way we do in the simple_looping example.

The main feature of the resource manager is the ability to decode and stream audio data asynchronously. Asynchronicity
is achieved with a job system. The resource manager will issue jobs which are processed by a configurable number of job
threads. You can also implement your own custom job threads which this example also demonstrates.

In this example we show how you can create a data source, mix them with other data sources, configure the number of job
threads to manage internally and how to implement your own custom job thread.
*/
#define MA_NO_ENGINE        /* We're intentionally not using the ma_engine API here. */
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"
#include "../ma_engine.h"

static ma_mixer                        g_mixer;
static ma_resource_manager_data_source g_dataSources[16];
static ma_uint32                       g_dataSourceCount;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    /* In this example we're just going to play our data sources layered on top of each other. */
    ma_uint32 bpf = ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels);
    ma_uint32 framesProcessed = 0;
    while (framesProcessed < frameCount) {
        ma_uint64 frameCountIn;
        ma_uint64 frameCountOut = (frameCount - framesProcessed);

        ma_mixer_begin(&g_mixer, NULL, &frameCountOut, &frameCountIn);
        {
            size_t iDataSource;
            for (iDataSource = 0; iDataSource < g_dataSourceCount; iDataSource += 1) {
                ma_mixer_mix_data_source(&g_mixer, &g_dataSources[iDataSource], frameCountIn, NULL, 1, NULL, MA_TRUE);
            }
        }
        ma_mixer_end(&g_mixer, NULL, ma_offset_ptr(pOutput, framesProcessed * bpf));

        framesProcessed += (ma_uint32)frameCountOut;    /* Safe cast. */
    }

    (void)pInput;
}

static ma_thread_result MA_THREADCALL custom_job_thread(void* pUserData)
{
    ma_resource_manager* pResourceManager = (ma_resource_manager*)pUserData;
    MA_ASSERT(pResourceManager != NULL);

    for (;;) {
        ma_result result;
        ma_job job;

        /*
        Retrieve a job from the queue first. This defines what it is you're about to do. By default this will be
        blocking. You can initialize the resource manager with MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING to not block in
        which case MA_NO_DATA_AVAILABLE will be returned if no jobs are available.

        When the quit job is returned (MA_JOB_QUIT), the return value will always be MA_CANCELLED. If you don't want
        to check the return value (you should), you can instead check if the job code is MA_JOB_QUIT and use that
        instead.
        */
        result = ma_resource_manager_next_job(pResourceManager, &job);
        if (result != MA_SUCCESS) {
            if (result == MA_CANCELLED) {
                printf("CUSTOM JOB THREAD TERMINATING VIA MA_CANCELLED... ");
            } else {
                printf("CUSTOM JOB THREAD ERROR: %s. TERMINATING... ", ma_result_description(result));
            }

            break;
        }

        /*
        Terminate if we got a quit message. You don't need to terminate like this, but's a bit more robust. You can
        just use a global variable or something similar if it's easier for you particular situation. The quit job
        remains in the queue and will continue to be returned by future calls to ma_resource_manager_next_job(). The
        reason for this is to give every job thread visibility to the quit job so they have a chance to exit.

        We won't actually be hitting this code because the call above will return MA_CANCELLED when the MA_JOB_QUIT
        event is received which means the `result != MA_SUCCESS` logic above will catch it. If you do not check the
        return value of ma_resource_manager_next_job() you will want to check for MA_JOB_QUIT like the code below.
        */
        if (job.toc.code == MA_JOB_QUIT) {
            printf("CUSTOM JOB THREAD TERMINATING VIA MA_JOB_QUIT... ");
            break;
        }

        /* Call ma_resource_manager_process_job() to actually do the work to process the job. */
        printf("PROCESSING IN CUSTOM JOB THREAD: %d\n", job.toc.code);
        ma_resource_manager_process_job(pResourceManager, &job);
    }

    printf("TERMINATED\n");
    return (ma_thread_result)0;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
    ma_resource_manager_config resourceManagerConfig;
    ma_resource_manager resourceManager;
    ma_mixer_config mixerConfig;
    ma_thread jobThread;
    int iFile;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.dataCallback    = data_callback;
    deviceConfig.pUserData       = NULL;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize device.");
        return -1;
    }

    /*
    Before starting the device we'll need to initialize the mixer. If we don't do this first, the data callback will be
    fired and will try to use the mixer without it being initialized.
    */
    mixerConfig = ma_mixer_config_init(device.playback.format, device.playback.channels, 1024, NULL, NULL);

    result = ma_mixer_init(&mixerConfig, &g_mixer);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to initialize mixer.");
        return -1;
    }

    /* We can start the device before loading any sounds. We'll just end up outputting silence. */
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to start device.");
        return -1;
    }


    /*
    We have the device so now we want to initialize the resource manager. We'll use the resource manager to load some
    sounds based on the command line.
    */
    resourceManagerConfig = ma_resource_manager_config_init();

    /*
    We'll set a standard decoding format to save us to processing time at mixing time. If you're wanting to use
    spatialization with your decoded sounds, you may want to consider leaving this as 0 to ensure the file's native
    channel count is used so you can do proper spatialization.
    */
    resourceManagerConfig.decodedFormat     = device.playback.format;
    resourceManagerConfig.decodedChannels   = device.playback.channels;
    resourceManagerConfig.decodedSampleRate = device.sampleRate;

    /* The number of job threads to be managed internally. Set this to 0 if you want to self-manage your job threads */
    resourceManagerConfig.jobThreadCount = 4;

    result = ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to initialize the resource manager.");
        return -1;
    }

    /*
    Now that we have a resource manager we can set up our custom job thread. This is optional. Normally when doing
    self-managed job threads you would set the internal job thread count to zero. We're doing both internal and
    self-managed job threads in this example just for demonstration purposes.
    */
    ma_thread_create(&jobThread, ma_thread_priority_default, 0, custom_job_thread, &resourceManager);

    /* Create each data source from the resource manager. Note that the caller is the owner. */
    for (iFile = 0; iFile < ma_countof(g_dataSources) && iFile < argc-1; iFile += 1) {
        result = ma_resource_manager_data_source_init(
            &resourceManager,
            argv[iFile+1],
            MA_DATA_SOURCE_FLAG_DECODE | MA_DATA_SOURCE_FLAG_ASYNC /*| MA_DATA_SOURCE_FLAG_STREAM*/,
            NULL,   /* Async notification. */
            &g_dataSources[iFile]);

        if (result != MA_SUCCESS) {
            break;
        }

        g_dataSourceCount += 1;
    }

    printf("Press Enter to quit...");
    getchar();


    /* Teardown. */

    /* Uninitialize the device first to ensure the data callback is stopped and doesn't try to access any data. */
    ma_device_uninit(&device);

    /*
    Before uninitializing the resource manager we need to make sure a quit event has been posted to ensure we can get
    out of our custom thread. The call to ma_resource_manager_uninit() will also do this, but we need to call it
    explicitly so that our thread can exit naturally. You only need to post a quit job if you're using that as the exit
    indicator. You can instead use whatever variable you want to terminate your job thread, but since this example is
    using a quit job we need to post one.
    */
    ma_resource_manager_post_job_quit(&resourceManager);
    ma_thread_wait(&jobThread); /* Wait for the custom job thread to finish so it doesn't try to access any data. */
    
    /* Our data sources need to be explicitly uninitialized. ma_resource_manager_uninit() will not do it for us. */
    for (iFile = 0; (size_t)iFile < g_dataSourceCount; iFile += 1) {
        ma_resource_manager_data_source_uninit(&g_dataSources[iFile]);
    }

    /* Uninitialize the resource manager after each data source. */
    ma_resource_manager_uninit(&resourceManager);

    /*
    We're uninitializing the mixer last, but it doesn't matter when it's done, so long as it's after the device has
    been stopped/uninitialized.
    */
    ma_mixer_uninit(&g_mixer);
    
    return 0;
}
