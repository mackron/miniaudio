//#define MA_NO_THREADING
#include "../../miniaudio.c"

ma_context context;
ma_device device;
ma_waveform waveform;
ma_decoder decoder;

typedef struct allocation_callbacks_data
{
    ma_uint32 allocationCount;
    ma_uint32 totalAllocationSizeInBytes;
} allocation_callbacks_data;

static void* test_malloc(size_t sz, void* pUserData)
{
    allocation_callbacks_data* pData = (allocation_callbacks_data*)pUserData;

    printf("malloc(%d)\n", (int)sz);

    pData->allocationCount += 1;
    pData->totalAllocationSizeInBytes += sz;
    return malloc(sz);
}

static void* test_realloc(void* p, size_t sz, void* pUserData)
{
    allocation_callbacks_data* pData = (allocation_callbacks_data*)pUserData;

    printf("realloc(%d)\n", (int)sz);

    pData->totalAllocationSizeInBytes += sz;    /* Don't actually know how to properly track this. Might need to keep a list of allocations. */
    return realloc(p, sz);
}

static void test_free(void* p, void* pUserData)
{
    allocation_callbacks_data* pData = (allocation_callbacks_data*)pUserData;

    printf("free()\n");

    pData->allocationCount -= 1;
    free(p);
}

allocation_callbacks_data allocationCallbacksData;


static void data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    (void)pDevice;
    (void)pFramesOut;
    (void)pFramesIn;
    (void)frameCount;

    //printf("data_callback(%d)\n", (int)frameCount);
    //printf("internalePeriodSizeInFrames = %d\n", device.playback.internalPeriodSizeInFrames);

    //ma_waveform_read_pcm_frames(&waveform, pFramesOut, frameCount, NULL);
    ma_decoder_read_pcm_frames(&decoder, pFramesOut, frameCount, NULL);

    // Just applying a volume factor to make testing a bit more bearable...
    ma_apply_volume_factor_pcm_frames(pFramesOut, frameCount, ma_device_get_format(pDevice, ma_device_type_playback), ma_device_get_channels(pDevice, ma_device_type_playback), 0.2f);
}

int main()
{
    ma_result result;
    ma_context_config contextConfig;
    ma_device_config deviceConfig;
    uint32 prevButtons = 0;

    dbgio_init();
    dbgio_dev_select("fb");


    vid_init(DEFAULT_VID_MODE, PM_RGB565);
    vid_clear(64, 192, 64);

    bfont_draw_str(vram_s + 20 * 640 + 20, 640, 0, "MINIAUDIO");

    printf("sizeof(ma_context)         = %d\n", (int)sizeof(ma_context));
    printf("sizeof(ma_device)          = %d\n", (int)sizeof(ma_device));
    printf("sizeof(ma_device.playback) = %d\n", (int)sizeof(device.playback));
    printf("sizeof(ma_data_converter)  = %d\n", (int)sizeof(ma_data_converter));


    memset(&allocationCallbacksData, 0, sizeof(allocationCallbacksData));

    contextConfig = ma_context_config_init();
    contextConfig.allocationCallbacks.onMalloc  = test_malloc;
    contextConfig.allocationCallbacks.onRealloc = test_realloc;
    contextConfig.allocationCallbacks.onFree    = test_free;
    contextConfig.allocationCallbacks.pUserData = &allocationCallbacksData;

    result = ma_context_init(NULL, 0, &contextConfig, &context);
    if (result != MA_SUCCESS) {
        printf("ma_context_init() failed.");
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.threadingMode        = MA_THREADING_MODE_SINGLE_THREADED;
    deviceConfig.playback.format      = ma_format_s16;
    deviceConfig.playback.channels    = 2;
    deviceConfig.sampleRate           = 44100;
    deviceConfig.periodSizeInFrames   = 2048;
    deviceConfig.noFixedSizedCallback = MA_TRUE;    /* With the period size between 2048 and 16384 we can be guaranteed a fixed sized callback. */
    deviceConfig.dataCallback         = data_callback;
    deviceConfig.pUserData            = NULL;
    result = ma_device_init(&context, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("ma_device_init() failed.\n");
    }

    printf("internalPeriodSizeInFrames = %d\n", device.playback.internalPeriodSizeInFrames);
    printf("format: %d %d\n", device.playback.format, device.playback.internalFormat);
    printf("channels: %d %d\n", device.playback.channels, device.playback.internalChannels);
    printf("rate: %d %d\n", device.sampleRate, device.playback.internalSampleRate);

    /* Waveform. */
    {
        ma_waveform_config waveformConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.1f, 400);
        ma_waveform_init(&waveformConfig, &waveform);
    }

    /* Decoder. */
    {
        ma_decoder_config decoderConfig = ma_decoder_config_init(device.playback.format, device.playback.channels, 0);
        ma_decoder_init_file("/rd/test.mp3", &decoderConfig, &decoder);
    }

    printf("Allocation Count = %d\n", (int)allocationCallbacksData.allocationCount);
    printf("Allocation Size  = %d\n", (int)allocationCallbacksData.totalAllocationSizeInBytes);

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("ma_device_start() failed.\n");
    }

    thd_sleep(1);

    for (;;) {
        maple_device_t* dev;
        cont_state_t* st;

        dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER); /* 0 = first controller found */
        if (dev) {
            st = (cont_state_t*)maple_dev_status(dev);
            if (st) {
                uint32 buttons;
                uint32 pressed;
                uint32 released;

                buttons  = st->buttons; /* bitmask of CONT_* */
                pressed  = (buttons ^ prevButtons) & buttons;
                released = (buttons ^ prevButtons) & prevButtons;
                prevButtons = buttons;

                (void)released;

                if (pressed & CONT_START) {
                    break;
                }

                if (buttons & CONT_DPAD_LEFT) {
                    /* Held */
                }
            }
        }

        ma_device_step(&device, MA_BLOCKING_MODE_NON_BLOCKING);

        thd_pass(); /* yield */
        //thd_sleep(1);
    }

    result = ma_device_stop(&device);
    if (result != MA_SUCCESS) {
        printf("ma_device_start() failed.\n");
    }

    ma_device_uninit(&device);
    ma_context_uninit(&context);

    return 0;
}
