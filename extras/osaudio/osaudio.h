/*
This is a simple API for low-level audio playback and capture. A reference implementation using
miniaudio is provided in osaudio.c which can be found alongside this file. Consider all code
public domain.

The idea behind this project came about after considering the absurd complexity of audio APIs on
various platforms after years of working on miniaudio. This project aims to disprove the idea that
complete and flexible audio solutions and simple APIs are mutually exclusive and that it's possible
to have both. The idea of reliability through simplicity is the first and foremost goal of this
project. The difference between this project and miniaudio is that this project is designed around
the idea of what I would build if I was building an audio API for an operating system, such as at
the level of WASAPI or ALSA. A cross-platform and cross-backend library like miniaudio is
necessarily different in design, but there are indeed things that I would have done differently if
given my time again, some of those ideas of which I'm expressing in this project.

---

The concept of low-level audio is simple - you have a device, such as a speaker system or a
micrphone system, and then you write or read audio data to/from it. So in the case of playback, you
need only write your raw audio data to the device which then emits it from the speakers when it's
ready. Likewise, for capture you simply read audio data from the device which is filled with data
by the microphone.

A complete low-level audio solution requires the following:

    1) The ability to enumerate devices that are connected to the system.
    2) The ability to open and close a connection to a device.
    3) The ability to start and stop the device.
    4) The ability to write and read audio data to/from the device.
    5) The ability to query the device for its data configuration.
    6) The ability to notify the application when certain events occur, such as the device being
       stopped, or rerouted.

The API presented here aims to meet all of the above requirements. It uses a single-threaded
blocking read/write model for data delivery instead of a callback model. This makes it a bit more
flexible since it gives the application full control over the audio thread. It might also make it
more feasible to use this API on single-threaded systems.

Device enumeration is achieved with a single function: osaudio_enumerate(). This function returns
an array of osaudio_info_t structures which contain information about each device. The array is
allocated must be freed with free(). Contained within the osaudio_info_t struct is, most
importantly, the device ID, which is used to open a connection to the device, and the name of the
device which can be used to display to the user. For advanced users, it also includes information
about the device's native data configuration.

Opening and closing a connection to a device is achieved with osaudio_open() and osaudio_close().
An important concept is that of the ability to configure the device. This is achieved with the
osaudio_config_t structure which is passed to osaudio_open(). In addition to the ID of the device,
this structure includes information about the desired format, channel count and sample rate. You
can also configure the latency of the device, or the buffer size, which is specified in frames. A
flags member is used for specifying additional options, such as whether or not to disable automatic
rerouting. Finally a callback can be specified for notifications. When osaudio_open() returns, the
config structure will be filled with the device's actual configuration. You can inspect the channel
map from this structure to know how to arrange the channels in your audio data.

This API uses a blocking write/read model for pushing and pulling data to/from the device. This
is done with the osaudio_write() and osaudio_read() functions. These functions will block until
the requested number of frames have been processed or the device is drained or flushed with
osaudio_drain() or osaudio_flush() respectively. It is from these functions that the device is
started. As soon as you start writing data with osaudio_write() or reading data with
osaudio_read(), the device will start. When the device is drained of flushed with osaudio_drain()
or osaudio_flush(), the device will be stopped. osaudio_drain() will block until the device has
been drained, whereas osaudio_flush() will stop playback immediately and return. You can also pause
and resume the device with osaudio_pause() and osaudio_resume(). Since reading and writing is
blocking, it can be useful to know how many frames can be written/read without blocking. This is
achieved with osaudio_get_avail().

Querying the device's configuration is achieved with osaudio_get_info(). This function will return
a pointer to an osaudio_info_t structure which contains information about the device, most
importantly its name and data configuration. The name is important for displaying on a UI, and
the data configuration is important for knowing how to format your audio data. The osaudio_info_t
structure will contain an array of osaudio_config_t structures. This will contain one entry, which
will contain the exact information that was returned in the config structure that was passed to
osaudio_open().

A common requirement is to open a device that represents the operating system's default device.
This is done easily by simply passing in NULL for the device ID. Below is an example for opening a
default device:

    int result;
    osaudio_t audio;
    osaudio_config_t config;

    osaudio_config_init(&config, OSAUDIO_OUTPUT);
    config.format   = OSAUDIO_FORMAT_F32;
    config.channels = 2;
    config.rate     = 48000;

    result = osaudio_open(&audio, &config);
    if (result != OSAUDIO_SUCCESS) {
        printf("Failed to open device.");
        return -1;
    }

    ...

    osaudio_close(audio);

In the above example, the default device is opened for playback (OSAUDIO_OUTPUT). The format is
set to 32-bit floating point (OSAUDIO_FORMAT_F32), the channel count is set to stereo (2), and the
sample rate is set to 48kHz. The device is then closed when we're done with it.

If instead we wanted to open a specific device, we can do that by passing in the device ID. Below
is an example for how to do this:

    int result;
    osaudio_t audio;
    osaudio_config_t config;
    unsigned int infoCount;
    osaudio_info_t* info;

    result = osaudio_enumerate(&infoCount, &info);
    if (result != OSAUDIO_SUCCESS) {
        printf("Failed to enumerate devices.\n");
        return -1;
    }

    // ... Iterate over the `info` array and find the device you want to open. Use the `direction` member to discriminate between input and output ...

    osaudio_config_init(&config, OSAUDIO_OUTPUT);
    config.id       = &info[indexOfYourChosenDevice].id;
    config.format   = OSAUDIO_FORMAT_F32;
    config.channels = 2;
    config.rate     = 48000;

    osaudio_open(&audio, &config);

    ...

    osaudio_close(audio);
    free(info); // The pointer returned by osaudio_enumerate() must be freed with free().

The id structure is just a 256 byte array that uniquely identifies the device. Implementations may
have different representations for device IDs, and A 256 byte array should accommodates all
device ID representations. Implementations are required to zero-fill unused bytes. The osaudio_id_t
structure can be copied which makes it suitable for serialization and deserialization in situations
where you may want to save the device ID to permanent storage so it can be stored in a config file.

Implementations need to do their own data conversion between the device's native data configuration
and the requested configuration. In this case, when the format, channels and rate are specified in
the config, they should be unchanged when osaudio_open() returns. If this is not possible,
osaudio_open() will return OSAUDIO_FORMAT_NOT_SUPPORTED. However, there are cases where it's useful
for a program to use the device's native configuration instead of some fixed configuration. This is
achieved by setting the format, channels and rate to 0. Below is an example:

    int result;
    osaudio_t audio;
    osaudio_config_t config;

    osaudio_config_init(&config, OSAUDIO_OUTPUT);

    result = osaudio_open(&audio, &config);
    if (result != OSAUDIO_SUCCESS) {
        printf("Failed to open device.");
        return -1;
    }

    // ... `config` will have been updated by osaudio_open() to contain the *actual* format/channels/rate ...

    osaudio_close(audio);

In addition to the code above, you can explicitly call `osaudio_get_info()` to retrieve the format
configuration. If you need to know the native configuration before opening the device, you can use
enumeration. The format, channels and rate will be continued in the first item in the configs array.

The examples above all use playback, but the same applies for capture. The only difference is that
the direction is set to OSAUDIO_INPUT instead of OSAUDIO_OUTPUT.

To output audio from the speakers you need to call osaudio_write(). Likewise, to capture audio from
a microphone you need to call osaudio_read(). These functions will block until the requested number
of frames have been written or read. The device will start automatically. Below is an example for
writing some data to a device:

    int result = osaudio_write(audio, myAudioData, myAudioDataFrameCount);
    if (result == OSAUDIO_SUCCESS) {
        printf("Successfully wrote %d frames of audio data.\n", myAudioDataFrameCount);
    } else {
        printf("Failed to write audio data.\n");
    }

osaudio_write() and osaudio_read() will return OSAUDIO_SUCCESS if the requested number of frames
were written or read. You cannot call osaudio_close() while a write or read operation is in
progress.

If you want to write or read audio data without blocking, you can use osaudio_get_avail() to
determine how many frames are available for writing or reading. Below is an example:

    unsigned int framesAvailable = osaudio_get_avail(audio);
    if (result > 0) {
        printf("There are %d frames available for writing.\n", framesAvailable);
    } else {
        printf("There are no frames available for writing.\n");
    }

If you want to abort a blocking write or read, you can use osaudio_flush(). This will result in any
pending write or read operation being aborted.

There are several ways of pausing a device. The first is to just drain or flush the device and
simply don't do any more read/write operations. A drain and flush will put the device into a
stopped state until the next call to either read or write, depending on the device's direction.
If, however, this does not suit your requirements, you can use osaudio_pause() and
osaudio_resume(). Take note, however, that these functions will result in osaudio_drain() never
returning because it'll result in the device being in a stopped state which in turn results in the
buffer never being read and therefore never drained.

Everything is thread safe with a few minor exceptions which has no practical issues for the client:
    
    * You cannot call any function while osaudio_open() is still in progress.
    * You cannot call osaudio_close() while any other function is still in progress.
    * You can only call osaudio_write() and osaudio_read() from one thread at a time.

None of these issues should be a problem for the client in practice. You won't have a valid
osaudio_t object until osaudio_open() has returned. For osaudio_close(), it makes no sense to
destroy the object while it's still in use, and doing so would mean the client is using very poor
form. For osaudio_write() and osaudio_read(), you wouldn't ever want to call this simultaneously
across multiple threads anyway because otherwise you'd end up with garbage audio.

The rules above only apply when working with a single osaudio_t object. You can have multiple
osaudio_t objects open at the same time, and you can call any function on different osaudio_t
objects simultaneously from different threads.

---

# Feedback

I'm looking for feedback on the following:

  * Are the supported formats enough? If not, what other formats are needed, and what is the
    justification for including it? Just because it's the native format on one particular
    piece of hardware is not enough. Big-endian and little-endian will never be supported. All
    formats are native-endian.
  * Are the available channel positions enough? What other positions are needed?
  * Just some general criticism would be appreciated.

*/
#ifndef osaudio_h
#define osaudio_h

#ifdef __cplusplus
extern "C" {
#endif

/*
Support far pointers on relevant platforms (DOS, in particular). The version of this file
distributed with an operating system wouldn't need this because they would just have an
OS-specific version of this file, but as a reference it's useful to use far pointers here.
*/
#if defined(__MSDOS__) || defined(_MSDOS) || defined(__DOS__)
    #define OSAUDIO_FAR far
#else
    #define OSAUDIO_FAR
#endif

typedef struct _osaudio_t*                      osaudio_t;
typedef struct osaudio_config_t                 osaudio_config_t;
typedef struct osaudio_id_t                     osaudio_id_t;
typedef struct osaudio_info_t                   osaudio_info_t;
typedef struct osaudio_notification_t           osaudio_notification_t;

/* Results codes. */
typedef int osaudio_result_t;
#define OSAUDIO_SUCCESS                         0
#define OSAUDIO_ERROR                          -1
#define OSAUDIO_INVALID_ARGS                   -2
#define OSAUDIO_INVALID_OPERATION              -3
#define OSAUDIO_OUT_OF_MEMORY                  -4
#define OSAUDIO_FORMAT_NOT_SUPPORTED           -101   /* The requested format is not supported. */
#define OSAUDIO_XRUN                           -102   /* An underrun or overrun occurred. Can be returned by osaudio_read() or osaudio_write(). */
#define OSAUDIO_DEVICE_STOPPED                 -103   /* The device is stopped. Can be returned by osaudio_drain(). It is invalid to call osaudio_drain() on a device that is not running because otherwise it'll get stuck. */

/* Directions. Cannot be combined. Use separate osaudio_t objects for bidirectional setups. */
typedef int osaudio_direction_t;
#define OSAUDIO_INPUT                           1
#define OSAUDIO_OUTPUT                          2

/* All formats are native endian and interleaved. */
typedef int osaudio_format_t;
#define OSAUDIO_FORMAT_UNKNOWN                  0
#define OSAUDIO_FORMAT_F32                      1
#define OSAUDIO_FORMAT_U8                       2
#define OSAUDIO_FORMAT_S16                      3
#define OSAUDIO_FORMAT_S24                      4   /* Tightly packed. */
#define OSAUDIO_FORMAT_S32                      5

/* Channel positions. */
typedef unsigned char osaudio_channel_t;
#define OSAUDIO_CHANNEL_NONE                    0
#define OSAUDIO_CHANNEL_MONO                    1
#define OSAUDIO_CHANNEL_FL                      2
#define OSAUDIO_CHANNEL_FR                      3
#define OSAUDIO_CHANNEL_FC                      4
#define OSAUDIO_CHANNEL_LFE                     5
#define OSAUDIO_CHANNEL_BL                      6
#define OSAUDIO_CHANNEL_BR                      7
#define OSAUDIO_CHANNEL_FLC                     8
#define OSAUDIO_CHANNEL_FRC                     9
#define OSAUDIO_CHANNEL_BC                      10
#define OSAUDIO_CHANNEL_SL                      11
#define OSAUDIO_CHANNEL_SR                      12
#define OSAUDIO_CHANNEL_TC                      13
#define OSAUDIO_CHANNEL_TFL                     14
#define OSAUDIO_CHANNEL_TFC                     15
#define OSAUDIO_CHANNEL_TFR                     16
#define OSAUDIO_CHANNEL_TBL                     17
#define OSAUDIO_CHANNEL_TBC                     18
#define OSAUDIO_CHANNEL_TBR                     19
#define OSAUDIO_CHANNEL_AUX0                    20
#define OSAUDIO_CHANNEL_AUX1                    21
#define OSAUDIO_CHANNEL_AUX2                    22
#define OSAUDIO_CHANNEL_AUX3                    23
#define OSAUDIO_CHANNEL_AUX4                    24
#define OSAUDIO_CHANNEL_AUX5                    25
#define OSAUDIO_CHANNEL_AUX6                    26
#define OSAUDIO_CHANNEL_AUX7                    27
#define OSAUDIO_CHANNEL_AUX8                    28
#define OSAUDIO_CHANNEL_AUX9                    29
#define OSAUDIO_CHANNEL_AUX10                   30
#define OSAUDIO_CHANNEL_AUX11                   31
#define OSAUDIO_CHANNEL_AUX12                   32
#define OSAUDIO_CHANNEL_AUX13                   33
#define OSAUDIO_CHANNEL_AUX14                   34
#define OSAUDIO_CHANNEL_AUX15                   35
#define OSAUDIO_CHANNEL_AUX16                   36
#define OSAUDIO_CHANNEL_AUX17                   37
#define OSAUDIO_CHANNEL_AUX18                   38
#define OSAUDIO_CHANNEL_AUX19                   39
#define OSAUDIO_CHANNEL_AUX20                   40
#define OSAUDIO_CHANNEL_AUX21                   41
#define OSAUDIO_CHANNEL_AUX22                   42
#define OSAUDIO_CHANNEL_AUX23                   43
#define OSAUDIO_CHANNEL_AUX24                   44
#define OSAUDIO_CHANNEL_AUX25                   45
#define OSAUDIO_CHANNEL_AUX26                   46
#define OSAUDIO_CHANNEL_AUX27                   47
#define OSAUDIO_CHANNEL_AUX28                   48
#define OSAUDIO_CHANNEL_AUX29                   49
#define OSAUDIO_CHANNEL_AUX30                   50
#define OSAUDIO_CHANNEL_AUX31                   51

/* The maximum number of channels supported. */
#define OSAUDIO_MAX_CHANNELS                    64

/* Notification types. */
typedef int osaudio_notification_type_t;
#define OSAUDIO_NOTIFICATION_STARTED            0   /* The device was started in response to a call to osaudio_write() or osaudio_read(). */
#define OSAUDIO_NOTIFICATION_STOPPED            1   /* The device was stopped in response to a call to osaudio_drain() or osaudio_flush(). */
#define OSAUDIO_NOTIFICATION_REROUTED           2   /* The device was rerouted. Not all implementations need to support rerouting. */
#define OSAUDIO_NOTIFICATION_INTERRUPTION_BEGIN 3   /* The device was interrupted due to something like a phone call. */
#define OSAUDIO_NOTIFICATION_INTERRUPTION_END   4   /* The interruption has been ended. */

/* Flags. */
#define OSAUDIO_FLAG_NO_REROUTING               1   /* When set, will tell the implementation to disable automatic rerouting if possible. This is a hint and may be ignored by the implementation. */
#define OSAUDIO_FLAG_REPORT_XRUN                2   /* When set, will tell the implementation to report underruns and overruns via osaudio_write() and osaudio_read() by aborting and returning OSAUDIO_XRUN. */

struct osaudio_notification_t
{
    osaudio_notification_type_t type;   /* OSAUDIO_NOTIFICATION_* */
    union
    {
        struct
        {
            int _unused;
        } started;
        struct
        {
            int _unused;
        } stopped;
        struct
        {
            int _unused;
        } rerouted;
        struct
        {
            int _unused;
        } interruption;
    } data;
};

struct osaudio_id_t
{
    char data[256];
};

struct osaudio_config_t
{
    osaudio_id_t* device_id;        /* Set to NULL to use default device. When non-null, automatic routing will be disabled. */
    osaudio_direction_t direction;  /* OSAUDIO_INPUT or OSAUDIO_OUTPUT. Cannot be combined. Use separate osaudio_t objects for bidirectional setups. */
    osaudio_format_t format;        /* OSAUDIO_FORMAT_* */
    unsigned int channels;          /* Number of channels. */
    unsigned int rate;              /* Sample rate in seconds. */
    osaudio_channel_t channel_map[OSAUDIO_MAX_CHANNELS];  /* Leave all items set to 0 for defaults. */
    unsigned int buffer_size;       /* In frames. Set to 0 to use the system default. */
    unsigned int flags;             /* A combination of OSAUDIO_FLAG_* */
    void (* notification)(void* user_data, const osaudio_notification_t* notification); /* Called when some kind of event occurs, such as a device being closed. Never called from the audio thread. */
    void* user_data;                /* Passed to notification(). */
};

struct osaudio_info_t
{
    osaudio_id_t id;
    char name[256];
    osaudio_direction_t direction;  /* OSAUDIO_INPUT or OSAUDIO_OUTPUT. */
    unsigned int config_count;
    osaudio_config_t* configs;
};


/*
Enumerates the available devices.

On output, `count` will contain the number of items in the `info` array. The array must be freed
with free() when it's no longer needed.

Use the `direction` member to discriminate between input and output devices. Below is an example:

    unsigned int count;
    osaudio_info_t* info;
    osaudio_enumerate(&count, &info);

    for (int i = 0; i < count; ++i) {
        if (info[i].direction == OSAUDIO_OUTPUT) {
            printf("Output device: %s\n", info[i].name);
        } else {
            printf("Input device: %s\n", info[i].name);
        }
    }

You can use the `id` member to open a specific device with osaudio_open(). You do not need to do
device enumeration if you only want to open the default device.
*/
osaudio_result_t osaudio_enumerate(unsigned int* count, osaudio_info_t** info);

/*
Initializes a default config.

The config object will be cleared to zero, with the direction set to `direction`. This will result
in a configuration that uses the device's native format, channels and rate.

osaudio_config_t is a transparent struct. Just set the relevant fields to the desired values after
calling this function. Example:

    osaudio_config_t config;
    osaudio_config_init(&config, OSAUDIO_OUTPUT);
    config.format   = OSAUDIO_FORMAT_F32;
    config.channels = 2;
    config.rate     = 48000;
*/
void osaudio_config_init(osaudio_config_t* config, osaudio_direction_t direction);

/*
Opens a connection to a device.

On input, config must be filled with the desired configuration. On output, it will be filled with
the actual configuration.

Initialize the config with osaudio_config_init() and then fill in the desired configuration. Below
is an example:

    osaudio_config_t config;
    osaudio_config_init(&config, OSAUDIO_OUTPUT);
    config.format   = OSAUDIO_FORMAT_F32;
    config.channels = 2;
    config.rate     = 48000;

When the format, channels or rate are left at their default values, or set to 0 (or
OSAUDIO_FORMAT_UNKNOWN for format), the native format, channels or rate will use the device's
native configuration:

    osaudio_config_t config;
    osaudio_config_init(&config, OSAUDIO_OUTPUT);
    config.format   = OSAUDIO_FORMAT_UNKNOWN;
    config.channels = 0;
    config.rate     = 0;

The code above is equivalent to this:

    osaudio_config_t config;
    osaudio_config_init(&config, OSAUDIO_OUTPUT);

On output the config will be filled with the actual configuration. The implementation will perform
any necessary data conversion between the requested data configuration and the device's native
configuration. If it cannot, the function will return a OSAUDIO_FORMAT_NOT_SUPPORTED error. In this
case the caller can decide to reinitialize the device to use its native configuration and do its
own data conversion, or abort if it cannot do so. Use the channel map to determine the ordering of
your channels. Automatic channel map conversion is not performed - that must be done manually by
the caller when transferring data to/from the device.

Close the device with osaudio_close().

Returns 0 on success, any other error code on failure.
*/
osaudio_result_t osaudio_open(osaudio_t* audio, osaudio_config_t* config);

/*
Closes a connection to a device.

As soon as this function is called, the device should be considered invalid and unusable. Do not
attempt to use the audio object once this function has been called.

It's invalid to call this while any other function is still running. You can use osaudio_flush() to
quickly abort any pending writes or reads. You can also use osaudio_drain() to wait for all pending
writes or reads to complete.

Returns 0 on success, < 0 on failure.
*/
osaudio_result_t osaudio_close(osaudio_t audio);

/*
Writes audio data to the device.

This will block until all data has been written or the device is closed.

You can only write from a single thread at any given time. If you want to write from multiple
threads, you need to use your own synchronization mechanism.

This will automatically start the device if frame_count is > 0 and it's not in a paused state.

Use osaudio_get_avail() to determine how much data can be written without blocking.

Returns 0 on success, < 0 on failure.
*/
osaudio_result_t osaudio_write(osaudio_t audio, const void OSAUDIO_FAR* data, unsigned int frame_count);

/*
Reads audio data from the device.

This will block until the requested number of frames has been read or the device is closed.

You can only read from a single thread at any given time. If you want to read from multiple
threads, you need to use your own synchronization mechanism.

This will automatically start the device if frame_count is > 0 and it's not in a paused state.

Use osaudio_get_avail() to determine how much data can be read without blocking.

Returns 0 on success, < 0 on failure.
*/
osaudio_result_t osaudio_read(osaudio_t audio, void OSAUDIO_FAR* data, unsigned int frame_count);

/*
Drains the device.

This will block until all pending reads or writes have completed.

If after calling this function another call to osaudio_write() or osaudio_read() is made, the
device will be resumed like normal.

It is invalid to call this while the device is paused.

Returns 0 on success, < 0 on failure.
*/
osaudio_result_t osaudio_drain(osaudio_t audio);

/*
Flushes the device.

This will immediately flush any pending reads or writes. It will not block. Any in-progress reads
or writes will return immediately.

If after calling this function another thread starts reading or writing, the device will be resumed
like normal.

Returns 0 on success, < 0 on failure.
*/
osaudio_result_t osaudio_flush(osaudio_t audio);

/*
Pauses or resumes the device.

Pausing a device will trigger a OSAUDIO_NOTIFICATION_STOPPED notification. Resuming a device will
trigger a OSAUDIO_NOTIFICATION_STARTED notification.

Returns 0 on success, < 0 on failure.
*/
osaudio_result_t osaudio_pause(osaudio_t audio);

/*
Resumes the device.

Returns 0 on success, < 0 on failure.
*/
osaudio_result_t osaudio_resume(osaudio_t audio);

/*
Returns the number of frames that can be read or written without blocking.
*/
unsigned int osaudio_get_avail(osaudio_t audio);

/*
Gets information about the device.

There will be one item in the configs array which will contain the device's current configuration,
the contents of which will match that of the config that was returned by osaudio_open().

Returns NULL on failure. Do not free the returned pointer. It's up to the implementation to manage
the memory of this object.
*/
const osaudio_info_t* osaudio_get_info(osaudio_t audio);


#ifdef __cplusplus
}
#endif
#endif  /* osaudio_h */
