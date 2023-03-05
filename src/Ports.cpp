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
#include "AySound.h"
#include "ESPectrum.h"
#include "Tape.h"
#include "CPU.h"
#include "Z80_JLS/z80.h"
#include "Video.h"
#include "Config.h"

// Ports
volatile uint8_t Ports::base[128];

///////////////////////////////////////////////////////////////////////////////
// Port Contention
///////////////////////////////////////////////////////////////////////////////
static void ALUContentEarly( uint16_t port )
{
    uint8_t page = port >> 14;
    if ((page == 1) || ((!Z80Ops::is48) && (page == 3) && (MemESP::bankLatch & 0x01 != 0)))
        VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 1);
    else
        VIDEO::Draw(1);
}

static void ALUContentLate( uint16_t port )
{

  if( (port & 0x0001) == 0x00) {
        VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 3);
  } else {
    uint8_t page = port >> 14;   
    if ((page == 1) || ((!Z80Ops::is48) && (page == 3) && (MemESP::bankLatch & 0x01 != 0))) {
        VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 1);
        VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 1);
        VIDEO::Draw(Z80Ops::delayContention(CPU::tstates) + 1);
    } else {
        VIDEO::Draw(3);
	}
  }

}

uint8_t Ports::input(uint8_t portLow, uint8_t portHigh)
{

    uint16_t address = portHigh << 8 | portLow;

    ALUContentEarly(address);
    ALUContentLate(address);

    if ((portLow == 0x1f) && (Config::joystick)) return base[0x1f]; // Kempston port

    if (portLow == 0xfe) // ULA PORT
    {
        // all result bits initially set to 1, may be set to 0 eventually
        uint8_t result = 0xbf;

        // Keyboard
        if (~(portHigh | 0xFE)&0xFF) result &= base[0];
        if (~(portHigh | 0xFD)&0xFF) result &= base[1];
        if (~(portHigh | 0xFB)&0xFF) result &= base[2];
        if (~(portHigh | 0xF7)&0xFF) result &= base[3];
        if (~(portHigh | 0xEF)&0xFF) result &= base[4];
        if (~(portHigh | 0xDF)&0xFF) result &= base[5];
        if (~(portHigh | 0xBF)&0xFF) result &= base[6];
        if (~(portHigh | 0x7F)&0xFF) result &= base[7];

        if (Tape::tapeStatus==TAPE_LOADING) {
            bitWrite(result,6,Tape::TAP_Read());
        } 
        
        result |= (0xa0); // ISSUE 2

        return result;

    }
    
    // Sound (AY-3-8912)
    if (address == 0xfffd)  // 0xFFFD
    {
        return AySound::getRegisterData();
    }

    uint8_t data = VIDEO::getFloatBusData();
    
    // if (((address & 0x8002) == 0) && (!Z80Ops::is48)) {
    if ((address == 0x7ffd) && (!Z80Ops::is48)) {        

        // //  Solo en el modelo 128K, pero no en los +2/+2A/+3, si se lee el puerto
        // //  0x7ffd, el valor leído es reescrito en el puerto 0x7ffd.
        // //  http://www.speccy.org/foro/viewtopic.php?f=8&t=2374
        if (!MemESP::pagingLock) {
            MemESP::pagingLock = bitRead(data, 5);
            MemESP::bankLatch = data & 0x7;
            MemESP::videoLatch = bitRead(data, 3);
            VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;        
            MemESP::romLatch = bitRead(data, 4);
            bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
        }

    }

    return data & 0xff;

}

void Ports::output(uint8_t portLow, uint8_t portHigh, uint8_t data) {
    
    uint16_t address = portHigh << 8 | portLow;
    uint8_t ulaPort = ((address & 1) == 0);

    ALUContentEarly(address);

    // ULA =======================================================================
    if (ulaPort) {

        VIDEO::borderColor = data & 0x07;
        VIDEO::brd = VIDEO::border32[VIDEO::borderColor];

        // if (Tape::SaveStatus==TAPE_SAVING)
        //     int Tapebit = bitRead(data,3);
        // else
            int Audiobit = bitRead(data,4);

        // ESPectrum::audioGetSample(Audiobit | Tapebit);
        ESPectrum::audioGetSample(Audiobit);        

    }

    // AY ========================================================================
    if ((ESPectrum::AY_emu) && ((address & 0x8002) == 0x8000)) {
      if ((address & 0x4000) != 0) {
        AySound::selectRegister(data);
      }
      else {
        AySound::setRegisterData(data);
      }
    }

    ALUContentLate(address);
    
    // 128 =======================================================================
    if ((!Z80Ops::is48) && ((address & 0x8002) == 0))
    {

        if (MemESP::pagingLock) return;

        MemESP::pagingLock = bitRead(data, 5);
        MemESP::bankLatch = data & 0x7;
        MemESP::videoLatch = bitRead(data, 3);
        VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;        
        MemESP::romLatch = bitRead(data, 4);
        bitWrite(MemESP::romInUse, 0, MemESP::romLatch);

    }
   
}
