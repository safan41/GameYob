#include <istream>
#include <ostream>

#include "gb_apu/Gb_Apu.h"
#include "gb_apu/Multi_Buffer.h"

#include "apu.h"
#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"

APU::APU(Gameboy* gameboy) : apu(gameboy) {
    this->gameboy = gameboy;
}

void APU::reset() {
    this->buffer.set_sample_rate((long) this->gameboy->settings.audioSampleRate);
    this->buffer.clock_rate(CYCLES_PER_SECOND);
    this->buffer.clear();

    this->apu.set_output(this->buffer.center(), this->buffer.left(), this->buffer.right());
    this->apu.reset();

    this->lastSoundCycle = 0;
    this->halfSpeed = false;
}

void APU::update() {
    if(this->gameboy->cpu.getCycle() >= this->lastSoundCycle + (CYCLES_PER_FRAME << this->halfSpeed)) {
        u32 cycles = (u32) (this->gameboy->cpu.getCycle() - this->lastSoundCycle) >> this->halfSpeed;

        this->apu.end_frame(cycles);
        this->buffer.end_frame(cycles);

        this->lastSoundCycle = this->gameboy->cpu.getCycle();

        if(this->gameboy->settings.getOption(GB_OPT_SOUND_ENABLED) && this->gameboy->settings.audioBuffer != nullptr) {
            long space = this->gameboy->settings.audioSamples - this->gameboy->audioSamplesWritten;
            long read = this->buffer.samples_avail() / 2;
            if(read > space) {
                read = space;
            }

            this->gameboy->audioSamplesWritten += this->buffer.read_samples((s16*) &this->gameboy->settings.audioBuffer[this->gameboy->audioSamplesWritten], read * 2) / 2;
        } else {
            this->buffer.clear();
        }
    }

    this->gameboy->cpu.setEventCycle(this->lastSoundCycle + (CYCLES_PER_FRAME << this->halfSpeed));
}

u8 APU::read(u16 addr) {
    return (u8) this->apu.read_register((u32) (this->gameboy->cpu.getCycle() - this->lastSoundCycle) >> this->halfSpeed, addr);
}

void APU::write(u16 addr, u8 val) {
    this->apu.write_register((u32) (this->gameboy->cpu.getCycle() - this->lastSoundCycle) >> this->halfSpeed, addr, val);
}

void APU::setHalfSpeed(bool halfSpeed) {
    if(!this->halfSpeed && halfSpeed) {
        this->lastSoundCycle -= this->gameboy->cpu.getCycle() - this->lastSoundCycle;
    } else if(this->halfSpeed && !halfSpeed) {
        this->lastSoundCycle += (this->gameboy->cpu.getCycle() - this->lastSoundCycle) >> 1;
    }

    this->halfSpeed = halfSpeed;
}

std::istream& operator>>(std::istream& is, APU& apu) {
    gb_apu_state_t apuState;
    is.read((char*) &apuState, sizeof(apuState));
    is.read((char*) &apu.lastSoundCycle, sizeof(apu.lastSoundCycle));
    is.read((char*) &apu.halfSpeed, sizeof(apu.halfSpeed));

    apu.apu.load_state(apuState);

    return is;
}

std::ostream& operator<<(std::ostream& os, APU& apu) {
    gb_apu_state_t apuState;
    apu.apu.save_state(&apuState);

    os.write((char*) &apuState, sizeof(apuState));
    os.write((char*) &apu.lastSoundCycle, sizeof(apu.lastSoundCycle));
    os.write((char*) &apu.halfSpeed, sizeof(apu.halfSpeed));

    return os;
}