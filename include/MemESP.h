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

#ifndef MemESP_h
#define MemESP_h

#include <inttypes.h>
#include <esp_attr.h>

// #define address_is_contended(addr) (1 == (addr >> 14))

#define MEM_PG_SZ 0x4000

class MemESP
{
public:

    static uint8_t* rom[4];

    static uint8_t* ram0;
    static uint8_t* ram1;
    static uint8_t* ram2;
    static uint8_t* ram3;
    static uint8_t* ram4;
    static uint8_t* ram5;
    static uint8_t* ram6;
    static uint8_t* ram7;

    static uint8_t* ram[8];

    static volatile uint8_t bankLatch;
    static volatile uint8_t videoLatch;
    static volatile uint8_t romLatch;
    static volatile uint8_t pagingLock;
    static uint8_t modeSP3;
    static uint8_t romSP3;
    static uint8_t romInUse;

    static uint8_t readbyte(uint16_t addr);
    static uint16_t readword(uint16_t addr);
    static void writebyte(uint16_t addr, uint8_t data);
    static void writeword(uint16_t addr, uint16_t data);
};

static uint8_t DRAM_ATTR staticMemPage0[0x4000] = { 0 };
static uint8_t DRAM_ATTR staticMemPage1[0x4000] = { 0 };
static uint8_t DRAM_ATTR staticMemPage2[0x4000] = { 0 };

///////////////////////////////////////////////////////////////////////////////
//
// inline memory access functions

inline uint8_t MemESP::readbyte(uint16_t addr) {
    uint8_t page = addr >> 14;
    switch (page) {
    case 0:
        return rom[romInUse][addr];
    case 1:
        return ram5[addr - 0x4000];
    case 2:
        return ram2[addr - 0x8000];
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
        ram5[addr - 0x4000] = data;
        break;
    case 2:
        ram2[addr - 0x8000] = data;
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