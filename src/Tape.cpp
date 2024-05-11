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

#include <stdio.h>
#include <vector>
#include <string>
#include <inttypes.h>

using namespace std;

#include "Tape.h"
#include "hardpins.h"
#include "FileUtils.h"
#include "CPU.h"
#include "Ports.h"
#include "Video.h"
#include "OSDMain.h"
#include "Config.h"
#include "Snapshot.h"
#include "messages.h"
#include "Z80_JLS/z80.h"

#define TAPE_LISTING_DIV 16

FILE *Tape::tape;
string Tape::tapeFileName = "none";
string Tape::tapeSaveName = "none";
int Tape::tapeFileType = TAPE_FTYPE_EMPTY;
uint8_t Tape::tapeStatus = TAPE_STOPPED;
uint8_t Tape::SaveStatus = SAVE_STOPPED;
uint8_t Tape::romLoading = false;
uint8_t Tape::tapeEarBit;
std::vector<TapeBlock> Tape::TapeListing;
int Tape::tapeCurBlock;
int Tape::tapeNumBlocks;
uint32_t Tape::tapebufByteCount;
uint32_t Tape::tapePlayOffset;
size_t Tape::tapeFileSize;

// Tape timing values
uint16_t Tape::tapeSyncLen;
uint16_t Tape::tapeSync1Len;
uint16_t Tape::tapeSync2Len;
uint16_t Tape::tapeBit0PulseLen; // lenght of pulse for bit 0
uint16_t Tape::tapeBit1PulseLen; // lenght of pulse for bit 1
uint16_t Tape::tapeHdrLong;  // Header sync lenght in pulses
uint16_t Tape::tapeHdrShort; // Data sync lenght in pulses
uint32_t Tape::tapeBlkPauseLen; 
uint32_t Tape::tapeNext;
uint8_t Tape::tapeLastByteUsedBits = 8;
uint8_t Tape::tapeEndBitMask;

uint8_t Tape::tapePhase = TAPE_PHASE_STOPPED;

static uint8_t tapeCurByte;
static uint8_t tapePhase;
static uint64_t tapeStart;
// static uint32_t tapePulseCount;
// static uint16_t tapeBitPulseLen;   
// static uint8_t tapeBitPulseCount;     
static uint16_t tapeHdrPulses;
static uint32_t tapeBlockLen;
static uint8_t tapeBitMask;

static uint16_t nLoops;
static uint16_t loopStart;
static uint32_t loop_tapeBlockLen;
static uint32_t loop_tapebufByteCount;
static bool loop_first = false;

static uint16_t callSeq = 0;
static int callBlock;
short jumpDistance;

// void (*Tape::TZX_Read)() = &Tape::TZX_Read_0x10;
void (*Tape::GetBlock)() = &Tape::TAP_GetBlock;

// Load tape file (.tap, .tzx)
void Tape::LoadTape(string mFile) {

    if (FileUtils::hasTAPextension(mFile)) {

        string keySel = mFile.substr(0,1);
        mFile.erase(0, 1);

        // Flashload .tap if needed
        if ((keySel ==  "R") && (Config::flashload) && (Config::romSet != "ZX81+") && (Config::romSet != "48Kcs") && (Config::romSet != "128Kcs")) {

                OSD::osdCenteredMsg(OSD_TAPE_FLASHLOAD, LEVEL_INFO, 0);

                uint8_t OSDprev = VIDEO::OSD;

                if (Z80Ops::is48)
                    FileZ80::loader48();
                else
                    FileZ80::loader128();

                // Put something random on FRAMES SYS VAR as recommended by Mark Woodmass
                // https://skoolkid.github.io/rom/asm/5C78.html
                MemESP::writebyte(0x5C78,rand() % 256);
                MemESP::writebyte(0x5C79,rand() % 256);            

                if (Config::ram_file != NO_RAM_FILE) {
                    Config::ram_file = NO_RAM_FILE;
                }
                Config::last_ram_file = NO_RAM_FILE;

                if (OSDprev) {
                    VIDEO::OSD = OSDprev;
                    if (Config::aspect_16_9)
                        VIDEO::Draw_OSD169 = VIDEO::MainScreen_OSD;
                    else
                        VIDEO::Draw_OSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;
                    ESPectrum::TapeNameScroller = 0;
                }    

        }

        Stop();

        // Read and analyze tap file
        Tape::TAP_Open(mFile);

        ESPectrum::TapeNameScroller = 0;

    } else if (FileUtils::hasTZXextension(mFile)) {

        string keySel = mFile.substr(0,1);
        mFile.erase(0, 1);

        Stop();

        // Read and analyze tzx file
        Tape::TZX_Open(mFile);

        ESPectrum::TapeNameScroller = 0;

        // printf("%s loaded.\n",mFile.c_str());

    }

}

void Tape::Init() {

    tape = NULL;
    tapeFileType = TAPE_FTYPE_EMPTY;

}

void Tape::TAP_Open(string name) {
   
    if (tape != NULL) {
        fclose(tape);
        tape = NULL;
        tapeFileType = TAPE_FTYPE_EMPTY;
    }

    string fname = FileUtils::MountPoint + "/" + FileUtils::TAP_Path + "/" + name;

    tape = fopen(fname.c_str(), "rb");
    if (tape == NULL) {
        OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
        return;
    }

    fseek(tape,0,SEEK_END);
    tapeFileSize = ftell(tape);
    rewind(tape);
    if (tapeFileSize == 0) return;
    
    tapeFileName = name;

    Tape::TapeListing.clear(); // Clear TapeListing vector
    std::vector<TapeBlock>().swap(TapeListing); // free memory

    int tapeListIndex=0;
    int tapeContentIndex=0;
    int tapeBlkLen=0;
    TapeBlock block;
    do {

        // Analyze .tap file
        tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));

        // printf("Analyzing block %d\n",tapeListIndex);
        // printf("    Block Len: %d\n",tapeBlockLen - 2);        

        // Read the flag byte from the block.
        // If the last block is a fragmented data block, there is no flag byte, so set the flag to 255
        // to indicate a data block.
        uint8_t flagByte;
        if (tapeContentIndex + 2 < tapeFileSize) {
            flagByte = readByteFile(tape);
        } else {
            flagByte = 255;
        }

        // Process the block depending on if it is a header or a data block.
        // Block type 0 should be a header block, but it happens that headerless blocks also
        // have block type 0, so we need to check the block length as well.
        if (flagByte == 0 && tapeBlkLen == 19) { // This is a header.

            // Get the block type.
            TapeBlock::BlockType dataBlockType;
            uint8_t blocktype = readByteFile(tape);
            switch (blocktype)
            {
                case 0:
                    dataBlockType = TapeBlock::BlockType::Program_header;
                    break;
                case 1:
                    dataBlockType = TapeBlock::BlockType::Number_array_header;
                    break;
                case 2:
                    dataBlockType = TapeBlock::BlockType::Character_array_header;
                    break;
                case 3:
                    dataBlockType = TapeBlock::BlockType::Code_header;
                    break;
                default:
                    dataBlockType = TapeBlock::BlockType::Unassigned;
                    break;
            }

            // Get the filename.
            for (int i = 0; i < 10; i++) {
                uint8_t tst = readByteFile(tape);
            }

            fseek(tape,6,SEEK_CUR);

            // Get the checksum.
            uint8_t checksum = readByteFile(tape);
        
            if ((tapeListIndex & (TAPE_LISTING_DIV - 1)) == 0) {
                block.StartPosition = tapeContentIndex;
                TapeListing.push_back(block);
            }

        } else {

            // Get the block content length.
            int contentLength;
            int contentOffset;
            if (tapeBlkLen >= 2) {
                // Normally the content length equals the block length minus two
                // (the flag byte and the checksum are not included in the content).
                contentLength = tapeBlkLen - 2;
                // The content is found at an offset of 3 (two byte block size + one flag byte).
                contentOffset = 3;
            } else {
                // Fragmented data doesn't have a flag byte or a checksum.
                contentLength = tapeBlkLen;
                // The content is found at an offset of 2 (two byte block size).
                contentOffset = 2;
            }

            fseek(tape,contentLength,SEEK_CUR);

            // Get the checksum.
            uint8_t checksum = readByteFile(tape);

            if ((tapeListIndex & (TAPE_LISTING_DIV - 1)) == 0) {
                block.StartPosition = tapeContentIndex;
                TapeListing.push_back(block);
            }

        }

        tapeListIndex++;
        
        tapeContentIndex += tapeBlkLen + 2;

    } while(tapeContentIndex < tapeFileSize);

    tapeCurBlock = 0;
    tapeNumBlocks = tapeListIndex;

    // printf("------------------------------------\n");        
    // for (int i = 0; i < tapeListIndex; i++) {
    //     printf("Block Index: %d\n",TapeListing[i].Index);

    //     string blktype;
    //     switch (TapeListing[i].Type) {
    //     case 0: 
    //         blktype = "Program header";
    //         break;
    //     case 1: 
    //         blktype = "Number array header";
    //         break;
    //     case 2: 
    //         blktype = "Character array header";
    //         break;
    //     case 3: 
    //         blktype = "Code header";
    //         break;
    //     case 4: 
    //         blktype = "Data block";
    //         break;
    //     case 5: 
    //         blktype = "Info";
    //         break;
    //     case 6: 
    //         blktype = "Unassigned";
    //         break;
    //     }
    //     printf("Block Type : %s\n",blktype.c_str());

    //     printf("Filename   : %s\n",(char *)TapeListing[i].FileName);
    //     printf("Lenght     : %d\n",TapeListing[i].BlockLength);        
    //     printf("Start Pos. : %d\n",TapeListing[i].StartPosition);                
    //     printf("------------------------------------\n");        

    // }

    rewind(tape);

    tapeFileType = TAPE_FTYPE_TAP;

    // Set tape timing values
    if (Config::tape_timing_rg) {

        tapeSyncLen = TAPE_SYNC_LEN_RG;
        tapeSync1Len = TAPE_SYNC1_LEN_RG;
        tapeSync2Len = TAPE_SYNC2_LEN_RG;
        tapeBit0PulseLen = TAPE_BIT0_PULSELEN_RG;
        tapeBit1PulseLen = TAPE_BIT1_PULSELEN_RG;
        tapeHdrLong = TAPE_HDR_LONG_RG;
        tapeHdrShort = TAPE_HDR_SHORT_RG;
        tapeBlkPauseLen = TAPE_BLK_PAUSELEN_RG; 

    } else {

        tapeSyncLen = TAPE_SYNC_LEN;
        tapeSync1Len = TAPE_SYNC1_LEN;
        tapeSync2Len = TAPE_SYNC2_LEN;
        tapeBit0PulseLen = TAPE_BIT0_PULSELEN;
        tapeBit1PulseLen = TAPE_BIT1_PULSELEN;
        tapeHdrLong = TAPE_HDR_LONG;
        tapeHdrShort = TAPE_HDR_SHORT;
        tapeBlkPauseLen = TAPE_BLK_PAUSELEN; 

    }

}

void Tape::TZX_Open(string name) {

    if (tape != NULL) {
        fclose(tape);
        tape = NULL;
        tapeFileType = TAPE_FTYPE_EMPTY;
    }

    string fname = FileUtils::MountPoint + "/" + FileUtils::TAP_Path + "/" + name;

    tape = fopen(fname.c_str(), "rb");
    if (tape == NULL) {
        OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
        tapeFileType = TAPE_FTYPE_EMPTY;
        return;
    }

    fseek(tape,0,SEEK_END);
    tapeFileSize = ftell(tape);
    rewind(tape);
    if (tapeFileSize == 0) {
        OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
        fclose(tape);
        tape = NULL;
        tapeFileType = TAPE_FTYPE_EMPTY;
        return;
    }
    
    // Check TZX header signature
    char tzxheader[8];
    fread(&tzxheader, 8, 1, tape);    
    if (strcmp(tzxheader,"ZXTape!\x1a") != 0) {
        OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
        fclose(tape);
        tape = NULL;
        tapeFileType = TAPE_FTYPE_EMPTY;
        return;
    }

    tapeFileName = name;

    fseek(tape, 2, SEEK_CUR); // Jump TZX version bytes
    // printf("TZX version: %d.%d\n",(int)readByteFile(tape),(int)readByteFile(tape));

    Tape::TapeListing.clear(); // Clear TapeListing vector
    std::vector<TapeBlock>().swap(TapeListing); // free memory

    int tapeListIndex=0;
    int tapeContentIndex=10;
    uint32_t tapeBlkLen=0;

    // printf("--- PARSING TZX FILE --------------------------------------\n");

    TapeBlock block;

    do {

        uint8_t tzx_blk_type = readByteFile(tape);

        switch (tzx_blk_type) {

            case 0x10: // Standard Speed Data
            
                // printf("TZX block: ID 0x10 - Standard Speed Data Block\n");

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // // printf("    Pause lenght: %d\n",tapeBlkLen);
                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // // printf("    Data lenght: %d\n",tapeBlkLen);
                // fseek(tape,tapeBlkLen,SEEK_CUR);

                fseek(tape,2,SEEK_CUR); // Jump pause len data
                tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8);
                fseek(tape,tapeBlkLen,SEEK_CUR);
                tapeBlkLen += 0x4; // Add pause len and block len bytes

                break;

            case 0x11: // Turbo Speed Data

                // printf("TZX block: ID 0x11 - Turbo Speed Data Block\n");

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pilot pulse lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Sync first pulse lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Sync second pulse lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Zero bit pulse lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    One bit pulse lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pilot tone lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=readByteFile(tape);
                // printf("    Used bits in last byte: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pause lenght: %d\n",tapeBlkLen);


                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
                // printf("    Data lenght: %d\n",tapeBlkLen);

                // printf("    Flag byte: %d\n",(int)readByteFile(tape));
                // fseek(tape,tapeBlkLen - 2,SEEK_CUR); 
                // printf("    Checksum byte: %d\n",(int)readByteFile(tape));

                fseek(tape,0xf,SEEK_CUR); // Jump block data
                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
                fseek(tape,tapeBlkLen,SEEK_CUR);

                tapeBlkLen +=  0x12; // Add block data bytes lenght

                break;

            case 0x12: // Pure Tone

                // printf("TZX block: ID 0x12 - Pure Tone Block\n");

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("Pulse lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("Number of pulses: %d\n",tapeBlkLen);

                fseek(tape,0x4,SEEK_CUR); // Jump block data
                tapeBlkLen = 0x4;

                break;

            case 0x13: // Pulse sequence

                // printf("TZX block: ID 0x14 - Pulse Sequence Block\n");
                
                // tapeBlkLen=readByteFile(tape);
                // printf("Number of pulses: %d\n",tapeBlkLen);
                // tapeBlkLen <<= 1;

                tapeBlkLen = readByteFile(tape) << 1;
                fseek(tape,tapeBlkLen,SEEK_CUR);
                tapeBlkLen += 0x01;

                break;

            case 0x14: // Pure Data

                // printf("TZX block: ID 0x14 - Pure Data Block\n");

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Zero bit pulse lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    One bit pulse lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=readByteFile(tape);
                // printf("    Used bits in last byte: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pause lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
                // printf("    Data lenght: %d\n",tapeBlkLen);

                // printf("    Flag byte: %d\n",(int)readByteFile(tape));
                // fseek(tape,tapeBlkLen - 2,SEEK_CUR); 
                // printf("    Checksum byte: %d\n",(int)readByteFile(tape));

                fseek(tape,0x7,SEEK_CUR); // Jump block data
                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
                fseek(tape,tapeBlkLen,SEEK_CUR);

                tapeBlkLen +=  0x0a;

                break;

            case 0x15: // Direct recording

                // printf("TZX block: ID 0x15 - Direct recording\n");

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    T-states per sample: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pause lenght: %d\n",tapeBlkLen);

                // tapeBlkLen=readByteFile(tape);
                // printf("    Used bits in last byte: %d\n",tapeBlkLen);

                // tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
                // printf("    Data lenght: %d\n",tapeBlkLen);

                fseek(tape,0x5,SEEK_CUR); // Jump block data
                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
                fseek(tape,tapeBlkLen,SEEK_CUR); 
                tapeBlkLen +=  0x08;

                break;

            case 0x20:

                // printf("TZX block: ID 0x20 - Pause (silence) or 'Stop the Tape' command\n");

                fseek(tape,0x2,SEEK_CUR);
                tapeBlkLen = 0x2;

                break;

            case 0x21:

                // printf("TZX block: ID 0x21 - Group Start\n");

                tapeBlkLen = readByteFile(tape);
                fseek(tape,tapeBlkLen,SEEK_CUR);
                tapeBlkLen += 0x1;

                break;

            case 0x22:

                // printf("TZX block: ID 0x22 - Group End\n");

                tapeBlkLen = 0;

                break;

            case 0x23:

                // printf("TZX block: ID 0x23 - Jump to block\n");

                fseek(tape,0x2,SEEK_CUR);
                tapeBlkLen = 0x2;

                break;

            case 0x24:

                // printf("TZX block: ID 0x24 - Loop start\n");

                fseek(tape,0x2,SEEK_CUR);
                tapeBlkLen = 0x2;

                break;

            case 0x25:

                // printf("TZX block: ID 0x25 - Loop End\n");

                tapeBlkLen = 0;

                break;

            case 0x26:

                // printf("TZX block: ID 0x26 - Call sequence\n");

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8)) << 1;
                fseek(tape,tapeBlkLen,SEEK_CUR); 

                tapeBlkLen +=  0x02;

                break;

            case 0x27:

                // printf("TZX block: ID 0x27 - Return from sequence\n");

                tapeBlkLen = 0;

                break;

            case 0x28:

                // printf("TZX block: ID 0x28 - Select block\n");

                tapeBlkLen= readByteFile(tape) | (readByteFile(tape) << 8);
                fseek(tape,tapeBlkLen,SEEK_CUR); 

                tapeBlkLen +=  0x02;

                break;

            case 0x2A:

                // printf("TZX block: ID 0x2A - Stop the tape if in 48K mode\n");

                fseek(tape,0x4,SEEK_CUR);
                tapeBlkLen = 0x4;

                break;

            case 0x2B:

                // printf("TZX block: ID 0x2B - Set signal level\n");

                fseek(tape,0x5,SEEK_CUR);
                tapeBlkLen = 0x5;

                break;

            case 0x30:

                // printf("TZX block: ID 0x30 - Text Description\n");

                tapeBlkLen = readByteFile(tape);
                fseek(tape,tapeBlkLen,SEEK_CUR);
                tapeBlkLen += 0x1;

                break;

            case 0x31:

                // printf("TZX block: ID 0x31 - Message block\n");

                fseek(tape,1,SEEK_CUR);
                tapeBlkLen = readByteFile(tape);
                fseek(tape,tapeBlkLen,SEEK_CUR);
                tapeBlkLen += 0x2;

                break;

            case 0x32:

                // printf("TZX block: ID 0x32 - Archive info\n");

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));                
                fseek(tape,tapeBlkLen,SEEK_CUR);
                tapeBlkLen += 0x2;

                break;

            case 0x33:

                // printf("TZX block: ID 0x33 - Hardware type\n");

                tapeBlkLen=readByteFile(tape) * 0x3;
                fseek(tape,tapeBlkLen,SEEK_CUR);
                tapeBlkLen += 0x1;

                break;

            case 0x35:

                // printf("TZX block: ID 0x35 - Custom info\n");

                fseek(tape,0x10,SEEK_CUR);
                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24));                
                fseek(tape,tapeBlkLen,SEEK_CUR);

                tapeBlkLen += 0x14;

                break;

            case 0x5a:

                // printf("TZX block: ID 0x5a - Glue Block\n");

                fseek(tape,0x9,SEEK_CUR);
                tapeBlkLen = 0x9;

                break;

            default:

                // printf("Unsupported block type found. Type %d\n",tzx_blk_type);

                Tape::TapeListing.clear(); // Clear TapeListing vector
                std::vector<TapeBlock>().swap(TapeListing); // free memory

                OSD::osdCenteredMsg("Block type not supported." /*OSD_TAPE_LOAD_ERR*/, LEVEL_ERROR);
                fclose(tape);
                tape = NULL;
                tapeFileName="none";
                tapeFileType = TAPE_FTYPE_EMPTY;
                return;

        }

        // Right now, we'll store all block start positions on .tzx files because a CalcTZXBlockPos function could be complex (TO DO: Check TZX with highest number of blocks)
        // if ((tapeListIndex & (TAPE_LISTING_DIV - 1)) == 0) {
            block.StartPosition = tapeContentIndex;
            TapeListing.push_back(block);
        // }

        tapeListIndex++;
        tapeContentIndex += tapeBlkLen + 1; // Add 1 for the block type byte

        // printf("Block: %d, TapeContentIndex: %d, TapeFileSize: %d\n", tapeListIndex, tapeContentIndex, tapeFileSize);
        // printf("-------------------------------------------------------\n");    

    } while(tapeContentIndex < tapeFileSize);

    tapeCurBlock = 0;
    tapeNumBlocks = tapeListIndex;

    // tapeBitMask=0b10000000;
    // tapeBitPulseCount=0;

    // printf("Number of blocks: %d\n",tapeNumBlocks);

    tapeFileType = TAPE_FTYPE_TZX;

    rewind(tape);

}

uint32_t Tape::CalcTapBlockPos(int block) {

    int TapeBlockRest = block & (TAPE_LISTING_DIV -1);
    int CurrentPos = TapeListing[block / TAPE_LISTING_DIV].StartPosition;
    // printf("TapeBlockRest: %d\n",TapeBlockRest);
    // printf("Tapecurblock: %d\n",Tape::tapeCurBlock);

    fseek(tape,CurrentPos,SEEK_SET);

    while (TapeBlockRest-- != 0) {
        uint16_t tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
        // printf("Tapeblklen: %d\n",tapeBlkLen);
        fseek(tape,tapeBlkLen,SEEK_CUR);
        CurrentPos += tapeBlkLen + 2;
    }

    return CurrentPos;

}

string Tape::tapeBlockReadData(int Blocknum) {

    int tapeContentIndex=0;
    int tapeBlkLen=0;
    string blktype;
    char buf[48];
    char fname[10];

    tapeContentIndex = Tape::CalcTapBlockPos(Blocknum);

    // Analyze .tap file
    tapeBlkLen=(readByteFile(Tape::tape) | (readByteFile(Tape::tape) << 8));

    // Read the flag byte from the block.
    // If the last block is a fragmented data block, there is no flag byte, so set the flag to 255
    // to indicate a data block.
    uint8_t flagByte;
    if (tapeContentIndex + 2 < Tape::tapeFileSize) {
        flagByte = readByteFile(Tape::tape);
    } else {
        flagByte = 255;
    }

    // Process the block depending on if it is a header or a data block.
    // Block type 0 should be a header block, but it happens that headerless blocks also
    // have block type 0, so we need to check the block length as well.
    if (flagByte == 0 && tapeBlkLen == 19) { // This is a header.

        // Get the block type.
        uint8_t blocktype = readByteFile(Tape::tape);

        switch (blocktype) {
        case 0: 
            blktype = "Program      ";
            break;
        case 1: 
            blktype = "Number array ";
            break;
        case 2: 
            blktype = "Char array   ";
            break;
        case 3: 
            blktype = "Code         ";
            break;
        case 4: 
            blktype = "Data block   ";
            break;
        case 5: 
            blktype = "Info         ";
            break;
        case 6: 
            blktype = "Unassigned   ";
            break;
        default:
            blktype = "Unassigned   ";
            break;
        }

        // Get the filename.
        if (blocktype > 5) {
            fname[0] = '\0';
        } else {
            for (int i = 0; i < 10; i++) {
                fname[i] = readByteFile(Tape::tape);
            }
            fname[10]='\0';
        }

    } else {

        blktype = "Data block   ";
        fname[0]='\0';

    }

    snprintf(buf, sizeof(buf), "%04d %s %10s % 6d\n", Blocknum + 1, blktype.c_str(), fname, tapeBlkLen);

    return buf;

}

void Tape::TAP_Play() {
   
    switch (Tape::tapeStatus) {
    case TAPE_STOPPED:

        if (tape == NULL) {
            OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
            return;
        }

        if (VIDEO::OSD) VIDEO::OSD = 1;

        // Init tape block vars
        tapePlayOffset = CalcTapBlockPos(tapeCurBlock);
        tapeBlockLen = 0;
        tapebufByteCount = 0;

        // Init tape vars
        tapeEarBit = 1;
        tapeBitMask=0x80;
        tapeLastByteUsedBits = 8;
        tapeEndBitMask=0x80;

        // Get block data
        tapeCurByte = readByteFile(tape);
        GetBlock = &TAP_GetBlock;
        GetBlock();

        // Start loading
        Tape::tapeStatus=TAPE_LOADING;
        tapeStart=CPU::global_tstates + CPU::tstates;

        break;

    case TAPE_LOADING:
        Stop();
        break;

    }
}


void Tape::TAP_GetBlock() {

    // Check end of tape
    if (tapeCurBlock >= tapeNumBlocks) {
        tapeCurBlock = 0;
        Stop();
        rewind(tape);
        return;
    }

    // Get block len and first byte of block
    tapeBlockLen += (tapeCurByte | (readByteFile(tape) <<8)) + 2;
    tapeCurByte = readByteFile(tape);
    tapebufByteCount += 2;

    // Set sync phase values
    tapePhase=TAPE_PHASE_SYNC;
    tapeNext=tapeSyncLen;
    if (tapeCurByte) tapeHdrPulses=tapeHdrShort; else tapeHdrPulses=tapeHdrLong;

}

void Tape::TZX_Play() {
   
    switch (Tape::tapeStatus) {
    case TAPE_STOPPED:

        if (tape == NULL) {
            OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
            return;
        }

        if (VIDEO::OSD) VIDEO::OSD = 1;

        // Prepare current block to play
        tapePlayOffset = TapeListing[tapeCurBlock].StartPosition;        
        tapeBlockLen = 10;
        tapebufByteCount = 10;

        // Init tape vars
        tapeEarBit = 1;

        GetBlock = &TZX_GetBlock;
        GetBlock();

        Tape::tapeStatus=TAPE_LOADING;
        tapeStart=CPU::global_tstates + CPU::tstates;

        break;

    case TAPE_LOADING:
        Stop();
        break;

    }
}

// void Tape::TZX_Play() {
   
//     switch (Tape::tapeStatus) {
//     case TAPE_STOPPED:

//         if (tape == NULL) {
//             OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
//             return;
//         }

//         // Prepare current block to play
//         tapePlayOffset = TapeListing[tapeCurBlock].StartPosition;        
//         tapeBlockLen = 10;
//         tapebufByteCount = 10;

//         tapeEarBit = 1; // Should this be set only on tape start ?

//         GetBlock = &TZX_GetBlock;
//         GetBlock();

//         Tape::tapeStatus=TAPE_LOADING;
//         if (VIDEO::OSD) VIDEO::OSD = 1;

//         break;

//     case TAPE_LOADING:
//         TAP_Stop();
//         break;

//     }
// }

void Tape::TZX_GetBlock() {

    int tapeBlkLen;

    // printf("TapeCurBlock: %d, TapeNumBlocks: %d\n",tapeCurBlock,tapeNumBlocks);

    for (;;) {

        if (tapeCurBlock >= tapeNumBlocks) {
            tapeCurBlock = 0;
            Stop();
            rewind(tape);
            return;
        }

        // Move to current block position
        fseek(tape,TapeListing[tapeCurBlock].StartPosition,SEEK_SET);
        // tapePlayOffset = TapeListing[tapeCurBlock].StartPosition;

        uint8_t tzx_blk_type = readByteFile(tape);

        switch (tzx_blk_type) {

            case 0x10: // Standard Speed Data

                // printf("0x10 Tape position: %d\n",tapebufByteCount);                

                // Set tape timing values
                if (Config::tape_timing_rg) {

                    tapeSyncLen = TAPE_SYNC_LEN_RG;
                    tapeSync1Len = TAPE_SYNC1_LEN_RG;
                    tapeSync2Len = TAPE_SYNC2_LEN_RG;
                    tapeBit0PulseLen = TAPE_BIT0_PULSELEN_RG;
                    tapeBit1PulseLen = TAPE_BIT1_PULSELEN_RG;
                    tapeHdrLong = TAPE_HDR_LONG_RG;
                    tapeHdrShort = TAPE_HDR_SHORT_RG;

                } else {

                    tapeSyncLen = TAPE_SYNC_LEN;
                    tapeSync1Len = TAPE_SYNC1_LEN;
                    tapeSync2Len = TAPE_SYNC2_LEN;
                    tapeBit0PulseLen = TAPE_BIT0_PULSELEN;
                    tapeBit1PulseLen = TAPE_BIT1_PULSELEN;
                    tapeHdrLong = TAPE_HDR_LONG;
                    tapeHdrShort = TAPE_HDR_SHORT;

                }

                tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500; // Use tzx data!

                tapeBlockLen += (readByteFile(tape) | (readByteFile(tape) << 8)) + 5;

                tapeBitMask=0x80;
                tapeLastByteUsedBits = 8;   
                tapeEndBitMask = 0x80;             
                
                tapeCurByte = readByteFile(tape);

                tapebufByteCount += 5;

                tapePhase = TAPE_PHASE_SYNC;
                tapeNext = tapeSyncLen;
                if (tapeCurByte & 0x80) tapeHdrPulses=tapeHdrShort; else tapeHdrPulses=tapeHdrLong;                

                return;

            case 0x11: // Turbo Speed Data

                // printf("Tape position: %d\n",tapebufByteCount);                            

                // printf("TZX block: %d, ID 0x11 - Turbo Speed Data Block\n",tapeCurBlock);

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pilot pulse lenght: %d\n",tapeBlkLen);
                tapeSyncLen = tapeBlkLen;

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Sync first pulse lenght: %d\n",tapeBlkLen);
                tapeSync1Len = tapeBlkLen;

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Sync second pulse lenght: %d\n",tapeBlkLen);
                tapeSync2Len = tapeBlkLen;

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Zero bit pulse lenght: %d\n",tapeBlkLen);
                tapeBit0PulseLen = tapeBlkLen;

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    One bit pulse lenght: %d\n",tapeBlkLen);
                tapeBit1PulseLen = tapeBlkLen;

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pilot tone lenght: %d\n",tapeBlkLen);
                tapeHdrLong = tapeHdrShort = tapeBlkLen;

                tapeLastByteUsedBits = readByteFile(tape);
                // printf("    Used bits in last byte: %d\n",tapeLastByteUsedBits);

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pause lenght: %d\n",tapeBlkLen);
                tapeBlkPauseLen = tapeBlkLen * 3500;                

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
                // printf("    Data lenght: %d\n",tapeBlkLen);

                // printf("    Flag byte: %d\n",(int)readByteFile(tape));
                // fseek(tape,tapeBlkLen - 2,SEEK_CUR); 
                // printf("    Checksum byte: %d\n",(int)readByteFile(tape));
                // tapeBlkLen +=  0x12;

                // tapePulseCount=0;

                tapeBitMask=0x80;
                
                // tapeBitPulseCount=0;

                tapeBlockLen += tapeBlkLen + 0x12 + 1;
                tapeCurByte = readByteFile(tape);
                if (tapeCurByte & 0x80) tapeHdrPulses=tapeHdrShort; else tapeHdrPulses=tapeHdrLong;
                tapebufByteCount += 0x12 + 1;

                tapeEndBitMask = 0x80;
                if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

                // if (tapeCurByte & tapeBitMask) tapeBitPulseLen=tapeBit1PulseLen; else tapeBitPulseLen=tapeBit0PulseLen;            

                tapePhase=TAPE_PHASE_SYNC;
                tapeNext = tapeSyncLen;

                return;

            case 0x12: // Pure Tone

                // Tape::TAP_Stop();
                // return;
                
                // printf("Pure tone - Tape position: %d\n",tapebufByteCount);                

                // printf("TZX block: ID 0x12 - Pure Tone Block\n");

                tapeSyncLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                tapeHdrPulses=(readByteFile(tape) | (readByteFile(tape) << 8));                
                
                // tapePulseCount = 0;

                // printf("Sync Len: %d, Number of pulses: %d\n",tapeSyncLen,tapeHdrPulses);

                tapeBlockLen += 0x4 + 1;
                tapebufByteCount += 0x4 + 1;

                // tapeStart = CPU::global_tstates + CPU::tstates;

                // printf("Tapestart: " "%lu" "\n",(unsigned long)tapeStart);

                tapePhase = TAPE_PHASE_PURETONE;
                tapeNext = tapeSyncLen;

                // TZX_Read = &TZX_Read_0x10;

                // TZX_Read = &Read;                                

                return;

            case 0x13: // Pulse sequence

                // printf("TZX block: ID 0x13 - Pulse Sequence Block\n");

                tapeHdrPulses=readByteFile(tape);                

                // tapePulseCount = 0;

                // printf("Number of pulses: %d\n",tapeHdrPulses);

                tapeSyncLen=readByteFile(tape) | (readByteFile(tape) << 8);

                tapeBlockLen += (tapeHdrPulses << 1) + 1 + 1;
                tapebufByteCount += 0x3 + 1;

                // tapeStart=CPU::global_tstates + CPU::tstates;

                tapePhase = TAPE_PHASE_PULSESEQ;
                tapeNext = tapeSyncLen;                
                
                // TZX_Read = &TZX_Read_0x10;

                // TZX_Read = &Read;                

                return;

            case 0x14: // Pure Data

                // printf("TZX block: %d, ID 0x14 - Pure Data Block\n",tapeCurBlock);

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Zero bit pulse lenght: %d\n",tapeBlkLen);
                tapeBit0PulseLen = tapeBlkLen;

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    One bit pulse lenght: %d\n",tapeBlkLen);
                tapeBit1PulseLen = tapeBlkLen;

                tapeLastByteUsedBits=readByteFile(tape);
                // printf("    Used bits in last byte: %d\n",tapeLastByteUsedBits);

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pause lenght: %d\n",tapeBlkLen);
                tapeBlkPauseLen = tapeBlkLen * 3500;                

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
                // printf("    Data lenght: %d\n",tapeBlkLen);

                // printf("    Flag byte: %d\n",(int)readByteFile(tape));
                // fseek(tape,tapeBlkLen - 2,SEEK_CUR); 
                // printf("    Checksum byte: %d\n",(int)readByteFile(tape));
                // tapeBlkLen +=  0x12;

                // tapePulseCount=0;
                tapeBitMask=0x80;
                // tapeBitPulseCount=0;
                tapeBlockLen += tapeBlkLen + 0x0a + 1;
                tapeCurByte = readByteFile(tape);

                // printf("Tape cur byte 1: %d\n",(int)tapeCurByte);

                tapebufByteCount += 0x0a + 1;

                tapeEndBitMask = 0x80;
                if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

                // if (tapeCurByte & tapeBitMask) tapeBitPulseLen=tapeBit1PulseLen; else tapeBitPulseLen=tapeBit0PulseLen;            

                // tapeStart=CPU::global_tstates + CPU::tstates;

                // TZX_Read = &TZX_Read_0x10;

                tapePhase=TAPE_PHASE_DATA1;
                tapeNext = tapeCurByte & tapeBitMask ? tapeBit1PulseLen : tapeBit0PulseLen;

                return;

            case 0x15: // Direct recording

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                tapeSyncLen = tapeBlkLen;

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                tapeBlkPauseLen = tapeBlkLen * 3500;                

                tapeLastByteUsedBits=readByteFile(tape);

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));

                tapePhase=TAPE_PHASE_DRB;
                tapeNext = tapeSyncLen;

                // tapePulseCount=0;
                tapeBitMask=0x80;
                // tapeBitPulseCount=0;

                tapeBlockLen += tapeBlkLen + 0x08 + 1;
                tapeCurByte = readByteFile(tape);

                tapebufByteCount += 0x08 + 1;

                tapeEndBitMask = 0x80;
                if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

                tapeEarBit = tapeCurByte & tapeBitMask ? 1 : 0;

                // if (tapeCurByte & tapeBitMask)
                //     tapeEarBit = 1; 
                // else 
                //     tapeEarBit = 0;

                // tapeStart=CPU::global_tstates + CPU::tstates;

                // TZX_Read = &TZX_Read_0x10;

                return;

            case 0x20:

                // printf("TZX block: ID 0x20 - Pause (silence) or 'Stop the Tape' command\n");

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
                // printf("    Pause lenght: %d\n",tapeBlkLen);
                tapeBlkPauseLen = tapeBlkLen * 3500;                

                // TO DO: if pause lenght is 0 -> STOP THE TAPE
                if (tapeBlkPauseLen == 0) {

                    Stop();

                    if (tapeCurBlock < (tapeNumBlocks - 1)) {
                        tapeCurBlock++;
                    } else {
                        tapeCurBlock = 0;
                        tapeEarBit = 0;
                        rewind(tape);
                    }

                } else {

                    tapePhase=TAPE_PHASE_PAUSE;
                    tapeNext=tapeBlkPauseLen;
                    
                    tapeCurBlock++;

                    tapeEarBit = 0;

                    tapeBlockLen += 2 + 1;
                    
                    tapebufByteCount += 2 + 1;

                    // tapeStart=CPU::global_tstates + CPU::tstates;

                    // TZX_Read = &TZX_Read_0x10;

                }

                return;

            case 0x21:

                //  printf("Group start - Tape position: %d\n",tapebufByteCount);                

                // printf("TZX block: ID 0x21 - Group Start\n");

                tapeBlkLen = readByteFile(tape) + 1;

                tapeBlockLen += tapeBlkLen + 1;
                tapebufByteCount += tapeBlkLen + 1;

                break;

            case 0x22:

                // printf("Group end - Tape position: %d\n",tapebufByteCount);                

                // printf("TZX block: ID 0x22 - Group End\n");

                tapeBlockLen++;
                tapebufByteCount++;

                break;

            case 0x23:

                // printf("TZX block: ID 0x23 - Jump to block\n");

                jumpDistance = readByteFile(tape) | (readByteFile(tape) << 8);
 
                tapeCurBlock += jumpDistance;

                if (tapeCurBlock >= (tapeNumBlocks - 1)) {
                    tapeCurBlock = 0;
                    Stop();
                    rewind(tape);
                    return;
                }

                if (tapeCurBlock < 0) tapeCurBlock = 0;

                tapeBlockLen += 2 + 1;
                tapebufByteCount += 2 + 1;

                continue;

            case 0x24:

                // printf("TZX block: ID 0x24 - Loop Start\n");

                nLoops=readByteFile(tape) | (readByteFile(tape) << 8);                
                loopStart = tapeCurBlock;
                loop_first = true;

                // printf("Loop start. NLoops: %d\n", (int)nLoops);

                tapeBlockLen += 2 + 1;
                tapebufByteCount += 2 + 1;

                break;

            case 0x25:
                
                // printf("TZX block: ID 0x25 - Loop End\n");

                if (loop_first) {
                    tapeBlockLen++;
                    tapebufByteCount++;
                    loop_tapeBlockLen = tapeBlockLen;
                    loop_tapebufByteCount = tapebufByteCount;
                    loop_first = false;
                }

                nLoops--;

                // printf("Loop end. NLoops: %d\n", (int)nLoops);

                if (nLoops > 0) {
                    tapeCurBlock = loopStart;
                } else {
                    tapeBlockLen = loop_tapeBlockLen;
                    tapebufByteCount = loop_tapebufByteCount;
                }


                break;

            case 0x26: // Call Sequence

                if (callSeq == 0) {
                    callSeq = readByteFile(tape) | (readByteFile(tape) << 8);
                    tapeBlockLen += (callSeq << 1) + 1;
                    tapebufByteCount += (callSeq << 1) + 1;
                } else {
                    fseek(tape, (callSeq << 1) + 2 ,SEEK_CUR);
                }

                jumpDistance = readByteFile(tape) | (readByteFile(tape) << 8);

                // printf("CallSeq: %d Jump to: %d\n", (int)callSeq,(int)jumpDistance);

                callBlock = tapeCurBlock;

                tapeCurBlock += jumpDistance;

                callSeq--;

                if (tapeCurBlock >= (tapeNumBlocks - 1)) {
                    tapeCurBlock = 0;
                    Stop();
                    rewind(tape);
                    return;
                }

                if (tapeCurBlock < 0) tapeCurBlock = 0;

                continue;

            case 0x27: // Return from Sequence

                tapeCurBlock = callBlock;
                
                if (callSeq == 0) tapeCurBlock++;

                // printf("Call return to: %d\n", (int)tapeCurBlock);

                tapeBlockLen++;
                tapebufByteCount++;
                
                continue;

            case 0x28: // Select Block

                // NOTE: Selection dialog not implemented. Now, this just jumps to next block.
                
                tapeBlkLen = (readByteFile(tape) | (readByteFile(tape) << 8)) + 2;
                
                // fseek(tape,1,SEEK_CUR);

                // jumpDistance = readByteFile(tape) | (readByteFile(tape) << 8);
 
                // tapeCurBlock += jumpDistance;

                // if (tapeCurBlock >= (tapeNumBlocks - 1)) {
                //     tapeCurBlock = 0;
                //     TAP_Stop();
                //     rewind(tape);
                //     return;
                // }

                // if (tapeCurBlock < 0) tapeCurBlock = 0;

                tapeBlockLen += tapeBlkLen + 1;
                tapebufByteCount += tapeBlkLen + 1;

                break;

            case 0x2A:

                // printf("TZX block: ID 0x2A - Stop the tape if in 48K mode\n");

                if (Z80Ops::is48) {

                    Stop();

                    if (tapeCurBlock < (tapeNumBlocks - 1)) {
                        tapeCurBlock++;
                    } else {
                        tapeCurBlock = 0;
                        rewind(tape);
                    }

                    return;

                }

                tapeBlockLen += 4 + 1;
                tapebufByteCount += 4 + 1;

                break;

            case 0x2B:

                fseek(tape,4,SEEK_CUR);

                tapeEarBit = readByteFile(tape);

                tapeBlockLen += 5 + 1;
                tapebufByteCount += 5 + 1;

                break;

            case 0x30:

                // printf("TZX block: ID 0x30 - Text Description\n");

                tapeBlkLen = readByteFile(tape) + 1;

                tapeBlockLen += tapeBlkLen + 1;
                tapebufByteCount += tapeBlkLen + 1;

                break;

            case 0x31:

                // printf("TZX block: ID 0x31 - Message block\n");
                // printf("Tape position: %d\n",tapebufByteCount);                

                fseek(tape,1,SEEK_CUR);

                tapeBlkLen = readByteFile(tape) + 2;

                tapeBlockLen += tapeBlkLen + 1;
                tapebufByteCount += tapeBlkLen + 1;

                break;

            case 0x32:            

                // printf("TZX block: ID 0x32 - Archive info\n");

                // printf("Tape position: %d\n",tapebufByteCount);                

                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8)) + 2;                

                tapeBlockLen += tapeBlkLen + 1;
                tapebufByteCount += tapeBlkLen + 1;

                break;

            case 0x33:

                tapeBlkLen = readByteFile(tape) * 3;

                tapeBlockLen += tapeBlkLen + 1;
                tapebufByteCount += tapeBlkLen + 1;

                break;

            case 0x35:

                fseek(tape,0x10,SEEK_CUR);
                tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24)) + 0x14;                

                tapeBlockLen += tapeBlkLen + 1;
                tapebufByteCount += tapeBlkLen + 1;

                break;

            case 0x5a:

                tapeBlockLen += 9 + 1;
                tapebufByteCount += 9 + 1;

                break;

        }

        tapeCurBlock++;

    }

}

// void Tape::TZX_GetBlock() {

//     int tapeBlkLen;

//     // printf("TapeCurBlock: %d, TapeNumBlocks: %d\n",tapeCurBlock,tapeNumBlocks);

//     for (;;) {

//         if (tapeCurBlock >= tapeNumBlocks) {
//             tapeCurBlock = 0;
//             TAP_Stop();
//             rewind(tape);
//             return;
//         }

//         // Move to current block position
//         fseek(tape,TapeListing[tapeCurBlock].StartPosition,SEEK_SET);
//         // tapePlayOffset = TapeListing[tapeCurBlock].StartPosition;

//         uint8_t tzx_blk_type = readByteFile(tape);

//         switch (tzx_blk_type) {

//             case 0x10: // Standard Speed Data

//                 // printf("0x10 Tape position: %d\n",tapebufByteCount);                

//                 // Set tape timing values
//                 if (Config::tape_timing_rg) {

//                     tapeSyncLen = TAPE_SYNC_LEN_RG;
//                     tapeSync1Len = TAPE_SYNC1_LEN_RG;
//                     tapeSync2Len = TAPE_SYNC2_LEN_RG;
//                     tapeBit0PulseLen = TAPE_BIT0_PULSELEN_RG;
//                     tapeBit1PulseLen = TAPE_BIT1_PULSELEN_RG;
//                     tapeHdrLong = TAPE_HDR_LONG_RG;
//                     tapeHdrShort = TAPE_HDR_SHORT_RG;

//                 } else {

//                     tapeSyncLen = TAPE_SYNC_LEN;
//                     tapeSync1Len = TAPE_SYNC1_LEN;
//                     tapeSync2Len = TAPE_SYNC2_LEN;
//                     tapeBit0PulseLen = TAPE_BIT0_PULSELEN;
//                     tapeBit1PulseLen = TAPE_BIT1_PULSELEN;
//                     tapeHdrLong = TAPE_HDR_LONG;
//                     tapeHdrShort = TAPE_HDR_SHORT;

//                 }

//                 tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500; // Use tzx data!

//                 // printf("Pause Len: %d\n",tapeBlkPauseLen);

//                 tapeBlockLen += (readByteFile(tape) | (readByteFile(tape) << 8)) + 5;

//                 tapePhase=TAPE_PHASE_SYNC;
//                 tapePulseCount=0;
//                 tapeBitMask=0b10000000;
//                 tapeLastByteUsedBits = 8;   
//                 tapeEndBitMask = 0x80;             
                
//                 tapeBitPulseCount=0;

//                 tapeCurByte = readByteFile(tape);

//                 if (tapeCurByte & 0x80) tapeHdrPulses=tapeHdrShort; else tapeHdrPulses=tapeHdrLong;

//                 tapebufByteCount += 5;

//                 if (tapeCurByte & tapeBitMask) tapeBitPulseLen=tapeBit1PulseLen; else tapeBitPulseLen=tapeBit0PulseLen;            

//                 tapeStart=CPU::global_tstates + CPU::tstates;

//                 TZX_Read = &TZX_Read_0x10;

//                 return;

//             case 0x11: // Turbo Speed Data

//                 // printf("Tape position: %d\n",tapebufByteCount);                            

//                 // printf("TZX block: %d, ID 0x11 - Turbo Speed Data Block\n",tapeCurBlock);

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Pilot pulse lenght: %d\n",tapeBlkLen);
//                 tapeSyncLen = tapeBlkLen;

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Sync first pulse lenght: %d\n",tapeBlkLen);
//                 tapeSync1Len = tapeBlkLen;

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Sync second pulse lenght: %d\n",tapeBlkLen);
//                 tapeSync2Len = tapeBlkLen;

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Zero bit pulse lenght: %d\n",tapeBlkLen);
//                 tapeBit0PulseLen = tapeBlkLen;

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    One bit pulse lenght: %d\n",tapeBlkLen);
//                 tapeBit1PulseLen = tapeBlkLen;

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Pilot tone lenght: %d\n",tapeBlkLen);
//                 tapeHdrLong = tapeHdrShort = tapeBlkLen;

//                 tapeLastByteUsedBits = readByteFile(tape);
//                 // printf("    Used bits in last byte: %d\n",tapeLastByteUsedBits);

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Pause lenght: %d\n",tapeBlkLen);
//                 tapeBlkPauseLen = tapeBlkLen * 3500;                

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
//                 // printf("    Data lenght: %d\n",tapeBlkLen);

//                 // printf("    Flag byte: %d\n",(int)readByteFile(tape));
//                 // fseek(tape,tapeBlkLen - 2,SEEK_CUR); 
//                 // printf("    Checksum byte: %d\n",(int)readByteFile(tape));
//                 // tapeBlkLen +=  0x12;

//                 tapePhase=TAPE_PHASE_SYNC;
//                 tapePulseCount=0;

//                 tapeBitMask=0b10000000;
//                 tapeBitPulseCount=0;

//                 tapeBlockLen += tapeBlkLen + 0x12 + 1;
//                 tapeCurByte = readByteFile(tape);
//                 if (tapeCurByte & 0x80) tapeHdrPulses=tapeHdrShort; else tapeHdrPulses=tapeHdrLong;
//                 tapebufByteCount += 0x12 + 1;

//                 tapeEndBitMask = 0x80;
//                 if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

//                 if (tapeCurByte & tapeBitMask) tapeBitPulseLen=tapeBit1PulseLen; else tapeBitPulseLen=tapeBit0PulseLen;            

//                 tapeStart=CPU::global_tstates + CPU::tstates;

//                 // tapeStart += tapeSyncLen;                                

//                 TZX_Read = &TZX_Read_0x10;

//                 return;

//             case 0x12: // Pure Tone

//                 // Tape::TAP_Stop();
//                 // return;
                
//                 // printf("Pure tone - Tape position: %d\n",tapebufByteCount);                

//                 // printf("TZX block: ID 0x12 - Pure Tone Block\n");

//                 tapeSyncLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 tapeHdrPulses=(readByteFile(tape) | (readByteFile(tape) << 8));                
                
//                 tapePulseCount = 0;

//                 // printf("Sync Len: %d, Number of pulses: %d\n",tapeSyncLen,tapeHdrPulses);

//                 tapeBlockLen += 0x4 + 1;
//                 tapebufByteCount += 0x4 + 1;

//                 tapeStart = CPU::global_tstates + CPU::tstates;

//                 // printf("Tapestart: " "%lu" "\n",(unsigned long)tapeStart);

//                 // tapePhase = TAPE_PHASE_SYNC;

//                 TZX_Read = &TZX_Read_0x12;

//                 return;

//             case 0x13: // Pulse sequence

//                 // printf("TZX block: ID 0x13 - Pulse Sequence Block\n");

//                 tapeHdrPulses=readByteFile(tape);                

//                 tapePulseCount = 0;

//                 // printf("Number of pulses: %d\n",tapeHdrPulses);

//                 tapeSyncLen=readByteFile(tape) | (readByteFile(tape) << 8);

//                 tapeBlockLen += (tapeHdrPulses << 1) + 1 + 1;
//                 tapebufByteCount += 0x3 + 1;

//                 tapeStart=CPU::global_tstates + CPU::tstates;

//                 TZX_Read = &TZX_Read_0x13;

//                 return;

//             case 0x14: // Pure Data

//                 // printf("TZX block: %d, ID 0x14 - Pure Data Block\n",tapeCurBlock);

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Zero bit pulse lenght: %d\n",tapeBlkLen);
//                 tapeBit0PulseLen = tapeBlkLen;

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    One bit pulse lenght: %d\n",tapeBlkLen);
//                 tapeBit1PulseLen = tapeBlkLen;

//                 tapeLastByteUsedBits=readByteFile(tape);
//                 // printf("    Used bits in last byte: %d\n",tapeLastByteUsedBits);

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Pause lenght: %d\n",tapeBlkLen);
//                 tapeBlkPauseLen = tapeBlkLen * 3500;                

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));
//                 // printf("    Data lenght: %d\n",tapeBlkLen);

//                 // printf("    Flag byte: %d\n",(int)readByteFile(tape));
//                 // fseek(tape,tapeBlkLen - 2,SEEK_CUR); 
//                 // printf("    Checksum byte: %d\n",(int)readByteFile(tape));
//                 // tapeBlkLen +=  0x12;

//                 tapePulseCount=0;
//                 tapeBitMask=0b10000000;
//                 tapeBitPulseCount=0;
//                 tapeBlockLen += tapeBlkLen + 0x0a + 1;
//                 tapeCurByte = readByteFile(tape);

//                 // printf("Tape cur byte 1: %d\n",(int)tapeCurByte);

//                 tapePhase=TAPE_PHASE_DATA;

//                 tapebufByteCount += 0x0a + 1;

//                 tapeEndBitMask = 0x80;
//                 if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

//                 if (tapeCurByte & tapeBitMask) tapeBitPulseLen=tapeBit1PulseLen; else tapeBitPulseLen=tapeBit0PulseLen;            

//                 tapeStart=CPU::global_tstates + CPU::tstates;

//                 TZX_Read = &TZX_Read_0x10;

//                 return;

//             case 0x15: // Direct recording

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 tapeSyncLen = tapeBlkLen;

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 tapeBlkPauseLen = tapeBlkLen * 3500;                

//                 tapeLastByteUsedBits=readByteFile(tape);

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16));

//                 tapePhase=TAPE_PHASE_DATA;

//                 tapePulseCount=0;
//                 tapeBitMask=0b10000000;
//                 tapeBitPulseCount=0;

//                 tapeBlockLen += tapeBlkLen + 0x08 + 1;
//                 tapeCurByte = readByteFile(tape);

//                 tapebufByteCount += 0x08 + 1;

//                 tapeEndBitMask = 0x80;
//                 if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

//                 tapeEarBit = tapeCurByte & tapeBitMask ? 1 : 0;

//                 // if (tapeCurByte & tapeBitMask)
//                 //     tapeEarBit = 1; 
//                 // else 
//                 //     tapeEarBit = 0;

//                 tapeStart=CPU::global_tstates + CPU::tstates;

//                 TZX_Read = &TZX_Read_0x15;

//                 return;

//             case 0x20:

//                 // printf("TZX block: ID 0x20 - Pause (silence) or 'Stop the Tape' command\n");

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//                 // printf("    Pause lenght: %d\n",tapeBlkLen);
//                 tapeBlkPauseLen = tapeBlkLen * 3500;                

//                 // TO DO: if pause lenght is 0 -> STOP THE TAPE
//                 if (tapeBlkPauseLen == 0) {

//                     Tape::TAP_Stop();

//                     if (tapeCurBlock < (tapeNumBlocks - 1)) {
//                         tapeCurBlock++;
//                     } else {
//                         tapeCurBlock = 0;
//                         tapeEarBit = 0;
//                         rewind(tape);
//                     }

//                 } else {

//                     tapePhase=TAPE_PHASE_PAUSE;
                    
//                     tapeCurBlock++;

//                     tapeEarBit = 0;

//                     tapeBlockLen += 2 + 1;
                    
//                     tapebufByteCount += 2 + 1;

//                     tapeStart=CPU::global_tstates + CPU::tstates;

//                     TZX_Read = &TZX_Read_0x10;

//                 }

//                 return;

//             case 0x21:

//                 //  printf("Group start - Tape position: %d\n",tapebufByteCount);                

//                 // printf("TZX block: ID 0x21 - Group Start\n");

//                 tapeBlkLen = readByteFile(tape) + 1;

//                 tapeBlockLen += tapeBlkLen + 1;
//                 tapebufByteCount += tapeBlkLen + 1;

//                 break;

//             case 0x22:

//                 // printf("Group end - Tape position: %d\n",tapebufByteCount);                

//                 // printf("TZX block: ID 0x22 - Group End\n");

//                 tapeBlockLen++;
//                 tapebufByteCount++;

//                 break;

//             case 0x23:

//                 // printf("TZX block: ID 0x23 - Jump to block\n");

//                 jumpDistance = readByteFile(tape) | (readByteFile(tape) << 8);
 
//                 tapeCurBlock += jumpDistance;

//                 if (tapeCurBlock >= (tapeNumBlocks - 1)) {
//                     tapeCurBlock = 0;
//                     TAP_Stop();
//                     rewind(tape);
//                     return;
//                 }

//                 if (tapeCurBlock < 0) tapeCurBlock = 0;

//                 tapeBlockLen += 2 + 1;
//                 tapebufByteCount += 2 + 1;

//                 continue;

//             case 0x24:

//                 // printf("TZX block: ID 0x24 - Loop Start\n");

//                 nLoops=readByteFile(tape) | (readByteFile(tape) << 8);                
//                 loopStart = tapeCurBlock;
//                 loop_first = true;

//                 // printf("Loop start. NLoops: %d\n", (int)nLoops);

//                 tapeBlockLen += 2 + 1;
//                 tapebufByteCount += 2 + 1;

//                 break;

//             case 0x25:
                
//                 // printf("TZX block: ID 0x25 - Loop End\n");

//                 if (loop_first) {
//                     tapeBlockLen++;
//                     tapebufByteCount++;
//                     loop_tapeBlockLen = tapeBlockLen;
//                     loop_tapebufByteCount = tapebufByteCount;
//                     loop_first = false;
//                 }

//                 nLoops--;

//                 // printf("Loop end. NLoops: %d\n", (int)nLoops);

//                 if (nLoops > 0) {
//                     tapeCurBlock = loopStart;
//                 } else {
//                     tapeBlockLen = loop_tapeBlockLen;
//                     tapebufByteCount = loop_tapebufByteCount;
//                 }


//                 break;

//             case 0x26: // Call Sequence

//                 if (callSeq == 0) {
//                     callSeq = readByteFile(tape) | (readByteFile(tape) << 8);
//                     tapeBlockLen += (callSeq << 1) + 1;
//                     tapebufByteCount += (callSeq << 1) + 1;
//                 } else {
//                     fseek(tape, (callSeq << 1) + 2 ,SEEK_CUR);
//                 }

//                 jumpDistance = readByteFile(tape) | (readByteFile(tape) << 8);

//                 // printf("CallSeq: %d Jump to: %d\n", (int)callSeq,(int)jumpDistance);

//                 callBlock = tapeCurBlock;

//                 tapeCurBlock += jumpDistance;

//                 callSeq--;

//                 if (tapeCurBlock >= (tapeNumBlocks - 1)) {
//                     tapeCurBlock = 0;
//                     TAP_Stop();
//                     rewind(tape);
//                     return;
//                 }

//                 if (tapeCurBlock < 0) tapeCurBlock = 0;

//                 continue;

//             case 0x27: // Return from Sequence

//                 tapeCurBlock = callBlock;
                
//                 if (callSeq == 0) tapeCurBlock++;

//                 // printf("Call return to: %d\n", (int)tapeCurBlock);

//                 tapeBlockLen++;
//                 tapebufByteCount++;
                
//                 continue;

//             case 0x28: // Select Block

//                 // NOTE: Selection dialog not implemented. Now, this just jumps to next block.
                
//                 tapeBlkLen = (readByteFile(tape) | (readByteFile(tape) << 8)) + 2;
                
//                 // fseek(tape,1,SEEK_CUR);

//                 // jumpDistance = readByteFile(tape) | (readByteFile(tape) << 8);
 
//                 // tapeCurBlock += jumpDistance;

//                 // if (tapeCurBlock >= (tapeNumBlocks - 1)) {
//                 //     tapeCurBlock = 0;
//                 //     TAP_Stop();
//                 //     rewind(tape);
//                 //     return;
//                 // }

//                 // if (tapeCurBlock < 0) tapeCurBlock = 0;

//                 tapeBlockLen += tapeBlkLen + 1;
//                 tapebufByteCount += tapeBlkLen + 1;

//                 break;

//             case 0x2A:

//                 // printf("TZX block: ID 0x2A - Stop the tape if in 48K mode\n");

//                 if (Z80Ops::is48) {

//                     Tape::TAP_Stop();

//                     if (tapeCurBlock < (tapeNumBlocks - 1)) {
//                         tapeCurBlock++;
//                     } else {
//                         tapeCurBlock = 0;
//                         rewind(tape);
//                     }

//                     return;

//                 }

//                 tapeBlockLen += 4 + 1;
//                 tapebufByteCount += 4 + 1;

//                 break;

//             case 0x2B:

//                 fseek(tape,4,SEEK_CUR);

//                 tapeEarBit = readByteFile(tape);

//                 tapeBlockLen += 5 + 1;
//                 tapebufByteCount += 5 + 1;

//                 break;

//             case 0x30:

//                 // printf("TZX block: ID 0x30 - Text Description\n");

//                 tapeBlkLen = readByteFile(tape) + 1;

//                 tapeBlockLen += tapeBlkLen + 1;
//                 tapebufByteCount += tapeBlkLen + 1;

//                 break;

//             case 0x31:

//                 // printf("TZX block: ID 0x31 - Message block\n");
//                 // printf("Tape position: %d\n",tapebufByteCount);                

//                 fseek(tape,1,SEEK_CUR);

//                 tapeBlkLen = readByteFile(tape) + 2;

//                 tapeBlockLen += tapeBlkLen + 1;
//                 tapebufByteCount += tapeBlkLen + 1;

//                 break;

//             case 0x32:            

//                 // printf("TZX block: ID 0x32 - Archive info\n");

//                 // printf("Tape position: %d\n",tapebufByteCount);                

//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8)) + 2;                

//                 tapeBlockLen += tapeBlkLen + 1;
//                 tapebufByteCount += tapeBlkLen + 1;

//                 break;

//             case 0x33:

//                 tapeBlkLen = readByteFile(tape) * 3;

//                 tapeBlockLen += tapeBlkLen + 1;
//                 tapebufByteCount += tapeBlkLen + 1;

//                 break;

//             case 0x35:

//                 fseek(tape,0x10,SEEK_CUR);
//                 tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24)) + 0x14;                

//                 tapeBlockLen += tapeBlkLen + 1;
//                 tapebufByteCount += tapeBlkLen + 1;

//                 break;

//             case 0x5a:

//                 tapeBlockLen += 9 + 1;
//                 tapebufByteCount += 9 + 1;

//                 break;

//         }

//         tapeCurBlock++;

//     }

// }

void Tape::Stop() {

    tapeStatus=TAPE_STOPPED;
    tapePhase=TAPE_PHASE_STOPPED;
    if (VIDEO::OSD) VIDEO::OSD = 2;

}

IRAM_ATTR void Tape::Read() {
    uint64_t tapeCurrent = CPU::global_tstates + CPU::tstates - tapeStart;
    if (tapeCurrent >= tapeNext) {
        do {
            tapeCurrent -= tapeNext;
            switch (tapePhase) {
            case TAPE_PHASE_DRB:
                tapeBitMask = (tapeBitMask >> 1) | (tapeBitMask << 7);
                if (tapeBitMask == tapeEndBitMask) {
                    tapeCurByte = readByteFile(tape);
                    tapebufByteCount++;
                    if (tapebufByteCount == tapeBlockLen) {
                        tapeCurBlock++;
                        if (tapeBlkPauseLen == 0) {
                            GetBlock();
                        } else {
                            tapePhase=TAPE_PHASE_TAIL;
                            tapeNext = TAPE_PHASE_TAIL_LEN;
                        }
                        return;
                    } else if ((tapebufByteCount + 1) == tapeBlockLen) {
                        if (tapeLastByteUsedBits < 8 )
                            tapeEndBitMask >>= tapeLastByteUsedBits;
                        else
                            tapeEndBitMask = 0x80;                        
                    } else {
                        tapeEndBitMask = 0x80;
                    }
                }
                tapeEarBit = tapeCurByte & tapeBitMask ? 1 : 0;
                break;
            case TAPE_PHASE_SYNC:
                tapeEarBit ^= 1;
                if (--tapeHdrPulses == 0) {
                    tapePhase=TAPE_PHASE_SYNC1;
                    tapeNext=tapeSync1Len;
                }
                break;
            case TAPE_PHASE_SYNC1:
                tapeEarBit ^= 1;
                tapePhase=TAPE_PHASE_SYNC2;
                tapeNext=tapeSync2Len;
                break;
            case TAPE_PHASE_SYNC2:
                if (tapebufByteCount == tapeBlockLen) { // This is for blocks with data lenght == 0
                    if (tapeBlkPauseLen == 0) {
                        tapeCurBlock++;
                        GetBlock();
                    } else {
                        tapePhase=TAPE_PHASE_TAIL;
                        tapeNext=TAPE_PHASE_TAIL_LEN;                        
                    }
                    break;
                }
                tapeEarBit ^= 1;
                tapePhase=TAPE_PHASE_DATA1;
                tapeNext = tapeCurByte & tapeBitMask ? tapeBit1PulseLen : tapeBit0PulseLen;
                break;
            case TAPE_PHASE_DATA1:
                tapeEarBit ^= 1;
                tapePhase=TAPE_PHASE_DATA2;
                break;
            case TAPE_PHASE_DATA2:
                tapeEarBit ^= 1;
                tapeBitMask = tapeBitMask >>1 | tapeBitMask <<7;
                if (tapeBitMask == tapeEndBitMask) {
                    tapeCurByte = readByteFile(tape);                    
                    tapebufByteCount++;
                    if (tapebufByteCount == tapeBlockLen) {
                        if (tapeBlkPauseLen == 0) {
                            tapeCurBlock++;
                            GetBlock();
                        } else {
                            tapePhase=TAPE_PHASE_TAIL;
                            tapeNext=TAPE_PHASE_TAIL_LEN;
                        }
                        break;
                    } else if ((tapebufByteCount + 1) == tapeBlockLen) {
                        if (tapeLastByteUsedBits < 8 )
                            tapeEndBitMask >>= tapeLastByteUsedBits;
                        else
                            tapeEndBitMask = 0x80;                        
                    } else {
                        tapeEndBitMask = 0x80;
                    }
                }
                tapePhase=TAPE_PHASE_DATA1;
                tapeNext = tapeCurByte & tapeBitMask ? tapeBit1PulseLen : tapeBit0PulseLen;
                break;
            case TAPE_PHASE_PURETONE:
                tapeEarBit ^= 1;
                if (--tapeHdrPulses == 0) {
                // tapePulseCount++;
                // if (tapePulseCount == tapeHdrPulses) {
                    // tapePulseCount = 0;
                    tapeCurBlock++;
                    GetBlock();
                }
                break;
            case TAPE_PHASE_PULSESEQ:
                tapeEarBit ^= 1;
                // tapePulseCount++;
                // if (tapePulseCount == tapeHdrPulses) {
                if (--tapeHdrPulses == 0) {
                    tapeCurBlock++;
                    GetBlock();
                } else {
                    tapeNext=(readByteFile(tape) | (readByteFile(tape) << 8));
                    tapebufByteCount += 2;
                }
                break;
            case TAPE_PHASE_TAIL:
                tapeEarBit = 0;
                tapePhase=TAPE_PHASE_PAUSE;
                tapeNext=tapeBlkPauseLen;
                break;
            case TAPE_PHASE_PAUSE:
                tapeEarBit = 1;
                tapeCurBlock++;
                GetBlock();
            } 
        } while (tapeCurrent >= tapeNext);
        tapeStart = CPU::global_tstates + CPU::tstates - tapeCurrent;
    }
}

// IRAM_ATTR void Tape::TAP_Read() {

//     uint64_t tapeCurrent = (CPU::global_tstates + CPU::tstates) - tapeStart;

//     switch (tapePhase) {
//     case TAPE_PHASE_SYNC:
//         if (tapeCurrent >= tapeSyncLen) {
//             tapeStart=CPU::global_tstates + CPU::tstates;
//             tapeEarBit ^= 1;
//             tapePulseCount++;
//             if (tapePulseCount>tapeHdrPulses) {
//                 tapePulseCount=0;
//                 tapePhase=TAPE_PHASE_SYNC1;
//             }
//         }
//         break;
//     case TAPE_PHASE_SYNC1:
//         if (tapeCurrent >= tapeSync1Len) {
//             tapeStart=CPU::global_tstates + CPU::tstates;
//             tapeEarBit ^= 1;
//             tapePhase=TAPE_PHASE_SYNC2;
//         }
//         break;
//     case TAPE_PHASE_SYNC2:
//         if (tapeCurrent >= tapeSync2Len) {
//             tapeStart=CPU::global_tstates + CPU::tstates;
//             tapeEarBit ^= 1;
//             if (tapeCurByte & tapeBitMask) tapeBitPulseLen=tapeBit1PulseLen; else tapeBitPulseLen=tapeBit0PulseLen;            
//             tapePhase=TAPE_PHASE_DATA;
//         }
//         break;
//     case TAPE_PHASE_DATA:
//         if (tapeCurrent >= tapeBitPulseLen) {
//             tapeStart=CPU::global_tstates + CPU::tstates;

//             if (tapeCurrent > (Config::tape_timing_rg ? 675000 : 1750000)) {
//                 tapeBitPulseCount = 0;
//                 tapeBitMask = 0x80;
//                 for (;;) {
//                     tapeEarBit ^= 1;
//                     tapeCurByte = readByteFile(tape);
//                     tapebufByteCount++;
//                     if (tapebufByteCount == tapeBlockLen) {
//                         tapePhase=TAPE_PHASE_TAIL;
//                         tapeCurBlock++;
//                         return;
//                     }
//                 }
//             }

                // ALT VERSION TO PREVIOUS BLOCK
                // if (tapeCurrent > (Config::tape_timing_rg ? 675000 : 1750000)) {
                //     tapeBitMask = 0x80;
                //     fseek(tape,tapeBlockLen,SEEK_SET);
                //     tapebufByteCount = tapeBlockLen;
                //     tapeCurByte = readByteFile(tape);
                //     tapePhase=TAPE_PHASE_TAIL;
                //     tapeNext=TAPE_PHASE_TAIL_LEN;
                //     tapeCurBlock++;
                //     return;                
                // }

//             tapeEarBit ^= 1;
//             tapeBitPulseCount++;
//             if (tapeBitPulseCount==2) {
//                 tapeBitPulseCount=0;
//                 tapeBitMask = (tapeBitMask >>1 | tapeBitMask <<7);
//                 if (tapeBitMask==0x80) {
//                     tapeCurByte = readByteFile(tape);
//                     tapebufByteCount++;
//                     if (tapebufByteCount == tapeBlockLen) {
//                         tapePhase=TAPE_PHASE_TAIL;
//                         tapeCurBlock++;
//                         break;
//                     }
//                 }
//                 if (tapeCurByte & tapeBitMask) tapeBitPulseLen=tapeBit1PulseLen; else tapeBitPulseLen=tapeBit0PulseLen;
//             }
//         }
//         break;
//     case TAPE_PHASE_TAIL:
//         if (tapeCurrent >= TAPE_PHASE_TAIL_LEN) {
//             tapeStart=CPU::global_tstates + CPU::tstates;                
//             tapePhase=TAPE_PHASE_PAUSE;
//             tapeEarBit = 0;
//         }
//         break;
//     case TAPE_PHASE_PAUSE:
//         if (tapeCurrent >= tapeBlkPauseLen) {
//             if ((tapebufByteCount + tapePlayOffset) < tapeFileSize) {
//                 tapeStart=CPU::global_tstates + CPU::tstates;
//                 tapePulseCount=0;
//                 tapePhase=TAPE_PHASE_SYNC;
//                 tapeBlockLen += (tapeCurByte | readByteFile(tape) << 8) + 2;
//                 tapebufByteCount+=2;
//                 tapeCurByte=readByteFile(tape);
//                 if (tapeCurByte) tapeHdrPulses=tapeHdrShort; else tapeHdrPulses=tapeHdrLong;
//                 tapeEarBit=1;
//             } else {
//                 tapeCurBlock=0;
//                 TAP_Stop();
//             }
//         }
//     } 

// }

// IRAM_ATTR void Tape::TZX_Read_0x10() {
//     uint64_t tapeCurrent = (CPU::global_tstates + CPU::tstates) - tapeStart;
//     if (tapeCurrent >= tapeNext) {
//         do {
//             tapeCurrent -= tapeNext;
//             switch (tapePhase) {
//             case TAPE_PHASE_DRB:
//                 tapeBitMask = (tapeBitMask >> 1) | (tapeBitMask << 7);
//                 if (tapeBitMask == tapeEndBitMask) {
//                     tapeCurByte = readByteFile(tape);
//                     tapebufByteCount++;
//                     if (tapebufByteCount == tapeBlockLen) {
//                         tapeCurBlock++;
//                         if (tapeBlkPauseLen == 0) {
//                             GetBlock();
//                         } else {
//                             tapePhase=TAPE_PHASE_TAIL;
//                             tapeNext = TAPE_PHASE_TAIL_LEN;
//                         }
//                         return;
//                     } else if ((tapebufByteCount + 1) == tapeBlockLen) {
//                         if (tapeLastByteUsedBits < 8 )
//                             tapeEndBitMask >>= tapeLastByteUsedBits;
//                         else
//                             tapeEndBitMask = 0x80;                        
//                     } else {
//                         tapeEndBitMask = 0x80;
//                     }
//                 }
//                 tapeEarBit = tapeCurByte & tapeBitMask ? 1 : 0;
//                 break;
//             case TAPE_PHASE_SYNC:
//                 tapeEarBit ^= 1;
//                 // tapePulseCount++;
//                 // if (tapePulseCount == tapeHdrPulses) {
//                 //     tapePulseCount=0;
//                 if (--tapeHdrPulses == 0) {
//                     tapePhase=TAPE_PHASE_SYNC1;
//                     tapeNext = tapeSync1Len;
//                 }
//                 break;
//             case TAPE_PHASE_SYNC1:
//                 tapeEarBit ^= 1;
//                 tapePhase=TAPE_PHASE_SYNC2;
//                 tapeNext = tapeSync2Len;
//                 break;
//             case TAPE_PHASE_SYNC2:
//                 if (tapebufByteCount == tapeBlockLen) { // This is for blocks with data lenght == 0
//                     if (tapeBlkPauseLen == 0) {
//                         tapeCurBlock++;
//                         GetBlock();
//                     } else {
//                         tapePhase=TAPE_PHASE_TAIL;
//                         tapeNext = TAPE_PHASE_TAIL_LEN;                                            
//                     }
//                     break;
//                 }
//                 tapeEarBit ^= 1;
//                 tapePhase=TAPE_PHASE_DATA1;
//                 tapeNext = tapeCurByte & tapeBitMask ? tapeBit1PulseLen : tapeBit0PulseLen;                
//                 break;
//             case TAPE_PHASE_DATA1:
//                 tapeEarBit ^= 1;
//                 tapePhase=TAPE_PHASE_DATA2;
//                 break;
//             case TAPE_PHASE_DATA2:
//                 tapeEarBit ^= 1;
//                 tapeBitMask = (tapeBitMask >>1 | tapeBitMask <<7);
//                 if (tapeBitMask==tapeEndBitMask) {
//                     tapeCurByte = readByteFile(tape);
//                     tapebufByteCount++;
//                     if (tapebufByteCount == tapeBlockLen) {
//                         if (tapeBlkPauseLen == 0) {
//                             tapeCurBlock++;
//                             GetBlock();
//                         } else {
//                             tapePhase=TAPE_PHASE_TAIL;
//                             tapeNext=TAPE_PHASE_TAIL_LEN;                        
//                         }
//                         break;
//                     } else if ((tapebufByteCount + 1) == tapeBlockLen) {
//                         if (tapeLastByteUsedBits < 8 )
//                             tapeEndBitMask >>= tapeLastByteUsedBits;
//                         else
//                             tapeEndBitMask = 0x80;                        
//                     } else {
//                         tapeEndBitMask = 0x80;
//                     }
//                 }
//                 tapePhase=TAPE_PHASE_DATA1;
//                 tapeNext = tapeCurByte & tapeBitMask ? tapeBit1PulseLen : tapeBit0PulseLen;                
//                 break;
//             case TAPE_PHASE_PURETONE:
//                 tapeEarBit ^= 1;
//                 tapePulseCount++;
//                 if (tapePulseCount == tapeHdrPulses) {
//                     tapePulseCount = 0;
//                     tapeCurBlock++;
//                     GetBlock();
//                 }
//                 break;
//             case TAPE_PHASE_PULSESEQ:
//                 tapeEarBit ^= 1;
//                 tapePulseCount++;
//                 if (tapePulseCount == tapeHdrPulses) {
//                     tapeCurBlock++;
//                     GetBlock();
//                 } else {
//                     tapeNext=(readByteFile(tape) | (readByteFile(tape) << 8));
//                     tapebufByteCount += 2;
//                 }
//                 break;
//             case TAPE_PHASE_TAIL:
//                 tapeEarBit = 0;
//                 tapePhase=TAPE_PHASE_PAUSE;
//                 tapeNext=tapeBlkPauseLen;
//                 break;
//             case TAPE_PHASE_PAUSE:
//                 tapeEarBit = 1;
//                 tapeCurBlock++;
//                 GetBlock();
//             } 
//         } while (tapeCurrent >= tapeNext);
//         tapeStart = CPU::global_tstates + CPU::tstates - tapeCurrent;
//     }
// }


// IRAM_ATTR void Tape::TZX_Read_0x12() {

//     uint64_t tapeCurrent = (CPU::global_tstates + CPU::tstates) - tapeStart;
    
//     if (tapeCurrent >= tapeSyncLen) {

//         for (;;) {
//             tapeEarBit ^= 1;
//             tapePulseCount++;
//             tapeCurrent -= tapeSyncLen;
//             if ((tapeCurrent < tapeSyncLen) || tapePulseCount == tapeHdrPulses) break;
//         }

//         tapeStart=(CPU::global_tstates + CPU::tstates) - tapeCurrent;        

//         if (tapePulseCount == tapeHdrPulses) {
//             tapePulseCount = 0;
//             tapeCurBlock++;
//             GetBlock();
//         }
//     }

// }

// IRAM_ATTR void Tape::TZX_Read_0x13() {

//     uint64_t tapeCurrent = (CPU::global_tstates + CPU::tstates) - tapeStart;
    
//     if (tapeCurrent >= tapeSyncLen) {

//         for (;;) {
//             tapeEarBit ^= 1;
//             tapePulseCount++;
//             tapeCurrent -= tapeSyncLen;
//             if ((tapeCurrent < tapeSyncLen) || tapePulseCount == tapeHdrPulses) break;
//         }

//         tapeStart=(CPU::global_tstates + CPU::tstates) - tapeCurrent;        
        
//         if (tapePulseCount == tapeHdrPulses) {
//             tapeCurBlock++;
//             GetBlock();
//         } else {
//             tapeSyncLen=(readByteFile(tape) | (readByteFile(tape) << 8));
//             tapebufByteCount += 2;
//         }

//     }

// }

// IRAM_ATTR void Tape::TZX_Read_0x15() {

//     uint64_t tapeCurrent = (CPU::global_tstates + CPU::tstates) - tapeStart;
    
//     switch (tapePhase) {

//     case TAPE_PHASE_DATA1:

//         if (tapeCurrent >= tapeSyncLen) {

//             for (;tapeCurrent > tapeSyncLen; tapeCurrent -= tapeSyncLen) {

//                 tapeBitMask = (tapeBitMask >> 1) | (tapeBitMask << 7);

//                 if (tapeBitMask == tapeEndBitMask) {

//                     tapeCurByte = readByteFile(tape);
                    
//                     tapebufByteCount++;

//                     if (tapebufByteCount == tapeBlockLen) {

//                         tapeStart = CPU::global_tstates + CPU::tstates;

//                         tapeCurBlock++;

//                         if (tapeBlkPauseLen == 0) {
//                             GetBlock();
//                         } else {
//                             tapePhase=TAPE_PHASE_TAIL;
//                         }

//                         return;

//                     } else if ((tapebufByteCount + 1) == tapeBlockLen) {

//                         if (tapeLastByteUsedBits < 8 )

//                             tapeEndBitMask >>= tapeLastByteUsedBits;

//                         else

//                             tapeEndBitMask = 0x80;                        

//                     } else {

//                         tapeEndBitMask = 0x80;

//                     }

//                 }

//             }

//             tapeEarBit = tapeCurByte & tapeBitMask ? 1 : 0;

//             tapeStart=(CPU::global_tstates + CPU::tstates) - tapeCurrent;

//         }

//         break;

//     case TAPE_PHASE_TAIL:
//         if (tapeCurrent >= TAPE_PHASE_TAIL_LEN) {
//             tapeStart=CPU::global_tstates + CPU::tstates;                
//             tapePhase=TAPE_PHASE_PAUSE;
//             tapeEarBit = 0;
//         }
//         break;
//     case TAPE_PHASE_PAUSE:
//         if (tapeCurrent >= tapeBlkPauseLen) {
//             tapeStart=CPU::global_tstates + CPU::tstates;                
//             tapeEarBit = 1;
//             GetBlock();
//         }

//     }

// }

void Tape::Save() {

	FILE *fichero;
    unsigned char xxor,salir_s;
	uint8_t dato;
	int longitud;

    fichero = fopen(tapeSaveName.c_str(), "ab");
    if (fichero == NULL)
    {
        OSD::osdCenteredMsg(OSD_TAPE_SAVE_ERR, LEVEL_ERROR);
        return;
    }

	xxor=0;
	
	longitud=(int)(Z80::getRegDE());
	longitud+=2;
	
	dato=(uint8_t)(longitud%256);
	fprintf(fichero,"%c",dato);
	dato=(uint8_t)(longitud/256);
	fprintf(fichero,"%c",dato); // file length

	fprintf(fichero,"%c",Z80::getRegA()); // flag

	xxor^=Z80::getRegA();

	salir_s = 0;
	do {
	 	if (Z80::getRegDE() == 0)
	 		salir_s = 2;
	 	if (!salir_s) {
            dato = MemESP::readbyte(Z80::getRegIX());
			fprintf(fichero,"%c",dato);
	 		xxor^=dato;
	        Z80::setRegIX(Z80::getRegIX() + 1);
	        Z80::setRegDE(Z80::getRegDE() - 1);
	 	}
	} while (!salir_s);
	fprintf(fichero,"%c",xxor);
	Z80::setRegIX(Z80::getRegIX() + 2);

    fclose(fichero);

}

bool Tape::FlashLoad() {

    if (tape == NULL) {

        string fname = FileUtils::MountPoint + "/" + FileUtils::TAP_Path + "/" + tapeFileName;        

        tape = fopen(fname.c_str(), "rb");
        if (tape == NULL) {
            return false;
        }

        CalcTapBlockPos(tapeCurBlock);

    }

    // printf("--< BLOCK: %d >--------------------------------\n",(int)tapeCurBlock);    

    uint16_t blockLen=(readByteFile(tape) | (readByteFile(tape) <<8));
    uint8_t tapeFlag = readByteFile(tape);

    // printf("blockLen: %d\n",(int)blockLen - 2);
    // printf("tapeFlag: %d\n",(int)tapeFlag);
    // printf("AX: %d\n",(int)Z80::getRegAx());

    if (Z80::getRegAx() != tapeFlag) {
        // printf("No coincide el flag\n");
        Z80::setFlags(0x00);
        Z80::setRegA(Z80::getRegAx() ^ tapeFlag);
        if (tapeCurBlock < (tapeNumBlocks - 1)) {
            tapeCurBlock++;
            CalcTapBlockPos(tapeCurBlock);
            return true;
        } else {
            tapeCurBlock = 0;
            rewind(tape);
            return false;
        }
    }

    // La paridad incluye el byte de flag
    Z80::setRegA(tapeFlag);

    int count = 0;
    int addr = Z80::getRegIX();    // Address start
    int nBytes = Z80::getRegDE();  // Lenght
    int addr2 = addr & 0x3fff;
    uint8_t page = addr >> 14;

    // printf("nBytes: %d\n",nBytes);

    if ((addr2 + nBytes) <= 0x4000) {

        // printf("Case 1\n");

        if (page != 0 )
            fread(&MemESP::ramCurrent[page][addr2], nBytes, 1, tape);
        else {
            fseek(tape,nBytes, SEEK_CUR);
        }

        while ((count < nBytes) && (count < blockLen - 1)) {
            Z80::Xor(MemESP::readbyte(addr));        
            addr = (addr + 1) & 0xffff;
            count++;
        }

    } else {

        // printf("Case 2\n");

        int chunk1 = 0x4000 - addr2;
        int chunkrest = nBytes > (blockLen - 1) ? (blockLen - 1) : nBytes;

        do {

            if ((page > 0) && (page < 4)) {

                fread(&MemESP::ramCurrent[page][addr2], chunk1, 1, tape);

                for (int i=0; i < chunk1; i++) {
                    Z80::Xor(MemESP::readbyte(addr));
                    addr = (addr + 1) & 0xffff;
                    count++;
                }

            } else {

                for (int i=0; i < chunk1; i++) {
                    Z80::Xor(readByteFile(tape));
                    addr = (addr + 1) & 0xffff;
                    count++;
                }

            }

            addr2 = 0;
            chunkrest = chunkrest - chunk1;
            if (chunkrest > 0x4000) chunk1 = 0x4000; else chunk1 = chunkrest;
            page++;

        } while (chunkrest > 0);

    }

    if (nBytes > (blockLen - 2)) {
        // Hay menos bytes en la cinta de los indicados en DE
        // En ese caso habrá dado un error de timeout en LD-SAMPLE (0x05ED)
        // que se señaliza con CARRY==reset & ZERO==set
        Z80::setFlags(0x50);
    } else {
        Z80::Xor(readByteFile(tape)); // Byte de paridad
        Z80::Cp(0x01);
    }

    if (tapeCurBlock < (tapeNumBlocks - 1)) {        
        tapeCurBlock++;
        if (nBytes != (blockLen -2)) CalcTapBlockPos(tapeCurBlock);
    } else {
        tapeCurBlock = 0;
        rewind(tape);
    }

    Z80::setRegIX(addr);
    Z80::setRegDE(nBytes - (blockLen - 2));

    return true;

}
