/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023-2025 Víctor Iborra [Eremus] and 2023 David Crespo [dcrespo3d]
https://github.com/EremusOne/ESPectrum

Based on ZX-ESPectrum-Wiimote
Copyright (c) 2020-2022 David Crespo [dcrespo3d]
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

To Contact the dev team you can write to zxespectrum@gmail.com

*/

#ifndef ESPECTRUM_AUDIOIN_H
#define ESPECTRUM_AUDIOIN_H

#include <inttypes.h>

#define AUDIOIN_STOP 0
#define AUDIOIN_PLAY 1
#define AUDIOIN_PAUSE 2

class AudioIn {

public:

    static volatile uint8_t Status;
    static volatile uint8_t* Buffer;
    static volatile int Bufsize;
    static volatile int sample_index;
    static double Factor;
    static uint64_t Basepos;
    static volatile uint32_t* gpio_in;
    static volatile uint32_t gpio_mask;

    static void Init();
    static void GPIOSetup();
    static void Start();
    static void Play();
    static void Stop();
    static void Pause();
    static void Resume();
    static void PrepareFrame();
    static uint8_t GetLevel();

};

#endif