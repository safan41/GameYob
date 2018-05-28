#pragma once

#include "types.h"

#include "apu.h"
#include "cpu.h"
#include "mmu.h"
#include "ppu.h"
#include "printer.h"
#include "serial.h"
#include "sgb.h"
#include "timer.h"

class APU;
class CPU;
class Cartridge;
class PPU;
class Printer;
class Serial;
class SGB;
class Timer;

// Cycle Constants
#define CYCLES_PER_SECOND 4194304
#define CYCLES_PER_FRAME 70224

// Buttons
#define GB_A 0x01
#define GB_B 0x02
#define GB_SELECT 0x04
#define GB_START 0x08
#define GB_RIGHT 0x10
#define GB_LEFT 0x20
#define GB_UP 0x40
#define GB_DOWN 0x80

typedef enum {
    MODE_GB,
    MODE_SGB,
    MODE_CGB
} GBMode;

typedef enum {
    SGB_OFF = 0,
    SGB_PREFER_GBC,
    SGB_PREFER_SGB
} SGBMode;

typedef enum {
    GBC_OFF = 0,
    GBC_IF_NEEDED,
    GBC_ON
} GBCMode;

typedef enum {
    /* SGBMode */
    GB_OPT_SGB_MODE = 0,
    /* GBCMode */
    GB_OPT_GBC_MODE,
    /* bool */
    GB_OPT_GBA_MODE,
    GB_OPT_BIOS_ENABLED,
    GB_OPT_PRINTER_ENABLED,
    GB_OPT_DRAW_ENABLED,
    GB_OPT_PER_PIXEL_RENDERING,
    GB_OPT_EMULATE_BLUR,
    GB_OPT_SOUND_ENABLED,
    GB_OPT_SOUND_CHANNEL_1_ENABLED,
    GB_OPT_SOUND_CHANNEL_2_ENABLED,
    GB_OPT_SOUND_CHANNEL_3_ENABLED,
    GB_OPT_SOUND_CHANNEL_4_ENABLED,

    NUM_GB_OPT
} GameboyOption;

typedef struct {
    void (*printDebug)(const char* s, ...);

    u16 (*readTiltX)();
    u16 (*readTiltY)();
    void (*setRumble)(bool rumble);

    u32* (*getCameraImage)();

    void (*printImage)(bool appending, u8* buf, int size, u8 palette);

    u8 (*getOption)(GameboyOption opt);

    u32* frameBuffer;
    u32 framePitch;

    u32* audioBuffer;
    u32 audioSamples;
    u32 audioSampleRate;
} GameboySettings;

class Gameboy {
public:
    Gameboy();
    ~Gameboy();

    void powerOn();
    void powerOff();

    bool loadState(std::istream& data);
    bool saveState(std::ostream& data);

    void runFrame();

    inline bool isPoweredOn() {
        return this->poweredOn;
    }

    GameboySettings settings;

    Cartridge* cartridge;

    MMU mmu;
    CPU cpu;
    PPU ppu;
    APU apu;
    SGB sgb;
    Timer timer;
    Serial serial;

    GBMode gbMode;

    bool ranFrame;
    u32 audioSamplesWritten;
private:
    bool poweredOn = false;
};