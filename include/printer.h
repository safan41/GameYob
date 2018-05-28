#pragma once

#include "types.h"

#define PRINTER_WIDTH 160
#define PRINTER_HEIGHT 208 // The actual value is 200, but 16 divides 208.

class Gameboy;

class Printer {
public:
    Printer(Gameboy* gb);

    void reset();
    void update();

    u8 link(u8 val);

    friend std::istream& operator>>(std::istream& is, Printer& printer);
    friend std::ostream& operator<<(std::ostream& os, const Printer& printer);
private:
    void processBodyData(u8 dat);

    Gameboy* gameboy;

    u8 gfx[PRINTER_WIDTH * PRINTER_HEIGHT / 4];
    u16 gfxIndex;

    u8 packetByte;
    u8 status;
    u8 cmd;
    u16 cmdLength;

    bool packetCompressed;
    u8 compressionByte;
    u8 compressionLen;

    u16 expectedChecksum;
    u16 checksum;

    u8 cmd2Index;
    s16 margins; // -1 = never set
    s16 lastMargins; // -1 = never set
    u8 palette;
    u8 exposure; // Ignored

    u64 nextUpdateCycle;
};
