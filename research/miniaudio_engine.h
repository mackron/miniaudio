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


/* Must never exceed 254. */
#ifndef MA_MAX_NODE_BUS_COUNT
#define MA_MAX_NODE_BUS_COUNT       254
#endif

/* Used internally by miniaudio for memory management. Must never exceed MA_MAX_NODE_BUS_COUNT. */
#ifndef MA_MAX_NODE_LOCAL_BUS_COUNT
#define MA_MAX_NODE_LOCAL_BUS_COUNT 2
#endif

/* Use this when the bus count is determined by the node instance rather than the vtable. */
#define MA_NODE_BUS_COUNT_UNKNOWN   255

typedef struct ma_node_graph ma_node_graph;
typedef void ma_node;


/* Node flags. */
#define MA_NODE_FLAG_PASSTHROUGH                0x00000001
#define MA_NODE_FLAG_CONTINUOUS_PROCESSING      0x00000002
#define MA_NODE_FLAG_ALLOW_NULL_INPUT           0x00000004
#define MA_NODE_FLAG_DIFFERENT_PROCESSING_RATES 0x00000008


/* The playback state of a node. Either started or stopped. */
typedef enum
{
    ma_node_state_started = 0,
    ma_node_state_stopped = 1
} ma_node_state;


typedef struct
{
    /*
    Extended processing callback. This callback is used for effects that process input and output
    at different rates (i.e. they perform resampling). This is similar to the simple version, only
    they take two sepate frame counts: one for input, and one for output.

    On input, `pFrameCountOut` is equal to the capacity of the output buffer for each bus, whereas
    `pFrameCountIn` will be equal to the number of PCM frames in each of the buffers in `ppFramesIn`.

    On output, set `pFrameCountOut` to the number of PCM frames that were actually output and set
    `pFrameCountIn` to the number of input frames that were consumed.
    */
    void (* onProcess)(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut);

    /*
    A callback for retrieving the number of a input frames that are required to output the
    specified number of output frames. You would only want to implement this when the node performs
    resampling. This is optional, even for nodes that perform resampling, but it does offer a
    small reduction in latency as it allows miniaudio to calculate the exact number of input frames
    to read at a time instead of having to estimate.
    */
    ma_result (* onGetRequiredInputFrameCount)(ma_node* pNode, ma_uint32 outputFrameCount, ma_uint32* pInputFrameCount);

    /*
    The number of input buses. This is how many sub-buffers will be contained in the `ppFramesIn`
    parameters of the callbacks above.
    */
    ma_uint8 inputBusCount;

    /*
    The number of output buses. This is how many sub-buffers will be contained in the `ppFramesOut`
    parameters of the callbacks above.
    */
    ma_uint8 outputBusCount;

    /*
    Flags describing characteristics of the node. This is currently just a placeholder for some
    ideas for later on.
    */
    ma_uint32 flags;
} ma_node_vtable;

typedef struct
{
    const ma_node_vtable* vtable;       /* Should never be null. Initialization of the node will fail if so. */
    ma_node_state initialState;         /* Defaults to ma_node_state_started. */
    ma_uint32 inputBusCount;            /* Only used if the vtable specifies an input bus count of `MA_NODE_BUS_COUNT_UNKNOWN`, otherwise must be set to `MA_NODE_BUS_COUNT_UNKNOWN` (default). */
    ma_uint32 outputBusCount;           /* Only used if the vtable specifies an output bus count of `MA_NODE_BUS_COUNT_UNKNOWN`, otherwise  be set to `MA_NODE_BUS_COUNT_UNKNOWN` (default). */
    const ma_uint32* pInputChannels;    /* The number of elements are determined by the input bus count as determined by the vtable, or `inputBusCount` if the vtable specifies `MA_NODE_BUS_COUNT_UNKNOWN`. */
    const ma_uint32* pOutputChannels;   /* The number of elements are determined by the output bus count as determined by the vtable, or `outputBusCount` if the vtable specifies `MA_NODE_BUS_COUNT_UNKNOWN`. */
} ma_node_config;

MA_API ma_node_config ma_node_config_init(void);


/*
A node has multiple output buses. An output bus is attached to an input bus as an item in a linked
list. Think of the input bus as a linked list, with the output bus being an item in that list.
*/
#define MA_NODE_OUTPUT_BUS_FLAG_HAS_READ    0x01    /* Whether or not this bus ready to read more data. Only used on nodes with multiple output buses. */

typedef struct ma_node_output_bus ma_node_output_bus;
struct ma_node_output_bus
{
    /* Immutable. */
    ma_node* pNode;                             /* The node that owns this output bus. The input node. Will be null for dummy head and tail nodes. */
    ma_uint8 outputBusIndex;                    /* The index of the output bus on pNode that this output bus represents. */
    ma_uint8 channels;                          /* The number of channels in the audio stream for this bus. */

    /* Mutable via multiple threads. Must be used atomically. The weird ordering here is for packing reasons. */
    MA_ATOMIC ma_uint8 inputNodeInputBusIndex;  /* The index of the input bus on the input. Required for detaching. */
    MA_ATOMIC ma_uint8 flags;                   /* Some state flags for tracking the read state of the output buffer. */
    MA_ATOMIC ma_uint16 refCount;               /* Reference count for some thread-safety when detaching. */
    MA_ATOMIC ma_bool8 isAttached;              /* This is used to prevent iteration of nodes that are in the middle of being detached. Used for thread safety. */
    MA_ATOMIC ma_spinlock lock;                 /* Unfortunate lock, but significantly simplifies the implementation. Required for thread-safe attaching and detaching. */
    MA_ATOMIC float volume;                     /* Linear. */
    MA_ATOMIC ma_node_output_bus* pNext;        /* If null, it's the tail node or detached. */
    MA_ATOMIC ma_node_output_bus* pPrev;        /* If null, it's the head node or detached. */
    MA_ATOMIC ma_node* pInputNode;              /* The node that this output bus is attached to. Required for detaching. */    
};

/*
A node has multiple input buses. The output buses of a node are connecting to the input busses of
another. An input bus is essentially just a linked list of output buses.
*/
typedef struct ma_node_input_bus ma_node_input_bus;
struct ma_node_input_bus
{
    /* Mutable via multiple threads. */
    ma_node_output_bus head;            /* Dummy head node for simplifying some lock-free thread-safety stuff. */
    MA_ATOMIC ma_uint16 nextCounter;    /* This is used to determine whether or not the input bus is finding the next node in the list. Used for thread safety when detaching output buses. */
    MA_ATOMIC ma_spinlock lock;         /* Unfortunate lock, but significantly simplifies the implementation. Required for thread-safe attaching and detaching. */

    /* Set once at startup. */
    ma_uint8 channels;                  /* The number of channels in the audio stream for this bus. */
};


typedef struct ma_node_base ma_node_base;
struct ma_node_base
{
    /* These variables are set once at startup. */
    ma_node_graph* pNodeGraph;  /* The graph this node belongs to. */
    const ma_node_vtable* vtable;
    float* pCachedData;                     /* Allocated on the heap. Fixed size. Needs to be stored on the heap because reading from output buses is done in separate function calls. */
    ma_uint16 cachedDataCapInFramesPerBus;  /* The capacity of the input data cache in frames, per bus. */

    /* These variables are read and written only from the audio thread. */
    ma_uint16 cachedFrameCountOut;
    ma_uint16 cachedFrameCountIn;
    ma_uint16 consumedFrameCountIn;
    
    /* These variables are read and written between different threads. */
    MA_ATOMIC ma_node_state state;          /* When set to stopped, nothing will be read, regardless of the times in stateTimes. */
    MA_ATOMIC ma_uint64 stateTimes[2];      /* Indexed by ma_node_state. Specifies the time based on the global clock that a node should be considered to be in the relevant state. */
    MA_ATOMIC ma_uint64 localTime;          /* The node's local clock. This is just a running sum of the number of output frames that have been processed. Can be modified by any thread with `ma_node_set_time()`. */
    ma_uint32 inputBusCount;
    ma_uint32 outputBusCount;
    ma_node_input_bus* pInputBuses;
    ma_node_output_bus* pOutputBuses;

    /* Memory management. */
    ma_node_input_bus _inputBuses[MA_MAX_NODE_LOCAL_BUS_COUNT];
    ma_node_output_bus _outputBuses[MA_MAX_NODE_LOCAL_BUS_COUNT];
    void* _pHeap;   /* A heap allocation for internal use only. pInputBuses and/or pOutputBuses will point to this if the bus count exceeds MA_MAX_NODE_LOCAL_BUS_COUNT. */
    ma_bool32 _ownsHeap;    /* If set to true, the node owns the heap allocation and _pHeap will be freed in ma_node_uninit(). */
};

MA_API ma_result ma_node_get_heap_size(const ma_node_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_node_init_preallocated(ma_node_graph* pNodeGraph, const ma_node_config* pConfig, void* pHeap, ma_node* pNode);
MA_API ma_result ma_node_init(ma_node_graph* pNodeGraph, const ma_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_node* pNode);
MA_API void ma_node_uninit(ma_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_node_graph* ma_node_get_node_graph(const ma_node* pNode);
MA_API ma_uint32 ma_node_get_input_bus_count(const ma_node* pNode);
MA_API ma_uint32 ma_node_get_output_bus_count(const ma_node* pNode);
MA_API ma_uint32 ma_node_get_input_channels(const ma_node* pNode, ma_uint32 inputBusIndex);
MA_API ma_uint32 ma_node_get_output_channels(const ma_node* pNode, ma_uint32 outputBusIndex);
MA_API ma_result ma_node_attach_output_bus(ma_node* pNode, ma_uint32 outputBusIndex, ma_node* pOtherNode, ma_uint32 otherNodeInputBusIndex);
MA_API ma_result ma_node_detach_output_bus(ma_node* pNode, ma_uint32 outputBusIndex);
MA_API ma_result ma_node_detach_all_output_buses(ma_node* pNode);
MA_API ma_result ma_node_set_output_bus_volume(ma_node* pNode, ma_uint32 outputBusIndex, float volume);
MA_API float ma_node_get_output_bus_volume(const ma_node* pNode, ma_uint32 outputBusIndex);
MA_API ma_result ma_node_set_state(ma_node* pNode, ma_node_state state);
MA_API ma_node_state ma_node_get_state(const ma_node* pNode);
MA_API ma_result ma_node_set_state_time(ma_node* pNode, ma_node_state state, ma_uint64 globalTime);
MA_API ma_uint64 ma_node_get_state_time(const ma_node* pNode, ma_node_state state);
MA_API ma_node_state ma_node_get_state_by_time(const ma_node* pNode, ma_uint64 globalTime);
MA_API ma_node_state ma_node_get_state_by_time_range(const ma_node* pNode, ma_uint64 globalTimeBeg, ma_uint64 globalTimeEnd);
MA_API ma_uint64 ma_node_get_time(const ma_node* pNode);
MA_API ma_result ma_node_set_time(ma_node* pNode, ma_uint64 localTime);


typedef struct
{
    ma_uint32 channels;
} ma_node_graph_config;

MA_API ma_node_graph_config ma_node_graph_config_init(ma_uint32 channels);


struct ma_node_graph
{
    /* Immutable. */
    ma_node_base endpoint;              /* Special node that all nodes eventually connect to. Data is read from this node in ma_node_graph_read_pcm_frames(). */

    /* Read and written by multiple threads. */
    MA_ATOMIC ma_bool8 isReading;
};

MA_API ma_result ma_node_graph_init(const ma_node_graph_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_node_graph* pNodeGraph);
MA_API void ma_node_graph_uninit(ma_node_graph* pNodeGraph, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_node* ma_node_graph_get_endpoint(ma_node_graph* pNodeGraph);
MA_API ma_result ma_node_graph_read_pcm_frames(ma_node_graph* pNodeGraph, void* pFramesOut, ma_uint32 frameCount, ma_uint32* pFramesRead);
MA_API ma_uint32 ma_node_graph_get_channels(const ma_node_graph* pNodeGraph);
MA_API ma_uint64 ma_node_graph_get_time(const ma_node_graph* pNodeGraph);
MA_API ma_result ma_node_graph_set_time(ma_node_graph* pNodeGraph, ma_uint64 globalTime);



/* Data source node. 0 input buses, 1 output bus. Used for reading from a data source. */
typedef struct
{
    ma_node_config nodeConfig;
    ma_data_source* pDataSource;
    ma_bool32 looping;  /* Can be changed after initialization with ma_data_source_node_set_looping(). */
} ma_data_source_node_config;

MA_API ma_data_source_node_config ma_data_source_node_config_init(ma_data_source* pDataSource, ma_bool32 looping);


typedef struct
{
    ma_node_base base;
    ma_data_source* pDataSource;
    MA_ATOMIC ma_bool32 looping;  /* This can be modified and read across different threads. Must be used atomically. */
} ma_data_source_node;

MA_API ma_result ma_data_source_node_init(ma_node_graph* pNodeGraph, const ma_data_source_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source_node* pDataSourceNode);
MA_API void ma_data_source_node_uninit(ma_data_source_node* pDataSourceNode, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_data_source_node_set_looping(ma_data_source_node* pDataSourceNode, ma_bool32 looping);
MA_API ma_bool32 ma_data_source_node_is_looping(ma_data_source_node* pDataSourceNode);


/* Splitter Node. 1 input, 2 outputs. Used for splitting/copying a stream so it can be as input into two separate output nodes. */
typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channels;
} ma_splitter_node_config;

MA_API ma_splitter_node_config ma_splitter_node_config_init(ma_uint32 channels);


typedef struct
{
    ma_node_base base;
} ma_splitter_node;

MA_API ma_result ma_splitter_node_init(ma_node_graph* pNodeGraph, const ma_splitter_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_splitter_node* pSplitterNode);
MA_API void ma_splitter_node_uninit(ma_splitter_node* pSplitterNode, const ma_allocation_callbacks* pAllocationCallbacks);




/*
Resource Manager Data Source Flags
==================================
The flags below are used for controlling how the resource manager should handle the loading and caching of data sources.
*/
#define MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM     0x00000001  /* When set, does not load the entire data source in memory. Disk I/O will happen on job threads. */
#define MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE     0x00000002  /* Decode data before storing in memory. When set, decoding is done at the resource manager level rather than the mixing thread. Results in faster mixing, but higher memory usage. */
#define MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC      0x00000004  /* When set, the resource manager will load the data source asynchronously. */
#define MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT  0x00000008  /* When set, waits for initialization of the underlying data source before returning from ma_resource_manager_data_source_init(). */


typedef struct ma_resource_manager                  ma_resource_manager;
typedef struct ma_resource_manager_data_buffer_node ma_resource_manager_data_buffer_node;
typedef struct ma_resource_manager_data_buffer      ma_resource_manager_data_buffer;
typedef struct ma_resource_manager_data_stream      ma_resource_manager_data_stream;
typedef struct ma_resource_manager_data_source      ma_resource_manager_data_source;



#ifndef MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY
#define MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY  1024
#endif

#define MA_RESOURCE_MANAGER_JOB_QUIT                    0x00000000
#define MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER_NODE   0x00000001
#define MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER_NODE   0x00000002
#define MA_RESOURCE_MANAGER_JOB_PAGE_DATA_BUFFER_NODE   0x00000003
#define MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER        0x00000004
#define MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER        0x00000005
#define MA_RESOURCE_MANAGER_JOB_LOAD_DATA_STREAM        0x00000006
#define MA_RESOURCE_MANAGER_JOB_FREE_DATA_STREAM        0x00000007
#define MA_RESOURCE_MANAGER_JOB_PAGE_DATA_STREAM        0x00000008
#define MA_RESOURCE_MANAGER_JOB_SEEK_DATA_STREAM        0x00000009
#define MA_RESOURCE_MANAGER_JOB_CUSTOM                  0x00000100  /* Number your custom job codes as (MA_RESOURCE_MANAGER_JOB_CUSTOM + 0), (MA_RESOURCE_MANAGER_JOB_CUSTOM + 1), etc. */



/*
Notification callback for asynchronous operations.
*/
typedef void ma_async_notification;

typedef struct
{
    void (* onSignal)(ma_async_notification* pNotification);
} ma_async_notification_callbacks;

MA_API ma_result ma_async_notification_signal(ma_async_notification* pNotification);


/*
Simple polling notification.

This just sets a variable when the notification has been signalled which is then polled with ma_async_notification_poll_is_signalled()
*/
typedef struct
{
    ma_async_notification_callbacks cb;
    ma_bool32 signalled;
} ma_async_notification_poll;

MA_API ma_result ma_async_notification_poll_init(ma_async_notification_poll* pNotificationPoll);
MA_API ma_bool32 ma_async_notification_poll_is_signalled(const ma_async_notification_poll* pNotificationPoll);


/*
Event Notification
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


/*
Pipeline notifications used by the resource manager. Made up of both an async notification and a fence, both of which are optionally.
*/
typedef struct
{
    ma_async_notification* pNotification;
    ma_fence* pFence;
} ma_resource_manager_pipeline_stage_notification;

typedef struct
{
    ma_resource_manager_pipeline_stage_notification init;    /* Initialization of the decoder. */
    ma_resource_manager_pipeline_stage_notification done;    /* Decoding fully completed. */
} ma_resource_manager_pipeline_notifications;

MA_API ma_resource_manager_pipeline_notifications ma_resource_manager_pipeline_notifications_init(void);


typedef struct
{
    union
    {
        struct
        {
            ma_uint16 code;
            ma_uint16 slot;
            ma_uint32 refcount;
        } breakup;
        ma_uint64 allocation;
    } toc;  /* 8 bytes. We encode the job code into the slot allocation data to save space. */
    ma_uint64 next;     /* refcount + slot for the next item. Does not include the job code. */
    ma_uint32 order;    /* Execution order. Used to create a data dependency and ensure a job is executed in order. Usage is contextual depending on the job type. */

    union
    {
        /* Resource Managemer Jobs */
        struct
        {
            ma_resource_manager_data_buffer_node* pDataBufferNode;
            char* pFilePath;
            wchar_t* pFilePathW;
            ma_bool32 decode;                               /* When set to true, the data buffer will be decoded. Otherwise it'll be encoded and will use a decoder for the connector. */
            ma_async_notification* pInitNotification;       /* Signalled when the data buffer has been initialized and the format/channels/rate can be retrieved. */
            ma_async_notification* pDoneNotification;       /* Signalled when the data buffer has been fully decoded. Will be passed through to MA_RESOURCE_MANAGER_JOB_PAGE_DATA_BUFFER_NODE when decoding. */
            ma_fence* pInitFence;                           /* Released when initialization of the decoder is complete. */
            ma_fence* pDoneFence;                           /* Released if initialization of the decoder fails. Passed through to PAGE_DATA_BUFFER_NODE untouched if init is successful. */
        } loadDataBufferNode;
        struct
        {
            ma_resource_manager_data_buffer_node* pDataBufferNode;
            ma_async_notification* pDoneNotification;
            ma_fence* pDoneFence;
        } freeDataBufferNode;
        struct
        {
            ma_resource_manager_data_buffer_node* pDataBufferNode;
            ma_decoder* pDecoder;
            ma_async_notification* pDoneNotification;       /* Signalled when the data buffer has been fully decoded. */
            ma_fence* pDoneFence;                           /* Passed through from LOAD_DATA_BUFFER_NODE and released when the data buffer completes decoding or an error occurs. */
        } pageDataBufferNode;

        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            ma_async_notification* pInitNotification;       /* Signalled when the data buffer has been initialized and the format/channels/rate can be retrieved. */
            ma_async_notification* pDoneNotification;       /* Signalled when the data buffer has been fully decoded. */
            ma_fence* pInitFence;                           /* Released when the data buffer has been initialized and the format/channels/rate can be retrieved. */
            ma_fence* pDoneFence;                           /* Released when the data buffer has been fully decoded. */
        } loadDataBuffer;
        struct
        {
            ma_resource_manager_data_buffer* pDataBuffer;
            ma_async_notification* pDoneNotification;
            ma_fence* pDoneFence;
        } freeDataBuffer;

        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            char* pFilePath;                            /* Allocated when the job is posted, freed by the job thread after loading. */
            wchar_t* pFilePathW;                        /* ^ As above ^. Only used if pFilePath is NULL. */
            ma_async_notification* pInitNotification;   /* Signalled after the first two pages have been decoded and frames can be read from the stream. */
            ma_fence* pInitFence;
        } loadDataStream;
        struct
        {
            ma_resource_manager_data_stream* pDataStream;
            ma_async_notification* pDoneNotification;
            ma_fence* pDoneFence;
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
} ma_resource_manager_job;

MA_API ma_resource_manager_job ma_resource_manager_job_init(ma_uint16 code);


/*
When set, ma_resource_manager_job_queue_next() will not wait and no semaphore will be signaled in
ma_resource_manager_job_queue_post(). ma_resource_manager_job_queue_next() will return MA_NO_DATA_AVAILABLE if nothing is available.

This flag should always be used for platforms that do not support multithreading.
*/
#define MA_RESOURCE_MANAGER_JOB_QUEUE_FLAG_NON_BLOCKING  0x00000001

typedef struct
{
    ma_uint32 flags;
    ma_uint32 capacity; /* The maximum number of jobs that can fit in the queue at a time. */
} ma_resource_manager_job_queue_config;

MA_API ma_resource_manager_job_queue_config ma_resource_manager_job_queue_config_init(ma_uint32 flags, ma_uint32 capacity);


typedef struct
{
    ma_uint32 flags;    /* Flags passed in at initialization time. */
    ma_uint32 capacity; /* The maximum number of jobs that can fit in the queue at a time. Set by the config. */
    ma_uint64 head;     /* The first item in the list. Required for removing from the top of the list. */
    ma_uint64 tail;     /* The last item in the list. Required for appending to the end of the list. */
    ma_semaphore sem;   /* Only used when MA_RESOURCE_MANAGER_JOB_QUEUE_FLAG_NON_BLOCKING is unset. */
    ma_slot_allocator allocator;
    ma_resource_manager_job* pJobs;

    /* Memory management. */
    void* _pHeap;
    ma_bool32 _ownsHeap;
} ma_resource_manager_job_queue;

MA_API ma_result ma_resource_manager_job_queue_get_heap_size(const ma_resource_manager_job_queue_config* pConfig, size_t* pHeapSizeInBytes);
MA_API ma_result ma_resource_manager_job_queue_init_preallocated(const ma_resource_manager_job_queue_config* pConfig, void* pHeap, ma_resource_manager_job_queue* pQueue);
MA_API ma_result ma_resource_manager_job_queue_init(const ma_resource_manager_job_queue_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_resource_manager_job_queue* pQueue);
MA_API void ma_resource_manager_job_queue_uninit(ma_resource_manager_job_queue* pQueue, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_resource_manager_job_queue_post(ma_resource_manager_job_queue* pQueue, const ma_resource_manager_job* pJob);
MA_API ma_result ma_resource_manager_job_queue_next(ma_resource_manager_job_queue* pQueue, ma_resource_manager_job* pJob); /* Returns MA_CANCELLED if the next job is a quit job. */


/* Maximum job thread count will be restricted to this, but this may be removed later and replaced with a heap allocation thereby removing any limitation. */
#ifndef MA_RESOURCE_MANAGER_MAX_JOB_THREAD_COUNT
#define MA_RESOURCE_MANAGER_MAX_JOB_THREAD_COUNT    64
#endif

/* Indicates ma_resource_manager_next_job() should not block. Only valid when the job thread count is 0. */
#define MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING   0x00000001

/* Disables any kind of multithreading. Implicitly enables MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING. */
#define MA_RESOURCE_MANAGER_FLAG_NO_THREADING   0x00000002


typedef enum
{
    ma_resource_manager_data_supply_type_unknown = 0,   /* Used for determining whether or the data supply has been initialized. */
    ma_resource_manager_data_supply_type_encoded,       /* Data supply is an encoded buffer. Connector is ma_decoder. */
    ma_resource_manager_data_supply_type_decoded,       /* Data supply is a decoded buffer. Connector is ma_audio_buffer. */
    ma_resource_manager_data_supply_type_decoded_paged  /* Data supply is a linked list of decoded buffers. Connector is ma_paged_audio_buffer. */
} ma_resource_manager_data_supply_type;

typedef struct
{
    MA_ATOMIC ma_resource_manager_data_supply_type type;  /* Read and written from different threads so needs to be accessed atomically. */
    union
    {
        struct
        {
            const void* pData;
            size_t sizeInBytes;
        } encoded;
        struct
        {
            const void* pData;
            ma_uint64 totalFrameCount;
            ma_uint64 decodedFrameCount;
            ma_format format;
            ma_uint32 channels;
            ma_uint32 sampleRate;
        } decoded;
        struct
        {
            ma_paged_audio_buffer_data data;
            ma_uint64 decodedFrameCount;
            ma_uint32 sampleRate;
        } decodedPaged;
    };
} ma_resource_manager_data_supply;

struct ma_resource_manager_data_buffer_node
{
    ma_uint32 hashedName32;                         /* The hashed name. This is the key. */
    ma_uint32 refCount;
    MA_ATOMIC ma_result result;                     /* Result from asynchronous loading. When loading set to MA_BUSY. When fully loaded set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. */
    ma_uint32 executionCounter;                     /* For allocating execution orders for jobs. */
    ma_uint32 executionPointer;                     /* For managing the order of execution for asynchronous jobs relating to this object. Incremented as jobs complete processing. */
    ma_bool32 isDataOwnedByResourceManager;         /* Set to true when the underlying data buffer was allocated the resource manager. Set to false if it is owned by the application (via ma_resource_manager_register_*()). */
    ma_resource_manager_data_supply data;
    ma_resource_manager_data_buffer_node* pParent;
    ma_resource_manager_data_buffer_node* pChildLo;
    ma_resource_manager_data_buffer_node* pChildHi;
};

struct ma_resource_manager_data_buffer
{
    ma_data_source_base ds;                         /* Base data source. A data buffer is a data source. */
    ma_resource_manager* pResourceManager;          /* A pointer to the resource manager that owns this buffer. */
    ma_resource_manager_data_buffer_node* pNode;    /* The data node. This is reference counted and is what supplies the data. */
    ma_uint32 flags;                                /* The flags that were passed used to initialize the buffer. */
    ma_uint32 executionCounter;                     /* For allocating execution orders for jobs. */
    ma_uint32 executionPointer;                     /* For managing the order of execution for asynchronous jobs relating to this object. Incremented as jobs complete processing. */
    ma_uint64 seekTargetInPCMFrames;                /* Only updated by the public API. Never written nor read from the job thread. */
    ma_bool32 seekToCursorOnNextRead;               /* On the next read we need to seek to the frame cursor. */
    MA_ATOMIC ma_result result;                     /* Keeps track of a result of decoding. Set to MA_BUSY while the buffer is still loading. Set to MA_SUCCESS when loading is finished successfully. Otherwise set to some other code. */
    MA_ATOMIC ma_bool32 isLooping;                  /* Can be read and written by different threads at the same time. Must be used atomically. */
    ma_bool32 isConnectorInitialized;               /* Used for asynchronous loading to ensure we don't try to initialize the connector multiple times while waiting for the node to fully load. */
    union
    {
        ma_decoder decoder;                 /* Supply type is ma_resource_manager_data_supply_type_encoded */
        ma_audio_buffer buffer;             /* Supply type is ma_resource_manager_data_supply_type_decoded */
        ma_paged_audio_buffer pagedBuffer;  /* Supply type is ma_resource_manager_data_supply_type_decoded_paged */
    } connector;    /* Connects this object to the node's data supply. */
};

struct ma_resource_manager_data_stream
{
    ma_data_source_base ds;                 /* Base data source. A data stream is a data source. */
    ma_resource_manager* pResourceManager;  /* A pointer to the resource manager that owns this data stream. */
    ma_uint32 flags;                        /* The flags that were passed used to initialize the stream. */
    ma_decoder decoder;                     /* Used for filling pages with data. This is only ever accessed by the job thread. The public API should never touch this. */
    ma_bool32 isDecoderInitialized;         /* Required for determining whether or not the decoder should be uninitialized in MA_RESOURCE_MANAGER_JOB_FREE_DATA_STREAM. */
    ma_uint64 totalLengthInPCMFrames;       /* This is calculated when first loaded by the MA_RESOURCE_MANAGER_JOB_LOAD_DATA_STREAM. */
    ma_uint32 relativeCursor;               /* The playback cursor, relative to the current page. Only ever accessed by the public API. Never accessed by the job thread. */
    ma_uint64 absoluteCursor;               /* The playback cursor, in absolute position starting from the start of the file. */
    ma_uint32 currentPageIndex;             /* Toggles between 0 and 1. Index 0 is the first half of pPageData. Index 1 is the second half. Only ever accessed by the public API. Never accessed by the job thread. */
    ma_uint32 executionCounter;             /* For allocating execution orders for jobs. */
    ma_uint32 executionPointer;             /* For managing the order of execution for asynchronous jobs relating to this object. Incremented as jobs complete processing. */

    /* Written by the public API, read by the job thread. */
    MA_ATOMIC ma_bool32 isLooping;          /* Whether or not the stream is looping. It's important to set the looping flag at the data stream level for smooth loop transitions. */

    /* Written by the job thread, read by the public API. */
    void* pPageData;                        /* Buffer containing the decoded data of each page. Allocated once at initialization time. */
    MA_ATOMIC ma_uint32 pageFrameCount[2];  /* The number of valid PCM frames in each page. Used to determine the last valid frame. */

    /* Written and read by both the public API and the job thread. These must be atomic. */
    MA_ATOMIC ma_result result;             /* Result from asynchronous loading. When loading set to MA_BUSY. When initialized set to MA_SUCCESS. When deleting set to MA_UNAVAILABLE. If an error occurs when loading, set to an error code. */
    MA_ATOMIC ma_bool32 isDecoderAtEnd;     /* Whether or not the decoder has reached the end. */
    MA_ATOMIC ma_bool32 isPageValid[2];     /* Booleans to indicate whether or not a page is valid. Set to false by the public API, set to true by the job thread. Set to false as the pages are consumed, true when they are filled. */
    MA_ATOMIC ma_bool32 seekCounter;        /* When 0, no seeking is being performed. When > 0, a seek is being performed and reading should be delayed with MA_BUSY. */
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
    ma_log* pLog;
    ma_format decodedFormat;        /* The decoded format to use. Set to ma_format_unknown (default) to use the file's native format. */
    ma_uint32 decodedChannels;      /* The decoded channel count to use. Set to 0 (default) to use the file's native channel count. */
    ma_uint32 decodedSampleRate;    /* the decoded sample rate to use. Set to 0 (default) to use the file's native sample rate. */
    ma_uint32 jobThreadCount;       /* Set to 0 if you want to self-manage your job threads. Defaults to 1. */
    ma_uint32 jobQueueCapacity;     /* The maximum number of jobs that can fit in the queue at a time. Defaults to MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY. Cannot be zero. */
    ma_uint32 flags;
    ma_vfs* pVFS;                   /* Can be NULL in which case defaults will be used. */
    ma_decoding_backend_vtable** ppCustomDecodingBackendVTables;
    ma_uint32 customDecodingBackendCount;
    void* pCustomDecodingBackendUserData;
} ma_resource_manager_config;

MA_API ma_resource_manager_config ma_resource_manager_config_init(void);

struct ma_resource_manager
{
    ma_resource_manager_config config;
    ma_resource_manager_data_buffer_node* pRootDataBufferNode;      /* The root buffer in the binary tree. */
    ma_mutex dataBufferBSTLock;                                     /* For synchronizing access to the data buffer binary tree. */
    ma_thread jobThreads[MA_RESOURCE_MANAGER_MAX_JOB_THREAD_COUNT]; /* The threads for executing jobs. */
    ma_resource_manager_job_queue jobQueue;                                          /* Lock-free multi-consumer, multi-producer job queue for managing jobs for asynchronous decoding and streaming. */
    ma_default_vfs defaultVFS;                                      /* Only used if a custom VFS is not specified. */
    ma_log log;                                                     /* Only used if no log was specified in the config. */
};

/* Init. */
MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager);
MA_API void ma_resource_manager_uninit(ma_resource_manager* pResourceManager);
MA_API ma_log* ma_resource_manager_get_log(ma_resource_manager* pResourceManager);

/* Registration. */
MA_API ma_result ma_resource_manager_register_file(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags);
MA_API ma_result ma_resource_manager_register_file_w(ma_resource_manager* pResourceManager, const wchar_t* pFilePath, ma_uint32 flags);
MA_API ma_result ma_resource_manager_register_decoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate);  /* Does not copy. Increments the reference count if already exists and returns MA_SUCCESS. */
MA_API ma_result ma_resource_manager_register_decoded_data_w(ma_resource_manager* pResourceManager, const wchar_t* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate);
MA_API ma_result ma_resource_manager_register_encoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, size_t sizeInBytes);    /* Does not copy. Increments the reference count if already exists and returns MA_SUCCESS. */
MA_API ma_result ma_resource_manager_register_encoded_data_w(ma_resource_manager* pResourceManager, const wchar_t* pName, const void* pData, size_t sizeInBytes);
MA_API ma_result ma_resource_manager_unregister_file(ma_resource_manager* pResourceManager, const char* pFilePath);
MA_API ma_result ma_resource_manager_unregister_file_w(ma_resource_manager* pResourceManager, const wchar_t* pFilePath);
MA_API ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName);
MA_API ma_result ma_resource_manager_unregister_data_w(ma_resource_manager* pResourceManager, const wchar_t* pName);

/* Data Buffers. */
MA_API ma_result ma_resource_manager_data_buffer_init(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_init_w(ma_resource_manager* pResourceManager, const wchar_t* pFilePath, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_init_copy(ma_resource_manager* pResourceManager, const ma_resource_manager_data_buffer* pExistingDataBuffer, ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_uninit(ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_read_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_resource_manager_data_buffer_seek_to_pcm_frame(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64 frameIndex);
MA_API ma_result ma_resource_manager_data_buffer_get_data_format(ma_resource_manager_data_buffer* pDataBuffer, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_resource_manager_data_buffer_get_cursor_in_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pCursor);
MA_API ma_result ma_resource_manager_data_buffer_get_length_in_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pLength);
MA_API ma_result ma_resource_manager_data_buffer_result(const ma_resource_manager_data_buffer* pDataBuffer);
MA_API ma_result ma_resource_manager_data_buffer_set_looping(ma_resource_manager_data_buffer* pDataBuffer, ma_bool32 isLooping);
MA_API ma_result ma_resource_manager_data_buffer_get_looping(const ma_resource_manager_data_buffer* pDataBuffer, ma_bool32* pIsLooping);
MA_API ma_result ma_resource_manager_data_buffer_get_available_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pAvailableFrames);

/* Data Streams. */
MA_API ma_result ma_resource_manager_data_stream_init(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_init_w(ma_resource_manager* pResourceManager, const wchar_t* pFilePath, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_uninit(ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_read_pcm_frames(ma_resource_manager_data_stream* pDataStream, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_resource_manager_data_stream_seek_to_pcm_frame(ma_resource_manager_data_stream* pDataStream, ma_uint64 frameIndex);
MA_API ma_result ma_resource_manager_data_stream_get_data_format(ma_resource_manager_data_stream* pDataStream, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_resource_manager_data_stream_get_cursor_in_pcm_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pCursor);
MA_API ma_result ma_resource_manager_data_stream_get_length_in_pcm_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pLength);
MA_API ma_result ma_resource_manager_data_stream_result(const ma_resource_manager_data_stream* pDataStream);
MA_API ma_result ma_resource_manager_data_stream_set_looping(ma_resource_manager_data_stream* pDataStream, ma_bool32 isLooping);
MA_API ma_result ma_resource_manager_data_stream_get_looping(const ma_resource_manager_data_stream* pDataStream, ma_bool32* pIsLooping);
MA_API ma_result ma_resource_manager_data_stream_get_available_frames(ma_resource_manager_data_stream* pDataStream, ma_uint64* pAvailableFrames);

/* Data Sources. */
MA_API ma_result ma_resource_manager_data_source_init(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_init_w(ma_resource_manager* pResourceManager, const wchar_t* pName, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_init_copy(ma_resource_manager* pResourceManager, const ma_resource_manager_data_source* pExistingDataSource, ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_uninit(ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_read_pcm_frames(ma_resource_manager_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_resource_manager_data_source_seek_to_pcm_frame(ma_resource_manager_data_source* pDataSource, ma_uint64 frameIndex);
MA_API ma_result ma_resource_manager_data_source_get_data_format(ma_resource_manager_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_resource_manager_data_source_get_cursor_in_pcm_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pCursor);
MA_API ma_result ma_resource_manager_data_source_get_length_in_pcm_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pLength);
MA_API ma_result ma_resource_manager_data_source_result(const ma_resource_manager_data_source* pDataSource);
MA_API ma_result ma_resource_manager_data_source_set_looping(ma_resource_manager_data_source* pDataSource, ma_bool32 isLooping);
MA_API ma_result ma_resource_manager_data_source_get_looping(const ma_resource_manager_data_source* pDataSource, ma_bool32* pIsLooping);
MA_API ma_result ma_resource_manager_data_source_get_available_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pAvailableFrames);

/* Job management. */
MA_API ma_result ma_resource_manager_post_job(ma_resource_manager* pResourceManager, const ma_resource_manager_job* pJob);
MA_API ma_result ma_resource_manager_post_job_quit(ma_resource_manager* pResourceManager);  /* Helper for posting a quit job. */
MA_API ma_result ma_resource_manager_next_job(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob);
MA_API ma_result ma_resource_manager_process_job(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob);
MA_API ma_result ma_resource_manager_process_next_job(ma_resource_manager* pResourceManager);   /* Returns MA_CANCELLED if a MA_RESOURCE_MANAGER_JOB_QUIT job is found. In non-blocking mode, returns MA_NO_DATA_AVAILABLE if no jobs are available. */


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



/*
Biquad Node
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_biquad_config biquad;
} ma_biquad_node_config;

MA_API ma_biquad_node_config ma_biquad_node_config_init(ma_uint32 channels, float b0, float b1, float b2, float a0, float a1, float a2);


typedef struct
{
    ma_node_base baseNode;
    ma_biquad biquad;
} ma_biquad_node;

MA_API ma_result ma_biquad_node_init(ma_node_graph* pNodeGraph, const ma_biquad_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_biquad_node* pNode);
MA_API ma_result ma_biquad_node_reinit(const ma_biquad_config* pConfig, ma_biquad_node* pNode);
MA_API void ma_biquad_node_uninit(ma_biquad_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);


/*
Low Pass Filter Node
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_lpf_config lpf;
} ma_lpf_node_config;

MA_API ma_lpf_node_config ma_lpf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order);


typedef struct
{
    ma_node_base baseNode;
    ma_lpf lpf;
} ma_lpf_node;

MA_API ma_result ma_lpf_node_init(ma_node_graph* pNodeGraph, const ma_lpf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_lpf_node* pNode);
MA_API ma_result ma_lpf_node_reinit(const ma_lpf_config* pConfig, ma_lpf_node* pNode);
MA_API void ma_lpf_node_uninit(ma_lpf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);


/*
High Pass Filter Node
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_hpf_config hpf;
} ma_hpf_node_config;

MA_API ma_hpf_node_config ma_hpf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order);


typedef struct
{
    ma_node_base baseNode;
    ma_hpf hpf;
} ma_hpf_node;

MA_API ma_result ma_hpf_node_init(ma_node_graph* pNodeGraph, const ma_hpf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_hpf_node* pNode);
MA_API ma_result ma_hpf_node_reinit(const ma_hpf_config* pConfig, ma_hpf_node* pNode);
MA_API void ma_hpf_node_uninit(ma_hpf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);


/*
Band Pass Filter Node
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_bpf_config bpf;
} ma_bpf_node_config;

MA_API ma_bpf_node_config ma_bpf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order);


typedef struct
{
    ma_node_base baseNode;
    ma_bpf bpf;
} ma_bpf_node;

MA_API ma_result ma_bpf_node_init(ma_node_graph* pNodeGraph, const ma_bpf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_bpf_node* pNode);
MA_API ma_result ma_bpf_node_reinit(const ma_bpf_config* pConfig, ma_bpf_node* pNode);
MA_API void ma_bpf_node_uninit(ma_bpf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);


/*
Notching Filter Node
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_notch_config notch;
} ma_notch_node_config;

MA_API ma_notch_node_config ma_notch_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double q, double frequency);


typedef struct
{
    ma_node_base baseNode;
    ma_notch2 notch;
} ma_notch_node;

MA_API ma_result ma_notch_node_init(ma_node_graph* pNodeGraph, const ma_notch_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_notch_node* pNode);
MA_API ma_result ma_notch_node_reinit(const ma_notch_config* pConfig, ma_notch_node* pNode);
MA_API void ma_notch_node_uninit(ma_notch_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);


/*
Peaking Filter Node
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_peak_config peak;
} ma_peak_node_config;

MA_API ma_peak_node_config ma_peak_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double q, double frequency);


typedef struct
{
    ma_node_base baseNode;
    ma_peak2 peak;
} ma_peak_node;

MA_API ma_result ma_peak_node_init(ma_node_graph* pNodeGraph, const ma_peak_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_peak_node* pNode);
MA_API ma_result ma_peak_node_reinit(const ma_peak_config* pConfig, ma_peak_node* pNode);
MA_API void ma_peak_node_uninit(ma_peak_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);


/*
Low Shelf Filter Node
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_loshelf_config loshelf;
} ma_loshelf_node_config;

MA_API ma_loshelf_node_config ma_loshelf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double q, double frequency);


typedef struct
{
    ma_node_base baseNode;
    ma_loshelf2 loshelf;
} ma_loshelf_node;

MA_API ma_result ma_loshelf_node_init(ma_node_graph* pNodeGraph, const ma_loshelf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_loshelf_node* pNode);
MA_API ma_result ma_loshelf_node_reinit(const ma_loshelf_config* pConfig, ma_loshelf_node* pNode);
MA_API void ma_loshelf_node_uninit(ma_loshelf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);


/*
High Shelf Filter Node
*/
typedef struct
{
    ma_node_config nodeConfig;
    ma_hishelf_config hishelf;
} ma_hishelf_node_config;

MA_API ma_hishelf_node_config ma_hishelf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double q, double frequency);


typedef struct
{
    ma_node_base baseNode;
    ma_hishelf2 hishelf;
} ma_hishelf_node;

MA_API ma_result ma_hishelf_node_init(ma_node_graph* pNodeGraph, const ma_hishelf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_hishelf_node* pNode);
MA_API ma_result ma_hishelf_node_reinit(const ma_hishelf_config* pConfig, ma_hishelf_node* pNode);
MA_API void ma_hishelf_node_uninit(ma_hishelf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks);



/*
Delay
*/
typedef struct
{
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 delayInFrames;
    ma_bool32 delayStart;       /* Set to true to delay the start of the output; false otherwise. */
    float wet;                  /* 0..1. Default = 1. */
    float dry;                  /* 0..1. Default = 1. */
    float decay;                /* 0..1. Default = 0 (no feedback). Feedback decay. Use this for echo. */
} ma_delay_config;

MA_API ma_delay_config ma_delay_config_init(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 delayInFrames, float decay);


typedef struct
{
    ma_delay_config config;
    ma_uint32 cursor;               /* Feedback is written to this cursor. Always equal or in front of the read cursor. */
    ma_uint32 bufferSizeInFrames;   /* The maximum of config.startDelayInFrames and config.feedbackDelayInFrames. */
    float* pBuffer;
} ma_delay;

MA_API ma_result ma_delay_init(const ma_delay_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_delay* pDelay);
MA_API void ma_delay_uninit(ma_delay* pDelay, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_delay_process_pcm_frames(ma_delay* pDelay, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount);
MA_API void ma_delay_set_wet(ma_delay* pDelay, float value);
MA_API float ma_delay_get_wet(const ma_delay* pDelay);
MA_API void ma_delay_set_dry(ma_delay* pDelay, float value);
MA_API float ma_delay_get_dry(const ma_delay* pDelay);
MA_API void ma_delay_set_decay(ma_delay* pDelay, float value);
MA_API float ma_delay_get_decay(const ma_delay* pDelay);



typedef struct
{
    ma_node_config nodeConfig;
    ma_delay_config delay;
} ma_delay_node_config;

MA_API ma_delay_node_config ma_delay_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 delayInFrames, float decay);


typedef struct
{
    ma_node_base baseNode;
    ma_delay delay;
} ma_delay_node;

MA_API ma_result ma_delay_node_init(ma_node_graph* pNodeGraph, const ma_delay_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_delay_node* pDelayNode);
MA_API void ma_delay_node_uninit(ma_delay_node* pDelayNode, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API void ma_delay_node_set_wet(ma_delay_node* pDelayNode, float value);
MA_API float ma_delay_node_get_wet(const ma_delay_node* pDelayNode);
MA_API void ma_delay_node_set_dry(ma_delay_node* pDelayNode, float value);
MA_API float ma_delay_node_get_dry(const ma_delay_node* pDelayNode);
MA_API void ma_delay_node_set_decay(ma_delay_node* pDelayNode, float value);
MA_API float ma_delay_node_get_decay(const ma_delay_node* pDelayNode);


#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_engine_h */


#if defined(MA_IMPLEMENTATION) || defined(MINIAUDIO_IMPLEMENTATION)

/* 10ms @ 48K = 480. Must never exceed 65535. */
#ifndef MA_DEFAULT_NODE_CACHE_CAP_IN_FRAMES_PER_BUS
#define MA_DEFAULT_NODE_CACHE_CAP_IN_FRAMES_PER_BUS 480
#endif


static ma_result ma_node_read_pcm_frames(ma_node* pNode, ma_uint32 outputBusIndex, float* pFramesOut, ma_uint32 frameCount, ma_uint32* pFramesRead, ma_uint64 globalTime);


MA_API void ma_debug_fill_pcm_frames_with_sine_wave(float* pFramesOut, ma_uint32 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    #ifndef MA_NO_GENERATION
    {
        ma_waveform_config waveformConfig;
        ma_waveform waveform;

        waveformConfig = ma_waveform_config_init(format, channels, sampleRate, ma_waveform_type_sine, 1.0, 400);
        ma_waveform_init(&waveformConfig, &waveform);
        ma_waveform_read_pcm_frames(&waveform, pFramesOut, frameCount);
    }
    #else
    {
        (void)pFramesOut;
        (void)frameCount;
        (void)format;
        (void)channels;
        (void)sampleRate;
        #if defined(MA_DEBUG_OUTPUT)
        {
            #warning ma_debug_fill_pcm_frames_with_sine_wave() will do nothing because MA_NO_GENERATION is enabled.
        }
        #endif
    }
    #endif
}





static ma_result ma_channel_map_build_shuffle_table(const ma_channel* pChannelMapIn, ma_uint32 channelCountIn, const ma_channel* pChannelMapOut, ma_uint32 channelCountOut, ma_uint8* pShuffleTable)
{
    ma_uint32 iChannelIn;
    ma_uint32 iChannelOut;

    if (pShuffleTable == NULL || channelCountIn == 0 || channelCountIn > MA_MAX_CHANNELS || channelCountOut == 0 || channelCountOut > MA_MAX_CHANNELS) {
        return MA_INVALID_ARGS;
    }

    /*
    When building the shuffle table we just do a 1:1 mapping based on the first occurance of a channel. If the
    input channel has more than one occurance of a channel position, the second one will be ignored.
    */
    for (iChannelOut = 0; iChannelOut < channelCountOut; iChannelOut += 1) {
        ma_channel channelOut;

        /* Default to MA_CHANNEL_INDEX_NULL so that if a mapping is not found it'll be set appropriately. */
        pShuffleTable[iChannelOut] = MA_CHANNEL_INDEX_NULL;

        channelOut = ma_channel_map_get_channel(pChannelMapOut, channelCountOut, iChannelOut);
        for (iChannelIn = 0; iChannelIn < channelCountIn; iChannelIn += 1) {
            ma_channel channelIn;

            channelIn = ma_channel_map_get_channel(pChannelMapIn, channelCountIn, iChannelIn);
            if (channelOut == channelIn) {
                pShuffleTable[iChannelOut] = (ma_uint8)iChannelIn;
                break;
            }

            /*
            Getting here means the channels don't exactly match, but we are going to support some
            relaxed matching for practicality. If, for example, there are two stereo channel maps,
            but one uses front left/right and the other uses side left/right, it makes logical
            sense to just map these. The way we'll do it is we'll check if there is a logical
            corresponding mapping, and if so, apply it, but we will *not* break from the loop,
            thereby giving the loop a chance to find an exact match later which will take priority.
            */
            switch (channelOut)
            {
                /* Left channels. */
                case MA_CHANNEL_FRONT_LEFT:
                case MA_CHANNEL_SIDE_LEFT:
                {
                    switch (channelIn) {
                        case MA_CHANNEL_FRONT_LEFT:
                        case MA_CHANNEL_SIDE_LEFT:
                        {
                            pShuffleTable[iChannelOut] = (ma_uint8)iChannelIn;
                        } break;
                    }
                } break;

                /* Right channels. */
                case MA_CHANNEL_FRONT_RIGHT:
                case MA_CHANNEL_SIDE_RIGHT:
                {
                    switch (channelIn) {
                        case MA_CHANNEL_FRONT_RIGHT:
                        case MA_CHANNEL_SIDE_RIGHT:
                        {
                            pShuffleTable[iChannelOut] = (ma_uint8)iChannelIn;
                        } break;
                    }
                } break;

                default: break;
            }
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_channel_map_apply_shuffle_table_f32(float* pFramesOut, ma_uint32 channelsOut, const float* pFramesIn, ma_uint32 channelsIn, ma_uint64 frameCount, const ma_uint8* pShuffleTable)
{
    ma_uint64 iFrame;
    ma_uint32 iChannelOut;

    if (pFramesOut == NULL || pFramesIn == NULL || channelsOut == 0 || channelsOut > MA_MAX_CHANNELS || pShuffleTable == NULL) {
        return MA_INVALID_ARGS;
    }

    for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
        for (iChannelOut = 0; iChannelOut < channelsOut; iChannelOut += 1) {
            ma_uint8 iChannelIn = pShuffleTable[iChannelOut];
            if (iChannelIn < channelsIn) {  /* For safety, and to deal with MA_CHANNEL_INDEX_NULL. */
                pFramesOut[iChannelOut] = pFramesIn[iChannelIn];
            } else {
                pFramesOut[iChannelOut] = 0;
            }
        }

        pFramesOut += channelsOut;
        pFramesIn  += channelsIn;
    }

    return MA_SUCCESS;
}

static ma_result ma_channel_map_apply_mono_out_f32(float* pFramesOut, const float* pFramesIn, const ma_channel* pChannelMapIn, ma_uint32 channelsIn, ma_uint64 frameCount)
{
    ma_uint64 iFrame;
    ma_uint32 iChannelIn;
    ma_uint32 accumulationCount;

    if (pFramesOut == NULL || pFramesIn == NULL || channelsIn == 0) {
        return MA_INVALID_ARGS;
    }

    /* In this case the output stream needs to be the average of all channels, ignoring NONE. */

    /* A quick pre-processing step to get the accumulation counter since we're ignoring NONE channels. */
    accumulationCount = 0;
    for (iChannelIn = 0; iChannelIn < channelsIn; iChannelIn += 1) {
        if (ma_channel_map_get_channel(pChannelMapIn, channelsIn, iChannelIn) != MA_CHANNEL_NONE) {
            accumulationCount += 1;
        }
    }

    if (accumulationCount > 0) {    /* <-- Prevent a division by zero. */
        for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
            float accumulation = 0;

            for (iChannelIn = 0; iChannelIn < channelsIn; iChannelIn += 1) {
                ma_channel channelIn = ma_channel_map_get_channel(pChannelMapIn, channelsIn, iChannelIn);
                if (channelIn != MA_CHANNEL_NONE) {
                    accumulation += pFramesIn[iChannelIn];
                }
            }

            pFramesOut[0] = accumulation / accumulationCount;
            pFramesOut += 1;
            pFramesIn  += channelsIn;
        }
    } else {
        ma_silence_pcm_frames(pFramesOut, frameCount, ma_format_f32, 1);
    }

    return MA_SUCCESS;
}

static ma_result ma_channel_map_apply_mono_in_f32(float* pFramesOut, const ma_channel* pChannelMapOut, ma_uint32 channelsOut, const float* pFramesIn, ma_uint64 frameCount)
{
    ma_uint64 iFrame;
    ma_uint32 iChannelOut;

    if (pFramesOut == NULL || channelsOut == 0 || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    /* In this case we just copy the mono channel to each of the output channels, ignoring NONE. */
    for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
        for (iChannelOut = 0; iChannelOut < channelsOut; iChannelOut += 1) {
            ma_channel channelOut = ma_channel_map_get_channel(pChannelMapOut, channelsOut, iChannelOut);
            if (channelOut != MA_CHANNEL_NONE) {
                pFramesOut[iChannelOut] = pFramesIn[0];
            }
        }

        pFramesOut += channelsOut;
        pFramesIn  += 1;
    }

    return MA_SUCCESS;
}

static void ma_channel_map_apply_f32(float* pFramesOut, const ma_channel* pChannelMapOut, ma_uint32 channelsOut, const float* pFramesIn, const ma_channel* pChannelMapIn, ma_uint32 channelsIn, ma_uint64 frameCount, ma_channel_mix_mode mode)
{
    ma_bool32 passthrough = MA_FALSE;

    if (channelsOut == channelsIn) {
        if (pChannelMapOut == pChannelMapIn) {
            passthrough = MA_TRUE;
        } else {
            if (ma_channel_map_equal(channelsOut, pChannelMapOut, pChannelMapIn)) {
                passthrough = MA_TRUE;
            }
        }
    }

    /* Optimized Path: Passthrough */
    if (passthrough) {
        ma_copy_pcm_frames(pFramesOut, pFramesIn, frameCount, ma_format_f32, channelsOut);
        return;
    }

    /* Special Path: Mono Output. */
    if (channelsOut == 1 && (pChannelMapOut == NULL || pChannelMapOut[0] == MA_CHANNEL_MONO)) {
        ma_channel_map_apply_mono_out_f32(pFramesOut, pFramesIn, pChannelMapIn, channelsIn, frameCount);
        return;
    }

    /* Special Path: Mono Input. */
    if (channelsIn == 1 && (pChannelMapIn == NULL || pChannelMapIn[0] == MA_CHANNEL_MONO)) {
        ma_channel_map_apply_mono_in_f32(pFramesOut, pChannelMapOut, channelsOut, pFramesIn, frameCount);
        return;
    }


    if (channelsOut <= MA_MAX_CHANNELS) {
        ma_result result;

        if (mode == ma_channel_mix_mode_simple) {    
            ma_channel shuffleTable[MA_MAX_CHANNELS];

            result = ma_channel_map_build_shuffle_table(pChannelMapIn, channelsIn, pChannelMapOut, channelsOut, shuffleTable);
            if (result != MA_SUCCESS) {
                return;
            }

            result = ma_channel_map_apply_shuffle_table_f32(pFramesOut, channelsOut, pFramesIn, channelsIn, frameCount, shuffleTable);
            if (result != MA_SUCCESS) {
                return;
            }
        } else {
            ma_uint32 iFrame;
            ma_uint32 iChannelOut;
            ma_uint32 iChannelIn;
            float weights[32][32];  /* Do not use MA_MAX_CHANNELS here! */

            /*
            If we have a small enough number of channels, pre-compute the weights. Otherwise we'll just need to
            fall back to a slower path because otherwise we'll run out of stack space.
            */
            if (channelsIn <= ma_countof(weights) && channelsOut <= ma_countof(weights)) {
                /* Pre-compute weights. */
                for (iChannelOut = 0; iChannelOut < channelsOut; iChannelOut += 1) {
                    ma_channel channelOut = ma_channel_map_get_channel(pChannelMapOut, channelsOut, iChannelOut);
                    for (iChannelIn = 0; iChannelIn < channelsIn; iChannelIn += 1) {
                        ma_channel channelIn = ma_channel_map_get_channel(pChannelMapIn, channelsIn, iChannelIn);
                        weights[iChannelOut][iChannelIn] = ma_calculate_channel_position_rectangular_weight(channelOut, channelIn);
                    }
                }

                for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    for (iChannelOut = 0; iChannelOut < channelsOut; iChannelOut += 1) {
                        float accumulation = 0;

                        for (iChannelIn = 0; iChannelIn < channelsIn; iChannelIn += 1) {
                            accumulation += pFramesIn[iChannelIn] * weights[iChannelOut][iChannelIn];
                        }

                        pFramesOut[iChannelOut] = accumulation;
                    }

                    pFramesOut += channelsOut;
                    pFramesIn  += channelsIn;
                }
            } else {
                /* Cannot pre-compute weights because not enough room in stack-allocated buffer. */
                for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
                    for (iChannelOut = 0; iChannelOut < channelsOut; iChannelOut += 1) {
                        float accumulation = 0;
                        ma_channel channelOut = ma_channel_map_get_channel(pChannelMapOut, channelsOut, iChannelOut);

                        for (iChannelIn = 0; iChannelIn < channelsIn; iChannelIn += 1) {
                            ma_channel channelIn = ma_channel_map_get_channel(pChannelMapIn, channelsIn, iChannelIn);
                            accumulation += pFramesIn[iChannelIn] * ma_calculate_channel_position_rectangular_weight(channelOut, channelIn);
                        }

                        pFramesOut[iChannelOut] = accumulation;
                    }

                    pFramesOut += channelsOut;
                    pFramesIn  += channelsIn;
                }
            }
        }
    } else {
        /* Fall back to silence. If you hit this, what are you doing with so many channels?! */
        ma_silence_pcm_frames(pFramesOut, frameCount, ma_format_f32, channelsOut);
    }
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


MA_API ma_node_graph_config ma_node_graph_config_init(ma_uint32 channels)
{
    ma_node_graph_config config;

    MA_ZERO_OBJECT(&config);
    config.channels = channels;

    return config;
}


static void ma_node_graph_set_is_reading(ma_node_graph* pNodeGraph, ma_bool8 isReading)
{
    MA_ASSERT(pNodeGraph != NULL);
    c89atomic_exchange_8(&pNodeGraph->isReading, isReading);
}

#if 0
static ma_bool8 ma_node_graph_is_reading(ma_node_graph* pNodeGraph)
{
    MA_ASSERT(pNodeGraph != NULL);
    return c89atomic_load_8(&pNodeGraph->isReading);
}
#endif


static void ma_node_graph_endpoint_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    MA_ASSERT(pNode != NULL);
    MA_ASSERT(ma_node_get_input_bus_count(pNode)  == 1);
    MA_ASSERT(ma_node_get_output_bus_count(pNode) == 1);

    /* Input channel count needs to be the same as the output channel count. */
    MA_ASSERT(ma_node_get_input_channels(pNode, 0) == ma_node_get_output_channels(pNode, 0));

    /* We don't need to do anything here because it's a passthrough. */
    (void)pNode;
    (void)ppFramesIn;
    (void)pFrameCountIn;
    (void)ppFramesOut;
    (void)pFrameCountOut;

#if 0
    /* The data has already been mixed. We just need to move it to the output buffer. */
    if (ppFramesIn != NULL) {
        ma_copy_pcm_frames(ppFramesOut[0], ppFramesIn[0], *pFrameCountOut, ma_format_f32, ma_node_get_output_channels(pNode, 0));
    }
#endif
}

static ma_node_vtable g_node_graph_endpoint_vtable =
{
    ma_node_graph_endpoint_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* 1 input bus. */
    1,      /* 1 output bus. */
    MA_NODE_FLAG_PASSTHROUGH    /* Flags. The endpoint is a passthrough. */
};

MA_API ma_result ma_node_graph_init(const ma_node_graph_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_node_graph* pNodeGraph)
{
    ma_result result;
    ma_node_config endpointConfig;

    if (pNodeGraph == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNodeGraph);

    endpointConfig = ma_node_config_init();
    endpointConfig.vtable          = &g_node_graph_endpoint_vtable;
    endpointConfig.pInputChannels  = &pConfig->channels;
    endpointConfig.pOutputChannels = &pConfig->channels;

    result = ma_node_init(pNodeGraph, &endpointConfig, pAllocationCallbacks, &pNodeGraph->endpoint);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_node_graph_uninit(ma_node_graph* pNodeGraph, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pNodeGraph == NULL) {
        return;
    }

    ma_node_uninit(&pNodeGraph->endpoint, pAllocationCallbacks);
}

MA_API ma_node* ma_node_graph_get_endpoint(ma_node_graph* pNodeGraph)
{
    if (pNodeGraph == NULL) {
        return NULL;
    }

    return &pNodeGraph->endpoint;
}

MA_API ma_result ma_node_graph_read_pcm_frames(ma_node_graph* pNodeGraph, void* pFramesOut, ma_uint32 frameCount, ma_uint32* pFramesRead)
{
    ma_result result = MA_SUCCESS;
    ma_uint32 totalFramesRead;
    ma_uint32 channels;

    if (pFramesRead != NULL) {
        *pFramesRead = 0;   /* Safety. */
    }

    if (pNodeGraph == NULL) {
        return MA_INVALID_ARGS;
    }

    channels = ma_node_get_output_channels(&pNodeGraph->endpoint, 0);

    
    /* We'll be nice and try to do a full read of all frameCount frames. */
    totalFramesRead = 0;
    while (totalFramesRead < frameCount) {
        ma_uint32 framesJustRead;
        ma_uint32 framesToRead = frameCount - totalFramesRead;
        
        ma_node_graph_set_is_reading(pNodeGraph, MA_TRUE);
        {
            result = ma_node_read_pcm_frames(&pNodeGraph->endpoint, 0, (float*)ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, ma_format_f32, channels), framesToRead, &framesJustRead, ma_node_get_time(&pNodeGraph->endpoint));
        }
        ma_node_graph_set_is_reading(pNodeGraph, MA_FALSE);

        totalFramesRead += framesJustRead;

        if (result != MA_SUCCESS) {
            break;
        }

        /* Abort if we weren't able to read any frames or else we risk getting stuck in a loop. */
        if (framesJustRead == 0) {
            break;
        }
    }

    /* Let's go ahead and silence any leftover frames just for some added safety to ensure the caller doesn't try emitting garbage out of the speakers. */
    if (totalFramesRead < frameCount) {
        ma_silence_pcm_frames(ma_offset_pcm_frames_ptr(pFramesOut, totalFramesRead, ma_format_f32, channels), (frameCount - totalFramesRead), ma_format_f32, channels);
    }

    if (pFramesRead != NULL) {
        *pFramesRead = totalFramesRead;
    }

    return result;
}

MA_API ma_uint32 ma_node_graph_get_channels(const ma_node_graph* pNodeGraph)
{
    if (pNodeGraph == NULL) {
        return 0;
    }

    return ma_node_get_output_channels(&pNodeGraph->endpoint, 0);
}

MA_API ma_uint64 ma_node_graph_get_time(const ma_node_graph* pNodeGraph)
{
    if (pNodeGraph == NULL) {
        return 0;
    }

    return ma_node_get_time(&pNodeGraph->endpoint); /* Global time is just the local time of the endpoint. */
}

MA_API ma_result ma_node_graph_set_time(ma_node_graph* pNodeGraph, ma_uint64 globalTime)
{
    if (pNodeGraph == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_node_set_time(&pNodeGraph->endpoint, globalTime); /* Global time is just the local time of the endpoint. */
}



static ma_result ma_node_output_bus_init(ma_node* pNode, ma_uint32 outputBusIndex, ma_uint32 channels, ma_node_output_bus* pOutputBus)
{
    MA_ASSERT(pOutputBus != NULL);
    MA_ASSERT(outputBusIndex < MA_MAX_NODE_BUS_COUNT);
    MA_ASSERT(outputBusIndex < ma_node_get_output_bus_count(pNode));
    MA_ASSERT(channels < 256);

    MA_ZERO_OBJECT(pOutputBus);

    if (channels == 0) {
        return MA_INVALID_ARGS;
    }

    pOutputBus->pNode          = pNode;
    pOutputBus->outputBusIndex = (ma_uint8)outputBusIndex;
    pOutputBus->channels       = (ma_uint8)channels;
    pOutputBus->flags          = MA_NODE_OUTPUT_BUS_FLAG_HAS_READ; /* <-- Important that this flag is set by default. */
    pOutputBus->volume         = 1;

    return MA_SUCCESS;
}

static void ma_node_output_bus_lock(ma_node_output_bus* pOutputBus)
{
    ma_spinlock_lock(&pOutputBus->lock);
}

static void ma_node_output_bus_unlock(ma_node_output_bus* pOutputBus)
{
    ma_spinlock_unlock(&pOutputBus->lock);
}


static ma_uint32 ma_node_output_bus_get_channels(const ma_node_output_bus* pOutputBus)
{
    return pOutputBus->channels;
}


static void ma_node_output_bus_set_has_read(ma_node_output_bus* pOutputBus, ma_bool32 hasRead)
{
    if (hasRead) {
        c89atomic_fetch_or_8(&pOutputBus->flags, MA_NODE_OUTPUT_BUS_FLAG_HAS_READ);
    } else {
        c89atomic_fetch_and_8(&pOutputBus->flags, (ma_uint8)~MA_NODE_OUTPUT_BUS_FLAG_HAS_READ);
    }
}

static ma_bool32 ma_node_output_bus_has_read(ma_node_output_bus* pOutputBus)
{
    return (c89atomic_load_8(&pOutputBus->flags) & MA_NODE_OUTPUT_BUS_FLAG_HAS_READ) != 0;
}


static void ma_node_output_bus_set_is_attached(ma_node_output_bus* pOutputBus, ma_bool8 isAttached)
{
    c89atomic_exchange_8(&pOutputBus->isAttached, isAttached);
}

static ma_bool8 ma_node_output_bus_is_attached(ma_node_output_bus* pOutputBus)
{
    return c89atomic_load_8(&pOutputBus->isAttached);
}


static ma_result ma_node_output_bus_set_volume(ma_node_output_bus* pOutputBus, float volume)
{
    MA_ASSERT(pOutputBus != NULL);

    if (volume < 0.0f) {
        volume = 0.0f;
    }

    c89atomic_exchange_f32(&pOutputBus->volume, volume);

    return MA_SUCCESS;
}

static float ma_node_output_bus_get_volume(const ma_node_output_bus* pOutputBus)
{
    return c89atomic_load_f32((float*)&pOutputBus->volume);
}


static ma_result ma_node_input_bus_init(ma_uint32 channels, ma_node_input_bus* pInputBus)
{
    MA_ASSERT(pInputBus != NULL);
    MA_ASSERT(channels < 256);

    MA_ZERO_OBJECT(pInputBus);

    if (channels == 0) {
        return MA_INVALID_ARGS;
    }

    pInputBus->channels = (ma_uint8)channels;

    return MA_SUCCESS;
}

static void ma_node_input_bus_lock(ma_node_input_bus* pInputBus)
{
    ma_spinlock_lock(&pInputBus->lock);
}

static void ma_node_input_bus_unlock(ma_node_input_bus* pInputBus)
{
    ma_spinlock_unlock(&pInputBus->lock);
}


static void ma_node_input_bus_next_begin(ma_node_input_bus* pInputBus)
{
    c89atomic_fetch_add_16(&pInputBus->nextCounter, 1);
}

static void ma_node_input_bus_next_end(ma_node_input_bus* pInputBus)
{
    c89atomic_fetch_sub_16(&pInputBus->nextCounter, 1);
}

static ma_uint16 ma_node_input_bus_get_next_counter(ma_node_input_bus* pInputBus)
{
    return c89atomic_load_16(&pInputBus->nextCounter);
}


static ma_uint32 ma_node_input_bus_get_channels(const ma_node_input_bus* pInputBus)
{
    return pInputBus->channels;
}


static void ma_node_input_bus_detach__no_output_bus_lock(ma_node_input_bus* pInputBus, ma_node_output_bus* pOutputBus)
{
    MA_ASSERT(pInputBus  != NULL);
    MA_ASSERT(pOutputBus != NULL);

    /*
    Mark the output bus as detached first. This will prevent future iterations on the audio thread
    from iterating this output bus.
    */
    ma_node_output_bus_set_is_attached(pOutputBus, MA_FALSE);

    /*
    We cannot use the output bus lock here since it'll be getting used at a higher level, but we do
    still need to use the input bus lock since we'll be updating pointers on two different output
    buses. The same rules apply here as the attaching case. Although we're using a lock here, we're
    *not* using a lock when iterating over the list in the audio thread. We therefore need to craft
    this in a way such that the iteration on the audio thread doesn't break.

    The the first thing to do is swap out the "next" pointer of the previous output bus with the
    new "next" output bus. This is the operation that matters for iteration on the audio thread.
    After that, the previous pointer on the new "next" pointer needs to be updated, after which
    point the linked list will be in a good state.
    */
    ma_node_input_bus_lock(pInputBus);
    {
        ma_node_output_bus* pOldPrev = (ma_node_output_bus*)c89atomic_load_ptr(&pOutputBus->pPrev);
        ma_node_output_bus* pOldNext = (ma_node_output_bus*)c89atomic_load_ptr(&pOutputBus->pNext);

        if (pOldPrev != NULL) {
            c89atomic_exchange_ptr(&pOldPrev->pNext, pOldNext); /* <-- This is where the output bus is detached from the list. */
        }
        if (pOldNext != NULL) {
            c89atomic_exchange_ptr(&pOldNext->pPrev, pOldPrev); /* <-- This is required for detachment. */
        }
    }
    ma_node_input_bus_unlock(pInputBus);

    /* At this point the output bus is detached and the linked list is completely unaware of it. Reset some data for safety. */
    c89atomic_exchange_ptr(&pOutputBus->pNext, NULL);   /* Using atomic exchanges here, mainly for the benefit of analysis tools which don't always recognize spinlocks. */
    c89atomic_exchange_ptr(&pOutputBus->pPrev, NULL);   /* As above. */
    pOutputBus->pInputNode             = NULL;
    pOutputBus->inputNodeInputBusIndex = 0;


    /*
    For thread-safety reasons, we don't want to be returning from this straight away. We need to
    wait for the audio thread to finish with the output bus. There's two things we need to wait
    for. The first is the part that selects the next output bus in the list, and the other is the
    part that reads from the output bus. Basically all we're doing is waiting for the input bus
    to stop referencing the output bus.

    We're doing this part last because we want the section above to run while the audio thread
    is finishing up with the output bus, just for efficiency reasons. We marked the output bus as
    detached right at the top of this function which is going to prevent the audio thread from
    iterating the output bus again.
    */

    /* Part 1: Wait for the current iteration to complete. */
    while (ma_node_input_bus_get_next_counter(pInputBus) > 0) {
        ma_yield();
    }

    /* Part 2: Wait for any reads to complete. */
    while (c89atomic_load_16(&pOutputBus->refCount) > 0) {
        ma_yield();
    }

    /*
    At this point we're done detaching and we can be guaranteed that the audio thread is not going
    to attempt to reference this output bus again (until attached again).
    */
}

#if 0   /* Not used at the moment, but leaving here in case I need it later. */
static void ma_node_input_bus_detach(ma_node_input_bus* pInputBus, ma_node_output_bus* pOutputBus)
{
    MA_ASSERT(pInputBus  != NULL);
    MA_ASSERT(pOutputBus != NULL);

    ma_node_output_bus_lock(pOutputBus);
    {
        ma_node_input_bus_detach__no_output_bus_lock(pInputBus, pOutputBus);
    }
    ma_node_output_bus_unlock(pOutputBus);
}
#endif

static void ma_node_input_bus_attach(ma_node_input_bus* pInputBus, ma_node_output_bus* pOutputBus, ma_node* pNewInputNode, ma_uint32 inputNodeInputBusIndex)
{
    MA_ASSERT(pInputBus  != NULL);
    MA_ASSERT(pOutputBus != NULL);

    ma_node_output_bus_lock(pOutputBus);
    {
        ma_node_output_bus* pOldInputNode = (ma_node_output_bus*)c89atomic_load_ptr(&pOutputBus->pInputNode);

        /* Detach from any existing attachment first if necessary. */
        if (pOldInputNode != NULL) {
            ma_node_input_bus_detach__no_output_bus_lock(pInputBus, pOutputBus);
        }

        /*
        At this point we can be sure the output bus is not attached to anything. The linked list in the
        old input bus has been updated so that pOutputBus will not get iterated again.
        */
        pOutputBus->pInputNode             = pNewInputNode;                     /* No need for an atomic assignment here because modification of this variable always happens within a lock. */
        pOutputBus->inputNodeInputBusIndex = (ma_uint8)inputNodeInputBusIndex;  /* As above. */

        /*
        Now we need to attach the output bus to the linked list. This involves updating two pointers on
        two different output buses so I'm going to go ahead and keep this simple and just use a lock.
        There are ways to do this without a lock, but it's just too hard to maintain for it's value.

        Although we're locking here, it's important to remember that we're *not* locking when iterating
        and reading audio data since that'll be running on the audio thread. As a result we need to be
        careful how we craft this so that we don't break iteration. What we're going to do is always
        attach the new item so that it becomes the first item in the list. That way, as we're iterating
        we won't break any links in the list and iteration will continue safely. The detaching case will
        also be crafted in a way as to not break list iteration. It's important to remember to use
        atomic exchanges here since no locking is happening on the audio thread during iteration.
        */
        ma_node_input_bus_lock(pInputBus);
        {
            ma_node_output_bus* pNewPrev = &pInputBus->head;
            ma_node_output_bus* pNewNext = (ma_node_output_bus*)c89atomic_load_ptr(&pInputBus->head.pNext);

            /* Update the local output bus. */
            c89atomic_exchange_ptr(&pOutputBus->pPrev, pNewPrev);
            c89atomic_exchange_ptr(&pOutputBus->pNext, pNewNext);

            /* Update the other output buses to point back to the local output bus. */
            c89atomic_exchange_ptr(&pInputBus->head.pNext, pOutputBus); /* <-- This is where the output bus is actually attached to the input bus. */

            /* Do the previous pointer last. This is only used for detachment. */
            if (pNewNext != NULL) {
                c89atomic_exchange_ptr(&pNewNext->pPrev,  pOutputBus);
            }
        }
        ma_node_input_bus_unlock(pInputBus);

        /*
        Mark the node as attached last. This is used to controlling whether or the output bus will be
        iterated on the audio thread. Mainly required for detachment purposes.
        */
        ma_node_output_bus_set_is_attached(pOutputBus, MA_TRUE);
    }
    ma_node_output_bus_unlock(pOutputBus);
}

static ma_node_output_bus* ma_node_input_bus_next(ma_node_input_bus* pInputBus, ma_node_output_bus* pOutputBus)
{
    ma_node_output_bus* pNext;

    MA_ASSERT(pInputBus != NULL);

    if (pOutputBus == NULL) {
        return NULL;
    }

    ma_node_input_bus_next_begin(pInputBus);
    {
        pNext = pOutputBus;
        for (;;) {
            pNext = (ma_node_output_bus*)c89atomic_load_ptr(&pNext->pNext);
            if (pNext == NULL) {
                break;      /* Reached the end. */
            }

            if (ma_node_output_bus_is_attached(pNext) == MA_FALSE) {
                continue;   /* The node is not attached. Keep checking. */
            }

            /* The next node has been selected. */
            break;
        }

        /* We need to increment the reference count of the selected node. */
        if (pNext != NULL) {
            c89atomic_fetch_add_16(&pNext->refCount, 1);
        }
        
        /* The previous node is no longer being referenced. */
        c89atomic_fetch_sub_16(&pOutputBus->refCount, 1);
    }
    ma_node_input_bus_next_end(pInputBus);

    return pNext;
}

static ma_node_output_bus* ma_node_input_bus_first(ma_node_input_bus* pInputBus)
{
    return ma_node_input_bus_next(pInputBus, &pInputBus->head);
}



static ma_result ma_node_input_bus_read_pcm_frames(ma_node* pInputNode, ma_node_input_bus* pInputBus, float* pFramesOut, ma_uint32 frameCount, ma_uint32* pFramesRead, ma_uint64 globalTime)
{
    ma_result result = MA_SUCCESS;
    ma_node_output_bus* pOutputBus;
    ma_node_output_bus* pFirst;
    ma_uint32 inputChannels;

    /*
    This will be called from the audio thread which means we can't be doing any locking. Basically,
    this function will not perfom any locking, whereas attaching and detaching will, but crafted in
    such a way that we don't need to perform any locking here. The important thing to remember is
    to always iterate in a forward direction.

    In order to process any data we need to first read from all input buses. That's where this
    function comes in. This iterates over each of the attachments and accumulates/mixes them. We
    also convert the channels to the nodes output channel count before mixing. We want to do this
    channel conversion so that the caller of this function can invoke the processing callback
    without having to do it themselves.

    When we iterate over each of the attachments on the input bus, we need to read as much data as
    we can from each of them so that we don't end up with holes between each of the attachments. To
    do this, we need to read from each attachment in a loop and read as many frames as we can, up
    to `frameCount`.
    */
    MA_ASSERT(pInputNode  != NULL);
    MA_ASSERT(pFramesRead != NULL); /* pFramesRead is critical and must always be specified. On input it's undefined and on output it'll be set to the number of frames actually read. */

    *pFramesRead = 0;   /* Safety. */

    inputChannels = ma_node_input_bus_get_channels(pInputBus);

    /*
    We need to be careful with how we call ma_node_input_bus_first() and ma_node_input_bus_next(). They
    are both critical to our lock-free thread-safety system. We can only call ma_node_input_bus_first()
    once per iteration, however we have an optimization to checks whether or not it's the first item in
    the list. We therefore need to store a pointer to the first item rather than repeatedly calling
    ma_node_input_bus_first(). It's safe to keep hold of this point, so long as we don't dereference it
    after calling ma_node_input_bus_next(), which we won't be.
    */
    pFirst = ma_node_input_bus_first(pInputBus);
    if (pFirst == NULL) {
        return MA_SUCCESS;  /* No attachments. Read nothing. */
    }

    for (pOutputBus = pFirst; pOutputBus != NULL; pOutputBus = ma_node_input_bus_next(pInputBus, pOutputBus)) {
        ma_uint32 framesProcessed = 0;

        MA_ASSERT(pOutputBus->pNode != NULL);

        if (pFramesOut != NULL) {
            /* Read. */
            float temp[MA_DATA_CONVERTER_STACK_BUFFER_SIZE / sizeof(float)];
            ma_uint32 tempCapInFrames = ma_countof(temp) / inputChannels;
            float volume = ma_node_output_bus_get_volume(pOutputBus);
                    
            while (framesProcessed < frameCount) {
                float* pRunningFramesOut;
                ma_uint32 framesToRead;
                ma_uint32 framesJustRead;

                framesToRead = frameCount - framesProcessed;
                if (framesToRead > tempCapInFrames) {
                    framesToRead = tempCapInFrames;
                }

                pRunningFramesOut = ma_offset_pcm_frames_ptr_f32(pFramesOut, framesProcessed, inputChannels);

                if (pOutputBus == pFirst) {
                    /* Fast path. First attachment. We just read straight into the output buffer (no mixing required). */
                    result = ma_node_read_pcm_frames(pOutputBus->pNode, pOutputBus->outputBusIndex, pRunningFramesOut, framesToRead, &framesJustRead, globalTime + framesProcessed);
                    if (result == MA_SUCCESS || result == MA_AT_END) {
                        /* Apply volume, if necessary. */
                        if (volume != 1) {
                            ma_apply_volume_factor_f32(pRunningFramesOut, framesJustRead * inputChannels, volume);
                        }
                    }
                } else {
                    /* Slow path. Not the first attachment. Mixing required. */
                    result = ma_node_read_pcm_frames(pOutputBus->pNode, pOutputBus->outputBusIndex, temp, framesToRead, &framesJustRead, globalTime + framesProcessed);
                    if (result == MA_SUCCESS || result == MA_AT_END) {
                        /* Apply volume, if necessary. */
                        if (volume != 1) {
                            ma_apply_volume_factor_f32(temp, framesJustRead * inputChannels, volume);
                        }

                        ma_mix_pcm_frames_f32(pRunningFramesOut, temp, framesJustRead, inputChannels, /*volume*/1);
                    }
                }

                framesProcessed += framesJustRead;

                /* If we reached the end or otherwise failed to read any data we need to finish up with this output node. */
                if (result != MA_SUCCESS) {
                    break;
                }

                /* If we didn't read anything, abort so we don't get stuck in a loop. */
                if (framesJustRead == 0) {
                    break;
                }
            }

            /* If it's the first attachment we didn't do any mixing. Any leftover samples need to be silenced. */
            if (pOutputBus == pFirst && framesProcessed < frameCount) {
                ma_silence_pcm_frames(ma_offset_pcm_frames_ptr(pFramesOut, framesProcessed, ma_format_f32, inputChannels), (frameCount - framesProcessed), ma_format_f32, inputChannels);
            }
        } else {
            /* Seek. */
            ma_node_read_pcm_frames(pOutputBus->pNode, pOutputBus->outputBusIndex, NULL, frameCount, &framesProcessed, globalTime);
        }
    }

    /* In this path we always "process" the entire amount. */
    *pFramesRead = frameCount;

    return result;
}


MA_API ma_node_config ma_node_config_init(void)
{
    ma_node_config config;

    MA_ZERO_OBJECT(&config);
    config.initialState   = ma_node_state_started;    /* Nodes are started by default. */
    config.inputBusCount  = MA_NODE_BUS_COUNT_UNKNOWN;
    config.outputBusCount = MA_NODE_BUS_COUNT_UNKNOWN;

    return config;
}



static ma_result ma_node_detach_full(ma_node* pNode);

static float* ma_node_get_cached_input_ptr(ma_node* pNode, ma_uint32 inputBusIndex)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;
    ma_uint32 iInputBus;
    float* pBasePtr;

    MA_ASSERT(pNodeBase != NULL);

    /* Input data is stored at the front of the buffer. */
    pBasePtr = pNodeBase->pCachedData;
    for (iInputBus = 0; iInputBus < inputBusIndex; iInputBus += 1) {
        pBasePtr += pNodeBase->cachedDataCapInFramesPerBus * ma_node_input_bus_get_channels(&pNodeBase->pInputBuses[iInputBus]);
    }

    return pBasePtr;
}

static float* ma_node_get_cached_output_ptr(ma_node* pNode, ma_uint32 outputBusIndex)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;
    ma_uint32 iInputBus;
    ma_uint32 iOutputBus;
    float* pBasePtr;

    MA_ASSERT(pNodeBase != NULL);

    /* Cached output data starts after the input data. */
    pBasePtr = pNodeBase->pCachedData;
    for (iInputBus = 0; iInputBus < ma_node_get_input_bus_count(pNodeBase); iInputBus += 1) {
        pBasePtr += pNodeBase->cachedDataCapInFramesPerBus * ma_node_input_bus_get_channels(&pNodeBase->pInputBuses[iInputBus]);
    }

    for (iOutputBus = 0; iOutputBus < outputBusIndex; iOutputBus += 1) {
        pBasePtr += pNodeBase->cachedDataCapInFramesPerBus * ma_node_output_bus_get_channels(&pNodeBase->pOutputBuses[iOutputBus]);
    }
    
    return pBasePtr;
}


typedef struct
{
    size_t sizeInBytes;
    size_t inputBusOffset;
    size_t outputBusOffset;
    size_t cachedDataOffset;
    ma_uint32 inputBusCount;    /* So it doesn't have to be calculated twice. */
    ma_uint32 outputBusCount;   /* So it doesn't have to be calculated twice. */
} ma_node_heap_layout;

static ma_result ma_node_translate_bus_counts(const ma_node_config* pConfig, ma_uint32* pInputBusCount, ma_uint32* pOutputBusCount)
{
    ma_uint32 inputBusCount;
    ma_uint32 outputBusCount;

    MA_ASSERT(pConfig != NULL);
    MA_ASSERT(pInputBusCount  != NULL);
    MA_ASSERT(pOutputBusCount != NULL);

    /* Bus counts are determined by the vtable, unless they're set to `MA_NODE_BUS_COUNT_UNKNWON`, in which case they're taken from the config. */
    if (pConfig->vtable->inputBusCount == MA_NODE_BUS_COUNT_UNKNOWN) {
        inputBusCount = pConfig->inputBusCount;
    } else {
        inputBusCount = pConfig->vtable->inputBusCount;

        if (pConfig->inputBusCount != MA_NODE_BUS_COUNT_UNKNOWN && pConfig->inputBusCount != pConfig->vtable->inputBusCount) {
            return MA_INVALID_ARGS; /* Invalid configuration. You must not specify a conflicting bus count between the node's config and the vtable. */
        }
    }

    if (pConfig->vtable->outputBusCount == MA_NODE_BUS_COUNT_UNKNOWN) {
        outputBusCount = pConfig->outputBusCount;
    } else {
        outputBusCount = pConfig->vtable->outputBusCount;

        if (pConfig->outputBusCount != MA_NODE_BUS_COUNT_UNKNOWN && pConfig->outputBusCount != pConfig->vtable->outputBusCount) {
            return MA_INVALID_ARGS; /* Invalid configuration. You must not specify a conflicting bus count between the node's config and the vtable. */
        }
    }

    /* Bus counts must be within limits. */
    if (inputBusCount > MA_MAX_NODE_BUS_COUNT || outputBusCount > MA_MAX_NODE_BUS_COUNT) {
        return MA_INVALID_ARGS;
    }


    /* We must have channel counts for each bus. */
    if ((inputBusCount > 0 && pConfig->pInputChannels == NULL) || (outputBusCount > 0 && pConfig->pOutputChannels == NULL)) {
        return MA_INVALID_ARGS; /* You must specify channel counts for each input and output bus. */
    }


    /* Some special rules for passthrough nodes. */
    if ((pConfig->vtable->flags & MA_NODE_FLAG_PASSTHROUGH) != 0) {
        if (pConfig->vtable->inputBusCount != 1 || pConfig->vtable->outputBusCount != 1) {
            return MA_INVALID_ARGS; /* Passthrough nodes must have exactly 1 input bus and 1 output bus. */
        }

        if (pConfig->pInputChannels[0] != pConfig->pOutputChannels[0]) {
            return MA_INVALID_ARGS; /* Passthrough nodes must have the same number of channels between input and output nodes. */
        }
    }


    *pInputBusCount  = inputBusCount;
    *pOutputBusCount = outputBusCount;

    return MA_SUCCESS;
}

static ma_result ma_node_get_heap_layout(const ma_node_config* pConfig, ma_node_heap_layout* pHeapLayout)
{
    ma_result result;
    ma_uint32 inputBusCount;
    ma_uint32 outputBusCount;

    MA_ASSERT(pHeapLayout != NULL);

    MA_ZERO_OBJECT(pHeapLayout);

    if (pConfig == NULL || pConfig->vtable == NULL || pConfig->vtable->onProcess == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_node_translate_bus_counts(pConfig, &inputBusCount, &outputBusCount);
    if (result != MA_SUCCESS) {
        return result;
    }

    pHeapLayout->sizeInBytes = 0;

    /* Input buses. */
    if (inputBusCount > MA_MAX_NODE_LOCAL_BUS_COUNT) {
        pHeapLayout->inputBusOffset = pHeapLayout->sizeInBytes;
        pHeapLayout->sizeInBytes += ma_align_64(sizeof(ma_node_input_bus) * inputBusCount);
    } else {
        pHeapLayout->inputBusOffset = MA_SIZE_MAX;  /* MA_SIZE_MAX indicates that no heap allocation is required for the input bus. */
    }

    /* Output buses. */
    if (outputBusCount > MA_MAX_NODE_LOCAL_BUS_COUNT) {
        pHeapLayout->outputBusOffset = pHeapLayout->sizeInBytes;
        pHeapLayout->sizeInBytes += ma_align_64(sizeof(ma_node_output_bus) * outputBusCount);
    } else {
        pHeapLayout->outputBusOffset = MA_SIZE_MAX;
    }

    /*
    Cached audio data.

    We need to allocate memory for a caching both input and output data. We have an optimization
    where no caching is necessary for specific conditions:

        - The node has 0 inputs and 1 output.

    When a node meets the above conditions, no cache is allocated.

    The size choice for this buffer is a little bit finicky. We don't want to be too wasteful by
    allocating too much, but at the same time we want it be large enough so that enough frames can
    be processed for each call to ma_node_read_pcm_frames() so that it keeps things efficient. For
    now I'm going with 10ms @ 48K which is 480 frames per bus. This is configurable at compile
    time. It might also be worth investigating whether or not this can be configured at run time.
    */
    if (inputBusCount == 0 && outputBusCount == 1) {
        /* Fast path. No cache needed. */
        pHeapLayout->cachedDataOffset = MA_SIZE_MAX;
    } else {
        /* Slow path. Cache needed. */
        size_t cachedDataSizeInBytes = 0;
        ma_uint32 iBus;

        MA_ASSERT(MA_DEFAULT_NODE_CACHE_CAP_IN_FRAMES_PER_BUS <= 0xFFFF);    /* Clamped to 16 bits. */

        for (iBus = 0; iBus < inputBusCount; iBus += 1) {
            cachedDataSizeInBytes += MA_DEFAULT_NODE_CACHE_CAP_IN_FRAMES_PER_BUS * ma_get_bytes_per_frame(ma_format_f32, pConfig->pInputChannels[iBus]);
        }

        for (iBus = 0; iBus < outputBusCount; iBus += 1) {
            cachedDataSizeInBytes += MA_DEFAULT_NODE_CACHE_CAP_IN_FRAMES_PER_BUS * ma_get_bytes_per_frame(ma_format_f32, pConfig->pOutputChannels[iBus]);
        }

        pHeapLayout->cachedDataOffset = pHeapLayout->sizeInBytes;
        pHeapLayout->sizeInBytes += ma_align_64(cachedDataSizeInBytes);
    }


    /*
    Not technically part of the heap, but we can output the input and output bus counts so we can
    avoid a redundant call to ma_node_translate_bus_counts().
    */
    pHeapLayout->inputBusCount  = inputBusCount;
    pHeapLayout->outputBusCount = outputBusCount;

    return MA_SUCCESS;
}

MA_API ma_result ma_node_get_heap_size(const ma_node_config* pConfig, size_t* pHeapSizeInBytes)
{
    ma_result result;
    ma_node_heap_layout heapLayout;

    if (pHeapSizeInBytes == NULL) {
        return MA_INVALID_ARGS;
    }

    *pHeapSizeInBytes = 0;

    result = ma_node_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    *pHeapSizeInBytes = heapLayout.sizeInBytes;

    return MA_SUCCESS;
}

MA_API ma_result ma_node_init_preallocated(ma_node_graph* pNodeGraph, const ma_node_config* pConfig, void* pHeap, ma_node* pNode)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;
    ma_result result;
    ma_node_heap_layout heapLayout;
    ma_uint32 iInputBus;
    ma_uint32 iOutputBus;

    if (pNodeBase == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNodeBase);

    result = ma_node_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    pNodeBase->_pHeap         = pHeap;
    pNodeBase->pNodeGraph     = pNodeGraph;
    pNodeBase->vtable         = pConfig->vtable;
    pNodeBase->state          = pConfig->initialState;
    pNodeBase->stateTimes[ma_node_state_started] = 0;
    pNodeBase->stateTimes[ma_node_state_stopped] = (ma_uint64)(ma_int64)-1; /* Weird casting for VC6 compatibility. */
    pNodeBase->inputBusCount  = heapLayout.inputBusCount;
    pNodeBase->outputBusCount = heapLayout.outputBusCount;

    if (heapLayout.inputBusOffset != MA_SIZE_MAX) {
        pNodeBase->pInputBuses = (ma_node_input_bus*)ma_offset_ptr(pHeap, heapLayout.inputBusOffset);
    } else {
        pNodeBase->pInputBuses = pNodeBase->_inputBuses;
    }

    if (heapLayout.outputBusOffset != MA_SIZE_MAX) {
        pNodeBase->pOutputBuses = (ma_node_output_bus*)ma_offset_ptr(pHeap, heapLayout.inputBusOffset);
    } else {
        pNodeBase->pOutputBuses = pNodeBase->_outputBuses;
    }

    if (heapLayout.cachedDataOffset != MA_SIZE_MAX) {
        pNodeBase->pCachedData = (float*)ma_offset_ptr(pHeap, heapLayout.cachedDataOffset);
        pNodeBase->cachedDataCapInFramesPerBus = MA_DEFAULT_NODE_CACHE_CAP_IN_FRAMES_PER_BUS;
    } else {
        pNodeBase->pCachedData = NULL;
    }

    

    /* We need to run an initialization step for each input and output bus. */ 
    for (iInputBus = 0; iInputBus < ma_node_get_input_bus_count(pNodeBase); iInputBus += 1) {
        result = ma_node_input_bus_init(pConfig->pInputChannels[iInputBus], &pNodeBase->pInputBuses[iInputBus]);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    for (iOutputBus = 0; iOutputBus < ma_node_get_output_bus_count(pNodeBase); iOutputBus += 1) {
        result = ma_node_output_bus_init(pNodeBase, iOutputBus, pConfig->pOutputChannels[iOutputBus], &pNodeBase->pOutputBuses[iOutputBus]);
        if (result != MA_SUCCESS) {
            return result;
        }
    }


    /* The cached data needs to be initialized to silence (or a sine wave tone if we're debugging). */
    if (pNodeBase->pCachedData != NULL) {
        ma_uint32 iBus;

    #if 1   /* Toggle this between 0 and 1 to turn debugging on or off. 1 = fill with a sine wave for debugging; 0 = fill with silence. */
        /* For safety we'll go ahead and default the buffer to silence. */
        for (iBus = 0; iBus < ma_node_get_input_bus_count(pNodeBase); iBus += 1) {
            ma_silence_pcm_frames(ma_node_get_cached_input_ptr(pNode, iBus), pNodeBase->cachedDataCapInFramesPerBus, ma_format_f32, ma_node_input_bus_get_channels(&pNodeBase->pInputBuses[iBus]));
        }
        for (iBus = 0; iBus < ma_node_get_output_bus_count(pNodeBase); iBus += 1) {
            ma_silence_pcm_frames(ma_node_get_cached_output_ptr(pNode, iBus), pNodeBase->cachedDataCapInFramesPerBus, ma_format_f32, ma_node_output_bus_get_channels(&pNodeBase->pOutputBuses[iBus]));
        }
    #else
        /* For debugging. Default to a sine wave. */
        for (iBus = 0; iBus < ma_node_get_input_bus_count(pNodeBase); iBus += 1) {
            ma_debug_fill_pcm_frames_with_sine_wave(ma_node_get_cached_input_ptr(pNode, iBus), pNodeBase->cachedDataCapInFramesPerBus, ma_format_f32, ma_node_input_bus_get_channels(&pNodeBase->pInputBuses[iBus]), 48000);
        }
        for (iBus = 0; iBus < ma_node_get_output_bus_count(pNodeBase); iBus += 1) {
            ma_debug_fill_pcm_frames_with_sine_wave(ma_node_get_cached_output_ptr(pNode, iBus), pNodeBase->cachedDataCapInFramesPerBus, ma_format_f32, ma_node_output_bus_get_channels(&pNodeBase->pOutputBuses[iBus]), 48000);
        }
    #endif
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_node_init(ma_node_graph* pNodeGraph, const ma_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_node* pNode)
{
    ma_result result;
    size_t heapSizeInBytes;
    void* pHeap;

    result = ma_node_get_heap_size(pConfig, &heapSizeInBytes);
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

    result = ma_node_init_preallocated(pNodeGraph, pConfig, pHeap, pNode);
    if (result != MA_SUCCESS) {
        ma_free(pHeap, pAllocationCallbacks);
        return result;
    }

    ((ma_node_base*)pNode)->_ownsHeap = MA_TRUE;
    return MA_SUCCESS;
}

MA_API void ma_node_uninit(ma_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;

    if (pNodeBase == NULL) {
        return;
    }

    /*
    The first thing we need to do is fully detach the node. This will detach all inputs and
    outputs. We need to do this first because it will sever the connection with the node graph and
    allow us to complete uninitialization without needing to worry about thread-safety with the
    audio thread. The detachment process will wait for any local processing of the node to finish.
    */
    ma_node_detach_full(pNode);

    /*
    At this point the node should be completely unreferenced by the node graph and we can finish up
    the uninitialization process without needing to worry about thread-safety.
    */
    if (pNodeBase->_pHeap != NULL && pNodeBase->_ownsHeap) {
        ma_free(pNodeBase->_pHeap, pAllocationCallbacks);
        pNodeBase->_pHeap;
    }
}

MA_API ma_node_graph* ma_node_get_node_graph(const ma_node* pNode)
{
    if (pNode == NULL) {
        return NULL;
    }

    return ((const ma_node_base*)pNode)->pNodeGraph;
}

MA_API ma_uint32 ma_node_get_input_bus_count(const ma_node* pNode)
{
    if (pNode == NULL) {
        return 0;
    }

    return ((ma_node_base*)pNode)->inputBusCount;
}

MA_API ma_uint32 ma_node_get_output_bus_count(const ma_node* pNode)
{
    if (pNode == NULL) {
        return 0;
    }

    return ((ma_node_base*)pNode)->outputBusCount;
}


MA_API ma_uint32 ma_node_get_input_channels(const ma_node* pNode, ma_uint32 inputBusIndex)
{
    const ma_node_base* pNodeBase = (const ma_node_base*)pNode;

    if (pNode == NULL) {
        return 0;
    }

    if (inputBusIndex >= ma_node_get_input_bus_count(pNode)) {
        return 0;   /* Invalid bus index. */
    }
    
    return ma_node_input_bus_get_channels(&pNodeBase->pInputBuses[inputBusIndex]);
}

MA_API ma_uint32 ma_node_get_output_channels(const ma_node* pNode, ma_uint32 outputBusIndex)
{
    const ma_node_base* pNodeBase = (const ma_node_base*)pNode;

    if (pNode == NULL) {
        return 0;
    }
    
    if (outputBusIndex >= ma_node_get_output_bus_count(pNode)) {
        return 0;   /* Invalid bus index. */
    }

    return ma_node_output_bus_get_channels(&pNodeBase->pOutputBuses[outputBusIndex]);
}


static ma_result ma_node_detach_full(ma_node* pNode)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;
    ma_uint32 iInputBus;

    if (pNodeBase == NULL) {
        return MA_INVALID_ARGS;
    }

    /*
    Make sure the node is completely detached first. This will not return until the output bus is
    guaranteed to no longer be referenced by the audio thread.
    */
    ma_node_detach_all_output_buses(pNode);

    /*
    At this point all output buses will have been detached from the graph and we can be guaranteed
    that none of it's input nodes will be getting processed by the graph. We can detach these
    without needing to worry about the audio thread touching them.
    */
    for (iInputBus = 0; iInputBus < ma_node_get_input_bus_count(pNode); iInputBus += 1) {
        ma_node_input_bus* pInputBus;
        ma_node_output_bus* pOutputBus;

        pInputBus = &pNodeBase->pInputBuses[iInputBus];

        /*
        This is important. We cannot be using ma_node_input_bus_first() or ma_node_input_bus_next(). Those
        functions are specifically for the audio thread. We'll instead just manually iterate using standard
        linked list logic. We don't need to worry about the audio thread referencing these because the step
        above severed the connection to the graph.
        */
        for (pOutputBus = (ma_node_output_bus*)c89atomic_load_ptr(&pInputBus->head.pNext); pOutputBus != NULL; pOutputBus = (ma_node_output_bus*)c89atomic_load_ptr(&pOutputBus->pNext)) {
            ma_node_detach_output_bus(pOutputBus->pNode, pOutputBus->outputBusIndex);   /* This won't do any waiting in practice and should be efficient. */
        }
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_node_detach_output_bus(ma_node* pNode, ma_uint32 outputBusIndex)
{
    ma_result result = MA_SUCCESS;
    ma_node_base* pNodeBase = (ma_node_base*)pNode;
    ma_node_base* pInputNodeBase;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    if (outputBusIndex >= ma_node_get_output_bus_count(pNode)) {
        return MA_INVALID_ARGS; /* Invalid output bus index. */
    }

    /* We need to lock the output bus because we need to inspect the input node and grab it's input bus. */
    ma_node_output_bus_lock(&pNodeBase->pOutputBuses[outputBusIndex]);
    {
        pInputNodeBase = (ma_node_base*)pNodeBase->pOutputBuses[outputBusIndex].pInputNode;
        if (pInputNodeBase != NULL) {
            ma_node_input_bus_detach__no_output_bus_lock(&pInputNodeBase->pInputBuses[pNodeBase->pOutputBuses[outputBusIndex].inputNodeInputBusIndex], &pNodeBase->pOutputBuses[outputBusIndex]);
        }
    }
    ma_node_output_bus_unlock(&pNodeBase->pOutputBuses[outputBusIndex]);

    return result;
}

MA_API ma_result ma_node_detach_all_output_buses(ma_node* pNode)
{
    ma_uint32 iOutputBus;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    for (iOutputBus = 0; iOutputBus < ma_node_get_output_bus_count(pNode); iOutputBus += 1) {
        ma_node_detach_output_bus(pNode, iOutputBus);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_node_attach_output_bus(ma_node* pNode, ma_uint32 outputBusIndex, ma_node* pOtherNode, ma_uint32 otherNodeInputBusIndex)
{
    ma_node_base* pNodeBase  = (ma_node_base*)pNode;
    ma_node_base* pOtherNodeBase = (ma_node_base*)pOtherNode;

    if (pNodeBase == NULL || pOtherNodeBase == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pNodeBase == pOtherNodeBase) {
        return MA_INVALID_OPERATION;    /* Cannot attach a node to itself. */
    }

    if (outputBusIndex >= ma_node_get_output_bus_count(pNode) || otherNodeInputBusIndex >= ma_node_get_input_bus_count(pOtherNode)) {
        return MA_INVALID_OPERATION;    /* Invalid bus index. */
    }

    /* The output channel count of the output node must be the same as the input channel count of the input node. */
    if (ma_node_get_output_channels(pNode, outputBusIndex) != ma_node_get_input_channels(pOtherNode, otherNodeInputBusIndex)) {
        return MA_INVALID_OPERATION;    /* Channel count is incompatible. */
    }

    /* This will deal with detaching if the output bus is already attached to something. */
    ma_node_input_bus_attach(&pOtherNodeBase->pInputBuses[otherNodeInputBusIndex], &pNodeBase->pOutputBuses[outputBusIndex], pOtherNode, otherNodeInputBusIndex);

    return MA_SUCCESS;
}

MA_API ma_result ma_node_set_output_bus_volume(ma_node* pNode, ma_uint32 outputBusIndex, float volume)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;

    if (pNodeBase == NULL) {
        return MA_INVALID_ARGS;
    }

    if (outputBusIndex >= ma_node_get_output_bus_count(pNode)) {
        return MA_INVALID_ARGS; /* Invalid bus index. */
    }

    return ma_node_output_bus_set_volume(&pNodeBase->pOutputBuses[outputBusIndex], volume);
}

MA_API float ma_node_get_output_bus_volume(const ma_node* pNode, ma_uint32 outputBusIndex)
{
    const ma_node_base* pNodeBase = (const ma_node_base*)pNode;

    if (pNodeBase == NULL) {
        return 0;
    }

    if (outputBusIndex >= ma_node_get_output_bus_count(pNode)) {
        return 0;   /* Invalid bus index. */
    }

    return ma_node_output_bus_get_volume(&pNodeBase->pOutputBuses[outputBusIndex]);
}

MA_API ma_result ma_node_set_state(ma_node* pNode, ma_node_state state)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;

    if (pNodeBase == NULL) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_i32(&pNodeBase->state, state);

    return MA_SUCCESS;
}

MA_API ma_node_state ma_node_get_state(const ma_node* pNode)
{
    const ma_node_base* pNodeBase = (const ma_node_base*)pNode;

    if (pNodeBase == NULL) {
        return ma_node_state_stopped;
    }

    return (ma_node_state)c89atomic_load_i32(&pNodeBase->state);
}

MA_API ma_result ma_node_set_state_time(ma_node* pNode, ma_node_state state, ma_uint64 globalTime)
{
    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Validation check for safety since we'll be using this as an index into stateTimes[]. */
    if (state != ma_node_state_started && state != ma_node_state_stopped) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_64(&((ma_node_base*)pNode)->stateTimes[state], globalTime);

    return MA_SUCCESS;
}

MA_API ma_uint64 ma_node_get_state_time(const ma_node* pNode, ma_node_state state)
{
    if (pNode == NULL) {
        return 0;
    }

    /* Validation check for safety since we'll be using this as an index into stateTimes[]. */
    if (state != ma_node_state_started && state != ma_node_state_stopped) {
        return 0;
    }

    return c89atomic_load_64(&((ma_node_base*)pNode)->stateTimes[state]);
}

MA_API ma_node_state ma_node_get_state_by_time(const ma_node* pNode, ma_uint64 globalTime)
{
    if (pNode == NULL) {
        return ma_node_state_stopped;
    }

    return ma_node_get_state_by_time_range(pNode, globalTime, globalTime);
}

MA_API ma_node_state ma_node_get_state_by_time_range(const ma_node* pNode, ma_uint64 globalTimeBeg, ma_uint64 globalTimeEnd)
{
    ma_node_state state;

    if (pNode == NULL) {
        return ma_node_state_stopped;
    }

    state = ma_node_get_state(pNode);

    /* An explicitly stopped node is always stopped. */
    if (state == ma_node_state_stopped) {
        return ma_node_state_stopped;
    }

    /*
    Getting here means the node is marked as started, but it may still not be truly started due to
    it's start time not having been reached yet. Also, the stop time may have also been reached in
    which case it'll be considered stopped.
    */
    if (ma_node_get_state_time(pNode, ma_node_state_started) > globalTimeBeg) {
        return ma_node_state_stopped;   /* Start time has not yet been reached. */
    }

    if (ma_node_get_state_time(pNode, ma_node_state_stopped) <= globalTimeEnd) {
        return ma_node_state_stopped;   /* Stop time has been reached. */
    }

    /* Getting here means the node is marked as started and is within it's start/stop times. */
    return ma_node_state_started;
}

MA_API ma_uint64 ma_node_get_time(const ma_node* pNode)
{
    if (pNode == NULL) {
        return 0;
    }

    return c89atomic_load_64(&((ma_node_base*)pNode)->localTime);
}

MA_API ma_result ma_node_set_time(ma_node* pNode, ma_uint64 localTime)
{
    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_64(&((ma_node_base*)pNode)->localTime, localTime);

    return MA_SUCCESS;
}



static void ma_node_process_pcm_frames_internal(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;

    MA_ASSERT(pNode != NULL);

    if (pNodeBase->vtable->onProcess) {
        pNodeBase->vtable->onProcess(pNode, ppFramesIn, pFrameCountIn, ppFramesOut, pFrameCountOut);
    }
}

static ma_result ma_node_read_pcm_frames(ma_node* pNode, ma_uint32 outputBusIndex, float* pFramesOut, ma_uint32 frameCount, ma_uint32* pFramesRead, ma_uint64 globalTime)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;
    ma_result result = MA_SUCCESS;
    ma_uint32 iInputBus;
    ma_uint32 iOutputBus;
    ma_uint32 inputBusCount;
    ma_uint32 outputBusCount;
    ma_uint32 totalFramesRead = 0;
    float* ppFramesIn[MA_MAX_NODE_BUS_COUNT];
    float* ppFramesOut[MA_MAX_NODE_BUS_COUNT];
    ma_uint64 globalTimeBeg;
    ma_uint64 globalTimeEnd;
    ma_uint64 startTime;
    ma_uint64 stopTime;
    ma_uint32 timeOffsetBeg;
    ma_uint32 timeOffsetEnd;
    ma_uint32 frameCountIn;
    ma_uint32 frameCountOut;

    /*
    pFramesRead is mandatory. It must be used to determine how many frames were read. It's normal and
    expected that the number of frames read may be different to that requested. Therefore, the caller
    must look at this value to correctly determine how many frames were read.
    */
    MA_ASSERT(pFramesRead != NULL); /* <-- If you've triggered this assert, you're using this function wrong. You *must* use this variable and inspect it after the call returns. */
    if (pFramesRead == NULL) {
        return MA_INVALID_ARGS;
    }

    *pFramesRead = 0;   /* Safety. */

    if (pNodeBase == NULL) {
        return MA_INVALID_ARGS;
    }

    if (outputBusIndex >= ma_node_get_output_bus_count(pNodeBase)) {
        return MA_INVALID_ARGS; /* Invalid output bus index. */
    }

    /* Don't do anything if we're in a stopped state. */
    if (ma_node_get_state_by_time_range(pNode, globalTime, globalTime + frameCount) != ma_node_state_started) {
        return MA_SUCCESS;  /* We're in a stopped state. This is not an error - we just need to not read anything. */
    }


    globalTimeBeg = globalTime;
    globalTimeEnd = globalTime + frameCount;
    startTime = ma_node_get_state_time(pNode, ma_node_state_started);
    stopTime  = ma_node_get_state_time(pNode, ma_node_state_stopped);

    /*
    At this point we know that we are inside our start/stop times. However, we may need to adjust
    our frame count and output pointer to accomodate since we could be straddling the time period
    that this function is getting called for.

    It's possible (and likely) that the start time does not line up with the output buffer. We
    therefore need to offset it by a number of frames to accomodate. The same thing applies for
    the stop time.
    */
    timeOffsetBeg = (globalTimeBeg < startTime) ? (ma_uint32)(globalTimeEnd - startTime) : 0;
    timeOffsetEnd = (globalTimeEnd > stopTime)  ? (ma_uint32)(globalTimeEnd - stopTime)  : 0;

    /* Trim based on the start offset. We need to silence the start of the buffer. */
    if (timeOffsetBeg > 0) {
        ma_silence_pcm_frames(pFramesOut, timeOffsetBeg, ma_format_f32, ma_node_get_output_channels(pNode, outputBusIndex));
        pFramesOut += timeOffsetBeg * ma_node_get_output_channels(pNode, outputBusIndex);
        frameCount -= timeOffsetBeg;
    }

    /* Trim based on the end offset. We don't need to silence the tail section because we'll just have a reduced value written to pFramesRead. */
    if (timeOffsetEnd > 0) {
        frameCount -= timeOffsetEnd;
    }


    /* We run on different paths depending on the bus counts. */
    inputBusCount  = ma_node_get_input_bus_count(pNode);
    outputBusCount = ma_node_get_output_bus_count(pNode);

    /*
    Run a simplified path when there are no inputs and one output. In this case there's nothing to
    actually read and we can go straight to output. This is a very common scenario because the vast
    majority of data source nodes will use this setup so this optimization I think is worthwhile.
    */
    if (inputBusCount == 0 && outputBusCount == 1) {
        /* Fast path. No need to read from input and no need for any caching. */
        frameCountIn  = 0;
        frameCountOut = frameCount;    /* Just read as much as we can. The callback will return what was actually read. */

        /* Don't do anything if our read counter is ahead of the node graph. That means we're */
        ppFramesOut[0] = pFramesOut;
        ma_node_process_pcm_frames_internal(pNode, NULL, &frameCountIn, ppFramesOut, &frameCountOut);
        totalFramesRead = frameCountOut;
    } else {
        /* Slow path. Need to read input data. */
        if ((pNodeBase->vtable->flags & MA_NODE_FLAG_PASSTHROUGH) != 0) {
            /*
            Fast path. We're running a passthrough. We need to read directly into the output buffer, but
            still fire the callback so that event handling and trigger nodes can do their thing. Since
            it's a passthrough there's no need for any kind of caching logic.
            */
            MA_ASSERT(outputBusCount == inputBusCount);
            MA_ASSERT(outputBusCount == 1);
            MA_ASSERT(outputBusIndex == 0);

            /* We just read directly from input bus to output buffer, and then afterwards fire the callback. */
            ppFramesOut[0] = pFramesOut;
            ppFramesIn[0] = ppFramesOut[0];

            result = ma_node_input_bus_read_pcm_frames(pNodeBase, &pNodeBase->pInputBuses[0], ppFramesIn[0], frameCount, &totalFramesRead, globalTime);
            if (result == MA_SUCCESS) {
                /* Even though it's a passthrough, we still need to fire the callback. */
                frameCountIn  = totalFramesRead;
                frameCountOut = totalFramesRead;

                if (totalFramesRead > 0) {
                    ma_node_process_pcm_frames_internal(pNode, (const float**)ppFramesIn, &frameCountIn, ppFramesOut, &frameCountOut);  /* From GCC: expected 'const float **' but argument is of type 'float **'. Shouldn't this be implicit? Excplicit cast to silence the warning. */
                }

                /*
                A passthrough should never have modified the input and output frame counts. If you're
                triggering these assers you need to fix your processing callback.
                */
                MA_ASSERT(frameCountIn  == totalFramesRead);
                MA_ASSERT(frameCountOut == totalFramesRead);
            }
        } else {
            /* Slow path. Need to do caching. */
            ma_uint32 framesToProcessIn;
            ma_uint32 framesToProcessOut;
            ma_bool32 consumeNullInput = MA_FALSE;

            /*
            We use frameCount as a basis for the number of frames to read since that's what's being
            requested, however we still need to clamp it to whatever can fit in the cache.

            This will also be used as the basis for determining how many input frames to read. This is
            not ideal because it can result in too many input frames being read which introduces latency.
            To solve this, nodes can implement an optional callback called onGetRequiredInputFrameCount
            which is used as hint to miniaudio as to how many input frames it needs to read at a time. This
            callback is completely optional, and if it's not set, miniaudio will assume `frameCount`.

            This function will be called multiple times for each period of time, once for each output node.
            We cannot read from each input node each time this function is called. Instead we need to check
            whether or not this is first output bus to be read from for this time period, and if so, read
            from our input data.

            To determine whether or not we're ready to read data, we check a flag. There will be one flag
            for each output. When the flag is set, it means data has been read previously and that we're
            ready to advance time forward for our input nodes by reading fresh data.
            */
            framesToProcessOut = frameCount;
            if (framesToProcessOut > pNodeBase->cachedDataCapInFramesPerBus) {
                framesToProcessOut = pNodeBase->cachedDataCapInFramesPerBus;
            }

            framesToProcessIn  = frameCount;
            if (pNodeBase->vtable->onGetRequiredInputFrameCount) {
                pNodeBase->vtable->onGetRequiredInputFrameCount(pNode, framesToProcessOut, &framesToProcessIn); /* <-- It does not matter if this fails. */
            }
            if (framesToProcessIn > pNodeBase->cachedDataCapInFramesPerBus) {
                framesToProcessIn = pNodeBase->cachedDataCapInFramesPerBus;
            }
            

            MA_ASSERT(framesToProcessIn  <= 0xFFFF);
            MA_ASSERT(framesToProcessOut <= 0xFFFF);

            if (ma_node_output_bus_has_read(&pNodeBase->pOutputBuses[outputBusIndex])) {
                /* Getting here means we need to do another round of processing. */
                pNodeBase->cachedFrameCountOut = 0;

                /*
                We need to prepare our output frame pointers for processing. In the same iteration we need
                to mark every output bus as unread so that future calls to this function for different buses
                for the current time period don't pull in data when they should instead be reading from cache.
                */
                for (iOutputBus = 0; iOutputBus < outputBusCount; iOutputBus += 1) {
                    ma_node_output_bus_set_has_read(&pNodeBase->pOutputBuses[iOutputBus], MA_FALSE); /* <-- This is what tells the next calls to this function for other output buses for this time period to read from cache instead of pulling in more data. */
                    ppFramesOut[iOutputBus] = ma_node_get_cached_output_ptr(pNode, iOutputBus);
                }

                /* We only need to read from input buses if there isn't already some data in the cache. */
                if (pNodeBase->cachedFrameCountIn == 0) {
                    ma_uint32 maxFramesReadIn = 0;

                    /* Here is where we pull in data from the input buses. This is what will trigger an advance in time. */
                    for (iInputBus = 0; iInputBus < inputBusCount; iInputBus += 1) {
                        ma_uint32 framesRead;

                        /* The first thing to do is get the offset within our bulk allocation to store this input data. */
                        ppFramesIn[iInputBus] = ma_node_get_cached_input_ptr(pNode, iInputBus);

                        /* Once we've determined our destination pointer we can read. Note that we must inspect the number of frames read and fill any leftovers with silence for safety. */
                        result = ma_node_input_bus_read_pcm_frames(pNodeBase, &pNodeBase->pInputBuses[iInputBus], ppFramesIn[iInputBus], framesToProcessIn, &framesRead, globalTime);
                        if (result != MA_SUCCESS) {
                            /* It doesn't really matter if we fail because we'll just fill with silence. */
                            framesRead = 0; /* Just for safety, but I don't think it's really needed. */
                        }

                        /* TODO: Minor optimization opportunity here. If no frames were read and the buffer is already filled with silence, no need to re-silence it. */
                        /* Any leftover frames need to silenced for safety. */
                        if (framesRead < framesToProcessIn) {
                            ma_silence_pcm_frames(ppFramesIn[iInputBus] + (framesRead * ma_node_get_input_channels(pNodeBase, iInputBus)), (framesToProcessIn - framesRead), ma_format_f32, ma_node_get_input_channels(pNodeBase, iInputBus));
                        }

                        maxFramesReadIn = ma_max(maxFramesReadIn, framesRead);
                    }

                    /* This was a fresh load of input data so reset our consumption counter. */
                    pNodeBase->consumedFrameCountIn = 0;

                    /*
                    We don't want to keep processing if there's nothing to process, so set the number of cached
                    input frames to the maximum number we read from each attachment (the lesser will be padded
                    with silence). If we didn't read anything, this will be set to 0 and the entire buffer will
                    have been assigned to silence. This being equal to 0 is an important property for us because
                    it allows us to detect when NULL can be passed into the processing callback for the input
                    buffer for the purpose of continuous processing.
                    */
                    pNodeBase->cachedFrameCountIn = (ma_uint16)maxFramesReadIn;
                } else {
                    /* We don't need to read anything, but we do need to prepare our input frame pointers. */
                    for (iInputBus = 0; iInputBus < inputBusCount; iInputBus += 1) {
                        ppFramesIn[iInputBus] = ma_node_get_cached_input_ptr(pNode, iInputBus) + (pNodeBase->consumedFrameCountIn * ma_node_get_input_channels(pNodeBase, iInputBus));
                    }
                }

                /*
                At this point we have our input data so now we need to do some processing. Sneaky little
                optimization here - we can set the pointer to the output buffer for this output bus so
                that the final copy into the output buffer is done directly by onProcess().
                */
                if (pFramesOut != NULL) {
                    ppFramesOut[outputBusIndex] = pFramesOut;
                }


                /* Give the processing function the entire capacity of the output buffer. */
                frameCountOut = framesToProcessOut;

                /*
                We need to treat nodes with continuous processing a little differently. For these ones,
                we always want to fire the callback with the requested number of frames, regardless of
                pNodeBase->cachedFrameCountIn, which could be 0. Also, we want to check if we can pass
                in NULL for the input buffer to the callback.
                */
                if ((pNodeBase->vtable->flags & MA_NODE_FLAG_CONTINUOUS_PROCESSING) != 0) {
                    /* We're using continuous processing. Make sure we specify the whole frame count at all times. */
                    frameCountIn = framesToProcessIn;    /* Give the processing function as much input data as we've got in the buffer. */

                    if ((pNodeBase->vtable->flags & MA_NODE_FLAG_ALLOW_NULL_INPUT) != 0 && pNodeBase->consumedFrameCountIn == 0 && pNodeBase->cachedFrameCountIn == 0) {
                        consumeNullInput = MA_TRUE;
                    } else {
                        consumeNullInput = MA_FALSE;
                    }
                } else {
                    frameCountIn = pNodeBase->cachedFrameCountIn;  /* Give the processing function as much valid input data as we've got. */
                    consumeNullInput = MA_FALSE;
                }

                /*
                Process data slightly differently depending on whether or not we're consuming NULL
                input (checked just above).
                */
                if (consumeNullInput) {
                    ma_node_process_pcm_frames_internal(pNode, NULL, &frameCountIn, ppFramesOut, &frameCountOut);
                } else {
                    /*
                    We want to skip processing if there's no input data, but we can only do that safely if
                    we know that there is no chance of any output frames being produced. If continuous
                    processing is being used, this won't be a problem because the input frame count will
                    always be non-0. However, if continuous processing is *not* enabled and input and output
                    data is processed at different rates, we still need to process that last input frame
                    because there could be a few excess output frames needing to be produced from cached
                    data. The `MA_NODE_FLAG_DIFFERENT_PROCESSING_RATES` flag is used as the indicator for
                    determining whether or not we need to process the node even when there are no input
                    frames available right now.
                    */
                    if (frameCountIn > 0 || (pNodeBase->vtable->flags & MA_NODE_FLAG_DIFFERENT_PROCESSING_RATES) != 0) {
                        ma_node_process_pcm_frames_internal(pNode, (const float**)ppFramesIn, &frameCountIn, ppFramesOut, &frameCountOut);    /* From GCC: expected 'const float **' but argument is of type 'float **'. Shouldn't this be implicit? Excplicit cast to silence the warning. */
                    }
                }
            
                /*
                Thanks to our sneaky optimization above we don't need to do any data copying directly into
                the output buffer - the onProcess() callback just did that for us. We do, however, need to
                apply the number of input and output frames that were processed. Note that due to continuous
                processing above, we need to do explicit checks here. If we just consumed a NULL input
                buffer it means that no actual input data was processed from the internal buffers and we
                don't want to be modifying any counters.
                */
                if (consumeNullInput == MA_FALSE) {
                    pNodeBase->consumedFrameCountIn += (ma_uint16)frameCountIn;
                    pNodeBase->cachedFrameCountIn   -= (ma_uint16)frameCountIn;
                }

                /* The cached output frame count is always equal to what we just read. */
                pNodeBase->cachedFrameCountOut = (ma_uint16)frameCountOut;
            } else {
                /*
                We're not needing to read anything from the input buffer so just read directly from our
                already-processed data.
                */
                if (pFramesOut != NULL) {
                    ma_copy_pcm_frames(pFramesOut, ma_node_get_cached_output_ptr(pNodeBase, outputBusIndex), pNodeBase->cachedFrameCountOut, ma_format_f32, ma_node_get_output_channels(pNodeBase, outputBusIndex));
                }
            }

            /* The number of frames read is always equal to the number of cached output frames. */
            totalFramesRead = pNodeBase->cachedFrameCountOut;

            /* Now that we've read the data, make sure our read flag is set. */
            ma_node_output_bus_set_has_read(&pNodeBase->pOutputBuses[outputBusIndex], MA_TRUE);
        }
    }

    /* Advance our local time forward. */
    c89atomic_fetch_add_64(&pNodeBase->localTime, (ma_uint64)totalFramesRead);

    *pFramesRead = totalFramesRead + timeOffsetBeg; /* Must include the silenced section at the start of the buffer. */
    return result;
}




/* Data source node. */
MA_API ma_data_source_node_config ma_data_source_node_config_init(ma_data_source* pDataSource, ma_bool32 looping)
{
    ma_data_source_node_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig  = ma_node_config_init();
    config.pDataSource = pDataSource;
    config.looping     = looping;

    return config;
}


static void ma_data_source_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_data_source_node* pDataSourceNode = (ma_data_source_node*)pNode;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 frameCount;
    ma_uint64 framesRead = 0;

    MA_ASSERT(pDataSourceNode != NULL);
    MA_ASSERT(pDataSourceNode->pDataSource != NULL);
    MA_ASSERT(ma_node_get_input_bus_count(pDataSourceNode)  == 0);
    MA_ASSERT(ma_node_get_output_bus_count(pDataSourceNode) == 1);

    /* We don't want to read from ppFramesIn at all. Instead we read from the data source. */
    (void)ppFramesIn;
    (void)pFrameCountIn;

    frameCount = *pFrameCountOut;

    if (ma_data_source_get_data_format(pDataSourceNode->pDataSource, &format, &channels, NULL, NULL, 0) == MA_SUCCESS) { /* <-- Don't care about sample rate here. */
        /* The node graph system requires samples be in floating point format. This is checked in ma_data_source_node_init(). */
        MA_ASSERT(format == ma_format_f32);
        (void)format;   /* Just to silence some static analysis tools. */

        ma_data_source_read_pcm_frames(pDataSourceNode->pDataSource, ppFramesOut[0], frameCount, &framesRead, c89atomic_load_32(&pDataSourceNode->looping));
    }

    *pFrameCountOut = (ma_uint32)framesRead;
}

static ma_node_vtable g_ma_data_source_node_vtable =
{
    ma_data_source_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    0,      /* 0 input buses. */
    1,      /* 1 output bus. */
    0
};

MA_API ma_result ma_data_source_node_init(ma_node_graph* pNodeGraph, const ma_data_source_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source_node* pDataSourceNode)
{
    ma_result result;
    ma_format format;   /* For validating the format, which must be ma_format_f32. */
    ma_uint32 channels; /* For specifying the channel count of the output bus. */
    ma_node_config baseConfig;

    if (pDataSourceNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataSourceNode);
    
    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_data_source_get_data_format(pConfig->pDataSource, &format, &channels, NULL, NULL, 0);    /* Don't care about sample rate. This will check pDataSource for NULL. */
    if (result != MA_SUCCESS) {
        return result;
    }

    MA_ASSERT(format == ma_format_f32); /* <-- If you've triggered this it means your data source is not outputting floating-point samples. You must configure your data source to use ma_format_f32. */
    if (format != ma_format_f32) {
        return MA_INVALID_ARGS; /* Invalid format. */
    }

    /* The channel count is defined by the data source. If the caller has manually changed the channels we just ignore it. */
    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable = &g_ma_data_source_node_vtable;  /* Explicitly set the vtable here to prevent callers from setting it incorrectly. */

    /*
    The channel count is defined by the data source. It is invalid for the caller to manually set
    the channel counts in the config. `ma_data_source_node_config_init()` will have defaulted the
    channel count pointer to NULL which is how it must remain. If you trigger any of these asserts
    it means you're explicitly setting the channel count. Instead, configure the output channel
    count of your data source to be the necessary channel count.
    */
    if (baseConfig.pOutputChannels != NULL) {
        return MA_INVALID_ARGS;
    }

    baseConfig.pOutputChannels = &channels;

    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pDataSourceNode->base);
    if (result != MA_SUCCESS) {
        return result;
    }

    pDataSourceNode->pDataSource = pConfig->pDataSource;
    pDataSourceNode->looping     = pConfig->looping;

    return MA_SUCCESS;
}

MA_API void ma_data_source_node_uninit(ma_data_source_node* pDataSourceNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(&pDataSourceNode->base, pAllocationCallbacks);
}

MA_API ma_result ma_data_source_node_set_looping(ma_data_source_node* pDataSourceNode, ma_bool32 looping)
{
    if (pDataSourceNode == NULL) {
        return MA_INVALID_ARGS;
    }

    c89atomic_exchange_32(&pDataSourceNode->looping, looping);

    return MA_SUCCESS;
}

MA_API ma_bool32 ma_data_source_node_is_looping(ma_data_source_node* pDataSourceNode)
{
    if (pDataSourceNode == NULL) {
        return MA_FALSE;
    }

    return c89atomic_load_32(&pDataSourceNode->looping);
}



/* Splitter Node. */
MA_API ma_splitter_node_config ma_splitter_node_config_init(ma_uint32 channels)
{
    ma_splitter_node_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_node_config_init();
    config.channels   = channels;

    return config;
}


static void ma_splitter_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_node_base* pNodeBase = (ma_node_base*)pNode;
    ma_uint32 iOutputBus;
    ma_uint32 channels;

    MA_ASSERT(pNodeBase != NULL);
    MA_ASSERT(ma_node_get_input_bus_count(pNodeBase)  == 1);
    MA_ASSERT(ma_node_get_output_bus_count(pNodeBase) >= 2);

    /* We don't need to consider the input frame count - it'll be the same as the output frame count and we process everything. */
    (void)pFrameCountIn;

    /* NOTE: This assumes the same number of channels for all inputs and outputs. This was checked in ma_splitter_node_init(). */
    channels = ma_node_get_input_channels(pNodeBase, 0);

    /* Splitting is just copying the first input bus and copying it over to each output bus. */
    for (iOutputBus = 0; iOutputBus < ma_node_get_output_bus_count(pNodeBase); iOutputBus += 1) {
        ma_copy_pcm_frames(ppFramesOut[iOutputBus], ppFramesIn[0], *pFrameCountOut, ma_format_f32, channels);
    }
}

static ma_node_vtable g_ma_splitter_node_vtable =
{
    ma_splitter_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* 1 input bus. */
    2,      /* 2 output buses. */
    0
};

MA_API ma_result ma_splitter_node_init(ma_node_graph* pNodeGraph, const ma_splitter_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_splitter_node* pSplitterNode)
{
    ma_result result;
    ma_node_config baseConfig;
    ma_uint32 pInputChannels[1];
    ma_uint32 pOutputChannels[2];

    if (pSplitterNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pSplitterNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Splitters require the same number of channels between inputs and outputs. */
    pInputChannels[0]  = pConfig->channels;
    pOutputChannels[0] = pConfig->channels;
    pOutputChannels[1] = pConfig->channels;

    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable = &g_ma_splitter_node_vtable;
    baseConfig.pInputChannels  = pInputChannels;
    baseConfig.pOutputChannels = pOutputChannels;

    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pSplitterNode->base);
    if (result != MA_SUCCESS) {
        return result;  /* Failed to initialize the base node. */
    }

    return MA_SUCCESS;
}

MA_API void ma_splitter_node_uninit(ma_splitter_node* pSplitterNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pSplitterNode, pAllocationCallbacks);
}





#ifndef MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS
#define MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS   1000
#endif

MA_API ma_result ma_async_notification_signal(ma_async_notification* pNotification)
{
    ma_async_notification_callbacks* pNotificationCallbacks = (ma_async_notification_callbacks*)pNotification;

    if (pNotification == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pNotificationCallbacks->onSignal == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    pNotificationCallbacks->onSignal(pNotification);
    return MA_INVALID_ARGS;
}


static void ma_async_notification_poll__on_signal(ma_async_notification* pNotification)
{
    ((ma_async_notification_poll*)pNotification)->signalled = MA_TRUE;
}

MA_API ma_result ma_async_notification_poll_init(ma_async_notification_poll* pNotificationPoll)
{
    if (pNotificationPoll == NULL) {
        return MA_INVALID_ARGS;
    }

    pNotificationPoll->cb.onSignal = ma_async_notification_poll__on_signal;
    pNotificationPoll->signalled = MA_FALSE;

    return MA_SUCCESS;
}

MA_API ma_bool32 ma_async_notification_poll_is_signalled(const ma_async_notification_poll* pNotificationPoll)
{
    if (pNotificationPoll == NULL) {
        return MA_FALSE;
    }

    return pNotificationPoll->signalled;
}


static void ma_async_notification_event__on_signal(ma_async_notification* pNotification)
{
    ma_async_notification_event_signal((ma_async_notification_event*)pNotification);
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



MA_API ma_resource_manager_pipeline_notifications ma_resource_manager_pipeline_notifications_init(void)
{
    ma_resource_manager_pipeline_notifications notifications;

    MA_ZERO_OBJECT(&notifications);

    return notifications;
}

static void ma_resource_manager_pipeline_notifications_signal_all_notifications(const ma_resource_manager_pipeline_notifications* pPipelineNotifications)
{
    if (pPipelineNotifications == NULL) {
        return;
    }

    if (pPipelineNotifications->init.pNotification) { ma_async_notification_signal(pPipelineNotifications->init.pNotification); }
    if (pPipelineNotifications->done.pNotification) { ma_async_notification_signal(pPipelineNotifications->done.pNotification); }
}

static void ma_resource_manager_pipeline_notifications_acquire_all_fences(const ma_resource_manager_pipeline_notifications* pPipelineNotifications)
{
    if (pPipelineNotifications == NULL) {
        return;
    }

    if (pPipelineNotifications->init.pFence != NULL) { ma_fence_acquire(pPipelineNotifications->init.pFence); }
    if (pPipelineNotifications->done.pFence != NULL) { ma_fence_acquire(pPipelineNotifications->done.pFence); }
}

static void ma_resource_manager_pipeline_notifications_release_all_fences(const ma_resource_manager_pipeline_notifications* pPipelineNotifications)
{
    if (pPipelineNotifications == NULL) {
        return;
    }

    if (pPipelineNotifications->init.pFence != NULL) { ma_fence_release(pPipelineNotifications->init.pFence); }
    if (pPipelineNotifications->done.pFence != NULL) { ma_fence_release(pPipelineNotifications->done.pFence); }
}



#define MA_RESOURCE_MANAGER_JOB_ID_NONE      ~((ma_uint64)0)
#define MA_RESOURCE_MANAGER_JOB_SLOT_NONE    (ma_uint16)(~0)

static MA_INLINE ma_uint32 ma_resource_manager_job_extract_refcount(ma_uint64 toc)
{
    return (ma_uint32)(toc >> 32);
}

static MA_INLINE ma_uint16 ma_resource_manager_job_extract_slot(ma_uint64 toc)
{
    return (ma_uint16)(toc & 0x0000FFFF);
}

static MA_INLINE ma_uint16 ma_resource_manager_job_extract_code(ma_uint64 toc)
{
    return (ma_uint16)((toc & 0xFFFF0000) >> 16);
}

static MA_INLINE ma_uint64 ma_resource_manager_job_toc_to_allocation(ma_uint64 toc)
{
    return ((ma_uint64)ma_resource_manager_job_extract_refcount(toc) << 32) | (ma_uint64)ma_resource_manager_job_extract_slot(toc);
}


MA_API ma_resource_manager_job ma_resource_manager_job_init(ma_uint16 code)
{
    ma_resource_manager_job job;
    
    MA_ZERO_OBJECT(&job);
    job.toc.breakup.code = code;
    job.toc.breakup.slot = MA_RESOURCE_MANAGER_JOB_SLOT_NONE;    /* Temp value. Will be allocated when posted to a queue. */
    job.next             = MA_RESOURCE_MANAGER_JOB_ID_NONE;

    return job;
}



MA_API ma_resource_manager_job_queue_config ma_resource_manager_job_queue_config_init(ma_uint32 flags, ma_uint32 capacity)
{
    ma_resource_manager_job_queue_config config;

    config.flags    = flags;
    config.capacity = capacity;

    return config;
}


typedef struct
{
    size_t sizeInBytes;
    size_t allocatorOffset;
    size_t jobsOffset;
} ma_resource_manager_job_queue_heap_layout;

static ma_result ma_resource_manager_job_queue_get_heap_layout(const ma_resource_manager_job_queue_config* pConfig, ma_resource_manager_job_queue_heap_layout* pHeapLayout)
{
    ma_result result;

    MA_ASSERT(pHeapLayout != NULL);

    MA_ZERO_OBJECT(pHeapLayout);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->capacity == 0) {
        return MA_INVALID_ARGS;
    }

    pHeapLayout->sizeInBytes = 0;

    /* Allocator. */
    {
        ma_slot_allocator_config allocatorConfig;
        size_t allocatorHeapSizeInBytes;

        allocatorConfig = ma_slot_allocator_config_init(pConfig->capacity);
        result = ma_slot_allocator_get_heap_size(&allocatorConfig, &allocatorHeapSizeInBytes);
        if (result != MA_SUCCESS) {
            return result;
        }

        pHeapLayout->allocatorOffset = pHeapLayout->sizeInBytes;
        pHeapLayout->sizeInBytes    += allocatorHeapSizeInBytes;
    }
    
    /* Jobs. */
    pHeapLayout->jobsOffset   = pHeapLayout->sizeInBytes;
    pHeapLayout->sizeInBytes += ma_align_64(pConfig->capacity * sizeof(ma_resource_manager_job));

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_job_queue_get_heap_size(const ma_resource_manager_job_queue_config* pConfig, size_t* pHeapSizeInBytes)
{
    ma_result result;
    ma_resource_manager_job_queue_heap_layout layout;

    if (pHeapSizeInBytes == NULL) {
        return MA_INVALID_ARGS;
    }

    *pHeapSizeInBytes = 0;

    result = ma_resource_manager_job_queue_get_heap_layout(pConfig, &layout);
    if (result != MA_SUCCESS) {
        return result;
    }

    *pHeapSizeInBytes = layout.sizeInBytes;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_job_queue_init_preallocated(const ma_resource_manager_job_queue_config* pConfig, void* pHeap, ma_resource_manager_job_queue* pQueue)
{
    ma_result result;
    ma_resource_manager_job_queue_heap_layout heapLayout;
    ma_slot_allocator_config allocatorConfig;

    if (pQueue == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pQueue);

    result = ma_resource_manager_job_queue_get_heap_layout(pConfig, &heapLayout);
    if (result != MA_SUCCESS) {
        return result;
    }

    pQueue->_pHeap   = pHeap;
    pQueue->flags    = pConfig->flags;
    pQueue->capacity = pConfig->capacity;
    pQueue->pJobs    = (ma_resource_manager_job*)ma_offset_ptr(pHeap, heapLayout.jobsOffset);

    allocatorConfig = ma_slot_allocator_config_init(pConfig->capacity);
    result = ma_slot_allocator_init_preallocated(&allocatorConfig, ma_offset_ptr(pHeap, heapLayout.allocatorOffset), &pQueue->allocator);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* We need a semaphore if we're running in synchronous mode. */
    if ((pQueue->flags & MA_RESOURCE_MANAGER_JOB_QUEUE_FLAG_NON_BLOCKING) == 0) {
        ma_semaphore_init(0, &pQueue->sem);
    }

    /*
    Our queue needs to be initialized with a free standing node. This should always be slot 0. Required for the lock free algorithm. The first job in the queue is
    just a dummy item for giving us the first item in the list which is stored in the "next" member.
    */
    ma_slot_allocator_alloc(&pQueue->allocator, &pQueue->head);  /* Will never fail. */
    pQueue->pJobs[ma_resource_manager_job_extract_slot(pQueue->head)].next = MA_RESOURCE_MANAGER_JOB_ID_NONE;
    pQueue->tail = pQueue->head;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_job_queue_init(const ma_resource_manager_job_queue_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_resource_manager_job_queue* pQueue)
{
    ma_result result;
    size_t heapSizeInBytes;
    void* pHeap;

    result = ma_resource_manager_job_queue_get_heap_size(pConfig, &heapSizeInBytes);
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

    result = ma_resource_manager_job_queue_init_preallocated(pConfig, pHeap, pQueue);
    if (result != MA_SUCCESS) {
        ma_free(pHeap, pAllocationCallbacks);
        return result;
    }
    
    pQueue->_ownsHeap = MA_TRUE;
    return MA_SUCCESS;
}

MA_API void ma_resource_manager_job_queue_uninit(ma_resource_manager_job_queue* pQueue, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pQueue == NULL) {
        return;
    }

    /* All we need to do is uninitialize the semaphore. */
    if ((pQueue->flags & MA_RESOURCE_MANAGER_JOB_QUEUE_FLAG_NON_BLOCKING) == 0) {
        ma_semaphore_uninit(&pQueue->sem);
    }

    ma_slot_allocator_uninit(&pQueue->allocator, pAllocationCallbacks);

    if (pQueue->_ownsHeap) {
        ma_free(pQueue->_pHeap, pAllocationCallbacks);
    }
}

MA_API ma_result ma_resource_manager_job_queue_post(ma_resource_manager_job_queue* pQueue, const ma_resource_manager_job* pJob)
{
    /*
    Lock free queue implementation based on the paper by Michael and Scott: Nonblocking Algorithms and Preemption-Safe Locking on Multiprogrammed Shared Memory Multiprocessors
    */
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
    MA_ASSERT(ma_resource_manager_job_extract_slot(slot) < pQueue->capacity);

    /* We need to put the job into memory before we do anything. */
    pQueue->pJobs[ma_resource_manager_job_extract_slot(slot)]                  = *pJob;
    pQueue->pJobs[ma_resource_manager_job_extract_slot(slot)].toc.allocation   = slot;                    /* This will overwrite the job code. */
    pQueue->pJobs[ma_resource_manager_job_extract_slot(slot)].toc.breakup.code = pJob->toc.breakup.code;  /* The job code needs to be applied again because the line above overwrote it. */
    pQueue->pJobs[ma_resource_manager_job_extract_slot(slot)].next             = MA_RESOURCE_MANAGER_JOB_ID_NONE;          /* Reset for safety. */

    /* The job is stored in memory so now we need to add it to our linked list. We only ever add items to the end of the list. */
    for (;;) {
        tail = pQueue->tail;
        next = pQueue->pJobs[ma_resource_manager_job_extract_slot(tail)].next;

        if (ma_resource_manager_job_toc_to_allocation(tail) == ma_resource_manager_job_toc_to_allocation(pQueue->tail)) {
            if (ma_resource_manager_job_extract_slot(next) == 0xFFFF) {
                if (c89atomic_compare_and_swap_64(&pQueue->pJobs[ma_resource_manager_job_extract_slot(tail)].next, next, slot) == next) {
                    break;
                }
            } else {
                c89atomic_compare_and_swap_64(&pQueue->tail, tail, next);
            }
        }
    }
    c89atomic_compare_and_swap_64(&pQueue->tail, tail, slot);


    /* Signal the semaphore as the last step if we're using synchronous mode. */
    if ((pQueue->flags & MA_RESOURCE_MANAGER_JOB_QUEUE_FLAG_NON_BLOCKING) == 0) {
        ma_semaphore_release(&pQueue->sem);
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_job_queue_next(ma_resource_manager_job_queue* pQueue, ma_resource_manager_job* pJob)
{
    ma_uint64 head;
    ma_uint64 tail;
    ma_uint64 next;

    if (pQueue == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    /* If we're running in synchronous mode we'll need to wait on a semaphore. */
    if ((pQueue->flags & MA_RESOURCE_MANAGER_JOB_QUEUE_FLAG_NON_BLOCKING) == 0) {
        ma_semaphore_wait(&pQueue->sem);
    }

    /* Now we need to remove the root item from the list. This must be done without locking. */
    for (;;) {
        head = pQueue->head;
        tail = pQueue->tail;
        next = pQueue->pJobs[ma_resource_manager_job_extract_slot(head)].next;

        if (ma_resource_manager_job_toc_to_allocation(head) == ma_resource_manager_job_toc_to_allocation(pQueue->head)) {
            if (ma_resource_manager_job_toc_to_allocation(head) == ma_resource_manager_job_toc_to_allocation(tail)) {
                if (ma_resource_manager_job_extract_slot(next) == 0xFFFF) {
                    return MA_NO_DATA_AVAILABLE;
                }
                c89atomic_compare_and_swap_64(&pQueue->tail, tail, next);
            } else {
                *pJob = pQueue->pJobs[ma_resource_manager_job_extract_slot(next)];
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
    if (pJob->toc.breakup.code == MA_RESOURCE_MANAGER_JOB_QUIT) {
        ma_resource_manager_job_queue_post(pQueue, pJob);
        return MA_CANCELLED;    /* Return a cancelled status just in case the thread is checking return codes and not properly checking for a quit job. */
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_job_queue_free(ma_resource_manager_job_queue* pQueue, ma_resource_manager_job* pJob)
{
    if (pQueue == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_slot_allocator_free(&pQueue->allocator, ma_resource_manager_job_toc_to_allocation(pJob->toc.allocation));
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

static ma_uint32 ma_hash_string_w_32(const wchar_t* str)
{
    return ma_hash_32(str, (int)wcslen(str) * sizeof(*str), MA_DEFAULT_HASH_SEED);
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
                pReplacementDataBufferNode->pChildHi->pParent = pReplacementDataBufferNode->pParent;
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

static ma_resource_manager_data_supply_type ma_resource_manager_data_buffer_node_get_data_supply_type(ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    return (ma_resource_manager_data_supply_type)c89atomic_load_i32(&pDataBufferNode->data.type);
}

static void ma_resource_manager_data_buffer_node_set_data_supply_type(ma_resource_manager_data_buffer_node* pDataBufferNode, ma_resource_manager_data_supply_type supplyType)
{
    c89atomic_exchange_i32(&pDataBufferNode->data.type, supplyType);
}

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
        if (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBufferNode) == ma_resource_manager_data_supply_type_encoded) {
            ma_free((void*)pDataBufferNode->data.encoded.pData, &pResourceManager->config.allocationCallbacks);
            pDataBufferNode->data.encoded.pData       = NULL;
            pDataBufferNode->data.encoded.sizeInBytes = 0;
        } else if (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBufferNode) == ma_resource_manager_data_supply_type_decoded) {
            ma_free((void*)pDataBufferNode->data.decoded.pData, &pResourceManager->config.allocationCallbacks);
            pDataBufferNode->data.decoded.pData           = NULL;
            pDataBufferNode->data.decoded.totalFrameCount = 0;
        } else if (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBufferNode) == ma_resource_manager_data_supply_type_decoded_paged) {
            ma_paged_audio_buffer_data_uninit(&pDataBufferNode->data.decodedPaged.data, &pResourceManager->config.allocationCallbacks);
        } else {
            /* Should never hit this if the node was successfully initialized. */
            MA_ASSERT(pDataBufferNode->result != MA_SUCCESS);
        }
    }

    /* The data buffer itself needs to be freed. */
    ma_free(pDataBufferNode, &pResourceManager->config.allocationCallbacks);
}

static ma_result ma_resource_manager_data_buffer_node_result(const ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    MA_ASSERT(pDataBufferNode != NULL);

    return c89atomic_load_i32((ma_result*)&pDataBufferNode->result);    /* Need a naughty const-cast here. */
}


static ma_bool32 ma_resource_manager_is_threading_enabled(const ma_resource_manager* pResourceManager)
{
    MA_ASSERT(pResourceManager != NULL);

    return (pResourceManager->config.flags & MA_RESOURCE_MANAGER_FLAG_NO_THREADING) == 0;
}


typedef struct
{
    union
    {
        ma_async_notification_event e;
        ma_async_notification_poll p;
    };  /* Must be the first member. */
    ma_resource_manager* pResourceManager;
} ma_resource_manager_inline_notification;

static ma_result ma_resource_manager_inline_notification_init(ma_resource_manager* pResourceManager, ma_resource_manager_inline_notification* pNotification)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pNotification    != NULL);

    pNotification->pResourceManager = pResourceManager;

    if (ma_resource_manager_is_threading_enabled(pResourceManager)) {
        return ma_async_notification_event_init(&pNotification->e);
    } else {
        return ma_async_notification_poll_init(&pNotification->p);
    }
}

static void ma_resource_manager_inline_notification_uninit(ma_resource_manager_inline_notification* pNotification)
{
    MA_ASSERT(pNotification != NULL);

    if (ma_resource_manager_is_threading_enabled(pNotification->pResourceManager)) {
        ma_async_notification_event_uninit(&pNotification->e);
    } else {
        /* No need to uninitialize a polling notification. */
    }
}

static void ma_resource_manager_inline_notification_wait(ma_resource_manager_inline_notification* pNotification)
{
    MA_ASSERT(pNotification != NULL);

    if (ma_resource_manager_is_threading_enabled(pNotification->pResourceManager)) {
        ma_async_notification_event_wait(&pNotification->e);
    } else {
        while (ma_async_notification_poll_is_signalled(&pNotification->p) == MA_FALSE) {
            ma_result result = ma_resource_manager_process_next_job(pNotification->pResourceManager);
            if (result == MA_NO_DATA_AVAILABLE || result == MA_RESOURCE_MANAGER_JOB_QUIT) {
                break;
            }
        }
    }
}

static void ma_resource_manager_inline_notification_wait_and_uninit(ma_resource_manager_inline_notification* pNotification)
{
    ma_resource_manager_inline_notification_wait(pNotification);
    ma_resource_manager_inline_notification_uninit(pNotification);
}


static void ma_resource_manager_data_buffer_bst_lock(ma_resource_manager* pResourceManager)
{
    MA_ASSERT(pResourceManager != NULL);

    if (ma_resource_manager_is_threading_enabled(pResourceManager)) {
        ma_mutex_lock(&pResourceManager->dataBufferBSTLock);
    } else {
        /* Threading not enabled. Do nothing. */
    }
}

static void ma_resource_manager_data_buffer_bst_unlock(ma_resource_manager* pResourceManager)
{
    MA_ASSERT(pResourceManager != NULL);

    if (ma_resource_manager_is_threading_enabled(pResourceManager)) {
        ma_mutex_unlock(&pResourceManager->dataBufferBSTLock);
    } else {
        /* Threading not enabled. Do nothing. */
    }
}

static ma_thread_result MA_THREADCALL ma_resource_manager_job_thread(void* pUserData)
{
    ma_resource_manager* pResourceManager = (ma_resource_manager*)pUserData;
    MA_ASSERT(pResourceManager != NULL);

    for (;;) {
        ma_result result;
        ma_resource_manager_job job;

        result = ma_resource_manager_next_job(pResourceManager, &job);
        if (result != MA_SUCCESS) {
            break;
        }

        /* Terminate if we got a quit message. */
        if (job.toc.breakup.code == MA_RESOURCE_MANAGER_JOB_QUIT) {
            break;
        }

        ma_resource_manager_process_job(pResourceManager, &job);
    }

    return (ma_thread_result)0;
}


MA_API ma_resource_manager_config ma_resource_manager_config_init(void)
{
    ma_resource_manager_config config;

    MA_ZERO_OBJECT(&config);
    config.decodedFormat     = ma_format_unknown;
    config.decodedChannels   = 0;
    config.decodedSampleRate = 0;
    config.jobThreadCount    = 1;   /* A single miniaudio-managed job thread by default. */
    config.jobQueueCapacity  = MA_RESOURCE_MANAGER_JOB_QUEUE_CAPACITY;

    return config;
}


MA_API ma_result ma_resource_manager_init(const ma_resource_manager_config* pConfig, ma_resource_manager* pResourceManager)
{
    ma_result result;
    ma_resource_manager_job_queue_config jobQueueConfig;
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

    /* Get the log set up early so we can start using it as soon as possible. */
    if (pResourceManager->config.pLog == NULL) {
        result = ma_log_init(&pResourceManager->config.allocationCallbacks, &pResourceManager->log);
        if (result == MA_SUCCESS) {
            pResourceManager->config.pLog = &pResourceManager->log;
        } else {
            pResourceManager->config.pLog = NULL;   /* Logging is unavailable. */
        }
    }

    if (pResourceManager->config.pVFS == NULL) {
        result = ma_default_vfs_init(&pResourceManager->defaultVFS, &pResourceManager->config.allocationCallbacks);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to initialize the default file system. */
        }

        pResourceManager->config.pVFS = &pResourceManager->defaultVFS;
    }

    /* We need to force MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING if MA_RESOURCE_MANAGER_FLAG_NO_THREADING is set. */
    if ((pResourceManager->config.flags & MA_RESOURCE_MANAGER_FLAG_NO_THREADING) != 0) {
        pResourceManager->config.flags |= MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING;

        /* We cannot allow job threads when MA_RESOURCE_MANAGER_FLAG_NO_THREADING has been set. This is an invalid use case. */
        if (pResourceManager->config.jobThreadCount > 0) {
            return MA_INVALID_ARGS;
        }
    }

    /* Job queue. */
    jobQueueConfig.capacity = pResourceManager->config.jobQueueCapacity;
    jobQueueConfig.flags    = 0;
    if ((pResourceManager->config.flags & MA_RESOURCE_MANAGER_FLAG_NON_BLOCKING) != 0) {
        if (pResourceManager->config.jobThreadCount > 0) {
            return MA_INVALID_ARGS; /* Non-blocking mode is only valid for self-managed job threads. */
        }

        jobQueueConfig.flags |= MA_RESOURCE_MANAGER_JOB_QUEUE_FLAG_NON_BLOCKING;
    }

    result = ma_resource_manager_job_queue_init(&jobQueueConfig, &pResourceManager->config.allocationCallbacks, &pResourceManager->jobQueue);
    if (result != MA_SUCCESS) {
        return result;
    }


    /* Custom decoding backends. */
    if (pConfig->ppCustomDecodingBackendVTables != NULL && pConfig->customDecodingBackendCount > 0) {
        size_t sizeInBytes = sizeof(*pResourceManager->config.ppCustomDecodingBackendVTables) * pConfig->customDecodingBackendCount;

        pResourceManager->config.ppCustomDecodingBackendVTables = (ma_decoding_backend_vtable**)ma_malloc(sizeInBytes, &pResourceManager->config.allocationCallbacks);
        if (pResourceManager->config.ppCustomDecodingBackendVTables == NULL) {
            ma_resource_manager_job_queue_uninit(&pResourceManager->jobQueue, &pResourceManager->config.allocationCallbacks);
            return MA_OUT_OF_MEMORY;
        }

        MA_COPY_MEMORY(pResourceManager->config.ppCustomDecodingBackendVTables, pConfig->ppCustomDecodingBackendVTables, sizeInBytes);

        pResourceManager->config.customDecodingBackendCount     = pConfig->customDecodingBackendCount;
        pResourceManager->config.pCustomDecodingBackendUserData = pConfig->pCustomDecodingBackendUserData;
    }
    


    /* Here is where we initialize our threading stuff. We don't do this if we don't support threading. */
    if (ma_resource_manager_is_threading_enabled(pResourceManager)) {
        /* Data buffer lock. */
        result = ma_mutex_init(&pResourceManager->dataBufferBSTLock);
        if (result != MA_SUCCESS) {
            ma_resource_manager_job_queue_uninit(&pResourceManager->jobQueue, &pResourceManager->config.allocationCallbacks);
            return result;
        }

        /* Create the job threads last to ensure the threads has access to valid data. */
        for (iJobThread = 0; iJobThread < pResourceManager->config.jobThreadCount; iJobThread += 1) {
            result = ma_thread_create(&pResourceManager->jobThreads[iJobThread], ma_thread_priority_normal, 0, ma_resource_manager_job_thread, pResourceManager, &pResourceManager->config.allocationCallbacks);
            if (result != MA_SUCCESS) {
                ma_mutex_uninit(&pResourceManager->dataBufferBSTLock);
                ma_resource_manager_job_queue_uninit(&pResourceManager->jobQueue, &pResourceManager->config.allocationCallbacks);
                return result;
            }
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
    if (ma_resource_manager_is_threading_enabled(pResourceManager)) {
        for (iJobThread = 0; iJobThread < pResourceManager->config.jobThreadCount; iJobThread += 1) {
            ma_thread_wait(&pResourceManager->jobThreads[iJobThread]);
        }
    }

    /* At this point the thread should have returned and no other thread should be accessing our data. We can now delete all data buffers. */
    ma_resource_manager_delete_all_data_buffer_nodes(pResourceManager);

    /* The job queue is no longer needed. */
    ma_resource_manager_job_queue_uninit(&pResourceManager->jobQueue, &pResourceManager->config.allocationCallbacks);

    /* We're no longer doing anything with data buffers so the lock can now be uninitialized. */
    if (ma_resource_manager_is_threading_enabled(pResourceManager)) {
        ma_mutex_uninit(&pResourceManager->dataBufferBSTLock);
    }

    ma_free(pResourceManager->config.ppCustomDecodingBackendVTables, &pResourceManager->config.allocationCallbacks);

    if (pResourceManager->config.pLog == &pResourceManager->log) {
        ma_log_uninit(&pResourceManager->log);
    }
}

MA_API ma_log* ma_resource_manager_get_log(ma_resource_manager* pResourceManager)
{
    if (pResourceManager == NULL) {
        return NULL;
    }

    return pResourceManager->config.pLog;
}


static ma_decoder_config ma_resource_manager__init_decoder_config(ma_resource_manager* pResourceManager)
{
    ma_decoder_config config;

    config = ma_decoder_config_init(pResourceManager->config.decodedFormat, pResourceManager->config.decodedChannels, pResourceManager->config.decodedSampleRate);
    config.allocationCallbacks    = pResourceManager->config.allocationCallbacks;
    config.ppCustomBackendVTables = pResourceManager->config.ppCustomDecodingBackendVTables;
    config.customBackendCount     = pResourceManager->config.customDecodingBackendCount;
    config.pCustomBackendUserData = pResourceManager->config.pCustomDecodingBackendUserData;

    return config;
}

static ma_result ma_resource_manager__init_decoder(ma_resource_manager* pResourceManager, const char* pFilePath, const wchar_t* pFilePathW, ma_decoder* pDecoder)
{
    ma_result result;
    ma_decoder_config config;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pFilePath        != NULL || pFilePathW != NULL);
    MA_ASSERT(pDecoder         != NULL);

    config = ma_resource_manager__init_decoder_config(pResourceManager);

    if (pFilePath != NULL) {
        result = ma_decoder_init_vfs(pResourceManager->config.pVFS, pFilePath, &config, pDecoder);
        if (result != MA_SUCCESS) {
            ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_WARNING, "Failed to load file \"%s\". %s.\n", pFilePath, ma_result_description(result));
            return result;
        }
    } else {
        result = ma_decoder_init_vfs_w(pResourceManager->config.pVFS, pFilePathW, &config, pDecoder);
        if (result != MA_SUCCESS) {
            ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_WARNING, "Failed to load file \"%ls\". %s.\n", pFilePathW, ma_result_description(result));
            return result;
        }
    }

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_buffer_init_connector(ma_resource_manager_data_buffer* pDataBuffer, ma_async_notification* pInitNotification, ma_fence* pInitFence)
{
    ma_result result;

    MA_ASSERT(pDataBuffer != NULL);
    MA_ASSERT(pDataBuffer->isConnectorInitialized == MA_FALSE);

    /* The underlying data buffer must be initialized before we'll be able to know how to initialize the backend. */
    result = ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode);
    if (result != MA_SUCCESS && result != MA_BUSY) {
        return result;  /* The data buffer is in an erroneous state. */
    }

    /*
    We need to initialize either a ma_decoder or an ma_audio_buffer depending on whether or not the backing data is encoded or decoded. These act as the
    "instance" to the data and are used to form the connection between underlying data buffer and the data source. If the data buffer is decoded, we can use
    an ma_audio_buffer. This enables us to use memory mapping when mixing which saves us a bit of data movement overhead.
    */
    switch (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode))
    {
        case ma_resource_manager_data_supply_type_encoded:          /* Connector is a decoder. */
        {
            ma_decoder_config config;
            config = ma_resource_manager__init_decoder_config(pDataBuffer->pResourceManager);
            result = ma_decoder_init_memory(pDataBuffer->pNode->data.encoded.pData, pDataBuffer->pNode->data.encoded.sizeInBytes, &config, &pDataBuffer->connector.decoder);
        } break;

        case ma_resource_manager_data_supply_type_decoded:          /* Connector is an audio buffer. */
        {
            ma_audio_buffer_config config;
            config = ma_audio_buffer_config_init(pDataBuffer->pNode->data.decoded.format, pDataBuffer->pNode->data.decoded.channels, pDataBuffer->pNode->data.decoded.totalFrameCount, pDataBuffer->pNode->data.decoded.pData, NULL);
            result = ma_audio_buffer_init(&config, &pDataBuffer->connector.buffer);
        } break;

        case ma_resource_manager_data_supply_type_decoded_paged:    /* Connector is a paged audio buffer. */
        {
            ma_paged_audio_buffer_config config;
            config = ma_paged_audio_buffer_config_init(&pDataBuffer->pNode->data.decodedPaged.data);
            result = ma_paged_audio_buffer_init(&config, &pDataBuffer->connector.pagedBuffer);
        } break;

        case ma_resource_manager_data_supply_type_unknown:
        default: 
        {
            /* Unknown data supply type. Should never happen. Need to post an error here. */
            return MA_INVALID_ARGS;
        };
    }

    /*
    Initialization of the connector is when we can fire the init notification. This will give the application access to
    the format/channels/rate of the data source.
    */
    if (result == MA_SUCCESS) {
        pDataBuffer->isConnectorInitialized = MA_TRUE;

        if (pInitNotification != NULL) {
            ma_async_notification_signal(pInitNotification);
        }

        if (pInitFence != NULL) {
            ma_fence_release(pInitFence);
        }
    }
    
    /* At this point the backend should be initialized. We do *not* want to set pDataSource->result here - that needs to be done at a higher level to ensure it's done as the last step. */
    return result;
}

static ma_result ma_resource_manager_data_buffer_uninit_connector(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBuffer      != NULL);

    switch (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode))
    {
        case ma_resource_manager_data_supply_type_encoded:          /* Connector is a decoder. */
        {
            ma_decoder_uninit(&pDataBuffer->connector.decoder);
        } break;

        case ma_resource_manager_data_supply_type_decoded:          /* Connector is an audio buffer. */
        {
            ma_audio_buffer_uninit(&pDataBuffer->connector.buffer);
        } break;

        case ma_resource_manager_data_supply_type_decoded_paged:    /* Connector is a paged audio buffer. */
        {
            ma_paged_audio_buffer_uninit(&pDataBuffer->connector.pagedBuffer);
        } break;

        case ma_resource_manager_data_supply_type_unknown:
        default: 
        {
            /* Unknown data supply type. Should never happen. Need to post an error here. */
            return MA_INVALID_ARGS;
        };
    }

    return MA_SUCCESS;
}

static ma_uint32 ma_resource_manager_data_buffer_node_next_execution_order(ma_resource_manager_data_buffer_node* pDataBufferNode)
{
    MA_ASSERT(pDataBufferNode != NULL);
    return c89atomic_fetch_add_32(&pDataBufferNode->executionCounter, 1);
}

static ma_data_source* ma_resource_manager_data_buffer_get_connector(ma_resource_manager_data_buffer* pDataBuffer)
{
    switch (pDataBuffer->pNode->data.type)
    {
        case ma_resource_manager_data_supply_type_encoded:       return &pDataBuffer->connector.decoder;
        case ma_resource_manager_data_supply_type_decoded:       return &pDataBuffer->connector.buffer;
        case ma_resource_manager_data_supply_type_decoded_paged: return &pDataBuffer->connector.pagedBuffer;

        case ma_resource_manager_data_supply_type_unknown:
        default:
        {
            ma_log_postf(ma_resource_manager_get_log(pDataBuffer->pResourceManager), MA_LOG_LEVEL_ERROR, "Failed to retrieve data buffer connector. Unknown data supply type.\n");
            return NULL;
        };
    };
}

static ma_result ma_resource_manager_data_buffer_node_init_supply_encoded(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode, const char* pFilePath, const wchar_t* pFilePathW)
{
    ma_result result;
    size_t dataSizeInBytes;
    void* pData;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);
    MA_ASSERT(pFilePath != NULL || pFilePathW != NULL);

    result = ma_vfs_open_and_read_file_ex(pResourceManager->config.pVFS, pFilePath, pFilePathW, &pData, &dataSizeInBytes, &pResourceManager->config.allocationCallbacks);
    if (result != MA_SUCCESS) {
        if (pFilePath != NULL) {
            ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_WARNING, "Failed to load file \"%s\". %s.\n", pFilePath, ma_result_description(result));
        } else {
            ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_WARNING, "Failed to load file \"%ls\". %s.\n", pFilePathW, ma_result_description(result));
        }

        return result;
    }

    pDataBufferNode->data.encoded.pData       = pData;
    pDataBufferNode->data.encoded.sizeInBytes = dataSizeInBytes;
    ma_resource_manager_data_buffer_node_set_data_supply_type(pDataBufferNode, ma_resource_manager_data_supply_type_encoded);  /* <-- Must be set last. */

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_buffer_node_init_supply_decoded(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode, const char* pFilePath, const wchar_t* pFilePathW, ma_decoder** ppDecoder)
{
    ma_result result = MA_SUCCESS;
    ma_decoder* pDecoder;
    ma_uint64 totalFrameCount;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);
    MA_ASSERT(ppDecoder         != NULL);
    MA_ASSERT(pFilePath != NULL || pFilePathW != NULL);

    *ppDecoder = NULL;  /* For safety. */

    pDecoder = (ma_decoder*)ma_malloc(sizeof(*pDecoder), &pResourceManager->config.allocationCallbacks);
    if (pDecoder == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_resource_manager__init_decoder(pResourceManager, pFilePath, pFilePathW, pDecoder);
    if (result != MA_SUCCESS) {
        ma_free(pDecoder, &pResourceManager->config.allocationCallbacks);
        return result;
    }

    /*
    At this point we have the decoder and we now need to initialize the data supply. This will
    be either a decoded buffer, or a decoded paged buffer. A regular buffer is just one big heap
    allocated buffer, whereas a paged buffer is a linked list of paged-sized buffers. The latter
    is used when the length of a sound is unknown until a full decode has been performed.
    */
    result = ma_decoder_get_length_in_pcm_frames(pDecoder, &totalFrameCount);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (totalFrameCount > 0) {
        /* It's a known length. The data supply is a regular decoded buffer. */
        ma_uint64 dataSizeInBytes;
        void* pData;

        dataSizeInBytes = totalFrameCount * ma_get_bytes_per_frame(pDecoder->outputFormat, pDecoder->outputChannels);
        if (dataSizeInBytes > MA_SIZE_MAX) {
            ma_decoder_uninit(pDecoder);
            ma_free(pDecoder, &pResourceManager->config.allocationCallbacks);
            return MA_TOO_BIG;
        }

        pData = ma_malloc((size_t)dataSizeInBytes, &pResourceManager->config.allocationCallbacks);
        if (pData == NULL) {
            ma_decoder_uninit(pDecoder);
            ma_free(pDecoder, &pResourceManager->config.allocationCallbacks);
            return MA_OUT_OF_MEMORY;
        }

        /* The buffer needs to be initialized to silence in case the caller reads from it. */
        ma_silence_pcm_frames(pData, totalFrameCount, pDecoder->outputFormat, pDecoder->outputChannels);

        /* Data has been allocated and the data supply can now be initialized. */
        pDataBufferNode->data.decoded.pData             = pData;
        pDataBufferNode->data.decoded.totalFrameCount   = totalFrameCount;
        pDataBufferNode->data.decoded.format            = pDecoder->outputFormat;
        pDataBufferNode->data.decoded.channels          = pDecoder->outputChannels;
        pDataBufferNode->data.decoded.sampleRate        = pDecoder->outputSampleRate;
        pDataBufferNode->data.decoded.decodedFrameCount = 0;
        ma_resource_manager_data_buffer_node_set_data_supply_type(pDataBufferNode, ma_resource_manager_data_supply_type_decoded);  /* <-- Must be set last. */
    } else {
        /*
        It's an unknown length. The data supply is a paged decoded buffer. Setting this up is
        actually easier than the non-paged decoded buffer because we just need to initialize
        a ma_paged_audio_buffer object.
        */
        result = ma_paged_audio_buffer_data_init(pDecoder->outputFormat, pDecoder->outputChannels, &pDataBufferNode->data.decodedPaged.data);
        if (result != MA_SUCCESS) {
            ma_decoder_uninit(pDecoder);
            ma_free(pDecoder, &pResourceManager->config.allocationCallbacks);
            return result;
        }

        pDataBufferNode->data.decodedPaged.sampleRate        = pDecoder->outputSampleRate;
        pDataBufferNode->data.decodedPaged.decodedFrameCount = 0;
        ma_resource_manager_data_buffer_node_set_data_supply_type(pDataBufferNode, ma_resource_manager_data_supply_type_decoded_paged);  /* <-- Must be set last. */
    }

    *ppDecoder = pDecoder;

    return MA_SUCCESS;
}

static ma_result ma_resource_manager_data_buffer_node_decode_next_page(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode, ma_decoder* pDecoder)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 pageSizeInFrames;
    ma_uint64 framesToTryReading;
    ma_uint64 framesRead;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pDataBufferNode  != NULL);
    MA_ASSERT(pDecoder         != NULL);

    /* We need to know the size of a page in frames to know how many frames to decode. */
    pageSizeInFrames = MA_RESOURCE_MANAGER_PAGE_SIZE_IN_MILLISECONDS * (pDecoder->outputSampleRate/1000);
    framesToTryReading = pageSizeInFrames;

    /*
    Here is where we do the decoding of the next page. We'll run a slightly different path depending
    on whether or not we're using a flat or paged buffer because the allocation of the page differs
    between the two. For a flat buffer it's an offset to an already-allocated buffer. For a paged
    buffer, we need to allocate a new page and attach it to the linked list.
    */
    switch (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBufferNode))
    {
        case ma_resource_manager_data_supply_type_decoded:
        {
            /* The destination buffer is an offset to the existing buffer. Don't read more than we originally retrieved when we first initialized the decoder. */
            void* pDst;
            ma_uint64 framesRemaining = pDataBufferNode->data.decoded.totalFrameCount - pDataBufferNode->data.decoded.decodedFrameCount;
            if (framesToTryReading > framesRemaining) {
                framesToTryReading = framesRemaining;
            }

            pDst = ma_offset_ptr(
                pDataBufferNode->data.decoded.pData,
                pDataBufferNode->data.decoded.decodedFrameCount * ma_get_bytes_per_frame(pDataBufferNode->data.decoded.format, pDataBufferNode->data.decoded.channels)
            );
            MA_ASSERT(pDst != NULL);

            result = ma_decoder_read_pcm_frames(pDecoder, pDst, framesToTryReading, &framesRead);
            if (framesRead > 0) {
                pDataBufferNode->data.decoded.decodedFrameCount += framesRead;
            }
        } break;

        case ma_resource_manager_data_supply_type_decoded_paged:
        {
            /* The destination buffer is a freshly allocated page. */
            ma_paged_audio_buffer_page* pPage;

            result = ma_paged_audio_buffer_data_allocate_page(&pDataBufferNode->data.decodedPaged.data, framesToTryReading, NULL, &pResourceManager->config.allocationCallbacks, &pPage);
            if (result != MA_SUCCESS) {
                return result;
            }

            result = ma_decoder_read_pcm_frames(pDecoder, pPage->pAudioData, framesToTryReading, &framesRead);
            if (framesRead > 0) {
                pPage->sizeInFrames = framesRead;

                result = ma_paged_audio_buffer_data_append_page(&pDataBufferNode->data.decodedPaged.data, pPage);
                if (result == MA_SUCCESS) {
                    pDataBufferNode->data.decodedPaged.decodedFrameCount += framesRead;
                } else {
                    /* Failed to append the page. Just abort and set the status to MA_AT_END. */
                    ma_paged_audio_buffer_data_free_page(&pDataBufferNode->data.decodedPaged.data, pPage, &pResourceManager->config.allocationCallbacks);
                    result = MA_AT_END;
                }
            } else {
                /* No frames were read. Free the page and just set the status to MA_AT_END. */
                ma_paged_audio_buffer_data_free_page(&pDataBufferNode->data.decodedPaged.data, pPage, &pResourceManager->config.allocationCallbacks);
                result = MA_AT_END;
            }
        } break;

        case ma_resource_manager_data_supply_type_encoded:
        case ma_resource_manager_data_supply_type_unknown:
        default:
        {
            /* Unexpected data supply type. */
            ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_ERROR, "Unexpected data supply type (%d) when decoding page.", ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBufferNode));
            return MA_ERROR;
        };
    }

    if (result == MA_SUCCESS && framesRead == 0) {
        result = MA_AT_END;
    }

    return result;
}

static ma_result ma_resource_manager_data_buffer_node_acquire(ma_resource_manager* pResourceManager, const char* pFilePath, const wchar_t* pFilePathW, ma_uint32 hashedName32, ma_uint32 flags, const ma_resource_manager_data_supply* pExistingData, ma_fence* pInitFence, ma_fence* pDoneFence, ma_resource_manager_data_buffer_node** ppDataBufferNode)
{
    ma_result result = MA_SUCCESS;
    ma_bool32 nodeAlreadyExists = MA_FALSE;
    ma_resource_manager_data_buffer_node* pDataBufferNode = NULL;
    ma_resource_manager_inline_notification initNotification;   /* Used when the WAIT_INIT flag is set. */

    if (ppDataBufferNode != NULL) {
        *ppDataBufferNode = NULL;   /* Safety. */
    }

    if (pResourceManager == NULL || (pFilePath == NULL && pFilePathW == NULL && hashedName32 == 0)) {
        return MA_INVALID_ARGS;
    }

    /* If we're specifying existing data, it must be valid. */
    if (pExistingData != NULL && pExistingData->type == ma_resource_manager_data_supply_type_unknown) {
        return MA_INVALID_ARGS;
    }

    /* If we don't support threading, remove the ASYNC flag to make the rest of this a bit simpler. */
    if (ma_resource_manager_is_threading_enabled(pResourceManager) == MA_FALSE) {
        flags &= ~MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;
    }

    if (hashedName32 == 0) {
        if (pFilePath != NULL) {
            hashedName32 = ma_hash_string_32(pFilePath);
        } else {
            hashedName32 = ma_hash_string_w_32(pFilePathW);
        }
    }

    /*
    Here is where we either increment the node's reference count or allocate a new one and add it
    to the BST. When allocating a new node, we need to make sure the LOAD_DATA_BUFFER_NODE job is
    posted inside the critical section just in case the caller immediately uninitializes the node
    as this will ensure the FREE_DATA_BUFFER_NODE job is given an execution order such that the
    node is not uninitialized before initialization.
    */
    ma_resource_manager_data_buffer_bst_lock(pResourceManager);
    {
        ma_resource_manager_data_buffer_node* pInsertPoint;

        result = ma_resource_manager_data_buffer_node_insert_point(pResourceManager, hashedName32, &pInsertPoint);
        if (result == MA_ALREADY_EXISTS) {
            /* The node already exists. We just need to increment the reference count. */
            pDataBufferNode = pInsertPoint;

            result = ma_resource_manager_data_buffer_node_increment_ref(pResourceManager, pDataBufferNode, NULL);
            if (result != MA_SUCCESS) {
                goto early_exit;  /* Should never happen. Failed to increment the reference count. */
            }

            nodeAlreadyExists = MA_TRUE;    /* This is used later on, outside of this critical section, to determine whether or not a synchronous load should happen now. */
        } else {
            /*
            The node does not already exist. We need to post a LOAD_DATA_BUFFER_NODE job here. This
            needs to be done inside the critical section to ensure an uninitialization of the node
            does not occur before initialization on another thread.
            */
            pDataBufferNode = (ma_resource_manager_data_buffer_node*)ma_malloc(sizeof(*pDataBufferNode), &pResourceManager->config.allocationCallbacks);
            if (pDataBufferNode == NULL) {
                result = MA_OUT_OF_MEMORY;
                goto early_exit;
            }

            MA_ZERO_OBJECT(pDataBufferNode);
            pDataBufferNode->hashedName32 = hashedName32;
            pDataBufferNode->refCount     = 1;        /* Always set to 1 by default (this is our first reference). */

            if (pExistingData == NULL) {
                pDataBufferNode->data.type    = ma_resource_manager_data_supply_type_unknown;    /* <-- We won't know this until we start decoding. */
                pDataBufferNode->result       = MA_BUSY;  /* Must be set to MA_BUSY before we leave the critical section, so might as well do it now. */
                pDataBufferNode->isDataOwnedByResourceManager = MA_TRUE;
            } else {
                pDataBufferNode->data         = *pExistingData;
                pDataBufferNode->result       = MA_SUCCESS;   /* Not loading asynchronously, so just set the status */
                pDataBufferNode->isDataOwnedByResourceManager = MA_FALSE;
            }

            result = ma_resource_manager_data_buffer_node_insert_at(pResourceManager, pDataBufferNode, pInsertPoint);
            if (result != MA_SUCCESS) {
                ma_free(pDataBufferNode, &pResourceManager->config.allocationCallbacks);
                goto early_exit;  /* Should never happen. Failed to insert the data buffer into the BST. */
            }

            /*
            Here is where we'll post the job, but only if we're loading asynchronously. If we're
            loading synchronously we'll defer loading to a later stage, outside of the critical
            section.
            */
            if (pDataBufferNode->isDataOwnedByResourceManager && (flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC) != 0) {
                /* Loading asynchronously. Post the job. */
                ma_resource_manager_job job;
                char* pFilePathCopy = NULL;
                wchar_t* pFilePathWCopy = NULL;

                /* We need a copy of the file path. We should probably make this more efficient, but for now we'll do a transient memory allocation. */
                if (pFilePath != NULL) {
                    pFilePathCopy = ma_copy_string(pFilePath, &pResourceManager->config.allocationCallbacks);
                } else {
                    pFilePathWCopy = ma_copy_string_w(pFilePathW, &pResourceManager->config.allocationCallbacks);
                }

                if (pFilePathCopy == NULL && pFilePathWCopy == NULL) {
                    result = MA_OUT_OF_MEMORY;
                    goto done;
                }

                if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) {
                    ma_resource_manager_inline_notification_init(pResourceManager, &initNotification);
                }

                /* Acquire init and done fences before posting the job. These will be unacquired by the job thread. */
                if (pInitFence != NULL) { ma_fence_acquire(pInitFence); }
                if (pDoneFence != NULL) { ma_fence_acquire(pDoneFence); }

                /* We now have everything we need to post the job to the job thread. */
                job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER_NODE);
                job.order = ma_resource_manager_data_buffer_node_next_execution_order(pDataBufferNode);
                job.loadDataBufferNode.pDataBufferNode   = pDataBufferNode;
                job.loadDataBufferNode.pFilePath         = pFilePathCopy;
                job.loadDataBufferNode.pFilePathW        = pFilePathWCopy;
                job.loadDataBufferNode.decode            =  (flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE   ) != 0;
                job.loadDataBufferNode.pInitNotification = ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) ? &initNotification : NULL;
                job.loadDataBufferNode.pDoneNotification = NULL;
                job.loadDataBufferNode.pInitFence        = pInitFence;
                job.loadDataBufferNode.pDoneFence        = pDoneFence;

                result = ma_resource_manager_post_job(pResourceManager, &job);
                if (result != MA_SUCCESS) {
                    /* Failed to post job. Probably ran out of memory. */
                    ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_ERROR, "Failed to post MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER_NODE job. %s.\n", ma_result_description(result));

                    /*
                    Fences were acquired before posting the job, but since the job was not able to
                    be posted, we need to make sure we release them so nothing gets stuck waiting.
                    */
                    if (pInitFence != NULL) { ma_fence_release(pInitFence); }
                    if (pDoneFence != NULL) { ma_fence_release(pDoneFence); }

                    ma_free(pFilePathCopy,  &pResourceManager->config.allocationCallbacks);
                    ma_free(pFilePathWCopy, &pResourceManager->config.allocationCallbacks);
                    goto done;
                }
            }
        }
    }
    ma_resource_manager_data_buffer_bst_unlock(pResourceManager);

early_exit:
    if (result != MA_SUCCESS) {
        return result;
    }

    /*
    If we're loading synchronously, we'll need to load everything now. When loading asynchronously,
    a job will have been posted inside the BST critical section so that an uninitialization can be
    allocated an appropriate execution order thereby preventing it from being uninitialized before
    the node is initialized by the decoding thread(s).
    */
    if (nodeAlreadyExists == MA_FALSE) {    /* Don't need to try loading anything if the node already exists. */
        if (pFilePath == NULL && pFilePathW == NULL) {
            /*
            If this path is hit, it means a buffer is being copied (i.e. initialized from only the
            hashed name), but that node has been freed in the meantime, probably from some other
            thread. This is an invalid operation.
            */
            ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_WARNING, "Cloning data buffer node failed because the source node was released. The source node must remain valid until the cloning has completed.\n");
            result = MA_INVALID_OPERATION;
            goto done;
        }

        if (pDataBufferNode->isDataOwnedByResourceManager) {
            if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC) == 0) {
                /* Loading synchronously. Load the sound in it's entirety here. */
                if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE) == 0) {
                    /* No decoding. This is the simple case - just store the file contents in memory. */
                    result = ma_resource_manager_data_buffer_node_init_supply_encoded(pResourceManager, pDataBufferNode, pFilePath, pFilePathW);
                    if (result != MA_SUCCESS) {
                        goto done;
                    }
                } else {
                    /* Decoding. We do this the same way as we do when loading asynchronously. */
                    ma_decoder* pDecoder;
                    result = ma_resource_manager_data_buffer_node_init_supply_decoded(pResourceManager, pDataBufferNode, pFilePath, pFilePathW, &pDecoder);
                    if (result != MA_SUCCESS) {
                        goto done;
                    }

                    /* We have the decoder, now decode page by page just like we do when loading asynchronously. */
                    for (;;) {
                        /* Decode next page. */
                        result = ma_resource_manager_data_buffer_node_decode_next_page(pResourceManager, pDataBufferNode, pDecoder);
                        if (result != MA_SUCCESS) {
                            break;  /* Will return MA_AT_END when the last page has been decoded. */
                        }
                    }

                    /* Reaching the end needs to be considered successful. */
                    if (result == MA_AT_END) {
                        result  = MA_SUCCESS;
                    }

                    /*
                    At this point the data buffer is either fully decoded or some error occurred. Either
                    way, the decoder is no longer necessary.
                    */
                    ma_decoder_uninit(pDecoder);
                    ma_free(pDecoder, &pResourceManager->config.allocationCallbacks);
                }

                /* Getting here means we were successful. Make sure the status of the node is updated accordingly. */
                c89atomic_exchange_i32(&pDataBufferNode->result, result);
            } else {
                /* Loading asynchronously. We may need to wait for initialization. */
                if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) {
                    ma_resource_manager_inline_notification_wait(&initNotification);
                }
            }
        } else {
            /* The data is not managed by the resource manager so there's nothing else to do. */
            MA_ASSERT(pExistingData != NULL);
        }
    }

done:
    /* If we failed to initialize the data buffer we need to free it. */
    if (result != MA_SUCCESS) {
        if (nodeAlreadyExists == MA_FALSE) {
            ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBufferNode);
            ma_free(pDataBufferNode, &pResourceManager->config.allocationCallbacks);
        }
    }

    /*
    The init notification needs to be uninitialized. This will be used if the node does not already
    exist, and we've specified ASYNC | WAIT_INIT.
    */
    if (nodeAlreadyExists == MA_FALSE && pDataBufferNode->isDataOwnedByResourceManager && (flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC) != 0) {
        if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) {
            ma_resource_manager_inline_notification_uninit(&initNotification);
        }
    }

    if (ppDataBufferNode != NULL) {
        *ppDataBufferNode = pDataBufferNode;
    }

    return result;
}

static ma_result ma_resource_manager_data_buffer_node_unacquire(ma_resource_manager* pResourceManager, ma_resource_manager_data_buffer_node* pDataBufferNode, const char* pName, const wchar_t* pNameW)
{
    ma_result result = MA_SUCCESS;
    ma_uint32 refCount = 0xFFFFFFFF; /* The new reference count of the node after decrementing. Initialize to non-0 to be safe we don't fall into the freeing path. */
    ma_uint32 hashedName32 = 0;

    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pDataBufferNode == NULL) {
        if (pName == NULL && pNameW == NULL) {
            return MA_INVALID_ARGS;
        }

        if (pName != NULL) {
            hashedName32 = ma_hash_string_32(pName);
        } else {
            hashedName32 = ma_hash_string_w_32(pNameW);
        }
    }

    /*
    The first thing to do is decrement the reference counter of the node. Then, if the reference
    count is zero, we need to free the node. If the node is still in the process of loading, we'll
    need to post a job to the job queue to free the node. Otherwise we'll just do it here.
    */
    ma_resource_manager_data_buffer_bst_lock(pResourceManager);
    {
        /* Might need to find the node. Must be done inside the critical section. */
        if (pDataBufferNode == NULL) {
            result = ma_resource_manager_data_buffer_node_search(pResourceManager, hashedName32, &pDataBufferNode);
            if (result != MA_SUCCESS) {
                goto stage2;    /* Couldn't find the node. */
            }
        }

        result = ma_resource_manager_data_buffer_node_decrement_ref(pResourceManager, pDataBufferNode, &refCount);
        if (result != MA_SUCCESS) {
            goto stage2;    /* Should never happen. */
        }

        if (refCount == 0) {
            result = ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBufferNode);
            if (result != MA_SUCCESS) {
                goto stage2;  /* An error occurred when trying to remove the data buffer. This should never happen. */
            }
        }
    }
    ma_resource_manager_data_buffer_bst_unlock(pResourceManager);

stage2:
    if (result != MA_SUCCESS) {
        return result;
    }

    /*
    Here is where we need to free the node. We don't want to do this inside the critical section
    above because we want to keep that as small as possible for multi-threaded efficiency.
    */
    if (refCount == 0) {
        if (ma_resource_manager_data_buffer_node_result(pDataBufferNode) == MA_BUSY) {
            /* The sound is still loading. We need to delay the freeing of the node to a safe time. */
            ma_resource_manager_job job;

			/* We need to mark the node as unavailable for the sake of the resource manager worker threads. */
			c89atomic_exchange_i32(&pDataBufferNode->result, MA_UNAVAILABLE);

			job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER_NODE);
            job.order = ma_resource_manager_data_buffer_node_next_execution_order(pDataBufferNode);
            job.freeDataBufferNode.pDataBufferNode = pDataBufferNode;

            result = ma_resource_manager_post_job(pResourceManager, &job);
            if (result != MA_SUCCESS) {
                ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_ERROR, "Failed to post MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER_NODE job. %s.\n", ma_result_description(result));
                return result;
            }

            /* If we don't support threading, process the job queue here. */
            if (ma_resource_manager_is_threading_enabled(pResourceManager) == MA_FALSE) {
                while (ma_resource_manager_data_buffer_node_result(pDataBufferNode) == MA_BUSY) {
                    result = ma_resource_manager_process_next_job(pResourceManager);
                    if (result == MA_NO_DATA_AVAILABLE || result == MA_RESOURCE_MANAGER_JOB_QUIT) {
                        result = MA_SUCCESS;
                        break;
                    }
                }
            } else {
                /* Threading is enabled. The job queue will deal with the rest of the cleanup from here. */
            }
        } else {
            /* The sound isn't loading so we can just free the node here. */
            ma_resource_manager_data_buffer_node_free(pResourceManager, pDataBufferNode);
        }
    }

    return result;
}



static ma_uint32 ma_resource_manager_data_buffer_next_execution_order(ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pDataBuffer != NULL);
    return c89atomic_fetch_add_32(&pDataBuffer->executionCounter, 1);
}

static ma_result ma_resource_manager_data_buffer_cb__read_pcm_frames(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    return ma_resource_manager_data_buffer_read_pcm_frames((ma_resource_manager_data_buffer*)pDataSource, pFramesOut, frameCount, pFramesRead);
}

static ma_result ma_resource_manager_data_buffer_cb__seek_to_pcm_frame(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_resource_manager_data_buffer_seek_to_pcm_frame((ma_resource_manager_data_buffer*)pDataSource, frameIndex);
}

static ma_result ma_resource_manager_data_buffer_cb__get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_resource_manager_data_buffer_get_data_format((ma_resource_manager_data_buffer*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_resource_manager_data_buffer_cb__get_cursor_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_resource_manager_data_buffer_get_cursor_in_pcm_frames((ma_resource_manager_data_buffer*)pDataSource, pCursor);
}

static ma_result ma_resource_manager_data_buffer_cb__get_length_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_resource_manager_data_buffer_get_length_in_pcm_frames((ma_resource_manager_data_buffer*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_resource_manager_data_buffer_vtable =
{
    ma_resource_manager_data_buffer_cb__read_pcm_frames,
    ma_resource_manager_data_buffer_cb__seek_to_pcm_frame,
    ma_resource_manager_data_buffer_cb__get_data_format,
    ma_resource_manager_data_buffer_cb__get_cursor_in_pcm_frames,
    ma_resource_manager_data_buffer_cb__get_length_in_pcm_frames
};

static ma_result ma_resource_manager_data_buffer_init_internal(ma_resource_manager* pResourceManager, const char* pFilePath, const wchar_t* pFilePathW, ma_uint32 hashedName32, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_result result = MA_SUCCESS;
    ma_resource_manager_data_buffer_node* pDataBufferNode;
    ma_data_source_config dataSourceConfig;
    ma_bool32 async;
    ma_resource_manager_pipeline_notifications notifications;

    if (pNotifications != NULL) {
        notifications = *pNotifications;
        pNotifications = NULL;  /* From here on out we should be referencing `notifications` instead of `pNotifications`. Set this to NULL to catch errors at testing time. */
    } else {
        MA_ZERO_OBJECT(&notifications);
    }

    if (pDataBuffer == NULL) {
        ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataBuffer);

    /* For safety, always remove the ASYNC flag if threading is disabled on the resource manager. */
    if (ma_resource_manager_is_threading_enabled(pResourceManager) == MA_FALSE) {
        flags &= ~MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC;
    }

    async = (flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC) != 0;

    /*
    Fences need to be acquired before doing anything. These must be aquired and released outside of
    the node to ensure there's no holes where ma_fence_wait() could prematurely return before the
    data buffer has completed initialization.

    When loading asynchronously, the node acquisition routine below will acquire the fences on this
    thread and then release them on the async thread when the operation is complete.

    These fences are always released at the "done" tag at the end of this function. They'll be
    acquired a second if loading asynchronously. This double acquisition system is just done to
    simplify code maintanence.
    */
    ma_resource_manager_pipeline_notifications_acquire_all_fences(&notifications);
    {
        /* We first need to acquire a node. If ASYNC is not set, this will not return until the entire sound has been loaded. */
        result = ma_resource_manager_data_buffer_node_acquire(pResourceManager, pFilePath, pFilePathW, hashedName32, flags, NULL, notifications.init.pFence, notifications.done.pFence, &pDataBufferNode);
        if (result != MA_SUCCESS) {
            ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
            goto done;
        }

        dataSourceConfig = ma_data_source_config_init();
        dataSourceConfig.vtable = &g_ma_resource_manager_data_buffer_vtable;

        result = ma_data_source_init(&dataSourceConfig, &pDataBuffer->ds);
        if (result != MA_SUCCESS) {
            ma_resource_manager_data_buffer_node_unacquire(pResourceManager, pDataBufferNode, NULL, NULL);
            ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
            goto done;
        }

        pDataBuffer->pResourceManager = pResourceManager;
        pDataBuffer->pNode  = pDataBufferNode;
        pDataBuffer->flags  = flags;
        pDataBuffer->result = MA_BUSY;  /* Always default to MA_BUSY for safety. It'll be overwritten when loading completes or an error occurs. */

        /* If we're loading asynchronously we need to post a job to the job queue to initialize the connector. */
        if (async == MA_FALSE || ma_resource_manager_data_buffer_node_result(pDataBufferNode) == MA_SUCCESS) {
            /* Loading synchronously or the data has already been fully loaded. We can just initialize the connector from here without a job. */
            result = ma_resource_manager_data_buffer_init_connector(pDataBuffer, NULL, NULL);
            c89atomic_exchange_i32(&pDataBuffer->result, result);

            ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
            goto done;
        } else {
            /* The node's data supply isn't initialized yet. The caller has requested that we load asynchronously so we need to post a job to do this. */
            ma_resource_manager_job job;
            ma_resource_manager_inline_notification initNotification;   /* Used when the WAIT_INIT flag is set. */

            if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) {
                ma_resource_manager_inline_notification_init(pResourceManager, &initNotification);
            }

            /*
            The status of the data buffer needs to be set to MA_BUSY before posting the job so that the
            worker thread is aware of it's busy state. If the LOAD_DATA_BUFFER job sees a status other
            than MA_BUSY, it'll assume an error and fall through to an early exit.
            */
            c89atomic_exchange_i32(&pDataBuffer->result, MA_BUSY);

            /* Acquire fences a second time. These will be released by the async thread. */
            ma_resource_manager_pipeline_notifications_acquire_all_fences(&notifications);

            job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER);
            job.order = ma_resource_manager_data_buffer_next_execution_order(pDataBuffer);
            job.loadDataBuffer.pDataBuffer       = pDataBuffer;
            job.loadDataBuffer.pInitNotification = ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) ? &initNotification : notifications.init.pNotification;
            job.loadDataBuffer.pDoneNotification = notifications.done.pNotification;
            job.loadDataBuffer.pInitFence        = notifications.init.pFence;
            job.loadDataBuffer.pDoneFence        = notifications.done.pFence;

            result = ma_resource_manager_post_job(pResourceManager, &job);
            if (result != MA_SUCCESS) {
                /* We failed to post the job. Most likely there isn't enough room in the queue's buffer. */
                ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_ERROR, "Failed to post MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER job. %s.\n", ma_result_description(result));
                c89atomic_exchange_i32(&pDataBuffer->result, result);

                /* Release the fences after the result has been set on the data buffer. */
                ma_resource_manager_pipeline_notifications_release_all_fences(&notifications);
            } else {
                if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) {
                    ma_resource_manager_inline_notification_wait(&initNotification);

                    if (notifications.init.pNotification != NULL) {
                        ma_async_notification_signal(notifications.init.pNotification);
                    }

                    /* NOTE: Do not release the init fence here. It will have been done by the job. */

				    /* Make sure we return an error if initialization failed on the async thread. */
				    result = ma_resource_manager_data_buffer_result(pDataBuffer);
				    if (result == MA_BUSY) {
					    result  = MA_SUCCESS;
				    }
                }
            }

            if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) {
                ma_resource_manager_inline_notification_uninit(&initNotification);
            }
        }

        if (result != MA_SUCCESS) {
            ma_resource_manager_data_buffer_node_unacquire(pResourceManager, pDataBufferNode, NULL, NULL);
            goto done;
        }
    }
done:
    ma_resource_manager_pipeline_notifications_release_all_fences(&notifications);

    return result;
}

MA_API ma_result ma_resource_manager_data_buffer_init(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_buffer* pDataBuffer)
{
    return ma_resource_manager_data_buffer_init_internal(pResourceManager, pFilePath, NULL, 0, flags, pNotifications, pDataBuffer);
}

MA_API ma_result ma_resource_manager_data_buffer_init_w(ma_resource_manager* pResourceManager, const wchar_t* pFilePath, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_buffer* pDataBuffer)
{
    return ma_resource_manager_data_buffer_init_internal(pResourceManager, NULL, pFilePath, 0, flags, pNotifications, pDataBuffer);
}

MA_API ma_result ma_resource_manager_data_buffer_init_copy(ma_resource_manager* pResourceManager, const ma_resource_manager_data_buffer* pExistingDataBuffer, ma_resource_manager_data_buffer* pDataBuffer)
{
    if (pExistingDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ASSERT(pExistingDataBuffer->pNode != NULL);  /* <-- If you've triggered this, you've passed in an invalid existing data buffer. */

    return ma_resource_manager_data_buffer_init_internal(pResourceManager, NULL, NULL, pExistingDataBuffer->pNode->hashedName32, pExistingDataBuffer->flags, NULL, pDataBuffer);
}

static ma_result ma_resource_manager_data_buffer_uninit_internal(ma_resource_manager_data_buffer* pDataBuffer)
{
    MA_ASSERT(pDataBuffer != NULL);

    /* The connector should be uninitialized first. */
    ma_resource_manager_data_buffer_uninit_connector(pDataBuffer->pResourceManager, pDataBuffer);

    /* With the connector uninitialized we can unacquire the node. */
    ma_resource_manager_data_buffer_node_unacquire(pDataBuffer->pResourceManager, pDataBuffer->pNode, NULL, NULL);

    /* The base data source needs to be uninitialized as well. */
    ma_data_source_uninit(&pDataBuffer->ds);

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_uninit(ma_resource_manager_data_buffer* pDataBuffer)
{
    ma_result result;

    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ma_resource_manager_data_buffer_result(pDataBuffer) == MA_SUCCESS) {
        /* The data buffer can be deleted synchronously. */
        return ma_resource_manager_data_buffer_uninit_internal(pDataBuffer);
    } else {
        /*
        The data buffer needs to be deleted asynchronously because it's still loading. With the status set to MA_UNAVAILABLE, no more pages will
        be loaded and the uninitialization should happen fairly quickly. Since the caller owns the data buffer, we need to wait for this event
        to get processed before returning.
        */
        ma_resource_manager_inline_notification notification;
        ma_resource_manager_job job;

        /*
        We need to mark the node as unavailable so we don't try reading from it anymore, but also to
        let the loading thread know that it needs to abort it's loading procedure.
        */
        c89atomic_exchange_i32(&pDataBuffer->result, MA_UNAVAILABLE);

        result = ma_resource_manager_inline_notification_init(pDataBuffer->pResourceManager, &notification);
        if (result != MA_SUCCESS) {
            return result;  /* Failed to create the notification. This should rarely, if ever, happen. */
        }

        job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER);
        job.order = ma_resource_manager_data_buffer_next_execution_order(pDataBuffer);
        job.freeDataBuffer.pDataBuffer       = pDataBuffer;
        job.freeDataBuffer.pDoneNotification = &notification;
        job.freeDataBuffer.pDoneFence        = NULL;

        result = ma_resource_manager_post_job(pDataBuffer->pResourceManager, &job);
        if (result != MA_SUCCESS) {
            ma_resource_manager_inline_notification_uninit(&notification);
            return result;
        }

        ma_resource_manager_inline_notification_wait_and_uninit(&notification);
    }

    return result;
}

MA_API ma_result ma_resource_manager_data_buffer_read_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 framesRead = 0;
    ma_bool32 isLooping;
    ma_bool32 isDecodedBufferBusy = MA_FALSE;

    /* Safety. */
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    /*
    We cannot be using the data buffer after it's been uninitialized. If you trigger this assert it means you're trying to read from the data buffer after
    it's been uninitialized or is in the process of uninitializing.
    */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    /* If the node is not initialized we need to abort with a busy code. */
    if (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode) == ma_resource_manager_data_supply_type_unknown) {
        return MA_BUSY; /* Still loading. */
    }

    if (pDataBuffer->seekToCursorOnNextRead) {
        pDataBuffer->seekToCursorOnNextRead = MA_FALSE;

        result = ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_buffer_get_connector(pDataBuffer), pDataBuffer->seekTargetInPCMFrames);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    result = ma_resource_manager_data_buffer_get_looping(pDataBuffer, &isLooping);
    if (result != MA_SUCCESS) {
        return result;
    }

    /*
    For decoded buffers (not paged) we need to check beforehand how many frames we have available. We cannot
    exceed this amount. We'll read as much as we can, and then return MA_BUSY.
    */
    if (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode) == ma_resource_manager_data_supply_type_decoded) {
        ma_uint64 availableFrames;

        isDecodedBufferBusy = (ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) == MA_BUSY);

        if (ma_resource_manager_data_buffer_get_available_frames(pDataBuffer, &availableFrames) == MA_SUCCESS) {
            /* Don't try reading more than the available frame count. */
            if (frameCount > availableFrames) {
                frameCount = availableFrames;
    
                /*
                If there's no frames available we want to set the status to MA_AT_END. The logic below
                will check if the node is busy, and if so, change it to MA_BUSY. The reason we do this
                is because we don't want to call `ma_data_source_read_pcm_frames()` if the frame count
                is 0 because that'll result in a situation where it's possible MA_AT_END won't get
                returned.
                */
                if (frameCount == 0) {
                    result = MA_AT_END;
                }
            } else {
                isDecodedBufferBusy = MA_FALSE; /* We have enough frames available in the buffer to avoid a MA_BUSY status. */
            }
        }
    }

    /* Don't attempt to read anything if we've got no frames available. */
    if (frameCount > 0) {
        result = ma_data_source_read_pcm_frames(ma_resource_manager_data_buffer_get_connector(pDataBuffer), pFramesOut, frameCount, &framesRead, isLooping);
    }

    /*
    If we returned MA_AT_END, but the node is still loading, we don't want to return that code or else the caller will interpret the sound
    as at the end and terminate decoding.
    */
    if (result == MA_AT_END) {
        if (ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) == MA_BUSY) {
            result = MA_BUSY;
        }
    }

    if (isDecodedBufferBusy) {
        result = MA_BUSY;
    }

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
    if (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode) == ma_resource_manager_data_supply_type_unknown) {
        pDataBuffer->seekTargetInPCMFrames = frameIndex;
        pDataBuffer->seekToCursorOnNextRead = MA_TRUE;
        return MA_BUSY; /* Still loading. */
    }

    result = ma_data_source_seek_to_pcm_frame(ma_resource_manager_data_buffer_get_connector(pDataBuffer), frameIndex);
    if (result != MA_SUCCESS) {
        return result;
    }

    pDataBuffer->seekTargetInPCMFrames = ~(ma_uint64)0; /* <-- For identification purposes. */
    pDataBuffer->seekToCursorOnNextRead = MA_FALSE;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_buffer_get_data_format(ma_resource_manager_data_buffer* pDataBuffer, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    switch (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode))
    {
        case ma_resource_manager_data_supply_type_encoded:
        {
            return ma_data_source_get_data_format(&pDataBuffer->connector.decoder, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
        };

        case ma_resource_manager_data_supply_type_decoded:
        {
            *pFormat     = pDataBuffer->pNode->data.decoded.format;
            *pChannels   = pDataBuffer->pNode->data.decoded.channels;
            *pSampleRate = pDataBuffer->pNode->data.decoded.sampleRate;
            ma_get_standard_channel_map(ma_standard_channel_map_default, pChannelMap, channelMapCap, pDataBuffer->pNode->data.decoded.channels);
            return MA_SUCCESS;
        };

        case ma_resource_manager_data_supply_type_decoded_paged:
        {
            *pFormat     = pDataBuffer->pNode->data.decodedPaged.data.format;
            *pChannels   = pDataBuffer->pNode->data.decodedPaged.data.channels;
            *pSampleRate = pDataBuffer->pNode->data.decodedPaged.sampleRate;
            ma_get_standard_channel_map(ma_standard_channel_map_default, pChannelMap, channelMapCap, pDataBuffer->pNode->data.decoded.channels);
            return MA_SUCCESS;
        };

        case ma_resource_manager_data_supply_type_unknown:
        {
            return MA_BUSY; /* Still loading. */
        };

        default:
        {
            /* Unknown supply type. Should never hit this. */
            return MA_INVALID_ARGS;
        }
    }
}

MA_API ma_result ma_resource_manager_data_buffer_get_cursor_in_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pCursor)
{
    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    if (pDataBuffer == NULL || pCursor == NULL) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;

    switch (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode))
    {
        case ma_resource_manager_data_supply_type_encoded:
        {
            return ma_decoder_get_cursor_in_pcm_frames(&pDataBuffer->connector.decoder, pCursor);
        };

        case ma_resource_manager_data_supply_type_decoded:
        {
            return ma_audio_buffer_get_cursor_in_pcm_frames(&pDataBuffer->connector.buffer, pCursor);
        };

        case ma_resource_manager_data_supply_type_decoded_paged:
        {
            return ma_paged_audio_buffer_get_cursor_in_pcm_frames(&pDataBuffer->connector.pagedBuffer, pCursor);
        };

        case ma_resource_manager_data_supply_type_unknown:
        {
            return MA_BUSY;
        };

        default:
        {
            return MA_INVALID_ARGS;
        }
    }
}

MA_API ma_result ma_resource_manager_data_buffer_get_length_in_pcm_frames(ma_resource_manager_data_buffer* pDataBuffer, ma_uint64* pLength)
{
    /* We cannot be using the data source after it's been uninitialized. */
    MA_ASSERT(ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) != MA_UNAVAILABLE);

    if (pDataBuffer == NULL || pLength == NULL) {
        return MA_INVALID_ARGS;
    }

    if (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode) == ma_resource_manager_data_supply_type_unknown) {
        return MA_BUSY; /* Still loading. */
    }

    return ma_data_source_get_length_in_pcm_frames(ma_resource_manager_data_buffer_get_connector(pDataBuffer), pLength);
}

MA_API ma_result ma_resource_manager_data_buffer_result(const ma_resource_manager_data_buffer* pDataBuffer)
{
    if (pDataBuffer == NULL) {
        return MA_INVALID_ARGS;
    }

    return c89atomic_load_i32((ma_result*)&pDataBuffer->result);    /* Need a naughty const-cast here. */
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

    if (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode) == ma_resource_manager_data_supply_type_unknown) {
        if (ma_resource_manager_data_buffer_node_result(pDataBuffer->pNode) == MA_BUSY) {
            return MA_BUSY;
        } else {
            return MA_INVALID_OPERATION;    /* No connector. */
        }
    }

    switch (ma_resource_manager_data_buffer_node_get_data_supply_type(pDataBuffer->pNode))
    {
        case ma_resource_manager_data_supply_type_encoded:
        {
            return ma_decoder_get_available_frames(&pDataBuffer->connector.decoder, pAvailableFrames);
        };

        case ma_resource_manager_data_supply_type_decoded:
        {
            return ma_audio_buffer_get_available_frames(&pDataBuffer->connector.buffer, pAvailableFrames);
        };

        case ma_resource_manager_data_supply_type_decoded_paged:
        {
            ma_uint64 cursor;
            ma_paged_audio_buffer_get_cursor_in_pcm_frames(&pDataBuffer->connector.pagedBuffer, &cursor);

            if (pDataBuffer->pNode->data.decodedPaged.decodedFrameCount > cursor) {
                *pAvailableFrames = pDataBuffer->pNode->data.decodedPaged.decodedFrameCount - cursor;
            } else {
                *pAvailableFrames = 0;
            }

            return MA_SUCCESS;
        };

        case ma_resource_manager_data_supply_type_unknown:
        default:
        {
            /* Unknown supply type. Should never hit this. */
            return MA_INVALID_ARGS;
        }
    }
}

MA_API ma_result ma_resource_manager_register_file(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags)
{
    return ma_resource_manager_data_buffer_node_acquire(pResourceManager, pFilePath, NULL, 0, flags, NULL, NULL, NULL, NULL);
}

MA_API ma_result ma_resource_manager_register_file_w(ma_resource_manager* pResourceManager, const wchar_t* pFilePath, ma_uint32 flags)
{
    return ma_resource_manager_data_buffer_node_acquire(pResourceManager, NULL, pFilePath, 0, flags, NULL, NULL, NULL, NULL);
}


static ma_result ma_resource_manager_register_data(ma_resource_manager* pResourceManager, const char* pName, const wchar_t* pNameW, ma_resource_manager_data_supply* pExistingData)
{
    return ma_resource_manager_data_buffer_node_acquire(pResourceManager, pName, pNameW, 0, 0, pExistingData, NULL, NULL, NULL);
}

static ma_result ma_resource_manager_register_decoded_data_internal(ma_resource_manager* pResourceManager, const char* pName, const wchar_t* pNameW, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    ma_resource_manager_data_supply data;
    data.type                    = ma_resource_manager_data_supply_type_decoded;
    data.decoded.pData           = pData;
    data.decoded.totalFrameCount = frameCount;
    data.decoded.format          = format;
    data.decoded.channels        = channels;
    data.decoded.sampleRate      = sampleRate;

    return ma_resource_manager_register_data(pResourceManager, pName, pNameW, &data);
}

MA_API ma_result ma_resource_manager_register_decoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    return ma_resource_manager_register_decoded_data_internal(pResourceManager, pName, NULL, pData, frameCount, format, channels, sampleRate);
}

MA_API ma_result ma_resource_manager_register_decoded_data_w(ma_resource_manager* pResourceManager, const wchar_t* pName, const void* pData, ma_uint64 frameCount, ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    return ma_resource_manager_register_decoded_data_internal(pResourceManager, NULL, pName, pData, frameCount, format, channels, sampleRate);
}


static ma_result ma_resource_manager_register_encoded_data_internal(ma_resource_manager* pResourceManager, const char* pName, const wchar_t* pNameW, const void* pData, size_t sizeInBytes)
{
    ma_resource_manager_data_supply data;
    data.type                = ma_resource_manager_data_supply_type_encoded;
    data.encoded.pData       = pData;
    data.encoded.sizeInBytes = sizeInBytes;

    return ma_resource_manager_register_data(pResourceManager, pName, pNameW, &data);
}

MA_API ma_result ma_resource_manager_register_encoded_data(ma_resource_manager* pResourceManager, const char* pName, const void* pData, size_t sizeInBytes)
{
    return ma_resource_manager_register_encoded_data_internal(pResourceManager, pName, NULL, pData, sizeInBytes);
}

MA_API ma_result ma_resource_manager_register_encoded_data_w(ma_resource_manager* pResourceManager, const wchar_t* pName, const void* pData, size_t sizeInBytes)
{
    return ma_resource_manager_register_encoded_data_internal(pResourceManager, NULL, pName, pData, sizeInBytes);
}


MA_API ma_result ma_resource_manager_unregister_file(ma_resource_manager* pResourceManager, const char* pFilePath)
{
    return ma_resource_manager_unregister_data(pResourceManager, pFilePath);
}

MA_API ma_result ma_resource_manager_unregister_file_w(ma_resource_manager* pResourceManager, const wchar_t* pFilePath)
{
    return ma_resource_manager_unregister_data_w(pResourceManager, pFilePath);
}

MA_API ma_result ma_resource_manager_unregister_data(ma_resource_manager* pResourceManager, const char* pName)
{
    return ma_resource_manager_data_buffer_node_unacquire(pResourceManager, NULL, pName, NULL);
}

MA_API ma_result ma_resource_manager_unregister_data_w(ma_resource_manager* pResourceManager, const wchar_t* pName)
{
    return ma_resource_manager_data_buffer_node_unacquire(pResourceManager, NULL, NULL, pName);
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

static ma_result ma_resource_manager_data_stream_cb__get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_resource_manager_data_stream_get_data_format((ma_resource_manager_data_stream*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_resource_manager_data_stream_cb__get_cursor_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_resource_manager_data_stream_get_cursor_in_pcm_frames((ma_resource_manager_data_stream*)pDataSource, pCursor);
}

static ma_result ma_resource_manager_data_stream_cb__get_length_in_pcm_frames(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_resource_manager_data_stream_get_length_in_pcm_frames((ma_resource_manager_data_stream*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_resource_manager_data_stream_vtable =
{
    ma_resource_manager_data_stream_cb__read_pcm_frames,
    ma_resource_manager_data_stream_cb__seek_to_pcm_frame,
    ma_resource_manager_data_stream_cb__get_data_format,
    ma_resource_manager_data_stream_cb__get_cursor_in_pcm_frames,
    ma_resource_manager_data_stream_cb__get_length_in_pcm_frames
};

static ma_result ma_resource_manager_data_stream_init_internal(ma_resource_manager* pResourceManager, const char* pFilePath, const wchar_t* pFilePathW, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_stream* pDataStream)
{
    ma_result result;
    ma_data_source_config dataSourceConfig;
    char* pFilePathCopy = NULL;
    wchar_t* pFilePathWCopy = NULL;
    ma_resource_manager_job job;
    ma_bool32 waitBeforeReturning = MA_FALSE;
    ma_resource_manager_inline_notification waitNotification;
    ma_resource_manager_pipeline_notifications notifications;

    if (pNotifications != NULL) {
        notifications = *pNotifications;
        pNotifications = NULL;  /* From here on out, `notifications` should be used instead of `pNotifications`. Setting this to NULL to catch any errors at testing time. */
    } else {
        MA_ZERO_OBJECT(&notifications);
    }

    if (pDataStream == NULL) {
        ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataStream);

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_resource_manager_data_stream_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pDataStream->ds);
    if (result != MA_SUCCESS) {
        ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
        return result;
    }

    pDataStream->pResourceManager = pResourceManager;
    pDataStream->flags            = flags;
    pDataStream->result           = MA_BUSY;

    if (pResourceManager == NULL || (pFilePath == NULL && pFilePathW == NULL)) {
        ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
        return MA_INVALID_ARGS;
    }

    /* We want all access to the VFS and the internal decoder to happen on the job thread just to keep things easier to manage for the VFS.  */

    /* We need a copy of the file path. We should probably make this more efficient, but for now we'll do a transient memory allocation. */
    if (pFilePath != NULL) {
        pFilePathCopy = ma_copy_string(pFilePath, &pResourceManager->config.allocationCallbacks);
    } else {
        pFilePathWCopy = ma_copy_string_w(pFilePathW, &pResourceManager->config.allocationCallbacks);
    }

    if (pFilePathCopy == NULL && pFilePathWCopy == NULL) {
        ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
        return MA_OUT_OF_MEMORY;
    }

    /*
    We need to check for the presence of MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC. If it's not set, we need to wait before returning. Otherwise we
    can return immediately. Likewise, we'll also check for MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT and do the same.
    */
    if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC) == 0 || (flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_WAIT_INIT) != 0) {
        waitBeforeReturning = MA_TRUE;
        ma_resource_manager_inline_notification_init(pResourceManager, &waitNotification);
    }

    ma_resource_manager_pipeline_notifications_acquire_all_fences(&notifications);

    /* We now have everything we need to post the job. This is the last thing we need to do from here. The rest will be done by the job thread. */
    job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_LOAD_DATA_STREAM);
    job.order = ma_resource_manager_data_stream_next_execution_order(pDataStream);
    job.loadDataStream.pDataStream       = pDataStream;
    job.loadDataStream.pFilePath         = pFilePathCopy;
    job.loadDataStream.pFilePathW        = pFilePathWCopy;
    job.loadDataStream.pInitNotification = (waitBeforeReturning == MA_TRUE) ? &waitNotification : notifications.init.pNotification;
    job.loadDataStream.pInitFence        = notifications.init.pFence;
    result = ma_resource_manager_post_job(pResourceManager, &job);
    if (result != MA_SUCCESS) {
        ma_resource_manager_pipeline_notifications_signal_all_notifications(&notifications);
        ma_resource_manager_pipeline_notifications_release_all_fences(&notifications);

        if (waitBeforeReturning) {
            ma_resource_manager_inline_notification_uninit(&waitNotification);
        }

        ma_free(pFilePathCopy,  &pResourceManager->config.allocationCallbacks);
        ma_free(pFilePathWCopy, &pResourceManager->config.allocationCallbacks);
        return result;
    }

    /* Wait if needed. */
    if (waitBeforeReturning) {
        ma_resource_manager_inline_notification_wait_and_uninit(&waitNotification);

        if (notifications.init.pNotification != NULL) {
            ma_async_notification_signal(notifications.init.pNotification);
        }

        /* NOTE: Do not release pInitFence here. That will be done by the job. */
    }

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_stream_init(ma_resource_manager* pResourceManager, const char* pFilePath, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_stream* pDataStream)
{
    return ma_resource_manager_data_stream_init_internal(pResourceManager, pFilePath, NULL, flags, pNotifications, pDataStream);
}

MA_API ma_result ma_resource_manager_data_stream_init_w(ma_resource_manager* pResourceManager, const wchar_t* pFilePath, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_stream* pDataStream)
{
    return ma_resource_manager_data_stream_init_internal(pResourceManager, NULL, pFilePath, flags, pNotifications, pDataStream);
}

MA_API ma_result ma_resource_manager_data_stream_uninit(ma_resource_manager_data_stream* pDataStream)
{
    ma_resource_manager_inline_notification freeEvent;
    ma_resource_manager_job job;

    if (pDataStream == NULL) {
        return MA_INVALID_ARGS;
    }

    /* The first thing to do is set the result to unavailable. This will prevent future page decoding. */
    c89atomic_exchange_i32(&pDataStream->result, MA_UNAVAILABLE);

    /*
    We need to post a job to ensure we're not in the middle or decoding or anything. Because the object is owned by the caller, we'll need
    to wait for it to complete before returning which means we need an event.
    */
    ma_resource_manager_inline_notification_init(pDataStream->pResourceManager, &freeEvent);

    job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_FREE_DATA_STREAM);
    job.order = ma_resource_manager_data_stream_next_execution_order(pDataStream);
    job.freeDataStream.pDataStream       = pDataStream;
    job.freeDataStream.pDoneNotification = &freeEvent;
    job.freeDataStream.pDoneFence        = NULL;
    ma_resource_manager_post_job(pDataStream->pResourceManager, &job);

    /* We need to wait for the job to finish processing before we return. */
    ma_resource_manager_inline_notification_wait_and_uninit(&freeEvent);

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
    ma_result result = MA_SUCCESS;
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
            result = ma_decoder_read_pcm_frames(&pDataStream->decoder, ma_offset_pcm_frames_ptr(pPageData, totalFramesReadForThisPage, pDataStream->decoder.outputFormat, pDataStream->decoder.outputChannels), framesRemaining, &framesRead);
            totalFramesReadForThisPage += framesRead;

            /* Loop back to the start if we reached the end. We'll also have a known length at this point as well. */
            if (result == MA_AT_END || framesRead < framesRemaining) {
                if (pDataStream->totalLengthInPCMFrames == 0) {
                    ma_decoder_get_cursor_in_pcm_frames(&pDataStream->decoder, &pDataStream->totalLengthInPCMFrames);
                }

                ma_decoder_seek_to_pcm_frame(&pDataStream->decoder, 0);
                result = MA_SUCCESS;    /* Clear the AT_END result so we don't incorrectly mark this looping stream as at the end and then have it stopped. */
            }

            if (result != MA_SUCCESS && result != MA_AT_END) {
                break;
            }
        }
    } else {
        result = ma_decoder_read_pcm_frames(&pDataStream->decoder, pPageData, pageSizeInFrames, &totalFramesReadForThisPage);
    }

    if (result == MA_AT_END || totalFramesReadForThisPage < pageSizeInFrames) {
        c89atomic_exchange_32(&pDataStream->isDecoderAtEnd, MA_TRUE);
    }

    c89atomic_exchange_32(&pDataStream->pageFrameCount[pageIndex], (ma_uint32)totalFramesReadForThisPage);
    c89atomic_exchange_32(&pDataStream->isPageValid[pageIndex], MA_TRUE);
}

static void ma_resource_manager_data_stream_fill_pages(ma_resource_manager_data_stream* pDataStream)
{
    ma_uint32 iPage;

    MA_ASSERT(pDataStream != NULL);

    for (iPage = 0; iPage < 2; iPage += 1) {
        ma_resource_manager_data_stream_fill_page(pDataStream, iPage);
    }
}


static ma_result ma_resource_manager_data_stream_map(ma_resource_manager_data_stream* pDataStream, void** ppFramesOut, ma_uint64* pFrameCount)
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

static void ma_resource_manager_data_stream_set_absolute_cursor(ma_resource_manager_data_stream* pDataStream, ma_uint64 absoluteCursor)
{
    /* Loop if possible. */
    if (absoluteCursor > pDataStream->totalLengthInPCMFrames && pDataStream->totalLengthInPCMFrames > 0) {
        absoluteCursor = absoluteCursor % pDataStream->totalLengthInPCMFrames;
    }

    c89atomic_exchange_64(&pDataStream->absoluteCursor, absoluteCursor);
}

static ma_result ma_resource_manager_data_stream_unmap(ma_resource_manager_data_stream* pDataStream, ma_uint64 frameCount)
{
    ma_uint32 newRelativeCursor;
    ma_uint32 pageSizeInFrames;
    ma_resource_manager_job job;

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

    /* The absolute cursor needs to be updated for ma_resource_manager_data_stream_get_cursor_in_pcm_frames(). */
    ma_resource_manager_data_stream_set_absolute_cursor(pDataStream, c89atomic_load_64(&pDataStream->absoluteCursor) + frameCount);

    /* Here is where we need to check if we need to load a new page, and if so, post a job to load it. */
    newRelativeCursor = pDataStream->relativeCursor + (ma_uint32)frameCount;

    /* If the new cursor has flowed over to the next page we need to mark the old one as invalid and post an event for it. */
    if (newRelativeCursor >= pageSizeInFrames) {
        newRelativeCursor -= pageSizeInFrames;

        /* Here is where we post the job start decoding. */
        job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_PAGE_DATA_STREAM);
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


MA_API ma_result ma_resource_manager_data_stream_read_pcm_frames(ma_resource_manager_data_stream* pDataStream, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_result result = MA_SUCCESS;
    ma_uint64 totalFramesProcessed;
    ma_format format;
    ma_uint32 channels;

    /* Safety. */
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

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

    ma_resource_manager_data_stream_get_data_format(pDataStream, &format, &channels, NULL, NULL, 0);

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

MA_API ma_result ma_resource_manager_data_stream_seek_to_pcm_frame(ma_resource_manager_data_stream* pDataStream, ma_uint64 frameIndex)
{
    ma_resource_manager_job job;
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

    /* Update the absolute cursor so that ma_resource_manager_data_stream_get_cursor_in_pcm_frames() returns the new position. */
    ma_resource_manager_data_stream_set_absolute_cursor(pDataStream, frameIndex);

    /*
    We need to clear our currently loaded pages so that the stream starts playback from the new seek point as soon as possible. These are for the purpose of the public
    API and will be ignored by the seek job. The seek job will operate on the assumption that both pages have been marked as invalid and the cursor is at the start of
    the first page.
    */
    pDataStream->relativeCursor   = 0;
    pDataStream->currentPageIndex = 0;
    c89atomic_exchange_32(&pDataStream->isPageValid[0], MA_FALSE);
    c89atomic_exchange_32(&pDataStream->isPageValid[1], MA_FALSE);

    /* Make sure the data stream is not marked as at the end or else if we seek in response to hitting the end, we won't be able to read any more data. */
    c89atomic_exchange_32(&pDataStream->isDecoderAtEnd, MA_FALSE);

    /*
    The public API is not allowed to touch the internal decoder so we need to use a job to perform the seek. When seeking, the job thread will assume both pages
    are invalid and any content contained within them will be discarded and replaced with newly decoded data.
    */
    job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_SEEK_DATA_STREAM);
    job.order = ma_resource_manager_data_stream_next_execution_order(pDataStream);
    job.seekDataStream.pDataStream = pDataStream;
    job.seekDataStream.frameIndex  = frameIndex;
    return ma_resource_manager_post_job(pDataStream->pResourceManager, &job);
}

MA_API ma_result ma_resource_manager_data_stream_get_data_format(ma_resource_manager_data_stream* pDataStream, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
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

    if (pChannelMap != NULL) {
        MA_ZERO_MEMORY(pChannelMap, sizeof(*pChannelMap) * channelMapCap);
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
    return ma_data_source_get_data_format(&pDataStream->decoder, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
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
    ma_uint32 pageIndex0;
    ma_uint32 pageIndex1;
    ma_uint32 relativeCursor;
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


static ma_result ma_resource_manager_data_source_preinit(ma_resource_manager* pResourceManager, ma_uint32 flags, ma_resource_manager_data_source* pDataSource)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDataSource);

    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    pDataSource->flags = flags;

    return MA_SUCCESS;
}

MA_API ma_result ma_resource_manager_data_source_init(ma_resource_manager* pResourceManager, const char* pName, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_source* pDataSource)
{
    ma_result result;

    result = ma_resource_manager_data_source_preinit(pResourceManager, flags, pDataSource);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* The data source itself is just a data stream or a data buffer. */
    if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_init(pResourceManager, pName, flags, pNotifications, &pDataSource->stream);
    } else {
        return ma_resource_manager_data_buffer_init(pResourceManager, pName, flags, pNotifications, &pDataSource->buffer);
    }
}

MA_API ma_result ma_resource_manager_data_source_init_w(ma_resource_manager* pResourceManager, const wchar_t* pName, ma_uint32 flags, const ma_resource_manager_pipeline_notifications* pNotifications, ma_resource_manager_data_source* pDataSource)
{
    ma_result result;

    result = ma_resource_manager_data_source_preinit(pResourceManager, flags, pDataSource);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* The data source itself is just a data stream or a data buffer. */
    if ((flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_init_w(pResourceManager, pName, flags, pNotifications, &pDataSource->stream);
    } else {
        return ma_resource_manager_data_buffer_init_w(pResourceManager, pName, flags, pNotifications, &pDataSource->buffer);
    }
}

MA_API ma_result ma_resource_manager_data_source_init_copy(ma_resource_manager* pResourceManager, const ma_resource_manager_data_source* pExistingDataSource, ma_resource_manager_data_source* pDataSource)
{
    ma_result result;

    if (pExistingDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    result = ma_resource_manager_data_source_preinit(pResourceManager, pExistingDataSource->flags, pDataSource);
    if (result != MA_SUCCESS) {
        return result;
    }

    /* Copying can only be done from data buffers. Streams cannot be copied. */
    if ((pExistingDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
        return MA_INVALID_OPERATION;
    }

    return ma_resource_manager_data_buffer_init_copy(pResourceManager, &pExistingDataSource->buffer, &pDataSource->buffer);
}

MA_API ma_result ma_resource_manager_data_source_uninit(ma_resource_manager_data_source* pDataSource)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    /* All we need to is uninitialize the underlying data buffer or data stream. */
    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_uninit(&pDataSource->stream);
    } else {
        return ma_resource_manager_data_buffer_uninit(&pDataSource->buffer);
    }
}

MA_API ma_result ma_resource_manager_data_source_read_pcm_frames(ma_resource_manager_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    /* Safety. */
    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
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

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
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

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_map(&pDataSource->stream, ppFramesOut, pFrameCount);
    } else {
        return MA_NOT_IMPLEMENTED;  /* Mapping not supported with data buffers. */
    }
}

MA_API ma_result ma_resource_manager_data_source_unmap(ma_resource_manager_data_source* pDataSource, ma_uint64 frameCount)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_unmap(&pDataSource->stream, frameCount);
    } else {
        return MA_NOT_IMPLEMENTED;  /* Mapping not supported with data buffers. */
    }
}

MA_API ma_result ma_resource_manager_data_source_get_data_format(ma_resource_manager_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_get_data_format(&pDataSource->stream, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
    } else {
        return ma_resource_manager_data_buffer_get_data_format(&pDataSource->buffer, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
    }
}

MA_API ma_result ma_resource_manager_data_source_get_cursor_in_pcm_frames(ma_resource_manager_data_source* pDataSource, ma_uint64* pCursor)
{
    if (pDataSource == NULL) {
        return MA_INVALID_ARGS;
    }

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
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

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
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

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
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

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
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

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
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

    if ((pDataSource->flags & MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_STREAM) != 0) {
        return ma_resource_manager_data_stream_get_available_frames(&pDataSource->stream, pAvailableFrames);
    } else {
        return ma_resource_manager_data_buffer_get_available_frames(&pDataSource->buffer, pAvailableFrames);
    }
}


MA_API ma_result ma_resource_manager_post_job(ma_resource_manager* pResourceManager, const ma_resource_manager_job* pJob)
{
    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_job_queue_post(&pResourceManager->jobQueue, pJob);
}

MA_API ma_result ma_resource_manager_post_job_quit(ma_resource_manager* pResourceManager)
{
    ma_resource_manager_job job = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_QUIT);
    return ma_resource_manager_post_job(pResourceManager, &job);
}

MA_API ma_result ma_resource_manager_next_job(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
{
    if (pResourceManager == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_resource_manager_job_queue_next(&pResourceManager->jobQueue, pJob);
}


static ma_result ma_resource_manager_process_job__load_data_buffer_node(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
{
    ma_result result = MA_SUCCESS;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob != NULL);
    MA_ASSERT(pJob->loadDataBufferNode.pDataBufferNode != NULL);
    MA_ASSERT(pJob->loadDataBufferNode.pDataBufferNode->isDataOwnedByResourceManager == MA_TRUE);  /* The data should always be owned by the resource manager. */

    /* First thing we need to do is check whether or not the data buffer is getting deleted. If so we just abort. */
    if (ma_resource_manager_data_buffer_node_result(pJob->loadDataBufferNode.pDataBufferNode) != MA_BUSY) {
        result = ma_resource_manager_data_buffer_node_result(pJob->loadDataBufferNode.pDataBufferNode);    /* The data buffer may be getting deleted before it's even been loaded. */
        goto done;
    }

    /* The data buffer is not getting deleted, but we may be getting executed out of order. If so, we need to push the job back onto the queue and return. */
    if (pJob->order != pJob->loadDataBufferNode.pDataBufferNode->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Attempting to execute out of order. Probably interleaved with a MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER job. */
    }

    /*
    We're ready to start loading. Essentially what we're doing here is initializing the data supply
    of the node. Once this is complete, data buffers can have their connectors initialized which
    will allow then to have audio data read from them.

    Note that when the data supply type has been moved away from "unknown", that is when other threads
    will determine that the node is available for data delivery and the data buffer connectors can be
    initialized. Therefore, it's important that it is set after the data supply has been initialized.
    */
    if (pJob->loadDataBufferNode.decode) {
        /*
        Decoding. This is the complex case because we're not going to be doing the entire decoding
        process here. Instead it's going to be split of multiple jobs and loaded in pages. The
        reason for this is to evenly distribute decoding time across multiple sounds, rather than
        having one huge sound hog all the available processing resources.

        The first thing we do is initialize a decoder. This is allocated on the heap and is passed
        around to the paging jobs. When the last paging job has completed it's processing, it'll
        free the decoder for us.

        This job does not do any actual decoding. It instead just posts a PAGE_DATA_BUFFER_NODE job
        which is where the actual decoding work will be done. However, once this job is complete,
        the node will be in a state where data buffer connectors can be initialized.
        */
        ma_decoder* pDecoder;   /* <-- Free'd on the last page decode. */
        ma_resource_manager_job pageDataBufferNodeJob;

        /* Allocate the decoder by initializing a decoded data supply. */
        result = ma_resource_manager_data_buffer_node_init_supply_decoded(pResourceManager, pJob->loadDataBufferNode.pDataBufferNode, pJob->loadDataBufferNode.pFilePath, pJob->loadDataBufferNode.pFilePathW, &pDecoder);
        
        /*
        Don't ever propagate an MA_BUSY result code or else the resource manager will think the
        node is just busy decoding rather than in an error state. This should never happen, but
        including this logic for safety just in case.
        */
        if (result == MA_BUSY) {
            result  = MA_ERROR;
        }

        if (result != MA_SUCCESS) {
            if (pJob->loadDataBufferNode.pFilePath != NULL) {
                ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_WARNING, "Failed to initialize data supply for \"%s\". %s.\n", pJob->loadDataBufferNode.pFilePath, ma_result_description(result));
            } else {
                ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_WARNING, "Failed to initialize data supply for \"%ls\", %s.\n", pJob->loadDataBufferNode.pFilePathW, ma_result_description(result));
            }

            goto done;
        }

        /*
        At this point the node's data supply is initialized and other threads can start initializing
        their data buffer connectors. However, no data will actually be available until we start to
        actually decode it. To do this, we need to post a paging job which is where the decoding
        work is done.

        Note that if an error occurred at an earlier point, this section will have been skipped.
        */
        pageDataBufferNodeJob = ma_resource_manager_job_init(MA_RESOURCE_MANAGER_JOB_PAGE_DATA_BUFFER_NODE);
        pageDataBufferNodeJob.order                                = ma_resource_manager_data_buffer_node_next_execution_order(pJob->loadDataBufferNode.pDataBufferNode);
        pageDataBufferNodeJob.pageDataBufferNode.pDataBufferNode   = pJob->loadDataBufferNode.pDataBufferNode;
        pageDataBufferNodeJob.pageDataBufferNode.pDecoder          = pDecoder;
        pageDataBufferNodeJob.pageDataBufferNode.pDoneNotification = pJob->loadDataBufferNode.pDoneNotification;
        pageDataBufferNodeJob.pageDataBufferNode.pDoneFence        = pJob->loadDataBufferNode.pDoneFence;

        /* The job has been set up so it can now be posted. */
        result = ma_resource_manager_post_job(pResourceManager, &pageDataBufferNodeJob);

        /*
        When we get here, we want to make sure the result code is set to MA_BUSY. The reason for
        this is that the result will be copied over to the node's internal result variable. In
        this case, since the decoding is still in-progress, we need to make sure the result code
        is set to MA_BUSY.
        */
        if (result != MA_SUCCESS) {
            ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_ERROR, "Failed to post MA_RESOURCE_MANAGER_JOB_PAGE_DATA_BUFFER_NODE job. %d\n", ma_result_description(result));
            ma_decoder_uninit(pDecoder);
            ma_free(pDecoder, &pResourceManager->config.allocationCallbacks);
        } else {
            result = MA_BUSY;
        }
    } else {
        /* No decoding. This is the simple case. We need only read the file content into memory and we're done. */
        result = ma_resource_manager_data_buffer_node_init_supply_encoded(pResourceManager, pJob->loadDataBufferNode.pDataBufferNode, pJob->loadDataBufferNode.pFilePath, pJob->loadDataBufferNode.pFilePathW);
    }


done:
    /* File paths are no longer needed. */
    ma_free(pJob->loadDataBufferNode.pFilePath,  &pResourceManager->config.allocationCallbacks);
    ma_free(pJob->loadDataBufferNode.pFilePathW, &pResourceManager->config.allocationCallbacks);

    /*
    We need to set the result to at the very end to ensure no other threads try reading the data before we've fully initialized the object. Other threads
    are going to be inspecting this variable to determine whether or not they're ready to read data. We can only change the result if it's set to MA_BUSY
    because otherwise we may be changing away from an error code which would be bad. An example is if the application creates a data buffer, but then
    immediately deletes it before we've got to this point. In this case, pDataBuffer->result will be MA_UNAVAILABLE, and setting it to MA_SUCCESS or any
    other error code would cause the buffer to look like it's in a state that it's not.
    */
    c89atomic_compare_and_swap_32(&pJob->loadDataBufferNode.pDataBufferNode->result, MA_BUSY, result);

    /* At this point initialization is complete and we can signal the notification if any. */
    if (pJob->loadDataBufferNode.pInitNotification != NULL) {
        ma_async_notification_signal(pJob->loadDataBufferNode.pInitNotification);
    }
    if (pJob->loadDataBufferNode.pInitFence != NULL) {
        ma_fence_release(pJob->loadDataBufferNode.pInitFence);
    }

    /* If we have a success result it means we've fully loaded the buffer. This will happen in the non-decoding case. */
    if (result != MA_BUSY) {
        if (pJob->loadDataBufferNode.pDoneNotification != NULL) {
            ma_async_notification_signal(pJob->loadDataBufferNode.pDoneNotification);
        }
        if (pJob->loadDataBufferNode.pDoneFence != NULL) {
            ma_fence_release(pJob->loadDataBufferNode.pDoneFence);
        }
    }

    /* Increment the node's execution pointer so that the next jobs can be processed. This is how we keep decoding of pages in-order. */
    c89atomic_fetch_add_32(&pJob->loadDataBufferNode.pDataBufferNode->executionPointer, 1);
    return result;
}

static ma_result ma_resource_manager_process_job__free_data_buffer_node(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);
    MA_ASSERT(pJob->freeDataBufferNode.pDataBufferNode != NULL);

    if (pJob->order != pJob->freeDataBufferNode.pDataBufferNode->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    ma_resource_manager_data_buffer_node_free(pResourceManager, pJob->freeDataBufferNode.pDataBufferNode);

    /* The event needs to be signalled last. */
    if (pJob->freeDataBufferNode.pDoneNotification != NULL) {
        ma_async_notification_signal(pJob->freeDataBufferNode.pDoneNotification);
    }

    if (pJob->freeDataBufferNode.pDoneFence != NULL) {
        ma_fence_release(pJob->freeDataBufferNode.pDoneFence);
    }

    c89atomic_fetch_add_32(&pJob->freeDataBufferNode.pDataBufferNode->executionPointer, 1);
    return MA_SUCCESS;
}

static ma_result ma_resource_manager_process_job__page_data_buffer_node(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
{
    ma_result result = MA_SUCCESS;

    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);

    /* Don't do any more decoding if the data buffer has started the uninitialization process. */
    if (ma_resource_manager_data_buffer_node_result(pJob->pageDataBufferNode.pDataBufferNode) != MA_BUSY) {
        result = ma_resource_manager_data_buffer_node_result(pJob->pageDataBufferNode.pDataBufferNode);
		goto done;
    }

    if (pJob->order != pJob->pageDataBufferNode.pDataBufferNode->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    /* We're ready to decode the next page. */
    result = ma_resource_manager_data_buffer_node_decode_next_page(pResourceManager, pJob->pageDataBufferNode.pDataBufferNode, pJob->pageDataBufferNode.pDecoder);

    /*
    If we have a success code by this point, we want to post another job. We're going to set the
    result back to MA_BUSY to make it clear that there's still more to load.
    */
    if (result == MA_SUCCESS) {
        ma_resource_manager_job newJob;
        newJob = *pJob; /* Everything is the same as the input job, except the execution order. */
        newJob.order = ma_resource_manager_data_buffer_node_next_execution_order(pJob->pageDataBufferNode.pDataBufferNode);   /* We need a fresh execution order. */

        result = ma_resource_manager_post_job(pResourceManager, &newJob);

        /* Since the sound isn't yet fully decoded we want the status to be set to busy. */
        if (result == MA_SUCCESS) {
            result  = MA_BUSY;
        }
    }

done:
    /* If there's still more to decode the result will be set to MA_BUSY. Otherwise we can free the decoder. */
    if (result != MA_BUSY) {
        ma_decoder_uninit(pJob->pageDataBufferNode.pDecoder);
        ma_free(pJob->pageDataBufferNode.pDecoder, &pResourceManager->config.allocationCallbacks);
    }

    /* If we reached the end we need to treat it as successful. */
    if (result == MA_AT_END) {
        result  = MA_SUCCESS;
    }

    /* Make sure we set the result of node in case some error occurred. */
    c89atomic_compare_and_swap_32(&pJob->pageDataBufferNode.pDataBufferNode->result, MA_BUSY, result);

    /* Signal the notification after setting the result in case the notification callback wants to inspect the result code. */
    if (result != MA_BUSY) {
        if (pJob->pageDataBufferNode.pDoneNotification != NULL) {
            ma_async_notification_signal(pJob->pageDataBufferNode.pDoneNotification);
        }

        if (pJob->pageDataBufferNode.pDoneFence != NULL) {
            ma_fence_release(pJob->pageDataBufferNode.pDoneFence);
        }
    }

    c89atomic_fetch_add_32(&pJob->pageDataBufferNode.pDataBufferNode->executionPointer, 1);
    return result;
}


static ma_result ma_resource_manager_process_job__load_data_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
{
    ma_result result = MA_SUCCESS;

    /*
    All we're doing here is checking if the node has finished loading. If not, we just re-post the job
    and keep waiting. Otherwise we increment the execution counter and set the buffer's result code.
    */
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);
    MA_ASSERT(pJob->loadDataBuffer.pDataBuffer != NULL);

    if (pJob->order != pJob->loadDataBuffer.pDataBuffer->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Attempting to execute out of order. Probably interleaved with a MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER job. */
    }

    /*
    First thing we need to do is check whether or not the data buffer is getting deleted. If so we
    just abort, but making sure we increment the execution pointer.
    */
    if (ma_resource_manager_data_buffer_result(pJob->loadDataBuffer.pDataBuffer) != MA_BUSY) {
        result = ma_resource_manager_data_buffer_result(pJob->loadDataBuffer.pDataBuffer);    /* The data buffer may be getting deleted before it's even been loaded. */
        goto done;
    }

    /* Try initializing the connector if we haven't already. */
    if (pJob->loadDataBuffer.pDataBuffer->isConnectorInitialized == MA_FALSE) {
        if (ma_resource_manager_data_buffer_node_get_data_supply_type(pJob->loadDataBuffer.pDataBuffer->pNode) != ma_resource_manager_data_supply_type_unknown) {
            /* We can now initialize the connector. If this fails, we need to abort. It's very rare for this to fail. */
            result = ma_resource_manager_data_buffer_init_connector(pJob->loadDataBuffer.pDataBuffer, pJob->loadDataBuffer.pInitNotification, pJob->loadDataBuffer.pInitFence);
            if (result != MA_SUCCESS) {
                ma_log_postf(ma_resource_manager_get_log(pResourceManager), MA_LOG_LEVEL_ERROR, "Failed to initialize connector for data buffer. %s.\n", ma_result_description(result));
                goto done;
            }
        }
    } else {
        /* The connector is already initialized. Nothing to do here. */
    }

    /*
    If the data node is still loading, we need to repost the job and *not* increment the execution
    pointer (i.e. we need to not fall through to the "done" label).
    */
    result = ma_resource_manager_data_buffer_node_result(pJob->loadDataBuffer.pDataBuffer->pNode);
    if (result == MA_BUSY) {
        return ma_resource_manager_post_job(pResourceManager, pJob);
    }

done:
    /* Only move away from a busy code so that we don't trash any existing error codes. */
    c89atomic_compare_and_swap_32(&pJob->loadDataBuffer.pDataBuffer->result, MA_BUSY, result);

    /* Only signal the other threads after the result has been set just for cleanliness sake. */
    if (pJob->loadDataBuffer.pDoneNotification != NULL) {
        ma_async_notification_signal(pJob->loadDataBuffer.pDoneNotification);
    }
    if (pJob->loadDataBuffer.pDoneFence != NULL) {
        ma_fence_release(pJob->loadDataBuffer.pDoneFence);
    }

	/*
	If at this point the data buffer has not had it's connector initialized, it means the
	notification event was never signalled which means we need to signal it here.
	*/
	if (pJob->loadDataBuffer.pDataBuffer->isConnectorInitialized == MA_FALSE && result != MA_SUCCESS) {
		if (pJob->loadDataBuffer.pInitNotification != NULL) {
		    ma_async_notification_signal(pJob->loadDataBuffer.pInitNotification);
		}
        if (pJob->loadDataBuffer.pInitFence != NULL) {
            ma_fence_release(pJob->loadDataBuffer.pInitFence);
        }
	}

    c89atomic_fetch_add_32(&pJob->loadDataBuffer.pDataBuffer->executionPointer, 1);
    return result;
}

static ma_result ma_resource_manager_process_job__free_data_buffer(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
{
    MA_ASSERT(pResourceManager != NULL);
    MA_ASSERT(pJob             != NULL);
    MA_ASSERT(pJob->freeDataBuffer.pDataBuffer != NULL);

    if (pJob->order != pJob->freeDataBuffer.pDataBuffer->executionPointer) {
        return ma_resource_manager_post_job(pResourceManager, pJob);    /* Out of order. */
    }

    ma_resource_manager_data_buffer_uninit_internal(pJob->freeDataBuffer.pDataBuffer);

    /* The event needs to be signalled last. */
    if (pJob->freeDataBuffer.pDoneNotification != NULL) {
        ma_async_notification_signal(pJob->freeDataBuffer.pDoneNotification);
    }

    if (pJob->freeDataBuffer.pDoneFence != NULL) {
        ma_fence_release(pJob->freeDataBuffer.pDoneFence);
    }

    c89atomic_fetch_add_32(&pJob->freeDataBuffer.pDataBuffer->executionPointer, 1);
    return MA_SUCCESS;
}

static ma_result ma_resource_manager_process_job__load_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
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

    if (pJob->loadDataStream.pFilePath != NULL) {
        result = ma_decoder_init_vfs(pResourceManager->config.pVFS, pJob->loadDataStream.pFilePath, &decoderConfig, &pDataStream->decoder);
    } else {
        result = ma_decoder_init_vfs_w(pResourceManager->config.pVFS, pJob->loadDataStream.pFilePathW, &decoderConfig, &pDataStream->decoder);
    }
    if (result != MA_SUCCESS) {
        goto done;
    }

    /* Retrieve the total length of the file before marking the decoder are loaded. */
    result = ma_decoder_get_length_in_pcm_frames(&pDataStream->decoder, &pDataStream->totalLengthInPCMFrames);
    if (result != MA_SUCCESS) {
        goto done;  /* Failed to retrieve the length. */
    }

    /*
    Only mark the decoder as initialized when the length of the decoder has been retrieved because that can possibly require a scan over the whole file
    and we don't want to have another thread trying to access the decoder while it's scanning.
    */
    pDataStream->isDecoderInitialized = MA_TRUE;

    /* We have the decoder so we can now initialize our page buffer. */
    pageBufferSizeInBytes = ma_resource_manager_data_stream_get_page_size_in_frames(pDataStream) * 2 * ma_get_bytes_per_frame(pDataStream->decoder.outputFormat, pDataStream->decoder.outputChannels);

    pDataStream->pPageData = ma_malloc(pageBufferSizeInBytes, &pResourceManager->config.allocationCallbacks);
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
    ma_free(pJob->loadDataStream.pFilePath,  &pResourceManager->config.allocationCallbacks);
    ma_free(pJob->loadDataStream.pFilePathW, &pResourceManager->config.allocationCallbacks);

    /* We can only change the status away from MA_BUSY. If it's set to anything else it means an error has occurred somewhere or the uninitialization process has started (most likely). */
    c89atomic_compare_and_swap_32(&pDataStream->result, MA_BUSY, result);

    /* Only signal the other threads after the result has been set just for cleanliness sake. */
    if (pJob->loadDataStream.pInitNotification != NULL) {
        ma_async_notification_signal(pJob->loadDataStream.pInitNotification);
    }
    if (pJob->loadDataStream.pInitFence != NULL) {
        ma_fence_release(pJob->loadDataStream.pInitFence);
    }

    c89atomic_fetch_add_32(&pDataStream->executionPointer, 1);
    return result;
}

static ma_result ma_resource_manager_process_job__free_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
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
        ma_free(pDataStream->pPageData, &pResourceManager->config.allocationCallbacks);
        pDataStream->pPageData = NULL;  /* Just in case... */
    }

    ma_data_source_uninit(&pDataStream->ds);

    /* The event needs to be signalled last. */
    if (pJob->freeDataStream.pDoneNotification != NULL) {
        ma_async_notification_signal(pJob->freeDataStream.pDoneNotification);
    }
    if (pJob->freeDataStream.pDoneFence != NULL) {
        ma_fence_release(pJob->freeDataStream.pDoneFence);
    }

    /*c89atomic_fetch_add_32(&pDataStream->executionPointer, 1);*/
    return MA_SUCCESS;
}

static ma_result ma_resource_manager_process_job__page_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
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

static ma_result ma_resource_manager_process_job__seek_data_stream(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
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

MA_API ma_result ma_resource_manager_process_job(ma_resource_manager* pResourceManager, ma_resource_manager_job* pJob)
{
    if (pResourceManager == NULL || pJob == NULL) {
        return MA_INVALID_ARGS;
    }

    switch (pJob->toc.breakup.code)
    {
        /* Data Buffer Node */
        case MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER_NODE: return ma_resource_manager_process_job__load_data_buffer_node(pResourceManager, pJob);
        case MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER_NODE: return ma_resource_manager_process_job__free_data_buffer_node(pResourceManager, pJob);
        case MA_RESOURCE_MANAGER_JOB_PAGE_DATA_BUFFER_NODE: return ma_resource_manager_process_job__page_data_buffer_node(pResourceManager, pJob);

        /* Data Buffer */
        case MA_RESOURCE_MANAGER_JOB_LOAD_DATA_BUFFER: return ma_resource_manager_process_job__load_data_buffer(pResourceManager, pJob);
        case MA_RESOURCE_MANAGER_JOB_FREE_DATA_BUFFER: return ma_resource_manager_process_job__free_data_buffer(pResourceManager, pJob);

        /* Data Stream */
        case MA_RESOURCE_MANAGER_JOB_LOAD_DATA_STREAM: return ma_resource_manager_process_job__load_data_stream(pResourceManager, pJob);
        case MA_RESOURCE_MANAGER_JOB_FREE_DATA_STREAM: return ma_resource_manager_process_job__free_data_stream(pResourceManager, pJob);
        case MA_RESOURCE_MANAGER_JOB_PAGE_DATA_STREAM: return ma_resource_manager_process_job__page_data_stream(pResourceManager, pJob);
        case MA_RESOURCE_MANAGER_JOB_SEEK_DATA_STREAM: return ma_resource_manager_process_job__seek_data_stream(pResourceManager, pJob);

        default: break;
    }

    /* Getting here means we don't know what the job code is and cannot do anything with it. */
    return MA_INVALID_OPERATION;
}

MA_API ma_result ma_resource_manager_process_next_job(ma_resource_manager* pResourceManager)
{
    ma_result result;
    ma_resource_manager_job job;

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




/*
Biquad Node
*/
MA_API ma_biquad_node_config ma_biquad_node_config_init(ma_uint32 channels, float b0, float b1, float b2, float a0, float a1, float a2)
{
    ma_biquad_node_config config;
    
    config.nodeConfig = ma_node_config_init();
    config.biquad = ma_biquad_config_init(ma_format_f32, channels, b0, b1, b2, a0, a1, a2);

    return config;
}

static void ma_biquad_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_biquad_node* pLPFNode = (ma_biquad_node*)pNode;

    MA_ASSERT(pNode != NULL);
    (void)pFrameCountIn;

    ma_biquad_process_pcm_frames(&pLPFNode->biquad, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_biquad_node_vtable =
{
    ma_biquad_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* One input. */
    1,      /* One output. */
    0       /* Default flags. */
};

MA_API ma_result ma_biquad_node_init(ma_node_graph* pNodeGraph, const ma_biquad_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_biquad_node* pNode)
{
    ma_result result;
    ma_node_config baseNodeConfig;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->biquad.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* The format must be f32. */
    }

    result = ma_biquad_init(&pConfig->biquad, &pNode->biquad);
    if (result != MA_SUCCESS) {
        return result;
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.vtable          = &g_ma_biquad_node_vtable;
    baseNodeConfig.pInputChannels  = &pConfig->biquad.channels;
    baseNodeConfig.pOutputChannels = &pConfig->biquad.channels;

    result = ma_node_init(pNodeGraph, &baseNodeConfig, pAllocationCallbacks, pNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return result;
}

MA_API ma_result ma_biquad_node_reinit(const ma_biquad_config* pConfig, ma_biquad_node* pNode)
{
    ma_biquad_node* pLPFNode = (ma_biquad_node*)pNode;

    MA_ASSERT(pNode != NULL);

    return ma_biquad_reinit(pConfig, &pLPFNode->biquad);
}

MA_API void ma_biquad_node_uninit(ma_biquad_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pNode, pAllocationCallbacks);
}



/*
Low Pass Filter Node
*/
MA_API ma_lpf_node_config ma_lpf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order)
{
    ma_lpf_node_config config;

    config.nodeConfig = ma_node_config_init();
    config.lpf = ma_lpf_config_init(ma_format_f32, channels, sampleRate, cutoffFrequency, order);

    return config;
}

static void ma_lpf_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_lpf_node* pLPFNode = (ma_lpf_node*)pNode;

    MA_ASSERT(pNode != NULL);
    (void)pFrameCountIn;

    ma_lpf_process_pcm_frames(&pLPFNode->lpf, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_lpf_node_vtable =
{
    ma_lpf_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* One input. */
    1,      /* One output. */
    0       /* Default flags. */
};

MA_API ma_result ma_lpf_node_init(ma_node_graph* pNodeGraph, const ma_lpf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_lpf_node* pNode)
{
    ma_result result;
    ma_node_config baseNodeConfig;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->lpf.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* The format must be f32. */
    }

    result = ma_lpf_init(&pConfig->lpf, &pNode->lpf);
    if (result != MA_SUCCESS) {
        return result;
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.vtable          = &g_ma_lpf_node_vtable;
    baseNodeConfig.pInputChannels  = &pConfig->lpf.channels;
    baseNodeConfig.pOutputChannels = &pConfig->lpf.channels;

    result = ma_node_init(pNodeGraph, &baseNodeConfig, pAllocationCallbacks, pNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return result;
}

MA_API ma_result ma_lpf_node_reinit(const ma_lpf_config* pConfig, ma_lpf_node* pNode)
{
    ma_lpf_node* pLPFNode = (ma_lpf_node*)pNode;

    MA_ASSERT(pNode != NULL);

    return ma_lpf_reinit(pConfig, &pLPFNode->lpf);
}

MA_API void ma_lpf_node_uninit(ma_lpf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pNode, pAllocationCallbacks);
}



/*
High Pass Filter Node
*/
MA_API ma_hpf_node_config ma_hpf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order)
{
    ma_hpf_node_config config;

    config.nodeConfig = ma_node_config_init();
    config.hpf = ma_hpf_config_init(ma_format_f32, channels, sampleRate, cutoffFrequency, order);

    return config;
}

static void ma_hpf_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_hpf_node* pHPFNode = (ma_hpf_node*)pNode;

    MA_ASSERT(pNode != NULL);
    (void)pFrameCountIn;

    ma_hpf_process_pcm_frames(&pHPFNode->hpf, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_hpf_node_vtable =
{
    ma_hpf_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* One input. */
    1,      /* One output. */
    0       /* Default flags. */
};

MA_API ma_result ma_hpf_node_init(ma_node_graph* pNodeGraph, const ma_hpf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_hpf_node* pNode)
{
    ma_result result;
    ma_node_config baseNodeConfig;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->hpf.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* The format must be f32. */
    }

    result = ma_hpf_init(&pConfig->hpf, &pNode->hpf);
    if (result != MA_SUCCESS) {
        return result;
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.vtable          = &g_ma_hpf_node_vtable;
    baseNodeConfig.pInputChannels  = &pConfig->hpf.channels;
    baseNodeConfig.pOutputChannels = &pConfig->hpf.channels;

    result = ma_node_init(pNodeGraph, &baseNodeConfig, pAllocationCallbacks, pNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return result;
}

MA_API ma_result ma_hpf_node_reinit(const ma_hpf_config* pConfig, ma_hpf_node* pNode)
{
    ma_hpf_node* pHPFNode = (ma_hpf_node*)pNode;

    MA_ASSERT(pNode != NULL);

    return ma_hpf_reinit(pConfig, &pHPFNode->hpf);
}

MA_API void ma_hpf_node_uninit(ma_hpf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pNode, pAllocationCallbacks);
}




/*
Band Pass Filter Node
*/
MA_API ma_bpf_node_config ma_bpf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double cutoffFrequency, ma_uint32 order)
{
    ma_bpf_node_config config;

    config.nodeConfig = ma_node_config_init();
    config.bpf = ma_bpf_config_init(ma_format_f32, channels, sampleRate, cutoffFrequency, order);

    return config;
}

static void ma_bpf_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_bpf_node* pBPFNode = (ma_bpf_node*)pNode;

    MA_ASSERT(pNode != NULL);
    (void)pFrameCountIn;

    ma_bpf_process_pcm_frames(&pBPFNode->bpf, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_bpf_node_vtable =
{
    ma_bpf_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* One input. */
    1,      /* One output. */
    0       /* Default flags. */
};

MA_API ma_result ma_bpf_node_init(ma_node_graph* pNodeGraph, const ma_bpf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_bpf_node* pNode)
{
    ma_result result;
    ma_node_config baseNodeConfig;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->bpf.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* The format must be f32. */
    }

    result = ma_bpf_init(&pConfig->bpf, &pNode->bpf);
    if (result != MA_SUCCESS) {
        return result;
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.vtable          = &g_ma_bpf_node_vtable;
    baseNodeConfig.pInputChannels  = &pConfig->bpf.channels;
    baseNodeConfig.pOutputChannels = &pConfig->bpf.channels;

    result = ma_node_init(pNodeGraph, &baseNodeConfig, pAllocationCallbacks, pNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return result;
}

MA_API ma_result ma_bpf_node_reinit(const ma_bpf_config* pConfig, ma_bpf_node* pNode)
{
    ma_bpf_node* pBPFNode = (ma_bpf_node*)pNode;

    MA_ASSERT(pNode != NULL);

    return ma_bpf_reinit(pConfig, &pBPFNode->bpf);
}

MA_API void ma_bpf_node_uninit(ma_bpf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pNode, pAllocationCallbacks);
}



/*
Notching Filter Node
*/
MA_API ma_notch_node_config ma_notch_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double q, double frequency)
{
    ma_notch_node_config config;

    config.nodeConfig = ma_node_config_init();
    config.notch = ma_notch2_config_init(ma_format_f32, channels, sampleRate, q, frequency);

    return config;
}

static void ma_notch_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_notch_node* pBPFNode = (ma_notch_node*)pNode;

    MA_ASSERT(pNode != NULL);
    (void)pFrameCountIn;

    ma_notch2_process_pcm_frames(&pBPFNode->notch, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_notch_node_vtable =
{
    ma_notch_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* One input. */
    1,      /* One output. */
    0       /* Default flags. */
};

MA_API ma_result ma_notch_node_init(ma_node_graph* pNodeGraph, const ma_notch_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_notch_node* pNode)
{
    ma_result result;
    ma_node_config baseNodeConfig;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->notch.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* The format must be f32. */
    }

    result = ma_notch2_init(&pConfig->notch, &pNode->notch);
    if (result != MA_SUCCESS) {
        return result;
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.vtable          = &g_ma_notch_node_vtable;
    baseNodeConfig.pInputChannels  = &pConfig->notch.channels;
    baseNodeConfig.pOutputChannels = &pConfig->notch.channels;

    result = ma_node_init(pNodeGraph, &baseNodeConfig, pAllocationCallbacks, pNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return result;
}

MA_API ma_result ma_notch_node_reinit(const ma_notch_config* pConfig, ma_notch_node* pNode)
{
    ma_notch_node* pBPFNode = (ma_notch_node*)pNode;

    MA_ASSERT(pNode != NULL);

    return ma_notch2_reinit(pConfig, &pBPFNode->notch);
}

MA_API void ma_notch_node_uninit(ma_notch_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pNode, pAllocationCallbacks);
}



/*
Peaking Filter Node
*/
MA_API ma_peak_node_config ma_peak_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double q, double frequency)
{
    ma_peak_node_config config;

    config.nodeConfig = ma_node_config_init();
    config.peak = ma_peak2_config_init(ma_format_f32, channels, sampleRate, gainDB, q, frequency);

    return config;
}

static void ma_peak_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_peak_node* pBPFNode = (ma_peak_node*)pNode;

    MA_ASSERT(pNode != NULL);
    (void)pFrameCountIn;

    ma_peak2_process_pcm_frames(&pBPFNode->peak, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_peak_node_vtable =
{
    ma_peak_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* One input. */
    1,      /* One output. */
    0       /* Default flags. */
};

MA_API ma_result ma_peak_node_init(ma_node_graph* pNodeGraph, const ma_peak_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_peak_node* pNode)
{
    ma_result result;
    ma_node_config baseNodeConfig;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->peak.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* The format must be f32. */
    }

    result = ma_peak2_init(&pConfig->peak, &pNode->peak);
    if (result != MA_SUCCESS) {
        ma_node_uninit(pNode, pAllocationCallbacks);
        return result;
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.vtable          = &g_ma_peak_node_vtable;
    baseNodeConfig.pInputChannels  = &pConfig->peak.channels;
    baseNodeConfig.pOutputChannels = &pConfig->peak.channels;

    result = ma_node_init(pNodeGraph, &baseNodeConfig, pAllocationCallbacks, pNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return result;
}

MA_API ma_result ma_peak_node_reinit(const ma_peak_config* pConfig, ma_peak_node* pNode)
{
    ma_peak_node* pBPFNode = (ma_peak_node*)pNode;

    MA_ASSERT(pNode != NULL);

    return ma_peak2_reinit(pConfig, &pBPFNode->peak);
}

MA_API void ma_peak_node_uninit(ma_peak_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pNode, pAllocationCallbacks);
}



/*
Low Shelf Filter Node
*/
MA_API ma_loshelf_node_config ma_loshelf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double q, double frequency)
{
    ma_loshelf_node_config config;

    config.nodeConfig = ma_node_config_init();
    config.loshelf = ma_loshelf2_config_init(ma_format_f32, channels, sampleRate, gainDB, q, frequency);

    return config;
}

static void ma_loshelf_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_loshelf_node* pBPFNode = (ma_loshelf_node*)pNode;

    MA_ASSERT(pNode != NULL);
    (void)pFrameCountIn;

    ma_loshelf2_process_pcm_frames(&pBPFNode->loshelf, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_loshelf_node_vtable =
{
    ma_loshelf_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* One input. */
    1,      /* One output. */
    0       /* Default flags. */
};

MA_API ma_result ma_loshelf_node_init(ma_node_graph* pNodeGraph, const ma_loshelf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_loshelf_node* pNode)
{
    ma_result result;
    ma_node_config baseNodeConfig;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->loshelf.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* The format must be f32. */
    }

    result = ma_loshelf2_init(&pConfig->loshelf, &pNode->loshelf);
    if (result != MA_SUCCESS) {
        return result;
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.vtable          = &g_ma_loshelf_node_vtable;
    baseNodeConfig.pInputChannels  = &pConfig->loshelf.channels;
    baseNodeConfig.pOutputChannels = &pConfig->loshelf.channels;

    result = ma_node_init(pNodeGraph, &baseNodeConfig, pAllocationCallbacks, pNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return result;
}

MA_API ma_result ma_loshelf_node_reinit(const ma_loshelf_config* pConfig, ma_loshelf_node* pNode)
{
    ma_loshelf_node* pBPFNode = (ma_loshelf_node*)pNode;

    MA_ASSERT(pNode != NULL);

    return ma_loshelf2_reinit(pConfig, &pBPFNode->loshelf);
}

MA_API void ma_loshelf_node_uninit(ma_loshelf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pNode, pAllocationCallbacks);
}



/*
High Shelf Filter Node
*/
MA_API ma_hishelf_node_config ma_hishelf_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, double gainDB, double q, double frequency)
{
    ma_hishelf_node_config config;

    config.nodeConfig = ma_node_config_init();
    config.hishelf = ma_hishelf2_config_init(ma_format_f32, channels, sampleRate, gainDB, q, frequency);

    return config;
}

static void ma_hishelf_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_hishelf_node* pBPFNode = (ma_hishelf_node*)pNode;

    MA_ASSERT(pNode != NULL);
    (void)pFrameCountIn;

    ma_hishelf2_process_pcm_frames(&pBPFNode->hishelf, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_hishelf_node_vtable =
{
    ma_hishelf_node_process_pcm_frames,
    NULL,   /* onGetRequiredInputFrameCount */
    1,      /* One input. */
    1,      /* One output. */
    0       /* Default flags. */
};

MA_API ma_result ma_hishelf_node_init(ma_node_graph* pNodeGraph, const ma_hishelf_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_hishelf_node* pNode)
{
    ma_result result;
    ma_node_config baseNodeConfig;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->hishelf.format != ma_format_f32) {
        return MA_INVALID_ARGS; /* The format must be f32. */
    }

    result = ma_hishelf2_init(&pConfig->hishelf, &pNode->hishelf);
    if (result != MA_SUCCESS) {
        return result;
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.vtable          = &g_ma_hishelf_node_vtable;
    baseNodeConfig.pInputChannels  = &pConfig->hishelf.channels;
    baseNodeConfig.pOutputChannels = &pConfig->hishelf.channels;

    result = ma_node_init(pNodeGraph, &baseNodeConfig, pAllocationCallbacks, pNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return result;
}

MA_API ma_result ma_hishelf_node_reinit(const ma_hishelf_config* pConfig, ma_hishelf_node* pNode)
{
    ma_hishelf_node* pBPFNode = (ma_hishelf_node*)pNode;

    MA_ASSERT(pNode != NULL);

    return ma_hishelf2_reinit(pConfig, &pBPFNode->hishelf);
}

MA_API void ma_hishelf_node_uninit(ma_hishelf_node* pNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_node_uninit(pNode, pAllocationCallbacks);
}



/*
Delay
*/
MA_API ma_delay_config ma_delay_config_init(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 delayInFrames, float decay)
{
    ma_delay_config config;

    MA_ZERO_OBJECT(&config);
    config.channels      = channels;
    config.sampleRate    = sampleRate;
    config.delayInFrames = delayInFrames;
    config.delayStart    = (decay == 0) ? MA_TRUE : MA_FALSE;   /* Delay the start if it looks like we're not configuring an echo. */
    config.wet           = 1;
    config.dry           = 1;
    config.decay         = decay;

    return config;
}


MA_API ma_result ma_delay_init(const ma_delay_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_delay* pDelay)
{
    if (pDelay == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDelay);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->decay < 0 || pConfig->decay > 1) {
        return MA_INVALID_ARGS;
    }

    pDelay->config             = *pConfig;
    pDelay->bufferSizeInFrames = pConfig->delayInFrames;
    pDelay->cursor             = 0;
    
    pDelay->pBuffer = (float*)ma_malloc((size_t)(pDelay->bufferSizeInFrames * ma_get_bytes_per_frame(ma_format_f32, pConfig->channels)), pAllocationCallbacks);
    if (pDelay->pBuffer == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    ma_silence_pcm_frames(pDelay->pBuffer, pDelay->bufferSizeInFrames, ma_format_f32, pConfig->channels);

    return MA_SUCCESS;
}

MA_API void ma_delay_uninit(ma_delay* pDelay, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pDelay == NULL) {
        return;
    }

    ma_free(pDelay->pBuffer, pAllocationCallbacks);
}

MA_API ma_result ma_delay_process_pcm_frames(ma_delay* pDelay, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    ma_uint32 iFrame;
    ma_uint32 iChannel;
    float* pFramesOutF32 = (float*)pFramesOut;
    const float* pFramesInF32 = (const float*)pFramesIn;

    if (pDelay == NULL || pFramesOut == NULL || pFramesIn == NULL) {
        return MA_INVALID_ARGS;
    }

    for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
        for (iChannel = 0; iChannel < pDelay->config.channels; iChannel += 1) {
            ma_uint32 iBuffer = (pDelay->cursor * pDelay->config.channels) + iChannel;

            if (pDelay->config.delayStart) {
                /* Delayed start. */

                /* Read */
                pFramesOutF32[iChannel] = pDelay->pBuffer[iBuffer] * pDelay->config.wet;

                /* Feedback */
                pDelay->pBuffer[iBuffer] = (pDelay->pBuffer[iBuffer] * pDelay->config.decay) + (pFramesInF32[iChannel] * pDelay->config.dry);
            } else {
                /* Immediate start */

                /* Feedback */
                pDelay->pBuffer[iBuffer] = (pDelay->pBuffer[iBuffer] * pDelay->config.decay) + (pFramesInF32[iChannel] * pDelay->config.dry);

                /* Read */
                pFramesOutF32[iChannel] = pDelay->pBuffer[iBuffer] * pDelay->config.wet;
            }
        }

        pDelay->cursor = (pDelay->cursor + 1) % pDelay->bufferSizeInFrames;
        
        pFramesOutF32 += pDelay->config.channels;
        pFramesInF32  += pDelay->config.channels;
    }

    return MA_SUCCESS;
}

MA_API void ma_delay_set_wet(ma_delay* pDelay, float value)
{
    if (pDelay == NULL) {
        return;
    }

    pDelay->config.wet = value;
}

MA_API float ma_delay_get_wet(const ma_delay* pDelay)
{
    if (pDelay == NULL) {
        return 0;
    }

    return pDelay->config.wet;
}

MA_API void ma_delay_set_dry(ma_delay* pDelay, float value)
{
    if (pDelay == NULL) {
        return;
    }

    pDelay->config.dry = value;
}

MA_API float ma_delay_get_dry(const ma_delay* pDelay)
{
    if (pDelay == NULL) {
        return 0;
    }

    return pDelay->config.dry;
}

MA_API void ma_delay_set_decay(ma_delay* pDelay, float value)
{
    if (pDelay == NULL) {
        return;
    }

    pDelay->config.decay = value;
}

MA_API float ma_delay_get_decay(const ma_delay* pDelay)
{
    if (pDelay == NULL) {
        return 0;
    }

    return pDelay->config.decay;
}




MA_API ma_delay_node_config ma_delay_node_config_init(ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 delayInFrames, float decay)
{
    ma_delay_node_config config;

    config.nodeConfig = ma_node_config_init();
    config.delay = ma_delay_config_init(channels, sampleRate, delayInFrames, decay);

    return config;
}


static void ma_delay_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_delay_node* pDelayNode = (ma_delay_node*)pNode;

    (void)pFrameCountIn;

    ma_delay_process_pcm_frames(&pDelayNode->delay, ppFramesOut[0], ppFramesIn[0], *pFrameCountOut);
}

static ma_node_vtable g_ma_delay_node_vtable =
{
    ma_delay_node_process_pcm_frames,
    NULL,
    1,  /* 1 input channels. */
    1,  /* 1 output channel. */
    MA_NODE_FLAG_CONTINUOUS_PROCESSING  /* Delay requires continuous processing to ensure the tail get's processed. */
};

MA_API ma_result ma_delay_node_init(ma_node_graph* pNodeGraph, const ma_delay_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_delay_node* pDelayNode)
{
    ma_result result;
    ma_node_config baseConfig;

    if (pDelayNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDelayNode);

    result = ma_delay_init(&pConfig->delay, pAllocationCallbacks, &pDelayNode->delay);
    if (result != MA_SUCCESS) {
        return result;
    }

    baseConfig = pConfig->nodeConfig;
    baseConfig.vtable          = &g_ma_delay_node_vtable;
    baseConfig.pInputChannels  = &pConfig->delay.channels;
    baseConfig.pOutputChannels = &pConfig->delay.channels;

    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pDelayNode->baseNode);
    if (result != MA_SUCCESS) {
        ma_delay_uninit(&pDelayNode->delay, pAllocationCallbacks);
        return result;
    }

    return result;
}

MA_API void ma_delay_node_uninit(ma_delay_node* pDelayNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pDelayNode == NULL) {
        return;
    }

    /* The base node is always uninitialized first. */
    ma_node_uninit(pDelayNode, pAllocationCallbacks);
    ma_delay_uninit(&pDelayNode->delay, pAllocationCallbacks);
}

MA_API void ma_delay_node_set_wet(ma_delay_node* pDelayNode, float value)
{
    if (pDelayNode == NULL) {
        return;
    }

    ma_delay_set_wet(&pDelayNode->delay, value);
}

MA_API float ma_delay_node_get_wet(const ma_delay_node* pDelayNode)
{
    if (pDelayNode == NULL) {
        return 0;
    }

    return ma_delay_get_wet(&pDelayNode->delay);
}

MA_API void ma_delay_node_set_dry(ma_delay_node* pDelayNode, float value)
{
    if (pDelayNode == NULL) {
        return;
    }

    ma_delay_set_dry(&pDelayNode->delay, value);
}

MA_API float ma_delay_node_get_dry(const ma_delay_node* pDelayNode)
{
    if (pDelayNode == NULL) {
        return 0;
    }

    return ma_delay_get_dry(&pDelayNode->delay);
}

MA_API void ma_delay_node_set_decay(ma_delay_node* pDelayNode, float value)
{
    if (pDelayNode == NULL) {
        return;
    }

    ma_delay_set_decay(&pDelayNode->delay, value);
}

MA_API float ma_delay_node_get_decay(const ma_delay_node* pDelayNode)
{
    if (pDelayNode == NULL) {
        return 0;
    }

    return ma_delay_get_decay(&pDelayNode->delay);
}

#endif
