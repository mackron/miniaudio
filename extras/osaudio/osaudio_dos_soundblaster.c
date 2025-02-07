/*
This is only designed to work on DOS. I have only tested compiling this with OpenWatcom v2.0. Open
to feedback on improving compiler compatibility.

This will look at the BLASTER environment variable for the base port, IRQ and DMA channel. We're
only allowing a single device to be initialized at any given time. The channel will be defined by
the BLASTER environment variable, or if that's not set, it will default to channel 1 (for 8-bit) or
channel 5 (for 16-bit).
*/

/* Keep this file empty if we're not compiling for DOS. */
#if defined(__MSDOS__) || defined(__DOS__)
#ifndef osaudio_dos_soundblaster_c
#define osaudio_dos_soundblaster_c

#include "osaudio.h"

#include <dos.h>    /* outportb() */
#include <conio.h>  /* delay() */
#include <string.h> /* memset() */
#include <stdlib.h> /* malloc(), free() */

int g_TESTING = 0;

#define OSAUDIO_TIMEOUT_TICKS                       18   /* ~1 second timeout (just under - runs at 18.2 ticks per second). Sound Blaster specs claim it should only take about 100 microseconds so this is way overkill. */

#define OSAUDIO_SB_MIXER_PORT                       0x004
#define OSAUDIO_SB_MIXER_DATA_PORT                  0x005
#define OSAUDIO_SB_DSP_RESET_PORT                   0x006
#define OSAUDIO_SB_DSP_READ_PORT                    0x00A
#define OSAUDIO_SB_DSP_WRITE_PORT                   0x00C
#define OSAUDIO_SB_DSP_READY_READ_PORT              0x00E

#define OSAUDIO_SB_DSP_RESET_CMD                    0x01
#define OSAUDIO_SB_DSP_GET_VERSION                  0xE1

#define OSAUDIO_ISA_DMA_MASK_REGISTER_8BIT          0x0A
#define OSAUDIO_ISA_DMA_MASK_REGISTER_16BIT         0xD4

#define OSAUDIO_ISA_DMA_FLIPFLOP_REGISTER_8BIT      0x0C
#define OSAUDIO_ISA_DMA_FLIPFLOP_REGISTER_16BIT     0xD8

#define OSAUDIO_ISA_DMA_MODE_REGISTER_8BIT          0x0B
#define OSAUDIO_ISA_DMA_MODE_REGISTER_16BIT         0xD6
#define OSAUDIO_ISA_DMA_MODE_DEMAND                 0x00
#define OSAUDIO_ISA_DMA_MODE_SINGLE                 0x40
#define OSAUDIO_ISA_DMA_MODE_BLOCK                  0x80
#define OSAUDIO_ISA_DMA_MODE_CASCADE                0xC0
#define OSAUDIO_ISA_DMA_MODE_READ                   0x08
#define OSAUDIO_ISA_DMA_MODE_WRITE                  0x04
#define OSAUDIO_ISA_DMA_MODE_AUTOINIT               0x10

#define OSAUDIO_ISA_DMA_ADDRESS_REGISTER_8BIT       0x00
#define OSAUDIO_ISA_DMA_ADDRESS_REGISTER_16BIT      0xC0

#define OSAUDIO_ISA_DMA_COUNT_REGISTER_8BIT         0x01
#define OSAUDIO_ISA_DMA_COUNT_REGISTER_16BIT        0xC2

#define OSAUDIO_SB_DEVICE_NAME                      "Sound Blaster"

#define OSAUDIO_COUNTOF(x)                          (sizeof(x) / sizeof(x[0]))
#define OSAUDIO_ALIGN(x, a)                         (((x) + ((a)-1)) & ~((a)-1))
#define OSAUDIO_ALIGN_32(x)                         OSAUDIO_ALIGN(x, 4)

/* BLASTER environment variable defaults. */
static unsigned short OSAUDIO_SB_BASE_PORT          = 0x220;
static unsigned short OSAUDIO_SB_IRQ                = 7;
static unsigned short OSAUDIO_SB_DMA_CHANNEL_8      = 1;
static unsigned short OSAUDIO_SB_DMA_CHANNEL_16     = 5;

static signed   char  osaudio_g_is_sb16_present     = -1;
static unsigned char  osaudio_g_sb16_version_major  = 0;
static unsigned char  osaudio_g_sb16_version_minor  = 0;
static osaudio_t      osaudio_g_audio = NULL;   /* For ISA DMA interrupt because there's no user data option. Only one device can be initialized at a time. */

static osaudio_format_t osaudio_g_supportedFormats[] =
{
    OSAUDIO_FORMAT_S16, OSAUDIO_FORMAT_U8
};

static unsigned char osaudio_g_supportedChannels[] =
{
    2, 1
};

static unsigned int osaudio_g_supportedSampleRates[] =
{
    44100, 22050, 11025, 24000, 12000, 8000
};


struct _osaudio_t
{
    void far* pDMABuffer;
    void (_interrupt _far* old_isr)(void);
    osaudio_info_t info;
    osaudio_config_t config;                /* info.configs will point to this. */
    unsigned int cursor;                    /* The position of the write or read cursor relative to the start of the current sub-buffer. In frames. */
    unsigned char subBufferIndex;           /* When 0, the next write and read will happen in the first half of the DMA buffer, when 1, the second half. Will flip-flop between 0 and 1 each interrupt. */
    unsigned char isActive;
    unsigned char isPaused;
};

static void osaudio_outportb(unsigned short port, unsigned char value)
{
    _outp(port, value);
}

static unsigned char osaudio_inportb(unsigned short port)
{
    return _inp(port);
}

static void osaudio_delay(unsigned int milliseconds)
{
    delay(milliseconds);
}

static unsigned long osaudio_get_ticks()
{
    union REGS regs;

    regs.h.ah = 0x00; /* Get system time */
    int86(0x1A, &regs, &regs);

    return ((unsigned long)regs.x.cx << 16) | regs.x.dx;
}

void far_memset(void far* dst, int c, unsigned int count)
{
    unsigned char far *p = dst;
    unsigned int i;

    for (i = 0; i < count; i += 1) {
        p[i] = c;
    }
}

static void far* osaudio_dos_calloc(unsigned int sz)
{
    /* This uses the DOS interrupt 0x21, function 0x48 allocation. */
    void far* p;
    union REGS regs;

    regs.h.ah = 0x48;
    regs.x.bx = (sz + 15) / 16;
    int86(0x21, &regs, &regs);

    if (regs.x.cflag) {
        return NULL;
    }

    p = MK_FP(regs.x.ax, 0);

    /* Clear to zero for safety. */
    far_memset(p, 0, sz);

    return p;
}

static void osaudio_dos_free(void far* p)
{
    /* This uses the DOS interrupt 0x21, function 0x49 free. */
    union REGS regs;
    struct SREGS sregs;

    regs.h.ah = 0x49;
    regs.x.bx = FP_OFF(p);
    sregs.es  = FP_SEG(p);
    intdosx(&regs, &regs, &sregs);
}

static void osaudio_blaster_parse_env()
{
    /* This parses the BLASTER environment variable. */
    char* pBlaster;
    char* p;
    int unused;

    pBlaster = getenv("BLASTER");
    if (pBlaster == NULL) {
        return;
    }

    /* We have a BLASTER environment variable. */
    p = pBlaster;

    /*
    I'm not sure if we can assume that each of these are present and in the same order. Therefore
    we will do this generically and parse each segment, separated by whitespace.
    */
    for (;;) {
        /* Skip whitespace. */
        while (*p == ' ' || *p == '\t') {
            p += 1;
        }

        if (*p == '\0') {
            break;
        }

        /* Parse the segment. */
        if (p[0] == 'A') {
            /* Base port. */
            p += 1;
            OSAUDIO_SB_BASE_PORT = (unsigned short)strtoul(p, &p, 16);
            /*printf("A%u\n", (unsigned int)OSAUDIO_SB_BASE_PORT);*/
        } else if (p[0] == 'I') {
            /* IRQ. */
            p += 1;
            OSAUDIO_SB_IRQ = (unsigned short)strtoul(p, &p, 10);
            /*printf("I%u\n", (unsigned int)OSAUDIO_SB_IRQ);*/
        } else if (p[0] == 'D') {
            /* 8-bit DMA channel. */
            p += 1;
            OSAUDIO_SB_DMA_CHANNEL_8 = (unsigned short)strtoul(p, &p, 10);
            /*printf("D%u\n", (unsigned int)OSAUDIO_SB_DMA_CHANNEL_8);*/
        } else if (p[0] == 'H') {
            /* 16-bit DMA channel. */
            p += 1;
            OSAUDIO_SB_DMA_CHANNEL_16 = (unsigned short)strtoul(p, &p, 10);
            /*printf("H%u\n", (unsigned int)OSAUDIO_SB_DMA_CHANNEL_16);*/
        } else if (p[0] == 'M' || p[0] == 'P' || p[0] == 'T') {
            /* These are ignored. */
            p += 1;
            unused = (int)strtoul(p, &p, 16);
        } else {
            /* Unknown segment. Skip. */
            p += 1;
            unused = (int)strtoul(p, &p, 16);
        }
    }
}

static osaudio_result_t osaudio_init_sb16()
{
    /* This checks for the presence of a Sound Blaster 16 card. */
    if (osaudio_g_is_sb16_present == -1) {
        unsigned long timeoutStart;

        /*
        Creative wants us to read settings from the BLASTER environment variable. We don't hard
        fail here - we'll just fall back to defaults. It'll fail later if we don't have Sound
        Blaster available.
        */
        osaudio_blaster_parse_env();


        /* Getting here means we need to check for SB16. */
        osaudio_outportb(OSAUDIO_SB_BASE_PORT | OSAUDIO_SB_DSP_RESET_PORT, OSAUDIO_SB_DSP_RESET_CMD);
        osaudio_delay(1);   /* Sound Blaster documentation says to wait 3 microseconds. We'll do 1 milliseconds. */
        osaudio_outportb(OSAUDIO_SB_BASE_PORT | OSAUDIO_SB_DSP_RESET_PORT, 0x00);

        /* Wait for DSP to be ready. */
        timeoutStart = osaudio_get_ticks();
        while ((osaudio_inportb(OSAUDIO_SB_BASE_PORT | OSAUDIO_SB_DSP_READY_READ_PORT) & 0x80) == 0) {
            if (osaudio_get_ticks() - timeoutStart > OSAUDIO_TIMEOUT_TICKS) {
                osaudio_g_is_sb16_present = 0;
                return OSAUDIO_ERROR;
            }
        }

        /* Check result of reset. */
        if (osaudio_inportb(OSAUDIO_SB_BASE_PORT | OSAUDIO_SB_DSP_READ_PORT) == 0xAA) {
            /* Wait for write port to be ready. */
            timeoutStart = osaudio_get_ticks();
            while (osaudio_inportb(OSAUDIO_SB_BASE_PORT | OSAUDIO_SB_DSP_WRITE_PORT) & 0x80) {
                if (osaudio_get_ticks() - timeoutStart > OSAUDIO_TIMEOUT_TICKS) {
                    osaudio_g_is_sb16_present = 0;
                    return OSAUDIO_ERROR;
                }
            }

            /* Send DSP command to get version. */
            osaudio_outportb(OSAUDIO_SB_BASE_PORT | OSAUDIO_SB_DSP_WRITE_PORT, OSAUDIO_SB_DSP_GET_VERSION);
            osaudio_g_sb16_version_major = osaudio_inportb(OSAUDIO_SB_BASE_PORT | OSAUDIO_SB_DSP_READ_PORT);
            osaudio_g_sb16_version_minor = osaudio_inportb(OSAUDIO_SB_BASE_PORT | OSAUDIO_SB_DSP_READ_PORT);
        
            if (osaudio_g_sb16_version_major == 4) {
                osaudio_g_is_sb16_present = 1;
            } else {
                osaudio_g_is_sb16_present = 0;
            }

            /* Now configure the IRQ. */
            if (osaudio_g_is_sb16_present) {
                unsigned char irqCode;

                switch (OSAUDIO_SB_IRQ) {
                    case 2:  irqCode = 0x01; break;
                    case 5:  irqCode = 0x02; break;
                    case 7:  irqCode = 0x04; break;
                    case 10: irqCode = 0x08; break;
                    default: irqCode = 0x04; break; /* Will never hit this. */
                }

                /*
                printf("Mixer port: %u\n", OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_MIXER_PORT);
                printf("Mixer data port: %u\n", OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_MIXER_DATA_PORT);
                printf("IRQ code: %u\n", (unsigned int)irqCode);
                */

                osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_MIXER_PORT,      0x80);
                osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_MIXER_DATA_PORT, irqCode);
            }
        } else {
            osaudio_g_is_sb16_present = 0;
        }
    }

    if (osaudio_g_is_sb16_present) {
        return OSAUDIO_SUCCESS;
    } else {
        return OSAUDIO_ERROR;   /* Don't appear to have SB16. */
    }
}

osaudio_result_t osaudio_enumerate(unsigned int* count, osaudio_info_t** info)
{
    /*
    We need only report a default playback device and a default capture device. We're going to
    support both OSAUDIO_FORMAT_U8 and OSAUDIO_FORMAT_S16. Supported channel counts are mono and
    stereo.
    */
    osaudio_result_t result;
    unsigned int nativeFormatCount;
    unsigned int iSupportedFormat;
    unsigned int iSupportedChannels;
    unsigned int iSupportedSampleRate;
    osaudio_config_t* pRunningConfig;

    if (count == NULL || info == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    result = osaudio_init_sb16();
    if (result != OSAUDIO_SUCCESS) {
        return result;
    }

    nativeFormatCount = OSAUDIO_COUNTOF(osaudio_g_supportedFormats) * OSAUDIO_COUNTOF(osaudio_g_supportedChannels) * OSAUDIO_COUNTOF(osaudio_g_supportedSampleRates);

    *info = (osaudio_info_t*)calloc(1, sizeof(osaudio_info_t) * 2 + (sizeof(osaudio_config_t) * 2 * nativeFormatCount));
    if (*info == NULL) {
        return OSAUDIO_OUT_OF_MEMORY;
    }

    *count = 2;

    /* Now we need to fill out the details. */
    pRunningConfig = (osaudio_config_t*)(*info + 2);

    /* Playback. */
    strcpy((*info)[0].name, OSAUDIO_SB_DEVICE_NAME);
    (*info)[0].direction    = OSAUDIO_OUTPUT;
    (*info)[0].config_count = nativeFormatCount;
    (*info)[0].configs      = pRunningConfig;

    for (iSupportedFormat = 0; iSupportedFormat < OSAUDIO_COUNTOF(osaudio_g_supportedFormats); iSupportedFormat += 1) {
        for (iSupportedChannels = 0; iSupportedChannels < OSAUDIO_COUNTOF(osaudio_g_supportedChannels); iSupportedChannels += 1) {
            for (iSupportedSampleRate = 0; iSupportedSampleRate < OSAUDIO_COUNTOF(osaudio_g_supportedSampleRates); iSupportedSampleRate += 1) {
                osaudio_config_init(pRunningConfig, OSAUDIO_OUTPUT);
                pRunningConfig->format   = osaudio_g_supportedFormats[iSupportedFormat];
                pRunningConfig->channels = osaudio_g_supportedChannels[iSupportedChannels];
                pRunningConfig->rate     = osaudio_g_supportedSampleRates[iSupportedSampleRate];

                if (pRunningConfig->channels == 1) {
                    pRunningConfig->channel_map[0] = OSAUDIO_CHANNEL_MONO;
                } else {
                    pRunningConfig->channel_map[0] = OSAUDIO_CHANNEL_FL;
                    pRunningConfig->channel_map[1] = OSAUDIO_CHANNEL_FR;
                }

                pRunningConfig += 1;
            }
        }
    }

    /* Capture. */
    strcpy((*info)[1].name, OSAUDIO_SB_DEVICE_NAME);
    (*info)[1].direction    = OSAUDIO_INPUT;
    (*info)[1].config_count = nativeFormatCount;
    (*info)[1].configs      = pRunningConfig;

    for (iSupportedFormat = 0; iSupportedFormat < OSAUDIO_COUNTOF(osaudio_g_supportedFormats); iSupportedFormat += 1) {
        for (iSupportedChannels = 0; iSupportedChannels < OSAUDIO_COUNTOF(osaudio_g_supportedChannels); iSupportedChannels += 1) {
            for (iSupportedSampleRate = 0; iSupportedSampleRate < OSAUDIO_COUNTOF(osaudio_g_supportedSampleRates); iSupportedSampleRate += 1) {
                osaudio_config_init(pRunningConfig, OSAUDIO_OUTPUT);
                pRunningConfig->format   = osaudio_g_supportedFormats[iSupportedFormat];
                pRunningConfig->channels = osaudio_g_supportedChannels[iSupportedChannels];
                pRunningConfig->rate     = osaudio_g_supportedSampleRates[iSupportedSampleRate];

                if (pRunningConfig->channels == 1) {
                    pRunningConfig->channel_map[0] = OSAUDIO_CHANNEL_MONO;
                } else {
                    pRunningConfig->channel_map[0] = OSAUDIO_CHANNEL_FL;
                    pRunningConfig->channel_map[1] = OSAUDIO_CHANNEL_FR;
                }

                pRunningConfig += 1;
            }
        }
    }

    return OSAUDIO_SUCCESS;
}

void osaudio_config_init(osaudio_config_t* config, osaudio_direction_t direction)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->direction = direction;
}

static unsigned int osaudio_find_closest_rate(unsigned int rate, const unsigned int* pAvailableRates, unsigned int availableRateCount)
{
    unsigned int iRate;
    unsigned int bestRate;
    unsigned int bestRateDelta;
    unsigned int rateDelta;

    bestRate = 0;
    bestRateDelta = 0xFFFFFFFF;
    for (iRate = 0; iRate < availableRateCount; iRate += 1) {
        rateDelta = (pAvailableRates[iRate] > rate) ? (pAvailableRates[iRate] - rate) : (rate - pAvailableRates[iRate]);
        if (rateDelta < bestRateDelta) {
            bestRate = pAvailableRates[iRate];
            bestRateDelta = rateDelta;
        }
    }

    return bestRate;
}



#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

static void send_eoi(int irq)
{
    if (irq >= 8) {
        /* If it's IRQ8 or higher, we have to send an EOI to both the master and slave controllers */
        osaudio_outportb(PIC2_COMMAND, PIC_EOI);
    }

    /* Always send an EOI to the master controller */
    osaudio_outportb(PIC1_COMMAND, PIC_EOI);
}


static void interrupt osaudio_isa_dma_interrupt_handler()
{
    unsigned char status;
    unsigned int cursor;

    /* Unfortunately there's no user-data associated with the interrupt handler, so we have to use a global. */
    osaudio_t audio = osaudio_g_audio;
    if (audio == NULL) {
        return;
    }

    /* In 16-bit mode, we need to check if the interrupt is for us. Only applies to SB16. */
#if 0
    if (osaudio_g_sb16_version_major == 4) {
        if (audio->config.format == OSAUDIO_FORMAT_S16) {
            osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_MIXER_PORT, 0x82);
            status = osaudio_inportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_MIXER_DATA_PORT);
            if ((status & 0x02) == 0) {
                return; /* Not for us. */
            }
        }
    }
#endif

    /*
    g_TESTING += 1;
    printf("TESTING: %d\n", g_TESTING);
    */

    cursor = audio->cursor;

    if (cursor < audio->config.buffer_size) {
        /* This is an xrun. */
        unsigned int bytesPerFrame;
        bytesPerFrame = audio->config.channels * ((audio->config.format == OSAUDIO_FORMAT_S16) ? 2 : 1);

        /* Fill the rest of the buffer with silence. */
        //far_memset((char OSAUDIO_FAR*)audio->pDMABuffer + (audio->subBufferIndex * audio->config.buffer_size * bytesPerFrame) + (cursor * bytesPerFrame), 0, (audio->config.buffer_size - cursor) * bytesPerFrame);
        //far_memset((char OSAUDIO_FAR*)audio->pDMABuffer + (audio->subBufferIndex * audio->config.buffer_size * bytesPerFrame), 0, audio->config.buffer_size * bytesPerFrame);

        /* Now fill the entire next buffer with silence as well. */
        //far_memset((char OSAUDIO_FAR*)audio->pDMABuffer + ((1 - audio->subBufferIndex) * audio->config.buffer_size * bytesPerFrame), 0, audio->config.buffer_size * bytesPerFrame);

        far_memset((char OSAUDIO_FAR*)audio->pDMABuffer, 0, audio->config.buffer_size * bytesPerFrame * 2);

        printf("XRUN: cursor = %u, subBufferIndex = %u; test = %u\n", cursor, audio->subBufferIndex, (audio->config.buffer_size * bytesPerFrame * 2));
        return;
    }

    /* Flip the sub-buffer index. */
    printf("Interrupt: sub-buffer index: %u\n", audio->subBufferIndex);
    audio->subBufferIndex = 1 - audio->subBufferIndex;

    /* Make sure the cursor is reset in prepration for the next half of the buffer. */
    audio->cursor = 0;

    /* Mark the sub-buffer as consumed. */
    //audio->subBufferConsumed[audio->subBufferIndexInterrupt] = 1;
    //audio->subBufferIndexInterrupt = 1 - audio->subBufferIndexInterrupt;

    /* Interrupt acknowledgment. */
    if (audio->config.format == OSAUDIO_FORMAT_U8) {
        status = osaudio_inportb(OSAUDIO_SB_BASE_PORT + 0x00E);
    } else {
        status = osaudio_inportb(OSAUDIO_SB_BASE_PORT + 0x00F);
    }

    //printf("status: 0x%02X\n", status);
    (void)status;

    send_eoi(OSAUDIO_SB_IRQ);
}



#include <time.h>

void fill_random_unsigned_chars(unsigned char far* buffer, unsigned int size)
{
    unsigned int i;

    /* Seed the random number generator */
    srand((unsigned)time(NULL));

    for (i = 0; i < size; i++) {
        /* Generate a random unsigned char integer */
        buffer[i] = (unsigned char)(rand() - RAND_MAX / 2) >> 4;

        //printf("buffer[%d] = %d\n", i, buffer[i]);
    }
}


osaudio_result_t osaudio_open(osaudio_t* audio, osaudio_config_t* config)
{
    osaudio_result_t result;
    unsigned short dmaMaskRegister;
    unsigned short flipflopRegister;
    unsigned char dmaChannel;
    unsigned int maxDMABufferSizeInFrames;
    unsigned int actualDMABufferSizeInFrames;
    unsigned int actualDMABufferSizeInBytes;
    void far* pDMABuffer;
    unsigned char picMask;


    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    *audio = NULL;

    if (config == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    /* We can only have a single device open at a time. */
    if (osaudio_g_audio != NULL) {
        return OSAUDIO_ERROR;
    }

    /* First check that we have SB16. */
    result = osaudio_init_sb16();
    if (result != OSAUDIO_SUCCESS) {
        return result;
    }

    /* Capture mode is not supported on anything older than Sound Blaster 16. */
    if (config->direction == OSAUDIO_INPUT && osaudio_g_sb16_version_major < 4) {
        return OSAUDIO_ERROR;   /* Capture mode not supported. */
    }

    /* We're going to choose our native format configuration first. This way we can determine the Sound Blaster ports, channels and the size of the DMA buffer. */
    if (config->format == OSAUDIO_FORMAT_UNKNOWN || (config->format != OSAUDIO_FORMAT_S16 && config->format != OSAUDIO_FORMAT_U8)) {
        config->format = osaudio_g_supportedFormats[0];
    }

    if (config->channels == 0 || config->channels > 2) {
        config->channels = osaudio_g_supportedChannels[0];
    }

    if (config->rate == 0) {
        config->rate = osaudio_g_supportedSampleRates[0];
    }

    if (config->rate > 44100) {
        config->rate = 44100;
    }
    if (config->rate < 8000) {
        config->rate = 8000;
    }

    /*config->rate = osaudio_find_closest_rate(config->rate, osaudio_g_supportedSampleRates, OSAUDIO_COUNTOF(osaudio_g_supportedSampleRates));*/

    /*
    Calculate a desired buffer size if none was specified. I'm not quite sure what would be an
    appropriate default in milliseconds, but lets go with 20ms for now. The buffer size is in
    frames, not bytes.
    */
    if (config->buffer_size == 0) {
        config->buffer_size = (80 * (unsigned long)config->rate) / 1000;
    }


    /*
    Getting here means Sound Blaster 16 is available. We now need to allocate memory. There are two
    things needing allocating - the DMA buffer, and the osaudio_t object. The allocation of the
    osaudio_t object will use malloc().

    The allocation of the DMA buffer will use the DOS interupt 0x21, function 0x48 for allocation.
    This ensures that it will be allocated within the first 1MB which we need for Sound Blaster. In
    addition, by making sure we don't allocate more than 65520 bytes we can ensure we don't cross a
    64KB boundary which is another requirement.
    */
    maxDMABufferSizeInFrames = 65520 / config->channels / ((config->format == OSAUDIO_FORMAT_S16) ? 2 : 1); /* 65520 = max size in bytes. */

    /*
    The actual size of the buffer is equal to what we asked for in the configuration. The config
    will contain valid values at this point.
    */
    actualDMABufferSizeInFrames = config->buffer_size;
    if (actualDMABufferSizeInFrames > maxDMABufferSizeInFrames) {
        actualDMABufferSizeInFrames = maxDMABufferSizeInFrames;
    }

    actualDMABufferSizeInBytes = actualDMABufferSizeInFrames * config->channels * ((config->format == OSAUDIO_FORMAT_S16) ? 2 : 1);

    /* Now that we have the size of the DMA buffer we can allocate it. 2x because we're using double buffering. */
    pDMABuffer = osaudio_dos_calloc(actualDMABufferSizeInBytes * 2);
    if (pDMABuffer == NULL) {
        return OSAUDIO_OUT_OF_MEMORY;
    }

    /*printf("actualDMABufferSizeInFrames = %d; maxDMABufferSizeInFrames = %d;\n", actualDMABufferSizeInFrames, maxDMABufferSizeInFrames);*/

    /*
    if (config->format == OSAUDIO_FORMAT_S16) {
        fill_random_unsigned_chars(pDMABuffer, actualDMABufferSizeInFrames * config->channels * 2 * 2);
    } else {
        fill_random_unsigned_chars(pDMABuffer, actualDMABufferSizeInFrames * config->channels * 2);
    }
    */

    //printf("actualDMABufferSizeInBytes = %u\n", actualDMABufferSizeInBytes);

    /*
    Allocate the osaudio_t object using malloc(). This is convenient because it means we can use a
    near pointer for this object since that is how it's declared in osaudio.h.
    */
    *audio = (osaudio_t)calloc(1, sizeof(**audio));
    if (*audio == NULL) {
        osaudio_dos_free(pDMABuffer);
        return OSAUDIO_OUT_OF_MEMORY;
    }

    (*audio)->subBufferIndex = 0;

    /* For playback we want to start our sub-buffer at 1. */
    if (config->direction == OSAUDIO_OUTPUT) {
        (*audio)->subBufferIndex = 1;
    } else {
        (*audio)->subBufferIndex = 0;
    }
    

    /* Turn on speaker. Can this be done last? */
    osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, 0xD1);

    /* Volume control. */
    /*osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_MIXER_PORT,      0x22);*/  /* 0x22 = Master Volume. */
    /*osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_MIXER_DATA_PORT, 0xCC);*/  /* 0xLR */


    /* Set up our interrupt. This is where we'll be notified when the buffer can be updated. */
    (*audio)->old_isr = _dos_getvect(OSAUDIO_SB_IRQ + 8);
    _dos_setvect(OSAUDIO_SB_IRQ + 8, osaudio_isa_dma_interrupt_handler);


    /* Need to unmask the interrupt or else nothing will be heard. Should probably do the opposite of this when we're done with the device. */
    picMask = osaudio_inportb(0x21);
    picMask = picMask & ~(1 << OSAUDIO_SB_IRQ);
    osaudio_outportb(0x21, picMask);


    /* At this point we've allocated our memory. We can now configure the DMA. */

    /* First we need to determine the mask register and the DMA channel. */
    if (config->format == OSAUDIO_FORMAT_S16) {
        dmaMaskRegister = OSAUDIO_ISA_DMA_MASK_REGISTER_16BIT;
        flipflopRegister = OSAUDIO_ISA_DMA_FLIPFLOP_REGISTER_16BIT;
        dmaChannel = OSAUDIO_SB_DMA_CHANNEL_16;
    } else {
        dmaMaskRegister = OSAUDIO_ISA_DMA_MASK_REGISTER_8BIT;
        flipflopRegister = OSAUDIO_ISA_DMA_FLIPFLOP_REGISTER_8BIT;
        dmaChannel = OSAUDIO_SB_DMA_CHANNEL_8;
    }

    /*printf("DMA Channel: %d\n", dmaChannel);*/

    /* Now we can mask the channel. */
    osaudio_outportb(dmaMaskRegister, (dmaChannel & 0x03) | 0x04);
    {
        osaudio_outportb(flipflopRegister, 0xFF);

        /* Mode. */
        {
            unsigned char modeRegister = (config->format == OSAUDIO_FORMAT_S16) ? OSAUDIO_ISA_DMA_MODE_REGISTER_16BIT : OSAUDIO_ISA_DMA_MODE_REGISTER_8BIT;
            unsigned char mode = OSAUDIO_ISA_DMA_MODE_DEMAND | OSAUDIO_ISA_DMA_MODE_AUTOINIT;

            if (config->direction == OSAUDIO_OUTPUT) {
                mode |= OSAUDIO_ISA_DMA_MODE_READ;     /* From the perspective of the device. It's reading from the DMA buffer. */
            } else {
                mode |= OSAUDIO_ISA_DMA_MODE_WRITE;    /* From the perspective of the device. It's writing to the DMA buffer. */
            }

            mode |= (dmaChannel & 0x03);   /* The DMA channel. */

            /* print the mode as a hex value for testing. */
            /*printf("Mode: 0x%02x\n", mode);*/

            osaudio_outportb(modeRegister, mode);
        }

        /* Address. */
        {
            unsigned char addressRegister;
            unsigned char pageRegister;
            unsigned long address;

            if (config->format == OSAUDIO_FORMAT_S16) {
                addressRegister  = OSAUDIO_ISA_DMA_ADDRESS_REGISTER_16BIT + ((dmaChannel & 0x03) * 4);
            } else {
                addressRegister  = OSAUDIO_ISA_DMA_ADDRESS_REGISTER_8BIT  + ((dmaChannel & 0x03) * 2);
            }

            /* The page register is annoying. It's different depending on the DMA channel. */
            switch (dmaChannel) {
                case 1: pageRegister = 0x83; break;
                case 2: pageRegister = 0x81; break;
                case 3: pageRegister = 0x82; break;
                case 5: pageRegister = 0x8B; break;
                case 6: pageRegister = 0x89; break;
                case 7: pageRegister = 0x8A; break;
                default: pageRegister = 0; break; /* Will never get here. */
            }

            address = ((unsigned long)FP_SEG(pDMABuffer) << 4) + FP_OFF(pDMABuffer);

            /*
            Need to do a random shift by 1 bit when specifying the address in 16-bit mode. This one
            screwed me over hardcore. Thanks to OSDev for the tip:

                https://web.archive.org/web/20230731120125/https://wiki.osdev.org/ISA_DMA#16_bit_issues
            */
            if (config->format == OSAUDIO_FORMAT_S16) {
                address >>= 1;
            }

            /* print the address as a hex value for testing. */
            /*
            printf("Address: 0x%04x\n", address);
            printf("Address Register: 0x%02x\n", addressRegister);
            printf("Page Register: 0x%02x\n", pageRegister);
            */

            /* Page. */
            osaudio_outportb(pageRegister,    (address >> 16) & 0xFF);

            /* Address. */
            osaudio_outportb(addressRegister, (address >>  0) & 0xFF);
            osaudio_outportb(addressRegister, (address >>  8) & 0xFF);
        }

        /* Size */
        {
            unsigned char countRegister;
            unsigned int count;

            if (config->format == OSAUDIO_FORMAT_S16) {
                countRegister = OSAUDIO_ISA_DMA_COUNT_REGISTER_16BIT + ((dmaChannel & 0x03) * 4);
            } else {
                countRegister = OSAUDIO_ISA_DMA_COUNT_REGISTER_8BIT  + ((dmaChannel & 0x03) * 2);
            }

            /*
            We're using a double-buffering technique which means we want to double the size of the buffer.
            */
            count = actualDMABufferSizeInBytes * 2; /* 2x because of double buffering. */
            if (config->format == OSAUDIO_FORMAT_S16) {
                count >>= 1; /* 1/2 because of 16-bit mode. */
            }

            count -= 1;

            /*
            printf("Size: 0x%04x\n", count);
            printf("Count Register: 0x%02x\n", countRegister);
            */

            /* Size. */
            osaudio_outportb(countRegister, (count >> 0) & 0xFF);
            osaudio_outportb(countRegister, (count >> 8) & 0xFF);
        }
    }
    osaudio_outportb(dmaMaskRegister, (dmaChannel & 0x03));

    /* At this point the DMA buffer should be configured. We can now configure the DSP. */

    /* Sample Rate */
    {
        unsigned char command;
        unsigned short sampleRate; /* Or time constant. */

        /* For Sound Blaster 16 we'll use the exact sample rate. Otherwise we'll use the time constant. */
        if (osaudio_g_sb16_version_major == 4) {
            if (config->direction == OSAUDIO_OUTPUT) {
                command = 0x41;
            } else {
                command = 0x42;
            }

            sampleRate = config->rate;
        } else {
            command = 0x40;
            sampleRate = 65536 - ((unsigned long)256000000 / (config->channels * config->rate));
        }

        /*printf("Command: 0x%02x\n", command);*/
        /*printf("Sample Rate: %u\n", sampleRate);*/

        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, command);

        if (osaudio_g_sb16_version_major == 4) {
            /* Note that it's high byte first, unlike the block size below which is low byte first. This one bit me. */
            osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, (sampleRate >> 8) & 0xFF);
            osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, (sampleRate >> 0) & 0xFF);
        } else {
            osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, (sampleRate >> 8) & 0xFF);
        }
    }

    /* Data Format and Block Size. */
    {
        unsigned char command;
        unsigned char mode;
        unsigned int blockSize; /* In bytes. */

        if (config->format == OSAUDIO_FORMAT_S16) {
            if (config->direction == OSAUDIO_OUTPUT) {
                command = 0xB6;
            } else {
                command = 0xBE;
            }

            if (config->channels == 1) {
                mode = 0x10;
            } else {
                mode = 0x30;
            }
        } else {
            if (config->direction == OSAUDIO_OUTPUT) {
                command = 0xC6;
            } else {
                command = 0xCE;
            }

            if (config->channels == 1) {
                mode = 0x00;
            } else {
                mode = 0x20;
            }
        }

        blockSize  = actualDMABufferSizeInBytes;
        if (config->format == OSAUDIO_FORMAT_S16) {
            blockSize >>= 1; /* 1/2 because of 16-bit mode. */
        }

        blockSize -= 1; /* Needs to be one less than the actual size. */

        /*
        printf("Command: 0x%02x\n", command);
        printf("Mode:    0x%02x\n", mode);
        printf("Block Size: %u\n", blockSize);
        */

        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, command);
        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, mode);
        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, (blockSize >> 0) & 0xFF);
        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, (blockSize >> 8) & 0xFF);
    }

    /* Start in a paused state. */
    if (config->format == OSAUDIO_FORMAT_S16) {
        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, 0xD5);
    } else {
        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, 0xD0);
    }


    (*audio)->pDMABuffer = pDMABuffer;
    strcpy((*audio)->info.name, OSAUDIO_SB_DEVICE_NAME);
    (*audio)->info.direction = config->direction;
    (*audio)->info.config_count = 1;
    (*audio)->info.configs = &(*audio)->config;
    (*audio)->config = *config;
    (*audio)->config.buffer_size = actualDMABufferSizeInFrames;

    /* Don't forget to set the global audio object. We need this for the ISR. */
    osaudio_g_audio = *audio;

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_close(osaudio_t audio)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    /* TODO: Implement me. Look at the SB docs for how to properly shut down. */

    /* Restore the old ISR. */
    _dos_setvect(OSAUDIO_SB_IRQ + 8, audio->old_isr);

    /* Free the DMA buffer. */
    osaudio_dos_free(audio->pDMABuffer);

    /* Now we can free the osaudio_t object. */
    free(audio);

    return OSAUDIO_SUCCESS;
}

static void osaudio_activate(osaudio_t audio)
{
    if (audio->config.format == OSAUDIO_FORMAT_S16) {
        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, 0xD6);
    } else {
        osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, 0xD4);
    }

    audio->isActive = 1;
}

osaudio_result_t osaudio_write(osaudio_t audio, const void OSAUDIO_FAR* data, unsigned int frame_count)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    //printf("ENTERED: frame_count = %u\n", frame_count);
    while (frame_count > 0) {
        unsigned int framesAvailable;

        if (audio->config.buffer_size > audio->cursor) {
            framesAvailable = audio->config.buffer_size - audio->cursor;
        } else {
            framesAvailable = 0;
        }
        
        if (framesAvailable > 0) {
            void far* dst;
            unsigned int bytesPerFrame;
            unsigned int framesToWrite = framesAvailable;
            if (framesToWrite > frame_count) {
                framesToWrite = frame_count;
            }

            //printf("framesAvailable = %u - %u = %u\n", audio->config.buffer_size, audio->cursor, framesAvailable);
            /*printf("framesToWrite = %u; framesAvailable = %u\n", framesToWrite, framesAvailable);*/

            /*
            Now we can copy the data. We're doing a cheeky little optimization here. If the input
            data pointer is equal to the DMA destination, we can skip the copy. This might happen
            when the caller is writing directly to the DMA buffer, which they may be doing by using
            a self-managed DMA buffer, in combination with osaudio_get_avail().
            */
            bytesPerFrame = audio->config.channels * (audio->config.format == OSAUDIO_FORMAT_S16 ? 2 : 1);

            dst = (void far*)((char far*)audio->pDMABuffer + (audio->subBufferIndex * audio->config.buffer_size * bytesPerFrame) + (audio->cursor * bytesPerFrame));
            if (dst == data) {
                /* Don't do anything. */
            } else {
                /* Move the memory. */
                unsigned int i;
                for (i = 0; i < framesToWrite * audio->config.channels; i += 1) {
                    if (audio->config.format == OSAUDIO_FORMAT_S16) {
                        ((short far*)dst)[i] = ((short far*)data)[i];
                    } else {
                        ((char  far*)dst)[i] = ((char  far*)data)[i];
                    }
                }
            }

            /* Update the cursor. */
            audio->cursor += framesToWrite;
            frame_count   -= framesToWrite;
            data = (char far*)data + (framesToWrite * bytesPerFrame);

            /* Activate the device. */
            if (!audio->isActive) {
                osaudio_activate(audio);
            }
        } else {
            /* No room. */
            if (!audio->isActive) {
                break;
            } else {
                /* Just keep looping. Don't sleep here - in my testing there just isn't enough resolution in the sleep timer. */
            }
        }
    }

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_read(osaudio_t audio, void OSAUDIO_FAR* data, unsigned int frame_count)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    /* TODO: Implement me. */

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_drain(osaudio_t audio)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    /*
    It's an invalid operation to drain while the device is paused or else we'll never return from
    this function.
    */
    if (audio->isPaused) {
        return OSAUDIO_INVALID_OPERATION;
    }

    /* DOS is single threaded so we don't need to worry about waiting for any pending reads or writes. */



    /* TODO: Implement me. */

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_flush(osaudio_t audio)
{
    if (audio == NULL) {
        return OSAUDIO_INVALID_ARGS;
    }

    /* TODO: Implement me. */

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_pause(osaudio_t audio)
{
    if (audio == NULL || audio != osaudio_g_audio) {
        return OSAUDIO_INVALID_ARGS;
    }

    if (audio->isPaused) {
        return OSAUDIO_SUCCESS;
    }

    /* No need to deactivate the device if it's already inactive. */
    if (audio->isActive) {
        if (audio->config.format == OSAUDIO_FORMAT_S16) {
            osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, 0xD5);
        } else {
            osaudio_outportb(OSAUDIO_SB_BASE_PORT + OSAUDIO_SB_DSP_WRITE_PORT, 0xD0);
        }
    }

    audio->isPaused = 1;

    return OSAUDIO_SUCCESS;
}

osaudio_result_t osaudio_resume(osaudio_t audio)
{
    if (audio == NULL || audio != osaudio_g_audio) {
        return OSAUDIO_INVALID_ARGS;
    }

    if (!audio->isPaused) {
        return OSAUDIO_SUCCESS;
    }

    /* Do not activate the device if it's inactive. */
    if (audio->isActive) {
        osaudio_activate(audio);
    }

    audio->isPaused = 0;

    return OSAUDIO_SUCCESS;
}

unsigned int osaudio_get_avail(osaudio_t audio)
{
    /* TODO: Implement me. */

    return 0;
}

const osaudio_info_t* osaudio_get_info(osaudio_t audio)
{
    if (audio == NULL) {
        return NULL;
    }

    return &audio->info;
}

#endif  /* osaudio_dos_soundblaster_c */
#endif  /* __MSDOS__ || __DOS__ */
