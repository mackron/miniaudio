/*
Demonstrates how to plug in custom decoders.

This example implements two custom decoders:

  * Vorbis via libvorbis
  * Opus via libopus

The files miniaudio_libvorbis.h and miniaudio_libopus.h are where the custom decoders are implemented.
Refer to these files for an example of how you can implement your own custom decoders.

To wire up your custom decoders to the `ma_decoder` API, you need to set up a `ma_decoder_config`
object and fill out the `ppBackendVTables` and `backendCount` members. The `ppBackendVTables` member
is an array of pointers to `ma_decoding_backend_vtable` objects. The order of the array defines the
order of priority, with the first being the highest priority. The `backendCount` member is the number
of items in the `ppBackendVTables` array.

A custom decoder must implement a data source. In this example, the libvorbis data source is called
`ma_libvorbis` and the Opus data source is called `ma_libopus`. These two objects are compatible
with the `ma_data_source` APIs and can be taken straight from this example and used in real code.

The custom decoding data sources (`ma_libvorbis` and `ma_libopus` in this example) are connected to
the decoder via the decoder config (`ma_decoder_config`). You need to implement a vtable for each
of your custom decoders. See `ma_decoding_backend_vtable` for the functions you need to implement.
The `onInitFile`, `onInitFileW` and `onInitMemory` functions are optional.
*/

/*
For now these need to be declared before miniaudio.c due to some compatibility issues with the old
MINIAUDIO_IMPLEMENTATION system. This will change from version 0.12.
*/
#include "../extras/decoders/libvorbis/miniaudio_libvorbis.c"
#include "../extras/decoders/libopus/miniaudio_libopus.c"
#include "../miniaudio.c"

#include <stdio.h>

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_data_source* pDataSource = (ma_data_source*)pDevice->pUserData;
    if (pDataSource == NULL) {
        return;
    }

    ma_data_source_read_pcm_frames(pDataSource, pOutput, frameCount, NULL);

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    ma_device_config deviceConfig;
    ma_device device;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    /*
    Add your custom backend vtables here. The order in the array defines the order of priority, with the
    first being the highest priority. The vtables are be passed in via the decoder config. If you want to
    support stock backends in addition to custom backends, you must add the stock backend vtables here as
    well. You should list the backends in your preferred order of priority.

    The list below shows how you would set up your array to prioritize the custom decoders over the stock
    decoders. If you want to prioritize the stock decoders over the custom decoders, you would simply
    change the order.
    */
    ma_decoding_backend_vtable* pBackendVTables[] =
    {
        ma_decoding_backend_libvorbis,
        ma_decoding_backend_libopus,
        ma_decoding_backend_wav,
        ma_decoding_backend_flac,
        ma_decoding_backend_mp3
    };


    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    
    /* Initialize the decoder. */
    decoderConfig = ma_decoder_config_init_default();
    decoderConfig.ppBackendVTables = pBackendVTables;
    decoderConfig.backendCount     = sizeof(pBackendVTables) / sizeof(pBackendVTables[0]);
    
    result = ma_decoder_init_file(argv[1], &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder.");
        return -1;
    }

    ma_data_source_set_looping(&decoder, MA_TRUE);


    /* Initialize the device. */
    result = ma_data_source_get_data_format(&decoder, &format, &channels, &sampleRate, NULL, 0);
    if (result != MA_SUCCESS) {
        printf("Failed to retrieve decoder data format.");
        ma_decoder_uninit(&decoder);
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
        ma_decoder_uninit(&decoder);
        return -1;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
        return -1;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);

    return 0;
}
