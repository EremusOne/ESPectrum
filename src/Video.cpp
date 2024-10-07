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

#include "Video.h"
#include "VidPrecalc.h"
#include "cpuESP.h"
#include "MemESP.h"
#include "ZXKeyb.h"
#include "Config.h"
#include "OSDMain.h"
#include "messages.h"
#include "hardconfig.h"
#include "hardpins.h"
#include "Z80_JLS/z80.h"
#include "Z80_JLS/z80operations.h"

#pragma GCC optimize("O3")

extern Font Font6x8;

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
uint8_t VIDEO::OSD = 0;
uint8_t VIDEO::tStatesPerLine;
int VIDEO::tStatesScreen;
int VIDEO::tStatesBorder;
uint8_t* VIDEO::grmem;
uint16_t VIDEO::offBmp[SPEC_H];
uint16_t VIDEO::offAtt[SPEC_H];
uint32_t* VIDEO::SaveRect;
int VIDEO::VsyncFinetune[2];
uint32_t VIDEO::framecnt = 0;
uint8_t VIDEO::dispUpdCycle;
uint8_t VIDEO::att1;
uint8_t VIDEO::bmp1;
uint8_t VIDEO::att2;
uint8_t VIDEO::bmp2;
bool VIDEO::snow_att = false;
bool VIDEO::dbl_att = false;
// bool VIDEO::opCodeFetch;
uint8_t VIDEO::lastbmp;
uint8_t VIDEO::lastatt;    
uint8_t VIDEO::snowpage;
uint8_t VIDEO::snowR;
bool VIDEO::snow_toggle = false;

static unsigned int is169;

static uint32_t* lineptr32;

static unsigned int tstateDraw; // Drawing start point (in Tstates)
static unsigned int linedraw_cnt;
static unsigned int lin_end, lin_end2 /*, lin_end3*/;
static unsigned int coldraw_cnt;
static unsigned int col_end;
static unsigned int video_rest;
static unsigned int video_opcode_rest;
static unsigned int curline;

static unsigned int bmpOffset;  // offset for bitmap in graphic memory
static unsigned int attOffset;  // offset for attrib in graphic memory

static const uint8_t wait_st[128] = {
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
    6, 5, 4, 3, 2, 1, 0, 0, 6, 5, 4, 3, 2, 1, 0, 0,
}; // sequence of wait states

IRAM_ATTR void VGA6Bit::interrupt(void *arg) {

    static int64_t prevmicros = 0;
    static int64_t elapsedmicros = 0;
    static int cntvsync = 0;

    if (Config::tape_player) {
        ESPectrum::vsync = true;
        return;
    }

    int64_t currentmicros = esp_timer_get_time();

    if (prevmicros) {

        elapsedmicros += currentmicros - prevmicros;

        if (elapsedmicros >= ESPectrum::target[0]) {

            ESPectrum::vsync = true;

            // This code is needed to "finetune" the sync. Without it, vsync and emu video gets slowly desynced.
            if (VIDEO::VsyncFinetune[0]) {
                if (cntvsync++ == VIDEO::VsyncFinetune[1]) {
                    elapsedmicros += VIDEO::VsyncFinetune[0];
                    cntvsync = 0;
                }
            }

            elapsedmicros -= ESPectrum::target[0];

        } else ESPectrum::vsync = false;
    
    } else {

        elapsedmicros = 0;
        ESPectrum::vsync = false;

    }

    prevmicros = currentmicros;

}

void (*VIDEO::Draw)(unsigned int, bool) = &VIDEO::Blank;
void (*VIDEO::Draw_Opcode)(bool) = &VIDEO::Blank_Opcode;
void (*VIDEO::Draw_OSD169)(unsigned int, bool) = &VIDEO::MainScreen;
void (*VIDEO::Draw_OSD43)() = &VIDEO::BottomBorder;

void (*VIDEO::DrawBorder)() = &VIDEO::TopBorder_Blank;

static uint32_t* brdptr32;
static uint16_t* brdptr16;

uint32_t VIDEO::lastBrdTstate;
uint8_t VIDEO::brdnextline;
uint8_t VIDEO::brdlin_osdstart;
uint8_t VIDEO::brdlin_osdend;
bool VIDEO::brdChange = false;
bool VIDEO::brdnextframe = true;

// Precalc ULA_SWAP
#define ULA_SWAP(y) ((y & 0xC0) | ((y & 0x38) >> 3) | ((y & 0x07) << 3))
void precalcULASWAP() {
    for (int i = 0; i < SPEC_H; i++) {
        VIDEO::offBmp[i] = ULA_SWAP(i) << 5;
        VIDEO::offAtt[i] = ((i >> 3) << 5) + 0x1800;
    }
}

void precalcborder32() {
    for (int i = 0; i < 8; i++) {
        uint8_t border = zxColor(i,0);
        VIDEO::border32[i] = border | (border << 8) | (border << 16) | (border << 24);
    }
}

void VIDEO::vgataskinit(void *unused) {

    uint8_t Mode;

    if (Config::videomode == 1) {

        char c_arch = Config::arch[0];

        switch (c_arch) {
        case '4':
            Mode = 4;
            break;
        case 'T':
            Mode = Config::ALUTK == 0 ? 4 : Config::ALUTK == 1 ? 8 : 12; // Video mode depends on ULA chosen
            break;
        case '1':
            Mode = 16;
            break;
        case 'P':
            Mode = 20;
            break;
        }

        Mode += (Config::aspect_16_9 ? 2 : 0) + Config::scanlines;

        OSD::scrW = vidmodes[Mode][vmodeproperties::hRes];
        OSD::scrH = (vidmodes[Mode][vmodeproperties::vRes] / vidmodes[Mode][vmodeproperties::vDiv]) >> Config::scanlines;

    } else {

        char c_arch = Config::arch[0];

        switch (c_arch) {
        case '4':
            Mode = 24;
            break;
        case 'T':
            Mode = Config::ALUTK == 0 ? 24 : Config::ALUTK == 1 ? 25 : 26; // Video mode depends on ULA chosen
            break;
        case '1':
            Mode = 27;
            break;
        case 'P':
            Mode = 28;
            break;
        }

        OSD::scrW = vidmodes[Mode][vmodeproperties::hRes];
        OSD::scrH = vidmodes[Mode][vmodeproperties::vRes] / vidmodes[Mode][vmodeproperties::vDiv];

        // CRT Centering
        vga.CenterH = Config::CenterH;
        vga.CenterV = Config::CenterV;

    }

    vga.VGA6Bit_useinterrupt=true;
    
    // Init mode
    vga.init(Mode, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);    
    
    // This 'for' is needed for video mode with use_interruption = true.
    for (;;){}

}

TaskHandle_t VIDEO::videoTaskHandle;

void VIDEO::Init() {

    if (Config::videomode) {

        // xTaskCreatePinnedToCore(&VIDEO::vgataskinit, "videoTask", 1024, NULL, configMAX_PRIORITIES - 2, &videoTaskHandle, 1);
        xTaskCreatePinnedToCore(&VIDEO::vgataskinit, "videoTask", 1024, NULL, 5, &videoTaskHandle, 1);

        // Wait for vertical sync to ensure vga.init is done
        for (;;) {
            if (ESPectrum::vsync) break;
        }
        
    } else {

        int Mode = Config::aspect_16_9 ? 2 : 0;

        Mode += Config::scanlines;

        OSD::scrW = vidmodes[Mode][vmodeproperties::hRes];
        OSD::scrH = (vidmodes[Mode][vmodeproperties::vRes] / vidmodes[Mode][vmodeproperties::vDiv]) >> Config::scanlines;
        
        vga.VGA6Bit_useinterrupt=false;

        vga.init( Mode, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);

    }

    // Precalculate colors for current VGA mode
    for (int i = 0; i < NUM_SPECTRUM_COLORS; i++)
        spectrum_colors[i] = (spectrum_colors[i] & VIDEO::vga.RGBAXMask) | VIDEO::vga.SBits;

    for (int n = 0; n < 16; n++)
        AluByte[n] = (unsigned int *)AluBytes[bitRead(VIDEO::vga.SBits,7)][n];

    precalcULASWAP();   // precalculate ULA SWAP values

    precalcborder32();  // Precalc border 32 bits values

    SaveRect = (uint32_t *) heap_caps_malloc(0x9000, MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);

    // Set font & Codepage
    vga.setFont(Font6x8);
    vga.setCodepage(LANGCODEPAGE[Config::lang]);

}

void VIDEO::Reset() {

    borderColor = 7;
    brd = border32[7];

    is169 = Config::aspect_16_9 ? 1 : 0;

    OSD = 0;

    Draw_OSD169 = MainScreen;
    Draw_OSD43 = BottomBorder;
    DrawBorder = TopBorder_Blank;        

    if (Config::arch == "48K") {
        tStatesPerLine = TSTATES_PER_LINE;
        tStatesScreen = TS_SCREEN_48;
        tStatesBorder = Config::videomode == 2 ? TS_BORDER_352x272 : (is169 ? TS_BORDER_360x200 : TS_BORDER_320x240);  
        if (Config::videomode == 1) {
            VsyncFinetune[0] = is169 ? -1 : 0;
            VsyncFinetune[1] = is169 ? 152 : 0;
        } else {
            VsyncFinetune[0] = is169 ? 0 : 0;
            VsyncFinetune[1] = is169 ? 0 : 0;
        }

    } else if (Config::arch == "TK90X" || Config::arch == "TK95" ) {

        switch (Config::ALUTK) {
        case 0: // Ferranti
            tStatesPerLine = TSTATES_PER_LINE;
            tStatesScreen = TS_SCREEN_48;
            tStatesBorder = Config::videomode == 2 ? TS_BORDER_352x272 : (is169 ? TS_BORDER_360x200 : TS_BORDER_320x240);  
            break;
        case 1: // Microdigital 50hz
            tStatesPerLine = TSTATES_PER_LINE_TK_50;
            tStatesScreen = TS_SCREEN_TK_50;
            tStatesBorder = Config::videomode == 2 ? TS_BORDER_352x272_TK_50 : (is169 ? TS_BORDER_360x200_TK_50 : TS_BORDER_320x240_TK_50);  
            break;
        case 2: // Microdigital 60hz
            tStatesPerLine = TSTATES_PER_LINE_TK_60;
            tStatesScreen = TS_SCREEN_TK_60;
            tStatesBorder = Config::videomode == 2 ? TS_BORDER_352x224_TK_60 : (is169 ? TS_BORDER_360x200_TK_60 : TS_BORDER_320x240_TK_60);  
        }

        if (Config::videomode == 1) {
            VsyncFinetune[0] = is169 ? -1 : 0;
            VsyncFinetune[1] = is169 ? 152 : 0;
        } else {
            VsyncFinetune[0] = is169 ? 0 : 0;
            VsyncFinetune[1] = is169 ? 0 : 0;
        }

    } else if (Config::arch == "128K") {

        tStatesPerLine = TSTATES_PER_LINE_128;
        tStatesScreen = TS_SCREEN_128;
        tStatesBorder = Config::videomode == 2 ? TS_BORDER_352x272_128 : (is169 ? TS_BORDER_360x200_128 : TS_BORDER_320x240_128);
        if (Config::videomode == 1) {
            VsyncFinetune[0] = is169 ? 1 : 1;
            VsyncFinetune[1] = is169 ? 123 : 123;
        } else {
            VsyncFinetune[0] = is169 ? 0 : 0;
            VsyncFinetune[1] = is169 ? 0 : 0;
        }

    } else if (Config::arch == "Pentagon") {

        tStatesPerLine = TSTATES_PER_LINE_PENTAGON;
        tStatesScreen = TS_SCREEN_PENTAGON;
        tStatesBorder = Config::videomode == 2 ? TS_BORDER_352x272_PENTAGON : (is169 ? TS_BORDER_360x200_PENTAGON : TS_BORDER_320x240_PENTAGON);
        // TODO: ADJUST THESE VALUES FOR PENTAGON
        if (Config::videomode == 1) {
            VsyncFinetune[0] = is169 ? 1 : 1;
            VsyncFinetune[1] = is169 ? 123 : 123;
        } else {
            VsyncFinetune[0] = is169 ? 0 : 0;
            VsyncFinetune[1] = is169 ? 0 : 0;
        }

        Draw_OSD43 = BottomBorder_Pentagon;
        DrawBorder = TopBorder_Blank_Pentagon;        

    }

    brdnextline = tStatesPerLine - 160 ; // T-states to advance to next border line

    if (Config::videomode == 2) {

        brdnextline -= 16;

        if (Config::arch[0] == 'T' && Config::ALUTK == 2) {
            lin_end = 16;
            lin_end2 = 208;
            brdlin_osdstart = 208;
            brdlin_osdend = 223;
        } else {
            lin_end = 40;
            lin_end2 = 232;
            brdlin_osdstart = 236;
            brdlin_osdend = 251;
        }

    } else {

        if (is169) {
            brdnextline -= 16;
            lin_end = 4;
            lin_end2 = 196;
        } else {
            lin_end = 24;
            lin_end2 = 216;
            brdlin_osdstart = 220;
            brdlin_osdend = 235;
        }

    }

    grmem = MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5];

    VIDEO::snow_toggle = Config::arch != "Pentagon" ? Config::render : false;

    if (VIDEO::snow_toggle) {
        Draw = &Blank_Snow;
        Draw_Opcode = &Blank_Snow_Opcode;
    } else {
        Draw = &Blank;
        Draw_Opcode = &Blank_Opcode;
    }

    // Restart border drawing
    lastBrdTstate = tStatesBorder;
    brdChange = false;
    brdnextframe = true;

}

///////////////////////////////////////////////////////////////////////////////
//  VIDEO DRAW FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

IRAM_ATTR void VIDEO::MainScreen_Blank(unsigned int statestoadd, bool contended) {    
    
    CPU::tstates += statestoadd;

    if (CPU::tstates >= tstateDraw) {

        if (brdChange) DrawBorder(); // Needed to avoid tearing in demos like Gabba (Pentagon)
        
        lineptr32 = (uint32_t *)(vga.frameBuffer[linedraw_cnt]) + (Config::videomode == 2 ? 12 : is169 ? 13 : 8);

        coldraw_cnt = 0;

        curline = linedraw_cnt - lin_end;
        bmpOffset = offBmp[curline];
        attOffset = offAtt[curline];

        Draw = linedraw_cnt >= 176 && linedraw_cnt <= 191 ? Draw_OSD169 : MainScreen;
        Draw_Opcode = MainScreen_Opcode;

        video_rest = CPU::tstates - tstateDraw;
        Draw(0,false);

    }

}    

IRAM_ATTR void VIDEO::MainScreen_Blank_Opcode(bool contended) { MainScreen_Blank(4, contended); }

IRAM_ATTR void VIDEO::MainScreen_Blank_Snow(unsigned int statestoadd, bool contended) {    
    
    CPU::tstates += statestoadd;

    if (CPU::tstates >= tstateDraw) {

        if (brdChange) DrawBorder();

        lineptr32 = (uint32_t *)(vga.frameBuffer[linedraw_cnt]) + (Config::videomode == 2 ? 12 : is169 ? 13 : 8);

        coldraw_cnt = 0;

        curline = linedraw_cnt - lin_end;
        bmpOffset = offBmp[curline];
        attOffset = offAtt[curline];

        snowpage = MemESP::videoLatch ? 7 : 5;
        
        dispUpdCycle = 0; // For ULA cycle perfect emulation

        Draw = &MainScreen_Snow;
        Draw_Opcode = &MainScreen_Snow_Opcode;

        // For ULA cycle perfect emulation
        int vid_rest = CPU::tstates - tstateDraw;
        if (vid_rest) {
            CPU::tstates = tstateDraw;
            Draw(vid_rest,false);
        }

    }

}    

IRAM_ATTR void VIDEO::MainScreen_Blank_Snow_Opcode(bool contended) {    
    
    CPU::tstates += 4;

    if (CPU::tstates >= tstateDraw) {

        if (brdChange) DrawBorder();

        lineptr32 = (uint32_t *)(vga.frameBuffer[linedraw_cnt]) + (Config::videomode == 2 ? 12 : is169 ? 13: 8);

        coldraw_cnt = 0;

        curline = linedraw_cnt - lin_end;
        bmpOffset = offBmp[curline];
        attOffset = offAtt[curline];

        snowpage = MemESP::videoLatch ? 7 : 5;
        
        dispUpdCycle = 0; // For ptime-128 compliant version

        Draw = &MainScreen_Snow;
        Draw_Opcode = &MainScreen_Snow_Opcode;

        // For ULA cycle perfect emulation
        video_opcode_rest = CPU::tstates - tstateDraw;
        if (video_opcode_rest) {
            CPU::tstates = tstateDraw;
            Draw_Opcode(false);
            video_opcode_rest = 0;
        }

    }

}    

// ----------------------------------------------------------------------------------
// Fast video emulation with no ULA cycle emulation and no snow effect support
// ----------------------------------------------------------------------------------
IRAM_ATTR void VIDEO::MainScreen(unsigned int statestoadd, bool contended) {    

    if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];

    CPU::tstates += statestoadd;
    statestoadd += video_rest;
    video_rest = statestoadd & 0x03;
    unsigned int loopCount = statestoadd >> 2;
    coldraw_cnt += loopCount;

    if (coldraw_cnt >= 32) {
        tstateDraw += tStatesPerLine;
        if (++linedraw_cnt == lin_end2) {
            Draw = &Blank;
            Draw_Opcode = &Blank_Opcode;
        } else {
            Draw = &MainScreen_Blank;
            Draw_Opcode = &MainScreen_Blank_Opcode;
        }
        loopCount -= coldraw_cnt - 32;
    }

    for (;loopCount--;) {
        uint8_t att = grmem[attOffset++];
        uint8_t bmp = att & flashing ? ~grmem[bmpOffset++] : grmem[bmpOffset++];
        *lineptr32++ = AluByte[bmp >> 4][att];
        *lineptr32++ = AluByte[bmp & 0xF][att];
    }

}

IRAM_ATTR void VIDEO::MainScreen_OSD(unsigned int statestoadd, bool contended) {    

    if (contended) statestoadd += wait_st[CPU::tstates - tstateDraw];

    CPU::tstates += statestoadd;
    statestoadd += video_rest;
    video_rest = statestoadd & 0x03;
    unsigned int loopCount = statestoadd >> 2; 
    unsigned int coldraw_osd = coldraw_cnt;
    coldraw_cnt += loopCount;
    
    if (coldraw_cnt >= 32) {
        tstateDraw += tStatesPerLine;
        if (++linedraw_cnt == lin_end2) {
            Draw = &Blank;
            Draw_Opcode = &Blank_Opcode;
        } else {
            Draw = &MainScreen_Blank;
            Draw_Opcode = &MainScreen_Blank_Opcode;
        }
        loopCount -= coldraw_cnt - 32;
    }

    for (;loopCount--;) {
        if (coldraw_osd >= 13 && coldraw_osd <= 30) {
            lineptr32+=2;
            attOffset++;
            bmpOffset++;
        } else {
            uint8_t att = grmem[attOffset++];
            uint8_t bmp = (att & flashing) ? ~grmem[bmpOffset++] : grmem[bmpOffset++];
            *lineptr32++ = AluByte[bmp >> 4][att];
            *lineptr32++ = AluByte[bmp & 0xF][att];
        }
        coldraw_osd++;
    }

}

IRAM_ATTR void VIDEO::MainScreen_Opcode(bool contended) { Draw(4,contended); }

// ----------------------------------------------------------------------------------
// ULA cycle perfect emulation with snow effect support
// ----------------------------------------------------------------------------------
IRAM_ATTR void VIDEO::MainScreen_Snow(unsigned int statestoadd, bool contended) {

    bool do_stats = false;

    if (contended) statestoadd += wait_st[coldraw_cnt]; // [CPU::tstates - tstateDraw];

    CPU::tstates += statestoadd;
    
    unsigned int col_osd = coldraw_cnt >> 2;
    if (linedraw_cnt >= 176 && linedraw_cnt <= 191) do_stats = (VIDEO::Draw_OSD169 == VIDEO::MainScreen_OSD);
    
    coldraw_cnt += statestoadd;

    if (coldraw_cnt >= 128) {
        tstateDraw += tStatesPerLine;
        if (++linedraw_cnt == lin_end2) {
            Draw = &Blank_Snow;
            Draw_Opcode = &Blank_Snow_Opcode;
        } else {
            Draw = &MainScreen_Blank_Snow;
            Draw_Opcode = &MainScreen_Blank_Snow_Opcode;
        }
        statestoadd -= coldraw_cnt - 128;  
    }

    for (;statestoadd--;) {

        switch(dispUpdCycle) {
            
            // In Weiv's Spectramine cycle starts in 2 and half black strip shows at 14349 in ptime-128.tap (early timings).
            // In SpecEmu cycle starts in 3, black strip at 14350. Will use Weiv's data for now.
            case 2:
                bmp1 = grmem[bmpOffset++];
                lastbmp = bmp1;
                break;
            case 3:
                if (snow_att) {
                    att1 = MemESP::ram[snowpage][(attOffset++ & 0xff80) | snowR];  // get attribute byte
                    snow_att = false;
                } else
                    att1 = grmem[attOffset++];  // get attribute byte                

                lastatt = att1;

                if (do_stats && (col_osd >= 13 && col_osd <= 30)) {                    
                    lineptr32 += 2;
                } else {
                    if (att1 & flashing) bmp1 = ~bmp1;
                    *lineptr32++ = AluByte[bmp1 >> 4][att1];
                    *lineptr32++ = AluByte[bmp1 & 0xF][att1];
                }

                col_osd++;

                break;
            case 4:
                bmp2 = grmem[bmpOffset++];
                break;
            case 5:            
                if (dbl_att) {
                    att2 = lastatt;
                    attOffset++;
                    dbl_att = false;
                } else
                    att2 = grmem[attOffset++];  // get attribute byte

                if (do_stats && (col_osd >= 13 && col_osd <= 30)) {
                    lineptr32 += 2;
                } else {
                    if (att2 & flashing) bmp2 = ~bmp2;
                    *lineptr32++ = AluByte[bmp2 >> 4][att2];
                    *lineptr32++ = AluByte[bmp2 & 0xF][att2];

                }

                col_osd++;

        }

        ++dispUpdCycle &= 0x07; // Update the cycle counter.

    }

}

// ----------------------------------------------------------------------------------
// ULA cycle perfect emulation with snow effect support
// ----------------------------------------------------------------------------------
IRAM_ATTR void VIDEO::MainScreen_Snow_Opcode(bool contended) {

    int snow_effect = 0;
    unsigned int addr;
    bool do_stats = false;

    unsigned int statestoadd = video_opcode_rest ? video_opcode_rest : 4;

    if (contended) statestoadd += wait_st[coldraw_cnt]; // [CPU::tstates - tstateDraw];

    CPU::tstates += statestoadd;

    unsigned int col_osd = coldraw_cnt >> 2;
    if (linedraw_cnt >= 176 && linedraw_cnt <= 191) do_stats = (VIDEO::Draw_OSD169 == VIDEO::MainScreen_OSD);
    
    coldraw_cnt += statestoadd;

    if (coldraw_cnt >= 128) {
        tstateDraw += tStatesPerLine;
        if (++linedraw_cnt == lin_end2) {
            Draw =&Blank_Snow;
            Draw_Opcode = &Blank_Snow_Opcode;
        } else {
            Draw = &MainScreen_Blank_Snow;
            Draw_Opcode = &MainScreen_Blank_Snow_Opcode;
        }

        statestoadd -= coldraw_cnt - 128;

    }

    if (dispUpdCycle == 6) {
        dispUpdCycle = 2;
        return;
    }

    // Determine if snow effect can be applied
    uint8_t page = Z80::getRegI() & 0xc0;
    if (page == 0x40) { // Snow 48K, 128K
        snow_effect = 1;
        snowpage = MemESP::videoLatch ? 7 : 5;
    } else if (Z80Ops::is128 && (MemESP::bankLatch & 0x01) && page == 0xc0) {  // Snow 128K
        snow_effect = 1;
        if (MemESP::bankLatch == 1 || MemESP::bankLatch == 3)
            snowpage = MemESP::videoLatch ? 3 : 1;
        else
            snowpage = MemESP::videoLatch ? 7 : 5;
    }

    for (;statestoadd--;) {

        switch(dispUpdCycle) {
            
            // In Weiv's Spectramine cycle starts in 2 and half black strip shows at 14349 in ptime-128.tap (early timings).
            // In SpecEmu cycle starts in 3, black strip at 14350. Will use Weiv's data for now.
            
            case 2:

                if (snow_effect && statestoadd == 0) {
                    snowR = Z80::getRegR() & 0x7f;
                    bmp1 = MemESP::ram[snowpage][(bmpOffset++ & 0xff80) | snowR];
                    snow_att = true;
                } else
                    bmp1 = grmem[bmpOffset++];

                lastbmp = bmp1;

                break;

            case 3:

                if (snow_att) {
                    att1 = MemESP::ram[snowpage][(attOffset++ & 0xff80) | snowR];  // get attribute byte
                    snow_att = false;
                } else
                    att1 = grmem[attOffset++];  // get attribute byte                

                lastatt = att1;

                if (do_stats && (col_osd >= 13 && col_osd <= 30)) {
                    lineptr32 += 2;
                } else {
                    if (att1 & flashing) bmp1 = ~bmp1;
                    *lineptr32++ = AluByte[bmp1 >> 4][att1];
                    *lineptr32++ = AluByte[bmp1 & 0xF][att1];
                }

                col_osd++;

                break;

            case 4:

                if (snow_effect && statestoadd == 0) {
                    bmp2 = lastbmp;
                    bmpOffset++;
                    dbl_att = true;                    
                } else
                    bmp2 = grmem[bmpOffset++];

                break;

            case 5:            

                if (dbl_att) {
                    att2 = lastatt;
                    attOffset++;
                    dbl_att = false;
                } else
                    att2 = grmem[attOffset++];  // get attribute byte

                if (do_stats && (col_osd >= 13 && col_osd <= 30)) {
                    lineptr32 += 2;
                } else {
                    if (att2 & flashing) bmp2 = ~bmp2;
                    *lineptr32++ = AluByte[bmp2 >> 4][att2];
                    *lineptr32++ = AluByte[bmp2 & 0xF][att2];
                }

                col_osd++;

        }

        ++dispUpdCycle &= 0x07; // Update the cycle counter.

    }

}

IRAM_ATTR void VIDEO::Blank(unsigned int statestoadd, bool contended) { CPU::tstates += statestoadd; }
IRAM_ATTR void VIDEO::Blank_Opcode(bool contended) { CPU::tstates += 4; }
IRAM_ATTR void VIDEO::Blank_Snow(unsigned int statestoadd, bool contended) { CPU::tstates += statestoadd; }
IRAM_ATTR void VIDEO::Blank_Snow_Opcode(bool contended) { CPU::tstates += 4; }

IRAM_ATTR void VIDEO::EndFrame() {

    linedraw_cnt = lin_end;

    tstateDraw = tStatesScreen;

    if (VIDEO::snow_toggle) {
        Draw = &MainScreen_Blank_Snow;
        Draw_Opcode = &MainScreen_Blank_Snow_Opcode;
    } else {
        Draw = &MainScreen_Blank;
        Draw_Opcode = &MainScreen_Blank_Opcode;
    }

    // if (brdChange) {
    //     DrawBorder();
    //     brdnextframe = true;
    // } else {
    //     if (brdnextframe) {
    //         DrawBorder();
    //         brdnextframe = false;
    //     }
    // }

    if (brdChange || brdnextframe) {
        DrawBorder();
        brdnextframe = brdChange;
    }

    // Restart border drawing
    DrawBorder = Z80Ops::isPentagon ? &TopBorder_Blank_Pentagon : &TopBorder_Blank;
    lastBrdTstate = tStatesBorder;
    brdChange = false;

    framecnt++;

}

//----------------------------------------------------------------------------------------------------------------
// Border Drawing
//----------------------------------------------------------------------------------------------------------------

// IRAM_ATTR void VIDEO::DrawBorderFast() {

//     uint8_t border = zxColor(borderColor,0);

//     int i = 0;

//     // Top border
//     for (; i < lin_end; i++) memset((uint32_t *)(vga.frameBuffer[i]),border, vga.xres);

//     // Paper border
//     int brdsize = (vga.xres - SPEC_W) >> 1;
//     for (; i < lin_end2; i++) {
//         memset((uint32_t *)(vga.frameBuffer[i]), border, brdsize);
//         memset((uint32_t *)(vga.frameBuffer[i] + vga.xres - brdsize), border, brdsize);
//     }

//     // Bottom border
//     for (; i < OSD::scrH; i++) memset((uint32_t *)(vga.frameBuffer[i]),border, vga.xres);

// }

static int brdcol_cnt = 0;
static int brdlin_cnt = 0;

IRAM_ATTR void VIDEO::TopBorder_Blank() {

    if (CPU::tstates >= tStatesBorder) {
        brdcol_cnt = 0;
        brdlin_cnt = 0;
        brdptr32 = (uint32_t *)(vga.frameBuffer[brdlin_cnt]) + (Config::videomode == 2 ? 0 : (is169 ? 1 : 0));
        DrawBorder = &TopBorder;
        DrawBorder();
    }

}    

IRAM_ATTR void VIDEO::TopBorder() {

    while (lastBrdTstate <= CPU::tstates) {

        *brdptr32++ = brd;
        *brdptr32++ = brd;

        lastBrdTstate += 4;

        brdcol_cnt++;

        if (brdcol_cnt == (Config::videomode == 2 || is169 ? 44 : 40)) {
            brdlin_cnt++;
            brdptr32 = (uint32_t *)(vga.frameBuffer[brdlin_cnt]) + (Config::videomode == 2 ? 0 : (is169 ? 1 : 0));
            brdcol_cnt = 0;
            lastBrdTstate += brdnextline;
            if (brdlin_cnt == lin_end) {
                DrawBorder = &MiddleBorder;
                MiddleBorder();
                return;
            }
        }

    }

}    

IRAM_ATTR void VIDEO::MiddleBorder() {

    while (lastBrdTstate <= CPU::tstates) {

        *brdptr32++ = brd;
        *brdptr32++ = brd;        

        lastBrdTstate+=4;

        brdcol_cnt++;

        if (brdcol_cnt == (Config::videomode == 2 || is169 ? 6 : 4)) {
            lastBrdTstate += 128;
            brdptr32 += 64;
            brdcol_cnt += 32;
        } else if (brdcol_cnt == (Config::videomode == 2 || is169 ? 44 : 40)) {
            brdlin_cnt++;
            brdptr32 = (uint32_t *)(vga.frameBuffer[brdlin_cnt]) + (Config::videomode == 2 ? 0 : (is169 ? 1 : 0));
            brdcol_cnt = 0;          
            lastBrdTstate += brdnextline;
            if (brdlin_cnt == lin_end2) {                                            
                DrawBorder = Draw_OSD43;
                DrawBorder();
                return;
            }
        }

    }

}    

IRAM_ATTR void VIDEO::BottomBorder() {

    while (lastBrdTstate <= CPU::tstates) {

        *brdptr32++ = brd;
        *brdptr32++ = brd;        

        lastBrdTstate+=4;

        brdcol_cnt++;

        if (brdcol_cnt == (Config::videomode == 2 || is169 ? 44 : 40)) {
            brdlin_cnt++;
            brdptr32 = (uint32_t *)(vga.frameBuffer[brdlin_cnt]) + (Config::videomode == 2 ? 0 : (is169 ? 1 : 0));
            brdcol_cnt = 0;
            lastBrdTstate += brdnextline;
            if (brdlin_cnt == OSD::scrH) {
                DrawBorder = &Border_Blank;
                return;
            }
        }

    }

}    

IRAM_ATTR void VIDEO::BottomBorder_OSD() {

    while (lastBrdTstate <= CPU::tstates) {

        if (brdlin_cnt < brdlin_osdstart || brdlin_cnt > brdlin_osdend) {        
            *brdptr32++ = brd;
            *brdptr32++ = brd;        
        } else if (brdcol_cnt < 21 || brdcol_cnt > 38) {
            *brdptr32++ = brd;
            *brdptr32++ = brd;        
        } else {
            brdptr32 += 2;
        }

        lastBrdTstate+=4;

        brdcol_cnt++;

        if (brdcol_cnt == (Config::videomode == 2 || is169 ? 44 : 40)) {
            brdlin_cnt++;
            brdptr32 = (uint32_t *)(vga.frameBuffer[brdlin_cnt]) + (Config::videomode == 2 ? 0 : (is169 ? 1 : 0));
            brdcol_cnt = 0;
            lastBrdTstate += brdnextline;
            if (brdlin_cnt == OSD::scrH) {
                DrawBorder = &Border_Blank;
                return;
            }
        }

    }

}    

IRAM_ATTR void VIDEO::Border_Blank() {

}    

static int brdcol_end = 0;
static int brdcol_end1 = 0;

IRAM_ATTR void VIDEO::TopBorder_Blank_Pentagon() {

    if (CPU::tstates >= tStatesBorder) {

        brdcol_cnt = Config::videomode == 2 ? 0 : (is169 ? 2 : 0);
        brdcol_end = Config::videomode == 2 ? 176 : (is169 ? 178 : 160);
        brdcol_end1 = Config::videomode == 2 ? 24 : (is169 ? 26 : 16);

        brdlin_cnt = 0;
        brdptr16 = (uint16_t *)(vga.frameBuffer[brdlin_cnt]);
        DrawBorder = &TopBorder_Pentagon;
        DrawBorder();
    }

}    

IRAM_ATTR void VIDEO::TopBorder_Pentagon() {

    while (lastBrdTstate <= CPU::tstates) {

        brdptr16[brdcol_cnt ^ 1] = (uint16_t) brd;

        lastBrdTstate++;

        brdcol_cnt++;

        if (brdcol_cnt == brdcol_end) {
            brdlin_cnt++;
            brdptr16 = (uint16_t *)(vga.frameBuffer[brdlin_cnt]);            
            brdcol_cnt = Config::videomode == 2 ? 0 : (is169 ? 2 : 0);
            lastBrdTstate += brdnextline;            
            if (brdlin_cnt == lin_end) {
                DrawBorder = &MiddleBorder_Pentagon;
                MiddleBorder_Pentagon();
                return;
            }
        }

    }

}    

IRAM_ATTR void VIDEO::MiddleBorder_Pentagon() {

    while (lastBrdTstate <= CPU::tstates) {

        brdptr16[brdcol_cnt ^ 1] = (uint16_t) brd;

        lastBrdTstate++;

        brdcol_cnt++;

        if (brdcol_cnt == brdcol_end1) {
            lastBrdTstate += 128;
            brdcol_cnt = Config::videomode == 2 ? 152 : (is169 ? 154 : 144);
        } else if (brdcol_cnt == brdcol_end) {
            brdlin_cnt++;
            brdptr16 = (uint16_t *)(vga.frameBuffer[brdlin_cnt]);                        
            brdcol_cnt = Config::videomode == 2 ? 0 : (is169 ? 2 : 0);
            lastBrdTstate += brdnextline;
            if (brdlin_cnt == lin_end2) {
                DrawBorder = Draw_OSD43;
                DrawBorder();
                return;
            }
        }

    }

}    

IRAM_ATTR void VIDEO::BottomBorder_Pentagon() {

    while (lastBrdTstate <= CPU::tstates) {

        brdptr16[brdcol_cnt ^ 1] = (uint16_t) brd;

        lastBrdTstate++;

        brdcol_cnt++;

        if (brdcol_cnt == brdcol_end) {
            brdlin_cnt++;
            brdptr16 = (uint16_t *)(vga.frameBuffer[brdlin_cnt]);
            brdcol_cnt = Config::videomode == 2 ? 0 : (is169 ? 2 : 0);
            lastBrdTstate += brdnextline;
            if (brdlin_cnt == OSD::scrH) {            
                DrawBorder = &Border_Blank;
                return;
            }
        }

    }

}    

IRAM_ATTR void VIDEO::BottomBorder_OSD_Pentagon() {

    while (lastBrdTstate <= CPU::tstates) {

        if (brdlin_cnt < brdlin_osdstart || brdlin_cnt > brdlin_osdend)
            brdptr16[brdcol_cnt ^ 1] = (uint16_t) brd;
        else if (brdcol_cnt < 84 || brdcol_cnt > 155)
            brdptr16[brdcol_cnt ^ 1] = (uint16_t) brd;

        lastBrdTstate++;

        brdcol_cnt++;

        if (brdcol_cnt == brdcol_end) {
            brdlin_cnt++;
            brdptr16 = (uint16_t *)(vga.frameBuffer[brdlin_cnt]);
            brdcol_cnt = 0;
            lastBrdTstate += brdnextline;
            if (brdlin_cnt == OSD::scrH) {
                DrawBorder = &Border_Blank;
                return;
            }
        }

    }

}    

