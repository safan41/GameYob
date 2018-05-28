#pragma once

#include "types.h"

#include "platform/ui.h"

class Menu {
public:
    virtual ~Menu() {
    }

    // Returns whether or not a redraw is needed.
    virtual bool processInput(UIKey key, u32 width, u32 height) = 0;
    virtual void draw(u32 width, u32 height) = 0;
};

bool menuIsVisible();

void menuPush(Menu* menu);
void menuPop();

void menuUpdate();

void menuOpenMain();

