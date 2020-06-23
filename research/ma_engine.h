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
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER 0x00000010  /* A ma_resource_manager_data_buffer object. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_STREAM 0x00000011  /* A ma_resource_manager_data_stream object. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_SOURCE 0x00000012  /* A ma_resource_manager_data_source object. */

/*
Resource Manager Data Source Flags
==================================
The flags below are used for controlling how the resource manager should handle the loading and caching of data sources.
*/
#define MA_DATA_SOURCE_FLAG_DECODE  0x00000001  /* Decode data before storing in memory. When set, decoding is done at the resource manager level rather than the mixing thread. Results in faster mixing, but higher memory usage. */
#define MA_DATA_SOURCE_FLAG_STREAM  0x00000002  /* When set, does not load the entire data source in memory. Disk I/O will happen on the resource manager thread. */
#define MA_DATA_SOURCE_FLAG_ASYNC   0x00000004  /* When set, the resource manager will load the data source asynchronously. */



typedef enum
{
    ma_resource_manager_data_buffer_encoding_encoded,
    ma_resource_manager_data_buffer_encoding_decoded
} ma_resource_manager_data_buffer_encoding;

/* The type of object that's used to connect a data buffer to a data source. */
typedef enum
{
    ma_resource_manager_data_buffer_connector_unknown,
    ma_resource_manager_data_buffer_connector_decoder,  /* ma_decoder */
    ma_resource_manager_data_buffer_connector_buffer    /* ma_audio_buffer */
} ma_resource_manager_data_buffer_connector;


typedef struct ma_resource_manager             ma_resource_manager;
typedef struct ma_resource_manager_data_buffer ma_resource_manager_data_buffer;
typedef struct ma_resource_manager_data_stream ma_resource_manager_data_stream;
typedef struct ma_resource_manager_data_source ma_resource_manager_data_source;





/* TODO: May need to do some stress testing and tweak this. */
/* The job queue capacity must be a multiple of 32. */
#ifndef MA_RESOURCE_MANAGER_MESSAGE_QUEUE_CAPACITY
#define MA_RESOURCE_MANAGER_MESSAGE_QUEUE_CAPACITY  1024
#endif

#define MA_MESSAGE_TERMINATE          0x00000000
#define MA_MESSAGE_LOAD_DATA_BUFFER   0x00000001
#define MA_MESSAGE_FREE_DATA_BUFFER   0x00000002
#define MA_MESSAGE_LOAD_DATA_STREAM   0x00000003
#define MA_MESSAGE_FREE_DATA_STREAM   0x00000004
#define MA_MESSAGE_LOAD_DATA_SOURCE   0x00000005
/*#define MA_MESSAGE_FREE_DATA_SOURCE   0x00000006*/
#define MA_MESSAGE_DECODE_BUFFER_PAGE 0x00000007
#define MA_MESSAGE_DECODE_STREAM_PAGE 0x00000008
#define MA_MESSAGE_SEEK_DATA_STREAM   0x00000009


/*
The idea of the slot allocator is for it to be used in conjunction with a fixed sized buffer. You use the slot allocator to allocator an index that can be used
as the insertion point for an object. This is lock-free.
*/
typedef struct
{
    struct
    {
        ma_uint32 bitfield;
        /*ma_uint32 refcount;*/ /* When greater than 0 it means something already has a hold on this group. */
    } groups[MA_RESOURCE_MANAGER_MESSAGE_QUEUE_CAPACITY/32];
    ma_uint32 counter;
} ma_slot_allocator;

MA_API ma_result ma_slot_allocator_init(ma_slot_allocator* pAllocator);
MA_API ma_result ma_slot_allocator_alloc(ma_slot_allocator* pAllocator, ma_uint32* pSlot);
MA_API ma_result ma_slot_allocator_alloc_16(ma_slot_allocator* pAllocator, ma_uint16* pSlot);
MA_API ma_result ma_slot_allocator_free(ma_slot_allocator* pAllocator, ma_uint32 slot);


typedef struct
{
    ma_uint16 code;
    ma_uint16 slot;     /* Internal use only. */
    ma_uint16 next;     /* Internal use only. The slot of the next job in the list. Set to 0xFFFF if this is the last item. */
    ma_uint16 padding;
    union
    {
        /* Resource Managemer Jobs */
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            char* pFilePath;
            ma_event* pEvent;
        } loadDataBuffer;
    };
} ma_job;

MA_API ma_job ma_job_init(ma_uint16 code);


#define MA_JOB_QUEUE_ASYNC  0x00000001  /* When set, ma_job_queue_next() will not wait and no semaphore will be signaled in ma_job_queue_post(). ma_job_queue_next() will return MA_NO_DATA_AVAILABLE if nothing is available. */

typedef struct
{
    ma_uint32 flags;    /* Flags passed in at initialization time. */
    ma_uint16 head;     /* The first item in the list. Required for removing from the top of the list. */
    ma_uint16 tail;     /* The last item in the list. Required for appending to the end of the list. */
    ma_semaphore sem;   /* Only used when MA_JOB_QUEUE_ASYNC is unset. */
    ma_slot_allocator allocator;
    ma_job jobs[MA_RESOURCE_MANAGER_MESSAGE_QUEUE_CAPACITY];
} ma_job_queue;

MA_API ma_result ma_job_queue_init(ma_uint32 flags, ma_job_queue* pQueue);
MA_API ma_result ma_job_queue_uninit(ma_job_queue* pQueue);
MA_API ma_result ma_job_queue_post(ma_job_queue* pQueue, const ma_job* pJob);
MA_API ma_result ma_job_queue_next(ma_job_queue* pQueue, ma_job* pJob);


typedef struct
{
    ma_uint16 code;
    ma_uint16 slot;
    union
    {
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            char* pFilePath;    /* Allocated when the message is posted, freed by the async thread after loading. */
            ma_event* pEvent;
        } loadDataBuffer;
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
        } freeDataBuffer;
        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            char* pFilePath;    /* Allocated when the message is posted, freed by the async thread after loading. */
            ma_event* pEvent;
        } loadDataStream;
        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            ma_event* pEvent;
        } freeDataStream;
        struct
        {
            ma_resource_manager_data_source* pDataSource;
            ma_event* pEvent;
        } loadDataSource;
        struct
        {
            ma_resource_manager_data_source* pDataSource;
        } freeDataSource;
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            ma_decoder* pDecoder;
            ma_event* pCompletedEvent;  /* Signalled when the data buffer has been fully decoded. */
            void* pData;
            size_t dataSizeInBytes;
            ma_uint64 decodedFrameCount;
            ma_bool32 isUnknownLength;  /* When set to true does not update the running frame count of the data buffer nor the data pointer until the last page has been decoded. */
        } decodeBufferPage;
        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            ma_uint32 pageIndex;    /* The index of the page to decode into. */
        } decodeStreamPage;
        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            ma_uint64 frameIndex;
        } seekDataStream;
    };
} ma_resource_manager_message;

MA_API ma_resource_manager_message ma_resource_manager_message_init(ma_uint16 code);


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
MA_API ma_result ma_resource_manager_message_queue_post_terminate(ma_resource_manager_message_queue* pQueue);


typedef struct
{
    const void* pData;
    ma_uint64 frameCount;           /* The total number of PCM frames making up the decoded data. */
    ma_uint64 decodedFrameCount;    /* For async decoding. Keeps track of how many frames are *currently* decoded. */
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
    ma_resource_manager_data_buffer_encoding type;
    union
    {
        ma_decoded_data decoded;
        ma_encoded_data encoded;
    };
} ma_resource_manager_memory_buffer;

struct ma_resource_manager_data_buffer
{
    ma_uint32 hashedName32; /* The hashed name. This is the key. */
    ma_uint32 refCount;
    ma_result result;       /* Result from asynchronous loading. When loading set to MA_BUSY. When fully loaded set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. */
    ma_bool32 isDataOwnedByResourceManager;
    ma_resource_manager_memory_buffer data;
    ma_resource_manager_data_buffer* pParent;
    ma_resource_manager_data_buffer* pChildLo;
    ma_resource_manager_data_buffer* pChildHi;
};

struct ma_resource_manager_data_stream
{
    ma_decoder decoder;             /* Used for filling pages with data. This is only ever accessed by the async thread. The public API should never touch this. */
    ma_bool32 isDecoderInitialized; /* Required for determining whether or not the decoder should be uninitialized in MA_MESSAGE_FREE_DATA_STREAM. */
    ma_uint32 relativeCursor;       /* The playback cursor, relative to the current page. Only ever accessed by the public API. Never accessed by the async thread. */
    ma_uint32 currentPageIndex;     /* Toggles between 0 and 1. Index 0 is the first half of pPageData. Index 1 is the second half. Only ever accessed by the public API. Never accessed by the async thread. */

    /* Written by the public API, read by the async thread. */
    ma_bool32 isLooping;            /* Whether or not the stream is looping. It's important to set the looping flag at the data stream level for smooth loop transitions. */

    /* Written by the async thread, read by the public API. */
    void* pPageData;                /* Buffer containing the decoded data of each page. Allocated once at initialization time. */
    ma_uint32 pageFrameCount[2];    /* The number of valid PCM frames in each page. Used to determine the last valid frame. */

    /* Written and read by both the public API and the async thread. */
    ma_result result;               /* Result from asynchronous loading. When loading set to MA_BUSY. When initialized set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. If an error occurs when loading, set to an error code. */
    ma_bool32 isDecoderAtEnd;       /* Whether or not the decoder has reached the end. */
    ma_bool32 isPageValid[2];       /* Booleans to indicate whether or not a page is valid. Set to false by the public API, set to true by the async thread. Set to false as the pages are consumed, true when they are filled. */
    ma_bool32 seekCounter;          /* When 0, no seeking is being performed. When > 0, a seek is being performed and reading should be delayed with MA_BUSY. */
};

struct ma_resource_manager_data_source
{
    ma_data_source_callbacks ds;
    ma_resource_manager* pResourceManager;
    ma_result result;   /* Result from asynchronous loading. When loading set to MA_BUSY. When fully loaded set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. */
    ma_uint32 flags;    /* The flags that were passed in to ma_resource_manager_data_source_init(). */
    union
    {
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            ma_uint64 cursor;                   /* Only used with data buffers (the cursor is drawn from the internal decoder for data streams). Only updated by the public API. Never written nor read from the async thread. */
            ma_bool32 seekToCursorOnNextRead;   /* On the next read we need to seek to the frame cursor. */
            ma_bool32 isLooping;
            ma_resource_manager_data_buffer_connector connectorType;
            union
            {
                ma_decoder decoder;
                ma_audio_buffer buffer;
            } connector;
        } dataBuffer;
        struct
        {
            ma_resource_manager_data_stream stream;
        } dataStream;
    };
};

typedef struct
{
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
    ma_resource_manager_data_buffer* pRootDataBuffer;   /* The root buffer in the binary tree. */
    ma_mutex dataBufferLock;                            /* For synchronizing access to the data buffer binary tree. */
    ma_thread asyncThread;                              /* Thread for running asynchronous operations. */
    ma_resource_manager_message_queue messageQueue;
    ma_default_vfs defaultVFS;                          /* Only used if a custom VFS is not specified. */
};

MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager);
MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager);

/* Data Buffers. */
MA_API ma_result ma_resource_manager_create_data_buffer(ma_resource_manager* pResourceManager, const char* pFilePath, ma_resource_manager_data_buffer_encoding type, ma_event* pEvent, ma_resource_manager_data_buffer** ppDataBuffer);
MA_API ma_result ma_resource_manager_delete_data_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_result(ma_resource_manager* pResourceManager, const ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_register_decoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate);  /* Does not copy. Increments the reference count if already exists and returns MA_SUCCESS. */
MA_API ma_result ma_resource_manager_register_encoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, size_t sizeInBytes);    /* Does not copy. Increments the reference count if already exists and returns MA_SUCCESS. */
MA_API ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName);


/* Data Streams. */
MA_API ma_result ma_resource_manager_create_data_stream(ma_resource_manager* pResourceManager, const char* pFilePath, ma_event* pEvent, ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_delete_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_result(ma_resource_manager* pResourceManager, const ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_set_looping(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_bool32 isLooping);
MA_API ma_result ma_resource_manager_data_stream_get_looping(ma_resource_manager* pResourceManager, const ma_resource_manager_data_stream* pDataStream, ma_bool32* pIsLooping);
MA_API ma_result ma_resource_manager_data_stream_read_paged_pcm_frames(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_resource_manager_data_stream_seek_to_pcm_frame(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_uint64 frameIndex);
MA_API ma_result ma_resource_manager_data_stream_map_paged_pcm_frames(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, void** ppFramesOut, ma_uint64* pFrameCount);
MA_API ma_result ma_resource_manager_data_stream_unmap_paged_pcm_frames(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_uint64 frameCount);
MA_API ma_result ma_resource_manager_data_stream_get_data_format(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_format* pFormat, ma_uint32* pChannels);

/* Data Sources. */
MA_API ma_result ma_resource_manager_data_source_init(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_uninit(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_result(ma_resource_manager* pResourceManager, const ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_set_looping(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource, ma_bool32 isLooping);
MA_API ma_result ma_resource_manager_data_source_get_looping(ma_resource_manager* pResourceManager, const ma_resource_manager_data_source* pDataSource, ma_bool32* pIsLooping);

/* Message handling. */
MA_API ma_result ma_resource_manager_handle_message(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage);
MA_API ma_result ma_resource_manager_post_message(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage);  /* Message will be copied. */
MA_API ma_result ma_resource_manager_next_message(ma_resource_manager* pResourceManager, ma_resource_manager_message* pMessage);
MA_API ma_result ma_resource_manager_peek_message(ma_resource_manager* pResourceManager, ma_resource_manager_message* pMessage);


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
    ma_sound* pFirstSoundInGroup;   /* Marked as volatile because we need to be very explicit with when we make copies of this and we can't have the compiler optimize it out. */
    ma_mixer mixer;
    ma_mutex lock;                  /* Only used by ma_engine_sound_init_*() and ma_engine_sound_uninit(). Not used in the mixing thread. */
    ma_bool32 isPlaying;            /* True by default. Sound groups can be stopped with ma_engine_sound_stop() and resumed with ma_engine_sound_start(). Also affects children. */
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

#ifndef MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS
#define MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS   1000
#endif


static ma_uint32 ma_ffs_32(ma_uint32 x)
{
    ma_uint32 i;

    /* Just a naive implementation just to get things working for now. Will optimize this later. */
    for (i = 0; i < 32; i += 1) {
        if ((x & (1 << i)) != 0) {
            return i;
        }
    }

    return i;
}


MA_API ma_result ma_slot_allocator_init(ma_slot_allocator* pAllocator)
{
    if (pAllocator == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pAllocator);

    return MA_SUCCESS;
}

MA_API ma_result ma_slot_allocator_alloc(ma_slot_allocator* pAllocator, ma_uint32* pSlot)
{
    ma_uint32 capacity;

    if (pAllocator == NULL || pSlot == NULL) {
        return MA_INVALID_ARGS;
    }

    capacity = ma_countof(pAllocator->groups) * 32;

    for (;;) {
        /* We need to acquire a suitable bitfield first. This is a bitfield that's got an available slot within it. */
        ma_uint32 iGroup;
        for (iGroup = 0; iGroup < ma_countof(pAllocator->groups); iGroup += 1) {
#if 1
            /* A CAS implementation which would rid us of the refcount requirement? */
            for (;;) {
                ma_uint32 newBitfield;
                ma_uint32 oldBitfield;
                ma_uint32 bitOffset;

                oldBitfield = pAllocator->groups[iGroup].bitfield;

                bitOffset = ma_ffs_32(~oldBitfield);
                if (bitOffset == 32) {
                    break;  /* No available bits in this bitfield. */
                }

                newBitfield = oldBitfield | (1 << bitOffset);

                if ((ma_uint32)c89atomic_compare_and_swap_32(&pAllocator->groups[iGroup].bitfield, oldBitfield, newBitfield) == oldBitfield) {
                    *pSlot = iGroup*32 + bitOffset;
                    c89atomic_fetch_add_32(&pAllocator->counter, 1);
                    return MA_SUCCESS;
                }
            }
#else
            /* A ref counted implementation which is a bit simpler to understand what's going on, but has the expense of an extra 32-bits for the group ref count. */
            ma_uint32 refcount;
            refcount = ma_atomic_increment_32(&pAllocator->groups[iGroup].refcount); /* <-- Grab a hold on the bitfield. */
            if (refcount == 1) {
                if (pAllocator->groups[iGroup].bitfield != 0xFFFFFFFF) {
                    /* We have an available bit. Now we just find the first unset bit. */
                    ma_uint32 bitOffset;

                    bitOffset = ma_ffs_32(~pAllocator->groups[iGroup].bitfield);    /* ffs = find first set. We just invert the bits to find the first unset. */
                    MA_ASSERT(bitOffset < 32);

                    pAllocator->groups[iGroup].bitfield = pAllocator->groups[iGroup].bitfield | (1 << bitOffset);

                    /* Before releasing the group we need to ensure the write operation above has completed so we'll throw a memory barrier in here for safety. */
                    ma_memory_barrier();
                    ma_atomic_increment_32(&pAllocator->counter);                   /* Incrementing the counter should happen before releasing the group's ref count to ensure we don't waste loop iterations in out-of-memory scenarios. */
                    ma_atomic_decrement_32(&pAllocator->groups[iGroup].refcount);   /* Release the hold as soon as possible to allow other things to access the bitfield. */

                    *pSlot = iGroup*32 + bitOffset;

                    return MA_SUCCESS;
                } else {
                    /* Every slot within this group has been consumed so we'll need to move on to the next one. */
                }
            } else {
                /* This group is being held by another thread for it's own allocation. Skip this group and move on to the next one. */
                MA_ASSERT(refcount > 1);
            }

            /* Getting here means we didn't find a slot in this group. We need to release the hold on this group and move to the next one. */
            ma_atomic_decrement_32(&pAllocator->groups[iGroup].refcount);
#endif
        }

        /* We weren't able to find a slot. If it's because we've reached our capacity we need to return MA_OUT_OF_MEMORY. Otherwise we need to do another iteration and try again. */
        if (pAllocator->counter < capacity) {
            ma_yield();
        } else {
            return MA_OUT_OF_MEMORY;
        }
    }

    /* Unreachable. */
}

MA_API ma_result ma_slot_allocator_alloc_16(ma_slot_allocator* pAllocator, ma_uint16* pSlot)
{
    ma_uint32 slot32;
    ma_result result = ma_slot_allocator_alloc(pAllocator, &slot32);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (slot32 > 65535) {
        return MA_OUT_OF_RANGE;
    }

    if (pSlot != NULL) {
        *pSlot = (ma_uint16)slot32;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_slot_allocator_free(ma_slot_allocator* pAllocator, ma_uint32 slot)
{
    ma_uint32 iGroup;
    ma_uint32 iBit;

    if (pAllocator == NULL) {
        return MA_INVALID_ARGS;
    }

    iGroup = slot >> 5;   /* slot / 32 */
    iBit   = slot & 31;   /* slot % 32 */

    if (iGroup >= ma_countof(pAllocator->groups)) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(iBit < 32);   /* This must be true due to the logic we used to actually calculate it. */

    while (pAllocator->counter > 0) {
#if 1
        /* CAS loop implementation. */
        ma_uint32 newBitfield;
        ma_uint32 oldBitfield;

        oldBitfield = pAllocator->groups[iGroup].bitfield;
        newBitfield = oldBitfield & ~(1 << iBit);

        if ((ma_uint32)c89atomic_compare_and_swap_32(&pAllocator->groups[iGroup].bitfield, oldBitfield, newBitfield) == oldBitfield) {
            c89atomic_fetch_sub_32(&pAllocator->counter, 1);
            return MA_SUCCESS;
        }
#else
        /* Ref counted implementation. */

        /* We need to get a hold on the group. We may need to spin for a few iterations, but this should complete in a reasonable amount of time. */
        ma_uint32 refcount = ma_atomic_increment_32(&pAllocator->groups[iGroup]);
        if (refcount == 1) {
            pAllocator->groups[iGroup].bitfield = pAllocator->groups[iGroup].bitfield & ~(1 << iBit);  /* Unset the bit. */

            /* Before releasing the group we need to ensure the write operation above has completed so we'll throw a memory barrier in here for safety. */
            ma_memory_barrier();
            c89atomic_fetch_sub_32(&pAllocator->counter, 1);                    /* Incrementing the counter should happen before releasing the group's ref count to ensure we don't waste loop iterations in out-of-memory scenarios. */
            c89atomic_fetch_sub_32(&pAllocator->groups[iGroup].refcount, 1);    /* Release the hold as soon as possible to allow other things to access the bitfield. */

            return MA_SUCCESS;
        } else {
            /* Something else is holding the group. We need to spin for a bit. */
            MA_ASSERT(refcount > 1);
        }

        /* Getting here means something is holding our lock. We need to release and spin. */
        c89atomic_fetch_sub_32(&pAllocator->groups[iGroup], 1);
        ma_yield();
#endif
    }

    /* Getting here means there are no allocations available for freeing. */
    return MA_INVALID_OPERATION;
}


MA_API ma_job ma_job_init(ma_uint16 code)
{
    ma_job job;
    
    MA_ZERO_OBJECT(&job);
    job.code = code;
    job.slot = 0xFFFF;
    job.next = 0xFFFF;

    return job;
}


/*
Lock free queue implementation based on the paper by Michael and Scott: Nonblocking Algorithms and Preemption-Safe Locking on Multiprogrammed Shared Memory Multiprocessors

TODO:
    - Figure out ABA protection.
*/
MA_API ma_result ma_job_queue_init(ma_uint32 flags, ma_job_queue* pQueue)
{
    if (pQueue == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pQueue);
    pQueue->flags = flags;

    ma_slot_allocator_init(&pQueue->allocator); /* Will not fail. */

    /* We need a semaphore if we're running in synchronous mode. */
    if ((pQueue->flags & MA_JOB_QUEUE_ASYNC) == 0) {
        ma_semaphore_init(0, &pQueue->sem);
    }

    /*
    Our queue needs to be initialized with a free standing node. This should always be slot 0. Required for the lock free algorithm. The first job in the queue is
    just a dummy item for giving us the first item in the list which is stored in the "next" member.
    */
    ma_slot_allocator_alloc_16(&pQueue->allocator, &pQueue->head);  /* Will never fail. */
    pQueue->tail = pQueue->head;

    pQueue->jobs[pQueue->head].next = 0xFFFF;

    return MA_SUCCESS;
}

MA_API ma_result ma_job_queue_uninit(ma_job_queue* pQueue)
{
    if (pQueue == NULL) {
        return MA_INVALID_ARGS;
    }

    /* All we need to do is uninitialize the semaphore. */
    if ((pQueue->flags & MA_JOB_QUEUE_ASYNC) == 0) {
        ma_semaphore_uninit(&pQueue->sem);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_job_queue_post(ma_job_queue* pQueue, const ma_job* pJob)
{
    ma_result result;
    ma_uint16 slot;
    ma_uint16 tail;
    ma_uint16 next;

    if (pQueue == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We need a new slot. */
    result = ma_slot_allocator_alloc_16(&pQueue->allocator, &slot);
    if (result != MA_SUCCESS) {
        return result;  /* Probably ran out of slots. If so, MA_OUT_OF_MEMORY will be returned. */
    }

    /* At this point we should have a slot to place the job. */
    MA_ASSERT(slot < MA_RESOURCE_MANAGER_MESSAGE_QUEUE_CAPACITY);

    /* We need to put the job into memory before we do anything. */
    pQueue->jobs[slot] = *pJob;
    pQueue->jobs[slot].slot = slot;     /* Safe cast as our maximum slot is <= 65535. */
    pQueue->jobs[slot].next = 0xFFFF;   /* Reset for safety. */

    /* The job is stored in memory so now we need to add it to our linked list. We only ever add items to the end of the list. */
    for (;;) {
        tail = pQueue->tail;
        next = pQueue->jobs[tail].next;

        if (tail == pQueue->tail) {
            if (next == 0xFFFF) {
                if (c89atomic_compare_and_swap_16(&pQueue->jobs[tail].next, next, slot) == next) {
                    break;
                }
            } else {
                c89atomic_compare_and_swap_16(&pQueue->tail, tail, next);
            }
        }
    }
    c89atomic_compare_and_swap_16(&pQueue->tail, tail, slot);


    /* Signal the semaphore as the last step if we're using synchronous mode. */
    if ((pQueue->flags & MA_JOB_QUEUE_ASYNC) == 0) {
        ma_semaphore_release(&pQueue->sem);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_job_queue_next(ma_job_queue* pQueue, ma_job* pJob)
{
    ma_uint16 head;
    ma_uint16 tail;
    ma_uint16 next;

    if (pQueue == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If we're running in synchronous mode we'll need to wait on a semaphore. */
    if ((pQueue->flags & MA_JOB_QUEUE_ASYNC) == 0) {
        ma_semaphore_wait(&pQueue->sem);
    }

    /* Now we need to remove the root item from the list. This must be done without locking. */
    for (;;) {
        head = pQueue->head;
        tail = pQueue->tail;
        next = pQueue->jobs[head].next;

        if (head == pQueue->head) {
            if (head == tail) {
                if (next == 0xFFFF) {
                    return MA_NO_DATA_AVAILABLE;
                }
                c89atomic_compare_and_swap_16(&pQueue->tail, tail, next);
            } else {
                *pJob = pQueue->jobs[next];
                if (c89atomic_compare_and_swap_16(&pQueue->head, head, next) == head) {
                    break;
                }
            }
        }
    }

    ma_slot_allocator_free(&pQueue->allocator, head);

    return MA_SUCCESS;
}

MA_API ma_result ma_job_queue_free(ma_job_queue* pQueue, ma_job* pJob)
{
    if (pQueue == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_slot_allocator_free(&pQueue->allocator, pJob->slot);
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
    return ma_hash_32(str, (int)strlen(str), MA_DEFAULT_HASH_SEED);
}



MA_API ma_resource_manager_message ma_resource_manager_message_init(ma_uint16 code)
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

    /*
    Here is where we can do some synchronized operations before inserting into the queue. This is useful for setting some state of an object
    or for cancelling an event based on the state of an object.
    */

    /* We cannot be decoding anything if the data buffer is set to any status other than MA_BUSY. */
    if (pMessage->code == MA_MESSAGE_DECODE_BUFFER_PAGE) {
        MA_ASSERT(pMessage->decodeBufferPage.pDataBuffer != NULL);

        if (pMessage->decodeBufferPage.pDataBuffer->result != MA_BUSY) {
            return MA_INVALID_OPERATION;    /* Cannot decode after the data buffer has been marked as unavailable. Abort. */
        }
    }


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

    c89atomic_exchange_32(&pQueue->putCursor, ma_rb__construct_offset(putIndex, putLoopFlag));

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

    return result;
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
    
    c89atomic_exchange_32(&pQueue->getCursor, ma_rb__construct_offset(getIndex, getLoopFlag));

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

MA_API ma_result ma_resource_manager_message_queue_post_terminate(ma_resource_manager_message_queue* pQueue)
{
    ma_resource_manager_message message = ma_resource_manager_message_init(MA_MESSAGE_TERMINATE);
    return ma_resource_manager_message_queue_post(pQueue, &message);
}



/*
Basic BST Functions
*/
static ma_result ma_resource_manager_data_buffer_search(ma_resource_manager* pResourceManager, ma_uint32 hashedName32, ma_resource_manager_data_buffer** ppDataBuffer)
{
    ma_resource_manager_data_buffer* pCurrentNode;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(ppDataBuffer     != NULL);

    pCurrentNode = pResourceManager->pRootDataBuffer;
    while (pCurrentNode != NULL) {
        if (hashedName32 == pCurrentNode->hashedName32) {
            break;  /* Found. */
        } else if (hashedName32 < pCurrentNode->hashedName32) {
            pCurrentNode = pCurrentNode->pChildLo;
        } else {
            pCurrentNode = pCurrentNode->pChildHi;
        }
    }

    *ppDataBuffer = pCurrentNode;

    if (pCurrentNode == NULL) {
        return MA_DOES_NOT_EXIST;
    } else {
        return MA_SUCCESS;
    }
}

static ma_result ma_resource_manager_data_buffer_insert_point(ma_resource_manager* pResourceManager, ma_uint32 hashedName32, ma_resource_manager_data_buffer** ppInsertPoint)
{
    ma_result result = MA_SUCCESS;
    ma_resource_manager_data_buffer* pCurrentNode;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(ppInsertPoint    != NULL);

    *ppInsertPoint = NULL;

    if (pResourceManager->pRootDataBuffer == NULL) {
        return MA_SUCCESS;  /* No items. */
    }

    /* We need to find the node that will become the parent of the new node. If a node is found that already has the same hashed name we need to return MA_ALREADY_EXISTS. */
    pCurrentNode = pResourceManager->pRootDataBuffer;
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

static ma_result ma_resource_manager_data_buffer_insert_at(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer, ma_resource_manager_data_buffer* pInsertPoint)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    /* The key must have been set before calling this function. */
    MA_ASSERT(pDataBuffer->hashedName32 != 0);

    if (pInsertPoint == NULL) {
        /* It's the first node. */
        pResourceManager->pRootDataBuffer = pDataBuffer;
    } else {
        /* It's not the first node. It needs to be inserted. */
        if (pDataBuffer->hashedName32 < pInsertPoint->hashedName32) {
            MA_ASSERT(pInsertPoint->pChildLo == NULL);
            pInsertPoint->pChildLo = pDataBuffer;
        } else {
            MA_ASSERT(pInsertPoint->pChildHi == NULL);
            pInsertPoint->pChildHi = pDataBuffer;
        }
    }

    pDataBuffer->pParent = pInsertPoint;

    return MA_SUCCESS;
}

#if 0   /* Unused for now. */
static ma_result ma_resource_manager_data_buffer_insert(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_result result;
    ma_resource_manager_data_buffer* pInsertPoint;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    result = ma_resource_manager_data_buffer_insert_point(pResourceManager, pDataBuffer->hashedName32, &pInsertPoint);
    if (result != MA_SUCCESS) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_data_buffer_insert_at(pResourceManager, pDataBuffer, pInsertPoint);
}
#endif

static MA_INLINE ma_resource_manager_data_buffer* ma_resource_manager_data_buffer_find_min(ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_resource_manager_data_buffer* pCurrentNode;

    MA_ASSERT(pDataBuffer != NULL);

    pCurrentNode = pDataBuffer;
    while (pCurrentNode->pChildLo != NULL) {
        pCurrentNode = pCurrentNode->pChildLo;
    }

    return pCurrentNode;
}

static MA_INLINE ma_resource_manager_data_buffer* ma_resource_manager_data_buffer_find_max(ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_resource_manager_data_buffer* pCurrentNode;

    MA_ASSERT(pDataBuffer != NULL);

    pCurrentNode = pDataBuffer;
    while (pCurrentNode->pChildHi != NULL) {
        pCurrentNode = pCurrentNode->pChildHi;
    }

    return pCurrentNode;
}

static MA_INLINE ma_resource_manager_data_buffer* ma_resource_manager_data_buffer_find_inorder_successor(ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pDataBuffer           != NULL);
    MA_ASSERT(pDataBuffer->pChildHi != NULL);

    return ma_resource_manager_data_buffer_find_min(pDataBuffer->pChildHi);
}

static MA_INLINE ma_resource_manager_data_buffer* ma_resource_manager_data_buffer_find_inorder_predecessor(ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pDataBuffer           != NULL);
    MA_ASSERT(pDataBuffer->pChildLo != NULL);

    return ma_resource_manager_data_buffer_find_max(pDataBuffer->pChildLo);
}

static ma_result ma_resource_manager_data_buffer_remove(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    if (pDataBuffer->pChildLo == NULL) {
        if (pDataBuffer->pChildHi == NULL) {
            /* Simple case - deleting a buffer with no children. */
            if (pDataBuffer->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataBuffer == pDataBuffer);    /* There is only a single buffer in the tree which should be equal to the root node. */
                pResourceManager->pRootDataBuffer = NULL;
            } else {
                if (pDataBuffer->pParent->pChildLo == pDataBuffer) {
                    pDataBuffer->pParent->pChildLo = NULL;
                } else {
                    pDataBuffer->pParent->pChildHi = NULL;
                }
            }
        } else {
            /* Node has one child - pChildHi != NULL. */
            pDataBuffer->pChildHi->pParent = pDataBuffer->pParent;

            if (pDataBuffer->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataBuffer == pDataBuffer);
                pResourceManager->pRootDataBuffer = pDataBuffer->pChildHi;
            } else {
                if (pDataBuffer->pParent->pChildLo == pDataBuffer) {
                    pDataBuffer->pParent->pChildLo = pDataBuffer->pChildHi;
                } else {
                    pDataBuffer->pParent->pChildHi = pDataBuffer->pChildHi;
                }
            }
        }
    } else {
        if (pDataBuffer->pChildHi == NULL) {
            /* Node has one child - pChildLo != NULL. */
            pDataBuffer->pChildLo->pParent = pDataBuffer->pParent;

            if (pDataBuffer->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataBuffer == pDataBuffer);
                pResourceManager->pRootDataBuffer = pDataBuffer->pChildLo;
            } else {
                if (pDataBuffer->pParent->pChildLo == pDataBuffer) {
                    pDataBuffer->pParent->pChildLo = pDataBuffer->pChildLo;
                } else {
                    pDataBuffer->pParent->pChildHi = pDataBuffer->pChildLo;
                }
            }
        } else {
            /* Complex case - deleting a node with two children. */
            ma_resource_manager_data_buffer* pReplacementDataBuffer;

            /* For now we are just going to use the in-order successor as the replacement, but we may want to try to keep this balanced by switching between the two. */
            pReplacementDataBuffer = ma_resource_manager_data_buffer_find_inorder_successor(pDataBuffer);
            MA_ASSERT(pReplacementDataBuffer != NULL);

            /*
            Now that we have our replacement node we can make the change. The simple way to do this would be to just exchange the values, and then remove the replacement
            node, however we track specific nodes via pointers which means we can't just swap out the values. We need to instead just change the pointers around. The
            replacement node should have at most 1 child. Therefore, we can detach it in terms of our simpler cases above. What we're essentially doing is detaching the
            replacement node and reinserting it into the same position as the deleted node.
            */
            MA_ASSERT(pReplacementDataBuffer->pParent  != NULL);  /* The replacement node should never be the root which means it should always have a parent. */
            MA_ASSERT(pReplacementDataBuffer->pChildLo == NULL);  /* Because we used in-order successor. This would be pChildHi == NULL if we used in-order predecessor. */

            if (pReplacementDataBuffer->pChildHi == NULL) {
                if (pReplacementDataBuffer->pParent->pChildLo == pReplacementDataBuffer) {
                    pReplacementDataBuffer->pParent->pChildLo = NULL;
                } else {
                    pReplacementDataBuffer->pParent->pChildHi = NULL;
                }
            } else {
                if (pReplacementDataBuffer->pParent->pChildLo == pReplacementDataBuffer) {
                    pReplacementDataBuffer->pParent->pChildLo = pReplacementDataBuffer->pChildHi;
                } else {
                    pReplacementDataBuffer->pParent->pChildHi = pReplacementDataBuffer->pChildHi;
                }
            }


            /* The replacement node has essentially been detached from the binary tree, so now we need to replace the old data buffer with it. The first thing to update is the parent */
            if (pDataBuffer->pParent != NULL) {
                if (pDataBuffer->pParent->pChildLo == pDataBuffer) {
                    pDataBuffer->pParent->pChildLo = pReplacementDataBuffer;
                } else {
                    pDataBuffer->pParent->pChildHi = pReplacementDataBuffer;
                }
            }

            /* Now need to update the replacement node's pointers. */
            pReplacementDataBuffer->pParent  = pDataBuffer->pParent;
            pReplacementDataBuffer->pChildLo = pDataBuffer->pChildLo;
            pReplacementDataBuffer->pChildHi = pDataBuffer->pChildHi;

            /* Now the children of the replacement node need to have their parent pointers updated. */
            if (pReplacementDataBuffer->pChildLo != NULL) {
                pReplacementDataBuffer->pChildLo->pParent = pReplacementDataBuffer;
            }
            if (pReplacementDataBuffer->pChildHi != NULL) {
                pReplacementDataBuffer->pChildHi->pParent = pReplacementDataBuffer;
            }

            /* Now the root node needs to be updated. */
            if (pResourceManager->pRootDataBuffer == pDataBuffer) {
                pResourceManager->pRootDataBuffer = pReplacementDataBuffer;
            }
        }
    }

    return MA_SUCCESS;
}

#if 0   /* Unused for now. */
static ma_result ma_resource_manager_data_buffer_remove_by_key(ma_resource_manager* pResourceManager, ma_uint32 hashedName32)
{
    ma_result result;
    ma_resource_manager_data_buffer* pDataBuffer;

    result = ma_resource_manager_data_buffer_search(pResourceManager, hashedName32, &pDataBuffer);
    if (result != MA_SUCCESS) {
        return result;  /* Could not find the data buffer. */
    }

    return ma_resource_manager_data_buffer_remove(pResourceManager, pDataBuffer);
}
#endif

static ma_result ma_resource_manager_data_buffer_increment_ref(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer, ma_uint32* pNewRefCount)
{
    ma_uint32 refCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    (void)pResourceManager;

    refCount = c89atomic_fetch_add_32(&pDataBuffer->refCount, 1) + 1;

    if (pNewRefCount != NULL) {
        *pNewRefCount = refCount;
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_buffer_decrement_ref(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer, ma_uint32* pNewRefCount)
{
    ma_uint32 refCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    (void)pResourceManager;

    refCount = c89atomic_fetch_sub_32(&pDataBuffer->refCount, 1) - 1;

    if (pNewRefCount != NULL) {
        *pNewRefCount = refCount;
    }

    return MA_SUCCESS;
}



static void ma_resource_manager_data_buffer_free(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    if (pDataBuffer->data.type == ma_resource_manager_data_buffer_encoding_encoded) {
        ma__free_from_callbacks((void*)pDataBuffer->data.encoded.pData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_ENCODED_BUFFER*/);
        pDataBuffer->data.encoded.pData       = NULL;
        pDataBuffer->data.encoded.sizeInBytes = 0;
    } else {
        ma__free_from_callbacks((void*)pDataBuffer->data.decoded.pData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
        pDataBuffer->data.decoded.pData       = NULL;
        pDataBuffer->data.decoded.frameCount  = 0;
    }

    /* The data buffer itself needs to be freed. */
    ma__free_from_callbacks(pDataBuffer, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
}





static ma_thread_result MA_THREADCALL ma_resource_manager_resource_thread(void* pUserData)
{
    ma_resource_manager* pResourceManager = (ma_resource_manager*)pUserData;
    MA_ASSERT(pResourceManager != NULL);

    for (;;) {
        ma_result result;
        ma_resource_manager_message message;

        result = ma_resource_manager_next_message(pResourceManager, &message);
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
        result = ma_default_vfs_init(&pResourceManager->defaultVFS, &pResourceManager->config.allocationCallbacks);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to initialize the default file system. */
        }

        pResourceManager->config.pVFS = &pResourceManager->defaultVFS;
    }

    /* Data buffer lock. */
    result = ma_mutex_init(&pResourceManager->dataBufferLock);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* We need a message queue. */
    result = ma_resource_manager_message_queue_init(pResourceManager, &pResourceManager->messageQueue);
    if (result != MA_SUCCESS) {
        ma_mutex_uninit(&pResourceManager->dataBufferLock);
        return result;
    }


    /* Create the resource thread last to ensure the new thread has access to valid data. */
    result = ma_thread_create(&pResourceManager->asyncThread, ma_thread_priority_normal, 0, ma_resource_manager_resource_thread, pResourceManager);
    if (result != MA_SUCCESS) {
        ma_mutex_uninit(&pResourceManager->dataBufferLock);
        ma_resource_manager_message_queue_uninit(&pResourceManager->messageQueue);
        return result;
    }

    return MA_SUCCESS;
}


static void ma_resource_manager_delete_all_data_buffers(ma_resource_manager* pResourceManager)
{
    MA_ASSERT(pResourceManager);

    /* If everything was done properly, there shouldn't be any active data buffers. */
    while (pResourceManager->pRootDataBuffer != NULL) {
        ma_resource_manager_data_buffer* pDataBuffer = pResourceManager->pRootDataBuffer;
        ma_resource_manager_data_buffer_remove(pResourceManager, pDataBuffer);

        /* The data buffer has been removed from the BST, so now we need to free it's data. */
        ma_resource_manager_data_buffer_free(pResourceManager, pDataBuffer);
    }
}

MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager)
{
    if (pResourceManager == NULL) {
        return;
    }

    /* The async threads need to be killed first. To do this we need to post a termination message to the message queue and then wait for the thread. */
    ma_resource_manager_message_queue_post_terminate(&pResourceManager->messageQueue);
    ma_thread_wait(&pResourceManager->asyncThread);

    /* At this point the thread should have returned and no other thread should be accessing our data. We can now delete all data buffers. */
    ma_resource_manager_delete_all_data_buffers(pResourceManager);

    /* The message queue is no longer needed. */
    ma_resource_manager_message_queue_uninit(&pResourceManager->messageQueue);

    /* We're no longer doing anything with data buffers so the lock can now be uninitialized. */
    ma_mutex_uninit(&pResourceManager->dataBufferLock);
}


static ma_result ma_resource_manager_create_data_buffer_nolock(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 hashedName32, ma_resource_manager_data_buffer_encoding type, ma_resource_manager_memory_buffer* pExistingData, ma_event* pEvent, ma_resource_manager_data_buffer** ppDataBuffer)
{
    ma_result result;
    ma_resource_manager_data_buffer* pDataBuffer;
    ma_resource_manager_data_buffer* pInsertPoint;
    char* pFilePathCopy;    /* Allocated here, freed in the resource thread. */

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pFilePath        != NULL);
    MA_ASSERT(ppDataBuffer     != NULL);

    /*
    The first thing to do is find the insertion point. If it's already loaded it means we can just increment the reference counter and signal the event. Otherwise we
    need to do a full load.
    */
    result = ma_resource_manager_data_buffer_insert_point(pResourceManager, hashedName32, &pInsertPoint);
    if (result == MA_ALREADY_EXISTS) {
        /* Fast path. The data buffer already exists. We just need to increment the reference counter and signal the event, if any. */
        pDataBuffer = pInsertPoint;

        result = ma_resource_manager_data_buffer_increment_ref(pResourceManager, pDataBuffer, NULL);
        if (result != MA_SUCCESS) {
            return result;  /* Should never happen. Failed to increment the reference count. */
        }

        if (pEvent != NULL) {
            ma_event_signal(pEvent);
        }
    } else {
        /* Slow path. The data for this buffer has not yet been initialized. The first thing to do is allocate the new data buffer and insert it into the BST. */
        pDataBuffer = ma__malloc_from_callbacks(sizeof(*pDataBuffer), &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
        if (pDataBuffer == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        MA_ZERO_OBJECT(pDataBuffer);
        pDataBuffer->hashedName32 = hashedName32;
        pDataBuffer->refCount     = 1;        /* Always set to 1 by default (this is our first reference). */
        pDataBuffer->data.type    = type;
        pDataBuffer->result       = MA_BUSY;  /* I think it's good practice to set the status to MA_BUSY by default. */

        result = ma_resource_manager_data_buffer_insert_at(pResourceManager, pDataBuffer, pInsertPoint);
        if (result != MA_SUCCESS) {
            return result;  /* Should never happen. Failed to insert the data buffer into the BST. */
        }

        /*
        The new data buffer has been inserted into the BST, so now we need to fire an event to get everything loaded. If the data is owned by the caller (not
        owned by the resource manager) we don't need to load anything which means we're done.
        */
        if (pExistingData != NULL) {
            /* We don't need to do anything if the data is owned by the application except set the necessary data pointers. */
            MA_ASSERT(type == pExistingData->type);

            pDataBuffer->isDataOwnedByResourceManager = MA_FALSE;
            pDataBuffer->data = *pExistingData;
            pDataBuffer->result = MA_SUCCESS;
        } else {
            /* The data needs to be loaded. We do this by posting an event to the resource thread. */
            ma_resource_manager_message message;

            pDataBuffer->isDataOwnedByResourceManager = MA_TRUE;
            pDataBuffer->result = MA_BUSY;

            /* We need a copy of the file path. We should probably make this more efficient, but for now we'll do a transient memory allocation. */
            pFilePathCopy = ma_copy_string(pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
            if (pFilePathCopy == NULL) {
                if (pEvent != NULL) {
                    ma_event_signal(pEvent);
                }

                ma_resource_manager_data_buffer_remove(pResourceManager, pDataBuffer);
                ma__free_from_callbacks(pDataBuffer, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
                return MA_OUT_OF_MEMORY;
            }

            /* We now have everything we need to post the message to the resource thread. This is the last thing we need to do from here. The rest will be done by the resource thread. */
            message = ma_resource_manager_message_init(MA_MESSAGE_LOAD_DATA_BUFFER);
            message.loadDataBuffer.pDataBuffer = pDataBuffer;
            message.loadDataBuffer.pFilePath   = pFilePathCopy;
            message.loadDataBuffer.pEvent      = pEvent;
            result = ma_resource_manager_post_message(pResourceManager, &message);
            if (result != MA_SUCCESS) {
                if (pEvent != NULL) {
                    ma_event_signal(pEvent);
                }

                ma_resource_manager_data_buffer_remove(pResourceManager, pDataBuffer);
                ma__free_from_callbacks(pDataBuffer,   &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
                ma__free_from_callbacks(pFilePathCopy, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
                return result;
            }
        }
    }

    MA_ASSERT(pDataBuffer != NULL);

    if (ppDataBuffer != NULL) {
        *ppDataBuffer = pDataBuffer;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_create_data_buffer(ma_resource_manager* pResourceManager, const char* pFilePath, ma_resource_manager_data_buffer_encoding type, ma_event* pEvent, ma_resource_manager_data_buffer** ppDataBuffer)
{
    ma_result result;
    ma_uint32 hashedName32;

    if (ppDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    *ppDataBuffer = NULL;

    if (pResourceManager == NULL || pFilePath == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Do as much set up before entering into the critical section to reduce our lock time as much as possible. */
    hashedName32 = ma_hash_string_32(pFilePath);

    /* At this point we can now enter the critical section. */
    ma_mutex_lock(&pResourceManager->dataBufferLock);
    {
        result = ma_resource_manager_create_data_buffer_nolock(pResourceManager, pFilePath, hashedName32, type, NULL, pEvent, ppDataBuffer);
    }
    ma_mutex_unlock(&pResourceManager->dataBufferLock);

    return result;
}

static ma_result ma_resource_manager_delete_data_buffer_nolock(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_uint32 result;
    ma_uint32 refCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    result = ma_resource_manager_data_buffer_decrement_ref(pResourceManager, pDataBuffer, &refCount);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* If the reference count has hit zero it means we need to delete the data buffer and it's backing data (so long as it's owned by the resource manager). */
    if (refCount == 0) {
        result = ma_resource_manager_data_buffer_remove(pResourceManager, pDataBuffer);
        if (result != MA_SUCCESS) {
            return result;  /* An error occurred when trying to remove the data buffer. This should never happen. */
        }

        /*
        The data buffer has been removed from the BST so now we need to delete the underyling data. This needs to be done in a separate thread. We don't
        want to delete anything if the data is owned by the application. Also, just to be safe, we set the result to MA_UNAVAILABLE.
        */
        c89atomic_exchange_32(&pDataBuffer->result, MA_UNAVAILABLE);

        /* Don't delete any underlying data if it's not owned by the resource manager. */
        if (pDataBuffer->isDataOwnedByResourceManager) {
            ma_resource_manager_message message = ma_resource_manager_message_init(MA_MESSAGE_FREE_DATA_BUFFER);
            message.freeDataBuffer.pDataBuffer = pDataBuffer;

            result = ma_resource_manager_post_message(pResourceManager, &message);
            if (result != MA_SUCCESS) {
                return result;  /* Failed to post the message for some reason. Probably due to too many messages in the queue. */
            }
        }
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_delete_data_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_result result;

    if (pResourceManager == NULL || pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_mutex_lock(&pResourceManager->dataBufferLock);
    {
        result = ma_resource_manager_delete_data_buffer_nolock(pResourceManager, pDataBuffer);
    }
    ma_mutex_unlock(&pResourceManager->dataBufferLock);

    return result;
}

MA_API ma_result ma_resource_manager_data_buffer_result(ma_resource_manager* pResourceManager, const ma_resource_manager_data_buffer* pDataBuffer)
{
    if (pResourceManager == NULL || pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    return pDataBuffer->result;
}


static ma_result ma_resource_manager_register_data(ma_resource_manager* pResourceManager, const char* pName, ma_resource_manager_data_buffer_encoding type, ma_resource_manager_memory_buffer* pExistingData, ma_event* pEvent, ma_resource_manager_data_buffer** ppDataBuffer)
{
    ma_result result = MA_SUCCESS;
    ma_uint32 hashedName32;

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    hashedName32 = ma_hash_string_32(pName);

    ma_mutex_lock(&pResourceManager->dataBufferLock);
    {
        result = ma_resource_manager_create_data_buffer_nolock(pResourceManager, pName, hashedName32, type, pExistingData, pEvent, ppDataBuffer);
    }
    ma_mutex_lock(&pResourceManager->dataBufferLock);
    return result;
}

MA_API ma_result ma_resource_manager_register_decoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_resource_manager_memory_buffer data;
    data.type               = ma_resource_manager_data_buffer_encoding_decoded;
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
    data.type                = ma_resource_manager_data_buffer_encoding_encoded;
    data.encoded.pData       = pData;
    data.encoded.sizeInBytes = sizeInBytes;

    return ma_resource_manager_register_data(pResourceManager, pName, data.type, &data, NULL, NULL);
}

MA_API ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName)
{
    ma_result result;
    ma_resource_manager_data_buffer* pDataBuffer;

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    /*
    It's assumed that the data specified by pName was registered with a prior call to ma_resource_manager_register_encoded/decoded_data(). To unregister it, all
    we need to do is delete the data buffer by it's name.
    */
    ma_mutex_lock(&pResourceManager->dataBufferLock);
    {
        result = ma_resource_manager_data_buffer_search(pResourceManager, ma_hash_string_32(pName), &pDataBuffer);
    }
    ma_mutex_unlock(&pResourceManager->dataBufferLock);

    if (result != MA_SUCCESS) {
        return result;  /* Could not find the data buffer. */
    }

    return ma_resource_manager_delete_data_buffer(pResourceManager, pDataBuffer);
}



MA_API ma_result ma_resource_manager_create_data_stream(ma_resource_manager* pResourceManager, const char* pFilePath, ma_event* pEvent, ma_resource_manager_data_stream* pDataStream)
{
    ma_result result;
    char* pFilePathCopy;
    ma_resource_manager_message message;

    if (pDataStream == NULL) {
        if (pEvent != NULL) {
            ma_event_signal(pEvent);
        }

        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataStream);
    pDataStream->result = MA_BUSY;

    if (pResourceManager == NULL || pFilePath == NULL) {
        if (pEvent != NULL) {
            ma_event_signal(pEvent);
        }

        return MA_INVALID_ARGS;
    }

    /* We want all access to the VFS and the internal decoder to happen on the async thread just to keep things easier to manage for the VFS.  */

    /* We need a copy of the file path. We should probably make this more efficient, but for now we'll do a transient memory allocation. */
    pFilePathCopy = ma_copy_string(pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
    if (pFilePathCopy == NULL) {
        if (pEvent != NULL) {
            ma_event_signal(pEvent);
        }

        return MA_OUT_OF_MEMORY;
    }

    /* We now have everything we need to post the message to the resource thread. This is the last thing we need to do from here. The rest will be done by the resource thread. */
    message = ma_resource_manager_message_init(MA_MESSAGE_LOAD_DATA_STREAM);
    message.loadDataStream.pDataStream = pDataStream;
    message.loadDataStream.pFilePath   = pFilePathCopy;
    message.loadDataStream.pEvent      = pEvent;
    result = ma_resource_manager_post_message(pResourceManager, &message);
    if (result != MA_SUCCESS) {
        if (pEvent != NULL) {
            ma_event_signal(pEvent);
        }

        ma__free_from_callbacks(pFilePathCopy, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
        return result;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_delete_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream)
{
    ma_event freeEvent;
    ma_resource_manager_message message;

    if (pResourceManager == NULL || pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The first thing to do is set the result to unavailable. This will prevent future page decoding. */
    c89atomic_exchange_32(&pDataStream->result, MA_UNAVAILABLE);

    /*
    We need to post a message to ensure we're not in the middle or decoding or anything. Because the object is owned by the caller, we'll need
    to wait for it to complete before returning which means we need an event.
    */
    ma_event_init(&freeEvent);

    message = ma_resource_manager_message_init(MA_MESSAGE_FREE_DATA_STREAM);
    message.freeDataStream.pDataStream = pDataStream;
    message.freeDataStream.pEvent      = &freeEvent;
    ma_resource_manager_post_message(pResourceManager, &message);

    /* We need to wait for the message before we return. */
    ma_event_wait(&freeEvent);
    ma_event_uninit(&freeEvent);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_result(ma_resource_manager* pResourceManager, const ma_resource_manager_data_stream* pDataStream)
{
    if (pResourceManager == NULL || pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    return pDataStream->result;
}

MA_API ma_result ma_resource_manager_data_stream_set_looping(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_bool32 isLooping)
{
    if (pResourceManager == NULL || pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_32(&pDataStream->isLooping, isLooping);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_get_looping(ma_resource_manager* pResourceManager, const ma_resource_manager_data_stream* pDataStream, ma_bool32* pIsLooping)
{
    if (pResourceManager == NULL || pDataStream == NULL || pIsLooping == NULL) {
        return MA_INVALID_ARGS;
    }

    *pIsLooping = pDataStream->isLooping;

    return MA_SUCCESS;
}


static ma_uint32 ma_resource_manager_data_stream_get_page_size_in_frames(ma_resource_manager_data_stream* pDataStream)
{
    MA_ASSERT(pDataStream != NULL);
    MA_ASSERT(pDataStream->isDecoderInitialized == MA_TRUE);

    return MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS * (pDataStream->decoder.outputSampleRate/1000); 
}

static void* ma_resource_manager_data_stream_get_page_data_pointer(ma_resource_manager_data_stream* pDataStream, ma_uint32 pageIndex, ma_uint32 relativeCursor)
{
    MA_ASSERT(pDataStream != NULL);
    MA_ASSERT(pDataStream->isDecoderInitialized == MA_TRUE);
    MA_ASSERT(pageIndex == 0 || pageIndex == 1);

    return ma_offset_ptr(pDataStream->pPageData, ((ma_resource_manager_data_stream_get_page_size_in_frames(pDataStream) * pageIndex) + relativeCursor) * ma_get_bytes_per_frame(pDataStream->decoder.outputFormat, pDataStream->decoder.outputChannels));
}



MA_API ma_result ma_resource_manager_data_stream_read_paged_pcm_frames(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 totalFramesProcessed;
    ma_format format;
    ma_uint32 channels;

    if (pResourceManager == NULL || pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataStream->result != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    /* Don't attempt to read while we're in the middle of seeking. Tell the caller that we're busy. */
    if (pDataStream->seekCounter > 0) {
        return MA_BUSY;
    }

    ma_resource_manager_data_stream_get_data_format(pResourceManager, pDataStream, &format, &channels);

    /* Reading is implemented in terms of map/unmap. We need to run this in a loop because mapping is clamped against page boundaries. */
    totalFramesProcessed = 0;
    while (totalFramesProcessed < frameCount) {
        void* pMappedFrames;
        ma_uint64 mappedFrameCount;

        mappedFrameCount = frameCount - totalFramesProcessed;
        result = ma_resource_manager_data_stream_map_paged_pcm_frames(pResourceManager, pDataStream, &pMappedFrames, &mappedFrameCount);
        if (result != MA_SUCCESS) {
            break;
        }

        /* Copy the mapped data to the output buffer if we have one. It's allowed for pFramesOut to be NULL in which case a relative forward seek is performed. */
        if (pFramesOut != NULL) {
            ma_copy_pcm_frames(ma_offset_pcm_frames_ptr(pFramesOut, totalFramesProcessed, format, channels), pMappedFrames, mappedFrameCount, format, channels);
        }

        totalFramesProcessed += mappedFrameCount;

        result = ma_resource_manager_data_stream_unmap_paged_pcm_frames(pResourceManager, pDataStream, mappedFrameCount);
        if (result != MA_SUCCESS) {
            break;  /* This is really bad - will only get an error here if we failed to post a message to the queue for loading the next page. */
        }
    }

    if (pFramesRead != NULL) {
        *pFramesRead = totalFramesProcessed;
    }

    return result;
}

MA_API ma_result ma_resource_manager_data_stream_seek_to_pcm_frame(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_uint64 frameIndex)
{
    ma_resource_manager_message message;

    if (pResourceManager == NULL || pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataStream->result != MA_SUCCESS && pDataStream->result != MA_BUSY) {
        return MA_INVALID_OPERATION;
    }

    /* Increment the seek counter first to indicate to read_paged_pcm_frames() and map_paged_pcm_frames() that we are in the middle of a seek and MA_BUSY should be returned. */
    c89atomic_fetch_add_32(&pDataStream->seekCounter, 1);

    /*
    We need to clear our currently loaded pages so that the stream starts playback from the new seek point as soon as possible. These are for the purpose of the public
    API and will be ignored by the seek message. The seek message on the async thread will operate on the assumption that both pages have been marked as invalid and
    the cursor is at the start of the first page.
    */
    pDataStream->relativeCursor   = 0;
    pDataStream->currentPageIndex = 0;
    c89atomic_exchange_32(&pDataStream->isPageValid[0], MA_FALSE);
    c89atomic_exchange_32(&pDataStream->isPageValid[1], MA_FALSE);

    /*
    The public API is not allowed to touch the internal decoder so we need to use a message to perform the seek. When seeking, the async thread will assume both pages
    are invalid and any content contained within them will be discarded and replaced with newly decoded data.
    */
    message = ma_resource_manager_message_init(MA_MESSAGE_SEEK_DATA_STREAM);
    message.seekDataStream.pDataStream = pDataStream;
    message.seekDataStream.frameIndex  = frameIndex;
    return ma_resource_manager_post_message(pResourceManager, &message);
}

MA_API ma_result ma_resource_manager_data_stream_map_paged_pcm_frames(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, void** ppFramesOut, ma_uint64* pFrameCount)
{
    ma_uint64 framesAvailable;
    ma_uint64 frameCount = 0;

    if (pFrameCount != NULL) {
        frameCount = *pFrameCount;
        *pFrameCount = 0;
    }
    if (ppFramesOut != NULL) {
        *ppFramesOut = NULL;
    }

    if (pResourceManager == NULL || pDataStream == NULL || ppFramesOut == NULL || pFrameCount == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataStream->result != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    /* Don't attempt to read while we're in the middle of seeking. Tell the caller that we're busy. */
    if (pDataStream->seekCounter > 0) {
        return MA_BUSY;
    }

    /* If the page we're on is invalid it means we've caught up to the async thread. */
    if (pDataStream->isPageValid[pDataStream->currentPageIndex] == MA_FALSE) {
        framesAvailable = 0;
    } else {
        /*
        The page we're on is valid so we must have some frames available. We need to make sure that we don't overflow into the next page, even if it's valid. The reason is
        that the unmap process will only post an update for one page at a time, which means it'll possible miss the page update for one of the pages.
        */
        MA_ASSERT(pDataStream->pageFrameCount[pDataStream->currentPageIndex] >= pDataStream->relativeCursor);
        framesAvailable = pDataStream->pageFrameCount[pDataStream->currentPageIndex] - pDataStream->relativeCursor;
    }

    /* If there's no frames available and the result is set to MA_AT_END we need to return MA_AT_END. */
    if (framesAvailable == 0) {
        if (pDataStream->isDecoderAtEnd) {
            return MA_AT_END;
        } else {
            return MA_BUSY; /* There are no frames available, but we're not marked as EOF so we might have caught up to the async thread. Need to return MA_BUSY and wait for more data. */
        }
    }

    MA_ASSERT(framesAvailable > 0);

    if (frameCount > framesAvailable) {
        frameCount = framesAvailable;
    }

    *ppFramesOut = ma_resource_manager_data_stream_get_page_data_pointer(pDataStream, pDataStream->currentPageIndex, pDataStream->relativeCursor);
    *pFrameCount = frameCount;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_unmap_paged_pcm_frames(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_uint64 frameCount)
{
    ma_uint32 newRelativeCursor;
    ma_uint32 pageSizeInFrames;
    ma_resource_manager_message message;

    if (pResourceManager == NULL || pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataStream->result != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    /* The frame count should always fit inside a 32-bit integer. */
    if (frameCount > 0xFFFFFFFF) {
        return MA_INVALID_ARGS;
    }

    pageSizeInFrames = ma_resource_manager_data_stream_get_page_size_in_frames(pDataStream);

    /* Here is where we need to check if we need to load a new page, and if so, post a message to the async thread to load it. */
    newRelativeCursor = pDataStream->relativeCursor + (ma_uint32)frameCount;

    /* If the new cursor has flowed over to the next page we need to mark the old one as invalid and post an event for it. */
    if (newRelativeCursor >= pageSizeInFrames) {
        /*newRelativeCursor -= (pageSizeInFrames*(pDataStream->currentPageIndex+1));*/
        newRelativeCursor -= pageSizeInFrames;

        /* Here is where we post the message to the async thread to start decoding. */
        message = ma_resource_manager_message_init(MA_MESSAGE_DECODE_STREAM_PAGE);
        message.decodeStreamPage.pDataStream = pDataStream;
        message.decodeStreamPage.pageIndex   = pDataStream->currentPageIndex;

        /* The page needs to be marked as invalid so that the public API doesn't try reading from it. */
        c89atomic_exchange_32(&pDataStream->isPageValid[pDataStream->currentPageIndex], MA_FALSE);

        /* Before sending the message we need to make sure we set some state. */
        pDataStream->relativeCursor   = newRelativeCursor;
        pDataStream->currentPageIndex = (pDataStream->currentPageIndex + 1) & 0x01;
        return ma_resource_manager_post_message(pResourceManager, &message);
    } else {
        /* We haven't moved into a new page so we can just move the cursor forward. */
        pDataStream->relativeCursor = newRelativeCursor;
        return MA_SUCCESS;
    }
}

MA_API ma_result ma_resource_manager_data_stream_get_data_format(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_format* pFormat, ma_uint32* pChannels)
{
    if (pResourceManager == NULL || pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataStream->result != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    /*
    We're being a little bit naughty here and accessing the internal decoder from the public API. The output data format is constant, and we've defined this function
    such that the application is responsible for ensuring it's not called while uninitializing so it should be safe.
    */
    return ma_data_source_get_data_format(&pDataStream->decoder, pFormat, pChannels);
}


static ma_result ma_resource_manager_data_source_read__stream(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /*
    We cannot be using the data source after it's been uninitialized. If you trigger this assert it means you're trying to read from the data source after
    it's been uninitialized or is in the process of uninitializing.
    */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    return ma_resource_manager_data_stream_read_paged_pcm_frames(pRMDataSource->pResourceManager, &pRMDataSource->dataStream.stream, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_resource_manager_data_source_seek__stream(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    return ma_resource_manager_data_stream_seek_to_pcm_frame(pRMDataSource->pResourceManager, &pRMDataSource->dataStream.stream, frameIndex);
}

static ma_result ma_resource_manager_data_source_map__stream(ma_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    return ma_resource_manager_data_stream_map_paged_pcm_frames(pRMDataSource->pResourceManager, &pRMDataSource->dataStream.stream, ppFramesOut, pFrameCount);
}

static ma_result ma_resource_manager_data_source_unmap__stream(ma_data_source* pDataSource, ma_uint64 frameCount)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    return ma_resource_manager_data_stream_unmap_paged_pcm_frames(pRMDataSource->pResourceManager, &pRMDataSource->dataStream.stream, frameCount);
}

static ma_result ma_resource_manager_data_source_get_data_format__stream(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    return ma_resource_manager_data_stream_get_data_format(pRMDataSource->pResourceManager, &pRMDataSource->dataStream.stream, pFormat, pChannels);
}

static ma_result ma_resource_manager_data_source_init_stream(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_resource_manager_data_source* pDataSource)
{
    ma_result result;
    ma_resource_manager_message message;
    ma_event waitEvent;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pName            != NULL);
    MA_ASSERT(pDataSource      != NULL);


    /* The first thing we need is a data stream. */
    result = ma_resource_manager_create_data_stream(pResourceManager, pName, NULL, &pDataSource->dataStream.stream);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* We use a different set of data source callbacks for data streams than we do data buffers as they use a completely different mechanism for data delivery. */
    pDataSource->ds.onRead          = ma_resource_manager_data_source_read__stream;
    pDataSource->ds.onSeek          = ma_resource_manager_data_source_seek__stream;
    pDataSource->ds.onMap           = ma_resource_manager_data_source_map__stream;
    pDataSource->ds.onUnmap         = ma_resource_manager_data_source_unmap__stream;
    pDataSource->ds.onGetDataFormat = ma_resource_manager_data_source_get_data_format__stream;
    pDataSource->result             = MA_BUSY;

    /* We need to post a message just like we do with data buffers because the caller may be wanting to run this asynchronously. */
    message = ma_resource_manager_message_init(MA_MESSAGE_LOAD_DATA_SOURCE);
    message.loadDataSource.pDataSource = pDataSource;

    if ((flags & MA_DATA_SOURCE_FLAG_ASYNC) == 0) {
        result = ma_event_init(&waitEvent);
        if (result != MA_SUCCESS) {
            ma_resource_manager_delete_data_stream(pResourceManager, &pDataSource->dataStream.stream);
            return result;
        }

        message.loadDataSource.pEvent = &waitEvent;
    } else {
        message.loadDataSource.pEvent = NULL;
    }

    result = ma_resource_manager_post_message(pResourceManager, &message);
    if (result != MA_SUCCESS) {
        ma_resource_manager_delete_data_stream(pResourceManager, &pDataSource->dataStream.stream);
        if (message.loadDataSource.pEvent != NULL) {
            ma_event_uninit(message.loadDataSource.pEvent);
        }

        return result;
    }

    /* The message has been posted. We now need to wait for the event to get signalled if we're in synchronous mode. */
    if (message.loadDataSource.pEvent != NULL) {
        ma_result streamResult;

        ma_event_wait(message.loadDataSource.pEvent);
        ma_event_uninit(message.loadDataSource.pEvent);
        message.loadDataSource.pEvent = NULL;

        /* If the data stream or data source have errors we need to return an error. */
        streamResult = ma_resource_manager_data_stream_result(pResourceManager, &pDataSource->dataStream.stream);
        if (pDataSource->result != MA_SUCCESS || streamResult != MA_SUCCESS) {
            ma_resource_manager_delete_data_stream(pResourceManager, &pDataSource->dataStream.stream);
            if (pDataSource->result != MA_SUCCESS) {
                return pDataSource->result;
            } else {
                return streamResult;
            }
        }
    }

    return MA_SUCCESS;
}



static ma_bool32 ma_resource_manager_data_source_buffer_is_busy(ma_resource_manager_data_source* pDataSource, ma_uint64 requiredFrameCount)
{
    /*
    Here is where we determine whether or not we need to return MA_BUSY from a data source callback. If we don't have enough data loaded to output all frameCount frames we
    will abort with MA_BUSY. We could also choose to do a partial read (only reading as many frames are available), but it's just easier to abort early and I don't think it
    really makes much practical difference. This only applies to decoded buffers.
    */
    if (pDataSource->dataBuffer.pDataBuffer->data.type == ma_resource_manager_data_buffer_encoding_decoded) {
        if (pDataSource->dataBuffer.pDataBuffer->data.decoded.decodedFrameCount < pDataSource->dataBuffer.pDataBuffer->data.decoded.frameCount) {
            ma_uint64 framesAvailable;

            if (pDataSource->dataBuffer.pDataBuffer->data.decoded.decodedFrameCount < pDataSource->dataBuffer.cursor) {
                return MA_TRUE; /* No data available.*/
            }

            framesAvailable = pDataSource->dataBuffer.pDataBuffer->data.decoded.decodedFrameCount - pDataSource->dataBuffer.cursor;
            if (framesAvailable < requiredFrameCount) {
                return MA_TRUE; /* Not enough frames available to read all frameCount frames. */
            }
        }
    }

    return MA_FALSE;
}

static ma_data_source* ma_resource_manager_data_source_get_buffer_connector(ma_resource_manager_data_source* pDataSource)
{
    if (pDataSource->dataBuffer.connectorType == ma_resource_manager_data_buffer_connector_buffer) {
        return &pDataSource->dataBuffer.connector.buffer;
    } else {
        return &pDataSource->dataBuffer.connector.decoder;
    }
}

static ma_result ma_resource_manager_data_source_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_result result;
    ma_uint64 framesRead;
    ma_bool32 skipBusyCheck = MA_FALSE;

    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /*
    We cannot be using the data source after it's been uninitialized. If you trigger this assert it means you're trying to read from the data source after
    it's been uninitialized or is in the process of uninitializing.
    */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    /* We don't do anything if we're busy. Returning MA_BUSY tells the mixer not to do anything with the data. */
    if (pRMDataSource->result == MA_BUSY) {
        return MA_BUSY;
    }

    if (pRMDataSource->dataBuffer.seekToCursorOnNextRead) {
        pRMDataSource->dataBuffer.seekToCursorOnNextRead = MA_FALSE;

        result = ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_source_get_buffer_connector(pRMDataSource), pRMDataSource->dataBuffer.cursor);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    if (skipBusyCheck == MA_FALSE) {
        if (ma_resource_manager_data_source_buffer_is_busy(pRMDataSource, frameCount)) {
            return MA_BUSY;
        }
    }

    result = ma_data_source_read_pcm_frames(ma_resource_manager_data_source_get_buffer_connector(pRMDataSource), pFramesOut, frameCount, &framesRead, MA_FALSE);
    pRMDataSource->dataBuffer.cursor += framesRead;

    if (pFramesRead != NULL) {
        *pFramesRead = framesRead;
    }

    return result;
}

static ma_result ma_resource_manager_data_source_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    ma_result result;
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    /* Can't do anything if the data source is not initialized yet. */
    if (pRMDataSource->result == MA_BUSY) {
        pRMDataSource->dataBuffer.cursor = frameIndex;
        pRMDataSource->dataBuffer.seekToCursorOnNextRead = MA_TRUE;
        return pRMDataSource->result;
    }

    result = ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_source_get_buffer_connector(pRMDataSource), frameIndex);
    if (result != MA_SUCCESS) {
        return result;
    }

    pRMDataSource->dataBuffer.cursor = frameIndex;
    pRMDataSource->dataBuffer.seekToCursorOnNextRead = MA_FALSE;

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_source_map(ma_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount)
{
    ma_result result;
    ma_bool32 skipBusyCheck = MA_FALSE;

    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    /* Can't do anything if the data source is not initialized yet. */
    if (pRMDataSource->result == MA_BUSY) {
        return pRMDataSource->result;
    }

    if (pRMDataSource->dataBuffer.seekToCursorOnNextRead) {
        pRMDataSource->dataBuffer.seekToCursorOnNextRead = MA_FALSE;

        result = ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_source_get_buffer_connector(pRMDataSource), pRMDataSource->dataBuffer.cursor);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    if (skipBusyCheck == MA_FALSE) {
        if (ma_resource_manager_data_source_buffer_is_busy(pDataSource, *pFrameCount)) {
            return MA_BUSY;
        }
    }

    /* The frame cursor is increment in unmap(). */
    return ma_data_source_map(ma_resource_manager_data_source_get_buffer_connector(pRMDataSource), ppFramesOut, pFrameCount);
}

static ma_result ma_resource_manager_data_source_unmap(ma_data_source* pDataSource, ma_uint64 frameCount)
{
    ma_result result;
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    /* NOTE: Don't do the same MA_BUSY status check here. If we were able to map, we want to unmap regardless of status. */

    result = ma_data_source_unmap(ma_resource_manager_data_source_get_buffer_connector(pRMDataSource), frameCount);
    if (result == MA_SUCCESS) {
        pRMDataSource->dataBuffer.cursor += frameCount;
    }

    return result;
}

static ma_result ma_resource_manager_data_source_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels)
{
    ma_resource_manager_data_source* pRMDataSource = (ma_resource_manager_data_source*)pDataSource;
    MA_ASSERT(pRMDataSource != NULL);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(pRMDataSource->result != MA_UNAVAILABLE);

    if (pRMDataSource->result == MA_BUSY) {
        return pRMDataSource->result;
    }

    return ma_data_source_get_data_format(ma_resource_manager_data_source_get_buffer_connector(pRMDataSource), pFormat, pChannels);
}


static ma_result ma_resource_manager_data_source_set_result_and_signal(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource, ma_result result, ma_event* pEvent)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataSource      != NULL);

    /* If the data source's status is anything other than MA_BUSY it means it is being deleted or an error occurred. We don't ever want to move away from that state. */
    c89atomic_compare_and_swap_32(&pDataSource->result, MA_BUSY, result);

    /* If we have an event we want to signal it after setting the data source's status. */
    if (pEvent != NULL) {
        ma_event_signal(pEvent);
    }

    return result;
}

static ma_result ma_resource_manager_data_source_init_backend_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource)
{
    ma_result result;
    ma_resource_manager_data_buffer* pDataBuffer;

    MA_ASSERT(pResourceManager                    != NULL);
    MA_ASSERT(pDataSource                         != NULL);
    MA_ASSERT(pDataSource->dataBuffer.pDataBuffer != NULL);

    pDataBuffer = pDataSource->dataBuffer.pDataBuffer;

    /* The underlying data buffer must be initialized before we'll be able to know how to initialize the backend. */
    result = ma_resource_manager_data_buffer_result(pResourceManager, pDataBuffer);
    if (result != MA_SUCCESS && result != MA_BUSY) {
        return result;  /* The data buffer is in an erroneous state. */
    }

    /* If the data buffer is busy, but the sound source is synchronous we need to report an error - that should never happen. */
    if (result == MA_BUSY && (pDataSource->flags & MA_DATA_SOURCE_FLAG_ASYNC) == 0) {
        return MA_INVALID_OPERATION;    /* Data source is being loaded synchronously, but the data buffer hasn't been fully initialized. */
    }


    /*
    We need to initialize either a ma_decoder or an ma_audio_buffer depending on whether or not the backing data is encoded or decoded. These act as the
    "instance" to the data and are used to form the connection between underlying data buffer and the data source. If the data buffer is decoded, we can use
    an ma_audio_buffer if the data format is identical to the primary format. This enables us to use memory mapping when mixing which saves us a bit of data
    movement overhead.
    */
    if (pDataBuffer->data.type == ma_resource_manager_data_buffer_encoding_decoded) {
        if (pDataBuffer->data.decoded.format     == pResourceManager->config.decodedFormat   &&
            pDataBuffer->data.decoded.sampleRate == pResourceManager->config.decodedSampleRate) {
            pDataSource->dataBuffer.connectorType = ma_resource_manager_data_buffer_connector_buffer;
        } else {
            pDataSource->dataBuffer.connectorType = ma_resource_manager_data_buffer_connector_decoder;
        }
    } else {
        pDataSource->dataBuffer.connectorType = ma_resource_manager_data_buffer_connector_decoder;
    }

    if (pDataSource->dataBuffer.connectorType == ma_resource_manager_data_buffer_connector_buffer) {
        ma_audio_buffer_config config;
        config = ma_audio_buffer_config_init(pDataBuffer->data.decoded.format, pDataBuffer->data.decoded.channels, pDataBuffer->data.decoded.frameCount, pDataBuffer->data.encoded.pData, NULL);
        result = ma_audio_buffer_init(&config, &pDataSource->dataBuffer.connector.buffer);
    } else {
        ma_decoder_config configIn;
        ma_decoder_config configOut;

        configIn  = ma_decoder_config_init(pDataBuffer->data.decoded.format, pDataBuffer->data.decoded.channels, pDataBuffer->data.decoded.sampleRate);
        configOut = ma_decoder_config_init(pResourceManager->config.decodedFormat, pDataBuffer->data.decoded.channels, pResourceManager->config.decodedSampleRate);  /* <-- Never perform channel conversion at this level - that will be done at a higher level. */

        if (pDataBuffer->data.type == ma_resource_manager_data_buffer_encoding_decoded) {
            ma_uint64 sizeInBytes = pDataBuffer->data.decoded.frameCount * ma_get_bytes_per_frame(configIn.format, configIn.channels);
            if (sizeInBytes > MA_SIZE_MAX) {
                result = MA_TOO_BIG;
            } else {
                result = ma_decoder_init_memory_raw(pDataBuffer->data.decoded.pData, (size_t)sizeInBytes, &configIn, &configOut, &pDataSource->dataBuffer.connector.decoder);  /* Safe cast thanks to the check above. */
            }
        } else {
            configOut.allocationCallbacks = pResourceManager->config.allocationCallbacks;
            result = ma_decoder_init_memory(pDataBuffer->data.encoded.pData, pDataBuffer->data.encoded.sizeInBytes, &configOut, &pDataSource->dataBuffer.connector.decoder);
        }
    }

    /*
    We can only do mapping if the data source's backend is an audio buffer. If it's not, clear out the callbacks thereby preventing the mixer from attempting
    memory map mode, only to fail.
    */
    if (pDataSource->dataBuffer.connectorType != ma_resource_manager_data_buffer_connector_buffer) {
        pDataSource->ds.onMap   = NULL;
        pDataSource->ds.onUnmap = NULL;
    }
    
    /* At this point the backend should be initialized. We do *not* want to set pDataSource->result here - that needs to be done at a higher level to ensure it's done as the last step. */
    return result;
}

static ma_result ma_resource_manager_data_source_uninit_backend_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource)
{
    MA_ASSERT(pResourceManager                    != NULL);
    MA_ASSERT(pDataSource                         != NULL);
    MA_ASSERT(pDataSource->dataBuffer.pDataBuffer != NULL);

    if (pDataSource->dataBuffer.connectorType == ma_resource_manager_data_buffer_connector_decoder) {
        ma_decoder_uninit(&pDataSource->dataBuffer.connector.decoder);
    } else {
        ma_audio_buffer_uninit(&pDataSource->dataBuffer.connector.buffer);
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_source_init_buffer(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_resource_manager_data_source* pDataSource)
{
    ma_result result;
    ma_result dataBufferResult;
    ma_resource_manager_data_buffer* pDataBuffer;
    ma_resource_manager_data_buffer_encoding dataBufferType;
    ma_event waitEvent;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pName            != NULL);
    MA_ASSERT(pDataSource      != NULL);

    /* The first thing we need to do is acquire a data buffer. */
    if ((flags & MA_DATA_SOURCE_FLAG_DECODE) != 0) {
        dataBufferType = ma_resource_manager_data_buffer_encoding_decoded;
    } else {
        dataBufferType = ma_resource_manager_data_buffer_encoding_encoded;
    }

    result = ma_resource_manager_create_data_buffer(pResourceManager, pName, dataBufferType, NULL, &pDataBuffer);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to acquire the data buffer. */
    }

    /* At this point we have our data buffer and we can start initializing the data source. */
    pDataSource->ds.onRead                = ma_resource_manager_data_source_read;
    pDataSource->ds.onSeek                = ma_resource_manager_data_source_seek;
    pDataSource->ds.onMap                 = ma_resource_manager_data_source_map;
    pDataSource->ds.onUnmap               = ma_resource_manager_data_source_unmap;
    pDataSource->ds.onGetDataFormat       = ma_resource_manager_data_source_get_data_format;
    pDataSource->dataBuffer.pDataBuffer   = pDataBuffer;
    pDataSource->dataBuffer.connectorType = ma_resource_manager_data_buffer_connector_unknown; /* The backend type hasn't been determine yet - that happens when it's initialized properly by the resource thread. */
    pDataSource->result                   = MA_BUSY;

    /*
    If the data buffer has been fully initialized we can complete initialization of the data source now. Otherwise we need to post an event to the resource thread to complete
    initialization to ensure it's done after the data buffer.
    */
    dataBufferResult = ma_resource_manager_data_buffer_result(pResourceManager, pDataBuffer);
    if (dataBufferResult == MA_BUSY) {
        /* The data buffer is in the middle of loading. We need to post an event to the resource thread. */
        ma_resource_manager_message message;
        message = ma_resource_manager_message_init(MA_MESSAGE_LOAD_DATA_SOURCE);
        message.loadDataSource.pDataSource = pDataSource;

        if ((flags & MA_DATA_SOURCE_FLAG_ASYNC) == 0) {
            result = ma_event_init(&waitEvent);
            if (result != MA_SUCCESS) {
                ma_resource_manager_delete_data_buffer(pResourceManager, pDataBuffer);
                return result;
            }

            message.loadDataSource.pEvent = &waitEvent;
        } else {
            message.loadDataSource.pEvent = NULL;
        }
        
        result = ma_resource_manager_post_message(pResourceManager, &message);
        if (result != MA_SUCCESS) {
            if (message.loadDataSource.pEvent != NULL) {
                ma_event_uninit(message.loadDataSource.pEvent);
            }

            ma_resource_manager_delete_data_buffer(pResourceManager, pDataBuffer);
            return result;
        }

        /* The message has been posted. We now need to wait for the event to get signalled if we're in synchronous mode. */
        if (message.loadDataSource.pEvent != NULL) {
            ma_event_wait(message.loadDataSource.pEvent);
            ma_event_uninit(message.loadDataSource.pEvent);
            message.loadDataSource.pEvent = NULL;
        
            /* Check the status of the data buffer for any errors. Even in the event of an error, the data source will not be deleted. */
            if (pDataBuffer->result != MA_SUCCESS) {
                result = pDataBuffer->result;
                ma_resource_manager_delete_data_buffer(pResourceManager, pDataBuffer);
                return result;
            }
        }

        return MA_SUCCESS;
    } else if (dataBufferResult == MA_SUCCESS) {
        /* The underlying data buffer has already been initialized so we can just complete initialization of the data source right now. */
        result = ma_resource_manager_data_source_init_backend_buffer(pResourceManager, pDataSource);
        if (result != MA_SUCCESS) {
            ma_resource_manager_delete_data_buffer(pResourceManager, pDataBuffer);
            return result;
        }

        c89atomic_exchange_32(&pDataSource->result, MA_SUCCESS);
        return MA_SUCCESS;
    } else {
        /* Some other error has occurred with the data buffer. Lets abandon everything and return the data buffer's result. */
        ma_resource_manager_delete_data_buffer(pResourceManager, pDataBuffer);
        return dataBufferResult;
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

    pDataSource->pResourceManager = pResourceManager;
    pDataSource->flags = flags;

    if ((flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_source_init_stream(pResourceManager, pName, flags, pDataSource);
    } else {
        return ma_resource_manager_data_source_init_buffer(pResourceManager, pName, flags, pDataSource);
    }
}


static ma_result ma_resource_manager_data_source_uninit_stream(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataSource      != NULL);

    /* All we need to do is uninitialize the data stream and we're done. */
    return ma_resource_manager_delete_data_stream(pResourceManager, &pDataSource->dataStream.stream);
}

static ma_result ma_resource_manager_data_source_uninit_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataSource      != NULL);

    /* We should uninitialize the data source's backend before deleting the data buffer just to keep the order of operations clean. */
    ma_resource_manager_data_source_uninit_backend_buffer(pResourceManager, pDataSource);
    pDataSource->dataBuffer.connectorType = ma_resource_manager_data_buffer_connector_unknown;

    /* The data buffer needs to be deleted. */
    if (pDataSource->dataBuffer.pDataBuffer != NULL) {
        ma_resource_manager_delete_data_buffer(pResourceManager, pDataSource->dataBuffer.pDataBuffer);
        pDataSource->dataBuffer.pDataBuffer = NULL;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_source_uninit(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource)
{
    if (pResourceManager == NULL || pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /*
    We need to run this synchronously because the caller owns the data source and we can't return before it's been fully uninitialized. We do, however, need to do
    the actual uninitialization on the resource thread for order-of-operations reasons.
    */

    /* We need to wait to finish loading before we try uninitializing. */
    while (pDataSource->result == MA_BUSY) {
        ma_yield();
    }

    /* The first thing to do is to mark the data source as unavailable. This will stop other threads from acquiring a hold on the data source which is what happens in the callbacks. */
    c89atomic_exchange_32(&pDataSource->result, MA_UNAVAILABLE);

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_source_uninit_stream(pResourceManager, pDataSource);
    } else {
        return ma_resource_manager_data_source_uninit_buffer(pResourceManager, pDataSource);
    }
}

MA_API ma_result ma_resource_manager_data_source_result(ma_resource_manager* pResourceManager, const ma_resource_manager_data_source* pDataSource)
{
    if (pResourceManager == NULL || pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    return pDataSource->result;
}

MA_API ma_result ma_resource_manager_data_source_set_looping(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource, ma_bool32 isLooping)
{
    if (pResourceManager == NULL || pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /*
    The loop flag is set at the level of the underlying data source. This is require for streams in particular because page loading logic performed on the
    async thread will ensure the loop transition is smooth.
    */
    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_set_looping(pResourceManager, &pDataSource->dataStream.stream, isLooping);
    } else {
        c89atomic_exchange_32(&pDataSource->dataBuffer.isLooping, isLooping);
        return MA_SUCCESS;
    }
}

MA_API ma_result ma_resource_manager_data_source_get_looping(ma_resource_manager* pResourceManager, const ma_resource_manager_data_source* pDataSource, ma_bool32* pIsLooping)
{
    if (pResourceManager == NULL || pDataSource == NULL || pIsLooping == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_get_looping(pResourceManager, &pDataSource->dataStream.stream, pIsLooping);
    } else {
        *pIsLooping = pDataSource->dataBuffer.isLooping;
        return MA_SUCCESS;
    }
}



static ma_result ma_resource_manager_handle_message__load_data_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer, char* pFilePath, ma_event* pEvent)
{
    ma_result result = MA_SUCCESS;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);
    MA_ASSERT(pFilePath        != NULL);
    MA_ASSERT(pDataBuffer->isDataOwnedByResourceManager == MA_TRUE);  /* The data should always be owned by the resource manager. */

    if (pDataBuffer->result != MA_BUSY) {
        result = MA_INVALID_OPERATION;    /* The data buffer may be getting deleted before it's even been loaded. */
        goto done;
    }

    if (pDataBuffer->data.type == ma_resource_manager_data_buffer_encoding_encoded) {
        /* No decoding. Just store the file contents in memory. */
        void* pData;
        size_t sizeInBytes;

        result = ma_vfs_open_and_read_file_ex(pResourceManager->config.pVFS, pFilePath, &pData, &sizeInBytes, &pResourceManager->config.allocationCallbacks, MA_ALLOCATION_TYPE_ENCODED_BUFFER);
        if (result == MA_SUCCESS) {
            pDataBuffer->data.encoded.pData       = pData;
            pDataBuffer->data.encoded.sizeInBytes = sizeInBytes;
        }
    } else  {
        /* Decoding. */
        ma_decoder* pDecoder;       /* Malloc'd here, and then free'd on the last page decode. */
        ma_decoder_config config;
        ma_uint64 totalFrameCount;
        void* pData;
        ma_uint64 dataSizeInBytes;
        ma_uint64 dataSizeInFrames;
        ma_uint64 pageSizeInFrames;
        ma_uint64 framesRead;   /* <-- Keeps track of how many frames we read for the first page. */
        ma_resource_manager_message decodeBufferPageMessage;

        /*
        With the file initialized we now need to initialize the decoder. We need to pass this decoder around on the message queue so we'll need to
        allocate memory for this dynamically.
        */
        pDecoder = (ma_decoder*)ma__malloc_from_callbacks(sizeof(*pDecoder), &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);
        if (pDecoder == NULL) {
            result = MA_OUT_OF_MEMORY;
            goto done;
        }

        config = ma_decoder_config_init(pResourceManager->config.decodedFormat, 0, pResourceManager->config.decodedSampleRate); /* Need to keep the native channel count because we'll be using that for spatialization. */
        config.allocationCallbacks = pResourceManager->config.allocationCallbacks;

        result = ma_decoder_init_vfs(pResourceManager->config.pVFS, pFilePath, &config, pDecoder);

        /* Make sure we never set the result code to MA_BUSY or else we'll get everything confused. */
        if (result == MA_BUSY) {
            result = MA_ERROR;
        }

        if (result != MA_SUCCESS) {
            ma__free_from_callbacks(pDecoder, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);
            goto done;
        }

        /*
        Getting here means we have the decoder. We can now get prepared to start decoding. The first thing we need is a buffer, but to determine the
        size we need to get the length of the sound in PCM frames. If the length cannot be determined we need to mark it as such and not set the data
        pointer in the data buffer until the very end.

        If after decoding the first page we complete decoding we need to fire the event and ensure the status is set to MA_SUCCESS.
        */
        pDataBuffer->data.decoded.format     = pDecoder->outputFormat;
        pDataBuffer->data.decoded.channels   = pDecoder->outputChannels;
        pDataBuffer->data.decoded.sampleRate = pDecoder->outputSampleRate;

        pageSizeInFrames = MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS * (pDecoder->outputSampleRate/1000);

        totalFrameCount = ma_decoder_get_length_in_pcm_frames(pDecoder);
        if (totalFrameCount > 0) {
            /* It's a known length. We can allocate the buffer now. */
            dataSizeInFrames = totalFrameCount;
        } else {
            /* It's an unknown length. We need to dynamically expand the buffer as we decode. To start with we allocate space for one page. We'll then double it as we need more space. */
            dataSizeInFrames = pageSizeInFrames;
        }

        dataSizeInBytes = dataSizeInFrames * ma_get_bytes_per_frame(pDecoder->outputFormat, pDecoder->outputChannels);
        if (dataSizeInBytes > MA_SIZE_MAX) {
            ma__free_from_callbacks(pDecoder, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);
            result = MA_TOO_BIG;
            goto done;
        }

        pData = ma__malloc_from_callbacks((size_t)dataSizeInBytes, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
        if (pData == NULL) {
            ma__free_from_callbacks(pDecoder, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);
            result = MA_OUT_OF_MEMORY;
            goto done;
        }

        /* The buffer needs to be initialized to silence in case the caller reads from it which they may decide to do. */
        ma_silence_pcm_frames(pData, dataSizeInFrames, pDecoder->outputFormat, pDecoder->outputChannels);


        /* We should have enough room in our buffer for at least a whole page, or the entire file (if it's less than a page). We can now decode that first page. */
        framesRead = ma_decoder_read_pcm_frames(pDecoder, pData, pageSizeInFrames);
        if (framesRead < pageSizeInFrames) {
            /* We've read the entire sound. This is the simple case. We just need to set the result to MA_SUCCESS. */
            pDataBuffer->data.decoded.pData      = pData;
            pDataBuffer->data.decoded.frameCount = framesRead;

            /*
            decodedFrameCount is what other threads will use to determine whether or not data is available. We must ensure pData and frameCount
            is set *before* setting the number of available frames. This way, the other thread need only check if decodedFrameCount > 0, in
            which case it can assume pData and frameCount are valid.
            */
            c89atomic_thread_fence(c89atomic_memory_order_acquire);
            pDataBuffer->data.decoded.decodedFrameCount = framesRead;

            ma__free_from_callbacks(pDecoder, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);
            result = MA_SUCCESS;
            goto done;
        } else {
            /* We've still got more to decode. We need to post a message to keep decoding the rest. */
            decodeBufferPageMessage = ma_resource_manager_message_init(MA_MESSAGE_DECODE_BUFFER_PAGE);
            decodeBufferPageMessage.decodeBufferPage.pDataBuffer       = pDataBuffer;
            decodeBufferPageMessage.decodeBufferPage.pDecoder          = pDecoder;
            decodeBufferPageMessage.decodeBufferPage.pCompletedEvent   = pEvent;
            decodeBufferPageMessage.decodeBufferPage.pData             = pData;
            decodeBufferPageMessage.decodeBufferPage.dataSizeInBytes   = (size_t)dataSizeInBytes;   /* Safe cast. Was checked for > MA_SIZE_MAX earlier. */
            decodeBufferPageMessage.decodeBufferPage.decodedFrameCount = framesRead;

            if (totalFrameCount > 0) {
                decodeBufferPageMessage.decodeBufferPage.isUnknownLength = MA_FALSE;

                pDataBuffer->data.decoded.pData      = pData;
                pDataBuffer->data.decoded.frameCount = totalFrameCount;

                /*
                decodedFrameCount is what other threads will use to determine whether or not data is available. We must ensure pData and frameCount
                is set *before* setting the number of available frames. This way, the other thread need only check if decodedFrameCount > 0, in
                which case it can assume pData and frameCount are valid.
                */
                c89atomic_thread_fence(c89atomic_memory_order_acquire);
                pDataBuffer->data.decoded.decodedFrameCount = framesRead;
            } else {
                decodeBufferPageMessage.decodeBufferPage.isUnknownLength = MA_TRUE;

                /*
                These members are all set after the last page has been decoded. The reason for this is that the application should not be attempting to
                read any data until the sound is fully decoded because we're going to be dynamically expanding pData and we'll be introducing complications
                by letting the application get access to it.
                */
                pDataBuffer->data.decoded.pData             = NULL;
                pDataBuffer->data.decoded.frameCount        = 0;
                pDataBuffer->data.decoded.decodedFrameCount = 0;
            }

            /* The message has been set up so it can now be posted. */
            result = ma_resource_manager_post_message(pResourceManager, &decodeBufferPageMessage);

            /* The result needs to be set to MA_BUSY to ensure the status of the data buffer is set properly in the next section. */
            if (result == MA_SUCCESS) {
                result = MA_BUSY;
            }
            
            /* We want to make sure we don't signal the event here. It needs to be delayed until the last page. */
            pEvent = NULL;
        }
    }

done:
    ma__free_from_callbacks(pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);

    /*
    We need to set the result to at the very end to ensure no other threads try reading the data before we've fully initialized the object. Other threads
    are going to be inspecting this variable to determine whether or not they're ready to read data. We can only change the result if it's set to MA_BUSY
    because otherwise we may be changing away from an error code which would be bad. An example is if the application creates a data buffer, but then
    immediately deletes it before we've got to this point. In this case, pDataBuffer->result will be MA_UNAVAILABLE, and setting it to MA_SUCCESS or any
    other error code would cause the buffer to look like it's in a state that it's not.
    */
    c89atomic_compare_and_swap_32(&pDataBuffer->result, MA_BUSY, result);

    /* Only signal the other threads after the result has been set just for cleanliness sake. */
    if (pEvent != NULL) {
        ma_event_signal(pEvent);
    }

    return result;
}

static ma_result ma_resource_manager_handle_message__free_data_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer)
{
    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pDataBuffer->result == MA_UNAVAILABLE);

    ma_resource_manager_data_buffer_free(pResourceManager, pDataBuffer);

    return MA_SUCCESS;
}


static void ma_resource_manager_data_stream_fill_page(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_uint32 pageIndex)
{
    (void)pResourceManager;

    ma_uint64 pageSizeInFrames;
    ma_uint64 totalFramesReadForThisPage = 0;
    void* pPageData = ma_resource_manager_data_stream_get_page_data_pointer(pDataStream, pageIndex, 0);

    pageSizeInFrames = ma_resource_manager_data_stream_get_page_size_in_frames(pDataStream);

    if (pDataStream->isLooping) {
        while (totalFramesReadForThisPage < pageSizeInFrames) {
            ma_uint64 framesRemaining;
            ma_uint64 framesRead;

            framesRemaining = pageSizeInFrames - totalFramesReadForThisPage;
            framesRead = ma_decoder_read_pcm_frames(&pDataStream->decoder, ma_offset_pcm_frames_ptr(pPageData, totalFramesReadForThisPage, pDataStream->decoder.outputFormat, pDataStream->decoder.outputChannels), framesRemaining);
            totalFramesReadForThisPage += framesRead;

            /* Loop back to the start if we reached the end. */
            if (framesRead < framesRemaining) {
                ma_decoder_seek_to_pcm_frame(&pDataStream->decoder, 0);
            }
        }
    } else {
        totalFramesReadForThisPage = ma_decoder_read_pcm_frames(&pDataStream->decoder, pPageData, pageSizeInFrames);
    }

    if (totalFramesReadForThisPage < pageSizeInFrames) {
        c89atomic_exchange_32(&pDataStream->isDecoderAtEnd, MA_TRUE);
    }

    c89atomic_exchange_32(&pDataStream->pageFrameCount[pageIndex], (ma_uint32)totalFramesReadForThisPage);
    c89atomic_exchange_32(&pDataStream->isPageValid[pageIndex], MA_TRUE);
}

static void ma_resource_manager_data_stream_fill_pages(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream)
{
    ma_uint32 iPage;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataStream      != NULL);

    /* For each page... */
    for (iPage = 0; iPage < 2; iPage += 1) {
        ma_resource_manager_data_stream_fill_page(pResourceManager, pDataStream, iPage);

        /* If we reached the end make sure we get out of the loop to prevent us from trying to load the second page. */
        if (pDataStream->isDecoderAtEnd) {
            break;
        }
    }
}

static ma_result ma_resource_manager_handle_message__load_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, char* pFilePath, ma_event* pEvent)
{
    ma_result result = MA_SUCCESS;
    ma_decoder_config decoderConfig;
    ma_uint32 pageBufferSizeInBytes;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataStream      != NULL);
    MA_ASSERT(pFilePath        != NULL);

    if (pDataStream->result != MA_BUSY) {
        result = MA_INVALID_OPERATION;  /* Most likely the data stream is being uninitialized. */
        goto done;
    }

    /* We need to initialize the decoder first so we can determine the size of the pages. */
    decoderConfig = ma_decoder_config_init(pResourceManager->config.decodedFormat, 0, pResourceManager->config.decodedSampleRate); /* Need to keep the native channel count because we'll be using that for spatialization. */
    decoderConfig.allocationCallbacks = pResourceManager->config.allocationCallbacks;

    result = ma_decoder_init_vfs(pResourceManager->config.pVFS, pFilePath, &decoderConfig, &pDataStream->decoder);
    if (result != MA_SUCCESS) {
        goto done;
    }

    pDataStream->isDecoderInitialized = MA_TRUE;

    /* We have the decoder so we can now initialize our page buffer. */
    pageBufferSizeInBytes = ma_resource_manager_data_stream_get_page_size_in_frames(pDataStream) * 2 * ma_get_bytes_per_frame(pDataStream->decoder.outputFormat, pDataStream->decoder.outputChannels);

    pDataStream->pPageData = ma__malloc_from_callbacks(pageBufferSizeInBytes, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
    if (pDataStream->pPageData == NULL) {
        ma_decoder_uninit(&pDataStream->decoder);
        result = MA_OUT_OF_MEMORY;
        goto done;
    }

    /* We have our decoder and our page buffer, so now we need to fill our pages. */
    ma_resource_manager_data_stream_fill_pages(pResourceManager, pDataStream);

    /* And now we're done. We want to make sure the result is MA_SUCCESS. */
    result = MA_SUCCESS;

done:
    ma__free_from_callbacks(pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);

    /* We can only change the status away from MA_BUSY. If it's set to anything else it means an error has occurred somewhere or the uninitialization process has started (most likely). */
    c89atomic_compare_and_swap_32(&pDataStream->result, MA_BUSY, result);

    /* Only signal the other threads after the result has been set just for cleanliness sake. */
    if (pEvent != NULL) {
        ma_event_signal(pEvent);
    }

    return result;
}

static ma_result ma_resource_manager_handle_message__free_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_event* pEvent)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataStream      != NULL);

    /* If our status is not MA_UNAVAILABLE we have a bug somewhere. */
    MA_ASSERT(pDataStream->result == MA_UNAVAILABLE);

    if (pDataStream->isDecoderInitialized) {
        ma_decoder_uninit(&pDataStream->decoder);
    }

    if (pDataStream->pPageData != NULL) {
        ma__free_from_callbacks(pDataStream->pPageData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
        pDataStream->pPageData = NULL;  /* Just in case... */
    }

    /* The event needs to be signalled last. */
    if (pEvent != NULL) {
        ma_event_signal(pEvent);
    }

    return MA_SUCCESS;
}


static ma_result ma_resource_manager_handle_message__load_data_source__buffer(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource, ma_event* pEvent)
{
    ma_result dataBufferResult;

    /* We shouldn't attempt to load anything if the data buffer is in an erroneous state. */
    dataBufferResult = ma_resource_manager_data_buffer_result(pResourceManager, pDataSource->dataBuffer.pDataBuffer);
    if (dataBufferResult != MA_SUCCESS && dataBufferResult != MA_BUSY) {
        return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, dataBufferResult, pEvent);
    }

    if (pDataSource->dataBuffer.pDataBuffer->data.type == ma_resource_manager_data_buffer_encoding_encoded) {
        if (pDataSource->dataBuffer.pDataBuffer->data.encoded.pData == NULL) {
            /* Something has gone badly wrong - no data is available from the data buffer, but it's not in an erroneous state (checked above). */
            MA_ASSERT(MA_FALSE);
            return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, MA_NO_DATA_AVAILABLE, pEvent);
        }

        return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, ma_resource_manager_data_source_init_backend_buffer(pResourceManager, pDataSource), pEvent);
    } else {
        /*
        We can initialize the data source if there is a non-zero decoded frame count. If the sound is being loaded synchronously or there are no frames available we need to re-insert
        the event and wait until the sound is fully loaded.
        */
        ma_bool32 canInitialize = MA_FALSE;

        MA_ASSERT(pDataSource->dataBuffer.pDataBuffer->data.decoded.decodedFrameCount <= pDataSource->dataBuffer.pDataBuffer->data.decoded.frameCount);

        if (pDataSource->dataBuffer.pDataBuffer->data.decoded.decodedFrameCount > 0) {
            /* We can maybe initialize. */
            if (pDataSource->dataBuffer.pDataBuffer->data.decoded.decodedFrameCount == pDataSource->dataBuffer.pDataBuffer->data.decoded.frameCount) {
                /* We can definitely initialize. */
                canInitialize = MA_TRUE;
            } else {
                /*
                We have some data available so we can initialize if we're supporting asynchronous loading of the data source. If the data source is being loaded
                synchronously we need to initialize later.
                */
                if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_ASYNC) != 0) {
                    canInitialize = MA_TRUE;    /* It's asynchronous so we can initialize now. */
                } else {
                    canInitialize = MA_FALSE;   /* It's synchronous so we need to initialize later. */
                }
            }
        } else {
            canInitialize = MA_FALSE;
        }

        if (canInitialize) {
            return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, ma_resource_manager_data_source_init_backend_buffer(pResourceManager, pDataSource), pEvent);
        } else {
            /* We can't initialize just yet so we need to just post the message again. */
            ma_resource_manager_message message = ma_resource_manager_message_init(MA_MESSAGE_LOAD_DATA_SOURCE);
            message.loadDataSource.pDataSource = pDataSource;
            message.loadDataSource.pEvent      = pEvent;
            return ma_resource_manager_post_message(pResourceManager, &message);
        }
    }
}

static ma_result ma_resource_manager_handle_message__load_data_source__stream(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource, ma_event* pEvent)
{
    ma_result dataStreamResult;

    /* For data sources backed by a data stream, the stream should never be in a busy state by this point. */
    dataStreamResult = ma_resource_manager_data_stream_result(pResourceManager, &pDataSource->dataStream.stream);
    if (dataStreamResult != MA_SUCCESS) {
        return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, dataStreamResult, pEvent);
    }

    /* We don't need to do anything other than set the result. The decoder will have been initialized from the MA_MESSAGE_LOAD_DATA_STREAM message. */
    return ma_resource_manager_data_source_set_result_and_signal(pResourceManager, pDataSource, MA_SUCCESS, pEvent);
}

static ma_result ma_resource_manager_handle_message__load_data_source(ma_resource_manager* pResourceManager, ma_resource_manager_data_source* pDataSource, ma_event* pEvent)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataSource      != NULL);
    MA_ASSERT(pDataSource->result == MA_BUSY || pDataSource->result == MA_UNAVAILABLE);

    if (pDataSource->result == MA_UNAVAILABLE) {
        /*
        The data source is getting deleted before it's even been loaded. We want to continue loading in this case because in the queue we'll have a
        corresponding MA_MESSAGE_FREE_DATA_SOURCE which will be doing the opposite. By letting it continue we can simplify the implementation because
        otherwise we'd need to keep track of a separate bit of state to track whether or not the backend has been initialized or not.
        */
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_handle_message__load_data_source__stream(pResourceManager, pDataSource, pEvent);
    } else {
        return ma_resource_manager_handle_message__load_data_source__buffer(pResourceManager, pDataSource, pEvent);
    }
}

static ma_result ma_resource_manager_handle_message__decode_buffer_page(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 pageSizeInFrames;
    ma_uint64 framesRead;
    void* pRunningData;
    ma_resource_manager_message messageCopy;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pMessage         != NULL);

    /* Don't do any more decoding if the data buffer has started the uninitialization process. */
    if (pMessage->decodeBufferPage.pDataBuffer->result != MA_BUSY) {
        return MA_INVALID_OPERATION;
    }

    /* We're going to base everything off the original message. */
    messageCopy = *pMessage;

    /* We need to know the size of a page in frames to know how many frames to decode. */
    pageSizeInFrames = MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS * (messageCopy.decodeBufferPage.pDecoder->outputSampleRate/1000);

    /* If the total length is unknown we may need to expand the size of the buffer. */
    if (messageCopy.decodeBufferPage.isUnknownLength == MA_TRUE) {
        ma_uint64 requiredSize = (messageCopy.decodeBufferPage.decodedFrameCount + pageSizeInFrames) * ma_get_bytes_per_frame(messageCopy.decodeBufferPage.pDecoder->outputFormat, messageCopy.decodeBufferPage.pDecoder->outputChannels);
        if (requiredSize <= MA_SIZE_MAX) {
            if (requiredSize > messageCopy.decodeBufferPage.dataSizeInBytes) {
                size_t newSize = (size_t)ma_max(requiredSize, messageCopy.decodeBufferPage.dataSizeInBytes * 2);
                void *pNewData = ma__realloc_from_callbacks(messageCopy.decodeBufferPage.pData, newSize, messageCopy.decodeBufferPage.dataSizeInBytes, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
                if (pNewData != NULL) {
                    messageCopy.decodeBufferPage.pData           = pNewData;
                    messageCopy.decodeBufferPage.dataSizeInBytes = newSize;
                } else {
                    result = MA_OUT_OF_MEMORY;
                }
            }
        } else {
            result = MA_TOO_BIG;
        }
    }

    /* We should have the memory set up so now we can decode the next page. */
    if (result == MA_SUCCESS) {
        pRunningData = ma_offset_ptr(messageCopy.decodeBufferPage.pData, messageCopy.decodeBufferPage.decodedFrameCount * ma_get_bytes_per_frame(messageCopy.decodeBufferPage.pDecoder->outputFormat, messageCopy.decodeBufferPage.pDecoder->outputChannels));

        framesRead = ma_decoder_read_pcm_frames(messageCopy.decodeBufferPage.pDecoder, pRunningData, pageSizeInFrames);
        if (framesRead < pageSizeInFrames) {
            result = MA_AT_END;
        }

        /* If the total length is known we can increment out decoded frame count. Otherwise it needs to be left at 0 until the last page is decoded. */
        if (messageCopy.decodeBufferPage.isUnknownLength == MA_FALSE) {
            messageCopy.decodeBufferPage.pDataBuffer->data.decoded.decodedFrameCount += framesRead;
        }

        /* If there's more to decode, post a message to keep decoding. */
        if (result != MA_AT_END) {
            messageCopy.decodeBufferPage.decodedFrameCount += framesRead;

            result = ma_resource_manager_post_message(pResourceManager, &messageCopy);
        }
    }

    /*
    The result will be MA_SUCCESS if another page is in the queue for decoding. Otherwise it will be set to MA_AT_END if the end has been reached or
    any other result code if some other error occurred. If we are not decoding another page we need to free the decoder and close the file.
    */
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(messageCopy.decodeBufferPage.pDecoder);
        ma__free_from_callbacks(messageCopy.decodeBufferPage.pDecoder, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);

        /* When the length is unknown we were doubling the size of the buffer each time we needed more data. Let's try reducing this by doing a final realloc(). */
        if (messageCopy.decodeBufferPage.isUnknownLength) {
            ma_uint64 newSizeInBytes = messageCopy.decodeBufferPage.decodedFrameCount * ma_get_bytes_per_frame(messageCopy.decodeBufferPage.pDataBuffer->data.decoded.format, messageCopy.decodeBufferPage.pDataBuffer->data.decoded.channels);
            void* pNewData = ma__realloc_from_callbacks(messageCopy.decodeBufferPage.pData, (size_t)newSizeInBytes, messageCopy.decodeBufferPage.dataSizeInBytes, &pResourceManager->config.allocationCallbacks);
            if (pNewData != NULL) {
                messageCopy.decodeBufferPage.pData = pNewData;
                messageCopy.decodeBufferPage.dataSizeInBytes = (size_t)newSizeInBytes;    /* <-- Don't really need to set this, but I think it's good practice. */
            }
        }

        /*
        We can now set the frame counts appropriately. We want to set the frame count regardless of whether or not it had a known length just in case we have
        a weird situation where the frame count an opening time was different to the final count we got after reading.
        */
        messageCopy.decodeBufferPage.pDataBuffer->data.decoded.pData      = messageCopy.decodeBufferPage.pData;
        messageCopy.decodeBufferPage.pDataBuffer->data.decoded.frameCount = messageCopy.decodeBufferPage.decodedFrameCount;

        /*
        decodedFrameCount is what other threads will use to determine whether or not data is available. We must ensure pData and frameCount
        is set *before* setting the number of available frames. This way, the other thread need only check if decodedFrameCount > 0, in
        which case it can assume pData and frameCount are valid.
        */
        c89atomic_thread_fence(c89atomic_memory_order_seq_cst);
        messageCopy.decodeBufferPage.pDataBuffer->data.decoded.decodedFrameCount = messageCopy.decodeBufferPage.decodedFrameCount;


        /* If we reached the end we need to treat it as successful. */
        if (result == MA_AT_END) {
            result  = MA_SUCCESS;
        }

        /* We need to set the status of the page so other things can know about it. We can only change the status away from MA_BUSY. If it's anything else it cannot be changed. */
        c89atomic_compare_and_swap_32(&messageCopy.decodeBufferPage.pDataBuffer->result, MA_BUSY, result);

        /* We need to signal an event to indicate that we're done. */
        if (messageCopy.decodeBufferPage.pCompletedEvent != NULL) {
            ma_event_signal(messageCopy.decodeBufferPage.pCompletedEvent);
        }
    }

    return result;
}

static ma_result ma_resource_manager_handle_message__decode_stream_page(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage)
{
    ma_resource_manager_data_stream* pDataStream;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pMessage         != NULL);

    pDataStream = pMessage->decodeStreamPage.pDataStream;
    MA_ASSERT(pDataStream != NULL);

    /* For streams, the status should be MA_SUCCESS. */
    if (pMessage->decodeStreamPage.pDataStream->result != MA_SUCCESS) {
        return MA_INVALID_OPERATION;   
    }

    ma_resource_manager_data_stream_fill_page(pResourceManager, pMessage->decodeStreamPage.pDataStream, pMessage->decodeStreamPage.pageIndex);

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_handle_message__seek_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_data_stream* pDataStream, ma_uint64 frameIndex)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataStream      != NULL);

    /* For streams the status should be MA_SUCCESS for this to do anything. */
    if (pDataStream->result != MA_SUCCESS || pDataStream->isDecoderInitialized == MA_FALSE) {
        return MA_INVALID_OPERATION;
    }

    /*
    With seeking we just assume both pages are invalid and the relative frame cursor at at position 0. This is basically exactly the same as loading, except
    instead of initializing the decoder, we seek to a frame.
    */
    ma_decoder_seek_to_pcm_frame(&pDataStream->decoder, frameIndex);

    /* After seeking we'll need to reload the pages. */
    ma_resource_manager_data_stream_fill_pages(pResourceManager, pDataStream);

    /* We need to let the public API know that we're done seeking. */
    c89atomic_fetch_sub_32(&pDataStream->seekCounter, 1);

    return MA_SUCCESS;
}


MA_API ma_result ma_resource_manager_handle_message(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage)
{
    if (pResourceManager == NULL || pMessage == NULL) {
        return MA_INVALID_ARGS;
    }

    switch (pMessage->code)
    {
        case MA_MESSAGE_LOAD_DATA_BUFFER:
        {
            return ma_resource_manager_handle_message__load_data_buffer(pResourceManager, pMessage->loadDataBuffer.pDataBuffer, pMessage->loadDataBuffer.pFilePath, pMessage->loadDataBuffer.pEvent);
        } break;

        case MA_MESSAGE_FREE_DATA_BUFFER:
        {
            return ma_resource_manager_handle_message__free_data_buffer(pResourceManager, pMessage->freeDataBuffer.pDataBuffer);
        } break;


        case MA_MESSAGE_LOAD_DATA_STREAM:
        {
            return ma_resource_manager_handle_message__load_data_stream(pResourceManager, pMessage->loadDataStream.pDataStream, pMessage->loadDataStream.pFilePath, pMessage->loadDataStream.pEvent);
        } break;

        case MA_MESSAGE_FREE_DATA_STREAM:
        {
            return ma_resource_manager_handle_message__free_data_stream(pResourceManager, pMessage->freeDataStream.pDataStream, pMessage->freeDataStream.pEvent);
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


        case MA_MESSAGE_DECODE_BUFFER_PAGE:
        {
            return ma_resource_manager_handle_message__decode_buffer_page(pResourceManager, pMessage);
        } break;

        case MA_MESSAGE_DECODE_STREAM_PAGE:
        {
            return ma_resource_manager_handle_message__decode_stream_page(pResourceManager, pMessage);
        } break;

        case MA_MESSAGE_SEEK_DATA_STREAM:
        {
            return ma_resource_manager_handle_message__seek_data_stream(pResourceManager, pMessage->seekDataStream.pDataStream, pMessage->seekDataStream.frameIndex);
        } break;

        default: break;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_post_message(ma_resource_manager* pResourceManager, const ma_resource_manager_message* pMessage)
{
    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_message_queue_post(&pResourceManager->messageQueue, pMessage);
}

MA_API ma_result ma_resource_manager_next_message(ma_resource_manager* pResourceManager, ma_resource_manager_message* pMessage)
{
    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_message_queue_next(&pResourceManager->messageQueue, pMessage);
}

MA_API ma_result ma_resource_manager_peek_message(ma_resource_manager* pResourceManager, ma_resource_manager_message* pMessage)
{
    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_message_queue_peek(&pResourceManager->messageQueue, pMessage);
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
        ma_yield();
    }
}


static void ma_engine_mix_sound(ma_engine* pEngine, ma_sound_group* pGroup, ma_sound* pSound, ma_uint32 frameCount)
{
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pGroup  != NULL);
    MA_ASSERT(pSound  != NULL);

    c89atomic_exchange_32(&pSound->isMixing, MA_TRUE);  /* This must be done before checking the isPlaying state. */
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
                c89atomic_exchange_32(&pSound->isPlaying, MA_FALSE);
                c89atomic_exchange_32(&pSound->atEnd, MA_TRUE);         /* Set to false in ma_engine_sound_start(). */
            }
        }
    }
    c89atomic_exchange_32(&pSound->isMixing, MA_FALSE);
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
            c89atomic_exchange_ptr(&pGroup->pFirstSoundInGroup, pSound->pNextSoundInGroup);
        } else {
            /*
            The sound is not the head. We need to remove the sound from the group by simply changing the pNextSoundInGroup member of the previous sound. This is
            the important part. This is the part that allows the mixing thread to continue iteration without locking.
            */
            c89atomic_exchange_ptr(&pSound->pPrevSoundInGroup->pNextSoundInGroup, pSound->pNextSoundInGroup);
        }

        /* This doesn't really need to be done atomically because we've wrapped this in a lock and it's not used by the mixing thread. */
        if (pSound->pNextSoundInGroup != NULL) {
            c89atomic_exchange_ptr(&pSound->pNextSoundInGroup->pPrevSoundInGroup, pSound->pPrevSoundInGroup);
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

        c89atomic_exchange_ptr(&pGroup->pFirstSoundInGroup, pNewFirstSoundInGroup);
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
        return ma_engine_effect__on_process_pcm_frames__no_pre_effect_no_pitch(pEngineEffect, pFramesOut, &frameCountIn, pFramesOut, &frameCountOut);  /* Intentionally setting the input buffer to pFramesOut for in-place processing. */
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
    while (totalFramesProcessedIn < frameCountIn && totalFramesProcessedOut < frameCountOut) {
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
    c89atomic_exchange_32(&pSound->isPlaying, MA_TRUE);

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_sound_stop(ma_engine* pEngine, ma_sound* pSound)
{
    if (pEngine == NULL || pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_32(&pSound->isPlaying, MA_FALSE);

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

    c89atomic_exchange_32(&pSound->isLooping, isLooping);

    /*
    This is a little bit of a hack, but basically we need to set the looping flag at the data source level if we are running a data source managed by
    the resource manager, and that is backed by a data stream. The reason for this is that the data stream itself needs to be aware of the looping
    requirements so that it can do seamless loop transitions. The better solution for this is to add ma_data_source_set_looping() and just call this
    generically.
    */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pSound->pDataSource == &pSound->resourceManagerDataSource) {
        ma_resource_manager_data_source_set_looping(pEngine->pResourceManager, pSound->pDataSource, isLooping);
    }
#endif

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
    ma_uint32 dataSourceFlags = 0;

    if (pEngine == NULL || pFilePath == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pGroup == NULL) {
        pGroup = &pEngine->masterSoundGroup;
    }

    dataSourceFlags |= MA_DATA_SOURCE_FLAG_ASYNC;

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
            if (c89atomic_compare_and_swap_32(&pNextSound->atEnd, MA_TRUE, MA_FALSE) == MA_TRUE) {
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
        result = ma_resource_manager_data_source_init(pEngine->pResourceManager, pFilePath, dataSourceFlags, &pSound->resourceManagerDataSource);
        if (result != MA_SUCCESS) {
            /* We failed to load the resource. We need to return an error. We must also put this sound back up for recycling by setting the at-end flag to true. */
            c89atomic_exchange_32(&pSound->atEnd, MA_TRUE); /* <-- Put the sound back up for recycling. */
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

        result = ma_engine_sound_init_from_file(pEngine, pFilePath, dataSourceFlags, pGroup, pSound);
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

        c89atomic_exchange_ptr(&pGroup->pParent->pFirstChild, pGroup->pNextSibling);
    } else {
        /* It's not the first child in the parent group. */
        c89atomic_exchange_ptr(&pGroup->pPrevSibling->pNextSibling, pGroup->pNextSibling);
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

    c89atomic_exchange_32(&pGroup->isPlaying, MA_TRUE);

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

    c89atomic_exchange_32(&pGroup->isPlaying, MA_FALSE);

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
