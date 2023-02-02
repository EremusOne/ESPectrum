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

#ifndef CPU_h
#define CPU_h

#include <inttypes.h>
#include "ESPectrum.h"
#include "ESP32Lib/ESP32Lib.h"

class CPU
{
public:
    // call this for initializing CPU
    static void setup();

    // call this for executing a frame's worth of instructions
    static void IRAM_ATTR loop();

    // call this for resetting the CPU
    static void reset();

    // Flush screen
    static void IRAM_ATTR FlushOnHalt();

    // get the number of CPU Tstates per frame (machine dependant)
    static uint32_t statesPerFrame();

    // get the number of microseconds per frame (machine dependant)
    static uint32_t microsPerFrame();

    // CPU Tstates elapsed in current frame
    static uint32_t tstates;

    // CPU Tstates elapsed since reset
    static uint64_t global_tstates;

    // CPU Tstates in frame
    static uint32_t statesInFrame;

    // Border color // TO DO: Move to ALUVIDEO OBJECT
    static uint8_t borderColor;
    static uint8_t BottomDraw;

    static VGA6Bit vga;

    // Frames elapsed
    static uint32_t framecnt;

};

// Video / ALU
#define SPEC_W 256
#define SPEC_H 192

// TO DO: Defines per arch (48, 128, etc..)

#define TSTATES_PER_LINE 224

#define TS_PHASE_1_320x240 8944  //  8960 - 16 =  8944 -> START OF VISIBLE ULA DRAW @ 320x240, SCANLINE 40
#define TS_PHASE_2_320x240 14320 // 14336 - 16 = 14320 -> START OF LEFT BORDER OF TOP LEFT CORNER OF MAINSCREEN, SCANLINE 64
#define TS_PHASE_3_320x240 57328 // 57344 - 16 = 57328 -> START OF BOTTOM BORDER, SCANLINE 256
#define TS_PHASE_4_320x240 62720 // END OF VISIBLE SCREEN, SCANLINE 280

#define TS_PHASE_1_360x200 13416 // START OF VISIBLE ULA DRAW @ 360x200, SCANLINE 60
#define TS_PHASE_2_360x200 14312 // START OF LEFT BORDER OF TOP LEFT CORNER OF MAINSCREEN, SCANLINE 64
#define TS_PHASE_3_360x200 57320 // START OF BOTTOM BORDER, SCANLINE 256
#define TS_PHASE_4_360x200 58240 // END OF VISIBLE SCREEN, SCANLINE 260

// DrawStatus values
#define TOPBORDER_BLANK 0
#define TOPBORDER 1
#define MAINSCREEN_BLANK 2
#define LEFTBORDER 3
#define LINEDRAW_SYNC 4
#define LINEDRAW 5
#define RIGHTBORDER 6
#define BOTTOMBORDER_BLANK 7
#define BOTTOMBORDER 8
#define BOTTOMBORDER_FPS 9
#define BLANK 10

static unsigned int is169;

static uint8_t flashing = 0;
static uint8_t flash_ctr = 0;

static unsigned int DRAM_ATTR offBmp[SPEC_H];
static unsigned int DRAM_ATTR offAtt[SPEC_H];

#define NUM_SPECTRUM_COLORS 16

static uint16_t spectrum_colors[NUM_SPECTRUM_COLORS] = {
    BLACK,     BLUE,     RED,     MAGENTA,     GREEN,     CYAN,     YELLOW,     WHITE,
    BRI_BLACK, BRI_BLUE, BRI_RED, BRI_MAGENTA, BRI_GREEN, BRI_CYAN, BRI_YELLOW, BRI_WHITE,
};

static uint32_t DRAM_ATTR ulabytes[16][256] = { 0 };

static unsigned int DRAM_ATTR lastBorder[312]= { 0 };

static unsigned char DrawStatus;

static uint8_t* grmem;
static uint32_t* lineptr32;

static unsigned int tstateDraw; // Drawing start point (in Tstates)
static unsigned int linedraw_cnt;
static unsigned int mainscrline_cnt;
static unsigned int coldraw_cnt;
static unsigned int ALU_video_rest;

static unsigned int bmpOffset;  // offset for bitmap in graphic memory
static unsigned int attOffset;  // offset for attrib in graphic memory

static unsigned int brd;

static unsigned char IRAM_ATTR delayContention(unsigned int currentTstates);

void ALU_video_init();
void ALU_video_reset();

static void (*ALU_draw)(unsigned int);
static void IRAM_ATTR ALU_video(unsigned int statestoadd);
static void IRAM_ATTR ALU_video_fast_43(unsigned int statestoadd);
static void IRAM_ATTR ALU_video_169(unsigned int statestoadd);
static void IRAM_ATTR ALU_flush_video();

#endif // CPU_h
