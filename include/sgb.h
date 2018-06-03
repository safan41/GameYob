#pragma once

#include "types.h"

class Gameboy;

class SGB {
public:
    SGB(Gameboy* gameboy);

    void reset();
    void update();

    void write(u16 addr, u8 val);

    void setController(u8 controller, u8 val);

    friend std::istream& operator>>(std::istream& is, SGB& sgb);
    friend std::ostream& operator<<(std::ostream& os, const SGB& sgb);

    inline u8 getGfxMask() {
        return this->mask;
    }

    u8* getPaletteMap() {
        return this->paletteMap;
    }
private:
    void refreshBg();

    void loadAttrFile(u8 index);

    // Begin commands
    void palXX();
    void attrBlock();
    void attrLin();
    void attrDiv();
    void attrChr();
    void sound();
    void souTrn();
    void palSet();
    void palTrn();
    void atrcEn();
    void testEn();
    void iconEn();
    void dataSnd();
    void dataTrn();
    void mltReq();
    void jump();
    void chrTrn();
    void pctTrn();
    void attrTrn();
    void attrSet();
    void maskEn();
    void objTrn();
    // End commands

    void (SGB::*sgbCommands[32])() = {
            &SGB::palXX, &SGB::palXX, &SGB::palXX, &SGB::palXX, &SGB::attrBlock,
            &SGB::attrLin, &SGB::attrDiv, &SGB::attrChr,
            &SGB::sound, &SGB::souTrn, &SGB::palSet, &SGB::palTrn, &SGB::atrcEn,
            &SGB::testEn, &SGB::iconEn, &SGB::dataSnd,
            &SGB::dataTrn, &SGB::mltReq, &SGB::jump, &SGB::chrTrn, &SGB::pctTrn,
            &SGB::attrTrn, &SGB::attrSet, &SGB::maskEn,
            &SGB::objTrn, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    };

    Gameboy* gameboy;

    u8 packetLength; // Number of packets to be transferred this command
    u8 packetsTransferred; // Number of packets which have been transferred so far
    u8 packetBit; // Next bit # to be sent in the packet. 0xFF if no packet is being transferred.
    u8 packet[16];

    u8 command;
    // Data for various different SGB commands
    struct SgbCmdData {
        u8 numDataSets;

        union {
            struct {
                u8 data[6];
                u8 dataBytes;
            } attrBlock;

            struct {
                u8 writeStyle;
                u8 x;
                u8 y;
            } attrChr;
        };
    } cmdData;

    u8 controllers[4];
    u8 numControllers;
    u8 selectedController; // Which controller is being observed
    u8 buttonsChecked;

    u16 palettes[0x1000];
    u8 attrFiles[0x1000];

    bool hasBg;
    u8 bgTiles[0x2000];
    u8 bgMap[0x1000];

    u8 mask;
    u8 paletteMap[20 * 18];
};