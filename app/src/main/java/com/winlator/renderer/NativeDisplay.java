package com.winlator.renderer;

import android.view.Surface;

public final class NativeDisplay {
    private static volatile boolean available = false;

    static {
        System.loadLibrary("winlator");
    }

    private NativeDisplay() {}

    public static boolean init() {
        if (!available) {
            available = nativeInit();
        }
        return available;
    }

    public static boolean isAvailable() {
        return available;
    }

    public static void present(int textureId, int viewX, int viewY, int viewWidth, int viewHeight, boolean flipY) {
        nativePresent(textureId, viewX, viewY, viewWidth, viewHeight, flipY);
    }

    public static boolean setSurface(Surface surface, int width, int height) {
        return nativeSetSurface(surface, width, height);
    }

    public static void destroySurface() {
        nativeDestroySurface();
    }

    public static boolean swapBuffers() {
        return nativeSwapBuffers();
    }

    public static void destroy() {
        available = false;
        nativeDestroy();
    }

    private static native boolean nativeInit();
    private static native void nativePresent(int textureId, int viewX, int viewY, int viewWidth, int viewHeight, boolean flipY);
    private static native boolean nativeSetSurface(Surface surface, int width, int height);
    private static native void nativeDestroySurface();
    private static native boolean nativeSwapBuffers();
    private static native void nativeDestroy();
}
