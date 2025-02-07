#include "../osaudio.h"
#include "../../decoders/litewav/litewav.c"

#include <stdio.h>
#include <stdlib.h> /* free() */

#if defined(__MSDOS__) || defined(__DOS__)
    #include <dos.h>
    #define OSAUDIO_DOS
#endif

const char* format_to_string(osaudio_format_t format)
{
    switch (format)
    {
        case OSAUDIO_FORMAT_F32: return "F32";
        case OSAUDIO_FORMAT_U8:  return "U8";
        case OSAUDIO_FORMAT_S16: return "S16";
        case OSAUDIO_FORMAT_S24: return "S24";
        case OSAUDIO_FORMAT_S32: return "S32";
        default: return "Unknown Format";
    }
}

void enumerate_devices()
{
    osaudio_result_t result;
    osaudio_info_t* pDeviceInfos;
    unsigned int deviceCount;
    unsigned int iDevice;

    result = osaudio_enumerate(&deviceCount, &pDeviceInfos);
    if (result != OSAUDIO_SUCCESS) {
        printf("Failed to enumerate devices.");
        return;
    }

    for (iDevice = 0; iDevice < deviceCount; iDevice += 1) {
        osaudio_info_t* pDeviceInfo = &pDeviceInfos[iDevice];
        
        printf("Device %u: [%s] %s\n", iDevice, (pDeviceInfo->direction == OSAUDIO_OUTPUT) ? "Playback" : "Capture", pDeviceInfo->name);

        #if 0
        {
            unsigned int iFormat;

            printf("   Native Formats\n");
            for (iFormat = 0; iFormat < pDeviceInfo->config_count; iFormat += 1) {
                osaudio_config_t* pConfig = &pDeviceInfo->configs[iFormat];
                printf("      %s %uHz %u channels\n", format_to_string(pConfig->format), pConfig->rate, pConfig->channels);
            }
        }
        #endif
    }

    free(pDeviceInfos);
}

extern int g_TESTING;

#include <string.h>

/* Sine wave generation. */
#include <math.h>

#if defined(OSAUDIO_DOS)
/* For farmalloc(). */
static void OSAUDIO_FAR* far_malloc(unsigned int sz)
{
    unsigned int segment;
    unsigned int err;

    err = _dos_allocmem(sz >> 4, &segment);
    if (err == 0) {
        return MK_FP(segment, 0);
    } else {
        return NULL;
    }
}
#else
#define far_malloc malloc
#endif

static char OSAUDIO_FAR* gen_sine_u8(unsigned long frameCount, unsigned int channels, unsigned int sampleRate)
{
    float phase = 0;
    float phaseIncrement = 2 * 3.14159265f * 220.0f / 44100.0f;
    unsigned long iFrame;
    char OSAUDIO_FAR* pData;
    char OSAUDIO_FAR* pRunningData;
    
    pData = (char OSAUDIO_FAR*)far_malloc(frameCount * channels);
    if (pData == NULL) {
        return NULL;
    }

    pRunningData = pData;

    for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
        unsigned int iChannel;
        float sample = (float)sin(phase) * 0.2f;
        sample = (sample + 1.0f) * 127.5f;

        for (iChannel = 0; iChannel < channels; iChannel += 1) {    
            pRunningData[iChannel] = (unsigned char)sample;
        }

        pRunningData += channels;
        phase += phaseIncrement;
    }

    return pData;
}

static short OSAUDIO_FAR* gen_sine_s16(unsigned long frameCount, unsigned int channels, unsigned int sampleRate)
{
    float phase = 0;
    float phaseIncrement = 2 * 3.14159265f * 220.0f / 44100.0f;
    unsigned long iFrame;
    short OSAUDIO_FAR* pData;
    short OSAUDIO_FAR* pRunningData;
    
    pData = (short OSAUDIO_FAR*)far_malloc(frameCount * channels * sizeof(short));
    if (pData == NULL) {
        return NULL;
    }

    pRunningData = pData;

    for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
        unsigned int iChannel;
        float sample = (float)sin(phase) * 0.2f;
        sample = sample * 32767.5f;

        for (iChannel = 0; iChannel < channels; iChannel += 1) {    
            pRunningData[iChannel] = (short)sample;
        }

        pRunningData += channels;
        phase += phaseIncrement;
    }

    return pData;
}

//
//
//float sinePhase = 0;
//float sinePhaseIncrement = 0;
//float sineVolume = 0.2f;
//
//static void sine_init()
//{
//    sinePhase = 0;
//    sinePhaseIncrement = 2 * 3.14159265f * 440.0f / 44100.0f;
//}
//
//static void sine_u8(unsigned char* dst, unsigned int frameCount, unsigned int channels)
//{
//    unsigned int iFrame;
//
//    for (iFrame = 0; iFrame < frameCount; iFrame += 1) {
//        unsigned int iChannel;
//        float sample = (float)sin(sinePhase) * sineVolume;
//        sample = (sample + 1.0f) * 127.5f;
//
//        for (iChannel = 0; iChannel < channels; iChannel += 1) {    
//            dst[iChannel] = (unsigned char)sample;
//        }
//
//        dst += channels;
//        sinePhase += sinePhaseIncrement;
//    }
//}
//
//
//unsigned char data[4096];

int main(int argc, char** argv)
{
    osaudio_result_t result;
    osaudio_t audio;
    osaudio_config_t config;
    void OSAUDIO_FAR* pSineWave;
    unsigned long sineWaveFrameCount;
    unsigned long sineWaveCursor = 0;

    enumerate_devices();

    osaudio_config_init(&config, OSAUDIO_OUTPUT);
    config.format   = OSAUDIO_FORMAT_S16;
    config.channels = 2;
    config.rate     = 44100;

    result = osaudio_open(&audio, &config);
    if (result != OSAUDIO_SUCCESS) {
        printf("Failed to initialize audio.\n");
        return -1;
    }

    printf("Device: %s (%s %uHz %u channels)\n", osaudio_get_info(audio)->name, format_to_string(config.format), config.rate, config.channels);

    //printf("sizeof(void*) = %u\n", (unsigned int)sizeof(void far *));

    /* 5 seconds. */
    sineWaveFrameCount = config.rate * 1;

    if (config.format == OSAUDIO_FORMAT_U8) {
        pSineWave = gen_sine_u8(sineWaveFrameCount, config.channels, config.rate);
    } else {
        pSineWave = gen_sine_s16(sineWaveFrameCount, config.channels, config.rate);
    }

    if (pSineWave == NULL) {
        printf("Failed to generate sine wave.\n");
        return -1;
    }

    if (config.format == OSAUDIO_FORMAT_U8) {
        /*unsigned int framesToSilence = config.rate;
        while (framesToSilence > 0) {
            unsigned int framesToWrite;
            char silence[256];
            memset(silence, 128, sizeof(silence));

            framesToWrite = framesToSilence;
            if (framesToWrite > sizeof(silence) / config.channels) {
                framesToWrite = sizeof(silence) / config.channels;
            }

            osaudio_write(audio, silence, framesToWrite);
            framesToSilence -= framesToWrite;
        }*/

        while (sineWaveCursor < sineWaveFrameCount) {
            unsigned long framesToWrite = sineWaveFrameCount - sineWaveCursor;
            if (framesToWrite > 0xFFFF) {
                framesToWrite = 0xFFFF;
            }

            //printf("Writing sine wave: %u\n", (unsigned int)framesToWrite);

            osaudio_write(audio, (char OSAUDIO_FAR*)pSineWave + (sineWaveCursor * config.channels), (unsigned int)framesToWrite);
            sineWaveCursor += framesToWrite;

            //printf("TRACE 0\n");
            //sine_u8(data, frameCount, config.channels);
            //printf("TRACE: %d\n", frameCount);
            //osaudio_write(audio, data, frameCount);
            //printf("DONE LOOP\n");
        }
    } else if (config.format == OSAUDIO_FORMAT_S16) {
        while (sineWaveCursor < sineWaveFrameCount) {
            unsigned long framesToWrite = sineWaveFrameCount - sineWaveCursor;
            if (framesToWrite > 0xFFFF) {
                framesToWrite = 0xFFFF;
            }

            osaudio_write(audio, (short OSAUDIO_FAR*)pSineWave + (sineWaveCursor * config.channels), (unsigned int)framesToWrite);
            sineWaveCursor += framesToWrite;
        }
    }

#if defined(OSAUDIO_DOS)
    printf("Processing...\n");
    for (;;) {
        /* Temporary. Just spinning here to ensure the program stays active. */
        //delay(1);
        if (g_TESTING > 0) {
            //printf("TESTING: %d\n", g_TESTING);
        }
    }
#endif

    printf("Shutting down... ");
    osaudio_close(audio);
    printf("Done.\n");

    (void)argc;
    (void)argv;

    return 0;
}
