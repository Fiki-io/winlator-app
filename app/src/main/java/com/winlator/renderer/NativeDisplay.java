package com.winlator.renderer;

import android.view.Surface;
import com.winlator.widget.XServerView;
import com.winlator.xserver.Pixmap;
import com.winlator.xserver.Window;
import com.winlator.xserver.extensions.PresentExtension;

public final class NativeDisplay {
    private static volatile boolean available = false;
    private static volatile PendingPresentation pendingPresentation = null;
    private static final Object lock = new Object();

    public static class PendingPresentation {
        public final int textureId;
        public final int width;
        public final int height;
        public final boolean flipY;
        public final Window window;
        public final Pixmap pixmap;
        public final int serial;
        public final int idleFence;
        public final long ust;
        public final long msc;
        public final long targetMSC;
        public final PresentExtension presentExtension;

        public PendingPresentation(int textureId, int width, int height, boolean flipY,
                                   Window window, Pixmap pixmap, int serial, int idleFence,
                                   long ust, long msc, long targetMSC, PresentExtension presentExtension) {
            this.textureId = textureId;
            this.width = width;
            this.height = height;
            this.flipY = flipY;
            this.window = window;
            this.pixmap = pixmap;
            this.serial = serial;
            this.idleFence = idleFence;
            this.ust = ust;
            this.msc = msc;
            this.targetMSC = targetMSC;
            this.presentExtension = presentExtension;
        }
    }

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

    public static void queuePresentation(int textureId, int width, int height, boolean flipY,
                                        Window window, Pixmap pixmap, int serial, int idleFence,
                                        long ust, long msc, long targetMSC,
                                        PresentExtension presentExtension,
                                        XServerView xServerView) {
        synchronized (lock) {
            if (pendingPresentation != null && pendingPresentation.presentExtension != null) {
                pendingPresentation.presentExtension.onPresentationFinished(
                    pendingPresentation.window,
                    pendingPresentation.pixmap,
                    pendingPresentation.serial,
                    pendingPresentation.idleFence,
                    pendingPresentation.targetMSC
                );
            }
            pendingPresentation = new PendingPresentation(textureId, width, height, flipY,
                    window, pixmap, serial, idleFence, ust, msc, targetMSC, presentExtension);
        }
        if (xServerView != null) {
            xServerView.requestRender();
        }
    }

    public static boolean renderPending(GLRenderer renderer) {
        PendingPresentation p;
        synchronized (lock) {
            p = pendingPresentation;
            pendingPresentation = null;
        }

        if (p == null) return false;

        boolean fullscreen = renderer.isFullscreen();
        int viewX = fullscreen ? 0 : renderer.viewTransformation.viewOffsetX;
        int viewY = fullscreen ? 0 : renderer.viewTransformation.viewOffsetY;
        int viewWidth = fullscreen ? renderer.getSurfaceWidth() : renderer.viewTransformation.viewWidth;
        int viewHeight = fullscreen ? renderer.getSurfaceHeight() : renderer.viewTransformation.viewHeight;

        nativePresent(p.textureId, viewX, viewY, viewWidth, viewHeight, p.flipY);

        if (p.presentExtension != null) {
            p.presentExtension.onPresentationFinished(p.window, p.pixmap, p.serial, p.idleFence, p.targetMSC);
        }

        return true;
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

