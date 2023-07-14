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

#include "OSDMain.h"
#include "FileUtils.h"
#include "CPU.h"
#include "Video.h"
#include "ESPectrum.h"
#include "messages.h"
#include "Config.h"
#include "FileSNA.h"
#include "FileZ80.h"
#include "MemESP.h"
#include "Tape.h"
#include "ZXKeyb.h"
#include "pwm_audio.h"

#ifndef ESP32_SDL2_WRAPPER
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "fabgl.h"

#include "soc/rtc_wdt.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#include <string>

using namespace std;

#define MENU_REDRAW true
#define MENU_UPDATE false
#define OSD_ERROR true
#define OSD_NORMAL false

#define OSD_W 248
#define OSD_H 184
#define OSD_MARGIN 4

extern Font Font6x8;

uint8_t OSD::menu_level = 0;
bool OSD::menu_saverect = false;
unsigned short OSD::menu_curopt = 1;
unsigned int OSD::SaveRectpos = 0;

unsigned short OSD::scrW = 320;
unsigned short OSD::scrH = 240;

// // X origin to center an element with pixel_width
unsigned short OSD::scrAlignCenterX(unsigned short pixel_width) { return (scrW / 2) - (pixel_width / 2); }

// // Y origin to center an element with pixel_height
unsigned short OSD::scrAlignCenterY(unsigned short pixel_height) { return (scrH / 2) - (pixel_height / 2); }

uint8_t OSD::osdMaxRows() { return (OSD_H - (OSD_MARGIN * 2)) / OSD_FONT_H; }
uint8_t OSD::osdMaxCols() { return (OSD_W - (OSD_MARGIN * 2)) / OSD_FONT_W; }
unsigned short OSD::osdInsideX() { return scrAlignCenterX(OSD_W) + OSD_MARGIN; }
unsigned short OSD::osdInsideY() { return scrAlignCenterY(OSD_H) + OSD_MARGIN; }

void OSD::esp_hard_reset() {
    // RESTART ESP32 (This is the most similar way to hard resetting it)
#ifndef ESP32_SDL2_WRAPPER
    rtc_wdt_protect_off();
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_RTC);
    rtc_wdt_set_time(RTC_WDT_STAGE0, 100);
    rtc_wdt_enable();
    rtc_wdt_protect_on();
    while (true);
#endif
}

// // Cursor to OSD first row,col
void OSD::osdHome() { VIDEO::vga.setCursor(osdInsideX(), osdInsideY()); }

// // Cursor positioning
void OSD::osdAt(uint8_t row, uint8_t col) {
    if (row > osdMaxRows() - 1)
        row = 0;
    if (col > osdMaxCols() - 1)
        col = 0;
    unsigned short y = (row * OSD_FONT_H) + osdInsideY();
    unsigned short x = (col * OSD_FONT_W) + osdInsideX();
    VIDEO::vga.setCursor(x, y);
}

void OSD::drawOSD(bool bottom_info) {
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);
    VIDEO::vga.fillRect(x, y, OSD_W, OSD_H, OSD::zxColor(1, 0));
    VIDEO::vga.rect(x, y, OSD_W, OSD_H, OSD::zxColor(0, 0));
    VIDEO::vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, OSD::zxColor(7, 0));
    VIDEO::vga.setTextColor(OSD::zxColor(0, 0), OSD::zxColor(5, 1));
    VIDEO::vga.setFont(Font6x8);
    osdHome();
    VIDEO::vga.print(OSD_TITLE);
    osdAt(21, 0);
    if (bottom_info) {
        string bottom_line;
        switch(Config::videomode) {
            case 0: bottom_line = " Video mode: Standard VGA   "; break;
            case 1: bottom_line = " Video mode: VGA 50hz       "; break;
            case 2: bottom_line = " Video mode: CRT 50hz       "; break;
        }
        VIDEO::vga.print(bottom_line.append(EMU_VERSION).c_str());
    } else VIDEO::vga.print(OSD_BOTTOM);
    osdHome();
}

void OSD::drawStats(char *line1, char *line2) {

    unsigned short x,y;

    if (Config::aspect_16_9) {
        x = 188;
        y = 176;
    } else {
        x = 168;
        y = 220;
    }

    VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(1, 0));
    VIDEO::vga.setFont(Font6x8);
    VIDEO::vga.setCursor(x,y);
    VIDEO::vga.print(line1);
    VIDEO::vga.setCursor(x,y+8);
    VIDEO::vga.print(line2);

}

static bool persistSave(uint8_t slotnumber)
{
    char persistfname[sizeof(DISK_PSNA_FILE) + 6];
    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    OSD::osdCenteredMsg(OSD_PSNA_SAVING, LEVEL_INFO, 0);
    if (!FileSNA::save(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname)) {
        OSD::osdCenteredMsg(OSD_PSNA_SAVE_ERR, LEVEL_WARN);
        return false;
    }
    OSD::osdCenteredMsg(OSD_PSNA_SAVED, LEVEL_INFO);
    return true;
}

static bool persistLoad(uint8_t slotnumber)
{
    
    OSD::osdCenteredMsg(OSD_PSNA_LOADING, LEVEL_INFO);

    char persistfname[sizeof(DISK_PSNA_FILE) + 6];
    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    if (!FileSNA::isPersistAvailable(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname)) {
        OSD::osdCenteredMsg(OSD_PSNA_NOT_AVAIL, LEVEL_INFO);
        return false;
    } else {
        if (!FileSNA::load(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname)) {
            OSD::osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
            return false;
        } else {
            Config::ram_file = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname;
            #ifdef SNAPSHOT_LOAD_LAST
            Config::save("ram");
            #endif
            Config::last_ram_file = Config::ram_file;
            OSD::osdCenteredMsg(OSD_PSNA_LOADED, LEVEL_INFO);
        }
    }

    return true;

}

// #ifdef ZXKEYB
#define REPDEL 140 // As in real ZX Spectrum (700 ms.)
static int zxDelay = 0;
// #endif

// OSD Main Loop
void OSD::do_OSD(fabgl::VirtualKey KeytoESP) {

    static uint8_t last_sna_row = 0;
    fabgl::VirtualKeyItem Nextkey;

    if (KeytoESP == fabgl::VK_PAUSE) {
        osdCenteredMsg(OSD_PAUSE[Config::lang], LEVEL_INFO, 0);
        while (1) {
            if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {        
                if (ESPectrum::readKbd(&Nextkey))
                    if ((Nextkey.down) && (Nextkey.vk == fabgl::VK_PAUSE)) break;
            }
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
        click();
    }
    else if (KeytoESP == fabgl::VK_F2) {
        menu_level = 0;
        menu_curopt = 1;
        string mFile = menuFile(FileUtils::MountPoint + DISK_SNA_DIR, MENU_SNA_TITLE[Config::lang],".sna.SNA.z80.Z80");
        if (mFile != "") {
            changeSnapshot(FileUtils::MountPoint + DISK_SNA_DIR + "/" + mFile);
        }
    }
    else if (KeytoESP == fabgl::VK_F3) {
        menu_level = 0;
        menu_curopt = 1;
        // Persist Load
        uint8_t opt2 = menuRun(MENU_PERSIST_LOAD[Config::lang]);
        if (opt2 > 0 && opt2<11) {
            persistLoad(opt2);
        }
    }
    else if (KeytoESP == fabgl::VK_F4) {
        menu_level = 0;
        menu_curopt = 1;
        // Persist Save
        uint8_t opt2 = menuRun(MENU_PERSIST_SAVE[Config::lang]);
        if (opt2 > 0 && opt2<11) {
            persistSave(opt2);
        }
    }
    else if (KeytoESP == fabgl::VK_F5) {
        menu_level = 0;      
        menu_curopt = 1;
        string mFile = menuFile(FileUtils::MountPoint + DISK_TAP_DIR, MENU_TAP_TITLE[Config::lang],".tap.TAP");
        if (mFile != "") {
            Tape::TAP_Stop();
            // Read and analyze tape file
            Tape::Open(FileUtils::MountPoint + DISK_TAP_DIR "/" + mFile);
            // Tape::tapeFileName=FileUtils::MountPoint + DISK_TAP_DIR "/" + mFile;
        }
    }
    else if (KeytoESP == fabgl::VK_F6) {
        // Start / Stop .tap reproduction
        Tape::TAP_Play();
        click();
    }
    else if (KeytoESP == fabgl::VK_F7) {

        // Tape Browser
        if (Tape::tapeFileName=="none") {
            OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR[Config::lang], LEVEL_WARN);
        } else {
            menu_level = 0;      
            menu_curopt = 1;
            int tBlock = menuTape(Tape::tapeFileName.substr(6,28));
            if (tBlock >= 0) {
                Tape::tapeCurBlock = tBlock;
                Tape::TAP_Stop();
                Tape::TAP_Play();
            }
        }

    }
    else if (KeytoESP == fabgl::VK_F8) {
        // Show / hide OnScreen Stats
        if (VIDEO::OSD) {
            if (Config::aspect_16_9) 
                VIDEO::DrawOSD169 = VIDEO::MainScreen;
            else
                VIDEO::DrawOSD43 = VIDEO::BottomBorder;
            VIDEO::OSD = false;
        } else {
            if (Config::aspect_16_9) 
                VIDEO::DrawOSD169 = VIDEO::MainScreen_OSD;
            else
                VIDEO::DrawOSD43  = VIDEO::BottomBorder_OSD;
            VIDEO::OSD = true;
        }    
        click();
    }
    else if (KeytoESP == fabgl::VK_F9) { // Volume down

        #ifdef TESTING_CODE

        ESPectrum::target--;

        // // Check if destination file exists
        // struct stat st;
        // if (stat("/sd/s/1942.z80", &st) == 0) {
        //     //printf("Exists!\n");
        // }

        // FILE *f = fopen("/sd/s/1942.z80", "r");
        // if (f == NULL) {
        //     printf("Null file!\n");
        // } else fclose(f);

        #else

        if (ESPectrum::aud_volume>-16) {
                click();
                ESPectrum::aud_volume--;
                pwm_audio_set_volume(ESPectrum::aud_volume);
        }

        #endif

    }
    else if (KeytoESP == fabgl::VK_F10) { // Volume up

        #ifdef TESTING_CODE
        
        ESPectrum::target++;

        #else

        if (ESPectrum::aud_volume<0) {
                click();                
                ESPectrum::aud_volume++;
                pwm_audio_set_volume(ESPectrum::aud_volume);
        }

        #endif

    }    
    // else if (KeytoESP == fabgl::VK_F9) {
    //     ESPectrum::ESPoffset -= 5;
    // }
    // else if (KeytoESP == fabgl::VK_F10) {
    //     ESPectrum::ESPoffset += 5;
    // }
    else if (KeytoESP == fabgl::VK_F12) {
        
        // // Switch boot partition
        // string splabel;
        // esp_err_t chg_ota;
        // const esp_partition_t *ESPectrum_partition = NULL;

        // ESPectrum_partition = esp_ota_get_running_partition();
        // if (ESPectrum_partition->label=="128k") splabel = "48k"; else splabel= "128k";

        // ESPectrum_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,splabel.c_str());
        // chg_ota = esp_ota_set_boot_partition(ESPectrum_partition);

        // ESP host reset
        #ifndef SNAPSHOT_LOAD_LAST
        Config::ram_file = NO_RAM_FILE;
        Config::save("ram");
        #endif
        esp_hard_reset();

    }
    // else if (KeytoESP == fabgl::VK_F12) {
    //     // Switch boot partition
    //     esp_err_t chg_ota;
    //     const esp_partition_t *ESPectrum_partition = NULL;

    //     //ESPectrum_partition = esp_ota_get_next_update_partition(NULL);
    //     ESPectrum_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,"128k");
    //     chg_ota = esp_ota_set_boot_partition(ESPectrum_partition);

    //     if (chg_ota == ESP_OK ) esp_hard_reset();
            
    // }
    else if (KeytoESP == fabgl::VK_F1) {

        menu_curopt = 1;
        
        while(1) {

        // Main menu
        menu_level = 0;
        uint8_t opt = menuRun("ESPectrum " + Config::getArch() + "\n" + MENU_MAIN[Config::lang]);
  
        if (opt == 1) {
            // ***********************************************************************************
            // SNAPSHOTS MENU
            // ***********************************************************************************
            menu_saverect = true;
            menu_curopt = 1;
            while(1) {
                menu_level = 1;
                // Snapshot menu
                uint8_t sna_mnu = menuRun(MENU_SNA[Config::lang]);
                if (sna_mnu > 0) {
                    menu_level = 2;
                    menu_saverect = true;
                    if (sna_mnu == 1) {
                        menu_curopt = 1;
                        string mFile = menuFile(FileUtils::MountPoint + DISK_SNA_DIR, MENU_SNA_TITLE[Config::lang],".sna.SNA.z80.Z80");
                        if (mFile != "") {
                            changeSnapshot(FileUtils::MountPoint + DISK_SNA_DIR + "/" + mFile);
                            return;
                        }
                    }
                    else if (sna_mnu == 2) {
                        // Persist Load
                        menu_curopt = 1;
                        while (1) {
                            uint8_t opt2 = menuRun(MENU_PERSIST_LOAD[Config::lang]);
                            if (opt2 > 0 && opt2<11) {
                                if (persistLoad(opt2)) return;
                                menu_saverect = false;
                                menu_curopt = opt2;
                            } else break;
                        }
                    }
                    else if (sna_mnu == 3) {
                        // Persist Save
                        menu_curopt = 1;
                        while (1) {
                            uint8_t opt2 = menuRun(MENU_PERSIST_SAVE[Config::lang]);
                            if (opt2 > 0 && opt2<11) {
                                if (persistSave(opt2)) return;
                                menu_saverect = false;
                                menu_curopt = opt2;
                            } else break;
                        }
                    }
                    menu_curopt = sna_mnu;
                } else {
                    menu_curopt = 1;
                    break;
                }
            }
        } 
        else if (opt == 2) {
            // ***********************************************************************************
            // TAPE MENU
            // ***********************************************************************************
            menu_saverect = true;
            menu_curopt = 1;            
            while(1) {
                menu_level = 1;
                // Tape menu
                uint8_t tap_num = menuRun(MENU_TAPE[Config::lang]);
                if (tap_num > 0) {
                    menu_level = 2;
                    menu_saverect = true;
                    if (tap_num == 1) {
                        menu_curopt = 1;
                        // Select TAP File
                        string mFile = menuFile(FileUtils::MountPoint + DISK_TAP_DIR, MENU_TAP_TITLE[Config::lang],".tap.TAP");
                        if (mFile != "") {
                            Tape::TAP_Stop();
                            // Read and analyze tape file
                            Tape::Open(FileUtils::MountPoint + DISK_TAP_DIR "/" + mFile);
                            return;
                        }
                    }
                    else if (tap_num == 2) {
                        // Start / Stop .tap reproduction
                        Tape::TAP_Play();
                        return;                        
                    }
                    else if (tap_num == 3) {
                        // Tape Browser
                        if (Tape::tapeFileName=="none") {
                            OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR[Config::lang], LEVEL_WARN);
                            menu_curopt = 2;
                            menu_saverect = false;
                        } else {
                            menu_level = 0;
                            menu_saverect = false;
                            menu_curopt = 1;
                            int tBlock = menuTape(Tape::tapeFileName.substr(6,28));
                            if (tBlock >= 0) {
                                Tape::tapeCurBlock = tBlock;
                                Tape::TAP_Stop();
                                Tape::TAP_Play();
                            }
                            return;
                        }
                    }
                } else {
                    menu_curopt = 2;
                    break;
                }
            }
        }
        else if (opt == 3) {
            // ***********************************************************************************
            // RESET MENU
            // ***********************************************************************************
            menu_saverect = true;
            menu_curopt = 1;            
            while(1) {
                menu_level = 1;
                // Reset
                uint8_t opt2 = menuRun(MENU_RESET[Config::lang]);
                if (opt2 == 1) {
                    // Soft
                    if (Config::last_ram_file != NO_RAM_FILE)
                        changeSnapshot(Config::last_ram_file);
                    else ESPectrum::reset();
                    return;
                }
                else if (opt2 == 2) {
                    // Hard
                    Config::ram_file = NO_RAM_FILE;
                    Config::save("ram");
                    Config::last_ram_file = NO_RAM_FILE;
                    ESPectrum::reset();
                    return;
                }
                else if (opt2 == 3) {
                    // ESP host reset
                    #ifndef SNAPSHOT_LOAD_LAST
                    Config::ram_file = NO_RAM_FILE;
                    Config::save("ram");
                    #endif
                    esp_hard_reset();
                } else {
                    menu_curopt = 3;
                    break;
                }
            }
        }
        else if (opt == 4) {
            // ***********************************************************************************
            // OPTIONS MENU
            // ***********************************************************************************
            menu_saverect = true;
            menu_curopt = 1;
            while(1) {
                menu_level = 1;
                // Options menu
                uint8_t options_num = menuRun(MENU_OPTIONS[Config::lang]);
                if (options_num == 1) {
                    menu_saverect = true;
                    menu_curopt = 1;
                    while (1) {
                        menu_level = 2;
                        // Storage source
                        string stor_menu = MENU_STORAGE[Config::lang];
                        // int curopt;
                        // if (FileUtils::MountPoint == MOUNT_POINT_SPIFFS) {
                        //     stor_menu.replace(stor_menu.find("[I",0),2,"[*");
                        //     stor_menu.replace(stor_menu.find("[S",0),2,"[ ");
                        //     curopt = 1;
                        // } else {
                        //     stor_menu.replace(stor_menu.find("[I",0),2,"[ ");
                        //     stor_menu.replace(stor_menu.find("[S",0),2,"[*");
                        //     curopt = 2;
                        // }
                        uint8_t opt2 = menuRun(stor_menu);
                        if (opt2) {
                            // if (opt2 == 3) {
                                OSD::osdCenteredMsg("Refreshing snap dir", LEVEL_INFO);
                                int chunks = FileUtils::DirToFile(FileUtils::MountPoint + DISK_SNA_DIR, ".sna.SNA.z80.Z80"); // Prepare sna filelist
                                if (chunks) FileUtils::Mergefiles(FileUtils::MountPoint + DISK_SNA_DIR,chunks); // Merge files
                                OSD::osdCenteredMsg("Refreshing tape dir", LEVEL_INFO);
                                chunks = FileUtils::DirToFile(FileUtils::MountPoint + DISK_TAP_DIR, ".tap.TAP"); // Prepare tap filelist
                                if (chunks) FileUtils::Mergefiles(FileUtils::MountPoint + DISK_TAP_DIR,chunks); // Merge files
                                return;
                            // } else if (opt2 != curopt) {
                            //     if (opt2 == 1)
                            //         FileUtils::MountPoint = MOUNT_POINT_SPIFFS;
                            //     else
                            //         FileUtils::MountPoint = MOUNT_POINT_SD;
                            //     Config::save();
                            // }
                            // menu_curopt = opt2;
                            // menu_saverect = false;
                        } else {
                            menu_curopt = 1;                            
                            break;
                        }
                    }
                }
                else if (options_num == 2) {
                    menu_level = 2;
                    menu_curopt = 1;
                    menu_saverect = true;
                    // Change ROM
                    string arch_menu = getArchMenu();
                    uint8_t arch_num = menuRun(arch_menu);
                    if (arch_num) {
                            string arch = (arch_num==1 ? "48K" : "128K");
                            if (arch != Config::getArch()) {
                                Config::requestMachine(arch, "SINCLAIR", true);
                                Config::ram_file = "none";
                                Config::save();
                                if(Config::videomode) esp_hard_reset();
                            }
                            ESPectrum::reset();
                            return;
                    }
                    menu_curopt = 2;
                }
                else if (options_num == 3) {
                    menu_level = 2;
                    menu_curopt = 1;                    
                    menu_saverect = true;
                    while (1) {
                        // aspect ratio
                        string asp_menu = MENU_ASPECT[Config::lang];
                        bool prev_asp = Config::aspect_16_9;
                        if (prev_asp) {
                            asp_menu.replace(asp_menu.find("[4",0),2,"[ ");
                            asp_menu.replace(asp_menu.find("[1",0),2,"[*");                        
                        } else {
                            asp_menu.replace(asp_menu.find("[4",0),2,"[*");
                            asp_menu.replace(asp_menu.find("[1",0),2,"[ ");                        
                        }
                        uint8_t opt2 = menuRun(asp_menu);
                        if (opt2) {
                            if (opt2 == 1)
                                Config::aspect_16_9 = false;
                            else
                                Config::aspect_16_9 = true;

                            if (Config::aspect_16_9 != prev_asp) {
                                #ifndef SNAPSHOT_LOAD_LAST
                                Config::ram_file = "none";
                                #endif
                                Config::save();
                                esp_hard_reset();
                            }

                            menu_curopt = opt2;
                            menu_saverect = false;

                        } else {
                            menu_curopt = 3;
                            break;
                        }
                    }
                }
                else if (options_num == 4) {
                    menu_level = 2;
                    menu_curopt = 1;
                    menu_saverect = true;
                    while (1) {
                        // joystick
                        string Mnustr = MENU_JOY[Config::lang];
                        std::size_t pos = Mnustr.find("[",0);
                        int nfind = 0;
                        while (pos != string::npos) {
                            if (nfind == Config::joystick) {
                                Mnustr.replace(pos,2,"[*");
                                break;
                            }
                            pos = Mnustr.find("[",pos + 1);
                            nfind++;
                        }
                        uint8_t opt2 = menuRun(Mnustr);
                        if (opt2) {
                            if (Config::joystick != (opt2 - 1)) {
                                Config::joystick = opt2 - 1;
                                Config::save("joystick");
                            }
                            menu_curopt = opt2;
                            menu_saverect = false;
                        } else {
                            menu_curopt = 4;
                            break;
                        }
                    }
                }
                else if (options_num == 5) {
                    menu_level = 2;
                    menu_curopt = 1;                    
                    menu_saverect = true;
                    while (1) {
                        // language
                        uint8_t opt2;
                        string Mnustr = MENU_INTERFACE_LANG[Config::lang];                            
                        std::size_t pos = Mnustr.find("[",0);
                        int nfind = 0;
                        while (pos != string::npos) {
                            if (nfind == Config::lang) {
                                Mnustr.replace(pos,2,"[*");
                                break;
                            }
                            pos = Mnustr.find("[",pos + 1);
                            nfind++;
                        }
                        opt2 = menuRun(Mnustr);
                        if (opt2) {
                            if (Config::lang != (opt2 - 1)) {
                                Config::lang = opt2 - 1;
                                Config::save("language");
                                return;
                            }
                            menu_curopt = opt2;
                            menu_saverect = false;
                        } else {
                            menu_curopt = 5;
                            break;
                        }
                    }
                }
                else if (options_num == 6) {
                    menu_level = 2;
                    menu_curopt = 1;                    
                    menu_saverect = true;
                    while (1) {
                        // Other
                        uint8_t options_num = menuRun(MENU_OTHER[Config::lang]);
                        if (options_num > 0) {
                            if (options_num == 1) {
                                menu_level = 3;
                                menu_curopt = 1;                    
                                menu_saverect = true;
                                while (1) {
                                    string ay_menu = MENU_AY48[Config::lang];
                                    bool prev_ay48 = Config::AY48;
                                    if (prev_ay48) {
                                        ay_menu.replace(ay_menu.find("[Y",0),2,"[*");
                                        ay_menu.replace(ay_menu.find("[N",0),2,"[ ");                        
                                    } else {
                                        ay_menu.replace(ay_menu.find("[Y",0),2,"[ ");
                                        ay_menu.replace(ay_menu.find("[N",0),2,"[*");                        
                                    }
                                    uint8_t opt2 = menuRun(ay_menu);
                                    if (opt2) {
                                        if (opt2 == 1)
                                            Config::AY48 = true;
                                        else
                                            Config::AY48 = false;

                                        if (Config::AY48 != prev_ay48) {
                                            Config::save("AY48");
                                        }
                                        menu_curopt = opt2;
                                        menu_saverect = false;
                                    } else {
                                        menu_curopt = 1;
                                        menu_level = 2;                                       
                                        break;
                                    }
                                }
                            }
                            else if (options_num == 2) {
                                menu_level = 3;
                                menu_curopt = 1;                    
                                menu_saverect = true;
                                while (1) {
                                    string alu_menu = MENU_ALUTIMING[Config::lang];
                                    uint8_t prev_AluTiming = Config::AluTiming;
                                    if (prev_AluTiming == 0) {
                                        alu_menu.replace(alu_menu.find("[E",0),2,"[*");
                                        alu_menu.replace(alu_menu.find("[L",0),2,"[ ");                        
                                    } else {
                                        alu_menu.replace(alu_menu.find("[E",0),2,"[ ");
                                        alu_menu.replace(alu_menu.find("[L",0),2,"[*");                        
                                    }
                                    uint8_t opt2 = menuRun(alu_menu);
                                    if (opt2) {
                                        if (opt2 == 1)
                                            Config::AluTiming = 0;
                                        else
                                            Config::AluTiming = 1;

                                        if (Config::AluTiming != prev_AluTiming) {
                                            CPU::latetiming = Config::AluTiming;
                                            Config::save("AluTiming");
                                        }
                                        menu_curopt = opt2;
                                        menu_saverect = false;
                                    } else {
                                        menu_curopt = 2;
                                        menu_level = 2;                                       
                                        break;
                                    }
                                }
                            }
                            else if (options_num == 3) {
                                menu_level = 3;
                                menu_curopt = 1;                    
                                menu_saverect = true;
                                while (1) {
                                    string iss_menu = MENU_ISSUE2[Config::lang];
                                    bool prev_iss = Config::Issue2;
                                    if (prev_iss) {
                                        iss_menu.replace(iss_menu.find("[Y",0),2,"[*");
                                        iss_menu.replace(iss_menu.find("[N",0),2,"[ ");                        
                                    } else {
                                        iss_menu.replace(iss_menu.find("[Y",0),2,"[ ");
                                        iss_menu.replace(iss_menu.find("[N",0),2,"[*");                        
                                    }
                                    uint8_t opt2 = menuRun(iss_menu);
                                    if (opt2) {
                                        if (opt2 == 1)
                                            Config::Issue2 = true;
                                        else
                                            Config::Issue2 = false;

                                        if (Config::Issue2 != prev_iss) {
                                            Config::save("Issue2");
                                        }
                                        menu_curopt = opt2;
                                        menu_saverect = false;
                                    } else {
                                        menu_curopt = 3;
                                        menu_level = 2;                                       
                                        break;
                                    }
                                }
                            }
                        } else {
                            menu_curopt = 6;
                            break;
                        }
                    }
                } else {
                    menu_curopt = 4;
                    break;
                }
            }
        }
        else if (opt == 5) {
            // Help
            drawOSD(true);
            osdAt(2, 0);
            VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(1, 0));
            VIDEO::vga.print(OSD_HELP[Config::lang]);

            // #ifdef ZXKEYB
            zxDelay = REPDEL;
            // #endif

            while (1) {

                // #ifdef ZXKEYB

                if (ZXKeyb::Exists) {

                    ZXKeyb::process();

                    if ((!bitRead(ZXKeyb::ZXcols[6], 0)) || (!bitRead(ZXKeyb::ZXcols[4], 0))) { // ENTER
                        if (zxDelay == 0) {
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_RETURN, true, false);
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_RETURN, false, false);                
                            zxDelay = REPDEL;
                        }
                    } else
                    if ((!bitRead(ZXKeyb::ZXcols[7], 0)) || (!bitRead(ZXKeyb::ZXcols[4], 1))) { // BREAK
                        if (zxDelay == 0) {
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE, true, false);
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE, false, false);                        
                            zxDelay = REPDEL;
                        }
                    } else
                    if (!bitRead(ZXKeyb::ZXcols[2], 0)) { // Q (Capture screen)
                        if (zxDelay == 0) {
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PRINTSCREEN, true, false);
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PRINTSCREEN, false, false);
                            zxDelay = REPDEL;
                        }
                    }
                
                }

                // #endif

                if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
                    if (ESPectrum::readKbd(&Nextkey)) {
                        if(!Nextkey.down) continue;
                        if ((Nextkey.vk == fabgl::VK_F1) || (Nextkey.vk == fabgl::VK_ESCAPE) || (Nextkey.vk == fabgl::VK_RETURN)) break;
                    }
                }

                vTaskDelay(5 / portTICK_PERIOD_MS);

                // #ifdef ZXKEYB        
                if (zxDelay > 0) zxDelay--;
                // #endif

            }

            click();

            return;

        }        
        else if (opt == 6) {
            // About
            drawOSD(false);
            osdAt(2, 0);
            VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(1, 0));
            VIDEO::vga.print(OSD_ABOUT[Config::lang]);
            
            // #ifdef ZXKEYB
            zxDelay = REPDEL;
            // #endif
            
            while (1) {

                // #ifdef ZXKEYB

                if (ZXKeyb::Exists) {

                    ZXKeyb::process();

                    if ((!bitRead(ZXKeyb::ZXcols[6], 0)) || (!bitRead(ZXKeyb::ZXcols[4], 0))) { // ENTER                
                        if (zxDelay == 0) {
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_RETURN, true, false);
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_RETURN, false, false);                
                            zxDelay = REPDEL;
                        }
                    } else
                    if ((!bitRead(ZXKeyb::ZXcols[7], 0)) || (!bitRead(ZXKeyb::ZXcols[4], 1))) { // BREAK                
                        if (zxDelay == 0) {
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE, true, false);
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE, false, false);                        
                            zxDelay = REPDEL;
                        }
                    } else
                    if (!bitRead(ZXKeyb::ZXcols[2], 0)) { // Q (Capture screen)
                        if (zxDelay == 0) {
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PRINTSCREEN, true, false);
                            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PRINTSCREEN, false, false);
                            zxDelay = REPDEL;
                        }
                    }

                }

                // #endif

                if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
                    if (ESPectrum::readKbd(&Nextkey)) {
                        if(!Nextkey.down) continue;
                        if ((Nextkey.vk == fabgl::VK_F1) || (Nextkey.vk == fabgl::VK_ESCAPE) || (Nextkey.vk == fabgl::VK_RETURN)) break;
                    }
                }

                vTaskDelay(5 / portTICK_PERIOD_MS);
                
                // #ifdef ZXKEYB        
                if (zxDelay > 0) zxDelay--;
                // #endif


            }

            click();

            return;            

        }
        else break;
    }        
    }
}

// Shows a red panel with error text
void OSD::errorPanel(string errormsg) {
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);

    if (Config::slog_on)
        printf((errormsg + "\n").c_str());

    VIDEO::vga.fillRect(x, y, OSD_W, OSD_H, OSD::zxColor(0, 0));
    VIDEO::vga.rect(x, y, OSD_W, OSD_H, OSD::zxColor(7, 0));
    VIDEO::vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, OSD::zxColor(2, 1));
    VIDEO::vga.setFont(Font6x8);
    osdHome();
    VIDEO::vga.setTextColor(OSD::zxColor(7, 1), OSD::zxColor(2, 1));
    VIDEO::vga.print(ERROR_TITLE);
    osdAt(2, 0);
    VIDEO::vga.setTextColor(OSD::zxColor(7, 1), OSD::zxColor(0, 0));
    VIDEO::vga.println(errormsg.c_str());
    osdAt(17, 0);
    VIDEO::vga.setTextColor(OSD::zxColor(7, 1), OSD::zxColor(2, 1));
    VIDEO::vga.print(ERROR_BOTTOM);
}

// Error panel and infinite loop
void OSD::errorHalt(string errormsg) {
    errorPanel(errormsg);
    while (1) {
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

// Centered message
void OSD::osdCenteredMsg(string msg, uint8_t warn_level) {
    osdCenteredMsg(msg,warn_level,1000);
}
void OSD::osdCenteredMsg(string msg, uint8_t warn_level, uint16_t millispause) {
    const unsigned short w = (msg.length() + 2) * OSD_FONT_W;
    const unsigned short h = OSD_FONT_H * 3;
    const unsigned short x = scrAlignCenterX(w);
    const unsigned short y = scrAlignCenterY(h);
    unsigned short paper;
    unsigned short ink;
    unsigned int j;

    switch (warn_level) {
    case LEVEL_OK:
        ink = OSD::zxColor(7, 1);
        paper = OSD::zxColor(4, 0);
        break;
    case LEVEL_ERROR:
        ink = OSD::zxColor(7, 1);
        paper = OSD::zxColor(2, 0);
        break;
    case LEVEL_WARN:
        ink = OSD::zxColor(0, 0);
        paper = OSD::zxColor(6, 0);
        break;
    default:
        ink = OSD::zxColor(7, 0);
        paper = OSD::zxColor(1, 0);
    }

    if (millispause > 0) {
        // Save backbuffer data
        j = SaveRectpos;
        for (int  m = y; m < y + h; m++) {
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                VIDEO::SaveRect[SaveRectpos] = backbuffer32[n];
                SaveRectpos++;
            }
        }
        // printf("Saverectpos: %d\n",SaveRectpos);
    }

    VIDEO::vga.fillRect(x, y, w, h, paper);
    // VIDEO::vga.rect(x - 1, y - 1, w + 2, h + 2, ink);
    VIDEO::vga.setTextColor(ink, paper);
    VIDEO::vga.setFont(Font6x8);
    VIDEO::vga.setCursor(x + OSD_FONT_W, y + OSD_FONT_H);
    VIDEO::vga.print(msg.c_str());
    
    if (millispause > 0) {

        vTaskDelay(millispause/portTICK_PERIOD_MS); // Pause if needed

        SaveRectpos = j;
        for (int  m = y; m < y + h; m++) {
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                backbuffer32[n] = VIDEO::SaveRect[j];
                j++;
            }
        }

    }
}

// // Count NL chars inside a string, useful to count menu rows
unsigned short OSD::rowCount(string menu) {
    unsigned short count = 0;
    for (unsigned short i = 0; i < menu.length(); i++) {
//    for (unsigned short i = 1; i < menu.length(); i++) {
        if (menu.at(i) == ASCII_NL) {
            count++;
        }
    }
    return count;
}

// // Get a row text
string OSD::rowGet(string menu, unsigned short row) {
    unsigned short count = 0;
    unsigned short last = 0;
    for (unsigned short i = 0; i < menu.length(); i++) {
        if (menu.at(i) == ASCII_NL) {
            if (count == row) {
                return menu.substr(last,i - last);
            }
            count++;
            last = i + 1;
        }
    }
    return "<Unknown menu row>";
}

// Change running snapshot
void OSD::changeSnapshot(string filename)
{
    // string dir = FileUtils::MountPoint + DISK_SNA_DIR;

    if (FileUtils::hasSNAextension(filename))
    {
    
        osdCenteredMsg(MSG_LOADING_SNA + (string) ": " + filename, LEVEL_INFO, 0);
        // printf("Loading SNA: <%s>\n", (dir + "/" + filename).c_str());
        FileSNA::load(filename);        

    }
    else if (FileUtils::hasZ80extension(filename))
    {
        osdCenteredMsg(MSG_LOADING_Z80 + (string)": " + filename, LEVEL_INFO, 0);
        // printf("Loading Z80: %s\n", filename.c_str());
        FileZ80::load(filename);

    }

    Config::ram_file = filename;
    #ifdef SNAPSHOT_LOAD_LAST
    Config::save("ram");
    #endif
    Config::last_ram_file = filename;

}
