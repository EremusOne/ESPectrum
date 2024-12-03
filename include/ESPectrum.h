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

#ifndef ESPectrum_h
#define ESPectrum_h
#include <string>
#include "hardpins.h"
#include "CaptureBMP.h"
#include "fabgl.h"
#include "wd1793.h"

using namespace std;

// For reference / calcs
// #define TSTATES_PER_FRAME_48 69888
// #define TSTATES_PER_FRAME_TK_50 71136
// #define TSTATES_PER_FRAME_TK_60 59736
// #define TSTATES_PER_FRAME_128 70908
// #define TSTATES_PER_FRAME_PENTAGON 71680
// #define MICROS_PER_FRAME_48 19968
// #define MICROS_PER_FRAME_TK_50 19895
// #define MICROS_PER_FRAME_TK_60 16707
// #define MICROS_PER_FRAME_128 19992
// #define MICROS_PER_FRAME_PENTAGON 20480

#define ESP_AUDIO_OVERSAMPLES_48 4368
#define ESP_AUDIO_FREQ_48 31250 // In 48K calcs are perfect :) -> ESP_AUDIO_SAMPLES_48 * 50,0801282 frames per second = 31250 Hz
#define ESP_AUDIO_FREQ_48_125SPEED 39063 // 125% speed
#define ESP_AUDIO_FREQ_48_150SPEED 46875 // 150% speed
#define ESP_AUDIO_SAMPLES_48  624
#define ESP_AUDIO_SAMPLES_DIV_48  7
#define ESP_AUDIO_AY_DIV_48  112
#define ESP_AUDIO_OVERSAMPLES_DIV_48  16

#define ESP_AUDIO_OVERSAMPLES_TK_50 3744
#define ESP_AUDIO_FREQ_TK_50 31365 // ESP_AUDIO_SAMPLES_TK_50 * 50,2638854 frames per second = 31364,6645 Hz
#define ESP_AUDIO_FREQ_TK_50_125SPEED 39206 // 125% speed
#define ESP_AUDIO_FREQ_TK_50_150SPEED 47048 // 150% speed
#define ESP_AUDIO_SAMPLES_TK_50  624
#define ESP_AUDIO_SAMPLES_DIV_TK_50  6
#define ESP_AUDIO_AY_DIV_TK_50  114
#define ESP_AUDIO_OVERSAMPLES_DIV_TK_50 19

#define ESP_AUDIO_OVERSAMPLES_TK_60 3144
#define ESP_AUDIO_FREQ_TK_60 31364 // ESP_AUDIO_SAMPLES_TK_60 * 59,8551505 frames per second = 31364,0989 Hz
#define ESP_AUDIO_FREQ_TK_60_125SPEED 39204 // 125% speed
#define ESP_AUDIO_FREQ_TK_60_150SPEED 47046 // 150% speed
#define ESP_AUDIO_SAMPLES_TK_60  524
#define ESP_AUDIO_SAMPLES_DIV_TK_60  6
#define ESP_AUDIO_AY_DIV_TK_60  114
#define ESP_AUDIO_OVERSAMPLES_DIV_TK_60 19

#define ESP_AUDIO_OVERSAMPLES_128 3732
#define ESP_AUDIO_FREQ_128 31112 // ESP_AUDIO_SAMPLES_128 * 50,020008 fps = 31112,445 Hz.
#define ESP_AUDIO_FREQ_128_125SPEED 38890 // 125% speed
#define ESP_AUDIO_FREQ_128_150SPEED 46669 // 150% speed
#define ESP_AUDIO_SAMPLES_128 622
#define ESP_AUDIO_SAMPLES_DIV_128  6
#define ESP_AUDIO_AY_DIV_128  114
#define ESP_AUDIO_OVERSAMPLES_DIV_128 19

#define ESP_AUDIO_OVERSAMPLES_PENTAGON 4480
#define ESP_AUDIO_FREQ_PENTAGON 31250 // ESP_AUDIO_SAMPLES_PENTAGON * 48,828125 frames per second = 31250 Hz
#define ESP_AUDIO_FREQ_PENTAGON_125SPEED 39062 // 125% speed
#define ESP_AUDIO_FREQ_PENTAGON_150SPEED 46876 // 150% speed
#define ESP_AUDIO_SAMPLES_PENTAGON  640
#define ESP_AUDIO_SAMPLES_DIV_PENTAGON  7
#define ESP_AUDIO_AY_DIV_PENTAGON  112
#define ESP_AUDIO_OVERSAMPLES_DIV_PENTAGON 16

#define ESP_VOLUME_DEFAULT -8
#define ESP_VOLUME_MAX 0
#define ESP_VOLUME_MIN -16
class ESPectrum
{
public:

    static void setup();
    static void loop();
    static void reset();

    // Kbd
    static void processKeyboard();
    static void bootKeyboard();
    static bool readKbd(fabgl::VirtualKeyItem *Nextkey);
    static void readKbdJoy();
    static fabgl::PS2Controller PS2Controller;
    static fabgl::VirtualKey JoyVKTranslation[24];
    static fabgl::VirtualKey VK_ESPECTRUM_FIRE1;
    static fabgl::VirtualKey VK_ESPECTRUM_FIRE2;
    static fabgl::VirtualKey VK_ESPECTRUM_TAB;
    static fabgl::VirtualKey VK_ESPECTRUM_GRAVEACCENT;

    // Audio
    static void BeeperGetSample();
    static void AYGetSample();
    static void COVOXGetSample();
    static uint8_t audioBuffer[ESP_AUDIO_SAMPLES_PENTAGON];
    static uint8_t SamplebufCOVOX[ESP_AUDIO_SAMPLES_PENTAGON];
    static uint32_t audbufcntCOVOX;
    static uint32_t faudbufcntCOVOX;
    static uint8_t covoxData[4];
    static uint16_t covoxMix;
    static uint16_t fcovoxMix;
    static uint32_t* overSamplebuf;
    static unsigned char audioSampleDivider;
    static unsigned char audioAYDivider;
    static unsigned char audioCOVOXDivider;
    static unsigned char audioOverSampleDivider;
    static signed char aud_volume;
    static uint32_t audbufcnt;
    // static uint32_t faudbufcnt;
    static uint32_t audbufcntover;
    static uint32_t faudbufcntover;
    static uint32_t audbufcntAY;
    static uint32_t faudbufcntAY;
    static int lastaudioBit;
    static int flastaudioBit;
    static int samplesPerFrame;
    static bool AY_emu;
    static int Audio_freq[4];
    static int audioBitBuf;
    static unsigned char audioBitbufCount;
    static uint8_t aud_active_sources;

    static uint8_t ESP_delay;
    static int sync_cnt;

    static int TapeNameScroller;

    static int64_t ts_start;
    static int64_t target[4];
    static double totalseconds;
    static double totalsecondsnodelay;
    static int64_t elapsed;
    static int64_t idle;
    static int ESPoffset;

    static int ESPtestvar;
    static int ESPtestvar1;
    static int ESPtestvar2;

    static volatile bool vsync;

    static TaskHandle_t audioTaskHandle;

    static bool ps2kbd;
    static bool ps2kbd2;
    static bool ps2mouse;

    static int zxDelay;

    static bool trdos;
    static WD1793 Betadisk;

    static int32_t mouseX;
    static int32_t mouseY;
    static bool mouseButtonL;
    static bool mouseButtonR;

    // static uint32_t sessid;

    static void showMemInfo(const char* caption = "ESPectrum");

private:

    static void audioTask(void* unused);

};

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

unsigned long millis();

inline void delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void delayMicroseconds(int64_t us);

#endif