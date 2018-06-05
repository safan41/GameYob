#ifdef BACKEND_SWITCH

#include <cstring>

#include <switch.h>

#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/gfx.h"
#include "platform/system.h"
#include "ppu.h"

#define GB_FRAME_PIXELS (GB_FRAME_WIDTH * GB_FRAME_HEIGHT)

#define GB_FRAME_2X_WIDTH (GB_FRAME_WIDTH * 2)
#define GB_FRAME_2X_HEIGHT (GB_FRAME_HEIGHT * 2)
#define GB_FRAME_2X_PIXELS (GB_FRAME_2X_WIDTH * GB_FRAME_2X_HEIGHT)

static u32 defaultWidth;
static u32 defaultHeight;

static u32* borderBuffer;
static u32 borderWidth;
static u32 borderHeight;

static u32* screenBuffer;
static u32* scale2xBuffer;

bool gfxInit() {
    gfxInitDefault();
    gfxGetFramebufferResolution(&defaultWidth, &defaultHeight);

    borderBuffer = nullptr;

    screenBuffer = (u32*) calloc(GB_FRAME_PIXELS, sizeof(u32));
    if(screenBuffer == nullptr) {
        return false;
    }

    scale2xBuffer = (u32*) calloc(GB_FRAME_2X_PIXELS, sizeof(u32));
    if(scale2xBuffer == nullptr) {
        free(screenBuffer);
        screenBuffer = nullptr;

        return false;
    }

    return true;
}

void gfxCleanup() {
    if(scale2xBuffer != nullptr) {
        free(scale2xBuffer);
        scale2xBuffer = nullptr;
    }

    if(screenBuffer != nullptr) {
        free(screenBuffer);
        screenBuffer = nullptr;
    }

    gfxExit();
}

u32* gfxGetScreenBuffer() {
    return screenBuffer;
}

u32 gfxGetScreenPitch() {
    return GB_FRAME_WIDTH;
}

void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight) {
    if(imgData == nullptr || (borderBuffer != nullptr && (borderWidth != (u32) imgWidth || borderHeight != (u32) imgHeight))) {
        if(borderBuffer != nullptr) {
            free(borderBuffer);
            borderBuffer = nullptr;
        }

        borderWidth = 0;
        borderHeight = 0;

        if(imgData == nullptr) {
            return;
        }
    }

    // Create the texture buffer.
    if(borderBuffer == nullptr) {
        borderBuffer = (u32*) malloc(imgWidth * imgHeight * sizeof(u32));
        if(borderBuffer == nullptr) {
            return;
        }
    }

    borderWidth = (u32) imgWidth;
    borderHeight = (u32) imgHeight;

    memcpy(borderBuffer, imgData, borderWidth * borderHeight * sizeof(u32));
}

static void gfxScaleDimensions(float* scaleWidth, float* scaleHeight, u32 viewportWidth, u32 viewportHeight) {
    u8 scaleMode = configGetMultiChoice(GROUP_DISPLAY, DISPLAY_SCALING_MODE);

    *scaleWidth = 1;
    *scaleHeight = 1;

    if(scaleMode == SCALING_MODE_125) {
        *scaleWidth = *scaleHeight = 1.25f;
    } else if(scaleMode == SCALING_MODE_150) {
        *scaleWidth = *scaleHeight = 1.50f;
    } else if(scaleMode == SCALING_MODE_ASPECT) {
        *scaleWidth = *scaleHeight = viewportHeight / (float) GB_SCREEN_HEIGHT;
    } else if(scaleMode == SCALING_MODE_FULL) {
        *scaleWidth = viewportWidth / (float) GB_SCREEN_WIDTH;
        *scaleHeight = viewportHeight / (float) GB_SCREEN_HEIGHT;
    }
}

// TODO: Scaling Filters, Full Screen Mode
void gfxDrawScreen() {
    if(!menuIsVisible()) {
        u8 scaleMode = configGetMultiChoice(GROUP_DISPLAY, DISPLAY_SCALING_MODE);

        u32 viewportWidth = 0;
        u32 viewportHeight = 0;
        u32* framebuffer = (u32*) gfxGetFramebuffer(&viewportWidth, &viewportHeight);

        float scaleWidth = 1;
        float scaleHeight = 1;
        gfxScaleDimensions(&scaleWidth, &scaleHeight, defaultWidth, defaultHeight);

        if(scaleWidth > scaleHeight) {
            scaleWidth = scaleHeight;
        }

        u32 scaledViewportWidth = (u32) (defaultWidth / scaleWidth);
        u32 scaledViewportHeight = (u32) (defaultHeight / scaleWidth);

        if(viewportWidth != scaledViewportWidth || viewportHeight != scaledViewportHeight) {
            gfxConfigureResolution(scaledViewportWidth, scaledViewportHeight);
            framebuffer = (u32*) gfxGetFramebuffer(&viewportWidth, &viewportHeight);
        }

        // TODO: Flip draw order when transparency is supported.

        // Draw the border.
        if(borderBuffer != nullptr && scaleMode != SCALING_MODE_FULL) {
            // Calculate output points.
            const int x1 = ((int) viewportWidth - borderWidth) / 2;
            const int y1 = ((int) viewportHeight - borderHeight) / 2;

            // Copy to framebuffer.
            for(int x = 0; x < (int) borderWidth; x++) {
                for(int y = 0; y < (int) borderHeight; y++) {
                    int dstX = x + x1;
                    int dstY = y + y1;

                    if(dstX >= 0 && dstY >= 0 && dstX < (int) viewportWidth && dstY < (int) viewportHeight) {
                        framebuffer[gfxGetFramebufferDisplayOffset((u32) dstX, (u32) dstY)] = __builtin_bswap32(borderBuffer[y * borderWidth + x]);
                    }
                }
            }
        }

        // Draw the screen.
        {
            // Calculate output points.
            const int x1 = ((int) viewportWidth - GB_FRAME_WIDTH) / 2;
            const int y1 = ((int) viewportHeight - GB_FRAME_HEIGHT) / 2;

            // Copy to framebuffer.
            for(int x = 0; x < GB_FRAME_WIDTH; x++) {
                for(int y = 0; y < GB_FRAME_HEIGHT; y++) {
                    int dstX = x + x1;
                    int dstY = y + y1;

                    if(dstX >= 0 && dstY >= 0 && dstX < (int) viewportWidth && dstY < (int) viewportHeight) {
                        framebuffer[gfxGetFramebufferDisplayOffset((u32) dstX, (u32) dstY)] = __builtin_bswap32(screenBuffer[y * GB_FRAME_WIDTH + x]);
                    }
                }
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();

        if(!mgrGetFastForward()) {
            gfxWaitForVsync();
        }
    }
}

void gfxResetResolution() {
    gfxConfigureResolution(defaultWidth, defaultHeight);
}

#endif
