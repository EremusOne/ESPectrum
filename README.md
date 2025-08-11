![ESPectrum](https://zxespectrum.speccy.org/wp-content/uploads/2023/05/ESPectrum-logo-v02-2.png)

This is an emulator of the Sinclair ZX Spectrum computer running on Espressif ESP32 SoC powered boards.

Currently, it can be used with many different boards being Lilygo's TTGo VGA32 board one of the best starting choices for his wonderful cost-features ratio. There are also some more boards out there that can run ESPectrum out-of-the-box like the ESP32-SBC-FabGL board from Olimex.

Just connect a VGA monitor or CRT TV (with special VGA-RGB cable needed), a PS/2 keyboard, prepare a SD Card as needed and power via microUSB.

This project is based on David Crespo excellent work on [ZX-ESPectrum-Wiimote](https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote) which is a fork of the [ZX-ESPectrum](https://github.com/rampa069/ZX-ESPectrum) project by Rampa and Queru which was inspired by Pete's Todd [PaseVGA](https://github.com/retrogubbins/paseVGA) project.

## Features

- ZX Spectrum 48K, 128K, +2A and Pentagon 128K 100% cycle accurate emulation (no PSRAM needed).
- Microdigital TK90X and TK95 models (w/Microdigital ULA at 50 and 60hz) emulation.
- State of the art Z80 emulation (Authored by [José Luis Sánchez](https://github.com/jsanchezv/z80cpp))
- Selectable Sinclair 48K, Sinclair 128K and Amstrad +2 english and spanish ROMs.
- Selectable TK90X v1, v2 and v3 (Rodolfo Guerra) ROMs.
- Possibility of using one 48K, one 128K and one TK90X custom ROM with easy flashing procedure from SD card.
- ZX81+ IF2 ROM by courtesy Paul Farrow with .P file loading from SD card.
- 6 bpp VGA output in three modes: Standard VGA (60 and 70hz), VGA with every machine real vertical frequency and CRT 15khz with real vert. freq. also.
- VGA fake scanlines effect.
- Support for two aspect ratios: 16:9 or 4:3 monitors (using 360x200 or 320x240 modes)
- Complete overscan supported in CRT 15Khz mode (at 352x272 resolution for 50hz machines and 352x224 for 60hz ones).
- Multicolor attribute effects emulated (Bifrost*2, Nirvana and Nirvana+ engines).
- Border effects emulated (Aquaplane, The Sentinel, Overscan demo).
- Floating bus effect emulated (Arkanoid, Sidewize).
- Snow effect accurate emulation (as [described](https://spectrumcomputing.co.uk/forums/viewtopic.php?t=8240) by Weiv and MartianGirl).
- Contended memory and contended I/O emulation.
- AY-3-8912 sound emulation.
- Beeper & Mic emulation (Cobra’s Arc).
- Dual PS/2 keyboard support: you can connect two devices using PS/2 protocol at the same time.
- PS/2 Joystick emulation (Cursor, Sinclair, Kempston and Fuller).
- Two real joysticks support (Up to 8 button joysticks) using DB9 to PS/2 converter.
- Kempston mouse emulation through PS/2 mouse.
- Emulation of Betadisk interface with reset to tr-dos option, four drives and TRD (read and write) and SCL (read only) support.
- Realtime (with OSD) TZX and TAP file loading.
- Flashload of TAP files.
- Rodolfo Guerra's ROMs fast load routines support with on the fly standard speed blocks translation.
- Virtual tape system with support for SAVE command and block renaming, deleting and moving.
- SNA, Z80 and SP snapshot loading.
- Real audio loading support through different GPIOs or specific board connectors.
- Complete own snapshot system for saving and loading your games with quick save and autonumbering.
- Complete file navigation system with autoindexing, folder support, search functions and file and directoty management functions.
- Complete OSD menu in three languages: English, Spanish and Portuguese.
- BMP screen capture to SD Card (thanks David Crespo 😉).

## Installing

The easiest method is using our [online web flasher](https://zxespectrum.speccy.org/flash/)

You can flash also the binaries directly to the board. Check the [releases section](https://github.com/EremusOne/ESPectrum/releases) for bin files and detailed instructions

## Compiling and installing

Quick start from PlatformIO:
- Clone this repo and Open from VSCode/PlatFormIO
- Execute task: Upload
- Enjoy

Windows, GNU/Linux and MacOS/X. This version has been developed using PlatformIO.

#### Install platformIO:

- There is an extension for Atom and VSCode, please check [this website](https://platformio.org/).
- Select your board, pico32 which behaves just like the TTGo VGA32.

#### Compile and flash it

`PlatformIO > Project Tasks > Build `, then

`PlatformIO > Project Tasks > Upload`.

Run these tasks (`Upload` also does a `Build`) whenever you make any change in the code.

#### Prepare micro SD Card

The SD card should be formatted in FAT16 / FAT32.

Just that: then put your .sna, .z80, .p, .tap, .trd and .scl whenever you like and create and use folders as you need.

There's also no need to sort files using external utilities: the emulator creates and updates indexes to sort files in folders by itself.

## PS/2 Keyboard functions

- F1 Main menu
- F2 Snapshot file browser (SNA,Z80, SP, P)
- F3 Disk file browser (TRD,SCL)
- F4 Tape file browser (TAP, TZX)
- F5 Play/Stop tape
- F6 ESP (own snapshot format) file browser
- F7 ESP Quick save
- F8 CPU / Tape load stats ( [CPU] microsecs per CPU cycle, [IDL] idle microsecs, [FPS] Frames per second, [FND] FPS w/no delay applied )
- F9 Volume down
- F10 Volume up
- F11 Hard reset
- F12 Reset ESP32
- CTRL + F1 Show current machine keyboard layout
- CTRL + F2 Cycle turbo modes -> 100% speed (blue OSD), 125% speed (red OSD), 150% speed (magenta OSD) and MAX speed (black speed and no sound)
- CTRL + F3 Second PS/2 port (if present) device selection
- CTRL + F4 Select mouse DPI
- CTRL + F5..F8 Screen centering in CRT 15K/50hz mode (select and restart ESP32 for changes to be applied)
- CTRL + F9 Input poke
- CTRL + F10 NMI
- CTRL + F11 Reset to TR-DOS
- SHIFT + F1 Hardware info
- SHIFT + F2 Real audio-in toggle
- SHIFT + F3 Eject disk
- SHIFT + F4 Tape blocks browser
- SHIFT + F5 Eject tape
- Pause Pause
- ScrollLock Switch "Cursor keys as joy" setting
- PrntScr BMP screen capture (Folder /.c at SDCard)

## ZX Keyboard functions

Press CAPS SHIFT + SYMBOL SHIFT and:

- 1 Main menu
- 2 Snapshot file browser (SNA,Z80, SP, P)
- 3 Disk file browser (TRD,SCL)
- 4 Tape file browser (TAP, TZX)
- 5 Play/Stop tape
- 6 ESP (own snapshot format) file browser
- 7 ESP Quick save
- 8 CPU / Tape load stats ( [CPU] microsecs per CPU cycle, [IDL] idle microsecs, [FPS] Frames per second, [FND] FPS w/no delay applied )
- 9 Volume down
- 0 Volume up
- Q Hard reset
- W Reset ESP32
- E Eject tape
- R Reset to TR-DOS
- T Tape blocks browser
- I Hardware info
- O Input poke
- P Pause
- S Second PS/2 port (if present) device selection
- D Select mouse DPI
- F Cycle turbo modes -> 100% speed (blue OSD), 125% speed (red OSD), 150% speed (magenta OSD) and MAX speed (black speed and no sound)
- K Show current machine keyboard layout
- L Real audio-in toggle
- Z,X,C,V Screen centering in CRT 15K mode
- B BMP screen capture (Folder /.c at SDCard)
- N NMI

## How to flash custom ROMs

Three custom ROMs can be installed: one for the 48K architecture, another for the 128K architecture and another one for TK90X.

The "Update firmware" option is now changed to the "Update" menu with four options: firmware, custom ROM 48K, custom ROM 128K and custom ROM TK90X.

You can put the ROM files (.rom extension) anywhere and select it with the browser.

For the 48K and TK90X architectures, the ROM file size must be 16384 bytes.

For the 128K architecture, it can be either 16kb or 32kb. If it's 16kb, the second bank of the custom ROM will be flashed with the second bank of the standard Sinclair 128K ROM.

It is important to note that for custom ROMs, fast loading of taps can be used, but the loading should be started manually, considering the possibility that the "traps" of the ROM loading routine might not work depending on the flashed ROM. For example, with Rodolfo Guerra's ROMs, both loading and recording traps using the SAVE command work perfectly.

Finally, keep in mind that when updating the firmware, you will need to re-flash the custom ROMs afterward, so I recommend leaving ROM files you wish to use on the SD card.

## Hardware configuration and pinout

Pin assignment in `hardpins.h` is set to match the boards we've tested emulator in, use it as-is, or change it to your own preference.

## Project links

- [Website](https://zxespectrum.speccy.org)
- [Patreon](https://www.patreon.com/ESPectrum)
- [Youtube Channel](https://www.youtube.com/@ZXESPectrum)
- [Twitter](https://twitter.com/ZX_ESPectrum)
- [Telegram](https://t.me/ZXESPectrum)

## Supported hardware

- [Lilygo FabGL VGA32](https://www.lilygo.cc/products/fabgl-vga32?_pos=1&_sid=b28e8cac0&_ss=r)
- [ESP32-SBC-FabGL board from Olimex](https://www.olimex.com/Products/Retro-Computers/ESP32-SBC-FabGL/open-source-hardware)

## Thanks to

- [David Crespo](https://youtube.com/Davidprograma) for his friendly help and support and his excellent work at his [Youtube Channel](https://youtube.com/Davidprograma) and the [ZX-ESPectrum-Wiimote](https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote) emulator.
- Pete Todd, developer of the original project [PaseVGA](https://github.com/retrogubbins/paseVGA).
- Ramón Martínez ["Rampa"](https://github.com/rampa069) and Jorge Fuertes ["Queru"](https://github.com/jorgefuertes) who improved PaseVGA in the first [ZX-ESPectrum](https://github.com/rampa069/ZX-ESPectrum).
- Z80 Emulation derived from [z80cpp](https://github.com/jsanchezv/z80cpp), authored by José Luis Sánchez.
- VGA Driver from [ESP32Lib by BitLuni](https://github.com/bitluni/ESP32Lib).
- AY-3-8912 emulation from [libayemu by Alexander Sashnov](https://asashnov.github.io/libayemu.html).
- PS2 Driver from Fabrizio di Vittorio for his [FabGL library](https://github.com/fdivitto/FabGL).
- [Paul Farrow](http://www.fruitcake.plus.com/index.html) for his kind permission to include his amazing ZX81+ IF2 ROM.
- Azesmbog for testing and providing very valuable info to make the emu more precise.
- David Carrión for hardware and ZX keyboard code and creating the [DB9_2_Keyboard](https://github.com/dacarsoft/DB9_2_Keyboard) firmware.
- ZjoyKiLer for his testing, code and ideas.
- [Ackerman](https://github.com/rpsubc8/ESP32TinyZXSpectrum) for his code and ideas.
- [Mark Woodmass](https://specemu.zxe.io) and [Juan Carlos González Amestoy](https://www.retrovirtualmachine.org) for his excellent emulators and his help with wd1793 emulation and many other things.
- Magnus Krook for [SoftSpectrum 48](https://softspectrum48.weebly.com), an excellent emulator which together with his complete and well documented website was of great help and inspiration.
- [Rodolfo Guerra](https://sites.google.com/view/rodolfoguerra) for his wonderful enhanced ROMs and his help for adding tape load turbo mode support to the emulator.
- [Juanjo Ponteprino](https://github.com/SplinterGU) for his help and contributions to ESPectrum
- Weiv and [MartianGirl](https://github.com/MartianGirl) for his detailed analysis of Snow effect.
- Tsvetan Usunov from [Olimex Ltd](https://www.olimex.com).
- [Amstrad PLC](http://www.amstrad.com) for the ZX-Spectrum ROM binaries [liberated for emulation purposes](http://www.worldofspectrum.org/permits/amstrad-roms.txt).
- [Jean Thomas](https://github.com/jeanthom/ESP32-APLL-cal) for his ESP32 APLL calculator.

## Thanks also to all this writters, hobbyists and documenters

- [Retrowiki](http://retrowiki.es/) especially the people at [ESP32 TTGO VGA32](http://retrowiki.es/viewforum.php?f=114) subforum.
- [RetroReal](https://www.youtube.com/@retroreal) for his kindness and hospitality and his great work.
- Rodrigo Méndez [Ron](https://www.twitch.tv/retrocrypta)
- Armand López [El Viejoven FX](https://www.youtube.com/@ElViejovenFX)
- Javi Ortiz [El Spectrumero](https://www.youtube.com/@ElSpectrumeroJaviOrtiz)
- José Luis Rodríguez [VidaExtraRetro](https://www.twitch.tv/vidaextraretro)
- [El Mundo del Spectrum](http://www.elmundodelspectrum.com/)
- [Microhobby magazine](https://es.wikipedia.org/wiki/MicroHobby).
- [The World of Spectrum](http://www.worldofspectrum.org/)
- Dr. Ian Logan & Dr. Frank O'Hara for [The Complete Spectrum ROM Disassembly book](http://freestuff.grok.co.uk/rom-dis/).
- Chris Smith for the The [ZX-Spectrum ULA book](http://www.zxdesign.info/book/).

## And all the involved people from the golden age

- [Sir Clive Sinclair](https://en.wikipedia.org/wiki/Clive_Sinclair).
- [Christopher Curry](https://en.wikipedia.org/wiki/Christopher_Curry).
- [The Sinclair Team](https://en.wikipedia.org/wiki/Sinclair_Research).
- [Lord Alan Michael Sugar](https://en.wikipedia.org/wiki/Alan_Sugar).
- [Investrónica team](https://es.wikipedia.org/wiki/Investr%C3%B3nica).
- [Matthew Smith](https://en.wikipedia.org/wiki/Matthew_Smith_(games_programmer)) for [Manic Miner](https://en.wikipedia.org/wiki/Manic_Miner).
