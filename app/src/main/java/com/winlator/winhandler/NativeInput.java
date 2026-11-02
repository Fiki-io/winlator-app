package com.winlator.winhandler;

import dalvik.annotation.optimization.CriticalNative;

public final class NativeInput {
    static {
        System.loadLibrary("winlator");
    }

    private NativeInput() {}

    public static native void init();

    public static native void stop();

    @CriticalNative
    public static native boolean sendMouseEvent(int flags, int dx, int dy, int wheelDelta, boolean cursorFeedback);

    @CriticalNative
    public static native boolean sendGamepadState(int port, byte slot, byte mapperType,
                                                  short buttons, byte povHat,
                                                  short thumbLX, short thumbLY,
                                                  short thumbRX, short thumbRY,
                                                  short triggerL, short triggerR);

    @CriticalNative
    public static native boolean sendKeyboardEvent(byte vkey, int flags);
}

