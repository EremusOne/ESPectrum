///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum - ZX Spectrum emulator for ESP32
//
// Copyright (c) 2020, 2021 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez, Jorge Fuertes and many others
// https://github.com/rampa069/ZX-ESPectrum
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

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
    static uint8_t  esp32rev;
    static bool     slog_on;
    static bool     aspect_16_9;
    static string   kbd_layout;
    static uint8_t  lang;

    // config persistence
    static void           load();
    static void IRAM_ATTR save();

    // list of snapshot file names
    static string   sna_file_list;
    // list of snapshot display names
    static string   sna_name_list;
    // load lists of snapshots
    static void loadSnapshotLists();

    // list of TAP file names
    static string   tap_file_list;
    // list of TAP display names
    static string   tap_name_list;
    // load lists of TAP files
    static void loadTapLists();

private:
    static string   arch;
    static string   romSet;
};

#endif // Config.h