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

#ifndef ESPectrum_h
#define ESPectrum_h
#include <string>
#include "hardpins.h"
#include "CaptureBMP.h"
#include "fabgl.h"
#include "wd1793.h"

using namespace std;

#define ESP_AUDIO_OVERSAMPLES_48 4368
#define ESP_AUDIO_FREQ_48 31250 // In 48K calcs are perfect :) -> ESP_AUDIO_SAMPLES_48 * 50,0801282 frames per second = 31250 Hz
#define ESP_AUDIO_SAMPLES_48  624

#define ESP_AUDIO_OVERSAMPLES_128 3732
#define ESP_AUDIO_FREQ_128 31112 // ESP_AUDIO_SAMPLES_128 * 50,020008 fps = 31112,445 Hz. 
#define ESP_AUDIO_SAMPLES_128 622

#define ESP_AUDIO_OVERSAMPLES_PENTAGON 4480
#define ESP_AUDIO_FREQ_PENTAGON 31250 // ESP_AUDIO_SAMPLES_PENTAGON * 48,828125 frames per second = 31250 Hz
#define ESP_AUDIO_SAMPLES_PENTAGON  640

#define ESP_DEFAULT_VOLUME -8

class ESPectrum
{
public:

    static void setup();
    // static void IRAM_ATTR loop(void* unused);
    static void IRAM_ATTR loop();
    static void reset();
    // static void loadRom(string arch, string romset);

    // Kbd
    static void IRAM_ATTR processKeyboard();
    static void bootKeyboard();
    static bool IRAM_ATTR readKbd(fabgl::VirtualKeyItem *Nextkey);
    static void IRAM_ATTR readKbdJoy();
    static fabgl::PS2Controller PS2Controller;
    static uint8_t PS2cols[8];

    // Audio
    static uint8_t audioBuffer[ESP_AUDIO_SAMPLES_PENTAGON];
    static uint8_t overSamplebuf[ESP_AUDIO_OVERSAMPLES_PENTAGON];
    // static uint8_t SamplebufAY[ESP_AUDIO_SAMPLES_48];
    static signed char aud_volume;
    static uint32_t audbufcnt;
    static uint32_t audbufcntAY;
    static uint32_t faudbufcnt;
    static uint32_t faudbufcntAY;
    static int lastaudioBit;
    static int faudioBit;
    static void audioFrameStart();
    static void IRAM_ATTR BeeperGetSample(int Audiobit);
    static void IRAM_ATTR AYGetSample();
    static void audioFrameEnd();
    static int overSamplesPerFrame;
    static int samplesPerFrame;
    static bool AY_emu;
    static int Audio_freq;
    static int sync_cnt;    
    static uint8_t *audbuffertosend;    

    static int TapeNameScroller;

    // static bool Audio_restart;

    static int64_t target;

    static int ESPoffset; // Testing

    static volatile bool vsync;

    // static TaskHandle_t loopTaskHandle;

    static bool ps2kbd2;

    static bool trdos;
    static WD1793 Betadisk;

private:

    static void IRAM_ATTR audioTask(void* unused);

};

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

// int64_t IRAM_ATTR micros();

unsigned long IRAM_ATTR millis();

inline void IRAM_ATTR delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void IRAM_ATTR delayMicroseconds(int64_t us);

#endif