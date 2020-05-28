
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

    /* Currently an explicit start is required. Perhaps make it so this is started by default, or maybe start it when the first sound is started? Maybe make it an option? */
    result = ma_engine_start(&engine);  /* Do we want the engine to be started by default? */
    if (result != MA_SUCCESS) {
        ma_engine_uninit(&engine);
        return (int)result;
    }


    /* We can load our resource after starting the engine - the engine will deal with loading everything properly. */
    if (argc > 1) {
        result = ma_engine_create_sound_from_file(&engine, argv[1], NULL, &sound1);
        if (result != MA_SUCCESS) {
            ma_engine_uninit(&engine);
            return (int)result;
        }

        ma_engine_sound_start(&engine, &sound1);
    }


    printf("Press Enter to quit...");
    getchar();

    return 0;
}
