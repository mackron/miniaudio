/*
Demonstrates how to implement a custom decoder.

This example implements two custom decoders:

  * Vorbis via libvorbis
  * Opus via libopus

A custom decoder must implement a data source. In this example, the libvorbis data source is called
`ma_libvorbis` and the Opus data source is called `ma_libopus`. These two objects are compatible
with the `ma_data_source` APIs and can be taken straight from this example and used in real code.

The custom decoding data sources (`ma_libvorbis` and `ma_libopus` in this example) are connected to
the decoder via the decoder config (`ma_decoder_config`). You need to implement a vtable for each
of your custom decoders. See `ma_decoding_backend_vtable` for the functions you need to implement.
The `onInitFile`, `onInitFileW` and `onInitMemory` functions are optional.
*/
#define MA_NO_VORBIS    /* Disable the built-in Vorbis decoder to ensure the libvorbis decoder is picked. */
#define MA_NO_OPUS      /* Disable the (not yet implemented) built-in Opus decoder to ensure the libopus decoder is picked. */
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"
#include "../extras/miniaudio_libvorbis.h"
#include "../extras/miniaudio_libopus.h"

#include <stdio.h>

/*
In this example we're going to be implementing our custom decoders as an extension to the ma_decoder
object. We declare our decoding backends after the ma_decoder object which allows us to avoid a
memory allocation. There are many ways to manage the backend objects so use whatever works best for
your particular scenario.
*/
typedef struct
{
    ma_decoder base;    /* Must be the first member so we can cast between ma_decoder_ex and ma_decoder. */
    union
    {
        ma_libvorbis libvorbis;
        ma_libopus   libopus;
    } backends;
} ma_decoder_ex;

static ma_result ma_decoder_ex_init__libvorbis(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_decoder_ex* pDecoderEx = (ma_decoder_ex*)pUserData;
    ma_result result;

    /*
    NOTE: We don't need to allocate any memory for the libvorbis object here because our backend
    data is just extended off the ma_decoder object (ma_decode_ex) which is passed in as a
    parameter to this function. We therefore need only cast to ma_decoder_ex and reference data
    directly from that structure.
    */
    *ppBackend = (ma_data_source*)&pDecoderEx->backends.libvorbis;

    result = ma_libvorbis_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, &pDecoderEx->backends.libvorbis);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoder_ex_init_file__libvorbis(void* pUserData, const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_decoder_ex* pDecoderEx = (ma_decoder_ex*)pUserData;
    ma_result result;

    *ppBackend = (ma_data_source*)&pDecoderEx->backends.libvorbis;

    result = ma_libvorbis_init_file(pFilePath, pConfig, pAllocationCallbacks, &pDecoderEx->backends.libvorbis);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_decoder_ex_uninit__libvorbis(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_libvorbis* pVorbis = (ma_libvorbis*)pBackend;
    ma_libvorbis_uninit(pVorbis, pAllocationCallbacks);

    /* No need to free the pVorbis object because it is sitting in the containing ma_decoder_ex object. */

    (void)pUserData;
}

static ma_result ma_decoder_ex_get_channel_map__libvorbis(void* pUserData, ma_data_source* pBackend, ma_channel* pChannelMap, size_t channelMapCap)
{
    ma_libvorbis* pVorbis = (ma_libvorbis*)pBackend;

    (void)pUserData;

    return ma_libvorbis_get_data_format(pVorbis, NULL, NULL, NULL, pChannelMap, channelMapCap);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_libvorbis =
{
    ma_decoder_ex_init__libvorbis,
    ma_decoder_ex_init_file__libvorbis,
    NULL, /* onInitFileW() */
    NULL, /* onInitMemory() */
    ma_decoder_ex_uninit__libvorbis,
    ma_decoder_ex_get_channel_map__libvorbis
};



static ma_result ma_decoder_ex_init__libopus(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_decoder_ex* pDecoderEx = (ma_decoder_ex*)pUserData;
    ma_result result;

    /*
    NOTE: We don't need to allocate any memory for the libopus object here because our backend
    data is just extended off the ma_decoder object (ma_decode_ex) which is passed in as a
    parameter to this function. We therefore need only cast to ma_decoder_ex and reference data
    directly from that structure.
    */
    *ppBackend = (ma_data_source*)&pDecoderEx->backends.libopus;

    result = ma_libopus_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, &pDecoderEx->backends.libopus);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static ma_result ma_decoder_ex_init_file__libopus(void* pUserData, const char* pFilePath, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_decoder_ex* pDecoderEx = (ma_decoder_ex*)pUserData;
    ma_result result;

    *ppBackend = (ma_data_source*)&pDecoderEx->backends.libopus;

    result = ma_libopus_init_file(pFilePath, pConfig, pAllocationCallbacks, &pDecoderEx->backends.libopus);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

static void ma_decoder_ex_uninit__libopus(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_libopus* pOpus = (ma_libopus*)pBackend;
    ma_libopus_uninit(pOpus, pAllocationCallbacks);

    /* No need to free the pOpus object because it is sitting in the containing ma_decoder_ex object. */

    (void)pUserData;
}

static ma_result ma_decoder_ex_get_channel_map__libopus(void* pUserData, ma_data_source* pBackend, ma_channel* pChannelMap, size_t channelMapCap)
{
    ma_libopus* pOpus = (ma_libopus*)pBackend;

    (void)pUserData;

    return ma_libopus_get_data_format(pOpus, NULL, NULL, NULL, pChannelMap, channelMapCap);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_libopus =
{
    ma_decoder_ex_init__libopus,
    ma_decoder_ex_init_file__libopus,
    NULL, /* onInitFileW() */
    NULL, /* onInitMemory() */
    ma_decoder_ex_uninit__libopus,
    ma_decoder_ex_get_channel_map__libopus
};




void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_data_source* pDataSource = (ma_data_source*)pDevice->pUserData;
    if (pDataSource == NULL) {
        return;
    }

    ma_data_source_read_pcm_frames(pDataSource, pOutput, frameCount, NULL, MA_TRUE);

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder_ex decoder;
    ma_device_config deviceConfig;
    ma_device device;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    /*
    Add your custom backend vtables here. The order in the array defines the order of priority. The
    vtables will be passed in via the decoder config.
    */
    ma_decoding_backend_vtable* pCustomBackendVTables[] =
    {
        &g_ma_decoding_backend_vtable_libvorbis,
        &g_ma_decoding_backend_vtable_libopus
    };


    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    
    /* Initialize the decoder. */
    decoderConfig = ma_decoder_config_init_default();
    decoderConfig.pCustomBackendUserData   = &decoder;  /* In this example our backend objects are contained within a ma_decoder_ex object to avoid a malloc. Our vtables need to know about this. */
    decoderConfig.ppCustomBackendVTables   = pCustomBackendVTables;
    decoderConfig.customBackendVTableCount = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
    
    result = ma_decoder_init_file(argv[1], &decoderConfig, &decoder.base);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.");
        return -1;
    }


    /* Initialize the device. */
    result = ma_data_source_get_data_format(&decoder, &format, &channels, &sampleRate);
    if (result != MA_SUCCESS) {
        printf("Failed to retrieve decoder data format.");
        ma_decoder_uninit(&decoder.base);
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = format;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate        = sampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &decoder;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        ma_decoder_uninit(&decoder.base);
        return -1;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder.base);
        return -1;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder.base);

    return 0;
}