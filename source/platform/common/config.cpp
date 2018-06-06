#include <fstream>
#include <vector>

#include "libs/inih/INIReader.h"

#include "platform/common/menu/menu.h"
#include "platform/common/cheatengine.h"
#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/input.h"
#include "platform/system.h"
#include "platform/ui.h"

#define INI_FILE "gameyob.ini"

static const std::string funcKeyNames[NUM_FUNC_KEYS] = {
        "-",
        "A",
        "B",
        "Left",
        "Right",
        "Up",
        "Down",
        "Start",
        "Select",
        "Menu",
        "Menu/Pause",
        "Save",
        "Autofire A",
        "Autofire B",
        "Fast Forward",
        "FF Toggle",
        "Scale",
        "Reset",
        "Screenshot"
};

typedef struct {
    std::string name;

    std::vector<std::string> extensions;
    std::string value;

    void (*onChange)();
} ConfigPath;

typedef struct {
    std::string name;

    std::vector<std::string> values;
    u8 value;

    void (*onChange)();
} ConfigMultiChoice;

typedef struct {
    std::string name;

    std::vector<ConfigMultiChoice> multiChoices;
    std::vector<ConfigPath> paths;
} ConfigGroup;

static ConfigGroup gameYob = {
        "GameYob",
        {
#ifdef BACKEND_SWITCH
                {
                        "Pause In Menu",
                        {"Off", "On"},
                        PAUSE_IN_MENU_ON,
                        nullptr
                },
#else
                {
                        "Pause In Menu",
                        {"Off", "On"},
                        PAUSE_IN_MENU_OFF,
                        nullptr
                },
                {
                        "Console Output",
                        {"Off", "FPS", "Time", "FPS+Time", "Debug"},
                        CONSOLE_OUTPUT_OFF,
                        nullptr
                },
#endif
#ifdef BACKEND_3DS
                {
                        "Game Screen",
                        {"Top", "Bottom"},
                        GAME_SCREEN_TOP,
                        uiUpdateScreen
                },
#endif
        },
        {
                {
                        "ROM Path",
                        {},
                        "/",
                        nullptr
                }
        }
};

static ConfigGroup gameBoy = {
        "GameBoy",
        {
                {
                        "GBC Mode",
                        {"Off", "If Needed", "On"},
                        GBC_MODE_ON,
                        nullptr
                },
                {
                        "SGB Mode",
                        {"Off", "Prefer GBC", "Prefer SGB"},
                        SGB_MODE_PREFER_GBC,
                        nullptr
                },
                {
                        "GBA Mode",
                        {"Off", "On"},
                        GBA_MODE_OFF,
                        nullptr
                },
                {
                        "BIOS",
                        {"Off", "On"},
                        BIOS_ON,
                        nullptr
                },
                {
                        "GB Printer",
                        {"Off", "On"},
                        GB_PRINTER_ON,
                        nullptr
                }
        },
        {}
};

static ConfigGroup display = {
        "Display",
        {
                {
                        "Scaling Mode",
                        {"Off", "Aspect", "Aspect (Screen Only)", "Full", "Full (Screen Only)"},
                        SCALING_MODE_OFF,
                        mgrRefreshBorder
                },
                {
                        "Scaling Filter",
                        {"Nearest", "Linear", "Scale2x"},
                        SCALING_FILTER_NEAREST,
                        nullptr
                },
                {
                        "FF Frame Skip",
                        {"0", "1", "2", "3"},
                        FF_FRAME_SKIP_3,
                        nullptr
                },
                {
                        "Colorize GB",
                        {"Off", "Auto", "Inverted", "Pastel Mix", "Red", "Orange", "Yellow", "Green", "Blue", "Brown", "Dark Green", "Dark Blue", "Dark Brown", "Classic Green"},
                        COLORIZE_GB_AUTO,
                        mgrRefreshPalette
                },
                {
                        "Rendering Mode",
                        {"Scanline", "Pixel"},
                        RENDERING_MODE_SCANLINE,
                        nullptr
                },
                {
                        "Emulate Blur",
                        {"Off", "On"},
                        EMULATE_BLUR_OFF,
                        nullptr
                },
                {
                        "Custom Borders",
                        {"Off", "On"},
                        CUSTOM_BORDERS_ON,
                        mgrRefreshBorder
                },
                {
                        "Custom Borders Scaling",
                        {"Pre-Scaled", "Scale Base"},
                        CUSTOM_BORDERS_SCALING_PRE_SCALED,
                        mgrRefreshBorder
                }
        },
        {
                {
                        "Custom Border Path",
                        {"jpg", "jpeg", "png", "bmp", "psd", "tga", "gif", "hdr", "pic", "ppm", "pgm"},
                        "",
                        mgrRefreshBorder
                }
        }
};

static ConfigGroup sound = {
        "Sound",
        {
                {
                        "Master",
                        {"Off", "On"},
                        SOUND_ON,
                        nullptr
                },
                {
                        "Channel 1",
                        {"Off", "On"},
                        SOUND_ON,
                        nullptr
                },
                {
                        "Channel 2",
                        {"Off", "On"},
                        SOUND_ON,
                        nullptr
                },
                {
                        "Channel 3",
                        {"Off", "On"},
                        SOUND_ON,
                        nullptr
                },
                {
                        "Channel 4",
                        {"Off", "On"},
                        SOUND_ON,
                        nullptr
                }
        },
        {}
};

static std::vector<ConfigGroup> groups = {
        gameYob,
        gameBoy,
        display,
        sound
};

static std::vector<KeyConfig> keyConfigs;
static u32 selectedKeyConfig = 0;

static void configValidateControls() {
    if(keyConfigs.empty()) {
        keyConfigs.push_back(*inputGetDefaultKeyConfig());
    }

    if(selectedKeyConfig >= keyConfigs.size()) {
        selectedKeyConfig = (u32) (keyConfigs.size() - 1);
    }

    inputLoadKeyConfig(&keyConfigs[selectedKeyConfig]);
}

static void configLoadControlsProfile(INIReader &reader, const std::string &name) {
    if(name.empty()) {
        return;
    }

    keyConfigs.push_back(KeyConfig());
    KeyConfig& config = keyConfigs.back();

    config.name = name;

    u32 keyCount = (u32) inputGetKeyCount();
    for(u32 i = 0; i < keyCount; i++) {
        if(inputIsValidKey(i)) {
            std::string mappedKey = reader.Get(name, inputGetKeyName(i), "");

            if(!mappedKey.empty()) {
                for(u8 funcKey = 0; funcKey < NUM_FUNC_KEYS; funcKey++) {
                    if(mappedKey == funcKeyNames[funcKey]) {
                        config.funcKeys[i] = funcKey;
                        break;
                    }
                }
            } else {
                config.funcKeys[i] = FUNC_KEY_NONE;
            }
        }
    }
}

static void configLoadControls(INIReader &reader) {
    selectedKeyConfig = (u32) reader.GetInteger("controls", "selected", 0);

    std::string profiles = reader.Get("controls", "profiles", "");
    if(profiles.length() > 0) {
        size_t startPos = 0;
        size_t endPos = 0;
        while((endPos = profiles.find(',', startPos)) != std::string::npos) {
            configLoadControlsProfile(reader, profiles.substr(startPos, endPos));

            startPos = endPos + 1;
        }

        if(startPos < profiles.length()) {
            configLoadControlsProfile(reader, profiles.substr(startPos));
        }
    }

    configValidateControls();
}

static void configSaveControls(std::ofstream &stream) {
    stream << "[controls]\n";
    stream << "selected=" << selectedKeyConfig << std::endl;
    stream << "profiles=";
    for(u32 i = 0; i < keyConfigs.size(); i++) {
        stream << keyConfigs[i].name << ",";
    }

    stream << std::endl << std::endl;

    u32 keyCount = (u32) inputGetKeyCount();
    for(u32 i = 0; i < keyConfigs.size(); i++) {
        KeyConfig& keyConfig = keyConfigs[i];

        stream << "[" << keyConfig.name << "]" << std::endl;
        for(u32 j = 0; j < keyCount; j++) {
            if(inputIsValidKey(j)) {
                stream << inputGetKeyName(j) << "=" << funcKeyNames[keyConfig.funcKeys[j]] << std::endl;
            }
        }

        stream << std::endl;
    }
}

bool configLoad() {
    keyConfigs.clear();

    INIReader reader(INI_FILE);
    if(reader.ParseError() < 0) {
        configValidateControls();

        mgrPrintDebug("Failed to load settings: %s\n", strerror(errno));
        return false;
    }

    for(u8 i = 0; i < groups.size(); i++) {
        for(ConfigMultiChoice& entry : groups[i].multiChoices) {
            std::string value = reader.Get(groups[i].name, entry.name, entry.values[entry.value]);
            for(u8 j = 0; j < entry.values.size(); j++) {
                if(entry.values[j] == value) {
                    entry.value = j;
                    break;
                }
            }
        }

        for(ConfigPath& entry : groups[i].paths) {
            entry.value = reader.Get(groups[i].name, entry.name, entry.value);
        }
    }

    configLoadControls(reader);

    return true;
}

void configSave() {
    std::ofstream stream(INI_FILE);
    if(!stream.is_open()) {
        mgrPrintDebug("Failed to save settings: %s\n", strerror(errno));
        return;
    }

    for(u8 i = 0; i < groups.size(); i++) {
        stream << "[" << groups[i].name << "]" << std::endl;

        for(ConfigMultiChoice& entry : groups[i].multiChoices) {
            stream << entry.name << "=" << entry.values[entry.value] << std::endl;
        }

        for(ConfigPath& entry : groups[i].paths) {
            stream << entry.name << "=" << entry.value << std::endl;
        }

        stream << std::endl;
    }

    configSaveControls(stream);

    stream.close();
}

u8 configGetGroupCount() {
    return (u8) groups.size();
}

const std::string configGetGroupName(u8 group) {
    if(group >= groups.size()) {
        return "";
    }

    return groups[group].name;
}

u8 configGetGroupMultiChoiceCount(u8 group) {
    if(group >= groups.size()) {
        return 0;
    }

    return (u8) groups[group].multiChoices.size();
}

u8 configGetGroupPathCount(u8 group) {
    if(group >= groups.size()) {
        return 0;
    }

    return (u8) groups[group].paths.size();
}

const std::string configGetMultiChoiceName(u8 group, u8 option) {
    if(group >= groups.size()) {
        return "";
    }

    ConfigGroup& g = groups[group];
    if(option >= g.multiChoices.size()) {
        return "";
    }

    return g.multiChoices[option].name;
}

const std::string configGetMultiChoiceValueName(u8 group, u8 option, u8 value) {
    if(group >= groups.size()) {
        return "";
    }

    ConfigGroup& g = groups[group];
    if(option >= g.multiChoices.size()) {
        return "";
    }

    ConfigMultiChoice& mc = g.multiChoices[option];
    if(value >= mc.values.size()) {
        return "";
    }

    return mc.values[value];
}

u8 configGetMultiChoice(u8 group, u8 option) {
    if(group >= groups.size()) {
        return 0;
    }

    ConfigGroup& g = groups[group];
    if(option >= g.multiChoices.size()) {
        return 0;
    }

    return groups[group].multiChoices[option].value;
}

void configSetMultiChoice(u8 group, u8 option, u8 value) {
    if(group >= groups.size()) {
        return;
    }

    ConfigGroup& g = groups[group];
    if(option >= g.multiChoices.size()) {
        return;
    }

    ConfigMultiChoice& mc = g.multiChoices[option];
    if(value >= mc.values.size()) {
        return;
    }

    mc.value = value;

    if(mc.onChange != nullptr) {
        mc.onChange();
    }
}

void configToggleMultiChoice(u8 group, u8 option, s8 dir) {
    if(group >= groups.size()) {
        return;
    }

    ConfigGroup& g = groups[group];
    if(option >= g.multiChoices.size()) {
        return;
    }

    ConfigMultiChoice& mc = g.multiChoices[option];
    if(dir == -1 && mc.value == 0) {
        mc.value = (u8) (mc.values.size() - 1);
    } else {
        mc.value = (u8) ((mc.value + dir) % mc.values.size());
    }

    if(mc.onChange != nullptr) {
        mc.onChange();
    }
}

const std::string configGetPathName(u8 group, u8 option) {
    if(group >= groups.size()) {
        return "";
    }

    ConfigGroup& g = groups[group];
    if(option >= g.paths.size()) {
        return "";
    }

    return g.paths[option].name;
}

std::vector<std::string> configGetPathExtensions(u8 group, u8 option) {
    if(group >= groups.size()) {
        return {};
    }

    ConfigGroup& g = groups[group];
    if(option >= g.paths.size()) {
        return {};
    }

    return g.paths[option].extensions;
}

const std::string configGetPath(u8 group, u8 option) {
    if(group >= groups.size()) {
        return "";
    }

    ConfigGroup& g = groups[group];
    if(option >= g.paths.size()) {
        return "";
    }

    return g.paths[option].value;
}

void configSetPath(u8 group, u8 option, const std::string &value) {
    if(group >= groups.size()) {
        return;
    }

    ConfigGroup& g = groups[group];
    if(option >= g.paths.size()) {
        return;
    }

    ConfigPath& p = g.paths[option];
    p.value = value;

    if(p.onChange != nullptr) {
        p.onChange();
    }
}

u32 configGetKeyConfigCount() {
    return (u32) keyConfigs.size();
}

KeyConfig* configGetKeyConfig(u32 num) {
    return &keyConfigs[num];
}

void configAddKeyConfig(const std::string& name) {
    keyConfigs.emplace_back();
    keyConfigs.back().name = name;

    configValidateControls();
}

void configRemoveKeyConfig(u32 num) {
    keyConfigs.erase(keyConfigs.begin() + num);

    configValidateControls();
}

u32 configGetSelectedKeyConfig() {
    return selectedKeyConfig;
}

void configSetSelectedKeyConfig(u32 config) {
    selectedKeyConfig = config;

    configValidateControls();
}

const std::string configGetFuncKeyName(u8 funcKey) {
    if(funcKey >= NUM_FUNC_KEYS) {
        return "";
    }

    return funcKeyNames[funcKey];
}