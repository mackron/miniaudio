// Just a simple test to check that MAL_NO_DEVICE_IO compiles.

#include "../extras/dr_flac.h"
#include "../extras/dr_mp3.h"
#include "../extras/dr_wav.h"

#define MAL_NO_DEVICE_IO
#define MINI_AL_IMPLEMENTATION
#include "../mini_al.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    mal_result result = MAL_ERROR;

    mal_dsp_config dspConfig = mal_dsp_config_init_new();
    mal_dsp dsp;
    result = mal_dsp_init(&dspConfig, &dsp);
    
    mal_decoder_config decoderConfig = mal_decoder_config_init(mal_format_unknown, 0, 0);
    mal_decoder decoder;
    result = mal_decoder_init_file("res/sine_s16_mono_48000.wav", &decoderConfig, &decoder);

    return result;
}

#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"
