/*
Compile with the following:

    gcc miniaudio_pipewire_test.c -lm -I/usr/include/spa-0.2
*/

#define MA_DEBUG_OUTPUT
#include "../../../miniaudio.c"
#include "miniaudio_pipewire.c"


/*
Main program starts here.
*/
#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);

    if (pDevice->type == ma_device_type_playback)  {
        ma_waveform* pSineWave;

        pSineWave = (ma_waveform*)pDevice->pUserData;
        MA_ASSERT(pSineWave != NULL);

        //printf("WAVEFORM: %d\n", (int)frameCount);
        ma_waveform_read_pcm_frames(pSineWave, pOutput, frameCount, NULL);
    }

    if (pDevice->type == ma_device_type_duplex) {
        ma_copy_pcm_frames(pOutput, pInput, frameCount, pDevice->playback.format, pDevice->playback.channels);
    }
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_context_config contextConfig;
    ma_context context;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform_config sineWaveConfig;
    ma_waveform sineWave;
    ma_context_config_pipewire pipewireContextConfig;
    ma_device_config_pipewire pipewireDeviceConfig;
    char name[256];


    /* Plug in our vtable pointers. Add any custom backends to this list. */
    pipewireContextConfig = ma_context_config_pipewire_init();

    ma_device_backend_config pBackends[] =
    {
        { ma_device_backend_pipewire, &pipewireContextConfig }
    };

    contextConfig = ma_context_config_init();

    result = ma_context_init(pBackends, sizeof(pBackends)/sizeof(pBackends[0]), &contextConfig, (ma_context*)&context);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize context.\n");
        return -1;
    }


    /* In playback mode we're just going to play a sine wave. */
    sineWaveConfig = ma_waveform_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE, ma_waveform_type_sine, 0.2, 220);
    ma_waveform_init(&sineWaveConfig, &sineWave);


    /* The device is created exactly as per normal. */
    pipewireDeviceConfig = ma_device_config_pipewire_init();

    ma_device_backend_config pBackendDeviceConfigs[] =
    {
        { ma_device_backend_pipewire, &pipewireDeviceConfig }
    };

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format    = DEVICE_FORMAT;
    deviceConfig.playback.channels  = DEVICE_CHANNELS;
    deviceConfig.capture.format     = DEVICE_FORMAT;
    deviceConfig.capture.channels   = DEVICE_CHANNELS;
    deviceConfig.sampleRate         = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback       = data_callback;
    deviceConfig.pUserData          = &sineWave;
    deviceConfig.pBackendConfigs    = pBackendDeviceConfigs;
    deviceConfig.backendConfigCount = (sizeof(pBackendDeviceConfigs) / sizeof(pBackendDeviceConfigs[0]));
    deviceConfig.periodSizeInMilliseconds = 20;

    result = ma_device_init((ma_context*)&context, &deviceConfig, (ma_device*)&device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize device.\n");
        ma_context_uninit((ma_context*)&context);
        return -1;
    }


    ma_device_get_name((ma_device*)&device, ma_device_type_playback, name, sizeof(name), NULL);
    printf("Device Name: %s\n", name);

    if (ma_device_start((ma_device*)&device) != MA_SUCCESS) {
        ma_device_uninit((ma_device*)&device);
        ma_context_uninit((ma_context*)&context);
        return -5;
    }
    
    printf("Press Enter to quit...\n");
    getchar();

    ma_device_uninit((ma_device*)&device);
    ma_context_uninit((ma_context*)&context);

    (void)argc;
    (void)argv;

    return 0;
}
