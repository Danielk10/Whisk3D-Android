// ============================================================================
//  W3dAudioAndroid.cpp - Backend de audio OpenSL ES nativo para Android.
//  Alimenta el mixer de software de Whisk3D con baja latencia y sin dependencias.
// ============================================================================
#if defined(W3D_ENABLE_AUDIO) && defined(__ANDROID__)

#include "W3dAudio.h"
#include "W3dAudioBackend.h"
#include "base/w3dlog.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <pthread.h>
#include <string.h>

namespace w3dEngine {

static SLObjectItf s_engineObject = NULL;
static SLEngineItf s_engineEngine = NULL;
static SLObjectItf s_outputMixObject = NULL;
static SLObjectItf s_playerObject = NULL;
static SLPlayItf s_playerPlay = NULL;
static SLAndroidSimpleBufferQueueItf s_playerBufferQueue = NULL;

static pthread_mutex_t s_audioMutex = PTHREAD_MUTEX_INITIALIZER;
static const int BUFFER_FRAMES = 1024;
static const int BUFFER_COUNT = 2;
static short s_buffers[BUFFER_COUNT][BUFFER_FRAMES * 2]; // stereo 16-bit
static int s_currentBuffer = 0;

static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* /*context*/) {
    short* buf = s_buffers[s_currentBuffer];
    s_currentBuffer = (s_currentBuffer + 1) % BUFFER_COUNT;

    // Mezclar audio desde el motor de Whisk3D
    W3dAudioMix(buf, BUFFER_FRAMES);

    (*bq)->Enqueue(bq, buf, BUFFER_FRAMES * 2 * sizeof(short));
}

bool W3dAudioBackendInit(int sampleRate) {
    SLresult result;

    result = slCreateEngine(&s_engineObject, 0, NULL, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*s_engineObject)->Realize(s_engineObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*s_engineObject)->GetInterface(s_engineObject, SL_IID_ENGINE, &s_engineEngine);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*s_engineEngine)->CreateOutputMix(s_engineEngine, &s_outputMixObject, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*s_outputMixObject)->Realize(s_outputMixObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        BUFFER_COUNT
    };

    SLuint32 sr = SL_SAMPLINGRATE_44_1;
    if (sampleRate == 22050) sr = SL_SAMPLINGRATE_22_05;
    else if (sampleRate == 48000) sr = SL_SAMPLINGRATE_48;

    SLDataFormat_PCM format_pcm = {
        SL_DATAFORMAT_PCM,
        2, // Stereo
        sr,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
        SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataSource audioSrc = { &loc_bufq, &format_pcm };

    SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX, s_outputMixObject };
    SLDataSink audioSnk = { &loc_outmix, NULL };

    const SLInterfaceID ids[2] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_PLAY };
    const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

    result = (*s_engineEngine)->CreateAudioPlayer(s_engineEngine, &s_playerObject, &audioSrc, &audioSnk, 2, ids, req);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*s_playerObject)->Realize(s_playerObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*s_playerObject)->GetInterface(s_playerObject, SL_IID_PLAY, &s_playerPlay);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*s_playerObject)->GetInterface(s_playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &s_playerBufferQueue);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*s_playerBufferQueue)->RegisterCallback(s_playerBufferQueue, bqPlayerCallback, NULL);
    if (result != SL_RESULT_SUCCESS) return false;

    memset(s_buffers, 0, sizeof(s_buffers));
    for (int i = 0; i < BUFFER_COUNT; i++) {
        (*s_playerBufferQueue)->Enqueue(s_playerBufferQueue, s_buffers[i], BUFFER_FRAMES * 2 * sizeof(short));
    }

    result = (*s_playerPlay)->SetPlayState(s_playerPlay, SL_PLAYSTATE_PLAYING);
    if (result != SL_RESULT_SUCCESS) return false;

    w3dLog("[audio] OpenSL ES backend inicializado a 44100Hz stereo");
    return true;
}

void W3dAudioBackendShutdown() {
    if (s_playerObject) {
        (*s_playerPlay)->SetPlayState(s_playerPlay, SL_PLAYSTATE_STOPPED);
        (*s_playerObject)->Destroy(s_playerObject);
        s_playerObject = NULL;
        s_playerPlay = NULL;
        s_playerBufferQueue = NULL;
    }
    if (s_outputMixObject) {
        (*s_outputMixObject)->Destroy(s_outputMixObject);
        s_outputMixObject = NULL;
    }
    if (s_engineObject) {
        (*s_engineObject)->Destroy(s_engineObject);
        s_engineObject = NULL;
        s_engineEngine = NULL;
    }
}

void W3dAudioBackendLock() {
    pthread_mutex_lock(&s_audioMutex);
}

void W3dAudioBackendUnlock() {
    pthread_mutex_unlock(&s_audioMutex);
}

} // namespace w3dEngine

#endif // W3D_ENABLE_AUDIO && __ANDROID__
