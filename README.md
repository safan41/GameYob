# GameYob

Fork of [Drenn](https://github.com/Drenn1/)'s (S)GB(C) emulator [GameYob](https://github.com/Drenn1/GameYob/).

Credit to [gamesquest1](http://gbatemp.net/members/gamesquest1.335456/) for the amazing 3D banner.

Download: https://github.com/Steveice10/GameYob/releases

Requires:
* devkitARM, ctrulib, and citro3d to build for 3DS.
* devkitA64 and libnx to build for Switch.
* SDL2 and libncurses to build for PC.

## General Features
* Gameboy, Gameboy Color, Super Gameboy emulation.
* Gameboy Color enhanced palettes for black-and-white games.
* Super Gameboy colorization and borders. (SGB mode only)
* Gameboy Printer emulation.
* Show the original BIOS animation.
* Fast forwarding.
* Save States.
* Remappable controls.
* Toggle individual sound channels.
* Multiple scaling modes.
  * Aspect - Fits game to screen height, including SGB border space if applicable.
  * Aspect (Screen Only) - Fits game to screen height.
  * Full - Stretches game to fill screen, including SGB border space if applicable.
  * Full (Screen Only) - Stretches game to fill screen.
* Custom border images
  * Place "(romname).(png/jpg/etc)" in the borders folder if you have one set; otherwise, place it in the same folder as the ROM.
  * Can also set a global default border in the display options.
* GameGenie (XXX-XXX or XXX-XXX-XXX) and GameShark (XXXXXXXX) Cheat Codes
  * Place "(romname).cht" in the cheats folder if you have one set; otherwise, place it in the same folder as the ROM.
  * Example code:
    ```
    [name]
    value=011092D0
    enabled=0
    ```
  * Cheats can be chained together under one name by separating individual codes with "+".
