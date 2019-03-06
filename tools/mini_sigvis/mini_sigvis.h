// Signal visualization library. Public domain. See "unlicense" statement at the end of this file.
// mini_sigvis - v0.x - 2018-xx-xx
//
// David Reid - davidreidsoftware@gmail.com

#ifndef mini_sigvis_h
#define mini_sigvis_h

#include "../../miniaudio.h"
#include "../external/dred/source/dred/dtk/dtk.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msigvis_context msigvis_context;
typedef struct msigvis_screen  msigvis_screen;
typedef struct msigvis_channel msigvis_channel;

struct msigvis_context
{
    dtk_context tk;
};

struct msigvis_screen
{
    dtk_window window;
    dtk_uint32 sampleRate;
    float zoomX;
    float zoomY;
    dtk_color bgColor;
    dtk_uint32 channelCount;
    dtk_uint32 channelCap;
    msigvis_channel** ppChannels;
};

struct msigvis_channel
{
    ma_format format;
    ma_uint32 sampleRate;
    dtk_color color;
    ma_uint32 sampleCount;
    ma_uint32 bufferCapInSamples;
    ma_uint8* pBuffer; // The buffer containing the sample to visualize.
};


// Context
ma_result msigvis_init(msigvis_context* pContext);
void msigvis_uninit(msigvis_context* pContext);
int msigvis_run(msigvis_context* pContext);

// Screen
ma_result msigvis_screen_init(msigvis_context* pContext, ma_uint32 screenWidth, ma_uint32 screenHeight, msigvis_screen* pScreen);
void msigvis_screen_uninit(msigvis_screen* pScreen);
ma_result msigvis_screen_show(msigvis_screen* pScreen);
ma_result msigvis_screen_hide(msigvis_screen* pScreen);
ma_result msigvis_screen_add_channel(msigvis_screen* pScreen, msigvis_channel* pChannel);
ma_result msigvis_screen_remove_channel(msigvis_screen* pScreen, msigvis_channel* pChannel);
ma_result msigvis_screen_remove_channel_by_index(msigvis_screen* pScreen, ma_uint32 iChannel);
ma_result msigvis_screen_find_channel_index(msigvis_screen* pScreen, msigvis_channel* pChannel, ma_uint32* pIndex);
ma_result msigvis_screen_redraw(msigvis_screen* pScreen);

// Channel
ma_result msigvis_channel_init(msigvis_context* pContext, ma_format format, ma_uint32 sampleRate, msigvis_channel* pChannel);
void msigvis_channel_uninit(msigvis_channel* pChannel);
ma_result msigvis_channel_push_samples(msigvis_channel* pChannel, ma_uint32 sampleCount, const void* pSamples);
ma_result msigvis_channel_pop_samples(msigvis_channel* pChannel, ma_uint32 sampleCount);
float msigvis_channel_get_sample_f32(msigvis_channel* pChannel, ma_uint32 iSample);

#ifdef __cplusplus
}
#endif

#endif  // mini_sigvis_h

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef MINI_SIGVIS_IMPLEMENTATION
#define MINIAUDIO_IMPLEMENTATION
#include "../../miniaudio.h"
#include "../external/dred/source/dred/dtk/dtk.c"

ma_result msigvis_result_from_dtk(dtk_result resultDTK)
{
    return (ma_result)resultDTK;
}


ma_result msigvis_init(msigvis_context* pContext)
{
    if (pContext == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_zero_object(pContext);

    // DTK context.
    dtk_result resultDTK = dtk_init(&pContext->tk, NULL, pContext);
    if (resultDTK != DTK_SUCCESS) {
        return msigvis_result_from_dtk(resultDTK);
    }

    return MA_SUCCESS;
}

void msigvis_uninit(msigvis_context* pContext)
{
    if (pContext == NULL) {
        return;
    }

    dtk_uninit(&pContext->tk);
}

int msigvis_run(msigvis_context* pContext)
{
    int exitCode = 0;
    for (;;) {
        dtk_result result = dtk_next_event(&pContext->tk, DTK_TRUE, &exitCode);  // <-- DTK_TRUE = blocking.
        if (result != DTK_SUCCESS) {
            break;
        }
    }

    return exitCode;
}


///////////////////////////////////////////////////////////////////////////////
//
// Screen
//
///////////////////////////////////////////////////////////////////////////////
dtk_bool32 msigvis_window_event_handler(dtk_event* pEvent)
{
    dtk_window* pWindow = DTK_WINDOW(pEvent->pControl);

    msigvis_screen* pScreen = (msigvis_screen*)DTK_CONTROL(pWindow)->pUserData;
    dtk_assert(pScreen != NULL);

    switch (pEvent->type)
    {
        case DTK_EVENT_CLOSE:
        {
            dtk_post_quit_event(pEvent->pTK, 0);
        } break;

        case DTK_EVENT_SIZE:
        {
            dtk_window_scheduled_redraw(pWindow, dtk_window_get_client_rect(pWindow));
        } break;

        case DTK_EVENT_MOUSE_WHEEL:
        {
            if (pEvent->mouseWheel.delta > 0) {
                pScreen->zoomX = pScreen->zoomX * ( 2*pEvent->mouseWheel.delta);
                if (pScreen->zoomX > 10000.0f) {
                    pScreen->zoomX = 10000.0f;
                }
            } else {
                pScreen->zoomX = pScreen->zoomX / (-2*pEvent->mouseWheel.delta);
                if (pScreen->zoomX < 0.000001f) {
                    pScreen->zoomX = 0.000001f;
                }
            }

            dtk_window_scheduled_redraw(pWindow, dtk_window_get_client_rect(pWindow));
        } break;

        case DTK_EVENT_PAINT:
        {
            dtk_surface* pSurface = pEvent->paint.pSurface;
            dtk_assert(pSurface != NULL);

            dtk_surface_clear(pSurface, dtk_rgb(0, 32, 16));

            // At zoom level 1 we draw one tenth of a second worth of samples to the screen at the screens sample rate.
            dtk_int32 screenSizeX;
            dtk_int32 screenSizeY;
            dtk_window_get_size(&pScreen->window, &screenSizeX, &screenSizeY);

            float baseSampleSpacingX = (screenSizeX / (float)(pScreen->sampleRate/10)) * pScreen->zoomX;
            float baseSampleSpacingY = ((screenSizeY/1) / 2.0f) * pScreen->zoomY;
            
            for (ma_uint32 iChannel = 0; iChannel < pScreen->channelCount; ++iChannel) {
                msigvis_channel* pChannel = pScreen->ppChannels[iChannel];
                float spacingFactorX = pScreen->sampleRate / (float)pChannel->sampleRate;
                float sampleSpacingX = baseSampleSpacingX * spacingFactorX;
                float sampleSpacingY = baseSampleSpacingY;

                ma_uint32 sampleInterval = 1;
                if (sampleSpacingX < 1) {
                    sampleInterval = (ma_uint32)(1/sampleSpacingX);
                }

                if (sampleInterval == 0) {
                    sampleInterval = 1; // Safety.
                }

                ma_uint32 iFirstSample = 0;
                for (ma_uint32 iSample = iFirstSample; iSample < pChannel->sampleCount; iSample += sampleInterval) {
                    float samplePosX = iSample * sampleSpacingX;
                    float samplePosY = msigvis_channel_get_sample_f32(pChannel, iSample) * sampleSpacingY * -1; // Invert the Y axis for graphics output.

                    dtk_rect pointRect;
                    pointRect.left = (dtk_int32)samplePosX;
                    pointRect.right = pointRect.left + 2;
                    pointRect.top = (dtk_int32)samplePosY + (screenSizeY/2);
                    pointRect.bottom = pointRect.top - 2;
                    dtk_surface_draw_rect(pSurface, pointRect, pChannel->color);

                    if (pointRect.right > screenSizeX) {
                        break;
                    }
                }
            }
        } break;
    }

    return dtk_window_default_event_handler(pEvent);
}

ma_result msigvis_screen_init(msigvis_context* pContext, ma_uint32 screenWidth, ma_uint32 screenHeight, msigvis_screen* pScreen)
{
    if (pScreen == NULL) {
        return DTK_INVALID_ARGS;
    }

    ma_zero_object(pScreen);

    dtk_result resultDTK = dtk_window_init(&pContext->tk, msigvis_window_event_handler, NULL, dtk_window_type_toplevel, "mini_sigvis", screenWidth, screenHeight, &pScreen->window);
    if (resultDTK != DTK_SUCCESS) {
        return msigvis_result_from_dtk(resultDTK);
    }

    pScreen->window.control.pUserData = pScreen;

    pScreen->sampleRate = 48000;
    pScreen->zoomX = 1;
    pScreen->zoomY = 1;
    pScreen->bgColor = dtk_rgb(0, 32, 16);

    return DTK_SUCCESS;
}

void msigvis_screen_uninit(msigvis_screen* pScreen)
{
    if (pScreen == NULL) {
        return;
    }

    dtk_window_uninit(&pScreen->window);
}

ma_result msigvis_screen_show(msigvis_screen* pScreen)
{
    if (pScreen == NULL) {
        return MA_INVALID_ARGS;
    }

    return msigvis_result_from_dtk(dtk_window_show(&pScreen->window, DTK_SHOW_NORMAL));
}

ma_result msigvis_screen_hide(msigvis_screen* pScreen)
{
    if (pScreen == NULL) {
        return MA_INVALID_ARGS;
    }

    return msigvis_result_from_dtk(dtk_window_hide(&pScreen->window));
}

ma_result msigvis_screen_add_channel(msigvis_screen* pScreen, msigvis_channel* pChannel)
{
    if (pScreen == NULL || pChannel == NULL) {
        return MA_INVALID_ARGS;
    }

    // Expand if necessary.
    if (pScreen->channelCap == pScreen->channelCount) {
        ma_uint32 newCap = pScreen->channelCap*2;
        if (newCap == 0) {
            newCap = 1;
        }

        msigvis_channel** ppNewBuffer = (msigvis_channel**)ma_realloc(pScreen->ppChannels, sizeof(*pScreen->ppChannels)*newCap);
        if (ppNewBuffer == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        pScreen->channelCap = newCap;
        pScreen->ppChannels = ppNewBuffer;
    }

    pScreen->ppChannels[pScreen->channelCount] = pChannel;
    pScreen->channelCount += 1;

    msigvis_screen_redraw(pScreen);
    return MA_SUCCESS;
}

ma_result msigvis_screen_remove_channel(msigvis_screen* pScreen, msigvis_channel* pChannel)
{
    if (pScreen == NULL || pChannel == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_uint32 iChannel;
    ma_result result = msigvis_screen_find_channel_index(pScreen, pChannel, &iChannel);
    if (result != MA_SUCCESS) {
        return result;
    }

    return msigvis_screen_remove_channel_by_index(pScreen, iChannel);
}

ma_result msigvis_screen_remove_channel_by_index(msigvis_screen* pScreen, ma_uint32 iChannel)
{
    if (pScreen == NULL || iChannel > pScreen->channelCount) {
        return MA_INVALID_ARGS;
    }

    if (pScreen->channelCount == 0) {
        return MA_INVALID_OPERATION;
    }

    if (iChannel < pScreen->channelCount-1) {
        memmove(pScreen->ppChannels + iChannel, pScreen->ppChannels + iChannel + 1, sizeof(*pScreen->ppChannels) * (pScreen->channelCount - iChannel - 1));
    }
    
    pScreen->channelCount -= 1;

    msigvis_screen_redraw(pScreen);
    return MA_SUCCESS;
}

ma_result msigvis_screen_find_channel_index(msigvis_screen* pScreen, msigvis_channel* pChannel, ma_uint32* pIndex)
{
    if (pScreen == NULL || pChannel == NULL) {
        return MA_INVALID_ARGS;
    }

    for (ma_uint32 iChannel = 0; iChannel < pScreen->channelCount; ++iChannel) {
        if (pScreen->ppChannels[iChannel] == pChannel) {
            *pIndex = iChannel;
            return MA_SUCCESS;
        }
    }

    return MA_ERROR;
}

ma_result msigvis_screen_redraw(msigvis_screen* pScreen)
{
    if (pScreen == NULL) {
        return MA_INVALID_ARGS;
    }

    return msigvis_result_from_dtk(dtk_window_scheduled_redraw(&pScreen->window, dtk_window_get_client_rect(&pScreen->window)));
}


///////////////////////////////////////////////////////////////////////////////
//
// Channel
//
///////////////////////////////////////////////////////////////////////////////
ma_result msigvis_channel_init(msigvis_context* pContext, ma_format format, ma_uint32 sampleRate, msigvis_channel* pChannel)
{
    (void)pContext;

    if (pChannel == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_zero_object(pChannel);

    if (format == ma_format_unknown || sampleRate == 0) {
        return MA_INVALID_ARGS;
    }

    pChannel->format = format;
    pChannel->sampleRate = sampleRate;
    pChannel->color = dtk_rgb(255, 255, 255);

    return MA_SUCCESS;
}

void msigvis_channel_uninit(msigvis_channel* pChannel)
{
    if (pChannel == NULL) {
        return;
    }

    ma_free(pChannel->pBuffer);
}

ma_result msigvis_channel_push_samples(msigvis_channel* pChannel, ma_uint32 sampleCount, const void* pSamples)
{
    if (pChannel == NULL) {
        return MA_INVALID_ARGS;
    }

    ma_uint32 bps = ma_get_bytes_per_sample(pChannel->format);

    // Resize the buffer if necessary.
    if (pChannel->sampleCount + sampleCount >= pChannel->bufferCapInSamples) {
        ma_uint32 newBufferCapInSamples = ma_max(pChannel->sampleCount + sampleCount, pChannel->bufferCapInSamples*2);
        if (newBufferCapInSamples == 0) {
            newBufferCapInSamples = 32;
        }

        ma_uint8* pNewBuffer = (ma_uint8*)ma_realloc(pChannel->pBuffer, newBufferCapInSamples*bps);
        if (pNewBuffer == NULL) {
            return MA_OUT_OF_MEMORY;
        }

        pChannel->pBuffer = pNewBuffer;
        pChannel->bufferCapInSamples = newBufferCapInSamples;
    }

    ma_copy_memory(pChannel->pBuffer + pChannel->sampleCount*bps, pSamples, sampleCount*bps);
    pChannel->sampleCount += sampleCount;
    
    return MA_SUCCESS;
}

ma_result msigvis_channel_pop_samples(msigvis_channel* pChannel, ma_uint32 sampleCount)
{
    if (pChannel == NULL) {
        return MA_INVALID_ARGS;
    }

    if (sampleCount > pChannel->sampleCount) {
        sampleCount = pChannel->sampleCount;
    }

    ma_uint32 bps = ma_get_bytes_per_sample(pChannel->format);

    // This is just a dumb "move everything down" type of data movement. Need to change this to a circular buffer to make this more efficient.
    ma_uint32 bytesToRemove = sampleCount*bps;
    ma_assert(bytesToRemove > 0);

    memmove(pChannel->pBuffer, pChannel->pBuffer + bytesToRemove, pChannel->sampleCount*bps - bytesToRemove);
    pChannel->sampleCount -= sampleCount;

    return MA_SUCCESS;
}

float msigvis_channel_get_sample_f32(msigvis_channel* pChannel, ma_uint32 iSample)
{
    switch (pChannel->format)
    {
        case ma_format_f32: return *((float*)pChannel->pBuffer + iSample);
        default: return 0;
    }
}

#endif