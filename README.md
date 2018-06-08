# GameYob

Fork of [Drenn](https://github.com/Drenn1/)'s (S)GB(C) emulator [GameYob](https://github.com/Drenn1/GameYob/).

Credit to [gamesquest1](http://gbatemp.net/members/gamesquest1.335456/) for the amazing 3D banner.

Download: https://github.com/Steveice10/GameYob/releases

Requires devkitARM, ctrulib, and citro3d to build for 3DS.
Requires devkitA64 and libnx to build for Switch.
Requires SDL2 and libncurses to build for PC.

## General Features
* Gameboy, Gameboy Color, Super Gameboy emulation.
* Gameboy Color enhanced palettes for black-and-white games.
* Super Gameboy colorization and borders. (SGB mode only)
* Gameboy Printer emulation.
* Show the original BIOS animation.
* Fast forwarding.
* Scale to fit the screen height or fill the screen entirely.
* Toggle individual sound channels.
* Save States.
* Remappable controls.
* Custom border images
  * Place "(romname).(png/jpg/etc)" in the borders folder if you have one set; otherwise, place it in the same folder as the ROM.
  * Can also set a global default border in the display options.
* Cheat Codes
  * Place "(romname).cht" in the cheats folder if you have one set; otherwise, place it in the same folder as the ROM.
  * Example code:
    ```
    [name]
    value=011092D0
    enabled=0
    ```
