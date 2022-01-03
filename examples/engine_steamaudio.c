/*
Demonstrates integration of Steam Audio with miniaudio's engine API.

In this example a HRTF effect from Steam Audio will be applied. To do this a custom node will be
implemented which uses Steam Audio's IPLBinauralEffect and IPLHRTF objects.

By implementing this as a node, it can be plugged into any position within the graph. The output
channel count of this node is always stereo.

Steam Audio requires fixed sized processing, the size of which must be specified at initialization
time of the IPLBinauralEffect and IPLHRTF objects. This creates a problem because the node graph
will at times need to break down processing into smaller chunks for it's internal processing. The
node graph internally will read into a temporary buffer which is then mixed into the final output
buffer. This temporary buffer is allocated on the stack and is a fixed size. However, variability
comes into play because the channel count of the node is variable. It's not safe to just blindly
process the effect with the frame count specified in miniaudio's node processing callback. Doing so
results in glitching. To work around this, this example is just setting the update size to a known
value that works (256). If it's set to something too big it'll exceed miniaudio's processing size
used by the node graph. Alternatively you could use some kind of intermediary cache which
accumulates input data until enough is available and then do the processing. Ideally, Steam Audio
would support variable sized updates which would avoid this whole mess entirely.
*/
#define MINIAUDIO_IMPLEMENTATION
#include "../miniaudio.h"

#include <phonon.h> /* Steam Audio */
#include <stdint.h> /* Required for uint32_t which is used by STEAMAUDIO_VERSION. That dependency needs to be removed from Steam Audio - use IPLuint32 or "unsigned int" instead! */

#define FORMAT      ma_format_f32   /* Must be floating point. */
#define CHANNELS    2               /* Must be stereo for this example. */
#define SAMPLE_RATE 48000


static ma_result ma_result_from_IPLerror(IPLerror error)
{
    switch (error)
    {
        case IPL_STATUS_SUCCESS:     return MA_SUCCESS;
        case IPL_STATUS_OUTOFMEMORY: return MA_OUT_OF_MEMORY;
        case IPL_STATUS_INITIALIZATION:
        case IPL_STATUS_FAILURE:
        default: return MA_ERROR;
    }
}


typedef struct
{
    ma_node_config nodeConfig;
    ma_uint32 channelsIn;
    IPLAudioSettings iplAudioSettings;
    IPLContext iplContext;
    IPLHRTF iplHRTF;   /* There is one HRTF object to many binaural effect objects. */
} ma_steamaudio_binaural_node_config;

MA_API ma_steamaudio_binaural_node_config ma_steamaudio_binaural_node_config_init(ma_uint32 channelsIn, IPLAudioSettings iplAudioSettings, IPLContext iplContext, IPLHRTF iplHRTF);


typedef struct
{
    ma_node_base baseNode;
    IPLAudioSettings iplAudioSettings;
    IPLContext iplContext;
    IPLHRTF iplHRTF;
    IPLBinauralEffect iplEffect;
    ma_vec3f direction;
    float* ppBuffersIn[2];      /* Each buffer is an offset of _pHeap. */
    float* ppBuffersOut[2];     /* Each buffer is an offset of _pHeap. */
    void* _pHeap;
} ma_steamaudio_binaural_node;

MA_API ma_result ma_steamaudio_binaural_node_init(ma_node_graph* pNodeGraph, const ma_steamaudio_binaural_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_steamaudio_binaural_node* pBinauralNode);
MA_API void ma_steamaudio_binaural_node_uninit(ma_steamaudio_binaural_node* pBinauralNode, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_steamaudio_binaural_node_set_direction(ma_steamaudio_binaural_node* pBinauralNode, float x, float y, float z);


MA_API ma_steamaudio_binaural_node_config ma_steamaudio_binaural_node_config_init(ma_uint32 channelsIn, IPLAudioSettings iplAudioSettings, IPLContext iplContext, IPLHRTF iplHRTF)
{
    ma_steamaudio_binaural_node_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig       = ma_node_config_init();
    config.channelsIn       = channelsIn;
    config.iplAudioSettings = iplAudioSettings;
    config.iplContext       = iplContext;
    config.iplHRTF          = iplHRTF;

    return config;
}


static void ma_steamaudio_binaural_node_process_pcm_frames(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_steamaudio_binaural_node* pBinauralNode = (ma_steamaudio_binaural_node*)pNode;
    IPLBinauralEffectParams binauralParams;
    IPLAudioBuffer inputBufferDesc;
    IPLAudioBuffer outputBufferDesc;
    ma_uint32 totalFramesToProcess = *pFrameCountOut;
    ma_uint32 totalFramesProcessed = 0;

    binauralParams.direction.x   = pBinauralNode->direction.x;
    binauralParams.direction.y   = pBinauralNode->direction.y;
    binauralParams.direction.z   = pBinauralNode->direction.z;
    binauralParams.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
    binauralParams.spatialBlend  = 1.0f;
    binauralParams.hrtf          = pBinauralNode->iplHRTF;

    inputBufferDesc.numChannels = (IPLint32)ma_node_get_input_channels(pNode, 0);

    /* We'll run this in a loop just in case our deinterleaved buffers are too small. */
    outputBufferDesc.numSamples  = pBinauralNode->iplAudioSettings.frameSize;
    outputBufferDesc.numChannels = 2;
    outputBufferDesc.data        = pBinauralNode->ppBuffersOut;

    while (totalFramesProcessed < totalFramesToProcess) {
        ma_uint32 framesToProcessThisIteration = totalFramesToProcess - totalFramesProcessed;
        if (framesToProcessThisIteration > (ma_uint32)pBinauralNode->iplAudioSettings.frameSize) {
            framesToProcessThisIteration = (ma_uint32)pBinauralNode->iplAudioSettings.frameSize;
        }

        if (inputBufferDesc.numChannels == 1) {
            /* Fast path. No need for deinterleaving since it's a mono stream. */
            pBinauralNode->ppBuffersIn[0] = (float*)ma_offset_pcm_frames_const_ptr_f32(ppFramesIn[0], totalFramesProcessed, 1);
        } else {
            /* Slow path. Need to deinterleave the input data. */
            ma_deinterleave_pcm_frames(ma_format_f32, inputBufferDesc.numChannels, framesToProcessThisIteration, ma_offset_pcm_frames_const_ptr_f32(ppFramesIn[0], totalFramesProcessed, inputBufferDesc.numChannels), pBinauralNode->ppBuffersIn);
        }

        inputBufferDesc.data       = pBinauralNode->ppBuffersIn;
        inputBufferDesc.numSamples = (IPLint32)framesToProcessThisIteration;

        /* Apply the effect. */
        iplBinauralEffectApply(pBinauralNode->iplEffect, &binauralParams, &inputBufferDesc, &outputBufferDesc);

        /* Interleave straight into the output buffer. */
        ma_interleave_pcm_frames(ma_format_f32, 2, framesToProcessThisIteration, pBinauralNode->ppBuffersOut, ma_offset_pcm_frames_ptr_f32(ppFramesOut[0], totalFramesProcessed, 2));

        /* Advance. */
        totalFramesProcessed += framesToProcessThisIteration;
    }

    (void)pFrameCountIn;    /* Unused. */
}

static ma_node_vtable g_ma_steamaudio_binaural_node_vtable =
{
    ma_steamaudio_binaural_node_process_pcm_frames,
    NULL,
    1,  /* 1 input channel. */
    1,  /* 1 output channel. */
    0
};

MA_API ma_result ma_steamaudio_binaural_node_init(ma_node_graph* pNodeGraph, const ma_steamaudio_binaural_node_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_steamaudio_binaural_node* pBinauralNode)
{
    ma_result result;
    ma_node_config baseConfig;
    ma_uint32 channelsIn;
    ma_uint32 channelsOut;
    IPLBinauralEffectSettings iplBinauralEffectSettings;
    size_t heapSizeInBytes;

    if (pBinauralNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pBinauralNode);

    if (pConfig == NULL || pConfig->iplAudioSettings.frameSize == 0 || pConfig->iplContext == NULL || pConfig->iplHRTF == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Steam Audio only supports mono and stereo input. */
    if (pConfig->channelsIn < 1 || pConfig->channelsIn > 2) {
        return MA_INVALID_ARGS;
    }

    channelsIn  = pConfig->channelsIn;
    channelsOut = 2;    /* Always stereo output. */

    baseConfig = ma_node_config_init();
    baseConfig.vtable          = &g_ma_steamaudio_binaural_node_vtable;
    baseConfig.pInputChannels  = &channelsIn;
    baseConfig.pOutputChannels = &channelsOut;
    result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &pBinauralNode->baseNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    pBinauralNode->iplAudioSettings = pConfig->iplAudioSettings;
    pBinauralNode->iplContext       = pConfig->iplContext;
    pBinauralNode->iplHRTF          = pConfig->iplHRTF;

    MA_ZERO_OBJECT(&iplBinauralEffectSettings);
    iplBinauralEffectSettings.hrtf = pBinauralNode->iplHRTF;

    result = ma_result_from_IPLerror(iplBinauralEffectCreate(pBinauralNode->iplContext, &pBinauralNode->iplAudioSettings, &iplBinauralEffectSettings, &pBinauralNode->iplEffect));
    if (result != MA_SUCCESS) {
        ma_node_uninit(&pBinauralNode->baseNode, pAllocationCallbacks);
        return result;
    }

    heapSizeInBytes = 0;

    /*
    Unfortunately Steam Audio uses deinterleaved buffers for everything so we'll need to use some
    intermediary buffers. We'll allocate one big buffer on the heap and then use offsets. We'll
    use the frame size from the IPLAudioSettings structure as a basis for the size of the buffer.
    */
    heapSizeInBytes += sizeof(float) * channelsOut * pBinauralNode->iplAudioSettings.frameSize; /* Output buffer. */
    heapSizeInBytes += sizeof(float) * channelsIn  * pBinauralNode->iplAudioSettings.frameSize; /* Input buffer. */

    pBinauralNode->_pHeap = ma_malloc(heapSizeInBytes, pAllocationCallbacks);
    if (pBinauralNode->_pHeap == NULL) {
        iplBinauralEffectRelease(&pBinauralNode->iplEffect);
        ma_node_uninit(&pBinauralNode->baseNode, pAllocationCallbacks);
        return MA_OUT_OF_MEMORY;
    }

    pBinauralNode->ppBuffersOut[0] = (float*)pBinauralNode->_pHeap;
    pBinauralNode->ppBuffersOut[1] = (float*)ma_offset_ptr(pBinauralNode->_pHeap, sizeof(float) * pBinauralNode->iplAudioSettings.frameSize);

    {
        ma_uint32 iChannelIn;
        for (iChannelIn = 0; iChannelIn < channelsIn; iChannelIn += 1) {
            pBinauralNode->ppBuffersIn[iChannelIn] = (float*)ma_offset_ptr(pBinauralNode->_pHeap, sizeof(float) * pBinauralNode->iplAudioSettings.frameSize * (channelsOut + iChannelIn));
        }
    }

    return MA_SUCCESS;
}

MA_API void ma_steamaudio_binaural_node_uninit(ma_steamaudio_binaural_node* pBinauralNode, const ma_allocation_callbacks* pAllocationCallbacks)
{
    if (pBinauralNode == NULL) {
        return;
    }

    /* The base node is always uninitialized first. */
    ma_node_uninit(&pBinauralNode->baseNode, pAllocationCallbacks);

    /*
    The Steam Audio objects are deleted after the base node. This ensures the base node is removed from the graph
    first to ensure these objects aren't getting used by the audio thread.
    */
    iplBinauralEffectRelease(&pBinauralNode->iplEffect);
    ma_free(pBinauralNode->_pHeap, pAllocationCallbacks);
}

MA_API ma_result ma_steamaudio_binaural_node_set_direction(ma_steamaudio_binaural_node* pBinauralNode, float x, float y, float z)
{
    if (pBinauralNode == NULL) {
        return MA_INVALID_ARGS;
    }

    pBinauralNode->direction.x = x;
    pBinauralNode->direction.y = y;
    pBinauralNode->direction.z = z;

    return MA_SUCCESS;
}




static ma_engine g_engine;
static ma_sound g_sound;            /* This example will play only a single sound at once, so we only need one `ma_sound` object. */
static ma_steamaudio_binaural_node g_binauralNode;   /* The echo effect is achieved using a delay node. */

int main(int argc, char** argv)
{
    ma_result result;
    ma_engine_config engineConfig;
    IPLAudioSettings iplAudioSettings;
    IPLContextSettings iplContextSettings;
    IPLContext iplContext;
    IPLHRTFSettings iplHRTFSettings;
    IPLHRTF iplHRTF;

    if (argc < 2) {
        printf("No input file.");
        return -1;
    }

    /* The engine needs to be initialized first. */
    engineConfig = ma_engine_config_init();
    engineConfig.channels           = CHANNELS;
    engineConfig.sampleRate         = SAMPLE_RATE;
    engineConfig.periodSizeInFrames = 256;

    result = ma_engine_init(&engineConfig, &g_engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio engine.");
        return -1;
    }

    /*
    Now that we have the engine we can initialize the Steam Audio objects.
    */
    MA_ZERO_OBJECT(&iplAudioSettings);
    iplAudioSettings.samplingRate = ma_engine_get_sample_rate(&g_engine);

    /*
    If there's any Steam Audio developers reading this, why is the frame size needed? This needs to
    be documented. If this is for some kind of buffer management with FFT or something, then this
    need not be exposed to the public API. There should be no need for the public API to require a
    fixed sized update.
    */
    iplAudioSettings.frameSize = engineConfig.periodSizeInFrames;


    /* IPLContext */
    MA_ZERO_OBJECT(&iplContextSettings);
    iplContextSettings.version = STEAMAUDIO_VERSION;
    
    result = ma_result_from_IPLerror(iplContextCreate(&iplContextSettings, &iplContext));
    if (result != MA_SUCCESS) {
        ma_engine_uninit(&g_engine);
        return result;
    }


    /* IPLHRTF */
    MA_ZERO_OBJECT(&iplHRTFSettings);
    iplHRTFSettings.type = IPL_HRTFTYPE_DEFAULT;

    result = ma_result_from_IPLerror(iplHRTFCreate(iplContext, &iplAudioSettings, &iplHRTFSettings, &iplHRTF));
    if (result != MA_SUCCESS) {
        iplContextRelease(&iplContext);
        ma_engine_uninit(&g_engine);
        return result;
    }


    /*
    The binaural node will need to know the input channel count of the sound so we'll need to load
    the sound first. We'll initialize this such that it'll be initially detached from the graph.
    It will be attached to the graph after the binaural node is initialized.
    */
    {
        ma_sound_config soundConfig;

        soundConfig = ma_sound_config_init();
        soundConfig.pFilePath   = argv[1];
        soundConfig.flags       = MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;  /* We'll attach this to the graph later. */

        result = ma_sound_init_ex(&g_engine, &soundConfig, &g_sound);
        if (result != MA_SUCCESS) {
            return result;
        }

        /* We'll let the Steam Audio binaural effect do the directional attenuation for us. */
        ma_sound_set_directional_attenuation_factor(&g_sound, 0);

        /* Loop the sound so we can get a continuous sound. */
        ma_sound_set_looping(&g_sound, MA_TRUE);
    }


    /*
    We'll build our graph starting from the end so initialize the binaural node now. The output of
    this node will be connected straight to the output. You could also attach it to a sound group
    or any other node that accepts an input.

    Creating a node requires a pointer to the node graph that owns it. The engine itself is a node
    graph. In the code below we can get a pointer to the node graph with `ma_engine_get_node_graph()`
    or we could simple cast the engine to a ma_node_graph* like so:
    
        (ma_node_graph*)&g_engine

    The endpoint of the graph can be retrieved with `ma_engine_get_endpoint()`.
    */
    {
        ma_steamaudio_binaural_node_config binauralNodeConfig;

        /*
        For this example we're just using the engine's channel count, but a more optimal solution
        might be to set this to mono if the source data is also mono.
        */
        binauralNodeConfig = ma_steamaudio_binaural_node_config_init(CHANNELS, iplAudioSettings, iplContext, iplHRTF);

        result = ma_steamaudio_binaural_node_init(ma_engine_get_node_graph(&g_engine), &binauralNodeConfig, NULL, &g_binauralNode);
        if (result != MA_SUCCESS) {
            printf("Failed to initialize binaural node.");
            return -1;
        }

        /* Connect the output of the delay node to the input of the endpoint. */
        ma_node_attach_output_bus(&g_binauralNode, 0, ma_engine_get_endpoint(&g_engine), 0);
    }


    /* We can now wire up the sound to the binaural node and start it. */
    ma_node_attach_output_bus(&g_sound, 0, &g_binauralNode, 0);
    ma_sound_start(&g_sound);

#if 1
    {
        /*
        We'll move the sound around the listener which we'll leave at the origin. We'll then get
        the direction to the listener and update the binaural node appropriately.
        */
        float stepAngle = 0.002f;
        float angle = 0;
        float distance = 2;

        for (;;) {
            double x = ma_cosd(angle) - ma_sind(angle);
            double y = ma_sind(angle) + ma_cosd(angle);
            ma_vec3f direction;

            ma_sound_set_position(&g_sound, (float)x * distance, 0, (float)y * distance);
            direction = ma_sound_get_direction_to_listener(&g_sound);

            /* Update the direction of the sound. */
            ma_steamaudio_binaural_node_set_direction(&g_binauralNode, direction.x, direction.y, direction.z);
            angle += stepAngle;

            ma_sleep(1);
        }
    }
#else
    printf("Press Enter to quit...");
    getchar();
#endif

    ma_sound_uninit(&g_sound);
    ma_steamaudio_binaural_node_uninit(&g_binauralNode, NULL);
    ma_engine_uninit(&g_engine);

    return 0;
}