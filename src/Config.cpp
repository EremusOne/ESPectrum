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

string   Config::arch = "48K";
string   Config::ram_file = NO_RAM_FILE;
string   Config::last_ram_file = NO_RAM_FILE;
string   Config::romSet = "SINCLAIR";
bool     Config::slog_on = false;
bool     Config::aspect_16_9 = true;
uint8_t  Config::videomode = 0; // 0 -> SAFE VGA, 1 -> 50HZ VGA, 2 -> 50HZ CRT
uint8_t  Config::esp32rev = 0;
// string   Config::kbd_layout = "US";
uint8_t  Config::lang = 0;
bool     Config::AY48 = false;
uint8_t  Config::joystick = 0; // 0 -> Cursor, 1 -> Kempston
uint8_t  Config::AluTiming = 0;

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

    pwm_audio_stop();

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
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");

        size_t required_size;
        char* str_data;
        
        err = nvs_get_str(handle, "arch", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "arch", str_data, &required_size);
            printf("arch:%s\n",str_data);
            arch = str_data;
            
            // FORCE MODEL FOR TESTING
            // arch = "48K";
            
            free(str_data);
        } else {
            // No nvs data found. Save it
            nvs_close(handle);
            Config::save();
            pwm_audio_start();
            return;
        }

        err = nvs_get_str(handle, "romSet", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "romSet", str_data, &required_size);
            printf("romSet:%s\n",str_data);
            romSet = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "ram", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "ram", str_data, &required_size);
            printf("ram:%s\n",str_data);
            ram_file = str_data;
            free(str_data);
        }

        err = nvs_get_str(handle, "slog", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "slog", str_data, &required_size);
            printf("slog:%s\n",str_data);
            slog_on = strcmp(str_data, "false");            
            free(str_data);
        }

        err = nvs_get_str(handle, "sdstorage", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "sdstorage", str_data, &required_size);
            printf("sdstorage:%s\n",str_data);

            // if (FileUtils::SDReady) {
            //     FileUtils::MountPoint = ( strcmp(str_data, "false") ? MOUNT_POINT_SD : MOUNT_POINT_SPIFFS);
            // } else
            //     FileUtils::MountPoint = MOUNT_POINT_SPIFFS;

            // Force SD from now on
            FileUtils::MountPoint = MOUNT_POINT_SD;

            free(str_data);
        }

        err = nvs_get_str(handle, "asp169", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "asp169", str_data, &required_size);
            printf("asp169:%s\n",str_data);
            aspect_16_9 = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_u8(handle, "videomode", &Config::videomode);
        if (err == ESP_OK) {
            printf("videomode:%u\n",Config::videomode);
        }


        err = nvs_get_u8(handle, "language", &Config::lang);
        if (err == ESP_OK) {
            printf("language:%u\n",Config::lang);
        }

        err = nvs_get_str(handle, "AY48", NULL, &required_size);
        if (err == ESP_OK) {
            str_data = (char *)malloc(required_size);
            nvs_get_str(handle, "AY48", str_data, &required_size);
            printf("AY48:%s\n",str_data);
            AY48 = strcmp(str_data, "false");
            free(str_data);
        }

        err = nvs_get_u8(handle, "joystick", &Config::joystick);
        if (err == ESP_OK) {
            printf("joystick:%u\n",Config::joystick);
        }

        err = nvs_get_u8(handle, "AluTiming", &Config::AluTiming);
        if (err == ESP_OK) {
            printf("AluTiming:%u\n",Config::AluTiming);
        }

        // Close
        nvs_close(handle);
    }

    // FILE *f = fopen(DISK_BOOT_FILENAME, "r");
    // if (f==NULL)
    // {
    //     // printf("Error opening %s",DISK_BOOT_FILENAME);
    //     Config::save(); // Try to create file if doesn't exist
    //     return;
    // }

    // char buf[256];
    // while(fgets(buf, sizeof(buf), f) != NULL)
    // {
    //     string line = buf;
    //     printf(line.c_str());
        // if (line.find("ram:") != string::npos) {
        //     ram_file = line.substr(line.find(':') + 1);
        //     erase_cntrl(ram_file);
        //     trim(ram_file);
        // } else if (line.find("arch:") != string::npos) {
        //     arch = line.substr(line.find(':') + 1);
        //     erase_cntrl(arch);
        //     trim(arch);
        // } else if (line.find("romset:") != string::npos) {
        //     romSet = line.substr(line.find(':') + 1);
        //     erase_cntrl(romSet);
        //     trim(romSet);
        // } else if (line.find("slog:") != string::npos) {
        //     line = line.substr(line.find(':') + 1);
        //     erase_cntrl(line);
        //     trim(line);
        //     slog_on = (line == "true" ? true : false);
        // } else if (line.find("asp169:") != string::npos) {
        //     line = line.substr(line.find(':') + 1);
        //     erase_cntrl(line);
        //     trim(line);
        //     aspect_16_9 = (line == "true");
        // } else if (line.find("sdstorage:") != string::npos) {
        //     if (FileUtils::SDReady) {
        //         line = line.substr(line.find(':') + 1);
        //         erase_cntrl(line);
        //         trim(line);
        //         FileUtils::MountPoint = (line == "true" ? MOUNT_POINT_SD : MOUNT_POINT_SPIFFS);
        //     } else
        //         FileUtils::MountPoint = MOUNT_POINT_SPIFFS;
        // } else if (line.find("kbdlayout:") != string::npos) {
        //     kbd_layout = line.substr(line.find(':') + 1);
        //     erase_cntrl(kbd_layout);
        //     trim(kbd_layout);
        // } else if (line.find("videomode:") != string::npos) {
        //     string svmode = line.substr(line.find(':') + 1);
        //     erase_cntrl(svmode);
        //     trim(svmode);
        //     Config::videomode = stoi(svmode);
        // } else if (line.find("language:") != string::npos) {
        //     string slang = line.substr(line.find(':') + 1);
        //     erase_cntrl(slang);
        //     trim(slang);
        //     Config::lang = stoi(slang);
        // } else if (line.find("AY48:") != string::npos) {
        //     line = line.substr(line.find(':') + 1);
        //     erase_cntrl(line);
        //     trim(line);
        //     AY48 = (line == "true");
        // } else if (line.find("joystick:") != string::npos) {
        //     string sjoy = line.substr(line.find(':') + 1);
        //     erase_cntrl(sjoy);
        //     trim(sjoy);
        //     Config::joystick = stoi(sjoy);
        // }


    // }
    // fclose(f);

    pwm_audio_start();

}

void Config::save() {
    Config::save("all");
}

// Dump actual config to FS
void Config::save(string value) {

    pwm_audio_stop();

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
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle_t handle;
    err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");


        if((value=="arch") || (value=="all"))
            nvs_set_str(handle,"arch",arch.c_str());

        if((value=="romSet") || (value=="all"))
            nvs_set_str(handle,"romSet",romSet.c_str());

        if((value=="ram") || (value=="all"))
            nvs_set_str(handle,"ram",ram_file.c_str());   

        if((value=="slog") || (value=="all"))
            // nvs_set_str(handle,"slog",slog_on ? "true" : "false");
            nvs_set_str(handle,"slog","true");            

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

        if((value=="joystick") || (value=="all"))
            nvs_set_u8(handle,"joystick",Config::joystick);

        if((value=="AluTiming") || (value=="all"))
            nvs_set_u8(handle,"AluTiming",Config::AluTiming);

        printf("Committing updates in NVS ... ");
        err = nvs_commit(handle);
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
        nvs_close(handle);
    }

    //printf("Saving config file '%s':\n", DISK_BOOT_FILENAME);

    // FILE *f = fopen(DISK_BOOT_FILENAME, "w");
    // if (f == NULL)
    // {
    //     printf("Error opening %s\n",DISK_BOOT_FILENAME);
    //     pwm_audio_start();
    //     return;
    // }

    // // Architecture
    // //printf(("arch:" + arch + "\n").c_str());
    // fputs(("arch:" + arch + "\n").c_str(),f);

    // // ROM set
    // //printf(("romset:" + romSet + "\n").c_str());
    // fputs(("romset:" + romSet + "\n").c_str(),f);

    // // RAM SNA
    // //printf(("ram:" + ram_file + "\n").c_str());
    // fputs(("ram:" + ram_file + "\n").c_str(),f);

    // // Serial logging
    // //printf(slog_on ? "slog:true\n" : "slog:false\n");
    // fputs(slog_on ? "slog:true\n" : "slog:false\n",f);

    // // Aspect ratio
    // //printf(aspect_16_9 ? "asp169:true\n" : "asp169:false\n");
    // fputs(aspect_16_9 ? "asp169:true\n" : "asp169:false\n",f);

    // // Mount point
    // //printf(FileUtils::MountPoint == MOUNT_POINT_SD ? "sdstorage:true\n" : "sdstorage:false\n");
    // fputs(FileUtils::MountPoint == MOUNT_POINT_SD ? "sdstorage:true\n" : "sdstorage:false\n",f);

    // // KBD layout
    // //printf(("kbdlayout:" + kbd_layout + "\n").c_str());
    // // fputs(("kbdlayout:" + kbd_layout + "\n").c_str(),f);

    // // Videomode
    // fputs(("videomode:" + std::to_string(Config::videomode) + "\n").c_str(),f);

    // // Language
    // //printf("language:%s\n",std::to_string(Config::lang).c_str());
    // fputs(("language:" + std::to_string(Config::lang) + "\n").c_str(),f);

    // // AY emulation on 48K mode
    // fputs(AY48 ? "AY48:true\n" : "AY48:false\n",f);

    // // Joystick
    // fputs(("joystick:" + std::to_string(Config::joystick) + "\n").c_str(),f);

    // fclose(f);
    
    printf("Config saved OK\n");

    pwm_audio_start();

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

    MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[1] = (unsigned char *)MemESP::ram[5];
    MemESP::ramCurrent[2] = (unsigned char *)MemESP::ram[2];
    MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = false;
  
}
