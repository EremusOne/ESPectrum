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

#include "MemESP.h"
#include "Config.h"
#include "Video.h"
#include "Z80_JLS/z80.h"
#include <stddef.h>
#include "esp_heap_caps.h"

#include <cstring>

using namespace std;

uint8_t* MemESP::rom[5];

uint8_t* MemESP::ram[8] = { NULL };

#ifdef ESPECTRUM_PSRAM
uint32_t* MemESP::timemachine[TIME_MACHINE_SLOTS][8];
uint8_t MemESP::tm_slotbanks[TIME_MACHINE_SLOTS][8];
slotdata MemESP::tm_slotdata[TIME_MACHINE_SLOTS];
bool MemESP::tm_bank_chg[8];
uint8_t MemESP::cur_timemachine = 0;
int MemESP::tm_framecnt = 0;
bool MemESP::tm_loading_slot = false;
#endif

uint8_t* MemESP::ramCurrent[4];
bool MemESP::ramContended[4];

uint8_t MemESP::bankLatch = 0;
uint8_t MemESP::videoLatch = 0;
uint8_t MemESP::romLatch = 0;
uint8_t MemESP::pagingLock = 0;
uint8_t MemESP::romInUse = 0;

bool MemESP::SPRom = false;

bool MemESP::Init() {

    #ifdef ESPECTRUM_PSRAM

    // Video pages in SRAM (faster)
    MemESP::ram[5] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[7] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);

    // Rest of pages in PSRAM
    MemESP::ram[0] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    MemESP::ram[2] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    MemESP::ram[1] = (unsigned char *) heap_caps_calloc(0x8000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    MemESP::ram[3] = ((unsigned char *) MemESP::ram[1]) + 0x4000;
    MemESP::ram[4] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    MemESP::ram[6] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    // Allocate time machine RAM
    for (int i=0;i < TIME_MACHINE_SLOTS; i++)
        for (int n=0; n < 8; n++)
            MemESP::timemachine[i][n] = (uint32_t *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    #else

    MemESP::ram[5] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[0] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[2] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[7] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[1] = (unsigned char *) heap_caps_calloc(0x8000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[3] = ((unsigned char *) MemESP::ram[1]) + 0x4000;
    MemESP::ram[4] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);
    MemESP::ram[6] = (unsigned char *) heap_caps_calloc(0x4000, sizeof(unsigned char), MALLOC_CAP_8BIT);

    #endif

    for (int i=0; i < 8; i++) {
        if (MemESP::ram[i] == NULL) {
            if (Config::slog_on) printf("ERROR! Unable to allocate ram%d\n",i);
            return false;
        }
    }

    return true;

}

void MemESP::Reset() {

    // Set memory to 0
    for (int i=0; i < 8; i++)
        memset(MemESP::ram[i],0,0x4000);

    MemESP::romInUse = 0;
    MemESP::bankLatch = 0;
    MemESP::videoLatch = 0;
    MemESP::romLatch = 0;

    MemESP::ramCurrent[0] = MemESP::rom[0];
    MemESP::ramCurrent[1] = MemESP::ram[5];
    MemESP::ramCurrent[2] = MemESP::ram[2];
    MemESP::ramCurrent[3] = MemESP::ram[0];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = Config::arch == "Pentagon" ? false : true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = false;

    MemESP::pagingLock = Config::arch == "48K" || Config::arch == "TK90X" || Config::arch == "TK95" ? 1 : 0;

    MemESP::SPRom = false;

    #ifdef ESPECTRUM_PSRAM
    Tm_Init();
    #endif

}

#ifdef ESPECTRUM_PSRAM

void MemESP::Tm_Init() {
    
    if (tm_loading_slot) return;
    
    // Init time machine

    for (int i=0; i < 8; i++) MemESP::tm_bank_chg[i] = !MemESP::pagingLock;
    
    if (MemESP::pagingLock) {
        MemESP::tm_bank_chg[0] = true;
        MemESP::tm_bank_chg[2] = true;
        MemESP::tm_bank_chg[5] = true;        
    }
    
    for (int i=0;i < TIME_MACHINE_SLOTS; i++)
        for (int n=0; n < 8; n++)
            MemESP::tm_slotbanks[i][n]=0xff;

    cur_timemachine = 0;  
    tm_framecnt = 0;      
    
}

void MemESP::Tm_Load(uint8_t slot) {

    tm_loading_slot = true;
    
    // TO DO:
    // tstates, globaltstates, disk or tape status, whatever else i figure out

    // Read in the registers
    Z80::setRegI(tm_slotdata[slot].RegI);

    Z80::setRegHLx(tm_slotdata[slot].RegHLx);
    Z80::setRegDEx(tm_slotdata[slot].RegDEx);
    Z80::setRegBCx(tm_slotdata[slot].RegBCx);
    Z80::setRegAFx(tm_slotdata[slot].RegAFx);

    Z80::setRegHL(tm_slotdata[slot].RegHL);
    Z80::setRegDE(tm_slotdata[slot].RegDE);
    Z80::setRegBC(tm_slotdata[slot].RegBC);

    Z80::setRegIY(tm_slotdata[slot].RegIY);
    Z80::setRegIX(tm_slotdata[slot].RegIX);

    uint8_t inter = tm_slotdata[slot].inter;
    Z80::setIFF2(inter & 0x04 ? true : false);
    Z80::setIFF1(Z80::isIFF2());
    Z80::setRegR(tm_slotdata[slot].RegR);

    Z80::setRegAF(tm_slotdata[slot].RegAF);
    Z80::setRegSP(tm_slotdata[slot].RegSP);

    Z80::setIM((Z80::IntMode)tm_slotdata[slot].IM);

    VIDEO::borderColor = tm_slotdata[slot].borderColor;
    VIDEO::brd = VIDEO::border32[VIDEO::borderColor];

    Z80::setRegPC(tm_slotdata[slot].RegPC);

    bool tr_dos = tm_slotdata[slot].trdos;  // Check if TR-DOS is paged

    MemESP::videoLatch = tm_slotdata[slot].videoLatch;
    MemESP::romLatch = tm_slotdata[slot].romLatch;
    MemESP::pagingLock = tm_slotdata[slot].pagingLock;
    MemESP::bankLatch = tm_slotdata[slot].bankLatch;
    
    if (tr_dos) {
        MemESP::romInUse = 4;
        ESPectrum::trdos = true;            
    } else {
        MemESP::romInUse = MemESP::romLatch;
        ESPectrum::trdos = false;
    }

    MemESP::ramCurrent[0] = MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[3] = MemESP::ram[MemESP::bankLatch];
    MemESP::ramContended[3] = Z80Ops::isPentagon ? false : (MemESP::bankLatch & 0x01 ? true: false);

    VIDEO::grmem = MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5];

    // Read memory banks
    for (int n=0; n < 8; n++) {   
        if (tm_slotbanks[slot][n] != 0xff) {
            printf("Loaded slot %d\n",n);
            uint32_t* dst32 = (uint32_t *)MemESP::ram[n];
            for (int i=0; i < 0x1000; i++)
                // dst32[i] = MemESP::timemachine[tm_slotbanks[slot][n]][n][i];
                dst32[i] = MemESP::timemachine[slot][n][i];
        }
    }

    tm_framecnt = 0;

    tm_loading_slot = false;

};

void MemESP::Tm_DoTimeMachine() {

    if (tm_framecnt++ < 250) return; // One "snapshot" every 250 frames (~5 seconds on 50hz machines)

    // printf("Time machine\n================\n");

    for (int n=0; n < 8; n++) { // Save active RAM banks during slot period
        
        if (n == 2 || MemESP::tm_bank_chg[n]) { // Bank 2 is always copied because is always active

            // printf("Copying bank %d\n",n);

            // Copy bank
            uint32_t* src32 = (uint32_t *)MemESP::ram[n];
            for (int i=0; i < 0x1000; i++)
                MemESP::timemachine[MemESP::cur_timemachine][n][i] = src32[i];

            // Mark as inactive if is not current bank latched or current videobank
            if (n != MemESP::bankLatch && n != (MemESP::videoLatch ? 7 : 5))
                MemESP::tm_bank_chg[n]=false;
            
            // Register copied bank as current into slot bank list
            MemESP::tm_slotbanks[MemESP::cur_timemachine][n] = MemESP::cur_timemachine;

        }

    }

    MemESP::tm_slotdata[MemESP::cur_timemachine].RegI = Z80::getRegI();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegHLx = Z80::getRegHLx();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegDEx = Z80::getRegDEx();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegBCx = Z80::getRegBCx();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegAFx = Z80::getRegAFx();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegHL = Z80::getRegHL();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegDE = Z80::getRegDE();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegBC = Z80::getRegBC();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegIY = Z80::getRegIY();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegIX = Z80::getRegIX();
    MemESP::tm_slotdata[MemESP::cur_timemachine].inter = Z80::isIFF2() ? 0x04 : 0;
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegR = Z80::getRegR();
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegAF = Z80::getRegAF();        

    MemESP::tm_slotdata[MemESP::cur_timemachine].RegSP = Z80::getRegSP();
    MemESP::tm_slotdata[MemESP::cur_timemachine].IM = Z80::getIM();
    MemESP::tm_slotdata[MemESP::cur_timemachine].borderColor = VIDEO::borderColor;
    MemESP::tm_slotdata[MemESP::cur_timemachine].RegPC = Z80::getRegPC();

    MemESP::tm_slotdata[MemESP::cur_timemachine].bankLatch = MemESP::bankLatch;
    MemESP::tm_slotdata[MemESP::cur_timemachine].videoLatch = MemESP::videoLatch;
    MemESP::tm_slotdata[MemESP::cur_timemachine].romLatch = MemESP::romLatch;
    MemESP::tm_slotdata[MemESP::cur_timemachine].pagingLock = MemESP::pagingLock;
    MemESP::tm_slotdata[MemESP::cur_timemachine].trdos = ESPectrum::trdos;

    if (MemESP::cur_timemachine == 7) {
        for (int n=0; n < 8; n++)
            MemESP::tm_slotbanks[0][n] = MemESP::tm_slotbanks[7][n];
    } else {
        for (int n=0; n < 8; n++)
            MemESP::tm_slotbanks[MemESP::cur_timemachine + 1][n] = MemESP::tm_slotbanks[MemESP::cur_timemachine][n];
    }

    // printf("Cur_tm: %d, Slot Banks: ",MemESP::cur_timemachine);
    // for (int n=0; n<8; n++)
    //     printf("%d ",MemESP::tm_slotbanks[MemESP::cur_timemachine][n]);
    // printf("\n");

    MemESP::cur_timemachine++;
    MemESP::cur_timemachine &= 0x07;

    // printf("================\n");

    MemESP::tm_framecnt = 0;

}

#endif