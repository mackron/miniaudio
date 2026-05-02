#include "../../miniaudio.c"
#include "../../external/fs/fs.c"

#include <stdio.h>

/* BEG ma_test.c */
typedef struct ma_test ma_test;

typedef int (* ma_test_proc)(ma_test* pUserData);

struct ma_test
{
    const char* name;
    ma_test_proc proc;
    void* pUserData;
    int result;
    ma_test* pFirstChild;
    ma_test* pNextSibling;
};

void ma_test_init(ma_test* pTest, const char* name, ma_test_proc proc, void* pUserData, ma_test* pParent)
{
    if (pTest == NULL) {
        return;
    }

    memset(pTest, 0, sizeof(ma_test));
    pTest->name = name;
    pTest->proc = proc;
    pTest->pUserData = pUserData;
    pTest->result = MA_SUCCESS;
    pTest->pFirstChild = NULL;
    pTest->pNextSibling = NULL;

    if (pParent != NULL) {
        if (pParent->pFirstChild == NULL) {
            pParent->pFirstChild = pTest;
        } else {
            ma_test* pSibling = pParent->pFirstChild;
            while (pSibling->pNextSibling != NULL) {
                pSibling = pSibling->pNextSibling;
            }

            pSibling->pNextSibling = pTest;
        }
    }
}

void ma_test_count(ma_test* pTest, int* pCount, int* pPassed)
{
    ma_test* pChild;

    if (pTest == NULL) {
        return;
    }

    *pCount += 1;

    if (pTest->result == MA_SUCCESS) {
        *pPassed += 1;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        ma_test_count(pChild, pCount, pPassed);
        pChild = pChild->pNextSibling;
    }
}

int ma_test_run(ma_test* pTest)
{
    /* Start our counts at -1 to exclude the root test. */
    int testCount = -1;
    int passedCount = -1;

    if (pTest == NULL) {
        return MA_ERROR;
    }

    if (pTest->name != NULL && pTest->proc != NULL) {
        printf("Running Test: %s\n", pTest->name);
    }

    if (pTest->proc != NULL) {
        pTest->result = pTest->proc(pTest);
        if (pTest->result != MA_SUCCESS) {
            return pTest->result;
        }
    }

    /* Now we need to recursively execute children. If any child test fails, the parent test needs to be marked as failed as well. */
    {
        ma_test* pChild = pTest->pFirstChild;
        while (pChild != NULL) {
            int result = ma_test_run(pChild);
            if (result != MA_SUCCESS) {
                pTest->result = result;
            }

            pChild = pChild->pNextSibling;
        }
    }

    /* Now count the number of failed tests and report success or failure depending on the result. */
    ma_test_count(pTest, &testCount, &passedCount);

    return (testCount == passedCount) ? MA_SUCCESS : MA_ERROR;
}

void ma_test_print_local_result(ma_test* pTest, int level)
{
    if (pTest == NULL) {
        return;
    }

    printf("[%s] %*s%s\n", pTest->result == MA_SUCCESS ? "PASS" : "FAIL", level * 2, "", pTest->name);
}

void ma_test_print_child_results(ma_test* pTest, int level)
{
    ma_test* pChild;

    if (pTest == NULL) {
        return;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        ma_test_print_local_result(pChild, level);
        ma_test_print_child_results(pChild, level + 1);

        pChild = pChild->pNextSibling;
    }
}

void ma_test_print_result(ma_test* pTest, int level)
{
    ma_test* pChild;

    if (pTest == NULL) {
        return;
    }

    if (pTest->name != NULL) {
        printf("[%s] %*s%s\n", pTest->result == MA_SUCCESS ? "PASS" : "FAIL", level * 2, "", pTest->name);
        level += 1;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        ma_test_print_result(pChild, level);
        pChild = pChild->pNextSibling;
    }
}

void ma_test_print_summary(ma_test* pTest)
{
    /* Start our counts at -1 to exclude the root test. */
    int testCount = -1;
    int passedCount = -1;

    if (pTest == NULL) {
        return;
    }

    /* This should only be called on a root test. */
    assert(pTest->name == NULL);

    printf("=== Test Summary ===\n");
    ma_test_print_result(pTest, 0);

    /* We need to count how many tests failed. */
    ma_test_count(pTest, &testCount, &passedCount);
    printf("---\n%s%d / %d tests passed.\n", (testCount == passedCount) ? "[PASS]: " : "[FAIL]: ", passedCount, testCount);
}
/* END ma_test.c */


/* BEG ma_test_data_source.c */
typedef struct ma_test_data_source
{
    ma_data_source_base base;
    ma_test* pTest;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint64 cursor;   /* Relative to the beginning of the current range. */
    ma_uint64 length;   /* The full length of the data source, not including the range. */
    ma_bool32 isInitialized;
    ma_bool32 isLooping;
    ma_uint64 rangeBegInFrames;
    ma_uint64 rangeEndInFrames;
    ma_uint64 loopBegInFrames;
    ma_uint64 loopEndInFrames;
} ma_test_data_source;

static size_t ma_test_data_source_sizeof(void)
{
    return sizeof(ma_test_data_source);
}

static void ma_test_data_source_uninit(ma_data_source* pDataSource)
{
    ma_test_data_source* pTestDataSource = (ma_test_data_source*)pDataSource;
    pTestDataSource->isInitialized = MA_FALSE;
}

static ma_result ma_test_data_source_copy(ma_data_source* pDataSource, ma_data_source* pNewDataSource)
{
    ma_test_data_source* pTestDataSource    = (ma_test_data_source*)pDataSource;
    ma_test_data_source* pNewTestDataSource = (ma_test_data_source*)pNewDataSource;

    pNewTestDataSource->format           = pTestDataSource->format;
    pNewTestDataSource->channels         = pTestDataSource->channels;
    pNewTestDataSource->sampleRate       = pTestDataSource->sampleRate;
    pNewTestDataSource->cursor           = pTestDataSource->cursor;
    pNewTestDataSource->length           = pTestDataSource->length;
    pNewTestDataSource->isInitialized    = pTestDataSource->isInitialized;
    pNewTestDataSource->isLooping        = pTestDataSource->isLooping;
    pNewTestDataSource->rangeBegInFrames = pTestDataSource->rangeBegInFrames;
    pNewTestDataSource->rangeEndInFrames = pTestDataSource->rangeEndInFrames;
    pNewTestDataSource->loopBegInFrames  = pTestDataSource->loopBegInFrames;
    pNewTestDataSource->loopEndInFrames  = pTestDataSource->loopEndInFrames;

    return MA_SUCCESS;
}

static ma_result ma_test_data_source_read(ma_data_source* pDataSource, void* pFrames, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_test_data_source* pTestDataSource = (ma_test_data_source*)pDataSource;
    ma_result result;
    ma_uint64 length;
    ma_uint64 framesRemaining;

    /* miniaudio should always be giving us a valid pointer for pFramesRead. */
    if (pFramesRead == NULL) {
        printf("%s: pFramesRead is null.\n", pTestDataSource->pTest->name);
        return MA_INVALID_ARGS;
    }

    result = ma_data_source_get_length_in_pcm_frames(pDataSource, &length);
    if (result != MA_SUCCESS) {
        printf("%s: Failed to retrieve the length of the data source when reading.\n", pTestDataSource->pTest->name);
        return result;
    }

    framesRemaining = length - pTestDataSource->cursor;
    
    if (frameCount > framesRemaining) {
        frameCount = framesRemaining;
    }

    MA_ZERO_MEMORY(pFrames, frameCount * ma_get_bytes_per_frame(pTestDataSource->format, pTestDataSource->channels));
    pTestDataSource->cursor += frameCount;

    if (pTestDataSource->cursor > length) {
        MA_ASSERT(!"cursor > length. Test is incorrect.");
    }

    *pFramesRead = frameCount;

    return MA_SUCCESS;
}

static ma_result ma_test_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    ma_test_data_source* pTestDataSource = (ma_test_data_source*)pDataSource;
    ma_result result;
    ma_uint64 length;

    result = ma_data_source_get_length_in_pcm_frames(pDataSource, &length);
    if (result != MA_SUCCESS) {
        printf("%s: Failed to retrieve the length of the data source when reading.\n", pTestDataSource->pTest->name);
        return result;
    }

    if (frameIndex > length) {
        return MA_BAD_SEEK;
    }

    pTestDataSource->cursor = frameIndex;

    return MA_SUCCESS;
}

static ma_result ma_test_data_source_prop(ma_data_source* pDataSource, int prop, void* pData)
{
    ma_test_data_source* pTestDataSource = (ma_test_data_source*)pDataSource;

    switch (prop)
    {
        case MA_DATA_SOURCE_GET_DATA_FORMAT:
        {
            ma_data_source_data_format* pDataFormat = (ma_data_source_data_format*)pData;
            pDataFormat->format     = pTestDataSource->format;
            pDataFormat->channels   = pTestDataSource->channels;
            pDataFormat->sampleRate = pTestDataSource->sampleRate;
            
            return MA_SUCCESS;
        }

        case MA_DATA_SOURCE_GET_CURSOR:
        {
            *((ma_uint64*)pData) = pTestDataSource->cursor;
            return MA_SUCCESS;
        }

        case MA_DATA_SOURCE_GET_LENGTH:
        {
            *((ma_uint64*)pData) = pTestDataSource->length;
            return MA_SUCCESS;
        }

        case MA_DATA_SOURCE_SET_LOOPING:
        {
            pTestDataSource->isLooping = *((ma_bool32*)pData);
            return MA_SUCCESS;
        }

        case MA_DATA_SOURCE_GET_LOOPING:
        {
            *((ma_bool32*)pData) = pTestDataSource->isLooping;
            return MA_SUCCESS;
        }

        case MA_DATA_SOURCE_SET_RANGE:
        {
            ma_pcm_range* pRange = (ma_pcm_range*)pData;

            if (pRange->begInFrames > pTestDataSource->length || pRange->endInFrames > pTestDataSource->length) {
                return MA_INVALID_ARGS;
            }

            pTestDataSource->rangeBegInFrames = pRange->begInFrames;
            pTestDataSource->rangeEndInFrames = pRange->endInFrames;

            return MA_SUCCESS;
        }

        case MA_DATA_SOURCE_GET_RANGE:
        {
            ma_pcm_range* pRange = (ma_pcm_range*)pData;
            pRange->begInFrames = pTestDataSource->rangeBegInFrames;
            pRange->endInFrames = pTestDataSource->rangeEndInFrames;

            return MA_SUCCESS;
        }

        case MA_DATA_SOURCE_SET_LOOP_POINT:
        {
            ma_pcm_range* pRange = (ma_pcm_range*)pData;

            if (pRange->begInFrames > pTestDataSource->length || pRange->endInFrames > pTestDataSource->length) {
                return MA_INVALID_ARGS;
            }

            pTestDataSource->loopBegInFrames = pRange->begInFrames;
            pTestDataSource->loopEndInFrames = pRange->endInFrames;

            return MA_SUCCESS;
        }

        case MA_DATA_SOURCE_GET_LOOP_POINT:
        {
            ma_pcm_range* pRange = (ma_pcm_range*)pData;
            pRange->begInFrames = pTestDataSource->loopBegInFrames;
            pRange->endInFrames = pTestDataSource->loopEndInFrames;

            return MA_SUCCESS;
        }

        default:
        {
            return MA_NOT_IMPLEMENTED;
        }
    }
}

static ma_data_source_vtable ma_gDataSourceVTable_Test =
{
    ma_test_data_source_sizeof,
    ma_test_data_source_uninit,
    ma_test_data_source_copy,
    ma_test_data_source_read,
    ma_test_data_source_seek,
    ma_test_data_source_prop
};

ma_result ma_test_data_source_init(ma_test* pTest, ma_test_data_source* pTestDataSource)
{
    ma_result result;
    ma_data_source_config baseConfig;

    if (pTestDataSource == NULL) {
        MA_ASSERT(!"No input data source. Fix the tests!");
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pTestDataSource);
    pTestDataSource->pTest            = pTest;
    pTestDataSource->format           = ma_format_s16;
    pTestDataSource->channels         = 2;
    pTestDataSource->sampleRate       = 48000;
    pTestDataSource->cursor           = 0;
    pTestDataSource->length           = 1024;
    pTestDataSource->isInitialized    = MA_FALSE;
    pTestDataSource->isLooping        = MA_FALSE;
    pTestDataSource->rangeBegInFrames = 0;
    pTestDataSource->rangeEndInFrames = ~((ma_uint64)0);
    pTestDataSource->loopBegInFrames  = 0;
    pTestDataSource->loopEndInFrames  = ~((ma_uint64)0);
    
    baseConfig = ma_data_source_config_init();
    baseConfig.pVTable = &ma_gDataSourceVTable_Test;

    result = ma_data_source_base_init(&baseConfig, &pTestDataSource->base);
    if (result != MA_SUCCESS) {
        printf("%s: Failed to initialize test data source.\n", pTest->name);
        return result;
    }

    return MA_SUCCESS;
}


int ma_test_data_source_properties(ma_test* pTest)
{
    /* This tests that properties are properly intercepted by data sources. To do this we need a special test data source. */
    ma_result result;
    ma_test_data_source testDataSource;

    result = ma_test_data_source_init(pTest, &testDataSource);
    if (result != MA_SUCCESS) {
        printf("%s: Failed to initialize test data source.\n", pTest->name);
        return 1;
    }

    /* Basic property tests first. */
    {
        ma_data_source_data_format dataFormat;

        result = ma_data_source_get_data_format(&testDataSource, &dataFormat.format, &dataFormat.channels, &dataFormat.sampleRate);
        if (result != MA_SUCCESS) {
            printf("%s: Failed to retrieve data format.\n", pTest->name);
            return 1;
        }

        if (dataFormat.format != testDataSource.format) {
            printf("%s: Sample format does not match.\n", pTest->name);
            return 1;
        }
        if (dataFormat.channels != testDataSource.channels) {
            printf("%s: Channel counts do not match.\n", pTest->name);
            return 1;
        }
        if (dataFormat.sampleRate != testDataSource.sampleRate) {
            printf("%s: Sample rates do not match.\n", pTest->name);
            return 1;
        }
    }

    /* Looping. */
    {
        ma_bool32 isLooping = MA_TRUE;

        result = ma_data_source_set_looping(&testDataSource, isLooping);
        if (result != MA_SUCCESS) {
            printf("%s: Failed to set the looping state of the data source.\n", pTest->name);
            return 1;
        }

        if (testDataSource.isLooping != isLooping) {
            printf("%s: SET_LOOPING was not handled.\n", pTest->name);
            return 1;
        }

        if (ma_data_source_is_looping(&testDataSource) != isLooping) {
            printf("%s: is_looping() did not return expected value of %u.\n", pTest->name, isLooping);
            return 1;
        }
    }

    /* Loop Points. */
    {
        ma_uint64 loopPointBeg = 256;
        ma_uint64 loopPointEnd = loopPointBeg + 128;

        result = ma_data_source_set_loop_point_in_pcm_frames(&testDataSource, loopPointBeg, loopPointEnd);
        if (result != MA_SUCCESS) {
            printf("%s: Failed to set loop points.\n", pTest->name);
            return 1;
        }

        if (testDataSource.loopBegInFrames != loopPointBeg || testDataSource.loopEndInFrames != loopPointEnd) {
            printf("%s: SET_LOOP_POINT was not handled.\n", pTest->name);
            return 1;
        }

        {
            ma_uint64 queriedLoopPointBeg;
            ma_uint64 queriedLoopPointEnd;

            ma_data_source_get_loop_point_in_pcm_frames(&testDataSource, &queriedLoopPointBeg, &queriedLoopPointEnd);

            if (queriedLoopPointBeg != loopPointBeg || queriedLoopPointEnd != loopPointEnd) {
                printf("%s: get_loop_point_in_pcm_frames() returned enexpected values.\n", pTest->name);
                return 1;
            }
        }

        /* Erroneous Loop Points. */
        result = ma_data_source_set_loop_point_in_pcm_frames(&testDataSource, 99999, 999999);   /* Exceeds the length of the data source.*/
        if (result == MA_SUCCESS) {
            printf("%s: Expecting error from get_loop_point_in_pcm_frames().\n", pTest->name);
            return 1;
        }

        result = ma_data_source_set_loop_point_in_pcm_frames(&testDataSource, 256, 0);          /* Beg > End */
        if (result == MA_SUCCESS) {
            printf("%s: Expecting error from get_loop_point_in_pcm_frames().\n", pTest->name);
            return 1;
        }
    }

    /* Ranges. */
    {
        ma_uint64 rangeBeg = 128;
        ma_uint64 rangeEnd = rangeBeg + 512;

        result = ma_data_source_set_range_in_pcm_frames(&testDataSource, rangeBeg, rangeEnd);
        if (result != MA_SUCCESS) {
            printf("%s: Failed to set range.\n", pTest->name);
            return 1;
        }

        if (testDataSource.rangeBegInFrames != rangeBeg || testDataSource.rangeEndInFrames != rangeEnd) {
            printf("%s: SET_RANGE was not handled.\n", pTest->name);
            return 1;
        }

        {
            ma_uint64 queriedRangeBeg;
            ma_uint64 queriedRangeEnd;

            ma_data_source_get_range_in_pcm_frames(&testDataSource, &queriedRangeBeg, &queriedRangeEnd);

            if (queriedRangeBeg != rangeBeg || queriedRangeEnd != rangeEnd) {
                printf("%s: get_range_in_pcm_frames() returned enexpected values.\n", pTest->name);
                return 1;
            }
        }

        /* Erroneous Ranges. */
        result = ma_data_source_set_range_in_pcm_frames(&testDataSource, 99999, 999999);    /* Exceeds the length of the data source.*/
        if (result == MA_SUCCESS) {
            printf("%s: Expecting error from get_range_in_pcm_frames().\n", pTest->name);
            return 1;
        }

        result = ma_data_source_set_range_in_pcm_frames(&testDataSource, 256, 0);           /* Beg > End */
        if (result == MA_SUCCESS) {
            printf("%s: Expecting error from get_range_in_pcm_frames().\n", pTest->name);
            return 1;
        }
    }

    /* Length. */
    {
        /*
        The length from ma_data_source_get_length_in_pcm_frames() needs to be clamped to the range, but the
        length returned by MA_DATA_SOURCE_GET_LENGTH needs to be unclamped.
        */
        ma_uint64 clampedLength = 512;
        ma_uint64 rangeBeg = 0;
        ma_uint64 rangeEnd = rangeBeg + clampedLength;
        ma_uint64 queriedLengthClamped;
        ma_uint64 queriedLengthFull;

        MA_ASSERT(clampedLength != testDataSource.length);  /* The clamped length cannot equal the full length for this test to work. */

        ma_data_source_set_range_in_pcm_frames(&testDataSource, rangeBeg, rangeEnd);

        ma_data_source_get_length_in_pcm_frames(&testDataSource, &queriedLengthClamped);
        if (queriedLengthClamped != clampedLength) {
            printf("%s: get_length_in_pcm_frames() returned an unexpected value.\n", pTest->name);
            return 1;
        }

        ma_data_source_prop(&testDataSource, MA_DATA_SOURCE_GET_LENGTH, &queriedLengthFull);
        if (queriedLengthFull != testDataSource.length) {
            printf("%s: MA_DATA_SOURCE_GET_LENGTH returned an unexpected value.\n", pTest->name);
            return 1;
        }
    }

    return FS_SUCCESS;
}
/* END ma_test_data_source.c */


int main(int argc, char** argv)
{
    int result;
    ma_test test_root;
    ma_test test_data_source;
    ma_test test_data_source_properties;

    
    /* Root. Only used for execution. */
    ma_test_init(&test_root, NULL, NULL, NULL, NULL);

    /* Data source tests. */
    ma_test_init(&test_data_source,            "Data Source",            NULL,                           NULL, &test_root);
    ma_test_init(&test_data_source_properties, "Data Source Properties", ma_test_data_source_properties, NULL, &test_data_source);


    result = ma_test_run(&test_root);

    /* Print the test summary. */
    printf("\n");
    ma_test_print_summary(&test_root);

    (void)argc;
    (void)argv;

    if (result == FS_SUCCESS) {
        return 0;
    } else {
        return 1;
    }
}
