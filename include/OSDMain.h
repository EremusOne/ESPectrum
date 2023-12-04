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

#define DLG_CANCEL 0
#define DLG_YES 1
#define DLG_NO 2

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

    // OSD
    static void osdHome();
    static void osdAt(uint8_t row, uint8_t col);
    static void drawOSD(bool bottom_info);
    static void drawStats();
    static void do_OSD(fabgl::VirtualKey KeytoESP, uint8_t CTRL);
    static void HWInfo();

    // Error
    static void errorPanel(string errormsg);
    static void errorHalt(string errormsg);
    static void osdCenteredMsg(string msg, uint8_t warn_level);
    static void osdCenteredMsg(string msg, uint8_t warn_level, uint16_t millispause);

    // Menu
    static unsigned short menuRealRowFor(uint8_t virtual_row_num);
    static bool menuIsSub(uint8_t virtual_row_num);
    static void menuPrintRow(uint8_t virtual_row_num, uint8_t line_type);
    static void menuRedraw();
    static void WindowDraw();
    static unsigned short menuRun(string new_menu);
    static string fileDialog(string &fdir, string title, uint8_t ftype, uint8_t mfcols, uint8_t mfrows);
    static int menuTape(string title);    
    static void menuScroll(bool up);
    static void fd_Redraw(string title, string fdir, uint8_t ftype);
    static void fd_PrintRow(uint8_t virtual_row_num, uint8_t line_type);
    static void tapemenuRedraw(string title);
    static void PrintRow(uint8_t virtual_row_num, uint8_t line_type);
    static void menuAt(short int row, short int col);
    static void menuScrollBar(unsigned short br);
    static void click();
    static void ZXKbdRead();
    static uint8_t menu_level;
    static bool menu_saverect;    
    static unsigned short menu_curopt;    
    static unsigned int SaveRectpos;    

    static uint8_t msgDialog(string title, string msg);

    static void progressDialog(string title, string msg, int percent, int action);

    // Rows
    static unsigned short rowCount(string menu);
    static string rowGet(string menu, unsigned short row_number);

    static void esp_hard_reset();

    static esp_err_t updateFirmware(FILE *firmware);

    static char stats_lin1[25]; // "CPU: 00000 / IDL: 00000 ";
    static char stats_lin2[25]; // "FPS:000.00 / FND:000.00 ";

};

#endif // ESPECTRUM_OSD_H
