package com.winlator.xserver.extensions;

public final class NativeSync {
    static {
        System.loadLibrary("winlator");
    }

    private NativeSync() {}

    public static native void createFence(int id, boolean initiallyTriggered);

    public static native void triggerFence(int id);

    public static native void resetFence(int id);

    public static native void destroyFence(int id);

    public static native boolean isTriggered(int id);

    public static native boolean hasFence(int id);

    public static native boolean awaitFence(int[] ids, int count, int timeoutMs);
}

