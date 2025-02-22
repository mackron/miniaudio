/*
This example shows how to plug in custom backends.

To use a custom backend you need to plug in a `ma_device_backend_vtable` pointer into the context config. You can plug in multiple
custom backends, but for this example we're just using the SDL backend which you can find in the extras folder in the miniaudio
repository. If your custom backend requires it, you can also plug in a user data pointer which will be passed to the backend callbacks.

Custom backends are identified with the `ma_backend_custom` backend type. For the purpose of demonstration, this example only uses the
`ma_backend_custom` backend type because otherwise the built-in backends would always get chosen first and none of the code for the custom
backends would actually get hit. By default, the `ma_backend_custom` backend is the second-lowest priority backend, sitting just above
`ma_backend_null`.
*/
#include "../miniaudio.c"

/* We're using SDL for this example. To use this in your own program, you need to include backend_sdl.h after miniaudio.h. */
#include "../extras/backends/sdl/backend_sdl.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

void main_loop__em()
{
}
#endif


/*
Main program starts here.
*/
#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    if (pDevice->type == ma_device_type_playback)  {
        ma_waveform_read_pcm_frames((ma_waveform*)pDevice->pUserData, pOutput, frameCount, NULL);
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
    char name[256];

    /*
    We're just using ma_backend_custom in this example for demonstration purposes, but a more realistic use case would probably want to include
    other backends as well for robustness.
    */
    ma_backend backends[] = {
        ma_backend_custom
    };


    /*
    Here is where we would config the SDL-specific context-level config. The custom SDL backend allows this to be null, but we're
    defining it here just for the sake of demonstration. Whether or not this is required depends on the backend. If you're not sure,
    check the documentation for the backend.
    */
    ma_context_config_sdl sdlContextConfig = ma_context_config_sdl_init();
    sdlContextConfig._unused = 0;

    /*
    You must include an entry for each backend you're using, even if the config is NULL. This is how miniaudio knows about
    your custom backend.
    */
    ma_device_backend_spec pCustomContextConfigs[] = {
        { MA_DEVICE_BACKEND_VTABLE_SDL, &sdlContextConfig, NULL }
    };

    contextConfig = ma_context_config_init();
    contextConfig.custom.pBackends = pCustomContextConfigs;
    contextConfig.custom.count     = (sizeof(pCustomContextConfigs) / sizeof(pCustomContextConfigs[0]));
    

    result = ma_context_init(backends, sizeof(backends)/sizeof(backends[0]), &contextConfig, (ma_context*)&context);
    if (result != MA_SUCCESS) {
        return -1;
    }

    /* In playback mode we're just going to play a sine wave. */
    sineWaveConfig = ma_waveform_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE, ma_waveform_type_sine, 0.2, 220);
    ma_waveform_init(&sineWaveConfig, &sineWave);


    
    /*
    Just like with context configs, we can define some device-level configs as well. It works the same way, except you will pass in
    a backend-specific device-level config. If the backend doesn't require a device-level config, you can set this to NULL.
    */
    ma_device_config_sdl sdlDeviceConfig = ma_device_config_sdl_init();
    sdlDeviceConfig._unused = 0;

    /*
    Unlike with contexts, if your backend does not require a device-level config, you can just leave it out of this list entirely.
    */
    ma_device_backend_spec pCustomDeviceConfigs[] = {
        { MA_DEVICE_BACKEND_VTABLE_SDL, &sdlDeviceConfig, NULL }
    };

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.capture.format    = DEVICE_FORMAT;
    deviceConfig.capture.channels  = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &sineWave;
    deviceConfig.custom.pBackends  = pCustomDeviceConfigs;
    deviceConfig.custom.count      = sizeof(pCustomDeviceConfigs) / sizeof(pCustomDeviceConfigs[0]);

    result = ma_device_init((ma_context*)&context, &deviceConfig, (ma_device*)&device);
    if (result != MA_SUCCESS) {
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
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop__em, 0, 1);
#else
    printf("Press Enter to quit...\n");
    getchar();
#endif
    
    ma_device_uninit((ma_device*)&device);
    ma_context_uninit((ma_context*)&context);

    (void)argc;
    (void)argv;

    return 0;
}

/* We put the SDL implementation here just to simplify the compilation process. This way you need only compile custom_backend.c. */
#include "../extras/backends/sdl/backend_sdl.c"
