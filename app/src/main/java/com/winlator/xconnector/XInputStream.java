package com.winlator.xconnector;

import com.winlator.xserver.XServer;

import java.nio.ByteBuffer;

import dalvik.annotation.optimization.CriticalNative;

public class XInputStream {
    private final long nativePtr;

    static {
        System.loadLibrary("winlator");
    }

    public XInputStream(int clientFd, int initialCapacity) {
        nativePtr = nativeAllocate(clientFd, initialCapacity);
    }

    public int readMoreData(boolean canReceiveAncillaryMessages) {
        return readMoreData(nativePtr, canReceiveAncillaryMessages);
    }

    public int getAncillaryFd() {
        return getAncillaryFd(nativePtr);
    }

    public int getActivePosition() {
        return getActivePosition(nativePtr);
    }

    public void setActivePosition(int activePosition) {
        setActivePosition(nativePtr, activePosition);
    }

    public int available() {
        return available(nativePtr);
    }

    public byte readByte() {
        return readByte(nativePtr);
    }

    public int readUnsignedByte() {
        return Byte.toUnsignedInt(readByte(nativePtr));
    }

    public short readShort() {
        return readShort(nativePtr);
    }

    public int readUnsignedShort() {
        return Short.toUnsignedInt(readShort(nativePtr));
    }

    public int readInt() {
        return readInt(nativePtr);
    }

    public long readUnsignedInt() {
        return Integer.toUnsignedLong(readInt(nativePtr));
    }

    public long readLong() {
        return readLong(nativePtr);
    }

    public void read(byte[] result) {
        read(result, 0, result.length);
    }

    public void read(byte[] result, int offset, int length) {
        readByteArray(nativePtr, result, offset, length);
    }

    public void readInts(int[] result, int offset, int count) {
        readIntArray(nativePtr, result, offset, count);
    }

    public ByteBuffer readByteBuffer(int length) {
        return readByteBuffer(nativePtr, length);
    }

    public String readString8(int length) {
        byte[] bytes = new byte[length];
        read(bytes);
        String str = new String(bytes, XServer.LATIN1_CHARSET);
        if ((-length & 3) > 0) skip(-length & 3);
        return str;
    }

    public void skip(int length) {
        skip(nativePtr, length);
    }

    public void destroy() {
        destroy(nativePtr);
    }

    private native long nativeAllocate(int fd, int initialCapacity);

    @CriticalNative
    private static native byte readByte(long nativePtr);

    @CriticalNative
    private static native short readShort(long nativePtr);

    @CriticalNative
    private static native int readInt(long nativePtr);

    @CriticalNative
    private static native long readLong(long nativePtr);

    private static native ByteBuffer readByteBuffer(long nativePtr, int length);

    @CriticalNative
    private static native void skip(long nativePtr, int length);

    @CriticalNative
    private static native int available(long nativePtr);

    @CriticalNative
    private static native int readMoreData(long nativePtr, boolean canReceiveAncillaryMessages);

    @CriticalNative
    private static native int getActivePosition(long nativePtr);

    @CriticalNative
    private static native void setActivePosition(long nativePtr, int activePosition);

    @CriticalNative
    private static native int getAncillaryFd(long nativePtr);

    @CriticalNative
    private static native void destroy(long nativePtr);

    private static native void readByteArray(long nativePtr, byte[] dest, int offset, int length);

    private static native void readIntArray(long nativePtr, int[] dest, int offset, int count);
}
