#pragma once

#include "types.h"

class Gameboy;

void mgrInit();
void mgrExit();

void mgrPrintDebug(const char* str, ...);

Gameboy* mgrGetGameboy();

bool mgrGetFastForward();

void mgrRefreshPalette();
void mgrRefreshBorder();

bool mgrStateExists(int stateNum);
bool mgrLoadState(int stateNum);
bool mgrSaveState(int stateNum);
void mgrDeleteState(int stateNum);

void mgrUnloadRom(bool save = true);
void mgrReset();

void mgrRun();
