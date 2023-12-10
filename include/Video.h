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

#ifndef VIDEO_h
#define VIDEO_h

#include <inttypes.h>
#include "ESPectrum.h"
#include "ESP32Lib/ESP32Lib.h"

#define SPEC_W 256
#define SPEC_H 192

#define TSTATES_PER_LINE 224
#define TSTATES_PER_LINE_128 228
#define TSTATES_PER_LINE_PENTAGON 224

#define TS_SCREEN_320x240 8944  // START OF VISIBLE ULA DRAW 48K @ 320x240, SCANLINE 40, -16 FROM BORDER
#define TS_SCREEN_320x240_128 8874  // START OF VISIBLE ULA DRAW 128K @ 320x240, SCANLINE 39, -16 FROM BORDER
                                    // ( ADDITIONAL -2 SEEMS NEEDED IF NOT USING 2 TSTATES AT A TIME PAPER DRAWING VERSION)
#define TS_SCREEN_320x240_PENTAGON 12594  // START OF VISIBLE ULA DRAW PENTAGON @ 320x240, SCANLINE 56 + 48TS + 2TS (NEEDED TO FIT BORDER)

#define TS_SCREEN_360x200 13424 // START OF VISIBLE ULA DRAW 48K @ 360x200, SCANLINE 60, -16 FROM BORDER
#define TS_SCREEN_360x200_128 13434 // START OF VISIBLE ULA DRAW 128K @ 360x200, SCANLINE 59, -16 FROM BORDER
                                    // ( ADDITIONAL -2 SEEMS NEEDED IF NOT USING 2 TSTATES AT A TIME PAPER DRAWING VERSION)                                    
#define TS_SCREEN_360x200_PENTAGON 17074 // START OF VISIBLE ULA DRAW PENTAGON @ 360x200, SCANLINE 76, +48TS + 2TS (NEEDED TO FIT BORDER)

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
  // Common
  #ifdef NO_VIDEO
  static void NoVideo(unsigned int statestoadd, bool contended);
  #endif
  // static void NoDraw(unsigned int statestoadd, bool contended);
  static void EndFrame();
  static void Blank(unsigned int statestoadd, bool contended);
  
  // 48 / 128
  static void TopBorder_Blank(unsigned int statestoadd, bool contended);
  static void TopBorder(unsigned int statestoadd, bool contended);
  static void MainScreen_Blank(unsigned int statestoadd, bool contended);
  static void MainScreenLB(unsigned int statestoadd, bool contended);
  static void MainScreen(unsigned int statestoadd, bool contended);
  static void MainScreen_OSD(unsigned int statestoadd, bool contended);
  static void MainScreenRB(unsigned int statestoadd, bool contended);    
  static void BottomBorder_Blank(unsigned int statestoadd, bool contended);
  static void BottomBorder(unsigned int statestoadd, bool contended);
  static void BottomBorder_OSD(unsigned int statestoadd, bool contended);    
    
  // Pentagon
  static void TopBorder_Blank_Pentagon(unsigned int statestoadd, bool contended);
  static void TopBorder_Pentagon(unsigned int statestoadd, bool contended);
  static void MainScreen_Blank_Pentagon(unsigned int statestoadd, bool contended);
  static void MainScreenLB_Pentagon(unsigned int statestoadd, bool contended);    
  static void MainScreen_Pentagon(unsigned int statestoadd, bool contended);
  static void MainScreen_Pentagon_delay(unsigned int statestoadd, bool contended);  
  static void MainScreen_OSD_Pentagon(unsigned int statestoadd, bool contended);
  static void MainScreenRB_Pentagon(unsigned int statestoadd, bool contended);    
  static void BottomBorder_Blank_Pentagon(unsigned int statestoadd, bool contended);
  static void BottomBorder_Pentagon(unsigned int statestoadd, bool contended);
  static void BottomBorder_OSD_Pentagon(unsigned int statestoadd, bool contended);

  static uint8_t (*getFloatBusData)();
  static uint8_t getFloatBusData48();
  static uint8_t getFloatBusData128();    

  static void (*Draw)(unsigned int, bool contended);
  static void (*DrawOSD43)(unsigned int, bool contended);
  static void (*DrawOSD169)(unsigned int, bool contended);

  static void vgataskinit(void *unused);

  static uint8_t* grmem;

  static VGA6Bit vga;

  static uint8_t borderColor;
  static uint32_t border32[8];
  static uint32_t brd;

  static uint8_t tStatesPerLine;
  static int tStatesScreen;

  // static unsigned int tstateDraw; // Drawing start point (in Tstates)
  // static unsigned int linedraw_cnt;

  static uint8_t flashing;
  static uint8_t flash_ctr;

  // static uint8_t dispUpdCycle;

  // static uint8_t contendOffset;
  // static uint8_t contendMod;    

  static bool OSD;

  static uint32_t* SaveRect;

  static TaskHandle_t videoTaskHandle;

  static int VsyncFinetune[2];

  static uint32_t framecnt; // Frames elapsed

};

static unsigned int is169;

static uint16_t offBmp[SPEC_H];
static uint16_t offAtt[SPEC_H];

// Colors for 6 bit mode
//                            //   BB GGRR 
#define BLACK       0xC0      // 1100 0000
#define BLUE        0xE0      // 1110 0000
#define RED         0xC2      // 1100 0010
#define MAGENTA     0xE2      // 1110 0010
#define GREEN       0xC8      // 1100 1000
#define CYAN        0xE8      // 1110 1000
#define YELLOW      0xCA      // 1100 1010
#define WHITE       0xEA      // 1110 1010
#define BRI_BLACK   0xC0      // 1100 0000
#define BRI_BLUE    0xF0      // 1111 0000
#define BRI_RED     0xC3      // 1100 0011
#define BRI_MAGENTA 0xF3      // 1111 0011
#define BRI_GREEN   0xCC      // 1100 1100
#define BRI_CYAN    0xFC      // 1111 1100
#define BRI_YELLOW  0xCF      // 1100 1111
#define BRI_WHITE   0xFF      // 1111 1111

// used in ESPectrum logo text
#define ESP_ORANGE  0xC7      // 1100 0111

#define NUM_SPECTRUM_COLORS 16
static uint16_t spectrum_colors[NUM_SPECTRUM_COLORS] = {
    BLACK,     BLUE,     RED,     MAGENTA,     GREEN,     CYAN,     YELLOW,     WHITE,
    BRI_BLACK, BRI_BLUE, BRI_RED, BRI_MAGENTA, BRI_GREEN, BRI_CYAN, BRI_YELLOW, BRI_WHITE,
};

// uint16_t zxColor(uint8_t color, uint8_t bright);

static uint32_t* AluBytes[16];

// static unsigned char DrawStatus;

static uint32_t* lineptr32;
static uint16_t* lineptr16;

static unsigned int tstateDraw; // Drawing start point (in Tstates)
static unsigned int linedraw_cnt;
static unsigned int lin_end;
// static unsigned int mainscrline_cnt;
static unsigned int coldraw_cnt;
static unsigned int col_end;
static unsigned int video_rest;

static unsigned int bmpOffset;  // offset for bitmap in graphic memory
static unsigned int attOffset;  // offset for attrib in graphic memory

void precalcAluBytes();

#endif // VIDEO_h
