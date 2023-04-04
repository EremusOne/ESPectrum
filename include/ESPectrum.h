///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
//
// Copyright (c) 2023 Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
// https://github.com/EremusOne/ZX-ESPectrum-IDF
//
// Based on ZX-ESPectrum-Wiimote
// Copyright (c) 2020, 2022 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez and Jorge Fuertes
// https://github.com/rampa069/ZX-ESPectrum
//
// Original project by Pete Todd
// https://github.com/retrogubbins/paseVGA
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

#define ESP_AUDIO_OVERSAMPLES_48 4368
#define ESP_AUDIO_FREQ_48 31250 // In 48K calcs are perfect :) -> ESP_AUDIO_SAMPLES_48 * 50,0801282 frames per second = 31250 Hz
#define ESP_AUDIO_SAMPLES_48  624
#define ESP_OFFSET_48 128;

#define ESP_AUDIO_OVERSAMPLES_128 3732
#define ESP_AUDIO_FREQ_128 31112 // ESP_AUDIO_SAMPLES_128 * 50,020008 fps = 31112,445 Hz. 
#define ESP_AUDIO_SAMPLES_128 622
#define ESP_OFFSET_128 128; // 90;

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
    static uint8_t audioBuffer[ESP_AUDIO_SAMPLES_48];
    static uint8_t overSamplebuf[ESP_AUDIO_OVERSAMPLES_48];
    static signed char aud_volume;
    static uint32_t audbufcnt;
    static uint32_t faudbufcnt;    
    static int lastaudioBit;
    static int faudioBit;
    static void audioFrameStart();
    static void IRAM_ATTR audioGetSample(int Audiobit);
    static void audioFrameEnd();
    static int overSamplesPerFrame;
    static int samplesPerFrame;
    static bool AY_emu;
    static int Audio_freq;

    // static bool Audio_restart;

    static int64_t target;

    static int ESPoffset; // Testing

private:

    static void IRAM_ATTR audioTask(void* unused);

};

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

int64_t IRAM_ATTR micros();

unsigned long IRAM_ATTR millis();

inline void IRAM_ATTR delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void IRAM_ATTR delayMicroseconds(int64_t us);

#endif