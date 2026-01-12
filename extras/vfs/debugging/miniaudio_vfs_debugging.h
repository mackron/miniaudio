#ifndef miniaudio_vfs_debugging_h
#define miniaudio_vfs_debugging_h

#include "../../../miniaudio.h"

/*
This is a VFS for debugging purposes. I use it for things like artificial latency.
*/
typedef struct ma_vfs_debugging_config
{
    ma_vfs* pUnderlyingVFS;             /* The underlying VFS to which all calls are forwarded. */
    ma_uint32 latencyInMilliseconds;    /* The amount of latency to introduce in milliseconds. This will be done with a sleep every read. */
} ma_vfs_debugging_config;

MA_API ma_vfs_debugging_config ma_vfs_debugging_config_init(ma_vfs* pUnderlyingVFS, ma_uint32 latencyInMilliseconds);


typedef struct ma_vfs_debugging
{
    ma_vfs_callbacks cb;    /* Must be first. */
    ma_vfs_debugging_config config;
} ma_vfs_debugging;

MA_API ma_result ma_vfs_debugging_init(const ma_vfs_debugging_config* pConfig, ma_vfs_debugging* pVFS);


#endif  /* miniaudio_vfs_debugging_h */
