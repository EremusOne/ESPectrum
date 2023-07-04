/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023 Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
https://github.com/EremusOne/ZX-ESPectrum-IDF

Based on ZX-ESPectrum-Wiimote
Copyright (c) 2020, 2022 David Crespo [dcrespo3d]
https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote

Based on previous work by Ramón Martinez and Jorge Fuertes
https://github.com/rampa069/ZX-ESPectrum

Original project by Pete Todd
https://github.com/retrogubbins/paseVGA

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

To Contact the dev team you can write to zxespectrum@gmail.com or 
visit https://zxespectrum.speccy.org/contacto

*/

#ifndef Config_h
#define Config_h

#include <stdio.h>
#include <inttypes.h>
#include <string>

using namespace std;

#include "esp_attr.h"

class Config
{
public:
    // machine type change request
    static void requestMachine(string newArch, string newRomSet, bool force);

    // config variables
    static const string& getArch()   { return arch;   }
    static const string& getRomSet() { return romSet; }
    static string   ram_file;
    static string   last_ram_file;
    static uint8_t  esp32rev;
    static bool     slog_on;
    static bool     aspect_16_9;
    // static string   kbd_layout;
    static uint8_t  lang;
    static bool     AY48;
    static uint8_t  joystick;
    static uint8_t  videomode;
    static uint8_t  AluTiming;

    // config persistence
    static void load();
    static void save();
    static void save(string value);

    // // list of snapshot file names
    // static string   sna_file_list;
    // // list of snapshot display names
    // static string   sna_name_list;
    // // load lists of snapshots
    // static void loadSnapshotLists();

    // // list of TAP file names
    // static string   tap_file_list;
    // // list of TAP display names
    // static string   tap_name_list;
    // // load lists of TAP files
    // static void loadTapLists();

private:
    static string   arch;
    static string   romSet;
};

#endif // Config.h