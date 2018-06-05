#ifdef BACKEND_SWITCH

#include <cstring>

#include <switch.h>

#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/gfx.h"
#include "platform/system.h"
#include "ppu.h"

#define GB_FRAME_PIXELS (GB_FRAME_WIDTH * GB_FRAME_HEIGHT)

#define GB_FRAME_2X_WIDTH (GB_FRAME_WIDTH * 2)
#define GB_FRAME_2X_HEIGHT (GB_FRAME_HEIGHT * 2)
#define GB_FRAME_2X_PIXELS (GB_FRAME_2X_WIDTH * GB_FRAME_2X_HEIGHT)

static u32* borderBuffer;
static u32 borderWidth;
static u32 borderHeight;

static u32* screenBuffer;
static u32* scale2xBuffer;

bool gfxInit() {
    gfxInitDefault();

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

#define SEEK_PIXEL(buf, x, y, pitch) ((buf) + ((((y) * (pitch)) + (x))))
#define DO_SCALE2X() if(Bp != Hp && Dp != Fp) {                    \
                         if(Dp == Bp) *(E0) = Dp;                  \
                         else *(E0) = Ep;                          \
                         if(Bp == Fp) *((E0) + 1) = Fp;            \
                         else *((E0) + 1) = Ep;                    \
                         if(Dp == Hp) *((E0) + dstPitch) = Dp;     \
                         else *((E0) + dstPitch) = Ep;             \
                         if(Hp == Fp) *((E0) + dstPitch + 1) = Fp; \
                         else *((E0) + dstPitch + 1) = Ep;         \
                     } else {                                      \
                         *(E0) = Ep;                               \
                         *((E0) + 1) = Ep;                         \
                         *((E0) + dstPitch) = Ep;                  \
                         *((E0) + dstPitch + 1) = Ep;              \
                     }

void gfxScale2xRGBA8888(u32* src, u32 srcPitch, u32* dst, u32 dstPitch, int width, int height) {
    u32* E, *E0;
    u32 Ep, Bp, Dp, Fp, Hp;

    // Top line and top corners
    E = SEEK_PIXEL(src, 0, 0, srcPitch);
    E0 = SEEK_PIXEL(dst, 0, 0, dstPitch);
    Ep = E[0];
    Bp = Ep;
    Dp = Ep;
    Fp = E[1];
    Hp = E[srcPitch];
    DO_SCALE2X();

    for(int x = 1; x < width - 1; x++) {
        E += 1;
        E0 += 2;
        Dp = Ep;
        Ep = Fp;
        Fp = E[1];
        Bp = Ep;
        Hp = E[srcPitch];
        DO_SCALE2X();
    }

    E += 1;
    E0 += 2;
    Dp = Ep;
    Ep = Fp;
    Bp = Ep;
    Hp = E[srcPitch];
    DO_SCALE2X();

    // Middle Rows and sides
    for(int y = 1; y < height - 1; y++) {
        E = SEEK_PIXEL(src, 0, y, srcPitch);
        E0 = SEEK_PIXEL(dst, 0, y * 2, dstPitch);
        Ep = E[0];
        Bp = E[-srcPitch];
        Dp = Ep;
        Fp = E[1];
        Hp = E[srcPitch];
        DO_SCALE2X();

        for(int x = 1; x < width - 1; x++) {
            E += 1;
            E0 += 2;
            Dp = Ep;
            Ep = Fp;
            Fp = E[1];
            Bp = E[-srcPitch];
            Hp = E[srcPitch];
            DO_SCALE2X();
        }

        E += 1;
        E0 += 2;
        Dp = Ep;
        Ep = Fp;
        Bp = E[-srcPitch];
        Hp = E[srcPitch];
        DO_SCALE2X();
    }

    // Bottom Row and Bottom Corners
    E = SEEK_PIXEL(src, 0, height - 1, srcPitch);
    E0 = SEEK_PIXEL(dst, 0, (height - 1) * 2, dstPitch);
    Ep = E[0];
    Bp = E[-srcPitch];
    Dp = Ep;
    Fp = E[1];
    Hp = Ep;
    DO_SCALE2X();

    for(int x = 1; x < width - 1; x++) {
        E += 1;
        E0 += 2;
        Dp = Ep;
        Ep = Fp;
        Fp = E[1];
        Bp = E[-srcPitch];
        Hp = Ep;
        DO_SCALE2X();
    }

    E += 1;
    E0 += 2;
    Dp = Ep;
    Ep = Fp;
    Bp = E[-srcPitch];
    Hp = Ep;
    DO_SCALE2X();
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

void gfxDrawScreen() {
    if(!menuIsVisible()) {
        u8 scaleMode = configGetMultiChoice(GROUP_DISPLAY, DISPLAY_SCALING_MODE);
        u8 scaleFilter = configGetMultiChoice(GROUP_DISPLAY, DISPLAY_SCALING_FILTER);

        u32 screenTexWidth = GB_FRAME_WIDTH;
        u32 screenTexHeight = GB_FRAME_HEIGHT;
        u32* transferBuffer = screenBuffer;

        if(scaleMode != SCALING_MODE_OFF && scaleFilter == SCALING_FILTER_SCALE2X) {
            screenTexWidth = GB_FRAME_2X_WIDTH;
            screenTexHeight = GB_FRAME_2X_HEIGHT;
            transferBuffer = scale2xBuffer;

            gfxScale2xRGBA8888(screenBuffer, GB_FRAME_WIDTH, scale2xBuffer, GB_FRAME_2X_WIDTH, GB_FRAME_WIDTH, GB_FRAME_HEIGHT);
        }

        u32 viewportWidth = 0;
        u32 viewportHeight = 0;
        u32* framebuffer = (u32*) gfxGetFramebuffer(&viewportWidth, &viewportHeight);

        float scaleWidth = 1;
        float scaleHeight = 1;
        gfxScaleDimensions(&scaleWidth, &scaleHeight, viewportWidth, viewportHeight);

        // TODO: Flip draw order when transparency is supported.

        // Draw the border.
        if(borderBuffer != nullptr && scaleMode != SCALING_MODE_FULL) {
            // Calculate output dimensions.
            int scaledBorderWidth = borderWidth;
            int scaledBorderHeight = borderHeight;
            if(configGetMultiChoice(GROUP_DISPLAY, DISPLAY_CUSTOM_BORDERS_SCALING) == CUSTOM_BORDERS_SCALING_SCALE_BASE) {
                scaledBorderWidth *= scaleWidth;
                scaledBorderHeight *= scaleHeight;
            }

            if(configGetMultiChoice(GROUP_DISPLAY, DISPLAY_CUSTOM_BORDERS_SCALING) == CUSTOM_BORDERS_SCALING_SCALE_BASE) {
                if(scaleMode == SCALING_MODE_125) {
                    scaledBorderWidth *= 1.25f;
                    scaledBorderHeight *= 1.25f;
                } else if(scaleMode == SCALING_MODE_150) {
                    scaledBorderWidth *= 1.50f;
                    scaledBorderHeight *= 1.50f;
                } else if(scaleMode == SCALING_MODE_ASPECT) {
                    scaledBorderWidth *= viewportHeight / (float) GB_FRAME_HEIGHT;
                    scaledBorderHeight *= viewportHeight / (float) GB_FRAME_HEIGHT;
                }
            }

            // Calculate output points.
            const int x1 = ((int) viewportWidth - scaledBorderWidth) / 2;
            const int y1 = ((int) viewportHeight - scaledBorderHeight) / 2;

            for(int x = 0; x < scaledBorderWidth; x++) {
                for(int y = 0; y < scaledBorderHeight; y++) {
                    int dstX = x + x1;
                    int dstY = y + y1;

                    if(dstX >= 0 && dstY >= 0 && dstX < viewportWidth && dstY < viewportHeight) {
                        framebuffer[gfxGetFramebufferDisplayOffset((u32) dstX, (u32) dstY)] = __builtin_bswap32(borderBuffer[(y * borderHeight / scaledBorderHeight) * borderWidth + (x * borderWidth / scaledBorderWidth)]);
                    }
                }
            }
        }

        // Draw the screen.
        {
            // Calculate output dimensions.
            int screenWidth = (int) (GB_FRAME_WIDTH * scaleWidth);
            int screenHeight = (int) (GB_FRAME_HEIGHT * scaleHeight);

            // Calculate output points.
            const int x1 = ((int) viewportWidth - screenWidth) / 2;
            const int y1 = ((int) viewportHeight - screenHeight) / 2;

            for(int x = 0; x < screenWidth; x++) {
                for(int y = 0; y < screenHeight; y++) {
                    int dstX = x + x1;
                    int dstY = y + y1;

                    if(dstX >= 0 && dstY >= 0 && dstX < viewportWidth && dstY < viewportHeight) {
                        framebuffer[gfxGetFramebufferDisplayOffset((u32) dstX, (u32) dstY)] = __builtin_bswap32(transferBuffer[(y * screenTexHeight / screenHeight) * screenTexWidth + (x * screenTexWidth / screenWidth)]);
                    }
                }
            }
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gfxWaitForVsync();
    }
}

#endif
