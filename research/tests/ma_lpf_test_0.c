#define DR_FLAC_IMPLEMENTATION
#include "../../extras/dr_flac.h"  /* Enables FLAC decoding. */
#define DR_MP3_IMPLEMENTATION
#include "../../extras/dr_mp3.h"   /* Enables MP3 decoding. */
#define DR_WAV_IMPLEMENTATION
#include "../../extras/dr_wav.h"   /* Enables WAV decoding. */


#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"
#include "../ma_lpf.h"

ma_lpf g_lpf;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_uint32 framesProcessed = 0;
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) {
        return;
    }

    /* We need to read into a temporary buffer and then run it through the low pass filter. */
    while (framesProcessed < frameCount) {
        float tempBuffer[4096];
        ma_uint32 framesToProcessThisIteration;

        framesToProcessThisIteration = frameCount - framesProcessed;
        if (framesToProcessThisIteration > ma_countof(tempBuffer)/pDecoder->internalChannels) {
            framesToProcessThisIteration = ma_countof(tempBuffer)/pDecoder->internalChannels;
        }

    #if 0
        ma_decoder_read_pcm_frames(pDecoder, ma_offset_ptr(pOutput, framesProcessed * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels)), framesToProcessThisIteration);
    #else
        ma_decoder_read_pcm_frames(pDecoder, tempBuffer, framesToProcessThisIteration);

        /* Out the results from the low pass filter straight into our output buffer. */
        ma_lpf_process(&g_lpf, ma_offset_ptr(pOutput, framesProcessed * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels)), tempBuffer, framesToProcessThisIteration);
    #endif

        framesProcessed += framesToProcessThisIteration;
    }

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    ma_lpf_config lpfConfig;
    ma_device_config deviceConfig;
    ma_device device;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);

    result = ma_decoder_init_file(argv[1], &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        return -2;
    }


    lpfConfig.format          = decoderConfig.format;
    lpfConfig.channels        = decoder.internalChannels;
    lpfConfig.sampleRate      = decoder.internalSampleRate;
    lpfConfig.cutoffFrequency = lpfConfig.sampleRate / 4;

    result = ma_lpf_init(&lpfConfig, &g_lpf);
    if (result != MA_SUCCESS) {
        return -100;
    }


    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = decoder.outputFormat;
    deviceConfig.playback.channels = decoder.outputChannels;
    deviceConfig.sampleRate        = decoder.outputSampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &decoder;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        ma_decoder_uninit(&decoder);
        return -3;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
        return -4;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);

    return 0;
}
