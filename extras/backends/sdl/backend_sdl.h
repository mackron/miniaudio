/*
The SDL backend does not require any user data, nor configs. Configs are provided here in case
they are needed in the future, however you can safely pass in NULL when setting up your context
and device configs.
*/
#ifndef miniaudio_backend_sdl_h
#define miniaudio_backend_sdl_h

#ifdef __cplusplus
extern "C" {
#endif

extern const ma_device_backend_vtable* MA_DEVICE_BACKEND_VTABLE_SDL;


typedef struct
{
    int _unused;
} ma_context_config_sdl;

MA_API ma_context_config_sdl ma_context_config_sdl_init(void);


typedef struct
{
    int _unused;
} ma_device_config_sdl;

MA_API ma_device_config_sdl ma_device_config_sdl_init(void);

#ifdef __cplusplus
}
#endif
#endif /* miniaudio_backend_sdl_h */
