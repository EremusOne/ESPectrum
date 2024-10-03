/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023, 2024 Víctor Iborra [Eremus] and 2023 David Crespo [dcrespo3d]
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
#include "cpuESP.h"
#include "Video.h"
#include "messages.h"
#include "OSDMain.h"
#include <math.h>
#include "ZXKeyb.h"
#include "pwm_audio.h"
#include "Z80_JLS/z80.h"
#include "Tape.h"

#define MENU_MAX_ROWS 17

// Scroll
#define UP true
#define DOWN false

extern Font Font6x8;

int OSD::prepare_checkbox_menu(string &menu, string curopt) {

    int mpos = -1;
    int rpos;
    int itempos = 0;
    int m_curopt = 0;
    while(1) {
        mpos = menu.find("[",mpos + 1);
        if (mpos == string::npos) break;
        rpos = menu.find("]",mpos + 1) - mpos - 1;
        itempos++;
        string rmenu = menu.substr(mpos + 1, rpos );
        trim(rmenu);
        if (rmenu == curopt) {
            menu.replace(mpos + 1, rpos,"*");
            m_curopt = itempos;
        } else
            menu.replace(mpos + 1, rpos," ");
    }

    return m_curopt;

}

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

void OSD::menuPrintRow(uint8_t virtual_row_num, uint8_t line_type) {
    PrintRow(virtual_row_num, line_type, true);
}

// Draw the complete menu
void OSD::WindowDraw() {

    // Set font
    VIDEO::vga.setFont(Font6x8);

    if (menu_level == 0) SaveRectpos = 0;

    OSD::saveBackbufferData();

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

    // CRT Overscan compensation
    if (Config::videomode == 2) {
        x = 18;
        if (menu_level == 0) {
            if (Config::arch[0] == 'T' && Config::ALUTK == 2) {
                y = 4;
            } else {
                y = 12;
            }
        }
    } else {
        x = 0;
        if (menu_level == 0) y = 0;
    }

    // Position
    if (menu_level == 0) {
        x += (Config::aspect_16_9 ? 24 : 8);
        y += 8;
        prev_y[0] = 0;
    } else {
        x += (Config::aspect_16_9 ? 24 : 8) + (54 /*60*/ * menu_level);
        if (menu_saverect && !prev_y[menu_level]) {
            y += (4 + (8 * menu_prevopt));
            prev_y[menu_level] = y;
        } else {
            y = prev_y[menu_level];
        }
    }

    for ( int i = menu_level + 1; i < 5; i++ ) prev_y[i] = 0;

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
    // printf("Cols previo: %d\n",cols);
    cols += 8;
    cols = (cols > 32 ? 32 : cols);    
    // printf("Cols final: %d\n",cols);

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

    int rmax = scrW == 320 ? 52 : 55;
    if ( x + cols * OSD_FONT_W > rmax * OSD_FONT_W ) x = ( rmax - cols ) * OSD_FONT_W;

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
                    if (real_rows - begin_row - virtual_rows > virtual_rows) {
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
                } else if (Menukey.vk == fabgl::VK_RETURN /*|| Menukey.vk == fabgl::VK_SPACE*/ || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY2C) {
                    click();
                    menu_prevopt = menuRealRowFor(focus);
                    return menu_prevopt;
                } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_F1 || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {
                    if (menu_level!=0) OSD::restoreBackbufferData(true);
                    click();
                    return 0;
                }
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

}

// Run a new menu
unsigned short OSD::simpleMenuRun(string new_menu, uint16_t posx, uint16_t posy, uint8_t max_rows, uint8_t max_cols) {

    fabgl::VirtualKeyItem Menukey;

    menu = new_menu;

    x = posx;
    y = posy;

    // Rows
    real_rows = rowCount(menu);
    virtual_rows = real_rows > max_rows ? max_rows : real_rows;

    // Columns
    cols = max_cols;

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = (virtual_rows * OSD_FONT_H) + 2;

    // Set font
    VIDEO::vga.setFont(Font6x8);

    if (menu_saverect && menu_level == 0) SaveRectpos = 0;

    OSD::saveBackbufferData();

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
                    if (real_rows - begin_row - virtual_rows > virtual_rows) {
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
                } else if (Menukey.vk == fabgl::VK_RETURN /*|| Menukey.vk == fabgl::VK_SPACE*/ || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY2C) {
                    OSD::restoreBackbufferData(true);
                    click();
                    menu_prevopt = menuRealRowFor(focus);
                    return menu_prevopt;
                } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_F1 || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {
                    OSD::restoreBackbufferData(true);
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
void OSD::PrintRow(uint8_t virtual_row_num, uint8_t line_type, bool is_menu) {

    uint8_t margin;

    string line = rowGet(menu, is_menu ? menuRealRowFor(virtual_row_num) : virtual_row_num);

    switch (line_type) {
    case IS_TITLE:
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
        margin = 2;
        break;
    case IS_FOCUSED:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    case IS_SELECTED:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(6, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    case IS_SELECTED_FOCUSED:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(4, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    default:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
    }

    if (line.find(ASCII_TAB) != line.npos) {
        line = line.substr(0,line.find(ASCII_TAB)) + string(cols - margin - line.length(),' ') + line.substr(line.find(ASCII_TAB) + 1);
    }

    menuAt(virtual_row_num, 0);

    VIDEO::vga.print(" ");

    if ((!is_menu || virtual_row_num == 0) && line.substr(0,9) == "ESPectrum") {
        VIDEO::vga.setTextColor(zxColor(16,0), zxColor(0, 0));
        VIDEO::vga.print("ESP");
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
        VIDEO::vga.print(("ectrum " + Config::arch).c_str());
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

// Redraw inside rows
void OSD::tapemenuRedraw(string title, bool force) {

    if ( force || focus != last_focus || begin_row != last_begin_row ) {
        // Read bunch of rows
        menu = title + "\n";
        if ( Tape::tapeNumBlocks ) {
            for (int i = begin_row - 1; i < virtual_rows - ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly ? 1 : 0 ) + begin_row - 2; i++) {
                if (i >= Tape::tapeNumBlocks) break;
                if (Tape::tapeFileType == TAPE_FTYPE_TAP)
                    menu += Tape::tapeBlockReadData(i);
                else
                    menu += Tape::tzxBlockReadData(i);
            }
            if ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && begin_row - 1 + virtual_rows >= real_rows ) menu += "\n";
        } else {
            menu += ( Config::lang ? "<Vacio>\n" : "<Empty>\n" );
        }

        for (uint8_t row = 1; row < virtual_rows - ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly ? 1 : 0 ); row++) {
            if (row == focus) {
                PrintRow(row, ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Tape::isSelectedBlock(begin_row - 2 + row) ) ? IS_SELECTED_FOCUSED : IS_FOCUSED);
            } else {
                PrintRow(row, ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Tape::isSelectedBlock(begin_row - 2 + row) ) ? IS_SELECTED : IS_NORMAL);
            }
        }

        if ( Tape::tapeFileType == TAPE_FTYPE_TAP ) {

            string options;
            if ( !Tape::tapeIsReadOnly ) {
                options = Config::lang == 0 ? " SPC Select | F2 Rename | F6 Move | F8 Delete" :
                          Config::lang == 1 ? " ESP Selec. | F2 Renombrar | F6 Mover | F8 Borrar" :
                                              " ESP Selec. | F2 Renomear | F6 Mover | F8 Excluir";
            } else {
                options = Config::lang == 0 ? " [Read-Only TAP]" :
                          Config::lang == 1 ? " [TAP de solo lectura]" :
                                              " [TAP somente leitura]";
            }

            menuAt(-1, 0);
            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
            VIDEO::vga.print((options + std::string(cols - options.size(), ' ')).c_str());
        }

        menuScrollBar(begin_row);

        last_focus = focus;
        last_begin_row = begin_row;

    }
}

// Tape Browser Menu
int OSD::menuTape(string title) {

    if ( !Tape::tape ) return -1;

    fabgl::VirtualKeyItem Menukey;

    uint32_t tapeBckPos = ftell(Tape::tape);

    // Tape::TapeListing.erase(Tape::TapeListing.begin(),Tape::TapeListing.begin() + 2);

    Tape::selectedBlocks.clear();

    real_rows = Tape::tapeNumBlocks + 1;
    virtual_rows = (real_rows > 19 ? 19 : real_rows) + ( Tape::tapeFileType == TAPE_FTYPE_TAP ? 1 : 0 );
    // begin_row = last_begin_row = last_focus = focus = 1;

    if ( !Tape::tapeNumBlocks ) virtual_rows++;

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
//    if (menu_level == 0) {

    // CRT Overscan compensation
    if (Config::videomode == 2) {
        x = 18;
        if (Config::arch[0] == 'T' && Config::ALUTK == 2) {
            y = 4;
        } else {
            y = 12;
        }
    } else {
        x = 0;
        y = 0;
    }

    x += (Config::aspect_16_9 ? 24 : 8);
    y += 8;

//    } else {
//        x = (Config::aspect_16_9 ? 24 : 8) + (60 * menu_level);
//        y = 8 + (16 * menu_level);
//    }

    // Columns
//    cols = 39; // 36 for block info + 2 pre and post space + 1 for scrollbar
    cols = 50; // 47 for block info + 2 pre and post space + 1 for scrollbar

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
                        PrintRow(focus, ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Tape::isSelectedBlock(begin_row - 2 + focus) ) ? IS_SELECTED_FOCUSED : IS_FOCUSED);
                        PrintRow(focus + 1, ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Tape::isSelectedBlock(begin_row - 2 + focus + 1) ) ? IS_SELECTED : IS_NORMAL);
                        click();
                    }
                } else if (Menukey.vk == fabgl::VK_DOWN || Menukey.vk == fabgl::VK_JOY1DOWN || Menukey.vk == fabgl::VK_JOY2DOWN) {
                    if (focus == virtual_rows - 1 - ( Tape::tapeFileType == TAPE_FTYPE_TAP ? 1 : 0 ) ) {
                        if ((begin_row + virtual_rows - 1 - ( Tape::tapeFileType == TAPE_FTYPE_TAP ? (!Tape::tapeIsReadOnly ? 2 : 1) : 0 )) < real_rows) {
                            last_begin_row = begin_row;
                            begin_row++;
                            tapemenuRedraw(title);
                            click();
                        }
                    } else if (focus < virtual_rows - 1 - ( Tape::tapeFileType == TAPE_FTYPE_TAP ? 1 : 0 )) {
                        last_focus = focus;
                        focus++;
                        PrintRow(focus, ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Tape::isSelectedBlock(begin_row - 2 + focus) ) ? IS_SELECTED_FOCUSED : IS_FOCUSED);
                        PrintRow(focus - 1, ( Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Tape::isSelectedBlock(begin_row - 2 + focus - 1) ) ? IS_SELECTED : IS_NORMAL);
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
                    if (real_rows - begin_row - virtual_rows > virtual_rows) {
                        last_focus = focus;
                        last_begin_row = begin_row;
                        focus = 1;
                        begin_row += virtual_rows - 1 - ( Tape::tapeFileType == TAPE_FTYPE_TAP ? 1 : 0 );
                        tapemenuRedraw(title);
                        click();
                    } else {
                        last_focus = focus;
                        last_begin_row = begin_row;
                        focus = virtual_rows - 1 - ( Tape::tapeFileType == TAPE_FTYPE_TAP ? 1 : 0 );
                        begin_row = real_rows - virtual_rows + 1 + ( Tape::tapeFileType == TAPE_FTYPE_TAP ? (!Tape::tapeIsReadOnly ? 2 : 1) : 0 );
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
                    focus = virtual_rows - 1 - ( Tape::tapeFileType == TAPE_FTYPE_TAP ? 1 : 0 );
                    begin_row = real_rows - virtual_rows + 1 + ( Tape::tapeFileType == TAPE_FTYPE_TAP ? (!Tape::tapeIsReadOnly ? 2 : 1) : 0 );
                    tapemenuRedraw(title);
                    click();
                } else if (Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && (Menukey.vk == fabgl::VK_SPACE || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C)) {
                    if ( begin_row - 1 + focus < real_rows ) Tape::selectBlockToggle(begin_row - 2 + focus);

                    if (focus == virtual_rows - 1 - 1 ) {
                        if ((begin_row + virtual_rows - 1 - 2) < real_rows) {
                            last_begin_row = begin_row;
                            begin_row++;
                            tapemenuRedraw(title);
                            click();
                        } else {
                            PrintRow(focus, Tape::isSelectedBlock(begin_row - 2 + focus) ? IS_SELECTED_FOCUSED : IS_FOCUSED);
                        }
                    } else if (focus < virtual_rows - 1 - 1) {
                        last_focus = focus;
                        focus++;
                        PrintRow(focus, Tape::isSelectedBlock(begin_row - 2 + focus) ? IS_SELECTED_FOCUSED : IS_FOCUSED);
                        PrintRow(focus - 1, Tape::isSelectedBlock(begin_row - 2 + focus - 1) ? IS_SELECTED : IS_NORMAL);
                        click();
                    }
                } else if (Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Menukey.vk == fabgl::VK_F2 && begin_row - 1 + focus < real_rows) {

                    long current_pos = ftell( Tape::tape );

                    int blocknum = begin_row - 2 + focus;
                    if (Tape::getBlockType(blocknum) <= TapeBlock::Code_header) {
                        string new_name = input(21, focus, "", Tape::getBlockName(blocknum), 10, 10, zxColor(0,0), zxColor(7,0), false);
                        if ( new_name != "" )
                            Tape::renameBlock(blocknum, new_name);
                        tapemenuRedraw(title, true);
                    } else
                        osdCenteredMsg(OSD_BLOCK_TYPE_ERR[Config::lang], LEVEL_WARN, 1000);
                            
                    fseek( Tape::tape, current_pos, SEEK_SET );

                } else if (Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Menukey.vk == fabgl::VK_F6) {
                    click();
                    if ( Tape::selectedBlocks.empty() ) {
                        osdCenteredMsg(OSD_BLOCK_SELECT_ERR[Config::lang], LEVEL_WARN, 1000);
                    } else {
                        Tape::moveSelectedBlocks(begin_row - 2 + focus);
                        tapemenuRedraw(title, true);
                    }
                } else if (Tape::tapeFileType == TAPE_FTYPE_TAP && !Tape::tapeIsReadOnly && Menukey.vk == fabgl::VK_F8) {
                    click();

                    if ( Tape::selectedBlocks.empty() && begin_row - 1 + focus == real_rows ) {
                        osdCenteredMsg(OSD_BLOCK_SELECT_ERR[Config::lang], LEVEL_WARN, 1000);
                    } else {
                        string title = Tape::selectedBlocks.empty() ? MENU_DELETE_CURRENT_TAP_BLOCK[Config::lang] : MENU_DELETE_TAP_BLOCKS[Config::lang];
                        string msg = OSD_DLG_SURE[Config::lang];
                        uint8_t res = msgDialog(title,msg);

                        if (res == DLG_YES) {
                            if ( Tape::selectedBlocks.empty() ) Tape::selectBlockToggle(begin_row - 2 + focus);
                            Tape::removeSelectedBlocks();
                            menu_saverect = true;
                            return -2;
                        }
                    }
                    
                } else if ( Menukey.vk == fabgl::VK_RETURN /*|| Menukey.vk == fabgl::VK_SPACE*/ || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B) {
                    click();
                    if (Tape::tapeFileType == TAPE_FTYPE_TAP) {
                        if ( begin_row - 1 + focus < real_rows ) {
                            Tape::CalcTapBlockPos(begin_row + focus - 2);
                            // printf("Ret value: %d\n", begin_row + focus - 2);
                            return (begin_row + focus - 2);
                        }
                    } else {
                        Tape::CalcTZXBlockPos(begin_row + focus - 2);
                        // printf("Ret value: %d\n", begin_row + focus - 2);
                        return (begin_row + focus - 2);
                    }
                } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {

                    // if (Tape::tapeStatus==TAPE_LOADING) {
                        fseek(Tape::tape, tapeBckPos, SEEK_SET);
                    // }

                    if (menu_level!=0) OSD::restoreBackbufferData(true);
                    click();
                    return -1;
                }
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

