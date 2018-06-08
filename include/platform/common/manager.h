#pragma once

#include "types.h"

class Cartridge;
class CheatEngine;

void mgrInit();
void mgrExit();

void mgrReset();

void mgrPrintDebug(const char* str, ...);

bool mgrIsRomLoaded();
Cartridge* mgrGetRom();
std::string mgrGetRomName();
void mgrPowerOff(bool save = true);

bool mgrStateExists(int stateNum);
bool mgrLoadState(int stateNum);
bool mgrSaveState(int stateNum);
void mgrDeleteState(int stateNum);

CheatEngine* mgrGetCheatEngine();

void mgrRefreshPalette();
void mgrRefreshBorder();

bool mgrGetFastForward();

void mgrRun();
