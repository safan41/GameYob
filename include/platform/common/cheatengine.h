#pragma once

#include "types.h"

#include <vector>

class Gameboy;

#define CHEAT_FLAG_ENABLED    (1 << 0)
#define CHEAT_FLAG_TYPE_MASK  (3 << 1)
#define CHEAT_FLAG_GAMEGENIE  (1 << 1)
#define CHEAT_FLAG_GAMEGENIE1 (2 << 1)
#define CHEAT_FLAG_GAMESHARK  (3 << 1)

typedef struct cheat_t {
    std::string name;
    std::string cheatString;
    u8 flags;
    u8 data;
    u16 address;
    union {
        u8 compare; /* For GameGenie codes */
        u8 bank;    /* For Gameshark codes */
    };
    std::vector<u16> patchedBanks;  /* For GameGenie codes */
    std::vector<u8> patchedValues; /* For GameGenie codes */
} cheat_t;

class CheatEngine {
public:
    CheatEngine(Gameboy* g);

    void loadCheats(const std::string& filename);
    void saveCheats(const std::string& filename);

    inline u32 getNumCheats() { return (u32) cheatsVec.size(); }

    inline const std::string getCheatName(u32 cheat) { return cheatsVec[cheat].name; }
    inline bool isCheatEnabled(u32 cheat) { return (cheatsVec[cheat].flags & CHEAT_FLAG_ENABLED) != 0; }

    bool addCheat(const std::string& name, const std::string& value);
    void toggleCheat(u32 cheat, bool enabled);

    void applyGSCheats();
private:
    Gameboy* gameboy;

    std::vector<cheat_t> cheatsVec;

    void applyGGCheatsToBank(u16 romBank);
    void unapplyGGCheat(u32 cheat);

    static int parseCheats(void* user, const char* section, const char* name, const char* value);
};