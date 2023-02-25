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

#ifndef CPU_h
#define CPU_h

#include <inttypes.h>
#include "ESPectrum.h"
#include "ESP32Lib/ESP32Lib.h"

class CPU
{
public:
    // call this for initializing CPU
    static void setup();

    // call this for executing a frame's worth of instructions
    static void IRAM_ATTR loop();

    // call this for resetting the CPU
    static void reset();

    // Flush screen
    static void FlushOnHalt();

    // get the number of CPU Tstates per frame (machine dependant)
    static uint32_t statesPerFrame();

    // get the number of microseconds per frame (machine dependant)
    static uint32_t microsPerFrame();

    // CPU Tstates elapsed in current frame
    static uint32_t tstates;

    // CPU Tstates elapsed since reset
    static uint64_t global_tstates;

    // CPU Tstates in frame
    static uint32_t statesInFrame;

    // Frames elapsed
    static uint32_t framecnt;

};

#endif // CPU_h
