#pragma once

#include "types.h"

class Gameboy;

#define GB_FRAME_WIDTH 256
#define GB_FRAME_HEIGHT 224

#define GB_SCREEN_X 48
#define GB_SCREEN_Y 40
#define GB_SCREEN_WIDTH 160
#define GB_SCREEN_HEIGHT 144

class PPU {
public:
    PPU(Gameboy* gb);

    void reset();
    void update();

    void write(u16 addr, u8 val);

    void setHalfSpeed(bool halfSpeed);

    void transferTiles(u8* dest);

    friend std::istream& operator>>(std::istream& is, PPU& ppu);
    friend std::ostream& operator<<(std::ostream& os, const PPU& ppu);

    inline u8 readOam(u16 addr) {
        return this->oam[addr & 0xFF];
    }

    inline void writeOam(u16 addr, u8 val) {
        this->oam[addr & 0xFF] = val;
    }

    inline u32* getBgPalette() {
        return this->bgPalette;
    }

    inline u32* getSprPalette() {
        return this->sprPalette;
    }
private:
    typedef struct {
        u8 color[8];
        u8 depth[8];
        u8 palette;
    } TileLine;

    typedef struct {
        u8 color[8];
        u8 depth[8];
        u8 x;
        u8 palette;
        u8 obp;
    } SpriteLine;

    void mapBanks();

    void checkLYC();

    void updateLineTile(u8 map, u8 x, u8 y);
    void updateLineSprites();

    void updateScanline();
    void drawPixel(u8 x, u8 y);
    void drawScanline(u8 scanline);

    Gameboy* gameboy;

    u64 lastScanlineCycle;
    u64 lastPhaseCycle;
    bool halfSpeed;

    u8 scanlineX;

    TileLine currTileLines[2];
    SpriteLine currSpriteLines[10];
    u8 currSprites;

    u8 vram[2][0x2000];
    u8 oam[0xA0];
    u8 rawBgPalette[0x40];
    u8 rawSprPalette[0x40];

    u32 bgPalette[0x20];
    u32 sprPalette[0x20];

    u8 expandedBgp[4];
    u8 expandedObp[8];
};
