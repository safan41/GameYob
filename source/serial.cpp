#include <istream>
#include <ostream>

#include "cartridge.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "printer.h"
#include "serial.h"

Serial::Serial(Gameboy* gameboy) : printer(gameboy) {
    this->gameboy = gameboy;
}

void Serial::reset() {
    this->nextSerialInternalCycle = 0;
    this->nextSerialExternalCycle = 0;

    this->printer.reset();
}

void Serial::update() {
    bool printerEnabled = this->gameboy->settings.getOption(GB_OPT_PRINTER_ENABLED);

    if(printerEnabled) {
        this->printer.update();
    }

    // For external clock
    if(this->nextSerialExternalCycle > 0) {
        if(this->gameboy->cpu.getCycle() >= this->nextSerialExternalCycle) {
            u8 sc = this->gameboy->mmu.readIO(SC);

            u8 received = 0xFF;
            if((sc & 0x81) == 0x80) {
                // TODO: Receive/Send
            }

            this->gameboy->mmu.writeIO(SB, received);

            if(sc & 0x80) {
                this->gameboy->mmu.writeIO(SC, (u8) (sc & ~0x80));

                this->gameboy->mmu.writeIO(IF, (u8) (this->gameboy->mmu.readIO(IF) | INT_SERIAL));
            }

            this->nextSerialExternalCycle = 0;
        } else {
            this->gameboy->cpu.setEventCycle(this->nextSerialExternalCycle);
        }
    }

    // For internal clock
    if(this->nextSerialInternalCycle > 0) {
        if(this->gameboy->cpu.getCycle() >= this->nextSerialInternalCycle) {
            u8 sc = this->gameboy->mmu.readIO(SC);
            u8 sb = this->gameboy->mmu.readIO(SB);

            u8 received = 0xFF;
            if(printerEnabled) {
                received = this->printer.link(sb);
            } else {
                // TODO: Send/Receive
            }

            this->gameboy->mmu.writeIO(SB, received);
            this->gameboy->mmu.writeIO(SC, (u8) (sc & ~0x80));

            this->gameboy->mmu.writeIO(IF, (u8) (this->gameboy->mmu.readIO(IF) | INT_SERIAL));

            this->nextSerialInternalCycle = 0;
        } else {
            this->gameboy->cpu.setEventCycle(this->nextSerialInternalCycle);
        }
    }
}

void Serial::write(u16 addr, u8 val) {
    if(addr == SC) {
        this->gameboy->mmu.writeIO(SC, val);

        if((val & 0x81) == 0x81) { // Internal clock
            if(this->nextSerialInternalCycle == 0) {
                if(this->gameboy->gbMode == MODE_CGB && (val & 0x02)) {
                    this->nextSerialInternalCycle = this->gameboy->cpu.getCycle() + CYCLES_PER_SECOND / (1024 * 32);
                } else {
                    this->nextSerialInternalCycle = this->gameboy->cpu.getCycle() + CYCLES_PER_SECOND / 1024;
                }

                this->gameboy->cpu.setEventCycle(this->nextSerialInternalCycle);
            }
        } else {
            this->nextSerialInternalCycle = 0;
        }
    }
}

std::istream& operator>>(std::istream& is, Serial& serial) {
    is.read((char*) &serial.nextSerialInternalCycle, sizeof(serial.nextSerialInternalCycle));
    is.read((char*) &serial.nextSerialExternalCycle, sizeof(serial.nextSerialExternalCycle));

    is >> serial.printer;

    return is;
}

std::ostream& operator<<(std::ostream& os, const Serial& serial) {
    os.write((char*) &serial.nextSerialInternalCycle, sizeof(serial.nextSerialInternalCycle));
    os.write((char*) &serial.nextSerialExternalCycle, sizeof(serial.nextSerialExternalCycle));

    os << serial.printer;

    return os;
}