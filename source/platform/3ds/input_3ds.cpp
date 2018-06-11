#ifdef BACKEND_3DS

#include <chrono>
#include <cstring>

#include <3ds.h>

#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/input.h"
#include "platform/ui.h"

#define NUM_BUTTONS 32

static const std::string dsKeyNames[NUM_BUTTONS] = {
        "A",         // 0
        "B",         // 1
        "Select",    // 2
        "Start",     // 3
        "Right",     // 4
        "Left",      // 5
        "Up",        // 6
        "Down",      // 7
        "R",         // 8
        "L",         // 9
        "X",         // 10
        "Y",         // 11
        "",          // 12
        "",          // 13
        "ZL",        // 14
        "ZR",        // 15
        "",          // 16
        "",          // 17
        "",          // 18
        "",          // 19
        "",          // 20
        "",          // 21
        "",          // 22
        "",          // 23
        "C-Right",   // 24
        "C-Left",    // 25
        "C-Up",      // 26
        "C-Down",    // 27
        "Pad-Right", // 28
        "Pad-Left",  // 29
        "Pad-Up",    // 30
        "Pad-Down"   // 31
};

static KeyConfig defaultKeyConfig = {
        "Main",
        {
                FUNC_KEY_A,                   // 0 = BUTTON_A
                FUNC_KEY_B,                   // 1 = BUTTON_B
                FUNC_KEY_SELECT,              // 2 = BUTTON_SELECT
                FUNC_KEY_START,               // 3 = BUTTON_START
                FUNC_KEY_RIGHT,               // 4 = BUTTON_DRIGHT
                FUNC_KEY_LEFT,                // 5 = BUTTON_DLEFT
                FUNC_KEY_UP,                  // 6 = BUTTON_DUP
                FUNC_KEY_DOWN,                // 7 = BUTTON_DDOWN
                FUNC_KEY_MENU,                // 8 = BUTTON_R
                FUNC_KEY_FAST_FORWARD,        // 9 = BUTTON_L
                FUNC_KEY_START,               // 10 = BUTTON_X
                FUNC_KEY_SELECT,              // 11 = BUTTON_Y
                FUNC_KEY_NONE,                // 12 = BUTTON_NONE
                FUNC_KEY_NONE,                // 13 = BUTTON_NONE
                FUNC_KEY_FAST_FORWARD_TOGGLE, // 14 = BUTTON_ZL
                FUNC_KEY_SCALE,               // 15 = BUTTON_ZR
                FUNC_KEY_NONE,                // 16 = BUTTON_NONE
                FUNC_KEY_NONE,                // 17 = BUTTON_NONE
                FUNC_KEY_NONE,                // 18 = BUTTON_NONE
                FUNC_KEY_NONE,                // 19 = BUTTON_NONE
                FUNC_KEY_NONE,                // 20 = BUTTON_TOUCH
                FUNC_KEY_NONE,                // 21 = BUTTON_NONE
                FUNC_KEY_NONE,                // 22 = BUTTON_NONE
                FUNC_KEY_NONE,                // 23 = BUTTON_NONE
                FUNC_KEY_RIGHT,               // 24 = BUTTON_CSTICK_RIGHT
                FUNC_KEY_LEFT,                // 25 = BUTTON_CSTICK_LEFT
                FUNC_KEY_UP,                  // 26 = BUTTON_CSTICK_UP
                FUNC_KEY_DOWN,                // 27 = BUTTON_CSTICK_DOWN
                FUNC_KEY_RIGHT,               // 28 = BUTTON_CPAD_RIGHT
                FUNC_KEY_LEFT,                // 29 = BUTTON_CPAD_LEFT
                FUNC_KEY_UP,                  // 30 = BUTTON_CPAD_UP
                FUNC_KEY_DOWN                 // 31 = BUTTON_CPAD_DOWN
        }
};

static UIKey uiKeyMapping[NUM_BUTTONS] = {
        UI_KEY_A,     // 0 = BUTTON_A
        UI_KEY_B,     // 1 = BUTTON_B
        UI_KEY_NONE,  // 2 = BUTTON_SELECT
        UI_KEY_NONE,  // 3 = BUTTON_START
        UI_KEY_RIGHT, // 4 = BUTTON_DRIGHT
        UI_KEY_LEFT,  // 5 = BUTTON_DLEFT
        UI_KEY_UP,    // 6 = BUTTON_DUP
        UI_KEY_DOWN,  // 7 = BUTTON_DDOWN
        UI_KEY_R,     // 8 = BUTTON_R
        UI_KEY_L,     // 9 = BUTTON_L
        UI_KEY_X,     // 10 = BUTTON_X
        UI_KEY_Y,     // 11 = BUTTON_Y
        UI_KEY_NONE,  // 12 = BUTTON_NONE
        UI_KEY_NONE,  // 13 = BUTTON_NONE
        UI_KEY_NONE,  // 14 = BUTTON_ZL
        UI_KEY_NONE,  // 15 = BUTTON_ZR
        UI_KEY_NONE,  // 16 = BUTTON_NONE
        UI_KEY_NONE,  // 17 = BUTTON_NONE
        UI_KEY_NONE,  // 18 = BUTTON_NONE
        UI_KEY_NONE,  // 19 = BUTTON_NONE
        UI_KEY_NONE,  // 20 = BUTTON_TOUCH
        UI_KEY_NONE,  // 21 = BUTTON_NONE
        UI_KEY_NONE,  // 22 = BUTTON_NONE
        UI_KEY_NONE,  // 23 = BUTTON_NONE
        UI_KEY_RIGHT, // 24 = BUTTON_CSTICK_RIGHT
        UI_KEY_LEFT,  // 25 = BUTTON_CSTICK_LEFT
        UI_KEY_UP,    // 26 = BUTTON_CSTICK_UP
        UI_KEY_DOWN,  // 27 = BUTTON_CSTICK_DOWN
        UI_KEY_RIGHT, // 28 = BUTTON_CPAD_RIGHT
        UI_KEY_LEFT,  // 29 = BUTTON_CPAD_LEFT
        UI_KEY_UP,    // 30 = BUTTON_CPAD_UP
        UI_KEY_DOWN   // 31 = BUTTON_CPAD_DOWN
};

static u32 funcKeyMapping[NUM_FUNC_KEYS];

static bool forceReleased[NUM_FUNC_KEYS] = {false};
static bool uiForceReleased[NUM_BUTTONS] = {false};

static std::chrono::time_point<std::chrono::system_clock> nextUiRepeat;

extern void uiPushInput(UIKey key);

void inputInit() {
    nextUiRepeat = std::chrono::system_clock::now();

    HIDUSER_EnableAccelerometer();
}

void inputCleanup() {
    HIDUSER_DisableAccelerometer();
}

void inputUpdate() {
    hidScanInput();

    u32 down = hidKeysDown();
    u32 held = hidKeysHeld();

    for(u32 i = 0; i < NUM_FUNC_KEYS; i++) {
        if(!(held & funcKeyMapping[i])) {
            forceReleased[i] = false;
        }
    }

    for(u32 i = 0; i < NUM_BUTTONS; i++) {
        if(!(held & (1U << i))) {
            uiForceReleased[i] = false;
        }
    }

    if(menuIsVisible()) {
        for(u32 i = 0; i < NUM_BUTTONS; i++) {
            if(uiForceReleased[i]) {
                continue;
            }

            auto time = std::chrono::system_clock::now();

            u32 button = 1U << i;
            bool pressed = false;
            if(down & button) {
                nextUiRepeat = time + std::chrono::milliseconds(250);
                pressed = true;
            } else if((held & button) && time >= nextUiRepeat) {
                nextUiRepeat = time + std::chrono::milliseconds(50);
                pressed = true;
            }

            if(pressed) {
                UIKey key = uiKeyMapping[i];
                if(key != UI_KEY_NONE) {
                    uiPushInput(key);
                }
            }
        }
    }
}

bool inputKeyHeld(u32 key) {
    return key < NUM_FUNC_KEYS && !forceReleased[key] && (hidKeysHeld() & funcKeyMapping[key]) != 0;
}

bool inputKeyPressed(u32 key) {
    return key < NUM_FUNC_KEYS && !forceReleased[key] && (hidKeysDown() & funcKeyMapping[key]) != 0;
}

void inputReleaseAll() {
    for(u32 i = 0; i < NUM_FUNC_KEYS; i++) {
        forceReleased[i] = true;
    }

    for(u32 i = 0; i < NUM_BUTTONS; i++) {
        uiForceReleased[i] = true;
    }
}

void inputGetMotionSensor(u16* x, u16* y) {
    accelVector vec;
    hidAccelRead(&vec);

    *x = (u16) ((vec.x >> 3) + 0x7FF);
    *y = (u16) ((vec.z >> 3) + 0x7FF);
}

void inputSetRumble(bool rumble) {
}

u32 inputGetKeyCount() {
    return NUM_BUTTONS;
}

bool inputIsValidKey(u32 keyIndex) {
    return keyIndex < NUM_BUTTONS && dsKeyNames[keyIndex].length() > 0;
}

const std::string inputGetKeyName(u32 keyIndex) {
    return dsKeyNames[keyIndex];
}

KeyConfig* inputGetDefaultKeyConfig() {
    return &defaultKeyConfig;
}

void inputLoadKeyConfig(KeyConfig* keyConfig) {
    memset(funcKeyMapping, 0, NUM_FUNC_KEYS * sizeof(u32));
    for(u32 i = 0; i < NUM_BUTTONS; i++) {
        funcKeyMapping[keyConfig->funcKeys[i]] |= (1 << i);
    }

    funcKeyMapping[FUNC_KEY_MENU] |= KEY_TOUCH;
}

#endif