#include "../../external/fs/fs.c"
#include "../../miniaudio.c"

#if defined(MA_CLI_ENABLE_LIBVORBIS)
#include "../../extras/decoders/libvorbis/miniaudio_libvorbis.c"
#endif
#if defined(MA_CLI_ENABLE_LIBOPUS)
#include "../../extras/decoders/libopus/miniaudio_libopus.c"
#endif

static const char* pHelpOverview =
    "USAGE:                                                                         \n"
    "    miniaudio [version | -v | --version] [help | -h | --help] [<global args>]  \n"
    "              <command> [<args>] : <command> [<args>] : ...                    \n"
    "                                                                               \n"
    "    The output of one command can be used as the input to another. Commands are\n"
    "    executed left to right, and are separated with ':'.                        \n"
    "                                                                               \n"
    "    miniaudio help commands        List all available commands.                \n"
    "    miniaudio help <command>       Print help for a specific command.          \n"
    "                                                                               \n"
    "EXAMPLES:                                                                      \n"
    "    miniaudio play sound.mp3                                                   \n"
    "        Play a sound from a file.                                              \n"
    "                                                                               \n"
    "    miniaudio record output.wav                                                \n"
    "        Record to a file.                                                      \n"
    "                                                                               \n"
    "    miniaudio record : play                                                    \n"
    "        Record and play back.                                                  \n"
    "                                                                               \n"
    "OPTIONS:                                                                       \n"
    "    --backend [backend1,backend2,...]                                          \n"
    "        Sets the backend prioritization for playback and capture. Backends     \n"
    "        should be comma separated and not have spaces. Available backends:     \n"
    "                                                                               \n"
    #if defined(MA_HAS_WASAPI)
    "        wasapi                                                                 \n"
    #endif
    #if defined(MA_HAS_DSOUND)
    "        directsound                                                            \n"
    #endif
    #if defined(MA_HAS_WINMM)
    "        winmm                                                                  \n"
    #endif
    #if defined(MA_HAS_COREAUDIO)
    "        coreaudio                                                              \n"
    #endif
    #if defined(MA_HAS_PIPEWIRE)
    "        pipewire                                                               \n"
    #endif
    #if defined(MA_HAS_PULSEAUDIO)
    "        pulseaudio                                                             \n"
    #endif
    #if defined(MA_HAS_JACK)
    "        jack                                                                   \n"
    #endif
    #if defined(MA_HAS_ALSA)
    "        alsa                                                                   \n"
    #endif
    #if defined(MA_HAS_SNDIO)
    "        sndio                                                                  \n"
    #endif
    #if defined(MA_HAS_AUDIO4)
    "        audio4                                                                 \n"
    #endif
    #if defined(MA_HAS_OSS)
    "        oss                                                                    \n"
    #endif
    #if defined(MA_HAS_AAUDIO)
    "        aaudio                                                                 \n"
    #endif
    #if defined(MA_HAS_OPENSL)
    "        opensl                                                                 \n"
    #endif
    #if defined(MA_HAS_WEBAUDIO)
    "        webaudio                                                               \n"
    #endif
    #if defined(MA_HAS_DREAMCAST)
    "        dreamcast                                                              \n"
    #endif
    #if defined(MA_HAS_XAUDIO)
    "        xaudio                                                                 \n"
    #endif
    #if defined(MA_HAS_VITA)
    "        vita                                                                   \n"
    #endif
    #if defined(MA_HAS_NULL)
    "        null                                                                   \n"
    #endif
    #if defined(MA_HAS_SDL2)
    "        sdl2                                                                   \n"
    #endif
    "                                                                               \n"
    "        Example: --backend pulseaudio,pipewire                                 \n"
    "                                                                               \n"
    "ALLOWABLE SAMPLE FORMATS:                                                      \n"
    "    u8     8-bit unsigned integer                                              \n"
    "    s16    16-bit signed integer                                               \n"
    "    s24    24-bit signed integer (tightly packed)                              \n"
    "    s32    32-bit signed integer                                               \n"
    "    f32    32-bit floating point                                               \n"
;

#define MA_CLI_HELP_PLAY                                                               \
    "USAGE:                                                                         \n"\
    "    play <input>                                                               \n"\
    "                                                                               \n"\
    "DESCRIPTION:                                                                   \n"\
    "    Plays a sound. If no inputs are specified, the sound will be read from the \n"\
    "    output of the previous command.                                            \n"\
    "                                                                               \n"\
    "    Only a single play command is allowed, and there are no outputs which means\n"\
    "    it would typically always be the last command.                             \n"\
    "                                                                               \n"\
    "EXAMPLES:                                                                      \n"\
    "    miniaudio play sound.mp3                                                   \n"\
    "        Play a sound from a file.                                              \n"\
    "                                                                               \n"\
    "    miniaudio decode sound.mp3 : play                                          \n"\
    "        Play a sound from a file.                                              \n"\
    "                                                                               \n"\
    "    miniaudio waveform : play                                                  \n"\
    "        Play a sine wave.                                                      \n"\
    "                                                                               \n"\
    "    miniaudio record : play                                                    \n"\
    "        Capture from microphone and play back.                                 \n"\
    "                                                                               \n"\
    "OPTIONS:                                                                       \n"\
    "    -f, --format     [default: system default]                                 \n"\
    "        The device sample format.                                              \n"\
    "                                                                               \n"\
    "    -c, --channels   [default: system default]                                 \n"\
    "        The device channel count.                                              \n"\
    "                                                                               \n"\
    "    -r, --rate       [default: system default]                                 \n"\
    "        The device sample rate.                                                \n"\

#define MA_CLI_HELP_RECORD                                                             \
    "USAGE:                                                                         \n"\
    "    record <output>                                                            \n"\
    "                                                                               \n"\
    "DESCRIPTION:                                                                   \n"\
    "    Records from a microphone.                                                 \n"\
    "                                                                               \n"\
    "EXAMPLES:                                                                      \n"\
    "    miniaudio record sound.wav                                                 \n"\
    "        Record a sound an output to a file.                                    \n"\
    "                                                                               \n"\
    "    miniaudio record : encode sound.wav                                        \n"\
    "        Record a sound an output to a file.                                    \n"\
    "                                                                               \n"\
    "    miniaudio record : play                                                    \n"\
    "        Capture from microphone and play back.                                 \n"\
    "                                                                               \n"\
    "OPTIONS:                                                                       \n"\
    "    -f, --format     [default: system default]                                 \n"\
    "        The device sample format.                                              \n"\
    "                                                                               \n"\
    "    -c, --channels   [default: system default]                                 \n"\
    "        The device channel count.                                              \n"\
    "                                                                               \n"\
    "    -r, --rate       [default: system default]                                 \n"\
    "        The device sample rate.                                                \n"\

#define MA_CLI_HELP_DECODE                                                             \
    "USAGE:                                                                         \n"\
    "    decode <input file> : <output command>                                     \n"\
    "                                                                               \n"\
    "DESCRIPTION:                                                                   \n"\
    "    Decodes a file. You would typically route the output to the input of       \n"\
    "    another command.                                                           \n"\
    "                                                                               \n"\
    "EXAMPLES:                                                                      \n"\
    "    miniaudio decode sound.mp3 : play                                          \n"\
    "        Decode a file and play it.                                             \n"\
    "                                                                               \n"\
    "    miniaudio decode sound.mp3 : encode sound.wav                              \n"\
    "        Decode and re-encode a file.                                           \n"\
    "                                                                               \n"\
    "OPTIONS:                                                                       \n"\
    "    -f, --format     [default: file default]                                   \n"\
    "        The output sample format.                                              \n"\
    "                                                                               \n"\
    "    -c, --channels   [default: file default]                                   \n"\
    "        The output channel count.                                              \n"\
    "                                                                               \n"\
    "    -r, --rate       [default: file default]                                   \n"\
    "        The output sample rate.                                                \n"\

#define MA_CLI_HELP_ENCODE                                                             \
    "USAGE:                                                                         \n"\
    "    <input command> : encode <output file>                                     \n"\
    "                                                                               \n"\
    "DESCRIPTION:                                                                   \n"\
    "    Encodes a file. You would typically route the output of another command to \n"\
    "    the input of this command.                                                 \n"\
    "                                                                               \n"\
    "EXAMPLES:                                                                      \n"\
    "    miniaudio record : encode sound.wav                                        \n"\
    "        Capture audio from a microphone and then output to a file.             \n"\
    "                                                                               \n"\
    "    miniaudio decode sound.mp3 : encode sound.wav                              \n"\
    "        Decode and re-encode a file.                                           \n"\
    "                                                                               \n"\
    "OPTIONS:                                                                       \n"\
    "    -f, --format     [default: file default]                                   \n"\
    "        The output sample format.                                              \n"\
    "                                                                               \n"\
    "    -c, --channels   [default: file default]                                   \n"\
    "        The output channel count.                                              \n"\
    "                                                                               \n"\
    "    -r, --rate       [default: file default]                                   \n"\
    "        The output sample rate.                                                \n"\

#define MA_CLI_HELP_WAVEFORM                                                           \
    "USAGE:                                                                         \n"\
    "    waveform <output>                                                          \n"\
    "                                                                               \n"\
    "DESCRIPTION:                                                                   \n"\
    "    Generates a sine wave.                                                     \n"\
    "                                                                               \n"\
    "    This does not accept any inputs.                                           \n"\
    "                                                                               \n"\
    "OPTIONS:                                                                       \n"\
    "    -f, --format     [default: f32]                                            \n"\
    "        The output sample format.                                              \n"\
    "                                                                               \n"\
    "    -c, --channels   [default: 1]                                              \n"\
    "        The output channel count.                                              \n"\
    "                                                                               \n"\
    "    -r, --rate       [default: 48000]                                          \n"\
    "        The output sample rate.                                                \n"\
    "                                                                               \n"\
    "    --type           [default: sine]                                           \n"\
    "        The waveform type. Allowable values:                                   \n"\
    "            sine                                                               \n"\
    "            square                                                             \n"\
    "            triangle                                                           \n"\
    "            sawtooth                                                           \n"\
    "                                                                               \n"\
    "    --amplitude      [default: 0.1]                                            \n"\
    "        Controls the volume of the output. A value of 1 will be quite harsh so \n"\
    "        take care when setting this.                                           \n"\
    "                                                                               \n"\
    "    --frequency      [default: 220]                                            \n"\
    "        The frequency of the generated sine wave. Larger values will be higher \n"\
    "        pitched.                                                               \n"\

#include <signal.h>

#define MA_CLI_DEFAULT_PERIOD_SIZE_IN_FRAMES    512

/* These should be in priority order. */
#if defined(MA_CLI_ENABLE_LIBVORBIS)
    #define MA_CLI_DECODING_BACKEND_LIBVORBIS   ma_decoding_backend_libvorbis,
#else
    #define MA_CLI_DECODING_BACKEND_LIBVORBIS
#endif

#if defined(MA_CLI_ENABLE_LIBOPUS)
    #define MA_CLI_DECODING_BACKEND_LIBOPUS     ma_decoding_backend_libopus,
#else
    #define MA_CLI_DECODING_BACKEND_LIBOPUS
#endif

#define MA_CLI_DECODING_BACKENDS   \
    MA_CLI_DECODING_BACKEND_LIBVORBIS \
    MA_CLI_DECODING_BACKEND_LIBOPUS   \
    ma_decoding_backend_wav,       \
    ma_decoding_backend_flac,      \
    ma_decoding_backend_mp3  /* <-- MP3 seems to be worst/slowest at detecting whether or not it's a valid file so we'll put this last. */


static fs_file* MA_CLI_STDIN;
static fs_file* MA_CLI_STDOUT;
static fs_file* MA_CLI_STDERR;

static ma_result ma_result_from_fs(fs_result result)
{
    return (ma_result)result;
}


/* BEG ma_cli_args */
typedef struct
{
    const char* pToken;
    size_t tokenLen;
    struct
    {
        int iarg;
        int argc;
        char** argv;
    } private;
} ma_cli_args;

ma_cli_args ma_cli_args_first(int argc, char** argv);
ma_bool32 ma_cli_args_at_end(ma_cli_args* pArgs);
ma_bool32 ma_cli_args_next(ma_cli_args* pArgs);

ma_cli_args ma_cli_args_first(int argc, char** argv)
{
    ma_cli_args args;

    MA_ZERO_OBJECT(&args);
    args.private.iarg = 0;
    args.private.argc = argc;
    args.private.argv = argv;
    ma_cli_args_next(&args);

    return args;
}

ma_bool32 ma_cli_args_at_end(ma_cli_args* pArgs)
{
    if (pArgs == NULL) {
        return MA_TRUE;
    }

    return pArgs->private.iarg >= pArgs->private.argc;
}

ma_bool32 ma_cli_args_next(ma_cli_args* pArgs)
{
    if (pArgs == NULL) {
        return MA_FALSE;
    }

    if (ma_cli_args_at_end(pArgs)) {
        return MA_FALSE;
    }

    /* Some setup for the first iteration. */
    if (pArgs->pToken == NULL && pArgs->private.iarg == 0 && pArgs->private.argc > 0) {
        pArgs->pToken   = pArgs->private.argv[0];
        pArgs->tokenLen = 0;
    }

    if (pArgs->pToken == NULL) {
        return MA_FALSE;
    }

    pArgs->pToken += pArgs->tokenLen;
    if (pArgs->pToken[0] == '\0') {
        /* Reached the end of the token. Move to the next argument. */
        pArgs->private.iarg += 1;
        if (ma_cli_args_at_end(pArgs)) {
            return MA_FALSE;
        }

        pArgs->pToken = pArgs->private.argv[pArgs->private.iarg];
    }

    /*
    For now we are just treating each whole argument as a single token, but later on I want to make it
    so each argument can be split up into parts so we can avoid excessive space separations. This can
    come later once things has stabilized a bit.
    */
    pArgs->tokenLen = strlen(pArgs->pToken);

    return MA_TRUE;
}

ma_bool32 ma_cli_args_equal(ma_cli_args* pArgs, const char* pName)
{
    return fs_strncmp(pName, pArgs->pToken, pArgs->tokenLen) == 0;
}
/* END ma_cli_args */


static void ma_cli_process_node_graph(ma_node_graph* pNodeGraph);

#include "miniaudio_cli_node.c"


typedef struct
{
    ma_cli_node_vtable* pVTable;
    ma_uint32 inputCount;
    ma_uint32 outputCount;  /* Set this to ~0 if the output count is variable, such as a splitter. */
    const char* pName;
    const char* pSummary;
    const char* pHelp;
} ma_cli_command;

static ma_cli_command pCommands[] =
{
    { &ma_gNodeVTable_Play,     1, 0, "play",     "Plays a sound.",             MA_CLI_HELP_PLAY     },
    { &ma_gNodeVTable_Record,   0, 1, "record",   "Records from a microphone.", MA_CLI_HELP_RECORD   },
    { &ma_gNodeVTable_Decode,   0, 1, "decode",   "Decode a sound.",            MA_CLI_HELP_DECODE   },
    { &ma_gNodeVTable_Encode,   1, 0, "encode",   "Encode a sound.",            MA_CLI_HELP_ENCODE   },
    { &ma_gNodeVTable_Waveform, 0, 1, "waveform", "Generates a waveform tone.", MA_CLI_HELP_WAVEFORM }
};



/* BEG ma_cli_node_list */
typedef struct
{
    ma_cli_node* items[255];
    ma_uint32 count;
} ma_cli_node_list;

MA_API ma_cli_node_list* ma_cli_node_list_push(ma_cli_node_list* pList, ma_cli_node* pNode)
{
    if (pList == NULL) {
        pList = (ma_cli_node_list*)ma_calloc(sizeof(ma_cli_node_list), NULL);
        if (pList == NULL) {
            return NULL;
        }
    }

    if (pList->count >= ma_countof(pList->items)) {
        return NULL;    /* Ran out of space. */ /* TODO: Just dynamically expand this. */
    }

    pList->items[pList->count] = pNode;
    pList->count += 1;

    return pList;
}

MA_API ma_uint32 ma_cli_node_list_count(ma_cli_node_list* pList)
{
    if (pList == NULL) {
        return 0;
    }

    return pList->count;
}

MA_API ma_cli_node* ma_cli_node_list_get(ma_cli_node_list* pList, ma_uint32 index)
{
    if (pList == NULL) {
        return NULL;
    }

    return pList->items[index];
}
/* END ma_cli_node_list */



/* Global program state. */
static ma_cli_node_list* g_pNodes = NULL;
static ma_cli_play* g_ppPlayNodes[16];
static ma_uint32 g_playNodeCount = 0;
static ma_cli_record* g_ppRecordNodes[16];
static ma_uint32 g_recordNodeCount = 0;
static ma_device_backend_config g_backends[32];
static ma_uint32 g_backendCount = 0;


static ma_result ma_cli_track_play_node(ma_cli_play* pPlay)
{
    if (g_playNodeCount >= ma_countof(g_ppPlayNodes)) {
        fs_file_writef(MA_CLI_STDERR, "Too many play nodes. You can have a maximum of %d.\n", (int)ma_countof(g_ppPlayNodes));
        return MA_INVALID_OPERATION;
    }

    g_ppPlayNodes[g_playNodeCount] = pPlay;
    g_playNodeCount += 1;

    return MA_SUCCESS;
}

static ma_uint32 ma_cli_get_play_node_count(void)
{
    return g_playNodeCount;
}

static ma_cli_play* ma_cli_get_play_node(ma_uint32 index)
{
    return g_ppPlayNodes[index];
}


static ma_result ma_cli_track_record_node(ma_cli_record* pRecord)
{
    if (g_recordNodeCount >= ma_countof(g_ppRecordNodes)) {
        fs_file_writef(MA_CLI_STDERR, "Too many record nodes. You can have a maximum of %d.\n", (int)ma_countof(g_ppRecordNodes));
        return MA_INVALID_OPERATION;
    }

    g_ppRecordNodes[g_recordNodeCount] = pRecord;
    g_recordNodeCount += 1;

    return MA_SUCCESS;
}

static ma_uint32 ma_cli_get_record_node_count(void)
{
    return g_recordNodeCount;
}

static ma_cli_record* ma_cli_get_record_node(ma_uint32 index)
{
    return g_ppRecordNodes[index];
}


static ma_result ma_cli_track_node(ma_cli_node* pNode)
{
    g_pNodes = ma_cli_node_list_push(g_pNodes, pNode);
    if (g_pNodes == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    if (pNode->pVTable == &ma_gNodeVTable_Play) {
        return ma_cli_track_play_node((ma_cli_play*)pNode);
    }

    if (pNode->pVTable == &ma_gNodeVTable_Record) {
        return ma_cli_track_record_node((ma_cli_record*)pNode);
    }

    return MA_SUCCESS;
}

static ma_uint32 ma_cli_get_node_count(void)
{
    return ma_cli_node_list_count(g_pNodes);
}

static ma_cli_node* ma_cli_get_node(ma_uint32 index)
{
    return ma_cli_node_list_get(g_pNodes, index);
}


static ma_cli_node* ma_cli_parse_command(ma_cli_args* pArgs, ma_cli_node* pPreviousNode);


static void ma_cli_version(void)
{
    /* The CLI version is just the version of miniaudio itself. */
    fs_file_writef(MA_CLI_STDOUT, "miniaudio v%d.%d.%d\n", MA_VERSION_MAJOR, MA_VERSION_MINOR, MA_VERSION_REVISION);
}

static void ma_cli_help_overview(void)
{
    ma_cli_version();
    fs_file_writef(MA_CLI_STDOUT, "\n%s\n", pHelpOverview);

    /* Now print our decoding backends. */
    fs_file_writef(MA_CLI_STDOUT, "AVAILABLE DECODING BACKENDS:\n");
    {
        size_t iDecodingBackend;
        ma_decoding_backend_vtable* pDecodingBackends[] =
        {
            MA_CLI_DECODING_BACKENDS
        };

        for (iDecodingBackend = 0; iDecodingBackend < ma_countof(pDecodingBackends); iDecodingBackend += 1) {
            if (pDecodingBackends[iDecodingBackend] != NULL && pDecodingBackends[iDecodingBackend]->onInfo != NULL) {
                ma_decoding_backend_info backendInfo;
                pDecodingBackends[iDecodingBackend]->onInfo(NULL, &backendInfo);
                fs_file_writef(MA_CLI_STDOUT, "    %s (via %s)\n", backendInfo.pName, backendInfo.pLibraryName);
            }
        }
    }
}

static void ma_cli_help_command(const char* pOpName)
{
    size_t iCommand;

    for (iCommand = 0; iCommand < ma_countof(pCommands); iCommand += 1) {
        if (strcmp(pCommands[iCommand].pName, pOpName) == 0) {
            fs_file_writef(MA_CLI_STDOUT, "%s\n", pCommands[iCommand].pHelp);
            break;
        }
    }
}

static void ma_cli_help_commands(void)
{
    size_t iCommand;

    for (iCommand = 0; iCommand < ma_countof(pCommands); iCommand += 1) {
        fs_file_writef(MA_CLI_STDOUT, "%-20s%s\n", pCommands[iCommand].pName, pCommands[iCommand].pSummary);
    }
}

static void ma_cli_help(int argc, char** argv)
{
    if (argc > 2) {
        int iarg;
        
        for (iarg = 2; iarg < argc; iarg += 1) {
            if (strcmp(argv[iarg], "commands") == 0) {
                ma_cli_help_commands();
            } else {
                ma_cli_help_command(argv[iarg]);
            }
        }
    } else {
        ma_cli_help_overview();
    }
}

typedef struct ma_cli_backend_name_map
{
    const char* pName;
    ma_device_backend_vtable* pVTable;
} ma_cli_backend_name_map;

static ma_device_backend_vtable* ma_cli_backend_vtable_from_string(const char* pName, size_t nameLen)
{
    size_t i;
    ma_cli_backend_name_map backends[] =
    {
        #if defined(MA_HAS_WASAPI)
        { "wasapi",      ma_device_backend_wasapi     },
        #endif
        #if defined(MA_HAS_DSOUND)
        { "directsound", ma_device_backend_dsound     },
        #endif
        #if defined(MA_HAS_WINMM)
        { "winmm",       ma_device_backend_winmm      },
        #endif
        #if defined(MA_HAS_COREAUDIO)
        { "coreaudio",   ma_device_backend_coreaudio  },
        #endif
        #if defined(MA_HAS_PIPEWIRE)
        { "pipewire",    ma_device_backend_pipewire   },
        #endif
        #if defined(MA_HAS_PULSEAUDIO)
        { "pulseaudio",  ma_device_backend_pulseaudio },
        #endif
        #if defined(MA_HAS_JACK)
        { "jack",        ma_device_backend_jack       },
        #endif
        #if defined(MA_HAS_ALSA)
        { "alsa",        ma_device_backend_alsa       },
        #endif
        #if defined(MA_HAS_SNDIO)
        { "sndio",       ma_device_backend_sndio      },
        #endif
        #if defined(MA_HAS_AUDIO4)
        { "audio4",      ma_device_backend_audio4     },
        #endif
        #if defined(MA_HAS_OSS)
        { "oss",         ma_device_backend_oss        },
        #endif
        #if defined(MA_HAS_AAUDIO)
        { "aaudio",      ma_device_backend_aaudio     },
        #endif
        #if defined(MA_HAS_OPENSL)
        { "opensl",      ma_device_backend_opensl     },
        #endif
        #if defined(MA_HAS_WEBAUDIO)
        { "webaudio",    ma_device_backend_webaudio   },
        #endif
        #if defined(MA_HAS_DREAMCAST)
        { "dreamcast",   ma_device_backend_dreamcast  },
        #endif
        #if defined(MA_HAS_XAUDIO)
        { "xaudio",      ma_device_backend_xaudio     },
        #endif
        #if defined(MA_HAS_VITA)
        { "vita",        ma_device_backend_vita       },
        #endif
        #if defined(MA_HAS_NULL)
        { "null",        ma_device_backend_null       }
        #endif
    };

    for (i = 0; i < sizeof(backends)/sizeof(backends[0]); i += 1) {
        if (fs_strncmp(backends[i].pName, pName, nameLen) == 0 && backends[i].pName[nameLen] == '\0') {
            return backends[i].pVTable;
        }
    }

    return NULL;
}

static ma_bool32 ma_cli_parse_backends(const char* pList)
{
    const char* pStart;
    const char* pEnd;

    g_backendCount = 0;

    pStart = pList;
    for (;;) {
        size_t nameLen;
        ma_device_backend_vtable* pVTable;

        pEnd = pStart;
        while (*pEnd != ',' && *pEnd != '\0') {
            pEnd += 1;
        }

        nameLen = (size_t)(pEnd - pStart);
        if (nameLen > 0) {
            if (g_backendCount >= ma_countof(g_backends)) {
                fs_file_writef(MA_CLI_STDERR, "Too many backends specified. Maximum is %d.\n", (int)ma_countof(g_backends));
                return MA_FALSE;
            }

            pVTable = ma_cli_backend_vtable_from_string(pStart, nameLen);
            if (pVTable == NULL) {
                fs_file_writef(MA_CLI_STDERR, "Unknown or unsupported backend \"%.*s\".\n", (int)nameLen, pStart);
                return MA_FALSE;
            }

            g_backends[g_backendCount] = ma_device_backend_config_init(pVTable, NULL);
            g_backendCount += 1;
        }

        if (*pEnd == '\0') {
            break;
        }

        pStart = pEnd + 1;
    }

    return MA_TRUE;
}

static ma_format ma_cli_format_from_string(const char* pFormat, size_t formatLen)
{
    if (fs_strncmp("f32", pFormat, formatLen) == 0) {
        return ma_format_f32;
    }
    if (fs_strncmp("s16", pFormat, formatLen) == 0) {
        return ma_format_s16;
    }
    if (fs_strncmp("s32", pFormat, formatLen) == 0) {
        return ma_format_s32;
    }
    if (fs_strncmp("s24", pFormat, formatLen) == 0) {
        return ma_format_s24;
    }
    if (fs_strncmp("u8", pFormat, formatLen) == 0) {
        return ma_format_u8;
    }

    return ma_format_unknown;
}



static ma_cli_node* ma_cli_create_node(void* pConfig)
{
    ma_result result;
    ma_cli_node* pNode;
    
    pNode = ma_cli_node_create(pConfig);
    if (pNode == NULL) {
        return NULL;
    }

    result = ma_cli_track_node(pNode);
    if (result != MA_SUCCESS) {
        ma_cli_node_delete(pNode);
        return NULL;
    }

    return pNode;
}

static void ma_cli_delete_node(ma_cli_node* pNode)
{
    ma_cli_node_delete(pNode);
}


static ma_bool32 ma_cli_try_parse_format(ma_cli_args* pArgs, ma_format* pFormat)
{
    if (ma_cli_args_equal(pArgs, "-f") || ma_cli_args_equal(pArgs, "--format")) {
        if (ma_cli_args_next(pArgs)) {
            *pFormat = ma_cli_format_from_string(pArgs->pToken, pArgs->tokenLen);

            if (*pFormat == ma_format_unknown) {
                fs_file_writef(MA_CLI_STDERR, "Unknown format \"%.*s\".\n", (int)pArgs->tokenLen, pArgs->pToken);
                return MA_FALSE;
            }
        } else {
            fs_file_writef(MA_CLI_STDERR, "Expecting format after \"%.*s\".\n", (int)pArgs->tokenLen, pArgs->pToken);
            return MA_FALSE;
        }
    } else {
        /* Not a format switch. This is not an error - it's just ignored. */
    }

    return MA_TRUE;
}

static ma_bool32 ma_cli_try_parse_channels(ma_cli_args* pArgs, ma_uint32* pChannels)
{
    if (ma_cli_args_equal(pArgs, "-c") || ma_cli_args_equal(pArgs, "--channels")) {
        if (ma_cli_args_next(pArgs)) {
            char temp[64];
            ma_strncpy_s(temp, sizeof(temp), pArgs->pToken, pArgs->tokenLen);

            *pChannels = atoi(temp);
        } else {
            fs_file_writef(MA_CLI_STDERR, "Expecting channel count after \"%.*s\".\n", (int)pArgs->tokenLen, pArgs->pToken);
            return MA_FALSE;
        }
    } else {
        /* Not a format switch. This is not an error - it's just ignored. */
    }

    return MA_TRUE;
}

static ma_bool32 ma_cli_try_parse_rate(ma_cli_args* pArgs, ma_uint32* pSampleRate)
{
    if (ma_cli_args_equal(pArgs, "-r") || ma_cli_args_equal(pArgs, "--rate")) {
        if (ma_cli_args_next(pArgs)) {
            char temp[64];
            ma_strncpy_s(temp, sizeof(temp), pArgs->pToken, pArgs->tokenLen);

            *pSampleRate = atoi(temp);
        } else {
            fs_file_writef(MA_CLI_STDERR, "Expecting sample rate after \"%.*s\".\n", (int)pArgs->tokenLen, pArgs->pToken);
            return MA_FALSE;
        }
    } else {
        /* Not a format switch. This is not an error - it's just ignored. */
    }

    return MA_TRUE;
}


static ma_cli_node* ma_cli_parse_command(ma_cli_args* pArgs, ma_cli_node* pPreviousNode)
{
    ma_cli_command* pCommand = NULL;
    size_t iCommand;
    ma_cli_node_config nodeConfig;
    ma_cli_node* pNode = NULL;
    const char* pCommandName;
    size_t commandNameLen;
    ma_cli_args firstArgs = *pArgs;
    ma_cli_node_list* pInputNodes = NULL;
    ma_cli_node_list* pOutputNodes = NULL;
    ma_bool32 areArgNodesInputs = MA_TRUE;
    ma_uint32 iNode;
    const char* pEncoderFilePath = NULL;
    size_t encoderFilePathLen = 0;

    pCommandName   = pArgs->pToken;
    commandNameLen = pArgs->tokenLen;

    for (iCommand = 0; iCommand < ma_countof(pCommands); iCommand += 1) {
        if (ma_cli_args_equal(pArgs, pCommands[iCommand].pName)) {
            pCommand = &pCommands[iCommand];
        }
    }

    if (pCommand == NULL) {
        fs_file_writef(MA_CLI_STDOUT, "Unknown command: \"%.*s\".\n", (int)pArgs->tokenLen, pArgs->pToken);
        return NULL;
    }

    if (pPreviousNode != NULL) {
        pInputNodes = ma_cli_node_list_push(pInputNodes, pPreviousNode);
    }

    if (pCommand->inputCount == 0 || pCommand->outputCount > 1) {
        areArgNodesInputs = MA_FALSE;
    }


    nodeConfig = ma_cli_node_config_init_default(pCommand->pVTable);
    nodeConfig.pBackends    = g_backends;
    nodeConfig.backendCount = g_backendCount;

    /* The current argument will be sitting on the command name. Skip past it. */
    ma_cli_args_next(pArgs);

    /* Parse arguments. */
    for (;;) {
        ma_bool32 hasArgBeenParsed = MA_FALSE;

        if (ma_cli_args_at_end(pArgs) || ma_cli_args_equal(pArgs, ":") || ma_cli_args_equal(pArgs, "]")) {
            break;
        }

        if (!ma_cli_try_parse_format  (pArgs, &nodeConfig.format    )) { return NULL; }
        if (!ma_cli_try_parse_channels(pArgs, &nodeConfig.channels  )) { return NULL; }
        if (!ma_cli_try_parse_rate    (pArgs, &nodeConfig.sampleRate)) { return NULL; }

        if (pArgs->tokenLen >= 2 && pArgs->pToken[0] == '-') {
            if (pCommand->pVTable->onArg != NULL) {
                if (!pCommand->pVTable->onArg(pArgs, &nodeConfig)) {
                    fs_file_writef(MA_CLI_STDERR, "Unknown argument '%.*s' for command '%.*s'.\n", (int)pArgs->tokenLen, pArgs->pToken, (int)commandNameLen, pCommandName);
                    return NULL;
                }
            }
        } else {
            /* It's not a conventional `--arg` or `-arg` style argument. */
            if (ma_cli_args_equal(pArgs, "[")) {
                /*
                It's either an input node or an output node. When it's an input node we want to parse
                it here so that this node can possibly infer it's format/channels/rate. If it's an
                output node, it needs to be parsed in a second pass so that they can infer this nodes
                format/channels/rate, which is only fully known after the node has been initialized.
                */
                if (areArgNodesInputs) {
                    if (ma_cli_args_next(pArgs)) {
                        ma_node* pInputNode = ma_cli_parse_command(pArgs, NULL);
                        if (pInputNode == NULL) {
                            return NULL;    /* ma_cli_parse_command() will have posted any error messages. */
                        }

                        pInputNodes = ma_cli_node_list_push(pInputNodes, pInputNode);
                    } else {
                        fs_file_writef(MA_CLI_STDERR, "Expecting command after \"[\".\n");
                        return NULL;
                    }
                } else {
                    /* It's an output node. This needs to be parsed in a second pass. Just skip over this one for now. */
                    int depth = 0;

                    for (;;) {
                        if (ma_cli_args_at_end(pArgs) || ma_cli_args_equal(pArgs, ":") || ma_cli_args_equal(pArgs, "]")) {
                            break;
                        }

                        /*  */ if (ma_cli_args_equal(pArgs, "[")) {
                            depth += 1;
                        } else if (ma_cli_args_equal(pArgs, "]")) {
                            depth -= 1;
                        } else {
                            /* It's an argument inside []. Skip it for now. */
                        }

                        if (depth == 0) {
                            break;
                        }

                        ma_cli_args_next(pArgs);
                    }
                }
            } else {
                /*
                Could be a file path. If the node if input-only we wire up a decode node. If it's an
                output-only node, we wire up a decode node.
                */
                
                /* Only encode and decode nodes have special handling of file paths. */
                if (pCommand->pVTable == &ma_gNodeVTable_Decode || pCommand->pVTable == &ma_gNodeVTable_Encode) {
                    nodeConfig.pFilePath   = pArgs->pToken;
                    nodeConfig.filePathLen = pArgs->tokenLen;
                } else {
                    if (pCommand->outputCount == 0) {
                        /* Need to wire up a decoder as an input node. */
                        ma_cli_node* pDecodeNode = NULL;
                        ma_cli_node_config decodeConfig;

                        decodeConfig = ma_cli_node_config_init_default(&ma_gNodeVTable_Decode);
                        decodeConfig.pFilePath   = pArgs->pToken;
                        decodeConfig.filePathLen = pArgs->tokenLen;

                        pDecodeNode = ma_cli_create_node(&decodeConfig);
                        if (pDecodeNode == NULL) {
                            fs_file_writef(MA_CLI_STDERR, "Failed to create decoder for '%.*s'.\n", (int)decodeConfig.filePathLen, decodeConfig.pFilePath);
                            return NULL;
                        }

                        pInputNodes = ma_cli_node_list_push(pInputNodes, pDecodeNode);
                        if (pInputNodes == NULL) {
                            fs_file_writef(MA_CLI_STDERR, "Failed to push child node.\n");
                            return NULL;
                        }
                    } else if (pCommand->inputCount == 0) {
                        /*
                        Need to wire up an encoder as an output node, but it needs to be delay till after
                        this node has been initialized.
                        */
                        pEncoderFilePath   = pArgs->pToken;
                        encoderFilePathLen = pArgs->tokenLen;
                    }
                }
            }
        }

        ma_cli_args_next(pArgs);
    }

    /* If the node takes inputs, infer the format/channels/rate from the first input, if there is one. */
    if (pInputNodes != NULL && pInputNodes->count > 0) {
        /* TODO: Format. This requires support from the node graph itself. */

        if (nodeConfig.channels == 0) {
            nodeConfig.channels = ma_cli_node_get_output_channels(pInputNodes->items[0], 0);
        }

        if (nodeConfig.sampleRate == 0) {
            nodeConfig.sampleRate = ma_cli_node_get_output_sample_rate(pInputNodes->items[0]);
        }
    }

    /* We should now have enough information to create the node. Inputs and outputs will be attached next. */
    pNode = ma_cli_create_node(&nodeConfig);
    if (pNode == NULL) {
        fs_file_writef(MA_CLI_STDERR, "Failed to create '%.*s' node.\n", (int)commandNameLen, pCommandName);
        return NULL;
    }

    /* We now need to parse parameters for output nodes (those in square brackets). */
    if (!areArgNodesInputs) {
        *pArgs = firstArgs;

        for (;;) {
            if (ma_cli_args_at_end(pArgs) || ma_cli_args_equal(pArgs, ":") || ma_cli_args_equal(pArgs, "]")) {
                break;
            }

            if (ma_cli_args_equal(pArgs, "[")) {
                if (ma_cli_args_next(pArgs)) {
                    ma_node* pOutputNode = ma_cli_parse_command(pArgs, pNode);

                    /* TODO: Do something with the output node? */
                    (void)pOutputNode;
                } else {
                    fs_file_writef(MA_CLI_STDERR, "Expecting command after \"[\".\n");
                    return NULL;
                }
            }

            ma_cli_args_next(pArgs);
        }
    }

    /* Wire up all the inputs. */
    if (pInputNodes != NULL) {
        for (iNode = 0; iNode < pInputNodes->count; iNode += 1) {
            ma_cli_node* pInputNode = pInputNodes->items[iNode];
            ma_uint32 outputChannels;
            ma_uint32 outputSampleRate;
            ma_uint32 inputChannels;
            ma_uint32 inputSampleRate;

            outputChannels   = ma_cli_node_get_output_channels(pInputNode, 0);
            outputSampleRate = ma_cli_node_get_output_sample_rate(pInputNode);
            inputChannels    = ma_cli_node_get_input_channels(pNode, 0);
            inputSampleRate  = ma_cli_node_get_output_sample_rate(pNode);   /* *Output* sample rate is correct here. */

            if (outputChannels != inputChannels || outputSampleRate != inputSampleRate) {
                /* Converter necessary. */
                ma_cli_converter_config converterConfig;
                ma_cli_node* pConverterNode;

                converterConfig = ma_cli_converter_config_init(inputChannels, outputChannels, inputSampleRate, outputSampleRate);
                
                pConverterNode = ma_cli_create_node(&converterConfig);
                if (pConverterNode == NULL) {
                    fs_file_writef(MA_CLI_STDERR, "Failed to create converter.\n");
                    return NULL;
                }

                /* Now the converter needs to be wired up and swapped out in the child list. */
                ma_cli_node_attach_output(pInputNode, 0, pConverterNode, 0);

                /* Swap the child for the converter in the child list. This ensures the converter node is attached as the input to the main node. */
                pInputNodes->items[iNode] = pConverterNode;
                pInputNode = pInputNodes->items[iNode];
            } else {
                /* Converter not necessary. */
            }

            ma_cli_node_attach_output(pInputNode, 0, pNode, 0);
        }
    }

    /*
    If we need an encoder, wire that up now. The encoder node will become the return value, and it
    will not allow outputs beyond it.
    */
    if (pEncoderFilePath != NULL) {
        ma_cli_node_config encodeConfig;
        ma_node* pEncodeNode;

        encodeConfig = ma_cli_node_config_init_default(&ma_gNodeVTable_Encode);
        encodeConfig.pFilePath   = pEncoderFilePath;
        encodeConfig.filePathLen = encoderFilePathLen;

        pEncodeNode = ma_cli_create_node(&encodeConfig);
        if (pEncodeNode != NULL) {
            fs_file_writef(MA_CLI_STDERR, "Failed to create encoder for '%.*s'.\n", (int)encoderFilePathLen, pEncoderFilePath);
            return NULL;    /* Failed to create encode node. */
        }

        ma_cli_node_attach_output(pNode, 0, pEncodeNode, 0);

        pNode = pEncodeNode;
    }

    return pNode;
}


#if defined(_WIN32)
/* TODO: Win32 SIGINT handler. */
static volatile LONG g_shouldQuit = 0;

static BOOL WINAPI ma_cli_handle_sigint(DWORD type)
{
    switch (type)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        {
            InterlockedExchange(&g_shouldQuit, 1);
            return TRUE;
        }

        default: return FALSE;
    }
}

static ma_bool32 ma_cli_wants_to_quit(void)
{
    return InterlockedCompareExchange(&g_shouldQuit, 0, 0) != 0;
}
#else
static volatile sig_atomic_t g_shouldQuit = 0;

static void ma_cli_handle_sigint(int signum)
{
    (void)signum;
    g_shouldQuit = 1;
}

static ma_bool32 ma_cli_wants_to_quit(void)
{
    return g_shouldQuit != 0;
}
#endif


static void ma_cli_process_node_graph(ma_node_graph* pNodeGraph)
{
    float temp[4096];   /* TODO: Make this a heap allocation for robustness. */
    ma_uint32 frameCount = ma_node_graph_get_processing_size_in_frames(pNodeGraph);
    ma_uint64 framesRead;
    ma_result result;

    MA_ASSERT(frameCount <= ma_countof(temp)/ma_node_graph_get_channels(pNodeGraph));
    
    result = ma_node_graph_read_pcm_frames(pNodeGraph, temp, frameCount, &framesRead);
    if (result != MA_SUCCESS || framesRead == 0) {
        g_shouldQuit = 1;
    }
}


int main(int argc, char** argv)
{
    ma_result result;
    int iarg;
    int iFirstNodeArg;
    ma_cli_args args;
    ma_cli_node* pLastNode = NULL;
    ma_uint32 iNode;
    ma_uint32 iPlayNode;
    ma_uint32 iRecordNode;
    ma_cli_node* pEndpointNode = NULL;
    ma_node_graph_config nodeGraphConfig;
    ma_node_graph nodeGraph;

    /*
    We want to handle SIGINT so we can do cleanup and close encoders. This is relevant for when
    one of the input sources is a microphone which has no end point.
    */
    #if defined(_WIN32)
    {
        if (!SetConsoleCtrlHandler(ma_cli_handle_sigint, TRUE)) {
            printf("SetConsoleCtrlHandler() failed.");
            return 1;
        }
    }
    #else
    {
        struct sigaction sa;

        sigemptyset(&sa.sa_mask);
        sa.sa_handler = ma_cli_handle_sigint;
        sa.sa_flags   = 0;

        if (sigaction(SIGINT, &sa, NULL) != 0) {
            printf("sigaction() failed.");
            return 1;
        }
    }
    #endif

    fs_file_open(NULL, FS_STDIN,  FS_READ,  &MA_CLI_STDIN);
    fs_file_open(NULL, FS_STDOUT, FS_WRITE, &MA_CLI_STDOUT);
    fs_file_open(NULL, FS_STDOUT, FS_WRITE, &MA_CLI_STDERR);

    if (argc < 2) {
        ma_cli_help_overview();
        return 1;
    }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        ma_cli_help(argc, argv);
        return 0;
    }

    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        ma_cli_version();
        return 0;
    }

    /* Parse global options before building the command graph. */
    iFirstNodeArg = 1;

    for (iarg = 1; iarg < argc; iarg += 1) {
        if (strcmp(argv[iarg], "--backend") == 0) {
            iarg += 1;
            if (iarg >= argc) {
                fs_file_writef(MA_CLI_STDERR, "Expecting backend list after \"--backend\".\n");
                return 1;
            }

            if (!ma_cli_parse_backends(argv[iarg])) {
                return 1;
            }

            iFirstNodeArg = iarg + 1;   /* +1 because the first argument is "--backend" and the second is the backend list. */
        }
    }

    /* Build the graph from the arguments. */
    for (args = ma_cli_args_first(argc - iFirstNodeArg, argv + iFirstNodeArg); !ma_cli_args_at_end(&args); ma_cli_args_next(&args)) {
        ma_cli_node* pNode = NULL;

        pNode = ma_cli_parse_command(&args, pLastNode);
        if (pNode == NULL) {
            return 1;   /* Parsing failed. A relevant message will have already been printed so no need to print a message here. */
        }

        /* The returned node needs to become the input of the next node. */
        pLastNode = pNode;

        /*printf("arg: %.*s\n", (int)args.tokenLen, args.pToken);*/
    }

    /*
    In order for our nodes to actually get processed we need to make sure everything is connected to the
    endpoint. To do this we can just create a single endpoint node, with one input for each unique channel
    count. We just look at nodes that do not have their outputs connected to anything.
    */
    {
        ma_cli_endpoint_config endpointConfig;
        ma_uint32 inputChannels[MA_MAX_NODE_INPUT_COUNT];
        ma_uint32 iInputChannels;
        ma_uint32 iOutput;

        MA_ZERO_MEMORY(inputChannels, sizeof(inputChannels));

        endpointConfig = ma_cli_endpoint_config_init();
        endpointConfig.inputCount          = 0; /* Will be updated below. */
        endpointConfig.pInputChannelCounts = inputChannels;

        /* Determine the unique channel counts and input count. */
        for (iNode = 0; iNode < ma_cli_get_node_count(); iNode += 1) {
            ma_cli_node* pNode = ma_cli_get_node(iNode);
            
            for (iOutput = 0; iOutput < ma_cli_node_get_output_count(pNode); iOutput += 1) {
                if (pNode->pOutputs[iOutput].pInputNode == NULL) {
                    ma_uint32 channels = ma_cli_node_get_output_channels(pNode, iOutput);

                    for (iInputChannels = 0; iInputChannels < endpointConfig.inputCount; iInputChannels += 1) {
                        if (inputChannels[iInputChannels] == channels) {
                            break;
                        }
                    }

                    if (iInputChannels == endpointConfig.inputCount) {
                        inputChannels[iInputChannels] = channels;
                        endpointConfig.inputCount += 1;
                    }
                }
            }
        }

        pEndpointNode = ma_cli_create_node(&endpointConfig);
        if (pEndpointNode == NULL) {
            fs_file_writef(MA_CLI_STDERR, "Failed to create endpoint node.\n");
            return 1;
        }

        /* Wire up the inputs. */
        for (iNode = 0; iNode < ma_cli_get_node_count(); iNode += 1) {
            ma_cli_node* pNode = ma_cli_get_node(iNode);
            if (pNode != pEndpointNode) {
                for (iOutput = 0; iOutput < ma_cli_node_get_output_count(pNode); iOutput += 1) {
                    if (pNode->pOutputs[iOutput].pInputNode == NULL) {
                        ma_uint32 channels = ma_cli_node_get_output_channels(pNode, iOutput);

                        for (iInputChannels = 0; iInputChannels < endpointConfig.inputCount; iInputChannels += 1) {
                            if (inputChannels[iInputChannels] == channels) {
                                break;
                            }
                        }

                        if (iInputChannels > ma_cli_node_get_input_count(pEndpointNode)) {
                            fs_file_writef(MA_CLI_STDERR, "Could not attach node to endpoint.\n");
                            return 1;
                        }

                        ma_cli_node_attach_output(pNode, iOutput, pEndpointNode, iInputChannels);
                    }
                }
            }
        }
    }

    /* Now we need to build the miniaudio node graph. First we need a node graph. */
    nodeGraphConfig = ma_node_graph_config_init(ma_cli_node_get_output_channels(pEndpointNode, 0));
    if (ma_cli_get_play_node_count() > 0) {
        nodeGraphConfig.processingSizeInFrames = ma_device_get_period_size_in_frames(ma_cli_play_get_device(ma_cli_get_play_node(0)));
    } else {
        nodeGraphConfig.processingSizeInFrames = MA_CLI_DEFAULT_PERIOD_SIZE_IN_FRAMES;
    }

    result = ma_node_graph_init(&nodeGraphConfig, NULL, &nodeGraph);
    if (result != MA_SUCCESS) {
        fs_file_writef(MA_CLI_STDERR, "Failed to initialize node graph.\n");
        return 1;
    }

    /* Now that we have the graph we can initialize the base nodes. */
    for (iNode = 0; iNode < ma_cli_get_node_count(); iNode += 1) {
        result = ma_cli_node_init_base_node(ma_cli_get_node(iNode), &nodeGraph);
        if (result != MA_SUCCESS) {
            fs_file_writef(MA_CLI_STDERR, "Failed to initialize base node.\n");
            return 1;
        }
    }

    /* Finally we can wire up the inputs and outputs. After this the node graph has been set up and we can start processing. */
    for (iNode = 0; iNode < ma_cli_get_node_count(); iNode += 1) {
        ma_uint32 iOutput;
        ma_cli_node* pNode;
        
        pNode = ma_cli_get_node(iNode);
        MA_ASSERT(pNode != NULL);

        for (iOutput = 0; iOutput < ma_cli_node_get_output_count(pNode); iOutput += 1) {
            if (pNode->pOutputs[iOutput].pInputNode != NULL) { /* <-- Can be null for the endpoint. Not an error. */
                ma_node_attach_output_bus(&pNode->baseNode, iOutput, &pNode->pOutputs[iOutput].pInputNode->baseNode, pNode->pOutputs[iOutput].inputNodeInputIndex);
            }
        }
    }

    /* Our endpoint needs to be attached to the miniaudio endpoint. */
    ma_node_attach_output_bus(&pEndpointNode->baseNode, 0, ma_node_graph_get_endpoint(&nodeGraph), 0);


    /* Now we need to process. Any playback and capture devices need to be started. */
    for (iRecordNode = 0; iRecordNode < ma_cli_get_record_node_count(); iRecordNode += 1) {
        ma_cli_record* pRecord = ma_cli_get_record_node(iRecordNode);
        MA_ASSERT(pRecord != NULL);

        result = ma_device_start(ma_cli_record_get_device(pRecord));
        if (result != MA_SUCCESS) {
            fs_file_writef(MA_CLI_STDERR, "Failed to start device.\n");
            return 1;
        }
    }

    for (iPlayNode = 0; iPlayNode < ma_cli_get_play_node_count(); iPlayNode += 1) {
        ma_cli_play* pPlay = ma_cli_get_play_node(iPlayNode);
        MA_ASSERT(pPlay != NULL);

        result = ma_device_start(ma_cli_play_get_device(pPlay));
        if (result != MA_SUCCESS) {
            fs_file_writef(MA_CLI_STDERR, "Failed to start device.\n");
            return 1;
        }
    }

    /* If we have a play or record node, the first one needs to be responsible for doing the node graph processing. */
    if (ma_cli_get_record_node_count() > 0) {
        ma_cli_record_set_node_graph(ma_cli_get_record_node(0), &nodeGraph);
    } else if (ma_cli_get_play_node_count() > 0) {
        ma_cli_play_set_node_graph(ma_cli_get_play_node(0), &nodeGraph);
    }

    /* We need to process in a different way depending on whether or not we are outputting to a device. */
    while (!ma_cli_wants_to_quit()) {
        /* If we have any devices we need to step them. */
        for (iRecordNode = 0; iRecordNode < ma_cli_get_record_node_count(); iRecordNode += 1) {
            ma_cli_record* pRecord = ma_cli_get_record_node(iRecordNode);
            MA_ASSERT(pRecord != NULL);

            #if 1
            while (ma_device_step(ma_cli_record_get_device(pRecord), MA_BLOCKING_MODE_BLOCKING) == MA_SUCCESS) {
                /* If the device has been processed, get out. Otherwise keep waiting. */
                if (pRecord->pNodeGraph != NULL || pRecord->cursor > 0) {
                    break;
                }
            }
            #else
            ma_device_step(ma_cli_record_get_device(pRecord), MA_BLOCKING_MODE_BLOCKING);
            #endif
        }

        /* Process the node graph, but only if there are no play nodes. When a play node is present, node processing will be done by the first play node. */
        if (ma_cli_get_play_node_count() == 0 && ma_cli_get_record_node_count() == 0) {
            ma_cli_process_node_graph(&nodeGraph);
        }

        /* Playback devices. */
        for (iPlayNode = 0; iPlayNode < ma_cli_get_play_node_count(); iPlayNode += 1) {
            ma_cli_play* pPlay = ma_cli_get_play_node(iPlayNode);
            MA_ASSERT(pPlay != NULL);

            #if 1
            while (ma_device_step(ma_cli_play_get_device(pPlay), MA_BLOCKING_MODE_BLOCKING) == MA_SUCCESS) {
                /* If the device has been processed, get out. Otherwise keep waiting. */
                if (pPlay->pNodeGraph != NULL || pPlay->cursor > 0) {
                    break;
                }
            }
            #else
            ma_device_step(ma_cli_play_get_device(pPlay), MA_BLOCKING_MODE_BLOCKING);
            #endif
        }
    }

    /* Delete every node in the graph. Needed to ensure encoders are uninitialized in particular. */
    for (iNode = 0; iNode < ma_cli_get_node_count(); iNode += 1) {
        ma_cli_delete_node(ma_cli_get_node(iNode));
    }

    return 0;
}
