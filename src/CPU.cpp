///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
//
// CPU LOOP, MEMORY CONTENTION FUNCTIONS AND Z80OPS FUNCTIONS
//
// Copyright (c) 2023 Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
// https://github.com/EremusOne/ZX-ESPectrum-IDF
//
// Based on ZX-ESPectrum-Wiimote
// Copyright (c) 2020, 2021 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez and Jorge Fuertes
// https://github.com/rampa069/ZX-ESPectrum
//
// Original project by Pete Todd
// https://github.com/retrogubbins/paseVGA
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

uint32_t CPU::statesPerFrame()
{
    if (Config::getArch() == "48K") return 69888;
    else                            return 70908;
}

uint32_t CPU::microsPerFrame()
{
    if (Config::getArch() == "48K") return 19968;
    else                            return 19992;
}

uint32_t CPU::tstates = 0;
uint64_t CPU::global_tstates = 0;
uint32_t CPU::statesInFrame = 0;
uint32_t CPU::framecnt = 0;

bool Z80Ops::is48 = true;

void CPU::setup()
{
    if (!createCalled) {
        Z80::create();
        createCalled = true;
    }
    
    statesInFrame = CPU::statesPerFrame();

    if (Config::getArch() == "48K") {
        VIDEO::getFloatBusData = &VIDEO::getFloatBusData48;
        Z80Ops::is48 = true;
    } else {
        VIDEO::getFloatBusData = &VIDEO::getFloatBusData128;
        Z80Ops::is48 = false;
    }

    tstates = 0;
    global_tstates = 0;

}

///////////////////////////////////////////////////////////////////////////////

void CPU::reset() {

    Z80::reset();
    
    statesInFrame = CPU::statesPerFrame();

    if (Config::getArch() == "48K") {
        VIDEO::getFloatBusData = &VIDEO::getFloatBusData48;
        Z80Ops::is48 = true;
    } else {
        VIDEO::getFloatBusData = &VIDEO::getFloatBusData128;
        Z80Ops::is48 = false;
    }

    tstates = 0;
    global_tstates = 0;

}

///////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR CPU::loop()
{

    while (tstates < statesInFrame ) {

            uint32_t pre_tstates = tstates;

            Z80::execute();

            global_tstates += (tstates - pre_tstates); // increase global Tstates

            //
            // PRELIMINARY TAPE SAVE TEST
            //            
            // // if PC is 0x970, a call to SA_CONTRL has been made:
            // // remove .tap output file if exists
            // if(Z80::getRegPC() == 0x970) {
            //     remove("/sd/tap/cinta1.tap");
			// }
            // // if PC is 0x04C2, a call to SA_BYTES has been made:
            // // Call Save function
            // if(Z80::getRegPC() == 0x04C2) {
            //     Tape::Save();
            //     // printf("Save in ROM called\n");
            //     Z80::setRegPC(0x555);
			// }

	}

    // If we're halted flush screen and update registers as needed
    if (tstates & 0xFF000000) FlushOnHalt(); else tstates -= statesInFrame;

    framecnt++;

}

void CPU::FlushOnHalt() {
        
    tstates &= 0x00FFFFFF;
    global_tstates &= 0x00FFFFFF;

    uint32_t pre_tstates = tstates;        

    VIDEO::Flush(); // Draw the rest of the frame

    tstates = pre_tstates;

    uint8_t page = (Z80::getRegPC() + 1) >> 14;
    if ((page == 1) || ((page == 3) && (!Z80Ops::is48) && (MemESP::bankLatch & 0x01))) {

        // printf("Contend on flushonhalt!\n");

        if (Z80Ops::is48) {

            while (tstates < statesInFrame ) {
                uint32_t currentTstates = CPU::tstates + 1;                
                unsigned short int line = currentTstates / 224;
                if (line >= 64 && line < 256) tstates += wait_st[currentTstates % 224];
                tstates += 4;
                Z80::incRegR(1);
            }

        } else {

            while (tstates < statesInFrame ) {
                uint32_t currentTstates = CPU::tstates + 3;
                unsigned short int line = currentTstates / 228;
                if (line >= 63 && line < 255) tstates += wait_st[currentTstates % 228];
                tstates += 4;
                Z80::incRegR(1);
            }

        }

    } else {

        // tstates
        uint32_t incr = (statesInFrame - pre_tstates) >> 2;
        if (tstates & 0x03) incr++;
        tstates += (incr << 2);

        // RegR
        Z80::incRegR(incr & 0x000000FF);

    }

    Z80::checkINT(); // I think I can put this out of the "while (tstates .. ". Study

    global_tstates += (tstates - pre_tstates); // increase global Tstates
        
    tstates -= statesInFrame;

}

// static const unsigned char wait_states[8] = { 6, 5, 4, 3, 2, 1, 0, 0 }; // sequence of wait states

// //unsigned char (*Z80Ops::delayContention)(unsigned int currentTstates);
// unsigned char (*Z80Ops::delayContention)();

// // unsigned char IRAM_ATTR Z80Ops::delayContention48(unsigned int currentTstates) {
// unsigned char IRAM_ATTR Z80Ops::delayContention48() {
    
//     // // delay states one t-state BEFORE the first pixel to be drawn
//     // currentTstates++;

//     // delay states one t-state BEFORE the first pixel to be drawn
//     uint32_t currentTstates = CPU::tstates + 1;

// 	// each line spans 224 t-states
// 	unsigned short int line = currentTstates / 224; // int line

//     // only the 192 lines between 64 and 255 have graphic data, the rest is border
// 	if (line < 64 || line >= 256) return 0;

// 	// only the first 128 t-states of each line correspond to a graphic data transfer
// 	// the remaining 96 t-states correspond to border
// 	unsigned char halfpix = currentTstates % 224; // int halfpix

// 	// if (halfpix >= 128) return 0;

//     // return(wait_states[halfpix & 0x07]);

//     return(wait_st[halfpix]);

// }

// //unsigned char IRAM_ATTR Z80Ops::delayContention128(unsigned int currentTstates) {
// unsigned char IRAM_ATTR Z80Ops::delayContention128() {    

//     // currentTstates+=3;

//     uint32_t currentTstates = CPU::tstates + 3;

//     unsigned short int line = currentTstates / 228; // int line
// 	if (line < 63 || line >= 255) return 0;

// 	unsigned char halfpix = currentTstates % 228; // int halfpix
// 	// if (halfpix >= 128) return 0;

//     // return(wait_states[halfpix & 0x07]);

//     return(wait_st[halfpix]);

// }

///////////////////////////////////////////////////////////////////////////////
// Z80Ops
///////////////////////////////////////////////////////////////////////////////
// /* Read opcode from RAM */
// uint8_t IRAM_ATTR Z80Ops::fetchOpcode(uint16_t address) {

//     // uint8_t page = address >> 14;
//     // switch (page) {
//     // case 0:
//     //     VIDEO::Draw(4);
//     //     return MemESP::rom[MemESP::romInUse][address & 0x3fff];
//     // case 1:
//     //     VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 4);
//     //     return MemESP::ram5[address & 0x3fff];
//     // case 2:
//     //     VIDEO::Draw(4);
//     //     return MemESP::ram2[address & 0x3fff];
//     // case 3:
//     //     if (is48)
//     //         VIDEO::Draw(4);
//     //     else
//     //         if (MemESP::bankLatch & 0x01 == 1)
//     //             VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 4);
//     //         else
//     //             VIDEO::Draw(4);
//     //     return MemESP::ram[MemESP::bankLatch][address & 0x3fff];
//     // // default:
//     // //     VIDEO::Draw(4);
//     // //     return MemESP::rom[MemESP::romInUse][address];
//     // }

//     uint8_t page = address >> 14;

//     // if (MemESP::ramContended[page])
//     //     VIDEO::Draw(Z80Ops::delayContention() + 4, false);
//     // else
//     //     VIDEO::Draw(4, false);

//     VIDEO::Draw(4, MemESP::ramContended[page]);

//     return MemESP::ramCurrent[page][address & 0x3fff];        


// }

/* Read byte from RAM */
uint8_t IRAM_ATTR Z80Ops::peek8(uint16_t address) {

    // uint8_t page = address >> 14;
    // switch (page) {
    // case 0:
    //     VIDEO::Draw(3);
    //     return MemESP::rom[MemESP::romInUse][address & 0x3fff];
    // case 1:
    //     VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
    //     return MemESP::ram5[address & 0x3fff];
    // case 2:
    //     VIDEO::Draw(3);
    //     return MemESP::ram2[address & 0x3fff];
    // case 3:
    //     if (is48)
    //         VIDEO::Draw(3);
    //     else
    //         if (MemESP::bankLatch & 0x01 == 1)
    //             VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
    //         else
    //             VIDEO::Draw(3);
    //     return MemESP::ram[MemESP::bankLatch][address & 0x3fff];
    // // default:
    // //     VIDEO::Draw(3);
    // //     return MemESP::rom[MemESP::romInUse][address & 0x3fff];
    // }

    uint8_t page = address >> 14;

    // if (MemESP::ramContended[page])
    //     VIDEO::Draw(Z80Ops::delayContention() + 3, false);
    // else
    //     VIDEO::Draw(3, false);

    VIDEO::Draw(3,MemESP::ramContended[page]);

    return MemESP::ramCurrent[page][address & 0x3fff];        

}

/* Write byte to RAM */
void IRAM_ATTR Z80Ops::poke8(uint16_t address, uint8_t value) {

    // uint8_t page = address >> 14;
    // switch (page) {
    // case 0:
    //     VIDEO::Draw(3);        
    //     return;
    // case 1:
    //     VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
    //     MemESP::ram5[address - 0x4000] = value;
    //     return;
    // case 2:
    //     VIDEO::Draw(3);        
    //     MemESP::ram2[address - 0x8000] = value;
    //     return;
    // case 3:
    //     if (is48)
    //         VIDEO::Draw(3);
    //     else
    //         if (MemESP::bankLatch & 0x01 == 1)
    //             VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
    //         else
    //             VIDEO::Draw(3);
    //     MemESP::ram[MemESP::bankLatch][address - 0xC000] = value;
    //     return;
    // }

    uint8_t page = address >> 14;

    // if (page == 0) { VIDEO::Draw(3, false); return; }    
   
    // if (MemESP::ramContended[page])
    //     VIDEO::Draw(Z80Ops::delayContention() + 3, false);
    // else
    //     VIDEO::Draw(3, false);

    VIDEO::Draw(3, MemESP::ramContended[page]);
    
    if (page != 0)
        MemESP::ramCurrent[page][address & 0x3fff] = value;
    
    return;

}

/* Read/Write word from/to RAM */
uint16_t IRAM_ATTR Z80Ops::peek16(uint16_t address) {

    // Check if address is between two different pages
    // if ((address >> 14) == ((address + 1) >> 14)) {
 
        // uint8_t page = address >> 14;
        // switch (page) {
        // case 0:
        //     VIDEO::Draw(6);
        //     return ((MemESP::rom[MemESP::romInUse][address + 1] << 8) | MemESP::rom[MemESP::romInUse][address]);
        // case 1:
        //     VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
        //     VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
        //     return ((MemESP::ram5[address - 0x3FFF] << 8) | MemESP::ram5[address - 0x4000]);
        // case 2:
        //     VIDEO::Draw(6);
        //     return ((MemESP::ram2[address - 0x7FFF] << 8) | MemESP::ram2[address - 0x8000]);
        // case 3:
        //     if (is48)
        //         VIDEO::Draw(6);
        //     else
        //         if (MemESP::bankLatch & 0x01 == 1) {
        //             VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
        //             VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
        //         } else
        //             VIDEO::Draw(6);
        //     return ((MemESP::ram[MemESP::bankLatch][address - 0xBFFF] << 8) | MemESP::ram[MemESP::bankLatch][address - 0xC000]);
        // default:
        //     VIDEO::Draw(6);
        //     return ((MemESP::rom[MemESP::romInUse][address + 1] << 8) | MemESP::rom[MemESP::romInUse][address]);
        // }

        uint8_t page = address >> 14;

        // if (MemESP::ramContended[page]) {
        //     VIDEO::Draw(Z80Ops::delayContention() + 3, false);
        //     VIDEO::Draw(Z80Ops::delayContention() + 3, false);            
        // } else
        //     VIDEO::Draw(6, false);

        if (MemESP::ramContended[page]) {
            VIDEO::Draw(3, true);
            VIDEO::Draw(3, true);            
        } else
            VIDEO::Draw(6, false);

        return ((MemESP::ramCurrent[page][(address & 0x3fff) + 1] << 8) | MemESP::ramCurrent[page][address & 0x3fff]);

    // } else {

    //     // Order matters, first read lsb, then read msb, don't "optimize"
    //     uint8_t lsb = Z80Ops::peek8(address);
    //     uint8_t msb = Z80Ops::peek8(address + 1);
    //     return (msb << 8) | lsb;

    //     printf("Interpaged Peek16!\n");

    // }

}

void IRAM_ATTR Z80Ops::poke16(uint16_t address, RegisterPair word) {

    // Check if address is between two different pages
    // if ((address >> 14) == ((address + 1) >> 14)) {

        // uint8_t page = address >> 14;
        // switch (page) {
        // case 0:
        //     VIDEO::Draw(6);        
        //     return;
        // case 1:
        //     VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
        //     VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
        //     MemESP::ram5[address - 0x4000] = word.byte8.lo;
        //     MemESP::ram5[address - 0x3fff] = word.byte8.hi;
        //     return;
        // case 2:
        //     VIDEO::Draw(6);        
        //     MemESP::ram2[address - 0x8000] = word.byte8.lo;
        //     MemESP::ram2[address - 0x7fff] = word.byte8.hi;
        //     return;
        // case 3:
        //     if (is48)
        //         VIDEO::Draw(6);
        //     else
        //         if (MemESP::bankLatch & 0x01 == 1) {
        //             VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
        //             VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
        //         } else
        //             VIDEO::Draw(6);
        //     MemESP::ram[MemESP::bankLatch][address - 0xC000] = word.byte8.lo;
        //     MemESP::ram[MemESP::bankLatch][address - 0xbfff] = word.byte8.hi;
        //     return;
        // }

        uint8_t page = address >> 14;
        
        // if (page == 0) { VIDEO::Draw(6, false); return; }
        
        // if (MemESP::ramContended[page]) {
        //     VIDEO::Draw(Z80Ops::delayContention() + 3, false);
        //     VIDEO::Draw(Z80Ops::delayContention() + 3, false);            
        // } else
        //     VIDEO::Draw(6, false);
      
        if (MemESP::ramContended[page]) {
            VIDEO::Draw(3, true);
            VIDEO::Draw(3, true);            
        } else
            VIDEO::Draw(6, false);

        if (page != 0) {
            MemESP::ramCurrent[page][address & 0x3fff] = word.byte8.lo;
            MemESP::ramCurrent[page][(address & 0x3fff) + 1] = word.byte8.hi;
        }

    // } else {
    //     // // Order matters, first write lsb, then write msb, don't "optimize"
    //     Z80Ops::poke8(address, word.byte8.lo);
    //     Z80Ops::poke8(address + 1, word.byte8.hi);

    //     printf("Interpaged Poke16!\n");

    // }

}

// /* In/Out byte from/to IO Bus */
// uint8_t IRAM_ATTR Z80Ops::inPort(uint16_t port) {
    
//     // uint8_t hiport = port >> 8;
//     // uint8_t loport = port & 0xFF;
//     // return Ports::input(loport, hiport);

//     return Ports::input(port);

// }

// void IRAM_ATTR Z80Ops::outPort(uint16_t port, uint8_t value) {
    
//     // uint8_t hiport = port >> 8;
//     // uint8_t loport = port & 0xFF;
//     // Ports::output(loport, hiport, value);

//     Ports::output(port, value);

// }

/* Put an address on bus lasting 'tstates' cycles */
void IRAM_ATTR Z80Ops::addressOnBus(uint16_t address, int32_t wstates){

    // uint8_t page = address >> 14;
    // switch (page) {
    // case 1:
    //     for (int idx = 0; idx < wstates; idx++)
    //         VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 1);  
    //     return;      
    // case 3:
    //     if (is48)
    //         VIDEO::Draw(wstates);
    //     else
    //         if (MemESP::bankLatch & 0x01 == 1) {
    //             for (int idx = 0; idx < wstates; idx++)
    //                 VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 1);        
    //         } else
    //             VIDEO::Draw(wstates);
    //     return;
    // default:
    //     VIDEO::Draw(wstates);        
    // }

    // if (MemESP::ramContended[address >> 14]) {
    //     for (int idx = 0; idx < wstates; idx++)
    //         VIDEO::Draw(Z80Ops::delayContention() + 1, false);  
    // } else {
    //     VIDEO::Draw(wstates, false);
    // }

    if (MemESP::ramContended[address >> 14]) {
        for (int idx = 0; idx < wstates; idx++)
            VIDEO::Draw(1, true);  
    } else {
        VIDEO::Draw(wstates, false);
    }

}

// /* Clocks needed for processing INT and NMI */
// void IRAM_ATTR Z80Ops::interruptHandlingTime(int32_t wstates) {
//     VIDEO::Draw(wstates, false);
// }

// /* Signal HALT in tstates */
// void IRAM_ATTR Z80Ops::signalHalt() {
//     CPU::tstates |= 0xFF000000;
// }

/* Callback to know when the INT signal is active */
bool IRAM_ATTR Z80Ops::isActiveINT(void) {
    int tmp = CPU::tstates;
    if (tmp >= CPU::statesInFrame) tmp -= CPU::statesInFrame;
    return ((tmp >= 0) && (tmp < (is48 ? 32 : 36)));
}
