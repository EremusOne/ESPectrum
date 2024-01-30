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
#include "ESP32Lib/VGA/VGA6Bit.h"

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
  
  static void (*Draw)(unsigned int, bool);
  static void (*DrawOSD43)(unsigned int, bool);
  static void (*DrawOSD169)(unsigned int, bool);

  static void vgataskinit(void *unused);

  static uint8_t* grmem;

  static uint16_t spectrum_colors[NUM_SPECTRUM_COLORS];

  static uint16_t offBmp[SPEC_H];
  static uint16_t offAtt[SPEC_H];

  static VGA6Bit vga;

  static uint8_t borderColor;
  static uint32_t border32[8];
  static uint32_t brd;

  static uint8_t tStatesPerLine;
  static int tStatesScreen;

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

#define zxColor(color,bright) VIDEO::spectrum_colors[bright ? color + 8 : color]

#endif // VIDEO_h
