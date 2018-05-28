#ifdef BACKEND_SDL

#include <cstdlib>

#include <SDL2/SDL.h>

#include "platform/gfx.h"
#include "ppu.h"

static SDL_Rect screenRect = {0, 0, GB_FRAME_WIDTH, GB_FRAME_HEIGHT};
static SDL_Rect windowScreenRect = {0, 0, GB_FRAME_WIDTH, GB_FRAME_HEIGHT};

static SDL_Rect borderRect = {0, 0, 0, 0};
static SDL_Rect windowBorderRect = {0, 0, 0, 0};

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* screenTexture = nullptr;
static SDL_Texture* borderTexture = nullptr;

static u32* screenBuffer;

bool gfxInit() {
    window = SDL_CreateWindow("GameYob", 0, 0, GB_FRAME_WIDTH, GB_FRAME_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if(window == nullptr) {
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if(renderer == nullptr) {
        return false;
    }

    screenTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, GB_FRAME_WIDTH, GB_FRAME_HEIGHT);
    if(screenTexture == nullptr) {
        return false;
    }

    screenBuffer = (u32*) malloc(GB_FRAME_WIDTH * GB_FRAME_HEIGHT * sizeof(u32));

    return true;
}

void gfxCleanup() {
    if(screenBuffer != nullptr) {
        free(screenBuffer);
        screenBuffer = nullptr;
    }

    if(borderTexture != nullptr) {
        SDL_DestroyTexture(borderTexture);
        borderTexture = nullptr;
    }

    if(screenTexture != nullptr) {
        SDL_DestroyTexture(screenTexture);
        screenTexture = nullptr;
    }

    if(renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if(window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}

u32* gfxGetScreenBuffer() {
    return screenBuffer;
}

u32 gfxGetScreenPitch() {
    return GB_FRAME_WIDTH;
}

void gfxUpdateWindow() {
    int w;
    int h;
    SDL_GetWindowSize(window, &w, &h);

    if(borderTexture != nullptr) {
        if(w == GB_FRAME_WIDTH && h == GB_FRAME_HEIGHT) {
            SDL_SetWindowSize(window, borderRect.w, borderRect.h);
            w = borderRect.w;
            h = borderRect.h;
        }

        windowBorderRect.w = w;
        windowBorderRect.h = h;
        windowBorderRect.x = 0;
        windowBorderRect.y = 0;

        windowScreenRect.w = (int) (w * ((float) GB_FRAME_WIDTH / (float) borderRect.w));
        windowScreenRect.h = (int) (h * ((float) GB_FRAME_HEIGHT / (float) borderRect.h));
        windowScreenRect.x = (w - windowScreenRect.w) / 2;
        windowScreenRect.y = (h - windowScreenRect.h) / 2;
    } else {
        if(w == borderRect.w && h == borderRect.h) {
            SDL_SetWindowSize(window, GB_FRAME_WIDTH, GB_FRAME_HEIGHT);
            w = GB_FRAME_WIDTH;
            h = GB_FRAME_HEIGHT;
        }

        windowScreenRect.w = w;
        windowScreenRect.h = h;
        windowScreenRect.x = 0;
        windowScreenRect.y = 0;
    }
}

void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight) {
    if(borderTexture != nullptr) {
        SDL_DestroyTexture(borderTexture);
        borderTexture = nullptr;
    }

    if(imgData != nullptr && imgWidth != 0 && imgHeight != 0) {
        borderTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, imgWidth, imgHeight);
        if(borderTexture == nullptr) {
            return;
        }

        SDL_SetTextureBlendMode(borderTexture, SDL_BLENDMODE_BLEND);

        borderRect.w = imgWidth;
        borderRect.h = imgHeight;

        SDL_UpdateTexture(borderTexture, &borderRect, imgData, imgWidth * sizeof(u32));
    }

    gfxUpdateWindow();
}

void gfxDrawScreen() {
    SDL_RenderClear(renderer);

    SDL_UpdateTexture(screenTexture, &screenRect, screenBuffer, GB_FRAME_WIDTH * sizeof(u32));
    SDL_RenderCopy(renderer, screenTexture, &screenRect, &windowScreenRect);

    if(borderTexture != nullptr) {
        SDL_RenderCopy(renderer, borderTexture, &borderRect, &windowBorderRect);
    }

    SDL_RenderPresent(renderer);
}

#endif