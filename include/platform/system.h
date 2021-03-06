#pragma once

#include "types.h"

bool systemInit(int argc, char* argv[]);
void systemExit();

bool systemIsRunning();
void systemRequestExit();

u32* systemGetCameraImage();