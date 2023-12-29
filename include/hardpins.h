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

#ifndef ESPectrum_hardpins_h
#define ESPectrum_hardpins_h

#include "hardconfig.h"

// Audio Out
#define SPEAKER_PIN 25

// Storage mode: pins for external SD card (LILYGO TTGO VGA32 Board and ESPectrum Board)
#define PIN_NUM_MISO_LILYGO_ESPECTRUM GPIO_NUM_2
#define PIN_NUM_MOSI_LILYGO_ESPECTRUM GPIO_NUM_12
#define PIN_NUM_CLK_LILYGO_ESPECTRUM  GPIO_NUM_14
#define PIN_NUM_CS_LILYGO_ESPECTRUM   GPIO_NUM_13

// Storage mode: pins for external SD card (Olimex ESP32-SBC-FABGL Board)
#define PIN_NUM_MISO_SBCFABGL GPIO_NUM_35
#define PIN_NUM_MOSI_SBCFABGL GPIO_NUM_12
#define PIN_NUM_CLK_SBCFABGL GPIO_NUM_14
#define PIN_NUM_CS_SBCFABGL GPIO_NUM_13

// VGA Pins (6 bit)
#define RED_PINS_6B 21, 22
#define GRE_PINS_6B 18, 19
#define BLU_PINS_6B  4,  5
#define HSYNC_PIN 23
#define VSYNC_PIN 15

// Shift Register pins
#define SR_CLK 0
#define SR_LOAD 26
#define SR_DATA 27

// keyboard membrane 5 input pins
#define KM_COL_0 3
#define KM_COL_1 34
#define KM_COL_2 35
#define KM_COL_3 36
#define KM_COL_4 39

#endif // ESPectrum_hardpins_h
