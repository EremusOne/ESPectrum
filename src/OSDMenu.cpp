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
#include <string>

using namespace std;

#include "FileUtils.h"
// #include "PS2Kbd.h"
#include "ESPectrum.h"
#include "CPU.h"
#include "messages.h"
#include "OSDMain.h"
// #include "Wiimote2Keys.h"
#include <math.h>

#define MENU_MAX_ROWS 23
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
    // Columns
    cols = 24;
    uint8_t col_count = 0;
    for (unsigned short i = 0; i < menu.length(); i++) {
        if (menu.at(i) == ASCII_NL) {
            if (col_count > cols) {
                cols = col_count;
            }
            col_count = 0;
        }
        col_count++;
    }
    cols = (cols > osdMaxCols() ? osdMaxCols() : cols);

    // Rows
    real_rows = rowCount(menu);
    virtual_rows = (real_rows > MENU_MAX_ROWS ? MENU_MAX_ROWS : real_rows);
    begin_row = last_begin_row = last_focus = focus = 1;

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

    // Position
    x = scrAlignCenterX(w);
    y = scrAlignCenterY(h);
}

// Get real row number for a virtual one
unsigned short OSD::menuRealRowFor(uint8_t virtual_row_num) { return begin_row + virtual_row_num - 1; }

// Menu relative AT
void OSD::menuAt(short int row, short int col) {
    if (col < 0)
        col = cols - 2 - col;
    if (row < 0)
        row = virtual_rows - 2 - row;
    CPU::vga.setCursor(x + 1 + (col * OSD_FONT_W), y + 1 + (row * OSD_FONT_H));
}

// Print a virtual row
void OSD::menuPrintRow(uint8_t virtual_row_num, uint8_t line_type) {
    VGA6Bit& vga = CPU::vga;
    uint8_t margin;
    string line = rowGet(menu, menuRealRowFor(virtual_row_num));
    switch (line_type) {
    case IS_TITLE:
        vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
        margin = 2;
        break;
    case IS_FOCUSED:
        vga.setTextColor(OSD::zxColor(0, 1), OSD::zxColor(5, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    default:
        vga.setTextColor(OSD::zxColor(0, 1), OSD::zxColor(7, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
    }

    menuAt(virtual_row_num, 0);
    vga.print(" ");
    if (line.length() < cols - margin) {
        vga.print(line.c_str());
        for (uint8_t i = line.length(); i < (cols - margin); i++)
            vga.print(" ");
    } else {
        // vga.print(line.substring(0, cols - margin).c_str());
    }
    vga.print(" ");
}

// Draw the complete menu
void OSD::menuDraw() {
    VGA6Bit& vga = CPU::vga;
    // Set font
    vga.setFont(Font6x8);
    // Menu border
    vga.rect(x, y, w, h, OSD::zxColor(0, 0));
    // Title
    menuPrintRow(0, IS_TITLE);
    // Rainbow
    unsigned short rb_y = y + 8;
    unsigned short rb_paint_x = x + w - 30;
    uint8_t rb_colors[] = {2, 6, 4, 5};
    for (uint8_t c = 0; c < 4; c++) {
        for (uint8_t i = 0; i < 5; i++) {
            vga.line(rb_paint_x + i, rb_y, rb_paint_x + 8 + i, rb_y - 8, OSD::zxColor(rb_colors[c], 1));
        }
        rb_paint_x += 5;
    }
    // Focused first line
    menuPrintRow(1, IS_FOCUSED);
    for (uint8_t r = 2; r < virtual_rows; r++) {
        menuPrintRow(r, IS_NORMAL);
    }
    focus = 1;
    menuScrollBar();
}

// string OSD::getArchMenu() {
//     string menu = (string)MENU_ARCH + FileUtils::getFileEntriesFromDir(DISK_ROM_DIR);
//     return menu;
// }

// string OSD::getRomsetMenu(string arch) {
//     string menu = (string)MENU_ROMSET + FileUtils::getFileEntriesFromDir((string)DISK_ROM_DIR + "/" + arch);
//     return menu;
// }

// Run a new menu
unsigned short OSD::menuRun(string new_menu) {

    auto kbd = ESPectrum::PS2Controller.keyboard();
    bool Kdown;
    fabgl::VirtualKey MenuKey;

    newMenu(new_menu);
    while (1) {
        MenuKey = kbd->getNextVirtualKey(&Kdown);
        if (!Kdown) continue;
        // updateWiimote2KeysOSD();
        if (MenuKey == fabgl::VK_UP) {
            if (focus == 1 and begin_row > 1) {
                menuScroll(DOWN);
            } else if (focus > 1) {
                last_focus = focus;
                focus--;
                menuPrintRow(focus, IS_FOCUSED);
                if (focus + 1 < virtual_rows) {
                    menuPrintRow(focus + 1, IS_NORMAL);
                }
            }
        } else if (MenuKey == fabgl::VK_DOWN) {
            if (focus == virtual_rows - 1) {
                menuScroll(UP);
            } else if (focus < virtual_rows - 1) {
                last_focus = focus;
                focus++;
                menuPrintRow(focus, IS_FOCUSED);
                if (focus - 1 > 0) {
                    menuPrintRow(focus - 1, IS_NORMAL);
                }
            }
        } else if (MenuKey == fabgl::VK_PAGEUP) {
            if (begin_row > virtual_rows) {
                focus = 1;
                begin_row -= virtual_rows;
            } else {
                focus = 1;
                begin_row = 1;
            }
            menuRedraw();
        } else if (MenuKey == fabgl::VK_PAGEDOWN) {
            if (real_rows - begin_row  - virtual_rows > virtual_rows) {
                focus = 1;
                begin_row += virtual_rows - 1;
            } else {
                focus = virtual_rows - 1;
                begin_row = real_rows - virtual_rows + 1;
            }
            menuRedraw();
        } else if (MenuKey == fabgl::VK_HOME) {
            focus = 1;
            begin_row = 1;
            menuRedraw();
        } else if (MenuKey == fabgl::VK_END) {
            focus = virtual_rows - 1;
            begin_row = real_rows - virtual_rows + 1;
            menuRedraw();
        } else if (MenuKey == fabgl::VK_RETURN) {
            return menuRealRowFor(focus);
        } else if ((MenuKey == fabgl::VK_ESCAPE) || (MenuKey == fabgl::VK_F1)) {
            return 0;
        }
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
        menuScrollBar();
        last_focus = focus;
        last_begin_row = begin_row;
    }
}

// Draw menu scroll bar
void OSD::menuScrollBar() {
    VGA6Bit& vga = CPU::vga;
    if (real_rows > virtual_rows) {
        // Top handle
        menuAt(1, -1);
        if (begin_row > 1) {
            vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
            vga.print("+");
        } else {
            vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
            vga.print("-");
        }

        // Complete bar
        unsigned short holder_x = x + (OSD_FONT_W * (cols - 1)) + 1;
        unsigned short holder_y = y + (OSD_FONT_H * 2);
        unsigned short holder_h = OSD_FONT_H * (virtual_rows - 3);
        unsigned short holder_w = OSD_FONT_W;
        vga.fillRect(holder_x, holder_y, holder_w, holder_h + 1, OSD::zxColor(7, 0));
        holder_y++;

        // Scroll bar
        unsigned short shown_pct = round(((float)virtual_rows / (float)real_rows) * 100.0);
        unsigned short begin_pct = round(((float)(begin_row - 1) / (float)real_rows) * 100.0);
        unsigned short bar_h = round(((float)holder_h / 100.0) * (float)shown_pct);
        unsigned short bar_y = round(((float)holder_h / 100.0) * (float)begin_pct);
        while ((bar_y + bar_h) >= holder_h) {
            bar_h--;
        }

        vga.fillRect(holder_x + 1, holder_y + bar_y, holder_w - 2, bar_h, OSD::zxColor(0, 0));

        // Bottom handle
        menuAt(-1, -1);
        if ((begin_row + virtual_rows - 1) < real_rows) {
            vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
            vga.print("+");
        } else {
            vga.setTextColor(OSD::zxColor(7, 0), OSD::zxColor(0, 0));
            vga.print("-");
        }
    }
}

// Return a test menu
string OSD::getTestMenu(unsigned short n_lines) {
    string test_menu = "Test Menu\n";
    for (unsigned short line = 1; line <= n_lines; line += 2) {
        // test_menu += "Option Line " + (string)line + "\n";
        test_menu += "1........10........20........30........40........50........60\n";
    }
    return test_menu;
}
