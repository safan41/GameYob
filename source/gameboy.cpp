#include <istream>
#include <ostream>

#include "apu.h"
#include "cartridge.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "printer.h"
#include "ppu.h"
#include "sgb.h"
#include "serial.h"
#include "timer.h"

static const u8 STATE_VERSION = 12;

Gameboy::Gameboy() : mmu(this), cpu(this), ppu(this), apu(this), sgb(this), timer(this), serial(this) {
    this->cartridge = nullptr;
}

Gameboy::~Gameboy() {
    this->cartridge = nullptr;
}

void Gameboy::powerOn() {
    if(this->poweredOn) {
        return;
    }

    if(this->cartridge != nullptr) {
        GBCMode gbcMode = (GBCMode) this->settings.getOption(GB_OPT_GBC_MODE);
        SGBMode sgbMode = (SGBMode) this->settings.getOption(GB_OPT_SGB_MODE);

        if((gbcMode == GBC_IF_NEEDED && this->cartridge->isCgbRequired()) || (gbcMode == GBC_ON && this->cartridge->isCgbSupported())) {
            if(sgbMode == SGB_PREFER_SGB && this->cartridge->isSgbEnhanced()) {
                this->gbMode = MODE_SGB;
            } else {
                this->gbMode = MODE_CGB;
            }
        } else {
            if(sgbMode != SGB_OFF && this->cartridge->isSgbEnhanced()) {
                this->gbMode = MODE_SGB;
            } else {
                this->gbMode = MODE_GB;
            }
        }
    } else {
        this->gbMode = MODE_CGB;
    }

    this->mmu.reset();
    this->cpu.reset();
    this->ppu.reset();
    this->apu.reset();
    this->sgb.reset();
    this->timer.reset();
    this->serial.reset();

    if(this->cartridge != nullptr) {
        this->cartridge->reset(this);
    }

    this->poweredOn = true;
}

void Gameboy::powerOff() {
    this->poweredOn = false;
}

bool Gameboy::loadState(std::istream& data) {
    u8 version;
    data.read((char*) &version, sizeof(version));

    if(version < 12 || version > STATE_VERSION) {
        return false;
    }

    data.read((char*) &this->gbMode, sizeof(this->gbMode));

    data >> this->mmu;
    data >> this->cpu;
    data >> this->ppu;
    data >> this->apu;
    data >> this->sgb;
    data >> this->timer;
    data >> this->serial;

    if(this->cartridge != nullptr) {
        data >> *this->cartridge;
    }

    return true;
}

bool Gameboy::saveState(std::ostream& data) {
    data.write((char*) &STATE_VERSION, sizeof(STATE_VERSION));

    data.write((char*) &this->gbMode, sizeof(this->gbMode));

    data << this->mmu;
    data << this->cpu;
    data << this->ppu;
    data << this->apu;
    data << this->sgb;
    data << this->timer;
    data << this->serial;

    if(this->cartridge != nullptr) {
        data << *this->cartridge;
    }

    return true;
}

void Gameboy::runFrame() {
    if(!this->poweredOn) {
        return;
    }

    this->ranFrame = false;
    this->audioSamplesWritten = 0;

    while(!this->ranFrame) {
        this->cpu.run();
    }
}
