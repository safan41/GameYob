#include <algorithm>
#include <cstring>
#include <ctime>
#include <istream>
#include <ostream>

#include "cpu.h"
#include "gameboy.h"
#include "cartridge.h"
#include "mmu.h"

#define HALF_ROM_BANK_SIZE 0x2000

#define SRAM_BANK_SIZE 0x2000
#define SRAM_BANK_MASK 0x1FFF

#define HALF_SRAM_BANK_SIZE 0x1000
#define HALF_SRAM_BANK_MASK 0x0FFF

Cartridge::Cartridge(std::istream& romData, u32 romSize) {
    this->totalRomBanks = (romSize + ROM_BANK_SIZE - 1) >> 14;

    // Round number of banks to next power of two.
    if(this->totalRomBanks > 0) {
        this->totalRomBanks--;
        this->totalRomBanks |= this->totalRomBanks >> 1;
        this->totalRomBanks |= this->totalRomBanks >> 2;
        this->totalRomBanks |= this->totalRomBanks >> 4;
        this->totalRomBanks |= this->totalRomBanks >> 8;
        this->totalRomBanks |= this->totalRomBanks >> 16;
        this->totalRomBanks++;
    }

    if(this->totalRomBanks < 2) {
        this->totalRomBanks = 2;
    }

    size_t roundedSize = (size_t) (this->totalRomBanks * ROM_BANK_SIZE);

    this->rom = new u8[roundedSize]();
    romData.read((char*) this->rom, romSize);

    // Most MMM01 dumps have the initial banks at the end of the ROM rather than the beginning, so check if this is the case and compensate.
    if(romSize > 0x8000) {
        // Check for the logo.
        static const u8 logo[] = {
                0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
                0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
                0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC ,0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
        };

        u8* checkBank = &this->rom[roundedSize - 0x8000];

        if(memcmp(logo, &checkBank[0x0104], sizeof(logo)) == 0) {
            // Check for MMM01.
            u8 mbcType = checkBank[0x0147];

            if(mbcType >= 0x0B && mbcType <= 0x0D) {
                u8* copy = new u8[roundedSize];
                memcpy(copy, this->rom, roundedSize);

                memcpy(this->rom, &copy[roundedSize - 0x8000], 0x8000);
                memcpy(&this->rom[0x8000], copy, roundedSize - 0x8000);

                delete[] copy;
            }
        }
    }

    this->romTitle = std::string(reinterpret_cast<char*>(&this->rom[0x0134]), this->rom[0x0143] == 0x80 || this->rom[0x0143] == 0xC0 ? 15 : 16);
    this->romTitle.erase(std::find_if(this->romTitle.rbegin(), this->romTitle.rend(), [](int c) { return c != 0; }).base(), this->romTitle.end());

    switch(this->getRawMBC()) {
        case 0x00:
        case 0x08:
        case 0x09:
            this->mbcType = MBC0;
            break;
        case 0x01:
        case 0x02:
        case 0x03: {
            this->mbcType = MBC1;

            static const u8 pattern[4] = {0xCE, 0xED, 0x66, 0x66};
            this->multicart = this->totalRomBanks > 0x30
                              && memcmp(this->getRomBank(0x10) + 0x104, pattern, sizeof(pattern)) == 0
                              && memcmp(this->getRomBank(0x20) + 0x104, pattern, sizeof(pattern)) == 0;

            this->rockmanMapper = this->romTitle.compare("ROCKMAN 99") == 0;
            break;
        }
        case 0x05:
        case 0x06:
            this->mbcType = MBC2;
            break;
        case 0x0B:
        case 0x0C:
        case 0x0D:
            this->mbcType = MMM01;
            break;
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            this->mbcType = MBC3;
            break;
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
            this->mbcType = MBC5;
            this->rumble = this->getRawMBC() >= 0x1C;
            break;
        case 0x20:
            this->mbcType = MBC6;
            break;
        case 0x22:
            this->mbcType = MBC7;
            break;
        case 0xEA: // Hack for SONIC5
            this->mbcType = MBC1;
            break;
        case 0xFC:
            this->mbcType = CAMERA;
            break;
        case 0xFD:
            this->mbcType = TAMA5;
            break;
        case 0xFE:
            this->mbcType = HUC3;
            break;
        case 0xFF:
            this->mbcType = HUC1;
            break;
        default:
            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("Unsupported mapper value %02x\n", this->rom[0x0147]);
            }

            this->mbcType = MBC5;
            break;
    }

    this->readFunc = this->mbcReads[this->mbcType];
    this->writeFunc = this->mbcWrites[this->mbcType];
    this->writeSramFunc = this->mbcSramWrites[this->mbcType];
    this->updateFunc = this->mbcUpdates[this->mbcType];

    if(this->mbcType == MBC2 || this->mbcType == MBC7 || this->mbcType == TAMA5) {
        this->totalRamBanks = 1;
    } else {
        switch(this->getRawRamSize()) {
            case 0:
                this->totalRamBanks = 0;
                break;
            case 1:
            case 2:
                this->totalRamBanks = 1;
                break;
            case 3:
                this->totalRamBanks = 4;
                break;
            case 4:
                this->totalRamBanks = 16;
                break;
            case 5:
                this->totalRamBanks = 8;
                break;
            default:
                if(this->gameboy->settings.printDebug != nullptr) {
                    this->gameboy->settings.printDebug("Invalid RAM bank number: %x\nDefaulting to 4 banks.\n", this->getRawRamSize());
                }

                this->totalRamBanks = 4;
                break;
        }
    }

    this->sram = new u8[this->totalRamBanks * SRAM_BANK_SIZE]();
    memset(&this->rtcClock, 0, sizeof(this->rtcClock));
}

Cartridge::~Cartridge() {
    if(this->gameboy != nullptr) {
        this->gameboy->mmu.mapBankBlock(0x0, nullptr);
        this->gameboy->mmu.mapBankBlock(0x1, nullptr);
        this->gameboy->mmu.mapBankBlock(0x2, nullptr);
        this->gameboy->mmu.mapBankBlock(0x3, nullptr);
        this->gameboy->mmu.mapBankBlock(0x4, nullptr);
        this->gameboy->mmu.mapBankBlock(0x5, nullptr);
        this->gameboy->mmu.mapBankBlock(0x6, nullptr);
        this->gameboy->mmu.mapBankBlock(0x7, nullptr);
        this->gameboy->mmu.mapBankBlock(0xA, nullptr);
        this->gameboy->mmu.mapBankBlock(0xB, nullptr);

        this->gameboy = nullptr;
    }

    if(this->rom != nullptr) {
        delete[] this->rom;
        this->rom = nullptr;
    }

    if(this->sram != nullptr) {
        delete[] this->sram;
        this->sram = nullptr;
    }
}

void Cartridge::reset(Gameboy* gameboy) {
    this->gameboy = gameboy;

    // General
    this->romBank0 = 0;
    this->romBank1 = 1;
    this->sramBank = 0;
    this->sramEnabled = this->mbcType == MBC0;

    // MBC1
    this->mbc1Mode = false;

    // MBC3
    this->mbc3Ctrl = 0;

    // MBC6
    this->mbc6RomBank1A = 2;
    this->mbc6RomBank1B = 3;
    this->mbc6SramBankA = 0;
    this->mbc6SramBankB = 1;

    // MBC7
    memset(&this->mbc7, 0, sizeof(this->mbc7));
    this->mbc7.tiltX = 0x8000;
    this->mbc7.tiltY = 0x8000;

    // HuC1
    this->huc1RamMode = false;

    // HuC3
    this->huc3Mode = 0;
    this->huc3Value = 0;
    this->huc3Shift = 0;

    // MMM01
    this->mmm01BankSelected = false;
    this->mmm01RomBaseBank = 0;

    // CAMERA
    this->cameraIO = false;
    this->cameraReadyCycle = 0;
    memset(this->cameraRegs, 0, sizeof(this->cameraRegs));

    // TAMA5
    memset(&this->tama5, 0, sizeof(this->tama5));

    this->mapBanks();
}

void Cartridge::load(std::istream& data) {
    data.read((char*) this->sram, this->totalRamBanks * SRAM_BANK_SIZE);

    if(this->mbcType == MBC3 || this->mbcType == HUC3 || this->mbcType == TAMA5) {
        data.read((char*) &this->rtcClock, sizeof(this->rtcClock));
    }
}

void Cartridge::save(std::ostream& data) {
    data.write((char*) this->sram, this->totalRamBanks * SRAM_BANK_SIZE);

    if(this->mbcType == MBC3 || this->mbcType == HUC3 || this->mbcType == TAMA5) {
        data.write((char*) &this->rtcClock, sizeof(this->rtcClock));
    }
}

std::istream& operator>>(std::istream& is, Cartridge& cart) {
    is.read((char*) &cart.romBank0, sizeof(cart.romBank0));
    is.read((char*) &cart.romBank1, sizeof(cart.romBank1));
    is.read((char*) &cart.sramBank, sizeof(cart.sramBank));
    is.read((char*) &cart.sramEnabled, sizeof(cart.sramEnabled));

    switch(cart.mbcType) {
        case MBC1:
            is.read((char*) &cart.mbc1Mode, sizeof(cart.mbc1Mode));
            break;
        case MBC3:
            is.read((char*) &cart.mbc3Ctrl, sizeof(cart.mbc3Ctrl));
            break;
        case MBC6:
            is.read((char*) &cart.mbc6RomBank1A, sizeof(cart.mbc6RomBank1A));
            is.read((char*) &cart.mbc6RomBank1B, sizeof(cart.mbc6RomBank1B));
            is.read((char*) &cart.mbc6SramBankA, sizeof(cart.mbc6SramBankA));
            is.read((char*) &cart.mbc6SramBankB, sizeof(cart.mbc6SramBankB));
            break;
        case MBC7:
            is.read((char*) &cart.mbc7, sizeof(cart.mbc7));
            break;
        case HUC1:
            is.read((char*) &cart.huc1RamMode, sizeof(cart.huc1RamMode));
            break;
        case HUC3:
            is.read((char*) &cart.huc3Mode, sizeof(cart.huc3Mode));
            is.read((char*) &cart.huc3Value, sizeof(cart.huc3Value));
            is.read((char*) &cart.huc3Shift, sizeof(cart.huc3Shift));
            break;
        case MMM01:
            is.read((char*) &cart.mmm01BankSelected, sizeof(cart.mmm01BankSelected));
            is.read((char*) &cart.mmm01RomBaseBank, sizeof(cart.mmm01RomBaseBank));
            break;
        case CAMERA:
            is.read((char*) &cart.cameraIO, sizeof(cart.cameraIO));
            is.read((char*) &cart.cameraReadyCycle, sizeof(cart.cameraReadyCycle));
            is.read((char*) cart.cameraRegs, sizeof(cart.cameraRegs));
            break;
        case TAMA5:
            is.read((char*) &cart.tama5, sizeof(cart.tama5));
        default:
            break;
    }

    cart.mapBanks();

    return is;
}

std::ostream& operator<<(std::ostream& os, const Cartridge& cart) {
    os.write((char*) &cart.romBank0, sizeof(cart.romBank0));
    os.write((char*) &cart.romBank1, sizeof(cart.romBank1));
    os.write((char*) &cart.sramBank, sizeof(cart.sramBank));
    os.write((char*) &cart.sramEnabled, sizeof(cart.sramEnabled));

    switch(cart.mbcType) {
        case MBC1:
            os.write((char*) &cart.mbc1Mode, sizeof(cart.mbc1Mode));
            break;
        case MBC3:
            os.write((char*) &cart.mbc3Ctrl, sizeof(cart.mbc3Ctrl));
            break;
        case MBC6:
            os.write((char*) &cart.mbc6RomBank1A, sizeof(cart.mbc6RomBank1A));
            os.write((char*) &cart.mbc6RomBank1B, sizeof(cart.mbc6RomBank1B));
            os.write((char*) &cart.mbc6SramBankA, sizeof(cart.mbc6SramBankA));
            os.write((char*) &cart.mbc6SramBankB, sizeof(cart.mbc6SramBankB));
            break;
        case MBC7:
            os.write((char*) &cart.mbc7, sizeof(cart.mbc7));
            break;
        case HUC1:
            os.write((char*) &cart.huc1RamMode, sizeof(cart.huc1RamMode));
            break;
        case HUC3:
            os.write((char*) &cart.huc3Mode, sizeof(cart.huc3Mode));
            os.write((char*) &cart.huc3Value, sizeof(cart.huc3Value));
            os.write((char*) &cart.huc3Shift, sizeof(cart.huc3Shift));
            break;
        case MMM01:
            os.write((char*) &cart.mmm01BankSelected, sizeof(cart.mmm01BankSelected));
            os.write((char*) &cart.mmm01RomBaseBank, sizeof(cart.mmm01RomBaseBank));
            break;
        case CAMERA:
            os.write((char*) &cart.cameraIO, sizeof(cart.cameraIO));
            os.write((char*) &cart.cameraReadyCycle, sizeof(cart.cameraReadyCycle));
            os.write((char*) cart.cameraRegs, sizeof(cart.cameraRegs));
            break;
        case TAMA5:
            os.write((char*) &cart.tama5, sizeof(cart.tama5));
        default:
            break;
    }

    return os;
}

u8 Cartridge::readSram(u16 addr) {
    u8 bank = this->sramBank & (this->totalRamBanks - 1);
    if(bank < this->totalRamBanks) {
        return this->sram[bank * SRAM_BANK_SIZE + (addr & SRAM_BANK_MASK)];
    } else {
        if(this->gameboy->settings.printDebug != nullptr) {
            this->gameboy->settings.printDebug("Attempted to read from invalid SRAM bank: %" PRIu8 "\n", bank);
        }

        return 0xFF;
    }
}

void Cartridge::writeSram(u16 addr, u8 val) {
    u8 bank = this->sramBank & (this->totalRamBanks - 1);
    if(bank < this->totalRamBanks) {
        this->sram[bank * SRAM_BANK_SIZE + (addr & SRAM_BANK_MASK)] = val;
    } else {
        if(this->gameboy->settings.printDebug != nullptr) {
            this->gameboy->settings.printDebug("Attempted to write to invalid SRAM bank: %" PRIu8 "\n", bank);
        }
    }
}

void Cartridge::mapRomBank0() {
    u16 bank = this->romBank0 & (this->totalRomBanks - 1);
    if(bank < this->totalRomBanks) {
        u8* bankPtr = &this->rom[bank * ROM_BANK_SIZE];

        this->gameboy->mmu.mapBankBlock(0x0, bankPtr + 0x0000);
        this->gameboy->mmu.mapBankBlock(0x1, bankPtr + 0x1000);
        this->gameboy->mmu.mapBankBlock(0x2, bankPtr + 0x2000);
        this->gameboy->mmu.mapBankBlock(0x3, bankPtr + 0x3000);
    } else {
        if(this->gameboy->settings.printDebug != nullptr) {
            this->gameboy->settings.printDebug("Attempted to access invalid ROM bank: %" PRIu16 "\n", bank);
        }

        this->gameboy->mmu.mapBankBlock(0x0, nullptr);
        this->gameboy->mmu.mapBankBlock(0x1, nullptr);
        this->gameboy->mmu.mapBankBlock(0x2, nullptr);
        this->gameboy->mmu.mapBankBlock(0x3, nullptr);
    }
}

void Cartridge::mapRomBank1() {
    if(this->mbcType == MBC6) {
        u16 totalHalfBanks = this->totalRomBanks << 1;

        u8 bank1A = this->mbc6RomBank1A & (totalHalfBanks - 1);
        if(bank1A < totalHalfBanks) {
            u8* bankPtr = &this->rom[bank1A * HALF_ROM_BANK_SIZE];

            this->gameboy->mmu.mapBankBlock(0x4, bankPtr + 0x0000);
            this->gameboy->mmu.mapBankBlock(0x5, bankPtr + 0x1000);
        } else {
            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("Attempted to access invalid ROM half-bank: %" PRIu8 "\n", bank1A);
            }

            this->gameboy->mmu.mapBankBlock(0x4, nullptr);
            this->gameboy->mmu.mapBankBlock(0x5, nullptr);
        }

        u8 bank1B = this->mbc6RomBank1B & (totalHalfBanks - 1);
        if(bank1B < totalHalfBanks) {
            u8* bankPtr = &this->rom[bank1B * HALF_ROM_BANK_SIZE];

            this->gameboy->mmu.mapBankBlock(0x6, bankPtr + 0x0000);
            this->gameboy->mmu.mapBankBlock(0x7, bankPtr + 0x1000);
        } else {
            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("Attempted to access invalid ROM half-bank: %" PRIu8 "\n", bank1B);
            }

            this->gameboy->mmu.mapBankBlock(0x6, nullptr);
            this->gameboy->mmu.mapBankBlock(0x7, nullptr);
        }
    } else {
        u16 bank = this->romBank1 & (this->totalRomBanks - 1);
        if(bank < this->totalRomBanks) {
            u8* bankPtr = &this->rom[bank * ROM_BANK_SIZE];

            this->gameboy->mmu.mapBankBlock(0x4, bankPtr + 0x0000);
            this->gameboy->mmu.mapBankBlock(0x5, bankPtr + 0x1000);
            this->gameboy->mmu.mapBankBlock(0x6, bankPtr + 0x2000);
            this->gameboy->mmu.mapBankBlock(0x7, bankPtr + 0x3000);
        } else {
            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("Attempted to access invalid ROM bank: %" PRIu16 "\n", bank);
            }

            this->gameboy->mmu.mapBankBlock(0x4, nullptr);
            this->gameboy->mmu.mapBankBlock(0x5, nullptr);
            this->gameboy->mmu.mapBankBlock(0x6, nullptr);
            this->gameboy->mmu.mapBankBlock(0x7, nullptr);
        }
    }
}

void Cartridge::mapSramBank() {
    if(this->sramEnabled && this->totalRamBanks > 0) {
        if(this->mbcType == MBC6) {
            u16 totalHalfBanks = this->totalRamBanks << 1;

            u8 bankA = this->mbc6SramBankA & (totalHalfBanks - 1);
            if(bankA < totalHalfBanks) {
                u8* bankPtr = &this->sram[bankA * HALF_SRAM_BANK_SIZE];

                this->gameboy->mmu.mapBankBlock(0xA, bankPtr);
            } else {
                if(this->gameboy->settings.printDebug != nullptr) {
                    this->gameboy->settings.printDebug("Attempted to access invalid SRAM half-bank: %" PRIu8 "\n", bankA);
                }

                this->gameboy->mmu.mapBankBlock(0xA, nullptr);
            }

            u8 bankB = this->mbc6SramBankB & (totalHalfBanks - 1);
            if(bankB < totalHalfBanks) {
                u8* bankPtr = &this->sram[bankB * HALF_SRAM_BANK_SIZE];

                this->gameboy->mmu.mapBankBlock(0xB, bankPtr);
            } else {
                if(this->gameboy->settings.printDebug != nullptr) {
                    this->gameboy->settings.printDebug("Attempted to access invalid SRAM half-bank: %" PRIu8 "\n", bankB);
                }

                this->gameboy->mmu.mapBankBlock(0xB, nullptr);
            }
        } else {
            u8 bank = this->sramBank & (this->totalRamBanks - 1);
            if(bank < this->totalRamBanks) {
                u8* bankPtr = &this->sram[bank * SRAM_BANK_SIZE];

                this->gameboy->mmu.mapBankBlock(0xA, bankPtr + 0x0000);
                this->gameboy->mmu.mapBankBlock(0xB, bankPtr + 0x1000);
            } else {
                // Only report if there's no chance it's a special bank number.
                if(this->readFunc == nullptr && this->gameboy->settings.printDebug != nullptr) {
                    this->gameboy->settings.printDebug("Attempted to access invalid SRAM bank: %" PRIu8 "\n", bank);
                }

                this->gameboy->mmu.mapBankBlock(0xA, nullptr);
                this->gameboy->mmu.mapBankBlock(0xB, nullptr);
            }
        }
    } else {
        this->gameboy->mmu.mapBankBlock(0xA, nullptr);
        this->gameboy->mmu.mapBankBlock(0xB, nullptr);
    }
}

void Cartridge::mapBanks() {
    this->mapRomBank0();
    this->mapRomBank1();
    this->mapSramBank();
}

/* MBC SRAM Read handlers */

/* MBC3 */
u8 Cartridge::m3r(u16 addr) {
    if(this->sramEnabled) {
        // Check for RTC register
        switch(this->sramBank) {
            case 0x8:
                return (u8) this->rtcClock.seconds;
            case 0x9:
                return (u8) this->rtcClock.minutes;
            case 0xA:
                return (u8) this->rtcClock.hours;
            case 0xB:
                return (u8) (this->rtcClock.days & 0xFF);
            case 0xC:
                return this->mbc3Ctrl;
            default: // Not an RTC register
                return this->readSram((u16) (addr & SRAM_BANK_MASK));
        }
    }

    return 0xFF;
}

/* MBC7 */
u8 Cartridge::m7r(u16 addr) {
    switch(addr & 0xF0) {
        case 0x20:
            return (u8) (this->mbc7.tiltX & 0xFF);
        case 0x30:
            return (u8) ((this->mbc7.tiltX >> 8) & 0xFF);
        case 0x40:
            return (u8) (this->mbc7.tiltY & 0xFF);
        case 0x50:
            return (u8) ((this->mbc7.tiltY >> 8) & 0xFF);
        case 0x60:
            return 0;
        case 0x80:
            return this->mbc7.status;
        default:
            return 0xFF;
    }
}

/* HuC3 */
u8 Cartridge::h3r(u16 addr) {
    switch(this->huc3Mode) {
        case 0xA:
            return this->readSram((u16) (addr & SRAM_BANK_MASK));
        case 0xC:
            return this->huc3Value;
        case 0xB:
        case 0xD:
            /* Return 1 as a fixed value, needed for some games to
             * boot, the meaning is unknown. */
            return 1;
        default:
            break;
    }

    return 0xFF;
}

#define TAMA5_NUM_WRITE_REGISTERS 8

#define TAMA5_ROM_BANK_LOW 0
#define TAMA5_ROM_BANK_HIGH 1
#define TAMA5_SRAM_WRITE_LOW 4
#define TAMA5_SRAM_WRITE_HIGH 5
#define TAMA5_SRAM_ADDR_CTRL 6
#define TAMA5_SRAM_ADDR_LOW 7

#define TAMA5_ENABLED 10
#define TAMA5_SRAM_READ_LOW 12
#define TAMA5_SRAM_READ_HIGH 13

#define TAMA5_SRAM_CTRL_ADDR_HIGH 1
#define TAMA5_SRAM_CTRL_READ 2
#define TAMA5_SRAM_CTRL_ALARM 4
#define TAMA5_SRAM_CTRL_RTC 8

/* TAMA5 */
u8 Cartridge::t5r(u16 addr) {
    if((addr & 1) == 0) {
        u8 val = 0xF0;

        if(this->tama5.enabled) {
            switch(this->tama5.reg) {
                case TAMA5_ENABLED:
                    val |= this->tama5.enabled;
                    break;
                case TAMA5_SRAM_READ_LOW:
                case TAMA5_SRAM_READ_HIGH: {
                    u8 ctrl = this->tama5.registers[TAMA5_SRAM_ADDR_CTRL];
                    u8 addrLow = this->tama5.registers[TAMA5_SRAM_ADDR_LOW];

                    if((ctrl & TAMA5_SRAM_CTRL_READ) != 0) {
                        if((ctrl & TAMA5_SRAM_CTRL_RTC) == 0) {
                            val |= (this->readSram(((ctrl & TAMA5_SRAM_CTRL_ADDR_HIGH) << 4) | addrLow) >> ((this->tama5.reg & 1) << 2)) & 0xF;
                        } else {
                            // TODO: RTC
                        }
                    }

                    break;
                }
                default:
                    val |= 0x1;
                    break;
            }
        }

        return val;
    } else {
        return 0xFF;
    }
}

/* CAMERA */
u8 Cartridge::camr(u16 addr) {
    if(this->cameraIO) {
        u8 reg = (u8) (addr & 0x7F);
        if(reg == 0) {
            return this->cameraRegs[reg];
        }

        return 0;
    } else if(this->sramEnabled) {
        return this->readSram((u16) (addr & SRAM_BANK_MASK));
    }

    return 0xFF;
}

/* MBC Write handlers */

/* MBC1 */
void Cartridge::m1w(u16 addr, u8 val) {
    switch(addr >> 13) {
        case 0x0: /* 0000 - 1FFF */
            this->sramEnabled = (val & 0xF) == 0xA;
            this->mapSramBank();
            break;
        case 0x1: /* 2000 - 3FFF */
            val &= 0x1F;
            val = val ? val : 1; // Writing 0 for the lower 5 bits translates to 1.

            if(this->rockmanMapper) {
                // ROCKMAN 99 accesses banks in a different way.
                this->romBank1 = (u8) ((val > 0xF) ? val - 8 : val);
            } else if(this->multicart) {
                // Multicarts only have 4 lower bits.
                this->romBank1 = (u8) ((this->romBank1 & 0x30) | (val & 0xF));
            } else {
                // Regular MBC1 mappers have 5 lower bits.
                this->romBank1 = (u8) ((this->romBank1 & 0x60) | val);
            }

            this->mapRomBank1();
            break;
        case 0x2: /* 4000 - 5FFF */
            val &= 0x03;

            if(this->mbc1Mode) { /* RAM mode */
                if(this->multicart) {
                    // Set ROM bank 0.
                    this->romBank0 = val << 4;
                    this->mapRomBank0();
                }

                // Set SRAM bank.
                this->sramBank = val;
                this->mapSramBank();
            } else {
                // Set ROM bank 1.
                this->romBank1 = (u8) ((val << (this->multicart ? 4 : 5)) | (this->romBank1 & 0x1F));
                this->mapRomBank1();
            }

            break;
        case 0x3: /* 6000 - 7FFF */
            this->mbc1Mode = (bool) (val & 1);

            if(this->mbc1Mode) {
                if(this->multicart) {
                    // Map ROM bank 0 from upper bits of ROM bank 1.
                    this->romBank0 = this->romBank1 & 0x30;
                    this->mapRomBank0();
                } else {
                    // Cannot access ROM banks after 0x1F in RAM mode.
                    this->romBank1 &= 0x1F;
                    this->mapRomBank1();
                }
            } else {
                if(this->multicart) {
                    // Reset ROM bank 0 to base.
                    this->romBank0 = 0;
                    this->mapRomBank0();
                }

                // Cannot access other RAM banks in ROM mode.
                this->sramBank = 0;
                this->mapSramBank();
            }

            break;
        default:
            break;
    }
}

/* MBC2 */
void Cartridge::m2w(u16 addr, u8 val) {
    switch(addr >> 13) {
        case 0x0: /* 0000 - 1FFF */
            // Least significant bit of upper address byte must be 0.
            if((addr & 0x100) == 0) {
                this->sramEnabled = (val & 0xF) == 0xA;
                this->mapSramBank();
            }

            break;
        case 0x1: /* 2000 - 3FFF */
            val &= 0xF;

            // Least significant bit of upper address byte must be 1.
            if((addr & 0x100) != 0) {
                this->romBank1 = val ? val : 1;
                this->mapRomBank1();
            }

            break;
        default:
            break;
    }
}

/* MBC3 */
void Cartridge::m3w(u16 addr, u8 val) {
    switch(addr >> 13) {
        case 0x0: /* 0000 - 1FFF */
            this->sramEnabled = (val & 0xF) == 0xA;
            this->mapSramBank();
            break;
        case 0x1: /* 2000 - 3FFF */
            val &= 0x7F;

            this->romBank1 = val ? val : 1;
            this->mapRomBank1();
            break;
        case 0x2: /* 4000 - 5FFF */
            /* The RTC register is selected by writing values 0x8-0xC, ram banks
             * are selected by values 0x0-0x7 */
            this->sramBank = val;
            this->mapSramBank();
            break;
        case 0x3: /* 6000 - 7FFF */
            if(val) {
                this->latchClock();

                if(this->rtcClock.days > 0x1FF) {
                    this->mbc3Ctrl |= 0x80;
                    this->rtcClock.days &= 0x1FF;
                }

                this->mbc3Ctrl = (u8) ((this->mbc3Ctrl & 0xFE) | ((this->rtcClock.days >> 8) & 1));
            }

            break;
        default:
            break;
    }
}

/* MBC5 */
void Cartridge::m5w(u16 addr, u8 val) {
    switch(addr >> 12) {
        case 0x0: /* 0000 - 1FFF */
        case 0x1:
            this->sramEnabled = (val & 0xF) == 0xA;
            this->mapSramBank();
            break;
        case 0x2: /* 2000 - 2FFF */
            this->romBank1 = (this->romBank1 & 0x100) | val;
            this->mapRomBank1();
            break;
        case 0x3: /* 3000 - 3FFF */
            this->romBank1 = (this->romBank1 & 0xFF) | ((val & 1) << 8);
            this->mapRomBank1();
            break;
        case 0x4: /* 4000 - 5FFF */
        case 0x5:
            /* MBC5 might have a rumble motor, which is triggered by the
             * 4th bit of the value written */
            this->gameboy->settings.setRumble(this->rumble && (val & 0x8) != 0);
            this->sramBank = this->rumble ? (val & 0x7) : (val & 0xF);
            this->mapSramBank();
            break;
        default:
            break;
    }
}

/* MBC6 */
void Cartridge::m6w(u16 addr, u8 val) {
    switch(addr >> 10) {
        case 0x0: /* 0000 - 03FF */
            this->sramEnabled = ((val & 0xF) == 0xA);
            this->mapSramBank();
            break;
        case 0x1: /* 0400 - 07FF */
            this->mbc6SramBankA = val;
            this->mapSramBank();
            break;
        case 0x2: /* 0800 - 0BFF */
            this->mbc6SramBankB = val;
            this->mapSramBank();
            break;
        case 0x3: /* 0C00 - 0FFF */
            // TODO: Flash Control Register
            break;
        case 0x4: /* 1000 - 1FFF */
        case 0x5:
        case 0x6:
        case 0x7:
            // TODO: Flash Control Register Enable
            break;
        case 0x8: /* 2000 - 27FF */
        case 0x9:
            // TODO: Additional behavior.
            this->mbc6RomBank1A = val;
            this->mapRomBank1();
            break;
        case 0xA: /* 2800 - 2FFF */
        case 0xB:
            // TODO: ROM vs Flash
            break;
        case 0xC: /* 3000 - 37FF */
        case 0xD:
            // TODO: Additional behavior.
            this->mbc6RomBank1B = val;
            this->mapRomBank1();
            break;
        case 0xE: /* 3800 - 3FFF */
        case 0xF:
            // TODO: ROM vs Flash
            break;
        default:
            break;
    }
}

/* MBC7 */
void Cartridge::m7w(u16 addr, u8 val) {
    switch(addr >> 13) {
        case 0x0: /* 0000 - 1FFF */
            if((val & 0xF) == 0xA) {
                this->mbc7.access |= 1;
            } else {
                this->mbc7.access &= ~1;
            }

            break;
        case 0x1: /* 2000 - 3FFF */
            this->romBank1 = val & 0x7F;
            this->mapRomBank1();
            break;
        case 0x2: /* 4000 - 5FFF */
            if(val == 0x40) {
                this->mbc7.access |= 2;
            } else {
                this->mbc7.access &= ~2;
            }

            break;
        default:
            break;
    }
}

/* MMM01 */
void Cartridge::mmm01w(u16 addr, u8 val) {
    switch(addr >> 13) {
        case 0x0: /* 0000 - 1FFF */
            if(this->mmm01BankSelected) {
                this->sramEnabled = (val & 0xF) == 0xA;
                this->mapSramBank();
            } else {
                this->mmm01BankSelected = true;

                this->romBank0 = this->mmm01RomBaseBank;
                this->mapRomBank0();

                this->romBank1 = this->mmm01RomBaseBank + 1;
                this->mapRomBank1();
            }

            break;
        case 0x1: /* 2000 - 3FFF */
            if(this->mmm01BankSelected) {
                this->romBank1 = this->mmm01RomBaseBank + (val ? val : 1);
            this->mapRomBank1();
            } else {
                this->mmm01RomBaseBank = (u8) (((val & 0x3F) & (this->totalRomBanks - 1)) + 2);
            }

            break;
        case 0x2: /* 4000 - 5FFF */
            if(this->mmm01BankSelected) {
                this->sramBank = val;
                this->mapSramBank();
            }

            break;
        default:
            break;
    }
}

/* HUC1 */
void Cartridge::h1w(u16 addr, u8 val) {
    switch(addr >> 13) {
        case 0x0: /* 0000 - 1FFF */
            this->sramEnabled = (val & 0xF) == 0xA;
            this->mapSramBank();
            break;
        case 0x1: /* 2000 - 3FFF */
            this->romBank1 = (u8) (val & 0x3F);
            this->mapRomBank1();
            break;
        case 0x2: /* 4000 - 5FFF */
            val &= 3;
            if(this->huc1RamMode) {
                this->sramBank = val;
                this->mapSramBank();
            } else {
                this->romBank1 = val;
                this->mapRomBank1();
            }

            break;
        case 0x3: /* 6000 - 7FFF */
            this->huc1RamMode = (bool) (val & 1);
            break;
        default:
            break;
    }
}

/* HuC3 */
void Cartridge::h3w(u16 addr, u8 val) {
    switch(addr >> 13) {
        case 0x0: /* 0000 - 1FFF */
            this->huc3Mode = val;
            break;
        case 0x1: /* 2000 - 3FFF */
            this->romBank1 = val ? val : 1;
            this->mapRomBank1();
            break;
        case 0x2: /* 4000 - 5FFF */
            this->sramBank = val & 0xF;
            this->mapSramBank();
            break;
        default:
            break;
    }
}

/* CAMERA */
void Cartridge::camw(u16 addr, u8 val) {
    switch(addr >> 13) {
        case 0x0: /* 0000 - 1FFF */
            this->sramEnabled = (val & 0xF) == 0xA;
            this->mapSramBank();
            break;
        case 0x1: /* 2000 - 3FFF */
            this->romBank1 = val ? val : 1;
            this->mapRomBank1();
            break;
        case 0x2: /* 4000 - 5FFF */
            this->cameraIO = (val & 0x10) != 0;
            if(!this->cameraIO) {
                this->sramBank = val & 0xF;
                this->mapSramBank();
            }

            break;
        default:
            break;
    }
}

/* MBC SRAM Write handlers */

/* MBC2 */
void Cartridge::m2ws(u16 addr, u8 val) {
    if(this->sramEnabled) {
        this->writeSram((u16) (addr & 0x1FF), (u8) (val & 0xF));
    }
}

/* MBC3 */
void Cartridge::m3ws(u16 addr, u8 val) {
    if(this->sramEnabled) {
        switch(this->sramBank) { // Check for RTC register
            case 0x8:
                this->rtcClock.seconds = val;
                break;
            case 0x9:
                this->rtcClock.minutes = val;
                break;
            case 0xA:
                this->rtcClock.hours = val;
                break;
            case 0xB:
                this->rtcClock.days = (this->rtcClock.days & 0x100) | val;
                break;
            case 0xC:
                this->rtcClock.days = ((val & 1) << 8) | (this->rtcClock.days & 0xFF);
                this->mbc3Ctrl = val;
                break;
            default: // Not an RTC register
                this->writeSram((u16) (addr & SRAM_BANK_MASK), val);
                break;
        }
    }
}

#define MBC7_STATE_IDLE 0
#define MBC7_STATE_READ_COMMAND 1
#define MBC7_STATE_DO 2

#define MBC7_STATE_EEPROM_EWDS 0x10
#define MBC7_STATE_EEPROM_WRAL 0x11
#define MBC7_STATE_EEPROM_ERAL 0x12
#define MBC7_STATE_EEPROM_EWEN 0x13
#define MBC7_STATE_EEPROM_WRITE 0x14
#define MBC7_STATE_EEPROM_READ 0x18
#define MBC7_STATE_EEPROM_ERASE 0x1C

#define MBC7_STATUS_DO 0x01
#define MBC7_STATUS_DI 0x02
#define MBC7_STATUS_CLK 0x40
#define MBC7_STATUS_CS 0x80

/* MBC7 */
void Cartridge::m7ws(u16 addr, u8 val) {
    if(this->mbc7.access == 3) {
        switch(addr & 0xF0) {
            case 0x00:
                this->mbc7.latch = (u8) ((val & 0x55) == 0x55);
                if(this->mbc7.latch) {
                    this->mbc7.tiltX = 0x8000;
                    this->mbc7.tiltY = 0x8000;
                }

                break;
            case 0x10:
                this->mbc7.latch |= val & 0xAA;
                if(this->mbc7.latch == 0xAB) {
                    if(this->gameboy->settings.readTilt != nullptr) {
                        this->gameboy->settings.readTilt(&this->mbc7.tiltX, &this->mbc7.tiltY);
                    } else {
                        this->mbc7.tiltX = 0x7FF;
                        this->mbc7.tiltY = 0x7FF;
                    }
                }

                this->mbc7.latch = 0;
                break;
            case 0x80: {
                u8 oldEeprom = this->mbc7.status;
                this->mbc7.status = val | MBC7_STATUS_DO;

                if((oldEeprom & MBC7_STATUS_CS) == 0 && (this->mbc7.status & MBC7_STATUS_CS) != 0) {
                    this->mbc7.state = MBC7_STATE_IDLE;
                }

                if((oldEeprom & MBC7_STATUS_CLK) == 0 && (this->mbc7.status & MBC7_STATUS_CLK) != 0) {
                    if(this->mbc7.state == MBC7_STATE_READ_COMMAND || this->mbc7.state == MBC7_STATE_EEPROM_WRITE || this->mbc7.state == MBC7_STATE_EEPROM_ERASE) {
                        this->mbc7.sr <<= 1;
                        this->mbc7.srBits++;

                        if((this->mbc7.status & MBC7_STATUS_DI) != 0) {
                            this->mbc7.sr |= 1;
                        }
                    }

                    switch(this->mbc7.state) {
                        case MBC7_STATE_IDLE:
                            if((this->mbc7.status & MBC7_STATUS_DI) != 0) {
                                this->mbc7.state = MBC7_STATE_READ_COMMAND;
                                this->mbc7.sr = 0;
                                this->mbc7.srBits = 0;
                            }

                            break;
                        case MBC7_STATE_READ_COMMAND:
                            if(this->mbc7.srBits == 10) {
                                this->mbc7.state = 0x10 | (this->mbc7.sr >> 6);
                                if((this->mbc7.state & 0xC) != 0) {
                                    this->mbc7.state &= ~0x3;
                                }

                                this->mbc7.srBits = 0;
                                this->mbc7.address = this->mbc7.sr & 0x7F;
                            }

                            break;
                        case MBC7_STATE_DO:
                            if(this->mbc7.sr >> 15) {
                                this->mbc7.status |= MBC7_STATUS_DO;
                            } else {
                                this->mbc7.status &= ~MBC7_STATUS_DO;
                            }

                            this->mbc7.sr <<= 1;
                            this->mbc7.srBits--;
                            if(this->mbc7.srBits == 0) {
                                this->mbc7.state = MBC7_STATE_IDLE;
                            }

                            break;
                        default:
                            break;
                    }

                    switch(this->mbc7.state) {
                        case MBC7_STATE_EEPROM_EWDS:
                            this->mbc7.writable = false;
                            this->mbc7.state = MBC7_STATE_IDLE;
                            break;
                        case MBC7_STATE_EEPROM_WRAL:
                            if(this->mbc7.srBits == 16) {
                                if(this->mbc7.writable) {
                                    for(u8 i = 0; i < 128; i++) {
                                        this->writeSram((u16) (i * 2), (u8) (this->mbc7.sr >> 8));
                                        this->writeSram((u16) (i * 2 + 1), (u8) (this->mbc7.sr & 0xFF));
                                    }
                                }

                                this->mbc7.state = MBC7_STATE_IDLE;
                            }

                            break;
                        case MBC7_STATE_EEPROM_ERAL:
                            if(this->mbc7.writable) {
                                for(u8 i = 0; i < 128; i++) {
                                    this->writeSram((u16) (i * 2), 0xFF);
                                    this->writeSram((u16) (i * 2 + 1), 0xFF);
                                }
                            }

                            this->mbc7.state = MBC7_STATE_IDLE;
                            break;
                        case MBC7_STATE_EEPROM_EWEN:
                            this->mbc7.writable = true;
                            this->mbc7.state = MBC7_STATE_IDLE;
                            break;
                        case MBC7_STATE_EEPROM_WRITE:
                            if(this->mbc7.srBits == 16) {
                                if(this->mbc7.writable) {
                                    this->writeSram((u16) (this->mbc7.address * 2), (u8) (this->mbc7.sr >> 8));
                                    this->writeSram((u16) (this->mbc7.address * 2 + 1), (u8) (this->mbc7.sr & 0xFF));
                                }

                                this->mbc7.state = MBC7_STATE_IDLE;
                            }

                            break;
                        case MBC7_STATE_EEPROM_READ:
                            this->mbc7.srBits = 16;
                            this->mbc7.sr = this->readSram((u16) (this->mbc7.address * 2)) << 8;
                            this->mbc7.sr |= this->readSram((u16) (this->mbc7.address * 2 + 1));
                            this->mbc7.state = MBC7_STATE_DO;
                            this->mbc7.status &= ~MBC7_STATUS_DO;
                            break;
                        case MBC7_STATE_EEPROM_ERASE:
                            if(this->mbc7.writable) {
                                this->writeSram((u16) (this->mbc7.address * 2), 0xFF);
                                this->writeSram((u16) (this->mbc7.address * 2 + 1), 0xFF);
                            }

                            this->mbc7.state = MBC7_STATE_IDLE;
                            break;
                        default:
                            break;
                    }
                } else if((this->mbc7.status & MBC7_STATUS_CS) != 0 && (oldEeprom & MBC7_STATUS_CLK) != 0 && (this->mbc7.status & MBC7_STATUS_CLK) == 0) {
                    if((oldEeprom & MBC7_STATUS_DO) == MBC7_STATUS_DO) {
                        this->mbc7.status |= MBC7_STATUS_DO;
                    } else {
                        this->mbc7.status &= ~MBC7_STATUS_DO;
                    }
                }

                break;
            }
        }
    }
}

/* MMM01 */
void Cartridge::mmm01ws(u16 addr, u8 val) {
    if(this->mmm01BankSelected && this->sramEnabled) {
        this->writeSram((u16) (addr & SRAM_BANK_MASK), val);
    }
}

/* HuC3 */
void Cartridge::h3ws(u16 addr, u8 val) {
    switch(this->huc3Mode) {
        case 0xA:
            this->writeSram((u16) (addr & SRAM_BANK_MASK), val);
            break;
        case 0xB:
            switch(val & 0xF0) {
                case 0x10: /* Read clock */
                    if(this->huc3Shift > 24) {
                        break;
                    }

                    switch(this->huc3Shift) {
                        case 0:
                        case 4:
                        case 8: /* Minutes */
                            this->huc3Value = (u8) ((this->rtcClock.minutes >> this->huc3Shift) & 0xF);
                            break;
                        case 12:
                        case 16:
                        case 20: /* Days */
                            this->huc3Value = (u8) ((this->rtcClock.days >> (this->huc3Shift - 12)) & 0xF);
                            break;
                        case 24: /* Year */
                            this->huc3Value = (u8) (this->rtcClock.years & 0xF);
                            break;
                        default:
                            break;
                    }

                    this->huc3Shift += 4;
                    break;
                case 0x40:
                    switch(val & 0xF) {
                        case 0:
                        case 4:
                        case 7:
                            this->huc3Shift = 0;
                            break;
                        default:
                            break;
                    }

                    this->latchClock();
                    break;
                case 0x50:
                    break;
                case 0x60:
                    this->huc3Value = 1;
                    break;
                default:
                    if(this->gameboy->settings.printDebug != nullptr) {
                        this->gameboy->settings.printDebug("Unhandled HuC3 command 0x%02X.\n", val);
                    }

                    break;
            }

            break;
        case 0xC:
        case 0xD:
        case 0xE:
            break;
        default:
            break;
    }
}

/* TAMA5 */
void Cartridge::t5ws(u16 addr, u8 val) {
    if((addr & 1) == 0) {
        val &= 0xF;

        if(this->tama5.reg < TAMA5_NUM_WRITE_REGISTERS) {
            this->tama5.registers[this->tama5.reg] = val;

            switch (this->tama5.reg) {
                case TAMA5_ROM_BANK_LOW:
                case TAMA5_ROM_BANK_HIGH:
                    this->romBank1 = this->tama5.registers[TAMA5_ROM_BANK_LOW] | (this->tama5.registers[TAMA5_ROM_BANK_HIGH] << 4);
                    this->mapRomBank1();
                    break;
                case TAMA5_SRAM_ADDR_LOW: {
                    u8 ctrl = this->tama5.registers[TAMA5_SRAM_ADDR_CTRL];
                    u8 addrLow = this->tama5.registers[TAMA5_SRAM_ADDR_LOW];
                    u8 dataLow = this->tama5.registers[TAMA5_SRAM_WRITE_LOW];
                    u8 dataHigh = this->tama5.registers[TAMA5_SRAM_WRITE_HIGH];

                    if((ctrl & TAMA5_SRAM_CTRL_READ) == 0) {
                        if((ctrl & TAMA5_SRAM_CTRL_RTC) == 0) {
                            this->writeSram(((ctrl & TAMA5_SRAM_CTRL_ADDR_HIGH) << 4) | addrLow, (dataHigh << 4) | dataLow);
                        } else {
                            // TODO: RTC
                        }
                    }

                    break;
                }
                default:
                    break;
            }
        }
    } else {
        if(val == 0xA) {
            this->tama5.enabled = true;
        }

        this->tama5.reg = val;
    }
}

/* CAMERA */
void Cartridge::camws(u16 addr, u8 val) {
    if(this->cameraIO) {
        u8 reg = (u8) (addr & 0x7F);
        if(reg < 0x36) {
            this->cameraRegs[reg] = val;
            if(reg == 0) {
                this->cameraRegs[reg] &= 0x7;

                if(val & 0x1) {
                    this->camTakePicture();
                }
            }
        }
    } else if(this->sramEnabled) {
        this->writeSram((u16) (addr & SRAM_BANK_MASK), val);
    }
}

/* MBC Update functions */

/* CAMERA */
void Cartridge::camu() {
    if(this->cameraReadyCycle != 0 && this->gameboy->cpu.getCycle() >= this->cameraReadyCycle) {
        this->cameraRegs[0] &= ~0x1;
        this->cameraReadyCycle = 0;
    }
}

#define CAMERA_SENSOR_EXTRA_LINES 8
#define CAMERA_SENSOR_WIDTH 128
#define CAMERA_SENSOR_HEIGHT (112 + CAMERA_SENSOR_EXTRA_LINES)
#define CAMERA_EXPOSURE_REFERENCE 512

#define CAMERA_PROCESSED_WIDTH 128
#define CAMERA_PROCESSED_HEIGHT 112

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(min, v, max) MIN(MAX((v), (min)), (max))

static const float edgeRatioLUT[8] = {0.50, 0.75, 1.00, 1.25, 2.00, 3.00, 4.00, 5.00};

void Cartridge::camTakePicture() {
    u32 pBits = (u32) (((this->cameraRegs[0] >> 1) & 3) != 0);
    u32 mBits = (u32) ((this->cameraRegs[0] >> 1) & 2);
    u32 nBit = (u32) ((this->cameraRegs[1] & 0x80) >> 7);
    u32 vhBits = (u32) ((this->cameraRegs[1] & 0x60) >> 5);
    u32 exposureBits = this->cameraRegs[3] | (this->cameraRegs[2] << 8);
    u32 iBit = (u32) ((this->cameraRegs[4] & 0x08) >> 3);
    float edgeAlpha = edgeRatioLUT[(this->cameraRegs[4] & 0x70) >> 4];
    u32 e3Bit = (u32) ((this->cameraRegs[4] & 0x80) >> 7);

    this->cameraReadyCycle = this->gameboy->cpu.getCycle() + exposureBits * 64 + (nBit ? 0 : 2048) + 129784;
    this->gameboy->cpu.setEventCycle(this->cameraReadyCycle);

    if(this->gameboy->settings.getCameraImage == nullptr) {
        return;
    }

    u32* image = this->gameboy->settings.getCameraImage();
    if(image == nullptr) {
        return;
    }

    int filtered[CAMERA_SENSOR_WIDTH * CAMERA_SENSOR_HEIGHT];

    for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
        u32* line = &image[y * CAMERA_SENSOR_WIDTH];
        for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
            int val = (int) CLAMP(0, ((((((2 * (line[x] & 0xFF) + 5 * ((line[x] >> 8) & 0xFF) + 1 * ((line[x] >> 16) & 0xFF)) >> 3) * exposureBits) / CAMERA_EXPOSURE_REFERENCE) - 128) / 8) + 128, 255);
            if(iBit) {
                val = 255 - val;
            }

            filtered[y * CAMERA_SENSOR_WIDTH + x] = val - 128;
        }
    }

    int tempBuf[CAMERA_SENSOR_WIDTH * CAMERA_SENSOR_HEIGHT];

    u32 filterMode = (nBit << 3) | (vhBits << 1) | e3Bit;
    switch(filterMode) {
        case 0x0:
            memcpy(tempBuf, filtered, sizeof(tempBuf));

            for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
                int* line = &filtered[y * CAMERA_SENSOR_WIDTH];
                int* tempLine = &tempBuf[y * CAMERA_SENSOR_WIDTH];
                int* tempLineS = &tempBuf[MIN(y + 1, CAMERA_SENSOR_HEIGHT - 1) * CAMERA_SENSOR_WIDTH];

                for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
                    int value = 0;
                    if(pBits & 0x1) value += tempLine[x];
                    if(pBits & 0x2) value += tempLineS[x];
                    if(mBits & 0x1) value -= tempLine[x];
                    if(mBits & 0x2) value -= tempLineS[x];
                    line[x] = CLAMP(-128, value, 127);
                }
            }

            break;
        case 0x1:
            memset(filtered, 0, sizeof(filtered));
            break;
        case 0x2:
            for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
                int* line = &filtered[y * CAMERA_SENSOR_WIDTH];
                int* tempLine = &tempBuf[y * CAMERA_SENSOR_WIDTH];

                for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
                    tempLine[x] = CLAMP(0, (int) (line[x] + ((2 * line[x] - line[MAX(0, x - 1)] - line[MIN(x + 1, CAMERA_SENSOR_WIDTH - 1)]) * edgeAlpha)), 255);
                }
            }

            for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
                int* line = &filtered[y * CAMERA_SENSOR_WIDTH];
                int* tempLine = &tempBuf[y * CAMERA_SENSOR_WIDTH];
                int* tempLineS = &tempBuf[MIN(y + 1, CAMERA_SENSOR_HEIGHT - 1) * CAMERA_SENSOR_WIDTH];

                for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
                    int value = 0;
                    if(pBits & 0x1) value += tempLine[x];
                    if(pBits & 0x2) value += tempLineS[x];
                    if(mBits & 0x1) value -= tempLine[x];
                    if(mBits & 0x2) value -= tempLineS[x];
                    line[x] = CLAMP(-128, value, 127);
                }
            }

            break;
        case 0xE:
            for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
                int* line = &filtered[y * CAMERA_SENSOR_WIDTH];
                int* lineS = &filtered[MIN(y + 1, CAMERA_SENSOR_HEIGHT - 1) * CAMERA_SENSOR_WIDTH];
                int* lineN = &filtered[MAX(0, y - 1) * CAMERA_SENSOR_WIDTH];
                int* tempLine = &tempBuf[y * CAMERA_SENSOR_WIDTH];

                for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
                    tempLine[x] = CLAMP(-128, (int) (line[x] + ((4 * line[x] - line[MAX(0, x - 1)] - line[MIN(x + 1, CAMERA_SENSOR_WIDTH - 1)] - lineN[x] - lineS[x]) * edgeAlpha)), 127);
                }
            }

            memcpy(filtered, tempBuf, sizeof(filtered));
            break;
        default:
            if(this->gameboy->settings.printDebug != nullptr) {
                this->gameboy->settings.printDebug("Unsupported camera filter mode: 0x%X\n", filterMode);
            }

            break;
    }

    int processed[CAMERA_PROCESSED_WIDTH * CAMERA_PROCESSED_HEIGHT];

    for(int y = 0; y < CAMERA_SENSOR_HEIGHT; y++) {
        int* line = &filtered[(y + (CAMERA_SENSOR_EXTRA_LINES / 2)) * CAMERA_SENSOR_WIDTH];
        int* processedLine = &processed[y * CAMERA_SENSOR_WIDTH];

        for(int x = 0; x < CAMERA_SENSOR_WIDTH; x++) {
            int base = 6 + ((y & 3) * 4 + (x & 3)) * 3;
            u32 value = (u32) (line[x] + 128);
            if(value < this->cameraRegs[base + 0]) {
                processedLine[x] = 0x00;
            } else if(value < this->cameraRegs[base + 1]) {
                processedLine[x] = 0x40;
            } else if(value < this->cameraRegs[base + 2]) {
                processedLine[x] = 0x80;
            } else {
                processedLine[x] = 0xC0;
            }
        }
    }

    for(int y = 0; y < CAMERA_PROCESSED_HEIGHT; y++) {
        int* line = &processed[y * CAMERA_PROCESSED_WIDTH];

        for(int x = 0; x < CAMERA_PROCESSED_WIDTH; x++) {
            u8 color = (u8) (3 - (line[x] >> 6));
            u16 addr = (u16) (0x0100 + ((y >> 3) * 16 + (x >> 3)) * 16 + ((y & 7) * 2));

            if(color & 1) {
                this->writeSram(addr, (u8) (1 << (7 - (7 & x))));
            }

            if(color & 2) {
                this->writeSram((u16) (addr + 1), (u8) (1 << (7 - (7 & x))));
            }
        }
    }
}

#undef MIN
#undef MAX
#undef CLAMP

/* Increment y if x is greater than val */
#define OVERFLOW_VAL(x, val, y) ({ \
    while ((x) >= (val)) {         \
        (x) -= (val);              \
        (y)++;                     \
    }                              \
})

static u32 daysInMonth[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static u32 daysInLeapMonth[12] = {
        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

void Cartridge::latchClock() {
    time_t now;
    time(&now);

    time_t difference = (time_t) (now - this->rtcClock.last);
    struct tm* lt = gmtime((const time_t*) &difference);

    this->rtcClock.seconds += lt->tm_sec;
    OVERFLOW_VAL(this->rtcClock.seconds, 60, this->rtcClock.minutes);
    this->rtcClock.minutes += lt->tm_min;
    OVERFLOW_VAL(this->rtcClock.minutes, 60, this->rtcClock.hours);
    this->rtcClock.hours += lt->tm_hour;
    OVERFLOW_VAL(this->rtcClock.hours, 24, this->rtcClock.days);
    this->rtcClock.days += lt->tm_mday - 1;
    OVERFLOW_VAL(this->rtcClock.days, (this->rtcClock.years & 3) == 0 ? daysInLeapMonth[this->rtcClock.months % 12] : daysInMonth[this->rtcClock.months % 12], this->rtcClock.months);
    this->rtcClock.months += lt->tm_mon;
    OVERFLOW_VAL(this->rtcClock.months, 12, this->rtcClock.years);
    this->rtcClock.years += lt->tm_year - 70;

    this->rtcClock.last = (u64) now;
}
