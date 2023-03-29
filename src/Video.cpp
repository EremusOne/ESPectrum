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

#include "Video.h"
#include "CPU.h"
#include "MemESP.h"
#include "Config.h"
#include "OSDMain.h"
#include "hardconfig.h"
#include "hardpins.h"
#include "Z80_JLS/z80.h"
#include "Z80_JLS/z80operations.h"

VGA6Bit VIDEO::vga;
uint8_t VIDEO::borderColor = 0;
uint32_t VIDEO::brd;
uint32_t VIDEO::border32[8];
uint8_t VIDEO::flashing = 0;
uint8_t VIDEO::flash_ctr= 0;
bool VIDEO::OSD = false;
uint8_t VIDEO::tStatesPerLine;
int VIDEO::tStatesScreen;
uint8_t* VIDEO::grmem;

uint8_t (*VIDEO::getFloatBusData)() = &VIDEO::getFloatBusData48;

#ifdef NO_VIDEO
void (*VIDEO::Draw)(unsigned int, bool) = &VIDEO::NoVideo;
#else
void (*VIDEO::Draw)(unsigned int, bool) = &VIDEO::Blank;
#endif

void (*VIDEO::DrawOSD43)(unsigned int, bool) = &VIDEO::BottomBorder;
void (*VIDEO::DrawOSD169)(unsigned int, bool) = &VIDEO::MainScreen;

// static uint16_t specfast_colors[128]; // Array for faster color calc in Draw

void precalcColors() {
    
    for (int i = 0; i < NUM_SPECTRUM_COLORS; i++) {
        spectrum_colors[i] = (spectrum_colors[i] & VIDEO::vga.RGBAXMask) | VIDEO::vga.SBits;
    }

//    // Calc array for faster color calcs in ALU_video
//     for (int i = 0; i < (NUM_SPECTRUM_COLORS >> 1); i++) {
//         // Normal
//         specfast_colors[i] = spectrum_colors[i];
//         specfast_colors[i << 3] = spectrum_colors[i];
//         // Bright
//         specfast_colors[i | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
//         specfast_colors[(i << 3) | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
//     }

}

void precalcAluBytes() {

    // return;

    uint16_t specfast_colors[128]; // Array for faster color calc in Draw

    unsigned int pal[2],b0,b1,b2,b3;

    // Calc array for faster color calcs in Draw
    for (int i = 0; i < (NUM_SPECTRUM_COLORS >> 1); i++) {
        // Normal
        specfast_colors[i] = spectrum_colors[i];
        specfast_colors[i << 3] = spectrum_colors[i];
        // Bright
        specfast_colors[i | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
        specfast_colors[(i << 3) | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
    }

    // Alloc ALUbytes
    AluBytes = new uint32_t*[16];
    for (int i = 0; i < 16; i++) {
        AluBytes[i] = new uint32_t[256];
    }

    for (int i = 0; i < 16; i++) {
        for (int n = 0; n < 256; n++) {
            pal[0] = specfast_colors[n & 0x78];
            pal[1] = specfast_colors[n & 0x47];
            b0 = pal[(i >> 3) & 0x01];
            b1 = pal[(i >> 2) & 0x01];
            b2 = pal[(i >> 1) & 0x01];
            b3 = pal[i & 0x01];
            AluBytes[i][n]=b2 | (b3<<8) | (b0<<16) | (b1<<24);
        }
    }    

}

void deallocAluBytes() {

    // return;

    // For dealloc
    for (int i = 0; i < 16; i++) {
        delete[] AluBytes[i];
    }
    delete[] AluBytes;

};

uint16_t zxColor(uint8_t color, uint8_t bright) {
    if (bright) color += 8;
    return spectrum_colors[color];
}

// Precalc ULA_SWAP
#define ULA_SWAP(y) ((y & 0xC0) | ((y & 0x38) >> 3) | ((y & 0x07) << 3))
void precalcULASWAP() {
    for (int i = 0; i < SPEC_H; i++) {
        offBmp[i] = ULA_SWAP(i) << 5;
        offAtt[i] = ((i >> 3) << 5) + 0x1800;
    }
}

void precalcborder32()
{
    for (int i = 0; i < 8; i++) {
        uint8_t border = zxColor(i,0);
        VIDEO::border32[i] = border | (border << 8) | (border << 16) | (border << 24);
    }
}

void VIDEO::Init() {

    const Mode& vgaMode = Config::aspect_16_9 ? vga.MODE360x200 : vga.MODE320x240;
    OSD::scrW = vgaMode.hRes;
    OSD::scrH = vgaMode.vRes / vgaMode.vDiv;
    
    const int redPins[] = {RED_PINS_6B};
    const int grePins[] = {GRE_PINS_6B};
    const int bluPins[] = {BLU_PINS_6B};
    vga.init(vgaMode, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);

    precalcColors();    // precalculate colors for current VGA mode

    precalcAluBytes(); // Alloc and calc AluBytes

    precalcULASWAP();   // precalculate ULA SWAP values

    precalcborder32();  // Precalc border 32 bits values

    borderColor = 0;
    brd = border32[0];

    is169 = Config::aspect_16_9 ? 1 : 0;

    if (Config::getArch() == "48K") {
        tStatesPerLine = TSTATES_PER_LINE;
        tStatesScreen = is169 ? TS_SCREEN_360x200 : TS_SCREEN_320x240;
    } else {
        tStatesPerLine = TSTATES_PER_LINE_128;
        tStatesScreen = is169 ? TS_SCREEN_360x200_128 : TS_SCREEN_320x240_128;
    }

    #ifdef NO_VIDEO
        Draw = &NoVideo;
    #else
        Draw = &Blank;
    #endif

}

void VIDEO::Reset() {

    borderColor = 7;
    brd = border32[7];    

    is169 = Config::aspect_16_9 ? 1 : 0;
    
    if (Config::getArch() == "48K") {
        tStatesPerLine = TSTATES_PER_LINE;
        tStatesScreen = is169 ? TS_SCREEN_360x200 : TS_SCREEN_320x240;

    } else {
        tStatesPerLine = TSTATES_PER_LINE_128;
        tStatesScreen = is169 ? TS_SCREEN_360x200_128 : TS_SCREEN_320x240_128;
    }

    grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;

    #ifdef NO_VIDEO
        Draw = &NoVideo;
    #else
        Draw = &Blank;
    #endif

}

uint8_t IRAM_ATTR VIDEO::getFloatBusData48() {

    unsigned int currentTstates = CPU::tstates;

	unsigned short int line = currentTstates / 224; // int line
	if (line < 64 || line >= 256) return 0xFF;

	unsigned char halfpix = currentTstates % 224;
	if (halfpix >= 128) return 0xFF;

    switch (halfpix & 0x07) {
        case 3: { // Bitmap
            unsigned int bmpOffset = offBmp[line - 64];
            int hpoffset = (halfpix - 3) >> 2;
            return(grmem[bmpOffset + hpoffset]);
        }
        case 4: { // Attr
            unsigned int attOffset = offAtt[line - 64];
            int hpoffset = (halfpix - 3) >> 2;
            return(grmem[attOffset + hpoffset]);
        }
        case 5: { // Bitmap + 1
            unsigned int bmpOffset = offBmp[line - 64];
            int hpoffset = ((halfpix - 3) >> 2) + 1;
            return(grmem[bmpOffset + hpoffset]);
        }
        case 6: { // Attr + 1
            unsigned int attOffset = offAtt[line - 64];
            int hpoffset = ((halfpix - 3) >> 2) + 1;
            return(grmem[attOffset + hpoffset]);
        }
    }

    return(0xFF);

}

uint8_t IRAM_ATTR VIDEO::getFloatBusData128() {

    unsigned int currentTstates = CPU::tstates;

    currentTstates--;

	unsigned short int line = currentTstates / 228; // int line
	if (line < 63 || line >= 255) return 0xFF;

	unsigned char halfpix = currentTstates % 228;
	if (halfpix >= 128) return 0xFF;

    switch (halfpix & 0x07) {
        case 0: { // Bitmap
            unsigned int bmpOffset = offBmp[line - 63];
            int hpoffset = (halfpix) >> 2;
            return(grmem[bmpOffset + hpoffset]);
        }
        case 1: { // Attr
            unsigned int attOffset = offAtt[line - 63];
            int hpoffset = (halfpix) >> 2;
            return(grmem[attOffset + hpoffset]);
        }
        case 2: { // Bitmap + 1
            unsigned int bmpOffset = offBmp[line - 63];
            int hpoffset = ((halfpix) >> 2) + 1;
            return(grmem[bmpOffset + hpoffset]);
        }
        case 3: { // Attr + 1
            unsigned int attOffset = offAtt[line - 63];
            int hpoffset = ((halfpix) >> 2) + 1;
            return(grmem[attOffset + hpoffset]);
        }
    }

    return(0xFF);

}

///////////////////////////////////////////////////////////////////////////////
//  VIDEO DRAW FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR VIDEO::NoVideo(unsigned int statestoadd, bool contended) { CPU::tstates += statestoadd; }

void IRAM_ATTR VIDEO::TopBorder_Blank(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        tstateDraw += tStatesPerLine;
        lineptr32 = (uint32_t *)(vga.backBuffer[linedraw_cnt]);
        if (is169) lineptr32 += 5;
        coldraw_cnt = 0;
        Draw = &TopBorder;
        Draw(0,contended);
    }

}

void IRAM_ATTR VIDEO::TopBorder(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    statestoadd += video_rest;
    video_rest = statestoadd & 0x03; // Mod 4
    for (int i=0; i < (statestoadd >> 2); i++) {
        *lineptr32++ = brd;
        *lineptr32++ = brd;
        if (++coldraw_cnt == 40) {
            Draw = ++linedraw_cnt == (is169 ? 4 : 24) ? &MainScreen_Blank : &TopBorder_Blank;
            return;
        }
    }

}

void IRAM_ATTR VIDEO::MainScreen_Blank(unsigned int statestoadd, bool contended) {
    
    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        tstateDraw += tStatesPerLine;
        lineptr32 = (uint32_t *)(vga.backBuffer[linedraw_cnt]);
        if (is169) lineptr32 += 5;
        coldraw_cnt = 0;
        bmpOffset = offBmp[linedraw_cnt-(is169 ? 4 : 24)];
        attOffset = offAtt[linedraw_cnt-(is169 ? 4 : 24)];
        Draw = MainScreenLB;
        Draw(0,contended);
    }

}    

void IRAM_ATTR VIDEO::MainScreenLB(unsigned int statestoadd, bool contended) {
  
    if (contended)
        statestoadd += Z80Ops::is48 ? wait_st[(CPU::tstates + 1) % 224] : wait_st[(CPU::tstates + 3) % 228];

    CPU::tstates += statestoadd;
    statestoadd += video_rest;
    video_rest = statestoadd & 0x03; // Mod 4

    for (int i=0; i < (statestoadd >> 2); i++) {    
        *lineptr32++ = brd;
        *lineptr32++ = brd;
        if (++coldraw_cnt > 3) {      
            Draw = DrawOSD169;
            video_rest += ((statestoadd >> 2) - (i + 1))  << 2;
            Draw(0,false);
            return;
        }
    }
    
}


void IRAM_ATTR VIDEO::MainScreen(unsigned int statestoadd, bool contended) {
  
    uint8_t att, bmp;

    unsigned int palette[2]; //0 backcolor 1 Forecolor
    unsigned int a0,a1,a2,a3;    
    
    if (contended)
        statestoadd += Z80Ops::is48 ? wait_st[(CPU::tstates + 1) % 224] : wait_st[(CPU::tstates + 3) % 228];

    CPU::tstates += statestoadd;
    
    statestoadd += video_rest;
    video_rest = statestoadd & 0x03; // Mod 4

    for (int i=0; i < (statestoadd >> 2); i++) {    

        // if ((coldraw_cnt>3) && (coldraw_cnt<36)) {
        // if (coldraw_cnt<36) {

            att = grmem[attOffset++];       // get attribute byte
            bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];
            
            *lineptr32++ = AluBytes[bmp >> 4][att];
            *lineptr32++ = AluBytes[bmp & 0xF][att];

            // // Faster color calc
            // att = grmem[attOffset];  // get attribute byte

            // if (att & flashing) {
            //     palette[0] = specfast_colors[att & 0x47];
            //     palette[1] = specfast_colors[att & 0x78];
            // } else {
            //     palette[0] = specfast_colors[att & 0x78];
            //     palette[1] = specfast_colors[att & 0x47];
            // }

            // bmp = grmem[bmpOffset];  // get bitmap byte

            // a0 = palette[(bmp >> 7) & 0x01];
            // a1 = palette[(bmp >> 6) & 0x01];
            // a2 = palette[(bmp >> 5) & 0x01];
            // a3 = palette[(bmp >> 4) & 0x01];
            // *lineptr32++ = a2 | (a3<<8) | (a0<<16) | (a1<<24);

            // a0 = palette[(bmp >> 3) & 0x01];
            // a1 = palette[(bmp >> 2) & 0x01];
            // a2 = palette[(bmp >> 1) & 0x01];
            // a3 = palette[bmp & 0x01];
            // *lineptr32++ = a2 | (a3<<8) | (a0<<16) | (a1<<24);

            // attOffset++;
            // bmpOffset++;

        // } else {

        //     *lineptr32++ = brd;
        //     *lineptr32++ = brd;

        // }

        if (++coldraw_cnt > 35) {      
            Draw = MainScreenRB;
            video_rest += ((statestoadd >> 2) - (i + 1))  << 2;
            Draw(0,false);
            return;
        }

        // if (++coldraw_cnt == 40) {      
        //     Draw = ++linedraw_cnt == (is169 ? 196 : 216) ? &BottomBorder_Blank : &MainScreen_Blank;
        //     return;
        // }

    }

}

void IRAM_ATTR VIDEO::MainScreen_OSD(unsigned int statestoadd, bool contended) {

    uint8_t att, bmp;

    unsigned int palette[2]; //0 backcolor 1 Forecolor
    unsigned int a0,a1,a2,a3;    
    
    if (contended)
        statestoadd += Z80Ops::is48 ? wait_st[(CPU::tstates + 1) % 224] : wait_st[(CPU::tstates + 3) % 228];

    CPU::tstates += statestoadd;

    statestoadd += video_rest;
    video_rest = statestoadd & 0x03; // Mod 4
    for (int i=0; i < (statestoadd >> 2); i++) {    

        if ((linedraw_cnt>175) && (linedraw_cnt<192) && (coldraw_cnt>20) && (coldraw_cnt<39)) {
            lineptr32+=2;
            attOffset++;
            bmpOffset++;
            coldraw_cnt++;
            continue;
        }

        if ((coldraw_cnt>3) && (coldraw_cnt<36)) {

            att = grmem[attOffset++];       // get attribute byte
            bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];
            *lineptr32++ = AluBytes[bmp >> 4][att];
            *lineptr32++ = AluBytes[bmp & 0xF][att];

            // // Faster color calc
            // att = grmem[attOffset];  // get attribute byte

            // if (att & flashing) {
            //     palette[0] = specfast_colors[att & 0x47];
            //     palette[1] = specfast_colors[att & 0x78];
            // } else {
            //     palette[0] = specfast_colors[att & 0x78];
            //     palette[1] = specfast_colors[att & 0x47];
            // }

            // bmp = grmem[bmpOffset];  // get bitmap byte

            // a0 = palette[(bmp >> 7) & 0x01];
            // a1 = palette[(bmp >> 6) & 0x01];
            // a2 = palette[(bmp >> 5) & 0x01];
            // a3 = palette[(bmp >> 4) & 0x01];
            // *lineptr32++ = a2 | (a3<<8) | (a0<<16) | (a1<<24);

            // a0 = palette[(bmp >> 3) & 0x01];
            // a1 = palette[(bmp >> 2) & 0x01];
            // a2 = palette[(bmp >> 1) & 0x01];
            // a3 = palette[bmp & 0x01];
            // *lineptr32++ = a2 | (a3<<8) | (a0<<16) | (a1<<24);

            // attOffset++;
            // bmpOffset++;

        } else {
            *lineptr32++ = brd;
            *lineptr32++ = brd;
        }

        if (++coldraw_cnt == 40) {
            Draw = ++linedraw_cnt == 196 ? &BottomBorder_Blank : &MainScreen_Blank;
            return;
        }

    }

}

void IRAM_ATTR VIDEO::MainScreenRB(unsigned int statestoadd, bool contended) {

    if (contended)
        statestoadd += Z80Ops::is48 ? wait_st[(CPU::tstates + 1) % 224] : wait_st[(CPU::tstates + 3) % 228];

    CPU::tstates += statestoadd;
    statestoadd += video_rest;
    video_rest = statestoadd & 0x03; // Mod 4

    for (int i=0; i < (statestoadd >> 2); i++) {
        *lineptr32++ = brd;
        *lineptr32++ = brd;

        if (++coldraw_cnt == 40) {
            Draw = ++linedraw_cnt == (is169 ? 196 : 216) ? &BottomBorder_Blank : &MainScreen_Blank;
            return;
        }

    }

}

void IRAM_ATTR VIDEO::BottomBorder_Blank(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        tstateDraw += tStatesPerLine;
        lineptr32 = (uint32_t *)(vga.backBuffer[linedraw_cnt]);
        if (is169) lineptr32 += 5;        
        coldraw_cnt = 0;
        Draw = DrawOSD43;
        Draw(0,contended);
    }

}

void IRAM_ATTR VIDEO::BottomBorder(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03; // Mod 4
    for (int i=0; i < (statestoadd >> 2); i++) {    

        *lineptr32++ = brd;
        *lineptr32++ = brd;
        if (++coldraw_cnt == 40) {
            Draw = ++linedraw_cnt == (is169 ? 200 : 240) ? &Blank : &BottomBorder_Blank ;
            return;
        }
    }
}

void IRAM_ATTR VIDEO::BottomBorder_OSD(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03; // Mod 4
    for (int i=0; i < (statestoadd >> 2); i++) {    
        
        if ((linedraw_cnt<220) || (linedraw_cnt>235)) {
            *lineptr32++ = brd;
            *lineptr32++ = brd;
        } else {
            if ((coldraw_cnt<21) || (coldraw_cnt>38)) {
                *lineptr32++ = brd;
                *lineptr32++ = brd;
            } else lineptr32+=2;
        }
        
        if (++coldraw_cnt == 40) {
            Draw = ++linedraw_cnt == 240 ? &Blank : &BottomBorder_Blank ;
            return;
        }
    }

}

void IRAM_ATTR VIDEO::Blank(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    if (CPU::tstates < tStatesPerLine) {
        linedraw_cnt = 0;
        tstateDraw = tStatesScreen;
        Draw = &TopBorder_Blank;
    }

}

// ///////////////////////////////////////////////////////////////////////////////
// // Flush -> Flush screen after HALT
// ///////////////////////////////////////////////////////////////////////////////
void VIDEO::Flush() {

    while (CPU::tstates < CPU::statesInFrame) {
        Draw(tStatesPerLine,false);
    }

    // return;

    // // THIS IS SLIGHTLY FASTER BUT IT'S NOT CORRECT YET (WHITE LINE ON FPGA48all.tap)

    // uint8_t att, bmp;

    // int n = linedraw_cnt;
    // if (coldraw_cnt >= 40) n++;

    // // Top border
    // while (n < (is169 ? 4 : 24)) {

    //     lineptr32 = (uint32_t *)(vga.backBuffer[n]);
    //     if (is169) lineptr32 += 5;

    //     for (int i=0; i < 40; i++) {
    //         *lineptr32++ = brd;
    //         *lineptr32++ = brd;
    //     }

    //     n++;

    // }

    // if (DrawOSD169 == MainScreen) {

    //     //Mainscreen
    //     while (n < (is169 ? 196 : 216)) {

    //         lineptr32 = (uint32_t *)(vga.backBuffer[n]);
    //         if (is169) lineptr32 += 5;

    //         bmpOffset = offBmp[n-(is169 ? 4 : 24)];
    //         attOffset = offAtt[n-(is169 ? 4 : 24)];
    
    //         for (int i = 0; i < 4; i++) {    
    //             *lineptr32++ = brd;
    //             *lineptr32++ = brd;
    //         }

    //         for (int i = 0; i < 32; i++) {    

    //                 att = grmem[attOffset++];       // get attribute byte
    //                 if (att & flashing) {
    //                     bmp = ~grmem[bmpOffset++];  // get inverted bitmap byte
    //                 } else 
    //                     bmp = grmem[bmpOffset++];   // get bitmap byte

    //                 *lineptr32++ = AluBytes[bmp >> 4][att];
    //                 *lineptr32++ = AluBytes[bmp & 0xF][att];

    //         }

    //         for (int i = 0; i < 4; i++) {    
    //             *lineptr32++ = brd;
    //             *lineptr32++ = brd;
    //         }

    //         n++;

    //     }

    // } else {

    //     // Mainscreen OSD
    //     while (n < 196) {

    //         lineptr32 = (uint32_t *)(vga.backBuffer[n]);
    //         lineptr32 += 5;

    //         bmpOffset = offBmp[n - 4];
    //         attOffset = offAtt[n - 4];

    //         for (int i=0; i < 40; i++) {    

    //             if ((n>175) && (n<192) && (i>20) && (i<39)) {
    //                 lineptr32+=2;
    //                 attOffset++;
    //                 bmpOffset++;
    //                 coldraw_cnt++;
    //                 continue;
    //             }

    //             if ((i>3) && (i<36)) {
    //                 att = grmem[attOffset++];       // get attribute byte
    //                 if (att & flashing) {
    //                     bmp = ~grmem[bmpOffset++];  // get inverted bitmap byte
    //                 } else 
    //                     bmp = grmem[bmpOffset++];   // get bitmap byte

    //                 *lineptr32++ = AluBytes[bmp >> 4][att];
    //                 *lineptr32++ = AluBytes[bmp & 0xF][att];
    //             } else {
    //                 *lineptr32++ = brd;
    //                 *lineptr32++ = brd;
    //             }

    //         }

    //         n++;

    //     }

    // }

    // // Bottom border
    // if (DrawOSD43 == BottomBorder) {

    //     while (n < (is169 ? 200 : 240)) {

    //         lineptr32 = (uint32_t *)(vga.backBuffer[n]);
    //         if (is169) lineptr32 += 5;

    //         for (int i=0; i < 40; i++) {
    //             *lineptr32++ = brd;
    //             *lineptr32++ = brd;
    //         }

    //         n++;

    //     }    

    // } else {

    //     while (n < 240) {

    //         lineptr32 = (uint32_t *)(vga.backBuffer[n]);

    //         for (int i=0; i < 40; i++) {
    //             if ((n<220) || (n>235)) {
    //                 *lineptr32++ = brd;
    //                 *lineptr32++ = brd;
    //             } else {
    //                 if ((i<21) || (i>38)) {
    //                     *lineptr32++ = brd;
    //                     *lineptr32++ = brd;
    //                 } else lineptr32+=2;
    //             }
    //         }

    //         n++;

    //     }
    
    // }
    
    // Draw = &Blank;

}

