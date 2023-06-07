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

            Z80::execute();

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

    if (tstates & 0xFF000000) FlushOnHalt(); // If we're halted flush screen and update registers as needed

    global_tstates += statesInFrame; // increase global Tstates
    tstates -= statesInFrame;

    framecnt++;

}

void CPU::FlushOnHalt() {
        
    tstates &= 0x00FFFFFF;

    uint32_t pre_tstates = tstates;        

    VIDEO::Flush(); // Draw the rest of the frame

    tstates = pre_tstates;

    uint8_t page = (Z80::getRegPC() + 1) >> 14;
    if ((page == 1) || ((page == 3) && (!Z80Ops::is48) && (MemESP::bankLatch & 0x01))) {

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

}

///////////////////////////////////////////////////////////////////////////////
// Z80Ops
///////////////////////////////////////////////////////////////////////////////

/* Read byte from RAM */
uint8_t IRAM_ATTR Z80Ops::peek8(uint16_t address) {
    uint8_t page = address >> 14;
    VIDEO::Draw(3,MemESP::ramContended[page]);
    return MemESP::ramCurrent[page][address & 0x3fff];        
}

/* Write byte to RAM */
void IRAM_ATTR Z80Ops::poke8(uint16_t address, uint8_t value) {
    uint8_t page = address >> 14;
    VIDEO::Draw(3, MemESP::ramContended[page]);
    if (page != 0) MemESP::ramCurrent[page][address & 0x3fff] = value;
    return;
}

/* Read/Write word from/to RAM */
uint16_t IRAM_ATTR Z80Ops::peek16(uint16_t address) {

    uint8_t page = address >> 14;

    if (MemESP::ramContended[page]) {
        VIDEO::Draw(3, true);
        VIDEO::Draw(3, true);            
    } else
        VIDEO::Draw(6, false);

    return ((MemESP::ramCurrent[page][(address & 0x3fff) + 1] << 8) | MemESP::ramCurrent[page][address & 0x3fff]);

}

void IRAM_ATTR Z80Ops::poke16(uint16_t address, RegisterPair word) {

    uint8_t page = address >> 14;
    
    if (MemESP::ramContended[page]) {
        VIDEO::Draw(3, true);
        VIDEO::Draw(3, true);            
    } else
        VIDEO::Draw(6, false);

    if (page != 0) {
        MemESP::ramCurrent[page][address & 0x3fff] = word.byte8.lo;
        MemESP::ramCurrent[page][(address & 0x3fff) + 1] = word.byte8.hi;
    }

}

/* Put an address on bus lasting 'tstates' cycles */
void IRAM_ATTR Z80Ops::addressOnBus(uint16_t address, int32_t wstates){

    if (MemESP::ramContended[address >> 14]) {
        for (int idx = 0; idx < wstates; idx++)
            VIDEO::Draw(1, true);  
    } else {
        VIDEO::Draw(wstates, false);
    }

}

/* Callback to know when the INT signal is active */
bool IRAM_ATTR Z80Ops::isActiveINT(void) {

    int tmp = CPU::tstates;
    if (tmp >= CPU::statesInFrame) tmp -= CPU::statesInFrame;
    return ((tmp >= 0) && (tmp < (is48 ? 32 : 36)));

}
