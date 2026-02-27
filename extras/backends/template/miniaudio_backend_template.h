#ifndef miniaudio_backend_template_h
#define miniaudio_backend_template_h

#include "../../../miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ma_device_backend_vtable* ma_device_backend_template;
MA_API ma_device_backend_vtable* ma_template_get_vtable(void);


typedef struct
{
    int _unused;
} ma_context_config_template;

MA_API ma_context_config_template ma_context_config_template_init(void);


typedef struct
{
    int _unused;
} ma_device_config_template;

MA_API ma_device_config_template ma_device_config_template_init(void);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_backend_template_h */

