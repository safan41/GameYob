#pragma once

#include "types.h"

#include <vector>

class Gameboy;

class CheatEngine {
public:
    CheatEngine(Gameboy* g) : gameboy(g) {}

    void loadCheats(const std::string& filename);
    void saveCheats(const std::string& filename);

    inline u32 getNumCheats() { return (u32) cheatsVec.size(); }

    inline const std::string getCheatName(u32 cheat) { return cheatsVec[cheat].name; }
    inline bool isCheatEnabled(u32 cheat) { return cheatsVec[cheat].enabled; }

    void addCheat(const std::string& name, const std::string& value);
    void toggleCheat(u32 cheat, bool enabled);

    void applyRamCheats();
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
            u8 bank;    /* For Gameshark codes */
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

    std::vector<Cheat> cheatsVec;

    static int parseCheatsIni(void* user, const char* section, const char* name, const char* value);
    void parseLine(Cheat& cheat, const std::string& line);

    void applyAllRomCheats();
    void unapplyRomCheat(u32 cheat);
};