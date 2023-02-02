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

#define MENU_REDRAW true
#define MENU_UPDATE false
#define OSD_ERROR true
#define OSD_NORMAL false

#define OSD_W 248
#define OSD_H 168
#define OSD_MARGIN 4

extern Font Font6x8;

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
void OSD::osdHome() { CPU::vga.setCursor(osdInsideX(), osdInsideY()); }

// // Cursor positioning
void OSD::osdAt(uint8_t row, uint8_t col) {
    if (row > osdMaxRows() - 1)
        row = 0;
    if (col > osdMaxCols() - 1)
        col = 0;
    unsigned short y = (row * OSD_FONT_H) + osdInsideY();
    unsigned short x = (col * OSD_FONT_W) + osdInsideX();
    CPU::vga.setCursor(x, y);
}

void OSD::drawOSD() {
    VGA6Bit& vga = CPU::vga;
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);
    vga.fillRect(x, y, OSD_W, OSD_H, OSD::zxColor(1, 0));
    vga.rect(x, y, OSD_W, OSD_H, OSD::zxColor(0, 0));
    vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, OSD::zxColor(7, 0));
    vga.setTextColor(OSD::zxColor(0, 0), OSD::zxColor(5, 1));
    vga.setFont(Font6x8);
    osdHome();
    vga.print(OSD_TITLE);
    osdAt(19, 0);
    vga.print(OSD_BOTTOM);
    osdHome();
}

void OSD::drawStats(char *line1, char *line2) {
    VGA6Bit& vga = CPU::vga;
    unsigned short x = 168;
    unsigned short y = 220;
    // vga.fillRect(x, y, 80, 16, OSD::zxColor(1, 0));
    // vga.rect(x, y, OSD_W, OSD_H, OSD::zxColor(0, 0));
    // vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, OSD::zxColor(7, 0));
    vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(1, 0));
    vga.setFont(Font6x8);
    vga.setCursor(x,y);
    vga.print(line1);
    vga.setCursor(x,y+8);
    vga.print(line2);

}

// static void quickSave()
// {
//     OSD::osdCenteredMsg(OSD_QSNA_SAVING, LEVEL_INFO);
//     if (!FileSNA::saveQuick()) {
//         OSD::osdCenteredMsg(OSD_QSNA_SAVE_ERR, LEVEL_WARN);
//         return;
//     }
//     OSD::osdCenteredMsg(OSD_QSNA_SAVED, LEVEL_INFO);
// }

// static void quickLoad()
// {
//     if (!FileSNA::isQuickAvailable()) {
//         OSD::osdCenteredMsg(OSD_QSNA_NOT_AVAIL, LEVEL_INFO);
//         return;
//     }
//     OSD::osdCenteredMsg(OSD_QSNA_LOADING, LEVEL_INFO);
//     if (!FileSNA::loadQuick()) {
//         OSD::osdCenteredMsg(OSD_QSNA_LOAD_ERR, LEVEL_WARN);
//         return;
//     }
//     if (Config::getArch() == "48K") AySound::reset();
//     if (Config::getArch() == "48K") ESPectrum::samplesPerFrame=546; else ESPectrum::samplesPerFrame=554;
//     OSD::osdCenteredMsg(OSD_QSNA_LOADED, LEVEL_INFO);
// }

static void persistSave(uint8_t slotnumber)
{
    char persistfname[strlen(DISK_PSNA_FILE) + 6];
    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    OSD::osdCenteredMsg(OSD_PSNA_SAVING, LEVEL_INFO, 0);
    if (!FileSNA::save(persistfname)) {
        OSD::osdCenteredMsg(OSD_PSNA_SAVE_ERR, LEVEL_WARN);
        return;
    }
    OSD::osdCenteredMsg(OSD_PSNA_SAVED, LEVEL_INFO);
}

static void persistLoad(uint8_t slotnumber)
{
    char persistfname[strlen(DISK_PSNA_FILE) + 6];
    sprintf(persistfname,DISK_PSNA_FILE "%u.sna",slotnumber);
    if (!FileSNA::isPersistAvailable(persistfname)) {
        OSD::osdCenteredMsg(OSD_PSNA_NOT_AVAIL, LEVEL_INFO);
        return;
    }
    OSD::osdCenteredMsg(OSD_PSNA_LOADING, LEVEL_INFO);
    if (!FileSNA::load(persistfname)) {
         OSD::osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
    }
    if (Config::getArch() == "48K") AySound::reset();
    if (Config::getArch() == "48K") ESPectrum::samplesPerFrame=546; else ESPectrum::samplesPerFrame=554;
    OSD::osdCenteredMsg(OSD_PSNA_LOADED, LEVEL_INFO);
}

// OSD Main Loop
void OSD::do_OSD(fabgl::VirtualKey KeytoESP) {

    VGA6Bit& vga = CPU::vga;
    static uint8_t last_sna_row = 0;
    auto kbd = ESPectrum::PS2Controller.keyboard();
    bool Kdown;

    if (KeytoESP == fabgl::VK_PAUSE) {
        AySound::disable();
        osdCenteredMsg(OSD_PAUSE, LEVEL_INFO, 0);
        while (1) {
            KeytoESP = kbd->getNextVirtualKey(&Kdown);
            if ((Kdown) && (KeytoESP == fabgl::VK_PAUSE)) break;
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
        AySound::enable();
    }
    else if (KeytoESP == fabgl::VK_F2) {
        AySound::disable();
        // Persist Load
        uint8_t opt2 = menuRun(MENU_PERSIST_LOAD);
        if (opt2 > 0 && opt2<6) {
            persistLoad(opt2);
        }
        AySound::enable();
    }
    else if (KeytoESP == fabgl::VK_F3) {
        AySound::disable();
        // Persist Save
        uint8_t opt2 = menuRun(MENU_PERSIST_SAVE);
        if (opt2 > 0 && opt2<6) {
            persistSave(opt2);
        }
        AySound::enable();
    }
    else if (KeytoESP == fabgl::VK_F5) {
        // Start .tap reproduction
        if (Tape::tapeFileName=="none") {
            OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR, LEVEL_WARN);
        } else {
            Tape::TAP_Play();
        }
    }
    else if (KeytoESP == fabgl::VK_F6) {
        // Stop .tap reproduction
        Tape::TAP_Stop();
    }
    else if (KeytoESP == fabgl::VK_F8) {
        // Show / hide OnScreen Stats
        if (CPU::BottomDraw == BOTTOMBORDER) CPU::BottomDraw = BOTTOMBORDER_FPS; else CPU::BottomDraw = BOTTOMBORDER;
    }
    else if (KeytoESP == fabgl::VK_F9) {
        if (ESPectrum::aud_volume>-16) {
                ESPectrum::aud_volume--;
                pwm_audio_set_volume(ESPectrum::aud_volume);
        }
    }
    else if (KeytoESP == fabgl::VK_F10) {
        if (ESPectrum::aud_volume<0) {
                ESPectrum::aud_volume++;
                pwm_audio_set_volume(ESPectrum::aud_volume);
        }
    }    
    // else if (KeytoESP == fabgl::VK_F9) {
    //     ESPectrum::ESPoffset = ESPectrum::ESPoffset >> 1;
    // }
    // else if (KeytoESP == fabgl::VK_F10) {
    //     ESPectrum::ESPoffset = ESPectrum::ESPoffset << 1;
    // }
    else if (KeytoESP == fabgl::VK_F11) {
        
        // Switch boot partition
        esp_err_t chg_ota;
        const esp_partition_t *ESPectrum_partition = NULL;

        //ESPectrum_partition = esp_ota_get_next_update_partition(NULL);
        ESPectrum_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,"48k");

        chg_ota = esp_ota_set_boot_partition(ESPectrum_partition);

        if (chg_ota == ESP_OK ) esp_hard_reset();

    }
    else if (KeytoESP == fabgl::VK_F12) {
        // Switch boot partition
        esp_err_t chg_ota;
        const esp_partition_t *ESPectrum_partition = NULL;

        //ESPectrum_partition = esp_ota_get_next_update_partition(NULL);
        ESPectrum_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,"128k");

        chg_ota = esp_ota_set_boot_partition(ESPectrum_partition);

        if (chg_ota == ESP_OK ) esp_hard_reset();
            
    }
    else if (KeytoESP == fabgl::VK_F1) {
        AySound::disable();

        // Main menu
        uint8_t opt = menuRun(MENU_MAIN);
        if (opt == 1) {
            // Change RAM
            unsigned short snanum = menuRun(Config::sna_name_list);
            if (snanum > 0) {
                changeSnapshot(rowGet(Config::sna_file_list, snanum));
            }
        } 
        else if (opt == 2) {
            // Select TAP File
            unsigned short tapnum = menuRun(Config::tap_name_list);
            if (tapnum > 0) {
                Tape::tapeFileName=DISK_TAP_DIR "/" + rowGet(Config::tap_file_list, tapnum);
            }
        }
        else if (opt == 3) {
            // Change ROM
            string arch_menu = getArchMenu();
            uint8_t arch_num = menuRun(arch_menu);
            if (arch_num > 0) {
                string romset_menu = getRomsetMenu(rowGet(arch_menu, arch_num));
                uint8_t romset_num = menuRun(romset_menu);
                if (romset_num > 0) {
                    string arch = rowGet(arch_menu, arch_num);
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
        else if (opt == 4) {
            // Persist Load
            uint8_t opt2 = menuRun(MENU_PERSIST_LOAD);
            if (opt2 > 0 && opt2<6) {
                persistLoad(opt2);
            }
        }
        else if (opt == 5) {
            // Persist Save
            uint8_t opt2 = menuRun(MENU_PERSIST_SAVE);
            if (opt2 > 0 && opt2<6) {
                persistSave(opt2);
            }
        }
        else if (opt == 6) {
            // aspect ratio
            uint8_t opt2;
            if (Config::aspect_16_9)
                opt2 = menuRun(MENU_ASPECT_169);
            else
                opt2 = menuRun(MENU_ASPECT_43);
            if (opt2 == 2)
            {
                Config::aspect_16_9 = !Config::aspect_16_9;
                Config::save();
                esp_hard_reset();
            }
        }
        else if (opt == 7) {
            // Reset
            uint8_t opt2 = menuRun(MENU_RESET);
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
        else if (opt == 8) {
            // Help
            drawOSD();
            osdAt(2, 0);
            vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(1, 0));
            vga.print(OSD_HELP);
            while (1) {
                KeytoESP = kbd->getNextVirtualKey(&Kdown);
                if(!Kdown) continue;
                if ((KeytoESP == fabgl::VK_F1) || (KeytoESP == fabgl::VK_ESCAPE) || (KeytoESP == fabgl::VK_RETURN)) break;
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

    VGA6Bit& vga = CPU::vga;

    vga.fillRect(x, y, OSD_W, OSD_H, OSD::zxColor(0, 0));
    vga.rect(x, y, OSD_W, OSD_H, OSD::zxColor(7, 0));
    vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, OSD::zxColor(2, 1));
    vga.setFont(Font6x8);
    osdHome();
    vga.setTextColor(OSD::zxColor(7, 1), OSD::zxColor(2, 1));
    vga.print(ERROR_TITLE);
    osdAt(2, 0);
    vga.setTextColor(OSD::zxColor(7, 1), OSD::zxColor(0, 0));
    vga.println(errormsg.c_str());
    osdAt(17, 0);
    vga.setTextColor(OSD::zxColor(7, 1), OSD::zxColor(2, 1));
    vga.print(ERROR_BOTTOM);
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

    VGA6Bit& vga = CPU::vga;

    vga.fillRect(x, y, w, h, paper);
    // vga.rect(x - 1, y - 1, w + 2, h + 2, ink);
    vga.setTextColor(ink, paper);
    vga.setFont(Font6x8);
    vga.setCursor(x + OSD_FONT_W, y + OSD_FONT_H);
    vga.print(msg.c_str());
    
    if (millispause > 0) vTaskDelay(millispause/portTICK_PERIOD_MS); // Pause if needed

}

// // Count NL chars inside a string, useful to count menu rows
unsigned short OSD::rowCount(string menu) {
    unsigned short count = 0;
    for (unsigned short i = 1; i < menu.length(); i++) {
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
    return "MENU ERROR! (Unknown row?)";
}

// Change running snapshot
void OSD::changeSnapshot(string filename)
{
    string dir = DISK_SNA_DIR;

    if (FileUtils::hasSNAextension(filename))
    {
    
        osdCenteredMsg(MSG_LOADING_SNA + (string) ": " + filename, LEVEL_INFO, 0);
        // printf("Loading SNA: %s\n", filename.c_str());
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
