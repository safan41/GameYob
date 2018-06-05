#ifdef BACKEND_SDL

#include <chrono>

#include <SDL2/SDL.h>

#include "platform/audio.h"
#include "platform/input.h"
#include "platform/gfx.h"
#include "platform/system.h"
#include "platform/ui.h"

extern void gfxUpdateWindow();

static bool requestedExit;

bool systemInit(int argc, char* argv[]) {
    if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0 || !gfxInit()) {
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

    SDL_Quit();
}

bool systemIsRunning() {
    if(requestedExit) {
        return false;
    }

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) {
            requestedExit = true;
            return false;
        } else if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
            gfxUpdateWindow();
        }
    }

    return true;
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