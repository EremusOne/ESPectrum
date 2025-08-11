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

This file includes some contributions from Juanjo Ponteprino

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

#include "ESPectrum.h"
#include "cpuESP.h"
#include "AudioIn.h"
#include "pwm_audio.h"

volatile uint8_t AudioIn::Status = AUDIOIN_STOP;
volatile uint8_t* AudioIn::Buffer = NULL;
volatile int AudioIn::Bufsize = 0;
volatile int AudioIn::sample_index = 0;
double AudioIn::Factor = 0;
uint64_t AudioIn::Basepos = 0;
volatile uint32_t* AudioIn::gpio_in;
volatile uint32_t AudioIn::gpio_mask;

extern "C" void IRAM_ATTR AudioInGetAudio();

void IRAM_ATTR AudioInGetAudio() {
    if (AudioIn::Status == AUDIOIN_PLAY) {
        AudioIn::Buffer[AudioIn::sample_index++] = (*AudioIn::gpio_in & AudioIn::gpio_mask) != 0;
        // AudioIn::Buffer[AudioIn::sample_index++] = (*AudioIn::gpio_in & AudioIn::gpio_mask) == 0; // Inverted polarity
        if (AudioIn::sample_index >= AudioIn::Bufsize) AudioIn::sample_index = 0;
    }
}

void AudioIn::GPIOSetup() {

    if (Config::AudioInGPIO != 0) {

        if (Config::AudioInGPIO < 32) {
            gpio_in = &GPIO.in;
            gpio_mask = 1 << Config::AudioInGPIO;
        } else {
            gpio_in = &GPIO.in1.val;
            gpio_mask = 1 << (Config::AudioInGPIO - 32);
        }

        gpio_set_direction((gpio_num_t)Config::AudioInGPIO, (gpio_mode_t)GPIO_MODE_INPUT);

    }

}

void AudioIn::Init() {

    GPIOSetup();

    Buffer = (volatile uint8_t*) heap_caps_malloc(ESP_AUDIO_SAMPLES_PENTAGON << 1, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);

}

void AudioIn::Start() {
    Factor = (double) ESPectrum::samplesPerFrame / (double) CPU::statesInFrame;
    Bufsize = ESPectrum::samplesPerFrame << 1;
}

void AudioIn::Play() {
    memset((void*)Buffer, 0, Bufsize);
    sample_index = ESPectrum::samplesPerFrame;
    Basepos = 0;
    Status = AUDIOIN_PLAY;
}

void AudioIn::Pause() {
    if (Status == AUDIOIN_PLAY) Status = AUDIOIN_PAUSE;
}

void AudioIn::Stop() {
    Status = AUDIOIN_STOP;
}

void AudioIn::Resume() {
    if (Status == AUDIOIN_PAUSE) Play();
}

void AudioIn::PrepareFrame() {
    Basepos += CPU::statesInFrame;
}

uint8_t IRAM_ATTR AudioIn::GetLevel() {
    return Buffer[(int)((Basepos + CPU::tstates) * Factor) % Bufsize];
}