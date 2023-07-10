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

#include "Ports.h"
#include "Config.h"
#include "ESPectrum.h"
#include "Z80_JLS/z80.h"
#include "MemESP.h"
#include "Video.h"
#include "AySound.h"
#include "Tape.h"
#include "CPU.h"

#pragma GCC optimize ("O3")

// Values calculated for BEEPER, EAR, MIC bit mask (values 0-7)
// Taken from FPGA values suggested by Rampa
//   0: ula <= 8'h00;
//   1: ula <= 8'h24;
//   2: ula <= 8'h40;
//   3: ula <= 8'h64;
//   4: ula <= 8'hB8;
//   5: ula <= 8'hC0;
//   6: ula <= 8'hF8;
//   7: ula <= 8'hFF;
// and adjusted for BEEPER_MAX_VOLUME = 97
uint8_t speaker_values[8]={ 0, 19, 34, 53, 97, 101, 130, 134 };

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

    // Kempston Joystick
    if ((Config::joystick) && ((address & 0x00E0) == 0 || (address & 0xFF) == 0xDF)) return port[0x1f];

    // ULA PORT    
    if ((address & 0x0001) == 0) {
        
        uint8_t result = 0xbf;

        uint8_t portHigh = ~(address >> 8) & 0xff;
        for (int row = 0, mask = 0x01; row < 8; row++, mask <<= 1) {
            if ((portHigh & mask) != 0) {
                result &= port[row];
            }
        }

        if (Tape::tapeStatus==TAPE_LOADING) {
            Tape::TAP_Read();
            bitWrite(result,6,Tape::tapeEarBit);
        }

        return result | (0xa0); // OR 0xa0 -> ISSUE 2
    
    }
    
    // Sound (AY-3-8912)
    if (ESPectrum::AY_emu) {
        if ((address & 0xC002) == 0xC000)
            return AySound::getRegisterData();
    }

    uint8_t data = VIDEO::getFloatBusData();
    
    if ((!Z80Ops::is48) && ((address & 0x8002) == 0)) {

        // //  Solo en el modelo 128K, pero no en los +2/+2A/+3, si se lee el puerto
        // //  0x7ffd, el valor leído es reescrito en el puerto 0x7ffd.
        // //  http://www.speccy.org/foro/viewtopic.php?f=8&t=2374
        if (!MemESP::pagingLock) {
            MemESP::pagingLock = bitRead(data, 5);
            MemESP::bankLatch = data & 0x7;
            MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];
            MemESP::ramContended[3] = MemESP::bankLatch & 0x01 ? true: false;
            if (MemESP::videoLatch != bitRead(data, 3)) {
                MemESP::videoLatch = bitRead(data, 3);
                // This, if not using the ptime128 draw version, fixs ptime and ptime128
                if (((address & 0x0001) != 0) && (MemESP::ramContended[address >> 14])) {
                    VIDEO::Draw(2, false);
                    CPU::tstates -= 2;
                }
                VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;
            }
            MemESP::romLatch = bitRead(data, 4);
            bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
            MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];            
        }

    }

    return data & 0xff;

}

void IRAM_ATTR Ports::output(uint16_t address, uint8_t data) {    
    
    int Audiobit;

    // ** I/O Contention (Early) *************************
    VIDEO::Draw(1, MemESP::ramContended[address >> 14]);
    // ** I/O Contention (Early) *************************

    // ULA =======================================================================
    if ((address & 0x0001) == 0) {

        // Border color
        if (VIDEO::borderColor != data & 0x07) {
            VIDEO::borderColor = data & 0x07;
            VIDEO::Draw(0,true);
            VIDEO::brd = VIDEO::border32[VIDEO::borderColor];
        }
    
        // Beeper Audio
        Audiobit = speaker_values[((data >> 2) & 0x04 ) | (Tape::tapeEarBit << 1) | ((data >> 3) & 0x01)];
        if (Audiobit != ESPectrum::lastaudioBit) {
            ESPectrum::BeeperGetSample(Audiobit);
            ESPectrum::lastaudioBit = Audiobit;
        }

    }

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
        // printf("Case 1\n");
    } else {
        if (MemESP::ramContended[address >> 14]) {
            VIDEO::Draw(1, true);
            VIDEO::Draw(1, true);
            VIDEO::Draw(1, true);        
            // printf("Case 2\n");
        } else {
            // printf("Case 3\n");
            VIDEO::Draw(3, false);
        }
    }
    // ** I/O Contention (Late) **************************

    // 128 =======================================================================
    if ((!Z80Ops::is48) && ((address & 0x8002) == 0)) {

        if (!MemESP::pagingLock) {

            MemESP::pagingLock = bitRead(data, 5);

            MemESP::bankLatch = data & 0x7;
            MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];
            MemESP::ramContended[3] = MemESP::bankLatch & 0x01 ? true: false;

            MemESP::romLatch = bitRead(data, 4);
            bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
            MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];

            if (MemESP::videoLatch != bitRead(data, 3)) {
                MemESP::videoLatch = bitRead(data, 3);
                // This, if not using the ptime128 draw version, fixs ptime and ptime128
                if (((address & 0x0001) != 0) && (MemESP::ramContended[address >> 14])) {
                    VIDEO::Draw(2, false);
                    CPU::tstates -= 2;
                }
                VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;
            }

        }

    }

}

// void Ports::ContendedIODelay(uint16_t portNumber)
// {

//     uint8_t lowByte = portNumber & 0xff;

//     if (bitRead(lowByte, 0)) {
//         if (Z80Ops::is48) {
//                 if (portNumber >= 0x4000 && portNumber <= 0x7FFF) {
//                     // Template D
//                     VIDEO::Draw(1,true);
//                     VIDEO::Draw(1,true);
//                     VIDEO::Draw(1,true);
//                     VIDEO::Draw(1,true);
//                 } else {
//                     // Template B
//                     VIDEO::Draw(4,false);
//                 }
//         } else {
//                 if ((portNumber >= 0x4000 && portNumber <= 0x7FFF) || ((portNumber >= 0xC000 && portNumber <= 0xFFFF) && (MemESP::ramContended[3]))) {
//                     // Template D
//                     VIDEO::Draw(1,true);
//                     VIDEO::Draw(1,true);
//                     VIDEO::Draw(1,true);
//                     VIDEO::Draw(1,true);
//                 } else {
//                     // Template B
//                     VIDEO::Draw(4,false);
//                 }
//         }
//     } else {
//         if (Z80Ops::is48) {
//                 if (portNumber >= 0x4000 && portNumber <= 0x7FFF) {
//                     // Template C
//                     VIDEO::Draw(1,true);
//                     VIDEO::Draw(3,true);                    
//                 } else {
//                     // Template A
//                     VIDEO::Draw(1,false);
//                     VIDEO::Draw(3,true);                    
//                 }
//         } else {
//                 if ((portNumber >= 0x4000 && portNumber <= 0x7FFF) || ((portNumber >= 0xC000 && portNumber <= 0xFFFF) && (MemESP::ramContended[3]))) {
//                     // Template C
//                     VIDEO::Draw(1,true);
//                     VIDEO::Draw(3,true);                    
//                 } else {
//                     // Template A
//                     VIDEO::Draw(1,false);
//                     VIDEO::Draw(3,true);
//                 }

//         }
//     }
// }
