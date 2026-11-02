#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <jni.h>
#include <stdint.h>
#include <stdbool.h>

#include "winlator.h"
#include "native_sync.h"

#define FENCE_BUCKET_COUNT 256

typedef struct NativeFenceEntry {
    uint32_t id;
    bool triggered;
    struct NativeFenceEntry* next;
} NativeFenceEntry;

static NativeFenceEntry* g_fenceBuckets[FENCE_BUCKET_COUNT] = {0};
static pthread_mutex_t g_syncLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_syncCond = PTHREAD_COND_INITIALIZER;

static inline uint32_t hashFenceId(uint32_t id) {
    return (id ^ (id >> 8) ^ (id >> 16)) & (FENCE_BUCKET_COUNT - 1);
}

static NativeFenceEntry* findFenceLocked(uint32_t id) {
    uint32_t bucket = hashFenceId(id);
    NativeFenceEntry* entry = g_fenceBuckets[bucket];
    while (entry) {
        if (entry->id == id) return entry;
        entry = entry->next;
    }
    return NULL;
}

void NativeSync_createFence(uint32_t id, bool initiallyTriggered) {
    pthread_mutex_lock(&g_syncLock);
    uint32_t bucket = hashFenceId(id);
    NativeFenceEntry* entry = g_fenceBuckets[bucket];
    while (entry) {
        if (entry->id == id) {
            entry->triggered = initiallyTriggered;
            pthread_mutex_unlock(&g_syncLock);
            return;
        }
        entry = entry->next;
    }

    NativeFenceEntry* newEntry = (NativeFenceEntry*)malloc(sizeof(NativeFenceEntry));
    newEntry->id = id;
    newEntry->triggered = initiallyTriggered;
    newEntry->next = g_fenceBuckets[bucket];
    g_fenceBuckets[bucket] = newEntry;
    pthread_mutex_unlock(&g_syncLock);
}

void NativeSync_triggerFence(uint32_t id) {
    pthread_mutex_lock(&g_syncLock);
    NativeFenceEntry* entry = findFenceLocked(id);
    if (entry) {
        entry->triggered = true;
        pthread_cond_broadcast(&g_syncCond);
    }
    pthread_mutex_unlock(&g_syncLock);
}

void NativeSync_resetFence(uint32_t id) {
    pthread_mutex_lock(&g_syncLock);
    NativeFenceEntry* entry = findFenceLocked(id);
    if (entry) {
        entry->triggered = false;
    }
    pthread_mutex_unlock(&g_syncLock);
}

void NativeSync_destroyFence(uint32_t id) {
    pthread_mutex_lock(&g_syncLock);
    uint32_t bucket = hashFenceId(id);
    NativeFenceEntry** pNext = &g_fenceBuckets[bucket];
    while (*pNext) {
        if ((*pNext)->id == id) {
            NativeFenceEntry* toFree = *pNext;
            *pNext = toFree->next;
            free(toFree);
            break;
        }
        pNext = &(*pNext)->next;
    }
    pthread_mutex_unlock(&g_syncLock);
}

bool NativeSync_isTriggered(uint32_t id) {
    pthread_mutex_lock(&g_syncLock);
    NativeFenceEntry* entry = findFenceLocked(id);
    bool res = entry ? entry->triggered : false;
    pthread_mutex_unlock(&g_syncLock);
    return res;
}

bool NativeSync_hasFence(uint32_t id) {
    pthread_mutex_lock(&g_syncLock);
    bool has = findFenceLocked(id) != NULL;
    pthread_mutex_unlock(&g_syncLock);
    return has;
}

static bool isAnyTriggeredLocked(const uint32_t* ids, int count) {
    for (int i = 0; i < count; i++) {
        NativeFenceEntry* entry = findFenceLocked(ids[i]);
        if (entry && entry->triggered) return true;
    }
    return false;
}

bool NativeSync_awaitFence(const uint32_t* ids, int count, uint32_t timeoutMs) {
    if (count <= 0) return true;

    pthread_mutex_lock(&g_syncLock);
    if (isAnyTriggeredLocked(ids, count)) {
        pthread_mutex_unlock(&g_syncLock);
        return true;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t nsec = (uint64_t)ts.tv_nsec + ((uint64_t)timeoutMs * 1000000ULL);
    ts.tv_sec += nsec / 1000000000ULL;
    ts.tv_nsec = nsec % 1000000000ULL;

    while (!isAnyTriggeredLocked(ids, count)) {
        int rc = pthread_cond_timedwait(&g_syncCond, &g_syncLock, &ts);
        if (rc != 0) break;
    }

    bool triggered = isAnyTriggeredLocked(ids, count);
    pthread_mutex_unlock(&g_syncLock);
    return triggered;
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_extensions_NativeSync_createFence(
    JNIEnv* env, jclass cls, jint id, jboolean initiallyTriggered) {
    (void)env;
    (void)cls;
    NativeSync_createFence((uint32_t)id, (bool)initiallyTriggered);
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_extensions_NativeSync_triggerFence(
    JNIEnv* env, jclass cls, jint id) {
    (void)env;
    (void)cls;
    NativeSync_triggerFence((uint32_t)id);
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_extensions_NativeSync_resetFence(
    JNIEnv* env, jclass cls, jint id) {
    (void)env;
    (void)cls;
    NativeSync_resetFence((uint32_t)id);
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_extensions_NativeSync_destroyFence(
    JNIEnv* env, jclass cls, jint id) {
    (void)env;
    (void)cls;
    NativeSync_destroyFence((uint32_t)id);
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xserver_extensions_NativeSync_isTriggered(
    JNIEnv* env, jclass cls, jint id) {
    (void)env;
    (void)cls;
    return NativeSync_isTriggered((uint32_t)id) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xserver_extensions_NativeSync_hasFence(
    JNIEnv* env, jclass cls, jint id) {
    (void)env;
    (void)cls;
    return NativeSync_hasFence((uint32_t)id) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xserver_extensions_NativeSync_awaitFence(
    JNIEnv* env, jclass cls, jintArray idsArray, jint count, jint timeoutMs) {
    (void)cls;
    jint* idPtr = (*env)->GetIntArrayElements(env, idsArray, NULL);
    if (!idPtr) return JNI_FALSE;

    bool result = NativeSync_awaitFence((const uint32_t*)idPtr, count, (uint32_t)timeoutMs);

    (*env)->ReleaseIntArrayElements(env, idsArray, idPtr, JNI_ABORT);
    return result ? JNI_TRUE : JNI_FALSE;
}

