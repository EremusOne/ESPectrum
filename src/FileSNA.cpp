///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
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

#include "hardconfig.h"
#include "FileUtils.h"
#include "Config.h"
#include "CPU.h"
#include "Video.h"
#include "MemESP.h"
#include "ESPectrum.h"
#include "messages.h"
#include "OSDMain.h"
#include "FileSNA.h"
#include "Tape.h"
#include "AySound.h"
#include "pwm_audio.h"
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

// using internal storage (spi flash)
#include "esp_spiffs.h"

// ///////////////////////////////////////////////////////////////////////////////

bool FileSNA::load(string sna_fn)
{
    FILE *file;
    int sna_size;
    
    ESPectrum::reset();

    // Stop keyboard input
    ESPectrum::PS2Controller.keyboard()->suspendPort();

    file = fopen(sna_fn.c_str(), "rb");
    if (file==NULL)
    {
        printf("FileSNA: Error opening %s",sna_fn.c_str());

        // Resume keyboard input
        ESPectrum::PS2Controller.keyboard()->resumePort();

        return false;
    }

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

    VIDEO::borderColor = readByteFile(file);
    VIDEO::brd = VIDEO::border32[VIDEO::borderColor];

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

    CPU::statesInFrame = CPU::statesPerFrame();

    if (Config::getArch() == "48K") {
        //printf("48K arch activated\n");
        VIDEO::tStatesPerLine = TSTATES_PER_LINE;
        VIDEO::tStatesScreen = Config::aspect_16_9 ? TS_SCREEN_360x200 : TS_SCREEN_320x240;
        VIDEO::getFloatBusData = &VIDEO::getFloatBusData48;
        Z80Ops::is48 = true;
    } else {
        //printf("128K arch activated\n");
        VIDEO::tStatesPerLine = TSTATES_PER_LINE_128;
        VIDEO::tStatesScreen = Config::aspect_16_9 ? TS_SCREEN_360x200_128 : TS_SCREEN_320x240_128;
        VIDEO::getFloatBusData = &VIDEO::getFloatBusData128;
        Z80Ops::is48 = false;
    }

    MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[1] = (unsigned char *)MemESP::ram[5];
    MemESP::ramCurrent[2] = (unsigned char *)MemESP::ram[2];
    MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = MemESP::bankLatch & 0x01 ? true: false;    

    Tape::tapeFileName = "none";
    Tape::tapeStatus = TAPE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    // Empty audio buffers
    for (int i=0;i<ESP_AUDIO_OVERSAMPLES_48;i++) ESPectrum::overSamplebuf[i]=0;
    for (int i=0;i<ESP_AUDIO_SAMPLES_48;i++) {
        ESPectrum::audioBuffer[i]=0;
        ESPectrum::SamplebufAY[i]=0;
    }
    ESPectrum::lastaudioBit=0;

    // Set samples per frame and AY_emu flag depending on arch
    if (Config::getArch() == "48K") {
        ESPectrum::overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_48;
        ESPectrum::samplesPerFrame=ESP_AUDIO_SAMPLES_48; 
        ESPectrum::AY_emu = Config::AY48;
        ESPectrum::Audio_freq = ESP_AUDIO_FREQ_48;
    } else {
        ESPectrum::overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_128;
        ESPectrum::samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        ESPectrum::AY_emu = true;        
        ESPectrum::Audio_freq = ESP_AUDIO_FREQ_128;
    }

    ESPectrum::ESPoffset = 0;

    pwm_audio_stop();
    pwm_audio_set_sample_rate(ESPectrum::Audio_freq);
    pwm_audio_start();
    pwm_audio_set_volume(ESPectrum::aud_volume);
    
    // Reset AY emulation
    AySound::init();
    AySound::set_sound_format(ESPectrum::Audio_freq,1,8);
    AySound::reset();

    // Video sync
    ESPectrum::target = CPU::microsPerFrame();

    // Resume keyboard input
    ESPectrum::PS2Controller.keyboard()->resumePort();

    return true;

}

// ///////////////////////////////////////////////////////////////////////////////

bool FileSNA::isPersistAvailable(string filename)
{

    FILE *f = fopen(filename.c_str(), "rb");

    if (f == NULL) { return false; }

    fclose(f);

    return true;
}

// ///////////////////////////////////////////////////////////////////////////////

static bool writeMemPage(uint8_t page, FILE *file, bool blockMode)
{
    page = page & 0x07;
    uint8_t* buffer = MemESP::ram[page];

    // printf("writing page %d in [%s] mode\n", page, blockMode ? "block" : "byte");

    if (blockMode) {
        // Writing blocks should be faster, but fails sometimes when flash is getting full.
        for (int offset = 0; offset < MEM_PG_SZ; offset+=0x4000) {
            // printf("writing block from page %d at offset %d\n", page, offset);
            if (1 != fwrite(&buffer[offset],0x4000,1,file)) {
                printf("error writing block from page %d at offset %d\n", page, offset);
                return false;
            }
        }
    }
    else {
        for (int offset = 0; offset < MEM_PG_SZ; offset++) {
            uint8_t b = buffer[offset];
            if (1 != fwrite(&b,1,1,file)) {
                printf("error writing byte from page %d at offset %d\n", page, offset);
                return false;
            }
        }
    }
    return true;
}

// ///////////////////////////////////////////////////////////////////////////////

bool FileSNA::save(string sna_file) {
        
        // Try to save using pages
        if (FileSNA::save(sna_file, true)) return true;

        OSD::osdCenteredMsg(OSD_PSNA_SAVE_WARN, LEVEL_WARN);

        // Try to save byte-by-byte
        return FileSNA::save(sna_file, false);

}

// ///////////////////////////////////////////////////////////////////////////////

bool FileSNA::save(string sna_file, bool blockMode) {

    FILE *file;

    // Stop keyboard input
    ESPectrum::PS2Controller.keyboard()->suspendPort();

    file = fopen(sna_file.c_str(), "wb");
    if (file==NULL)
    {
        printf("FileSNA: Error opening %s for writing",sna_file.c_str());
        // Resume keyboard input
        ESPectrum::PS2Controller.keyboard()->resumePort();
        return false;
    }

    // write registers: begin with I
    writeByteFile(Z80_GET_I(), file);

    writeWordFileLE(Z80_GET_HLx(), file);
    writeWordFileLE(Z80_GET_DEx(), file);
    writeWordFileLE(Z80_GET_BCx(), file);
    writeWordFileLE(Z80_GET_AFx(), file);

    writeWordFileLE(Z80_GET_HL(), file);
    writeWordFileLE(Z80_GET_DE(), file);
    writeWordFileLE(Z80_GET_BC(), file);

    writeWordFileLE(Z80_GET_IY(), file);
    writeWordFileLE(Z80_GET_IX(), file);

    uint8_t inter = Z80_GET_IFF2() ? 0x04 : 0;
    writeByteFile(inter, file);
    writeByteFile(Z80_GET_R(), file);

    writeWordFileLE(Z80_GET_AF(), file);

    uint16_t SP = Z80_GET_SP();
    if (Config::getArch() == "48K") {
        // decrement stack pointer it for pushing PC to stack, only on 48K
        SP -= 2;
        MemESP::writeword(SP, Z80_GET_PC());
    }
    writeWordFileLE(SP, file);

    writeByteFile(Z80_GET_IM(), file);
    uint8_t bordercol = VIDEO::borderColor;
    writeByteFile(bordercol, file);

    // write RAM pages in 48K address space (0x4000 - 0xFFFF)
    uint8_t pages[3] = {5, 2, 0};
    if (Config::getArch() == "128K")
        pages[2] = MemESP::bankLatch;

    for (uint8_t ipage = 0; ipage < 3; ipage++) {
        uint8_t page = pages[ipage];
        if (!writeMemPage(page, file, blockMode)) {
            fclose(file);
            ESPectrum::PS2Controller.keyboard()->resumePort();
            return false;
        }
    }

    if (Config::getArch() == "48K")
    {
        // nothing to do here
    }
    else if (Config::getArch() == "128K")
    {
        // write pc
        writeWordFileLE(Z80_GET_PC(), file);

        // write memESP bank control port
        uint8_t tmp_port = MemESP::bankLatch;
        bitWrite(tmp_port, 3, MemESP::videoLatch);
        bitWrite(tmp_port, 4, MemESP::romLatch);
        bitWrite(tmp_port, 5, MemESP::pagingLock);
        writeByteFile(tmp_port, file);

        writeByteFile(0, file);     // TR-DOS not paged

        // write remaining ram pages
        for (int page = 0; page < 8; page++) {
            if (page != MemESP::bankLatch && page != 2 && page != 5) {
                if (!writeMemPage(page, file, blockMode)) {
                    fclose(file);
                    ESPectrum::PS2Controller.keyboard()->resumePort();
                    return false;
                }
            }
        }
    }

    fclose(file);

    ESPectrum::PS2Controller.keyboard()->resumePort();

    return true;
}
