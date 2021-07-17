/* !!! THIS FILE WILL BE MERGED INTO miniaudio.h WHEN COMPLETE !!! */

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
start by default. You can use `ma_engine_play_sound()` to "fire and forget" sounds.

Sounds can be allocated to groups called `ma_sound_group`. This is how you can support submixing and is one way you could achieve the kinds of groupings you see
in games for things like SFX, Music and Voices. Unlike sounds, groups are started by default. When you stop a group, all sounds within that group will be
stopped atomically. When the group is started again, all sounds attached to the group will also be started, so long as the sound is also marked as started.

The creation and deletion of sounds and groups should be thread safe.

The engine runs on top of a node graph, and sounds and groups are just nodes within that graph. The output of a sound can be attached to the input of any node
on the graph. To apply an effect to a sound or group, attach it's output to the input of an effect node. See the Routing Infrastructure section below for
details on this.

The best resource to use when understanding the API is the function declarations for `ma_engine`. I expect you should be able to figure it out! :)
*/
#ifndef miniaudio_engine_h
#define miniaudio_engine_h

#ifdef __cplusplus
extern "C" {
#endif

/*
Resource Management
===================
Many programs will want to manage sound resources for things such as reference counting and
streaming. This is supported by miniaudio via the `ma_resource_manager` API.

The resource manager is mainly responsible for the following:

    1) Loading of sound files into memory with reference counting.
    2) Streaming of sound data

When loading a sound file, the resource manager will give you back a `ma_data_source` compatible
object called `ma_resource_manager_data_source`. This object can be passed into any
`ma_data_source` API which is how you can read and seek audio data. When loading a sound file, you
specify whether or not you want the sound to be fully loaded into memory (and optionally
pre-decoded) or streamed. When loading into memory, you can also specify whether or not you want
the data to be loaded asynchronously.

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

You can configure the format, channels and sample rate of the decoded audio data. By default it
will use the file's native data format, but you can configure it to use a consistent format. This
is useful for offloading the cost of data conversion to load time rather than dynamically
converting a mixing time. To do this, you configure the decoded format, channels and sample rate
like the code below:

    ```c
    config = ma_resource_manager_config_init();
    config.decodedFormat     = device.playback.format;
    config.decodedChannels   = device.playback.channels;
    config.decodedSampleRate = device.sampleRate;
    ```

In the code above, the resource manager will be configured so that any decoded audio data will be
pre-converted at load time to the device's native data format. If instead you used defaults and
the data format of the file did not match the device's data format, you would need to convert the
data at mixing time which may be prohibitive in high-performance and large scale scenarios like
games.

Asynchronicity is achieved via a job system. When an operation needs to be performed, such as the
decoding of a page, a job will be posted to a queue which will then be processed by a job thread.
By default there will be only one job thread running, but this can be configured, like so:

    ```c
    config = ma_resource_manager_config_init();
    config.jobThreadCount = MY_JOB_THREAD_COUNT;
    ```

By default job threads are managed internally by the resource manager, however you can also self
manage your job threads if, for example, you want to integrate the job processing into your
existing job infrastructure, or if you simply don't like the way the resource manager does it. To
do this, just set the job thread count to 0 and process jobs manually. To process jobs, you first
need to retrieve a job using `ma_resource_manager_next_job()` and then process it using
`ma_resource_manager_process_job()`:

    ```c
    config = ma_resource_manager_config_init();
    config.jobThreadCount = 0;                            // Don't manage any job threads internally.
    config.flags = MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING; // Optional. Makes `ma_resource_manager_next_job()` non-blocking.

    // ... Initialize your custom job threads ...

    void my_custom_job_thread(...)
    {
        for (;;) {
            ma_resource_manager_job job;
            ma_result result = ma_resource_manager_next_job(pMyResourceManager, &job);
            if (result != MA_SUCCESS) {
                if (result == MA_NOT_DATA_AVAILABLE) {
                    // No jobs are available. Keep going. Will only get this if the resource manager was initialized
                    // with MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING.
                    continue;
                } else if (result == MA_CANCELLED) {
                    // MA_RESOURCE_MANAGER_JOB_QUIT was posted. Exit.
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

In the example above, the `MA_RESOURCE_MANAGER_JOB_QUIT` event is the used as the termination indicator, but you can
use whatever you would like to terminate the thread. The call to `ma_resource_manager_next_job()`
is blocking by default, by can be configured to be non-blocking by initializing the resource
manager with the `MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING` configuration flag.

When loading a file, it's sometimes convenient to be able to customize how files are opened and
read. This can be done by setting `pVFS` member of the resource manager's config:

    ```c
    // Initialize your custom VFS object. See documentation for VFS for information on how to do this.
    my_custom_vfs vfs = my_custom_vfs_init();

    config = ma_resource_manager_config_init();
    config.pVFS = &vfs;
    ```

If you do not specify a custom VFS, the resource manager will use the operating system's normal
file operations. This is default.

To load a sound file and create a data source, call `ma_resource_manager_data_source_init()`. When
loading a sound you need to specify the file path and options for how the sounds should be loaded.
By default a sound will be loaded synchronously. The returned data source is owned by the caller
which means the caller is responsible for the allocation and freeing of the data source. Below is
an example for initializing a data source:

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

The `flags` parameter specifies how you want to perform loading of the sound file. It can be a
combination of the following flags:

    ```
    MA_DATA_SOURCE_STREAM
    MA_DATA_SOURCE_DECODE
    MA_DATA_SOURCE_ASYNC
    ```

When no flags are specified (set to 0), the sound will be fully loaded into memory, but not
decoded, meaning the raw file data will be stored in memory, and then dynamically decoded when
`ma_data_source_read_pcm_frames()` is called. To instead decode the audio data before storing it in
memory, use the `MA_DATA_SOURCE_DECODE` flag. By default, the sound file will be loaded
synchronously, meaning `ma_resource_manager_data_source_init()` will only return after the entire
file has been loaded. This is good for simplicity, but can be prohibitively slow. You can instead
load the sound asynchronously using the `MA_DATA_SOURCE_ASYNC` flag. This will result in
`ma_resource_manager_data_source_init()` returning quickly, but no data will be returned by
`ma_data_source_read_pcm_frames()` until some data is available. When no data is available because
the asynchronous decoding hasn't caught up, `MA_BUSY` will be returned by
`ma_data_source_read_pcm_frames()`.

For large sounds, it's often prohibitive to store the entire file in memory. To mitigate this, you
can instead stream audio data which you can do by specifying the `MA_DATA_SOURCE_STREAM` flag. When
streaming, data will be decoded in 1 second pages. When a new page needs to be decoded, a job will
be posted to the job queue and then subsequently processed in a job thread.

When loading asynchronously, it can be useful to poll whether or not loading has finished. Use
`ma_resource_manager_data_source_result()` to determine this. For in-memory sounds, this will
return `MA_SUCCESS` when the file has been *entirely* decoded. If the sound is still being decoded,
`MA_BUSY` will be returned. Otherwise, some other error code will be returned if the sound failed
to load. For streaming data sources, `MA_SUCCESS` will be returned when the first page has been
decoded and the sound is ready to be played. If the first page is still being decoded, `MA_BUSY`
will be returned. Otherwise, some other error code will be returned if the sound failed to load.

For in-memory sounds, reference counting is used to ensure the data is loaded only once. This means
multiple calls to `ma_resource_manager_data_source_init()` with the same file path will result in
the file data only being loaded once. Each call to `ma_resource_manager_data_source_init()` must be
matched up with a call to `ma_resource_manager_data_source_uninit()`. Sometimes it can be useful
for a program to register self-managed raw audio data and associate it with a file path. Use the
`ma_resource_manager_register_*()` and `ma_resource_manager_unregister_*()` APIs to do this.
`ma_resource_manager_register_decoded_data()` is used to associate a pointer to raw, self-managed
decoded audio data in the specified data format with the specified name. Likewise,
`ma_resource_manager_register_encoded_data()` is used to associate a pointer to raw self-managed
encoded audio data (the raw file data) with the specified name. Note that these names need not be
actual file paths. When `ma_resource_manager_data_source_init()` is called (without the
`MA_DATA_SOURCE_STREAM` flag), the resource manager will look for these explicitly registered data
buffers and, if found, will use it as the backing data for the data source. Note that the resource
manager does *not* make a copy of this data so it is up to the caller to ensure the pointer stays
valid for it's lifetime. Use `ma_resource_manager_unregister_data()` to unregister the self-managed
data. It does not make sense to use the `MA_DATA_SOURCE_STREAM` flag with a self-managed data
pointer. When `MA_DATA_SOURCE_STREAM` is specified, it will try loading the file data through the
VFS.


Resource Manager Implementation Details
---------------------------------------
Resources are managed in two main ways:

    1) By storing the entire sound inside an in-memory buffer (referred to as a data buffer)
    2) By streaming audio data on the fly (referred to as a data stream)

A resource managed data source (`ma_resource_manager_data_source`) encapsulates a data buffer or
data stream, depending on whether or not the data source was initialized with the
`MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM` flag. If so, it will make use of a
`ma_resource_manager_data_stream` object. Otherwise it will use a `ma_resource_manager_data_buffer`
object. Both of these objects are data sources which means they can be used with any
`ma_data_source_*()` API.

Another major feature of the resource manager is the ability to asynchronously decode audio files.
This relieves the audio thread of time-consuming decoding which can negatively affect scalability
due to the audio thread needing to complete it's work extremely quickly to avoid glitching.
Asynchronous decoding is achieved through a job system. There is a central multi-producer,
multi-consumer, lock-free, fixed-capacity job queue. When some asynchronous work needs to be done,
a job is posted to the queue which is then read by a job thread. The number of job threads can be
configured for improved scalability, and job threads can all run in parallel without needing to
worry about the order of execution (how this is achieved is explained below).

When a sound is being loaded asynchronously, playback can begin before the sound has been fully
decoded. This enables the application to start playback of the sound quickly, while at the same
time allowing to resource manager to keep loading in the background. Since there may be less
threads than the number of sounds being loaded at a given time, a simple scheduling system is used
to keep decoding time balanced and fair. The resource manager solves this by splitting decoding
into chunks called pages. By default, each page is 1 second long. When a page has been decoded, a
new job will be posted to start decoding the next page. By dividing up decoding into pages, an
individual sound shouldn't ever delay every other sound from having their first page decoded. Of
course, when loading many sounds at the same time, there will always be an amount of time required
to process jobs in the queue so in heavy load situations there will still be some delay. To
determine if a data source is ready to have some frames read, use
`ma_resource_manager_data_source_get_available_frames()`. This will return the number of frames
available starting from the current position.


Data Buffers
------------
When the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM` flag is excluded at initialization time, the
resource manager will try to load the data into an in-memory data buffer. Before doing so, however,
it will first check if the specified file has already been loaded. If so, it will increment a
reference counter and just use the already loaded data. This saves both time and memory. A binary
search tree (BST) is used for storing data buffers as it has good balance between efficiency and
simplicity. The key of the BST is a 64-bit hash of the file path that was passed into
`ma_resource_manager_data_source_init()`. The advantage of using a hash is that it saves memory
over storing the entire path, has faster comparisons, and results in a mostly balanced BST due to
the random nature of the hash. The disadvantage is that file names are case-sensitive. If this is
an issue, you should normalize your file names to upper- or lower-case before initializing your
data sources.

When a sound file has not already been loaded and the `MMA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC`
is excluded, the file will be decoded synchronously by the calling thread. There are two options
for controlling how the audio is stored in the data buffer - encoded or decoded. When the
`MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE` option is excluded, the raw file data will be stored
in memory. Otherwise the sound will be decoded before storing it in memory. Synchronous loading is
a very simple and standard process of simply adding an item to the BST, allocating a block of
memory and then decoding (if `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE` is specified).

When the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC` flag is specified, loading of the data buffer
is done asynchronously. In this case, a job is posted to the queue to start loading and then the
function immediately returns, setting an internal result code to `MA_BUSY`. This result code is
returned when the program calls `ma_resource_manager_data_source_result()`. When decoding has fully
completed `MA_RESULT` will be returned. This can be used to know if loading has fully completed.

When loading asynchronously, a single job is posted to the queue of the type
`MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER_NODE`. This involves making a copy of the file path and
associating it with job. When the job is processed by the job thread, it will first load the file
using the VFS associated with the resource manager. When using a custom VFS, it's important that it
be completely thread-safe because it will be used from one or more job threads at the same time.
Individual files should only ever be accessed by one thread at a time, however. After opening the
file via the VFS, the job will determine whether or not the file is being decoded. If not, it
simply allocates a block of memory and loads the raw file contents into it and returns. On the
other hand, when the file is being decoded, it will first allocate a decoder on the heap and
initialize it. Then it will check if the length of the file is known. If so it will allocate a
block of memory to store the decoded output and initialize it to silence. If the size is unknown,
it will allocate room for one page. After memory has been allocated, the first page will be
decoded. If the sound is shorter than a page, the result code will be set to `MA_SUCCESS` and the
completion event will be signalled and loading is now complete. If, however, there is more to
decode, a job with the code `MA_RESOURCE_MANAGER_JOB_PAGE_DATA_BUFFER_NODE` is posted. This job
will decode the next page and perform the same process if it reaches the end. If there is more to
decode, the job will post another `MA_RESOURCE_MANAGER_JOB_PAGE_DATA_BUFFER_NODE` job which will
keep on happening until the sound has been fully decoded. For sounds of an unknown length, the
buffer will be dynamically expanded as necessary, and then shrunk with a final realloc() when the
end of the file has been reached.


Data Streams
------------
Data streams only ever store two pages worth of data for each instance. They are most useful for
large sounds like music tracks in games that would consume too much memory if fully decoded in
memory. After every frame from a page has been read, a job will be posted to load the next page
which is done from the VFS.

For data streams, the `MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC` flag will determine whether or
not initialization of the data source waits until the two pages have been decoded. When unset,
`ma_resource_manager_data_source_init()` will wait until the two pages have been loaded, otherwise
it will return immediately.

When frames are read from a data stream using `ma_resource_manager_data_source_read_pcm_frames()`,
`MA_BUSY` will be returned if there are no frames available. If there are some frames available,
but less than the number requested, `MA_SUCCESS` will be returned, but the actual number of frames
read will be less than the number requested. Due to the asymchronous nature of data streams,
seeking is also asynchronous. If the data stream is in the middle of a seek, `MA_BUSY` will be
returned when trying to read frames.

When `ma_resource_manager_data_source_read_pcm_frames()` results in a page getting fully consumed
a job is posted to load the next page. This will be posted from the same thread that called
`ma_resource_manager_data_source_read_pcm_frames()` which should be lock-free.

Data streams are uninitialized by posting a job to the queue, but the function won't return until
that job has been processed. The reason for this is that the caller owns the data stream object and
therefore miniaudio needs to ensure everything completes before handing back control to the caller.
Also, if the data stream is uninitialized while pages are in the middle of decoding, they must
complete before destroying any underlying object and the job system handles this cleanly.


Job Queue
---------
The resource manager uses a job queue which is multi-producer, multi-consumer, lock-free and
fixed-capacity. The lock-free property of the queue is achieved using the algorithm described by
Michael and Scott: Nonblocking Algorithms and Preemption-Safe Locking on Multiprogrammed Shared
Memory Multiprocessors. In order for this to work, only a fixed number of jobs can be allocated and
inserted into the queue which is done through a lock-free data structure for allocating an index
into a fixed sized array, with reference counting for mitigation of the ABA problem. The reference
count is 32-bit.

For many types of jobs it's important that they execute in a specific order. In these cases, jobs
are executed serially. For the resource manager, serial execution of jobs is only required on a
per-object basis (per data buffer or per data stream). Each of these objects stores an execution
counter. When a job is posted it is associated with an execution counter. When the job is
processed, it checks if the execution counter of the job equals the execution counter of the
owning object and if so, processes the job. If the counters are not equal, the job will be posted
back onto the job queue for later processing. When the job finishes processing the execution order
of the main object is incremented. This system means the no matter how many job threads are
executing, decoding of an individual sound will always get processed serially. The advantage to
having multiple threads comes into play when loading multiple sounds at the time time.
*/


/*
Routing Infrastructure
======================
miniaudio's routing infrastructure follows a node graph paradigm. The idea is that you create a
node whose outputs are attached to inputs of another node, thereby creating a graph. There are
different types of nodes, with each node in the graph processing input data to produce output,
which is then fed through the chain. Each node in the graph can apply their own custom effects. At
the end of the graph is an endpoint which represents the end of the chain and is where the final
output is ultimately extracted from.

Each node has a number of input buses and a number of output buses. An output bus from a node is
attached to an input bus of another. Multiple nodes can connect their output buses to another
node's input bus, in which case their outputs will be mixed before processing by the node.

Each input bus must be configured to accept the same number of channels, but input buses and output
buses can each have different channel counts, in which case miniaudio will automatically convert
the input data to the output channel count before processing. The number of channels of an output
bus of one node must match the channel count of the input bus it's attached to. The channel counts
cannot be changed after the node has been initialized. If you attempt to attach an output bus to
an input bus with a different channel count, attachment will fail.

To use a node graph, you first need to initialize a `ma_node_graph` object. This is essentially a
container around the entire graph. The `ma_node_graph` object is required for some thread-safety
issues which will be explained later. A `ma_node_graph` object is initialized using miniaudio's
standard config/init system:

    ```c
    ma_node_graph_config nodeGraphConfig = ma_node_graph_config_init(myChannelCount);

    result = ma_node_graph_init(&nodeGraphConfig, NULL, &nodeGraph);    // Second parameter is a pointer to allocation callbacks.
    if (result != MA_SUCCESS) {
        // Failed to initialize node graph.
    }
    ```

When you initialize the node graph, you're specifying the channel count of the endpoint. The
endpoint is a special node which has one input bus and one output bus, both of which have the
same channel count, which is specified in the config. Any nodes that connect directly to the
endpoint must be configured such that their output buses have the same channel count. When you read
audio data from the node graph, it'll have the channel count you specified in the config. To read
data from the graph:

    ```c
    ma_uint32 framesRead;
    result = ma_node_graph_read_pcm_frames(&nodeGraph, pFramesOut, frameCount, &framesRead);
    if (result != MA_SUCCESS) {
        // Failed to read data from the node graph.
    }
    ```

When you read audio data, miniaudio starts at the node graph's endpoint node which then pulls in
data from it's input attachments, which in turn recusively pull in data from their inputs, and so
on. At the very base of the graph there will be some kind of data source node which will have zero
inputs and will instead read directly from a data source. The base nodes don't literally need to
read from a `ma_data_source` object, but they will always have some kind of underlying object that
sources some kind of audio. The `ma_data_source_node` node can be used to read from a
`ma_data_source`. Data is always in floating-point format and in the number of channels you
specified when the graph was initialized. The sample rate is defined by the underlying data sources.
It's up to you to ensure they use a consistent and appropraite sample rate.

The `ma_node` API is designed to allow custom nodes to be implemented with relative ease, but
miniaudio includes a few stock nodes for common functionality. This is how you would initialize a
node which reads directly from a data source (`ma_data_source_node`) which is an example of one
of the stock nodes that comes with miniaudio:

    ```c
    ma_data_source_node_config config = ma_data_source_node_config_init(pMyDataSource, isLooping);

    ma_data_source_node dataSourceNode;
    result = ma_data_source_node_init(&nodeGraph, &config, NULL, &dataSourceNode);
    if (result != MA_SUCCESS) {
        // Failed to create data source node.
    }
    ```

The data source node will use the output channel count to determine the channel count of the output
bus. There will be 1 output bus and 0 input buses (data will be drawn directly from the data
source). The data source must output to floating-point (ma_format_f32) or else an error will be
returned from `ma_data_source_node_init()`.

By default the node will not be attached to the graph. To do so, use `ma_node_attach_output_bus()`:

    ```c
    result = ma_node_attach_output_bus(&dataSourceNode, 0, ma_node_graph_get_endpoint(&nodeGraph), 0);
    if (result != MA_SUCCESS) {
        // Failed to attach node.
    }
    ```

The code above connects the data source node directly to the endpoint. Since the data source node
has only a single output bus, the index will always be 0. Likewise, the endpoint only has a single
input bus which means the input bus index will also always be 0.

To detach a specific output bus, use `ma_node_detach_output_bus()`. To detach all output buses, use
`ma_node_detach_all_output_buses()`. If you want to just move the output bus from one attachment to
another, you do not need to detach first. You can just call `ma_node_attach_output_bus()` and it'll
deal with it for you.

Less frequently you may want to create a specialized node. This will be a node where you implement
your own processing callback to apply a custom effect of some kind. This is similar to initalizing
one of the stock node types, only this time you need to specify a pointer to a vtable containing a
pointer to the processing function and the number of input and output buses. Example:

    ```c
    static void my_custom_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
    {
        // Do some processing of ppFramesIn (one stream of audio data per input bus)
        const float* pFramesIn_0 = ppFramesIn[0]; // Input bus @ index 0.
        const float* pFramesIn_1 = ppFramesIn[1]; // Input bus @ index 1.
        float* pFramesOut_0 = ppFramesOut[0];     // Output bus @ index 0.

        // Do some processing. On input, `pFrameCountIn` will be the number of input frames in each
        // buffer in `ppFramesIn` and `pFrameCountOut` will be the capacity of each of the buffers
        // in `ppFramesOut`. On output, `pFrameCountIn` should be set to the number of input frames
        // your node consumed and `pFrameCountOut` should be set the number of output frames that
        // were produced.
        //
        // You should process as many frames as you can. If your effect consumes input frames at the
        // same rate as output frames (always the case, unless you're doing resampling), you need
        // only look at `ppFramesOut` and process that exact number of frames. If you're doing
        // resampling, you'll need to be sure to set both `pFrameCountIn` and `pFrameCountOut`
        // properly.
    }

    static ma_node_vtable my_custom_node_vtable = 
    {
        my_custom_node_process_pcm_frames, // The function that will be called process your custom node. This is where you'd implement your effect processing.
        NULL,   // Optional. A callback for calculating the number of input frames that are required to process a specified number of output frames.
        2,      // 2 input buses.
        1,      // 1 output bus.
        0       // Default flags.
    };

    ...

    // Each bus needs to have a channel count specified. To do this you need to specify the channel
    // counts in an array and then pass that into the node config.
    ma_uint32 inputChannels[2];     // Equal in size to the number of input channels specified in the vtable.
    ma_uint32 outputChannels[1];    // Equal in size to the number of output channels specicied in the vtable.

    inputChannels[0]  = channelsIn;
    inputChannels[1]  = channelsIn;
    outputChannels[0] = channelsOut;
    
    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable          = &my_custom_node_vtable;
    nodeConfig.pInputChannels  = inputChannels;
    nodeConfig.pOutputChannels = outputChannels;

    ma_node_base node;
    result = ma_node_init(&nodeGraph, &nodeConfig, NULL, &node);
    if (result != MA_SUCCESS) {
        // Failed to initialize node.
    }
    ```

When initializing a custom node, as in the code above, you'll normally just place your vtable in
static space. The number of input and output buses are specified as part of the vtable. If you need
a variable number of buses on a per-node bases, the vtable should have the relevant bus count set
to `MA_NODE_BUS_COUNT_UNKNOWN`. In this case, the bus count should be set in the node config:

    ```c
    static ma_node_vtable my_custom_node_vtable = 
    {
        my_custom_node_process_pcm_frames, // The function that will be called process your custom node. This is where you'd implement your effect processing.
        NULL,   // Optional. A callback for calculating the number of input frames that are required to process a specified number of output frames.
        MA_NODE_BUS_COUNT_UNKNOWN,  // The number of input buses is determined on a per-node basis.
        1,      // 1 output bus.
        0       // Default flags.
    };

    ...

    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable          = &my_custom_node_vtable;
    nodeConfig.inputBusCount   = myBusCount;        // <-- Since the vtable specifies MA_NODE_BUS_COUNT_UNKNOWN, the input bus count should be set here.
    nodeConfig.pInputChannels  = inputChannels;     // <-- Make sure there are nodeConfig.inputBusCount elements in this array.
    nodeConfig.pOutputChannels = outputChannels;    // <-- The vtable specifies 1 output bus, so there must be 1 element in this array.
    ```

In the above example it's important to never set the `inputBusCount` and `outputBusCount` members
to anything other than their defaults if the vtable specifies an explicit count. They can only be
set if the vtable specifies MA_NODE_BUS_COUNT_UNKNOWN in the relevant bus count.

Most often you'll want to create a structure to encapsulate your node with some extra data. You
need to make sure the `ma_node_base` object is your first member of the structure:

    ```c
    typedef struct
    {
        ma_node_base base; // <-- Make sure this is always the first member.
        float someCustomData;
    } my_custom_node;
    ```

By doing this, your object will be compatible with all `ma_node` APIs and you can attach it to the
graph just like any other node.

In the custom processing callback (`my_custom_node_process_pcm_frames()` in the example above), the
number of channels for each bus is what was specified by the config when the node was initialized
with `ma_node_init()`. In addition, all attachments to each of the input buses will have been
pre-mixed by miniaudio. The config allows you to specify different channel counts for each
individual input and output bus. It's up to the effect to handle it appropriate, and if it can't,
return an error in it's initialization routine.

Custom nodes can be assigned some flags to describe their behaviour. These are set via the vtable
and include the following:

    +-----------------------------------------+---------------------------------------------------+
    | Flag Name                               | Description                                       |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_PASSTHROUGH                | Useful for nodes that do not do any kind of audio |
    |                                         | processing, but are instead used for tracking     |
    |                                         | time, handling events, etc. Also used by the      |
    |                                         | internal endpoint node. It reads directly from    |
    |                                         | the input bus to the output bus. Nodes with this  |
    |                                         | flag must have exactly 1 input bus and 1 output   |
    |                                         | bus, and both buses must have the same channel    |
    |                                         | counts.                                           |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_CONTINUOUS_PROCESSING      | Causes the processing callback to be called even  |
    |                                         | when no data is available to be read from input   |
    |                                         | attachments. This is useful for effects like      |
    |                                         | echos where there will be a tail of audio data    |
    |                                         | that still needs to be processed even when the    |
    |                                         | original data sources have reached their ends.    |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_ALLOW_NULL_INPUT           | Used in conjunction with                          |
    |                                         | `MA_NODE_FLAG_CONTINUOUS_PROCESSING`. When this   |
    |                                         | is set, the `ppFramesIn` parameter of the         |
    |                                         | processing callback will be set to NULL when      |
    |                                         | there are no input frames are available. When     |
    |                                         | this is unset, silence will be posted to the      |
    |                                         | processing callback.                              |
    +-----------------------------------------+---------------------------------------------------+
    | MA_NODE_FLAG_DIFFERENT_PROCESSING_RATES | Used to tell miniaudio that input and output      |
    |                                         | frames are processed at different rates. You      |
    |                                         | should set this for any nodes that perform        |
    |                                         | resampling.                                       |
    +-----------------------------------------+---------------------------------------------------+


If you need to make a copy of an audio stream for effect processing you can use a splitter node
called `ma_splitter_node`. This takes has 1 input bus and splits the stream into 2 output buses.
You can use it like this:

    ```c
    ma_splitter_node_config splitterNodeConfig = ma_splitter_node_config_init(channelsIn, channelsOut);

    ma_splitter_node splitterNode;
    result = ma_splitter_node_init(&nodeGraph, &splitterNodeConfig, NULL, &splitterNode);
    if (result != MA_SUCCESS) {
        // Failed to create node.
    }

    // Attach your output buses to two different input buses (can be on two different nodes).
    ma_node_attach_output_bus(&splitterNode, 0, ma_node_graph_get_endpoint(&nodeGraph), 0); // Attach directly to the endpoint.
    ma_node_attach_output_bus(&splitterNode, 1, &myEffectNode,                          0); // Attach to input bus 0 of some effect node.
    ```

The volume of an output bus can be configured on a per-bus basis:

    ```c
    ma_node_set_output_bus_volume(&splitterNode, 0, 0.5f);
    ma_node_set_output_bus_volume(&splitterNode, 1, 0.5f);
    ```

In the code above we're using the splitter node from before and changing the volume of each of the
copied streams.

You can start, stop and mute a node with the following:

    ```c
    ma_node_set_state(&splitterNode, ma_node_state_started);    // The default state.
    ma_node_set_state(&splitterNode, ma_node_state_stopped);
    ma_node_set_state(&splitterNode, ma_node_state_muted);
    ```

By default the node is in a started state, but since it won't be connected to anything won't
actually be invoked by the node graph until it's connected. When you stop a node, data will not be
read from any of it's input connections. You can use this property to stop a group of sounds
atomically.

You can configure the initial state of a node in it's config:

    ```c
    nodeConfig.initialState = ma_node_state_stopped;
    ```

Note that for the stock specialized nodes, all of their configs will have a `nodeConfig` member
which is the config to use with the base node. This is where the initial state can be configured
for specialized nodes:

    ```c
    dataSourceNodeConfig.nodeConfig.initialState = ma_node_state_stopped;
    ```

When using a specialized node like `ma_data_source_node` or `ma_splitter_node`, be sure to not
modify the `vtable` member of the `nodeConfig` object.


Timing
------
The node graph supports starting and stopping nodes at scheduled times. This is especially useful
for data source nodes where you want to get the node set up, but only start playback at a specific
time. There are two clocks: local and global.

A local clock is per-node, whereas the global clock is per graph. Scheduling starts and stops can
only be done based on the global clock because the local clock will not be running while the node
is stopped. The global clocks advances whenever `ma_node_graph_read_pcm_frames()` is called. On the
other hand, the local clock only advances when the node's processing callback is fired, and is
advanced based on the output frame count.

To retrieve the global time, use `ma_node_graph_get_time()`. The global time can be set with
`ma_node_graph_set_time()` which might be useful if you want to do seeking on a global timeline.
Getting and setting the local time is similar. Use `ma_node_get_time()` to retrieve the local time,
and `ma_node_set_time()` to set the local time. The global and local times will be advanced by the
audio thread, so care should be taken to avoid data races. Ideally you should avoid calling these
outside of the node processing callbacks which are always run on the audio thread.

There is basic support for scheduling the starting and stopping of nodes. You can only schedule one
start and one stop at a time. This is mainly intended for putting nodes into a started or stopped
state in a frame-exact manner. Without this mechanism, starting and stopping of a node is limited
to the resolution of a call to `ma_node_graph_read_pcm_frames()` which would typically be in blocks
of several milliseconds. The following APIs can be used for scheduling node states:

    ```c
    ma_node_set_state_time()
    ma_node_get_state_time()
    ```

The time is absolute and must be based on the global clock. An example is below:

    ```c
    ma_node_set_state_time(&myNode, ma_node_state_started, sampleRate*1);   // Delay starting to 1 second.
    ma_node_set_state_time(&myNode, ma_node_state_stopped, sampleRate*5);   // Delay stopping to 5 seconds.
    ```

An example for changing the state using a relative time.

    ```c
    ma_node_set_state_time(&myNode, ma_node_state_started, sampleRate*1 + ma_node_graph_get_time(&myNodeGraph));
    ma_node_set_state_time(&myNode, ma_node_state_stopped, sampleRate*5 + ma_node_graph_get_time(&myNodeGraph));
    ```

Note that due to the nature of multi-threading the times may not be 100% exact. If this is an
issue, consider scheduling state changes from within a processing callback. An idea might be to
have some kind of passthrough trigger node that is used specifically for tracking time and handling
events.



Thread Safety and Locking
-------------------------
When processing audio, it's ideal not to have any kind of locking in the audio thread. Since it's
expected that `ma_node_graph_read_pcm_frames()` would be run on the audio thread, it does so
without the use of any locks. This section discusses the implementation used by miniaudio and goes
over some of the compromises employed by miniaudio to achieve this goal. Note that the current
implementation may not be ideal - feedback and critiques are most welcome.

The node graph API is not *entirely* lock-free. Only `ma_node_graph_read_pcm_frames()` is expected
to be lock-free. Attachment, detachment and uninitialization of nodes use locks to simplify the
implementation, but are crafted in a way such that such locking is not required when reading audio
data from the graph. Locking in these areas are achieved by means of spinlocks.

The main complication with keeping `ma_node_graph_read_pcm_frames()` lock-free stems from the fact
that a node can be uninitialized, and it's memory potentially freed, while in the middle of being
processed on the audio thread. There are times when the audio thread will be referencing a node,
which means the uninitialization process of a node needs to make sure it delays returning until the
audio thread is finished so that control is not handed back to the caller thereby giving them a
chance to free the node's memory.

When the audio thread is processing a node, it does so by reading from each of the output buses of
the node. In order for a node to process data for one of it's output buses, it needs to read from
each of it's input buses, and so on an so forth. It follows that once all output buses of a node
are detached, the node as a whole will be disconnected and no further processing will occur unless
it's output buses are reattached, which won't be happening when the node is being uninitialized.
By having `ma_node_detach_output_bus()` wait until the audio thread is finished with it, we can
simplify a few things, at the expense of making `ma_node_detach_output_bus()` a bit slower. By
doing this, the implementation of `ma_node_uninit()` becomes trivial - just detach all output
nodes, followed by each of the attachments to each of it's input nodes, and then do any final clean
up.

With the above design, the worst-case scenario is `ma_node_detach_output_bus()` taking as long as
it takes to process the output bus being detached. This will happen if it's called at just the
wrong moment where the audio thread has just iterated it and has just started processing. The
caller of `ma_node_detach_output_bus()` will stall until the audio thread is finished, which
includes the cost of recursively processing it's inputs. This is the biggest compromise made with
the approach taken by miniaudio for it's lock-free processing system. The cost of detaching nodes
earlier in the pipeline (data sources, for example) will be cheaper than the cost of detaching
higher level nodes, such as some kind of final post-processing endpoint. If you need to do mass
detachments, detach starting from the lowest level nodes and work your way towards the final
endpoint node (but don't try detaching the node graph's endpoint). If the audio thread is not
running, detachment will be fast and detachment in any order will be the same. The reason nodes
need to wait for their input attachments to complete is due to the potential for desyncs between
data sources. If the node was to terminate processing mid way through processing it's inputs,
there's a chance that some of the underlying data sources will have been read, but then others not.
That will then result in a potential desynchronization when detaching and reattaching higher-level
nodes. A possible solution to this is to have an option when detaching to terminate processing
before processing all input attachments which should be fairly simple.

Another compromise, albeit less significant, is locking when attaching and detaching nodes. This
locking is achieved by means of a spinlock in order to reduce memory overhead. A lock is present
for each input bus and output bus. When an output bus is connected to an input bus, both the output
bus and input bus is locked. This locking is specifically for attaching and detaching across
different threads and does not affect `ma_node_graph_read_pcm_frames()` in any way. The locking and
unlocking is mostly self-explanatory, but a slightly less intuitive aspect comes into it when
considering that iterating over attachments must not break as a result of attaching or detaching a
node while iteration is occuring.

Attaching and detaching are both quite simple. When an output bus of a node is attached to an input
bus of another node, it's added to a linked list. Basically, an input bus is a linked list, where
each item in the list is and output bus. We have some intentional (and convenient) restrictions on
what can done with the linked list in order to simplify the implementation. First of all, whenever
something needs to iterate over the list, it must do so in a forward direction. Backwards iteration
is not supported. Also, items can only be added to the start of the list.

The linked list is a doubly-linked list where each item in the list (an output bus) holds a pointer
to the next item in the list, and another to the previous item. A pointer to the previous item is
only required for fast detachment of the node - it is never used in iteration. This is an
important property because it means from the perspective of iteration, attaching and detaching of
an item can be done with a single atomic assignment. This is exploited by both the attachment and
detachment process. When attaching the node, the first thing that is done is the setting of the
local "next" and "previous" pointers of the node. After that, the item is "attached" to the list
by simply performing an atomic exchange with the head pointer. After that, the node is "attached"
to the list from the perspective of iteration. Even though the "previous" pointer of the next item
hasn't yet been set, from the perspective of iteration it's been attached because iteration will
only be happening in a forward direction which means the "previous" pointer won't actually ever get
used. The same general process applies to detachment. See `ma_node_attach_output_bus()` and
`ma_node_detach_output_bus()` for the implementation of this mechanism.
*/




/*
Engine
======
The `ma_engine` API is a high-level API for audio playback. Internally it contains sounds (`ma_sound`) with resources managed via a resource manager
(`ma_resource_manager`).

Within the world there is the concept of a "listener". Each `ma_engine` instances has a single listener, but you can instantiate multiple `ma_engine` instances
if you need more than one listener. In this case you will want to share a resource manager which you can do by initializing one manually and passing it into
`ma_engine_config`. Using this method will require your application to manage groups and sounds on a per `ma_engine` basis.
*/
typedef struct ma_engine ma_engine;
typedef struct ma_sound  ma_sound;


/* Gainer for smooth volume changes. */
typedef struct
{
    ma_uint32 channels;
    ma_uint32 smoothTimeInFrames;
} ma_gainer_config;

MA_API ma_gainer_config ma_gainer_config_init(ma_uint32 channels, ma_uint32 smoothTimeInFrames);


typedef struct
{
    ma_gainer_config config;
    ma_uint32 t;
    float* pOldGains;
    float* pNewGains;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_gainer;

MA_API ma_result ma_gainer_get_heap_size(const ma_gainer_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_gainer_init_preallocated(const ma_gainer_config* pConfig, void* pHeap, ma_gainer* pGainer);
MA_API ma_result ma_gainer_init(const ma_gainer_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_gainer* pGainer);
MA_API void ma_gainer_uninit(ma_gainer* pGainer, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_gainer_process_pcm_frames(ma_gainer* pGainer, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_result ma_gainer_set_gain(ma_gainer* pGainer, float newGain);
MA_API ma_result ma_gainer_set_gains(ma_gainer* pGainer, float* pNewGains);



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
    ma_format format;
    ma_uint32 channels;
    ma_pan_mode mode;
    float pan;  /* -1..1 where 0 is no pan, -1 is left side, +1 is right side. Defaults to 0. */
} ma_panner;

MA_API ma_result ma_panner_init(const ma_panner_config* pConfig, ma_panner* pPanner);
MA_API ma_result ma_panner_process_pcm_frames(ma_panner* pPanner, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API void ma_panner_set_mode(ma_panner* pPanner, ma_pan_mode mode);
MA_API void ma_panner_set_pan(ma_panner* pPanner, float pan);



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
    ma_fader_config config;
    float volumeBeg;            /* If volumeBeg and volumeEnd is equal to 1, no fading happens (ma_fader_process_pcm_frames() will run as a passthrough). */
    float volumeEnd;
    ma_uint64 lengthInFrames;   /* The total length of the fade. */
    ma_uint64 cursorInFrames;   /* The current time in frames. Incremented by ma_fader_process_pcm_frames(). */
} ma_fader;

MA_API ma_result ma_fader_init(const ma_fader_config* pConfig, ma_fader* pFader);
MA_API ma_result ma_fader_process_pcm_frames(ma_fader* pFader, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API void ma_fader_get_data_format(const ma_fader* pFader, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate);
MA_API void ma_fader_set_fade(ma_fader* pFader, float volumeBeg, float volumeEnd, ma_uint64 lengthInFrames);
MA_API float ma_fader_get_current_volume(ma_fader* pFader);



/* Spatializer. */
typedef struct
{
    float x;
    float y;
    float z;
} ma_vec3f;

typedef enum
{
    ma_attenuation_model_none,          /* No distance attenuation and no spatialization. */
    ma_attenuation_model_inverse,       /* Equivalent to OpenAL's AL_INVERSE_DISTANCE_CLAMPED. */
    ma_attenuation_model_linear,        /* Linear attenuation. Equivalent to OpenAL's AL_LINEAR_DISTANCE_CLAMPED. */
    ma_attenuation_model_exponential    /* Exponential attenuation. Equivalent to OpenAL's AL_EXPONENT_DISTANCE_CLAMPED. */
} ma_attenuation_model;

typedef enum
{
    ma_positioning_absolute,
    ma_positioning_relative
} ma_positioning;

typedef enum
{
    ma_handedness_right,
    ma_handedness_left
} ma_handedness;


typedef struct
{
    ma_uint32 channelsOut;
    ma_channel* pChannelMapOut;
    ma_handedness handedness;   /* Defaults to right. Forward is -1 on the Z axis. In a left handed system, forward is +1 on the Z axis. */
    float coneInnerAngleInRadians;
    float coneOuterAngleInRadians;
    float coneOuterGain;
    float speedOfSound;
    ma_vec3f worldUp;
} ma_spatializer_listener_config;

MA_API ma_spatializer_listener_config ma_spatializer_listener_config_init(ma_uint32 channelsOut);


typedef struct
{
    ma_spatializer_listener_config config;
    ma_vec3f position;  /* The absolute position of the listener. */
    ma_vec3f direction; /* The direction the listener is facing. The world up vector is config.worldUp. */
    ma_vec3f velocity;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_spatializer_listener;

MA_API ma_result ma_spatializer_listener_get_heap_size(const ma_spatializer_listener_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_spatializer_listener_init_preallocated(const ma_spatializer_listener_config* pConfig, void* pHeap, ma_spatializer_listener* pListener);
MA_API ma_result ma_spatializer_listener_init(const ma_spatializer_listener_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_uninit(ma_spatializer_listener* pListener, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_channel* ma_spatializer_listener_get_channel_map(ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_cone(ma_spatializer_listener* pListener, float innerAngleInRadians, float outerAngleInRadians, float outerGain);
MA_API void ma_spatializer_listener_get_cone(const ma_spatializer_listener* pListener, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain);
MA_API void ma_spatializer_listener_set_position(ma_spatializer_listener* pListener, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_listener_get_position(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_direction(ma_spatializer_listener* pListener, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_listener_get_direction(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_velocity(ma_spatializer_listener* pListener, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_listener_get_velocity(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_speed_of_sound(ma_spatializer_listener* pListener, float speedOfSound);
MA_API float ma_spatializer_listener_get_speed_of_sound(const ma_spatializer_listener* pListener);
MA_API void ma_spatializer_listener_set_world_up(ma_spatializer_listener* pListener, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_listener_get_world_up(const ma_spatializer_listener* pListener);


typedef struct
{
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_channel* pChannelMapIn;
    ma_attenuation_model attenuationModel;
    ma_positioning positioning;
    ma_handedness handedness;           /* Defaults to right. Forward is -1 on the Z axis. In a left handed system, forward is +1 on the Z axis. */
    float minGain;
    float maxGain;
    float minDistance;
    float maxDistance;
    float rolloff;
    float coneInnerAngleInRadians;
    float coneOuterAngleInRadians;
    float coneOuterGain;
    float dopplerFactor;                /* Set to 0 to disable doppler effect. This will run on a fast path. */
    ma_uint32 gainSmoothTimeInFrames;   /* When the gain of a channel changes during spatialization, the transition will be linearly interpolated over this number of frames. */
} ma_spatializer_config;

MA_API ma_spatializer_config ma_spatializer_config_init(ma_uint32 channelsIn, ma_uint32 channelsOut);


typedef struct
{
    ma_spatializer_config config;
    ma_vec3f position;
    ma_vec3f direction;
    ma_vec3f velocity;  /* For doppler effect. */
    float dopplerPitch; /* Will be updated by ma_spatializer_process_pcm_frames() and can be used by higher level functions to apply a pitch shift for doppler effect. */
    ma_gainer gainer;   /* For smooth gain transitions. */
    float* pNewChannelGainsOut; /* An offset of _pHeap. Used by ma_spatializer_process_pcm_frames() to store new channel gains. The number of elements in this array is equal to config.channelsOut. */

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_spatializer;

MA_API ma_result ma_spatializer_get_heap_size(const ma_spatializer_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_spatializer_init_preallocated(const ma_spatializer_config* pConfig, void* pHeap, ma_spatializer* pSpatializer);
MA_API ma_result ma_spatializer_init(const ma_spatializer_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_spatializer* pSpatializer);
MA_API void ma_spatializer_uninit(ma_spatializer* pSpatializer, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_spatializer_process_pcm_frames(ma_spatializer* pSpatializer, ma_spatializer_listener* pListener, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount);
MA_API ma_uint32 ma_spatializer_get_input_channels(const ma_spatializer* pSpatializer);
MA_API ma_uint32 ma_spatializer_get_output_channels(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_attenuation_model(ma_spatializer* pSpatializer, ma_attenuation_model attenuationModel);
MA_API ma_attenuation_model ma_spatializer_get_attenuation_model(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_positioning(ma_spatializer* pSpatializer, ma_positioning positioning);
MA_API ma_positioning ma_spatializer_get_positioning(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_rolloff(ma_spatializer* pSpatializer, float rolloff);
MA_API float ma_spatializer_get_rolloff(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_min_gain(ma_spatializer* pSpatializer, float minGain);
MA_API float ma_spatializer_get_min_gain(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_max_gain(ma_spatializer* pSpatializer, float maxGain);
MA_API float ma_spatializer_get_max_gain(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_min_distance(ma_spatializer* pSpatializer, float minDistance);
MA_API float ma_spatializer_get_min_distance(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_max_distance(ma_spatializer* pSpatializer, float maxDistance);
MA_API float ma_spatializer_get_max_distance(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_cone(ma_spatializer* pSpatializer, float innerAngleInRadians, float outerAngleInRadians, float outerGain);
MA_API void ma_spatializer_get_cone(const ma_spatializer* pSpatializer, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain);
MA_API void ma_spatializer_set_doppler_factor(ma_spatializer* pSpatializer, float dopplerFactor);
MA_API float ma_spatializer_get_doppler_factor(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_position(ma_spatializer* pSpatializer, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_get_position(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_direction(ma_spatializer* pSpatializer, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_get_direction(const ma_spatializer* pSpatializer);
MA_API void ma_spatializer_set_velocity(ma_spatializer* pSpatializer, float x, float y, float z);
MA_API ma_vec3f ma_spatializer_get_velocity(const ma_spatializer* pSpatializer);



/* Sound flags. */
#define MA_SOUND_FLAG_STREAM                MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM      /* 0x00000001 */
#define MA_SOUND_FLAG_DECODE                MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE      /* 0x00000002 */
#define MA_SOUND_FLAG_ASYNC                 MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC       /* 0x00000004 */
#define MA_SOUND_FLAG_WAIT_INIT             MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT   /* 0x00000008 */
#define MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT 0x00000010  /* Do not attach to the endpoint by default. Useful for when setting up nodes in a complex graph system. */
#define MA_SOUND_FLAG_NO_PITCH              0x00000020  /* Disable pitch shifting with ma_sound_set_pitch() and ma_sound_group_set_pitch(). This is an optimization. */
#define MA_SOUND_FLAG_NO_SPATIALIZATION     0x00000040  /* Disable spatialization. */

#ifndef MA_ENGINE_MAX_LISTENERS
#define MA_ENGINE_MAX_LISTENERS             4
#endif

#define MA_LISTENER_INDEX_CLOSEST           ((ma_uint8)-1)

typedef enum
{
    ma_engine_node_type_sound,
    ma_engine_node_type_group
} ma_engine_node_type;

typedef struct
{
    ma_engine* pEngine;
    ma_engine_node_type type;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_uint32 sampleRate;               /* Only used when the type is set to ma_engine_node_type_sound. */
    ma_bool8 isPitchDisabled;           /* Pitching can be explicitly disable with MA_SOUND_FLAG_NO_PITCH to optimize processing. */
    ma_bool8 isSpatializationDisabled;  /* Spatialization can be explicitly disabled with MA_SOUND_FLAG_NO_SPATIALIZATION. */
    ma_uint8 pinnedListenerIndex;       /* The index of the listener this node should always use for spatialization. If set to MA_LISTENER_INDEX_CLOSEST the engine will use the closest listener. */
} ma_engine_node_config;

MA_API ma_engine_node_config ma_engine_node_config_init(ma_engine* pEngine, ma_engine_node_type type, ma_uint32 flags);


/* Base node object for both ma_sound and ma_sound_group. */
typedef struct
{
    ma_node_base baseNode;                          /* Must be the first member for compatiblity with the ma_node API. */
    ma_engine* pEngine;                             /* A pointer to the engine. Set based on the value from the config. */
    ma_uint32 sampleRate;                           /* The sample rate of the input data. For sounds backed by a data source, this will be the data source's sample rate. Otherwise it'll be the engine's sample rate. */
    ma_fader fader;
    ma_linear_resampler resampler;                  /* For pitch shift. */
    ma_spatializer spatializer;
    ma_panner panner;
    MA_ATOMIC float pitch;
    float oldPitch;                                 /* For determining whether or not the resampler needs to be updated to reflect the new pitch. The resampler will be updated on the mixing thread. */
    float oldDopplerPitch;                          /* For determining whether or not the resampler needs to be updated to take a new doppler pitch into account. */
    MA_ATOMIC ma_bool8 isPitchDisabled;             /* When set to true, pitching will be disabled which will allow the resampler to be bypassed to save some computation. */
    MA_ATOMIC ma_bool8 isSpatializationDisabled;    /* Set to false by default. When set to false, will not have spatialisation applied. */
    MA_ATOMIC ma_uint8 pinnedListenerIndex;         /* The index of the listener this node should always use for spatialization. If set to MA_LISTENER_INDEX_CLOSEST the engine will use the closest listener. */

    /* Memory management. */
    ma_bool8 _ownsHeap;
    void* _pHeap;
} ma_engine_node;

MA_API ma_result ma_engine_node_get_heap_size(const ma_engine_node_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_engine_node_init_preallocated(const ma_engine_node_config* pConfig, void* pHeap, ma_engine_node* pEngineNode);
MA_API ma_result ma_engine_node_init(const ma_engine_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_engine_node* pEngineNode);
MA_API void ma_engine_node_uninit(ma_engine_node* pEngineNode, const ma_allocation_callbacks* pAllocationCallbacks);


typedef struct
{
    const char* pFilePath;                      /* Set this to load from the resource manager. */
    const wchar_t* pFilePathW;                  /* Set this to load from the resource manager. */
    ma_data_source* pDataSource;                /* Set this to load from an existing data source. */
    ma_node* pInitialAttachment;                /* If set, the sound will be attached to an input of this node. This can be set to a ma_sound. If set to NULL, the sound will be attached directly to the endpoint unless MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT is set in `flags`. */
    ma_uint32 initialAttachmentInputBusIndex;   /* The index of the input bus of pInitialAttachment to attach the sound to. */
    ma_uint32 channelsIn;                       /* Ignored if using a data source as input (the data source's channel count will be used always). Otherwise, setting to 0 will cause the engine's channel count to be used. */
    ma_uint32 channelsOut;                      /* Set this to 0 (default) to use the engine's channel count. */
    ma_uint32 flags;                            /* A combination of MA_SOUND_FLAG_* flags. */
    ma_fence* pDoneFence;                       /* Released when the resource manager has finished decoding the entire sound. Not used with streams. */
} ma_sound_config;

MA_API ma_sound_config ma_sound_config_init(void);

struct ma_sound
{
    ma_engine_node engineNode;          /* Must be the first member for compatibility with the ma_node API. */
    ma_data_source* pDataSource;
    ma_uint64 seekTarget;               /* The PCM frame index to seek to in the mixing thread. Set to (~(ma_uint64)0) to not perform any seeking. */
    MA_ATOMIC ma_bool8 isLooping;       /* False by default. */
    MA_ATOMIC ma_bool8 atEnd;
    ma_bool8 ownsDataSource;

    /*
    We're declaring a resource manager data source object here to save us a malloc when loading a
    sound via the resource manager, which I *think* will be the most common scenario.
    */
#ifndef MA_NO_RESOURCE_MANAGER
    ma_resource_manager_data_source* pResourceManagerDataSource;
#endif
};

/* Structure specifically for sounds played with ma_engine_play_sound(). Making this a separate structure to reduce overhead. */
typedef struct ma_sound_inlined ma_sound_inlined;
struct ma_sound_inlined
{
    ma_sound sound;
    ma_sound_inlined* pNext;
    ma_sound_inlined* pPrev;
};

/* A sound group is just a sound. */
typedef ma_sound_config ma_sound_group_config;
typedef ma_sound        ma_sound_group;

MA_API ma_sound_group_config ma_sound_group_config_init(void);


typedef struct
{
    ma_resource_manager* pResourceManager;  /* Can be null in which case a resource manager will be created for you. */
    ma_context* pContext;
    ma_device* pDevice;                     /* If set, the caller is responsible for calling ma_engine_data_callback() in the device's data callback. */
    ma_log* pLog;                           /* When set to NULL, will use the context's log. */
    ma_uint32 listenerCount;                /* Must be between 1 and MA_ENGINE_MAX_LISTENERS. */
    ma_uint32 channels;                     /* The number of channels to use when mixing and spatializing. When set to 0, will use the native channel count of the device. */
    ma_uint32 sampleRate;                   /* The sample rate. When set to 0 will use the native channel count of the device. */
    ma_uint32 periodSizeInFrames;           /* If set to something other than 0, updates will always be exactly this size. The underlying device may be a different size, but from the perspective of the mixer that won't matter.*/
    ma_uint32 periodSizeInMilliseconds;     /* Used if periodSizeInFrames is unset. */
    ma_uint32 gainSmoothTimeInFrames;       /* The number of frames to interpolate the gain of spatialized sounds across. If set to 0, will use gainSmoothTimeInMilliseconds. */
    ma_uint32 gainSmoothTimeInMilliseconds; /* When set to 0, gainSmoothTimeInFrames will be used. If both are set to 0, a default value will be used. */
    ma_device_id* pPlaybackDeviceID;        /* The ID of the playback device to use with the default listener. */
    ma_allocation_callbacks allocationCallbacks;
    ma_bool32 noAutoStart;                  /* When set to true, requires an explicit call to ma_engine_start(). This is false by default, meaning the engine will be started automatically in ma_engine_init(). */
    ma_vfs* pResourceManagerVFS;            /* A pointer to a pre-allocated VFS object to use with the resource manager. This is ignored if pResourceManager is not NULL. */
} ma_engine_config;

MA_API ma_engine_config ma_engine_config_init(void);


struct ma_engine
{
    ma_node_graph nodeGraph;                /* An engine is a node graph. It should be able to be plugged into any ma_node_graph API (with a cast) which means this must be the first member of this struct. */
    ma_resource_manager* pResourceManager;
    ma_device* pDevice;                     /* Optionally set via the config, otherwise allocated by the engine in ma_engine_init(). */
    ma_log* pLog;
    ma_uint32 listenerCount;
    ma_spatializer_listener listeners[MA_ENGINE_MAX_LISTENERS];
    ma_allocation_callbacks allocationCallbacks;
    ma_bool8 ownsResourceManager;
    ma_bool8 ownsDevice;
    ma_mutex inlinedSoundLock;              /* For synchronizing access so the inlined sound list. */
    ma_sound_inlined* pInlinedSoundHead;    /* The first inlined sound. Inlined sounds are tracked in a linked list. */
    MA_ATOMIC ma_uint32 inlinedSoundCount;  /* The total number of allocated inlined sound objects. Used for debugging. */
    ma_uint32 gainSmoothTimeInFrames;       /* The number of frames to interpolate the gain of spatialized sounds across. */
};

MA_API ma_result ma_engine_init(const ma_engine_config* pConfig, ma_engine* pEngine);
MA_API void ma_engine_uninit(ma_engine* pEngine);
MA_API void ma_engine_data_callback(ma_engine* pEngine, void* pOutput, const void* pInput, ma_uint32 frameCount);
MA_API ma_device* ma_engine_get_device(ma_engine* pEngine);
MA_API ma_log* ma_engine_get_log(ma_engine* pEngine);
MA_API ma_node* ma_engine_get_endpoint(ma_engine* pEngine);
MA_API ma_uint64 ma_engine_get_time(const ma_engine* pEngine);
MA_API ma_uint64 ma_engine_set_time(ma_engine* pEngine, ma_uint64 globalTime);
MA_API ma_uint32 ma_engine_get_channels(const ma_engine* pEngine);
MA_API ma_uint32 ma_engine_get_sample_rate(const ma_engine* pEngine);

MA_API ma_result ma_engine_start(ma_engine* pEngine);
MA_API ma_result ma_engine_stop(ma_engine* pEngine);
MA_API ma_result ma_engine_set_volume(ma_engine* pEngine, float volume);
MA_API ma_result ma_engine_set_gain_db(ma_engine* pEngine, float gainDB);

MA_API ma_uint32 ma_engine_get_listener_count(const ma_engine* pEngine);
MA_API ma_uint32 ma_engine_find_closest_listener(const ma_engine* pEngine, float absolutePosX, float absolutePosY, float absolutePosZ);
MA_API void ma_engine_listener_set_position(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z);
MA_API ma_vec3f ma_engine_listener_get_position(const ma_engine* pEngine, ma_uint32 listenerIndex);
MA_API void ma_engine_listener_set_direction(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z);
MA_API ma_vec3f ma_engine_listener_get_direction(const ma_engine* pEngine, ma_uint32 listenerIndex);
MA_API void ma_engine_listener_set_velocity(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z);
MA_API ma_vec3f ma_engine_listener_get_velocity(const ma_engine* pEngine, ma_uint32 listenerIndex);
MA_API void ma_engine_listener_set_cone(ma_engine* pEngine, ma_uint32 listenerIndex, float innerAngleInRadians, float outerAngleInRadians, float outerGain);
MA_API void ma_engine_listener_get_cone(const ma_engine* pEngine, ma_uint32 listenerIndex, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain);
MA_API void ma_engine_listener_set_world_up(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z);
MA_API ma_vec3f ma_engine_listener_get_world_up(const ma_engine* pEngine, ma_uint32 listenerIndex);

MA_API ma_result ma_engine_play_sound(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup);   /* Fire and forget. */


#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_sound_init_from_file(ma_engine* pEngine, const char* pFilePath, ma_uint32 flags, ma_sound_group* pGroup, ma_fence* pDoneFence, ma_sound* pSound);
MA_API ma_result ma_sound_init_from_file_w(ma_engine* pEngine, const wchar_t* pFilePath, ma_uint32 flags, ma_sound_group* pGroup, ma_fence* pDoneFence, ma_sound* pSound);
MA_API ma_result ma_sound_init_copy(ma_engine* pEngine, const ma_sound* pExistingSound, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound);
#endif
MA_API ma_result ma_sound_init_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound);
MA_API ma_result ma_sound_init_ex(ma_engine* pEngine, const ma_sound_config* pConfig, ma_sound* pSound);
MA_API void ma_sound_uninit(ma_sound* pSound);
MA_API ma_engine* ma_sound_get_engine(const ma_sound* pSound);
MA_API ma_data_source* ma_sound_get_data_source(const ma_sound* pSound);
MA_API ma_result ma_sound_start(ma_sound* pSound);
MA_API ma_result ma_sound_stop(ma_sound* pSound);
MA_API ma_result ma_sound_set_volume(ma_sound* pSound, float volume);
MA_API ma_result ma_sound_set_gain_db(ma_sound* pSound, float gainDB);
MA_API void ma_sound_set_pan(ma_sound* pSound, float pan);
MA_API void ma_sound_set_pan_mode(ma_sound* pSound, ma_pan_mode panMode);
MA_API void ma_sound_set_pitch(ma_sound* pSound, float pitch);
MA_API void ma_sound_set_spatialization_enabled(ma_sound* pSound, ma_bool32 enabled);
MA_API void ma_sound_set_pinned_listener_index(ma_sound* pSound, ma_uint8 listenerIndex);
MA_API ma_uint8 ma_sound_get_pinned_listener_index(const ma_sound* pSound);
MA_API void ma_sound_set_position(ma_sound* pSound, float x, float y, float z);
MA_API ma_vec3f ma_sound_get_position(const ma_sound* pSound);
MA_API void ma_sound_set_direction(ma_sound* pSound, float x, float y, float z);
MA_API ma_vec3f ma_sound_get_direction(const ma_sound* pSound);
MA_API void ma_sound_set_velocity(ma_sound* pSound, float x, float y, float z);
MA_API ma_vec3f ma_sound_get_velocity(const ma_sound* pSound);
MA_API void ma_sound_set_attenuation_model(ma_sound* pSound, ma_attenuation_model attenuationModel);
MA_API ma_attenuation_model ma_sound_get_attenuation_model(const ma_sound* pSound);
MA_API void ma_sound_set_positioning(ma_sound* pSound, ma_positioning positioning);
MA_API ma_positioning ma_sound_get_positioning(const ma_sound* pSound);
MA_API void ma_sound_set_rolloff(ma_sound* pSound, float rolloff);
MA_API float ma_sound_get_rolloff(const ma_sound* pSound);
MA_API void ma_sound_set_min_gain(ma_sound* pSound, float minGain);
MA_API float ma_sound_get_min_gain(const ma_sound* pSound);
MA_API void ma_sound_set_max_gain(ma_sound* pSound, float maxGain);
MA_API float ma_sound_get_max_gain(const ma_sound* pSound);
MA_API void ma_sound_set_min_distance(ma_sound* pSound, float minDistance);
MA_API float ma_sound_get_min_distance(const ma_sound* pSound);
MA_API void ma_sound_set_max_distance(ma_sound* pSound, float maxDistance);
MA_API float ma_sound_get_max_distance(const ma_sound* pSound);
MA_API void ma_sound_set_cone(ma_sound* pSound, float innerAngleInRadians, float outerAngleInRadians, float outerGain);
MA_API void ma_sound_get_cone(const ma_sound* pSound, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain);
MA_API void ma_sound_set_doppler_factor(ma_sound* pSound, float dopplerFactor);
MA_API float ma_sound_get_doppler_factor(const ma_sound* pSound);
MA_API void ma_sound_set_fade_in_pcm_frames(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames);
MA_API void ma_sound_set_fade_in_milliseconds(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds);
MA_API float ma_sound_get_current_fade_volume(ma_sound* pSound);
MA_API void ma_sound_set_start_time_in_pcm_frames(ma_sound* pSound, ma_uint64 absoluteGlobalTimeInFrames);
MA_API void ma_sound_set_start_time_in_milliseconds(ma_sound* pSound, ma_uint64 absoluteGlobalTimeInMilliseconds);
MA_API void ma_sound_set_stop_time_in_pcm_frames(ma_sound* pSound, ma_uint64 absoluteGlobalTimeInFrames);
MA_API void ma_sound_set_stop_time_in_milliseconds(ma_sound* pSound, ma_uint64 absoluteGlobalTimeInMilliseconds);
MA_API ma_bool32 ma_sound_is_playing(const ma_sound* pSound);
MA_API ma_uint64 ma_sound_get_time_in_pcm_frames(const ma_sound* pSound);
MA_API void ma_sound_set_looping(ma_sound* pSound, ma_bool8 isLooping);
MA_API ma_bool32 ma_sound_is_looping(const ma_sound* pSound);
MA_API ma_bool32 ma_sound_at_end(const ma_sound* pSound);
MA_API ma_result ma_sound_seek_to_pcm_frame(ma_sound* pSound, ma_uint64 frameIndex); /* Just a wrapper around ma_data_source_seek_to_pcm_frame(). */
MA_API ma_result ma_sound_get_data_format(ma_sound* pSound, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_sound_get_cursor_in_pcm_frames(ma_sound* pSound, ma_uint64* pCursor);
MA_API ma_result ma_sound_get_length_in_pcm_frames(ma_sound* pSound, ma_uint64* pLength);

MA_API ma_result ma_sound_group_init(ma_engine* pEngine, ma_uint32 flags, ma_sound_group* pParentGroup, ma_sound_group* pGroup);
MA_API ma_result ma_sound_group_init_ex(ma_engine* pEngine, const ma_sound_group_config* pConfig, ma_sound_group* pGroup);
MA_API void ma_sound_group_uninit(ma_sound_group* pGroup);
MA_API ma_engine* ma_sound_group_get_engine(const ma_sound_group* pGroup);
MA_API ma_result ma_sound_group_start(ma_sound_group* pGroup);
MA_API ma_result ma_sound_group_stop(ma_sound_group* pGroup);
MA_API ma_result ma_sound_group_set_volume(ma_sound_group* pGroup, float volume);
MA_API ma_result ma_sound_group_set_gain_db(ma_sound_group* pGroup, float gainDB);
MA_API void ma_sound_group_set_pan(ma_sound_group* pGroup, float pan);
MA_API void ma_sound_group_set_pan_mode(ma_sound_group* pGroup, ma_pan_mode panMode);
MA_API void ma_sound_group_set_pitch(ma_sound_group* pGroup, float pitch);
MA_API void ma_sound_group_set_spatialization_enabled(ma_sound_group* pGroup, ma_bool32 enabled);
MA_API void ma_sound_group_set_pinned_listener_index(ma_sound_group* pGroup, ma_uint8 listenerIndex);
MA_API ma_uint8 ma_sound_group_get_pinned_listener_index(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_position(ma_sound_group* pGroup, float x, float y, float z);
MA_API ma_vec3f ma_sound_group_get_position(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_direction(ma_sound_group* pGroup, float x, float y, float z);
MA_API ma_vec3f ma_sound_group_get_direction(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_velocity(ma_sound_group* pGroup, float x, float y, float z);
MA_API ma_vec3f ma_sound_group_get_velocity(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_attenuation_model(ma_sound_group* pGroup, ma_attenuation_model attenuationModel);
MA_API ma_attenuation_model ma_sound_group_get_attenuation_model(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_positioning(ma_sound_group* pGroup, ma_positioning positioning);
MA_API ma_positioning ma_sound_group_get_positioning(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_rolloff(ma_sound_group* pGroup, float rolloff);
MA_API float ma_sound_group_get_rolloff(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_min_gain(ma_sound_group* pGroup, float minGain);
MA_API float ma_sound_group_get_min_gain(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_max_gain(ma_sound_group* pGroup, float maxGain);
MA_API float ma_sound_group_get_max_gain(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_min_distance(ma_sound_group* pGroup, float minDistance);
MA_API float ma_sound_group_get_min_distance(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_max_distance(ma_sound_group* pGroup, float maxDistance);
MA_API float ma_sound_group_get_max_distance(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_cone(ma_sound_group* pGroup, float innerAngleInRadians, float outerAngleInRadians, float outerGain);
MA_API void ma_sound_group_get_cone(const ma_sound_group* pGroup, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain);
MA_API void ma_sound_group_set_doppler_factor(ma_sound_group* pGroup, float dopplerFactor);
MA_API float ma_sound_group_get_doppler_factor(const ma_sound_group* pGroup);
MA_API void ma_sound_group_set_fade_in_pcm_frames(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames);
MA_API void ma_sound_group_set_fade_in_milliseconds(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds);
MA_API float ma_sound_group_get_current_fade_volume(ma_sound_group* pGroup);
MA_API void ma_sound_group_set_start_time_in_pcm_frames(ma_sound_group* pGroup, ma_uint64 absoluteGlobalTimeInFrames);
MA_API void ma_sound_group_set_start_time_in_milliseconds(ma_sound_group* pGroup, ma_uint64 absoluteGlobalTimeInMilliseconds);
MA_API void ma_sound_group_set_stop_time_in_pcm_frames(ma_sound_group* pGroup, ma_uint64 absoluteGlobalTimeInFrames);
MA_API void ma_sound_group_set_stop_time_in_milliseconds(ma_sound_group* pGroup, ma_uint64 absoluteGlobalTimeInMilliseconds);
MA_API ma_bool32 ma_sound_group_is_playing(const ma_sound_group* pGroup);
MA_API ma_uint64 ma_sound_group_get_time_in_pcm_frames(const ma_sound_group* pGroup);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_engine_h */


#if defined(MA_IMPLEMENTATION) || defined(MINIAUDIO_IMPLEMENTATION)

MA_API ma_gainer_config ma_gainer_config_init(ma_uint32 channels, ma_uint32 smoothTimeInFrames)
{
    ma_gainer_config config;

    MA_ZERO_OBJECT(&config);
    config.channels           = channels;
    config.smoothTimeInFrames = smoothTimeInFrames;

    return config;
}


typedef struct
{
    size_t sizeInBytes;
    size_t oldGainsOffset;
    size_t newGainsOffset;
} ma_gainer_heap_layout;

static ma_result ma_gainer_get_heap_layout(const ma_gainer_config* pConfig, ma_gainer_heap_layout* pHeapLayout)
{
    MA_ASSERT(pHeapLayout != NULL);

    MA_ZERO_OBJECT(pHeapLayout);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->channels == 0) {
        return MA_INVALID_ARGS;
    }

    pHeapLayout->sizeInBytes = 0;

    /* Old gains. */
    pHeapLayout->oldGainsOffset = pHeapLayout->sizeInBytes;
    pHeapLayout->sizeInBytes += sizeof(float) * pConfig->channels;

    /* New gains. */
    pHeapLayout->newGainsOffset = pHeapLayout->sizeInBytes;
    pHeapLayout->sizeInBytes += sizeof(float) * pConfig->channels;

    /* Alignment. */
    pHeapLayout->sizeInBytes = ma_align_64(pHeapLayout->sizeInBytes);

    return MA_SUCCESS;
}


MA_API ma_result ma_gainer_get_heap_size(const ma_gainer_config* pConfig, size_t* pHeapSizeInBytes)
{
    ma_result result;
    ma_gainer_heap_layout heapLayout;

    if (pHeapSizeInBytes == NULL) {
        return MA_INVALID_ARGS;
    }

    *pHeapSizeInBytes = 0;

    result = ma_gainer_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return MA_INVALID_ARGS;
    }

    *pHeapSizeInBytes = heapLayout.sizeInBytes;

    return MA_SUCCESS;
}


MA_API ma_result ma_gainer_init_preallocated(const ma_gainer_config* pConfig, void* pHeap, ma_gainer* pGainer)
{
    ma_result result;
    ma_gainer_heap_layout heapLayout;
    ma_uint32 iChannel;

    if (pGainer == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pGainer);

    if (pConfig == NULL || pHeap == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_gainer_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    pGainer->_pHeap    = pHeap;
    pGainer->pOldGains = (float*)ma_offset_ptr(pHeap, heapLayout.oldGainsOffset);
    pGainer->pNewGains = (float*)ma_offset_ptr(pHeap, heapLayout.newGainsOffset);

    pGainer->config = *pConfig;
    pGainer->t      = (ma_uint32)-1;  /* No interpolation by default. */

    for (iChannel = 0; iChannel < pConfig->channels; iChannel += 1) {
        pGainer->pOldGains[iChannel] = 1;
        pGainer->pNewGains[iChannel] = 1;
    }
    
    return MA_SUCCESS;
}

MA_API ma_result ma_gainer_init(const ma_gainer_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_gainer* pGainer)
{
    ma_result result;
    size_t heapSizeInBytes;
    void* pHeap;

    result = ma_gainer_get_heap_size(pConfig, &heapSizeInBytes);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to retrieve the size of the heap allocation. */
    }

    if (heapSizeInBytes > 0) {
        pHeap = ma_malloc(heapSizeInBytes, pAllocationCallbacks);
        if (pHeap == NULL) {
            return MA_OUT_OF_MEMORY;
        }
    } else {
        pHeap = NULL;
    }

    result = ma_gainer_init_preallocated(pConfig, pHeap, pGainer);
    if (result != MA_SUCCESS) {
        ma_free(pHeap, pAllocationCallbacks);
        return result;
    }

    pGainer->_ownsHeap = MA_TRUE;
    return MA_SUCCESS;
}

MA_API void ma_gainer_uninit(ma_gainer* pGainer, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pGainer == NULL) {
        return;
    }

    if (pGainer->_ownsHeap) {
        ma_free(pGainer->_pHeap, pAllocationCallbacks);
    }
}

static float ma_gainer_calculate_current_gain(const ma_gainer* pGainer, ma_uint32 channel)
{
    float a = (float)pGainer->t / pGainer->config.smoothTimeInFrames;
    return ma_mix_f32_fast(pGainer->pOldGains[channel], pGainer->pNewGains[channel], a);
}

MA_API ma_result ma_gainer_process_pcm_frames(ma_gainer* pGainer, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_uint64 iFrame;
    ma_uint32 iChannel;
    float* pFramesOutF32 = (float*)pFramesOut;
    const float* pFramesInF32 = (const float*)pFramesIn;

    if (pGainer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pGainer->t >= pGainer->config.smoothTimeInFrames) {
        /* Fast path. No gain calculation required. */
        ma_copy_and_apply_volume_factor_per_channel_f32(pFramesOutF32, pFramesInF32, frameCount, pGainer->config.channels, pGainer->pNewGains);

        /* Now that some frames have been processed we need to make sure future changes to the gain are interpolated. */
        if (pGainer->t == (ma_uint32)-1) {
            pGainer->t = pGainer->config.smoothTimeInFrames;
        }
    } else {
        /* Slow path. Need to interpolate the gain for each channel individually. */

        /* We can allow the input and output buffers to be null in which case we'll just update the internal timer. */
        if (pFramesOut != NULL && pFramesIn != NULL) {
            float a = (float)pGainer->t / pGainer->config.smoothTimeInFrames;
            float d = 1.0f / pGainer->config.smoothTimeInFrames;
            ma_uint32 channelCount = pGainer->config.channels;

            for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
                for (iChannel = 0; iChannel < channelCount; iChannel += 1) {
                    pFramesOutF32[iChannel] = pFramesInF32[iChannel] * ma_mix_f32_fast(pGainer->pOldGains[iChannel], pGainer->pNewGains[iChannel], a);
                }

                pFramesOutF32 += channelCount;
                pFramesInF32  += channelCount;

                a += d;
                if (a > 1) {
                    a = 1;
                }
            }   
        }

        pGainer->t = (ma_uint32)ma_min(pGainer->t + frameCount, pGainer->config.smoothTimeInFrames);

    #if 0   /* Reference implementation. */
        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            /* We can allow the input and output buffers to be null in which case we'll just update the internal timer. */
            if (pFramesOut != NULL && pFramesIn != NULL) {
                for (iChannel = 0; iChannel < pGainer->config.channels; iChannel += 1) {
                    pFramesOutF32[iFrame*pGainer->config.channels + iChannel] = pFramesInF32[iFrame*pGainer->config.channels + iChannel] * ma_gainer_calculate_current_gain(pGainer, iChannel);
                }
            }

            /* Move interpolation time forward, but don't go beyond our smoothing time. */
            pGainer->t = ma_min(pGainer->t + 1, pGainer->config.smoothTimeInFrames);
        }
    #endif
    }

    return MA_SUCCESS;
}

static void ma_gainer_set_gain_by_index(ma_gainer* pGainer, float newGain, ma_uint32 iChannel)
{
    pGainer->pOldGains[iChannel] = ma_gainer_calculate_current_gain(pGainer, iChannel);
    pGainer->pNewGains[iChannel] = newGain;
}

static void ma_gainer_reset_smoothing_time(ma_gainer* pGainer)
{
    if (pGainer->t == (ma_uint32)-1) {
        pGainer->t = pGainer->config.smoothTimeInFrames;    /* No smoothing required for initial gains setting. */
    } else {
        pGainer->t = 0;
    }
}

MA_API ma_result ma_gainer_set_gain(ma_gainer* pGainer, float newGain)
{
    ma_uint32 iChannel;

    if (pGainer == NULL) {
        return MA_INVALID_ARGS;
    }

    for (iChannel = 0; iChannel < pGainer->config.channels; iChannel += 1) {
        ma_gainer_set_gain_by_index(pGainer, newGain, iChannel);
    }

    /* The smoothing time needs to be reset to ensure we always interpolate by the configured smoothing time, but only if it's not the first setting. */
    ma_gainer_reset_smoothing_time(pGainer);

    return MA_SUCCESS;
}

MA_API ma_result ma_gainer_set_gains(ma_gainer* pGainer, float* pNewGains)
{
    ma_uint32 iChannel;

    if (pGainer == NULL || pNewGains == NULL) {
        return MA_INVALID_ARGS;
    }

    for (iChannel = 0; iChannel < pGainer->config.channels; iChannel += 1) {
        ma_gainer_set_gain_by_index(pGainer, pNewGains[iChannel], iChannel);
    }

    /* The smoothing time needs to be reset to ensure we always interpolate by the configured smoothing time, but only if it's not the first setting. */
    ma_gainer_reset_smoothing_time(pGainer);
    
    return MA_SUCCESS;
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


MA_API ma_result ma_panner_init(const ma_panner_config* pConfig, ma_panner* pPanner)
{
    if (pPanner == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pPanner);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

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

MA_API void ma_panner_set_mode(ma_panner* pPanner, ma_pan_mode mode)
{
    if (pPanner == NULL) {
        return;
    }

    pPanner->mode = mode;
}

MA_API void ma_panner_set_pan(ma_panner* pPanner, float pan)
{
    if (pPanner == NULL) {
        return;
    }

    pPanner->pan = ma_clamp(pan, -1.0f, 1.0f);
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
            ma_copy_and_apply_volume_and_clip_pcm_frames(pFramesOut, pFramesIn, frameCount, pFader->config.format, pFader->config.channels, pFader->volumeEnd);
        }
    } else {
        /* Slower path. Volumes are different, so may need to do an interpolation. */
        if (pFader->cursorInFrames >= pFader->lengthInFrames) {
            /* Fast path. We've gone past the end of the fade period so just apply the end volume to all samples. */
            ma_copy_and_apply_volume_and_clip_pcm_frames(pFramesOut, pFramesIn, frameCount, pFader->config.format, pFader->config.channels, pFader->volumeEnd);
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

MA_API void ma_fader_get_data_format(const ma_fader* pFader, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate)
{
    if (pFader == NULL) {
        return;
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
}

MA_API void ma_fader_set_fade(ma_fader* pFader, float volumeBeg, float volumeEnd, ma_uint64 lengthInFrames)
{
    if (pFader == NULL) {
        return;
    }

    /* If the volume is negative, use current volume. */
    if (volumeBeg < 0) {
        volumeBeg = ma_fader_get_current_volume(pFader);
    }

    pFader->volumeBeg      = volumeBeg;
    pFader->volumeEnd      = volumeEnd;
    pFader->lengthInFrames = lengthInFrames;
    pFader->cursorInFrames = 0; /* Reset cursor. */
}

MA_API float ma_fader_get_current_volume(ma_fader* pFader)
{
    if (pFader == NULL) {
        return 0.0f;
    }

    /* The current volume depends on the position of the cursor. */
    if (pFader->cursorInFrames <= 0) {
        return pFader->volumeBeg;
    } else if (pFader->cursorInFrames >= pFader->lengthInFrames) {
        return pFader->volumeEnd;
    } else {
        /* The cursor is somewhere inside the fading period. We can figure this out with a simple linear interpoluation between volumeBeg and volumeEnd based on our cursor position. */
        return ma_mix_f32_fast(pFader->volumeBeg, pFader->volumeEnd, pFader->cursorInFrames / (float)pFader->lengthInFrames);
    }
}





MA_API ma_vec3f ma_vec3f_init_3f(float x, float y, float z)
{
    ma_vec3f v;

    v.x = x;
    v.y = y;
    v.z = z;

    return v;
}

MA_API ma_vec3f ma_vec3f_sub(ma_vec3f a, ma_vec3f b)
{
    return ma_vec3f_init_3f(
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    );
}

MA_API ma_vec3f ma_vec3f_neg(ma_vec3f a)
{
    return ma_vec3f_init_3f(
        -a.x,
        -a.y,
        -a.z
    );
}

MA_API float ma_vec3f_dot(ma_vec3f a, ma_vec3f b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

MA_API float ma_vec3f_len2(ma_vec3f v)
{
    return ma_vec3f_dot(v, v);
}

MA_API float ma_vec3f_len(ma_vec3f v)
{
    return (float)ma_sqrtd(ma_vec3f_len2(v));
}

MA_API float ma_vec3f_dist(ma_vec3f a, ma_vec3f b)
{
    return ma_vec3f_len(ma_vec3f_sub(a, b));
}

MA_API ma_vec3f ma_vec3f_normalize(ma_vec3f v)
{
    float f;
    float l = ma_vec3f_len(v);
    if (l == 0) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    f = 1 / l;
    v.x *= f;
    v.y *= f;
    v.z *= f;

    return v;
}

MA_API ma_vec3f ma_vec3f_cross(ma_vec3f a, ma_vec3f b)
{
    return ma_vec3f_init_3f(
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    );
}




#ifndef MA_DEFAULT_SPEED_OF_SOUND
#define MA_DEFAULT_SPEED_OF_SOUND   343.3f
#endif

/*
These vectors represent the direction that speakers are facing from the center point. They're used
for panning in the spatializer. Must be normalized.
*/
static ma_vec3f g_maChannelDirections[MA_CHANNEL_POSITION_COUNT] = {
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_NONE */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_MONO */
    {-0.7071f,  0.0f,    -0.7071f },  /* MA_CHANNEL_FRONT_LEFT */
    {+0.7071f,  0.0f,    -0.7071f },  /* MA_CHANNEL_FRONT_RIGHT */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_FRONT_CENTER */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_LFE */
    {-0.7071f,  0.0f,    +0.7071f },  /* MA_CHANNEL_BACK_LEFT */
    {+0.7071f,  0.0f,    +0.7071f },  /* MA_CHANNEL_BACK_RIGHT */
    {-0.3162f,  0.0f,    -0.9487f },  /* MA_CHANNEL_FRONT_LEFT_CENTER */
    {+0.3162f,  0.0f,    -0.9487f },  /* MA_CHANNEL_FRONT_RIGHT_CENTER */
    { 0.0f,     0.0f,    +1.0f    },  /* MA_CHANNEL_BACK_CENTER */
    {-1.0f,     0.0f,     0.0f    },  /* MA_CHANNEL_SIDE_LEFT */
    {+1.0f,     0.0f,     0.0f    },  /* MA_CHANNEL_SIDE_RIGHT */
    { 0.0f,    +1.0f,     0.0f    },  /* MA_CHANNEL_TOP_CENTER */
    {-0.5774f, +0.5774f, -0.5774f },  /* MA_CHANNEL_TOP_FRONT_LEFT */
    { 0.0f,    +0.7071f, -0.7071f },  /* MA_CHANNEL_TOP_FRONT_CENTER */
    {+0.5774f, +0.5774f, -0.5774f },  /* MA_CHANNEL_TOP_FRONT_RIGHT */
    {-0.5774f, +0.5774f, +0.5774f },  /* MA_CHANNEL_TOP_BACK_LEFT */
    { 0.0f,    +0.7071f, +0.7071f },  /* MA_CHANNEL_TOP_BACK_CENTER */
    {+0.5774f, +0.5774f, +0.5774f },  /* MA_CHANNEL_TOP_BACK_RIGHT */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_0 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_1 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_2 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_3 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_4 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_5 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_6 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_7 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_8 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_9 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_10 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_11 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_12 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_13 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_14 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_15 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_16 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_17 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_18 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_19 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_20 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_21 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_22 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_23 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_24 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_25 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_26 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_27 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_28 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_29 */
    { 0.0f,     0.0f,    -1.0f    },  /* MA_CHANNEL_AUX_30 */
    { 0.0f,     0.0f,    -1.0f    }   /* MA_CHANNEL_AUX_31 */
};

static ma_vec3f ma_get_channel_direction(ma_channel channel)
{
    if (channel >= MA_CHANNEL_POSITION_COUNT) {
        return ma_vec3f_init_3f(0, 0, -1);
    } else {
        return g_maChannelDirections[channel];
    }
}



static float ma_attenuation_inverse(float distance, float minDistance, float maxDistance, float rolloff)
{
    if (minDistance >= maxDistance) {
        return 1;   /* To avoid division by zero. Do not attenuate. */
    }

    return minDistance / (minDistance + rolloff * (ma_clamp(distance, minDistance, maxDistance) - minDistance));
}

static float ma_attenuation_linear(float distance, float minDistance, float maxDistance, float rolloff)
{
    if (minDistance >= maxDistance) {
        return 1;   /* To avoid division by zero. Do not attenuate. */
    }

    return 1 - rolloff * (ma_clamp(distance, minDistance, maxDistance) - minDistance) / (maxDistance - minDistance);
}

static float ma_attenuation_exponential(float distance, float minDistance, float maxDistance, float rolloff)
{
    if (minDistance >= maxDistance) {
        return 1;   /* To avoid division by zero. Do not attenuate. */
    }

    return (float)ma_powd(ma_clamp(distance, minDistance, maxDistance) / minDistance, -rolloff);
}


/*
Dopper Effect calculation taken from the OpenAL spec, with two main differences:

  1) The source to listener vector will have already been calcualted at an earlier step so we can
     just use that directly. We need only the position of the source relative to the origin.

  2) We don't scale by a frequency because we actually just want the ratio which we'll plug straight
     into the resampler directly.
*/
static float ma_doppler_pitch(ma_vec3f relativePosition, ma_vec3f sourceVelocity, ma_vec3f listenVelocity, float speedOfSound, float dopplerFactor)
{
    float len;
    float vls;
    float vss;

    len = ma_vec3f_len(relativePosition);

    /*
    There's a case where the position of the source will be right on top of the listener in which
    case the length will be 0 and we'll end up with a division by zero. We can just return a ratio
    of 1.0 in this case. This is not considered in the OpenAL spec, but is necessary.
    */
    if (len == 0) {
        return 1.0;
    }

    vls = ma_vec3f_dot(relativePosition, listenVelocity) / len;
    vss = ma_vec3f_dot(relativePosition, sourceVelocity) / len;

    vls = ma_min(vls, speedOfSound / dopplerFactor);
    vss = ma_min(vss, speedOfSound / dopplerFactor);

    return (speedOfSound - dopplerFactor*vls) / (speedOfSound - dopplerFactor*vss);
}


static void ma_get_default_channel_map_for_spatializer(ma_channel* pChannelMap, size_t channelMapCap, ma_uint32 channelCount)
{
    /*
    Special case for stereo. Want to default the left and right speakers to side left and side
    right so that they're facing directly down the X axis rather than slightly forward. Not
    doing this will result in sounds being quieter when behind the listener. This might
    actually be good for some scenerios, but I don't think it's an appropriate default because
    it can be a bit unexpected.
    */
    if (channelCount == 2) {
        pChannelMap[0] = MA_CHANNEL_SIDE_LEFT;
        pChannelMap[1] = MA_CHANNEL_SIDE_RIGHT;
    } else {
        ma_get_standard_channel_map(ma_standard_channel_map_default, pChannelMap, channelMapCap, channelCount);
    }
}


MA_API ma_spatializer_listener_config ma_spatializer_listener_config_init(ma_uint32 channelsOut)
{
    ma_spatializer_listener_config config;

    MA_ZERO_OBJECT(&config);
    config.channelsOut             = channelsOut;
    config.pChannelMapOut          = NULL;
    config.handedness              = ma_handedness_right;
    config.worldUp                 = ma_vec3f_init_3f(0, 1,  0);
    config.coneInnerAngleInRadians = 6.283185f; /* 360 degrees. */
    config.coneOuterAngleInRadians = 6.283185f; /* 360 degrees. */
    config.coneOuterGain           = 0;
    config.speedOfSound            = 343.3f;    /* Same as OpenAL. Used for doppler effect. */

    return config;
}


typedef struct
{
    size_t sizeInBytes;
    size_t channelMapOutOffset;
} ma_spatializer_listener_heap_layout;

static ma_result ma_spatializer_listener_get_heap_layout(const ma_spatializer_listener_config* pConfig, ma_spatializer_listener_heap_layout* pHeapLayout)
{
    MA_ASSERT(pHeapLayout != NULL);

    MA_ZERO_OBJECT(pHeapLayout);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->channelsOut == 0) {
        return MA_INVALID_ARGS;
    }

    pHeapLayout->sizeInBytes = 0;

    /* Channel map. We always need this, even for passthroughs. */
    pHeapLayout->channelMapOutOffset = pHeapLayout->sizeInBytes;
    pHeapLayout->sizeInBytes += ma_align_64(sizeof(*pConfig->pChannelMapOut) * pConfig->channelsOut);

    return MA_SUCCESS;
}


MA_API ma_result ma_spatializer_listener_get_heap_size(const ma_spatializer_listener_config* pConfig, size_t* pHeapSizeInBytes)
{
    ma_result result;
    ma_spatializer_listener_heap_layout heapLayout;

    if (pHeapSizeInBytes == NULL) {
        return MA_INVALID_ARGS;
    }

    *pHeapSizeInBytes = 0;

    result = ma_spatializer_listener_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    *pHeapSizeInBytes = heapLayout.sizeInBytes;

    return MA_SUCCESS;
}

MA_API ma_result ma_spatializer_listener_init_preallocated(const ma_spatializer_listener_config* pConfig, void* pHeap, ma_spatializer_listener* pListener)
{
    ma_result result;
    ma_spatializer_listener_heap_layout heapLayout;

    if (pListener == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pListener);

    result = ma_spatializer_listener_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    pListener->_pHeap    = pHeap;
    pListener->config    = *pConfig;
    pListener->position  = ma_vec3f_init_3f(0, 0,  0);
    pListener->direction = ma_vec3f_init_3f(0, 0, -1);
    pListener->velocity  = ma_vec3f_init_3f(0, 0,  0);

    /* Swap the forward direction if we're left handed (it was initialized based on right handed). */
    if (pListener->config.handedness == ma_handedness_left) {
        pListener->direction = ma_vec3f_neg(pListener->direction);
    }


    /* We must always have a valid channel map. */
    pListener->config.pChannelMapOut = (ma_channel*)ma_offset_ptr(pHeap, heapLayout.channelMapOutOffset);

    /* Use a slightly different default channel map for stereo. */
    if (pConfig->pChannelMapOut == NULL) {
        ma_get_default_channel_map_for_spatializer(pListener->config.pChannelMapOut, pConfig->channelsOut, pConfig->channelsOut);
    } else {
        ma_channel_map_copy_or_default(pListener->config.pChannelMapOut, pConfig->pChannelMapOut, pConfig->channelsOut);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_spatializer_listener_init(const ma_spatializer_listener_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_spatializer_listener* pListener)
{
    ma_result result;
    size_t heapSizeInBytes;
    void* pHeap;

    result = ma_spatializer_listener_get_heap_size(pConfig, &heapSizeInBytes);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (heapSizeInBytes > 0) {
        pHeap = ma_malloc(heapSizeInBytes, pAllocationCallbacks);
        if (pHeap == NULL) {
            return MA_OUT_OF_MEMORY;
        }
    } else {
        pHeap = NULL;
    }

    result = ma_spatializer_listener_init_preallocated(pConfig, pHeap, pListener);
    if (result != MA_SUCCESS) {
        ma_free(pHeap, pAllocationCallbacks);
        return result;
    }

    pListener->_ownsHeap = MA_TRUE;
    return MA_SUCCESS;
}

MA_API void ma_spatializer_listener_uninit(ma_spatializer_listener* pListener, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pListener == NULL) {
        return;
    }

    if (pListener->_pHeap != NULL && pListener->_ownsHeap) {
        ma_free(pListener->_pHeap, pAllocationCallbacks);
    }
}

MA_API ma_channel* ma_spatializer_listener_get_channel_map(ma_spatializer_listener* pListener)
{
    if (pListener == NULL) {
        return NULL;
    }

    return pListener->config.pChannelMapOut;
}

MA_API void ma_spatializer_listener_set_cone(ma_spatializer_listener* pListener, float innerAngleInRadians, float outerAngleInRadians, float outerGain)
{
    if (pListener == NULL) {
        return;
    }

    pListener->config.coneInnerAngleInRadians = innerAngleInRadians;
    pListener->config.coneOuterAngleInRadians = outerAngleInRadians;
    pListener->config.coneOuterGain           = outerGain;
}

MA_API void ma_spatializer_listener_get_cone(const ma_spatializer_listener* pListener, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain)
{
    if (pListener == NULL) {
        return;
    }

    if (pInnerAngleInRadians != NULL) {
        *pInnerAngleInRadians = pListener->config.coneInnerAngleInRadians;
    }

    if (pOuterAngleInRadians != NULL) {
        *pOuterAngleInRadians = pListener->config.coneOuterAngleInRadians;
    }

    if (pOuterGain != NULL) {
        *pOuterGain = pListener->config.coneOuterGain;
    }
}

MA_API void ma_spatializer_listener_set_position(ma_spatializer_listener* pListener, float x, float y, float z)
{
    if (pListener == NULL) {
        return;
    }

    pListener->position = ma_vec3f_init_3f(x, y, z);
}

MA_API ma_vec3f ma_spatializer_listener_get_position(const ma_spatializer_listener* pListener)
{
    if (pListener == NULL) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return pListener->position;
}

MA_API void ma_spatializer_listener_set_direction(ma_spatializer_listener* pListener, float x, float y, float z)
{
    if (pListener == NULL) {
        return;
    }

    pListener->direction = ma_vec3f_init_3f(x, y, z);
}

MA_API ma_vec3f ma_spatializer_listener_get_direction(const ma_spatializer_listener* pListener)
{
    if (pListener == NULL) {
        return ma_vec3f_init_3f(0, 0, -1);
    }

    return pListener->direction;
}

MA_API void ma_spatializer_listener_set_velocity(ma_spatializer_listener* pListener, float x, float y, float z)
{
    if (pListener == NULL) {
        return;
    }

    pListener->velocity = ma_vec3f_init_3f(x, y, z);
}

MA_API ma_vec3f ma_spatializer_listener_get_velocity(const ma_spatializer_listener* pListener)
{
    if (pListener == NULL) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return pListener->velocity;
}

MA_API void ma_spatializer_listener_set_speed_of_sound(ma_spatializer_listener* pListener, float speedOfSound)
{
    if (pListener == NULL) {
        return;
    }

    pListener->config.speedOfSound = speedOfSound;
}

MA_API float ma_spatializer_listener_get_speed_of_sound(const ma_spatializer_listener* pListener)
{
    if (pListener == NULL) {
        return 0;
    }

    return pListener->config.speedOfSound;
}

MA_API void ma_spatializer_listener_set_world_up(ma_spatializer_listener* pListener, float x, float y, float z)
{
    if (pListener == NULL) {
        return;
    }

    pListener->config.worldUp = ma_vec3f_init_3f(x, y, z);
}

MA_API ma_vec3f ma_spatializer_listener_get_world_up(const ma_spatializer_listener* pListener)
{
    if (pListener == NULL) {
        return ma_vec3f_init_3f(0, 1, 0);
    }

    return pListener->config.worldUp;
}




MA_API ma_spatializer_config ma_spatializer_config_init(ma_uint32 channelsIn, ma_uint32 channelsOut)
{
    ma_spatializer_config config;

    MA_ZERO_OBJECT(&config);
    config.channelsIn              = channelsIn;
    config.channelsOut             = channelsOut;
    config.pChannelMapIn           = NULL;
    config.attenuationModel        = ma_attenuation_model_inverse;
    config.positioning             = ma_positioning_absolute;
    config.handedness              = ma_handedness_right;
    config.minGain                 = 0;
    config.maxGain                 = 1;
    config.minDistance             = 1;
    config.maxDistance             = MA_FLT_MAX;
    config.rolloff                 = 1;
    config.coneInnerAngleInRadians = 6.283185f; /* 360 degrees. */
    config.coneOuterAngleInRadians = 6.283185f; /* 360 degress. */
    config.coneOuterGain           = 0.0f;
    config.dopplerFactor           = 1;
    config.gainSmoothTimeInFrames  = 360;       /* 7.5ms @ 48K. */

    return config;
}


static ma_gainer_config ma_spatializer_gainer_config_init(const ma_spatializer_config* pConfig)
{
    MA_ASSERT(pConfig != NULL);
    return ma_gainer_config_init(pConfig->channelsOut, pConfig->gainSmoothTimeInFrames);
}

static ma_result ma_spatializer_validate_config(const ma_spatializer_config* pConfig)
{
    MA_ASSERT(pConfig != NULL);

    if (pConfig->channelsIn == 0 || pConfig->channelsOut == 0) {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

typedef struct
{
    size_t sizeInBytes;
    size_t channelMapInOffset;
    size_t newChannelGainsOffset;
    size_t gainerOffset;
} ma_spatializer_heap_layout;

static ma_result ma_spatializer_get_heap_layout(const ma_spatializer_config* pConfig, ma_spatializer_heap_layout* pHeapLayout)
{
    ma_result result;

    MA_ASSERT(pHeapLayout != NULL);

    MA_ZERO_OBJECT(pHeapLayout);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_spatializer_validate_config(pConfig);
    if (result != MA_SUCCESS) {
        return result;
    }

    pHeapLayout->sizeInBytes = 0;

    /* Channel map. */
    pHeapLayout->channelMapInOffset = MA_SIZE_MAX;  /* <-- MA_SIZE_MAX indicates no allocation necessary. */
    if (pConfig->pChannelMapIn != NULL) {
        pHeapLayout->channelMapInOffset = pHeapLayout->sizeInBytes;
        pHeapLayout->sizeInBytes += ma_align_64(sizeof(*pConfig->pChannelMapIn) * pConfig->channelsIn);
    }

    /* New channel gains for output. */
    pHeapLayout->newChannelGainsOffset = pHeapLayout->sizeInBytes;
    pHeapLayout->sizeInBytes += ma_align_64(sizeof(float) * pConfig->channelsOut);

    /* Gainer. */
    {
        size_t gainerHeapSizeInBytes;
        ma_gainer_config gainerConfig;

        gainerConfig = ma_spatializer_gainer_config_init(pConfig);

        result = ma_gainer_get_heap_size(&gainerConfig, &gainerHeapSizeInBytes);
        if (result != MA_SUCCESS) {
            return result;
        }

        pHeapLayout->gainerOffset = pHeapLayout->sizeInBytes;
        pHeapLayout->sizeInBytes += ma_align_64(gainerHeapSizeInBytes);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_spatializer_get_heap_size(const ma_spatializer_config* pConfig, size_t* pHeapSizeInBytes)
{
    ma_result result;
    ma_spatializer_heap_layout heapLayout;

    if (pHeapSizeInBytes == NULL) {
        return MA_INVALID_ARGS;
    }

    *pHeapSizeInBytes = 0;  /* Safety. */

    result = ma_spatializer_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    *pHeapSizeInBytes = heapLayout.sizeInBytes;

    return MA_SUCCESS;
}


MA_API ma_result ma_spatializer_init_preallocated(const ma_spatializer_config* pConfig, void* pHeap, ma_spatializer* pSpatializer)
{
    ma_result result;
    ma_spatializer_heap_layout heapLayout;
    ma_gainer_config gainerConfig;

    if (pSpatializer == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pSpatializer);

    if (pConfig == NULL || pHeap == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_spatializer_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    pSpatializer->_pHeap       = pHeap;
    pSpatializer->config       = *pConfig;
    pSpatializer->position     = ma_vec3f_init_3f(0, 0,  0);
    pSpatializer->direction    = ma_vec3f_init_3f(0, 0, -1);
    pSpatializer->velocity     = ma_vec3f_init_3f(0, 0,  0);
    pSpatializer->dopplerPitch = 1;

    /* Swap the forward direction if we're left handed (it was initialized based on right handed). */
    if (pSpatializer->config.handedness == ma_handedness_left) {
        pSpatializer->direction = ma_vec3f_neg(pSpatializer->direction);
    }

    /* Channel map. This will be on the heap. */
    if (pConfig->pChannelMapIn != NULL) {
        pSpatializer->config.pChannelMapIn = (ma_channel*)ma_offset_ptr(pHeap, heapLayout.channelMapInOffset);
        ma_channel_map_copy_or_default(pSpatializer->config.pChannelMapIn, pConfig->pChannelMapIn, pSpatializer->config.channelsIn);
    }

    /* New channel gains for output channels. */
    pSpatializer->pNewChannelGainsOut = (float*)ma_offset_ptr(pHeap, heapLayout.newChannelGainsOffset);

    /* Gainer. */
    gainerConfig = ma_spatializer_gainer_config_init(pConfig);

    result = ma_gainer_init_preallocated(&gainerConfig, ma_offset_ptr(pHeap, heapLayout.gainerOffset), &pSpatializer->gainer);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the gainer. */
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_spatializer_init(const ma_spatializer_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_spatializer* pSpatializer)
{
    ma_result result;
    size_t heapSizeInBytes;
    void* pHeap;

    /* We'll need a heap allocation to retrieve the size. */
    result = ma_spatializer_get_heap_size(pConfig, &heapSizeInBytes);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (heapSizeInBytes > 0) {
        pHeap = ma_malloc(heapSizeInBytes, pAllocationCallbacks);
        if (pHeap == NULL) {
            return MA_OUT_OF_MEMORY;
        }
    } else {
        pHeap = NULL;
    }

    result = ma_spatializer_init_preallocated(pConfig, pHeap, pSpatializer);
    if (result != MA_SUCCESS) {
        ma_free(pHeap, pAllocationCallbacks);
        return result;
    }

    pSpatializer->_ownsHeap = MA_TRUE;
    return MA_SUCCESS;
}

MA_API void ma_spatializer_uninit(ma_spatializer* pSpatializer, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pSpatializer == NULL) {
        return;
    }

    ma_gainer_uninit(&pSpatializer->gainer, pAllocationCallbacks);

    if (pSpatializer->_pHeap != NULL && pSpatializer->_ownsHeap) {
        ma_free(pSpatializer->_pHeap, pAllocationCallbacks);
    }
}

static float ma_calculate_angular_gain(ma_vec3f dirA, ma_vec3f dirB, float coneInnerAngleInRadians, float coneOuterAngleInRadians, float coneOuterGain)
{
    /*
    Angular attenuation.

    Unlike distance gain, the math for this is not specified by the OpenAL spec so we'll just go ahead and figure
    this out for ourselves at the expense of possibly being inconsistent with other implementations.

    To do cone attenuation, I'm just using the same math that we'd use to implement a basic spotlight in OpenGL. We
    just need to get the direction from the source to the listener and then do a dot product against that and the
    direction of the spotlight. Then we just compare that dot product against the cosine of the inner and outer
    angles. If the dot product is greater than the the outer angle, we just use coneOuterGain. If it's less than
    the inner angle, we just use a gain of 1. Otherwise we linearly interpolate between 1 and coneOuterGain.
    */
    if (coneInnerAngleInRadians < 6.283185f) {
        float angularGain = 1;
        float cutoffInner = (float)ma_cosd(coneInnerAngleInRadians*0.5f);
        float cutoffOuter = (float)ma_cosd(coneOuterAngleInRadians*0.5f);
        float d;

        d = ma_vec3f_dot(dirA, dirB);

        if (d > cutoffInner) {
            /* It's inside the inner angle. */
            angularGain = 1;
        } else {
            /* It's outside the inner angle. */
            if (d > cutoffOuter) {
                /* It's between the inner and outer angle. We need to linearly interpolate between 1 and coneOuterGain. */
                angularGain = ma_mix_f32(coneOuterGain, 1, (d - cutoffOuter) / (cutoffInner - cutoffOuter));
            } else {
                /* It's outside the outer angle. */
                angularGain = coneOuterGain;
            }
        }

        /*printf("d = %f; cutoffInner = %f; cutoffOuter = %f; angularGain = %f\n", d, cutoffInner, cutoffOuter, angularGain);*/
        return angularGain;
    } else {
        /* Inner angle is 360 degrees so no need to do any attenuation. */
        return 1;
    }
}

MA_API ma_result ma_spatializer_process_pcm_frames(ma_spatializer* pSpatializer, ma_spatializer_listener* pListener, void* pFramesOut, const void* pFramesIn, ma_uint64 frameCount)
{
    ma_channel* pChannelMapIn  = pSpatializer->config.pChannelMapIn;
    ma_channel* pChannelMapOut = pListener->config.pChannelMapOut;

    if (pSpatializer == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If we're not spatializing we need to run an optimized path. */
    if (pSpatializer->config.attenuationModel == ma_attenuation_model_none) {
        /* No attenuation is required, but we'll need to do some channel conversion. */
        if (pSpatializer->config.channelsIn == pSpatializer->config.channelsOut) {
            ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, ma_format_f32, pSpatializer->config.channelsIn);
        } else {
            ma_channel_map_apply_f32((float*)pFramesOut, pChannelMapOut, pSpatializer->config.channelsOut, (const float*)pFramesIn, pChannelMapIn, pSpatializer->config.channelsIn, frameCount, ma_channel_mix_mode_rectangular);   /* Safe casts to float* because f32 is the only supported format. */
        }

        /*
        We're not doing attenuation so don't bother with doppler for now. I'm not sure if this is
        the correct thinking so might need to review this later.
        */
        pSpatializer->dopplerPitch = 1;
    } else {
        /*
        Let's first determine which listener the sound is closest to. Need to keep in mind that we
        might not have a world or any listeners, in which case we just spatializer based on the
        listener being positioned at the origin (0, 0, 0).
        */
        ma_vec3f relativePosNormalized;
        ma_vec3f relativePos;   /* The position relative to the listener. */
        ma_vec3f relativeDir;   /* The direction of the sound, relative to the listener. */
        ma_vec3f listenerVel;   /* The volocity of the listener. For doppler pitch calculation. */
        float speedOfSound;
        float distance = 0;
        float gain = 1;
        ma_uint32 iChannel;
        const ma_uint32 channelsOut = pSpatializer->config.channelsOut;
        const ma_uint32 channelsIn  = pSpatializer->config.channelsIn;

        /*
        We'll need the listener velocity for doppler pitch calculations. The speed of sound is
        defined by the listener, so we'll grab that here too.
        */
        if (pListener != NULL) {
            listenerVel  = pListener->velocity;
            speedOfSound = pListener->config.speedOfSound;
        } else {
            listenerVel  = ma_vec3f_init_3f(0, 0, 0);
            speedOfSound = MA_DEFAULT_SPEED_OF_SOUND;
        }

        if (pListener == NULL || pSpatializer->config.positioning == ma_positioning_relative) {
            /* There's no listener or we're using relative positioning. */
            relativePos = pSpatializer->position;
            relativeDir = pSpatializer->direction;
        } else {
            /*
            We've found a listener and we're using absolute positioning. We need to transform the
            sound's position and direction so that it's relative to listener. Later on we'll use
            this for determining the factors to apply to each channel to apply the panning effect.
            */
            ma_vec3f v;
            ma_vec3f axisX;
            ma_vec3f axisY;
            ma_vec3f axisZ;
            float m[4][4];

            /*
            We need to calcualte the right vector from our forward and up vectors. This is done with
            a cross product.
            */
            axisZ = ma_vec3f_normalize(pListener->direction);                               /* Normalization required here because we can't trust the caller. */
            axisX = ma_vec3f_normalize(ma_vec3f_cross(axisZ, pListener->config.worldUp));   /* Normalization required here because the world up vector may not be perpendicular with the forward vector. */

            /*
            The calculation of axisX above can result in a zero-length vector if the listener is
            looking straight up on the Y axis. We'll need to fall back to a +X in this case so that
            the calculations below don't fall apart. This is where a quaternion based listener and
            sound orientation would come in handy.
            */
            if (ma_vec3f_len2(axisX) == 0) {
                axisX = ma_vec3f_init_3f(1, 0, 0);
            }

            axisY = ma_vec3f_cross(axisX, axisZ);                                           /* No normalization is required here because axisX and axisZ are unit length and perpendicular. */

            /*
            We need to swap the X axis if we're left handed because otherwise the cross product above
            will have resulted in it pointing in the wrong direction (right handed was assumed in the
            cross products above).
            */
            if (pListener->config.handedness == ma_handedness_left) {
                axisX = ma_vec3f_neg(axisX);
            }

            /* Lookat. */
            m[0][0] =  axisX.x; m[1][0] =  axisX.y; m[2][0] =  axisX.z; m[3][0] = -ma_vec3f_dot(axisX,               pListener->position);
            m[0][1] =  axisY.x; m[1][1] =  axisY.y; m[2][1] =  axisY.z; m[3][1] = -ma_vec3f_dot(axisY,               pListener->position);
            m[0][2] = -axisZ.x; m[1][2] = -axisZ.y; m[2][2] = -axisZ.z; m[3][2] = -ma_vec3f_dot(ma_vec3f_neg(axisZ), pListener->position);
            m[0][3] = 0;        m[1][3] = 0;        m[2][3] = 0;        m[3][3] = 1;

            /*
            Multiply the lookat matrix by the spatializer position to transform it to listener
            space. This allows calculations to work based on the sound being relative to the
            origin which makes things simpler.
            */
            v = pSpatializer->position;
            relativePos.x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * 1;
            relativePos.y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * 1;
            relativePos.z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * 1;

            /*
            The direction of the sound needs to also be transformed so that it's relative to the
            rotation of the listener.
            */
            v = pSpatializer->direction;
            relativeDir.x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z;
            relativeDir.y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z;
            relativeDir.z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z;
        }

        distance = ma_vec3f_len(relativePos);

        /* We've gathered the data, so now we can apply some spatialization. */
        switch (pSpatializer->config.attenuationModel) {
            case ma_attenuation_model_inverse:
            {
                gain = ma_attenuation_inverse(distance, pSpatializer->config.minDistance, pSpatializer->config.maxDistance, pSpatializer->config.rolloff);
            } break;
            case ma_attenuation_model_linear:
            {
                gain = ma_attenuation_linear(distance, pSpatializer->config.minDistance, pSpatializer->config.maxDistance, pSpatializer->config.rolloff);
            } break;
            case ma_attenuation_model_exponential:
            {
                gain = ma_attenuation_exponential(distance, pSpatializer->config.minDistance, pSpatializer->config.maxDistance, pSpatializer->config.rolloff);
            } break;
            case ma_attenuation_model_none:
            default:
            {
                gain = 1;
            } break;
        }

        /* Normalize the position. */
        if (distance > 0.001f) {
            float distanceInv = 1/distance;
            relativePosNormalized    = relativePos;
            relativePosNormalized.x *= distanceInv;
            relativePosNormalized.y *= distanceInv;
            relativePosNormalized.z *= distanceInv;
        } else {
            distance = 0;
            relativePosNormalized = ma_vec3f_init_3f(0, 0, 0);
        }

        /*
        Angular attenuation.

        Unlike distance gain, the math for this is not specified by the OpenAL spec so we'll just go ahead and figure
        this out for ourselves at the expense of possibly being inconsistent with other implementations.

        To do cone attenuation, I'm just using the same math that we'd use to implement a basic spotlight in OpenGL. We
        just need to get the direction from the source to the listener and then do a dot product against that and the
        direction of the spotlight. Then we just compare that dot product against the cosine of the inner and outer
        angles. If the dot product is greater than the the outer angle, we just use coneOuterGain. If it's less than
        the inner angle, we just use a gain of 1. Otherwise we linearly interpolate between 1 and coneOuterGain.
        */
        if (distance > 0) {
            /* Source anglular gain. */
            gain *= ma_calculate_angular_gain(relativeDir, ma_vec3f_neg(relativePosNormalized), pSpatializer->config.coneInnerAngleInRadians, pSpatializer->config.coneOuterAngleInRadians, pSpatializer->config.coneOuterGain);

            /*
            We're supporting angular gain on the listener as well for those who want to reduce the volume of sounds that
            are positioned behind the listener. On default settings, this will have no effect.
            */
            if (pListener != NULL && pListener->config.coneInnerAngleInRadians < 6.283185f) {
                ma_vec3f listenerDirection;
                float listenerInnerAngle;
                float listenerOuterAngle;
                float listenerOuterGain;

                if (pListener->config.handedness == ma_handedness_right) {
                    listenerDirection = ma_vec3f_init_3f(0, 0, -1);
                } else {
                    listenerDirection = ma_vec3f_init_3f(0, 0, +1);
                }

                listenerInnerAngle = pListener->config.coneInnerAngleInRadians;
                listenerOuterAngle = pListener->config.coneOuterAngleInRadians;
                listenerOuterGain  = pListener->config.coneOuterGain;

                gain *= ma_calculate_angular_gain(listenerDirection, relativePosNormalized, listenerInnerAngle, listenerOuterAngle, listenerOuterGain);
            }
        } else {
            /* The sound is right on top of the listener. Don't do any angular attenuation. */
        }


        /* Clamp the gain. */
        gain = ma_clamp(gain, pSpatializer->config.minGain, pSpatializer->config.maxGain);

        /*
        Panning. This is where we'll apply the gain and convert to the output channel count. We have an optimized path for
        when we're converting to a mono stream. In that case we don't really need to do any panning - we just apply the
        gain to the final output.
        */
        /*printf("distance=%f; gain=%f\n", distance, gain);*/

        /* We must have a valid channel map here to ensure we spatialize properly. */
        MA_ASSERT(pChannelMapOut != NULL);

        /*
        We're not converting to mono so we'll want to apply some panning. This is where the feeling of something being
        to the left, right, infront or behind the listener is calculated. I'm just using a basic model here. Note that
        the code below is not based on any specific algorithm. I'm just implementing this off the top of my head and
        seeing how it goes. There might be better ways to do this.

        To determine the direction of the sound relative to a speaker I'm using dot products. Each speaker is given a
        direction. For example, the left channel in a stereo system will be -1 on the X axis and the right channel will
        be +1 on the X axis. A dot product is performed against the direction vector of the channel and the normalized
        position of the sound.
        */
        for (iChannel = 0; iChannel < channelsOut; iChannel += 1) {
            pSpatializer->pNewChannelGainsOut[iChannel] = gain;
        }

        /* Convert to our output channel count. */
        ma_channel_map_apply_f32((float*)pFramesOut, pChannelMapOut, channelsOut, (const float*)pFramesIn, pChannelMapIn, channelsIn, frameCount, ma_channel_mix_mode_rectangular);

        /*
        Calculate our per-channel gains. We do this based on the normalized relative position of the sound and it's
        relation to the direction of the channel.
        */
        if (distance > 0) {
            ma_vec3f unitPos = relativePos;
            float distanceInv = 1/distance;
            unitPos.x *= distanceInv;
            unitPos.y *= distanceInv;
            unitPos.z *= distanceInv;

            for (iChannel = 0; iChannel < channelsOut; iChannel += 1) {
                ma_channel channelOut;
                float d;

                channelOut = ma_channel_map_get_channel(pChannelMapOut, channelsOut, iChannel);
                if (ma_is_spatial_channel_position(channelOut)) {
                    d = ma_vec3f_dot(unitPos, ma_get_channel_direction(channelOut));
                } else {
                    d = 1;  /* It's not a spatial channel so there's no real notion of direction. */
                }

                /*
                In my testing, if the panning effect is too aggressive it makes spatialization feel uncomfortable.
                The "dMin" variable below is used to control the aggressiveness of the panning effect. When set to
                0, panning will be most extreme and any sounds that are positioned on the opposite side of the
                speaker will be completely silent from that speaker. Not only does this feel uncomfortable, it
                doesn't even remotely represent the real world at all because sounds that come from your right side
                are still clearly audible from your left side. Setting "dMin" to 1 will  result in no panning at
                all, which is also not ideal. By setting it to something greater than 0, the spatialization effect
                becomes much less dramatic and a lot more bearable.

                Summary: 0 = more extreme panning; 1 = no panning.
                */
                float dMin = 0.2f;  /* TODO: Consider making this configurable. */

                /*
                At this point, "d" will be positive if the sound is on the same side as the channel and negative if
                it's on the opposite side. It will be in the range of -1..1. There's two ways I can think of to
                calculate a panning value. The first is to simply convert it to 0..1, however this has a problem
                which I'm not entirely happy with. Considering a stereo system, when a sound is positioned right
                in front of the listener it'll result in each speaker getting a gain of 0.5. I don't know if I like
                the idea of having a scaling factor of 0.5 being applied to a sound when it's sitting right in front
                of the listener. I would intuitively expect that to be played at full volume, or close to it.

                The second idea I think of is to only apply a reduction in gain when the sound is on the opposite
                side of the speaker. That is, reduce the gain only when the dot product is negative. The problem
                with this is that there will not be any attenuation as the sound sweeps around the 180 degrees
                where the dot product is positive. The idea with this option is that you leave the gain at 1 when
                the sound is being played on the same side as the speaker and then you just reduce the volume when
                the sound is on the other side.

                The summarize, I think the first option should give a better sense of spatialization, but the second
                option is better for preserving the sound's power.

                UPDATE: In my testing, I find the first option to sound better. You can feel the sense of space a
                bit better, but you can also hear the reduction in volume when it's right in front.
                */
                #if 1
                {
                    /*
                    Scale the dot product from -1..1 to 0..1. Will result in a sound directly in front losing power
                    by being played at 0.5 gain.
                    */
                    d = (d + 1) * 0.5f;  /* -1..1 to 0..1 */
                    d = ma_max(d, dMin);
                    pSpatializer->pNewChannelGainsOut[iChannel] *= d;
                }
                #else
                {
                    /*
                    Only reduce the volume of the sound if it's on the opposite side. This path keeps the volume more
                    consistent, but comes at the expense of a worse sense of space and positioning.
                    */
                    if (d < 0) {
                        d += 1; /* Move into the positive range. */
                        d = ma_max(d, dMin);
                        channelGainsOut[iChannel] *= d;
                    }
                }
                #endif
            }
        } else {
            /* Assume the sound is right on top of us. Don't do any panning. */
        }

        /* Now we need to apply the volume to each channel. This needs to run through the gainer to ensure we get a smooth volume transition. */
        ma_gainer_set_gains(&pSpatializer->gainer, pSpatializer->pNewChannelGainsOut);
        ma_gainer_process_pcm_frames(&pSpatializer->gainer, pFramesOut, pFramesOut, frameCount);

        /*
        Before leaving we'll want to update our doppler pitch so that the caller can apply some
        pitch shifting if they desire. Note that we need to negate the relative position here
        because the doppler calculation needs to be source-to-listener, but ours is listener-to-
        source.
        */
        if (pSpatializer->config.dopplerFactor > 0) {
            pSpatializer->dopplerPitch = ma_doppler_pitch(ma_vec3f_sub(pListener->position, pSpatializer->position), pSpatializer->velocity, listenerVel, speedOfSound, pSpatializer->config.dopplerFactor);
        } else {
            pSpatializer->dopplerPitch = 1;
        }
    }

    return MA_SUCCESS;
}

MA_API ma_uint32 ma_spatializer_get_input_channels(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return 0;
    }

    return pSpatializer->config.channelsIn;
}

MA_API ma_uint32 ma_spatializer_get_output_channels(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return 0;
    }

    return pSpatializer->config.channelsOut;
}

MA_API void ma_spatializer_set_attenuation_model(ma_spatializer* pSpatializer, ma_attenuation_model attenuationModel)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.attenuationModel = attenuationModel;
}

MA_API ma_attenuation_model ma_spatializer_get_attenuation_model(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return ma_attenuation_model_none;
    }

    return pSpatializer->config.attenuationModel;
}

MA_API void ma_spatializer_set_positioning(ma_spatializer* pSpatializer, ma_positioning positioning)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.positioning = positioning;
}

MA_API ma_positioning ma_spatializer_get_positioning(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return ma_positioning_absolute;
    }

    return pSpatializer->config.positioning;
}

MA_API void ma_spatializer_set_rolloff(ma_spatializer* pSpatializer, float rolloff)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.rolloff = rolloff;
}

MA_API float ma_spatializer_get_rolloff(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return 0;
    }

    return pSpatializer->config.rolloff;
}

MA_API void ma_spatializer_set_min_gain(ma_spatializer* pSpatializer, float minGain)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.minGain = minGain;
}

MA_API float ma_spatializer_get_min_gain(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return 0;
    }

    return pSpatializer->config.minGain;
}

MA_API void ma_spatializer_set_max_gain(ma_spatializer* pSpatializer, float maxGain)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.maxGain = maxGain;
}

MA_API float ma_spatializer_get_max_gain(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return 0;
    }

    return pSpatializer->config.maxGain;
}

MA_API void ma_spatializer_set_min_distance(ma_spatializer* pSpatializer, float minDistance)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.minDistance = minDistance;
}

MA_API float ma_spatializer_get_min_distance(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return 0;
    }

    return pSpatializer->config.minDistance;
}

MA_API void ma_spatializer_set_max_distance(ma_spatializer* pSpatializer, float maxDistance)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.maxDistance = maxDistance;
}

MA_API float ma_spatializer_get_max_distance(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return 0;
    }

    return pSpatializer->config.maxDistance;
}

MA_API void ma_spatializer_set_cone(ma_spatializer* pSpatializer, float innerAngleInRadians, float outerAngleInRadians, float outerGain)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.coneInnerAngleInRadians = innerAngleInRadians;
    pSpatializer->config.coneOuterAngleInRadians = outerAngleInRadians;
    pSpatializer->config.coneOuterGain           = outerGain;
}

MA_API void ma_spatializer_get_cone(const ma_spatializer* pSpatializer, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain)
{
    if (pSpatializer == NULL) {
        return;
    }

    if (pInnerAngleInRadians != NULL) {
        *pInnerAngleInRadians = pSpatializer->config.coneInnerAngleInRadians;
    }

    if (pOuterAngleInRadians != NULL) {
        *pOuterAngleInRadians = pSpatializer->config.coneOuterAngleInRadians;
    }

    if (pOuterGain != NULL) {
        *pOuterGain = pSpatializer->config.coneOuterGain;
    }
}

MA_API void ma_spatializer_set_doppler_factor(ma_spatializer* pSpatializer, float dopplerFactor)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->config.dopplerFactor = dopplerFactor;
}

MA_API float ma_spatializer_get_doppler_factor(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return 1;
    }

    return pSpatializer->config.dopplerFactor;
}

MA_API void ma_spatializer_set_position(ma_spatializer* pSpatializer, float x, float y, float z)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->position = ma_vec3f_init_3f(x, y, z);
}

MA_API ma_vec3f ma_spatializer_get_position(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return pSpatializer->position;
}

MA_API void ma_spatializer_set_direction(ma_spatializer* pSpatializer, float x, float y, float z)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->direction = ma_vec3f_init_3f(x, y, z);
}

MA_API ma_vec3f ma_spatializer_get_direction(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return ma_vec3f_init_3f(0, 0, -1);
    }

    return pSpatializer->direction;
}

MA_API void ma_spatializer_set_velocity(ma_spatializer* pSpatializer, float x, float y, float z)
{
    if (pSpatializer == NULL) {
        return;
    }

    pSpatializer->velocity = ma_vec3f_init_3f(x, y, z);
}

MA_API ma_vec3f ma_spatializer_get_velocity(const ma_spatializer* pSpatializer)
{
    if (pSpatializer == NULL) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return pSpatializer->velocity;
}




/**************************************************************************************************************************************************************

Engine

**************************************************************************************************************************************************************/
#define MA_SEEK_TARGET_NONE         (~(ma_uint64)0)

MA_API ma_engine_node_config ma_engine_node_config_init(ma_engine* pEngine, ma_engine_node_type type, ma_uint32 flags)
{
    ma_engine_node_config config;

    MA_ZERO_OBJECT(&config);
    config.pEngine                  = pEngine;
    config.type                     = type;
    config.isPitchDisabled          = (flags & MA_SOUND_FLAG_NO_PITCH) != 0;
    config.isSpatializationDisabled = (flags & MA_SOUND_FLAG_NO_SPATIALIZATION) != 0;

    return config;
}


static void ma_engine_node_update_pitch_if_required(ma_engine_node* pEngineNode)
{
    ma_bool32 isUpdateRequired = MA_FALSE;
    float newPitch;

    MA_ASSERT(pEngineNode != NULL);

    newPitch = c89atomic_load_explicit_f32(&pEngineNode->pitch, c89atomic_memory_order_acquire);

    if (pEngineNode->oldPitch != newPitch) {
        pEngineNode->oldPitch  = newPitch;
        isUpdateRequired = MA_TRUE;
    }

    if (pEngineNode->oldDopplerPitch != pEngineNode->spatializer.dopplerPitch) {
        pEngineNode->oldDopplerPitch  = pEngineNode->spatializer.dopplerPitch;
        isUpdateRequired = MA_TRUE;
    }

    if (isUpdateRequired) {
        float basePitch = (float)pEngineNode->sampleRate / ma_engine_get_sample_rate(pEngineNode->pEngine);
        ma_linear_resampler_set_rate_ratio(&pEngineNode->resampler, basePitch * pEngineNode->oldPitch * pEngineNode->oldDopplerPitch);
    }
}

static ma_bool32 ma_engine_node_is_pitching_enabled(const ma_engine_node* pEngineNode)
{
    MA_ASSERT(pEngineNode != NULL);

    /* Don't try to be clever by skiping resampling in the pitch=1 case or else you'll glitch when moving away from 1. */
    return !c89atomic_load_explicit_8(&pEngineNode->isPitchDisabled, c89atomic_memory_order_acquire);
}

static ma_bool32 ma_engine_node_is_spatialization_enabled(const ma_engine_node* pEngineNode)
{
    MA_ASSERT(pEngineNode != NULL);

    return !c89atomic_load_explicit_8(&pEngineNode->isSpatializationDisabled, c89atomic_memory_order_acquire);
}

static ma_uint64 ma_engine_node_get_required_input_frame_count(const ma_engine_node* pEngineNode, ma_uint64 outputFrameCount)
{
    ma_uint64 inputFrameCount = 0;

    if (ma_engine_node_is_pitching_enabled(pEngineNode)) {
        ma_result result = ma_linear_resampler_get_required_input_frame_count(&pEngineNode->resampler, outputFrameCount, &inputFrameCount);
        if (result != MA_SUCCESS) {
            inputFrameCount = 0;
        }
    } else {
        inputFrameCount = outputFrameCount;    /* No resampling, so 1:1. */
    }

    return inputFrameCount;
}

static void ma_engine_node_process_pcm_frames__general(ma_engine_node* pEngineNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_uint32 frameCountIn;
    ma_uint32 frameCountOut;
    ma_uint32 totalFramesProcessedIn;
    ma_uint32 totalFramesProcessedOut;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    ma_bool32 isPitchingEnabled;
    ma_bool32 isFadingEnabled;
    ma_bool32 isSpatializationEnabled;
    ma_bool32 isPanningEnabled;

    frameCountIn  = *pFrameCountIn;
    frameCountOut = *pFrameCountOut;

    channelsIn  = ma_spatializer_get_input_channels(&pEngineNode->spatializer);
    channelsOut = ma_spatializer_get_output_channels(&pEngineNode->spatializer);

    totalFramesProcessedIn  = 0;
    totalFramesProcessedOut = 0;

    isPitchingEnabled       = ma_engine_node_is_pitching_enabled(pEngineNode);
    isFadingEnabled         = pEngineNode->fader.volumeBeg != 1 || pEngineNode->fader.volumeEnd != 1;
    isSpatializationEnabled = ma_engine_node_is_spatialization_enabled(pEngineNode);
    isPanningEnabled        = pEngineNode->panner.pan != 0 && channelsOut != 1;

    /* Keep going while we've still got data available for processing. */
    while (totalFramesProcessedOut < frameCountOut) {
        /*
        We need to process in a specific order. We always do resampling first because it's likely
        we're going to be increasing the channel count after spatialization. Also, I want to do
        fading based on the output sample rate.

        We'll first read into a buffer from the resampler. Then we'll do all processing that
        operates on the on the input channel count. We'll then get the spatializer to output to
        the output buffer and then do all effects from that point directly in the output buffer
        in-place.

        Note that we're always running the resampler. If we try to be clever and skip resampling
        when the pitch is 1, we'll get a glitch when we move away from 1, back to 1, and then
        away from 1 again. We'll want to implement any pitch=1 optimizations in the resampler
        itself.

        There's a small optimization here that we'll utilize since it might be a fairly common
        case. When the input and output channel counts are the same, we'll read straight into the
        output buffer from the resampler and do everything in-place.
        */
        const float* pRunningFramesIn;
        float* pRunningFramesOut;
        float* pWorkingBuffer;   /* This is the buffer that we'll be processing frames in. This is in input channels. */
        float temp[MA_DATA_CONVERTER_STACK_BUFFER_SIZE / sizeof(float)];
        ma_uint32 tempCapInFrames = ma_countof(temp) / channelsIn;
        ma_uint32 framesAvailableIn;
        ma_uint32 framesAvailableOut;
        ma_uint32 framesJustProcessedIn;
        ma_uint32 framesJustProcessedOut;
        ma_bool32 isWorkingBufferValid = MA_FALSE;

        framesAvailableIn  = frameCountIn  - totalFramesProcessedIn;
        framesAvailableOut = frameCountOut - totalFramesProcessedOut;

        pRunningFramesIn  = ma_offset_pcm_frames_const_ptr_f32(ppFramesIn[0], totalFramesProcessedIn, channelsIn);
        pRunningFramesOut = ma_offset_pcm_frames_ptr_f32(ppFramesOut[0], totalFramesProcessedOut, channelsOut);

        if (channelsIn == channelsOut) {
            /* Fast path. Channel counts are the same. No need for an intermediary input buffer. */
            pWorkingBuffer = pRunningFramesOut;
        } else {
            /* Slow path. Channel counts are different. Need to use an intermediary input buffer. */
            pWorkingBuffer = temp;
            if (framesAvailableOut > tempCapInFrames) {
                framesAvailableOut = tempCapInFrames;
            }
        }

        /* First is resampler. */
        if (isPitchingEnabled) {
            ma_uint64 resampleFrameCountIn  = framesAvailableIn;
            ma_uint64 resampleFrameCountOut = framesAvailableOut;

            ma_linear_resampler_process_pcm_frames(&pEngineNode->resampler, pRunningFramesIn, &resampleFrameCountIn, pWorkingBuffer, &resampleFrameCountOut);
            isWorkingBufferValid = MA_TRUE;

            framesJustProcessedIn  = (ma_uint32)resampleFrameCountIn;
            framesJustProcessedOut = (ma_uint32)resampleFrameCountOut;
        } else {
            framesJustProcessedIn  = framesAvailableIn;
            framesJustProcessedOut = framesAvailableOut;
        }

        /* Fading. */
        if (isFadingEnabled) {
            if (isWorkingBufferValid) {
                ma_fader_process_pcm_frames(&pEngineNode->fader, pWorkingBuffer, pWorkingBuffer, framesJustProcessedOut);   /* In-place processing. */
            } else {
                ma_fader_process_pcm_frames(&pEngineNode->fader, pWorkingBuffer, pRunningFramesIn, framesJustProcessedOut);
                isWorkingBufferValid = MA_TRUE;
            }
        }

        /*
        If at this point we still haven't actually done anything with the working buffer we need
        to just read straight from the input buffer.
        */
        if (isWorkingBufferValid == MA_FALSE) {
            pWorkingBuffer = (float*)pRunningFramesIn;  /* Naughty const cast, but it's safe at this point because we won't ever be writing to it from this point out. */
        }

        /* Spatialization. */
        if (isSpatializationEnabled) {
            ma_uint32 iListener;

            /*
            When determining the listener to use, we first check to see if the sound is pinned to a
            specific listener. If so, we use that. Otherwise we just use the closest listener.
            */
            if (pEngineNode->pinnedListenerIndex != MA_LISTENER_INDEX_CLOSEST && pEngineNode->pinnedListenerIndex < ma_engine_get_listener_count(pEngineNode->pEngine)) {
                iListener = pEngineNode->pinnedListenerIndex;
            } else {
                iListener = ma_engine_find_closest_listener(pEngineNode->pEngine, pEngineNode->spatializer.position.x, pEngineNode->spatializer.position.y, pEngineNode->spatializer.position.z);
            }

            ma_spatializer_process_pcm_frames(&pEngineNode->spatializer, &pEngineNode->pEngine->listeners[iListener], pRunningFramesOut, pWorkingBuffer, framesJustProcessedOut);
        } else {
            /* No spatialization, but we still need to do channel conversion. */
            if (channelsIn == channelsOut) {
                /* No channel conversion required. Just copy straight to the output buffer. */
                ma_copy_pcm_frames(pRunningFramesOut, pWorkingBuffer, framesJustProcessedOut, ma_format_f32, channelsOut);
            } else {
                /* Channel conversion required. TODO: Add support for channel maps here. */
                ma_channel_map_apply_f32(pRunningFramesOut, NULL, channelsOut, pWorkingBuffer, NULL, channelsIn, framesJustProcessedOut, ma_channel_mix_mode_simple);
            }
        }

        /* At this point we can guarantee that the output buffer contains valid data. We can process everything in place now. */

        /* Panning. */
        if (isPanningEnabled) {
            ma_panner_process_pcm_frames(&pEngineNode->panner, pRunningFramesOut, pRunningFramesOut, framesJustProcessedOut);   /* In-place processing. */
        }

        /* We're done for this chunk. */
        totalFramesProcessedIn  += framesJustProcessedIn;
        totalFramesProcessedOut += framesJustProcessedOut;

        /* If we didn't process any output frames this iteration it means we've either run out of input data, or run out of room in the output buffer. */
        if (framesJustProcessedOut == 0) {
            break;
        }
    }

    /* At this point we're done processing. */
    *pFrameCountIn  = totalFramesProcessedIn;
    *pFrameCountOut = totalFramesProcessedOut;
}

static void ma_engine_node_process_pcm_frames__sound(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    /* For sounds, we need to first read from the data source. Then we need to apply the engine effects (pan, pitch, fades, etc.). */
    ma_result result = MA_SUCCESS;
    ma_sound* pSound = (ma_sound*)pNode;
    ma_uint32 frameCount = *pFrameCountOut;
    ma_uint32 totalFramesRead = 0;
    ma_format dataSourceFormat;
    ma_uint32 dataSourceChannels;
    ma_uint8 temp[MA_DATA_CONVERTER_STACK_BUFFER_SIZE];
    ma_uint32 tempCapInFrames;

    /* This is a data source node which means no input buses. */
    (void)ppFramesIn;
    (void)pFrameCountIn;

    /* If we're marked at the end we need to stop the sound and do nothing. */
    if (ma_sound_at_end(pSound)) {
        ma_sound_stop(pSound);
        *pFrameCountOut = 0;
        return;
    }

    /* If we're seeking, do so now before reading. */
    if (pSound->seekTarget != MA_SEEK_TARGET_NONE) {
        ma_data_source_seek_to_pcm_frame(pSound->pDataSource, pSound->seekTarget);
                
        /* Any time-dependant effects need to have their times updated. */
        ma_node_set_time(pSound, pSound->seekTarget);

        pSound->seekTarget  = MA_SEEK_TARGET_NONE;
    }

    /*
    We want to update the pitch once. For sounds, this can be either at the start or at the end. If
    we don't force this to only ever be updating once, we could end up in a situation where
    retrieving the required input frame count ends up being different to what we actually retrieve.
    What could happen is that the required input frame count is calculated, the pitch is update,
    and then this processing function is called resulting in a different number of input frames
    being processed. Do not call this in ma_engine_node_process_pcm_frames__general() or else
    you'll hit the aforementioned bug.
    */
    ma_engine_node_update_pitch_if_required(&pSound->engineNode);

    /*
    For the convenience of the caller, we're doing to allow data sources to use non-floating-point formats and channel counts that differ
    from the main engine.
    */
    result = ma_data_source_get_data_format(pSound->pDataSource, &dataSourceFormat, &dataSourceChannels, NULL, NULL, 0);
    if (result == MA_SUCCESS) {
        tempCapInFrames = sizeof(temp) / ma_get_bytes_per_frame(dataSourceFormat, dataSourceChannels);

        /* Keep reading until we've read as much as was requested or we reach the end of the data source. */
        while (totalFramesRead < frameCount) {
            ma_uint32 framesRemaining = frameCount - totalFramesRead;
            ma_uint32 framesToRead;
            ma_uint64 framesJustRead;
            ma_uint32 frameCountIn;
            ma_uint32 frameCountOut;
            const float* pRunningFramesIn;
            float* pRunningFramesOut;

            /*
            The first thing we need to do is read into the temporary buffer. We can calculate exactly
            how many input frames we'll need after resampling.
            */
            framesToRead = (ma_uint32)ma_engine_node_get_required_input_frame_count(&pSound->engineNode, framesRemaining);
            if (framesToRead > tempCapInFrames) {
                framesToRead = tempCapInFrames;
            }

            result = ma_data_source_read_pcm_frames(pSound->pDataSource, temp, framesToRead, &framesJustRead, ma_sound_is_looping(pSound));

            /* If we reached the end of the sound we'll want to mark it as at the end and stop it. This should never be returned for looping sounds. */
            if (result == MA_AT_END) {
                c89atomic_exchange_8(&pSound->atEnd, MA_TRUE); /* This will be set to false in ma_sound_start(). */
            }

            pRunningFramesOut = ma_offset_pcm_frames_ptr_f32(ppFramesOut[0], totalFramesRead, ma_engine_get_channels(ma_sound_get_engine(pSound)));

            frameCountIn = (ma_uint32)framesJustRead;
            frameCountOut = framesRemaining;

            /* Convert if necessary. */
            if (dataSourceFormat == ma_format_f32) {
                /* Fast path. No data conversion necessary. */
                pRunningFramesIn = (float*)temp;
                ma_engine_node_process_pcm_frames__general(&pSound->engineNode, &pRunningFramesIn, &frameCountIn, &pRunningFramesOut, &frameCountOut);
            } else {
                /* Slow path. Need to do sample format conversion to f32. If we give the f32 buffer the same count as the first temp buffer, we're guaranteed it'll be large enough. */
                float tempf32[MA_DATA_CONVERTER_STACK_BUFFER_SIZE]; /* Do not do `MA_DATA_CONVERTER_STACK_BUFFER_SIZE/sizeof(float)` here like we've done in other places. */
                ma_convert_pcm_frames_format(tempf32, ma_format_f32, temp, dataSourceFormat, framesJustRead, dataSourceChannels, ma_dither_mode_none);

                /* Now that we have our samples in f32 format we can process like normal. */
                pRunningFramesIn = tempf32;
                ma_engine_node_process_pcm_frames__general(&pSound->engineNode, &pRunningFramesIn, &frameCountIn, &pRunningFramesOut, &frameCountOut);
            }

            /* We should have processed all of our input frames since we calculated the required number of input frames at the top. */
            MA_ASSERT(frameCountIn == framesJustRead);
            totalFramesRead += (ma_uint32)frameCountOut;   /* Safe cast. */

            if (result != MA_SUCCESS || ma_sound_at_end(pSound)) {
                break;  /* Might have reached the end. */
            }
        }
    }

    *pFrameCountOut = totalFramesRead;
}

static void ma_engine_node_process_pcm_frames__group(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    /*
    Make sure the pitch is updated before trying to read anything. It's important that this is done
    only once and not in ma_engine_node_process_pcm_frames__general(). The reason for this is that
    ma_engine_node_process_pcm_frames__general() will call ma_engine_node_get_required_input_frame_count(), 
    and if another thread modifies the pitch just after that call it can result in a glitch due to
    the input rate changing.
    */
    ma_engine_node_update_pitch_if_required((ma_engine_node*)pNode);

    /* For groups, the input data has already been read and we just need to apply the effect. */
    ma_engine_node_process_pcm_frames__general((ma_engine_node*)pNode, ppFramesIn, pFrameCountIn, ppFramesOut, pFrameCountOut);
}

static ma_result ma_engine_node_get_required_input_frame_count__group(ma_node* pNode, ma_uint32 outputFrameCount, ma_uint32* pInputFrameCount)
{
    ma_uint64 inputFrameCount;

    MA_ASSERT(pInputFrameCount != NULL);

    /* Our pitch will affect this calculation. We need to update it. */
    ma_engine_node_update_pitch_if_required((ma_engine_node*)pNode);

    inputFrameCount = ma_engine_node_get_required_input_frame_count((ma_engine_node*)pNode, outputFrameCount);
    if (inputFrameCount > 0xFFFFFFFF) {
        inputFrameCount = 0xFFFFFFFF;    /* Will never happen because miniaudio will only ever process in relatively small chunks. */
    }

    *pInputFrameCount = (ma_uint32)inputFrameCount;

    return MA_SUCCESS;
}


static ma_node_vtable g_ma_engine_node_vtable__sound =
{
    ma_engine_node_process_pcm_frames__sound,
    NULL,   /* onGetRequiredInputFrameCount */
    0,      /* Sounds are data source nodes which means they have zero inputs (their input is drawn from the data source itself). */
    1,      /* Sounds have one output bus. */
    0       /* Default flags. */
};

static ma_node_vtable g_ma_engine_node_vtable__group =
{
    ma_engine_node_process_pcm_frames__group,
    ma_engine_node_get_required_input_frame_count__group,
    1,      /* Groups have one input bus. */
    1,      /* Groups have one output bus. */
    MA_NODE_FLAG_DIFFERENT_PROCESSING_RATES /* The engine node does resampling so should let miniaudio know about it. */
};



static ma_node_config ma_engine_node_base_node_config_init(const ma_engine_node_config* pConfig)
{
    ma_node_config baseNodeConfig;

    if (pConfig->type == ma_engine_node_type_sound) {
        /* Sound. */
        baseNodeConfig = ma_node_config_init();
        baseNodeConfig.vtable       = &g_ma_engine_node_vtable__sound;
        baseNodeConfig.initialState = ma_node_state_stopped;    /* Sounds are stopped by default. */
    } else {
        /* Group. */
        baseNodeConfig = ma_node_config_init();
        baseNodeConfig.vtable       = &g_ma_engine_node_vtable__group;
        baseNodeConfig.initialState = ma_node_state_started;    /* Groups are started by default. */
    }

    return baseNodeConfig;
}

static ma_spatializer_config ma_engine_node_spatializer_config_init(const ma_node_config* pBaseNodeConfig)
{
    return ma_spatializer_config_init(pBaseNodeConfig->pInputChannels[0], pBaseNodeConfig->pOutputChannels[0]);
}

typedef struct
{
    size_t sizeInBytes;
    size_t baseNodeOffset;
    size_t spatializerOffset;
} ma_engine_node_heap_layout;

static ma_result ma_engine_node_get_heap_layout(const ma_engine_node_config* pConfig, ma_engine_node_heap_layout* pHeapLayout)
{
    ma_result result;
    size_t tempHeapSize;
    ma_node_config baseNodeConfig;
    ma_spatializer_config spatializerConfig;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;

    MA_ASSERT(pHeapLayout);

    MA_ZERO_OBJECT(pHeapLayout);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->pEngine == NULL) {
        return MA_INVALID_ARGS; /* An engine must be specified. */
    }

    pHeapLayout->sizeInBytes = 0;

    channelsIn  = (pConfig->channelsIn  != 0) ? pConfig->channelsIn  : ma_engine_get_channels(pConfig->pEngine);
    channelsOut = (pConfig->channelsOut != 0) ? pConfig->channelsOut : ma_engine_get_channels(pConfig->pEngine);


    /* Base node. */
    baseNodeConfig = ma_engine_node_base_node_config_init(pConfig);
    baseNodeConfig.pInputChannels  = &channelsIn;
    baseNodeConfig.pOutputChannels = &channelsOut;

    result = ma_node_get_heap_size(&baseNodeConfig, &tempHeapSize);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to retrieve the size of the heap for the base node. */
    }

    pHeapLayout->baseNodeOffset = pHeapLayout->sizeInBytes;
    pHeapLayout->sizeInBytes += ma_align_64(tempHeapSize);


    /* Spatializer. */
    spatializerConfig = ma_engine_node_spatializer_config_init(&baseNodeConfig);

    result = ma_spatializer_get_heap_size(&spatializerConfig, &tempHeapSize);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to retrieve the size of the heap for the spatializer. */
    }

    pHeapLayout->spatializerOffset = pHeapLayout->sizeInBytes;
    pHeapLayout->sizeInBytes += ma_align_64(tempHeapSize);


    return MA_SUCCESS;
}

MA_API ma_result ma_engine_node_get_heap_size(const ma_engine_node_config* pConfig, size_t* pHeapSizeInBytes)
{
    ma_result result;
    ma_engine_node_heap_layout heapLayout;

    if (pHeapSizeInBytes == NULL) {
        return MA_INVALID_ARGS;
    }

    *pHeapSizeInBytes = 0;

    result = ma_engine_node_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    *pHeapSizeInBytes = heapLayout.sizeInBytes;

    return MA_SUCCESS;
}

MA_API ma_result ma_engine_node_init_preallocated(const ma_engine_node_config* pConfig, void* pHeap, ma_engine_node* pEngineNode)
{
    ma_result result;
    ma_engine_node_heap_layout heapLayout;
    ma_node_config baseNodeConfig;
    ma_linear_resampler_config resamplerConfig;
    ma_fader_config faderConfig;
    ma_spatializer_config spatializerConfig;
    ma_panner_config pannerConfig;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;

    if (pEngineNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pEngineNode);

    result = ma_engine_node_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (pConfig->pinnedListenerIndex != MA_LISTENER_INDEX_CLOSEST && pConfig->pinnedListenerIndex >= ma_engine_get_listener_count(pConfig->pEngine)) {
        return MA_INVALID_ARGS; /* Invalid listener. */
    }

    pEngineNode->_pHeap                   = pHeap;
    pEngineNode->pEngine                  = pConfig->pEngine;
    pEngineNode->sampleRate               = (pConfig->sampleRate > 0) ? pConfig->sampleRate : ma_engine_get_sample_rate(pEngineNode->pEngine);
    pEngineNode->pitch                    = 1;
    pEngineNode->oldPitch                 = 1;
    pEngineNode->oldDopplerPitch          = 1;
    pEngineNode->isPitchDisabled          = pConfig->isPitchDisabled;
    pEngineNode->isSpatializationDisabled = pConfig->isSpatializationDisabled;
    pEngineNode->pinnedListenerIndex      = pConfig->pinnedListenerIndex;


    channelsIn  = (pConfig->channelsIn  != 0) ? pConfig->channelsIn  : ma_engine_get_channels(pConfig->pEngine);
    channelsOut = (pConfig->channelsOut != 0) ? pConfig->channelsOut : ma_engine_get_channels(pConfig->pEngine);


    /* Base node. */
    baseNodeConfig = ma_engine_node_base_node_config_init(pConfig);
    baseNodeConfig.pInputChannels  = &channelsIn;
    baseNodeConfig.pOutputChannels = &channelsOut;

    result = ma_node_init_preallocated(&pConfig->pEngine->nodeGraph, &baseNodeConfig, ma_offset_ptr(pHeap, heapLayout.baseNodeOffset), &pEngineNode->baseNode);
    if (result != MA_SUCCESS) {
        goto error0;
    }
    

    /*
    We can now initialize the effects we need in order to implement the engine node. There's a
    defined order of operations here, mainly centered around when we convert our channels from the
    data source's native channel count to the engine's channel count. As a rule, we want to do as
    much computation as possible before spatialization because there's a chance that will increase
    the channel count, thereby increasing the amount of work needing to be done to process.
    */

    /* We'll always do resampling first. */
    resamplerConfig = ma_linear_resampler_config_init(ma_format_f32, baseNodeConfig.pInputChannels[0], pEngineNode->sampleRate, ma_engine_get_sample_rate(pEngineNode->pEngine));
    resamplerConfig.lpfOrder = 0;    /* <-- Need to disable low-pass filtering for pitch shifting for now because there's cases where the biquads are becoming unstable. Need to figure out a better fix for this. */

    result = ma_linear_resampler_init(&resamplerConfig, &pEngineNode->pEngine->allocationCallbacks, &pEngineNode->resampler);  /* TODO: Use pre-allocation here. */
    if (result != MA_SUCCESS) {
        goto error1;
    }


    /* After resampling will come the fader. */
    faderConfig = ma_fader_config_init(ma_format_f32, baseNodeConfig.pInputChannels[0], ma_engine_get_sample_rate(pEngineNode->pEngine));

    result = ma_fader_init(&faderConfig, &pEngineNode->fader);
    if (result != MA_SUCCESS) {
        goto error2;
    }


    /*
    Spatialization comes next. We spatialize based ont he node's output channel count. It's up the caller to
    ensure channels counts link up correctly in the node graph.
    */
    spatializerConfig = ma_engine_node_spatializer_config_init(&baseNodeConfig);
    spatializerConfig.gainSmoothTimeInFrames = pEngineNode->pEngine->gainSmoothTimeInFrames;
    
    result = ma_spatializer_init_preallocated(&spatializerConfig, ma_offset_ptr(pHeap, heapLayout.spatializerOffset), &pEngineNode->spatializer);
    if (result != MA_SUCCESS) {
        goto error2;
    }


    /*
    After spatialization comes panning. We need to do this after spatialization because otherwise we wouldn't
    be able to pan mono sounds.
    */
    pannerConfig = ma_panner_config_init(ma_format_f32, baseNodeConfig.pOutputChannels[0]);

    result = ma_panner_init(&pannerConfig, &pEngineNode->panner);
    if (result != MA_SUCCESS) {
        goto error3;
    }

    return MA_SUCCESS;

error3: ma_spatializer_uninit(&pEngineNode->spatializer, NULL); /* <-- No need for allocation callbacks here because we use a preallocated heap. */
error2: ma_linear_resampler_uninit(&pEngineNode->resampler, &pConfig->pEngine->allocationCallbacks);   /* TODO: Remove this when we have support for preallocated heaps with resamplers. */
error1: ma_node_uninit(&pEngineNode->baseNode, NULL);           /* <-- No need for allocation callbacks here because we use a preallocated heap. */
error0: return result;
}

MA_API ma_result ma_engine_node_init(const ma_engine_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_engine_node* pEngineNode)
{
    ma_result result;
    size_t heapSizeInBytes;
    void* pHeap;

    result = ma_engine_node_get_heap_size(pConfig, &heapSizeInBytes);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (heapSizeInBytes > 0) {
        pHeap = ma_malloc(heapSizeInBytes, pAllocationCallbacks);
        if (pHeap == NULL) {
            return MA_OUT_OF_MEMORY;
        }
    } else {
        pHeap = NULL;
    }

    result = ma_engine_node_init_preallocated(pConfig, pHeap, pEngineNode);
    if (result != MA_SUCCESS) {
        ma_free(pHeap, pAllocationCallbacks);
        return result;
    }

    pEngineNode->_ownsHeap = MA_TRUE;
    return MA_SUCCESS;
}

MA_API void ma_engine_node_uninit(ma_engine_node* pEngineNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    /*
    The base node always needs to be uninitialized first to ensure it's detached from the graph completely before we
    destroy anything that might be in the middle of being used by the processing function.
    */
    ma_node_uninit(&pEngineNode->baseNode, NULL);

    /* Now that the node has been uninitialized we can safely uninitialize the rest. */
    ma_spatializer_uninit(&pEngineNode->spatializer, NULL);
    ma_linear_resampler_uninit(&pEngineNode->resampler, NULL);

    /* Free the heap last. */
    if (pEngineNode->_pHeap != NULL && pEngineNode->_ownsHeap) {
        ma_free(pEngineNode->_pHeap, pAllocationCallbacks);
    }
}


MA_API ma_sound_config ma_sound_config_init(void)
{
    ma_sound_config config;

    MA_ZERO_OBJECT(&config);

    return config;
}

MA_API ma_sound_group_config ma_sound_group_config_init(void)
{
    ma_sound_group_config config;

    MA_ZERO_OBJECT(&config);

    return config;
}


MA_API ma_engine_config ma_engine_config_init(void)
{
    ma_engine_config config;

    MA_ZERO_OBJECT(&config);
    config.listenerCount = 1;   /* Always want at least one listener. */

    return config;
}


static void ma_engine_data_callback_internal(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    ma_engine* pEngine = (ma_engine*)pDevice->pUserData;

    /*
    Experiment: Try processing a resource manager job if we're on the Emscripten build.

    This serves two purposes:

        1) It ensures jobs are actually processed at some point since we cannot guarantee that the
           caller is doing the right thing and calling ma_resource_manager_process_next_job(); and

        2) It's an attempt at working around an issue where processing jobs on the Emscripten main
           loop doesn't work as well as it should. When trying to load sounds without the `DECODE`
           flag or with the `ASYNC` flag, the sound data is just not able to be loaded in time
           before the callback is processed. I think it's got something to do with the single-
           threaded nature of Web, but I'm not entirely sure.
    */
    #if !defined(MA_NO_RESOURCE_MANAGER) && defined(MA_EMSCRIPTEN)
    {
        if (pEngine->pResourceManager != NULL) {
            if ((pEngine->pResourceManager->config.flags & MA_RESOURCE_MANAGER_FLAG_NO_THREADING) != 0) {
                ma_resource_manager_process_next_job(pEngine->pResourceManager);
            }
        }
    }
    #endif

    ma_engine_data_callback(pEngine, pFramesOut, pFramesIn, frameCount);
}

MA_API ma_result ma_engine_init(const ma_engine_config* pConfig, ma_engine* pEngine)
{
    ma_result result;
    ma_node_graph_config nodeGraphConfig;
    ma_engine_config engineConfig;
    ma_spatializer_listener_config listenerConfig;
    ma_uint32 iListener;

    if (pEngine != NULL) {
        MA_ZERO_OBJECT(pEngine);
    }

    /* The config is allowed to be NULL in which case we use defaults for everything. */
    if (pConfig != NULL) {
        engineConfig = *pConfig;
    } else {
        engineConfig = ma_engine_config_init();
    }

    pEngine->pResourceManager = engineConfig.pResourceManager;
    pEngine->pDevice          = engineConfig.pDevice;
    ma_allocation_callbacks_init_copy(&pEngine->allocationCallbacks, &engineConfig.allocationCallbacks);

    /* If we don't have a device, we need one. */
    if (pEngine->pDevice == NULL) {
        ma_device_config deviceConfig;

        pEngine->pDevice = (ma_device*)ma_malloc(sizeof(*pEngine->pDevice), &pEngine->allocationCallbacks);
        if (pEngine->pDevice == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.pDeviceID        = engineConfig.pPlaybackDeviceID;
        deviceConfig.playback.format           = ma_format_f32;
        deviceConfig.playback.channels         = engineConfig.channels;
        deviceConfig.sampleRate                = engineConfig.sampleRate;
        deviceConfig.dataCallback              = ma_engine_data_callback_internal;
        deviceConfig.pUserData                 = pEngine;
        deviceConfig.periodSizeInFrames        = engineConfig.periodSizeInFrames;
        deviceConfig.periodSizeInMilliseconds  = engineConfig.periodSizeInMilliseconds;
        deviceConfig.noPreSilencedOutputBuffer = MA_TRUE;    /* We'll always be outputting to every frame in the callback so there's no need for a pre-silenced buffer. */
        deviceConfig.noClip                    = MA_TRUE;    /* The mixing engine will do clipping itself. */

        if (engineConfig.pContext == NULL) {
            ma_context_config contextConfig = ma_context_config_init();
            contextConfig.allocationCallbacks = pEngine->allocationCallbacks;
            contextConfig.pLog = engineConfig.pLog;

            /* If the engine config does not specify a log, use the resource manager's if we have one. */
            #ifndef MA_NO_RESOURCE_MANAGER
            {
                if (contextConfig.pLog == NULL && engineConfig.pResourceManager != NULL) {
                    contextConfig.pLog = ma_resource_manager_get_log(engineConfig.pResourceManager);
                }
            }
            #endif

            result = ma_device_init_ex(NULL, 0, &contextConfig, &deviceConfig, pEngine->pDevice);
        } else {
            result = ma_device_init(engineConfig.pContext, &deviceConfig, pEngine->pDevice);
        }

        if (result != MA_SUCCESS) {
            ma_free(pEngine->pDevice, &pEngine->allocationCallbacks);
            pEngine->pDevice = NULL;
            return result;
        }

        pEngine->ownsDevice = MA_TRUE;
    }

    /*
    The engine always uses either the log that was passed into the config, or the context's log. Either
    way, the engine never has ownership of the log.
    */
    if (engineConfig.pLog != NULL) {
        pEngine->pLog = engineConfig.pLog;
    } else {
        pEngine->pLog = ma_device_get_log(pEngine->pDevice);
    }


    /* The engine is a node graph. This needs to be initialized after we have the device so we can can determine the channel count. */
    nodeGraphConfig = ma_node_graph_config_init(pEngine->pDevice->playback.channels);

    result = ma_node_graph_init(&nodeGraphConfig, &pEngine->allocationCallbacks, &pEngine->nodeGraph);
    if (result != MA_SUCCESS) {
        goto on_error_1;
    }


    /* We need at least one listener. */
    if (engineConfig.listenerCount == 0) {
        engineConfig.listenerCount = 1;
    }

    if (engineConfig.listenerCount > MA_ENGINE_MAX_LISTENERS) {
        result = MA_INVALID_ARGS;   /* Too many listeners. */
        goto on_error_1;
    }

    for (iListener = 0; iListener < engineConfig.listenerCount; iListener += 1) {
        listenerConfig = ma_spatializer_listener_config_init(pEngine->pDevice->playback.channels);

        result = ma_spatializer_listener_init(&listenerConfig, &pEngine->allocationCallbacks, &pEngine->listeners[iListener]);  /* TODO: Change this to a pre-allocated heap. */
        if (result != MA_SUCCESS) {
            goto on_error_2;
        }

        pEngine->listenerCount += 1;
    }


    /* Gain smoothing for spatialized sounds. */
    pEngine->gainSmoothTimeInFrames = engineConfig.gainSmoothTimeInFrames;
    if (pEngine->gainSmoothTimeInFrames == 0) {
        ma_uint32 gainSmoothTimeInMilliseconds = engineConfig.gainSmoothTimeInMilliseconds;
        if (gainSmoothTimeInMilliseconds == 0) {
            gainSmoothTimeInMilliseconds = 8;
        }

        pEngine->gainSmoothTimeInFrames = (gainSmoothTimeInMilliseconds * ma_engine_get_sample_rate(pEngine)) / 1000;  /* 8ms by default. */
    }


    /* We need a resource manager. */
    #ifndef MA_NO_RESOURCE_MANAGER
    {
        if (pEngine->pResourceManager == NULL) {
            ma_resource_manager_config resourceManagerConfig;

            pEngine->pResourceManager = (ma_resource_manager*)ma_malloc(sizeof(*pEngine->pResourceManager), &pEngine->allocationCallbacks);
            if (pEngine->pResourceManager == NULL) {
                result = MA_OUT_OF_MEMORY;
                goto on_error_2;
            }

            resourceManagerConfig = ma_resource_manager_config_init();
            resourceManagerConfig.pLog              = pEngine->pLog;    /* Always use the engine's log for internally-managed resource managers. */
            resourceManagerConfig.decodedFormat     = ma_format_f32;
            resourceManagerConfig.decodedChannels   = 0;  /* Leave the decoded channel count as 0 so we can get good spatialization. */
            resourceManagerConfig.decodedSampleRate = ma_engine_get_sample_rate(pEngine);
            ma_allocation_callbacks_init_copy(&resourceManagerConfig.allocationCallbacks, &pEngine->allocationCallbacks);
            resourceManagerConfig.pVFS              = engineConfig.pResourceManagerVFS;

            /* The Emscripten build cannot use threads. */
            #if defined(MA_EMSCRIPTEN)
            {
                resourceManagerConfig.jobThreadCount = 0;
                resourceManagerConfig.flags |= MA_RESOURCE_MANAGER_FLAG_NO_THREADING;
            }
            #endif

            result = ma_resource_manager_init(&resourceManagerConfig, pEngine->pResourceManager);
            if (result != MA_SUCCESS) {
                goto on_error_3;
            }

            pEngine->ownsResourceManager = MA_TRUE;
        }
    }
    #endif

    /* Setup some stuff for inlined sounds. That is sounds played with ma_engine_play_sound(). */
    ma_mutex_init(&pEngine->inlinedSoundLock);
    pEngine->pInlinedSoundHead = NULL;

    /* Start the engine if required. This should always be the last step. */
    if (engineConfig.noAutoStart == MA_FALSE) {
        result = ma_engine_start(pEngine);
        if (result != MA_SUCCESS) {
            goto on_error_4;    /* Failed to start the engine. */
        }
    }

    return MA_SUCCESS;

on_error_4:
    ma_mutex_uninit(&pEngine->inlinedSoundLock);
#ifndef MA_NO_RESOURCE_MANAGER
on_error_3:
    if (pEngine->ownsResourceManager) {
        ma_free(pEngine->pResourceManager, &pEngine->allocationCallbacks);
    }
#endif  /* MA_NO_RESOURCE_MANAGER */
on_error_2:
    for (iListener = 0; iListener < pEngine->listenerCount; iListener += 1) {
        ma_spatializer_listener_uninit(&pEngine->listeners[iListener], &pEngine->allocationCallbacks);
    }

    ma_node_graph_uninit(&pEngine->nodeGraph, &pEngine->allocationCallbacks);
on_error_1:
    if (pEngine->ownsDevice) {
        ma_device_uninit(pEngine->pDevice);
        ma_free(pEngine->pDevice, &pEngine->allocationCallbacks);
    }

    return result;
}

MA_API void ma_engine_uninit(ma_engine* pEngine)
{
    if (pEngine == NULL) {
        return;
    }

    /* The device must be uninitialized before the node graph to ensure the audio thread doesn't try accessing it. */
    if (pEngine->ownsDevice) {
        ma_device_uninit(pEngine->pDevice);
        ma_free(pEngine->pDevice, &pEngine->allocationCallbacks);
    } else {
        ma_device_stop(pEngine->pDevice);
    }

    /*
    All inlined sounds need to be deleted. I'm going to use a lock here just to future proof in case
    I want to do some kind of garbage collection later on.
    */
    ma_mutex_lock(&pEngine->inlinedSoundLock);
    {
        for (;;) {
            ma_sound_inlined* pSoundToDelete = pEngine->pInlinedSoundHead;
            if (pSoundToDelete == NULL) {
                break;  /* Done. */
            }

            pEngine->pInlinedSoundHead = pSoundToDelete->pNext;

            ma_sound_uninit(&pSoundToDelete->sound);
            ma_free(pSoundToDelete, &pEngine->allocationCallbacks);
        }
    }
    ma_mutex_unlock(&pEngine->inlinedSoundLock);
    ma_mutex_uninit(&pEngine->inlinedSoundLock);


    /* Make sure the node graph is uninitialized after the audio thread has been shutdown to prevent accessing of the node graph after being uninitialized. */
    ma_node_graph_uninit(&pEngine->nodeGraph, &pEngine->allocationCallbacks);

    /* Uninitialize the resource manager last to ensure we don't have a thread still trying to access it. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pEngine->ownsResourceManager) {
        ma_resource_manager_uninit(pEngine->pResourceManager);
        ma_free(pEngine->pResourceManager, &pEngine->allocationCallbacks);
    }
#endif
}

MA_API void ma_engine_data_callback(ma_engine* pEngine, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    (void)pFramesIn;    /* Unused. */

    ma_node_graph_read_pcm_frames(&pEngine->nodeGraph, pFramesOut, frameCount, NULL);
}

MA_API ma_device* ma_engine_get_device(ma_engine* pEngine)
{
    if (pEngine == NULL) {
        return NULL;
    }

    return pEngine->pDevice;
}

MA_API ma_log* ma_engine_get_log(ma_engine* pEngine)
{
    if (pEngine == NULL) {
        return NULL;
    }

    if (pEngine->pLog != NULL) {
        return pEngine->pLog;
    } else {
        return ma_device_get_log(ma_engine_get_device(pEngine));
    }
}

MA_API ma_node* ma_engine_get_endpoint(ma_engine* pEngine)
{
    return ma_node_graph_get_endpoint(&pEngine->nodeGraph);
}

MA_API ma_uint64 ma_engine_get_time(const ma_engine* pEngine)
{
    return ma_node_graph_get_time(&pEngine->nodeGraph);
}

MA_API ma_uint64 ma_engine_set_time(ma_engine* pEngine, ma_uint64 globalTime)
{
    return ma_node_graph_set_time(&pEngine->nodeGraph, globalTime);
}

MA_API ma_uint32 ma_engine_get_channels(const ma_engine* pEngine)
{
    return ma_node_graph_get_channels(&pEngine->nodeGraph);
}

MA_API ma_uint32 ma_engine_get_sample_rate(const ma_engine* pEngine)
{
    if (pEngine == NULL) {
        return 0;
    }

    if (pEngine->pDevice != NULL) {
        return pEngine->pDevice->sampleRate;
    } else {
        return 0;   /* No device. */
    }
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


MA_API ma_uint32 ma_engine_get_listener_count(const ma_engine* pEngine)
{
    if (pEngine == NULL) {
        return 0;
    }

    return pEngine->listenerCount;
}

MA_API ma_uint32 ma_engine_find_closest_listener(const ma_engine* pEngine, float absolutePosX, float absolutePosY, float absolutePosZ)
{
    ma_uint32 iListener;
    ma_uint32 iListenerClosest;
    float closestLen2 = MA_FLT_MAX;

    if (pEngine == NULL || pEngine->listenerCount == 1) {
        return 0;
    }

    iListenerClosest = 0;
    for (iListener = 0; iListener < pEngine->listenerCount; iListener += 1) {
        float len2 = ma_vec3f_len2(ma_vec3f_sub(pEngine->listeners[iListener].position, ma_vec3f_init_3f(absolutePosX, absolutePosY, absolutePosZ)));
        if (closestLen2 > len2) {
            closestLen2 = len2;
            iListenerClosest = iListener;
        }
    }

    MA_ASSERT(iListenerClosest < 255);
    return iListenerClosest;
}

MA_API void ma_engine_listener_set_position(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return;
    }

    ma_spatializer_listener_set_position(&pEngine->listeners[listenerIndex], x, y, z);
}

MA_API ma_vec3f ma_engine_listener_get_position(const ma_engine* pEngine, ma_uint32 listenerIndex)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return ma_spatializer_listener_get_position(&pEngine->listeners[listenerIndex]);
}

MA_API void ma_engine_listener_set_direction(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return;
    }

    ma_spatializer_listener_set_direction(&pEngine->listeners[listenerIndex], x, y, z);
}

MA_API ma_vec3f ma_engine_listener_get_direction(const ma_engine* pEngine, ma_uint32 listenerIndex)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return ma_vec3f_init_3f(0, 0, -1);
    }

    return ma_spatializer_listener_get_direction(&pEngine->listeners[listenerIndex]);
}

MA_API void ma_engine_listener_set_velocity(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return;
    }

    ma_spatializer_listener_set_velocity(&pEngine->listeners[listenerIndex], x, y, z);
}

MA_API ma_vec3f ma_engine_listener_get_velocity(const ma_engine* pEngine, ma_uint32 listenerIndex)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return ma_spatializer_listener_get_velocity(&pEngine->listeners[listenerIndex]);
}

MA_API void ma_engine_listener_set_cone(ma_engine* pEngine, ma_uint32 listenerIndex, float innerAngleInRadians, float outerAngleInRadians, float outerGain)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return;
    }

    ma_spatializer_listener_set_cone(&pEngine->listeners[listenerIndex], innerAngleInRadians, outerAngleInRadians, outerGain);
}

MA_API void ma_engine_listener_get_cone(const ma_engine* pEngine, ma_uint32 listenerIndex, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain)
{
    if (pInnerAngleInRadians != NULL) {
        *pInnerAngleInRadians = 0;
    }

    if (pOuterAngleInRadians != NULL) {
        *pOuterAngleInRadians = 0;
    }

    if (pOuterGain != NULL) {
        *pOuterGain = 0;
    }

    ma_spatializer_listener_get_cone(&pEngine->listeners[listenerIndex], pInnerAngleInRadians, pOuterAngleInRadians, pOuterGain);
}

MA_API void ma_engine_listener_set_world_up(ma_engine* pEngine, ma_uint32 listenerIndex, float x, float y, float z)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return;
    }

    ma_spatializer_listener_set_world_up(&pEngine->listeners[listenerIndex], x, y, z);
}

MA_API ma_vec3f ma_engine_listener_get_world_up(const ma_engine* pEngine, ma_uint32 listenerIndex)
{
    if (pEngine == NULL || listenerIndex >= pEngine->listenerCount) {
        return ma_vec3f_init_3f(0, 1, 0);
    }

    return ma_spatializer_listener_get_world_up(&pEngine->listeners[listenerIndex]);
}


#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_engine_play_sound_ex(ma_engine* pEngine, const char* pFilePath, ma_node* pNode, ma_uint32 nodeInputBusIndex)
{
    ma_result result = MA_SUCCESS;
    ma_sound_inlined* pSound = NULL;
    ma_sound_inlined* pNextSound = NULL;

    if (pEngine == NULL || pFilePath == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Attach to the endpoint node if nothing is specicied. */
    if (pNode == NULL) {
        pNode = ma_node_graph_get_endpoint(&pEngine->nodeGraph);
        nodeInputBusIndex = 0;
    }

    /*
    We want to check if we can recycle an already-allocated inlined sound. Since this is just a
    helper I'm not *too* concerned about performance here and I'm happy to use a lock to keep
    the implementation simple. Maybe this can be optimized later if there's enough demand, but
    if this function is being used it probably means the caller doesn't really care too much.
    
    What we do is check the atEnd flag. When this is true, we can recycle the sound. Otherwise
    we just keep iterating. If we reach the end without finding a sound to recycle we just
    allocate a new one. This doesn't scale well for a massive number of sounds being played
    simultaneously as we don't ever actually free the sound objects. Some kind of garbage
    collection routine might be valuable for this which I'll think about.
    */
    ma_mutex_lock(&pEngine->inlinedSoundLock);
    {
        ma_uint32 soundFlags = 0;

        for (pNextSound = pEngine->pInlinedSoundHead; pNextSound != NULL; pNextSound = pNextSound->pNext) {
            if (ma_sound_at_end(&pNextSound->sound)) {
                /*
                The sound is at the end which means it's available for recycling. All we need to do
                is uninitialize it and reinitialize it. All we're doing is recycling memory.
                */
                pSound = pNextSound;
                c89atomic_fetch_sub_32(&pEngine->inlinedSoundCount, 1);
                break;
            }
        }

        if (pSound != NULL) {
            /*
            We actually want to detach the sound from the list here. The reason is because we want the sound
            to be in a consistent state at the non-recycled case to simplify the logic below.
            */
            if (pEngine->pInlinedSoundHead == pSound) {
                pEngine->pInlinedSoundHead =  pSound->pNext;
            }

            if (pSound->pPrev != NULL) {
                pSound->pPrev->pNext = pSound->pNext;
            }
            if (pSound->pNext != NULL) {
                pSound->pNext->pPrev = pSound->pPrev;
            }

            /* Now the previous sound needs to be uninitialized. */
            ma_sound_uninit(&pNextSound->sound);
        } else {
            /* No sound available for recycling. Allocate one now. */
            pSound = (ma_sound_inlined*)ma_malloc(sizeof(*pSound), &pEngine->allocationCallbacks);
        }

        if (pSound != NULL) {   /* Safety check for the allocation above. */
            /*
            At this point we should have memory allocated for the inlined sound. We just need
            to initialize it like a normal sound now.
            */
            soundFlags |= MA_SOUND_FLAG_ASYNC;                 /* For inlined sounds we don't want to be sitting around waiting for stuff to load so force an async load. */
            soundFlags |= MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT; /* We want specific control over where the sound is attached in the graph. We'll attach it manually just before playing the sound. */
            soundFlags |= MA_SOUND_FLAG_NO_PITCH;              /* Pitching isn't usable with inlined sounds, so disable it to save on speed. */
            soundFlags |= MA_SOUND_FLAG_NO_SPATIALIZATION;     /* Not currently doing spatialization with inlined sounds, but this might actually change later. For now disable spatialization. Will be removed if we ever add support for spatialization here. */

            result = ma_sound_init_from_file(pEngine, pFilePath, soundFlags, NULL, NULL, &pSound->sound);
            if (result == MA_SUCCESS) {
                /* Now attach the sound to the graph. */
                result = ma_node_attach_output_bus(pSound, 0, pNode, nodeInputBusIndex);
                if (result == MA_SUCCESS) {
                    /* At this point the sound should be loaded and we can go ahead and add it to the list. The new item becomes the new head. */
                    pSound->pNext = pEngine->pInlinedSoundHead;
                    pSound->pPrev = NULL;

                    pEngine->pInlinedSoundHead = pSound;    /* <-- This is what attaches the sound to the list. */
                    if (pSound->pNext != NULL) {
                        pSound->pNext->pPrev = pSound;
                    }
                } else {
                    ma_free(pSound, &pEngine->allocationCallbacks);
                }
            } else {
                ma_free(pSound, &pEngine->allocationCallbacks);
            }
        } else {
            result = MA_OUT_OF_MEMORY;
        }
    }
    ma_mutex_unlock(&pEngine->inlinedSoundLock);

    if (result != MA_SUCCESS) {
        return result;
    }

    /* Finally we can start playing the sound. */
    result = ma_sound_start(&pSound->sound);
    if (result != MA_SUCCESS) {
        /* Failed to start the sound. We need to mark it for recycling and return an error. */
        c89atomic_exchange_8(&pSound->sound.atEnd, MA_TRUE);
        return result;
    }

    c89atomic_fetch_add_32(&pEngine->inlinedSoundCount, 1);
    return result;
}

MA_API ma_result ma_engine_play_sound(ma_engine* pEngine, const char* pFilePath, ma_sound_group* pGroup)
{
    return ma_engine_play_sound_ex(pEngine, pFilePath, pGroup, 0);
}
#endif


static ma_result ma_sound_preinit(ma_engine* pEngine, ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pSound);
    pSound->seekTarget = MA_SEEK_TARGET_NONE;

    if (pEngine == NULL) {
        return MA_INVALID_ARGS;
    }

    return MA_SUCCESS;
}

static ma_result ma_sound_init_from_data_source_internal(ma_engine* pEngine, const ma_sound_config* pConfig, ma_sound* pSound)
{
    ma_result result;
    ma_engine_node_config engineNodeConfig;
    ma_engine_node_type type;   /* Will be set to ma_engine_node_type_group if no data source is specified. */

    /* Do not clear pSound to zero here - that's done at a higher level with ma_sound_preinit(). */
    MA_ASSERT(pEngine != NULL);
    MA_ASSERT(pSound  != NULL);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pSound->pDataSource = pConfig->pDataSource;

    if (pConfig->pDataSource != NULL) {
        type = ma_engine_node_type_sound;
    } else {
        type = ma_engine_node_type_group;
    }

    /*
    Sounds are engine nodes. Before we can initialize this we need to determine the channel count.
    If we can't do this we need to abort. It's up to the caller to ensure they're using a data
    source that provides this information upfront.
    */
    engineNodeConfig = ma_engine_node_config_init(pEngine, type, pConfig->flags);
    engineNodeConfig.channelsIn  = pConfig->channelsIn;
    engineNodeConfig.channelsOut = pConfig->channelsOut;

    /* If we're loading from a data source the input channel count needs to be the data source's native channel count. */
    if (pConfig->pDataSource != NULL) {
        result = ma_data_source_get_data_format(pConfig->pDataSource, NULL, &engineNodeConfig.channelsIn, &engineNodeConfig.sampleRate, NULL, 0);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to retrieve the channel count. */
        }

        if (engineNodeConfig.channelsIn == 0) {
            return MA_INVALID_OPERATION;    /* Invalid channel count. */
        }
    }
    

    /* Getting here means we should have a valid channel count and we can initialize the engine node. */
    result = ma_engine_node_init(&engineNodeConfig, &pEngine->allocationCallbacks, &pSound->engineNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* If no attachment is specified, attach the sound straight to the endpoint. */
    if (pConfig->pInitialAttachment == NULL) {
        /* No group. Attach straight to the endpoint by default, unless the caller has requested that do not. */
        if ((pConfig->flags & MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT) == 0) {
            result = ma_node_attach_output_bus(pSound, 0, ma_node_graph_get_endpoint(&pEngine->nodeGraph), 0);
        }
    } else {
        /* An attachment is specified. Attach to it by default. The sound has only a single output bus, and the config will specify which input bus to attach to. */
        result = ma_node_attach_output_bus(pSound, 0, pConfig->pInitialAttachment, pConfig->initialAttachmentInputBusIndex);
    }

    if (result != MA_SUCCESS) {
        ma_engine_node_uninit(&pSound->engineNode, &pEngine->allocationCallbacks);
        return result;
    }

    return MA_SUCCESS;
}

#ifndef MA_NO_RESOURCE_MANAGER
MA_API ma_result ma_sound_init_from_file_internal(ma_engine* pEngine, const ma_sound_config* pConfig, ma_sound* pSound)
{
    ma_result result = MA_SUCCESS;
    ma_uint32 flags;
    ma_sound_config config;
    ma_resource_manager_pipeline_notifications notifications;

    /*
    The engine requires knowledge of the channel count of the underlying data source before it can
    initialize the sound. Therefore, we need to make the resource manager wait until initialization
    of the underlying data source to be initialized so we can get access to the channel count. To
    do this, the MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT is forced.

    Because we're initializing the data source before the sound, there's a chance the notification
    will get triggered before this function returns. This is OK, so long as the caller is aware of
    it and can avoid accessing the sound from within the notification.
    */
    flags = pConfig->flags | MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT;

    pSound->pResourceManagerDataSource = (ma_resource_manager_data_source*)ma_malloc(sizeof(*pSound->pResourceManagerDataSource), &pEngine->allocationCallbacks);
    if (pSound->pResourceManagerDataSource == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    notifications = ma_resource_manager_pipeline_notifications_init();
    notifications.done.pFence = pConfig->pDoneFence;

    /*
    We must wrap everything around the fence if one was specified. This ensures ma_fence_wait() does
    not return prematurely before the sound has finished initializing.
    */
    if (notifications.done.pFence) { ma_fence_acquire(notifications.done.pFence); }
    {
        if (pConfig->pFilePath != NULL) {
            result = ma_resource_manager_data_source_init(pEngine->pResourceManager, pConfig->pFilePath, flags, &notifications, pSound->pResourceManagerDataSource);
        } else {
            result = ma_resource_manager_data_source_init_w(pEngine->pResourceManager, pConfig->pFilePathW, flags, &notifications, pSound->pResourceManagerDataSource);
        }
    
        if (result != MA_SUCCESS) {
            goto done;
        }

        pSound->ownsDataSource = MA_TRUE;   /* <-- Important. Not setting this will result in the resource manager data source never getting uninitialized. */

        /* We need to use a slightly customized version of the config so we'll need to make a copy. */
        config = *pConfig;
        config.pFilePath   = NULL;
        config.pFilePathW  = NULL;
        config.pDataSource = pSound->pResourceManagerDataSource;

        result = ma_sound_init_from_data_source_internal(pEngine, &config, pSound);
        if (result != MA_SUCCESS) {
            ma_resource_manager_data_source_uninit(pSound->pResourceManagerDataSource);
            ma_free(pSound->pResourceManagerDataSource, &pEngine->allocationCallbacks);
            MA_ZERO_OBJECT(pSound);
            goto done;
        }
    }
done:
    if (notifications.done.pFence) { ma_fence_release(notifications.done.pFence); }
    return result;
}

MA_API ma_result ma_sound_init_from_file(ma_engine* pEngine, const char* pFilePath, ma_uint32 flags, ma_sound_group* pGroup, ma_fence* pDoneFence, ma_sound* pSound)
{
    ma_sound_config config = ma_sound_config_init();
    config.pFilePath          = pFilePath;
    config.flags              = flags;
    config.pInitialAttachment = pGroup;
    config.pDoneFence         = pDoneFence;
    return ma_sound_init_ex(pEngine, &config, pSound);
}

MA_API ma_result ma_sound_init_from_file_w(ma_engine* pEngine, const wchar_t* pFilePath, ma_uint32 flags, ma_sound_group* pGroup, ma_fence* pDoneFence, ma_sound* pSound)
{
    ma_sound_config config = ma_sound_config_init();
    config.pFilePathW         = pFilePath;
    config.flags              = flags;
    config.pInitialAttachment = pGroup;
    config.pDoneFence         = pDoneFence;
    return ma_sound_init_ex(pEngine, &config, pSound);
}

MA_API ma_result ma_sound_init_copy(ma_engine* pEngine, const ma_sound* pExistingSound, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound)
{
    ma_result result;
    ma_sound_config config;

    result = ma_sound_preinit(pEngine, pSound);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (pExistingSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Cloning only works for data buffers (not streams) that are loaded from the resource manager. */
    if (pExistingSound->pResourceManagerDataSource == NULL) {
        return MA_INVALID_OPERATION;
    }

    /*
    We need to make a clone of the data source. If the data source is not a data buffer (i.e. a stream)
    the this will fail.
    */
    pSound->pResourceManagerDataSource = (ma_resource_manager_data_source*)ma_malloc(sizeof(*pSound->pResourceManagerDataSource), &pEngine->allocationCallbacks);
    if (pSound->pResourceManagerDataSource == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_resource_manager_data_source_init_copy(pEngine->pResourceManager, pExistingSound->pResourceManagerDataSource, pSound->pResourceManagerDataSource);
    if (result != MA_SUCCESS) {
        ma_free(pSound->pResourceManagerDataSource, &pEngine->allocationCallbacks);
        return result;
    }

    config = ma_sound_config_init();
    config.pDataSource        = pSound->pResourceManagerDataSource;
    config.flags              = flags;
    config.pInitialAttachment = pGroup;
    
    result = ma_sound_init_from_data_source_internal(pEngine, &config, pSound);
    if (result != MA_SUCCESS) {
        ma_resource_manager_data_source_uninit(pSound->pResourceManagerDataSource);
        ma_free(pSound->pResourceManagerDataSource, &pEngine->allocationCallbacks);
        MA_ZERO_OBJECT(pSound);
        return result;
    }

    return MA_SUCCESS;
}
#endif

MA_API ma_result ma_sound_init_from_data_source(ma_engine* pEngine, ma_data_source* pDataSource, ma_uint32 flags, ma_sound_group* pGroup, ma_sound* pSound)
{
    ma_sound_config config = ma_sound_config_init();
    config.pDataSource        = pDataSource;
    config.flags              = flags;
    config.pInitialAttachment = pGroup;
    return ma_sound_init_ex(pEngine, &config, pSound);
}

MA_API ma_result ma_sound_init_ex(ma_engine* pEngine, const ma_sound_config* pConfig, ma_sound* pSound)
{
    ma_result result;

    result = ma_sound_preinit(pEngine, pSound);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    /* We need to load the sound differently depending on whether or not we're loading from a file. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pConfig->pFilePath != NULL || pConfig->pFilePathW != NULL) {
        return ma_sound_init_from_file_internal(pEngine, pConfig, pSound);
    } else
#endif
    {
        /*
        Getting here means we're not loading from a file. We may be loading from an already-initialized
        data source, or none at all. If we aren't specifying any data source, we'll be initializing the
        the equivalent to a group. ma_data_source_init_from_data_source_internal() will deal with this
        for us, so no special treatment required here.
        */
        return ma_sound_init_from_data_source_internal(pEngine, pConfig, pSound);
    }
}

MA_API void ma_sound_uninit(ma_sound* pSound)
{
    if (pSound == NULL) {
        return;
    }

    /*
    Always uninitialize the node first. This ensures it's detached from the graph and does not return until it has done
    so which makes thread safety beyond this point trivial.
    */
    ma_node_uninit(pSound, &pSound->engineNode.pEngine->allocationCallbacks);

    /* Once the sound is detached from the group we can guarantee that it won't be referenced by the mixer thread which means it's safe for us to destroy the data source. */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pSound->ownsDataSource) {
        ma_resource_manager_data_source_uninit(pSound->pResourceManagerDataSource);
        ma_free(pSound->pResourceManagerDataSource, &pSound->engineNode.pEngine->allocationCallbacks);
        pSound->pDataSource = NULL;
    }
#else
    MA_ASSERT(pSound->ownsDataSource == MA_FALSE);
#endif
}

MA_API ma_engine* ma_sound_get_engine(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return NULL;
    }

    return pSound->engineNode.pEngine;
}

MA_API ma_data_source* ma_sound_get_data_source(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return NULL;
    }

    return pSound->pDataSource;
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
        if (result != MA_SUCCESS && result != MA_NOT_IMPLEMENTED) {
            return result;  /* Failed to seek back to the start. */
        }

        /* Make sure we clear the end indicator. */
        c89atomic_exchange_8(&pSound->atEnd, MA_FALSE);
    }

    /* Make sure the sound is started. If there's a start delay, the sound won't actually start until the start time is reached. */
    ma_node_set_state(pSound, ma_node_state_started);

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_stop(ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* This will stop the sound immediately. Use ma_sound_set_stop_time() to stop the sound at a specific time. */
    ma_node_set_state(pSound, ma_node_state_stopped);

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_set_volume(ma_sound* pSound, float volume)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The volume is controlled via the output bus. */
    ma_node_set_output_bus_volume(pSound, 0, volume);

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_set_gain_db(ma_sound* pSound, float gainDB)
{
    return ma_sound_set_volume(pSound, ma_gain_db_to_factor(gainDB));
}

MA_API void ma_sound_set_pan(ma_sound* pSound, float pan)
{
    if (pSound == NULL) {
        return;
    }

    ma_panner_set_pan(&pSound->engineNode.panner, pan);
}

MA_API void ma_sound_set_pan_mode(ma_sound* pSound, ma_pan_mode panMode)
{
    if (pSound == NULL) {
        return;
    }

    ma_panner_set_mode(&pSound->engineNode.panner, panMode);
}

MA_API void ma_sound_set_pitch(ma_sound* pSound, float pitch)
{
    if (pSound == NULL) {
        return;
    }

    c89atomic_exchange_explicit_f32(&pSound->engineNode.pitch, pitch, c89atomic_memory_order_release);
}

MA_API void ma_sound_set_spatialization_enabled(ma_sound* pSound, ma_bool32 enabled)
{
    if (pSound == NULL) {
        return;
    }

    c89atomic_exchange_explicit_8(&pSound->engineNode.isSpatializationDisabled, !enabled, c89atomic_memory_order_release);
}

MA_API void ma_sound_set_pinned_listener_index(ma_sound* pSound, ma_uint8 listenerIndex)
{
    if (pSound == NULL || listenerIndex >= ma_engine_get_listener_count(ma_sound_get_engine(pSound))) {
        return;
    }

    c89atomic_exchange_explicit_8(&pSound->engineNode.pinnedListenerIndex, listenerIndex, c89atomic_memory_order_release);
}

MA_API ma_uint8 ma_sound_get_pinned_listener_index(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_LISTENER_INDEX_CLOSEST;
    }

    return c89atomic_load_explicit_8(&pSound->engineNode.pinnedListenerIndex, c89atomic_memory_order_acquire);
}

MA_API void ma_sound_set_position(ma_sound* pSound, float x, float y, float z)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_position(&pSound->engineNode.spatializer, x, y, z);
}

MA_API ma_vec3f ma_sound_get_position(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return ma_spatializer_get_position(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_direction(ma_sound* pSound, float x, float y, float z)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_direction(&pSound->engineNode.spatializer, x, y, z);
}

MA_API ma_vec3f ma_sound_get_direction(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return ma_spatializer_get_direction(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_velocity(ma_sound* pSound, float x, float y, float z)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_velocity(&pSound->engineNode.spatializer, x, y, z);
}

MA_API ma_vec3f ma_sound_get_velocity(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return ma_vec3f_init_3f(0, 0, 0);
    }

    return ma_spatializer_get_velocity(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_attenuation_model(ma_sound* pSound, ma_attenuation_model attenuationModel)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_attenuation_model(&pSound->engineNode.spatializer, attenuationModel);
}

MA_API ma_attenuation_model ma_sound_get_attenuation_model(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return ma_attenuation_model_none;
    }

    return ma_spatializer_get_attenuation_model(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_positioning(ma_sound* pSound, ma_positioning positioning)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_positioning(&pSound->engineNode.spatializer, positioning);
}

MA_API ma_positioning ma_sound_get_positioning(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return ma_positioning_absolute;
    }

    return ma_spatializer_get_positioning(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_rolloff(ma_sound* pSound, float rolloff)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_rolloff(&pSound->engineNode.spatializer, rolloff);
}

MA_API float ma_sound_get_rolloff(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return 0;
    }

    return ma_spatializer_get_rolloff(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_min_gain(ma_sound* pSound, float minGain)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_min_gain(&pSound->engineNode.spatializer, minGain);
}

MA_API float ma_sound_get_min_gain(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return 0;
    }

    return ma_spatializer_get_min_gain(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_max_gain(ma_sound* pSound, float maxGain)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_max_gain(&pSound->engineNode.spatializer, maxGain);
}

MA_API float ma_sound_get_max_gain(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return 0;
    }

    return ma_spatializer_get_max_gain(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_min_distance(ma_sound* pSound, float minDistance)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_min_distance(&pSound->engineNode.spatializer, minDistance);
}

MA_API float ma_sound_get_min_distance(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return 0;
    }

    return ma_spatializer_get_min_distance(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_max_distance(ma_sound* pSound, float maxDistance)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_max_distance(&pSound->engineNode.spatializer, maxDistance);
}

MA_API float ma_sound_get_max_distance(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return 0;
    }

    return ma_spatializer_get_max_distance(&pSound->engineNode.spatializer);
}

MA_API void ma_sound_set_cone(ma_sound* pSound, float innerAngleInRadians, float outerAngleInRadians, float outerGain)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_cone(&pSound->engineNode.spatializer, innerAngleInRadians, outerAngleInRadians, outerGain);
}

MA_API void ma_sound_get_cone(const ma_sound* pSound, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain)
{
    if (pInnerAngleInRadians != NULL) {
        *pInnerAngleInRadians = 0;
    }

    if (pOuterAngleInRadians != NULL) {
        *pOuterAngleInRadians = 0;
    }

    if (pOuterGain != NULL) {
        *pOuterGain = 0;
    }

    ma_spatializer_get_cone(&pSound->engineNode.spatializer, pInnerAngleInRadians, pOuterAngleInRadians, pOuterGain);
}

MA_API void ma_sound_set_doppler_factor(ma_sound* pSound, float dopplerFactor)
{
    if (pSound == NULL) {
        return;
    }

    ma_spatializer_set_doppler_factor(&pSound->engineNode.spatializer, dopplerFactor);
}

MA_API float ma_sound_get_doppler_factor(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return 0;
    }

    return ma_spatializer_get_doppler_factor(&pSound->engineNode.spatializer);
}


MA_API void ma_sound_set_fade_in_pcm_frames(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames)
{
    if (pSound == NULL) {
        return;
    }

    ma_fader_set_fade(&pSound->engineNode.fader, volumeBeg, volumeEnd, fadeLengthInFrames);
}

MA_API void ma_sound_set_fade_in_milliseconds(ma_sound* pSound, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds)
{
    if (pSound == NULL) {
        return;
    }

    ma_sound_set_fade_in_pcm_frames(pSound, volumeBeg, volumeEnd, (fadeLengthInMilliseconds * pSound->engineNode.fader.config.sampleRate) / 1000);
}

MA_API float ma_sound_get_current_fade_volume(ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_fader_get_current_volume(&pSound->engineNode.fader);
}

MA_API void ma_sound_set_start_time_in_pcm_frames(ma_sound* pSound, ma_uint64 absoluteGlobalTimeInFrames)
{
    if (pSound == NULL) {
        return;
    }

    ma_node_set_state_time(pSound, ma_node_state_started, absoluteGlobalTimeInFrames);
}

MA_API void ma_sound_set_start_time_in_milliseconds(ma_sound* pSound, ma_uint64 absoluteGlobalTimeInMilliseconds)
{
    if (pSound == NULL) {
        return;
    }

    ma_sound_set_start_time_in_pcm_frames(pSound, absoluteGlobalTimeInMilliseconds * ma_engine_get_sample_rate(ma_sound_get_engine(pSound)) / 1000);
}

MA_API void ma_sound_set_stop_time_in_pcm_frames(ma_sound* pSound, ma_uint64 absoluteGlobalTimeInFrames)
{
    if (pSound == NULL) {
        return;
    }

    ma_node_set_state_time(pSound, ma_node_state_stopped, absoluteGlobalTimeInFrames);
}

MA_API void ma_sound_set_stop_time_in_milliseconds(ma_sound* pSound, ma_uint64 absoluteGlobalTimeInMilliseconds)
{
    if (pSound == NULL) {
        return;
    }

    ma_sound_set_stop_time_in_pcm_frames(pSound, absoluteGlobalTimeInMilliseconds * ma_engine_get_sample_rate(ma_sound_get_engine(pSound)) / 1000);
}

MA_API ma_bool32 ma_sound_is_playing(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_FALSE;
    }

    return ma_node_get_state_by_time(pSound, ma_engine_get_time(ma_sound_get_engine(pSound))) == ma_node_state_started;
}

MA_API ma_uint64 ma_sound_get_time_in_pcm_frames(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return 0;
    }

    return ma_node_get_time(pSound);
}

MA_API void ma_sound_set_looping(ma_sound* pSound, ma_bool8 isLooping)
{
    if (pSound == NULL) {
        return;
    }

    /* Looping is only a valid concept if the sound is backed by a data source. */
    if (pSound->pDataSource == NULL) {
        return;
    }

    c89atomic_exchange_8(&pSound->isLooping, isLooping);

    /*
    This is a little bit of a hack, but basically we need to set the looping flag at the data source level if we are running a data source managed by
    the resource manager, and that is backed by a data stream. The reason for this is that the data stream itself needs to be aware of the looping
    requirements so that it can do seamless loop transitions. The better solution for this is to add ma_data_source_set_looping() and just call this
    generically.
    */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pSound->pDataSource == pSound->pResourceManagerDataSource) {
        ma_resource_manager_data_source_set_looping(pSound->pResourceManagerDataSource, isLooping);
    }
#endif
}

MA_API ma_bool32 ma_sound_is_looping(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_FALSE;
    }

    /* There is no notion of looping for sounds that are not backed by a data source. */
    if (pSound->pDataSource == NULL) {
        return MA_FALSE;
    }

    return c89atomic_load_8(&pSound->isLooping);
}

MA_API ma_bool32 ma_sound_at_end(const ma_sound* pSound)
{
    if (pSound == NULL) {
        return MA_FALSE;
    }

    /* There is no notion of an end of a sound if it's not backed by a data source. */
    if (pSound->pDataSource == NULL) {
        return MA_FALSE;
    }

    return c89atomic_load_8(&pSound->atEnd);
}

MA_API ma_result ma_sound_seek_to_pcm_frame(ma_sound* pSound, ma_uint64 frameIndex)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Seeking is only valid for sounds that are backed by a data source. */
    if (pSound->pDataSource == NULL) {
        return MA_INVALID_OPERATION;
    }

    /*
    Resource manager data sources are thread safe which means we can just seek immediately. However, we cannot guarantee that other data sources are
    thread safe as well so in that case we'll need to get the mixing thread to seek for us to ensure we don't try seeking at the same time as reading.
    */
#ifndef MA_NO_RESOURCE_MANAGER
    if (pSound->pDataSource == pSound->pResourceManagerDataSource) {
        ma_result result = ma_resource_manager_data_source_seek_to_pcm_frame(pSound->pResourceManagerDataSource, frameIndex);
        if (result != MA_SUCCESS) {
            return result;
        }

        /* Time dependant effects need to have their timers updated. */
        return ma_node_set_time(&pSound->engineNode, frameIndex);
    }
#endif

    /* Getting here means the data source is not a resource manager data source so we'll need to get the mixing thread to do the seeking for us. */
    pSound->seekTarget = frameIndex;

    return MA_SUCCESS;
}

MA_API ma_result ma_sound_get_data_format(ma_sound* pSound, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The data format is retrieved directly from the data source if the sound is backed by one. Otherwise we pull it from the node. */
    if (pSound->pDataSource == NULL) {
        ma_uint32 channels;

        if (pFormat != NULL) {
            *pFormat = ma_format_f32;
        }

        channels = ma_node_get_input_channels(&pSound->engineNode, 0);
        if (pChannels != NULL) {
            *pChannels = channels;
        }

        if (pSampleRate != NULL) {
            *pSampleRate = pSound->engineNode.resampler.config.sampleRateIn;
        }

        if (pChannelMap != NULL) {
            ma_get_standard_channel_map(ma_standard_channel_map_default, pChannelMap, channelMapCap, channels);
        }

        return MA_SUCCESS;
    } else {
        return ma_data_source_get_data_format(pSound->pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
    }
}

MA_API ma_result ma_sound_get_cursor_in_pcm_frames(ma_sound* pSound, ma_uint64* pCursor)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The notion of a cursor is only valid for sounds that are backed by a data source. */
    if (pSound->pDataSource == NULL) {
        return MA_INVALID_OPERATION;
    }

    return ma_data_source_get_cursor_in_pcm_frames(pSound->pDataSource, pCursor);
}

MA_API ma_result ma_sound_get_length_in_pcm_frames(ma_sound* pSound, ma_uint64* pLength)
{
    if (pSound == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The notion of a sound length is only valid for sounds that are backed by a data source. */
    if (pSound->pDataSource == NULL) {
        return MA_INVALID_OPERATION;
    }

    return ma_data_source_get_length_in_pcm_frames(pSound->pDataSource, pLength);
}


MA_API ma_result ma_sound_group_init(ma_engine* pEngine, ma_uint32 flags, ma_sound_group* pParentGroup, ma_sound_group* pGroup)
{
    ma_sound_group_config config = ma_sound_group_config_init();
    config.flags              = flags;
    config.pInitialAttachment = pParentGroup;
    return ma_sound_group_init_ex(pEngine, &config, pGroup);
}

MA_API ma_result ma_sound_group_init_ex(ma_engine* pEngine, const ma_sound_group_config* pConfig, ma_sound_group* pGroup)
{
    ma_sound_config soundConfig;

    if (pGroup == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pGroup);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    /* A sound group is just a sound without a data source. */
    soundConfig = *pConfig;
    soundConfig.pFilePath   = NULL;
    soundConfig.pFilePathW  = NULL;
    soundConfig.pDataSource = NULL;

    /*
    Groups need to have spatialization disabled by default because I think it'll be pretty rare
    that programs will want to spatialize groups (but not unheard of). Certainly it feels like
    disabling this by default feels like the right option. Spatialization can be enabled with a
    call to ma_sound_group_set_spatialization_enabled().
    */
    soundConfig.flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

    return ma_sound_init_ex(pEngine, &soundConfig, pGroup);
}

MA_API void ma_sound_group_uninit(ma_sound_group* pGroup)
{
    ma_sound_uninit(pGroup);
}

MA_API ma_engine* ma_sound_group_get_engine(const ma_sound_group* pGroup)
{
    return ma_sound_get_engine(pGroup);
}

MA_API ma_result ma_sound_group_start(ma_sound_group* pGroup)
{
    return ma_sound_start(pGroup);
}

MA_API ma_result ma_sound_group_stop(ma_sound_group* pGroup)
{
    return ma_sound_stop(pGroup);
}

MA_API ma_result ma_sound_group_set_volume(ma_sound_group* pGroup, float volume)
{
    return ma_sound_set_volume(pGroup, volume);
}

MA_API ma_result ma_sound_group_set_gain_db(ma_sound_group* pGroup, float gainDB)
{
    return ma_sound_set_gain_db(pGroup, gainDB);
}

MA_API void ma_sound_group_set_pan(ma_sound_group* pGroup, float pan)
{
    ma_sound_set_pan(pGroup, pan);
}

MA_API void ma_sound_group_set_pan_mode(ma_sound_group* pGroup, ma_pan_mode panMode)
{
    ma_sound_set_pan_mode(pGroup, panMode);
}

MA_API void ma_sound_group_set_pitch(ma_sound_group* pGroup, float pitch)
{
    ma_sound_set_pitch(pGroup, pitch);
}

MA_API void ma_sound_group_set_spatialization_enabled(ma_sound_group* pGroup, ma_bool32 enabled)
{
    ma_sound_set_spatialization_enabled(pGroup, enabled);
}

MA_API void ma_sound_group_set_pinned_listener_index(ma_sound_group* pGroup, ma_uint8 listenerIndex)
{
    ma_sound_set_pinned_listener_index(pGroup, listenerIndex);
}

MA_API ma_uint8 ma_sound_group_get_pinned_listener_index(const ma_sound_group* pGroup)
{
    return ma_sound_get_pinned_listener_index(pGroup);
}

MA_API void ma_sound_group_set_position(ma_sound_group* pGroup, float x, float y, float z)
{
    ma_sound_set_position(pGroup, x, y, z);
}

MA_API ma_vec3f ma_sound_group_get_position(const ma_sound_group* pGroup)
{
    return ma_sound_get_position(pGroup);
}

MA_API void ma_sound_group_set_direction(ma_sound_group* pGroup, float x, float y, float z)
{
    ma_sound_set_direction(pGroup, x, y, z);
}

MA_API ma_vec3f ma_sound_group_get_direction(const ma_sound_group* pGroup)
{
    return ma_sound_get_direction(pGroup);
}

MA_API void ma_sound_group_set_velocity(ma_sound_group* pGroup, float x, float y, float z)
{
    ma_sound_set_velocity(pGroup, x, y, z);
}

MA_API ma_vec3f ma_sound_group_get_velocity(const ma_sound_group* pGroup)
{
    return ma_sound_get_velocity(pGroup);
}

MA_API void ma_sound_group_set_attenuation_model(ma_sound_group* pGroup, ma_attenuation_model attenuationModel)
{
    ma_sound_set_attenuation_model(pGroup, attenuationModel);
}

MA_API ma_attenuation_model ma_sound_group_get_attenuation_model(const ma_sound_group* pGroup)
{
    return ma_sound_get_attenuation_model(pGroup);
}

MA_API void ma_sound_group_set_positioning(ma_sound_group* pGroup, ma_positioning positioning)
{
    ma_sound_set_positioning(pGroup, positioning);
}

MA_API ma_positioning ma_sound_group_get_positioning(const ma_sound_group* pGroup)
{
    return ma_sound_get_positioning(pGroup);
}

MA_API void ma_sound_group_set_rolloff(ma_sound_group* pGroup, float rolloff)
{
    ma_sound_set_rolloff(pGroup, rolloff);
}

MA_API float ma_sound_group_get_rolloff(const ma_sound_group* pGroup)
{
    return ma_sound_get_rolloff(pGroup);
}

MA_API void ma_sound_group_set_min_gain(ma_sound_group* pGroup, float minGain)
{
    ma_sound_set_min_gain(pGroup, minGain);
}

MA_API float ma_sound_group_get_min_gain(const ma_sound_group* pGroup)
{
    return ma_sound_get_min_gain(pGroup);
}

MA_API void ma_sound_group_set_max_gain(ma_sound_group* pGroup, float maxGain)
{
    ma_sound_set_max_gain(pGroup, maxGain);
}

MA_API float ma_sound_group_get_max_gain(const ma_sound_group* pGroup)
{
    return ma_sound_get_max_gain(pGroup);
}

MA_API void ma_sound_group_set_min_distance(ma_sound_group* pGroup, float minDistance)
{
    ma_sound_set_min_distance(pGroup, minDistance);
}

MA_API float ma_sound_group_get_min_distance(const ma_sound_group* pGroup)
{
    return ma_sound_get_min_distance(pGroup);
}

MA_API void ma_sound_group_set_max_distance(ma_sound_group* pGroup, float maxDistance)
{
    ma_sound_set_max_distance(pGroup, maxDistance);
}

MA_API float ma_sound_group_get_max_distance(const ma_sound_group* pGroup)
{
    return ma_sound_get_max_distance(pGroup);
}

MA_API void ma_sound_group_set_cone(ma_sound_group* pGroup, float innerAngleInRadians, float outerAngleInRadians, float outerGain)
{
    ma_sound_set_cone(pGroup, innerAngleInRadians, outerAngleInRadians, outerGain);
}

MA_API void ma_sound_group_get_cone(const ma_sound_group* pGroup, float* pInnerAngleInRadians, float* pOuterAngleInRadians, float* pOuterGain)
{
    ma_sound_get_cone(pGroup, pInnerAngleInRadians, pOuterAngleInRadians, pOuterGain);
}

MA_API void ma_sound_group_set_doppler_factor(ma_sound_group* pGroup, float dopplerFactor)
{
    ma_sound_set_doppler_factor(pGroup, dopplerFactor);
}

MA_API float ma_sound_group_get_doppler_factor(const ma_sound_group* pGroup)
{
    return ma_sound_get_doppler_factor(pGroup);
}

MA_API void ma_sound_group_set_fade_in_pcm_frames(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInFrames)
{
    ma_sound_set_fade_in_pcm_frames(pGroup, volumeBeg, volumeEnd, fadeLengthInFrames);
}

MA_API void ma_sound_group_set_fade_in_milliseconds(ma_sound_group* pGroup, float volumeBeg, float volumeEnd, ma_uint64 fadeLengthInMilliseconds)
{
    ma_sound_set_fade_in_milliseconds(pGroup, volumeBeg, volumeEnd, fadeLengthInMilliseconds);
}

MA_API float ma_sound_group_get_current_fade_volume(ma_sound_group* pGroup)
{
    return ma_sound_get_current_fade_volume(pGroup);
}

MA_API void ma_sound_group_set_start_time_in_pcm_frames(ma_sound_group* pGroup, ma_uint64 absoluteGlobalTimeInFrames)
{
    ma_sound_set_start_time_in_pcm_frames(pGroup, absoluteGlobalTimeInFrames);
}

MA_API void ma_sound_group_set_start_time_in_milliseconds(ma_sound_group* pGroup, ma_uint64 absoluteGlobalTimeInMilliseconds)
{
    ma_sound_set_start_time_in_milliseconds(pGroup, absoluteGlobalTimeInMilliseconds);
}

MA_API void ma_sound_group_set_stop_time_in_pcm_frames(ma_sound_group* pGroup, ma_uint64 absoluteGlobalTimeInFrames)
{
    ma_sound_set_stop_time_in_pcm_frames(pGroup, absoluteGlobalTimeInFrames);
}

MA_API void ma_sound_group_set_stop_time_in_milliseconds(ma_sound_group* pGroup, ma_uint64 absoluteGlobalTimeInMilliseconds)
{
    ma_sound_set_stop_time_in_milliseconds(pGroup, absoluteGlobalTimeInMilliseconds);
}

MA_API ma_bool32 ma_sound_group_is_playing(const ma_sound_group* pGroup)
{
    return ma_sound_is_playing(pGroup);
}

MA_API ma_uint64 ma_sound_group_get_time_in_pcm_frames(const ma_sound_group* pGroup)
{
    return ma_sound_get_time_in_pcm_frames(pGroup);
}

#endif
