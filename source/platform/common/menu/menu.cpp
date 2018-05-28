#include <stack>

#include "platform/common/menu/mainmenu.h"
#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/input.h"
#include "platform/ui.h"

static std::stack<Menu*> menuStack;
static Menu* currMenu = nullptr;

static MainMenu mainMenu = MainMenu();

bool menuIsVisible() {
    return !menuStack.empty();
}

void menuPush(Menu* menu) {
    if(menuStack.empty()) {
        inputReleaseAll();
    }

    menuStack.push(menu);
}

void menuPop() {
    Menu* menu = menuStack.top();
    menuStack.pop();

    if(menu != &mainMenu) {
        delete menu;
    }

    if(menuStack.empty()) {
        inputReleaseAll();

        uiClear();
        uiFlush();
    }
}

void menuUpdate() {
    if(!menuStack.empty()) {
        u32 width = 0;
        u32 height = 0;
        bool redraw = false;

        UIKey key;
        while(!menuStack.empty() && (key = uiReadKey()) != UI_KEY_NONE) {
            uiGetSize(&width, &height);

            redraw = menuStack.top()->processInput(key, width, height) || redraw;
        }

        if(!menuStack.empty()) {
            Menu* menu = menuStack.top();
            if(currMenu != menu) {
                currMenu = menu;

                redraw = true;
            }

            if(redraw) {
                uiGetSize(&width, &height);

                uiClear();
                menu->draw(width, height);
                uiFlush();
            }
        } else {
            currMenu = nullptr;
        }
    } else {
        currMenu = nullptr;
    }
}

void menuOpenMain() {
    menuStack.push(&mainMenu);

    mainMenu.updateGameStatus();
}