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

#ifndef ESPECTRUM_OSD_H
#define ESPECTRUM_OSD_H

#include "fabgl.h"
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

// Defines

// Line type
#define IS_TITLE 0
#define IS_FOCUSED 1
#define IS_NORMAL 2
#define IS_INFO 3
#define IS_SELECTED 4
#define IS_SELECTED_FOCUSED 5

#define OSD_FONT_W 6
#define OSD_FONT_H 8

#define LEVEL_INFO 0
#define LEVEL_OK 1
#define LEVEL_WARN 2
#define LEVEL_ERROR 3

#define DLG_CANCEL 0
#define DLG_YES 1
#define DLG_NO 2

// File dialog

#define MAXSEARCHLEN 16

// OSD Interface
class OSD {

public:

    // Screen size to be set at initialization
    static unsigned short scrW;
    static unsigned short scrH;

    // Calc
    static unsigned short scrAlignCenterX(unsigned short pixel_width);
    static unsigned short scrAlignCenterY(unsigned short pixel_height);
    static uint8_t osdMaxRows();
    static uint8_t osdMaxCols();
    static unsigned short osdInsideX();
    static unsigned short osdInsideY();

    // OSD
    static void osdHome();
    static void osdAt(uint8_t row, uint8_t col);
    static void drawOSD(bool bottom_info);
    static void drawWindow(uint16_t width, uint16_t height,string top, string bottom, bool clear);
    static void drawKbdLayout(uint8_t layout);
    static void drawStats();
    static int  prepare_checkbox_menu(string &menu, string curopt);
    static void pref_rom_menu();
    static void do_OSD(fabgl::VirtualKey KeytoESP, bool CTRL, bool SHIFT);
    static void HWInfo();
    // static void UART_test();

    // Error
    static void errorPanel(string errormsg);
    static void errorHalt(string errormsg);
    static void osdCenteredMsg(string msg, uint8_t warn_level);
    static void osdCenteredMsg(string msg, uint8_t warn_level, uint16_t millispause);

    static void restoreBackbufferData(bool force = false);
    static void saveBackbufferData(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool force = false);
    static void saveBackbufferData(bool force = false);

    // Menu
    static unsigned short menuRealRowFor(uint8_t virtual_row_num);
    // static bool menuIsSub(uint8_t virtual_row_num);
    static void menuPrintRow(uint8_t virtual_row_num, uint8_t line_type);
    static void menuRedraw();
    static void WindowDraw();
    static unsigned short menuRun(string new_menu);
    static unsigned short simpleMenuRun(string new_menu, uint16_t posx, uint16_t posy, uint8_t max_rows, uint8_t max_cols);
    static string fileDialog(string &fdir, string title, uint8_t ftype, uint8_t mfcols, uint8_t mfrows);
    static int menuTape(string title);    
    static void menuScroll(bool up);
    static void fd_Redraw(string title, string fdir, uint8_t ftype);
    static void fd_PrintRow(uint8_t virtual_row_num, uint8_t line_type);
    static void tapemenuRedraw(string title, bool force = true);
    static void PrintRow(uint8_t virtual_row_num, uint8_t line_type, bool is_menu = false);
    static void menuAt(short int row, short int col);
    static void menuScrollBar(unsigned short br);
    static void click();
    static uint8_t menu_level;
    static bool menu_saverect;    
    static unsigned short menu_curopt;    
    static unsigned int SaveRectpos;    

    static int8_t fdScrollPos;
    static int timeStartScroll;
    static int timeScroll;
    static unsigned int elements;
    static unsigned int ndirs;

    static uint8_t msgDialog(string title, string msg);
    static void progressDialog(string title, string msg, int percent, int action);
    static void joyDialog(uint8_t joynum);
    static void pokeDialog();

    static string input(int x, int y, string inputLabel, string text, int inputSize, int maxSize, uint16_t ink_color, uint16_t paper_color, bool fat32mode);

    // Rows
    static unsigned short rowCount(string menu);
    static string rowGet(string menu, unsigned short row_number);

    static void esp_hard_reset();

    static esp_err_t updateFirmware(FILE *firmware);
    static esp_err_t updateROM(FILE *rombinary, uint8_t arch);

    static char stats_lin1[25]; // "CPU: 00000 / IDL: 00000 ";
    static char stats_lin2[25]; // "FPS:000.00 / FND:000.00 ";

    static uint8_t cols;                     // Maximum columns
    static uint8_t mf_rows;                  // File menu maximum rows
    static unsigned short real_rows;      // Real row count
    static uint8_t virtual_rows;             // Virtual maximum rows on screen
    static uint16_t w;                        // Width in pixels
    static uint16_t h;                        // Height in pixels
    static uint16_t x;                        // X vertical position
    static uint16_t y;                        // Y horizontal position
    static uint16_t prev_y[5];                // Y prev. position
    static unsigned short menu_prevopt;
    static string menu;                   // Menu string
    static unsigned short begin_row;      // First real displayed row
    static uint8_t focus;                    // Focused virtual row
    static uint8_t last_focus;               // To check for changes
    static unsigned short last_begin_row; // To check for changes

    static uint8_t fdCursorFlash;    
    static bool fdSearchRefresh;    
    static unsigned int fdSearchElements;   

};

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}

#endif // ESPECTRUM_OSD_H
