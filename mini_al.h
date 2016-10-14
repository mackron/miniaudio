// Mini audio library. Public domain. See "unlicense" statement at the end of this file.
// mini_al - v0.0 - UNRELEASED
//
// David Reid - mackron@gmail.com

// USAGE
// =====
//
// (WRITE ME)
//
//
// NOTES
// =====
// - This library uses an asynchronous API delivering and requesting audio data. Each device will have
//   it's own worker thread which is managed by the library.
// - This is not currently thread-safe, but can still be used from multiple threads if you do your own
//   synchronization.

#ifndef dr_mal_h
#define dr_mal_h

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
	
	// Unfortunate #includes, but needed for pthread_t and sem_t types.
	#include <pthread.h>
	#include <semaphore.h>
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
	typedef mal_handle mal_semaphore;
#else
	typedef pthread_t mal_thread;
	typedef sem_t mal_semaphore;
#endif

#ifdef MAL_ENABLE_DSOUND
    #define MAL_MAX_FRAGMENTS_DSOUND    16
#endif

typedef int mal_result;
#define MAL_SUCCESS					0
#define MAL_UNKNOWN_ERROR			-1
#define MAL_INVALID_ARGS			-2
#define MAL_OUT_OF_MEMORY			-3
#define MAL_NO_BACKEND				-16
#define MAL_DEVICE_ALREADY_STARTED	-17
#define MAL_DEVICE_ALREADY_STOPPED  -18
#define MAL_FAILED_TO_INIT_BACKEND  -19
#define MAL_FORMAT_NOT_SUPPORTED	-20

typedef struct mal_device mal_device;

typedef void (* mal_recv_proc)(mal_device* pDevice, mal_uint32 sampleCount, const void* pSamples);
typedef mal_uint32 (* mal_send_proc)(mal_device* pDevice, mal_uint32 sampleCount, void* pSamples);

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
	// I like to keep these explicitly defined because they're used as a key into a lookup table.
	mal_format_u8    = 0,
	mal_format_s16   = 1,
	mal_format_s24   = 2,
	mal_format_s32   = 3,
	mal_format_f32   = 4,
	mal_format_f64   = 5,
	mal_format_alaw  = 6,
	mal_format_mulaw = 7
} mal_format;

typedef union
{
	char name[256];		// ALSA uses a name string for identification.
	mal_uint8 guid[16];	// DirectSound uses a GUID to identify a device.
} mal_device_id;

typedef struct
{
	mal_device_id id;
	char description[256];
} mal_device_info;

struct mal_device
{
	mal_api api;		// DirectSound, ALSA, etc.
	mal_device_type type;
	mal_format format;
	mal_uint32 channels;
	mal_uint32 sampleRate;
	mal_uint32 fragmentSizeInFrames;
	mal_uint32 fragmentCount;
	mal_uint32 flags;
	mal_recv_proc onRecv;
	mal_send_proc onSend;
	void* pUserData;	// Application defined data.
	
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
            mal_thread thread;
            mal_semaphore semaphore;    // <-- This is used to wake up the worker thread.
		} dsound;
	#endif
		
	#ifdef MAL_ENABLE_ALSA
		struct
		{
			/*snd_pcm_t**/mal_ptr pPCM;
			mal_thread thread;
			mal_semaphore semaphore;	// <-- This is used to wake up the thread.
			void* pIntermediaryBuffer;
		} alsa;
	#endif
		
	#ifdef MAL_ENABLE_NULL
		struct
		{
			int unused;
		} null_device;
	#endif
	};
};

// Enumerates over each device of the given type (playback or capture).
//
// Thread Safety: SAFE, SEE NOTES.
//   This API uses an application-defined buffer for output. This is thread-safe so long as the
//   application ensures mutal exclusion to the output buffer at their level.
//
// Efficiency: SLOW
//   This API dynamically links to backend DLLs/SOs (such as dsound.dll).
mal_result mal_enumerate_devices(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo);

// Initializes a device.
//
// The device ID (pDeviceID) can be null, in which case the default device is used. Otherwise, you
// can retrieve the ID by calling mal_enumerate_devices() and retrieve the ID from the returned
// information.
//
// This will try it's hardest to create a valid device, even if it means adjusting input arguments.
// Use pDevice->channels, pDevice->sampleRate, etc. to determine the actual properties after
// initialization.
//
// Thread Safety: SAFE
//   This API is thread safe so long as the application does not try to use the device object before
//   this call has returned.
//
// Efficiency: SLOW
//   This API will dynamically link to backend DLLs/SOs like dsound.dll, and is otherwise just slow
//   due to the fact that it's an initialization API.
mal_result mal_device_init(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount);

// Uninitializes a device.
//
// This will explicitly stop the device. You do not need to call mal_device_stop() beforehand, but it's
// harmless if you do.
//
// Thread Safety: UNSAFE
//   This API shouldn't crash in a multi-threaded environment, but results are undefined if an application
//   attempts to do something with the device at the same time as uninitializing.
//
// Efficiency: SLOW
//   This will stop the device with mal_device_stop() which is a slow, synchronized call. It also needs
//   to destroy internal objects like the backend-specific objects and the background thread.
void mal_device_uninit(mal_device* pDevice);

// Sets the callback to use when the application has receives data from the device.
//
// Thread Safety: SAFE
//   This API is implemented as a simple atomic assignment.
//
// Efficiency: FAST
//   This is just an atomic assignment.
void mal_device_set_recv_callback(mal_device* pDevice, mal_recv_proc proc);

// Sets the callback to use when the application needs to send data to the device for playback.
//
// Thread Safety: SAFE
//   This API is implemented as a simple atomic assignment.
//
// Efficiency: FAST
//   This is just an atomic assignment.
void mal_device_set_send_callback(mal_device* pDevice, mal_send_proc proc);

// Activates the device. For playback devices this begins playback. For recording devices it begins
// recording.
//
// Thread Safety: SAFE
//
// Efficiency: SLOW
//   This API needs to wait on the worker thread via a semaphore.
mal_result mal_device_start(mal_device* pDevice);

// Puts the device to sleep, but does not uninitialize it. Use mal_device_start() to start it up again.
//
// Thread Safety: SAFE
//
// Efficiency: SLOW
//   This API needs to wait on the worker thread to stop the backend device properly before returning.
mal_result mal_device_stop(mal_device* pDevice);

// Determines whether or not the device is started.
//
// Thread Safety: SAFE
//   If another thread calls mal_device_start() or mal_device_stop() at this same time as this function
//   is called, there's a very small chance the return value will out of sync.
//
// Efficiency: FAST
//   This is implemented with a simple accessor.
mal_bool32 mal_device_is_started(mal_device* pDevice);

// Retrieves the size of a fragment in bytes for the given device.
//
// Thread Safety: SAFE
//   This is calculated from constant values which are set at initialization time and never change.
//
// Efficiency: FAST
//   This is implemented with just a few 32-bit integer multiplications.
mal_uint32 mal_device_get_fragment_size_in_bytes(mal_device* pDevice);

// Retrieves the size of a sample in bytes for the given format.
//
// Thread Safety: SAFE
//   This is API is pure.
//
// Efficiency: FAST
//   This is implemented with a lookup table.
mal_uint32 mal_get_sample_size_in_bytes(mal_format format);


#ifdef __cplusplus
}
#endif
#endif  //dr_mal_h


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
#include <string.h>	// For memset()
#endif

#ifdef MAL_POSIX
#include <pthread.h>
#include <semaphore.h>
#endif

#include <assert.h>
#include <errno.h>

#include <stdio.h>  // For printf() debugging. TODO: Delete this and replace with a proper logging system.

#ifdef MAL_WIN32
	#define MAL_THREADCALL WINAPI
	typedef mal_uint32 mal_thread_result;
#else
	#define MAL_THREADCALL
	typedef void* mal_thread_result;
#endif
typedef mal_thread_result (MAL_THREADCALL * mal_thread_entry_proc)(void* pData);


#define MAL_FLAG_INITIALIZED	(1 << 0)
#define MAL_FLAG_AWAKE			(1 << 1)
#define MAL_FLAG_STARTED		(1 << 2)	// Whether or not the device is currently awake and running.
#define MAL_FLAG_TERMINATING	(1 << 3)	// Used for thread management.


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
#ifdef _WIN32
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

static int mal_strncpy_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return EINVAL;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
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
    return ERANGE;
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


mal_bool32 mal_semaphore_create__win32(mal_semaphore* pSemaphore, int initialValue)
{
    *pSemaphore = CreateSemaphoreA(NULL, initialValue, LONG_MAX, NULL);
    if (*pSemaphore == NULL) {
        return MAL_FALSE;
    }

    return MAL_TRUE;
}

void mal_semaphore_delete__win32(mal_semaphore* pSemaphore)
{
    CloseHandle(*pSemaphore);
}

mal_bool32 mal_semaphore_wait__win32(mal_semaphore* pSemaphore)
{
    return WaitForSingleObject(*pSemaphore, INFINITE) == WAIT_OBJECT_0;
}

mal_bool32 mal_semaphore_release__win32(mal_semaphore* pSemaphore)
{
    return ReleaseSemaphore(*pSemaphore, 1, NULL) != 0;
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


mal_bool32 mal_semaphore_create__posix(mal_semaphore* pSemaphore, int initialValue)
{
    return sem_init(pSemaphore, 0, (unsigned int)initialValue) != -1;
}

void mal_semaphore_delete__posix(mal_semaphore* pSemaphore)
{
    sem_close(pSemaphore);
}

mal_bool32 mal_semaphore_wait__posix(mal_semaphore* pSemaphore)
{
    return sem_wait(pSemaphore) != -1;
}

mal_bool32 mal_semaphore_release__posix(mal_semaphore* pSemaphore)
{
    return sem_post(pSemaphore) != -1;
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


mal_bool32 mal_semaphore_create(mal_semaphore* pSemaphore, int initialValue)
{
    if (pSemaphore == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_semaphore_create__win32(pSemaphore, initialValue);
#endif

#ifdef MAL_POSIX
    return mal_semaphore_create__posix(pSemaphore, initialValue);
#endif
}

void mal_semaphore_delete(mal_semaphore* pSemaphore)
{
    if (pSemaphore == NULL) return;

#ifdef MAL_WIN32
    mal_semaphore_delete__win32(pSemaphore);
#endif

#ifdef MAL_POSIX
    mal_semaphore_delete__posix(pSemaphore);
#endif
}

mal_bool32 mal_semaphore_wait(mal_semaphore* pSemaphore)
{
    if (pSemaphore == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_semaphore_wait__win32(pSemaphore);
#endif

#ifdef MAL_POSIX
    return mal_semaphore_wait__posix(pSemaphore);
#endif
}

mal_bool32 mal_semaphore_release(mal_semaphore* pSemaphore)
{
    if (pSemaphore == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_semaphore_release__win32(pSemaphore);
#endif

#ifdef MAL_POSIX
    return mal_semaphore_release__posix(pSemaphore);
#endif
}



// A helper function for reading sample data from the client. Returns the number of samples read from the client. Remaining samples
// are filled with silence.
static inline mal_uint32 mal_device__read_fragment_from_client(mal_device* pDevice, mal_uint32 sampleCount, void* pSamples)
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
static inline void mal_device__send_fragment_to_client(mal_device* pDevice, mal_uint32 sampleCount, const void* pSamples)
{
    mal_assert(pDevice != NULL);
    mal_assert(sampleCount > 0);
    mal_assert(pSamples != NULL);

    if (pDevice->onRecv) {
        pDevice->onRecv(pDevice, sampleCount, pSamples);
    }
}


///////////////////////////////////////////////////////////////////////////////
//
// Null Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_NULL
mal_result mal_enumerate_devices__null(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
	mal_uint32 infoSize = *pCount;
	*pCount = 1;	// There's only one "device" each for playback and recording for the null backend.
	
	if (pInfo != NULL && infoSize > 0) {
		mal_zero_memory(&pInfo->id, sizeof(pInfo->id));
		
		if (type == mal_device_type_playback) {
			mal_strncpy_s(pInfo->description, sizeof(pInfo->description), "NULL Playback Device", (size_t)-1);
		} else {
			mal_strncpy_s(pInfo->description, sizeof(pInfo->description), "NULL Capture Device", (size_t)-1);
		}
	}
	
	return MAL_SUCCESS;
}

void mal_device_uninit__null(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
}

mal_result mal_device_init__null(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount)
{
	mal_assert(pDevice != NULL);
	pDevice->api = mal_api_null;
	
	// TODO: Implement me.
	return MAL_NO_BACKEND;
}

mal_result mal_device_start__null(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);

	return MAL_UNKNOWN_ERROR;
}

mal_result mal_device_stop__null(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	
	return MAL_UNKNOWN_ERROR;
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

#define MAL_FLAG_DSOUND_HAS_SEMAPHORE   (1 << 16)	// Used for cleanup.
#define MAL_FLAG_DSOUND_HAS_THREAD      (1 << 17)	// Used for cleanup.

static HMODULE mal_open_dsound_dll()
{
    return LoadLibraryW(L"dsound.dll");
}

static void mal_close_dsound_dll(HMODULE hModule)
{
    FreeLibrary(hModule);
}

static mal_result mal_device__read_fragment_from_client__dsound(mal_device* pDevice, mal_uint32 fragmentIndex)
{
    mal_assert(pDevice != NULL);

    DWORD fragmentSizeInBytes = pDevice->fragmentSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
    DWORD offset = fragmentIndex * fragmentSizeInBytes;

    void* pLockPtr;
    DWORD lockSize;
    if (FAILED(IDirectSoundBuffer_Lock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, offset, fragmentSizeInBytes, &pLockPtr, &lockSize, NULL, NULL, 0))) {
        return MAL_UNKNOWN_ERROR;
    }

    mal_device__read_fragment_from_client(pDevice, pDevice->fragmentSizeInFrames * pDevice->channels, pLockPtr);

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
        return MAL_UNKNOWN_ERROR;
    }

    mal_device__send_fragment_to_client(pDevice, pDevice->fragmentSizeInFrames * pDevice->channels, pLockPtr);

    IDirectSoundCaptureBuffer_Unlock((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, pLockPtr, lockSize, NULL, 0);
    return MAL_SUCCESS;
}

mal_thread_result MAL_THREADCALL mal_worker_thread__dsound(void* pData)
{
	mal_device* pDevice = (mal_device*)pData;
	mal_assert(pDevice != NULL);

	for (;;) {
		mal_semaphore_wait(&pDevice->dsound.semaphore);
		
		// Just break if we're terminating.
		if (pDevice->flags & MAL_FLAG_TERMINATING) {
			break;
		}
		
		// Continue if the device has been stopped.
		if (!mal_device_is_started(pDevice)) {
            if (pDevice->type == mal_device_type_playback) {
                IDirectSoundBuffer_Stop((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer);
                IDirectSoundBuffer_SetCurrentPosition((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, 0);
            } else {
                IDirectSoundCaptureBuffer_Stop((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer);
            }

			continue;
		}
		
		// Getting here means we just started the device and we need to wait for the device to
		// either deliver us data (recording) or request more data (playback).
		if (pDevice->type == mal_device_type_playback) {
            // Before playing anything we need to grab an initial fragment of sample data from the client.
            if (mal_device__read_fragment_from_client__dsound(pDevice, 0) != MAL_SUCCESS) {
                continue;   // Just cancel playback and go back to the start of the loop.
            }

            IDirectSoundBuffer_Play((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, 0, 0, DSBPLAY_LOOPING);
        } else {
            IDirectSoundCaptureBuffer8_Start((LPDIRECTSOUNDCAPTUREBUFFER8)pDevice->dsound.pCaptureBuffer, DSCBSTART_LOOPING);
        }

        for (;;) {
            // Wait for a notification. Notifications are tied to fragments.
            unsigned int eventCount = pDevice->fragmentCount + 1;
            HANDLE eventHandles[MAL_MAX_FRAGMENTS_DSOUND + 1];   // +1 for the stop event.
            mal_copy_memory(eventHandles, pDevice->dsound.pNotifyEvents, sizeof(HANDLE) * pDevice->fragmentCount);
            eventHandles[eventCount-1] = pDevice->dsound.hStopEvent;

            DWORD rc = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);
            if (rc < WAIT_OBJECT_0 || rc >= WAIT_OBJECT_0 + eventCount) {
                break;
            }

            unsigned int eventIndex = rc - WAIT_OBJECT_0;
            HANDLE hEvent = eventHandles[eventIndex];

            // Has the device been stopped? If so, need to get out of this loop.
            if (hEvent == pDevice->dsound.hStopEvent) {
                break;
            }

            // If we get here it means the event that's been signaled represents a fragment.
            unsigned int fragmentIndex = eventIndex;    // <-- Just for clarity.
            mal_assert(fragmentIndex < pDevice->fragmentCount);
            
            if (pDevice->type == mal_device_type_playback) {
                mal_device__read_fragment_from_client__dsound(pDevice, (fragmentIndex + 1) % pDevice->fragmentCount);
            } else {
                mal_device__send_fragment_to_client__dsound(pDevice, fragmentIndex);
            }
        }
	}

	return (mal_thread_result)0;
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
            mal_strncpy_s(pData->pInfo->description, sizeof(pData->pInfo->description), lpcstrDescription, (size_t)-1);

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

mal_result mal_enumerate_devices__dsound(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
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

void mal_device_uninit__dsound(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);

    pDevice->flags |= MAL_FLAG_TERMINATING;
	
	if (pDevice->flags & MAL_FLAG_DSOUND_HAS_THREAD) {
		mal_semaphore_release(&pDevice->dsound.semaphore);
		mal_thread_wait(&pDevice->dsound.thread);
	}
	
	if (pDevice->flags & MAL_FLAG_DSOUND_HAS_SEMAPHORE) {
		mal_semaphore_delete(&pDevice->dsound.semaphore);
	}

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

mal_result mal_device_init__dsound(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount)
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

        case mal_format_s24:    // <-- Untested. Not quite sure on alignment.
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
        mal_DirectSoundCreate8Proc pDirectSoundCreate8 = (mal_DirectSoundCreate8Proc)GetProcAddress(pDevice->dsound.hDSoundDLL, "DirectSoundCreate8");
        if (pDirectSoundCreate8 == NULL) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }

        if (FAILED(pDirectSoundCreate8((pDeviceID == NULL) ? NULL : (LPCGUID)pDeviceID->guid, (LPDIRECTSOUND8*)&pDevice->dsound.pPlayback, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }

        // The cooperative level must be set before doing anything else.
        if (FAILED(IDirectSound_SetCooperativeLevel((LPDIRECTSOUND8)pDevice->dsound.pPlayback, GetForegroundWindow(), DSSCL_PRIORITY))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }

        DSBUFFERDESC descDSPrimary;
        mal_zero_object(&descDSPrimary);
        descDSPrimary.dwSize  = sizeof(DSBUFFERDESC);
        descDSPrimary.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
        if (FAILED(IDirectSound_CreateSoundBuffer((LPDIRECTSOUND8)pDevice->dsound.pPlayback, &descDSPrimary, (LPDIRECTSOUNDBUFFER*)&pDevice->dsound.pPlaybackPrimaryBuffer, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }

        // From MSDN:
        //
        // The method succeeds even if the hardware does not support the requested format; DirectSound sets the buffer to the closest
        // supported format. To determine whether this has happened, an application can call the GetFormat method for the primary buffer
        // and compare the result with the format that was requested with the SetFormat method.
        if (FAILED(IDirectSoundBuffer_SetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, (WAVEFORMATEX*)&wf))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }

        // Get the _actual_ properties of the buffer. This is silly API design...
        DWORD requiredSize;
        if (FAILED(IDirectSoundBuffer_GetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, NULL, 0, &requiredSize))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }

        char rawdata[1024];
        WAVEFORMATEXTENSIBLE* pActualFormat = (WAVEFORMATEXTENSIBLE*)rawdata;
        if (FAILED(IDirectSoundBuffer_GetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, (WAVEFORMATEX*)pActualFormat, requiredSize, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
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
            return MAL_NO_BACKEND;
        }

        
        // Notifications are set up via a DIRECTSOUNDNOTIFY object which is retrieved from the buffer.
        if (FAILED(IDirectSoundBuffer8_QueryInterface((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, g_mal_GUID_IID_DirectSoundNotify, (void**)&pDevice->dsound.pNotify))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }
    } else {
        mal_DirectSoundCaptureCreate8Proc pDirectSoundCaptureCreate8 = (mal_DirectSoundCaptureCreate8Proc)GetProcAddress(pDevice->dsound.hDSoundDLL, "DirectSoundCaptureCreate8");
        if (pDirectSoundCaptureCreate8 == NULL) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }

        if (FAILED(pDirectSoundCaptureCreate8((pDeviceID == NULL) ? NULL : (LPCGUID)pDeviceID->guid, (LPDIRECTSOUNDCAPTURE8*)&pDevice->dsound.pCapture, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
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
            return MAL_NO_BACKEND;
        }

        HRESULT hr = IDirectSoundCapture_QueryInterface(pDSCB_Temp, g_mal_GUID_IID_IDirectSoundCaptureBuffer8, (LPVOID*)&pDevice->dsound.pCaptureBuffer);
        IDirectSoundCaptureBuffer_Release(pDSCB_Temp);
        if (FAILED(hr)) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
        }

        // Notifications are set up via a DIRECTSOUNDNOTIFY object which is retrieved from the buffer.
        if (FAILED(IDirectSoundCaptureBuffer8_QueryInterface((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, g_mal_GUID_IID_DirectSoundNotify, (void**)&pDevice->dsound.pNotify))) {
            mal_device_uninit__dsound(pDevice);
            return MAL_NO_BACKEND;
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
            return MAL_NO_BACKEND;
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
        return MAL_NO_BACKEND;
    }



    // When the device is playing the worker thread will be waiting on a bunch of notification events. To return from
    // this wait state we need to signal a special event.
    pDevice->dsound.hStopEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (pDevice->dsound.hStopEvent == NULL) {
        mal_device_uninit__dsound(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }


    if (!mal_semaphore_create(&pDevice->dsound.semaphore, 0)) {
		mal_device_uninit__dsound(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	pDevice->flags |= MAL_FLAG_DSOUND_HAS_SEMAPHORE;
	
	if (!mal_thread_create(&pDevice->dsound.thread, mal_worker_thread__dsound, pDevice)) {
		mal_device_uninit__dsound(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	pDevice->flags |= MAL_FLAG_DSOUND_HAS_THREAD;


	return MAL_SUCCESS;
}

mal_result mal_device_start__dsound(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);

    // We don't actually start the device here. We instead signal the semaphore on the worker thread and
	// let that thread play the device.
	pDevice->flags |= MAL_FLAG_STARTED;
	mal_semaphore_release(&pDevice->dsound.semaphore);

    // Make sure the signal used to stop the worker thread is no longer signaled.
    ResetEvent(pDevice->dsound.hStopEvent);
	return MAL_SUCCESS;
}

mal_result mal_device_stop__dsound(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	
    // The device is not stopped here. We instead mark the device as stopped and signal the semaphore
	// on the worker thread.
	pDevice->flags &= ~MAL_FLAG_STARTED;
	mal_semaphore_release(&pDevice->dsound.semaphore);

    // The worker thread is likely waiting on 
    SetEvent(pDevice->dsound.hStopEvent);
	return MAL_SUCCESS;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// ALSA Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_ALSA
#include <alsa/asoundlib.h>

#define MAL_FLAG_ALSA_USING_MMAP	(1 << 15)
#define MAL_FLAG_ALSA_HAS_SEMAPHORE	(1 << 16)	// Used for cleanup.
#define MAL_FLAG_ALSA_HAS_THREAD	(1 << 17)	// Used for cleanup.

mal_bool32 mal_device_write__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	if (!mal_device_is_started(pDevice)) {
		return MAL_FALSE;
	}
	
	void* pBuffer = NULL;
	if (pDevice->alsa.pIntermediaryBuffer == NULL) {
		// mmap.
		return MAL_FALSE;
	} else {
		// readi/writei.
		pBuffer = pDevice->alsa.pIntermediaryBuffer;
	}
	
	
	mal_uint32 desiredSampleCount = pDevice->fragmentSizeInFrames * pDevice->channels;
	mal_uint32 samplesRead = 0;
	if (pDevice->onSend) {
		samplesRead = pDevice->onSend(pDevice, desiredSampleCount, pBuffer);
		if (samplesRead != desiredSampleCount) {
			// Not enough samples were read. Fill the remainder with silence.
			mal_uint32 sampleSize = mal_get_sample_size_in_bytes(pDevice->format);
			mal_uint32 consumedBytes = samplesRead*sampleSize;
			mal_uint32 remainingBytes = (desiredSampleCount-samplesRead)*sampleSize;
			mal_zero_memory((mal_uint8*)pBuffer + consumedBytes, remainingBytes);
		}
	}
	
	if (pDevice->alsa.pIntermediaryBuffer == NULL) {
		// mmap.
	} else {
		// readi/writei.
		for (;;) {
			snd_pcm_sframes_t framesWritten = snd_pcm_writei(pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, pDevice->fragmentSizeInFrames);
			if (framesWritten < 0) {
				if (framesWritten == -EAGAIN) {
					continue;	// Just keep trying...
				} else if (framesWritten == -EPIPE) {
					// Underrun. Just recover and try writing again.
					if (snd_pcm_recover(pDevice->alsa.pPCM, framesWritten, MAL_TRUE) < 0) {
						return MAL_FALSE;
					}
					
					framesWritten = snd_pcm_writei(pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, pDevice->fragmentSizeInFrames);
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

mal_bool32 mal_device_read__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	if (!mal_device_is_started(pDevice)) {
		return MAL_FALSE;
	}
	
	mal_uint32 samplesToSend = 0;
	void* pBuffer = NULL;
	if (pDevice->alsa.pIntermediaryBuffer == NULL) {
		// mmap.
		return MAL_FALSE;
	} else {
		// readi/writei.
		snd_pcm_sframes_t framesRead = 0;
		for (;;) {
			framesRead = snd_pcm_readi(pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, pDevice->fragmentSizeInFrames);
			if (framesRead < 0) {
				if (framesRead == -EAGAIN) {
					continue;	// Just keep trying...
				} else if (framesRead == -EPIPE) {
					// Overrun. Just recover and try reading again.
					if (snd_pcm_recover(pDevice->alsa.pPCM, framesRead, MAL_TRUE) < 0) {
						return MAL_FALSE;
					}
					
					snd_pcm_sframes_t framesRead = snd_pcm_readi(pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, pDevice->fragmentSizeInFrames);
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


mal_thread_result MAL_THREADCALL mal_worker_thread__alsa(void* pData)
{
	mal_device* pDevice = (mal_device*)pData;
	mal_assert(pDevice != NULL);

	for (;;) {
		mal_semaphore_wait(&pDevice->alsa.semaphore);
		
		// Just break if we're terminating.
		if (pDevice->flags & MAL_FLAG_TERMINATING) {
			break;
		}
		
		// Continue if the device has been stopped.
		if (!mal_device_is_started(pDevice)) {
			snd_pcm_drop(pDevice->alsa.pPCM);
			continue;
		}
		
		// Getting here means we just started the device and we need to wait for the device to
		// either deliver us data (recording) or request more data (playback).
		snd_pcm_prepare(pDevice->alsa.pPCM);
		
		if (pDevice->type == mal_device_type_playback) {
			// Playback. Read from client, write to device.
			while (mal_device_write__alsa(pDevice)) {
			}
		} else {
			// Playback. Read from device, write to client.
			while (mal_device_read__alsa(pDevice)) {
			}
		}
	}

	return (mal_thread_result)0;
}

mal_result mal_enumerate_devices__alsa(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
	mal_uint32 infoSize = *pCount;
	*pCount = 0;
	
	char** ppDeviceHints;
    if (snd_device_name_hint(-1, "pcm", (void***)&ppDeviceHints) < 0) {
        return MAL_UNKNOWN_ERROR;
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
            if (pInfo != NULL) {
                if (infoSize > 0) {
                    mal_strncpy_s(pInfo->id.name,     sizeof(pInfo->id.name),     NAME ? NAME : "", (size_t)-1);
				    mal_strncpy_s(pInfo->description, sizeof(pInfo->description), DESC ? DESC : "", (size_t)-1);
				
				    pInfo += 1;
				    infoSize -= 1;
				    *pCount += 1;
                }
            } else {
                *pCount += 1;
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

void mal_device_uninit__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	
	pDevice->flags |= MAL_FLAG_TERMINATING;
	
	if (pDevice->flags & MAL_FLAG_ALSA_HAS_THREAD) {
		mal_semaphore_release(&pDevice->alsa.semaphore);
		mal_thread_wait(&pDevice->alsa.thread);
	}
	
	if (pDevice->flags & MAL_FLAG_ALSA_HAS_SEMAPHORE) {
		mal_semaphore_delete(&pDevice->alsa.semaphore);
	}
	
	if (pDevice->alsa.pPCM) {
		snd_pcm_close((snd_pcm_t*)pDevice->alsa.pPCM);
		
		if (pDevice->alsa.pIntermediaryBuffer == NULL) {
			mal_free(pDevice->alsa.pIntermediaryBuffer);
		}
	}
}

mal_result mal_device_init__alsa(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount)
{
	mal_assert(pDevice != NULL);
	pDevice->api = mal_api_alsa;
	
	char deviceName[256];
	if (pDeviceID == NULL) {
		strcpy(deviceName, "default");
	} else {
		strcpy(deviceName, pDeviceID->name);
	}
	
	snd_pcm_format_t formatALSA;
	switch (format)
	{
		case mal_format_u8:    formatALSA = SND_PCM_FORMAT_U8;       break;
		case mal_format_s16:   formatALSA = SND_PCM_FORMAT_S16_LE;   break;
		case mal_format_s32:   formatALSA = SND_PCM_FORMAT_S32_LE;   break;
		case mal_format_f32:   formatALSA = SND_PCM_FORMAT_FLOAT_LE; break;
		case mal_format_alaw:  formatALSA = SND_PCM_FORMAT_A_LAW;    break;
		case mal_format_mulaw: formatALSA = SND_PCM_FORMAT_MU_LAW;   break;
		default: return MAL_FORMAT_NOT_SUPPORTED;
	}
	
	if (snd_pcm_open((snd_pcm_t**)&pDevice->alsa.pPCM, deviceName, (type == mal_device_type_playback) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE, 0) < 0) {
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	
	snd_pcm_hw_params_t* pHWParams = NULL;
	if (snd_pcm_hw_params_malloc(&pHWParams) < 0) {
		mal_device_uninit__alsa(pDevice);
		return MAL_OUT_OF_MEMORY;
	}
	
	if (snd_pcm_hw_params_any((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams) < 0) {
		snd_pcm_hw_params_free(pHWParams);
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	
	if (snd_pcm_hw_params_set_access((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		snd_pcm_hw_params_free(pHWParams);
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	
	if (snd_pcm_hw_params_set_format((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, formatALSA) < 0) {
		snd_pcm_hw_params_free(pHWParams);
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	
	if (snd_pcm_hw_params_set_rate_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &sampleRate, 0) < 0) {
        snd_pcm_hw_params_free(pHWParams);
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }

    if (snd_pcm_hw_params_set_channels_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &channels) < 0) {
        snd_pcm_hw_params_free(pHWParams);
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }
	
	int dir = 0;
	if (snd_pcm_hw_params_set_periods_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &fragmentCount, &dir) < 0) {
        snd_pcm_hw_params_free(pHWParams);
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }
	
	// A few properties may have been adjusted so we need to make sure the device object is aware.
	pDevice->channels = channels;
	pDevice->sampleRate = sampleRate;
	pDevice->fragmentCount = fragmentCount;
	
	// According to the ALSA documentation, the value passed to snd_pcm_sw_params_set_avail_min() must be a power
    // of 2 on some hardware. The value passed to this function is the size in frames of a fragment. Thus, to be
    // as robust as possible the size of the hardware buffer should be sized based on the size of the next power-
    // of-two frame count.
	pDevice->fragmentSizeInFrames = mal_next_power_of_2(fragmentSizeInFrames);
	
	if (snd_pcm_hw_params_set_buffer_size((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, pDevice->fragmentSizeInFrames * pDevice->fragmentCount) < 0) {
        snd_pcm_hw_params_free(pHWParams);
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }

    if (snd_pcm_hw_params((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams) < 0) {
        snd_pcm_hw_params_free(pHWParams);
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }

    snd_pcm_hw_params_free(pHWParams);
	
	
	
	snd_pcm_sw_params_t* pSWParams = NULL;
	if (snd_pcm_sw_params_malloc(&pSWParams) < 0) {
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	
	if (snd_pcm_sw_params_current((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams) != 0) {
		snd_pcm_sw_params_free(pSWParams);
        mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }

    if (snd_pcm_sw_params_set_avail_min((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams, pDevice->fragmentSizeInFrames) != 0) {
        snd_pcm_sw_params_free(pSWParams);
        mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }
	
	if (type == mal_device_type_playback) {
	    if (snd_pcm_sw_params_set_start_threshold((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams, pDevice->fragmentSizeInFrames) != 0) {
	        snd_pcm_sw_params_free(pSWParams);
	        mal_device_uninit__alsa(pDevice);
			return MAL_FAILED_TO_INIT_BACKEND;
	    }
	}

    if (snd_pcm_sw_params((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams) != 0) {
        snd_pcm_sw_params_free(pSWParams);
        mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
    }
	
    snd_pcm_sw_params_free(pSWParams);
	
	
	
	// If we're _not_ using mmap we need to use an intermediary buffer.
	if (!(pDevice->flags & MAL_FLAG_ALSA_USING_MMAP)) {
		pDevice->alsa.pIntermediaryBuffer = mal_malloc(pDevice->fragmentSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format));
		if (pDevice->alsa.pIntermediaryBuffer == NULL) {
			mal_device_uninit__alsa(pDevice);
			return MAL_FAILED_TO_INIT_BACKEND;
		}
	}
	
	
	
	if (!mal_semaphore_create(&pDevice->alsa.semaphore, 0)) {
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	pDevice->flags |= MAL_FLAG_ALSA_HAS_SEMAPHORE;
	
	if (!mal_thread_create(&pDevice->alsa.thread, mal_worker_thread__alsa, pDevice)) {
		mal_device_uninit__alsa(pDevice);
		return MAL_FAILED_TO_INIT_BACKEND;
	}
	pDevice->flags |= MAL_FLAG_ALSA_HAS_THREAD;
	
	
	return MAL_SUCCESS;
}

mal_result mal_device_start__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	
	// We don't actually start the device here. We instead signal the semaphore on the worker thread and
	// let that thread do the device preparation.
	pDevice->flags |= MAL_FLAG_STARTED;
	mal_semaphore_release(&pDevice->alsa.semaphore);
	return MAL_SUCCESS;
}

mal_result mal_device_stop__alsa(mal_device* pDevice)
{
	mal_assert(pDevice != NULL);
	
	// The device is not stopped here. We instead mark the device as stopped and signal the semaphore
	// on the worker thread.
	pDevice->flags &= ~MAL_FLAG_STARTED;
	mal_semaphore_release(&pDevice->alsa.semaphore);
	
	// If we are in snd_pcm_writei()/snd_pcm_readi() we won't return from it until it's signaled. It
	// appears from my admittedly limited research and experimentation that we can signal the PCM with
	// snd_pcm_prepare(). If don't do this the thread will still return, but it'll wait for the next
	// fragment to be processed which might take some time depending on the size of the fragment.
	//
	// I'm not sure if this is the proper way to do this so best look into this.
	snd_pcm_prepare(pDevice->alsa.pPCM);
	return MAL_SUCCESS;
}
#endif


// Helper for determining whether or not the given device is initialized.
mal_bool32 mal_device__is_initialized(mal_device* pDevice)
{
	if (pDevice == NULL) return MAL_FALSE;
	return (pDevice->flags & MAL_FLAG_INITIALIZED) != 0;
}


mal_result mal_enumerate_devices(mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
	if (pCount == NULL) return MAL_INVALID_ARGS;
	
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

mal_result mal_device_init(mal_device* pDevice, mal_device_type type, mal_device_id* pDeviceID, mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_uint32 fragmentSizeInFrames, mal_uint32 fragmentCount)
{
	if (pDevice == NULL) return MAL_INVALID_ARGS;
	mal_zero_object(pDevice);
	
	if (channels == 0 || sampleRate == 0 || fragmentSizeInFrames == 0 || fragmentCount == 0) return MAL_INVALID_ARGS;
	
	pDevice->type = type;
	pDevice->format = format;
	pDevice->channels = channels;
	pDevice->sampleRate = sampleRate;
	pDevice->fragmentSizeInFrames = fragmentSizeInFrames;
	pDevice->fragmentCount = fragmentCount;

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
		return MAL_NO_BACKEND;
	}

	pDevice->flags |= MAL_FLAG_INITIALIZED;
	return MAL_SUCCESS;
}

void mal_device_uninit(mal_device* pDevice)
{
	if (!mal_device__is_initialized(pDevice)) return;
	
	// Make sure the device is stopped first. The backends will probably handle this naturally,
	// but I like to do it explicitly for my own sanity.
	mal_device_stop(pDevice);

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
	pDevice->onRecv = proc;
}

void mal_device_set_send_callback(mal_device* pDevice, mal_send_proc proc)
{
	if (pDevice == NULL) return;
	pDevice->onSend = proc;
}

mal_result mal_device_start(mal_device* pDevice)
{
	if (pDevice == NULL) return MAL_INVALID_ARGS;
	if (mal_device_is_started(pDevice)) {
		return MAL_DEVICE_ALREADY_STARTED;
	}
	
	mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_DSOUND
	if (pDevice->api == mal_api_dsound) {
		result = mal_device_start__dsound(pDevice);
	}
#endif

#ifdef MAL_ENABLE_ALSA
	if (pDevice->api == mal_api_alsa) {
		result = mal_device_start__alsa(pDevice);
	}
#endif

#ifdef MAL_ENABLE_NULL
	if (pDevice->api == mal_api_null) {
		result = mal_device_start__null(pDevice);
	}
#endif

	if (result == MAL_SUCCESS) {
		pDevice->flags |= MAL_FLAG_STARTED;
	}

	return result;
}

mal_result mal_device_stop(mal_device* pDevice)
{
	if (pDevice == NULL) return MAL_INVALID_ARGS;
	if (!mal_device_is_started(pDevice)) {
		return MAL_DEVICE_ALREADY_STOPPED;
	}
	
	mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_DSOUND
	if (pDevice->api == mal_api_dsound) {
		result = mal_device_stop__dsound(pDevice);
	}
#endif

#ifdef MAL_ENABLE_ALSA
	if (pDevice->api == mal_api_alsa) {
		result = mal_device_stop__alsa(pDevice);
	}
#endif

#ifdef MAL_ENABLE_NULL
	if (pDevice->api == mal_api_null) {
		result = mal_device_stop__null(pDevice);
	}
#endif

	if (result == MAL_SUCCESS) {
		pDevice->flags &= ~MAL_FLAG_STARTED;
	}
	
	return result;
}

mal_bool32 mal_device_is_started(mal_device* pDevice)
{
	if (pDevice == NULL) return MAL_FALSE;
	return (pDevice->flags & MAL_FLAG_STARTED) != 0;
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
// - Error handling in worker threads isn't quite right. At the top of the main loop, before the semaphore wait, I think
//   the device needs to be unmarked as playing.
// - mal_device_start() and mal_device_stop() need improving:
//   - Need to wait for a synchronization primitive on the worker thread to signal that the operation is complete.
//   - Need a more accurate return code.
// - Make starting and stopping thread-safe
// - More error codes
// - Logging
// - Implement mmap mode for ALSA.
// - Make device initialization more robust for ALSA
//   - Clamp period sizes to their min/max.
// - Support rewinding. This will enable applications to employ better anti-latency.


// DEVELOPMENT NOTES
// =================
//
// ALSA
// ----
// - [DONE] Use snd_pcm_recover() when snd_pcm_writei() or snd_pcm_readi() fails.


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