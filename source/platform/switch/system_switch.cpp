#ifdef BACKEND_SWITCH

#include <switch.h>

#include "platform/audio.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"

static bool requestedExit;

bool systemInit(int argc, char* argv[]) {
    if(!gfxInit()) {
        return false;
    }

    audioInit();
    uiInit();
    inputInit();

    requestedExit = false;

    return true;
}

void systemExit() {
    inputCleanup();
    uiCleanup();
    audioCleanup();
    gfxCleanup();
}

bool systemIsRunning() {
    return !requestedExit && appletMainLoop();
}

void systemRequestExit() {
    requestedExit = true;
}

u32* systemGetCameraImage() {
    return nullptr;
}

#endif