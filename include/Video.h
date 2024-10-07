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

#ifndef VIDEO_h
#define VIDEO_h

#include <inttypes.h>
#include "ESPectrum.h"
#include "ESP32Lib/VGA/VGA6Bit.h"

#define SPEC_W 256
#define SPEC_H 192

#define TSTATES_PER_LINE 224
#define TSTATES_PER_LINE_TK_50 228
#define TSTATES_PER_LINE_TK_60 228
#define TSTATES_PER_LINE_128 228
#define TSTATES_PER_LINE_PENTAGON 224

#define TS_SCREEN_48           14335  // START OF ULA DRAW PAPER 48K
#define TS_SCREEN_TK_50        14687  // START OF ULA DRAW PAPER TK 50HZ
#define TS_SCREEN_TK_60        8759   // START OF ULA DRAW PAPER TK 60HZ
#define TS_SCREEN_128          14361  // START OF ULA DRAW PAPER 128K
#define TS_SCREEN_PENTAGON     17983  // START OF ULA DRAW PAPER PENTAGON

#define TS_BORDER_320x240 8948  // START OF BORDER 48
#define TS_BORDER_320x240_TK_50  9201 // START OF BORDER TK 50HZ
#define TS_BORDER_320x240_TK_60  3273 // START OF BORDER TK 60HZ
#define TS_BORDER_320x240_128 8878  // START OF BORDER 128
#define TS_BORDER_320x240_PENTAGON 12595  // START OF BORDER PENTAGON

// // TS_BORDER_320X240 + (TSTATES_PER_LINE * 20)
// #define TS_BORDER_360x200 13428  // START OF BORDER 48
// #define TS_BORDER_360x200_TK_50 13761  // START OF BORDER TK 50HZ
// #define TS_BORDER_360x200_TK_60 7833  // START OF BORDER TK 60HZ
// #define TS_BORDER_360x200_128 13438  // START OF BORDER 128
// #define TS_BORDER_360x200_PENTAGON 17075  // START OF BORDER PENTAGON

// TS_BORDER_320X240 + (TSTATES_PER_LINE * 20)
#define TS_BORDER_360x200 13420  // START OF BORDER 48
#define TS_BORDER_360x200_TK_50 13753  // START OF BORDER TK 50HZ
#define TS_BORDER_360x200_TK_60 7825  // START OF BORDER TK 60HZ
#define TS_BORDER_360x200_128 13430  // START OF BORDER 128
#define TS_BORDER_360x200_PENTAGON 17067  // START OF BORDER PENTAGON

// TS_BORDER_320X240 - (TSTATES_PER_LINE * 16) - 8
#define TS_BORDER_352x272 5356  // START OF BORDER 48
#define TS_BORDER_352x272_TK_50  5545 // START OF BORDER TK 50HZ
#define TS_BORDER_352x224_TK_60  5089 // START OF BORDER TK 60HZ TS_BORDER_320X240 + (TSTATES_PER_LINE * 8) - 8
#define TS_BORDER_352x272_128 5222  // START OF BORDER 128
#define TS_BORDER_352x272_PENTAGON 9003  // START OF BORDER PENTAGON

// Colors for 6 bit mode
//                  //  BBGGRR 
#define BLACK       0b00000000 
#define BLUE        0b00100000
#define RED         0b00000010
#define MAGENTA     0b00100010
#define GREEN       0b00001000
#define CYAN        0b00101000
#define YELLOW      0b00001010
#define WHITE       0b00101010
#define BRI_BLACK   0b00000000
#define BRI_BLUE    0b00110000
#define BRI_RED     0b00000011
#define BRI_MAGENTA 0b00110011
#define BRI_GREEN   0b00001100
#define BRI_CYAN    0b00111100
#define BRI_YELLOW  0b00001111
#define BRI_WHITE   0b00111111
#define ORANGE      0b00000111 // used in ESPectrum logo text

#define NUM_SPECTRUM_COLORS 17

const int redPins[] = {RED_PINS_6B};
const int grePins[] = {GRE_PINS_6B};
const int bluPins[] = {BLU_PINS_6B};

#define zxColor(color,bright) VIDEO::spectrum_colors[bright ? color + 8 : color]

class VIDEO
{
public:

  // Initialize video
  static void Init();
    
  // Reset video
  static void Reset();

  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  // Video draw functions
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  static void EndFrame();
  static void Blank(unsigned int statestoadd, bool contended);
  static void Blank_Opcode(bool contended);
  static void Blank_Snow(unsigned int statestoadd, bool contended);
  static void Blank_Snow_Opcode(bool contended);
  // 48 / 128
  static void MainScreen_Blank(unsigned int statestoadd, bool contended);
  static void MainScreen_Blank_Opcode(bool contended);
  static void MainScreen(unsigned int statestoadd, bool contended);
  static void MainScreen_OSD(unsigned int statestoadd, bool contended);
  static void MainScreen_Opcode(bool contended);
  static void MainScreen_Blank_Snow(unsigned int statestoadd, bool contended);
  static void MainScreen_Blank_Snow_Opcode(bool contended);
  static void MainScreen_Snow(unsigned int statestoadd, bool contended);
  static void MainScreen_Snow_Opcode(bool contended);
  
  static void TopBorder_Blank();
  static void TopBorder();
  static void MiddleBorder();
  static void BottomBorder();
  static void BottomBorder_OSD();
  static void Border_Blank();

  static void TopBorder_Blank_Pentagon();
  static void TopBorder_Pentagon();
  static void MiddleBorder_Pentagon();
  static void BottomBorder_Pentagon();
  static void BottomBorder_OSD_Pentagon();  
  
  static void (*Draw)(unsigned int, bool);
  static void (*Draw_Opcode)(bool);
  static void (*Draw_OSD169)(unsigned int, bool);
  static void (*Draw_OSD43)();
  
  static void (*DrawBorder)();

  static void vgataskinit(void *unused);

  static uint8_t* grmem;

  static uint16_t spectrum_colors[NUM_SPECTRUM_COLORS];

  static uint16_t offBmp[SPEC_H];
  static uint16_t offAtt[SPEC_H];

  static VGA6Bit vga;

  static uint8_t borderColor;
  static uint32_t border32[8];
  static uint32_t brd;
  static bool brdChange;
  static bool brdnextframe;
  static uint32_t lastBrdTstate;
  static uint8_t brdnextline;
  static uint8_t brdlin_osdstart;
  static uint8_t brdlin_osdend;

  static uint8_t tStatesPerLine;
  static int tStatesScreen;
  static int tStatesBorder;  

  static uint8_t flashing;
  static uint8_t flash_ctr;

  static uint8_t att1;
  static uint8_t bmp1;
  static uint8_t att2;
  static uint8_t bmp2;
  
  static uint8_t dispUpdCycle;
  static bool snow_att;
  static bool dbl_att;
  static uint8_t lastbmp;
  static uint8_t lastatt;    
  static uint8_t snowpage;
  static uint8_t snowR;
  static bool snow_toggle;
  
  static uint8_t OSD;

  static uint32_t* SaveRect;

  static TaskHandle_t videoTaskHandle;

  static int VsyncFinetune[2];

  static uint32_t framecnt; // Frames elapsed

};

#endif // VIDEO_h
