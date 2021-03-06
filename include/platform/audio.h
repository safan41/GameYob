#pragma once

#include "types.h"

void audioInit();
void audioCleanup();

u32 audioGetSampleRate();

void audioPlay(u32* buffer, long samples);