/*
SDL3 backend. Requires SDL 3.2.0 or later. Context and device backend configs may be NULL.
Initialize the context on the main thread, as required by SDL_InitSubSystem().

Like SDL2, this backend initializes and shuts down only the audio subsystem. The application owns
SDL's global lifetime.

Device initialization sets SDL_AUDIO_DEVICE_SAMPLE_FRAMES at normal hint priority. This hint is
process-wide. SDL environment settings and override-priority hints take precedence.

On Emscripten, multi-threaded mode requires pthreads. Without threading, the application must call
ma_device_step() regularly from its browser main loop. Single-threaded stepping does not block.
*/
#ifndef miniaudio_backend_sdl3_h
#define miniaudio_backend_sdl3_h

#include "../../../miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int _unused;
} ma_context_config_sdl3;

MA_API ma_context_config_sdl3 ma_context_config_sdl3_init(void);


typedef struct
{
    int _unused;
} ma_device_config_sdl3;

MA_API ma_device_config_sdl3 ma_device_config_sdl3_init(void);


extern ma_device_backend_vtable* ma_device_backend_sdl3;
MA_API ma_device_backend_vtable* ma_sdl3_get_vtable(void);

#ifdef __cplusplus
}
#endif
#endif /* miniaudio_backend_sdl3_h */
