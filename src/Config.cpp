/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023, 2024 Víctor Iborra [Eremus] and 2023 David Crespo [dcrespo3d]
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
#include "OSDMain.h"

string   Config::arch = "48K";
string   Config::romSet = "48K";
string   Config::romSet48 = "48K";
string   Config::romSet128 = "128K";
string   Config::romSetTK90X = "v1es";
string   Config::romSetTK95 = "95es";
string   Config::pref_arch = "48K";
string   Config::pref_romSet_48 = "48K";
string   Config::pref_romSet_128 = "128K";
string   Config::pref_romSet_TK90X = "v1es";
string   Config::pref_romSet_TK95 = "95es";
string   Config::ram_file = NO_RAM_FILE;
string   Config::last_ram_file = NO_RAM_FILE;

bool     Config::slog_on = true;
bool     Config::aspect_16_9 = false;
uint8_t  Config::videomode = 0; // 0 -> SAFE VGA, 1 -> 50HZ VGA, 2 -> 50HZ CRT
uint8_t  Config::esp32rev = 0;
uint8_t  Config::lang = 0;
bool     Config::AY48 = true;
bool     Config::Issue2 = true;
bool     Config::flashload = true;
bool     Config::tape_player = false; // Tape player mode
bool     Config::tape_timing_rg = false; // Rodolfo Guerra ROMs tape timings

uint8_t  Config::joystick1 = JOY_SINCLAIR1;
uint8_t  Config::joystick2 = JOY_SINCLAIR2;
uint16_t Config::joydef[24] = { 
    fabgl::VK_6,
    fabgl::VK_7,
    fabgl::VK_9,
    fabgl::VK_8,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_0,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_1,
    fabgl::VK_2,
    fabgl::VK_4,
    fabgl::VK_3,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_5,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_NONE,
    fabgl::VK_NONE
};

uint8_t  Config::joyPS2 = JOY_KEMPSTON;
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

uint8_t Config::scanlines = 0;
uint8_t Config::render = 0;

bool Config::TABasfire1 = false;

bool Config::StartMsg = true;

uint8_t Config::port254default = 0xbf; // For TK90X v1 ROM -> 0xbf: Spanish, 0x3f: Portuguese

uint8_t Config::ALUTK = 1; // TK ALU -> 0 -> Ferranti, 1 -> Microdigital 50hz, 2 -> Microdigital 60hz
uint8_t Config::DiskCtrl = 1; // 0 -> None, 1 -> Betadisk

bool Config::TimeMachine = false; 

int8_t Config::volume = ESP_VOLUME_DEFAULT;

// erase control characters (in place)
static inline void erase_cntrl(std::string &s) {
    s.erase(std::remove_if(s.begin(), s.end(), 
            [&](char ch) 
            { return std::iscntrl(static_cast<unsigned char>(ch));}), 
            s.end());
}

// // trim from start (in place)
// static inline void ltrim(std::string &s) {
//     s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
//         return !std::isspace(ch);
//     }));
// }

// // trim from end (in place)
// static inline void rtrim(std::string &s) {
//     s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
//         return !std::isspace(ch);
//     }).base(), s.end());
// }

// // trim from both ends (in place)
// static inline void trim(std::string &s) {
//     rtrim(s);
//     ltrim(s);
// }

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

        err = nvs_get_str(handle, "romSet48", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "romSet48", str_data, &required_size);
            // printf("romSet48:%s\n",str_data);
            romSet48 = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "romSet128", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "romSet128", str_data, &required_size);
            // printf("romSet128:%s\n",str_data);
            romSet128 = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "romSetTK90X", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "romSetTK90X", str_data, &required_size);
            // printf("romSetTK90X:%s\n",str_data);
            romSetTK90X = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "romSetTK95", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "romSetTK95", str_data, &required_size);
            // printf("romSetTK95:%s\n",str_data);
            romSetTK95 = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "pref_arch", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "pref_arch", str_data, &required_size);
            // printf("pref_arch:%s\n",str_data);
            pref_arch = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "pref_romSet_48", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "pref_romSet_48", str_data, &required_size);
            // printf("pref_romSet_48:%s\n",str_data);
            pref_romSet_48 = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "pref_romSet_128", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "pref_romSet_128", str_data, &required_size);
            // printf("pref_romSet_128:%s\n",str_data);
            pref_romSet_128 = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "pref_romSet_90X", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "pref_romSet_90X", str_data, &required_size);
            // printf("pref_romSet_TK90X:%s\n",str_data);
            pref_romSet_TK90X = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "pref_romSet_95", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "pref_romSet_95", str_data, &required_size);
            // printf("pref_romSet_TK95:%s\n",str_data);
            pref_romSet_TK95 = str_data;
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

        err = nvs_get_str(handle, "tape_player", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "tape_player", str_data, &required_size);
            // printf("Tape player:%s\n",str_data);
            tape_player = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_str(handle, "tape_timing_rg", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "tape_timing_rg", str_data, &required_size);
            // printf("Tape timing RG:%s\n",str_data);
            tape_timing_rg = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_u8(handle, "joystick1", &Config::joystick1);
        if (err == ESP_OK) {
            // printf("joystick1:%u\n",Config::joystick1);
        }

        err = nvs_get_u8(handle, "joystick2", &Config::joystick2);
        if (err == ESP_OK) {
            // printf("joystick2:%u\n",Config::joystick2);
        }

        // Read joystick definition
        for (int n=0; n < 24; n++) {
            char joykey[9];
            sprintf(joykey,"joydef%02u",n);
            // printf("%s\n",joykey);
            err = nvs_get_u16(handle, joykey, &Config::joydef[n]);
            if (err == ESP_OK) {
                // printf("joydef00:%u\n",Config::joydef[n]);
            }
        }

        err = nvs_get_u8(handle, "joyPS2", &Config::joyPS2);
        if (err == ESP_OK) {
            // printf("joyPS2:%u\n",Config::joyPS2);
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

        err = nvs_get_u8(handle, "scanlines", &Config::scanlines);
        if (err == ESP_OK) {
            // printf("scanlines:%u\n",Config::scanlines);
        }

        err = nvs_get_u8(handle, "render", &Config::render);
        if (err == ESP_OK) {
            // printf("render:%u\n",Config::render);
        }

        err = nvs_get_str(handle, "TABasfire1", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "TABasfire1", str_data, &required_size);
            // printf("TABasfire1:%s\n",str_data);
            TABasfire1 = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_str(handle, "StartMsg", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "StartMsg", str_data, &required_size);
            // printf("StartMsg:%s\n",str_data);
            StartMsg = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_u8(handle, "ALUTK", &Config::ALUTK);
        if (err == ESP_OK) {
            // printf("ALUTK:%u\n",Config::ALUTK);
        }

        err = nvs_get_u8(handle, "DiskCtrl", &Config::DiskCtrl);
        if (err == ESP_OK) {
            // printf("DiskCtrl:%u\n",Config::DiskCtrl);
        }

        err = nvs_get_i8(handle, "volume", &Config::volume);
        if (err == ESP_OK) {
            // printf("volume:%d\n",Config::volume);
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

        if((value=="romSet48") || (value=="all"))
            nvs_set_str(handle,"romSet48",romSet48.c_str());

        if((value=="romSet128") || (value=="all"))
            nvs_set_str(handle,"romSet128",romSet128.c_str());

        if((value=="romSetTK90X") || (value=="all"))
            nvs_set_str(handle,"romSetTK90X",romSetTK90X.c_str());

        if((value=="romSetTK95") || (value=="all"))
            nvs_set_str(handle,"romSetTK95",romSetTK95.c_str());

        if((value=="pref_arch") || (value=="all"))
            nvs_set_str(handle,"pref_arch",pref_arch.c_str());

        if((value=="pref_romSet_48") || (value=="all"))
            nvs_set_str(handle,"pref_romSet_48",pref_romSet_48.c_str());

        if((value=="pref_romSet_128") || (value=="all"))
            nvs_set_str(handle,"pref_romSet_128",pref_romSet_128.c_str());

        if((value=="pref_romSet_TK90X") || (value=="all"))
            nvs_set_str(handle,"pref_romSet_90X",pref_romSet_TK90X.c_str());

        if((value=="pref_romSet_TK95") || (value=="all"))
            nvs_set_str(handle,"pref_romSet_95",pref_romSet_TK95.c_str());

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

        if((value=="tape_player") || (value=="all"))
            nvs_set_str(handle,"tape_player",tape_player ? "true" : "false");

        if((value=="tape_timing_rg") || (value=="all"))
            nvs_set_str(handle,"tape_timing_rg",tape_timing_rg ? "true" : "false");

        if((value=="joystick1") || (value=="all"))
            nvs_set_u8(handle,"joystick1",Config::joystick1);

        if((value=="joystick2") || (value=="all"))
            nvs_set_u8(handle,"joystick2",Config::joystick2);

        // Write joystick definition
        for (int n=0; n < 24; n++) {
            char joykey[9];
            sprintf(joykey,"joydef%02u",n);
            if((value == joykey) || (value=="all")) {
                nvs_set_u16(handle,joykey,Config::joydef[n]);
                // printf("%s %u\n",joykey, joydef[n]);
            }
        }

        if((value=="joyPS2") || (value=="all"))
            nvs_set_u8(handle,"joyPS2",Config::joyPS2);

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

        if((value=="scanlines") || (value=="all"))
            nvs_set_u8(handle,"scanlines",Config::scanlines);

        if((value=="render") || (value=="all"))
            nvs_set_u8(handle,"render",Config::render);

        if((value=="TABasfire1") || (value=="all"))
            nvs_set_str(handle,"TABasfire1", TABasfire1 ? "true" : "false");

        if((value=="StartMsg") || (value=="all"))
            nvs_set_str(handle,"StartMsg", StartMsg ? "true" : "false");

        if((value=="ALUTK") || (value=="all"))
            nvs_set_u8(handle,"ALUTK",Config::ALUTK);

        if((value=="DiskCtrl") || (value=="all"))
            nvs_set_u8(handle,"DiskCtrl",Config::DiskCtrl);

        if((value=="volume") || (value=="all"))
            nvs_set_i8(handle,"volume",Config::volume);

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

void Config::requestMachine(string newArch, string newRomSet) {

    arch = newArch;

    port254default = 0xbf; // Default value for port 254

    if (arch == "48K") {

        if (newRomSet=="") romSet = "48K"; else romSet = newRomSet;
        
        if (newRomSet=="") romSet48 = "48K"; else romSet48 = newRomSet;        

        if (romSet48 == "48K")
            MemESP::rom[0] = (uint8_t *) gb_rom_0_sinclair_48k;
        else if (romSet48 == "48Kes")
            MemESP::rom[0] = (uint8_t *) gb_rom_0_48k_es;
        else if (romSet48 == "48Kcs") {
            MemESP::rom[0] = (uint8_t *) gb_rom_0_48k_custom;
            MemESP::rom[0] += 8;
        }

    } else if (arch == "128K") {

        if (newRomSet=="") romSet = "128K"; else romSet = newRomSet;

        if (newRomSet=="") romSet128 = "128K"; else romSet128 = newRomSet;                

        if (romSet128 == "128K") {
            MemESP::rom[0] = (uint8_t *) gb_rom_0_sinclair_128k;
            MemESP::rom[1] = (uint8_t *) gb_rom_1_sinclair_128k;
        } else if (romSet128 == "128Kes") {
            MemESP::rom[0] = (uint8_t *) gb_rom_0_128k_es;
            MemESP::rom[1] = (uint8_t *) gb_rom_1_128k_es;
        } else if (romSet128 == "128Kcs") {

            MemESP::rom[0] = (uint8_t *) gb_rom_0_128k_custom;
            MemESP::rom[0] += 8;

            MemESP::rom[1] = (uint8_t *) gb_rom_0_128k_custom;
            MemESP::rom[1] += 16392;

        } else if (romSet128 == "+2") {
            MemESP::rom[0] = (uint8_t *) gb_rom_0_plus2;
            MemESP::rom[1] = (uint8_t *) gb_rom_1_plus2;
        } else if (romSet128 == "+2es") {
            MemESP::rom[0] = (uint8_t *) gb_rom_0_plus2_es;
            MemESP::rom[1] = (uint8_t *) gb_rom_1_plus2_es;
        } else if (romSet128 == "ZX81+") {
            MemESP::rom[0] = (uint8_t *) gb_rom_0_s128_zx81;
            MemESP::rom[1] = (uint8_t *) gb_rom_1_sinclair_128k;
        }

    } else if (arch == "Pentagon") {

        if (newRomSet=="") romSet = "Pentagon"; else romSet = newRomSet;

        MemESP::rom[0] = (uint8_t *) gb_rom_0_pentagon_128k;
        MemESP::rom[1] = (uint8_t *) gb_rom_1_pentagon_128k;

    } else if (arch == "TK90X") {

        if (newRomSet=="") romSet = "v1es"; else romSet = newRomSet;
        
        if (newRomSet=="") romSetTK90X = "v1es"; else romSetTK90X = newRomSet;

        if (romSetTK90X == "v1es")
            MemESP::rom[0] = (uint8_t *) rom_0_TK90X_v1;
        else if (romSetTK90X == "v1pt") {
            MemESP::rom[0] = (uint8_t *) rom_0_TK90X_v1;
            port254default = 0x3f;                
        } else if (romSetTK90X == "v2es") {
            MemESP::rom[0] = (uint8_t *) rom_0_TK90X_v2;
        } else if (romSetTK90X == "v2pt") {
            MemESP::rom[0] = (uint8_t *) rom_0_TK90X_v2;
            port254default = 0x3f;                
        } else if (romSetTK90X == "v3es") {
            MemESP::rom[0] = (uint8_t *) rom_0_TK90X_v3es;
        } else if (romSetTK90X == "v3pt") {
            MemESP::rom[0] = (uint8_t *) rom_0_TK90X_v3pt;
        } else if (romSetTK90X == "v3en") {
            MemESP::rom[0] = (uint8_t *) rom_0_TK90X_v3en;
        } else if (romSetTK90X == "TKcs") {
            MemESP::rom[0] = (uint8_t *) rom_0_tk_custom;
            MemESP::rom[0] += 8;
        }


    } else if (arch == "TK95") {

        if (newRomSet=="") romSet = "95es"; else romSet = newRomSet;
        
        if (newRomSet=="") romSetTK95 = "95es"; else romSetTK95 = newRomSet;

        if (romSetTK95 == "95es")
            MemESP::rom[0] = (uint8_t *) rom_0_TK95ES;
        else if (romSetTK95 == "95pt") {
            MemESP::rom[0] = (uint8_t *) rom_0_TK95ES;
            port254default = 0x3f;
        }

    }

    MemESP::rom[4] = (uint8_t *) gb_rom_4_trdos_503;

}

void Config::setJoyMap(uint8_t joynum, uint8_t joytype) {

fabgl::VirtualKey newJoy[12];

for (int n=0; n < 12; n++) newJoy[n] = fabgl::VK_NONE;

// Ask to overwrite map with default joytype values
string title = (joynum == 1 ? "Joystick 1" : "Joystick 2");
string msg = OSD_DLG_SETJOYMAPDEFAULTS[Config::lang];
uint8_t res = OSD::msgDialog(title,msg);
if (res == DLG_YES) {

    switch (joytype) {
    case JOY_CURSOR:
        newJoy[0] = fabgl::VK_5;
        newJoy[1] = fabgl::VK_8;
        newJoy[2] = fabgl::VK_7;
        newJoy[3] = fabgl::VK_6;
        newJoy[6] = fabgl::VK_0;
        break;
    case JOY_KEMPSTON:
        newJoy[0] = fabgl::VK_KEMPSTON_LEFT;
        newJoy[1] = fabgl::VK_KEMPSTON_RIGHT;
        newJoy[2] = fabgl::VK_KEMPSTON_UP;
        newJoy[3] = fabgl::VK_KEMPSTON_DOWN;
        newJoy[6] = fabgl::VK_KEMPSTON_FIRE;
        newJoy[7] = fabgl::VK_KEMPSTON_ALTFIRE;
        break;
    case JOY_SINCLAIR1:
        newJoy[0] = fabgl::VK_6;
        newJoy[1] = fabgl::VK_7;
        newJoy[2] = fabgl::VK_9;
        newJoy[3] = fabgl::VK_8;
        newJoy[6] = fabgl::VK_0;
        break;
    case JOY_SINCLAIR2:
        newJoy[0] = fabgl::VK_1;
        newJoy[1] = fabgl::VK_2;
        newJoy[2] = fabgl::VK_4;
        newJoy[3] = fabgl::VK_3;
        newJoy[6] = fabgl::VK_5;
        break;
    case JOY_FULLER:
        newJoy[0] = fabgl::VK_FULLER_LEFT;
        newJoy[1] = fabgl::VK_FULLER_RIGHT;
        newJoy[2] = fabgl::VK_FULLER_UP;
        newJoy[3] = fabgl::VK_FULLER_DOWN;
        newJoy[6] = fabgl::VK_FULLER_FIRE;
        break;
    }

}

// Fill joystick values in Config and clean Kempston or Fuller values if needed
int m = (joynum == 1) ? 0 : 12;

for (int n = m; n < m + 12; n++) {

    bool save = false;
    if (newJoy[n - m] != fabgl::VK_NONE) {
        ESPectrum::JoyVKTranslation[n] = newJoy[n - m];
        save = true;
    } else {

        if (joytype != JOY_KEMPSTON) {
            if (ESPectrum::JoyVKTranslation[n] >= fabgl::VK_KEMPSTON_RIGHT && ESPectrum::JoyVKTranslation[n] <= fabgl::VK_KEMPSTON_ALTFIRE) {
                ESPectrum::JoyVKTranslation[n] = fabgl::VK_NONE;
                save = true;
            }
        }

        if (joytype != JOY_FULLER) {
            if (ESPectrum::JoyVKTranslation[n] >= fabgl::VK_FULLER_RIGHT && ESPectrum::JoyVKTranslation[n] <= fabgl::VK_FULLER_FIRE) {
                ESPectrum::JoyVKTranslation[n] = fabgl::VK_NONE;
                save = true;                
            }
        }

    }

    if (save) {
        // Save to config (only changes)
        if (Config::joydef[n] != (uint16_t) ESPectrum::JoyVKTranslation[n]) {
            Config::joydef[n] = (uint16_t) ESPectrum::JoyVKTranslation[n];
            char joykey[9];
            sprintf(joykey,"joydef%02u",n);
            Config::save(joykey);
            // printf("%s %u\n",joykey, joydef[n]);
        }
    }

}

}
