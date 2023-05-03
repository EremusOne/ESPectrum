///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
//
// I/O PORTS EMULATION
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

#include "Ports.h"
#include "Config.h"
#include "ESPectrum.h"
#include "Z80_JLS/z80.h"
#include "MemESP.h"
#include "Video.h"
#include "AySound.h"
#include "Tape.h"

#pragma GCC optimize ("O3")

//
// TO DO: CONSIDER THIS VALUES IN THE FUTURE FOR SPEAKER, EAR, MIC COMBINATIONS
//
// reg[7:0] ula;
// always @(*) case({ speaker, ear, mic })
//   0: ula <= 8'h00;
//   1: ula <= 8'h24;
//   2: ula <= 8'h40;
//   3: ula <= 8'h64;
//   4: ula <= 8'hB8;
//   5: ula <= 8'hC0;
//   6: ula <= 8'hF8;
//   7: ula <= 8'hFF;
// endcase

#define SPEAKER_VOLUME 97
int sp_volt[4]={ 0, (int) (SPEAKER_VOLUME * 0.11f), (int) (SPEAKER_VOLUME * 0.96f), SPEAKER_VOLUME };

static int TapeBit = 0;

uint8_t Ports::port[128];

uint8_t IRAM_ATTR Ports::input(uint16_t address)
{

    // ** I/O Contention (Early) *************************
    VIDEO::Draw(1, MemESP::ramContended[address >> 14]);
    // ** I/O Contention (Early) *************************
    
    // ** I/O Contention (Late) **************************
    if ((address & 0x0001) == 0) {
        VIDEO::Draw(3, true);
    } else {
        if (MemESP::ramContended[address >> 14]) {
            VIDEO::Draw(1, true);
            VIDEO::Draw(1, true);
            VIDEO::Draw(1, true);        
        } else VIDEO::Draw(3, false);
    }
    // ** I/O Contention (Late) **************************

    if (((address & 0xff) == 0x1f) && (Config::joystick)) return port[0x1f]; // Kempston port

    if ((address & 0xff) == 0xfe) // ULA PORT    
    {
        
        uint8_t result = 0xbf;

        uint8_t portHigh = ~(address >> 8) & 0xff;
        for (int row = 0, mask = 0x01; row < 8; row++, mask <<= 1) {
            if ((portHigh & mask) != 0) {
                result &= port[row];
            }
        }

        if (Tape::tapeStatus==TAPE_LOADING) {
            TapeBit = Tape::TAP_Read();
            bitWrite(result,6,TapeBit);
        }

        return result | (0xa0); // OR 0xa0 -> ISSUE 2
    
    }
    
    // Sound (AY-3-8912)
    if (ESPectrum::AY_emu) {
        if ( (((address >> 8) & 0xC0) == 0xC0) && (((address & 0xff) & 0x02) == 0x00) )
            return AySound::getRegisterData();
    }

    uint8_t data = VIDEO::getFloatBusData();
    
    if (((address & 0x8002) == 0) && (!Z80Ops::is48)) {
    // if ((address == 0x7ffd) && (!Z80Ops::is48)) {        

        // //  Solo en el modelo 128K, pero no en los +2/+2A/+3, si se lee el puerto
        // //  0x7ffd, el valor leído es reescrito en el puerto 0x7ffd.
        // //  http://www.speccy.org/foro/viewtopic.php?f=8&t=2374
        if (!MemESP::pagingLock) {
            MemESP::pagingLock = bitRead(data, 5);
            MemESP::bankLatch = data & 0x7;
            MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];
            MemESP::ramContended[3] = MemESP::bankLatch & 0x01 ? true: false;
            MemESP::videoLatch = bitRead(data, 3);
            VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;        
            MemESP::romLatch = bitRead(data, 4);
            bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
            MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];            
        }

    }

    return data & 0xff;

}

void IRAM_ATTR Ports::output(uint16_t address, uint8_t data) {    
    
    // ** I/O Contention (Early) *************************
    VIDEO::Draw(1, MemESP::ramContended[address >> 14]);
    // ** I/O Contention (Early) *************************

    // ULA =======================================================================
    if ((address & 0x0001) == 0) {

        if (VIDEO::borderColor != data & 0x07) {
            VIDEO::borderColor = data & 0x07;
            VIDEO::Draw(0,true);
            VIDEO::brd = VIDEO::border32[VIDEO::borderColor];
        }
    
        if (Tape::tapeStatus==TAPE_LOADING)
            if (TapeBit) data |= 0x00001000; else data &= 0b11110111;
            // if (TapeBit) data |= 0x10;
        
        int Audiobit = sp_volt[data >> 3 & 3];
        if (Audiobit != ESPectrum::lastaudioBit) {
            ESPectrum::BeeperGetSample(Audiobit);
            ESPectrum::lastaudioBit = Audiobit;
        }

    }


    //ay chip
    /*
    El circuito de sonido contiene dieciseis registros; para seleccionarlos, primero se escribe
    el numero de registro en la puerta de escritura de direcciones, FFFDh (65533), y despues
    lee el valor del registro (en la misma direccion) o se escribe en la direccion de escritura
    de registros de datos, BFFDh (49149). Una vez seleccionado un registro, se puede realizar
    cualquier numero de operaciones de lectura o escritura de datos. S~1o habr~ que volver
    escribir en la puerta de escritura de direcciones cuando se necesite seleccio~ar otro registro.
    La frecuencia de reloj basica de este circuito es 1.7734 MHz (con precision del 0.01~~o).
    */
        //el comportamiento de los puertos ay es con mascaras AND... esto se ve asi en juegos como chase hq
    /*
    Peripheral: 128K AY Register.
    Port: 11-- ---- ---- --0-
    Peripheral: 128K AY (Data).
    Port: 10-- ---- ---- --0-
    49154=1100000000000010
    */

	// // Puertos Chip AY
	// if ( ((address & 49154) == 49152) || ((address & 49154) == 32768) ) {
		
    //     uint16_t puerto_final=address;

	// 	if ((address & 49154) == 49152) puerto_final=65533;
	// 	else if ((address & 49154) == 32768) puerto_final=49149;

    //     if (puerto_final==65533) {
    //         AySound::selectRegister(data);
    //     } else
    //     if (puerto_final==49149) {
    //         ESPectrum::AYGetSample();
    //         AySound::setRegisterData(data);
    //     }

	// }

    // // Handle AY-commands.
    // // Port 0xFFFD selects a AY register.
    // // The port is partially decoded: Bit 1 must be reset and bits 14-15 set.
    // if (ESPectrum::AY_emu) {
    //     if ((address & 0xC002) == 0xC000)
    //     {
    //         AySound::selectRegister(data);
    //     }
    //     else
    //     // Port 0xBFFD writes to the selected register.
    //     // The port is partially decoded: Bit 1 must be reset and bit 15 set.
    //     if ((address & 0x8002) == 0x8000)
    //     {
    //         ESPectrum::AYGetSample();
    //         AySound::setRegisterData(data);
    //     }
    // }

    // AY ========================================================================
    if ((ESPectrum::AY_emu) && ((address & 0x8002) == 0x8000)) {
      if ((address & 0x4000) != 0)
        AySound::selectRegister(data);
      else {
        ESPectrum::AYGetSample();
        AySound::setRegisterData(data);
      }
    }

    // ** I/O Contention (Late) **************************
    if ((address & 0x0001) == 0) {
        VIDEO::Draw(3, true);
    } else {
        if (MemESP::ramContended[address >> 14]) {
            VIDEO::Draw(1, true);
            VIDEO::Draw(1, true);
            VIDEO::Draw(1, true);        
        } else VIDEO::Draw(3, false);
    }
    // ** I/O Contention (Late) **************************
    
    // 128 =======================================================================
    if ((!Z80Ops::is48) && ((address & 0x8002) == 0)) {

        if (MemESP::pagingLock) return;

        MemESP::pagingLock = bitRead(data, 5);
        
        MemESP::bankLatch = data & 0x7;
        MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];
        MemESP::ramContended[3] = MemESP::bankLatch & 0x01 ? true: false;

        MemESP::videoLatch = bitRead(data, 3);
        
        VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;        
        
        MemESP::romLatch = bitRead(data, 4);
        bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
        MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];

    }
   
}
