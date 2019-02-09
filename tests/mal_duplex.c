#include <stdio.h>

#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

void log_callback(mal_context* pContext, mal_device* pDevice, mal_uint32 logLevel, const char* message)
{
    (void)pContext;
    (void)pDevice;
    (void)logLevel;
    printf("%s\n", message);
}

void stop_callback(mal_device* pDevice)
{
    (void)pDevice;
    printf("STOPPED\n");
}

void data_callback(mal_device* pDevice, void* pOutput, const void* pInput, mal_uint32 frameCount)
{
    /* In this test the format and channel count are the same for both input and output which means we can just memcpy(). */
    mal_copy_memory(pOutput, pInput, frameCount * mal_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels));

    /* Also write to a wav file for debugging. */
    drwav* pWav = (drwav*)pDevice->pUserData;
    mal_assert(pWav != NULL);

    drwav_write_pcm_frames(pWav, frameCount, pInput);
}

int main(int argc, char** argv)
{
    mal_result result;

    drwav_data_format wavFormat;
    wavFormat.container     = drwav_container_riff;
    wavFormat.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
    wavFormat.channels      = 2;
    wavFormat.sampleRate    = 44100;
    wavFormat.bitsPerSample = 32;

    drwav wav;
    if (drwav_init_file_write(&wav, "output.wav", &wavFormat) == DRWAV_FALSE) {
        printf("Failed to initialize output file.\n");
        return -1;
    }
    

    mal_backend backend = mal_backend_dsound;

    mal_context_config contextConfig = mal_context_config_init();
    contextConfig.logCallback = log_callback;

    mal_context context;
    result = mal_context_init(&backend, 1, &contextConfig, &context);
    if (result != MAL_SUCCESS) {
        printf("Failed to initialize context.\n");
        return result;
    }

    mal_device_config deviceConfig = mal_device_config_init(mal_device_type_duplex);
    deviceConfig.capture.pDeviceID  = NULL;
    deviceConfig.capture.format     = mal_format_f32;
    deviceConfig.capture.channels   = 2;
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format    = mal_format_f32;
    deviceConfig.playback.channels  = 2;
    deviceConfig.sampleRate         = 44100;
    deviceConfig.bufferSizeInMilliseconds = 50;
    deviceConfig.periods            = 3;
    deviceConfig.dataCallback       = data_callback;
    deviceConfig.stopCallback       = stop_callback;
    deviceConfig.pUserData          = &wav;

    mal_device device;
    result = mal_device_init(&context, &deviceConfig, &device);
    if (result != MAL_SUCCESS) {
        return result;
    }

    mal_device_start(&device);

    printf("Press Enter to quit...\n");
    getchar();

    mal_device_uninit(&device);
    drwav_uninit(&wav);

    (void)argc;
    (void)argv;
    return 0;
}