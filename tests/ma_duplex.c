#include <stdio.h>

#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"

#define OUTPUT_WAV 0

#ifdef __EMSCRIPTEN__
void main_loop__em()
{
}
#endif

void log_callback(ma_context* pContext, ma_device* pDevice, ma_uint32 logLevel, const char* message)
{
    (void)pContext;
    (void)pDevice;
    (void)logLevel;
    printf("%s\n", message);
}

void stop_callback(ma_device* pDevice)
{
    (void)pDevice;
    printf("STOPPED\n");
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    /* In this test the format and channel count are the same for both input and output which means we can just memcpy(). */
    ma_copy_memory(pOutput, pInput, frameCount * ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels));

#if defined(OUTPUT_WAV) && OUTPUT_WAV==1
    /* Also write to a wav file for debugging. */
    drwav* pWav = (drwav*)pDevice->pUserData;
    ma_assert(pWav != NULL);

    drwav_write_pcm_frames(pWav, frameCount, pInput);
#endif
}

int main(int argc, char** argv)
{
    ma_result result;

#if defined(OUTPUT_WAV) && OUTPUT_WAV==1
    drwav_data_format wavFormat;
    wavFormat.container     = drwav_container_riff;
    wavFormat.format        = DR_WAVE_FORMAT_PCM;
    wavFormat.channels      = 2;
    wavFormat.sampleRate    = 44100;
    wavFormat.bitsPerSample = 16;

    drwav wav;
    if (drwav_init_file_write(&wav, "output.wav", &wavFormat) == DRWAV_FALSE) {
        printf("Failed to initialize output file.\n");
        return -1;
    }
#endif
    

    ma_backend backend = ma_backend_wasapi;

    ma_context_config contextConfig = ma_context_config_init();
    contextConfig.logCallback = log_callback;
    contextConfig.alsa.useVerboseDeviceEnumeration = MA_TRUE;
    contextConfig.threadPriority = ma_thread_priority_realtime;

    ma_context context;
    result = ma_context_init(&backend, 1, &contextConfig, &context);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize context.\n");
        return result;
    }


    /* ALSA debugging. */
#if defined(MA_SUPPORT_ALSA)
    if (backend == ma_backend_alsa) {
        ma_device_info* pPlaybackDevices;
        ma_uint32 playbackDeviceCount;
        ma_device_info* pCaptureDevices;
        ma_uint32 captureDeviceCount;
        ma_context_get_devices(&context, &pPlaybackDevices, &playbackDeviceCount, &pCaptureDevices, &captureDeviceCount);

        printf("Playback Devices:\n");
        for (ma_uint32 iDevice = 0; iDevice < playbackDeviceCount; iDevice += 1) {
            printf("    ALSA Device ID: %s\n", pPlaybackDevices[iDevice].id.alsa);
        }
        printf("Capture Devices:\n");
        for (ma_uint32 iDevice = 0; iDevice < captureDeviceCount; iDevice += 1) {
            printf("    ALSA Device ID: %s\n", pCaptureDevices[iDevice].id.alsa);
        }
    }
#endif


    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.pDeviceID        = NULL;
    deviceConfig.capture.format           = ma_format_s16;
    deviceConfig.capture.channels         = 2;
    deviceConfig.capture.shareMode        = ma_share_mode_shared;
    deviceConfig.playback.pDeviceID       = NULL;
    deviceConfig.playback.format          = ma_format_s16;
    deviceConfig.playback.channels        = 2;
    deviceConfig.playback.shareMode       = ma_share_mode_shared;
    deviceConfig.sampleRate               = 0;
    deviceConfig.bufferSizeInFrames       = 0;
    deviceConfig.bufferSizeInMilliseconds = 60;
    deviceConfig.periods                  = 3;
    deviceConfig.dataCallback             = data_callback;
    deviceConfig.stopCallback             = stop_callback;
#if defined(OUTPUT_WAV) && OUTPUT_WAV==1
    deviceConfig.pUserData          = &wav;
#endif

    ma_device device;
    result = ma_device_init(&context, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* For debugging. */
    printf("device.playback.internalBufferSizeInFrames = %d\n", device.playback.internalBufferSizeInFrames);
    printf("device.playback.internalPeriods            = %d\n", device.playback.internalPeriods);
    printf("device.capture.internalBufferSizeInFrames  = %d\n", device.capture.internalBufferSizeInFrames);
    printf("device.capture.internalPeriods             = %d\n", device.capture.internalPeriods);


#ifdef __EMSCRIPTEN__
    getchar();
#endif

    ma_device_start(&device);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop__em, 0, 1);
#else
    printf("Press Enter to quit...\n");
    getchar();
#endif

    ma_device_uninit(&device);

#if defined(OUTPUT_WAV) && OUTPUT_WAV==1
    drwav_uninit(&wav);
#endif

    (void)argc;
    (void)argv;
    return 0;
}
