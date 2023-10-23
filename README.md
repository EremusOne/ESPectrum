![ESPectrum](https://zxespectrum.speccy.org/wp-content/uploads/2023/05/ESPectrum-logo-v02-2.png)

This is an emulator of the Sinclair ZX Spectrum computer running on Espressif ESP32 SoC powered boards.

Currently, it can be used with Lilygo's TTGo VGA32 board, Antonio Villena's ESPectrum board and ESP32-SBC-FabGL board from Olimex.

Just connect a VGA monitor or CRT TV (with special VGA-RGB cable needed), a PS/2 keyboard, prepare a SD Card as needed and power via microUSB.

This project is based on David Crespo excellent work on [ZX-ESPectrum-Wiimote](https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote) which is a fork of the [ZX-ESPectrum](https://github.com/rampa069/ZX-ESPectrum) project by Rampa and Queru which was inspired by Pete's Todd [PaseVGA](https://github.com/retrogubbins/paseVGA) project.

## Features

- ZX Spectrum 48K, 128K and Pentagon 128K emulation (no PSRAM needed).
- Accurate Z80 emulation (Authored by [José Luis Sánchez](https://github.com/jsanchezv/z80cpp))
- 6 bpp VGA output in three modes: Standard VGA (60 and 70hz), VGA 50hz and CRT 15khz 50hz.
- Support for two aspect ratios: 16:9 or 4:3 monitors (using 360x200 or 320x240 modes)
- Multicolor attribute effects emulated (Bifrost*2, Nirvana and Nirvana+ engines).
- Border effects emulated (Aquaplane, The Sentinel, Overscan demo).
- Floating bus effect emulated (Arkanoid, Sidewize).
- Contended memory and contended I/O emulation.
- AY-3-8912 sound emulation.
- Beeper & Mic emulation (Cobra’s Arc).
- Dual PS/2 keyboard support: you can connect two devices using PS/2 protocol at the same time.
- Kempston and Cursor type Joystick emulation.
- Emulation of Betadisk interface with four drives and TRD (read and write) and SCL (read only) support.
- Realtime and fast TAP file loading.
- TAP file saving to SD card.
- SNA and Z80 snapshot loading.
- Snapshot saving and loading.
- Complete OSD menu in two languages: English & Spanish.
- BMP screen capture to SD Card (thanks David Crespo 😉).

## Work in progress

- Folder support.
- Customizable joystick maps.
- On screen keyboard.
- TZX support.

## Installing

You can flash the binaries directly to the board if do not want to mess with code and compilers. Check the [releases section](https://github.com/EremusOne/ZX-ESPectrum-IDF/releases)

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

The SD card should be formatted in FAT16 / FAT32 and you must create the following folders in root directory:

- "p" folder     -> Will be used for persist snapshots.
- "s" folder     -> Put .SNA and .Z80 files here.
- "d" folder     -> Put .TRD and .SCL files here.
- "t" folder     -> Put .TAP files here.
- "c" folder     -> For SCR (not yet!) and BMP screen captures.

First time the emulator access sna, tape or disk directories, it will create and index for sorting the files in it. It may take some time if you put many archives (15-20 seconds in my tests for about 1000 files, about 3 minutes for about 6000 files). Once created, file dialogs will open fast but if you extract the card and add files, you must later use "Options/Storage/Refresh directories" to be able to view new files on the files dialogs.

## PS/2 Keyboard functions

- F1 Menu
- F2 Load (SNA,Z80)
- F3 Load custom snapshot
- F4 Save customn snapshot
- F5 Select TAP file
- F6 Play/Stop tape
- F7 Tape Browser
- F8 OSD Stats ( [CPU] microsecs per CPU cycle, [IDL] idle microsecs, [FPS] Frames per second, [FND] FPS w/no delay applied )
- F9 Volume down
- F10 Volume up
- F11 Hard reset
- F12 Reset ESP32
- Pause Pause
- PrntScr BMP screen capture (Folder /c at SDCard)

## ZX Keyboard functions

Press CAPS SHIFT + SYMBOL SHIFT and:

- 1 Menu
- 2 Load (SNA,Z80)
- 3 Load custom snapshot
- 4 Save custom snapshot
- 5 Select TAP file
- 6 Play/Stop tape
- 7 Tape browser
- 8 OSD Stats ( [CPU] microsecs per CPU cycle, [IDL] idle microsecs, [FPS] Frames per second, [FND] FPS w/no delay applied )
- 9 Volume down
- 0 Volume up
- Q Hard reset
- W Reset ESP32
- P Pause
- C BMP screen capture (Folder /c at SDCard)

## Hardware configuration and pinout

Pin assignment in `hardpins.h` is set to match the boards we've tested emulator in, use it as-is, or change it to your own preference.

## Thanks to

- [David Crespo](https://youtube.com/Davidprograma) for his friendly help and support and his excellent work at his [Youtube Channel](https://youtube.com/Davidprograma) and the [ZX-ESPectrum-Wiimote](https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote) emulator.
- Pete Todd, developer of the original project [PaseVGA](https://github.com/retrogubbins/paseVGA).
- Ramón Martínez ["Rampa"](https://github.com/rampa069) and Jorge Fuertes ["Queru"](https://github.com/jorgefuertes) who improved PaseVGA in the first [ZX-ESPectrum](https://github.com/rampa069/ZX-ESPectrum).
- Z80 Emulation derived from [z80cpp](https://github.com/jsanchezv/z80cpp), authored by José Luis Sánchez.
- VGA Driver from [ESP32Lib by BitLuni](https://github.com/bitluni/ESP32Lib).
- AY-3-8912 emulation from [libayemu by Alexander Sashnov](https://asashnov.github.io/libayemu.html).
- PS2 Driver from Fabrizio di Vittorio for his [FabGL library](https://github.com/fdivitto/FabGL).
- [Ackerman](https://github.com/rpsubc8/ESP32TinyZXSpectrum) for his code and ideas.
- Azesmbog for testing and providing very valuable info to make the emu more precise.
- David Carrión for hardware and ZX keyboard code.
- ZjoyKiLer for his testing and ideas.
- [Mark Woodmass](https://specemu.zxe.io) and [Juan Carlos González Amestoy](https://www.retrovirtualmachine.org) for his excellent emulators and his help with wd1793 emulation.
- [Antonio Villena](https://antoniovillena.es/store) for creating the ESPectrum board.
- Tsvetan Usunov from [Olimex Ltd](https://www.olimex.com).
- [Amstrad PLC](http://www.amstrad.com) for the ZX-Spectrum ROM binaries [liberated for emulation purposes](http://www.worldofspectrum.org/permits/amstrad-roms.txt).
- [Jean Thomas](https://github.com/jeanthom/ESP32-APLL-cal) for his ESP32 APLL calculator.

## And all the involved people from the golden age

- [Sir Clive Sinclair](https://en.wikipedia.org/wiki/Clive_Sinclair).
- [Christopher Curry](https://en.wikipedia.org/wiki/Christopher_Curry).
- [The Sinclair Team](https://en.wikipedia.org/wiki/Sinclair_Research).
- [Lord Alan Michael Sugar](https://en.wikipedia.org/wiki/Alan_Sugar).
- [Investrónica team](https://es.wikipedia.org/wiki/Investr%C3%B3nica).
- [Matthew Smith](https://en.wikipedia.org/wiki/Matthew_Smith_(games_programmer)) for [Manic Miner](https://en.wikipedia.org/wiki/Manic_Miner).

## And all the writters, hobbist and documenters

- [Retrowiki](http://retrowiki.es/) especially the people at [ESP32 TTGO VGA32](http://retrowiki.es/viewforum.php?f=114) subforum.
- [RetroReal](https://www.youtube.com/@retroreal) for his kindness and hospitality and his great work.
- Armand López [El Viejoven FX](https://www.youtube.com/@ElViejovenFX)
- Javi Ortiz [El Spectrumero](https://www.youtube.com/@ElSpectrumeroJaviOrtiz) 
- [El Mundo del Spectrum](http://www.elmundodelspectrum.com/)
- [Microhobby magazine](https://es.wikipedia.org/wiki/MicroHobby).
- [The World of Spectrum](http://www.worldofspectrum.org/)
- Dr. Ian Logan & Dr. Frank O'Hara for [The Complete Spectrum ROM Disassembly book](http://freestuff.grok.co.uk/rom-dis/).
- Chris Smith for the The [ZX-Spectrum ULA book](http://www.zxdesign.info/book/).
