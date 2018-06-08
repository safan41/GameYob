#ifdef BACKEND_SWITCH

#include <cstring>
#include <cstdarg>
#include <queue>

#include <switch.h>

#include "platform/ui.h"

// TODO: Console-based UI draws slowly on Switch.

static PrintConsole* console;

static TextColor textColor;
static bool highlighted;

static std::queue<UIKey> keyQueue;

void uiInit() {
    console = consoleInit(nullptr);
    consoleDebugInit(debugDevice_SVC);
}

void uiCleanup() {
}

void uiGetSize(u32* width, u32* height) {
    if(width != nullptr) {
        *width = (u32) console->consoleWidth;
    }

    if(height != nullptr) {
        *height = (u32) console->consoleHeight;
    }
}

void uiClear() {
    gfxConfigureResolution(0, 0);
    consoleClear();
}

static void uiUpdateAttr() {
    if(highlighted) {
        printf("\x1b[0m\x1b[47m");
        switch(textColor) {
            case TEXT_COLOR_NONE:
                printf("\x1b[30m");
                break;
            case TEXT_COLOR_YELLOW:
                printf("\x1b[33m");
                break;
            case TEXT_COLOR_RED:
                printf("\x1b[31m");
                break;
            case TEXT_COLOR_GREEN:
                printf("\x1b[32m");
                break;
            case TEXT_COLOR_PURPLE:
                printf("\x1b[35m");
                break;
            case TEXT_COLOR_GRAY:
                printf("\x1b[2m\x1b[37m");
                break;
            case TEXT_COLOR_WHITE:
                printf("\x1b[30m");
                break;
        }
    } else {
        printf("\x1b[0m\x1b[40m");
        switch(textColor) {
            case TEXT_COLOR_NONE:
                printf("\x1b[37m");
                break;
            case TEXT_COLOR_YELLOW:
                printf("\x1b[1m\x1b[33m");
                break;
            case TEXT_COLOR_RED:
                printf("\x1b[1m\x1b[31m");
                break;
            case TEXT_COLOR_GREEN:
                printf("\x1b[1m\x1b[32m");
                break;
            case TEXT_COLOR_PURPLE:
                printf("\x1b[1m\x1b[35m");
                break;
            case TEXT_COLOR_GRAY:
                printf("\x1b[2m\x1b[37m");
                break;
            case TEXT_COLOR_WHITE:
                printf("\x1b[37m");
                break;
        }
    }
}

void uiSetLineHighlighted(bool highlight) {
    highlighted = highlight;
    uiUpdateAttr();
}

void uiSetTextColor(TextColor color) {
    textColor = color;
    uiUpdateAttr();
}

void uiAdvanceCursor(u32 n) {
    printf("\x1b[%" PRIu32 "C", n);
}

void uiSetLine(u32 n) {
    printf("\x1b[%" PRIu32 ";1H", n);
}

void uiPrint(const char* str, ...) {
    va_list list;
    va_start(list, str);
    uiPrintv(str, list);
    va_end(list);
}

void uiPrintv(const char* str, va_list list) {
    vprintf(str, list);
}

void uiFlush() {
    gfxFlushBuffers();
}

void uiPushInput(UIKey key) {
    keyQueue.push(key);
}

UIKey uiReadKey() {
    if(!keyQueue.empty()) {
        UIKey key = keyQueue.front();
        keyQueue.pop();
        return key;
    }

    return UI_KEY_NONE;
}

// TODO: Eventual software keyboard support, once supported by libnx.

bool uiIsStringInputSupported() {
    return false;
}

bool uiInputString(std::string& out, size_t maxLength, const std::string& hint) {
    return false;
}

#endif
