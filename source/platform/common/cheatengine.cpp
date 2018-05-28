#include <algorithm>
#include <fstream>

#include "libs/inih/INIReader.h"

#include "platform/common/cheatengine.h"
#include "platform/common/manager.h"
#include "cartridge.h"
#include "gameboy.h"
#include "mmu.h"

#define TO_INT(a) ( (a) >= 'a' ? (a) - 'a' + 10 : (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

CheatEngine::CheatEngine(Gameboy* g) {
    gameboy = g;
}

void CheatEngine::loadCheats(const std::string& filename) {
    cheatsVec.clear();

    ini_parse(filename.c_str(), CheatEngine::parseCheats, this);
}

void CheatEngine::saveCheats(const std::string& filename) {
    if(cheatsVec.size() == 0) {
        return;
    }

    std::ofstream stream(filename);
    if(stream.is_open()) {
        for(const cheat_t& cheat : cheatsVec) {
            stream << "[" << cheat.name << "]\n";
            stream << "value=" << cheat.cheatString << "\n";
            stream << "enabled=" << ((cheat.flags & CHEAT_FLAG_ENABLED) != 0) << "\n";
            stream << "\n";
        }

        stream.close();
    }
}

int CheatEngine::parseCheats(void* user, const char* section, const char* name, const char* value) {
    CheatEngine* engine = (CheatEngine*) user;

    if(strcmp(name, "value") == 0) {
        engine->addCheat(std::string(section), std::string(value));
    } else if(strcmp(name, "enabled") == 0) {
        for(u32 i = 0; i < engine->cheatsVec.size(); i++) {
            if(engine->cheatsVec[i].name == section) {
                engine->toggleCheat(i, strcmp(value, "1") == 0);
                break;
            }
        }
    }

    return 1;
}

bool CheatEngine::addCheat(const std::string& name, const std::string& value) {
    cheat_t cheat;

    cheat.name = name;
    cheat.cheatString = value;

    // Clear all flags
    cheat.flags = 0;
    cheat.patchedBanks = std::vector<u16>();
    cheat.patchedValues = std::vector<u8>();

    size_t len = value.length();

    if(len == 11) { // GameGenie AAA-BBB-CCC
        cheat.flags |= CHEAT_FLAG_GAMEGENIE;

        cheat.data = (u8) (TO_INT(value[0]) << 4 | TO_INT(value[1]));
        cheat.address = (u16) (TO_INT(value[6]) << 12 | TO_INT(value[2]) << 8 | TO_INT(value[4]) << 4 | TO_INT(value[5]));
        cheat.compare = (u8) (TO_INT(value[8]) << 4 | TO_INT(value[10]));

        cheat.address ^= 0xf000;
        cheat.compare = (u8) ((cheat.compare >> 2) | (cheat.compare & 0x3) << 6);
        cheat.compare ^= 0xba;

        mgrPrintDebug("GG %04x / %02x -> %02x\n", cheat.address, cheat.data, cheat.compare);
    } else if(len == 7) { // GameGenie (6digit version) AAA-BBB
        cheat.flags |= CHEAT_FLAG_GAMEGENIE1;

        cheat.data = (u8) (TO_INT(value[0]) << 4 | TO_INT(value[1]));
        cheat.address = (u16) (TO_INT(value[6]) << 12 | TO_INT(value[2]) << 8 | TO_INT(value[4]) << 4 | TO_INT(value[5]));

        mgrPrintDebug("GG1 %04x / %02x\n", cheat.address, cheat.data);
    } else if(len == 8) { // Gameshark AAAAAAAA
        cheat.flags |= CHEAT_FLAG_GAMESHARK;

        cheat.data = (u8) (TO_INT(value[2]) << 4 | TO_INT(value[3]));
        cheat.bank = (u8) (TO_INT(value[0]) << 4 | TO_INT(value[1]));
        cheat.address = (u16) (TO_INT(value[6]) << 12 | TO_INT(value[7]) << 8 | TO_INT(value[4]) << 4 | TO_INT(value[5]));

        mgrPrintDebug("GS (%02x)%04x/ %02x\n", cheat.bank, cheat.address, cheat.data);
    } else { // Unknown
        return false;
    }

    cheatsVec.push_back(cheat);

    return true;
}

void CheatEngine::toggleCheat(u32 cheat, bool enabled) {
    if(gameboy->cartridge != nullptr) {
        if(cheat >= cheatsVec.size()) {
            return;
        }

        cheat_t& c = cheatsVec[cheat];

        if(enabled) {
            c.flags |= CHEAT_FLAG_ENABLED;
            if((c.flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK) {
                for(u16 bank = 0; bank < gameboy->cartridge->getRomBanks(); bank++) {
                    applyGGCheatsToBank(bank);
                }
            }
        } else {
            unapplyGGCheat(cheat);
            c.flags &= ~CHEAT_FLAG_ENABLED;
        }
    }
}

void CheatEngine::applyGGCheatsToBank(u16 bank) {
    if(gameboy->cartridge != nullptr) {
        u8* bankPtr = gameboy->cartridge->getRomBank(bank);
        for(cheat_t& cheat : cheatsVec) {
            if((cheat.flags & CHEAT_FLAG_ENABLED) && ((cheat.flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK)) {
                u16 bankSlot = cheat.address / ROM_BANK_SIZE;
                if((bankSlot == 0 && bank == 0) || (bankSlot == 1 && bank != 0)) {
                    u16 address = cheat.address & ROM_BANK_MASK;
                    if(((cheat.flags & CHEAT_FLAG_TYPE_MASK) == CHEAT_FLAG_GAMEGENIE1 || bankPtr[address] == cheat.compare)
                       && std::find(cheat.patchedBanks.begin(), cheat.patchedBanks.end(), bank) == cheat.patchedBanks.end()) {
                        cheat.patchedBanks.push_back(bank);
                        cheat.patchedValues.push_back(bankPtr[address]);
                        bankPtr[address] = cheat.data;
                    }
                }
            }
        }
    }
}

void CheatEngine::unapplyGGCheat(u32 cheat) {
    if(gameboy->cartridge != nullptr) {
        if(cheat >= cheatsVec.size()) {
            return;
        }

        cheat_t& c = cheatsVec[cheat];

        if((c.flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK) {
            for(u32 i = 0; i < c.patchedBanks.size(); i++) {
                u8* bank = gameboy->cartridge->getRomBank(c.patchedBanks[i]);
                bank[c.address & ROM_BANK_MASK] = c.patchedValues[i];
            }

            c.patchedBanks = std::vector<u16>();
            c.patchedValues = std::vector<u8>();
        }
    }
}

void CheatEngine::applyGSCheats() {
    for(const cheat_t& cheat : cheatsVec) {
        if(cheat.flags & CHEAT_FLAG_ENABLED && ((cheat.flags & CHEAT_FLAG_TYPE_MASK) == CHEAT_FLAG_GAMESHARK)) {
            u8 oldSvbk = gameboy->mmu.read(SVBK);
            switch(cheat.bank & 0xF0) {
                case 0x90:
                    gameboy->mmu.write(SVBK, (u8) (cheat.bank & 7));
                    gameboy->mmu.write(cheat.address, cheat.data);
                    gameboy->mmu.write(SVBK, oldSvbk);
                    break;
                case 0x80: /* TODO : Find info and stuff */
                    break;
                case 0x00:
                    gameboy->mmu.write(cheat.address, cheat.data);
                    break;
                default:
                    break;
            }
        }
    }
}