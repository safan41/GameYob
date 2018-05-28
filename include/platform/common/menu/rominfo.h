#pragma once

#include "types.h"

#include "menu.h"

class RomInfoMenu : public Menu {
public:
    bool processInput(UIKey key, u32 width, u32 height);
    void draw(u32 width, u32 height);
};