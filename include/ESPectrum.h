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

#ifndef ESPectrum_h
#define ESPectrum_h
#include <string>
#include "hardpins.h"
#include "CaptureBMP.h"
#include "fabgl.h"

using namespace std;

#define ESP_AUDIO_OVERSAMPLES 4432 // For 48K we get 4368 samples per frame, for 128K we get 4432
#define ESP_AUDIO_FREQ_48 27300
#define ESP_AUDIO_FREQ_128 27700
#define ESP_AUDIO_SAMPLES 554 // For 48K we get 546 samples per frame, for 128K we get 554
#define ESP_AUDIO_TSTATES 128

class ESPectrum
{
public:

    static void setup();
    static void IRAM_ATTR loop();
    static void reset();
    static void loadRom(string arch, string romset);

    // Kbd
    static void IRAM_ATTR processKeyboard();
    static bool IRAM_ATTR readKbd(fabgl::VirtualKeyItem *Nextkey);
    static fabgl::PS2Controller PS2Controller;

    // Audio
    static uint8_t audioBuffer[ESP_AUDIO_SAMPLES];
    static uint8_t overSamplebuf[ESP_AUDIO_OVERSAMPLES];
    static signed char aud_volume;
    static uint32_t audbufcnt;
    static uint32_t faudbufcnt;    
    static int lastaudioBit;
    static int faudioBit;
    static void audioFrameStart();
    static void IRAM_ATTR audioGetSample(int Audiobit);
    static void audioFrameEnd();
    static int samplesPerFrame;
    static bool AY_emu;
    static int Audio_freq;

    static int ESPoffset; // Testing
    
private:

    static void IRAM_ATTR audioTask(void* unused);

};

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

#endif