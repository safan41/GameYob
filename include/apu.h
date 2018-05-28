#pragma once

#include "types.h"

#include "gb_apu/Gb_Apu.h"
#include "gb_apu/Multi_Buffer.h"

class Gameboy;

class APU {
public:
    APU(Gameboy* gameboy);

    void reset();
    void update();

    void setHalfSpeed(bool halfSpeed);

    friend std::istream& operator>>(std::istream& is, APU& apu);
    friend std::ostream& operator<<(std::ostream& os, APU& apu);
private:
    Gameboy* gameboy;

    Stereo_Buffer buffer;
    Gb_Apu apu;

    u64 lastSoundCycle;
    bool halfSpeed;
};