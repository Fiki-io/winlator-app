package com.winlator.alsaserver;

import java.nio.ByteBuffer;

public final class NativeAAudio {
    static {
        System.loadLibrary("winlator");
    }

    private NativeAAudio() {}

    public static native long createStream(int sampleRate, int channels, int formatType, int performanceMode, int latencyMillis);

    public static native boolean start(long handle);

    public static native boolean stop(long handle);

    public static native boolean pause(long handle);

    public static native boolean flush(long handle);

    public static native void release(long handle);

    public static native int write(long handle, ByteBuffer buffer, int offsetInBytes, int lengthInBytes);

    public static native long getFramePosition(long handle);

    public static native void setVolume(long handle, float volume);
}

