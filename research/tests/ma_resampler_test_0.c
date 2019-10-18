#define DR_FLAC_IMPLEMENTATION
#include "../../extras/dr_flac.h"  /* Enables FLAC decoding. */
#define DR_MP3_IMPLEMENTATION
#include "../../extras/dr_mp3.h"   /* Enables MP3 decoding. */
#define DR_WAV_IMPLEMENTATION
#include "../../extras/dr_wav.h"   /* Enables WAV decoding. */


#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"
#include "../ma_resampler.h"

#define USE_NEW_RESAMPLER   1

ma_uint64 g_outputFrameCount;
void* g_pRunningFrameData;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_uint32 framesToCopy;

    framesToCopy = frameCount;
    if (framesToCopy > (ma_uint32)g_outputFrameCount) {
        framesToCopy = (ma_uint32)g_outputFrameCount;
    }

    MA_COPY_MEMORY(pOutput, g_pRunningFrameData, framesToCopy * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));

    g_pRunningFrameData = ma_offset_ptr(g_pRunningFrameData, framesToCopy * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
    g_outputFrameCount -= framesToCopy;

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_uint64 inputFrameCount;
    void* pInputFrameData;
    ma_uint64 outputFrameCount = 0;
    void* pOutputFrameData = NULL;
    ma_device_config deviceConfig;
    ma_device device;
    ma_backend backend;

    /* This example just resamples the input file to an exclusive device's native sample rate. */
    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    decoderConfig = ma_decoder_config_init(ma_format_f32, 1, 0);

    result = ma_decode_file(argv[1], &decoderConfig, &inputFrameCount, &pInputFrameData);
    if (result != MA_SUCCESS) {
        return (int)result;
    }

    backend = ma_backend_wasapi;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
#if USE_NEW_RESAMPLER
    deviceConfig.playback.shareMode = ma_share_mode_exclusive;  /* <-- We need to use exclusive mode to ensure there's no resampling going on by the OS. */
    deviceConfig.sampleRate         = 0; /* <-- Always use the device's native sample rate. */
#else
    deviceConfig.playback.shareMode = ma_share_mode_shared;  /* <-- We need to use exclusive mode to ensure there's no resampling going on by the OS. */
    deviceConfig.sampleRate         = decoderConfig.sampleRate;
#endif
    deviceConfig.playback.format    = decoderConfig.format;
    deviceConfig.playback.channels  = decoderConfig.channels;
    deviceConfig.dataCallback       = data_callback;
    deviceConfig.pUserData          = NULL;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        ma_free(pInputFrameData);
        return -3;
    }

#if USE_NEW_RESAMPLER
    /* Resample. */
    outputFrameCount = ma_calculate_frame_count_after_src(device.sampleRate, decoderConfig.sampleRate, inputFrameCount);
    pOutputFrameData = ma_malloc((size_t)(outputFrameCount * ma_get_bytes_per_frame(device.playback.format, device.playback.channels)));
    if (pOutputFrameData == NULL) {
        printf("Out of memory.\n");
        ma_free(pInputFrameData);
        ma_device_uninit(&device);
    }

    ma_resample_f32(ma_resample_algorithm_sinc, device.playback.internalSampleRate, decoderConfig.sampleRate, outputFrameCount, pOutputFrameData, inputFrameCount, pInputFrameData);

    g_pRunningFrameData = pOutputFrameData;
    g_outputFrameCount = outputFrameCount;
#else
    g_pRunningFrameData = pInputFrameData;
    g_outputFrameCount = inputFrameCount;
#endif

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_free(pInputFrameData);
        ma_free(pOutputFrameData);
        return -4;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    ma_free(pInputFrameData);
    ma_free(pOutputFrameData);

    return 0;
}

#if 0
#define SAMPLE_RATE_IN  44100
#define SAMPLE_RATE_OUT 44100
#define CHANNELS        1
#define OUTPUT_FILE     "output.wav"

ma_sine_wave sineWave;

ma_uint32 on_read(ma_resampler* pResampler, ma_uint32 frameCount, void** ppFramesOut)
{
    ma_assert(pResampler->config.format == ma_format_f32);

    return (ma_uint32)ma_sine_wave_read_f32_ex(&sineWave, frameCount, pResampler->config.channels, pResampler->config.layout, (float**)ppFramesOut);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_resampler_config resamplerConfig;
    ma_resampler resampler;

    ma_zero_object(&resamplerConfig);
    resamplerConfig.format = ma_format_f32;
    resamplerConfig.channels = CHANNELS;
    resamplerConfig.sampleRateIn = SAMPLE_RATE_IN;
    resamplerConfig.sampleRateOut = SAMPLE_RATE_OUT;
    resamplerConfig.algorithm = ma_resampler_algorithm_linear;
    resamplerConfig.endOfInputMode = ma_resampler_end_of_input_mode_consume;
    resamplerConfig.layout = ma_stream_layout_interleaved;
    resamplerConfig.onRead = on_read;
    resamplerConfig.pUserData = NULL;

    result = ma_resampler_init(&resamplerConfig, &resampler);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize resampler.\n");
        return -1;
    }

    ma_sine_wave_init(0.5, 400, resamplerConfig.sampleRateIn, &sineWave);


    // Write to a WAV file. We'll do about 10 seconds worth, making sure we read in chunks to make sure everything is seamless.
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = resampler.config.channels;
    format.sampleRate = resampler.config.sampleRateOut;
    format.bitsPerSample = 32;
    drwav* pWavWriter = drwav_open_file_write(OUTPUT_FILE, &format);

    float buffer[SAMPLE_RATE_IN*CHANNELS];
    float* pBuffer = buffer;
    ma_uint32 iterations = 10;
    ma_uint32 framesToReadPerIteration = ma_countof(buffer)/CHANNELS;
    for (ma_uint32 i = 0; i < iterations; ++i) {
        ma_uint32 framesRead = (ma_uint32)ma_resampler_read(&resampler, framesToReadPerIteration, &pBuffer);
        drwav_write(pWavWriter, framesRead*CHANNELS, buffer);
    }

    //float** test = &buffer;

    drwav_close(pWavWriter);

    (void)argc;
    (void)argv;
    return 0;
}
#endif
