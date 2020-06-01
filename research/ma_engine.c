
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
    ma_sound sound1;

    (void)argc;
    (void)argv;

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio engine.\n");
        return (int)result;
    }

    /* We can load our resource after starting the engine - the engine will deal with loading everything properly. */
    if (argc > 1) {
        result = ma_engine_create_sound_from_file(&engine, argv[1], NULL, &sound1);
        if (result != MA_SUCCESS) {
            ma_engine_uninit(&engine);
            return (int)result;
        }

        ma_engine_sound_set_pitch(&engine, &sound1, 0.75f);
        ma_engine_sound_set_pan(&engine, &sound1, 0.0f);
        ma_engine_sound_set_looping(&engine, &sound1, MA_TRUE);
        ma_engine_sound_start(&engine, &sound1);


        /*result = ma_engine_play_sound(&engine, argv[1], NULL);
        if (result != MA_SUCCESS) {
            printf("ma_engine_play_sound() failed with: %s\n", ma_result_description(result));
        }*/
    }


    printf("Press Enter to quit...");
    getchar();

    /* Normally you would uninitialize and clean up all of your sounds manually because ma_engine_uninit() will _not_ do it for you. */
    ma_engine_uninit(&engine);

    return 0;
}
