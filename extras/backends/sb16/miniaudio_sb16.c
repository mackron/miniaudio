#ifndef miniaudio_backend_sb16_c
#define miniaudio_backend_sb16_c

#include "miniaudio_sb16.h"

#if defined(MA_DOS) && defined(__DJGPP__)
    #define MA_SUPPORT_SB16
#endif

#if defined(MA_SUPPORT_SB16) && !defined(MA_NO_SB16) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_SB16))
    #define MA_HAS_SB16
#endif

#if defined(MA_HAS_SB16)
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <pc.h>
#include <bios.h>
#include <dpmi.h>
#include <sys/movedata.h>
#include <sys/segments.h>

/* TODO: Replace with MA_ZERO_MEMORY() when this backend is integrated into miniaudio. */
#include <string.h>
#define MA_SB16_ZERO_MEMORY(p, sz) memset((p), 0, (sz))

/* TODO: Replace with MA_ASSERT() when this backend is integrated into miniaudio. */
#include <assert.h>
#define MA_SB16_ASSERT(cond) assert(cond)

/* Keep the marked functions in one code section, without reordering, cloning or merging. */
#if defined(__GNUC__) && __GNUC__ >= 5
    #define MA_SB16_NO_REORDER __attribute__((no_reorder, noinline, noclone, no_icf, section(".text.ma_sb16_locked")))
#else
    /* Older compilers do not guarantee this ordering. The application can lock all code if necessary. */
    #define MA_SB16_NO_REORDER
#endif

/* TODO: Get rid of this section when we integrate this into miniaudio proper. Use ma_sleep() directly. */
#if 1
    #include <dos.h>

    static void ma_sb16_sleep(ma_uint32 milliseconds)
    {
        delay(milliseconds);
    }
#endif


#define MA_SB16_PERIOD_COUNT                        2    /* Always using double buffering. */

#define MA_SB16_TICKS_PER_DAY                       0x1800B0UL
#define MA_SB16_TIMEOUT_TICKS                       18   /* ~1 second timeout (just under - runs at 18.2 ticks per second). Sound Blaster specs claim it should only take about 100 microseconds so this is way overkill. */

#define MA_SB16_MIXER_PORT                          0x004
#define MA_SB16_MIXER_DATA_PORT                     0x005
#define MA_SB16_DSP_RESET_PORT                      0x006
#define MA_SB16_DSP_READ_PORT                       0x00A
#define MA_SB16_DSP_WRITE_PORT                      0x00C
#define MA_SB16_DSP_READY_READ_PORT                 0x00E
#define MA_SB16_DSP_ACK_16BIT_PORT                  0x00F

#define MA_SB16_MIXER_INTERRUPT_STATUS_REGISTER     0x82
#define MA_SB16_INTERRUPT_STATUS_8BIT               0x01
#define MA_SB16_INTERRUPT_STATUS_16BIT              0x02

#define MA_SB16_DSP_RESET_CMD                       0x01
#define MA_SB16_DSP_GET_VERSION                     0xE1
#define MA_SB16_DSP_SET_OUTPUT_SAMPLE_RATE          0x41
#define MA_SB16_DSP_SET_INPUT_SAMPLE_RATE           0x42

#define MA_SB16_DSP_DMA_8BIT                        0xC0
#define MA_SB16_DSP_DMA_16BIT                       0xB0
#define MA_SB16_DSP_DMA_INPUT                       0x08
#define MA_SB16_DSP_DMA_AUTOINIT                    0x04
#define MA_SB16_DSP_DMA_FIFO                        0x02

#define MA_SB16_DSP_PAUSE_DMA_8BIT                  0xD0
#define MA_SB16_DSP_PAUSE_DMA_16BIT                 0xD5

#define MA_SB16_DSP_MODE_UNSIGNED                   0x00
#define MA_SB16_DSP_MODE_SIGNED                     0x10
#define MA_SB16_DSP_MODE_MONO                       0x00
#define MA_SB16_DSP_MODE_STEREO                     0x20

#define MA_SB16_PIC_COMMAND_REGISTER_MASTER         0x20
#define MA_SB16_PIC_COMMAND_REGISTER_SLAVE          0xA0
#define MA_SB16_PIC_MASK_REGISTER_MASTER            0x21
#define MA_SB16_PIC_MASK_REGISTER_SLAVE             0xA1
#define MA_SB16_PIC_END_OF_INTERRUPT                0x20
#define MA_SB16_PIC_CASCADE_MASK_BIT                0x04

#define MA_SB16_ISA_DMA_MASK_REGISTER_8BIT          0x0A
#define MA_SB16_ISA_DMA_MASK_REGISTER_16BIT         0xD4

#define MA_SB16_ISA_DMA_FLIPFLOP_REGISTER_8BIT      0x0C
#define MA_SB16_ISA_DMA_FLIPFLOP_REGISTER_16BIT     0xD8

#define MA_SB16_ISA_DMA_MODE_REGISTER_8BIT          0x0B
#define MA_SB16_ISA_DMA_MODE_REGISTER_16BIT         0xD6
#define MA_SB16_ISA_DMA_MODE_DEMAND                 0x00
#define MA_SB16_ISA_DMA_MODE_SINGLE                 0x40
#define MA_SB16_ISA_DMA_MODE_BLOCK                  0x80
#define MA_SB16_ISA_DMA_MODE_CASCADE                0xC0
#define MA_SB16_ISA_DMA_MODE_READ                   0x08
#define MA_SB16_ISA_DMA_MODE_WRITE                  0x04
#define MA_SB16_ISA_DMA_MODE_AUTOINIT               0x10

#define MA_SB16_ISA_DMA_ADDRESS_REGISTER_8BIT       0x00
#define MA_SB16_ISA_DMA_ADDRESS_REGISTER_16BIT      0xC0

#define MA_SB16_ISA_DMA_COUNT_REGISTER_8BIT         0x01
#define MA_SB16_ISA_DMA_COUNT_REGISTER_16BIT        0xC2

typedef struct
{
    unsigned short basePort;
    unsigned short irq;
    unsigned short dmaChannel8;
    unsigned short dmaChannel16;
} ma_sb16_env;

typedef struct
{
    ma_sb16_env env;
    int interruptVector;
    unsigned short interruptMaskPort;
    unsigned char interruptMaskBit;
    unsigned char oldInterruptMask;
    unsigned char oldCascadeInterruptMask;
    _go32_dpmi_seginfo oldInterruptHandler;
    _go32_dpmi_seginfo interruptHandler;
    unsigned char versionMajor;
    unsigned char versionMinor;
} ma_context_state_sb16;

typedef struct
{
    unsigned short dmaChannel;
    int dmaBufferSelector;
    ma_uint32 dmaBufferAddress;
    ma_uint32 dmaBufferSizeInBytes;
    void* pIntermediaryBuffer;
    volatile ma_uint32 completedPeriodCount;    /* Written by the interrupt handler and read by step(). */
    ma_uint32 processedPeriodCount;             /* Last completed period processed by step(). */
} ma_device_state_sb16;


static ma_format ma_gSupportedFormatsSB16[] =
{
    ma_format_s16,
    ma_format_u8
};

static ma_uint32 ma_gSupportedChannelsSB16[] =
{
    2,
    1
};

static ma_uint32 ma_gMinSampleRateSB16 = 8000;
static ma_uint32 ma_gMaxSampleRateSB16 = 44100;

/*
Only a single context and device are allowed to be initialized at a time. We need to store global pointers
to their states for the benefit of the interrupt handler. The context owns the handler.
*/
static ma_context_state_sb16* ma_gContextStateSB16 = NULL;
static ma_device_state_sb16* ma_gDeviceStateSB16 = NULL;



static ma_context_state_sb16* ma_context_get_backend_state__sb16(ma_context* pContext)
{
    return (ma_context_state_sb16*)ma_context_get_backend_state(pContext);
}

static ma_device_state_sb16* ma_device_get_backend_state__sb16(ma_device* pDevice)
{
    return (ma_device_state_sb16*)ma_device_get_backend_state(pDevice);
}


/*
We're not using DJGPP's inportb() and outportb() directly because we call it from an interrupt
which requires the memory for called functions to be locked. To deal with this we're going to
use inlined assembly directly, called from a force-inlined function.
*/
static MA_INLINE unsigned char ma_sb16_inportb(unsigned short port)
{
    unsigned char value;

    __asm__ __volatile__("inb %1, %0" : "=a" (value) : "dN" (port));

    return value;
}

static MA_INLINE void ma_sb16_outportb(unsigned short port, unsigned char value)
{
    __asm__ __volatile__("outb %1, %0" : : "dN" (port), "a" (value));
}


static int ma_sb16_lock_code(void (*pCode)(void), unsigned long sizeInBytes)
{
    return _go32_dpmi_lock_code((void*)(unsigned long)pCode, sizeInBytes);
}

static int ma_sb16_unlock_code(void (*pCode)(void), unsigned long sizeInBytes)
{
    __dpmi_meminfo info;

    if (__dpmi_get_segment_base_address(_my_cs(), &info.address) != 0) {
        return -1;
    }

    info.handle = 0;
    info.size = sizeInBytes;
    info.address += (unsigned long)pCode;

    return __dpmi_unlock_linear_region(&info);
}


static int ma_sb16_lock_data(void* pData, unsigned long sizeInBytes)
{
    return _go32_dpmi_lock_data(pData, sizeInBytes);
}

static int ma_sb16_unlock_data(void* pData, unsigned long sizeInBytes)
{
    __dpmi_meminfo info;

    if (__dpmi_get_segment_base_address(_my_ds(), &info.address) != 0) {
        return -1;
    }

    info.handle = 0;
    info.size = sizeInBytes;
    info.address += (unsigned long)pData;

    return __dpmi_unlock_linear_region(&info);
}


static MA_SB16_NO_REORDER void ma_sb16_locked_code_begin(void)
{
}

static MA_SB16_NO_REORDER unsigned char ma_sb16_acknowledge_dsp_interrupt(ma_context_state_sb16* pContextStateSB16)
{
    unsigned char interruptStatus;

    ma_sb16_outportb(pContextStateSB16->env.basePort | MA_SB16_MIXER_PORT, MA_SB16_MIXER_INTERRUPT_STATUS_REGISTER);
    interruptStatus = ma_sb16_inportb(pContextStateSB16->env.basePort | MA_SB16_MIXER_DATA_PORT);

    if ((interruptStatus & MA_SB16_INTERRUPT_STATUS_8BIT) != 0) {
        ma_sb16_inportb(pContextStateSB16->env.basePort | MA_SB16_DSP_READY_READ_PORT);
    }
    if ((interruptStatus & MA_SB16_INTERRUPT_STATUS_16BIT) != 0) {
        ma_sb16_inportb(pContextStateSB16->env.basePort | MA_SB16_DSP_ACK_16BIT_PORT);
    }

    return interruptStatus;
}

static MA_SB16_NO_REORDER void ma_sb16_interrupt_handler(void)
{
    ma_context_state_sb16* pContextStateSB16;
    ma_device_state_sb16* pDeviceStateSB16;
    unsigned char interruptStatus;

    pContextStateSB16 = ma_gContextStateSB16;
    pDeviceStateSB16  = ma_gDeviceStateSB16;

    interruptStatus = ma_sb16_acknowledge_dsp_interrupt(pContextStateSB16);

    if (pDeviceStateSB16 != NULL && (interruptStatus & (MA_SB16_INTERRUPT_STATUS_8BIT | MA_SB16_INTERRUPT_STATUS_16BIT)) != 0) {
        pDeviceStateSB16->completedPeriodCount += 1;
    }

    /* End of iterrupt acknowledgement. Slave PIC interrupts must be acknowledged on both controllers, slave first. */
    if (pContextStateSB16->env.irq >= 8) {
        ma_sb16_outportb(MA_SB16_PIC_COMMAND_REGISTER_SLAVE, MA_SB16_PIC_END_OF_INTERRUPT);
    }

    ma_sb16_outportb(MA_SB16_PIC_COMMAND_REGISTER_MASTER, MA_SB16_PIC_END_OF_INTERRUPT);
}

static MA_SB16_NO_REORDER void ma_sb16_locked_code_end(void)
{
}


static void ma_sb16_restore_interrupt_mask(unsigned short interruptMaskPort, unsigned char interruptMaskBit, unsigned char oldInterruptMask)
{
    unsigned char mask;

    mask = ma_sb16_inportb(interruptMaskPort);
    mask &= ~interruptMaskBit;
    mask |= oldInterruptMask & interruptMaskBit;
    ma_sb16_outportb(interruptMaskPort, mask);
}


static void ma_backend_info__sb16(ma_device_backend_info* pBackendInfo)
{
    pBackendInfo->pName = "Sound Blaster 16";
    pBackendInfo->noAudioThread = MA_TRUE;
}


static unsigned long ma_sb16_get_ticks(void)
{
    return (unsigned long)biostime(0, 0);
}

static unsigned long ma_sb16_get_elapsed_ticks(unsigned long startTicks)
{
    unsigned long currentTicks;

    currentTicks = ma_sb16_get_ticks();
    if (currentTicks < startTicks) {
        currentTicks += MA_SB16_TICKS_PER_DAY;
    }

    return currentTicks - startTicks;
}


static unsigned short ma_sb16_env_parse_value(const char* pValue, int base, unsigned short fallbackValue)
{
    char* pEnd;
    unsigned long value;

    /* strtoul() accepts signs and leading whitespace. Neither belongs here. */
    if (pValue[0] == '-' || pValue[0] == '+' || isspace((unsigned char)pValue[0])) {
        return fallbackValue;
    }

    value = strtoul(pValue, &pEnd, base);
    if (pEnd == pValue || value > USHRT_MAX || (*pEnd != '\0' && *pEnd != ' ' && *pEnd != '\t')) {
        return fallbackValue;
    }

    return (unsigned short)value;
}

static ma_sb16_env ma_sb16_env_parse(void)
{
    ma_sb16_env env;
    char* p;

    env.basePort     = 0x220;
    env.irq          = 7;
    env.dmaChannel8  = 1;
    env.dmaChannel16 = 5;

    p = getenv("BLASTER");
    if (p != NULL) {
        for (;;) {
            /* Skip whitespace. */
            while (*p == ' ' || *p == '\t') {
                p += 1;
            }

            if (*p == '\0') {
                break;
            }

            /* Parse the segment. */
            if (p[0] == 'A' || p[0] == 'a') {
                p += 1;
                env.basePort = ma_sb16_env_parse_value(p, 16, env.basePort);
                /*printf("A%u\n", (unsigned int)env.basePort);*/
            } else if (p[0] == 'I' || p[0] == 'i') {
                p += 1;
                env.irq = ma_sb16_env_parse_value(p, 10, env.irq);
                /*printf("I%u\n", (unsigned int)env.irq);*/
            } else if (p[0] == 'D' || p[0] == 'd') {
                p += 1;
                env.dmaChannel8 = ma_sb16_env_parse_value(p, 10, env.dmaChannel8);
                /*printf("D%u\n", (unsigned int)env.dmaChannel8);*/
            } else if (p[0] == 'H' || p[0] == 'h') {
                p += 1;
                env.dmaChannel16 = ma_sb16_env_parse_value(p, 10, env.dmaChannel16);
                /*printf("H%u\n", (unsigned int)env.dmaChannel16);*/
            }

            /* Skip the rest of this segment, including any unknown or ignored setting. */
            while (*p != '\0' && *p != ' ' && *p != '\t') {
                p += 1;
            }
        }
    }

    return env;
}

#define MA_SB16_WAIT_PORT(port, readyValue) \
    do { \
        timeoutStart = ma_sb16_get_ticks(); \
        while ((ma_sb16_inportb(env.basePort | (port)) & 0x80) != (readyValue)) { \
            if (ma_sb16_get_elapsed_ticks(timeoutStart) > MA_SB16_TIMEOUT_TICKS) { \
                return MA_NO_DEVICE; \
            } \
        } \
    } while (0)

#define MA_SB16_WAIT_READY_READ_PORT() MA_SB16_WAIT_PORT(MA_SB16_DSP_READY_READ_PORT, 0x80)
#define MA_SB16_WAIT_WRITE_PORT()      MA_SB16_WAIT_PORT(MA_SB16_DSP_WRITE_PORT,      0x00)

static ma_result ma_context_init__sb16(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    ma_context_state_sb16* pContextStateSB16;
    ma_sb16_env env;
    int interruptVector;
    unsigned short interruptMaskPort;
    unsigned char interruptMaskBit;
    unsigned char oldInterruptMask;
    _go32_dpmi_seginfo oldInterruptHandler;
    _go32_dpmi_seginfo interruptHandler;
    unsigned char versionMajor;
    unsigned char versionMinor;
    unsigned long timeoutStart;
    __dpmi_version_ret dpmiVersion;
    ma_result result;

    (void)pContextBackendConfig;

    *ppContextState = NULL;

    if (ma_gContextStateSB16 != NULL) {
        ma_log_postf(ma_context_get_log(pContext), MA_LOG_LEVEL_ERROR, "[SB16] Only a single context can be initialized at a time.");
        return MA_INVALID_OPERATION;
    }

    /*
    Creative wants us to read settings from the BLASTER environment variable. We don't hard fail
    if the BLASTER environment variable is not set - it'll just use defaults.
    */
    env = ma_sb16_env_parse();

    /* IRQ2 on the ISA bus is routed through IRQ9 on the slave PIC. */
    if (env.irq == 2) {
        env.irq = 9;
    }

    if (env.irq != 5 && env.irq != 7 && env.irq != 9 && env.irq != 10) {
        return MA_INVALID_DEVICE_CONFIG;
    }

    __dpmi_get_version(&dpmiVersion);

    if (env.irq < 8) {
        interruptVector   = dpmiVersion.master_pic + env.irq;
        interruptMaskPort = MA_SB16_PIC_MASK_REGISTER_MASTER;
    } else {
        interruptVector   = dpmiVersion.slave_pic + (env.irq - 8);
        interruptMaskPort = MA_SB16_PIC_MASK_REGISTER_SLAVE;
    }

    interruptMaskBit = 1 << (env.irq & 7);


    /*
    Now that we have the environment parsed we can read the hardware configuration and detect a
    SoundBlaster 16 compatible DSP.
    */

    /* First thing to do is reset the DSP. */
    ma_sb16_outportb(env.basePort | MA_SB16_DSP_RESET_PORT, MA_SB16_DSP_RESET_CMD);
    ma_sb16_sleep(1);   /* Sound Blaster documentation says to wait 3 microseconds. We'll do 1 milliseconds. */
    ma_sb16_outportb(env.basePort | MA_SB16_DSP_RESET_PORT, 0x00);

    /* Now check the result of the reset. */
    MA_SB16_WAIT_READY_READ_PORT();
    if (ma_sb16_inportb(env.basePort | MA_SB16_DSP_READ_PORT) != 0xAA) {
        return MA_NO_DEVICE;
    }

    /* We can now query the version. A major version of 4 is what we're after. */
    MA_SB16_WAIT_WRITE_PORT();
    ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, MA_SB16_DSP_GET_VERSION);

    MA_SB16_WAIT_READY_READ_PORT();
    versionMajor = ma_sb16_inportb(env.basePort | MA_SB16_DSP_READ_PORT);
    MA_SB16_WAIT_READY_READ_PORT();
    versionMinor = ma_sb16_inportb(env.basePort | MA_SB16_DSP_READ_PORT);

    if (versionMajor != 4) {
        return MA_NO_DEVICE;
    }

    /* Getting here means we have detected an SB16-compatible device. */
    MA_SB16_ZERO_MEMORY(&oldInterruptHandler, sizeof(oldInterruptHandler));
    if (_go32_dpmi_get_protected_mode_interrupt_vector(interruptVector, &oldInterruptHandler) != 0) {
        return MA_ERROR;
    }

    if (ma_sb16_lock_code(ma_sb16_locked_code_begin, (unsigned long)ma_sb16_locked_code_end - (unsigned long)ma_sb16_locked_code_begin) != 0) {
        return MA_ERROR;
    }

    /* The wrapper supplies the interrupt return code, stack, and protected-mode code selector. */
    MA_SB16_ZERO_MEMORY(&interruptHandler, sizeof(interruptHandler));
    interruptHandler.pm_offset = (unsigned long)ma_sb16_interrupt_handler;
    if (_go32_dpmi_allocate_iret_wrapper(&interruptHandler) != 0) {
        result = MA_OUT_OF_MEMORY;
        goto error_0;
    }

    /* Keep the IRQ masked until a device unmasks it when it starts. */
    oldInterruptMask = ma_sb16_inportb(interruptMaskPort);
    ma_sb16_outportb(interruptMaskPort, oldInterruptMask | interruptMaskBit);

    if (_go32_dpmi_set_protected_mode_interrupt_vector(interruptVector, &interruptHandler) != 0) {
        result = MA_ERROR;
        goto error_1;
    }

    pContextStateSB16 = (ma_context_state_sb16*)ma_calloc(sizeof(*pContextStateSB16), ma_context_get_allocation_callbacks(pContext));
    if (pContextStateSB16 == NULL) {
        result = MA_OUT_OF_MEMORY;
        goto error_2;
    }

    pContextStateSB16->env                 = env;
    pContextStateSB16->interruptVector     = interruptVector;
    pContextStateSB16->interruptMaskPort   = interruptMaskPort;
    pContextStateSB16->interruptMaskBit    = interruptMaskBit;
    pContextStateSB16->oldInterruptMask    = oldInterruptMask;
    pContextStateSB16->oldInterruptHandler = oldInterruptHandler;
    pContextStateSB16->interruptHandler    = interruptHandler;
    pContextStateSB16->versionMajor        = versionMajor;
    pContextStateSB16->versionMinor        = versionMinor;

    /* This can be changed when the device starts. The original value will be restored when the device is uninitialized. */
    if (env.irq >= 8) {
        pContextStateSB16->oldCascadeInterruptMask = ma_sb16_inportb(MA_SB16_PIC_MASK_REGISTER_MASTER) & MA_SB16_PIC_CASCADE_MASK_BIT;
    }

    /* Any data that is accessed by the interrupt handler needs to be locked. It'll be unlocked when the context is uninitialized. */
    if (ma_sb16_lock_data(&ma_gContextStateSB16, sizeof(ma_gContextStateSB16)) != 0) {
        result = MA_ERROR;
        goto error_3;
    }
    if (ma_sb16_lock_data(&ma_gDeviceStateSB16, sizeof(ma_gDeviceStateSB16)) != 0) {
        result = MA_ERROR;
        goto error_4;
    }
    if (ma_sb16_lock_data(pContextStateSB16, sizeof(*pContextStateSB16)) != 0) {
        result = MA_ERROR;
        goto error_5;
    }

    ma_gContextStateSB16 = pContextStateSB16;
    *ppContextState = pContextStateSB16;

    return MA_SUCCESS;

error_5:
    ma_sb16_unlock_data(&ma_gDeviceStateSB16, sizeof(ma_gDeviceStateSB16));
error_4:
    ma_sb16_unlock_data(&ma_gContextStateSB16, sizeof(ma_gContextStateSB16));
error_3:
    ma_free(pContextStateSB16, ma_context_get_allocation_callbacks(pContext));
error_2:
    _go32_dpmi_set_protected_mode_interrupt_vector(interruptVector, &oldInterruptHandler);
error_1:
    ma_sb16_restore_interrupt_mask(interruptMaskPort, interruptMaskBit, oldInterruptMask);
    _go32_dpmi_free_iret_wrapper(&interruptHandler);
error_0:
    ma_sb16_unlock_code(ma_sb16_locked_code_begin, (unsigned long)ma_sb16_locked_code_end - (unsigned long)ma_sb16_locked_code_begin);
    return result;
}

static void ma_context_uninit__sb16(ma_context* pContext)
{
    ma_context_state_sb16* pContextStateSB16 = (ma_context_state_sb16*)ma_context_get_backend_state(pContext);

    ma_sb16_outportb(pContextStateSB16->interruptMaskPort, ma_sb16_inportb(pContextStateSB16->interruptMaskPort) | pContextStateSB16->interruptMaskBit);
    if (_go32_dpmi_set_protected_mode_interrupt_vector(pContextStateSB16->interruptVector, &pContextStateSB16->oldInterruptHandler) != 0) {
        ma_log_postf(ma_context_get_log(pContext), MA_LOG_LEVEL_ERROR, "[SB16] Failed to restore the interrupt handler. Retaining its state and keeping the IRQ masked.");
        return;
    }

    ma_gContextStateSB16 = NULL;

    ma_sb16_restore_interrupt_mask(pContextStateSB16->interruptMaskPort, pContextStateSB16->interruptMaskBit, pContextStateSB16->oldInterruptMask);
    
    if (pContextStateSB16->env.irq >= 8) {
        ma_sb16_restore_interrupt_mask(MA_SB16_PIC_MASK_REGISTER_MASTER, MA_SB16_PIC_CASCADE_MASK_BIT, pContextStateSB16->oldCascadeInterruptMask);
    }

    _go32_dpmi_free_iret_wrapper(&pContextStateSB16->interruptHandler);
    ma_sb16_unlock_code(ma_sb16_locked_code_begin, (unsigned long)ma_sb16_locked_code_end - (unsigned long)ma_sb16_locked_code_begin);
    ma_sb16_unlock_data(pContextStateSB16, sizeof(*pContextStateSB16));
    ma_sb16_unlock_data(&ma_gDeviceStateSB16, sizeof(ma_gDeviceStateSB16));
    ma_sb16_unlock_data(&ma_gContextStateSB16, sizeof(ma_gContextStateSB16));
    ma_free(pContextStateSB16, ma_context_get_allocation_callbacks(pContext));
}

static ma_result ma_context_enumerate_devices__sb16(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pCallbackUserData)
{
    ma_device_info info;
    ma_uint32 minChannels = 0xFFFFFFFF;
    ma_uint32 maxChannels = 0;
    ma_uint32 i;

    (void)pContext;

    for (i = 0; i < ma_countof(ma_gSupportedChannelsSB16); i += 1) {
        if (minChannels > ma_gSupportedChannelsSB16[i]) {
            minChannels = ma_gSupportedChannelsSB16[i];
        }
        if (maxChannels < ma_gSupportedChannelsSB16[i]) {
            maxChannels = ma_gSupportedChannelsSB16[i];
        }
    }

    MA_SB16_ZERO_MEMORY(&info, sizeof(info));
    info.id.custom.i = 0;
    info.isDefault = MA_TRUE;

    for (i = 0; i < ma_countof(ma_gSupportedFormatsSB16); i += 1) {
        ma_device_info_add_native_data_format(&info, ma_gSupportedFormatsSB16[i], minChannels, maxChannels, ma_gMinSampleRateSB16, ma_gMaxSampleRateSB16);
    }

    /* Playback. */
    ma_strncpy_s(info.name, sizeof(info.name), "Default Playback Device", (size_t)-1);
    if (callback(ma_device_type_playback, &info, pCallbackUserData) == MA_DEVICE_ENUMERATION_ABORT) {
        return MA_SUCCESS;
    }

    /* Capture. */
    ma_strncpy_s(info.name, sizeof(info.name), "Default Capture Device", (size_t)-1);
    if (callback(ma_device_type_capture, &info, pCallbackUserData) == MA_DEVICE_ENUMERATION_ABORT) {
        return MA_SUCCESS;
    }

    return MA_SUCCESS;
}


static ma_format ma_sb16_find_best_format(ma_format desiredFormat)
{
    ma_format format;
    ma_uint32 i;

    format = ma_gSupportedFormatsSB16[0];

    if (desiredFormat != ma_format_unknown) {
        for (i = 0; i < ma_countof(ma_gSupportedFormatsSB16); i += 1) {
            if (ma_gSupportedFormatsSB16[i] == desiredFormat) {
                format = ma_gSupportedFormatsSB16[i];
                break;
            }
        }
    }

    return format;
}

static ma_uint32 ma_sb16_find_best_channels(ma_uint32 desiredChannels)
{
    ma_uint32 channels;
    ma_uint32 i;

    channels = ma_gSupportedChannelsSB16[0];

    if (desiredChannels != 0) {
        for (i = 0; i < ma_countof(ma_gSupportedChannelsSB16); i += 1) {
            if (ma_gSupportedChannelsSB16[i] == desiredChannels) {
                channels = ma_gSupportedChannelsSB16[i];
                break;
            }
        }
    }

    return channels;
}

static ma_uint32 ma_sb16_find_best_sample_rate(ma_uint32 desiredSampleRate)
{
    ma_uint32 rate;

    rate = ma_gMaxSampleRateSB16;

    if (desiredSampleRate != 0) {
        rate = desiredSampleRate;
        if (rate < ma_gMinSampleRateSB16) {
            rate = ma_gMinSampleRateSB16;
        }
        if (rate > ma_gMaxSampleRateSB16) {
            rate = ma_gMaxSampleRateSB16;
        }
    }

    return rate;
}


static ma_result ma_device_init__sb16(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    ma_context_state_sb16* pContextStateSB16 = ma_context_get_backend_state__sb16(ma_device_get_context(pDevice));
    ma_device_state_sb16* pDeviceStateSB16;
    ma_device_type deviceType;
    ma_device_descriptor* pDescriptor;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_uint32 periodCount;
    ma_uint32 periodSizeInFrames;
    ma_uint32 maxPeriodSizeInFrames;
    ma_uint32 dmaBufferSizeInBytes;
    ma_uint32 dmaBufferAddress;
    ma_uint32 intermediaryBufferSizeInBytes;
    unsigned short dmaChannel;
    int dmaBufferSelector;
    int dmaBufferSegment;
    ma_result result;

    (void)pDeviceBackendConfig;

    *ppDeviceState = NULL;

    if (ma_gDeviceStateSB16 != NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "[SB16] Only a single device can be initialized at a time.");
        return MA_INVALID_OPERATION;
    }

    /* Playback and capture are supported, but not loopback or duplex. */
    deviceType = ma_device_get_type(pDevice);
    if (deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    if (deviceType == ma_device_type_duplex) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    pDescriptor = pDescriptorPlayback;
    if (deviceType == ma_device_type_capture) {
        pDescriptor = pDescriptorCapture;
    }

    /* Negotiate our data format. */
    format      = ma_sb16_find_best_format(pDescriptor->format);
    channels    = ma_sb16_find_best_channels(pDescriptor->channels);
    sampleRate  = ma_sb16_find_best_sample_rate(pDescriptor->sampleRate);
    periodCount = MA_SB16_PERIOD_COUNT;

    if (format == ma_format_s16) {
        dmaChannel = pContextStateSB16->env.dmaChannel16;
        if (dmaChannel != 0 && dmaChannel != 1 && dmaChannel != 3 && dmaChannel != 5 && dmaChannel != 6 && dmaChannel != 7) {   /* SB16 also supports 16-bit samples on an 8-bit DMA channel. */
            return MA_INVALID_DEVICE_CONFIG;
        }
    } else {
        dmaChannel = pContextStateSB16->env.dmaChannel8;
        if (dmaChannel != 0 && dmaChannel != 1 && dmaChannel != 3) {
            return MA_INVALID_DEVICE_CONFIG;
        }
    }

    /*
    The total size of the buffer cannot exceed 65536 bytes because the active DMA buffer is not
    allowed to exceed 64K boundaries.
    */
    maxPeriodSizeInFrames = 65536 / ma_get_bytes_per_frame(format, channels) / periodCount; /* 65536 = max size in bytes. */

    periodSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pDescriptor, sampleRate);
    if (periodSizeInFrames > maxPeriodSizeInFrames) {
        periodSizeInFrames = maxPeriodSizeInFrames;
    }

    /* Allocate twice the buffer size so we can avoid crossing a physical 64K boundary. DOS allocations use 16-byte paragraphs. */
    intermediaryBufferSizeInBytes = periodSizeInFrames * ma_get_bytes_per_frame(format, channels);
    dmaBufferSizeInBytes = intermediaryBufferSizeInBytes * periodCount;

    dmaBufferSegment = __dpmi_allocate_dos_memory((dmaBufferSizeInBytes * 2 + 15) / 16, &dmaBufferSelector);
    if (dmaBufferSegment == -1) {
        return MA_OUT_OF_MEMORY;
    }

    dmaBufferAddress = dmaBufferSegment * 16;
    if ((dmaBufferAddress & 0xFFFF) + dmaBufferSizeInBytes > 65536) {
        dmaBufferAddress = (dmaBufferAddress + 0xFFFF) & ~0xFFFF;
    }

    /* Allocate one period of ordinary memory for transferring samples between the client and the DOS DMA buffer. */
    pDeviceStateSB16 = (ma_device_state_sb16*)ma_calloc(ma_align_64(sizeof(*pDeviceStateSB16)) + intermediaryBufferSizeInBytes, ma_device_get_allocation_callbacks(pDevice));
    if (pDeviceStateSB16 == NULL) {
        result = MA_OUT_OF_MEMORY;
        goto error_0;
    }

    /*
    The handler accesses the state, but not the intermediary sample buffer. This data needs to be
    locked. It's unlocked when the device is uninitialized.
    */
    if (ma_sb16_lock_data(pDeviceStateSB16, sizeof(*pDeviceStateSB16)) != 0) {
        result = MA_ERROR;
        goto error_1;
    }

    pDeviceStateSB16->dmaChannel           = dmaChannel;
    pDeviceStateSB16->dmaBufferSelector    = dmaBufferSelector;
    pDeviceStateSB16->dmaBufferAddress     = dmaBufferAddress;
    pDeviceStateSB16->dmaBufferSizeInBytes = dmaBufferSizeInBytes;
    pDeviceStateSB16->pIntermediaryBuffer  = ma_offset_ptr(pDeviceStateSB16, ma_align_64(sizeof(*pDeviceStateSB16)));

    /* We're done. */
    pDescriptor->format             = format;
    pDescriptor->channels           = channels;
    pDescriptor->sampleRate         = sampleRate;
    pDescriptor->periodCount        = periodCount;
    pDescriptor->periodSizeInFrames = periodSizeInFrames;
    ma_channel_map_init_standard(ma_standard_channel_map_default, pDescriptor->channelMap, ma_countof(pDescriptor->channelMap), pDescriptor->channels);

    ma_gDeviceStateSB16 = pDeviceStateSB16;
    *ppDeviceState = pDeviceStateSB16;

    return MA_SUCCESS;

error_1:
    ma_free(pDeviceStateSB16, ma_device_get_allocation_callbacks(pDevice));
error_0:
    __dpmi_free_dos_memory(dmaBufferSelector);
    return result;
}

static void ma_device_uninit__sb16(ma_device* pDevice)
{
    ma_context_state_sb16* pContextStateSB16 = ma_context_get_backend_state__sb16(ma_device_get_context(pDevice));
    ma_device_state_sb16* pDeviceStateSB16 = ma_device_get_backend_state__sb16(pDevice);

    ma_sb16_outportb(pContextStateSB16->interruptMaskPort, ma_sb16_inportb(pContextStateSB16->interruptMaskPort) | pContextStateSB16->interruptMaskBit);
    ma_gDeviceStateSB16 = NULL;

    __dpmi_free_dos_memory(pDeviceStateSB16->dmaBufferSelector);
    ma_sb16_unlock_data(pDeviceStateSB16, sizeof(*pDeviceStateSB16));
    ma_free(pDeviceStateSB16, ma_device_get_allocation_callbacks(pDevice));
}

static ma_result ma_device_start__sb16(ma_device* pDevice)
{
    ma_device_state_sb16* pDeviceStateSB16 = ma_device_get_backend_state__sb16(pDevice);
    ma_context_state_sb16* pContextStateSB16 = ma_context_get_backend_state__sb16(ma_device_get_context(pDevice));
    ma_sb16_env env;
    static const unsigned char dmaPageRegisters[8] = {0x87, 0x83, 0x81, 0x82, 0x00, 0x8B, 0x89, 0x8A};
    unsigned short dmaMaskRegister;
    unsigned short dmaModeRegister;
    unsigned short dmaFlipflopRegister;
    unsigned short dmaAddressRegister;
    unsigned short dmaCountRegister;
    unsigned char dmaMode;
    unsigned char dmaChannelIndex;
    ma_uint32 dmaAddress;
    ma_uint32 dmaTransferCount;
    int interruptState;

    env = pContextStateSB16->env;

    dmaChannelIndex  = pDeviceStateSB16->dmaChannel & 0x03;
    dmaAddress       = pDeviceStateSB16->dmaBufferAddress;
    dmaTransferCount = pDeviceStateSB16->dmaBufferSizeInBytes;

    /* The DMA channel determines the controller width, independently of the sample format. */
    if (pDeviceStateSB16->dmaChannel < 4) {
        dmaMaskRegister     = MA_SB16_ISA_DMA_MASK_REGISTER_8BIT;
        dmaModeRegister     = MA_SB16_ISA_DMA_MODE_REGISTER_8BIT;
        dmaFlipflopRegister = MA_SB16_ISA_DMA_FLIPFLOP_REGISTER_8BIT;
        dmaAddressRegister  = MA_SB16_ISA_DMA_ADDRESS_REGISTER_8BIT + dmaChannelIndex * 2;
        dmaCountRegister    = MA_SB16_ISA_DMA_COUNT_REGISTER_8BIT   + dmaChannelIndex * 2;
    } else {
        dmaMaskRegister     = MA_SB16_ISA_DMA_MASK_REGISTER_16BIT;
        dmaModeRegister     = MA_SB16_ISA_DMA_MODE_REGISTER_16BIT;
        dmaFlipflopRegister = MA_SB16_ISA_DMA_FLIPFLOP_REGISTER_16BIT;
        dmaAddressRegister  = MA_SB16_ISA_DMA_ADDRESS_REGISTER_16BIT + dmaChannelIndex * 4;
        dmaCountRegister    = MA_SB16_ISA_DMA_COUNT_REGISTER_16BIT   + dmaChannelIndex * 4;

        /* Channels 5-7 use 16-bit word addresses and counts. */
        dmaAddress       /= sizeof(ma_int16);
        dmaTransferCount /= sizeof(ma_int16);
    }

    dmaTransferCount -= 1; /* The controller stores the transfer count minus one. */

    dmaMode  = dmaChannelIndex;
    dmaMode |= MA_SB16_ISA_DMA_MODE_DEMAND;     /* Could also be SINGLE. Not sure which would be best. */
    dmaMode |= MA_SB16_ISA_DMA_MODE_AUTOINIT;   /* For double buffering. */
    if (ma_device_get_type(pDevice) == ma_device_type_playback) {
        dmaMode |= MA_SB16_ISA_DMA_MODE_READ;
    } else {
        dmaMode |= MA_SB16_ISA_DMA_MODE_WRITE;
    }

    /* Keep the channel masked until the DMA buffer and DSP are ready. This will be unmasked when startup is complete. */
    ma_sb16_outportb(dmaMaskRegister, dmaChannelIndex | 0x04);
    {
        /* Configure the DMA. */
        interruptState = __dpmi_get_and_disable_virtual_interrupt_state();
        {
            ma_sb16_outportb(dmaModeRegister, dmaMode);
            ma_sb16_outportb(dmaPageRegisters[pDeviceStateSB16->dmaChannel], (pDeviceStateSB16->dmaBufferAddress >> 16) & 0xFF);

            ma_sb16_outportb(dmaFlipflopRegister, 0);
            ma_sb16_outportb(dmaAddressRegister, (dmaAddress >> 0) & 0xFF);
            ma_sb16_outportb(dmaAddressRegister, (dmaAddress >> 8) & 0xFF);

            ma_sb16_outportb(dmaFlipflopRegister, 0);
            ma_sb16_outportb(dmaCountRegister, (dmaTransferCount >> 0) & 0xFF);
            ma_sb16_outportb(dmaCountRegister, (dmaTransferCount >> 8) & 0xFF);
        }
        __dpmi_get_and_set_virtual_interrupt_state(interruptState);

        /* Configure the DSP. */
        {
            ma_device_type deviceType;
            ma_format format;
            ma_uint32 channels;
            ma_uint32 sampleRate;
            unsigned char dspCommand;
            unsigned char dspMode;
            ma_uint32 dspTransferCount;
            unsigned long timeoutStart;

            deviceType = ma_device_get_type(pDevice);
            format     = ma_device_get_internal_format(pDevice, deviceType);
            channels   = ma_device_get_internal_channels(pDevice, deviceType);
            sampleRate = ma_device_get_internal_sample_rate(pDevice, deviceType);

            MA_SB16_WAIT_WRITE_PORT();
            if (deviceType == ma_device_type_playback) {
                ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, MA_SB16_DSP_SET_OUTPUT_SAMPLE_RATE);
            } else {
                ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, MA_SB16_DSP_SET_INPUT_SAMPLE_RATE);
            }

            /* The sample rate is sent high byte first, unlike the DMA address and count. */
            MA_SB16_WAIT_WRITE_PORT();
            ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, (sampleRate >> 8) & 0xFF);
            MA_SB16_WAIT_WRITE_PORT();
            ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, (sampleRate >> 0) & 0xFF);

            if (format == ma_format_s16) {
                dspCommand = MA_SB16_DSP_DMA_16BIT;
                dspMode    = MA_SB16_DSP_MODE_SIGNED;
            } else if (format == ma_format_u8) {
                dspCommand = MA_SB16_DSP_DMA_8BIT;
                dspMode    = MA_SB16_DSP_MODE_UNSIGNED;
            } else {
                /* Should never get here. If we do, it's a bug in miniaudio. */
                MA_SB16_ASSERT(!"[SB16] Unexpected internal format.");
                return MA_ERROR;
            }

            dspCommand |= MA_SB16_DSP_DMA_AUTOINIT | MA_SB16_DSP_DMA_FIFO;
            if (deviceType == ma_device_type_capture) {
                dspCommand |= MA_SB16_DSP_DMA_INPUT;
            }

            if (channels == 2) {
                dspMode |= MA_SB16_DSP_MODE_STEREO;
            } else {
                dspMode |= MA_SB16_DSP_MODE_MONO;
            }

            /* The DMA buffer holds two periods. The DSP count is one period in samples, minus one. */
            dspTransferCount = (pDeviceStateSB16->dmaBufferSizeInBytes / MA_SB16_PERIOD_COUNT) / ma_get_bytes_per_sample(format);
            dspTransferCount -= 1;

            MA_SB16_WAIT_WRITE_PORT();
            ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, dspCommand);
            MA_SB16_WAIT_WRITE_PORT();
            ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, dspMode);
            MA_SB16_WAIT_WRITE_PORT();
            ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, (dspTransferCount >> 0) & 0xFF);
            MA_SB16_WAIT_WRITE_PORT();
            ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, (dspTransferCount >> 8) & 0xFF);
        }

        /* Make sure the buffer is pre-silenced or filled with data from the client. I don't want random noise in the buffer. */
        if (ma_device_get_type(pDevice) == ma_device_type_playback) {
            ma_uint32 periodSizeInBytes;
            ma_uint32 periodSizeInFrames;
            ma_uint32 iPeriod;
            ma_result result;

            periodSizeInBytes  = pDeviceStateSB16->dmaBufferSizeInBytes / MA_SB16_PERIOD_COUNT;
            periodSizeInFrames = periodSizeInBytes / ma_get_bytes_per_frame(ma_device_get_internal_format(pDevice, ma_device_type_playback), ma_device_get_internal_channels(pDevice, ma_device_type_playback));

            for (iPeriod = 0; iPeriod < MA_SB16_PERIOD_COUNT; iPeriod += 1) {
                result = ma_device_handle_backend_data_callback(pDevice, pDeviceStateSB16->pIntermediaryBuffer, NULL, periodSizeInFrames);
                if (result != MA_SUCCESS) {
                    return result;
                }

                dosmemput(pDeviceStateSB16->pIntermediaryBuffer, periodSizeInBytes, pDeviceStateSB16->dmaBufferAddress + iPeriod * periodSizeInBytes);
            }
        }

        /* Reset period tracking and enable the IRQ before unmasking DMA. */
        interruptState = __dpmi_get_and_disable_virtual_interrupt_state();
        {
            pDeviceStateSB16->completedPeriodCount = 0;
            pDeviceStateSB16->processedPeriodCount = 0;

            ma_sb16_outportb(pContextStateSB16->interruptMaskPort, ma_sb16_inportb(pContextStateSB16->interruptMaskPort) & ~pContextStateSB16->interruptMaskBit);

            /* Slave PIC interrupts must also pass through IRQ2 on the master PIC. */
            if (env.irq >= 8) {
                ma_sb16_outportb(MA_SB16_PIC_MASK_REGISTER_MASTER, ma_sb16_inportb(MA_SB16_PIC_MASK_REGISTER_MASTER) & ~MA_SB16_PIC_CASCADE_MASK_BIT);
            }
        }
        __dpmi_get_and_set_virtual_interrupt_state(interruptState);
    }
    ma_sb16_outportb(dmaMaskRegister, dmaChannelIndex);

    return MA_SUCCESS;
}

static ma_result ma_device_stop__sb16(ma_device* pDevice)
{
    ma_device_state_sb16* pDeviceStateSB16 = ma_device_get_backend_state__sb16(pDevice);
    ma_context_state_sb16* pContextStateSB16 = ma_context_get_backend_state__sb16(ma_device_get_context(pDevice));
    ma_device_type deviceType = ma_device_get_type(pDevice);
    ma_sb16_env env;
    unsigned short dmaMaskRegister;
    unsigned char dspCommand;
    unsigned long timeoutStart;
    int interruptState;
    ma_result result = MA_SUCCESS;

    env = pContextStateSB16->env;

    if (pDeviceStateSB16->dmaChannel < 4) {
        dmaMaskRegister = MA_SB16_ISA_DMA_MASK_REGISTER_8BIT;
    } else {
        dmaMaskRegister = MA_SB16_ISA_DMA_MASK_REGISTER_16BIT;
    }

    if (ma_device_get_internal_format(pDevice, deviceType) == ma_format_s16) {
        dspCommand = MA_SB16_DSP_PAUSE_DMA_16BIT;
    } else {
        dspCommand = MA_SB16_DSP_PAUSE_DMA_8BIT;
    }

    ma_sb16_outportb(dmaMaskRegister, (pDeviceStateSB16->dmaChannel & 0x03) | 0x04);

    interruptState = __dpmi_get_and_disable_virtual_interrupt_state();
    {
        ma_sb16_outportb(pContextStateSB16->interruptMaskPort, ma_sb16_inportb(pContextStateSB16->interruptMaskPort) | pContextStateSB16->interruptMaskBit);
    }
    __dpmi_get_and_set_virtual_interrupt_state(interruptState);

    /* Keep timer interrupts enabled while waiting. A timeout must still acknowledge the DSP below. */
    timeoutStart = ma_sb16_get_ticks();
    while ((ma_sb16_inportb(env.basePort | MA_SB16_DSP_WRITE_PORT) & 0x80) != 0) {
        if (ma_sb16_get_elapsed_ticks(timeoutStart) > MA_SB16_TIMEOUT_TICKS) {
            result = MA_NO_DEVICE;
            goto error_0;
        }
    }

    ma_sb16_outportb(env.basePort | MA_SB16_DSP_WRITE_PORT, dspCommand);

error_0:
    /*
    The IRQ is masked, so acknowledge any pending DSP interrupt here. Otherwise, an old interrupt
    could arrive when start() unmasks the IRQ and incorrectly increment completedPeriodCount. This
    could cause step() to overwrite the prefilled audio before it has played, or read capture data
    before it is ready.
    */
    ma_sb16_acknowledge_dsp_interrupt(pContextStateSB16);

    return result;
}

static ma_result ma_device_step__sb16(ma_device* pDevice, ma_blocking_mode blockingMode)
{
    ma_device_state_sb16* pDeviceStateSB16 = ma_device_get_backend_state__sb16(pDevice);
    ma_device_type deviceType = ma_device_get_type(pDevice);
    ma_uint32 periodSizeInBytes;
    ma_uint32 periodSizeInFrames;
    ma_uint32 periodIndex;
    ma_uint32 completedPeriodCount;
    ma_result result;

    periodSizeInBytes  = pDeviceStateSB16->dmaBufferSizeInBytes / MA_SB16_PERIOD_COUNT;
    periodSizeInFrames = periodSizeInBytes / ma_get_bytes_per_frame(ma_device_get_internal_format(pDevice, deviceType), ma_device_get_internal_channels(pDevice, deviceType));

    for (;;) {
        if (!ma_device_is_started(pDevice)) {
            return MA_DEVICE_NOT_STARTED;
        }

        if (pDeviceStateSB16->completedPeriodCount != pDeviceStateSB16->processedPeriodCount) {
            if (deviceType == ma_device_type_playback) {
                result = ma_device_handle_backend_data_callback(pDevice, pDeviceStateSB16->pIntermediaryBuffer, NULL, periodSizeInFrames);
                if (result != MA_SUCCESS) {
                    return result;
                }

                completedPeriodCount = pDeviceStateSB16->completedPeriodCount;
                periodIndex = (completedPeriodCount - 1) % MA_SB16_PERIOD_COUNT;
                dosmemput(pDeviceStateSB16->pIntermediaryBuffer, periodSizeInBytes, pDeviceStateSB16->dmaBufferAddress + periodIndex * periodSizeInBytes);
            } else {
                /* Copy the completed period before the client callback, while DMA fills the other period. */
                completedPeriodCount = pDeviceStateSB16->completedPeriodCount;
                periodIndex = (completedPeriodCount - 1) % MA_SB16_PERIOD_COUNT;
                dosmemget(pDeviceStateSB16->dmaBufferAddress + periodIndex * periodSizeInBytes, periodSizeInBytes, pDeviceStateSB16->pIntermediaryBuffer);

                result = ma_device_handle_backend_data_callback(pDevice, NULL, pDeviceStateSB16->pIntermediaryBuffer, periodSizeInFrames);
                if (result != MA_SUCCESS) {
                    return result;
                }
            }

            pDeviceStateSB16->processedPeriodCount = completedPeriodCount;

            break;
        }

        if (blockingMode == MA_BLOCKING_MODE_NON_BLOCKING) {
            break;
        }
    }

    return MA_SUCCESS;
}

static ma_device_backend_vtable ma_gDeviceBackendVTable_SB16 =
{
    ma_backend_info__sb16,
    ma_context_init__sb16,
    ma_context_uninit__sb16,
    ma_context_enumerate_devices__sb16,
    ma_device_init__sb16,
    ma_device_uninit__sb16,
    ma_device_start__sb16,
    ma_device_stop__sb16,
    ma_device_step__sb16,
    NULL    /* onDeviceWakeup */
};

ma_device_backend_vtable* ma_device_backend_sb16 = &ma_gDeviceBackendVTable_SB16;
#else
ma_device_backend_vtable* ma_device_backend_sb16 = NULL;
#endif /* MA_HAS_SB16 */

MA_API ma_device_backend_vtable* ma_sb16_get_vtable(void)
{
    return ma_device_backend_sb16;
}

MA_API ma_context_config_sb16 ma_context_config_sb16_init(void)
{
    ma_context_config_sb16 config;

    config._unused = 0;

    return config;
}

MA_API ma_device_config_sb16 ma_device_config_sb16_init(void)
{
    ma_device_config_sb16 config;

    config._unused = 0;

    return config;
}

#endif /* miniaudio_backend_sb16_c */
