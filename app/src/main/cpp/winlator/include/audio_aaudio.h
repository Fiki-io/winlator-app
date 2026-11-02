#ifndef WINLATOR_AUDIO_AAUDIO_H
#define WINLATOR_AUDIO_AAUDIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* AAudioHandle;

AAudioHandle AAudio_createStream(int32_t sampleRate, int32_t channels,
                                int32_t formatType, int32_t performanceMode,
                                int32_t latencyMillis);

bool AAudio_start(AAudioHandle handle);
bool AAudio_stop(AAudioHandle handle);
bool AAudio_pause(AAudioHandle handle);
bool AAudio_flush(AAudioHandle handle);
void AAudio_release(AAudioHandle handle);

int32_t AAudio_write(AAudioHandle handle, const void* buffer, int32_t offsetInBytes, int32_t lengthInBytes);
int64_t AAudio_getFramePosition(AAudioHandle handle);
void AAudio_setVolume(AAudioHandle handle, float volume);

#ifdef __cplusplus
}
#endif

#endif // WINLATOR_AUDIO_AAUDIO_H

