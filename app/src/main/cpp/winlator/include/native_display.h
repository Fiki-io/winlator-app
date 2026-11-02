#ifndef NATIVE_DISPLAY_H
#define NATIVE_DISPLAY_H

#include <jni.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool NativeDisplay_init(void);
void NativeDisplay_present(int32_t textureId, int32_t viewX, int32_t viewY, int32_t viewWidth, int32_t viewHeight, bool flipY);
bool NativeDisplay_setSurface(JNIEnv* env, jobject surface, int32_t width, int32_t height);
void NativeDisplay_destroySurface(void);
bool NativeDisplay_swapBuffers(void);
void NativeDisplay_destroy(void);

#ifdef __cplusplus
}
#endif

#endif // NATIVE_DISPLAY_H

