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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "ESPectrum.h"
#include "MemESP.h"
#include "Ports.h"
#include "hardconfig.h"
#include "hardware.h"
#include "hardpins.h"
#include "CPU.h"
#include "Config.h"
#include "OSDMain.h"
#include "Tape.h"

#pragma GCC optimize ("O3")

///////////////////////////////////////////////////////////////////////////////

#include "Z80_JLS/z80.h"
static bool createCalled = false;
static bool interruptPending = false;

///////////////////////////////////////////////////////////////////////////////

// machine tstates  f[MHz]   micros
//   48K:   69888 / 3.5    = 19968
//  128K:   70908 / 3.5469 = 19992

uint32_t CPU::statesPerFrame()
{
    if (Config::getArch() == "48K") return 69888; // 69888 is the right value.
    else                            return 70912; // 70908 is the right value. Added 4 states to make it divisible by 128 (audio issues)
}

uint32_t CPU::microsPerFrame()
{
    if (Config::getArch() == "48K") return 19968;
    else                            return 19992;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef LOG_DEBUG_TIMING
uint32_t CPU::framecnt = 0;
#endif

uint32_t CPU::tstates = 0;
uint64_t CPU::global_tstates = 0;
uint32_t CPU::statesInFrame = 0;

void CPU::setup()
{
    if (!createCalled)
    {
        Z80::create();
        createCalled = true;
    }

    statesInFrame = CPU::statesPerFrame();

    ESPectrum::reset();
}

///////////////////////////////////////////////////////////////////////////////

void CPU::reset() {

    Z80::reset();

    statesInFrame = CPU::statesPerFrame();

    global_tstates = 0;

}

///////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR CPU::loop()
{

    tstates = 0;

	while (tstates < statesInFrame )
	{
            // frame Tstates before instruction
            uint32_t pre_tstates = tstates;

            #ifdef TAPE_TRAPS
                switch (Z80::getRegPC()) {
                case 0x0556: // START LOAD        
                    Tape::romLoading=true;
                    if (Tape::tapeStatus!=TAPE_LOADING && Tape::tapeFileName!="none") Tape::TAP_Play();
                    // Serial.printf("------\n");
                    // Serial.printf("TapeStatus: %u\n", Tape::tapeStatus);
                    break;
                case 0x04d0: // START SAVE (used for rerouting mic out to speaker in Ports.cpp)
                    Tape::SaveStatus=TAPE_SAVING; 
                    // Serial.printf("------\n");
                    // Serial.printf("SaveStatus: %u\n", Tape::SaveStatus);
                    break;
                case 0x053f: // END LOAD / SAVE
                    Tape::romLoading=false;
                    if (Tape::tapeStatus!=TAPE_STOPPED)
                        if (Z80::isCarryFlag()) Tape::tapeStatus=TAPE_PAUSED; else Tape::tapeStatus=TAPE_STOPPED;
                    Tape::SaveStatus=SAVE_STOPPED;
                    /*
                    Serial.printf("TapeStatus: %u\n", Tape::tapeStatus);
                    Serial.printf("SaveStatus: %u\n", Tape::SaveStatus);
                    Serial.printf("Carry Flag: %u\n", Z80::isCarryFlag());            
                    Serial.printf("------\n");
                    */
                    break;
                }
            #endif

            Z80::execute();
        
            // increase global Tstates
            global_tstates += (tstates - pre_tstates);

	}

    // We're halted: flush screen and update registers as needed
    if (tstates & 0xFF000000) FlushOnHalt();

    #ifdef LOG_DEBUG_TIMING
    framecnt++;
    #endif

    CPU::vga.show();

    interruptPending = true;
    
    // Flashing flag change
    if (!(flash_ctr & 0x0f)) flashing ^= 0b10000000;
    flash_ctr++;

}

void IRAM_ATTR CPU::FlushOnHalt() {
        
        tstates &= 0x00FFFFFF;
        global_tstates &= 0x00FFFFFF;

        uint32_t calcRegR = Z80::getRegR();

        bool LowR = ADDRESS_IN_LOW_RAM(Z80::getRegPC());
        if (LowR) {
            uint32_t tIncr = tstates;
            while (tIncr < statesInFrame) {
                tIncr += delayContention(tIncr) + 4;
                calcRegR++;
            }
            global_tstates += (tIncr - tstates);
            Z80::setRegR(calcRegR & 0x000000FF);
        } else {        
            global_tstates += (statesInFrame - tstates) + (tstates & 0x03);
            uint32_t calcRegR = Z80::getRegR() + ((statesInFrame - tstates) >> 2);
            Z80::setRegR(calcRegR & 0x000000FF);
        }

        // TO DO: Write special endframe flush function
        while (tstates < TS_PHASE_4_320x240) { // Bigger phase 4 of two aspects ratio
            // ALU_draw(4);
            ALU_draw(ESPectrum::ESPoffset);
        }

}

///////////////////////////////////////////////////////////////////////////////
//
// Delay Contention: for emulating CPU slowing due to sharing bus with ULA
// NOTE: Only 48K spectrum contention implemented. This function must be called
// only when dealing with affected memory (use ADDRESS_IN_LOW_RAM macro)
//
// delay contention: emulates wait states introduced by the ULA (graphic chip)
// whenever there is a memory access to contended memory (shared between ULA and CPU).
// detailed info: https://worldofspectrum.org/faq/reference/48kreference.htm#ZXSpectrum
// from paragraph which starts with "The 50 Hz interrupt is synchronized with..."
// if you only read from https://worldofspectrum.org/faq/reference/48kreference.htm#Contention
// without reading the previous paragraphs about line timings, it may be confusing.
//

static unsigned char wait_states[8] = { 6, 5, 4, 3, 2, 1, 0, 0 }; // sequence of wait states
static unsigned char IRAM_ATTR delayContention(unsigned int currentTstates) {
    
    // delay states one t-state BEFORE the first pixel to be drawn
    currentTstates++;

	// each line spans 224 t-states
	unsigned short int line = currentTstates / 224; // int line

    // only the 192 lines between 64 and 255 have graphic data, the rest is border
	if (line < 64 || line >= 256) return 0;

	// only the first 128 t-states of each line correspond to a graphic data transfer
	// the remaining 96 t-states correspond to border
	unsigned char halfpix = currentTstates % 224; // int halfpix

	if (halfpix >= 128) return 0;

    return(wait_states[halfpix & 0x07]);

}

///////////////////////////////////////////////////////////////////////////////
// Port Contention
///////////////////////////////////////////////////////////////////////////////
static void ALUContentEarly( uint16_t port )
{
    if ( ( port & 49152 ) == 16384 )
        Z80Ops::addTstates(delayContention(CPU::tstates) + 1,true);
    else
        Z80Ops::addTstates(1,true);
}

static void ALUContentLate( uint16_t port )
{

  if( (port & 0x0001) == 0x00) {
        Z80Ops::addTstates(delayContention(CPU::tstates) + 3,true);
  } else {
    if ( (port & 49152) == 16384 ) {
        Z80Ops::addTstates(delayContention(CPU::tstates) + 1,true);
        Z80Ops::addTstates(delayContention(CPU::tstates) + 1,true);
        Z80Ops::addTstates(delayContention(CPU::tstates) + 1,true);
	} else {
        Z80Ops::addTstates(3,true);
	}

  }

}

///////////////////////////////////////////////////////////////////////////////
// Z80Ops
///////////////////////////////////////////////////////////////////////////////
/* Read opcode from RAM */
uint8_t IRAM_ATTR Z80Ops::fetchOpcode(uint16_t address) {
    // 3 clocks to fetch opcode from RAM and 1 execution clock
    if (ADDRESS_IN_LOW_RAM(address))
        addTstates(delayContention(CPU::tstates) + 4,true);
    else
        addTstates(4,true);

    return MemESP::readbyte(address);
}

/* Read byte from RAM */
uint8_t IRAM_ATTR Z80Ops::peek8(uint16_t address) {
    // 3 clocks for read byte from RAM
    if (ADDRESS_IN_LOW_RAM(address))
        addTstates(delayContention(CPU::tstates) + 3,true);
    else
        addTstates(3,true);

    return MemESP::readbyte(address);
}

/* Write byte to RAM */
void IRAM_ATTR Z80Ops::poke8(uint16_t address, uint8_t value) {
    if (ADDRESS_IN_LOW_RAM(address)) {
        addTstates(delayContention(CPU::tstates) + 3, true);
    } else 
        addTstates(3,true);

    MemESP::writebyte(address, value);
}

/* Read/Write word from/to RAM */
uint16_t IRAM_ATTR Z80Ops::peek16(uint16_t address) {

    // Order matters, first read lsb, then read msb, don't "optimize"
    uint8_t lsb = Z80Ops::peek8(address);
    uint8_t msb = Z80Ops::peek8(address + 1);

    return (msb << 8) | lsb;
}

void IRAM_ATTR Z80Ops::poke16(uint16_t address, RegisterPair word) {
    // Order matters, first write lsb, then write msb, don't "optimize"
    Z80Ops::poke8(address, word.byte8.lo);
    Z80Ops::poke8(address + 1, word.byte8.hi);
}

/* In/Out byte from/to IO Bus */
uint8_t IRAM_ATTR Z80Ops::inPort(uint16_t port) {
    
    ALUContentEarly( port );   // Contended I/O
    ALUContentLate( port );    // Contended I/O

    uint8_t hiport = port >> 8;
    uint8_t loport = port & 0xFF;
    return Ports::input(loport, hiport);

}

void IRAM_ATTR Z80Ops::outPort(uint16_t port, uint8_t value) {
    
    ALUContentEarly( port );   // Contended I/O

    uint8_t hiport = port >> 8;
    uint8_t loport = port & 0xFF;
    Ports::output(loport, hiport, value);

    ALUContentLate( port );    // Contended I/O

}

/* Put an address on bus lasting 'tstates' cycles */
void IRAM_ATTR Z80Ops::addressOnBus(uint16_t address, int32_t wstates){

    // Additional clocks to be added on some instructions
    if (ADDRESS_IN_LOW_RAM(address))
        for (int idx = 0; idx < wstates; idx++)
            addTstates(delayContention(CPU::tstates) + 1,true);
    else
        addTstates(wstates,true); // I've changed this to CPU::tstates direct increment. All seems working OK. Investigate.

}

/* Clocks needed for processing INT and NMI */
void IRAM_ATTR Z80Ops::interruptHandlingTime(int32_t wstates) {
    addTstates(wstates,true);
}

/* Signal HALT in tstates */
void IRAM_ATTR Z80Ops::signalHalt() {
    CPU::tstates |= 0xFF000000;
}

/* Callback to know when the INT signal is active */
bool IRAM_ATTR Z80Ops::isActiveINT(void) {
    if (!interruptPending) return false;
    interruptPending = false;
    return true;
}

void IRAM_ATTR Z80Ops::addTstates(int32_t tstatestoadd, bool dovideo) {
    if (dovideo)
        ALU_draw(tstatestoadd);
    else
        CPU::tstates+=tstatestoadd;
}

///////////////////////////////////////////////////////////////////////////////
//  VIDEO EMULATION
///////////////////////////////////////////////////////////////////////////////
VGA6Bit CPU::vga;

uint8_t CPU::borderColor = 0;

void precalcColors() {

    uint16_t specfast_colors[128]; // Array for faster color calc in ALU_video

    unsigned int pal[2],b0,b1,b2,b3;

    for (int i = 0; i < NUM_SPECTRUM_COLORS; i++) {
        spectrum_colors[i] = (spectrum_colors[i] & CPU::vga.RGBAXMask) | CPU::vga.SBits;
    }

    // Calc array for faster color calcs in ALU_video
    for (int i = 0; i < (NUM_SPECTRUM_COLORS >> 1); i++) {
        // Normal
        specfast_colors[i] = spectrum_colors[i];
        specfast_colors[i << 3] = spectrum_colors[i];
        // Bright
        specfast_colors[i | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
        specfast_colors[(i << 3) | 0x40] = spectrum_colors[i + (NUM_SPECTRUM_COLORS >> 1)];
    }

    // Calc ULA bytes
    for (int i = 0; i < 16; i++) {
        for (int n = 0; n < 256; n++) {
            pal[0] = specfast_colors[n & 0x78];
            pal[1] = specfast_colors[n & 0x47];
            b0 = pal[(i >> 3) & 0x01];
            b1 = pal[(i >> 2) & 0x01];
            b2 = pal[(i >> 1) & 0x01];
            b3 = pal[i & 0x01];
            ulabytes[i][n]=b2 | (b3<<8) | (b0<<16) | (b1<<24);
        }
    }    

}

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

// Precalc border 32 bits values
static unsigned int border32[8];
void precalcborder32()
{
    for (int i = 0; i < 8; i++) {
        uint8_t border = zxColor(i,0);
        border32[i] = border | (border << 8) | (border << 16) | (border << 24);
    }
}

void ALU_video_init() {

    const Mode& vgaMode = Config::aspect_16_9 ? CPU::vga.MODE360x200 : CPU::vga.MODE320x240;
    OSD::scrW = vgaMode.hRes;
    OSD::scrH = vgaMode.vRes / vgaMode.vDiv;
    
    const int redPins[] = {RED_PINS_6B};
    const int grePins[] = {GRE_PINS_6B};
    const int bluPins[] = {BLU_PINS_6B};
    CPU::vga.init(vgaMode, redPins, grePins, bluPins, HSYNC_PIN, VSYNC_PIN);

    precalcColors();    // precalculate colors for current VGA mode

    precalcULASWAP();   // precalculate ULA SWAP values

    precalcborder32();  // Precalc border 32 bits values

    for (int i=0;i<312;i++) {
        lastBorder[i]=8; // 8 -> Force repaint of border
    }

    CPU::borderColor = 0;

    is169 = Config::aspect_16_9 ? 1 : 0;

    DrawStatus = BLANK;

    // ALU_draw = is169 ? &ALU_video_169 : &ALU_video_fast_43;
    ALU_draw = is169 ? &ALU_video_169 : &ALU_video;

}

void ALU_video_reset() {

    for (int i=0;i<312;i++) {
        lastBorder[i]=8; // 8 -> Force repaint of border
    }

    CPU::borderColor = 7;

    is169 = Config::aspect_16_9 ? 1 : 0;

    DrawStatus = BLANK;

    // ALU_draw = is169 ? &ALU_video_169 : &ALU_video_fast_43;
    ALU_draw = is169 ? &ALU_video_169 : &ALU_video;

}

///////////////////////////////////////////////////////////////////////////////
//  VIDEO DRAW FUNCTION
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ALU_video_169 -> Fast Border 16:9
///////////////////////////////////////////////////////////////////////////////
static void IRAM_ATTR ALU_video_169(unsigned int statestoadd) {

uint8_t att, bmp;

    CPU::tstates += statestoadd;

#ifndef NO_VIDEO

    if (DrawStatus==TOPBORDER) {

        if (CPU::tstates > tstateDraw) {

            tstateDraw += TSTATES_PER_LINE;

            brd = CPU::borderColor;
            if (lastBorder[linedraw_cnt] != brd) {

                lineptr32 = (uint32_t *)(CPU::vga.backBuffer[linedraw_cnt]);                

                for (int i=0; i < 90 ; i++) {
                    *lineptr32++ = border32[brd];
                }

                lastBorder[linedraw_cnt]=brd;
            }

            linedraw_cnt++;

            if (linedraw_cnt == 4) {
                DrawStatus = LEFTBORDER;
                tstateDraw = TS_PHASE_2_360x200;
            }

        }

        return;

    }

    if (DrawStatus==LEFTBORDER) {
     
        if (CPU::tstates > tstateDraw) {

            lineptr32 = (uint32_t *)(CPU::vga.backBuffer[linedraw_cnt]);                

            brd = CPU::borderColor;
            if (lastBorder[linedraw_cnt] != brd) {
                for (int i=0; i < 13; i++) {    
                    *lineptr32++ = border32[brd];
                }
            } else lineptr32 += 13;

            tstateDraw += 24;

            DrawStatus = LINEDRAW_SYNC;

        }
        
        return;

    }    

    if (DrawStatus == LINEDRAW_SYNC) {

        if (CPU::tstates > tstateDraw) {

            statestoadd = CPU::tstates - tstateDraw;

            bmpOffset = offBmp[mainscrline_cnt];
            attOffset = offAtt[mainscrline_cnt];

            grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;

            coldraw_cnt = 0;
            ALU_video_rest = 0;
            DrawStatus = LINEDRAW;

        } else return;

    }

    if (DrawStatus == LINEDRAW) {
   
        statestoadd += ALU_video_rest;
        ALU_video_rest = statestoadd & 0x03; // Mod 4

        for (int i=0; i < (statestoadd >> 2); i++) {    

            att = grmem[attOffset];  // get attribute byte

            if (att & flashing) {
                bmp = ~grmem[bmpOffset];  // get inverted bitmap byte
            } else 
                bmp = grmem[bmpOffset];   // get bitmap byte

            *lineptr32++ = ulabytes[bmp >> 4][att];
            *lineptr32++ = ulabytes[bmp & 0xF][att];

            attOffset++;
            bmpOffset++;

            coldraw_cnt++;

            if (coldraw_cnt == 32) {
                DrawStatus = RIGHTBORDER;
                return;
            }

        }

        return;

    }

    if (DrawStatus==RIGHTBORDER) {
     
        if (lastBorder[linedraw_cnt] != brd) {
            for (int i=0; i < 13; i++) {
                *lineptr32++ = border32[brd];
            }
            lastBorder[linedraw_cnt]=brd;
        }

        mainscrline_cnt++;
        linedraw_cnt++;

        if (mainscrline_cnt < 192) {
            DrawStatus = LEFTBORDER;
            tstateDraw += 200;
        } else {
            mainscrline_cnt = 0;
            DrawStatus = BOTTOMBORDER;
            tstateDraw = TS_PHASE_3_360x200;
        }

        return;

    }    

    if (DrawStatus==BOTTOMBORDER) {

        if (CPU::tstates > tstateDraw) {

            tstateDraw += TSTATES_PER_LINE;

            brd = CPU::borderColor;
            if (lastBorder[linedraw_cnt] != brd) {

                lineptr32 = (uint32_t *)(CPU::vga.backBuffer[linedraw_cnt]);

                for (int i=0; i < 90; i++) {    
                    *lineptr32++ = border32[brd];
                }

                lastBorder[linedraw_cnt]=brd;            
            }

            linedraw_cnt++;
            if (linedraw_cnt == 200) {
                linedraw_cnt=0;
                DrawStatus = BLANK;
            }

        }

        return;

    }

    if (DrawStatus==BLANK)
        if (CPU::tstates < TSTATES_PER_LINE) {
                DrawStatus = TOPBORDER;
                linedraw_cnt=0;
                tstateDraw = TS_PHASE_1_360x200;
        }

#endif

}

///////////////////////////////////////////////////////////////////////////////
// ALU_video -> Multicolour and Border effects support
// 4:3 (320x240)
///////////////////////////////////////////////////////////////////////////////
static void IRAM_ATTR ALU_video(unsigned int statestoadd) {

uint8_t att, bmp;

CPU::tstates += statestoadd;

#ifndef NO_VIDEO

if (DrawStatus==TOPBORDER_BLANK) {
    if (CPU::tstates > tstateDraw) {
        statestoadd = CPU::tstates - tstateDraw;
        tstateDraw += TSTATES_PER_LINE;
        lineptr32 = (uint32_t *)(CPU::vga.backBuffer[linedraw_cnt]);
        coldraw_cnt = 0;
        DrawStatus = TOPBORDER;
        ALU_video_rest = 0;
    } else return;
}

if (DrawStatus==TOPBORDER) {
    statestoadd += ALU_video_rest;
    ALU_video_rest = statestoadd & 0x03; // Mod 4
    brd = border32[CPU::borderColor];
    for (int i=0; i < (statestoadd >> 2); i++) {
        *lineptr32++ = brd;
        *lineptr32++ = brd;
        if (++coldraw_cnt == 40) {
            DrawStatus = TOPBORDER_BLANK;
            if (++linedraw_cnt == 24) {
                DrawStatus = MAINSCREEN_BLANK;
                tstateDraw=TS_PHASE_2_320x240;
            }
            return;
        }
    }
    return;
}

if (DrawStatus==MAINSCREEN_BLANK) {
    if (CPU::tstates > tstateDraw) {
        statestoadd = CPU::tstates - tstateDraw;
        tstateDraw += TSTATES_PER_LINE;
        lineptr32 = (uint32_t *)(CPU::vga.backBuffer[linedraw_cnt]);
        coldraw_cnt = 0;
        ALU_video_rest = 0;
        DrawStatus = LEFTBORDER;
    } else return;
}    

if (DrawStatus==LEFTBORDER) {
    statestoadd += ALU_video_rest;
    ALU_video_rest = statestoadd & 0x03; // Mod 4
    brd = border32[CPU::borderColor];
    for (int i=0; i < (statestoadd >> 2); i++) {    
        *lineptr32++ = brd;
        *lineptr32++ = brd;
        if (++coldraw_cnt == 4) {      
            DrawStatus = LINEDRAW;
            bmpOffset = offBmp[linedraw_cnt - 24];
            attOffset = offAtt[linedraw_cnt - 24];
            grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;
            ALU_video_rest += statestoadd - (i++ << 2);
            return;
        }
    }
    return;
}    

if (DrawStatus == LINEDRAW) {

    statestoadd += ALU_video_rest;
    ALU_video_rest = statestoadd & 0x03; // Mod 4

    for (int i=0; i < (statestoadd >> 2); i++) {    

        att = grmem[attOffset++];       // get attribute byte
        if (att & flashing) {
            bmp = ~grmem[bmpOffset++];  // get inverted bitmap byte
        } else 
            bmp = grmem[bmpOffset++];   // get bitmap byte

        *lineptr32++ = ulabytes[bmp >> 4][att];
        *lineptr32++ = ulabytes[bmp & 0xF][att];

        if (++coldraw_cnt == 36) {
            DrawStatus = RIGHTBORDER;
            ALU_video_rest += statestoadd - (i++ << 2);
            return;
        }

    }

    return;

}

if (DrawStatus==RIGHTBORDER) {
    statestoadd += ALU_video_rest;
    ALU_video_rest = statestoadd & 0x03; // Mod 4
    brd = border32[CPU::borderColor];
    for (int i=0; i < (statestoadd >> 2); i++) {
        *lineptr32++ = brd;
        *lineptr32++ = brd;
        if (++coldraw_cnt == 40 ) { 
            DrawStatus = MAINSCREEN_BLANK;
            if (++linedraw_cnt == 216 ) {
                DrawStatus = BOTTOMBORDER_BLANK;
                tstateDraw = TS_PHASE_3_320x240;
            }
            return;
        }
    }
    return;
}    

if (DrawStatus==BOTTOMBORDER_BLANK) {
    if (CPU::tstates > tstateDraw) {
        statestoadd = CPU::tstates - tstateDraw;
        tstateDraw += TSTATES_PER_LINE;
        lineptr32 = (uint32_t *)(CPU::vga.backBuffer[linedraw_cnt]);
        coldraw_cnt = 0;
        ALU_video_rest = 0;
        DrawStatus = BOTTOMBORDER;
    } else return;
}

if (DrawStatus==BOTTOMBORDER) {
    statestoadd += ALU_video_rest;
    ALU_video_rest = statestoadd & 0x03; // Mod 4
    brd = border32[CPU::borderColor];
    for (int i=0; i < (statestoadd >> 2); i++) {    
        *lineptr32++ = brd;
        *lineptr32++ = brd;
        if (++coldraw_cnt == 40) {
            if (++linedraw_cnt == 240) DrawStatus = BLANK; else DrawStatus = BOTTOMBORDER_BLANK;
            return;
        }
    }
    return;
}

if (DrawStatus==BLANK) {
    if (CPU::tstates < TSTATES_PER_LINE) {
        linedraw_cnt = 0;
        tstateDraw = TS_PHASE_1_320x240;
        DrawStatus = TOPBORDER_BLANK;
    }
}

#endif

}

///////////////////////////////////////////////////////////////////////////////
// ALU_video_fast_43 -> Fast draw (no multicolour, border effects support)
///////////////////////////////////////////////////////////////////////////////
static void IRAM_ATTR ALU_video_fast_43(unsigned int statestoadd) {

uint8_t att, bmp;

    CPU::tstates += statestoadd;

#ifndef NO_VIDEO

    if (DrawStatus==TOPBORDER) {
        if (CPU::tstates > tstateDraw) {
            
            // Has border changed?
            if (lastBorder[0] != CPU::borderColor) {

                // Draw border
                brd = border32[CPU::borderColor];
                for (int n=0; n < 24; n++) {
                    memset((unsigned char *)CPU::vga.backBuffer[n],brd,320);
                }
                for (int n=24; n < 216; n++) {
                    uint8_t *lineptr = (uint8_t *)(CPU::vga.backBuffer[n]);
                    memset(lineptr,brd,32);
                    memset(lineptr + 288,brd,32);
                }
                for (int n=216; n < 240; n++) {
                    memset((unsigned char *)CPU::vga.backBuffer[n],brd,320);
                }
                lastBorder[0]=CPU::borderColor;

            }
 
            // Draw mainscreen

            grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;

            for (int n=24; n < 216; n++) {

                lineptr32 = (uint32_t *)(CPU::vga.backBuffer[n]);
                lineptr32 += 8;

                bmpOffset = offBmp[n - 24];
                attOffset = offAtt[n - 24];

                for (int i=0; i < 32; i++) {
                        att = grmem[attOffset++];       // get attribute byte
                        if (att & flashing) {
                            bmp = ~grmem[bmpOffset++];  // get inverted bitmap byte
                        } else 
                            bmp = grmem[bmpOffset++];   // get bitmap byte
                        *lineptr32++ = ulabytes[bmp >> 4][att];
                        *lineptr32++ = ulabytes[bmp & 0xF][att];
                }

            }
 
            DrawStatus = BLANK;
        }
        return;
    }

    if (DrawStatus==BLANK)
        if (CPU::tstates < TSTATES_PER_LINE) {
                DrawStatus = TOPBORDER;
                tstateDraw = TS_PHASE_1_320x240;
        }

#endif

}

///////////////////////////////////////////////////////////////////////////////
// ALU_flush_video -> Flush screen after HALT
///////////////////////////////////////////////////////////////////////////////
static void IRAM_ATTR ALU_flush_video() {

uint8_t att, bmp;

#ifndef NO_VIDEO

// TO DO

#endif

}
