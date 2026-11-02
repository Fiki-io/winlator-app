#ifndef WINLATOR_NATIVE_INPUT_H
#define WINLATOR_NATIVE_INPUT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void NativeInput_init(void);
void NativeInput_stop(void);
bool NativeInput_sendMouseEvent(int32_t flags, int16_t dx, int16_t dy, int16_t wheelDelta, bool cursorFeedback);
bool NativeInput_sendGamepadState(int32_t port, uint8_t slot, uint8_t mapperType,
                                 int16_t buttons, uint8_t povHat,
                                 int16_t thumbLX, int16_t thumbLY,
                                 int16_t thumbRX, int16_t thumbRY,
                                 int16_t triggerL, int16_t triggerR);
bool NativeInput_sendKeyboardEvent(uint8_t vkey, int32_t flags);

#ifdef __cplusplus
}
#endif

#endif // WINLATOR_NATIVE_INPUT_H

