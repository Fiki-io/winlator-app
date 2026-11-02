#include "native_display.h"
#include "winlator.h"

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define TAG "NativeDisplay"

static const char* VERTEX_SHADER =
    "#version 300 es\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "uniform int uFlipY;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "    vUV = vec2(aPosition.x, (uFlipY != 0) ? (1.0 - aPosition.y) : aPosition.y);\n"
    "    gl_Position = vec4(aPosition.x * 2.0 - 1.0, 1.0 - aPosition.y * 2.0, 0.0, 1.0);\n"
    "}\n";

static const char* FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D uTexture;\n"
    "in vec2 vUV;\n"
    "layout(location = 0) out vec4 outFragColor;\n"
    "void main() {\n"
    "    outFragColor = texture(uTexture, vUV);\n"
    "}\n";

static const float QUAD_VERTICES[] = {
    0.0f, 0.0f, // top-left
    0.0f, 1.0f, // bottom-left
    1.0f, 0.0f, // top-right
    1.0f, 1.0f  // bottom-right
};

static GLuint g_program = 0;
static GLuint g_quadVbo = 0;
static GLint g_uTextureLoc = -1;
static GLint g_uFlipYLoc = -1;

static ANativeWindow* g_nativeWindow = NULL;
static EGLDisplay g_eglDisplay = EGL_NO_DISPLAY;
static EGLSurface g_eglSurface = EGL_NO_SURFACE;
static EGLContext g_eglContext = EGL_NO_CONTEXT;

extern EGLContext globalEGLContext;

static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    if (!shader) return 0;

    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char infoLog[512] = {0};
        glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Shader compile error: %s", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool NativeDisplay_init(void) {
    if (g_program != 0) return true;

    GLuint vs = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
    if (!vs) return false;

    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glLinkProgram(g_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = 0;
    glGetProgramiv(g_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char infoLog[512] = {0};
        glGetProgramInfoLog(g_program, sizeof(infoLog), NULL, infoLog);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Program link error: %s", infoLog);
        glDeleteProgram(g_program);
        g_program = 0;
        return false;
    }

    g_uTextureLoc = glGetUniformLocation(g_program, "uTexture");
    g_uFlipYLoc = glGetUniformLocation(g_program, "uFlipY");

    glGenBuffers(1, &g_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void NativeDisplay_present(int32_t textureId, int32_t viewX, int32_t viewY, int32_t viewWidth, int32_t viewHeight, bool flipY) {
    if (g_program == 0) {
        if (!NativeDisplay_init()) return;
    }

    glViewport(viewX, viewY, viewWidth, viewHeight);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glUseProgram(g_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)textureId);
    glUniform1i(g_uTextureLoc, 0);
    glUniform1i(g_uFlipYLoc, flipY ? 1 : 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_quadVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

bool NativeDisplay_setSurface(JNIEnv* env, jobject surface, int32_t width, int32_t height) {
    NativeDisplay_destroySurface();

    if (!surface) return false;

    g_nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (!g_nativeWindow) return false;

    ANativeWindow_setBuffersGeometry(g_nativeWindow, width, height, WINDOW_FORMAT_RGBA_8888);

    g_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_eglDisplay == EGL_NO_DISPLAY) return false;

    static const EGLint confAttribList[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig eglConfig;
    int numConfigs = 0;
    if (!eglChooseConfig(g_eglDisplay, confAttribList, &eglConfig, 1, &numConfigs) || numConfigs == 0) {
        return false;
    }

    static const EGLint ctxAttribList[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    g_eglContext = eglCreateContext(g_eglDisplay, eglConfig, globalEGLContext != EGL_NO_CONTEXT ? globalEGLContext : NULL, ctxAttribList);
    if (g_eglContext == EGL_NO_CONTEXT) return false;

    g_eglSurface = eglCreateWindowSurface(g_eglDisplay, eglConfig, g_nativeWindow, NULL);
    if (g_eglSurface == EGL_NO_SURFACE) {
        eglDestroyContext(g_eglDisplay, g_eglContext);
        g_eglContext = EGL_NO_CONTEXT;
        return false;
    }

    eglMakeCurrent(g_eglDisplay, g_eglSurface, g_eglSurface, g_eglContext);
    eglSwapInterval(g_eglDisplay, 0);

    return true;
}

void NativeDisplay_destroySurface(void) {
    if (g_eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(g_eglDisplay, g_eglSurface);
            g_eglSurface = EGL_NO_SURFACE;
        }
        if (g_eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(g_eglDisplay, g_eglContext);
            g_eglContext = EGL_NO_CONTEXT;
        }
    }
    if (g_nativeWindow) {
        ANativeWindow_release(g_nativeWindow);
        g_nativeWindow = NULL;
    }
}

bool NativeDisplay_swapBuffers(void) {
    if (g_eglDisplay != EGL_NO_DISPLAY && g_eglSurface != EGL_NO_SURFACE) {
        return eglSwapBuffers(g_eglDisplay, g_eglSurface) == EGL_TRUE;
    }
    return false;
}

void NativeDisplay_destroy(void) {
    NativeDisplay_destroySurface();
    if (g_quadVbo != 0) {
        glDeleteBuffers(1, &g_quadVbo);
        g_quadVbo = 0;
    }
    if (g_program != 0) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_renderer_NativeDisplay_nativeInit(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;
    return NativeDisplay_init() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_winlator_renderer_NativeDisplay_nativePresent(JNIEnv *env, jclass clazz,
                                                      jint textureId, jint viewX, jint viewY,
                                                      jint viewWidth, jint viewHeight, jboolean flipY) {
    (void)env;
    (void)clazz;
    NativeDisplay_present(textureId, viewX, viewY, viewWidth, viewHeight, flipY == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_renderer_NativeDisplay_nativeSetSurface(JNIEnv *env, jclass clazz,
                                                         jobject surface, jint width, jint height) {
    (void)clazz;
    return NativeDisplay_setSurface(env, surface, width, height) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_winlator_renderer_NativeDisplay_nativeDestroySurface(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;
    NativeDisplay_destroySurface();
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_renderer_NativeDisplay_nativeSwapBuffers(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;
    return NativeDisplay_swapBuffers() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_winlator_renderer_NativeDisplay_nativeDestroy(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;
    NativeDisplay_destroy();
}
