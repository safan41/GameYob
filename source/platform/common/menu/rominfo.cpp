#include "platform/common/menu/menu.h"
#include "platform/common/menu/rominfo.h"
#include "platform/common/manager.h"
#include "platform/ui.h"
#include "cartridge.h"

bool RomInfoMenu::processInput(UIKey key, u32 width, u32 height) {
    if(key == UI_KEY_A || key == UI_KEY_B) {
        menuPop();
    }

    return false;
}

void RomInfoMenu::draw(u32 width, u32 height) {
    if(mgrIsRomLoaded()) {
        static const char* mbcNames[] = {"ROM", "MBC1", "MBC2", "MBC3", "MBC5", "MBC7", "MMM01", "HUC1", "HUC3", "CAMERA", "TAMA5"};

        Cartridge* cartridge = mgrGetRom();

        uiPrint("ROM Title: \"%s\"\n", cartridge->getRomTitle().c_str());
        uiPrint("SGB: Supported: %d\n", cartridge->isSgbEnhanced());
        uiPrint("CGB: Supported: %d, Required: %d\n", cartridge->isCgbSupported(), cartridge->isCgbRequired());
        uiPrint("Cartridge type: %.2x (%s)\n", cartridge->getRawMBC(), mbcNames[cartridge->getMBCType()]);
        uiPrint("ROM Size: %.2x (%" PRIu32 " banks)\n", cartridge->getRawRomSize(), cartridge->getRomBanks());
        uiPrint("RAM Size: %.2x (%" PRIu32 " banks)\n", cartridge->getRawRamSize(), cartridge->getRamBanks());
    } else {
        uiPrint("ROM not loaded.\n");
    }
}