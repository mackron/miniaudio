
#define DR_FLAC_IMPLEMENTATION
#include "../extras/dr_flac.h"  /* Enables FLAC decoding. */
#define DR_MP3_IMPLEMENTATION
#include "../extras/dr_mp3.h"   /* Enables MP3 decoding. */
#define DR_WAV_IMPLEMENTATION
#include "../extras/dr_wav.h"   /* Enables WAV decoding. */

#define MA_DEBUG_OUTPUT
#define MA_IMPLEMENTATION
#include "../miniaudio.h"
#include "ma_engine.h"

int main(int argc, char** argv)
{
    ma_result result;
    ma_engine engine;
    ma_sound sound;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio engine.\n");
        return -1;
    }

    result = ma_engine_sound_init_from_file(&engine, argv[1], NULL, &sound);
    if (result != MA_SUCCESS) {
        ma_engine_uninit(&engine);
        return -1;
    }

    ma_engine_sound_set_pitch(&engine, &sound, 0.75f);
    ma_engine_sound_set_pan(&engine, &sound, 0.0f);
    ma_engine_sound_set_looping(&engine, &sound, MA_TRUE);
    ma_engine_sound_start(&engine, &sound);


    float pitch     = 1;
    float pitchStep = 0.01f;
    float pitchMin  = 0.125f;
    float pitchMax  = 8;
    for (;;) {
        pitch += pitchStep;
        if (pitch < pitchMin) {
            pitch = pitchMin;
            pitchStep = -pitchStep;
        }
        if (pitch > pitchMax) {
            pitch = pitchMax;
            pitchStep = -pitchStep;
        }

        ma_engine_sound_set_pitch(&engine, &sound, pitch);

        Sleep(1);
    }

    printf("Press Enter to quit...");
    getchar();

    ma_engine_sound_uninit(&engine, &sound);
    ma_engine_uninit(&engine);

    return 0;
}
