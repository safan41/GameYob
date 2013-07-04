// This file can be included by arm7 and arm9.
#pragma once

typedef struct SharedData {
    u8 scalingOn;
    u8 scaleTransferReady;
    volatile int fifosWaiting;

    // Sound stuff
    bool hyperSound;
    int cycles;
    int dsCycles;
    bool frameFlip_Gameboy;
    bool frameFlip_DS;

    int SO1Vol, SO2Vol;
    u8 chanRealVol[4];
    int chanRealFreq[4];
    u8 chanPan[4];
    int chanDuty[2];
    u8 chanOn;
    bool chanEnabled[4];
    bool lfsr7Bit;
    u8* sampleData;

    // Use this with basicSound to remember the channels which have been enabled 
    // this frame.
    int channelsToStart;

    u32 message;
} SharedData;

enum {
    GBSND_UPDATE_VBLANK_COMMAND=0,
    GBSND_UPDATE_COMMAND,
    GBSND_START_COMMAND,
    GBSND_VOLUME_COMMAND,
    GBSND_MASTER_VOLUME_COMMAND,
    GBSND_KILL_COMMAND,
    GBSND_MUTE_COMMAND,
    GBSND_HYPERSOUND_ENABLE_COMMAND
};

#define VALUE32_SIZE 16
extern volatile SharedData* sharedData;

