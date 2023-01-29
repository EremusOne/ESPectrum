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

#include "hardconfig.h"
#include "Config.h"
// #include <FS.h>
// #include "PS2Kbd.h"
#include "FileUtils.h"
#include "messages.h"
#include "ESPectrum.h"


#ifdef USE_INT_FLASH
// using internal storage (spi flash)
#include "esp_spiffs.h"
// set The Filesystem to SPIFFS
//#define THE_FS SPIFFS
#endif

#ifdef USE_SD_CARD
// using external storage (SD card)
#include <SD.h>
// set The Filesystem to SD
#define THE_FS SD
static SPIClass customSPI;
#endif

string   Config::arch = "128K";
string   Config::ram_file = NO_RAM_FILE;
string   Config::romSet = "SINCLAIR";
string   Config::sna_file_list; // list of file names
string   Config::sna_name_list; // list of names (without ext, '_' -> ' ')
string   Config::tap_file_list; // list of file names
string   Config::tap_name_list; // list of names (without ext, '_' -> ' ')
bool     Config::slog_on = true;
bool     Config::aspect_16_9 = false;
uint8_t  Config::esp32rev = 0;

// Read config from FS
void Config::load() {
    
    FILE *f = fopen(DISK_BOOT_FILENAME, "r");
    if (f==NULL)
    {
        // printf("Error opening %s",DISK_BOOT_FILENAME);
        return;
    }
    // f = FileUtils::safeOpenFileRead(DISK_BOOT_FILENAME);

    char buf[256];
    while(fgets(buf, sizeof(buf), f) != NULL)
    {
        string line = buf;
        if (line.find("ram:") != string::npos) {
            ram_file = line.substr(line.find(':') + 1);
            ram_file.pop_back();
        } else if (line.find("arch:") != string::npos) {
            arch = line.substr(line.find(':') + 1);
            arch.pop_back();
        } else if (line.find("romset:") != string::npos) {
            romSet = line.substr(line.find(':') + 1);
            romSet.pop_back();
        } else if (line.find("slog:") != string::npos) {
            line = line.substr(line.find(':') + 1);
            line.pop_back();
            slog_on = (line == "true");
        } else if (line.find("asp169:") != string::npos) {
            line = line.substr(line.find(':') + 1);
            line.pop_back();
            aspect_16_9 = (line == "true");
        }
    }
    fclose(f);
}

// void Config::loadSnapshotLists()
// {
//     KB_INT_STOP;
//     sna_file_list = (String)MENU_SNA_TITLE + "\n" + FileUtils::getSortedFileList(DISK_SNA_DIR);
//     sna_name_list = String(sna_file_list);
//     sna_name_list.replace(".SNA", "");
//     sna_name_list.replace(".sna", "");
//     sna_name_list.replace(".Z80", "");
//     sna_name_list.replace(".z80", "");
//     sna_name_list.replace("_", " ");
//     sna_name_list.replace("-", " ");
//     KB_INT_START;
// }

// void Config::loadTapLists()
// {
//     KB_INT_STOP;
//     tap_file_list = (String)MENU_TAP_TITLE + "\n" + FileUtils::getSortedFileList(DISK_TAP_DIR);
//     tap_name_list = String(tap_file_list);
//     tap_name_list.replace(".TAP", "");
//     tap_name_list.replace(".tap", "");
//     KB_INT_START;
// }

// // Dump actual config to FS
// void Config::save() {
//     KB_INT_STOP;
//     Serial.printf("Saving config file '%s':\n", DISK_BOOT_FILENAME);
//     File f = THE_FS.open(DISK_BOOT_FILENAME, FILE_WRITE);
//     // Architecture
//     Serial.printf("  + arch:%s\n", arch.c_str());
//     f.printf("arch:%s\n", arch.c_str());
//     // ROM set
//     Serial.printf("  + romset:%s\n", romSet.c_str());
//     f.printf("romset:%s\n", romSet.c_str());
//     // RAM SNA
// #ifdef SNAPSHOT_LOAD_LAST    
//     Serial.printf("  + ram:%s\n", ram_file.c_str());
//     f.printf("ram:%s\n", ram_file.c_str());
// #endif // SNAPSHOT_LOAD_LAST
//     // Serial logging
//     Serial.printf("  + slog:%s\n", (slog_on ? "true" : "false"));
//     f.printf("slog:%s\n", (slog_on ? "true" : "false"));
//     // Serial logging
//     Serial.printf("  + asp169:%s\n", (aspect_16_9 ? "true" : "false"));
//     f.printf("asp169:%s\n", (aspect_16_9 ? "true" : "false"));

//     f.close();
//     vTaskDelay(5);
//     Serial.println("Config saved OK");
//     KB_INT_START;

//     // do not reload after saving
//     // load();
//     // loadSnapshotLists();
// }

void Config::requestMachine(string newArch, string newRomSet, bool force)
{
    if (!force && newArch == arch) {
        // printf("Config::requestMachine(newArch=%s, force=false): unchanged arch, nothing to do\n", newArch.c_str());
        return;
    }

    arch = newArch;
    romSet = newRomSet;

    FileUtils::loadRom(arch, romSet);

}
