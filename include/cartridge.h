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

class Cartridge;

typedef void (Cartridge::*mbcWrite)(u16, u8);
typedef u8 (Cartridge::*mbcRead)(u16);
typedef void (Cartridge::*mbcUpdate)();

class Cartridge {
public:
    Cartridge(std::istream& romData, u32 romSize);
    ~Cartridge();

    void reset(Gameboy* gameboy);

    void load(std::istream& data);
    void save(std::ostream& data);

    friend std::istream& operator>>(std::istream& is, Cartridge& cart);
    friend std::ostream& operator<<(std::ostream& os, const Cartridge& cart);

    inline mbcRead getReadFunc() {
        return this->readFunc;
    }

    inline mbcWrite getWriteFunc() {
        return this->writeFunc;
    }

    inline mbcWrite getWriteSramFunc() {
        return this->writeSramFunc;
    }

    inline mbcUpdate getUpdateFunc() {
        return this->updateFunc;
    }

    inline const std::string getRomTitle() {
        return this->romTitle;
    }

    inline bool isCgbSupported() {
        return this->rom[0x0143] == 0x80 || this->isCgbRequired();
    }

    inline bool isCgbRequired() {
        return this->rom[0x0143] == 0xC0;
    }

    inline bool isSgbEnhanced() {
        return this->rom[0x146] == 0x03 && this->rom[0x014B] == 0x33;
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
    u8 m7r(u16 addr);
    u8 h3r(u16 addr);
    u8 t5r(u16 addr);
    u8 camr(u16 addr);

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

    void m2ws(u16 addr, u8 val);
    void m3ws(u16 addr, u8 val);
    void m7ws(u16 addr, u8 val);
    void mmm01ws(u16 addr, u8 val);
    void h3ws(u16 addr, u8 val);
    void t5ws(u16 addr, u8 val);
    void camws(u16 addr, u8 val);

    void camu();

    void camTakePicture();

    void latchClock();

    const mbcRead mbcReads[MBC_MAX] = {
            nullptr,
            nullptr,
            nullptr,
            &Cartridge::m3r,
            nullptr,
            nullptr,
            &Cartridge::m7r,
            nullptr,
            nullptr,
            &Cartridge::h3r,
            &Cartridge::camr,
            &Cartridge::t5r
    };

    const mbcWrite mbcWrites[MBC_MAX] = {
            nullptr,
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
            nullptr
    };

    const mbcWrite mbcSramWrites[MBC_MAX] = {
            nullptr,
            nullptr,
            &Cartridge::m2ws,
            &Cartridge::m3ws,
            nullptr,
            nullptr,
            &Cartridge::m7ws,
            &Cartridge::mmm01ws,
            nullptr,
            &Cartridge::h3ws,
            &Cartridge::camws,
            &Cartridge::t5ws
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
    bool multicart;
    bool rockmanMapper;
    bool rumble;

    mbcRead readFunc;
    mbcWrite writeFunc;
    mbcWrite writeSramFunc;
    mbcUpdate updateFunc;

    u8* sram;

    // General
    u16 romBank0;
    u16 romBank1;
    u8 sramBank;
    bool sramEnabled;

#pragma pack(push, 1)

    struct {
        u32 seconds;
        u32 minutes;
        u32 hours;
        u32 days;
        u32 months;
        u32 years;

        u64 last;
    } rtcClock;

    // MBC1
    struct {
        bool mode;
    } mbc1;

    // MBC3
    struct {
        u8 ctrl;
    } mbc3;

    // MBC6
    struct {
        u8 romBank1A;
        u8 romBank1B;
        u8 sramBankA;
        u8 sramBankB;
    } mbc6;

    // MBC7
    struct {
        u8 access;
        u8 latch;
        u8 state;

        u8 status;
        u16 cmd;
        u16 sr;
        u8 srBits;
        u16 address;
        bool writable;

        u16 tiltX;
        u16 tiltY;
    } mbc7;

    // HuC1
    struct {
        bool ramMode;
    } huc1;

    // HuC3
    struct {
        u8 mode;
        u8 value;
        u8 shift;
    } huc3;

    // MMM01
    struct {
        bool bankSelected;
        u8 romBaseBank;
    } mmm01;

    // CAMERA
    struct {
        bool io;
        u64 readyCycle;
        u8 regs[0x36];
    } camera;

    // TAMA5
    struct {
        bool enabled;

        u8 reg;
        u8 registers[0x10];
    } tama5;

#pragma pack(pop)
};