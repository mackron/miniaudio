#ifndef miniaudio_backend_pipewire_c
#define miniaudio_backend_pipewire_c

#include "miniaudio_pipewire.h"

/* This is temporary until this is merged into miniaudio proper. */
MA_API MA_NO_INLINE int ma_pipewire_itoa_s(int value, char* dst, size_t dstSizeInBytes, int radix)
{
    int sign;
    unsigned int valueU;
    char* dstEnd;

    if (dst == NULL || dstSizeInBytes == 0) {
        return 22;
    }
    if (radix < 2 || radix > 36) {
        dst[0] = '\0';
        return 22;
    }

    sign = (value < 0 && radix == 10) ? -1 : 1;     /* The negative sign is only used when the base is 10. */

    if (value < 0) {
        valueU = -value;
    } else {
        valueU = value;
    }

    dstEnd = dst;
    do
    {
        int remainder = valueU % radix;
        if (remainder > 9) {
            *dstEnd = (char)((remainder - 10) + 'a');
        } else {
            *dstEnd = (char)(remainder + '0');
        }

        dstEnd += 1;
        dstSizeInBytes -= 1;
        valueU /= radix;
    } while (dstSizeInBytes > 0 && valueU > 0);

    if (dstSizeInBytes == 0) {
        dst[0] = '\0';
        return 22;  /* Ran out of room in the output buffer. */
    }

    if (sign < 0) {
        *dstEnd++ = '-';
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes == 0) {
        dst[0] = '\0';
        return 22;  /* Ran out of room in the output buffer. */
    }

    *dstEnd = '\0';


    /* At this point the string will be reversed. */
    dstEnd -= 1;
    while (dst < dstEnd) {
        char temp = *dst;
        *dst = *dstEnd;
        *dstEnd = temp;

        dst += 1;
        dstEnd -= 1;
    }

    return 0;
}

MA_API MA_NO_INLINE int ma_pipewire_strcat_s(char* dst, size_t dstSizeInBytes, const char* src)
{
    char* dstorig;

    if (dst == 0) {
        return 22;
    }
    if (dstSizeInBytes == 0) {
        return 34;
    }
    if (src == 0) {
        dst[0] = '\0';
        return 22;
    }

    dstorig = dst;

    while (dstSizeInBytes > 0 && dst[0] != '\0') {
        dst += 1;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes == 0) {
        return 22;  /* Unterminated. */
    }


    while (dstSizeInBytes > 0 && src[0] != '\0') {
        *dst++ = *src++;
        dstSizeInBytes -= 1;
    }

    if (dstSizeInBytes > 0) {
        dst[0] = '\0';
    } else {
        dstorig[0] = '\0';
        return 34;
    }

    return 0;
}


#include <string.h> /* memset() */
#include <assert.h> /* assert() */

#ifndef MA_PIPEWIRE_ASSERT
#define MA_PIPEWIRE_ASSERT(x) assert(x)
#endif

#ifndef MA_PIPEWIRE_COPY_MEMORY
#define MA_PIPEWIRE_COPY_MEMORY(dest, src, size) memcpy((dest), (src), (size))
#endif

#ifndef MA_PIPEWIRE_ZERO_MEMORY
#define MA_PIPEWIRE_ZERO_MEMORY(x, size) memset((x), 0, (size))
#endif

#ifndef MA_PIPEWIRE_ZERO_OBJECT
#define MA_PIPEWIRE_ZERO_OBJECT(x) MA_PIPEWIRE_ZERO_MEMORY((x), sizeof(*(x)))
#endif


#if defined(MA_LINUX)
    #if !defined(MA_NO_RUNTIME_LINKING) || (defined(__STDC_VERSION__) || defined(__cplusplus))   /* <-- PipeWire cannot be used with C89 mode (__STDC_VERSION__ is only defined starting with C90). */
        #define MA_SUPPORT_PIPEWIRE
    #endif
#endif

#if defined(MA_SUPPORT_PIPEWIRE) && !defined(MA_NO_PIPEWIRE) && (!defined(MA_ENABLE_ONLY_SPECIFIC_BACKENDS) || defined(MA_ENABLE_PIPEWIRE))
    #define MA_HAS_PIPEWIRE
#endif

#if defined(MA_HAS_PIPEWIRE)
#if defined(MA_NO_RUNTIME_LINKING)
#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    #if defined(__clang__)
        #pragma GCC diagnostic ignored "-Wc99-extensions"
        #pragma GCC diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
        #pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
    #endif
#endif

#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>

#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#include <spa/utils/dict.h>
#include <spa/pod/command.h>
#include <spa/buffer/buffer.h>

#define ma_spa_source_event_func_t          spa_source_event_func_t

#define ma_spa_direction                    spa_direction
#define ma_spa_audio_format                 spa_audio_format
#define ma_spa_audio_channel                spa_audio_channel
#define ma_spa_callbacks                    spa_callbacks
#define ma_spa_interface                    spa_interface
#define ma_spa_dict_item                    spa_dict_item
#define MA_SPA_DICT_ITEM_INIT               SPA_DICT_ITEM_INIT
#define ma_spa_dict                         spa_dict
#define MA_SPA_DICT_INIT                    SPA_DICT_INIT
#define ma_spa_dict_lookup                  spa_dict_lookup
#define ma_spa_chunk                        spa_chunk
#define ma_spa_data                         spa_data
#define ma_spa_meta                         spa_meta
#define ma_spa_buffer                       spa_buffer
#define ma_spa_list                         spa_list
#define ma_spa_hook                         spa_hook
#define ma_spa_pod                          spa_pod
#define ma_spa_pod_id                       spa_pod_id
#define ma_spa_pod_int                      spa_pod_int
#define ma_spa_choice_type                  spa_choice_type
#define ma_spa_pod_choice                   spa_pod_choice
#define ma_spa_pod_choice_body              spa_pod_choice_body
#define ma_spa_pod_array_body               spa_pod_array_body
#define ma_spa_pod_array                    spa_pod_array
#define ma_spa_pod_object_body              spa_pod_object_body
#define ma_spa_pod_object                   spa_pod_object
#define ma_spa_pod_prop                     spa_pod_prop
#define ma_spa_command                      spa_command
#define ma_spa_fraction                     spa_fraction

#define MA_SPA_DIRECTION_OUTPUT             SPA_DIRECTION_OUTPUT
#define MA_SPA_DIRECTION_INPUT              SPA_DIRECTION_INPUT

#define MA_SPA_TYPE_Id                      SPA_TYPE_Id
#define MA_SPA_TYPE_Int                     SPA_TYPE_Int
#define MA_SPA_TYPE_Array                   SPA_TYPE_Array
#define MA_SPA_TYPE_Object                  SPA_TYPE_Object
#define MA_SPA_TYPE_Choice                  SPA_TYPE_Choice

#define MA_SPA_TYPE_OBJECT_Format           SPA_TYPE_OBJECT_Format
#define MA_SPA_TYPE_OBJECT_ParamBuffers     SPA_TYPE_OBJECT_ParamBuffers

#define MA_SPA_PARAM_EnumFormat             SPA_PARAM_EnumFormat
#define MA_SPA_PARAM_Format                 SPA_PARAM_Format
#define MA_SPA_PARAM_Buffers                SPA_PARAM_Buffers

#define MA_SPA_PARAM_BUFFERS_buffers        SPA_PARAM_BUFFERS_buffers
#define MA_SPA_PARAM_BUFFERS_blocks         SPA_PARAM_BUFFERS_blocks
#define MA_SPA_PARAM_BUFFERS_size           SPA_PARAM_BUFFERS_size
#define MA_SPA_PARAM_BUFFERS_stride         SPA_PARAM_BUFFERS_stride

#define MA_SPA_FORMAT_mediaType             SPA_FORMAT_mediaType
#define MA_SPA_FORMAT_mediaSubtype          SPA_FORMAT_mediaSubtype

#define MA_SPA_MEDIA_TYPE_audio             SPA_MEDIA_TYPE_audio
#define MA_SPA_MEDIA_SUBTYPE_raw            SPA_MEDIA_SUBTYPE_raw

#define MA_SPA_FORMAT_AUDIO_format          SPA_FORMAT_AUDIO_format
#define MA_SPA_FORMAT_AUDIO_rate            SPA_FORMAT_AUDIO_rate
#define MA_SPA_FORMAT_AUDIO_channels        SPA_FORMAT_AUDIO_channels
#define MA_SPA_FORMAT_AUDIO_position        SPA_FORMAT_AUDIO_position

#define MA_SPA_AUDIO_FORMAT_UNKNOWN         SPA_AUDIO_FORMAT_UNKNOWN
#define MA_SPA_AUDIO_FORMAT_U8              SPA_AUDIO_FORMAT_U8
#define MA_SPA_AUDIO_FORMAT_S16             SPA_AUDIO_FORMAT_S16
#define MA_SPA_AUDIO_FORMAT_S24             SPA_AUDIO_FORMAT_S24
#define MA_SPA_AUDIO_FORMAT_S32             SPA_AUDIO_FORMAT_S32
#define MA_SPA_AUDIO_FORMAT_F32             SPA_AUDIO_FORMAT_F32

#define MA_SPA_AUDIO_CHANNEL_MONO           SPA_AUDIO_CHANNEL_MONO
#define MA_SPA_AUDIO_CHANNEL_FL             SPA_AUDIO_CHANNEL_FL
#define MA_SPA_AUDIO_CHANNEL_FR             SPA_AUDIO_CHANNEL_FR
#define MA_SPA_AUDIO_CHANNEL_FC             SPA_AUDIO_CHANNEL_FC
#define MA_SPA_AUDIO_CHANNEL_LFE            SPA_AUDIO_CHANNEL_LFE
#define MA_SPA_AUDIO_CHANNEL_SL             SPA_AUDIO_CHANNEL_SL
#define MA_SPA_AUDIO_CHANNEL_SR             SPA_AUDIO_CHANNEL_SR
#define MA_SPA_AUDIO_CHANNEL_FLC            SPA_AUDIO_CHANNEL_FLC
#define MA_SPA_AUDIO_CHANNEL_FRC            SPA_AUDIO_CHANNEL_FRC
#define MA_SPA_AUDIO_CHANNEL_RC             SPA_AUDIO_CHANNEL_RC
#define MA_SPA_AUDIO_CHANNEL_RL             SPA_AUDIO_CHANNEL_RL
#define MA_SPA_AUDIO_CHANNEL_RR             SPA_AUDIO_CHANNEL_RR
#define MA_SPA_AUDIO_CHANNEL_TC             SPA_AUDIO_CHANNEL_TC
#define MA_SPA_AUDIO_CHANNEL_TFL            SPA_AUDIO_CHANNEL_TFL
#define MA_SPA_AUDIO_CHANNEL_TFC            SPA_AUDIO_CHANNEL_TFC
#define MA_SPA_AUDIO_CHANNEL_TFR            SPA_AUDIO_CHANNEL_TFR
#define MA_SPA_AUDIO_CHANNEL_TRL            SPA_AUDIO_CHANNEL_TRL
#define MA_SPA_AUDIO_CHANNEL_TRC            SPA_AUDIO_CHANNEL_TRC
#define MA_SPA_AUDIO_CHANNEL_TRR            SPA_AUDIO_CHANNEL_TRR
#define MA_SPA_AUDIO_CHANNEL_RLC            SPA_AUDIO_CHANNEL_RLC
#define MA_SPA_AUDIO_CHANNEL_RRC            SPA_AUDIO_CHANNEL_RRC
#define MA_SPA_AUDIO_CHANNEL_FLW            SPA_AUDIO_CHANNEL_FLW
#define MA_SPA_AUDIO_CHANNEL_FRW            SPA_AUDIO_CHANNEL_FRW
#define MA_SPA_AUDIO_CHANNEL_LFE2           SPA_AUDIO_CHANNEL_LFE2
#define MA_SPA_AUDIO_CHANNEL_FLH            SPA_AUDIO_CHANNEL_FLH
#define MA_SPA_AUDIO_CHANNEL_FCH            SPA_AUDIO_CHANNEL_FCH
#define MA_SPA_AUDIO_CHANNEL_FRH            SPA_AUDIO_CHANNEL_FRH
#define MA_SPA_AUDIO_CHANNEL_TFLC           SPA_AUDIO_CHANNEL_TFLC
#define MA_SPA_AUDIO_CHANNEL_TFRC           SPA_AUDIO_CHANNEL_TFRC
#define MA_SPA_AUDIO_CHANNEL_TSL            SPA_AUDIO_CHANNEL_TSL
#define MA_SPA_AUDIO_CHANNEL_TSR            SPA_AUDIO_CHANNEL_TSR
#define MA_SPA_AUDIO_CHANNEL_LLFE           SPA_AUDIO_CHANNEL_LLFE
#define MA_SPA_AUDIO_CHANNEL_RLFE           SPA_AUDIO_CHANNEL_RLFE
#define MA_SPA_AUDIO_CHANNEL_BC             SPA_AUDIO_CHANNEL_BC
#define MA_SPA_AUDIO_CHANNEL_BLC            SPA_AUDIO_CHANNEL_BLC
#define MA_SPA_AUDIO_CHANNEL_BRC            SPA_AUDIO_CHANNEL_BRC
#define MA_SPA_AUDIO_CHANNEL_AUX0           SPA_AUDIO_CHANNEL_AUX0
#define MA_SPA_AUDIO_CHANNEL_AUX1           SPA_AUDIO_CHANNEL_AUX1
#define MA_SPA_AUDIO_CHANNEL_AUX2           SPA_AUDIO_CHANNEL_AUX2
#define MA_SPA_AUDIO_CHANNEL_AUX3           SPA_AUDIO_CHANNEL_AUX3
#define MA_SPA_AUDIO_CHANNEL_AUX4           SPA_AUDIO_CHANNEL_AUX4
#define MA_SPA_AUDIO_CHANNEL_AUX5           SPA_AUDIO_CHANNEL_AUX5
#define MA_SPA_AUDIO_CHANNEL_AUX6           SPA_AUDIO_CHANNEL_AUX6
#define MA_SPA_AUDIO_CHANNEL_AUX7           SPA_AUDIO_CHANNEL_AUX7
#define MA_SPA_AUDIO_CHANNEL_AUX8           SPA_AUDIO_CHANNEL_AUX8
#define MA_SPA_AUDIO_CHANNEL_AUX9           SPA_AUDIO_CHANNEL_AUX9
#define MA_SPA_AUDIO_CHANNEL_AUX10          SPA_AUDIO_CHANNEL_AUX10
#define MA_SPA_AUDIO_CHANNEL_AUX11          SPA_AUDIO_CHANNEL_AUX11
#define MA_SPA_AUDIO_CHANNEL_AUX12          SPA_AUDIO_CHANNEL_AUX12
#define MA_SPA_AUDIO_CHANNEL_AUX13          SPA_AUDIO_CHANNEL_AUX13
#define MA_SPA_AUDIO_CHANNEL_AUX14          SPA_AUDIO_CHANNEL_AUX14
#define MA_SPA_AUDIO_CHANNEL_AUX15          SPA_AUDIO_CHANNEL_AUX15
#define MA_SPA_AUDIO_CHANNEL_AUX16          SPA_AUDIO_CHANNEL_AUX16
#define MA_SPA_AUDIO_CHANNEL_AUX17          SPA_AUDIO_CHANNEL_AUX17
#define MA_SPA_AUDIO_CHANNEL_AUX18          SPA_AUDIO_CHANNEL_AUX18
#define MA_SPA_AUDIO_CHANNEL_AUX19          SPA_AUDIO_CHANNEL_AUX19
#define MA_SPA_AUDIO_CHANNEL_AUX20          SPA_AUDIO_CHANNEL_AUX20
#define MA_SPA_AUDIO_CHANNEL_AUX21          SPA_AUDIO_CHANNEL_AUX21
#define MA_SPA_AUDIO_CHANNEL_AUX22          SPA_AUDIO_CHANNEL_AUX22
#define MA_SPA_AUDIO_CHANNEL_AUX23          SPA_AUDIO_CHANNEL_AUX23
#define MA_SPA_AUDIO_CHANNEL_AUX24          SPA_AUDIO_CHANNEL_AUX24
#define MA_SPA_AUDIO_CHANNEL_AUX25          SPA_AUDIO_CHANNEL_AUX25
#define MA_SPA_AUDIO_CHANNEL_AUX26          SPA_AUDIO_CHANNEL_AUX26
#define MA_SPA_AUDIO_CHANNEL_AUX27          SPA_AUDIO_CHANNEL_AUX27
#define MA_SPA_AUDIO_CHANNEL_AUX28          SPA_AUDIO_CHANNEL_AUX28
#define MA_SPA_AUDIO_CHANNEL_AUX29          SPA_AUDIO_CHANNEL_AUX29
#define MA_SPA_AUDIO_CHANNEL_AUX30          SPA_AUDIO_CHANNEL_AUX30
#define MA_SPA_AUDIO_CHANNEL_AUX31          SPA_AUDIO_CHANNEL_AUX31
#define MA_SPA_AUDIO_CHANNEL_AUX32          SPA_AUDIO_CHANNEL_AUX32
#define MA_SPA_AUDIO_CHANNEL_AUX33          SPA_AUDIO_CHANNEL_AUX33
#define MA_SPA_AUDIO_CHANNEL_AUX34          SPA_AUDIO_CHANNEL_AUX34
#define MA_SPA_AUDIO_CHANNEL_AUX35          SPA_AUDIO_CHANNEL_AUX35
#define MA_SPA_AUDIO_CHANNEL_AUX36          SPA_AUDIO_CHANNEL_AUX36
#define MA_SPA_AUDIO_CHANNEL_AUX37          SPA_AUDIO_CHANNEL_AUX37
#define MA_SPA_AUDIO_CHANNEL_AUX38          SPA_AUDIO_CHANNEL_AUX38
#define MA_SPA_AUDIO_CHANNEL_AUX39          SPA_AUDIO_CHANNEL_AUX39
#define MA_SPA_AUDIO_CHANNEL_AUX40          SPA_AUDIO_CHANNEL_AUX40
#define MA_SPA_AUDIO_CHANNEL_AUX41          SPA_AUDIO_CHANNEL_AUX41
#define MA_SPA_AUDIO_CHANNEL_AUX42          SPA_AUDIO_CHANNEL_AUX42
#define MA_SPA_AUDIO_CHANNEL_AUX43          SPA_AUDIO_CHANNEL_AUX43
#define MA_SPA_AUDIO_CHANNEL_AUX44          SPA_AUDIO_CHANNEL_AUX44
#define MA_SPA_AUDIO_CHANNEL_AUX45          SPA_AUDIO_CHANNEL_AUX45
#define MA_SPA_AUDIO_CHANNEL_AUX46          SPA_AUDIO_CHANNEL_AUX46
#define MA_SPA_AUDIO_CHANNEL_AUX47          SPA_AUDIO_CHANNEL_AUX47
#define MA_SPA_AUDIO_CHANNEL_AUX48          SPA_AUDIO_CHANNEL_AUX48
#define MA_SPA_AUDIO_CHANNEL_AUX49          SPA_AUDIO_CHANNEL_AUX49
#define MA_SPA_AUDIO_CHANNEL_AUX50          SPA_AUDIO_CHANNEL_AUX50
#define MA_SPA_AUDIO_CHANNEL_AUX51          SPA_AUDIO_CHANNEL_AUX51
#define MA_SPA_AUDIO_CHANNEL_AUX52          SPA_AUDIO_CHANNEL_AUX52
#define MA_SPA_AUDIO_CHANNEL_AUX53          SPA_AUDIO_CHANNEL_AUX53
#define MA_SPA_AUDIO_CHANNEL_AUX54          SPA_AUDIO_CHANNEL_AUX54
#define MA_SPA_AUDIO_CHANNEL_AUX55          SPA_AUDIO_CHANNEL_AUX55
#define MA_SPA_AUDIO_CHANNEL_AUX56          SPA_AUDIO_CHANNEL_AUX56
#define MA_SPA_AUDIO_CHANNEL_AUX57          SPA_AUDIO_CHANNEL_AUX57
#define MA_SPA_AUDIO_CHANNEL_AUX58          SPA_AUDIO_CHANNEL_AUX58
#define MA_SPA_AUDIO_CHANNEL_AUX59          SPA_AUDIO_CHANNEL_AUX59
#define MA_SPA_AUDIO_CHANNEL_AUX60          SPA_AUDIO_CHANNEL_AUX60
#define MA_SPA_AUDIO_CHANNEL_AUX61          SPA_AUDIO_CHANNEL_AUX61
#define MA_SPA_AUDIO_CHANNEL_AUX62          SPA_AUDIO_CHANNEL_AUX62
#define MA_SPA_AUDIO_CHANNEL_AUX63          SPA_AUDIO_CHANNEL_AUX63


#define MA_PW_KEY_MEDIA_TYPE                PW_KEY_MEDIA_TYPE
#define MA_PW_KEY_MEDIA_CATEGORY            PW_KEY_MEDIA_CATEGORY
#define MA_PW_KEY_MEDIA_ROLE                PW_KEY_MEDIA_ROLE
#define MA_PW_KEY_MEDIA_CLASS               PW_KEY_MEDIA_CLASS
#define MA_PW_KEY_NODE_LATENCY              PW_KEY_NODE_LATENCY
#define MA_PW_KEY_TARGET_OBJECT             PW_KEY_TARGET_OBJECT
#define MA_PW_KEY_METADATA_NAME             PW_KEY_METADATA_NAME

#define MA_PW_TYPE_INTERFACE_Node           PW_TYPE_INTERFACE_Node
#define MA_PW_TYPE_INTERFACE_Metadata       PW_TYPE_INTERFACE_Metadata

#define MA_PW_ID_CORE                       PW_ID_CORE
#define MA_PW_ID_ANY                        PW_ID_ANY

#define ma_pw_thread_loop                   pw_thread_loop
#define ma_pw_loop                          pw_loop
#define ma_pw_context                       pw_context
#define ma_pw_core                          pw_core
#define ma_pw_registry                      pw_registry
#define ma_pw_metadata                      pw_metadata
#define ma_pw_metadata_events               pw_metadata_events
#define ma_pw_proxy                         pw_proxy
#define ma_pw_properties                    pw_properties
#define ma_pw_stream                        pw_stream
#define ma_pw_stream_control                pw_stream_control
#define ma_pw_core_info                     pw_core_info

#define ma_pw_stream_state                  pw_stream_state
#define MA_PW_STREAM_STATE_ERROR            PW_STREAM_STATE_ERROR
#define MA_PW_STREAM_STATE_UNCONNECTED      PW_STREAM_STATE_UNCONNECTED
#define MA_PW_STREAM_STATE_CONNECTING       PW_STREAM_STATE_CONNECTING
#define MA_PW_STREAM_STATE_PAUSED           PW_STREAM_STATE_PAUSED
#define MA_PW_STREAM_STATE_STREAMING        PW_STREAM_STATE_STREAMING

#define ma_pw_stream_flags                  pw_stream_flags
#define MA_PW_STREAM_FLAG_NONE              PW_STREAM_FLAG_NONE
#define MA_PW_STREAM_FLAG_AUTOCONNECT       PW_STREAM_FLAG_AUTOCONNECT
#define MA_PW_STREAM_FLAG_INACTIVE          PW_STREAM_FLAG_INACTIVE
#define MA_PW_STREAM_FLAG_MAP_BUFFERS       PW_STREAM_FLAG_MAP_BUFFERS
#define MA_PW_STREAM_FLAG_DRIVER            PW_STREAM_FLAG_DRIVER
#define MA_PW_STREAM_FLAG_RT_PROCESS        PW_STREAM_FLAG_RT_PROCESS
#define MA_PW_STREAM_FLAG_NO_CONVERT        PW_STREAM_FLAG_NO_CONVERT
#define MA_PW_STREAM_FLAG_EXCLUSIVE         PW_STREAM_FLAG_EXCLUSIVE
#define MA_PW_STREAM_FLAG_DONT_RECONNECT    PW_STREAM_FLAG_DONT_RECONNECT
#define MA_PW_STREAM_FLAG_ALLOC_BUFFERS     PW_STREAM_FLAG_ALLOC_BUFFERS
#define MA_PW_STREAM_FLAG_TRIGGER           PW_STREAM_FLAG_TRIGGER
#define MA_PW_STREAM_FLAG_ASYNC             PW_STREAM_FLAG_ASYNC

#define ma_pw_buffer                        pw_buffer
#define ma_pw_time                          pw_time

#define MA_PW_VERSION_CORE_EVENTS           PW_VERSION_CORE_EVENTS
#define ma_pw_core_events                   pw_core_events

#define MA_PW_VERSION_REGISTRY              PW_VERSION_REGISTRY
#define MA_PW_VERSION_REGISTRY_EVENTS       PW_VERSION_REGISTRY_EVENTS
#define ma_pw_registry_events               pw_registry_events

#define MA_PW_VERSION_METADATA_METHODS      PW_VERSION_METADATA_METHODS
#define ma_pw_metadata_methods              pw_metadata_methods

#define MA_PW_VERSION_METADATA              PW_VERSION_METADATA
#define MA_PW_VERSION_METADATA_EVENTS       PW_VERSION_METADATA_EVENTS
#define ma_pw_metadata_events               pw_metadata_events

#define MA_PW_VERSION_STREAM_EVENTS         PW_VERSION_STREAM_EVENTS
#define ma_pw_stream_events                 pw_stream_events
#else
typedef void (* ma_spa_source_event_func_t)(void* data, ma_uint64 count);

/* SPA enums. */
enum ma_spa_direction
{
    MA_SPA_DIRECTION_INPUT  = 0,
    MA_SPA_DIRECTION_OUTPUT = 1
};

enum ma_spa_audio_format
{
    MA_SPA_AUDIO_FORMAT_UNKNOWN = 0,
    MA_SPA_AUDIO_FORMAT_ENCODED,

    MA_SPA_AUDIO_FORMAT_START_Interleaved = 0x100,
    MA_SPA_AUDIO_FORMAT_S8,
    MA_SPA_AUDIO_FORMAT_U8,
    MA_SPA_AUDIO_FORMAT_S16_LE,
    MA_SPA_AUDIO_FORMAT_S16_BE,
    MA_SPA_AUDIO_FORMAT_U16_LE,
    MA_SPA_AUDIO_FORMAT_U16_BE,
    MA_SPA_AUDIO_FORMAT_S24_32_LE,
    MA_SPA_AUDIO_FORMAT_S24_32_BE,
    MA_SPA_AUDIO_FORMAT_U24_32_LE,
    MA_SPA_AUDIO_FORMAT_U24_32_BE,
    MA_SPA_AUDIO_FORMAT_S32_LE,
    MA_SPA_AUDIO_FORMAT_S32_BE,
    MA_SPA_AUDIO_FORMAT_U32_LE,
    MA_SPA_AUDIO_FORMAT_U32_BE,
    MA_SPA_AUDIO_FORMAT_S24_LE,
    MA_SPA_AUDIO_FORMAT_S24_BE,
    MA_SPA_AUDIO_FORMAT_U24_LE,
    MA_SPA_AUDIO_FORMAT_U24_BE,
    MA_SPA_AUDIO_FORMAT_S20_LE,
    MA_SPA_AUDIO_FORMAT_S20_BE,
    MA_SPA_AUDIO_FORMAT_U20_LE,
    MA_SPA_AUDIO_FORMAT_U20_BE,
    MA_SPA_AUDIO_FORMAT_S18_LE,
    MA_SPA_AUDIO_FORMAT_S18_BE,
    MA_SPA_AUDIO_FORMAT_U18_LE,
    MA_SPA_AUDIO_FORMAT_U18_BE,
    MA_SPA_AUDIO_FORMAT_F32_LE,
    MA_SPA_AUDIO_FORMAT_F32_BE,
    MA_SPA_AUDIO_FORMAT_F64_LE,
    MA_SPA_AUDIO_FORMAT_F64_BE,
    MA_SPA_AUDIO_FORMAT_ULAW,
    MA_SPA_AUDIO_FORMAT_ALAW,

#if __BYTE_ORDER == __BIG_ENDIAN
    MA_SPA_AUDIO_FORMAT_S16       = MA_SPA_AUDIO_FORMAT_S16_BE,
    MA_SPA_AUDIO_FORMAT_U16       = MA_SPA_AUDIO_FORMAT_U16_BE,
    MA_SPA_AUDIO_FORMAT_S24_32    = MA_SPA_AUDIO_FORMAT_S24_32_BE,
    MA_SPA_AUDIO_FORMAT_U24_32    = MA_SPA_AUDIO_FORMAT_U24_32_BE,
    MA_SPA_AUDIO_FORMAT_S32       = MA_SPA_AUDIO_FORMAT_S32_BE,
    MA_SPA_AUDIO_FORMAT_U32       = MA_SPA_AUDIO_FORMAT_U32_BE,
    MA_SPA_AUDIO_FORMAT_S24       = MA_SPA_AUDIO_FORMAT_S24_BE,
    MA_SPA_AUDIO_FORMAT_U24       = MA_SPA_AUDIO_FORMAT_U24_BE,
    MA_SPA_AUDIO_FORMAT_S20       = MA_SPA_AUDIO_FORMAT_S20_BE,
    MA_SPA_AUDIO_FORMAT_U20       = MA_SPA_AUDIO_FORMAT_U20_BE,
    MA_SPA_AUDIO_FORMAT_S18       = MA_SPA_AUDIO_FORMAT_S18_BE,
    MA_SPA_AUDIO_FORMAT_U18       = MA_SPA_AUDIO_FORMAT_U18_BE,
    MA_SPA_AUDIO_FORMAT_F32       = MA_SPA_AUDIO_FORMAT_F32_BE,
    MA_SPA_AUDIO_FORMAT_F64       = MA_SPA_AUDIO_FORMAT_F64_BE,
    MA_SPA_AUDIO_FORMAT_S16_OE    = MA_SPA_AUDIO_FORMAT_S16_LE,
    MA_SPA_AUDIO_FORMAT_U16_OE    = MA_SPA_AUDIO_FORMAT_U16_LE,
    MA_SPA_AUDIO_FORMAT_S24_32_OE = MA_SPA_AUDIO_FORMAT_S24_32_LE,
    MA_SPA_AUDIO_FORMAT_U24_32_OE = MA_SPA_AUDIO_FORMAT_U24_32_LE,
    MA_SPA_AUDIO_FORMAT_S32_OE    = MA_SPA_AUDIO_FORMAT_S32_LE,
    MA_SPA_AUDIO_FORMAT_U32_OE    = MA_SPA_AUDIO_FORMAT_U32_LE,
    MA_SPA_AUDIO_FORMAT_S24_OE    = MA_SPA_AUDIO_FORMAT_S24_LE,
    MA_SPA_AUDIO_FORMAT_U24_OE    = MA_SPA_AUDIO_FORMAT_U24_LE,
    MA_SPA_AUDIO_FORMAT_S20_OE    = MA_SPA_AUDIO_FORMAT_S20_LE,
    MA_SPA_AUDIO_FORMAT_U20_OE    = MA_SPA_AUDIO_FORMAT_U20_LE,
    MA_SPA_AUDIO_FORMAT_S18_OE    = MA_SPA_AUDIO_FORMAT_S18_LE,
    MA_SPA_AUDIO_FORMAT_U18_OE    = MA_SPA_AUDIO_FORMAT_U18_LE,
    MA_SPA_AUDIO_FORMAT_F32_OE    = MA_SPA_AUDIO_FORMAT_F32_LE,
    MA_SPA_AUDIO_FORMAT_F64_OE    = MA_SPA_AUDIO_FORMAT_F64_LE
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    MA_SPA_AUDIO_FORMAT_S16       = MA_SPA_AUDIO_FORMAT_S16_LE,
    MA_SPA_AUDIO_FORMAT_U16       = MA_SPA_AUDIO_FORMAT_U16_LE,
    MA_SPA_AUDIO_FORMAT_S24_32    = MA_SPA_AUDIO_FORMAT_S24_32_LE,
    MA_SPA_AUDIO_FORMAT_U24_32    = MA_SPA_AUDIO_FORMAT_U24_32_LE,
    MA_SPA_AUDIO_FORMAT_S32       = MA_SPA_AUDIO_FORMAT_S32_LE,
    MA_SPA_AUDIO_FORMAT_U32       = MA_SPA_AUDIO_FORMAT_U32_LE,
    MA_SPA_AUDIO_FORMAT_S24       = MA_SPA_AUDIO_FORMAT_S24_LE,
    MA_SPA_AUDIO_FORMAT_U24       = MA_SPA_AUDIO_FORMAT_U24_LE,
    MA_SPA_AUDIO_FORMAT_S20       = MA_SPA_AUDIO_FORMAT_S20_LE,
    MA_SPA_AUDIO_FORMAT_U20       = MA_SPA_AUDIO_FORMAT_U20_LE,
    MA_SPA_AUDIO_FORMAT_S18       = MA_SPA_AUDIO_FORMAT_S18_LE,
    MA_SPA_AUDIO_FORMAT_U18       = MA_SPA_AUDIO_FORMAT_U18_LE,
    MA_SPA_AUDIO_FORMAT_F32       = MA_SPA_AUDIO_FORMAT_F32_LE,
    MA_SPA_AUDIO_FORMAT_F64       = MA_SPA_AUDIO_FORMAT_F64_LE,
    MA_SPA_AUDIO_FORMAT_S16_OE    = MA_SPA_AUDIO_FORMAT_S16_BE,
    MA_SPA_AUDIO_FORMAT_U16_OE    = MA_SPA_AUDIO_FORMAT_U16_BE,
    MA_SPA_AUDIO_FORMAT_S24_32_OE = MA_SPA_AUDIO_FORMAT_S24_32_BE,
    MA_SPA_AUDIO_FORMAT_U24_32_OE = MA_SPA_AUDIO_FORMAT_U24_32_BE,
    MA_SPA_AUDIO_FORMAT_S32_OE    = MA_SPA_AUDIO_FORMAT_S32_BE,
    MA_SPA_AUDIO_FORMAT_U32_OE    = MA_SPA_AUDIO_FORMAT_U32_BE,
    MA_SPA_AUDIO_FORMAT_S24_OE    = MA_SPA_AUDIO_FORMAT_S24_BE,
    MA_SPA_AUDIO_FORMAT_U24_OE    = MA_SPA_AUDIO_FORMAT_U24_BE,
    MA_SPA_AUDIO_FORMAT_S20_OE    = MA_SPA_AUDIO_FORMAT_S20_BE,
    MA_SPA_AUDIO_FORMAT_U20_OE    = MA_SPA_AUDIO_FORMAT_U20_BE,
    MA_SPA_AUDIO_FORMAT_S18_OE    = MA_SPA_AUDIO_FORMAT_S18_BE,
    MA_SPA_AUDIO_FORMAT_U18_OE    = MA_SPA_AUDIO_FORMAT_U18_BE,
    MA_SPA_AUDIO_FORMAT_F32_OE    = MA_SPA_AUDIO_FORMAT_F32_BE,
    MA_SPA_AUDIO_FORMAT_F64_OE    = MA_SPA_AUDIO_FORMAT_F64_BE
#endif
};

enum ma_spa_audio_channel
{
    MA_SPA_AUDIO_CHANNEL_UNKNOWN,
    MA_SPA_AUDIO_CHANNEL_NA,

    MA_SPA_AUDIO_CHANNEL_MONO,
    MA_SPA_AUDIO_CHANNEL_FL,
    MA_SPA_AUDIO_CHANNEL_FR,
    MA_SPA_AUDIO_CHANNEL_FC,
    MA_SPA_AUDIO_CHANNEL_LFE,
    MA_SPA_AUDIO_CHANNEL_SL,
    MA_SPA_AUDIO_CHANNEL_SR,
    MA_SPA_AUDIO_CHANNEL_FLC,
    MA_SPA_AUDIO_CHANNEL_FRC,
    MA_SPA_AUDIO_CHANNEL_RC,
    MA_SPA_AUDIO_CHANNEL_RL,
    MA_SPA_AUDIO_CHANNEL_RR,
    MA_SPA_AUDIO_CHANNEL_TC,
    MA_SPA_AUDIO_CHANNEL_TFL,
    MA_SPA_AUDIO_CHANNEL_TFC,
    MA_SPA_AUDIO_CHANNEL_TFR,
    MA_SPA_AUDIO_CHANNEL_TRL,
    MA_SPA_AUDIO_CHANNEL_TRC,
    MA_SPA_AUDIO_CHANNEL_TRR,
    MA_SPA_AUDIO_CHANNEL_RLC,
    MA_SPA_AUDIO_CHANNEL_RRC,
    MA_SPA_AUDIO_CHANNEL_FLW,
    MA_SPA_AUDIO_CHANNEL_FRW,
    MA_SPA_AUDIO_CHANNEL_LFE2,
    MA_SPA_AUDIO_CHANNEL_FLH,
    MA_SPA_AUDIO_CHANNEL_FCH,
    MA_SPA_AUDIO_CHANNEL_FRH,
    MA_SPA_AUDIO_CHANNEL_TFLC,
    MA_SPA_AUDIO_CHANNEL_TFRC,
    MA_SPA_AUDIO_CHANNEL_TSL,
    MA_SPA_AUDIO_CHANNEL_TSR,
    MA_SPA_AUDIO_CHANNEL_LLFE,
    MA_SPA_AUDIO_CHANNEL_RLFE,
    MA_SPA_AUDIO_CHANNEL_BC,
    MA_SPA_AUDIO_CHANNEL_BLC,
    MA_SPA_AUDIO_CHANNEL_BRC,

    MA_SPA_AUDIO_CHANNEL_START_Aux = 0x1000,
    MA_SPA_AUDIO_CHANNEL_AUX0 = MA_SPA_AUDIO_CHANNEL_START_Aux,
    MA_SPA_AUDIO_CHANNEL_AUX1,
    MA_SPA_AUDIO_CHANNEL_AUX2,
    MA_SPA_AUDIO_CHANNEL_AUX3,
    MA_SPA_AUDIO_CHANNEL_AUX4,
    MA_SPA_AUDIO_CHANNEL_AUX5,
    MA_SPA_AUDIO_CHANNEL_AUX6,
    MA_SPA_AUDIO_CHANNEL_AUX7,
    MA_SPA_AUDIO_CHANNEL_AUX8,
    MA_SPA_AUDIO_CHANNEL_AUX9,
    MA_SPA_AUDIO_CHANNEL_AUX10,
    MA_SPA_AUDIO_CHANNEL_AUX11,
    MA_SPA_AUDIO_CHANNEL_AUX12,
    MA_SPA_AUDIO_CHANNEL_AUX13,
    MA_SPA_AUDIO_CHANNEL_AUX14,
    MA_SPA_AUDIO_CHANNEL_AUX15,
    MA_SPA_AUDIO_CHANNEL_AUX16,
    MA_SPA_AUDIO_CHANNEL_AUX17,
    MA_SPA_AUDIO_CHANNEL_AUX18,
    MA_SPA_AUDIO_CHANNEL_AUX19,
    MA_SPA_AUDIO_CHANNEL_AUX20,
    MA_SPA_AUDIO_CHANNEL_AUX21,
    MA_SPA_AUDIO_CHANNEL_AUX22,
    MA_SPA_AUDIO_CHANNEL_AUX23,
    MA_SPA_AUDIO_CHANNEL_AUX24,
    MA_SPA_AUDIO_CHANNEL_AUX25,
    MA_SPA_AUDIO_CHANNEL_AUX26,
    MA_SPA_AUDIO_CHANNEL_AUX27,
    MA_SPA_AUDIO_CHANNEL_AUX28,
    MA_SPA_AUDIO_CHANNEL_AUX29,
    MA_SPA_AUDIO_CHANNEL_AUX30,
    MA_SPA_AUDIO_CHANNEL_AUX31,
    MA_SPA_AUDIO_CHANNEL_AUX32,
    MA_SPA_AUDIO_CHANNEL_AUX33,
    MA_SPA_AUDIO_CHANNEL_AUX34,
    MA_SPA_AUDIO_CHANNEL_AUX35,
    MA_SPA_AUDIO_CHANNEL_AUX36,
    MA_SPA_AUDIO_CHANNEL_AUX37,
    MA_SPA_AUDIO_CHANNEL_AUX38,
    MA_SPA_AUDIO_CHANNEL_AUX39,
    MA_SPA_AUDIO_CHANNEL_AUX40,
    MA_SPA_AUDIO_CHANNEL_AUX41,
    MA_SPA_AUDIO_CHANNEL_AUX42,
    MA_SPA_AUDIO_CHANNEL_AUX43,
    MA_SPA_AUDIO_CHANNEL_AUX44,
    MA_SPA_AUDIO_CHANNEL_AUX45,
    MA_SPA_AUDIO_CHANNEL_AUX46,
    MA_SPA_AUDIO_CHANNEL_AUX47,
    MA_SPA_AUDIO_CHANNEL_AUX48,
    MA_SPA_AUDIO_CHANNEL_AUX49,
    MA_SPA_AUDIO_CHANNEL_AUX50,
    MA_SPA_AUDIO_CHANNEL_AUX51,
    MA_SPA_AUDIO_CHANNEL_AUX52,
    MA_SPA_AUDIO_CHANNEL_AUX53,
    MA_SPA_AUDIO_CHANNEL_AUX54,
    MA_SPA_AUDIO_CHANNEL_AUX55,
    MA_SPA_AUDIO_CHANNEL_AUX56,
    MA_SPA_AUDIO_CHANNEL_AUX57,
    MA_SPA_AUDIO_CHANNEL_AUX58,
    MA_SPA_AUDIO_CHANNEL_AUX59,
    MA_SPA_AUDIO_CHANNEL_AUX60,
    MA_SPA_AUDIO_CHANNEL_AUX61,
    MA_SPA_AUDIO_CHANNEL_AUX62,
    MA_SPA_AUDIO_CHANNEL_AUX63
};



/* The spa_interface thing is only used by one thing here, and only because the function we want is not exported by the SO. */
struct ma_spa_callbacks
{
    const void* funcs;
    void* data;
};

struct ma_spa_interface
{
    const char* type;
    ma_uint32 version;
    struct ma_spa_callbacks cb;
};


/*
spa_dict is just a list of key/value pairs as a string. We can easily write our own version of this to remove
the dependency on the SPA headers.

We'll keep the naming consistent here with the actual SPA library, with just the "ma_" prefix added.
*/
struct ma_spa_dict_item
{
    const char* key;
    const char* value;
};

static MA_INLINE struct ma_spa_dict_item MA_SPA_DICT_ITEM_INIT(const char* key, const char* value)
{
    struct ma_spa_dict_item item;

    item.key   = key;
    item.value = value;

    return item;
}


struct ma_spa_dict
{
    ma_uint32 flags;
    ma_uint32 n_items;
    const struct ma_spa_dict_item* items;
};

static MA_INLINE struct ma_spa_dict MA_SPA_DICT_INIT(const struct ma_spa_dict_item* items, ma_uint32 n_items)
{
    struct ma_spa_dict dict;

    dict.flags   = 0;
    dict.n_items = n_items;
    dict.items   = items;

    return dict;
}

static MA_INLINE const char* ma_spa_dict_lookup(const struct ma_spa_dict* dict, const char* key)
{
    ma_uint32 i;

    for (i = 0; i < dict->n_items; i += 1) {
        if (strcmp(dict->items[i].key, key) == 0) {
            return dict->items[i].value;
        }
    }

    return NULL;
}


/*
spa_buffer is just a few trivial structs.
*/
struct ma_spa_chunk
{
    ma_uint32 offset;
    ma_uint32 size;
    ma_int32 stride;
    ma_int32 flags;
};

struct ma_spa_data
{
    ma_uint32 type;
    ma_uint32 flags;
    ma_int64 fd;
    ma_uint32 mapoffset;
    ma_uint32 maxsize;
    void* data;
    struct ma_spa_chunk* chunk;
};

struct ma_spa_meta
{
    ma_uint32 type;
    ma_uint32 size;
    void* data;
};

struct ma_spa_buffer
{
    ma_uint32 n_metas;
    ma_uint32 n_datas;
    struct ma_spa_meta* metas;
    struct ma_spa_data* datas;
};


/* spa_hook */
struct ma_spa_list
{
    struct ma_spa_list* next;
    struct ma_spa_list* prev;
};

struct ma_spa_hook
{
    struct ma_spa_list link;
    struct ma_spa_callbacks cb;
    void (* removed)(struct ma_spa_hook* hook);
    void* priv;
};


/* spa_pod is the most complicated part of SPA that we use. We're only implementing specifically what we need. */
#define MA_SPA_TYPE_Id                      3
#define MA_SPA_TYPE_Int                     4
#define MA_SPA_TYPE_Array                   13
#define MA_SPA_TYPE_Object                  15
#define MA_SPA_TYPE_Choice                  19

#define MA_SPA_TYPE_OBJECT_Format           262147
#define MA_SPA_TYPE_OBJECT_ParamBuffers     262148

#define MA_SPA_PARAM_EnumFormat             3
#define MA_SPA_PARAM_Format                 4
#define MA_SPA_PARAM_Buffers                5

#define MA_SPA_PARAM_BUFFERS_buffers        1
#define MA_SPA_PARAM_BUFFERS_blocks         2
#define MA_SPA_PARAM_BUFFERS_size           3
#define MA_SPA_PARAM_BUFFERS_stride         4

#define MA_SPA_FORMAT_mediaType             1
#define MA_SPA_FORMAT_mediaSubtype          2

#define MA_SPA_MEDIA_TYPE_audio             1
#define MA_SPA_MEDIA_SUBTYPE_raw            1

#define MA_SPA_FORMAT_AUDIO_format          65537
#define MA_SPA_FORMAT_AUDIO_rate            65539
#define MA_SPA_FORMAT_AUDIO_channels        65540
#define MA_SPA_FORMAT_AUDIO_position        65541


struct ma_spa_pod
{
    ma_uint32 size;
    ma_uint32 type;
};


struct ma_spa_pod_id
{
    struct ma_spa_pod pod;
    ma_uint32 value;
    ma_int32 _padding;
};

struct ma_spa_pod_int
{
    struct ma_spa_pod pod;
    ma_int32 value;
    ma_int32 _padding;
};


enum ma_spa_choice_type
{
    MA_SPA_CHOICE_None = 0,
    MA_SPA_CHOICE_Range,
    MA_SPA_CHOICE_Step,
    MA_SPA_CHOICE_Enum,
    MA_SPA_CHOICE_Flags
};

struct ma_spa_pod_choice_body
{
    ma_uint32 type;
    ma_uint32 flags;
    struct ma_spa_pod child;
};

struct ma_spa_pod_choice
{
    struct ma_spa_pod pod;
    struct ma_spa_pod_choice_body body;
};


struct ma_spa_pod_array_body
{
    struct ma_spa_pod child;
};

struct ma_spa_pod_array
{
    struct ma_spa_pod pod;
    struct ma_spa_pod_array_body body;
};


struct ma_spa_pod_object_body
{
    ma_uint32 type;
    ma_uint32 id;
};

struct ma_spa_pod_object
{
    struct ma_spa_pod pod;
    struct ma_spa_pod_object_body body;
};

struct ma_spa_pod_prop
{
    ma_uint32 key;
    ma_uint32 flags;
    struct ma_spa_pod value;
};


/* Miscellaneous SPA references that we don't actively use but are required by structs or function parameters. */
typedef struct ma_spa_command ma_spa_command;

struct ma_spa_fraction
{
    ma_uint32 num;
    ma_uint32 denom;
};



#define MA_PW_KEY_MEDIA_TYPE            "media.type"
#define MA_PW_KEY_MEDIA_CATEGORY        "media.category"
#define MA_PW_KEY_MEDIA_ROLE            "media.role"
#define MA_PW_KEY_MEDIA_CLASS           "media.class"
#define MA_PW_KEY_NODE_LATENCY          "node.latency"
#define MA_PW_KEY_TARGET_OBJECT         "target.object"
#define MA_PW_KEY_METADATA_NAME         "metadata.name"

#define MA_PW_TYPE_INTERFACE_Node       "PipeWire:Interface:Node"
#define MA_PW_TYPE_INTERFACE_Metadata   "PipeWire:Interface:Metadata"

#define MA_PW_ID_CORE                   0
#define MA_PW_ID_ANY                    0xFFFFFFFF

struct ma_pw_thread_loop;
struct ma_pw_loop;
struct ma_pw_context;
struct ma_pw_core;
struct ma_pw_registry;
struct ma_pw_metadata;
struct ma_pw_metadata_events;
struct ma_pw_proxy;
struct ma_pw_properties;
struct ma_pw_stream;
struct ma_pw_stream_control;
struct ma_pw_core_info;

enum ma_pw_stream_state
{
    MA_PW_STREAM_STATE_ERROR       = -1,
    MA_PW_STREAM_STATE_UNCONNECTED =  0,
    MA_PW_STREAM_STATE_CONNECTING  =  1,
    MA_PW_STREAM_STATE_PAUSED      =  2,
    MA_PW_STREAM_STATE_STREAMING   =  3
};

enum ma_pw_stream_flags
{
    MA_PW_STREAM_FLAG_NONE           = 0,
    MA_PW_STREAM_FLAG_AUTOCONNECT    = (1 << 0),
    MA_PW_STREAM_FLAG_INACTIVE       = (1 << 1),
    MA_PW_STREAM_FLAG_MAP_BUFFERS    = (1 << 2),
    MA_PW_STREAM_FLAG_DRIVER         = (1 << 3),
    MA_PW_STREAM_FLAG_RT_PROCESS     = (1 << 4),
    MA_PW_STREAM_FLAG_NO_CONVERT     = (1 << 5),
    MA_PW_STREAM_FLAG_EXCLUSIVE      = (1 << 6),
    MA_PW_STREAM_FLAG_DONT_RECONNECT = (1 << 7),
    MA_PW_STREAM_FLAG_ALLOC_BUFFERS  = (1 << 8),
    MA_PW_STREAM_FLAG_TRIGGER        = (1 << 9),
    MA_PW_STREAM_FLAG_ASYNC          = (1 << 10)
};

struct ma_pw_buffer
{
    struct ma_spa_buffer* buffer;
    void* user_data;
    ma_uint64 size;
    ma_uint64 requested;
};

struct ma_pw_time
{
    ma_int64 now;
    struct ma_spa_fraction rate;
    ma_uint64 ticks;
    ma_int64 delay;
    ma_uint64 queued;
    ma_uint64 buffered;
    ma_uint32 queued_buffers;
    ma_uint32 avail_buffers;
    ma_uint64 size;
};


#define MA_PW_VERSION_CORE_EVENTS       1
struct ma_pw_core_events
{
    ma_uint32 version;
    void (* info       )(void* data, const struct ma_pw_core_info* info);
    void (* done       )(void* data, ma_uint32 id, int seq);
    void (* ping       )(void* data, ma_uint32 id, int seq);
    void (* error      )(void* data, ma_uint32 id, int seq, int res, const char* message);
    void (* remove_id  )(void* data, ma_uint32 id);
    void (* bound_id   )(void* data, ma_uint32 id, ma_uint32 global_id);
    void (* add_mem    )(void* data, ma_uint32 id, ma_uint32 type, int fd, ma_uint32 flags);
    void (* remove_mem )(void* data, ma_uint32 id);
    void (* bound_props)(void* data, ma_uint32 id, ma_uint32 global_id, const struct ma_spa_dict* props);
};


#define MA_PW_VERSION_REGISTRY          3
#define MA_PW_VERSION_REGISTRY_EVENTS   0
struct ma_pw_registry_events
{
    ma_uint32 version;
    void (* global_add   )(void* data, ma_uint32 id, ma_uint32 permissions, const char* type, ma_uint32 version, const struct ma_spa_dict* props);
    void (* global_remove)(void* data, ma_uint32 id);
};



#define PW_VERSION_METADATA_METHODS 0
struct ma_pw_metadata_methods
{
    ma_uint32 version;
    int (* add_listener)(void* object, struct ma_spa_hook* listener, const struct ma_pw_metadata_events* events, void* data);
    int (* set_property)(void* object, ma_uint32 subject, const char* key, const char* type, const char* value);
    int (* clear       )(void* object);
};

#define MA_PW_VERSION_METADATA          3
#define MA_PW_VERSION_METADATA_EVENTS   0
struct ma_pw_metadata_events
{
    ma_uint32 version;
    int (* property)(void* data, ma_uint32 subject, const char* key, const char* type, const char* value);
};


#define MA_PW_VERSION_STREAM_EVENTS     2
struct ma_pw_stream_events
{
    ma_uint32 version;
    void (* destroy      )(void* data);
    void (* state_changed)(void* data, enum ma_pw_stream_state oldState, enum ma_pw_stream_state newState, const char* error);
    void (* control_info )(void* data, ma_uint32 id, const struct ma_pw_stream_control* control);
    void (* io_changed   )(void* data, ma_uint32 id, void* area, ma_uint32 size);
    void (* param_changed)(void* data, ma_uint32 id, const struct ma_spa_pod* param);
    void (* add_buffer   )(void* data, struct ma_pw_buffer* buffer);
    void (* remove_buffer)(void* data, struct ma_pw_buffer* buffer);
    void (* process      )(void* data);
    void (* drained      )(void* data);
    void (* command      )(void* data, const struct ma_spa_command* command);
    void (* trigger_done )(void* data);
};
#endif

static MA_INLINE struct ma_spa_pod_id ma_spa_pod_id_init(ma_uint32 value)
{
    struct ma_spa_pod_id pod;

    MA_PIPEWIRE_ZERO_OBJECT(&pod);
    pod.pod.size = 4; /* Does not include the header or padding. */
    pod.pod.type = MA_SPA_TYPE_Id;
    pod.value    = value;

    return pod;
}

static MA_INLINE struct ma_spa_pod_int ma_spa_pod_int_init(ma_int32 value)
{
    struct ma_spa_pod_int pod;

    MA_PIPEWIRE_ZERO_OBJECT(&pod);
    pod.pod.size = 4; /* Does not include the header or padding. */
    pod.pod.type = MA_SPA_TYPE_Int;
    pod.value    = value;

    return pod;
}

static MA_INLINE void* ma_spa_pod_choice_get_values(const struct ma_spa_pod_choice* choice)
{
    /* Values are stored as a flat array after the main struct. */
    return (void*)ma_offset_ptr(choice, sizeof(struct ma_spa_pod_choice));
}

static MA_INLINE ma_uint32 ma_spa_pod_array_get_length(const struct ma_spa_pod_array* array)
{
    return (array->pod.size - sizeof(struct ma_spa_pod_array_body)) / array->body.child.size;
}

static MA_INLINE void* ma_spa_pod_array_get_values(const struct ma_spa_pod_array* array)
{
    /* Values are stored as a flat array after the main struct. */
    return (void*)ma_offset_ptr(array, sizeof(struct ma_spa_pod_array));
}


struct ma_spa_pod_prop_id
{
    ma_uint32 key;
    ma_uint32 flags;
    struct ma_spa_pod_id value;
};

static MA_INLINE struct ma_spa_pod_prop_id ma_spa_pod_prop_id_init(ma_uint32 key, ma_uint32 value)
{
    struct ma_spa_pod_prop_id prop;

    prop.key   = key;
    prop.flags = 0;
    prop.value = ma_spa_pod_id_init(value);

    return prop;
}


struct ma_spa_pod_prop_int
{
    ma_uint32 key;
    ma_uint32 flags;
    struct ma_spa_pod_int value;
};

static MA_INLINE struct ma_spa_pod_prop_int ma_spa_pod_prop_int_init(ma_uint32 key, ma_int32 value)
{
    struct ma_spa_pod_prop_int prop;

    prop.key   = key;
    prop.flags = 0;
    prop.value = ma_spa_pod_int_init(value);

    return prop;
}


/* This is a custom pod for our buffer parameters. */
struct ma_spa_pod_buffer_params
{
    struct ma_spa_pod_object object;
    struct ma_spa_pod_prop_int buffers;
    struct ma_spa_pod_prop_int blocks;
    struct ma_spa_pod_prop_int stride;
    struct ma_spa_pod_prop_int size;
};

static MA_INLINE struct ma_spa_pod_buffer_params ma_spa_pod_buffer_params_init(ma_uint32 strideInBytes, ma_uint32 bufferSizeInFrames)
{
    struct ma_spa_pod_buffer_params pod;

    /*
    spa_pod_builder_add_object(&podBuilder,
        SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_Int(2),
        SPA_PARAM_BUFFERS_blocks,  SPA_POD_Int(1),
        SPA_PARAM_BUFFERS_stride,  SPA_POD_Int(bytesPerFrame),
        SPA_PARAM_BUFFERS_size,    SPA_POD_Int(bytesPerFrame * pStreamState->bufferSizeInFrames));
    */

    MA_PIPEWIRE_ZERO_OBJECT(&pod);

    pod.object.pod.size  = sizeof(struct ma_spa_pod_buffer_params) - sizeof(struct ma_spa_pod); /* Don't include the header in the size. */
    pod.object.pod.type  = MA_SPA_TYPE_Object;
    pod.object.body.type = MA_SPA_TYPE_OBJECT_ParamBuffers;
    pod.object.body.id   = MA_SPA_PARAM_Buffers;

    pod.buffers = ma_spa_pod_prop_int_init(MA_SPA_PARAM_BUFFERS_buffers, 2);
    pod.blocks  = ma_spa_pod_prop_int_init(MA_SPA_PARAM_BUFFERS_blocks,  1);
    pod.stride  = ma_spa_pod_prop_int_init(MA_SPA_PARAM_BUFFERS_stride,  (ma_int32)strideInBytes);
    pod.size    = ma_spa_pod_prop_int_init(MA_SPA_PARAM_BUFFERS_size,    (ma_int32)(strideInBytes * bufferSizeInFrames));

    return pod;
}


/* A custom pod for audio info. */
struct ma_spa_pod_audio_info_raw
{
    struct ma_spa_pod_object object;
    struct ma_spa_pod_prop_id mediaType;
    struct ma_spa_pod_prop_id mediaSubtype;
    struct ma_spa_pod_prop_id format;
    struct ma_spa_pod_prop_int extra[2];    /* Channels and sample rate. Both are optional. */
};

static MA_INLINE struct ma_spa_pod_audio_info_raw ma_spa_pod_audio_info_raw_init(enum ma_spa_audio_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    struct ma_spa_pod_audio_info_raw pod;
    int extraIndex = 0;

    /*
    struct spa_audio_info_raw audioInfo;
    char podBuilderBuffer[1024];
    struct spa_pod_builder podBuilder;

    podBuilder = SPA_POD_BUILDER_INIT(podBuilderBuffer, sizeof(podBuilderBuffer));

    memset(&audioInfo, 0, sizeof(audioInfo));
    audioInfo.flags    = SPA_AUDIO_FLAG_UNPOSITIONED;
    audioInfo.format   = (enum spa_audio_format)formatPA;
    audioInfo.channels = pDescriptor->channels;
    audioInfo.rate     = pDescriptor->sampleRate;

    pConnectionParameters[0] = spa_format_audio_raw_build(&podBuilder, SPA_PARAM_EnumFormat, &audioInfo);
    */

    MA_PIPEWIRE_ZERO_OBJECT(&pod);

    pod.object.pod.size  = sizeof(struct ma_spa_pod_object) - sizeof(struct ma_spa_pod); /* Don't include the header in the size. */
    pod.object.pod.type  = MA_SPA_TYPE_Object;
    pod.object.body.type = MA_SPA_TYPE_OBJECT_Format;
    pod.object.body.id   = MA_SPA_PARAM_EnumFormat;

    pod.mediaType = ma_spa_pod_prop_id_init(MA_SPA_FORMAT_mediaType, MA_SPA_MEDIA_TYPE_audio);
    pod.object.pod.size += sizeof(struct ma_spa_pod_prop_id);

    pod.mediaSubtype = ma_spa_pod_prop_id_init(MA_SPA_FORMAT_mediaSubtype, MA_SPA_MEDIA_SUBTYPE_raw);
    pod.object.pod.size += sizeof(struct ma_spa_pod_prop_id);

    pod.format = ma_spa_pod_prop_id_init(MA_SPA_FORMAT_AUDIO_format, (ma_uint32)format);
    pod.object.pod.size += sizeof(struct ma_spa_pod_prop_id);

    if (channels > 0) {
        pod.extra[extraIndex] = ma_spa_pod_prop_int_init(MA_SPA_FORMAT_AUDIO_channels, (ma_int32)channels);
        pod.object.pod.size += sizeof(struct ma_spa_pod_prop_int);
        extraIndex += 1;
    }

    if (sampleRate > 0) {
        pod.extra[extraIndex] = ma_spa_pod_prop_int_init(MA_SPA_FORMAT_AUDIO_rate, (ma_int32)sampleRate);
        pod.object.pod.size += sizeof(struct ma_spa_pod_prop_int);
        extraIndex += 1;
    }

    /* We're going to leave the channel map alone and just do a conversion ourselves if it differs from the native map. */

    return pod;
}


/* Extracts the value of an ID from a pod, depending on its type. */
static MA_INLINE ma_uint32 ma_spa_pod_get_id_value(const struct ma_spa_pod* pod)
{
    MA_PIPEWIRE_ASSERT(pod != NULL);

    switch (pod->type)
    {
        case MA_SPA_TYPE_Id:
        {
            const struct ma_spa_pod_id* pPodID = (const struct ma_spa_pod_id*)pod;
            return pPodID->value;
        }

        case MA_SPA_TYPE_Choice:
        {
            const struct ma_spa_pod_choice* pPodChoice = (const struct ma_spa_pod_choice*)pod;
            const void* pValues = ma_spa_pod_choice_get_values(pPodChoice);

            /* We're just going to return the first value. */
            return ((ma_uint32*)pValues)[0];
        }

        default:
        {
            MA_PIPEWIRE_ASSERT(MA_FALSE); /* Unsupported type. */
            return 0;
        }
    }
}

static MA_INLINE ma_int32 ma_spa_pod_get_int_value(const struct ma_spa_pod* pod)
{
    MA_PIPEWIRE_ASSERT(pod != NULL);

    switch (pod->type)
    {
        case MA_SPA_TYPE_Id:
        {
            const struct ma_spa_pod_int* pPodInt = (const struct ma_spa_pod_int*)pod;
            return pPodInt->value;
        }

        case MA_SPA_TYPE_Choice:
        {
            const struct ma_spa_pod_choice* pPodChoice = (const struct ma_spa_pod_choice*)pod;
            const void* pValues = ma_spa_pod_choice_get_values(pPodChoice);

            /* We're just going to return the first value. */
            return ((ma_int32*)pValues)[0];
        }

        default:
        {
            MA_PIPEWIRE_ASSERT(MA_FALSE); /* Unsupported type. */
            return 0;
        }
    }
}



typedef void                      (* ma_pw_init_proc                    )(int* argc, char*** argv);
typedef void                      (* ma_pw_deinit_proc                  )(void);
typedef struct ma_pw_loop*        (* ma_pw_loop_new_proc                )(const struct ma_spa_dict* props);
typedef void                      (* ma_pw_loop_destroy_proc            )(struct ma_pw_loop* loop);
typedef int                       (* ma_pw_loop_set_name_proc           )(struct ma_pw_loop* loop, const char* name);
typedef void                      (* ma_pw_loop_enter_proc              )(struct ma_pw_loop* loop);
typedef void                      (* ma_pw_loop_leave_proc              )(struct ma_pw_loop* loop);
typedef int                       (* ma_pw_loop_iterate_proc            )(struct ma_pw_loop* loop, int timeout);
typedef struct spa_source*        (* ma_pw_loop_add_event_proc          )(struct ma_pw_loop* loop, ma_spa_source_event_func_t func, void* data);
typedef int                       (* ma_pw_loop_signal_event_proc       )(struct ma_pw_loop* loop, struct spa_source* source);
typedef struct ma_pw_thread_loop* (* ma_pw_thread_loop_new_proc         )(const char* name, const struct ma_spa_dict* props);
typedef void                      (* ma_pw_thread_loop_destroy_proc     )(struct ma_pw_thread_loop* loop);
typedef struct ma_pw_loop*        (* ma_pw_thread_loop_get_loop_proc    )(struct ma_pw_thread_loop* loop);
typedef int                       (* ma_pw_thread_loop_start_proc       )(struct ma_pw_thread_loop* loop);
typedef void                      (* ma_pw_thread_loop_lock_proc        )(struct ma_pw_thread_loop* loop);
typedef void                      (* ma_pw_thread_loop_unlock_proc      )(struct ma_pw_thread_loop* loop);
typedef struct ma_pw_context*     (* ma_pw_context_new_proc             )(struct ma_pw_loop* loop, struct ma_pw_properties *props, size_t user_data_size);
typedef void                      (* ma_pw_context_destroy_proc         )(struct ma_pw_context* context);
typedef struct ma_pw_core*        (* ma_pw_context_connect_proc         )(struct ma_pw_context* context, struct ma_pw_properties* properties, size_t user_data_size);
typedef int                       (* ma_pw_core_disconnect_proc         )(struct ma_pw_core* core);
typedef int                       (* ma_pw_core_add_listener_proc       )(struct ma_pw_core* core, struct ma_spa_hook* listener, const struct ma_pw_core_events* events, void* data);
typedef struct ma_pw_registry*    (* ma_pw_core_get_registry_proc       )(struct ma_pw_core* core, ma_uint32 version, size_t user_data_size);
typedef int                       (* ma_pw_core_sync_proc               )(struct ma_pw_core* core, ma_uint32 id, int seq);
typedef int                       (* ma_pw_registry_add_listener_proc   )(struct ma_pw_registry* registry, struct ma_spa_hook* listener, const struct ma_pw_registry_events* events, void* data);
typedef void*                     (* ma_pw_registry_bind_proc           )(struct ma_pw_registry* registry, ma_uint32 id, const char* type, ma_uint32 version, size_t user_data_size);
typedef void                      (* ma_pw_proxy_destroy_proc           )(struct ma_pw_proxy* proxy);
typedef struct ma_pw_properties*  (* ma_pw_properties_new_proc          )(const char* key, ...);
typedef void                      (* ma_pw_properties_free_proc         )(struct ma_pw_properties* properties);
typedef int                       (* ma_pw_properties_set_proc          )(struct ma_pw_properties* properties, const char* key, const char* value);
typedef struct ma_pw_stream*      (* ma_pw_stream_new_proc              )(struct ma_pw_core* core, const char* name, struct ma_pw_properties* props);
typedef void                      (* ma_pw_stream_destroy_proc          )(struct ma_pw_stream* stream);
typedef void                      (* ma_pw_stream_add_listener_proc     )(struct ma_pw_stream* stream, struct ma_spa_hook* listener, const struct ma_pw_stream_events* events, void* data);
typedef int                       (* ma_pw_stream_connect_proc          )(struct ma_pw_stream* stream, enum ma_spa_direction direction, ma_uint32 target_id, enum ma_pw_stream_flags flags, const struct ma_spa_pod** params, ma_uint32 paramCount);
typedef int                       (* ma_pw_stream_set_active_proc       )(struct ma_pw_stream* stream, ma_bool8 active);
typedef struct ma_pw_buffer*      (* ma_pw_stream_dequeue_buffer_proc   )(struct ma_pw_stream* stream);
typedef int                       (* ma_pw_stream_queue_buffer_proc     )(struct ma_pw_stream* stream, struct ma_pw_buffer* buffer);
typedef int                       (* ma_pw_stream_update_params_proc    )(struct ma_pw_stream* stream, const struct ma_spa_pod** params, ma_uint32 paramCount);
typedef int                       (* ma_pw_stream_update_properties_proc)(struct ma_pw_stream* stream, const struct ma_spa_dict* dict);
typedef int                       (* ma_pw_stream_get_time_n_proc       )(struct ma_pw_stream* stream, struct ma_pw_time* time, size_t size);

typedef struct
{
    ma_log* pLog;
    ma_handle hPipeWire;
    ma_pw_init_proc                     pw_init;
    ma_pw_deinit_proc                   pw_deinit;
    ma_pw_loop_new_proc                 pw_loop_new;
    ma_pw_loop_destroy_proc             pw_loop_destroy;
    ma_pw_loop_set_name_proc            pw_loop_set_name;
    ma_pw_loop_enter_proc               pw_loop_enter;
    ma_pw_loop_leave_proc               pw_loop_leave;
    ma_pw_loop_iterate_proc             pw_loop_iterate;
    ma_pw_loop_add_event_proc           pw_loop_add_event;
    ma_pw_loop_signal_event_proc        pw_loop_signal_event;
    ma_pw_thread_loop_new_proc          pw_thread_loop_new;
    ma_pw_thread_loop_destroy_proc      pw_thread_loop_destroy;
    ma_pw_thread_loop_get_loop_proc     pw_thread_loop_get_loop;
    ma_pw_thread_loop_start_proc        pw_thread_loop_start;
    ma_pw_thread_loop_lock_proc         pw_thread_loop_lock;
    ma_pw_thread_loop_unlock_proc       pw_thread_loop_unlock;
    ma_pw_context_new_proc              pw_context_new;
    ma_pw_context_destroy_proc          pw_context_destroy;
    ma_pw_context_connect_proc          pw_context_connect;
    ma_pw_core_disconnect_proc          pw_core_disconnect;
    ma_pw_core_add_listener_proc        pw_core_add_listener;
    ma_pw_core_get_registry_proc        pw_core_get_registry;
    ma_pw_core_sync_proc                pw_core_sync;
    ma_pw_registry_add_listener_proc    pw_registry_add_listener;
    ma_pw_registry_bind_proc            pw_registry_bind;
    ma_pw_proxy_destroy_proc            pw_proxy_destroy;
    ma_pw_properties_new_proc           pw_properties_new;
    ma_pw_properties_free_proc          pw_properties_free;
    ma_pw_properties_set_proc           pw_properties_set;
    ma_pw_stream_new_proc               pw_stream_new;
    ma_pw_stream_destroy_proc           pw_stream_destroy;
    ma_pw_stream_add_listener_proc      pw_stream_add_listener;
    ma_pw_stream_connect_proc           pw_stream_connect;
    ma_pw_stream_set_active_proc        pw_stream_set_active;
    ma_pw_stream_dequeue_buffer_proc    pw_stream_dequeue_buffer;
    ma_pw_stream_queue_buffer_proc      pw_stream_queue_buffer;
    ma_pw_stream_update_params_proc     pw_stream_update_params;
    ma_pw_stream_update_properties_proc pw_stream_update_properties;
    ma_pw_stream_get_time_n_proc        pw_stream_get_time_n;
} ma_context_state_pipewire;


#define MA_PIPEWIRE_INIT_STATUS_HAS_FORMAT  0x01
#define MA_PIPEWIRE_INIT_STATUS_HAS_LATENCY 0x02
#define MA_PIPEWIRE_INIT_STATUS_INITIALIZED 0x04

typedef struct
{
    struct ma_pw_stream* pStream;
    struct ma_spa_hook eventListener;
    ma_uint32 initStatus;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;
    ma_channel channelMap[MA_MAX_CHANNELS];
    ma_uint32 bufferSizeInFrames;
    ma_uint32 bufferCount;
    ma_uint32 rbSizeInFrames;
    ma_pcm_rb rb;                       /* Only used in single-threaded mode. Acts as an intermediary buffer for step(). For playback, PipeWire will read from this ring buffer. For capture, it'll write to it. */
    ma_device_descriptor* pDescriptor;  /* This is only used for setting up internal format. It's needed here because it looks like the only way to get the internal format is via a stupid callback. Will be set to NULL after initialization of the PipeWire stream. */
} ma_pipewire_stream_state;

typedef struct
{
    ma_context_state_pipewire* pContextStatePipeWire;
    ma_device_type deviceType;
    ma_device* pDevice; /* Only needed for ma_stream_event_process__pipewire(). We may change this later in which case we can delete this. */
    struct ma_pw_loop* pLoop;
    struct ma_pw_context* pContext;
    struct ma_pw_core* pCore;
    struct spa_source* pWakeup; /* This is for waking up the loop which we need to do after each data processing callback and the miniaudio wakeup callback. */
    ma_pipewire_stream_state playback;
    ma_pipewire_stream_state capture;
    struct
    {
        ma_timer timer;
        double lastTimeInSeconds;
    } debugging;
} ma_device_state_pipewire;


static enum ma_spa_audio_format ma_format_to_pipewire(ma_format format)
{
    switch (format)
    {
        case ma_format_u8:  return MA_SPA_AUDIO_FORMAT_U8;
        case ma_format_s16: return MA_SPA_AUDIO_FORMAT_S16;
        case ma_format_s24: return MA_SPA_AUDIO_FORMAT_S24;
        case ma_format_s32: return MA_SPA_AUDIO_FORMAT_S32;
        case ma_format_f32: return MA_SPA_AUDIO_FORMAT_F32;
        default: return MA_SPA_AUDIO_FORMAT_UNKNOWN;
    }
}

static ma_format ma_format_from_pipewire(enum ma_spa_audio_format format)
{
    switch (format)
    {
        case MA_SPA_AUDIO_FORMAT_U8:  return ma_format_u8;
        case MA_SPA_AUDIO_FORMAT_S16: return ma_format_s16;
        case MA_SPA_AUDIO_FORMAT_S24: return ma_format_s24;
        case MA_SPA_AUDIO_FORMAT_S32: return ma_format_s32;
        case MA_SPA_AUDIO_FORMAT_F32: return ma_format_f32;
        default: return ma_format_unknown;
    }
}

static ma_channel ma_channel_from_pipewire(ma_uint32 channel)
{
    switch (channel)
    {
        case MA_SPA_AUDIO_CHANNEL_MONO:  return MA_CHANNEL_MONO;
        case MA_SPA_AUDIO_CHANNEL_FL:    return MA_CHANNEL_FRONT_LEFT;
        case MA_SPA_AUDIO_CHANNEL_FR:    return MA_CHANNEL_FRONT_RIGHT;
        case MA_SPA_AUDIO_CHANNEL_FC:    return MA_CHANNEL_FRONT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_LFE:   return MA_CHANNEL_LFE;
        case MA_SPA_AUDIO_CHANNEL_SL:    return MA_CHANNEL_SIDE_LEFT;
        case MA_SPA_AUDIO_CHANNEL_SR:    return MA_CHANNEL_SIDE_RIGHT;
        case MA_SPA_AUDIO_CHANNEL_FLC:   return MA_CHANNEL_FRONT_LEFT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_FRC:   return MA_CHANNEL_FRONT_RIGHT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_RC:    return MA_CHANNEL_BACK_CENTER;
        case MA_SPA_AUDIO_CHANNEL_RL:    return MA_CHANNEL_BACK_LEFT;
        case MA_SPA_AUDIO_CHANNEL_RR:    return MA_CHANNEL_BACK_RIGHT;
        case MA_SPA_AUDIO_CHANNEL_TC:    return MA_CHANNEL_TOP_CENTER;
        case MA_SPA_AUDIO_CHANNEL_TFL:   return MA_CHANNEL_TOP_FRONT_LEFT;
        case MA_SPA_AUDIO_CHANNEL_TFC:   return MA_CHANNEL_TOP_FRONT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_TFR:   return MA_CHANNEL_TOP_FRONT_RIGHT;
        case MA_SPA_AUDIO_CHANNEL_TRL:   return MA_CHANNEL_TOP_BACK_LEFT;
        case MA_SPA_AUDIO_CHANNEL_TRC:   return MA_CHANNEL_TOP_BACK_CENTER;
        case MA_SPA_AUDIO_CHANNEL_TRR:   return MA_CHANNEL_TOP_BACK_RIGHT;

        /* These need to be added to miniaudio. */
        #if 0
        case MA_SPA_AUDIO_CHANNEL_RLC:   return MA_CHANNEL_BACK_LEFT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_RRC:   return MA_CHANNEL_BACK_RIGHT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_FLW:   return MA_CHANNEL_FRONT_LEFT_WIDE;
        case MA_SPA_AUDIO_CHANNEL_FRW:   return MA_CHANNEL_FRONT_RIGHT_WIDE;
        case MA_SPA_AUDIO_CHANNEL_LFE2:  return MA_CHANNEL_LFE2;
        case MA_SPA_AUDIO_CHANNEL_FLH:   return MA_CHANNEL_FRONT_LEFT_HIGH;
        case MA_SPA_AUDIO_CHANNEL_FCH:   return MA_CHANNEL_FRONT_CENTER_HIGH;
        case MA_SPA_AUDIO_CHANNEL_FRH:   return MA_CHANNEL_FRONT_RIGHT_HIGH;
        case MA_SPA_AUDIO_CHANNEL_TFLC:  return MA_CHANNEL_TOP_FRONT_LEFT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_TFRC:  return MA_CHANNEL_TOP_FRONT_RIGHT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_TSL:   return MA_CHANNEL_TOP_SIDE_LEFT;
        case MA_SPA_AUDIO_CHANNEL_TSR:   return MA_CHANNEL_TOP_SIDE_RIGHT;
        case MA_SPA_AUDIO_CHANNEL_LLFE:  return MA_CHANNEL_LEFT_LFE;
        case MA_SPA_AUDIO_CHANNEL_RLFE:  return MA_CHANNEL_RIGHT_LFE;
        case MA_SPA_AUDIO_CHANNEL_BC:    return MA_CHANNEL_BOTTOM_CENTER;
        case MA_SPA_AUDIO_CHANNEL_BLC:   return MA_CHANNEL_BOTTOM_LEFT_CENTER;
        case MA_SPA_AUDIO_CHANNEL_BRC:   return MA_CHANNEL_BOTTOM_RIGHT_CENTER;
        #endif

        case MA_SPA_AUDIO_CHANNEL_AUX0:  return MA_CHANNEL_AUX_0;
        case MA_SPA_AUDIO_CHANNEL_AUX1:  return MA_CHANNEL_AUX_1;
        case MA_SPA_AUDIO_CHANNEL_AUX2:  return MA_CHANNEL_AUX_2;
        case MA_SPA_AUDIO_CHANNEL_AUX3:  return MA_CHANNEL_AUX_3;
        case MA_SPA_AUDIO_CHANNEL_AUX4:  return MA_CHANNEL_AUX_4;
        case MA_SPA_AUDIO_CHANNEL_AUX5:  return MA_CHANNEL_AUX_5;
        case MA_SPA_AUDIO_CHANNEL_AUX6:  return MA_CHANNEL_AUX_6;
        case MA_SPA_AUDIO_CHANNEL_AUX7:  return MA_CHANNEL_AUX_7;
        case MA_SPA_AUDIO_CHANNEL_AUX8:  return MA_CHANNEL_AUX_8;
        case MA_SPA_AUDIO_CHANNEL_AUX9:  return MA_CHANNEL_AUX_9;
        case MA_SPA_AUDIO_CHANNEL_AUX10: return MA_CHANNEL_AUX_10;
        case MA_SPA_AUDIO_CHANNEL_AUX11: return MA_CHANNEL_AUX_11;
        case MA_SPA_AUDIO_CHANNEL_AUX12: return MA_CHANNEL_AUX_12;
        case MA_SPA_AUDIO_CHANNEL_AUX13: return MA_CHANNEL_AUX_13;
        case MA_SPA_AUDIO_CHANNEL_AUX14: return MA_CHANNEL_AUX_14;
        case MA_SPA_AUDIO_CHANNEL_AUX15: return MA_CHANNEL_AUX_15;
        case MA_SPA_AUDIO_CHANNEL_AUX16: return MA_CHANNEL_AUX_16;
        case MA_SPA_AUDIO_CHANNEL_AUX17: return MA_CHANNEL_AUX_17;
        case MA_SPA_AUDIO_CHANNEL_AUX18: return MA_CHANNEL_AUX_18;
        case MA_SPA_AUDIO_CHANNEL_AUX19: return MA_CHANNEL_AUX_19;
        case MA_SPA_AUDIO_CHANNEL_AUX20: return MA_CHANNEL_AUX_20;
        case MA_SPA_AUDIO_CHANNEL_AUX21: return MA_CHANNEL_AUX_21;
        case MA_SPA_AUDIO_CHANNEL_AUX22: return MA_CHANNEL_AUX_22;
        case MA_SPA_AUDIO_CHANNEL_AUX23: return MA_CHANNEL_AUX_23;
        case MA_SPA_AUDIO_CHANNEL_AUX24: return MA_CHANNEL_AUX_24;
        case MA_SPA_AUDIO_CHANNEL_AUX25: return MA_CHANNEL_AUX_25;
        case MA_SPA_AUDIO_CHANNEL_AUX26: return MA_CHANNEL_AUX_26;
        case MA_SPA_AUDIO_CHANNEL_AUX27: return MA_CHANNEL_AUX_27;
        case MA_SPA_AUDIO_CHANNEL_AUX28: return MA_CHANNEL_AUX_28;
        case MA_SPA_AUDIO_CHANNEL_AUX29: return MA_CHANNEL_AUX_29;
        case MA_SPA_AUDIO_CHANNEL_AUX30: return MA_CHANNEL_AUX_30;
        case MA_SPA_AUDIO_CHANNEL_AUX31: return MA_CHANNEL_AUX_31;

        /* These need to be added to miniaudio. */
        #if 0
        case MA_SPA_AUDIO_CHANNEL_AUX32: return MA_CHANNEL_AUX_32;
        case MA_SPA_AUDIO_CHANNEL_AUX33: return MA_CHANNEL_AUX_33;
        case MA_SPA_AUDIO_CHANNEL_AUX34: return MA_CHANNEL_AUX_34;
        case MA_SPA_AUDIO_CHANNEL_AUX35: return MA_CHANNEL_AUX_35;
        case MA_SPA_AUDIO_CHANNEL_AUX36: return MA_CHANNEL_AUX_36;
        case MA_SPA_AUDIO_CHANNEL_AUX37: return MA_CHANNEL_AUX_37;
        case MA_SPA_AUDIO_CHANNEL_AUX38: return MA_CHANNEL_AUX_38;
        case MA_SPA_AUDIO_CHANNEL_AUX39: return MA_CHANNEL_AUX_39;
        case MA_SPA_AUDIO_CHANNEL_AUX40: return MA_CHANNEL_AUX_40;
        case MA_SPA_AUDIO_CHANNEL_AUX41: return MA_CHANNEL_AUX_41;
        case MA_SPA_AUDIO_CHANNEL_AUX42: return MA_CHANNEL_AUX_42;
        case MA_SPA_AUDIO_CHANNEL_AUX43: return MA_CHANNEL_AUX_43;
        case MA_SPA_AUDIO_CHANNEL_AUX44: return MA_CHANNEL_AUX_44;
        case MA_SPA_AUDIO_CHANNEL_AUX45: return MA_CHANNEL_AUX_45;
        case MA_SPA_AUDIO_CHANNEL_AUX46: return MA_CHANNEL_AUX_46;
        case MA_SPA_AUDIO_CHANNEL_AUX47: return MA_CHANNEL_AUX_47;
        case MA_SPA_AUDIO_CHANNEL_AUX48: return MA_CHANNEL_AUX_48;
        case MA_SPA_AUDIO_CHANNEL_AUX49: return MA_CHANNEL_AUX_49;
        case MA_SPA_AUDIO_CHANNEL_AUX50: return MA_CHANNEL_AUX_50;
        case MA_SPA_AUDIO_CHANNEL_AUX51: return MA_CHANNEL_AUX_51;
        case MA_SPA_AUDIO_CHANNEL_AUX52: return MA_CHANNEL_AUX_52;
        case MA_SPA_AUDIO_CHANNEL_AUX53: return MA_CHANNEL_AUX_53;
        case MA_SPA_AUDIO_CHANNEL_AUX54: return MA_CHANNEL_AUX_54;
        case MA_SPA_AUDIO_CHANNEL_AUX55: return MA_CHANNEL_AUX_55;
        case MA_SPA_AUDIO_CHANNEL_AUX56: return MA_CHANNEL_AUX_56;
        case MA_SPA_AUDIO_CHANNEL_AUX57: return MA_CHANNEL_AUX_57;
        case MA_SPA_AUDIO_CHANNEL_AUX58: return MA_CHANNEL_AUX_58;
        case MA_SPA_AUDIO_CHANNEL_AUX59: return MA_CHANNEL_AUX_59;
        case MA_SPA_AUDIO_CHANNEL_AUX60: return MA_CHANNEL_AUX_60;
        case MA_SPA_AUDIO_CHANNEL_AUX61: return MA_CHANNEL_AUX_61;
        case MA_SPA_AUDIO_CHANNEL_AUX62: return MA_CHANNEL_AUX_62;
        case MA_SPA_AUDIO_CHANNEL_AUX63: return MA_CHANNEL_AUX_63;
        #endif

        default: return MA_CHANNEL_NONE;
    }
}


static ma_context_state_pipewire* ma_context_get_backend_state__pipewire(ma_context* pContext)
{
    return (ma_context_state_pipewire*)ma_context_get_backend_state(pContext);
}

static ma_device_state_pipewire* ma_device_get_backend_state__pipewire(ma_device* pDevice)
{
    return (ma_device_state_pipewire*)ma_device_get_backend_state(pDevice);
}


static ma_result ma_device_step__pipewire(ma_device* pDevice, ma_blocking_mode blockingMode);


static void ma_backend_info__pipewire(ma_device_backend_info* pBackendInfo)
{
    MA_PIPEWIRE_ASSERT(pBackendInfo != NULL);
    pBackendInfo->pName = "PipeWire";
}

static ma_result ma_context_init__pipewire(ma_context* pContext, const void* pContextBackendConfig, void** ppContextState)
{
    /* We'll use a list of possible shared object names for easier extensibility. */
    ma_context_state_pipewire* pContextStatePipeWire;
    ma_context_config_pipewire* pContextConfigPipeWire = (ma_context_config_pipewire*)pContextBackendConfig;
    ma_log* pLog = ma_context_get_log(pContext);

    (void)pContextConfigPipeWire;

    pContextStatePipeWire = (ma_context_state_pipewire*)ma_calloc(sizeof(*pContextStatePipeWire), ma_context_get_allocation_callbacks(pContext));
    if (pContextStatePipeWire == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    pContextStatePipeWire->pLog = pLog;


    /* Check if we have a PipeWire SO. If we can't find this we need to abort. */
    #ifndef MA_NO_RUNTIME_LINKING
    {
        ma_handle hPipeWire;
        size_t iName;
        const char* pSONames[] = {
            "libpipewire-0.3.so.0",
            "libpipewire.so"
        };

        for (iName = 0; iName < ma_countof(pSONames); iName += 1) {
            hPipeWire = ma_dlopen(pLog, pSONames[iName]);
            if (hPipeWire != NULL) {
                break;
            }
        }

        if (hPipeWire == NULL) {
            ma_free(pContextStatePipeWire, ma_context_get_allocation_callbacks(pContext));
            return MA_NO_BACKEND;   /* PipeWire could not be loaded. */
        }

        pContextStatePipeWire->hPipeWire = hPipeWire;
        pContextStatePipeWire->pw_init                     = (ma_pw_init_proc                    )ma_dlsym(pLog, hPipeWire, "pw_init");
        pContextStatePipeWire->pw_deinit                   = (ma_pw_deinit_proc                  )ma_dlsym(pLog, hPipeWire, "pw_deinit");
        pContextStatePipeWire->pw_loop_new                 = (ma_pw_loop_new_proc                )ma_dlsym(pLog, hPipeWire, "pw_loop_new");
        pContextStatePipeWire->pw_loop_destroy             = (ma_pw_loop_destroy_proc            )ma_dlsym(pLog, hPipeWire, "pw_loop_destroy");
        pContextStatePipeWire->pw_loop_set_name            = (ma_pw_loop_set_name_proc           )ma_dlsym(pLog, hPipeWire, "pw_loop_set_name");
        pContextStatePipeWire->pw_loop_enter               = (ma_pw_loop_enter_proc              )ma_dlsym(pLog, hPipeWire, "pw_loop_enter");
        pContextStatePipeWire->pw_loop_leave               = (ma_pw_loop_leave_proc              )ma_dlsym(pLog, hPipeWire, "pw_loop_leave");
        pContextStatePipeWire->pw_loop_iterate             = (ma_pw_loop_iterate_proc            )ma_dlsym(pLog, hPipeWire, "pw_loop_iterate");
        pContextStatePipeWire->pw_loop_add_event           = (ma_pw_loop_add_event_proc          )ma_dlsym(pLog, hPipeWire, "pw_loop_add_event");
        pContextStatePipeWire->pw_loop_signal_event        = (ma_pw_loop_signal_event_proc       )ma_dlsym(pLog, hPipeWire, "pw_loop_signal_event");
        pContextStatePipeWire->pw_thread_loop_new          = (ma_pw_thread_loop_new_proc         )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_new");
        pContextStatePipeWire->pw_thread_loop_destroy      = (ma_pw_thread_loop_destroy_proc     )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_destroy");
        pContextStatePipeWire->pw_thread_loop_get_loop     = (ma_pw_thread_loop_get_loop_proc    )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_get_loop");
        pContextStatePipeWire->pw_thread_loop_start        = (ma_pw_thread_loop_start_proc       )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_start");
        pContextStatePipeWire->pw_thread_loop_lock         = (ma_pw_thread_loop_lock_proc        )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_lock");
        pContextStatePipeWire->pw_thread_loop_unlock       = (ma_pw_thread_loop_unlock_proc      )ma_dlsym(pLog, hPipeWire, "pw_thread_loop_unlock");
        pContextStatePipeWire->pw_context_new              = (ma_pw_context_new_proc             )ma_dlsym(pLog, hPipeWire, "pw_context_new");
        pContextStatePipeWire->pw_context_destroy          = (ma_pw_context_destroy_proc         )ma_dlsym(pLog, hPipeWire, "pw_context_destroy");
        pContextStatePipeWire->pw_context_connect          = (ma_pw_context_connect_proc         )ma_dlsym(pLog, hPipeWire, "pw_context_connect");
        pContextStatePipeWire->pw_core_disconnect          = (ma_pw_core_disconnect_proc         )ma_dlsym(pLog, hPipeWire, "pw_core_disconnect");
        pContextStatePipeWire->pw_core_add_listener        = (ma_pw_core_add_listener_proc       )ma_dlsym(pLog, hPipeWire, "pw_core_add_listener");
        pContextStatePipeWire->pw_core_get_registry        = (ma_pw_core_get_registry_proc       )ma_dlsym(pLog, hPipeWire, "pw_core_get_registry");
        pContextStatePipeWire->pw_core_sync                = (ma_pw_core_sync_proc               )ma_dlsym(pLog, hPipeWire, "pw_core_sync");
        pContextStatePipeWire->pw_registry_add_listener    = (ma_pw_registry_add_listener_proc   )ma_dlsym(pLog, hPipeWire, "pw_registry_add_listener");
        pContextStatePipeWire->pw_registry_bind            = (ma_pw_registry_bind_proc           )ma_dlsym(pLog, hPipeWire, "pw_registry_bind");
        pContextStatePipeWire->pw_proxy_destroy            = (ma_pw_proxy_destroy_proc           )ma_dlsym(pLog, hPipeWire, "pw_proxy_destroy");
        pContextStatePipeWire->pw_properties_new           = (ma_pw_properties_new_proc          )ma_dlsym(pLog, hPipeWire, "pw_properties_new");
        pContextStatePipeWire->pw_properties_free          = (ma_pw_properties_free_proc         )ma_dlsym(pLog, hPipeWire, "pw_properties_free");
        pContextStatePipeWire->pw_properties_set           = (ma_pw_properties_set_proc          )ma_dlsym(pLog, hPipeWire, "pw_properties_set");
        pContextStatePipeWire->pw_stream_new               = (ma_pw_stream_new_proc              )ma_dlsym(pLog, hPipeWire, "pw_stream_new");
        pContextStatePipeWire->pw_stream_destroy           = (ma_pw_stream_destroy_proc          )ma_dlsym(pLog, hPipeWire, "pw_stream_destroy");
        pContextStatePipeWire->pw_stream_add_listener      = (ma_pw_stream_add_listener_proc     )ma_dlsym(pLog, hPipeWire, "pw_stream_add_listener");
        pContextStatePipeWire->pw_stream_connect           = (ma_pw_stream_connect_proc          )ma_dlsym(pLog, hPipeWire, "pw_stream_connect");
        pContextStatePipeWire->pw_stream_set_active        = (ma_pw_stream_set_active_proc       )ma_dlsym(pLog, hPipeWire, "pw_stream_set_active");
        pContextStatePipeWire->pw_stream_dequeue_buffer    = (ma_pw_stream_dequeue_buffer_proc   )ma_dlsym(pLog, hPipeWire, "pw_stream_dequeue_buffer");
        pContextStatePipeWire->pw_stream_queue_buffer      = (ma_pw_stream_queue_buffer_proc     )ma_dlsym(pLog, hPipeWire, "pw_stream_queue_buffer");
        pContextStatePipeWire->pw_stream_update_params     = (ma_pw_stream_update_params_proc    )ma_dlsym(pLog, hPipeWire, "pw_stream_update_params");
        pContextStatePipeWire->pw_stream_update_properties = (ma_pw_stream_update_properties_proc)ma_dlsym(pLog, hPipeWire, "pw_stream_update_properties");
        pContextStatePipeWire->pw_stream_get_time_n        = (ma_pw_stream_get_time_n_proc       )ma_dlsym(pLog, hPipeWire, "pw_stream_get_time_n");
    }
    #else
    {
        pContextStatePipeWire->pw_init                     = pw_init;
        pContextStatePipeWire->pw_deinit                   = pw_deinit;
        pContextStatePipeWire->pw_loop_new                 = pw_loop_new;
        pContextStatePipeWire->pw_loop_destroy             = pw_loop_destroy;
        pContextStatePipeWire->pw_loop_set_name            = pw_loop_set_name;
        pContextStatePipeWire->pw_loop_enter               = pw_loop_enter;
        pContextStatePipeWire->pw_loop_leave               = pw_loop_leave;
        pContextStatePipeWire->pw_loop_iterate             = pw_loop_iterate;
        pContextStatePipeWire->pw_loop_add_event           = pw_loop_add_event;
        pContextStatePipeWire->pw_loop_signal_event        = pw_loop_signal_event;       
        pContextStatePipeWire->pw_thread_loop_new          = pw_thread_loop_new;
        pContextStatePipeWire->pw_thread_loop_destroy      = pw_thread_loop_destroy;
        pContextStatePipeWire->pw_thread_loop_get_loop     = pw_thread_loop_get_loop;
        pContextStatePipeWire->pw_thread_loop_start        = pw_thread_loop_start;
        pContextStatePipeWire->pw_thread_loop_lock         = pw_thread_loop_lock;
        pContextStatePipeWire->pw_thread_loop_unlock       = pw_thread_loop_unlock;
        pContextStatePipeWire->pw_context_new              = pw_context_new;
        pContextStatePipeWire->pw_context_destroy          = pw_context_destroy;
        pContextStatePipeWire->pw_context_connect          = pw_context_connect;
        pContextStatePipeWire->pw_core_disconnect          = pw_core_disconnect;
        pContextStatePipeWire->pw_core_add_listener        = pw_core_add_listener;
        pContextStatePipeWire->pw_core_get_registry        = pw_core_get_registry;
        pContextStatePipeWire->pw_core_sync                = pw_core_sync;
        pContextStatePipeWire->pw_registry_add_listener    = pw_registry_add_listener;
        pContextStatePipeWire->pw_registry_bind            = pw_registry_bind;
        pContextStatePipeWire->pw_proxy_destroy            = pw_proxy_destroy;
        pContextStatePipeWire->pw_properties_new           = pw_properties_new;
        pContextStatePipeWire->pw_properties_free          = pw_properties_free;
        pContextStatePipeWire->pw_properties_set           = pw_properties_set;
        pContextStatePipeWire->pw_stream_new               = pw_stream_new;
        pContextStatePipeWire->pw_stream_destroy           = pw_stream_destroy;
        pContextStatePipeWire->pw_stream_add_listener      = pw_stream_add_listener;
        pContextStatePipeWire->pw_stream_connect           = pw_stream_connect;
        pContextStatePipeWire->pw_stream_set_active        = (ma_pw_stream_set_active_proc)pw_stream_set_active;    /* This cast is because we use `unsigned char` instead of `bool`. */
        pContextStatePipeWire->pw_stream_dequeue_buffer    = pw_stream_dequeue_buffer;
        pContextStatePipeWire->pw_stream_queue_buffer      = pw_stream_queue_buffer;
        pContextStatePipeWire->pw_stream_update_params     = pw_stream_update_params;
        pContextStatePipeWire->pw_stream_update_properties = pw_stream_update_properties;
        pContextStatePipeWire->pw_stream_get_time_n        = pw_stream_get_time_n;
    }
    #endif

    if (pContextStatePipeWire->pw_init != NULL) {
        pContextStatePipeWire->pw_init(NULL, NULL);
    }

    *ppContextState = pContextStatePipeWire;

    return MA_SUCCESS;
}

static void ma_context_uninit__pipewire(ma_context* pContext)
{
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(pContext);

    MA_PIPEWIRE_ASSERT(pContextStatePipeWire != NULL);

    if (pContextStatePipeWire->pw_deinit != NULL) {
        pContextStatePipeWire->pw_deinit();
    }

    /* Close the handle to the PipeWire shared object last. */
    ma_dlclose(ma_context_get_log(pContext), pContextStatePipeWire->hPipeWire);
    pContextStatePipeWire->hPipeWire = NULL;

    ma_free(pContextStatePipeWire, ma_context_get_allocation_callbacks(pContext));
}


static ma_device_enumeration_result ma_context_enumerate_default_device_by_type__pipewire(ma_context* pContext, ma_device_type deviceType, ma_enum_devices_callback_proc callback, void* pUserData)
{
    ma_device_info deviceInfo;

    (void)pContext;

    MA_PIPEWIRE_ZERO_OBJECT(&deviceInfo);

    /* Default. */
    deviceInfo.isDefault = MA_TRUE;

    /* ID. */
    deviceInfo.id.custom.i = 0;

    /* Name. */
    if (deviceType == ma_device_type_playback) {
        ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), "Default Playback Device", (size_t)-1);
    } else {
        ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), "Default Capture Device", (size_t)-1);
    }

    /* Data Format. */
    deviceInfo.nativeDataFormats[deviceInfo.nativeDataFormatCount].format     = ma_format_unknown;
    deviceInfo.nativeDataFormats[deviceInfo.nativeDataFormatCount].channels   = 0;
    deviceInfo.nativeDataFormats[deviceInfo.nativeDataFormatCount].sampleRate = 0;
    deviceInfo.nativeDataFormatCount += 1;

    return callback(deviceType, &deviceInfo, pUserData);
}



#define MA_PW_CORE_SYNC_FLAG_ENUM_DONE      (1 << 0)
#define MA_PW_CORE_SYNC_FLAG_DEFAULTS_DONE  (1 << 1)

typedef struct
{
    ma_context_state_pipewire* pContextStatePipeWire;
    struct ma_pw_loop* pLoop;
    struct ma_pw_core* pCore;
    struct ma_pw_registry* pRegistry;
    struct ma_pw_metadata* pMetadata;
    struct ma_spa_hook metadataListener;
    int seqDefaults;
    int seqEnumeration;
    ma_uint32 syncFlags;
    const ma_allocation_callbacks* pAllocationCallbacks;
    struct
    {
        ma_device_id defaultDeviceID;
        ma_device_info* pDeviceInfos;
        size_t deviceInfoCount;
        size_t deviceInfoCap;
    } playback, capture;
    ma_bool32 isAborted;    /* We can't seem to be able to abort enumeration with PipeWire, so we'll just set a flag to indicate it and then ignore anything that comes after. */
} ma_enumerate_devices_data_pipewire;


static void ma_on_core_done__pipewire(void* pUserData, ma_uint32 id, int seq)
{
    ma_enumerate_devices_data_pipewire* pEnumData = (ma_enumerate_devices_data_pipewire*)pUserData;

    (void)id;
    (void)seq;

    /*printf("Core Done: ID=%u, Seq=%d\n", id, seq);*/
    if (pEnumData->seqEnumeration == seq) {
        pEnumData->syncFlags |= MA_PW_CORE_SYNC_FLAG_ENUM_DONE;
    } else if (pEnumData->seqDefaults == seq) {
        pEnumData->syncFlags |= MA_PW_CORE_SYNC_FLAG_DEFAULTS_DONE;
    }
}

static const struct ma_pw_core_events ma_gCoreEventsPipeWire =
{
    MA_PW_VERSION_CORE_EVENTS,
    NULL,
    ma_on_core_done__pipewire,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};


static void ma_enumerate_devices_data_pipewire_init(ma_enumerate_devices_data_pipewire* pEnumData, ma_context_state_pipewire* pContextStatePipeWire, struct ma_pw_loop* pLoop, struct ma_pw_core* pCore, struct ma_pw_registry* pRegistry, const ma_allocation_callbacks* pAllocationCallbacks)
{
    MA_PIPEWIRE_ZERO_OBJECT(pEnumData);
    pEnumData->pContextStatePipeWire = pContextStatePipeWire;
    pEnumData->pLoop = pLoop;
    pEnumData->pCore = pCore;
    pEnumData->pRegistry = pRegistry;
    pEnumData->pAllocationCallbacks  = pAllocationCallbacks;
}

static void ma_enumerate_devices_data_pipewire_uninit(ma_enumerate_devices_data_pipewire* pEnumData)
{
    /* TODO: Delete pMetadata object. */
    ma_free(pEnumData->playback.pDeviceInfos, pEnumData->pAllocationCallbacks);
    ma_free(pEnumData->capture.pDeviceInfos, pEnumData->pAllocationCallbacks);
}

static ma_result ma_enumerate_devices_data_pipewire_add(ma_enumerate_devices_data_pipewire* pEnumData, ma_device_type deviceType, const ma_device_info* pDeviceInfo)
{
    ma_device_info* pNewDeviceInfos;
    size_t* pDeviceInfoCount;
    size_t* pDeviceInfoCap;
    ma_device_info** ppDeviceInfos;

    if (deviceType == ma_device_type_playback) {
        pDeviceInfoCount = &pEnumData->playback.deviceInfoCount;
        pDeviceInfoCap   = &pEnumData->playback.deviceInfoCap;
        ppDeviceInfos    = &pEnumData->playback.pDeviceInfos;
    } else {
        pDeviceInfoCount = &pEnumData->capture.deviceInfoCount;
        pDeviceInfoCap   = &pEnumData->capture.deviceInfoCap;
        ppDeviceInfos    = &pEnumData->capture.pDeviceInfos;
    }

    if (*pDeviceInfoCount + 1 > *pDeviceInfoCap) {
        size_t newCap = *pDeviceInfoCap * 2;
        
        if (newCap == 0) {
            newCap = 8;
        }

        pNewDeviceInfos = (ma_device_info*)ma_realloc(*ppDeviceInfos, newCap * sizeof(ma_device_info), pEnumData->pAllocationCallbacks);
        if (pNewDeviceInfos == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        *ppDeviceInfos  = pNewDeviceInfos;
        *pDeviceInfoCap = newCap;
    }

    MA_PIPEWIRE_COPY_MEMORY(&(*ppDeviceInfos)[*pDeviceInfoCount], pDeviceInfo, sizeof(ma_device_info));
    *pDeviceInfoCount += 1;

    return MA_SUCCESS;
}



static int ma_on_metadata_property_default__pipewire(void* data, ma_uint32 subject, const char* key, const char* type, const char* value)
{
    ma_enumerate_devices_data_pipewire* pEnumData = (ma_enumerate_devices_data_pipewire*)data;

    (void)subject;
    (void)type;

    /*
    Well this is fun. To get the default device we need to get the value of the "default.audio.sink" and "default.audio.source" keys. Sounds
    simple enough, except that the value is actually JSON... Why is the default device stored as a JSON string? Who does this? We're just
    going to use a simplified parser that finds the first ":\"" and takes everything until the next "\"".
    */
    if (strcmp(key, "default.audio.sink") == 0 || strcmp(key, "default.audio.source") == 0) {
        const char* pValue = value;
        const char* pStart;
        const char* pEnd;
        ma_device_id* pDefaultDeviceID;

        if (strcmp(key, "default.audio.sink") == 0) {
            pDefaultDeviceID = &pEnumData->playback.defaultDeviceID;
        } else {
            pDefaultDeviceID = &pEnumData->capture.defaultDeviceID;
        }

        pStart = strstr(pValue, ":\"");
        if (pStart != NULL) {
            pStart += 2;    /* Move past the :". */
            pEnd = strchr(pStart, '\"');
            if (pEnd != NULL) {
                size_t len = (size_t)(pEnd - pStart);
                if (len >= sizeof(pDefaultDeviceID->custom.s)) {
                    len  = sizeof(pDefaultDeviceID->custom.s) - 1;
                }

                ma_strncpy_s(pDefaultDeviceID->custom.s, sizeof(pDefaultDeviceID->custom.s), pStart, len);
            }
        }
    }

    /*printf("Metadata Property: Subject=%u, Key=%s, Type=%s, Value=%s\n", subject, key, type, value);*/
    return 0;
}

static struct ma_pw_metadata_events ma_gMetadataEventsPipeWire =
{
    MA_PW_VERSION_METADATA_EVENTS,
    ma_on_metadata_property_default__pipewire
};


static void ma_registry_event_global_add_enumeration_by_type__pipewire(ma_enumerate_devices_data_pipewire* pEnumData, ma_uint32 id, ma_uint32 permissions, const char* type, ma_uint32 version, const struct ma_spa_dict* props, ma_device_type deviceType)
{
    ma_device_info deviceInfo;
    const char* pNodeName;  /* <-- This is the ID. */
    const char* pNiceName;

    (void)permissions;
    (void)version;
    (void)id;
    (void)type;

    /* The node name is the ID. */
    pNodeName = ma_spa_dict_lookup(props, "node.name");

    /* Name. */
    pNiceName = ma_spa_dict_lookup(props, "node.description");
    if (pNiceName == NULL) { pNiceName = ma_spa_dict_lookup(props, "device.description"); }
    if (pNiceName == NULL) { pNiceName = ma_spa_dict_lookup(props, "device.nick"); }
    if (pNiceName == NULL) { pNiceName = pNodeName; }
    if (pNiceName == NULL) { pNiceName = "Unknown"; }

    /* Fill out the device info structure. */
    MA_PIPEWIRE_ZERO_OBJECT(&deviceInfo);

    /* The default flag is set later in a second pass. */

    /* ID. */
    ma_strncpy_s(deviceInfo.id.custom.s, sizeof(deviceInfo.id.custom.s), pNodeName, (size_t)-1);

    /* Name. */
    ma_strncpy_s(deviceInfo.name, sizeof(deviceInfo.name), pNiceName, (size_t)-1);

    /* Data Format. Just support everything for now. */
    /* TODO: See if there's a reasonable way to query the true "native" format. Maybe just initialize a stream and handle the SPA_PARAM_Format parameter in param_changed()? */
    deviceInfo.nativeDataFormats[deviceInfo.nativeDataFormatCount].format     = ma_format_unknown;
    deviceInfo.nativeDataFormats[deviceInfo.nativeDataFormatCount].channels   = 0;
    deviceInfo.nativeDataFormats[deviceInfo.nativeDataFormatCount].sampleRate = 0;
    deviceInfo.nativeDataFormatCount += 1;

    ma_enumerate_devices_data_pipewire_add(pEnumData, deviceType, &deviceInfo);

    /*printf("Registry Global Added By Type: ID=%u, Type=%s, DeviceType=%d, NiceName=%s\n", id, type, deviceType, pNiceName);*/
}

static void ma_registry_event_global_add_enumeration__pipewire(void* pUserData, ma_uint32 id, ma_uint32 permissions, const char* type, ma_uint32 version, const struct ma_spa_dict* props)
{
    ma_enumerate_devices_data_pipewire* pEnumData = (ma_enumerate_devices_data_pipewire*)pUserData;
    const char* pMediaClass;

    /* Ignore all future iterations if we have aborted. */
    if (pEnumData->isAborted) {
        return;
    }

    /* We need to check for our default devices. */
    if (strcmp(type, MA_PW_TYPE_INTERFACE_Metadata) == 0) {
        const char* pName;

        pName = ma_spa_dict_lookup(props, MA_PW_KEY_METADATA_NAME);
        if (pName != NULL && strcmp(pName, "default") == 0) {
            pEnumData->pMetadata = (struct ma_pw_metadata*)pEnumData->pContextStatePipeWire->pw_registry_bind(pEnumData->pRegistry, id, MA_PW_TYPE_INTERFACE_Metadata, MA_PW_VERSION_METADATA, 0);
            if (pEnumData->pMetadata != NULL) {
                MA_PIPEWIRE_ZERO_OBJECT(&pEnumData->metadataListener);

                /* Not using pw_metadata_add_listener() because it appears to be an inline function and thus not exported by libpipewire. */
                ((struct ma_pw_metadata_methods*)((struct ma_spa_interface*)pEnumData->pMetadata)->cb.funcs)->add_listener(pEnumData->pMetadata, &pEnumData->metadataListener, &ma_gMetadataEventsPipeWire, pEnumData);

                /*spa_api_method_r(int, -ENOTSUP, ma_pw_metadata, (struct spa_interface*)pEnumData->pMetadata, add_listener, 0, &pEnumData->metadataListener, &ma_gMetadataEventsPipeWire, pEnumData);*/
                /*pEnumData->pContextStatePipeWire->pw_metadata_add_listener(pMetadata, &pEnumData->metadataListener, &ma_gMetadataEventsPipeWire, NULL);*/

                pEnumData->seqDefaults = pEnumData->pContextStatePipeWire->pw_core_sync(pEnumData->pCore, MA_PW_ID_CORE, 0);
            }
        }

        return;
    }

    /* From here on out we only care about nodes. */
    if (strcmp(type, MA_PW_TYPE_INTERFACE_Node) != 0) {
        return;
    }

    pMediaClass = ma_spa_dict_lookup(props, MA_PW_KEY_MEDIA_CLASS);
    if (pMediaClass == NULL) {
        return;
    }

    /* If the string contains Audio/Sink or Audio/Source we can assume it's an enumerable node. */
    /*  */ if (strcmp(pMediaClass, "Audio/Sink") == 0) {
        ma_registry_event_global_add_enumeration_by_type__pipewire(pEnumData, id, permissions, type, version, props, ma_device_type_playback);
    } else if (strcmp(pMediaClass, "Audio/Source") == 0) {
        ma_registry_event_global_add_enumeration_by_type__pipewire(pEnumData, id, permissions, type, version, props, ma_device_type_capture);
    } else {
        return; /* Not an audio node we care about. */
    }
}

static const struct ma_pw_registry_events ma_gRegistryEventsPipeWire_Enumeration =
{
    MA_PW_VERSION_REGISTRY_EVENTS,
    ma_registry_event_global_add_enumeration__pipewire,
    NULL
};


static ma_result ma_context_enumerate_devices__pipewire(ma_context* pContext, ma_enum_devices_callback_proc callback, void* pUserData)
{
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(pContext);
    ma_device_enumeration_result cbResult = MA_DEVICE_ENUMERATION_CONTINUE;
    struct ma_pw_loop* pLoop;
    struct ma_pw_context* pPipeWireContext;
    struct ma_pw_core* pCore;
    struct ma_pw_registry* pRegistry;
    struct ma_spa_hook coreListener;
    struct ma_spa_hook registeryListener;
    ma_enumerate_devices_data_pipewire enumData;

    MA_PIPEWIRE_ASSERT(pContextStatePipeWire != NULL);
    MA_PIPEWIRE_ASSERT(callback != NULL);

    pLoop = pContextStatePipeWire->pw_loop_new(NULL);
    if (pLoop == NULL) {
        return MA_ERROR;
    }

    pPipeWireContext = pContextStatePipeWire->pw_context_new(pLoop, NULL, 0);
    if (pPipeWireContext == NULL) {
        pContextStatePipeWire->pw_loop_destroy(pLoop);
        return MA_ERROR;
    }

    pCore = pContextStatePipeWire->pw_context_connect(pPipeWireContext, NULL, 0);
    if (pCore == NULL) {
        pContextStatePipeWire->pw_context_destroy(pPipeWireContext);
        pContextStatePipeWire->pw_loop_destroy(pLoop);
        return MA_ERROR;
    }

    pContextStatePipeWire->pw_core_add_listener(pCore, &coreListener, &ma_gCoreEventsPipeWire, &enumData);

    pRegistry = pContextStatePipeWire->pw_core_get_registry(pCore, MA_PW_VERSION_REGISTRY, 0);
    if (pRegistry == NULL) {
        pContextStatePipeWire->pw_core_disconnect(pCore);
        pContextStatePipeWire->pw_context_destroy(pPipeWireContext);
        pContextStatePipeWire->pw_loop_destroy(pLoop);
        return MA_ERROR;
    }

    ma_enumerate_devices_data_pipewire_init(&enumData, pContextStatePipeWire, pLoop, pCore, pRegistry, ma_context_get_allocation_callbacks(pContext));

    MA_PIPEWIRE_ZERO_OBJECT(&registeryListener);
    pContextStatePipeWire->pw_registry_add_listener(pRegistry, &registeryListener, &ma_gRegistryEventsPipeWire_Enumeration, &enumData);

    /*
    The pw_core_sync() function is extremely confusing. The documentation says this:

        Ask the server to emit the 'done' event with seq.

    The last parameter of pw_core_sync() is `seq`, and in the `done` callback, there is a parameter called `seq`. The documentation
    makes it sound like the `seq` argument of the `done` callback will be set to what you specify in the `pw_core_sync()` call, but
    this is not the case. The `seq` argument in the `done` callback will actually be the return value of `pw_core_sync()`. If there
    is a PipeWire developer reading this, this documentation needs to be addressed. Feedback welcome if I'm misunderstanding or just
    doing something wrong here.
    */
    enumData.seqEnumeration = pContextStatePipeWire->pw_core_sync(pCore, MA_PW_ID_CORE, 0);
    for (;;) {
        pContextStatePipeWire->pw_loop_iterate(pLoop, -1);

        if (enumData.syncFlags & MA_PW_CORE_SYNC_FLAG_ENUM_DONE) {
            if (enumData.seqDefaults == 0) {
                break;  /* We don't have a "default" metadata. */
            }

            if (enumData.syncFlags & MA_PW_CORE_SYNC_FLAG_DEFAULTS_DONE) {
                break;
            }
        }
    }


    /* Here is where we iterate over each device and fire the callback. */
    {
        size_t iDevice;
        ma_bool32 hasDefaultPlaybackDevice = MA_FALSE;
        ma_bool32 hasDefaultCaptureDevice  = MA_FALSE;

        /* Playback devices. */
        for (iDevice = 0; iDevice < enumData.playback.deviceInfoCount; iDevice += 1) {
            if (cbResult == MA_DEVICE_ENUMERATION_CONTINUE) {
                if (enumData.playback.defaultDeviceID.custom.s[0] != '\0' && strcmp(enumData.playback.pDeviceInfos[iDevice].id.custom.s, enumData.playback.defaultDeviceID.custom.s) == 0) {
                    enumData.playback.pDeviceInfos[iDevice].isDefault = MA_TRUE;
                    hasDefaultPlaybackDevice = MA_TRUE;
                }

                cbResult = callback(ma_device_type_playback, &enumData.playback.pDeviceInfos[iDevice], pUserData);
            }
        }

        if (enumData.playback.deviceInfoCount > 0 && !hasDefaultPlaybackDevice) {
            if (cbResult == MA_DEVICE_ENUMERATION_CONTINUE) {
                cbResult = ma_context_enumerate_default_device_by_type__pipewire(pContext, ma_device_type_playback, callback, pUserData);
            }
        }


        /* Capture devices. */
        for (iDevice = 0; iDevice < enumData.capture.deviceInfoCount; iDevice += 1) {
            if (cbResult == MA_DEVICE_ENUMERATION_CONTINUE) {
                if (enumData.capture.defaultDeviceID.custom.s[0] != '\0' && strcmp(enumData.capture.pDeviceInfos[iDevice].id.custom.s, enumData.capture.defaultDeviceID.custom.s) == 0) {
                    enumData.capture.pDeviceInfos[iDevice].isDefault = MA_TRUE;
                    hasDefaultCaptureDevice = MA_TRUE;
                }

                cbResult = callback(ma_device_type_capture, &enumData.capture.pDeviceInfos[iDevice], pUserData);
            }
        }

        if (enumData.capture.deviceInfoCount > 0 && !hasDefaultCaptureDevice) {
            if (cbResult == MA_DEVICE_ENUMERATION_CONTINUE) {
                cbResult = ma_context_enumerate_default_device_by_type__pipewire(pContext, ma_device_type_capture, callback, pUserData);
            }
        }
    }


    ma_enumerate_devices_data_pipewire_uninit(&enumData);
    pContextStatePipeWire->pw_proxy_destroy((struct ma_pw_proxy*)pRegistry);
    pContextStatePipeWire->pw_core_disconnect(pCore);
    pContextStatePipeWire->pw_context_destroy(pPipeWireContext);
    pContextStatePipeWire->pw_loop_destroy(pLoop);

    return MA_SUCCESS;
}


static void ma_stream_event_param_changed__pipewire(void* pUserData, ma_uint32 id, const struct ma_spa_pod* pParam, ma_device_type deviceType)
{
    ma_device_state_pipewire*  pDeviceStatePipeWire  = (ma_device_state_pipewire*)pUserData;
    ma_context_state_pipewire* pContextStatePipeWire = pDeviceStatePipeWire->pContextStatePipeWire;

    if (id == MA_SPA_PARAM_Format) {
        ma_pipewire_stream_state* pStreamState;
        struct ma_spa_pod_buffer_params bufferParams;
        const struct ma_spa_pod* pBufferParameters[1];
        ma_uint32 bytesPerFrame;
        ma_uint32 iChannel;
        enum ma_spa_audio_format formatPA;
        ma_uint32 channels;
        ma_uint32 sampleRate;
        const ma_uint32* pChannelPositionsPA;

        /* It's possible for PipeWire to fire this callback with pParam set to NULL. I noticed it when tearing down a stream. Why does it do this? */
        if (pParam == NULL) {
            return;
        }

        if (deviceType == ma_device_type_playback) {
            pStreamState = &pDeviceStatePipeWire->playback;
        } else {
            pStreamState = &pDeviceStatePipeWire->capture;
        }

        if ((pStreamState->initStatus & MA_PIPEWIRE_INIT_STATUS_HAS_FORMAT) != 0) {
            ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_WARNING, "PipeWire format parameter changed after device has been initialized.");
            return;
        }

        /*
        We can now determine the format/channels/rate which will let us configure the size of the buffer and set the
        internal format of the device. To do this we need to parse the pParam pod.
        */
        #if 0
        {
            struct spa_audio_info_raw audioInfo;
            spa_format_audio_raw_parse(pParam, &audioInfo);
        }
        #endif
        {
            struct ma_spa_pod_object* pObject;
            struct ma_spa_pod_prop* pNextProp;
            ma_uint8* ptr = (ma_uint8*)pParam;
            ma_uint32 cursor = 0;
            ma_uint32 size;

            if (pParam->type != MA_SPA_TYPE_Object || pParam->size < sizeof(struct ma_spa_pod_object_body)) {
                ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "Failed to parse PipeWire format parameter (invalid pod type).");
                return;
            }

            pObject = (struct ma_spa_pod_object*)pParam;

            /* The size of the pod does not include the header which makes parsing kind of annoying for us. We'll add it here. */
            size = pParam->size + sizeof(struct ma_spa_pod);
            cursor += sizeof(struct ma_spa_pod);
            
            if (pObject->body.type != MA_SPA_TYPE_OBJECT_Format) {
                ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "Failed to parse PipeWire format parameter (invalid body type).");
                return;
            }

            cursor += sizeof(struct ma_spa_pod_object_body);

            /* At this point we have parsed the header part of the object, and what follows now should be a list of properties. */
            while (cursor < size) {
                struct ma_spa_pod* pPropValue;

                pNextProp = (struct ma_spa_pod_prop*)(ptr + cursor);
                if (cursor + sizeof(struct ma_spa_pod_prop) > size) {
                    ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "Failed to parse PipeWire format parameter (invalid size for property).");
                    return;
                }

                /* Place the cursor on the prop value, which is a spa_pod. */
                cursor += 8;    /* Size of the key and flags. */
                pPropValue = (struct ma_spa_pod*)(ptr + cursor);

                switch (pNextProp->key)
                {
                    case MA_SPA_FORMAT_AUDIO_format:
                    {
                        formatPA = (enum ma_spa_audio_format)ma_spa_pod_get_id_value(pPropValue);
                    } break;

                    case MA_SPA_FORMAT_AUDIO_channels:
                    {
                        channels = ma_spa_pod_get_int_value(pPropValue);
                    } break;

                    case MA_SPA_FORMAT_AUDIO_rate:
                    {
                        sampleRate = ma_spa_pod_get_int_value(pPropValue);
                    } break;

                    case MA_SPA_FORMAT_AUDIO_position:
                    {
                        ma_uint32 positionCount;

                        /* I'm assuming we can only get an array back for this. */
                        if (pPropValue->type != MA_SPA_TYPE_Array) {
                            ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "Failed to parse PipeWire format parameter (invalid type for position property).");
                            return;
                        }

                        positionCount = ma_spa_pod_array_get_length((const struct ma_spa_pod_array*)pPropValue);
                        if (positionCount != channels) {
                            ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "Failed to parse PipeWire format parameter (mismatched channel count for position property).");
                            return;
                        }

                        pChannelPositionsPA = (const ma_uint32*)ma_spa_pod_array_get_values((const struct ma_spa_pod_array*)pPropValue);
                    } break;
                }

                cursor += ma_align_64(8 + pPropValue->size);    /* Size of the value pod header + the size of the pod itself. */
            }
        }


        /* Now that we definitely know the sample rate, we can reliably configure the size of the buffer. */
        if (pStreamState->bufferSizeInFrames == 0) {
            pStreamState->bufferSizeInFrames = ma_calculate_buffer_size_in_frames_from_descriptor(pStreamState->pDescriptor, sampleRate);
        }

        pStreamState->format     = ma_format_from_pipewire(formatPA);
        pStreamState->channels   = channels;
        pStreamState->sampleRate = sampleRate;

        for (iChannel = 0; iChannel < MA_MAX_CHANNELS; iChannel += 1) {
            pStreamState->channelMap[iChannel] = ma_channel_from_pipewire(pChannelPositionsPA[iChannel]);
        }


        /* Now that we know both the buffer size and sample rate we can update the latency on the PipeWire side. */
        {
            struct ma_spa_dict_item items[1];
            struct ma_spa_dict dict;
            char bufferSizeInFramesStr[32];
            char sampleRateStr[32];
            char latencyStr[32];

            ma_pipewire_itoa_s(pStreamState->bufferSizeInFrames, bufferSizeInFramesStr, sizeof(bufferSizeInFramesStr), 10);
            ma_pipewire_itoa_s(pStreamState->sampleRate,         sampleRateStr,         sizeof(sampleRateStr),         10);

            latencyStr[0] = '\0';
            ma_pipewire_strcat_s(latencyStr, sizeof(latencyStr), bufferSizeInFramesStr);
            ma_pipewire_strcat_s(latencyStr, sizeof(latencyStr), "/");
            ma_pipewire_strcat_s(latencyStr, sizeof(latencyStr), sampleRateStr);

            items[0] = MA_SPA_DICT_ITEM_INIT(MA_PW_KEY_NODE_LATENCY, latencyStr);

            dict = MA_SPA_DICT_INIT(items, 1);

            pContextStatePipeWire->pw_stream_update_properties(pStreamState->pStream, &dict);
        }


        bytesPerFrame = ma_get_bytes_per_frame(pStreamState->format, pStreamState->channels);
        
        /*
        Now update the PipeWire buffer properties. Note that spa_pod_buffer_params_init() is not a standard SPA function. It's a
        custom function to eliminate the dependency on libspa for building miniaudio.
        */
        bufferParams = ma_spa_pod_buffer_params_init(bytesPerFrame, pStreamState->bufferSizeInFrames);
        pBufferParameters[0] = (struct ma_spa_pod*)&bufferParams;

        pContextStatePipeWire->pw_stream_update_params(pStreamState->pStream, pBufferParameters, sizeof(pBufferParameters) / sizeof(pBufferParameters[0]));

        pStreamState->initStatus |= MA_PIPEWIRE_INIT_STATUS_HAS_FORMAT;
    }
}

static void ma_stream_event_process__pipewire(void* pUserData, ma_device_type deviceType)
{
    ma_device_state_pipewire*  pDeviceStatePipeWire  = (ma_device_state_pipewire*)pUserData;
    ma_context_state_pipewire* pContextStatePipeWire = pDeviceStatePipeWire->pContextStatePipeWire;
    ma_result result;
    ma_pipewire_stream_state* pStreamState;
    struct ma_pw_buffer* pBuffer;
    ma_uint32 bytesPerFrame;
    ma_uint32 frameCount;

    if (deviceType == ma_device_type_playback) {
        pStreamState = &pDeviceStatePipeWire->playback;
    } else {
        pStreamState = &pDeviceStatePipeWire->capture;
    }

    /* Debugging stuff. */
    #if 0
    {
        double currentTimeInSeconds;
        double deltaTimeInSeconds;

        currentTimeInSeconds = ma_timer_get_time_in_seconds(&pDeviceStatePipeWire->debugging.timer);
        deltaTimeInSeconds = currentTimeInSeconds - pDeviceStatePipeWire->debugging.lastTimeInSeconds;
        pDeviceStatePipeWire->debugging.lastTimeInSeconds = currentTimeInSeconds;

        printf("TRACE: Process Callback %.2f ms\n", deltaTimeInSeconds * 1000.0);
    }
    #endif

    /*
    PipeWire has a bizarre buffer management system. Normally with an audio API you do processing after a certain amount of
    time has elapsed, based on the sample rate and buffer size. The frequency at which the processing callback is fired
    directly affects latency which is an important metric for audio applications and data management. From what I can tell,
    the only way to determine the rate at which this processing callback is fired is from within the callback itself. There
    are two ways I'm aware of:
    
        1) Dequeue the first buffer and check the `requested` member of `pw_buffer`.
        2) Get the stream time using `pw_stream_get_time_n()` and inspect the `size` member of `pw_time`.

    In capture, the first option cannot be used because `requested` is always zero. That leaves only the second option which
    appears to work for both playback and capture. However, the `size` member will only work for the first invocation of the
    processing callback because it can change as you enqueue buffers.
    
    Advice welcome on how to improve this.
    */
    if ((pStreamState->initStatus & MA_PIPEWIRE_INIT_STATUS_HAS_LATENCY) == 0) {
        struct ma_pw_time time;

        pContextStatePipeWire->pw_stream_get_time_n(pStreamState->pStream, &time, sizeof(time));

        if (pStreamState->rbSizeInFrames > 0) {
            if (ma_device_get_threading_mode(pDeviceStatePipeWire->pDevice) == MA_THREADING_MODE_SINGLE_THREADED) {
                ma_pcm_rb_uninit(&pStreamState->rb);
            }
        }

        pStreamState->rbSizeInFrames = (ma_uint32)time.size;

        if (ma_device_get_threading_mode(pDeviceStatePipeWire->pDevice) == MA_THREADING_MODE_SINGLE_THREADED) {
            ma_pcm_rb_init(pStreamState->format, pStreamState->channels, pStreamState->rbSizeInFrames, NULL, ma_device_get_allocation_callbacks(pDeviceStatePipeWire->pDevice), &pStreamState->rb);
        }

        pStreamState->initStatus |= MA_PIPEWIRE_INIT_STATUS_HAS_LATENCY;
        return;
    }


    bytesPerFrame = ma_get_bytes_per_frame(pStreamState->format, pStreamState->channels);

    pBuffer = pContextStatePipeWire->pw_stream_dequeue_buffer(pStreamState->pStream);
    if (pBuffer == NULL) {
        return;
    }

    if (deviceType == ma_device_type_playback) {
        frameCount = (ma_uint32)ma_min(pBuffer->requested, pBuffer->buffer->datas[0].maxsize / bytesPerFrame);
        /*frameCount = (ma_uint32)(pBuffer->buffer->datas[0].maxsize / bytesPerFrame);*/
    } else {
        frameCount = (ma_uint32)(pBuffer->buffer->datas[0].chunk->size / bytesPerFrame);
    }

    MA_PIPEWIRE_ASSERT(pBuffer->buffer != NULL);
    MA_PIPEWIRE_ASSERT(pBuffer->buffer->n_datas > 0);
    MA_PIPEWIRE_ASSERT(pBuffer->buffer->datas[0].data != NULL);

    if (frameCount > 0) {
        if (deviceType == ma_device_type_playback) {
            if (ma_device_get_threading_mode(pDeviceStatePipeWire->pDevice) == MA_THREADING_MODE_MULTI_THREADED) {
                ma_device_handle_backend_data_callback(pDeviceStatePipeWire->pDevice, pBuffer->buffer->datas[0].data, NULL, frameCount);
            } else {
                ma_uint32 framesRemaining = frameCount;
                ma_uint32 framesAvailable = ma_pcm_rb_available_read(&pStreamState->rb);

                /* Copy data in. Read from the ring buffer, output to the PipeWire buffer. */
                if (framesAvailable < frameCount) {
                    /* Underflow. Just write silence. */
                    MA_PIPEWIRE_ZERO_MEMORY((char*)pBuffer->buffer->datas[0].data + (frameCount - framesRemaining) * bytesPerFrame, framesRemaining * bytesPerFrame);
                } else {
                    /*
                    Two ways of handling this. In multi-threaded mode, it doesn't really matter which thread does the data
                    processing so we can just do it directly from here and bypass the ring buffer entirely.
                    */
                    
                    while (framesRemaining > 0) {
                        ma_uint32 framesToProcess = (ma_uint32)ma_min(framesRemaining, framesAvailable);
                        void* pMappedBuffer;

                        result = ma_pcm_rb_acquire_read(&pStreamState->rb, &framesToProcess, &pMappedBuffer);
                        if (result != MA_SUCCESS) {
                            ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "(PipeWire) Failed to acquire data from ring buffer.");
                            break;
                        }

                        MA_PIPEWIRE_COPY_MEMORY((char*)pBuffer->buffer->datas[0].data + ((frameCount - framesRemaining) * bytesPerFrame), pMappedBuffer, framesToProcess * bytesPerFrame);
                        framesRemaining -= framesToProcess;

                        result = ma_pcm_rb_commit_read(&pStreamState->rb, framesToProcess);
                        if (result != MA_SUCCESS) {
                            ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "(PipeWire) Failed to commit read to ring buffer.");
                            break;
                        }
                    }
                }
            }

            /*printf("(Playback) Processing %d frames... %d %d\n", (int)frameCount, (int)pBuffer->requested, pBuffer->buffer->datas[0].maxsize / bytesPerFrame);*/
        } else {
            if (ma_device_get_threading_mode(pDeviceStatePipeWire->pDevice) == MA_THREADING_MODE_MULTI_THREADED) {
                ma_device_handle_backend_data_callback(pDeviceStatePipeWire->pDevice, NULL, pBuffer->buffer->datas[0].data, frameCount);
            } else {
                ma_uint32 framesRemaining = frameCount;
                ma_uint32 framesAvailable = ma_pcm_rb_available_write(&pStreamState->rb);

                /* Copy data out. Write from the PipeWire buffer to the ring buffer. */
                while (framesRemaining > 0) {
                    ma_uint32 framesToProcess = (ma_uint32)ma_min(framesRemaining, framesAvailable);
                    void* pMappedBuffer;

                    result = ma_pcm_rb_acquire_write(&pStreamState->rb, &framesToProcess, &pMappedBuffer);
                    if (result != MA_SUCCESS) {
                        ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "(PipeWire) Failed to acquire space in ring buffer.");
                        break;
                    }

                    MA_PIPEWIRE_COPY_MEMORY(pMappedBuffer, (char*)pBuffer->buffer->datas[0].data + ((frameCount - framesRemaining) * bytesPerFrame), framesToProcess * bytesPerFrame);
                    framesRemaining -= framesToProcess;

                    result = ma_pcm_rb_commit_write(&pStreamState->rb, framesToProcess);
                    if (result != MA_SUCCESS) {
                        ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_ERROR, "(PipeWire) Failed to commit write to ring buffer.");
                        break;
                    }
                }
            }

            /*printf("(Capture) Processing %d frames...\n", (int)frameCount);*/
        }
    } else {
        /*ma_log_postf(pContextStatePipeWire->pLog, MA_LOG_LEVEL_WARNING, "(PipeWire) No frames to process.\n");*/
    }

    pBuffer->buffer->datas[0].chunk->offset = 0;
    pBuffer->buffer->datas[0].chunk->size   = frameCount * bytesPerFrame;

    pContextStatePipeWire->pw_stream_queue_buffer(pStreamState->pStream, pBuffer);

    /* We need to make sure the loop is woken up so we can refill the intermediary buffer in the step function. */
    pContextStatePipeWire->pw_loop_signal_event(pDeviceStatePipeWire->pLoop, pDeviceStatePipeWire->pWakeup);
}


static void ma_stream_event_param_changed_playback__pipewire(void* pUserData, ma_uint32 id, const struct ma_spa_pod* pParam)
{
    ma_stream_event_param_changed__pipewire(pUserData, id, pParam, ma_device_type_playback);
}

static void ma_stream_event_param_changed_capture__pipewire(void* pUserData, ma_uint32 id, const struct ma_spa_pod* pParam)
{
    ma_stream_event_param_changed__pipewire(pUserData, id, pParam, ma_device_type_capture);
}


static void ma_stream_event_process_playback__pipewire(void* pUserData)
{
    ma_stream_event_process__pipewire(pUserData, ma_device_type_playback);
}

static void ma_stream_event_process_capture__pipewire(void* pUserData)
{
    ma_stream_event_process__pipewire(pUserData, ma_device_type_capture);
}


static const struct ma_pw_stream_events ma_gStreamEventsPipeWire_Playback =
{
    MA_PW_VERSION_STREAM_EVENTS,
    NULL, /* destroy       */
    NULL, /* state_changed */
    NULL, /* control_info  */
    NULL, /* io_changed    */
    ma_stream_event_param_changed_playback__pipewire,
    NULL, /* add_buffer */
    NULL, /* remove_buffer */
    ma_stream_event_process_playback__pipewire,
    NULL, /* drained       */
    NULL, /* command       */
    NULL, /* trigger_done  */
};


static const struct ma_pw_stream_events ma_gStreamEventsPipeWire_Capture =
{
    MA_PW_VERSION_STREAM_EVENTS,
    NULL, /* destroy       */
    NULL, /* state_changed */
    NULL, /* control_info  */
    NULL, /* io_changed    */
    ma_stream_event_param_changed_capture__pipewire,
    NULL, /* add_buffer */
    NULL, /* remove_buffer */
    ma_stream_event_process_capture__pipewire,
    NULL, /* drained       */
    NULL, /* command       */
    NULL, /* trigger_done  */
};

static ma_result ma_device_init_internal__pipewire(ma_device* pDevice, ma_context_state_pipewire* pContextStatePipeWire, ma_device_state_pipewire* pDeviceStatePipeWire, const ma_device_config_pipewire* pDeviceConfigPipeWire, ma_device_type deviceType, ma_device_descriptor* pDescriptor)
{
    ma_pipewire_stream_state* pStreamState;
    struct ma_pw_properties* pProperties;
    const struct ma_pw_stream_events* pStreamEvents;
    enum ma_spa_audio_format formatPA;
    struct ma_spa_pod_audio_info_raw podAudioInfo;
    const struct ma_spa_pod* pConnectionParameters[1];
    enum ma_pw_stream_flags streamFlags;
    int connectResult;

    /* This function can only be called for playback or capture sides. */
    if (deviceType != ma_device_type_playback && deviceType != ma_device_type_capture) {
        return MA_INVALID_ARGS;
    }

    if (deviceType == ma_device_type_playback) {
        pStreamState = &pDeviceStatePipeWire->playback;
        pStreamEvents = &ma_gStreamEventsPipeWire_Playback;
    } else {
        pStreamState = &pDeviceStatePipeWire->capture;
        pStreamEvents = &ma_gStreamEventsPipeWire_Capture;
    }

    /* Set up the buffer size first so the parameter negotiation callback knows how to configure the buffer on the PipeWire side. */
    pStreamState->pDescriptor = pDescriptor;
    pStreamState->bufferSizeInFrames = pDescriptor->periodSizeInFrames;

    pProperties = pContextStatePipeWire->pw_properties_new(
        MA_PW_KEY_MEDIA_TYPE,     "Audio",
        MA_PW_KEY_MEDIA_CATEGORY, (deviceType == ma_device_type_playback) ? "Playback" : "Capture",
        MA_PW_KEY_MEDIA_ROLE,     (pDeviceConfigPipeWire->pMediaRole != NULL) ? pDeviceConfigPipeWire->pMediaRole : "Game",
        /* MA_PW_KEY_NODE_LATENCY is set during format negotiation because it depends on knowledge of the sample rate. */
        NULL);

    if (pDescriptor->pDeviceID != NULL) {
        pContextStatePipeWire->pw_properties_set(pProperties, MA_PW_KEY_TARGET_OBJECT, pDescriptor->pDeviceID->custom.s);
    }

    pStreamState->pStream = pContextStatePipeWire->pw_stream_new(pDeviceStatePipeWire->pCore, (pDeviceConfigPipeWire->pStreamName != NULL) ? pDeviceConfigPipeWire->pStreamName : "miniaudio", pProperties);
    if (pStreamState->pStream == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to create PipeWire stream.");
        return MA_ERROR;
    }

    /* This installs callbacks for process and param_changed. "process" is for queuing audio data, and "param_changed" is for getting the internal format/channels/rate. */
    pContextStatePipeWire->pw_stream_add_listener(pStreamState->pStream, &pStreamState->eventListener, pStreamEvents, pDeviceStatePipeWire);

    /* If the format is SPA_AUDIO_FORMAT_UNKNOWN, PipeWire can pick a planar data layout (de-interleaved) which breaks things for us. Just force interleaved F32 in this case. */
    formatPA = ma_format_to_pipewire(pDescriptor->format);
    if (formatPA == MA_SPA_AUDIO_FORMAT_UNKNOWN) {
        formatPA =  MA_SPA_AUDIO_FORMAT_F32;
    }

    podAudioInfo = ma_spa_pod_audio_info_raw_init(formatPA, pDescriptor->channels, pDescriptor->sampleRate);
    pConnectionParameters[0] = (struct ma_spa_pod*)&podAudioInfo;
    

    /*
    I'm just using MAP_BUFFERS because it's what the PipeWire examples do. I don't know what this does. Also, what's the
    point in the AUTOCONNECT flag? Is that not what we're already doing by calling a function called "connect"?!

    We also can't use INACTIVE because without it, the param_changed callback will not get called, but we depend on that
    so we can get access to the internal format/channels/rate.
    */
    streamFlags = (enum ma_pw_stream_flags)(MA_PW_STREAM_FLAG_AUTOCONNECT | /*MA_PW_STREAM_FLAG_INACTIVE |*/ MA_PW_STREAM_FLAG_MAP_BUFFERS);

    connectResult = pContextStatePipeWire->pw_stream_connect(pStreamState->pStream, (deviceType == ma_device_type_playback) ? MA_SPA_DIRECTION_OUTPUT : MA_SPA_DIRECTION_INPUT, MA_PW_ID_ANY, streamFlags, pConnectionParameters, ma_countof(pConnectionParameters));
    if (connectResult < 0) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to connect PipeWire stream.");
        pContextStatePipeWire->pw_stream_destroy(pStreamState->pStream);
        return MA_ERROR;
    }

    /* We need to keep iterating until we have finalized our internal format. */
    while ((pStreamState->initStatus & MA_PIPEWIRE_INIT_STATUS_HAS_FORMAT) == 0) {
        pContextStatePipeWire->pw_loop_iterate(pDeviceStatePipeWire->pLoop, 1);
    }

    /* We should have our format at this point, but we will not know the exact period size yet until we've done the first processing callback. */
    pDescriptor->format     = pStreamState->format;
    pDescriptor->channels   = pStreamState->channels;
    pDescriptor->sampleRate = pStreamState->sampleRate;
    ma_channel_map_copy_or_default(pDescriptor->channelMap, ma_countof(pDescriptor->channelMap), pStreamState->channelMap, pStreamState->channels);

    /* Now we need to wait until we know our period size. */
    while ((pStreamState->initStatus & MA_PIPEWIRE_INIT_STATUS_HAS_LATENCY) == 0) {
        pContextStatePipeWire->pw_loop_iterate(pDeviceStatePipeWire->pLoop, 1);
    }

    pDescriptor->periodSizeInFrames = pStreamState->rbSizeInFrames;
    pDescriptor->periodCount        = 1;

    /* Devices are in a stopped state by default in miniaudio. */
    pContextStatePipeWire->pw_stream_set_active(pStreamState->pStream, MA_FALSE);

    pStreamState->pDescriptor = NULL;
    pStreamState->initStatus |= MA_PIPEWIRE_INIT_STATUS_INITIALIZED;
    return MA_SUCCESS;
}

static void ma_device_on_wakupe__pipewire(void* pUserData, ma_uint64 count)
{
    /* Nothing to do here. This is only used for waking up the loop. */
    (void)pUserData;
    (void)count;
}

static ma_result ma_device_init__pipewire(ma_device* pDevice, const void* pDeviceBackendConfig, ma_device_descriptor* pDescriptorPlayback, ma_device_descriptor* pDescriptorCapture, void** ppDeviceState)
{
    ma_result result;
    struct ma_pw_loop* pLoop;
    struct ma_pw_context* pPipeWireContext;
    struct ma_pw_core* pCore;
    ma_context_state_pipewire* pContextStatePipeWire;
    ma_device_state_pipewire* pDeviceStatePipeWire;
    const ma_device_config_pipewire* pDeviceConfigPipeWire;
    ma_device_config_pipewire defaultDeviceConfigPipeWire;
    ma_device_type deviceType;

    pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));
    MA_PIPEWIRE_ASSERT(pContextStatePipeWire != NULL);

    /* Grab the config. This can be null in which case we'll use a default. */
    pDeviceConfigPipeWire = (const ma_device_config_pipewire*)pDeviceBackendConfig;
    if (pDeviceConfigPipeWire == NULL) {
        defaultDeviceConfigPipeWire = ma_device_config_pipewire_init();
        pDeviceConfigPipeWire = &defaultDeviceConfigPipeWire;
    }

    deviceType = ma_device_get_type(pDevice);
    
    /* Not sure how to do loopback with PipeWire, but it feels like something PipeWire would support. Look into this. */
    if (deviceType == ma_device_type_loopback) {
        return MA_DEVICE_TYPE_NOT_SUPPORTED;
    }

    pLoop = pContextStatePipeWire->pw_loop_new(NULL);
    if (pLoop == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to create PipeWire loop.");
        return MA_ERROR;
    }

    pPipeWireContext = pContextStatePipeWire->pw_context_new(pLoop, NULL, 0);
    if (pPipeWireContext == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to create PipeWire context.");
        pContextStatePipeWire->pw_loop_destroy(pLoop);
        return MA_ERROR;
    }

    pCore = pContextStatePipeWire->pw_context_connect(pPipeWireContext, NULL, 0);
    if (pCore == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to connect PipeWire context.");
        pContextStatePipeWire->pw_context_destroy(pPipeWireContext);
        pContextStatePipeWire->pw_loop_destroy(pLoop);
        return MA_ERROR;
    }

    /* We can now allocate our per-device PipeWire-specific data. */
    pDeviceStatePipeWire = (ma_device_state_pipewire*)ma_calloc(sizeof(*pDeviceStatePipeWire), ma_device_get_allocation_callbacks(pDevice));
    if (pDeviceStatePipeWire == NULL) {
        pContextStatePipeWire->pw_core_disconnect(pCore);
        pContextStatePipeWire->pw_context_destroy(pPipeWireContext);
        pContextStatePipeWire->pw_loop_destroy(pLoop);
        return MA_OUT_OF_MEMORY;
    }

    pDeviceStatePipeWire->pContextStatePipeWire = pContextStatePipeWire;
    pDeviceStatePipeWire->deviceType = deviceType;
    pDeviceStatePipeWire->pDevice    = pDevice;
    pDeviceStatePipeWire->pLoop      = pLoop;
    pDeviceStatePipeWire->pContext   = pPipeWireContext;
    pDeviceStatePipeWire->pCore      = pCore;

    /* Enter the main loop before we start iterating. */
    pContextStatePipeWire->pw_loop_enter(pLoop);
    
    if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__pipewire(pDevice, pContextStatePipeWire, pDeviceStatePipeWire, pDeviceConfigPipeWire, ma_device_type_capture, pDescriptorCapture);
    }
    if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
        result = ma_device_init_internal__pipewire(pDevice, pContextStatePipeWire, pDeviceStatePipeWire, pDeviceConfigPipeWire, ma_device_type_playback, pDescriptorPlayback);
    }

    if (result != MA_SUCCESS) {
        pContextStatePipeWire->pw_core_disconnect(pCore);
        pContextStatePipeWire->pw_context_destroy(pPipeWireContext);
        pContextStatePipeWire->pw_loop_destroy(pLoop);
        ma_free(pDeviceStatePipeWire, ma_device_get_allocation_callbacks(pDevice));
        return result;
    }

    /* We need an event for waking up the loop. */
    pDeviceStatePipeWire->pWakeup = pContextStatePipeWire->pw_loop_add_event(pLoop, (ma_spa_source_event_func_t)ma_device_on_wakupe__pipewire, pDeviceStatePipeWire);   /* Cast should be fine here. It's complaining about the difference between ma_uint64 and uint64_t. */
    if (pDeviceStatePipeWire->pWakeup == NULL) {
        ma_log_postf(ma_device_get_log(pDevice), MA_LOG_LEVEL_ERROR, "Failed to create PipeWire loop wakeup event.");
        pContextStatePipeWire->pw_core_disconnect(pCore);
        pContextStatePipeWire->pw_context_destroy(pPipeWireContext);
        pContextStatePipeWire->pw_loop_destroy(pLoop);
        ma_free(pDeviceStatePipeWire, ma_device_get_allocation_callbacks(pDevice));
        return MA_ERROR;
    }

    *ppDeviceState = pDeviceStatePipeWire;

    return MA_SUCCESS;
}

static void ma_device_uninit__pipewire(ma_device* pDevice)
{
    ma_device_state_pipewire* pDeviceStatePipeWire = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));
    ma_device_type deviceType = ma_device_get_type(pDevice);

    if (pDeviceStatePipeWire->capture.pStream != NULL) {
        pContextStatePipeWire->pw_stream_destroy(pDeviceStatePipeWire->capture.pStream);
        pDeviceStatePipeWire->capture.pStream = NULL;
    }

    if (pDeviceStatePipeWire->playback.pStream != NULL) {
        pContextStatePipeWire->pw_stream_destroy(pDeviceStatePipeWire->playback.pStream);
        pDeviceStatePipeWire->playback.pStream = NULL;
    }

    /* This will be called from the same thread that called ma_device_init__pipewire() and is therefore an appropriate place to leave the main loop. */
    pContextStatePipeWire->pw_loop_leave(pDeviceStatePipeWire->pLoop);

    pContextStatePipeWire->pw_core_disconnect(pDeviceStatePipeWire->pCore);
    pContextStatePipeWire->pw_context_destroy(pDeviceStatePipeWire->pContext);
    pContextStatePipeWire->pw_loop_destroy(pDeviceStatePipeWire->pLoop);

    if (ma_device_get_threading_mode(pDeviceStatePipeWire->pDevice) == MA_THREADING_MODE_SINGLE_THREADED) {
        if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
            ma_pcm_rb_uninit(&pDeviceStatePipeWire->capture.rb);
        }
        if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
            ma_pcm_rb_uninit(&pDeviceStatePipeWire->playback.rb);
        }
    }

    ma_free(pDeviceStatePipeWire, ma_device_get_allocation_callbacks(pDevice));
}


static ma_result ma_device_start__pipewire(ma_device* pDevice)
{
    ma_device_state_pipewire* pDeviceStatePipeWire = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));

    /* Prepare our buffers before starting the streams. To do this we just need to step. */
    ma_device_step__pipewire(pDevice, MA_BLOCKING_MODE_NON_BLOCKING);

    if (pDeviceStatePipeWire->capture.pStream != NULL) {
        pContextStatePipeWire->pw_stream_set_active(pDeviceStatePipeWire->capture.pStream, MA_TRUE);
    }

    if (pDeviceStatePipeWire->playback.pStream != NULL) {
        pContextStatePipeWire->pw_stream_set_active(pDeviceStatePipeWire->playback.pStream, MA_TRUE);
    }

    return MA_SUCCESS;
}

static ma_result ma_device_stop__pipewire(ma_device* pDevice)
{
    ma_device_state_pipewire* pDeviceStatePipeWire = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));

    if (pDeviceStatePipeWire->capture.pStream != NULL) {
        pContextStatePipeWire->pw_stream_set_active(pDeviceStatePipeWire->capture.pStream, MA_FALSE);
    }

    if (pDeviceStatePipeWire->playback.pStream != NULL) {
        pContextStatePipeWire->pw_stream_set_active(pDeviceStatePipeWire->playback.pStream, MA_FALSE);
    }

    return MA_SUCCESS;
}

static ma_result ma_device_step__pipewire(ma_device* pDevice, ma_blocking_mode blockingMode)
{
    ma_device_state_pipewire* pDeviceStatePipeWire = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));
    ma_device_type deviceType = ma_device_get_type(pDevice);
    int timeout;
    ma_bool32 hasProcessedData = MA_FALSE;

    if (blockingMode == MA_BLOCKING_MODE_BLOCKING) {
        timeout = -1;
    } else {
        timeout = 0;
    }

    /* We will keep looping until we've processed some data. This should keep our stepping in time with data processing. */
    for (;;) {
        pContextStatePipeWire->pw_loop_iterate(pDeviceStatePipeWire->pLoop, timeout);

        if (!ma_device_is_started(pDevice)) {
            return MA_DEVICE_NOT_STARTED;
        }

        /* We need only process data here in single-threaded mode. */
        if (ma_device_get_threading_mode(pDeviceStatePipeWire->pDevice) == MA_THREADING_MODE_SINGLE_THREADED) {
            /* We want to handle both playback and capture in a single iteration for duplex mode. */
            if (deviceType == ma_device_type_capture || deviceType == ma_device_type_duplex) {
                ma_uint32 framesAvailable;

                framesAvailable = ma_pcm_rb_available_read(&pDeviceStatePipeWire->capture.rb);
                if (framesAvailable > 0) {
                    hasProcessedData = MA_TRUE;
                }

                while (framesAvailable > 0) {
                    void* pMappedBuffer;
                    ma_uint32 framesToRead = framesAvailable;
                    ma_result result;

                    result = ma_pcm_rb_acquire_read(&pDeviceStatePipeWire->capture.rb, &framesToRead, &pMappedBuffer);
                    if (result == MA_SUCCESS) {
                        ma_device_handle_backend_data_callback(pDevice, NULL, pMappedBuffer, framesToRead);

                        result = ma_pcm_rb_commit_read(&pDeviceStatePipeWire->capture.rb, framesToRead);
                        framesAvailable -= framesToRead;
                    }
                }
            }
            
            if (deviceType == ma_device_type_playback || deviceType == ma_device_type_duplex) {
                ma_uint32 framesAvailable;

                framesAvailable = ma_pcm_rb_available_write(&pDeviceStatePipeWire->playback.rb);
                if (framesAvailable > 0) {
                    hasProcessedData = MA_TRUE;
                }

                while (framesAvailable > 0) {
                    void* pMappedBuffer;
                    ma_uint32 framesToWrite = framesAvailable;
                    ma_result result;

                    result = ma_pcm_rb_acquire_write(&pDeviceStatePipeWire->playback.rb, &framesToWrite, &pMappedBuffer);
                    if (result == MA_SUCCESS) {
                        ma_device_handle_backend_data_callback(pDevice, pMappedBuffer, NULL, framesToWrite);

                        result = ma_pcm_rb_commit_write(&pDeviceStatePipeWire->playback.rb, framesToWrite);
                        framesAvailable -= framesToWrite;
                    }
                }
            }
        }

        if (hasProcessedData || blockingMode == MA_BLOCKING_MODE_NON_BLOCKING) {
            break;
        }
    }

    return MA_SUCCESS;
}

static void ma_device_wake__pipewire(ma_device* pDevice)
{
    ma_device_state_pipewire* pDeviceStatePipeWire = ma_device_get_backend_state__pipewire(pDevice);
    ma_context_state_pipewire* pContextStatePipeWire = ma_context_get_backend_state__pipewire(ma_device_get_context(pDevice));

    pContextStatePipeWire->pw_loop_signal_event(pDeviceStatePipeWire->pLoop, pDeviceStatePipeWire->pWakeup);
}


static ma_device_backend_vtable ma_gDeviceBackendVTable_PipeWire =
{
    ma_backend_info__pipewire,
    ma_context_init__pipewire,
    ma_context_uninit__pipewire,
    ma_context_enumerate_devices__pipewire,
    ma_device_init__pipewire,
    ma_device_uninit__pipewire,
    ma_device_start__pipewire,
    ma_device_stop__pipewire,
    ma_device_step__pipewire,
    ma_device_wake__pipewire
};

ma_device_backend_vtable* ma_device_backend_pipewire = &ma_gDeviceBackendVTable_PipeWire;

#if defined(MA_NO_RUNTIME_LINKING)
    #if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
        #pragma GCC diagnostic pop
    #endif
#endif

#else
ma_device_backend_vtable* ma_device_backend_pipewire = NULL;
#endif  /* MA_HAS_PIPEWIRE */

MA_API ma_context_config_pipewire ma_context_config_pipewire_init(void)
{
    ma_context_config_pipewire config;

    memset(&config, 0, sizeof(config));

    return config;
}

MA_API ma_device_config_pipewire ma_device_config_pipewire_init(void)
{
    ma_device_config_pipewire config;

    memset(&config, 0, sizeof(config));

    return config;
}
#endif /* miniaudio_backend_pipewire_c */
