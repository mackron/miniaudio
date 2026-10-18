/*
Reproduces resource manager jobs touching a data stream or data buffer after the thread waiting for
them has returned, which lets the caller free the memory while the job still uses it.

A job signals the notification the caller is waiting on, then increments the object's execution
pointer. If the caller frees the object in between, the increment lands in freed memory. When the next
object of the same type is allocated at the same address, which is likely as it is the same size, the
stale increment moves its execution pointer past its load job's order. That job is then reposted as
out of order forever, and the init call waiting for it never returns.

Three ways to get there, each tested below:

  1. A data stream that fails to initialize (the file can't be decoded), freed without
     uninitializing, as ma_sound does.
  2. The same with a data buffer loaded with DECODE | ASYNC | WAIT_INIT, as ma_sound does with
     MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, while another data buffer is still loading the
     same file.
  3. A data buffer uninitialized while its asynchronous load is still running, then freed.

The race needs the job thread to be preempted between the signal and the increment. Running on a
single CPU makes that close to certain, so the test pins itself to one CPU before the job thread is
created. A watchdog reports a hang rather than leaving the program stuck.

Build and run from tests/_build:

    gcc ../resource_manager/job_tail_race.c -o bin/job_tail_race -ldl -lm -lpthread -Wall -Wextra -std=c89
    ./bin/job_tail_race

To run only one of the scenarios, pass 1, 2 or 3.
*/
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE     /* For sched_setaffinity(). */
#endif

#include "../../miniaudio.c"

#include <stdio.h>
#include <stdlib.h>

#if defined(__linux__)
#include <sched.h>
#endif

#define ITERATIONS          1000
#define WATCHDOG_SECONDS    5
#define VALID_FILE          "job_tail_race_valid.wav"
#define INVALID_FILE        "job_tail_race_invalid.bin"

static ma_uint32 g_progress = 0;
static const char* g_pScenario = "";

static ma_thread_result MA_THREADCALL watchdog(void* pUserData)
{
    ma_uint32 lastProgress = ma_atomic_load_32(&g_progress);
    ma_uint32 stalledMilliseconds = 0;

    (void)pUserData;

    for (;;) {
        ma_uint32 progress;

        ma_sleep(100);

        progress = ma_atomic_load_32(&g_progress);
        if (progress != lastProgress) {
            lastProgress = progress;
            stalledMilliseconds = 0;
            continue;
        }

        stalledMilliseconds += 100;
        if (stalledMilliseconds >= WATCHDOG_SECONDS * 1000) {
            printf("FAILED: %s made no progress for %d seconds, at iteration %u. An init call is waiting for a job that never runs.\n", g_pScenario, WATCHDOG_SECONDS, (unsigned int)progress);
            fflush(stdout);
            exit(1);
        }
    }

    return (ma_thread_result)0;
}

static void pin_to_one_cpu(void)
{
#if defined(__linux__)
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(0, &cpus);
    if (sched_setaffinity(0, sizeof(cpus), &cpus) != 0) {
        printf("Warning: couldn't pin to one CPU; the race may not show.\n");
    }
#elif defined(MA_WIN32)
    if (!SetProcessAffinityMask(GetCurrentProcess(), 1)) {
        printf("Warning: couldn't pin to one CPU; the race may not show.\n");
    }
#else
    printf("Warning: run this pinned to one CPU, or the race may not show.\n");
#endif
}

static void write_le16(FILE* pFile, ma_uint16 value)
{
    fputc(value & 0xFF, pFile);
    fputc((value >> 8) & 0xFF, pFile);
}

static void write_le32(FILE* pFile, ma_uint32 value)
{
    write_le16(pFile, (ma_uint16)(value & 0xFFFF));
    write_le16(pFile, (ma_uint16)(value >> 16));
}

/* Half a second of silence, as 16-bit mono PCM at 22050 Hz. */
static int write_test_files(void)
{
    const ma_uint32 frameCount = 11025;
    ma_uint32 iFrame;
    FILE* pFile;

    pFile = fopen(VALID_FILE, "wb");
    if (pFile == NULL) {
        return 0;
    }

    fwrite("RIFF", 1, 4, pFile);
    write_le32(pFile, 36 + frameCount * 2);
    fwrite("WAVEfmt ", 1, 8, pFile);
    write_le32(pFile, 16);
    write_le16(pFile, 1);           /* PCM */
    write_le16(pFile, 1);           /* Channels */
    write_le32(pFile, 22050);       /* Sample rate */
    write_le32(pFile, 22050 * 2);   /* Bytes per second */
    write_le16(pFile, 2);           /* Block align */
    write_le16(pFile, 16);          /* Bits per sample */
    fwrite("data", 1, 4, pFile);
    write_le32(pFile, frameCount * 2);
    for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
        write_le16(pFile, 0);
    }
    fclose(pFile);

    pFile = fopen(INVALID_FILE, "wb");
    if (pFile == NULL) {
        return 0;
    }

    fputs("This is not audio, so every decoder rejects it.\n", pFile);
    fclose(pFile);

    return 1;
}

/* 1. A data stream that fails to initialize, freed without uninitializing it, as ma_sound does. */
static int test_failed_stream_init(ma_resource_manager* pResourceManager)
{
    int i;

    g_pScenario = "failed data stream init";

    for (i = 0; i < ITERATIONS; i += 1) {
        ma_resource_manager_data_stream* pStream;

        pStream = (ma_resource_manager_data_stream*)malloc(sizeof(*pStream));
        if (ma_resource_manager_data_stream_init(pResourceManager, INVALID_FILE, 0, NULL, pStream) == MA_SUCCESS) {
            printf("FAILED: %s: the invalid file was accepted.\n", g_pScenario);
            return 0;
        }
        free(pStream);

        pStream = (ma_resource_manager_data_stream*)malloc(sizeof(*pStream));
        if (ma_resource_manager_data_stream_init(pResourceManager, VALID_FILE, 0, NULL, pStream) != MA_SUCCESS) {
            printf("FAILED: %s: the valid file was rejected at iteration %d.\n", g_pScenario, i);
            return 0;
        }
        ma_resource_manager_data_stream_uninit(pStream);
        free(pStream);

        ma_atomic_fetch_add_32(&g_progress, 1);
    }

    return 1;
}

/*
2. The same with a data buffer, loaded the way ma_sound loads one with MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC.
The load only fails on the job thread when the file's node is shared with another data buffer that is
still loading it. Otherwise init waits for the node itself and fails without involving the job thread.
*/
static int test_failed_buffer_init(ma_resource_manager* pResourceManager)
{
    const ma_uint32 asyncFlags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;
    const ma_uint32 waitFlags  = asyncFlags | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT;
    int i;

    g_pScenario = "failed data buffer init";

    for (i = 0; i < ITERATIONS; i += 1) {
        ma_resource_manager_data_buffer* pLoading;
        ma_resource_manager_data_buffer* pBuffer;

        /* Keeps the invalid file's node loading while the next data buffer acquires it. */
        pLoading = (ma_resource_manager_data_buffer*)malloc(sizeof(*pLoading));
        if (ma_resource_manager_data_buffer_init(pResourceManager, INVALID_FILE, asyncFlags, NULL, pLoading) != MA_SUCCESS) {
            free(pLoading);
            pLoading = NULL;
        }

        pBuffer = (ma_resource_manager_data_buffer*)malloc(sizeof(*pBuffer));
        if (ma_resource_manager_data_buffer_init(pResourceManager, INVALID_FILE, waitFlags, NULL, pBuffer) == MA_SUCCESS) {
            printf("FAILED: %s: the invalid file was accepted.\n", g_pScenario);
            return 0;
        }
        free(pBuffer);

        /* Allocated at the address just freed, so that a stale increment shows as a hang. */
        pBuffer = (ma_resource_manager_data_buffer*)malloc(sizeof(*pBuffer));
        if (ma_resource_manager_data_buffer_init(pResourceManager, VALID_FILE, waitFlags, NULL, pBuffer) != MA_SUCCESS) {
            printf("FAILED: %s: the valid file was rejected at iteration %d.\n", g_pScenario, i);
            return 0;
        }
        ma_resource_manager_data_buffer_uninit(pBuffer);
        free(pBuffer);

        if (pLoading != NULL) {
            ma_resource_manager_data_buffer_uninit(pLoading);
            free(pLoading);
        }

        ma_atomic_fetch_add_32(&g_progress, 1);
    }

    return 1;
}

/*
3. A data buffer uninitialized while its asynchronous load is still running. The data buffer loaded
after it waits for initialization, as ma_sound always does, so that a stale increment shows as a hang.
*/
static int test_uninit_while_loading(ma_resource_manager* pResourceManager)
{
    const ma_uint32 asyncFlags = MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;
    const ma_uint32 waitFlags  = asyncFlags | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT;
    int i;

    g_pScenario = "data buffer uninit while loading";

    for (i = 0; i < ITERATIONS; i += 1) {
        ma_resource_manager_data_buffer* pBuffer;

        /* Uninitialized straight away, before the job thread has had a chance to load it. */
        pBuffer = (ma_resource_manager_data_buffer*)malloc(sizeof(*pBuffer));
        if (ma_resource_manager_data_buffer_init(pResourceManager, VALID_FILE, asyncFlags, NULL, pBuffer) != MA_SUCCESS) {
            printf("FAILED: %s: the valid file was rejected at iteration %d.\n", g_pScenario, i);
            return 0;
        }
        ma_resource_manager_data_buffer_uninit(pBuffer);
        free(pBuffer);

        pBuffer = (ma_resource_manager_data_buffer*)malloc(sizeof(*pBuffer));
        if (ma_resource_manager_data_buffer_init(pResourceManager, VALID_FILE, waitFlags, NULL, pBuffer) != MA_SUCCESS) {
            printf("FAILED: %s: the valid file was rejected at iteration %d.\n", g_pScenario, i);
            return 0;
        }
        ma_resource_manager_data_buffer_uninit(pBuffer);
        free(pBuffer);

        ma_atomic_fetch_add_32(&g_progress, 1);
    }

    return 1;
}

int main(int argc, char** argv)
{
    ma_resource_manager_config resourceManagerConfig;
    ma_resource_manager resourceManager;
    ma_thread watchdogThread;
    int scenario = 0;   /* 0 for all of them. */
    int success = 1;

    if (argc > 1) {
        scenario = atoi(argv[1]);
        if (scenario < 1 || scenario > 3) {
            printf("Usage: %s [1|2|3]\n", argv[0]);
            return 1;
        }
    }

    if (!write_test_files()) {
        printf("Failed to write the test files.\n");
        return 1;
    }

    /* Before the resource manager creates its job thread, so that the job thread is pinned too. */
    pin_to_one_cpu();

    resourceManagerConfig = ma_resource_manager_config_init();
    if (ma_resource_manager_init(&resourceManagerConfig, &resourceManager) != MA_SUCCESS) {
        printf("Failed to initialize the resource manager.\n");
        return 1;
    }

    if (ma_thread_create(&watchdogThread, ma_thread_priority_default, 0, watchdog, NULL, NULL) != MA_SUCCESS) {
        printf("Failed to start the watchdog.\n");
        return 1;
    }

    if (success && (scenario == 0 || scenario == 1)) {
        success = test_failed_stream_init(&resourceManager);
    }
    if (success && (scenario == 0 || scenario == 2)) {
        success = test_failed_buffer_init(&resourceManager);
    }
    if (success && (scenario == 0 || scenario == 3)) {
        success = test_uninit_while_loading(&resourceManager);
    }

    ma_resource_manager_uninit(&resourceManager);

    remove(VALID_FILE);
    remove(INVALID_FILE);

    if (!success) {
        return 1;
    }

    printf("Passed: %d iterations of %s.\n", ITERATIONS, scenario == 0 ? "each scenario" : g_pScenario);
    return 0;
}
