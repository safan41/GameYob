#pragma once

#include "types.h"

bool gfxInit();
void gfxCleanup();

u32* gfxGetScreenBuffer();
u32 gfxGetScreenPitch();

void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight);

void gfxDrawScreen();
