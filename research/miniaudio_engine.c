#define STB_VORBIS_HEADER_ONLY
#include "../extras/stb_vorbis.c"    /* Enables Vorbis decoding. */

#define MA_EXPERIMENTAL__DATA_LOOPING_AND_CHAINING
#define MA_DEBUG_OUTPUT
#define MA_IMPLEMENTATION
#include "../miniaudio.h"
#include "miniaudio_engine.h"

typedef struct
{
    ma_async_notification_callbacks cb;
    ma_sound* pSound;
} sound_loaded_notification;

void on_sound_loaded(ma_async_notification* pNotification)
{
    //sound_loaded_notification* pLoadedNotification = (sound_loaded_notification*)pNotification;
    //ma_uint64 lengthInPCMFrames;

    (void)pNotification;

    /*
    This will be fired when the sound has finished loading. We should be able to retrieve the length of the sound at this point. Here we'll just set
    the fade out time.
    */
    //ma_sound_get_length_in_pcm_frames(pLoadedNotification->pSound, &lengthInPCMFrames);
    //ma_sound_set_fade_point_in_frames(pLoadedNotification->pSound, 1, 1, 0, lengthInPCMFrames - 192000, lengthInPCMFrames);
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_resource_manager resourceManager;
    ma_resource_manager_config resourceManagerConfig;
    ma_engine engine;
    ma_engine_config engineConfig;
    ma_sound baseSound;
    ma_sound sound;
    ma_sound sound2;
    sound_loaded_notification loadNotification;
    ma_sound_group group;
    

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    resourceManagerConfig = ma_resource_manager_config_init();
    //resourceManagerConfig.decodedFormat = ma_format_f32;
    //resourceManagerConfig.decodedChannels = 2;
    resourceManagerConfig.decodedSampleRate = 48000;
    //resourceManagerConfig.flags |= MA_RESOURCE_MANAGER_FLAG_NO_THREADING;
    resourceManagerConfig.jobThreadCount = 1;
    result = ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize resource manager.\n");
        return -1;
    }

    engineConfig = ma_engine_config_init();
    engineConfig.pResourceManager = &resourceManager;

    result = ma_engine_init(&engineConfig, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio engine.\n");
        return -1;
    }

    result = ma_sound_group_init(&engine, 0, NULL, &group);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize sound group.");
        return -1;
    }


#if 1
    loadNotification.cb.onSignal = on_sound_loaded;
    loadNotification.pSound = &sound;

    result = ma_sound_init_from_file(&engine, argv[1], MA_DATA_SOURCE_FLAG_DECODE | MA_DATA_SOURCE_FLAG_ASYNC /*| MA_DATA_SOURCE_FLAG_STREAM*/, &group, NULL, &baseSound);
    if (result != MA_SUCCESS) {
        printf("Failed to load sound: %s\n", argv[1]);
        ma_engine_uninit(&engine);
        return -1;
    }

    result = ma_sound_init_copy(&engine, &baseSound, 0, &group, &sound);
    if (result != MA_SUCCESS) {
        printf("Failed to copy sound.\n");
        return -1;
    }

#if 0
    result = ma_sound_init_from_file(&engine, argv[1], MA_DATA_SOURCE_FLAG_DECODE /*| MA_DATA_SOURCE_FLAG_ASYNC | MA_DATA_SOURCE_FLAG_STREAM*/, NULL, &sound2);
    if (result != MA_SUCCESS) {
        printf("Failed to load sound: %s\n", argv[1]);
        ma_engine_uninit(&engine);
        return -1;
    }
#endif

    

    /*ma_data_source_seek_to_pcm_frame(sound.pDataSource, 5000000);*/

    //ma_sound_group_set_pan(ma_engine_get_master_sound_group(&engine), -1);
    ma_sound_group_set_pitch(&group, 1.25f);
    //ma_sound_group_set_start_time(ma_engine_get_master_sound_group(&engine), 2000);
    //ma_sound_group_set_fade_in_milliseconds(&group, 0, 1, 5000);
    //ma_sound_group_stop(&group);
    
    //ma_sound_set_fade_in_milliseconds(&sound, 0, 1, 5000);
    /*ma_sound_set_volume(&sound, 0.25f);*/
    /*ma_sound_set_pitch(&sound, 1.1f);*/
    /*ma_sound_set_pan(&sound, 0.0f);*/
    ma_sound_set_looping(&sound, MA_TRUE);
    //ma_sound_seek_to_pcm_frame(&sound, 6000000);
    //ma_sound_set_start_time(&sound, 1110);
    //ma_sound_set_volume(&sound, 0.5f);
    //ma_sound_set_fade_point_in_milliseconds(&sound, 0, 0, 1, 0, 2000);
    //ma_sound_set_fade_point_auto_reset(&sound, 0, MA_FALSE);    /* Enable fading around loop transitions. */
    //ma_sound_set_fade_point_auto_reset(&sound, 1, MA_FALSE);
    //ma_sound_set_stop_time(&sound, 1000);
    //ma_sound_set_volume(&sound, 1);
    //ma_sound_set_start_time(&sound, 48000);
    ma_sound_set_position(&sound, 0, 0, -1);
    //ma_sound_set_spatialization_enabled(&sound, MA_FALSE);
    ma_sound_start(&sound);
    /*ma_sound_uninit(&sound);*/

    //ma_sleep(1000);
    //ma_sound_set_looping(&sound2, MA_TRUE);
    //ma_sound_set_volume(&sound2, 0.5f);
    //ma_sound_start(&sound2);

    //ma_sleep(2000);
    //printf("Stopping...\n");
    //ma_sound_stop(&sound);
    //ma_sound_group_stop(ma_engine_get_master_sound_group(&engine));
#endif

#if 1
    /*ma_engine_play_sound(&engine, argv[1], NULL);*/
    /*ma_engine_play_sound(&engine, argv[2], NULL);
    ma_engine_play_sound(&engine, argv[3], NULL);*/
#endif

#if 0
    for (;;) {
        ma_resource_manager_process_next_job(&resourceManager);
        ma_sleep(5);
    }
#endif

#if 1
    float maxX = +10;
    float minX = -10;
    float posX = 0;
    float posZ = -1.0f;
    float step = 0.1f;
    float stepAngle = 0.02f;
    float angle = 0;

    float pitch     = 1;
    float pitchStep = 0.01f;
    float pitchMin  = 0.125f;
    float pitchMax  = 2;
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

        //ma_sound_group_set_pitch(ma_engine_get_master_sound_group(&engine), pitch);
        //ma_sound_set_pitch(&sound, pitch);
        //ma_sound_set_volume(&sound, pitch);
        //ma_sound_set_pan(&sound, pitch);

        //printf("Pitch: %f\n", pitch);


        posX += step;
        if (posX > maxX) {
            posX = maxX;
            step = -step;
        } else if (posX < minX) {
            posX = minX;
            step = -step;
        }

        //ma_spatializer_set_position(&g_spatializer, ma_vec3f_init_3f(posX, 0, posZ));
        ma_sound_set_position(&sound, 0, 0, -2);
        ma_engine_listener_set_position(&engine, 0, 0, 0, -20);
        ma_engine_listener_set_direction(&engine, 0, -1, 0, 0);
        //ma_sound_set_velocity(&sound, step*1000, 0, 0);

        ma_engine_listener_set_direction(&engine, 0, (float)ma_cosd(angle), 0, (float)ma_sind(angle));
        angle += stepAngle;

        ma_sleep(1);
    }
#endif

    printf("Press Enter to quit...");
    getchar();

    ma_sound_uninit(&sound);
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
