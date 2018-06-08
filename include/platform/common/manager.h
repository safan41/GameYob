#pragma once

#include "types.h"

class Cartridge;
class CheatEngine;

void mgrInit();
void mgrExit();

void mgrPrintDebug(const char* str, ...);

bool mgrIsRomLoaded();
Cartridge* mgrGetRom();
CheatEngine* mgrGetCheatEngine();

bool mgrGetFastForward();

void mgrRefreshPalette();
void mgrRefreshBorder();

bool mgrStateExists(int stateNum);
bool mgrLoadState(int stateNum);
bool mgrSaveState(int stateNum);
void mgrDeleteState(int stateNum);

void mgrReset();
void mgrPowerOff(bool save = true);

void mgrRun();
