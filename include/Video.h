///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
//
// VIDEO EMULATION
//
// Copyright (c) 2023 Víctor Iborra [EremusOne]
// https://github.com/EremusOne/ZX-ESPectrum-IDF
//
// Based on ZX-ESPectrum-Wiimote
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

#ifndef VIDEO_h
#define VIDEO_h

#include <inttypes.h>
#include "ESPectrum.h"
#include "ESP32Lib/ESP32Lib.h"

#define SPEC_W 256
#define SPEC_H 192

#define TSTATES_PER_LINE 224
#define TSTATES_PER_LINE_128 228

#define TS_SCREEN_320x240 8944  // START OF VISIBLE ULA DRAW 48K @ 320x240, SCANLINE 40, -16 FROM BORDER
#define TS_SCREEN_320x240_128 8874  // START OF VISIBLE ULA DRAW 128K @ 320x240, SCANLINE 39, -16 FROM BORDER

#define TS_SCREEN_360x200 13424 // START OF VISIBLE ULA DRAW 48K @ 360x200, SCANLINE 60, -16 FROM BORDER
#define TS_SCREEN_360x200_128 13434 // START OF VISIBLE ULA DRAW 128K @ 360x200, SCANLINE 59, -16 FROM BORDER

class VIDEO
{
public:

    // Initialize video
    static void Init();
    
    // Reset video
    static void Reset();

    // Video draw functions
    static void IRAM_ATTR TopBorder_Blank(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR TopBorder(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR MainScreen_Blank(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR MainScreen(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR MainScreen_OSD(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR BottomBorder_Blank(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR BottomBorder(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR BottomBorder_OSD(unsigned int statestoadd, bool contended);    
    static void IRAM_ATTR Blank(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR MainScreenLB(unsigned int statestoadd, bool contended);
    static void IRAM_ATTR MainScreenRB(unsigned int statestoadd, bool contended);    
    static void IRAM_ATTR NoVideo(unsigned int statestoadd, bool contended);
    
    static uint8_t (*getFloatBusData)();
    static uint8_t IRAM_ATTR getFloatBusData48();
    static uint8_t IRAM_ATTR getFloatBusData128();    

    static void (*Draw)(unsigned int, bool contended);
    static void (*DrawOSD43)(unsigned int, bool contended);
    static void (*DrawOSD169)(unsigned int, bool contended);

    static uint8_t* grmem;

    // For flushing video buffer as fast as possible after HALT
    static void Flush();

    static VGA6Bit vga;

    static uint8_t borderColor;
    static uint32_t border32[8];
    static uint32_t brd;

    static uint8_t tStatesPerLine;
    static int tStatesScreen;

    static uint8_t flashing;
    static uint8_t flash_ctr;

    static bool OSD;

};

static unsigned int is169;

static unsigned int DRAM_ATTR offBmp[SPEC_H];
static unsigned int DRAM_ATTR offAtt[SPEC_H];

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

#define NUM_SPECTRUM_COLORS 16
static uint16_t spectrum_colors[NUM_SPECTRUM_COLORS] = {
    BLACK,     BLUE,     RED,     MAGENTA,     GREEN,     CYAN,     YELLOW,     WHITE,
    BRI_BLACK, BRI_BLUE, BRI_RED, BRI_MAGENTA, BRI_GREEN, BRI_CYAN, BRI_YELLOW, BRI_WHITE,
};

static uint32_t** AluBytes;

static unsigned char DrawStatus;

static uint32_t* lineptr32;

static unsigned int tstateDraw; // Drawing start point (in Tstates)
static unsigned int linedraw_cnt;
static unsigned int mainscrline_cnt;
static unsigned int coldraw_cnt;
static unsigned int video_rest;

static unsigned int bmpOffset;  // offset for bitmap in graphic memory
static unsigned int attOffset;  // offset for attrib in graphic memory

void precalcAluBytes();

void deallocAluBytes();

#endif // VIDEO_h
