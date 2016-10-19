// Mini audio library. Public domain. See "unlicense" statement at the end of this file.
// mini_al - v0.0 - UNRELEASED
//
// David Reid - mackron@gmail.com

// ABOUT
// =====
// mini_al is a small library for making it easy to do audio playback and recording. It's focused on
// simplicity and being light-weight so don't expect all the bells and whistles.
//
// mini_al uses an asynchronous API. Every device is created with it's own thread, with audio data
// being either delivered to the application from the device (in the case of recording/capture) or
// delivered from the application to the device in the case of playback. Synchronous APIs are not
// supported in the interest of keeping the library as small and lightweight as possible.
//
// Supported backends:
//   - DirectSound (Windows Only)
//   - ALSA (Linux Only)
//   - null
//   - ... and many more in the future.
//
//
// USAGE
// =====
// mini_al is a single-file library. To use it, do something like the following in one .c file.
//   #define MAL_IMPLEMENTATION
//   #include "mini_al.h"
//
//
// NOTES
// =====
// - This library uses an asynchronous API for delivering and requesting audio data. Each device will have
//   it's own worker thread which is managed by the library.
// - If mal_device_init() is called with a device that's not aligned to the platform's natural alignment
//   boundary (4 bytes on 32-bit, 8 bytes on 64-bit), it will _not_ be thread-safe. The reason for this
//   is that it depends on members of mal_device being correctly aligned for atomic assignments and bit
//   manipulation.
// - Sample data is always little-endian and interleaved. For example, mal_format_s16 means signed 16-bit
//   integer samples, interleaved. Let me know if you need non-interleaved and I'll look into it.
//
//
// BACKEND NUANCES
// ===============

#ifndef mini_al_h
#define mini_al_h

#ifdef __cplusplus
extern "C" {
#endif

// Platform/backend detection.
#ifdef _WIN32
	#define MAL_WIN32
	#ifndef MAL_NO_DSOUND
		#define MAL_ENABLE_DSOUND
	#endif
#else
	#define MAL_POSIX
	#if !defined(MAL_NO_ALSA) && defined(__linux__)
		#define MAL_ENABLE_ALSA
	#endif
	#include <pthread.h>    // Unfortunate #include, but needed for pthread_t, pthread_mutex_t and pthread_cond_t types.
#endif

#ifndef MAL_NO_NULL
	#define MAL_ENABLE_NULL
#endif


#if defined(_MSC_VER) && _MSC_VER < 1600
typedef   signed char    mal_int8;
typedef unsigned char    mal_uint8;
typedef   signed short   mal_int16;
typedef unsigned short   mal_uint16;
typedef   signed int     mal_int32;
typedef unsigned int     mal_uint32;
typedef   signed __int64 mal_int64;
typedef unsigned __int64 mal_uint64;
#else
#include <stdint.h>
typedef int8_t           mal_int8;
typedef uint8_t          mal_uint8;
typedef int16_t          mal_int16;
typedef uint16_t         mal_uint16;
typedef int32_t          mal_int32;
typedef uint32_t         mal_uint32;
typedef int64_t          mal_int64;
typedef uint64_t         mal_uint64;
#endif
typedef mal_int8         mal_bool8;
typedef mal_int32        mal_bool32;
#define MAL_TRUE         1
#define MAL_FALSE        0

typedef void* mal_handle;
typedef void* mal_ptr;

#ifdef MAL_WIN32
	typedef mal_handle mal_thread;
	typedef mal_handle mal_event;
#else
	typedef pthread_t mal_thread;
    typedef struct
    {
        pthread_mutex_t mutex;
        pthread_cond_t condition; 
        mal_uint32 value;
    } mal_event;
#endif

#ifdef MAL_ENABLE_DSOUND
    #define MAL_MAX_FRAGMENTS_DSOUND    16
#endif

typedef int mal_result;
#define MAL_SUCCESS                              0
#define MAL_ERROR                               -1      // A generic error.
#define MAL_INVALID_ARGS                        -2
#define MAL_OUT_OF_MEMORY                       -3
#define MAL_FORMAT_NOT_SUPPORTED                -4
#define MAL_NO_BACKEND                          -5
#define MAL_NO_DEVICE                           -6
#define MAL_API_NOT_FOUND                       -7
#define MAL_DEVICE_BUSY                         -8
#define MAL_DEVICE_NOT_INITIALIZED              -9
#define MAL_DEVICE_ALREADY_STARTED              -10
#define MAL_DEVICE_ALREADY_STARTING             -11
#define MAL_DEVICE_ALREADY_STOPPED              -12
#define MAL_DEVICE_ALREADY_STOPPING             -13
#define MAL_FAILED_TO_MAP_DEVICE_BUFFER         -14
#define MAL_FAILED_TO_INIT_BACKEND              -15
#define MAL_FAILED_TO_READ_DATA_FROM_CLIENT     -16
#define MAL_FAILED_TO_START_BACKEND_DEVICE      -17
#define MAL_FAILED_TO_STOP_BACKEND_DEVICE       -18
#define MAL_FAILED_TO_CREATE_EVENT              -19
#define MAL_FAILED_TO_CREATE_THREAD             -20
#define MAL_DSOUND_FAILED_TO_CREATE_DEVICE      -1024
#define MAL_DSOUND_FAILED_TO_SET_COOP_LEVEL     -1025
#define MAL_DSOUND_FAILED_TO_CREATE_BUFFER      -1026
#define MAL_DSOUND_FAILED_TO_QUERY_INTERFACE    -1027
#define MAL_DSOUND_FAILED_TO_SET_NOTIFICATIONS  -1028
#define MAL_ALSA_FAILED_TO_OPEN_DEVICE          -2048
#define MAL_ALSA_FAILED_TO_SET_HW_PARAMS        -2049
#define MAL_ALSA_FAILED_TO_SET_SW_PARAMS        -2050

typedef struct mal_device mal_device;

typedef void       (* mal_recv_proc)(mal_device* pDevice, mal_uint32 sampleCount, const void* pSamples);
typedef mal_uint32 (* mal_send_proc)(mal_device* pDevice, mal_uint32 sampleCount, void* pSamples);
typedef void       (* mal_log_proc) (mal_device* pDevice, const char* message);

typedef enum
{
	mal_api_null,
	mal_api_dsound,
	mal_api_alsa
} mal_api;

typedef enum
{
	mal_device_type_playback,
	mal_device_type_capture
} mal_device_type;

typedef enum
{
	// I like to keep these explicitly defined because they're used as a key into a lookup table. When items are
    // added to this, make sure there are no gaps and that they're added to the lookup table in mal_get_sample_size_in_bytes().
	mal_format_u8    = 0,
	mal_format_s16   = 1,
	mal_format_s24   = 2,   // Tightly packed. 3 bytes per sample.
	mal_format_s32   = 3,
	mal_format_f32   = 4,
	mal_format_f64   = 5,
	mal_format_alaw  = 6,
	mal_format_mulaw = 7
} mal_format;

typedef union
{
	char str[32];		// ALSA uses a name string for identification.
	mal_uint8 guid[16];	// DirectSound uses a GUID to identify a device.
} mal_device_id;

typedef struct
{
	mal_device_id id;
    char name[256];
} mal_device_info;

typedef struct
{
    int64_t counter;
} mal_timer;


struct mal_device
{
	mal_api api;		    // DirectSound, ALSA, etc.
	mal_device_type type;
	mal_format format;
	mal_uint32 channels;
	mal_uint32 sampleRate;
	mal_uint32 fragmentSizeInFrames;
	mal_uint32 fragmentCount;
	mal_uint32 state;
	mal_recv_proc onRecv;
	mal_send_proc onSend;
    mal_log_proc onLog;
	void* pUserData;	    // Application defined data.
	mal_event wakeupEvent;
    mal_event startEvent;
    mal_event stopEvent;
    mal_thread thread;
    mal_result workResult;  // This is set by the worker thread after it's finished doing a job.

	union
	{
	#ifdef MAL_ENABLE_DSOUND
		struct
		{
			/*HMODULE*/ mal_handle hDSoundDLL;
            /*LPDIRECTSOUND8*/ mal_ptr pPlayback;
            /*LPDIRECTSOUNDBUFFER*/ mal_ptr pPlaybackPrimaryBuffer;
            /*LPDIRECTSOUNDBUFFER*/ mal_ptr pPlaybackBuffer;
            /*LPDIRECTSOUNDCAPTURE8*/ mal_ptr pCapture;
            /*LPDIRECTSOUNDCAPTUREBUFFER8*/ mal_ptr pCaptureBuffer;
            /*LPDIRECTSOUNDNOTIFY*/ mal_ptr pNotify;
            /*HANDLE*/ mal_handle pNotifyEvents[MAL_MAX_FRAGMENTS_DSOUND];  // One event handle for each fragment.
            /*HANDLE*/ mal_handle hStopEvent;
            mal_uint32 lastProcessedFrame;      // This is circular.
            mal_bool32 breakFromMainLoop;
		} dsound;
	#endif
		
	#ifdef MAL_ENABLE_ALSA
		struct
		{
			/*snd_pcm_t**/mal_ptr pPCM;
			mal_bool32 isUsingMMap;
			mal_bool32 breakFromMainLoop;
			void* pIntermediaryBuffer;
		} alsa;
	#endif
		
	#ifdef MAL_ENABLE_NULL
		struct
		{
            mal_timer timer;
            mal_uint32 lastProcessedFrame;      // This is circular.
			mal_bool32 breakFromMainLoop;
            mal_uint8* pBuffer;                 // This is malloc()'d and is used as the destination for reading from the client. Typed as mal_uint8 for easy offsetting.
		} null_device;
	#endif
	};
};

// Enumerates over each device of the given type (playback or capture).
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_NO_BACKEND
//       There is no supported backend, or there was an error loading it (such as a missing dll/so).
//
// Thread Safety: SAFE, SEE NOTES.
//   This API uses an application-defined buffer for output. This is thread-safe so long as the
//   application ensures mutal exclusion to the output buffer at their level.
//
// Efficiency: LOW
//   This API dynamically links to backend DLLs/SOs (such as dsound.dll).
mal_result mal_enumerate_devices(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo);

// Initializes a device in asynchronous mode.
//
// The device ID (pDeviceID) can be null, in which case the default device is used. Otherwise, you
// can retrieve the ID by calling mal_enumerate_devices() and retrieve the ID from the returned
// information.
//
// This will try it's hardest to create a valid device, even if it means adjusting input arguments.
// Look at pDevice->channels, pDevice->sampleRate, etc. to determine the actual properties after
// initialization.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_NO_BACKEND
//       There is no supported backend, or there was an error loading it (such as a missing dll/so).
//   - MAL_OUT_OF_MEMORY
//       A necessary memory allocation failed, likely due to running out of memory. The only backend
//       that performs a memory allocation is ALSA when mmap mode is not supported.
//   - MAL_FORMAT_NOT_SUPPORTED
//       The specified format is not supported by the backend. mini_al does not currently do any
//       software format conversions which means initialization must fail if the backend device does
//       not natively support it.
//   - MAL_FAILED_TO_INIT_BACKEND
//       There was a backend-specific error during initialization.
//
// Thread Safety: ???
//   This API is thread safe so long as the application does not try to use the device object before
//   this call has returned.
//
// Efficiency: LOW
//   This API will dynamically link to backend DLLs/SOs like dsound.dll, and is otherwise just slow
//   due to the nature of it being an initialization API.
mal_result mal_device_init(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount, mal_log_proc onLog);

// Uninitializes a device.
//
// This will explicitly stop the device. You do not need to call mal_device_stop() beforehand, but it's
// harmless if you do.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       pDevice is NULL.
//   - MAL_DEVICE_NOT_INITIALIZED
//       The device is not currently or was never initialized.
//
// Thread Safety: ???
//   This API shouldn't crash in a multi-threaded environment, but results are undefined if an application
//   attempts to do something with the device at the same time as uninitializing.
//
// Efficiency: LOW
//   This will stop the device with mal_device_stop() which is a slow, synchronized call. It also needs
//   to destroy internal objects like the backend-specific objects and the background thread.
void mal_device_uninit(mal_device* pDevice);

// Sets the callback to use when the application has received data from the device.
//
// The fragment size specified at initialization time controls the sample count.
//
// Thread Safety: SAFE
//   This API is implemented as a simple atomic assignment.
//
// Efficiency: HIGH
//   This is just an atomic assignment.
void mal_device_set_recv_callback(mal_device* pDevice, mal_recv_proc proc);

// Sets the callback to use when the application needs to send data to the device for playback.
//
// Note that the implementation of this callback must copy over as many samples as is available. The
// return value specifies how many samples were written to the output buffer. The backend will fill
// any leftover samples with silence. The fragment size specified at initialization time controls the
// number of samples the device will request.
//
// Thread Safety: SAFE
//   This API is implemented as a simple atomic assignment.
//
// Efficiency: HIGH
//   This is just an atomic assignment.
void mal_device_set_send_callback(mal_device* pDevice, mal_send_proc proc);

// Activates the device. For playback devices this begins playback. For recording devices it begins
// recording.
//
// For a playback device, this will retrieve an initial chunk of audio data from the client before
// returning. This reason for this is to ensure there is valid audio data in the buffer, which needs
// to be done _before_ the device starts playing back audio.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_DEVICE_NOT_INITIALIZED
//       The device is not currently or was never initialized.
//   - MAL_DEVICE_BUSY
//       The device is in the process of stopping. This will only happen if mal_device_start() and
//       mal_device_stop() is called simultaneous on separate threads. This will never be returned in
//       single-threaded applications.
//   - MAL_DEVICE_ALREADY_STARTING
//       The device is already in the process of starting. This will never be returned in single-threaded
//       applications.
//   - MAL_DEVICE_ALREADY_STARTED
//       The device is already started.
//   - MAL_FAILED_TO_READ_DATA_FROM_CLIENT
//       Failed to read the initial chunk of audio data from the client. This initial chunk of data is
//       required so that the device has valid audio data as soon as it starts playing. This will never
//       be returned for capture devices.
//   - MAL_FAILED_TO_START_BACKEND_DEVICE
//       There was a backend-specific error starting the device.
//
// Thread Safety: SAFE
//
// Efficiency: LOW
//   This API waits until the backend device has been closed for real by the worker thread.
mal_result mal_device_start(mal_device* pDevice);

// Puts the device to sleep, but does not uninitialize it. Use mal_device_start() to start it up again.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_DEVICE_NOT_INITIALIZED
//       The device is not currently or was never initialized.
//   - MAL_DEVICE_BUSY
//       The device is in the process of starting. This will only happen if mal_device_start() and
//       mal_device_stop() is called simultaneous on separate threads. This will never be returned in
//       single-threaded applications.
//   - MAL_DEVICE_ALREADY_STOPPING
//       The device is already in the process of stopping. This will never be returned in single-threaded
//       applications.
//   - MAL_DEVICE_ALREADY_STOPPED
//       The device is already stopped.
//   - MAL_FAILED_TO_STOP_BACKEND_DEVICE
//       There was a backend-specific error stopping the device.
//
// Thread Safety: SAFE
//
// Efficiency: LOW
//   This API needs to wait on the worker thread to stop the backend device properly before returning.
mal_result mal_device_stop(mal_device* pDevice);

// Determines whether or not the device is started.
//
// Return Value:
//   True if the device is started, false otherwise.
//
// Thread Safety: SAFE
//   If another thread calls mal_device_start() or mal_device_stop() at this same time as this function
//   is called, there's a very small chance the return value will out of sync.
//
// Efficiency: HIGH
//   This is implemented with a simple accessor.
mal_bool32 mal_device_is_started(mal_device* pDevice);

// Retrieves the number of frames available for rewinding.
mal_uint32 mal_device_get_available_rewind_amount(mal_device* pDevice);

// Rewinds by a number of frames.
//
// The return value is the number of frames rewound.
mal_uint32 mal_device_rewind(mal_device* pDevice, mal_uint32 frames);

// Retrieves the size of a fragment in bytes for the given device.
//
// Thread Safety: SAFE
//   This is calculated from constant values which are set at initialization time and never change.
//
// Efficiency: HIGH
//   This is implemented with just a few 32-bit integer multiplications.
mal_uint32 mal_device_get_fragment_size_in_bytes(mal_device* pDevice);

// Retrieves the size of a sample in bytes for the given format.
//
// Thread Safety: SAFE
//   This is API is pure.
//
// Efficiency: HIGH
//   This is implemented with a lookup table.
mal_uint32 mal_get_sample_size_in_bytes(mal_format format);


#ifdef __cplusplus
}
#endif
#endif  //mini_al_h


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_IMPLEMENTATION
#ifdef MAL_WIN32
#include <windows.h>
#else
#include <stdlib.h> // For malloc()/free()
#include <string.h>	// For memset()
#endif

#ifdef MAL_POSIX
#include <unistd.h>
#endif

#if !defined(NDEBUG)
#include <assert.h>
#endif

#ifdef MAL_ENABLE_ALSA
#include <stdio.h>  // Needed for printf() which is used for "hw:%d,%d" formatting. TODO: Remove this later.
#endif

#if !defined(MAL_64BIT) && !defined(MAL_32BIT)
#ifdef _WIN32
#ifdef _WIN64
#define MAL_64BIT
#else
#define MAL_32BIT
#endif
#endif
#endif

#if !defined(MAL_64BIT) && !defined(MAL_32BIT)
#ifdef __GNUC__
#ifdef __LP64__
#define MAL_64BIT
#else
#define MAL_32BIT
#endif
#endif
#endif

#if !defined(MAL_64BIT) && !defined(MAL_32BIT)
#include <stdint.h>
#if INTPTR_MAX == INT64_MAX
#define MAL_64BIT
#else
#define MAL_32BIT
#endif
#endif



#ifdef MAL_WIN32
	#define MAL_THREADCALL WINAPI
	typedef unsigned long mal_thread_result;
#else
	#define MAL_THREADCALL
	typedef void* mal_thread_result;
#endif
typedef mal_thread_result (MAL_THREADCALL * mal_thread_entry_proc)(void* pData);

#define MAL_STATE_UNINITIALIZED     0
#define MAL_STATE_STOPPED           1   // The device's default state after initialization.
#define MAL_STATE_STARTED           2   // The worker thread is in it's main loop waiting for the driver to request or deliver audio data.
#define MAL_STATE_STARTING          3   // Transitioning from a stopped state to started.
#define MAL_STATE_STOPPING          4   // Transitioning from a started state to stopped.

///////////////////////////////////////////////////////////////////////////////
//
// Standard Library Stuff
//
///////////////////////////////////////////////////////////////////////////////
#ifndef mal_zero_memory
#ifdef MAL_WIN32
#define mal_zero_memory(p, sz) ZeroMemory((p), (sz))
#else
#define mal_zero_memory(p, sz) memset((p), 0, (sz))
#endif
#endif

#define mal_zero_object(p) mal_zero_memory((p), sizeof(*(p)))

#ifndef mal_copy_memory
#ifdef MAL_WIN32
#define mal_copy_memory(dst, src, sz) CopyMemory((dst), (src), (sz))
#else
#define mal_copy_memory(dst, src, sz) memcpy((dst), (src), (sz))
#endif
#endif

#ifndef mal_malloc
#ifdef MAL_WIN32
#define mal_malloc(sz) HeapAlloc(GetProcessHeap(), 0, (sz))
#else
#define mal_malloc(sz) malloc((sz))
#endif
#endif

#ifndef mal_free
#ifdef MAL_WIN32
#define mal_free(p) HeapFree(GetProcessHeap(), 0, (p))
#else
#define mal_free(p) free((p))
#endif
#endif

#ifndef mal_assert
#ifdef MAL_WIN32
#define mal_assert(condition) assert(condition)
#else
#define mal_assert(condition) assert(condition)
#endif
#endif

// Return Values:
//   0:  Success
//   22: EINVAL
//   34: ERANGE
//
// Not using symbolic constants for errors because I want to avoid #including errno.h
static int mal_strncpy_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    if (dst == 0) {
        return 22;
    }
    if (dstSizeInBytes == 0) {
        return 22;
    }
    if (src == 0) {
        dst[0] = '\0';
        return 22;
    }

    size_t maxcount = count;
    if (count == ((size_t)-1) || count >= dstSizeInBytes) {        // -1 = _TRUNCATE
        maxcount = dstSizeInBytes - 1;
    }

    size_t i;
    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return 34;
}

// Thanks to good old Bit Twiddling Hacks for this one: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static unsigned int mal_next_power_of_2(unsigned int x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;

    return x;
}

static unsigned int mal_prev_power_of_2(unsigned int x)
{
    return mal_next_power_of_2(x) >> 1;
}


///////////////////////////////////////////////////////////////////////////////
//
// Atomics
//
///////////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) && defined(_MSC_VER)
#define mal_memory_barrier()            MemoryBarrier()
#define mal_atomic_exchange_32(a, b)    InterlockedExchange((LONG*)a, (LONG)b)
#define mal_atomic_exchange_64(a, b)    InterlockedExchange64((LONGLONG*)a, (LONGLONG)b)
#else
#define mal_memory_barrier()            __sync_synchronize()
#define mal_atomic_exchange_32(a, b)    (void)__sync_lock_test_and_set(a, b); __sync_synchronize()
#define mal_atomic_exchange_64(a, b)    (void)__sync_lock_test_and_set(a, b); __sync_synchronize()
#endif

#ifdef MAL_64BIT
#define mal_atomic_exchange_ptr mal_atomic_exchange_64
#endif
#ifdef MAL_32BIT
#define mal_atomic_exchange_ptr mal_atomic_exchange_32
#endif


///////////////////////////////////////////////////////////////////////////////
//
// Timing
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_WIN32
static LARGE_INTEGER g_mal_TimerFrequency = {{0}};
void mal_timer_init(mal_timer* pTimer)
{
    if (g_mal_TimerFrequency.QuadPart == 0) {
        QueryPerformanceFrequency(&g_mal_TimerFrequency);
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    pTimer->counter = (uint64_t)counter.QuadPart;
}

double mal_timer_get_time_in_seconds(mal_timer* pTimer)
{
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter)) {
        return 0;
    }

    return (counter.QuadPart - pTimer->counter) / (double)g_mal_TimerFrequency.QuadPart;
}
#else
void mal_timer_init(mal_timer* pTimer)
{
    struct timespec newTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &newTime);

    pTimer->counter = (newTime.tv_sec * 1000000000LL) + newTime.tv_nsec;
}

double mal_timer_get_time_in_seconds(mal_timer* pTimer)
{
    struct timespec newTime;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &newTime);

    uint64_t newTimeCounter = (newTime.tv_sec * 1000000000LL) + newTime.tv_nsec;
    uint64_t oldTimeCounter = pTimer->counter;

    return (newTimeCounter - oldTimeCounter) / 1000000000.0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// Threading
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_WIN32
mal_bool32 mal_thread_create__win32(mal_thread* pThread, mal_thread_entry_proc entryProc, void* pData)
{
    *pThread = CreateThread(NULL, 0, entryProc, pData, 0, NULL);
    if (*pThread == NULL) {
        return MAL_FALSE;
    }

    return MAL_TRUE;
}

void mal_thread_wait__win32(mal_thread* pThread)
{
    WaitForSingleObject(*pThread, INFINITE);
}

void mal_sleep__win32(mal_uint32 milliseconds)
{
    Sleep((DWORD)milliseconds);
}


mal_bool32 mal_event_create__win32(mal_event* pEvent)
{
    *pEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (*pEvent == NULL) {
        return MAL_FALSE;
    }

    return MAL_TRUE;
}

void mal_event_delete__win32(mal_event* pEvent)
{
    CloseHandle(*pEvent);
}

mal_bool32 mal_event_wait__win32(mal_event* pEvent)
{
    return WaitForSingleObject(*pEvent, INFINITE) == WAIT_OBJECT_0;
}

mal_bool32 mal_event_signal__win32(mal_event* pEvent)
{
    return SetEvent(*pEvent);
}
#endif


#ifdef MAL_POSIX
mal_bool32 mal_thread_create__posix(mal_thread* pThread, mal_thread_entry_proc entryProc, void* pData)
{
    return pthread_create(pThread, NULL, entryProc, pData) == 0;
}

void mal_thread_wait__posix(mal_thread* pThread)
{
    pthread_join(*pThread, NULL);
}

void mal_sleep__posix(mal_uint32 milliseconds)
{
    usleep(milliseconds * 1000);    // <-- usleep is in microseconds.
}


mal_bool32 mal_event_create__posix(mal_event* pEvent)
{
    if (pthread_mutex_init(&pEvent->mutex, NULL) != 0) {
        return MAL_FALSE;
    }

    if (pthread_cond_init(&pEvent->condition, NULL) != 0) {
        return MAL_FALSE;
    }

    pEvent->value = 0;
    return MAL_TRUE;
}

void mal_event_delete__posix(mal_event* pEvent)
{
    pthread_cond_destroy(&pEvent->condition);
    pthread_mutex_destroy(&pEvent->mutex);
}

mal_bool32 mal_event_wait__posix(mal_event* pEvent)
{
    pthread_mutex_lock(&pEvent->mutex);
    {
        while (pEvent->value == 0) {
            pthread_cond_wait(&pEvent->condition, &pEvent->mutex);
        }

        pEvent->value = 0;  // Auto-reset.
    }
    pthread_mutex_unlock(&pEvent->mutex);

    return MAL_TRUE;
}

mal_bool32 mal_event_signal__posix(mal_event* pEvent)
{
    pthread_mutex_lock(&pEvent->mutex);
    {
        pEvent->value = 1;
        pthread_cond_signal(&pEvent->condition);
    }
    pthread_mutex_unlock(&pEvent->mutex);

    return MAL_TRUE;
}
#endif

mal_bool32 mal_thread_create(mal_thread* pThread, mal_thread_entry_proc entryProc, void* pData)
{
    if (pThread == NULL || entryProc == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_thread_create__win32(pThread, entryProc, pData);
#endif
#ifdef MAL_POSIX
    return mal_thread_create__posix(pThread, entryProc, pData);
#endif
}

void mal_thread_wait(mal_thread* pThread)
{
    if (pThread == NULL) return;

#ifdef MAL_WIN32
    mal_thread_wait__win32(pThread);
#endif
#ifdef MAL_POSIX
    mal_thread_wait__posix(pThread);
#endif
}

void mal_sleep(mal_uint32 milliseconds)
{
#ifdef MAL_WIN32
    mal_sleep__win32(milliseconds);
#endif
#ifdef MAL_POSIX
    mal_sleep__posix(milliseconds);
#endif
}


mal_bool32 mal_event_create(mal_event* pEvent)
{
    if (pEvent == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_event_create__win32(pEvent);
#endif
#ifdef MAL_POSIX
    return mal_event_create__posix(pEvent);
#endif
}

void mal_event_delete(mal_event* pEvent)
{
    if (pEvent == NULL) return;

#ifdef MAL_WIN32
    mal_event_delete__win32(pEvent);
#endif
#ifdef MAL_POSIX
    mal_event_delete__posix(pEvent);
#endif
}

mal_bool32 mal_event_wait(mal_event* pEvent)
{
    if (pEvent == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_event_wait__win32(pEvent);
#endif
#ifdef MAL_POSIX
    return mal_event_wait__posix(pEvent);
#endif
}

mal_bool32 mal_event_signal(mal_event* pEvent)
{
    if (pEvent == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_event_signal__win32(pEvent);
#endif
#ifdef MAL_POSIX
    return mal_event_signal__posix(pEvent);
#endif
}


// Posts a log message.
static void mal_log(mal_device* pDevice, const char* message)
{
    if (pDevice && pDevice->onLog) {
        pDevice->onLog(pDevice, message);
    }
}

// Posts an error. Throw a breakpoint in here if you're needing to debug. The return value is always "resultCode".
static mal_result mal_post_error(mal_device* pDevice, const char* message, mal_result resultCode)
{
    mal_log(pDevice, message);
    return resultCode;
}



// A helper function for reading sample data from the client. Returns the number of samples read from the client. Remaining samples
// are filled with silence.
static inline mal_uint32 mal_device__read_samples_from_client(mal_device* pDevice, mal_uint32 sampleCount, void* pSamples)
{
    mal_assert(pDevice != NULL);
    mal_assert(sampleCount > 0);
    mal_assert(pSamples != NULL);

    mal_uint32 samplesRead = 0;
    if (pDevice->onSend) {
        samplesRead = pDevice->onSend(pDevice, sampleCount, pSamples);
    }

    mal_uint32 sampleSize = mal_get_sample_size_in_bytes(pDevice->format);
	mal_uint32 consumedBytes = samplesRead*sampleSize;
	mal_uint32 remainingBytes = (sampleCount - samplesRead)*sampleSize;
	mal_zero_memory((mal_uint8*)pSamples + consumedBytes, remainingBytes);

    return samplesRead;
}

// A helper for sending sample data to the client.
static inline void mal_device__send_samples_to_client(mal_device* pDevice, mal_uint32 sampleCount, const void* pSamples)
{
    mal_assert(pDevice != NULL);
    mal_assert(sampleCount > 0);
    mal_assert(pSamples != NULL);

    if (pDevice->onRecv) {
        pDevice->onRecv(pDevice, sampleCount, pSamples);
    }
}

// A helper for changing the state of the device.
static inline void mal_device__set_state(mal_device* pDevice, mal_uint32 newState)
{
    mal_atomic_exchange_32(&pDevice->state, newState);
}

// A helper for getting the state of the device.
static inline mal_uint32 mal_device__get_state(mal_device* pDevice)
{
    return pDevice->state;
}


///////////////////////////////////////////////////////////////////////////////
//
// Null Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_NULL
static mal_result mal_enumerate_devices__null(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
	mal_uint32 infoSize = *pCount;
	*pCount = 1;	// There's only one "device" each for playback and recording for the null backend.
	
	if (pInfo != NULL && infoSize > 0) {
        mal_zero_object(pInfo);
		
		if (type == mal_device_type_playback) {
			mal_strncpy_s(pInfo->name, sizeof(pInfo->name), "NULL Playback Device", (size_t)-1);
		} else {
			mal_strncpy_s(pInfo->name, sizeof(pInfo->name), "NULL Capture Device", (size_t)-1);
		}
	}
	
	return MAL_SUCCESS;
}

static void mal_device_uninit__null(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
    mal_free(pDevice->null_device.pBuffer);
}

static mal_result mal_device_init__null(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount)
{
	mal_assert(pDevice != NULL);
	pDevice->api = mal_api_null;
    mal_timer_init(&pDevice->null_device.timer);

    pDevice->null_device.pBuffer = (mal_uint8*)mal_malloc(mal_device_get_fragment_size_in_bytes(pDevice) * pDevice->fragmentCount);
    if (pDevice->null_device.pBuffer == NULL) {
        return MAL_OUT_OF_MEMORY;
    }

    mal_zero_memory(pDevice->null_device.pBuffer, mal_device_get_fragment_size_in_bytes(pDevice) * pDevice->fragmentCount);

	return MAL_SUCCESS;
}

static mal_result mal_device__start_backend__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_timer_init(&pDevice->null_device.timer);
    pDevice->null_device.lastProcessedFrame = 0;

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    return MAL_SUCCESS;
}

static mal_result mal_device__break_main_loop__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    pDevice->null_device.breakFromMainLoop = MAL_TRUE;
    return MAL_SUCCESS;
}

static mal_bool32 mal_device__get_current_frame__null(mal_device* pDevice, mal_uint32* pCurrentPos)
{
    mal_assert(pDevice != NULL);
    mal_assert(pCurrentPos != NULL);
    *pCurrentPos = 0;

    mal_uint64 currentFrameAbs = (mal_uint64)(mal_timer_get_time_in_seconds(&pDevice->null_device.timer) * pDevice->sampleRate) / pDevice->channels;

    *pCurrentPos = currentFrameAbs % (pDevice->fragmentSizeInFrames * pDevice->fragmentCount);
    return MAL_TRUE;
}

static mal_bool32 mal_device__get_available_frames__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_uint32 currentFrame;
    if (!mal_device__get_current_frame__null(pDevice, &currentFrame)) {
        return 0;
    }

    // In a playback device the last processed frame should always be ahead of the current frame. The space between
    // the last processed and current frame (moving forward, starting from the last processed frame) is the amount
    // of space available to write.
    //
    // For a recording device it's the other way around - the last processed frame is always _behind_ the current
    // frame and the space between is the available space.
    mal_uint32 totalFrameCount = pDevice->fragmentSizeInFrames*pDevice->fragmentCount;
    if (pDevice->type == mal_device_type_playback) {
        mal_uint32 committedBeg = currentFrame;
        mal_uint32 committedEnd = pDevice->null_device.lastProcessedFrame;
        if (committedEnd <= committedBeg) {
            committedEnd += totalFrameCount;    // Wrap around.
        }

        mal_uint32 committedSize = (committedEnd - committedBeg);
        mal_assert(committedSize <= totalFrameCount);

        return totalFrameCount - committedSize;
    } else {
        mal_uint32 validBeg = pDevice->null_device.lastProcessedFrame;
        mal_uint32 validEnd = currentFrame;
        if (validEnd < validBeg) {
            validEnd += totalFrameCount;        // Wrap around.
        }

        mal_uint32 validSize = (validEnd - validBeg);
        mal_assert(validSize <= totalFrameCount);

        return validSize;
    }
}

static mal_uint32 mal_device__wait_for_frames__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    while (!pDevice->null_device.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__get_available_frames__null(pDevice);
        if (framesAvailable == 0) {
            return 0;
        }

        // Never return more frames that will fit in a fragment.
        if (framesAvailable >= pDevice->fragmentSizeInFrames) {
            return pDevice->fragmentSizeInFrames;
        }

        mal_sleep(16);
    }

    // We'll get here if the loop was terminated. Just return whatever's available.
    return mal_device__get_available_frames__null(pDevice);
}

static mal_result mal_device__main_loop__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    pDevice->null_device.breakFromMainLoop = MAL_FALSE;
    while (!pDevice->null_device.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__wait_for_frames__null(pDevice);
        if (framesAvailable == 0) {
            continue;
        }

        // If it's a playback device, don't bother grabbing more data if the device is being stopped.
        if (pDevice->null_device.breakFromMainLoop && pDevice->type == mal_device_type_playback) {
            return MAL_FALSE;
        }

        mal_uint32 sampleCount = framesAvailable * pDevice->channels;
        mal_uint32 lockOffset  = pDevice->null_device.lastProcessedFrame * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
        mal_uint32 lockSize    = sampleCount * mal_get_sample_size_in_bytes(pDevice->format);

        if (pDevice->type == mal_device_type_playback) {
            if (pDevice->null_device.breakFromMainLoop) {
                return MAL_FALSE;
            }

            mal_device__read_samples_from_client(pDevice, sampleCount, pDevice->null_device.pBuffer + lockOffset);
        } else {
            mal_zero_memory(pDevice->null_device.pBuffer + lockOffset, lockSize);
            mal_device__send_samples_to_client(pDevice, sampleCount, pDevice->null_device.pBuffer + lockOffset);
        }

        pDevice->null_device.lastProcessedFrame = (pDevice->null_device.lastProcessedFrame + (sampleCount/pDevice->channels)) % (pDevice->fragmentSizeInFrames*pDevice->fragmentCount);
    }

    return MAL_SUCCESS;
}

static mal_uint32 mal_device_get_available_rewind_amount__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Rewinding on the null device is unimportant. Not willing to add maintenance costs for this.
    (void)pDevice;
    return 0;
}

static mal_uint32 mal_device_rewind__null(mal_device* pDevice, mal_uint32 frames)
{
    mal_assert(pDevice != NULL);
    mal_assert(frames > 0);

    // Rewinding on the null device is unimportant. Not willing to add maintenance costs for this.
    (void)pDevice;
    (void)frames;
    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// DirectSound Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_DSOUND
#include <dsound.h>
#include <mmreg.h>  // WAVEFORMATEX

static GUID MAL_GUID_NULL                               = {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
static GUID _g_mal_GUID_IID_DirectSoundNotify           = {0xb0210783, 0x89cd, 0x11d0, {0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16}};
static GUID _g_mal_GUID_IID_IDirectSoundCaptureBuffer8  = {0x00990df4, 0x0dbb, 0x4872, {0x83, 0x3e, 0x6d, 0x30, 0x3e, 0x80, 0xae, 0xb6}};
static GUID _g_mal_GUID_KSDATAFORMAT_SUBTYPE_PCM        = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static GUID _g_mal_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static GUID _g_mal_GUID_KSDATAFORMAT_SUBTYPE_ALAW       = {0x00000006, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static GUID _g_mal_GUID_KSDATAFORMAT_SUBTYPE_MULAW      = {0x00000007, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
#ifdef __cplusplus
static GUID g_mal_GUID_IID_DirectSoundNotify            = _g_mal_GUID_IID_DirectSoundNotify;
static GUID g_mal_GUID_IID_IDirectSoundCaptureBuffer8   = _g_mal_GUID_IID_IDirectSoundCaptureBuffer8;
#else
static GUID* g_mal_GUID_IID_DirectSoundNotify           = &_g_mal_GUID_IID_DirectSoundNotify;
static GUID* g_mal_GUID_IID_IDirectSoundCaptureBuffer8  = &_g_mal_GUID_IID_IDirectSoundCaptureBuffer8;
#endif

typedef HRESULT (WINAPI * mal_DirectSoundCreate8Proc)(LPCGUID pcGuidDevice, LPDIRECTSOUND8 *ppDS8, LPUNKNOWN pUnkOuter);
typedef HRESULT (WINAPI * mal_DirectSoundEnumerateAProc)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);
typedef HRESULT (WINAPI * mal_DirectSoundCaptureCreate8Proc)(LPCGUID pcGuidDevice, LPDIRECTSOUNDCAPTURE8 *ppDSC8, LPUNKNOWN pUnkOuter);
typedef HRESULT (WINAPI * mal_DirectSoundCaptureEnumerateAProc)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);

static HMODULE mal_open_dsound_dll()
{
    return LoadLibraryW(L"dsound.dll");
}

static void mal_close_dsound_dll(HMODULE hModule)
{
    FreeLibrary(hModule);
}


typedef struct
{
    mal_uint32 deviceCount;
    mal_uint32 infoCount;
    mal_device_info* pInfo;
} mal_device_enum_data__dsound;

static BOOL CALLBACK mal_enum_devices_callback__dsound(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext)
{
    (void)lpcstrModule;

    mal_device_enum_data__dsound* pData = (mal_device_enum_data__dsound*)lpContext;
    assert(pData != NULL);

    if (pData->pInfo != NULL) {
        if (pData->infoCount > 0) {
            mal_zero_object(pData->pInfo);
            mal_strncpy_s(pData->pInfo->name, sizeof(pData->pInfo->name), lpcstrDescription, (size_t)-1);

            if (lpGuid != NULL) {
                mal_copy_memory(pData->pInfo->id.guid, lpGuid, 16);
            } else {
                mal_zero_memory(pData->pInfo->id.guid, 16);
            }
            
            pData->pInfo += 1;
            pData->infoCount -= 1;
            pData->deviceCount += 1;
        }
    } else {
        pData->deviceCount += 1;
    }

    return TRUE;
}

static mal_result mal_enumerate_devices__dsound(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
    mal_uint32 infoSize = *pCount;
	*pCount = 0;

    mal_device_enum_data__dsound enumData;
    enumData.deviceCount = 0;
    enumData.infoCount = infoSize;
    enumData.pInfo = pInfo;

    HMODULE dsoundDLL = mal_open_dsound_dll();
    if (dsoundDLL == NULL) {
        return MAL_NO_BACKEND;
    }

    if (type == mal_device_type_playback) {
        mal_DirectSoundEnumerateAProc pDirectSoundEnumerateA = (mal_DirectSoundEnumerateAProc)GetProcAddress(dsoundDLL, "DirectSoundEnumerateA");
        if (pDirectSoundEnumerateA) {
            pDirectSoundEnumerateA(mal_enum_devices_callback__dsound, &enumData);
        }
    } else {
        mal_DirectSoundCaptureEnumerateAProc pDirectSoundCaptureEnumerateA = (mal_DirectSoundCaptureEnumerateAProc)GetProcAddress(dsoundDLL, "DirectSoundCaptureEnumerateA");
        if (pDirectSoundCaptureEnumerateA) {
            pDirectSoundCaptureEnumerateA(mal_enum_devices_callback__dsound, &enumData);
        }
    }


    mal_close_dsound_dll(dsoundDLL);

    *pCount = enumData.deviceCount;
    return MAL_SUCCESS;
}

static void mal_device_uninit__dsound(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);

    if (pDevice->dsound.hDSoundDLL != NULL) {
        if (pDevice->dsound.hStopEvent) {
            CloseHandle(pDevice->dsound.hStopEvent);
        }

        for (mal_uint32 i = 0; i < pDevice->fragmentCount; ++i) {
            if (pDevice->dsound.pNotifyEvents[i]) {
                CloseHandle(pDevice->dsound.pNotifyEvents[i]);
            }
        }

        if (pDevice->dsound.pCaptureBuffer) {
            IDirectSoundCaptureBuffer8_Release((LPDIRECTSOUNDBUFFER8)pDevice->dsound.pCaptureBuffer);
        }
        if (pDevice->dsound.pCapture) {
            IDirectSoundCapture_Release((LPDIRECTSOUNDCAPTURE8)pDevice->dsound.pCaptureBuffer);
        }

        if (pDevice->dsound.pPlaybackBuffer) {
            IDirectSoundBuffer_Release((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer);
        }
        if (pDevice->dsound.pPlaybackPrimaryBuffer) {
            IDirectSoundBuffer_Release((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer);
        }
        if (pDevice->dsound.pPlayback != NULL) {
            IDirectSound_Release((LPDIRECTSOUND8)pDevice->dsound.pPlayback);
        }

        mal_close_dsound_dll((HMODULE)pDevice->dsound.hDSoundDLL);
    }
}

static mal_result mal_device_init__dsound(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount)
{
	mal_assert(pDevice != NULL);
	pDevice->api = mal_api_dsound;

    pDevice->dsound.hDSoundDLL = (mal_handle)mal_open_dsound_dll();
    if (pDevice->dsound.hDSoundDLL == NULL) {
        return MAL_NO_BACKEND;
    }

    // Check that we have a valid format.
    GUID subformat;
    switch (format)
    {
        case mal_format_u8:
        case mal_format_s16:
        case mal_format_s24:
        case mal_format_s32:
        {
            subformat = _g_mal_GUID_KSDATAFORMAT_SUBTYPE_PCM;
        } break;

        case mal_format_f32:
        case mal_format_f64:
        {
            subformat = _g_mal_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        } break;

        case mal_format_alaw:
        {
            subformat = _g_mal_GUID_KSDATAFORMAT_SUBTYPE_ALAW;
        } break;

        case mal_format_mulaw:
        {
            subformat = _g_mal_GUID_KSDATAFORMAT_SUBTYPE_MULAW;
        } break;

        default:
        return MAL_FORMAT_NOT_SUPPORTED;
    }

    if (fragmentCount > MAL_MAX_FRAGMENTS_DSOUND) {
        fragmentCount = MAL_MAX_FRAGMENTS_DSOUND;
    }

    WAVEFORMATEXTENSIBLE wf;
    mal_zero_object(&wf);
    wf.Format.cbSize               = sizeof(wf);
    wf.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels            = (WORD)channels;
    wf.Format.nSamplesPerSec       = (DWORD)sampleRate;
    wf.Format.wBitsPerSample       = mal_get_sample_size_in_bytes(format)*8;
    wf.Format.nBlockAlign          = (wf.Format.nChannels * wf.Format.wBitsPerSample) / 8;
    wf.Format.nAvgBytesPerSec      = wf.Format.nBlockAlign * wf.Format.nSamplesPerSec;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.dwChannelMask               = (channels <= 2) ? 0 : ~(((DWORD)-1) << channels);
    wf.SubFormat                   = subformat;

    DWORD fragmentSizeInBytes = mal_device_get_fragment_size_in_bytes(pDevice);
    
    // Unfortunately DirectSound uses different APIs and data structures for playback and catpure devices :(
    if (type == mal_device_type_playback) {
        mal_DirectSoundCreate8Proc pDirectSoundCreate8 = (mal_DirectSoundCreate8Proc)GetProcAddress((HMODULE)pDevice->dsound.hDSoundDLL, "DirectSoundCreate8");
        if (pDirectSoundCreate8 == NULL) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Could not find DirectSoundCreate8().", MAL_API_NOT_FOUND);
        }

        if (FAILED(pDirectSoundCreate8((pDeviceID == NULL) ? NULL : (LPCGUID)pDeviceID->guid, (LPDIRECTSOUND8*)&pDevice->dsound.pPlayback, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] DirectSoundCreate8() failed for playback device.", MAL_DSOUND_FAILED_TO_CREATE_DEVICE);
        }

        // The cooperative level must be set before doing anything else.
        if (FAILED(IDirectSound_SetCooperativeLevel((LPDIRECTSOUND8)pDevice->dsound.pPlayback, GetForegroundWindow(), DSSCL_PRIORITY))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSound_SetCooperateiveLevel() failed for playback device.", MAL_DSOUND_FAILED_TO_SET_COOP_LEVEL);
        }

        DSBUFFERDESC descDSPrimary;
        mal_zero_object(&descDSPrimary);
        descDSPrimary.dwSize  = sizeof(DSBUFFERDESC);
        descDSPrimary.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
        if (FAILED(IDirectSound_CreateSoundBuffer((LPDIRECTSOUND8)pDevice->dsound.pPlayback, &descDSPrimary, (LPDIRECTSOUNDBUFFER*)&pDevice->dsound.pPlaybackPrimaryBuffer, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSound_CreateSoundBuffer() failed for playback device's primary buffer.", MAL_DSOUND_FAILED_TO_CREATE_BUFFER);
        }

        // From MSDN:
        //
        // The method succeeds even if the hardware does not support the requested format; DirectSound sets the buffer to the closest
        // supported format. To determine whether this has happened, an application can call the GetFormat method for the primary buffer
        // and compare the result with the format that was requested with the SetFormat method.
        if (FAILED(IDirectSoundBuffer_SetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, (WAVEFORMATEX*)&wf))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Failed to set format of playback device's primary buffer.", MAL_FORMAT_NOT_SUPPORTED);
        }

        // Get the _actual_ properties of the buffer. This is silly API design...
        DWORD requiredSize;
        if (FAILED(IDirectSoundBuffer_GetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, NULL, 0, &requiredSize))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Failed to retrieve the actual format of the playback device's primary buffer.", MAL_FORMAT_NOT_SUPPORTED);
        }

        char rawdata[1024];
        WAVEFORMATEXTENSIBLE* pActualFormat = (WAVEFORMATEXTENSIBLE*)rawdata;
        if (FAILED(IDirectSoundBuffer_GetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, (WAVEFORMATEX*)pActualFormat, requiredSize, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Failed to retrieve the actual format of the playback device's primary buffer.", MAL_FORMAT_NOT_SUPPORTED);
        }

        pDevice->channels = pActualFormat->Format.nChannels;
        pDevice->sampleRate = pActualFormat->Format.nSamplesPerSec;
        pDevice->fragmentCount = fragmentCount;
        pDevice->fragmentSizeInFrames = mal_next_power_of_2(fragmentSizeInFrames);  // Keeping the fragment size a multiple of 2 just for consistency with ALSA.


        // Meaning of dwFlags (from MSDN):
        //
        // DSBCAPS_CTRLPOSITIONNOTIFY
        //   The buffer has position notification capability.
        //
        // DSBCAPS_GLOBALFOCUS
        //   With this flag set, an application using DirectSound can continue to play its buffers if the user switches focus to
        //   another application, even if the new application uses DirectSound.
        //
        // DSBCAPS_GETCURRENTPOSITION2
        //   In the first version of DirectSound, the play cursor was significantly ahead of the actual playing sound on emulated
        //   sound cards; it was directly behind the write cursor. Now, if the DSBCAPS_GETCURRENTPOSITION2 flag is specified, the
        //   application can get a more accurate play cursor.
        DSBUFFERDESC descDS;
        memset(&descDS, 0, sizeof(DSBUFFERDESC));
        descDS.dwSize = sizeof(DSBUFFERDESC);
        descDS.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        descDS.dwBufferBytes = fragmentSizeInBytes * pDevice->fragmentCount;
        descDS.lpwfxFormat = (WAVEFORMATEX*)&wf;
        if (FAILED(IDirectSound_CreateSoundBuffer((LPDIRECTSOUND8)pDevice->dsound.pPlayback, &descDS, (LPDIRECTSOUNDBUFFER*)&pDevice->dsound.pPlaybackBuffer, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSound_CreateSoundBuffer() failed for playback device's secondary buffer.", MAL_DSOUND_FAILED_TO_CREATE_BUFFER);
        }

        
        // Notifications are set up via a DIRECTSOUNDNOTIFY object which is retrieved from the buffer.
        if (FAILED(IDirectSoundBuffer8_QueryInterface((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, g_mal_GUID_IID_DirectSoundNotify, (void**)&pDevice->dsound.pNotify))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer8_QueryInterface() failed for playback device's IDirectSoundNotify object.", MAL_DSOUND_FAILED_TO_QUERY_INTERFACE);
        }
    } else {
        mal_DirectSoundCaptureCreate8Proc pDirectSoundCaptureCreate8 = (mal_DirectSoundCaptureCreate8Proc)GetProcAddress((HMODULE)pDevice->dsound.hDSoundDLL, "DirectSoundCaptureCreate8");
        if (pDirectSoundCaptureCreate8 == NULL) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Could not find DirectSoundCreate8().", MAL_API_NOT_FOUND);
        }

        if (FAILED(pDirectSoundCaptureCreate8((pDeviceID == NULL) ? NULL : (LPCGUID)pDeviceID->guid, (LPDIRECTSOUNDCAPTURE8*)&pDevice->dsound.pCapture, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] DirectSoundCaptureCreate8() failed for capture device.", MAL_DSOUND_FAILED_TO_CREATE_DEVICE);
        }

        DSCBUFFERDESC descDS;
        mal_zero_object(&descDS);
        descDS.dwSize = sizeof(descDS);
        descDS.dwFlags = 0;
        descDS.dwBufferBytes = fragmentSizeInBytes * pDevice->fragmentCount;
        descDS.lpwfxFormat = (WAVEFORMATEX*)&wf;
        LPDIRECTSOUNDCAPTUREBUFFER pDSCB_Temp;
        if (FAILED(IDirectSoundCapture_CreateCaptureBuffer((LPDIRECTSOUNDCAPTURE8)pDevice->dsound.pCapture, &descDS, &pDSCB_Temp, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCapture_CreateCaptureBuffer() failed for capture device.", MAL_DSOUND_FAILED_TO_CREATE_BUFFER);
        }

        HRESULT hr = IDirectSoundCapture_QueryInterface(pDSCB_Temp, g_mal_GUID_IID_IDirectSoundCaptureBuffer8, (LPVOID*)&pDevice->dsound.pCaptureBuffer);
        IDirectSoundCaptureBuffer_Release(pDSCB_Temp);
        if (FAILED(hr)) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCapture_QueryInterface() failed for capture device's IDirectSoundCaptureBuffer8 object.", MAL_DSOUND_FAILED_TO_QUERY_INTERFACE);
        }

        // Notifications are set up via a DIRECTSOUNDNOTIFY object which is retrieved from the buffer.
        if (FAILED(IDirectSoundCaptureBuffer8_QueryInterface((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, g_mal_GUID_IID_DirectSoundNotify, (void**)&pDevice->dsound.pNotify))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer8_QueryInterface() failed for capture device's IDirectSoundNotify object.", MAL_DSOUND_FAILED_TO_QUERY_INTERFACE);
        }
    }


    // We need a notification for each fragment. The notification offset is slightly different depending on whether or not the
    // device is a playback or capture device. For a playback device we want to be notified when a fragment just starts playing,
    // whereas for a capture device we want to be notified when a fragment has just _finished_ capturing.
    DSBPOSITIONNOTIFY notifyPoints[MAL_MAX_FRAGMENTS_DSOUND];  // One notification event for each fragment.
    for (mal_uint32 i = 0; i < pDevice->fragmentCount; ++i) {
        pDevice->dsound.pNotifyEvents[i] = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (pDevice->dsound.pNotifyEvents[i] == NULL) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Failed to create event for buffer notifications.", MAL_FAILED_TO_CREATE_EVENT);
        }

        // The notification offset is in bytes.
        DWORD dwOffset;
        if (type == mal_device_type_playback) {
            dwOffset = i * fragmentSizeInBytes;
        } else {
            dwOffset = ((i+1)*fragmentSizeInBytes) % (fragmentSizeInBytes*pDevice->fragmentCount);
        }

        notifyPoints[i].dwOffset = dwOffset;
        notifyPoints[i].hEventNotify = pDevice->dsound.pNotifyEvents[i];
    }

    if (FAILED(IDirectSoundNotify_SetNotificationPositions((LPDIRECTSOUNDNOTIFY)pDevice->dsound.pNotify, pDevice->fragmentCount, notifyPoints))) {
        mal_device_uninit__dsound(pDevice);
        return mal_post_error(pDevice, "[DirectSound] IDirectSoundNotify_SetNotificationPositions() failed.", MAL_DSOUND_FAILED_TO_SET_NOTIFICATIONS);
    }



    // When the device is playing the worker thread will be waiting on a bunch of notification events. To return from
    // this wait state we need to signal a special event.
    pDevice->dsound.hStopEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (pDevice->dsound.hStopEvent == NULL) {
        mal_device_uninit__dsound(pDevice);
		return mal_post_error(pDevice, "[DirectSound] Failed to create event for main loop break notification.", MAL_FAILED_TO_CREATE_EVENT);
    }


	return MAL_SUCCESS;
}


static mal_result mal_device__read_fragment_from_client__dsound(mal_device* pDevice, mal_uint32 fragmentIndex)
{
    mal_assert(pDevice != NULL);

    DWORD fragmentSizeInBytes = pDevice->fragmentSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
    DWORD offset = fragmentIndex * fragmentSizeInBytes;

    void* pLockPtr;
    DWORD lockSize;
    if (FAILED(IDirectSoundBuffer_Lock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, offset, fragmentSizeInBytes, &pLockPtr, &lockSize, NULL, NULL, 0))) {
        return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer_Lock() failed.", MAL_FAILED_TO_MAP_DEVICE_BUFFER);
    }

    mal_device__read_samples_from_client(pDevice, pDevice->fragmentSizeInFrames * pDevice->channels, pLockPtr);

    IDirectSoundBuffer_Unlock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, pLockPtr, lockSize, NULL, 0);
    return MAL_SUCCESS;
}

static mal_result mal_device__send_fragment_to_client__dsound(mal_device* pDevice, mal_uint32 fragmentIndex)
{
    mal_assert(pDevice != NULL);

    DWORD fragmentSizeInBytes = pDevice->fragmentSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
    DWORD offset = fragmentIndex * fragmentSizeInBytes;

    void* pLockPtr;
    DWORD lockSize;
    if (FAILED(IDirectSoundCaptureBuffer_Lock((LPDIRECTSOUNDCAPTUREBUFFER8)pDevice->dsound.pCaptureBuffer, offset, fragmentSizeInBytes, &pLockPtr, &lockSize, NULL, NULL, 0))) {
        return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer_Lock() failed.", MAL_FAILED_TO_MAP_DEVICE_BUFFER);
    }

    mal_device__send_samples_to_client(pDevice, pDevice->fragmentSizeInFrames * pDevice->channels, pLockPtr);

    IDirectSoundCaptureBuffer_Unlock((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, pLockPtr, lockSize, NULL, 0);
    return MAL_SUCCESS;
}


static mal_result mal_device__start_backend__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    
    if (pDevice->type == mal_device_type_playback) {
        // Before playing anything we need to grab an initial fragment of sample data from the client.
        mal_result result = mal_device__read_fragment_from_client__dsound(pDevice, 0);
        if (result != MAL_SUCCESS) {
            return result;  // The error will have already been posted.
        }

        pDevice->dsound.lastProcessedFrame = pDevice->fragmentSizeInFrames;

        if (IDirectSoundBuffer_Play((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, 0, 0, DSBPLAY_LOOPING) != DS_OK) {
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer_Play() failed.", MAL_FAILED_TO_START_BACKEND_DEVICE);
        }
    } else {
        if (IDirectSoundCaptureBuffer8_Start((LPDIRECTSOUNDCAPTUREBUFFER8)pDevice->dsound.pCaptureBuffer, DSCBSTART_LOOPING) != DS_OK) {
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer8_Start() failed.", MAL_FAILED_TO_START_BACKEND_DEVICE);
        }
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    
    if (pDevice->type == mal_device_type_playback) {
        if (IDirectSoundBuffer_Stop((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer) != DS_OK) {
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer_Stop() failed.", MAL_FAILED_TO_STOP_BACKEND_DEVICE);
        }

        IDirectSoundBuffer_SetCurrentPosition((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, 0);
    } else {
        if (IDirectSoundCaptureBuffer_Stop((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer) != DS_OK) {
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer_Stop() failed.", MAL_FAILED_TO_STOP_BACKEND_DEVICE);
        }
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__break_main_loop__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // The main loop will be waiting on a bunch of events via the WaitForMultipleObjects() API. One of those events
    // is a special event we use for forcing that function to return.
    pDevice->dsound.breakFromMainLoop = MAL_TRUE;
    SetEvent(pDevice->dsound.hStopEvent);
    return MAL_SUCCESS;
}

static mal_bool32 mal_device__get_current_frame__dsound(mal_device* pDevice, mal_uint32* pCurrentPos)
{
    mal_assert(pDevice != NULL);
    mal_assert(pCurrentPos != NULL);
    *pCurrentPos = 0;

    DWORD dwCurrentPosition;
    if (pDevice->type == mal_device_type_playback) {
        if (IDirectSoundBuffer_GetCurrentPosition((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, NULL, &dwCurrentPosition) != DS_OK) {
            return MAL_FALSE;
        }
    } else {
        if (IDirectSoundCaptureBuffer8_GetCurrentPosition((LPDIRECTSOUNDCAPTUREBUFFER8)pDevice->dsound.pCaptureBuffer, &dwCurrentPosition, NULL) != DS_OK) {
            return MAL_FALSE;
        }
    }

    *pCurrentPos = (mal_uint32)dwCurrentPosition / mal_get_sample_size_in_bytes(pDevice->format) / pDevice->channels;
    return MAL_TRUE;
}

static mal_bool32 mal_device__get_available_frames__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_uint32 currentFrame;
    if (!mal_device__get_current_frame__dsound(pDevice, &currentFrame)) {
        return 0;
    }

    // In a playback device the last processed frame should always be ahead of the current frame. The space between
    // the last processed and current frame (moving forward, starting from the last processed frame) is the amount
    // of space available to write.
    //
    // For a recording device it's the other way around - the last processed frame is always _behind_ the current
    // frame and the space between is the available space.
    mal_uint32 totalFrameCount = pDevice->fragmentSizeInFrames*pDevice->fragmentCount;
    if (pDevice->type == mal_device_type_playback) {
        mal_uint32 committedBeg = currentFrame;
        mal_uint32 committedEnd = pDevice->dsound.lastProcessedFrame;
        if (committedEnd <= committedBeg) {
            committedEnd += totalFrameCount;    // Wrap around.
        }

        mal_uint32 committedSize = (committedEnd - committedBeg);
        mal_assert(committedSize <= totalFrameCount);

        return totalFrameCount - committedSize;
    } else {
        mal_uint32 validBeg = pDevice->dsound.lastProcessedFrame;
        mal_uint32 validEnd = currentFrame;
        if (validEnd < validBeg) {
            validEnd += totalFrameCount;        // Wrap around.
        }

        mal_uint32 validSize = (validEnd - validBeg);
        mal_assert(validSize <= totalFrameCount);

        return validSize;
    }
}

static mal_uint32 mal_device__wait_for_frames__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    unsigned int eventCount = pDevice->fragmentCount + 1;
    HANDLE eventHandles[MAL_MAX_FRAGMENTS_DSOUND + 1];   // +1 for the stop event.
    mal_copy_memory(eventHandles, pDevice->dsound.pNotifyEvents, sizeof(HANDLE) * pDevice->fragmentCount);
    eventHandles[eventCount-1] = pDevice->dsound.hStopEvent;

    while (!pDevice->dsound.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__get_available_frames__dsound(pDevice);
        if (framesAvailable == 0) {
            return 0;
        }

        // Never return more frames that will fit in a fragment.
        if (framesAvailable >= pDevice->fragmentSizeInFrames) {
            return pDevice->fragmentSizeInFrames;
        }

        // If we get here it means we weren't able to find a full fragment. We'll just wait here for a bit.
        const DWORD timeoutInMilliseconds = 5;  // <-- This affects latency with the DirectSound backend. Should this be a property?
        DWORD rc = WaitForMultipleObjects(eventCount, eventHandles, FALSE, timeoutInMilliseconds);
        if (rc < WAIT_OBJECT_0 || rc >= WAIT_OBJECT_0 + eventCount) {
            return 0;
        }
    }

    // We'll get here if the loop was terminated. Just return whatever's available.
    return mal_device__get_available_frames__dsound(pDevice);
}

static mal_result mal_device__main_loop__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Make sure the stop event is not signaled to ensure we don't end up immediately returning from WaitForMultipleObjects().
    ResetEvent(pDevice->dsound.hStopEvent);

    pDevice->dsound.breakFromMainLoop = MAL_FALSE;
    while (!pDevice->dsound.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__wait_for_frames__dsound(pDevice);
        if (framesAvailable == 0) {
            continue;
        }

        // If it's a playback device, don't bother grabbing more data if the device is being stopped.
        if (pDevice->dsound.breakFromMainLoop && pDevice->type == mal_device_type_playback) {
            return MAL_FALSE;
        }

        DWORD lockOffset = pDevice->dsound.lastProcessedFrame * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
        DWORD lockSize   = framesAvailable * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);

        if (pDevice->type == mal_device_type_playback) {
            if (pDevice->dsound.breakFromMainLoop) {
                return MAL_FALSE;
            }

            void* pLockPtr;
            DWORD actualLockSize;
            if (FAILED(IDirectSoundBuffer_Lock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, lockOffset, lockSize, &pLockPtr, &actualLockSize, NULL, NULL, 0))) {
                return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer_Lock() failed.", MAL_FAILED_TO_MAP_DEVICE_BUFFER);
            }

            mal_uint32 sampleCount = actualLockSize / mal_get_sample_size_in_bytes(pDevice->format);
            mal_device__read_samples_from_client(pDevice, sampleCount, pLockPtr);
            pDevice->dsound.lastProcessedFrame = (pDevice->dsound.lastProcessedFrame + (sampleCount/pDevice->channels)) % (pDevice->fragmentSizeInFrames*pDevice->fragmentCount);

            IDirectSoundBuffer_Unlock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, pLockPtr, actualLockSize, NULL, 0);
        } else {
            void* pLockPtr;
            DWORD actualLockSize;
            if (FAILED(IDirectSoundCaptureBuffer_Lock((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, lockOffset, lockSize, &pLockPtr, &actualLockSize, NULL, NULL, 0))) {
                return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer_Lock() failed.", MAL_FAILED_TO_MAP_DEVICE_BUFFER);
            }

            mal_uint32 sampleCount = actualLockSize / mal_get_sample_size_in_bytes(pDevice->format);
            mal_device__send_samples_to_client(pDevice, sampleCount, pLockPtr);
            pDevice->dsound.lastProcessedFrame = (pDevice->dsound.lastProcessedFrame + (sampleCount/pDevice->channels)) % (pDevice->fragmentSizeInFrames*pDevice->fragmentCount);

            IDirectSoundCaptureBuffer_Unlock((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, pLockPtr, actualLockSize, NULL, 0);
        }
    }

    return MAL_SUCCESS;
}

static mal_uint32 mal_device_get_available_rewind_amount__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    return 0;
}

static mal_uint32 mal_device_rewind__dsound(mal_device* pDevice, mal_uint32 frames)
{
    mal_assert(pDevice != NULL);
    mal_assert(frames > 0);
    return 0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// ALSA Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_ALSA
#include <alsa/asoundlib.h>

static const char* mal_find_char(const char* str, char c, int* index)
{
    int i = 0;
    for (;;) {
        if (str[i] == '\0') {
            if (index) *index = -1;
            return NULL;
        }
    
        if (str[i] == c) {
            if (index) *index = i;
            return str + i;
        }
        
        i += 1;
    }
    
    // Should never get here, but treat it as though the character was not found to make me feel
    // better inside.
    if (index) *index = -1;
    return NULL;
}

// Waits for a number of frames to become available for either capture or playback. The return
// value is the number of frames available. If this is less than the fragment size it means the
// main loop has been terminated from another thread. The return value will be clamped to the
// fragment size if the main loop is still running, but could be larger if it returns due to the
// main loop being terminated.
//
// This will return early if the main loop is broken with mal_device__break_main_loop(), in
// which case it is possible for the returned number of frames will be greater than the size of
// a fragment (but smaller than the total buffer size).
static mal_uint32 mal_device__wait_for_frames__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	
	while (!pDevice->alsa.breakFromMainLoop) {
		snd_pcm_sframes_t framesAvailable = snd_pcm_avail((snd_pcm_t*)pDevice->alsa.pPCM);
		if (framesAvailable >= pDevice->fragmentSizeInFrames) {
			return pDevice->fragmentSizeInFrames;
		}
		
		if (framesAvailable < 0) {
			if (framesAvailable == -EPIPE) {
				if (snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, framesAvailable, MAL_TRUE) < 0) {
					return 0;
				}
                
                framesAvailable = snd_pcm_avail((snd_pcm_t*)pDevice->alsa.pPCM);
                if (framesAvailable < 0) {
                    return 0;
                }
			}
		}
        
		const int timeoutInMilliseconds = 20;  // <-- The larger this value, the longer it'll take to stop the device!
		int waitResult = snd_pcm_wait((snd_pcm_t*)pDevice->alsa.pPCM, timeoutInMilliseconds);
		if (waitResult < 0) {
			snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, waitResult, MAL_TRUE);
		}
	}
    
	// We'll get here if the loop was terminated. Just return whatever's available, making sure it's never
    // more than the size of a fragment.
	snd_pcm_sframes_t framesAvailable = snd_pcm_avail((snd_pcm_t*)pDevice->alsa.pPCM);
	if (framesAvailable < 0) {
		return 0;
	}
	
    // There's a small chance we'll have more frames available than the size of a fragment.
	return framesAvailable;
}

static mal_bool32 mal_device_write__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	if (!mal_device_is_started(pDevice)) {
		return MAL_FALSE;
	}
	if (pDevice->alsa.breakFromMainLoop) {
		return MAL_FALSE;
	}
	

	if (pDevice->alsa.pIntermediaryBuffer == NULL) {
		// mmap.
        mal_uint32 framesAvailable = mal_device__wait_for_frames__alsa(pDevice);
        if (framesAvailable == 0) {
            return MAL_FALSE;
        }
        
        // Don't bother asking the client for more audio data if we're just stopping the device anyway.
        if (pDevice->alsa.breakFromMainLoop) {
            return MAL_FALSE;
        }
        
        const snd_pcm_channel_area_t* pAreas;
        snd_pcm_uframes_t mappedOffset;
        snd_pcm_uframes_t mappedFrames = framesAvailable;
        while (framesAvailable > 0) {
            int result = snd_pcm_mmap_begin((snd_pcm_t*)pDevice->alsa.pPCM, &pAreas, &mappedOffset, &mappedFrames);
            if (result < 0) {
                return MAL_FALSE;
            }
            
            void* pBuffer = (mal_uint8*)pAreas[0].addr + ((pAreas[0].first + (mappedOffset * pAreas[0].step)) / 8);
            mal_device__read_samples_from_client(pDevice, mappedFrames * pDevice->channels, pBuffer);
            
            result = snd_pcm_mmap_commit((snd_pcm_t*)pDevice->alsa.pPCM, mappedOffset, mappedFrames);
            if (result < 0 || (snd_pcm_uframes_t)result != mappedFrames) {
                snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, result, MAL_TRUE);
                return MAL_FALSE;
            }
            
            framesAvailable -= mappedFrames;
        }
	} else {
		// readi/writei.
		while (!pDevice->alsa.breakFromMainLoop) {
            mal_uint32 framesAvailable = mal_device__wait_for_frames__alsa(pDevice);
			if (framesAvailable == 0) {
				return MAL_FALSE;
			}
            
            // Don't bother asking the client for more audio data if we're just stopping the device anyway.
            if (pDevice->alsa.breakFromMainLoop) {
                return MAL_FALSE;
            }

            mal_device__read_samples_from_client(pDevice, framesAvailable * pDevice->channels, pDevice->alsa.pIntermediaryBuffer);
        
			snd_pcm_sframes_t framesWritten = snd_pcm_writei((snd_pcm_t*)pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, framesAvailable);
			if (framesWritten < 0) {
				if (framesWritten == -EAGAIN) {
					continue;	// Just keep trying...
				} else if (framesWritten == -EPIPE) {
					// Underrun. Just recover and try writing again.
					if (snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, framesWritten, MAL_TRUE) < 0) {
						return MAL_FALSE;
					}
					
					framesWritten = snd_pcm_writei((snd_pcm_t*)pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, framesAvailable);
					if (framesWritten < 0) {
						return MAL_FALSE;
					}
					
					break;	// Success.
				} else {
					return MAL_FALSE;
				}
			} else {
				break;	// Success.
			}
		}
	}
	
	return MAL_TRUE;
}

static mal_bool32 mal_device_read__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	if (!mal_device_is_started(pDevice)) {
		return MAL_FALSE;
	}
	if (pDevice->alsa.breakFromMainLoop) {
		return MAL_FALSE;
	}
	
	mal_uint32 samplesToSend = 0;
	void* pBuffer = NULL;
	if (pDevice->alsa.pIntermediaryBuffer == NULL) {
		// mmap.
        mal_uint32 framesAvailable = mal_device__wait_for_frames__alsa(pDevice);
        if (framesAvailable == 0) {
            return MAL_FALSE;
        }
        
        const snd_pcm_channel_area_t* pAreas;
        snd_pcm_uframes_t mappedOffset;
        snd_pcm_uframes_t mappedFrames = framesAvailable;
        while (framesAvailable > 0) {
            int result = snd_pcm_mmap_begin((snd_pcm_t*)pDevice->alsa.pPCM, &pAreas, &mappedOffset, &mappedFrames);
            if (result < 0) {
                return MAL_FALSE;
            }
            
            void* pBuffer = (mal_uint8*)pAreas[0].addr + ((pAreas[0].first + (mappedOffset * pAreas[0].step)) / 8);
            mal_device__send_samples_to_client(pDevice, mappedFrames * pDevice->channels, pBuffer);
            
            result = snd_pcm_mmap_commit((snd_pcm_t*)pDevice->alsa.pPCM, mappedOffset, mappedFrames);
            if (result < 0 || (snd_pcm_uframes_t)result != mappedFrames) {
                snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, result, MAL_TRUE);
                return MAL_FALSE;
            }
            
            framesAvailable -= mappedFrames;
        }
	} else {
		// readi/writei.
		snd_pcm_sframes_t framesRead = 0;
		while (!pDevice->alsa.breakFromMainLoop) {
			mal_uint32 framesAvailable = mal_device__wait_for_frames__alsa(pDevice);
			if (framesAvailable == 0) {
				return MAL_FALSE;
			}

			framesRead = snd_pcm_readi((snd_pcm_t*)pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, framesAvailable);
			if (framesRead < 0) {
				if (framesRead == -EAGAIN) {
					continue;	// Just keep trying...
				} else if (framesRead == -EPIPE) {
					// Overrun. Just recover and try reading again.
					if (snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, framesRead, MAL_TRUE) < 0) {
						return MAL_FALSE;
					}
					
					framesRead = snd_pcm_readi((snd_pcm_t*)pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, pDevice->fragmentSizeInFrames);
					if (framesRead < 0) {
						return MAL_FALSE;
					}
					
					break;	// Success.
				} else {
					return MAL_FALSE;
				}
			} else {
				break;	// Success.
			}
		}
		
		samplesToSend = framesRead * pDevice->channels;
		pBuffer = pDevice->alsa.pIntermediaryBuffer;
	}
	
	
	if (pDevice->onRecv && samplesToSend > 0) {
		pDevice->onRecv(pDevice, samplesToSend, pBuffer);
	}
	
	
	if (pDevice->alsa.pIntermediaryBuffer == NULL) {
		// mmap.
	} else {
		// readi/writei.
	}
	
	return MAL_TRUE;
}


static mal_result mal_enumerate_devices__alsa(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
	mal_uint32 infoSize = *pCount;
	*pCount = 0;
    
    // What I've learned about device iteration with ALSA
    // ==================================================
    //
    // The preferred method for enumerating devices is to use snd_device_name_hint() and family. The
    // reason this is preferred is because it includes user-space devices like the "default" device
    // which goes through PulseAudio. The problem, however, is that it is extremely un-user-friendly
    // because it enumerates a _lot_ of devices. On my test machine I have only a typical output device
    // for speakers/headerphones and a microphone - this results 52 devices getting enumerated!
    //
    // One way to pull this back a bit is to ignore all but "hw" devices. At initialization time we
    // can simply append "plug" to the ID string to enable software conversions.
    //
    // An alternative enumeration technique is to use snd_card_next() and family. The problem with this
    // one, which is significant, is that it does _not_ include user-space devices.
    
	char** ppDeviceHints;
    if (snd_device_name_hint(-1, "pcm", (void***)&ppDeviceHints) < 0) {
        return MAL_NO_BACKEND;
    }
	
	char** ppNextDeviceHint = ppDeviceHints;
	while (*ppNextDeviceHint != NULL) {
		char* NAME = snd_device_name_get_hint(*ppNextDeviceHint, "NAME");
		char* DESC = snd_device_name_get_hint(*ppNextDeviceHint, "DESC");
		char* IOID = snd_device_name_get_hint(*ppNextDeviceHint, "IOID");
		
		if (IOID == NULL ||
			(type == mal_device_type_playback && strcmp(IOID, "Output") == 0) ||
			(type == mal_device_type_capture  && strcmp(IOID, "Input" ) == 0))
		{
            // Experiment. Skip over any non "hw" devices to try and pull back on the number
            // of enumerated devices.
            int colonPos;
            mal_find_char(NAME, ':', &colonPos);
            if (colonPos == -1 || (colonPos == 2 && (NAME[0]=='h' && NAME[1]=='w'))) {
                if (pInfo != NULL) {
                    if (infoSize > 0) {
                        mal_zero_object(pInfo);
                        
                        // NAME is the ID.
                        mal_strncpy_s(pInfo->id.str, sizeof(pInfo->id.str), NAME ? NAME : "", (size_t)-1);
    				    
                        // DESC is the name, followed by the description on a new line.
                        int lfPos = 0;
                        mal_find_char(DESC, '\n', &lfPos);
                        
                        mal_strncpy_s(pInfo->name, sizeof(pInfo->name), DESC ? DESC : "", (lfPos != -1) ? (size_t)lfPos : (size_t)-1);
    
    				    pInfo += 1;
    				    infoSize -= 1;
    				    *pCount += 1;
                    }
                } else {
                    *pCount += 1;
                }
            }
		}
		
		free(NAME);
		free(DESC);
		free(IOID);
		ppNextDeviceHint += 1;
	}
	
	snd_device_name_free_hint((void**)ppDeviceHints);
	return MAL_SUCCESS;
}

static void mal_device_uninit__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	
	if ((snd_pcm_t*)pDevice->alsa.pPCM) {
		snd_pcm_close((snd_pcm_t*)pDevice->alsa.pPCM);
		
		if (pDevice->alsa.pIntermediaryBuffer != NULL) {
			mal_free(pDevice->alsa.pIntermediaryBuffer);
		}
	}
}

static mal_result mal_device_init__alsa(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount)
{
	mal_assert(pDevice != NULL);
	pDevice->api = mal_api_alsa;
	
	char deviceName[32];
	if (pDeviceID == NULL) {
		strcpy(deviceName, "default");  // TODO: What if PulseAudio is not installed? Use "plughw:0,0" instead?
	} else {
        // For now, convert "hw" devices to "plughw". The reason for this is that mini_al is still a
        // a quite unstable with non "plughw" devices.
        if (pDeviceID->str[0] == 'h' && pDeviceID->str[1] == 'w' && pDeviceID->str[2] == ':') {
            deviceName[0] = 'p'; deviceName[1] = 'l'; deviceName[2] = 'u'; deviceName[3] = 'g';
            strcpy(deviceName+4, pDeviceID->str);
        } else {
            strcpy(deviceName, pDeviceID->str);
        }
		
	}
	
	snd_pcm_format_t formatALSA;
	switch (format)
	{
		case mal_format_u8:    formatALSA = SND_PCM_FORMAT_U8;         break;
		case mal_format_s16:   formatALSA = SND_PCM_FORMAT_S16_LE;     break;
        case mal_format_s24:   formatALSA = SND_PCM_FORMAT_S24_3LE;    break;
		case mal_format_s32:   formatALSA = SND_PCM_FORMAT_S32_LE;     break;
		case mal_format_f32:   formatALSA = SND_PCM_FORMAT_FLOAT_LE;   break;
        case mal_format_f64:   formatALSA = SND_PCM_FORMAT_FLOAT64_LE; break;
		case mal_format_alaw:  formatALSA = SND_PCM_FORMAT_A_LAW;      break;
		case mal_format_mulaw: formatALSA = SND_PCM_FORMAT_MU_LAW;     break;
		return mal_post_error(pDevice, "[ALSA] Format not supported.", MAL_FORMAT_NOT_SUPPORTED);
	}
	
	if (snd_pcm_open((snd_pcm_t**)&pDevice->alsa.pPCM, deviceName, (type == mal_device_type_playback) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE, 0) < 0) {
		mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] snd_pcm_open() failed.", MAL_ALSA_FAILED_TO_OPEN_DEVICE);
	}
	
	snd_pcm_hw_params_t* pHWParams = NULL;
    snd_pcm_hw_params_alloca(&pHWParams);
	
	if (snd_pcm_hw_params_any((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams) < 0) {
		mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Failed to initialize hardware parameters. snd_pcm_hw_params_any() failed.", MAL_ALSA_FAILED_TO_SET_HW_PARAMS);
	}


    // Most important properties first.    

    // Sample Rate
    if (snd_pcm_hw_params_set_rate_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &sampleRate, 0) < 0) {
		mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Sample rate not supported. snd_pcm_hw_params_set_rate_near() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }
    pDevice->sampleRate = sampleRate;
    
    
    // Channels.
    if (snd_pcm_hw_params_set_channels_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &channels) < 0) {
		mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Failed to set channel count. snd_pcm_hw_params_set_channels_near() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }
    pDevice->channels = channels;
    
    
    // Format.
    if (snd_pcm_hw_params_set_format((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, formatALSA) < 0) {
		mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Format not supported. snd_pcm_hw_params_set_format() failed.", MAL_FORMAT_NOT_SUPPORTED);
	}
    
    
    // Buffer Size
    snd_pcm_uframes_t bufferSize = fragmentSizeInFrames * pDevice->fragmentCount;
	if (snd_pcm_hw_params_set_buffer_size_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &bufferSize) < 0) {
		mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Failed to set buffer size for device. snd_pcm_hw_params_set_buffer_size() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }
    
    
    // Periods.
    int dir = 1;
	if (snd_pcm_hw_params_set_periods_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &fragmentCount, &dir) < 0) {
		mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Failed to set fragment count. snd_pcm_hw_params_set_periods_near() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }
	pDevice->fragmentCount = fragmentCount;
    pDevice->fragmentSizeInFrames = bufferSize / pDevice->fragmentCount;
    
    
    // MMAP Mode
    //
    // Try using interleaved MMAP access. If this fails, fall back to standard readi/writei.
    pDevice->alsa.isUsingMMap = MAL_FALSE;
#ifdef MAL_ENABLE_EXPERIMENTAL_ALSA_MMAP
    if (snd_pcm_hw_params_set_access((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, SND_PCM_ACCESS_MMAP_INTERLEAVED) == 0) {
        pDevice->alsa.isUsingMMap = MAL_TRUE;
        mal_log(pDevice, "USING MMAP\n");
    }
#endif
    
    if (!pDevice->alsa.isUsingMMap) {
        if (snd_pcm_hw_params_set_access((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {;
    		mal_device_uninit__alsa(pDevice);
    		return mal_post_error(pDevice, "[ALSA] Failed to set access mode to neither SND_PCM_ACCESS_MMAP_INTERLEAVED nor SND_PCM_ACCESS_RW_INTERLEAVED. snd_pcm_hw_params_set_access() failed.", MAL_FORMAT_NOT_SUPPORTED);
    	}
    }
    
    
    // Apply hardware parameters.
    if (snd_pcm_hw_params((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams) < 0) {
		mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Failed to set hardware parameters. snd_pcm_hw_params() failed.", MAL_ALSA_FAILED_TO_SET_SW_PARAMS);
    }


	
	snd_pcm_sw_params_t* pSWParams = NULL;
    snd_pcm_sw_params_alloca(&pSWParams);
	
	if (snd_pcm_sw_params_current((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams) != 0) {
        mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Failed to initialize software parameters. snd_pcm_sw_params_current() failed.", MAL_ALSA_FAILED_TO_SET_SW_PARAMS);
    }

    if (snd_pcm_sw_params_set_avail_min((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams, mal_prev_power_of_2(pDevice->fragmentSizeInFrames)) != 0) {
        mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Failed to set fragment size. snd_pcm_sw_params_set_avail_min() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }
	
	if (type == mal_device_type_playback) {
	    if (snd_pcm_sw_params_set_start_threshold((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams, mal_prev_power_of_2(pDevice->fragmentSizeInFrames)) != 0) {
	        mal_device_uninit__alsa(pDevice);
			return mal_post_error(pDevice, "[ALSA] Failed to set start threshold for playback device. snd_pcm_sw_params_set_start_threshold() failed.", MAL_ALSA_FAILED_TO_SET_SW_PARAMS);
	    }
	}

    if (snd_pcm_sw_params((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams) != 0) {
        mal_device_uninit__alsa(pDevice);
		return mal_post_error(pDevice, "[ALSA] Failed to set software parameters. snd_pcm_sw_params() failed.", MAL_ALSA_FAILED_TO_SET_SW_PARAMS);
    }
	
	
    
	
	// If we're _not_ using mmap we need to use an intermediary buffer.
	if (!pDevice->alsa.isUsingMMap) {
		pDevice->alsa.pIntermediaryBuffer = mal_malloc(pDevice->fragmentSizeInFrames*pDevice->fragmentCount * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format));
		if (pDevice->alsa.pIntermediaryBuffer == NULL) {
			mal_device_uninit__alsa(pDevice);
			return mal_post_error(pDevice, "[ALSA] Failed to set software parameters. snd_pcm_sw_params() failed.", MAL_OUT_OF_MEMORY);
		}
	}
	
	
	
	return MAL_SUCCESS;
}


static mal_result mal_device__start_backend__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Prepare the device first...
    snd_pcm_prepare((snd_pcm_t*)pDevice->alsa.pPCM);

    // ... and then grab an initial fragment from the client. After this is done, the device should
    // automatically start playing, since that's how we configured the software parameters.
    if (pDevice->type == mal_device_type_playback) {
        mal_device_write__alsa(pDevice);
    } else {
		snd_pcm_start((snd_pcm_t*)pDevice->alsa.pPCM);
	}

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    snd_pcm_drop((snd_pcm_t*)pDevice->alsa.pPCM);
    return MAL_SUCCESS;
}

static mal_result mal_device__break_main_loop__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
	
	// Fallback. We just set a variable to tell the worker thread to terminate after handling the
	// next fragment. This is a slow way of handling this.
	pDevice->alsa.breakFromMainLoop = MAL_TRUE;
    return MAL_SUCCESS;
}

static mal_result mal_device__main_loop__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

	pDevice->alsa.breakFromMainLoop = MAL_FALSE;
    if (pDevice->type == mal_device_type_playback) {
		// Playback. Read from client, write to device.
		while (!pDevice->alsa.breakFromMainLoop && mal_device_write__alsa(pDevice)) {
		}
	} else {
		// Playback. Read from device, write to client.
		while (!pDevice->alsa.breakFromMainLoop && mal_device_read__alsa(pDevice)) {
		}
	}

    return MAL_SUCCESS;
}


static mal_uint32 mal_device_get_available_rewind_amount__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    return 0;   // Not supporting rewinding with ALSA for the moment.
}

static mal_uint32 mal_device_rewind__alsa(mal_device* pDevice, mal_uint32 frames)
{
    mal_assert(pDevice != NULL);
    mal_assert(frames > 0);
    return 0;
}
#endif

static mal_result mal_device__start_backend(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    
    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->api == mal_api_dsound) {
        result = mal_device__start_backend__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->api == mal_api_alsa) {
        result = mal_device__start_backend__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->api == mal_api_null) {
        result = mal_device__start_backend__null(pDevice);
    }
#endif

    return result;
}

static mal_result mal_device__stop_backend(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    
    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->api == mal_api_dsound) {
        result = mal_device__stop_backend__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->api == mal_api_alsa) {
        result = mal_device__stop_backend__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->api == mal_api_null) {
        result = mal_device__stop_backend__null(pDevice);
    }
#endif

    return result;
}

static mal_result mal_device__break_main_loop(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    
    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->api == mal_api_dsound) {
        result = mal_device__break_main_loop__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->api == mal_api_alsa) {
        result = mal_device__break_main_loop__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->api == mal_api_null) {
        result = mal_device__break_main_loop__null(pDevice);
    }
#endif

    return result;
}

static mal_result mal_device__main_loop(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->api == mal_api_dsound) {
        result = mal_device__main_loop__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->api == mal_api_alsa) {
        result = mal_device__main_loop__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->api == mal_api_null) {
        result = mal_device__main_loop__null(pDevice);
    }
#endif

    return result;
}

mal_thread_result MAL_THREADCALL mal_worker_thread(void* pData)
{
	mal_device* pDevice = (mal_device*)pData;
	mal_assert(pDevice != NULL);

	for (;;) {
        // At the start of iteration the device is stopped - we must explicitly mark it as such.
        mal_device__stop_backend(pDevice);

        // Let the other threads know that the device has stopped.
        mal_device__set_state(pDevice, MAL_STATE_STOPPED);
        mal_event_signal(&pDevice->stopEvent);

        // We use an event to wait for a request to wake up.
		mal_event_wait(&pDevice->wakeupEvent);

        // Default result code.
		pDevice->workResult = MAL_SUCCESS;

		// Just break if we're terminating.
		if (mal_device__get_state(pDevice) == MAL_STATE_UNINITIALIZED) {
			break;
		}

		
		// Getting here means we just started the device and we need to wait for the device to
		// either deliver us data (recording) or request more data (playback).
        mal_assert(mal_device__get_state(pDevice) == MAL_STATE_STARTING);

        pDevice->workResult = mal_device__start_backend(pDevice);
        if (pDevice->workResult != MAL_SUCCESS) {
            mal_event_signal(&pDevice->startEvent);
            continue;
        }

        // The thread that requested the device to start playing is waiting for this thread to start the
        // device for real, which is now.
        mal_device__set_state(pDevice, MAL_STATE_STARTED);
        mal_event_signal(&pDevice->startEvent);

        // Now we just enter the main loop. The main loop can be broken with mal_device__break_main_loop().
        mal_device__main_loop(pDevice);
	}

    // Make sure we aren't continuously waiting on a stop event.
    mal_event_signal(&pDevice->stopEvent);  // <-- Is this still needed?
	return (mal_thread_result)0;
}


// Helper for determining whether or not the given device is initialized.
mal_bool32 mal_device__is_initialized(mal_device* pDevice)
{
	if (pDevice == NULL) return MAL_FALSE;
	return mal_device__get_state(pDevice) != MAL_STATE_UNINITIALIZED;
}


mal_result mal_enumerate_devices(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
	if (pCount == NULL) return mal_post_error(NULL, "mal_enumerate_devices() called with invalid arguments.", MAL_INVALID_ARGS);
	
	mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_DSOUND
	if (result != MAL_SUCCESS) {
		result = mal_enumerate_devices__dsound(type, pCount, pInfo);
	}
#endif
#ifdef MAL_ENABLE_ALSA
	if (result != MAL_SUCCESS) {
		result = mal_enumerate_devices__alsa(type, pCount, pInfo);
	}
#endif
#ifdef MAL_ENABLE_NULL
	if (result != MAL_SUCCESS) {
		result = mal_enumerate_devices__null(type, pCount, pInfo);
	}
#endif

	return result;
}

mal_result mal_device_init(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount, mal_log_proc onLog)
{
	if (pDevice == NULL) return mal_post_error(pDevice, "mal_device_init() called with invalid arguments.", MAL_INVALID_ARGS);
	mal_zero_object(pDevice);
    pDevice->onLog = onLog;     // <-- Set this ASAP to ensure as many log messages are captured as possible during initialization.

    if (((mal_uint64)pDevice % sizeof(pDevice)) != 0) {
        if (pDevice->onLog) {
            pDevice->onLog(pDevice, "WARNING: mal_device_init() called for a device that is not properly aligned. Thread safety is not supported.");
        }
    }
	
	if (channels == 0 || sampleRate == 0 || fragmentSizeInFrames == 0 || fragmentCount == 0) return mal_post_error(pDevice, "mal_device_init() called with invalid arguments.", MAL_INVALID_ARGS);
	
	pDevice->type = type;
	pDevice->format = format;
	pDevice->channels = channels;
	pDevice->sampleRate = sampleRate;
	pDevice->fragmentSizeInFrames = fragmentSizeInFrames;
	pDevice->fragmentCount = fragmentCount;


    // When the device is started, the worker thread is the one that does the actual startup of the backend device. We
    // use a semaphore to wait for the background thread to finish the work. The same applies for stopping the device.
    //
    // Each of these semaphores is released internally by the worker thread when the work is completed. The start
    // semaphore is also used to wake up the worker thread.
	if (!mal_event_create(&pDevice->wakeupEvent)) {
		return mal_post_error(pDevice, "Failed to create worker thread wakeup event.", MAL_FAILED_TO_CREATE_EVENT);
	}
    if (!mal_event_create(&pDevice->startEvent)) {
		mal_event_delete(&pDevice->wakeupEvent);
		return mal_post_error(pDevice, "Failed to create worker thread start event.", MAL_FAILED_TO_CREATE_EVENT);
    }
    if (!mal_event_create(&pDevice->stopEvent)) {
		mal_event_delete(&pDevice->startEvent);
        mal_event_delete(&pDevice->wakeupEvent);
		return mal_post_error(pDevice, "Failed to create worker thread stop event.", MAL_FAILED_TO_CREATE_EVENT);
    }


	mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_DSOUND
	if (result != MAL_SUCCESS) {
		result = mal_device_init__dsound(pDevice, type, pDeviceID, format, channels, sampleRate, fragmentSizeInFrames, fragmentCount);
	}
#endif
#ifdef MAL_ENABLE_ALSA
	if (result != MAL_SUCCESS) {
		result = mal_device_init__alsa(pDevice, type, pDeviceID, format, channels, sampleRate, fragmentSizeInFrames, fragmentCount);
	}
#endif
#ifdef MAL_ENABLE_NULL
	if (result != MAL_SUCCESS) {
		result = mal_device_init__null(pDevice, type, pDeviceID, format, channels, sampleRate, fragmentSizeInFrames, fragmentCount);
	}
#endif

	if (result != MAL_SUCCESS) {
		return MAL_NO_BACKEND;  // The error message will have been posted by the source of the error.
	}


    // The worker thread.
    if (!mal_thread_create(&pDevice->thread, mal_worker_thread, pDevice)) {
		mal_device_uninit(pDevice);
		return mal_post_error(pDevice, "Failed to create worker thread.", MAL_FAILED_TO_CREATE_THREAD);
	}


    // Wait for the worker thread to put the device into it's stopped state for real.
	mal_event_wait(&pDevice->stopEvent);
    mal_assert(mal_device__get_state(pDevice) == MAL_STATE_STOPPED);

	return MAL_SUCCESS;
}

void mal_device_uninit(mal_device* pDevice)
{
	if (!mal_device__is_initialized(pDevice)) return;
	
	// Make sure the device is stopped first. The backends will probably handle this naturally,
	// but I like to do it explicitly for my own sanity.
    if (mal_device_is_started(pDevice)) {
	    while (mal_device_stop(pDevice) == MAL_DEVICE_BUSY) {
            mal_sleep(1);
        }
    }

    // Putting the device into an uninitialized state will make the worker thread return.
    mal_device__set_state(pDevice, MAL_STATE_UNINITIALIZED);
	
    // Wake up the worker thread and wait for it to properly terminate.
	mal_event_signal(&pDevice->wakeupEvent);
	mal_thread_wait(&pDevice->thread);
	
    mal_event_delete(&pDevice->stopEvent);
    mal_event_delete(&pDevice->startEvent);
	mal_event_delete(&pDevice->wakeupEvent);

#ifdef MAL_ENABLE_DSOUND
	if (pDevice->api == mal_api_dsound) {
		mal_device_uninit__dsound(pDevice);
	}
#endif
#ifdef MAL_ENABLE_ALSA
	if (pDevice->api == mal_api_alsa) {
		mal_device_uninit__alsa(pDevice);
	}
#endif
#ifdef MAL_ENABLE_NULL
	if (pDevice->api == mal_api_null) {
		mal_device_uninit__null(pDevice);
	}
#endif

	mal_zero_object(pDevice);
}

void mal_device_set_recv_callback(mal_device* pDevice, mal_recv_proc proc)
{
	if (pDevice == NULL) return;
    mal_atomic_exchange_ptr(&pDevice->onRecv, proc);
}

void mal_device_set_send_callback(mal_device* pDevice, mal_send_proc proc)
{
	if (pDevice == NULL) return;
	mal_atomic_exchange_ptr(&pDevice->onSend, proc);
}

mal_result mal_device_start(mal_device* pDevice)
{
	if (pDevice == NULL) return mal_post_error(pDevice, "mal_device_start() called with invalid arguments.", MAL_INVALID_ARGS);
    if (mal_device__get_state(pDevice) == MAL_STATE_UNINITIALIZED) return mal_post_error(pDevice, "mal_device_start() called for an uninitialized device.", MAL_DEVICE_NOT_INITIALIZED);

    // Be a bit more descriptive if the device is already started or is already in the process of starting. This is likely
    // a bug with the application.
    if (mal_device__get_state(pDevice) == MAL_STATE_STARTING) {
        return mal_post_error(pDevice, "mal_device_start() called while another thread is already starting it.", MAL_DEVICE_ALREADY_STARTING);
    }
    if (mal_device__get_state(pDevice) == MAL_STATE_STARTED) {
        return mal_post_error(pDevice, "mal_device_start() called for a device that's already started.", MAL_DEVICE_ALREADY_STARTED);
    }

    // The device needs to be in a stopped state. If it's not, we just let the caller know the device is busy.
    if (mal_device__get_state(pDevice) != MAL_STATE_STOPPED) {
        return mal_post_error(pDevice, "mal_device_start() called while another thread is in the process of stopping it.", MAL_DEVICE_BUSY);
    }

    mal_device__set_state(pDevice, MAL_STATE_STARTING);
	mal_event_signal(&pDevice->wakeupEvent);

    // Wait for the worker thread to finish starting the device. Note that the worker thread will be the one
    // who puts the device into the started state. Don't call mal_device__set_state() here.
    mal_event_wait(&pDevice->startEvent);
	return pDevice->workResult;
}

mal_result mal_device_stop(mal_device* pDevice)
{
	if (pDevice == NULL) return mal_post_error(pDevice, "mal_device_stop() called with invalid arguments.", MAL_INVALID_ARGS);
    if (mal_device__get_state(pDevice) == MAL_STATE_UNINITIALIZED) return mal_post_error(pDevice, "mal_device_stop() called for an uninitialized device.", MAL_DEVICE_NOT_INITIALIZED);

    // Be a bit more descriptive if the device is already stopped or is already in the process of stopping. This is likely
    // a bug with the application.
    if (mal_device__get_state(pDevice) == MAL_STATE_STOPPING) {
        return mal_post_error(pDevice, "mal_device_stop() called while another thread is already stopping it.", MAL_DEVICE_ALREADY_STOPPING);
    }
    if (mal_device__get_state(pDevice) == MAL_STATE_STOPPED) {
        return mal_post_error(pDevice, "mal_device_stop() called for a device that's already stopped.", MAL_DEVICE_ALREADY_STOPPED);
    }

    // The device needs to be in a started state. If it's not, we just let the caller know the device is busy.
    if (mal_device__get_state(pDevice) != MAL_STATE_STARTED) {
        return mal_post_error(pDevice, "mal_device_stop() called while another thread is in the process of starting it.", MAL_DEVICE_BUSY);
    }

    mal_device__set_state(pDevice, MAL_STATE_STOPPING);

    // There's no need to wake up the thread like we do when starting.
	
    // When we get here the worker thread is likely in a wait state while waiting for the backend device to deliver or request
    // audio data. We need to force these to return as quickly as possible.
    mal_device__break_main_loop(pDevice);

    // We need to wait for the worker thread to become available for work before returning. Note that the worker thread will be
    // the one who puts the device into the stopped state. Don't call mal_device__set_state() here.
    mal_event_wait(&pDevice->stopEvent);
	return MAL_SUCCESS;
}

mal_bool32 mal_device_is_started(mal_device* pDevice)
{
	if (pDevice == NULL) return MAL_FALSE;
	return mal_device__get_state(pDevice) == MAL_STATE_STARTED;
}

mal_uint32 mal_device_get_available_rewind_amount(mal_device* pDevice)
{
    if (pDevice == NULL) return 0;
    
#ifdef MAL_ENABLE_DSOUND
	if (pDevice->api == mal_api_dsound) {
		return mal_device_get_available_rewind_amount__dsound(pDevice);
	}
#endif
#ifdef MAL_ENABLE_ALSA
	if (pDevice->api == mal_api_alsa) {
		return mal_device_get_available_rewind_amount__alsa(pDevice);
	}
#endif
#ifdef MAL_ENABLE_NULL
	if (pDevice->api == mal_api_null) {
		return mal_device_get_available_rewind_amount__null(pDevice);
	}
#endif

    return 0;
}

mal_uint32 mal_device_rewind(mal_device* pDevice, mal_uint32 frames)
{
    if (pDevice == NULL || frames == 0) return 0;
#ifdef MAL_ENABLE_DSOUND
	if (pDevice->api == mal_api_dsound) {
		return mal_device_rewind__dsound(pDevice, frames);
	}
#endif
#ifdef MAL_ENABLE_ALSA
	if (pDevice->api == mal_api_alsa) {
		return mal_device_rewind__alsa(pDevice, frames);
	}
#endif
#ifdef MAL_ENABLE_NULL
	if (pDevice->api == mal_api_null) {
		return mal_device_rewind__null(pDevice, frames);
	}
#endif

    return 0;
}

mal_uint32 mal_device_get_fragment_size_in_bytes(mal_device* pDevice)
{
    if (pDevice == NULL) return 0;
    return pDevice->fragmentSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
}

mal_uint32 mal_get_sample_size_in_bytes(mal_format format)
{
	mal_uint32 sizes[] = {
		1,	// u8
		2,	// s16
		3,	// s24
		4,	// s32
		4,	// f32
		8,	// f64
		1,	// alaw
		1	// mulaw
	};
	return sizes[format];
}
#endif


// REVISION HISTORY
// ================
//
// v0.1 - TBD
//   - Initial versioned release


// TODO
// ====
// - Support rewinding. This will enable applications to employ better anti-latency.
// - Thread safety for start, stop and rewind.
//
//
// ALSA
// ----
// - Make device initialization more robust.
//   - Need to have my test hardware working with plughw at a minimum.
// - Finish mmap mode for ALSA.


/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/