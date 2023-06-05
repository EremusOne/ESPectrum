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

#ifndef ESPECTRUM_OSD_H
#define ESPECTRUM_OSD_H

#include "fabgl.h"
#include <string>

using namespace std;

// Defines
#define OSD_FONT_W 6
#define OSD_FONT_H 8

#define LEVEL_INFO 0
#define LEVEL_OK 1
#define LEVEL_WARN 2
#define LEVEL_ERROR 3

// OSD Interface
class OSD
{
public:
    // ZX Color
    static uint16_t zxColor(uint8_t color, uint8_t bright);

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

    // // OSD
    static void osdHome();
    static void osdAt(uint8_t row, uint8_t col);
    static void drawOSD(bool bottom_info);
    static void drawStats(char *line1, char *line2);    
    static void do_OSD(fabgl::VirtualKey KeytoESP);

    // // Error
    static void errorPanel(string errormsg);
    static void errorHalt(string errormsg);
    static void osdCenteredMsg(string msg, uint8_t warn_level);
    static void osdCenteredMsg(string msg, uint8_t warn_level, uint16_t millispause);

    // // Menu
    static void newMenu(string new_menu);
    static void menuRecalc();
    static unsigned short menuRealRowFor(uint8_t virtual_row_num);
    static bool menuIsSub(uint8_t virtual_row_num);
    static void menuPrintRow(uint8_t virtual_row_num, uint8_t line_type);
    static void menuDraw();
    static void menuRedraw();
    static string getArchMenu();
    static string getRomsetMenu(string arch);
    static unsigned short menuRun(string new_menu);
    static string menuFile(string new_menu, string title, string extensions);
    static void menuScroll(bool up);
    static void filemenuRedraw(string title);
    static void filemenuPrintRow(uint8_t virtual_row_num, uint8_t line_type);
    static void menuAt(short int row, short int col);
    static void menuScrollBar();
    static void click();
    static uint8_t menu_level;
    static bool menu_saverect;    
    static unsigned short menu_curopt;    
    static unsigned int SaveRectpos;    

    // // Rows
    static unsigned short rowCount(string menu);
    static string rowGet(string menu, unsigned short row_number);

    // // Snapshot (SNA/Z80) Management
    static void changeSnapshot(string sna_filename);

    static void esp_hard_reset();

};

#endif // ESPECTRUM_OSD_H
