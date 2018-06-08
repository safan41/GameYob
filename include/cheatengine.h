#pragma once

#include "types.h"

#include <vector>

class Gameboy;

class CheatEngine {
public:
    CheatEngine(Gameboy* g) : gameboy(g) {}

    void reset();
    void update();

    void addCheat(const std::string& name, const std::string& value);

    void toggleCheat(u32 cheat, bool enabled);
    void toggleCheat(const std::string& cheat, bool enabled);

    inline u32 getNumCheats() {
        return (u32) this->cheats.size();
    }

    inline const std::string getCheatName(u32 cheat) {
        return this->cheats[cheat].name;
    }

    inline const std::string getCheatValue(u32 cheat) {
        return this->cheats[cheat].value;
    }

    inline bool isCheatEnabled(u32 cheat) {
        return this->cheats[cheat].enabled;
    }
private:
    typedef enum {
        CHEAT_TYPE_UNKNOWN,
        CHEAT_TYPE_GAMEGENIE_SHORT,
        CHEAT_TYPE_GAMEGENIE_LONG,
        CHEAT_TYPE_GAMESHARK
    } CheatType;

    typedef struct {
        CheatType type;

        u16 address;
        u8 data;
        union {
            u8 compare; /* For GameGenie codes */
            u8 bank;    /* For GameShark codes */
        };
        std::vector<u16> patchedBanks;  /* For GameGenie codes */
        std::vector<u8> patchedValues; /* For GameGenie codes */
    } CheatLine;

    typedef struct {
        std::string name;
        std::string value;
        bool enabled;

        std::vector<CheatLine> lines;
    } Cheat;

    Gameboy* gameboy;

    std::vector<Cheat> cheats;

    void parseLine(Cheat& cheat, const std::string& line);
};