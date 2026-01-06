/*
The SDL2 backend does not require any user data, nor configs. Configs are provided here in case
they are needed in the future, however you can safely pass in NULL when setting up your context
and device configs.
*/
#ifndef miniaudio_backend_sdl2_h
#define miniaudio_backend_sdl2_h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int _unused;
} ma_context_config_sdl2;

MA_API ma_context_config_sdl2 ma_context_config_sdl2_init(void);


typedef struct
{
    int _unused;
} ma_device_config_sdl2;

MA_API ma_device_config_sdl2 ma_device_config_sdl2_init(void);


extern ma_device_backend_vtable* ma_device_backend_sdl2;
MA_API ma_device_backend_vtable* ma_sdl2_get_vtable(void);

#ifdef __cplusplus
}
#endif
#endif /* miniaudio_backend_sdl_h */
