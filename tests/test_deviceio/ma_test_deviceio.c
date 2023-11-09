/*
USAGE: ma_test_deviceio [input/output file] [mode] [backend] [waveform] [noise]

In playback mode the input file is optional, in which case a waveform or noise source will be used instead. For capture and loopback modes
it must specify an output parameter, and must be specified. In duplex mode it is optional, but if specified will be an output file that
will receive the captured audio.

"mode" can be one of the following:
    playback
    capture
    duplex
    loopback

"backend" is one of the miniaudio backends:
    wasapi
    dsound or directsound
    winmm
    coreaudio
    sndio
    audio4
    oss
    pulseaudio or pulse
    alsa
    jack
    aaudio
    opensl
    webaudio
    null

"waveform" can be one of the following:
    sine
    square
    triangle
    sawtooth

"noise" can be one of the following:
    white
    pink
    brownian or brown

If multiple backends are specified, the priority will be based on the order in which you specify them. If multiple waveform or noise types
are specified the last one on the command line will have priority.
*/
#define MA_FORCE_UWP
#include "../test_common/ma_test_common.c"

typedef enum
{
    source_type_waveform,
    source_type_noise,
    source_type_decoder
} source_type;

static struct
{
    ma_context context;
    ma_device device;
    source_type sourceType;
    ma_waveform waveform;
    ma_noise noise;
    ma_decoder decoder;
    ma_encoder encoder;
    ma_bool32 hasEncoder;   /* Used for duplex mode to determine whether or not audio data should be written to a file. */
} g_State;

const char* get_mode_description(ma_device_type deviceType)
{
    if (deviceType == ma_device_type_playback) {
        return "Playback";
    }
    if (deviceType == ma_device_type_capture) {
        return "Capture";
    }
    if (deviceType == ma_device_type_duplex) {
        return "Duplex";
    }
    if (deviceType == ma_device_type_loopback) {
        return "Loopback";
    }

    /* Should never get here. */
    return "Unknown";
}

ma_bool32 try_parse_mode(const char* arg, ma_device_type* pDeviceType)
{
    MA_ASSERT(arg         != NULL);
    MA_ASSERT(pDeviceType != NULL);

    if (strcmp(arg, "playback") == 0) {
        *pDeviceType = ma_device_type_playback;
        return MA_TRUE;
    }
    if (strcmp(arg, "capture") == 0) {
        *pDeviceType = ma_device_type_capture;
        return MA_TRUE;
    }
    if (strcmp(arg, "duplex") == 0) {
        *pDeviceType = ma_device_type_duplex;
        return MA_TRUE;
    }
    if (strcmp(arg, "loopback") == 0) {
        *pDeviceType = ma_device_type_loopback;
        return MA_TRUE;
    }

    return MA_FALSE;
}

ma_bool32 try_parse_backend(const char* arg, ma_backend* pBackends, ma_uint32 backendCap, ma_uint32* pBackendCount)
{
    ma_uint32 backendCount;

    MA_ASSERT(arg           != NULL);
    MA_ASSERT(pBackends     != NULL);
    MA_ASSERT(pBackendCount != NULL);

    backendCount = *pBackendCount;
    if (backendCount == backendCap) {
        return MA_FALSE; /* No more room. */
    }

    if (strcmp(arg, "wasapi") == 0) {
        pBackends[backendCount++] = ma_backend_wasapi;
        goto done;
    }
    if (strcmp(arg, "dsound") == 0 || strcmp(arg, "directsound") == 0) {
        pBackends[backendCount++] = ma_backend_dsound;
        goto done;
    }
    if (strcmp(arg, "winmm") == 0) {
        pBackends[backendCount++] = ma_backend_winmm;
        goto done;
    }
    if (strcmp(arg, "coreaudio") == 0) {
        pBackends[backendCount++] = ma_backend_coreaudio;
        goto done;
    }
    if (strcmp(arg, "sndio") == 0) {
        pBackends[backendCount++] = ma_backend_sndio;
        goto done;
    }
    if (strcmp(arg, "audio4") == 0) {
        pBackends[backendCount++] = ma_backend_audio4;
        goto done;
    }
    if (strcmp(arg, "oss") == 0) {
        pBackends[backendCount++] = ma_backend_oss;
        goto done;
    }
    if (strcmp(arg, "pulseaudio") == 0 || strcmp(arg, "pulse") == 0) {
        pBackends[backendCount++] = ma_backend_pulseaudio;
        goto done;
    }
    if (strcmp(arg, "alsa") == 0) {
        pBackends[backendCount++] = ma_backend_alsa;
        goto done;
    }
    if (strcmp(arg, "jack") == 0) {
        pBackends[backendCount++] = ma_backend_jack;
        goto done;
    }
    if (strcmp(arg, "aaudio") == 0) {
        pBackends[backendCount++] = ma_backend_aaudio;
        goto done;
    }
    if (strcmp(arg, "opensl") == 0) {
        pBackends[backendCount++] = ma_backend_opensl;
        goto done;
    }
    if (strcmp(arg, "webaudio") == 0) {
        pBackends[backendCount++] = ma_backend_webaudio;
        goto done;
    }
    if (strcmp(arg, "null") == 0) {
        pBackends[backendCount++] = ma_backend_null;
        goto done;
    }

done:
    if (*pBackendCount < backendCount) {
        *pBackendCount = backendCount;
        return MA_TRUE;     /* This argument was a backend. */
    } else {
        return MA_FALSE;    /* This argument was not a backend. */
    }
}

ma_bool32 try_parse_waveform(const char* arg, ma_waveform_type* pWaveformType)
{
    MA_ASSERT(arg           != NULL);
    MA_ASSERT(pWaveformType != NULL);

    if (strcmp(arg, "sine") == 0) {
        *pWaveformType = ma_waveform_type_sine;
        return MA_TRUE;
    }
    if (strcmp(arg, "square") == 0) {
        *pWaveformType = ma_waveform_type_square;
        return MA_TRUE;
    }
    if (strcmp(arg, "triangle") == 0) {
        *pWaveformType = ma_waveform_type_triangle;
        return MA_TRUE;
    }
    if (strcmp(arg, "sawtooth") == 0) {
        *pWaveformType = ma_waveform_type_sawtooth;
        return MA_TRUE;
    }

    return MA_FALSE;
}

ma_bool32 try_parse_noise(const char* arg, ma_noise_type* pNoiseType)
{
    MA_ASSERT(arg        != NULL);
    MA_ASSERT(pNoiseType != NULL);

    if (strcmp(arg, "white") == 0) {
        *pNoiseType = ma_noise_type_white;
        return MA_TRUE;
    }
    if (strcmp(arg, "pink") == 0) {
        *pNoiseType = ma_noise_type_pink;
        return MA_TRUE;
    }
    if (strcmp(arg, "brownian") == 0 || strcmp(arg, "brown") == 0) {
        *pNoiseType = ma_noise_type_brownian;
        return MA_TRUE;
    }

    return MA_FALSE;
}

ma_result print_device_info(ma_context* pContext, ma_device_type deviceType, const ma_device_info* pDeviceInfo)
{
    ma_result result;
    ma_device_info detailedDeviceInfo;

    MA_ASSERT(pDeviceInfo != NULL);

#if 1
    result = ma_context_get_device_info(pContext, deviceType, &pDeviceInfo->id, &detailedDeviceInfo);
    if (result != MA_SUCCESS) {
        return result;
    }
#else
    detailedDeviceInfo = *pDeviceInfo;
#endif

    {
        ma_uint32 iFormat;

        printf("%s\n", pDeviceInfo->name);
        printf("    Default:      %s\n", (detailedDeviceInfo.isDefault) ? "Yes" : "No");
        printf("    Format Count: %d\n", detailedDeviceInfo.nativeDataFormatCount);
        for (iFormat = 0; iFormat < detailedDeviceInfo.nativeDataFormatCount; ++iFormat) {
            printf("        %s, %d, %d\n", ma_get_format_name(detailedDeviceInfo.nativeDataFormats[iFormat].format), detailedDeviceInfo.nativeDataFormats[iFormat].channels, detailedDeviceInfo.nativeDataFormats[iFormat].sampleRate);
        }
    }

    return MA_SUCCESS;
}

ma_result enumerate_devices(ma_context* pContext)
{
    ma_result result;
    ma_device_info* pPlaybackDevices;
    ma_uint32 playbackDeviceCount;
    ma_device_info* pCaptureDevices;
    ma_uint32 captureDeviceCount;
    ma_uint32 iDevice;

    MA_ASSERT(pContext != NULL);

    result = ma_context_get_devices(pContext, &pPlaybackDevices, &playbackDeviceCount, &pCaptureDevices, &captureDeviceCount);
    if (result != MA_SUCCESS) {
        return result;
    }

    printf("Playback Devices\n");
    printf("----------------\n");
    for (iDevice = 0; iDevice < playbackDeviceCount; iDevice += 1) {
        printf("%d: ", iDevice);
        print_device_info(pContext, ma_device_type_playback, &pPlaybackDevices[iDevice]);
    }
    printf("\n");

    printf("Capture Devices\n");
    printf("---------------\n");
    for (iDevice = 0; iDevice < captureDeviceCount; iDevice += 1) {
        printf("%d: ", iDevice);
        print_device_info(pContext, ma_device_type_capture, &pCaptureDevices[iDevice]);
    }
    printf("\n");

    return MA_SUCCESS;
}

void on_log(void* pUserData, ma_uint32 logLevel, const char* message)
{
    (void)pUserData;

    printf("%s: %s", ma_log_level_to_string(logLevel), message);
}

void on_notification(const ma_device_notification* pNotification)
{
    MA_ASSERT(pNotification != NULL);
    
    switch (pNotification->type)
    {
        case ma_device_notification_type_started:
        {
            printf("Started\n");
        } break;

        case ma_device_notification_type_stopped:
        {
            printf("Stopped\n");
        } break;

        case ma_device_notification_type_rerouted:
        {
            printf("Rerouted\n");
        } break;

        case ma_device_notification_type_interruption_began:
        {
            printf("Interruption Began\n");
        } break;

        case ma_device_notification_type_interruption_ended:
        {
            printf("Interruption Ended\n");
        } break;

        default: break;
    }
}

void on_data(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    switch (pDevice->type)
    {
        case ma_device_type_playback:
        {
            /*
            In the playback case we just read from our input source. We're going to use ma_data_source_read_pcm_frames() for this
            to ensure the data source abstraction is working properly for each type. */
            if (g_State.sourceType == source_type_decoder) {
                ma_data_source_read_pcm_frames(&g_State.decoder, pFramesOut, frameCount, NULL);
            } else if (g_State.sourceType == source_type_waveform) {
                ma_data_source_read_pcm_frames(&g_State.waveform, pFramesOut, frameCount, NULL);
            } else if (g_State.sourceType == source_type_noise) {
                ma_data_source_read_pcm_frames(&g_State.noise, pFramesOut, frameCount, NULL);
            }
        } break;

        case ma_device_type_capture:
        case ma_device_type_loopback:
        {
            /* In the capture and loopback cases we just output straight to a file. */
            ma_encoder_write_pcm_frames(&g_State.encoder, pFramesIn, frameCount, NULL);
        } break;

        case ma_device_type_duplex:
        {
            /* The duplex case is easy. We just move from pFramesIn to pFramesOut. */
            MA_ASSERT(pDevice->playback.format == pDevice->capture.format);
            MA_ASSERT(pDevice->playback.channels == pDevice->capture.channels);
            MA_COPY_MEMORY(pFramesOut, pFramesIn, ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels) * frameCount);

            /* Also output to the encoder if necessary. */
            if (g_State.hasEncoder) {
                ma_encoder_write_pcm_frames(&g_State.encoder, pFramesIn, frameCount, NULL);
            }
        } break;

        default:
        {
            /* Should never hit this. */
            MA_ASSERT(MA_FALSE);
        } break;
    }
}

int main(int argc, char** argv)
{
    int iarg;
    ma_result result;
    ma_backend enabledBackends[MA_BACKEND_COUNT];
    size_t enabledBackendCount;
    size_t iEnabledBackend;
    ma_backend backends[MA_BACKEND_COUNT];
    ma_uint32 backendCount = 0;
    ma_context_config contextConfig;
    ma_device_type deviceType = ma_device_type_playback;
    ma_format deviceFormat = ma_format_unknown;
    ma_uint32 deviceChannels = 0;
    ma_uint32 deviceSampleRate = 0;
    ma_device_config deviceConfig;
    ma_waveform_type waveformType = ma_waveform_type_sine;
    ma_noise_type noiseType = ma_noise_type_white;
    const char* pFilePath = NULL;  /* Input or output file path, depending on the mode. */
    ma_bool32 enumerate = MA_TRUE;

    /* Default to a sine wave if nothing is passed into the command line. */
    waveformType = ma_waveform_type_sine;
    g_State.sourceType = source_type_waveform;

    /* We need to iterate over the command line arguments and gather our settings. */
    for (iarg = 1; iarg < argc; iarg += 1) {
        /* mode */
        if (try_parse_mode(argv[iarg], &deviceType)) {
            continue;
        }

        /* backend */
        if (try_parse_backend(argv[iarg], backends, ma_countof(backends), &backendCount)) {
            continue;
        }

        /* waveform */
        if (try_parse_waveform(argv[iarg], &waveformType)) {
            g_State.sourceType = source_type_waveform;
            continue;
        }
        
        /* noise */
        if (try_parse_noise(argv[iarg], &noiseType)) {
            g_State.sourceType = source_type_noise;
            continue;
        }

        /* Getting here means the argument should be considered the input or output file. */
        pFilePath = argv[iarg];
        g_State.sourceType = source_type_decoder;
    }

    /* Here we'll quickly print the available backends. */
    printf("Enabled Backends:\n");
    result = ma_get_enabled_backends(enabledBackends, ma_countof(enabledBackends), &enabledBackendCount);
    if (result != MA_SUCCESS) {
        printf("Failed to retrieve available backends.\n");
        return -1;
    }

    for (iEnabledBackend = 0; iEnabledBackend < enabledBackendCount; iEnabledBackend += 1) {
        printf("    %s\n", ma_get_backend_name(enabledBackends[iEnabledBackend]));
    }
    printf("\n");


    /* Initialize the context first. If no backends were passed into the command line we just use defaults. */
    contextConfig = ma_context_config_init();
    result = ma_context_init((backendCount == 0) ? NULL : backends, backendCount, &contextConfig, &g_State.context);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize context.\n");
        return -1;
    }

    ma_log_register_callback(ma_context_get_log(&g_State.context), ma_log_callback_init(on_log, NULL));

    /* Here we'll print some info about what we're doing. */
    printf("Backend: %s\n", ma_get_backend_name(g_State.context.backend));
    printf("Mode:    %s\n", get_mode_description(deviceType));
    printf("\n");

    /* Enumerate if required. */
    if (enumerate) {
        enumerate_devices(&g_State.context);
    }

    /*
    Now that the context has been initialized we can do the device. In duplex mode we want to use the same format for both playback and capture so we don't need
    to do any data conversion between the two.
    */
    if (deviceType == ma_device_type_duplex) {
        if (deviceFormat == ma_format_unknown) {
            deviceFormat = ma_format_f32;
        }
        if (deviceChannels == 0) {
            deviceChannels = 0;
        }
        if (deviceSampleRate == 0) {
            deviceSampleRate = 48000;
        }
    }

    deviceConfig = ma_device_config_init(deviceType);
    deviceConfig.playback.format      = deviceFormat;
    deviceConfig.playback.channels    = deviceChannels;
    deviceConfig.capture.format       = deviceFormat;
    deviceConfig.capture.channels     = deviceChannels;
    deviceConfig.sampleRate           = deviceSampleRate;
    deviceConfig.dataCallback         = on_data;
    deviceConfig.notificationCallback = on_notification;
    result = ma_device_init(&g_State.context, &deviceConfig, &g_State.device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize device.\n");
        ma_context_uninit(&g_State.context);
        return -1;
    }

    /* We can now initialize our input and output sources. */
    if (deviceType == ma_device_type_playback) {
        if (g_State.sourceType == source_type_decoder) {
            ma_decoder_config decoderConfig;
            decoderConfig = ma_decoder_config_init(g_State.device.playback.format, g_State.device.playback.channels, g_State.device.sampleRate);
            result = ma_decoder_init_file(pFilePath, &decoderConfig, &g_State.decoder);
            if (result != MA_SUCCESS) {
                printf("Failed to open file for decoding \"%s\".\n", pFilePath);
                ma_device_uninit(&g_State.device);
                ma_context_uninit(&g_State.context);
                return -1;
            }
        }

        if (g_State.sourceType == source_type_waveform) {
            ma_waveform_config waveformConfig;
            waveformConfig = ma_waveform_config_init(g_State.device.playback.format, g_State.device.playback.channels, g_State.device.sampleRate, waveformType, 0.1, 220);
            result = ma_waveform_init(&waveformConfig, &g_State.waveform);
            if (result != MA_SUCCESS) {
                printf("Failed to initialize waveform.\n");
                ma_device_uninit(&g_State.device);
                ma_context_uninit(&g_State.context);
                return -1;
            }
        }

        if (g_State.sourceType == source_type_noise) {
            ma_noise_config noiseConfig;
            noiseConfig = ma_noise_config_init(g_State.device.playback.format, g_State.device.playback.channels, noiseType, 0, 0.1);
            result = ma_noise_init(&noiseConfig, NULL, &g_State.noise);
            if (result != MA_SUCCESS) {
                printf("Failed to initialize noise.\n");
                ma_device_uninit(&g_State.device);
                ma_context_uninit(&g_State.context);
                return -1;
            }
        }
    }

    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_loopback || (deviceType == ma_device_type_duplex && pFilePath != NULL && pFilePath[0] != '\0')) {
        ma_encoder_config encoderConfig;
        encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, g_State.device.capture.format, g_State.device.capture.channels, g_State.device.sampleRate);
        result = ma_encoder_init_file(pFilePath, &encoderConfig, &g_State.encoder);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize output file for capture \"%s\".\n", pFilePath);
            ma_device_uninit(&g_State.device);
            ma_context_uninit(&g_State.context);
            return -1;
        }

        g_State.hasEncoder = MA_TRUE;
    }


    /* Print the name of the device. */
    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
        ma_device_get_name(&g_State.device, ma_device_type_playback, name, sizeof(name), NULL);
        printf("Playback Device: %s\n", name);
    }
    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex || deviceType == ma_device_type_loopback) {
        char name[MA_MAX_DEVICE_NAME_LENGTH + 1];
        ma_device_get_name(&g_State.device, ma_device_type_capture, name, sizeof(name), NULL);
        printf("Capture Device:  %s\n", name);
    }
    

    /* Everything should be initialized at this point so we can now print our configuration and start the device. */
    result = ma_device_start(&g_State.device);
    if (result != MA_SUCCESS) {
        printf("Failed to start device.\n");
        goto done;
    }

    /* Now we just keep looping and wait for user input. */
    for (;;) {
        int c;

        if (ma_device_is_started(&g_State.device)) {
            printf("Press Q to quit, P to pause.\n");
        } else {
            printf("Press Q to quit, P to resume.\n");
        }
        
        for (;;) {
            c = getchar();
            if (c != '\n') {
                break;
            }
        }
        
        if (c == 'q' || c == 'Q') {
            break;
        }
        if (c == 'p' || c == 'P') {
            if (ma_device_is_started(&g_State.device)) {
                result = ma_device_stop(&g_State.device);
                if (result != MA_SUCCESS) {
                    printf("ERROR: Error when stopping the device: %s\n", ma_result_description(result));
                }
            } else {
                result = ma_device_start(&g_State.device);
                if (result != MA_SUCCESS) {
                    printf("ERROR: Error when starting the device: %s\n", ma_result_description(result));
                }
            }
        }
    }

done:
    ma_device_uninit(&g_State.device);
    ma_context_uninit(&g_State.context);
    if (g_State.sourceType == source_type_decoder) {
        ma_decoder_uninit(&g_State.decoder);
    }
    if (g_State.sourceType == source_type_waveform) {
        ma_waveform_uninit(&g_State.waveform);
    }
    if (g_State.sourceType == source_type_noise) {
        ma_noise_uninit(&g_State.noise, NULL);
    }
    if (g_State.hasEncoder) {
        ma_encoder_uninit(&g_State.encoder);
    }

    return 0;
}
