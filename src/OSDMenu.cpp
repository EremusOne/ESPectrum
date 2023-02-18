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
        x = x + (((cols >> 1) - 1)* 6);
        y = y + 16;
    }

    menu_level++;

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

// Draw the complete menu
void OSD::menuDraw() {

    // Set font
    VIDEO::vga.setFont(Font6x8);
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
    menuPrintRow(1, IS_FOCUSED);
    for (uint8_t r = 2; r < virtual_rows; r++) {
        menuPrintRow(r, IS_NORMAL);
    }
    focus = 1;
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

// Run a new menu
unsigned short OSD::menuRun(string new_menu) {

    fabgl::VirtualKeyItem Menukey;    

    newMenu(new_menu);
    while (1) {
        ESPectrum::readKbd(&Menukey);
        if (!Menukey.down) continue;
        if (Menukey.vk == fabgl::VK_UP) {
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
        } else if (Menukey.vk == fabgl::VK_DOWN) {
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
        } else if (Menukey.vk == fabgl::VK_PAGEUP) {
            if (begin_row > virtual_rows) {
                focus = 1;
                begin_row -= virtual_rows;
            } else {
                focus = 1;
                begin_row = 1;
            }
            menuRedraw();
        } else if (Menukey.vk == fabgl::VK_PAGEDOWN) {
            if (real_rows - begin_row  - virtual_rows > virtual_rows) {
                focus = 1;
                begin_row += virtual_rows - 1;
            } else {
                focus = virtual_rows - 1;
                begin_row = real_rows - virtual_rows + 1;
            }
            menuRedraw();
        } else if (Menukey.vk == fabgl::VK_HOME) {
            focus = 1;
            begin_row = 1;
            menuRedraw();
        } else if (Menukey.vk == fabgl::VK_END) {
            focus = virtual_rows - 1;
            begin_row = real_rows - virtual_rows + 1;
            menuRedraw();
        } else if (Menukey.vk == fabgl::VK_RETURN) {
            if (!menuIsSub(focus)) menu_level=0; 
            return menuRealRowFor(focus);
        } else if ((Menukey.vk == fabgl::VK_ESCAPE) || (Menukey.vk == fabgl::VK_F1)) {
            menu_level=0;
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
        deallocAluBytes();
        OSD::osdCenteredMsg("Please wait: sorting directory", LEVEL_INFO);
        int chunks = FileUtils::DirToFile(filedir, extensions); // Prepare sna filelist
        if (chunks) FileUtils::Mergefiles(filedir,chunks); // Merge files
        OSD::osdCenteredMsg(" Done: directory index ready  ", LEVEL_INFO);
        precalcAluBytes();
        rc = stat((filedir + "/.d").c_str(), &stat_buf);
    }
    
    // Open dir file for read
    dirfile = fopen((filedir + "/.d").c_str(), "r");
    if (dirfile == NULL) {
        printf("Error opening dir file\n");
        menu_level = 0;
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
        // x = (Config::aspect_16_9 ? 59 : 39);
        // y = 40;
        x = x + (((cols >> 1) - 3)* 6);
        y = y + 16;
    }

    // Columns
    cols = 31; // 28 for filename + 2 pre and post space + 1 for scrollbar

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

    menuDraw();

    while (1) {
        ESPectrum::readKbd(&Menukey);
        if (!Menukey.down) continue;
        if (Menukey.vk == fabgl::VK_UP) {
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
        } else if (Menukey.vk == fabgl::VK_PAGEUP) {
            if (begin_row > virtual_rows) {
                focus = 1;
                begin_row -= virtual_rows;
            } else {
                focus = 1;
                begin_row = 1;
            }
            filemenuRedraw(title);
        } else if (Menukey.vk == fabgl::VK_PAGEDOWN) {
            if (real_rows - begin_row  - virtual_rows > virtual_rows) {
                focus = 1;
                begin_row += virtual_rows - 1;
            } else {
                focus = virtual_rows - 1;
                begin_row = real_rows - virtual_rows + 1;
            }
            filemenuRedraw(title);
        } else if (Menukey.vk == fabgl::VK_HOME) {
            focus = 1;
            begin_row = 1;
            filemenuRedraw(title);
        } else if (Menukey.vk == fabgl::VK_END) {
            focus = virtual_rows - 1;
            begin_row = real_rows - virtual_rows + 1;
            filemenuRedraw(title);
        } else if (Menukey.vk == fabgl::VK_RETURN) {
            if (!menuIsSub(focus)) menu_level=0; 
            fclose(dirfile);
            filedir = rowGet(menu,focus);
            rtrim(filedir);
            return filedir;
        } else if (Menukey.vk == fabgl::VK_ESCAPE) {
            menu_level=0;
            fclose(dirfile);
            return "";
        }
    }

}

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
