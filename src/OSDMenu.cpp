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

#include <string>
#include <algorithm>
#include <sys/stat.h>
#include "errno.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"

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
#include "Tape.h"

#define MENU_MAX_ROWS 18

// Scroll
#define UP true
#define DOWN false

extern Font Font6x8;

// Get real row number for a virtual one
unsigned short OSD::menuRealRowFor(uint8_t virtual_row_num) { return begin_row + virtual_row_num - 1; }

// // Get real row number for a virtual one
// bool OSD::menuIsSub(uint8_t virtual_row_num) { 
//     string line = rowGet(menu, menuRealRowFor(virtual_row_num));
//     int n = line.find(ASCII_TAB);
//     if (n == line.npos) return false;
//     return (line.substr(n+1).find(">") != line.npos);
// }

// Menu relative AT
void OSD::menuAt(short int row, short int col) {
    if (col < 0)
        col = cols - 2 - col;
    if (row < 0)
        row = virtual_rows - 2 - row;
    VIDEO::vga.setCursor(x + 1 + (col * OSD_FONT_W), y + 1 + (row * OSD_FONT_H));
}

// Print a virtual row
void OSD::menuPrintRow(uint8_t virtual_row_num, uint8_t line_type) {
    
    uint8_t margin;

    string line = rowGet(menu, menuRealRowFor(virtual_row_num));
    
    switch (line_type) {
    case IS_TITLE:
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
        margin = 2;
        break;
    case IS_FOCUSED:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    default:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
    }

    if (line.find(ASCII_TAB) != line.npos) {
        line = line.substr(0,line.find(ASCII_TAB)) + string(cols - margin - line.length(),' ') + line.substr(line.find(ASCII_TAB)+1);
    }

    menuAt(virtual_row_num, 0);

    VIDEO::vga.print(" ");

    if (line.substr(0,9) == "ESPectrum") {
        VIDEO::vga.setTextColor(ESP_ORANGE, zxColor(0, 0));
        VIDEO::vga.print("ESP");        
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));        
        VIDEO::vga.print(("ectrum " + Config::getArch()).c_str());
        for (uint8_t i = line.length(); i < (cols - margin); i++)
            VIDEO::vga.print(" ");
    } else {
        if (line.length() < cols - margin) {
        VIDEO::vga.print(line.c_str());
        for (uint8_t i = line.length(); i < (cols - margin); i++)
            VIDEO::vga.print(" ");
        } else {
            VIDEO::vga.print(line.substr(0, cols - margin).c_str());
        }
    }

    VIDEO::vga.print(" ");

}

// Draw the complete menu
void OSD::WindowDraw() {

    // Set font
    VIDEO::vga.setFont(Font6x8);

    if (menu_level == 0) SaveRectpos = 0;

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
        // printf("SaveRectPos: %04X\n",SaveRectpos << 2);
    }

    // Menu border
    VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

    // Title
    PrintRow(0, IS_TITLE);

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

}

// Run a new menu
unsigned short OSD::menuRun(string new_menu) {

    fabgl::VirtualKeyItem Menukey;    

    menu = new_menu;

    // Position
    if (menu_level == 0) {
        x = (Config::aspect_16_9 ? 24 : 8);
        y = 8;
    } else {
        x = (Config::aspect_16_9 ? 24 : 8) + (60 * menu_level);
        if (menu_saverect) {
            y += (8 + (8 * menu_prevopt));
            prev_y[menu_level] = y;
        } else {
            y = prev_y[menu_level];
        }
    }

    // Rows
    real_rows = rowCount(menu);
    virtual_rows = (real_rows > MENU_MAX_ROWS ? MENU_MAX_ROWS : real_rows);
    // begin_row = last_begin_row = last_focus = focus = 1;

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
    // printf("Cols: %d\n",cols);
    cols += 8;
    cols = (cols > 28 ? 28 : cols);

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

    WindowDraw(); // Draw menu outline

    begin_row = 1;
    focus = menu_curopt;        
    last_begin_row = last_focus = 0;

    menuRedraw(); // Draw menu content

    while (1) {
        
        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

        ESPectrum::readKbdJoy();

        // Process external keyboard
        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            if (ESPectrum::readKbd(&Menukey)) {
                if (!Menukey.down) continue;
                if (Menukey.vk == fabgl::VK_UP || Menukey.vk == fabgl::VK_JOY1UP || Menukey.vk == fabgl::VK_JOY2UP) {
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
                } else if (Menukey.vk == fabgl::VK_DOWN || Menukey.vk == fabgl::VK_JOY1DOWN || Menukey.vk == fabgl::VK_JOY2DOWN) {
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
                } else if (Menukey.vk == fabgl::VK_PAGEUP || Menukey.vk == fabgl::VK_LEFT || Menukey.vk == fabgl::VK_JOY1LEFT || Menukey.vk == fabgl::VK_JOY2LEFT) {
                    if (begin_row > virtual_rows) {
                        focus = 1;
                        begin_row -= virtual_rows - 1;
                    } else {
                        focus = 1;
                        begin_row = 1;
                    }
                    menuRedraw();
                    click();
                } else if (Menukey.vk == fabgl::VK_PAGEDOWN || Menukey.vk == fabgl::VK_RIGHT || Menukey.vk == fabgl::VK_JOY1RIGHT || Menukey.vk == fabgl::VK_JOY2RIGHT) {
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
                } else if (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_SPACE || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY2C) {
                    click();
                    menu_prevopt = menuRealRowFor(focus);
                    return menu_prevopt;
                } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_F1 || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {

                    if (menu_level!=0) {
                        // Restore backbuffer data
                        int j = SaveRectpos - (((w >> 2) + 1) * h);
                        //printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        SaveRectpos = j - 4;
                        for (int  m = y; m < y + h; m++) {
                            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
                            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                                backbuffer32[n] = VIDEO::SaveRect[j];
                                j++;
                            }
                        }
                        //printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        menu_saverect = false;                        
                    }

                    click();
                    return 0;
                }
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
        
    }

}

// Run a new menu
unsigned short OSD::simpleMenuRun(string new_menu, uint16_t posx, uint16_t posy) {

    fabgl::VirtualKeyItem Menukey;    

    menu = new_menu;

    x = posx;
    y = posy;

    // Rows
    real_rows = rowCount(menu);
    virtual_rows = real_rows > 6 ? 6 : real_rows;

    // Columns
    cols = 11;

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

    // Set font
    VIDEO::vga.setFont(Font6x8);

    if (menu_saverect) {
        
        if (menu_level == 0) SaveRectpos = 0;
        
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
        // printf("SaveRectPos: %04X\n",SaveRectpos << 2);
    }

    // Menu border
    VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

    // Title
    PrintRow(0, IS_TITLE);

    begin_row = 1;
    focus = menu_curopt;        
    last_begin_row = last_focus = 0;

    menuRedraw(); // Draw menu content

    while (1) {
        
        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

        ESPectrum::readKbdJoy();

        // Process external keyboard
        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            if (ESPectrum::readKbd(&Menukey)) {
                if (!Menukey.down) continue;
                if (Menukey.vk == fabgl::VK_UP || Menukey.vk == fabgl::VK_JOY1UP || Menukey.vk == fabgl::VK_JOY2UP) {
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
                } else if (Menukey.vk == fabgl::VK_DOWN || Menukey.vk == fabgl::VK_JOY1DOWN || Menukey.vk == fabgl::VK_JOY2DOWN) {
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
                } else if (Menukey.vk == fabgl::VK_PAGEUP || Menukey.vk == fabgl::VK_LEFT || Menukey.vk == fabgl::VK_JOY1LEFT || Menukey.vk == fabgl::VK_JOY2LEFT) {
                    if (begin_row > virtual_rows) {
                        focus = 1;
                        begin_row -= virtual_rows - 1;
                    } else {
                        focus = 1;
                        begin_row = 1;
                    }
                    menuRedraw();
                    click();
                } else if (Menukey.vk == fabgl::VK_PAGEDOWN || Menukey.vk == fabgl::VK_RIGHT || Menukey.vk == fabgl::VK_JOY1RIGHT || Menukey.vk == fabgl::VK_JOY2RIGHT) {
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
                } else if (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_SPACE || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY2C) {
                    // if (menu_saverect) {
                        // Restore backbuffer data
                        int j = SaveRectpos - (((w >> 2) + 1) * h);
                        //printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        SaveRectpos = j - 4;
                        for (int  m = y; m < y + h; m++) {
                            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
                            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                                backbuffer32[n] = VIDEO::SaveRect[j];
                                j++;
                            }
                        }
                        //printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        menu_saverect = false;                        
                    // }

                    click();
                    menu_prevopt = menuRealRowFor(focus);
                    return menu_prevopt;
                } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_F1 || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {

                    // if (menu_saverect) {
                        // Restore backbuffer data
                        int j = SaveRectpos - (((w >> 2) + 1) * h);
                        //printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        SaveRectpos = j - 4;
                        for (int  m = y; m < y + h; m++) {
                            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
                            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                                backbuffer32[n] = VIDEO::SaveRect[j];
                                j++;
                            }
                        }
                        //printf("SaveRectpos: %d; J b4 restore: %d\n",SaveRectpos, j);
                        menu_saverect = false;                        
                    // }

                    click();
                    return 0;
                }
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
        
    }

}

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
        menuScrollBar(begin_row);
        last_focus = focus;
        last_begin_row = begin_row;

    }

}

// Draw menu scroll bar
void OSD::menuScrollBar(unsigned short br) {

    if (real_rows > virtual_rows) {
        // Top handle
        menuAt(1, -1);
        if (br > 1) {
            VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(0, 0));
            VIDEO::vga.print("+");
        } else {
            VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(0, 0));
            VIDEO::vga.print("-");
        }

        // Complete bar
        unsigned short holder_x = x + (OSD_FONT_W * (cols - 1)) + 1;
        unsigned short holder_y = y + (OSD_FONT_H * 2);
        unsigned short holder_h = OSD_FONT_H * (virtual_rows - 3);
        unsigned short holder_w = OSD_FONT_W;
        VIDEO::vga.fillRect(holder_x, holder_y, holder_w, holder_h + 1, zxColor(7, 0));
        holder_y++;

        // Scroll bar
        unsigned long shown_pct = round(((float)virtual_rows / (float)real_rows) * 100.0);
        unsigned long begin_pct = round(((float)(br - 1) / (float)real_rows) * 100.0);
        unsigned long bar_h = round(((float)holder_h / 100.0) * (float)shown_pct);
        unsigned long bar_y = round(((float)holder_h / 100.0) * (float)begin_pct);
        
        while ((bar_y + bar_h) >= holder_h) {
            bar_h--;
        }

        if (bar_h == 0) bar_h = 1;

        VIDEO::vga.fillRect(holder_x + 1, holder_y + bar_y, holder_w - 2, bar_h, zxColor(0, 0));

        // Bottom handle
        menuAt(-1, -1);
        if ((br + virtual_rows - 1) < real_rows) {
            VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(0, 0));
            VIDEO::vga.print("+");
        } else {
            VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(0, 0));
            VIDEO::vga.print("-");
        }
    }
}

// Print a virtual row
void OSD::PrintRow(uint8_t virtual_row_num, uint8_t line_type) {
    
    uint8_t margin;

    string line = rowGet(menu, virtual_row_num);
    
    switch (line_type) {
    case IS_TITLE:
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
        margin = 2;
        break;
    case IS_FOCUSED:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    default:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
    }

    if (line.find(ASCII_TAB) != line.npos) {
        line = line.substr(0,line.find(ASCII_TAB)) + string(cols - margin - line.length(),' ') + line.substr(line.find(ASCII_TAB)+1);
    }

    menuAt(virtual_row_num, 0);

    VIDEO::vga.print(" ");

    if ((virtual_row_num == 0) && (line.substr(0,9) == "ESPectrum")) {
        VIDEO::vga.setTextColor(ESP_ORANGE, zxColor(0, 0));
        VIDEO::vga.print("ESP");        
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));        
        VIDEO::vga.print(("ectrum " + Config::getArch()).c_str());
        for (uint8_t i = line.length(); i < (cols - margin); i++)
            VIDEO::vga.print(" ");
    } else {
        if (line.length() < cols - margin) {
        VIDEO::vga.print(line.c_str());
        for (uint8_t i = line.length(); i < (cols - margin); i++)
            VIDEO::vga.print(" ");
        } else {
            VIDEO::vga.print(line.substr(0, cols - margin).c_str());
        }
    }

    VIDEO::vga.print(" ");

}


string tapeBlockReadData(int Blocknum) {

    int tapeContentIndex=0;
    int tapeBlkLen=0;
    string blktype;
    char buf[48];
    char fname[10];

    tapeContentIndex = Tape::CalcTapBlockPos(Blocknum);

    // Analyze .tap file
    tapeBlkLen=(readByteFile(Tape::tape) | (readByteFile(Tape::tape) << 8));

    // Read the flag byte from the block.
    // If the last block is a fragmented data block, there is no flag byte, so set the flag to 255
    // to indicate a data block.
    uint8_t flagByte;
    if (tapeContentIndex + 2 < Tape::tapeFileSize) {
        flagByte = readByteFile(Tape::tape);
    } else {
        flagByte = 255;
    }

    // Process the block depending on if it is a header or a data block.
    // Block type 0 should be a header block, but it happens that headerless blocks also
    // have block type 0, so we need to check the block length as well.
    if (flagByte == 0 && tapeBlkLen == 19) { // This is a header.

        // Get the block type.
        uint8_t blocktype = readByteFile(Tape::tape);

        switch (blocktype) {
        case 0: 
            blktype = "Program      ";
            break;
        case 1: 
            blktype = "Number array ";
            break;
        case 2: 
            blktype = "Char array   ";
            break;
        case 3: 
            blktype = "Code         ";
            break;
        case 4: 
            blktype = "Data block   ";
            break;
        case 5: 
            blktype = "Info         ";
            break;
        case 6: 
            blktype = "Unassigned   ";
            break;
        default:
            blktype = "Unassigned   ";
            break;
        }

        // Get the filename.
        if (blocktype > 5) {
            fname[0] = '\0';
        } else {
            for (int i = 0; i < 10; i++) {
                fname[i] = readByteFile(Tape::tape);
            }
            fname[10]='\0';
        }

    } else {

        blktype = "Data block   ";
        fname[0]='\0';

    }

    snprintf(buf, sizeof(buf), "%04d %s %10s % 6d\n", Blocknum + 1, blktype.c_str(), fname, tapeBlkLen);

    return buf;

}

// Redraw inside rows
void OSD::tapemenuRedraw(string title) {

    if ((focus != last_focus) || (begin_row != last_begin_row)) {

        // Read bunch of rows
        menu = title + "\n";
        for (int i = begin_row - 1; i < virtual_rows + begin_row - 2; i++) {
            if (i > Tape::tapeNumBlocks) break;
            menu += tapeBlockReadData(i);
        }

        for (uint8_t row = 1; row < virtual_rows; row++) {
            if (row == focus) {
                PrintRow(row, IS_FOCUSED);
            } else {
                PrintRow(row, IS_NORMAL);
            }
        }
        
        menuScrollBar(begin_row);
        
        last_focus = focus;
        last_begin_row = begin_row;

    }
}

// Tape Browser Menu
int OSD::menuTape(string title) {

    fabgl::VirtualKeyItem Menukey;

    uint32_t tapeBckPos = ftell(Tape::tape);

    // Tape::TapeListing.erase(Tape::TapeListing.begin(),Tape::TapeListing.begin() + 2);

    real_rows = Tape::tapeNumBlocks + 1;
    virtual_rows = (real_rows > 19 ? 19 : real_rows);
    // begin_row = last_begin_row = last_focus = focus = 1;
    
    if (Tape::tapeCurBlock > 17) {
        begin_row = Tape::tapeCurBlock - 16;
        focus = 18;
    } else{
        begin_row = 1;    
        focus = Tape::tapeCurBlock + 1;
    }
    // last_focus = focus;
    // last_begin_row = begin_row;
    menu_curopt = focus;

    // // Get first bunch of rows
    // menu = title + "\n";
    // for (int i = (begin_row - 1); i < (begin_row - 1) + (virtual_rows - 1); i++) {
    //     if (i > Tape::tapeNumBlocks) break;
    //     menu += tapeBlockReadData(i);
    // }

    // printf(menu.c_str());

    // Position
    if (menu_level == 0) {
        x = (Config::aspect_16_9 ? 24 : 8);
        y = 8;
    } else {
        x = (Config::aspect_16_9 ? 24 : 8) + (60 * menu_level);
        y = 8 + (16 * menu_level);
    }

    // Columns
    cols = 39; // 36 for block info + 2 pre and post space + 1 for scrollbar

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

    menu = title + "\n";
    WindowDraw();

    last_begin_row = last_focus = 0;

    tapemenuRedraw(title);

    // zxDelay = REPDEL;
    // lastzxKey = 0;

    while (1) {

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

        ESPectrum::readKbdJoy();

        // Process external keyboard
        if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {
            if (ESPectrum::readKbd(&Menukey)) {

                if (!Menukey.down) continue;

                if (Menukey.vk == fabgl::VK_UP || Menukey.vk == fabgl::VK_JOY1UP || Menukey.vk == fabgl::VK_JOY2UP) {
                    if (focus == 1 and begin_row > 1) {
                        if (begin_row > 1) {
                            last_begin_row = begin_row;
                            begin_row--;
                        }
                        tapemenuRedraw(title);
                        click();                        
                    } else if (focus > 1) {
                        last_focus = focus;
                        focus--;
                        PrintRow(focus, IS_FOCUSED);
                        PrintRow(focus + 1, IS_NORMAL);
                        click();
                    }
                } else if (Menukey.vk == fabgl::VK_DOWN || Menukey.vk == fabgl::VK_JOY1DOWN || Menukey.vk == fabgl::VK_JOY2DOWN) {
                    if (focus == virtual_rows - 1) {
                        if ((begin_row + virtual_rows - 1) < real_rows) {
                            last_begin_row = begin_row;
                            begin_row++;
                            tapemenuRedraw(title);
                            click();
                        }
                    } else if (focus < virtual_rows - 1) {
                        last_focus = focus;
                        focus++;
                        PrintRow(focus, IS_FOCUSED);
                        PrintRow(focus - 1, IS_NORMAL);
                        click();
                    }
                } else if (Menukey.vk == fabgl::VK_PAGEUP || Menukey.vk == fabgl::VK_LEFT || Menukey.vk == fabgl::VK_JOY1LEFT || Menukey.vk == fabgl::VK_JOY2LEFT) {
                    // printf("%u\n",begin_row);
                    if (begin_row > virtual_rows) {
                        last_focus = focus;
                        last_begin_row = begin_row;                    
                        focus = 1;
                        begin_row -= virtual_rows - 1;
                        tapemenuRedraw(title);
                        click();
                    } else {
                        last_focus = focus;
                        last_begin_row = begin_row;                    
                        focus = 1;
                        begin_row = 1;
                        tapemenuRedraw(title);
                        click();
                    }
                } else if (Menukey.vk == fabgl::VK_PAGEDOWN || Menukey.vk == fabgl::VK_RIGHT || Menukey.vk == fabgl::VK_JOY1RIGHT || Menukey.vk == fabgl::VK_JOY2RIGHT) {
                    if (real_rows - begin_row  - virtual_rows > virtual_rows) {
                        last_focus = focus;
                        last_begin_row = begin_row;                    
                        focus = 1;
                        begin_row += virtual_rows - 1;
                        tapemenuRedraw(title);
                        click();
                    } else {
                        last_focus = focus;
                        last_begin_row = begin_row;                    
                        focus = virtual_rows - 1;
                        begin_row = real_rows - virtual_rows + 1;
                        tapemenuRedraw(title);
                        click();
                    }
                } else if (Menukey.vk == fabgl::VK_HOME) {
                    last_focus = focus;
                    last_begin_row = begin_row;                    
                    focus = 1;
                    begin_row = 1;
                    tapemenuRedraw(title);
                    click();
                } else if (Menukey.vk == fabgl::VK_END) {
                    last_focus = focus;
                    last_begin_row = begin_row;                    
                    focus = virtual_rows - 1;
                    begin_row = real_rows - virtual_rows + 1;
                    tapemenuRedraw(title);
                    click();
                } else if (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_SPACE || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C) {
                    click();
                    Tape::CalcTapBlockPos(begin_row + focus - 2);
                    // printf("Ret value: %d\n", begin_row + focus - 2);
                    return (begin_row + focus - 2);
                } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {

                    // if (Tape::tapeStatus==TAPE_LOADING) {
                        fseek(Tape::tape, tapeBckPos, SEEK_SET);
                    // }

                    if (menu_level!=0) {
                        // Restore backbuffer data
                        int j = SaveRectpos - (((w >> 2) + 1) * h);
                        SaveRectpos = j - 4;
                        for (int  m = y; m < y + h; m++) {
                            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.backBuffer[m]);
                            for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                                backbuffer32[n] = VIDEO::SaveRect[j];
                                j++;
                            }
                        }
                        menu_saverect = false;
                    }

                    click();
                    return -1;
                }
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);    

    }
}

