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

typedef void      ma_vfs;
typedef ma_handle ma_vfs_file;

#define MA_OPEN_MODE_READ   0x00000001
#define MA_OPEN_MODE_WRITE  0x00000002

typedef struct
{
    ma_uint64 sizeInBytes;
} ma_file_info;

typedef struct
{
    ma_result (* onOpen) (ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile);
    ma_result (* onClose)(ma_vfs* pVFS, ma_vfs_file file);
    ma_result (* onRead) (ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead);
    ma_result (* onWrite)(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten);
    ma_result (* onSeek) (ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin);
    ma_result (* onTell) (ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor);
    ma_result (* onInfo) (ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo);
} ma_vfs_callbacks;

MA_API ma_result ma_vfs_open(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile);
MA_API ma_result ma_vfs_close(ma_vfs* pVFS, ma_vfs_file file);
MA_API ma_result ma_vfs_read(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead);
MA_API ma_result ma_vfs_write(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten);
MA_API ma_result ma_vfs_seek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin);
MA_API ma_result ma_vfs_tell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor);
MA_API ma_result ma_vfs_info(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo);
MA_API ma_result ma_vfs_open_and_read_file(ma_vfs* pVFS, const char* pFilePath, void** ppData, size_t* pSize, const ma_allocation_callbacks* pAllocationCallbacks);

typedef struct
{
    ma_vfs_callbacks cb;
} ma_default_vfs;

MA_API ma_result ma_default_vfs_init(ma_default_vfs* pVFS);


/*
Fully decodes a file from a VFS.
*/
MA_API ma_result ma_decode_from_vfs(ma_vfs* pVFS, const char* pFilePath, ma_decoder_config* pConfig, ma_uint64* pFrameCountOut, void** ppPCMFramesOut);


/*
Memory Allocation Types
=======================
When allocating memory you may want to optimize your custom allocators based on what it is miniaudio is actually allocating. Normally the context in which you
are using the allocator is enough to optimize allocations, however there are high-level APIs that perform many different types of allocations and it can be
useful to be told exactly what it being allocated so you can optimize your allocations appropriately.
*/
#define MA_ALLOCATION_TYPE_GENERAL                      0x00000001  /* A general memory allocation. */
#define MA_ALLOCATION_TYPE_CONTEXT                      0x00000002  /* A ma_context allocation. */
#define MA_ALLOCATION_TYPE_DEVICE                       0x00000003  /* A ma_device allocation. */
#define MA_ALLOCATION_TYPE_DECODER                      0x00000004  /* A ma_decoder allocation. */
#define MA_ALLOCATION_TYPE_AUDIO_BUFFER                 0x00000005  /* A ma_audio_buffer allocation. */
#define MA_ALLOCATION_TYPE_ENCODED_BUFFER               0x00000006  /* Allocation for encoded audio data containing the raw file data of a sound file. */
#define MA_ALLOCATION_TYPE_DECODED_BUFFER               0x00000007  /* Allocation for decoded audio data from a sound file. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_NODE        0x00000010  /* A ma_resource_manager_data_node object. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_PAGE        0x00000011  /* A ma_resource_manager_page object. Used by the resource manager for when a page containing decoded audio data is loaded for streaming. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_SOURCE 0x00000012  /* A ma_resource_manager_data_source object. */

/*
Resource Manager Data Source Flags
==================================
The flags below are used for controlling how the resource manager should handle the loading and caching of data sources.
*/
#define MA_DATA_SOURCE_FLAG_DECODE  0x00000001  /* Decode data before storing in memory. When set, decoding is done at the resource manager level rather than the mixing thread. Results in faster mixing, but higher memory usage. */
#define MA_DATA_SOURCE_FLAG_STREAM  0x00000002  /* When set, does not load the entire data source in memory. Disk I/O will happen on the resource manager thread. */
#define MA_DATA_SOURCE_FLAG_ASYNC   0x00000004  /* When set, the resource manager will load the data source asynchronously. */

/*
The size in bytes of a page containing raw audio data.
*/
#ifndef MA_RESOURCE_MANAGER_PAGE_SIZE_IN_BYTES
#define MA_RESOURCE_MANAGER_PAGE_SIZE_IN_BYTES  16384
#endif

typedef enum
{
    ma_resource_manager_data_node_type_encoded,
    ma_resource_manager_data_node_type_decoded
} ma_resource_manager_data_node_type;

typedef enum
{
    ma_resource_manager_data_source_type_unknown,
    ma_resource_manager_data_source_type_decoder,
    ma_resource_manager_data_source_type_buffer
} ma_resource_manager_data_source_type;


typedef struct ma_resource_manager             ma_resource_manager;
typedef struct ma_resource_manager_data_node   ma_resource_manager_data_node;
typedef struct ma_resource_manager_data_source ma_resource_manager_data_source;


typedef struct ma_resource_manager_page ma_resource_manager_page;

typedef struct
{
    ma_resource_manager_page* pPrev;
    ma_resource_manager_page* pNext;
    ma_uint32 frameCount;
} ma_resource_manager_page_header;

struct ma_resource_manager_page
{
    ma_resource_manager_page_header header;
    ma_uint8 data[MA_RESOURCE_MANAGER_PAGE_SIZE_IN_BYTES - sizeof(ma_resource_manager_page_header)];
};



/* TODO: May need to do some stress testing and tweak this. */
#ifndef MA_RESOURCE_MANAGER_MESSAGE_QUEUE_CAPACITY
#define MA_RESOURCE_MANAGER_MESSAGE_QUEUE_CAPACITY  1024
#endif

#define MA_MESSAGE_TERMINATE        0x00000000
#define MA_MESSAGE_LOAD_DATA_NODE   0x00000001
#define MA_MESSAGE_FREE_DATA_NODE   0x00000002
#define MA_MESSAGE_LOAD_DATA_SOURCE 0x00000003
/*#define MA_MESSAGE_FREE_DATA_SOURCE 0x00000004*/

typedef struct
{
    ma_event* pEvent;
    ma_uint32 code;
    union
    {
        struct
        {
            ma_resource_manager_data_node* pDataNode;
            char* pFilePath;    /* Allocated when the message is posted, freed by the async thread after loading. */
            ma_event* pEvent;
        } loadDataNode;
        struct
        {
            ma_resource_manager_data_node* pDataNode;
        } freeDataNode;
        struct
        {
            ma_resource_manager_data_source* pDataSource;
            ma_event* pEvent;
        } loadDataSource;
        struct
        {
            ma_resource_manager_data_source* pDataSource;
        } freeDataSource;
    };
} ma_resource_manager_message;

MA_API ma_resource_manager_message ma_resource_manager_message_init(ma_uint32 code);


typedef struct
{
    ma_resource_manager_message messages[MA_RESOURCE_MANAGER_MESSAGE_QUEUE_CAPACITY];
    volatile ma_uint32 getCursor;   /* For reading. */
    volatile ma_uint32 putCursor;   /* For writing. */
    ma_semaphore sem;               /* Semaphore for only freeing */
    ma_mutex lock;                  /* For thread-safe access to the message queue. */
} ma_resource_manager_message_queue;

MA_API ma_result ma_resource_manager_message_queue_init(ma_resource_manager* pResourceManager, ma_resource_manager_message_queue* pQueue);
MA_API void ma_resource_manager_message_queue_uninit(ma_resource_manager_message_queue* pQueue);
MA_API ma_result ma_resource_manager_message_queue_post(ma_resource_manager_message_queue* pQueue, const ma_resource_manager_message* pMessage);
MA_API ma_result ma_resource_manager_message_queue_next(ma_resource_manager_message_queue* pQueue, ma_resource_manager_message* pMessage); /* Blocking */
MA_API ma_result ma_resource_manager_message_queue_peek(ma_resource_manager_message_queue* pQueue, ma_resource_manager_message* pMessage); /* Non-Blocking */


typedef struct
{
    const void* pData;
    ma_uint64 frameCount;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
} ma_decoded_data;

typedef struct
{
    const void* pData;
    size_t sizeInBytes;
} ma_encoded_data;

typedef struct
{
    ma_resource_manager_data_node_type type;
    union
    {
        ma_decoded_data decoded;
        ma_encoded_data encoded;
    };
} ma_resource_manager_memory_buffer;

struct ma_resource_manager_data_node
{
    ma_uint32 hashedName32; /* The hashed name. This is the key. */
    ma_uint32 refCount;
    ma_result result;       /* Result from asynchronous loading. When loading set to MA_BUSY. When fully loaded set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. */
    ma_bool32 isDataOwnedByResourceManager;
    ma_resource_manager_memory_buffer data;
    ma_resource_manager_data_node* pParent;
    ma_resource_manager_data_node* pChildLo;
    ma_resource_manager_data_node* pChildHi;
};

struct ma_resource_manager_data_source
{
    ma_data_source_callbacks ds;
    ma_resource_manager_data_node* pDataNode;
    ma_resource_manager_data_source_type type;
    ma_result result;       /* Result from asynchronous loading. When loading set to MA_BUSY. When fully loaded set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. */
    /*ma_uint32 holdCount;*/    /* For preventing the backend from being uninitialized from under the data source while it's in the middle of performing an operation. */

    ma_bool32 isStreaming;
    union
    {
        int _unused;
    } streaming;
    union
    {
        ma_decoder decoder;
        ma_audio_buffer buffer;
    } backend;
};

typedef struct
{
    size_t level1CacheSizeInBytes;  /* Set to 0 to disable level 1 cache. Set to (size_t)-1 for unlimited. */
    size_t level2CacheSizeInBytes;  /* Set to 0 to disable level 2 cache. Set to (size_t)-1 for unlimited. */
    ma_uint32 level1CacheFlags;
    ma_uint32 level2CacheFlags;
    ma_allocation_callbacks allocationCallbacks;
    ma_format decodedFormat;
    ma_uint32 decodedChannels;
    ma_uint32 decodedSampleRate;
    ma_vfs* pVFS;                   /* Can be NULL in which case defaults will be used. */
} ma_resource_manager_config;

MA_API ma_resource_manager_config ma_resource_manager_config_init(ma_format decodedFormat, ma_uint32 decodedChannels, ma_uint32 decodedSampleRate, const ma_allocation_callbacks* pAllocationCallbacks);

struct ma_resource_manager
{
    ma_resource_manager_config config;
    ma_resource_manager_data_node* pRootDataNode;   /* The root node in the binary tree. */
    ma_mutex dataNodeLock;                          /* For synchronizing access to the data node binary tree. */
    ma_thread asyncThread;                          /* Thread for running asynchronous operations. */
    ma_resource_manager_message_queue messageQueue;
    ma_default_vfs defaultVFS;                      /* Only used if a custom VFS is not specified. */
};

MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager);
MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager);

/* Data Nodes. */
MA_API ma_result ma_resource_manager_create_data_node(ma_resource_manager* pResourceManager, const char* pFilePath, ma_resource_manager_data_node_type type, ma_event* pEvent, ma_resource_manager_data_node** ppDataNode);
MA_API ma_result ma_resource_manager_delete_data_node(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode);
MA_API ma_result ma_resource_manager_data_node_result(ma_resource_manager* pResourceManager, const ma_resource_manager_data_node* pDataNode);
MA_API ma_result ma_resource_manager_register_decoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate);  /* Does not copy. Increments the reference count if already exists and returns MA_SUCCESS. */
MA_API ma_result ma_resource_manager_register_encoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, size_t sizeInBytes);    /* Does not copy. Increments the reference count if already exists and returns MA_SUCCESS. */
MA_API ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName);

/* Data Sources. */
MA_API ma_result ma_resource_manager_data_source_init(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_uninit(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource);

/* Message handling. */
MA_API ma_result ma_resource_manager_handle_message(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage);
MA_API ma_result ma_resource_manager_post_message(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage);  /* Message will be copied. */


/*
Engine
======
The `ma_engine` API is a high-level API for audio playback. Internally it contains sounds (`ma_sound`) with resources managed via a resource manager
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
typedef enum
{
    ma_pan_mode_balance = 0,    /* Does not blend one side with the other. Technically just a balance. Compatible with other popular audio engines and therefore the default. */
    ma_pan_mode_pan             /* A true pan. The sound from one side will "move" to the other side and blend with it. */
} ma_pan_mode;

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_pan_mode mode;
    float pan;
} ma_panner_config;

MA_API ma_panner_config ma_panner_config_init(ma_format format, ma_uint32 channels);


typedef struct
{
    ma_effect_base effect;
    ma_format format;
    ma_uint32 channels;
    ma_pan_mode mode;
    float pan;  /* -1..1 where 0 is no pan, -1 is left side, +1 is right side. Defaults to 0. */
} ma_panner;

MA_API ma_result ma_panner_init(const ma_panner_config* pConfig, ma_panner* pPanner);
MA_API ma_result ma_panner_process_pcm_frames(ma_panner* pPanner, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_result ma_panner_set_mode(ma_panner* pPanner, ma_pan_mode mode);
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
    ma_resource_manager_data_source resourceManagerDataSource;
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
MA_API ma_result ma_engine_set_gain_db(ma_engine* pEngine, float gainDB);

#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_engine_sound_init_from_file(ma_engine* pEngine, const char* pFilePath, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound);
#endif
MA_API ma_result ma_engine_sound_init_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound);
MA_API void ma_engine_sound_uninit(ma_engine* pEngine, ma_sound* pSound);
MA_API ma_result ma_engine_sound_start(ma_engine* pEngine, ma_sound* pSound);
MA_API ma_result ma_engine_sound_stop(ma_engine* pEngine, ma_sound* pSound);
MA_API ma_result ma_engine_sound_set_volume(ma_engine* pEngine, ma_sound* pSound, float volume);
MA_API ma_result ma_engine_sound_set_gain_db(ma_engine* pEngine, ma_sound* pSound, float gainDB);
MA_API ma_result ma_engine_sound_set_pan(ma_engine* pEngine, ma_sound* pSound, float pan);
MA_API ma_result ma_engine_sound_set_pitch(ma_engine* pEngine, ma_sound* pSound, float pitch);
MA_API ma_result ma_engine_sound_set_position(ma_engine* pEngine, ma_sound* pSound, ma_vec3 position);
MA_API ma_result ma_engine_sound_set_rotation(ma_engine* pEngine, ma_sound* pSound, ma_quat rotation);
MA_API ma_result ma_engine_sound_set_effect(ma_engine* pEngine, ma_sound* pSound, ma_effect* pEffect);
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

MA_API ma_result ma_vfs_open(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
    ma_vfs_callbacks* pCallbacks = (ma_vfs_callbacks*)pVFS;

    if (pFile == NULL) {
        return MA_INVALID_ARGS;
    }

    *pFile = NULL;

    if (pVFS == NULL || pFilePath == NULL || openMode == 0) {
        return MA_INVALID_ARGS;
    }

    if (pCallbacks->onOpen == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    return pCallbacks->onOpen(pVFS, pFilePath, openMode, pFile);
}

MA_API ma_result ma_vfs_close(ma_vfs* pVFS, ma_vfs_file file)
{
    ma_vfs_callbacks* pCallbacks = (ma_vfs_callbacks*)pVFS;

    if (pVFS == NULL || file == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pCallbacks->onClose == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    return pCallbacks->onClose(pVFS, file);
}

MA_API ma_result ma_vfs_read(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
{
    ma_vfs_callbacks* pCallbacks = (ma_vfs_callbacks*)pVFS;

    if (pBytesRead != NULL) {
        *pBytesRead = 0;
    }

    if (pVFS == NULL || file == NULL || pDst == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pCallbacks->onRead == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    return pCallbacks->onRead(pVFS, file, pDst, sizeInBytes, pBytesRead);
}

MA_API ma_result ma_vfs_write(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten)
{
    ma_vfs_callbacks* pCallbacks = (ma_vfs_callbacks*)pVFS;

    if (pBytesWritten == NULL) {
        *pBytesWritten = 0;
    }

    if (pVFS == NULL || file == NULL || pSrc == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pCallbacks->onWrite == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    return pCallbacks->onWrite(pVFS, file, pSrc, sizeInBytes, pBytesWritten);
}

MA_API ma_result ma_vfs_seek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
    ma_vfs_callbacks* pCallbacks = (ma_vfs_callbacks*)pVFS;

    if (pVFS == NULL || file == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pCallbacks->onSeek == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    return pCallbacks->onSeek(pVFS, file, offset, origin);
}

MA_API ma_result ma_vfs_tell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor)
{
    ma_vfs_callbacks* pCallbacks = (ma_vfs_callbacks*)pVFS;

    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;

    if (pVFS == NULL || file == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pCallbacks->onTell == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    return pCallbacks->onTell(pVFS, file, pCursor);
}

MA_API ma_result ma_vfs_info(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo)
{
    ma_vfs_callbacks* pCallbacks = (ma_vfs_callbacks*)pVFS;

    if (pInfo == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pInfo);

    if (pVFS == NULL || file == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pCallbacks->onInfo == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    return pCallbacks->onInfo(pVFS, file, pInfo);
}


static ma_result ma_vfs_open_and_read_file_ex(ma_vfs* pVFS, const char* pFilePath, void** ppData, size_t* pSize, const ma_allocation_callbacks* pAllocationCallbacks, ma_uint32 allocationType)
{
    ma_result result;
    ma_vfs_file file;
    ma_file_info info;
    void* pData;
    size_t bytesRead;

    (void)allocationType;

    if (ppData != NULL) {
        *ppData = NULL;
    }
    if (pSize != NULL) {
        *pSize = 0;
    }

    if (ppData == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_vfs_open(pVFS, pFilePath, MA_OPEN_MODE_READ, &file);
    if (result != MA_SUCCESS) {
        return result;
    }

    result = ma_vfs_info(pVFS, file, &info);
    if (result != MA_SUCCESS) {
        ma_vfs_close(pVFS, file);
        return result;
    }

    if (info.sizeInBytes > MA_SIZE_MAX) {
        ma_vfs_close(pVFS, file);
        return MA_TOO_BIG;
    }

    pData = ma__malloc_from_callbacks((size_t)info.sizeInBytes, pAllocationCallbacks);  /* Safe cast. */
    if (pData == NULL) {
        ma_vfs_close(pVFS, file);
        return result;
    }

    result = ma_vfs_read(pVFS, file, pData, (size_t)info.sizeInBytes, &bytesRead);  /* Safe cast. */
    ma_vfs_close(pVFS, file);

    if (result != MA_SUCCESS) {
        ma__free_from_callbacks(pData, pAllocationCallbacks);
        return result;
    }

    if (pSize != NULL) {
        *pSize = bytesRead;
    }

    MA_ASSERT(ppData != NULL);
    *ppData = pData;

    return MA_SUCCESS;
}

ma_result ma_vfs_open_and_read_file(ma_vfs* pVFS, const char* pFilePath, void** ppData, size_t* pSize, const ma_allocation_callbacks* pAllocationCallbacks)
{
    return ma_vfs_open_and_read_file_ex(pVFS, pFilePath, ppData, pSize, pAllocationCallbacks, MA_ALLOCATION_TYPE_GENERAL);
}


static ma_result ma_default_vfs_open__stdio(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
    ma_result result;
    FILE* pFileStd;
    const char* pOpenModeStr;

    MA_ASSERT(pVFS      != NULL);
    MA_ASSERT(pFilePath != NULL);
    MA_ASSERT(openMode  != 0);
    MA_ASSERT(pFile     != NULL);

    if ((openMode & MA_OPEN_MODE_READ) != 0) {
        if ((openMode & MA_OPEN_MODE_WRITE) != 0) {
            pOpenModeStr = "r+";
        } else {
            pOpenModeStr = "rb";
        }
    } else {
        pOpenModeStr = "wb";
    }

    result = ma_fopen(&pFileStd, pFilePath, pOpenModeStr);
    if (result != MA_SUCCESS) {
        return result;
    }

    *pFile = pFileStd;

    return MA_SUCCESS;
}

static ma_result ma_default_vfs_close__stdio(ma_vfs* pVFS, ma_vfs_file file)
{
    MA_ASSERT(pVFS != NULL);
    MA_ASSERT(file != NULL);

    fclose((FILE*)file);

    return MA_SUCCESS;
}

static ma_result ma_default_vfs_read__stdio(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
{
    size_t result;

    MA_ASSERT(pVFS != NULL);
    MA_ASSERT(file != NULL);
    MA_ASSERT(pDst != NULL);
    
    result = fread(pDst, 1, sizeInBytes, (FILE*)file);

    if (pBytesRead != NULL) {
        *pBytesRead = result;
    }
    
    if (result != sizeInBytes) {
        if (feof((FILE*)file)) {
            return MA_END_OF_FILE;
        } else {
            return ma_result_from_errno(ferror((FILE*)file));
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_default_vfs_write__stdio(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten)
{
    size_t result;

    MA_ASSERT(pVFS != NULL);
    MA_ASSERT(file != NULL);
    MA_ASSERT(pSrc != NULL);

    result = fwrite(pSrc, 1, sizeInBytes, (FILE*)file);

    if (pBytesWritten != NULL) {
        *pBytesWritten = result;
    }

    if (result != sizeInBytes) {
        return ma_result_from_errno(ferror((FILE*)file));
    }

    return MA_SUCCESS;
}

static ma_result ma_default_vfs_seek__stdio(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
    int result;

    MA_ASSERT(pVFS != NULL);
    MA_ASSERT(file != NULL);
    
#if defined(_WIN32)
    result = _fseeki64((FILE*)file, offset, origin);
#else
    result = fseek((FILE*)file, (long int)offset, origin);
#endif
    if (result != 0) {
        return MA_ERROR;
    }

    return MA_SUCCESS;
}

static ma_result ma_default_vfs_tell__stdio(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor)
{
    ma_int64 result;

    MA_ASSERT(pVFS    != NULL);
    MA_ASSERT(file    != NULL);
    MA_ASSERT(pCursor != NULL);

#if defined(_WIN32)
    result = _ftelli64((FILE*)file);
#else
    result = ftell((FILE*)file);
#endif

    *pCursor = result;

    return MA_SUCCESS;
}

static ma_result ma_default_vfs_info__stdio(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo)
{
    int fd;
    struct stat info;

    MA_ASSERT(pVFS  != NULL);
    MA_ASSERT(file  != NULL);
    MA_ASSERT(pInfo != NULL);

#if defined(_MSC_VER)
    fd = _fileno((FILE*)file);
#else
    fd =  fileno((FILE*)file);
#endif

    if (fstat(fd, &info) != 0) {
        return ma_result_from_errno(errno);
    }

    pInfo->sizeInBytes = info.st_size;

    return MA_SUCCESS;
}


static ma_result ma_default_vfs_open(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile)
{
    if (pFile == NULL) {
        return MA_INVALID_ARGS;
    }

    *pFile = NULL;

    if (pVFS == NULL || pFilePath == NULL || openMode == 0) {
        return MA_INVALID_ARGS;
    }

    return ma_default_vfs_open__stdio(pVFS, pFilePath, openMode, pFile);
}

static ma_result ma_default_vfs_close(ma_vfs* pVFS, ma_vfs_file file)
{
    if (pVFS == NULL || file == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_default_vfs_close__stdio(pVFS, file);
}

static ma_result ma_default_vfs_read(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead)
{
    if (pVFS == NULL || file == NULL || pDst == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_default_vfs_read__stdio(pVFS, file, pDst, sizeInBytes, pBytesRead);
}

static ma_result ma_default_vfs_write(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten)
{
    if (pVFS == NULL || file == NULL || pSrc == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_default_vfs_write__stdio(pVFS, file, pSrc, sizeInBytes, pBytesWritten);
}

static ma_result ma_default_vfs_seek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin)
{
    if (pVFS == NULL || file == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_default_vfs_seek__stdio(pVFS, file, offset, origin);
}

static ma_result ma_default_vfs_tell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor)
{
    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;

    if (pVFS == NULL || file == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_default_vfs_tell__stdio(pVFS, file, pCursor);
}

static ma_result ma_default_vfs_info(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo)
{
    if (pInfo == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pInfo);

    if (pVFS == NULL || file == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_default_vfs_info__stdio(pVFS, file, pInfo);
}


MA_API ma_result ma_default_vfs_init(ma_default_vfs* pVFS)
{
    if (pVFS == NULL) {
        return MA_INVALID_ARGS;
    }

    pVFS->cb.onOpen  = ma_default_vfs_open;
    pVFS->cb.onClose = ma_default_vfs_close;
    pVFS->cb.onRead  = ma_default_vfs_read;
    pVFS->cb.onWrite = ma_default_vfs_write;
    pVFS->cb.onSeek  = ma_default_vfs_seek;
    pVFS->cb.onTell  = ma_default_vfs_tell;
    pVFS->cb.onInfo  = ma_default_vfs_info;

    return MA_SUCCESS;
}


static MA_INLINE ma_uint32 ma_swap_endian_uint32(ma_uint32 n)
{
#ifdef MA_HAS_BYTESWAP32_INTRINSIC
    #if defined(_MSC_VER)
        return _byteswap_ulong(n);
    #elif defined(__GNUC__) || defined(__clang__)
        #if defined(MA_ARM) && (defined(__ARM_ARCH) && __ARM_ARCH >= 6) && !defined(MA_64BIT)   /* <-- 64-bit inline assembly has not been tested, so disabling for now. */
            /* Inline assembly optimized implementation for ARM. In my testing, GCC does not generate optimized code with __builtin_bswap32(). */
            ma_uint32 r;
            __asm__ __volatile__ (
            #if defined(MA_64BIT)
                "rev %w[out], %w[in]" : [out]"=r"(r) : [in]"r"(n)   /* <-- This is untested. If someone in the community could test this, that would be appreciated! */
            #else
                "rev %[out], %[in]" : [out]"=r"(r) : [in]"r"(n)
            #endif
            );
            return r;
        #else
            return __builtin_bswap32(n);
        #endif
    #else
        #error "This compiler does not support the byte swap intrinsic."
    #endif
#else
    return ((n & 0xFF000000) >> 24) |
           ((n & 0x00FF0000) >>  8) |
           ((n & 0x0000FF00) <<  8) |
           ((n & 0x000000FF) << 24);
#endif
}



#ifndef MA_DEFAULT_HASH_SEED
#define MA_DEFAULT_HASH_SEED    42
#endif

/* MurmurHash3. Based on code from https://github.com/PeterScott/murmur3/blob/master/murmur3.c (public domain). */
static MA_INLINE ma_uint32 ma_rotl32(ma_uint32 x, ma_int8 r)
{
    return (x << r) | (x >> (32 - r));
}

static MA_INLINE ma_uint32 ma_hash_getblock(const ma_uint32* blocks, int i)
{
    if (ma_is_little_endian()) {
        return blocks[i];
    } else {
        return ma_swap_endian_uint32(blocks[i]);
    }
}

static MA_INLINE ma_uint32 ma_hash_fmix32(ma_uint32 h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    
    return h;
}

static ma_uint32 ma_hash_32(const void* key, int len, ma_uint32 seed)
{
    const ma_uint8* data = (const ma_uint8*)key;
    const ma_uint32* blocks;
    const ma_uint8* tail;
    const int nblocks = len / 4;
    ma_uint32 h1 = seed;
    ma_uint32 c1 = 0xcc9e2d51;
    ma_uint32 c2 = 0x1b873593;
    ma_uint32 k1;
    int i;

    blocks = (const ma_uint32 *)(data + nblocks*4);

    for(i = -nblocks; i; i++) {
        k1 = ma_hash_getblock(blocks,i);

        k1 *= c1;
        k1 = ma_rotl32(k1, 15);
        k1 *= c2;
    
        h1 ^= k1;
        h1 = ma_rotl32(h1, 13); 
        h1 = h1*5 + 0xe6546b64;
    }


    tail = (const ma_uint8*)(data + nblocks*4);

    k1 = 0;
    switch(len & 3) {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
                k1 *= c1; k1 = ma_rotl32(k1, 15); k1 *= c2; h1 ^= k1;
    };


    h1 ^= len;
    h1  = ma_hash_fmix32(h1);

    return h1;
}
/* End MurmurHash3 */

static ma_uint32 ma_hash_string_32(const char* str)
{
    return ma_hash_32(str, strlen(str), MA_DEFAULT_HASH_SEED);
}


typedef struct
{
    ma_vfs* pVFS;
    ma_vfs_file file;
} ma_decoder_callback_data_vfs;

static size_t ma_decoder_on_read__vfs(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead)
{
    ma_decoder_callback_data_vfs* pData = (ma_decoder_callback_data_vfs*)pDecoder->pUserData;
    size_t bytesRead;

    MA_ASSERT(pDecoder   != NULL);
    MA_ASSERT(pBufferOut != NULL);

    ma_vfs_read(pData->pVFS, pData->file, pBufferOut, bytesToRead, &bytesRead);
    return bytesRead;
}

static ma_bool32 ma_decoder_on_seek__vfs(ma_decoder* pDecoder, int offset, ma_seek_origin origin)
{
    ma_decoder_callback_data_vfs* pData = (ma_decoder_callback_data_vfs*)pDecoder->pUserData;
    ma_result result;

    MA_ASSERT(pDecoder != NULL);

    result = ma_vfs_seek(pData->pVFS, pData->file, offset, origin);
    if (result != MA_SUCCESS) {
        return MA_FALSE;
    }

    return MA_TRUE;
}

MA_API ma_result ma_decode_from_vfs(ma_vfs* pVFS, const char* pFilePath, ma_decoder_config* pConfig, ma_uint64* pFrameCountOut, void** ppPCMFramesOut)
{
    ma_result result;
    ma_decoder_config config;
    ma_decoder decoder;
    ma_decoder_callback_data_vfs data;

    if (pFrameCountOut != NULL) {
        *pFrameCountOut = 0;
    }
    if (ppPCMFramesOut != NULL) {
        *ppPCMFramesOut = NULL;
    }

    if (pVFS == NULL || pFilePath == NULL) {
        return MA_INVALID_ARGS;
    }

    data.pVFS = pVFS;

    result = ma_vfs_open(pVFS, pFilePath, MA_OPEN_MODE_READ, &data.file);
    if (result != MA_SUCCESS) {
        return result;
    }

    config = ma_decoder_config_init_copy(pConfig);

    result = ma_decoder_init(ma_decoder_on_read__vfs, ma_decoder_on_seek__vfs, &data, &config, &decoder);
    if (result != MA_SUCCESS) {
        ma_vfs_close(pVFS, data.file);
        return result;
    }

    result = ma_decoder__full_decode_and_uninit(&decoder, pConfig, pFrameCountOut, ppPCMFramesOut);
    ma_vfs_close(pVFS, data.file);

    return result;
}


MA_API ma_resource_manager_message ma_resource_manager_message_init(ma_uint32 code)
{
    ma_resource_manager_message message;
    
    MA_ZERO_OBJECT(&message);
    message.code = code;
    
    return message;
}

MA_API ma_result ma_resource_manager_message_queue_init(ma_resource_manager* pResourceManager, ma_resource_manager_message_queue* pQueue)
{
    ma_result result;

    if (pQueue == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pQueue);

    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We need a semaphore for blocking while there are no messages available. */
    result = ma_semaphore_init(0, &pQueue->sem);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize semaphore. */
    }

    /* Currently we're naively locking access to the queue using a mutex. It will be nice to make this lock-free later on. */
    result = ma_mutex_init(&pQueue->lock);
    if (result != MA_SUCCESS) {
        ma_semaphore_uninit(&pQueue->sem);
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_resource_manager_message_queue_uninit(ma_resource_manager_message_queue* pQueue)
{
    if (pQueue == NULL) {
        return;
    }

    ma_mutex_uninit(&pQueue->lock);
    ma_semaphore_uninit(&pQueue->sem);
}


static ma_uint32 ma_resource_manager_message_queue_get_count(ma_resource_manager_message_queue* pQueue)
{
    ma_uint32 getCursor;
    ma_uint32 getIndex;
    ma_uint32 getLoopFlag;
    ma_uint32 putCursor;
    ma_uint32 putIndex;
    ma_uint32 putLoopFlag;

    MA_ASSERT(pQueue != NULL);

    getCursor = pQueue->getCursor;
    putCursor = pQueue->putCursor;

    ma_rb__deconstruct_offset(getCursor, &getIndex, &getLoopFlag);
    ma_rb__deconstruct_offset(putCursor, &putIndex, &putLoopFlag);

    if (getLoopFlag == putLoopFlag) {
        return putIndex - getIndex;
    } else {
        return putIndex + (ma_countof(pQueue->messages) - getIndex);
    }
}

static ma_result ma_resource_manager_message_queue_post_nolock(ma_resource_manager_message_queue* pQueue, const ma_resource_manager_message* pMessage)
{
    ma_uint32 putIndex;
    ma_uint32 putLoopFlag;

    MA_ASSERT(pQueue != NULL);

    if (ma_resource_manager_message_queue_get_count(pQueue) == ma_countof(pQueue->messages)) {
        return MA_OUT_OF_MEMORY;    /* The queue is already full. */
    }

    ma_rb__deconstruct_offset(pQueue->putCursor, &putIndex, &putLoopFlag);

    pQueue->messages[putIndex] = *pMessage;

    /* Move the cursor forward. */
    putIndex += 1;
    if (putIndex > ma_countof(pQueue->messages)) {
        putIndex     = 0;
        putLoopFlag ^= 0x80000000;
    }

    ma_atomic_exchange_32(&pQueue->putCursor, ma_rb__construct_offset(putIndex, putLoopFlag));

    /* Now that the message is in the queue we can let the consumer thread know about it by releasing the semaphore. */
    ma_semaphore_release(&pQueue->sem);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_message_queue_post(ma_resource_manager_message_queue* pQueue, const ma_resource_manager_message* pMessage)
{
    ma_result result;

    if (pQueue == NULL || pMessage == NULL) {
        return MA_INVALID_ARGS;
    }

    /* This is the producer. There can be many producer threads, so a typical single-producer, single-consumer ring buffer will not work here. */
    if (ma_resource_manager_message_queue_get_count(pQueue) == ma_countof(pQueue->messages)) {
        return MA_OUT_OF_MEMORY;    /* The queue is already full. */
    }

    ma_mutex_lock(&pQueue->lock);
    {
        result = ma_resource_manager_message_queue_post_nolock(pQueue, pMessage);
    }
    ma_mutex_unlock(&pQueue->lock);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_message_queue_next(ma_resource_manager_message_queue* pQueue, ma_resource_manager_message* pMessage)
{
    ma_result result;
    ma_uint32 getIndex;
    ma_uint32 getLoopFlag;

    if (pQueue == NULL) {
        return MA_INVALID_ARGS;
    }

    /* This is the consumer. There is only ever a single consumer thread which means we have simplified lock-free requirements. */

    /* We first need to wait for a message. */
    result = ma_semaphore_wait(&pQueue->sem);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to retrieve a message. */
    }

    MA_ASSERT(ma_resource_manager_message_queue_get_count(pQueue) > 0);

    /* We have a message so now we need to copy it to the output buffer and increment the cursor. */
    ma_rb__deconstruct_offset(pQueue->getCursor, &getIndex, &getLoopFlag);

    *pMessage = pQueue->messages[getIndex];

    /* The cursor needs to be moved forward. */
    getIndex += 1;
    if (getIndex == ma_countof(pQueue->messages)) {
        getIndex     = 0;
        getLoopFlag ^= 0x80000000;
    }
    
    ma_atomic_exchange_32(&pQueue->getCursor, ma_rb__construct_offset(getIndex, getLoopFlag));

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_message_queue_peek(ma_resource_manager_message_queue* pQueue, ma_resource_manager_message* pMessage)
{
    ma_uint32 readIndex;
    ma_uint32 loopFlag;

    if (pQueue == NULL || pMessage == NULL) {
        return MA_INVALID_ARGS;
    }

    /* This should only ever be called by the consumer thread. If the count is greater than zero it won't ever be reduced which means it's safe to read the message. */
    if (ma_resource_manager_message_queue_get_count(pQueue) == 0) {
        MA_ZERO_OBJECT(pMessage);
        return MA_NO_DATA_AVAILABLE;
    }

    ma_rb__deconstruct_offset(pQueue->getCursor, &readIndex, &loopFlag);

    *pMessage = pQueue->messages[readIndex];

    return MA_SUCCESS;
}



/*
Basic BST Functions
*/
static ma_result ma_resource_manager_data_node_search(ma_resource_manager* pResourceManager, ma_uint32 hashedName32, ma_resource_manager_data_node** ppDataNode)
{
    ma_resource_manager_data_node* pCurrentNode;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(ppDataNode       != NULL);

    pCurrentNode = pResourceManager->pRootDataNode;
    while (pCurrentNode != NULL) {
        if (hashedName32 == pCurrentNode->hashedName32) {
            break;  /* Found. */
        } else if (hashedName32 < pCurrentNode->hashedName32) {
            pCurrentNode = pCurrentNode->pChildLo;
        } else {
            pCurrentNode = pCurrentNode->pChildHi;
        }
    }

    *ppDataNode = pCurrentNode;

    if (pCurrentNode == NULL) {
        return MA_DOES_NOT_EXIST;
    } else {
        return MA_SUCCESS;
    }
}

static ma_result ma_resource_manager_data_node_insert_point(ma_resource_manager* pResourceManager, ma_uint32 hashedName32, ma_resource_manager_data_node** ppInsertPoint)
{
    ma_result result = MA_SUCCESS;
    ma_resource_manager_data_node* pCurrentNode;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(ppInsertPoint    != NULL);

    *ppInsertPoint = NULL;

    if (pResourceManager->pRootDataNode == NULL) {
        return MA_SUCCESS;  /* No items. */
    }

    /* We need to find the node that will become the parent of the new node. If a node is found that already has the same hashed name we need to return MA_ALREADY_EXISTS. */
    pCurrentNode = pResourceManager->pRootDataNode;
    while (pCurrentNode != NULL) {
        if (hashedName32 == pCurrentNode->hashedName32) {
            result = MA_ALREADY_EXISTS;
            break;
        } else {
            if (hashedName32 < pCurrentNode->hashedName32) {
                if (pCurrentNode->pChildLo == NULL) {
                    result = MA_SUCCESS;
                    break;
                } else {
                    pCurrentNode = pCurrentNode->pChildLo;
                }
            } else {
                if (pCurrentNode->pChildHi == NULL) {
                    result = MA_SUCCESS;
                    break;
                } else {
                    pCurrentNode = pCurrentNode->pChildHi;
                }
            }
        }
    }

    *ppInsertPoint = pCurrentNode;
    return result;
}

static ma_result ma_resource_manager_data_node_insert_at(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode, ma_resource_manager_data_node* pInsertPoint)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataNode        != NULL);

    /* The key must have been set before calling this function. */
    MA_ASSERT(pDataNode->hashedName32 != 0);

    if (pInsertPoint == NULL) {
        /* It's the first node. */
        pResourceManager->pRootDataNode = pDataNode;
    } else {
        /* It's not the first node. It needs to be inserted. */
        if (pDataNode->hashedName32 < pInsertPoint->hashedName32) {
            MA_ASSERT(pInsertPoint->pChildLo == NULL);
            pInsertPoint->pChildLo = pDataNode;
        } else {
            MA_ASSERT(pInsertPoint->pChildHi == NULL);
            pInsertPoint->pChildHi = pDataNode;
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_node_insert(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode)
{
    ma_result result;
    ma_resource_manager_data_node* pInsertPoint;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataNode        != NULL);

    result = ma_resource_manager_data_node_insert_point(pResourceManager, pDataNode->hashedName32, &pInsertPoint);
    if (result != MA_SUCCESS) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_data_node_insert_at(pResourceManager, pDataNode, pInsertPoint);
}

static MA_INLINE ma_resource_manager_data_node* ma_resource_manager_data_node_find_min(ma_resource_manager_data_node* pDataNode)
{
    ma_resource_manager_data_node* pCurrentNode;

    MA_ASSERT(pDataNode != NULL);

    pCurrentNode = pDataNode;
    while (pCurrentNode->pChildLo != NULL) {
        pCurrentNode = pCurrentNode->pChildLo;
    }

    return pCurrentNode;
}

static MA_INLINE ma_resource_manager_data_node* ma_resource_manager_data_node_find_max(ma_resource_manager_data_node* pDataNode)
{
    ma_resource_manager_data_node* pCurrentNode;

    MA_ASSERT(pDataNode != NULL);

    pCurrentNode = pDataNode;
    while (pCurrentNode->pChildHi != NULL) {
        pCurrentNode = pCurrentNode->pChildHi;
    }

    return pCurrentNode;
}

static MA_INLINE ma_resource_manager_data_node* ma_resource_manager_data_node_find_inorder_successor(ma_resource_manager_data_node* pDataNode)
{
    MA_ASSERT(pDataNode           != NULL);
    MA_ASSERT(pDataNode->pChildHi != NULL);

    return ma_resource_manager_data_node_find_min(pDataNode->pChildHi);
}

static MA_INLINE ma_resource_manager_data_node* ma_resource_manager_data_node_find_inorder_predecessor(ma_resource_manager_data_node* pDataNode)
{
    MA_ASSERT(pDataNode           != NULL);
    MA_ASSERT(pDataNode->pChildLo != NULL);

    return ma_resource_manager_data_node_find_max(pDataNode->pChildLo);
}

static ma_result ma_resource_manager_data_node_remove(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataNode        != NULL);

    if (pDataNode->pChildLo == NULL) {
        if (pDataNode->pChildHi == NULL) {
            /* Simple case - deleting a node with no children. */
            if (pDataNode->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataNode == pDataNode);    /* There is only a single node in the tree which should be equal to the root node. */
                pResourceManager->pRootDataNode = NULL;
            } else {
                if (pDataNode->pParent->pChildLo == pDataNode) {
                    pDataNode->pParent->pChildLo = NULL;
                } else {
                    pDataNode->pParent->pChildHi = NULL;
                }
            }
        } else {
            /* Node has one child - pChildHi != NULL. */
            if (pDataNode->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataNode == pDataNode);
                pResourceManager->pRootDataNode = pDataNode->pChildHi;
            } else {
                if (pDataNode->pParent->pChildLo == pDataNode) {
                    pDataNode->pParent->pChildLo = pDataNode->pChildHi;
                } else {
                    pDataNode->pParent->pChildHi = pDataNode->pChildHi;
                }
            }
        }
    } else {
        if (pDataNode->pChildHi == NULL) {
            /* Node has one child - pChildLo != NULL. */
            if (pDataNode->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataNode == pDataNode);
                pResourceManager->pRootDataNode = pDataNode->pChildLo;
            } else {
                if (pDataNode->pParent->pChildLo == pDataNode) {
                    pDataNode->pParent->pChildLo = pDataNode->pChildLo;
                } else {
                    pDataNode->pParent->pChildHi = pDataNode->pChildLo;
                }
            }
        } else {
            /* Complex case - deleting a node with two children. */
            ma_resource_manager_data_node* pReplacementDataNode;

            /* For now we are just going to use the in-order successor as the replacement, but we may want to try to keep this balanced by switching between the two. */
            pReplacementDataNode = ma_resource_manager_data_node_find_inorder_successor(pDataNode);
            MA_ASSERT(pReplacementDataNode != NULL);

            /*
            Now that we have our replacement node we can make the change. The simple way to do this would be to just exchange the values, and then remove the replacement
            node, however we track specific nodes via pointers which means we can't just swap out the values. We need to instead just change the pointers around. The
            replacement node should have at most 1 child. Therefore, we can detach it in terms of our simpler cases above. What we're essentially doing is detaching the
            replacement node and reinserting it into the same position as the deleted node.
            */
            MA_ASSERT(pReplacementDataNode->pParent  != NULL);  /* The replacement node should never be the root which means it should always have a parent. */
            MA_ASSERT(pReplacementDataNode->pChildLo == NULL);  /* Because we used in-order successor. This would be pChildHi == NULL if we used in-order predecessor. */

            if (pReplacementDataNode->pChildHi == NULL) {
                if (pReplacementDataNode->pParent->pChildLo == pReplacementDataNode) {
                    pReplacementDataNode->pParent->pChildLo = NULL;
                } else {
                    pReplacementDataNode->pParent->pChildHi = NULL;
                }
            } else {
                if (pReplacementDataNode->pParent->pChildLo == pReplacementDataNode) {
                    pReplacementDataNode->pParent->pChildLo = pReplacementDataNode->pChildHi;
                } else {
                    pReplacementDataNode->pParent->pChildHi = pReplacementDataNode->pChildHi;
                }
            }


            /* The replacement node has essentially been detached from the binary tree, so now we need to replace the old data node with it. The first thing to update is the parent */
            if (pDataNode->pParent != NULL) {
                if (pDataNode->pParent->pChildLo == pDataNode) {
                    pDataNode->pParent->pChildLo = pReplacementDataNode;
                } else {
                    pDataNode->pParent->pChildHi = pReplacementDataNode;
                }
            }

            /* Now need to update the replacement node's pointers. */
            pReplacementDataNode->pParent  = pDataNode->pParent;
            pReplacementDataNode->pChildLo = pDataNode->pChildLo;
            pReplacementDataNode->pChildHi = pDataNode->pChildHi;

            /* Now the children of the replacement node need to have their parent pointers updated. */
            if (pReplacementDataNode->pChildLo != NULL) {
                pReplacementDataNode->pChildLo->pParent = pReplacementDataNode;
            }
            if (pReplacementDataNode->pChildHi != NULL) {
                pReplacementDataNode->pChildHi->pParent = pReplacementDataNode;
            }

            /* Now the root node needs to be updated. */
            if (pResourceManager->pRootDataNode == pDataNode) {
                pResourceManager->pRootDataNode = pReplacementDataNode;
            }
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_node_remove_by_key(ma_resource_manager* pResourceManager, ma_uint32 hashedName32)
{
    ma_result result;
    ma_resource_manager_data_node* pDataNode;

    result = ma_resource_manager_data_node_search(pResourceManager, hashedName32, &pDataNode);
    if (result != MA_SUCCESS) {
        return result;  /* Could not find the node. */
    }

    return ma_resource_manager_data_node_remove(pResourceManager, pDataNode);
}

static ma_result ma_resource_manager_data_node_increment_ref(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode, ma_uint32* pNewRefCount)
{
    ma_uint32 refCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataNode        != NULL);

    (void)pResourceManager;

    refCount = ma_atomic_increment_32(&pDataNode->refCount);

    if (pNewRefCount != NULL) {
        *pNewRefCount = refCount;
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_node_decrement_ref(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode, ma_uint32* pNewRefCount)
{
    ma_uint32 refCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataNode        != NULL);

    (void)pResourceManager;

    refCount = ma_atomic_decrement_32(pDataNode->refCount);

    if (pNewRefCount != NULL) {
        *pNewRefCount = refCount;
    }

    return MA_SUCCESS;
}





static ma_thread_result MA_THREADCALL ma_resource_manager_resource_thread(void* pUserData)
{
    ma_resource_manager* pResourceManager = (ma_resource_manager*)pUserData;
    MA_ASSERT(pResourceManager != NULL);

    for (;;) {
        ma_result result;
        ma_resource_manager_message message;

        result = ma_resource_manager_message_queue_next(&pResourceManager->messageQueue, &message);
        if (result != MA_SUCCESS) {
            break;
        }

        /* Terminate if we got a termination message. */
        if (message.code == MA_MESSAGE_TERMINATE) {
            break;
        }

        ma_resource_manager_handle_message(pResourceManager, &message);
    }

    return (ma_thread_result)0;
}



MA_API ma_resource_manager_config ma_resource_manager_config_init(ma_format decodedFormat, ma_uint32 decodedChannels, ma_uint32 decodedSampleRate, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_resource_manager_config config;

    MA_ZERO_OBJECT(&config);
    config.decodedFormat     = decodedFormat;
    config.decodedChannels   = decodedChannels;
    config.decodedSampleRate = decodedSampleRate;
    
    if (pAllocationCallbacks != NULL) {
        config.allocationCallbacks = *pAllocationCallbacks;
    }

    return config;
}


MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager)
{
    ma_result result;

    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pResourceManager);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pResourceManager->config = *pConfig;
    ma_allocation_callbacks_init_copy(&pResourceManager->config.allocationCallbacks, &pConfig->allocationCallbacks);

    if (pResourceManager->config.pVFS == NULL) {
        result = ma_default_vfs_init(&pResourceManager->defaultVFS);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to initialize the default file system. */
        }

        pResourceManager->config.pVFS = &pResourceManager->defaultVFS;
    }

    /* Data node lock. */
    result = ma_mutex_init(&pResourceManager->dataNodeLock);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* We need a message queue. */
    result = ma_resource_manager_message_queue_init(pResourceManager, &pResourceManager->messageQueue);
    if (result != MA_SUCCESS) {
        ma_mutex_uninit(&pResourceManager->dataNodeLock);
        return result;
    }


    /* Create the resource thread last to ensure the new thread has access to valid data. */
    result = ma_thread_create(&pResourceManager->asyncThread, ma_thread_priority_normal, ma_resource_manager_resource_thread, pResourceManager);
    if (result != MA_SUCCESS) {
        ma_mutex_uninit(&pResourceManager->dataNodeLock);
        ma_resource_manager_message_queue_uninit(&pResourceManager->messageQueue);
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager)
{
    if (pResourceManager == NULL) {
        return;
    }

    /* TODO: Need to delete all data nodes and free all of their memory. */
}


static ma_result ma_resource_manager_create_data_node_nolock(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 hashedName32, ma_resource_manager_data_node_type type, ma_resource_manager_memory_buffer* pExistingData, ma_event* pEvent, ma_resource_manager_data_node** ppDataNode)
{
    ma_result result;
    ma_resource_manager_data_node* pDataNode;
    ma_resource_manager_data_node* pInsertPoint;
    char* pFilePathCopy;    /* Allocated here, freed in the resource thread. */

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pFilePath        != NULL);
    MA_ASSERT(ppDataNode       != NULL);

    /*
    The first thing to do is find the insertion point. If it's already loaded it means we can just increment the reference counter and signal the event. Otherwise we
    need to do a full load.
    */
    result = ma_resource_manager_data_node_insert_point(pResourceManager, hashedName32, &pInsertPoint);
    if (result == MA_ALREADY_EXISTS) {
        /* Fast path. The node already exists. We just need to increment the reference counter and signal the event, if any. */
        pDataNode = pInsertPoint;

        result = ma_resource_manager_data_node_increment_ref(pResourceManager, pDataNode, NULL);
        if (result != MA_SUCCESS) {
            return result;  /* Should never happen. Failed to increment the reference count. */
        }

        if (pEvent != NULL) {
            ma_event_signal(pEvent);
        }
    } else {
        /* Slow path. The data for this node has not yet been initialized. The first thing to do is allocate the new data node and insert it into the BST. */
        pDataNode = ma__malloc_from_callbacks(sizeof(*pDataNode), &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_NODE*/);
        if (pDataNode == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        MA_ZERO_OBJECT(pDataNode);
        pDataNode->hashedName32 = hashedName32;
        pDataNode->refCount     = 1;        /* Always set to 1 by default (this is our first reference). */
        pDataNode->data.type    = type;
        pDataNode->result       = MA_BUSY;  /* I think it's good practice to set the status to MA_BUSY by default. */

        result = ma_resource_manager_data_node_insert_at(pResourceManager, pDataNode, pInsertPoint);
        if (result != MA_SUCCESS) {
            return result;  /* Should never happen. Failed to insert the data node into the BST. */
        }

        /*
        The new data node has been inserted into the BST, so now we need to fire an event to get everything loaded. If the data is owned by the caller (not
        owned by the resource manager) we don't need to load anything which means we're done.
        */
        if (pExistingData != NULL) {
            /* We don't need to do anything if the data is owned by the application except set the necessary data pointers. */
            MA_ASSERT(type == pExistingData->type);

            pDataNode->isDataOwnedByResourceManager = MA_FALSE;
            pDataNode->data = *pExistingData;
            pDataNode->result = MA_SUCCESS;
        } else {
            /* The data needs to be loaded. We do this by posting an event to the resource thread. */
            ma_resource_manager_message message;

            pDataNode->isDataOwnedByResourceManager = MA_TRUE;
            pDataNode->result = MA_BUSY;

            /* We need a copy of the file path. We should probably make this more efficient, but for now we'll do a transient memory allocation. */
            pFilePathCopy = ma_copy_string(pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
            if (pFilePathCopy == NULL) {
                ma_resource_manager_data_node_remove(pResourceManager, pDataNode);
                ma__free_from_callbacks(pDataNode, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_NODE*/);
                return MA_OUT_OF_MEMORY;
            }

            /* We now have everything we need to post the message to the resource thread. This is the last thing we need to do from here. The rest will be done by the resource thread. */
            message = ma_resource_manager_message_init(MA_MESSAGE_LOAD_DATA_NODE);
            message.loadDataNode.pDataNode = pDataNode;
            message.loadDataNode.pFilePath = pFilePathCopy;
            message.loadDataNode.pEvent    = pEvent;
            result = ma_resource_manager_post_message(pResourceManager, &message);
            if (result != MA_SUCCESS) {
                ma_resource_manager_data_node_remove(pResourceManager, pDataNode);
                ma__free_from_callbacks(pDataNode,     &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_NODE*/);
                ma__free_from_callbacks(pFilePathCopy, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
                return result;
            }
        }
    }

    MA_ASSERT(pDataNode != NULL);

    if (ppDataNode != NULL) {
        *ppDataNode = pDataNode;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_create_data_node(ma_resource_manager* pResourceManager, const char* pFilePath, ma_resource_manager_data_node_type type, ma_event* pEvent, ma_resource_manager_data_node** ppDataNode)
{
    ma_result result;
    ma_uint32 hashedName32;

    if (ppDataNode == NULL) {
        return MA_INVALID_ARGS;
    }

    *ppDataNode = NULL;

    if (pResourceManager == NULL || pFilePath == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Do as much set up before entering into the critical section to reduce our lock time as much as possible. */
    hashedName32 = ma_hash_string_32(pFilePath);

    /* At this point we can now enter the critical section. */
    ma_mutex_lock(&pResourceManager->dataNodeLock);
    {
        result = ma_resource_manager_create_data_node_nolock(pResourceManager, pFilePath, hashedName32, type, NULL, pEvent, ppDataNode);
    }
    ma_mutex_unlock(&pResourceManager->dataNodeLock);

    return result;
}


static ma_result ma_resource_manager_delete_data_node_nolock(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode)
{
    ma_uint32 result;
    ma_uint32 refCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataNode        != NULL);

    result = ma_resource_manager_data_node_decrement_ref(pResourceManager, pDataNode, &refCount);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* If the reference count has hit zero it means we need to delete the data node and it's backing data (so long as it's owned by the resource manager). */
    if (refCount == 0) {
        result = ma_resource_manager_data_node_remove(pResourceManager, pDataNode);
        if (result != MA_SUCCESS) {
            return result;  /* An error occurred when trying to remove the node. This should never happen. */
        }

        /*
        The data node has been removed from the BST so now we need to delete the underyling data. This needs to be done in a separate thread. We don't
        want to delete anything if the data is owned by the application. Also, just to be safe, we set the result to MA_UNAVAILABLE.
        */
        ma_atomic_exchange_32(&pDataNode->result, MA_UNAVAILABLE);

        /* Don't delete any underlying data if it's not owned by the resource manager. */
        if (pDataNode->isDataOwnedByResourceManager) {
            ma_resource_manager_message message = ma_resource_manager_message_init(MA_MESSAGE_FREE_DATA_NODE);
            message.freeDataNode.pDataNode = pDataNode;

            result = ma_resource_manager_post_message(pResourceManager, &message);
            if (result != MA_SUCCESS) {
                return result;  /* Failed to post the message for some reason. Probably due to too many messages in the queue. */
            }
        }
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_delete_data_node(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode)
{
    ma_result result;

    if (pResourceManager == NULL || pDataNode == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_mutex_lock(&pResourceManager->dataNodeLock);
    {
        result = ma_resource_manager_delete_data_node_nolock(pResourceManager, pDataNode);
    }
    ma_mutex_unlock(&pResourceManager->dataNodeLock);

    return result;
}

MA_API ma_result ma_resource_manager_data_node_result(ma_resource_manager* pResourceManager, const ma_resource_manager_data_node* pDataNode)
{
    if (pResourceManager == NULL || pDataNode == NULL) {
        return MA_INVALID_ARGS;
    }

    return pDataNode->result;
}


static ma_result ma_resource_manager_register_data(ma_resource_manager* pResourceManager, const char* pName, ma_resource_manager_data_node_type type, ma_resource_manager_memory_buffer* pExistingData, ma_event* pEvent, ma_resource_manager_data_node** ppDataNode)
{
    ma_result result = MA_SUCCESS;
    ma_uint32 hashedName32;

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    hashedName32 = ma_hash_string_32(pName);

    ma_mutex_lock(&pResourceManager->dataNodeLock);
    {
        result = ma_resource_manager_create_data_node_nolock(pResourceManager, pName, hashedName32, type, pExistingData, pEvent, ppDataNode);
    }
    ma_mutex_lock(&pResourceManager->dataNodeLock);
    return result;
}

MA_API ma_result ma_resource_manager_register_decoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_resource_manager_memory_buffer data;
    data.type               = ma_resource_manager_data_node_type_decoded;
    data.decoded.pData      = pData;
    data.decoded.frameCount = frameCount;
    data.decoded.format     = format;
    data.decoded.channels   = channels;
    data.decoded.sampleRate = sampleRate;

    return ma_resource_manager_register_data(pResourceManager, pName, data.type, &data, NULL, NULL);
}

MA_API ma_result ma_resource_manager_register_encoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, size_t sizeInBytes)
{
    ma_resource_manager_memory_buffer data;
    data.type                = ma_resource_manager_data_node_type_encoded;
    data.encoded.pData       = pData;
    data.encoded.sizeInBytes = sizeInBytes;

    return ma_resource_manager_register_data(pResourceManager, pName, data.type, &data, NULL, NULL);
}

MA_API ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName)
{
    ma_result result;
    ma_resource_manager_data_node* pDataNode;

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    /*
    It's assumed that the data specified by pName was registered with a prior call to ma_resource_manager_register_encoded/decoded_data(). To unregister it, all
    we need to do is delete the node by it's name.
    */
    ma_mutex_lock(&pResourceManager->dataNodeLock);
    {
        result = ma_resource_manager_data_node_search(pResourceManager, ma_hash_string_32(pName), &pDataNode);
    }
    ma_mutex_unlock(&pResourceManager->dataNodeLock);

    if (result != MA_SUCCESS) {
        return result;  /* Could not find the node. */
    }

    return ma_resource_manager_delete_data_node(pResourceManager, pDataNode);
}


#if 0
static ma_result ma_resource_manager_data_source_hold(ma_resource_manager_data_source* pDataSource)
{
    MA_ASSERT(pDataSource != NULL);

    /* Don't allow holding if the data source is being deleted. */
    if (pDataSource->result == MA_UNAVAILABLE) {
        return MA_UNAVAILABLE;
    }

    ma_atomic_increment_32(&pDataSource->holdCount);

    /* If while we were incrementing the hold count we became unavailble we need to abort. */
    if (pDataSource->result == MA_UNAVAILABLE) {
        ma_atomic_decrement_32(&pDataSource->holdCount);
        return MA_UNAVAILABLE;
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_source_release(ma_resource_manager_data_source* pDataSource)
{
    ma_uint32 newCount;

    MA_ASSERT(pDataSource != NULL);

    /* Note: Don't check for MA_UNAVAILABLE in this case because it's possible to be calling this just after the the data source has been uninitialized from another thread. */
    newCount = ma_atomic_decrement_32(&pDataSource->holdCount);
    if (newCount == 0xFFFFFFFF) {
        MA_ASSERT(MA_FALSE);    /* <-- If you hit this it means you have a hold/release mismatch. */
        ma_atomic_exchange_32(&pDataSource->holdCount, 0);
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}
#endif


static ma_data_source* ma_resource_manager_data_source_get_backing_data_source(ma_data_source* pDataSource)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    if (pRMDataSource->type == ma_resource_manager_data_source_type_buffer) {
        return &pRMDataSource->backend.buffer;
    } else {
        return &pRMDataSource->backend.decoder;
    }
}

static ma_result ma_resource_manager_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We don't do anything if we're busy. Returning MA_BUSY tells the mixer not to do anything with the data. */
    if (pRMDataSource->result == MA_BUSY) {
        return MA_BUSY;
    }

    /*
    Don't do anything if the data source has been deleted. This is not truely safe, but just here as an added layer of vague protection. Another thread can still
    be doing stuff on the data source.
    */
    if (pRMDataSource->result == MA_UNAVAILABLE) {
        return MA_UNAVAILABLE;
    }

    return ma_data_source_read_pcm_frames(ma_resource_manager_data_source_get_backing_data_source(pDataSource), pFramesOut, frameCount, pFramesRead, MA_FALSE);
}

static ma_result ma_resource_manager_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    if (pRMDataSource->result == MA_BUSY || pRMDataSource->result == MA_UNAVAILABLE) {
        return pRMDataSource->result;
    }

    return ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_source_get_backing_data_source(pDataSource), frameIndex);
}

static ma_result ma_resource_manager_data_source_map(ma_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    if (pRMDataSource->result == MA_BUSY || pRMDataSource->result == MA_UNAVAILABLE) {
        return pRMDataSource->result;
    }

    return ma_data_source_map(ma_resource_manager_data_source_get_backing_data_source(pDataSource), ppFramesOut, pFrameCount);
}

static ma_result ma_resource_manager_data_source_unmap(ma_data_source* pDataSource, ma_uint64 frameCount)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* NOTE: Don't do the same status checks here. If we were able to map, we want to unmap regardless of status. */

    return ma_data_source_unmap(ma_resource_manager_data_source_get_backing_data_source(pDataSource), frameCount);
}

static ma_result ma_resource_manager_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    if (pRMDataSource->result == MA_BUSY || pRMDataSource->result == MA_UNAVAILABLE) {
        return pRMDataSource->result;
    }

    return ma_data_source_get_data_format(ma_resource_manager_data_source_get_backing_data_source(pDataSource), pFormat, pChannels);
}


static ma_result ma_resource_manager_data_source_set_result_and_signal(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource, ma_result result, ma_event* pEvent)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataSource      != NULL);

    /* If the data source's status is MA_UNAVAILABLE it means it is being deleted. We don't ever want to move away from that state. */
    if (pDataSource->result != MA_UNAVAILABLE) {
        ma_atomic_exchange_32(&pDataSource->result, result);
    }

    /* If we have an event we want to signal it after setting the data source's status. */
    if (pEvent != NULL) {
        ma_event_signal(pEvent);
    }

    return result;
}

static ma_result ma_resource_manager_data_source_init_stream(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_resource_manager_data_source* pDataSource)
{
    (void)pResourceManager;
    (void)pName;
    (void)flags;
    (void)pDataSource;

    /* Streaming not yet implemented. */
    return MA_NOT_IMPLEMENTED;
}

static ma_result ma_resource_manager_data_source_init_backend_node(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource)
{
    ma_result result;
    ma_resource_manager_data_node* pDataNode;

    MA_ASSERT(pResourceManager       != NULL);
    MA_ASSERT(pDataSource            != NULL);
    MA_ASSERT(pDataSource->pDataNode != NULL);

    pDataNode = pDataSource->pDataNode;

    /* The underlying data node must be initialized before we'll be able to know how to initialize the backend. */
    result = ma_resource_manager_data_node_result(pResourceManager, pDataNode);
    if (result != MA_SUCCESS) {
        return result;  /* The data node is not initialized. */
    }

    /*
    We need to initialize either a ma_decoder or an ma_audio_buffer depending on whether or not the backing data is encoded or decoded. These act as the
    "instance" to the data and are used to form the connection between underlying data buffer and the data source. If the data buffer is decoded, we can use
    an ma_audio_buffer if the data format is identical to the primary format. This enables us to use memory mapping when mixing which saves us a bit of data
    movement overhead.
    */
    if (pDataNode->data.type == ma_resource_manager_data_node_type_decoded) {
        if (pDataNode->data.decoded.format     == pResourceManager->config.decodedFormat   &&
            pDataNode->data.decoded.sampleRate == pResourceManager->config.decodedSampleRate) {
            pDataSource->type = ma_resource_manager_data_source_type_buffer;
        } else {
            pDataSource->type = ma_resource_manager_data_source_type_decoder;
        }
    } else {
        pDataSource->type = ma_resource_manager_data_source_type_decoder;
    }

    if (pDataSource->type == ma_resource_manager_data_source_type_buffer) {
        ma_audio_buffer_config config;
        config = ma_audio_buffer_config_init(pDataNode->data.decoded.format, pDataNode->data.decoded.channels, pDataNode->data.decoded.frameCount, pDataNode->data.encoded.pData, NULL);
        result = ma_audio_buffer_init(&config, &pDataSource->backend.buffer);
    } else {
        ma_decoder_config configIn;
        ma_decoder_config configOut;

        configIn  = ma_decoder_config_init(pDataNode->data.decoded.format, pDataNode->data.decoded.channels, pDataNode->data.decoded.sampleRate);
        configOut = ma_decoder_config_init(pResourceManager->config.decodedFormat, pDataNode->data.decoded.channels, pResourceManager->config.decodedSampleRate);  /* <-- Never perform channel conversion at this level - that will be done at a higher level. */

        if (pDataNode->data.type == ma_resource_manager_data_node_type_decoded) {
            ma_uint64 sizeInBytes = pDataNode->data.decoded.frameCount * ma_get_bytes_per_frame(configIn.format, configIn.channels);
            if (sizeInBytes > MA_SIZE_MAX) {
                result = MA_TOO_BIG;
            } else {
                result = ma_decoder_init_memory_raw(pDataNode->data.decoded.pData, (size_t)sizeInBytes, &configIn, &configOut, &pDataSource->backend.decoder);  /* Safe cast thanks to the check above. */
            }
        } else {
            configOut.allocationCallbacks = pResourceManager->config.allocationCallbacks;
            result = ma_decoder_init_memory(pDataNode->data.encoded.pData, pDataNode->data.encoded.sizeInBytes, &configOut, &pDataSource->backend.decoder);
        }
    }

    /*
    We can only do mapping if the data source's backend is an audio buffer. If it's not, clear out the callbacks thereby preventing the mixer from attempting
    memory map mode, only to fail.
    */
    if (pDataSource->type != ma_resource_manager_data_source_type_buffer) {
        pDataSource->ds.onMap   = NULL;
        pDataSource->ds.onUnmap = NULL;
    }
    
    /* At this point the backend should be initialized. We do *not* want to set pDataSource->result here - that needs to be done at a higher level to ensure it's done as the last step. */
    return result;
}

static ma_result ma_resource_manager_data_source_uninit_backend_node(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource)
{
    MA_ASSERT(pResourceManager       != NULL);
    MA_ASSERT(pDataSource            != NULL);
    MA_ASSERT(pDataSource->pDataNode != NULL);

    if (pDataSource->type == ma_resource_manager_data_source_type_decoder) {
        ma_decoder_uninit(&pDataSource->backend.decoder);
    } else {
        ma_audio_buffer_uninit(&pDataSource->backend.buffer);
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_source_init_node(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_resource_manager_data_source* pDataSource)
{
    ma_result result;
    ma_result dataNodeResult;
    ma_resource_manager_data_node* pDataNode;
    ma_resource_manager_data_node_type dataNodeType;
    ma_event* pEvent = NULL;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pName            != NULL);
    MA_ASSERT(pDataSource      != NULL);

    /* The first thing we need to do is acquire a node. */
    if ((flags & MA_DATA_SOURCE_FLAG_DECODE) != 0) {
        dataNodeType = ma_resource_manager_data_node_type_decoded;
    } else {
        dataNodeType = ma_resource_manager_data_node_type_encoded;
    }

    result = ma_resource_manager_create_data_node(pResourceManager, pName, dataNodeType, NULL, &pDataNode);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to acquire the data node. */
    }

    /* At this point we have our data node and we can start initializing the data source. */
    pDataSource->ds.onRead          = ma_resource_manager_data_source_read;
    pDataSource->ds.onSeek          = ma_resource_manager_data_source_seek;
    pDataSource->ds.onMap           = ma_resource_manager_data_source_map;
    pDataSource->ds.onUnmap         = ma_resource_manager_data_source_unmap;
    pDataSource->ds.onGetDataFormat = ma_resource_manager_data_source_get_data_format;
    pDataSource->pDataNode          = pDataNode;
    pDataSource->type               = ma_resource_manager_data_source_type_unknown; /* The backend type hasn't been determine yet - that happens when it's initialized properly by the resource thread. */
    pDataSource->result             = MA_BUSY;

    /*
    If the data node has been fully initialized we can complete initialization of the data source now. Otherwise we need to post an event to the resource thread to complete
    initialization to ensure it's done after the data node.
    */
    dataNodeResult = ma_resource_manager_data_node_result(pResourceManager, pDataNode);
    if (dataNodeResult == MA_BUSY) {
        /* The data node is in the middle of loading. We need to post an event to the resource thread. */
        ma_resource_manager_message message;

        if ((flags & MA_DATA_SOURCE_FLAG_ASYNC) == 0) {
            result = ma_event_alloc_and_init(&pEvent, &pResourceManager->config.allocationCallbacks);
            if (result != MA_SUCCESS) {
                ma_resource_manager_delete_data_node(pResourceManager, pDataNode);
                return result;
            }
        } else {
            pEvent = NULL;
        }

        message = ma_resource_manager_message_init(MA_MESSAGE_LOAD_DATA_SOURCE);
        message.loadDataSource.pDataSource = pDataSource;
        message.loadDataSource.pEvent      = pEvent;
        result = ma_resource_manager_post_message(pResourceManager, &message);
        if (result != MA_SUCCESS) {
            if (pEvent != NULL) {
                ma_event_uninit_and_free(pEvent, &pResourceManager->config.allocationCallbacks);
            }

            ma_resource_manager_delete_data_node(pResourceManager, pDataNode);
            return result;
        }

        /* The message has been posted. We now need to wait for the event to get signalled if we're in synchronous mode. */
        if (pEvent != NULL) {
            ma_event_wait(pEvent);
            ma_event_uninit_and_free(message.pEvent, &pResourceManager->config.allocationCallbacks);
        
            /* Check the status of the data node for any errors. Even in the event of an error, the data source will not be deleted. */
            if (pDataNode->result != MA_SUCCESS) {
                result = pDataNode->result;
                ma_resource_manager_delete_data_node(pResourceManager, pDataNode);
                return result;
            }
        }

        return MA_SUCCESS;
    } else if (dataNodeResult == MA_SUCCESS) {
        /* The underlying data node has already been initialized so we can just complete initialization of the data source right now. */
        result = ma_resource_manager_data_source_init_backend_node(pResourceManager, pDataSource);
        if (result != MA_SUCCESS) {
            ma_resource_manager_delete_data_node(pResourceManager, pDataNode);
            return result;
        }

        ma_atomic_exchange_32(&pDataSource->result, MA_SUCCESS);
        return MA_SUCCESS;
    } else {
        /* Some other error has occurred with the data node. Lets abandon everything and return the data node's result. */
        ma_resource_manager_delete_data_node(pResourceManager, pDataNode);
        return dataNodeResult;
    }
}

MA_API ma_result ma_resource_manager_data_source_init(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_resource_manager_data_source* pDataSource)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataSource);

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_source_init_stream(pResourceManager, pName, flags, pDataSource);
    } else {
        return ma_resource_manager_data_source_init_node(pResourceManager, pName, flags, pDataSource);
    }
}

MA_API ma_result ma_resource_manager_data_source_uninit(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource)
{
    /*
    We need to run this synchronously because the caller owns the data source and we can't return before it's been fully uninitialized. We do, however, need to do
    the actual uninitialization on the resource thread for order-of-operations reasons.
    */

    /* We need to wait to finish loading before we try uninitializing. */
    while (pDataSource->result == MA_BUSY) {
        ma_yield();
    }

    /* The first thing to do is to mark the data source as unavailable. This will stop other threads from acquiring a hold on the data source which is what happens in the callbacks. */
    ma_atomic_exchange_32(&pDataSource->result, MA_UNAVAILABLE);

#if 0
    /* Wait for everything to release the data source. This should be a short hold so just spin. The audio thread might be in the middle of reading data from the data source. */
    while (pDataSource->holdCount > 0) {
        ma_yield();
    }
#endif

    /* We should uninitialize the data source's backend before deleting the node just to keep the order of operations clean. */
    ma_resource_manager_data_source_uninit_backend_node(pResourceManager, pDataSource);
    pDataSource->type = ma_resource_manager_data_source_type_unknown;

    /* The data node needs to be deleted. */
    if (pDataSource->pDataNode != NULL) {
        ma_resource_manager_delete_data_node(pResourceManager, pDataSource->pDataNode);
        pDataSource->pDataNode = NULL;
    }

    return MA_SUCCESS;
}



static ma_result ma_resource_manager_handle_message__load_data_node(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode, char* pFileName, ma_event* pEvent)
{
    ma_result result;

    MA_ASSERT(pResourceManager != NULL);

    if (pDataNode == NULL || pFileName == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataNode->result != MA_BUSY) {
        return MA_INVALID_OPERATION;    /* The data node may be getting deleted before it's even been loaded. */
    }

    MA_ASSERT(pDataNode->isDataOwnedByResourceManager == MA_TRUE);  /* The data should always be owned by the resource manager. */

    if (pDataNode->data.type == ma_resource_manager_data_node_type_encoded) {
        /* No decoding. Just store the file contents in memory. */
        void* pData;
        size_t sizeInBytes;

        result = ma_vfs_open_and_read_file_ex(pResourceManager->config.pVFS, pFileName, &pData, &sizeInBytes, &pResourceManager->config.allocationCallbacks, MA_ALLOCATION_TYPE_ENCODED_BUFFER);
        if (result == MA_SUCCESS) {
            pDataNode->data.encoded.pData       = pData;
            pDataNode->data.encoded.sizeInBytes = sizeInBytes;
        }
    } else  {
        /* Decoding. */
        ma_decoder_config config;
        void* pData;
        ma_uint64 frameCount;

        config = ma_decoder_config_init(pResourceManager->config.decodedFormat, 0, pResourceManager->config.decodedSampleRate); /* Need to keep the native channel count because we'll be using that for spatialization. */
        config.allocationCallbacks = pResourceManager->config.allocationCallbacks;

        result = ma_decode_from_vfs(pResourceManager->config.pVFS, pFileName, &config, &frameCount, &pData);
        if (result == MA_SUCCESS) {
            pDataNode->data.decoded.pData      = pData;
            pDataNode->data.decoded.frameCount = frameCount;
            pDataNode->data.decoded.format     = config.format;
            pDataNode->data.decoded.channels   = config.channels;
            pDataNode->data.decoded.sampleRate = config.sampleRate;
        }
    }

    ma__free_from_callbacks(pFileName, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
    if (result != MA_SUCCESS) {
        return result;
    }

    /*
    We need to set the result to MA_SUCCESS at the very end to ensure no other threads try reading the data before we've fully initialized the object. Other threads
    are going to be inspecting this variable to determine whether or not they're ready to read data.
    */
    ma_atomic_exchange_32(&pDataNode->result, MA_SUCCESS);

    /* Only signal the other threads after the result has been set just for cleanliness sake. */
    if (pEvent != NULL) {
        ma_event_signal(pEvent);
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_handle_message__free_data_node(ma_resource_manager* pResourceManager, ma_resource_manager_data_node* pDataNode)
{
    if (pDataNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pDataNode->result == MA_UNAVAILABLE);

    if (pDataNode->data.type == ma_resource_manager_data_node_type_encoded) {
        ma__free_from_callbacks((void*)pDataNode->data.encoded.pData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_ENCODED_BUFFER*/);
        pDataNode->data.encoded.pData       = NULL;
        pDataNode->data.encoded.sizeInBytes = 0;
    } else {
        ma__free_from_callbacks((void*)pDataNode->data.decoded.pData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
        pDataNode->data.decoded.pData       = NULL;
        pDataNode->data.decoded.frameCount  = 0;
    }

    /* The data node itself needs to be freed. */
    ma__free_from_callbacks(pDataNode, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_NODE*/);

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_handle_message__load_data_source(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource, ma_event* pEvent)
{
    ma_result dataNodeResult;

    MA_ASSERT(pResourceManager       != NULL);
    MA_ASSERT(pDataSource            != NULL);
    MA_ASSERT(pDataSource->pDataNode != NULL);
    MA_ASSERT(pDataSource->result == MA_BUSY || pDataSource->result == MA_UNAVAILABLE);

    if (pDataSource->result == MA_UNAVAILABLE) {
        /*
        The data source is getting deleted before it's even been loaded. We want to continue loading in this case because in the queue we'll have a
        corresponding MA_MESSAGE_FREE_DATA_SOURCE which will be doing the opposite. By letting it continue we can simplify the implementation because
        otherwise we'd need to keep track of a separate bit of state to track whether or not the backend has been initialized or not.
        */
    }

    /* We shouldn't attempt to load anything if the data node is in an erroneous state. */
    dataNodeResult = ma_resource_manager_data_node_result(pResourceManager, pDataSource->pDataNode);
    if (dataNodeResult != MA_SUCCESS && dataNodeResult != MA_BUSY) {
        return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, dataNodeResult, pEvent);
    }

    if (pDataSource->pDataNode->data.type == ma_resource_manager_data_node_type_encoded) {
        if (pDataSource->pDataNode->data.encoded.pData == NULL) {
            /* Something has gone badly wrong - no data is available from the data node, but it's not in an erroneous state (checked above). */
            MA_ASSERT(MA_FALSE);
            return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, MA_NO_DATA_AVAILABLE, pEvent);
        }
    } else {
        if (pDataSource->pDataNode->data.decoded.pData == NULL) {
            /* Something has gone badly wrong - no data is available from the data node, but it's not in an erroneous state (checked above). */
            MA_ASSERT(MA_FALSE);
            return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, MA_NO_DATA_AVAILABLE, pEvent);
        }
    }

    return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, ma_resource_manager_data_source_init_backend_node(pResourceManager, pDataSource), pEvent);
}


MA_API ma_result ma_resource_manager_handle_message(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage)
{
    if (pResourceManager == NULL || pMessage == NULL) {
        return MA_INVALID_ARGS;
    }

    switch (pMessage->code)
    {
        case MA_MESSAGE_LOAD_DATA_NODE:
        {
            return ma_resource_manager_handle_message__load_data_node(pResourceManager, pMessage->loadDataNode.pDataNode, pMessage->loadDataNode.pFilePath, pMessage->loadDataNode.pEvent);
        } break;

        case MA_MESSAGE_FREE_DATA_NODE:
        {
            return ma_resource_manager_handle_message__free_data_node(pResourceManager, pMessage->freeDataNode.pDataNode);
        } break;


        case MA_MESSAGE_LOAD_DATA_SOURCE:
        {
            return ma_resource_manager_handle_message__load_data_source(pResourceManager, pMessage->loadDataSource.pDataSource, pMessage->loadDataSource.pEvent);
        } break;

    #if 0
        case MA_MESSAGE_FREE_DATA_SOURCE:
        {
            return ma_resource_manager_handle_message__free_data_source(pResourceManager, pMessage->freeDataSource.pDataSource);
        } break;
    #endif

        default: break;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_post_message(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage)
{
    if (pResourceManager == NULL || pMessage == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_message_queue_post(&pResourceManager->messageQueue, pMessage);
}




MA_API ma_panner_config ma_panner_config_init(ma_format format, ma_uint32 channels)
{
    ma_panner_config config;

    MA_ZERO_OBJECT(&config);
    config.format   = format;
    config.channels = channels;
    config.mode     = ma_pan_mode_balance;  /* Set to balancing mode by default because it's consistent with other audio engines and most likely what the caller is expecting. */
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
    pPanner->mode     = pConfig->mode;
    pPanner->pan      = pConfig->pan;

    return MA_SUCCESS;
}



static void ma_stereo_balance_pcm_frames_f32(float* pFramesOut, const float* pFramesIn, ma_uint64 frameCount, float pan)
{
    ma_uint64 iFrame;

    if (pan > 0) {
        float factor = 1.0f - pan;
        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            pFramesOut[iFrame*2 + 0] = pFramesIn[iFrame*2 + 0] * factor;
        }
    } else {
        float factor = 1.0f + pan;
        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            pFramesOut[iFrame*2 + 1] = pFramesIn[iFrame*2 + 1] * factor;
        }
    }
}

static void ma_stereo_balance_pcm_frames(void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount, ma_format format, float pan)
{
    if (pan == 0) {
        /* Fast path. No panning required. */
        if (pFramesOut == pFramesIn) {
            /* No-op */
        } else {
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, format, 2);
        }
    }

    switch (format) {
        case ma_format_f32: ma_stereo_balance_pcm_frames_f32(pFramesOut, pFramesIn, frameCount, pan); break;

        /* Unknown format. Just copy. */
        default:
        {
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, format, 2);
        } break;
    }
}


static void ma_stereo_pan_pcm_frames_f32(float* pFramesOut, const float* pFramesIn, ma_uint64 frameCount, float pan)
{
    ma_uint64 iFrame;

    if (pan > 0) {
        float factorL0 = 1.0f - pan;
        float factorL1 = 0.0f + pan;

        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            float sample0 = (pFramesIn[iFrame*2 + 0] * factorL0);
            float sample1 = (pFramesIn[iFrame*2 + 0] * factorL1) + pFramesIn[iFrame*2 + 1];

            pFramesOut[iFrame*2 + 0] = sample0;
            pFramesOut[iFrame*2 + 1] = sample1;
        }
    } else {
        float factorR0 = 0.0f - pan;
        float factorR1 = 1.0f + pan;

        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            float sample0 = pFramesIn[iFrame*2 + 0] + (pFramesIn[iFrame*2 + 1] * factorR0);
            float sample1 =                           (pFramesIn[iFrame*2 + 1] * factorR1);

            pFramesOut[iFrame*2 + 0] = sample0;
            pFramesOut[iFrame*2 + 1] = sample1;
        }
    }
}

static void ma_stereo_pan_pcm_frames(void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount, ma_format format, float pan)
{
    if (pan == 0) {
        /* Fast path. No panning required. */
        if (pFramesOut == pFramesIn) {
            /* No-op */
        } else {
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, format, 2);
        }
    }

    switch (format) {
        case ma_format_f32: ma_stereo_pan_pcm_frames_f32(pFramesOut, pFramesIn, frameCount, pan); break;

        /* Unknown format. Just copy. */
        default:
        {
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, format, 2);
        } break;
    }
}

MA_API ma_result ma_panner_process_pcm_frames(ma_panner* pPanner, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    if (pPanner == NULL || pFramesOut == NULL || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pPanner->channels == 2) {
        /* Stereo case. For now assume channel 0 is left and channel right is 1, but should probably add support for a channel map. */
        if (pPanner->mode == ma_pan_mode_balance) {
            ma_stereo_balance_pcm_frames(pFramesOut, pFramesIn, frameCount, pPanner->format, pPanner->pan);
        } else {
            ma_stereo_pan_pcm_frames(pFramesOut, pFramesIn, frameCount, pPanner->format, pPanner->pan);
        }
    } else {
        if (pPanner->channels == 1) {
            /* Panning has no effect on mono streams. */
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, pPanner->format, pPanner->channels);
        } else {
            /* For now we're not going to support non-stereo set ups. Not sure how I want to handle this case just yet. */
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, pPanner->format, pPanner->channels);
        }
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_panner_set_mode(ma_panner* pPanner, ma_pan_mode mode)
{
    if (pPanner == NULL) {
        return MA_INVALID_ARGS;
    }

    pPanner->mode = mode;

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
                result = ma_data_source_seek_pcm_frames(pSound->pDataSource, frameCount, NULL, pSound->isLooping);
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

MA_API ma_result ma_engine_set_gain_db(ma_engine* pEngine, float gainDB)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_device_set_master_gain_db(&pEngine->listener.device, gainDB);
}


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
    converterConfig.resampling.linear.lpfOrder        = 0;

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

static ma_result ma_engine_sound_init_from_data_source_internal(ma_engine* pEngine, ma_data_source* pDataSource, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound)
{
    ma_result result;

    (void)flags;

    if (pEngine == NULL || pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Do no clear pSound to zero. Otherwise it may overwrite some members we set earlier. */

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

#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_engine_sound_init_from_file(ma_engine* pEngine, const char* pFilePath, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound)
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
    result = ma_resource_manager_data_source_init(pEngine->pResourceManager, pFilePath, flags, &pSound->resourceManagerDataSource);
    if (result != MA_SUCCESS) {
        return result;
    }

    pDataSource = &pSound->resourceManagerDataSource;

    /* Now that we have our data source we can create the sound using our generic function. */
    result = ma_engine_sound_init_from_data_source_internal(pEngine, pDataSource, flags, pGroup, pSound);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* We need to tell the engine that we own the data source so that it knows to delete it when deleting the sound. This needs to be done after creating the sound with ma_engine_create_sound_from_data_source(). */
    pSound->ownsDataSource = MA_TRUE;

    return MA_SUCCESS;
}
#endif

MA_API ma_result ma_engine_sound_init_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pSound);

    return ma_engine_sound_init_from_data_source_internal(pEngine, pDataSource, flags, pGroup, pSound);
}

MA_API void ma_engine_sound_uninit(ma_engine* pEngine, ma_sound* pSound)
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
        ma_resource_manager_data_source_uninit(pEngine->pResourceManager, &pSound->resourceManagerDataSource);
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

MA_API ma_result ma_engine_sound_set_gain_db(ma_engine* pEngine, ma_sound* pSound, float gainDB)
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

MA_API ma_result ma_engine_sound_set_pan(ma_engine* pEngine, ma_sound* pSound, float pan)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_panner_set_pan(&pSound->effect.panner, pan);
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

MA_API ma_result ma_engine_sound_set_effect(ma_engine* pEngine, ma_sound* pSound, ma_effect* pEffect)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->effect.pPreEffect = pEffect;

    return MA_SUCCESS;
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

        /* The at-end flag should have been set to false when we acquired the sound for recycling. */
        MA_ASSERT(pSound->atEnd == MA_FALSE);

        /* We're just going to reuse the same data source as before so we need to make sure we uninitialize the old one first. */
        if (pSound->pDataSource != NULL) {  /* <-- Safety. Should never happen. */
            MA_ASSERT(pSound->ownsDataSource == MA_TRUE);
            ma_resource_manager_data_source_uninit(pEngine->pResourceManager, &pSound->resourceManagerDataSource);
        }

        /* The old data source has been uninitialized so now we need to initialize the new one. */
        result = ma_resource_manager_data_source_init(pEngine->pResourceManager, pFilePath, 0, &pSound->resourceManagerDataSource);
        if (result != MA_SUCCESS) {
            /* We failed to load the resource. We need to return an error. We must also put this sound back up for recycling by setting the at-end flag to true. */
            ma_atomic_exchange_32(&pSound->atEnd, MA_TRUE); /* <-- Put the sound back up for recycling. */
            return result;
        }

        /* Set the data source again. It should always be set to the correct value but just set it again for completeness and consistency with the main init API. */
        pSound->pDataSource = &pSound->resourceManagerDataSource;

        /* We need to reset the effect. */
        result = ma_engine_effect_reinit(pEngine, &pSound->effect);
        if (result != MA_SUCCESS) {
            /* We failed to reinitialize the effect. The sound is currently in a bad state and we need to delete it and return an error. Should never happen. */
            ma_engine_sound_uninit(pEngine, pSound);
            return result;
        }
    } else {
        /* There's no available sounds for recycling. We need to allocate a sound. This can be done using a stack allocator. */
        pSound = ma__malloc_from_callbacks(sizeof(*pSound), &pEngine->allocationCallbacks/*, MA_ALLOCATION_TYPE_SOUND*/); /* TODO: This can certainly be optimized. */
        if (pSound == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        result = ma_engine_sound_init_from_file(pEngine, pFilePath, 0, pGroup, pSound);
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
    result = ma_mutex_init(&pGroup->lock);
    if (result != MA_SUCCESS) {
        ma_engine_sound_group_detach(pEngine, pGroup);
        ma_mixer_uninit(&pGroup->mixer);
        return result;
    }

    /* The group needs to be started by default, but needs to be done after attaching to the internal list. */
    pGroup->isPlaying = MA_TRUE;

    return MA_SUCCESS;
}

static void ma_engine_sound_group_uninit_all_internal_sounds(ma_engine* pEngine, ma_sound_group* pGroup)
{
    ma_sound* pCurrentSound;

    /* We need to be careful here that we keep our iteration valid. */
    pCurrentSound = pGroup->pFirstSoundInGroup;
    while (pCurrentSound != NULL) {
        ma_sound* pSoundToDelete = pCurrentSound;
        pCurrentSound = pCurrentSound->pNextSoundInGroup;

        if (pSoundToDelete->_isInternal) {
            ma_engine_sound_uninit(pEngine, pSoundToDelete);
        }
    }
}

MA_API void ma_engine_sound_group_uninit(ma_engine* pEngine, ma_sound_group* pGroup)
{
    ma_result result;

    result = ma_engine_sound_group_stop(pEngine, pGroup);
    if (result != MA_SUCCESS) {
        MA_ASSERT(MA_FALSE);    /* Should never happen. Trigger an assert for debugging, but don't stop uninitializing in production to ensure we free memory down below. */
    }

    /* Any in-place sounds need to be uninitialized. */
    ma_engine_sound_group_uninit_all_internal_sounds(pEngine, pGroup);

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
