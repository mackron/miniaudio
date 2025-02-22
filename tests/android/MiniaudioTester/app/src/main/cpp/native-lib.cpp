#include <jni.h>
#include <string>

#define MA_DEBUG_OUTPUT
#include "../../../../../../../miniaudio.c" /* Android projects have very deep folder structures... */

typedef enum
{
    BACKEND_AUTO,
    BACKEND_AAUDIO,
    BACKEND_OPENSL
} backend_choice_t;

typedef struct
{
    ma_device device;
    ma_waveform waveform;
    bool hasDevice;
    bool hasError;              /* Will be set to true if something went wrong. */
    std::string errorMessage;   /* Will be an empty string if there is no error message. */
} audio_state_t;

static void audio_state_set_error(audio_state_t* pAudioState, const char* pMessage)
{
    assert(pAudioState != nullptr);

    pAudioState->hasError = true;
    pAudioState->errorMessage = pMessage;
}


static void data_callback(ma_device* pDevice, void* pFramesOut, const void* pFramesIn, ma_uint32 frameCount)
{
    auto* pAudioState = (audio_state_t*)pDevice->pUserData;
    assert(pAudioState != nullptr);

    ma_waveform_read_pcm_frames(&pAudioState->waveform, pFramesOut, frameCount, nullptr);
}


extern "C"
JNIEXPORT jlong JNICALL
Java_io_miniaud_miniaudiotester_MainActivity_UninitializeAudio(JNIEnv *env, jobject, jlong audioState)
{
    auto* pAudioState = (audio_state_t*)audioState;
    if (pAudioState == nullptr) {
        return 0;
    }

    if (pAudioState->hasDevice) {
        ma_device_uninit(&pAudioState->device);
        ma_waveform_uninit(&pAudioState->waveform);
        pAudioState->hasDevice = false;
    }

    pAudioState->hasError = false;
    pAudioState->errorMessage = "";

    return (jlong)pAudioState;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_io_miniaud_miniaudiotester_MainActivity_PlayAudio(JNIEnv *env, jobject, jlong audioState, int backend)
{
    auto* pAudioState = (audio_state_t*)audioState;
    ma_result result;

    if (pAudioState == nullptr) {
        pAudioState = new audio_state_t;
        pAudioState->hasDevice = false;
        pAudioState->hasError  = false;
    }

    /* If we don't have a device, create one. */
    if (!pAudioState->hasDevice) {
        ma_context_config contextConfig = ma_context_config_init();
        ma_backend pBackends[1];
        size_t backendCount;

        if (backend == BACKEND_AUTO) {
            backendCount = 0;
        } else {
            backendCount = 1;
            if (backend == BACKEND_AAUDIO) {
                pBackends[0] = ma_backend_aaudio;
            } else if (backend == BACKEND_OPENSL) {
                pBackends[0] = ma_backend_opensl;
            } else {
                backendCount = 0;
            }
        }

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.dataCallback = data_callback;
        deviceConfig.pUserData = pAudioState;

        result = ma_device_init_ex((backendCount == 0) ? nullptr : pBackends, backendCount, &contextConfig, &deviceConfig, &pAudioState->device);
        if (result != MA_SUCCESS) {
            audio_state_set_error(pAudioState, (std::string("Failed to initialize device. ") + ma_result_description(result)).c_str());
            pAudioState->hasDevice = false;
        }

        /* Before starting the device we will need a waveform object. This should never fail to initialize. */
        ma_waveform_config waveformConfig = ma_waveform_config_init(pAudioState->device.playback.format, pAudioState->device.playback.channels, pAudioState->device.sampleRate, ma_waveform_type_sine, 0.2, 400);
        ma_waveform_init(&waveformConfig, &pAudioState->waveform);

        pAudioState->hasDevice = true;
    }

    /* At this point we should have a device. Start it. */
    result = ma_device_start(&pAudioState->device);
    if (result != MA_SUCCESS) {
        audio_state_set_error(pAudioState, (std::string("Failed to start device. ") + ma_result_description(result)).c_str());
    }

    return (jlong)pAudioState;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_io_miniaud_miniaudiotester_MainActivity_PauseAudio(JNIEnv *env, jobject, jlong audioState)
{
    auto* pAudioState = (audio_state_t*)audioState;
    if (pAudioState == nullptr) {
        return true;
    }

    if (!pAudioState->hasError) {
        if (pAudioState->hasDevice) {
            ma_result result = ma_device_stop(&pAudioState->device);
            if (result != MA_SUCCESS) {
                audio_state_set_error(pAudioState, ma_result_description(result));
            }
        } else {
            audio_state_set_error(pAudioState, "Trying to pause audio, but there is no device.");
        }
    }

    return (jlong)pAudioState;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_io_miniaud_miniaudiotester_MainActivity_HasAudioError(JNIEnv *env, jobject, jlong audioState)
{
    auto* pAudioState = (audio_state_t*)audioState;
    if (pAudioState == nullptr) {
        return true;
    }

    return pAudioState->hasError;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_io_miniaud_miniaudiotester_MainActivity_GetAudioError(JNIEnv *env, jobject, jlong audioState)
{
    auto* pAudioState = (audio_state_t*)audioState;
    if (pAudioState == nullptr) {
        return env->NewStringUTF("Out of memory");
    }

    return env->NewStringUTF(pAudioState->errorMessage.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_io_miniaud_miniaudiotester_MainActivity_DeleteAudioState(JNIEnv *env, jobject thiz, jlong audioState)
{
    Java_io_miniaud_miniaudiotester_MainActivity_UninitializeAudio(env, thiz, audioState);
    delete (audio_state_t*)audioState;
}