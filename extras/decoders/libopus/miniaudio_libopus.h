/*
This implements a data source that decodes Opus streams via libopus + libopusfile

This object can be plugged into any `ma_data_source_*()` API and can also be used as a custom
decoding backend. See the custom_decoder example.
*/
#ifndef miniaudio_libopus_h
#define miniaudio_libopus_h

#ifdef __cplusplus
extern "C" {
#endif

#include "../../../miniaudio.h"

typedef struct
{
    ma_data_source_base ds;     /* The libopus decoder can be used independently as a data source. */
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    void* pReadSeekTellUserData;
    ma_format format;           /* Will be either f32 or s16. */
    /*OggOpusFile**/ void* of;  /* Typed as void* so we can avoid a dependency on opusfile in the header section. */
} ma_libopus;

MA_API ma_result ma_libopus_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libopus* pOpus);
MA_API ma_result ma_libopus_init_file(const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_libopus* pOpus);
MA_API void ma_libopus_uninit(ma_libopus* pOpus, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_libopus_read_pcm_frames(ma_libopus* pOpus, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_libopus_seek_to_pcm_frame(ma_libopus* pOpus, ma_uint64 frameIndex);
MA_API ma_result ma_libopus_get_data_format(ma_libopus* pOpus, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_libopus_get_cursor_in_pcm_frames(ma_libopus* pOpus, ma_uint64* pCursor);
MA_API ma_result ma_libopus_get_length_in_pcm_frames(ma_libopus* pOpus, ma_uint64* pLength);

/* Decoding backend vtable. This is what you'll plug into ma_decoder_config.pBackendVTables. No user data required. */
extern ma_decoding_backend_vtable* ma_decoding_backend_libopus;

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_libopus_h */

