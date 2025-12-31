#ifndef miniaudio_backend_pipewire_h
#define miniaudio_backend_pipewire_h

#include "../../../miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ma_device_backend_vtable* ma_device_backend_pipewire;


typedef struct
{
    int _unused;
} ma_context_config_pipewire;

MA_API ma_context_config_pipewire ma_context_config_pipewire_init(void);


typedef struct
{
    const char* pThreadName;    /* If NULL, defaults to "miniaudio-pipewire". */
    const char* pStreamName;    /* If NULL, defaults to "miniaudio" */
    const char* pMediaRole;     /* If NULL, defaults to "Game". */
} ma_device_config_pipewire;

MA_API ma_device_config_pipewire ma_device_config_pipewire_init(void);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_backend_pipewire_h */
