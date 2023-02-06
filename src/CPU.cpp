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

#include "CPU.h"
#include "ESPectrum.h"
#include "MemESP.h"
#include "Ports.h"
#include "hardconfig.h"
#include "Config.h"
#include "Tape.h"
#include "Video.h"
#include "Z80_JLS/z80.h"

#pragma GCC optimize ("O3")

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

uint32_t CPU::tstates = 0;
uint64_t CPU::global_tstates = 0;
uint32_t CPU::statesInFrame = 0;
uint32_t CPU::framecnt = 0;

void CPU::setup()
{
    if (!createCalled) {
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

	while (tstates < statesInFrame ) {

            uint32_t pre_tstates = tstates;

            Z80::execute();
        
            global_tstates += (tstates - pre_tstates); // increase global Tstates

	}

    // If we're halted flush screen and update registers as needed
    if (tstates & 0xFF000000) FlushOnHalt(); else tstates -= statesInFrame;

    interruptPending = true;

    framecnt++;

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

    // Draw the rest of the frame
    VIDEO::Flush();

    // End value
    tstates = global_tstates & 0x03;

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
        VIDEO::Draw(tstatestoadd);
    else
        CPU::tstates+=tstatestoadd;
}
