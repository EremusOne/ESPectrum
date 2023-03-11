# ZX-ESPectrum-IDF

This is an emulator of the Sinclair ZX Spectrum computer running on a Lilygo TTGo VGA32 board.

Just connect a VGA monitor, a PS/2 keyboard, a SD Card (optional) and power via microUSB.

This project is based on David Crespo excellent work on [ZX-ESPectrum-Wiimote](https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote) which is a fork of the [ZX-ESPectrum](https://github.com/rampa069/ZX-ESPectrum) project by Rampa and Queru.

## Features

- Spectrum 48K and 128K emulation (no PSRAM needed).
- Accurate Z80 emulation (Authored by [José Luis Sánchez](https://github.com/jsanchezv/z80cpp))
- VGA output (6 bpp, BRIGHT attribute kept) with good emulation of Spectrum screen.
- Support for two aspect ratios: 16:9 or 4:3 monitors (using 360x200 or 320x240 modes)
- Multicolor attribute effects emulated (Bifrost*2, Nirvana and Nirvana+ engines).
- Border effects emulated (Aquaplane, The Sentinel, Overscan demo).
- Floating bus effect emulation (Arkanoid, Sidewize).
- Contended memory and contended I/O emulation.
- Beeper sound emulation.
- Partial AY-3-8912 sound emulation.
- PS/2 keyboard used as input for Spectrum keys with all symbols mapped.
- International kbd layout support: US, ES, DE, FR and UK.
- Kempston and Cursor type Joystick emulation.
- Complete OSD menu in two languages: English & Spanish.
- Realtime TAP file loading.
- SNA and Z80 snapshot loading.
- Snapshot saving and loading (both 48K and 128K supported).
- BMP screen capture to SD Card (thanks David Crespo 😉)
- Simultaneous internal (SPIFFS) and external (SD card) storage support.

## Work in progress

- Better AY-3-8912 emulation (128K sound is still a little dirty).

## Installing

You can flash the binaries directly to the board if do not want to mess with code and compilers. Check the [releases section](https://github.com/EremusOne/ZX-ESPectrum-IDF/releases)

## Compiling and installing

Quick start from PlatformIO:
- Clone this repo and Open from VSCode/PlatFormIO
- Execute task: Upload File System Image
- Execute task: Upload
- Enjoy

Windows, GNU/Linux and MacOS/X. This version has been developed using PlatformIO.

#### Install platformIO:

- There is an extension for Atom and VSCode, please check [this website](https://platformio.org/).
- Select your board, pico32 which behaves just like the TTGo VGA32.

#### Customize platformio.ini

PlatformIO now autodetects port, so there is no need to specify it (unless autodetection fails).

#### Select your aspect ratio

Default aspect ratio is 16:9, so if your monitor has this, you don't need to change anything.

(If you change aspect ratio to 4:3 and get no image, re-flash board or re-upload file system image to roll back to default settings).

#### Upload the data filesystem

`PlatformIO > Project Tasks > Upload File System Image`

All files under the `/data` subdirectory will be copied to the SPIFFS filesystem partition. Run this task whenever you add any file to the data subdirectory (e.g, adding games in .SNA or .Z80 format, into the `/data/s` subdirectory).

#### Using a external micro SD Card and copying games into it

If using external micro sd card, you must create the following folders in root directory:

- "p" folder     -> Will be used for persist snapshots.
- "s" folder     -> Put .SNA and .Z80 files here.
- "t" folder     -> Put .TAP files here.
- "c" folder     -> For SCR (not yet!) and BMP screen captures.

The SD card should be formatted in FAT16 / FAT32.

First time the emulator access sna or tape directories, it will create and index for sorting the files in it. It may take some time if you put many archives (15-20 seconds in my tests for about 1000 files, about 3 minutes for about 6000 files). Once created, file dialogs will open fast but if you extract the card and add files, you must later use "Options/Storage/Refresh directories" to be able to view new files on the files dialogs.

#### Compile and flash it

`PlatformIO > Project Tasks > Build `, then

`PlatformIO > Project Tasks > Upload`.

Run these tasks (`Upload` also does a `Build`) whenever you make any change in the code.

## Hardware configuration and pinout

Pin assignment in `hardpins.h` is set to match the TTGo VGA32, use it as-is, or change it to your own preference. It is already set for the [TTGo version 1.4](http://www.lilygo.cn/prod_view.aspx?TypeId=50033&Id=1083&FId=t3:50033:3).

## Thanks to

- [David Crespo](https://youtube.com/Davidprograma) for his friendly help and support and his excellent work at his [Youtube Channel](https://youtube.com/Davidprograma) and the [ZX-ESPectrum-Wiimote](https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote) emulator.
- Developers of the [original project](https://github.com/rampa069/ZX-ESPectrum), [Rampa](https://github.com/rampa069) and [Queru](https://github.com/jorgefuertes).
- [Ackerman](https://github.com/rpsubc8/ESP32TinyZXSpectrum) for his code and ideas.
- Azesmbog for testing and providing very valuable info to make the emu more precise.
- VGA Driver from [ESP32Lib by BitLuni](https://github.com/bitluni/ESP32Lib).
- PS2 Driver from Fabrizio di Vittorio for his [FabGL library](https://github.com/fdivitto/FabGL).
- Z80 Emulation derived from [z80cpp](https://github.com/jsanchezv/z80cpp), authored by José Luis Sánchez.
- [Amstrad PLC](http://www.amstrad.com) for the ZX-Spectrum ROM binaries [liberated for emulation purposes](http://www.worldofspectrum.org/permits/amstrad-roms.txt).
- [Nine Tiles Networs Ltd](http://www.worldofspectrum.org/sinclairbasic/index.html) for Sinclair BASIC.
- [Retroleum](http://blog.retroleum.co.uk/electronics-articles/a-diagnostic-rom-image-for-the-zx-spectrum/) for the diagnostics ROM.
- [Jean Thomas](https://github.com/jeanthom/ESP32-APLL-cal) for his ESP32 APLL calculator, useful for getting a rebel TTGO board to work at 320x240.

## And all the involved people from the golden age

- [Sir Clive Sinclair](https://en.wikipedia.org/wiki/Clive_Sinclair).
- [Christopher Curry](https://en.wikipedia.org/wiki/Christopher_Curry).
- [The Sinclair Team](https://en.wikipedia.org/wiki/Sinclair_Research).
- [Lord Alan Michael Sugar](https://en.wikipedia.org/wiki/Alan_Sugar).
- [Investrónica team](https://es.wikipedia.org/wiki/Investr%C3%B3nica).
- [Matthew Smith](https://en.wikipedia.org/wiki/Matthew_Smith_(games_programmer)) for [Manic Miner](https://en.wikipedia.org/wiki/Manic_Miner).

## And all the writters, hobbist and documenters

- [Retrowiki](http://retrowiki.es/) especially the people at [ESP32 TTGO VGA32](http://retrowiki.es/viewforum.php?f=114) subforum.
- [El Mundo del Spectrum](http://www.elmundodelspectrum.com/)
- [Microhobby magazine](https://es.wikipedia.org/wiki/MicroHobby).
- [The World of Spectrum](http://www.worldofspectrum.org/)
- Dr. Ian Logan & Dr. Frank O'Hara for [The Complete Spectrum ROM Disassembly book](http://freestuff.grok.co.uk/rom-dis/).
- Chris Smith for the The [ZX-Spectrum ULA book](http://www.zxdesign.info/book/).

## A lot of programmers, especially

- [GreenWeb Sevilla](https://youtube.com/GreenWebSevilla) for its Fantasy Zone game.
- Julián Urbano Muñoz for [Speccy Pong](https://zx-dev-conversions.proboards.com/thread/25/speccypong).
- Others who have donated distribution rights for this project.
