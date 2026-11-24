/*
TODO:
    - When this is integrated into the main library, all Interlocked*() functions needs to be replaced with miniaudio's regular atomics.
*/

#ifndef miniaudio_backend_xaudio2_c
#define miniaudio_backend_xaudio2_c

#include "miniaudio_xaudio2.h"

/* TODO: Replace with MA_ZERO_MEMORY() when this backend is integrated into miniaudio. */
#include <string.h>
#define MA_XAUDIO2_ZERO_MEMORY(p, sz) memset((p), 0, (sz))
#define MA_XAUDIO2_COPY_MEMORY(dst, src, sz) memcpy((dst), (src), (sz))

#if defined(MA_WIN32) && !defined(MA_XBOX)
    #define MA_SUPPORT_XAUDIO2
#endif

#if defined(MA_SUPPORT_XAUDIO2) && !defined(MA_NO_XAUDIO2) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_XAUDIO2))
    #define MA_HAS_XAUDIO2
#endif

#if defined(MA_HAS_XAUDIO2)
#if defined(MA_XBOX_360)
#include <xtl.h>
#include <xobjbase.h>
#else
#include <windows.h>
#include <objbase.h>
#endif
#include <string.h>

/* TODO: Remove these temporary thread declarations when this backend is integrated into miniaudio. */
#if 1
    #ifndef MA_THREADCALL
        #if defined(MA_THREADING_BACKEND_WIN32)
            #define MA_THREADCALL WINAPI
            typedef unsigned long ma_thread_result;
        #else
            #define MA_THREADCALL
            typedef void* ma_thread_result;
        #endif

        typedef ma_thread_result (MA_THREADCALL * ma_thread_entry_proc)(void* pData);
    #endif

    MA_API ma_result ma_thread_create(ma_thread* pThread, ma_thread_priority priority, size_t stackSize, ma_thread_entry_proc entryProc, void* pData, const ma_allocation_callbacks* pAllocationCallbacks);
    MA_API void ma_thread_wait(ma_thread* pThread);
#endif

static const IID MA_CLSID_XAudio2_27            = {0x5A508685, 0xA254, 0x4FBA, {0x9B, 0x82, 0x9A, 0x24, 0xB0, 0x03, 0x06, 0xAF}};
static const IID MA_IID_IClassFactory           = {0x00000001, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID MA_IID_IXAudio2_27             = {0x8BCF1F58, 0x9FE7, 0x4583, {0x8A, 0xC6, 0xE2, 0xAD, 0xC4, 0x65, 0xC8, 0xBB}};
static const IID MA_IID_IXAudio2_28             = {0x60D8DAC8, 0x5AA1, 0x4E8E, {0xB5, 0x97, 0x2F, 0x5E, 0x28, 0x83, 0xD4, 0x84}};
static const IID MA_IID_IXAudio2_29             = {0x2B02E3CF, 0x2E0B, 0x4EC3, {0xBE, 0x45, 0x1B, 0x2A, 0x3F, 0xE7, 0x21, 0x0D}};
static const IID MA_IID_IXAudio2Extension       = {0x84AC29BB, 0xD619, 0x44D2, {0xB1, 0x97, 0xE4, 0xAC, 0xF7, 0xDF, 0x3E, 0xD6}};

static const GUID MA_XAUDIO2_SUBTYPE_PCM        = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};
static const GUID MA_XAUDIO2_SUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

#if !defined(MA_XBOX_360)
typedef struct
{
    GUID fmtid;
    DWORD pid;
} MA_XAUDIO2_PROPERTYKEY;

typedef struct
{
    WORD vt;
    WORD wReserved1;
    WORD wReserved2;
    WORD wReserved3;
    union
    {
        struct
        {
            ULONG cbSize;
            BYTE* pBlobData;
        } blob;
        WCHAR* pwszVal;
        char pad[16];
    } data;
} MA_XAUDIO2_PROPVARIANT;

static const IID MA_XAUDIO2_CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const IID MA_XAUDIO2_IID_IMMDeviceEnumerator   = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};

static const MA_XAUDIO2_PROPERTYKEY MA_XAUDIO2_PKEY_Device_FriendlyName      = {{0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 14};
static const MA_XAUDIO2_PROPERTYKEY MA_XAUDIO2_PKEY_AudioEngine_DeviceFormat = {{0xF19F064D, 0x082C, 0x4E27, {0xBC, 0x73, 0x68, 0x82, 0xA1, 0xBB, 0x8E, 0x4C}},  0};

#define MA_XAUDIO2_MM_DEVICE_STATE_ACTIVE 1
#define MA_XAUDIO2_VT_LPWSTR              31
#define MA_XAUDIO2_VT_BLOB                65

typedef enum
{
    ma_eRender_xaudio2  = 0,
    ma_eCapture_xaudio2 = 1,
    ma_eAll_xaudio2     = 2
} ma_EDataFlow_xaudio2;

typedef enum
{
    ma_eConsole_xaudio2        = 0,
    ma_eMultimedia_xaudio2     = 1,
    ma_eCommunications_xaudio2 = 2
} ma_ERole_xaudio2;

typedef struct ma_IMMDeviceEnumerator_xaudio2   ma_IMMDeviceEnumerator_xaudio2;
typedef struct ma_IMMDeviceCollection_xaudio2   ma_IMMDeviceCollection_xaudio2;
typedef struct ma_IMMDevice_xaudio2             ma_IMMDevice_xaudio2;
typedef struct ma_IMMNotificationClient_xaudio2 ma_IMMNotificationClient_xaudio2;
typedef struct ma_IPropertyStore_xaudio2        ma_IPropertyStore_xaudio2;


typedef struct
{
    HRESULT (STDMETHODCALLTYPE * QueryInterface)(ma_IMMDeviceEnumerator_xaudio2* pThis, const IID* const riid, void** ppObject);
    ULONG   (STDMETHODCALLTYPE * AddRef)(ma_IMMDeviceEnumerator_xaudio2* pThis);
    ULONG   (STDMETHODCALLTYPE * Release)(ma_IMMDeviceEnumerator_xaudio2* pThis);
    HRESULT (STDMETHODCALLTYPE * EnumAudioEndpoints)(ma_IMMDeviceEnumerator_xaudio2* pThis, ma_EDataFlow_xaudio2 dataFlow, DWORD dwStateMask, ma_IMMDeviceCollection_xaudio2** ppDevices);
    HRESULT (STDMETHODCALLTYPE * GetDefaultAudioEndpoint)(ma_IMMDeviceEnumerator_xaudio2* pThis, ma_EDataFlow_xaudio2 dataFlow, ma_ERole_xaudio2 role, ma_IMMDevice_xaudio2** ppEndpoint);
    HRESULT (STDMETHODCALLTYPE * GetDevice)(ma_IMMDeviceEnumerator_xaudio2* pThis, const WCHAR* pID, ma_IMMDevice_xaudio2** ppDevice);
    HRESULT (STDMETHODCALLTYPE * RegisterEndpointNotificationCallback)(ma_IMMDeviceEnumerator_xaudio2* pThis, ma_IMMNotificationClient_xaudio2* pClient);
    HRESULT (STDMETHODCALLTYPE * UnregisterEndpointNotificationCallback)(ma_IMMDeviceEnumerator_xaudio2* pThis, ma_IMMNotificationClient_xaudio2* pClient);
} ma_IMMDeviceEnumeratorVtbl_xaudio2;

struct ma_IMMDeviceEnumerator_xaudio2
{
    ma_IMMDeviceEnumeratorVtbl_xaudio2* lpVtbl;
};

static MA_INLINE HRESULT ma_IMMDeviceEnumerator_QueryInterface__xaudio2(ma_IMMDeviceEnumerator_xaudio2* pThis, const IID* const riid, void** ppObject) { return pThis->lpVtbl->QueryInterface(pThis, riid, ppObject); }
static MA_INLINE ULONG   ma_IMMDeviceEnumerator_AddRef__xaudio2(ma_IMMDeviceEnumerator_xaudio2* pThis){ return pThis->lpVtbl->AddRef(pThis); }
static MA_INLINE ULONG   ma_IMMDeviceEnumerator_Release__xaudio2(ma_IMMDeviceEnumerator_xaudio2* pThis){ return pThis->lpVtbl->Release(pThis); }
static MA_INLINE HRESULT ma_IMMDeviceEnumerator_EnumAudioEndpoints__xaudio2(ma_IMMDeviceEnumerator_xaudio2* pThis, ma_EDataFlow_xaudio2 dataFlow, DWORD dwStateMask, ma_IMMDeviceCollection_xaudio2** ppDevices) { return pThis->lpVtbl->EnumAudioEndpoints(pThis, dataFlow, dwStateMask, ppDevices); }
static MA_INLINE HRESULT ma_IMMDeviceEnumerator_GetDefaultAudioEndpoint__xaudio2(ma_IMMDeviceEnumerator_xaudio2* pThis, ma_EDataFlow_xaudio2 dataFlow, ma_ERole_xaudio2 role, ma_IMMDevice_xaudio2** ppEndpoint) { return pThis->lpVtbl->GetDefaultAudioEndpoint(pThis, dataFlow, role, ppEndpoint); }
static MA_INLINE HRESULT ma_IMMDeviceEnumerator_GetDevice__xaudio2(ma_IMMDeviceEnumerator_xaudio2* pThis, const WCHAR* pID, ma_IMMDevice_xaudio2** ppDevice) { return pThis->lpVtbl->GetDevice(pThis, pID, ppDevice); }
static MA_INLINE HRESULT ma_IMMDeviceEnumerator_RegisterEndpointNotificationCallback__xaudio2(ma_IMMDeviceEnumerator_xaudio2* pThis, ma_IMMNotificationClient_xaudio2* pClient) { return pThis->lpVtbl->RegisterEndpointNotificationCallback(pThis, pClient); }
static MA_INLINE HRESULT ma_IMMDeviceEnumerator_UnregisterEndpointNotificationCallback__xaudio2(ma_IMMDeviceEnumerator_xaudio2* pThis, ma_IMMNotificationClient_xaudio2* pClient) { return pThis->lpVtbl->UnregisterEndpointNotificationCallback(pThis, pClient); }


typedef struct
{
    HRESULT (STDMETHODCALLTYPE * QueryInterface)(ma_IMMDeviceCollection_xaudio2* pThis, const IID* const riid, void** ppObject);
    ULONG   (STDMETHODCALLTYPE * AddRef)(ma_IMMDeviceCollection_xaudio2* pThis);
    ULONG   (STDMETHODCALLTYPE * Release)(ma_IMMDeviceCollection_xaudio2* pThis);
    HRESULT (STDMETHODCALLTYPE * GetCount)(ma_IMMDeviceCollection_xaudio2* pThis, UINT* pDevices);
    HRESULT (STDMETHODCALLTYPE * Item)(ma_IMMDeviceCollection_xaudio2* pThis, UINT nDevice, ma_IMMDevice_xaudio2** ppDevice);
} ma_IMMDeviceCollectionVtbl_xaudio2;

struct ma_IMMDeviceCollection_xaudio2
{
    ma_IMMDeviceCollectionVtbl_xaudio2* lpVtbl;
};

static MA_INLINE HRESULT ma_IMMDeviceCollection_QueryInterface__xaudio2(ma_IMMDeviceCollection_xaudio2* pThis, const IID* const riid, void** ppObject) { return pThis->lpVtbl->QueryInterface(pThis, riid, ppObject); }
static MA_INLINE ULONG   ma_IMMDeviceCollection_AddRef__xaudio2(ma_IMMDeviceCollection_xaudio2* pThis){ return pThis->lpVtbl->AddRef(pThis); }
static MA_INLINE ULONG   ma_IMMDeviceCollection_Release__xaudio2(ma_IMMDeviceCollection_xaudio2* pThis){ return pThis->lpVtbl->Release(pThis); }
static MA_INLINE HRESULT ma_IMMDeviceCollection_GetCount__xaudio2(ma_IMMDeviceCollection_xaudio2* pThis, UINT* pDevices){ return pThis->lpVtbl->GetCount(pThis, pDevices); }
static MA_INLINE HRESULT ma_IMMDeviceCollection_Item__xaudio2(ma_IMMDeviceCollection_xaudio2* pThis, UINT nDevice, ma_IMMDevice_xaudio2** ppDevice){ return pThis->lpVtbl->Item(pThis, nDevice, ppDevice); }


typedef struct
{
    HRESULT (STDMETHODCALLTYPE * QueryInterface)(ma_IMMDevice_xaudio2* pThis, const IID* const riid, void** ppObject);
    ULONG   (STDMETHODCALLTYPE * AddRef)(ma_IMMDevice_xaudio2* pThis);
    ULONG   (STDMETHODCALLTYPE * Release)(ma_IMMDevice_xaudio2* pThis);
    HRESULT (STDMETHODCALLTYPE * Activate)(ma_IMMDevice_xaudio2* pThis, const IID* const iid, DWORD dwClsCtx, MA_XAUDIO2_PROPVARIANT* pActivationParams, void** ppInterface);
    HRESULT (STDMETHODCALLTYPE * OpenPropertyStore)(ma_IMMDevice_xaudio2* pThis, DWORD stgmAccess, ma_IPropertyStore_xaudio2** ppProperties);
    HRESULT (STDMETHODCALLTYPE * GetId)(ma_IMMDevice_xaudio2* pThis, WCHAR** pID);
    HRESULT (STDMETHODCALLTYPE * GetState)(ma_IMMDevice_xaudio2* pThis, DWORD *pState);
} ma_IMMDeviceVtbl_xaudio2;

struct ma_IMMDevice_xaudio2
{
    ma_IMMDeviceVtbl_xaudio2* lpVtbl;
};

static MA_INLINE HRESULT ma_IMMDevice_QueryInterface__xaudio2(ma_IMMDevice_xaudio2* pThis, const IID* const riid, void** ppObject) { return pThis->lpVtbl->QueryInterface(pThis, riid, ppObject); }
static MA_INLINE ULONG   ma_IMMDevice_AddRef__xaudio2(ma_IMMDevice_xaudio2* pThis) { return pThis->lpVtbl->AddRef(pThis); }
static MA_INLINE ULONG   ma_IMMDevice_Release__xaudio2(ma_IMMDevice_xaudio2* pThis) { return pThis->lpVtbl->Release(pThis); }
static MA_INLINE HRESULT ma_IMMDevice_Activate__xaudio2(ma_IMMDevice_xaudio2* pThis, const IID* const iid, DWORD dwClsCtx, MA_XAUDIO2_PROPVARIANT* pActivationParams, void** ppInterface) { return pThis->lpVtbl->Activate(pThis, iid, dwClsCtx, pActivationParams, ppInterface); }
static MA_INLINE HRESULT ma_IMMDevice_OpenPropertyStore__xaudio2(ma_IMMDevice_xaudio2* pThis, DWORD stgmAccess, ma_IPropertyStore_xaudio2** ppProperties) { return pThis->lpVtbl->OpenPropertyStore(pThis, stgmAccess, ppProperties); }
static MA_INLINE HRESULT ma_IMMDevice_GetId__xaudio2(ma_IMMDevice_xaudio2* pThis, WCHAR** pID) { return pThis->lpVtbl->GetId(pThis, pID); }
static MA_INLINE HRESULT ma_IMMDevice_GetState__xaudio2(ma_IMMDevice_xaudio2* pThis, DWORD *pState) { return pThis->lpVtbl->GetState(pThis, pState); }


typedef struct
{
    HRESULT (STDMETHODCALLTYPE * QueryInterface)(ma_IPropertyStore_xaudio2* pThis, const IID* const riid, void** ppObject);
    ULONG   (STDMETHODCALLTYPE * AddRef)(ma_IPropertyStore_xaudio2* pThis);
    ULONG   (STDMETHODCALLTYPE * Release)(ma_IPropertyStore_xaudio2* pThis);
    HRESULT (STDMETHODCALLTYPE * GetCount)(ma_IPropertyStore_xaudio2* pThis, DWORD* pPropCount);
    HRESULT (STDMETHODCALLTYPE * GetAt)(ma_IPropertyStore_xaudio2* pThis, DWORD propIndex, MA_XAUDIO2_PROPERTYKEY* pPropKey);
    HRESULT (STDMETHODCALLTYPE * GetValue)(ma_IPropertyStore_xaudio2* pThis, const MA_XAUDIO2_PROPERTYKEY* const pKey, MA_XAUDIO2_PROPVARIANT* pPropVar);
    HRESULT (STDMETHODCALLTYPE * SetValue)(ma_IPropertyStore_xaudio2* pThis, const MA_XAUDIO2_PROPERTYKEY* const pKey, const MA_XAUDIO2_PROPVARIANT* const pPropVar);
    HRESULT (STDMETHODCALLTYPE * Commit)(ma_IPropertyStore_xaudio2* pThis);
} ma_IPropertyStoreVtbl_xaudio2;

struct ma_IPropertyStore_xaudio2
{
    ma_IPropertyStoreVtbl_xaudio2* lpVtbl;
};

static MA_INLINE HRESULT ma_IPropertyStore_QueryInterface__xaudio2(ma_IPropertyStore_xaudio2* pThis, const IID* const riid, void** ppObject) { return pThis->lpVtbl->QueryInterface(pThis, riid, ppObject); }
static MA_INLINE ULONG   ma_IPropertyStore_AddRef__xaudio2(ma_IPropertyStore_xaudio2* pThis) { return pThis->lpVtbl->AddRef(pThis); }
static MA_INLINE ULONG   ma_IPropertyStore_Release__xaudio2(ma_IPropertyStore_xaudio2* pThis) { return pThis->lpVtbl->Release(pThis); }
static MA_INLINE HRESULT ma_IPropertyStore_GetCount__xaudio2(ma_IPropertyStore_xaudio2* pThis, DWORD* pPropCount) { return pThis->lpVtbl->GetCount(pThis, pPropCount); }
static MA_INLINE HRESULT ma_IPropertyStore_GetAt__xaudio2(ma_IPropertyStore_xaudio2* pThis, DWORD propIndex, MA_XAUDIO2_PROPERTYKEY* pPropKey) { return pThis->lpVtbl->GetAt(pThis, propIndex, pPropKey); }
static MA_INLINE HRESULT ma_IPropertyStore_GetValue__xaudio2(ma_IPropertyStore_xaudio2* pThis, const MA_XAUDIO2_PROPERTYKEY* const pKey, MA_XAUDIO2_PROPVARIANT* pPropVar) { return pThis->lpVtbl->GetValue(pThis, pKey, pPropVar); }
static MA_INLINE HRESULT ma_IPropertyStore_SetValue__xaudio2(ma_IPropertyStore_xaudio2* pThis, const MA_XAUDIO2_PROPERTYKEY* const pKey, const MA_XAUDIO2_PROPVARIANT* const pPropVar) { return pThis->lpVtbl->SetValue(pThis, pKey, pPropVar); }
static MA_INLINE HRESULT ma_IPropertyStore_Commit__xaudio2(ma_IPropertyStore_xaudio2* pThis) { return pThis->lpVtbl->Commit(pThis); }
#endif

#if defined(MA_XBOX_360)
#define MA_XAUDIO2_DEFAULT_PROCESSOR            0x00000010
#else
#define MA_XAUDIO2_DEFAULT_PROCESSOR            0xFFFFFFFF
#endif

#define MA_XAUDIO2_DEFAULT_GAME_DEVICE          0x00000008
#define MA_XAUDIO2_PROCESSOR_1                  0x00000001
#define MA_XAUDIO2_VOICE_NOPITCH                0x00000002
#define MA_XAUDIO2_VOICE_NOSAMPLESPLAYED        0x00000100
#define MA_XAUDIO2_1024_QUANTUM                 0x00008000
#define MA_XAUDIO2_MIN_SAMPLE_RATE              1000
#define MA_XAUDIO2_MAX_SAMPLE_RATE              200000
#define MA_XAUDIO2_MAX_QUEUED_BUFFERS           64
#define MA_XAUDIO2_AUDIO_CATEGORY_GAME_EFFECTS  6

typedef struct
{
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
    WORD wValidBitsPerSample;
    DWORD dwChannelMask;
    GUID SubFormat;
} MA_XAUDIO2_WAVEFORMATEXTENSIBLE;

typedef struct
{
    WCHAR DeviceID[256];
    WCHAR DisplayName[256];
    ma_uint32 Role;
    MA_XAUDIO2_WAVEFORMATEXTENSIBLE OutputFormat;
} MA_XAUDIO2_DEVICE_DETAILS;

typedef struct
{
    ma_uint32 CreationFlags;
    ma_uint32 InputChannels;
    ma_uint32 InputSampleRate;
} MA_XAUDIO2_VOICE_DETAILS_27;

typedef struct
{
    ma_uint32 CreationFlags;
    ma_uint32 ActiveFlags;
    ma_uint32 InputChannels;
    ma_uint32 InputSampleRate;
} MA_XAUDIO2_VOICE_DETAILS;

typedef struct
{
    void* pCurrentBufferContext;
    ma_uint32 BuffersQueued;
    ma_uint8 SamplesPlayed[8];  /* Microsoft declares their XAudio2 structs with tight packing, but I want to maximize compiler compatibility. Normally this is typed as `ma_uint64 SamplesPlayed`, but that would break the tight packing. Instead we're using a byte array. We're not using this field so shouldn't be a big deal for us. */
} MA_XAUDIO2_VOICE_STATE;

typedef struct
{
    ma_uint32 Flags;
    ma_uint32 AudioBytes;
    const ma_uint8* pAudioData;
    ma_uint32 PlayBegin;
    ma_uint32 PlayLength;
    ma_uint32 LoopBegin;
    ma_uint32 LoopLength;
    ma_uint32 LoopCount;
    ma_uint8 pContext[sizeof(void*)];
} MA_XAUDIO2_BUFFER;

typedef struct MA_XAUDIO2_VOICE_SENDS       MA_XAUDIO2_VOICE_SENDS;
typedef struct MA_XAUDIO2_EFFECT_CHAIN      MA_XAUDIO2_EFFECT_CHAIN;
typedef struct MA_XAUDIO2_FILTER_PARAMETERS MA_XAUDIO2_FILTER_PARAMETERS;
typedef struct MA_XAUDIO2_BUFFER_WMA        MA_XAUDIO2_BUFFER_WMA;

typedef struct ma_IXAudio2EngineCallback    ma_IXAudio2EngineCallback;
typedef struct ma_IXAudio2VoiceCallback     ma_IXAudio2VoiceCallback;

typedef struct ma_context_state_xaudio2     ma_context_state_xaudio2;
typedef struct ma_device_state_xaudio2      ma_device_state_xaudio2;

typedef struct
{
    void (STDMETHODCALLTYPE * OnProcessingPassStart)(ma_IXAudio2EngineCallback* pThis);
    void (STDMETHODCALLTYPE * OnProcessingPassEnd)  (ma_IXAudio2EngineCallback* pThis);
    void (STDMETHODCALLTYPE * OnCriticalError)      (ma_IXAudio2EngineCallback* pThis, HRESULT error);
} ma_IXAudio2EngineCallback_Vtbl;

struct ma_IXAudio2EngineCallback
{
    ma_IXAudio2EngineCallback_Vtbl* lpVtbl;
    ma_device_state_xaudio2* pDeviceState;
};

typedef struct
{
    void (STDMETHODCALLTYPE * OnVoiceProcessingPassStart)(ma_IXAudio2VoiceCallback* pThis, ma_uint32 bytesRequired);
    void (STDMETHODCALLTYPE * OnVoiceProcessingPassEnd)  (ma_IXAudio2VoiceCallback* pThis);
    void (STDMETHODCALLTYPE * OnStreamEnd)               (ma_IXAudio2VoiceCallback* pThis);
    void (STDMETHODCALLTYPE * OnBufferStart)             (ma_IXAudio2VoiceCallback* pThis, void* pBufferContext);
    void (STDMETHODCALLTYPE * OnBufferEnd)               (ma_IXAudio2VoiceCallback* pThis, void* pBufferContext);
    void (STDMETHODCALLTYPE * OnLoopEnd)                 (ma_IXAudio2VoiceCallback* pThis, void* pBufferContext);
    void (STDMETHODCALLTYPE * OnVoiceError)              (ma_IXAudio2VoiceCallback* pThis, void* pBufferContext, HRESULT error);
} ma_IXAudio2VoiceCallback_Vtbl;

struct ma_IXAudio2VoiceCallback
{
    ma_IXAudio2VoiceCallback_Vtbl* lpVtbl;
    ma_device_state_xaudio2* pDeviceState;
};

typedef struct ma_IXAudio2Voice_27 ma_IXAudio2Voice_27;
typedef struct
{
    void    (STDMETHODCALLTYPE * GetVoiceDetails)          (ma_IXAudio2Voice_27* pThis, MA_XAUDIO2_VOICE_DETAILS_27* pDetails);
    HRESULT (STDMETHODCALLTYPE * SetOutputVoices)          (ma_IXAudio2Voice_27* pThis, const MA_XAUDIO2_VOICE_SENDS* pSendList);
    HRESULT (STDMETHODCALLTYPE * SetEffectChain)           (ma_IXAudio2Voice_27* pThis, const MA_XAUDIO2_EFFECT_CHAIN* pEffectChain);
    HRESULT (STDMETHODCALLTYPE * EnableEffect)             (ma_IXAudio2Voice_27* pThis, ma_uint32 effectIndex, ma_uint32 operationSet);
    HRESULT (STDMETHODCALLTYPE * DisableEffect)            (ma_IXAudio2Voice_27* pThis, ma_uint32 effectIndex, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetEffectState)           (ma_IXAudio2Voice_27* pThis, ma_uint32 effectIndex, BOOL* pEnabled);
    HRESULT (STDMETHODCALLTYPE * SetEffectParameters)      (ma_IXAudio2Voice_27* pThis, ma_uint32 effectIndex, const void* pParameters, ma_uint32 parametersByteSize, ma_uint32 operationSet);
    HRESULT (STDMETHODCALLTYPE * GetEffectParameters)      (ma_IXAudio2Voice_27* pThis, ma_uint32 effectIndex, void* pParameters, ma_uint32 parametersByteSize);
    HRESULT (STDMETHODCALLTYPE * SetFilterParameters)      (ma_IXAudio2Voice_27* pThis, const MA_XAUDIO2_FILTER_PARAMETERS* pParameters, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetFilterParameters)      (ma_IXAudio2Voice_27* pThis, MA_XAUDIO2_FILTER_PARAMETERS* pParameters);
    HRESULT (STDMETHODCALLTYPE * SetOutputFilterParameters)(ma_IXAudio2Voice_27* pThis, ma_IXAudio2Voice_27* pDestinationVoice, const MA_XAUDIO2_FILTER_PARAMETERS* pParameters, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetOutputFilterParameters)(ma_IXAudio2Voice_27* pThis, ma_IXAudio2Voice_27* pDestinationVoice, MA_XAUDIO2_FILTER_PARAMETERS* pParameters);
    HRESULT (STDMETHODCALLTYPE * SetVolume)                (ma_IXAudio2Voice_27* pThis, float volume, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetVolume)                (ma_IXAudio2Voice_27* pThis, float* pVolume);
    HRESULT (STDMETHODCALLTYPE * SetChannelVolumes)        (ma_IXAudio2Voice_27* pThis, ma_uint32 channels, const float* pVolumes, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetChannelVolumes)        (ma_IXAudio2Voice_27* pThis, ma_uint32 channels, float* pVolumes);
    HRESULT (STDMETHODCALLTYPE * SetOutputMatrix)          (ma_IXAudio2Voice_27* pThis, ma_IXAudio2Voice_27* pDestinationVoice, ma_uint32 sourceChannels, ma_uint32 destinationChannels, const float* pLevelMatrix, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetOutputMatrix)          (ma_IXAudio2Voice_27* pThis, ma_IXAudio2Voice_27* pDestinationVoice, ma_uint32 sourceChannels, ma_uint32 destinationChannels, float* pLevelMatrix);
    void    (STDMETHODCALLTYPE * DestroyVoice)             (ma_IXAudio2Voice_27* pThis);
} ma_IXAudio2Voice_27_Vtbl;

struct ma_IXAudio2Voice_27
{
    ma_IXAudio2Voice_27_Vtbl* lpVtbl;
};

typedef struct
{
    ma_IXAudio2Voice_27_Vtbl base;
    HRESULT (STDMETHODCALLTYPE * Start)              (ma_IXAudio2Voice_27* pThis, ma_uint32 flags, ma_uint32 operationSet);
    HRESULT (STDMETHODCALLTYPE * Stop)               (ma_IXAudio2Voice_27* pThis, ma_uint32 flags, ma_uint32 operationSet);
    HRESULT (STDMETHODCALLTYPE * SubmitSourceBuffer) (ma_IXAudio2Voice_27* pThis, const MA_XAUDIO2_BUFFER* pBuffer, const MA_XAUDIO2_BUFFER_WMA* pBufferWMA);
    HRESULT (STDMETHODCALLTYPE * FlushSourceBuffers) (ma_IXAudio2Voice_27* pThis);
    HRESULT (STDMETHODCALLTYPE * Discontinuity)      (ma_IXAudio2Voice_27* pThis);
    HRESULT (STDMETHODCALLTYPE * ExitLoop)           (ma_IXAudio2Voice_27* pThis, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetState)           (ma_IXAudio2Voice_27* pThis, MA_XAUDIO2_VOICE_STATE* pVoiceState);
    HRESULT (STDMETHODCALLTYPE * SetFrequencyRatio)  (ma_IXAudio2Voice_27* pThis, float ratio, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetFrequencyRatio)  (ma_IXAudio2Voice_27* pThis, float* pRatio);
    HRESULT (STDMETHODCALLTYPE * SetSourceSampleRate)(ma_IXAudio2Voice_27* pThis, ma_uint32 newSourceSampleRate);
} ma_IXAudio2SourceVoice_27_Vtbl;

typedef struct ma_IXAudio2Voice ma_IXAudio2Voice;
typedef struct
{
    void    (STDMETHODCALLTYPE * GetVoiceDetails)          (ma_IXAudio2Voice* pThis, MA_XAUDIO2_VOICE_DETAILS* pDetails);
    HRESULT (STDMETHODCALLTYPE * SetOutputVoices)          (ma_IXAudio2Voice* pThis, const MA_XAUDIO2_VOICE_SENDS* pSendList);
    HRESULT (STDMETHODCALLTYPE * SetEffectChain)           (ma_IXAudio2Voice* pThis, const MA_XAUDIO2_EFFECT_CHAIN* pEffectChain);
    HRESULT (STDMETHODCALLTYPE * EnableEffect)             (ma_IXAudio2Voice* pThis, ma_uint32 effectIndex, ma_uint32 operationSet);
    HRESULT (STDMETHODCALLTYPE * DisableEffect)            (ma_IXAudio2Voice* pThis, ma_uint32 effectIndex, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetEffectState)           (ma_IXAudio2Voice* pThis, ma_uint32 effectIndex, BOOL* pEnabled);
    HRESULT (STDMETHODCALLTYPE * SetEffectParameters)      (ma_IXAudio2Voice* pThis, ma_uint32 effectIndex, const void* pParameters, ma_uint32 parametersByteSize, ma_uint32 operationSet);
    HRESULT (STDMETHODCALLTYPE * GetEffectParameters)      (ma_IXAudio2Voice* pThis, ma_uint32 effectIndex, void* pParameters, ma_uint32 parametersByteSize);
    HRESULT (STDMETHODCALLTYPE * SetFilterParameters)      (ma_IXAudio2Voice* pThis, const MA_XAUDIO2_FILTER_PARAMETERS* pParameters, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetFilterParameters)      (ma_IXAudio2Voice* pThis, MA_XAUDIO2_FILTER_PARAMETERS* pParameters);
    HRESULT (STDMETHODCALLTYPE * SetOutputFilterParameters)(ma_IXAudio2Voice* pThis, ma_IXAudio2Voice* pDestinationVoice, const MA_XAUDIO2_FILTER_PARAMETERS* pParameters, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetOutputFilterParameters)(ma_IXAudio2Voice* pThis, ma_IXAudio2Voice* pDestinationVoice, MA_XAUDIO2_FILTER_PARAMETERS* pParameters);
    HRESULT (STDMETHODCALLTYPE * SetVolume)                (ma_IXAudio2Voice* pThis, float volume, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetVolume)                (ma_IXAudio2Voice* pThis, float* pVolume);
    HRESULT (STDMETHODCALLTYPE * SetChannelVolumes)        (ma_IXAudio2Voice* pThis, ma_uint32 channels, const float* pVolumes, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetChannelVolumes)        (ma_IXAudio2Voice* pThis, ma_uint32 channels, float* pVolumes);
    HRESULT (STDMETHODCALLTYPE * SetOutputMatrix)          (ma_IXAudio2Voice* pThis, ma_IXAudio2Voice* pDestinationVoice, ma_uint32 sourceChannels, ma_uint32 destinationChannels, const float* pLevelMatrix, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetOutputMatrix)          (ma_IXAudio2Voice* pThis, ma_IXAudio2Voice* pDestinationVoice, ma_uint32 sourceChannels, ma_uint32 destinationChannels, float* pLevelMatrix);
    void    (STDMETHODCALLTYPE * DestroyVoice)             (ma_IXAudio2Voice* pThis);
} ma_IXAudio2Voice_Vtbl;

struct ma_IXAudio2Voice
{
    ma_IXAudio2Voice_Vtbl* lpVtbl;
};

typedef struct
{
    ma_IXAudio2Voice_Vtbl base;
    HRESULT (STDMETHODCALLTYPE * Start)              (ma_IXAudio2Voice* pThis, ma_uint32 flags, ma_uint32 operationSet);
    HRESULT (STDMETHODCALLTYPE * Stop)               (ma_IXAudio2Voice* pThis, ma_uint32 flags, ma_uint32 operationSet);
    HRESULT (STDMETHODCALLTYPE * SubmitSourceBuffer) (ma_IXAudio2Voice* pThis, const MA_XAUDIO2_BUFFER* pBuffer, const MA_XAUDIO2_BUFFER_WMA* pBufferWMA);
    HRESULT (STDMETHODCALLTYPE * FlushSourceBuffers) (ma_IXAudio2Voice* pThis);
    HRESULT (STDMETHODCALLTYPE * Discontinuity)      (ma_IXAudio2Voice* pThis);
    HRESULT (STDMETHODCALLTYPE * ExitLoop)           (ma_IXAudio2Voice* pThis, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetState)           (ma_IXAudio2Voice* pThis, MA_XAUDIO2_VOICE_STATE* pVoiceState, ma_uint32 flags);
    HRESULT (STDMETHODCALLTYPE * SetFrequencyRatio)  (ma_IXAudio2Voice* pThis, float ratio, ma_uint32 operationSet);
    void    (STDMETHODCALLTYPE * GetFrequencyRatio)  (ma_IXAudio2Voice* pThis, float* pRatio);
    HRESULT (STDMETHODCALLTYPE * SetSourceSampleRate)(ma_IXAudio2Voice* pThis, ma_uint32 newSourceSampleRate);
} ma_IXAudio2SourceVoice_Vtbl;

typedef struct ma_IClassFactory ma_IClassFactory;
typedef struct
{
    HRESULT (STDMETHODCALLTYPE * QueryInterface)(ma_IClassFactory* pThis, const IID* riid, void** ppObject);
    ULONG   (STDMETHODCALLTYPE * AddRef)        (ma_IClassFactory* pThis);
    ULONG   (STDMETHODCALLTYPE * Release)       (ma_IClassFactory* pThis);
    HRESULT (STDMETHODCALLTYPE * CreateInstance)(ma_IClassFactory* pThis, void* pUnkOuter, const IID* riid, void** ppObject);
    HRESULT (STDMETHODCALLTYPE * LockServer)    (ma_IClassFactory* pThis, BOOL lock);
} ma_IClassFactory_Vtbl;

struct ma_IClassFactory
{
    ma_IClassFactory_Vtbl* lpVtbl;
};

static MA_INLINE ULONG ma_IClassFactory_Release(ma_IClassFactory* pThis) { return pThis->lpVtbl->Release(pThis); }
static MA_INLINE HRESULT ma_IClassFactory_CreateInstance(ma_IClassFactory* pThis, void* pUnkOuter, const IID* riid, void** ppObject) { return pThis->lpVtbl->CreateInstance(pThis, pUnkOuter, riid, ppObject); }

typedef struct ma_IXAudio2_27 ma_IXAudio2_27;
typedef struct
{
    HRESULT (STDMETHODCALLTYPE * QueryInterface)        (ma_IXAudio2_27* pThis, const IID* riid, void** ppObject);
    ULONG   (STDMETHODCALLTYPE * AddRef)                (ma_IXAudio2_27* pThis);
    ULONG   (STDMETHODCALLTYPE * Release)               (ma_IXAudio2_27* pThis);
    HRESULT (STDMETHODCALLTYPE * GetDeviceCount)        (ma_IXAudio2_27* pThis, ma_uint32* pCount);
    HRESULT (STDMETHODCALLTYPE * GetDeviceDetails)      (ma_IXAudio2_27* pThis, ma_uint32 index, MA_XAUDIO2_DEVICE_DETAILS* pDetails);
    HRESULT (STDMETHODCALLTYPE * Initialize)            (ma_IXAudio2_27* pThis, ma_uint32 flags, ma_uint32 processor);
    HRESULT (STDMETHODCALLTYPE * RegisterForCallbacks)  (ma_IXAudio2_27* pThis, ma_IXAudio2EngineCallback* pCallback);
    void    (STDMETHODCALLTYPE * UnregisterForCallbacks)(ma_IXAudio2_27* pThis, ma_IXAudio2EngineCallback* pCallback);
    HRESULT (STDMETHODCALLTYPE * CreateSourceVoice)     (ma_IXAudio2_27* pThis, ma_IXAudio2Voice_27** ppSourceVoice, const MA_XAUDIO2_WAVEFORMATEXTENSIBLE* pSourceFormat, ma_uint32 flags, float maxFrequencyRatio, ma_IXAudio2VoiceCallback* pCallback, const MA_XAUDIO2_VOICE_SENDS* pSendList, const MA_XAUDIO2_EFFECT_CHAIN* pEffectChain);
    HRESULT (STDMETHODCALLTYPE * CreateSubmixVoice)     (ma_IXAudio2_27* pThis, ma_IXAudio2Voice_27** ppSubmixVoice, ma_uint32 inputChannels, ma_uint32 inputSampleRate, ma_uint32 flags, ma_uint32 processingStage, const MA_XAUDIO2_VOICE_SENDS* pSendList, const MA_XAUDIO2_EFFECT_CHAIN* pEffectChain);
    HRESULT (STDMETHODCALLTYPE * CreateMasteringVoice)  (ma_IXAudio2_27* pThis, ma_IXAudio2Voice_27** ppMasteringVoice, ma_uint32 inputChannels, ma_uint32 inputSampleRate, ma_uint32 flags, ma_uint32 deviceIndex, const MA_XAUDIO2_EFFECT_CHAIN* pEffectChain);
} ma_IXAudio2_27_Vtbl;

struct ma_IXAudio2_27
{
    ma_IXAudio2_27_Vtbl* lpVtbl;
};

typedef struct ma_IXAudio2 ma_IXAudio2;
typedef struct
{
    HRESULT (STDMETHODCALLTYPE * QueryInterface)        (ma_IXAudio2* pThis, const IID* riid, void** ppObject);
    ULONG   (STDMETHODCALLTYPE * AddRef)                (ma_IXAudio2* pThis);
    ULONG   (STDMETHODCALLTYPE * Release)               (ma_IXAudio2* pThis);
    HRESULT (STDMETHODCALLTYPE * RegisterForCallbacks)  (ma_IXAudio2* pThis, ma_IXAudio2EngineCallback* pCallback);
    void    (STDMETHODCALLTYPE * UnregisterForCallbacks)(ma_IXAudio2* pThis, ma_IXAudio2EngineCallback* pCallback);
    HRESULT (STDMETHODCALLTYPE * CreateSourceVoice)     (ma_IXAudio2* pThis, ma_IXAudio2Voice** ppSourceVoice, const MA_XAUDIO2_WAVEFORMATEXTENSIBLE* pSourceFormat, ma_uint32 flags, float maxFrequencyRatio, ma_IXAudio2VoiceCallback* pCallback, const MA_XAUDIO2_VOICE_SENDS* pSendList, const MA_XAUDIO2_EFFECT_CHAIN* pEffectChain);
    HRESULT (STDMETHODCALLTYPE * CreateSubmixVoice)     (ma_IXAudio2* pThis, ma_IXAudio2Voice** ppSubmixVoice, ma_uint32 inputChannels, ma_uint32 inputSampleRate, ma_uint32 flags, ma_uint32 processingStage, const MA_XAUDIO2_VOICE_SENDS* pSendList, const MA_XAUDIO2_EFFECT_CHAIN* pEffectChain);
    HRESULT (STDMETHODCALLTYPE * CreateMasteringVoice)  (ma_IXAudio2* pThis, ma_IXAudio2Voice** ppMasteringVoice, ma_uint32 inputChannels, ma_uint32 inputSampleRate, ma_uint32 flags, const WCHAR* pDeviceID, const MA_XAUDIO2_EFFECT_CHAIN* pEffectChain, int streamCategory);
} ma_IXAudio2_Vtbl;

struct ma_IXAudio2
{
    ma_IXAudio2_Vtbl* lpVtbl;
};

typedef struct ma_IXAudio2Extension ma_IXAudio2Extension;
typedef struct
{
    HRESULT (STDMETHODCALLTYPE * QueryInterface)      (ma_IXAudio2Extension* pThis, const IID* riid, void** ppObject);
    ULONG   (STDMETHODCALLTYPE * AddRef)              (ma_IXAudio2Extension* pThis);
    ULONG   (STDMETHODCALLTYPE * Release)             (ma_IXAudio2Extension* pThis);
    void    (STDMETHODCALLTYPE * GetProcessingQuantum)(ma_IXAudio2Extension* pThis, ma_uint32* pQuantumNumerator, ma_uint32* pQuantumDenominator);
    void    (STDMETHODCALLTYPE * GetProcessor)        (ma_IXAudio2Extension* pThis, ma_uint32* pProcessor);
} ma_IXAudio2Extension_Vtbl;

struct ma_IXAudio2Extension
{
    ma_IXAudio2Extension_Vtbl* lpVtbl;
};

typedef HRESULT (WINAPI * ma_XAudio2CreateProc)(ma_IXAudio2** ppXAudio2, ma_uint32 flags, ma_uint32 processor);

#if defined(MA_NO_RUNTIME_LINKING) || defined(MA_XBOX_360)
#ifdef __cplusplus
extern "C" {
#endif

#if defined(MA_XBOX_360)
HRESULT WINAPI XAudio2Create(ma_IXAudio2_27** ppXAudio2, ma_uint32 flags, ma_uint32 processor);
#else
HRESULT WINAPI XAudio2Create(ma_IXAudio2** ppXAudio2, ma_uint32 flags, ma_uint32 processor);
#endif

#ifdef __cplusplus
}
#endif
#endif

typedef HRESULT (WINAPI * ma_CoInitializeExProc  )(void* pReserved, DWORD flags);
typedef void    (WINAPI * ma_CoUninitializeProc  )(void);
typedef HRESULT (WINAPI * ma_CoGetClassObjectProc)(const IID* classID, DWORD classContext, void* pReserved, const IID* interfaceID, void** ppObject);
#if !defined(MA_XBOX_360)
typedef HRESULT (WINAPI * ma_CoCreateInstanceProc)(const IID* classID, void* pOuter, DWORD classContext, const IID* interfaceID, void** ppObject);
typedef void    (WINAPI * ma_CoTaskMemFreeProc    )(void* pMemory);
typedef HRESULT (WINAPI * ma_PropVariantClearProc )(MA_XAUDIO2_PROPVARIANT* pValue);
#endif

struct ma_context_state_xaudio2
{
    ma_xaudio2_version version;
    ma_handle hOle32DLL;
    #if !defined(MA_XBOX_360)
    ma_handle hXAudio2DLL;
    #endif
    ma_XAudio2CreateProc    XAudio2Create;
    ma_CoInitializeExProc   CoInitializeEx;
    ma_CoUninitializeProc   CoUninitialize;
    ma_CoGetClassObjectProc CoGetClassObject;
    #if !defined(MA_XBOX_360)
    ma_CoCreateInstanceProc CoCreateInstance;
    ma_CoTaskMemFreeProc    CoTaskMemFree;
    ma_PropVariantClearProc PropVariantClear;
    #endif
};

struct ma_device_state_xaudio2
{
    void* pXAudio2;
    void* pMasteringVoice;
    void* pSourceVoice;
    ma_IXAudio2EngineCallback engineCallback;
    ma_IXAudio2VoiceCallback voiceCallback;
    ma_event event;
    ma_uint32 error;
    void* pBuffer;
    ma_uint32 iNextBuffer;
};


static ma_context_state_xaudio2* ma_context_get_backend_state__xaudio2(ma_context* pContext)
{
    return (ma_context_state_xaudio2*)ma_context_get_backend_state(pContext);
}

static ma_device_state_xaudio2* ma_device_get_backend_state__xaudio2(ma_device* pDevice)
{
    return (ma_device_state_xaudio2*)ma_device_get_backend_state(pDevice);
}


static void STDMETHODCALLTYPE ma_IXAudio2EngineCallback_OnProcessingPassStart(ma_IXAudio2EngineCallback* pThis)
{
    (void)pThis;
}

static void STDMETHODCALLTYPE ma_IXAudio2EngineCallback_OnProcessingPassEnd(ma_IXAudio2EngineCallback* pThis)
{
    (void)pThis;
}

static void STDMETHODCALLTYPE ma_IXAudio2EngineCallback_OnCriticalError(ma_IXAudio2EngineCallback* pThis, HRESULT error)
{
    InterlockedExchange((LONG*)&pThis->pDeviceState->error, error);
    ma_event_signal(&pThis->pDeviceState->event);
}

static ma_IXAudio2EngineCallback_Vtbl ma_gIXAudio2EngineCallback_VTable =
{
    ma_IXAudio2EngineCallback_OnProcessingPassStart,
    ma_IXAudio2EngineCallback_OnProcessingPassEnd,
    ma_IXAudio2EngineCallback_OnCriticalError
};

static void STDMETHODCALLTYPE ma_IXAudio2VoiceCallback_OnVoiceProcessingPassStart(ma_IXAudio2VoiceCallback* pThis, ma_uint32 bytesRequired)
{
    (void)pThis;
    (void)bytesRequired;
}

static void STDMETHODCALLTYPE ma_IXAudio2VoiceCallback_OnVoiceProcessingPassEnd(ma_IXAudio2VoiceCallback* pThis)
{
    (void)pThis;
}

static void STDMETHODCALLTYPE ma_IXAudio2VoiceCallback_OnStreamEnd(ma_IXAudio2VoiceCallback* pThis)
{
    (void)pThis;
}

static void STDMETHODCALLTYPE ma_IXAudio2VoiceCallback_OnBufferStart(ma_IXAudio2VoiceCallback* pThis, void* pBufferContext)
{
    (void)pThis;
    (void)pBufferContext;
}

static void STDMETHODCALLTYPE ma_IXAudio2VoiceCallback_OnBufferEnd(ma_IXAudio2VoiceCallback* pThis, void* pBufferContext)
{
    (void)pBufferContext;

    ma_event_signal(&pThis->pDeviceState->event);
}

static void STDMETHODCALLTYPE ma_IXAudio2VoiceCallback_OnLoopEnd(ma_IXAudio2VoiceCallback* pThis, void* pBufferContext)
{
    (void)pThis;
    (void)pBufferContext;
}

static void STDMETHODCALLTYPE ma_IXAudio2VoiceCallback_OnVoiceError(ma_IXAudio2VoiceCallback* pThis, void* pBufferContext, HRESULT error)
{
    (void)pBufferContext;

    InterlockedExchange((LONG*)&pThis->pDeviceState->error, error);
    ma_event_signal(&pThis->pDeviceState->event);
}

static ma_IXAudio2VoiceCallback_Vtbl ma_gIXAudio2VoiceCallback_VTable =
{
    ma_IXAudio2VoiceCallback_OnVoiceProcessingPassStart,
    ma_IXAudio2VoiceCallback_OnVoiceProcessingPassEnd,
    ma_IXAudio2VoiceCallback_OnStreamEnd,
    ma_IXAudio2VoiceCallback_OnBufferStart,
    ma_IXAudio2VoiceCallback_OnBufferEnd,
    ma_IXAudio2VoiceCallback_OnLoopEnd,
    ma_IXAudio2VoiceCallback_OnVoiceError
};


static void ma_backend_info__xaudio2(ma_device_backend_info* pBackendInfo)
{
    pBackendInfo->pName = "XAudio2";
}

static ma_result ma_context_init_modern__xaudio2(ma_context* pContext, ma_xaudio2_version version, ma_context_state_xaudio2* pContextStateXAudio2)
{
    #if defined(MA_XBOX_360)
    {
        (void)pContext;
        (void)version;
        (void)pContextStateXAudio2;

        /* The Xbox 360 cannot use the modern path. */
        return MA_NOT_IMPLEMENTED;
    }
    #else
    {
        #ifndef MA_NO_RUNTIME_LINKING
        {
            const char* pDLLName;

            if (version == ma_xaudio2_version_29) {
                pDLLName = "XAudio2_9.dll";
            } else {
                pDLLName = "XAudio2_8.dll";
            }

            pContextStateXAudio2->hXAudio2DLL = ma_dlopen(ma_context_get_log(pContext), pDLLName);
            if (pContextStateXAudio2->hXAudio2DLL == NULL) {
                return MA_NO_BACKEND;
            }

            pContextStateXAudio2->XAudio2Create = (ma_XAudio2CreateProc)ma_dlsym(ma_context_get_log(pContext), pContextStateXAudio2->hXAudio2DLL, "XAudio2Create");
        }
        #else
        {
            /*
            We'll get here if we're doing compile time linking. With runtime linking we just use
            the DLL to determine which version we're using, but we can't use that technique with
            compile time linking. Instead we're going to query the IXAudio2 interface for each
            version, starting with 2.9, then 2.8, using the first one that works. The interface is
            immediately released.
            */
            ma_IXAudio2* pXAudio2;
            ma_IXAudio2* pInterface;
            HRESULT hr;
            HRESULT hrCoInitialize;

            pContextStateXAudio2->XAudio2Create = XAudio2Create;

            hrCoInitialize = pContextStateXAudio2->CoInitializeEx(NULL, COINIT_MULTITHREADED);
            if (FAILED(hrCoInitialize) && hrCoInitialize != RPC_E_CHANGED_MODE) {
                return MA_ERROR;
            }

            hr = pContextStateXAudio2->XAudio2Create(&pXAudio2, 0, MA_XAUDIO2_PROCESSOR_1);
            if (SUCCEEDED(hr)) {
                hr = pXAudio2->lpVtbl->QueryInterface(pXAudio2, &MA_IID_IXAudio2_29, (void**)&pInterface);
                if (SUCCEEDED(hr)) {
                    version = ma_xaudio2_version_29;
                } else if (hr == E_NOINTERFACE) {
                    hr = pXAudio2->lpVtbl->QueryInterface(pXAudio2, &MA_IID_IXAudio2_28, (void**)&pInterface);
                    if (SUCCEEDED(hr)) {
                        version = ma_xaudio2_version_28;
                    }
                }

                if (SUCCEEDED(hr)) {
                    pInterface->lpVtbl->Release(pInterface);
                }

                pXAudio2->lpVtbl->Release(pXAudio2);
            }

            if (SUCCEEDED(hrCoInitialize)) {
                pContextStateXAudio2->CoUninitialize();
            }

            if (FAILED(hr)) {
                return MA_NO_BACKEND;
            }
        }
        #endif

        if (pContextStateXAudio2->XAudio2Create == NULL) {
            ma_dlclose(ma_context_get_log(pContext), pContextStateXAudio2->hXAudio2DLL);
            pContextStateXAudio2->hXAudio2DLL = NULL;

            return MA_NO_BACKEND;
        }

        pContextStateXAudio2->version = version;
        return MA_SUCCESS;
    }
    #endif
}

static ma_result ma_context_init_legacy__xaudio2(ma_context* pContext, ma_context_state_xaudio2* pContextStateXAudio2)
{
    #if defined(MA_XBOX_360)
    {
        (void)pContext;

        pContextStateXAudio2->version = ma_xaudio2_version_27;
        return MA_SUCCESS;
    }
    #else
    {
        ma_IClassFactory* pClassFactory;
        HRESULT hr;
        HRESULT hrCoInitialize;

        /* Legacy (XAudio2 2.7). CoGetClassObject() is used to detect whether or not XAudio2 is available. TODO: Use ma_CoInitializeEx(), etc. when this is integrated into miniaudio proper. Will need to add a ma_CoGetClassObject(). */

        /*
        There's a known issue with XAudio2 version 2.7 on older versions of Windows. Basically, you can
        get a crash due to the XAudio2 DLL being prematurely unloaded. To address this we just load it
        ourselves and keep it loaded for the lifetime of the context. More details here:

            https://web.archive.org/web/20260619123549/https://walbourn.github.io/known-issues-xaudio-2-7/
        */
        pContextStateXAudio2->hXAudio2DLL = ma_dlopen(ma_context_get_log(pContext), "XAudio2_7.dll");
        if (pContextStateXAudio2->hXAudio2DLL == NULL) {
            return MA_API_NOT_FOUND;
        }

        /*
        In order to detect whether or not XAudio2 is available we just query the IXAudio2 object and
        then immediately release it. If we can retrieve it we'll assume support.
        */
        hrCoInitialize = pContextStateXAudio2->CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hrCoInitialize) && hrCoInitialize != RPC_E_CHANGED_MODE) {
            return MA_ERROR;
        }

        hr = pContextStateXAudio2->CoGetClassObject(&MA_CLSID_XAudio2_27, CLSCTX_INPROC_SERVER, NULL, &MA_IID_IClassFactory, (void**)&pClassFactory);
    
        if (SUCCEEDED(hr)) {
            ma_IClassFactory_Release(pClassFactory);
        }

        if (SUCCEEDED(hrCoInitialize)) {
            pContextStateXAudio2->CoUninitialize();
        }

        if (FAILED(hr)) {
            return MA_API_NOT_FOUND;
        }

        pContextStateXAudio2->version = ma_xaudio2_version_27;
        return MA_SUCCESS;

    }
    #endif
}

static ma_result ma_context_init__xaudio2(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    const ma_context_config_xaudio2* pContextConfigXAudio2 = (const ma_context_config_xaudio2*)pContextBackendConfig;
    ma_context_config_xaudio2 defaultConfigXAudio2;
    ma_context_state_xaudio2* pContextStateXAudio2;
    ma_xaudio2_version version;
    ma_result result;
    size_t iVersion;
    const ma_xaudio2_version versions[] = {
        #if defined(MA_NO_RUNTIME_LINKING) || defined(MA_XBOX_360)
            #if defined(MA_XBOX_360)
            ma_xaudio2_version_27
            #else
            ma_xaudio2_version_default
            #endif
        #else
        ma_xaudio2_version_29,
        ma_xaudio2_version_28,
        ma_xaudio2_version_27
        #endif
    };

    *ppContextState = NULL;

    if (pContextConfigXAudio2 == NULL) {
        defaultConfigXAudio2 = ma_context_config_xaudio2_init();
        pContextConfigXAudio2 = &defaultConfigXAudio2;
    }

    version = pContextConfigXAudio2->version;
    #if defined(MA_NO_RUNTIME_LINKING) || defined(MA_XBOX_360)
    version = ma_xaudio2_version_default;
    #endif

    pContextStateXAudio2 = (ma_context_state_xaudio2*)ma_calloc(sizeof(*pContextStateXAudio2), ma_context_get_allocation_callbacks(pContext));
    if (pContextStateXAudio2 == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    #if !defined(MA_XBOX_360)
    {
        #ifndef MA_NO_RUNTIME_LINKING
        {
            pContextStateXAudio2->hOle32DLL = ma_dlopen(ma_context_get_log(pContext), "ole32.dll");
            if (pContextStateXAudio2->hOle32DLL == NULL) {
                result = MA_API_NOT_FOUND;
                goto error;
            }

            pContextStateXAudio2->CoInitializeEx   = (ma_CoInitializeExProc  )ma_dlsym(ma_context_get_log(pContext), pContextStateXAudio2->hOle32DLL, "CoInitializeEx");
            pContextStateXAudio2->CoUninitialize   = (ma_CoUninitializeProc  )ma_dlsym(ma_context_get_log(pContext), pContextStateXAudio2->hOle32DLL, "CoUninitialize");
            pContextStateXAudio2->CoGetClassObject = (ma_CoGetClassObjectProc)ma_dlsym(ma_context_get_log(pContext), pContextStateXAudio2->hOle32DLL, "CoGetClassObject");
            pContextStateXAudio2->CoCreateInstance = (ma_CoCreateInstanceProc)ma_dlsym(ma_context_get_log(pContext), pContextStateXAudio2->hOle32DLL, "CoCreateInstance");
            pContextStateXAudio2->CoTaskMemFree    = (ma_CoTaskMemFreeProc   )ma_dlsym(ma_context_get_log(pContext), pContextStateXAudio2->hOle32DLL, "CoTaskMemFree");
            pContextStateXAudio2->PropVariantClear = (ma_PropVariantClearProc)ma_dlsym(ma_context_get_log(pContext), pContextStateXAudio2->hOle32DLL, "PropVariantClear");
        }
        #else
        {
            pContextStateXAudio2->CoInitializeEx   = CoInitializeEx;
            pContextStateXAudio2->CoUninitialize   = CoUninitialize;
            pContextStateXAudio2->CoGetClassObject = (ma_CoGetClassObjectProc)CoGetClassObject;
            pContextStateXAudio2->CoCreateInstance = (ma_CoCreateInstanceProc)CoCreateInstance;
            pContextStateXAudio2->CoTaskMemFree    = CoTaskMemFree;
            pContextStateXAudio2->PropVariantClear = (ma_PropVariantClearProc)PropVariantClear;
        }
        #endif

        if (pContextStateXAudio2->CoInitializeEx == NULL || pContextStateXAudio2->CoUninitialize == NULL || pContextStateXAudio2->CoGetClassObject == NULL) {
            result = MA_API_NOT_FOUND;
            goto error;
        }

        if (pContextStateXAudio2->CoCreateInstance == NULL || pContextStateXAudio2->CoTaskMemFree == NULL || pContextStateXAudio2->PropVariantClear == NULL) {
            result = MA_API_NOT_FOUND;
            goto error;
        }
    }
    #endif

    /*
    There are two code paths at play with the XAudio2 backend, depending on the version:

        Modern: 2.9 and 2.8 (Xbox One onwards)
        Legacy: 2.7 (Xbox 360)

    The modern path is prioritized, and if that fails the the legacy path will be attempted.
    */
    result = MA_NO_BACKEND;
    for (iVersion = 0; iVersion < ma_countof(versions); iVersion += 1) {
        if (version != ma_xaudio2_version_default && version != versions[iVersion]) {
            continue;
        }

        if (versions[iVersion] == ma_xaudio2_version_27) {
            result = ma_context_init_legacy__xaudio2(pContext, pContextStateXAudio2);
        } else {
            result = ma_context_init_modern__xaudio2(pContext, versions[iVersion], pContextStateXAudio2);
        }

        if (result == MA_SUCCESS) {
            break;
        }
    }

    if (result != MA_SUCCESS) {
        goto error;
    }

    *ppContextState = pContextStateXAudio2;
    ma_log_postf(ma_context_get_log(pContext), MA_LOG_LEVEL_DEBUG, "[XAudio2] Version: %d.%d\n", pContextStateXAudio2->version / 10, pContextStateXAudio2->version % 10);

    return MA_SUCCESS;

error:
    #if !defined(MA_XBOX_360)
    {
        if (pContextStateXAudio2->hXAudio2DLL != NULL) {
            ma_dlclose(ma_context_get_log(pContext), pContextStateXAudio2->hXAudio2DLL);
        }

        if (pContextStateXAudio2->hOle32DLL != NULL) {
            ma_dlclose(ma_context_get_log(pContext), pContextStateXAudio2->hOle32DLL);
        }
    }
    #endif

    ma_free(pContextStateXAudio2, ma_context_get_allocation_callbacks(pContext));
    return result;
}

static void ma_context_uninit__xaudio2(ma_context* pContext)
{
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(pContext);

    #if !defined(MA_XBOX_360)
    {
        if (pContextStateXAudio2->hXAudio2DLL != NULL) {
            ma_dlclose(ma_context_get_log(pContext), pContextStateXAudio2->hXAudio2DLL);
        }

        ma_dlclose(ma_context_get_log(pContext), pContextStateXAudio2->hOle32DLL);
    }
    #endif

    ma_free(pContextStateXAudio2, ma_context_get_allocation_callbacks(pContext));
}

static ma_format ma_format_from_wave_format__xaudio2(const MA_XAUDIO2_WAVEFORMATEXTENSIBLE* pFormat)
{
    WORD formatTag;

    formatTag = pFormat->wFormatTag;
    if (formatTag == 0xFFFE) {
        if (memcmp(&pFormat->SubFormat, &MA_XAUDIO2_SUBTYPE_PCM, sizeof(GUID)) == 0) {
            formatTag = 1;
        } else if (memcmp(&pFormat->SubFormat, &MA_XAUDIO2_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) {
            formatTag = 3;
        }
    }

    if (formatTag == 1) {
        switch (pFormat->wBitsPerSample) {
            case 8:  return ma_format_u8;
            case 16: return ma_format_s16;
            case 24: return ma_format_s24;
            case 32: return ma_format_s32;
            default: break;
        }
    } else if (formatTag == 3) {
        switch (pFormat->wBitsPerSample) {
            case 32: return ma_format_f32;
            default: break;
        }
    }

    return ma_format_unknown;
}

static ma_result ma_create_engine__xaudio2(ma_context_state_xaudio2* pContextStateXAudio2, ma_uint32 flags, void** ppXAudio2)
{
    *ppXAudio2 = NULL;

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2_27* pXAudio2;
        HRESULT hr;

        #if defined(MA_XBOX_360)
        {
            hr = XAudio2Create(&pXAudio2, flags, MA_XAUDIO2_DEFAULT_PROCESSOR);
            if (FAILED(hr)) {
                return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
            }
        }
        #else
        {
            ma_IClassFactory* pClassFactory;

            /* We need a class factory in order to create the XAudio2 instance. Once we have the instance we can release the factory. */
            hr = pContextStateXAudio2->CoGetClassObject(&MA_CLSID_XAudio2_27, CLSCTX_INPROC_SERVER, NULL, &MA_IID_IClassFactory, (void**)&pClassFactory);
            if (FAILED(hr)) {
                return MA_API_NOT_FOUND;
            }

            hr = ma_IClassFactory_CreateInstance(pClassFactory, NULL, &MA_IID_IXAudio2_27, (void**)&pXAudio2);
            ma_IClassFactory_Release(pClassFactory);

            if (FAILED(hr)) {
                return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
            }

            hr = pXAudio2->lpVtbl->Initialize(pXAudio2, flags, MA_XAUDIO2_DEFAULT_PROCESSOR);
            if (FAILED(hr)) {
                pXAudio2->lpVtbl->Release(pXAudio2);
                return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
            }
        }
        #endif

        *ppXAudio2 = pXAudio2;
        return MA_SUCCESS;
    } else {
        ma_IXAudio2* pXAudio2;
        HRESULT hr;

        hr = pContextStateXAudio2->XAudio2Create(&pXAudio2, flags, MA_XAUDIO2_PROCESSOR_1);
        if (FAILED(hr)) {
            return MA_FAILED_TO_OPEN_BACKEND_DEVICE;
        }

        *ppXAudio2 = pXAudio2;
        return MA_SUCCESS;
    }
}

static ma_result ma_device_name_from_wide__xaudio2(const WCHAR* pName, char* pNameOut, size_t nameCapacity)
{
    char deviceName[256 * 3];
    int wideNameLength;
    int nameLengthUTF8;
    size_t nameLength;

    wideNameLength = lstrlenW(pName);
    if (wideNameLength > 255) {
        wideNameLength = 255;
        if (pName[wideNameLength - 1] >= 0xD800 && pName[wideNameLength - 1] <= 0xDBFF) {
            wideNameLength -= 1;
        }
    }

    pNameOut[0] = '\0';
    if (wideNameLength == 0) {
        return MA_SUCCESS;
    }

    /* XAudio2 device names are UTF-16, but ours are UTF-8. Convert and truncate. */
    nameLengthUTF8 = WideCharToMultiByte(CP_UTF8, 0, pName, wideNameLength, deviceName, sizeof(deviceName), NULL, NULL);
    if (nameLengthUTF8 == 0) {
        return MA_ERROR;
    }

    nameLength = nameLengthUTF8;
    if (nameLength >= nameCapacity) {
        nameLength = nameCapacity - 1;

        /* This is just making sure we don't truncate in the middle of a UTF-8 code point. */
        while (nameLength > 0 && (deviceName[nameLength] & 0xC0) == 0x80) {
            nameLength -= 1;
        }
    }

    MA_XAUDIO2_COPY_MEMORY(pNameOut, deviceName, nameLength);
    pNameOut[nameLength] = '\0';

    return MA_SUCCESS;
}

static ma_result ma_context_enumerate_devices_legacy__xaudio2(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pUserData)
{
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(pContext);
    ma_IXAudio2_27* pXAudio2 = NULL;
    ma_result result;
    HRESULT hr;
    ma_uint32 deviceCount;
    ma_uint32 iDevice;

    result = ma_create_engine__xaudio2(pContextStateXAudio2, 0, (void**)&pXAudio2);
    if (result != MA_SUCCESS) {
        return result;
    }

    hr = pXAudio2->lpVtbl->GetDeviceCount(pXAudio2, &deviceCount);
    if (FAILED(hr)) {
        result = MA_ERROR;
        goto done;
    }

    for (iDevice = 0; iDevice < deviceCount; iDevice += 1) {
        MA_XAUDIO2_DEVICE_DETAILS details;
        ma_device_info info;
        ma_format format;

        hr = pXAudio2->lpVtbl->GetDeviceDetails(pXAudio2, iDevice, &details);
        if (FAILED(hr)) {
            continue;
        }

        MA_XAUDIO2_ZERO_MEMORY(&info, sizeof(info));
        info.id.custom.u = iDevice;
        info.isDefault = (details.Role & MA_XAUDIO2_DEFAULT_GAME_DEVICE) != 0;

        if (ma_device_name_from_wide__xaudio2(details.DisplayName, info.name, sizeof(info.name)) != MA_SUCCESS) {
            continue;
        }

        format = ma_format_from_wave_format__xaudio2(&details.OutputFormat);
        if (format != ma_format_unknown) {
            ma_device_info_add_native_data_format(&info, format, details.OutputFormat.nChannels, details.OutputFormat.nChannels, details.OutputFormat.nSamplesPerSec, details.OutputFormat.nSamplesPerSec);
        }

        if (callback(ma_device_type_playback, &info, pUserData) == MA_DEVICE_ENUMERATION_ABORT) {
            break;
        }
    }

    result = MA_SUCCESS;

done:
    pXAudio2->lpVtbl->Release(pXAudio2);
    return result;
}

#if defined(MA_WIN32_DESKTOP) || defined(MA_WIN32_GDK)
static ma_EDataFlow_xaudio2 ma_device_type_to_EDataFlow__xaudio2(ma_device_type deviceType)
{
    if (deviceType == ma_device_type_capture) {
        return ma_eCapture_xaudio2;
    } else {
        return ma_eRender_xaudio2;
    }
}

static WCHAR* ma_context_get_default_device_id_from_IMMDeviceEnumerator__xaudio2(ma_context* pContext, ma_IMMDeviceEnumerator_xaudio2* pDeviceEnumerator, ma_device_type deviceType)
{
    HRESULT hr;
    ma_IMMDevice_xaudio2* pMMDefaultDevice = NULL;
    WCHAR* pDefaultDeviceID = NULL;
    ma_EDataFlow_xaudio2 dataFlow;
    ma_ERole_xaudio2 role;

    (void)pContext;

    dataFlow = ma_device_type_to_EDataFlow__xaudio2(deviceType);
    role = ma_eConsole_xaudio2;

    hr = ma_IMMDeviceEnumerator_GetDefaultAudioEndpoint__xaudio2(pDeviceEnumerator, dataFlow, role, &pMMDefaultDevice);
    if (FAILED(hr)) {
        return NULL;
    }

    hr = ma_IMMDevice_GetId__xaudio2(pMMDefaultDevice, &pDefaultDeviceID);

    ma_IMMDevice_Release__xaudio2(pMMDefaultDevice);
    pMMDefaultDevice = NULL;

    if (FAILED(hr)) {
        return NULL;
    }

    return pDefaultDeviceID;
}

static ma_result ma_context_get_device_info_from_MMDevice__xaudio2(ma_context* pContext, ma_IMMDevice_xaudio2* pMMDevice, WCHAR* pDefaultDeviceID, ma_device_info* pInfo)
{
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(pContext);
    HRESULT hr;
    WCHAR* pDeviceIDString = NULL;
    int idLength;
    ma_IPropertyStore_xaudio2* pProperties;
    MA_XAUDIO2_PROPVARIANT var;

    hr = ma_IMMDevice_GetId__xaudio2(pMMDevice, &pDeviceIDString);
    if (FAILED(hr)) {
        return MA_ERROR;
    }

    if (pDefaultDeviceID != NULL) {
        if (lstrcmpW(pDeviceIDString, pDefaultDeviceID) == 0) {
            pInfo->isDefault = MA_TRUE;
        }
    }

    idLength = WideCharToMultiByte(CP_UTF8, 0, pDeviceIDString, -1, pInfo->id.custom.s, sizeof(pInfo->id.custom.s), NULL, NULL);
    pContextStateXAudio2->CoTaskMemFree(pDeviceIDString);

    if (idLength == 0) {
        return MA_ERROR;
    }

    ma_strncpy_s(pInfo->name, sizeof(pInfo->name), pInfo->id.custom.s, (size_t)-1);

    hr = ma_IMMDevice_OpenPropertyStore__xaudio2(pMMDevice, STGM_READ, &pProperties);
    if (FAILED(hr)) {
        return MA_SUCCESS;
    }


    MA_XAUDIO2_ZERO_MEMORY(&var, sizeof(var));
    hr = ma_IPropertyStore_GetValue__xaudio2(pProperties, &MA_XAUDIO2_PKEY_Device_FriendlyName, &var);
    if (SUCCEEDED(hr) && var.vt == MA_XAUDIO2_VT_LPWSTR && var.data.pwszVal != NULL) {
        ma_device_name_from_wide__xaudio2(var.data.pwszVal, pInfo->name, sizeof(pInfo->name));
    }

    pContextStateXAudio2->PropVariantClear(&var);


    MA_XAUDIO2_ZERO_MEMORY(&var, sizeof(var));
    hr = ma_IPropertyStore_GetValue__xaudio2(pProperties, &MA_XAUDIO2_PKEY_AudioEngine_DeviceFormat, &var);
    if (SUCCEEDED(hr) && var.vt == MA_XAUDIO2_VT_BLOB && var.data.blob.cbSize >= 18 && var.data.blob.pBlobData != NULL) {
        MA_XAUDIO2_WAVEFORMATEXTENSIBLE wf;
        ma_format format;

        MA_XAUDIO2_ZERO_MEMORY(&wf, sizeof(wf));
        MA_XAUDIO2_COPY_MEMORY(&wf, var.data.blob.pBlobData, ma_min(var.data.blob.cbSize, sizeof(wf)));

        format = ma_format_from_wave_format__xaudio2(&wf);
        if (format != ma_format_unknown) {
            ma_device_info_add_native_data_format(pInfo, format, wf.nChannels, wf.nChannels, wf.nSamplesPerSec, wf.nSamplesPerSec);
        }
    }

    pContextStateXAudio2->PropVariantClear(&var);


    ma_IPropertyStore_Release__xaudio2(pProperties);
    return MA_SUCCESS;
}

static ma_result ma_context_enumerate_devices_by_type__xaudio2(ma_context* pContext, ma_IMMDeviceEnumerator_xaudio2* pDeviceEnumerator, ma_device_type deviceType, ma_enum_devices_callback_proc callback, void* pUserData)
{
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(pContext);
    ma_result result;
    UINT deviceCount;
    HRESULT hr;
    ma_uint32 iDevice;
    WCHAR* pDefaultDeviceID = NULL;
    ma_IMMDeviceCollection_xaudio2* pDeviceCollection = NULL;

    pDefaultDeviceID = ma_context_get_default_device_id_from_IMMDeviceEnumerator__xaudio2(pContext, pDeviceEnumerator, deviceType);

    hr = ma_IMMDeviceEnumerator_EnumAudioEndpoints__xaudio2(pDeviceEnumerator, ma_device_type_to_EDataFlow__xaudio2(deviceType), MA_XAUDIO2_MM_DEVICE_STATE_ACTIVE, &pDeviceCollection);
    if (FAILED(hr)) {
        result = MA_ERROR;
        goto done;
    }

    hr = ma_IMMDeviceCollection_GetCount__xaudio2(pDeviceCollection, &deviceCount);
    if (FAILED(hr)) {
        result = MA_ERROR;
        goto done;
    }

    for (iDevice = 0; iDevice < deviceCount; ++iDevice) {
        ma_device_info deviceInfo;
        ma_IMMDevice_xaudio2* pMMDevice;
        ma_device_enumeration_result cbResult;

        MA_XAUDIO2_ZERO_MEMORY(&deviceInfo, sizeof(deviceInfo));

        hr = ma_IMMDeviceCollection_Item__xaudio2(pDeviceCollection, iDevice, &pMMDevice);
        if (FAILED(hr)) {
            continue;
        }

        result = ma_context_get_device_info_from_MMDevice__xaudio2(pContext, pMMDevice, pDefaultDeviceID, &deviceInfo);

        ma_IMMDevice_Release__xaudio2(pMMDevice);
        if (result != MA_SUCCESS) {
            continue;
        }

        cbResult = callback(deviceType, &deviceInfo, pUserData);
        if (cbResult == MA_DEVICE_ENUMERATION_ABORT) {
            break;
        }
    }

    result = MA_SUCCESS;

done:
    if (pDefaultDeviceID != NULL) {
        pContextStateXAudio2->CoTaskMemFree(pDefaultDeviceID);
        pDefaultDeviceID = NULL;
    }

    if (pDeviceCollection != NULL) {
        ma_IMMDeviceCollection_Release__xaudio2(pDeviceCollection);
        pDeviceCollection = NULL;
    }

    return result;
}
#endif

static ma_result ma_context_enumerate_devices_modern__xaudio2(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pUserData)
{
    #if defined(MA_WIN32_DESKTOP) || defined(MA_WIN32_GDK)
    {
        ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(pContext);
        ma_IMMDeviceEnumerator_xaudio2* pDeviceEnumerator;
        ma_result result;
        HRESULT hr;

        hr = pContextStateXAudio2->CoCreateInstance(&MA_XAUDIO2_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &MA_XAUDIO2_IID_IMMDeviceEnumerator, (void**)&pDeviceEnumerator);
        if (FAILED(hr)) {
            return MA_ERROR;
        }

        result = ma_context_enumerate_devices_by_type__xaudio2(pContext, pDeviceEnumerator, ma_device_type_playback, callback, pUserData);
        ma_IMMDeviceEnumerator_Release__xaudio2(pDeviceEnumerator);

        return result;
    }
    #else
    {
        (void)pContext;
        (void)callback;
        (void)pUserData;

        /* Should never get here. */
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

typedef struct
{
    ma_context* pContext;
    ma_enum_devices_callback_proc callback;
    void* pUserData;
    ma_result result;
} ma_context_enumerate_devices_thread_data__xaudio2;

static ma_thread_result MA_THREADCALL ma_context_enumerate_devices_thread__xaudio2(void* pUserData)
{
    ma_context_enumerate_devices_thread_data__xaudio2* pData = (ma_context_enumerate_devices_thread_data__xaudio2*)pUserData;
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(pData->pContext);

    #if !defined(MA_XBOX_360)
    {
        HRESULT hr;

        hr = pContextStateXAudio2->CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            pData->result = MA_ERROR;
            return 0;
        }
    }
    #endif

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        pData->result = ma_context_enumerate_devices_legacy__xaudio2(pData->pContext, pData->callback, pData->pUserData);
    } else {
        pData->result = ma_context_enumerate_devices_modern__xaudio2(pData->pContext, pData->callback, pData->pUserData);
    }

    #if !defined(MA_XBOX_360)
    pContextStateXAudio2->CoUninitialize();
    #endif

    return 0;
}

static ma_result ma_context_enumerate_devices__xaudio2(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    ma_result result;
    ma_thread thread;
    ma_context_enumerate_devices_thread_data__xaudio2 data;

    data.pContext  = pContext;
    data.callback  = callback;
    data.pUserData = pCallbackUserData;
    data.result    = MA_ERROR;

    result = ma_thread_create(&thread, ma_thread_priority_default, 0, ma_context_enumerate_devices_thread__xaudio2, &data, ma_context_get_allocation_callbacks(pContext));
    if (result != MA_SUCCESS) {
        return result;
    }

    ma_thread_wait(&thread);

    return data.result;
}

static ma_result ma_device_init__xaudio2(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(ma_device_get_context(pDevice));
    ma_device_state_xaudio2* pDeviceStateXAudio2;
    void* pXAudio2;
    void* pMasteringVoice;
    MA_XAUDIO2_WAVEFORMATEXTENSIBLE wf;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_channel channelMap[MA_MAX_CHANNELS];
    ma_uint32 periodSizeInFrames;
    ma_uint32 periodCount;
    ma_uint32 flags = 0;
    ma_uint32 iChannel;
    size_t allocationSize;
    HRESULT hr;
    ma_result result;

    (void)pDeviceBackendConfig;
    (void)pDescriptorCapture;

    *ppDeviceState = NULL;

    if (ma_device_get_type(pDevice) != ma_device_type_playback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    if (pDescriptorPlayback->shareMode == ma_share_mode_exclusive) {
        return MA_SHARE_MODE_NOT_SUPPORTED;
    }

    /*
    Version 2.9 supports XAUDIO2_1024_QUANTUM which basically just tells XAudio2 to use a bigger
    processing quantum size. We're going to use this if our requested period size is big enough.
    Unfortunately this is set as a flag when we create the engine, before we know the actual
    sample rate. This means when we're determining whether or not to use XAUDIO2_1024_QUANTUM our
    calculation might be slightly inaccurate. We will assume 48000 as the native rate.
    */
    if (pContextStateXAudio2->version == ma_xaudio2_version_29) {
        sampleRate = pDescriptorPlayback->sampleRate;
        if (sampleRate == 0) {
            sampleRate = MA_DEFAULT_SAMPLE_RATE;
        }

        periodSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDescriptorPlayback, sampleRate);
        
        /* Microsoft documents XAUDIO2_1024_QUANTUM as 1024 frames at 48 kHz, hence the hard coding of 1024 and 48000. */
        if ((ma_uint64)periodSizeInFrames * 48000 >= (ma_uint64)sampleRate * 1024) {
            flags |= MA_XAUDIO2_1024_QUANTUM;
        }
    }

    result = ma_create_engine__xaudio2(pContextStateXAudio2, flags, &pXAudio2);
    if (result != MA_SUCCESS && (flags & MA_XAUDIO2_1024_QUANTUM) != 0) {
        flags &= ~MA_XAUDIO2_1024_QUANTUM;
        result = ma_create_engine__xaudio2(pContextStateXAudio2, flags, &pXAudio2);
    }

    if (result != MA_SUCCESS) {
        return result;
    }

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2_27* pXAudio2Legacy = (ma_IXAudio2_27*)pXAudio2;
        ma_IXAudio2Voice_27* pMasteringVoiceLegacy;
        MA_XAUDIO2_VOICE_DETAILS_27 voiceDetails;
        ma_uint32 deviceIndex = 0;
        ma_uint32 deviceCount;

        if (pDescriptorPlayback->pDeviceID != NULL) {
            deviceIndex = pDescriptorPlayback->pDeviceID->custom.u;
        }

        /*
        Normally you would expect CreateMasteringVoice() to check the device index, but unfortunately
        when running under Wine it actually crashes when an invalid index is used. We're going to
        explicitly check for this with GetDeviceCount().
        */
        hr = pXAudio2Legacy->lpVtbl->GetDeviceCount(pXAudio2Legacy, &deviceCount);
        if (FAILED(hr) || deviceIndex >= deviceCount) {
            result = MA_NO_DEVICE;
            goto error_0;
        }

        hr = pXAudio2Legacy->lpVtbl->CreateMasteringVoice(pXAudio2Legacy, &pMasteringVoiceLegacy, 0, 0, 0, deviceIndex, NULL);
        if (FAILED(hr)) {
            result = MA_FAILED_TO_OPEN_BACKEND_DEVICE;
            goto error_0;
        }

        pMasteringVoice = pMasteringVoiceLegacy;

        pMasteringVoiceLegacy->lpVtbl->GetVoiceDetails(pMasteringVoiceLegacy, &voiceDetails);
        channels   = voiceDetails.InputChannels;
        sampleRate = voiceDetails.InputSampleRate;
    } else {
        ma_IXAudio2* pXAudio2Modern = (ma_IXAudio2*)pXAudio2;
        ma_IXAudio2Voice* pMasteringVoiceModern;
        MA_XAUDIO2_VOICE_DETAILS voiceDetails;
        WCHAR deviceID[sizeof(pDescriptorPlayback->pDeviceID->custom.s)];
        const WCHAR* pDeviceID = NULL;

        if (pDescriptorPlayback->pDeviceID != NULL) {
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pDescriptorPlayback->pDeviceID->custom.s, -1, deviceID, ma_countof(deviceID)) == 0) {
                result = MA_INVALID_DEVICE_CONFIG;
                goto error_0;
            }

            pDeviceID = deviceID;
        }

        hr = pXAudio2Modern->lpVtbl->CreateMasteringVoice(pXAudio2Modern, &pMasteringVoiceModern, 0, 0, 0, pDeviceID, NULL, MA_XAUDIO2_AUDIO_CATEGORY_GAME_EFFECTS);
        if (FAILED(hr)) {
            result = MA_FAILED_TO_OPEN_BACKEND_DEVICE;
            goto error_0;
        }

        pMasteringVoice = pMasteringVoiceModern;

        pMasteringVoiceModern->lpVtbl->GetVoiceDetails(pMasteringVoiceModern, &voiceDetails);
        channels   = voiceDetails.InputChannels;
        sampleRate = voiceDetails.InputSampleRate;
    }

    if (channels > 8) {
        result = MA_FORMAT_NOT_SUPPORTED;
        goto error_1;
    }

    format = pDescriptorPlayback->format;
    if (format == ma_format_unknown) {
        format = ma_format_f32;
    }

    if (pDescriptorPlayback->channels != 0) {
        channels = pDescriptorPlayback->channels;
    }
    if (channels > 8) {
        channels = 8;
    }

    if (pDescriptorPlayback->sampleRate != 0) {
        sampleRate = pDescriptorPlayback->sampleRate;
    }
    if (sampleRate < MA_XAUDIO2_MIN_SAMPLE_RATE) {
        sampleRate = MA_XAUDIO2_MIN_SAMPLE_RATE;
    }
    if (sampleRate > MA_XAUDIO2_MAX_SAMPLE_RATE) {
        sampleRate = MA_XAUDIO2_MAX_SAMPLE_RATE;
    }

    /*
    From MSDN (https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-audio-graph):

        All audio processing takes place in a separate thread with a periodicity defined by the
        graph's quantum (currently 10 milliseconds on Microsoft Windows, and 5 1/3 milliseconds on
        Xbox 360).
    */
    {
        #if defined(MA_XBOX_360)
        ma_uint32 quantumNum = 2;
        ma_uint32 quantumDen = 375;
        #else
        ma_uint32 quantumNum = 1;
        ma_uint32 quantumDen = 100;
        #endif

        if ((flags & MA_XAUDIO2_1024_QUANTUM) != 0) {
            quantumNum = 1024;
            quantumDen = 48000;
        }

        if (pContextStateXAudio2->version == ma_xaudio2_version_29) {
            ma_IXAudio2* pXAudio2Modern = (ma_IXAudio2*)pXAudio2;
            ma_IXAudio2Extension* pExtension;

            hr = pXAudio2Modern->lpVtbl->QueryInterface(pXAudio2Modern, &MA_IID_IXAudio2Extension, (void**)&pExtension);
            if (SUCCEEDED(hr)) {
                pExtension->lpVtbl->GetProcessingQuantum(pExtension, &quantumNum, &quantumDen);
                pExtension->lpVtbl->Release(pExtension);
            }
        }

        periodSizeInFrames = (ma_uint32)(((ma_uint64)sampleRate * quantumNum) / quantumDen);
        if (((ma_uint64)sampleRate * quantumNum) % quantumDen != 0) {
            periodSizeInFrames += 1;
        }
    }

    /* XAudio2 uses a buffer queue system. We map enqueued buffers to periods. */
    periodCount = pDescriptorPlayback->periodCount;
    if (periodCount < 2) {
        periodCount = 2;
    }
    if (periodCount > MA_XAUDIO2_MAX_QUEUED_BUFFERS) {
        periodCount = MA_XAUDIO2_MAX_QUEUED_BUFFERS;
    }
    

    MA_XAUDIO2_ZERO_MEMORY(&wf, sizeof(wf));
    wf.wFormatTag          = 0xFFFE;
    wf.nChannels           = (WORD)channels;
    wf.nSamplesPerSec      = sampleRate;
    wf.wBitsPerSample      = (WORD)(ma_get_bytes_per_sample(format) * 8);
    wf.nBlockAlign         = (WORD)(wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec     = wf.nSamplesPerSec * wf.nBlockAlign;
    wf.cbSize              = 22;
    wf.wValidBitsPerSample = wf.wBitsPerSample;
    wf.SubFormat           = MA_XAUDIO2_SUBTYPE_PCM;
    if (format == ma_format_f32) {
        wf.SubFormat = MA_XAUDIO2_SUBTYPE_IEEE_FLOAT;
    }

    ma_channel_map_init_standard(ma_standard_channel_map_microsoft, channelMap, sizeof(channelMap) / sizeof(channelMap[0]), channels);

    /* TODO: Replace this with ma_channel_map_to_channel_mask__win32() when this is integrated into miniaudio proper. */
    for (iChannel = 0; iChannel < channels; iChannel += 1) {
        if (channelMap[iChannel] == MA_CHANNEL_MONO) {
            wf.dwChannelMask |= 0x00000004;
        } else {
            wf.dwChannelMask |= 1UL << (channelMap[iChannel] - MA_CHANNEL_FRONT_LEFT);
        }
    }

    allocationSize  = 0;
    allocationSize += sizeof(*pDeviceStateXAudio2);
    allocationSize += periodSizeInFrames * ma_get_bytes_per_frame(format, channels) * periodCount;

    pDeviceStateXAudio2 = (ma_device_state_xaudio2*)ma_malloc(allocationSize, ma_device_get_allocation_callbacks(pDevice));
    if (pDeviceStateXAudio2 == NULL) {
        result = MA_OUT_OF_MEMORY;
        goto error_1;
    }

    pDeviceStateXAudio2->pXAudio2                    = pXAudio2;
    pDeviceStateXAudio2->pMasteringVoice             = pMasteringVoice;
    pDeviceStateXAudio2->pSourceVoice                = NULL;
    pDeviceStateXAudio2->pBuffer                     = ma_offset_ptr(pDeviceStateXAudio2, sizeof(*pDeviceStateXAudio2));
    pDeviceStateXAudio2->iNextBuffer                 = 0;
    pDeviceStateXAudio2->error                       = S_OK;
    pDeviceStateXAudio2->engineCallback.lpVtbl       = &ma_gIXAudio2EngineCallback_VTable;
    pDeviceStateXAudio2->engineCallback.pDeviceState = pDeviceStateXAudio2;
    pDeviceStateXAudio2->voiceCallback.lpVtbl        = &ma_gIXAudio2VoiceCallback_VTable;
    pDeviceStateXAudio2->voiceCallback.pDeviceState  = pDeviceStateXAudio2;

    result = ma_event_init(&pDeviceStateXAudio2->event);
    if (result != MA_SUCCESS) {
        goto error_2;
    }

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2_27* pXAudio2Legacy = (ma_IXAudio2_27*)pXAudio2;
        hr = pXAudio2Legacy->lpVtbl->RegisterForCallbacks(pXAudio2Legacy, &pDeviceStateXAudio2->engineCallback);
    } else {
        ma_IXAudio2* pXAudio2Modern = (ma_IXAudio2*)pXAudio2;
        hr = pXAudio2Modern->lpVtbl->RegisterForCallbacks(pXAudio2Modern, &pDeviceStateXAudio2->engineCallback);
    }

    if (FAILED(hr)) {
        result = MA_ERROR;
        goto error_3;
    }

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2_27* pXAudio2Legacy = (ma_IXAudio2_27*)pXAudio2;
        hr = pXAudio2Legacy->lpVtbl->CreateSourceVoice(pXAudio2Legacy, (ma_IXAudio2Voice_27**)&pDeviceStateXAudio2->pSourceVoice, &wf, MA_XAUDIO2_VOICE_NOPITCH, 1.0f, &pDeviceStateXAudio2->voiceCallback, NULL, NULL);
    } else {
        ma_IXAudio2* pXAudio2Modern = (ma_IXAudio2*)pXAudio2;
        hr = pXAudio2Modern->lpVtbl->CreateSourceVoice(pXAudio2Modern, (ma_IXAudio2Voice**)&pDeviceStateXAudio2->pSourceVoice, &wf, MA_XAUDIO2_VOICE_NOPITCH, 1.0f, &pDeviceStateXAudio2->voiceCallback, NULL, NULL);
    }

    if (FAILED(hr)) {
        result = MA_FORMAT_NOT_SUPPORTED;
        goto error_4;
    }

    pDescriptorPlayback->format             = format;
    pDescriptorPlayback->channels           = channels;
    pDescriptorPlayback->sampleRate         = sampleRate;
    pDescriptorPlayback->periodSizeInFrames = periodSizeInFrames;
    pDescriptorPlayback->periodCount        = periodCount;
    ma_channel_map_copy(pDescriptorPlayback->channelMap, channelMap, channels);

    *ppDeviceState = pDeviceStateXAudio2;

    return MA_SUCCESS;

error_4:
    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2_27* pXAudio2Legacy = (ma_IXAudio2_27*)pXAudio2;
        pXAudio2Legacy->lpVtbl->UnregisterForCallbacks(pXAudio2Legacy, &pDeviceStateXAudio2->engineCallback);
    } else {
        ma_IXAudio2* pXAudio2Modern = (ma_IXAudio2*)pXAudio2;
        pXAudio2Modern->lpVtbl->UnregisterForCallbacks(pXAudio2Modern, &pDeviceStateXAudio2->engineCallback);
    }
error_3:
    ma_event_uninit(&pDeviceStateXAudio2->event);
error_2:
    ma_free(pDeviceStateXAudio2, ma_device_get_allocation_callbacks(pDevice));
error_1:
    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2Voice_27* pMasteringVoiceLegacy = (ma_IXAudio2Voice_27*)pMasteringVoice;
        pMasteringVoiceLegacy->lpVtbl->DestroyVoice(pMasteringVoiceLegacy);
    } else {
        ma_IXAudio2Voice* pMasteringVoiceModern = (ma_IXAudio2Voice*)pMasteringVoice;
        pMasteringVoiceModern->lpVtbl->DestroyVoice(pMasteringVoiceModern);
    }
error_0:
    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2_27* pXAudio2Legacy = (ma_IXAudio2_27*)pXAudio2;
        pXAudio2Legacy->lpVtbl->Release(pXAudio2Legacy);
    } else {
        ma_IXAudio2* pXAudio2Modern = (ma_IXAudio2*)pXAudio2;
        pXAudio2Modern->lpVtbl->Release(pXAudio2Modern);
    }

    return result;
}

static void ma_device_uninit__xaudio2(ma_device* pDevice)
{
    ma_device_state_xaudio2* pDeviceStateXAudio2 = ma_device_get_backend_state__xaudio2(pDevice);
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(ma_device_get_context(pDevice));

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2_27* pXAudio2 = (ma_IXAudio2_27*)pDeviceStateXAudio2->pXAudio2;
        ma_IXAudio2Voice_27* pSourceVoice = (ma_IXAudio2Voice_27*)pDeviceStateXAudio2->pSourceVoice;
        ma_IXAudio2Voice_27* pMasteringVoice = (ma_IXAudio2Voice_27*)pDeviceStateXAudio2->pMasteringVoice;

        pSourceVoice->lpVtbl->DestroyVoice(pSourceVoice);
        pMasteringVoice->lpVtbl->DestroyVoice(pMasteringVoice);
        pXAudio2->lpVtbl->UnregisterForCallbacks(pXAudio2, &pDeviceStateXAudio2->engineCallback);
        pXAudio2->lpVtbl->Release(pXAudio2);
    } else {
        ma_IXAudio2* pXAudio2 = (ma_IXAudio2*)pDeviceStateXAudio2->pXAudio2;
        ma_IXAudio2Voice* pSourceVoice = (ma_IXAudio2Voice*)pDeviceStateXAudio2->pSourceVoice;
        ma_IXAudio2Voice* pMasteringVoice = (ma_IXAudio2Voice*)pDeviceStateXAudio2->pMasteringVoice;

        pSourceVoice->lpVtbl->DestroyVoice(pSourceVoice);
        pMasteringVoice->lpVtbl->DestroyVoice(pMasteringVoice);
        pXAudio2->lpVtbl->UnregisterForCallbacks(pXAudio2, &pDeviceStateXAudio2->engineCallback);
        pXAudio2->lpVtbl->Release(pXAudio2);
    }

    ma_event_uninit(&pDeviceStateXAudio2->event);
    ma_free(pDeviceStateXAudio2, ma_device_get_allocation_callbacks(pDevice));
}

static void ma_device_get_source_voice_state__xaudio2(ma_device* pDevice, MA_XAUDIO2_VOICE_STATE* pVoiceState)
{
    ma_device_state_xaudio2* pDeviceStateXAudio2 = ma_device_get_backend_state__xaudio2(pDevice);
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(ma_device_get_context(pDevice));

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2Voice_27* pSourceVoice = (ma_IXAudio2Voice_27*)pDeviceStateXAudio2->pSourceVoice;
        ma_IXAudio2SourceVoice_27_Vtbl* pVTable = (ma_IXAudio2SourceVoice_27_Vtbl*)pSourceVoice->lpVtbl;

        pVTable->GetState(pSourceVoice, pVoiceState);
    } else {
        ma_IXAudio2Voice* pSourceVoice = (ma_IXAudio2Voice*)pDeviceStateXAudio2->pSourceVoice;
        ma_IXAudio2SourceVoice_Vtbl* pVTable = (ma_IXAudio2SourceVoice_Vtbl*)pSourceVoice->lpVtbl;

        pVTable->GetState(pSourceVoice, pVoiceState, MA_XAUDIO2_VOICE_NOSAMPLESPLAYED);
    }
}

static ma_result ma_device_start__xaudio2(ma_device* pDevice)
{
    ma_device_state_xaudio2* pDeviceStateXAudio2 = ma_device_get_backend_state__xaudio2(pDevice);
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(ma_device_get_context(pDevice));
    HRESULT hr;

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2Voice_27* pSourceVoice = (ma_IXAudio2Voice_27*)pDeviceStateXAudio2->pSourceVoice;
        ma_IXAudio2SourceVoice_27_Vtbl* pVTable = (ma_IXAudio2SourceVoice_27_Vtbl*)pSourceVoice->lpVtbl;

        hr = pVTable->Start(pSourceVoice, 0, 0);
    } else {
        ma_IXAudio2Voice* pSourceVoice = (ma_IXAudio2Voice*)pDeviceStateXAudio2->pSourceVoice;
        ma_IXAudio2SourceVoice_Vtbl* pVTable = (ma_IXAudio2SourceVoice_Vtbl*)pSourceVoice->lpVtbl;

        hr = pVTable->Start(pSourceVoice, 0, 0);
    }

    if (FAILED(hr)) {
        return MA_FAILED_TO_START_BACKEND_DEVICE;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stop__xaudio2(ma_device* pDevice)
{
    ma_device_state_xaudio2* pDeviceStateXAudio2 = ma_device_get_backend_state__xaudio2(pDevice);
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(ma_device_get_context(pDevice));
    MA_XAUDIO2_VOICE_STATE voiceState;
    HRESULT hr;

    while (SUCCEEDED(InterlockedCompareExchange((LONG*)&pDeviceStateXAudio2->error, 0, 0))) {
        ma_device_get_source_voice_state__xaudio2(pDevice, &voiceState);
        if (voiceState.BuffersQueued == 0) {
            break;
        }

        Sleep(1);
    }

    if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
        ma_IXAudio2Voice_27* pSourceVoice = (ma_IXAudio2Voice_27*)pDeviceStateXAudio2->pSourceVoice;
        ma_IXAudio2SourceVoice_27_Vtbl* pVTable = (ma_IXAudio2SourceVoice_27_Vtbl*)pSourceVoice->lpVtbl;

        hr = pVTable->Stop(pSourceVoice, 0, 0);
    } else {
        ma_IXAudio2Voice* pSourceVoice = (ma_IXAudio2Voice*)pDeviceStateXAudio2->pSourceVoice;
        ma_IXAudio2SourceVoice_Vtbl* pVTable = (ma_IXAudio2SourceVoice_Vtbl*)pSourceVoice->lpVtbl;

        hr = pVTable->Stop(pSourceVoice, 0, 0);
    }

    if (FAILED(hr) || FAILED(InterlockedCompareExchange((LONG*)&pDeviceStateXAudio2->error, 0, 0))) {
        ma_device_set_errored(pDevice);
        return MA_FAILED_TO_STOP_BACKEND_DEVICE;
    }

    return MA_SUCCESS;
}

static ma_result ma_device_step__xaudio2(ma_device* pDevice, ma_blocking_mode blockingMode)
{
    ma_device_state_xaudio2* pDeviceStateXAudio2 = ma_device_get_backend_state__xaudio2(pDevice);
    ma_context_state_xaudio2* pContextStateXAudio2 = ma_context_get_backend_state__xaudio2(ma_device_get_context(pDevice));
    MA_XAUDIO2_VOICE_STATE voiceState;
    MA_XAUDIO2_BUFFER buffer;
    ma_uint32 availableBufferCount;
    ma_uint32 iBuffer;
    ma_result result;
    HRESULT hr;

    MA_XAUDIO2_ZERO_MEMORY(&buffer, sizeof(buffer));
    buffer.AudioBytes = pDevice->playback.internalPeriodSizeInFrames * ma_get_bytes_per_frame(pDevice->playback.internalFormat, pDevice->playback.internalChannels);

    while (ma_device_is_started(pDevice)) {
        if (FAILED(InterlockedCompareExchange((LONG*)&pDeviceStateXAudio2->error, 0, 0))) {
            ma_device_set_errored(pDevice);
            return MA_ERROR;
        }

        ma_device_get_source_voice_state__xaudio2(pDevice, &voiceState);

        availableBufferCount = pDevice->playback.internalPeriods - voiceState.BuffersQueued;
        if (availableBufferCount > 0) {
            for (iBuffer = 0; iBuffer < availableBufferCount; iBuffer += 1) {
                buffer.pAudioData = (const ma_uint8*)ma_offset_ptr(pDeviceStateXAudio2->pBuffer, buffer.AudioBytes * pDeviceStateXAudio2->iNextBuffer);

                result = ma_device_handle_backend_data_callback(pDevice, (void*)buffer.pAudioData, NULL, pDevice->playback.internalPeriodSizeInFrames);
                if (result != MA_SUCCESS) {
                    return result;
                }

                if (pContextStateXAudio2->version == ma_xaudio2_version_27) {
                    ma_IXAudio2Voice_27* pSourceVoice = (ma_IXAudio2Voice_27*)pDeviceStateXAudio2->pSourceVoice;
                    ma_IXAudio2SourceVoice_27_Vtbl* pVTable = (ma_IXAudio2SourceVoice_27_Vtbl*)pSourceVoice->lpVtbl;

                    hr = pVTable->SubmitSourceBuffer(pSourceVoice, &buffer, NULL);
                } else {
                    ma_IXAudio2Voice* pSourceVoice = (ma_IXAudio2Voice*)pDeviceStateXAudio2->pSourceVoice;
                    ma_IXAudio2SourceVoice_Vtbl* pVTable = (ma_IXAudio2SourceVoice_Vtbl*)pSourceVoice->lpVtbl;

                    hr = pVTable->SubmitSourceBuffer(pSourceVoice, &buffer, NULL);
                }

                if (FAILED(hr)) {
                    return MA_ERROR;
                }

                pDeviceStateXAudio2->iNextBuffer = (pDeviceStateXAudio2->iNextBuffer + 1) % pDevice->playback.internalPeriods;
            }

            break;
        }

        if (blockingMode == MA_BLOCKING_MODE_NON_BLOCKING) {
            break;
        }

        result = ma_event_wait(&pDeviceStateXAudio2->event);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    return MA_SUCCESS;
}

static void ma_device_wakeup__xaudio2(ma_device* pDevice)
{
    ma_device_state_xaudio2* pDeviceStateXAudio2 = ma_device_get_backend_state__xaudio2(pDevice);

    ma_event_signal(&pDeviceStateXAudio2->event);
}

static ma_device_backend_vtable ma_gDeviceBackendVTable_XAudio2 =
{
    ma_backend_info__xaudio2,
    ma_context_init__xaudio2,
    ma_context_uninit__xaudio2,
    ma_context_enumerate_devices__xaudio2,
    ma_device_init__xaudio2,
    ma_device_uninit__xaudio2,
    ma_device_start__xaudio2,
    ma_device_stop__xaudio2,
    ma_device_step__xaudio2,
    ma_device_wakeup__xaudio2
};

ma_device_backend_vtable* ma_device_backend_xaudio2 = &ma_gDeviceBackendVTable_XAudio2;
#else
ma_device_backend_vtable* ma_device_backend_xaudio2 = NULL;
#endif /* MA_HAS_XAUDIO2 */

MA_API ma_device_backend_vtable* ma_xaudio2_get_vtable(void)
{
    return ma_device_backend_xaudio2;
}

MA_API ma_context_config_xaudio2 ma_context_config_xaudio2_init(void)
{
    ma_context_config_xaudio2 config;

    config.version = ma_xaudio2_version_default;

    return config;
}

MA_API ma_device_config_xaudio2 ma_device_config_xaudio2_init(void)
{
    ma_device_config_xaudio2 config;

    config._unused = 0;

    return config;
}

#endif /* miniaudio_backend_xaudio2_c */
