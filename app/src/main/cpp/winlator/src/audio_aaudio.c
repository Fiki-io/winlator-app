#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <jni.h>
#include <stdint.h>
#include <stdbool.h>

#include "winlator.h"
#include "audio_aaudio.h"

// AAudio types and definitions
typedef struct AAudioStreamStruct AAudioStream;
typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder;

typedef int32_t aaudio_result_t;
typedef int32_t aaudio_stream_state_t;
typedef int32_t aaudio_direction_t;
typedef int32_t aaudio_format_t;
typedef int32_t aaudio_sharing_mode_t;
typedef int32_t aaudio_performance_mode_t;

#define AAUDIO_OK 0
#define AAUDIO_DIRECTION_OUTPUT 0
#define AAUDIO_FORMAT_PCM_I16 1
#define AAUDIO_FORMAT_PCM_FLOAT 2
#define AAUDIO_SHARING_MODE_EXCLUSIVE 0
#define AAUDIO_SHARING_MODE_SHARED 1
#define AAUDIO_PERFORMANCE_MODE_NONE 10
#define AAUDIO_PERFORMANCE_MODE_POWER_SAVING 11
#define AAUDIO_PERFORMANCE_MODE_LOW_LATENCY 12
#define AAUDIO_STREAM_STATE_PLAYING 4

// Function pointer definitions
typedef aaudio_result_t (*pfn_createStreamBuilder)(AAudioStreamBuilder**);
typedef void (*pfn_builderSetDirection)(AAudioStreamBuilder*, aaudio_direction_t);
typedef void (*pfn_builderSetSampleRate)(AAudioStreamBuilder*, int32_t);
typedef void (*pfn_builderSetChannelCount)(AAudioStreamBuilder*, int32_t);
typedef void (*pfn_builderSetFormat)(AAudioStreamBuilder*, aaudio_format_t);
typedef void (*pfn_builderSetSharingMode)(AAudioStreamBuilder*, aaudio_sharing_mode_t);
typedef void (*pfn_builderSetPerformanceMode)(AAudioStreamBuilder*, aaudio_performance_mode_t);
typedef void (*pfn_builderSetBufferCapacityInFrames)(AAudioStreamBuilder*, int32_t);
typedef aaudio_result_t (*pfn_builderOpenStream)(AAudioStreamBuilder*, AAudioStream**);
typedef aaudio_result_t (*pfn_builderDelete)(AAudioStreamBuilder*);

typedef aaudio_result_t (*pfn_streamRequestStart)(AAudioStream*);
typedef aaudio_result_t (*pfn_streamRequestPause)(AAudioStream*);
typedef aaudio_result_t (*pfn_streamRequestFlush)(AAudioStream*);
typedef aaudio_result_t (*pfn_streamRequestStop)(AAudioStream*);
typedef aaudio_result_t (*pfn_streamClose)(AAudioStream*);
typedef aaudio_result_t (*pfn_streamWrite)(AAudioStream*, const void*, int32_t, int64_t);
typedef int32_t (*pfn_streamGetFramesPerBurst)(AAudioStream*);
typedef int32_t (*pfn_streamGetBufferCapacityInFrames)(AAudioStream*);
typedef int32_t (*pfn_streamGetBufferSizeInFrames)(AAudioStream*);
typedef aaudio_result_t (*pfn_streamSetBufferSizeInFrames)(AAudioStream*, int32_t);
typedef int32_t (*pfn_streamGetXRunCount)(AAudioStream*);
typedef aaudio_stream_state_t (*pfn_streamGetState)(AAudioStream*);

typedef struct {
    void* libHandle;
    pfn_createStreamBuilder createStreamBuilder;
    pfn_builderSetDirection builderSetDirection;
    pfn_builderSetSampleRate builderSetSampleRate;
    pfn_builderSetChannelCount builderSetChannelCount;
    pfn_builderSetFormat builderSetFormat;
    pfn_builderSetSharingMode builderSetSharingMode;
    pfn_builderSetPerformanceMode builderSetPerformanceMode;
    pfn_builderSetBufferCapacityInFrames builderSetBufferCapacityInFrames;
    pfn_builderOpenStream builderOpenStream;
    pfn_builderDelete builderDelete;

    pfn_streamRequestStart streamRequestStart;
    pfn_streamRequestPause streamRequestPause;
    pfn_streamRequestFlush streamRequestFlush;
    pfn_streamRequestStop streamRequestStop;
    pfn_streamClose streamClose;
    pfn_streamWrite streamWrite;
    pfn_streamGetFramesPerBurst streamGetFramesPerBurst;
    pfn_streamGetBufferCapacityInFrames streamGetBufferCapacityInFrames;
    pfn_streamGetBufferSizeInFrames streamGetBufferSizeInFrames;
    pfn_streamSetBufferSizeInFrames streamSetBufferSizeInFrames;
    pfn_streamGetXRunCount streamGetXRunCount;
    pfn_streamGetState streamGetState;
} AAudioAPI;

static AAudioAPI g_aaudio = {0};
static bool g_aaudioLoaded = false;
static pthread_mutex_t g_initMutex = PTHREAD_MUTEX_INITIALIZER;

static bool loadAAudioLibrary(void) {
    pthread_mutex_lock(&g_initMutex);
    if (g_aaudioLoaded) {
        pthread_mutex_unlock(&g_initMutex);
        return g_aaudio.libHandle != NULL;
    }

    g_aaudioLoaded = true;
    void* handle = dlopen("libaaudio.so", RTLD_NOW);
    if (!handle) {
        pthread_mutex_unlock(&g_initMutex);
        return false;
    }

    g_aaudio.libHandle = handle;
    g_aaudio.createStreamBuilder = (pfn_createStreamBuilder)dlsym(handle, "AAudio_createStreamBuilder");
    g_aaudio.builderSetDirection = (pfn_builderSetDirection)dlsym(handle, "AAudioStreamBuilder_setDirection");
    g_aaudio.builderSetSampleRate = (pfn_builderSetSampleRate)dlsym(handle, "AAudioStreamBuilder_setSampleRate");
    g_aaudio.builderSetChannelCount = (pfn_builderSetChannelCount)dlsym(handle, "AAudioStreamBuilder_setChannelCount");
    g_aaudio.builderSetFormat = (pfn_builderSetFormat)dlsym(handle, "AAudioStreamBuilder_setFormat");
    g_aaudio.builderSetSharingMode = (pfn_builderSetSharingMode)dlsym(handle, "AAudioStreamBuilder_setSharingMode");
    g_aaudio.builderSetPerformanceMode = (pfn_builderSetPerformanceMode)dlsym(handle, "AAudioStreamBuilder_setPerformanceMode");
    g_aaudio.builderSetBufferCapacityInFrames = (pfn_builderSetBufferCapacityInFrames)dlsym(handle, "AAudioStreamBuilder_setBufferCapacityInFrames");
    g_aaudio.builderOpenStream = (pfn_builderOpenStream)dlsym(handle, "AAudioStreamBuilder_openStream");
    g_aaudio.builderDelete = (pfn_builderDelete)dlsym(handle, "AAudioStreamBuilder_delete");

    g_aaudio.streamRequestStart = (pfn_streamRequestStart)dlsym(handle, "AAudioStream_requestStart");
    g_aaudio.streamRequestPause = (pfn_streamRequestPause)dlsym(handle, "AAudioStream_requestPause");
    g_aaudio.streamRequestFlush = (pfn_streamRequestFlush)dlsym(handle, "AAudioStream_requestFlush");
    g_aaudio.streamRequestStop = (pfn_streamRequestStop)dlsym(handle, "AAudioStream_requestStop");
    g_aaudio.streamClose = (pfn_streamClose)dlsym(handle, "AAudioStream_close");
    g_aaudio.streamWrite = (pfn_streamWrite)dlsym(handle, "AAudioStream_write");
    g_aaudio.streamGetFramesPerBurst = (pfn_streamGetFramesPerBurst)dlsym(handle, "AAudioStream_getFramesPerBurst");
    g_aaudio.streamGetBufferCapacityInFrames = (pfn_streamGetBufferCapacityInFrames)dlsym(handle, "AAudioStream_getBufferCapacityInFrames");
    g_aaudio.streamGetBufferSizeInFrames = (pfn_streamGetBufferSizeInFrames)dlsym(handle, "AAudioStream_getBufferSizeInFrames");
    g_aaudio.streamSetBufferSizeInFrames = (pfn_streamSetBufferSizeInFrames)dlsym(handle, "AAudioStream_setBufferSizeInFrames");
    g_aaudio.streamGetXRunCount = (pfn_streamGetXRunCount)dlsym(handle, "AAudioStream_getXRunCount");
    g_aaudio.streamGetState = (pfn_streamGetState)dlsym(handle, "AAudioStream_getState");

    pthread_mutex_unlock(&g_initMutex);
    return g_aaudio.createStreamBuilder && g_aaudio.builderOpenStream && g_aaudio.streamWrite;
}

typedef struct {
    AAudioStream* stream;
    int32_t sampleRate;
    int32_t channels;
    int32_t bytesPerSample;
    int32_t frameBytes;
    int32_t burstFrames;
    int32_t targetBufferFrames;
    int32_t previousXRunCount;
    int32_t stableWriteCount;
    int64_t positionFrames;
    uint8_t formatType; // 0: U8, 1: S16LE, 2: S16BE, 3: FLOATLE, 4: FLOATBE
    float volume;
    int16_t* conversionBuffer;
    int32_t conversionBufferCapacity;
    pthread_mutex_t lock;
} NativeAudioContext;

AAudioHandle AAudio_createStream(int32_t sampleRate, int32_t channels,
                                int32_t formatType, int32_t performanceMode,
                                int32_t latencyMillis) {
    if (!loadAAudioLibrary()) return NULL;

    AAudioStreamBuilder* builder = NULL;
    aaudio_result_t res = g_aaudio.createStreamBuilder(&builder);
    if (res != AAUDIO_OK || !builder) return NULL;

    g_aaudio.builderSetDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    g_aaudio.builderSetSampleRate(builder, sampleRate);
    g_aaudio.builderSetChannelCount(builder, channels);

    aaudio_format_t aaudioFormat = AAUDIO_FORMAT_PCM_I16;
    int bytesPerSample = 2;
    if (formatType == 3 || formatType == 4) { // FLOATLE or FLOATBE
        aaudioFormat = AAUDIO_FORMAT_PCM_FLOAT;
        bytesPerSample = 4;
    }
    g_aaudio.builderSetFormat(builder, aaudioFormat);

    aaudio_performance_mode_t perfMode = AAUDIO_PERFORMANCE_MODE_LOW_LATENCY;
    if (performanceMode == 0) perfMode = AAUDIO_PERFORMANCE_MODE_NONE;
    else if (performanceMode == 2) perfMode = AAUDIO_PERFORMANCE_MODE_POWER_SAVING;
    g_aaudio.builderSetPerformanceMode(builder, perfMode);

    // Attempt EXCLUSIVE mode first for lowest latency (hardware MMAP path)
    g_aaudio.builderSetSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);

    AAudioStream* stream = NULL;
    res = g_aaudio.builderOpenStream(builder, &stream);
    if (res != AAUDIO_OK || !stream) {
        // Fall back to SHARED mode if device driver does not permit exclusive mode
        g_aaudio.builderSetSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
        res = g_aaudio.builderOpenStream(builder, &stream);
        if (res != AAUDIO_OK || !stream) {
            g_aaudio.builderDelete(builder);
            return NULL;
        }
    }
    g_aaudio.builderDelete(builder);

    NativeAudioContext* ctx = (NativeAudioContext*)calloc(1, sizeof(NativeAudioContext));
    if (!ctx) {
        g_aaudio.streamClose(stream);
        return NULL;
    }

    ctx->stream = stream;
    ctx->sampleRate = sampleRate;
    ctx->channels = channels;
    ctx->bytesPerSample = bytesPerSample;
    ctx->frameBytes = channels * bytesPerSample;
    ctx->formatType = (uint8_t)formatType;
    ctx->volume = 1.0f;
    ctx->burstFrames = g_aaudio.streamGetFramesPerBurst(stream);
    if (ctx->burstFrames <= 0) ctx->burstFrames = 192;

    // Calculate target low-latency buffer frames from latencyMillis
    int32_t requestedFrames = (int32_t)((int64_t)latencyMillis * sampleRate / 1000);
    // Align to burst multiple and enforce minimum 2 bursts
    int32_t bursts = (requestedFrames + ctx->burstFrames - 1) / ctx->burstFrames;
    if (bursts < 2) bursts = 2;
    ctx->targetBufferFrames = bursts * ctx->burstFrames;

    int32_t capacity = g_aaudio.streamGetBufferCapacityInFrames(stream);
    if (ctx->targetBufferFrames > capacity) ctx->targetBufferFrames = capacity;

    g_aaudio.streamSetBufferSizeInFrames(stream, ctx->targetBufferFrames);
    pthread_mutex_init(&ctx->lock, NULL);

    return (AAudioHandle)ctx;
}

bool AAudio_start(AAudioHandle handle) {
    if (!handle || !g_aaudio.streamRequestStart) return false;
    NativeAudioContext* ctx = (NativeAudioContext*)handle;
    pthread_mutex_lock(&ctx->lock);
    aaudio_result_t res = g_aaudio.streamRequestStart(ctx->stream);
    pthread_mutex_unlock(&ctx->lock);
    return res == AAUDIO_OK;
}

bool AAudio_stop(AAudioHandle handle) {
    if (!handle || !g_aaudio.streamRequestStop) return false;
    NativeAudioContext* ctx = (NativeAudioContext*)handle;
    pthread_mutex_lock(&ctx->lock);
    aaudio_result_t res = g_aaudio.streamRequestStop(ctx->stream);
    if (g_aaudio.streamRequestFlush) g_aaudio.streamRequestFlush(ctx->stream);
    pthread_mutex_unlock(&ctx->lock);
    return res == AAUDIO_OK;
}

bool AAudio_pause(AAudioHandle handle) {
    if (!handle || !g_aaudio.streamRequestPause) return false;
    NativeAudioContext* ctx = (NativeAudioContext*)handle;
    pthread_mutex_lock(&ctx->lock);
    aaudio_result_t res = g_aaudio.streamRequestPause(ctx->stream);
    pthread_mutex_unlock(&ctx->lock);
    return res == AAUDIO_OK;
}

bool AAudio_flush(AAudioHandle handle) {
    if (!handle || !g_aaudio.streamRequestFlush) return false;
    NativeAudioContext* ctx = (NativeAudioContext*)handle;
    pthread_mutex_lock(&ctx->lock);
    aaudio_result_t res = g_aaudio.streamRequestFlush(ctx->stream);
    pthread_mutex_unlock(&ctx->lock);
    return res == AAUDIO_OK;
}

void AAudio_release(AAudioHandle handle) {
    if (!handle) return;
    NativeAudioContext* ctx = (NativeAudioContext*)handle;
    pthread_mutex_lock(&ctx->lock);
    if (ctx->stream) {
        g_aaudio.streamRequestStop(ctx->stream);
        g_aaudio.streamClose(ctx->stream);
        ctx->stream = NULL;
    }
    if (ctx->conversionBuffer) {
        free(ctx->conversionBuffer);
        ctx->conversionBuffer = NULL;
    }
    pthread_mutex_unlock(&ctx->lock);
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
}

int32_t AAudio_write(AAudioHandle handle, const void* buffer, int32_t offsetInBytes, int32_t lengthInBytes) {
    if (!handle || !buffer || lengthInBytes <= 0) return 0;
    NativeAudioContext* ctx = (NativeAudioContext*)handle;

    pthread_mutex_lock(&ctx->lock);
    if (!ctx->stream) {
        pthread_mutex_unlock(&ctx->lock);
        return 0;
    }

    const uint8_t* srcData = ((const uint8_t*)buffer) + offsetInBytes;
    const void* writePtr = srcData;
    int32_t numFrames = 0;

    if (ctx->formatType == 0) { // U8 conversion to S16
        numFrames = lengthInBytes / ctx->channels;
        int32_t totalSamples = lengthInBytes;
        if (ctx->conversionBufferCapacity < totalSamples) {
            ctx->conversionBuffer = (int16_t*)realloc(ctx->conversionBuffer, totalSamples * sizeof(int16_t));
            ctx->conversionBufferCapacity = totalSamples;
        }
        for (int32_t i = 0; i < totalSamples; i++) {
            ctx->conversionBuffer[i] = (int16_t)(((int16_t)srcData[i] - 128) << 8);
        }
        writePtr = ctx->conversionBuffer;
    } else {
        numFrames = lengthInBytes / ctx->frameBytes;
    }

    // 10ms timeout prevents blocking socket thread
    int64_t timeoutNanos = 10000000LL;
    int32_t framesWritten = g_aaudio.streamWrite(ctx->stream, writePtr, numFrames, timeoutNanos);

    if (framesWritten > 0) {
        ctx->positionFrames += framesWritten;

        // Adaptive Buffer Sizing with Automatic Decay (Anti-Ratchet Bug Fix)
        int32_t currentXRun = g_aaudio.streamGetXRunCount(ctx->stream);
        if (currentXRun > ctx->previousXRunCount) {
            ctx->previousXRunCount = currentXRun;
            ctx->stableWriteCount = 0;

            int32_t currentBuffer = g_aaudio.streamGetBufferSizeInFrames(ctx->stream);
            int32_t capacity = g_aaudio.streamGetBufferCapacityInFrames(ctx->stream);
            int32_t maxAllowed = ctx->burstFrames * 4;
            if (maxAllowed > capacity) maxAllowed = capacity;

            if (currentBuffer < maxAllowed) {
                g_aaudio.streamSetBufferSizeInFrames(ctx->stream, currentBuffer + ctx->burstFrames);
            }
        } else {
            ctx->stableWriteCount++;
            // After 100 successful writes (~1 second), decay buffer size back down towards target
            if (ctx->stableWriteCount > 100) {
                ctx->stableWriteCount = 0;
                int32_t currentBuffer = g_aaudio.streamGetBufferSizeInFrames(ctx->stream);
                if (currentBuffer > ctx->targetBufferFrames) {
                    g_aaudio.streamSetBufferSizeInFrames(ctx->stream, currentBuffer - ctx->burstFrames);
                }
            }
        }
    }

    pthread_mutex_unlock(&ctx->lock);
    return framesWritten > 0 ? framesWritten : 0;
}

int64_t AAudio_getFramePosition(AAudioHandle handle) {
    if (!handle) return 0;
    NativeAudioContext* ctx = (NativeAudioContext*)handle;
    return ctx->positionFrames;
}

void AAudio_setVolume(AAudioHandle handle, float volume) {
    if (!handle) return;
    NativeAudioContext* ctx = (NativeAudioContext*)handle;
    ctx->volume = volume;
}

// JNI Wrappers
JNIEXPORT jlong JNICALL
Java_com_winlator_alsaserver_NativeAAudio_createStream(
    JNIEnv* env, jclass cls, jint sampleRate, jint channels,
    jint formatType, jint performanceMode, jint latencyMillis) {
    (void)env;
    (void)cls;
    return (jlong)AAudio_createStream(sampleRate, channels, formatType, performanceMode, latencyMillis);
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_alsaserver_NativeAAudio_start(JNIEnv* env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    return AAudio_start((AAudioHandle)handle) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_alsaserver_NativeAAudio_stop(JNIEnv* env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    return AAudio_stop((AAudioHandle)handle) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_alsaserver_NativeAAudio_pause(JNIEnv* env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    return AAudio_pause((AAudioHandle)handle) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_alsaserver_NativeAAudio_flush(JNIEnv* env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    return AAudio_flush((AAudioHandle)handle) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_winlator_alsaserver_NativeAAudio_release(JNIEnv* env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    AAudio_release((AAudioHandle)handle);
}

JNIEXPORT jint JNICALL
Java_com_winlator_alsaserver_NativeAAudio_write(
    JNIEnv* env, jclass cls, jlong handle, jobject byteBuffer, jint offsetInBytes, jint lengthInBytes) {
    (void)cls;
    void* bufferAddr = (*env)->GetDirectBufferAddress(env, byteBuffer);
    if (!bufferAddr) return 0;
    return AAudio_write((AAudioHandle)handle, bufferAddr, offsetInBytes, lengthInBytes);
}

JNIEXPORT jlong JNICALL
Java_com_winlator_alsaserver_NativeAAudio_getFramePosition(JNIEnv* env, jclass cls, jlong handle) {
    (void)env;
    (void)cls;
    return (jlong)AAudio_getFramePosition((AAudioHandle)handle);
}

JNIEXPORT void JNICALL
Java_com_winlator_alsaserver_NativeAAudio_setVolume(JNIEnv* env, jclass cls, jlong handle, jfloat volume) {
    (void)env;
    (void)cls;
    AAudio_setVolume((AAudioHandle)handle, (float)volume);
}

