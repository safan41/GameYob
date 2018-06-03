#pragma once

#include "types.h"

class Timer {
public:
    Timer(Gameboy* gameboy);

    void reset();
    void update();

    u8 read(u16 addr);
    void write(u16 addr, u8 val);

    friend std::istream& operator>>(std::istream& is, Timer& timer);
    friend std::ostream& operator<<(std::ostream& os, const Timer& timer);
private:
    Gameboy* gameboy;

    u64 lastDividerCycle;
    u64 lastTimerCycle;
};