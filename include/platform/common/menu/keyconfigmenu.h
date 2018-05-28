#pragma once

#include "types.h"

#include "menu.h"

class KeyConfigMenu : public Menu {
public:
    bool processInput(UIKey key, u32 width, u32 height);
    void draw(u32 width, u32 height);
private:
    int option = -1;
    int cursor = -1;
    u32 scrollY = 0;
};