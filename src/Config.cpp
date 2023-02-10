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

#include <cctype>
#include <algorithm>
#include <locale>
#include "hardconfig.h"
#include "Config.h"
#include "FileUtils.h"
#include "messages.h"
#include "ESPectrum.h"

#include "esp_spiffs.h"

string   Config::arch = "48K";
string   Config::ram_file = NO_RAM_FILE;
string   Config::romSet = "SINCLAIR";
string   Config::sna_file_list; // list of file names
string   Config::sna_name_list; // list of names (without ext, '_' -> ' ')
string   Config::tap_file_list; // list of file names
string   Config::tap_name_list; // list of names (without ext, '_' -> ' ')
bool     Config::slog_on = false;
bool     Config::aspect_16_9 = true;
uint8_t  Config::esp32rev = 0;
string   Config::kbd_layout = "US";
uint8_t  Config::lang = 0;

// erase control characters (in place)
static inline void erase_cntrl(std::string &s) {
    s.erase(std::remove_if(s.begin(), s.end(), 
            [&](char ch) 
            { return std::iscntrl(static_cast<unsigned char>(ch));}), 
            s.end());
}

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

// Read config from FS
void Config::load() {
    
    FILE *f = fopen(DISK_BOOT_FILENAME, "r");
    if (f==NULL)
    {
        // printf("Error opening %s",DISK_BOOT_FILENAME);
        Config::save(); // Try to create file if doesn't exist
        return;
    }

    char buf[256];
    while(fgets(buf, sizeof(buf), f) != NULL)
    {
        string line = buf;
        // printf(line.c_str());
        if (line.find("ram:") != string::npos) {
            ram_file = line.substr(line.find(':') + 1);
            erase_cntrl(ram_file);
            trim(ram_file);
        } else if (line.find("arch:") != string::npos) {
            arch = line.substr(line.find(':') + 1);
            erase_cntrl(arch);
            trim(arch);
        } else if (line.find("romset:") != string::npos) {
            romSet = line.substr(line.find(':') + 1);
            erase_cntrl(romSet);
            trim(romSet);
        } else if (line.find("slog:") != string::npos) {
            line = line.substr(line.find(':') + 1);
            erase_cntrl(line);
            trim(line);
            slog_on = (line == "true" ? true : false);
        } else if (line.find("asp169:") != string::npos) {
            line = line.substr(line.find(':') + 1);
            erase_cntrl(line);
            trim(line);
            aspect_16_9 = (line == "true");
        } else if (line.find("sdstorage:") != string::npos) {
            if (FileUtils::SDReady) {
                line = line.substr(line.find(':') + 1);
                erase_cntrl(line);
                trim(line);
                FileUtils::MountPoint = (line == "true" ? MOUNT_POINT_SD : MOUNT_POINT_SPIFFS);
            } else
                FileUtils::MountPoint = MOUNT_POINT_SPIFFS;
        } else if (line.find("kbdlayout:") != string::npos) {
            kbd_layout = line.substr(line.find(':') + 1);
            erase_cntrl(kbd_layout);
            trim(kbd_layout);
        } else if (line.find("language:") != string::npos) {
            string slang = line.substr(line.find(':') + 1);
            erase_cntrl(slang);
            trim(slang);
            Config::lang = stoi(slang);
        }

    }
    fclose(f);

}

void Config::loadSnapshotLists()
{
    
    sna_file_list = (string) MENU_SNA_TITLE[Config::lang] + "\n" + FileUtils::getSortedFileList(FileUtils::MountPoint + DISK_SNA_DIR);

    sna_name_list = sna_file_list;
    
//    printf((sna_name_list + "\n").c_str());

    std::string::size_type n = 0;
    while ( ( n = sna_name_list.find( ".sna", n ) ) != std::string::npos )
    {
        sna_name_list.replace( n, 4, "" );
        n++;
    }

    n = 0;
    while ( ( n = sna_name_list.find( ".SNA", n ) ) != std::string::npos )
    {
        sna_name_list.replace( n, 4, "" );
        n++;
    }

    n = 0;
    while ( ( n = sna_name_list.find( ".z80", n ) ) != std::string::npos )
    {
        sna_name_list.replace( n, 4, "" );
        n++;
    }

    n = 0;
    while ( ( n = sna_name_list.find( ".Z80", n ) ) != std::string::npos )
    {
        sna_name_list.replace( n, 4, "" );
        n++;
    }

    n = 0;
    while ( ( n = sna_name_list.find( "_", n ) ) != std::string::npos )
    {
        sna_name_list.replace( n, 1, "" );
        n++;
    }

    n = 0;
    while ( ( n = sna_name_list.find( "-", n ) ) != std::string::npos )
    {
        sna_name_list.replace( n, 1, "" );
        n++;
    }
    
}

void Config::loadTapLists()
{

    tap_file_list = (string) MENU_TAP_TITLE[Config::lang] + "\n" + FileUtils::getSortedFileList(FileUtils::MountPoint + DISK_TAP_DIR);
    tap_name_list = tap_file_list;
    
//    printf((tap_name_list + "\n").c_str());

    std::string::size_type n = 0;
    while ( ( n = tap_name_list.find( ".TAP", n ) ) != std::string::npos )
    {
        tap_name_list.replace( n, 4, "" );
        n++;
    }

    n = 0;
    while ( ( n = tap_name_list.find( ".tap", n ) ) != std::string::npos )
    {
        tap_name_list.replace( n, 4, "" );
        n++;
    }

}

// Dump actual config to FS
void Config::save() {

    //printf("Saving config file '%s':\n", DISK_BOOT_FILENAME);

    FILE *f = fopen(DISK_BOOT_FILENAME, "w");
    if (f==NULL)
    {
        printf("Error opening %s",DISK_BOOT_FILENAME);
        return;
    }

    // Architecture
    //printf(("arch:" + arch + "\n").c_str());
    fputs(("arch:" + arch + "\n").c_str(),f);

    // ROM set
    //printf(("romset:" + romSet + "\n").c_str());
    fputs(("romset:" + romSet + "\n").c_str(),f);

    // RAM SNA
    #ifdef SNAPSHOT_LOAD_LAST    
    //printf(("ram:" + ram_file + "\n").c_str());
    fputs(("ram:" + ram_file + "\n").c_str(),f);
    #endif // SNAPSHOT_LOAD_LAST

    // Serial logging
    //printf(slog_on ? "slog:true\n" : "slog:false\n");
    fputs(slog_on ? "slog:true\n" : "slog:false\n",f);

    // Aspect ratio
    //printf(aspect_16_9 ? "asp169:true\n" : "asp169:false\n");
    fputs(aspect_16_9 ? "asp169:true\n" : "asp169:false\n",f);

    // Mount point
    //printf(FileUtils::MountPoint == MOUNT_POINT_SD ? "sdstorage:true\n" : "sdstorage:false\n");
    fputs(FileUtils::MountPoint == MOUNT_POINT_SD ? "sdstorage:true\n" : "sdstorage:false\n",f);

    // KBD layout
    //printf(("kbdlayout:" + kbd_layout + "\n").c_str());
    fputs(("kbdlayout:" + kbd_layout + "\n").c_str(),f);

    // Language
    //printf("language:%s\n",std::to_string(Config::lang).c_str());
    fputs(("language:" + std::to_string(Config::lang) + "\n").c_str(),f);

    fclose(f);
    
    vTaskDelay(5);

    printf("Config saved OK\n");

}

void Config::requestMachine(string newArch, string newRomSet, bool force)
{
    if (!force && newArch == arch) {
        // printf("Config::requestMachine(newArch=%s, force=false): unchanged arch, nothing to do\n", newArch.c_str());
        return;
    }

    arch = newArch;
    romSet = newRomSet;

    ESPectrum::loadRom(arch,romSet);

}
