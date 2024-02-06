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

#include "Video.h"
#include "VidPrecalc.h"
#include "CPU.h"
#include "MemESP.h"
#include "ZXKeyb.h"
#include "Config.h"
#include "OSDMain.h"
#include "hardconfig.h"
#include "hardpins.h"
#include "Z80_JLS/z80.h"
#include "Z80_JLS/z80operations.h"

#pragma GCC optimize("O3")

VGA6Bit VIDEO::vga;

uint16_t VIDEO::spectrum_colors[NUM_SPECTRUM_COLORS] = {
    BLACK,     BLUE,     RED,     MAGENTA,     GREEN,     CYAN,     YELLOW,     WHITE,
    BRI_BLACK, BRI_BLUE, BRI_RED, BRI_MAGENTA, BRI_GREEN, BRI_CYAN, BRI_YELLOW, BRI_WHITE, ORANGE
};

uint8_t VIDEO::borderColor = 0;
uint32_t VIDEO::brd;
uint32_t VIDEO::border32[8];
uint8_t VIDEO::flashing = 0;
uint8_t VIDEO::flash_ctr= 0;
bool VIDEO::OSD = false;
uint8_t VIDEO::tStatesPerLine;
int VIDEO::tStatesScreen;
uint8_t* VIDEO::grmem;
uint16_t VIDEO::offBmp[SPEC_H];
uint16_t VIDEO::offAtt[SPEC_H];
uint32_t* VIDEO::SaveRect;
int VIDEO::VsyncFinetune[2];
uint32_t VIDEO::framecnt = 0;

static unsigned int is169;

// static unsigned char DrawStatus;

static uint32_t* lineptr32;
static uint16_t* lineptr16;

static unsigned int tstateDraw; // Drawing start point (in Tstates)
static unsigned int linedraw_cnt;
static unsigned int lin_end, lin_end2, lin_end3;
static unsigned int coldraw_cnt;
static unsigned int col_end;
static unsigned int video_rest;

static unsigned int bmpOffset;  // offset for bitmap in graphic memory
static unsigned int attOffset;  // offset for attrib in graphic memory

/* DRAM_ATTR */ static const uint8_t wait_st[243] = { 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0
}; // sequence of wait states

IRAM_ATTR void VGA6Bit::interrupt(void *arg) {

    static int64_t prevmicros = 0;
    static int64_t elapsedmicros = 0;
    static int cntvsync = 0;

    // VGA6Bit * staticthis = (VGA6Bit *)arg;

    // if (++staticthis->currentLine == staticthis->totalLines << 1 ) {
	//     staticthis->currentLine = 0;
    //     ESPectrum::vsync = true;
    // } else ESPectrum::vsync = false;

    int64_t currentmicros = esp_timer_get_time();

    if (prevmicros) {

        elapsedmicros += currentmicros - prevmicros;

        if (elapsedmicros >= ESPectrum::target) {

            ESPectrum::vsync = true;

            // // This code is needed to "finetune" the sync. Without it, vsync and emu video gets slowly desynced.
            if (VIDEO::VsyncFinetune[0]) {
                if (cntvsync++ == VIDEO::VsyncFinetune[1]) {
                    elapsedmicros += VIDEO::VsyncFinetune[0];
                    cntvsync = 0;
                }
            }

            elapsedmicros -= ESPectrum::target;

        } else ESPectrum::vsync = false;
    
    } else {

        elapsedmicros = 0;
        ESPectrum::vsync = false;

    }

    prevmicros = currentmicros;

}

#ifdef NO_VIDEO
void (*VIDEO::Draw)(unsigned int, bool) = &VIDEO::NoVideo;
#else
void (*VIDEO::Draw)(unsigned int, bool) = &VIDEO::Blank;
#endif

void (*VIDEO::DrawOSD43)(unsigned int, bool) = &VIDEO::BottomBorder;
void (*VIDEO::DrawOSD169)(unsigned int, bool) = &VIDEO::MainScreen;

// void precalcColors() {
    
//     for (int i = 0; i < NUM_SPECTRUM_COLORS; i++) {
//         // printf("RGBAXMask: %d, SBits: %d\n",(int)VIDEO::vga.RGBAXMask,(int)VIDEO::vga.SBits);
//         // printf("Before: %d -> %d, ",i,(int)spectrum_colors[i]);
//         spectrum_colors[i] = (spectrum_colors[i] & VIDEO::vga.RGBAXMask) | VIDEO::vga.SBits;
//         // printf("After : %d -> %d\n",i,(int)spectrum_colors[i]);
//     }

// }

// void precalcAluBytes() {


//     uint16_t specfast_colors[128]; // Array for faster color calc in Draw

//     unsigned int pal[2],b0,b1,b2,b3;

//     // Calc array for faster color calcs in Draw
//     for (int i = 0; i < (NUM_SPECTRUM_COLORS >> 1); i++) {
//         // Normal
//         specfast_colors[i] = spectrum_colors[i];
//         specfast_colors[i << 3] = spectrum_colors[i];
//         // Bright
//         specfast_colors[i | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
//         specfast_colors[(i << 3) | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
//     }

//     // // Alloc ALUbytes
//     // for (int i = 0; i < 16; i++) {
//     //     AluBytes[i] = (uint32_t *) heap_caps_malloc(0x400, MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);
//     // }

//     FILE *f;

//     f = fopen("/sd/alubytes", "w");
//     fprintf(f,"{\n");
    
//     for (int i = 0; i < 16; i++) {
//         fprintf(f,"{");
//         for (int n = 0; n < 256; n++) {
//             pal[0] = specfast_colors[n & 0x78];
//             pal[1] = specfast_colors[n & 0x47];
//             b0 = pal[(i >> 3) & 0x01];
//             b1 = pal[(i >> 2) & 0x01];
//             b2 = pal[(i >> 1) & 0x01];
//             b3 = pal[i & 0x01];

//             // AluBytes[i][n]=b2 | (b3<<8) | (b0<<16) | (b1<<24);

//             int dato = b2 | (b3<<8) | (b0<<16) | (b1<<24);

//             fprintf(f,"0x%08x,",dato);

//         }
//         fprintf(f,"},\n");
//     }    

//     fprintf(f,"},\n");

//     fclose(f);

// }

// Precalc ULA_SWAP
#define ULA_SWAP(y) ((y & 0xC0) | ((y & 0x38) >> 3) | ((y & 0x07) << 3))
void precalcULASWAP() {
    for (int i = 0; i < SPEC_H; i++) {
        VIDEO::offBmp[i] = ULA_SWAP(i) << 5;
        VIDEO::offAtt[i] = ((i >> 3) << 5) + 0x1800;
    }
}

void precalcborder32()
{
    for (int i = 0; i < 8; i++) {
        uint8_t border = zxColor(i,0);
        VIDEO::border32[i] = border | (border << 8) | (border << 16) | (border << 24);
    }
}

const int redPins[] = {RED_PINS_6B};
const int grePins[] = {GRE_PINS_6B};
const int bluPins[] = {BLU_PINS_6B};

void VIDEO::vgataskinit(void *unused) {

    uint8_t Mode = (Config::videomode == 1 ? 2 : 8) + (Config::getArch() == "48K" ? 0 : (Config::getArch() == "128K" ? 2 : 4)) + (Config::aspect_16_9 ? 1 : 0);

    OSD::scrW = vidmodes[Mode][vmodeproperties::hRes];
    OSD::scrH = vidmodes[Mode][vmodeproperties::vRes] / vidmodes[Mode][vmodeproperties::vDiv];

    vga.VGA6Bit_useinterrupt=true;
    
    // CRT Centering
    if (Config::videomode == 2) {
        vga.CenterH = Config::CenterH;
        vga.CenterV = Config::CenterV;
    }
    
    // Init mode
    vga.init(Mode, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);    
    
    for (;;){}    

}

TaskHandle_t VIDEO::videoTaskHandle;

void VIDEO::Init() {

    if (Config::videomode) {
        xTaskCreatePinnedToCore(&VIDEO::vgataskinit, "videoTask", 1024, NULL, configMAX_PRIORITIES - 2, &videoTaskHandle, 1);        
        // Wait for vertical sync to ensure vga.init is done
        for (;;) {
            if (ESPectrum::vsync) break;
        }
    } else {

        int Mode = Config::aspect_16_9 ? 1 : 0;

        OSD::scrW = vidmodes[Mode][vmodeproperties::hRes];
        OSD::scrH = vidmodes[Mode][vmodeproperties::vRes] / vidmodes[Mode][vmodeproperties::vDiv];
        vga.VGA6Bit_useinterrupt=false;

        vga.init( Mode, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);

    }

    // precalcColors();    // precalculate colors for current VGA mode

    // Precalculate colors for current VGA mode
    for (int i = 0; i < NUM_SPECTRUM_COLORS; i++) {
        // printf("RGBAXMask: %d, SBits: %d\n",(int)VIDEO::vga.RGBAXMask,(int)VIDEO::vga.SBits);
        // printf("Before: %d -> %d, ",i,(int)spectrum_colors[i]);
        spectrum_colors[i] = (spectrum_colors[i] & VIDEO::vga.RGBAXMask) | VIDEO::vga.SBits;
        // printf("After : %d -> %d\n",i,(int)spectrum_colors[i]);
    }

    // precalcAluBytes(); // Alloc and calc AluBytes

    for (int n = 0; n < 16; n++)
        AluByte[n] = (unsigned int *)AluBytes[bitRead(VIDEO::vga.SBits,7)][n];

    precalcULASWAP();   // precalculate ULA SWAP values

    precalcborder32();  // Precalc border 32 bits values

    SaveRect = (uint32_t *) heap_caps_malloc(0x9000, MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);

}

void VIDEO::Reset() {

    borderColor = 7;
    brd = border32[7];

    is169 = Config::aspect_16_9 ? 1 : 0;

    OSD = false;

    string arch = Config::getArch();
    if (arch == "48K") {
        tStatesPerLine = TSTATES_PER_LINE;
        tStatesScreen = is169 ? TS_SCREEN_360x200 : TS_SCREEN_320x240;
        if (Config::videomode == 1) {
            VsyncFinetune[0] = is169 ? -1 : 0;
            VsyncFinetune[1] = is169 ? 152 : 0;
        } else {
            VsyncFinetune[0] = is169 ? 0 : 0;
            VsyncFinetune[1] = is169 ? 0 : 0;
        }

        DrawOSD43 = VIDEO::BottomBorder;
        DrawOSD169 = VIDEO::MainScreen;

    } else if (arch == "128K") {
        tStatesPerLine = TSTATES_PER_LINE_128;
        tStatesScreen = is169 ? TS_SCREEN_360x200_128 : TS_SCREEN_320x240_128;
        if (Config::videomode == 1) {
            VsyncFinetune[0] = is169 ? 1 : 1;
            VsyncFinetune[1] = is169 ? 123 : 123;
        } else {
            VsyncFinetune[0] = is169 ? 0 : 0;
            VsyncFinetune[1] = is169 ? 0 : 0;
        }

        DrawOSD43 = VIDEO::BottomBorder;
        DrawOSD169 = VIDEO::MainScreen;

    } else if (arch == "Pentagon") {
        tStatesPerLine = TSTATES_PER_LINE_PENTAGON;
        tStatesScreen = is169 ? TS_SCREEN_360x200_PENTAGON : TS_SCREEN_320x240_PENTAGON;
        // TODO: ADJUST THESE VALUES FOR PENTAGON
        if (Config::videomode == 1) {
            VsyncFinetune[0] = is169 ? 1 : 1;
            VsyncFinetune[1] = is169 ? 123 : 123;
        } else {
            VsyncFinetune[0] = is169 ? 0 : 0;
            VsyncFinetune[1] = is169 ? 0 : 0;
        }

        DrawOSD43 = BottomBorder_Pentagon;
        DrawOSD169 = MainScreen_Pentagon;

    }

    if (is169) {
        lin_end = 4;
        lin_end2 = 196;
        lin_end3 = 200;
    } else {
        lin_end = 24;
        lin_end2 = 216;
        lin_end3 = 240;
    }

    grmem = MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5];

    #ifdef NO_VIDEO
        Draw = &NoVideo;
    #else
        Draw = &Blank;
    #endif

}

///////////////////////////////////////////////////////////////////////////////
//  VIDEO DRAW FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

#ifdef NO_VIDEO
void VIDEO::NoVideo(unsigned int statestoadd, bool contended) { CPU::tstates += statestoadd; }
#endif

// IRAM_ATTR void VIDEO::NoDraw(unsigned int statestoadd, bool contended) {
//     if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];
//     CPU::tstates += statestoadd;
//     video_rest += statestoadd;
// }

IRAM_ATTR void VIDEO::TopBorder_Blank(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        tstateDraw += tStatesPerLine;
        lineptr32 = (uint32_t *)(vga.frameBuffer[linedraw_cnt]) + (is169 ? 5: 0);
        coldraw_cnt = 0;
        Draw = &TopBorder;
        TopBorder(0,false);
    }

}

IRAM_ATTR void VIDEO::TopBorder_Blank_Pentagon(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        tstateDraw += tStatesPerLine;
        lineptr16 = (uint16_t *)(vga.frameBuffer[linedraw_cnt]);
        if (is169) {
            coldraw_cnt = 10;
            col_end = 170;
        } else {
            coldraw_cnt = 0;
            col_end = 160;
        }
        Draw = &TopBorder_Pentagon;
        TopBorder_Pentagon(0,false);
    }

}

IRAM_ATTR void VIDEO::TopBorder(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03; // Mod 4

    unsigned int i = coldraw_cnt;

    coldraw_cnt += statestoadd >> 2;
    
    if (coldraw_cnt >= 40) {

        for (;i < 40;i++) {
            *lineptr32++ = brd;
            *lineptr32++ = brd;
        }

        Draw = ++linedraw_cnt == lin_end ? &MainScreen_Blank : &TopBorder_Blank;

    } else {

        for (;i < coldraw_cnt; i++) {
            *lineptr32++ = brd;
            *lineptr32++ = brd;
        }

    }

}

IRAM_ATTR void VIDEO::TopBorder_Pentagon(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    statestoadd += video_rest;
    
    video_rest = 0;
  
    if (coldraw_cnt + statestoadd >= col_end) {

        for (;;) {

            lineptr16[coldraw_cnt ^ 1] = (uint16_t) brd;

            if (++coldraw_cnt == col_end) {
                Draw = ++linedraw_cnt == lin_end ? &MainScreen_Blank_Pentagon : &TopBorder_Blank_Pentagon;
                return;
            }

        }

    } else {

        for (; statestoadd > 0; statestoadd--) {
            lineptr16[coldraw_cnt ^ 1] = (uint16_t) brd;
            coldraw_cnt++;
        }

    }

}

IRAM_ATTR void VIDEO::MainScreen_Blank(unsigned int statestoadd, bool contended) {    
    
    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        lineptr32 = (uint32_t *)(vga.frameBuffer[linedraw_cnt]) + (is169 ? 5: 0);
        coldraw_cnt = 0;
        unsigned int curline = linedraw_cnt - lin_end;
        bmpOffset = offBmp[curline];
        attOffset = offAtt[curline];
        Draw = MainScreenLB;
        MainScreenLB(0,contended);
    }

}    

IRAM_ATTR void VIDEO::MainScreen_Blank_Pentagon(unsigned int statestoadd, bool contended) {    
    
    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        lineptr32 = (uint32_t *)(vga.frameBuffer[linedraw_cnt]);
        lineptr16 = (uint16_t *)(vga.frameBuffer[linedraw_cnt]);
        unsigned int curline = linedraw_cnt - lin_end;        
        bmpOffset = offBmp[curline];
        attOffset = offAtt[curline];
        if (is169) {
            lineptr32 += 5;
            coldraw_cnt = 10;
            col_end = 25;
        } else {
            coldraw_cnt = 0;
            col_end = 15;
            curline -= 20;
        }
        Draw = MainScreenLB_Pentagon;
        MainScreenLB_Pentagon(0,false);
    }

}    

IRAM_ATTR void VIDEO::MainScreenLB(unsigned int statestoadd, bool contended) {    
  
    if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];

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

// // ---------------------------
// // ptime-128 compliant version
// // ---------------------------    
// IRAM_ATTR void VIDEO::MainScreenLB(unsigned int statestoadd, bool contended) {    
  
//     if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];

//     CPU::tstates += statestoadd;
//     statestoadd += video_rest;
//     video_rest = statestoadd & 0x03; // Mod 4

//     for (int i=0; i < (statestoadd >> 2); i++) {    
//         *lineptr32++ = brd;
//         *lineptr32++ = brd;
//         if (++coldraw_cnt > 3) {      
//             Draw = DrawOSD169;
//             video_rest += ((statestoadd >> 2) - (i + 1))  << 2;
//             dispUpdCycle = 6 + CPU::latetiming;
//             Draw(0,false);
//             video_rest = 0;
//             return;
//         }
//     }
    
// }

IRAM_ATTR void VIDEO::MainScreenLB_Pentagon(unsigned int statestoadd, bool contended) {    
  
    CPU::tstates += statestoadd;
    statestoadd += video_rest;

    video_rest = 0;
    for (int i=0; i < statestoadd; i++) {    
        lineptr16[coldraw_cnt ^ 1] = (uint16_t) brd;
        if (++coldraw_cnt > col_end) {
            lineptr32 += 8;
            coldraw_cnt = 4;
            col_end = is169 ? 154 : 144;
            Draw = DrawOSD169;
            video_rest += statestoadd - (i + 1) + 2; // Add 2 to compensate +2 in TS_SCREEN_320x240_PENTAGON
            Draw(0,false);
            return;
        }
    }

}

/* IRAM_ATTR */void VIDEO::MainScreen(unsigned int statestoadd, bool contended) {    

    uint8_t att, bmp;

    if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03;
    
    unsigned int loopCount = statestoadd >> 2;

    if (loopCount) {

        if (coldraw_cnt + loopCount > 35) {

            for (;;) {

                att = grmem[attOffset++];
                bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];

                *lineptr32++ = AluByte[bmp >> 4][att];
                *lineptr32++ = AluByte[bmp & 0xF][att];

                loopCount--;

                if (++coldraw_cnt > 35) {
                    Draw = MainScreenRB;
                    video_rest += loopCount << 2;
                    MainScreenRB(0,false);
                    return;
                }

            }

        } else {

            coldraw_cnt += loopCount;

            for (;loopCount > 0 ; loopCount--) {

                att = grmem[attOffset++];       // get attribute byte
                bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];

                *lineptr32++ = AluByte[bmp >> 4][att];
                *lineptr32++ = AluByte[bmp & 0xF][att];

            }

        }

    }

}

/* IRAM_ATTR */void VIDEO::MainScreen_Pentagon(unsigned int statestoadd, bool contended) {    

    uint8_t att, bmp;

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03;
   
    int loopCount = statestoadd >> 2;
   
    if (loopCount) {

        if (coldraw_cnt + loopCount > 35) {

            for (;;) {
                
                att = grmem[attOffset++];
                bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];

                *lineptr32++ = AluByte[bmp >> 4][att];
                *lineptr32++ = AluByte[bmp & 0xF][att];

                loopCount--;

                if (++coldraw_cnt > 35) {
                    coldraw_cnt = 0;
                    Draw = MainScreen_Pentagon_delay;            
                    video_rest += loopCount << 2;
                    MainScreen_Pentagon_delay(0,false);
                    return;
                }

            }

        } else {

            coldraw_cnt += loopCount;

            for (;loopCount > 0 ; loopCount--) {

                att = grmem[attOffset++];
                bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];

                *lineptr32++ = AluByte[bmp >> 4][att];
                *lineptr32++ = AluByte[bmp & 0xF][att];

            }

        }

    }

}

// Needed to compensate +2 tstates added before in MainScreenLB_Pentagon
IRAM_ATTR void VIDEO::MainScreen_Pentagon_delay(unsigned int statestoadd, bool contended) {    

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = 0;

    for (int i=0; i < statestoadd; i++) {
        if (++coldraw_cnt > 1) {
            coldraw_cnt = col_end;
            col_end = is169 ? 170 : 160;
            Draw = MainScreenRB_Pentagon;
            video_rest += statestoadd - (i + 1);
            MainScreenRB_Pentagon(0,false);
            return;
        }
    }

}

// // ---------------------------
// // ptime-128 compliant version
// // ---------------------------    
// void VIDEO::MainScreen(unsigned int statestoadd, bool contended) {

//     static uint8_t att1,bmp1;

//     if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];

//     CPU::tstates += statestoadd;

//     statestoadd += video_rest;
    
//     for (int i=0; i < statestoadd; i++) {    

//         switch(dispUpdCycle) {
//             case 0:
//             case 2:
//                 bmp1 = grmem[bmpOffset++];
//                 break;
//             case 1:
//                 att1 = grmem[attOffset++];  // get attribute byte
//             case 5:

//                 if (att1 & flashing) bmp1 = ~bmp1;
//                 *lineptr32++ = AluByte[bmp1 >> 4][att1];
//                 *lineptr32++ = AluByte[bmp1 & 0xF][att1];

//                 if (++coldraw_cnt > 35) {
//                     Draw = MainScreenRB;
//                     video_rest += statestoadd - (i + 1);
//                     MainScreenRB(0,false);
//                     return;
//                 }

//                 break;
//             case 3:
//                 att1 = grmem[attOffset++];  // get attribute byte
//                 break;
//         }

//         // Update the cycle counter.
//         ++dispUpdCycle &= 0x07;

//     }

// }

IRAM_ATTR void VIDEO::MainScreen_OSD(unsigned int statestoadd, bool contended) {    

    uint8_t att, bmp;

    if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03; // Mod 4

    for (int i=0; i < (statestoadd >> 2); i++) {    

        if ((linedraw_cnt>175) && (linedraw_cnt<192) && (coldraw_cnt>16) && (coldraw_cnt<35)) {
            lineptr32+=2;
            attOffset++;
            bmpOffset++;
        } else {
            att = grmem[attOffset++];       // get attribute byte
            bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];

            *lineptr32++ = AluByte[bmp >> 4][att];
            *lineptr32++ = AluByte[bmp & 0xF][att];

        }

        if (++coldraw_cnt > 35) {
            Draw = MainScreenRB;
            video_rest += ((statestoadd >> 2) - (i + 1))  << 2;
            MainScreenRB(0,false);
            return;
        }

    }

}

IRAM_ATTR void VIDEO::MainScreen_OSD_Pentagon(unsigned int statestoadd, bool contended) {    

    uint8_t att, bmp;

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03; // Mod 4
    
    for (int i=0; i < (statestoadd >> 2); i++) {

        if ((linedraw_cnt>175) && (linedraw_cnt<192) && (coldraw_cnt>16) && (coldraw_cnt<35)) {

            lineptr32 += 2;
            attOffset++;
            bmpOffset++;

        } else {

            att = grmem[attOffset++];       // get attribute byte
            bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];

            *lineptr32++ = AluByte[bmp >> 4][att];
            *lineptr32++ = AluByte[bmp & 0xF][att];
        
        }

        if (++coldraw_cnt > 35) {
            coldraw_cnt = 0;
            Draw = MainScreen_Pentagon_delay;
            video_rest += ((statestoadd >> 2) - (i + 1))  << 2;
            MainScreen_Pentagon_delay(0,false);
            return;
        }

    }

}

IRAM_ATTR void VIDEO::MainScreenRB(unsigned int statestoadd, bool contended) {

    if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03;

    for (int i = 0; i < statestoadd >> 2; i++) {

        *lineptr32++ = brd;
        *lineptr32++ = brd;

        if (++coldraw_cnt == 40) {

            tstateDraw += tStatesPerLine;
            Draw = ++linedraw_cnt == lin_end2 ? &BottomBorder_Blank : &MainScreen_Blank;
            return;

        }

    }

}

IRAM_ATTR void VIDEO::MainScreenRB_Pentagon(unsigned int statestoadd, bool contended) {    

    CPU::tstates += statestoadd;

    statestoadd += video_rest;
    video_rest = 0;
    for (int i=0; i < statestoadd; i++) {
        lineptr16[coldraw_cnt ^ 1] = (uint16_t) brd;
        if (++coldraw_cnt == col_end) {
            tstateDraw += tStatesPerLine;
            Draw = ++linedraw_cnt == lin_end2 ? &BottomBorder_Blank_Pentagon : &MainScreen_Blank_Pentagon;            
            return;
        }

    }

}

IRAM_ATTR void VIDEO::BottomBorder_Blank(unsigned int statestoadd, bool contended) {    

    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        tstateDraw += tStatesPerLine;
        lineptr32 = (uint32_t *)(vga.frameBuffer[linedraw_cnt]) + (is169 ? 5 : 0);
        coldraw_cnt = 0;
        Draw = DrawOSD43;
        Draw(0,contended);
    }

}

IRAM_ATTR void VIDEO::BottomBorder_Blank_Pentagon(unsigned int statestoadd, bool contended) {    

    CPU::tstates += statestoadd;

    if (CPU::tstates > tstateDraw) {
        video_rest = CPU::tstates - tstateDraw;
        tstateDraw += tStatesPerLine;
        lineptr16 = (uint16_t *)(vga.frameBuffer[linedraw_cnt]); // Pentagon
        if (is169) {
            coldraw_cnt = 10;
            col_end = 170;
        } else {
            coldraw_cnt = 0;
            col_end = 160;
        }
        Draw = DrawOSD43;
        Draw(0,false);
    }

}

IRAM_ATTR void VIDEO::BottomBorder(unsigned int statestoadd, bool contended) {    

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03; // Mod 4
    
    unsigned int i = coldraw_cnt;

    coldraw_cnt += statestoadd >> 2;
    
    if (coldraw_cnt > 39) {

        for (;i < 40;i++) {
            *lineptr32++ = brd;
            *lineptr32++ = brd;
        }

        Draw = ++linedraw_cnt == lin_end3 ? &Blank : &BottomBorder_Blank ;

    } else {

        for (;i < coldraw_cnt; i++) {
            *lineptr32++ = brd;
            *lineptr32++ = brd;
        }

    }

}

IRAM_ATTR void VIDEO::BottomBorder_Pentagon(unsigned int statestoadd, bool contended) {    

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = 0;
    for (int i=0; i < statestoadd; i++) {    
        lineptr16[coldraw_cnt ^ 1] = (uint16_t) brd;
        if (++coldraw_cnt == col_end) {
            Draw = ++linedraw_cnt == lin_end3 ? &Blank : &BottomBorder_Blank_Pentagon;
            return;
        }
    }

}

IRAM_ATTR void VIDEO::BottomBorder_OSD(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = statestoadd & 0x03; // Mod 4

    if (linedraw_cnt < 220 || linedraw_cnt > 235) {

        unsigned int i = coldraw_cnt;

        coldraw_cnt += statestoadd >> 2;
    
        if (coldraw_cnt > 39) {

            for (;i < 40;i++) {
                *lineptr32++ = brd;
                *lineptr32++ = brd;
            }

            Draw = ++linedraw_cnt == 240 ? &Blank : &BottomBorder_Blank ;

        } else {

            for (;i < coldraw_cnt; i++) {
                *lineptr32++ = brd;
                *lineptr32++ = brd;
            }

        }

    } else {

        for (unsigned int i=0; i < statestoadd >> 2; i++) {    
            
            if (coldraw_cnt < 21) {
                *lineptr32++ = brd;
                *lineptr32++ = brd;
            } else if (coldraw_cnt > 38) {
                *lineptr32++ = brd;
                *lineptr32++ = brd;
                Draw = ++linedraw_cnt == 240 ? &Blank : &BottomBorder_Blank ;
                return;
            } else lineptr32 += 2;

            coldraw_cnt++;

        }

    }

}

IRAM_ATTR void VIDEO::BottomBorder_OSD_Pentagon(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

    statestoadd += video_rest;

    video_rest = 0;

    for(;statestoadd > 0; statestoadd--) {

        if (linedraw_cnt < 220 || linedraw_cnt > 235)
            lineptr16[coldraw_cnt ^ 1] = (uint16_t) brd;
        else if (coldraw_cnt < 84 || coldraw_cnt > 155)
            lineptr16[coldraw_cnt ^ 1] = (uint16_t) brd;
        
        if (++coldraw_cnt > 159) {
            Draw = ++linedraw_cnt == 240 ? &Blank : &BottomBorder_Blank_Pentagon;
            return;
        }
    }

}

IRAM_ATTR void VIDEO::Blank(unsigned int statestoadd, bool contended) {

    CPU::tstates += statestoadd;

}

IRAM_ATTR void VIDEO::EndFrame() {

    linedraw_cnt = 0;
    tstateDraw = tStatesScreen;
    Draw = Z80Ops::isPentagon ? &TopBorder_Blank_Pentagon : &TopBorder_Blank;
    framecnt++;

}
