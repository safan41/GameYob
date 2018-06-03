#pragma once

#include "types.h"

class Serial {
public:
    Serial(Gameboy* gameboy);

    void reset();
    void update();

    void write(u16 addr, u8 val);

    friend std::istream& operator>>(std::istream& is, Serial& serial);
    friend std::ostream& operator<<(std::ostream& os, const Serial& serial);
private:
    Gameboy* gameboy;

    Printer printer;

    u64 nextSerialInternalCycle;
    u64 nextSerialExternalCycle;
};