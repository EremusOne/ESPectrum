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

#ifndef CPU_h
#define CPU_h

#include <inttypes.h>
#include "ESPectrum.h"

#define TSTATES_PER_FRAME_48 69888
#define TSTATES_PER_FRAME_128 70908
#define TSTATES_PER_FRAME_PENTAGON 71680

#define MICROS_PER_FRAME_48 19968
#define MICROS_PER_FRAME_128 19992
#define MICROS_PER_FRAME_PENTAGON 20480

#define INT_START48 0
#define INT_END48 32
#define INT_START128 0
#define INT_END128 36
#define INT_START_PENTAGON 0
#define INT_END_PENTAGON 36

class CPU
{
public:
    
    // call this for executing a frame's worth of instructions
    static void loop();

    // call this for resetting the CPU
    static void reset();

    // Flush screen
    static void FlushOnHalt();

    // CPU Tstates elapsed in current frame
    static uint32_t tstates;

    // CPU Tstates elapsed since reset
    static uint64_t global_tstates;

    // CPU Tstates in frame
    static uint32_t statesInFrame;

    // Late timing
    static uint8_t latetiming;

    // INT signal lenght
    static uint8_t IntStart;
    static uint8_t IntEnd;

};

#endif // CPU_h
