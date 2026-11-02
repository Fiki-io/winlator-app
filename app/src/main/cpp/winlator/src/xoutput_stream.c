#include <malloc.h>
#include <string.h>
#include <jni.h>
#include <unistd.h>
#include <stdint.h>

#include "winlator.h"
#include "socket_utils.h"

typedef struct XOutputStream {
    int fd;
    int ancillaryFd;
    struct {
        void* data;
        int capacity;
        int position;
        int limit;
    } buffer;
} XOutputStream;

static void ensureSpaceIsAvailable(XOutputStream* outputStream, int length) {
    if ((outputStream->buffer.capacity - outputStream->buffer.position) >= length) return;

    int newCapacity = outputStream->buffer.capacity + length;
    void* newData = realloc(outputStream->buffer.data, newCapacity);
    outputStream->buffer.capacity = newCapacity;
    outputStream->buffer.limit = newCapacity;
    outputStream->buffer.data = newData;
}

static XOutputStream* XOutputStream_allocate(int fd, int initialCapacity) {
    XOutputStream* outputStream = calloc(1, sizeof(XOutputStream));
    outputStream->fd = fd;
    outputStream->buffer.data = calloc(initialCapacity, 1);
    outputStream->buffer.capacity = initialCapacity;
    outputStream->buffer.limit = initialCapacity;
    return outputStream;
}

static void XOutputStream_destroy(XOutputStream* outputStream) {
    if (!outputStream) return;
    MEMFREE(outputStream->buffer.data);
    MEMFREE(outputStream);
}

static jboolean XOutputStream_send(XOutputStream* outputStream) {
    if (outputStream->buffer.position == 0) return JNI_TRUE;
    outputStream->buffer.limit = outputStream->buffer.position;
    outputStream->buffer.position = 0;

    int bytesSent;
    if (outputStream->ancillaryFd > 0) {
        bytesSent = send_fds(outputStream->fd, &outputStream->ancillaryFd, 1, outputStream->buffer.data, outputStream->buffer.limit);
        outputStream->ancillaryFd = 0;
    }
    else bytesSent = write(outputStream->fd, outputStream->buffer.data, outputStream->buffer.limit);

    outputStream->buffer.limit = outputStream->buffer.capacity;
    return bytesSent >= 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_xconnector_XOutputStream_nativeAllocate(JNIEnv *env, jobject obj, jint fd,
                                                          jint initialCapacity) {
    XOutputStream* outputStream = XOutputStream_allocate(fd, initialCapacity);
    return (jlong)outputStream;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_setAncillaryFd(jlong nativePtr, jint ancillaryFd) {
    ((XOutputStream*)nativePtr)->ancillaryFd = ancillaryFd;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeByte(jlong nativePtr, jbyte value) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 1);
    *(jbyte*)(outputStream->buffer.data + outputStream->buffer.position++) = value;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeShort(jlong nativePtr, jshort value) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 2);
    *(jshort*)(outputStream->buffer.data + outputStream->buffer.position) = value;
    outputStream->buffer.position += 2;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeInt(jlong nativePtr, jint value) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 4);
    *(jint*)(outputStream->buffer.data + outputStream->buffer.position) = value;
    outputStream->buffer.position += 4;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeLong(jlong nativePtr, jlong value) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 8);
    *(jlong*)(outputStream->buffer.data + outputStream->buffer.position) = value;
    outputStream->buffer.position += 8;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writePad(jlong nativePtr, jint length) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, length);
    memset(outputStream->buffer.data + outputStream->buffer.position, 0, length);
    outputStream->buffer.position += length;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeAt(JNIEnv *env, jclass obj,
                                                   jlong nativePtr, jint position, jbyteArray data) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    jbyte* dataPtr = (*env)->GetByteArrayElements(env, data, 0);
    jsize length = (*env)->GetArrayLength(env, data);
    memcpy(outputStream->buffer.data + position, dataPtr, length);
    (*env)->ReleaseByteArrayElements(env, data, dataPtr, JNI_ABORT);
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeByteBuffer(JNIEnv *env, jclass obj,
                                                           jlong nativePtr, jobject data,
                                                           jint offset, jint length) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    void* dataAddr = (*env)->GetDirectBufferAddress(env, data);
    ensureSpaceIsAvailable(outputStream, length);
    memcpy(outputStream->buffer.data + outputStream->buffer.position, dataAddr + offset, length);
    outputStream->buffer.position += length;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xconnector_XOutputStream_sendData(jlong nativePtr) {
    return XOutputStream_send((XOutputStream*)nativePtr);
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_destroy(jlong nativePtr) {
    XOutputStream_destroy((XOutputStream*)nativePtr);
}

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_XOutputStream_length(jlong nativePtr) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    return outputStream->buffer.position;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeInputDeviceEvent(
    jlong nativePtr, jbyte code, jbyte detail, jshort seq, jint timestamp,
    jint rootId, jint eventId, jint childId,
    jshort rootX, jshort rootY, jshort eventX, jshort eventY, jshort stateBits) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 32);
    uint8_t* p = (uint8_t*)(outputStream->buffer.data + outputStream->buffer.position);
    p[0] = (uint8_t)code;
    p[1] = (uint8_t)detail;
    *(uint16_t*)(p + 2) = (uint16_t)seq;
    *(uint32_t*)(p + 4) = (uint32_t)timestamp;
    *(uint32_t*)(p + 8) = (uint32_t)rootId;
    *(uint32_t*)(p + 12) = (uint32_t)eventId;
    *(uint32_t*)(p + 16) = (uint32_t)childId;
    *(int16_t*)(p + 20) = (int16_t)rootX;
    *(int16_t*)(p + 22) = (int16_t)rootY;
    *(int16_t*)(p + 24) = (int16_t)eventX;
    *(int16_t*)(p + 26) = (int16_t)eventY;
    *(uint16_t*)(p + 28) = (uint16_t)stateBits;
    p[30] = 1;
    p[31] = 0;
    outputStream->buffer.position += 32;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeByteArray(JNIEnv *env, jclass obj,
                                                          jlong nativePtr, jbyteArray data,
                                                          jint offset, jint length) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, length);
    (*env)->GetByteArrayRegion(env, data, offset, length,
                               (jbyte*)(outputStream->buffer.data + outputStream->buffer.position));
    outputStream->buffer.position += length;
}