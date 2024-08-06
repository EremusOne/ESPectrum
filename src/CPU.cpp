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

    if (Config::arch == "48K" /*|| Config::arch == "TK90X" || Config::arch == "TK95"*/) {
        Ports::getFloatBusData = &Ports::getFloatBusData48;
        Z80Ops::is48 = true;
        Z80Ops::is128 = false;
        Z80Ops::isPentagon = false;
        statesInFrame = TSTATES_PER_FRAME_48;
        IntStart = INT_START48;
        IntEnd = INT_END48 + CPU::latetiming;
        // Set emulation loop sync target
        ESPectrum::target = MICROS_PER_FRAME_48;
    } else if (Config::arch == "TK90X" || Config::arch == "TK95") {

        Z80Ops::is48 = true;
        Z80Ops::is128 = false;
        Z80Ops::isPentagon = false;

        switch (Config::ALUTK) {
        case 0:
            Ports::getFloatBusData = &Ports::getFloatBusData48;
            statesInFrame = TSTATES_PER_FRAME_48;
            ESPectrum::target = MICROS_PER_FRAME_48;
            break;
        case 1:
            Ports::getFloatBusData = &Ports::getFloatBusDataTK;
            statesInFrame = TSTATES_PER_FRAME_TK_50;
            ESPectrum::target = MICROS_PER_FRAME_TK_50;
            break;
        case 2:
            Ports::getFloatBusData = &Ports::getFloatBusDataTK;
            statesInFrame = TSTATES_PER_FRAME_TK_60;
            ESPectrum::target = MICROS_PER_FRAME_TK_60;
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
        // Set emulation loop sync target
        ESPectrum::target = MICROS_PER_FRAME_128;
    } else if (Config::arch == "Pentagon") {
        Z80Ops::is48 = false;
        Z80Ops::is128 = false;
        Z80Ops::isPentagon = true;
        statesInFrame = TSTATES_PER_FRAME_PENTAGON;
        IntStart = INT_START_PENTAGON;
        IntEnd = INT_END_PENTAGON + CPU::latetiming;
        // Set emulation loop sync target
        ESPectrum::target = MICROS_PER_FRAME_PENTAGON;
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

// // Write byte to RAM
// IRAM_ATTR void Z80Ops::poke8(uint16_t address, uint8_t value) {

//     uint8_t page = address >> 14;

//     if (page == 0) {
//         VIDEO::Draw(3, false);
//         return;
//     }

//     #ifndef DIRTY_LINES

//     VIDEO::Draw(3, MemESP::ramContended[page]);
//     MemESP::ramCurrent[page][address & 0x3fff] = value;

//     #else

//     if (page == 2) {
//         VIDEO::Draw(3, false);
//         MemESP::ramCurrent[2][address & 0x3fff] = value;
//         return;
//     }

//     VIDEO::Draw(3, MemESP::ramContended[page]);

//     if (page == 3) {
//         if (MemESP::videoLatch) {
//             if (MemESP::bankLatch != 7) {
//                 MemESP::ramCurrent[3][address & 0x3fff] = value;
//                 return;
//             }
//         } else if (MemESP::bankLatch != 5) {
//             MemESP::ramCurrent[3][address & 0x3fff] = value;
//             return;
//         }
//     } else if (MemESP::videoLatch) {
//         // Page == 1 == videoLatch
//         MemESP::ramCurrent[1][address & 0x3fff] = value;
//         return;
//     }

//     uint16_t vid_line = address & 0x3fff;

//     if (vid_line < 6144) {
    
//         uint8_t result =  (vid_line >> 5) & 0b11000000;
//         result |=  (vid_line >> 2) & 0b00111000;
//         result |=  (vid_line >> 8) & 0b00000111;

//         VIDEO::dirty_lines[result] |= 0x01;

//     } else if (vid_line < 6912) {

//         uint8_t result = ((vid_line - 6144) >> 5) << 3;
//         // for (int i=result; i < result + 8; i++)
//         //     VIDEO::dirty_lines[i] = (value & 0x80) | 0x01;
//         memset((uint8_t *)VIDEO::dirty_lines + result, (value & 0x80) | 0x01, 8);

//     }

//     MemESP::ramCurrent[page][vid_line] = value;

//     #endif

// }

// Write byte to RAM
IRAM_ATTR void Z80Ops::poke8(uint16_t address, uint8_t value) {

    uint8_t page = address >> 14;

    if (page == 0) {
        VIDEO::Draw(3, false);
        return;
    }

    #ifndef DIRTY_LINES

    VIDEO::Draw(3, MemESP::ramContended[page]);
    MemESP::ramCurrent[page][address & 0x3fff] = value;
   
    #else

    uint16_t page_addr = address & 0x3fff;

    if (page == 2) {
        VIDEO::Draw(3, false);
        MemESP::ramCurrent[2][page_addr] = value;
        return;
    }

    if (page == 3) {
        VIDEO::Draw(3, MemESP::ramContended[3]);
        MemESP::ramCurrent[3][page_addr] = value;
        if (MemESP::videoLatch) {
            if (MemESP::bankLatch != 7) return;
        } else {
            if (MemESP::bankLatch != 5) return;
        }
    } else {
        // page = 1
        VIDEO::Draw(3, MemESP::ramContended[1]);
        MemESP::ramCurrent[1][page_addr] = value;
        if (MemESP::videoLatch) return;
    }

    if (page_addr < 6144) {
    
        uint8_t result =  (page_addr >> 5) & 0b11000000;
        result |=  (page_addr >> 2) & 0b00111000;
        result |=  (page_addr >> 8) & 0b00000111;

        VIDEO::dirty_lines[result] |= 0x01;

    } else if (page_addr < 6912) {

        uint8_t result = ((page_addr - 6144) >> 5) << 3;
        memset((uint8_t *)VIDEO::dirty_lines + result, (value & 0x80) | 0x01, 8);

    }

    #endif

}

// Read word from RAM
IRAM_ATTR uint16_t Z80Ops::peek16(uint16_t address) {

    uint8_t page = address >> 14;

    if (page == ((address + 1) >> 14)) {    // Check if address is between two different pages

        if (MemESP::ramContended[page]) {
            VIDEO::Draw(3, true);
            VIDEO::Draw(3, true);            
        } else
            VIDEO::Draw(6, false);

        return ((MemESP::ramCurrent[page][(address & 0x3fff) + 1] << 8) | MemESP::ramCurrent[page][address & 0x3fff]);

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

        #ifndef DIRTY_LINES

        if (MemESP::ramContended[page]) {
            VIDEO::Draw(3, true);
            VIDEO::Draw(3, true);            
        } else
            VIDEO::Draw(6, false);

        MemESP::ramCurrent[page][page_addr] = word.byte8.lo;
        MemESP::ramCurrent[page][page_addr + 1] = word.byte8.hi;

        #else

        if (page == 2) {
            VIDEO::Draw(6, false);
            MemESP::ramCurrent[2][page_addr] = word.byte8.lo;
            MemESP::ramCurrent[2][page_addr + 1] = word.byte8.hi;
            return;
        }

        if (MemESP::ramContended[page]) {
            VIDEO::Draw(3, true);
            VIDEO::Draw(3, true);            
        } else
            VIDEO::Draw(6, false);

        if (page == 3) {
            MemESP::ramCurrent[3][page_addr] = word.byte8.lo;
            MemESP::ramCurrent[3][page_addr + 1] = word.byte8.hi;
            if (MemESP::videoLatch) {
                if (MemESP::bankLatch != 7) return;
            } else {
                if (MemESP::bankLatch != 5) return;
            }
        } else {
            // page == 1
            MemESP::ramCurrent[1][page_addr] = word.byte8.lo;
            MemESP::ramCurrent[1][page_addr + 1] = word.byte8.hi;
            if (MemESP::videoLatch) return;
        }

        if (page_addr < 6144) {
        
            uint8_t result =  (page_addr >> 5) & 0b11000000;
            result |=  (page_addr >> 2) & 0b00111000;
            result |=  (page_addr >> 8) & 0b00000111;

            VIDEO::dirty_lines[result] |= 0x01;

        } else if (page_addr < 6912) {

            uint8_t result = ((page_addr - 6144) >> 5) << 3;
            memset((uint8_t *)VIDEO::dirty_lines + result, (word.byte8.lo & 0x80) | 0x01, 8);

        } else return;

        page_addr++;

        if (page_addr < 6144) {
        
            uint8_t result =  (page_addr >> 5) & 0b11000000;
            result |=  (page_addr >> 2) & 0b00111000;
            result |=  (page_addr >> 8) & 0b00000111;

            VIDEO::dirty_lines[result] |= 0x01;

        } else if (page_addr < 6912) {

            uint8_t result = ((page_addr - 6144) >> 5) << 3;
            memset((uint8_t *)VIDEO::dirty_lines + result, (word.byte8.hi & 0x80) | 0x01, 8);

        }

        #endif

    } else {

        // Order matters, first write lsb, then write msb, don't "optimize"
        Z80Ops::poke8(address, word.byte8.lo);
        Z80Ops::poke8(address + 1, word.byte8.hi);

    }

}

// // Write word to RAM
// IRAM_ATTR void Z80Ops::poke16(uint16_t address, RegisterPair word) {

//     uint8_t page = address >> 14;
//     uint16_t vid_line = address & 0x3fff;

//     if (vid_line < 0x3fff) {    // Check if address is between two different pages    

//         if (page == 0) {
//             VIDEO::Draw(6, false);
//             return;
//         }

//         #ifndef DIRTY_LINES

//         if (MemESP::ramContended[page]) {
//             VIDEO::Draw(3, true);
//             VIDEO::Draw(3, true);            
//         } else
//             VIDEO::Draw(6, false);

//         MemESP::ramCurrent[page][vid_line] = word.byte8.lo;
//         MemESP::ramCurrent[page][vid_line + 1] = word.byte8.hi;

//         #else

//         if (page == 2) {
//             VIDEO::Draw(6, false);
//             MemESP::ramCurrent[2][vid_line] = word.byte8.lo;
//             MemESP::ramCurrent[2][vid_line + 1] = word.byte8.hi;
//             return;
//         }

//         if (MemESP::ramContended[page]) {
//             VIDEO::Draw(3, true);
//             VIDEO::Draw(3, true);            
//         } else
//             VIDEO::Draw(6, false);

//         if (page == 3) {
//             if (MemESP::videoLatch) {
//                 if (MemESP::bankLatch != 7) {
//                     MemESP::ramCurrent[3][vid_line] = word.byte8.lo;
//                     MemESP::ramCurrent[3][vid_line + 1] = word.byte8.hi;
//                     return;
//                 }
//             } else if (MemESP::bankLatch != 5) {
//                 MemESP::ramCurrent[3][vid_line] = word.byte8.lo;
//                 MemESP::ramCurrent[3][vid_line + 1] = word.byte8.hi;
//                 return;
//             }
//         } else if (MemESP::videoLatch) {
//             // Page == 1 == videoLatch            
//             MemESP::ramCurrent[1][vid_line] = word.byte8.lo;
//             MemESP::ramCurrent[1][vid_line + 1] = word.byte8.hi;
//             return;
//         }

//         MemESP::ramCurrent[page][vid_line] = word.byte8.lo;
//         MemESP::ramCurrent[page][vid_line + 1] = word.byte8.hi;

//         if (vid_line < 6144) {
        
//             uint8_t result =  (vid_line >> 5) & 0b11000000;
//             result |=  (vid_line >> 2) & 0b00111000;
//             result |=  (vid_line >> 8) & 0b00000111;

//             VIDEO::dirty_lines[result] |= 0x01;

//         } else if (vid_line < 6912) {

//             uint8_t result = ((vid_line - 6144) >> 5) << 3;
//             // for (int i=result; i < result + 8; i++)
//             //     VIDEO::dirty_lines[i] = (word.byte8.lo & 0x80) | 0x01;
//             memset((uint8_t *)VIDEO::dirty_lines + result, (word.byte8.lo & 0x80) | 0x01, 8);

//         } else return;

//         vid_line++;

//         if (vid_line < 6144) {
        
//             uint8_t result =  (vid_line >> 5) & 0b11000000;
//             result |=  (vid_line >> 2) & 0b00111000;
//             result |=  (vid_line >> 8) & 0b00000111;

//             VIDEO::dirty_lines[result] |= 0x01;

//         } else if (vid_line < 6912) {

//             uint8_t result = ((vid_line - 6144) >> 5) << 3;
//             // for (int i=result; i < result + 8; i++)
//             //     VIDEO::dirty_lines[i] = (word.byte8.hi & 0x80) | 0x01;
//             memset((uint8_t *)VIDEO::dirty_lines + result, (word.byte8.hi & 0x80) | 0x01, 8);

//         }

//         #endif

//     } else {

//         // Order matters, first write lsb, then write msb, don't "optimize"
//         Z80Ops::poke8(address, word.byte8.lo);
//         Z80Ops::poke8(address + 1, word.byte8.hi);

//     }

// }


/* Put an address on bus lasting 'tstates' cycles */
IRAM_ATTR void Z80Ops::addressOnBus(uint16_t address, int32_t wstates) {
    if (MemESP::ramContended[address >> 14]) {
        for (int idx = 0; idx < wstates; idx++)
            VIDEO::Draw(1, true);
    } else
        VIDEO::Draw(wstates, false);
}

/* Callback to know when the INT signal is active */
IRAM_ATTR bool Z80Ops::isActiveINT(void) {
    int tmp = CPU::tstates + CPU::latetiming;
    if (tmp >= CPU::statesInFrame) tmp -= CPU::statesInFrame;
    return ((tmp >= CPU::IntStart) && (tmp < CPU::IntEnd));
}

