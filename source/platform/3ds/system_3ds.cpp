#ifdef BACKEND_3DS

#include <chrono>

#include <3ds.h>

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

    osSetSpeedupEnable(true);

    requestedExit = false;

    return true;
}

void systemExit() {
    osSetSpeedupEnable(false);

    inputCleanup();
    uiCleanup();
    audioCleanup();
    gfxCleanup();
}

bool systemIsRunning() {
    return !requestedExit && aptMainLoop();
}

void systemRequestExit() {
    requestedExit = true;
}

u64 systemGetNanoTime() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

u32* systemGetCameraImage() {
    return nullptr;
}

#endif