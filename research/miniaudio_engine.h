/*
EXPERIMENTAL
============
Everything in this file is experimental and subject to change. Some stuff isn't yet implemented, in particular spatialization. I've noted some ideas that are
basically straight off the top of my head - many of these are probably outright wrong or just generally bad ideas.

Very simple APIs for spatialization are declared by not yet implemented. They're just placeholders to give myself an idea on some of the API design.

The idea is that you have an `ma_engine` object - one per listener. Decoupled from that is the `ma_resource_manager` object. You can have one `ma_resource_manager`
object to many `ma_engine` objects. This will allow you to share resources between each listener. The `ma_engine` is responsible for the playback of audio from a
list of data sources. The `ma_resource_manager` is responsible for the actual loading, caching and unloading of those data sources. This decoupling is
something that I'm really liking right now and will likely stay in place for the final version.

You create "sounds" from the engine which represent a sound/voice in the world. You first need to create a sound, and then you need to start it. Sounds do not
start by default. You can use `ma_engine_play_sound()` to "fire and forget" sounds. Sounds can have an effect (`ma_effect`) applied to it which can be set with
`ma_sound_set_effect()`.

Sounds can be allocated to groups called `ma_sound_group`. The creation and deletion of groups is not thread safe and should usually happen at initialization
time. Groups are how you handle submixing. In many games you will see settings to control the master volume in addition to groups, usually called SFX, Music
and Voices. The `ma_sound_group` object is how you would achieve this via the `ma_engine` API. When a sound is created you need to specify the group it should
be associated with. The sound's group cannot be changed after it has been created.

The creation and deletion of sounds should, hopefully, be thread safe. I have not yet done thorough testing on this, so there's a good chance there may be some
subtle bugs there.

The best resource to use when understanding the API is the function declarations for `ma_engine`. I expect you should be able to figure it out! :)
*/

/*
Memory Allocation Types
=======================
When allocating memory you may want to optimize your custom allocators based on what it is miniaudio is actually allocating. Normally the context in which you
are using the allocator is enough to optimize allocations, however there are high-level APIs that perform many different types of allocations and it can be
useful to be told exactly what it being allocated so you can optimize your allocations appropriately.
*/
#define MA_ALLOCATION_TYPE_GENERAL                              0x00000001  /* A general memory allocation. */
#define MA_ALLOCATION_TYPE_CONTEXT                              0x00000002  /* A ma_context allocation. */
#define MA_ALLOCATION_TYPE_DEVICE                               0x00000003  /* A ma_device allocation. */
#define MA_ALLOCATION_TYPE_DECODER                              0x00000004  /* A ma_decoder allocation. */
#define MA_ALLOCATION_TYPE_AUDIO_BUFFER                         0x00000005  /* A ma_audio_buffer allocation. */
#define MA_ALLOCATION_TYPE_ENCODED_BUFFER                       0x00000006  /* Allocation for encoded audio data containing the raw file data of a sound file. */
#define MA_ALLOCATION_TYPE_DECODED_BUFFER                       0x00000007  /* Allocation for decoded audio data from a sound file. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER_NODE    0x00000010  /* A ma_resource_manager_data_buffer_node object. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER         0x00000011  /* A ma_resource_manager_data_buffer_node object. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_STREAM         0x00000012  /* A ma_resource_manager_data_stream object. */
#define MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_SOURCE         0x00000013  /* A ma_resource_manager_data_source object. */

/*
Resource Management
===================
Many programs will want to manage sound resources for things such as reference counting and streaming. This is supported by miniaudio via the
`ma_resource_manager` API.

The resource manager is mainly responsible for the following:

    1) Loading of sound files into memory with reference counting.
    2) Streaming of sound data

When loading a sound file, the resource manager will give you back a data source compatible object called `ma_resource_manager_data_source`. This object can be
passed into any `ma_data_source` API which is how you can read and seek audio data. When loading a sound file, you specify whether or not you want the sound to
be fully loaded into memory (and optionally pre-decoded) or streamed. When loading into memory, you can also specify whether or not you want the data to be
loaded asynchronously.

The example below is how you can initialize a resource manager using it's default configuration:

    ```c
    ma_resource_manager_config config;
    ma_resource_manager resourceManager;

    config = ma_resource_manager_config_init();
    result = ma_resource_manager_init(&config, &resourceManager);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device);
        printf("Failed to initialize the resource manager.");
        return -1;
    }
    ```

You can configure the format, channels and sample rate of the decoded audio data. By default it will use the file's native data format, but you can configure
it to use a consistent format. This is useful for offloading the cost of data conversion to load time rather than dynamically converting a mixing time. To do
this, you configure the decoded format, channels and sample rate like the code below:

    ```c
    config = ma_resource_manager_config_init();
    config.decodedFormat     = device.playback.format;
    config.decodedChannels   = device.playback.channels;
    config.decodedSampleRate = device.sampleRate;
    ```

In the code above, the resource manager will be configured so that any decoded audio data will be pre-converted at load time to the device's native data
format. If instead you used defaults and the data format of the file did not match the device's data format, you would need to convert the data at mixing time
which may be prohibitive in high-performance and large scale scenarios like games.

Asynchronicity is achieved via a job system. When an operation needs to be performed, such as the decoding of a page, a job will be posted to a queue which
will then be processed by a job thread. By default there will be only one job thread running, but this can be configured, like so:

    ```c
    config = ma_resource_manager_config_init();
    config.jobThreadCount = MY_JOB_THREAD_COUNT;
    ```

By default job threads are managed internally by the resource manager, however you can also self-manage your job threads if, for example, you want to integrate
the job processing into your existing job infrastructure, or if you simply don't like the way the resource manager does it. To do this, just set the job thread
count to 0 and process jobs manually. To process jobs, you first need to retrieve a job using `ma_resource_manager_next_job()` and then process it using
`ma_resource_manager_process_job()`:

    ```c
    config = ma_resource_manager_config_init();
    config.jobThreadCount = 0;                            // Don't manage any job threads internally.
    config.flags = MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING; // Optional. Makes `ma_resource_manager_next_job()` non-blocking.

    // ... Initialize your custom job threads ...

    void my_custom_job_thread(...)
    {
        for (;;) {
            ma_job job;
            ma_result result = ma_resource_manager_next_job(pMyResourceManager, &job);
            if (result != MA_SUCCESS) {
                if (result == MA_NOT_DATA_AVAILABLE) {
                    // No jobs are available. Keep going. Will only get this if the resource manager was initialized
                    // with MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING.
                    continue;
                } else if (result == MA_CANCELLED) {
                    // MA_JOB_QUIT was posted. Exit.
                    break;
                } else {
                    // Some other error occurred.
                    break;
                }
            }

            ma_resource_manager_process_job(pMyResourceManager, &job);
        }
    }
    ```

In the example above, the MA_JOB_QUIT event is the used as the termination indicator. You can instead use whatever variable you would like to terminate the
thread. The call to `ma_resource_manager_next_job()` is blocking by default, by can be configured to be non-blocking by initializing the resource manager
with the MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING configuration flag.

When loading a file, it's sometimes convenient to be able to customize how files are opened and read. This can be done by setting `pVFS` member of the
resource manager's config:

    ```c
    // Initialize your custom VFS object. See documentation for VFS for information on how to do this.
    my_custom_vfs vfs = my_custom_vfs_init();

    config = ma_resource_manager_config_init();
    config.pVFS = &vfs;
    ```

If you do not specify a custom VFS, the resource manager will use the operating system's normal file operations. This is default.

To load a sound file and create a data source, call `ma_resource_manager_data_source_init()`. When loading a sound you need to specify the file path and
options for how the sounds should be loaded. By default a sound will be loaded synchronously. The returned data source is owned by the caller which means the
caller is responsible for the allocation and freeing of the data source. Below is an example for initializing a data source:

    ```c
    ma_resource_manager_data_source dataSource;
    ma_result result = ma_resource_manager_data_source_init(pResourceManager, pFilePath, flags, &dataSource);
    if (result != MA_SUCCESS) {
        // Error.
    }

    // ...

    // A ma_resource_manager_data_source object is compatible with the `ma_data_source` API. To read data, just call
    // the `ma_data_source_read_pcm_frames()` like you would with any normal data source.
    result = ma_data_source_read_pcm_frames(&dataSource, pDecodedData, frameCount, &framesRead);
    if (result != MA_SUCCESS) {
        // Failed to read PCM frames.
    }

    // ...

    ma_resource_manager_data_source_uninit(pResourceManager, &dataSource);
    ```

The `flags` parameter specifies how you want to perform loading of the sound file. It can be a combination of the following flags:

    ```
    MA_DATA_SOURCE_STREAM
    MA_DATA_SOURCE_DECODE
    MA_DATA_SOURCE_ASYNC
    ```

When no flags are specified (set to 0), the sound will be fully loaded into memory, but not decoded, meaning the raw file data will be stored in memory, and
then dynamically decoded when `ma_data_source_read_pcm_frames()` is called. To instead decode the audio data before storing it in memory, use the
`MA_DATA_SOURCE_DECODE` flag. By default, the sound file will be loaded synchronously, meaning `ma_resource_manager_data_source_init()` will only return after
the entire file has been loaded. This is good for simplicity, but can be prohibitively slow. You can instead load the sound asynchronously using the
`MA_DATA_SOURCE_ASYNC` flag. This will result in `ma_resource_manager_data_source_init()` returning quickly, but no data will be returned by
`ma_data_source_read_pcm_frames()` until some data is available. When no data is available because the asynchronous decoding hasn't caught up, MA_BUSY will be
returned by `ma_data_source_read_pcm_frames()`.

For large sounds, it's often prohibitive to store the entire file in memory. To mitigate this, you can instead stream audio data which you can do by specifying
the `MA_DATA_SOURCE_STREAM` flag. When streaming, data will be decoded in 1 second pages. When a new page needs to be decoded, a job will be posted to the job
queue and then subsequently processed in a job thread.

When loading asynchronously, it can be useful to poll whether or not loading has finished. Use `ma_resource_manager_data_source_result()` to determine this.
For in-memory sounds, this will return `MA_SUCCESS` when the file has been *entirely* decoded. If the sound is still being decoded, `MA_BUSY` will be returned.
Otherwise, some other error code will be returned if the sound failed to load. For streaming data sources, `MA_SUCCESS` will be returned when the first page
has been decoded and the sound is ready to be played. If the first page is still being decoded, `MA_BUSY` will be returned. Otherwise, some other error code
will be returned if the sound failed to load.

For in-memory sounds, reference counting is used to ensure the data is loaded only once. This means multiple calls to `ma_resource_manager_data_source_init()`
with the same file path will result in the file data only being loaded once. Each call to `ma_resource_manager_data_source_init()` must be matched up with a
call to `ma_resource_manager_data_source_uninit()`. Sometimes it can be useful for a program to register self-managed raw audio data and associate it with a
file path. Use `ma_resource_manager_register_decoded_data()`, `ma_resource_manager_register_encoded_data()` and `ma_resource_manager_unregister_data()` to do
this. `ma_resource_manager_register_decoded_data()` is used to associate a pointer to raw, self-managed decoded audio data in the specified data format with
the specified name. Likewise, `ma_resource_manager_register_encoded_data()` is used to associate a pointer to raw self-managed encoded audio data (the raw
file data) with the specified name. Note that these names need not be actual file paths. When `ma_resource_manager_data_source_init()` is called (without the
`MA_DATA_SOURCE_STREAM` flag), the resource manager will look for these explicitly registered data buffers and, if found, will use it as the backing data for
the data source. Note that the resource manager does *not* make a copy of this data so it is up to the caller to ensure the pointer stays valid for it's
lifetime. Use `ma_resource_manager_unregister_data()` to unregister the self-managed data. It does not make sense to use the `MA_DATA_SOURCE_STREAM` flag with
a self-managed data pointer. When `MA_DATA_SOURCE_STREAM` is specified, it will try loading the file data through the VFS.


Resource Manager Implementation Details
---------------------------------------
Resources are managed in two main ways:

    1) By storing the entire sound inside an in-memory buffer (referred to as a data buffer - `ma_resource_manager_data_buffer_node`)
    2) By streaming audio data on the fly (referred to as a data stream - `ma_resource_manager_data_stream`)

A resource managed data source (`ma_resource_manager_data_source`) encapsulates a data buffer or data stream, depending on whether or not the data source was
initialized with the `MA_DATA_SOURCE_FLAG_STREAM` flag. If so, it will make use of a `ma_resource_manager_data_stream` object. Otherwise it will use a
`ma_resource_manager_data_buffer_node` object.

Another major feature of the resource manager is the ability to asynchronously decode audio files. This relieves the audio thread of time-consuming decoding
which can negatively affect scalability due to the audio thread needing to complete it's work extremely quickly to avoid glitching. Asynchronous decoding is
achieved through a job system. There is a central multi-producer, multi-consumer, lock-free, fixed-capacity job queue. When some asynchronous work needs to be
done, a job is posted to the queue which is then read by a job thread. The number of job threads can be configured for improved scalability, and job threads
can all run in parallel without needing to worry about the order of execution (how this is achieved is explained below).

When a sound is being loaded asynchronously, playback can begin before the sound has been fully decoded. This enables the application to start playback of the
sound quickly, while at the same time allowing to resource manager to keep loading in the background. Since there may be less threads than the number of sounds
being loaded at a given time, a simple scheduling system is used to keep decoding time fair. The resource manager solves this by splitting decoding into chunks
called pages. By default, each page is 1 second long. When a page has been decoded, the a new job will be posted to start decoding the next page. By dividing
up decoding into pages, an individual sound shouldn't ever delay every other sound from having their first page decoded. Of course, when loading many sounds at
the same time, there will always be an amount of time required to process jobs in the queue so in heavy load situations there will still be some delay. To
determine if a data source is ready to have some frames read, use `ma_resource_manager_data_source_get_available_frames()`. This will return the number of
frames available starting from the current position.


Data Buffers
------------
When the `MA_DATA_SOURCE_FLAG_STREAM` flag is not specified at initialization time, the resource manager will try to load the data into an in-memory data
buffer. Before doing so, however, it will first check if the specified file has already been loaded. If so, it will increment a reference counter and just use
the already loaded data. This saves both time and memory. A binary search tree (BST) is used for storing data buffers as it has good balance between efficiency
and simplicity. The key of the BST is a 64-bit hash of the file path that was passed into `ma_resource_manager_data_source_init()`. The advantage of using a
hash is that it saves memory over storing the entire path, has faster comparisons, and results in a mostly balanced BST due to the random nature of the hash.
The disadvantage is that file names are case-sensitive. If this is an issue, you should normalize your file names to upper- or lower-case before initializing
your data sources.

When a sound file has not already been loaded and the `MA_DATA_SOURCE_ASYNC` is not specified, the file will be decoded synchronously by the calling thread.
There are two options for controlling how the audio is stored in the data buffer - encoded or decoded. When the `MA_DATA_SOURCE_DECODE` option is not
specified, the raw file data will be stored in memory. Otherwise the sound will be decoded before storing it in memory. Synchronous loading is a very simple
and standard process of simply adding an item to the BST, allocating a block of memory and then decoding (if `MA_DATA_SOURCE_DECODE` is specified).

When the `MA_DATA_SOURCE_ASYNC` flag is specified, loading of the data buffer is done asynchronously. In this case, a job is posted to the queue to start
loading and then the function instantly returns, setting an internal result code to `MA_BUSY`. This result code is returned when the program calls
`ma_resource_manager_data_source_result()`. When decoding has fully completed, `MA_RESULT` will be returned. This can be used to know if loading has fully
completed.

When loading asynchronously, a single job is posted to the queue of the type `MA_JOB_LOAD_DATA_BUFFER`. This involves making a copy of the file path and
associating it with job. When the job is processed by the job thread, it will first load the file using the VFS associated with the resource manager. When
using a custom VFS, it's important that it be completely thread-safe because it will be used from one or more job threads at the same time. Individual files
should only ever be accessed by one thread at a time, however. After opening the file via the VFS, the job will determine whether or not the file is being
decoded. If not, it simply allocates a block of memory and loads the raw file contents into it and returns. On the other hand, when the file is being decoded,
it will first allocate a decoder on the heap and initialize it. Then it will check if the length of the file is known. If so it will allocate a block of memory
to store the decoded output and initialize it to silence. If the size is unknown, it will allocate room for one page. After memory has been allocated, the
first page will be decoded. If the sound is shorter than a page, the result code will be set to `MA_SUCCESS` and the completion event will be signalled and
loading is now complete. If, however, there is store more to decode, a job with the code `MA_JOB_PAGE_DATA_BUFFER` is posted. This job will decode the next
page and perform the same process if it reaches the end. If there is more to decode, the job will post another `MA_JOB_PAGE_DATA_BUFFER` job which will keep on
happening until the sound has been fully decoded. For sounds of an unknown length, the buffer will be dynamically expanded as necessary, and then shrunk with a
final realloc() when the end of the file has been reached.


Data Streams
------------
Data streams only ever store two pages worth of data for each sound. They are most useful for large sounds like music tracks in games which would consume too
much memory if fully decoded in memory. Only two pages of audio data are stored in memory at a time for each data stream. After every frame from a page has
been read, a job will be posted to load the next page which is done from the VFS.

For data streams, the `MA_DATA_SOURCE_FLAG_ASYNC` flag will determine whether or not initialization of the data source waits until the two pages have been
decoded. When unset, `ma_resource_manager_data_source()` will wait until the two pages have been loaded, otherwise it will return immediately.

When frames are read from a data stream using `ma_resource_manager_data_source_read_pcm_frames()`, `MA_BUSY` will be returned if there are no frames available.
If there are some frames available, but less than the number requested, `MA_SUCCESS` will be returned, but the actual number of frames read will be less than
the number requested. Due to the asymchronous nature of data streams, seeking is also asynchronous. If the data stream is in the middle of a seek, `MA_BUSY`
will be returned when trying to read frames.

When `ma_resource_manager_data_source_read_pcm_frames()` results in a page getting fully consumed, a job is posted to load the next page. This will be posted
from the same thread that called `ma_resource_manager_data_source_read_pcm_frames()` which should be lock-free.

Data streams are uninitialized by posting a job to the queue, but the function won't return until that job has been processed. The reason for this is that the
caller owns the data stream object and therefore we need to ensure everything completes before handing back control to the caller. Also, if the data stream is
uninitialized while pages are in the middle of decoding, they must complete before destroying any underlying object and the job system handles this cleanly.


Job Queue
---------
The resource manager uses a job queue which is multi-producer, multi-consumer, lock-free and fixed-capacity. The lock-free property of the queue is achieved
using the algorithm described by Michael and Scott: Nonblocking Algorithms and Preemption-Safe Locking on Multiprogrammed Shared Memory Multiprocessors. In
order for this to work, only a fixed number of jobs can be allocated and inserted into the queue which is done through a lock-free data structure for
allocating an index into a fixed sized array, with reference counting for mitigation of the ABA problem. The reference count is 32-bit.

For many types of jobs it's important that they execute in a specific order. In these cases, jobs are executed serially. The way in which each type of job
handles this is specific to the job type. For the resource manager, serial execution of jobs is only required on a per-object basis (per data buffer or per
data stream). Each of these objects stores an execution counter. When a job is posted it is associated with an execution counter. When the job is processed, it
checks if the execution counter of the job equals the execution counter of the owning object and if so, processes the job. If the counters are not equal, the
job will be posted back onto the job queue for later processing. When the job finishes processing the execution order of the main object is incremented. This
system means the no matter how many job threads are executing, decoding of an individual sound will always get processed serially. The advantage to having
multiple threads comes into play when loading multiple sounds at the time time.
*/
#ifndef miniaudio_engine_h
#define miniaudio_engine_h

#ifdef __cplusplus
extern "C" {
#endif


/*
Effects
=======
The `ma_effect` API is a mid-level API for chaining together effects. This is a wrapper around lower level APIs which you can continue to use by themselves if
this API does not work for you.

Effects can be linked together as a chain, with one input and one output. When processing audio data through an effect, it starts at the top of the chain and
works it's way down.


Usage
-----
To apply the effect to some audio data, do something like the following:

    ```c
    ma_uint64 framesToProcessIn  = availableInputFrameCount;
    ma_uint64 framesToProcessOut = frameCountOut;
    ma_result result = ma_effect_process_pcm_frames(pEffect, pFramesIn, &framesToProcessIn, pFramesOut, &framesToProcessOut);
    if (result != MA_SUCCESS) {
        // Error.
    }

    // At this point framesToProcessIn contains the number of input frames that were consumed and framesToProcessOut contains the number of output frames that
    // were processed.
    ```

Some effects can change the sample rate, which means the number of output frames may be different to the number of input frames consumed. Therefore they both
need to be specified when processing a chunk of audio data.
*/
#define MA_EFFECT_MIN_INPUT_STREAM_COUNT    1
#define MA_EFFECT_MAX_INPUT_STREAM_COUNT    4

typedef void ma_effect;

typedef struct ma_effect_base ma_effect_base;
struct ma_effect_base
{
    ma_result (* onProcessPCMFrames)(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
    ma_uint64 (* onGetRequiredInputFrameCount)(ma_effect* pEffect, ma_uint64 outputFrameCount);
    ma_uint64 (* onGetExpectedOutputFrameCount)(ma_effect* pEffect, ma_uint64 inputFrameCount);
    ma_result (* onGetInputDataFormat)(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
    ma_result (* onGetOutputDataFormat)(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
};

MA_API ma_result ma_effect_process_pcm_frames(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
MA_API ma_result ma_effect_process_pcm_frames_ex(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut);
MA_API ma_result ma_effect_process_pcm_frames_with_conversion(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut, ma_format formatIn, ma_uint32 channelsIn, ma_format formatOut, ma_uint32 channelsOut);
MA_API ma_uint64 ma_effect_get_required_input_frame_count(ma_effect* pEffect, ma_uint64 outputFrameCount);
MA_API ma_uint64 ma_effect_get_expected_output_frame_count(ma_effect* pEffect, ma_uint64 inputFrameCount);
MA_API ma_result ma_effect_get_output_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API ma_result ma_effect_get_input_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);



/*
Mixing
======
Mixing is done via the ma_mixer API. You can use this if you want to mix multiple sources of audio together and play them all at the same time, layered on top
of each other. This is a mid-level procedural API. Do not confuse this with a high-level data-driven API. You do not "attach" and "detach" sounds, but instead
write raw audio data directly into an accumulation buffer procedurally. High-level data-driven APIs will be coming at a later date.

Below are the features of the ma_mixer API:

    * Mixing to and from any data format with seamless conversion when necessary.
      * Initialize the `ma_mixer` object using whatever format is convenient, and then mix audio in any other format with seamless data conversion.
    * Submixing (mix one `ma_mixer` directly into another `ma_mixer`, with volume and effect control).
    * Volume control.
    * Effects (via the `ma_effect` API).
    * Mix directly from raw audio data in addition to `ma_decoder`, `ma_waveform`, `ma_noise`, `ma_pcm_rb` and `ma_rb` objects.

Mixing sounds together is as simple as summing their samples. As samples are summed together they are stored in a buffer called the accumulation buffer. In
order to ensure there is enough precision to store the intermediary results, the accumulation buffer needs to be at a higher bit depth than the sample format
being mixed, with the exception of floating point. Below is a mapping of the sample format and the data type of the accumulation buffer:

    +---------------+------------------------+
    | Sample Format | Accumulation Data Type |
    +---------------+------------------------+
    | ma_format_u8  | ma_int16               |
    | ma_format_s16 | ma_int32               |
    | ma_format_s24 | ma_int64               |
    | ma_format_s32 | ma_int64               |
    | ma_format_f32 | float                  |
    +---------------+------------------------+

The size of the accumulation buffer is fixed and must be specified at initialization time. When you initialize a mixer you need to also specify a sample format
which will be the format of the returned data after mixing. The format is also what's used to determine the bit depth to use for the accumulation buffer and
how to interpret the data contained within it. You must also specify a channel count in order to support interleaved multi-channel data. The sample rate is not
required by the mixer as it only cares about raw sample data.

The mixing process involves three main steps:

    1) Clearing the accumulation buffer to zero
         ma_mixer_begin()

    2) Accumulating all audio sources
         ma_mixer_mix_pcm_frames()
         ma_mixer_mix_data_source()
         ma_mixer_mix_rb()
         ma_mixer_mix_pcm_rb()

    3) Volume, clipping, effects and final output
         ma_mixer_end()

At the beginning of mixing the accumulation buffer will be cleared to zero. When you begin mixing you need to specify the number of PCM frames you want to
output at the end of mixing. If the requested number of output frames exceeds the capacity of the internal accumulation buffer, it will be clamped and returned
back to the caller. An effect can be applied at the end of mixing (after volume and clipping). Effects can do resampling which means the number of input frames
required to generate the requested number of output frames may be different. Therefore, another parameter is required which will receive the input frame count.
When mixing audio sources, you must do so based on the input frame count, not the output frame count (usage examples are in the next section).

After the accumulation buffer has been cleared to zero (the first step), you can start mixing audio data. When you mix audio data you should do so based on the
required number of input frames returned by ma_mixer_begin() or ma_mixer_begin_submix(). You can specify audio data in any data format in which case the data
will be automatically converted to the format required by the accumulation buffer. Input data can be specified in multiple ways:

    - A pointer to raw PCM data
    - A data source (ma_data_source, ma_decoder, ma_audio_buffer, ma_waveform, ma_noise)
    - A ring buffer (ma_rb, ma_pcm_rb)

Once you've finished accumulating all of your audio sources you need to perform a post process step which performs the final volume adjustment, clipping,
effects and copying to the specified output buffer in the format specified when the mixer was initialized. Volume is applied before clipping, which is applied
before the effect, which is done before final output. In between these steps is all of the necessary data conversion, so for performance it's important to be
mindful of where and when data will be converted.

The mixing API in miniaudio supports seamless data conversion at all stages of the mixing pipeline. If you're not mindful about the data formats used by each
of the different stages of the mixing pipeline you may introduce unnecessary inefficiency. For maximum performance you should use a consistent sample format,
channel count and sample rate for as much of the mixing pipeline as possible. As soon as you introduce a different format, the mixing pipeline will perform the
necessary data conversion.



Usage
-----
Initialize a mixer like the following:

    ```c
    ma_mixer_config config = ma_mixer_config_init(ma_format_f32, 2, 1024, NULL);

    ma_mixer mixer;
    result = ma_mixer_init(&config, &mixer);
    if (result != MA_SUCCESS) {
        // An error occurred.
    }
    ```

Before you can initialize a mixer you need to specify it's configuration via a `ma_mixer_config` object. This can be created with `ma_mixer_config_init()`
which requires the mixing format, channel count, size of the intermediary buffer in PCM frames and an optional pointer to a pre-allocated accumulation buffer.
Once you have the configuration set up, you can call `ma_mixer_init()` to initialize the mixer. If you passed in NULL for the pre-allocated accumulation buffer
this will allocate it on the stack for you, using custom allocation callbacks specified in the `allocationCallbacks` member of the mixer config.

Below is an example for mixing two decoders together:

    ```c
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut = desiredOutputFrameCount;
    ma_mixer_begin(&mixer, NULL, &frameCountOut, &frameCountIn);
    {
        // At this point, frameCountIn contains the number of frames we should be mixing in this iteration, whereas frameCountOut contains the number of output
        // frames we'll be outputting in ma_mixer_end().
        ma_mixer_mix_data_source(&mixer, &decoder1, 0, frameCountIn, isLooping1);
        ma_mixer_mix_data_source(&mixer, &decoder2, 0, frameCountIn, isLooping2);
    }
    ma_mixer_end(&mixer, NULL, pFinalMix, 0); // pFinalMix must be large enough to store frameCountOut frames in the mixer's format (specified at initialization time).
    ```

When you want to mix sounds together, you need to specify how many output frames you would like to end up with by the end. This depends on the size of the
accumulation buffer, however, which is of a fixed size. Therefore, the number of output frames you ask for is not necessarily what you'll get. In addition, an
effect can be applied at the end of mixing, and since that may perform resampling, the number of input frames required to generate the desired number of output
frames may differ which means you must also specify a pointer to a variable which will receive the required input frame count. In order to avoid glitching you
should write all of these input frames if they're available.

The ma_mixer API uses a sort of "immediate mode" design. The idea is that you "begin" and "end" mixing. When you begin mixing a number of frames you need to
call `ma_mixer_begin()`. This will initialize the accumulation buffer to zero (silence) in preparation for mixing. Next, you can start mixing audio data which
can be done in several ways, depending on the source of the audio data. In the example above we are using a `ma_decoder` as the input data source. This will
automatically convert the input data to an appropriate format for mixing.

Each call to ma_mixer_mix_*() accumulates from the beginning of the accumulation buffer.

Once all of your input data has been mixed you need to call `ma_mixer_end()`. This is where the data in the accumulation buffer has volume applied, is clipped
and has the effect applied, in that order. Finally, the data is output to the specified buffer in the format specified when the mixer was first initialized,
overwriting anything that was previously contained within the buffer, unless it's a submix in which case it will be mixed with the parent mixer. See section
below for more details.

The mixing API also supports submixing. This is where the final output of one mixer is mixed directly into the accumulation buffer of another mixer. A common
example is a game with a music submix and an effects submix, which are then combined to form the master mix. Example:

    ```c
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut = desiredOutputFrameCount;  // <-- Must be set to the desired number of output frames. Upon returning, will contain the actual number of output frames.
    ma_mixer_begin(&masterMixer, NULL, &frameCountOut, &frameCountIn);
    {
        ma_uint64 submixFrameCountIn;
        ma_uint64 submixFrameCountOut;  // <-- No pre-initialization required for a submix as it's derived from the parent mix's input frame count.

        // Music submix.
        ma_mixer_begin(&musicMixer, &masterMixer, &submixFrameCountIn, &submixFrameCountOut);
        {
            ma_mixer_mix_data_source(&musicMixer, &musicDecoder, 0, submixFrameCountIn, isMusicLooping);
        }
        ma_mixer_end(&musicMixer, &masterMixer, NULL, 0);

        // Effects submix.
        ma_mixer_begin(&effectsMixer, &masterMixer, &submixFrameCountIn, &submixFrameCountOut);
        {
            ma_mixer_mix_data_source(&effectsMixer, &decoder1, 0, frameCountIn, isLooping1);
            ma_mixer_mix_data_source(&effectsMixer, &decoder2, 0, frameCountIn, isLooping2);
        }
        ma_mixer_end(&effectsMixer, &masterMixer, NULL, 0);
    }
    ma_mixer_end(&masterMixer, NULL, pFinalMix); // pFinalMix must be large enough to store frameCountOut frames in the mixer's format (specified at initialization time).
    ```

If you want to use submixing, you need to ensure the accumulation buffers of each mixer is large enough to accomodate each other. That is, the accumulation
buffer of the sub-mixer needs to be large enough to store the required number of input frames returned by the parent call to `ma_mixer_begin()`. If you are not
doing any resampling you can just make the accumulation buffers the same size and you will fine. If you want to submix, you can only call `ma_mixer_begin()`
between the begin and end pairs of the parent mixer, which can be a master mix or another submix.



Implementation Details and Performance Guidelines
-------------------------------------------------
There are two main factors which affect mixing performance: data conversion and data movement. This section will detail the implementation of the ma_mixer API
and hopefully give you a picture on how best to identify and avoid potential performance pitfalls.

TODO: Write me.

Below a summary of some things to keep in mind for high performance mixing:

    * Choose a sample format at compile time and use it for everything. Optimized pipelines will be implemented for ma_format_s16 and ma_format_f32. The most
      common format is ma_format_f32 which will work in almost all cases. If you're building a game, ma_format_s16 may also work. Professional audio work will
      likely require ma_format_f32 for the added precision for authoring work. Do not use ma_format_s24 if you have high performance requirements as it is not
      nicely aligned and thus requires an inefficient conversion to 32-bit.

    * If you're building a game, try to use a consistent sample format, channel count and sample rate for all of your audio files, or at least all of your
      audio files for a specific category (same format for all sfx, same format for all music, same format for all voices, etc.)

    * Be mindful of when you perform resampling. Most desktop platforms output at a sample rate of 48000Hz or 44100Hz. If your input data is, for example,
      22050Hz, consider doing your mixing at 22050Hz, and then doing a final resample to the playback device's output format. In this example, resampling all
      of your data sources to 48000Hz before mixing may be unnecessarily inefficient because it'll need to perform mixing on a greater number of samples.
*/

MA_API size_t ma_get_accumulation_bytes_per_sample(ma_format format);
MA_API size_t ma_get_accumulation_bytes_per_frame(ma_format format, ma_uint32 channels);

typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint64 accumulationBufferSizeInFrames;
    void* pPreAllocatedAccumulationBuffer;
    ma_allocation_callbacks allocationCallbacks;
    float volume;
} ma_mixer_config;

MA_API ma_mixer_config ma_mixer_config_init(ma_format format, ma_uint32 channels, ma_uint64 accumulationBufferSizeInFrames, void* pPreAllocatedAccumulationBuffer, const ma_allocation_callbacks* pAllocationCallbacks);


typedef struct
{
    ma_format format;   /* This will be the format output by ma_mixer_end(). */
    ma_uint32 channels;
    ma_uint64 accumulationBufferSizeInFrames;
    void* pAccumulationBuffer;  /* In the accumulation format. */
    ma_allocation_callbacks allocationCallbacks;
    ma_bool32 ownsAccumulationBuffer;
    float volume;
    ma_effect* pEffect; /* The effect to apply after mixing input sources. */
    struct
    {
        ma_uint64 frameCountIn;
        ma_uint64 frameCountOut;
        ma_bool32 isInsideBeginEnd;
    } mixingState;
} ma_mixer;

/*
Initialize a mixer.

A mixer is used to mix/layer/blend sounds together.


Parameters
----------
pConfig (in)
    A pointer to the mixer's configuration. Cannot be NULL. See remarks.

pMixer (out)
    A pointer to the mixer object being initialized.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Thread Safety
-------------
Unsafe. You should not be trying to initialize a mixer from one thread, while at the same time trying to use it on another.


Callback Safety
---------------
This is safe to call in the data callback, but do if you do so, keep in mind that if you do not supply a pre-allocated accumulation buffer it will allocate
memory on the heap for you.


Remarks
-------
The mixer can be configured via the `pConfig` argument. The config object is initialized with `ma_mixer_config_init()`. Individual configuration settings can
then be set directly on the structure. Below are the members of the `ma_mixer_config` object.

    format
        The sample format to use for mixing. This is the format that will be output by `ma_mixer_end()`.

    channels
        The channel count to use for mixing. This is the number of channels that will be output by `ma_mixer_end()`.

    accumulationBufferSizeInFrames
        A mixer uses a fixed sized buffer for it's entire life time. This specifies the size in PCM frames of the accumulation buffer. When calling
        `ma_mixer_begin()`, the requested output frame count will be clamped based on the value of this property. You should not use this propertry to
        determine how many frames to mix at a time with `ma_mixer_mix_*()` - use the value returned by `ma_mixer_begin()`.

    pPreAllocatedAccumulationBuffer
        A pointer to a pre-allocated buffer to use for the accumulation buffer. This can be null in which case a buffer will be allocated for you using the
        specified allocation callbacks, if any. You can calculate the size in bytes of the accumulation buffer like so:

            ```c
            sizeInBytes = config.accumulationBufferSizeInFrames * ma_get_accumulation_bytes_per_frame(config.format, config.channels)
            ```

        Note that you should _not_ use `ma_get_bytes_per_frame()` when calculating the size of the buffer because the accumulation buffer requires a higher bit
        depth for accumulation in order to avoid wrapping.

    allocationCallbacks
        Memory allocation callbacks to use for allocating memory for the accumulation buffer. If all callbacks in this object are NULL, `MA_MALLOC()` and
        `MA_FREE()` will be used.

    volume
        The default output volume in linear scale. Defaults to 1. This can be changed after initialization with `ma_mixer_set_volume()`.
*/
MA_API ma_result ma_mixer_init(ma_mixer_config* pConfig, ma_mixer* pMixer);

/*
Uninitializes a mixer.


Parameters:
-----------
pMixer (in)
    A pointer to the mixer being unintialized.


Thread Safety
-------------
Unsafe. You should not be uninitializing a mixer while using it on another thread.


Callback Safety
---------------
If you did not specify a pre-allocated accumulation buffer, this will free it.


Remarks
-------
If you specified a pre-allocated buffer it will be left as-is. Otherwise it will be freed using the allocation callbacks specified in the config when the mixer
was initialized.
*/
MA_API void ma_mixer_uninit(ma_mixer* pMixer);

/*
Marks the beginning of a mix of a specified number of frames.

When you begin mixing, you must specify how many frames you want to mix. You specify the number of output frames you want, and upon returning you will receive
the number of output frames you'll actually get. When an effect is attached, there may be a chance that the number of input frames required to output the given
output frame count differs. The input frame count is also returned, and this is number of frames you must use with the `ma_mixer_mix_*()` APIs, provided that
number of input frames are available to you at mixing time.

Each call to `ma_mixer_begin()` must be matched with a call to `ma_mixer_end()`. In between these you mix audio data using the `ma_mixer_mix_*()` APIs. When
you call `ma_mixer_end()`, the number of frames that are output will be equal to the output frame count. When you call `ma_mixer_mix_*()`, you specify a frame
count based on the input frame count.


Parameters
----------
pMixer (in)
    A pointer to the relevant mixer.

pParentMixer (in, optional)
    A pointer to the parent mixer. Set this to non-NULL if you want the output of `pMixer` to be mixed with `pParentMixer`. Otherwise, if you want to output
    directly to a buffer, set this to NULL. You would set this to NULL for a master mixer, and non-NULL for a submix. See remarks.

pFrameCountOut (in, out)
    On input, specifies the desired number of output frames to mix in this iteration. The requested number of output frames may not be able to fit in the
    internal accumulation buffer which means on output this variable will receive the actual number of output frames. On input, this will be ignored if
    `pParentMixer` is non-NULL because the output frame count of a submix must be compatible with the parent mixer.

pFramesCountIn (out)
    A pointer to the variable that will receive the number of input frames to mix with each call to `ma_mixer_mix_*()`. This will usually always equal the
    output frame count, but will be different if an effect is applied and that effect performs resampling. See remarks.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Thread Safety
-------------
This can be called from any thread so long as you perform your own synchronization against the `pMixer` and `pParentMixer` object.


Callback Safety
---------------
Safe.


Remarks
-------
When you call `ma_mixer_begin()`, you need to specify how many output frames you want. The number of input frames required to generate those output frames can
differ, however. This will only happen if you have an effect attached (see `ma_mixer_set_effect()`) and if one of the effects in the chain performs resampling.
The input frame count will be returned by the `pFrameCountIn` parameter, and this is how many frames should be used when mixing with `ma_mixer_mix_*()`. See
examples below.

The mixer API supports the concept of submixing which is where the output of one mixer is mixed with that of another. A common example from a game:

    Master
        SFX
        Music
        Voices

In the example above, "Master" is the master mix and "SFX", "Music" and "Voices" are submixes. When you call `ma_mixer_begin()` for the "Master" mix, you would
set `pParentMixer` to NULL. For the "SFX", "Music" and "Voices" you would set it to a pointer to the master mixer, and you must call `ma_mixer_begin()` and
`ma_mixer_end()` between the begin and end pairs of the parent mixer. If you want to perform submixing, you need to pass the same parent mixer (`pParentMixer`)
to `ma_mixer_end()`. See example 2 for an example on how to do submixing.


Example 1
---------
This example shows a basic mixer without any submixing.

```c
ma_uint64 frameCountIn;
ma_uint64 frameCountOut = desiredFrameCount;    // <-- On input specifies what you want, on output receives what you actually got.
ma_mixer_begin(&mixer, NULL, &frameCountOut, &frameCountIn);
{
    ma_mixer_mix_data_source(&mixer, &decoder1, frameCountIn, isLooping1);
    ma_mixer_mix_data_source(&mixer, &decoder2, frameCountIn, isLooping2);
}
ma_mixer_end(&mixer, NULL, pFramesOut); // <-- pFramesOut must be large enough to receive frameCountOut frames in mixer.format/mixer.channels format.
```


Example 2
---------
This example shows how you can do submixing.

```c
ma_uint64 frameCountIn;
ma_uint64 frameCountOut = desiredFrameCount;    // <-- On input specifies what you want, on output receives what you actually got.
ma_mixer_begin(&masterMixer, NULL, &frameCountOut, &frameCountIn);
{
    ma_uint64 submixFrameCountIn;

    // SFX submix.
    ma_mixer_begin(&sfxMixer, &masterMixer, &submixFrameCountIn, NULL);     // Output frame count not required for submixing.
    {
        ma_mixer_mix_data_source(&sfxMixer, &sfxDecoder1, 0, submixFrameCountIn, isSFXLooping1);
        ma_mixer_mix_data_source(&sfxMixer, &sfxDecoder2, 0, submixFrameCountIn, isSFXLooping2);
    }
    ma_mixer_end(&sfxMixer, &masterMixer, NULL, 0);

    // Voice submix.
    ma_mixer_begin(&voiceMixer, &masterMixer, &submixFrameCountIn, NULL);
    {
        ma_mixer_mix_data_source(&voiceMixer, &voiceDecoder1, 0, submixFrameCountIn, isVoiceLooping1);
    }
    ma_mixer_end(&voiceMixer, &masterMixer, NULL, 0);
    
    // Music submix.
    ma_mixer_begin(&musicMixer, &masterMixer, &submixFrameCountIn, NULL);
    {
        ma_mixer_mix_data_source(&musicMixer, &musicDecoder1, 0, submixFrameCountIn, isMusicLooping1);
    }
    ma_mixer_end(&musicMixer, &masterMixer, NULL, 0);
}
ma_mixer_end(&masterMixer, NULL, pFramesOut, 0); // <-- pFramesOut must be large enough to receive frameCountOut frames in mixer.format/mixer.channels format.
```


See Also
--------
ma_mixer_end()
ma_mixer_set_effect()
ma_mixer_get_effect()
*/
MA_API ma_result ma_mixer_begin(ma_mixer* pMixer, ma_mixer* pParentMixer, ma_uint64* pFrameCountOut, ma_uint64* pFrameCountIn);

/*
Applies volume, performs clipping, applies the effect (if any) and outputs the final mix to the specified output buffer or mixed with another mixer.


Parameters
----------
pMixer (in)
    A pointer to the mixer.

pParentMixer (in, optional)
    A pointer to the parent mixer. If this is non-NULL, the output of `pMixer` will be mixed with `pParentMixer`. It is an error for `pParentMixer` and
    `pFramesOut` to both be non-NULL. If this is non-NULL, it must have also been specified as the parent mixer in the prior call to `ma_mixer_begin()`.

pFramesOut (in, optional)
    A pointer to the buffer that will receive the final mixed output. The output buffer must be in the format specified by the mixer's configuration that was
    used to initialized it. The required size in frames is defined by the output frame count returned by `ma_mixer_begin()`. It is an error for `pFramesOut`
    and `pParentMixer` to both be non-NULL.

outputOffsetInFrames (in)
    The offset in frames to start writing the output data to the destination buffer.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Remarks
-------
It is an error both both `pParentMixer` and `pFramesOut` to both be NULL or non-NULL. You must specify one or the other.

When outputting to a parent mixer (`pParentMixer` is non-NULL), the output is mixed with the parent mixer. Otherwise (`pFramesOut` is non-NULL), the output
will overwrite anything already in the output buffer.

When calculating the final output, the volume will be applied before clipping, which is done before applying the effect (if any).

See documentation for `ma_mixer_begin()` for an example on how to use `ma_mixer_end()`.


See Also
--------
ma_mixer_begin()
ma_mixer_set_volume()
ma_mixer_get_volume()
ma_mixer_set_effect()
ma_mixer_get_effect()
*/
MA_API ma_result ma_mixer_end(ma_mixer* pMixer, ma_mixer* pParentMixer, void* pFramesOut, ma_uint64 outputOffsetInFrames);


/*
Mixes audio data from a buffer containing raw PCM data.


Parameters
----------
pMixer (in)
    A pointer to the mixer.

pFramesIn (in)
    A pointer to the buffer containing the raw PCM data to mix with the mixer. The data contained within this buffer is assumed to be of the same format as the
    mixer, which was specified when the mixer was initialized. Use `ma_mixer_mix_pcm_frames_ex()` to mix data of a different format.

frameCountIn (in)
    The number of frames to mix. This cannot exceed the number of input frames returned by `ma_mixer_begin()`. If it does, an error will be returned. If it is
    less, silence will be mixed to make up the excess.

formatIn (in)
    The sample format of the input data.

channelsIn (in)
    The channel count of the input data.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Remarks
-------
Each call to this function will start mixing from the start of the internal accumulation buffer.

This will automatically convert the data to the mixer's native format. The sample format will be converted without dithering. Channels will be converted based
on the default channel map.


See Also
--------
ma_mixer_mix_pcm_frames()
ma_mixer_begin()
ma_mixer_end()
*/
MA_API ma_result ma_mixer_mix_pcm_frames(ma_mixer* pMixer, const void* pFramesIn, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, float volume, ma_format formatIn, ma_uint32 channelsIn);

/*
Mixes audio data from a data source


Parameters
----------
pMixer (in)
    A pointer to the mixer.

pDataSource (in)
    A pointer to the data source to read input data from.

frameCountIn (in)
    The number of frames to mix. This cannot exceed the number of input frames returned by `ma_mixer_begin()`. If it does, an error will be returned. If it is
    less, silence will be mixed to make up the excess.

pFrameCountOut (out)
    Receives the number of frames that were processed from the data source.

formatIn (in)
    The sample format of the input data.

channelsIn (in)
    The channel count of the input data.


Return Value
------------
MA_SUCCESS if successful; any other error code otherwise.


Remarks
-------
Each call to this function will start mixing from the start of the internal accumulation buffer.

This will automatically convert the data to the mixer's native format. The sample format will be converted without dithering. Channels will be converted based
on the default channel map.


See Also
--------
ma_mixer_begin()
ma_mixer_end()
*/
MA_API ma_result ma_mixer_mix_data_source(ma_mixer* pMixer, ma_data_source* pDataSource, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, ma_uint64* pFrameCountOut, float volume, ma_effect* pEffect, ma_bool32 loop);
MA_API ma_result ma_mixer_mix_rb(ma_mixer* pMixer, ma_rb* pRB, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, ma_uint64* pFrameCountOut, float volume, ma_effect* pEffect, ma_format formatIn, ma_uint32 channelsIn); /* Caller is the consumer. */
MA_API ma_result ma_mixer_mix_pcm_rb(ma_mixer* pMixer, ma_pcm_rb* pRB, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, ma_uint64* pFrameCountOut, float volume, ma_effect* pEffect);                                   /* Caller is the consumer. */

MA_API ma_result ma_mixer_set_volume(ma_mixer* pMixer, float volume);
MA_API ma_result ma_mixer_get_volume(ma_mixer* pMixer, float* pVolume);
MA_API ma_result ma_mixer_set_gain_db(ma_mixer* pMixer, float gainDB);
MA_API ma_result ma_mixer_get_gain_db(ma_mixer* pMixer, float* pGainDB);
MA_API ma_result ma_mixer_set_effect(ma_mixer* pMixer, ma_effect* pEffect);
MA_API ma_result ma_mixer_get_effect(ma_mixer* pMixer, ma_effect** ppEffect);
MA_API ma_result ma_mixer_get_output_data_format(ma_mixer* pMixer, ma_format* pFormat, ma_uint32* pChannels);
MA_API ma_result ma_mixer_get_input_data_format(ma_mixer* pMixer, ma_format* pFormat, ma_uint32* pChannels);



/*
Resource Manager Data Source Flags
==================================
The flags below are used for controlling how the resource manager should handle the loading and caching of data sources.
*/
#define MA_DATA_SOURCE_FLAG_STREAM  0x00000001  /* When set, does not load the entire data source in memory. Disk I/O will happen on job threads. */
#define MA_DATA_SOURCE_FLAG_DECODE  0x00000002  /* Decode data before storing in memory. When set, decoding is done at the resource manager level rather than the mixing thread. Results in faster mixing, but higher memory usage. */
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


typedef struct ma_resource_manager                  ma_resource_manager;
typedef struct ma_resource_manager_data_buffer_node ma_resource_manager_data_buffer_node;
typedef struct ma_resource_manager_data_buffer      ma_resource_manager_data_buffer;
typedef struct ma_resource_manager_data_stream      ma_resource_manager_data_stream;
typedef struct ma_resource_manager_data_source      ma_resource_manager_data_source;



#ifndef MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY
#define MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY  1024
#endif

#define MA_JOB_QUIT                 0x00000000
#define MA_JOB_LOAD_DATA_BUFFER     0x00000001
#define MA_JOB_FREE_DATA_BUFFER     0x00000002
#define MA_JOB_PAGE_DATA_BUFFER     0x00000003
#define MA_JOB_LOAD_DATA_STREAM     0x00000004
#define MA_JOB_FREE_DATA_STREAM     0x00000005
#define MA_JOB_PAGE_DATA_STREAM     0x00000006
#define MA_JOB_SEEK_DATA_STREAM     0x00000007
#define MA_JOB_CUSTOM               0x000000FF  /* Number your custom job codes as (MA_JOB_CUSTOM + 0), (MA_JOB_CUSTOM + 1), etc. */


/*
The idea of the slot allocator is for it to be used in conjunction with a fixed sized buffer. You use the slot allocator to allocator an index that can be used
as the insertion point for an object.

Slots are reference counted to help mitigate the ABA problem in the lock-free queue we use for tracking jobs.

The slot index is stored in the low 32 bits. The reference counter is stored in the high 32 bits:

    +-----------------+-----------------+
    | 32 Bits         | 32 Bits         |
    +-----------------+-----------------+
    | Reference Count | Slot Index      |
    +-----------------+-----------------+
*/
typedef struct
{
    struct
    {
        volatile ma_uint32 bitfield;                            /* Marking as volatile because the allocation and freeing routines need to make copies of this which must never be optimized away by the compiler. */
    } groups[MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY/32];
    ma_uint32 slots[MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY];    /* 32 bits for reference counting for ABA mitigation. */
    ma_uint32 count;    /* Allocation count. */
} ma_slot_allocator;

MA_API ma_result ma_slot_allocator_init(ma_slot_allocator* pAllocator);
MA_API ma_result ma_slot_allocator_alloc(ma_slot_allocator* pAllocator, ma_uint64* pSlot);
MA_API ma_result ma_slot_allocator_free(ma_slot_allocator* pAllocator, ma_uint64 slot);


/* Notification codes for ma_async_notification. Used to allow some granularity for notification callbacks. */
#define MA_NOTIFICATION_COMPLETE    0   /* Operation has fully completed. */
#define MA_NOTIFICATION_INIT        1   /* Object has been initialized, but operation not fully completed yet. */
#define MA_NOTIFICATION_FAILED      2   /* Failed to initialize. */


/*
Notification callback for asynchronous operations.
*/
typedef void ma_async_notification;

typedef struct
{
    void (* onSignal)(ma_async_notification* pNotification, int code);
} ma_async_notification_callbacks;

MA_API ma_result ma_async_notification_signal(ma_async_notification* pNotification, int code);


/*
Event Notification

This notification signals an event internally on the MA_NOTIFICATION_COMPLETE and MA_NOTIFICATION_FAILED codes. All other codes are ignored.
*/
typedef struct
{
    ma_async_notification_callbacks cb;
    ma_event e;
} ma_async_notification_event;

MA_API ma_result ma_async_notification_event_init(ma_async_notification_event* pNotificationEvent);
MA_API ma_result ma_async_notification_event_uninit(ma_async_notification_event* pNotificationEvent);
MA_API ma_result ma_async_notification_event_wait(ma_async_notification_event* pNotificationEvent);
MA_API ma_result ma_async_notification_event_signal(ma_async_notification_event* pNotificationEvent);


typedef struct
{
    union
    {
        struct
        {
            ma_uint16 code;
            ma_uint16 slot;
            ma_uint32 refcount;
        };
        ma_uint64 allocation;
    } toc;  /* 8 bytes. We encode the job code into the slot allocation data to save space. */
    ma_uint64 next;     /* refcount + slot for the next item. Does not include the job code. */
    ma_uint32 order;    /* Execution order. Used to create a data dependency and ensure a job is executed in order. Usage is contextual depending on the job type. */

    union
    {
        /* Resource Managemer Jobs */
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            char* pFilePath;
            ma_async_notification* pNotification;   /* Signalled when the data buffer has been fully decoded. */
        } loadDataBuffer;
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            ma_async_notification* pNotification;
        } freeDataBuffer;
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            ma_decoder* pDecoder;
            ma_async_notification* pCompletedNotification;  /* Signalled when the data buffer has been fully decoded. */
            void* pData;
            size_t dataSizeInBytes;
            ma_uint64 decodedFrameCount;
            ma_bool32 isUnknownLength;              /* When set to true does not update the running frame count of the data buffer nor the data pointer until the last page has been decoded. */
        } pageDataBuffer;

        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            char* pFilePath;                        /* Allocated when the job is posted, freed by the job thread after loading. */
            ma_async_notification* pNotification;   /* Signalled after the first two pages have been decoded and frames can be read from the stream. */
        } loadDataStream;
        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            ma_async_notification* pNotification;
        } freeDataStream;
        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            ma_uint32 pageIndex;                    /* The index of the page to decode into. */
        } pageDataStream;
        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            ma_uint64 frameIndex;
        } seekDataStream;

        /* Others. */
        struct
        {
            ma_uintptr data0;
            ma_uintptr data1;
        } custom;
    };
} ma_job;

MA_API ma_job ma_job_init(ma_uint16 code);


#define MA_JOB_QUEUE_FLAG_NON_BLOCKING  0x00000001  /* When set, ma_job_queue_next() will not wait and no semaphore will be signaled in ma_job_queue_post(). ma_job_queue_next() will return MA_NO_DATA_AVAILABLE if nothing is available. */

typedef struct
{
    ma_uint32 flags;    /* Flags passed in at initialization time. */
    ma_uint64 head;     /* The first item in the list. Required for removing from the top of the list. */
    ma_uint64 tail;     /* The last item in the list. Required for appending to the end of the list. */
    ma_semaphore sem;   /* Only used when MA_JOB_QUEUE_ASYNC is unset. */
    ma_slot_allocator allocator;
    ma_job jobs[MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY];
} ma_job_queue;

MA_API ma_result ma_job_queue_init(ma_uint32 flags, ma_job_queue* pQueue);
MA_API ma_result ma_job_queue_uninit(ma_job_queue* pQueue);
MA_API ma_result ma_job_queue_post(ma_job_queue* pQueue, const ma_job* pJob);
MA_API ma_result ma_job_queue_next(ma_job_queue* pQueue, ma_job* pJob); /* Returns MA_CANCELLED if the next job is a quit job. */


/* Maximum job thread count will be restricted to this, but this may be removed later and replaced with a heap allocation thereby removing any limitation. */
#ifndef MA_RESOURCE_MANAGER_MAX_JOB_THREAD_COUNT
#define MA_RESOURCE_MANAGER_MAX_JOB_THREAD_COUNT    64
#endif

#define MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING       0x00000001  /* Indicates ma_resource_manager_next_job() should not block. Only valid with MA_RESOURCE_MANAGER_NO_JOB_THREAD. */   

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

struct ma_resource_manager_data_buffer_node
{
    ma_uint32 hashedName32;                         /* The hashed name. This is the key. */
    ma_uint32 refCount;
    volatile ma_result result;                      /* Result from asynchronous loading. When loading set to MA_BUSY. When fully loaded set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. */
    ma_uint32 executionCounter;                     /* For allocating execution orders for jobs. */
    ma_uint32 executionPointer;                     /* For managing the order of execution for asynchronous jobs relating to this object. Incremented as jobs complete processing. */
    ma_bool32 isDataOwnedByResourceManager;         /* Set to true when the underlying data buffer was allocated the resource manager. Set to false if it is owned by the application (via ma_resource_manager_register_*()). */
    ma_resource_manager_memory_buffer data;
    ma_resource_manager_data_buffer_node* pParent;
    ma_resource_manager_data_buffer_node* pChildLo;
    ma_resource_manager_data_buffer_node* pChildHi;
};

struct ma_resource_manager_data_buffer
{
    ma_data_source_callbacks ds;                    /* Data source callbacks. A data buffer is a data source. */
    ma_resource_manager* pResourceManager;          /* A pointer to the resource manager that owns this buffer. */
    ma_uint32 flags;                                /* The flags that were passed used to initialize the buffer. */
    ma_resource_manager_data_buffer_node* pNode;    /* The data node. This is reference counted. */
    ma_uint64 cursorInPCMFrames;                    /* Only updated by the public API. Never written nor read from the job thread. */
    ma_uint64 lengthInPCMFrames;                    /* The total length of the sound in PCM frames. This is set at load time. */
    ma_bool32 seekToCursorOnNextRead;               /* On the next read we need to seek to the frame cursor. */
    volatile ma_bool32 isLooping;
    ma_resource_manager_data_buffer_connector connectorType;
    union
    {
        ma_decoder decoder;
        ma_audio_buffer buffer;
    } connector;
};

struct ma_resource_manager_data_stream
{
    ma_data_source_callbacks ds;            /* Data source callbacks. A data stream is a data source. */
    ma_resource_manager* pResourceManager;  /* A pointer to the resource manager that owns this data stream. */
    ma_uint32 flags;                        /* The flags that were passed used to initialize the stream. */
    ma_decoder decoder;                     /* Used for filling pages with data. This is only ever accessed by the job thread. The public API should never touch this. */
    ma_bool32 isDecoderInitialized;         /* Required for determining whether or not the decoder should be uninitialized in MA_JOB_FREE_DATA_STREAM. */
    ma_uint64 totalLengthInPCMFrames;       /* This is calculated when first loaded by the MA_JOB_LOAD_DATA_STREAM. */
    ma_uint32 relativeCursor;               /* The playback cursor, relative to the current page. Only ever accessed by the public API. Never accessed by the job thread. */
    ma_uint64 absoluteCursor;               /* The playback cursor, in absolute position starting from the start of the file. */
    ma_uint32 currentPageIndex;             /* Toggles between 0 and 1. Index 0 is the first half of pPageData. Index 1 is the second half. Only ever accessed by the public API. Never accessed by the job thread. */
    ma_uint32 executionCounter;             /* For allocating execution orders for jobs. */
    ma_uint32 executionPointer;             /* For managing the order of execution for asynchronous jobs relating to this object. Incremented as jobs complete processing. */

    /* Written by the public API, read by the job thread. */
    volatile ma_bool32 isLooping;           /* Whether or not the stream is looping. It's important to set the looping flag at the data stream level for smooth loop transitions. */

    /* Written by the job thread, read by the public API. */
    void* pPageData;                        /* Buffer containing the decoded data of each page. Allocated once at initialization time. */
    volatile ma_uint32 pageFrameCount[2];   /* The number of valid PCM frames in each page. Used to determine the last valid frame. */

    /* Written and read by both the public API and the job thread. */
    volatile ma_result result;              /* Result from asynchronous loading. When loading set to MA_BUSY. When initialized set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. If an error occurs when loading, set to an error code. */
    volatile ma_bool32 isDecoderAtEnd;      /* Whether or not the decoder has reached the end. */
    volatile ma_bool32 isPageValid[2];      /* Booleans to indicate whether or not a page is valid. Set to false by the public API, set to true by the job thread. Set to false as the pages are consumed, true when they are filled. */
    volatile ma_bool32 seekCounter;         /* When 0, no seeking is being performed. When > 0, a seek is being performed and reading should be delayed with MA_BUSY. */
};

struct ma_resource_manager_data_source
{
    union
    {
        ma_resource_manager_data_buffer buffer;
        ma_resource_manager_data_stream stream;
    };  /* Must be the first item because we need the first item to be the data source callbacks for the buffer or stream. */

    ma_uint32 flags;                /* The flags that were passed in to ma_resource_manager_data_source_init(). */
    ma_uint32 executionCounter;     /* For allocating execution orders for jobs. */
    ma_uint32 executionPointer;     /* For managing the order of execution for asynchronous jobs relating to this object. Incremented as jobs complete processing. */
};

typedef struct
{
    ma_allocation_callbacks allocationCallbacks;
    ma_format decodedFormat;        /* The decoded format to use. Set to ma_format_unknown (default) to use the file's native format. */
    ma_uint32 decodedChannels;      /* The decoded channel count to use. Set to 0 (default) to use the file's native channel count. */
    ma_uint32 decodedSampleRate;    /* the decoded sample rate to use. Set to 0 (default) to use the file's native sample rate. */
    ma_uint32 jobThreadCount;       /* Set to 0 if you want to self-manage your job threads. Defaults to 1. */
    ma_uint32 flags;
    ma_vfs* pVFS;                   /* Can be NULL in which case defaults will be used. */
} ma_resource_manager_config;

MA_API ma_resource_manager_config ma_resource_manager_config_init();

struct ma_resource_manager
{
    ma_resource_manager_config config;
    ma_resource_manager_data_buffer_node* pRootDataBufferNode;      /* The root buffer in the binary tree. */
    ma_mutex dataBufferLock;                                        /* For synchronizing access to the data buffer binary tree. */
    ma_thread jobThreads[MA_RESOURCE_MANAGER_MAX_JOB_THREAD_COUNT]; /* The thread for executing jobs. Will probably turn this into an array. */
    ma_job_queue jobQueue;                                          /* Lock-free multi-consumer, multi-producer job queue for managing jobs for asynchronous decoding and streaming. */
    ma_default_vfs defaultVFS;                                      /* Only used if a custom VFS is not specified. */
};

/* Init. */
MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager);
MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager);

/* Registration. */
MA_API ma_result ma_resource_manager_register_decoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate);  /* Does not copy. Increments the reference count if already exists and returns MA_SUCCESS. */
MA_API ma_result ma_resource_manager_register_encoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, size_t sizeInBytes);    /* Does not copy. Increments the reference count if already exists and returns MA_SUCCESS. */
MA_API ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName);

/* Data Buffers. */
MA_API ma_result ma_resource_manager_data_buffer_init(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags, ma_async_notification* pNotification, ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_uninit(ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_read_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_resource_manager_data_buffer_seek_to_pcm_frame(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64 frameIndex);
MA_API ma_result ma_resource_manager_data_buffer_map(ma_resource_manager_data_buffer* pDataBuffer, void** ppFramesOut, ma_uint64* pFrameCount);
MA_API ma_result ma_resource_manager_data_buffer_unmap(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64 frameCount);
MA_API ma_result ma_resource_manager_data_buffer_get_data_format(ma_resource_manager_data_buffer* pDataBuffer, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API ma_result ma_resource_manager_data_buffer_get_cursor_in_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pCursor);
MA_API ma_result ma_resource_manager_data_buffer_get_length_in_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pLength);
MA_API ma_result ma_resource_manager_data_buffer_result(const ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_set_looping(ma_resource_manager_data_buffer* pDataBuffer, ma_bool32 isLooping);
MA_API ma_result ma_resource_manager_data_buffer_get_looping(const ma_resource_manager_data_buffer* pDataBuffer, ma_bool32* pIsLooping);
MA_API ma_result ma_resource_manager_data_buffer_get_available_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pAvailableFrames);

/* Data Streams. */
MA_API ma_result ma_resource_manager_data_stream_init(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags, ma_async_notification* pNotification, ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_uninit(ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_read_pcm_frames(ma_resource_manager_data_stream* pDataStream, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_resource_manager_data_stream_seek_to_pcm_frame(ma_resource_manager_data_stream* pDataStream, ma_uint64 frameIndex);
MA_API ma_result ma_resource_manager_data_stream_map(ma_resource_manager_data_stream* pDataStream, void** ppFramesOut, ma_uint64* pFrameCount);
MA_API ma_result ma_resource_manager_data_stream_unmap(ma_resource_manager_data_stream* pDataStream, ma_uint64 frameCount);
MA_API ma_result ma_resource_manager_data_stream_get_data_format(ma_resource_manager_data_stream* pDataStream, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API ma_result ma_resource_manager_data_stream_get_cursor_in_pcm_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pCursor);
MA_API ma_result ma_resource_manager_data_stream_get_length_in_pcm_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pLength);
MA_API ma_result ma_resource_manager_data_stream_result(const ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_set_looping(ma_resource_manager_data_stream* pDataStream, ma_bool32 isLooping);
MA_API ma_result ma_resource_manager_data_stream_get_looping(const ma_resource_manager_data_stream* pDataStream, ma_bool32* pIsLooping);
MA_API ma_result ma_resource_manager_data_stream_get_available_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pAvailableFrames);

/* Data Sources. */
MA_API ma_result ma_resource_manager_data_source_init(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_async_notification* pNotification, ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_uninit(ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_read_pcm_frames(ma_resource_manager_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_resource_manager_data_source_seek_to_pcm_frame(ma_resource_manager_data_source* pDataSource, ma_uint64 frameIndex);
MA_API ma_result ma_resource_manager_data_source_map(ma_resource_manager_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount);
MA_API ma_result ma_resource_manager_data_source_unmap(ma_resource_manager_data_source* pDataSource, ma_uint64 frameCount);
MA_API ma_result ma_resource_manager_data_source_get_data_format(ma_resource_manager_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API ma_result ma_resource_manager_data_source_get_cursor_in_pcm_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pCursor);
MA_API ma_result ma_resource_manager_data_source_get_length_in_pcm_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pLength);
MA_API ma_result ma_resource_manager_data_source_result(const ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_set_looping(ma_resource_manager_data_source* pDataSource, ma_bool32 isLooping);
MA_API ma_result ma_resource_manager_data_source_get_looping(const ma_resource_manager_data_source* pDataSource, ma_bool32* pIsLooping);
MA_API ma_result ma_resource_manager_data_source_get_available_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pAvailableFrames);

/* Job management. */
MA_API ma_result ma_resource_manager_post_job(ma_resource_manager* pResourceManager, const ma_job* pJob);
MA_API ma_result ma_resource_manager_post_job_quit(ma_resource_manager* pResourceManager);  /* Helper for posting a quit job. */
MA_API ma_result ma_resource_manager_next_job(ma_resource_manager* pResourceManager, ma_job* pJob);
MA_API ma_result ma_resource_manager_process_job(ma_resource_manager* pResourceManager, ma_job* pJob);
MA_API ma_result ma_resource_manager_process_next_job(ma_resource_manager* pResourceManager);   /* Returns MA_CANCELLED if a MA_JOB_QUIT job is found. In non-blocking mode, returns MA_NO_DATA_AVAILABLE if no jobs are available. */


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


/* Fader. */
typedef struct
{
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
} ma_fader_config;

MA_API ma_fader_config ma_fader_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate);

typedef struct
{
    ma_effect_base effect;
    ma_fader_config config;
    float volumeBeg;            /* If volumeBeg and volumeEnd is equal to 1, no fading happens (ma_fader_process_pcm_frames() will run as a passthrough). */
    float volumeEnd;
    ma_uint64 lengthInFrames;   /* The total length of the fade. */
    ma_uint64 cursorInFrames;   /* The current time in frames. Incremented by ma_fader_process_pcm_frames(). */
} ma_fader;

MA_API ma_result ma_fader_init(const ma_fader_config* pConfig, ma_fader* pFader);
MA_API ma_result ma_fader_process_pcm_frames(ma_fader* pFader, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_result ma_fader_get_data_format(const ma_fader* pFader, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API ma_result ma_fader_set_fade(ma_fader* pFader, float volumeBeg, float volumeEnd, ma_uint64 lengthInFrames);
MA_API ma_result ma_fader_get_current_volume(ma_fader* pFader, float* pVolume);


/* All of the proprties supported by the engine are handled via an effect. */
typedef struct
{
    ma_effect_base baseEffect;
    ma_engine* pEngine;             /* For accessing global, per-engine data such as the listener position and environmental information. */
    ma_effect* pPreEffect;          /* The application-defined effect that will be applied before spationalization, etc. */
    ma_panner panner;
    ma_spatializer spatializer;
    ma_fader fader;
    float pitch;
    float oldPitch;                 /* For determining whether or not the resampler needs to be updated to reflect the new pitch. The resampler will be updated on the mixing thread. */
    ma_data_converter converter;    /* For pitch shift. May change this to ma_linear_resampler later. */
    ma_uint64 timeInFrames;         /* The running time in input frames. */
    ma_bool32 isSpatial;            /* Set the false by default. When set to false, will not have spatialisation applied. */
} ma_engine_effect;

struct ma_sound
{
    ma_engine* pEngine;                         /* A pointer to the object that owns this sound. */
    ma_data_source* pDataSource;
    ma_sound_group* pGroup;                     /* The group the sound is attached to. */
    volatile ma_sound* pPrevSoundInGroup;
    volatile ma_sound* pNextSoundInGroup;
    ma_engine_effect effect;                    /* The effect containing all of the information for spatialization, pitching, etc. */
    float volume;
    ma_uint64 seekTarget;                       /* The PCM frame index to seek to in the mixing thread. Set to (~(ma_uint64)0) to not perform any seeking. */
    ma_uint64 runningTimeInEngineFrames;        /* The amount of time the sound has been running in engine frames, including start delays. */
    ma_uint64 startDelayInEngineFrames;         /* In the engine's sample rate. */
    ma_uint64 stopDelayInEngineFrames;          /* In the engine's sample rate. */
    ma_uint64 stopDelayInEngineFramesRemaining; /* The number of frames relative to the engine's clock before the sound is stopped. */
    volatile ma_bool32 isPlaying;               /* False by default. Sounds need to be explicitly started with ma_sound_start() and stopped with ma_sound_stop(). */
    volatile ma_bool32 isMixing;
    volatile ma_bool32 isLooping;               /* False by default. */
    volatile ma_bool32 atEnd;
    ma_bool32 ownsDataSource;
    ma_bool32 _isInternal;                      /* A marker to indicate the sound is managed entirely by the engine. This will be set to true when the sound is created internally by ma_engine_play_sound(). */
    ma_resource_manager_data_source resourceManagerDataSource;
};

struct ma_sound_group
{
    ma_engine* pEngine;                         /* A pointer to the engine that owns this sound group. */
    ma_sound_group* pParent;
    ma_sound_group* pFirstChild;
    ma_sound_group* pPrevSibling;
    ma_sound_group* pNextSibling;
    volatile ma_sound* pFirstSoundInGroup;
    ma_engine_effect effect;                    /* The main effect for panning, etc. This is set on the mixer at initialisation time. */
    ma_mixer mixer;
    ma_mutex lock;                              /* Only used by ma_sound_init_*() and ma_sound_uninit(). Not used in the mixing thread. */
    ma_uint64 runningTimeInEngineFrames;        /* The amount of time the sound has been running in engine frames, including start delays. */
    ma_uint64 startDelayInEngineFrames;
    ma_uint64 stopDelayInEngineFrames;          /* In the engine's sample rate. */
    ma_uint64 stopDelayInEngineFramesRemaining; /* The number of frames relative to the engine's clock before the sound is stopped. */
    volatile ma_bool32 isPlaying;               /* True by default. Sound groups can be stopped with ma_sound_stop() and resumed with ma_sound_start(). Also affects children. */
};

struct ma_listener
{
    ma_vec3 position;
    ma_quat rotation;
};


typedef struct
{
    ma_resource_manager* pResourceManager;  /* Can be null in which case a resource manager will be created for you. */
    ma_context* pContext;
    ma_device* pDevice;                     /* If set, the caller is responsible for calling ma_engine_data_callback() in the device's data callback. */
    ma_format format;                       /* The format to use when mixing and spatializing. When set to 0 will use the native format of the device. */
    ma_uint32 channels;                     /* The number of channels to use when mixing and spatializing. When set to 0, will use the native channel count of the device. */
    ma_uint32 sampleRate;                   /* The sample rate. When set to 0 will use the native channel count of the device. */
    ma_uint32 periodSizeInFrames;           /* If set to something other than 0, updates will always be exactly this size. The underlying device may be a different size, but from the perspective of the mixer that won't matter.*/
    ma_uint32 periodSizeInMilliseconds;     /* Used if periodSizeInFrames is unset. */
    ma_device_id* pPlaybackDeviceID;        /* The ID of the playback device to use with the default listener. */
    ma_allocation_callbacks allocationCallbacks;
    ma_bool32 noAutoStart;                  /* When set to true, requires an explicit call to ma_engine_start(). This is false by default, meaning the engine will be started automatically in ma_engine_init(). */
    ma_vfs* pResourceManagerVFS;            /* A pointer to a pre-allocated VFS object to use with the resource manager. This is ignored if pResourceManager is not NULL. */
} ma_engine_config;

MA_API ma_engine_config ma_engine_config_init_default(void);


struct ma_engine
{
    ma_resource_manager* pResourceManager;
    ma_device* pDevice;                     /* Optionally set via the config, otherwise allocated by the engine in ma_engine_init(). */
    ma_pcm_rb fixedRB;                      /* The intermediary ring buffer for helping with fixed sized updates. */
    ma_listener listener;
    ma_sound_group masterSoundGroup;        /* Sounds are associated with this group by default. */
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 periodSizeInFrames;
    ma_uint32 periodSizeInMilliseconds;
    ma_allocation_callbacks allocationCallbacks;
    ma_bool8 ownsResourceManager;
    ma_bool8 ownsDevice;
};

MA_API ma_result ma_engine_init(const ma_engine_config* pConfig, ma_engine* pEngine);
MA_API void ma_engine_uninit(ma_engine* pEngine);
MA_API void ma_engine_data_callback(ma_engine* pEngine, void* pOutput, const void* pInput, ma_uint32 frameCount);

MA_API ma_result ma_engine_start(ma_engine* pEngine);
MA_API ma_result ma_engine_stop(ma_engine* pEngine);
MA_API ma_result ma_engine_set_volume(ma_engine* pEngine, float volume);
MA_API ma_result ma_engine_set_gain_db(ma_engine* pEngine, float gainDB);

MA_API ma_sound_group* ma_engine_get_master_sound_group(ma_engine* pEngine);

MA_API ma_result ma_engine_listener_set_position(ma_engine* pEngine, ma_vec3 position);
MA_API ma_result ma_engine_listener_set_rotation(ma_engine* pEngine, ma_quat rotation);

MA_API ma_result ma_engine_play_sound(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup);   /* Fire and forget. */


#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_sound_init_from_file(ma_engine* pEngine, const char* pFilePath, ma_uint32 flags, ma_async_notification* pNotification, ma_sound_group* pGroup, ma_sound* pSound);
#endif
MA_API ma_result ma_sound_init_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound);
MA_API void ma_sound_uninit(ma_sound* pSound);
MA_API ma_result ma_sound_start(ma_sound* pSound);
MA_API ma_result ma_sound_stop(ma_sound* pSound);
MA_API ma_result ma_sound_set_volume(ma_sound* pSound, float volume);
MA_API ma_result ma_sound_set_gain_db(ma_sound* pSound, float gainDB);
MA_API ma_result ma_sound_set_effect(ma_sound* pSound, ma_effect* pEffect);
MA_API ma_result ma_sound_set_pan(ma_sound* pSound, float pan);
MA_API ma_result ma_sound_set_pan_mode(ma_sound* pSound, ma_pan_mode pan_mode);
MA_API ma_result ma_sound_set_pitch(ma_sound* pSound, float pitch);
MA_API ma_result ma_sound_set_position(ma_sound* pSound, ma_vec3 position);
MA_API ma_result ma_sound_set_rotation(ma_sound* pSound, ma_quat rotation);
MA_API ma_result ma_sound_set_looping(ma_sound* pSound, ma_bool32 isLooping);
MA_API ma_bool32 ma_sound_is_looping(const ma_sound* pSound);
MA_API ma_result ma_sound_set_fade_in_frames(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames);
MA_API ma_result ma_sound_set_fade_in_milliseconds(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds);
MA_API ma_result ma_sound_get_current_fade_volume(ma_sound* pSound, float* pVolume);
MA_API ma_result ma_sound_set_start_delay(ma_sound* pSound, ma_uint64 delayInMilliseconds);
MA_API ma_result ma_sound_set_stop_delay(ma_sound* pSound, ma_uint64 delayInMilliseconds);
MA_API ma_bool32 ma_sound_is_playing(const ma_sound* pSound);
MA_API ma_bool32 ma_sound_at_end(const ma_sound* pSound);
MA_API ma_result ma_sound_get_time_in_frames(const ma_sound* pSound, ma_uint64* pTimeInFrames);
MA_API ma_result ma_sound_seek_to_pcm_frame(ma_sound* pSound, ma_uint64 frameIndex); /* Just a wrapper around ma_data_source_seek_to_pcm_frame(). */
MA_API ma_result ma_sound_get_data_format(ma_sound* pSound, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API ma_result ma_sound_get_cursor_in_pcm_frames(ma_sound* pSound, ma_uint64* pCursor);
MA_API ma_result ma_sound_get_length_in_pcm_frames(ma_sound* pSound, ma_uint64* pLength);

MA_API ma_result ma_sound_group_init(ma_engine* pEngine, ma_sound_group* pParentGroup, ma_sound_group* pGroup);  /* Parent must be set at initialization time and cannot be changed. Not thread-safe. */
MA_API void ma_sound_group_uninit(ma_sound_group* pGroup);   /* Not thread-safe. */
MA_API ma_result ma_sound_group_start(ma_sound_group* pGroup);
MA_API ma_result ma_sound_group_stop(ma_sound_group* pGroup);
MA_API ma_result ma_sound_group_set_volume(ma_sound_group* pGroup, float volume);
MA_API ma_result ma_sound_group_set_gain_db(ma_sound_group* pGroup, float gainDB);
MA_API ma_result ma_sound_group_set_effect(ma_sound_group* pGroup, ma_effect* pEffect);
MA_API ma_result ma_sound_group_set_pan(ma_sound_group* pGroup, float pan);
MA_API ma_result ma_sound_group_set_pitch(ma_sound_group* pGroup, float pitch);
MA_API ma_result ma_sound_group_set_fade_in_frames(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames);
MA_API ma_result ma_sound_group_set_fade_in_milliseconds(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds);
MA_API ma_result ma_sound_group_get_current_fade_volume(ma_sound_group* pGroup, float* pVolume);
MA_API ma_result ma_sound_group_set_start_delay(ma_sound_group* pGroup, ma_uint64 delayInMilliseconds);
MA_API ma_result ma_sound_group_set_stop_delay(ma_sound_group* pGroup, ma_uint64 delayInMilliseconds);
MA_API ma_bool32 ma_sound_group_is_playing(const ma_sound_group* pGroup);
MA_API ma_result ma_sound_group_get_time_in_frames(const ma_sound_group* pGroup, ma_uint64* pTimeInFrames);

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



static void ma_convert_pcm_frames_format_and_channels(void* pDst, ma_format formatOut, ma_uint32 channelsOut, const void* pSrc, ma_format formatIn, ma_uint32 channelsIn, ma_uint64 frameCount, ma_dither_mode ditherMode)
{
    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    if (channelsOut == channelsIn) {
        /* Only format conversion required. */
        if (formatOut == formatIn) {
            /* No data conversion required at all - just copy. */
            if (pDst == pSrc) {
                /* No-op. */
            } else {
                ma_copy_pcm_frames(pDst, pSrc, frameCount, formatOut, channelsOut);
            }
        } else {
            /* Simple format conversion. */
            ma_convert_pcm_frames_format(pDst, formatOut, pSrc, formatIn, frameCount, channelsOut, ditherMode);
        }
    } else {
        /* Getting here means we require a channel converter. We do channel conversion in the input format, and then format convert as a post process step if required. */
        ma_result result;
        ma_channel_converter_config channelConverterConfig;
        ma_channel_converter channelConverter;

        channelConverterConfig = ma_channel_converter_config_init(formatIn, channelsIn, NULL, channelsOut, NULL, ma_channel_mix_mode_default);
        result = ma_channel_converter_init(&channelConverterConfig, &channelConverter);
        if (result != MA_SUCCESS) {
            return; /* Failed to initialize channel converter for some reason. Should never fail. */
        }

        /* If we don't require any format conversion we can output straight into the output buffer. Otherwise we need to use an intermediary. */
        if (formatOut == formatIn) {
            /* No format conversion required. Output straight to the output buffer. */
            ma_channel_converter_process_pcm_frames(&channelConverter, pDst, pSrc, frameCount);
        } else {
            /* Format conversion required. We need to use an intermediary buffer. */
            ma_uint8  buffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE]; /* formatIn, channelsOut */
            ma_uint32 bufferCap = sizeof(buffer) / ma_get_bytes_per_frame(formatIn, channelsOut);
            ma_uint64 totalFramesProcessed = 0;
            
            while (totalFramesProcessed < frameCount) {
                ma_uint64 framesToProcess = frameCount - totalFramesProcessed;
                if (framesToProcess > bufferCap) {
                    framesToProcess = bufferCap;
                }

                result = ma_channel_converter_process_pcm_frames(&channelConverter, buffer, ma_offset_ptr(pSrc, totalFramesProcessed * ma_get_bytes_per_frame(formatIn, channelsIn)), framesToProcess);
                if (result != MA_SUCCESS) {
                    break;
                }

                /* Channel conversion is done, now format conversion straight into the output buffer. */
                ma_convert_pcm_frames_format(ma_offset_ptr(pDst, totalFramesProcessed * ma_get_bytes_per_frame(formatOut, channelsOut)), formatOut, buffer, formatIn, framesToProcess, channelsOut, ditherMode);

                totalFramesProcessed += framesToProcess;
            }
        }
    }
}


MA_API ma_result ma_effect_process_pcm_frames(ma_effect* pEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    return ma_effect_process_pcm_frames_ex(pEffect, 1, &pFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
}

MA_API ma_result ma_effect_process_pcm_frames_ex(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_effect_base* pBase = (ma_effect_base*)pEffect;

    if (pEffect == NULL || pBase->onProcessPCMFrames == NULL) {
        return MA_INVALID_ARGS;
    }

    return pBase->onProcessPCMFrames(pEffect, inputStreamCount, ppFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
}

MA_API ma_result ma_effect_process_pcm_frames_with_conversion(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut, ma_format formatIn, ma_uint32 channelsIn, ma_format formatOut, ma_uint32 channelsOut)
{
    ma_result result;
    ma_format effectFormatIn;
    ma_uint32 effectChannelsIn;
    ma_format effectFormatOut;
    ma_uint32 effectChannelsOut;

    if (inputStreamCount < MA_EFFECT_MIN_INPUT_STREAM_COUNT || inputStreamCount > MA_EFFECT_MAX_INPUT_STREAM_COUNT) {
        return MA_INVALID_ARGS;
    }

    /* First thing to retrieve the effect's input and output format to determine if conversion is necessary. */
    result = ma_effect_get_input_data_format(pEffect, &effectFormatIn, &effectChannelsIn, NULL);
    if (result != MA_SUCCESS) {
        return result;
    }

    result = ma_effect_get_output_data_format(pEffect, &effectFormatOut, &effectChannelsOut, NULL);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (effectFormatIn == formatIn && effectChannelsIn == channelsIn && effectFormatOut == formatOut && effectChannelsOut == channelsOut) {
        /* Fast path. No need for any data conversion. */
        return ma_effect_process_pcm_frames_ex(pEffect, inputStreamCount, ppFramesIn, pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        /* Slow path. Getting here means we need to do pre- and/or post-data conversion. */
        ma_uint8  effectInBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
        ma_uint32 effectInBufferCap = sizeof(effectInBuffer) / ma_get_bytes_per_frame(effectFormatIn, effectChannelsIn) / inputStreamCount;
        ma_uint8  effectOutBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
        ma_uint32 effectOutBufferCap = sizeof(effectOutBuffer) / ma_get_bytes_per_frame(effectFormatOut, effectChannelsOut);
        ma_uint64 totalFramesProcessedIn  = 0;
        ma_uint64 totalFramesProcessedOut = 0;
        ma_uint64 frameCountIn  = *pFrameCountIn;
        ma_uint64 frameCountOut = *pFrameCountOut;
        
        while (totalFramesProcessedIn < frameCountIn && totalFramesProcessedOut < frameCountOut) {
            ma_uint32 iInputStream;
            ma_uint64 framesToProcessThisIterationIn;
            ma_uint64 framesToProcessThisIterationOut;
            const void* ppRunningFramesIn[MA_EFFECT_MAX_INPUT_STREAM_COUNT];
            /* */ void* pRunningFramesOut = ma_offset_ptr(pFramesOut, totalFramesProcessedOut * ma_get_bytes_per_frame(formatOut, channelsOut));

            for (iInputStream = 0; iInputStream < inputStreamCount; iInputStream += 1) {
                ppRunningFramesIn[iInputStream] = ma_offset_ptr(ppFramesIn[iInputStream], totalFramesProcessedIn * ma_get_bytes_per_frame(formatIn, channelsIn));
            }

            framesToProcessThisIterationOut = frameCountOut - totalFramesProcessedOut;
            if (framesToProcessThisIterationOut > effectOutBufferCap) {
                framesToProcessThisIterationOut = effectOutBufferCap;
            }

            framesToProcessThisIterationIn = ma_effect_get_required_input_frame_count(pEffect, framesToProcessThisIterationOut);
            if (framesToProcessThisIterationIn > (frameCountIn - totalFramesProcessedIn)) {
                framesToProcessThisIterationIn = (frameCountIn - totalFramesProcessedIn);
            }
            if (framesToProcessThisIterationIn > effectInBufferCap) {
                framesToProcessThisIterationIn = effectInBufferCap;
            }

            /* At this point our input data has been converted to the effect's input format, so now we need to run the effect, making sure we output to the intermediary buffer. */
            if (effectFormatIn == formatIn && effectChannelsIn == channelsIn) {
                /* Fast path. No input conversion required. */
                if (effectFormatOut == formatOut && effectChannelsOut == channelsOut) {
                    /* Fast path. Neither input nor output data conversion required. */
                    ma_effect_process_pcm_frames_ex(pEffect, inputStreamCount, ppRunningFramesIn, &framesToProcessThisIterationIn, pRunningFramesOut, &framesToProcessThisIterationOut);
                } else {
                    /* Slow path. Output conversion required. */
                    ma_effect_process_pcm_frames_ex(pEffect, inputStreamCount, ppRunningFramesIn, &framesToProcessThisIterationIn, effectOutBuffer, &framesToProcessThisIterationOut);
                    ma_convert_pcm_frames_format_and_channels(pRunningFramesOut, formatOut, channelsOut, effectOutBuffer, effectFormatOut, effectChannelsOut, framesToProcessThisIterationOut, ma_dither_mode_none);
                }
            } else {
                /* Slow path. Input conversion required. */
                void* ppEffectInBuffer[MA_EFFECT_MAX_INPUT_STREAM_COUNT];

                for (iInputStream = 0; iInputStream < inputStreamCount; iInputStream += 1) {
                    ppEffectInBuffer[iInputStream] = ma_offset_ptr(effectInBuffer, effectInBufferCap * iInputStream);
                    ma_convert_pcm_frames_format_and_channels(ppEffectInBuffer[iInputStream], effectFormatIn, effectChannelsIn, ppRunningFramesIn[iInputStream], formatIn, channelsIn, framesToProcessThisIterationIn, ma_dither_mode_none);
                }
                
                if (effectFormatOut == formatOut && effectChannelsOut == channelsOut) {
                    /* Fast path. No output format conversion required. */
                    ma_effect_process_pcm_frames_ex(pEffect, inputStreamCount, (const void**)&ppEffectInBuffer[0], &framesToProcessThisIterationIn, pRunningFramesOut, &framesToProcessThisIterationOut);
                } else {
                    /* Slow path. Output format conversion required. */
                    ma_effect_process_pcm_frames_ex(pEffect, inputStreamCount, (const void**)&ppEffectInBuffer[0], &framesToProcessThisIterationIn, effectOutBuffer, &framesToProcessThisIterationOut);
                    ma_convert_pcm_frames_format_and_channels(pRunningFramesOut, formatOut, channelsOut, effectOutBuffer, effectFormatOut, effectChannelsOut, framesToProcessThisIterationOut, ma_dither_mode_none);
                }
            }

            totalFramesProcessedIn  += framesToProcessThisIterationIn;
            totalFramesProcessedOut += framesToProcessThisIterationOut;
        }
    }

    return MA_SUCCESS;
}

static ma_uint64 ma_effect_get_required_input_frame_count_local(ma_effect* pEffect, ma_uint64 outputFrameCount)
{
    ma_effect_base* pBase = (ma_effect_base*)pEffect;

    MA_ASSERT(pEffect != NULL);

    if (pBase->onGetRequiredInputFrameCount) {
        return pBase->onGetRequiredInputFrameCount(pEffect, outputFrameCount);
    } else {
        /* If there is no callback, assume a 1:1 mapping. */
        return outputFrameCount;
    }
}

MA_API ma_uint64 ma_effect_get_required_input_frame_count(ma_effect* pEffect, ma_uint64 outputFrameCount)
{
    if (pEffect == NULL) {
        return 0;
    }

    return ma_effect_get_required_input_frame_count_local(pEffect, outputFrameCount);
}

static ma_uint64 ma_effect_get_expected_output_frame_count_local(ma_effect* pEffect, ma_uint64 inputFrameCount)
{
    ma_effect_base* pBase = (ma_effect_base*)pEffect;

    MA_ASSERT(pEffect != NULL);

    if (pBase->onGetExpectedOutputFrameCount) {
        return pBase->onGetExpectedOutputFrameCount(pEffect, inputFrameCount);
    } else {
        /* If there is no callback, assume a 1:1 mapping. */
        return inputFrameCount;
    }
}

MA_API ma_uint64 ma_effect_get_expected_output_frame_count(ma_effect* pEffect, ma_uint64 inputFrameCount)
{
    if (pEffect == NULL) {
        return 0;
    }

    return ma_effect_get_expected_output_frame_count_local(pEffect, inputFrameCount);
}

MA_API ma_result ma_effect_get_output_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_effect_base* pBaseEffect = (ma_effect_base*)pEffect;
    ma_result result;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    if (pFormat != NULL) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels != NULL) {
        *pChannels = 0;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = 0;
    }

    if (pBaseEffect == NULL || pBaseEffect->onGetOutputDataFormat == NULL) {
        return MA_INVALID_ARGS;
    }

    result = pBaseEffect->onGetOutputDataFormat(pEffect, &format, &channels, &sampleRate);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (pFormat != NULL) {
        *pFormat = format;
    }
    if (pChannels != NULL) {
        *pChannels = channels;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = sampleRate;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_effect_get_input_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_effect_base* pBase = (ma_effect_base*)pEffect;
    ma_result result;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    if (pFormat != NULL) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels != NULL) {
        *pChannels = 0;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = 0;
    }

    result = pBase->onGetInputDataFormat(pEffect, &format, &channels, &sampleRate);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (pFormat != NULL) {
        *pFormat = format;
    }
    if (pChannels != NULL) {
        *pChannels = channels;
    }
    if (pSampleRate != NULL) {
        *pSampleRate = sampleRate;
    }

    return MA_SUCCESS;
}



MA_API size_t ma_get_accumulation_bytes_per_sample(ma_format format)
{
    size_t bytesPerSample[ma_format_count] = {
        0,                  /* ma_format_unknown */
        sizeof(ma_int16),   /* ma_format_u8  */
        sizeof(ma_int32),   /* ma_format_s16 */
        sizeof(ma_int64),   /* ma_format_s24 */
        sizeof(ma_int64),   /* ma_format_s32 */
        sizeof(float)       /* ma_format_f32 */
    };

    return bytesPerSample[format];
}

MA_API size_t ma_get_accumulation_bytes_per_frame(ma_format format, ma_uint32 channels)
{
    return ma_get_accumulation_bytes_per_sample(format) * channels;
}



static MA_INLINE ma_int16 ma_float_to_fixed_16(float x)
{
    return (ma_int16)(x * (1 << 8));
}



static MA_INLINE ma_int16 ma_apply_volume_unclipped_u8(ma_int16 x, ma_int16 volume)
{
    return (ma_int16)(((ma_int32)x * (ma_int32)volume) >> 8);
}

static MA_INLINE ma_int32 ma_apply_volume_unclipped_s16(ma_int32 x, ma_int16 volume)
{
    return (ma_int32)((x * volume) >> 8);
}

static MA_INLINE ma_int64 ma_apply_volume_unclipped_s24(ma_int64 x, ma_int16 volume)
{
    return (ma_int64)((x * volume) >> 8);
}

static MA_INLINE ma_int64 ma_apply_volume_unclipped_s32(ma_int64 x, ma_int16 volume)
{
    return (ma_int64)((x * volume) >> 8);
}

static MA_INLINE float ma_apply_volume_unclipped_f32(float x, float volume)
{
    return x * volume;
}


#if 0
static void ma_accumulate_and_clip_u8(ma_uint8* pDst, const ma_int16* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_u8(ma_pcm_sample_u8_to_s16_no_scale(pDst[iSample]) + pSrc[iSample]);
    }
}

static void ma_accumulate_and_clip_s16(ma_int16* pDst, const ma_int32* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s16(pDst[iSample] + pSrc[iSample]);
    }
}

static void ma_accumulate_and_clip_s24(ma_uint8* pDst, const ma_int64* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        ma_int64 s = ma_clip_s24(ma_pcm_sample_s24_to_s32_no_scale(&pDst[iSample*3]) + pSrc[iSample]);
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_accumulate_and_clip_s32(ma_int32* pDst, const ma_int64* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s32(pDst[iSample] + pSrc[iSample]);
    }
}

static void ma_accumulate_and_clip_f32(float* pDst, const float* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_f32(pDst[iSample] + pSrc[iSample]);
    }
}
#endif


static void ma_clip_samples_u8(ma_uint8* pDst, const ma_int16* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_u8(pSrc[iSample]);
    }
}

static void ma_clip_samples_s16(ma_int16* pDst, const ma_int32* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s16(pSrc[iSample]);
    }
}

static void ma_clip_samples_s24(ma_uint8* pDst, const ma_int64* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        ma_int64 s = ma_clip_s24(pSrc[iSample]);
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_clip_samples_s32(ma_int32* pDst, const ma_int64* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s32(pSrc[iSample]);
    }
}

static void ma_clip_samples_f32_ex(float* pDst, const float* pSrc, ma_uint64 count)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_f32(pSrc[iSample]);
    }
}



static void ma_volume_and_clip_samples_u8(ma_uint8* pDst, const ma_int16* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_u8(ma_apply_volume_unclipped_u8(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_clip_samples_s16(ma_int16* pDst, const ma_int32* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s16(ma_apply_volume_unclipped_s16(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_clip_samples_s24(ma_uint8* pDst, const ma_int64* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        ma_int64 s = ma_clip_s24(ma_apply_volume_unclipped_s24(pSrc[iSample], volumeFixed));
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_volume_and_clip_samples_s32(ma_int32* pDst, const ma_int64* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s32(ma_apply_volume_unclipped_s32(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_clip_samples_f32(float* pDst, const float* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    /* For the f32 case we need to make sure this supports in-place processing where the input and output buffers are the same. */

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_f32(ma_apply_volume_unclipped_f32(pSrc[iSample], volume));
    }
}

static void ma_clip_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels)
{
    ma_uint64 sampleCount;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    sampleCount = frameCount * channels;

    switch (format) {
        case ma_format_u8:  ma_clip_samples_u8( (ma_uint8*)pDst, (const ma_int16*)pSrc, sampleCount); break;
        case ma_format_s16: ma_clip_samples_s16((ma_int16*)pDst, (const ma_int32*)pSrc, sampleCount); break;
        case ma_format_s24: ma_clip_samples_s24((ma_uint8*)pDst, (const ma_int64*)pSrc, sampleCount); break;
        case ma_format_s32: ma_clip_samples_s32((ma_int32*)pDst, (const ma_int64*)pSrc, sampleCount); break;
        case ma_format_f32: ma_clip_samples_f32_ex((float*)pDst, (const    float*)pSrc, sampleCount); break;
        
        /* Do nothing if we don't know the format. We're including these here to silence a compiler warning about enums not being handled by the switch. */
        case ma_format_unknown:
        case ma_format_count:
            break;
    }
}

static void ma_volume_and_clip_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels, float volume)
{
    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    if (volume == 1) {
        ma_clip_pcm_frames(pDst, pSrc, frameCount, format, channels);   /* Optimized case for volume = 1. */
    } else if (volume == 0) {
        ma_silence_pcm_frames(pDst, frameCount, format, channels);      /* Optimized case for volume = 0. */
    } else {
        ma_uint64 sampleCount = frameCount * channels;

        switch (format) {
            case ma_format_u8:  ma_volume_and_clip_samples_u8( (ma_uint8*)pDst, (const ma_int16*)pSrc, sampleCount, volume); break;
            case ma_format_s16: ma_volume_and_clip_samples_s16((ma_int16*)pDst, (const ma_int32*)pSrc, sampleCount, volume); break;
            case ma_format_s24: ma_volume_and_clip_samples_s24((ma_uint8*)pDst, (const ma_int64*)pSrc, sampleCount, volume); break;
            case ma_format_s32: ma_volume_and_clip_samples_s32((ma_int32*)pDst, (const ma_int64*)pSrc, sampleCount, volume); break;
            case ma_format_f32: ma_volume_and_clip_samples_f32((   float*)pDst, (const    float*)pSrc, sampleCount, volume); break;
            
            /* Do nothing if we don't know the format. We're including these here to silence a compiler warning about enums not being handled by the switch. */
            case ma_format_unknown:
            case ma_format_count:
                break;
        }
    }
}


/* Not used at the moment, but leaving it here in case I want to use it again later. */
#if 0
static void ma_clipped_accumulate_u8(ma_uint8* pDst, const ma_uint8* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = ma_clip_u8(ma_pcm_sample_u8_to_s16_no_scale(pDst[iSample]) + ma_pcm_sample_u8_to_s16_no_scale(pSrc[iSample]));
    }
}

static void ma_clipped_accumulate_s16(ma_int16* pDst, const ma_int16* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = ma_clip_s16((ma_int32)pDst[iSample] + (ma_int32)pSrc[iSample]);
    }
}

static void ma_clipped_accumulate_s24(ma_uint8* pDst, const ma_uint8* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        ma_int64 s = ma_clip_s24(ma_pcm_sample_s24_to_s32_no_scale(&pDst[iSample*3]) + ma_pcm_sample_s24_to_s32_no_scale(&pSrc[iSample*3]));
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_clipped_accumulate_s32(ma_int32* pDst, const ma_int32* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = ma_clip_s32((ma_int64)pDst[iSample] + (ma_int64)pSrc[iSample]);
    }
}

static void ma_clipped_accumulate_f32(float* pDst, const float* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = ma_clip_f32(pDst[iSample] + pSrc[iSample]);
    }
}

static void ma_clipped_accumulate_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels)
{
    ma_uint64 sampleCount;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    sampleCount = frameCount * channels;

    switch (format) {
        case ma_format_u8:  ma_clipped_accumulate_u8( (ma_uint8*)pDst, (const ma_uint8*)pSrc, sampleCount); break;
        case ma_format_s16: ma_clipped_accumulate_s16((ma_int16*)pDst, (const ma_int16*)pSrc, sampleCount); break;
        case ma_format_s24: ma_clipped_accumulate_s24((ma_uint8*)pDst, (const ma_uint8*)pSrc, sampleCount); break;
        case ma_format_s32: ma_clipped_accumulate_s32((ma_int32*)pDst, (const ma_int32*)pSrc, sampleCount); break;
        case ma_format_f32: ma_clipped_accumulate_f32((   float*)pDst, (const    float*)pSrc, sampleCount); break;

        /* Do nothing if we don't know the format. We're including these here to silence a compiler warning about enums not being handled by the switch. */
        case ma_format_unknown:
        case ma_format_count:
            break;
    }
}
#endif


static void ma_unclipped_accumulate_u8(ma_int16* pDst, const ma_uint8* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = pDst[iSample] + ma_pcm_sample_u8_to_s16_no_scale(pSrc[iSample]);
    }
}

static void ma_unclipped_accumulate_s16(ma_int32* pDst, const ma_int16* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = (ma_int32)pDst[iSample] + (ma_int32)pSrc[iSample];
    }
}

static void ma_unclipped_accumulate_s24(ma_int64* pDst, const ma_uint8* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = pDst[iSample] + ma_pcm_sample_s24_to_s32_no_scale(&pSrc[iSample*3]);
    }
}

static void ma_unclipped_accumulate_s32(ma_int64* pDst, const ma_int32* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = (ma_int64)pDst[iSample] + (ma_int64)pSrc[iSample];
    }
}

static void ma_unclipped_accumulate_f32(float* pDst, const float* pSrc, ma_uint64 sampleCount)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] = pDst[iSample] + pSrc[iSample];
    }
}

static void ma_unclipped_accumulate_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels)
{
    ma_uint64 sampleCount;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    sampleCount = frameCount * channels;

    switch (format) {
        case ma_format_u8:  ma_unclipped_accumulate_u8( (ma_int16*)pDst, (const ma_uint8*)pSrc, sampleCount); break;
        case ma_format_s16: ma_unclipped_accumulate_s16((ma_int32*)pDst, (const ma_int16*)pSrc, sampleCount); break;
        case ma_format_s24: ma_unclipped_accumulate_s24((ma_int64*)pDst, (const ma_uint8*)pSrc, sampleCount); break;
        case ma_format_s32: ma_unclipped_accumulate_s32((ma_int64*)pDst, (const ma_int32*)pSrc, sampleCount); break;
        case ma_format_f32: ma_unclipped_accumulate_f32((   float*)pDst, (const    float*)pSrc, sampleCount); break;

        /* Do nothing if we don't know the format. We're including these here to silence a compiler warning about enums not being handled by the switch. */
        case ma_format_unknown:
        case ma_format_count:
            break;
    }
}


#if 0
static void ma_volume_and_accumulate_and_clip_u8(ma_uint8* pDst, const ma_int16* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_u8(ma_pcm_sample_u8_to_s16_no_scale(pDst[iSample]) + ma_apply_volume_unclipped_u8(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_accumulate_and_clip_s16(ma_int16* pDst, const ma_int32* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_s16(pDst[iSample] + ma_apply_volume_unclipped_s16(pSrc[iSample], volumeFixed));
    }
}

static void ma_volume_and_accumulate_and_clip_s24(ma_uint8* pDst, const ma_int64* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        ma_int64 s = ma_clip_s24(ma_pcm_sample_s24_to_s32_no_scale(&pDst[iSample*3]) + ma_apply_volume_unclipped_s24(pSrc[iSample], volumeFixed));
        pDst[iSample*3 + 0] = (ma_uint8)((s & 0x000000FF) >>  0);
        pDst[iSample*3 + 1] = (ma_uint8)((s & 0x0000FF00) >>  8);
        pDst[iSample*3 + 2] = (ma_uint8)((s & 0x00FF0000) >> 16);
    }
}

static void ma_volume_and_accumulate_and_clip_s32(ma_int32* dst, const ma_int64* src, ma_uint64 count, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(dst != NULL);
    MA_ASSERT(src != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < count; iSample += 1) {
        dst[iSample] = ma_clip_s32(dst[iSample] + ma_apply_volume_unclipped_s32(src[iSample], volumeFixed));
    }
}

static void ma_volume_and_accumulate_and_clip_f32(float* pDst, const float* pSrc, ma_uint64 count, float volume)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < count; iSample += 1) {
        pDst[iSample] = ma_clip_f32(pDst[iSample] + ma_apply_volume_unclipped_f32(pSrc[iSample], volume));
    }
}

static ma_result ma_volume_and_accumulate_and_clip_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels, float volume)
{
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The output buffer cannot be the same as the accumulation buffer. */
    if (pDst == pSrc) {
        return MA_INVALID_OPERATION;
    }

    /* No-op if there's no volume. */
    if (volume == 0) {
        return MA_SUCCESS;
    }

    sampleCount = frameCount * channels;

    /* No need for volume control if the volume is 1. */
    if (volume == 1) {
        switch (format) {
            case ma_format_u8:  ma_accumulate_and_clip_u8( pDst, pSrc, sampleCount); break;
            case ma_format_s16: ma_accumulate_and_clip_s16(pDst, pSrc, sampleCount); break;
            case ma_format_s24: ma_accumulate_and_clip_s24(pDst, pSrc, sampleCount); break;
            case ma_format_s32: ma_accumulate_and_clip_s32(pDst, pSrc, sampleCount); break;
            case ma_format_f32: ma_accumulate_and_clip_f32(pDst, pSrc, sampleCount); break;
            default: return MA_INVALID_ARGS;    /* Unknown format. */
        }
    } else {
        /* Getting here means the volume is not 0 nor 1. */
        MA_ASSERT(volume != 0 && volume != 1);

        switch (format) {
            case ma_format_u8:  ma_volume_and_accumulate_and_clip_u8( pDst, pSrc, sampleCount, volume); break;
            case ma_format_s16: ma_volume_and_accumulate_and_clip_s16(pDst, pSrc, sampleCount, volume); break;
            case ma_format_s24: ma_volume_and_accumulate_and_clip_s24(pDst, pSrc, sampleCount, volume); break;
            case ma_format_s32: ma_volume_and_accumulate_and_clip_s32(pDst, pSrc, sampleCount, volume); break;
            case ma_format_f32: ma_volume_and_accumulate_and_clip_f32(pDst, pSrc, sampleCount, volume); break;
            default: return MA_INVALID_ARGS;        /* Unknown format. */
        }
    }

    return MA_SUCCESS;
}
#endif

static ma_result ma_volume_and_clip_and_effect_pcm_frames(void* pDst, ma_format formatOut, ma_uint32 channelsOut, ma_uint64 frameCountOut, const void* pSrc, ma_format formatIn, ma_uint32 channelsIn, ma_uint64 frameCountIn, float volume, ma_effect* pEffect, ma_bool32 isAccumulation)
{
    ma_result result;
    ma_uint8  effectBufferIn[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
    ma_uint32 effectBufferInCapInFrames;
    ma_uint8  effectBufferOut[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
    ma_uint32 effectBufferOutCapInFrames;
    ma_format effectFormatIn;
    ma_uint32 effectChannelsIn;
    ma_format effectFormatOut;
    ma_uint32 effectChannelsOut;
    ma_uint64 totalFramesProcessedOut = 0;
    ma_uint64 totalFramesProcessedIn  = 0;
    /* */ void* pRunningDst = pDst;
    const void* pRunningSrc = pSrc;

    if (pDst == NULL || pSrc == NULL || pEffect == NULL) {
        return MA_INVALID_ARGS;
    }

    /* No op if silent. */
    if (volume == 0) {
        return MA_SUCCESS;
    }

    /* We need to know the effect's input and output formats so we can do pre- and post-effect data conversion if necessary. */
    ma_effect_get_input_data_format( pEffect, &effectFormatIn,  &effectChannelsIn,  NULL);
    ma_effect_get_output_data_format(pEffect, &effectFormatOut, &effectChannelsOut, NULL);

    effectBufferInCapInFrames  = sizeof(effectBufferIn ) / ma_get_bytes_per_frame(effectFormatIn,  effectChannelsIn );
    effectBufferOutCapInFrames = sizeof(effectBufferOut) / ma_get_bytes_per_frame(effectFormatOut, effectChannelsOut);

    while (totalFramesProcessedOut < frameCountOut && totalFramesProcessedIn < frameCountIn) {
        ma_uint64 effectFrameCountIn;
        ma_uint64 effectFrameCountOut;

        effectFrameCountOut = frameCountOut - totalFramesProcessedOut;
        if (effectFrameCountOut > effectBufferOutCapInFrames) {
            effectFrameCountOut = effectBufferOutCapInFrames;
        }

        effectFrameCountIn = ma_effect_get_required_input_frame_count(pEffect, effectFrameCountOut);
        if (effectFrameCountIn > frameCountIn - totalFramesProcessedIn) {
            effectFrameCountIn = frameCountIn - totalFramesProcessedIn;
        }
        if (effectFrameCountIn > effectBufferInCapInFrames) {
            effectFrameCountIn = effectBufferInCapInFrames;
        }

        /*
        The first step is to get the data ready for the effect. If the effect's input format and channels are the same as the source buffer, we just
        clip the accumulation buffer straight input the effect's input buffer. Otherwise need to do a conversion.
        */
        if (effectFormatIn == formatIn && effectChannelsIn == channelsIn) {
            /* Fast path. No data conversion required for the input data except clipping. */
            ma_volume_and_clip_pcm_frames(effectBufferIn, pRunningSrc, effectFrameCountIn, formatIn, channelsIn, volume);
        } else {
            /* Slow path. Data conversion required between the input data and the effect input data. */
            ma_uint8  clippedSrcBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint32 clippedSrcBufferCapInFrames = sizeof(clippedSrcBuffer) / ma_get_bytes_per_frame(formatIn, channelsIn);

            if (effectFrameCountIn > clippedSrcBufferCapInFrames) {
                effectFrameCountIn = clippedSrcBufferCapInFrames;
            }

            ma_volume_and_clip_pcm_frames(clippedSrcBuffer, pRunningSrc, effectFrameCountIn, formatIn, channelsIn, volume);

            /* At this point the input data has had volume and clipping applied. We can now convert this to the effect's input format. */
            ma_convert_pcm_frames_format_and_channels(effectBufferIn, effectFormatIn, effectChannelsIn, clippedSrcBuffer, formatIn, channelsIn, effectFrameCountIn, ma_dither_mode_none);
        }

        /* At this point we have our input data in the effect's input format and we can now apply it. */
        result = ma_effect_process_pcm_frames(pEffect, effectBufferIn, &effectFrameCountIn, effectBufferOut, &effectFrameCountOut);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to process the effect. */
        }

        /*
        The effect has been applied. If the effect's output format is the same as the final output we can just accumulate straight into the output buffer,
        otherwise we need to convert.
        */
        if (effectFormatOut == formatOut && effectChannelsOut == channelsOut) {
            /* Fast path. No data conversion required for output data. Just accumulate or overwrite. */
            if (isAccumulation) {
                ma_unclipped_accumulate_pcm_frames(pRunningDst, effectBufferOut, effectFrameCountOut, effectFormatOut, effectChannelsOut);
            } else {
                ma_clip_pcm_frames(pRunningDst, effectBufferOut, effectFrameCountOut, effectFormatOut, effectChannelsOut);
            }
        } else {
            /* Slow path. Data conversion required before accumulating. */
            ma_uint8  accumulationInBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint32 accumulationInBufferCapInFrames = sizeof(accumulationInBuffer) / ma_get_bytes_per_frame(formatOut, channelsOut);
            ma_uint64 totalFramesAccumulated = 0;
            ma_uint8* pRunningEffectBufferOut = effectBufferOut;

            while (totalFramesAccumulated < effectFrameCountOut) {
                ma_uint64 framesToAccumulate = effectFrameCountOut - totalFramesAccumulated;
                if (framesToAccumulate > accumulationInBufferCapInFrames) {
                    framesToAccumulate = accumulationInBufferCapInFrames;
                }

                /* We know how many frames to process in this iteration, so first of all do the conversion from the effect's output to the final output format.*/
                ma_convert_pcm_frames_format_and_channels(accumulationInBuffer, formatOut, channelsOut, pRunningEffectBufferOut, effectFormatOut, effectChannelsOut, framesToAccumulate, ma_dither_mode_none);

                /* We have the data in the final output format, so now we just accumulate or overwrite. */
                if (isAccumulation) {
                    ma_unclipped_accumulate_pcm_frames(ma_offset_ptr(pRunningDst, totalFramesAccumulated * ma_get_accumulation_bytes_per_frame(formatOut, channelsOut)), accumulationInBuffer, framesToAccumulate, formatOut, channelsOut);
                } else {
                    ma_clip_pcm_frames(ma_offset_ptr(pRunningDst, totalFramesAccumulated * ma_get_bytes_per_frame(formatOut, channelsOut)), accumulationInBuffer, framesToAccumulate, formatOut, channelsOut);
                }
                
                totalFramesAccumulated += framesToAccumulate;
                pRunningEffectBufferOut = ma_offset_ptr(pRunningEffectBufferOut, framesToAccumulate * ma_get_bytes_per_frame(formatOut, channelsOut));
            }
        }

        totalFramesProcessedIn  += effectFrameCountIn;
        totalFramesProcessedOut += effectFrameCountOut;

        pRunningSrc = ma_offset_ptr(pRunningSrc, effectFrameCountIn * ma_get_accumulation_bytes_per_frame(formatIn, channelsIn));
        if (isAccumulation) {
            pRunningDst = ma_offset_ptr(pRunningDst, effectFrameCountOut * ma_get_accumulation_bytes_per_frame(formatIn, channelsIn));
        } else {
            pRunningDst = ma_offset_ptr(pRunningDst, effectFrameCountOut * ma_get_bytes_per_frame(formatOut, channelsOut));
        }
    }

    return MA_SUCCESS;
}


static ma_result ma_mix_pcm_frames_u8(ma_int16* pDst, const ma_uint8* pSrc, ma_uint64 frameCount, ma_uint32 channels, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }
    
    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_pcm_sample_u8_to_s16_no_scale(pSrc[iSample]);
        }
    } else {
        ma_int16 volumeFixed = ma_float_to_fixed_16(volume);
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_u8(ma_pcm_sample_u8_to_s16_no_scale(pSrc[iSample]), volumeFixed);
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames_s16(ma_int32* pDst, const ma_int16* pSrc, ma_uint64 frameCount, ma_uint32 channels, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }
    
    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += pSrc[iSample];
        }
    } else {
        ma_int16 volumeFixed = ma_float_to_fixed_16(volume);
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_s16(pSrc[iSample], volumeFixed);
        }
    }
    
    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames_s24(ma_int64* pDst, const ma_uint8* pSrc, ma_uint64 frameCount, ma_uint32 channels, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }
    
    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_pcm_sample_s24_to_s32_no_scale(&pSrc[iSample*3]);
        }
    } else {
        ma_int16 volumeFixed = ma_float_to_fixed_16(volume);
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_s24(ma_pcm_sample_s24_to_s32_no_scale(&pSrc[iSample*3]), volumeFixed);
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames_s32(ma_int64* pDst, const ma_int32* pSrc, ma_uint64 frameCount, ma_uint32 channels, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }
    
    
    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += pSrc[iSample];
        }
    } else {
        ma_int16 volumeFixed = ma_float_to_fixed_16(volume);
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_s32(pSrc[iSample], volumeFixed);
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames_f32(float* pDst, const float* pSrc, ma_uint64 frameCount, ma_uint32 channels, float volume)
{
    ma_uint64 iSample;
    ma_uint64 sampleCount;

    if (pDst == NULL || pSrc == NULL || channels == 0) {
        return MA_INVALID_ARGS;
    }

    if (volume == 0) {
        return MA_SUCCESS;  /* No changes if the volume is 0. */
    }

    sampleCount = frameCount * channels;

    if (volume == 1) {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += pSrc[iSample];
        }
    } else {
        for (iSample = 0; iSample < sampleCount; iSample += 1) {
            pDst[iSample] += ma_apply_volume_unclipped_f32(pSrc[iSample], volume);
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_mix_pcm_frames(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format format, ma_uint32 channels, float volume)
{
    ma_result result;

    switch (format)
    {
        case ma_format_u8:  result = ma_mix_pcm_frames_u8( (ma_int16*)pDst, (const ma_uint8*)pSrc, frameCount, channels, volume); break;
        case ma_format_s16: result = ma_mix_pcm_frames_s16((ma_int32*)pDst, (const ma_int16*)pSrc, frameCount, channels, volume); break;
        case ma_format_s24: result = ma_mix_pcm_frames_s24((ma_int64*)pDst, (const ma_uint8*)pSrc, frameCount, channels, volume); break;
        case ma_format_s32: result = ma_mix_pcm_frames_s32((ma_int64*)pDst, (const ma_int32*)pSrc, frameCount, channels, volume); break;
        case ma_format_f32: result = ma_mix_pcm_frames_f32((   float*)pDst, (const    float*)pSrc, frameCount, channels, volume); break;
        default: return MA_INVALID_ARGS;    /* Unknown format. */
    }

    return result;
}

static ma_result ma_mix_pcm_frames_ex(void* pDst, ma_format formatOut, ma_uint32 channelsOut, const void* pSrc, ma_format formatIn, ma_uint32 channelsIn, ma_uint64 frameCount, float volume)
{
    if (pDst == NULL || pSrc == NULL) {
        return MA_INVALID_ARGS;
    }

    if (formatOut == formatIn && channelsOut == channelsIn) {
        /* Fast path. */
        return ma_mix_pcm_frames(pDst, pSrc, frameCount, formatOut, channelsOut, volume);
    } else {
        /* Slow path. Data conversion required. */
        ma_uint8  buffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
        ma_uint32 bufferCapInFrames = sizeof(buffer) / ma_get_bytes_per_frame(formatOut, channelsOut);
        ma_uint64 totalFramesProcessed = 0;
        /* */ void* pRunningDst = pDst;
        const void* pRunningSrc = pSrc;

        while (totalFramesProcessed < frameCount) {
            ma_uint64 framesToProcess = frameCount - totalFramesProcessed;
            if (framesToProcess > bufferCapInFrames) {
                framesToProcess = bufferCapInFrames;
            }

            /* Conversion. */
            ma_convert_pcm_frames_format_and_channels(buffer, formatOut, channelsOut, pRunningSrc, formatIn, channelsIn, framesToProcess, ma_dither_mode_none);

            /* Mixing. */
            ma_mix_pcm_frames(pRunningDst, buffer, framesToProcess, formatOut, channelsOut, volume);

            totalFramesProcessed += framesToProcess;
            pRunningDst = ma_offset_ptr(pRunningDst, framesToProcess * ma_get_accumulation_bytes_per_frame(formatOut, channelsOut));
            pRunningSrc = ma_offset_ptr(pRunningSrc, framesToProcess * ma_get_bytes_per_frame(formatIn, channelsIn));
        }
    }

    return MA_SUCCESS;
}


static void ma_mix_accumulation_buffers_u8(ma_int16* pDst, const ma_int16* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_u8(pSrc[iSample], volumeFixed);
    }
}

static void ma_mix_accumulation_buffers_s16(ma_int32* pDst, const ma_int32* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_s16(pSrc[iSample], volumeFixed);
    }
}

static void ma_mix_accumulation_buffers_s24(ma_int64* pDst, const ma_int64* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_s24(pSrc[iSample], volumeFixed);
    }
}

static void ma_mix_accumulation_buffers_s32(ma_int64* pDst, const ma_int64* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;
    ma_int16  volumeFixed;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    volumeFixed = ma_float_to_fixed_16(volume);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_s32(pSrc[iSample], volumeFixed);
    }
}

static void ma_mix_accumulation_buffers_f32(float* pDst, const float* pSrc, ma_uint64 sampleCount, float volume)
{
    ma_uint64 iSample;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    for (iSample = 0; iSample < sampleCount; iSample += 1) {
        pDst[iSample] += ma_apply_volume_unclipped_f32(pSrc[iSample], volume);
    }
}

static void ma_mix_accumulation_buffers(void* pDst, const void* pSrc, ma_uint64 frameCount, ma_format formatIn, ma_uint32 channelsIn, float volume)
{
    ma_uint64 sampleCount;

    MA_ASSERT(pDst != NULL);
    MA_ASSERT(pSrc != NULL);

    sampleCount = frameCount * channelsIn;

    switch (formatIn)
    {
        case ma_format_u8:  ma_mix_accumulation_buffers_u8( (ma_int16*)pDst, (const ma_int16*)pSrc, sampleCount, volume); break;
        case ma_format_s16: ma_mix_accumulation_buffers_s16((ma_int32*)pDst, (const ma_int32*)pSrc, sampleCount, volume); break;
        case ma_format_s24: ma_mix_accumulation_buffers_s24((ma_int64*)pDst, (const ma_int64*)pSrc, sampleCount, volume); break;
        case ma_format_s32: ma_mix_accumulation_buffers_s32((ma_int64*)pDst, (const ma_int64*)pSrc, sampleCount, volume); break;
        case ma_format_f32: ma_mix_accumulation_buffers_f32((   float*)pDst, (const    float*)pSrc, sampleCount, volume); break;
        default: break;
    }
}

static void ma_mix_accumulation_buffers_ex(void* pDst, ma_format formatOut, ma_uint32 channelsOut, const void* pSrc, ma_format formatIn, ma_uint32 channelsIn, ma_uint64 frameCount, float volume)
{
    if (formatOut == formatIn && channelsOut == channelsIn) {
        /* Fast path. No conversion required. */
        ma_mix_accumulation_buffers(pDst, pSrc, frameCount, formatIn, channelsIn, volume);
    } else {
        /* Slow path. Conversion required. The way we're going to do this is clip the input buffer, and then use existing mixing infrastructure to mix as if it were regular input. */
        ma_uint8  clippedSrcBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE]; /* formatIn, channelsIn */
        ma_uint32 clippedSrcBufferCapInFrames = sizeof(clippedSrcBuffer) / ma_get_bytes_per_frame(formatIn, channelsIn);
        ma_uint64 totalFramesProcessed = 0;
        /* */ void* pRunningDst = pDst;
        const void* pRunningSrc = pSrc;

        while (totalFramesProcessed < frameCount) {
            ma_uint64 framesToProcess = frameCount - totalFramesProcessed;
            if (framesToProcess > clippedSrcBufferCapInFrames) {
                framesToProcess = clippedSrcBufferCapInFrames;
            }

            /* Volume and clip. */
            ma_volume_and_clip_pcm_frames(clippedSrcBuffer, pRunningSrc, framesToProcess, formatIn, channelsIn, volume);

            /* Mix. */
            ma_mix_pcm_frames_ex(pRunningDst, formatOut, channelsOut, clippedSrcBuffer, formatIn, channelsIn, framesToProcess, 1);
            
            totalFramesProcessed += framesToProcess;
            pRunningDst = ma_offset_ptr(pRunningDst, framesToProcess * ma_get_accumulation_bytes_per_frame(formatOut, channelsOut));
            pRunningSrc = ma_offset_ptr(pRunningSrc, framesToProcess * ma_get_accumulation_bytes_per_frame(formatIn,  channelsIn ));
        }
    }
}




MA_API ma_mixer_config ma_mixer_config_init(ma_format format, ma_uint32 channels, ma_uint64 accumulationBufferSizeInFrames, void* pPreAllocatedAccumulationBuffer, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_mixer_config config;
    
    MA_ZERO_OBJECT(&config);
    config.format = format;
    config.channels = channels;
    config.accumulationBufferSizeInFrames = accumulationBufferSizeInFrames;
    config.pPreAllocatedAccumulationBuffer = pPreAllocatedAccumulationBuffer;
    config.volume = 1;
    ma_allocation_callbacks_init_copy(&config.allocationCallbacks, pAllocationCallbacks);

    return config;
}


MA_API ma_result ma_mixer_init(ma_mixer_config* pConfig, ma_mixer* pMixer)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pMixer);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->accumulationBufferSizeInFrames == 0) {
        return MA_INVALID_ARGS; /* Must have an accumulation buffer. */
    }

    pMixer->format                         = pConfig->format;
    pMixer->channels                       = pConfig->channels;
    pMixer->accumulationBufferSizeInFrames = pConfig->accumulationBufferSizeInFrames;
    pMixer->pAccumulationBuffer            = pConfig->pPreAllocatedAccumulationBuffer;
    ma_allocation_callbacks_init_copy(&pMixer->allocationCallbacks, &pConfig->allocationCallbacks);
    pMixer->volume                         = pConfig->volume;

    if (pMixer->pAccumulationBuffer == NULL) {
        ma_uint64 accumulationBufferSizeInBytes = pConfig->accumulationBufferSizeInFrames * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels);
        if (accumulationBufferSizeInBytes > MA_SIZE_MAX) {
            return MA_OUT_OF_MEMORY;
        }

        pMixer->pAccumulationBuffer = ma__malloc_from_callbacks((size_t)accumulationBufferSizeInBytes, &pMixer->allocationCallbacks);   /* Safe cast. */
        if (pMixer->pAccumulationBuffer == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        pMixer->ownsAccumulationBuffer = MA_TRUE;
    } else {
        pMixer->ownsAccumulationBuffer = MA_FALSE;
    }

    return MA_SUCCESS;
}

MA_API void ma_mixer_uninit(ma_mixer* pMixer)
{
    if (pMixer == NULL) {
        return;
    }

    if (pMixer->ownsAccumulationBuffer) {
        ma__free_from_callbacks(pMixer->pAccumulationBuffer, &pMixer->allocationCallbacks);
    }
}

MA_API ma_result ma_mixer_begin(ma_mixer* pMixer, ma_mixer* pParentMixer, ma_uint64* pFrameCountOut, ma_uint64* pFrameCountIn)
{
    ma_uint64 frameCountOut;
    ma_uint64 frameCountIn;

    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pMixer->mixingState.isInsideBeginEnd == MA_TRUE) {
        return MA_INVALID_OPERATION;    /* Cannot call this while already inside a begin/end pair. */
    }

    /* If we're submixing we need to make the frame counts compatible with the parent mixer. */
    if (pParentMixer != NULL) {
        /* The output frame count must match the input frame count of the parent. If this cannot be accommodated we need to fail. */
        frameCountOut = pParentMixer->mixingState.frameCountIn;
    } else {
        if (pFrameCountOut == NULL) {
            return MA_INVALID_ARGS; /* The desired output frame count is required for a root level mixer. */
        }

        frameCountOut = *pFrameCountOut;
    }

    if (pMixer->pEffect != NULL) {
        frameCountIn = ma_effect_get_required_input_frame_count(pMixer->pEffect, frameCountOut);
        if (frameCountIn > pMixer->accumulationBufferSizeInFrames) {
            /*
            The required number of input frames for the requested number of output frames is too much to fit in the accumulation buffer. We need
            to reduce the output frame count to accommodate.
            */
            ma_uint64 newFrameCountOut;
            newFrameCountOut = ma_effect_get_expected_output_frame_count(pMixer->pEffect, pMixer->accumulationBufferSizeInFrames);
            MA_ASSERT(newFrameCountOut <= frameCountOut);

            frameCountOut = newFrameCountOut;
            frameCountIn  = ma_effect_get_required_input_frame_count(pMixer->pEffect, frameCountOut);
        }
    } else {
        frameCountIn = frameCountOut;

        if (frameCountIn  > pMixer->accumulationBufferSizeInFrames) {
            frameCountIn  = pMixer->accumulationBufferSizeInFrames;
            frameCountOut = pMixer->accumulationBufferSizeInFrames;
        }
    }

    /* If the output frame count cannot match the parent's input frame count we need to fail. */
    if (pParentMixer != NULL && frameCountOut != pParentMixer->mixingState.frameCountIn) {
        return MA_INVALID_OPERATION;    /* Not compatible with the parent mixer. */
    }

    pMixer->mixingState.isInsideBeginEnd = MA_TRUE;
    pMixer->mixingState.frameCountOut    = frameCountOut;
    pMixer->mixingState.frameCountIn     = frameCountIn;

    ma_zero_memory_64(pMixer->pAccumulationBuffer, frameCountIn * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels));

    if (pFrameCountOut != NULL) {
        *pFrameCountOut = frameCountOut;
    }
    if (pFrameCountIn != NULL) {
        *pFrameCountIn  = frameCountIn;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_end(ma_mixer* pMixer, ma_mixer* pParentMixer, void* pFramesOut, ma_uint64 outputOffsetInFrames)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    /* It's an error for both pParentMixer and pFramesOut to be NULL. */
    if (pParentMixer == NULL && pFramesOut == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If both pParentMixer and pFramesOut are both non-NULL, it indicates an error on the callers side. Make sure they're aware of it. */
    if (pParentMixer != NULL && pFramesOut != NULL) {
        MA_ASSERT(MA_FALSE);
        return MA_INVALID_ARGS;
    }

    if (pMixer->mixingState.isInsideBeginEnd == MA_FALSE) {
        return MA_INVALID_OPERATION;    /* No matching begin. */
    }

    /* Completely different paths if we're outputting to a parent mixer rather than directly to an output buffer. */
    if (pParentMixer != NULL) {
        ma_format localFormatOut;
        ma_uint32 localChannelsOut;
        ma_format parentFormatIn;
        ma_uint32 parentChannelsIn;
        void* pDst;

        /*
        We need to accumulate the output of pMixer straight into the accumulation buffer of pParentMixer. If the output format of pMixer is different
        to the input format of pParentMixer it needs to be converted.
        */
        ma_mixer_get_output_data_format(pMixer, &localFormatOut, &localChannelsOut);
        ma_mixer_get_input_data_format(pParentMixer, &parentFormatIn, &parentChannelsIn);

        /* A reminder that the output frame count of pMixer must match the input frame count of pParentMixer. */
        MA_ASSERT(pMixer->mixingState.frameCountOut == pParentMixer->mixingState.frameCountIn);

        pDst = ma_offset_ptr(pParentMixer->pAccumulationBuffer, outputOffsetInFrames * ma_get_accumulation_bytes_per_frame(parentFormatIn, parentChannelsIn));

        if (pMixer->pEffect == NULL) {
            /* No effect. Input needs to come straight from the accumulation buffer. */
            ma_mix_accumulation_buffers_ex(pDst, parentFormatIn, parentChannelsIn, pMixer->pAccumulationBuffer, localFormatOut, localChannelsOut, pMixer->mixingState.frameCountOut, pMixer->volume);
        } else {
            /* With effect. Input needs to be pre-processed from the effect. */
            ma_volume_and_clip_and_effect_pcm_frames(pDst, parentFormatIn, parentChannelsIn, pParentMixer->mixingState.frameCountIn, pMixer->pAccumulationBuffer, pMixer->format, pMixer->channels, pMixer->mixingState.frameCountIn, pMixer->volume, pMixer->pEffect, /*isAccumulation*/ MA_TRUE);
        }
    } else {
        /* We're not submixing so we can overwite. */
        void* pDst;

        pDst = ma_offset_ptr(pFramesOut, outputOffsetInFrames * ma_get_bytes_per_frame(pMixer->format, pMixer->channels));

        if (pMixer->pEffect == NULL) {
            /* All we need to do is convert the accumulation buffer to the output format. */
            ma_volume_and_clip_pcm_frames(pDst, pMixer->pAccumulationBuffer, pMixer->mixingState.frameCountOut, pMixer->format, pMixer->channels, pMixer->volume);
        } else {
            /* We need to run our accumulation through the effect. */
            ma_volume_and_clip_and_effect_pcm_frames(pDst, pMixer->format, pMixer->channels, pMixer->mixingState.frameCountOut, pMixer->pAccumulationBuffer, pMixer->format, pMixer->channels, pMixer->mixingState.frameCountIn, pMixer->volume, pMixer->pEffect, /*isAccumulation*/ MA_FALSE);
        }
    }

    pMixer->mixingState.isInsideBeginEnd = MA_FALSE;
    pMixer->mixingState.frameCountOut    = 0;
    pMixer->mixingState.frameCountIn     = 0;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_mix_pcm_frames(ma_mixer* pMixer, const void* pFramesIn, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, float volume, ma_format formatIn, ma_uint32 channelsIn)
{
    if (pMixer == NULL || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    if (frameCountIn > pMixer->mixingState.frameCountIn) {
        return MA_INVALID_ARGS; /* Passing in too many input frames. */
    }

    ma_mix_pcm_frames(ma_offset_ptr(pMixer->pAccumulationBuffer, offsetInFrames * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels)), pFramesIn, frameCountIn, formatIn, channelsIn, volume);

    return MA_SUCCESS;
}

static ma_result ma_mixer_mix_data_source_mmap(ma_mixer* pMixer, ma_data_source* pDataSource, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, ma_uint64* pFrameCountOut, float volume, ma_effect* pEffect, ma_format formatIn, ma_uint32 channelsIn, ma_bool32 loop)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 totalFramesProcessed = 0;
    void* pRunningAccumulationBuffer = NULL;
    ma_bool32 preEffectConversionRequired = MA_FALSE;
    ma_format effectFormatIn = ma_format_unknown;
    ma_uint32 effectChannelsIn = 0;
    ma_format effectFormatOut = ma_format_unknown;
    ma_uint32 effectChannelsOut = 0;

    MA_ASSERT(pMixer      != NULL);
    MA_ASSERT(pDataSource != NULL);

    if (pFrameCountOut != NULL) {
        *pFrameCountOut = 0;
    }

    if ((offsetInFrames + frameCountIn) > pMixer->mixingState.frameCountIn) {
        return MA_INVALID_ARGS; /* Passing in too many input frames. */
    }

    /* Initially offset the accumulation buffer by the offset. */
    pRunningAccumulationBuffer = ma_offset_ptr(pMixer->pAccumulationBuffer, offsetInFrames * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels));

    if (pEffect != NULL) {
        /* We need to know the effect's input and output data format before we'll be able to apply it properly. */
        result = ma_effect_get_input_data_format(pEffect, &effectFormatIn, &effectChannelsIn, NULL);
        if (result != MA_SUCCESS) {
            return result;
        }

        result = ma_effect_get_output_data_format(pEffect, &effectFormatOut, &effectChannelsOut, NULL);
        if (result != MA_SUCCESS) {
            return result;
        }

        preEffectConversionRequired = (formatIn != effectFormatIn || channelsIn != effectChannelsIn);
    }

    while (totalFramesProcessed < frameCountIn) {
        void* pMappedBuffer;
        ma_uint64 framesToProcess = frameCountIn - totalFramesProcessed;

        if (pEffect == NULL) {
            /* Fast path. Mix directly from the data source and don't bother applying an effect. */
            result = ma_data_source_map(pDataSource, &pMappedBuffer, &framesToProcess);
            if (result != MA_SUCCESS) {
                break;  /* Failed to map. Abort. */
            }

            if (framesToProcess == 0) {
                break;  /* Wasn't able to map any data. Abort. */
            }

            ma_mix_pcm_frames_ex(pRunningAccumulationBuffer, pMixer->format, pMixer->channels, pMappedBuffer, formatIn, channelsIn, framesToProcess, volume);

            result = ma_data_source_unmap(pDataSource, framesToProcess);
        } else {
            /* Slow path. Need to apply an effect. This requires the use of an intermediary buffer. */
            ma_uint8 effectInBuffer [MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint8 effectOutBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint32 effectInBufferCap  = sizeof(effectInBuffer)  / ma_get_bytes_per_frame(effectFormatIn,  effectChannelsIn);
            ma_uint32 effectOutBufferCap = sizeof(effectOutBuffer) / ma_get_bytes_per_frame(effectFormatOut, effectChannelsOut);
            ma_uint64 framesMapped;

            if (framesToProcess > effectOutBufferCap) {
                framesToProcess = effectOutBufferCap;
            }

            framesMapped = ma_effect_get_required_input_frame_count(pEffect, framesToProcess);
            if (framesMapped > effectInBufferCap) {
                framesMapped = effectInBufferCap;
            }

            /* We need to map our input data first. The input data will be either fed directly into the effect, or will be converted first. */
            result = ma_data_source_map(pDataSource, &pMappedBuffer, &framesMapped);
            if (result != MA_SUCCESS) {
                break;  /* Failed to map. Abort. */
            }

            /* We have the data from the data source so no we can apply the effect. */
            if (preEffectConversionRequired == MA_FALSE) {
                /* Fast path. No format required before applying the effect. */
                ma_effect_process_pcm_frames(pEffect, pMappedBuffer, &framesMapped, effectOutBuffer, &framesToProcess);
            } else {
                /* Slow path. Need to convert the data before applying the effect. */
                ma_convert_pcm_frames_format_and_channels(effectInBuffer, effectFormatIn, effectChannelsIn, pMappedBuffer, formatIn, channelsIn, framesMapped, ma_dither_mode_none);
                ma_effect_process_pcm_frames(pEffect, effectInBuffer, &framesMapped, effectOutBuffer, &framesToProcess);
            }

            /* The effect has been applied so now we can mix it. */
            ma_mix_pcm_frames_ex(pRunningAccumulationBuffer, pMixer->format, pMixer->channels, effectOutBuffer, effectFormatOut, effectChannelsOut, framesToProcess, volume);

            /* We're finished with the input data. */
            result = ma_data_source_unmap(pDataSource, framesMapped);   /* Do this last because the result code is used below to determine whether or not we need to loop. */
        }

        totalFramesProcessed += framesToProcess;
        pRunningAccumulationBuffer = ma_offset_ptr(pRunningAccumulationBuffer, framesToProcess * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels));

        if (result != MA_SUCCESS) {
            if (result == MA_AT_END) {
                if (loop) {
                    ma_data_source_seek_to_pcm_frame(pDataSource, 0);
                    result = MA_SUCCESS;    /* Make sure we don't return MA_AT_END which will happen if we conicidentally hit the end of the data source at the same time as we finish outputting. */
                } else {
                    break;  /* We've reached the end and we're not looping. */
                }
            } else {
                break;  /* An error occurred. */
            }
        }
    }

    if (pFrameCountOut != NULL) {
        *pFrameCountOut = totalFramesProcessed;
    }

    return result;
}

static ma_result ma_mixer_mix_data_source_read(ma_mixer* pMixer, ma_data_source* pDataSource, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, ma_uint64* pFrameCountOut, float volume, ma_effect* pEffect, ma_format formatIn, ma_uint32 channelsIn, ma_bool32 loop)
{
    ma_result result = MA_SUCCESS;
    ma_uint8  preMixBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
    ma_uint32 preMixBufferCap;
    ma_uint64 totalFramesProcessed = 0;
    void* pRunningAccumulationBuffer;
    ma_format effectFormatIn = ma_format_unknown;
    ma_uint32 effectChannelsIn = 0;
    ma_format preMixFormat = ma_format_unknown;
    ma_uint32 preMixChannels = 0;
    ma_bool32 preEffectConversionRequired = MA_FALSE;

    MA_ASSERT(pMixer      != NULL);
    MA_ASSERT(pDataSource != NULL);

    if (pFrameCountOut != NULL) {
        *pFrameCountOut = 0;
    }

    if ((offsetInFrames + frameCountIn) > pMixer->mixingState.frameCountIn) {
        return MA_INVALID_ARGS; /* Passing in too many input frames. */
    }

    if (pEffect == NULL) {
        preMixFormat   = formatIn;
        preMixChannels = channelsIn;
    } else {
        /* We need to know the effect's input and output data format before we'll be able to apply it properly. */
        result = ma_effect_get_input_data_format(pEffect, &effectFormatIn, &effectChannelsIn, NULL);
        if (result != MA_SUCCESS) {
            return result;
        }

        result = ma_effect_get_output_data_format(pEffect, &preMixFormat, &preMixChannels, NULL);
        if (result != MA_SUCCESS) {
            return result;
        }

        preEffectConversionRequired = (formatIn != effectFormatIn || channelsIn != effectChannelsIn);
    }

    preMixBufferCap = sizeof(preMixBuffer) / ma_get_bytes_per_frame(preMixFormat, preMixChannels);

    totalFramesProcessed = 0;

    /* Initially offset the accumulation buffer by the offset. */
    pRunningAccumulationBuffer = ma_offset_ptr(pMixer->pAccumulationBuffer, offsetInFrames * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels));

    while (totalFramesProcessed < frameCountIn) {
        ma_uint64 framesRead;
        ma_uint64 framesToRead = frameCountIn - totalFramesProcessed;
        if (framesToRead > preMixBufferCap) {
            framesToRead = preMixBufferCap;
        }

        if (pEffect == NULL) {
            result = ma_data_source_read_pcm_frames(pDataSource, preMixBuffer, framesToRead, &framesRead, loop);
            ma_mix_pcm_frames_ex(pRunningAccumulationBuffer, pMixer->format, pMixer->channels, preMixBuffer, formatIn, channelsIn, framesRead, volume);
        } else {
            ma_uint8 callbackBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint8 effectInBuffer[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
            ma_uint32 callbackBufferCap = sizeof(callbackBuffer) / ma_get_bytes_per_frame(formatIn, channelsIn);
            ma_uint32 effectInBufferCap = sizeof(effectInBuffer) / ma_get_bytes_per_frame(effectFormatIn, effectChannelsIn);
            ma_uint64 effectFrameCountOut;
            ma_uint64 effectFrameCountIn;
            ma_uint64 framesReadFromCallback;
            ma_uint64 framesToReadFromCallback = ma_effect_get_required_input_frame_count(pEffect, framesToRead);
            if (framesToReadFromCallback > callbackBufferCap) {
                framesToReadFromCallback = callbackBufferCap;
            }
            if (framesToReadFromCallback > effectInBufferCap) {
                framesToReadFromCallback = effectInBufferCap;
            }

            /*
            We can now read some data from the callback. We should never read more input frame than will be consumed. If the format of the callback is the same as the effect's input
            format we can save ourselves a copy and run on a slightly faster path.
            */
            if (preEffectConversionRequired == MA_FALSE) {
                /* Fast path. No need for conversion between the callback and the  */
                result = ma_data_source_read_pcm_frames(pDataSource, effectInBuffer, framesToReadFromCallback, &framesReadFromCallback, loop);
            } else {
                /* Slow path. Conversion between the callback and the effect required. */
                result = ma_data_source_read_pcm_frames(pDataSource, callbackBuffer, framesToReadFromCallback, &framesReadFromCallback, loop);
                ma_convert_pcm_frames_format_and_channels(effectInBuffer, effectFormatIn, effectChannelsIn, callbackBuffer, formatIn, channelsIn, framesReadFromCallback, ma_dither_mode_none);
            }

            /* We have our input data for the effect so now we just process as much as we can based on our input and output frame counts. */
            effectFrameCountIn  = framesReadFromCallback;
            effectFrameCountOut = framesToRead;
            ma_effect_process_pcm_frames(pEffect, effectInBuffer, &effectFrameCountIn, preMixBuffer, &effectFrameCountOut);

            /* At this point the effect should be applied and we can mix it. */
            framesRead = (ma_uint32)effectFrameCountOut;    /* Safe cast. */
            ma_mix_pcm_frames_ex(pRunningAccumulationBuffer, pMixer->format, pMixer->channels, preMixBuffer, preMixFormat, preMixChannels, framesRead, volume);

            /* An emergency failure case. Abort if we didn't consume any input nor any output frames. */
            if (framesRead == 0 && framesReadFromCallback == 0) {
                break;
            }
        }

        totalFramesProcessed += framesRead;
        pRunningAccumulationBuffer = ma_offset_ptr(pRunningAccumulationBuffer, framesRead * ma_get_accumulation_bytes_per_frame(pMixer->format, pMixer->channels));

        /* If the data source is busy we need to end mixing now. */
        if (result == MA_BUSY || result == MA_AT_END) {
            break;
        }
    }

    if (pFrameCountOut != NULL) {
        *pFrameCountOut = totalFramesProcessed;
    }

    return result;
}

MA_API ma_result ma_mixer_mix_data_source(ma_mixer* pMixer, ma_data_source* pDataSource, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, ma_uint64* pFrameCountOut, float volume, ma_effect* pEffect, ma_bool32 loop)
{
    ma_result result;
    ma_format formatIn;
    ma_uint32 channelsIn;
    ma_bool32 supportsMMap = MA_FALSE;
    ma_data_source_callbacks* pDataSourceCallbacks = (ma_data_source_callbacks*)pDataSource;

    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }
    
    result = ma_data_source_get_data_format(pDataSource, &formatIn, &channelsIn, NULL);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* Use memory mapping if it's available. */
    if (pDataSourceCallbacks->onMap != NULL && pDataSourceCallbacks->onUnmap != NULL) {
        supportsMMap = MA_TRUE;
    }

    if (supportsMMap) {
        /* Fast path. This is memory mapping mode. */
        return ma_mixer_mix_data_source_mmap(pMixer, pDataSourceCallbacks, offsetInFrames, frameCountIn, pFrameCountOut, volume, pEffect, formatIn, channelsIn, loop);
    } else {
        /* Slow path. This is reading mode. */
        return ma_mixer_mix_data_source_read(pMixer, pDataSourceCallbacks, offsetInFrames, frameCountIn, pFrameCountOut, volume, pEffect, formatIn, channelsIn, loop);
    }
}


typedef struct
{
    ma_data_source_callbacks ds;
    ma_rb* pRB;
    ma_format format;
    ma_uint32 channels;
    void* pMappedBuffer;
} ma_rb_data_source;

static ma_result ma_rb_data_source__on_map(ma_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount)
{
    ma_rb_data_source* pRB = (ma_rb_data_source*)pDataSource;
    ma_result result;
    ma_uint32 bpf = ma_get_bytes_per_frame(pRB->format, pRB->channels);
    size_t sizeInBytes;

    sizeInBytes = (size_t)(*pFrameCount * bpf);
    result = ma_rb_acquire_read(pRB->pRB, &sizeInBytes, ppFramesOut);
    *pFrameCount = sizeInBytes / bpf;

    pRB->pMappedBuffer = *ppFramesOut;

    return result;
}

static ma_result ma_rb_data_source__on_unmap(ma_data_source* pDataSource, ma_uint64 frameCount)
{
    ma_rb_data_source* pRB = (ma_rb_data_source*)pDataSource;
    ma_result result;
    ma_uint32 bpf = ma_get_bytes_per_frame(pRB->format, pRB->channels);
    size_t sizeInBytes;

    sizeInBytes = (size_t)(frameCount * bpf);
    result = ma_rb_commit_read(pRB->pRB, sizeInBytes, pRB->pMappedBuffer);

    pRB->pMappedBuffer = NULL;

    return result;  /* We never actually return MA_AT_END here because a ring buffer doesn't have any notion of an end. */
}

static ma_result ma_rb_data_source__on_get_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    ma_rb_data_source* pRB = (ma_rb_data_source*)pDataSource;

    *pFormat     = pRB->format;
    *pChannels   = pRB->channels;
    *pSampleRate = 0;   /* No sample rate. */

    return MA_SUCCESS;
}

static ma_result ma_rb_data_source_init(ma_rb* pRB, ma_format format, ma_uint32 channels, ma_rb_data_source* pDataSource)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataSource);    /* For safety. */

    if (pRB == NULL) {
        return MA_INVALID_ARGS;
    }

    pDataSource->ds.onRead          = NULL;
    pDataSource->ds.onSeek          = NULL;  /* We can't really seek in a ring buffer - there's no notion of a beginning and an end in a ring buffer. */
    pDataSource->ds.onMap           = ma_rb_data_source__on_map;
    pDataSource->ds.onUnmap         = ma_rb_data_source__on_unmap;
    pDataSource->ds.onGetDataFormat = ma_rb_data_source__on_get_format;
    pDataSource->ds.onGetCursor     = NULL;
    pDataSource->ds.onGetLength     = NULL;
    pDataSource->pRB                = pRB;
    pDataSource->format             = format;
    pDataSource->channels           = channels;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_mix_rb(ma_mixer* pMixer, ma_rb* pRB, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, ma_uint64* pFrameCountOut, float volume, ma_effect* pEffect, ma_format formatIn, ma_uint32 channelsIn)
{
    /* Ring buffer mixing can be implemented in terms of a memory mapped data source. */
    ma_rb_data_source ds;
    ma_rb_data_source_init(pRB, formatIn, channelsIn, &ds); /* Will never fail and does not require an uninit() implementation. */

    return ma_mixer_mix_data_source(pMixer, &ds, offsetInFrames, frameCountIn, pFrameCountOut, volume, pEffect, MA_TRUE);   /* Ring buffers always loop, but the loop parameter will never actually be used because ma_rb_data_source__on_unmap() will never return MA_AT_END. */
}

MA_API ma_result ma_mixer_mix_pcm_rb(ma_mixer* pMixer, ma_pcm_rb* pRB, ma_uint64 offsetInFrames, ma_uint64 frameCountIn, ma_uint64* pFrameCountOut, float volume, ma_effect* pEffect)
{
    return ma_mixer_mix_rb(pMixer, &pRB->rb, offsetInFrames, frameCountIn, pFrameCountOut, volume, pEffect, pRB->format, pRB->channels);
}


MA_API ma_result ma_mixer_set_volume(ma_mixer* pMixer, float volume)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (volume < 0.0f || volume > 1.0f) {
        return MA_INVALID_ARGS;
    }

    pMixer->volume = volume;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_get_volume(ma_mixer* pMixer, float* pVolume)
{
    if (pVolume == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pMixer == NULL) {
        *pVolume = 0;
        return MA_INVALID_ARGS;
    }

    *pVolume = pMixer->volume;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_set_gain_db(ma_mixer* pMixer, float gainDB)
{
    if (gainDB > 0) {
        return MA_INVALID_ARGS;
    }

    return ma_mixer_set_volume(pMixer, ma_gain_db_to_factor(gainDB));
}

MA_API ma_result ma_mixer_get_gain_db(ma_mixer* pMixer, float* pGainDB)
{
    float factor;
    ma_result result;

    if (pGainDB == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_mixer_get_volume(pMixer, &factor);
    if (result != MA_SUCCESS) {
        *pGainDB = 0;
        return result;
    }

    *pGainDB = ma_factor_to_gain_db(factor);

    return MA_SUCCESS;
}


MA_API ma_result ma_mixer_set_effect(ma_mixer* pMixer, ma_effect* pEffect)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pMixer->pEffect == pEffect) {
        return MA_SUCCESS;  /* No-op. */
    }

    /* The effect cannot be changed if we're in the middle of a begin/end pair. */
    if (pMixer->mixingState.isInsideBeginEnd) {
        return MA_INVALID_OPERATION;
    }

    pMixer->pEffect = pEffect;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_get_effect(ma_mixer* pMixer, ma_effect** ppEffect)
{
    if (ppEffect == NULL) {
        return MA_INVALID_ARGS;
    }

    *ppEffect = NULL;   /* Safety. */

    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    *ppEffect = pMixer->pEffect;

    return MA_SUCCESS;
}

MA_API ma_result ma_mixer_get_output_data_format(ma_mixer* pMixer, ma_format* pFormat, ma_uint32* pChannels)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If we have an effect, the output data format will be the effect's output data format. */
    if (pMixer->pEffect != NULL) {
        return ma_effect_get_output_data_format(pMixer->pEffect, pFormat, pChannels, NULL);
    } else {
        if (pFormat != NULL) {
            *pFormat = pMixer->format;
        }

        if (pChannels != NULL) {
            *pChannels = pMixer->channels;
        }

        return MA_SUCCESS;
    }
}

MA_API ma_result ma_mixer_get_input_data_format(ma_mixer* pMixer, ma_format* pFormat, ma_uint32* pChannels)
{
    if (pMixer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pFormat != NULL) {
        *pFormat = pMixer->format;
    }

    if (pChannels != NULL) {
        *pChannels = pMixer->channels;
    }

    return MA_SUCCESS;
}





MA_API ma_result ma_slot_allocator_init(ma_slot_allocator* pAllocator)
{
    if (pAllocator == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pAllocator);

    return MA_SUCCESS;
}

MA_API ma_result ma_slot_allocator_alloc(ma_slot_allocator* pAllocator, ma_uint64* pSlot)
{
    ma_uint32 capacity;
    ma_uint32 iAttempt;
    const ma_uint32 maxAttempts = 2;    /* The number of iterations to perform until returning MA_OUT_OF_MEMORY if no slots can be found. */

    if (pAllocator == NULL || pSlot == NULL) {
        return MA_INVALID_ARGS;
    }

    capacity = ma_countof(pAllocator->groups) * 32;

    for (iAttempt = 0; iAttempt < maxAttempts; iAttempt += 1) {
        /* We need to acquire a suitable bitfield first. This is a bitfield that's got an available slot within it. */
        ma_uint32 iGroup;
        for (iGroup = 0; iGroup < ma_countof(pAllocator->groups); iGroup += 1) {
            /* CAS */
            for (;;) {
                ma_uint32 oldBitfield;
                ma_uint32 newBitfield;
                ma_uint32 bitOffset;

                oldBitfield = pAllocator->groups[iGroup].bitfield;  /* <-- This copy must happen. The compiler must not optimize this away. pAllocator->groups[iGroup].bitfield is marked as volatile. */

                /* Fast check to see if anything is available. */
                if (oldBitfield == 0xFFFFFFFF) {
                    break;  /* No available bits in this bitfield. */
                }

                bitOffset = ma_ffs_32(~oldBitfield);
                MA_ASSERT(bitOffset < 32);

                newBitfield = oldBitfield | (1 << bitOffset);

                if (c89atomic_compare_and_swap_32(&pAllocator->groups[iGroup].bitfield, oldBitfield, newBitfield) == oldBitfield) {
                    ma_uint32 slotIndex;

                    /* Increment the counter as soon as possible to have other threads report out-of-memory sooner than later. */
                    c89atomic_fetch_add_32(&pAllocator->count, 1);

                    /* The slot index is required for constructing the output value. */
                    slotIndex = (iGroup << 5) + bitOffset;  /* iGroup << 5 = iGroup * 32 */

                    /* Increment the reference count before constructing the output value. */
                    pAllocator->slots[slotIndex] += 1;  

                    /* Construct the output value. */
                    *pSlot = ((ma_uint64)pAllocator->slots[slotIndex] << 32 | slotIndex);

                    return MA_SUCCESS;
                }
            }
        }

        /* We weren't able to find a slot. If it's because we've reached our capacity we need to return MA_OUT_OF_MEMORY. Otherwise we need to do another iteration and try again. */
        if (pAllocator->count < capacity) {
            ma_yield();
        } else {
            return MA_OUT_OF_MEMORY;
        }
    }

    /* We couldn't find a slot within the maximum number of attempts. */
    return MA_OUT_OF_MEMORY;
}

MA_API ma_result ma_slot_allocator_free(ma_slot_allocator* pAllocator, ma_uint64 slot)
{
    ma_uint32 iGroup;
    ma_uint32 iBit;

    if (pAllocator == NULL) {
        return MA_INVALID_ARGS;
    }

    iGroup = (slot & 0xFFFFFFFF) >> 5;   /* slot / 32 */
    iBit   = (slot & 0xFFFFFFFF) & 31;   /* slot % 32 */

    if (iGroup >= ma_countof(pAllocator->groups)) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(iBit < 32);   /* This must be true due to the logic we used to actually calculate it. */

    while (pAllocator->count > 0) {
        /* CAS */
        ma_uint32 oldBitfield;
        ma_uint32 newBitfield;

        oldBitfield = pAllocator->groups[iGroup].bitfield;  /* <-- This copy must happen. The compiler must not optimize this away. pAllocator->groups[iGroup].bitfield is marked as volatile. */
        newBitfield = oldBitfield & ~(1 << iBit);

        if (c89atomic_compare_and_swap_32(&pAllocator->groups[iGroup].bitfield, oldBitfield, newBitfield) == oldBitfield) {
            c89atomic_fetch_sub_32(&pAllocator->count, 1);
            return MA_SUCCESS;
        }
    }

    /* Getting here means there are no allocations available for freeing. */
    return MA_INVALID_OPERATION;
}



MA_API ma_result ma_async_notification_signal(ma_async_notification* pNotification, int code)
{
    ma_async_notification_callbacks* pNotificationCallbacks = (ma_async_notification_callbacks*)pNotification;

    if (pNotification == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pNotificationCallbacks->onSignal == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    pNotificationCallbacks->onSignal(pNotification, code);
    return MA_INVALID_ARGS;
}


static void ma_async_notification_event__on_signal(ma_async_notification* pNotification, int code)
{
    if (code == MA_NOTIFICATION_COMPLETE || code == MA_NOTIFICATION_FAILED) {
        ma_async_notification_event_signal((ma_async_notification_event*)pNotification);
    }
}

MA_API ma_result ma_async_notification_event_init(ma_async_notification_event* pNotificationEvent)
{
    ma_result result;

    if (pNotificationEvent == NULL) {
        return MA_INVALID_ARGS;
    }

    pNotificationEvent->cb.onSignal = ma_async_notification_event__on_signal;
    
    result = ma_event_init(&pNotificationEvent->e);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_async_notification_event_uninit(ma_async_notification_event* pNotificationEvent)
{
    if (pNotificationEvent == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_event_uninit(&pNotificationEvent->e);
    return MA_SUCCESS;
}

MA_API ma_result ma_async_notification_event_wait(ma_async_notification_event* pNotificationEvent)
{
    if (pNotificationEvent == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_event_wait(&pNotificationEvent->e);
}

MA_API ma_result ma_async_notification_event_signal(ma_async_notification_event* pNotificationEvent)
{
    if (pNotificationEvent == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_event_signal(&pNotificationEvent->e);
}



#define MA_JOB_ID_NONE      ~((ma_uint64)0)
#define MA_JOB_SLOT_NONE    (ma_uint16)(~0)

static MA_INLINE ma_uint32 ma_job_extract_refcount(ma_uint64 toc)
{
    return (ma_uint32)(toc >> 32);
}

static MA_INLINE ma_uint16 ma_job_extract_slot(ma_uint64 toc)
{
    return (ma_uint16)(toc & 0x0000FFFF);
}

static MA_INLINE ma_uint16 ma_job_extract_code(ma_uint64 toc)
{
    return (ma_uint16)((toc & 0xFFFF0000) >> 16);
}

static MA_INLINE ma_uint64 ma_job_toc_to_allocation(ma_uint64 toc)
{
    return ((ma_uint64)ma_job_extract_refcount(toc) << 32) | (ma_uint64)ma_job_extract_slot(toc);
}


MA_API ma_job ma_job_init(ma_uint16 code)
{
    ma_job job;
    
    MA_ZERO_OBJECT(&job);
    job.toc.code = code;
    job.toc.slot = MA_JOB_SLOT_NONE;    /* Temp value. Will be allocated when posted to a queue. */
    job.next     = MA_JOB_ID_NONE;

    return job;
}


/*
Lock free queue implementation based on the paper by Michael and Scott: Nonblocking Algorithms and Preemption-Safe Locking on Multiprogrammed Shared Memory Multiprocessors
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
    if ((pQueue->flags & MA_JOB_QUEUE_FLAG_NON_BLOCKING) == 0) {
        ma_semaphore_init(0, &pQueue->sem);
    }

    /*
    Our queue needs to be initialized with a free standing node. This should always be slot 0. Required for the lock free algorithm. The first job in the queue is
    just a dummy item for giving us the first item in the list which is stored in the "next" member.
    */
    ma_slot_allocator_alloc(&pQueue->allocator, &pQueue->head);  /* Will never fail. */
    pQueue->jobs[ma_job_extract_slot(pQueue->head)].next = MA_JOB_ID_NONE;
    pQueue->tail = pQueue->head;

    return MA_SUCCESS;
}

MA_API ma_result ma_job_queue_uninit(ma_job_queue* pQueue)
{
    if (pQueue == NULL) {
        return MA_INVALID_ARGS;
    }

    /* All we need to do is uninitialize the semaphore. */
    if ((pQueue->flags & MA_JOB_QUEUE_FLAG_NON_BLOCKING) == 0) {
        ma_semaphore_uninit(&pQueue->sem);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_job_queue_post(ma_job_queue* pQueue, const ma_job* pJob)
{
    ma_result result;
    ma_uint64 slot;
    ma_uint64 tail;
    ma_uint64 next;

    if (pQueue == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We need a new slot. */
    result = ma_slot_allocator_alloc(&pQueue->allocator, &slot);
    if (result != MA_SUCCESS) {
        return result;  /* Probably ran out of slots. If so, MA_OUT_OF_MEMORY will be returned. */
    }

    /* At this point we should have a slot to place the job. */
    MA_ASSERT(ma_job_extract_slot(slot) < MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY);

    /* We need to put the job into memory before we do anything. */
    pQueue->jobs[ma_job_extract_slot(slot)]                = *pJob;
    pQueue->jobs[ma_job_extract_slot(slot)].toc.allocation = slot;           /* This will overwrite the job code. */
    pQueue->jobs[ma_job_extract_slot(slot)].toc.code       = pJob->toc.code; /* The job code needs to be applied again because the line above overwrote it. */
    pQueue->jobs[ma_job_extract_slot(slot)].next           = MA_JOB_ID_NONE; /* Reset for safety. */

    /* The job is stored in memory so now we need to add it to our linked list. We only ever add items to the end of the list. */
    for (;;) {
        tail = pQueue->tail;
        next = pQueue->jobs[ma_job_extract_slot(tail)].next;

        if (ma_job_toc_to_allocation(tail) == ma_job_toc_to_allocation(pQueue->tail)) {
            if (ma_job_extract_slot(next) == 0xFFFF) {
                if (c89atomic_compare_and_swap_64(&pQueue->jobs[ma_job_extract_slot(tail)].next, next, slot) == next) {
                    break;
                }
            } else {
                c89atomic_compare_and_swap_64(&pQueue->tail, tail, next);
            }
        }
    }
    c89atomic_compare_and_swap_64(&pQueue->tail, tail, slot);


    /* Signal the semaphore as the last step if we're using synchronous mode. */
    if ((pQueue->flags & MA_JOB_QUEUE_FLAG_NON_BLOCKING) == 0) {
        ma_semaphore_release(&pQueue->sem);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_job_queue_next(ma_job_queue* pQueue, ma_job* pJob)
{
    ma_uint64 head;
    ma_uint64 tail;
    ma_uint64 next;

    if (pQueue == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If we're running in synchronous mode we'll need to wait on a semaphore. */
    if ((pQueue->flags & MA_JOB_QUEUE_FLAG_NON_BLOCKING) == 0) {
        ma_semaphore_wait(&pQueue->sem);
    }

    /* Now we need to remove the root item from the list. This must be done without locking. */
    for (;;) {
        head = pQueue->head;
        tail = pQueue->tail;
        next = pQueue->jobs[ma_job_extract_slot(head)].next;

        if (ma_job_toc_to_allocation(head) == ma_job_toc_to_allocation(pQueue->head)) {
            if (ma_job_toc_to_allocation(head) == ma_job_toc_to_allocation(tail)) {
                if (ma_job_extract_slot(next) == 0xFFFF) {
                    return MA_NO_DATA_AVAILABLE;
                }
                c89atomic_compare_and_swap_64(&pQueue->tail, tail, next);
            } else {
                *pJob = pQueue->jobs[ma_job_extract_slot(next)];
                if (c89atomic_compare_and_swap_64(&pQueue->head, head, next) == head) {
                    break;
                }
            }
        }
    }

    ma_slot_allocator_free(&pQueue->allocator, head);

    /*
    If it's a quit job make sure it's put back on the queue to ensure other threads have an opportunity to detect it and terminate naturally. We
    could instead just leave it on the queue, but that would involve fiddling with the lock-free code above and I want to keep that as simple as
    possible.
    */
    if (pJob->toc.code == MA_JOB_QUIT) {
        ma_job_queue_post(pQueue, pJob);
        return MA_CANCELLED;    /* Return a cancelled status just in case the thread is checking return codes and not properly checking for a quit job. */
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_job_queue_free(ma_job_queue* pQueue, ma_job* pJob)
{
    if (pQueue == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_slot_allocator_free(&pQueue->allocator, ma_job_toc_to_allocation(pJob->toc.allocation));
}





#ifndef MA_DEFAULT_HASH_SEED
#define MA_DEFAULT_HASH_SEED    42
#endif

/* MurmurHash3. Based on code from https://github.com/PeterScott/murmur3/blob/master/murmur3.c (public domain). */
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

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

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic push
#endif
/* End MurmurHash3 */

static ma_uint32 ma_hash_string_32(const char* str)
{
    return ma_hash_32(str, (int)strlen(str), MA_DEFAULT_HASH_SEED);
}



/*
Basic BST Functions
*/
static ma_result ma_resource_manager_data_buffer_node_search(ma_resource_manager* pResourceManager, ma_uint32 hashedName32, ma_resource_manager_data_buffer_node** ppDataBufferNode)
{
    ma_resource_manager_data_buffer_node* pCurrentNode;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(ppDataBufferNode != NULL);

    pCurrentNode = pResourceManager->pRootDataBufferNode;
    while (pCurrentNode != NULL) {
        if (hashedName32 == pCurrentNode->hashedName32) {
            break;  /* Found. */
        } else if (hashedName32 < pCurrentNode->hashedName32) {
            pCurrentNode = pCurrentNode->pChildLo;
        } else {
            pCurrentNode = pCurrentNode->pChildHi;
        }
    }

    *ppDataBufferNode = pCurrentNode;

    if (pCurrentNode == NULL) {
        return MA_DOES_NOT_EXIST;
    } else {
        return MA_SUCCESS;
    }
}

static ma_result ma_resource_manager_data_buffer_node_insert_point(ma_resource_manager* pResourceManager, ma_uint32 hashedName32, ma_resource_manager_data_buffer_node** ppInsertPoint)
{
    ma_result result = MA_SUCCESS;
    ma_resource_manager_data_buffer_node* pCurrentNode;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(ppInsertPoint    != NULL);

    *ppInsertPoint = NULL;

    if (pResourceManager->pRootDataBufferNode == NULL) {
        return MA_SUCCESS;  /* No items. */
    }

    /* We need to find the node that will become the parent of the new node. If a node is found that already has the same hashed name we need to return MA_ALREADY_EXISTS. */
    pCurrentNode = pResourceManager->pRootDataBufferNode;
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

static ma_result ma_resource_manager_data_buffer_node_insert_at(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode, ma_resource_manager_data_buffer_node* pInsertPoint)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);

    /* The key must have been set before calling this function. */
    MA_ASSERT(pDataBufferNode->hashedName32 != 0);

    if (pInsertPoint == NULL) {
        /* It's the first node. */
        pResourceManager->pRootDataBufferNode = pDataBufferNode;
    } else {
        /* It's not the first node. It needs to be inserted. */
        if (pDataBufferNode->hashedName32 < pInsertPoint->hashedName32) {
            MA_ASSERT(pInsertPoint->pChildLo == NULL);
            pInsertPoint->pChildLo = pDataBufferNode;
        } else {
            MA_ASSERT(pInsertPoint->pChildHi == NULL);
            pInsertPoint->pChildHi = pDataBufferNode;
        }
    }

    pDataBufferNode->pParent = pInsertPoint;

    return MA_SUCCESS;
}

#if 0   /* Unused for now. */
static ma_result ma_resource_manager_data_buffer_node_insert(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    ma_result result;
    ma_resource_manager_data_buffer_node* pInsertPoint;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);

    result = ma_resource_manager_data_buffer_node_insert_point(pResourceManager, pDataBufferNode->hashedName32, &pInsertPoint);
    if (result != MA_SUCCESS) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_data_buffer_node_insert_at(pResourceManager, pDataBufferNode, pInsertPoint);
}
#endif

static MA_INLINE ma_resource_manager_data_buffer_node* ma_resource_manager_data_buffer_node_find_min(ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    ma_resource_manager_data_buffer_node* pCurrentNode;

    MA_ASSERT(pDataBufferNode != NULL);

    pCurrentNode = pDataBufferNode;
    while (pCurrentNode->pChildLo != NULL) {
        pCurrentNode = pCurrentNode->pChildLo;
    }

    return pCurrentNode;
}

static MA_INLINE ma_resource_manager_data_buffer_node* ma_resource_manager_data_buffer_node_find_max(ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    ma_resource_manager_data_buffer_node* pCurrentNode;

    MA_ASSERT(pDataBufferNode != NULL);

    pCurrentNode = pDataBufferNode;
    while (pCurrentNode->pChildHi != NULL) {
        pCurrentNode = pCurrentNode->pChildHi;
    }

    return pCurrentNode;
}

static MA_INLINE ma_resource_manager_data_buffer_node* ma_resource_manager_data_buffer_node_find_inorder_successor(ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    MA_ASSERT(pDataBufferNode           != NULL);
    MA_ASSERT(pDataBufferNode->pChildHi != NULL);

    return ma_resource_manager_data_buffer_node_find_min(pDataBufferNode->pChildHi);
}

static MA_INLINE ma_resource_manager_data_buffer_node* ma_resource_manager_data_buffer_node_find_inorder_predecessor(ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    MA_ASSERT(pDataBufferNode           != NULL);
    MA_ASSERT(pDataBufferNode->pChildLo != NULL);

    return ma_resource_manager_data_buffer_node_find_max(pDataBufferNode->pChildLo);
}

static ma_result ma_resource_manager_data_buffer_node_remove(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);

    if (pDataBufferNode->pChildLo == NULL) {
        if (pDataBufferNode->pChildHi == NULL) {
            /* Simple case - deleting a buffer with no children. */
            if (pDataBufferNode->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataBufferNode == pDataBufferNode);    /* There is only a single buffer in the tree which should be equal to the root node. */
                pResourceManager->pRootDataBufferNode = NULL;
            } else {
                if (pDataBufferNode->pParent->pChildLo == pDataBufferNode) {
                    pDataBufferNode->pParent->pChildLo = NULL;
                } else {
                    pDataBufferNode->pParent->pChildHi = NULL;
                }
            }
        } else {
            /* Node has one child - pChildHi != NULL. */
            pDataBufferNode->pChildHi->pParent = pDataBufferNode->pParent;

            if (pDataBufferNode->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataBufferNode == pDataBufferNode);
                pResourceManager->pRootDataBufferNode = pDataBufferNode->pChildHi;
            } else {
                if (pDataBufferNode->pParent->pChildLo == pDataBufferNode) {
                    pDataBufferNode->pParent->pChildLo = pDataBufferNode->pChildHi;
                } else {
                    pDataBufferNode->pParent->pChildHi = pDataBufferNode->pChildHi;
                }
            }
        }
    } else {
        if (pDataBufferNode->pChildHi == NULL) {
            /* Node has one child - pChildLo != NULL. */
            pDataBufferNode->pChildLo->pParent = pDataBufferNode->pParent;

            if (pDataBufferNode->pParent == NULL) {
                MA_ASSERT(pResourceManager->pRootDataBufferNode == pDataBufferNode);
                pResourceManager->pRootDataBufferNode = pDataBufferNode->pChildLo;
            } else {
                if (pDataBufferNode->pParent->pChildLo == pDataBufferNode) {
                    pDataBufferNode->pParent->pChildLo = pDataBufferNode->pChildLo;
                } else {
                    pDataBufferNode->pParent->pChildHi = pDataBufferNode->pChildLo;
                }
            }
        } else {
            /* Complex case - deleting a node with two children. */
            ma_resource_manager_data_buffer_node* pReplacementDataBufferNode;

            /* For now we are just going to use the in-order successor as the replacement, but we may want to try to keep this balanced by switching between the two. */
            pReplacementDataBufferNode = ma_resource_manager_data_buffer_node_find_inorder_successor(pDataBufferNode);
            MA_ASSERT(pReplacementDataBufferNode != NULL);

            /*
            Now that we have our replacement node we can make the change. The simple way to do this would be to just exchange the values, and then remove the replacement
            node, however we track specific nodes via pointers which means we can't just swap out the values. We need to instead just change the pointers around. The
            replacement node should have at most 1 child. Therefore, we can detach it in terms of our simpler cases above. What we're essentially doing is detaching the
            replacement node and reinserting it into the same position as the deleted node.
            */
            MA_ASSERT(pReplacementDataBufferNode->pParent  != NULL);  /* The replacement node should never be the root which means it should always have a parent. */
            MA_ASSERT(pReplacementDataBufferNode->pChildLo == NULL);  /* Because we used in-order successor. This would be pChildHi == NULL if we used in-order predecessor. */

            if (pReplacementDataBufferNode->pChildHi == NULL) {
                if (pReplacementDataBufferNode->pParent->pChildLo == pReplacementDataBufferNode) {
                    pReplacementDataBufferNode->pParent->pChildLo = NULL;
                } else {
                    pReplacementDataBufferNode->pParent->pChildHi = NULL;
                }
            } else {
                if (pReplacementDataBufferNode->pParent->pChildLo == pReplacementDataBufferNode) {
                    pReplacementDataBufferNode->pParent->pChildLo = pReplacementDataBufferNode->pChildHi;
                } else {
                    pReplacementDataBufferNode->pParent->pChildHi = pReplacementDataBufferNode->pChildHi;
                }
            }


            /* The replacement node has essentially been detached from the binary tree, so now we need to replace the old data buffer with it. The first thing to update is the parent */
            if (pDataBufferNode->pParent != NULL) {
                if (pDataBufferNode->pParent->pChildLo == pDataBufferNode) {
                    pDataBufferNode->pParent->pChildLo = pReplacementDataBufferNode;
                } else {
                    pDataBufferNode->pParent->pChildHi = pReplacementDataBufferNode;
                }
            }

            /* Now need to update the replacement node's pointers. */
            pReplacementDataBufferNode->pParent  = pDataBufferNode->pParent;
            pReplacementDataBufferNode->pChildLo = pDataBufferNode->pChildLo;
            pReplacementDataBufferNode->pChildHi = pDataBufferNode->pChildHi;

            /* Now the children of the replacement node need to have their parent pointers updated. */
            if (pReplacementDataBufferNode->pChildLo != NULL) {
                pReplacementDataBufferNode->pChildLo->pParent = pReplacementDataBufferNode;
            }
            if (pReplacementDataBufferNode->pChildHi != NULL) {
                pReplacementDataBufferNode->pChildHi->pParent = pReplacementDataBufferNode;
            }

            /* Now the root node needs to be updated. */
            if (pResourceManager->pRootDataBufferNode == pDataBufferNode) {
                pResourceManager->pRootDataBufferNode = pReplacementDataBufferNode;
            }
        }
    }

    return MA_SUCCESS;
}

#if 0   /* Unused for now. */
static ma_result ma_resource_manager_data_buffer_node_remove_by_key(ma_resource_manager* pResourceManager, ma_uint32 hashedName32)
{
    ma_result result;
    ma_resource_manager_data_buffer_node* pDataBufferNode;

    result = ma_resource_manager_data_buffer_search(pResourceManager, hashedName32, &pDataBufferNode);
    if (result != MA_SUCCESS) {
        return result;  /* Could not find the data buffer. */
    }

    return ma_resource_manager_data_buffer_remove(pResourceManager, pDataBufferNode);
}
#endif

static ma_result ma_resource_manager_data_buffer_node_increment_ref(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode, ma_uint32* pNewRefCount)
{
    ma_uint32 refCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);

    (void)pResourceManager;

    refCount = c89atomic_fetch_add_32(&pDataBufferNode->refCount, 1) + 1;

    if (pNewRefCount != NULL) {
        *pNewRefCount = refCount;
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_buffer_node_decrement_ref(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode, ma_uint32* pNewRefCount)
{
    ma_uint32 refCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);

    (void)pResourceManager;

    refCount = c89atomic_fetch_sub_32(&pDataBufferNode->refCount, 1) - 1;

    if (pNewRefCount != NULL) {
        *pNewRefCount = refCount;
    }

    return MA_SUCCESS;
}

static void ma_resource_manager_data_buffer_node_free(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);

    if (pDataBufferNode->isDataOwnedByResourceManager) {
        if (pDataBufferNode->data.type == ma_resource_manager_data_buffer_encoding_encoded) {
            ma__free_from_callbacks((void*)pDataBufferNode->data.encoded.pData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_ENCODED_BUFFER*/);
            pDataBufferNode->data.encoded.pData       = NULL;
            pDataBufferNode->data.encoded.sizeInBytes = 0;
        } else {
            ma__free_from_callbacks((void*)pDataBufferNode->data.decoded.pData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
            pDataBufferNode->data.decoded.pData       = NULL;
            pDataBufferNode->data.decoded.frameCount  = 0;
        }
    }

    /* The data buffer itself needs to be freed. */
    ma__free_from_callbacks(pDataBufferNode, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
}

static ma_result ma_resource_manager_data_buffer_node_result(const ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    MA_ASSERT(pDataBufferNode != NULL);

    return c89atomic_load_i32((ma_result*)&pDataBufferNode->result);    /* Need a naughty const-cast here. */
}


static ma_thread_result MA_THREADCALL ma_resource_manager_job_thread(void* pUserData)
{
    ma_resource_manager* pResourceManager = (ma_resource_manager*)pUserData;
    MA_ASSERT(pResourceManager != NULL);

    for (;;) {
        ma_result result;
        ma_job job;

        result = ma_resource_manager_next_job(pResourceManager, &job);
        if (result != MA_SUCCESS) {
            break;
        }

        /* Terminate if we got a quit message. */
        if (job.toc.code == MA_JOB_QUIT) {
            break;
        }

        ma_resource_manager_process_job(pResourceManager, &job);
    }

    return (ma_thread_result)0;
}


MA_API ma_resource_manager_config ma_resource_manager_config_init()
{
    ma_resource_manager_config config;

    MA_ZERO_OBJECT(&config);
    config.decodedFormat     = ma_format_unknown;
    config.decodedChannels   = 0;
    config.decodedSampleRate = 0;
    config.jobThreadCount    = 1;   /* A single miniaudio-managed job thread by default. */

    return config;
}


MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager)
{
    ma_result result;
    ma_uint32 jobQueueFlags;
    ma_uint32 iJobThread;

    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pResourceManager);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->jobThreadCount > ma_countof(pResourceManager->jobThreads)) {
        return MA_INVALID_ARGS; /* Requesting too many job threads. */
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

    /* Job queue. */
    jobQueueFlags = 0;
    if ((pConfig->flags & MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING) != 0) {
        if (pConfig->jobThreadCount > 0) {
            return MA_INVALID_ARGS; /* Non-blocking mode is only valid for self-managed job threads. */
        }

        jobQueueFlags |= MA_JOB_QUEUE_FLAG_NON_BLOCKING;
    }

    result = ma_job_queue_init(jobQueueFlags, &pResourceManager->jobQueue);
    if (result != MA_SUCCESS) {
        ma_mutex_uninit(&pResourceManager->dataBufferLock);
        return result;
    }


    /* Data buffer lock. */
    result = ma_mutex_init(&pResourceManager->dataBufferLock);
    if (result != MA_SUCCESS) {
        return result;
    }


    /* Create the job threads last to ensure the threads has access to valid data. */
    for (iJobThread = 0; iJobThread < pConfig->jobThreadCount; iJobThread += 1) {
        result = ma_thread_create(&pResourceManager->jobThreads[iJobThread], ma_thread_priority_normal, 0, ma_resource_manager_job_thread, pResourceManager);
        if (result != MA_SUCCESS) {
            ma_mutex_uninit(&pResourceManager->dataBufferLock);
            ma_job_queue_uninit(&pResourceManager->jobQueue);
            return result;
        }
    }

    return MA_SUCCESS;
}


static void ma_resource_manager_delete_all_data_buffer_nodes(ma_resource_manager* pResourceManager)
{
    MA_ASSERT(pResourceManager);

    /* If everything was done properly, there shouldn't be any active data buffers. */
    while (pResourceManager->pRootDataBufferNode != NULL) {
        ma_resource_manager_data_buffer_node* pDataBufferNode = pResourceManager->pRootDataBufferNode;
        ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBufferNode);

        /* The data buffer has been removed from the BST, so now we need to free it's data. */
        ma_resource_manager_data_buffer_node_free(pResourceManager, pDataBufferNode);
    }
}

MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager)
{
    ma_uint32 iJobThread;

    if (pResourceManager == NULL) {
        return;
    }

    /*
    Job threads need to be killed first. To do this we need to post a quit message to the message queue and then wait for the thread. The quit message will never be removed from the
    queue which means it will never not be returned after being encounted for the first time which means all threads will eventually receive it.
    */
    ma_resource_manager_post_job_quit(pResourceManager);

    /* Wait for every job to finish before continuing to ensure nothing is sill trying to access any of our objects below. */
    for (iJobThread = 0; iJobThread < pResourceManager->config.jobThreadCount; iJobThread += 1) {
        ma_thread_wait(&pResourceManager->jobThreads[iJobThread]);
    }

    /* At this point the thread should have returned and no other thread should be accessing our data. We can now delete all data buffers. */
    ma_resource_manager_delete_all_data_buffer_nodes(pResourceManager);

    /* The job queue is no longer needed. */
    ma_job_queue_uninit(&pResourceManager->jobQueue);

    /* We're no longer doing anything with data buffers so the lock can now be uninitialized. */
    ma_mutex_uninit(&pResourceManager->dataBufferLock);
}


static ma_result ma_resource_manager__init_decoder(ma_resource_manager* pResourceManager, const char* pFilePath, ma_decoder* pDecoder)
{
    ma_decoder_config config;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pFilePath        != NULL);
    MA_ASSERT(pDecoder         != NULL);

    config = ma_decoder_config_init(pResourceManager->config.decodedFormat, pResourceManager->config.decodedChannels, pResourceManager->config.decodedSampleRate);
    config.allocationCallbacks = pResourceManager->config.allocationCallbacks;

    return ma_decoder_init_vfs(pResourceManager->config.pVFS, pFilePath, &config, pDecoder);
}

static ma_result ma_resource_manager_data_buffer_init_connector(ma_resource_manager_data_buffer* pDataBuffer, ma_async_notification* pNotification)
{
    ma_result result;

    MA_ASSERT(pDataBuffer != NULL);

    /* The underlying data buffer must be initialized before we'll be able to know how to initialize the backend. */
    result = ma_resource_manager_data_buffer_result(pDataBuffer);
    if (result != MA_SUCCESS && result != MA_BUSY) {
        return result;  /* The data buffer is in an erroneous state. */
    }


    /*
    We need to initialize either a ma_decoder or an ma_audio_buffer depending on whether or not the backing data is encoded or decoded. These act as the
    "instance" to the data and are used to form the connection between underlying data buffer and the data source. If the data buffer is decoded, we can use
    an ma_audio_buffer. This enables us to use memory mapping when mixing which saves us a bit of data movement overhead.
    */
    if (pDataBuffer->pNode->data.type == ma_resource_manager_data_buffer_encoding_decoded) {
        pDataBuffer->connectorType = ma_resource_manager_data_buffer_connector_buffer;
    } else {
        pDataBuffer->connectorType = ma_resource_manager_data_buffer_connector_decoder;
    }

    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_buffer) {
        ma_audio_buffer_config config;
        config = ma_audio_buffer_config_init(pDataBuffer->pNode->data.decoded.format, pDataBuffer->pNode->data.decoded.channels, pDataBuffer->pNode->data.decoded.frameCount, pDataBuffer->pNode->data.encoded.pData, NULL);
        result = ma_audio_buffer_init(&config, &pDataBuffer->connector.buffer);

        pDataBuffer->lengthInPCMFrames = pDataBuffer->connector.buffer.sizeInFrames;
    } else {
        ma_decoder_config configOut;
        configOut = ma_decoder_config_init(pDataBuffer->pResourceManager->config.decodedFormat, pDataBuffer->pResourceManager->config.decodedChannels, pDataBuffer->pResourceManager->config.decodedSampleRate);

        if (pDataBuffer->pNode->data.type == ma_resource_manager_data_buffer_encoding_decoded) {
            ma_decoder_config configIn;
            ma_uint64 sizeInBytes;

            configIn  = ma_decoder_config_init(pDataBuffer->pNode->data.decoded.format, pDataBuffer->pNode->data.decoded.channels, pDataBuffer->pNode->data.decoded.sampleRate);

            sizeInBytes = pDataBuffer->pNode->data.decoded.frameCount * ma_get_bytes_per_frame(configIn.format, configIn.channels);
            if (sizeInBytes > MA_SIZE_MAX) {
                result = MA_TOO_BIG;
            } else {
                result = ma_decoder_init_memory_raw(pDataBuffer->pNode->data.decoded.pData, (size_t)sizeInBytes, &configIn, &configOut, &pDataBuffer->connector.decoder);  /* Safe cast thanks to the check above. */
            }

            /*
            We will know the length for decoded sounds. Don't use ma_decoder_get_length_in_pcm_frames() as it may return 0 for sounds where the length
            is not known until it has been fully decoded which we've just done at a higher level.
            */
            pDataBuffer->lengthInPCMFrames = pDataBuffer->pNode->data.decoded.frameCount;
        } else {
            configOut.allocationCallbacks = pDataBuffer->pResourceManager->config.allocationCallbacks;
            result = ma_decoder_init_memory(pDataBuffer->pNode->data.encoded.pData, pDataBuffer->pNode->data.encoded.sizeInBytes, &configOut, &pDataBuffer->connector.decoder);

            /* Our only option is to use ma_decoder_get_length_in_pcm_frames() when loading from an encoded data source. */
            pDataBuffer->lengthInPCMFrames = ma_decoder_get_length_in_pcm_frames(&pDataBuffer->connector.decoder);
        }
    }

    /*
    We can only do mapping if the data source's backend is an audio buffer. If it's not, clear out the callbacks thereby preventing the mixer from attempting
    memory map mode, only to fail.
    */
    if (pDataBuffer->connectorType != ma_resource_manager_data_buffer_connector_buffer) {
        pDataBuffer->ds.onMap   = NULL;
        pDataBuffer->ds.onUnmap = NULL;
    }

    /*
    Initialization of the connector is when we can fire the MA_NOTIFICATION_INIT notification. This will give the application access to
    the format/channels/rate of the data source.
    */
    if (result == MA_SUCCESS) {
        if (pNotification != NULL) {
            ma_async_notification_signal(pNotification, MA_NOTIFICATION_INIT);
        }
    }
    
    /* At this point the backend should be initialized. We do *not* want to set pDataSource->result here - that needs to be done at a higher level to ensure it's done as the last step. */
    return result;
}

static ma_result ma_resource_manager_data_buffer_uninit_connector(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_decoder) {
        ma_decoder_uninit(&pDataBuffer->connector.decoder);
    } else {
        ma_audio_buffer_uninit(&pDataBuffer->connector.buffer);
    }

    return MA_SUCCESS;
}

static ma_uint32 ma_resource_manager_data_buffer_next_execution_order(ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pDataBuffer != NULL);
    return c89atomic_fetch_add_32(&pDataBuffer->pNode->executionCounter, 1);
}

static ma_bool32 ma_resource_manager_data_buffer_is_busy(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64 requiredFrameCount)
{
    /*
    Here is where we determine whether or not we need to return MA_BUSY from a data source callback. If we don't have enough data loaded to output all requiredFrameCount frames
    we will abort with MA_BUSY. We could also choose to do a partial read (only reading as many frames are available), but it's just easier to abort early and I don't think it
    really makes much practical difference. This only applies to decoded buffers.
    */
    if (pDataBuffer->pNode->data.type == ma_resource_manager_data_buffer_encoding_decoded) {
        ma_uint64 availableFrames;

        /* If the sound has been fully loaded then we'll never be busy. */
        if (pDataBuffer->pNode->data.decoded.decodedFrameCount == pDataBuffer->pNode->data.decoded.frameCount) {
            return MA_FALSE;    /* The sound is fully loaded. The buffer will never be busy. */
        }

        if (ma_resource_manager_data_buffer_get_available_frames(pDataBuffer, &availableFrames) == MA_SUCCESS) {
            return availableFrames < requiredFrameCount;
        }
    }

    return MA_FALSE;
}

static ma_data_source* ma_resource_manager_data_buffer_get_connector(ma_resource_manager_data_buffer* pDataBuffer)
{
    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_buffer) {
        return &pDataBuffer->connector.buffer;
    } else {
        return &pDataBuffer->connector.decoder;
    }
}


static ma_result ma_resource_manager_data_buffer_cb__read_pcm_frames(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_resource_manager_data_buffer_read_pcm_frames((ma_resource_manager_data_buffer*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_resource_manager_data_buffer_cb__seek_to_pcm_frame(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_resource_manager_data_buffer_seek_to_pcm_frame((ma_resource_manager_data_buffer*)pDataSource, frameIndex);
}

static ma_result ma_resource_manager_data_buffer_cb__map(ma_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount)
{
    return ma_resource_manager_data_buffer_map((ma_resource_manager_data_buffer*)pDataSource, ppFramesOut, pFrameCount);
}

static ma_result ma_resource_manager_data_buffer_cb__unmap(ma_data_source* pDataSource, ma_uint64 frameCount)
{
    return ma_resource_manager_data_buffer_unmap((ma_resource_manager_data_buffer*)pDataSource, frameCount);
}

static ma_result ma_resource_manager_data_buffer_cb__get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    return ma_resource_manager_data_buffer_get_data_format((ma_resource_manager_data_buffer*)pDataSource, pFormat, pChannels, pSampleRate);
}

static ma_result ma_resource_manager_data_buffer_cb__get_cursor_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_resource_manager_data_buffer_get_cursor_in_pcm_frames((ma_resource_manager_data_buffer*)pDataSource, pCursor);
}

static ma_result ma_resource_manager_data_buffer_cb__get_length_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_resource_manager_data_buffer_get_length_in_pcm_frames((ma_resource_manager_data_buffer*)pDataSource, pLength);
}

static ma_result ma_resource_manager_data_buffer_init_nolock(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 hashedName32, ma_uint32 flags, ma_async_notification* pNotification, ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_result result;
    ma_resource_manager_data_buffer_node* pInsertPoint;
    char* pFilePathCopy;    /* Allocated here, freed in the job thread. */
    ma_resource_manager_data_buffer_encoding dataBufferType;
    ma_bool32 async;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pFilePath        != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    MA_ZERO_OBJECT(pDataBuffer);
    pDataBuffer->ds.onRead          = ma_resource_manager_data_buffer_cb__read_pcm_frames;
    pDataBuffer->ds.onSeek          = ma_resource_manager_data_buffer_cb__seek_to_pcm_frame;
    pDataBuffer->ds.onMap           = ma_resource_manager_data_buffer_cb__map;
    pDataBuffer->ds.onUnmap         = ma_resource_manager_data_buffer_cb__unmap;
    pDataBuffer->ds.onGetDataFormat = ma_resource_manager_data_buffer_cb__get_data_format;
    pDataBuffer->ds.onGetCursor     = ma_resource_manager_data_buffer_cb__get_cursor_in_pcm_frames;
    pDataBuffer->ds.onGetLength     = ma_resource_manager_data_buffer_cb__get_length_in_pcm_frames;

    pDataBuffer->pResourceManager   = pResourceManager;
    pDataBuffer->flags              = flags;

    /* The backend type hasn't been determined yet - that happens when it's initialized properly by the job thread. */
    pDataBuffer->connectorType = ma_resource_manager_data_buffer_connector_unknown;

    /* The encoding of the data buffer is taken from the flags. */
    if ((flags & MA_DATA_SOURCE_FLAG_DECODE) != 0) {
        dataBufferType = ma_resource_manager_data_buffer_encoding_decoded;
    } else {
        dataBufferType = ma_resource_manager_data_buffer_encoding_encoded;
    }

    /* The data buffer needs to be loaded by the calling thread if we're in synchronous mode. */
    async = (flags & MA_DATA_SOURCE_FLAG_ASYNC) != 0;

    /*
    The first thing to do is find the insertion point. If it's already loaded it means we can just increment the reference counter and signal the event. Otherwise we
    need to do a full load.
    */
    result = ma_resource_manager_data_buffer_node_insert_point(pResourceManager, hashedName32, &pInsertPoint);
    if (result == MA_ALREADY_EXISTS) {
        /* Fast path. The data buffer already exists. We just need to increment the reference counter and signal the event, if any. */
        pDataBuffer->pNode = pInsertPoint;

        result = ma_resource_manager_data_buffer_node_increment_ref(pResourceManager, pDataBuffer->pNode, NULL);
        if (result != MA_SUCCESS) {
            return result;  /* Should never happen. Failed to increment the reference count. */
        }

        /* The existing node may be in the middle of loading. We need to wait for the node to finish loading before going any further. */
        /* TODO: This needs to be improved so that when loading asynchronously we post a message to the job queue instead of just waiting. */
        while (ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) == MA_BUSY) {
            ma_yield();
        }

        result = ma_resource_manager_data_buffer_init_connector(pDataBuffer, pNotification);
        if (result != MA_SUCCESS) {
            ma_resource_manager_data_buffer_node_free(pDataBuffer->pResourceManager, pDataBuffer->pNode);
            return result;
        }

        if (pNotification != NULL) {
            ma_async_notification_signal(pNotification, MA_NOTIFICATION_COMPLETE);
        }
    } else {
        /* Slow path. The data for this buffer has not yet been initialized. The first thing to do is allocate the new data buffer and insert it into the BST. */
        pDataBuffer->pNode = (ma_resource_manager_data_buffer_node*)ma__malloc_from_callbacks(sizeof(*pDataBuffer->pNode), &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
        if (pDataBuffer->pNode == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        MA_ZERO_OBJECT(pDataBuffer->pNode);
        pDataBuffer->pNode->hashedName32 = hashedName32;
        pDataBuffer->pNode->refCount     = 1;        /* Always set to 1 by default (this is our first reference). */
        pDataBuffer->pNode->data.type    = dataBufferType;
        pDataBuffer->pNode->result       = MA_BUSY;  /* I think it's good practice to set the status to MA_BUSY by default. */

        result = ma_resource_manager_data_buffer_node_insert_at(pResourceManager, pDataBuffer->pNode, pInsertPoint);
        if (result != MA_SUCCESS) {
            return result;  /* Should never happen. Failed to insert the data buffer into the BST. */
        }

        /*
        The new data buffer has been inserted into the BST. We now need to load the data. If we are loading synchronously we need to load
        everything from the calling thread because we may be in a situation where there are no job threads running and therefore the data
        will never get loaded. If we are loading asynchronously, we can assume at least one job thread exists and we can do everything
        from there.
        */
        pDataBuffer->pNode->isDataOwnedByResourceManager = MA_TRUE;
        pDataBuffer->pNode->result = MA_BUSY;

        if (async) {
            /* Asynchronous. Post to the job thread. */
            ma_job job;

            /* We need a copy of the file path. We should probably make this more efficient, but for now we'll do a transient memory allocation. */
            pFilePathCopy = ma_copy_string(pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
            if (pFilePathCopy == NULL) {
                if (pNotification != NULL) {
                    ma_async_notification_signal(pNotification, MA_NOTIFICATION_COMPLETE);
                }

                ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBuffer->pNode);
                ma__free_from_callbacks(pDataBuffer->pNode, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
                return MA_OUT_OF_MEMORY;
            }

            /* We now have everything we need to post the job to the job thread. */
            job = ma_job_init(MA_JOB_LOAD_DATA_BUFFER);
            job.order = ma_resource_manager_data_buffer_next_execution_order(pDataBuffer);
            job.loadDataBuffer.pDataBuffer   = pDataBuffer;
            job.loadDataBuffer.pFilePath     = pFilePathCopy;
            job.loadDataBuffer.pNotification = pNotification;
            result = ma_resource_manager_post_job(pResourceManager, &job);
            if (result != MA_SUCCESS) {
                /* Failed to post the job to the queue. Probably ran out of space. */
                if (pNotification != NULL) {
                    ma_async_notification_signal(pNotification, MA_NOTIFICATION_COMPLETE);
                }

                ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBuffer->pNode);
                ma__free_from_callbacks(pDataBuffer->pNode, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
                ma__free_from_callbacks(pFilePathCopy,      &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
                return result;
            }
        } else {
            /* Synchronous. Do everything here. */
            if (pDataBuffer->pNode->data.type == ma_resource_manager_data_buffer_encoding_encoded) {
                /* No decoding. Just store the file contents in memory. */
                void* pData;
                size_t sizeInBytes;
                result = ma_vfs_open_and_read_file_ex(pResourceManager->config.pVFS, pFilePath, &pData, &sizeInBytes, &pResourceManager->config.allocationCallbacks, MA_ALLOCATION_TYPE_ENCODED_BUFFER);
                if (result == MA_SUCCESS) {
                    pDataBuffer->pNode->data.encoded.pData       = pData;
                    pDataBuffer->pNode->data.encoded.sizeInBytes = sizeInBytes;
                }
            } else  {
                /* Decoding. */
                ma_decoder decoder;

                result = ma_resource_manager__init_decoder(pResourceManager, pFilePath, &decoder);
                if (result == MA_SUCCESS) {
                    ma_uint64 totalFrameCount;
                    ma_uint64 dataSizeInBytes;
                    void* pData = NULL;

                    pDataBuffer->pNode->data.decoded.format     = decoder.outputFormat;
                    pDataBuffer->pNode->data.decoded.channels   = decoder.outputChannels;
                    pDataBuffer->pNode->data.decoded.sampleRate = decoder.outputSampleRate;

                    totalFrameCount = ma_decoder_get_length_in_pcm_frames(&decoder);
                    if (totalFrameCount > 0) {
                        /* It's a known length. We can use an optimized allocation strategy in this case. */
                        dataSizeInBytes = totalFrameCount * ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
                        if (dataSizeInBytes <= MA_SIZE_MAX) {
                            pData = ma__malloc_from_callbacks((size_t)dataSizeInBytes, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
                            if (pData != NULL) {
                                totalFrameCount = ma_decoder_read_pcm_frames(&decoder, pData, totalFrameCount);
                            } else {
                                result = MA_OUT_OF_MEMORY;
                            }
                        } else {
                            result = MA_TOO_BIG;
                        }
                    } else {
                        /* It's an unknown length. We need to dynamically expand the buffer as we decode. To start with we allocate space for one page. We'll then double it as we need more space. */
                        ma_uint64 bytesPerFrame;
                        ma_uint64 pageSizeInFrames;
                        ma_uint64 dataSizeInFrames;
                            
                        bytesPerFrame    = ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
                        pageSizeInFrames = MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS * (decoder.outputSampleRate/1000);
                        dataSizeInFrames = 0;

                        /* Keep loading, page-by-page. */
                        for (;;) {
                            ma_uint64 framesRead;

                            /* Expand the buffer if need be. */
                            if (totalFrameCount + pageSizeInFrames > dataSizeInFrames) {                                    
                                ma_uint64 oldDataSizeInFrames;
                                ma_uint64 oldDataSizeInBytes;
                                ma_uint64 newDataSizeInFrames;
                                ma_uint64 newDataSizeInBytes;
                                void* pNewData;

                                oldDataSizeInFrames = (dataSizeInFrames);
                                newDataSizeInFrames = (dataSizeInFrames == 0) ? pageSizeInFrames : dataSizeInFrames * 2;

                                oldDataSizeInBytes = bytesPerFrame * oldDataSizeInFrames;
                                newDataSizeInBytes = bytesPerFrame * newDataSizeInFrames;

                                if (newDataSizeInBytes > MA_SIZE_MAX) {
                                    result = MA_TOO_BIG;
                                    break;
                                }

                                pNewData = ma__realloc_from_callbacks(pData, (size_t)newDataSizeInBytes, (size_t)oldDataSizeInBytes, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
                                if (pNewData == NULL) {
                                    ma__free_from_callbacks(pData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
                                    result = MA_OUT_OF_MEMORY;
                                    break;
                                }

                                pData            = pNewData;
                                dataSizeInFrames = newDataSizeInFrames;
                            }

                            framesRead = ma_decoder_read_pcm_frames(&decoder, ma_offset_ptr(pData, bytesPerFrame * totalFrameCount), pageSizeInFrames);
                            totalFrameCount += framesRead;

                            if (framesRead < pageSizeInFrames) {
                                /* We've reached the end. As we were loading we were doubling the size of the buffer each time we needed more memory. Let's try reducing this by doing a final realloc(). */
                                size_t newDataSizeInBytes = (size_t)(totalFrameCount  * bytesPerFrame);
                                size_t oldDataSizeInBytes = (size_t)(dataSizeInFrames * bytesPerFrame);
                                void* pNewData = ma__realloc_from_callbacks(pData, newDataSizeInBytes, oldDataSizeInBytes, &pResourceManager->config.allocationCallbacks);
                                if (pNewData != NULL) {
                                    pData = pNewData;
                                }

                                /* We're done, so get out of the loop. */
                                break;
                            }
                        }
                    }

                    if (result == MA_SUCCESS) {
                        pDataBuffer->pNode->data.decoded.pData             = pData;
                        pDataBuffer->pNode->data.decoded.frameCount        = totalFrameCount;
                        pDataBuffer->pNode->data.decoded.decodedFrameCount = totalFrameCount;  /* We've decoded everything. */
                    } else {
                        pDataBuffer->pNode->data.decoded.pData             = NULL;
                        pDataBuffer->pNode->data.decoded.frameCount        = 0;
                        pDataBuffer->pNode->data.decoded.decodedFrameCount = 0;
                    }

                    ma_decoder_uninit(&decoder);
                }
            }

            /* When loading synchronously we need to initialize the connector straight away. */
            if (result == MA_SUCCESS) {
                result = ma_resource_manager_data_buffer_init_connector(pDataBuffer, pNotification);
            }

            pDataBuffer->pNode->result = result;
        }

        /* If we failed to initialize make sure we fire the event and free memory. */
        if (result != MA_SUCCESS) {
            if (pNotification != NULL) {
                ma_async_notification_signal(pNotification, MA_NOTIFICATION_COMPLETE);
            }

            ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBuffer->pNode);
            ma__free_from_callbacks(pDataBuffer->pNode, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
            return result;
        }

        /* We'll need to fire the event if we have one in synchronous mode. */
        if (async == MA_FALSE) {
            if (pNotification != NULL) {
                ma_async_notification_signal(pNotification, MA_NOTIFICATION_COMPLETE);
            }
        }
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_init(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags, ma_async_notification* pNotification, ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_result result;
    ma_uint32 hashedName32;

    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pResourceManager == NULL || pFilePath == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Do as much set up before entering into the critical section to reduce our lock time as much as possible. */
    hashedName32 = ma_hash_string_32(pFilePath);

    /* At this point we can now enter the critical section. */
    ma_mutex_lock(&pResourceManager->dataBufferLock);
    {
        result = ma_resource_manager_data_buffer_init_nolock(pResourceManager, pFilePath, hashedName32, flags, pNotification, pDataBuffer);
    }
    ma_mutex_unlock(&pResourceManager->dataBufferLock);

    return result;
}

static ma_result ma_resource_manager_data_buffer_uninit_internal(ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pDataBuffer != NULL);

    /* The connector should be uninitialized first. */
    ma_resource_manager_data_buffer_uninit_connector(pDataBuffer->pResourceManager, pDataBuffer);
    pDataBuffer->connectorType = ma_resource_manager_data_buffer_connector_unknown;

    /* Free the node last. */
    ma_resource_manager_data_buffer_node_free(pDataBuffer->pResourceManager, pDataBuffer->pNode);

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_buffer_uninit_nolock(ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_uint32 result;
    ma_uint32 refCount;

    MA_ASSERT(pDataBuffer != NULL);

    result = ma_resource_manager_data_buffer_node_decrement_ref(pDataBuffer->pResourceManager, pDataBuffer->pNode, &refCount);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* If the reference count has hit zero it means we need to delete the data buffer and it's backing data (so long as it's owned by the resource manager). */
    if (refCount == 0) {
        ma_bool32 asyncUninit = MA_TRUE;

        result = ma_resource_manager_data_buffer_node_remove(pDataBuffer->pResourceManager, pDataBuffer->pNode);
        if (result != MA_SUCCESS) {
            return result;  /* An error occurred when trying to remove the data buffer. This should never happen. */
        }

        if (ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) == MA_SUCCESS) {
            asyncUninit = MA_FALSE;
        }

        /*
        The data buffer has been removed from the BST so now we need to delete the underyling data. This needs to be done in a separate thread. We don't
        want to delete anything if the data is owned by the application. Also, just to be safe, we set the result to MA_UNAVAILABLE.
        */
        c89atomic_exchange_i32(&pDataBuffer->pNode->result, MA_UNAVAILABLE);

        if (asyncUninit == MA_FALSE) {
            /* The data buffer can be deleted synchronously. */
            return ma_resource_manager_data_buffer_uninit_internal(pDataBuffer);
        } else {
            /*
            The data buffer needs to be deleted asynchronously because it's still loading. With the status set to MA_UNAVAILABLE, no more pages will
            be loaded and the uninitialization should happen fairly quickly. Since the caller owns the data buffer, we need to wait for this event
            to get processed before returning.
            */
            ma_async_notification_event waitEvent;
            ma_job job;

            result = ma_async_notification_event_init(&waitEvent);
            if (result != MA_SUCCESS) {
                return result;  /* Failed to create the wait event. This should rarely if ever happen. */
            }

            job = ma_job_init(MA_JOB_FREE_DATA_BUFFER);
            job.order = ma_resource_manager_data_buffer_next_execution_order(pDataBuffer);
            job.freeDataBuffer.pDataBuffer   = pDataBuffer;
            job.freeDataBuffer.pNotification = &waitEvent;

            result = ma_resource_manager_post_job(pDataBuffer->pResourceManager, &job);
            if (result != MA_SUCCESS) {
                ma_async_notification_event_uninit(&waitEvent);
                return result;
            }

            ma_async_notification_event_wait(&waitEvent);
            ma_async_notification_event_uninit(&waitEvent);
        }
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_uninit(ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_result result;

    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_mutex_lock(&pDataBuffer->pResourceManager->dataBufferLock);
    {
        result = ma_resource_manager_data_buffer_uninit_nolock(pDataBuffer);
    }
    ma_mutex_unlock(&pDataBuffer->pResourceManager->dataBufferLock);

    return result;
}

MA_API ma_result ma_resource_manager_data_buffer_read_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_result result;
    ma_uint64 framesRead;
    ma_bool32 isLooping;
    ma_bool32 skipBusyCheck = MA_FALSE;

    /*
    We cannot be using the data buffer after it's been uninitialized. If you trigger this assert it means you're trying to read from the data buffer after
    it's been uninitialized or is in the process of uninitializing.
    */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    /* If we haven't yet got a connector we need to abort. */
    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_unknown) {
        return MA_BUSY; /* Still loading. */
    }

    if (pDataBuffer->seekToCursorOnNextRead) {
        pDataBuffer->seekToCursorOnNextRead = MA_FALSE;

        result = ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_buffer_get_connector(pDataBuffer), pDataBuffer->cursorInPCMFrames);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    if (skipBusyCheck == MA_FALSE) {
        if (ma_resource_manager_data_buffer_is_busy(pDataBuffer, frameCount)) {
            return MA_BUSY;
        }
    }

    result = ma_resource_manager_data_buffer_get_looping(pDataBuffer, &isLooping);
    if (result != MA_SUCCESS) {
        return result;
    }

    result = ma_data_source_read_pcm_frames(ma_resource_manager_data_buffer_get_connector(pDataBuffer), pFramesOut, frameCount, &framesRead, isLooping);
    pDataBuffer->cursorInPCMFrames += framesRead;

    if (pFramesRead != NULL) {
        *pFramesRead = framesRead;
    }

    return result;
}

MA_API ma_result ma_resource_manager_data_buffer_seek_to_pcm_frame(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64 frameIndex)
{
    ma_result result;

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    /* If we haven't yet got a connector we need to abort. */
    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_unknown) {
        pDataBuffer->cursorInPCMFrames = frameIndex;
        pDataBuffer->seekToCursorOnNextRead = MA_TRUE;
        return MA_BUSY; /* Still loading. */
    }

    result = ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_buffer_get_connector(pDataBuffer), frameIndex);
    if (result != MA_SUCCESS) {
        return result;
    }

    pDataBuffer->cursorInPCMFrames = frameIndex;
    pDataBuffer->seekToCursorOnNextRead = MA_FALSE;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_map(ma_resource_manager_data_buffer* pDataBuffer, void** ppFramesOut, ma_uint64* pFrameCount)
{
    ma_result result;
    ma_bool32 skipBusyCheck = MA_FALSE;

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    /* If we haven't yet got a connector we need to abort. */
    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_unknown) {
        return MA_BUSY; /* Still loading. */
    }

    if (pDataBuffer->seekToCursorOnNextRead) {
        pDataBuffer->seekToCursorOnNextRead = MA_FALSE;

        result = ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_buffer_get_connector(pDataBuffer), pDataBuffer->cursorInPCMFrames);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    if (skipBusyCheck == MA_FALSE) {
        if (ma_resource_manager_data_buffer_is_busy(pDataBuffer, *pFrameCount)) {
            return MA_BUSY;
        }
    }

    /* The frame cursor is incremented in unmap(). */
    return ma_data_source_map(ma_resource_manager_data_buffer_get_connector(pDataBuffer), ppFramesOut, pFrameCount);
}

MA_API ma_result ma_resource_manager_data_buffer_unmap(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64 frameCount)
{
    ma_result result;

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    result = ma_data_source_unmap(ma_resource_manager_data_buffer_get_connector(pDataBuffer), frameCount);
    if (result == MA_SUCCESS) {
        pDataBuffer->cursorInPCMFrames += frameCount;
    }

    return result;
}

MA_API ma_result ma_resource_manager_data_buffer_get_data_format(ma_resource_manager_data_buffer* pDataBuffer, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    /* If we haven't yet got a connector we need to abort. */
    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_unknown) {
        return MA_BUSY; /* Still loading. */
    }

    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_buffer) {
        MA_ASSERT(pDataBuffer->pNode->data.type == ma_resource_manager_data_buffer_encoding_decoded);

        *pFormat     = pDataBuffer->pNode->data.decoded.format;
        *pChannels   = pDataBuffer->pNode->data.decoded.channels;
        *pSampleRate = pDataBuffer->pNode->data.decoded.sampleRate;

        return MA_SUCCESS;
    } else {
        return ma_data_source_get_data_format(&pDataBuffer->connector.decoder, pFormat, pChannels, pSampleRate);
    }
}

MA_API ma_result ma_resource_manager_data_buffer_get_cursor_in_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pCursor)
{
    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    if (pDataBuffer == NULL || pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = pDataBuffer->cursorInPCMFrames;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_get_length_in_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pLength)
{
    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    if (pDataBuffer == NULL || pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_unknown) {
        return MA_BUSY; /* Still loading. */
    }

    *pLength = pDataBuffer->lengthInPCMFrames;
    if (*pLength == 0) {
        return MA_NOT_IMPLEMENTED;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_result(const ma_resource_manager_data_buffer* pDataBuffer)
{
    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode);
}

MA_API ma_result ma_resource_manager_data_buffer_set_looping(ma_resource_manager_data_buffer* pDataBuffer, ma_bool32 isLooping)
{
    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_32(&pDataBuffer->isLooping, isLooping);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_get_looping(const ma_resource_manager_data_buffer* pDataBuffer, ma_bool32* pIsLooping)
{
    if (pIsLooping == NULL) {
        return MA_INVALID_ARGS;
    }

    *pIsLooping = MA_FALSE;

    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    *pIsLooping = c89atomic_load_32((ma_bool32*)&pDataBuffer->isLooping);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_get_available_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pAvailableFrames)
{
    if (pAvailableFrames == NULL) {
        return MA_INVALID_ARGS;
    }

    *pAvailableFrames = 0;

    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_unknown) {
        if (ma_resource_manager_data_buffer_result(pDataBuffer) == MA_BUSY) {
            return MA_BUSY;
        } else {
            return MA_INVALID_OPERATION;    /* No connector. */
        }
    }

    if (pDataBuffer->connectorType == ma_resource_manager_data_buffer_connector_buffer) {
        /* Retrieve the available frames based on how many frames we've currently decoded, and *not* the total capacity of the audio buffer. */
        if (pDataBuffer->pNode->data.decoded.decodedFrameCount > pDataBuffer->cursorInPCMFrames) {
            *pAvailableFrames = pDataBuffer->pNode->data.decoded.decodedFrameCount - pDataBuffer->cursorInPCMFrames;
        } else {
            *pAvailableFrames = 0;
        }

        return MA_SUCCESS;
    } else {
        return ma_decoder_get_available_frames(&pDataBuffer->connector.decoder, pAvailableFrames);
    }
}


static ma_result ma_resource_manager_register_data_nolock(ma_resource_manager* pResourceManager, ma_uint32 hashedName32, ma_resource_manager_data_buffer_encoding type, ma_resource_manager_memory_buffer* pExistingData)
{
    ma_result result;
    ma_resource_manager_data_buffer_node* pInsertPoint;
    ma_resource_manager_data_buffer_node* pNode;

    result = ma_resource_manager_data_buffer_node_insert_point(pResourceManager, hashedName32, &pInsertPoint);
    if (result == MA_ALREADY_EXISTS) {
        /* Fast path. The data buffer already exists. We just need to increment the reference counter and signal the event, if any. */
        pNode = pInsertPoint;

        result = ma_resource_manager_data_buffer_node_increment_ref(pResourceManager, pNode, NULL);
        if (result != MA_SUCCESS) {
            return result;  /* Should never happen. Failed to increment the reference count. */
        }
    } else {
        /* Slow path. The data for this buffer has not yet been initialized. The first thing to do is allocate the new data buffer and insert it into the BST. */
        pNode = (ma_resource_manager_data_buffer_node*)ma__malloc_from_callbacks(sizeof(*pNode), &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_RESOURCE_MANAGER_DATA_BUFFER*/);
        if (pNode == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        MA_ZERO_OBJECT(pNode);
        pNode->hashedName32 = hashedName32;
        pNode->refCount     = 1;        /* Always set to 1 by default (this is our first reference). */
        pNode->data.type    = type;
        pNode->result       = MA_SUCCESS;

        result = ma_resource_manager_data_buffer_node_insert_at(pResourceManager, pNode, pInsertPoint);
        if (result != MA_SUCCESS) {
            return result;  /* Should never happen. Failed to insert the data buffer into the BST. */
        }

        pNode->isDataOwnedByResourceManager = MA_FALSE;
        pNode->data = *pExistingData;
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_register_data(ma_resource_manager* pResourceManager, const char* pName, ma_resource_manager_data_buffer_encoding type, ma_resource_manager_memory_buffer* pExistingData)
{
    ma_result result = MA_SUCCESS;
    ma_uint32 hashedName32;

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    hashedName32 = ma_hash_string_32(pName);

    ma_mutex_lock(&pResourceManager->dataBufferLock);
    {
        result = ma_resource_manager_register_data_nolock(pResourceManager, hashedName32, type, pExistingData);
    }
    ma_mutex_unlock(&pResourceManager->dataBufferLock);

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

    return ma_resource_manager_register_data(pResourceManager, pName, data.type, &data);
}

MA_API ma_result ma_resource_manager_register_encoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, size_t sizeInBytes)
{
    ma_resource_manager_memory_buffer data;
    data.type                = ma_resource_manager_data_buffer_encoding_encoded;
    data.encoded.pData       = pData;
    data.encoded.sizeInBytes = sizeInBytes;

    return ma_resource_manager_register_data(pResourceManager, pName, data.type, &data);
}


static ma_result ma_resource_manager_unregister_data_nolock(ma_resource_manager* pResourceManager, ma_uint32 hashedName32)
{
    ma_result result;
    ma_resource_manager_data_buffer_node* pDataBufferNode;
    ma_uint32 refCount;

    result = ma_resource_manager_data_buffer_node_search(pResourceManager, hashedName32, &pDataBufferNode);
    if (result != MA_SUCCESS) {
        return result;  /* Couldn't find the node. */
    }

    result = ma_resource_manager_data_buffer_node_decrement_ref(pResourceManager, pDataBufferNode, &refCount);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* If the reference count has hit zero it means we can remove it from the BST. */
    if (refCount == 0) {
        result = ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBufferNode);
        if (result != MA_SUCCESS) {
            return result;  /* An error occurred when trying to remove the data buffer. This should never happen. */
        }
    }

    /* Finally we need to free the node. */
    ma_resource_manager_data_buffer_node_free(pResourceManager, pDataBufferNode);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName)
{
    ma_result result;
    ma_uint32 hashedName32;

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    hashedName32 = ma_hash_string_32(pName);

    /*
    It's assumed that the data specified by pName was registered with a prior call to ma_resource_manager_register_encoded/decoded_data(). To unregister it, all
    we need to do is delete the data buffer by it's name.
    */
    ma_mutex_lock(&pResourceManager->dataBufferLock);
    {
        result = ma_resource_manager_unregister_data_nolock(pResourceManager, hashedName32);
    }
    ma_mutex_unlock(&pResourceManager->dataBufferLock);

    return result;
}


static ma_uint32 ma_resource_manager_data_stream_next_execution_order(ma_resource_manager_data_stream* pDataStream)
{
    MA_ASSERT(pDataStream != NULL);
    return c89atomic_fetch_add_32(&pDataStream->executionCounter, 1);
}

static ma_bool32 ma_resource_manager_data_stream_is_decoder_at_end(const ma_resource_manager_data_stream* pDataStream)
{
    MA_ASSERT(pDataStream != NULL);
    return c89atomic_load_32((ma_bool32*)&pDataStream->isDecoderAtEnd);
}

static ma_uint32 ma_resource_manager_data_stream_seek_counter(const ma_resource_manager_data_stream* pDataStream)
{
    MA_ASSERT(pDataStream != NULL);
    return c89atomic_load_32((ma_uint32*)&pDataStream->seekCounter);
}


static ma_result ma_resource_manager_data_stream_cb__read_pcm_frames(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_resource_manager_data_stream_read_pcm_frames((ma_resource_manager_data_stream*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_resource_manager_data_stream_cb__seek_to_pcm_frame(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_resource_manager_data_stream_seek_to_pcm_frame((ma_resource_manager_data_stream*)pDataSource, frameIndex);
}

static ma_result ma_resource_manager_data_stream_cb__map(ma_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount)
{
    return ma_resource_manager_data_stream_map((ma_resource_manager_data_stream*)pDataSource, ppFramesOut, pFrameCount);
}

static ma_result ma_resource_manager_data_stream_cb__unmap(ma_data_source* pDataSource, ma_uint64 frameCount)
{
    return ma_resource_manager_data_stream_unmap((ma_resource_manager_data_stream*)pDataSource, frameCount);
}

static ma_result ma_resource_manager_data_stream_cb__get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    return ma_resource_manager_data_stream_get_data_format((ma_resource_manager_data_stream*)pDataSource, pFormat, pChannels, pSampleRate);
}

static ma_result ma_resource_manager_data_stream_cb__get_cursor_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_resource_manager_data_stream_get_cursor_in_pcm_frames((ma_resource_manager_data_stream*)pDataSource, pCursor);
}

static ma_result ma_resource_manager_data_stream_cb__get_length_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_resource_manager_data_stream_get_length_in_pcm_frames((ma_resource_manager_data_stream*)pDataSource, pLength);
}

MA_API ma_result ma_resource_manager_data_stream_init(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags, ma_async_notification* pNotification, ma_resource_manager_data_stream* pDataStream)
{
    ma_result result;
    char* pFilePathCopy;
    ma_job job;

    if (pDataStream == NULL) {
        if (pNotification != NULL) {
            ma_async_notification_signal(pNotification, MA_NOTIFICATION_COMPLETE);
        }

        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataStream);
    pDataStream->ds.onRead          = ma_resource_manager_data_stream_cb__read_pcm_frames;
    pDataStream->ds.onSeek          = ma_resource_manager_data_stream_cb__seek_to_pcm_frame;
    pDataStream->ds.onMap           = ma_resource_manager_data_stream_cb__map;
    pDataStream->ds.onUnmap         = ma_resource_manager_data_stream_cb__unmap;
    pDataStream->ds.onGetDataFormat = ma_resource_manager_data_stream_cb__get_data_format;
    pDataStream->ds.onGetCursor     = ma_resource_manager_data_stream_cb__get_cursor_in_pcm_frames;
    pDataStream->ds.onGetLength     = ma_resource_manager_data_stream_cb__get_length_in_pcm_frames;

    pDataStream->pResourceManager   = pResourceManager;
    pDataStream->flags              = flags;
    pDataStream->result             = MA_BUSY;

    if (pResourceManager == NULL || pFilePath == NULL) {
        if (pNotification != NULL) {
            ma_async_notification_signal(pNotification, MA_NOTIFICATION_COMPLETE);
        }

        return MA_INVALID_ARGS;
    }

    /* We want all access to the VFS and the internal decoder to happen on the job thread just to keep things easier to manage for the VFS.  */

    /* We need a copy of the file path. We should probably make this more efficient, but for now we'll do a transient memory allocation. */
    pFilePathCopy = ma_copy_string(pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
    if (pFilePathCopy == NULL) {
        if (pNotification != NULL) {
            ma_async_notification_signal(pNotification, MA_NOTIFICATION_FAILED);
        }

        return MA_OUT_OF_MEMORY;
    }

    /* We now have everything we need to post the job. This is the last thing we need to do from here. The rest will be done by the job thread. */
    job = ma_job_init(MA_JOB_LOAD_DATA_STREAM);
    job.order = ma_resource_manager_data_stream_next_execution_order(pDataStream);
    job.loadDataStream.pDataStream   = pDataStream;
    job.loadDataStream.pFilePath     = pFilePathCopy;
    job.loadDataStream.pNotification = pNotification;
    result = ma_resource_manager_post_job(pResourceManager, &job);
    if (result != MA_SUCCESS) {
        if (pNotification != NULL) {
            ma_async_notification_signal(pNotification, MA_NOTIFICATION_FAILED);
        }

        ma__free_from_callbacks(pFilePathCopy, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);
        return result;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_uninit(ma_resource_manager_data_stream* pDataStream)
{
    ma_async_notification_event freeEvent;
    ma_job job;

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The first thing to do is set the result to unavailable. This will prevent future page decoding. */
    c89atomic_exchange_i32(&pDataStream->result, MA_UNAVAILABLE);

    /*
    We need to post a job to ensure we're not in the middle or decoding or anything. Because the object is owned by the caller, we'll need
    to wait for it to complete before returning which means we need an event.
    */
    ma_async_notification_event_init(&freeEvent);

    job = ma_job_init(MA_JOB_FREE_DATA_STREAM);
    job.order = ma_resource_manager_data_stream_next_execution_order(pDataStream);
    job.freeDataStream.pDataStream   = pDataStream;
    job.freeDataStream.pNotification = &freeEvent;
    ma_resource_manager_post_job(pDataStream->pResourceManager, &job);

    /* We need to wait for the job to finish processing before we return. */
    ma_async_notification_event_wait(&freeEvent);
    ma_async_notification_event_uninit(&freeEvent);

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

static void ma_resource_manager_data_stream_fill_page(ma_resource_manager_data_stream* pDataStream, ma_uint32 pageIndex)
{
    ma_bool32 isLooping;
    ma_uint64 pageSizeInFrames;
    ma_uint64 totalFramesReadForThisPage = 0;
    void* pPageData = ma_resource_manager_data_stream_get_page_data_pointer(pDataStream, pageIndex, 0);

    pageSizeInFrames = ma_resource_manager_data_stream_get_page_size_in_frames(pDataStream);

    ma_resource_manager_data_stream_get_looping(pDataStream, &isLooping);   /* Won't fail. */

    if (isLooping) {
        while (totalFramesReadForThisPage < pageSizeInFrames) {
            ma_uint64 framesRemaining;
            ma_uint64 framesRead;

            framesRemaining = pageSizeInFrames - totalFramesReadForThisPage;
            framesRead = ma_decoder_read_pcm_frames(&pDataStream->decoder, ma_offset_pcm_frames_ptr(pPageData, totalFramesReadForThisPage, pDataStream->decoder.outputFormat, pDataStream->decoder.outputChannels), framesRemaining);
            totalFramesReadForThisPage += framesRead;

            /* Loop back to the start if we reached the end. We'll also have a known length at this point as well. */
            if (framesRead < framesRemaining) {
                if (pDataStream->totalLengthInPCMFrames == 0) {
                    ma_decoder_get_cursor_in_pcm_frames(&pDataStream->decoder, &pDataStream->totalLengthInPCMFrames);
                }

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

static void ma_resource_manager_data_stream_fill_pages(ma_resource_manager_data_stream* pDataStream)
{
    ma_uint32 iPage;

    MA_ASSERT(pDataStream != NULL);

    /* For each page... */
    for (iPage = 0; iPage < 2; iPage += 1) {
        ma_resource_manager_data_stream_fill_page(pDataStream, iPage);

        /* If we reached the end make sure we get out of the loop to prevent us from trying to load the second page. */
        if (ma_resource_manager_data_stream_is_decoder_at_end(pDataStream)) {
            break;
        }
    }
}

MA_API ma_result ma_resource_manager_data_stream_read_pcm_frames(ma_resource_manager_data_stream* pDataStream, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 totalFramesProcessed;
    ma_format format;
    ma_uint32 channels;

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_stream_result(pDataStream) != MA_UNAVAILABLE);

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ma_resource_manager_data_stream_result(pDataStream) != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    /* Don't attempt to read while we're in the middle of seeking. Tell the caller that we're busy. */
    if (ma_resource_manager_data_stream_seek_counter(pDataStream) > 0) {
        return MA_BUSY;
    }

    ma_resource_manager_data_stream_get_data_format(pDataStream, &format, &channels, NULL);

    /* Reading is implemented in terms of map/unmap. We need to run this in a loop because mapping is clamped against page boundaries. */
    totalFramesProcessed = 0;
    while (totalFramesProcessed < frameCount) {
        void* pMappedFrames;
        ma_uint64 mappedFrameCount;

        mappedFrameCount = frameCount - totalFramesProcessed;
        result = ma_resource_manager_data_stream_map(pDataStream, &pMappedFrames, &mappedFrameCount);
        if (result != MA_SUCCESS) {
            break;
        }

        /* Copy the mapped data to the output buffer if we have one. It's allowed for pFramesOut to be NULL in which case a relative forward seek is performed. */
        if (pFramesOut != NULL) {
            ma_copy_pcm_frames(ma_offset_pcm_frames_ptr(pFramesOut, totalFramesProcessed, format, channels), pMappedFrames, mappedFrameCount, format, channels);
        }

        totalFramesProcessed += mappedFrameCount;

        result = ma_resource_manager_data_stream_unmap(pDataStream, mappedFrameCount);
        if (result != MA_SUCCESS) {
            break;  /* This is really bad - will only get an error here if we failed to post a job to the queue for loading the next page. */
        }
    }

    if (pFramesRead != NULL) {
        *pFramesRead = totalFramesProcessed;
    }

    return result;
}

MA_API ma_result ma_resource_manager_data_stream_map(ma_resource_manager_data_stream* pDataStream, void** ppFramesOut, ma_uint64* pFrameCount)
{
    ma_uint64 framesAvailable;
    ma_uint64 frameCount = 0;

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_stream_result(pDataStream) != MA_UNAVAILABLE);

    if (pFrameCount != NULL) {
        frameCount = *pFrameCount;
        *pFrameCount = 0;
    }
    if (ppFramesOut != NULL) {
        *ppFramesOut = NULL;
    }

    if (pDataStream == NULL || ppFramesOut == NULL || pFrameCount == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ma_resource_manager_data_stream_result(pDataStream) != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    /* Don't attempt to read while we're in the middle of seeking. Tell the caller that we're busy. */
    if (ma_resource_manager_data_stream_seek_counter(pDataStream) > 0) {
        return MA_BUSY;
    }

    /* If the page we're on is invalid it means we've caught up to the job thread. */
    if (c89atomic_load_32(&pDataStream->isPageValid[pDataStream->currentPageIndex]) == MA_FALSE) {
        framesAvailable = 0;
    } else {
        /*
        The page we're on is valid so we must have some frames available. We need to make sure that we don't overflow into the next page, even if it's valid. The reason is
        that the unmap process will only post an update for one page at a time. Keeping mapping tied to page boundaries makes this simpler.
        */
        ma_uint32 currentPageFrameCount = c89atomic_load_32(&pDataStream->pageFrameCount[pDataStream->currentPageIndex]);
        MA_ASSERT(currentPageFrameCount >= pDataStream->relativeCursor);

        framesAvailable = currentPageFrameCount - pDataStream->relativeCursor;
    }

    /* If there's no frames available and the result is set to MA_AT_END we need to return MA_AT_END. */
    if (framesAvailable == 0) {
        if (ma_resource_manager_data_stream_is_decoder_at_end(pDataStream)) {
            return MA_AT_END;
        } else {
            return MA_BUSY; /* There are no frames available, but we're not marked as EOF so we might have caught up to the job thread. Need to return MA_BUSY and wait for more data. */
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

MA_API ma_result ma_resource_manager_data_stream_unmap(ma_resource_manager_data_stream* pDataStream, ma_uint64 frameCount)
{
    ma_uint32 newRelativeCursor;
    ma_uint32 pageSizeInFrames;
    ma_job job;

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_stream_result(pDataStream) != MA_UNAVAILABLE);

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ma_resource_manager_data_stream_result(pDataStream) != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    /* The frame count should always fit inside a 32-bit integer. */
    if (frameCount > 0xFFFFFFFF) {
        return MA_INVALID_ARGS;
    }

    pageSizeInFrames = ma_resource_manager_data_stream_get_page_size_in_frames(pDataStream);

    /* The absolute cursor needs to be updated. We want to make sure to loop if possible. */
    pDataStream->absoluteCursor += frameCount;
    if (pDataStream->absoluteCursor > pDataStream->totalLengthInPCMFrames && pDataStream->totalLengthInPCMFrames > 0) {
        pDataStream->absoluteCursor = pDataStream->absoluteCursor % pDataStream->totalLengthInPCMFrames;
    }

    /* Here is where we need to check if we need to load a new page, and if so, post a job to load it. */
    newRelativeCursor = pDataStream->relativeCursor + (ma_uint32)frameCount;

    /* If the new cursor has flowed over to the next page we need to mark the old one as invalid and post an event for it. */
    if (newRelativeCursor >= pageSizeInFrames) {
        newRelativeCursor -= pageSizeInFrames;

        /* Here is where we post the job start decoding. */
        job = ma_job_init(MA_JOB_PAGE_DATA_STREAM);
        job.order = ma_resource_manager_data_stream_next_execution_order(pDataStream);
        job.pageDataStream.pDataStream = pDataStream;
        job.pageDataStream.pageIndex   = pDataStream->currentPageIndex;

        /* The page needs to be marked as invalid so that the public API doesn't try reading from it. */
        c89atomic_exchange_32(&pDataStream->isPageValid[pDataStream->currentPageIndex], MA_FALSE);

        /* Before posting the job we need to make sure we set some state. */
        pDataStream->relativeCursor   = newRelativeCursor;
        pDataStream->currentPageIndex = (pDataStream->currentPageIndex + 1) & 0x01;
        return ma_resource_manager_post_job(pDataStream->pResourceManager, &job);
    } else {
        /* We haven't moved into a new page so we can just move the cursor forward. */
        pDataStream->relativeCursor = newRelativeCursor;
        return MA_SUCCESS;
    }
}

MA_API ma_result ma_resource_manager_data_stream_seek_to_pcm_frame(ma_resource_manager_data_stream* pDataStream, ma_uint64 frameIndex)
{
    ma_job job;
    ma_result streamResult;

    streamResult = ma_resource_manager_data_stream_result(pDataStream);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(streamResult != MA_UNAVAILABLE);

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (streamResult != MA_SUCCESS && streamResult != MA_BUSY) {
        return MA_INVALID_OPERATION;
    }

    /* Increment the seek counter first to indicate to read_paged_pcm_frames() and map_paged_pcm_frames() that we are in the middle of a seek and MA_BUSY should be returned. */
    c89atomic_fetch_add_32(&pDataStream->seekCounter, 1);

    /*
    We need to clear our currently loaded pages so that the stream starts playback from the new seek point as soon as possible. These are for the purpose of the public
    API and will be ignored by the seek job. The seek job will operate on the assumption that both pages have been marked as invalid and the cursor is at the start of
    the first page.
    */
    pDataStream->relativeCursor   = 0;
    pDataStream->currentPageIndex = 0;
    c89atomic_exchange_32(&pDataStream->isPageValid[0], MA_FALSE);
    c89atomic_exchange_32(&pDataStream->isPageValid[1], MA_FALSE);

    /*
    The public API is not allowed to touch the internal decoder so we need to use a job to perform the seek. When seeking, the job thread will assume both pages
    are invalid and any content contained within them will be discarded and replaced with newly decoded data.
    */
    job = ma_job_init(MA_JOB_SEEK_DATA_STREAM);
    job.order = ma_resource_manager_data_stream_next_execution_order(pDataStream);
    job.seekDataStream.pDataStream = pDataStream;
    job.seekDataStream.frameIndex  = frameIndex;
    return ma_resource_manager_post_job(pDataStream->pResourceManager, &job);
}

MA_API ma_result ma_resource_manager_data_stream_get_data_format(ma_resource_manager_data_stream* pDataStream, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_stream_result(pDataStream) != MA_UNAVAILABLE);

    if (pFormat != NULL) {
        *pFormat = ma_format_unknown;
    }

    if (pChannels != NULL) {
        *pChannels = 0;
    }

    if (pSampleRate != NULL) {
        *pSampleRate = 0;
    }

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ma_resource_manager_data_stream_result(pDataStream) != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    /*
    We're being a little bit naughty here and accessing the internal decoder from the public API. The output data format is constant, and we've defined this function
    such that the application is responsible for ensuring it's not called while uninitializing so it should be safe.
    */
    return ma_data_source_get_data_format(&pDataStream->decoder, pFormat, pChannels, pSampleRate);
}

MA_API ma_result ma_resource_manager_data_stream_get_cursor_in_pcm_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pCursor)
{
    if (pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_stream_result(pDataStream) != MA_UNAVAILABLE);

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ma_resource_manager_data_stream_result(pDataStream) != MA_SUCCESS) {
        return MA_INVALID_OPERATION;
    }

    *pCursor = pDataStream->absoluteCursor;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_get_length_in_pcm_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pLength)
{
    ma_result streamResult;

    if (pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    *pLength = 0;

    streamResult = ma_resource_manager_data_stream_result(pDataStream);

    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(streamResult != MA_UNAVAILABLE);

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    if (streamResult != MA_SUCCESS) {
        return streamResult;
    }

    /*
    We most definitely do not want to be calling ma_decoder_get_length_in_pcm_frames() directly. Instead we want to use a cached value that we
    calculated when we initialized it on the job thread.
    */
    *pLength = pDataStream->totalLengthInPCMFrames;
    if (*pLength == 0) {
        return MA_NOT_IMPLEMENTED;  /* Some decoders may not have a known length. */
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_result(const ma_resource_manager_data_stream* pDataStream)
{
    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    return c89atomic_load_i32(&pDataStream->result);
}

MA_API ma_result ma_resource_manager_data_stream_set_looping(ma_resource_manager_data_stream* pDataStream, ma_bool32 isLooping)
{
    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_32(&pDataStream->isLooping, isLooping);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_get_looping(const ma_resource_manager_data_stream* pDataStream, ma_bool32* pIsLooping)
{
    if (pIsLooping == NULL) {
        return MA_INVALID_ARGS;
    }

    *pIsLooping = MA_FALSE;

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    *pIsLooping = c89atomic_load_32((ma_bool32*)&pDataStream->isLooping);   /* Naughty const-cast. Value won't change from here in practice (maybe from another thread). */

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_get_available_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pAvailableFrames)
{
    volatile ma_uint32 pageIndex0;
    volatile ma_uint32 pageIndex1;
    volatile ma_uint32 relativeCursor;
    ma_uint64 availableFrames;

    if (pAvailableFrames == NULL) {
        return MA_INVALID_ARGS;
    }

    *pAvailableFrames = 0;

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    pageIndex0     =  pDataStream->currentPageIndex;
    pageIndex1     = (pDataStream->currentPageIndex + 1) & 0x01;
    relativeCursor =  pDataStream->relativeCursor;

    availableFrames = 0;
    if (c89atomic_load_32(&pDataStream->isPageValid[pageIndex0])) {
        availableFrames += c89atomic_load_32(&pDataStream->pageFrameCount[pageIndex0]) - relativeCursor;
        if (c89atomic_load_32(&pDataStream->isPageValid[pageIndex1])) {
            availableFrames += c89atomic_load_32(&pDataStream->pageFrameCount[pageIndex1]);
        }
    }

    *pAvailableFrames = availableFrames;
    return MA_SUCCESS;
}



MA_API ma_result ma_resource_manager_data_source_init(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, ma_async_notification* pNotification, ma_resource_manager_data_source* pDataSource)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataSource);

    if (pResourceManager == NULL || pName == NULL) {
        return MA_INVALID_ARGS;
    }

    pDataSource->flags = flags;

    /* The data source itself is just a data stream or a data buffer. */
    if ((flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_init(pResourceManager, pName, flags, pNotification, &pDataSource->stream);
    } else {
        return ma_resource_manager_data_buffer_init(pResourceManager, pName, flags, pNotification, &pDataSource->buffer);
    }
}

MA_API ma_result ma_resource_manager_data_source_uninit(ma_resource_manager_data_source* pDataSource)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /* All we need to is uninitialize the underlying data buffer or data stream. */
    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_uninit(&pDataSource->stream);
    } else {
        return ma_resource_manager_data_buffer_uninit(&pDataSource->buffer);
    }
}

MA_API ma_result ma_resource_manager_data_source_read_pcm_frames(ma_resource_manager_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_read_pcm_frames(&pDataSource->stream, pFramesOut, frameCount, pFramesRead);
    } else {
        return ma_resource_manager_data_buffer_read_pcm_frames(&pDataSource->buffer, pFramesOut, frameCount, pFramesRead);
    }
}

MA_API ma_result ma_resource_manager_data_source_seek_to_pcm_frame(ma_resource_manager_data_source* pDataSource, ma_uint64 frameIndex)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_seek_to_pcm_frame(&pDataSource->stream, frameIndex);
    } else {
        return ma_resource_manager_data_buffer_seek_to_pcm_frame(&pDataSource->buffer, frameIndex);
    }
}

MA_API ma_result ma_resource_manager_data_source_map(ma_resource_manager_data_source* pDataSource, void** ppFramesOut, ma_uint64* pFrameCount)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_map(&pDataSource->stream, ppFramesOut, pFrameCount);
    } else {
        return ma_resource_manager_data_buffer_map(&pDataSource->buffer, ppFramesOut, pFrameCount);
    }
}

MA_API ma_result ma_resource_manager_data_source_unmap(ma_resource_manager_data_source* pDataSource, ma_uint64 frameCount)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_unmap(&pDataSource->stream, frameCount);
    } else {
        return ma_resource_manager_data_buffer_unmap(&pDataSource->buffer, frameCount);
    }
}

MA_API ma_result ma_resource_manager_data_source_get_data_format(ma_resource_manager_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_get_data_format(&pDataSource->stream, pFormat, pChannels, pSampleRate);
    } else {
        return ma_resource_manager_data_buffer_get_data_format(&pDataSource->buffer, pFormat, pChannels, pSampleRate);
    }
}

MA_API ma_result ma_resource_manager_data_source_get_cursor_in_pcm_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pCursor)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_get_cursor_in_pcm_frames(&pDataSource->stream, pCursor);
    } else {
        return ma_resource_manager_data_buffer_get_cursor_in_pcm_frames(&pDataSource->buffer, pCursor);
    }
}

MA_API ma_result ma_resource_manager_data_source_get_length_in_pcm_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pLength)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_get_length_in_pcm_frames(&pDataSource->stream, pLength);
    } else {
        return ma_resource_manager_data_buffer_get_length_in_pcm_frames(&pDataSource->buffer, pLength);
    }
}

MA_API ma_result ma_resource_manager_data_source_result(const ma_resource_manager_data_source* pDataSource)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_result(&pDataSource->stream);
    } else {
        return ma_resource_manager_data_buffer_result(&pDataSource->buffer);
    }
}

MA_API ma_result ma_resource_manager_data_source_set_looping(ma_resource_manager_data_source* pDataSource, ma_bool32 isLooping)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_set_looping(&pDataSource->stream, isLooping);
    } else {
        return ma_resource_manager_data_buffer_set_looping(&pDataSource->buffer, isLooping);
    }
}

MA_API ma_result ma_resource_manager_data_source_get_looping(const ma_resource_manager_data_source* pDataSource, ma_bool32* pIsLooping)
{
    if (pDataSource == NULL || pIsLooping == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_get_looping(&pDataSource->stream, pIsLooping);
    } else {
        return ma_resource_manager_data_buffer_get_looping(&pDataSource->buffer, pIsLooping);
    }
}

MA_API ma_result ma_resource_manager_data_source_get_available_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pAvailableFrames)
{
    if (pAvailableFrames == NULL) {
        return MA_INVALID_ARGS;
    }

    *pAvailableFrames = 0;

    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_get_available_frames(&pDataSource->stream, pAvailableFrames);
    } else {
        return ma_resource_manager_data_buffer_get_available_frames(&pDataSource->buffer, pAvailableFrames);
    }
}


MA_API ma_result ma_resource_manager_post_job(ma_resource_manager* pResourceManager, const ma_job* pJob)
{
    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_job_queue_post(&pResourceManager->jobQueue, pJob);
}

MA_API ma_result ma_resource_manager_post_job_quit(ma_resource_manager* pResourceManager)
{
    ma_job job = ma_job_init(MA_JOB_QUIT);
    return ma_resource_manager_post_job(pResourceManager, &job);
}

MA_API ma_result ma_resource_manager_next_job(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_job_queue_next(&pResourceManager->jobQueue, pJob);
}


static ma_result ma_resource_manager_process_job__load_data_buffer(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    ma_result result = MA_SUCCESS;
    ma_resource_manager_data_buffer* pDataBuffer;
    ma_decoder* pDecoder = NULL;       /* Malloc'd here, and then free'd on the last page decode. */
    ma_uint64 totalFrameCount = 0;
    void* pData = NULL;
    ma_uint64 dataSizeInBytes = 0;
    ma_uint64 framesRead = 0;   /* <-- Keeps track of how many frames we read for the first page. */

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);
    MA_ASSERT(pJob->loadDataBuffer.pFilePath          != NULL);
    MA_ASSERT(pJob->loadDataBuffer.pDataBuffer        != NULL);
    MA_ASSERT(pJob->freeDataBuffer.pDataBuffer->pNode != NULL);
    MA_ASSERT(pJob->loadDataBuffer.pDataBuffer->pNode->isDataOwnedByResourceManager == MA_TRUE);  /* The data should always be owned by the resource manager. */

    pDataBuffer = pJob->loadDataBuffer.pDataBuffer;

    /* First thing we need to do is check whether or not the data buffer is getting deleted. If so we just abort. */
    if (ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_BUSY) {
        result = MA_INVALID_OPERATION;    /* The data buffer may be getting deleted before it's even been loaded. */
        goto done;
    }

    /* The data buffer is not getting deleted, but we may be getting executed out of order. If so, we need to push the job back onto the queue and return. */
    if (pJob->order != pDataBuffer->pNode->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Attempting to execute out of order. Probably interleaved with a MA_JOB_FREE_DATA_BUFFER job. */
    }

    if (pDataBuffer->pNode->data.type == ma_resource_manager_data_buffer_encoding_encoded) {
        /* No decoding. Just store the file contents in memory. */
        size_t sizeInBytes;

        result = ma_vfs_open_and_read_file_ex(pResourceManager->config.pVFS, pJob->loadDataBuffer.pFilePath, &pData, &sizeInBytes, &pResourceManager->config.allocationCallbacks, MA_ALLOCATION_TYPE_ENCODED_BUFFER);
        if (result == MA_SUCCESS) {
            pDataBuffer->pNode->data.encoded.pData       = pData;
            pDataBuffer->pNode->data.encoded.sizeInBytes = sizeInBytes;
        }

        result = ma_resource_manager_data_buffer_init_connector(pDataBuffer, pJob->loadDataBuffer.pNotification);
    } else  {
        /* Decoding. */
        ma_uint64 dataSizeInFrames;
        ma_uint64 pageSizeInFrames;

        /*
        With the file initialized we now need to initialize the decoder. We need to pass this decoder around on the job queue so we'll need to
        allocate memory for this dynamically.
        */
        pDecoder = (ma_decoder*)ma__malloc_from_callbacks(sizeof(*pDecoder), &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);
        if (pDecoder == NULL) {
            result = MA_OUT_OF_MEMORY;
            goto done;
        }

        result = ma_resource_manager__init_decoder(pResourceManager, pJob->loadDataBuffer.pFilePath, pDecoder);

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
        pDataBuffer->pNode->data.decoded.format     = pDecoder->outputFormat;
        pDataBuffer->pNode->data.decoded.channels   = pDecoder->outputChannels;
        pDataBuffer->pNode->data.decoded.sampleRate = pDecoder->outputSampleRate;

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

        /* The buffer needs to be initialized to silence in case the caller reads from it. */
        ma_silence_pcm_frames(pData, dataSizeInFrames, pDecoder->outputFormat, pDecoder->outputChannels);


        /* We should have enough room in our buffer for at least a whole page, or the entire file (if it's less than a page). We can now decode that first page. */
        framesRead = ma_decoder_read_pcm_frames(pDecoder, pData, pageSizeInFrames);
        if (framesRead < pageSizeInFrames) {
            /* We've read the entire sound. This is the simple case. We just need to set the result to MA_SUCCESS. */
            pDataBuffer->pNode->data.decoded.pData      = pData;
            pDataBuffer->pNode->data.decoded.frameCount = framesRead;

            /*
            decodedFrameCount is what other threads will use to determine whether or not data is available. We must ensure pData and frameCount
            is set *before* setting the number of available frames. This way, the other thread need only check if decodedFrameCount > 0, in
            which case it can assume pData and frameCount are valid.
            */
            c89atomic_thread_fence(c89atomic_memory_order_acquire);
            pDataBuffer->pNode->data.decoded.decodedFrameCount = framesRead;

            ma_decoder_uninit(pDecoder);
            ma__free_from_callbacks(pDecoder, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);

            result = ma_resource_manager_data_buffer_init_connector(pDataBuffer, pJob->loadDataBuffer.pNotification);
            goto done;
        } else {
            /* We've still got more to decode. We just set the result to MA_BUSY which will tell the next section below to post a paging event. */
            result = MA_BUSY;
        }

        /* If we successfully initialized and the sound is of a known length we can start initialize the connector. */
        if (result == MA_SUCCESS || result == MA_BUSY) {
            if (pDataBuffer->pNode->data.decoded.decodedFrameCount > 0) {
                result = ma_resource_manager_data_buffer_init_connector(pDataBuffer, pJob->loadDataBuffer.pNotification);
            }
        }
    }

done:
    ma__free_from_callbacks(pJob->loadDataBuffer.pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);

    /*
    We need to set the result to at the very end to ensure no other threads try reading the data before we've fully initialized the object. Other threads
    are going to be inspecting this variable to determine whether or not they're ready to read data. We can only change the result if it's set to MA_BUSY
    because otherwise we may be changing away from an error code which would be bad. An example is if the application creates a data buffer, but then
    immediately deletes it before we've got to this point. In this case, pDataBuffer->result will be MA_UNAVAILABLE, and setting it to MA_SUCCESS or any
    other error code would cause the buffer to look like it's in a state that it's not.
    */
    c89atomic_compare_and_swap_32(&pDataBuffer->pNode->result, MA_BUSY, result);

    /*
    If our result is MA_BUSY we need to post a job to start loading. It's important that we do this after setting the result to the buffer so that the
    decoding process happens at the right time. If we don't, there's a window where the MA_JOB_PAGE_DATA_BUFFER event will see a status of something
    other than MA_BUSY and then abort the decoding process with an error.
    */
    if (result == MA_BUSY && pDecoder != NULL) {
        /* We've still got more to decode. We need to post a job to continue decoding. */
        ma_job pageDataBufferJob;

        MA_ASSERT(pDecoder != NULL);
        MA_ASSERT(pData    != NULL);

        pageDataBufferJob = ma_job_init(MA_JOB_PAGE_DATA_BUFFER);
        pageDataBufferJob.order                                 = ma_resource_manager_data_buffer_next_execution_order(pDataBuffer);
        pageDataBufferJob.pageDataBuffer.pDataBuffer            = pDataBuffer;
        pageDataBufferJob.pageDataBuffer.pDecoder               = pDecoder;
        pageDataBufferJob.pageDataBuffer.pCompletedNotification = pJob->loadDataBuffer.pNotification;
        pageDataBufferJob.pageDataBuffer.pData                  = pData;
        pageDataBufferJob.pageDataBuffer.dataSizeInBytes        = (size_t)dataSizeInBytes;   /* Safe cast. Was checked for > MA_SIZE_MAX earlier. */
        pageDataBufferJob.pageDataBuffer.decodedFrameCount      = framesRead;

        if (totalFrameCount > 0) {
            pageDataBufferJob.pageDataBuffer.isUnknownLength = MA_FALSE;

            pDataBuffer->pNode->data.decoded.pData      = pData;
            pDataBuffer->pNode->data.decoded.frameCount = totalFrameCount;

            /*
            decodedFrameCount is what other threads will use to determine whether or not data is available. We must ensure pData and frameCount
            is set *before* setting the number of available frames. This way, the other thread need only check if decodedFrameCount > 0, in
            which case it can assume pData and frameCount are valid.
            */
            c89atomic_thread_fence(c89atomic_memory_order_acquire);
            pDataBuffer->pNode->data.decoded.decodedFrameCount = framesRead;

            /* The sound is of a known length so we can go ahead and initialize the connector now. */
            result = ma_resource_manager_data_buffer_init_connector(pDataBuffer, pJob->loadDataBuffer.pNotification);
        } else {
            pageDataBufferJob.pageDataBuffer.isUnknownLength = MA_TRUE;

            /*
            These members are all set after the last page has been decoded. The reason for this is that the application should not be attempting to
            read any data until the sound is fully decoded because we're going to be dynamically expanding pData and we'll be introducing complications
            by letting the application get access to it.
            */
            pDataBuffer->pNode->data.decoded.pData             = NULL;
            pDataBuffer->pNode->data.decoded.frameCount        = 0;
            pDataBuffer->pNode->data.decoded.decodedFrameCount = 0;
        }

        if (result == MA_SUCCESS) {
            /* The job has been set up so it can now be posted. */
            result = ma_resource_manager_post_job(pResourceManager, &pageDataBufferJob);

            /* The result needs to be set to MA_BUSY to ensure the status of the data buffer is set properly in the next section. */
            if (result == MA_SUCCESS) {
                result = MA_BUSY;
            }
        }
            
        /* We want to make sure we don't signal the event here. It needs to be delayed until the last page. */
        pJob->loadDataBuffer.pNotification = NULL;

        /* Make sure the buffer's status is updated appropriately, but make sure we never move away from a MA_BUSY state to ensure we don't overwrite any error codes. */
        c89atomic_compare_and_swap_32(&pDataBuffer->pNode->result, MA_BUSY, result);
    }

    /* Only signal the other threads after the result has been set just for cleanliness sake. */
    if (pJob->loadDataBuffer.pNotification != NULL) {
        ma_async_notification_signal(pJob->loadDataBuffer.pNotification, MA_NOTIFICATION_COMPLETE);
    }

    c89atomic_fetch_add_32(&pDataBuffer->pNode->executionPointer, 1);
    return result;
}

static ma_result ma_resource_manager_process_job__free_data_buffer(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);
    MA_ASSERT(pJob->freeDataBuffer.pDataBuffer        != NULL);
    MA_ASSERT(pJob->freeDataBuffer.pDataBuffer->pNode != NULL);
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pJob->freeDataBuffer.pDataBuffer->pNode) == MA_UNAVAILABLE);

    if (pJob->order != pJob->freeDataBuffer.pDataBuffer->pNode->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    ma_resource_manager_data_buffer_uninit_internal(pJob->freeDataBuffer.pDataBuffer);

    /* The event needs to be signalled last. */
    if (pJob->freeDataBuffer.pNotification != NULL) {
        ma_async_notification_signal(pJob->freeDataBuffer.pNotification, MA_NOTIFICATION_COMPLETE);
    }

    /*c89atomic_fetch_add_32(&pJob->freeDataBuffer.pDataBuffer->pNode->executionPointer, 1);*/
    return MA_SUCCESS;
}

static ma_result ma_resource_manager_process_job__page_data_buffer(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 pageSizeInFrames;
    ma_uint64 framesRead;
    void* pRunningData;
    ma_job jobCopy;
    ma_resource_manager_data_buffer* pDataBuffer;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);

    pDataBuffer = pJob->pageDataBuffer.pDataBuffer;

    /* Don't do any more decoding if the data buffer has started the uninitialization process. */
    if (ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_BUSY) {
        return MA_INVALID_OPERATION;
    }

    if (pJob->order != pDataBuffer->pNode->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    /* We're going to base everything off the original job. */
    jobCopy = *pJob;

    /* We need to know the size of a page in frames to know how many frames to decode. */
    pageSizeInFrames = MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS * (jobCopy.pageDataBuffer.pDecoder->outputSampleRate/1000);

    /* If the total length is unknown we may need to expand the size of the buffer. */
    if (jobCopy.pageDataBuffer.isUnknownLength == MA_TRUE) {
        ma_uint64 requiredSize = (jobCopy.pageDataBuffer.decodedFrameCount + pageSizeInFrames) * ma_get_bytes_per_frame(jobCopy.pageDataBuffer.pDecoder->outputFormat, jobCopy.pageDataBuffer.pDecoder->outputChannels);
        if (requiredSize <= MA_SIZE_MAX) {
            if (requiredSize > jobCopy.pageDataBuffer.dataSizeInBytes) {
                size_t newSize = (size_t)ma_max(requiredSize, jobCopy.pageDataBuffer.dataSizeInBytes * 2);
                void *pNewData = ma__realloc_from_callbacks(jobCopy.pageDataBuffer.pData, newSize, jobCopy.pageDataBuffer.dataSizeInBytes, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
                if (pNewData != NULL) {
                    jobCopy.pageDataBuffer.pData           = pNewData;
                    jobCopy.pageDataBuffer.dataSizeInBytes = newSize;
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
        pRunningData = ma_offset_ptr(jobCopy.pageDataBuffer.pData, jobCopy.pageDataBuffer.decodedFrameCount * ma_get_bytes_per_frame(jobCopy.pageDataBuffer.pDecoder->outputFormat, jobCopy.pageDataBuffer.pDecoder->outputChannels));

        framesRead = ma_decoder_read_pcm_frames(jobCopy.pageDataBuffer.pDecoder, pRunningData, pageSizeInFrames);
        if (framesRead < pageSizeInFrames) {
            result = MA_AT_END;
        }

        /* If the total length is known we can increment out decoded frame count. Otherwise it needs to be left at 0 until the last page is decoded. */
        if (jobCopy.pageDataBuffer.isUnknownLength == MA_FALSE) {
            pDataBuffer->pNode->data.decoded.decodedFrameCount += framesRead;
        }

        /*
        If there's more to decode, post a job to keep decoding. Note that we always increment the decoded frame count in the copy of the job because it'll be
        referenced below and we'll need to know the new frame count.
        */
        jobCopy.pageDataBuffer.decodedFrameCount += framesRead;

        if (result != MA_AT_END) {
            jobCopy.order = ma_resource_manager_data_buffer_next_execution_order(pDataBuffer);   /* We need a fresh execution order. */
            result = ma_resource_manager_post_job(pResourceManager, &jobCopy);
        }
    }

    /*
    The result will be MA_SUCCESS if another page is in the queue for decoding. Otherwise it will be set to MA_AT_END if the end has been reached or
    any other result code if some other error occurred. If we are not decoding another page we need to free the decoder and close the file.
    */
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(jobCopy.pageDataBuffer.pDecoder);
        ma__free_from_callbacks(jobCopy.pageDataBuffer.pDecoder, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODER*/);

        /* When the length is unknown we were doubling the size of the buffer each time we needed more data. Let's try reducing this by doing a final realloc(). */
        if (jobCopy.pageDataBuffer.isUnknownLength) {
            ma_uint64 newSizeInBytes = jobCopy.pageDataBuffer.decodedFrameCount * ma_get_bytes_per_frame(pDataBuffer->pNode->data.decoded.format, pDataBuffer->pNode->data.decoded.channels);
            void* pNewData = ma__realloc_from_callbacks(jobCopy.pageDataBuffer.pData, (size_t)newSizeInBytes, jobCopy.pageDataBuffer.dataSizeInBytes, &pResourceManager->config.allocationCallbacks);
            if (pNewData != NULL) {
                jobCopy.pageDataBuffer.pData = pNewData;
                jobCopy.pageDataBuffer.dataSizeInBytes = (size_t)newSizeInBytes;    /* <-- Don't really need to set this, but I think it's good practice. */
            }
        }

        /*
        We can now set the frame counts appropriately. We want to set the frame count regardless of whether or not it had a known length just in case we have
        a weird situation where the frame count an opening time was different to the final count we got after reading.
        */
        pDataBuffer->pNode->data.decoded.pData      = jobCopy.pageDataBuffer.pData;
        pDataBuffer->pNode->data.decoded.frameCount = jobCopy.pageDataBuffer.decodedFrameCount;

        /*
        decodedFrameCount is what other threads will use to determine whether or not data is available. We must ensure pData and frameCount
        is set *before* setting the number of available frames. This way, the other thread need only check if decodedFrameCount > 0, in
        which case it can assume pData and frameCount are valid.
        */
        c89atomic_thread_fence(c89atomic_memory_order_seq_cst);
        pDataBuffer->pNode->data.decoded.decodedFrameCount = jobCopy.pageDataBuffer.decodedFrameCount;


        /* If we reached the end we need to treat it as successful. */
        if (result == MA_AT_END) {
            result  = MA_SUCCESS;
        }

        /* If it was an unknown length, we can finally initialize the connector. For sounds of a known length, the connector was initialized when the first page was decoded in MA_JOB_LOAD_DATA_BUFFER. */
        if (jobCopy.pageDataBuffer.isUnknownLength) {
            result = ma_resource_manager_data_buffer_init_connector(pDataBuffer, pJob->pageDataBuffer.pCompletedNotification);
        }

        /* We need to set the status of the page so other things can know about it. We can only change the status away from MA_BUSY. If it's anything else it cannot be changed. */
        c89atomic_compare_and_swap_32(&pDataBuffer->pNode->result, MA_BUSY, result);

        /* We need to signal an event to indicate that we're done. */
        if (jobCopy.pageDataBuffer.pCompletedNotification != NULL) {
            ma_async_notification_signal(jobCopy.pageDataBuffer.pCompletedNotification, MA_NOTIFICATION_COMPLETE);
        }
    }

    c89atomic_fetch_add_32(&pDataBuffer->pNode->executionPointer, 1);
    return result;
}


static ma_result ma_resource_manager_process_job__load_data_stream(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    ma_result result = MA_SUCCESS;
    ma_decoder_config decoderConfig;
    ma_uint32 pageBufferSizeInBytes;
    ma_resource_manager_data_stream* pDataStream;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);

    pDataStream = pJob->loadDataStream.pDataStream;

    if (ma_resource_manager_data_stream_result(pDataStream) != MA_BUSY) {
        result = MA_INVALID_OPERATION;  /* Most likely the data stream is being uninitialized. */
        goto done;
    }

    if (pJob->order != pDataStream->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    /* We need to initialize the decoder first so we can determine the size of the pages. */
    decoderConfig = ma_decoder_config_init(pResourceManager->config.decodedFormat, pResourceManager->config.decodedChannels, pResourceManager->config.decodedSampleRate);
    decoderConfig.allocationCallbacks = pResourceManager->config.allocationCallbacks;

    result = ma_decoder_init_vfs(pResourceManager->config.pVFS, pJob->loadDataStream.pFilePath, &decoderConfig, &pDataStream->decoder);
    if (result != MA_SUCCESS) {
        goto done;
    }

    /* Retrieve the total length of the file before marking the decoder are loaded. */
    pDataStream->totalLengthInPCMFrames = ma_decoder_get_length_in_pcm_frames(&pDataStream->decoder);

    /*
    Only mark the decoder as initialized when the length of the decoder has been retrieved because that can possibly require a scan over the whole file
    and we don't want to have another thread trying to access the decoder while it's scanning.
    */
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
    ma_resource_manager_data_stream_fill_pages(pDataStream);

    /* And now we're done. We want to make sure the result is MA_SUCCESS. */
    result = MA_SUCCESS;

done:
    ma__free_from_callbacks(pJob->loadDataStream.pFilePath, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_TRANSIENT_STRING*/);

    /* We can only change the status away from MA_BUSY. If it's set to anything else it means an error has occurred somewhere or the uninitialization process has started (most likely). */
    c89atomic_compare_and_swap_32(&pDataStream->result, MA_BUSY, result);

    /* Only signal the other threads after the result has been set just for cleanliness sake. */
    if (pJob->loadDataStream.pNotification != NULL) {
        ma_async_notification_signal(pJob->loadDataStream.pNotification, MA_NOTIFICATION_INIT);
        ma_async_notification_signal(pJob->loadDataStream.pNotification, MA_NOTIFICATION_COMPLETE);
    }

    c89atomic_fetch_add_32(&pDataStream->executionPointer, 1);
    return result;
}

static ma_result ma_resource_manager_process_job__free_data_stream(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    ma_resource_manager_data_stream* pDataStream;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);

    pDataStream = pJob->freeDataStream.pDataStream;
    MA_ASSERT(pDataStream != NULL);

    /* If our status is not MA_UNAVAILABLE we have a bug somewhere. */
    MA_ASSERT(ma_resource_manager_data_stream_result(pDataStream) == MA_UNAVAILABLE);

    if (pJob->order != pDataStream->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    if (pDataStream->isDecoderInitialized) {
        ma_decoder_uninit(&pDataStream->decoder);
    }

    if (pDataStream->pPageData != NULL) {
        ma__free_from_callbacks(pDataStream->pPageData, &pResourceManager->config.allocationCallbacks/*, MA_ALLOCATION_TYPE_DECODED_BUFFER*/);
        pDataStream->pPageData = NULL;  /* Just in case... */
    }

    /* The event needs to be signalled last. */
    if (pJob->freeDataStream.pNotification != NULL) {
        ma_async_notification_signal(pJob->freeDataStream.pNotification, MA_NOTIFICATION_COMPLETE);
    }

    /*c89atomic_fetch_add_32(&pDataStream->executionPointer, 1);*/
    return MA_SUCCESS;
}

static ma_result ma_resource_manager_process_job__page_data_stream(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    ma_result result = MA_SUCCESS;
    ma_resource_manager_data_stream* pDataStream;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);

    pDataStream = pJob->pageDataStream.pDataStream;
    MA_ASSERT(pDataStream != NULL);

    /* For streams, the status should be MA_SUCCESS. */
    if (ma_resource_manager_data_stream_result(pDataStream) != MA_SUCCESS) {
        result = MA_INVALID_OPERATION;
        goto done;
    }

    if (pJob->order != pDataStream->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    ma_resource_manager_data_stream_fill_page(pDataStream, pJob->pageDataStream.pageIndex);

done:
    c89atomic_fetch_add_32(&pDataStream->executionPointer, 1);
    return result;
}

static ma_result ma_resource_manager_process_job__seek_data_stream(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    ma_result result = MA_SUCCESS;
    ma_resource_manager_data_stream* pDataStream;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);

    pDataStream = pJob->seekDataStream.pDataStream;
    MA_ASSERT(pDataStream != NULL);

    /* For streams the status should be MA_SUCCESS for this to do anything. */
    if (ma_resource_manager_data_stream_result(pDataStream) != MA_SUCCESS || pDataStream->isDecoderInitialized == MA_FALSE) {
        result = MA_INVALID_OPERATION;
        goto done;
    }

    if (pJob->order != pDataStream->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    /*
    With seeking we just assume both pages are invalid and the relative frame cursor at at position 0. This is basically exactly the same as loading, except
    instead of initializing the decoder, we seek to a frame.
    */
    ma_decoder_seek_to_pcm_frame(&pDataStream->decoder, pJob->seekDataStream.frameIndex);

    /* After seeking we'll need to reload the pages. */
    ma_resource_manager_data_stream_fill_pages(pDataStream);

    /* We need to let the public API know that we're done seeking. */
    c89atomic_fetch_sub_32(&pDataStream->seekCounter, 1);

done:
    c89atomic_fetch_add_32(&pDataStream->executionPointer, 1);
    return result;
}

MA_API ma_result ma_resource_manager_process_job(ma_resource_manager* pResourceManager, ma_job* pJob)
{
    if (pResourceManager == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    switch (pJob->toc.code)
    {
        /* Data Buffer */
        case MA_JOB_LOAD_DATA_BUFFER: return ma_resource_manager_process_job__load_data_buffer(pResourceManager, pJob);
        case MA_JOB_FREE_DATA_BUFFER: return ma_resource_manager_process_job__free_data_buffer(pResourceManager, pJob);
        case MA_JOB_PAGE_DATA_BUFFER: return ma_resource_manager_process_job__page_data_buffer(pResourceManager, pJob);

        /* Data Stream */
        case MA_JOB_LOAD_DATA_STREAM: return ma_resource_manager_process_job__load_data_stream(pResourceManager, pJob);
        case MA_JOB_FREE_DATA_STREAM: return ma_resource_manager_process_job__free_data_stream(pResourceManager, pJob);
        case MA_JOB_PAGE_DATA_STREAM: return ma_resource_manager_process_job__page_data_stream(pResourceManager, pJob);
        case MA_JOB_SEEK_DATA_STREAM: return ma_resource_manager_process_job__seek_data_stream(pResourceManager, pJob);

        default: break;
    }

    /* Getting here means we don't know what the job code is and cannot do anything with it. */
    return MA_INVALID_OPERATION;
}

MA_API ma_result ma_resource_manager_process_next_job(ma_resource_manager* pResourceManager)
{
    ma_result result;
    ma_job job;

    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    /* This will return MA_CANCELLED if the next job is a quit job. */
    result = ma_resource_manager_next_job(pResourceManager, &job);
    if (result != MA_SUCCESS) {
        return result;
    }

    return ma_resource_manager_process_job(pResourceManager, &job);
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


static ma_result ma_panner_effect__on_process_pcm_frames(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_panner* pPanner = (ma_panner*)pEffect;
    ma_result result;
    ma_uint64 frameCount;

    (void)inputStreamCount;

    /* The panner has a 1:1 relationship between input and output frame counts. */
    frameCount = ma_min(*pFrameCountIn, *pFrameCountOut);

    /* Only the first input stream is considered. Extra streams are ignored. */
    result = ma_panner_process_pcm_frames(pPanner, pFramesOut, ppFramesIn[0], ma_min(*pFrameCountIn, *pFrameCountOut));

    *pFrameCountIn  = frameCount;
    *pFrameCountOut = frameCount;

    return result;
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
        if (pFramesOut == pFramesIn) {
            for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
                pFramesOut[iFrame*2 + 0] = pFramesIn[iFrame*2 + 0] * factor;
            }
        } else {
            for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
                pFramesOut[iFrame*2 + 0] = pFramesIn[iFrame*2 + 0] * factor;
                pFramesOut[iFrame*2 + 1] = pFramesIn[iFrame*2 + 1];
            }
        }
    } else {
        float factor = 1.0f + pan;
        if (pFramesOut == pFramesIn) {
            for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
                pFramesOut[iFrame*2 + 1] = pFramesIn[iFrame*2 + 1] * factor;
            }
        } else {
            for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
                pFramesOut[iFrame*2 + 0] = pFramesIn[iFrame*2 + 0];
                pFramesOut[iFrame*2 + 1] = pFramesIn[iFrame*2 + 1] * factor;
            }
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

        return;
    }

    switch (format) {
        case ma_format_f32: ma_stereo_balance_pcm_frames_f32((float*)pFramesOut, (float*)pFramesIn, frameCount, pan); break;

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

        return;
    }

    switch (format) {
        case ma_format_f32: ma_stereo_pan_pcm_frames_f32((float*)pFramesOut, (float*)pFramesIn, frameCount, pan); break;

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


static ma_result ma_spatializer_effect__on_process_pcm_frames(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_spatializer* pSpatializer = (ma_spatializer*)pEffect;
    ma_result result;
    ma_uint64 frameCount;

    (void)inputStreamCount;

    /* The spatializer has a 1:1 relationship between input and output frame counts. */
    frameCount = ma_min(*pFrameCountIn, *pFrameCountOut);

    /* Only the first input stream is considered. Extra streams are ignored. */
    result = ma_spatializer_process_pcm_frames(pSpatializer, pFramesOut, ppFramesIn[0], frameCount);

    *pFrameCountIn  = frameCount;
    *pFrameCountOut = frameCount;

    return result;
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
    if (pSpatializer == NULL || pFramesOut == NULL || pFramesIn == NULL) {
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



MA_API ma_fader_config ma_fader_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_fader_config config;

    MA_ZERO_OBJECT(&config);
    config.format     = format;
    config.channels   = channels;
    config.sampleRate = sampleRate;
    
    return config;
}



static ma_result ma_fader_effect__on_process_pcm_frames(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_fader* pFader = (ma_fader*)pEffect;
    ma_result result;
    ma_uint64 frameCount;

    (void)inputStreamCount;

    /* The fader has a 1:1 relationship between input and output frame counts. */
    frameCount = ma_min(*pFrameCountIn, *pFrameCountOut);

    result = ma_fader_process_pcm_frames(pFader, pFramesOut, ppFramesIn[0], frameCount);

    *pFrameCountIn  = frameCount;
    *pFrameCountOut = frameCount;

    return result;
}

static ma_result ma_fader_effect__on_get_data_format(ma_effect* pEffect, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    return ma_fader_get_data_format((ma_fader*)pEffect, pFormat, pChannels, pSampleRate);
}

MA_API ma_result ma_fader_init(const ma_fader_config* pConfig, ma_fader* pFader)
{
    if (pFader == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pFader);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Only f32 is supported for now. */
    if (pConfig->format != ma_format_f32) {
        return MA_INVALID_ARGS;
    }

    pFader->effect.onProcessPCMFrames            = ma_fader_effect__on_process_pcm_frames;
    pFader->effect.onGetRequiredInputFrameCount  = NULL;
    pFader->effect.onGetExpectedOutputFrameCount = NULL;
    pFader->effect.onGetInputDataFormat          = ma_fader_effect__on_get_data_format;
    pFader->effect.onGetOutputDataFormat         = ma_fader_effect__on_get_data_format;

    pFader->config         = *pConfig;
    pFader->volumeBeg      = 1;
    pFader->volumeEnd      = 1;
    pFader->lengthInFrames = 0;
    pFader->cursorInFrames = 0;

    return MA_SUCCESS;
}

MA_API ma_result ma_fader_process_pcm_frames(ma_fader* pFader, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    if (pFader == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Optimized path if volumeBeg and volumeEnd are equal. */
    if (pFader->volumeBeg == pFader->volumeEnd) {
        if (pFader->volumeBeg == 1) {
            /* Straight copy. */
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, pFader->config.format, pFader->config.channels);
        } else {
            /* Copy with volume. */
            ma_volume_and_clip_pcm_frames(pFramesOut, pFramesIn, frameCount, pFader->config.format, pFader->config.channels, pFader->volumeEnd);
        }
    } else {
        /* Slower path. Volumes are different, so may need to do an interpolation. */
        if (pFader->cursorInFrames >= pFader->lengthInFrames) {
            /* Fast path. We've gone past the end of the fade period so just apply the end volume to all samples. */
            ma_volume_and_clip_pcm_frames(pFramesOut, pFramesIn, frameCount, pFader->config.format, pFader->config.channels, pFader->volumeEnd);
        } else {
            /* Slow path. This is where we do the actual fading. */
            ma_uint64 iFrame;
            ma_uint32 iChannel;

            /* For now we only support f32. Support for other formats will be added later. */
            if (pFader->config.format == ma_format_f32) {
                const float* pFramesInF32  = (const float*)pFramesIn;
                /* */ float* pFramesOutF32 = (      float*)pFramesOut;

                for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    float a = ma_min(pFader->cursorInFrames + iFrame, pFader->lengthInFrames) / (float)pFader->lengthInFrames;
                    float volume = ma_mix_f32_fast(pFader->volumeBeg, pFader->volumeEnd, a);

                    for (iChannel = 0; iChannel < pFader->config.channels; iChannel += 1) {
                        pFramesOutF32[iFrame*pFader->config.channels + iChannel] = pFramesInF32[iFrame*pFader->config.channels + iChannel] * volume;
                    }
                }
            } else {
                return MA_NOT_IMPLEMENTED;
            }
        }
    }

    pFader->cursorInFrames += frameCount;

    return MA_SUCCESS;
}

MA_API ma_result ma_fader_get_data_format(const ma_fader* pFader, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    if (pFader == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pFormat != NULL) {
        *pFormat = pFader->config.format;
    }

    if (pChannels != NULL) {
        *pChannels = pFader->config.channels;
    }

    if (pSampleRate != NULL) {
        *pSampleRate = pFader->config.sampleRate;
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_fader_set_fade(ma_fader* pFader, float volumeBeg, float volumeEnd, ma_uint64 lengthInFrames)
{
    if (pFader == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If the volume is negative, use current volume. */
    if (volumeBeg < 0) {
        ma_fader_get_current_volume(pFader, &volumeBeg);
    }

    pFader->volumeBeg      = volumeBeg;
    pFader->volumeEnd      = volumeEnd;
    pFader->lengthInFrames = lengthInFrames;
    pFader->cursorInFrames = 0; /* Reset cursor. */

    return MA_SUCCESS;
}

MA_API ma_result ma_fader_get_current_volume(ma_fader* pFader, float* pVolume)
{
    if (pFader == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The current volume depends on the position of the cursor. */
    if (pFader->cursorInFrames <= 0) {
        *pVolume = pFader->volumeBeg;
    } else if (pFader->cursorInFrames >= pFader->lengthInFrames) {
        *pVolume = pFader->volumeEnd;
    } else {
        /* The cursor is somewhere inside the fading period. We can figure this out with a simple linear interpoluation between volumeBeg and volumeEnd based on our cursor position. */
        *pVolume = ma_mix_f32_fast(pFader->volumeBeg, pFader->volumeEnd, pFader->cursorInFrames / (float)pFader->lengthInFrames);
    }

    return MA_SUCCESS;
}


/**************************************************************************************************************************************************************

Engine

**************************************************************************************************************************************************************/
#define MA_SEEK_TARGET_NONE (~(ma_uint64)0)

static void ma_engine_effect__update_resampler_for_pitching(ma_engine_effect* pEngineEffect)
{
    MA_ASSERT(pEngineEffect != NULL);

    if (pEngineEffect->oldPitch != pEngineEffect->pitch) {
        pEngineEffect->oldPitch  = pEngineEffect->pitch;
        ma_data_converter_set_rate_ratio(&pEngineEffect->converter, pEngineEffect->pitch);
    }
}

static ma_result ma_engine_effect__on_process_pcm_frames__no_pre_effect_no_pitch(ma_engine_effect* pEngineEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_uint64 frameCount;
    ma_effect* pSubEffect[32];  /* The list of effects to be executed. Increase the size of this buffer if the number of sub-effects is exceeded. */
    ma_uint32 subEffectCount = 0;

    /*
    This will be called if either there is no pre-effect nor pitch shift, or the pre-effect and pitch shift have already been processed. In this case it's allowed for
    pFramesIn to be equal to pFramesOut as from here on we support in-place processing. Also, the input and output frame counts should always be equal.
    */
    frameCount = ma_min(*pFrameCountIn, *pFrameCountOut);

    /*
    This is a little inefficient, but it simplifies maintenance of this function a lot as we add new sub-effects. We are going to build a list of effects
    and then just run a loop to execute them. Some sub-effects must always be executed for state-updating reasons, but others can be skipped entirely.
    */

    /* Panning. This is a no-op when the engine has only 1 channel or the pan is 0. */
    if (pEngineEffect->pEngine->channels == 1 || pEngineEffect->panner.pan == 0) {
        /* Fast path. No panning. */
    } else {
        /* Slow path. Panning required. */
        pSubEffect[subEffectCount++] = &pEngineEffect->panner;
    }

    /* Spatialization. */
    if (pEngineEffect->isSpatial == MA_FALSE) {
        /* Fast path. No spatialization. */
    } else {
        /* Slow path. Spatialization required. */
        pSubEffect[subEffectCount++] = &pEngineEffect->spatializer;
    }

    /* Fader. Don't need to bother running this is the fade volumes are both 1. */
    if (pEngineEffect->fader.volumeBeg == 1 && pEngineEffect->fader.volumeEnd == 1) {
        /* Fast path. No fading. */
    } else {
        /* Slow path. Fading required. */
        pSubEffect[subEffectCount++] = &pEngineEffect->fader;
    }


    /* We've built our list of effects, now we just need to execute them. */
    if (subEffectCount == 0) {
        /* Fast path. No sub-effects. */
        if (pFramesIn == pFramesOut) {
            /* Fast path. No-op. */
        } else {
            /* Slow path. Copy. */
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, pEngineEffect->pEngine->format, pEngineEffect->pEngine->channels);
        }
    } else {
        /* Slow path. We have sub-effects to execute. The first effect reads from pFramesIn and then outputs to pFramesOut. The remaining read and write to pFramesOut in-place. */
        ma_uint32 iSubEffect = 0;
        for (iSubEffect = 0; iSubEffect < subEffectCount; iSubEffect += 1) {
            ma_uint64 frameCountIn  = frameCount;
            ma_uint64 frameCountOut = frameCount;

            ma_effect_process_pcm_frames(pSubEffect[iSubEffect], pFramesIn, &frameCountIn, pFramesOut, &frameCountOut);

            /* The first effect will have written to the output buffer which means we can now operate on the output buffer in-place. */
            if (iSubEffect == 0) {
                pFramesIn = pFramesOut;
            }
        }
    }

    
    /* We're done. */
    *pFrameCountIn  = frameCount;
    *pFrameCountOut = frameCount;

    return MA_SUCCESS;
}

static ma_result ma_engine_effect__on_process_pcm_frames__no_pre_effect(ma_engine_effect* pEngineEffect, const void* pFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_bool32 isPitchingRequired = MA_FALSE;

    if (pEngineEffect->converter.hasResampler && pEngineEffect->pitch != 1) {
        isPitchingRequired = MA_TRUE;
    }

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

        result = ma_effect_process_pcm_frames_with_conversion(pEngineEffect->pPreEffect, 1, &pRunningFramesIn, &frameCountInThisIteration, preEffectOutBuffer, &frameCountOutThisIteration, effectFormat, effectChannels, effectFormat, effectChannels);
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

        totalFramesProcessedOut += frameCountOutThisIteration;
    }


    *pFrameCountIn  = totalFramesProcessedIn;
    *pFrameCountOut = totalFramesProcessedOut;

    return MA_SUCCESS;
}

static ma_result ma_engine_effect__on_process_pcm_frames(ma_effect* pEffect, ma_uint32 inputStreamCount, const void** ppFramesIn, ma_uint64* pFrameCountIn, void* pFramesOut, ma_uint64* pFrameCountOut)
{
    ma_engine_effect* pEngineEffect = (ma_engine_effect*)pEffect;
    ma_result result;

    MA_ASSERT(pEffect != NULL);

    /* Only the first input stream is considered. Extra streams are ignored. */
    (void)inputStreamCount;

    /* Make sure we update the resampler to take any pitch changes into account. Not doing this will result in incorrect frame counts being returned. */
    ma_engine_effect__update_resampler_for_pitching(pEngineEffect);

    /* Optimized path for when there is no pre-effect. */
    if (pEngineEffect->pPreEffect == NULL) {
        result = ma_engine_effect__on_process_pcm_frames__no_pre_effect(pEngineEffect, ppFramesIn[0], pFrameCountIn, pFramesOut, pFrameCountOut);
    } else {
        result = ma_engine_effect__on_process_pcm_frames__general(pEngineEffect, ppFramesIn[0], pFrameCountIn, pFramesOut, pFrameCountOut);
    }

    pEngineEffect->timeInFrames += *pFrameCountIn;

    return result;
}

static ma_uint64 ma_engine_effect__on_get_required_input_frame_count(ma_effect* pEffect, ma_uint64 outputFrameCount)
{
    ma_engine_effect* pEngineEffect = (ma_engine_effect*)pEffect;
    ma_uint64 inputFrameCount;

    MA_ASSERT(pEffect != NULL);

    /* Make sure we update the resampler to take any pitch changes into account. Not doing this will result in incorrect frame counts being returned. */
    ma_engine_effect__update_resampler_for_pitching(pEngineEffect);

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

    /* Make sure we update the resampler to take any pitch changes into account. Not doing this will result in incorrect frame counts being returned. */
    ma_engine_effect__update_resampler_for_pitching(pEngineEffect);

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
        return ma_effect_get_input_data_format(pEngineEffect->pPreEffect, pFormat, pChannels, pSampleRate);
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
    ma_fader_config faderConfig;
    ma_data_converter_config converterConfig;

    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pEffect != NULL);

    MA_ZERO_OBJECT(pEffect);

    pEffect->baseEffect.onProcessPCMFrames            = ma_engine_effect__on_process_pcm_frames;
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

    faderConfig = ma_fader_config_init(pEngine->format, pEngine->channels, pEngine->sampleRate);
    result = ma_fader_init(&faderConfig, &pEffect->fader);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to create the fader. */
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

/* Not used at the moment, but might re-enable this later. */
#if 0
static ma_bool32 ma_engine_effect_is_passthrough(ma_engine_effect* pEffect)
{
    MA_ASSERT(pEffect != NULL);

    /* A pre-effect will require processing. */
    if (pEffect->pPreEffect != NULL) {
        return MA_FALSE;
    }

    /* If pitch shifting we'll need to do processing through the resampler. */
    if (pEffect->pitch != 1) {
        return MA_FALSE;
    }

    return MA_TRUE;
}
#endif

static ma_result ma_engine_effect_set_time(ma_engine_effect* pEffect, ma_uint64 timeInFrames)
{
    MA_ASSERT(pEffect != NULL);

    pEffect->timeInFrames = timeInFrames;

    return MA_SUCCESS;
}


static MA_INLINE ma_result ma_sound_stop_internal(ma_sound* pSound);
static MA_INLINE ma_result ma_sound_group_stop_internal(ma_sound_group* pGroup);

MA_API ma_engine_config ma_engine_config_init_default(void)
{
    ma_engine_config config;
    MA_ZERO_OBJECT(&config);

    config.format = ma_format_f32;

    return config;
}


static ma_sound* ma_sound_group_first_sound(ma_sound_group* pGroup)
{
    return (ma_sound*)c89atomic_load_ptr(&pGroup->pFirstSoundInGroup);
}

static ma_sound* ma_sound_next_sound_in_group(ma_sound* pSound)
{
    return (ma_sound*)c89atomic_load_ptr(&pSound->pNextSoundInGroup);
}

static ma_sound* ma_sound_prev_sound_in_group(ma_sound* pSound)
{
    return (ma_sound*)c89atomic_load_ptr(&pSound->pPrevSoundInGroup);
}

static ma_bool32 ma_sound_is_mixing(const ma_sound* pSound)
{
    MA_ASSERT(pSound != NULL);
    return c89atomic_load_32((ma_bool32*)&pSound->isMixing);
}

static void ma_sound_mix_wait(ma_sound* pSound)
{
    /* This function is only safe when the sound is not flagged as playing. */
    MA_ASSERT(ma_sound_is_playing(pSound) == MA_FALSE);

    /* Just do a basic spin wait. */
    while (ma_sound_is_mixing(pSound)) {
        ma_yield();
    }
}

static void ma_engine_mix_sound_internal(ma_engine* pEngine, ma_sound_group* pGroup, ma_sound* pSound, ma_uint64 frameCount)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 framesProcessed;

    (void)pEngine;  /* Unused at the moment. */

    /* Don't do anything if we're not playing. */
    if (ma_sound_is_playing(pSound) == MA_FALSE) {
        return;
    }

    /* If we're marked at the end we need to stop the sound and do nothing. */
    if (ma_sound_at_end(pSound)) {
        ma_sound_stop_internal(pSound);
        return;
    }

    /* If we're seeking, do so now before reading. */
    if (pSound->seekTarget != MA_SEEK_TARGET_NONE) {
        ma_data_source_seek_to_pcm_frame(pSound->pDataSource, pSound->seekTarget);
                
        /* Any time-dependant effects need to have their times updated. */
        ma_engine_effect_set_time(&pSound->effect, pSound->seekTarget);

        pSound->seekTarget  = MA_SEEK_TARGET_NONE;
    }

    /* If the sound is being delayed we don't want to mix anything, nor do we want to advance time forward from the perspective of the data source. */
    if ((pSound->runningTimeInEngineFrames + frameCount) > pSound->startDelayInEngineFrames) {
        /* We're not delayed so we can mix or seek. In order to get frame-exact playback timing we need to start mixing from an offset. */
        ma_uint64 currentTimeInFrames;
        ma_uint64 offsetInFrames;

        offsetInFrames = 0;
        if (pSound->startDelayInEngineFrames > pSound->runningTimeInEngineFrames) {
            offsetInFrames = pSound->startDelayInEngineFrames - pSound->runningTimeInEngineFrames;
        }

        MA_ASSERT(offsetInFrames < frameCount);

        /*
        An obvious optimization is to skip mixing if the sound is not audible. The problem with this, however, is that the effect may need to update some
        internal state such as timing information for things like fades, delays, echos, etc. We're going to always mix the sound if it's active and trust
        the mixer to optimize the volume = 0 case, and let the effect do it's own internal optimizations in non-audible cases.
        */
        result = ma_mixer_mix_data_source(&pGroup->mixer, pSound->pDataSource, offsetInFrames, (frameCount - offsetInFrames), &framesProcessed, pSound->volume, &pSound->effect, ma_sound_is_looping(pSound));
                
        /* If we reached the end of the sound we'll want to mark it as at the end and stop it. This should never be returned for looping sounds. */
        if (result == MA_AT_END) {
            c89atomic_exchange_32(&pSound->atEnd, MA_TRUE); /* This will be set to false in ma_sound_start(). */
        }

        /*
        For the benefit of the main effect we need to ensure the local time is updated explicitly. This is required for allowing time-based effects to
        support loop transitions properly.
        */
        result = ma_sound_get_cursor_in_pcm_frames(pSound, &currentTimeInFrames);
        if (result == MA_SUCCESS) {
            ma_engine_effect_set_time(&pSound->effect, currentTimeInFrames);
        }

        pSound->runningTimeInEngineFrames += offsetInFrames + framesProcessed;
    } else {
        /* The sound hasn't started yet. Just keep advancing time forward, but leave the data source alone. */
        pSound->runningTimeInEngineFrames += frameCount;
    }

    /* If we're stopping after a delay we need to check if the delay has expired and if so, stop for real. */
    if (pSound->stopDelayInEngineFramesRemaining > 0) {
        if (pSound->stopDelayInEngineFramesRemaining >= frameCount) {
            pSound->stopDelayInEngineFramesRemaining -= frameCount;
        } else {
            pSound->stopDelayInEngineFramesRemaining = 0;
        }

        /* Stop the sound if the delay has been reached. */
        if (pSound->stopDelayInEngineFramesRemaining == 0) {
            ma_sound_stop_internal(pSound);
        }
    }
}

static void ma_engine_mix_sound(ma_engine* pEngine, ma_sound_group* pGroup, ma_sound* pSound, ma_uint64 frameCount)
{
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pGroup  != NULL);
    MA_ASSERT(pSound  != NULL);

    c89atomic_exchange_32(&pSound->isMixing, MA_TRUE);  /* This must be done before checking the isPlaying state. */
    {
        ma_engine_mix_sound_internal(pEngine, pGroup, pSound, frameCount);
    }
    c89atomic_exchange_32(&pSound->isMixing, MA_FALSE);
}

static void ma_engine_mix_sound_group(ma_engine* pEngine, ma_sound_group* pGroup, void* pFramesOut, ma_uint64 frameCount)
{
    ma_result result;
    ma_mixer* pParentMixer = NULL;
    ma_uint64 frameCountOut;
    ma_uint64 frameCountIn;
    ma_uint64 totalFramesProcessed;
    ma_sound_group* pNextChildGroup;
    ma_sound* pNextSound;

    MA_ASSERT(pEngine    != NULL);
    MA_ASSERT(pGroup     != NULL);
    MA_ASSERT(frameCount != 0);

    /* Don't do anything if we're not playing. */
    if (ma_sound_group_is_playing(pGroup) == MA_FALSE) {
        return;
    }

    if (pGroup->pParent != NULL) {
        pParentMixer = &pGroup->pParent->mixer;
    }

    frameCountOut = frameCount;
    frameCountIn  = frameCount;

    /* If the group is being delayed we don't want to mix anything. */
    if ((pGroup->runningTimeInEngineFrames + frameCount) > pGroup->startDelayInEngineFrames) {
        /* We're not delayed so we can mix or seek. In order to get frame-exact playback timing we need to start mixing from an offset. */
        ma_uint64 offsetInFrames = 0;
        if (pGroup->startDelayInEngineFrames > pGroup->runningTimeInEngineFrames) {
            offsetInFrames = pGroup->startDelayInEngineFrames - pGroup->runningTimeInEngineFrames;
        }

        MA_ASSERT(offsetInFrames < frameCount);

        /* We need to loop here to ensure we fill every frame. This won't necessarily be able to be done in one iteration due to resampling within the effect. */
        totalFramesProcessed = 0;
        while (totalFramesProcessed < (frameCount - offsetInFrames)) {
            frameCountOut = frameCount - offsetInFrames - totalFramesProcessed;
            frameCountIn  = frameCount - offsetInFrames - totalFramesProcessed;

            /* Before can mix the group we need to mix it's children. */
            result = ma_mixer_begin(&pGroup->mixer, pParentMixer, &frameCountOut, &frameCountIn);
            if (result != MA_SUCCESS) {
                break;
            }

            /* Child groups need to be mixed based on the parent's input frame count. */
            for (pNextChildGroup = pGroup->pFirstChild; pNextChildGroup != NULL; pNextChildGroup = pNextChildGroup->pNextSibling) {
                ma_engine_mix_sound_group(pEngine, pNextChildGroup, NULL, frameCountIn);
            }

            /* Sounds in the group can now be mixed. This is where the real mixing work is done. */
            for (pNextSound = ma_sound_group_first_sound(pGroup); pNextSound != NULL; pNextSound = ma_sound_next_sound_in_group(pNextSound)) {
                ma_engine_mix_sound(pEngine, pGroup, pNextSound, frameCountIn);
            }

            /* Now mix into the parent. */
            result = ma_mixer_end(&pGroup->mixer, pParentMixer, pFramesOut, offsetInFrames + totalFramesProcessed);
            if (result != MA_SUCCESS) {
                break;
            }

            totalFramesProcessed += frameCountOut;
        }

        pGroup->runningTimeInEngineFrames += offsetInFrames + totalFramesProcessed;
    } else {
        /* The group hasn't started yet. Just keep advancing time forward, but leave the data source alone. */
        pGroup->runningTimeInEngineFrames += frameCount;
    }

    /* If we're stopping after a delay we need to check if the delay has expired and if so, stop for real. */
    if (pGroup->stopDelayInEngineFramesRemaining > 0) {
        if (pGroup->stopDelayInEngineFramesRemaining >= frameCount) {
            pGroup->stopDelayInEngineFramesRemaining -= frameCount;
        } else {
            pGroup->stopDelayInEngineFramesRemaining = 0;
        }

        /* Stop the sound if the delay has been reached. */
        if (pGroup->stopDelayInEngineFramesRemaining == 0) {
            ma_sound_group_stop_internal(pGroup);
        }
    }
}

static void ma_engine_listener__data_callback_fixed(ma_engine* pEngine, void* pFramesOut, ma_uint32 frameCount)
{
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pEngine->periodSizeInFrames == frameCount);   /* This must always be true. */

    /* Recursively mix the sound groups. */
    ma_engine_mix_sound_group(pEngine, &pEngine->masterSoundGroup, pFramesOut, frameCount);
}

static void ma_engine_data_callback_internal(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    ma_engine_data_callback((ma_engine*)pDevice->pUserData, pFramesOut, pFramesIn, frameCount);
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
    pEngine->pDevice                  = engineConfig.pDevice;
    pEngine->format                   = engineConfig.format;
    pEngine->channels                 = engineConfig.channels;
    pEngine->sampleRate               = engineConfig.sampleRate;
    pEngine->periodSizeInFrames       = engineConfig.periodSizeInFrames;
    pEngine->periodSizeInMilliseconds = engineConfig.periodSizeInMilliseconds;
    ma_allocation_callbacks_init_copy(&pEngine->allocationCallbacks, &engineConfig.allocationCallbacks);


    /* We need a context before we'll be able to create the default listener. */
    contextConfig = ma_context_config_init();
    contextConfig.allocationCallbacks = pEngine->allocationCallbacks;

    /* If we don't have a device, we need one. */
    if (pEngine->pDevice == NULL) {
        ma_device_config deviceConfig;

        pEngine->pDevice = (ma_device*)ma__malloc_from_callbacks(sizeof(*pEngine->pDevice), &pEngine->allocationCallbacks/*, MA_ALLOCATION_TYPE_CONTEXT*/);
        if (pEngine->pDevice == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.pDeviceID       = engineConfig.pPlaybackDeviceID;
        deviceConfig.playback.format          = pEngine->format;
        deviceConfig.playback.channels        = pEngine->channels;
        deviceConfig.sampleRate               = pEngine->sampleRate;
        deviceConfig.dataCallback             = ma_engine_data_callback_internal;
        deviceConfig.pUserData                = pEngine;
        deviceConfig.periodSizeInFrames       = pEngine->periodSizeInFrames;
        deviceConfig.periodSizeInMilliseconds = pEngine->periodSizeInMilliseconds;
        deviceConfig.noPreZeroedOutputBuffer  = MA_TRUE;    /* We'll always be outputting to every frame in the callback so there's no need for a pre-silenced buffer. */
        deviceConfig.noClip                   = MA_TRUE;    /* The mixing engine will do clipping itself. */

        if (engineConfig.pContext == NULL) {
            result = ma_device_init_ex(NULL, 0, &contextConfig, &deviceConfig, pEngine->pDevice);
        } else {
            result = ma_device_init(engineConfig.pContext, &deviceConfig, pEngine->pDevice);
        }

        if (result != MA_SUCCESS) {
            ma__free_from_callbacks(pEngine->pDevice, &pEngine->allocationCallbacks/*, MA_ALLOCATION_TYPE_CONTEXT*/);
            pEngine->pDevice = NULL;
            return result;
        }

        pEngine->ownsDevice = MA_TRUE;
    }

    /* With the device initialized we need an intermediary buffer for handling fixed sized updates. Currently using a ring buffer for this, but can probably use something a bit more optimal. */
    result = ma_pcm_rb_init(pEngine->pDevice->playback.format, pEngine->pDevice->playback.channels, pEngine->pDevice->playback.internalPeriodSizeInFrames, NULL, &pEngine->allocationCallbacks, &pEngine->fixedRB);
    if (result != MA_SUCCESS) {
        goto on_error_1;
    }

    /* Now that have the default listener we can ensure we have the format, channels and sample rate set to proper values to ensure future listeners are configured consistently. */
    pEngine->format                   = pEngine->pDevice->playback.format;
    pEngine->channels                 = pEngine->pDevice->playback.channels;
    pEngine->sampleRate               = pEngine->pDevice->sampleRate;
    pEngine->periodSizeInFrames       = pEngine->pDevice->playback.internalPeriodSizeInFrames;
    pEngine->periodSizeInMilliseconds = (pEngine->periodSizeInFrames * 1000) / pEngine->sampleRate;


    /* We need a default sound group. This must be done after setting the format, channels and sample rate to their proper values. */
    result = ma_sound_group_init(pEngine, NULL, &pEngine->masterSoundGroup);
    if (result != MA_SUCCESS) {
        goto on_error_2;  /* Failed to initialize master sound group. */
    }


    /* We need a resource manager. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pEngine->pResourceManager == NULL) {
        ma_resource_manager_config resourceManagerConfig;

        pEngine->pResourceManager = (ma_resource_manager*)ma__malloc_from_callbacks(sizeof(*pEngine->pResourceManager), &pEngine->allocationCallbacks);
        if (pEngine->pResourceManager == NULL) {
            result = MA_OUT_OF_MEMORY;
            goto on_error_3;
        }

        resourceManagerConfig = ma_resource_manager_config_init();
        resourceManagerConfig.decodedFormat     = pEngine->format;
        resourceManagerConfig.decodedChannels   = 0;  /* Leave the decoded channel count as 0 so we can get good spatialization. */
        resourceManagerConfig.decodedSampleRate = pEngine->sampleRate;
        ma_allocation_callbacks_init_copy(&resourceManagerConfig.allocationCallbacks, &pEngine->allocationCallbacks);
        resourceManagerConfig.pVFS              = engineConfig.pResourceManagerVFS;

        result = ma_resource_manager_init(&resourceManagerConfig, pEngine->pResourceManager);
        if (result != MA_SUCCESS) {
            goto on_error_4;
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

#ifndef MA_NO_RESOURCE_MANAGER
on_error_4:
    if (pEngine->ownsResourceManager) {
        ma__free_from_callbacks(pEngine->pResourceManager, &pEngine->allocationCallbacks);
    }
on_error_3:
    ma_sound_group_uninit(&pEngine->masterSoundGroup);
#endif  /* MA_NO_RESOURCE_MANAGER */
on_error_2:
    ma_pcm_rb_uninit(&pEngine->fixedRB);
on_error_1:
    if (pEngine->ownsDevice) {
        ma_device_uninit(pEngine->pDevice);
        ma__free_from_callbacks(pEngine->pDevice, &pEngine->allocationCallbacks/*, MA_ALLOCATION_TYPE_CONTEXT*/);
    }

    return result;
}

MA_API void ma_engine_uninit(ma_engine* pEngine)
{
    if (pEngine == NULL) {
        return;
    }

    ma_sound_group_uninit(&pEngine->masterSoundGroup);
    
    if (pEngine->ownsDevice) {
        ma_device_uninit(pEngine->pDevice);
        ma__free_from_callbacks(pEngine->pDevice, &pEngine->allocationCallbacks/*, MA_ALLOCATION_TYPE_CONTEXT*/);
    }

    /* Uninitialize the resource manager last to ensure we don't have a thread still trying to access it. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pEngine->ownsResourceManager) {
        ma_resource_manager_uninit(pEngine->pResourceManager);
        ma__free_from_callbacks(pEngine->pResourceManager, &pEngine->allocationCallbacks);
    }
#endif
}

MA_API void ma_engine_data_callback(ma_engine* pEngine, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    ma_uint32 pcmFramesAvailableInRB;
    ma_uint32 pcmFramesProcessed = 0;
    ma_uint8* pRunningOutput = (ma_uint8*)pFramesOut;

    if (pEngine == NULL) {
        return;
    }

    /* We need to do updates in fixed sizes based on the engine's period size in frames. */

    /*
    The first thing to do is check if there's enough data available in the ring buffer. If so we can read from it. Otherwise we need to keep filling
    the ring buffer until there's enough, making sure we only fill the ring buffer in chunks of pEngine->periodSizeInFrames.
    */
    while (pcmFramesProcessed < frameCount) {    /* Keep going until we've filled the output buffer. */
        ma_uint32 framesRemaining = frameCount - pcmFramesProcessed;

        pcmFramesAvailableInRB = ma_pcm_rb_available_read(&pEngine->fixedRB);
        if (pcmFramesAvailableInRB > 0) {
            ma_uint32 framesToRead = (framesRemaining < pcmFramesAvailableInRB) ? framesRemaining : pcmFramesAvailableInRB;
            void* pReadBuffer;

            ma_pcm_rb_acquire_read(&pEngine->fixedRB, &framesToRead, &pReadBuffer);
            {
                memcpy(pRunningOutput, pReadBuffer, framesToRead * ma_get_bytes_per_frame(pEngine->pDevice->playback.format, pEngine->pDevice->playback.channels));
            }
            ma_pcm_rb_commit_read(&pEngine->fixedRB, framesToRead, pReadBuffer);

            pRunningOutput += framesToRead * ma_get_bytes_per_frame(pEngine->pDevice->playback.format, pEngine->pDevice->playback.channels);
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

            ma_pcm_rb_reset(&pEngine->fixedRB);
            ma_pcm_rb_acquire_write(&pEngine->fixedRB, &framesToWrite, &pWriteBuffer);
            {
                MA_ASSERT(framesToWrite == pEngine->periodSizeInFrames);   /* <-- This should always work in this example because we just reset the ring buffer. */
                ma_engine_listener__data_callback_fixed(pEngine, pWriteBuffer, framesToWrite);
            }
            ma_pcm_rb_commit_write(&pEngine->fixedRB, framesToWrite, pWriteBuffer);
        }
    }

    (void)pFramesIn;   /* Not doing anything with input right now. */
}


MA_API ma_result ma_engine_start(ma_engine* pEngine)
{
    ma_result result;

    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_device_start(pEngine->pDevice);
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

    result = ma_device_stop(pEngine->pDevice);
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

    return ma_device_set_master_volume(pEngine->pDevice, volume);
}

MA_API ma_result ma_engine_set_gain_db(ma_engine* pEngine, float gainDB)
{
    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_device_set_master_gain_db(pEngine->pDevice, gainDB);
}


MA_API ma_sound_group* ma_engine_get_master_sound_group(ma_engine* pEngine)
{
    if (pEngine == NULL) {
        return NULL;
    }

    return &pEngine->masterSoundGroup;
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
    for (pNextSound = ma_sound_group_first_sound(pGroup); pNextSound != NULL; pNextSound = ma_sound_next_sound_in_group(pNextSound)) {
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
        MA_ASSERT(ma_sound_at_end(pSound) == MA_FALSE);

        /* We're just going to reuse the same data source as before so we need to make sure we uninitialize the old one first. */
        if (pSound->pDataSource != NULL) {  /* <-- Safety. Should never happen. */
            MA_ASSERT(pSound->ownsDataSource == MA_TRUE);
            ma_resource_manager_data_source_uninit(&pSound->resourceManagerDataSource);
        }

        /* The old data source has been uninitialized so now we need to initialize the new one. */
        result = ma_resource_manager_data_source_init(pEngine->pResourceManager, pFilePath, dataSourceFlags, NULL, &pSound->resourceManagerDataSource);
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
            ma_sound_uninit(pSound);
            return result;
        }
    } else {
        /* There's no available sounds for recycling. We need to allocate a sound. This can be done using a stack allocator. */
        pSound = (ma_sound*)ma__malloc_from_callbacks(sizeof(*pSound), &pEngine->allocationCallbacks/*, MA_ALLOCATION_TYPE_SOUND*/); /* TODO: This can certainly be optimized. */
        if (pSound == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        result = ma_sound_init_from_file(pEngine, pFilePath, dataSourceFlags, NULL, pGroup, pSound);
        if (result != MA_SUCCESS) {
            ma__free_from_callbacks(pEngine, &pEngine->allocationCallbacks);
            return result;
        }

        /* The sound needs to be marked as internal for our own internal memory management reasons. This is how we know whether or not the sound is available for recycling. */
        pSound->_isInternal = MA_TRUE;  /* This is the only place _isInternal will be modified. We therefore don't need to worry about synchronizing access to this variable. */
    }

    /* Finally we can start playing the sound. */
    result = ma_sound_start(pSound);
    if (result != MA_SUCCESS) {
        /* Failed to start the sound. We need to uninitialize it and return an error. */
        ma_sound_uninit(pSound);
        return result;
    }

    return MA_SUCCESS;
}


static ma_result ma_sound_detach(ma_sound* pSound)
{
    ma_sound_group* pGroup;

    MA_ASSERT(pSound != NULL);

    pGroup = pSound->pGroup;
    MA_ASSERT(pGroup != NULL);

    /*
    The sound should never be in a playing state when this is called. It *can*, however, but in the middle of mixing in the mixing thread. It needs to finish
    mixing before being uninitialized completely, but that is done at a higher level to this function.
    */
    MA_ASSERT(ma_sound_is_playing(pSound) == MA_FALSE);

    /*
    We want the creation and deletion of sounds to be supported across multiple threads. An application may have any thread that want's to call
    ma_engine_play_sound(), for example. The application would expect this to just work. The problem, however, is that the mixing thread will be iterating over
    the list at the same time. We need to be careful with how we remove a sound from the list because we'll essentially be taking the sound out from under the
    mixing thread and the mixing thread must continue to work. Normally you would wrap the iteration in a lock as well, however an added complication is that
    the mixing thread cannot be locked as it's running on the audio thread, and locking in the audio thread is a no-no).

    To start with, ma_sound_detach() (this function) and ma_sound_attach() need to be wrapped in a lock. This lock will *not* be used by the
    mixing thread. We therefore need to craft this in a very particular way so as to ensure the mixing thread does not lose track of it's iteration state. What
    we don't want to do is clear the pNextSoundInGroup variable to NULL. This need to be maintained to ensure the mixing thread can continue iteration even
    after the sound has been removed from the group. This is acceptable because sounds are fixed to their group for the entire life, and this function will
    only ever be called when the sound is being uninitialized which therefore means it'll never be iterated again.
    */
    ma_mutex_lock(&pGroup->lock);
    {
        ma_sound* pNextSoundInGroup = ma_sound_next_sound_in_group(pSound);
        ma_sound* pPrevSoundInGroup = ma_sound_prev_sound_in_group(pSound);

        if (pPrevSoundInGroup == NULL) {
            /* The sound is the head of the list. All we need to do is change the pPrevSoundInGroup member of the next sound to NULL and make it the new head. */

            /* Make a new head. */
            c89atomic_exchange_ptr(&pGroup->pFirstSoundInGroup, pNextSoundInGroup);
        } else {
            /*
            The sound is not the head. We need to remove the sound from the group by simply changing the pNextSoundInGroup member of the previous sound. This is
            the important part. This is the part that allows the mixing thread to continue iteration without locking.
            */
            c89atomic_exchange_ptr(&pPrevSoundInGroup->pNextSoundInGroup, pNextSoundInGroup);
        }

        /* This doesn't really need to be done atomically because we've wrapped this in a lock and it's not used by the mixing thread. */
        if (pNextSoundInGroup != NULL) {
            c89atomic_exchange_ptr(&pNextSoundInGroup->pPrevSoundInGroup, pPrevSoundInGroup);
        }
    }
    ma_mutex_unlock(&pGroup->lock);

    return MA_SUCCESS;
}

static ma_result ma_sound_attach(ma_sound* pSound, ma_sound_group* pGroup)
{
    MA_ASSERT(pSound != NULL);
    MA_ASSERT(pGroup != NULL);
    MA_ASSERT(pSound->pGroup == NULL);

    /* This should only ever be called when the sound is first initialized which means we should never be in a playing state. */
    MA_ASSERT(ma_sound_is_playing(pSound) == MA_FALSE);

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
        ma_sound* pOldFirstSoundInGroup = ma_sound_group_first_sound(pGroup);

        pNewFirstSoundInGroup->pNextSoundInGroup = pOldFirstSoundInGroup;
        if (pOldFirstSoundInGroup != NULL) {
            pOldFirstSoundInGroup->pPrevSoundInGroup = pNewFirstSoundInGroup;
        }

        c89atomic_exchange_ptr(&pGroup->pFirstSoundInGroup, pNewFirstSoundInGroup);
    }
    ma_mutex_unlock(&pGroup->lock);

    return MA_SUCCESS;
}



static ma_result ma_sound_preinit(ma_engine* pEngine, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound)
{
    ma_result result;

    (void)flags;

    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pSound);

    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->pEngine = pEngine;

    /* An effect is used for handling panning, pitching, etc. */
    result = ma_engine_effect_init(pEngine, &pSound->effect);
    if (result != MA_SUCCESS) {
        return result;
    }

    pSound->pDataSource = NULL; /* This will be set a higher level outside of this function. */
    pSound->volume      = 1;
    pSound->seekTarget  = MA_SEEK_TARGET_NONE;

    if (pGroup == NULL) {
        pGroup = &pEngine->masterSoundGroup;
    }

    /*
    By default the sound needs to be added to the master group. It's safe to add to the master group before the sound has been fully initialized because
    the playing flag is set to false which means the group won't be attempting to do anything with it. Also, the sound won't prematurely be recycled
    because the atEnd flag is also set to false which is the indicator that the sound object is not available for recycling.
    */
    result = ma_sound_attach(pSound, pGroup);
    if (result != MA_SUCCESS) {
        ma_engine_effect_uninit(pEngine, &pSound->effect);
        return result;  /* Should never happen. Failed to attach the sound to the group. */
    }

    return MA_SUCCESS;
}

#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_sound_init_from_file(ma_engine* pEngine, const char* pFilePath, ma_uint32 flags, ma_async_notification* pNotification, ma_sound_group* pGroup, ma_sound* pSound)
{
    ma_result result;

    result = ma_sound_preinit(pEngine, flags, pGroup, pSound);
    if (result != MA_SUCCESS) {
        return result;
    }

    /*
    The preinitialization process has succeeded so now we need to load the data source from the resource manager. This needs to be the very last part of the
    process because we want to ensure the notification is only fired when the sound is fully initialized and usable. This is important because the caller may
    want to do things like query the length of the sound, set fade points, etc.
    */
    pSound->pDataSource    = &pSound->resourceManagerDataSource;   /* <-- Make sure the pointer to our data source is set before calling into the resource manager. */
    pSound->ownsDataSource = MA_TRUE;

    result = ma_resource_manager_data_source_init(pEngine->pResourceManager, pFilePath, flags, pNotification, &pSound->resourceManagerDataSource);
    if (result != MA_SUCCESS) {
        pSound->pDataSource = NULL;
        pSound->ownsDataSource = MA_FALSE;
        ma_sound_uninit(pSound);
        MA_ZERO_OBJECT(pSound);
        return result;
    }

    return MA_SUCCESS;
}
#endif

MA_API ma_result ma_sound_init_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound)
{
    ma_result result;

    result = ma_sound_preinit(pEngine, flags, pGroup, pSound);
    if (result != MA_SUCCESS) {
        return result;
    }

    pSound->pDataSource = pDataSource;
    pSound->ownsDataSource = MA_FALSE;

    return MA_SUCCESS;
}

MA_API void ma_sound_uninit(ma_sound* pSound)
{
    ma_result result;

    if (pSound == NULL) {
        return;
    }

    /* Make sure the sound is stopped as soon as possible to reduce the chance that it gets locked by the mixer. We also need to stop it before detaching from the group. */
    ma_sound_set_stop_delay(pSound, 0);   /* <-- Ensures the sound stops immediately. */
    result = ma_sound_stop(pSound);
    if (result != MA_SUCCESS) {
        return;
    }

    /* The sound needs to removed from the group to ensure it doesn't get iterated again and cause things to break again. This is thread-safe. */
    result = ma_sound_detach(pSound);
    if (result != MA_SUCCESS) {
        return;
    }

    /*
    The sound is detached from the group, but it may still be in the middle of mixing which means our data source is locked. We need to wait for
    this to finish before deleting from the resource manager.

    We could define this so that we don't wait if the sound does not own the underlying data source, but this might end up being dangerous because
    the application may think it's safe to destroy the data source when it actually isn't. It just feels untidy doing it like that.
    */
    ma_sound_mix_wait(pSound);


    /* Once the sound is detached from the group we can guarantee that it won't be referenced by the mixer thread which means it's safe for us to destroy the data source. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pSound->ownsDataSource) {
        ma_resource_manager_data_source_uninit(&pSound->resourceManagerDataSource);
        pSound->pDataSource = NULL;
    }
#else
    MA_ASSERT(pSound->ownsDataSource == MA_FALSE);
#endif
}

MA_API ma_result ma_sound_start(ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If the sound is already playing, do nothing. */
    if (ma_sound_is_playing(pSound)) {
        return MA_SUCCESS;
    }

    /* If the sound is at the end it means we want to start from the start again. */
    if (ma_sound_at_end(pSound)) {
        ma_result result = ma_data_source_seek_to_pcm_frame(pSound->pDataSource, 0);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to seek back to the start. */
        }

        /* Make sure we clear the end indicator. */
        c89atomic_exchange_32(&pSound->atEnd, MA_FALSE);
    }

    /* Once everything is set up we can tell the mixer thread about it. */
    c89atomic_exchange_32(&pSound->isPlaying, MA_TRUE);

    return MA_SUCCESS;
}

static MA_INLINE ma_result ma_sound_stop_internal(ma_sound* pSound)
{
    MA_ASSERT(pSound != NULL);

    c89atomic_exchange_32(&pSound->isPlaying, MA_FALSE);

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_stop(ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->stopDelayInEngineFramesRemaining = pSound->stopDelayInEngineFrames;

    /* Stop immediately if we don't have a delay. */
    if (pSound->stopDelayInEngineFrames == 0) {
        ma_sound_stop_internal(pSound);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_set_volume(ma_sound* pSound, float volume)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->volume = volume;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_set_gain_db(ma_sound* pSound, float gainDB)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_sound_set_volume(pSound, ma_gain_db_to_factor(gainDB));
}

MA_API ma_result ma_sound_set_effect(ma_sound* pSound, ma_effect* pEffect)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->effect.pPreEffect = pEffect;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_set_pitch(ma_sound* pSound, float pitch)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->effect.pitch = pitch;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_set_pan(ma_sound* pSound, float pan)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_panner_set_pan(&pSound->effect.panner, pan);
}

MA_API ma_result ma_sound_set_pan_mode(ma_sound* pSound, ma_pan_mode pan_mode)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_panner_set_mode(&pSound->effect.panner, pan_mode);
}

MA_API ma_result ma_sound_set_position(ma_sound* pSound, ma_vec3 position)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_spatializer_set_position(&pSound->effect.spatializer, position);
}

MA_API ma_result ma_sound_set_rotation(ma_sound* pSound, ma_quat rotation)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_spatializer_set_rotation(&pSound->effect.spatializer, rotation);
}

MA_API ma_result ma_sound_set_looping(ma_sound* pSound, ma_bool32 isLooping)
{
    if (pSound == NULL) {
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
        ma_resource_manager_data_source_set_looping(&pSound->resourceManagerDataSource, isLooping);
    }
#endif

    return MA_SUCCESS;
}

MA_API ma_bool32 ma_sound_is_looping(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_FALSE;
    }

    return c89atomic_load_32((ma_bool32*)&pSound->isLooping);
}


MA_API ma_result ma_sound_set_fade_in_frames(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_fader_set_fade(&pSound->effect.fader, volumeBeg, volumeEnd, fadeLengthInFrames);
}

MA_API ma_result ma_sound_set_fade_in_milliseconds(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_sound_set_fade_in_frames(pSound, volumeBeg, volumeEnd, (fadeLengthInMilliseconds * pSound->effect.fader.config.sampleRate) / 1000);
}

MA_API ma_result ma_sound_get_current_fade_volume(ma_sound* pSound, float* pVolume)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_fader_get_current_volume(&pSound->effect.fader, pVolume);
}

MA_API ma_result ma_sound_set_start_delay(ma_sound* pSound, ma_uint64 delayInMilliseconds)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pSound->pEngine != NULL);

    /*
    It's important that the delay be timed based on the engine's sample rate and not the rate of the sound. The reason is that no processing will be happening
    by the sound before playback has actually begun and we won't have accurate frame counters due to resampling.
    */
    pSound->startDelayInEngineFrames = (pSound->pEngine->sampleRate * delayInMilliseconds) / 1000;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_set_stop_delay(ma_sound* pSound, ma_uint64 delayInMilliseconds)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pSound->pEngine != NULL);

    pSound->stopDelayInEngineFrames = (pSound->pEngine->sampleRate * delayInMilliseconds) / 1000;

    return MA_SUCCESS;
}

MA_API ma_bool32 ma_sound_is_playing(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_FALSE;
    }

    return c89atomic_load_32((ma_bool32*)&pSound->isPlaying);
}

MA_API ma_bool32 ma_sound_at_end(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_FALSE;
    }

    return c89atomic_load_32((ma_bool32*)&pSound->atEnd);
}

MA_API ma_result ma_sound_get_time_in_frames(const ma_sound* pSound, ma_uint64* pTimeInFrames)
{
    if (pTimeInFrames == NULL) {
        return MA_INVALID_ARGS;
    }

    *pTimeInFrames = 0;

    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    *pTimeInFrames = pSound->effect.timeInFrames;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_seek_to_pcm_frame(ma_sound* pSound, ma_uint64 frameIndex)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /*
    Resource manager data sources are thread safe which means we can just seek immediately. However, we cannot guarantee that other data sources are
    thread safe as well so in that case we'll need to get the mixing thread to seek for us to ensure we don't try seeking at the same time as reading.
    */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pSound->pDataSource == &pSound->resourceManagerDataSource) {
        ma_result result = ma_resource_manager_data_source_seek_to_pcm_frame(&pSound->resourceManagerDataSource, frameIndex);
        if (result != MA_SUCCESS) {
            return result;
        }

        /* Time dependant effects need to have their timers updated. */
        return ma_engine_effect_set_time(&pSound->effect, frameIndex);
    }
#endif

    /* Getting here means the data source is not a resource manager data source so we'll need to get the mixing thread to do the seeking for us. */
    pSound->seekTarget = frameIndex;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_get_data_format(ma_sound* pSound, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_data_source_get_data_format(pSound->pDataSource, pFormat, pChannels, pSampleRate);
}

MA_API ma_result ma_sound_get_cursor_in_pcm_frames(ma_sound* pSound, ma_uint64* pCursor)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_data_source_get_cursor_in_pcm_frames(pSound->pDataSource, pCursor);
}

MA_API ma_result ma_sound_get_length_in_pcm_frames(ma_sound* pSound, ma_uint64* pLength)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_data_source_get_length_in_pcm_frames(pSound->pDataSource, pLength);
}



static ma_result ma_sound_group_attach(ma_sound_group* pGroup, ma_sound_group* pParentGroup)
{
    ma_sound_group* pNewFirstChild;
    ma_sound_group* pOldFirstChild;

    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pGroup->pEngine != NULL);

    /* Don't do anything for the master sound group. This should never be attached to anything. */
    if (pGroup == &pGroup->pEngine->masterSoundGroup) {
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

    pParentGroup->pFirstChild = pNewFirstChild;

    return MA_SUCCESS;
}

static ma_result ma_sound_group_detach(ma_sound_group* pGroup)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pGroup->pEngine != NULL);

    /* Don't do anything for the master sound group. This should never be detached from anything. */
    if (pGroup == &pGroup->pEngine->masterSoundGroup) {
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

MA_API ma_result ma_sound_group_init(ma_engine* pEngine, ma_sound_group* pParentGroup, ma_sound_group* pGroup)
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

    pGroup->pEngine = pEngine;

    /* Use the master group if the parent group is NULL, so long as it's not the master group itself. */
    if (pParentGroup == NULL && pGroup != &pEngine->masterSoundGroup) {
        pParentGroup = &pEngine->masterSoundGroup;
    }


    /* TODO: Look at the possibility of allowing groups to use a different format to the primary data format. Makes mixing and group management much more complicated. */

    /* For handling panning, etc. we'll need an engine effect. */
    result = ma_engine_effect_init(pEngine, &pGroup->effect);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the engine effect. */
    }

    /* The sound group needs a mixer. This is what's used to mix each of the sounds contained within the group, and sub-groups. */
    mixerConfig = ma_mixer_config_init(pEngine->format, pEngine->channels, pEngine->periodSizeInFrames, NULL, &pEngine->allocationCallbacks);
    result = ma_mixer_init(&mixerConfig, &pGroup->mixer);
    if (result != MA_SUCCESS) {
        ma_engine_effect_uninit(pEngine, &pGroup->effect);
        return result;
    }

    /* The mixer's effect is always set to the main engine effect. */
    ma_mixer_set_effect(&pGroup->mixer, &pGroup->effect);


    /* Attach the sound group to it's parent if it has one (this will only happen if it's the master group). */
    if (pParentGroup != NULL) {
        result = ma_sound_group_attach(pGroup, pParentGroup);
        if (result != MA_SUCCESS) {
            ma_mixer_uninit(&pGroup->mixer);
            ma_engine_effect_uninit(pEngine, &pGroup->effect);
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
        ma_sound_group_detach(pGroup);
        ma_mixer_uninit(&pGroup->mixer);
        ma_engine_effect_uninit(pEngine, &pGroup->effect);
        return result;
    }

    /* The group needs to be started by default, but needs to be done after attaching to the internal list. */
    c89atomic_exchange_32(&pGroup->isPlaying, MA_TRUE);

    return MA_SUCCESS;
}

static void ma_sound_group_uninit_all_internal_sounds(ma_sound_group* pGroup)
{
    ma_sound* pCurrentSound;

    /* We need to be careful here that we keep our iteration valid. */
    pCurrentSound = ma_sound_group_first_sound(pGroup);
    while (pCurrentSound != NULL) {
        ma_sound* pSoundToDelete = pCurrentSound;
        pCurrentSound = ma_sound_next_sound_in_group(pCurrentSound);

        if (pSoundToDelete->_isInternal) {
            ma_sound_uninit(pSoundToDelete);
        }
    }
}

MA_API void ma_sound_group_uninit(ma_sound_group* pGroup)
{
    ma_result result;

    ma_sound_group_set_stop_delay(pGroup, 0); /* <-- Make sure we disable fading out so the sound group is stopped immediately. */
    result = ma_sound_group_stop(pGroup);
    if (result != MA_SUCCESS) {
        MA_ASSERT(MA_FALSE);    /* Should never happen. Trigger an assert for debugging, but don't stop uninitializing in production to ensure we free memory down below. */
    }

    /* Any in-place sounds need to be uninitialized. */
    ma_sound_group_uninit_all_internal_sounds(pGroup);

    result = ma_sound_group_detach(pGroup);
    if (result != MA_SUCCESS) {
        MA_ASSERT(MA_FALSE);    /* As above, should never happen, but just in case trigger an assert in debug mode, but continue processing. */
    }

    ma_mixer_uninit(&pGroup->mixer);
    ma_mutex_uninit(&pGroup->lock);

    ma_engine_effect_uninit(pGroup->pEngine, &pGroup->effect);
}

MA_API ma_result ma_sound_group_start(ma_sound_group* pGroup)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_32(&pGroup->isPlaying, MA_TRUE);

    return MA_SUCCESS;
}

static MA_INLINE ma_result ma_sound_group_stop_internal(ma_sound_group* pGroup)
{
    MA_ASSERT(pGroup != NULL);

    c89atomic_exchange_32(&pGroup->isPlaying, MA_FALSE);

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_group_stop(ma_sound_group* pGroup)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    pGroup->stopDelayInEngineFramesRemaining = pGroup->stopDelayInEngineFrames;

    /* Stop immediately if we're not delaying. */
    if (pGroup->stopDelayInEngineFrames == 0) {
        ma_sound_group_stop_internal(pGroup);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_group_set_volume(ma_sound_group* pGroup, float volume)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The volume is set via the mixer. */
    ma_mixer_set_volume(&pGroup->mixer, volume);

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_group_set_gain_db(ma_sound_group* pGroup, float gainDB)
{
    return ma_sound_group_set_volume(pGroup, ma_gain_db_to_factor(gainDB));
}

MA_API ma_result ma_sound_group_set_effect(ma_sound_group* pGroup, ma_effect* pEffect)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    pGroup->effect.pPreEffect = pEffect;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_group_set_pan(ma_sound_group* pGroup, float pan)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_panner_set_pan(&pGroup->effect.panner, pan);
}

MA_API ma_result ma_sound_group_set_pitch(ma_sound_group* pGroup, float pitch)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    pGroup->effect.pitch = pitch;

    return MA_SUCCESS;
}


MA_API ma_result ma_sound_group_set_fade_in_frames(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_fader_set_fade(&pGroup->effect.fader, volumeBeg, volumeEnd, fadeLengthInFrames);
}

MA_API ma_result ma_sound_group_set_fade_in_milliseconds(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_sound_group_set_fade_in_frames(pGroup, volumeBeg, volumeEnd, (fadeLengthInMilliseconds * pGroup->effect.fader.config.sampleRate) / 1000);
}

MA_API ma_result ma_sound_group_get_current_fade_volume(ma_sound_group* pGroup, float* pVolume)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_fader_get_current_volume(&pGroup->effect.fader, pVolume);
}

MA_API ma_result ma_sound_group_set_start_delay(ma_sound_group* pGroup, ma_uint64 delayInMilliseconds)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pGroup->pEngine != NULL);

    pGroup->startDelayInEngineFrames = (pGroup->pEngine->sampleRate * delayInMilliseconds) / 1000;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_group_set_stop_delay(ma_sound_group* pGroup, ma_uint64 delayInMilliseconds)
{
    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pGroup->pEngine != NULL);

    pGroup->stopDelayInEngineFrames = (pGroup->pEngine->sampleRate * delayInMilliseconds) / 1000;

    return MA_SUCCESS;
}

MA_API ma_bool32 ma_sound_group_is_playing(const ma_sound_group* pGroup)
{
    if (pGroup == NULL) {
        return MA_FALSE;
    }

    return c89atomic_load_32((ma_bool32*)&pGroup->isPlaying);
}

MA_API ma_result ma_sound_group_get_time_in_frames(const ma_sound_group* pGroup, ma_uint64* pTimeInFrames)
{
    if (pTimeInFrames == NULL) {
        return MA_INVALID_ARGS;
    }

    *pTimeInFrames = 0;

    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    *pTimeInFrames = pGroup->effect.timeInFrames;

    return MA_SUCCESS;
}


#endif
