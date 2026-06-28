/*
NOTE:

This file is temporary and will eventually be integrated into miniaudio_cli.c pending some changes to the
node graph system in miniaudio.
*/

typedef struct ma_cli_play_config ma_cli_play_config;
struct ma_cli_play_config
{
    ma_uint32 periodSizeInFrames;
};

typedef struct ma_cli_record_config ma_cli_record_config;
struct ma_cli_record_config
{
    ma_uint32 periodSizeInFrames;
};

typedef struct ma_cli_decode_config ma_cli_decode_config;
struct ma_cli_decode_config
{
    int _unused;
};

typedef struct ma_cli_encode_config ma_cli_encode_config;
struct ma_cli_encode_config
{
    int _unused;
};

typedef struct ma_cli_waveform_config ma_cli_waveform_config;
struct ma_cli_waveform_config
{
    ma_waveform_type type;
    double amplitude;
    double frequency;
    const ma_allocation_callbacks* pAllocationCallbacks;
};



/* BEG miniaudio_cli_node.h */
typedef struct ma_cli_node_vtable ma_cli_node_vtable;
typedef struct ma_cli_node_info   ma_cli_node_info;
typedef struct ma_cli_node_config ma_cli_node_config;
typedef struct ma_cli_node        ma_cli_node;


struct ma_cli_node_info
{
    const char* pName;
};

struct ma_cli_node_vtable
{
    void      (* onInfo         )(ma_cli_node_info* pInfo);
    void      (* onDefaultConfig)(ma_cli_node_config* pConfig);
    ma_bool32 (* onArg          )(ma_cli_args* pArgs, ma_cli_node_config* pConfig);
    size_t    (* onSizeof       )(const void* pConfig);
    ma_result (* onInit         )(const void* pConfig, ma_cli_node* pNode);
    void      (* onUninit       )(ma_cli_node* pNode);
    void      (* onProcess      )(ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut);
};

typedef struct ma_cli_node_output ma_cli_node_output;
struct ma_cli_node_output
{
    /* Immutable. */
    ma_cli_node* pNode;                                     /* The node that owns this output bus. The input node. Will be null for dummy head and tail nodes. */
    ma_uint8 outputIndex;                                   /* The index of the output bus on pNode that this output bus represents. */
    ma_uint8 channels;                                      /* The number of channels in the audio stream for this bus. */

    /* Mutable via multiple threads. Must be used atomically. The weird ordering here is for packing reasons. */
    ma_uint8 inputNodeInputIndex;                           /* The index of the input bus on the input. Required for detaching. Will only be used within the spinlock so does not need to be atomic. */
    MA_ATOMIC(4, ma_uint32) refCount;                       /* Reference count for some thread-safety when detaching. */
    MA_ATOMIC(4, ma_bool32) isAttached;                     /* This is used to prevent iteration of nodes that are in the middle of being detached. Used for thread safety. */
    MA_ATOMIC(4, ma_spinlock) lock;                         /* Unfortunate lock, but significantly simplifies the implementation. Required for thread-safe attaching and detaching. */
    MA_ATOMIC(MA_SIZEOF_PTR, ma_cli_node_output*) pNext;    /* If null, it's the tail node or detached. */
    MA_ATOMIC(MA_SIZEOF_PTR, ma_cli_node_output*) pPrev;    /* If null, it's the head node or detached. */
    MA_ATOMIC(MA_SIZEOF_PTR, ma_cli_node*) pInputNode;      /* The node that this output bus is attached to. Required for detaching. */
};

typedef struct ma_cli_node_input ma_cli_node_input;
struct ma_cli_node_input
{
    ma_cli_node_output head;                /* Dummy head node for simplifying some lock-free thread-safety stuff. */
    MA_ATOMIC(4, ma_uint32) nextCounter;    /* This is used to determine whether or not the input bus is finding the next node in the list. Used for thread safety when detaching output buses. */
    MA_ATOMIC(4, ma_spinlock) lock;         /* Unfortunate lock, but significantly simplifies the implementation. Required for thread-safe attaching and detaching. */
    ma_uint8 channels;                      /* The number of channels in the audio stream for this bus. */
};

struct ma_cli_node_config
{
    ma_cli_node_vtable* pVTable;
    ma_uint32 inputCount;       /* Maximum of 255. */
    ma_uint32 outputCount;      /* Maximum of 255. */
    const ma_uint32* pInputChannelCounts;
    const ma_uint32* pOutputChannelCounts;
    ma_uint32 flags;            /* A combination of MA_NODE_FLAG_*. Can be 0. */
    const ma_allocation_callbacks* pAllocationCallbacks;

    /* These should be considered output format/channels/rate. */
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;       /* Can be 0 if unknown. This is for informational purposes in case something higher level wants to query it. */

    /* Node-specific config properties. */
    ma_cli_waveform_config waveform;
    ma_cli_play_config play;
    ma_cli_record_config record;
    ma_cli_decode_config decode;

    /* File paths for decode and encode nodes. */
    const char* pFilePath;
    size_t filePathLen;

    /* Backends for play and record nodes. */
    ma_device_backend_config* pBackends;
    ma_uint32 backendCount;
};

MA_API ma_cli_node_config ma_cli_node_config_init(ma_cli_node_vtable* pVTable, ma_uint32 inputCount, ma_uint32 outputCount, const ma_uint32* pInputChannelCounts, const ma_uint32* pOutputChannelCounts);
MA_API ma_cli_node_config ma_cli_node_config_init_default(ma_cli_node_vtable* pVTable);

struct ma_cli_node
{
    ma_node_base baseNode;
    const ma_cli_node_vtable* pVTable;
    ma_allocation_callbacks allocationCallbacks;
    ma_cli_node_input* pInputs;
    ma_cli_node_output* pOutputs;
    ma_uint8 inputCount;
    ma_uint8 outputCount;
    ma_uint16 flags;
    ma_uint32 outputSampleRate; /* Can be 0 if unknown. This is for informational purposes in case something higher level wants to query it. */
};

MA_API ma_result ma_cli_node_init(const ma_cli_node_config* pNodeConfig, ma_cli_node* pNode);
MA_API void ma_cli_node_uninit(ma_cli_node* pNode);
MA_API ma_uint32 ma_cli_node_get_input_count(const ma_cli_node* pNode);
MA_API ma_uint32 ma_cli_node_get_output_count(const ma_cli_node* pNode);
MA_API ma_uint32 ma_cli_node_get_input_channels(const ma_cli_node* pNode, ma_uint32 inputIndex);
MA_API ma_uint32 ma_cli_node_get_output_channels(const ma_cli_node* pNode, ma_uint32 outputIndex);
MA_API ma_uint32 ma_cli_node_get_output_sample_rate(const ma_cli_node* pNode);
MA_API ma_result ma_cli_node_attach_output(ma_cli_node* pNode, ma_uint32 outputIndex, ma_cli_node* pOtherNode, ma_uint32 otherNodeInputIndex);
MA_API ma_result ma_cli_node_detach_output(ma_cli_node* pNode, ma_uint32 outputIndex);
MA_API ma_result ma_cli_node_detach_all_outputs(ma_cli_node* pNode);
MA_API ma_result ma_cli_node_init_base_node(ma_cli_node* pNode, ma_node_graph* pNodeGraph);
/* END miniaudio_cli_node.h */


/* BEG miniaudio_cli_node.c */
#define MA_MAX_NODE_INPUT_COUNT     255
#define MA_MAX_NODE_OUTPUT_COUNT    255

MA_API ma_cli_node_config ma_cli_node_config_init_default(ma_cli_node_vtable* pVTable)
{
    ma_cli_node_config config;

    MA_ZERO_OBJECT(&config);
    config.pVTable = pVTable;

    if (pVTable->onDefaultConfig != NULL) {
        pVTable->onDefaultConfig(&config);
    }

    return config;
}

MA_API ma_cli_node_config ma_cli_node_config_init(ma_cli_node_vtable* pVTable, ma_uint32 inputCount, ma_uint32 outputCount, const ma_uint32* pInputChannelCounts, const ma_uint32* pOutputChannelCounts)
{
    ma_cli_node_config config = ma_cli_node_config_init_default(pVTable);

    config.inputCount           = inputCount;
    config.outputCount          = outputCount;
    config.pInputChannelCounts  = pInputChannelCounts;
    config.pOutputChannelCounts = pOutputChannelCounts;

    return config;
}




static ma_result ma_cli_node_output_init(ma_cli_node* pNode, ma_uint32 outputIndex, ma_uint32 channels, ma_cli_node_output* pOutput)
{
    MA_ASSERT(pOutput != NULL);
    MA_ASSERT(outputIndex < MA_MAX_NODE_BUS_COUNT);
    MA_ASSERT(outputIndex < ma_cli_node_get_output_count(pNode));
    MA_ASSERT(channels < 256);

    MA_ZERO_OBJECT(pOutput);

    if (channels == 0) {
        return MA_INVALID_ARGS;
    }

    pOutput->pNode       = pNode;
    pOutput->outputIndex = (ma_uint8)outputIndex;
    pOutput->channels    = (ma_uint8)channels;

    return MA_SUCCESS;
}

static void ma_cli_node_output_lock(ma_cli_node_output* pOutput)
{
    ma_spinlock_lock(&pOutput->lock);
}

static void ma_cli_node_output_unlock(ma_cli_node_output* pOutput)
{
    ma_spinlock_unlock(&pOutput->lock);
}

static ma_uint32 ma_cli_node_output_get_channels(const ma_cli_node_output* pOutput)
{
    return pOutput->channels;
}

static void ma_cli_node_output_set_is_attached(ma_cli_node_output* pOutput, ma_bool32 isAttached)
{
    ma_atomic_exchange_32(&pOutput->isAttached, isAttached);
}

static ma_bool32 ma_cli_node_output_is_attached(ma_cli_node_output* pOutput)
{
    return ma_atomic_load_32(&pOutput->isAttached);
}


static ma_result ma_cli_node_input_init(ma_uint32 channels, ma_cli_node_input* pInput)
{
    MA_ASSERT(pInput != NULL);
    MA_ASSERT(channels < 256);

    MA_ZERO_OBJECT(pInput);

    if (channels == 0) {
        return MA_INVALID_ARGS;
    }

    pInput->channels = (ma_uint8)channels;

    return MA_SUCCESS;
}

static void ma_cli_node_input_lock(ma_cli_node_input* pInput)
{
    MA_ASSERT(pInput != NULL);

    ma_spinlock_lock(&pInput->lock);
}

static void ma_cli_node_input_unlock(ma_cli_node_input* pInput)
{
    MA_ASSERT(pInput != NULL);

    ma_spinlock_unlock(&pInput->lock);
}


static void ma_cli_node_input_next_begin(ma_cli_node_input* pInput)
{
    ma_atomic_fetch_add_32(&pInput->nextCounter, 1);
}

static void ma_cli_node_input_next_end(ma_cli_node_input* pInput)
{
    ma_atomic_fetch_sub_32(&pInput->nextCounter, 1);
}

static ma_uint32 ma_cli_node_input_get_next_counter(ma_cli_node_input* pInput)
{
    return ma_atomic_load_32(&pInput->nextCounter);
}


static ma_uint32 ma_cli_node_input_get_channels(const ma_cli_node_input* pInput)
{
    return pInput->channels;
}


static void ma_cli_node_input_detach__no_output_lock(ma_cli_node_input* pInput, ma_cli_node_output* pOutput)
{
    MA_ASSERT(pInput  != NULL);
    MA_ASSERT(pOutput != NULL);

    /*
    Mark the output bus as detached first. This will prevent future iterations on the audio thread
    from iterating this output bus.
    */
    ma_cli_node_output_set_is_attached(pOutput, MA_FALSE);

    /*
    We cannot use the output bus lock here since it'll be getting used at a higher level, but we do
    still need to use the input bus lock since we'll be updating pointers on two different output
    buses. The same rules apply here as the attaching case. Although we're using a lock here, we're
    *not* using a lock when iterating over the list in the audio thread. We therefore need to craft
    this in a way such that the iteration on the audio thread doesn't break.

    The first thing to do is swap out the "next" pointer of the previous output bus with the
    new "next" output bus. This is the operation that matters for iteration on the audio thread.
    After that, the previous pointer on the new "next" pointer needs to be updated, after which
    point the linked list will be in a good state.
    */
    ma_cli_node_input_lock(pInput);
    {
        ma_cli_node_output* pOldPrev = (ma_cli_node_output*)ma_atomic_load_ptr(&pOutput->pPrev);
        ma_cli_node_output* pOldNext = (ma_cli_node_output*)ma_atomic_load_ptr(&pOutput->pNext);

        if (pOldPrev != NULL) {
            ma_atomic_exchange_ptr(&pOldPrev->pNext, pOldNext); /* <-- This is where the output bus is detached from the list. */
        }
        if (pOldNext != NULL) {
            ma_atomic_exchange_ptr(&pOldNext->pPrev, pOldPrev); /* <-- This is required for detachment. */
        }
    }
    ma_cli_node_input_unlock(pInput);

    /* At this point the output bus is detached and the linked list is completely unaware of it. Reset some data for safety. */
    ma_atomic_exchange_ptr(&pOutput->pNext, NULL);   /* Using atomic exchanges here, mainly for the benefit of analysis tools which don't always recognize spinlocks. */
    ma_atomic_exchange_ptr(&pOutput->pPrev, NULL);   /* As above. */
    pOutput->pInputNode          = NULL;
    pOutput->inputNodeInputIndex = 0;


    /*
    For thread-safety reasons, we don't want to be returning from this straight away. We need to
    wait for the audio thread to finish with the output bus. There's two things we need to wait
    for. The first is the part that selects the next output bus in the list, and the other is the
    part that reads from the output bus. Basically all we're doing is waiting for the input bus
    to stop referencing the output bus.

    We're doing this part last because we want the section above to run while the audio thread
    is finishing up with the output bus, just for efficiency reasons. We marked the output bus as
    detached right at the top of this function which is going to prevent the audio thread from
    iterating the output bus again.
    */

    /* Part 1: Wait for the current iteration to complete. */
    while (ma_cli_node_input_get_next_counter(pInput) > 0) {
        ma_yield();
    }

    /* Part 2: Wait for any reads to complete. */
    while (ma_atomic_load_32(&pOutput->refCount) > 0) {
        ma_yield();
    }

    /*
    At this point we're done detaching and we can be guaranteed that the audio thread is not going
    to attempt to reference this output bus again (until attached again).
    */
}

#if 0   /* Not used at the moment, but leaving here in case I need it later. */
static void ma_cli_node_input_detach(ma_cli_node_input* pInput, ma_cli_node_output* pOutput)
{
    MA_ASSERT(pInput  != NULL);
    MA_ASSERT(pOutput != NULL);

    ma_cli_node_output_lock(pOutput);
    {
        ma_cli_node_input_detach__no_output_lock(pInput, pOutput);
    }
    ma_cli_node_output_unlock(pOutput);
}
#endif

static void ma_cli_node_input_attach(ma_cli_node_input* pInput, ma_cli_node_output* pOutput, ma_node* pNewInputNode, ma_uint32 inputNodeInputIndex)
{
    MA_ASSERT(pInput  != NULL);
    MA_ASSERT(pOutput != NULL);

    ma_cli_node_output_lock(pOutput);
    {
        ma_cli_node_output* pOldInputNode = (ma_cli_node_output*)ma_atomic_load_ptr(&pOutput->pInputNode);

        /* Detach from any existing attachment first if necessary. */
        if (pOldInputNode != NULL) {
            ma_cli_node_input_detach__no_output_lock(pInput, pOutput);
        }

        /*
        At this point we can be sure the output bus is not attached to anything. The linked list in the
        old input bus has been updated so that pOutput will not get iterated again.
        */
        pOutput->pInputNode          = pNewInputNode;                     /* No need for an atomic assignment here because modification of this variable always happens within a lock. */
        pOutput->inputNodeInputIndex = (ma_uint8)inputNodeInputIndex;

        /*
        Now we need to attach the output bus to the linked list. This involves updating two pointers on
        two different output buses so I'm going to go ahead and keep this simple and just use a lock.
        There are ways to do this without a lock, but it's just too hard to maintain for its value.

        Although we're locking here, it's important to remember that we're *not* locking when iterating
        and reading audio data since that'll be running on the audio thread. As a result we need to be
        careful how we craft this so that we don't break iteration. What we're going to do is always
        attach the new item so that it becomes the first item in the list. That way, as we're iterating
        we won't break any links in the list and iteration will continue safely. The detaching case will
        also be crafted in a way as to not break list iteration. It's important to remember to use
        atomic exchanges here since no locking is happening on the audio thread during iteration.
        */
        ma_cli_node_input_lock(pInput);
        {
            ma_cli_node_output* pNewPrev = &pInput->head;
            ma_cli_node_output* pNewNext = (ma_cli_node_output*)ma_atomic_load_ptr(&pInput->head.pNext);

            /* Update the local output bus. */
            ma_atomic_exchange_ptr(&pOutput->pPrev, pNewPrev);
            ma_atomic_exchange_ptr(&pOutput->pNext, pNewNext);

            /* Update the other output buses to point back to the local output bus. */
            ma_atomic_exchange_ptr(&pInput->head.pNext, pOutput); /* <-- This is where the output bus is actually attached to the input bus. */

            /* Do the previous pointer last. This is only used for detachment. */
            if (pNewNext != NULL) {
                ma_atomic_exchange_ptr(&pNewNext->pPrev, pOutput);
            }
        }
        ma_cli_node_input_unlock(pInput);

        /*
        Mark the node as attached last. This is used to controlling whether or the output bus will be
        iterated on the audio thread. Mainly required for detachment purposes.
        */
        ma_cli_node_output_set_is_attached(pOutput, MA_TRUE);
    }
    ma_cli_node_output_unlock(pOutput);
}

static ma_cli_node_output* ma_cli_node_input_next(ma_cli_node_input* pInput, ma_cli_node_output* pOutput)
{
    ma_cli_node_output* pNext;

    MA_ASSERT(pInput != NULL);

    if (pOutput == NULL) {
        return NULL;
    }

    ma_cli_node_input_next_begin(pInput);
    {
        pNext = pOutput;
        for (;;) {
            pNext = (ma_cli_node_output*)ma_atomic_load_ptr(&pNext->pNext);
            if (pNext == NULL) {
                break;      /* Reached the end. */
            }

            if (ma_cli_node_output_is_attached(pNext) == MA_FALSE) {
                continue;   /* The node is not attached. Keep checking. */
            }

            /* The next node has been selected. */
            break;
        }

        /* We need to increment the reference count of the selected node. */
        if (pNext != NULL) {
            ma_atomic_fetch_add_32(&pNext->refCount, 1);
        }

        /* The previous node is no longer being referenced. */
        ma_atomic_fetch_sub_32(&pOutput->refCount, 1);
    }
    ma_cli_node_input_next_end(pInput);

    return pNext;
}

static ma_cli_node_output* ma_cli_node_input_first(ma_cli_node_input* pInput)
{
    return ma_cli_node_input_next(pInput, &pInput->head);
}


static void ma_cli_node_vtable_info(const ma_cli_node_vtable* pVTable, ma_cli_node_info* pInfo)
{
    MA_ASSERT(pVTable != NULL);

    if (pVTable->onInfo == NULL) {
        return;
    }

    pVTable->onInfo(pInfo);
}

static size_t ma_cli_node_vtable_sizeof(const ma_cli_node_vtable* pVTable, const void* pConfig)
{
    MA_ASSERT(pVTable != NULL);

    if (pVTable->onSizeof == NULL) {
        return 0;
    }

    return pVTable->onSizeof(pConfig);
}

static ma_result ma_cli_node_vtable_init(const ma_cli_node_vtable* pVTable, const void* pConfig, ma_cli_node* pNode)
{
    MA_ASSERT(pVTable != NULL);

    if (pVTable->onInit == NULL) {
        return MA_NOT_IMPLEMENTED;
    }

    return pVTable->onInit(pConfig, pNode);
}

static void ma_cli_node_vtable_uninit(const ma_cli_node_vtable* pVTable, ma_cli_node* pNode)
{
    MA_ASSERT(pVTable != NULL);

    if (pVTable->onUninit == NULL) {
        return;
    }

    pVTable->onUninit(pNode);
}

void ma_cli_node_vtable_process(const ma_cli_node_vtable* pVTable, ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    MA_ASSERT(pVTable != NULL);

    if (pVTable->onProcess == NULL) {
        return;
    }

    pVTable->onProcess(pNode, ppFramesIn, pFrameCountIn, ppFramesOut, pFrameCountOut);
}


static void ma_cli_node_get_info(ma_cli_node* pNode, ma_cli_node_info* pInfo)
{
    if (pInfo == NULL) {
        return;
    }

    MA_ZERO_OBJECT(pInfo);

    if (pNode == NULL) {
        return;
    }

    ma_cli_node_vtable_info(pNode->pVTable, pInfo);
}


MA_API ma_result ma_cli_node_init(const ma_cli_node_config* pConfig, ma_cli_node* pNode)
{
    ma_result result;
    ma_uint32 iInput;
    ma_uint32 iOutput;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pNode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pConfig->inputCount > MA_MAX_NODE_INPUT_COUNT || pConfig->outputCount > MA_MAX_NODE_OUTPUT_COUNT) {
        return MA_INVALID_ARGS; /* Too many inputs or outputs. */
    }

    pNode->pVTable     =            pConfig->pVTable;
    pNode->inputCount  = (ma_uint8 )pConfig->inputCount;
    pNode->outputCount = (ma_uint8 )pConfig->outputCount;
    pNode->flags       = (ma_uint16)pConfig->flags;
    pNode->pInputs     = (ma_cli_node_input* )ma_malloc(sizeof(*pNode->pInputs ) * pNode->inputCount,  pConfig->pAllocationCallbacks);
    pNode->pOutputs    = (ma_cli_node_output*)ma_malloc(sizeof(*pNode->pOutputs) * pNode->outputCount, pConfig->pAllocationCallbacks);
    pNode->allocationCallbacks = ma_allocation_callbacks_init_copy(pConfig->pAllocationCallbacks);
    pNode->outputSampleRate = pConfig->sampleRate;

    /* We need to run an initialization step for each input and output bus. */
    for (iInput = 0; iInput < ma_cli_node_get_input_count(pNode); iInput += 1) {
        result = ma_cli_node_input_init(pConfig->pInputChannelCounts[iInput], &pNode->pInputs[iInput]);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    for (iOutput = 0; iOutput < ma_cli_node_get_output_count(pNode); iOutput += 1) {
        result = ma_cli_node_output_init(pNode, iOutput, pConfig->pOutputChannelCounts[iOutput], &pNode->pOutputs[iOutput]);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    return MA_SUCCESS;
}

MA_API void ma_cli_node_uninit(ma_cli_node* pNode)
{
    if (pNode == NULL) {
        return;
    }

    ma_free(pNode->pInputs,  &pNode->allocationCallbacks);
    ma_free(pNode->pOutputs, &pNode->allocationCallbacks);
}

MA_API ma_cli_node* ma_cli_node_create(const void* pConfig)
{
    ma_cli_node_config* pNodeConfig = (ma_cli_node_config*)pConfig; /* The first member of the config should be a ma_cli_node_config object. */
    ma_cli_node* pNode;
    ma_result result;

    if (pNodeConfig == NULL || pNodeConfig->pVTable == NULL) {
        return NULL;
    }

    pNode = (ma_cli_node*)ma_malloc(ma_cli_node_vtable_sizeof(pNodeConfig->pVTable, pConfig), pNodeConfig->pAllocationCallbacks);
    if (pNode == NULL) {
        return NULL;
    }

    result = ma_cli_node_vtable_init(pNodeConfig->pVTable, pConfig, pNode);
    if (result != MA_SUCCESS) {
        ma_free(pNode, pNodeConfig->pAllocationCallbacks);
        return NULL;
    }

    return pNode;
}

MA_API void ma_cli_node_delete(ma_cli_node* pNode)
{
    ma_allocation_callbacks allocationCallbacks;

    if (pNode == NULL) {
        return;
    }

    allocationCallbacks = pNode->allocationCallbacks;

    ma_cli_node_vtable_uninit(pNode->pVTable, pNode);
    ma_free(pNode, &allocationCallbacks);
}


MA_API ma_uint32 ma_cli_node_get_input_count(const ma_cli_node* pNode)
{
    if (pNode == NULL) {
        return 0;
    }

    return pNode->inputCount;
}

MA_API ma_uint32 ma_cli_node_get_output_count(const ma_cli_node* pNode)
{
    if (pNode == NULL) {
        return 0;
    }

    return pNode->outputCount;
}

MA_API ma_uint32 ma_cli_node_get_input_channels(const ma_cli_node* pNode, ma_uint32 inputIndex)
{
    if (pNode == NULL) {
        return 0;
    }

    if (inputIndex >= ma_cli_node_get_input_count(pNode)) {
        return 0;   /* Invalid bus index. */
    }

    return ma_cli_node_input_get_channels(&pNode->pInputs[inputIndex]);
}

MA_API ma_uint32 ma_cli_node_get_output_channels(const ma_cli_node* pNode, ma_uint32 outputIndex)
{
    if (pNode == NULL) {
        return 0;
    }

    if (outputIndex >= ma_cli_node_get_output_count(pNode)) {
        return 0;   /* Invalid bus index. */
    }

    return ma_cli_node_output_get_channels(&pNode->pOutputs[outputIndex]);
}

MA_API ma_uint32 ma_cli_node_get_output_sample_rate(const ma_cli_node* pNode)
{
    if (pNode == NULL) {
        return 0;
    }

    return pNode->outputSampleRate;
}

MA_API ma_result ma_cli_node_attach_output(ma_cli_node* pNode, ma_uint32 outputIndex, ma_cli_node* pOtherNode, ma_uint32 otherNodeInputIndex)
{
    if (pNode == NULL || pOtherNode == NULL) {
        return MA_INVALID_ARGS;
    }

    if (pNode == pOtherNode) {
        return MA_INVALID_OPERATION;    /* Cannot attach a node to itself. */
    }

    if (outputIndex >= ma_cli_node_get_output_count(pNode) || otherNodeInputIndex >= ma_cli_node_get_input_count(pOtherNode)) {
        return MA_INVALID_OPERATION;    /* Invalid bus index. */
    }

    /* The output channel count of the output node must be the same as the input channel count of the input node. */
    if (ma_cli_node_get_output_channels(pNode, outputIndex) != ma_cli_node_get_input_channels(pOtherNode, otherNodeInputIndex)) {
        return MA_INVALID_OPERATION;    /* Channel count is incompatible. */
    }

    /* This will deal with detaching if the output bus is already attached to something. */
    ma_cli_node_input_attach(&pOtherNode->pInputs[otherNodeInputIndex], &pNode->pOutputs[outputIndex], pOtherNode, otherNodeInputIndex);

    return MA_SUCCESS;
}

MA_API ma_result ma_cli_node_detach_output(ma_cli_node* pNode, ma_uint32 outputIndex)
{
    ma_result result = MA_SUCCESS;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    if (outputIndex >= ma_cli_node_get_output_count(pNode)) {
        return MA_INVALID_ARGS; /* Invalid output bus index. */
    }

    /* We need to lock the output bus because we need to inspect the input node and grab its input bus. */
    ma_cli_node_output_lock(&pNode->pOutputs[outputIndex]);
    {
        ma_cli_node* pInputNode = pNode->pOutputs[outputIndex].pInputNode;
        if (pInputNode != NULL) {
            ma_cli_node_input_detach__no_output_lock(&pInputNode->pInputs[pNode->pOutputs[outputIndex].inputNodeInputIndex], &pNode->pOutputs[outputIndex]);
        }
    }
    ma_cli_node_output_unlock(&pNode->pOutputs[outputIndex]);

    return result;
}

MA_API ma_result ma_cli_node_detach_all_outputs(ma_cli_node* pNode)
{
    ma_uint32 iOutputBus;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    for (iOutputBus = 0; iOutputBus < ma_cli_node_get_output_count(pNode); iOutputBus += 1) {
        ma_cli_node_detach_output(pNode, iOutputBus);
    }

    return MA_SUCCESS;
}



static void ma_cli_node__on_node_process(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_cli_node* pCLINode = (ma_cli_node*)pNode;
    pCLINode->pVTable->onProcess(pCLINode, ppFramesIn, pFrameCountIn, ppFramesOut, pFrameCountOut);
}

static ma_node_vtable ma_gNodeVTable_CLINode =
{
    ma_cli_node__on_node_process,
    NULL,   /* onGetRequiredInputFrameCount */
    MA_NODE_BUS_COUNT_UNKNOWN,
    MA_NODE_BUS_COUNT_UNKNOWN,
    0
};

MA_API ma_result ma_cli_node_init_base_node(ma_cli_node* pNode, ma_node_graph* pNodeGraph)
{
    ma_result result;
    ma_node_config baseNodeConfig;
    ma_uint32 inputCount;
    ma_uint32 outputCount;
    ma_uint32 inputChannelCounts[MA_MAX_NODE_INPUT_COUNT];
    ma_uint32 outputChannelCounts[MA_MAX_NODE_OUTPUT_COUNT];
    ma_uint32 iInput;
    ma_uint32 iOutput;

    if (pNode == NULL) {
        return MA_INVALID_ARGS;
    }

    inputCount = ma_cli_node_get_input_count(pNode);
    outputCount = ma_cli_node_get_output_count(pNode);

    for (iInput = 0; iInput < inputCount; iInput += 1) {
        inputChannelCounts[iInput] = ma_cli_node_get_input_channels(pNode, iInput);
    }

    for (iOutput = 0; iOutput < outputCount; iOutput += 1) {
        outputChannelCounts[iOutput] = ma_cli_node_get_output_channels(pNode, iOutput);
    }

    baseNodeConfig = ma_node_config_init();
    baseNodeConfig.pVTable         = &ma_gNodeVTable_CLINode;
    baseNodeConfig.inputBusCount   = inputCount;
    baseNodeConfig.outputBusCount  = outputCount;
    baseNodeConfig.pInputChannels  = inputChannelCounts;
    baseNodeConfig.pOutputChannels = outputChannelCounts;
    
    result = ma_node_init(pNodeGraph, &baseNodeConfig, &pNode->allocationCallbacks, &pNode->baseNode);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}
/* END miniaudio_cli_node.c */



/* BEG miniaudio_cli_waveform.h */
//MA_API ma_cli_waveform_config ma_cli_waveform_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_waveform_type type, double amplitude, double frequency);


typedef struct ma_cli_waveform ma_cli_waveform;
struct ma_cli_waveform
{
    ma_cli_node node;
    ma_waveform waveform;
};

MA_API ma_result ma_cli_waveform_init(const ma_cli_node_config* pConfig, ma_cli_waveform* pWaveform);
MA_API void ma_cli_waveform_uninit(ma_cli_waveform* pWaveform);
MA_API ma_cli_node* ma_cli_waveform_node(ma_cli_waveform* pWaveform);
MA_API ma_result ma_cli_waveform_read_pcm_frames(ma_cli_waveform* pWaveform, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
/* END miniaudio_cli_waveform.h */

/* BEG miniaudio_cli_waveform.c */
static void ma_cli_waveform__node_on_info(ma_cli_node_info* pInfo)
{
    pInfo->pName = "waveform";
}

static void ma_cli_waveform__node_on_default_config(ma_cli_node_config* pConfig)
{
    pConfig->format             = ma_format_f32;
    pConfig->channels           = 1;
    pConfig->sampleRate         = 48000;
    pConfig->waveform.type      = ma_waveform_type_sine;
    pConfig->waveform.amplitude = 0.1;
    pConfig->waveform.frequency = 220;
}

static ma_bool32 ma_cli_waveform__node_on_arg(ma_cli_args* pArgs, ma_cli_node_config* pConfig)
{
    if (ma_cli_args_equal(pArgs, "--type")) {
        if (ma_cli_args_next(pArgs)) {
            /*  */ if (ma_cli_args_equal(pArgs, "sine")) {
                pConfig->waveform.type = ma_waveform_type_sine;
            } else if (ma_cli_args_equal(pArgs, "square")) {
                pConfig->waveform.type = ma_waveform_type_square;
            } else if (ma_cli_args_equal(pArgs, "triangle")) {
                pConfig->waveform.type = ma_waveform_type_triangle;
            } else if (ma_cli_args_equal(pArgs, "sawtooth")) {
                pConfig->waveform.type = ma_waveform_type_sawtooth;
            } else {
                fs_file_writef(MA_CLI_STDERR, "Unknown waveform type \"%.*s\"", (int)pArgs->tokenLen, pArgs->pToken);
                return MA_FALSE;
            }

            return MA_TRUE;
        } else {
            fs_file_writef(MA_CLI_STDERR, "Expecting waveform type after `--type`.");
            return MA_FALSE;
        }
    }

    if (ma_cli_args_equal(pArgs, "--amplitude")) {
        if (ma_cli_args_next(pArgs)) {
            char amplitudeStr[32];

            if (fs_strncpy_s(amplitudeStr, sizeof(amplitudeStr), pArgs->pToken, pArgs->tokenLen) != 0) {
                fs_file_writef(MA_CLI_STDERR, "Invalid waveform amplitude \"%.*s\"", (int)pArgs->tokenLen, pArgs->pToken);
                return MA_FALSE;
            }

            pConfig->waveform.amplitude = atof(amplitudeStr);
            return MA_TRUE;
        } else {
            fs_file_writef(MA_CLI_STDERR, "Expecting number after `--amplitude`.");
            return MA_FALSE;
        }
    }

    if (ma_cli_args_equal(pArgs, "--frequency")) {
        if (ma_cli_args_next(pArgs)) {
            char frequencyStr[32];

            if (fs_strncpy_s(frequencyStr, sizeof(frequencyStr), pArgs->pToken, pArgs->tokenLen) != 0) {
                fs_file_writef(MA_CLI_STDERR, "Invalid waveform frequency \"%.*s\"", (int)pArgs->tokenLen, pArgs->pToken);
                return MA_FALSE;
            }

            pConfig->waveform.frequency = (ma_uint32)atoi(frequencyStr);
            return MA_TRUE;
        } else {
            fs_file_writef(MA_CLI_STDERR, "Expecting number after `--frequency`.");
            return MA_FALSE;
        }
    }

    return MA_FALSE;
}

static size_t ma_cli_waveform__node_on_sizeof(const void* pConfig)
{
    return sizeof(ma_cli_waveform);
}

static ma_result ma_cli_waveform__node_on_init(const void* pConfig, ma_cli_node* pNode)
{
    return ma_cli_waveform_init((ma_cli_node_config*)pConfig, (ma_cli_waveform*)pNode);
}

static void ma_cli_waveform__node_on_uninit(ma_cli_node* pNode)
{
    ma_cli_waveform_uninit((ma_cli_waveform*)pNode);
}

static void ma_cli_waveform__node_on_process(ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_cli_waveform* pWaveform = (ma_cli_waveform*)pNode;
    ma_uint64 frameCount;
    ma_uint64 framesRead;

    /* A waveform takes no input. */
    (void)ppFramesIn;
    (void)pFrameCountIn;

    frameCount = *pFrameCountOut;

    if (pWaveform->waveform.config.format == ma_format_f32) {
        ma_cli_waveform_read_pcm_frames(pWaveform, ppFramesOut[0], frameCount, &framesRead);
    } else {
        /* Need to convert. */
        ma_uint32 temp[1024];
        ma_uint32 tempCap = sizeof(temp) / ma_get_bytes_per_frame(pWaveform->waveform.config.format, pWaveform->waveform.config.channels);
        ma_uint64 totalFramesProcessed;
        float* pRunningFramesOut;
        
        pRunningFramesOut = ppFramesOut[0];

        totalFramesProcessed = 0;
        while (totalFramesProcessed < frameCount) {
            ma_uint64 framesRemaining = frameCount - totalFramesProcessed;
            ma_uint64 framesToReadThisIteration = framesRemaining;
            if (framesToReadThisIteration > tempCap) {
                framesToReadThisIteration = tempCap;
            }

            ma_cli_waveform_read_pcm_frames(pWaveform, temp, framesToReadThisIteration, NULL);
            ma_pcm_convert(pRunningFramesOut, ma_format_f32, temp, pWaveform->waveform.config.format, framesToReadThisIteration * pWaveform->waveform.config.channels, ma_dither_mode_none);

            totalFramesProcessed += framesToReadThisIteration;
            pRunningFramesOut    += framesToReadThisIteration * pWaveform->waveform.config.channels;
        }

        framesRead = totalFramesProcessed;
    }

    *pFrameCountOut = (ma_uint32)framesRead;
}

static ma_cli_node_vtable ma_gNodeVTable_Waveform =
{
    ma_cli_waveform__node_on_info,
    ma_cli_waveform__node_on_default_config,
    ma_cli_waveform__node_on_arg,
    ma_cli_waveform__node_on_sizeof,
    ma_cli_waveform__node_on_init,
    ma_cli_waveform__node_on_uninit,
    ma_cli_waveform__node_on_process
};


#if 0
static ma_cli_node_config ma_cli_node_config_init_waveform(const ma_uint32* pInputChannelCounts)
{
    return ma_cli_node_config_init(&ma_gNodeVTable_Waveform, 0, 1, NULL, pInputChannelCounts);
}

MA_API ma_cli_waveform_config ma_cli_waveform_config_init(ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_waveform_type type, double amplitude, double frequency)
{
    ma_cli_waveform_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_cli_node_config_init_waveform(NULL);
    config.format     = format;
    config.channels   = channels;
    config.sampleRate = sampleRate;
    config.type       = type;
    config.amplitude  = amplitude;
    config.frequency  = frequency;

    return config;
}
#endif


MA_API ma_result ma_cli_waveform_init(const ma_cli_node_config* pConfig, ma_cli_waveform* pWaveform)
{
    ma_result result;
    ma_cli_node_config nodeConfig;
    ma_waveform_config waveformConfig;

    if (pWaveform == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pWaveform);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    nodeConfig = *pConfig;
    nodeConfig.outputCount          = 1;
    nodeConfig.pOutputChannelCounts = &pConfig->channels;

    result = ma_cli_node_init(&nodeConfig, &pWaveform->node);
    if (result != MA_SUCCESS) {
        return result;
    }

    waveformConfig = ma_waveform_config_init(pConfig->format, pConfig->channels, pConfig->sampleRate, pConfig->waveform.type, pConfig->waveform.amplitude, pConfig->waveform.frequency);

    result = ma_waveform_init(&waveformConfig, &pWaveform->waveform);
    if (result != MA_SUCCESS) {
        ma_cli_node_uninit(&pWaveform->node);
        return result;
    }
    
    return MA_SUCCESS;
}

MA_API void ma_cli_waveform_uninit(ma_cli_waveform* pWaveform)
{
    ma_cli_node_uninit(&pWaveform->node);
    ma_waveform_uninit(&pWaveform->waveform);
}

MA_API ma_cli_node* ma_cli_waveform_node(ma_cli_waveform* pWaveform)
{
    return (ma_cli_node*)pWaveform;
}

MA_API ma_result ma_cli_waveform_read_pcm_frames(ma_cli_waveform* pWaveform, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pWaveform == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_waveform_read_pcm_frames(&pWaveform->waveform, pFramesOut, frameCount, pFramesRead);
}
/* END miniaudio_cli_waveform.c */


/* BEG miniaudio_cli_decode.h */
typedef struct ma_cli_decode ma_cli_decode;
struct ma_cli_decode
{
    ma_cli_node node;
    ma_decoder decoder;
};

MA_API ma_result ma_cli_decode_init(const ma_cli_node_config* pConfig, ma_cli_decode* pDecode);
MA_API void ma_cli_decode_uninit(ma_cli_decode* pDecode);
MA_API ma_cli_node* ma_cli_decode_node(ma_cli_decode* pDecode);
MA_API ma_result ma_cli_decode_read_pcm_frames(ma_cli_decode* pDecode, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
/* END miniaudio_cli_decode.h */

/* BEG miniaudio_cli_decode.c */
static void ma_cli_decode__node_on_info(ma_cli_node_info* pInfo)
{
    pInfo->pName = "decode";
}

static void ma_cli_decode__node_on_default_config(ma_cli_node_config* pConfig)
{
    (void)pConfig;
}

static ma_bool32 ma_cli_decode__node_on_arg(ma_cli_args* pArgs, ma_cli_node_config* pConfig)
{
    (void)pArgs;
    (void)pConfig;

    return MA_FALSE;
}

static size_t ma_cli_decode__node_on_sizeof(const void* pConfig)
{
    return sizeof(ma_cli_decode);
}

static ma_result ma_cli_decode__node_on_init(const void* pConfig, ma_cli_node* pNode)
{
    return ma_cli_decode_init((ma_cli_node_config*)pConfig, (ma_cli_decode*)pNode);
}

static void ma_cli_decode__node_on_uninit(ma_cli_node* pNode)
{
    ma_cli_decode_uninit((ma_cli_decode*)pNode);
}

static void ma_cli_decode__node_on_process(ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_cli_decode* pDecode = (ma_cli_decode*)pNode;
    ma_result result;
    ma_uint64 frameCount;
    ma_uint64 framesRead;

    /* A decoder takes no input. */
    (void)ppFramesIn;
    (void)pFrameCountIn;

    frameCount = *pFrameCountOut;

    if (pDecode->decoder.outputFormat == ma_format_f32) {
        ma_cli_decode_read_pcm_frames(pDecode, ppFramesOut[0], frameCount, &framesRead);
    } else {
        /* Need to convert. */
        ma_uint32 temp[1024];
        ma_uint32 tempCap = sizeof(temp) / ma_get_bytes_per_frame(pDecode->decoder.outputFormat, pDecode->decoder.outputChannels);
        ma_uint64 totalFramesProcessed;
        float* pRunningFramesOut;
        
        pRunningFramesOut = ppFramesOut[0];

        totalFramesProcessed = 0;
        while (totalFramesProcessed < frameCount) {
            ma_uint64 framesJustRead;
            ma_uint64 framesRemaining = frameCount - totalFramesProcessed;
            ma_uint64 framesToReadThisIteration = framesRemaining;
            if (framesToReadThisIteration > tempCap) {
                framesToReadThisIteration = tempCap;
            }

            result = ma_cli_decode_read_pcm_frames(pDecode, temp, framesToReadThisIteration, &framesJustRead);
            if (result != MA_SUCCESS) {
                break;
            }

            ma_pcm_convert(pRunningFramesOut, ma_format_f32, temp, pDecode->decoder.outputFormat, framesJustRead * pDecode->decoder.outputChannels, ma_dither_mode_none);

            totalFramesProcessed += framesJustRead;
            pRunningFramesOut    += framesJustRead * pDecode->decoder.outputChannels;
        }

        framesRead = totalFramesProcessed;
    }

    *pFrameCountOut = (ma_uint32)framesRead;
}

static ma_cli_node_vtable ma_gNodeVTable_Decode =
{
    ma_cli_decode__node_on_info,
    ma_cli_decode__node_on_default_config,
    ma_cli_decode__node_on_arg,
    ma_cli_decode__node_on_sizeof,
    ma_cli_decode__node_on_init,
    ma_cli_decode__node_on_uninit,
    ma_cli_decode__node_on_process
};

#if 0
static ma_cli_node_config ma_cli_node_config_init_decode(void)
{
    return ma_cli_node_config_init(&ma_gNodeVTable_Decode, 0, 0, NULL, NULL);
}

MA_API ma_cli_decode_config ma_cli_decode_config_init(void)
{
    ma_cli_decode_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_cli_node_config_init_decode();

    return config;
}
#endif


static ma_result ma_cli_decode__on_read(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file* pFile = (fs_file*)pDecoder->pUserData;
    fs_result result;

    result = fs_file_read(pFile, pBufferOut, bytesToRead, pBytesRead);
    if (result != FS_SUCCESS) {
        return MA_ERROR;
    }

    return MA_SUCCESS;
}

static ma_result ma_cli_decode__on_seek(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin)
{
    fs_file* pFile = (fs_file*)pDecoder->pUserData;
    fs_result result;

    result = fs_file_seek(pFile, byteOffset, (fs_seek_origin)origin);
    if (result != FS_SUCCESS) {
        return MA_ERROR;
    }

    return MA_SUCCESS;
}

static ma_result ma_cli_decode__on_tell(ma_decoder* pDecoder, ma_int64* pCursor)
{
    fs_file* pFile = (fs_file*)pDecoder->pUserData;
    fs_result result;

    result = fs_file_tell(pFile, pCursor);
    if (result != FS_SUCCESS) {
        return MA_ERROR;
    }

    return MA_SUCCESS;
}


MA_API ma_result ma_cli_decode_init(const ma_cli_node_config* pConfig, ma_cli_decode* pDecode)
{
    ma_result result;
    ma_cli_node_config nodeConfig;
    ma_decoder_config decoderConfig;
    ma_decoding_backend_vtable* pDecodingBackends[] =
    {
        MA_CLI_DECODING_BACKENDS
    };

    if (pDecode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pDecode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    decoderConfig = ma_decoder_config_init(pConfig->format, pConfig->channels, pConfig->sampleRate);
    decoderConfig.ppBackendVTables = pDecodingBackends;
    decoderConfig.backendCount     = ma_countof(pDecodingBackends);

    if (pConfig->pFilePath != NULL) {
        size_t filePathLen;
        char* pFilePath;

        filePathLen = pConfig->filePathLen;
        if (filePathLen == (size_t)-1) {
            filePathLen = strlen(pConfig->pFilePath);
        }

        pFilePath = (char*)ma_malloc(filePathLen + 1, pConfig->pAllocationCallbacks);
        if (pFilePath == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        ma_strcpy_s(pFilePath, filePathLen + 1, pConfig->pFilePath);
        result = ma_decoder_init_file(pFilePath, &decoderConfig, &pDecode->decoder);
        ma_free(pFilePath, pConfig->pAllocationCallbacks);
    } else {
        return MA_INVALID_ARGS;
    }

    if (result != MA_SUCCESS) {
        fs_file_writef(MA_CLI_STDERR, "ma_decoder_init() failed. result = %s\n", ma_result_description(result));
        ma_cli_node_uninit(&pDecode->node);
        return result;
    }


    nodeConfig = *pConfig;
    nodeConfig.inputCount           = 0;
    nodeConfig.outputCount          = 1;
    nodeConfig.pOutputChannelCounts = &pDecode->decoder.outputChannels;
    nodeConfig.sampleRate           = pDecode->decoder.outputSampleRate;
    nodeConfig.pAllocationCallbacks = pConfig->pAllocationCallbacks;

    result = ma_cli_node_init(&nodeConfig, &pDecode->node);
    if (result != MA_SUCCESS) {
        return result;
    }
    
    return MA_SUCCESS;
}

MA_API void ma_cli_decode_uninit(ma_cli_decode* pDecode)
{
    ma_cli_node_uninit(&pDecode->node);
    ma_decoder_uninit(&pDecode->decoder);
}

MA_API ma_cli_node* ma_cli_decode_node(ma_cli_decode* pDecode)
{
    return (ma_cli_node*)pDecode;
}

MA_API ma_result ma_cli_decode_read_pcm_frames(ma_cli_decode* pDecode, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    if (pDecode == NULL) {
        return MA_INVALID_ARGS;
    }

    return ma_decoder_read_pcm_frames(&pDecode->decoder, pFramesOut, frameCount, pFramesRead);
}
/* END miniaudio_cli_decode.c */


/* BEG miniaudio_cli_play.h */
typedef struct ma_cli_play ma_cli_play;
struct ma_cli_play
{
    ma_cli_node node;
    ma_device device;
    void* pBuffer;
    ma_uint32 bufferSizeInFrames;
    ma_uint32 cursor;
    ma_node_graph* pNodeGraph;  /* When non-null, this node will be the one that needs to process the node graph. */
};

MA_API ma_result ma_cli_play_init(const ma_cli_node_config* pConfig, ma_cli_play* pPlay);
MA_API void ma_cli_play_uninit(ma_cli_play* pPlay);
MA_API ma_cli_node* ma_cli_play_node(ma_cli_play* pPlay);
MA_API ma_result ma_cli_play_read_pcm_frames(ma_cli_play* pPlay, void* pFrames, ma_uint32 frameCount, ma_uint32* pFramesRead);
MA_API ma_device* ma_cli_play_get_device(ma_cli_play* pPlay);
MA_API void ma_cli_play_set_node_graph(ma_cli_play* pPlay, ma_node_graph* pNodeGraph);
/* END miniaudio_cli_play.h */

/* BEG miniaudio_cli_play.c */
static void ma_cli_play__node_on_info(ma_cli_node_info* pInfo)
{
    pInfo->pName = "play";
}

static void ma_cli_play__node_on_default_config(ma_cli_node_config* pConfig)
{
    pConfig->play.periodSizeInFrames = MA_CLI_DEFAULT_PERIOD_SIZE_IN_FRAMES;
}

static ma_bool32 ma_cli_play__node_on_arg(ma_cli_args* pArgs, ma_cli_node_config* pConfig)
{
    (void)pArgs;
    (void)pConfig;

    return MA_FALSE;
}

static size_t ma_cli_play__node_on_sizeof(const void* pConfig)
{
    return sizeof(ma_cli_play);
}

static ma_result ma_cli_play__node_on_init(const void* pConfig, ma_cli_node* pNode)
{
    return ma_cli_play_init((ma_cli_node_config*)pConfig, (ma_cli_play*)pNode);
}

static void ma_cli_play__node_on_uninit(ma_cli_node* pNode)
{
    ma_cli_play_uninit((ma_cli_play*)pNode);
}

static void ma_cli_play__node_on_process(ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_cli_play* pPlay = (ma_cli_play*)pNode;
    ma_uint32 frameCount = *pFrameCountOut;
    ma_format format   = pPlay->device.playback.format;
    ma_uint32 channels = pPlay->device.playback.channels;

    (void)pFrameCountIn;

    /* The data is passed straight through, with a copy stored in our internal buffer. */
    MA_COPY_MEMORY(ppFramesOut[0], ppFramesIn[0], frameCount * ma_get_bytes_per_frame(format, channels));

    /* Make a copy for our internal buffer. */
    if (frameCount > pPlay->bufferSizeInFrames) {
        frameCount = pPlay->bufferSizeInFrames;
    }
    
    if (format == ma_format_f32) {
        MA_COPY_MEMORY(pPlay->pBuffer, ppFramesIn[0], frameCount * ma_get_bytes_per_frame(format, channels));
    } else {
        ma_pcm_convert(pPlay->pBuffer, format, ppFramesIn[0], ma_format_f32, frameCount * channels, ma_dither_mode_none);
    }

    /* Silence out the tail of the buffer if the frame count is less than the buffer size. Should never happen, but just for safety. */
    ma_silence_pcm_frames(ma_offset_pcm_frames_ptr(pPlay->pBuffer, frameCount, format, channels), pPlay->bufferSizeInFrames - frameCount, format, channels);

    pPlay->cursor = 0;
}

static ma_cli_node_vtable ma_gNodeVTable_Play =
{
    ma_cli_play__node_on_info,
    ma_cli_play__node_on_default_config,
    ma_cli_play__node_on_arg,
    ma_cli_play__node_on_sizeof,
    ma_cli_play__node_on_init,
    ma_cli_play__node_on_uninit,
    ma_cli_play__node_on_process
};

#if 0
static ma_cli_node_config ma_cli_node_config_init_play(const ma_uint32* pInputChannelCounts, const ma_uint32* pOutputChannelCounts)
{
    return ma_cli_node_config_init(&ma_gNodeVTable_Play, 1, 1, pInputChannelCounts, pOutputChannelCounts);
}


MA_API ma_cli_play_config ma_cli_play_config_init()
{
    ma_cli_play_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig         = ma_cli_node_config_init_play(NULL, NULL);
    config.format             = ma_format_f32;
    config.channels           = 0;
    config.sampleRate         = 0;
    config.periodSizeInFrames = 1024;

    return config;
}
#endif


static void ma_cli_play_device_data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    ma_cli_play* pPlay = (ma_cli_play*)ma_device_get_user_data(pDevice);

    if (pPlay->pNodeGraph != NULL) {
        ma_cli_process_node_graph(pPlay->pNodeGraph);
    }

    ma_cli_play_read_pcm_frames(pPlay, pFramesOut, frameCount, NULL);

    (void)pFramesIn;
}

MA_API ma_result ma_cli_play_init(const ma_cli_node_config* pConfig, ma_cli_play* pPlay)
{
    ma_result result;
    ma_cli_node_config nodeConfig;
    ma_device_config deviceConfig;

    if (pPlay == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pPlay);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format    = pConfig->format;
    deviceConfig.playback.channels  = pConfig->channels;
    deviceConfig.sampleRate         = pConfig->sampleRate;
    deviceConfig.periodSizeInFrames = pConfig->play.periodSizeInFrames;
    deviceConfig.dataCallback       = ma_cli_play_device_data_callback;
    deviceConfig.pUserData          = pPlay;
    deviceConfig.threadingMode      = MA_THREADING_MODE_SINGLE_THREADED;    /* Devices will be stepped manually. */

    result = ma_device_init_ex((pConfig->backendCount == 0) ? NULL : pConfig->pBackends, pConfig->backendCount, NULL, &deviceConfig, &pPlay->device);
    if (result != MA_SUCCESS) {
        ma_cli_node_uninit(&pPlay->node);
        return result;
    }


    nodeConfig = *pConfig;
    nodeConfig.inputCount           = 1;
    nodeConfig.outputCount          = 1;
    nodeConfig.pInputChannelCounts  = &pPlay->device.playback.channels;
    nodeConfig.pOutputChannelCounts = &pPlay->device.playback.channels;
    nodeConfig.channels             = pPlay->device.playback.channels;
    nodeConfig.sampleRate           = pPlay->device.sampleRate;
    nodeConfig.pAllocationCallbacks = pConfig->pAllocationCallbacks;

    result = ma_cli_node_init(&nodeConfig, &pPlay->node);
    if (result != MA_SUCCESS) {
        return result;
    }

    /*
    We need an internal buffer. The idea is that it'll be filled during node graph processing and then
    emptied from the data device callback.
    */
    pPlay->bufferSizeInFrames = ma_device_get_period_size_in_frames(&pPlay->device);

    pPlay->pBuffer = ma_calloc(pPlay->bufferSizeInFrames * ma_get_bytes_per_frame(pPlay->device.playback.format, pPlay->device.playback.channels), nodeConfig.pAllocationCallbacks);
    if (pPlay->pBuffer == NULL) {
        ma_cli_node_uninit(&pPlay->node);
        return MA_OUT_OF_MEMORY;
    }

    return MA_SUCCESS;
}

MA_API void ma_cli_play_uninit(ma_cli_play* pPlay)
{
    ma_allocation_callbacks allocationCallbacks;

    if (pPlay == NULL) {
        return;
    }

    allocationCallbacks = pPlay->node.allocationCallbacks;

    ma_cli_node_uninit(&pPlay->node);
    ma_device_uninit(&pPlay->device);
    ma_free(pPlay->pBuffer, &allocationCallbacks);
}

MA_API ma_cli_node* ma_cli_play_node(ma_cli_play* pPlay)
{
    return (ma_cli_node*)pPlay;
}

MA_API ma_result ma_cli_play_read_pcm_frames(ma_cli_play* pPlay, void* pFramesOut, ma_uint32 frameCount, ma_uint32* pFramesRead)
{
    ma_uint32 framesAvailable;
    ma_uint32 bpf;

    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (pPlay == NULL || pFramesOut == NULL) {
        return MA_INVALID_ARGS;
    }

    framesAvailable = pPlay->bufferSizeInFrames - pPlay->cursor;
    
    if (frameCount > framesAvailable) {
        frameCount = framesAvailable;
    }

    bpf = ma_get_bytes_per_frame(pPlay->device.playback.format, pPlay->device.playback.channels);

    MA_COPY_MEMORY(pFramesOut, ma_offset_ptr(pPlay->pBuffer, pPlay->cursor * bpf), frameCount * bpf);
    pPlay->cursor += frameCount;

    return MA_SUCCESS;
}

MA_API ma_device* ma_cli_play_get_device(ma_cli_play* pPlay)
{
    if (pPlay == NULL) {
        return NULL;
    }

    return &pPlay->device;
}

MA_API void ma_cli_play_set_node_graph(ma_cli_play* pPlay, ma_node_graph* pNodeGraph)
{
    if (pPlay == NULL) {
        return;
    }

    pPlay->pNodeGraph = pNodeGraph;
}
/* END miniaudio_cli_play.c */


/* BEG miniaudio_cli_record.h */
typedef struct ma_cli_record ma_cli_record;
struct ma_cli_record
{
    ma_cli_node node;
    ma_device device;
    void* pBuffer;
    ma_uint32 bufferSizeInFrames;
    ma_uint32 cursor;
    ma_node_graph* pNodeGraph;  /* When non-null, this node will be the one that needs to process the node graph. */
};

MA_API ma_result ma_cli_record_init(const ma_cli_node_config* pConfig, ma_cli_record* pRecord);
MA_API void ma_cli_record_uninit(ma_cli_record* pRecord);
MA_API ma_cli_node* ma_cli_record_node(ma_cli_record* pRecord);
MA_API ma_result ma_cli_record_read_pcm_frames(ma_cli_record* pRecord, void* pFrames, ma_uint32 frameCount, ma_uint32* pFramesRead);
MA_API ma_device* ma_cli_record_get_device(ma_cli_record* pRecord);
MA_API void ma_cli_record_set_node_graph(ma_cli_record* pRecord, ma_node_graph* pNodeGraph);
/* END miniaudio_cli_record.h */

/* BEG miniaudio_cli_record.c */
static void ma_cli_record__node_on_info(ma_cli_node_info* pInfo)
{
    pInfo->pName = "record";
}

static void ma_cli_record__node_on_default_config(ma_cli_node_config* pConfig)
{
    pConfig->record.periodSizeInFrames = MA_CLI_DEFAULT_PERIOD_SIZE_IN_FRAMES;
}

static ma_bool32 ma_cli_record__node_on_arg(ma_cli_args* pArgs, ma_cli_node_config* pConfig)
{
    (void)pArgs;
    (void)pConfig;

    return MA_FALSE;
}

static size_t ma_cli_record__node_on_sizeof(const void* pConfig)
{
    return sizeof(ma_cli_record);
}

static ma_result ma_cli_record__node_on_init(const void* pConfig, ma_cli_node* pNode)
{
    return ma_cli_record_init((ma_cli_node_config*)pConfig, (ma_cli_record*)pNode);
}

static void ma_cli_record__node_on_uninit(ma_cli_node* pNode)
{
    ma_cli_record_uninit((ma_cli_record*)pNode);
}

static void ma_cli_record__node_on_process(ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_cli_record* pRecord = (ma_cli_record*)pNode;
    ma_uint32 frameCount = *pFrameCountOut;
    ma_format format   = pRecord->device.capture.format;
    ma_uint32 channels = pRecord->device.capture.channels;

    /* No input. */
    (void)ppFramesIn;
    (void)pFrameCountIn;

    /* We copy from our internal cache to the output buffer. */
    if (frameCount > pRecord->bufferSizeInFrames) {
        frameCount = pRecord->bufferSizeInFrames;
    }
    
    if (format == ma_format_f32) {
        MA_COPY_MEMORY(ppFramesOut[0], pRecord->pBuffer, frameCount * ma_get_bytes_per_frame(format, channels));
    } else {
        ma_pcm_convert(ppFramesOut[0], ma_format_f32, pRecord->pBuffer, format, frameCount * channels, ma_dither_mode_none);
    }

    /* Silence out the tail of the buffer if the frame count is less than the buffer size. */
    ma_silence_pcm_frames(ma_offset_pcm_frames_ptr(pRecord->pBuffer, frameCount, format, channels), pRecord->bufferSizeInFrames - frameCount, format, channels);

    pRecord->cursor = 0;

    *pFrameCountOut = frameCount;
}

static ma_cli_node_vtable ma_gNodeVTable_Record =
{
    ma_cli_record__node_on_info,
    ma_cli_record__node_on_default_config,
    ma_cli_record__node_on_arg,
    ma_cli_record__node_on_sizeof,
    ma_cli_record__node_on_init,
    ma_cli_record__node_on_uninit,
    ma_cli_record__node_on_process
};

#if 0
static ma_cli_node_config ma_cli_node_config_init_record(const ma_uint32* pOutputChannelCounts)
{
    return ma_cli_node_config_init(&ma_gNodeVTable_Record, 0, 1, NULL, pOutputChannelCounts);
}


MA_API ma_cli_record_config ma_cli_record_config_init()
{
    ma_cli_record_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig         = ma_cli_node_config_init_record(NULL);
    config.format             = ma_format_f32;
    config.channels           = 0;
    config.sampleRate         = 0;
    config.periodSizeInFrames = 1024;

    return config;
}
#endif


static void ma_cli_record_device_data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    ma_cli_record* pRecord = (ma_cli_record*)ma_device_get_user_data(pDevice);

    MA_COPY_MEMORY(pRecord->pBuffer, pFramesIn, frameCount * ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels));
    pRecord->bufferSizeInFrames = frameCount;

    if (pRecord->pNodeGraph != NULL) {
        ma_cli_process_node_graph(pRecord->pNodeGraph);
    }

    (void)pFramesIn;
}

MA_API ma_result ma_cli_record_init(const ma_cli_node_config* pConfig, ma_cli_record* pRecord)
{
    ma_result result;
    ma_cli_node_config nodeConfig;
    ma_device_config deviceConfig;

    if (pRecord == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pRecord);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format     = pConfig->format;
    deviceConfig.capture.channels   = pConfig->channels;
    deviceConfig.sampleRate         = pConfig->sampleRate;
    deviceConfig.periodSizeInFrames = pConfig->record.periodSizeInFrames;
    deviceConfig.dataCallback       = ma_cli_record_device_data_callback;
    deviceConfig.pUserData          = pRecord;
    deviceConfig.threadingMode      = MA_THREADING_MODE_SINGLE_THREADED;    /* Devices will be stepped manually. */

    result = ma_device_init_ex((pConfig->backendCount == 0) ? NULL : pConfig->pBackends, pConfig->backendCount, NULL, &deviceConfig, &pRecord->device);
    if (result != MA_SUCCESS) {
        ma_cli_node_uninit(&pRecord->node);
        return result;
    }


    nodeConfig = *pConfig;
    nodeConfig.outputCount          = 1;
    nodeConfig.pOutputChannelCounts = &pRecord->device.capture.channels;
    nodeConfig.sampleRate           = pRecord->device.sampleRate;
    nodeConfig.pAllocationCallbacks = pConfig->pAllocationCallbacks;

    result = ma_cli_node_init(&nodeConfig, &pRecord->node);
    if (result != MA_SUCCESS) {
        return result;
    }


    /*
    We need an internal buffer. The idea is that it'll be filled during node graph processing and then
    emptied from the data device callback.
    */
    pRecord->bufferSizeInFrames = ma_device_get_period_size_in_frames(&pRecord->device);

    pRecord->pBuffer = ma_calloc(pRecord->bufferSizeInFrames * ma_get_bytes_per_frame(pRecord->device.capture.format, pRecord->device.capture.channels), nodeConfig.pAllocationCallbacks);
    if (pRecord->pBuffer == NULL) {
        ma_cli_node_uninit(&pRecord->node);
        return MA_OUT_OF_MEMORY;
    }

    return MA_SUCCESS;
}

MA_API void ma_cli_record_uninit(ma_cli_record* pRecord)
{
    ma_allocation_callbacks allocationCallbacks;

    if (pRecord == NULL) {
        return;
    }

    allocationCallbacks = pRecord->node.allocationCallbacks;

    ma_cli_node_uninit(&pRecord->node);
    ma_device_uninit(&pRecord->device);
    ma_free(pRecord->pBuffer, &allocationCallbacks);
}

MA_API ma_cli_node* ma_cli_record_node(ma_cli_record* pRecord)
{
    return (ma_cli_node*)pRecord;
}

MA_API ma_result ma_cli_record_read_pcm_frames(ma_cli_record* pRecord, void* pFramesOut, ma_uint32 frameCount, ma_uint32* pFramesRead)
{
    ma_uint32 framesAvailable;
    ma_uint32 bpf;

    if (pFramesRead != NULL) {
        *pFramesRead = 0;
    }

    if (pRecord == NULL || pFramesOut == NULL) {
        return MA_INVALID_ARGS;
    }

    framesAvailable = pRecord->bufferSizeInFrames - pRecord->cursor;
    
    if (frameCount > framesAvailable) {
        frameCount = framesAvailable;
    }

    bpf = ma_get_bytes_per_frame(pRecord->device.capture.format, pRecord->device.capture.channels);

    MA_COPY_MEMORY(pFramesOut, ma_offset_ptr(pRecord->pBuffer, pRecord->cursor * bpf), frameCount * bpf);
    pRecord->cursor += frameCount;

    return MA_SUCCESS;
}

MA_API ma_device* ma_cli_record_get_device(ma_cli_record* pRecord)
{
    if (pRecord == NULL) {
        return NULL;
    }

    return &pRecord->device;
}

MA_API void ma_cli_record_set_node_graph(ma_cli_record* pRecord, ma_node_graph* pNodeGraph)
{
    if (pRecord == NULL) {
        return;
    }

    pRecord->pNodeGraph = pNodeGraph;
}
/* END miniaudio_cli_record.c */


/* BEG miniaudio_cli_encode.h */
typedef struct ma_cli_encode ma_cli_encode;
struct ma_cli_encode
{
    ma_cli_node node;
    ma_encoding_format encodingFormat;  /* When unknown, will use raw. */
    ma_encoder encoder;
    ma_format format;
    ma_uint32 channels;
    fs_file* pFile;
    ma_bool32 isOwnerOfFile;
};

MA_API ma_result ma_cli_encode_init(const ma_cli_node_config* pConfig, ma_cli_encode* pEncode);
MA_API void ma_cli_encode_uninit(ma_cli_encode* pEncode);
MA_API ma_cli_node* ma_cli_encode_node(ma_cli_encode* pEncode);
/* END miniaudio_cli_encode.h */

/* BEG miniaudio_cli_encode.c */
static void ma_cli_encode__node_on_info(ma_cli_node_info* pInfo)
{
    pInfo->pName = "encode";
}

static void ma_cli_encode__node_on_default_config(ma_cli_node_config* pConfig)
{
    (void)pConfig;
}

static ma_bool32 ma_cli_encode__node_on_arg(ma_cli_args* pArgs, ma_cli_node_config* pConfig)
{
    (void)pArgs;
    (void)pConfig;

    return MA_FALSE;
}

static size_t ma_cli_encode__node_on_sizeof(const void* pConfig)
{
    return sizeof(ma_cli_encode);
}

static ma_result ma_cli_encode__node_on_init(const void* pConfig, ma_cli_node* pNode)
{
    return ma_cli_encode_init((ma_cli_node_config*)pConfig, (ma_cli_encode*)pNode);
}

static void ma_cli_encode__node_on_uninit(ma_cli_node* pNode)
{
    ma_cli_encode_uninit((ma_cli_encode*)pNode);
}

static void ma_cli_encode__node_on_process(ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_cli_encode* pEncode = (ma_cli_encode*)pNode;
    ma_result result;
    ma_uint64 frameCount;
    ma_uint64 framesRead;
    ma_uint32 bpf = ma_get_bytes_per_frame(pEncode->format, pEncode->channels);

    frameCount = *pFrameCountIn;

    if (pEncode->format == ma_format_f32) {
        if (pEncode->pFile != NULL) {
            fs_file_write(pEncode->pFile, ppFramesIn[0], (size_t)(frameCount * bpf), NULL);
        } else {
            ma_encoder_write_pcm_frames(&pEncode->encoder, ppFramesIn[0], frameCount, NULL);
        }
    } else {
        /* Need to convert. */
        ma_uint32 temp[1024];
        ma_uint32 tempCap = sizeof(temp) / ma_get_bytes_per_frame(pEncode->format, pEncode->channels);
        ma_uint64 totalFramesProcessed;
        const float* pRunningFramesIn;
        
        pRunningFramesIn = ppFramesIn[0];

        totalFramesProcessed = 0;
        while (totalFramesProcessed < frameCount) {
            ma_uint64 framesRemaining = frameCount - totalFramesProcessed;
            ma_uint64 framesToWriteThisIteration = framesRemaining;
            if (framesToWriteThisIteration > tempCap) {
                framesToWriteThisIteration = tempCap;
            }

            ma_pcm_convert(temp, pEncode->format, pRunningFramesIn, ma_format_f32, framesToWriteThisIteration, ma_dither_mode_triangle);

            if (pEncode->pFile != NULL) {
                result = ma_result_from_fs(fs_file_write(pEncode->pFile, ppFramesIn[0], (size_t)(framesToWriteThisIteration * bpf), NULL));
            } else {
                result = ma_encoder_write_pcm_frames(&pEncode->encoder, ppFramesIn[0], framesToWriteThisIteration, NULL);
            }

            if (result != MA_SUCCESS) {
                break;
            }

            totalFramesProcessed += framesToWriteThisIteration;
            pRunningFramesIn     += framesToWriteThisIteration * pEncode->channels;
        }

        framesRead = totalFramesProcessed;
    }
}

static ma_cli_node_vtable ma_gNodeVTable_Encode =
{
    ma_cli_encode__node_on_info,
    ma_cli_encode__node_on_default_config,
    ma_cli_encode__node_on_arg,
    ma_cli_encode__node_on_sizeof,
    ma_cli_encode__node_on_init,
    ma_cli_encode__node_on_uninit,
    ma_cli_encode__node_on_process
};


MA_API ma_result ma_cli_encode_init(const ma_cli_node_config* pConfig, ma_cli_encode* pEncode)
{
    ma_result result;
    ma_cli_node_config nodeConfig;

    if (pEncode == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pEncode);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    pEncode->format = pConfig->format;
    if (pEncode->format == ma_format_unknown) {
        pEncode->format =  ma_format_f32;
    }

    pEncode->channels = pConfig->channels;

    if (pConfig->pFilePath != NULL) {
        /* Open the file. */
        size_t filePathLen;
        char* pFilePath;

        filePathLen = pConfig->filePathLen;
        if (filePathLen == (size_t)-1) {
            filePathLen = strlen(pConfig->pFilePath);
        }

        pFilePath = (char*)ma_malloc(filePathLen + 1, pConfig->pAllocationCallbacks);
        if (pFilePath == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        ma_strcpy_s(pFilePath, filePathLen + 1, pConfig->pFilePath);

        pEncode->encodingFormat = ma_encoding_format_from_path(pFilePath);
        if (pEncode->encodingFormat == ma_encoding_format_unknown) {
            result = ma_result_from_fs(fs_file_open(NULL, pFilePath, FS_WRITE, &pEncode->pFile));
        } else {
            ma_encoder_config encoderConfig = ma_encoder_config_init(pEncode->encodingFormat, pEncode->format, pEncode->channels, pConfig->sampleRate);
            
            result = ma_encoder_init_file(pFilePath, &encoderConfig, &pEncode->encoder);
        }

        ma_free(pFilePath, pConfig->pAllocationCallbacks);

        if (result != MA_SUCCESS) {
            return result;
        }

        pEncode->isOwnerOfFile = MA_TRUE;
    } else {
        return MA_INVALID_ARGS;
    }

    nodeConfig = *pConfig;
    nodeConfig.inputCount           = 1;
    nodeConfig.outputCount          = 1;
    nodeConfig.pInputChannelCounts  = &pConfig->channels;
    nodeConfig.pOutputChannelCounts = &pConfig->channels;
    nodeConfig.pAllocationCallbacks = pConfig->pAllocationCallbacks;

    result = ma_cli_node_init(&nodeConfig, &pEncode->node);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_cli_encode_uninit(ma_cli_encode* pEncode)
{
    if (pEncode == NULL) {
        return;
    }

    if (pEncode->pFile != NULL) {
        if (pEncode->isOwnerOfFile) {
            fs_file_close(pEncode->pFile);
        }
    } else {
        ma_encoder_uninit(&pEncode->encoder);
    }

    ma_cli_node_uninit(&pEncode->node);
}

MA_API ma_cli_node* ma_cli_encode_node(ma_cli_encode* pEncode)
{
    return (ma_cli_node*)pEncode;
}
/* END miniaudio_cli_encode.c */


/* BEG miniaudio_cli_converter.h */
typedef struct ma_cli_converter_config ma_cli_converter_config;
struct ma_cli_converter_config
{
    ma_cli_node_config nodeConfig;
    ma_uint32 inputChannels;
    ma_uint32 outputChannels;
    ma_uint32 inputSampleRate;
    ma_uint32 outputSampleRate;
};

MA_API ma_cli_converter_config ma_cli_converter_config_init(ma_uint32 inputChannels, ma_uint32 outputChannels, ma_uint32 inputSampleRate, ma_uint32 outputSampleRate);


typedef struct ma_cli_converter ma_cli_converter;
struct ma_cli_converter
{
    ma_cli_node node;
    ma_data_converter dataConverter;
};

MA_API ma_result ma_cli_converter_init(const ma_cli_converter_config* pConfig, ma_cli_converter* pConverter);
MA_API void ma_cli_converter_uninit(ma_cli_converter* pConverter);
MA_API ma_cli_node* ma_cli_converter_node(ma_cli_converter* pConverter);
/* END miniaudio_cli_converter.h */

/* BEG miniaudio_cli_converter.c */
static void ma_cli_converter__node_on_info(ma_cli_node_info* pInfo)
{
    pInfo->pName = "converter";
}

static void ma_cli_converter__node_on_default_config(ma_cli_node_config* pConfig)
{
    (void)pConfig;
}

static ma_bool32 ma_cli_converter__node_on_arg(ma_cli_args* pArgs, ma_cli_node_config* pConfig)
{
    (void)pArgs;
    (void)pConfig;

    return MA_FALSE;
}

static size_t ma_cli_converter__node_on_sizeof(const void* pConfig)
{
    return sizeof(ma_cli_converter);
}

static ma_result ma_cli_converter__node_on_init(const void* pConfig, ma_cli_node* pNode)
{
    return ma_cli_converter_init((ma_cli_converter_config*)pConfig, (ma_cli_converter*)pNode);
}

static void ma_cli_converter__node_on_uninit(ma_cli_node* pNode)
{
    ma_cli_converter_uninit((ma_cli_converter*)pNode);
}

static void ma_cli_converter__node_on_process(ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    ma_cli_converter* pConverter = (ma_cli_converter*)pNode;
    ma_uint64 frameCountIn;
    ma_uint64 frameCountOut;

    (void)pNode;
    (void)ppFramesIn;
    (void)pFrameCountIn;
    (void)ppFramesOut;
    (void)pFrameCountOut;

    frameCountIn  = *pFrameCountIn;
    frameCountOut = *pFrameCountOut;

    ma_data_converter_process_pcm_frames(&pConverter->dataConverter, ppFramesIn[0], &frameCountIn, ppFramesOut[0], &frameCountOut);

    *pFrameCountIn  = (ma_uint32)frameCountIn;
    *pFrameCountOut = (ma_uint32)frameCountOut;
}

static ma_cli_node_vtable ma_gNodeVTable_Converter =
{
    ma_cli_converter__node_on_info,
    ma_cli_converter__node_on_default_config,
    ma_cli_converter__node_on_arg,
    ma_cli_converter__node_on_sizeof,
    ma_cli_converter__node_on_init,
    ma_cli_converter__node_on_uninit,
    ma_cli_converter__node_on_process
};

static ma_cli_node_config ma_cli_node_config_init_converter(ma_uint32 inputCount, ma_uint32 outputCount, const ma_uint32* pInputChannelCounts, const ma_uint32* pOutputChannelCounts)
{
    return ma_cli_node_config_init(&ma_gNodeVTable_Converter, inputCount, outputCount, pInputChannelCounts, pOutputChannelCounts);
}


MA_API ma_cli_converter_config ma_cli_converter_config_init(ma_uint32 inputChannels, ma_uint32 outputChannels, ma_uint32 inputSampleRate, ma_uint32 outputSampleRate)
{
    ma_cli_converter_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_cli_node_config_init_converter(0, 0, NULL, NULL);
    config.inputChannels    = inputChannels;
    config.outputChannels   = outputChannels;
    config.inputSampleRate  = inputSampleRate;
    config.outputSampleRate = outputSampleRate;

    return config;
}


MA_API ma_result ma_cli_converter_init(const ma_cli_converter_config* pConfig, ma_cli_converter* pConverter)
{
    ma_result result;
    ma_cli_node_config nodeConfig;
    ma_data_converter_config dataConverterConfig;

    if (pConverter == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pConverter);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    dataConverterConfig = ma_data_converter_config_init(ma_format_f32, ma_format_f32, pConfig->inputChannels, pConfig->outputChannels, pConfig->inputSampleRate, pConfig->outputSampleRate);

    result = ma_data_converter_init(&dataConverterConfig, pConfig->nodeConfig.pAllocationCallbacks, &pConverter->dataConverter);
    if (result != MA_SUCCESS) {
        return result;
    }


    nodeConfig = ma_cli_node_config_init_converter(1, 1, &pConfig->inputChannels, &pConfig->outputChannels);

    result = ma_cli_node_init(&nodeConfig, &pConverter->node);
    if (result != MA_SUCCESS) {
        ma_data_converter_uninit(&pConverter->dataConverter, pConfig->nodeConfig.pAllocationCallbacks);
        return result;
    }
    
    return MA_SUCCESS;
}

MA_API void ma_cli_converter_uninit(ma_cli_converter* pConverter)
{
    if (pConverter == NULL) {
        return;
    }

    ma_data_converter_uninit(&pConverter->dataConverter, &pConverter->node.allocationCallbacks);
    ma_cli_node_uninit(&pConverter->node);
}

MA_API ma_cli_node* ma_cli_converter_node(ma_cli_converter* pConverter)
{
    return (ma_cli_node*)pConverter;
}
/* END miniaudio_cli_converter.c */


/* BEG miniaudio_cli_endpoint.h */
typedef struct ma_cli_endpoint_config ma_cli_endpoint_config;
struct ma_cli_endpoint_config
{
    ma_cli_node_config nodeConfig;
    ma_uint32 inputCount;
    ma_uint32* pInputChannelCounts;
};

MA_API ma_cli_endpoint_config ma_cli_endpoint_config_init(void);


typedef struct ma_cli_endpoint ma_cli_endpoint;
struct ma_cli_endpoint
{
    ma_cli_node node;
};

MA_API ma_result ma_cli_endpoint_init(const ma_cli_endpoint_config* pConfig, ma_cli_endpoint* pEndpoint);
MA_API void ma_cli_endpoint_uninit(ma_cli_endpoint* pEndpoint);
MA_API ma_cli_node* ma_cli_endpoint_node(ma_cli_endpoint* pEndpoint);
/* END miniaudio_cli_endpoint.h */

/* BEG miniaudio_cli_endpoint.c */
static void ma_cli_endpoint__node_on_info(ma_cli_node_info* pInfo)
{
    pInfo->pName = "endpoint";
}

static void ma_cli_endpoint__node_on_default_config(ma_cli_node_config* pConfig)
{
    (void)pConfig;
}

static ma_bool32 ma_cli_endpoint__node_on_arg(ma_cli_args* pArgs, ma_cli_node_config* pConfig)
{
    (void)pArgs;
    (void)pConfig;

    return MA_FALSE;
}

static size_t ma_cli_endpoint__node_on_sizeof(const void* pConfig)
{
    return sizeof(ma_cli_endpoint);
}

static ma_result ma_cli_endpoint__node_on_init(const void* pConfig, ma_cli_node* pNode)
{
    return ma_cli_endpoint_init((ma_cli_endpoint_config*)pConfig, (ma_cli_endpoint*)pNode);
}

static void ma_cli_endpoint__node_on_uninit(ma_cli_node* pNode)
{
    ma_cli_endpoint_uninit((ma_cli_endpoint*)pNode);
}

static void ma_cli_endpoint__node_on_process(ma_cli_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn, float** ppFramesOut, ma_uint32* pFrameCountOut)
{
    /* Our endpoint doesn't care about data because we'll never be doing anything with the output. */
    (void)pNode;
    (void)ppFramesIn;
    (void)pFrameCountIn;
    (void)ppFramesOut;
    (void)pFrameCountOut;
}

static ma_cli_node_vtable ma_gNodeVTable_Endpoint =
{
    ma_cli_endpoint__node_on_info,
    ma_cli_endpoint__node_on_default_config,
    ma_cli_endpoint__node_on_arg,
    ma_cli_endpoint__node_on_sizeof,
    ma_cli_endpoint__node_on_init,
    ma_cli_endpoint__node_on_uninit,
    ma_cli_endpoint__node_on_process
};

static ma_cli_node_config ma_cli_node_config_init_endpoint(ma_uint32 inputCount, ma_uint32 outputCount, const ma_uint32* pInputChannelCounts, const ma_uint32* pOutputChannelCounts)
{
    return ma_cli_node_config_init(&ma_gNodeVTable_Endpoint, inputCount, outputCount, pInputChannelCounts, pOutputChannelCounts);
}


MA_API ma_cli_endpoint_config ma_cli_endpoint_config_init(void)
{
    ma_cli_endpoint_config config;

    MA_ZERO_OBJECT(&config);
    config.nodeConfig = ma_cli_node_config_init_endpoint(0, 0, NULL, NULL);

    return config;
}


MA_API ma_result ma_cli_endpoint_init(const ma_cli_endpoint_config* pConfig, ma_cli_endpoint* pEndpoint)
{
    ma_result result;
    ma_cli_node_config nodeConfig;
    ma_uint32 outputChannels = 1;   /* Doesn't matter what we set this to since the endpoint will be discarding everything. */

    if (pEndpoint == NULL) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pEndpoint);

    if (pConfig == NULL) {
        return MA_INVALID_ARGS;
    }

    nodeConfig = ma_cli_node_config_init_endpoint(pConfig->inputCount, 1, pConfig->pInputChannelCounts, &outputChannels);

    result = ma_cli_node_init(&nodeConfig, &pEndpoint->node);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}

MA_API void ma_cli_endpoint_uninit(ma_cli_endpoint* pEndpoint)
{
    if (pEndpoint == NULL) {
        return;
    }

    ma_cli_node_uninit(&pEndpoint->node);
}

MA_API ma_cli_node* ma_cli_endpoint_node(ma_cli_endpoint* pEndpoint)
{
    return (ma_cli_node*)pEndpoint;
}
/* END miniaudio_cli_endpoint.c */
