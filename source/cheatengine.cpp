#include <algorithm>
#include <cheatengine.h>

#include "cheatengine.h"
#include "cartridge.h"
#include "gameboy.h"
#include "mmu.h"

#define TO_INT(a) ( (a) >= 'a' ? (a) - 'a' + 10 : (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

void CheatEngine::reset() {
    u32 numCheats = this->cheats.size();
    for(u32 i = 0; i < numCheats; i++) {
        this->toggleCheat(i, false);
    }

    this->cheats.clear();
}

void CheatEngine::update() {
    for(const Cheat& cheat : this->cheats) {
        if(cheat.enabled) {
            for(const CheatLine& line : cheat.lines) {
                if(line.type == CHEAT_TYPE_GAMESHARK) {
                    u8 oldSvbk = this->gameboy->mmu.read(SVBK);
                    switch(line.bank & 0xF0) {
                        case 0x00:
                            this->gameboy->mmu.write(line.address, line.data);
                            break;
                        case 0x80: /* TODO : Find info and stuff */
                            break;
                        case 0x90:
                            this->gameboy->mmu.write(SVBK, (u8) (line.bank & 7));
                            this->gameboy->mmu.write(line.address, line.data);
                            this->gameboy->mmu.write(SVBK, oldSvbk);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
}

void CheatEngine::parseLine(Cheat& cheat, const std::string& line) {
    if(!line.empty()) {
        CheatLine cheatLine;

        // Clear all flags
        cheatLine.type = CHEAT_TYPE_UNKNOWN;
        cheatLine.patchedBanks.clear();
        cheatLine.patchedValues.clear();

        std::string::size_type len = line.length();

        if(len == 7) { // GameGenie AAA-BBB
            cheatLine.type = CHEAT_TYPE_GAMEGENIE_SHORT;

            cheatLine.data = (u8) (TO_INT(line[0]) << 4 | TO_INT(line[1]));
            cheatLine.address = (u16) (TO_INT(line[6]) << 12 | TO_INT(line[2]) << 8 | TO_INT(line[4]) << 4 | TO_INT(line[5]));

            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("GG (Short) 0x%04X / 0x%02X\n", cheatLine.address, cheatLine.data);
            }
        } else if(len == 11) { // GameGenie AAA-BBB-CCC
            cheatLine.type = CHEAT_TYPE_GAMEGENIE_LONG;

            cheatLine.data = (u8) (TO_INT(line[0]) << 4 | TO_INT(line[1]));
            cheatLine.address = (u16) (TO_INT(line[6]) << 12 | TO_INT(line[2]) << 8 | TO_INT(line[4]) << 4 | TO_INT(line[5]));
            cheatLine.compare = (u8) (TO_INT(line[8]) << 4 | TO_INT(line[10]));

            cheatLine.address ^= 0xF000;
            cheatLine.compare = (u8) ((cheatLine.compare >> 2) | (cheatLine.compare & 0x3) << 6);
            cheatLine.compare ^= 0xBA;

            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("GG (Long) 0x%04X / 0x%02X -> 0x%02X\n", cheatLine.address, cheatLine.data, cheatLine.compare);
            }
        } else if(len == 8) { // GameShark AAAAAAAA
            cheatLine.type = CHEAT_TYPE_GAMESHARK;

            cheatLine.data = (u8) (TO_INT(line[2]) << 4 | TO_INT(line[3]));
            cheatLine.address = (u16) (TO_INT(line[6]) << 12 | TO_INT(line[7]) << 8 | TO_INT(line[4]) << 4 | TO_INT(line[5]));
            cheatLine.bank = (u8) (TO_INT(line[0]) << 4 | TO_INT(line[1]));

            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("GS (0x%02X) 0x%04X / 0x%02X\n", cheatLine.bank, cheatLine.address, cheatLine.data);
            }
        } else if(this->gameboy->settings.printDebug != nullptr) { // Unknown
            this->gameboy->settings.printDebug("Line \"%s\" of cheat \"%s\" is of unknown type.\n", line.c_str(), cheat.name.c_str());
        }

        cheat.lines.push_back(cheatLine);
    }
}

void CheatEngine::addCheat(const std::string& name, const std::string& value) {
    Cheat cheat;

    cheat.name = name;
    cheat.value = value;
    cheat.enabled = false;

    std::string::size_type start = 0;
    std::string::size_type separator = 0;
    while((separator = value.find('+', start)) != std::string::npos) {
        const std::string line = value.substr(start, separator - start);
        this->parseLine(cheat, line);
        start = separator + 1;
    }

    std::string::size_type len = value.length();
    if(start < len) {
        this->parseLine(cheat, value.substr(start, len - start));
    }

    this->cheats.push_back(cheat);
}

void CheatEngine::toggleCheat(u32 cheat, bool enabled) {
    if(cheat >= this->cheats.size()) {
        return;
    }

    Cheat& c = this->cheats[cheat];
    c.enabled = enabled;

    if(this->gameboy->cartridge != nullptr) {
        if(enabled) {
            for(u16 bank = 0; bank < this->gameboy->cartridge->getRomBanks(); bank++) {
                u8* bankPtr = this->gameboy->cartridge->getRomBank(bank);
                if(bankPtr != nullptr) {
                    for(CheatLine &line : c.lines) {
                        if(line.type == CHEAT_TYPE_GAMEGENIE_SHORT || line.type == CHEAT_TYPE_GAMEGENIE_LONG) {
                            u16 bankSlot = line.address / ROM_BANK_SIZE;
                            if((bankSlot == 0 && bank == 0) || (bankSlot == 1 && bank != 0)) {
                                u16 address = line.address & ROM_BANK_MASK;
                                if((line.type == CHEAT_TYPE_GAMEGENIE_SHORT || bankPtr[address] == line.compare)
                                   && std::find(line.patchedBanks.begin(), line.patchedBanks.end(), bank) == line.patchedBanks.end()) {
                                    line.patchedBanks.push_back(bank);
                                    line.patchedValues.push_back(bankPtr[address]);
                                    bankPtr[address] = line.data;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            for(CheatLine &line : c.lines) {
                if(line.type == CHEAT_TYPE_GAMEGENIE_SHORT || line.type == CHEAT_TYPE_GAMEGENIE_LONG) {
                    for(u32 i = 0; i < line.patchedBanks.size(); i++) {
                        u8* bank = this->gameboy->cartridge->getRomBank(line.patchedBanks[i]);
                        if(bank != nullptr) {
                            bank[line.address & ROM_BANK_MASK] = line.patchedValues[i];
                        }
                    }

                    line.patchedBanks.clear();
                    line.patchedValues.clear();
                }
            }
        }
    }
}

void CheatEngine::toggleCheat(const std::string& name, bool enabled) {
    u32 numCheats = this->cheats.size();
    for(u32 i = 0; i < numCheats; i++) {
        if(this->cheats[i].name == name) {
            this->toggleCheat(i, enabled);
            break;
        }
    }
}