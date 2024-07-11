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

#include "MemESP.h"
#include "Config.h"
#include <stddef.h>
#include "esp_heap_caps.h"

using namespace std;

uint8_t* MemESP::rom[5];

uint8_t* MemESP::ram[8] = { NULL };
uint8_t* MemESP::ramCurrent[4];
bool MemESP::ramContended[4];

uint8_t MemESP::bankLatch = 0;
uint8_t MemESP::videoLatch = 0;
uint8_t MemESP::romLatch = 0;
uint8_t MemESP::pagingLock = 0;
uint8_t MemESP::romInUse = 0;

bool MemESP::Init() {

    #ifdef ESPECTRUM_PSRAM

    // Video pages in SRAM (faster)
    MemESP::ram[5] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[7] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);

    // Rest of pages in PSRAM
    MemESP::ram[0] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    MemESP::ram[2] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    MemESP::ram[1] = (unsigned char *) heap_caps_calloc(0x8000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    MemESP::ram[3] = ((unsigned char *) MemESP::ram[1]) + 0x4000;
    MemESP::ram[4] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    MemESP::ram[6] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    #else

    MemESP::ram[5] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[0] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[2] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[7] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[1] = (unsigned char *) heap_caps_calloc(0x8000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[3] = ((unsigned char *) MemESP::ram[1]) + 0x4000;
    MemESP::ram[4] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[6] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);

    #endif

    for (int i=0; i < 8; i++) {
        if (MemESP::ram[i] == NULL) {
            if (Config::slog_on) printf("ERROR! Unable to allocate ram%d\n",i);
            return false;
        }
    }

    return true;

}

void MemESP::Reset() {

    MemESP::romInUse = 0;
    MemESP::bankLatch = 0;
    MemESP::videoLatch = 0;
    MemESP::romLatch = 0;

    MemESP::ramCurrent[0] = MemESP::rom[0];
    MemESP::ramCurrent[1] = MemESP::ram[5];
    MemESP::ramCurrent[2] = MemESP::ram[2];
    MemESP::ramCurrent[3] = MemESP::ram[0];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = Config::arch == "Pentagon" ? false : true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = false;

    MemESP::pagingLock = Config::arch == "48K" ? 1 : 0;

}