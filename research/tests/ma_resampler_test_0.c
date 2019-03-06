#define DR_WAV_IMPLEMENTATION
#include "../../../../dr_libs/dr_wav.h"

#define MA_DEBUG_OUTPUT
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"
#include "../ma_resampler.h"

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