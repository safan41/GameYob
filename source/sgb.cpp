#include <cstring>
#include <istream>
#include <ostream>

#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"

SGB::SGB(Gameboy* gameboy) {
    this->gameboy = gameboy;
}

void SGB::reset() {
    this->packetLength = 0;
    this->packetsTransferred = 0;
    this->packetBit = 0xFF;
    memset(this->packet, 0, sizeof(this->packet));

    this->command = 0;
    memset(&this->cmdData, 0, sizeof(this->cmdData));

    memset(this->controllers, 0xFF, sizeof(this->controllers));
    this->numControllers = 1;
    this->selectedController = 0;
    this->buttonsChecked = 0;

    memset(this->palettes, 0, sizeof(this->palettes));
    memset(this->attrFiles, 0, sizeof(this->attrFiles));

    this->hasBg = false;
    memset(this->bgTiles, 0, sizeof(this->bgTiles));
    memset(this->bgMap, 0, sizeof(this->bgMap));

    this->mask = 0;
    memset(this->paletteMap, 0, sizeof(this->paletteMap));
}

void SGB::update() {
    u8 interrupts = this->gameboy->mmu.readIO(IF);

    u8 joyp = this->gameboy->mmu.readIO(JOYP);
    if(!(joyp & 0x10)) {
        if((this->controllers[0] & 0xF0) != 0xF0) {
            interrupts |= INT_JOYPAD;
        }
    } else if(!(joyp & 0x20)) {
        if((this->controllers[0] & 0x0F) != 0x0F) {
            interrupts |= INT_JOYPAD;
        }
    }

    this->gameboy->mmu.writeIO(IF, interrupts);
}

void SGB::write(u16 addr, u8 val) {
    if(addr == JOYP) {
        if(this->gameboy->gbMode == MODE_SGB) {
            if((val & 0x30) == 0) {
                // Start packet transfer.

                this->packetBit = 0;
            } else if(this->packetBit != 0xFF) {
                // Process packet bit.

                u8 shift = this->packetBit & 0x7;
                u8 byte = (this->packetBit >> 3) & 0xF;
                if(shift == 0) {
                    // Just began a byte; initialize to zero.
                    this->packet[byte] = 0;
                }

                // 0x30 = No Data.
                if((val & 0x30) != 0x30) {
                    u8 oldJoyp = this->gameboy->mmu.readIO(JOYP);

                    if((oldJoyp & 0x30) != 0) { // A bit of speculation here. Fixes Castlevania.
                        this->packet[byte] |= (val & 0x10) == 0 ? 0 : 1 << shift;
                        this->packetBit++;

                        // 128 (0x80) bits to a packet.
                        if(this->packetBit == 0x80) {
                            if(this->packetsTransferred == 0) {
                                this->command = this->packet[0] >> 3;
                                this->packetLength = this->packet[0] & 7;
                            }

                            if(this->sgbCommands[this->command] != nullptr) {
                                (this->*sgbCommands[this->command])();
                            }

                            // End packet transfer.
                            this->packetBit = 0xFF;
                            this->packetsTransferred++;

                            if(this->packetsTransferred == this->packetLength) {
                                this->packetLength = 0;
                                this->packetsTransferred = 0;
                            }
                        }
                    } else {
                        // End packet transfer.
                        this->packetBit = 0xFF;
                    }
                }
            } else {
                // Keep track of bits for controller change commands.
                if((val & 0x30) == 0x10) {
                    this->buttonsChecked |= 1;
                } else if((val & 0x30) == 0x20) {
                    this->buttonsChecked |= 2;
                } else if((val & 0x30) == 0x30 && this->buttonsChecked == 3) {
                    this->selectedController++;
                    if(this->selectedController >= this->numControllers) {
                        this->selectedController = 0;
                    }

                    this->buttonsChecked = 0;
                }
            }
        }

        this->gameboy->mmu.writeIO(JOYP, val);

        // Update input bits.
        this->setController(this->selectedController, this->controllers[this->selectedController]);
    }
}

void SGB::setController(u8 controller, u8 val) {
    this->controllers[controller] = val;

    if(controller == this->selectedController) {
        u8 joyp = this->gameboy->mmu.readIO(JOYP);

        u8 inputBits = 0xF;
        if((joyp & 0x20) == 0) {
            inputBits = this->controllers[controller] & 0x0F;
        } else if((joyp & 0x10) == 0) {
            inputBits = (this->controllers[controller] & 0xF0) >> 4;
        } else if(this->gameboy->gbMode == MODE_SGB) {
            inputBits = (~this->selectedController) & 0x0F;
        }

        this->gameboy->mmu.writeIO(JOYP, 0xC0 | (joyp & 0x30) | inputBits);
    }
}

std::istream& operator>>(std::istream& is, SGB& sgb) {
    is.read((char*) &sgb.packetLength, sizeof(sgb.packetLength));
    is.read((char*) &sgb.packetsTransferred, sizeof(sgb.packetsTransferred));
    is.read((char*) &sgb.packetBit, sizeof(sgb.packetBit));
    is.read((char*) sgb.packet, sizeof(sgb.packet));
    is.read((char*) &sgb.command, sizeof(sgb.command));
    is.read((char*) &sgb.cmdData, sizeof(sgb.cmdData));

    is.read((char*) &sgb.controllers, sizeof(sgb.controllers));
    is.read((char*) &sgb.numControllers, sizeof(sgb.numControllers));
    is.read((char*) &sgb.selectedController, sizeof(sgb.selectedController));
    is.read((char*) &sgb.buttonsChecked, sizeof(sgb.buttonsChecked));

    is.read((char*) sgb.palettes, sizeof(sgb.palettes));
    is.read((char*) sgb.attrFiles, sizeof(sgb.attrFiles));

    is.read((char*) &sgb.hasBg, sizeof(sgb.hasBg));
    is.read((char*) sgb.bgTiles, sizeof(sgb.bgTiles));
    is.read((char*) sgb.bgMap, sizeof(sgb.bgMap));

    is.read((char*) &sgb.mask, sizeof(sgb.mask));
    is.read((char*) sgb.paletteMap, sizeof(sgb.paletteMap));

    sgb.refreshBg();

    return is;
}

std::ostream& operator<<(std::ostream& os, const SGB& sgb) {
    os.write((char*) &sgb.packetLength, sizeof(sgb.packetLength));
    os.write((char*) &sgb.packetsTransferred, sizeof(sgb.packetsTransferred));
    os.write((char*) &sgb.packetBit, sizeof(sgb.packetBit));
    os.write((char*) sgb.packet, sizeof(sgb.packet));
    os.write((char*) &sgb.command, sizeof(sgb.command));
    os.write((char*) &sgb.cmdData, sizeof(sgb.cmdData));

    os.write((char*) &sgb.controllers, sizeof(sgb.controllers));
    os.write((char*) &sgb.numControllers, sizeof(sgb.numControllers));
    os.write((char*) &sgb.selectedController, sizeof(sgb.selectedController));
    os.write((char*) &sgb.buttonsChecked, sizeof(sgb.buttonsChecked));

    os.write((char*) sgb.palettes, sizeof(sgb.palettes));
    os.write((char*) sgb.attrFiles, sizeof(sgb.attrFiles));

    os.write((char*) &sgb.hasBg, sizeof(sgb.hasBg));
    os.write((char*) sgb.bgTiles, sizeof(sgb.bgTiles));
    os.write((char*) sgb.bgMap, sizeof(sgb.bgMap));

    os.write((char*) &sgb.mask, sizeof(sgb.mask));
    os.write((char*) sgb.paletteMap, sizeof(sgb.paletteMap));

    return os;
}

void SGB::refreshBg() {
    if(this->hasBg && this->gameboy->settings.frameBuffer != nullptr) {
        for(u8 tileY = 0; tileY < 28; tileY++) {
            u16* lineMap = (u16*) &this->bgMap[tileY * 32 * sizeof(u16)];
            for(u8 tileX = 0; tileX < 32; tileX++) {
                u16 mapEntry = lineMap[tileX];
                u8* tile = &this->bgTiles[(mapEntry & 0xFF) * 0x20];
                u16* palette = (u16*) &this->bgMap[0x800 + ((mapEntry >> 10) & 3) * 0x20];
                bool flipX = (mapEntry & 0x4000) != 0;
                bool flipY = (mapEntry & 0x8000) != 0;

                for(u8 y = 0; y < 8; y++) {
                    for(u8 x = 0; x < 8; x++) {
                        u16 pixelX = tileX * 8 + x;
                        u16 pixelY = tileY * 8 + y;

                        u8 dataX = flipX ? 7 - x : x;
                        u8 dataY = flipY ? 7 - y : y;
                        u8 colorId = (u8) (((tile[dataY * 2] >> (7 - dataX)) & 1) | (((tile[dataY * 2 + 1] >> (7 - dataX)) & 1) << 1) | (((tile[0x10 + dataY * 2] >> (7 - dataX)) & 1) << 2) | (((tile[0x10 + dataY * 2 + 1] >> (7 - dataX)) & 1) << 3));

                        u32 color = 0;
                        if(colorId != 0) {
                            color = RGB555ToRGB8888(palette[colorId]);
                        } else if(pixelX < GB_SCREEN_X || pixelX >= GB_SCREEN_X + GB_SCREEN_WIDTH || pixelY < GB_SCREEN_Y || pixelY >= GB_SCREEN_Y + GB_SCREEN_HEIGHT) {
                            color = this->gameboy->ppu.getBgPalette()[0];
                        } else {
                            continue;
                        }

                        this->gameboy->settings.frameBuffer[pixelY * this->gameboy->settings.framePitch + pixelX] = color;
                    }
                }
            }
        }
    }
}

void SGB::loadAttrFile(u8 index) {
    if(index > 0x2C) {
        return;
    }

    u16 src = index * 0x5A;
    u16 dest = 0;
    for(int i = 0; i < 20 * 18 / 4; i++) {
        this->paletteMap[dest++] = (u8) ((this->attrFiles[src] >> 6) & 3);
        this->paletteMap[dest++] = (u8) ((this->attrFiles[src] >> 4) & 3);
        this->paletteMap[dest++] = (u8) ((this->attrFiles[src] >> 2) & 3);
        this->paletteMap[dest++] = (u8) ((this->attrFiles[src] >> 0) & 3);

        src++;
    }
}

// Begin commands

void SGB::palXX() {
    int s1, s2;
    switch(this->command) {
        case 0:
            s1 = 0;
            s2 = 1;
            break;
        case 1:
            s1 = 2;
            s2 = 3;
            break;
        case 2:
            s1 = 0;
            s2 = 3;
            break;
        case 3:
            s1 = 1;
            s2 = 2;
            break;
        default:
            return;
    }

    u16* paletteData = (u16*) &this->packet[1];

    u32 color0 = RGB555ToRGB8888(paletteData[0]);

    u32 palette[3];

    palette[0] = RGB555ToRGB8888(paletteData[1]);
    palette[1] = RGB555ToRGB8888(paletteData[2]);
    palette[2] = RGB555ToRGB8888(paletteData[3]);

    memcpy(&this->gameboy->ppu.getBgPalette()[s1 * 4 + 1], palette, sizeof(palette));
    memcpy(&this->gameboy->ppu.getSprPalette()[s1 * 4 + 1], palette, sizeof(palette));
    memcpy(&this->gameboy->ppu.getSprPalette()[(s1 + 4) * 4 + 1], palette, sizeof(palette));

    palette[0] = RGB555ToRGB8888(paletteData[4]);
    palette[1] = RGB555ToRGB8888(paletteData[5]);
    palette[2] = RGB555ToRGB8888(paletteData[6]);

    memcpy(&this->gameboy->ppu.getBgPalette()[s2 * 4 + 1], palette, sizeof(palette));
    memcpy(&this->gameboy->ppu.getSprPalette()[s2 * 4 + 1], palette, sizeof(palette));
    memcpy(&this->gameboy->ppu.getSprPalette()[(s2 + 4) * 4 + 1], palette, sizeof(palette));

    for(int i = 0; i < 4; i++) {
        this->gameboy->ppu.getBgPalette()[i * 4] = color0;
        this->gameboy->ppu.getSprPalette()[i * 4] = color0;
        this->gameboy->ppu.getSprPalette()[(i + 4) * 4] = color0;
    }
}

void SGB::attrBlock() {
    int pos;
    if(this->packetsTransferred == 0) {
        this->cmdData.attrBlock.dataBytes = 0;
        this->cmdData.numDataSets = packet[1];
        pos = 2;
    } else {
        pos = 0;
    }

    u8* const data = this->cmdData.attrBlock.data;
    while(pos < 16 && this->cmdData.numDataSets > 0) {
        for(; this->cmdData.attrBlock.dataBytes < 6 && pos < 16; this->cmdData.attrBlock.dataBytes++, pos++) {
            data[this->cmdData.attrBlock.dataBytes] = this->packet[pos];
        }

        if(this->cmdData.attrBlock.dataBytes == 6) {
            u8 pIn = (u8) (data[1] & 3);
            u8 pLine = (u8) ((data[1] >> 2) & 3);
            u8 pOut = (u8) ((data[1] >> 4) & 3);
            u8 x1 = data[2];
            u8 y1 = data[3];
            u8 x2 = data[4];
            u8 y2 = data[5];
            bool changeLine = (bool) (data[0] & 2);
            if(!changeLine) {
                if((data[0] & 7) == 1) {
                    changeLine = true;
                    pLine = pIn;
                } else if((data[0] & 7) == 4) {
                    changeLine = true;
                    pLine = pOut;
                }
            }

            if(data[0] & 1) { // Inside block
                for(int x = x1 + 1; x < x2; x++) {
                    for(int y = y1 + 1; y < y2; y++) {
                        this->paletteMap[y * 20 + x] = pIn;
                    }
                }
            }

            if(data[0] & 4) { // Outside block
                for(int x = 0; x < 20; x++) {
                    if(x < x1 || x > x2) {
                        for(int y = 0; y < 18; y++) {
                            if(y < y1 || y > y2) {
                                this->paletteMap[y * 20 + x] = pOut;
                            }
                        }
                    }
                }
            }

            if(changeLine) { // Line surrounding block
                for(int x = x1; x <= x2; x++) {
                    this->paletteMap[y1 * 20 + x] = pLine;
                    this->paletteMap[y2 * 20 + x] = pLine;
                }

                for(int y = y1; y <= y2; y++) {
                    this->paletteMap[y * 20 + x1] = pLine;
                    this->paletteMap[y * 20 + x2] = pLine;
                }
            }

            this->cmdData.attrBlock.dataBytes = 0;
            this->cmdData.numDataSets--;
        }
    }
}

void SGB::attrLin() {
    int index = 0;
    if(this->packetsTransferred == 0) {
        this->cmdData.numDataSets = packet[1];
        index = 2;
    }

    while(this->cmdData.numDataSets > 0 && index < 16) {
        u8 dat = packet[index++];
        this->cmdData.numDataSets--;

        u8 line = (u8) (dat & 0x1f);
        u8 pal = (u8) ((dat >> 5) & 3);

        if(dat & 0x80) { // Horizontal
            for(int i = 0; i < 20; i++) {
                this->paletteMap[line * 20 + i] = pal;
            }
        } else { // Vertical
            for(int i = 0; i < 18; i++) {
                this->paletteMap[i * 20 + line] = pal;
            }
        }
    }
}

void SGB::attrDiv() {
    u8 p0 = (u8) ((this->packet[1] >> 2) & 3);
    u8 p1 = (u8) ((this->packet[1] >> 4) & 3);
    u8 p2 = (u8) ((this->packet[1] >> 0) & 3);

    if(this->packet[1] & 0x40) {
        for(int y = 0; y < this->packet[2] && y < 18; y++) {
            for(int x = 0; x < 20; x++) {
                this->paletteMap[y * 20 + x] = p0;
            }
        }

        if(this->packet[2] < 18) {
            for(int x = 0; x < 20; x++) {
                this->paletteMap[this->packet[2] * 20 + x] = p1;
            }

            for(int y = this->packet[2] + 1; y < 18; y++) {
                for(int x = 0; x < 20; x++) {
                    this->paletteMap[y * 20 + x] = p2;
                }
            }
        }
    } else {
        for(int x = 0; x < this->packet[2] && x < 20; x++) {
            for(int y = 0; y < 18; y++) {
                this->paletteMap[y * 20 + x] = p0;
            }
        }

        if(this->packet[2] < 20) {
            for(int y = 0; y < 18; y++) {
                this->paletteMap[y * 20 + this->packet[2]] = p1;
            }

            for(int x = packet[2] + 1; x < 20; x++) {
                for(int y = 0; y < 18; y++) {
                    this->paletteMap[y * 20 + x] = p2;
                }
            }
        }
    }
}

void SGB::attrChr() {
    u8 &x = this->cmdData.attrChr.x;
    u8 &y = this->cmdData.attrChr.y;

    int index = 0;
    if(this->packetsTransferred == 0) {
        this->cmdData.numDataSets = this->packet[3] | (this->packet[4] << 8);
        this->cmdData.attrChr.writeStyle = (u8) (this->packet[5] & 1);
        x = (u8) (this->packet[1] >= 20 ? 19 : this->packet[1]);
        y = (u8) (this->packet[2] >= 18 ? 17 : this->packet[2]);

        index = 6 * 4;
    }

    while(this->cmdData.numDataSets != 0 && index < 16 * 4) {
        this->paletteMap[x + y * 20] = (u8) ((packet[index / 4] >> (6 - (index & 3) * 2)) & 3);
        if(this->cmdData.attrChr.writeStyle == 0) {
            x++;
            if(x == 20) {
                x = 0;
                y++;
                if(y == 18) {
                    y = 0;
                }
            }
        } else {
            y++;
            if(y == 18) {
                y = 0;
                x++;
                if(x == 20) {
                    x = 0;
                }
            }
        }

        index++;
        this->cmdData.numDataSets--;
    }
}

void SGB::sound() {
    // Unimplemented
}

void SGB::souTrn() {
    // Unimplemented
}

void SGB::palSet() {
    int color0PaletteId = (this->packet[1] | (this->packet[2] << 8)) & 0x1ff;
    u32 color0 = RGB555ToRGB8888(this->palettes[color0PaletteId * 4]);

    for(int i = 0; i < 4; i++) {
        int paletteId = (this->packet[i * 2 + 1] | (this->packet[i * 2 + 2] << 8)) & 0x1ff;

        u32 palette[4];
        palette[0] = color0;
        palette[1] = RGB555ToRGB8888(this->palettes[paletteId * 4 + 1]);
        palette[2] = RGB555ToRGB8888(this->palettes[paletteId * 4 + 2]);
        palette[3] = RGB555ToRGB8888(this->palettes[paletteId * 4 + 3]);

        memcpy(&this->gameboy->ppu.getBgPalette()[i * 4], palette, sizeof(palette));
        memcpy(&this->gameboy->ppu.getSprPalette()[i * 4], palette, sizeof(palette));
        memcpy(&this->gameboy->ppu.getSprPalette()[(i + 4) * 4], palette, sizeof(palette));
    }

    if(this->packet[9] & 0x80) {
        this->loadAttrFile(this->packet[9] & 0x3f);
    }

    if(this->packet[9] & 0x40) {
        this->mask = 0;
    }
}

void SGB::palTrn() {
    this->gameboy->ppu.transferTiles((u8*) this->palettes);
}

void SGB::atrcEn() {
    // Unimplemented
}

void SGB::testEn() {
    // Unimplemented
}

void SGB::iconEn() {
    // Unimplemented
}

void SGB::dataSnd() {
    // Unimplemented
}

void SGB::dataTrn() {
    // Unimplemented
}

void SGB::mltReq() {
    this->numControllers = (u8) ((this->packet[1] & 3) + 1);
    this->selectedController = (u8) (this->numControllers > 1 ? 1 : 0);

    // Update input bits.
    this->setController(this->selectedController, this->controllers[this->selectedController]);
}

void SGB::jump() {
    // Unimplemented
}

void SGB::chrTrn() {
    this->gameboy->ppu.transferTiles(&this->bgTiles[(this->packet[1] & 1) * 0x1000]);

    this->hasBg = true;
    this->refreshBg();
}

void SGB::pctTrn() {
    this->gameboy->ppu.transferTiles(this->bgMap);

    this->hasBg = true;
    this->refreshBg();
}

void SGB::attrTrn() {
    this->gameboy->ppu.transferTiles(this->attrFiles);
}

void SGB::attrSet() {
    this->loadAttrFile(this->packet[1] & 0x3f);
    if(this->packet[1] & 0x40) {
        this->mask = 0;
    }
}

void SGB::maskEn() {
    this->mask = (u8) (this->packet[1] & 3);
}

void SGB::objTrn() {
    // Unimplemented
}

// End commands
