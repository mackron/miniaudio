/*
Demonstrates playback of a time-varying sine wave by directly specifying the sample values.

The intention is that a new miniaudio user who wants to learn the
low-level API can start here.  In contrast, `simple_playback_sine.c`
uses the `ma_waveform` API to generate samples, which makes it a little
bit harder to understand.

This example works with Emscripten.
*/

/* The encoding and decoding facilities are not needed here. */
#define MA_NO_DECODING
#define MA_NO_ENCODING

#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"              /* ma_XXX, MA_XXX */

#include <math.h>                      /* floor */
#include <stdlib.h>                    /* strtod */

#ifdef __EMSCRIPTEN__
#include <emscripten.h>                /* emscripten_set_main_loop */

void main_loop__em()
{
}
#endif


/* Use a common format. */
#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  48000


/* Describes a wave to produce, and where we are within that wave. */
typedef struct SineWave {
  /* Central value around which we oscillate. */
  double center;

  /* Amount to go above and below the center. */
  double amplitude;

  /* Frequency with which we repeat, in Hz. */
  double frequency;

  /* Current phase in [0,1). */
  double phase;
} SineWave;


/* Advance 'wave' by one audio sample of time, and return the value of
 * the wave form at that point. */
double nextWaveSample(SineWave *wave)
{
  /* Advance the phase. */
  wave->phase += wave->frequency / DEVICE_SAMPLE_RATE;
  if (wave->phase >= 1.0) {
    wave->phase -= floor(wave->phase);
  }

  /* Convert phase into amplitude. */
  return wave->center + ma_sind(MA_TAU_D * wave->phase) * wave->amplitude;
}


/* The output sound frequency will vary over time.  By default, it
 * runs from 250 to 350 Hz and back over the period of one second,
 * making a sort of "woo woo" sound, hence this file's name. */
SineWave frequencyWave = { 300, 50, 1 };


/* The output sound wave itself.  Its frequency varies. */
SineWave soundWave = { 0, 1 };


void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    /* Each sample is represented by a single 'float' when using
     * 'ma_format_f32'. */
    float* pOutputF32 = (float*)pOutput;

    /* Frame counter in [0, frameCount-1]. */
    ma_uint32 curFrame;

    /* This callback is tied to the specific sample format and rate. */
    MA_ASSERT(pDevice->playback.format   == DEVICE_FORMAT);
    MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);
    MA_ASSERT(pDevice->sampleRate        == DEVICE_SAMPLE_RATE);

    /* Populate 'pOutput' with 'frameCount' frames. */
    for (curFrame = 0; curFrame < frameCount; ++curFrame) {
        /* Advance the frequency wave. */
        soundWave.frequency = nextWaveSample(&frequencyWave);

        /* Advance the sound wave to obtain the sound sample value
         * in [-1,1].  This represents, to a first approximation, the
         * physical location of the speaker cone within its range of
         * travel as it moves in order to generate audible sound. */
        float value = (float)nextWaveSample(&soundWave);

        /* Stereo output has two channels. */
        pOutputF32[curFrame * DEVICE_CHANNELS + 0] = value;
        pOutputF32[curFrame * DEVICE_CHANNELS + 1] = value;
    }

    (void)pInput;   /* Unused. */
}


int main(int argc, char** argv)
{
    ma_device_config deviceConfig;
    ma_device device;

    /* Command line can specify center, amplitude, and frequency. */
    if (argc >= 2) {
        frequencyWave.center = strtod(argv[1], NULL);
        if (argc >= 3) {
            frequencyWave.amplitude = strtod(argv[2], NULL);
            if (argc >= 4) {
                frequencyWave.frequency = strtod(argv[3], NULL);
            }
        }
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    printf("Device Name: %s\n", device.playback.name);

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return -5;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop__em, 0, 1);
#else
    printf("Press Enter to quit...\n");
    getchar();
#endif

    ma_device_uninit(&device);

    return 0;
}
