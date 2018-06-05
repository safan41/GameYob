#ifdef BACKEND_3DS

#include <3ds.h>
#include <citro3d.h>

#include "platform/3ds/default_shbin.h"
#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/gfx.h"
#include "platform/system.h"
#include "ppu.h"

#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define GPU_FRAME_DIM 256
#define GPU_FRAME_PIXELS (GPU_FRAME_DIM * GPU_FRAME_DIM)
#define GPU_FRAME_SIZE (GPU_FRAME_PIXELS * sizeof(u32))

#define GPU_FRAME_2X_DIM 512
#define GPU_FRAME_2X_PIXELS (GPU_FRAME_2X_DIM * GPU_FRAME_2X_DIM)
#define GPU_FRAME_2X_SIZE (GPU_FRAME_2X_PIXELS * sizeof(u32))

static bool c3dInitialized;

static bool shaderInitialized;
static DVLB_s* dvlb;
static shaderProgram_s program;

static C3D_RenderTarget* targets[2];
static C3D_Mtx projections[2];

static bool screenInit;
static C3D_Tex screenTexture;

static bool borderInit;
static u16 borderWidth;
static u16 borderHeight;
static u16 gpuBorderWidth;
static u16 gpuBorderHeight;
static C3D_Tex borderTexture;

static u32* screenBuffer;
static u32* scale2xBuffer;

bool gfxInit() {
    gfxInitDefault();

    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxCleanup();
        return false;
    }

    c3dInitialized = true;

    u32 displayFlags = GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8);

    C3D_RenderTarget* top = C3D_RenderTargetCreate(TOP_SCREEN_HEIGHT, TOP_SCREEN_WIDTH, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    if(top == nullptr) {
        gfxCleanup();
        return false;
    }

    C3D_RenderTargetSetOutput(top, GFX_TOP, GFX_LEFT, displayFlags);
    targets[GAME_SCREEN_TOP] = top;

    C3D_RenderTarget* bottom = C3D_RenderTargetCreate(BOTTOM_SCREEN_HEIGHT, BOTTOM_SCREEN_WIDTH, GPU_RB_RGBA8, GPU_RB_DEPTH16);
    if(bottom == nullptr) {
        gfxCleanup();
        return false;
    }

    C3D_RenderTargetSetOutput(bottom, GFX_BOTTOM, GFX_LEFT, displayFlags);
    targets[GAME_SCREEN_BOTTOM] = bottom;

    Mtx_OrthoTilt(&projections[GAME_SCREEN_TOP], 0.0, TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT, 0.0, 0.0, 1.0, true);
    Mtx_OrthoTilt(&projections[GAME_SCREEN_BOTTOM], 0.0, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT, 0.0, 0.0, 1.0, true);

    dvlb = DVLB_ParseFile((u32*) default_shbin, default_shbin_len);
    if(dvlb == nullptr) {
        gfxCleanup();
        return false;
    }

    Result progInitRes = shaderProgramInit(&program);
    if(R_FAILED(progInitRes)) {
        gfxCleanup();
        return false;
    }

    shaderInitialized = true;

    Result progSetVshRes = shaderProgramSetVsh(&program, &dvlb->DVLE[0]);
    if(R_FAILED(progSetVshRes)) {
        gfxCleanup();
        return false;
    }

    C3D_BindProgram(&program);

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    if(attrInfo == nullptr) {
        gfxCleanup();
        return false;
    }

    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    if(env == NULL) {
        gfxCleanup();
        return false;
    }

    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
    C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);

    screenInit = false;
    borderInit = false;

    // Allocate and clear the screen buffer.
    screenBuffer = (u32*) linearAlloc(GPU_FRAME_SIZE);
    memset(screenBuffer, 0, GPU_FRAME_SIZE);

    // Allocate and clear the scale2x buffer.
    scale2xBuffer = (u32*) linearAlloc(GPU_FRAME_2X_SIZE);
    memset(scale2xBuffer, 0, GPU_FRAME_2X_SIZE);

    return true;
}

void gfxCleanup() {
    if(scale2xBuffer != nullptr) {
        linearFree(scale2xBuffer);
        scale2xBuffer = nullptr;
    }

    if(screenBuffer != nullptr) {
        linearFree(screenBuffer);
        screenBuffer = nullptr;
    }

    if(borderInit) {
        C3D_TexDelete(&borderTexture);
        borderInit = false;
    }

    if(screenInit) {
        C3D_TexDelete(&screenTexture);
        screenInit = false;
    }

    if(shaderInitialized) {
        shaderProgramFree(&program);
        shaderInitialized = false;
    }

    if(dvlb != nullptr) {
        DVLB_Free(dvlb);
        dvlb = nullptr;
    }

    for(u8 i = 0; i < 2; i++) {
        if(targets[i] != nullptr) {
            C3D_RenderTargetDelete(targets[i]);
            targets[i] = nullptr;
        }
    }

    if(c3dInitialized) {
        C3D_Fini();
        c3dInitialized = false;
    }

    gfxExit();
}

u32* gfxGetScreenBuffer() {
    return screenBuffer;
}

u32 gfxGetScreenPitch() {
    return GPU_FRAME_DIM;
}

void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight) {
    if(imgData == nullptr || (borderInit && (borderWidth != (u16) imgWidth || borderHeight != (u16) imgHeight))) {
        if(borderInit) {
            C3D_TexDelete(&borderTexture);
            borderInit = false;
        }

        borderWidth = 0;
        borderHeight = 0;
        gpuBorderWidth = 0;
        gpuBorderHeight = 0;

        if(imgData == nullptr) {
            return;
        }
    }

    // Adjust the texture to power-of-two dimensions.
    borderWidth = (u16) imgWidth;
    borderHeight = (u16) imgHeight;
    gpuBorderWidth = (u16) pow(2, ceil(log(borderWidth) / log(2)));
    gpuBorderHeight = (u16) pow(2, ceil(log(borderHeight) / log(2)));

    // Create the texture.
    if(!borderInit && !C3D_TexInit(&borderTexture, gpuBorderWidth, gpuBorderHeight, GPU_RGBA8)) {
        return;
    }

    C3D_TexSetFilter(&borderTexture, GPU_LINEAR, GPU_LINEAR);

    // Copy the texture to a power-of-two sized buffer.
    u32* imgBuffer = (u32*) imgData;
    u32* temp = (u32*) linearAlloc(gpuBorderWidth * gpuBorderHeight * sizeof(u32));
    for(int x = 0; x < borderWidth; x++) {
        for(int y = 0; y < borderHeight; y++) {
            temp[y * gpuBorderWidth + x] = imgBuffer[y * borderWidth + x];
        }
    }

    GSPGPU_FlushDataCache(temp, gpuBorderWidth * gpuBorderHeight * sizeof(u32));
    C3D_SyncDisplayTransfer(temp, (u32) GX_BUFFER_DIM(gpuBorderWidth, gpuBorderHeight), (u32*) borderTexture.data, (u32) GX_BUFFER_DIM(gpuBorderWidth, gpuBorderHeight), GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8));

    linearFree(temp);

    borderInit = true;
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

static void gfxScaleDimensions(float* scaleWidth, float* scaleHeight, u16 viewportWidth, u16 viewportHeight) {
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
    u8 gameScreen = configGetMultiChoice(GROUP_GAMEYOB, GAMEYOB_GAME_SCREEN);
    u8 scaleMode = configGetMultiChoice(GROUP_DISPLAY, DISPLAY_SCALING_MODE);
    u8 scaleFilter = configGetMultiChoice(GROUP_DISPLAY, DISPLAY_SCALING_FILTER);

    u16 screenTexSize = GPU_FRAME_DIM;
    u32* transferBuffer = screenBuffer;
    GPU_TEXTURE_FILTER_PARAM filter = GPU_NEAREST;

    if(scaleMode != SCALING_MODE_OFF && scaleFilter != SCALING_FILTER_NEAREST) {
        filter = GPU_LINEAR;

        if(scaleFilter == SCALING_FILTER_SCALE2X) {
            screenTexSize = GPU_FRAME_2X_DIM;
            transferBuffer = scale2xBuffer;

            gfxScale2xRGBA8888(screenBuffer, GPU_FRAME_DIM, scale2xBuffer, GPU_FRAME_2X_DIM, GB_FRAME_WIDTH, GB_FRAME_HEIGHT);
        }
    }

    if(!screenInit || screenTexture.width != screenTexSize || screenTexture.height != screenTexSize) {
        if(screenInit) {
            C3D_TexDelete(&screenTexture);
            screenInit = false;
        }

        screenInit = C3D_TexInit(&screenTexture, screenTexSize, screenTexSize, GPU_RGBA8);
    }

    C3D_TexSetFilter(&screenTexture, filter, filter);

    GSPGPU_FlushDataCache(transferBuffer, screenTexSize * screenTexSize * sizeof(u32));
    C3D_SyncDisplayTransfer(transferBuffer, (u32) GX_BUFFER_DIM(screenTexSize, screenTexSize), (u32*) screenTexture.data, (u32) GX_BUFFER_DIM(screenTexSize, screenTexSize), GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8));

    if(!C3D_FrameBegin(0)) {
        return;
    }

    C3D_RenderTarget* target = targets[gameScreen];
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0, 0);

    C3D_FrameDrawOn(target);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaderInstanceGetUniformLocation(program.vertexShader, "projection"), &projections[gameScreen]);

    u16 viewportWidth = target->frameBuf.height;
    u16 viewportHeight = target->frameBuf.width;

    float scaleWidth = 1;
    float scaleHeight = 1;
    gfxScaleDimensions(&scaleWidth, &scaleHeight, viewportWidth, viewportHeight);

    // Draw the screen.
    if(screenInit) {
        // Calculate the VBO dimensions.
        u16 screenWidth = (u16) (GB_FRAME_WIDTH * scaleWidth);
        u16 screenHeight = (u16) (GB_FRAME_HEIGHT * scaleHeight);

        // Calculate VBO points.
        const float x1 = ((int) viewportWidth - screenWidth) / 2.0f;
        const float y1 = ((int) viewportHeight - screenHeight) / 2.0f;
        const float x2 = x1 + screenWidth;
        const float y2 = y1 + screenHeight;

        static const float baseTX2 = (float) GB_FRAME_WIDTH / (float) GPU_FRAME_DIM;
        static const float baseTY2 = (float) GB_FRAME_HEIGHT / (float) GPU_FRAME_DIM;
        static const float baseFilterMod = 0.25f / (float) GPU_FRAME_DIM;

        float tx2 = baseTX2;
        float ty2 = baseTY2;
        if(scaleMode != SCALING_MODE_OFF && scaleFilter == SCALING_FILTER_LINEAR) {
            tx2 -= baseFilterMod;
            ty2 -= baseFilterMod;
        }

        C3D_TexBind(0, &screenTexture);

        C3D_ImmDrawBegin(GPU_TRIANGLES);

        C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0, 0, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, ty2, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, 0, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0, 0, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0, ty2, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, ty2, 0.0f, 0.0f);

        C3D_ImmDrawEnd();
    }

    // Draw the border.
    if(borderInit && scaleMode != SCALING_MODE_FULL) {
        // Calculate the VBO dimensions.
        u16 scaledBorderWidth = borderWidth;
        u16 scaledBorderHeight = borderHeight;
        if(configGetMultiChoice(GROUP_DISPLAY, DISPLAY_CUSTOM_BORDERS_SCALING) == CUSTOM_BORDERS_SCALING_SCALE_BASE) {
            scaledBorderWidth *= scaleWidth;
            scaledBorderHeight *= scaleHeight;
        }

        // Calculate VBO points.
        const float x1 = ((int) viewportWidth - scaledBorderWidth) / 2.0f;
        const float y1 = ((int) viewportHeight - scaledBorderHeight) / 2.0f;
        const float x2 = x1 + scaledBorderWidth;
        const float y2 = y1 + scaledBorderHeight;

        float tx2 = (float) borderWidth / (float) gpuBorderWidth;
        float ty2 = (float) borderHeight / (float) gpuBorderHeight;

        C3D_TexBind(0, &borderTexture);

        C3D_ImmDrawBegin(GPU_TRIANGLES);

        C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0, 0, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, ty2, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, 0, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0, 0, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0, ty2, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, ty2, 0.0f, 0.0f);

        C3D_ImmDrawEnd();
    }

    C3D_FrameEnd(0);
}

#endif
