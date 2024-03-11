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
#include "roms.h"

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

static const uint8_t click48[12]={ 0,8,32,32,32,32,32,32,32,32,8,0 };

static const uint8_t click128[116] = {   0,8,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,8,0
                                    };

IRAM_ATTR void OSD::click() {

    size_t written;

    if (Config::tape_player) return; // Disable interface click on tape player mode

    pwm_audio_set_volume(ESP_VOLUME_MAX);

    if (Z80Ops::is48)
        pwm_audio_write((uint8_t *) click48, 12, &written,  5 / portTICK_PERIOD_MS);
    else
        pwm_audio_write((uint8_t *) click128, 116, &written, 5 / portTICK_PERIOD_MS);

    pwm_audio_set_volume(ESPectrum::aud_volume);

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

    VIDEO::vga.setTextColor(zxColor(7, 0), zxColor( ESPectrum::ESP_delay ? 1 : 2, 0));
    VIDEO::vga.setFont(Font6x8);
    VIDEO::vga.setCursor(x,y);
    VIDEO::vga.print(stats_lin1);
    VIDEO::vga.setCursor(x,y+8);
    VIDEO::vga.print(stats_lin2);

}

static bool persistSave(uint8_t slotnumber)
{
    struct stat stat_buf;
    char persistfname[sizeof(DISK_PSNA_FILE) + 7];
    char persistfinfo[sizeof(DISK_PSNA_FILE) + 7];    

    // printf(DISK_PSNA_FILE "%u.sna\n",slotnumber);
    // printf(DISK_PSNA_FILE "%u.esp\n",slotnumber);

    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    sprintf(persistfinfo,DISK_PSNA_FILE "%u.esp",slotnumber);
    string finfo = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfinfo;

    // Create dir if it doesn't exist
    string dir = FileUtils::MountPoint + DISK_PSNA_DIR;
    if (stat(dir.c_str(), &stat_buf) != 0) {
        if (mkdir(dir.c_str(),0775) != 0) {
            printf("persistSave: problem creating persist save dir\n");
            return false;
        }
    }

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
        return false;

    } else {

        fputs((Config::arch + "\n" + Config::romSet + "\n").c_str(),f);    // Put architecture and romset on info file
        fclose(f);    

        if (!FileSNA::save(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname)) OSD::osdCenteredMsg(OSD_PSNA_SAVE_ERR, LEVEL_WARN);
    
    }

    return true;

}

static bool persistLoad(uint8_t slotnumber)
{
    
    char persistfname[sizeof(DISK_PSNA_FILE) + 7];
    char persistfinfo[sizeof(DISK_PSNA_FILE) + 7];        

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

        fgets(buf, sizeof(buf),f);
        string persist_romset = buf;
        persist_romset.pop_back();
        // printf("[%s]\n",persist_romset.c_str());

        fclose(f);

        if (!LoadSnapshot(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname, persist_arch, persist_romset)) {
            OSD::osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
            return false;
        } else {
            Config::ram_file = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + persistfname;
            Config::last_ram_file = Config::ram_file;
        }
    }

    return true;

}

// OSD Main Loop
void OSD::do_OSD(fabgl::VirtualKey KeytoESP, bool CTRL) {

    static uint8_t last_sna_row = 0;
    fabgl::VirtualKeyItem Nextkey;

    if (CTRL) {

        if (KeytoESP == fabgl::VK_F2) { // Turbo mode
            ESPectrum::ESP_delay = !ESPectrum::ESP_delay;
        } else 
        if (KeytoESP == fabgl::VK_F9) { // Input Poke
            pokeDialog();
        } else 
        if (KeytoESP == fabgl::VK_F10) { // NMI
            Z80::triggerNMI();
        } else 
        if (KeytoESP == fabgl::VK_F3) { 
            // Test variable decrease
            ESPectrum::ESPtestvar -= 1;
        } else 
        if (KeytoESP == fabgl::VK_F4) {
            // Test variable increase
            ESPectrum::ESPtestvar += 1;
        } else 
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
                            if (Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2B || Nextkey.vk == fabgl::VK_PAUSE) {
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
                LoadSnapshot(fname,"","");
                Config::ram_file = fname;
                Config::last_ram_file = fname;
            }
        }
        else if (KeytoESP == fabgl::VK_F3) {
            menu_level = 0;
            menu_curopt = 1;
            // Persist Load
            string menuload = MENU_PERSIST_LOAD[Config::lang];
            for(int i=1; i <= 100; i++) {
                menuload += (Config::lang ? "Ranura " : "Slot ") + to_string(i) + "\n";
            }
            uint8_t opt2 = menuRun(menuload);
            if (opt2) {
                persistLoad(opt2);
            }
        }
        else if (KeytoESP == fabgl::VK_F4) {
            // Persist Save
            menu_level = 0;
            menu_curopt = 1;
            while (1) {
                string menusave = MENU_PERSIST_SAVE[Config::lang];
                for(int i=1; i <= 100; i++) {
                    menusave += (Config::lang ? "Ranura " : "Slot ") + to_string(i) + "\n";
                }
                uint8_t opt2 = menuRun(menusave);
                if (opt2) {
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

                if ((keySel ==  "R") && (Config::flashload) && (Config::romSet != "ZX81+") && (Config::romSet != "48Kcs") && (Config::romSet != "128Kcs")) {

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
                        }
                        Config::last_ram_file = NO_RAM_FILE;

                }

                Tape::TAP_Stop();

                // Read and analyze tape file
                Tape::TAP_Open(mFile);

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
            if ((VIDEO::OSD & 0x03) == 0)
                VIDEO::OSD |= Tape::tapeStatus == TAPE_LOADING ? 1 : 2;
            else
                VIDEO::OSD++;

            if ((VIDEO::OSD & 0x03) > 2) {
                if ((VIDEO::OSD & 0x04) == 0) {
                    if (Config::aspect_16_9) 
                        VIDEO::DrawOSD169 = Z80Ops::isPentagon ? VIDEO::MainScreen_Pentagon : VIDEO::MainScreen;
                    else
                        VIDEO::DrawOSD43 = Z80Ops::isPentagon ? VIDEO::BottomBorder_Pentagon :  VIDEO::BottomBorder;
                }
                VIDEO::OSD &= 0xfc;
            } else {

                if ((VIDEO::OSD & 0x04) == 0) {
                    if (Config::aspect_16_9) 
                        VIDEO::DrawOSD169 = Z80Ops::isPentagon ? VIDEO::MainScreen_OSD_Pentagon : VIDEO::MainScreen_OSD;
                    else
                        VIDEO::DrawOSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;

                    OSD::drawStats();
                }

                ESPectrum::TapeNameScroller = 0;

            }

            click();

        }
        else if (KeytoESP == fabgl::VK_F9) { 

            if (VIDEO::OSD == 0) {

                if (Config::aspect_16_9) 
                    VIDEO::DrawOSD169 = Z80Ops::isPentagon ? VIDEO::MainScreen_OSD_Pentagon : VIDEO::MainScreen_OSD;
                else
                    VIDEO::DrawOSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;

                VIDEO::OSD = 0x04;

            } else VIDEO::OSD |= 0x04;

            ESPectrum::totalseconds = 0;
            ESPectrum::totalsecondsnodelay = 0;
            VIDEO::framecnt = 0;

            if (ESPectrum::aud_volume>ESP_VOLUME_MIN) {
                ESPectrum::aud_volume--;
                pwm_audio_set_volume(ESPectrum::aud_volume);
            }

            unsigned short x,y;

            if (Config::aspect_16_9) {
                x = 156;
                y = 180;
            } else {
                x = 168;
                y = 224;
            }

            VIDEO::vga.fillRect(x ,y - 4, 24 * 6, 16, zxColor(1, 0));
            VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));
            VIDEO::vga.setFont(Font6x8);
            VIDEO::vga.setCursor(x + 4,y + 1);
            VIDEO::vga.print(Config::tape_player ? "TAP" : "VOL");
            for (int i = 0; i < ESPectrum::aud_volume + 16; i++)
                VIDEO::vga.fillRect(x + 26 + (i * 7) , y + 1, 6, 7, zxColor( 7, 0));                

        }
        else if (KeytoESP == fabgl::VK_F10) { 

            if (VIDEO::OSD == 0) {

                if (Config::aspect_16_9) 
                    VIDEO::DrawOSD169 = Z80Ops::isPentagon ? VIDEO::MainScreen_OSD_Pentagon : VIDEO::MainScreen_OSD;
                else
                    VIDEO::DrawOSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;

                VIDEO::OSD = 0x04;

            } else VIDEO::OSD |= 0x04;

            ESPectrum::totalseconds = 0;
            ESPectrum::totalsecondsnodelay = 0;
            VIDEO::framecnt = 0;

            if (ESPectrum::aud_volume<ESP_VOLUME_MAX) {
                ESPectrum::aud_volume++;
                pwm_audio_set_volume(ESPectrum::aud_volume);
            }

            unsigned short x,y;
            if (Config::aspect_16_9) {
                x = 156;
                y = 180;
            } else {
                x = 168;
                y = 224;
            }

            VIDEO::vga.fillRect(x ,y - 4, 24 * 6, 16, zxColor(1, 0));
            VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));
            VIDEO::vga.setFont(Font6x8);
            VIDEO::vga.setCursor(x + 4,y + 1);
            VIDEO::vga.print(Config::tape_player ? "TAP" : "VOL");
            for (int i = 0; i < ESPectrum::aud_volume + 16; i++)
                VIDEO::vga.fillRect(x + 26 + (i * 7) , y + 1, 6, 7, zxColor( 7, 0));                                

        }
        else if (KeytoESP == fabgl::VK_F11) { // Hard reset
            if (Config::ram_file != NO_RAM_FILE) {
                Config::ram_file = NO_RAM_FILE;
            }
            Config::last_ram_file = NO_RAM_FILE;
            ESPectrum::reset();
        }
        else if (KeytoESP == fabgl::VK_F12) { // ESP32 reset
            // ESP host reset
            Config::ram_file = NO_RAM_FILE;
            Config::save("ram");
            esp_hard_reset();
        }
        else if (KeytoESP == fabgl::VK_F1) {

            menu_curopt = 1;
            
            while(1) {

            // Main menu
            menu_saverect = false;
            menu_level = 0;
            uint8_t opt = menuRun("ESPectrum " + Config::arch + "\n" + MENU_MAIN[Config::lang]);
    
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
                                LoadSnapshot(fname,"","");
                                Config::ram_file = fname;
                                Config::last_ram_file = fname;
                                return;
                            }
                        }
                        else if (sna_mnu == 2) {
                            // Persist Load
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                string menuload = MENU_PERSIST_LOAD[Config::lang];
                                for(int i=1; i <= 100; i++) {
                                    menuload += (Config::lang ? "Ranura " : "Slot ") + to_string(i) + "\n";
                                }
                                uint8_t opt2 = menuRun(menuload);
                                if (opt2) {
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
                                string menusave = MENU_PERSIST_SAVE[Config::lang];
                                for(int i=1; i <= 100; i++) {
                                    menusave += (Config::lang ? "Ranura " : "Slot ") + to_string(i) + "\n";
                                }
                                uint8_t opt2 = menuRun(menusave);
                                if (opt2) {
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
                                        }
                                        Config::last_ram_file = NO_RAM_FILE;

                                }

                                Tape::TAP_Stop();

                                // Read and analyze tape file
                                // Tape::Open(FileUtils::MountPoint + "/" + FileUtils::TAP_Path + "/" + mFile);
                                Tape::TAP_Open(mFile);
                                
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
                        else if (tap_num == 4) {

                            menu_level = 2;
                            menu_curopt = 1;                    
                            menu_saverect = true;
                            while (1) {
                                string Mnustr = MENU_TAPEPLAYER[Config::lang];
                                Mnustr += MENU_YESNO[Config::lang];
                                bool prev_opt = Config::tape_player;
                                if (prev_opt) {
                                    Mnustr.replace(Mnustr.find("[Y",0),2,"[*");
                                    Mnustr.replace(Mnustr.find("[N",0),2,"[ ");                        
                                } else {
                                    Mnustr.replace(Mnustr.find("[Y",0),2,"[ ");
                                    Mnustr.replace(Mnustr.find("[N",0),2,"[*");                        
                                }

                                uint8_t opt2 = menuRun(Mnustr);
                                if (opt2) {
                                    if (opt2 == 1)
                                        Config::tape_player = true;
                                    else
                                        Config::tape_player = false;

                                    if (Config::tape_player != prev_opt) {
                                        if (Config::tape_player) {
                                            ESPectrum::aud_volume = ESP_VOLUME_MAX;
                                            pwm_audio_set_volume(ESPectrum::aud_volume);
                                        }
                                        Config::save("tape_player");
                                    }
                                    menu_curopt = opt2;
                                    menu_saverect = false;
                                } else {
                                    menu_curopt = 4;
                                    menu_level = 1;                                       
                                    break;
                                }

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
                // MACHINE MENU
                // ***********************************************************************************
                menu_saverect = true;
                menu_curopt = 1;
                while (1) {
                    menu_level = 1;
                    uint8_t arch_num = menuRun(MENU_ARCH[Config::lang]);
                    if (arch_num) {
                        string arch = Config::arch;
                        string romset = Config::romSet;
                        uint8_t opt2 = 0;
                        if (arch_num == 1) { // 48K
                            menu_level = 2;
                            menu_curopt = 1;                    
                            menu_saverect = true;
                            opt2 = menuRun(MENU_ROMS48[Config::lang]);
                            if (opt2) {
                                arch = "48K";
                                if (opt2 == 1) {
                                    romset = "48K";
                                } else 
                                if (opt2 == 2) {
                                    romset = "48Kes";
                                } else 
                                if (opt2 == 3) {
                                    romset = "48Kcs";
                                }
                                menu_curopt = opt2;
                                menu_saverect = false;
                            } else {
                                menu_curopt = 1;
                                menu_level = 2;                                       
                            }
                        } else if (arch_num == 2) { // 128K
                            menu_level = 2;
                            menu_curopt = 1;                    
                            menu_saverect = true;
                            opt2 = menuRun(MENU_ROMS128[Config::lang]);
                            if (opt2) {
                                arch = "128K";
                                if (opt2 == 1) {
                                    romset = "128K";
                                } else 
                                if (opt2 == 2) {
                                    romset = "128Kes";
                                } else 
                                if (opt2 == 3) {
                                    romset = "+2";
                                } else
                                if (opt2 == 4) {
                                    romset = "+2es";
                                } else
                                if (opt2 == 5) {
                                    romset = "ZX81+";
                                } else
                                if (opt2 == 6) {
                                    romset = "128Kcs";
                                }
                                menu_curopt = opt2;
                                menu_saverect = false;
                            } else {
                                menu_curopt = 1;
                                menu_level = 2;                                       
                            }
                        } else if (arch_num == 3) {
                            arch = "Pentagon";
                            romset = "Pentagon";
                            opt2 = 1;
                        }

                        if (opt2) {

                            if (arch != Config::arch || romset != Config::romSet) {

                                Config::ram_file = "none";
                                Config::save("ram");

                               
                                if (romset != Config::romSet) {

                                    if (arch == "48K") {
                                        
                                        if (Config::pref_romSet_48 == "Last") {

                                            Config::romSet = romset;
                                            Config::save("romSet");
                                            Config::romSet48 = romset;
                                            Config::save("romSet48");

                                        }

                                    } else if (arch == "128K") {

                                        if (Config::pref_romSet_128 == "Last") {

                                            Config::romSet = romset;
                                            Config::save("romSet");
                                            Config::romSet128 = romset;
                                            Config::save("romSet128");

                                        }

                                    }
                                }

                                if (arch != Config::arch) {

                                    if (Config::pref_arch == "Last") {
                                        Config::arch = arch;
                                        Config::save("arch");
                                    }

                                    if (Config::videomode) {
                                        Config::pref_arch += "R";
                                        Config::save("pref_arch");
                                        Config::arch = arch;
                                        Config::save("arch");
                                        Config::romSet = romset;
                                        Config::save("romSet");
                                        Config::romSet48 = romset;
                                        Config::save("romSet48");
                                        esp_hard_reset();
                                    }

                                }

                                Config::requestMachine(arch, romset);

                            }
                        
                            ESPectrum::reset();

                            return;

                        }

                        menu_curopt = arch_num;
                        menu_saverect = false;

                    } else {
                        menu_curopt = 4;                            
                        break;
                    }

                }
            }
            else if (opt == 5) {
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
                            LoadSnapshot(Config::last_ram_file,"","");
                            Config::ram_file = Config::last_ram_file;
                        } else ESPectrum::reset();
                        return;
                    }
                    else if (opt2 == 2) {
                        // Hard
                        if (Config::ram_file != NO_RAM_FILE) {
                            Config::ram_file = NO_RAM_FILE;
                        }
                        Config::last_ram_file = NO_RAM_FILE;
                        ESPectrum::reset();
                        return;
                    }
                    else if (opt2 == 3) {
                        // ESP host reset
                        Config::ram_file = NO_RAM_FILE;
                        Config::save("ram");
                        esp_hard_reset();
                    } else {
                        menu_curopt = 5;
                        break;
                    }
                }
            }
            else if (opt == 6) {
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
                            string stor_menu = MENU_STORAGE[Config::lang];
                            uint8_t opt2 = menuRun(stor_menu);
                            if (opt2) {
                                if (opt2 == 1) {
                                    menu_level = 3;
                                    menu_curopt = 1;                    
                                    menu_saverect = true;
                                    while (1) {
                                        string flash_menu = MENU_FLASHLOAD[Config::lang];
                                        flash_menu += MENU_YESNO[Config::lang];
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
                                else if (opt2 == 2) {
                                    menu_level = 3;
                                    menu_curopt = 1;                    
                                    menu_saverect = true;
                                    while (1) {
                                        string mnu_str = MENU_RGTIMINGS[Config::lang];
                                        mnu_str += MENU_YESNO[Config::lang];
                                        bool prev_opt = Config::tape_timing_rg;
                                        if (prev_opt) {
                                            mnu_str.replace(mnu_str.find("[Y",0),2,"[*");
                                            mnu_str.replace(mnu_str.find("[N",0),2,"[ ");                        
                                        } else {
                                            mnu_str.replace(mnu_str.find("[Y",0),2,"[ ");
                                            mnu_str.replace(mnu_str.find("[N",0),2,"[*");                        
                                        }
                                        uint8_t opt2 = menuRun(mnu_str);
                                        if (opt2) {
                                            if (opt2 == 1)
                                                Config::tape_timing_rg = true;
                                            else
                                                Config::tape_timing_rg = false;

                                            if (Config::tape_timing_rg != prev_opt) {
                                                Config::save("tape_timing_rg");
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
                        while (1) {
                            string archprefmenu = MENU_ARCH_PREF[Config::lang];
                            string prev_archpref = Config::pref_arch;
                            if (Config::pref_arch == "48K") {
                                archprefmenu.replace(archprefmenu.find("[4",0),2,"[*");
                                archprefmenu.replace(archprefmenu.find("[1",0),2,"[ ");                        
                                archprefmenu.replace(archprefmenu.find("[P",0),2,"[ ");                        
                                archprefmenu.replace(archprefmenu.find("[L",0),2,"[ ");                                                        
                            } else if (Config::pref_arch == "128K") {
                                archprefmenu.replace(archprefmenu.find("[4",0),2,"[ ");
                                archprefmenu.replace(archprefmenu.find("[1",0),2,"[*");                        
                                archprefmenu.replace(archprefmenu.find("[P",0),2,"[ ");                        
                                archprefmenu.replace(archprefmenu.find("[L",0),2,"[ ");                                                        
                            } else if (Config::pref_arch == "Pentagon") {
                                archprefmenu.replace(archprefmenu.find("[4",0),2,"[ ");
                                archprefmenu.replace(archprefmenu.find("[1",0),2,"[ ");
                                archprefmenu.replace(archprefmenu.find("[P",0),2,"[*");                        
                                archprefmenu.replace(archprefmenu.find("[L",0),2,"[ ");                                                        
                            } else {
                                archprefmenu.replace(archprefmenu.find("[4",0),2,"[ ");
                                archprefmenu.replace(archprefmenu.find("[1",0),2,"[ ");
                                archprefmenu.replace(archprefmenu.find("[P",0),2,"[ ");
                                archprefmenu.replace(archprefmenu.find("[L",0),2,"[*");
                            }
                            uint8_t opt2 = menuRun(archprefmenu);
                            if (opt2) {

                                if (opt2 == 1)
                                    Config::pref_arch = "48K";
                                else
                                if (opt2 == 2)
                                    Config::pref_arch = "128K";
                                else
                                if (opt2 == 3)
                                    Config::pref_arch = "Pentagon";
                                else
                                if (opt2 == 4)
                                    Config::pref_arch = "Last";

                                if (Config::pref_arch != prev_archpref) {
                                    Config::save("pref_arch");
                                }

                                menu_curopt = opt2;
                                menu_saverect = false;

                            } else {
                                menu_curopt = 2;
                                break;
                            }
                        }
                    }
                    else if (options_num == 3) {
                        menu_level = 2;
                        menu_curopt = 1;
                        menu_saverect = true;
                        while (1) {
                            uint8_t opt2 = menuRun(MENU_ROM_PREF[Config::lang]);
                            if (opt2) {
                                if (opt2 == 1) {
                                    menu_level = 3;
                                    menu_curopt = 1;                    
                                    menu_saverect = true;
                                    while (1) {
                                        
                                        string rpref48_menu = MENU_ROM_PREF_48[Config::lang];

                                        // printf("%s\n",Config::pref_romSet_48.c_str());

                                        int mpos = -1;
                                        while(1) {
                                            mpos = rpref48_menu.find("[",mpos + 1);
                                            if (mpos == string::npos) break;
                                            string rmenu = rpref48_menu.substr(mpos + 1, 5);
                                            trim(rmenu);
                                            if (rmenu == Config::pref_romSet_48) 
                                                rpref48_menu.replace(mpos + 1, 5,"*");
                                            else
                                                rpref48_menu.replace(mpos + 1,5," ");
                                        }

                                        // printf("%s\n",rpref48_menu.c_str());

                                        string prev_rpref48 = Config::pref_romSet_48;
                                        uint8_t opt2 = menuRun(rpref48_menu);
                                        if (opt2) {

                                            if (opt2 == 1)
                                                Config::pref_romSet_48 = "48K";
                                            else
                                            if (opt2 == 2)
                                                Config::pref_romSet_48 = "48Kes";
                                            else
                                            if (opt2 == 3)
                                                Config::pref_romSet_48 = "48Kcs";
                                            else
                                            if (opt2 == 4)
                                                Config::pref_romSet_48 = "Last";

                                            if (Config::pref_romSet_48 != prev_rpref48) {
                                                Config::save("pref_romSet_48");
                                            }

                                            menu_curopt = opt2;
                                            menu_saverect = false;

                                        } else {
                                            menu_curopt = 1;
                                            menu_level = 2;                                       
                                            break;
                                        }
                                    }
                                } else if (opt2 == 2) {
                                    menu_level = 3;
                                    menu_curopt = 1;                    
                                    menu_saverect = true;
                                    while (1) {
                                        string rpref128_menu = MENU_ROM_PREF_128[Config::lang];

                                        // printf("%s\n",Config::pref_romSet_128.c_str());

                                        int mpos = -1;
                                        while(1) {
                                            mpos = rpref128_menu.find("[",mpos + 1);
                                            if (mpos == string::npos) break;
                                            string rmenu = rpref128_menu.substr(mpos + 1, 6);
                                            trim(rmenu);
                                            if (rmenu == Config::pref_romSet_128) 
                                                rpref128_menu.replace(mpos + 1, 6,"*");
                                            else
                                                rpref128_menu.replace(mpos + 1,6," ");
                                        }

                                        // printf("%s\n",rpref128_menu.c_str());

                                        string prev_rpref128 = Config::pref_romSet_128;
                                        uint8_t opt2 = menuRun(rpref128_menu);
                                        if (opt2) {

                                            if (opt2 == 1)
                                                Config::pref_romSet_128 = "128K";
                                            else
                                            if (opt2 == 2)
                                                Config::pref_romSet_128 = "128Kes";
                                            else
                                            if (opt2 == 3)
                                                Config::pref_romSet_128 = "+2";
                                            else
                                            if (opt2 == 4)
                                                Config::pref_romSet_128 = "+2es";
                                            else
                                            if (opt2 == 5)
                                                Config::pref_romSet_128 = "ZX81+";
                                            else
                                            if (opt2 == 6)
                                                Config::pref_romSet_128 = "128Kcs";
                                            else
                                            if (opt2 == 7)
                                                Config::pref_romSet_128 = "Last";

                                            if (Config::pref_romSet_128 != prev_rpref128) {
                                                Config::save("pref_romSet_128");
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
                                menu_curopt = 3;                            
                                break;
                            }
                        }
                    }                          
                    else if (options_num == 6) {
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
                                    Config::ram_file = "none";
                                    Config::save("asp169");
                                    Config::save("ram");
                                    esp_hard_reset();
                                }

                                menu_curopt = opt2;
                                menu_saverect = false;

                            } else {
                                menu_curopt = 6;
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
                            uint8_t opt2 = menuRun(Mnustr);
                            if (opt2) {
                                // Joystick customization 
                                menu_level = 3;
                                menu_curopt = 1;
                                menu_saverect = true;
                                while (1) {
                                    string joy_menu = MENU_DEFJOY[Config::lang];
                                    joy_menu.replace(joy_menu.find("#",0),1,(string)" " + char(48 + opt2)); 
                                    std::size_t pos = joy_menu.find("[",0);
                                    int nfind = 0;
                                    while (pos != string::npos) {
                                        if (nfind == (opt2 == 1 ? Config::joystick1 : Config::joystick2)) {
                                            joy_menu.replace(pos,2,"[*");
                                            break;
                                        }
                                        pos = joy_menu.find("[",pos + 1);
                                        nfind++;
                                    }
                                    uint8_t optjoy = menuRun(joy_menu);
                                    if (optjoy>0 && optjoy<6) {
                                        if (opt2 == 1) {                                        
                                            Config::joystick1 = optjoy - 1;
                                            Config::save("joystick1");
                                        } else {
                                            Config::joystick2 = optjoy - 1;
                                            Config::save("joystick2");
                                        }
                                        Config::setJoyMap(opt2,optjoy - 1);
                                        menu_curopt = optjoy;
                                        menu_saverect = false;
                                    } else if (optjoy == 6) {
                                        joyDialog(opt2);
                                        return;
                                    } else {
                                        menu_curopt = opt2;
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
                    else if (options_num == 5) {
                        menu_level = 2;
                        menu_curopt = 1;
                        menu_saverect = true;
                        while (1) {
                            // joystick
                            string Mnustr = MENU_JOYPS2[Config::lang];
                            std::size_t pos = Mnustr.find("[",0);
                            int nfind = 0;
                            while (pos != string::npos) {
                                if (nfind == Config::joyPS2) {
                                    Mnustr.replace(pos,2,"[*");
                                    break;
                                }
                                pos = Mnustr.find("[",pos + 1);
                                nfind++;
                            }
                            uint8_t opt2 = menuRun(Mnustr);
                            if (opt2 > 0 &&  opt2 < 6) {
                                if (Config::joyPS2 != (opt2 - 1)) {
                                    Config::joyPS2 = opt2 - 1;
                                    Config::save("joyPS2");
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
                                        csasjoy_menu += MENU_YESNO[Config::lang];
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
                                            menu_curopt = 6;
                                            menu_level = 2;                                       
                                            break;
                                        }
                                    }

                                } else {
                                    menu_curopt = 5;
                                    break;
                                }
                            }
                        }
                    }
                    else if (options_num == 8) {
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
                                menu_curopt = 8;
                                break;
                            }
                        }
                    }
                    else if (options_num == 7) {
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
                                        ay_menu += MENU_YESNO[Config::lang];
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
                                        iss_menu += MENU_YESNO[Config::lang];
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
                                menu_curopt = 7;
                                break;
                            }
                        }
                    } else if (options_num == 9) {
                        menu_level = 2;
                        menu_curopt = 1;
                        menu_saverect = true;
                        while (1) {
                            // Update
                            string Mnustr = MENU_UPDATE_FW[Config::lang];
                            uint8_t opt2 = menuRun(Mnustr);
                            if (opt2) {
                                // Update
                                if (opt2 == 1) {

                                    string title = OSD_FIRMW_UPDATE[Config::lang];
                                    string msg = OSD_DLG_SURE[Config::lang];
                                    uint8_t res = msgDialog(title,msg);

                                    if (res == DLG_YES) {

                                        // Open firmware file
                                        FILE *firmware = fopen("/sd/firmware.upg", "rb");
                                        if (firmware == NULL) {
                                            osdCenteredMsg(OSD_NOFIRMW_ERR[Config::lang], LEVEL_WARN, 2000);
                                        } else {
                                            esp_err_t res = updateFirmware(firmware);
                                            fclose(firmware);
                                            string errMsg = OSD_FIRMW_ERR[Config::lang];
                                            errMsg += " Code = " + to_string(res);
                                            osdCenteredMsg(errMsg, LEVEL_ERROR, 3000);
                                        }
                                    
                                    } else {
                                        menu_curopt = 1;
                                        menu_level = 2;                                       
                                        menu_saverect = false;
                                    }

                                } else if (opt2 == 2) {

                                    string title = OSD_ROM[Config::lang];
                                    title += " 48K   ";            
                                    string msg = OSD_DLG_SURE[Config::lang];
                                    uint8_t res = msgDialog(title,msg);

                                    if (res == DLG_YES) {

                                        // Flash custom ROM 48K
                                        FILE *customrom = fopen("/sd/48custom.rom", "rb");
                                        if (customrom == NULL) {
                                            osdCenteredMsg(OSD_NOROMFILE_ERR[Config::lang], LEVEL_WARN, 2000);
                                        } else {
                                            esp_err_t res = updateROM(customrom, 1);
                                            fclose(customrom);
                                            string errMsg = OSD_ROM_ERR[Config::lang];
                                            errMsg += " Code = " + to_string(res);
                                            osdCenteredMsg(errMsg, LEVEL_ERROR, 3000);
                                        }

                                    } else {
                                        menu_curopt = 2;
                                        menu_level = 2;                                       
                                        menu_saverect = false;
                                    }

                                } else if (opt2 == 3) {                                    

                                    string title = OSD_ROM[Config::lang];
                                    title += " 128K  ";
                                    string msg = OSD_DLG_SURE[Config::lang];
                                    uint8_t res = msgDialog(title,msg);

                                    if (res == DLG_YES) {

                                        // Flash custom ROM 128K
                                        FILE *customrom = fopen("/sd/128custom.rom", "rb");
                                        if (customrom == NULL) {
                                            osdCenteredMsg(OSD_NOROMFILE_ERR[Config::lang], LEVEL_WARN, 2000);
                                        } else {
                                            esp_err_t res = updateROM(customrom, 2);
                                            fclose(customrom);
                                            string errMsg = OSD_ROM_ERR[Config::lang];
                                            errMsg += " Code = " + to_string(res);
                                            osdCenteredMsg(errMsg, LEVEL_ERROR, 3000);
                                        }

                                    } else {
                                        menu_curopt = 3;
                                        menu_level = 2;                                       
                                        menu_saverect = false;
                                    }

                                }

                            } else {
                                menu_curopt = 9;
                                break;
                            }
                        }

                    } else {
                        menu_curopt = 6;
                        break;
                    }
                }
            }
            else if (opt == 7) {
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
                            if (Nextkey.vk == fabgl::VK_F1 || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY2B) break;
                        }
                    }

                    vTaskDelay(5 / portTICK_PERIOD_MS);

                }

                click();

                return;

            }        
            else if (opt == 8) {

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
                            if (msgIndex==8) msgIndex = 0;
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
                            if (Nextkey.vk == fabgl::VK_F1 || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY2B) break;                            
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

    if (msg.length() > (scrW / 6) - 10) msg = msg.substr(0,(scrW / 6) - 10);

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
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                VIDEO::SaveRect[SaveRectpos] = backbuffer32[n];
                SaveRectpos++;
            }
        }
        // printf("Saverectpos: %04X\n",SaveRectpos << 2);

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
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
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
            if (Nextkey.vk == fabgl::VK_F1 || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY2B) {
                click();
                break;
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

}

#define FWBUFFSIZE 4096

esp_err_t OSD::updateROM(FILE *customrom, uint8_t arch) {

    // get the currently running partition
    const esp_partition_t *partition = esp_ota_get_running_partition();
    if (partition == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    // printf("Running partition:\n");
    // printf("address: 0x%lX\n", partition->address);
    // printf("size: %ld\n", partition->size); // size of partition, not binary
    // printf("partition label: %s\n", partition->label);

    // Grab next update target
    // const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);

    // char splabel[17]="esp0";
    // if (strcmp(partition->label,splabel)==0) strcpy(splabel,"esp1"); else strcpy(splabel,"esp1");
    // const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY, (const char *) splabel);

    string splabel;
    if (strcmp(partition->label,"esp0")==0) splabel = "esp1"; else splabel= "esp0";
    const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,splabel.c_str());
    if (target == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    // printf("Running partition %s type %d subtype %d at offset 0x%x.\n", partition->label, partition->type, partition->subtype, partition->address);
    // printf("Target  partition %s type %d subtype %d at offset 0x%x.\n", target->label, target->type, target->subtype, target->address);
    
    // Get firmware size
    fseek(customrom, 0, SEEK_END);
    long bytesfirmware = ftell(customrom);
    rewind(customrom);

    // printf("Custom ROM lenght: %ld\n", bytesfirmware);

    string dlgTitle = OSD_ROM[Config::lang];

    if (arch == 1) {

        // Check rom size
        if (bytesfirmware > 0x4000) {
            return ESP_ERR_INVALID_SIZE;
        }

        dlgTitle += " 48K   ";

    } else {

        // Check rom size
        if (bytesfirmware != 0x4000 && bytesfirmware != 0x8000) {
            return ESP_ERR_INVALID_SIZE;
        }

        dlgTitle += " 128K  ";
    }        

    uint8_t data[FWBUFFSIZE] = { 0 };

    // printf("MAGIC -> ");
    // for(int i=0;i<8;i++)
    //     printf("%c",magic[i]);
    // printf("\n");

    int sindex = 0;
    int sindex128 = 0;
    uint32_t rom_off;
    uint32_t rom_off_128;
    uint8_t magic[8] =	{ 0x45, 0x53, 0x50, 0x52, 0x00, 0x34, 0x38, 0x4B }; // MAGIC -> "ESPR_48K" ;
    uint8_t magic128[8] = { 0x45, 0x53, 0x50, 0x52, 0x00, 0x31, 0x32, 0x38 }; // MAGIC -> "ESPR_128"

    // for (int n=0; n<8; n++) {
    //     magic[n] = gb_rom_0_48k_custom[n];
    //     magic128[n] = gb_rom_0_128k_custom[n];
    // }

    magic[4] = 0x5F;
    magic128[4] = 0x5F;    

    progressDialog(dlgTitle,OSD_ROM_BEGIN[Config::lang],0,0);

    for (uint32_t offset = 0; offset < partition->size; offset+=FWBUFFSIZE) {
        esp_err_t result = esp_partition_read(partition, offset, data, FWBUFFSIZE);
        if (result == ESP_OK) {    
            for (int n=0; n < FWBUFFSIZE; n++) {
                if (data[n] == magic[sindex]) {
                    sindex++;
                    if (sindex == 8) {
                        rom_off = offset + n - 7;
                        // printf("FOUND! OFFSET -> %ld\n",rom_off);
                        // for (int m = 0; m < 256; m+=16) {
                        //     for (int j = m; j < m + 16; j++) {
                        //         printf("%02X ", data[ n + j + 1]);
                        //     }
                        //     printf("\n");  
                        // }
                    }
                } else {
                    sindex = 0;
                }
                if (data[n] == magic128[sindex128]) {
                    sindex128++;
                    if (sindex128 == 8) {
                        rom_off_128 = offset + n - 7;
                        // printf("FOUND! OFFSET 128 -> %ld\n",rom_off_128);
                        // for (int m = 0; m < 256; m+=16) {
                        //     for (int j = m; j < m + 16; j++) {
                        //         printf("%02X ", data[ n + j + 1]);
                        //     }
                        //     printf("\n");  
                        // }
                    }
                } else {
                    sindex128 = 0;
                }
            }
        } else {
            printf("esp_partition_read failed, err=0x%x.\n", result);
            progressDialog("","",0,2); 
            return result;
        }
    }

    // Fake erase progress bar ;D
    delay(100);
    for(int n=0; n <= 100; n += 10) {
        progressDialog("","",n,1);
        delay(100);
    }

    // Erase entire target partition
    esp_err_t result = esp_partition_erase_range(target, 0, target->size);
    if (result != ESP_OK) {
        printf("esp_partition_erase_range failed, err=0x%x.\n", result);
        progressDialog("","",0,2); 
        return result;
    }


    // Copy active to target injecting new custom roms
    uint32_t psize = partition->size;

    // printf("Before -> %ld\n",psize);

    rom_off += 8;
    rom_off_128 += 8;    

    // FILE *file;
    // file = fopen("/sd/firmware.out", "wb");
    // if (file==NULL) {
    //     printf("FileSNA: Error opening firmware.out for writing");
    //     return;
    // }

    
    progressDialog(dlgTitle,OSD_ROM_WRITE[Config::lang],0,1);

    for(uint32_t i=0; i < partition->size; i += FWBUFFSIZE) {

            esp_err_t result = esp_partition_read(partition, i, data, FWBUFFSIZE);
            if (result == ESP_OK) {    

                for(int m=i; m < i + FWBUFFSIZE; m++) {

                    if (m >= rom_off && m<(rom_off + 0x4000)) {
                        data[m - i]=0xff;
                    } else if (m >= rom_off_128 && m<(rom_off_128 + 0x8000)) {
                        data[m - i]=0xff;
                    }

                }

                // Write the data, starting from the beginning of the partition
                esp_err_t result = esp_partition_write(target, i, data, FWBUFFSIZE);
                if (result != ESP_OK) {    
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                    progressDialog("","",0,2); 
                    return result;
                }

                // for (int m=0;m<FWBUFFSIZE;m++)
                //     writeByteFile(data[m], file);

            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
                progressDialog("","",0,2); 
                return result;
            }

            psize -= FWBUFFSIZE;

    }

    progressDialog("","",25,1);

    // for(uint32_t i=0; i < partition->size; i += FWBUFFSIZE) {
    //     esp_err_t result = esp_partition_read(target, i, data, FWBUFFSIZE);
    //     for (int m=0;m<FWBUFFSIZE;m++)
    //         writeByteFile(data[m], file);
    // }

    // fclose(file);

    // printf("After -> %ld\n",psize);

    // for(int n=0;n<256;n++) tst[n]=0xFF;

    // for (int i=0; i < 0x4000; i += 256) {
    //     // for (int n=0;n<256;n++) {
    //     //     tst[n] = gb_rom_0_plus2_es[i + n];
    //     // }
    //     result = esp_partition_write(target, rom_off + i, tst, 256);
    //     if (result != ESP_OK) {
    //         printf("esp_partition_write failed, err=0x%x.\n", result);
    //         return;
    //     }
    // }

    // for (int i= 0; i < 0x4000; i += 256) {
    //     // for (int n=0;n<256;n++) {
    //     //     tst[n] = gb_rom_1_plus2_es[i + n];
    //     // }
    //     result = esp_partition_write(target, rom_off + i + 0x4000, tst, 256);
    //     if (result != ESP_OK) {
    //         printf("esp_partition_write failed, err=0x%x.\n", result);
    //         return;
    //     }
    // }

    // esp_partition_write(target,rom_off,&gb_rom_0_48k_rg[8],16384);
    // esp_partition_write(target,rom_off + 0x4000,&gb_rom_0_48k_rg[8],16384);    

    result = esp_ota_set_boot_partition(target);
    if (result != ESP_OK) {
        printf("esp_ota_set_boot_partition failed, err=0x%x.\n", result);
        progressDialog("","",0,2); 
        return result;
    }

    size_t bytesread;

    if (arch == 1) {

        progressDialog("","",50,1);

        // Inject new 48K custom ROM
        for (int i=0; i < 0x4000; i += 0x1000) {
            bytesread = fread(data, 1, 0x1000 , customrom);
            result = esp_partition_write(target, rom_off + i, data, 0x1000);
            if (result != ESP_OK) {
                printf("esp_partition_write failed, err=0x%x.\n", result);
                progressDialog("","",0,2); 
                return result;
            }
        }

        progressDialog("","",75,1);

        // Copy previous 128K custom ROM
        for (int i=0; i < 0x8000; i += 0x1000) {
            esp_err_t result = esp_partition_read(partition, rom_off_128 + i, data, 0x1000);
            if (result == ESP_OK) {    
                result = esp_partition_write(target, rom_off_128 + i, data, 0x1000);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                    progressDialog("","",0,2); 
                    return result;
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
                progressDialog("","",0,2); 
                return result;
            }
        }

        progressDialog("","",100,1);

    } else if (arch == 2) {

        // Inject new 128K custom ROM part 1
        for (int i=0; i < 0x4000; i += 0x1000) {
            bytesread = fread(data, 1, 0x1000 , customrom);
            result = esp_partition_write(target, rom_off_128 + i, data, 0x1000);
            if (result != ESP_OK) {
                printf("esp_partition_write failed, err=0x%x.\n", result);
                progressDialog("","",0,2); 
                return result;
            }
        }

        progressDialog("","",50,1);

        // Inject new 128K custom ROM part 2
        for (int i=0; i < 0x4000; i += 0x1000) {
            
            if (bytesfirmware == 0x4000) {
                for (int n=0;n<0x1000;n++)
                    data[n] = gb_rom_1_sinclair_128k[i + n];
            } else {
                bytesread = fread(data, 1, 0x1000 , customrom);
            }
            
            result = esp_partition_write(target, rom_off_128 + i + 0x4000, data, 0x1000);
            if (result != ESP_OK) {
                printf("esp_partition_write failed, err=0x%x.\n", result);
                progressDialog("","",0,2); 
                return result;
            }

        }

        progressDialog("","",75,1);

        // Copy previous 48K custom ROM
        for (int i=0; i < 0x4000; i += 0x1000) {
            esp_err_t result = esp_partition_read(partition, rom_off + i, data, 0x1000);
            if (result == ESP_OK) {    
                result = esp_partition_write(target, rom_off + i, data, 0x1000);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                    progressDialog("","",0,2); 
                    return result;
                }
            } else {
                    printf("esp_partition_read failed, err=0x%x.\n", result);
                    progressDialog("","",0,2); 
                    return result;
            }
        }

        progressDialog("","",100,1);

    }

    // for (int i=0; i < 0x4000; i += 256) {
    //     result = esp_partition_read(target, rom_off + i, tst, 256);
    //     if (result != ESP_OK) {
    //         printf("esp_partition_read failed, err=0x%x.\n", result);
    //         return;
    //     }
    //     for (int n=0;n<256;n++) {
    //                 if (tst[n] != gb_rom_0_plus2_es[i+n]) printf("Dif!\n");
    //     }
    // }

    // for (int i=0; i < 0x4000; i += 256) {
    //     result = esp_partition_read(target, rom_off + i + 0x4000, tst, 256);
    //     if (result != ESP_OK) {
    //         printf("esp_partition_read failed, err=0x%x.\n", result);
    //         return;
    //     }
    //     for (int n=0;n<256;n++) {
    //                 if (tst[n] != gb_rom_1_plus2_es[ i + n]) printf("Dif!\n");
    //     }
    // }

    progressDialog(dlgTitle,OSD_FIRMW_END[Config::lang],0,1);

    delay(1000);

    // Firmware written: reboot
    OSD::esp_hard_reset();

}

esp_err_t OSD::updateFirmware(FILE *firmware) {

char ota_write_data[FWBUFFSIZE + 1] = { 0 };

// get the currently running partition
const esp_partition_t *partition = esp_ota_get_running_partition();
if (partition == NULL) {
    return ESP_ERR_NOT_FOUND;
}

// Grab next update target
// const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
string splabel;
if (strcmp(partition->label,"esp0")==0) splabel = "esp1"; else splabel= "esp0";
const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,splabel.c_str());
if (target == NULL) {
    return ESP_ERR_NOT_FOUND;
}

// printf("Running partition %s type %d subtype %d at offset 0x%x.\n", partition->label, partition->type, partition->subtype, partition->address);
// printf("Target  partition %s type %d subtype %d at offset 0x%x.\n", target->label, target->type, target->subtype, target->address);

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
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
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
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
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
        uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
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
    
    // Keyboard loop
    fabgl::VirtualKeyItem Menukey;
    while (1) {

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

        ESPectrum::readKbdJoy();

        // Process external keyboard
        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            if (ESPectrum::readKbd(&Menukey)) {
                if (!Menukey.down) continue;

                if (Menukey.vk == fabgl::VK_LEFT || Menukey.vk == fabgl::VK_JOY1LEFT || Menukey.vk == fabgl::VK_JOY2LEFT) {
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
                } else if (Menukey.vk == fabgl::VK_RIGHT || Menukey.vk == fabgl::VK_JOY1RIGHT || Menukey.vk == fabgl::VK_JOY2RIGHT) {
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
                } else if (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_SPACE || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C) {
                    break;
                } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {
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
        uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
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

#define MENU_JOYSELKEY_EN "Key      \n"\
    "A-Z      \n"\
    "1-0      \n"\
    "Special  \n"\
    "PS/2     \n"
#define MENU_JOYSELKEY_ES "Tecla    \n"\
    "A-Z      \n"\
    "1-0      \n"\
    "Especial \n"\
    "PS/2     \n"
static const char *MENU_JOYSELKEY[2] = { MENU_JOYSELKEY_EN, MENU_JOYSELKEY_ES };

#define MENU_JOY_AZ "A-Z\n"\
    "A\n"\
    "B\n"\
    "C\n"\
    "D\n"\
    "E\n"\				
    "F\n"\
    "G\n"\
	"H\n"\			
	"I\n"\				
	"J\n"\					
	"K\n"\						
    "L\n"\
    "M\n"\
    "N\n"\
    "O\n"\
    "P\n"\				
    "Q\n"\
    "R\n"\
	"S\n"\			
	"T\n"\				
	"U\n"\					
	"V\n"\						
    "W\n"\
    "X\n"\
    "Y\n"\
    "Z\n"

#define MENU_JOY_09 "0-9\n"\
	"0\n"\
    "1\n"\
    "2\n"\
    "3\n"\
    "4\n"\
    "5\n"\				
    "6\n"\
    "7\n"\
	"8\n"\			
	"9\n"

#define MENU_JOY_SPECIAL "Enter\n"\
    "Caps\n"\
    "SymbShift\n"\
    "Brk/Space\n"\
    "None\n"

#define MENU_JOY_PS2 "PS/2\n"\
    "F1\n"\
    "F2\n"\
    "F3\n"\
    "F4\n"\
    "F5\n"\
	"F6\n"\
	"F7\n"\
    "F8\n"\
    "F9\n"\
    "F10\n"\
    "F11\n"\
    "F12\n"\
    "Pause\n"\
    "PrtScr\n"\
    "Left\n"\
    "Right\n"\
    "Up\n"\
    "Down\n"

#define MENU_JOY_KEMPSTON "Kempston\n"\
    "Left\n"\
    "Right\n"\
    "Up\n"\
    "Down\n"\
    "Fire 1\n"\
    "Fire 2\n"		

#define MENU_JOY_FULLER "Fuller\n"\
    "Left\n"\
    "Right\n"\
    "Up\n"\
    "Down\n"\
    "Fire\n"	

string vkToText(int key) {

fabgl::VirtualKey vk = (fabgl::VirtualKey) key;

switch (vk)
{
case fabgl::VK_0:
    return "    0    ";
case fabgl::VK_1:
    return "    1    ";
case fabgl::VK_2:
    return "    2    ";
case fabgl::VK_3:
    return "    3    ";
case fabgl::VK_4:
    return "    4    ";
case fabgl::VK_5:
    return "    5    ";
case fabgl::VK_6:
    return "    6    ";
case fabgl::VK_7:
    return "    7    ";
case fabgl::VK_8:
    return "    8    ";
case fabgl::VK_9:
    return "    9    ";
case fabgl::VK_A:
    return "    A    ";
case fabgl::VK_B:
    return "    B    ";
case fabgl::VK_C:
    return "    C    ";
case fabgl::VK_D:
    return "    D    ";
case fabgl::VK_E:
    return "    E    ";
case fabgl::VK_F:
    return "    F    ";
case fabgl::VK_G:
    return "    G    ";
case fabgl::VK_H:
    return "    H    ";
case fabgl::VK_I:
    return "    I    ";
case fabgl::VK_J:
    return "    J    ";
case fabgl::VK_K:
    return "    K    ";
case fabgl::VK_L:
    return "    L    ";
case fabgl::VK_M:
    return "    M    ";
case fabgl::VK_N:
    return "    N    ";
case fabgl::VK_O:
    return "    O    ";
case fabgl::VK_P:
    return "    P    ";
case fabgl::VK_Q:
    return "    Q    ";
case fabgl::VK_R:
    return "    R    ";
case fabgl::VK_S:
    return "    S    ";
case fabgl::VK_T:
    return "    T    ";
case fabgl::VK_U:
    return "    U    ";
case fabgl::VK_V:
    return "    V    ";
case fabgl::VK_W:
    return "    W    ";
case fabgl::VK_X:
    return "    X    ";
case fabgl::VK_Y:
    return "    Y    ";
case fabgl::VK_Z:
    return "    Z    ";
case fabgl::VK_RETURN:
    return "  Enter  ";
case fabgl::VK_SPACE:
    return "Brk/Space";
case fabgl::VK_LSHIFT:
    return "  Caps   ";
case fabgl::VK_LCTRL:
    return "SymbShift";
case fabgl::VK_F1:
    return "   F1    ";
case fabgl::VK_F2:
    return "   F2    ";
case fabgl::VK_F3:
    return "   F3    ";
case fabgl::VK_F4:
    return "   F4    ";
case fabgl::VK_F5:
    return "   F5    ";
case fabgl::VK_F6:
    return "   F6    ";
case fabgl::VK_F7:
    return "   F7    ";
case fabgl::VK_F8:
    return "   F8    ";
case fabgl::VK_F9:
    return "   F9    ";
case fabgl::VK_F10:
    return "   F10   ";
case fabgl::VK_F11:
    return "   F11   ";
case fabgl::VK_F12:
    return "   F12   ";
case fabgl::VK_PAUSE:
    return "  Pause  ";
case fabgl::VK_PRINTSCREEN:
    return " PrtScr  ";
case fabgl::VK_LEFT:
    return "  Left   ";
case fabgl::VK_RIGHT:
    return "  Right  ";
case fabgl::VK_UP:
    return "   Up    ";
case fabgl::VK_DOWN:
    return "  Down   ";
case fabgl::VK_KEMPSTON_LEFT:
    return "Kmp.Left ";
case fabgl::VK_KEMPSTON_RIGHT:
    return "Kmp.Right";
case fabgl::VK_KEMPSTON_UP:   
    return " Kmp.Up  ";
case fabgl::VK_KEMPSTON_DOWN:
    return "Kmp.Down ";
case fabgl::VK_KEMPSTON_FIRE:    
    return "Kmp.Fire1";
case fabgl::VK_KEMPSTON_ALTFIRE:
    return "Kmp.Fire2";		
case fabgl::VK_FULLER_LEFT:
    return "Fll.Left ";
case fabgl::VK_FULLER_RIGHT:
    return "Fll.Right";
case fabgl::VK_FULLER_UP:
    return " Fll.Up  ";
case fabgl::VK_FULLER_DOWN:
    return "Fll.Down ";
case fabgl::VK_FULLER_FIRE:
    return "Fll.Fire ";
default:
    return "  None   ";
}

}

unsigned int joyControl[12][3]={
    {34,55,zxColor(0,0)}, // Left
    {87,55,zxColor(0,0)}, // Right
    {63,30,zxColor(0,0)}, // Up
    {63,78,zxColor(0,0)}, // Down
    {49,109,zxColor(0,0)}, // Start
    {136,109,zxColor(0,0)}, // Mode
    {145,69,zxColor(0,0)}, // A
    {205,69,zxColor(0,0)}, // B
    {265,69,zxColor(0,0)}, // C
    {145,37,zxColor(0,0)}, // X
    {205,37,zxColor(0,0)}, // Y
    {265,37,zxColor(0,0)} // Z
};

void DrawjoyControls(unsigned short x, unsigned short y) {

    // Draw joy controls

    // Left arrow
    for (int i = 0; i <= 5; i++) {
        VIDEO::vga.line(x + joyControl[0][0] + i, y + joyControl[0][1] - i, x + joyControl[0][0] + i, y + joyControl[0][1] + i, joyControl[0][2]);
    }

    // Right arrow
    for (int i = 0; i <= 5; i++) {
        VIDEO::vga.line(x + joyControl[1][0] + i, y + joyControl[1][1] - ( 5 - i), x + joyControl[1][0] + i, y + joyControl[1][1] + ( 5 - i), joyControl[1][2]);
    }

    // Up arrow
    for (int i = 0; i <= 6; i++) {
        VIDEO::vga.line(x + joyControl[2][0] - i, y + joyControl[2][1] + i, x + joyControl[2][0] + i, y + joyControl[2][1] + i, joyControl[2][2]);
    }

    // Down arrow
    for (int i = 0; i <= 6; i++) {
        VIDEO::vga.line(x + joyControl[3][0] - (6 - i), y + joyControl[3][1] + i, x + joyControl[3][0] + ( 6 - i), y + joyControl[3][1] + i, joyControl[3][2]);
    }

    // START text
    VIDEO::vga.setTextColor(joyControl[4][2], zxColor(7, 1));        
    VIDEO::vga.setCursor(x + joyControl[4][0], y + joyControl[4][1]);
    VIDEO::vga.print("START");

    // MODE text
    VIDEO::vga.setTextColor(joyControl[5][2], zxColor(7, 1));        
    VIDEO::vga.setCursor(x + joyControl[5][0], y + joyControl[5][1]);
    VIDEO::vga.print("MODE");

    // Text A
    VIDEO::vga.setTextColor( joyControl[6][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[6][0], y + joyControl[6][1]);
    VIDEO::vga.circle(x + joyControl[6][0] + 3, y + joyControl[6][1] + 3, 6,  joyControl[6][2]);
    VIDEO::vga.print("A");

    // Text B
    VIDEO::vga.setTextColor(joyControl[7][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[7][0], y + joyControl[7][1]);
    VIDEO::vga.circle(x + joyControl[7][0] + 3, y + joyControl[7][1] + 3, 6,  joyControl[7][2]);
    VIDEO::vga.print("B");

    // Text C
    VIDEO::vga.setTextColor(joyControl[8][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[8][0], y + joyControl[8][1]);
    VIDEO::vga.circle(x + joyControl[8][0] + 3, y + joyControl[8][1] + 3, 6, joyControl[8][2]);
    VIDEO::vga.print("C");

    // Text X
    VIDEO::vga.setTextColor(joyControl[9][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[9][0], y + joyControl[9][1]);
    VIDEO::vga.circle(x + joyControl[9][0] + 3, y + joyControl[9][1] + 3, 6, joyControl[9][2]);
    VIDEO::vga.print("X");

    // Text Y
    VIDEO::vga.setTextColor(joyControl[10][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[10][0], y + joyControl[10][1]);
    VIDEO::vga.circle(x + joyControl[10][0] + 3, y + joyControl[10][1] + 3, 6, joyControl[10][2]);    
    VIDEO::vga.print("Y");

    // Text Z
    VIDEO::vga.setTextColor(joyControl[11][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[11][0], y + joyControl[11][1]);
    VIDEO::vga.circle(x + joyControl[11][0] + 3, y + joyControl[11][1] + 3, 6, joyControl[11][2]);
    VIDEO::vga.print("Z");

}

void OSD::joyDialog(uint8_t joynum) {

    int joyDropdown[14][7]={
        {7,65,-1,1,2,3,0}, // Left
        {67,65,0,-1,2,3,0}, // Right
        {37,17,-1,9,-1,0,0}, // Up
        {37,89,-1,6,0,4,0}, // Down
        {37,121,-1,5,3,-1,0}, // Start
        {121,121,4,12,6,-1,0}, // Mode
        {121,89,3,7,9,5,0}, // A
        {181,89,6,8,10,12,0}, // B
        {241,89,7,-1,11,13,0}, // C
        {121,17,2,10,-1,6,0}, // X
        {181,17,9,11,-1,7,0}, // Y                                        
        {241,17,10,-1,-1,8,0}, // Z
        {181,121,5,13,7,-1,0}, // Ok
        {241,121,12,-1,8,-1,0} // Test                
    };

    string keymenu = MENU_JOYSELKEY[Config::lang];
    int joytype = joynum == 1 ? Config::joystick1 : Config::joystick2;

    string selkeymenu[5] = {
        MENU_JOY_AZ,
        MENU_JOY_09,
        "",
        MENU_JOY_PS2,
        ""
    };

    selkeymenu[2] = (Config::lang ? "Especial\n" : "Special\n");
    selkeymenu[2] += MENU_JOY_SPECIAL;

    if (joytype == JOY_KEMPSTON) {
        keymenu += "Kempston \n";
        selkeymenu[4] = MENU_JOY_KEMPSTON;
    } else
    if (joytype == JOY_FULLER) {
        keymenu += "Fuller   \n";
        selkeymenu[4] = MENU_JOY_FULLER;
    }

    int curDropDown = 2;
    uint8_t joyDialogMode = 0; // 0 -> Define, 1 -> Test

    const unsigned short h = (OSD_FONT_H * 18) + 2;
    const unsigned short y = scrAlignCenterY(h) - 8;

    const unsigned short w = (50 * OSD_FONT_W) + 2;
    const unsigned short x = scrAlignCenterX(w) - 3;

    // Set font
    VIDEO::vga.setFont(Font6x8);

    // Menu border
    VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

    VIDEO::vga.fillRect(x + 1, y + 1, w - 2, OSD_FONT_H, zxColor(0,0));
    VIDEO::vga.fillRect(x + 1, y + 1 + OSD_FONT_H, w - 2, h - OSD_FONT_H - 2, zxColor(7,1));

    // Title
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));        
    VIDEO::vga.setCursor(x + OSD_FONT_W + 1, y + 1);
    VIDEO::vga.print((joynum == 1 ? "Joystick 1" : "Joystick 2"));

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

    // Read joy definition into joyDropdown
    for (int n=0; n<12; n++)
        joyDropdown[n][6] = ESPectrum::JoyVKTranslation[n + (joynum == 1 ? 0 : 12)];

    // Draw Joy DropDowns
    for (int n=0; n<12; n++) {
        VIDEO::vga.rect(x + joyDropdown[n][0] - 2, y + joyDropdown[n][1] - 2, 58, 12, zxColor(0, 0));
        if (n == curDropDown) 
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        else
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        VIDEO::vga.setCursor(x + joyDropdown[n][0], y + joyDropdown[n][1]);
        VIDEO::vga.print(vkToText(joyDropdown[n][6]).c_str());
    }

    // Draw dialog buttons
    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyDropdown[12][0], y + joyDropdown[12][1]);
    VIDEO::vga.print("   Ok    ");
    VIDEO::vga.setCursor(x + joyDropdown[13][0], y + joyDropdown[13][1]);
    VIDEO::vga.print(" JoyTest ");

    // // Ruler
    // VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));        
    // VIDEO::vga.setCursor(x + 1, y + 1 + (OSD_FONT_H * 3));
    // VIDEO::vga.print("123456789012345678901234567");

    DrawjoyControls(x,y);

    // Wait for key
    fabgl::VirtualKeyItem Nextkey;

    int joyTestExitCount1 = 0;
    int joyTestExitCount2 = 0;    

    while (1) {

        if (joyDialogMode) {
            DrawjoyControls(x,y);            
        }

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

        ESPectrum::readKbdJoy();

        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            ESPectrum::PS2Controller.keyboard()->getNextVirtualKey(&Nextkey);
            if(!Nextkey.down) continue;
            
            if (Nextkey.vk == fabgl::VK_LEFT || Nextkey.vk == fabgl::VK_JOY1LEFT || Nextkey.vk == fabgl::VK_JOY2LEFT) {

                if (joyDialogMode == 0 && joyDropdown[curDropDown][2] >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());                    
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    curDropDown = joyDropdown[curDropDown][2];

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());                    
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    click();                        

                }

            } else
            if (Nextkey.vk == fabgl::VK_RIGHT || Nextkey.vk == fabgl::VK_JOY1RIGHT || Nextkey.vk == fabgl::VK_JOY2RIGHT) {

                if (joyDialogMode == 0 && joyDropdown[curDropDown][3] >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());                    
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    curDropDown = joyDropdown[curDropDown][3];

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());                    
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    click();                        

                }

            } else
            if (Nextkey.vk == fabgl::VK_UP || Nextkey.vk == fabgl::VK_JOY1UP || Nextkey.vk == fabgl::VK_JOY2UP) {

                if (joyDialogMode == 0 && joyDropdown[curDropDown][4] >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());                    
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    curDropDown = joyDropdown[curDropDown][4];

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());                    
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    click();                        

                }

            } else
            if (Nextkey.vk == fabgl::VK_DOWN || Nextkey.vk == fabgl::VK_JOY1DOWN || Nextkey.vk == fabgl::VK_JOY2DOWN) {

                if (joyDialogMode == 0 && joyDropdown[curDropDown][5] >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());                    
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    curDropDown = joyDropdown[curDropDown][5];

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());                    
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    click();                        

                }

            } else
            if (Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2B) {

                if (joyDialogMode == 0) {

                    if (curDropDown>=0 && curDropDown<12) {
                        
                        click();

                        // Launch assign menu
                        menu_curopt = 1;
                        while (1) {
                            menu_level = 0;
                            menu_saverect = true;
                            uint8_t opt = simpleMenuRun(keymenu,x + joyDropdown[curDropDown][0],y + joyDropdown[curDropDown][1],6,11);
                            if(opt!=0) {
                                // Key select menu
                                menu_saverect = true;
                                menu_level = 0;
                                menu_curopt = 1;
                                uint8_t opt2 = simpleMenuRun(selkeymenu[opt - 1],x + joyDropdown[curDropDown][0],y + joyDropdown[curDropDown][1],6,11);
                                if(opt2!=0) {

                                    if (opt == 1) {// A-Z
                                        joyDropdown[curDropDown][6] = (fabgl::VirtualKey) 47 + opt2;
                                    } else
                                    if (opt == 2) {// 1-0
                                        joyDropdown[curDropDown][6] = (fabgl::VirtualKey) 1 + opt2;
                                    } else
                                    if (opt == 3) {// Special
                                        if (opt2 == 1) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_RETURN;
                                        } else
                                        if (opt2 == 2) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_LSHIFT;
                                        } else
                                        if (opt2 == 3) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_LCTRL;
                                        } else
                                        if (opt2 == 4) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_SPACE;
                                        } else
                                        if (opt2 == 5) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_NONE;
                                        }
                                    } else
                                    if (opt == 4) {// PS/2
                                        if (opt2 < 13) {
                                            joyDropdown[curDropDown][6] = (fabgl::VirtualKey) 158 + opt2;
                                        } else 
                                        if (opt2 == 13) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_PAUSE;
                                        } else
                                        if (opt2 == 14) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_PRINTSCREEN;
                                        } else
                                        if (opt2 == 15) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_LEFT;
                                        } else
                                        if (opt2 == 16) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_RIGHT;
                                        } else
                                        if (opt2 == 17) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_UP;
                                        } else
                                        if (opt2 == 18) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_DOWN;
                                        }
                                    } else
                                    if (opt == 5) {// Kempston / Fuller
                                        if (opt2 == 1) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_LEFT;
                                        } else
                                        if (opt2 == 2) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_RIGHT;
                                        } else
                                        if (opt2 == 3) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_UP;
                                        } else
                                        if (opt2 == 4) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_DOWN;
                                        } else
                                        if (opt2 == 5) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_FIRE;
                                        } else
                                        if (opt2 == 6) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_ALTFIRE;
                                        }

                                        if (joytype == JOY_FULLER)
                                            joyDropdown[curDropDown][6] += 6;

                                    }

                                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                                    VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());

                                    break;

                                }
                            } else break;
                            menu_curopt = opt;
                        }

                    } else
                    if (curDropDown == 12) {
                        // Ok button

                        // Check if there are changes and ask to save them
                        bool changed = false;
                        for (int n = 0; n < 12; n++) {
                            if (ESPectrum::JoyVKTranslation[n + (joynum == 1 ? 0 : 12)] != joyDropdown[n][6]) {
                                changed = true;
                                break;
                            }
                        }

                        // Ask to save changes
                        if (changed) {

                            string title = (joynum == 1 ? "Joystick 1" : "Joystick 2");
                            string msg = OSD_DLG_JOYSAVE[Config::lang];
                            uint8_t res = OSD::msgDialog(title,msg);
                            if (res == DLG_YES) {

                                // Fill joystick values in Config
                                int m = (joynum == 1) ? 0 : 12;
                                for (int n = m; n < m + 12; n++) {
                                    // Save to config (only changes)
                                    if (Config::joydef[n] != (uint16_t) joyDropdown[n - m][6]) {
                                        ESPectrum::JoyVKTranslation[n] = (fabgl::VirtualKey) joyDropdown[n - m][6];
                                        Config::joydef[n] = (uint16_t) joyDropdown[n - m][6];
                                        char joykey[9];
                                        sprintf(joykey,"joydef%02u",n);
                                        Config::save(joykey);
                                        // printf("%s %u\n",joykey, joydef[n]);
                                    }
                                }

                                click();
                                break;

                            } else
                            if (res == DLG_NO) {
                                click();
                                break;
                            }

                        } else {
                            click();
                            break;
                        }

                    } else
                    if (curDropDown == 13) {                    
                        // Enable joyTest
                        joyDialogMode = 1;

                        joyTestExitCount1 = 0;
                        joyTestExitCount2 = 0;

                        VIDEO::vga.setTextColor(zxColor(4, 1), zxColor(5, 1));
                        VIDEO::vga.setCursor(x + joyDropdown[13][0], y + joyDropdown[13][1]);
                        VIDEO::vga.print(" JoyTest ");

                        click();

                    }

                }

            } else
            if (Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY2A) {
                
                if (joyDialogMode) {

                    if (Nextkey.vk == fabgl::VK_ESCAPE) {

                        // Disable joyTest
                        joyDialogMode = 0;
                        
                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                        VIDEO::vga.setCursor(x + joyDropdown[13][0], y + joyDropdown[13][1]);
                        VIDEO::vga.print(" JoyTest ");
                        
                        for (int n = 0; n < 12; n++)
                            joyControl[n][2] = zxColor(0,0);  

                        DrawjoyControls(x,y);

                        click();

                    }

                } else {

                    // Check if there are changes and ask to discard them
                    bool changed = false;
                    for (int n = 0; n < 12; n++) {
                        if (ESPectrum::JoyVKTranslation[n + (joynum == 1 ? 0 : 12)] != joyDropdown[n][6]) {
                            changed = true;
                            break;
                        }
                    }

                    // Ask to discard changes
                    if (changed) {
                        string title = (joynum == 1 ? "Joystick 1" : "Joystick 2");
                        string msg = OSD_DLG_JOYDISCARD[Config::lang];
                        uint8_t res = OSD::msgDialog(title,msg);
                        if (res == DLG_YES) {
                            click();
                            break;
                        }
                    } else {
                        click();
                        break;
                    }

                }

            }

        }

        // Joy Test Mode: Check joy status and color controls
        if (joyDialogMode) {

            for (int n = (joynum == 1 ? fabgl::VK_JOY1LEFT : fabgl::VK_JOY2LEFT); n <= (joynum == 1 ? fabgl::VK_JOY1Z : fabgl::VK_JOY2Z); n++) {
                if (ESPectrum::PS2Controller.keyboard()->isVKDown((fabgl::VirtualKey) n))
                    joyControl[n - (joynum == 1 ? 248 : 260)][2] = zxColor(4,1);            
                else
                    joyControl[n - (joynum == 1 ? 248 : 260)][2] = zxColor(0,0);
            }

            if (ESPectrum::PS2Controller.keyboard()->isVKDown(fabgl::VK_JOY1A)) {
                joyTestExitCount1++;
                if (joyTestExitCount1 == 30)
                    ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE,true,false);
            } else
                joyTestExitCount1 = 0;

            if (ESPectrum::PS2Controller.keyboard()->isVKDown(fabgl::VK_JOY2A)) {
                joyTestExitCount2++;
                if (joyTestExitCount2 == 30)
                    ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE,true,false);
            } else
                joyTestExitCount2 = 0;

        }

        vTaskDelay(50 / portTICK_PERIOD_MS);

    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// POKE DIALOG
////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DLG_OBJ_BUTTON 0
#define DLG_OBJ_INPUT 1
#define DLG_OBJ_COMBO 2

struct dlgObject {
    string Name;
    unsigned short int posx;
    unsigned short int posy;
    int objLeft;
    int objRight;
    int objTop;
    int objDown;
    unsigned char objType;
    string Label[2];
};

const dlgObject dlg_Objects[5] = {
    {"Bank",70,16,-1,-1, 4, 1, DLG_OBJ_COMBO , {"RAM Bank  ","Banco RAM "}},
    {"Address",70,32,-1,-1, 0, 2, DLG_OBJ_INPUT , {"Address   ","Direccion "}},
    {"Value",70,48,-1,-1, 1, 4, DLG_OBJ_INPUT , {"Value     ","Valor     "}},
    {"Ok",7,65,-1, 4, 2, 0, DLG_OBJ_BUTTON,  {"  Ok  "  ,"  Ok  "  }},
    {"Cancel",52,65, 3,-1, 2, 0, DLG_OBJ_BUTTON, {"  Cancel  "," Cancelar "}}
};

const string BankCombo[9] = { "   -   ", "   0   ", "   1   ", "   2   ", "   3   ", "   4   ", "   5   ", "   6   ", "   7   " };

void OSD::pokeDialog() {

    string dlgValues[5]={
        "   -   ", // Bank
        "16384", // Address
        "0", // Value
        "",
        ""
    };

    string Bankmenu = (Config::lang ? " Banco \n" : " Bank  \n");
    int i=0;
    for (;i<9;i++) Bankmenu += BankCombo[i] + "\n";

    int curObject = 0;
    uint8_t dlgMode = 0; // 0 -> Move, 1 -> Input

    const unsigned short h = (OSD_FONT_H * 10) + 2;
    const unsigned short y = scrAlignCenterY(h) - 8;

    const unsigned short w = (OSD_FONT_W * 20) + 2;
    const unsigned short x = scrAlignCenterX(w) - 3;

    click();                        

    // Set font
    VIDEO::vga.setFont(Font6x8);

    // Menu border
    VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

    VIDEO::vga.fillRect(x + 1, y + 1, w - 2, OSD_FONT_H, zxColor(0,0));
    VIDEO::vga.fillRect(x + 1, y + 1 + OSD_FONT_H, w - 2, h - OSD_FONT_H - 2, zxColor(7,1));

    // Title
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));        
    VIDEO::vga.setCursor(x + OSD_FONT_W + 1, y + 1);
    VIDEO::vga.print(Config::lang ? "A" "\xA4" "adir Poke" : "Input Poke");

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

    // Draw objects
    for (int n = 0; n < 5; n++) {
        
        if (dlg_Objects[n].Label[Config::lang] != "" && dlg_Objects[n].objType != DLG_OBJ_BUTTON) {
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
            VIDEO::vga.setCursor(x + dlg_Objects[n].posx - 63, y + dlg_Objects[n].posy);
            VIDEO::vga.print(dlg_Objects[n].Label[Config::lang].c_str());
            VIDEO::vga.rect(x + dlg_Objects[n].posx - 2, y + dlg_Objects[n].posy - 2, 46, 12, zxColor(0, 0));
        }

        if (n == curObject) 
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        else
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));

        VIDEO::vga.setCursor(x + dlg_Objects[n].posx, y + dlg_Objects[n].posy);
        if (dlg_Objects[n].objType == DLG_OBJ_BUTTON) {
            VIDEO::vga.print(dlg_Objects[n].Label[Config::lang].c_str());        
        } else {
            VIDEO::vga.print(dlgValues[n].c_str());
        }

    }

    // Wait for key
    fabgl::VirtualKeyItem Nextkey;

    uint8_t CursorFlash = 0;

    while (1) {

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

        ESPectrum::readKbdJoy();

        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {

            ESPectrum::PS2Controller.keyboard()->getNextVirtualKey(&Nextkey);

            if(!Nextkey.down) continue;
            
            if ((Nextkey.vk >= fabgl::VK_0) && (Nextkey.vk <= fabgl::VK_9)) {

                if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) {
                    if (dlgValues[curObject].length() < (curObject == 1 ? 5 : 3)) {
                        dlgValues[curObject] += char(Nextkey.vk + 46);
                    }
                }

                click();

            } else
            if (Nextkey.vk == fabgl::VK_LEFT || Nextkey.vk == fabgl::VK_JOY1LEFT || Nextkey.vk == fabgl::VK_JOY2LEFT) {

                if (dlg_Objects[curObject].objLeft >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                    if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                        VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());        
                    } else {
                        VIDEO::vga.print(dlgValues[curObject].c_str());
                    }

                    curObject = dlg_Objects[curObject].objLeft;

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                    if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                        VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());        
                    } else {
                        VIDEO::vga.print(dlgValues[curObject].c_str());
                    }

                    click();                        

                }

            } else
            if (Nextkey.vk == fabgl::VK_RIGHT || Nextkey.vk == fabgl::VK_JOY1RIGHT || Nextkey.vk == fabgl::VK_JOY2RIGHT) {

                if (dlg_Objects[curObject].objRight >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                    if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                        VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());        
                    } else {
                        VIDEO::vga.print(dlgValues[curObject].c_str());
                    }

                    curObject = dlg_Objects[curObject].objRight;

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                    if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                        VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());        
                    } else {
                        VIDEO::vga.print(dlgValues[curObject].c_str());
                    }

                    click();                        

                }

            } else
            if (Nextkey.vk == fabgl::VK_UP || Nextkey.vk == fabgl::VK_JOY1UP || Nextkey.vk == fabgl::VK_JOY2UP) {

                if (dlg_Objects[curObject].objTop >= 0) {

                    // Input values validation
                    bool validated = true;

                    if (dlg_Objects[curObject].Name == "Address") {

                        string val = dlgValues[1];
                        trim(val);
                        if (val!="") {
                            // Check value
                            if (dlgValues[0] == "   -   ") {
                                if (stoi(val) < 16384 || stoi(val) > 65535) {
                                    osdCenteredMsg(POKE_ERR_ADDR1[Config::lang], LEVEL_WARN, 1000);
                                    validated = false;
                                }
                            } else {
                                if (stoi(val) > 16383) {
                                    osdCenteredMsg(POKE_ERR_ADDR2[Config::lang], LEVEL_WARN, 1000);
                                    validated = false;
                                }
                            }

                        } else {
                            dlgValues[1]= dlgValues[0] == "   -   " ? "16384" : "0";
                        }
                    } else
                    if (dlg_Objects[curObject].Name == "Value") {
                        // Input values validation
                        string val = dlgValues[2];
                        trim(val);
                        if (val!="") {
                            // Check value
                            if (stoi(val) > 255) {
                                osdCenteredMsg(POKE_ERR_VALUE[Config::lang], LEVEL_WARN, 1000);
                                validated = false;  
                            }
                        } else {
                            dlgValues[2]="0";
                        }
                    }

                    if (validated) {

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                        VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                        if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                            VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());        
                        } else {
                            VIDEO::vga.print(dlgValues[curObject].c_str());
                            if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) VIDEO::vga.print(" "); // Clear K cursor
                        }

                        curObject = dlg_Objects[curObject].objTop;

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                        VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                        if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                            VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());        
                        } else {
                            VIDEO::vga.print(dlgValues[curObject].c_str());
                        }

                        click();                        

                    }

                }

            } else
            if (Nextkey.vk == fabgl::VK_DOWN || Nextkey.vk == fabgl::VK_JOY1DOWN || Nextkey.vk == fabgl::VK_JOY2DOWN) {

                if (dlg_Objects[curObject].objDown >= 0) {

                    // Input values validation
                    bool validated = true;

                    if (dlg_Objects[curObject].Name == "Address") {

                        string val = dlgValues[1];
                        trim(val);
                        if (val!="") {
                            // Check value
                            if (dlgValues[0] == "   -   ") {
                                if (stoi(val) < 16384 || stoi(val) > 65535) {
                                    osdCenteredMsg(POKE_ERR_ADDR1[Config::lang], LEVEL_WARN, 1000);
                                    validated = false;
                                }
                            } else {
                                if (stoi(val) > 16383) {
                                    osdCenteredMsg(POKE_ERR_ADDR2[Config::lang], LEVEL_WARN, 1000);
                                    validated = false;
                                }
                            }

                        } else {
                            dlgValues[1]= dlgValues[0] == "   -   " ? "16384" : "0";
                        }
                    } else
                    if (dlg_Objects[curObject].Name == "Value") {
                        // Input values validation
                        string val = dlgValues[2];
                        trim(val);
                        if (val!="") {
                            // Check value
                            if (stoi(val) > 255) {
                                osdCenteredMsg(POKE_ERR_VALUE[Config::lang], LEVEL_WARN, 1000);
                                validated = false;  
                            }
                        } else {
                            dlgValues[2]="0";
                        }
                    }

                    if (validated) {

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                        VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                        if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                            VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());        
                        } else {
                            VIDEO::vga.print(dlgValues[curObject].c_str());
                            if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) VIDEO::vga.print(" "); // Clear K cursor
                        }

                        curObject = dlg_Objects[curObject].objDown;

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                        VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                        if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                            VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());        
                        } else {
                            VIDEO::vga.print(dlgValues[curObject].c_str());
                        }

                        click();                        

                    }

                }

            } else
            if (Nextkey.vk == fabgl::VK_BACKSPACE) {            

                if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) {
                    if (dlgValues[curObject] != "") dlgValues[curObject].pop_back();
                }

                click();

            } else
            if (Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2B) {

                if (dlg_Objects[curObject].Name == "Bank" && !Z80Ops::is48) {

                    click();

                    // Launch bank menu
                    menu_curopt = 1;
                    while (1) {
                        menu_level = 0;
                        menu_saverect = true;
                        uint8_t opt = simpleMenuRun( Bankmenu, x + dlg_Objects[curObject].posx,y + dlg_Objects[curObject].posy, 10, 9);
                        if(opt!=0) {

                            if (BankCombo[opt -1] != dlgValues[curObject]) {

                                dlgValues[curObject] = BankCombo[opt - 1];

                                VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                                VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                                VIDEO::vga.print(dlgValues[curObject].c_str());

                                if (dlgValues[curObject]==BankCombo[0]) {
                                    dlgValues[1] = "16384";
                                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                                    VIDEO::vga.setCursor(x + dlg_Objects[1].posx, y + dlg_Objects[1].posy);
                                    VIDEO::vga.print("16384");
                                } else {
                                    string val = dlgValues[1];
                                    trim(val);
                                    if(stoi(val) > 16383) {
                                        dlgValues[1] = "0";
                                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                                        VIDEO::vga.setCursor(x + dlg_Objects[1].posx, y + dlg_Objects[1].posy);
                                        VIDEO::vga.print("0    ");
                                    }
                                }

                            }

                            break;
                        } else break;
                        menu_curopt = opt;
                    }

                } else
                if (dlg_Objects[curObject].Name == "Ok") {

                    string addr = dlgValues[1];
                    string val = dlgValues[2];
                    trim(addr);
                    trim(val);
                    int address = stoi(addr);
                    int value = stoi(val);

                    // Apply poke
                    if (dlgValues[0]=="   -   ") {
                        // Poke address between 16384 and 65535                        
                        uint8_t page = address >> 14;
                        MemESP::ramCurrent[page][address & 0x3fff] = value;
                    } else {
                        // Poke address in bank
                        string bank = dlgValues[0];
                        trim(bank);
                        MemESP::ram[stoi(bank)][address] = value;
                    }

                    click();

                    break;

                } else
                if (dlg_Objects[curObject].Name == "Cancel") {

                    click();
                    break;

                }

            } else
            if (Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY2A) {
                
                click();
                break;

            }

        }

        if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) {

            if ((++CursorFlash & 0xF) == 0) {

                VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));                
                VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx, y + dlg_Objects[curObject].posy);
                VIDEO::vga.print(dlgValues[curObject].c_str());

                if (CursorFlash > 63) {
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 1));
                    if (CursorFlash == 128) CursorFlash = 0;
                } else {
                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                }
                VIDEO::vga.print("K");

                VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                VIDEO::vga.print(" ");

            }

        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

}
