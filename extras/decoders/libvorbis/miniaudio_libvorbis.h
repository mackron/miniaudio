/*
This implements a data source that decodes Vorbis streams via libvorbis + libvorbisfile

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.
*/
#ifndef miniaudio_libvorbis_h
#define miniaudio_libvorbis_h

#ifdef __cplusplus
extern "C" {
#endif

#include "../../../miniaudio.h"

typedef struct
{
    ma_data_source_base ds;     /* The libvorbis decoder can be used independently as a data source. */
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    void* pReadSeekTellUserData;
    ma_format format;           /* Will be either f32 or s16. */
    /*OggVorbis_File**/ void* vf;   /* Typed as void* so we can avoid a dependency on opusfile in the header section. */
} ma_libvorbis;

MA_API ma_result ma_libvorbis_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libvorbis* pVorbis);
MA_API ma_result ma_libvorbis_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libvorbis* pVorbis);
MA_API void ma_libvorbis_uninit(ma_libvorbis* pVorbis, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_libvorbis_read_pcm_frames(ma_libvorbis* pVorbis, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_libvorbis_seek_to_pcm_frame(ma_libvorbis* pVorbis, ma_uint64 frameIndex);
MA_API ma_result ma_libvorbis_get_data_format(ma_libvorbis* pVorbis, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_libvorbis_get_cursor_in_pcm_frames(ma_libvorbis* pVorbis, ma_uint64* pCursor);
MA_API ma_result ma_libvorbis_get_length_in_pcm_frames(ma_libvorbis* pVorbis, ma_uint64* pLength);

/* Decoding backend vtable. This is what you'll plug into ma_decoder_config.pBackendVTables. No user data required. */
extern ma_decoding_backend_vtable* ma_decoding_backend_libvorbis;

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_libvorbis_h */
