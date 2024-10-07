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

#ifndef MemESP_h
#define MemESP_h

#include <inttypes.h>
#include <esp_attr.h>

#define MEM_PG_SZ 0x4000

#ifdef ESPECTRUM_PSRAM
struct slotdata {
    uint8_t RegI;
    uint16_t RegHLx;
    uint16_t RegDEx;
    uint16_t RegBCx;
    uint16_t RegAFx;
    uint16_t RegHL;
    uint16_t RegDE;
    uint16_t RegBC;
    uint16_t RegIY;
    uint16_t RegIX;
    uint8_t inter;
    uint8_t RegR;
    uint16_t RegAF;
    uint16_t RegSP;
    uint8_t IM;
    uint8_t borderColor;
    uint16_t RegPC;
    uint8_t bankLatch;
    uint8_t videoLatch;
    uint8_t romLatch;
    uint8_t pagingLock;
    bool trdos;
};
#endif
class MemESP
{
public:

    static uint8_t* rom[5];

    static uint8_t* ram[8];

    #ifdef ESPECTRUM_PSRAM
    #define TIME_MACHINE_SLOTS 8
    static uint32_t* timemachine[TIME_MACHINE_SLOTS][8];
    static uint8_t tm_slotbanks[TIME_MACHINE_SLOTS][8];
    static slotdata tm_slotdata[TIME_MACHINE_SLOTS];
    static bool tm_bank_chg[8];
    static uint8_t cur_timemachine;
    static int tm_framecnt;  
    static bool tm_loading_slot;
    #endif

    static uint8_t* ramCurrent[4];    
    static bool ramContended[4];

    static uint8_t bankLatch;
    static uint8_t videoLatch;
    static uint8_t romLatch;
    static uint8_t pagingLock;

    static uint8_t romInUse;

    static bool SPRom;

    static bool Init();
    static void Reset();    

    #ifdef ESPECTRUM_PSRAM
    static void Tm_Load(uint8_t slot);
    static void Tm_Init();
    static void Tm_DoTimeMachine();
    #endif

    static uint8_t readbyte(uint16_t addr);
    static uint16_t readword(uint16_t addr);
    static void writebyte(uint16_t addr, uint8_t data);
    static void writeword(uint16_t addr, uint16_t data);

};

///////////////////////////////////////////////////////////////////////////////
// Inline memory access functions
///////////////////////////////////////////////////////////////////////////////

inline uint8_t MemESP::readbyte(uint16_t addr) {
    uint8_t page = addr >> 14;
    switch (page) {
    case 0:
        return rom[romInUse][addr];
    case 1:
        return ram[5][addr - 0x4000];
    case 2:
        return ram[2][addr - 0x8000];
    case 3:
        return ram[bankLatch][addr - 0xC000];
    default:
        return rom[romInUse][addr];
    }
}

inline uint16_t MemESP::readword(uint16_t addr) {
    return ((readbyte(addr + 1) << 8) | readbyte(addr));
}

inline void MemESP::writebyte(uint16_t addr, uint8_t data)
{
    uint8_t page = addr >> 14;
    switch (page) {
    case 0:
        return;
    case 1:
        ram[5][addr - 0x4000] = data;
        break;
    case 2:
        ram[2][addr - 0x8000] = data;
        break;
    case 3:
        ram[bankLatch][addr - 0xC000] = data;
        break;
    }
    return;
}

inline void MemESP::writeword(uint16_t addr, uint16_t data) {
    writebyte(addr, (uint8_t)data);
    writebyte(addr + 1, (uint8_t)(data >> 8));
}


#endif