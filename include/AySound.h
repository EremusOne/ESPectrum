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

#ifndef AySound_h
#define AySound_h

#include "hardconfig.h"
#include "fabgl.h"

class AySound
{
public:
#ifndef USE_AY_SOUND
    static void initialize() {}
    static void update() {}
    static void reset() {}
    static void disable() {}
    static void enable() {}
    static uint8_t getRegisterData() { return 0; }
    static void selectRegister(uint8_t data) {}
    static void setRegisterData(uint8_t data) {}
#else
    static void initialize();

    static void update();

    static void reset();

    static void disable();
    static void enable();

    static uint8_t getRegisterData();
    static void selectRegister(uint8_t data);
    static void setRegisterData(uint8_t data);

    static SquareWaveformGenerator _channel[3];

private:
    static uint8_t finePitchChannelA;
    static uint8_t coarsePitchChannelA;
    static uint8_t finePitchChannelB;
    static uint8_t coarsePitchChannelB;
    static uint8_t finePitchChannelC;
    static uint8_t coarsePitchChannelC;
    static uint8_t noisePitch;
    static uint8_t mixer;
    static uint8_t volumeChannelA;
    static uint8_t volumeChannelB;
    static uint8_t volumeChannelC;
    static uint8_t envelopeFineDuration;
    static uint8_t envelopeCoarseDuration;
    static uint8_t envelopeShape;
    static uint8_t ioPortA;

    // Status
    static uint8_t selectedRegister;
    static uint8_t channelVolume[3];
    static uint16_t channelFrequency[3];
#endif
};

#endif // AySound_h