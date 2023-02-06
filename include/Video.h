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

// TO DO: Defines per arch (48, 128, etc..)
#define TSTATES_PER_LINE 224

#define TS_PHASE_1_320x240 8944  //  8960 - 16 =  8944 -> START OF VISIBLE ULA DRAW @ 320x240, SCANLINE 40
#define TS_PHASE_2_320x240 14320 // 14336 - 16 = 14320 -> START OF LEFT BORDER OF TOP LEFT CORNER OF MAINSCREEN, SCANLINE 64
#define TS_PHASE_3_320x240 57328 // 57344 - 16 = 57328 -> START OF BOTTOM BORDER, SCANLINE 256

#define TS_PHASE_1_360x200 13416 // START OF VISIBLE ULA DRAW @ 360x200, SCANLINE 60
#define TS_PHASE_2_360x200 14312 // START OF LEFT BORDER OF TOP LEFT CORNER OF MAINSCREEN, SCANLINE 64
#define TS_PHASE_3_360x200 57320 // START OF BOTTOM BORDER, SCANLINE 256

// DrawStatus values
#define TOPBORDER_BLANK 0
#define TOPBORDER 1
#define MAINSCREEN_BLANK 2
#define LEFTBORDER 3
#define LINEDRAW_SYNC 4
#define LINEDRAW 5
#define LINEDRAW_FPS 6
#define RIGHTBORDER 7
#define RIGHTBORDER_FPS 8
#define BOTTOMBORDER_BLANK 9
#define BOTTOMBORDER 10
#define BOTTOMBORDER_FPS 11
#define BLANK 12

class VIDEO
{
public:

    // Initialize video
    static void Init();
    
    // Reset video
    static void Reset();

    // Video draw function (4:3)
    static void IRAM_ATTR Draw_43(unsigned int statestoadd);

    // Video draw function (4:3 fast) No multicolour effects, no border effects
    // static void IRAM_ATTR Draw_43_fast(unsigned int statestoadd);

    // Video draw function (16:9) No border effects support (only 4 pixels of up & down border)
    static void IRAM_ATTR Draw_169(unsigned int statestoadd);
    
    // For flushing video buffer as fast as possible after HALT
    static void IRAM_ATTR Flush();

    static void (*Draw)(unsigned int);

    static VGA6Bit vga;

    static uint8_t borderColor;
    static unsigned lastBorder[312];

    static uint8_t LineDraw;
    static uint8_t BottomDraw;

    static uint8_t flashing;
    static uint8_t flash_ctr;

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

static uint32_t DRAM_ATTR ulabytes[16][256] = { 0 };

static unsigned char DrawStatus;

static uint8_t* grmem;
static uint32_t* lineptr32;

static unsigned int tstateDraw; // Drawing start point (in Tstates)
static unsigned int linedraw_cnt;
static unsigned int mainscrline_cnt;
static unsigned int coldraw_cnt;
static unsigned int video_rest;

static unsigned int bmpOffset;  // offset for bitmap in graphic memory
static unsigned int attOffset;  // offset for attrib in graphic memory

static unsigned int brd;

#endif // VIDEO_h
