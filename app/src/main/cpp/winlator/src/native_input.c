#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <jni.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "winlator.h"
#include "native_input.h"

#define WINHANDLER_DEFAULT_CLIENT_PORT 7946

#pragma pack(push, 1)
typedef struct {
    uint8_t  requestCode;   // 7 = MOUSE_EVENT
    uint32_t length;        // 10
    uint32_t flags;
    int16_t  dx;
    int16_t  dy;
    int16_t  wheelDelta;
    uint8_t  cursorFeedback;
} MouseEventPacket;

typedef struct {
    uint8_t  requestCode;   // 9 = GET_GAMEPAD_STATE
    uint8_t  slot;
    int16_t  buttons;
    uint8_t  povHat;
    int16_t  thumbLX;
    int16_t  thumbLY;
    int16_t  thumbRX;
    int16_t  thumbRY;
    int16_t  triggerL;
    int16_t  triggerR;
} GamepadStateXInputPacket;

typedef struct {
    uint8_t  requestCode;   // 9 = GET_GAMEPAD_STATE
    uint8_t  slot;
    int16_t  buttons;
    uint8_t  povHat;
    int16_t  thumbLX;
    int16_t  thumbLY;
    int16_t  thumbRX;
    int16_t  thumbRY;
} GamepadStateStandardPacket;

typedef struct {
    uint8_t  requestCode;   // 11 = KEYBOARD_EVENT
    uint8_t  vkey;
    uint32_t flags;
} KeyboardEventPacket;
#pragma pack(pop)

static int g_inputSock = -1;
static struct sockaddr_in g_defaultClientAddr;
static pthread_mutex_t g_sockMutex = PTHREAD_MUTEX_INITIALIZER;

void NativeInput_init(void) {
    pthread_mutex_lock(&g_sockMutex);
    if (g_inputSock < 0) {
        g_inputSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (g_inputSock >= 0) {
            memset(&g_defaultClientAddr, 0, sizeof(g_defaultClientAddr));
            g_defaultClientAddr.sin_family = AF_INET;
            g_defaultClientAddr.sin_port = htons(WINHANDLER_DEFAULT_CLIENT_PORT);
            inet_pton(AF_INET, "127.0.0.1", &g_defaultClientAddr.sin_addr);
        }
    }
    pthread_mutex_unlock(&g_sockMutex);
}

void NativeInput_stop(void) {
    pthread_mutex_lock(&g_sockMutex);
    if (g_inputSock >= 0) {
        close(g_inputSock);
        g_inputSock = -1;
    }
    pthread_mutex_unlock(&g_sockMutex);
}

bool NativeInput_sendMouseEvent(int32_t flags, int16_t dx, int16_t dy, int16_t wheelDelta, bool cursorFeedback) {
    if (g_inputSock < 0) {
        NativeInput_init();
        if (g_inputSock < 0) return false;
    }

    MouseEventPacket packet;
    packet.requestCode = 7;
    packet.length = 10;
    packet.flags = (uint32_t)flags;
    packet.dx = dx;
    packet.dy = dy;
    packet.wheelDelta = wheelDelta;
    packet.cursorFeedback = cursorFeedback ? 1 : 0;

    ssize_t sent = sendto(g_inputSock, &packet, sizeof(packet), 0,
                          (struct sockaddr*)&g_defaultClientAddr, sizeof(g_defaultClientAddr));
    return sent == sizeof(packet);
}

bool NativeInput_sendGamepadState(int32_t port, uint8_t slot, uint8_t mapperType,
                                 int16_t buttons, uint8_t povHat,
                                 int16_t thumbLX, int16_t thumbLY,
                                 int16_t thumbRX, int16_t thumbRY,
                                 int16_t triggerL, int16_t triggerR) {
    if (g_inputSock < 0) {
        NativeInput_init();
        if (g_inputSock < 0) return false;
    }

    struct sockaddr_in targetAddr;
    memset(&targetAddr, 0, sizeof(targetAddr));
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &targetAddr.sin_addr);

    if (mapperType == 1) { // DINPUT_MAPPER_TYPE_XINPUT
        GamepadStateXInputPacket packet;
        packet.requestCode = 9;
        packet.slot = slot;
        packet.buttons = buttons;
        packet.povHat = povHat;
        packet.thumbLX = thumbLX;
        packet.thumbLY = thumbLY;
        packet.thumbRX = thumbRX;
        packet.thumbRY = thumbRY;
        packet.triggerL = triggerL;
        packet.triggerR = triggerR;

        ssize_t sent = sendto(g_inputSock, &packet, sizeof(packet), 0,
                              (struct sockaddr*)&targetAddr, sizeof(targetAddr));
        return sent == sizeof(packet);
    }
    else { // DINPUT_MAPPER_TYPE_STANDARD
        GamepadStateStandardPacket packet;
        packet.requestCode = 9;
        packet.slot = slot;
        packet.buttons = buttons;
        packet.povHat = povHat;
        packet.thumbLX = thumbLX;
        packet.thumbLY = thumbLY;
        packet.thumbRX = thumbRX;
        packet.thumbRY = thumbRY;

        ssize_t sent = sendto(g_inputSock, &packet, sizeof(packet), 0,
                              (struct sockaddr*)&targetAddr, sizeof(targetAddr));
        return sent == sizeof(packet);
    }
}

bool NativeInput_sendKeyboardEvent(uint8_t vkey, int32_t flags) {
    if (g_inputSock < 0) {
        NativeInput_init();
        if (g_inputSock < 0) return false;
    }

    KeyboardEventPacket packet;
    packet.requestCode = 11;
    packet.vkey = vkey;
    packet.flags = (uint32_t)flags;

    ssize_t sent = sendto(g_inputSock, &packet, sizeof(packet), 0,
                          (struct sockaddr*)&g_defaultClientAddr, sizeof(g_defaultClientAddr));
    return sent == sizeof(packet);
}

// JNI exports
JNIEXPORT void JNICALL
Java_com_winlator_winhandler_NativeInput_init(JNIEnv* env, jclass cls) {
    (void)env;
    (void)cls;
    NativeInput_init();
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_NativeInput_stop(JNIEnv* env, jclass cls) {
    (void)env;
    (void)cls;
    NativeInput_stop();
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_winhandler_NativeInput_sendMouseEvent(
    jint flags, jint dx, jint dy, jint wheelDelta, jboolean cursorFeedback) {
    return NativeInput_sendMouseEvent((int32_t)flags, (int16_t)dx, (int16_t)dy,
                                      (int16_t)wheelDelta, (bool)cursorFeedback) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_winhandler_NativeInput_sendGamepadState(
    jint port, jbyte slot, jbyte mapperType,
    jshort buttons, jbyte povHat,
    jshort thumbLX, jshort thumbLY, jshort thumbRX, jshort thumbRY,
    jshort triggerL, jshort triggerR) {
    return NativeInput_sendGamepadState(
        (int32_t)port, (uint8_t)slot, (uint8_t)mapperType,
        (int16_t)buttons, (uint8_t)povHat,
        (int16_t)thumbLX, (int16_t)thumbLY, (int16_t)thumbRX, (int16_t)thumbRY,
        (int16_t)triggerL, (int16_t)triggerR) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_winhandler_NativeInput_sendKeyboardEvent(jbyte vkey, jint flags) {
    return NativeInput_sendKeyboardEvent((uint8_t)vkey, (int32_t)flags) ? JNI_TRUE : JNI_FALSE;
}

