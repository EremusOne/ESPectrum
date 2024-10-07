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

#include "Snapshot.h"
#include "hardconfig.h"
#include "FileUtils.h"
#include "Config.h"
#include "cpuESP.h"
#include "Video.h"
#include "MemESP.h"
#include "ESPectrum.h"
#include "Ports.h"
#include "messages.h"
#include "OSDMain.h"
#include "Tape.h"
#include "AySound.h"
#include "pwm_audio.h"
#include "Z80_JLS/z80.h"
#include "Config.h"
#include "Tape.h"
#include "pwm_audio.h"
#include "AySound.h"
#include "loaders.h"
#include "ZXKeyb.h"
#include "Config.h"

#include <sys/unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <inttypes.h>
#include <string>

using namespace std;

// Change running snapshot
bool LoadSnapshot(string filename, string force_arch, string force_romset, uint8_t force_ALU) {

    bool res = false;

    uint8_t OSDprev = VIDEO::OSD;
    
    if (FileUtils::hasSNAextension(filename)) {

        // OSD::osdCenteredMsg(MSG_LOADING_SNA + (string) ": " + filename.substr(filename.find_last_of("/") + 1), LEVEL_INFO, 0);

        res = FileSNA::load(filename, force_arch, force_romset, force_ALU);

    } else if (FileUtils::hasZ80extension(filename)) {

        // OSD::osdCenteredMsg(MSG_LOADING_Z80 + (string) ": " + filename.substr(filename.find_last_of("/") + 1), LEVEL_INFO, 0);

        res = FileZ80::load(filename);

    } else if (FileUtils::hasExtension(filename, "sp")) {
        
        res = FileSP::load(filename);

    } else if (FileUtils::hasPextension(filename)) {

        res = FileP::load(filename);

    }

    if (res && OSDprev) {
        VIDEO::OSD = OSDprev;
        if (Config::aspect_16_9)
            VIDEO::Draw_OSD169 = VIDEO::MainScreen_OSD;
        else
            VIDEO::Draw_OSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;
        ESPectrum::TapeNameScroller = 0;
    }    

    return res;

}

// ///////////////////////////////////////////////////////////////////////////////

bool FileSNA::load(string sna_fn, string force_arch, string force_romset, uint8_t force_ALU) {

    FILE *file;
    int sna_size;
    string snapshotArch;

    file = fopen(sna_fn.c_str(), "rb");
    if (file==NULL)
    {
        printf("FileSNA: Error opening %s\n",sna_fn.c_str());
        return false;
    }

    printf("Force arch: %s, Force romset: %s, Force ALU: %d\n", force_arch.c_str(),force_romset.c_str(),force_ALU);

    fseek(file,0,SEEK_END);
    sna_size = ftell(file);
    rewind (file);

    // Check snapshot arch
    if (sna_size == SNA_48K_SIZE || sna_size == SNA_48K_WITH_ROM_SIZE) {

        if (force_arch == "" && !Z80Ops::is48) {
            force_arch = "48K";
        }

        // // If using some 48K arch it keeps unmodified. If not, we choose 48k because is SNA format default
        // if (ConfigZ80Ops::is48)
        //     snapshotArch = Config::arch;
        // else    
        //     snapshotArch = "48K";

    } else if ((sna_size == SNA_128K_SIZE1) || (sna_size == SNA_128K_SIZE2)) {

        if (force_arch == "" && Z80Ops::is48) {
            force_arch = "Pentagon";
        }

        // // If using some 128K arch it keeps unmodified. If not, we choose Pentagon because is SNA format default
        // if (!Z80Ops::is48)
        //     snapshotArch = Config::arch;
        // else    
        //     snapshotArch = "Pentagon";

    } else {
        printf("FileSNA::load: bad SNA %s: size = %d\n", sna_fn.c_str(), sna_size);
        return false;
    }

    // Change arch if needed
    if (force_arch != "" && force_arch != Config::arch) {
        
        bool vreset = Config::videomode;

        // If switching between TK models there's no need to reset in vidmodes > 0
        if (force_arch[0] == 'T' && Config::arch[0] == 'T') vreset = false;
    
        Config::requestMachine(force_arch, force_romset);
    
        // Condition this to 50hz mode
        if (vreset) {

            Config::SNA_Path = FileUtils::SNA_Path;
            Config::SNA_begin_row = FileUtils::fileTypes[DISK_SNAFILE].begin_row;
            Config::SNA_focus = FileUtils::fileTypes[DISK_SNAFILE].focus;
            Config::SNA_fdMode = FileUtils::fileTypes[DISK_SNAFILE].fdMode;
            Config::SNA_fileSearch = FileUtils::fileTypes[DISK_SNAFILE].fileSearch;

            Config::TAP_Path = FileUtils::TAP_Path;
            Config::TAP_begin_row = FileUtils::fileTypes[DISK_TAPFILE].begin_row;
            Config::TAP_focus = FileUtils::fileTypes[DISK_TAPFILE].focus;
            Config::TAP_fdMode = FileUtils::fileTypes[DISK_TAPFILE].fdMode;
            Config::TAP_fileSearch = FileUtils::fileTypes[DISK_TAPFILE].fileSearch;

            Config::DSK_Path = FileUtils::DSK_Path;
            Config::DSK_begin_row = FileUtils::fileTypes[DISK_DSKFILE].begin_row;
            Config::DSK_focus = FileUtils::fileTypes[DISK_DSKFILE].focus;
            Config::DSK_fdMode = FileUtils::fileTypes[DISK_DSKFILE].fdMode;
            Config::DSK_fileSearch = FileUtils::fileTypes[DISK_DSKFILE].fileSearch;

            Config::ram_file = sna_fn;
            Config::save();
            OSD::esp_hard_reset(); 
        }                           
    
    } else if (force_romset != "" && force_romset != Config::romSet) {

        Config::requestMachine(Config::arch, force_romset);

    }

    // // Manage arch change
    
    // if (Z80Ops::is128) { // If we are on 128K machine

    //     if (snapshotArch == "48K") {

    //         Config::requestMachine("48K", force_romset);

    //         // Condition this to 50hz mode
    //         if(Config::videomode) {

    //             Config::SNA_Path = FileUtils::SNA_Path;
    //             Config::SNA_begin_row = FileUtils::fileTypes[DISK_SNAFILE].begin_row;
    //             Config::SNA_focus = FileUtils::fileTypes[DISK_SNAFILE].focus;
    //             Config::SNA_fdMode = FileUtils::fileTypes[DISK_SNAFILE].fdMode;
    //             Config::SNA_fileSearch = FileUtils::fileTypes[DISK_SNAFILE].fileSearch;

    //             Config::TAP_Path = FileUtils::TAP_Path;
    //             Config::TAP_begin_row = FileUtils::fileTypes[DISK_TAPFILE].begin_row;
    //             Config::TAP_focus = FileUtils::fileTypes[DISK_TAPFILE].focus;
    //             Config::TAP_fdMode = FileUtils::fileTypes[DISK_TAPFILE].fdMode;
    //             Config::TAP_fileSearch = FileUtils::fileTypes[DISK_TAPFILE].fileSearch;

    //             Config::DSK_Path = FileUtils::DSK_Path;
    //             Config::DSK_begin_row = FileUtils::fileTypes[DISK_DSKFILE].begin_row;
    //             Config::DSK_focus = FileUtils::fileTypes[DISK_DSKFILE].focus;
    //             Config::DSK_fdMode = FileUtils::fileTypes[DISK_DSKFILE].fdMode;
    //             Config::DSK_fileSearch = FileUtils::fileTypes[DISK_DSKFILE].fileSearch;

    //             Config::ram_file = sna_fn;
    //             Config::save();
    //             OSD::esp_hard_reset(); 
    //         }                           
        
    //     } else {
            
    //         if ((force_arch != "") && ((Config::arch != force_arch) || (Config::romSet != force_romset))) {
                
    //             snapshotArch = force_arch;

    //             Config::requestMachine(force_arch, force_romset);

    //             // Condition this to 50hz mode
    //             if(Config::videomode) {

    //                 Config::SNA_Path = FileUtils::SNA_Path;
    //                 Config::SNA_begin_row = FileUtils::fileTypes[DISK_SNAFILE].begin_row;
    //                 Config::SNA_focus = FileUtils::fileTypes[DISK_SNAFILE].focus;
    //                 Config::SNA_fdMode = FileUtils::fileTypes[DISK_SNAFILE].fdMode;
    //                 Config::SNA_fileSearch = FileUtils::fileTypes[DISK_SNAFILE].fileSearch;

    //                 Config::TAP_Path = FileUtils::TAP_Path;
    //                 Config::TAP_begin_row = FileUtils::fileTypes[DISK_TAPFILE].begin_row;
    //                 Config::TAP_focus = FileUtils::fileTypes[DISK_TAPFILE].focus;
    //                 Config::TAP_fdMode = FileUtils::fileTypes[DISK_TAPFILE].fdMode;
    //                 Config::TAP_fileSearch = FileUtils::fileTypes[DISK_TAPFILE].fileSearch;

    //                 Config::DSK_Path = FileUtils::DSK_Path;
    //                 Config::DSK_begin_row = FileUtils::fileTypes[DISK_DSKFILE].begin_row;
    //                 Config::DSK_focus = FileUtils::fileTypes[DISK_DSKFILE].focus;
    //                 Config::DSK_fdMode = FileUtils::fileTypes[DISK_DSKFILE].fdMode;
    //                 Config::DSK_fileSearch = FileUtils::fileTypes[DISK_DSKFILE].fileSearch;

    //                 Config::ram_file = sna_fn;
    //                 Config::save();
    //                 OSD::esp_hard_reset();                            
    //             }

    //         }

    //     }

    // } else if (Z80Ops::is48) {

    //     if (snapshotArch == "Pentagon") {

    //         if (force_arch == "")
    //             Config::requestMachine("Pentagon", "");
    //         else {
    //             snapshotArch = force_arch;
    //             Config::requestMachine(force_arch, force_romset);
    //         }

    //         // Condition this to 50hz mode
    //         if(Config::videomode) {

    //             Config::SNA_Path = FileUtils::SNA_Path;
    //             Config::SNA_begin_row = FileUtils::fileTypes[DISK_SNAFILE].begin_row;
    //             Config::SNA_focus = FileUtils::fileTypes[DISK_SNAFILE].focus;
    //             Config::SNA_fdMode = FileUtils::fileTypes[DISK_SNAFILE].fdMode;
    //             Config::SNA_fileSearch = FileUtils::fileTypes[DISK_SNAFILE].fileSearch;

    //             Config::TAP_Path = FileUtils::TAP_Path;
    //             Config::TAP_begin_row = FileUtils::fileTypes[DISK_TAPFILE].begin_row;
    //             Config::TAP_focus = FileUtils::fileTypes[DISK_TAPFILE].focus;
    //             Config::TAP_fdMode = FileUtils::fileTypes[DISK_TAPFILE].fdMode;
    //             Config::TAP_fileSearch = FileUtils::fileTypes[DISK_TAPFILE].fileSearch;

    //             Config::DSK_Path = FileUtils::DSK_Path;
    //             Config::DSK_begin_row = FileUtils::fileTypes[DISK_DSKFILE].begin_row;
    //             Config::DSK_focus = FileUtils::fileTypes[DISK_DSKFILE].focus;
    //             Config::DSK_fdMode = FileUtils::fileTypes[DISK_DSKFILE].fdMode;
    //             Config::DSK_fileSearch = FileUtils::fileTypes[DISK_DSKFILE].fileSearch;

    //             Config::ram_file = sna_fn;
    //             Config::save();
    //             OSD::esp_hard_reset();                            
    //         }

    //     }

    // }
    
    // Change ALU to snapshot one if present
    if (force_ALU != 0xff && force_ALU != Config::ALUTK) {

        Config::ALUTK = force_ALU;

        // Condition this to 50hz mode
        if(Config::videomode) {

            Config::SNA_Path = FileUtils::SNA_Path;
            Config::SNA_begin_row = FileUtils::fileTypes[DISK_SNAFILE].begin_row;
            Config::SNA_focus = FileUtils::fileTypes[DISK_SNAFILE].focus;
            Config::SNA_fdMode = FileUtils::fileTypes[DISK_SNAFILE].fdMode;
            Config::SNA_fileSearch = FileUtils::fileTypes[DISK_SNAFILE].fileSearch;

            Config::TAP_Path = FileUtils::TAP_Path;
            Config::TAP_begin_row = FileUtils::fileTypes[DISK_TAPFILE].begin_row;
            Config::TAP_focus = FileUtils::fileTypes[DISK_TAPFILE].focus;
            Config::TAP_fdMode = FileUtils::fileTypes[DISK_TAPFILE].fdMode;
            Config::TAP_fileSearch = FileUtils::fileTypes[DISK_TAPFILE].fileSearch;

            Config::DSK_Path = FileUtils::DSK_Path;
            Config::DSK_begin_row = FileUtils::fileTypes[DISK_DSKFILE].begin_row;
            Config::DSK_focus = FileUtils::fileTypes[DISK_DSKFILE].focus;
            Config::DSK_fdMode = FileUtils::fileTypes[DISK_DSKFILE].fdMode;
            Config::DSK_fileSearch = FileUtils::fileTypes[DISK_DSKFILE].fileSearch;

            Config::ram_file = sna_fn;
            Config::save();
            OSD::esp_hard_reset(); 

        }                           

    }

    ESPectrum::reset();

    // printf("FileSNA::load: Opening %s: size = %d\n", sna_fn.c_str(), sna_size);

    // Read in the registers
    Z80::setRegI(readByteFile(file));

    Z80::setRegHLx(readWordFileLE(file));
    Z80::setRegDEx(readWordFileLE(file));
    Z80::setRegBCx(readWordFileLE(file));
    Z80::setRegAFx(readWordFileLE(file));

    Z80::setRegHL(readWordFileLE(file));
    Z80::setRegDE(readWordFileLE(file));
    Z80::setRegBC(readWordFileLE(file));

    Z80::setRegIY(readWordFileLE(file));
    Z80::setRegIX(readWordFileLE(file));

    uint8_t inter = readByteFile(file);
    Z80::setIFF2(inter & 0x04 ? true : false);
    Z80::setIFF1(Z80::isIFF2());
    Z80::setRegR(readByteFile(file));

    Z80::setRegAF(readWordFileLE(file));
    Z80::setRegSP(readWordFileLE(file));

    Z80::setIM((Z80::IntMode)(readByteFile(file)));

    VIDEO::borderColor = readByteFile(file);
    VIDEO::brd = VIDEO::border32[VIDEO::borderColor];

    if (Z80Ops::is48 && sna_size == SNA_48K_WITH_ROM_SIZE) {
        // Load ROM if present
        readBlockFile(file, MemESP::ram[1], 0x4000);
        MemESP::ramCurrent[0] = MemESP::ram[1];
        MemESP::SPRom = true;
    }

    // read 48K memory
    readBlockFile(file, MemESP::ram[5], 0x4000);
    readBlockFile(file, MemESP::ram[2], 0x4000);
    readBlockFile(file, MemESP::ram[0], 0x4000);

    if (Z80Ops::is48) {

        // in 48K mode, pop PC from stack
        uint16_t SP = Z80::getRegSP();
        Z80::setRegPC(MemESP::readword(SP));
        Z80::setRegSP(SP + 2);

    } else {

        // in 128K mode, recover stored PC
        uint16_t sna_PC = readWordFileLE(file);
        Z80::setRegPC(sna_PC);

        // tmp_port contains page switching status, including current page number (latch)
        uint8_t tmp_port = readByteFile(file);
        uint8_t tmp_latch = tmp_port & 0x07;

        // copy what was read into page 0 to correct page
        memcpy(MemESP::ram[tmp_latch], MemESP::ram[0], 0x4000);

        uint8_t tr_dos = readByteFile(file);     // Check if TR-DOS is paged
        
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

        // if (Z80Ops::isPentagon) CPU::tstates = 22; // Pentagon SNA load fix... still dunno why this works but it works

    }
    
    fclose(file);

    return true;

}

// ///////////////////////////////////////////////////////////////////////////////

bool FileSNA::isPersistAvailable(string filename) {

    FILE *f = fopen(filename.c_str(), "rb");
    if (f == NULL)
        return false;
    else
        fclose(f);

    return true;

}

// ///////////////////////////////////////////////////////////////////////////////

bool check_and_create_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if ((st.st_mode & S_IFDIR) != 0) {
            // printf("Directory exists\n");
            return true;
        } else {
            // printf("Path exists but it is not a directory\n");
            // Create the directory
            if (mkdir(path, 0755) == 0) {
                // printf("Directory created\n");
                return true;
            } else {
                printf("Failed to create directory\n");
                return false;
            }
        }
    } else {
        // printf("Directory does not exist\n");
        // Create the directory
        if (mkdir(path, 0755) == 0) {
            // printf("Directory created\n");
            return true;
        } else {
            printf("Failed to create directory\n");
            return false;
        }
    }
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

    file = fopen(sna_file.c_str(), "wb");
    if (file==NULL)
    {
        printf("FileSNA: Error opening %s for writing",sna_file.c_str());
        return false;
    }

    // write registers: begin with I
    writeByteFile(Z80::getRegI(), file);

    writeWordFileLE(Z80::getRegHLx(), file);
    writeWordFileLE(Z80::getRegDEx(), file);
    writeWordFileLE(Z80::getRegBCx(), file);
    writeWordFileLE(Z80::getRegAFx(), file);

    writeWordFileLE(Z80::getRegHL(), file);
    writeWordFileLE(Z80::getRegDE(), file);
    writeWordFileLE(Z80::getRegBC(), file);

    writeWordFileLE(Z80::getRegIY(), file);
    writeWordFileLE(Z80::getRegIX(), file);

    uint8_t inter = Z80::isIFF2() ? 0x04 : 0;
    writeByteFile(inter, file);
    writeByteFile(Z80::getRegR(), file);

    writeWordFileLE(Z80::getRegAF(), file);

    uint16_t SP = Z80::getRegSP();
    
    // if (Config::arch == "48K" || Config::arch == "TK90X" || Config::arch == "TK95") {
    if (Z80Ops::is48) {    
        // decrement stack pointer it for pushing PC to stack, only on 48K
        SP -= 2;
        MemESP::writeword(SP, Z80::getRegPC());
    }
    writeWordFileLE(SP, file);

    writeByteFile(Z80::getIM(), file);
    
    uint8_t bordercol = VIDEO::borderColor;
    writeByteFile(bordercol, file);

    // write 16K ROM page if we've loaded it previously

    if (Z80Ops::is48 && MemESP::SPRom) {
        if (!writeMemPage(1, file, blockMode)) {
            fclose(file);
            return false;
        }
    }

    // write RAM pages in 48K address space (0x4000 - 0xFFFF)
    uint8_t pages[3] = {5, 2, 0};

    if (Z80Ops::is128 || Z80Ops::isPentagon)    
        pages[2] = MemESP::bankLatch;

    for (uint8_t ipage = 0; ipage < 3; ipage++) {
        uint8_t page = pages[ipage];
        if (!writeMemPage(page, file, blockMode)) {
            fclose(file);
            return false;
        }
    }

    if (Z80Ops::is128 || Z80Ops::isPentagon) {
    // if (Config::arch != "48K") {

        // write pc
        writeWordFileLE( Z80::getRegPC(), file);
        // printf("PC: %u\n",(unsigned int)Z80::getRegPC());

        // write memESP bank control port
        uint8_t tmp_port = MemESP::bankLatch;
        bitWrite(tmp_port, 3, MemESP::videoLatch);
        bitWrite(tmp_port, 4, MemESP::romLatch);
        bitWrite(tmp_port, 5, MemESP::pagingLock);
        writeByteFile(tmp_port, file);
        // printf("7FFD: %u\n",(unsigned int)tmp_port);

        if (ESPectrum::trdos)
            writeByteFile(1, file);     // TR-DOS paged
        else            
            writeByteFile(0, file);     // TR-DOS not paged

        // write remaining ram pages
        for (int page = 0; page < 8; page++) {
            if (page != MemESP::bankLatch && page != 2 && page != 5) {
                if (!writeMemPage(page, file, blockMode)) {
                    fclose(file);
                    return false;

                }
            }
        }
    }

    fclose(file);

    return true;

}

static uint16_t mkword(uint8_t lobyte, uint8_t hibyte) {
    return lobyte | (hibyte << 8);
}

bool FileZ80::keepArch = false;      

bool FileZ80::load(string z80_fn) {

    FILE *file;

    file = fopen(z80_fn.c_str(), "rb");
    if (file == NULL) {
        printf("FileZ80: Error opening %s\n",z80_fn.c_str());
        return false;
    }

    // Check Z80 version and arch
    uint8_t z80version;
    uint8_t mch;
    string z80_arch = "";
    uint16_t ahb_len;

    fseek(file,6,SEEK_SET);

    if (mkword(readByteFile(file),readByteFile(file)) != 0) { // Version 1

        z80version = 1;
        mch = 0;
        z80_arch = "48K";

    } else { // Version 2 o 3

        fseek(file,30,SEEK_SET);
        ahb_len = mkword(readByteFile(file),readByteFile(file));

        // additional header block length
        if (ahb_len == 23)
            z80version = 2;
        else if (ahb_len == 54 || ahb_len == 55)
            z80version = 3;
        else {
            OSD::osdCenteredMsg("Z80 load: unknown version", LEVEL_ERROR);
            printf("Z80.load: unknown version, ahblen = %u\n", (unsigned int) ahb_len);
            fclose(file);
            return false;
        }

        fseek(file,34,SEEK_SET);
        mch = readByteFile(file); // Machine

        if (z80version == 2) {
            if (mch == 0) z80_arch = "48K";
            if (mch == 1) z80_arch = "48K"; // + if1
            // if (mch == 2) z80_arch = "SAMRAM";
            if (mch == 3) z80_arch = "128K";
            if (mch == 4) z80_arch = "128K"; // + if1
        }
        else if (z80version == 3) {
            if (mch == 0) z80_arch = "48K";
            if (mch == 1) z80_arch = "48K"; // + if1
            // if (mch == 2) z80_arch = "SAMRAM";
            if (mch == 3) z80_arch = "48K"; // + mgt
            if (mch == 4) z80_arch = "128K";
            if (mch == 5) z80_arch = "128K"; // + if1
            if (mch == 6) z80_arch = "128K"; // + mgt
            if (mch == 7) z80_arch = "128K"; // Spectrum +3
            if (mch == 9) z80_arch = "Pentagon";
            if (mch == 12) z80_arch = "128K"; // Spectrum +2
            if (mch == 13) z80_arch = "128K"; // Spectrum +2A            
        }

    }

    // printf("Z80 version %u, AHB Len: %u, machine code: %u\n",(unsigned char)z80version,(unsigned int)ahb_len, (unsigned char)mch);

    if (z80_arch == "") {
        OSD::osdCenteredMsg("Z80 load: unknown machine", LEVEL_ERROR);
        printf("Z80.load: unknown machine, machine code = %u\n", (unsigned char)mch);
        fclose(file);
        return false;
    }

    // Force keepArch for testing
    // keepArch = true;

    if (keepArch) {

        if (z80_arch == "48K") {
            if (Config::arch == "128K" || Config::arch == "Pentagon") keepArch = false;
        } else {
            if (Config::arch == "48K" || Config::arch == "TK90X" || Config::arch == "TK95") keepArch = false;
        }

    }

    // printf("fileTypes -> Path: %s, begin_row: %d, focus: %d\n",FileUtils::SNA_Path.c_str(),FileUtils::fileTypes[DISK_SNAFILE].begin_row,FileUtils::fileTypes[DISK_SNAFILE].focus);                    
    // printf("Config    -> Path: %s, begin_row: %d, focus: %d\n",Config::Path.c_str(),(int)Config::begin_row,(int)Config::focus);                    

    // Manage arch change
    if (Config::arch != z80_arch) {

        if (!keepArch) {

            string z80_romset = "";

            // printf("z80_arch: %s mch: %d pref_romset48: %s pref_romset128: %s z80_romset: %s\n",z80_arch.c_str(),mch,Config::pref_romSet_48.c_str(),Config::pref_romSet_128.c_str(),z80_romset.c_str());

            if (z80_arch == "48K") {
                if (Config::pref_romSet_48 == "48K" || Config::pref_romSet_48 == "48Kes")
                    z80_romset = Config::pref_romSet_48;
            } else
            if (z80_arch == "128K") {
                if (mch == 12) { // +2
                    if (Config::pref_romSet_128 == "+2" || Config::pref_romSet_128 == "+2es")
                        z80_romset = Config::pref_romSet_128;
                    else
                        z80_romset = "+2";
                } else {
                    if (Config::pref_romSet_128 == "128K" || Config::pref_romSet_128 == "128Kes")
                        z80_romset = Config::pref_romSet_128;
                }
            }

            // printf("z80_arch: %s mch: %d pref_romset48: %s pref_romset128: %s z80_romset: %s\n",z80_arch.c_str(),mch,Config::pref_romSet_48.c_str(),Config::pref_romSet_128.c_str(),z80_romset.c_str());
            
            Config::requestMachine(z80_arch, z80_romset);

            // Condition this to 50hz mode
            if(Config::videomode) {

                Config::SNA_Path = FileUtils::SNA_Path;
                Config::SNA_begin_row = FileUtils::fileTypes[DISK_SNAFILE].begin_row;
                Config::SNA_focus = FileUtils::fileTypes[DISK_SNAFILE].focus;
                Config::SNA_fdMode = FileUtils::fileTypes[DISK_SNAFILE].fdMode;
                Config::SNA_fileSearch = FileUtils::fileTypes[DISK_SNAFILE].fileSearch;

                Config::TAP_Path = FileUtils::TAP_Path;
                Config::TAP_begin_row = FileUtils::fileTypes[DISK_TAPFILE].begin_row;
                Config::TAP_focus = FileUtils::fileTypes[DISK_TAPFILE].focus;
                Config::TAP_fdMode = FileUtils::fileTypes[DISK_TAPFILE].fdMode;
                Config::TAP_fileSearch = FileUtils::fileTypes[DISK_TAPFILE].fileSearch;

                Config::DSK_Path = FileUtils::DSK_Path;
                Config::DSK_begin_row = FileUtils::fileTypes[DISK_DSKFILE].begin_row;
                Config::DSK_focus = FileUtils::fileTypes[DISK_DSKFILE].focus;
                Config::DSK_fdMode = FileUtils::fileTypes[DISK_DSKFILE].fdMode;
                Config::DSK_fileSearch = FileUtils::fileTypes[DISK_DSKFILE].fileSearch;

                Config::ram_file = z80_fn;
                Config::save();
                OSD::esp_hard_reset(); 
            }                           

        }

    } else {

        if (z80_arch == "128K") {
            
            string z80_romset = "";
            
            // printf("z80_arch: %s mch: %d pref_romset48: %s pref_romset128: %s z80_romset: %s\n",z80_arch.c_str(),mch,Config::pref_romSet_48.c_str(),Config::pref_romSet_128.c_str(),z80_romset.c_str());

            if (mch == 12) { // +2

                if (Config::romSet != "+2" && Config::romSet != "+2es" && Config::romSet != "128Kcs") {

                    if (Config::pref_romSet_128 == "+2" || Config::pref_romSet_128 == "+2es")
                        z80_romset = Config::pref_romSet_128;
                    else
                        z80_romset = "+2";

                    Config::requestMachine(z80_arch, z80_romset);        

                }

            } else {

                if (Config::romSet != "128K" && Config::romSet != "128Kes" && Config::romSet != "128Kcs") {

                    if (Config::pref_romSet_128 == "128K" || Config::pref_romSet_128 == "128Kes")
                        z80_romset = Config::pref_romSet_128;
                    else
                        z80_romset = "128K";

                    Config::requestMachine(z80_arch, z80_romset);        
                }

            }

            // printf("z80_arch: %s mch: %d pref_romset48: %s pref_romset128: %s z80_romset: %s\n",z80_arch.c_str(),mch,Config::pref_romSet_48.c_str(),Config::pref_romSet_128.c_str(),z80_romset.c_str());

        }

    }
    
    ESPectrum::reset();

    keepArch = false;

    // Get file size
    fseek(file,0,SEEK_END);
    uint32_t file_size = ftell(file);
    rewind (file);

    uint32_t dataOffset = 0;

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
    
    // Z80::setRegBC (mkword(header[2], header[3]));
    Z80::setRegC(header[2]);    
    Z80::setRegB(header[3]);
    
    // Z80::setRegHL (mkword(header[4], header[5]));
    Z80::setRegL(header[4]);    
    Z80::setRegH(header[5]);

    Z80::setRegPC (mkword(header[6], header[7]));
    Z80::setRegSP (mkword(header[8], header[9]));

    Z80::setRegI  (       header[10]);

    // Z80::setRegR  (       header[11]);
    uint8_t regR = header[11] & 0x7f;
    if ((header[12] & 0x01) != 0) {
        regR |= 0x80;
    }
    Z80::setRegR(regR);

    b12 =                 header[12];

    VIDEO::borderColor = (b12 >> 1) & 0x07;
    VIDEO::brd = VIDEO::border32[VIDEO::borderColor];
    
    // Z80::setRegDE (mkword(header[13], header[14]));
    Z80::setRegE(header[13]);
    Z80::setRegD(header[14]);

    // Z80::setRegBCx(mkword(header[15], header[16]));
    Z80::setRegCx(header[15]);
    Z80::setRegBx(header[16]);

    // Z80::setRegDEx(mkword(header[17], header[18]));
    Z80::setRegEx(header[17]);
    Z80::setRegDx(header[18]);

    // Z80::setRegHLx(mkword(header[19], header[20]));
    Z80::setRegLx(header[19]);
    Z80::setRegHx(header[20]);

    // Z80::setRegAFx(mkword(header[22], header[21])); // watch out for order!!!
    Z80::setRegAx(header[21]);
    Z80::setRegFx(header[22]);

    Z80::setRegIY (mkword(header[23], header[24]));
    Z80::setRegIX (mkword(header[25], header[26]));

    Z80::setIFF1  (       header[27] ? true : false);
    Z80::setIFF2  (       header[28] ? true : false);
    b29 =                 header[29];
    Z80::setIM((Z80::IntMode)(b29 & 0x03));

    // spectrum.setIssue2((z80Header1[29] & 0x04) != 0); // TO DO: Implement this

    uint16_t RegPC = Z80::getRegPC();
   
    bool dataCompressed = (b12 & 0x20) ? true : false;

    if (z80version == 1) {

        // version 1, the simplest, 48K only.
        uint32_t memRawLength = file_size - dataOffset;

        if (dataCompressed) {
            // assuming stupid 00 ED ED 00 terminator present, should check for it instead of assuming
            uint16_t dataLen = (uint16_t)(memRawLength - 4);

            // load compressed data into memory
            loadCompressedMemData(file, dataLen, 0x4000, 0xC000);
        } else {
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

    } else {

        // read 2 more bytes
        for (uint8_t i = 30; i < 32; i++) {
            header[i] = readByteFile(file);
            dataOffset ++;
        }

        // additional header block length
        uint16_t ahblen = mkword(header[30], header[31]);

        // read additional header block
        for (uint8_t i = 32; i < 32 + ahblen; i++) {
            header[i] = readByteFile(file);
            dataOffset ++;
        }

        // program counter
        RegPC = mkword(header[32], header[33]);
        Z80::setRegPC(RegPC);

        if (z80_arch == "48K") {

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
                
                uint16_t memoff = pageStart[hdr2];

                if (compDataLen == 0xffff) {                 

                    // Uncompressed data

                    compDataLen = 0x4000;

                    for (int i = 0; i < compDataLen; i++)                    
                        MemESP::writebyte(memoff + i, readByteFile(file));

                } else {

                    loadCompressedMemData(file, compDataLen, memoff, 0x4000);

                }

                dataOffset += compDataLen;

            }

        } else if ((z80_arch == "128K") || (z80_arch == "Pentagon")) {
            
            // paging register
            uint8_t b35 = header[35];
            // printf("Paging register: %u\n",b35);
            MemESP::videoLatch = bitRead(b35, 3);
            MemESP::romLatch = bitRead(b35, 4);
            MemESP::pagingLock = bitRead(b35, 5);
            MemESP::bankLatch = b35 & 0x07;
            MemESP::romInUse = MemESP::romLatch;

            uint8_t* pages[12] = {
                MemESP::rom[0], MemESP::rom[2], MemESP::rom[1],
                MemESP::ram[0], MemESP::ram[1], MemESP::ram[2], MemESP::ram[3],
                MemESP::ram[4], MemESP::ram[5], MemESP::ram[6], MemESP::ram[7],
                MemESP::rom[3] };

            // const char* pagenames[12] = { "rom0", "IDP", "rom1",
            //     "ram0", "ram1", "ram2", "ram3", "ram4", "ram5", "ram6", "ram7", "MFR" };

            uint32_t dataLen = file_size;
            while (dataOffset < dataLen) {

                uint8_t hdr0 = readByteFile(file); dataOffset ++;
                uint8_t hdr1 = readByteFile(file); dataOffset ++;
                uint8_t hdr2 = readByteFile(file); dataOffset ++;
                uint16_t compDataLen = mkword(hdr0, hdr1);
                
                if (compDataLen == 0xffff) { 

                    // load uncompressed data into memory
                    // printf("Loading uncompressed data\n");

                    compDataLen = 0x4000;

                    if ((hdr2 > 2) && (hdr2 < 11)) 
                        for (int i = 0; i < compDataLen; i++)
                            pages[hdr2][i] = readByteFile(file);

                } else {

                    // Block is compressed

                    if ((hdr2 > 2) && (hdr2 < 11)) 
                        loadCompressedMemPage(file, compDataLen, pages[hdr2], 0x4000);

                }

                dataOffset += compDataLen;

            }

            MemESP::ramCurrent[0] = MemESP::rom[MemESP::romInUse];
            MemESP::ramCurrent[3] = MemESP::ram[MemESP::bankLatch];
            MemESP::ramContended[3] = Z80Ops::isPentagon ? false : (MemESP::bankLatch & 0x01 ? true: false);

            VIDEO::grmem = MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5];

        }
    }

    fclose(file);

    return true;

}

void FileZ80::loadCompressedMemData(FILE *f, uint16_t dataLen, uint16_t memoff, uint16_t memlen) {

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

void FileZ80::loader48() {

    unsigned char *z80_array = (unsigned char *) load48;
    uint32_t dataOffset = 86;

    ESPectrum::reset();

    // begin loading registers
    Z80::setRegA  (z80_array[0]);
    Z80::setFlags (z80_array[1]);
    Z80::setRegBC (mkword(z80_array[2], z80_array[3]));
    Z80::setRegHL (mkword(z80_array[4], z80_array[5]));
    Z80::setRegPC (mkword(z80_array[6], z80_array[7]));
    Z80::setRegSP (mkword(z80_array[8], z80_array[9]));
    Z80::setRegI  (z80_array[10]);

    uint8_t regR = z80_array[11] & 0x7f;
    if ((z80_array[12] & 0x01) != 0) {
        regR |= 0x80;
    }
    Z80::setRegR(regR);

    VIDEO::borderColor = (z80_array[12] >> 1) & 0x07;
    VIDEO::brd = VIDEO::border32[VIDEO::borderColor];

    Z80::setRegDE (mkword(z80_array[13], z80_array[14]));
    Z80::setRegBCx(mkword(z80_array[15], z80_array[16]));
    Z80::setRegDEx(mkword(z80_array[17], z80_array[18]));
    Z80::setRegHLx(mkword(z80_array[19], z80_array[20]));
    
    Z80::setRegAx(z80_array[21]);
    Z80::setRegFx(z80_array[22]);
    
    Z80::setRegIY (mkword(z80_array[23], z80_array[24]));
    Z80::setRegIX (mkword(z80_array[25], z80_array[26]));
    Z80::setIFF1  (z80_array[27] ? true : false);
    Z80::setIFF2  (z80_array[28] ? true : false);
    Z80::setIM((Z80::IntMode)(z80_array[29] & 0x03));

    // program counter
    uint16_t RegPC = mkword(z80_array[32], z80_array[33]);
    Z80::setRegPC(RegPC);

    z80_array += dataOffset;

    MemESP::romLatch = 0;
    MemESP::romInUse = 0;
    MemESP::bankLatch = 0;
    MemESP::pagingLock = 1;
    MemESP::videoLatch = 0;

    uint16_t pageStart[12] = {0, 0, 0, 0, 0x8000, 0xC000, 0, 0, 0x4000, 0, 0};

    uint32_t dataLen = sizeof(load48);
    while (dataOffset < dataLen) {
        uint8_t hdr0 = z80_array[0]; dataOffset ++;
        uint8_t hdr1 = z80_array[1]; dataOffset ++;
        uint8_t hdr2 = z80_array[2]; dataOffset ++;
        z80_array += 3;
        uint16_t compDataLen = mkword(hdr0, hdr1);
        
        uint16_t memoff = pageStart[hdr2];
        
        {

            uint16_t dataOff = 0;
            uint8_t ed_cnt = 0;
            uint8_t repcnt = 0;
            uint8_t repval = 0;
            uint16_t memidx = 0;

            while(dataOff < compDataLen && memidx < 0x4000) {
                uint8_t databyte = z80_array[0]; z80_array ++;
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

        }

        dataOffset += compDataLen;

    }

    memset(MemESP::ram[2],0,0x4000);

    MemESP::ramCurrent[0] = MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[3] = MemESP::ram[MemESP::bankLatch];
    MemESP::ramContended[3] = false;

    VIDEO::grmem = MemESP::ram[5];

}

void FileZ80::loader128() {

    unsigned char *z80_array;
    uint32_t dataLen;

    if (Config::arch == "128K") {

        z80_array = (unsigned char *) load128;
        dataLen = sizeof(load128);

        if (Config::romSet == "128K") {
            // printf("128K\n");
            z80_array = (unsigned char *) load128;
            dataLen = sizeof(load128);
        } else if (Config::romSet == "128Kes") {
            // printf("128Kes\n");
            z80_array = (unsigned char *) load128spa;
            dataLen = sizeof(load128spa);
        } else if (Config::romSet == "+2") {
            // printf("+2\n");
            z80_array = (unsigned char *) loadplus2;
            dataLen = sizeof(loadplus2);
        } else if (Config::romSet == "+2es") {
            // printf("+2es\n");
            z80_array = (unsigned char *) loadplus2;
            dataLen = sizeof(loadplus2);
        } else if (Config::romSet == "ZX81+") {
            // printf("ZX81+\n");
            z80_array = (unsigned char *) loadzx81;
            dataLen = sizeof(loadzx81);
        }


    } else if (Config::arch == "Pentagon") {

        z80_array = (unsigned char *) loadpentagon;
        dataLen = sizeof(loadpentagon);

    }

    // unsigned char *z80_array = Z80Ops::is128 ? (unsigned char *) load128spa : (unsigned char *) loadpentagon;    
    // uint32_t dataLen = Z80Ops::is128 ? sizeof(load128spa) : sizeof(loadpentagon);

    uint32_t dataOffset = 86;

    ESPectrum::reset();

    // begin loading registers
    Z80::setRegA  (z80_array[0]);
    Z80::setFlags (z80_array[1]);
    Z80::setRegBC (mkword(z80_array[2], z80_array[3]));
    Z80::setRegHL (mkword(z80_array[4], z80_array[5]));
    Z80::setRegPC (mkword(z80_array[6], z80_array[7]));
    Z80::setRegSP (mkword(z80_array[8], z80_array[9]));
    Z80::setRegI  (z80_array[10]);

    uint8_t regR = z80_array[11] & 0x7f;
    if ((z80_array[12] & 0x01) != 0) {
        regR |= 0x80;
    }
    Z80::setRegR(regR);

    VIDEO::borderColor = (z80_array[12] >> 1) & 0x07;
    VIDEO::brd = VIDEO::border32[VIDEO::borderColor];

    Z80::setRegDE (mkword(z80_array[13], z80_array[14]));
    Z80::setRegBCx(mkword(z80_array[15], z80_array[16]));
    Z80::setRegDEx(mkword(z80_array[17], z80_array[18]));
    Z80::setRegHLx(mkword(z80_array[19], z80_array[20]));
    
    Z80::setRegAx(z80_array[21]);
    Z80::setRegFx(z80_array[22]);
    
    Z80::setRegIY (mkword(z80_array[23], z80_array[24]));
    Z80::setRegIX (mkword(z80_array[25], z80_array[26]));
    Z80::setIFF1  (z80_array[27] ? true : false);
    Z80::setIFF2  (z80_array[28] ? true : false);
    Z80::setIM((Z80::IntMode)(z80_array[29] & 0x03));

    // program counter
    Z80::setRegPC(mkword(z80_array[32], z80_array[33]));

    // paging register
    MemESP::pagingLock = bitRead(z80_array[35], 5);
    MemESP::romLatch = bitRead(z80_array[35], 4);
    MemESP::videoLatch = bitRead(z80_array[35], 3);
    MemESP::bankLatch = z80_array[35] & 0x07;
    MemESP::romInUse = MemESP::romLatch;

    z80_array += dataOffset;

    uint8_t* pages[12] = {
        MemESP::rom[0], MemESP::rom[2], MemESP::rom[1],
        MemESP::ram[0], MemESP::ram[1], MemESP::ram[2], MemESP::ram[3],
        MemESP::ram[4], MemESP::ram[5], MemESP::ram[6], MemESP::ram[7],
        MemESP::rom[3] };

    while (dataOffset < dataLen) {
        uint8_t hdr0 = z80_array[0]; dataOffset ++;
        uint8_t hdr1 = z80_array[1]; dataOffset ++;
        uint8_t hdr2 = z80_array[2]; dataOffset ++;
        z80_array += 3;
        uint16_t compDataLen = mkword(hdr0, hdr1);

        {
            uint16_t dataOff = 0;
            uint8_t ed_cnt = 0;
            uint8_t repcnt = 0;
            uint8_t repval = 0;
            uint16_t memidx = 0;

            while(dataOff < compDataLen && memidx < 0x4000) {
                uint8_t databyte = z80_array[0];
                z80_array ++;
                if (ed_cnt == 0) {
                    if (databyte != 0xED)
                        pages[hdr2][memidx++] = databyte;
                    else
                        ed_cnt++;
                } else if (ed_cnt == 1) {
                    if (databyte != 0xED) {
                        pages[hdr2][memidx++] = 0xED;
                        pages[hdr2][memidx++] = databyte;
                        ed_cnt = 0;
                    } else
                        ed_cnt++;
                } else if (ed_cnt == 2) {
                    repcnt = databyte;
                    ed_cnt++;
                } else if (ed_cnt == 3) {
                    repval = databyte;
                    for (uint16_t i = 0; i < repcnt; i++)
                        pages[hdr2][memidx++] = repval;
                    ed_cnt = 0;
                }
            }
        }

        dataOffset += compDataLen;

    }

    // Empty void ram pages
    memset(MemESP::ram[1],0,0x4000);

    // ZX81+ loader has block 3 void and has info on block5
    if (Config::romSet128 == "ZX81+")
        memset(MemESP::ram[0],0,0x4000);
    else
        memset(MemESP::ram[2],0,0x4000);

    memset(MemESP::ram[3],0,0x4000);
    memset(MemESP::ram[4],0,0x4000);
    memset(MemESP::ram[6],0,0x4000);
    
    MemESP::ramCurrent[0] = MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[3] = MemESP::ram[MemESP::bankLatch];
    MemESP::ramContended[3] = Z80Ops::isPentagon ? false : (MemESP::bankLatch & 0x01 ? true: false);

    VIDEO::grmem = MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5];

}

// ///////////////////////////////////////////////////////////////////////////////

bool FileP::load(string p_fn) {

    FILE *file;
    int p_size;

    file = fopen(p_fn.c_str(), "rb");
    if (file==NULL) {
        printf("FileP: Error opening %s\n",p_fn.c_str());
        return false;
    }

    fseek(file,0,SEEK_END);
    p_size = ftell(file);
    rewind (file);

    if (p_size > (0x4000 - 9)) {
        printf("FileP: Invalid .P file %s\n",p_fn.c_str());
        return false;
    }

    // Manage arch change
    if (Config::arch != "128K" || Config::romSet != "ZX81+") {
        Config::requestMachine("128K", "ZX81+");
        // Condition this to 50hz mode
        if(Config::videomode) {

            Config::SNA_Path = FileUtils::SNA_Path;
            Config::SNA_begin_row = FileUtils::fileTypes[DISK_SNAFILE].begin_row;
            Config::SNA_focus = FileUtils::fileTypes[DISK_SNAFILE].focus;
            Config::SNA_fdMode = FileUtils::fileTypes[DISK_SNAFILE].fdMode;
            Config::SNA_fileSearch = FileUtils::fileTypes[DISK_SNAFILE].fileSearch;

            Config::TAP_Path = FileUtils::TAP_Path;
            Config::TAP_begin_row = FileUtils::fileTypes[DISK_TAPFILE].begin_row;
            Config::TAP_focus = FileUtils::fileTypes[DISK_TAPFILE].focus;
            Config::TAP_fdMode = FileUtils::fileTypes[DISK_TAPFILE].fdMode;
            Config::TAP_fileSearch = FileUtils::fileTypes[DISK_TAPFILE].fileSearch;

            Config::DSK_Path = FileUtils::DSK_Path;
            Config::DSK_begin_row = FileUtils::fileTypes[DISK_DSKFILE].begin_row;
            Config::DSK_focus = FileUtils::fileTypes[DISK_DSKFILE].focus;
            Config::DSK_fdMode = FileUtils::fileTypes[DISK_DSKFILE].fdMode;
            Config::DSK_fileSearch = FileUtils::fileTypes[DISK_DSKFILE].fileSearch;

            Config::ram_file = p_fn;
            Config::save();
            OSD::esp_hard_reset(); 
        }                           
    }

    FileZ80::loader128();

    uint16_t address = 16393;
    uint8_t page = address >> 14;
    fread(&MemESP::ramCurrent[page][address & 0x3fff], p_size, 1, file);

    fclose(file);

    return true;

}

// ///////////////////////////////////////////////////////////////////////////////
/*
      Ficheros *.SP:
Offset    Longitud    Descripci¢n
------   ----------  -------------------
  0       2 bytes    "SP" (53h, 50h) Signatura.
  2       1 palabra  Longitud del programa en bytes (el emulador actualmente
                     s¢lo genera programas de 49152 bytes)
  4       1 palabra  Posici¢n inicial del programa (el emulador actualmente
                     s¢lo genera programas que comiencen en la pos. 16384)
  6       1 palabra  Registro BC del Z80
  8       1 palabra  Registro DE del Z80
 10       1 palabra  Registro HL del Z80
 12       1 palabra  Registro AF del Z80
 14       1 palabra  Registro IX del Z80
 16       1 palabra  Registro IY del Z80
 18       1 palabra  Registro BC' del Z80
 20       1 palabra  Registro DE' del Z80
 22       1 palabra  Registro HL' del Z80
 24       1 palabra  Registro AF' del Z80
 26       1 byte     Registro R (de refresco) del Z80
 27       1 byte     Registro I (de interrupciones) del Z80
 28       1 palabra  Registro SP del Z80
 30       1 palabra  Registro PC del Z80
 32       1 palabra  Reservada para uso futuro, siempre 0
 34       1 byte     Color del borde al comenzar
 35       1 byte     Reservado para uso futuro, siempre 0
 36       1 palabra  Palabra de estado codificada por bits. Formato:
                     Bit     Descripci¢n
                     ---     -----------
                     15-8    Reservados para uso futuro
                     7-6     Reservados para uso interno, siempre 0
                     5       Estado del Flash: 0 - tinta INK, papel PAPER
                                               1 - tinta PAPER, papel INK
                     4       Interrupci¢n pendiente de ejecutarse
                     3       Reservado para uso futuro
                     2       Biestable IFF2 (uso interno)
                     1       Modo de interrupci¢n: 0=IM1; 1=IM2
                     0       Biestable IFF1 (estado de interrupci¢n):
                                 0 - Interrupciones desactivadas (DI)
                                 1 - Interrupciones activadas (EI)
*/

bool FileSP::load(string sp_fn) {

    FILE *file;

    file = fopen(sp_fn.c_str(), "rb");
    if (file==NULL) {
        printf("FileSP: Error opening %s\n",sp_fn.c_str());
        return false;
    }

    uint8_t sign[2] = { 0, 0 };
    sign[0] = readByteFile(file);
    sign[1] = readByteFile(file);
    if ( sign[0] != 'S' && sign[1] != 'P' ) {
        printf("FileSP: invalid format %s\n",sp_fn.c_str());
        fclose(file);
        return false;
    }

    // data size
    uint16_t dataSize = readWordFileLE(file);
    // start address
    uint16_t startAddress = readWordFileLE(file);
    if ( ( dataSize != 0 && dataSize != 49152 ) || ( startAddress != 0 && startAddress != 16384 ) ) {
        printf("FileSP: invalid format %s\n",sp_fn.c_str());
        fclose(file);
        return false;
    }

    // Change arch if needed
    if ( !Z80Ops::is48) {

        bool vreset = Config::videomode;

        Config::requestMachine("48K", "");

        // Condition this to 50hz mode
        if(vreset) {
            Config::SNA_Path = FileUtils::SNA_Path;
            Config::SNA_begin_row = FileUtils::fileTypes[DISK_SNAFILE].begin_row;
            Config::SNA_focus = FileUtils::fileTypes[DISK_SNAFILE].focus;
            Config::SNA_fdMode = FileUtils::fileTypes[DISK_SNAFILE].fdMode;
            Config::SNA_fileSearch = FileUtils::fileTypes[DISK_SNAFILE].fileSearch;

            Config::TAP_Path = FileUtils::TAP_Path;
            Config::TAP_begin_row = FileUtils::fileTypes[DISK_TAPFILE].begin_row;
            Config::TAP_focus = FileUtils::fileTypes[DISK_TAPFILE].focus;
            Config::TAP_fdMode = FileUtils::fileTypes[DISK_TAPFILE].fdMode;
            Config::TAP_fileSearch = FileUtils::fileTypes[DISK_TAPFILE].fileSearch;

            Config::DSK_Path = FileUtils::DSK_Path;
            Config::DSK_begin_row = FileUtils::fileTypes[DISK_DSKFILE].begin_row;
            Config::DSK_focus = FileUtils::fileTypes[DISK_DSKFILE].focus;
            Config::DSK_fdMode = FileUtils::fileTypes[DISK_DSKFILE].fdMode;
            Config::DSK_fileSearch = FileUtils::fileTypes[DISK_DSKFILE].fileSearch;

            Config::ram_file = sp_fn;
            Config::save();
            OSD::esp_hard_reset();
        }

    }

    ESPectrum::reset();

    // Read in the registers
    Z80::setRegBC(readWordFileLE(file));
    Z80::setRegDE(readWordFileLE(file));
    Z80::setRegHL(readWordFileLE(file));

    Z80::setFlags(readByteFile(file));
    Z80::setRegA(readByteFile(file));

    Z80::setRegIX(readWordFileLE(file));
    Z80::setRegIY(readWordFileLE(file));

    Z80::setRegBCx(readWordFileLE(file));
    Z80::setRegDEx(readWordFileLE(file));
    Z80::setRegHLx(readWordFileLE(file));

    Z80::setRegFx(readByteFile(file));
    Z80::setRegAx(readByteFile(file));

    Z80::setRegR(readByteFile(file));

    Z80::setRegI(readByteFile(file));

    Z80::setRegSP(readWordFileLE(file));
    Z80::setRegPC(readWordFileLE(file));

    readWordFileLE(file);

    VIDEO::borderColor = readByteFile(file);

    readByteFile(file);

    // flags
    uint16_t flags = readWordFileLE(file);
    Z80::setIFF2(flags & 0x04 ? true : false);
    Z80::setIM(flags & 0x02 ? Z80::IM2 : Z80::IM1);
    Z80::setIFF1(flags & 0x01 ? true : false);

    VIDEO::brd = VIDEO::border32[VIDEO::borderColor];

    MemESP::romLatch = 0;
    MemESP::romInUse = 0;
    MemESP::bankLatch = 0;
    MemESP::pagingLock = 1;
    MemESP::videoLatch = 0;

    // read ROM page if present
    if (!startAddress && !dataSize) {
        readBlockFile(file, MemESP::ram[1], 0x4000);
        MemESP::ramCurrent[0] = MemESP::ram[1];
        MemESP::SPRom = true;
    }

    // read 48K memory
    readBlockFile(file, MemESP::ram[5], 0x4000);
    readBlockFile(file, MemESP::ram[2], 0x4000);
    readBlockFile(file, MemESP::ram[0], 0x4000);

    fclose(file);

    return true;

}
