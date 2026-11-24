#ifndef miniaudio_backend_xaudio2_h
#define miniaudio_backend_xaudio2_h

#include "../../../miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ma_xaudio2_version_default = 0,
    ma_xaudio2_version_27 = 27,
    ma_xaudio2_version_28 = 28,
    ma_xaudio2_version_29 = 29
} ma_xaudio2_version;

typedef struct
{
    ma_xaudio2_version version; /* Use a specific version of XAudio2. Only applies to Windows with runtime linking. Will be ignored with compile-time linking because the version depends on the library you're linking with. */
} ma_context_config_xaudio2;

MA_API ma_context_config_xaudio2 ma_context_config_xaudio2_init(void);


typedef struct
{
    int _unused;
} ma_device_config_xaudio2;

MA_API ma_device_config_xaudio2 ma_device_config_xaudio2_init(void);


extern ma_device_backend_vtable* ma_device_backend_xaudio2;
MA_API ma_device_backend_vtable* ma_xaudio2_get_vtable(void);

#ifdef __cplusplus
}
#endif
#endif /* miniaudio_backend_xaudio2_h */
