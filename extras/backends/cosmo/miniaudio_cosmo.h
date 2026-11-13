/*
Cosmopolitan backend using the CosmoAudio API supplied by cosmocc.

Cosmopolitan builds require MA_THREADING_MODE_SINGLE_THREADED for all backends, including this one.
It's for this reason that this backend is not currently built into the main library.
*/
#ifndef miniaudio_backend_cosmo_h
#define miniaudio_backend_cosmo_h

#include "../../../miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int _unused;
} ma_context_config_cosmo;

MA_API ma_context_config_cosmo ma_context_config_cosmo_init(void);


typedef struct
{
    ma_bool32 enableDebugLog;  /* Enables CosmoAudio's own diagnostic output. */
} ma_device_config_cosmo;

MA_API ma_device_config_cosmo ma_device_config_cosmo_init(void);


extern ma_device_backend_vtable* ma_device_backend_cosmo;
MA_API ma_device_backend_vtable* ma_cosmo_get_vtable(void);

#ifdef __cplusplus
}
#endif
#endif /* miniaudio_backend_cosmo_h */
