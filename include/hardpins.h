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

#ifndef ESPectrum_hardpins_h
#define ESPectrum_hardpins_h

#include "hardconfig.h"

// adjusted for Lilygo TTGO
#ifdef SPEAKER_PRESENT
// NOTE: PIN 25 is hardwired in FabGL audio (used for AY3891x emulation)
#define SPEAKER_PIN 25
#endif // SPEAKER_PRESENT

#ifdef EAR_PRESENT
#define EAR_PIN 16
#endif // EAR_PRESENT

#ifdef MIC_PRESENT
#define MIC_PIN 17
#endif // MIC_PRESENT

#ifdef PS2_KEYB_PRESENT
// adjusted for Lilygo TTGO
#define KEYBOARD_DATA 32
#define KEYBOARD_CLK 33
#endif // PS2_KEYB_PRESENT

#ifdef ZX_KEYB_PRESENT
#define AD8 12
#define AD9 26
#define AD10 25
#define AD11 33
#define AD12 27
#define AD13 14
#define AD14 0
#define AD15 13

#define DB0 36
#define DB1 39
#define DB2 34
#define DB3 35
#define DB4 32
#endif // ZX_KEYB_PRESENT

// Storage mode: pins for external SD card
// adjusted for Lilygo TTGO
#define PIN_NUM_MISO GPIO_NUM_2
#define PIN_NUM_MOSI GPIO_NUM_12
#define PIN_NUM_CLK  GPIO_NUM_14
#define PIN_NUM_CS   GPIO_NUM_13

// 6 bit pins
// adjusted for Lilygo TTGO
#define RED_PINS_6B 21, 22
#define GRE_PINS_6B 18, 19
#define BLU_PINS_6B  4,  5
// VGA sync pins
#define HSYNC_PIN 23
#define VSYNC_PIN 15

/////////////////////////////////////////////////
// Colors for 6 bit mode
                              //   BB GGRR 
#define BLACK       0xC0      // 1100 0000
#define BLUE        0xE0      // 1110 0000
#define RED         0xC2      // 1100 0010
#define MAGENTA     0xE2      // 1110 0010
#define GREEN       0xC8      // 1100 1000
#define CYAN        0xE8      // 1110 1000
#define YELLOW      0xCA      // 1100 1010
#define WHITE       0xEA      // 1110 1010
                              //   BB GGRR 
#define BRI_BLACK   0xC0      // 1100 0000
#define BRI_BLUE    0xF0      // 1111 0000
#define BRI_RED     0xC3      // 1100 0011
#define BRI_MAGENTA 0xF3      // 1111 0011
#define BRI_GREEN   0xCC      // 1100 1100
#define BRI_CYAN    0xFC      // 1111 1100
#define BRI_YELLOW  0xCF      // 1100 1111
#define BRI_WHITE   0xFF      // 1111 1111

#endif // ESPectrum_hardpins_h
