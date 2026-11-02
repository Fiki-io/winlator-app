#ifndef WINLATOR_NATIVE_SYNC_H
#define WINLATOR_NATIVE_SYNC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void NativeSync_createFence(uint32_t id, bool initiallyTriggered);
void NativeSync_triggerFence(uint32_t id);
void NativeSync_resetFence(uint32_t id);
void NativeSync_destroyFence(uint32_t id);
bool NativeSync_isTriggered(uint32_t id);
bool NativeSync_hasFence(uint32_t id);
bool NativeSync_awaitFence(const uint32_t* ids, int count, uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif

#endif // WINLATOR_NATIVE_SYNC_H

