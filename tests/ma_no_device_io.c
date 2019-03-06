// Just a simple test to check that MA_NO_DEVICE_IO compiles.

#include "../extras/dr_flac.h"
#include "../extras/dr_mp3.h"
#include "../extras/dr_wav.h"

#define MA_NO_DEVICE_IO
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    ma_result result = MA_ERROR;

    ma_pcm_converter_config dspConfig = ma_pcm_converter_config_init_new();
    ma_pcm_converter converter;
    result = ma_pcm_converter_init(&dspConfig, &converter);
    
    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_unknown, 0, 0);
    ma_decoder decoder;
    result = ma_decoder_init_file("res/sine_s16_mono_48000.wav", &decoderConfig, &decoder);

    return result;
}

#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"
