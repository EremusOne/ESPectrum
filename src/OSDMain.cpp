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
#include "Snapshot.h"
#include "MemESP.h"
#include "Tape.h"
#include "ZXKeyb.h"
#include "pwm_audio.h"
#include "Z80_JLS/z80.h"

#ifndef ESP32_SDL2_WRAPPER
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_efuse.h"
#include "soc/efuse_reg.h"

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

uint8_t OSD::cols;                     // Maximum columns
uint8_t OSD::mf_rows;                  // File menu maximum rows
unsigned short OSD::real_rows;      // Real row count
uint8_t OSD::virtual_rows;             // Virtual maximum rows on screen
uint16_t OSD::w;                        // Width in pixels
uint16_t OSD::h;                        // Height in pixels
uint16_t OSD::x;                        // X vertical position
uint16_t OSD::y;                        // Y horizontal position
uint16_t OSD::prev_y[5];                // Y prev. position
unsigned short OSD::menu_prevopt = 1;
string OSD::menu;                   // Menu string
unsigned short OSD::begin_row = 1;      // First real displayed row
uint8_t OSD::focus = 1;                    // Focused virtual row
uint8_t OSD::last_focus = 0;               // To check for changes
unsigned short OSD::last_begin_row = 0; // To check for changes

uint8_t OSD::menu_level = 0;
bool OSD::menu_saverect = false;
unsigned short OSD::menu_curopt = 1;
unsigned int OSD::SaveRectpos = 0;

unsigned short OSD::scrW = 320;
unsigned short OSD::scrH = 240;

char OSD::stats_lin1[25]; // "CPU: 00000 / IDL: 00000 ";
char OSD::stats_lin2[25]; // "FPS:000.00 / FND:000.00 ";

// // X origin to center an element with pixel_width
unsigned short OSD::scrAlignCenterX(unsigned short pixel_width) { return (scrW / 2) - (pixel_width / 2); }

// // Y origin to center an element with pixel_height
unsigned short OSD::scrAlignCenterY(unsigned short pixel_height) { return (scrH / 2) - (pixel_height / 2); }

uint8_t OSD::osdMaxRows() { return (OSD_H - (OSD_MARGIN * 2)) / OSD_FONT_H; }
uint8_t OSD::osdMaxCols() { return (OSD_W - (OSD_MARGIN * 2)) / OSD_FONT_W; }
unsigned short OSD::osdInsideX() { return scrAlignCenterX(OSD_W) + OSD_MARGIN; }
unsigned short OSD::osdInsideY() { return scrAlignCenterY(OSD_H) + OSD_MARGIN; }

DRAM_ATTR static const uint8_t click48[12]={ 0,0x16,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x16,0 };

DRAM_ATTR static const uint8_t click128[116]= { 0x00,0x16,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                                0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                                0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                                0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                                0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                                0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                                0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                                0x61,0x61,0x16,0x00
                                            };

IRAM_ATTR void OSD::click() {

    size_t written;

    pwm_audio_set_volume(ESP_DEFAULT_VOLUME);

    if (Z80Ops::is48) {
        pwm_audio_write((uint8_t *) click48, 12, &written,  5 / portTICK_PERIOD_MS);
    } else {
        pwm_audio_write((uint8_t *) click128, 116, &written, 5 / portTICK_PERIOD_MS);
    }

    pwm_audio_set_volume(ESPectrum::aud_volume);

    // printf("Written: %d\n",written);

}

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
    VIDEO::vga.fillRect(x, y, OSD_W, OSD_H, zxColor(1, 0));
    VIDEO::vga.rect(x, y, OSD_W, OSD_H, zxColor(0, 0));
    VIDEO::vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, zxColor(7, 0));
    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(5, 1));
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

void OSD::drawStats() {

    unsigned short x,y;

    if (Config::aspect_16_9) {
        x = 156;
        y = 176;
    } else {
        x = 168;
        y = 220;
    }

    VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));
    VIDEO::vga.setFont(Font6x8);
    VIDEO::vga.setCursor(x,y);
    VIDEO::vga.print(stats_lin1);
    VIDEO::vga.setCursor(x,y+8);
    VIDEO::vga.print(stats_lin2);

}

static bool persistSave(uint8_t slotnumber)
{
    struct stat stat_buf;
    char persistfname[sizeof(DISK_PSNA_FILE) + 6];
    char persistfinfo[sizeof(DISK_PSNA_FILE) + 6];    

    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    sprintf(persistfinfo,DISK_PSNA_FILE "%u.esp",slotnumber);
    string finfo = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfinfo;

    // Slot isn't void
    if (stat(finfo.c_str(), &stat_buf) == 0) {
        string title = OSD_PSNA_SAVE[Config::lang];
        string msg = OSD_PSNA_EXISTS[Config::lang];
        uint8_t res = OSD::msgDialog(title,msg);
        if (res != DLG_YES) return false;
    }

    OSD::osdCenteredMsg(OSD_PSNA_SAVING, LEVEL_INFO, 0);

    // Save info file
    FILE *f = fopen(finfo.c_str(), "w");
    if (f == NULL) {

        printf("Error opening %s\n",persistfinfo);

    } else {

        fputs((Config::getArch() + "\n").c_str(),f);    // Put architecture on info file
        fclose(f);    

        if (!FileSNA::save(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname)) OSD::osdCenteredMsg(OSD_PSNA_SAVE_ERR, LEVEL_WARN);
    
    }

    return true;

}

static bool persistLoad(uint8_t slotnumber)
{
    
    char persistfname[sizeof(DISK_PSNA_FILE) + 6];
    char persistfinfo[sizeof(DISK_PSNA_FILE) + 6];        

    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    sprintf(persistfinfo,DISK_PSNA_FILE "%u.esp",slotnumber);

    if (!FileSNA::isPersistAvailable(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname)) {
        OSD::osdCenteredMsg(OSD_PSNA_NOT_AVAIL, LEVEL_INFO);
        return false;
    } else {

        // Read info file
        string finfo = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfinfo;
        FILE *f = fopen(finfo.c_str(), "r");
        if (f == NULL) {
            OSD::osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
            // printf("Error opening %s\n",persistfinfo);
            return false;
        }

        char buf[256];
        fgets(buf, sizeof(buf),f);
        string persist_arch = buf;
        persist_arch.pop_back();
        // printf("[%s]\n",persist_arch.c_str());

        fclose(f);

        if (!LoadSnapshot(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname, persist_arch)) {
            OSD::osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
            return false;
        } else {
            Config::ram_file = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname;
            #ifdef SNAPSHOT_LOAD_LAST
            Config::save("ram");
            #endif
            Config::last_ram_file = Config::ram_file;
        }
    }

    return true;

}

// OSD Main Loop
void OSD::do_OSD(fabgl::VirtualKey KeytoESP, uint8_t CTRL) {

    static uint8_t last_sna_row = 0;
    fabgl::VirtualKeyItem Nextkey;

    if (CTRL) {

        if (KeytoESP == fabgl::VK_F5) {
            if (Config::CenterH > -16) Config::CenterH--;
            Config::save("CenterH");
            osdCenteredMsg("Horiz. center: " + to_string(Config::CenterH), LEVEL_INFO, 375);
        } else 
        if (KeytoESP == fabgl::VK_F6) {
            if (Config::CenterH < 16) Config::CenterH++;
            Config::save("CenterH");
            osdCenteredMsg("Horiz. center: " + to_string(Config::CenterH), LEVEL_INFO, 375);
        } else 
        if (KeytoESP == fabgl::VK_F7) {
            if (Config::CenterV > -16) Config::CenterV--;
            Config::save("CenterV");
            osdCenteredMsg("Vert. center: " + to_string(Config::CenterV), LEVEL_INFO, 375);
        } else 
        if (KeytoESP == fabgl::VK_F8) {
            if (Config::CenterV < 16) Config::CenterV++;
            Config::save("CenterV");
            
            osdCenteredMsg("Vert. center: " + to_string(Config::CenterV), LEVEL_INFO, 375);
        } else
        if (KeytoESP == fabgl::VK_F1) { // Show mem info
            OSD::HWInfo();
        }

    } else {

        if (KeytoESP == fabgl::VK_PAUSE) {

            click();

            osdCenteredMsg(OSD_PAUSE[Config::lang], LEVEL_INFO, 1000);

            while (1) {
                
                if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

                ESPectrum::readKbdJoy();

                if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
                    if (ESPectrum::readKbd(&Nextkey)) {
                        if(!Nextkey.down) continue;
                            if (Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_PAUSE) {
                                click();
                                break;
                            } else
                                osdCenteredMsg(OSD_PAUSE[Config::lang], LEVEL_INFO, 500);
                    }
                }

                vTaskDelay(5 / portTICK_PERIOD_MS);

            }

        }
        else if (KeytoESP == fabgl::VK_F2) {
            menu_level = 0;
            menu_saverect = false;
            string mFile = fileDialog(FileUtils::SNA_Path, MENU_SNA_TITLE[Config::lang],DISK_SNAFILE,51,22);
            if (mFile != "") {
                mFile.erase(0, 1);
                string fname = FileUtils::MountPoint + "/" + FileUtils::SNA_Path + "/" + mFile;
                LoadSnapshot(fname,"");
                Config::ram_file = fname;
                #ifdef SNAPSHOT_LOAD_LAST
                Config::save("ram");
                #endif
                Config::last_ram_file = fname;
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
            // Persist Save
            menu_level = 0;
            menu_curopt = 1;
            while (1) {
                uint8_t opt2 = menuRun(MENU_PERSIST_SAVE[Config::lang]);
                if (opt2 > 0 && opt2<11) {
                    if (persistSave(opt2)) return;
                    menu_curopt = opt2;
                } else break;
            }
        }
        else if (KeytoESP == fabgl::VK_F5) {
            menu_level = 0; 
            menu_saverect = false;  
            string mFile = fileDialog(FileUtils::TAP_Path, MENU_TAP_TITLE[Config::lang],DISK_TAPFILE,51,22);
            if (mFile != "") {

                string keySel = mFile.substr(0,1);
                mFile.erase(0, 1);

                if ((keySel ==  "R") && (Config::flashload)) {

                        OSD::osdCenteredMsg(OSD_TAPE_FLASHLOAD, LEVEL_INFO, 0);

                        if (Z80Ops::is48)
                            FileZ80::loader48();
                        else
                            FileZ80::loader128();

                        // Put something random on FRAMES SYS VAR as recommended by Mark Woodmass
                        // https://skoolkid.github.io/rom/asm/5C78.html
                        MemESP::writebyte(0x5C78,rand() % 256);
                        MemESP::writebyte(0x5C79,rand() % 256);            

                        if (Config::ram_file != NO_RAM_FILE) {
                            Config::ram_file = NO_RAM_FILE;
                            #ifdef SNAPSHOT_LOAD_LAST
                            Config::save("ram");
                            #endif
                        }
                        Config::last_ram_file = NO_RAM_FILE;

                }

                Tape::TAP_Stop();

                // Read and analyze tape file
                // Tape::Open(FileUtils::MountPoint + "/" + FileUtils::TAP_Path + "/" + mFile);
                Tape::Open(mFile);

                ESPectrum::TapeNameScroller = 0;

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
                // int tBlock = menuTape(Tape::tapeFileName.substr(6,28));
                int tBlock = menuTape(Tape::tapeFileName.substr(0,22));
                if (tBlock >= 0) {
                    Tape::tapeCurBlock = tBlock;
                    Tape::TAP_Stop();
                }
            }

        }
        else if (KeytoESP == fabgl::VK_F8) {
            // Show / hide OnScreen Stats
            if (VIDEO::OSD) {
                if (Config::aspect_16_9) 
                    VIDEO::DrawOSD169 = Z80Ops::isPentagon ? VIDEO::MainScreen_Pentagon : VIDEO::MainScreen;
                else
                    VIDEO::DrawOSD43 = Z80Ops::isPentagon ? VIDEO::BottomBorder_Pentagon :  VIDEO::BottomBorder;
                VIDEO::OSD = false;
            } else {
                if (Config::aspect_16_9) 
                    VIDEO::DrawOSD169 = Z80Ops::isPentagon ? VIDEO::MainScreen_OSD_Pentagon : VIDEO::MainScreen_OSD;
                else
                    VIDEO::DrawOSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;
                VIDEO::OSD = true;
                ESPectrum::TapeNameScroller = 0;
                OSD::drawStats();
            }    
            click();
        }
        else if (KeytoESP == fabgl::VK_F9) { // Volume down
            if (ESPectrum::aud_volume>-16) {
                ESPectrum::aud_volume--;
                pwm_audio_set_volume(ESPectrum::aud_volume);
                click();
            }
            // osdCenteredMsg("Volume: " + to_string(ESPectrum::aud_volume + 16), LEVEL_INFO, 125);
        }
        else if (KeytoESP == fabgl::VK_F10) { // Volume up
            if (ESPectrum::aud_volume<0) {
                ESPectrum::aud_volume++;
                pwm_audio_set_volume(ESPectrum::aud_volume);
                click();
            }
            // osdCenteredMsg("Volume: " + to_string(ESPectrum::aud_volume + 16), LEVEL_INFO, 125);
        }    
        // else if (KeytoESP == fabgl::VK_F9) {
        //     ESPectrum::ESPoffset -= 5;
        // }
        // else if (KeytoESP == fabgl::VK_F10) {
        //     ESPectrum::ESPoffset += 5;
        // }
        else if (KeytoESP == fabgl::VK_F11) { // Hard reset
            if (Config::ram_file != NO_RAM_FILE) {
                Config::ram_file = NO_RAM_FILE;
                #ifdef SNAPSHOT_LOAD_LAST
                Config::save("ram");
                #endif
            }
            Config::last_ram_file = NO_RAM_FILE;
            ESPectrum::reset();
        }
        else if (KeytoESP == fabgl::VK_F12) { // ESP32 reset
            // ESP host reset
            #ifndef SNAPSHOT_LOAD_LAST
            Config::ram_file = NO_RAM_FILE;
            Config::save("ram");
            #endif
            esp_hard_reset();
        }
        else if (KeytoESP == fabgl::VK_F1) {

            menu_curopt = 1;
            
            while(1) {

            // Main menu
            menu_saverect = false;
            menu_level = 0;
            uint8_t opt = menuRun("ESPectrum " + Config::getArch() + "\n" + MENU_MAIN[Config::lang]);
            // uint8_t opt = menuRun(Config::getArch() + "\n" + MENU_MAIN[Config::lang]);            
    
            if (opt == 1) {
                // ***********************************************************************************
                // SNAPSHOTS MENU
                // ***********************************************************************************
                menu_curopt = 1;
                menu_saverect = true;
                while(1) {
                    menu_level = 1;
                    // Snapshot menu
                    uint8_t sna_mnu = menuRun(MENU_SNA[Config::lang]);
                    if (sna_mnu > 0) {
                        menu_level = 2;
                        menu_saverect = true;
                        if (sna_mnu == 1) {
                            string mFile = fileDialog(FileUtils::SNA_Path, MENU_SNA_TITLE[Config::lang],DISK_SNAFILE,28,16);
                            if (mFile != "") {
                                mFile.erase(0, 1);
                                string fname = FileUtils::MountPoint + "/" + FileUtils::SNA_Path + "/" + mFile;
                                LoadSnapshot(fname,"");
                                Config::ram_file = fname;
                                #ifdef SNAPSHOT_LOAD_LAST
                                Config::save("ram");
                                #endif
                                Config::last_ram_file = fname;
                                return;
                            }
                        }
                        else if (sna_mnu == 2) {
                            // Persist Load
                            menu_curopt = 1;
                            menu_saverect = true;
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
                            menu_saverect = true;
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
                            // menu_curopt = 1;
                            // Select TAP File
                            string mFile = fileDialog(FileUtils::TAP_Path, MENU_TAP_TITLE[Config::lang],DISK_TAPFILE,28,16);
                            if (mFile != "") {

                                string keySel = mFile.substr(0,1);
                                mFile.erase(0, 1);

                                if ((keySel ==  "R") && (Config::flashload)) {

                                        OSD::osdCenteredMsg(OSD_TAPE_FLASHLOAD, LEVEL_INFO, 0);
                                        
                                        if (Z80Ops::is48) {
                                            FileZ80::loader48();
                                            // changeSnapshot(FileUtils::MountPoint + "/load48.z80");
                                        } else {
                                            FileZ80::loader128();
                                            // changeSnapshot(FileUtils::MountPoint + "/load128.z80");
                                        }

                                        // Put something random on FRAMES SYS VAR as recommended by Mark Woodmass
                                        // https://skoolkid.github.io/rom/asm/5C78.html
                                        MemESP::writebyte(0x5C78,rand() % 256);
                                        MemESP::writebyte(0x5C79,rand() % 256);            

                                        if (Config::ram_file != NO_RAM_FILE) {
                                            Config::ram_file = NO_RAM_FILE;
                                            #ifdef SNAPSHOT_LOAD_LAST
                                            Config::save("ram");
                                            #endif
                                        }
                                        Config::last_ram_file = NO_RAM_FILE;

                                }

                                Tape::TAP_Stop();

                                // Read and analyze tape file
                                // Tape::Open(FileUtils::MountPoint + "/" + FileUtils::TAP_Path + "/" + mFile);
                                Tape::Open(mFile);
                                
                                ESPectrum::TapeNameScroller = 0;

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
                                // int tBlock = menuTape(Tape::tapeFileName.substr(6,28));
                                int tBlock = menuTape(Tape::tapeFileName.substr(0,22));
                                if (tBlock >= 0) {
                                    Tape::tapeCurBlock = tBlock;
                                    Tape::TAP_Stop();
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
                // BETADISK MENU
                // ***********************************************************************************
                menu_saverect = true;
                menu_curopt = 1;            
                while(1) {
                    // Betadisk menu
                    menu_level = 1;
                    uint8_t dsk_num = menuRun(MENU_BETADISK[Config::lang]);
                    if (dsk_num > 0) {
                        string drvmenu = MENU_BETADRIVE[Config::lang];
                        drvmenu.replace(drvmenu.find("#",0),1,(string)" " + char(64 + dsk_num));
                        menu_saverect = true;
                        menu_curopt = 1;
                        while (1) {
                            menu_level = 2;
                            uint8_t opt2 = menuRun(drvmenu);
                            if (opt2 > 0) {
                                if (opt2 == 1) {
                                    menu_saverect = true;
                                    string mFile = fileDialog(FileUtils::DSK_Path, MENU_DSK_TITLE[Config::lang],DISK_DSKFILE,26,15);
                                    if (mFile != "") {
                                        mFile.erase(0, 1);
                                        string fname = FileUtils::MountPoint + "/" + FileUtils::DSK_Path + "/" + mFile;
                                        ESPectrum::Betadisk.EjectDisk(dsk_num - 1);
                                        ESPectrum::Betadisk.InsertDisk(dsk_num - 1,fname);
                                        return;
                                    }
                                } else 
                                if (opt2 == 2) {
                                    ESPectrum::Betadisk.EjectDisk(dsk_num - 1);
                                    return;
                                }
                            } else {
                                menu_curopt = dsk_num;
                                break;                            
                            }
                        }
                    } else {
                        menu_curopt = 3;
                        break;
                    }
                }
            }
            else if (opt == 4) {
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
                        if (Config::last_ram_file != NO_RAM_FILE) {
                            LoadSnapshot(Config::last_ram_file,"");
                            Config::ram_file = Config::last_ram_file;
                            #ifdef SNAPSHOT_LOAD_LAST
                            Config::save("ram");
                            #endif
                        } else ESPectrum::reset();
                        return;
                    }
                    else if (opt2 == 2) {
                        // Hard
                        if (Config::ram_file != NO_RAM_FILE) {
                            Config::ram_file = NO_RAM_FILE;
                            #ifdef SNAPSHOT_LOAD_LAST
                            Config::save("ram");
                            #endif
                        }
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
                        menu_curopt = 4;
                        break;
                    }
                }
            }
            else if (opt == 5) {
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
                        menu_level = 2;
                        menu_curopt = 1;
                        menu_saverect = true;
                        while (1) {
                            // menu_level = 2;
                            // Storage source
                            // string stor_menu = MENU_STORAGE[Config::lang];
                            string stor_menu = Config::lang ? MENU_STORAGE_ES : MENU_STORAGE_EN;
                            uint8_t opt2 = menuRun(stor_menu);
                            if (opt2) {
                                if (opt2 == 1) {
                                    menu_level = 3;
                                    menu_curopt = 1;                    
                                    menu_saverect = true;
                                    while (1) {
                                        string flash_menu = Config::lang ? MENU_FLASHLOAD_ES : MENU_FLASHLOAD_EN;
                                        bool prev_flashload = Config::flashload;
                                        if (prev_flashload) {
                                            flash_menu.replace(flash_menu.find("[Y",0),2,"[*");
                                            flash_menu.replace(flash_menu.find("[N",0),2,"[ ");                        
                                        } else {
                                            flash_menu.replace(flash_menu.find("[Y",0),2,"[ ");
                                            flash_menu.replace(flash_menu.find("[N",0),2,"[*");                        
                                        }
                                        uint8_t opt2 = menuRun(flash_menu);
                                        if (opt2) {
                                            if (opt2 == 1)
                                                Config::flashload = true;
                                            else
                                                Config::flashload = false;

                                            if (Config::flashload != prev_flashload) {
                                                Config::save("flashload");
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
                                menu_curopt = opt2;
                                menu_saverect = false;
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
                        string arch_menu = (string)MENU_ARCH[Config::lang];                    
                        uint8_t arch_num = menuRun(arch_menu);
                        if (arch_num) {
                                // string arch = (arch_num==1 ? "48K" : "128K");
                                string arch = Config::archnames[arch_num - 1];
                                if (arch != Config::getArch()) {
                                    Config::requestMachine(arch, "SINCLAIR");
                                    Config::ram_file = "none";
                                    Config::save("ram");
                                    Config::save("arch");
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
                                    Config::save("asp169");
                                    Config::save("ram");
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
                            if (opt2 > 0 &&  opt2 < 3) {
                                if (Config::joystick != (opt2 - 1)) {
                                    Config::joystick = opt2 - 1;
                                    Config::save("joystick");
                                }
                                menu_curopt = opt2;
                                menu_saverect = false;
                            } else {
                                if (opt2) {
                                    // Menu cursor keys as joy
                                    menu_level = 3;
                                    menu_curopt = 1;
                                    menu_saverect = true;
                                    while (1) {
                                        string csasjoy_menu = MENU_CURSORJOY[Config::lang];
                                        if (Config::CursorAsJoy) {
                                            csasjoy_menu.replace(csasjoy_menu.find("[Y",0),2,"[*");
                                            csasjoy_menu.replace(csasjoy_menu.find("[N",0),2,"[ ");                        
                                        } else {
                                            csasjoy_menu.replace(csasjoy_menu.find("[Y",0),2,"[ ");
                                            csasjoy_menu.replace(csasjoy_menu.find("[N",0),2,"[*");                        
                                        }
                                        uint8_t opt2 = menuRun(csasjoy_menu);
                                        if (opt2) {
                                            if (opt2 == 1)
                                                Config::CursorAsJoy = true;
                                            else
                                                Config::CursorAsJoy = false;

                                            ESPectrum::PS2Controller.keyboard()->setLEDs(false,false,Config::CursorAsJoy);
                                            if(ESPectrum::ps2kbd2)
                                                ESPectrum::PS2Controller.keybjoystick()->setLEDs(false, false, Config::CursorAsJoy);
                                            Config::save("CursorAsJoy");

                                            menu_curopt = opt2;
                                            menu_saverect = false;
                                        } else {
                                            menu_curopt = 3;
                                            menu_level = 2;                                       
                                            break;
                                        }
                                    }

                                } else {
                                    menu_curopt = 4;
                                    break;
                                }
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
                                else if (options_num == 4) {
                                    menu_level = 3;
                                    menu_curopt = 1;                    
                                    menu_saverect = true;
                                    while (1) {
                                        string ps2_menu = MENU_KBD2NDPS2[Config::lang];
                                        uint8_t prev_ps2 = Config::ps2_dev2;
                                        if (prev_ps2) {
                                            ps2_menu.replace(ps2_menu.find("[N",0),2,"[ ");                        
                                            ps2_menu.replace(ps2_menu.find("[K",0),2,"[*");
                                        } else {
                                            ps2_menu.replace(ps2_menu.find("[N",0),2,"[*");
                                            ps2_menu.replace(ps2_menu.find("[K",0),2,"[ ");
                                        }
                                        uint8_t opt2 = menuRun(ps2_menu);
                                        if (opt2) {
                                            if (opt2 == 1)
                                                Config::ps2_dev2 = 0;
                                            else
                                                Config::ps2_dev2 = 1;

                                            if (Config::ps2_dev2 != prev_ps2) {
                                                Config::save("PS2Dev2");
                                            }
                                            menu_curopt = opt2;
                                            menu_saverect = false;
                                        } else {
                                            menu_curopt = 4;
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
                    } else if (options_num == 7) {

                        menu_level = 2;

                        string title = OSD_FIRMW_UPDATE[Config::lang];
                        string msg = OSD_DLG_SURE[Config::lang];
                        uint8_t res = msgDialog(title,msg);

                        if (res == DLG_YES) {

                            // Open firmware file
                            FILE *firmware = fopen("/sd/firmware.bin", "rb");
                            if (firmware == NULL) {
                                osdCenteredMsg(OSD_NOFIRMW_ERR[Config::lang], LEVEL_WARN, 2000);
                                return;
                            } else {
                                esp_err_t res = updateFirmware(firmware);
                                fclose(firmware);
                                string errMsg = OSD_FIRMW_ERR[Config::lang];
                                errMsg += " Code = " + to_string(res);
                                osdCenteredMsg(errMsg, LEVEL_ERROR, 3000);
                            }

                            return;

                        } else {

                            menu_curopt = 7;
                            menu_saverect = false;

                        }

                    } else {
                        menu_curopt = 5;
                        break;
                    }
                }
            }
            else if (opt == 6) {
                // Help
                drawOSD(true);
                osdAt(2, 0);
                VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));
                if (ZXKeyb::Exists)
                    VIDEO::vga.print(Config::lang ? OSD_HELP_ES_ZX : OSD_HELP_EN_ZX);
                else
                    VIDEO::vga.print(Config::lang ? OSD_HELP_ES : OSD_HELP_EN);

                while (1) {

                    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

                    ESPectrum::readKbdJoy();

                    if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
                        if (ESPectrum::readKbd(&Nextkey)) {
                            if(!Nextkey.down) continue;
                            if ((Nextkey.vk == fabgl::VK_F1) || (Nextkey.vk == fabgl::VK_ESCAPE) || (Nextkey.vk == fabgl::VK_RETURN)) break;
                        }
                    }

                    vTaskDelay(5 / portTICK_PERIOD_MS);

                }

                click();

                return;

            }        
            else if (opt == 7) {

                // About
                drawOSD(false);
                
                VIDEO::vga.fillRect(Config::aspect_16_9 ? 60 : 40,Config::aspect_16_9 ? 12 : 32,240,50,zxColor(0, 0));            

                // Decode Logo in EBF8 format
                uint8_t *logo = (uint8_t *)ESPectrum_logo;
                int pos_x = Config::aspect_16_9 ? 86 : 66;
                int pos_y = Config::aspect_16_9 ? 23 : 43;
                int logo_w = (logo[5] << 8) + logo[4]; // Get Width
                int logo_h = (logo[7] << 8) + logo[6]; // Get Height
                logo+=8; // Skip header
                for (int i=0; i < logo_h; i++)
                    for(int n=0; n<logo_w; n++)
                        VIDEO::vga.dotFast(pos_x + n,pos_y + i,logo[n+(i*logo_w)]);

                // About Page 1
                // osdAt(7, 0);
                VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));
                // VIDEO::vga.print(Config::lang ? OSD_ABOUT1_ES : OSD_ABOUT1_EN);
                
                pos_x = Config::aspect_16_9 ? 66 : 46;
                pos_y = Config::aspect_16_9 ? 68 : 88;            
                int osdRow = 0; int osdCol = 0;
                int msgIndex = 0; int msgChar = 0;
                int msgDelay = 0; int cursorBlink = 16; int nextChar = 0;
                uint16_t cursorCol = zxColor(7,1);
                uint16_t cursorCol2 = zxColor(1,0);

                while (1) {

                    if (msgDelay == 0) {
                        nextChar = AboutMsg[Config::lang][msgIndex][msgChar];
                        if (nextChar != 13) {
                            if (nextChar == 10) {
                                char fore = AboutMsg[Config::lang][msgIndex][++msgChar];
                                char back = AboutMsg[Config::lang][msgIndex][++msgChar];
                                int foreint = (fore >= 'A') ? (fore - 'A' + 10) : (fore - '0');
                                int backint = (back >= 'A') ? (back - 'A' + 10) : (back - '0');
                                VIDEO::vga.setTextColor(zxColor(foreint & 0x7, foreint >> 3), zxColor(backint & 0x7, backint >> 3));
                                msgChar++;
                                continue;
                            } else {
                                VIDEO::vga.drawChar(pos_x + (osdCol * 6), pos_y + (osdRow * 8), nextChar);
                            }
                            msgChar++;
                        } else {
                            VIDEO::vga.fillRect(pos_x + (osdCol * 6), pos_y + (osdRow * 8), 6,8, zxColor(1, 0) );
                        }
                        osdCol++;
                        if (osdCol == 38) {
                            if (osdRow == 12) {
                                osdCol--;
                                msgDelay = 192;
                            } else {
                                VIDEO::vga.fillRect(pos_x + (osdCol * 6), pos_y + (osdRow * 8), 6,8, zxColor(1, 0) );
                                osdCol = 0;
                                msgChar++;
                                osdRow++;
                            }
                        }
                    } else {
                        msgDelay--;
                        if (msgDelay==0) {
                            VIDEO::vga.fillRect(Config::aspect_16_9 ? 60 : 40,Config::aspect_16_9 ? 64 : 84,240,114,zxColor(1, 0));
                            osdCol = 0;
                            osdRow  = 0;
                            msgChar = 0;
                            msgIndex++;
                            if (msgIndex==6) msgIndex = 0;
                        }
                    }

                    if (--cursorBlink == 0) {
                        uint16_t cursorSwap = cursorCol;
                        cursorCol = cursorCol2;
                        cursorCol2 = cursorSwap;
                        cursorBlink = 16;
                    }

                    VIDEO::vga.fillRect(pos_x + ((osdCol + 1) * 6), pos_y + (osdRow * 8), 6,8, cursorCol );
                    
                    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

                    ESPectrum::readKbdJoy();

                    if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
                        if (ESPectrum::readKbd(&Nextkey)) {
                            if(!Nextkey.down) continue;
                            if ((Nextkey.vk == fabgl::VK_F1) || (Nextkey.vk == fabgl::VK_ESCAPE) || (Nextkey.vk == fabgl::VK_RETURN)) break;
                        }
                    }

                    vTaskDelay(20 / portTICK_PERIOD_MS);

                }

                click();

                return;            

            }
            else break;
        }        
        }

    }

}

// Shows a red panel with error text
void OSD::errorPanel(string errormsg) {
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);

    if (Config::slog_on)
        printf((errormsg + "\n").c_str());

    VIDEO::vga.fillRect(x, y, OSD_W, OSD_H, zxColor(0, 0));
    VIDEO::vga.rect(x, y, OSD_W, OSD_H, zxColor(7, 0));
    VIDEO::vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, zxColor(2, 1));
    VIDEO::vga.setFont(Font6x8);
    osdHome();
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(2, 1));
    VIDEO::vga.print(ERROR_TITLE);
    osdAt(2, 0);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
    VIDEO::vga.println(errormsg.c_str());
    osdAt(17, 0);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(2, 1));
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

    const unsigned short h = OSD_FONT_H * 3;
    const unsigned short y = scrAlignCenterY(h);
    unsigned short paper;
    unsigned short ink;
    unsigned int j;

    if (msg.length() > (scrW / 6) - 4) msg = msg.substr(0,(scrW / 6) - 4);

    const unsigned short w = (msg.length() + 2) * OSD_FONT_W;
    const unsigned short x = scrAlignCenterX(w);

    switch (warn_level) {
    case LEVEL_OK:
        ink = zxColor(7, 1);
        paper = zxColor(4, 0);
        break;
    case LEVEL_ERROR:
        ink = zxColor(7, 1);
        paper = zxColor(2, 0);
        break;
    case LEVEL_WARN:
        ink = zxColor(0, 0);
        paper = zxColor(6, 0);
        break;
    default:
        ink = zxColor(7, 0);
        paper = zxColor(1, 0);
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

void OSD::HWInfo() {

    fabgl::VirtualKeyItem Nextkey;

    click();

    // Draw Hardware and memory info
    drawOSD(true);
    osdAt(2, 0);

    VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));

    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    VIDEO::vga.print(" Hardware info\n");
    VIDEO::vga.print(" --------------------------------------\n");

    // string chipmodel[6]={"","ESP32","ESP32-S2","","ESP32-S3","ESP32-C3"};
    // string textout = " Chip model    : " + chipmodel[chip_info.model] + "\n";
    // VIDEO::vga.print(textout.c_str());

    // Chip models for ESP32
    string textout = " Chip model    : ";
    uint32_t chip_ver = esp_efuse_get_pkg_ver();
    uint32_t pkg_ver = chip_ver & 0x7;
    switch (pkg_ver) {
        case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ6 :
            if (chip_info.revision == 3)
                textout += "ESP32-D0WDQ6-V3";  
            else
                textout += "ESP32-D0WDQ6";
            break;
        case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ5 :
            if (chip_info.revision == 3)
                textout += "ESP32-D0WD-V3";  
            else
                textout += "ESP32-D0WD";
            break;                
        case EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5 :
            textout += "ESP32-D2WD";
            break;            
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2 :
            textout += "ESP32-PICO-D2";
            break;            
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4 :
            textout += "ESP32-PICO-D4";
            break;            
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302 :
            textout += "ESP32-PICO-V3-02";
            break;            
        case /*EFUSE_RD_CHIP_VER_PKG_ESP32D0WDR2V3*/ 7 : // Not defined in ESP-IDF version we're using
             textout += "ESP32-D0WDR2-V3";
            break;             
        default:
            textout += "Unknown";
    }
    textout += "\n";
    VIDEO::vga.print(textout.c_str());    

    textout = " Chip cores    : " + to_string(chip_info.cores) + "\n";
    VIDEO::vga.print(textout.c_str());

    textout = " Chip revision : " + to_string(chip_info.revision) + "\n";
    VIDEO::vga.print(textout.c_str());

    textout = " Flash size    : " + to_string(spi_flash_get_chip_size() / (1024 * 1024)) + (chip_info.features & CHIP_FEATURE_EMB_FLASH ? "MB embedded" : "MB external") + "\n";
    VIDEO::vga.print(textout.c_str());

    multi_heap_info_t info;    
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    uint32_t psramsize = (info.total_free_bytes + info.total_allocated_bytes) >> 10;
    textout = " PSRAM size    : " + ( psramsize == 0 ? "N/A or disabled" : to_string(psramsize) + " MB") + "\n";
    VIDEO::vga.print(textout.c_str());

    textout = " IDF Version   : " + (string)(esp_get_idf_version()) + "\n";
    VIDEO::vga.print(textout.c_str());

    VIDEO::vga.print("\n Memory info\n");
    VIDEO::vga.print(" --------------------------------------\n");

    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task
    textout = " Total free bytes           : " + to_string(info.total_free_bytes) + "\n";
    VIDEO::vga.print(textout.c_str());

    textout = " Minimum free ever          : " + to_string(info.minimum_free_bytes) + "\n";
    VIDEO::vga.print(textout.c_str());    
    
    textout = " Largest free block         : " + to_string(info.largest_free_block) + "\n";
    VIDEO::vga.print(textout.c_str());
    
    textout = " Free (MALLOC_CAP_32BIT)    : " + to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT)) + "\n";
    VIDEO::vga.print(textout.c_str());
        
    UBaseType_t wm;
    wm = uxTaskGetStackHighWaterMark(ESPectrum::audioTaskHandle);
    textout = " Audio Task Stack HWM       : " + to_string(wm) + "\n";
    VIDEO::vga.print(textout.c_str());

    // wm = uxTaskGetStackHighWaterMark(loopTaskHandle);
    // printf("Loop Task Stack HWM: %u\n", wm);

    wm = uxTaskGetStackHighWaterMark(VIDEO::videoTaskHandle);
    textout = " Video Task Stack HWM       : " + (Config::videomode ? to_string(wm) : "N/A") + "\n";    
    VIDEO::vga.print(textout.c_str());

    // Wait for key
    while (1) {

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

        ESPectrum::readKbdJoy();

        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            ESPectrum::PS2Controller.keyboard()->getNextVirtualKey(&Nextkey);
            if(!Nextkey.down) continue;
            if (Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_F1) {
                click();
                break;
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

}

// // Switch boot partition
// string splabel;
// esp_err_t chg_ota;
// const esp_partition_t *ESPectrum_partition = NULL;

// ESPectrum_partition = esp_ota_get_running_partition();
// if (ESPectrum_partition->label=="128k") splabel = "48k"; else splabel= "128k";

// ESPectrum_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,splabel.c_str());
// chg_ota = esp_ota_set_boot_partition(ESPectrum_partition);
// if (chg_ota == ESP_OK ) esp_hard_reset();

#define FWBUFFSIZE 4096

esp_err_t OSD::updateFirmware(FILE *firmware) {

char ota_write_data[FWBUFFSIZE + 1] = { 0 };

// get the currently running partition
const esp_partition_t *partition = esp_ota_get_running_partition();
if (partition == NULL) {
    return ESP_ERR_NOT_FOUND;
}

// Grab next update target
const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
if (target == NULL) {
    return ESP_ERR_NOT_FOUND;
}

// printf("Running partition type %d subtype %d at offset 0x%x.\n", partition->type, partition->subtype, partition->address);
// printf("Target  partition type %d subtype %d at offset 0x%x.\n", target->type, target->subtype, target->address);

// osdCenteredMsg(OSD_FIRMW_BEGIN[Config::lang], LEVEL_INFO,0);


progressDialog(OSD_FIRMW[Config::lang],OSD_FIRMW_BEGIN[Config::lang],0,0);

// Fake erase progress bar ;D
delay(100);
for(int n=0; n <= 100; n += 10) {
    progressDialog("","",n,1);
    delay(100);
}

esp_ota_handle_t ota_handle;
esp_err_t result = esp_ota_begin(target, OTA_SIZE_UNKNOWN, &ota_handle);
if (result != ESP_OK) {
    progressDialog("","",0,2);
    return result;
}

size_t bytesread;
uint32_t byteswritten = 0;

// osdCenteredMsg(OSD_FIRMW_WRITE[Config::lang], LEVEL_INFO,0);
progressDialog(OSD_FIRMW[Config::lang],OSD_FIRMW_WRITE[Config::lang],0,1);

// Get firmware size
fseek(firmware, 0, SEEK_END);
long bytesfirmware = ftell(firmware);
rewind(firmware);

while (1) {
    bytesread = fread(ota_write_data, 1, 0x1000 , firmware);
    result = esp_ota_write(ota_handle,(const void *) ota_write_data, bytesread);
    if (result != ESP_OK) {
        progressDialog("","",0,2);
        return result;
    }
    byteswritten += bytesread;
    progressDialog("","",(float) 100 / ((float) bytesfirmware / (float) byteswritten),1);
    // printf("Bytes written: %d\n",byteswritten);
    if (feof(firmware)) break;
}

result = esp_ota_end(ota_handle);
if (result != ESP_OK) 
{
    // printf("esp_ota_end failed, err=0x%x.\n", result);
    progressDialog("","",0,2);
    return result;
}

result = esp_ota_set_boot_partition(target);
if (result != ESP_OK) {
    // printf("esp_ota_set_boot_partition failed, err=0x%x.\n", result);
    progressDialog("","",0,2);
    return result;
}

// osdCenteredMsg(OSD_FIRMW_END[Config::lang], LEVEL_INFO, 0);
progressDialog(OSD_FIRMW[Config::lang],OSD_FIRMW_END[Config::lang],100,1);

delay(1000);

// Firmware written: reboot
OSD::esp_hard_reset();

}

void OSD::progressDialog(string title, string msg, int percent, int action) {

    static unsigned short h;
    static unsigned short y;
    static unsigned short w;
    static unsigned short x;
    static unsigned short progress_x;    
    static unsigned short progress_y;        
    static unsigned int j;

    if (action == 0 ) { // SHOW

        h = (OSD_FONT_H * 6) + 2;
        y = scrAlignCenterY(h);

        if (msg.length() > (scrW / 6) - 4) msg = msg.substr(0,(scrW / 6) - 4);
        if (title.length() > (scrW / 6) - 4) title = title.substr(0,(scrW / 6) - 4);

        w = (((msg.length() > title.length() + 6 ? msg.length(): title.length() + 6) + 2) * OSD_FONT_W) + 2;
        x = scrAlignCenterX(w);

        // Save backbuffer data
        j = SaveRectpos;
        for (int  m = y; m < y + h; m++) {
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                VIDEO::SaveRect[SaveRectpos] = backbuffer32[n];
                SaveRectpos++;
            }
        }
        
        // printf("SaveRectPos: %04X\n",SaveRectpos << 2);        

        // Set font
        VIDEO::vga.setFont(Font6x8);

        // Menu border
        VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

        VIDEO::vga.fillRect(x + 1, y + 1, w - 2, OSD_FONT_H, zxColor(0,0));
        VIDEO::vga.fillRect(x + 1, y + 1 + OSD_FONT_H, w - 2, h - OSD_FONT_H - 2, zxColor(7,1));

        // Title
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));        
        VIDEO::vga.setCursor(x + OSD_FONT_W + 1, y + 1);
        VIDEO::vga.print(title.c_str());
        
        // Msg
        VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));        
        VIDEO::vga.setCursor(scrAlignCenterX(msg.length() * OSD_FONT_W), y + 1 + (OSD_FONT_H * 2));
        VIDEO::vga.print(msg.c_str());

        // Rainbow
        unsigned short rb_y = y + 8;
        unsigned short rb_paint_x = x + w - 30;
        uint8_t rb_colors[] = {2, 6, 4, 5};
        for (uint8_t c = 0; c < 4; c++) {
            for (uint8_t i = 0; i < 5; i++) {
                VIDEO::vga.line(rb_paint_x + i, rb_y, rb_paint_x + 8 + i, rb_y - 8, zxColor(rb_colors[c], 1));
            }
            rb_paint_x += 5;
        }

        // Progress bar frame
        progress_x = scrAlignCenterX(72);
        progress_y = y + (OSD_FONT_H * 4);
        VIDEO::vga.rect(progress_x, progress_y, 72, OSD_FONT_H + 2, zxColor(0, 0));
        progress_x++;
        progress_y++;

    } else if (action == 1 ) { // UPDATE

        // Msg
        VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));        
        VIDEO::vga.setCursor(scrAlignCenterX(msg.length() * OSD_FONT_W), y + 1 + (OSD_FONT_H * 2));
        VIDEO::vga.print(msg.c_str());

        // Progress bar
        int barsize = (70 * percent) / 100;
        VIDEO::vga.fillRect(progress_x, progress_y, barsize, OSD_FONT_H, zxColor(5,1));
        VIDEO::vga.fillRect(progress_x + barsize, progress_y, 70 - barsize, OSD_FONT_H, zxColor(7,1));        

    } else if (action == 2) { // CLOSE

        // Restore backbuffer data
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

uint8_t OSD::msgDialog(string title, string msg) {

    const unsigned short h = (OSD_FONT_H * 6) + 2;
    const unsigned short y = scrAlignCenterY(h);
    uint8_t res = DLG_NO;

    if (msg.length() > (scrW / 6) - 4) msg = msg.substr(0,(scrW / 6) - 4);
    if (title.length() > (scrW / 6) - 4) title = title.substr(0,(scrW / 6) - 4);

    const unsigned short w = (((msg.length() > title.length() + 6 ? msg.length() : title.length() + 6) + 2) * OSD_FONT_W) + 2;
    const unsigned short x = scrAlignCenterX(w);

    // Save backbuffer data
    unsigned int j = SaveRectpos;
    for (int  m = y; m < y + h; m++) {
        uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
        for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
            VIDEO::SaveRect[SaveRectpos] = backbuffer32[n];
            SaveRectpos++;
        }
    }
    // printf("SaveRectPos: %04X\n",SaveRectpos << 2);    

     // Set font
    VIDEO::vga.setFont(Font6x8);

    // Menu border
    VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

    VIDEO::vga.fillRect(x + 1, y + 1, w - 2, OSD_FONT_H, zxColor(0,0));
    VIDEO::vga.fillRect(x + 1, y + 1 + OSD_FONT_H, w - 2, h - OSD_FONT_H - 2, zxColor(7,1));

    // Title
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));        
    VIDEO::vga.setCursor(x + OSD_FONT_W + 1, y + 1);
    VIDEO::vga.print(title.c_str());
    
    // Msg
    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));        
    VIDEO::vga.setCursor(scrAlignCenterX(msg.length() * OSD_FONT_W), y + 1 + (OSD_FONT_H * 2));
    VIDEO::vga.print(msg.c_str());

    // Yes
    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));        
    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) - (w >> 2), y + 1 + (OSD_FONT_H * 4));
    VIDEO::vga.print(Config::lang ? "  Si  " : " Yes  ");

    // // Ruler
    // VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));        
    // VIDEO::vga.setCursor(x + 1, y + 1 + (OSD_FONT_H * 3));
    // VIDEO::vga.print("123456789012345678901234567");

    // No
    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) + (w >> 2), y + 1 + (OSD_FONT_H * 4));
    VIDEO::vga.print("  No  ");

    // Rainbow
    unsigned short rb_y = y + 8;
    unsigned short rb_paint_x = x + w - 30;
    uint8_t rb_colors[] = {2, 6, 4, 5};
    for (uint8_t c = 0; c < 4; c++) {
        for (uint8_t i = 0; i < 5; i++) {
            VIDEO::vga.line(rb_paint_x + i, rb_y, rb_paint_x + 8 + i, rb_y - 8, zxColor(rb_colors[c], 1));
        }
        rb_paint_x += 5;
    }

    // VIDEO::vga.fillRect(x, y, w, h, paper);
    // // VIDEO::vga.rect(x - 1, y - 1, w + 2, h + 2, ink);
    // VIDEO::vga.setTextColor(ink, paper);
    // VIDEO::vga.setFont(Font6x8);
    // VIDEO::vga.setCursor(x + OSD_FONT_W, y + OSD_FONT_H);
    // VIDEO::vga.print(msg.c_str());
    
    // Keyboard loop
    fabgl::VirtualKeyItem Menukey;
    while (1) {

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

        ESPectrum::readKbdJoy();

        // Process external keyboard
        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            if (ESPectrum::readKbd(&Menukey)) {
                if (!Menukey.down) continue;

                if (Menukey.vk == fabgl::VK_LEFT) {
                    // Yes
                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) - (w >> 2), y + 1 + (OSD_FONT_H * 4));
                    VIDEO::vga.print(Config::lang ? "  Si  " : " Yes  ");
                    // No
                    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));        
                    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) + (w >> 2), y + 1 + (OSD_FONT_H * 4));
                    VIDEO::vga.print("  No  ");
                    click();
                    res = DLG_YES;
                } else if (Menukey.vk == fabgl::VK_RIGHT) {
                    // Yes
                    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));        
                    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) - (w >> 2), y + 1 + (OSD_FONT_H * 4));
                    VIDEO::vga.print(Config::lang ? "  Si  " : " Yes  ");
                    // No
                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) + (w >> 2), y + 1 + (OSD_FONT_H * 4));
                    VIDEO::vga.print("  No  ");
                    click();
                    res = DLG_NO;
                } else if (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_SPACE) {
                    break;
                } else if (Menukey.vk == fabgl::VK_ESCAPE) {
                    res = DLG_CANCEL;
                    break;
                }
            }

        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

    click();

    // Restore backbuffer data
    SaveRectpos = j;
    for (int  m = y; m < y + h; m++) {
        uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
        for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
            backbuffer32[n] = VIDEO::SaveRect[j];
            j++;
        }
    }

    return res;

}

string OSD::inputBox(int x, int y, string text) {

return text;

}
