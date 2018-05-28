#pragma once

#include "types.h"

#include "menu.h"

#include <vector>

class MainMenu : public Menu {
public:
    bool processInput(UIKey key, u32 width, u32 height);
    void draw(u32 width, u32 height);

    void printMessage(const std::string& m);

    void updateGameStatus();
private:
    void exit();
    void reset();
    void suspend();
    void stateSlotChanged();
    void saveState();
    void loadState();
    void deleteState();
    void romInfo();
    void inputSettings();
    void manageCheats();
    void saveSettings();
    void exitWithoutSaving();
    void quitToLauncher();

    typedef enum {
        MENU_ITEM_GENERIC,
        MENU_ITEM_MULTI_CHOICE,
        MENU_ITEM_PATH
    } MenuItemType;

    typedef struct {
        std::string name;

        void (MainMenu::*action)();

        bool enabled;
    } ActionMenuItem;

    std::vector<ActionMenuItem> actionMenu = {
            {"Exit", &MainMenu::exit, true},
            {"Reset", &MainMenu::reset, true},
            {"Suspend", &MainMenu::suspend, false},
            {"State Slot", &MainMenu::stateSlotChanged, false},
            {"Save State", &MainMenu::saveState, false},
            {"Load State", &MainMenu::loadState, false},
            {"Delete State", &MainMenu::deleteState, false},
            {"ROM Info", &MainMenu::romInfo, false},
            {"Input Settings", &MainMenu::inputSettings, true},
            {"Manage Cheats", &MainMenu::manageCheats, false},
            {"Save Settings", &MainMenu::saveSettings, true},
            {"Exit without saving", &MainMenu::exitWithoutSaving, true},
            {"Quit to Launcher", &MainMenu::quitToLauncher, true}
    };

    u8 currMenu = 0;
    s16 currOption = -1;

    std::string message = "";

    u8 stateNum = 0;

    u8 getSubMenuCount();
    const std::string getSubMenuName(u8 menu);
    u8 getSubMenuItemCount(u8 menu);
    bool isItemEnabled(u8 menu, u8 item);
    void setItemEnabled(u8 menu, u8 item, bool enabled);
    MenuItemType getItemType(u8 menu, u8 item);
    const std::string getItemName(u8 menu, u8 item);
    const std::string getItemValue(u8 menu, u8 item);
    void toggleItemValue(u8 menu, u8 item, s8 dir);
    void doItemAction(u8 menu, u8 item);
};