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

#include "Ports.h"
#include "Config.h"
#include "ESPectrum.h"
#include "Z80_JLS/z80.h"
#include "MemESP.h"
#include "Video.h"
#include "AySound.h"
#include "Tape.h"
#include "cpuESP.h"
#include "wd1793.h"

// #pragma GCC optimize("O3")

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
uint8_t Ports::speaker_values[8]={ 0, 19, 34, 53, 97, 101, 130, 134 };
uint8_t Ports::port[128];
uint8_t Ports::port254 = 0;

uint8_t (*Ports::getFloatBusData)() = &Ports::getFloatBusData48;

uint8_t Ports::getFloatBusData48() {

    // unsigned int currentTstates = CPU::tstates;

	// unsigned int line = (currentTstates / 224) - 64;
	// if (line >= 192) return 0xFF;

	// unsigned char halfpix = (CPU::tstates % 224) - 3;
    // if ((halfpix >= 125) || (halfpix & 0x04)) return 0xFF;

    //int hpoffset = (halfpix >> 2) + ((halfpix >> 1) & 0x01);
    // if (halfpix & 0x01) return(VIDEO::grmem[VIDEO::offAtt[line] + hpoffset]);
    // return(VIDEO::grmem[VIDEO::offBmp[line] + hpoffset]);

	uint32_t line = CPU::tstates / 224;
    if (line < 64 || line >= 256) return 0xFF;

	uint8_t halfpix = CPU::tstates % 224;
    if (halfpix & 0x80) return 0xFF;
    halfpix -= 3;
    if (halfpix & 0x04) return 0xFF;

    line -= 64;
    return (VIDEO::grmem[(halfpix & 0x01 ? VIDEO::offAtt[line] : VIDEO::offBmp[line]) + (halfpix >> 2) + ((halfpix >> 1) & 0x01)]);

}

uint8_t Ports::getFloatBusDataTK() {

    unsigned int currentTstates = CPU::tstates;

	unsigned int line = (currentTstates / 228) - (Config::ALUTK == 1 ? 64 : 38);
	if (line >= 192) return 0xFF;

	unsigned char halfpix = (currentTstates % 228) - 99;
	if ((halfpix >= 125) || (halfpix & 0x04)) return 0xFF;

    // int hpoffset = (halfpix >> 2) + ((halfpix >> 1) & 0x01);
    // if (halfpix & 0x01) return(VIDEO::grmem[VIDEO::offAtt[line] + hpoffset]);
    // return(VIDEO::grmem[VIDEO::offBmp[line] + hpoffset]);

    return (VIDEO::grmem[(halfpix & 0x01 ? VIDEO::offAtt[line] : VIDEO::offBmp[line]) + (halfpix >> 2) + ((halfpix >> 1) & 0x01)]);

}

uint8_t Ports::getFloatBusData128() {

    // unsigned int currentTstates = CPU::tstates - 1;

	// unsigned int line = (currentTstates / 228) - 63;
	// if (line >= 192) return 0xFF;

	// unsigned char halfpix = currentTstates % 228;
	// if ((halfpix >= 128) || (halfpix & 0x04)) return 0xFF;

    // int hpoffset = (halfpix >> 2) + ((halfpix >> 1) & 0x01);
    // if (halfpix & 0x01) return(VIDEO::grmem[VIDEO::offAtt[line] + hpoffset]);
    // return(VIDEO::grmem[VIDEO::offBmp[line] + hpoffset]);

    uint32_t currentTstates = CPU::tstates - 1;

	uint32_t line = currentTstates / 228;
    if (line < 63 || line >= 255) return 0xFF;

	uint8_t halfpix = currentTstates % 228;
	if (halfpix & 0x84) return 0xFF;

    line -= 63;
    return (VIDEO::grmem[(halfpix & 0x01 ? VIDEO::offAtt[line] : VIDEO::offBmp[line]) + (halfpix >> 2) + ((halfpix >> 1) & 0x01)]);

}

const uint8_t contention2[8] = {6, 6, 5, 4, 3, 2, 1, 0};
const uint8_t contention3[129] = {
    6,  6,  12, 12, 11, 10, 9,  8,  7,  6,  12, 12, 11, 10, 9,  8,  7,  6,  12,
    12, 11, 10, 9,  8,  7,  6,  12, 12, 11, 10, 9,  8,  7,  6,  12, 12, 11, 10,
    9,  8,  7,  6,  12, 12, 11, 10, 9,  8,  7,  6,  12, 12, 11, 10, 9,  8,  7,
    6,  12, 12, 11, 10, 9,  8,  7,  6,  12, 12, 11, 10, 9,  8,  7,  6,  12, 12,
    11, 10, 9,  8,  7,  6,  12, 12, 11, 10, 9,  8,  7,  6,  12, 12, 11, 10, 9,
    8,  7,  6,  12, 12, 11, 10, 9,  8,  7,  6,  12, 12, 11, 10, 9,  8,  7,  6,
    12, 12, 11, 10, 9,  8,  7,  6,  6,  6,  5,  4,  3,  2,  1};

uint8_t tkIOcon(uint16_t a) {

    uint32_t t = (CPU::tstates % 228) - 93;
	uint32_t l = (CPU::tstates / 228) - (Config::ALUTK == 1 ? 64 : 38);

    if (t >= 228) {
        t -= 228;
        l++;
        if (l >= (Config::ALUTK == 1 ? 312 : 262)) l = 0;
    }

    if (l < 192) {
        if ((a & 0xc000) == 0x4000) {
            if (t < 129) {
                return contention3[t];
            }
        } else {
            if (a & 0x1)
                return 0;
            else if (t < 128) {
                return contention2[t & 07];
            }
        }
    }

    return 0;

}

IRAM_ATTR uint8_t Ports::input(uint16_t address) {

    uint8_t data;
    uint8_t rambank = address >> 14;    

    VIDEO::Draw(1, MemESP::ramContended[rambank]); // I/O Contention (Early)
    
    // ULA PORT    
    if ((address & 0x0001) == 0) {

        if (Config::arch[0] == 'T' && Config::ALUTK > 0) {
            VIDEO::Draw( 3 + tkIOcon(address), false);
        } else {
            VIDEO::Draw(3, !Z80Ops::isPentagon);   // I/O Contention (Late)
        }

        data = Config::port254default; // For TK90X spanish and rest of machines default port value is 0xBF. For TK90X portuguese is 0x3f.

        uint8_t portHigh = ~(address >> 8) & 0xff;
        for (int row = 0, mask = 0x01; row < 8; row++, mask <<= 1) {
            if ((portHigh & mask) != 0)
                data &= port[row];
        }

        // ** ESPectrum **
        if (Tape::tapeStatus==TAPE_LOADING) {
            Tape::Read();
            bitWrite(data,6,Tape::tapeEarBit);
        } else {
    		if ((Z80Ops::is48) && (Config::Issue2)) // Issue 2 behaviour only on Spectrum 48K
				if (port254 & 0x18) data |= 0x40;
			else
				if (port254 & 0x10) data |= 0x40;
		}

        // ** RVM **
        // if (Tape::tapeStatus==TAPE_LOADING) Tape::Read();
        // if ((Z80Ops::is48) && (Config::Issue2)) // Issue 2 behaviour only on Spectrum 48K
        //     if (port254 & 0x18) data |= 0x40;
        // else
        //     if (port254 & 0x10) data |= 0x40;
        // if (Tape::tapeEarBit) data ^= 0x40;

    } else {

        if (Config::arch[0] == 'T' && Config::ALUTK > 0)
            VIDEO::Draw( 3 + tkIOcon(address), false);
        else
            ioContentionLate(MemESP::ramContended[rambank]);

        // The default port value is 0xFF.
        data = 0xff;

        // Check if TRDOS Rom is mapped.
        if (ESPectrum::trdos) {
            
            // int lowByte = address & 0xFF;

            // // Process Beta Disk instruction.
            // if (lowByte & 0x80) {
            //         data = ESPectrum::Betadisk.ReadSystemReg();
            //         // printf("WD1793 Read Control Register: %d\n",(int)data);
            //         return data;
            // }

            switch (address & 0xFF) {
                case 0xFF:
                    data = ESPectrum::Betadisk.ReadSystemReg();
                    // printf("WD1793 Read Control Register: %d\n",(int)data);
                    return data;
                case 0x1F:
                    data = ESPectrum::Betadisk.ReadStatusReg();
                    // printf("WD1793 Read Status Register: %d\n",(int)data);
                    return data;
                case 0x3F:
                    data = ESPectrum::Betadisk.ReadTrackReg();
                    // printf("WD1793 Read Track Register: %d\n",(int)data);
                    return data;
                case 0x5F:
                    data = ESPectrum::Betadisk.ReadSectorReg();
                    // printf("WD1793 Read Sector Register: %d\n",(int)data);
                    return data;
                case 0x7F:
                    data = ESPectrum::Betadisk.ReadDataReg();                    
                    // printf("WD1793 Read Data Register: %d\n",(int)data);                    
                    return data;                    
            }

        }

        // Kempston Joystick
        if ((Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) && ((address & 0x00E0) == 0 || (address & 0xFF) == 0xDF)) return port[0x1f];

        // Fuller Joystick
        if ((Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) && (address & 0xFF) == 0x7F) return port[0x7f];

        // Sound (AY-3-8912)
        if (ESPectrum::AY_emu) {
            if ((address & 0xC002) == 0xC000)
                return AySound::getRegisterData();
        }

        if (!Z80Ops::isPentagon) {

            data = getFloatBusData();
            
            if ((!Z80Ops::is48) && ((address & 0x8002) == 0)) {

                // //  Solo en el modelo 128K, pero no en los +2/+2A/+3, si se lee el puerto
                // //  0x7ffd, el valor leído es reescrito en el puerto 0x7ffd.
                // //  http://www.speccy.org/foro/viewtopic.php?f=8&t=2374
                if (!MemESP::pagingLock) {

                    MemESP::pagingLock = bitRead(data, 5);

                    if (MemESP::bankLatch != (data & 0x7)) {
                        MemESP::bankLatch = data & 0x7;
                        MemESP::ramCurrent[3] = MemESP::ram[MemESP::bankLatch];
                        MemESP::ramContended[3] = MemESP::bankLatch & 0x01 ? true: false;
                    }

                    if (MemESP::videoLatch != bitRead(data, 3)) {
                        MemESP::videoLatch = bitRead(data, 3);
                        VIDEO::grmem = MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5];
                    }
                    
                    MemESP::romLatch = bitRead(data, 4);
                    bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
                    MemESP::ramCurrent[0] = MemESP::rom[MemESP::romInUse];            
                }

            }

        }

    }

    return data;

}

IRAM_ATTR void Ports::output(uint16_t address, uint8_t data) {    
    
    int Audiobit;
    uint8_t rambank = address >> 14;

    VIDEO::Draw(1, MemESP::ramContended[rambank]); // I/O Contention (Early)

    // ULA =======================================================================
    if ((address & 0x0001) == 0) {

        port254 = data;

        // Border color
        if (VIDEO::borderColor != data & 0x07) {
            
            VIDEO::brdChange = true;
            
            if (!Z80Ops::isPentagon) 
                if (Config::arch[0] == 'T' && Config::ALUTK > 0)
                    VIDEO::Draw(tkIOcon(address),false);
                else            
                    VIDEO::Draw(0,true); // Seems not needed in Pentagon

            VIDEO::DrawBorder();

            VIDEO::borderColor = data & 0x07;
            VIDEO::brd = VIDEO::border32[VIDEO::borderColor];

        }
    
        if (ESPectrum::ESP_delay) { // Disable beeper on turbo mode

            if (Config::tape_player)
                Audiobit = Tape::tapeEarBit ? 255 : 0; // For tape player mode
            else
                // Beeper Audio
                Audiobit = speaker_values[((data >> 2) & 0x04 ) | (Tape::tapeEarBit << 1) | ((data >> 3) & 0x01)];

            if (Audiobit != ESPectrum::lastaudioBit) {
                ESPectrum::BeeperGetSample();
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

            if (Config::arch[0] == 'T' && Config::ALUTK > 0)
                VIDEO::Draw( 3 + tkIOcon(address), false);
            else
                VIDEO::Draw(3, !Z80Ops::isPentagon);   // I/O Contention (Late)
            
            return;

        }

        if (Config::arch[0] == 'T' && Config::ALUTK > 0)
            VIDEO::Draw( 3 + tkIOcon(address), false);
        else
            VIDEO::Draw(3, !Z80Ops::isPentagon);   // I/O Contention (Late)

    } else {

        // AY ========================================================================
        if ((ESPectrum::AY_emu) && ((address & 0x8002) == 0x8000)) {

            if ((address & 0x4000) != 0)
                AySound::selectRegister(data);
            else {
                ESPectrum::AYGetSample();
                AySound::setRegisterData(data);
            }

            if (Config::arch[0] == 'T' && Config::ALUTK > 0)
                VIDEO::Draw( 3 + tkIOcon(address), false);
            else
                ioContentionLate(MemESP::ramContended[rambank]);

            return;

        }

        // Check if TRDOS Rom is mapped.
        if (ESPectrum::trdos) {

            // int lowByte = address & 0xFF;

            switch (address & 0xFF) {
                case 0xFF:
                    // printf("WD1793 Write Control Register: %d\n",data);
                    ESPectrum::Betadisk.WriteSystemReg(data);
                    break;
                case 0x1F:
                    // printf("WD1793 Write Command Register: %d\n",data);
                    ESPectrum::Betadisk.WriteCommandReg(data);
                    break;
                case 0x3F:
                    // printf("WD1793 Write Track Register: %d\n",data);                    
                    ESPectrum::Betadisk.WriteTrackReg(data);
                    break;
                case 0x5F:
                    // printf("WD1793 Write Sector Register: %d\n",data);                    
                    ESPectrum::Betadisk.WriteSectorReg(data);
                    break;
                case 0x7F:
                    // printf("WD1793 Write Data Register: %d\n",data);
                    ESPectrum::Betadisk.WriteDataReg(data);
                    break;
            }

        }

        if (Config::arch[0] == 'T' && Config::ALUTK > 0)
            VIDEO::Draw( 3 + tkIOcon(address), false);
        else
            ioContentionLate(MemESP::ramContended[rambank]);

    }

    // 128 / PENTAGON ==================================================================
    if ((!Z80Ops::is48) && ((address & 0x8002) == 0)) {

        if (!MemESP::pagingLock) {

            MemESP::pagingLock = bitRead(data, 5);

            if (MemESP::bankLatch != (data & 0x7)) {
                MemESP::bankLatch = data & 0x7;
                #ifdef ESPECTRUM_PSRAM
                MemESP::tm_bank_chg[MemESP::bankLatch] = true; // Bank selected. Mark for time machine
                #endif
                MemESP::ramCurrent[3] = MemESP::ram[MemESP::bankLatch];
                MemESP::ramContended[3] = Z80Ops::isPentagon ? false : (MemESP::bankLatch & 0x01 ? true: false);
            }

            MemESP::romLatch = bitRead(data, 4);
            bitWrite(MemESP::romInUse, 0, MemESP::romLatch);
            MemESP::ramCurrent[0] = MemESP::rom[MemESP::romInUse];

            if (MemESP::videoLatch != bitRead(data, 3)) {
                MemESP::videoLatch = bitRead(data, 3);
                #ifdef ESPECTRUM_PSRAM
                MemESP::tm_bank_chg[MemESP::videoLatch ? 7 : 5] = true; // Bank selected. Mark for time machine
                #endif
                VIDEO::grmem = MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5];
            }

        }

    }

}

IRAM_ATTR void Ports::ioContentionLate(bool contend) {

    if (contend) {
        VIDEO::Draw(1, true);
        VIDEO::Draw(1, true);
        VIDEO::Draw(1, true);        
    } else {
        VIDEO::Draw(3, false);
    }

}