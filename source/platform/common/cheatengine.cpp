#include <algorithm>
#include <fstream>

#include "libs/inih/INIReader.h"

#include "platform/common/cheatengine.h"
#include "platform/common/manager.h"
#include "cartridge.h"
#include "gameboy.h"
#include "mmu.h"

#define TO_INT(a) ( (a) >= 'a' ? (a) - 'a' + 10 : (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

int CheatEngine::parseCheatsIni(void* user, const char* section, const char* name, const char* value) {
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

void CheatEngine::loadCheats(const std::string& filename) {
    this->cheatsVec.clear();

    ini_parse(filename.c_str(), CheatEngine::parseCheatsIni, this);
}

void CheatEngine::saveCheats(const std::string& filename) {
    if(this->cheatsVec.size() == 0) {
        return;
    }

    std::ofstream stream(filename);
    if(stream.is_open()) {
        for(const Cheat& cheat : this->cheatsVec) {
            stream << "[" << cheat.name << "]\n";
            stream << "value=" << cheat.value << "\n";
            stream << "enabled=" << (cheat.enabled ? 1 : 0) << "\n";
            stream << "\n";
        }

        stream.close();
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

            mgrPrintDebug("GG (Short) %04x / %02x\n", cheatLine.address, cheatLine.data);
        } else if(len == 11) { // GameGenie AAA-BBB-CCC
            cheatLine.type = CHEAT_TYPE_GAMEGENIE_LONG;

            cheatLine.data = (u8) (TO_INT(line[0]) << 4 | TO_INT(line[1]));
            cheatLine.address = (u16) (TO_INT(line[6]) << 12 | TO_INT(line[2]) << 8 | TO_INT(line[4]) << 4 | TO_INT(line[5]));
            cheatLine.compare = (u8) (TO_INT(line[8]) << 4 | TO_INT(line[10]));

            cheatLine.address ^= 0xF000;
            cheatLine.compare = (u8) ((cheatLine.compare >> 2) | (cheatLine.compare & 0x3) << 6);
            cheatLine.compare ^= 0xBA;

            mgrPrintDebug("GG (Long) %04x / %02x -> %02x\n", cheatLine.address, cheatLine.data, cheatLine.compare);
        } else if(len == 8) { // Gameshark AAAAAAAA
            cheatLine.type = CHEAT_TYPE_GAMESHARK;

            cheatLine.data = (u8) (TO_INT(line[2]) << 4 | TO_INT(line[3]));
            cheatLine.address = (u16) (TO_INT(line[6]) << 12 | TO_INT(line[7]) << 8 | TO_INT(line[4]) << 4 | TO_INT(line[5]));
            cheatLine.bank = (u8) (TO_INT(line[0]) << 4 | TO_INT(line[1]));

            mgrPrintDebug("GS (%02x) %04x / %02x\n", cheatLine.bank, cheatLine.address, cheatLine.data);
        } else { // Unknown
            mgrPrintDebug("Line \"%s\" of cheat \"%s\" is of unknown type.\n", line.c_str(), cheat.name.c_str());
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

    this->cheatsVec.push_back(cheat);
}

void CheatEngine::toggleCheat(u32 cheat, bool enabled) {
    if(this->gameboy->cartridge != nullptr) {
        if(cheat >= this->cheatsVec.size()) {
            return;
        }

        Cheat& c = this->cheatsVec[cheat];
        c.enabled = enabled;

        if(enabled) {
            this->applyAllRomCheats();
        } else {
            this->unapplyRomCheat(cheat);
        }
    }
}

void CheatEngine::applyAllRomCheats() {
    if(this->gameboy->cartridge != nullptr) {
        for(u16 bank = 0; bank < this->gameboy->cartridge->getRomBanks(); bank++) {
            u8* bankPtr = this->gameboy->cartridge->getRomBank(bank);
            if(bankPtr != nullptr) {
                for(Cheat &cheat : this->cheatsVec) {
                    if(cheat.enabled) {
                        for(CheatLine &line : cheat.lines) {
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
            }
        }
    }
}

void CheatEngine::unapplyRomCheat(u32 cheat) {
    if(cheat >= this->cheatsVec.size()) {
        return;
    }

    if(this->gameboy->cartridge != nullptr) {
        Cheat& c = this->cheatsVec[cheat];
        for(CheatLine& line : c.lines) {
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

void CheatEngine::applyRamCheats() {
    for(const Cheat& cheat : this->cheatsVec) {
        if(cheat.enabled) {
            for(const CheatLine& line : cheat.lines) {
                if(line.type == CHEAT_TYPE_GAMESHARK) {
                    u8 oldSvbk = gameboy->mmu.read(SVBK);
                    switch(line.bank & 0xF0) {
                        case 0x00:
                            this->gameboy->settings.printDebug("Applying cheat \"%s\": 0x%04X = 0x%02X\n", cheat.name.c_str(), line.address, line.data);
                            this->gameboy->mmu.write(line.address, line.data);
                            break;
                        case 0x80: /* TODO : Find info and stuff */
                            break;
                        case 0x90:
                            this->gameboy->settings.printDebug("Applying cheat \"%s\": 0x%02X, 0x%04X = 0x%02X\n", cheat.name.c_str(), line.bank, line.address, line.data);
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