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

#include "cpuESP.h"
#include "ESPectrum.h"
#include "MemESP.h"
#include "Ports.h"
#include "hardconfig.h"
#include "Config.h"
#include "Video.h"
#include "Z80_JLS/z80.h"

// #pragma GCC optimize("O3")

uint32_t CPU::tstates = 0;
uint64_t CPU::global_tstates = 0;
uint32_t CPU::statesInFrame = 0;
uint8_t CPU::latetiming = 0;
uint8_t CPU::IntStart = 0;
uint8_t CPU::IntEnd = 0;
uint32_t CPU::stFrame = 0;

bool Z80Ops::is48;
bool Z80Ops::is128;
bool Z80Ops::isPentagon;

///////////////////////////////////////////////////////////////////////////////

void CPU::reset() {

    Z80::reset();
    
    CPU::latetiming = Config::AluTiming;

    if (Config::arch == "48K") {
        Ports::getFloatBusData = &Ports::getFloatBusData48;
        Z80Ops::is48 = true;
        Z80Ops::is128 = false;
        Z80Ops::isPentagon = false;
        statesInFrame = TSTATES_PER_FRAME_48;
        IntStart = INT_START48;
        IntEnd = INT_END48 + CPU::latetiming;
        ESPectrum::target[0] = MICROS_PER_FRAME_48;
        ESPectrum::target[1] = MICROS_PER_FRAME_48;
        ESPectrum::target[2] = MICROS_PER_FRAME_48_125SPEED;
        ESPectrum::target[3] = MICROS_PER_FRAME_48_150SPEED;                        

    } else if (Config::arch == "TK90X" || Config::arch == "TK95") {

        Z80Ops::is48 = true;
        Z80Ops::is128 = false;
        Z80Ops::isPentagon = false;

        switch (Config::ALUTK) {
        case 0:
            Ports::getFloatBusData = &Ports::getFloatBusData48;
            statesInFrame = TSTATES_PER_FRAME_48;
            ESPectrum::target[0] = MICROS_PER_FRAME_48;
            ESPectrum::target[1] = MICROS_PER_FRAME_48;
            ESPectrum::target[2] = MICROS_PER_FRAME_48_125SPEED;
            ESPectrum::target[3] = MICROS_PER_FRAME_48_150SPEED;                        
            break;
        case 1:
            Ports::getFloatBusData = &Ports::getFloatBusDataTK;
            statesInFrame = TSTATES_PER_FRAME_TK_50;
            ESPectrum::target[0] = MICROS_PER_FRAME_TK_50;
            ESPectrum::target[1] = MICROS_PER_FRAME_TK_50;
            ESPectrum::target[2] = MICROS_PER_FRAME_TK_50_125SPEED;
            ESPectrum::target[3] = MICROS_PER_FRAME_TK_50_150SPEED;                        
            break;
        case 2:
            Ports::getFloatBusData = &Ports::getFloatBusDataTK;
            statesInFrame = TSTATES_PER_FRAME_TK_60;
            ESPectrum::target[0] = MICROS_PER_FRAME_TK_60;
            ESPectrum::target[1] = MICROS_PER_FRAME_TK_60;
            ESPectrum::target[2] = MICROS_PER_FRAME_TK_60_125SPEED;
            ESPectrum::target[3] = MICROS_PER_FRAME_TK_60_150SPEED;
        }

        IntStart = INT_STARTTK;
        IntEnd = INT_ENDTK + CPU::latetiming;

    } else if (Config::arch == "128K") {
        Ports::getFloatBusData = &Ports::getFloatBusData128;
        Z80Ops::is48 = false;
        Z80Ops::is128 = true;
        Z80Ops::isPentagon = false;
        statesInFrame = TSTATES_PER_FRAME_128;
        IntStart = INT_START128;
        IntEnd = INT_END128 + CPU::latetiming;
        ESPectrum::target[0] = MICROS_PER_FRAME_128;
        ESPectrum::target[1] = MICROS_PER_FRAME_128;
        ESPectrum::target[2] = MICROS_PER_FRAME_128_125SPEED;
        ESPectrum::target[3] = MICROS_PER_FRAME_128_150SPEED;                        
    } else if (Config::arch == "Pentagon") {
        Z80Ops::is48 = false;
        Z80Ops::is128 = false;
        Z80Ops::isPentagon = true;
        statesInFrame = TSTATES_PER_FRAME_PENTAGON;
        IntStart = INT_START_PENTAGON;
        IntEnd = INT_END_PENTAGON + CPU::latetiming;
        ESPectrum::target[0] = MICROS_PER_FRAME_PENTAGON;
        ESPectrum::target[1] = MICROS_PER_FRAME_PENTAGON;
        ESPectrum::target[2] = MICROS_PER_FRAME_PENTAGON_125SPEED;
        ESPectrum::target[3] = MICROS_PER_FRAME_PENTAGON_150SPEED;
    }

    stFrame = statesInFrame - IntEnd;

    tstates = 0;
    global_tstates = 0;

}

///////////////////////////////////////////////////////////////////////////////

IRAM_ATTR void CPU::loop() {

    // Check NMI
    if (Z80::isNMI()) {
        Z80::execute();
        Z80::doNMI();
    }

    while (tstates < IntEnd) Z80::execute();

    if (!Z80::isHalted()) {
        stFrame = statesInFrame - IntEnd;
        Z80::exec_nocheck();
        if (stFrame == 0) FlushOnHalt();
    } else {
        FlushOnHalt();
    }

    while (tstates < statesInFrame) Z80::execute();
    
    VIDEO::EndFrame();

    global_tstates += statesInFrame; // increase global Tstates
    tstates -= statesInFrame;

}

///////////////////////////////////////////////////////////////////////////////

IRAM_ATTR void CPU::FlushOnHalt() {
        
    uint32_t stEnd = statesInFrame - IntEnd;    

    uint8_t page = Z80::getRegPC() >> 14;
    if (MemESP::ramContended[page]) {

        while (tstates < stEnd ) {
            VIDEO::Draw_Opcode(true);
            Z80::incRegR(1);
        }

    } else {

        if (VIDEO::snow_toggle) {

            // ULA perfect cycle & snow effect use this code
            while (tstates < stEnd ) {
                VIDEO::Draw_Opcode(false);
                Z80::incRegR(1);
            }

        } else {

            // Flush the rest of frame
            uint32_t pre_tstates = tstates;
            while (VIDEO::Draw != &VIDEO::Blank)
                VIDEO::Draw(VIDEO::tStatesPerLine, false);
            tstates = pre_tstates;

            uint32_t incr = (stEnd - pre_tstates) >> 2;
            if (pre_tstates & 0x03) incr++;
            tstates += (incr << 2);
            Z80::incRegR(incr & 0x000000FF);

        }

    }

}

///////////////////////////////////////////////////////////////////////////////
// Z80Ops
///////////////////////////////////////////////////////////////////////////////

// Read byte from RAM
IRAM_ATTR uint8_t Z80Ops::peek8(uint16_t address) {
    uint8_t page = address >> 14;
    VIDEO::Draw(3,MemESP::ramContended[page]);
    return MemESP::ramCurrent[page][address & 0x3fff];
}

// Write byte to RAM
IRAM_ATTR void Z80Ops::poke8(uint16_t address, uint8_t value) {

    uint8_t page = address >> 14;

    if (page == 0) {
        VIDEO::Draw(3, false);
        return;
    }

    VIDEO::Draw(3, MemESP::ramContended[page]);
    MemESP::ramCurrent[page][address & 0x3fff] = value;

}

// Read word from RAM
IRAM_ATTR uint16_t Z80Ops::peek16(uint16_t address) {

    uint8_t page = address >> 14;
    // uint16_t addrinpage = address & 0x3fff;
    // if (addrinpage < 0x3fff) {    // Check if address is between two different pages
    if (page == ((address + 1) >> 14)) {    // Check if address is between two different pages

        // uint8_t page = address >> 14;

        if (MemESP::ramContended[page]) {
            VIDEO::Draw(3, true);
            VIDEO::Draw(3, true);            
        } else
            VIDEO::Draw(6, false);

        return ((MemESP::ramCurrent[page][(address & 0x3fff) + 1] << 8) | MemESP::ramCurrent[page][address & 0x3fff]);

        // return (MemESP::ramCurrent[page][addrinpage + 1] << 8) | MemESP::ramCurrent[page][addrinpage];

    } else {

        // Order matters, first read lsb, then read msb, don't "optimize"
        uint8_t lsb = Z80Ops::peek8(address);
        uint8_t msb = Z80Ops::peek8(address + 1);
        return (msb << 8) | lsb;

    }

}

// Write word to RAM
IRAM_ATTR void Z80Ops::poke16(uint16_t address, RegisterPair word) {

    uint8_t page = address >> 14;
    uint16_t page_addr = address & 0x3fff;

    if (page_addr < 0x3fff) {    // Check if address is between two different pages    

        if (page == 0) {
            VIDEO::Draw(6, false);
            return;
        }

        if (MemESP::ramContended[page]) {
            VIDEO::Draw(3, true);
            VIDEO::Draw(3, true);            
        } else
            VIDEO::Draw(6, false);

        MemESP::ramCurrent[page][page_addr] = word.byte8.lo;
        MemESP::ramCurrent[page][page_addr + 1] = word.byte8.hi;

    } else {

        // Order matters, first write lsb, then write msb, don't "optimize"
        Z80Ops::poke8(address, word.byte8.lo);
        Z80Ops::poke8(address + 1, word.byte8.hi);

    }

}

// Put an address on bus lasting 'tstates' cycles
IRAM_ATTR void Z80Ops::addressOnBus(uint16_t address, int32_t wstates) {
    if (MemESP::ramContended[address >> 14]) {
        for (int idx = 0; idx < wstates; idx++)
            VIDEO::Draw(1, true);
    } else
        VIDEO::Draw(wstates, false);
}

// Callback to know when the INT signal is active
IRAM_ATTR bool Z80Ops::isActiveINT(void) {
    int tmp = CPU::tstates + CPU::latetiming;
    if (tmp >= CPU::statesInFrame) tmp -= CPU::statesInFrame;
    return ((tmp >= CPU::IntStart) && (tmp < CPU::IntEnd));
}

