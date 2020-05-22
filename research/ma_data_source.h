#ifndef miniaudio_data_source_h
#define miniaudio_data_source_h

#ifdef __cplusplus
extern "C" {
#endif

/*
The raw data source. This is the dry mix.
*/
typedef enum
{
    ma_data_source_type_callbacks,
    ma_data_source_type_decoder,
    ma_data_source_type_waveform,
    ma_data_source_type_noise
} ma_data_source_type;

typedef struct
{
    void* pUserData;
    ma_uint64 (* onRead)(void* pUserData, void* pFramesOut, ma_uint64 frameCount);
    ma_result (* onSeek)(void* pUserData, ma_uint64 targetFrame);
} ma_data_source_callbacks;

typedef struct
{
    ma_data_source_type type;
    ma_data_source_callbacks callbacks;
    ma_decoder_config decoder;
    ma_waveform_config waveform;
    ma_noise_config noise;
    ma_allocation_callbacks allocationCallbacks;
} ma_data_source_config;

typedef struct
{
    ma_data_source_type type;
    ma_data_source_callbacks callbacks;
    union
    {
        ma_decoder decoder;
        ma_waveform waveform;
        ma_noise noise;
    } source;
} ma_data_source;

ma_result ma_data_source_init_callbacks(const ma_data_source_config* pConfig);
ma_result ma_data_source_init_decoder(ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek, void* pUserData, const ma_data_source_config* pConfig, ma_data_source* pDataSource);
ma_result ma_data_source_init_decoder_file(const char* pFilePath, const ma_data_source_config* pConfig, ma_data_source* pDataSource);
ma_result ma_data_source_init_decoder_memory(const void* pData, size_t dataSize, const ma_data_source_config* pConfig, ma_data_source* pDataSource);
ma_result ma_data_source_init_waveform(const ma_data_source_config* pConfig, ma_data_source* pDataSource);
ma_result ma_data_source_init_noise(const ma_data_source_config* pConfig, ma_data_source* pDataSource);
ma_uint64 ma_data_source_read_pcm_frames(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount);
ma_result ma_data_source_seek_to_pcm_frame(ma_data_source* pDataSource, ma_uint64 targetPCMFrame);

#ifdef __cplusplus
}
#endif
#endif  /* miniaudio_data_source_h */
