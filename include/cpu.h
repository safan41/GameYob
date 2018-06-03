#pragma once

#include "types.h"

class Gameboy;

#define INT_VBLANK 0x01
#define INT_LCD 0x02
#define INT_TIMER 0x04
#define INT_SERIAL 0x08
#define INT_JOYPAD 0x10

class CPU {
public:
    CPU(Gameboy* gameboy);

    void reset();
    void run();

    void write(u16 addr, u8 val);

    friend std::istream& operator>>(std::istream& is, CPU& cpu);
    friend std::ostream& operator<<(std::ostream& os, const CPU& cpu);

    inline void advanceCycles(u64 cycles) {
        this->cycleCount += cycles;

        if(this->cycleCount >= this->eventCycle) {
            this->eventCycle = UINT64_MAX;

            this->updateEvents();
        }
    }

    inline u64 getCycle() {
        return this->cycleCount;
    }

    inline void setEventCycle(u64 cycle) {
        if(cycle < this->eventCycle) {
            this->eventCycle = cycle > this->cycleCount ? cycle : this->cycleCount;
        }
    }
private:
    void updateEvents();

    void alu(u8 func, u8 val);
    u8 rot(u8 func, u8 val);

    Gameboy* gameboy;

    u64 cycleCount;
    u64 eventCycle;

    struct {
        union {
            u8 r8[12];
            u16 r16[6];
        };
    } registers;

    bool haltState;
    bool haltBug;

    bool ime;
    u64 imeCycle;
};