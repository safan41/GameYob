#include <cstring>
#include <istream>
#include <ostream>

#include "cartridge.h"
#include "cpu.h"
#include "gameboy.h"
#include "printer.h"

#define PRINTER_STATUS_READY        0x08
#define PRINTER_STATUS_REQUESTED    0x04
#define PRINTER_STATUS_PRINTING     0x02
#define PRINTER_STATUS_CHECKSUM     0x01

Printer::Printer(Gameboy* gb) {
    this->gameboy = gb;
}

void Printer::reset() {
    memset(this->gfx, 0, sizeof(this->gfx));
    this->gfxIndex = 0;

    this->packetByte = 0;
    this->status = 0;
    this->cmd = 0;
    this->cmdLength = 0;

    this->packetCompressed = false;
    this->compressionByte = 0;
    this->compressionLen = 0;

    this->expectedChecksum = 0;
    this->checksum = 0;

    this->margins = -1;
    this->lastMargins = -1;
    this->cmd2Index = 0;
    this->palette = 0;
    this->exposure = 0;

    this->nextUpdateCycle = 0;
}

void Printer::update() {
    if(this->nextUpdateCycle != 0) {
        if(this->gameboy->cpu.getCycle() >= this->nextUpdateCycle) {
            this->nextUpdateCycle = 0;

            if(this->status & PRINTER_STATUS_PRINTING) {
                this->status &= ~PRINTER_STATUS_PRINTING;
            } else {
                this->status |= PRINTER_STATUS_REQUESTED;
                this->status |= PRINTER_STATUS_PRINTING;
                this->status &= ~PRINTER_STATUS_READY;

                // if "appending" is true, this image will be slapped onto the old one.
                // Some games have a tendency to print an image in multiple goes.
                bool appending = false;
                if(this->lastMargins != -1 && (this->lastMargins & 0x0F) == 0 && (this->margins & 0xF0) == 0) {
                    appending = true;
                }

                this->gameboy->settings.printImage(appending, this->gfx, this->gfxIndex, this->palette);
                this->gfxIndex = 0;

                // PRINTER_STATUS_PRINTING will be unset after this many frames
                int height = this->gfxIndex / PRINTER_WIDTH * 4;
                this->nextUpdateCycle = this->gameboy->cpu.getCycle() + ((height != 0 ? height : 1) * CYCLES_PER_FRAME);
                this->gameboy->cpu.setEventCycle(this->nextUpdateCycle);
            }
        } else {
            this->gameboy->cpu.setEventCycle(this->nextUpdateCycle);
        }
    }
}

u8 Printer::link(u8 val) {
    // "Byte" 6 is actually a number of bytes. The counter stays at 6 until the
    // required number of bytes have been read.
    if(this->packetByte == 6 && this->cmdLength == 0) {
        this->packetByte++;
    }

    // Checksum: don't count the magic bytes or checksum bytes
    if(this->packetByte != 0 && this->packetByte != 1 && this->packetByte != 7 && this->packetByte != 8) {
        this->checksum += val;
    }

    switch(this->packetByte++) {
        case 0: // Magic byte
            if(val != 0x88) {
                this->packetByte = 0;
                this->checksum = 0;
                this->cmd2Index = 0;
            }

            return 0;
        case 1: // Magic byte
            if(val != 0x33) {
                this->packetByte = 0;
                this->checksum = 0;
                this->cmd2Index = 0;
            }

            return 0;
        case 2: // Command
            this->cmd = val;
            return 0;
        case 3: // Compression flag
            this->packetCompressed = val;
            if(this->packetCompressed) {
                this->compressionLen = 0;
            }

            return 0;
        case 4: // Length (LSB)
            this->cmdLength = (u16) ((this->cmdLength & 0xFF00) | val);
            return 0;
        case 5: // Length (MSB)
            this->cmdLength = (u16) ((this->cmdLength & 0xFF) | (val << 8));
            return 0;
        case 6: // variable-length data
            if(!this->packetCompressed) {
                this->processBodyData(val);
            } else {
                // Handle RLE compression
                if(this->compressionLen == 0) {
                    this->compressionByte = val;
                    this->compressionLen = (u8) ((val & 0x7f) + 1);
                    if(this->compressionByte & 0x80) {
                        this->compressionLen++;
                    }
                } else {
                    if(this->compressionByte & 0x80) {
                        while(this->compressionLen != 0) {
                            this->processBodyData(val);
                            this->compressionLen--;
                        }
                    } else {
                        this->processBodyData(val);
                        this->compressionLen--;
                    }
                }
            }

            this->cmdLength--;
            this->packetByte--; // Remain on 6 until we've received everything.
            return 0;
        case 7: // Checksum (LSB)
            this->expectedChecksum = (u16) ((this->expectedChecksum & 0xFF00) | val);
            return 0;
        case 8: // Checksum (MSB)
            this->expectedChecksum = (u16) ((this->expectedChecksum & 0xFF) | (val << 8));
            return 0;
        case 9: // Alive indicator
            return 0x81;
        case 10: { // Status
            if(this->checksum != this->expectedChecksum) {
                this->status |= PRINTER_STATUS_CHECKSUM;
                if(this->gameboy->settings.printDebug != nullptr) {
                    this->gameboy->settings.printDebug("Printer Error: Checksum %.4x, Expected %.4x\n", this->checksum, this->expectedChecksum);
                }
            } else {
                this->status &= ~PRINTER_STATUS_CHECKSUM;
            }

            switch(this->cmd) {
                case 1: // Initialize
                    this->nextUpdateCycle = 0;
                    this->status = 0;
                    this->gfxIndex = 0;
                    memset(this->gfx, 0, sizeof(this->gfx));
                    break;
                case 2: // Start printing (after a short delay)
                    this->nextUpdateCycle = this->gameboy->cpu.getCycle() + CYCLES_PER_FRAME;
                    this->gameboy->cpu.setEventCycle(this->nextUpdateCycle);
                    break;
                case 4: // Fill buffer
                    // Data has been read, nothing more to do
                    break;
                default:
                    break;
            }

            u8 reply = this->status;

            // The received value apparently shouldn't contain this until next packet.
            if(this->gfxIndex >= 0x280) {
                this->status |= PRINTER_STATUS_READY;
            }

            this->packetByte = 0;
            this->checksum = 0;
            this->cmd2Index = 0;
            return reply;
        }
        default:
            break;
    }

    return 0;
}

std::istream& operator>>(std::istream& is, Printer& printer) {
    is.read((char*) printer.gfx, sizeof(printer.gfx));
    is.read((char*) &printer.gfxIndex, sizeof(printer.gfxIndex));
    is.read((char*) &printer.packetByte, sizeof(printer.packetByte));
    is.read((char*) &printer.status, sizeof(printer.status));
    is.read((char*) &printer.cmd, sizeof(printer.cmd));
    is.read((char*) &printer.cmdLength, sizeof(printer.cmdLength));
    is.read((char*) &printer.packetCompressed, sizeof(printer.packetCompressed));
    is.read((char*) &printer.compressionByte, sizeof(printer.compressionByte));
    is.read((char*) &printer.compressionLen, sizeof(printer.compressionLen));
    is.read((char*) &printer.expectedChecksum, sizeof(printer.expectedChecksum));
    is.read((char*) &printer.checksum, sizeof(printer.checksum));
    is.read((char*) &printer.cmd2Index, sizeof(printer.cmd2Index));
    is.read((char*) &printer.margins, sizeof(printer.margins));
    is.read((char*) &printer.lastMargins, sizeof(printer.lastMargins));
    is.read((char*) &printer.palette, sizeof(printer.palette));
    is.read((char*) &printer.exposure, sizeof(printer.exposure));
    is.read((char*) &printer.nextUpdateCycle, sizeof(printer.nextUpdateCycle));

    return is;
}

std::ostream& operator<<(std::ostream& os, const Printer& printer) {
    os.write((char*) printer.gfx, sizeof(printer.gfx));
    os.write((char*) &printer.gfxIndex, sizeof(printer.gfxIndex));
    os.write((char*) &printer.packetByte, sizeof(printer.packetByte));
    os.write((char*) &printer.status, sizeof(printer.status));
    os.write((char*) &printer.cmd, sizeof(printer.cmd));
    os.write((char*) &printer.cmdLength, sizeof(printer.cmdLength));
    os.write((char*) &printer.packetCompressed, sizeof(printer.packetCompressed));
    os.write((char*) &printer.compressionByte, sizeof(printer.compressionByte));
    os.write((char*) &printer.compressionLen, sizeof(printer.compressionLen));
    os.write((char*) &printer.expectedChecksum, sizeof(printer.expectedChecksum));
    os.write((char*) &printer.checksum, sizeof(printer.checksum));
    os.write((char*) &printer.cmd2Index, sizeof(printer.cmd2Index));
    os.write((char*) &printer.margins, sizeof(printer.margins));
    os.write((char*) &printer.lastMargins, sizeof(printer.lastMargins));
    os.write((char*) &printer.palette, sizeof(printer.palette));
    os.write((char*) &printer.exposure, sizeof(printer.exposure));
    os.write((char*) &printer.nextUpdateCycle, sizeof(printer.nextUpdateCycle));

    return os;
}

void Printer::processBodyData(u8 dat) {
    switch(this->cmd) {
        case 0x2: // Print
            switch(this->cmd2Index) {
                case 0: // Unknown (0x01)
                    break;
                case 1: // Margins
                    this->lastMargins = this->margins;
                    this->margins = dat;
                    break;
                case 2: // Palette
                    this->palette = dat;
                    break;
                case 3: // Exposure / brightness
                    this->exposure = dat;
                    break;
                default:
                    break;
            }

            this->cmd2Index++;
            break;
        case 0x4: // Fill buffer
            if(this->gfxIndex < PRINTER_WIDTH * PRINTER_HEIGHT / 4) {
                this->gfx[this->gfxIndex++] = dat;
            }

            break;
        default:
            break;
    }
}

