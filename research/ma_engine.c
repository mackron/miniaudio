#define STB_VORBIS_HEADER_ONLY
#include "../extras/stb_vorbis.c"    /* Enables Vorbis decoding. */

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


#if 1
    result = ma_engine_sound_init_from_file(&engine, argv[1], MA_DATA_SOURCE_FLAG_DECODE /*| MA_DATA_SOURCE_FLAG_ASYNC | MA_DATA_SOURCE_FLAG_STREAM*/, NULL, &sound);
    if (result != MA_SUCCESS) {
        printf("Failed to load sound: %s\n", argv[1]);
        ma_engine_uninit(&engine);
        return -1;
    }

    /*ma_data_source_seek_to_pcm_frame(sound.pDataSource, 5000000);*/
    
    ma_engine_sound_set_volume(&engine, &sound, 0.25f);
    ma_engine_sound_set_pitch(&engine, &sound, 1.0f);
    ma_engine_sound_set_pan(&engine, &sound, 0.0f);
    ma_engine_sound_set_looping(&engine, &sound, MA_TRUE);
    ma_engine_sound_start(&engine, &sound);
#endif

#if 1
    /*ma_engine_play_sound(&engine, argv[1], NULL);*/
    /*ma_engine_play_sound(&engine, argv[2], NULL);
    ma_engine_play_sound(&engine, argv[3], NULL);*/
#endif

#if 1
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

        ma_sleep(1);
    }
#endif

    printf("Press Enter to quit...");
    getchar();

    ma_engine_sound_uninit(&engine, &sound);
    ma_engine_uninit(&engine);

    return 0;
}

/* stb_vorbis implementation must come after the implementation of miniaudio. */
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(push)
    #pragma warning(disable:4100)   /* unreferenced formal parameter */
    #pragma warning(disable:4244)   /* '=': conversion from '' to '', possible loss of data */
    #pragma warning(disable:4245)   /* '=': conversion from '' to '', signed/unsigned mismatch */
    #pragma warning(disable:4456)   /* declaration of '' hides previous local declaration */
    #pragma warning(disable:4457)   /* declaration of '' hides function parameter */
    #pragma warning(disable:4701)   /* potentially uninitialized local variable '' used */
#else
#endif
#undef STB_VORBIS_HEADER_ONLY
#include "../extras/stb_vorbis.c"
#if defined(_MSC_VER) && !defined(__clang__)
    #pragma warning(pop)
#else
#endif
