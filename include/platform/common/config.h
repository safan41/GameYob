#pragma once

#include "types.h"

#include <vector>

/* Groups */

#define GROUP_GAMEYOB 0
#define GROUP_GAMEBOY 1
#define GROUP_DISPLAY 2
#define GROUP_SOUND 3


/* GameYob */

#define GAMEYOB_ROM_PATH 0

#define GAMEYOB_CONSOLE_OUTPUT 0
#define GAMEYOB_PAUSE_IN_MENU 1

#define CONSOLE_OUTPUT_OFF 0
#define CONSOLE_OUTPUT_FPS 1
#define CONSOLE_OUTPUT_TIME 2
#define CONSOLE_OUTPUT_FPS_TIME 3
#define CONSOLE_OUTPUT_DEBUG 4

#define PAUSE_IN_MENU_OFF 0
#define PAUSE_IN_MENU_ON 1

/* GameBoy */

#define GAMEBOY_GBC_MODE 0
#define GAMEBOY_SGB_MODE 1
#define GAMEBOY_GBA_MODE 2
#define GAMEBOY_BIOS 3
#define GAMEBOY_GB_PRINTER 4

#define GB_PRINTER_OFF 0
#define GB_PRINTER_ON 1

#define GBA_MODE_OFF 0
#define GBA_MODE_ON 1

#define GBC_MODE_OFF 0
#define GBC_MODE_IF_NEEDED 1
#define GBC_MODE_ON 2

#define SGB_MODE_OFF 0
#define SGB_MODE_PREFER_GBC 1
#define SGB_MODE_PREFER_SGB 2

#define BIOS_OFF 0
#define BIOS_ON 1

/* Display */

#define DISPLAY_CUSTOM_BORDER_PATH 0

#define DISPLAY_GAME_SCREEN 0
#define DISPLAY_SCALING_MODE 1
#define DISPLAY_SCALING_FILTER 2
#define DISPLAY_FF_FRAME_SKIP 3
#define DISPLAY_COLORIZE_GB 4
#define DISPLAY_RENDERING_MODE 5
#define DISPLAY_EMULATE_BLUR 6
#define DISPLAY_CUSTOM_BORDERS 7
#define DISPLAY_CUSTOM_BORDERS_SCALING 8

#define GAME_SCREEN_TOP 0
#define GAME_SCREEN_BOTTOM 1

#define SCALING_MODE_OFF 0
#define SCALING_MODE_125 1
#define SCALING_MODE_150 2
#define SCALING_MODE_ASPECT 3
#define SCALING_MODE_FULL 4

#define SCALING_FILTER_NEAREST 0
#define SCALING_FILTER_LINEAR 1
#define SCALING_FILTER_SCALE2X 2

#define FF_FRAME_SKIP_0 0
#define FF_FRAME_SKIP_1 1
#define FF_FRAME_SKIP_2 2
#define FF_FRAME_SKIP_3 3

#define COLORIZE_GB_OFF 0
#define COLORIZE_GB_AUTO 1
#define COLORIZE_GB_INVERTED 2
#define COLORIZE_GB_PASTEL_MIX 3
#define COLORIZE_GB_RED 4
#define COLORIZE_GB_ORANGE 5
#define COLORIZE_GB_YELLOW 6
#define COLORIZE_GB_GREEN 7
#define COLORIZE_GB_BLUE 8
#define COLORIZE_GB_BROWN 9
#define COLORIZE_GB_DARK_GREEN 10
#define COLORIZE_GB_DARK_BLUE 11
#define COLORIZE_GB_DARK_BROWN 12
#define COLORIZE_GB_CLASSIC_GREEN 13

#define RENDERING_MODE_SCANLINE 0
#define RENDERING_MODE_PIXEL 1

#define EMULATE_BLUR_OFF 0
#define EMULATE_BLUR_ON 1

#define CUSTOM_BORDERS_OFF 0
#define CUSTOM_BORDERS_ON 1

#define CUSTOM_BORDERS_SCALING_PRE_SCALED 0
#define CUSTOM_BORDERS_SCALING_SCALE_BASE 1

/* Sound */

#define SOUND_MASTER 0
#define SOUND_CHANNEL_1 1
#define SOUND_CHANNEL_2 2
#define SOUND_CHANNEL_3 3
#define SOUND_CHANNEL_4 4

#define SOUND_OFF 0
#define SOUND_ON 1

struct KeyConfig {
    std::string name;
    u8 funcKeys[512];
};

bool configLoad();
void configSave();

u8 configGetGroupCount();

const std::string configGetGroupName(u8 group);
u8 configGetGroupMultiChoiceCount(u8 group);
u8 configGetGroupPathCount(u8 group);

const std::string configGetMultiChoiceName(u8 group, u8 option);
const std::string configGetMultiChoiceValueName(u8 group, u8 option, u8 value);
u8 configGetMultiChoice(u8 group, u8 option);
void configSetMultiChoice(u8 group, u8 option, u8 value);
void configToggleMultiChoice(u8 group, u8 option, s8 dir);

const std::string configGetPathName(u8 group, u8 option);
std::vector<std::string> configGetPathExtensions(u8 group, u8 option);
const std::string configGetPath(u8 group, u8 option);
void configSetPath(u8 group, u8 option, const std::string& value);

u32 configGetKeyConfigCount();
KeyConfig* configGetKeyConfig(u32 num);
void configAddKeyConfig(const std::string& name);
void configRemoveKeyConfig(u32 num);

u32 configGetSelectedKeyConfig();
void configSetSelectedKeyConfig(u32 config);

const std::string configGetFuncKeyName(u8 funcKey);