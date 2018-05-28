#include "platform/common/menu/cheatmenu.h"
#include "platform/common/menu/menu.h"
#include "platform/common/cheatengine.h"
#include "platform/common/manager.h"
#include "platform/ui.h"

bool CheatsMenu::processInput(UIKey key, u32 width, u32 height) {
    CheatEngine* cheatEngine = mgrGetCheatEngine();
    u32 numCheats = cheatEngine->getNumCheats();

    if(key == UI_KEY_UP) {
        if(selection > 0) {
            selection--;

            return true;
        }
    } else if(key == UI_KEY_DOWN) {
        if(selection < numCheats - 1) {
            selection++;

            return true;
        }
    } else if(key == UI_KEY_RIGHT || key == UI_KEY_LEFT) {
        cheatEngine->toggleCheat(selection, !cheatEngine->isCheatEnabled(selection));

        return true;
    } else if(key == UI_KEY_R) {
        selection += cheatsPerPage;
        if(selection >= numCheats) {
            selection = 0;
        }

        return true;
    } else if(key == UI_KEY_L) {
        selection -= cheatsPerPage;
        if(selection < 0) {
            selection = numCheats - 1;
        }

        return true;
    } else if(key == UI_KEY_B) {
        menuPop();
    }

    return false;
}

void CheatsMenu::draw(u32 width, u32 height) {
    cheatsPerPage = height - 2;

    CheatEngine* cheatEngine = mgrGetCheatEngine();

    u32 numCheats = cheatEngine->getNumCheats();
    u32 numPages = numCheats > 0 ? (numCheats - 1) / cheatsPerPage + 1 : 0;
    u32 page = selection / cheatsPerPage;

    uiPrint("          Cheat Menu      ");
    uiPrint("%" PRIu32 "/%" PRIu32 "\n\n", page + 1, numPages);
    for(u32 i = page * cheatsPerPage; i < numCheats && i < (page + 1) * cheatsPerPage; i++) {
        if(selection == i) {
            uiSetLineHighlighted(true);
        }

        const std::string name = cheatEngine->getCheatName(i);

        uiPrint("%s", name.c_str());

        size_t nameLen = name.length();
        for(u32 j = 0; j < 25 - nameLen; j++) {
            uiPrint(" ");
        }

        if(cheatEngine->isCheatEnabled(i)) {
            if(selection == i) {
                uiSetTextColor(TEXT_COLOR_YELLOW);
                uiPrint("* ");
                uiSetTextColor(TEXT_COLOR_GREEN);
                uiPrint("On");
                uiSetTextColor(TEXT_COLOR_YELLOW);
                uiPrint("  *");
                uiSetTextColor(TEXT_COLOR_NONE);
            } else {
                uiSetTextColor(TEXT_COLOR_GREEN);
                uiPrint("  On   ");
                uiSetTextColor(TEXT_COLOR_NONE);
            }
        } else {
            if(selection == i) {
                uiSetTextColor(TEXT_COLOR_YELLOW);
                uiPrint("* ");
                uiSetTextColor(TEXT_COLOR_RED);
                uiPrint("Off");
                uiSetTextColor(TEXT_COLOR_YELLOW);
                uiPrint(" *");
                uiSetTextColor(TEXT_COLOR_NONE);
            } else {
                uiSetTextColor(TEXT_COLOR_RED);
                uiPrint("  Off  ");
                uiSetTextColor(TEXT_COLOR_NONE);
            }
        }

        uiPrint("\n");

        uiSetLineHighlighted(false);
    }
}