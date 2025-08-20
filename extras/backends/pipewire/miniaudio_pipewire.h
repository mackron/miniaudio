/*
The PipeWire backend depends on the libspa development packages. On Debian-based distributions,
this can be installed with:

    sudo apt install libspa-0.2-dev

If using Ubuntu, this may install it in a "spa-0.2" subfolder. In this case, you might need
to add the following to your build command:

    -I/usr/include/spa-0.2

Unfortunately PipeWire has a hard dependency on the above package, and because it's made up
entirely of non-trivial inlined code, it's not possible to avoid this dependency. It's for
this reason the PipeWire backend cannot be included in miniaudio.h since it has a requirement
that it does not depend on external development packages.

The PipeWire backend cannot be used with `-std=c89`. This is because the SPA headers do not
support it.
*/
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