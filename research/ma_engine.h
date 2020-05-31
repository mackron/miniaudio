/*
EXPERIMENTAL
============
Everything in this file is experimental and subject to change. Some stuff isn't yet implemented, in particular spatialization. I've noted some ideas that are
basically straight off the top of my head - many of these are probably outright wrong or just generally bad ideas.

Very simple APIs for spatialization are declared by not yet implemented. They're just placeholders to give myself an idea on some of the API design. The
caching system outlined in the resource manager are just ideas off the top of my head. This will almost certainly change. The resource manager currently just
naively allocates ma_decoder objects on the heap and streams them from the disk. No caching or background loading is going on here - I just want to get some
API ideas written and a prototype up and and running.

The idea is that you have an `ma_engine` object - one per listener. Decoupled from that is the `ma_resource_manager` object. You can have one `ma_resource_manager`
object to many `ma_engine` objects. This will allow you to share resources for each listener. The `ma_engine` is responsible for the playback of audio from a
list of data sources. The `ma_resource_manager` is responsible for the actual loading, caching and unloading of those data sources. This decoupling is
something that I'm really liking right now and will likely stay in place for the final version.

You create "sounds" from the engine which represent a sound/voice in the world. You first need to create a sound, and then you need to start it. Sounds do not
start by default. You can use `ma_engine_play_sound()` to "fire and forget" sounds. Sounds can have an effect (`ma_effect`) applied to it which can be set with
`ma_engine_sound_set_effect()`.

Sounds can be allocated to groups called `ma_sound_group`. The creation and deletion of groups is not thread safe and should usually happen at initialization
time. Groups are how you handle submixing. In many games you will see settings to control the master volume in addition to groups, usually called SFX, Music
and Voices. The `ma_sound_group` object is how you would achieve this via the `ma_engine` API. When a sound is created you need to specify the group it should
be associated with. The sound's group cannot be changed after it has been created.

The creation and deletion of sounds should, hopefully, be thread safe. I have not yet done thorough testing on this, so there's a good chance there may be some
subtle bugs there.


Some things haven't yet been fully decided on. The following things in particular are some of the things I'm considering. If you have any opinions, feel free
to send me a message and give me your opinions/advice:

    - You need to explicitly start playback with `ma_engine_start()`. I'm considering making the default behaviour cause it to auto-start when the first sound
      is started. The question then is do we automatically stop it when the last sound is stopped? If so, would we still auto-stop it if the user explicitly
      called `ma_engine_start()`?
    - I haven't yet got spatialization working. I'm expecting it may be required to use an acceleration structure for querying audible sounds and only mixing
      those which can be heard by the listener, but then that will cause problems in the mixing thread because that should, ideally, not have any locking.
    - No caching or background loading is implemented in the resource manager. This is planned.
    - Sound groups can have an effect applied to them before being mixed with the parent group, but I'm considering making it so the effect is not allowed to
      have resampling enabled thereby simplifying memory management between parent and child groups.


The best resource to use when understanding the API is the function declarations for `ma_engine`. I expect you should be able to figure it out! :)
*/

/*
Resource Management
===================
Resources are managed via the `ma_resource_manager` API.

At it's core, the resource manager is responsible for the loading and caching of audio data. There are two types of audio data: encoded and decoded. Encoded
audio data is the raw contents of an audio file on disk. Decoded audio data is raw, uncompressed PCM audio data. Both encoded and decoded audio data are
associated with a name for purpose of instancing. The idea is that you have one chunk of encoded or decoded audio data to many `ma_data_source` objects. In
this case, the `ma_data_source` object is the instance.

There are three levels of storage, in order of speed:

    1) Decoded/Uncompressed Cache
    2) Encoded/Compressed Cache
    3) Disk (accessed via a VFS)

Whenever a sound is played, it should usually be loaded into one of the in-memory caches (level 1 or 2). 
*/
#ifndef miniaudio_engine_h
#define miniaudio_engine_h

#include "ma_mixing.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    ma_uint64 sizeInFrames;
    void* pData;
} ma_decoded_data;

typedef struct
{
    size_t sizeInBytes;
    void* pData;
} ma_encoded_data;

typedef struct
{
    size_t level1CacheSizeInBytes;  /* Set to 0 to disable level 1 cache. Set to (size_t)-1 for unlimited. */
    size_t level2CacheSizeInBytes;  /* Set to 0 to disable level 2 cache. Set to (size_t)-1 for unlimited. */
    ma_uint32 level1CacheFlags;
    ma_uint32 level2CacheFlags;
    ma_allocation_callbacks level1CacheAllocationCallbacks;
    ma_allocation_callbacks level2CacheAllocationCallbacks;
    ma_allocation_callbacks allocationCallbacks;    /* Generic allocations. */
    ma_format decodedFormat;
    ma_uint32 decodedChannels;
    ma_uint32 decodedSampleRate;
} ma_resource_manager_config;

MA_API ma_resource_manager_config ma_resource_manager_config_init(ma_format decodedFormat, ma_uint32 decodedChannels, ma_uint32 decodedSampleRate, const ma_allocation_callbacks* pAllocationCallbacks);

typedef struct
{
    ma_resource_manager_config config;
} ma_resource_manager;

MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager);
MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager);
#if 0
MA_API ma_result ma_resource_manager_cache_decoded_data(ma_resource_manager* pResourceManager, const char* pName, ma_uint64 frameCount, const void* pData);
MA_API ma_result ma_resource_manager_eject_decoded_data(ma_resource_manager* pResourceManager, const char* pName);
MA_API ma_result ma_resource_manager_cache_encoded_data(ma_resource_manager* pResourceManager, const char* pName, size_t sizeInBytes, const void* pData);
MA_API ma_result ma_resource_manager_eject_encoded_data(ma_resource_manager* pResourceManager, const char* pName);
#endif
MA_API ma_result ma_resource_manager_create_data_source(ma_resource_manager* pResourceManager, const char* pName, ma_data_source** ppDataSource);
MA_API ma_result ma_resource_manager_delete_data_source(ma_resource_manager* pResourceManager, ma_data_source* pDataSource);



/*
Engine
======
The `ma_engine` API is a high-level API for audio playback. Internally it contains a world of sounds (`ma_sound`) with resources managed via a resource manager
(`ma_resource_manager`).

Within the world there is the concept of a "listener". Each `ma_engine` instances has a single listener, but you can instantiate multiple `ma_engine` instances
if you need more than one listener. In this case you will want to share a resource manager which you can do by initializing one manually and passing it into
`ma_engine_config`. Using this method will require your application to manage groups and sounds on a per `ma_engine` basis.
*/
typedef struct ma_engine      ma_engine;
typedef struct ma_sound       ma_sound;
typedef struct ma_sound_group ma_sound_group;
typedef struct ma_listener    ma_listener;

typedef struct
{
    float x;
    float y;
    float z;
} ma_vec3;

static MA_INLINE ma_vec3 ma_vec3f(float x, float y, float z)
{
    ma_vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}


typedef struct
{
    float x;
    float y;
    float z;
    float w;
} ma_quat;

static MA_INLINE ma_quat ma_quatf(float x, float y, float z, float w)
{
    ma_quat q;
    q.x = x;
    q.y = y;
    q.z = z;
    q.w = w;
    return q;
}


/* Stereo panner. */
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    float pan;
} ma_panner_config;

MA_API ma_panner_config ma_panner_config_init(ma_format format, ma_uint32 channels);


typedef struct
{
    ma_effect_base effect;
    ma_format format;
    ma_uint32 channels;
    float pan;  /* -1..1 where 0 is no pan, -1 is left side, +1 is right side. Defaults to 0. */
} ma_panner;

MA_API ma_result ma_panner_init(const ma_panner_config* pConfig, ma_panner* pPanner);
MA_API ma_result ma_panner_process_pcm_frames(ma_panner* pPanner, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_result ma_panner_set_pan(ma_panner* pPanner, float pan);



/* Spatializer. */
typedef struct
{
    ma_engine* pEngine;
    ma_format format;
    ma_uint32 channels;
    ma_vec3 position;
    ma_quat rotation;
} ma_spatializer_config;

MA_API ma_spatializer_config ma_spatializer_config_init(ma_engine* pEngine, ma_format format, ma_uint32 channels);


typedef struct
{
    ma_effect_base effect;
    ma_engine* pEngine;             /* For accessing global, per-engine data such as the listener position and environmental information. */
    ma_format format;
    ma_uint32 channels;
    ma_vec3 position;
    ma_quat rotation;
} ma_spatializer;

MA_API ma_result ma_spatializer_init(const ma_spatializer_config* pConfig, ma_spatializer* pSpatializer);
MA_API ma_result ma_spatializer_process_pcm_frames(ma_spatializer* pSpatializer, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_result ma_spatializer_set_position(ma_spatializer* pSpatializer, ma_vec3 position);
MA_API ma_result ma_spatializer_set_rotation(ma_spatializer* pSpatializer, ma_quat rotation);



/* All of the proprties supported by the engine are handled via an effect. */
typedef struct
{
    ma_effect_base baseEffect;
    ma_engine* pEngine;             /* For accessing global, per-engine data such as the listener position and environmental information. */
    ma_effect* pPreEffect;          /* The application-defined effect that will be applied before spationalization, etc. */
    ma_panner panner;
    ma_spatializer spatializer;
    float pitch;
    float oldPitch;                 /* For determining whether or not the resampler needs to be updated to reflect the new pitch. The resampler will be updated on the mixing thread. */
    ma_data_converter converter;    /* For pitch shift. May change this to just a resampler later. */
    ma_bool32 isSpatial;            /* Set the false by default. When set to false, with not have spatialisation applied. */
} ma_engine_effect;

struct ma_sound
{
    ma_data_source* pDataSource;
    ma_sound_group* pGroup;         /* The group the sound is attached to. */
    ma_sound* pPrevSoundInGroup;
    ma_sound* pNextSoundInGroup;
    ma_engine_effect effect;        /* The effect containing all of the information for spatialization, pitching, etc. */
    float volume;
    ma_bool32 isPlaying;            /* False by default. Sounds need to be explicitly started with ma_engine_sound_start() and stopped with ma_engine_sound_stop(). */
    ma_bool32 isMixing;
    ma_bool32 atEnd;
    ma_bool32 isLooping;            /* False by default. */
    ma_bool32 ownsDataSource;
    ma_bool32 _isInternal;          /* A marker to indicate the sound is managed entirely by the engine. This will be set to true when the sound is created internally by ma_engine_play_sound(). */
};

struct ma_sound_group
{
    ma_sound_group* pParent;
    ma_sound_group* pFirstChild;
    ma_sound_group* pPrevSibling;
    ma_sound_group* pNextSibling;
    ma_sound* pFirstSoundInGroup;  /* Marked as volatile because we need to be very explicit with when we make copies of this and we can't have the compiler optimize it out. */
    ma_mixer mixer;
    ma_mutex lock;          /* Only used by ma_engine_create_sound_*() and ma_engine_delete_sound(). Not used in the mixing thread. */
    ma_bool32 isPlaying;    /* True by default. Sound groups can be stopped with ma_engine_sound_stop() and resumed with ma_engine_sound_start(). Also affects children. */
};

struct ma_listener
{
    ma_device device;   /* The playback device associated with this listener. */
    ma_pcm_rb fixedRB;  /* The intermediary ring buffer for helping with fixed sized updates. */
    ma_vec3 position;
    ma_quat rotation;
};

typedef struct
{
    ma_resource_manager* pResourceManager;  /* Can be null in which case a resource manager will be created for you. */
    ma_format format;                       /* The format to use when mixing and spatializing. When set to 0 will use the native format of the device. */
    ma_uint32 channels;                     /* The number of channels to use when mixing and spatializing. When set to 0, will use the native channel count of the device. */
    ma_uint32 sampleRate;                   /* The sample rate. When set to 0 will use the native channel count of the device. */
    ma_uint32 periodSizeInFrames;           /* If set to something other than 0, updates will always be exactly this size. The underlying device may be a different size, but from the perspective of the mixer that won't matter.*/
    ma_uint32 periodSizeInMilliseconds;     /* Used if periodSizeInFrames is unset. */
    ma_device_id* pPlaybackDeviceID;        /* The ID of the playback device to use with the default listener. */
    ma_allocation_callbacks allocationCallbacks;
    ma_bool32 noAutoStart;                  /* When set to true, requires an explicit call to ma_engine_start(). This is false by default, meaning the engine will be started automatically in ma_engine_init(). */
} ma_engine_config;

MA_API ma_engine_config ma_engine_config_init_default();


struct ma_engine
{
    ma_resource_manager* pResourceManager;
    ma_context context;
    ma_listener listener;
    ma_sound_group masterSoundGroup;        /* Sounds are associated with this group by default. */
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 periodSizeInFrames;
    ma_uint32 periodSizeInMilliseconds;
    ma_allocation_callbacks allocationCallbacks;
    ma_bool32 ownsResourceManager : 1;
};

MA_API ma_result ma_engine_init(const ma_engine_config* pConfig, ma_engine* pEngine);
MA_API void ma_engine_uninit(ma_engine* pEngine);

MA_API ma_result ma_engine_start(ma_engine* pEngine);
MA_API ma_result ma_engine_stop(ma_engine* pEngine);
MA_API ma_result ma_engine_set_volume(ma_engine* pEngine, float volume);
MA_API ma_result me_engine_set_gain_db(ma_engine* pEngine, float gainDB);

#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_engine_create_sound_from_file(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup, ma_sound* pSound);
#endif
MA_API ma_result ma_engine_create_sound_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_sound_group* pGroup, ma_sound* pSound);
MA_API void ma_engine_delete_sound(ma_engine* pEngine, ma_sound* pSound);
MA_API ma_result ma_engine_sound_start(ma_engine* pEngine, ma_sound* pSound);
MA_API ma_result ma_engine_sound_stop(ma_engine* pEngine, ma_sound* pSound);
MA_API ma_result ma_engine_sound_set_volume(ma_engine* pEngine, ma_sound* pSound, float volume);
MA_API ma_result me_engine_sound_set_gain_db(ma_engine* pEngine, ma_sound* pSound, float gainDB);
MA_API ma_result ma_engine_sound_set_pitch(ma_engine* pEngine, ma_sound* pSound, float pitch);
MA_API ma_result ma_engine_sound_set_effect(ma_engine* pEngine, ma_sound* pSound, ma_effect* pEffect);
MA_API ma_result ma_engine_sound_set_position(ma_engine* pEngine, ma_sound* pSound, ma_vec3 position);
MA_API ma_result ma_engine_sound_set_rotation(ma_engine* pEngine, ma_sound* pSound, ma_quat rotation);
MA_API ma_result ma_engine_sound_set_looping(ma_engine* pEngine, ma_sound* pSound, ma_bool32 isLooping);
MA_API ma_bool32 ma_engine_sound_at_end(ma_engine* pEngine, const ma_sound* pSound);
MA_API ma_result ma_engine_play_sound(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup);   /* Fire and forget. */

MA_API ma_result ma_engine_sound_group_init(ma_engine* pEngine, ma_sound_group* pParentGroup, ma_sound_group* pGroup);  /* Parent must be set at initialization time and cannot be changed. Not thread-safe. */
MA_API void ma_engine_sound_group_uninit(ma_engine* pEngine, ma_sound_group* pGroup);   /* Not thread-safe. */
MA_API ma_result ma_engine_sound_group_start(ma_engine* pEngine, ma_sound_group* pGroup);
MA_API ma_result ma_engine_sound_group_stop(ma_engine* pEngine, ma_sound_group* pGroup);
MA_API ma_result ma_engine_sound_group_set_volume(ma_engine* pEngine, ma_sound_group* pGroup, float volume);
MA_API ma_result ma_engine_sound_group_set_gain_db(ma_engine* pEngine, ma_sound_group* pGroup, float gainDB);
MA_API ma_result ma_engine_sound_group_set_effect(ma_engine* pEngine, ma_sound_group* pGroup, ma_effect* pEffect);

MA_API ma_result ma_engine_listener_set_position(ma_engine* pEngine, ma_vec3 position);
MA_API ma_result ma_engine_listener_set_rotation(ma_engine* pEngine, ma_quat rotation);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_engine_h */


#if defined(MA_IMPLEMENTATION) || defined(MINIAUDIO_IMPLEMENTATION)

MA_API ma_resource_manager_config ma_resource_manager_config_init(ma_format decodedFormat, ma_uint32 decodedChannels, ma_uint32 decodedSampleRate, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_resource_manager_config config;

    MA_ZERO_OBJECT(&config);
    config.decodedFormat     = decodedFormat;
    config.decodedChannels   = decodedChannels;
    config.decodedSampleRate = decodedSampleRate;
    
    if (pAllocationCallbacks != NULL) {
        config.level1CacheAllocationCallbacks = *pAllocationCallbacks;
        config.level2CacheAllocationCallbacks = *pAllocationCallbacks;
    }

    return config;
}


MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager)
{
    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pResourceManager);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pResourceManager->config = *pConfig;
    ma_allocation_callbacks_init_copy(&pResourceManager->config.level1CacheAllocationCallbacks, &pConfig->level1CacheAllocationCallbacks);
    ma_allocation_callbacks_init_copy(&pResourceManager->config.level2CacheAllocationCallbacks, &pConfig->level2CacheAllocationCallbacks);
    ma_allocation_callbacks_init_copy(&pResourceManager->config.allocationCallbacks,            &pConfig->allocationCallbacks);

    return MA_SUCCESS;
}

MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager)
{
    if (pResourceManager == NULL) {
        return;
    }

    
}

MA_API ma_result ma_resource_manager_create_data_source(ma_resource_manager* pResourceManager, const char* pName, ma_data_source** ppDataSource)
{
    ma_result result;
    ma_decoder* pDecoder;
    ma_decoder_config decoderConfig;

    if (ppDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    *ppDataSource = NULL;

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    /* For testing and prototyping we're just allocating a decoder on the heap. Later on this will be a custom resource manager specific data source. */
    pDecoder = ma_malloc(sizeof(*pDecoder), NULL);
    if (pDecoder == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    decoderConfig = ma_decoder_config_init(pResourceManager->config.decodedFormat, pResourceManager->config.decodedChannels, pResourceManager->config.decodedSampleRate);
    result = ma_decoder_init_file(pName, &decoderConfig, pDecoder);
    if (result != MA_SUCCESS) {
        ma_free(pDecoder, NULL);
        return result;
    }

    *ppDataSource = (ma_data_source*)pDecoder;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_delete_data_source(ma_resource_manager* pResourceManager, ma_data_source* pDataSource)
{
    ma_decoder* pDecoder;

    if (pResourceManager == NULL || pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Everything is a ma_decoder while we're prototyping. */
    pDecoder = (ma_decoder*)pDataSource;
    ma_decoder_uninit(pDecoder);

    ma_free(pDecoder, NULL);

    return MA_SUCCESS;
}




MA_API ma_panner_config ma_panner_config_init(ma_format format, ma_uint32 channels)
{
    ma_panner_config config;

    MA_ZERO_OBJECT(&config);
    config.format   = format;
    config.channels = channels;
    config.pan      = 0;

    return config;
}


static ma_result ma_panner_effect__on_process_pcm_frames(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_panner* pPanner = (ma_panner*)pEffect;

    /* The panner has a 1:1 relationship between input and output frame counts. */
    return ma_panner_process_pcm_frames(pPanner, pFramesOut, pFramesIn, ma_min(*pFrameCountIn, *pFrameCountOut));
}

static ma_result ma_panner_effect__on_get_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_panner* pPanner = (ma_panner*)pEffect;

    *pFormat     = pPanner->format;
    *pChannels   = pPanner->channels;
    *pSampleRate = 0;   /* There's no notion of sample rate with this effect. */

    return MA_SUCCESS;
}

MA_API ma_result ma_panner_init(const ma_panner_config* pConfig, ma_panner* pPanner)
{
    if (pPanner == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pPanner);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pPanner->effect.onProcessPCMFrames            = ma_panner_effect__on_process_pcm_frames;
    pPanner->effect.onGetRequiredInputFrameCount  = NULL;
    pPanner->effect.onGetExpectedOutputFrameCount = NULL;
    pPanner->effect.onGetInputDataFormat          = ma_panner_effect__on_get_data_format;   /* Same format for both input and output. */
    pPanner->effect.onGetOutputDataFormat         = ma_panner_effect__on_get_data_format;

    pPanner->format   = pConfig->format;
    pPanner->channels = pConfig->channels;
    pPanner->pan      = pConfig->pan;

    return MA_SUCCESS;
}

MA_API ma_result ma_panner_process_pcm_frames(ma_panner* pPanner, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    if (pPanner == NULL || pFramesOut == NULL || pFramesIn) {
        return MA_INVALID_ARGS;
    }

    /* TODO: Implement me. Just copying for now. */
    ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, pPanner->format, pPanner->channels);

    return MA_SUCCESS;
}

MA_API ma_result ma_panner_set_pan(ma_panner* pPanner, float pan)
{
    if (pPanner == NULL) {
        return MA_INVALID_ARGS;
    }

    pPanner->pan = ma_clamp(pan, -1.0f, 1.0f);

    return MA_SUCCESS;
}




MA_API ma_spatializer_config ma_spatializer_config_init(ma_engine* pEngine, ma_format format, ma_uint32 channels)
{
    ma_spatializer_config config;

    MA_ZERO_OBJECT(&config);

    config.pEngine  = pEngine;
    config.format   = format;
    config.channels = channels;
    config.position = ma_vec3f(0, 0, 0);
    config.rotation = ma_quatf(0, 0, 0, 1);

    return config;
}


static ma_result ma_spatializer_effect__on_process_pcm_frames(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_spatializer* pSpatializer = (ma_spatializer*)pEffect;

    /* The panner has a 1:1 relationship between input and output frame counts. */
    return ma_spatializer_process_pcm_frames(pSpatializer, pFramesOut, pFramesIn, ma_min(*pFrameCountIn, *pFrameCountOut));
}

static ma_result ma_spatializer_effect__on_get_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_spatializer* pSpatializer = (ma_spatializer*)pEffect;

    *pFormat     = pSpatializer->format;
    *pChannels   = pSpatializer->channels;
    *pSampleRate = 0;   /* There's no notion of sample rate with this effect. */

    return MA_SUCCESS;
}

MA_API ma_result ma_spatializer_init(const ma_spatializer_config* pConfig, ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pSpatializer);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pSpatializer->effect.onProcessPCMFrames            = ma_spatializer_effect__on_process_pcm_frames;
    pSpatializer->effect.onGetRequiredInputFrameCount  = NULL;
    pSpatializer->effect.onGetExpectedOutputFrameCount = NULL;
    pSpatializer->effect.onGetInputDataFormat          = ma_spatializer_effect__on_get_data_format;  /* Same format for both input and output. */
    pSpatializer->effect.onGetOutputDataFormat         = ma_spatializer_effect__on_get_data_format;

    pSpatializer->pEngine  = pConfig->pEngine;
    pSpatializer->format   = pConfig->format;
    pSpatializer->channels = pConfig->channels;
    pSpatializer->position = pConfig->position;
    pSpatializer->rotation = pConfig->rotation;

    return MA_SUCCESS;
}

MA_API ma_result ma_spatializer_process_pcm_frames(ma_spatializer* pSpatializer, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    if (pSpatializer || pFramesOut == NULL || pFramesIn) {
        return MA_INVALID_ARGS;
    }

    /* TODO: Implement me. Just copying for now. */
    ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, pSpatializer->format, pSpatializer->channels);

    return MA_SUCCESS;
}

MA_API ma_result ma_spatializer_set_position(ma_spatializer* pSpatializer, ma_vec3 position)
{
    if (pSpatializer == NULL) {
        return MA_INVALID_ARGS;
    }

    pSpatializer->position = position;

    return MA_SUCCESS;
}

MA_API ma_result ma_spatializer_set_rotation(ma_spatializer* pSpatializer, ma_quat rotation)
{
    if (pSpatializer == NULL) {
        return MA_INVALID_ARGS;
    }

    pSpatializer->rotation = rotation;

    return MA_SUCCESS;
}



/**************************************************************************************************************************************************************

Engine

**************************************************************************************************************************************************************/
MA_API ma_engine_config ma_engine_config_init_default()
{
    ma_engine_config config;
    MA_ZERO_OBJECT(&config);

    config.format = ma_format_f32;

    return config;
}


static void ma_engine_sound_mix_wait(ma_sound* pSound)
{
    /* This function is only safe when the sound is not flagged as playing. */
    MA_ASSERT(pSound->isPlaying == MA_FALSE);

    /* Just do a basic spin wait. */
    while (pSound->isMixing) {
        continue;   /* Do nothing - just keep waiting for the mixer thread. */
    }
}


static void ma_engine_mix_sound(ma_engine* pEngine, ma_sound_group* pGroup, ma_sound* pSound, ma_uint32 frameCount)
{
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pGroup  != NULL);
    MA_ASSERT(pSound  != NULL);

    ma_atomic_exchange_32(&pSound->isMixing, MA_TRUE);  /* This must be done before checking the isPlaying state. */
    {
        if (pSound->isPlaying) {
            ma_result result = MA_SUCCESS;

            /* If the pitch has changed we need to update the resampler. */
            if (pSound->effect.oldPitch != pSound->effect.pitch) {
                pSound->effect.oldPitch  = pSound->effect.pitch;
                ma_data_converter_set_rate_ratio(&pSound->effect.converter, pSound->effect.pitch);
            }

            /*
            If the sound is muted we still need to move time forward, but we can save time by not mixing as it won't actually affect anything. If there's an
            effect we need to make sure we run it through the mixer because it may require us to update internal state for things like echo effects.
            */
            if (pSound->volume > 0 || pSound->effect.pPreEffect != NULL || pSound->effect.pitch != 1) {
                result = ma_mixer_mix_data_source(&pGroup->mixer, pSound->pDataSource, frameCount, pSound->volume, &pSound->effect, pSound->isLooping);
            } else {
                /* The sound is muted. We want to move time forward, but it be made faster by simply seeking instead of reading. We also want to bypass mixing completely. */
                ma_uint64 framesSeeked = ma_data_source_seek_pcm_frames(pSound->pDataSource, frameCount, pSound->isLooping);
                if (framesSeeked < frameCount) {
                    result = MA_AT_END;
                }
            }

            /* If we reached the end of the sound we'll want to mark it as at the end and not playing. */
            if (result == MA_AT_END) {
                ma_atomic_exchange_32(&pSound->isPlaying, MA_FALSE);
                ma_atomic_exchange_32(&pSound->atEnd, MA_TRUE);         /* Set to false in ma_engine_sound_start(). */
            }
        }
    }
    ma_atomic_exchange_32(&pSound->isMixing, MA_FALSE);
}

static void ma_engine_mix_sound_group(ma_engine* pEngine, ma_sound_group* pGroup, void* pFramesOut, ma_uint32 frameCount)
{
    ma_result result;
    ma_mixer* pParentMixer = NULL;
    ma_uint64 frameCountOut;
    ma_uint64 frameCountIn;
    ma_sound_group* pNextChildGroup;
    ma_sound* pNextSound;

    MA_ASSERT(pEngine    != NULL);
    MA_ASSERT(pGroup     != NULL);
    MA_ASSERT(frameCount != 0);

    /* Don't do anything if we're not playing. */
    if (pGroup->isPlaying == MA_FALSE) {
        return;
    }

    if (pGroup->pParent != NULL) {
        pParentMixer = &pGroup->pParent->mixer;
    }

    frameCountOut = frameCount;
    frameCountIn  = frameCount;

    /* Before can mix the group we need to mix it's children. */
    result = ma_mixer_begin(&pGroup->mixer, pParentMixer, &frameCountOut, &frameCountIn);
    if (result != MA_SUCCESS) {
        return;
    }

    MA_ASSERT(frameCountIn < 0xFFFFFFFF);

    /* Child groups need to be mixed based on the parent's input frame count. */
    for (pNextChildGroup = pGroup->pFirstChild; pNextChildGroup != NULL; pNextChildGroup = pNextChildGroup->pNextSibling) {
        ma_engine_mix_sound_group(pEngine, pNextChildGroup, NULL, (ma_uint32)frameCountIn); /* Safe cast. */
    }

    /* Sounds in the group can now be mixed. This is where the real mixing work is done. */
    for (pNextSound = pGroup->pFirstSoundInGroup; pNextSound != NULL; pNextSound = pNextSound->pNextSoundInGroup) {
        ma_engine_mix_sound(pEngine, pGroup, pNextSound, (ma_uint32)frameCountIn);          /* Safe cast. */
    }

    /* Now mix into the parent. */
    result = ma_mixer_end(&pGroup->mixer, pParentMixer, pFramesOut);
    if (result != MA_SUCCESS) {
        return;
    }
}

static void ma_engine_listener__data_callback_fixed(ma_engine* pEngine, void* pFramesOut, ma_uint32 frameCount)
{
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pEngine->periodSizeInFrames == frameCount);   /* This must always be true. */

    /* Recursively mix the sound groups. */
    ma_engine_mix_sound_group(pEngine, &pEngine->masterSoundGroup, pFramesOut, frameCount);
}

static void ma_engine_listener__data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    ma_uint32 pcmFramesAvailableInRB;
    ma_uint32 pcmFramesProcessed = 0;
    ma_uint8* pRunningOutput = (ma_uint8*)pFramesOut;
    ma_engine* pEngine = (ma_engine*)pDevice->pUserData;
    MA_ASSERT(pEngine != NULL);

    /* We need to do updates in fixed sizes based on the engine's period size in frames. */

    /*
    The first thing to do is check if there's enough data available in the ring buffer. If so we can read from it. Otherwise we need to keep filling
    the ring buffer until there's enough, making sure we only fill the ring buffer in chunks of pEngine->periodSizeInFrames.
    */
    while (pcmFramesProcessed < frameCount) {    /* Keep going until we've filled the output buffer. */
        ma_uint32 framesRemaining = frameCount - pcmFramesProcessed;

        pcmFramesAvailableInRB = ma_pcm_rb_available_read(&pEngine->listener.fixedRB);
        if (pcmFramesAvailableInRB > 0) {
            ma_uint32 framesToRead = (framesRemaining < pcmFramesAvailableInRB) ? framesRemaining : pcmFramesAvailableInRB;
            void* pReadBuffer;

            ma_pcm_rb_acquire_read(&pEngine->listener.fixedRB, &framesToRead, &pReadBuffer);
            {
                memcpy(pRunningOutput, pReadBuffer, framesToRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
            }
            ma_pcm_rb_commit_read(&pEngine->listener.fixedRB, framesToRead, pReadBuffer);

            pRunningOutput += framesToRead * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels);
            pcmFramesProcessed += framesToRead;
        } else {
            /*
            There's nothing in the buffer. Fill it with more data from the callback. We reset the buffer first so that the read and write pointers
            are reset back to the start so we can fill the ring buffer in chunks of pEngine->periodSizeInFrames which is what we initialized it
            with. Note that this is not how you would want to do it in a multi-threaded environment. In this case you would want to seek the write
            pointer forward via the producer thread and the read pointer forward via the consumer thread (this thread).
            */
            ma_uint32 framesToWrite = pEngine->periodSizeInFrames;
            void* pWriteBuffer;

            ma_pcm_rb_reset(&pEngine->listener.fixedRB);
            ma_pcm_rb_acquire_write(&pEngine->listener.fixedRB, &framesToWrite, &pWriteBuffer);
            {
                MA_ASSERT(framesToWrite == pEngine->periodSizeInFrames);   /* <-- This should always work in this example because we just reset the ring buffer. */
                ma_engine_listener__data_callback_fixed(pEngine, pWriteBuffer, framesToWrite);
            }
            ma_pcm_rb_commit_write(&pEngine->listener.fixedRB, framesToWrite, pWriteBuffer);
        }
    }

    (void)pFramesIn;
}


static ma_result ma_engine_listener_init(ma_engine* pEngine, const ma_device_id* pPlaybackDeviceID, ma_listener* pListener)
{
    ma_result result;
    ma_device_config deviceConfig;

    if (pListener == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pListener);

    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID       = pPlaybackDeviceID;
    deviceConfig.playback.format          = pEngine->format;
    deviceConfig.playback.channels        = pEngine->channels;
    deviceConfig.sampleRate               = pEngine->sampleRate;
    deviceConfig.dataCallback             = ma_engine_listener__data_callback;
    deviceConfig.pUserData                = pEngine;
    deviceConfig.periodSizeInFrames       = pEngine->periodSizeInFrames;
    deviceConfig.periodSizeInMilliseconds = pEngine->periodSizeInMilliseconds;
    deviceConfig.noPreZeroedOutputBuffer  = MA_TRUE;    /* We'll always be outputting to every frame in the callback so there's no need for a pre-silenced buffer. */
    deviceConfig.noClip                   = MA_TRUE;    /* The mixing engine here will do clipping for us. */

    result = ma_device_init(&pEngine->context, &deviceConfig, &pListener->device);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* With the device initialized we need an intermediary buffer for handling fixed sized updates. Currently using a ring buffer for this, but can probably use something a bit more optimal. */
    result = ma_pcm_rb_init(pListener->device.playback.format, pListener->device.playback.channels, pListener->device.playback.internalPeriodSizeInFrames, NULL, &pEngine->allocationCallbacks, &pListener->fixedRB);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_engine_listener_uninit(ma_engine* pEngine, ma_listener* pListener)
{
    if (pEngine == NULL || pListener == NULL) {
        return;
    }

    ma_device_uninit(&pListener->device);
}

MA_API ma_result ma_engine_init(const ma_engine_config* pConfig, ma_engine* pEngine)
{
    ma_result result;
    ma_engine_config engineConfig;
    ma_context_config contextConfig;

    /* The config is allowed to be NULL in which case we use defaults for everything. */
    if (pConfig != NULL) {
        engineConfig = *pConfig;
    } else {
        engineConfig = ma_engine_config_init_default();
    }

    /*
    For now we only support f32 but may add support for other formats later. To do this we need to add support for all formats to ma_panner and ma_spatializer (and any other future effects).
    */
    if (engineConfig.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* Format not supported. */
    }

    pEngine->pResourceManager         = engineConfig.pResourceManager;
    pEngine->format                   = engineConfig.format;
    pEngine->channels                 = engineConfig.channels;
    pEngine->sampleRate               = engineConfig.sampleRate;
    pEngine->periodSizeInFrames       = engineConfig.periodSizeInFrames;
    pEngine->periodSizeInMilliseconds = engineConfig.periodSizeInMilliseconds;
    ma_allocation_callbacks_init_copy(&pEngine->allocationCallbacks, &engineConfig.allocationCallbacks);


    /* We need a context before we'll be able to create the default listener. */
    contextConfig = ma_context_config_init();
    contextConfig.allocationCallbacks = pEngine->allocationCallbacks;

    result = ma_context_init(NULL, 0, &contextConfig, &pEngine->context);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize context. */
    }

    /* With the context create we can now create the default listener. After we have the listener we can set the format, channels and sample rate appropriately. */
    result = ma_engine_listener_init(pEngine, engineConfig.pPlaybackDeviceID, &pEngine->listener);
    if (result != MA_SUCCESS) {
        ma_context_uninit(&pEngine->context);
        return result;  /* Failed to initialize default listener. */
    }

    /* Now that have the default listener we can ensure we have the format, channels and sample rate set to proper values to ensure future listeners are configured consistently. */
    pEngine->format                   = pEngine->listener.device.playback.format;
    pEngine->channels                 = pEngine->listener.device.playback.channels;
    pEngine->sampleRate               = pEngine->listener.device.sampleRate;
    pEngine->periodSizeInFrames       = pEngine->listener.device.playback.internalPeriodSizeInFrames;
    pEngine->periodSizeInMilliseconds = (pEngine->periodSizeInFrames * pEngine->sampleRate) / 1000;


    /* We need a default sound group. This must be done after setting the format, channels and sample rate to their proper values. */
    result = ma_engine_sound_group_init(pEngine, NULL, &pEngine->masterSoundGroup);
    if (result != MA_SUCCESS) {
        ma_engine_listener_uninit(pEngine, &pEngine->listener);
        ma_context_uninit(&pEngine->context);
        return result;  /* Failed to initialize master sound group. */
    }


    /* We need a resource manager. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pEngine->pResourceManager == NULL) {
        ma_resource_manager_config resourceManagerConfig;

        pEngine->pResourceManager = (ma_resource_manager*)ma__malloc_from_callbacks(sizeof(*pEngine->pResourceManager), &pEngine->allocationCallbacks);
        if (pEngine->pResourceManager == NULL) {
            ma_engine_sound_group_uninit(pEngine, &pEngine->masterSoundGroup);
            ma_engine_listener_uninit(pEngine, &pEngine->listener);
            ma_context_uninit(&pEngine->context);
            return MA_OUT_OF_MEMORY;
        }

        resourceManagerConfig = ma_resource_manager_config_init(pEngine->format, pEngine->channels, pEngine->sampleRate, &pEngine->allocationCallbacks);
        result = ma_resource_manager_init(&resourceManagerConfig, pEngine->pResourceManager);
        if (result != MA_SUCCESS) {
            ma__free_from_callbacks(pEngine->pResourceManager, &pEngine->allocationCallbacks);
            ma_engine_sound_group_uninit(pEngine, &pEngine->masterSoundGroup);
            ma_engine_listener_uninit(pEngine, &pEngine->listener);
            ma_context_uninit(&pEngine->context);
            return result;
        }

        pEngine->ownsResourceManager = MA_TRUE;
    }
#endif

    /* Start the engine if required. This should always be the last step. */
    if (engineConfig.noAutoStart == MA_FALSE) {
        result = ma_engine_start(pEngine);
        if (result != MA_SUCCESS) {
            ma_engine_uninit(pEngine);
            return result;  /* Failed to start the engine. */
        }
    }

    return MA_SUCCESS;
}

MA_API void ma_engine_uninit(ma_engine* pEngine)
{
    if (pEngine == NULL) {
        return;
    }

    ma_engine_sound_group_uninit(pEngine, &pEngine->masterSoundGroup);
    ma_engine_listener_uninit(pEngine, &pEngine->listener);
    ma_context_uninit(&pEngine->context);

    /* Uninitialize the resource manager last to ensure we don't have a thread still trying to access it. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pEngine->ownsResourceManager) {
        ma_resource_manager_uninit(pEngine->pResourceManager);
        ma__free_from_callbacks(pEngine->pResourceManager, &pEngine->allocationCallbacks);
    }
#endif
}


MA_API ma_result ma_engine_start(ma_engine* pEngine)
{
    ma_result result;

    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_device_start(&pEngine->listener.device);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_stop(ma_engine* pEngine)
{
    ma_result result;

    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_device_stop(&pEngine->listener.device);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_set_volume(ma_engine* pEngine, float volume)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_device_set_master_volume(&pEngine->listener.device, volume);
}

MA_API ma_result me_engine_set_gain_db(ma_engine* pEngine, float gainDB)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_device_set_master_gain_db(&pEngine->listener.device, gainDB);
}


#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_engine_create_sound_from_file(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup, ma_sound* pSound)
{
    ma_result result;
    ma_data_source* pDataSource;

    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pSound);

    if (pEngine == NULL || pFilePath == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We need to user the resource manager to load the data source. */
    result = ma_resource_manager_create_data_source(pEngine->pResourceManager, pFilePath, &pDataSource);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* Now that we have our data source we can create the sound using our generic function. */
    result = ma_engine_create_sound_from_data_source(pEngine, pDataSource, pGroup, pSound);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* We need to tell the engine that we own the data source so that it knows to delete it when deleting the sound. This needs to be done after creating the sound with ma_engine_create_sound_from_data_source(). */
    pSound->ownsDataSource = MA_TRUE;

    return MA_SUCCESS;
}
#endif

static ma_result ma_engine_sound_detach(ma_engine* pEngine, ma_sound* pSound)
{
    ma_sound_group* pGroup;

    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pSound  != NULL);

    pGroup = pSound->pGroup;
    MA_ASSERT(pGroup != NULL);

    /*
    The sound should never be in a playing state when this is called. It *can*, however, but in the middle of mixing in the mixing thread. It needs to finish
    mixing before being uninitialized completely, but that is done at a higher level to this function.
    */
    MA_ASSERT(pSound->isPlaying == MA_FALSE);

    /*
    We want the creation and deletion of sounds to be supported across multiple threads. An application may have any thread that want's to call
    ma_engine_play_sound(), for example. The application would expect this to just work. The problem, however, is that the mixing thread will be iterating over
    the list at the same time. We need to be careful with how we remove a sound from the list because we'll essentially be taking the sound out from under the
    mixing thread and the mixing thread must continue to work. Normally you would wrap the iteration in a lock as well, however an added complication is that
    the mixing thread cannot be locked as it's running on the audio thread, and locking in the audio thread is a no-no).

    To start with, ma_engine_sound_detach() (this function) and ma_engine_sound_attach() need to be wrapped in a lock. This lock will *not* be used by the
    mixing thread. We therefore need to craft this in a very particular way so as to ensure the mixing thread does not lose track of it's iteration state. What
    we don't want to do is clear the pNextSoundInGroup variable to NULL. This need to be maintained to ensure the mixing thread can continue iteration even
    after the sound has been removed from the group. This is acceptable because sounds are fixed to their group for the entire life, and this function will
    only ever be called when the sound is being uninitialized which therefore means it'll never be iterated again.
    */
    ma_mutex_lock(&pGroup->lock);
    {
        if (pSound->pPrevSoundInGroup == NULL) {
            /* The sound is the head of the list. All we need to do is change the pPrevSoundInGroup member of the next sound to NULL and make it the new head. */

            /* Make a new head. */
            ma_atomic_exchange_ptr(&pGroup->pFirstSoundInGroup, pSound->pNextSoundInGroup);
        } else {
            /*
            The sound is not the head. We need to remove the sound from the group by simply changing the pNextSoundInGroup member of the previous sound. This is
            the important part. This is the part that allows the mixing thread to continue iteration without locking.
            */
            ma_atomic_exchange_ptr(&pSound->pPrevSoundInGroup->pNextSoundInGroup, pSound->pNextSoundInGroup);
        }

        /* This doesn't really need to be done atomically because we've wrapped this in a lock and it's not used by the mixing thread. */
        if (pSound->pNextSoundInGroup != NULL) {
            ma_atomic_exchange_ptr(&pSound->pNextSoundInGroup->pPrevSoundInGroup, pSound->pPrevSoundInGroup);
        }
    }
    ma_mutex_unlock(&pGroup->lock);

    return MA_SUCCESS;
}

static ma_result ma_engine_sound_attach(ma_engine* pEngine, ma_sound* pSound, ma_sound_group* pGroup)
{
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pSound  != NULL);
    MA_ASSERT(pGroup  != NULL);
    MA_ASSERT(pSound->pGroup == NULL);

    /* This should only ever be called when the sound is first initialized which means we should never be in a playing state. */
    MA_ASSERT(pSound->isPlaying == MA_FALSE);

    /* We can set the group at the start. */
    pSound->pGroup = pGroup;

    /*
    The sound will become the new head of the list. If we were only adding we could do this lock-free, but unfortunately we need to support fast, constant
    time removal of sounds from the list. This means we need to update two pointers, not just one, which means we can't use a standard compare-and-swap.

    One of our requirements is that the mixer thread must be able to iterate over the list *without* locking. We don't really need to do anything special
    here to support this, but we will want to use an atomic assignment.
    */
    ma_mutex_lock(&pGroup->lock);
    {
        ma_sound* pNewFirstSoundInGroup = pSound;
        ma_sound* pOldFirstSoundInGroup = pGroup->pFirstSoundInGroup;

        pNewFirstSoundInGroup->pNextSoundInGroup = pOldFirstSoundInGroup;
        if (pOldFirstSoundInGroup != NULL) {
            pOldFirstSoundInGroup->pPrevSoundInGroup = pNewFirstSoundInGroup;
        }

        ma_atomic_exchange_ptr(&pGroup->pFirstSoundInGroup, pNewFirstSoundInGroup);
    }
    ma_mutex_unlock(&pGroup->lock);

    return MA_SUCCESS;
}


static ma_result ma_engine_effect__on_process_pcm_frames__no_pre_effect_no_pitch(ma_engine_effect* pEngineEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_uint64 frameCount;

    /*
    This will be called if either there is no pre-effect nor pitch shift, or the pre-effect and pitch shift have already been processed. In this case it's allowed for
    pFramesIn to be equal to pFramesOut as from here on we support in-place processing. Also, the input and output frame counts should always be equal.
    */
    frameCount = ma_min(*pFrameCountIn, *pFrameCountOut);

    /* Panning. This is a no-op when the engine has only 1 channel or the pan is 0. */
    if (pEngineEffect->pEngine->channels == 1 || pEngineEffect->panner.pan == 0) {
        /* Fast path. No panning. */
        if (pEngineEffect->isSpatial == MA_FALSE) {
            /* Fast path. No spatialization. */
            if (pFramesIn == pFramesOut) {
                /* Super fast path. No-op. */
            } else {
                /* Slow path. Copy. */
                ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, pEngineEffect->pEngine->format, pEngineEffect->pEngine->channels);
            }
        } else {
            /* Slow path. Spatialization required. */
            ma_spatializer_process_pcm_frames(&pEngineEffect->spatializer, pFramesOut, pFramesIn, frameCount);
        }
    } else {
        /* Slow path. Panning required. */
        ma_panner_process_pcm_frames(&pEngineEffect->panner, pFramesOut, pFramesIn, frameCount);

        if (pEngineEffect->isSpatial == MA_FALSE) {
            /* Fast path. No spatialization. Don't do anything - the panning step above moved data into the output buffer for us. */
        } else {
            /* Slow path. Spatialization required. Note that we just panned which means the output buffer currently contains valid data. We can spatialize in-place. */
            ma_spatializer_process_pcm_frames(&pEngineEffect->spatializer, pFramesOut, pFramesOut, frameCount); /* <-- Intentionally set both input and output buffers to pFramesOut because we want this to be in-place. */
        }
    }

    *pFrameCountIn  = frameCount;
    *pFrameCountOut = frameCount;

    return MA_SUCCESS;
}

static ma_result ma_engine_effect__on_process_pcm_frames__no_pre_effect(ma_engine_effect* pEngineEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_bool32 isPitchingRequired = MA_TRUE;

    /*
    This will be called if either there is no pre-effect or the pre-effect has already been processed. We can safely assume the input and output data in the engine's format so no
    data conversion should be necessary here.
    */

    /* Fast path for when no pitching is required. */
    if (isPitchingRequired == MA_FALSE) {
        /* Fast path. No pitch shifting. */
        return ma_engine_effect__on_process_pcm_frames__no_pre_effect_no_pitch(pEngineEffect, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        /* Slow path. Pitch shifting required. We need to run everything through our data converter first. */

        /*
        We can output straight into the output buffer. The remaining effects support in-place processing so when we process those we'll just pass in the output buffer
        as the input buffer as well and the effect will operate on the buffer in-place.
        */
        ma_result result;
        ma_uint64 frameCountIn;
        ma_uint64 frameCountOut;

        result = ma_data_converter_process_pcm_frames(&pEngineEffect->converter, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
        if (result != MA_SUCCESS) {
            return result;
        }

        /* Here is where we want to apply the remaining effects. These can be processed in-place which means we want to set the input and output buffers to be the same. */
        frameCountIn  = *pFrameCountOut;  /* Not a mistake. Intentionally set to *pFrameCountOut. */
        frameCountOut = *pFrameCountOut;
        return ma_engine_effect__on_process_pcm_frames__no_pre_effect_no_pitch(pEngineEffect, pFramesOut, &frameCountIn, pFramesOut, &frameCountIn);  /* Intentionally setting the input buffer to pFramesOut for in-place processing. */
    }
}

static ma_result ma_engine_effect__on_process_pcm_frames__general(ma_engine_effect* pEngineEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_result result;
    ma_uint64 frameCountIn  = *pFrameCountIn;
    ma_uint64 frameCountOut = *pFrameCountOut;
    ma_uint64 totalFramesProcessedIn  = 0;
    ma_uint64 totalFramesProcessedOut = 0;
    ma_format effectFormat;
    ma_uint32 effectChannels;

    MA_ASSERT(pEngineEffect  != NULL);
    MA_ASSERT(pEngineEffect->pPreEffect != NULL);
    MA_ASSERT(pFramesIn      != NULL);
    MA_ASSERT(pFrameCountIn  != NULL);
    MA_ASSERT(pFramesOut     != NULL);
    MA_ASSERT(pFrameCountOut != NULL);

    /* The effect's input and output format will be the engine's format. If the pre-effect is of a different format it will need to be converted appropriately. */
    effectFormat   = pEngineEffect->pEngine->format;
    effectChannels = pEngineEffect->pEngine->channels;
    
    /*
    Getting here means we have a pre-effect. This must alway be run first. We do this in chunks into an intermediary buffer and then call ma_engine_effect__on_process_pcm_frames__no_pre_effect()
    against the intermediary buffer. The output of ma_engine_effect__on_process_pcm_frames__no_pre_effect() will be the final output buffer.
    */
    while (totalFramesProcessedIn < frameCountIn, totalFramesProcessedOut < frameCountOut) {
        ma_uint8  preEffectOutBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];  /* effectFormat / effectChannels */
        ma_uint32 preEffectOutBufferCap = sizeof(preEffectOutBuffer) / ma_get_bytes_per_frame(effectFormat, effectChannels);
        const void* pRunningFramesIn  = ma_offset_ptr(pFramesIn,  totalFramesProcessedIn  * ma_get_bytes_per_frame(effectFormat, effectChannels));
        /* */ void* pRunningFramesOut = ma_offset_ptr(pFramesOut, totalFramesProcessedOut * ma_get_bytes_per_frame(effectFormat, effectChannels));
        ma_uint64 frameCountInThisIteration;
        ma_uint64 frameCountOutThisIteration;

        frameCountOutThisIteration = frameCountOut - totalFramesProcessedOut;
        if (frameCountOutThisIteration > preEffectOutBufferCap) {
            frameCountOutThisIteration = preEffectOutBufferCap;
        }

        /* We need to ensure we don't read too many input frames that we won't be able to process them all in the next step. */
        frameCountInThisIteration = ma_data_converter_get_required_input_frame_count(&pEngineEffect->converter, frameCountOutThisIteration);
        if (frameCountInThisIteration > (frameCountIn - totalFramesProcessedIn)) {
            frameCountInThisIteration = (frameCountIn - totalFramesProcessedIn);
        }

        result = ma_effect_process_pcm_frames_ex(pEngineEffect->pPreEffect, pRunningFramesIn, &frameCountInThisIteration, preEffectOutBuffer, &frameCountOutThisIteration, effectFormat, effectChannels, effectFormat, effectChannels);
        if (result != MA_SUCCESS) {
            break;
        }

        totalFramesProcessedIn += frameCountInThisIteration;

        /* At this point we have run the pre-effect and we can now run it through the main engine effect. */
        frameCountOutThisIteration = frameCountOut - totalFramesProcessedOut;   /* Process as many frames as will fit in the output buffer. */
        result = ma_engine_effect__on_process_pcm_frames__no_pre_effect(pEngineEffect, preEffectOutBuffer, &frameCountInThisIteration, pRunningFramesOut, &frameCountOutThisIteration);
        if (result != MA_SUCCESS) {
            break;
        }

        totalFramesProcessedIn += frameCountOutThisIteration;
    }


    *pFrameCountIn  = totalFramesProcessedIn;
    *pFrameCountOut = totalFramesProcessedOut;

    return MA_SUCCESS;
}

static ma_result ma_engine_effect__on_process_pcm_frames(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_engine_effect* pEngineEffect = (ma_engine_effect*)pEffect;

    MA_ASSERT(pEffect != NULL);

    /* Optimized path for when there is no pre-effect. */
    if (pEngineEffect->pPreEffect == NULL) {
        return ma_engine_effect__on_process_pcm_frames__no_pre_effect(pEngineEffect, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        return ma_engine_effect__on_process_pcm_frames__general(pEngineEffect, pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    }
}

static ma_uint64 ma_engine_effect__on_get_required_input_frame_count(ma_effect* pEffect, ma_uint64 outputFrameCount)
{
    ma_engine_effect* pEngineEffect = (ma_engine_effect*)pEffect;
    ma_uint64 inputFrameCount;

    MA_ASSERT(pEffect != NULL);

    inputFrameCount = ma_data_converter_get_required_input_frame_count(&pEngineEffect->converter, outputFrameCount);

    if (pEngineEffect->pPreEffect != NULL) {
        ma_uint64 preEffectInputFrameCount = ma_effect_get_required_input_frame_count(pEngineEffect->pPreEffect, outputFrameCount);
        if (inputFrameCount < preEffectInputFrameCount) {
            inputFrameCount = preEffectInputFrameCount;
        }
    }

    return inputFrameCount;
}

static ma_uint64 ma_engine_effect__on_get_expected_output_frame_count(ma_effect* pEffect, ma_uint64 inputFrameCount)
{
    ma_engine_effect* pEngineEffect = (ma_engine_effect*)pEffect;
    ma_uint64 outputFrameCount;

    MA_ASSERT(pEffect != NULL);

    outputFrameCount = ma_data_converter_get_expected_output_frame_count(&pEngineEffect->converter, inputFrameCount);

    if (pEngineEffect->pPreEffect != NULL) {
        ma_uint64 preEffectOutputFrameCount = ma_effect_get_expected_output_frame_count(pEngineEffect->pPreEffect, inputFrameCount);
        if (outputFrameCount > preEffectOutputFrameCount) {
            outputFrameCount = preEffectOutputFrameCount;
        }
    }

    return outputFrameCount;
}

static ma_result ma_engine_effect__on_get_input_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_engine_effect* pEngineEffect = (ma_engine_effect*)pEffect;

    MA_ASSERT(pEffect != NULL);

    if (pEngineEffect->pPreEffect != NULL) {
        return ma_engine_effect__on_get_input_data_format(pEffect, pFormat, pChannels, pSampleRate);
    } else {
        *pFormat     = pEngineEffect->converter.config.formatIn;
        *pChannels   = pEngineEffect->converter.config.channelsIn;
        *pSampleRate = pEngineEffect->converter.config.sampleRateIn;

        return MA_SUCCESS;
    }
}

static ma_result ma_engine_effect__on_get_output_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_engine_effect* pEngineEffect = (ma_engine_effect*)pEffect;

    MA_ASSERT(pEffect != NULL);

    *pFormat     = pEngineEffect->converter.config.formatOut;
    *pChannels   = pEngineEffect->converter.config.channelsOut;
    *pSampleRate = pEngineEffect->converter.config.sampleRateOut;

    return MA_SUCCESS;
}

static ma_result ma_engine_effect_init(ma_engine* pEngine, ma_engine_effect* pEffect)
{
    ma_result result;
    ma_panner_config pannerConfig;
    ma_spatializer_config spatializerConfig;
    ma_data_converter_config converterConfig;

    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pEffect != NULL);

    MA_ZERO_OBJECT(pEffect);

    pEffect->baseEffect.onProcessPCMFrames = ma_engine_effect__on_process_pcm_frames;
    pEffect->baseEffect.onGetRequiredInputFrameCount  = ma_engine_effect__on_get_required_input_frame_count;
    pEffect->baseEffect.onGetExpectedOutputFrameCount = ma_engine_effect__on_get_expected_output_frame_count;
    pEffect->baseEffect.onGetInputDataFormat          = ma_engine_effect__on_get_input_data_format;
    pEffect->baseEffect.onGetOutputDataFormat         = ma_engine_effect__on_get_output_data_format;

    pEffect->pEngine    = pEngine;
    pEffect->pPreEffect = NULL;
    pEffect->pitch      = 1;
    pEffect->oldPitch   = 1;

    pannerConfig = ma_panner_config_init(pEngine->format, pEngine->channels);
    result = ma_panner_init(&pannerConfig, &pEffect->panner);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to create the panner. */
    }

    spatializerConfig = ma_spatializer_config_init(pEngine, pEngine->format, pEngine->channels);
    result = ma_spatializer_init(&spatializerConfig, &pEffect->spatializer);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to create the spatializer. */
    }



    /* Our effect processor requires f32 for now, but I may implement an s16 optimized pipeline. */
    

    converterConfig = ma_data_converter_config_init(pEngine->format, pEngine->format, pEngine->channels, pEngine->channels, pEngine->sampleRate, pEngine->sampleRate);

    /*
    TODO: A few things to figure out with the resampler:
        - In order to support dynamic pitch shifting we need to set allowDynamicSampleRate which means the resampler will always be initialized and will always
          have samples run through it. An optimization would be to have a flag that disables pitch shifting. Can alternatively just skip running samples through
          the data converter when pitch=1, but this may result in glitching when moving away from pitch=1 due to the internal buffer not being update while the
          pitch=1 case was in place.
        - We may want to have customization over resampling properties.
    */
    converterConfig.resampling.allowDynamicSampleRate = MA_TRUE;    /* This makes sure a resampler is always initialized. TODO: Need a flag that specifies that no pitch shifting is required for this sound so we can avoid the cost of the resampler. Even when the pitch is 1, samples still run through the resampler. */
    converterConfig.resampling.algorithm              = ma_resample_algorithm_linear;
    converterConfig.resampling.linear.lpfOrder        = 2;

    result = ma_data_converter_init(&converterConfig, &pEffect->converter);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_engine_effect_uninit(ma_engine* pEngine, ma_engine_effect* pEffect)
{
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pEffect != NULL);

    (void)pEngine;
    ma_data_converter_uninit(&pEffect->converter);
}

static ma_result ma_engine_effect_reinit(ma_engine* pEngine, ma_engine_effect* pEffect)
{
    /* This function assumes the data converter was previously initialized and needs to be uninitialized. */
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pEffect != NULL);

    ma_engine_effect_uninit(pEngine, pEffect);

    return ma_engine_effect_init(pEngine, pEffect);
}

MA_API ma_result ma_engine_create_sound_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_sound_group* pGroup, ma_sound* pSound)
{
    ma_result result;

    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pSound);

    if (pEngine == NULL || pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_engine_effect_init(pEngine, &pSound->effect);
    if (result != MA_SUCCESS) {
        return result;
    }

    pSound->pDataSource = pDataSource;
    pSound->volume      = 1;

    if (pGroup == NULL) {
        pGroup = &pEngine->masterSoundGroup;
    }

    /* By default the sound needs to be added to the master group. */
    result = ma_engine_sound_attach(pEngine, pSound, pGroup);
    if (result != MA_SUCCESS) {
        return result;  /* Should never happen. Failed to attach the sound to the group. */
    }

    return MA_SUCCESS;
}

MA_API void ma_engine_delete_sound(ma_engine* pEngine, ma_sound* pSound)
{
    ma_result result;

    if (pEngine == NULL || pSound == NULL) {
        return;
    }

    /* Make sure the sound is stopped as soon as possible to reduce the chance that it gets locked by the mixer. We also need to stop it before detaching from the group. */
    result = ma_engine_sound_stop(pEngine, pSound);
    if (result != MA_SUCCESS) {
        return;
    }

    /* The sound needs to removed from the group to ensure it doesn't get iterated again and cause things to break again. This is thread-safe. */
    result = ma_engine_sound_detach(pEngine, pSound);
    if (result != MA_SUCCESS) {
        return;
    }

    /*
    The sound is detached from the group, but it may still be in the middle of mixing which means our data source is locked. We need to wait for
    this to finish before deleting from the resource manager.

    We could define this so that we don't wait if the sound does not own the underlying data source, but this might end up being dangerous because
    the application may think it's safe to destroy the data source when it actually isn't. It just feels untidy doing it like that.
    */
    ma_engine_sound_mix_wait(pSound);


    /* Once the sound is detached from the group we can guarantee that it won't be referenced by the mixer thread which means it's safe for us to destroy the data source. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pSound->ownsDataSource) {
        ma_resource_manager_delete_data_source(pEngine->pResourceManager, pSound->pDataSource);
        pSound->pDataSource = NULL;
    }
#else
    MA_ASSERT(pSound->ownsDataSource == MA_FALSE);
#endif
}

MA_API ma_result ma_engine_sound_start(ma_engine* pEngine, ma_sound* pSound)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If the sound is already playing, do nothing. */
    if (pSound->isPlaying) {
        return MA_SUCCESS;
    }

    /* If the sound is at the end it means we want to start from the start again. */
    if (pSound->atEnd) {
        ma_result result = ma_data_source_seek_to_pcm_frame(pSound->pDataSource, 0);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to seek back to the start. */
        }
    }

    /* Once everything is set up we can tell the mixer thread about it. */
    ma_atomic_exchange_32(&pSound->isPlaying, MA_TRUE);

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_stop(ma_engine* pEngine, ma_sound* pSound)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_atomic_exchange_32(&pSound->isPlaying, MA_FALSE);

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_set_volume(ma_engine* pEngine, ma_sound* pSound, float volume)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->volume = volume;

    return MA_SUCCESS;
}

MA_API ma_result me_engine_sound_set_gain_db(ma_engine* pEngine, ma_sound* pSound, float gainDB)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_engine_sound_set_volume(pEngine, pSound, ma_gain_db_to_factor(gainDB));
}

MA_API ma_result ma_engine_sound_set_pitch(ma_engine* pEngine, ma_sound* pSound, float pitch)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->effect.pitch = pitch;

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_set_effect(ma_engine* pEngine, ma_sound* pSound, ma_effect* pEffect)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->effect.pPreEffect = pEffect;

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_set_position(ma_engine* pEngine, ma_sound* pSound, ma_vec3 position)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_spatializer_set_position(&pSound->effect.spatializer, position);
}

MA_API ma_result ma_engine_sound_set_rotation(ma_engine* pEngine, ma_sound* pSound, ma_quat rotation)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_spatializer_set_rotation(&pSound->effect.spatializer, rotation);
}

MA_API ma_result ma_engine_sound_set_looping(ma_engine* pEngine, ma_sound* pSound, ma_bool32 isLooping)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_atomic_exchange_32(&pSound->isLooping, isLooping);

    return MA_SUCCESS;
}

MA_API ma_bool32 ma_engine_sound_at_end(ma_engine* pEngine, const ma_sound* pSound)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_FALSE;
    }

    return pSound->atEnd;
}

MA_API ma_result ma_engine_play_sound(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup)
{
    ma_result result;
    ma_sound* pSound = NULL;
    ma_sound* pNextSound = NULL;

    if (pEngine == NULL || pFilePath == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pGroup == NULL) {
        pGroup = &pEngine->masterSoundGroup;
    }

    /*
    Fire and forget sounds are never actually removed from the group. In practice there should never be a huge number of sounds playing at the same time so we
    should be able to get away with recycling sounds. What we need, however, is a way to switch out the old data source with a new one.

    The first thing to do is find an available sound. We will only be doing a forward iteration here so we should be able to do this part without locking. A
    sound will be available for recycling if it's marked as internal and is at the end.
    */
    for (pNextSound = pGroup->pFirstSoundInGroup; pNextSound != NULL; pNextSound = pNextSound->pNextSoundInGroup) {
        if (pNextSound->_isInternal) {
            /*
            We need to check that atEnd flag to determine if this sound is available. The problem is that another thread might be wanting to acquire this
            sound at the same time. We want to avoid as much locking as possible, so we'll do this as a compare and swap.
            */
            if (ma_compare_and_swap_32(&pNextSound->atEnd, MA_FALSE, MA_TRUE) == MA_TRUE) {
                /* We got it. */
                pSound = pNextSound;
                break;
            } else {
                /* The sound is not available for recycling. Move on to the next one. */
            }
        }
    }

    if (pSound != NULL) {
        /*
        An existing sound is being recycled. There's no need to allocate memory or re-insert into the group (it's already there). All we need to do is replace
        the data source. The at-end flag has already been unset, and it will marked as playing at the end of this function.
        */
        ma_data_source* pNewDataSource;

        /* The at-end flag should have been set to false when we acquired the sound for recycling. */
        MA_ASSERT(pSound->atEnd == MA_FALSE);

        /*
        We want to create the new data source first. If the resource manager detects that the resource is already loaded it will run in an optimized path by
        simply incrementing a reference counter. If, however, we delete the resource first, we may end up in a situation where the reference counter is
        decremented to 0, the resource manager free's the internal resources, and then we end up just reloading the resource again. This will only ever happen
        if the recycled sound coincidentally uses the same underlying resource.

        TODO: Look at checking if there's a way to determine if the old data source shares the same file path as pFilePath and make this more intelligent.
        */
        result = ma_resource_manager_create_data_source(pEngine->pResourceManager, pFilePath, &pNewDataSource);
        if (result != MA_SUCCESS) {
            /* We failed to load the resource. We need to return an error. We must also put this sound back up for recycling by setting the at-end flag to true. */
            ma_atomic_exchange_32(&pSound->atEnd, MA_TRUE); /* <-- Put the sound back up for recycling. */
            return result;
        }

        /* We have the new data source, so now we can delete the old one. */
        if (pSound->pDataSource != NULL) {  /* <-- Safety. Should never happen. */
            ma_resource_manager_delete_data_source(pEngine->pResourceManager, pSound->pDataSource);
        }

        /* We need to reset the effect. */
        result = ma_engine_effect_reinit(pEngine, &pSound->effect);
        if (result != MA_SUCCESS) {
            /* We failed to reinitialize the effect. The sound is currently in a bad state and we need to delete it and return an error. Should never happen. */
            ma_engine_delete_sound(pEngine, pSound);
            return result;
        }

        /* We can now do the switch over to the new data source. */
        pSound->pDataSource = pNewDataSource;
    } else {
        /* There's no available sounds for recycling. We need to allocate a sound. This can be done using a stack allocator. */
        pSound = ma__malloc_from_callbacks(sizeof(*pSound), &pEngine->allocationCallbacks); /* TODO: This can certainly be optimized. Maybe add a soundAllocationCallbacks member or something. */
        if (pSound == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        result = ma_engine_create_sound_from_file(pEngine, pFilePath, pGroup, pSound);
        if (result != MA_SUCCESS) {
            ma__free_from_callbacks(pEngine, &pEngine->allocationCallbacks);
            return result;
        }

        /* The sound needs to be marked as internal for our own internal memory management reasons. This is how we know whether or not the sound is available for recycling. */
        pSound->_isInternal = MA_TRUE;  /* This is the only place _isInternal will be modified. We therefore don't need to worry about synchronizing access to this variable. */
    }

    /* Finally we can start playing the sound. */
    ma_engine_sound_start(pEngine, pSound);

    return MA_SUCCESS;
}


static ma_result ma_engine_sound_group_attach(ma_engine* pEngine, ma_sound_group* pGroup, ma_sound_group* pParentGroup)
{
    ma_sound_group* pNewFirstChild;
    ma_sound_group* pOldFirstChild;

    if (pEngine == NULL || pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Don't do anything for the master sound group. This should never be attached to anything. */
    if (pGroup == &pEngine->masterSoundGroup) {
        return MA_SUCCESS;
    }

    /* Must have a parent. */
    if (pParentGroup == NULL) {
        return MA_SUCCESS;
    }

    pNewFirstChild = pGroup;
    pOldFirstChild = pParentGroup->pFirstChild;

    /* It's an error for the group to already be assigned to a group. */
    MA_ASSERT(pGroup->pParent == NULL);
    pGroup->pParent = pParentGroup;

    /* Like sounds, we just make it so the new group becomes the new head. */
    pNewFirstChild->pNextSibling = pOldFirstChild;
    if (pOldFirstChild != NULL) {
        pOldFirstChild->pPrevSibling = pNewFirstChild;
    }

    pGroup->pFirstChild = pNewFirstChild;

    return MA_SUCCESS;
}

static ma_result ma_engine_sound_group_detach(ma_engine* pEngine, ma_sound_group* pGroup)
{
    if (pEngine == NULL || pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Don't do anything for the master sound group. This should never be detached from anything. */
    if (pGroup == &pEngine->masterSoundGroup) {
        return MA_SUCCESS;
    }

    if (pGroup->pPrevSibling == NULL) {
        /* It's the first child in the parent group. */
        MA_ASSERT(pGroup->pParent != NULL);
        MA_ASSERT(pGroup->pParent->pFirstChild == pGroup);

        ma_atomic_exchange_ptr(&pGroup->pParent->pFirstChild, pGroup->pNextSibling);
    } else {
        /* It's not the first child in the parent group. */
        ma_atomic_exchange_ptr(&pGroup->pPrevSibling->pNextSibling, pGroup->pNextSibling);
    }

    /* The previous sibling needs to be changed for the old next sibling. */
    if (pGroup->pNextSibling != NULL) {
        pGroup->pNextSibling->pPrevSibling = pGroup->pPrevSibling;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_group_init(ma_engine* pEngine, ma_sound_group* pParentGroup, ma_sound_group* pGroup)
{
    ma_result result;
    ma_mixer_config mixerConfig;

    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pGroup);

    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Use the master group if the parent group is NULL, so long as it's not the master group itself. */
    if (pParentGroup == NULL && pGroup != &pEngine->masterSoundGroup) {
        pParentGroup = &pEngine->masterSoundGroup;
    }


    /* TODO: Look at the possibility of allowing groups to use a different format to the primary data format. Makes mixing and group management much more complicated. */

    /* The sound group needs a mixer. */
    mixerConfig = ma_mixer_config_init(pEngine->format, pEngine->channels, pEngine->periodSizeInFrames, NULL, &pEngine->allocationCallbacks);
    result = ma_mixer_init(&mixerConfig, &pGroup->mixer);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* Attach the sound group to it's parent if it has one (this will only happen if it's the master group). */
    if (pParentGroup != NULL) {
        result = ma_engine_sound_group_attach(pEngine, pGroup, pParentGroup);
        if (result != MA_SUCCESS) {
            ma_mixer_uninit(&pGroup->mixer);
            return result;
        }
    } else {
        MA_ASSERT(pGroup == &pEngine->masterSoundGroup);    /* The master group is the only one allowed to not have a parent group. */
    }

    /*
    We need to initialize the lock that'll be used to synchronize adding and removing of sounds to the group. This lock is _not_ used by the mixing thread. The mixing
    thread is written in a way where a lock should not be required.
    */
    result = ma_mutex_init(&pEngine->context, &pGroup->lock);
    if (result != MA_SUCCESS) {
        ma_engine_sound_group_detach(pEngine, pGroup);
        ma_mixer_uninit(&pGroup->mixer);
        return result;
    }

    /* The group needs to be started by default, but needs to be done after attaching to the internal list. */
    pGroup->isPlaying = MA_TRUE;

    return MA_SUCCESS;
}

MA_API void ma_engine_sound_group_uninit(ma_engine* pEngine, ma_sound_group* pGroup)
{
    ma_result result;

    result = ma_engine_sound_group_stop(pEngine, pGroup);
    if (result != MA_SUCCESS) {
        MA_ASSERT(MA_FALSE);    /* Should never happen. Trigger an assert for debugging, but don't stop uninitializing in production to ensure we free memory down below. */
    }

    result = ma_engine_sound_group_detach(pEngine, pGroup);
    if (result != MA_SUCCESS) {
        MA_ASSERT(MA_FALSE);    /* As above, should never happen, but just in case trigger an assert in debug mode, but continue processing. */
    }

    ma_mixer_uninit(&pGroup->mixer);
    ma_mutex_uninit(&pGroup->lock);
}

MA_API ma_result ma_engine_sound_group_start(ma_engine* pEngine, ma_sound_group* pGroup)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pGroup == NULL) {
        pGroup = &pEngine->masterSoundGroup;
    }

    ma_atomic_exchange_32(&pGroup->isPlaying, MA_TRUE);

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_group_stop(ma_engine* pEngine, ma_sound_group* pGroup)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pGroup == NULL) {
        pGroup = &pEngine->masterSoundGroup;
    }

    ma_atomic_exchange_32(&pGroup->isPlaying, MA_FALSE);

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_group_set_volume(ma_engine* pEngine, ma_sound_group* pGroup, float volume)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pGroup == NULL) {
        pGroup = &pEngine->masterSoundGroup;
    }

    /* The volume is set via the mixer. */
    ma_mixer_set_volume(&pGroup->mixer, volume);

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_group_set_gain_db(ma_engine* pEngine, ma_sound_group* pGroup, float gainDB)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_engine_sound_group_set_volume(pEngine, pGroup, ma_gain_db_to_factor(gainDB));
}

MA_API ma_result ma_engine_sound_group_set_effect(ma_engine* pEngine, ma_sound_group* pGroup, ma_effect* pEffect)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pGroup == NULL) {
        pGroup = &pEngine->masterSoundGroup;
    }

    /* The effect is set on the mixer. */
    ma_mixer_set_effect(&pGroup->mixer, pEffect);

    return MA_SUCCESS;
}



MA_API ma_result ma_engine_listener_set_position(ma_engine* pEngine, ma_vec3 position)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    pEngine->listener.position = position;

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_listener_set_rotation(ma_engine* pEngine, ma_quat rotation)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    pEngine->listener.rotation = rotation;

    return MA_SUCCESS;
}


#endif
