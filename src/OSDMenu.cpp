///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
//
// Copyright (c) 2023 Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
// https://github.com/EremusOne/ZX-ESPectrum-IDF
//
// Based on ZX-ESPectrum-Wiimote
// Copyright (c) 2020, 2022 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez and Jorge Fuertes
// https://github.com/rampa069/ZX-ESPectrum
//
// Original project by Pete Todd
// https://github.com/retrogubbins/paseVGA
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

//#include <stdio.h>
#include <string>
//#include <inttypes.h>
//#include <cctype>
#include <algorithm>
//#include <locale>

#include <sys/stat.h>
#include "errno.h"

using namespace std;

#include "FileUtils.h"
#include "Config.h"
#include "ESPectrum.h"
#include "CPU.h"
#include "Video.h"
#include "messages.h"
#include "OSDMain.h"
#include <math.h>
#include "ZXKeyb.h"
#include "pwm_audio.h"
#include "Z80_JLS/z80.h"

// #include <cctype>
// #include <algorithm>
// #include <locale>
// #include "hardconfig.h"
// #include "Config.h"
// #include "FileUtils.h"
// #include "messages.h"
// #include "ESPectrum.h"
// #include "esp_spiffs.h"

#define MENU_MAX_ROWS 18
// Line type
#define IS_TITLE 0
#define IS_FOCUSED 1
#define IS_NORMAL 2
// Scroll
#define UP true
#define DOWN false

extern Font Font6x8;

static uint8_t cols;                     // Maximum columns
static unsigned short real_rows;      // Real row count
static uint8_t virtual_rows;             // Virtual maximum rows on screen
static uint8_t w;                        // Width in pixels
static uint8_t h;                        // Height in pixels
static uint8_t x;                        // X vertical position
static uint8_t y;                        // Y horizontal position
static string menu;                   // Menu string
static unsigned short begin_row;      // First real displayed row
static uint8_t focus;                    // Focused virtual row
static uint8_t last_focus;               // To check for changes
static unsigned short last_begin_row; // To check for changes


uint8_t DRAM_ATTR click48[12]={0,0x16,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x16,0};
uint8_t DRAM_ATTR click128[116]= {  0x00,0x16,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                    0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                    0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                    0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                    0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                    0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                    0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61,
                                    0x61,0x61,0x16,0x00
                                };

void IRAM_ATTR OSD::click() {

    size_t written;

    pwm_audio_set_volume(ESP_DEFAULT_VOLUME);

    if (Z80Ops::is48) {
        pwm_audio_write(click48, 12, &written,  5 / portTICK_PERIOD_MS);
    } else {
        pwm_audio_write(click128, 116, &written, 5 / portTICK_PERIOD_MS);
    }

    pwm_audio_set_volume(ESPectrum::aud_volume);

    // printf("Written: %d\n",written);

}

uint16_t OSD::zxColor(uint8_t color, uint8_t bright) {
    if (bright) color += 8;
    return spectrum_colors[color];
}

// Set menu and force recalc
void OSD::newMenu(string new_menu) {
    menu = new_menu;
    menuRecalc();
    menuDraw();
}

void OSD::menuRecalc() {

    // Position
    if (menu_level == 0) {
        x = (Config::aspect_16_9 ? 24 : 8);
        y = 8;
    } else {
//        x = (Config::aspect_16_9 ? 24 : 8) + ((((cols >> 1) - 3)* 6) * menu_level);
        x = (Config::aspect_16_9 ? 24 : 8) + (60 * menu_level);
        y = 8 + (16 * menu_level);
    }

    // Rows
    real_rows = rowCount(menu);
    virtual_rows = (real_rows > MENU_MAX_ROWS ? MENU_MAX_ROWS : real_rows);
    begin_row = last_begin_row = last_focus = focus = 1;

    // Columns
    cols = 0;
    uint8_t col_count = 0;
    for (unsigned short i = 0; i < menu.length(); i++) {
        if ((menu.at(i) == ASCII_TAB) || (menu.at(i) == ASCII_NL)) {
            if (col_count > cols) {
                cols = col_count;
            }
            while (menu.at(i) != ASCII_NL) i++;
            col_count = 0;
        }
        col_count++;
    }
    cols += 8;
    cols = (cols > 28 ? 28 : cols);

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

}

// Get real row number for a virtual one
unsigned short OSD::menuRealRowFor(uint8_t virtual_row_num) { return begin_row + virtual_row_num - 1; }

// Get real row number for a virtual one
bool OSD::menuIsSub(uint8_t virtual_row_num) { 
    string line = rowGet(menu, menuRealRowFor(virtual_row_num));
    int n = line.find(ASCII_TAB);
    if (n == line.npos) return false;
    return (line.substr(n+1).find(">") != line.npos);
}

// Menu relative AT
void OSD::menuAt(short int row, short int col) {
    if (col < 0)
        col = cols - 2 - col;
    if (row < 0)
        row = virtual_rows - 2 - row;
    VIDEO::VIDEO::vga.setCursor(x + 1 + (col * OSD_FONT_W), y + 1 + (row * OSD_FONT_H));
}

// Print a virtual row
void OSD::menuPrintRow(uint8_t virtual_row_num, uint8_t line_type) {
    
    uint8_t margin;

    string line = rowGet(menu, menuRealRowFor(virtual_row_num));
    
    switch (line_type) {
    case IS_TITLE:
        VIDEO::vga.setTextColor(OSD::zxColor(7, 1), OSD::zxColor(0, 0));
        margin = 2;
        break;
    case IS_FOCUSED:
        VIDEO::vga.setTextColor(OSD::zxColor(0, 1), OSD::zxColor(5, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    default:
        VIDEO::vga.setTextColor(OSD::zxColor(0, 1), OSD::zxColor(7, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
    }

    if (line.find(ASCII_TAB) != line.npos) {
        line = line.substr(0,line.find(ASCII_TAB)) + string(cols - margin - line.length(),' ') + line.substr(line.find(ASCII_TAB)+1);
    }

    menuAt(virtual_row_num, 0);

    VIDEO::vga.print(" ");

    if (line.length() < cols - margin) {
    VIDEO::vga.print(line.c_str());
    for (uint8_t i = line.length(); i < (cols - margin); i++)
        VIDEO::vga.print(" ");
    } else {
        VIDEO::vga.print(line.substr(0, cols - margin).c_str());
    }

    VIDEO::vga.print(" ");

}

static int SaveRectpos = 0;

// Draw the complete menu
void OSD::menuDraw() {

    // Set font
    VIDEO::vga.setFont(Font6x8);

    if (menu_level!=0) {
        if (menu_saverect) {
            // Save backbuffer data
            VIDEO::SaveRect[SaveRectpos] = x;
            VIDEO::SaveRect[SaveRectpos + 1] = y;
            VIDEO::SaveRect[SaveRectpos + 2] = w;
            VIDEO::SaveRect[SaveRectpos + 3] = h;
            SaveRectpos += 4;
            for (int  m = y; m < y + h; m++) {
                uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
                for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                    VIDEO::SaveRect[SaveRectpos] = backbuffer32[n];
                    SaveRectpos++;
                }
            }
            printf("Saverectpos after save: %d\n",SaveRectpos);
        }
    } else SaveRectpos = 0;

    // Menu border
    VIDEO::vga.rect(x, y, w, h, OSD::zxColor(0, 0));

    // Title
    menuPrintRow(0, IS_TITLE);
    // Rainbow
    unsigned short rb_y = y + 8;
    unsigned short rb_paint_x = x + w - 30;
    uint8_t rb_colors[] = {2, 6, 4, 5};
    for (uint8_t c = 0; c < 4; c++) {
        for (uint8_t i = 0; i < 5; i++) {
            VIDEO::vga.line(rb_paint_x + i, rb_y, rb_paint_x + 8 + i, rb_y - 8, OSD::zxColor(rb_colors[c], 1));
        }
        rb_paint_x += 5;
    }
    // Focused first line
    // menuPrintRow(1, IS_FOCUSED);
    for (uint8_t r = 1; r < virtual_rows; r++) {
        menuPrintRow(r, r == menu_curopt ? IS_FOCUSED : IS_NORMAL);
    }

    focus = menu_curopt;

    menuScrollBar();
}

string OSD::getArchMenu() {
    string menu = (string)MENU_ARCH[Config::lang]; // + FileUtils::getFileEntriesFromDir(DISK_ROM_DIR);
    return menu;
}

string OSD::getRomsetMenu(string arch) {
    string menu;
    if (arch == "48K") {
        menu = (string)MENU_ROMSET48[Config::lang]; // + FileUtils::getFileEntriesFromDir((string)DISK_ROM_DIR + "/" + arch);
    } else {
        menu = (string)MENU_ROMSET128[Config::lang];
    }
    return menu;
}

#ifdef ZXKEYB
#define REPDEL 140 // As in real ZX Spectrum (700 ms.)
#define REPPER 20 // As in real ZX Spectrum (100 ms.)
static int zxDelay = 0;
static int lastzxKey = 0;
#endif

// Run a new menu
unsigned short OSD::menuRun(string new_menu) {

    fabgl::VirtualKeyItem Menukey;    

    newMenu(new_menu);

    #ifdef ZXKEYB
    zxDelay = REPDEL;
    lastzxKey = 0;
    #endif

    while (1) {
        
        #ifdef ZXKEYB
        
        ZXKeyb::process();

        if (!bitRead(ZXKeyb::ZXcols[4], 3)) { // 6 DOWN
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_UP, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_UP, false, false);                
                if (lastzxKey == 1)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 1;
            }
        } else
        if (!bitRead(ZXKeyb::ZXcols[4], 4)) { // 7 UP (Yes, like the drink's name, I know... :D)
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_DOWN, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_DOWN, false, false);                
                if (lastzxKey == 2)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 2;
            }
        } else
        if ((!bitRead(ZXKeyb::ZXcols[6], 0)) || (!bitRead(ZXKeyb::ZXcols[4], 0))) { // ENTER
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_RETURN, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_RETURN, false, false);                
                if (lastzxKey == 3)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 3;
            }
        } else
        if ((!bitRead(ZXKeyb::ZXcols[7], 0)) || (!bitRead(ZXKeyb::ZXcols[4], 1))) { // BREAK
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE, false, false);
                if (lastzxKey == 4)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 4;
            }
        } else
        if (!bitRead(ZXKeyb::ZXcols[3], 4)) { // LEFT
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PAGEUP, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PAGEUP, false, false);
                if (lastzxKey == 5)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 5;
            }
        } else
        if (!bitRead(ZXKeyb::ZXcols[4], 2)) { // RIGHT
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PAGEDOWN, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PAGEDOWN, false, false);
                if (lastzxKey == 6)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 6;
            }
        } else
        {
            zxDelay = 0;
            lastzxKey = 0;
        }

        #endif
       
        // Process external keyboard
        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            if (ESPectrum::readKbd(&Menukey)) {
                if (!Menukey.down) continue;
                if (Menukey.vk == fabgl::VK_UP) {
                    if (focus == 1 and begin_row > 1) {
                        menuScroll(DOWN);
                    } else {
                        last_focus = focus;
                        focus--;
                        if (focus < 1) {
                            focus = virtual_rows - 1;
                            last_begin_row = begin_row;
                            begin_row = real_rows - virtual_rows + 1;
                            menuRedraw();
                            menuPrintRow(focus, IS_FOCUSED);
                        }
                        else {
                            menuPrintRow(focus, IS_FOCUSED);
                            menuPrintRow(last_focus, IS_NORMAL);
                        }
                    }
                    click();
                } else if (Menukey.vk == fabgl::VK_DOWN) {
                    if (focus == virtual_rows - 1 && virtual_rows + begin_row - 1 < real_rows) {                
                        menuScroll(UP);
                    } else {
                        last_focus = focus;
                        focus++;
                        if (focus > virtual_rows - 1) {
                            focus = 1;
                            last_begin_row = begin_row;
                            begin_row = 1;
                            menuRedraw();
                            menuPrintRow(focus, IS_FOCUSED);
                        }
                        else {
                            menuPrintRow(focus, IS_FOCUSED);
                            menuPrintRow(last_focus, IS_NORMAL);                
                        }
                    }
                    click();
                } else if ((Menukey.vk == fabgl::VK_PAGEUP) || (Menukey.vk == fabgl::VK_LEFT)) {
                    if (begin_row > virtual_rows) {
                        focus = 1;
                        begin_row -= virtual_rows;
                    } else {
                        focus = 1;
                        begin_row = 1;
                    }
                    menuRedraw();
                    click();
                } else if ((Menukey.vk == fabgl::VK_PAGEDOWN) || (Menukey.vk == fabgl::VK_RIGHT)) {
                    if (real_rows - begin_row  - virtual_rows > virtual_rows) {
                        focus = 1;
                        begin_row += virtual_rows - 1;
                    } else {
                        focus = virtual_rows - 1;
                        begin_row = real_rows - virtual_rows + 1;
                    }
                    menuRedraw();
                    click();
                } else if (Menukey.vk == fabgl::VK_HOME) {
                    focus = 1;
                    begin_row = 1;
                    menuRedraw();
                    click();
                } else if (Menukey.vk == fabgl::VK_END) {
                    focus = virtual_rows - 1;
                    begin_row = real_rows - virtual_rows + 1;
                    menuRedraw();
                    click();
                } else if (Menukey.vk == fabgl::VK_RETURN) {
                    click();
                    return menuRealRowFor(focus);
                } else if ((Menukey.vk == fabgl::VK_ESCAPE) || (Menukey.vk == fabgl::VK_F1)) {

                    if (menu_level!=0) {
                        // Restore backbuffer data
                        int j = SaveRectpos - (((w >> 2) + 1) * h);
                        printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        SaveRectpos = j - 4;
                        for (int  m = y; m < y + h; m++) {
                            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
                            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                                backbuffer32[n] = VIDEO::SaveRect[j];
                                j++;
                            }
                        }
                        printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        menu_saverect = false;                        
                    }

                    click();
                    return 0;
                }
            }
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
        
        #ifdef ZXKEYB        
        if (zxDelay > 0) zxDelay--;
        #endif

    }

}

// fabgl::VirtualKey menuVKs[] = {
//     fabgl::VK_UP, fabgl::VK_DOWN, fabgl::VK_RETURN, fabgl::VK_ESCAPE, fabgl::VK_F1,
//     fabgl::VK_PAGEUP, fabgl::VK_PAGEDOWN, fabgl::VK_HOME, fabgl::VK_END
// };

// #define STARTKBDREPEATMS 140 // As in real ZX Spectrum (700 ms.)
// #define KBDREPEATMS 20 // As in real ZX Spectrum (100 ms.)

// // Run a new menu
// unsigned short OSD::menuRun_alt(string new_menu) {

//     fabgl::VirtualKey Menukey;    

//     int kbdDelay = STARTKBDREPEATMS;

//     // Make sure no menu key keeps pressed
//     for (int i=0; i < (sizeof(menuVKs)/sizeof(menuVKs[0])); i++) {
//         while(ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {
//             vTaskDelay(5 / portTICK_PERIOD_MS);                    
//         }
//     }

//     newMenu(new_menu);
//     while (1) {
//         bool noKbd = true;
//         for (int i=0; i < (sizeof(menuVKs)/sizeof(menuVKs[0])); i++) {

//             if (ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {

//                 noKbd = false;

//                 Menukey = menuVKs[i];

//                 if (Menukey == fabgl::VK_UP) {
//                     if (focus == 1 and begin_row > 1) {
//                         menuScroll(DOWN);
//                     } else {
//                         last_focus = focus;
//                         focus--;
//                         if (focus < 1) {
//                             focus = virtual_rows - 1;
//                             last_begin_row = begin_row;
//                             begin_row = real_rows - virtual_rows + 1;
//                             menuRedraw();
//                             menuPrintRow(focus, IS_FOCUSED);
//                         }
//                         else {
//                             menuPrintRow(focus, IS_FOCUSED);
//                             menuPrintRow(last_focus, IS_NORMAL);
//                         }
//                     }
//                 } else if (Menukey == fabgl::VK_DOWN) {
//                     if (focus == virtual_rows - 1 && virtual_rows + begin_row - 1 < real_rows) {                
//                         menuScroll(UP);
//                     } else {
//                         last_focus = focus;
//                         focus++;
//                         if (focus > virtual_rows - 1) {
//                             focus = 1;
//                             last_begin_row = begin_row;
//                             begin_row = 1;
//                             menuRedraw();
//                             menuPrintRow(focus, IS_FOCUSED);
//                         }
//                         else {
//                             menuPrintRow(focus, IS_FOCUSED);
//                             menuPrintRow(last_focus, IS_NORMAL);                
//                         }
//                     }
//                 } else if (Menukey == fabgl::VK_PAGEUP) {
//                     if (begin_row > virtual_rows) {
//                         focus = 1;
//                         begin_row -= virtual_rows;
//                     } else {
//                         focus = 1;
//                         begin_row = 1;
//                     }
//                     menuRedraw();
//                 } else if (Menukey == fabgl::VK_PAGEDOWN) {
//                     if (real_rows - begin_row  - virtual_rows > virtual_rows) {
//                         focus = 1;
//                         begin_row += virtual_rows - 1;
//                     } else {
//                         focus = virtual_rows - 1;
//                         begin_row = real_rows - virtual_rows + 1;
//                     }
//                     menuRedraw();
//                 } else if (Menukey == fabgl::VK_HOME) {
//                     focus = 1;
//                     begin_row = 1;
//                     menuRedraw();
//                 } else if (Menukey == fabgl::VK_END) {
//                     focus = virtual_rows - 1;
//                     begin_row = real_rows - virtual_rows + 1;
//                     menuRedraw();
//                 } else if (Menukey == fabgl::VK_RETURN) {

//                     while(ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {
//                         vTaskDelay(5 / portTICK_PERIOD_MS);                    
//                     }

//                     if (!menuIsSub(focus)) menu_level=0; 

//                     return menuRealRowFor(focus);

//                 } else if ((Menukey == fabgl::VK_ESCAPE) || (Menukey == fabgl::VK_F1)) {

//                     while(ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {
//                         vTaskDelay(5 / portTICK_PERIOD_MS);                    
//                     }

//                     menu_level=0;
                    
//                     return 0;
                
//                 }

//                 // Key repeat management
//                 int kbdDelayCnt = 0;
//                 while(ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {
//                     vTaskDelay(5 / portTICK_PERIOD_MS);                    
//                     if (kbdDelayCnt++ == kbdDelay) {
//                         kbdDelay = KBDREPEATMS;
//                         break;
//                     }
//                 }

//             }
//         }
//         if (noKbd) kbdDelay = STARTKBDREPEATMS;
//     }
// }

// Scroll
void OSD::menuScroll(bool dir) {
    if ((dir == DOWN) && (begin_row > 1)) {
        last_begin_row = begin_row;
        begin_row--;
    } else if (dir == UP and (begin_row + virtual_rows - 1) < real_rows) {
        last_begin_row = begin_row;
        begin_row++;
    } else {
        return;
    }
    menuRedraw();
}

// Redraw inside rows
void OSD::menuRedraw() {
    if ((focus != last_focus) || (begin_row != last_begin_row)) {
        for (uint8_t row = 1; row < virtual_rows; row++) {
            if (row == focus) {
                menuPrintRow(row, IS_FOCUSED);
            } else {
                menuPrintRow(row, IS_NORMAL);
            }
        }
        menuScrollBar();
        last_focus = focus;
        last_begin_row = begin_row;
    }
}

// Draw menu scroll bar
void OSD::menuScrollBar() {

    if (real_rows > virtual_rows) {
        // Top handle
        menuAt(1, -1);
        if (begin_row > 1) {
            VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
            VIDEO::vga.print("+");
        } else {
            VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
            VIDEO::vga.print("-");
        }

        // Complete bar
        unsigned short holder_x = x + (OSD_FONT_W * (cols - 1)) + 1;
        unsigned short holder_y = y + (OSD_FONT_H * 2);
        unsigned short holder_h = OSD_FONT_H * (virtual_rows - 3);
        unsigned short holder_w = OSD_FONT_W;
        VIDEO::vga.fillRect(holder_x, holder_y, holder_w, holder_h + 1, OSD::zxColor(7, 0));
        holder_y++;

        // Scroll bar
        unsigned long shown_pct = round(((float)virtual_rows / (float)real_rows) * 100.0);
        unsigned long begin_pct = round(((float)(begin_row - 1) / (float)real_rows) * 100.0);
        unsigned long bar_h = round(((float)holder_h / 100.0) * (float)shown_pct);
        unsigned long bar_y = round(((float)holder_h / 100.0) * (float)begin_pct);
        
        while ((bar_y + bar_h) >= holder_h) {
            bar_h--;
        }

        if (bar_h == 0) bar_h = 1;

        VIDEO::vga.fillRect(holder_x + 1, holder_y + bar_y, holder_w - 2, bar_h, OSD::zxColor(0, 0));

        // Bottom handle
        menuAt(-1, -1);
        if ((begin_row + virtual_rows - 1) < real_rows) {
            VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
            VIDEO::vga.print("+");
        } else {
            VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
            VIDEO::vga.print("-");
        }
    }
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

FILE *dirfile;

// Run a new file menu
string OSD::menuFile(string filedir, string title, string extensions) {

    fabgl::VirtualKeyItem Menukey;

    // Get dir file size and use it for calc dialog rows
    struct stat stat_buf;
    int rc;

    rc = stat((filedir + "/.d").c_str(), &stat_buf);
    if (rc < 0) {
        // deallocAluBytes();
        OSD::osdCenteredMsg("Please wait: sorting directory", LEVEL_INFO);
        int chunks = FileUtils::DirToFile(filedir, extensions); // Prepare sna filelist
        if (chunks) FileUtils::Mergefiles(filedir,chunks); // Merge files
        OSD::osdCenteredMsg(" Done: directory index ready  ", LEVEL_INFO);
        // precalcAluBytes();
        rc = stat((filedir + "/.d").c_str(), &stat_buf);
    }
    
    // Open dir file for read
    dirfile = fopen((filedir + "/.d").c_str(), "r");
    if (dirfile == NULL) {
        printf("Error opening dir file\n");
        return "";
    }

    real_rows = (stat_buf.st_size / 32) + 1; // Add 1 for title
    virtual_rows = (real_rows > 19 ? 19 : real_rows);
    begin_row = last_begin_row = last_focus = focus = 1;
    
    printf("Real rows: %d; st_size: %d\n",real_rows,stat_buf.st_size);

    // Get first bunch of rows
    menu = title + "\n";
    for (int i = 1; i < virtual_rows; i++) {
        char buf[256];
        fgets(buf, sizeof(buf), dirfile);
        if (feof(dirfile)) break;
        menu += buf;
    }


    // Position
    if (menu_level == 0) {
        x = (Config::aspect_16_9 ? 24 : 8);
        y = 8;
    } else {
        // x = (Config::aspect_16_9 ? 24 : 8) + ((((cols >> 1) - 3) * 6) * menu_level);
        x = (Config::aspect_16_9 ? 24 : 8) + (60 * menu_level);
        y = 8 + (16 * menu_level);
    }

    // Columns
    cols = 31; // 28 for filename + 2 pre and post space + 1 for scrollbar

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

    menuDraw();

    #ifdef ZXKEYB
    zxDelay = REPDEL;
    lastzxKey = 0;
    #endif

    while (1) {

        #ifdef ZXKEYB

        ZXKeyb::process();

        if (!bitRead(ZXKeyb::ZXcols[4], 3)) { // 6 DOWN
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_UP, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_UP, false, false);                
                if (lastzxKey == 1)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 1;
            }
        } else
        if (!bitRead(ZXKeyb::ZXcols[4], 4)) { // 7 UP (Yes, like the drink's name, I know... :D)
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_DOWN, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_DOWN, false, false);                
                if (lastzxKey == 2)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 2;
            }
        } else
        if ((!bitRead(ZXKeyb::ZXcols[6], 0)) || (!bitRead(ZXKeyb::ZXcols[4], 0))) { // ENTER
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_RETURN, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_RETURN, false, false);                
                if (lastzxKey == 3)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 3;
            }
        } else
        if ((!bitRead(ZXKeyb::ZXcols[7], 0)) || (!bitRead(ZXKeyb::ZXcols[4], 1))) { // BREAK        
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE, false, false);                
                if (lastzxKey == 4)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 4;
            }
        } else
        if (!bitRead(ZXKeyb::ZXcols[3], 4)) { // LEFT
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PAGEUP, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PAGEUP, false, false);
                if (lastzxKey == 5)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 5;
            }
        } else
        if (!bitRead(ZXKeyb::ZXcols[4], 2)) { // RIGHT
            if (zxDelay == 0) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PAGEDOWN, true, false);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_PAGEDOWN, false, false);                
                if (lastzxKey == 6)
                    zxDelay = REPPER;
                else
                    zxDelay = REPDEL;
                lastzxKey = 6;
            }
        } else
        {
            zxDelay = 0;
            lastzxKey = 0;
        }

        #endif
        
        // Process external keyboard
        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            if (ESPectrum::readKbd(&Menukey)) {
                if (!Menukey.down) continue;

                // // Search first ocurrence of letter if we're not on that letter yet
                // if (((Menukey.vk >= fabgl::VK_a) && (Menukey.vk <= fabgl::VK_Z)) || ((Menukey.vk >= fabgl::VK_0) && (Menukey.vk <= fabgl::VK_9))) {
                //     uint8_t dif;
                //     if (Menukey.vk<=fabgl::VK_9) dif = 46;
                //         else if (Menukey.vk<=fabgl::VK_z) dif = 75;
                //             else if (Menukey.vk<=fabgl::VK_Z) dif = 17;
                //     uint8_t letra = rowGet(menu,focus).at(0);
                //     if (letra != Menukey.vk + dif) { 
                //             // TO DO: Position on first ocurrence of letra
                //             filemenuRedraw(title);
                //     }
                /*} else*/ if (Menukey.vk == fabgl::VK_UP) {
                    if (focus == 1 and begin_row > 1) {
                        // filemenuScroll(DOWN);
                        if (begin_row > 1) {
                            last_begin_row = begin_row;
                            begin_row--;
                        }
                        filemenuRedraw(title);
                    } else if (focus > 1) {
                        last_focus = focus;
                        focus--;
                        filemenuPrintRow(focus, IS_FOCUSED);
                        if (focus + 1 < virtual_rows) {
                            filemenuPrintRow(focus + 1, IS_NORMAL);
                        }
                    }
                    click();
                } else if (Menukey.vk == fabgl::VK_DOWN) {
                    if (focus == virtual_rows - 1) {
                        if ((begin_row + virtual_rows - 1) < real_rows) {
                            last_begin_row = begin_row;
                            begin_row++;
                        }
                        filemenuRedraw(title);
                    } else if (focus < virtual_rows - 1) {
                        last_focus = focus;
                        focus++;
                        filemenuPrintRow(focus, IS_FOCUSED);
                        if (focus - 1 > 0) {
                            filemenuPrintRow(focus - 1, IS_NORMAL);
                        }
                    }
                    click();
                } else if ((Menukey.vk == fabgl::VK_PAGEUP) || (Menukey.vk == fabgl::VK_LEFT)) {
                    if (begin_row > virtual_rows) {
                        focus = 1;
                        begin_row -= virtual_rows;
                    } else {
                        focus = 1;
                        begin_row = 1;
                    }
                    filemenuRedraw(title);
                    click();
                } else if ((Menukey.vk == fabgl::VK_PAGEDOWN) || (Menukey.vk == fabgl::VK_RIGHT)) {
                    if (real_rows - begin_row  - virtual_rows > virtual_rows) {
                        focus = 1;
                        begin_row += virtual_rows - 1;
                    } else {
                        focus = virtual_rows - 1;
                        begin_row = real_rows - virtual_rows + 1;
                    }
                    filemenuRedraw(title);
                    click();
                } else if (Menukey.vk == fabgl::VK_HOME) {
                    focus = 1;
                    begin_row = 1;
                    filemenuRedraw(title);
                    click();
                } else if (Menukey.vk == fabgl::VK_END) {
                    focus = virtual_rows - 1;
                    begin_row = real_rows - virtual_rows + 1;
                    filemenuRedraw(title);
                    click();
                } else if (Menukey.vk == fabgl::VK_RETURN) {
                    fclose(dirfile);
                    filedir = rowGet(menu,focus);
                    rtrim(filedir);
                    click();
                    return filedir;
                } else if (Menukey.vk == fabgl::VK_ESCAPE) {

                    if (menu_level!=0) {
                        // Restore backbuffer data
                        int j = SaveRectpos - (((w >> 2) + 1) * h);
                        printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        SaveRectpos = j - 4;
                        for (int  m = y; m < y + h; m++) {
                            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
                            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                                backbuffer32[n] = VIDEO::SaveRect[j];
                                j++;
                            }
                        }
                        printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        menu_saverect = false;
                    }

                    fclose(dirfile);
                    click();
                    return "";
                }
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);    

        #ifdef ZXKEYB        
        if (zxDelay > 0) zxDelay--;
        #endif


    }
}


// // Run a new file menu
// string OSD::menuFile_alt(string filedir, string title, string extensions) {

//     fabgl::VirtualKey Menukey;

//     // Get dir file size and use it for calc dialog rows
//     struct stat stat_buf;
//     int rc;

//     rc = stat((filedir + "/.d").c_str(), &stat_buf);
//     if (rc < 0) {
//         // deallocAluBytes();
//         OSD::osdCenteredMsg("Please wait: sorting directory", LEVEL_INFO);
//         int chunks = FileUtils::DirToFile(filedir, extensions); // Prepare sna filelist
//         if (chunks) FileUtils::Mergefiles(filedir,chunks); // Merge files
//         OSD::osdCenteredMsg(" Done: directory index ready  ", LEVEL_INFO);
//         // precalcAluBytes();
//         rc = stat((filedir + "/.d").c_str(), &stat_buf);
//     }
    
//     // Open dir file for read
//     dirfile = fopen((filedir + "/.d").c_str(), "r");
//     if (dirfile == NULL) {
//         printf("Error opening dir file\n");
//         menu_level = 0;
//         return "";
//     }

//     real_rows = (stat_buf.st_size / 32) + 1; // Add 1 for title
//     virtual_rows = (real_rows > 19 ? 19 : real_rows);
//     begin_row = last_begin_row = last_focus = focus = 1;
    
//     printf("Real rows: %d; st_size: %d\n",real_rows,stat_buf.st_size);

//     // Get first bunch of rows
//     menu = title + "\n";
//     for (int i = 1; i < virtual_rows; i++) {
//         char buf[256];
//         fgets(buf, sizeof(buf), dirfile);
//         if (feof(dirfile)) break;
//         menu += buf;
//     }


//     // Position
//     if (menu_level == 0) {
//         x = (Config::aspect_16_9 ? 24 : 8);
//         y = 8;
//     } else {
//         // x = (Config::aspect_16_9 ? 59 : 39);
//         // y = 40;
//         x = x + (((cols >> 1) - 3)* 6);
//         y = y + 16;
//     }

//     // Columns
//     cols = 31; // 28 for filename + 2 pre and post space + 1 for scrollbar

//     // Size
//     w = (cols * OSD_FONT_W) + 2;
//     h = (virtual_rows * OSD_FONT_H) + 2;

//     int kbdDelay = STARTKBDREPEATMS;

//     // Make sure no menu key keeps pressed
//     for (int i=0; i < (sizeof(menuVKs)/sizeof(menuVKs[0])); i++) {
//         while(ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {
//             vTaskDelay(5 / portTICK_PERIOD_MS);                    
//         }
//     }

//     menuDraw();
    
//     while (1) {

//         bool noKbd = true;

//         for (int i=0; i < (sizeof(menuVKs)/sizeof(menuVKs[0])); i++) {

//             if (ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {

//                 noKbd = false;

//                 Menukey = menuVKs[i];

//                 // SKETCH PRE-KBD CODE REVAMP
//                 // // Search first ocurrence of letter if we're not on that letter yet
//                 // if (((Menukey >= fabgl::VK_a) && (Menukey <= fabgl::VK_Z)) || ((Menukey >= fabgl::VK_0) && (Menukey <= fabgl::VK_9))) {
//                 //     uint8_t dif;
//                 //     if (Menukey<=fabgl::VK_9) dif = 46;
//                 //         else if (Menukey<=fabgl::VK_z) dif = 75;
//                 //             else if (Menukey<=fabgl::VK_Z) dif = 17;
//                 //     uint8_t letter = rowGet(menu,focus).at(0);
//                 //     if (letter != Menukey + dif) { 
//                 //             // TO DO: Position on first ocurrence of letter
//                 //             filemenuRedraw(title);
//                 //     }
//                 /*} else*/ 
                
//                 if (Menukey == fabgl::VK_UP) {
//                     if (focus == 1 and begin_row > 1) {
//                         // filemenuScroll(DOWN);
//                         if (begin_row > 1) {
//                             last_begin_row = begin_row;
//                             begin_row--;
//                         }
//                         filemenuRedraw(title);
//                     } else if (focus > 1) {
//                         last_focus = focus;
//                         focus--;
//                         filemenuPrintRow(focus, IS_FOCUSED);
//                         if (focus + 1 < virtual_rows) {
//                             filemenuPrintRow(focus + 1, IS_NORMAL);
//                         }
//                     }
//                 } else if (Menukey == fabgl::VK_DOWN) {
//                     if (focus == virtual_rows - 1) {
//                         if ((begin_row + virtual_rows - 1) < real_rows) {
//                             last_begin_row = begin_row;
//                             begin_row++;
//                         }
//                         filemenuRedraw(title);
//                     } else if (focus < virtual_rows - 1) {
//                         last_focus = focus;
//                         focus++;
//                         filemenuPrintRow(focus, IS_FOCUSED);
//                         if (focus - 1 > 0) {
//                             filemenuPrintRow(focus - 1, IS_NORMAL);
//                         }
//                     }
//                 } else if (Menukey == fabgl::VK_PAGEUP) {
//                     if (begin_row > virtual_rows) {
//                         focus = 1;
//                         begin_row -= virtual_rows;
//                     } else {
//                         focus = 1;
//                         begin_row = 1;
//                     }
//                     filemenuRedraw(title);
//                 } else if (Menukey == fabgl::VK_PAGEDOWN) {
//                     if (real_rows - begin_row  - virtual_rows > virtual_rows) {
//                         focus = 1;
//                         begin_row += virtual_rows - 1;
//                     } else {
//                         focus = virtual_rows - 1;
//                         begin_row = real_rows - virtual_rows + 1;
//                     }
//                     filemenuRedraw(title);
//                 } else if (Menukey == fabgl::VK_HOME) {
//                     focus = 1;
//                     begin_row = 1;
//                     filemenuRedraw(title);
//                 } else if (Menukey == fabgl::VK_END) {
//                     focus = virtual_rows - 1;
//                     begin_row = real_rows - virtual_rows + 1;
//                     filemenuRedraw(title);
//                 } else if (Menukey == fabgl::VK_RETURN) {
//                     if (!menuIsSub(focus)) menu_level=0; 
//                     fclose(dirfile);
//                     filedir = rowGet(menu,focus);
//                     rtrim(filedir);

//                     while(ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {
//                         vTaskDelay(5 / portTICK_PERIOD_MS);                    
//                     }

//                     return filedir;

//                 } else if (Menukey == fabgl::VK_ESCAPE) {
//                     menu_level=0;
//                     fclose(dirfile);

//                     while(ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {
//                         vTaskDelay(5 / portTICK_PERIOD_MS);                    
//                     }

//                     return "";
//                 }

//                 // Key repeat management
//                 int kbdDelayCnt = 0;
//                 while(ESPectrum::PS2Controller.keyboard()->isVKDown(menuVKs[i])) {
//                     vTaskDelay(5 / portTICK_PERIOD_MS);                    
//                     if (kbdDelayCnt++ == kbdDelay) {
//                         kbdDelay = KBDREPEATMS;
//                         break;
//                     }
//                 }
//             }

//         }

//         if (noKbd) kbdDelay = STARTKBDREPEATMS;

//     }

// }

// Print a virtual row
void OSD::filemenuPrintRow(uint8_t virtual_row_num, uint8_t line_type) {
    
    uint8_t margin;

    string line = rowGet(menu, virtual_row_num);
    
    switch (line_type) {
    case IS_TITLE:
        VIDEO::vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
        margin = 2;
        break;
    case IS_FOCUSED:
        VIDEO::vga.setTextColor(OSD::zxColor(0, 1), OSD::zxColor(5, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    default:
        VIDEO::vga.setTextColor(OSD::zxColor(0, 1), OSD::zxColor(7, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
    }

    if (line.find(ASCII_TAB) != line.npos) {
        line = line.substr(0,line.find(ASCII_TAB)) + string(cols - margin - line.length(),' ') + line.substr(line.find(ASCII_TAB)+1);
    }

    menuAt(virtual_row_num, 0);

    VIDEO::vga.print(" ");

    if (line.length() < cols - margin) {
    VIDEO::vga.print(line.c_str());
    for (uint8_t i = line.length(); i < (cols - margin); i++)
        VIDEO::vga.print(" ");
    } else {
        VIDEO::vga.print(line.substr(0, cols - margin).c_str());
    }

    VIDEO::vga.print(" ");

}

// Redraw inside rows
void OSD::filemenuRedraw(string title) {
    if ((focus != last_focus) || (begin_row != last_begin_row)) {

        // Read bunch of rows
        fseek(dirfile, (begin_row - 1) * 32,SEEK_SET);
        menu = title + "\n";
        for (int i = 1; i < virtual_rows; i++) {
            char buf[256];
            fgets(buf, sizeof(buf), dirfile);
            if (feof(dirfile)) break;
            menu += buf;
        }

        for (uint8_t row = 1; row < virtual_rows; row++) {
            if (row == focus) {
                filemenuPrintRow(row, IS_FOCUSED);
            } else {
                filemenuPrintRow(row, IS_NORMAL);
            }
        }
        
        menuScrollBar();
        
        last_focus = focus;
        last_begin_row = begin_row;
    }
}
