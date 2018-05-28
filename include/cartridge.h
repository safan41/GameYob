#pragma once

#include "types.h"

class Gameboy;

#define ROM_BANK_SIZE 0x4000
#define ROM_BANK_MASK 0x3FFF

typedef enum : u8 {
    MBC0 = 0,
    MBC1,
    MBC2,
    MBC3,
    MBC5,
    MBC6,
    MBC7,
    MMM01,
    HUC1,
    HUC3,
    CAMERA,
    TAMA5,

    MBC_MAX
} MBCType;

class Cartridge {
public:
    Cartridge(std::istream& romData, u32 romSize, std::istream& saveData);
    ~Cartridge();

    void reset(Gameboy* gameboy);
    void update();

    void save(std::ostream& data);

    friend std::istream& operator>>(std::istream& is, Cartridge& cart);
    friend std::ostream& operator<<(std::ostream& os, const Cartridge& cart);

    inline const std::string getRomTitle() {
        return this->romTitle;
    }

    inline bool isCgbSupported() {
        return this->rom[0x0143] == 0x80 || this->isCgbRequired();
    }

    inline bool isCgbRequired() {
        return this->rom[0x0143] == 0xC0;
    }

    inline u8 getRawRomSize() {
        return this->rom[0x0148];
    }

    inline u16 getRomBanks() {
        return this->totalRomBanks;
    }

    inline u8 getRawRamSize() {
        return this->rom[0x0149];
    }

    inline u8 getRamBanks() {
        return this->totalRamBanks;
    }

    inline bool isSgbEnhanced() {
        return this->rom[0x146] == 0x03 && this->rom[0x014B] == 0x33;
    }

    inline u8 getRawMBC() {
        return this->rom[0x0147];
    }

    inline MBCType getMBCType() {
        return this->mbcType;
    }

    inline u8* getRomBank(u16 bank) {
        if(bank >= this->totalRomBanks) {
            return nullptr;
        }

        return &this->rom[bank * ROM_BANK_SIZE];
    }
private:
    u8 readSram(u16 addr);
    void writeSram(u16 addr, u8 val);

    void mapRomBank0();
    void mapRomBank1();
    void mapSramBank();
    void mapBanks();

    u8 m3r(u16 addr);
    u8 m6r(u16 addr);
    u8 m7r(u16 addr);
    u8 h3r(u16 addr);
    u8 camr(u16 addr);

    void m0w(u16 addr, u8 val);
    void m1w(u16 addr, u8 val);
    void m2w(u16 addr, u8 val);
    void m3w(u16 addr, u8 val);
    void m5w(u16 addr, u8 val);
    void m6w(u16 addr, u8 val);
    void m7w(u16 addr, u8 val);
    void mmm01w(u16 addr, u8 val);
    void h1w(u16 addr, u8 val);
    void h3w(u16 addr, u8 val);
    void camw(u16 addr, u8 val);
    void t5w(u16 addr, u8 val);

    void camu();

    void camTakePicture();

    void latchClock();

    typedef void (Cartridge::*mbcWrite)(u16, u8);
    typedef u8 (Cartridge::*mbcRead)(u16);
    typedef void (Cartridge::*mbcUpdate)();

    const mbcRead mbcReads[MBC_MAX] = {
            nullptr,
            nullptr,
            nullptr,
            &Cartridge::m3r,
            nullptr,
            &Cartridge::m6r,
            &Cartridge::m7r,
            nullptr,
            nullptr,
            &Cartridge::h3r,
            &Cartridge::camr,
            nullptr
    };

    const mbcWrite mbcWrites[MBC_MAX] = {
            &Cartridge::m0w,
            &Cartridge::m1w,
            &Cartridge::m2w,
            &Cartridge::m3w,
            &Cartridge::m5w,
            &Cartridge::m6w,
            &Cartridge::m7w,
            &Cartridge::mmm01w,
            &Cartridge::h1w,
            &Cartridge::h3w,
            &Cartridge::camw,
            &Cartridge::t5w
    };

    const mbcUpdate mbcUpdates[MBC_MAX] = {
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &Cartridge::camu,
            nullptr
    };

    Gameboy* gameboy;

    u8* rom;

    std::string romTitle;
    u16 totalRomBanks;
    u8 totalRamBanks;
    MBCType mbcType;
    bool rockmanMapper;
    bool rumble;

    mbcRead readFunc;
    mbcWrite writeFunc;
    mbcUpdate updateFunc;

    u8* sram;

    struct {
        u32 seconds;
        u32 minutes;
        u32 hours;
        u32 days;
        u32 months;
        u32 years;

        u64 last;
    } rtcClock;

    // General
    u16 romBank0;
    u16 romBank1;
    u8 sramBank;
    bool sramEnabled;

    // MBC1
    bool mbc1RamMode;

    // MBC3
    u8 mbc3Ctrl;

    // MBC6
    u8 mbc6RomBank1A;
    u8 mbc6RomBank1B;
    u8 mbc6SramBankA;
    u8 mbc6SramBankB;

    // MBC7
    bool mbc7WriteEnable;
    u8 mbc7Cs;
    u8 mbc7Sk;
    u8 mbc7OpCode;
    u8 mbc7Addr;
    u8 mbc7Count;
    u8 mbc7State;
    u16 mbc7Buffer;
    u8 mbc7RA;

    // HuC1
    bool huc1RamMode;

    // HuC3
    u8 huc3Mode;
    u8 huc3Value;
    u8 huc3Shift;

    // MMM01
    bool mmm01BankSelected;
    u8 mmm01RomBaseBank;

    // CAMERA
    bool cameraIO;
    u64 cameraReadyCycle;
    u8 cameraRegs[0x36];

    // TAMA5
    u8 tama5CommandNumber;
    u8 tama5RamByteSelect;
    u8 tama5Commands[0x10];
    u8 tama5RAM[0x100];
};