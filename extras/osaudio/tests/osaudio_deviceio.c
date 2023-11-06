#include "../osaudio.h"

/* This example uses miniaudio for decoding audio files. */
#define MINIAUDIO_IMPLEMENTATION
#include "../../../miniaudio.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MODE_PLAYBACK   0
#define MODE_CAPTURE    1
#define MODE_DUPLEX     2

void enumerate_devices()
{
    int result;
    unsigned int iDevice;
    unsigned int count;
    osaudio_info_t* pDeviceInfos;

    result = osaudio_enumerate(&count, &pDeviceInfos);
    if (result != OSAUDIO_SUCCESS) {
        printf("Failed to enumerate audio devices.\n");
        return;
    }

    for (iDevice = 0; iDevice < count; iDevice += 1) {
        printf("(%s) %s\n", (pDeviceInfos[iDevice].direction == OSAUDIO_OUTPUT) ? "Playback" : "Capture", pDeviceInfos[iDevice].name);
    }

    free(pDeviceInfos);
}

osaudio_t open_device(int direction)
{
    int result;
    osaudio_t audio;
    osaudio_config_t config;
    
    osaudio_config_init(&config, direction);
    config.format   = OSAUDIO_FORMAT_F32;
    config.channels = 2;
    config.rate     = 48000;
    config.flags    = OSAUDIO_FLAG_REPORT_XRUN;

    result = osaudio_open(&audio, &config);
    if (result != OSAUDIO_SUCCESS) {
        printf("Failed to open audio device.\n");
        return NULL;
    }

    return audio;
}

void do_playback(int argc, char** argv)
{
    int result;
    osaudio_t audio;
    const osaudio_config_t* config;
    const char* pFilePath = NULL;
    ma_result resultMA;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;

    audio = open_device(OSAUDIO_OUTPUT);
    if (audio == NULL) {
        printf("Failed to open audio device.\n");
        return;
    }

    config = &osaudio_get_info(audio)->configs[0];

    /* We want to always use f32. */
    if (config->format == OSAUDIO_FORMAT_F32) {
        if (argc > 1) {
            pFilePath = argv[1];

            decoderConfig = ma_decoder_config_init(ma_format_f32, (ma_uint32)config->channels, (ma_uint32)config->rate);
            
            resultMA = ma_decoder_init_file(pFilePath, &decoderConfig, &decoder);
            if (resultMA == MA_SUCCESS) {
                /* Now just keep looping over each sample until we get to the end. */
                for (;;) {
                    float frames[1024];
                    ma_uint64 frameCount;

                    resultMA = ma_decoder_read_pcm_frames(&decoder, frames, ma_countof(frames) / config->channels, &frameCount);
                    if (resultMA != MA_SUCCESS) {
                        break;
                    }

                    result = osaudio_write(audio, frames, (unsigned int)frameCount); /* Safe cast. */
                    if (result != OSAUDIO_SUCCESS && result != OSAUDIO_XRUN) {
                        printf("Error writing to audio device.");
                        break;
                    }

                    if (result == OSAUDIO_XRUN) {
                        printf("WARNING: An xrun occurred while writing to the playback device.\n");
                    }
                }
            } else {
                printf("Failed to open file: %s\n", pFilePath);
            }
        } else {
            printf("No input file.\n");
        }    
    } else {
        printf("Unsupported device format.\n");
    }

    /* Getting here means we're done and we can tear down. */
    osaudio_close(audio);
}

void do_duplex()
{
    int result;
    osaudio_t capture;
    osaudio_t playback;

    capture = open_device(OSAUDIO_INPUT);
    if (capture == NULL) {
        printf("Failed to open capture device.\n");
        return;
    }

    playback = open_device(OSAUDIO_OUTPUT);
    if (playback == NULL) {
        osaudio_close(capture);
        printf("Failed to open playback device.\n");
        return;
    }

    for (;;) {
        float frames[1024];
        unsigned int frameCount;

        frameCount = ma_countof(frames) / osaudio_get_info(capture)->configs[0].channels;

        /* Capture. */
        result = osaudio_read(capture, frames, frameCount);
        if (result != OSAUDIO_SUCCESS && result != OSAUDIO_XRUN) {
            printf("Error reading from capture device.\n");
            break;
        }

        if (result == OSAUDIO_XRUN) {
            printf("WARNING: An xrun occurred while reading from the capture device.\n");
        }


        /* Playback. */
        result = osaudio_write(playback, frames, frameCount);
        if (result != OSAUDIO_SUCCESS && result != OSAUDIO_XRUN) {
            printf("Error writing to playback device.\n");
            break;
        }

        if (result == OSAUDIO_XRUN) {
            printf("WARNING: An xrun occurred while writing to the playback device.\n");
        }
    }

    osaudio_close(capture);
    osaudio_close(playback);
}

int main(int argc, char** argv)
{
    int mode = MODE_PLAYBACK;
    int iarg;

    enumerate_devices();

    for (iarg = 0; iarg < argc; iarg += 1) {
        if (strcmp(argv[iarg], "capture") == 0) {
            mode = MODE_CAPTURE;
        } else if (strcmp(argv[iarg], "duplex") == 0) {
            mode = MODE_DUPLEX;
        }
    }

    switch (mode)
    {
        case MODE_PLAYBACK: do_playback(argc, argv); break;
        case MODE_CAPTURE:  break;
        case MODE_DUPLEX:   do_duplex(); break;
    }

    (void)argc;
    (void)argv;

    return 0;
}