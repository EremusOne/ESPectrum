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

#include "OSDMain.h"
#include "FileUtils.h"
#include "CPU.h"
#include "Video.h"
#include "ESPectrum.h"
#include "messages.h"
#include "Config.h"
#include "FileSNA.h"
#include "FileZ80.h"
#include "AySound.h"
#include "MemESP.h"
#include "Tape.h"
#include "pwm_audio.h"

#include "esp_system.h"
#include "esp_ota_ops.h"
#include "fabgl.h"

#include "soc/rtc_wdt.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string>

using namespace std;

#define MENU_REDRAW true
#define MENU_UPDATE false
#define OSD_ERROR true
#define OSD_NORMAL false

#define OSD_W 248
#define OSD_H 176
#define OSD_MARGIN 4

extern Font Font6x8;

uint8_t OSD::menu_level = 0;

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

void esp_hard_reset() {
    // RESTART ESP32 (This is the most similar way to hard resetting it)
    rtc_wdt_protect_off();   
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_RTC);
    rtc_wdt_set_time(RTC_WDT_STAGE0, 100);
    rtc_wdt_enable();
    rtc_wdt_protect_on();
    while (true);
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

void OSD::drawOSD() {
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);
    VIDEO::vga.fillRect(x, y, OSD_W, OSD_H, OSD::zxColor(1, 0));
    VIDEO::vga.rect(x, y, OSD_W, OSD_H, OSD::zxColor(0, 0));
    VIDEO::vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, OSD::zxColor(7, 0));
    VIDEO::vga.setTextColor(OSD::zxColor(0, 0), OSD::zxColor(5, 1));
    VIDEO::vga.setFont(Font6x8);
    osdHome();
    VIDEO::vga.print(OSD_TITLE);
    osdAt(20, 0);
    VIDEO::vga.print(OSD_BOTTOM);
    osdHome();
}

void OSD::drawStats(char *line1, char *line2) {

    unsigned short x,y;

    if (Config::aspect_16_9) {
        x = 184;
        y = 176;
    } else {
        x = 168;
        y = 220;
    }

    // VIDEO::vga.fillRect(x, y, 80, 16, OSD::zxColor(1, 0));
    // VIDEO::vga.rect(x, y, OSD_W, OSD_H, OSD::zxColor(0, 0));
    // VIDEO::vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, OSD::zxColor(7, 0));

    VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(1, 0));
    VIDEO::vga.setFont(Font6x8);
    VIDEO::vga.setCursor(x,y);
    VIDEO::vga.print(line1);
    VIDEO::vga.setCursor(x,y+8);
    VIDEO::vga.print(line2);

}

static void persistSave(uint8_t slotnumber)
{
    char persistfname[strlen(DISK_PSNA_FILE) + 6];
    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    OSD::osdCenteredMsg(OSD_PSNA_SAVING, LEVEL_INFO, 0);
    if (!FileSNA::save(FileUtils::MountPoint + persistfname)) {
        OSD::osdCenteredMsg(OSD_PSNA_SAVE_ERR, LEVEL_WARN);
        return;
    }
    OSD::osdCenteredMsg(OSD_PSNA_SAVED, LEVEL_INFO);
}

static void persistLoad(uint8_t slotnumber)
{
    char persistfname[strlen(DISK_PSNA_FILE) + 6];
    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    if (!FileSNA::isPersistAvailable(FileUtils::MountPoint + persistfname)) {
        OSD::osdCenteredMsg(OSD_PSNA_NOT_AVAIL, LEVEL_INFO);
        return;
    }
    OSD::osdCenteredMsg(OSD_PSNA_LOADING, LEVEL_INFO);
    if (!FileSNA::load(FileUtils::MountPoint + persistfname)) {
         OSD::osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
    }
    if (Config::getArch() == "48K") AySound::reset();
    if (Config::getArch() == "48K") ESPectrum::samplesPerFrame=546; else ESPectrum::samplesPerFrame=554;
    OSD::osdCenteredMsg(OSD_PSNA_LOADED, LEVEL_INFO);
}

// OSD Main Loop
void OSD::do_OSD(fabgl::VirtualKey KeytoESP) {

    static uint8_t last_sna_row = 0;
    fabgl::VirtualKeyItem Nextkey;

    if (KeytoESP == fabgl::VK_PAUSE) {
        AySound::disable();
        osdCenteredMsg(OSD_PAUSE, LEVEL_INFO, 0);
        while (1) {
            ESPectrum::readKbd(&Nextkey);
            if ((Nextkey.down) && (Nextkey.vk == fabgl::VK_PAUSE)) break;
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
        AySound::enable();
    }
    else if (KeytoESP == fabgl::VK_F2) {
        AySound::disable();
        string mFile = menuFile(FileUtils::MountPoint + DISK_SNA_DIR, MENU_SNA_TITLE[Config::lang],".sna.SNA.z80.Z80");
        if (mFile != "") {
            changeSnapshot(mFile);
        }
        AySound::enable();
    }
    else if (KeytoESP == fabgl::VK_F3) {
        AySound::disable();
        // Persist Load
        uint8_t opt2 = menuRun(MENU_PERSIST_LOAD[Config::lang]);
        if (opt2 > 0 && opt2<6) {
            persistLoad(opt2);
        }
        AySound::enable();
    }
    else if (KeytoESP == fabgl::VK_F4) {
        AySound::disable();
        // Persist Save
        uint8_t opt2 = menuRun(MENU_PERSIST_SAVE[Config::lang]);
        if (opt2 > 0 && opt2<6) {
            persistSave(opt2);
        }
        AySound::enable();
    }
    else if (KeytoESP == fabgl::VK_F5) {
        AySound::disable();
        string mFile = menuFile(FileUtils::MountPoint + DISK_TAP_DIR, MENU_TAP_TITLE[Config::lang],".tap.TAP");
        if (mFile != "") {
            Tape::tapeFileName=FileUtils::MountPoint + DISK_TAP_DIR "/" + mFile;
        }
        AySound::enable();
    }
    else if (KeytoESP == fabgl::VK_F6) {
        // Start .tap reproduction
        if (Tape::tapeFileName=="none") {
            OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR, LEVEL_WARN);
        } else {
            Tape::TAP_Play();
        }
    }
    else if (KeytoESP == fabgl::VK_F7) {
        // Stop .tap reproduction
        Tape::TAP_Stop();
    }
    else if (KeytoESP == fabgl::VK_F8) {
        // Show / hide OnScreen Stats
        if (VIDEO::LineDraw == LINEDRAW) {
            VIDEO::LineDraw = LINEDRAW_FPS;
            VIDEO::BottomDraw = BOTTOMBORDER_FPS;
        } else {
            VIDEO::LineDraw = LINEDRAW;
            VIDEO::BottomDraw = BOTTOMBORDER;
        }    
    }
    // else if (KeytoESP == fabgl::VK_F9) {

    //     OSD::osdCenteredMsg("Refresh snapshot dir", LEVEL_INFO);
    //     deallocAluBytes();
    //     int chunks = FileUtils::DirToFile(FileUtils::MountPoint + DISK_SNA_DIR, ".sna.SNA.z80.Z80"); // Prepare sna filelist
    //     if (chunks) FileUtils::Mergefiles(FileUtils::MountPoint + DISK_SNA_DIR,chunks); // Merge files
    //     precalcAluBytes();

    //     // ESPectrum::ESPoffset = ESPectrum::ESPoffset >> 1;

    // }
    // else if (KeytoESP == fabgl::VK_F10) {

    //     OSD::osdCenteredMsg("Refresh tape dir", LEVEL_INFO);
    //     deallocAluBytes();
    //     int chunks = FileUtils::DirToFile(FileUtils::MountPoint + DISK_TAP_DIR, ".tap.TAP"); // Prepare tap filelist
    //     if (chunks) FileUtils::Mergefiles(FileUtils::MountPoint + DISK_TAP_DIR,chunks); // Merge files
    //     precalcAluBytes();

    //     // ESPectrum::ESPoffset = ESPectrum::ESPoffset << 1;

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
        AySound::disable();

        // Main menu
        uint8_t opt = menuRun(MENU_MAIN[Config::lang]);
        if (opt == 1) {
            // Snapshot menu
            uint8_t sna_mnu = menuRun(MENU_SNA[Config::lang]);
            if (sna_mnu > 0) {
                if (sna_mnu == 1) {
                    string mFile = menuFile(FileUtils::MountPoint + DISK_SNA_DIR, MENU_SNA_TITLE[Config::lang],".sna.SNA.z80.Z80");
                    if (mFile != "") {
                        changeSnapshot(mFile);
                    }
                    // unsigned short snanum = menuRun(Config::sna_name_list);
                    // if (snanum > 0) {
                    //     changeSnapshot(rowGet(Config::sna_file_list, snanum));
                    // }
                }
                else if (sna_mnu == 2) {
                    // Persist Load
                    uint8_t opt2 = menuRun(MENU_PERSIST_LOAD[Config::lang]);
                    if (opt2 > 0 && opt2<6) {
                        persistLoad(opt2);
                    }
                }
                else if (sna_mnu == 3) {
                    // Persist Save
                    uint8_t opt2 = menuRun(MENU_PERSIST_SAVE[Config::lang]);
                    if (opt2 > 0 && opt2<6) {
                        persistSave(opt2);
                    }
                }
            }
        } 
        else if (opt == 2) {
            // Tape menu
            uint8_t tap_num = menuRun(MENU_TAPE[Config::lang]);
            if (tap_num > 0) {
                if (tap_num == 1) {
                    // Select TAP File
                    string mFile = menuFile(FileUtils::MountPoint + DISK_TAP_DIR, MENU_TAP_TITLE[Config::lang],".tap.TAP");
                    if (mFile != "") {
                        Tape::tapeFileName=FileUtils::MountPoint + DISK_TAP_DIR "/" + mFile;
                    }
                }
                else if (tap_num == 2) {
                    // Start .tap reproduction
                    if (Tape::tapeFileName=="none") {
                        OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR, LEVEL_WARN);
                    } else {
                        Tape::TAP_Play();
                    }
                }
                else if (tap_num == 3) {
                    Tape::TAP_Stop();
                }
            }
        }
        else if (opt == 3) {
            // Reset
            uint8_t opt2 = menuRun(MENU_RESET[Config::lang]);
            if (opt2 == 1) {
                // Soft
                if (Config::ram_file != NO_RAM_FILE)
                    changeSnapshot(Config::ram_file);
                else ESPectrum::reset();
            }
            else if (opt2 == 2) {
                // Hard
                Config::ram_file = NO_RAM_FILE;
                Config::save();
                ESPectrum::reset();
            }
            else if (opt2 == 3) {
                // ESP host reset
                esp_hard_reset();
            }
        }
        else if (opt == 4) {
            // Tape menu
            uint8_t options_num = menuRun(MENU_OPTIONS[Config::lang]);
            if (options_num > 0) {
                if (options_num == 1) {
                    // Storage source
                    string stor_menu = MENU_STORAGE[Config::lang];
                    int curopt;
                    if (FileUtils::MountPoint == MOUNT_POINT_SPIFFS) {
                        stor_menu.replace(stor_menu.find("[I",0),2,"[*");
                        stor_menu.replace(stor_menu.find("[S",0),2,"[ ");
                        curopt = 1;
                    } else {
                        stor_menu.replace(stor_menu.find("[I",0),2,"[ ");
                        stor_menu.replace(stor_menu.find("[S",0),2,"[*");
                        curopt = 2;
                    }
                    uint8_t opt2 = menuRun(stor_menu);
                    if (opt2) {
                        if (opt2 == 3) {
                            deallocAluBytes();
                            OSD::osdCenteredMsg("Refreshing snap dir", LEVEL_INFO);
                            int chunks = FileUtils::DirToFile(FileUtils::MountPoint + DISK_SNA_DIR, ".sna.SNA.z80.Z80"); // Prepare sna filelist
                            if (chunks) FileUtils::Mergefiles(FileUtils::MountPoint + DISK_SNA_DIR,chunks); // Merge files
                            OSD::osdCenteredMsg("Refreshing tape dir", LEVEL_INFO);
                            chunks = FileUtils::DirToFile(FileUtils::MountPoint + DISK_TAP_DIR, ".tap.TAP"); // Prepare tap filelist
                            if (chunks) FileUtils::Mergefiles(FileUtils::MountPoint + DISK_TAP_DIR,chunks); // Merge files
                            precalcAluBytes();
                        } else if (opt2 != curopt) {
                            if (opt2 == 1)
                                FileUtils::MountPoint = MOUNT_POINT_SPIFFS;
                            else
                                FileUtils::MountPoint = MOUNT_POINT_SD;
                            // Config::loadSnapshotLists();
                            // Config::loadTapLists();
                            Config::save();
                        }
                    }
                }
                else if (options_num == 2) {
                    
                    // Change ROM
                    string arch_menu = getArchMenu();
                    uint8_t arch_num = menuRun(arch_menu);
                    if (arch_num) {
                        string romset_menu = getRomsetMenu((arch_num==1 ? "48K" : "128K"));
                        uint8_t romset_num = menuRun(romset_menu);
                        if (romset_num > 0) {
                            string arch = (arch_num==1 ? "48K" : "128K");
                            string romSet = rowGet(romset_menu, romset_num);
                            Config::requestMachine(arch, romSet, true);
                            Config::ram_file = "none";
                            vTaskDelay(2);
                            Config::save();
                            vTaskDelay(2);
                            ESPectrum::reset();
                        }
                    }
                }
                else if (options_num == 3) {
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
                            Config::save();
                            esp_hard_reset();
                        }
                    }
                }
                else if (options_num == 4) {
                    // aspect ratio
                    uint8_t opt2;
                    opt2 = menuRun(MENU_LANGUAGE[Config::lang]);
                    if (opt2) {
                        if (opt2 == 1) {
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
                                    Config::save();
                                }
                            }
                        } else if (opt2 == 2) {
                            string Mnustr = MENU_KBD_LAYOUT[Config::lang];
                            Mnustr.replace(Mnustr.find("[" + Config::kbd_layout),3,"[*");
                            std::size_t pos = Mnustr.find("[",0);
                            while (pos != string::npos) {
                                if (Mnustr.at(pos + 1) != (char)42) {
                                    Mnustr.replace(pos,3,"[ ");
                                }
                                pos = Mnustr.find("[",pos + 1);
                            }
                            opt2 = menuRun(Mnustr);
                            if (opt2) {
                                switch (opt2) {
                                    case 1:
                                        Config::kbd_layout = "US";
                                        break;
                                    case 2:
                                        Config::kbd_layout = "ES";
                                        break;
                                    case 3:
                                        Config::kbd_layout = "DE";
                                        break;
                                    case 4:
                                        Config::kbd_layout = "FR";
                                        break;
                                    case 5:
                                        Config::kbd_layout = "UK";
                                }
                                Config::save();
                                
                                // TO DO: Crear funcion en objeto de teclado aparte para esto
                                string cfgLayout = Config::kbd_layout;
                                if(cfgLayout == "ES") 
                                        ESPectrum::PS2Controller.keyboard()->setLayout(&fabgl::SpanishLayout);                
                                else if(cfgLayout == "UK") 
                                        ESPectrum::PS2Controller.keyboard()->setLayout(&fabgl::UKLayout);                
                                else if(cfgLayout == "DE") 
                                        ESPectrum::PS2Controller.keyboard()->setLayout(&fabgl::GermanLayout);                
                                else if(cfgLayout == "FR") 
                                        ESPectrum::PS2Controller.keyboard()->setLayout(&fabgl::FrenchLayout);            
                                else 
                                        ESPectrum::PS2Controller.keyboard()->setLayout(&fabgl::USLayout);

                            }
                        }
                    }
                }
            }
        }
        else if (opt == 5) {
            // Help
            drawOSD();
            osdAt(2, 0);
            VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(1, 0));
            VIDEO::vga.print(OSD_ABOUT[Config::lang]);
            while (1) {
                ESPectrum::readKbd(&Nextkey);
                if(!Nextkey.down) continue;
                if ((Nextkey.vk == fabgl::VK_F1) || (Nextkey.vk == fabgl::VK_ESCAPE) || (Nextkey.vk == fabgl::VK_RETURN)) break;
                vTaskDelay(5 / portTICK_PERIOD_MS);
            }
        }
        
        AySound::enable();
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

    VIDEO::vga.fillRect(x, y, w, h, paper);
    // VIDEO::vga.rect(x - 1, y - 1, w + 2, h + 2, ink);
    VIDEO::vga.setTextColor(ink, paper);
    VIDEO::vga.setFont(Font6x8);
    VIDEO::vga.setCursor(x + OSD_FONT_W, y + OSD_FONT_H);
    VIDEO::vga.print(msg.c_str());
    
    if (millispause > 0) vTaskDelay(millispause/portTICK_PERIOD_MS); // Pause if needed

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
    string dir = FileUtils::MountPoint + DISK_SNA_DIR;

    if (FileUtils::hasSNAextension(filename))
    {
    
        osdCenteredMsg(MSG_LOADING_SNA + (string) ": " + filename, LEVEL_INFO, 0);
        // printf("Loading SNA: <%s>\n", (dir + "/" + filename).c_str());
        FileSNA::load(dir + "/" + filename);

    }
    else if (FileUtils::hasZ80extension(filename))
    {
        osdCenteredMsg(MSG_LOADING_Z80 + (string)": " + filename, LEVEL_INFO, 0);
        // printf("Loading Z80: %s\n", filename.c_str());
        FileZ80::load(dir + "/" + filename);
    }

    // osdCenteredMsg(MSG_SAVE_CONFIG, LEVEL_WARN, 0);
    
    Config::ram_file = filename;
    Config::save();

}
