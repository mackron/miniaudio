/*
IMPORTANT NOTE: Cosmopolitan is not officially supported by miniaudio. This file was added just as
a way to play around and experiment with Cosmopolitan as a proof of concept and to test the viability
of supporting such a compiler. If you get compilation or runtime errors you're on your own.

---------------------------------------------------------------------------------------------------

This is a version of windows.h for compiling with Cosmopolitan. It's not complete. It's intended to
define some missing items from cosmopolitan.h. Hopefully as the project develops we can eventually
eliminate all of the content in this file.
*/
#ifndef _WINDOWS_
#define _WINDOWS_

#define WINAPI
#define STDMETHODCALLTYPE
#define CALLBACK

typedef uint64_t                    HWND;
typedef uint64_t                    HANDLE;
typedef uint64_t                    HKEY;
typedef uint64_t                    HWAVEIN;
typedef uint64_t                    HWAVEOUT;
typedef uint32_t                    HRESULT;
typedef uint8_t                     BYTE;
typedef uint16_t                    WORD;
typedef uint32_t                    DWORD;
typedef uint64_t                    DWORDLONG;
typedef int32_t                     BOOL;
typedef int32_t                     LONG;       /* `long` is always 32-bit on Windows. */
typedef int64_t                     LONGLONG;
typedef uint32_t                    ULONG;      /* `long` is always 32-bit on Windows. */
typedef uint64_t                    ULONGLONG;
typedef char16_t                    WCHAR;
typedef unsigned int                UINT;
typedef char                        CHAR;
typedef uint64_t                    ULONG_PTR;  /* Everything is 64-bit with Cosmopolitan. */
typedef ULONG_PTR                   DWORD_PTR;

#define TRUE                        1
#define FALSE                       0

#define WAIT_OBJECT_0               0
#define INFINITE                    0xFFFFFFFF

#define CP_UTF8                     65001

#define FAILED(hr)                  ((hr) <  0)
#define SUCCEEDED(hr)               ((hr) >= 0)

#define NOERROR                     0
#define S_OK                        0
#define S_FALSE                     1
#define E_POINTER                   ((HRESULT)0x80004003)
#define E_UNEXPECTED                ((HRESULT)0x8000FFFF)
#define E_NOTIMPL                   ((HRESULT)0x80004001)
#define E_OUTOFMEMORY               ((HRESULT)0x8007000E)
#define E_INVALIDARG                ((HRESULT)0x80070057)
#define E_NOINTERFACE               ((HRESULT)0x80004002)
#define E_HANDLE                    ((HRESULT)0x80070006)
#define E_ABORT                     ((HRESULT)0x80004004)
#define E_FAIL                      ((HRESULT)0x80004005)
#define E_ACCESSDENIED              ((HRESULT)0x80070005)

#define ERROR_SUCCESS               0
#define ERROR_FILE_NOT_FOUND        2        
#define ERROR_PATH_NOT_FOUND        3
#define ERROR_TOO_MANY_OPEN_FILES   4
#define ERROR_ACCESS_DENIED         5        
#define ERROR_NOT_ENOUGH_MEMORY     8
#define ERROR_HANDLE_EOF            38
#define ERROR_INVALID_PARAMETER     87         
#define ERROR_DISK_FULL             112
#define ERROR_SEM_TIMEOUT           121
#define ERROR_NEGATIVE_SEEK         131

typedef struct
{
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} GUID, IID;

typedef int64_t LARGE_INTEGER;



#define HKEY_LOCAL_MACHINE      ((HKEY)(ULONG_PTR)(0x80000002))
#define KEY_READ                0x00020019


static HANDLE CreateEventA(struct NtSecurityAttributes* lpEventAttributes, bool32 bManualReset, bool32 bInitialState, const char* lpName)
{
    assert(lpName == NULL);  /* If this is ever triggered we'll need to do a ANSI-to-Unicode conversion. */
    return (HANDLE)CreateEvent(lpEventAttributes, bManualReset, bInitialState, (const char16_t*)lpName);
}

static BOOL IsEqualGUID(const GUID* a, const GUID* b)
{
    return memcmp(a, b, sizeof(GUID)) == 0;
}

#endif  /* _WINDOWS_ */
