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

#include <stdio.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <cctype>
#include <algorithm>
#include <locale>
#include "hardconfig.h"
#include "Config.h"
#include "FileUtils.h"
#include "messages.h"
#include "ESPectrum.h"
#include "pwm_audio.h"
#include "roms.h"

const string Config::archnames[3] = { "48K", "128K", "Pentagon"};
string   Config::arch = "48K";
string   Config::ram_file = NO_RAM_FILE;
string   Config::last_ram_file = NO_RAM_FILE;
string   Config::romSet = "SINCLAIR";
bool     Config::slog_on = false;
bool     Config::aspect_16_9 = false;
uint8_t  Config::videomode = 0; // 0 -> SAFE VGA, 1 -> 50HZ VGA, 2 -> 50HZ CRT
uint8_t  Config::esp32rev = 0;
uint8_t  Config::lang = 0;
bool     Config::AY48 = true;
bool     Config::Issue2 = true;
bool     Config::flashload = true;
uint8_t  Config::joystick = 1; // 0 -> Cursor, 1 -> Kempston
uint8_t  Config::AluTiming = 0;
uint8_t  Config::ps2_dev2 = 0; // Second port PS/2 device: 0 -> None, 1 -> PS/2 keyboard, 2 -> PS/2 Mouse (TO DO)
bool     Config::CursorAsJoy = false;
int8_t   Config::CenterH = 0;
int8_t   Config::CenterV = 0;

string   Config::SNA_Path = "/";
string   Config::TAP_Path = "/";
string   Config::DSK_Path = "/";

uint16_t Config::SNA_begin_row = 1;
uint16_t Config::SNA_focus = 1;
uint8_t  Config::SNA_fdMode = 0;
string   Config::SNA_fileSearch = "";

uint16_t Config::TAP_begin_row = 1;
uint16_t Config::TAP_focus = 1;
uint8_t  Config::TAP_fdMode = 0;
string   Config::TAP_fileSearch = "";

uint16_t Config::DSK_begin_row = 1;
uint16_t Config::DSK_focus = 1;
uint8_t  Config::DSK_fdMode = 0;
string   Config::DSK_fileSearch = "";

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

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    // printf("\n");
    // printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        // printf("Done\n");

        size_t required_size;
        char* str_data;
        
        err = nvs_get_str(handle, "arch", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "arch", str_data, &required_size);
            // printf("arch:%s\n",str_data);
            arch = str_data;
            
            // FORCE MODEL FOR TESTING
            // arch = "48K";
            
            free(str_data);
        } else {
            // No nvs data found. Save it
            nvs_close(handle);
            Config::save();
            return;
        }

        err = nvs_get_str(handle, "romSet", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "romSet", str_data, &required_size);
            // printf("romSet:%s\n",str_data);
            romSet = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "ram", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "ram", str_data, &required_size);
            // printf("ram:%s\n",str_data);
            ram_file = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "slog", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "slog", str_data, &required_size);
            // printf("slog:%s\n",str_data);
            slog_on = strcmp(str_data, "false");            
            free(str_data);

            // slog_on = true; // Force for testing

        }

        err = nvs_get_str(handle, "sdstorage", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "sdstorage", str_data, &required_size);
            // printf("sdstorage:%s\n",str_data);

            // Force SD from now on
            FileUtils::MountPoint = MOUNT_POINT_SD;

            free(str_data);
        }

        err = nvs_get_str(handle, "asp169", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "asp169", str_data, &required_size);
            // printf("asp169:%s\n",str_data);
            aspect_16_9 = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_u8(handle, "videomode", &Config::videomode);
        if (err == ESP_OK) {
            // printf("videomode:%u\n",Config::videomode);
        }


        err = nvs_get_u8(handle, "language", &Config::lang);
        if (err == ESP_OK) {
            // printf("language:%u\n",Config::lang);
        }

        err = nvs_get_str(handle, "AY48", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "AY48", str_data, &required_size);
            // printf("AY48:%s\n",str_data);
            AY48 = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_str(handle, "Issue2", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "Issue2", str_data, &required_size);
            // printf("Issue2:%s\n",str_data);
            Issue2 = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_str(handle, "flashload", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "flashload", str_data, &required_size);
            // printf("Flashload:%s\n",str_data);
            flashload = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_u8(handle, "joystick", &Config::joystick);
        if (err == ESP_OK) {
            // printf("joystick:%u\n",Config::joystick);
        }

        err = nvs_get_u8(handle, "AluTiming", &Config::AluTiming);
        if (err == ESP_OK) {
            // printf("AluTiming:%u\n",Config::AluTiming);
        }

        err = nvs_get_u8(handle, "PS2Dev2", &Config::ps2_dev2);
        if (err == ESP_OK) {
            // printf("PS2Dev2:%u\n",Config::ps2_dev2);
        }

        err = nvs_get_str(handle, "CursorAsJoy", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "CursorAsJoy", str_data, &required_size);
            // printf("CursorAsJoy:%s\n",str_data);
            CursorAsJoy = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_i8(handle, "CenterH", &Config::CenterH);
        if (err == ESP_OK) {
            // printf("PS2Dev2:%u\n",Config::ps2_dev2);
        }

        err = nvs_get_i8(handle, "CenterV", &Config::CenterV);
        if (err == ESP_OK) {
            // printf("PS2Dev2:%u\n",Config::ps2_dev2);
        }

        err = nvs_get_str(handle, "SNA_Path", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "SNA_Path", str_data, &required_size);
            // printf("SNA_Path:%s\n",str_data);
            SNA_Path = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "TAP_Path", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "TAP_Path", str_data, &required_size);
            // printf("TAP_Path:%s\n",str_data);
            TAP_Path = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "DSK_Path", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "DSK_Path", str_data, &required_size);
            // printf("DSK_Path:%s\n",str_data);
            DSK_Path = str_data;
            free(str_data);
        }

        err = nvs_get_u16(handle, "SNA_begin_row", &Config::SNA_begin_row);
        if (err == ESP_OK) {
            // printf("SNA_begin_row:%u\n",Config::SNA_begin_row);
        }

        err = nvs_get_u16(handle, "TAP_begin_row", &Config::TAP_begin_row);
        if (err == ESP_OK) {
            // printf("TAP_begin_row:%u\n",Config::TAP_begin_row);
        }

        err = nvs_get_u16(handle, "DSK_begin_row", &Config::DSK_begin_row);
        if (err == ESP_OK) {
            // printf("begin_row:%u\n",Config::DSK_begin_row);
        }

        err = nvs_get_u16(handle, "SNA_focus", &Config::SNA_focus);
        if (err == ESP_OK) {
            // printf("SNA_focus:%u\n",Config::SNA_focus);
        }

        err = nvs_get_u16(handle, "TAP_focus", &Config::TAP_focus);
        if (err == ESP_OK) {
            // printf("TAP_focus:%u\n",Config::TAP_focus);
        }

        err = nvs_get_u16(handle, "DSK_focus", &Config::DSK_focus);
        if (err == ESP_OK) {
            // printf("DSK_focus:%u\n",Config::DSK_focus);
        }

        err = nvs_get_u8(handle, "SNA_fdMode", &Config::SNA_fdMode);
        if (err == ESP_OK) {
            // printf("SNA_fdMode:%u\n",Config::SNA_fdMode);
        }

        err = nvs_get_u8(handle, "TAP_fdMode", &Config::TAP_fdMode);
        if (err == ESP_OK) {
            // printf("TAP_fdMode:%u\n",Config::TAP_fdMode);
        }

        err = nvs_get_u8(handle, "DSK_fdMode", &Config::DSK_fdMode);
        if (err == ESP_OK) {
            // printf("DSK_fdMode:%u\n",Config::DSK_fdMode);
        }

        err = nvs_get_str(handle, "SNA_fileSearch", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "SNA_fileSearch", str_data, &required_size);
            // printf("SNA_fileSearch:%s\n",str_data);
            SNA_fileSearch = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "TAP_fileSearch", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "TAP_fileSearch", str_data, &required_size);
            // printf("TAP_fileSearch:%s\n",str_data);
            TAP_fileSearch = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "DSK_fileSearch", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "DSK_fileSearch", str_data, &required_size);
            // printf("DSK_fileSearch:%s\n",str_data);
            DSK_fileSearch = str_data;
            free(str_data);
        }

        // Close
        nvs_close(handle);
    }

}

void Config::save() {
    Config::save("all");
}

// Dump actual config to FS
void Config::save(string value) {

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    // printf("\n");
    // printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        // printf("Done\n");


        if((value=="arch") || (value=="all"))
            nvs_set_str(handle,"arch",arch.c_str());

        if((value=="romSet") || (value=="all"))
            nvs_set_str(handle,"romSet",romSet.c_str());

        if((value=="ram") || (value=="all"))
            nvs_set_str(handle,"ram",ram_file.c_str());   

        if((value=="slog") || (value=="all"))
            nvs_set_str(handle,"slog",slog_on ? "true" : "false");

        if((value=="sdstorage") || (value=="all"))
            nvs_set_str(handle,"sdstorage",FileUtils::MountPoint == MOUNT_POINT_SD ? "true" : "false");

        if((value=="asp169") || (value=="all"))
            nvs_set_str(handle,"asp169",aspect_16_9 ? "true" : "false");

        if((value=="videomode") || (value=="all"))
            nvs_set_u8(handle,"videomode",Config::videomode);

        if((value=="language") || (value=="all"))
            nvs_set_u8(handle,"language",Config::lang);

        if((value=="AY48") || (value=="all"))
            nvs_set_str(handle,"AY48",AY48 ? "true" : "false");

        if((value=="Issue2") || (value=="all"))
            nvs_set_str(handle,"Issue2",Issue2 ? "true" : "false");

        if((value=="flashload") || (value=="all"))
            nvs_set_str(handle,"flashload",flashload ? "true" : "false");

        if((value=="joystick") || (value=="all"))
            nvs_set_u8(handle,"joystick",Config::joystick);

        if((value=="AluTiming") || (value=="all"))
            nvs_set_u8(handle,"AluTiming",Config::AluTiming);

        if((value=="PS2Dev2") || (value=="all"))
            nvs_set_u8(handle,"PS2Dev2",Config::ps2_dev2);

        if((value=="CursorAsJoy") || (value=="all"))
            nvs_set_str(handle,"CursorAsJoy", CursorAsJoy ? "true" : "false");

        if((value=="CenterH") || (value=="all"))
            nvs_set_i8(handle,"CenterH",Config::CenterH);

        if((value=="CenterV") || (value=="all"))
            nvs_set_i8(handle,"CenterV",Config::CenterV);

        if((value=="SNA_Path") || (value=="all"))
            nvs_set_str(handle,"SNA_Path",Config::SNA_Path.c_str());

        if((value=="TAP_Path") || (value=="all"))
            nvs_set_str(handle,"TAP_Path",Config::TAP_Path.c_str());

        if((value=="DSK_Path") || (value=="all"))
            nvs_set_str(handle,"DSK_Path",Config::DSK_Path.c_str());

        if((value=="SNA_begin_row") || (value=="all"))
            nvs_set_u16(handle,"SNA_begin_row",Config::SNA_begin_row);

        if((value=="TAP_begin_row") || (value=="all"))
            nvs_set_u16(handle,"TAP_begin_row",Config::TAP_begin_row);

        if((value=="DSK_begin_row") || (value=="all"))
            nvs_set_u16(handle,"DSK_begin_row",Config::DSK_begin_row);

        if((value=="SNA_focus") || (value=="all"))
            nvs_set_u16(handle,"SNA_focus",Config::SNA_focus);

        if((value=="TAP_focus") || (value=="all"))
            nvs_set_u16(handle,"TAP_focus",Config::TAP_focus);

        if((value=="DSK_focus") || (value=="all"))
            nvs_set_u16(handle,"DSK_focus",Config::DSK_focus);

        if((value=="SNA_fdMode") || (value=="all"))
            nvs_set_u8(handle,"SNA_fdMode",Config::SNA_fdMode);

        if((value=="TAP_fdMode") || (value=="all"))
            nvs_set_u8(handle,"TAP_fdMode",Config::TAP_fdMode);

        if((value=="DSK_fdMode") || (value=="all"))
            nvs_set_u8(handle,"DSK_fdMode",Config::DSK_fdMode);

        if((value=="SNA_fileSearch") || (value=="all"))
            nvs_set_str(handle,"SNA_fileSearch",Config::SNA_fileSearch.c_str());

        if((value=="TAP_fileSearch") || (value=="all"))
            nvs_set_str(handle,"TAP_fileSearch",Config::TAP_fileSearch.c_str());

        if((value=="DSK_fileSearch") || (value=="all"))
            nvs_set_str(handle,"DSK_fileSearch",Config::DSK_fileSearch.c_str());

        // printf("Committing updates in NVS ... ");

        err = nvs_commit(handle);
        if (err != ESP_OK) {
            printf("Error (%s) commiting updates to NVS!\n", esp_err_to_name(err));
        }
        
        // printf("Done\n");

        // Close
        nvs_close(handle);
    }

    // printf("Config saved OK\n");

}

void Config::requestMachine(string newArch, string newRomSet)
{

    arch = newArch;
    romSet = newRomSet;

    if (arch == "48K") {

        MemESP::rom[0] = (uint8_t *) gb_rom_0_sinclair_48k;

    } else if (arch == "128K") {

        MemESP::rom[0] = (uint8_t *) gb_rom_0_sinclair_128k;
        MemESP::rom[1] = (uint8_t *) gb_rom_1_sinclair_128k;

    } else if (arch == "Pentagon") {

        MemESP::rom[0] = (uint8_t *) gb_rom_0_pentagon_128k;
        MemESP::rom[1] = (uint8_t *) gb_rom_1_pentagon_128k;

    }

    MemESP::rom[4] = (uint8_t *) gb_rom_4_trdos_503;

    MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[1] = (unsigned char *)MemESP::ram[5];
    MemESP::ramCurrent[2] = (unsigned char *)MemESP::ram[2];
    MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = arch == "Pentagon" ? false : true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = false;
  
}
