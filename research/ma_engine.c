#define STB_VORBIS_HEADER_ONLY
#include "../extras/stb_vorbis.c"    /* Enables Vorbis decoding. */

#define MA_DEBUG_OUTPUT
#define MA_IMPLEMENTATION
#include "../miniaudio.h"
#include "ma_engine.h"

typedef struct
{
    ma_async_notification_callbacks cb;
    ma_engine* pEngine;
    ma_sound* pSound;
} sound_loaded_notification;

void on_sound_loaded(ma_async_notification* pNotification)
{
    sound_loaded_notification* pLoadedNotification = (sound_loaded_notification*)pNotification;
    ma_uint64 lengthInPCMFrames;

    /*
    This will be fired when the sound has finished loading. We should be able to retrieve the length of the sound at this point. Here we'll just set
    the fade out time.
    */
    ma_engine_sound_get_length_in_pcm_frames(pLoadedNotification->pEngine, pLoadedNotification->pSound, &lengthInPCMFrames);
    ma_engine_sound_set_fade_point_in_frames(pLoadedNotification->pEngine, pLoadedNotification->pSound, 1, 1, 0, lengthInPCMFrames - 192000, lengthInPCMFrames);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_engine engine;
    ma_sound sound;
    sound_loaded_notification loadNotification;
    

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
    loadNotification.pEngine = &engine;
    loadNotification.pSound  = &sound;

    result = ma_engine_sound_init_from_file(&engine, argv[1], MA_DATA_SOURCE_FLAG_DECODE | MA_DATA_SOURCE_FLAG_ASYNC | MA_DATA_SOURCE_FLAG_STREAM, &loadNotification, NULL, &sound);
    if (result != MA_SUCCESS) {
        printf("Failed to load sound: %s\n", argv[1]);
        ma_engine_uninit(&engine);
        return -1;
    }

    /*ma_data_source_seek_to_pcm_frame(sound.pDataSource, 5000000);*/

    //ma_engine_sound_group_set_pan(&engine, NULL, -1);
    //ma_engine_sound_group_set_pitch(&engine, NULL, 1.0f);
    //ma_engine_sound_group_set_start_delay(&engine, NULL, 2000);
    
    /*ma_engine_sound_set_volume(&engine, &sound, 0.25f);*/
    //ma_engine_sound_set_pitch(&engine, &sound, 2.0f);
    ma_engine_sound_set_pan(&engine, &sound, 0.0f);
    ma_engine_sound_set_looping(&engine, &sound, MA_TRUE);
    //ma_engine_sound_seek_to_pcm_frame(&engine, &sound, 6000000);
    //ma_engine_sound_set_start_delay(&engine, &sound, 1110);
    ma_engine_sound_set_fade_point_in_milliseconds(&engine, &sound, 0, 0, 1, 0, 2000);
    ma_engine_sound_set_stop_delay(&engine, &sound, 1000);
    ma_engine_sound_start(&engine, &sound);

    ma_sleep(2000);
    printf("Stopping...\n");
    ma_engine_sound_stop(&engine, &sound);
    //ma_engine_sound_group_stop(&engine, NULL);
#endif

#if 1
    /*ma_engine_play_sound(&engine, argv[1], NULL);*/
    /*ma_engine_play_sound(&engine, argv[2], NULL);
    ma_engine_play_sound(&engine, argv[3], NULL);*/
#endif

#if 0
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

        //ma_engine_sound_group_set_pitch(&engine, NULL, pitch);
        ma_engine_sound_set_pitch(&engine, &sound, pitch);
        printf("Pitch: %f\n", pitch);

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
