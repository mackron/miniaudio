/* This test is only basic right now. Will be expanded on later. */
#include "../common/common.c"
#include "../../extras/decoders/libvorbis/miniaudio_libvorbis.c"
#include "../../extras/decoders/libopus/miniaudio_libopus.c"
#include "../../extras/vfs/debugging/miniaudio_vfs_debugging.c"

#include <vector>

static ma_thread_result MA_THREADCALL delete_sound(void* pUserData)
{
    ma_sound* pSound = (ma_sound*)pUserData;

    ma_sound_uninit(pSound);
    ma_free(pSound, NULL);

    return 0;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_default_vfs vfsDefault;
    ma_vfs_debugging_config vfsDebuggingConfig;
    ma_vfs_debugging vfsDebugging;
    ma_resource_manager_config resourceManagerConfig;
    ma_resource_manager resourceManager;
    ma_engine_config engineConfig;
    ma_engine engine;
    const char* pFilePaths[] = {
        "data/16-44100-stereo.flac",
        "data/48000-stereo.ogg",
        "data/48000-stereo.opus"
    };
    std::vector<ma_sound*> sounds;


    ma_decoding_backend_vtable* pDecodingBackendVTables[] =
    {
        ma_decoding_backend_libvorbis,
        ma_decoding_backend_libopus,
        ma_decoding_backend_wav,
        ma_decoding_backend_flac,
        ma_decoding_backend_mp3
    };


    /* Need a default VFS to act as the underlying VFS for debugging. */
    ma_default_vfs_init(&vfsDefault, NULL);

    vfsDebuggingConfig = ma_vfs_debugging_config_init(&vfsDefault, 10);
    ma_vfs_debugging_init(&vfsDebuggingConfig, &vfsDebugging);

    
    resourceManagerConfig = ma_resource_manager_config_init();
    resourceManagerConfig.pVFS                     = &vfsDebugging;
    resourceManagerConfig.ppDecodingBackendVTables = pDecodingBackendVTables;
    resourceManagerConfig.decodingBackendCount     = ma_countof(pDecodingBackendVTables);

    result = ma_resource_manager_init(&resourceManagerConfig, &resourceManager);
    if (result != MA_SUCCESS) {
        return result;
    }


    engineConfig = ma_engine_config_init();
    engineConfig.pResourceManager = &resourceManager;

    result = ma_engine_init(&engineConfig, &engine);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* Sometimes I don't want output to come through the speakers because it'll sound obnoxious. */
    ma_engine_set_volume(&engine, 1.0f);

    ma_engine_start(&engine);

    /* Rapidly create and delete sounds. */
    {
        ma_sound* pSound = NULL;
        ma_uint32 soundCount = 10;

        for (ma_uint32 i = 0; i < soundCount; i += 1) {
            pSound = new ma_sound;

            result = ma_sound_init_from_file(&engine, pFilePaths[i % ma_countof(pFilePaths)], MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC, NULL, NULL, pSound);
            if (result == MA_SUCCESS) {
                //ma_sound_set_position(pSound, 1, 0, 0);
                ma_sound_set_volume(pSound, 0.1f);
                ma_sound_start(pSound);
                
                /* Delete every second sound to test for early termination while the sound is still loading. */
                if ((i % 2) != 0) {
                    //ma_sleep(1);
                    #if 1
                    {
                        ma_thread thread;
                        ma_thread_create(&thread, ma_thread_priority_normal, 0, delete_sound, pSound, NULL);
                    }
                    #else
                    {
                        delete_sound(pSound);
                    }
                    #endif

                    continue;
                }
    
                sounds.push_back(pSound);
            } else {
                delete pSound;
            }
        }

        #ifndef __EMSCRIPTEN__
        {
            ma_sleep(20000);
        }
        #endif

        for (size_t i = 0; i < sounds.size(); i += 1) {
            ma_sound_uninit(sounds[i]);
            ma_free(sounds[i], &engine.allocationCallbacks);
        }
        sounds.clear();
    }

    ma_engine_uninit(&engine);

    (void)argc;
    (void)argv;

    return 0;
}