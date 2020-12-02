//#include "../test_deviceio/ma_test_deviceio.c"

//#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
    ma_backend backend = ma_backend_null;
    for(int i=0;i<1000;++i){
        deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format   = ma_format_f32;
        deviceConfig.playback.channels = 2;
        deviceConfig.sampleRate        = 44100;
        deviceConfig.dataCallback      = data_callback;
        deviceConfig.pUserData         = NULL;
        printf("iter: %d\n", i);
        printf("init\n");
        if (ma_device_init_ex(&backend, 1, NULL, &deviceConfig, &device) != MA_SUCCESS) {
            printf("Failed to open playback device.\n");
            return -1;
        }
        printf("start\n");
        if (ma_device_start(&device) != MA_SUCCESS) {
            printf("Failed to start playback device.\n");
            ma_device_uninit(&device);
            return -1;
        }
        printf("uninit\n");
        ma_device_uninit(&device);
    }
    return 0;
}