#include <vector>

#include "platform/common/menu/cheatmenu.h"
#include "platform/common/menu/keyconfigmenu.h"
#include "platform/common/menu/filechooser.h"
#include "platform/common/menu/mainmenu.h"
#include "platform/common/menu/rominfo.h"
#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/system.h"
#include "platform/ui.h"
#include "cartridge.h"
#include "gameboy.h"

#define ACTION_MENU 0

#define ACTION_MENU_EXIT 0
#define ACTION_MENU_RESET 1
#define ACTION_MENU_SUSPEND 2
#define ACTION_MENU_STATE_SLOT 3
#define ACTION_MENU_SAVE_STATE 4
#define ACTION_MENU_LOAD_STATE 5
#define ACTION_MENU_DELETE_STATE 6
#define ACTION_MENU_ROM_INFO 7
#define ACTION_MENU_BUTTON_MAPPING 8
#define ACTION_MENU_MANAGE_CHEATS 9
#define ACTION_MENU_SAVE_SETTINGS 10
#define ACTION_MENU_EXIT_WITHOUT_SAVING 11
#define ACTION_MENU_QUIT_TO_LAUNCHER 12

#define STATE_SLOT_MIN 0
#define STATE_SLOT_MAX 9

bool MainMenu::processInput(UIKey key, u32 width, u32 height) {
    if(key == UI_KEY_UP) {
        currOption--;
        if(currOption < -1) {
            currOption = getSubMenuItemCount(currMenu) - 1;
        }

        return true;
    } else if(key == UI_KEY_DOWN) {
        currOption++;
        if(currOption >= getSubMenuItemCount(currMenu)) {
            currOption = -1;
        }

        return true;
    } else if(key == UI_KEY_LEFT) {
        if(currOption == -1) {
            if(currMenu == 0) {
                currMenu = getSubMenuCount() - 1;
            } else {
                currMenu--;
            }

            u8 numOptions = getSubMenuItemCount(currMenu);
            if(currOption >= numOptions) {
                currOption = numOptions - 1;
            }
        } else if(getItemType(currMenu, (u8) currOption) == MENU_ITEM_MULTI_CHOICE && isItemEnabled(currMenu, (u8) currOption)) {
            toggleItemValue(currMenu, (u8) currOption, -1);
        }

        return true;
    } else if(key == UI_KEY_RIGHT) {
        if(currOption == -1) {
            currMenu++;
            if(currMenu >= getSubMenuCount()) {
                currMenu = 0;
            }

            u8 numOptions = getSubMenuItemCount(currMenu);
            if(currOption >= numOptions) {
                currOption = numOptions - 1;
            }

        } else if(getItemType(currMenu, (u8) currOption) == MENU_ITEM_MULTI_CHOICE && isItemEnabled(currMenu, (u8) currOption)) {
            toggleItemValue(currMenu, (u8) currOption, 1);
        }

        return true;
    } else if(key == UI_KEY_A) {
        if(currOption >= 0 && isItemEnabled(currMenu, (u8) currOption)) {
            doItemAction(currMenu, (u8) currOption);
        }

        return true;
    } else if(key == UI_KEY_B) {
        menuPop();
    } else if(key == UI_KEY_L) {
        if(currMenu == 0) {
            currMenu = getSubMenuCount() - 1;
        } else {
            currMenu--;
        }

        u8 numOptions = getSubMenuItemCount(currMenu);
        if(currOption >= numOptions) {
            currOption = numOptions - 1;
        }

        return true;
    } else if(key == UI_KEY_R) {
        currMenu++;
        if(currMenu >= getSubMenuCount()) {
            currMenu = 0;
        }

        u8 numOptions = getSubMenuItemCount(currMenu);
        if(currOption >= numOptions) {
            currOption = numOptions - 1;
        }

        return true;
    }

    return false;
}

void MainMenu::draw(u32 width, u32 height) {
    std::string menuName = getSubMenuName(currMenu);

    u32 menuMax = width - 8;
    if(menuName.length() > menuMax) {
        menuName = menuName.substr(0, menuMax);
    }

    u32 menuPad = (menuMax - menuName.length()) / 2;

    if(currOption == -1) {
        uiSetTextColor(TEXT_COLOR_GREEN);
        uiPrint("<");
        uiSetTextColor(TEXT_COLOR_YELLOW);
    } else {
        uiPrint("<");
    }

    uiAdvanceCursor(menuPad);

    if(currOption == -1) {
        uiPrint("* [%s] *", menuName.c_str());
    } else {
        uiAdvanceCursor(2);
        uiPrint("[%s]", menuName.c_str());
        uiAdvanceCursor(2);
    }

    if((width & 1) == 0) {
        menuPad++;
    }

    uiAdvanceCursor(menuPad);

    if(currOption == -1) {
        uiSetTextColor(TEXT_COLOR_GREEN);
        uiPrint(">\n");
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiPrint(">\n");
    }

    // Rest of the lines: options
    u8 numOptions = getSubMenuItemCount(currMenu);
    for(u8 i = 0; i < numOptions; i++) {
        bool enabled = isItemEnabled(currMenu, i);

        if(!enabled) {
            uiSetTextColor(TEXT_COLOR_GRAY);
        } else if(currOption == i) {
            uiSetTextColor(TEXT_COLOR_YELLOW);
        }

        MenuItemType type = getItemType(currMenu, i);
        std::string name = getItemName(currMenu, i);

        u32 entryMax = width - 4;
        if(name.length() > entryMax) {
            name = name.substr(0, entryMax);
        }

        if(type != MENU_ITEM_MULTI_CHOICE) {
            uiAdvanceCursor((entryMax - name.length()) / 2);

            if(i == currOption) {
                uiPrint("* %s *\n\n", name.c_str());
            } else {
                uiAdvanceCursor(2);
                uiPrint("%s\n\n", name.c_str());
            }
        } else {
            std::string value = getItemValue(currMenu, i);

            u32 entryValueMax = entryMax - name.length() - 2;
            if(value.length() > entryValueMax) {
                value = value.substr(0, entryValueMax);
            }

            uiAdvanceCursor((entryValueMax - value.length()) / 2);

            if(i == currOption) {
                uiPrint("* %s  ", name.c_str());

                if(enabled) {
                    uiSetTextColor(TEXT_COLOR_GREEN);
                }

                uiPrint("%s", value.c_str());

                if(!enabled) {
                    uiSetTextColor(TEXT_COLOR_GRAY);
                } else if(currOption == i) {
                    uiSetTextColor(TEXT_COLOR_YELLOW);
                }

                uiPrint(" *\n\n");
            } else {
                uiAdvanceCursor(2);
                uiPrint("%s  %s\n\n", name.c_str(), value.c_str());
            }
        }

        uiSetTextColor(TEXT_COLOR_NONE);
    }

    if(!message.empty()) {
        uiSetLine(height - 1);
        uiAdvanceCursor(width - message.length());
        uiPrint("%s", message.c_str());

        message = "";
    }
}

void MainMenu::printMessage(const std::string& m) {
    u32 width = 0;
    u32 height = 0;
    uiGetSize(&width, &height);

    if(m.length() > width) {
        message = m.substr(0, width);
    } else {
        message = m;
    }

    uiSetLine(height - 1);
    uiAdvanceCursor(width - message.length());
    uiPrint("%s", message.c_str());

    uiFlush();
}

void MainMenu::updateGameStatus() {
    bool cartLoaded = mgrGetGameboy()->cartridge != nullptr;
    bool stateExists = mgrStateExists(stateNum);

    setItemEnabled(ACTION_MENU, ACTION_MENU_SUSPEND, cartLoaded);
    setItemEnabled(ACTION_MENU, ACTION_MENU_STATE_SLOT, cartLoaded);
    setItemEnabled(ACTION_MENU, ACTION_MENU_SAVE_STATE, cartLoaded);
    setItemEnabled(ACTION_MENU, ACTION_MENU_LOAD_STATE, cartLoaded && stateExists);
    setItemEnabled(ACTION_MENU, ACTION_MENU_DELETE_STATE, cartLoaded && stateExists);
    setItemEnabled(ACTION_MENU, ACTION_MENU_ROM_INFO, cartLoaded);
    setItemEnabled(ACTION_MENU, ACTION_MENU_MANAGE_CHEATS, cartLoaded);
}

u8 MainMenu::getSubMenuCount() {
    return configGetGroupCount() + 1;
}

const std::string MainMenu::getSubMenuName(u8 menu) {
    if(menu == 0) {
        return "Main Menu";
    } else {
        return configGetGroupName(menu - 1);
    }
}

u8 MainMenu::getSubMenuItemCount(u8 menu) {
    if(menu == 0) {
        return actionMenu.size();
    } else {
        return configGetGroupMultiChoiceCount(menu - 1) + configGetGroupPathCount(menu - 1);
    }
}

bool MainMenu::isItemEnabled(u8 menu, u8 item) {
    return menu != 0 || (item < actionMenu.size() && actionMenu[item].enabled);
}

void MainMenu::setItemEnabled(u8 menu, u8 item, bool enabled) {
    if(menu == 0 && item < actionMenu.size()) {
        actionMenu[item].enabled = enabled;
    }
}

MainMenu::MenuItemType MainMenu::getItemType(u8 menu, u8 item) {
    if(menu == 0) {
        if(item == ACTION_MENU_STATE_SLOT) {
            return MENU_ITEM_MULTI_CHOICE;
        } else {
            return MENU_ITEM_GENERIC;
        }
    } else {
        return item < configGetGroupMultiChoiceCount(menu - 1) ? MENU_ITEM_MULTI_CHOICE : MENU_ITEM_PATH;
    }
}

const std::string MainMenu::getItemName(u8 menu, u8 item) {
    if(menu == 0) {
        if(item < actionMenu.size()) {
            return actionMenu[item].name;
        } else {
            return "";
        }
    } else {
        u8 group = menu - 1;
        u8 multiChoiceCount = configGetGroupMultiChoiceCount(group);

        if(item < multiChoiceCount) {
            return configGetMultiChoiceName(group, item);
        } else {
            return configGetPathName(group, item - multiChoiceCount);
        }
    }
}

const std::string MainMenu::getItemValue(u8 menu, u8 item) {
    if(menu == 0) {
        if(item == ACTION_MENU_STATE_SLOT) {
            return std::to_string(stateNum);
        } else {
            return "";
        }
    } else {
        u8 group = menu - 1;
        u8 multiChoiceCount = configGetGroupMultiChoiceCount(group);

        if(item < multiChoiceCount) {
            return configGetMultiChoiceValueName(group, item, configGetMultiChoice(group, item));
        } else {
            return configGetPath(group, item - multiChoiceCount);
        }
    }
}

void MainMenu::toggleItemValue(u8 menu, u8 item, s8 dir) {
    if(menu == 0) {
        if(item == ACTION_MENU_STATE_SLOT) {
            if(dir == -1 && stateNum == STATE_SLOT_MIN) {
                stateNum = STATE_SLOT_MAX;
            } else if(dir == 1 && stateNum == STATE_SLOT_MAX) {
                stateNum = STATE_SLOT_MIN;
            } else {
                stateNum += dir;
            }

            (this->*(actionMenu[item].action))();
        }
    } else {
        u8 group = menu - 1;
        u8 multiChoiceCount = configGetGroupMultiChoiceCount(group);

        if(item < multiChoiceCount) {
            configToggleMultiChoice(group, item, dir);
        }
    }
}

void MainMenu::doItemAction(u8 menu, u8 item) {
    if(menu == 0) {
        if(item < actionMenu.size() && item != ACTION_MENU_STATE_SLOT) {
            (this->*(actionMenu[item].action))();
        }
    } else {
        u8 group = menu - 1;
        u8 multiChoiceCount = configGetGroupMultiChoiceCount(group);

        if(item >= multiChoiceCount) {
            u8 pathItem = item - multiChoiceCount;

            menuPush(new FileChooser([group, pathItem](bool chosen, const std::string &path) -> void {
                if(chosen) {
                    configSetPath(group, pathItem, path);
                }
            }, configGetPath(group, pathItem), configGetPathExtensions(group, pathItem), true));
        }
    }
}

void MainMenu::exit() {
    mgrUnloadRom();

    menuPop();
}

void MainMenu::reset() {
    mgrReset();

    menuPop();
}

void MainMenu::suspend() {
    printMessage("Saving state...");
    mgrSaveState(-1);
    mgrUnloadRom();

    menuPop();
}

void MainMenu::stateSlotChanged() {
    updateGameStatus();
}

void MainMenu::saveState() {
    printMessage("Saving state...");
    if(mgrSaveState(stateNum)) {
        printMessage("State saved.");
    } else {
        printMessage("Could not save state.");
    }

    updateGameStatus();
}

void MainMenu::loadState() {
    if(!mgrStateExists(stateNum)) {
        printMessage("State does not exist.");
        return;
    }

    printMessage("Loading state...");
    if(!mgrLoadState(stateNum)) {
        printMessage("Could not load state.");
        return;
    }

    menuPop();
}

void MainMenu::deleteState() {
    mgrDeleteState(stateNum);

    updateGameStatus();
}

void MainMenu::romInfo() {
    menuPush(new RomInfoMenu());
}

void MainMenu::inputSettings() {
    menuPush(new KeyConfigMenu());
}

void MainMenu::manageCheats() {
    menuPush(new CheatsMenu());
}

void MainMenu::saveSettings() {
    printMessage("Saving settings...");
    configSave();
    printMessage("Settings saved.");
}

void MainMenu::exitWithoutSaving() {
    menuPop();

    mgrUnloadRom(false);
}

void MainMenu::quitToLauncher() {
    systemRequestExit();
}