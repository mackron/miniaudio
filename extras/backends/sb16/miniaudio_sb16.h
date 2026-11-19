/*
Sound Blaster 16 backend for DOS with DJGPP.
*/
#ifndef miniaudio_backend_sb16_h
#define miniaudio_backend_sb16_h

#include "../../../miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int _unused;
} ma_context_config_sb16;

MA_API ma_context_config_sb16 ma_context_config_sb16_init(void);


typedef struct
{
    int _unused;
} ma_device_config_sb16;

MA_API ma_device_config_sb16 ma_device_config_sb16_init(void);


extern ma_device_backend_vtable* ma_device_backend_sb16;
MA_API ma_device_backend_vtable* ma_sb16_get_vtable(void);

#ifdef __cplusplus
}
#endif
#endif /* miniaudio_backend_sb16_h */
