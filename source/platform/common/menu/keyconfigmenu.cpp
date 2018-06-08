#include <sstream>
#include <vector>

#include "platform/common/menu/keyconfigmenu.h"
#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/input.h"
#include "platform/ui.h"

bool KeyConfigMenu::processInput(UIKey key, u32 width, u32 height) {
    KeyConfig* config = configGetKeyConfig(configGetSelectedKeyConfig());
    u32 keyCount = inputGetKeyCount();

    if(key == UI_KEY_B) {
        inputLoadKeyConfig(config);

        menuPop();
    } else if(key == UI_KEY_X) {
        std::stringstream name;
        name << "Custom " << configGetKeyConfigCount();
        configAddKeyConfig(name.str());
        configSetSelectedKeyConfig(configGetKeyConfigCount() - 1);

        option = -1;
        cursor = -1;
        scrollY = 0;

        return true;
    } else if(key == UI_KEY_Y) {
        u32 selected = configGetSelectedKeyConfig();
        if(selected != 0) { /* can't erase the default */
            configRemoveKeyConfig(selected);

            return true;
        }
    } else if(key == UI_KEY_DOWN) {
        if(option == (int) keyCount - 1) {
            option = -1;
            cursor = -1;
        } else {
            cursor++;
            option++;

            while(!inputIsValidKey((u32) option)) {
                option++;

                if(option >= (int) keyCount) {
                    option = -1;
                    cursor = -1;

                    break;
                }
            }
        }

        if(cursor < 0) {
            scrollY = 0;
        } else {
            while(cursor < (int) scrollY) {
                scrollY--;
            }

            while(cursor >= (int) (scrollY + height - 7)) {
                scrollY++;
            }
        }

        return true;
    } else if(key == UI_KEY_UP) {
        if(option == -1) {
            option = keyCount - 1;
            while(!inputIsValidKey((u32) option)) {
                option--;
                if(option < 0) {
                    option = -1;
                    cursor = -1;
                    break;
                }
            }

            if(option != -1) {
                cursor = 0;
                for(u32 i = 0; i < keyCount; i++) {
                    if(inputIsValidKey(i)) {
                        cursor++;
                    }
                }
            }
        } else {
            cursor--;
            option--;
            while(!inputIsValidKey((u32) option)) {
                option--;
                if(option < 0) {
                    option = -1;
                    cursor = -1;
                    break;
                }
            }
        }

        if(cursor < 0) {
            scrollY = 0;
        } else {
            while(cursor < (int) scrollY) {
                scrollY--;
            }

            while(cursor >= (int) (scrollY + height - 7)) {
                scrollY++;
            }
        }

        return true;
    } else if(key == UI_KEY_LEFT) {
        if(option == -1) {
            u32 selected = configGetSelectedKeyConfig();
            if(selected == 0) {
                configSetSelectedKeyConfig(configGetKeyConfigCount() - 1);
            } else {
                configSetSelectedKeyConfig(selected - 1);
            }
        } else {
            if(config->funcKeys[option] <= 0) {
                config->funcKeys[option] = NUM_FUNC_KEYS - 1;
            } else {
                config->funcKeys[option]--;
            }
        }

        return true;
    } else if(key == UI_KEY_RIGHT) {
        if(option == -1) {
            u32 selected = configGetSelectedKeyConfig();
            if(selected >= configGetKeyConfigCount() - 1) {
                configSetSelectedKeyConfig(0);
            } else {
                configSetSelectedKeyConfig(selected + 1);
            }
        } else {
            if(config->funcKeys[option] >= NUM_FUNC_KEYS - 1) {
                config->funcKeys[option] = FUNC_KEY_NONE;
            } else {
                config->funcKeys[option]++;
            }
        }

        return true;
    }

    return false;
}

void KeyConfigMenu::draw(u32 width, u32 height) {
    KeyConfig* config = configGetKeyConfig(configGetSelectedKeyConfig());

    uiPrint("Config: ");
    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_YELLOW);
        uiPrint("* %s *\n\n", config->name.c_str());
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiAdvanceCursor(2);
        uiPrint("%s\n\n", config->name.c_str());
    }

    uiAdvanceCursor(width / 2 - 7);
    uiPrint("Button   Function\n\n");

    u32 keyCount = inputGetKeyCount();
    for(u32 i = 0, elements = 0; i < keyCount && elements < scrollY + height - 7; i++) {
        if(!inputIsValidKey(i)) {
            continue;
        }

        if(elements < scrollY) {
            elements++;
            continue;
        }

        std::string keyName = inputGetKeyName(i);

        u32 maxKeyNameLength = width / 2 - 3;
        if(keyName.length() > maxKeyNameLength) {
            keyName = keyName.substr(0, maxKeyNameLength);
        }

        uiAdvanceCursor(maxKeyNameLength - keyName.length());

        if(option == (int) i) {
            uiSetLineHighlighted(true);
            uiPrint("* %s | %s *\n", keyName.c_str(), configGetFuncKeyName(config->funcKeys[i]).c_str());
            uiSetLineHighlighted(false);
        } else {
            uiAdvanceCursor(2);
            uiPrint("%s | %s\n", keyName.c_str(), configGetFuncKeyName(config->funcKeys[i]).c_str());
        }

        elements++;
    }

    uiPrint("\nPress X to make a new config.");
    if(configGetSelectedKeyConfig() != 0) {
        uiPrint("\nPress Y to delete this config.");
    }
}