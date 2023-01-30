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
#include "FileUtils.h"
#include "Config.h"
#include "CPU.h"
#include "MemESP.h"
#include "ESPectrum.h"
#include "messages.h"
#include "OSDMain.h"
// #include "Wiimote2Keys.h"
#include "FileSNA.h"

#include <stdio.h>

#include <inttypes.h>

#include <string>

using namespace std;

// ///////////////////////////////////////////////////////////////////////////////

#include "Z80_JLS/z80.h"

#define Z80_GET_AF() Z80::getRegAF()
#define Z80_GET_BC() Z80::getRegBC()
#define Z80_GET_DE() Z80::getRegDE()
#define Z80_GET_HL() Z80::getRegHL()

#define Z80_GET_AFx() Z80::getRegAFx()
#define Z80_GET_BCx() Z80::getRegBCx()
#define Z80_GET_DEx() Z80::getRegDEx()
#define Z80_GET_HLx() Z80::getRegHLx()

#define Z80_GET_IY() Z80::getRegIY()
#define Z80_GET_IX() Z80::getRegIX()

#define Z80_GET_SP() Z80::getRegSP()
#define Z80_GET_PC() Z80::getRegPC()

#define Z80_GET_I() Z80::getRegI()
#define Z80_GET_R() Z80::getRegR()
#define Z80_GET_IM() Z80::getIM()
#define Z80_GET_IFF1() Z80::isIFF1()
#define Z80_GET_IFF2() Z80::isIFF2()

#define Z80_SET_AF(v) Z80::setRegAF(v)
#define Z80_SET_BC(v) Z80::setRegBC(v)
#define Z80_SET_DE(v) Z80::setRegDE(v)
#define Z80_SET_HL(v) Z80::setRegHL(v)

#define Z80_SET_AFx(v) Z80::setRegAFx(v)
#define Z80_SET_BCx(v) Z80::setRegBCx(v)
#define Z80_SET_DEx(v) Z80::setRegDEx(v)
#define Z80_SET_HLx(v) Z80::setRegHLx(v)

#define Z80_SET_IY(v) Z80::setRegIY(v)
#define Z80_SET_IX(v) Z80::setRegIX(v)

#define Z80_SET_SP(v) Z80::setRegSP(v)
#define Z80_SET_PC(v) Z80::setRegPC(v)

#define Z80_SET_I(v) Z80::setRegI(v)
#define Z80_SET_R(v) Z80::setRegR(v)
#define Z80_SET_IM(v) Z80::setIM((Z80::IntMode)(v))
#define Z80_SET_IFF1(v) Z80::setIFF1(v)
#define Z80_SET_IFF2(v) Z80::setIFF2(v)

// ///////////////////////////////////////////////////////////////////////////////

#ifdef USE_INT_FLASH
// using internal storage (spi flash)
#include "esp_spiffs.h"
// set The Filesystem to SPIFFS
// #define THE_FS SPIFFS
#endif

// ///////////////////////////////////////////////////////////////////////////////

// #ifdef USE_SD_CARD
// // using external storage (SD card)
// #include <SD.h>
// // set The Filesystem to SD
// #define THE_FS SD
// #endif

// ///////////////////////////////////////////////////////////////////////////////

bool FileSNA::load(string sna_fn)
{
    FILE *file;
    //uint16_t retaddr;
    int sna_size;
    
    ESPectrum::reset();

    // Stop keyboard input
    ESPectrum::PS2Controller.keyboard()->suspendPort();

    // if (sna_fn != DISK_PSNA_FILE)
    //     loadKeytableForGame(sna_fn.c_str());
    
    file = fopen(sna_fn.c_str(), "rb");
    if (file==NULL)
    {
        printf("FileSNA: Error opening %s",sna_fn.c_str());

        // Resume keyboard input
        ESPectrum::PS2Controller.keyboard()->resumePort();

        return false;
    }
    // file = FileUtils::safeOpenFileRead(sna_fn);

    fseek(file,0,SEEK_END);
    sna_size = ftell(file);
    rewind (file);

    if (sna_size < SNA_48K_SIZE) {
        printf("FileSNA::load: bad SNA %s: size = %d < %d\n", sna_fn.c_str(), sna_size, SNA_48K_SIZE);

        // Resume keyboard input
        ESPectrum::PS2Controller.keyboard()->resumePort();

        return false;
    }

    printf("FileSNA::load: Opening %s: size = %d\n", sna_fn.c_str(), sna_size);

    string snapshotArch = "48K";

    MemESP::bankLatch = 0;
    MemESP::pagingLock = 1;
    MemESP::videoLatch = 0;
    MemESP::romLatch = 0;
    MemESP::romInUse = 0;

    // Read in the registers
    Z80_SET_I(readByteFile(file));

    Z80_SET_HLx(readWordFileLE(file));
    Z80_SET_DEx(readWordFileLE(file));
    Z80_SET_BCx(readWordFileLE(file));
    Z80_SET_AFx(readWordFileLE(file));

    Z80_SET_HL(readWordFileLE(file));
    Z80_SET_DE(readWordFileLE(file));
    Z80_SET_BC(readWordFileLE(file));

    Z80_SET_IY(readWordFileLE(file));
    Z80_SET_IX(readWordFileLE(file));

    uint8_t inter = readByteFile(file);
    Z80_SET_IFF2((inter & 0x04) ? true : false);
    Z80_SET_IFF1(Z80_GET_IFF2());
    Z80_SET_R(readByteFile(file));

    Z80_SET_AF(readWordFileLE(file));
    Z80_SET_SP(readWordFileLE(file));

    Z80_SET_IM(readByteFile(file));

    CPU::borderColor = readByteFile(file);

    // read 48K memory
    readBlockFile(file, MemESP::ram5, 0x4000);
    readBlockFile(file, MemESP::ram2, 0x4000);
    readBlockFile(file, MemESP::ram0, 0x4000);

    if (sna_size == SNA_48K_SIZE)
    {
        snapshotArch = "48K";

        // in 48K mode, pop PC from stack
        uint16_t SP = Z80_GET_SP();
        Z80_SET_PC(MemESP::readword(SP));
        Z80_SET_SP(SP + 2);
    }
    else
    {
        snapshotArch = "128K";

        // in 128K mode, recover stored PC
        Z80_SET_PC(readWordFileLE(file));

        // tmp_port contains page switching status, including current page number (latch)
        uint8_t tmp_port = readByteFile(file);
        uint8_t tmp_latch = tmp_port & 0x07;

        // copy what was read into page 0 to correct page
        memcpy(MemESP::ram[tmp_latch], MemESP::ram[0], 0x4000);

        uint8_t tr_dos = readByteFile(file);     // unused
        
        // read remaining pages
        for (int page = 0; page < 8; page++) {
            if (page != tmp_latch && page != 2 && page != 5) {
                readBlockFile(file, MemESP::ram[page], 0x4000);
            }
        }

        // decode tmp_port
        MemESP::videoLatch = bitRead(tmp_port, 3);
        MemESP::romLatch = bitRead(tmp_port, 4);
        MemESP::pagingLock = bitRead(tmp_port, 5);
        MemESP::bankLatch = tmp_latch;
        MemESP::romInUse = MemESP::romLatch;

    }
    
    fclose(file);

    // just architecturey things
    if (Config::getArch() == "128K")
    {
        if (snapshotArch == "48K")
        {
            #ifdef SNAPSHOT_LOAD_FORCE_ARCH
                Config::requestMachine("48K", "SINCLAIR", true);
                MemESP::romInUse = 0;
            #else
                MemESP::romInUse = 1;
            #endif
        }
    }
    else if (Config::getArch() == "48K")
    {
        if (snapshotArch == "128K")
        {
            Config::requestMachine("128K", "SINCLAIR", true);
            MemESP::romInUse = 1;
        }
    }

    // Resume keyboard input
    ESPectrum::PS2Controller.keyboard()->resumePort();

    return true;

}

// ///////////////////////////////////////////////////////////////////////////////

// bool FileSNA::isPersistAvailable(string filename)
// {
//     // string filename = DISK_PSNA_FILE;
//     return THE_FS.exists(filename.c_str());
// }

// ///////////////////////////////////////////////////////////////////////////////

// //static bool IRAM_ATTR writeMemESPPage(uint8_t page, File file, bool blockMode)
// static bool writeMemESPPage(uint8_t page, File file, bool blockMode)
// {
//     page = page & 0x07;
//     uint8_t* buffer = MemESP::ram[page];

//     printf("writing page %d in [%s] mode\n", page, blockMode ? "block" : "byte");

//     if (blockMode) {
//         // writing blocks should be faster, but fails sometimes when flash is getting full.
//         uint16_t bytesWritten = file.write(buffer, MEMESP_PG_SZ);
//         if (bytesWritten != MEMESP_PG_SZ) {
//             printf("error writing page %d: %d of %d bytes written\n", page, bytesWritten, MEMESP_PG_SZ);
//             return false;
//         }
//     }
//     else {
//         for (int offset = 0; offset < MEMESP_PG_SZ; offset++) {
//             uint8_t b = buffer[offset];
//             if (1 != file.write(b)) {
//                 printf("error writing byte from page %d at offset %d\n", page, offset);
//                 return false;
//             }
//         }
//     }
//     return true;
// }

// ///////////////////////////////////////////////////////////////////////////////

// bool FileSNA::save(string sna_file) {
//     // #define BENCHMARK_BOTH_SAVE_METHODS
//     #ifndef BENCHMARK_BOTH_SAVE_METHODS
//         // try to save using pages

//         if (FileSNA::save(sna_file, true))
//             return true;
//         OSD::osdCenteredMsg(OSD_PSNA_SAVE_WARN, 2);
//         // try to save byte-by-byte
//         return FileSNA::save(sna_file, false);
//     #else
//         uint32_t ts_start, ts_end;
//         bool ok;

//         ts_start = millis();
//         ok = FileSNA::save(sna_file, true);
//         ts_end = millis();

//         printf("save SNA [page]: ok = %d, millis = %d\n", ok, (ts_end - ts_start));

//         ts_start = millis();
//         ok = FileSNA::save(sna_file, false);
//         ts_end = millis();

//         printf("save SNA [byte]: ok = %d, millis = %d\n", ok, (ts_end - ts_start));

//         return ok;
//     #endif // BENCHMARK_BOTH_SAVE_METHODS
// }

// ///////////////////////////////////////////////////////////////////////////////

// bool FileSNA::save(string sna_file, bool blockMode) {
//     KB_INT_STOP;

//     // open file
//     File file = THE_FS.open(sna_file, FILE_WRITE);
//     if (!file) {
//         printf("FileSNA::save: failed to open %s for writing\n", sna_file.c_str());
//         KB_INT_START;
//         return false;
//     }

//     // write registers: begin with I
//     writeByteFile(Z80_GET_I(), file);

//     writeWordFileLE(Z80_GET_HLx(), file);
//     writeWordFileLE(Z80_GET_DEx(), file);
//     writeWordFileLE(Z80_GET_BCx(), file);
//     writeWordFileLE(Z80_GET_AFx(), file);

//     writeWordFileLE(Z80_GET_HL(), file);
//     writeWordFileLE(Z80_GET_DE(), file);
//     writeWordFileLE(Z80_GET_BC(), file);

//     writeWordFileLE(Z80_GET_IY(), file);
//     writeWordFileLE(Z80_GET_IX(), file);

//     uint8_t inter = Z80_GET_IFF2() ? 0x04 : 0;
//     writeByteFile(inter, file);
//     writeByteFile(Z80_GET_R(), file);

//     writeWordFileLE(Z80_GET_AF(), file);

//     uint16_t SP = Z80_GET_SP();
//     if (Config::getArch() == "48K") {
//         // decrement stack pointer it for pushing PC to stack, only on 48K
//         SP -= 2;
//         MemESP::writeword(SP, Z80_GET_PC());
//     }
//     writeWordFileLE(SP, file);

//     writeByteFile(Z80_GET_IM(), file);
//     uint8_t bordercol = ESPectrum::CPU::borderColor;
//     writeByteFile(bordercol, file);

//     // write RAM pages in 48K address space (0x4000 - 0xFFFF)
//     uint8_t pages[3] = {5, 2, 0};
//     if (Config::getArch() == "128K")
//         pages[2] = MemESP::bankLatch;

//     for (uint8_t ipage = 0; ipage < 3; ipage++) {
//         uint8_t page = pages[ipage];
//         if (!writeMemESPPage(page, file, blockMode)) {
//             file.close();
//             KB_INT_START;
//             return false;
//         }
//     }

//     if (Config::getArch() == "48K")
//     {
//         // nothing to do here
//     }
//     else if (Config::getArch() == "128K")
//     {
//         // write pc
//         writeWordFileLE(Z80_GET_PC(), file);

//         // write memESP bank control port
//         uint8_t tmp_port = MemESP::bankLatch;
//         bitWrite(tmp_port, 3, MemESP::videoLatch);
//         bitWrite(tmp_port, 4, MemESP::romLatch);
//         bitWrite(tmp_port, 5, MemESP::pagingLock);
//         writeByteFile(tmp_port, file);

//         writeByteFile(0, file);     // TR-DOS not paged

//         // write remaining ram pages
//         for (int page = 0; page < 8; page++) {
//             if (page != MemESP::bankLatch && page != 2 && page != 5) {
//                 if (!writeMemESPPage(page, file, blockMode)) {
//                     file.close();
//                     KB_INT_START;
//                     return false;
//                 }
//             }
//         }
//     }

//     file.close();
//     KB_INT_START;
//     return true;
// }

// ///////////////////////////////////////////////////////////////////////////////

// static uint8_t* quick_sna_buffer = NULL;
// static uint32_t quick_sna_size   = 0;

// bool FileSNA::isQuickAvailable()
// {
//     return quick_sna_buffer != NULL;
// }

// bool FileSNA::saveQuick()
// {
//     KB_INT_STOP;

//     // deallocate buffer if it does not fit required size
//     if (quick_sna_buffer != NULL)
//     {
//         if (quick_sna_size == SNA_48K_SIZE && Config::getArch() != "48K") {
//             free(quick_sna_buffer);
//             quick_sna_buffer = NULL;
//             quick_sna_size = 0;
//         }
//         else if (quick_sna_size != SNA_48K_SIZE && Config::getArch() == "48K") {
//             free(quick_sna_buffer);
//             quick_sna_buffer = NULL;
//             quick_sna_size = 0;
//         }
//     } 

//     // allocate buffer it not allocated
//     if (quick_sna_buffer == NULL)
//     {
//         uint32_t requested_sna_size = 0;
//         if (Config::getArch() == "48K")
//             requested_sna_size = SNA_48K_SIZE;
//         else
//             requested_sna_size = SNA_128K_SIZE2;

//         #ifdef BOARD_HAS_PSRAM
//             quick_sna_buffer = (uint8_t*)ps_calloc(1, requested_sna_size);
//         #else
//             quick_sna_buffer = (uint8_t*)calloc(1, requested_sna_size);
//         #endif
//         if (quick_sna_buffer == NULL) {
//             printf("FileSNA::saveQuick: cannot allocate %d bytes for snapshot buffer\n", requested_sna_size);
//             KB_INT_START;
//             return false;
//         }
//         quick_sna_size = requested_sna_size;
//         printf("saveQuick: allocated %d bytes for snapshot buffer\n", quick_sna_size);
//     }

//     bool result = saveToMemESP(quick_sna_buffer, quick_sna_size);
//     KB_INT_START;
//     return result;
// }

// bool FileSNA::saveToMemESP(uint8_t* dstBuffer, uint32_t size)
// {
//     uint8_t* snaptr = dstBuffer;

//     // write registers: begin with I
//     writeByteMemESP(Z80_GET_I(), snaptr);

//     writeWordMemESPLE(Z80_GET_HLx(), snaptr);
//     writeWordMemESPLE(Z80_GET_DEx(), snaptr);
//     writeWordMemESPLE(Z80_GET_BCx(), snaptr);
//     writeWordMemESPLE(Z80_GET_AFx(), snaptr);

//     writeWordMemESPLE(Z80_GET_HL(), snaptr);
//     writeWordMemESPLE(Z80_GET_DE(), snaptr);
//     writeWordMemESPLE(Z80_GET_BC(), snaptr);

//     writeWordMemESPLE(Z80_GET_IY(), snaptr);
//     writeWordMemESPLE(Z80_GET_IX(), snaptr);

//     uint8_t inter = Z80_GET_IFF2() ? 0x04 : 0;
//     writeByteMemESP(inter, snaptr);
//     writeByteMemESP(Z80_GET_R(), snaptr);

//     writeWordMemESPLE(Z80_GET_AF(), snaptr);

//     uint16_t SP = Z80_GET_SP();
//     if (Config::getArch() == "48K") {
//         // decrement stack pointer it for pushing PC to stack, only on 48K
//         SP -= 2;
//         MemESP::writeword(SP, Z80_GET_PC());
//     }
//     writeWordMemESPLE(SP, snaptr);

//     writeByteMemESP(Z80_GET_IM(), snaptr);
//     uint8_t bordercol = ESPectrum::CPU::borderColor;
//     writeByteMemESP(bordercol, snaptr);

//     // write RAM pages in 48K address space (0x4000 - 0xFFFF)
//     uint8_t pages[3] = {5, 2, 0};
//     if (Config::getArch() == "128K")
//         pages[2] = MemESP::bankLatch;

//     for (uint8_t ipage = 0; ipage < 3; ipage++) {
//         uint8_t page = pages[ipage];
//         writeBlockMemESP(MemESP::ram[page], snaptr, MEMESP_PG_SZ);
//     }

//     if (Config::getArch() == "48K")
//     {
//         // nothing to do here
//     }
//     else if (Config::getArch() == "128K")
//     {
//         // write pc
//         writeWordMemESPLE(Z80_GET_PC(), snaptr);

//         // write memESP bank control port
//         uint8_t tmp_port = MemESP::bankLatch;
//         bitWrite(tmp_port, 3, MemESP::videoLatch);
//         bitWrite(tmp_port, 4, MemESP::romLatch);
//         bitWrite(tmp_port, 5, MemESP::pagingLock);
//         writeByteMemESP(tmp_port, snaptr);

//         writeByteMemESP(0, snaptr);     // TR-DOS not paged

//         // write remaining ram pages
//         for (int page = 0; page < 8; page++) {
//             if (page != MemESP::bankLatch && page != 2 && page != 5) {
//                 writeBlockMemESP(MemESP::ram[page], snaptr, MEMESP_PG_SZ);
//             }
//         }
//     }

//     return true;
// }

// ///////////////////////////////////////////////////////////////////////////////

// bool FileSNA::loadQuick()
// {
//     KB_INT_STOP;

//     if (NULL == quick_sna_buffer) {
//         // nothing to read
//         println("FileSNA::loadQuick(): nothing to load");
//         KB_INT_START;
//         return false;
//     }

//     bool result = loadFromMemESP(quick_sna_buffer, quick_sna_size);
//     KB_INT_START;
//     return result;
// }

// ///////////////////////////////////////////////////////////////////////////////

// bool FileSNA::loadFromMemESP(uint8_t* srcBuffer, uint32_t size)
// {
//     uint8_t* snaptr = srcBuffer;

//     string snapshotArch = "48K";

//     MemESP::bankLatch = 0;
//     MemESP::pagingLock = 1;
//     MemESP::videoLatch = 0;
//     MemESP::romLatch = 0;
//     MemESP::romInUse = 0;

//     Z80_SET_I(readByteMemESP(snaptr));

//     Z80_SET_HLx(readWordMemESPLE(snaptr));
//     Z80_SET_DEx(readWordMemESPLE(snaptr));
//     Z80_SET_BCx(readWordMemESPLE(snaptr));
//     Z80_SET_AFx(readWordMemESPLE(snaptr));

//     Z80_SET_HL(readWordMemESPLE(snaptr));
//     Z80_SET_DE(readWordMemESPLE(snaptr));
//     Z80_SET_BC(readWordMemESPLE(snaptr));

//     Z80_SET_IY(readWordMemESPLE(snaptr));
//     Z80_SET_IX(readWordMemESPLE(snaptr));

//     uint8_t inter = readByteMemESP(snaptr);
//     Z80_SET_IFF2((inter & 0x04) ? true : false);
//     Z80_SET_IFF1(Z80_GET_IFF2());
//     Z80_SET_R(readByteMemESP(snaptr));

//     Z80_SET_AF(readWordMemESPLE(snaptr));
//     Z80_SET_SP(readWordMemESPLE(snaptr));

//     Z80_SET_IM(readByteMemESP(snaptr));

//     ESPectrum::CPU::borderColor = readByteMemESP(snaptr);

//     // read 48K memESPory
//     readBlockMemESP(snaptr, MemESP::ram[5], MEMESP_PG_SZ);
//     readBlockMemESP(snaptr, MemESP::ram[2], MEMESP_PG_SZ);
//     readBlockMemESP(snaptr, MemESP::ram[0], MEMESP_PG_SZ);

//     if (size == SNA_48K_SIZE)
//     {
//         snapshotArch = "48K";

//         // in 48K mode, pop PC from stack
//         uint16_t SP = Z80_GET_SP();
//         Z80_SET_PC(MemESP::readword(SP));
//         Z80_SET_SP(SP + 2);
//     }
//     else
//     {
//         snapshotArch = "128K";

//         // in 128K mode, recover stored PC
//         Z80_SET_PC(readWordMemESPLE(snaptr));

//         // tmp_port contains page switching status, including current page number (latch)
//         uint8_t tmp_port = readByteMemESP(snaptr);
//         uint8_t tmp_latch = tmp_port & 0x07;

//         // copy what was read into page 0 to correct page
//         memESPcpy(MemESP::ram[tmp_latch], MemESP::ram[0], 0x4000);

//         uint8_t tr_dos = readByteMemESP(snaptr);     // unused
        
//         // read remaining pages
//         for (int page = 0; page < 8; page++) {
//             if (page != tmp_latch && page != 2 && page != 5) {
//                 readBlockMemESP(snaptr, MemESP::ram[page], 0x4000);
//             }
//         }

//         // decode tmp_port
//         MemESP::videoLatch = bitRead(tmp_port, 3);
//         MemESP::romLatch = bitRead(tmp_port, 4);
//         MemESP::pagingLock = bitRead(tmp_port, 5);
//         MemESP::bankLatch = tmp_latch;
//         MemESP::romInUse = MemESP::romLatch;
//     }

//     // just architecturey things
//     if (Config::getArch() == "128K")
//     {
//         if (snapshotArch == "48K")
//         {
//             #ifdef SNAPSHOT_LOAD_FORCE_ARCH
//                 Config::requestMachine("48K", "SINCLAIR", true);
//                 MemESP::romInUse = 0;
//             #else
//                 MemESP::romInUse = 1;
//             #endif
//         }
//     }
//     else if (Config::getArch() == "48K")
//     {
//         if (snapshotArch == "128K")
//         {
//             Config::requestMachine("128K", "SINCLAIR", true);
//             MemESP::romInUse = 1;
//         }
//     }

//     return true;
// }

// ///////////////////////////////////////////////////////////////////////////////

