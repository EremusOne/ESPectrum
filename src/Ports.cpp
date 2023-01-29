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
#include "hardconfig.h"
#include "Ports.h"
#include "MemESP.h"
//#include "AySound.h"
#include "ESPectrum.h"
//#include "Tape.h"
#include "CPU.h"

// Ports
volatile uint8_t Ports::base[128];
volatile uint8_t Ports::wii[128];

static uint8_t port_data = 0;

// using partial port address decoding
// see https://worldofspectrum.org/faq/reference/ports.htm
//
//        Peripheral: 48K ULA
//        Port: ---- ---- ---- ---0
//
//        Peripheral: Kempston Joystick.
//        Port: ---- ---- 000- ---- 
//
//        Peripheral: 128K AY Register
//        Port: 11-- ---- ---- --0-
//
//        Peripheral: 128K AY (Data)
//        Port: 10-- ---- ---- --0-
//
//        Peripheral: ZX Spectrum 128K / +2 Memory Control
//        Port: 0--- ---- ---- --0-
//
//        Peripheral: ZX Spectrum +2A / +3 Primary Memory Control
//        Port: 01-- ---- ---- --0-
//
//        Peripheral: ZX Spectrum +2A / +3 Secondary Memory Control
//        Port: 0001 ---- ---- --0-
//
uint8_t Ports::input(uint8_t portLow, uint8_t portHigh)
{
    
    // uint32_t ts_start = micros();
    
    // 48K ULA
    if ((portLow & 0x01) == 0x00) // (portLow == 0xFE) 
    {
        // all result bits initially set to 1, may be set to 0 eventually
        uint8_t result = 0xbF;

        #ifdef EAR_PRESENT
        // EAR_PIN
        if (portHigh == 0xFE) {
            bitWrite(result, 6, digitalRead(EAR_PIN));
        }
        #endif

        // Keyboard
        if (~(portHigh | 0xFE)&0xFF) result &= (base[0] & wii[0]);
        if (~(portHigh | 0xFD)&0xFF) result &= (base[1] & wii[1]);
        if (~(portHigh | 0xFB)&0xFF) result &= (base[2] & wii[2]);
        if (~(portHigh | 0xF7)&0xFF) result &= (base[3] & wii[3]);
        if (~(portHigh | 0xEF)&0xFF) result &= (base[4] & wii[4]);
        if (~(portHigh | 0xDF)&0xFF) result &= (base[5] & wii[5]);
        if (~(portHigh | 0xBF)&0xFF) result &= (base[6] & wii[6]);
        if (~(portHigh | 0x7F)&0xFF) result &= (base[7] & wii[7]);

        // if (Tape::tapeStatus==TAPE_LOADING) {
            
        //     bitWrite(result,6,Tape::TAP_Read());
            
        //     /*
        //     uint32_t ts_end = micros();
        //     uint32_t elapsed = ts_end - ts_start;
            
        //     static int ctr = 0;
        //     if (ctr == 0) {
        //     ctr = 8192;
        //         Serial.printf("[TAPE] elapsed: %u\n", elapsed);
        //     }
        //     else ctr--;
        //     */
           
        // } else {
            if (base[0x20] & 0x18) result |= (0xe0); else result |= (0xa0); // ISSUE 2 behaviour
        // }

        return result;

    }

    #ifdef PS2_ARROWKEYS_AS_KEMPSTON
    // Kempston
    if ((portLow & 0xE0) == 0x00) // (portLow == 0x1F)
    {
        return base[0x1f];
    }
    #endif
    
    // Sound (AY-3-8912)
    #ifdef USE_AY_SOUND
    if ((portHigh & 0xC0) == 0xC0 && (portLow & 0x02) == 0x00)  // 0xFFFD
    {
        return AySound::getRegisterData();
    }
    #endif

    uint8_t data = port_data;
    data |= (0xe0); /* Set bits 5-7 - as reset above */
    data &= ~0x40;
    // Serial.printf("Port %x%x  Data %x\n", portHigh,portLow,data);
    return data;
}

int Audiobit, Tapebit;

void Ports::output(uint8_t portLow, uint8_t portHigh, uint8_t data) {
    // Serial.printf("%02X,%02X:%02X|", portHigh, portLow, data);

    // 48K ULA
    if ((portLow & 0x01) == 0x00)
    {
        CPU::borderColor = data & 0x07;

        #ifdef SPEAKER_PRESENT

        // if (Tape::SaveStatus==TAPE_SAVING)
        //     Tapebit = bitRead(data,3);
        // else
             Audiobit = bitRead(data,4);

        ESPectrum::audioGetSample(Audiobit | Tapebit);

        #endif

        #ifdef MIC_PRESENT
        digitalWrite(MIC_PIN, bitRead(data, 3)); // tape_out
        #endif
        base[0x20] = data; // ?
    }
    
    if ((portLow & 0x02) == 0x00)
    {
        // 128K AY
        #ifdef USE_AY_SOUND
        if ((portHigh & 0x80) == 0x80)
        {
            if ((portHigh & 0x40) == 0x40)
                AySound::selectRegister(data);
            else
                AySound::setRegisterData(data);
        }
        #endif

        // will decode both
        // 128K / +2 Memory Control
        // +2A / +3 Memory Control
        if ((portHigh & 0xC0) == 0x40)
        {
            if (!MemESP::pagingLock) {
                MemESP::pagingLock = bitRead(data, 5);
                MemESP::romLatch = bitRead(data, 4);
                MemESP::videoLatch = bitRead(data, 3);
                MemESP::bankLatch = data & 0x7;
                bitWrite(MemESP::romInUse, 1, MemESP::romSP3);
                bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
            }
        }
        
        // +2A / +3 Secondary Memory Control
        if ((portHigh & 0xF0) == 0x01)
        {
            MemESP::modeSP3 = bitRead(data, 0);
            MemESP::romSP3 = bitRead(data, 2);
            bitWrite(MemESP::romInUse, 1, MemESP::romSP3);
            bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
        }

    }
    
}
