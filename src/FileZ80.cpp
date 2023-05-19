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
#include "FileZ80.h"
#include "FileUtils.h"
#include "CPU.h"
#include "Video.h"
#include "Ports.h"
#include "MemESP.h"
#include "ESPectrum.h"
#include "messages.h"
#include "OSDMain.h"
#include "Config.h"
#include "Tape.h"
#include "pwm_audio.h"
#include "AySound.h"

///////////////////////////////////////////////////////////////////////////////

#include "Z80_JLS/z80.h"

///////////////////////////////////////////////////////////////////////////////

// using internal storage (spi flash)
#include "esp_spiffs.h"

///////////////////////////////////////////////////////////////////////////////

static uint16_t mkword(uint8_t lobyte, uint8_t hibyte) {
    return lobyte | (hibyte << 8);
}

bool FileZ80::load(string z80_fn)
{
    FILE *file;

    // Stop keyboard input
    ESPectrum::PS2Controller.keyboard()->suspendPort();
    // Stop audio
    pwm_audio_stop();

    file = fopen(z80_fn.c_str(), "rb");
    if (file==NULL)
    {
        printf("FileZ80: Error opening %s\n",z80_fn.c_str());

        // Resume audio
        pwm_audio_start();
        // Resume keyboard input
        ESPectrum::PS2Controller.keyboard()->resumePort();

        return false;
    }

    fseek(file,0,SEEK_END);
    uint32_t file_size = ftell(file);
    rewind (file);

    // Reset Z80 and set bankLatch to default
    MemESP::bankLatch = 0;
    Z80::reset();

    uint32_t dataOffset = 0;

    // initially assuming version 1; this assumption may change
    uint8_t version = 1;

    // stack space for header, should be enough for
    // version 1 (30 bytes)
    // version 2 (55 bytes) (30 + 2 + 23)
    // version 3 (87 bytes) (30 + 2 + 55) or (86 bytes) (30 + 2 + 54)
    uint8_t header[87];

    // read first 30 bytes
    for (uint8_t i = 0; i < 30; i++) {
        header[i] = readByteFile(file);
        dataOffset ++;
    }

    // additional vars
    uint8_t b12, b29;

    // begin loading registers
    Z80::setRegA  (       header[0]);
    Z80::setFlags (       header[1]);
    Z80::setRegBC (mkword(header[2], header[3]));
    Z80::setRegHL (mkword(header[4], header[5]));
    Z80::setRegPC (mkword(header[6], header[7]));
    Z80::setRegSP (mkword(header[8], header[9]));
    Z80::setRegI  (       header[10]);
    Z80::setRegR  (       header[11]);
    b12 =                 header[12];
    Z80::setRegDE (mkword(header[13], header[14]));
    Z80::setRegBCx(mkword(header[15], header[16]));
    Z80::setRegDEx(mkword(header[17], header[18]));
    Z80::setRegHLx(mkword(header[19], header[20]));
    Z80::setRegAFx(mkword(header[22], header[21])); // watch out for order!!!
    Z80::setRegIY (mkword(header[23], header[24]));
    Z80::setRegIX (mkword(header[25], header[26]));
    Z80::setIFF1  (       header[27] ? true : false);
    Z80::setIFF2  (       header[28] ? true : false);
    b29 =                 header[29];
    Z80::setIM((Z80::IntMode)(b29 & 0x03));

    uint16_t RegPC = Z80::getRegPC();

    VIDEO::borderColor = (b12 >> 1) & 0x07;
    VIDEO::brd = VIDEO::border32[VIDEO::borderColor];
    
    bool dataCompressed = (b12 & 0x20) ? true : false;
    string fileArch = "48K";
    uint8_t memPagingReg = 0;

    // #define LOG_Z80_DETAILS

    if (RegPC != 0) {

        // version 1, the simplest, 48K only.
        uint32_t memRawLength = file_size - dataOffset;

        #ifdef LOG_Z80_DETAILS
        printf("Z80 format version: %d\n", version);
        printf("machine type: %s\n", fileArch.c_str());
        printf("data offset: %d\n", dataOffset);
        printf("data compressed: %s\n", dataCompressed ? "true" : "false");
        printf("file length: %d\n", file_size);
        printf("data length: %d\n", memRawLength);
        printf("b12: %d\n", b12);
        printf("pc: %d\n", RegPC);
        printf("border: %d\n", VIDEO::borderColor);
        #endif

        if (dataCompressed)
        {
            // assuming stupid 00 ED ED 00 terminator present, should check for it instead of assuming
            uint16_t dataLen = (uint16_t)(memRawLength - 4);

            // load compressed data into memory
            loadCompressedMemData(file, dataLen, 0x4000, 0xC000);
        }
        else
        {
            uint16_t dataLen = (memRawLength < 0xC000) ? memRawLength : 0xC000;

            // load uncompressed data into memory
            for (int i = 0; i < dataLen; i++)
                MemESP::writebyte(0x4000 + i, readByteFile(file));
        }

        // latches for 48K
        MemESP::romLatch = 0;
        MemESP::romInUse = 0;
        MemESP::bankLatch = 0;
        MemESP::pagingLock = 1;
        MemESP::videoLatch = 0;
    }
    else {
        // read 2 more bytes
        for (uint8_t i = 30; i < 32; i++) {
            header[i] = readByteFile(file);
            dataOffset ++;
        }

        // additional header block length
        uint16_t ahblen = mkword(header[30], header[31]);
        if (ahblen == 23)
            version = 2;
        else if (ahblen == 54 || ahblen == 55)
            version = 3;
        else {
            
            OSD::osdCenteredMsg("Z80 load: unknown version", LEVEL_ERROR);
            // printf("Z80.load: unknown version, ahblen = %d\n", ahblen);
            
            fclose(file);

            // Resume audio
            pwm_audio_start();
            // Resume keyboard input
            ESPectrum::PS2Controller.keyboard()->resumePort();

            ESPectrum::reset();

            return false;
        }

        // read additional header block
        for (uint8_t i = 32; i < 32 + ahblen; i++) {
            header[i] = readByteFile(file);
            dataOffset ++;
        }

        // program counter
        RegPC = mkword(header[32], header[33]);
        Z80::setRegPC(RegPC);

        // hardware mode
        uint8_t b34 = header[34];
        // defaulting to 128K
        fileArch = "128K";

        if (version == 2) {
            if (b34 == 0) fileArch = "48K";
            if (b34 == 1) fileArch = "48K"; // + if1
            if (b34 == 2) fileArch = "SAMRAM";
        }
        else if (version == 3) {
            if (b34 == 0) fileArch = "48K";
            if (b34 == 1) fileArch = "48K"; // + if1
            if (b34 == 2) fileArch = "SAMRAM";
            if (b34 == 3) fileArch = "48K"; // + mgt
        }

        #ifdef LOG_Z80_DETAILS
        uint32_t memRawLength = file_size - dataOffset;
        printf("Z80 format version: %d\n", version);
        printf("machine type: %s\n", fileArch.c_str());
        printf("data offset: %d\n", dataOffset);
        printf("file length: %d\n", file_size);
        printf("b12: %d\n", b12);
        printf("b34: %d\n", b34);
        printf("pc: %d\n", RegPC);
        printf("border: %d\n", VIDEO::borderColor);
        #endif

        if (fileArch == "48K") {
            MemESP::romLatch = 0;
            MemESP::romInUse = 0;
            MemESP::bankLatch = 0;
            MemESP::pagingLock = 1;
            MemESP::videoLatch = 0;

            uint16_t pageStart[12] = {0, 0, 0, 0, 0x8000, 0xC000, 0, 0, 0x4000, 0, 0};

            uint32_t dataLen = file_size;
            while (dataOffset < dataLen) {
                uint8_t hdr0 = readByteFile(file); dataOffset ++;
                uint8_t hdr1 = readByteFile(file); dataOffset ++;
                uint8_t hdr2 = readByteFile(file); dataOffset ++;
                uint16_t compDataLen = mkword(hdr0, hdr1);
                
                #ifdef LOG_Z80_DETAILS
                printf("compressed data length: %d\n", compDataLen);
                printf("page number: %d\n", hdr2);
                #endif
                
                uint16_t memoff = pageStart[hdr2];
                
                #ifdef LOG_Z80_DETAILS
                printf("page start: 0x%X\n", memoff);
                #endif
                
                loadCompressedMemData(file, compDataLen, memoff, 0x4000);
                dataOffset += compDataLen;
            }

            // great success!!!
        }
        else if (fileArch == "128K") {
            MemESP::romInUse = 1;

            // paging register
            uint8_t b35 = header[35];
            MemESP::pagingLock = bitRead(b35, 5);
            MemESP::romLatch = bitRead(b35, 4);
            MemESP::videoLatch = bitRead(b35, 3);
            MemESP::bankLatch = b35 & 0x07;

            uint8_t* pages[12] = {
                MemESP::rom[0], MemESP::rom[2], MemESP::rom[1],
                MemESP::ram0, MemESP::ram1, MemESP::ram2, MemESP::ram3,
                MemESP::ram4, MemESP::ram5, MemESP::ram6, MemESP::ram7,
                MemESP::rom[3] };

            const char* pagenames[12] = { "rom0", "IDP", "rom1",
                "ram0", "ram1", "ram2", "ram3", "ram4", "ram5", "ram6", "ram7", "MFR" };

            uint32_t dataLen = file_size;
            while (dataOffset < dataLen) {
                uint8_t hdr0 = readByteFile(file); dataOffset ++;
                uint8_t hdr1 = readByteFile(file); dataOffset ++;
                uint8_t hdr2 = readByteFile(file); dataOffset ++;
                uint16_t compDataLen = mkword(hdr0, hdr1);

                #ifdef LOG_Z80_DETAILS
                printf("compressed data length: %d\n", compDataLen);
                printf("page: %s\n", pagenames[hdr2]);
                #endif
                
                uint8_t* memPage = pages[hdr2];

                loadCompressedMemPage(file, compDataLen, memPage, 0x4000);
                dataOffset += compDataLen;
            }

            // great success!!!
        }
    }

    fclose(file);

    // Arch check
    if (Config::getArch() == "128K") {
        if (fileArch == "48K") {
            #ifdef SNAPSHOT_LOAD_FORCE_ARCH
            Config::requestMachine("48K", "SINCLAIR", true);

            // TO DO: Condition this to 50hz mode
            Config::ram_file = z80_fn;
            Config::save();
            OSD::esp_hard_reset();                            

            MemESP::romInUse = 0;
            #else
            MemESP::romInUse = 1;
            #endif
        }
    }
    else if (Config::getArch() == "48K") {
        if (fileArch == "128K") {
            Config::requestMachine("128K", "SINCLAIR", true);

            // TO DO: Condition this to 50hz mode
            Config::ram_file = z80_fn;
            Config::save();
            OSD::esp_hard_reset();                            

            MemESP::romInUse = 1;
        }
    }

    // Ports
    for (int i = 0; i < 128; i++) Ports::port[i] = 0x1F;
    if (Config::joystick) Ports::port[0x1f] = 0; // Kempston

    CPU::statesInFrame = CPU::statesPerFrame();
    CPU::tstates = 0;
    CPU::global_tstates = 0;
    ESPectrum::target = CPU::microsPerFrame();
    ESPectrum::ESPoffset = 0;

    if (Config::getArch() == "48K") {

        Z80Ops::is48 = true;

        VIDEO::tStatesPerLine = TSTATES_PER_LINE;
        VIDEO::tStatesScreen = Config::aspect_16_9 ? TS_SCREEN_360x200 : TS_SCREEN_320x240;
        VIDEO::getFloatBusData = &VIDEO::getFloatBusData48;

        ESPectrum::overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_48;
        ESPectrum::samplesPerFrame=ESP_AUDIO_SAMPLES_48; 
        ESPectrum::AY_emu = Config::AY48;
        ESPectrum::Audio_freq = ESP_AUDIO_FREQ_48;

    } else {
        
        Z80Ops::is48 = false;

        VIDEO::tStatesPerLine = TSTATES_PER_LINE_128;
        VIDEO::tStatesScreen = Config::aspect_16_9 ? TS_SCREEN_360x200_128 : TS_SCREEN_320x240_128;
        VIDEO::getFloatBusData = &VIDEO::getFloatBusData128;

        ESPectrum::overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_128;
        ESPectrum::samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        ESPectrum::AY_emu = true;        
        ESPectrum::Audio_freq = ESP_AUDIO_FREQ_128;

    }

    VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;
    VIDEO::Draw = &VIDEO::Blank;

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
        AySound::SamplebufAY[i]=0;
    }
    ESPectrum::lastaudioBit=0;

    // Reset AY emulation
    AySound::init();
    AySound::set_sound_format(ESPectrum::Audio_freq,1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    // Resume audio
    pwm_audio_set_param(ESPectrum::Audio_freq,LEDC_TIMER_8_BIT,1);
    pwm_audio_start();
    pwm_audio_set_volume(ESPectrum::aud_volume);
  
    // Resume keyboard input
    ESPectrum::PS2Controller.keyboard()->resumePort();

    return true;

}

void FileZ80::loadCompressedMemData(FILE *f, uint16_t dataLen, uint16_t memoff, uint16_t memlen)
{
    uint16_t dataOff = 0;
    uint8_t ed_cnt = 0;
    uint8_t repcnt = 0;
    uint8_t repval = 0;
    uint16_t memidx = 0;

    while(dataOff < dataLen && memidx < memlen) {
        uint8_t databyte = readByteFile(f);
        if (ed_cnt == 0) {
            if (databyte != 0xED)
                MemESP::writebyte(memoff + memidx++, databyte);
            else
                ed_cnt++;
        }
        else if (ed_cnt == 1) {
            if (databyte != 0xED) {
                MemESP::writebyte(memoff + memidx++, 0xED);
                MemESP::writebyte(memoff + memidx++, databyte);
                ed_cnt = 0;
            }
            else
                ed_cnt++;
        }
        else if (ed_cnt == 2) {
            repcnt = databyte;
            ed_cnt++;
        }
        else if (ed_cnt == 3) {
            repval = databyte;
            for (uint16_t i = 0; i < repcnt; i++)
                MemESP::writebyte(memoff + memidx++, repval);
            ed_cnt = 0;
        }
    }
    #ifdef LOG_Z80_DETAILS
        printf("last byte: %d\n", (memoff+memidx-1));
    #endif
}

void FileZ80::loadCompressedMemPage(FILE *f, uint16_t dataLen, uint8_t* memPage, uint16_t memlen)
{
    uint16_t dataOff = 0;
    uint8_t ed_cnt = 0;
    uint8_t repcnt = 0;
    uint8_t repval = 0;
    uint16_t memidx = 0;

    while(dataOff < dataLen && memidx < memlen) {
        uint8_t databyte = readByteFile(f);
        if (ed_cnt == 0) {
            if (databyte != 0xED)
                memPage[memidx++] = databyte;
            else
                ed_cnt++;
        }
        else if (ed_cnt == 1) {
            if (databyte != 0xED) {
                memPage[memidx++] = 0xED;
                memPage[memidx++] = databyte;
                ed_cnt = 0;
            }
            else
                ed_cnt++;
        }
        else if (ed_cnt == 2) {
            repcnt = databyte;
            ed_cnt++;
        }
        else if (ed_cnt == 3) {
            repval = databyte;
            for (uint16_t i = 0; i < repcnt; i++)
                memPage[memidx++] = repval;
            ed_cnt = 0;
        }
    }
}
