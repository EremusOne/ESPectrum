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

#include <stdio.h>
#include <vector>
#include <string>
#include <inttypes.h>
#include "miniz/miniz.h"
// #include "rom/miniz.h"

using namespace std;

#include "Tape.h"
#include "FileUtils.h"
#include "cpuESP.h"
#include "Video.h"
#include "OSDMain.h"
#include "Config.h"
#include "Snapshot.h"
#include "messages.h"
#include "Z80_JLS/z80.h"

FILE *Tape::tape;
FILE *Tape::cswBlock;
string Tape::tapeFileName = "none";
string Tape::tapeFilePath = "none";
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
std::vector<int> Tape::selectedBlocks;

bool Tape::tapeIsReadOnly = false;

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
double  Tape::tapeCompensation = 1;

uint8_t Tape::tapePhase = TAPE_PHASE_STOPPED;

uint8_t Tape::tapeCurByte;
uint64_t Tape::tapeStart;
uint16_t Tape::tapeHdrPulses;
uint32_t Tape::tapeBlockLen;
uint8_t Tape::tapeBitMask;

uint16_t Tape::nLoops;
uint16_t Tape::loopStart;
uint32_t Tape::loop_tapeBlockLen;
uint32_t Tape::loop_tapebufByteCount;
bool Tape::loop_first = false;

uint16_t Tape::callSeq = 0;
int Tape::callBlock;

int Tape::CSW_SampleRate;
int Tape::CSW_PulseLenght;
uint8_t Tape::CSW_CompressionType;
uint32_t Tape::CSW_StoredPulses;

 // GDB vars
uint32_t Tape::totp;
uint8_t Tape::npp;
uint16_t Tape::asp;
uint32_t Tape::totd;
uint8_t Tape::npd;
uint16_t Tape::asd;
uint32_t Tape::curGDBSymbol;
uint8_t Tape::curGDBPulse;
uint8_t Tape::GDBsymbol;
uint8_t Tape::nb;
uint8_t Tape::curBit;
bool Tape::GDBEnd = false;
Symdef* Tape::SymDefTable;

#define my_max(a,b) (((a) > (b)) ? (a) : (b))
#define my_min(a,b) (((a) < (b)) ? (a) : (b))
#define BUF_SIZE 1024

int Tape::inflateCSW(int blocknumber, long startPos, long data_length) {

    char destFileName[16]; // Nombre del archivo descomprimido
    uint8_t s_inbuf[BUF_SIZE];
    uint8_t s_outbuf[BUF_SIZE];
    FILE *pOutfile;
    z_stream stream;

    // printf(MOUNT_POINT_SD "/.csw%04d.tmp\n",blocknumber);

    sprintf(destFileName,MOUNT_POINT_SD "/.csw%04d.tmp",blocknumber);

    // Move to input file compressed data position
    fseek(tape, startPos, SEEK_SET);

    // Open output file.
    pOutfile = fopen(destFileName, "wb");
    if (!pOutfile) {
        printf("Failed opening output file!\n");
        return EXIT_FAILURE;
    }

    // Init the z_stream
    memset(&stream, 0, sizeof(stream));
    stream.next_in = s_inbuf;
    stream.avail_in = 0;
    stream.next_out = s_outbuf;
    stream.avail_out = BUF_SIZE;

    // Decompression.
    uint infile_remaining = data_length;

    uint32_t *speccyram = (uint32_t *)MemESP::ram[1];

    memcpy(VIDEO::SaveRect,speccyram,0x8000);
    memset(MemESP::ram[1],0,0x8000);

    if (inflateInit(&stream, MemESP::ram[1])) {
        printf("inflateInit() failed!\n");
        return EXIT_FAILURE;
    }

    for ( ; ; ) {

        int status;
        if (!stream.avail_in) {

            // Input buffer is empty, so read more bytes from input file.
            uint n = my_min(BUF_SIZE, infile_remaining);

            if (fread(s_inbuf, 1, n, tape) != n) {
                printf("Failed reading from input file!\n");
                return EXIT_FAILURE;
            }

            stream.next_in = s_inbuf;
            stream.avail_in = n;

            infile_remaining -= n;

        }

        status = inflate(&stream, Z_SYNC_FLUSH);

        if ((status == Z_STREAM_END) || (!stream.avail_out)) {
            // Output buffer is full, or decompression is done, so write buffer to output file.
            uint n = BUF_SIZE - stream.avail_out;
            if (fwrite(s_outbuf, 1, n, pOutfile) != n) {
                printf("Failed writing to output file!\n");
                return EXIT_FAILURE;
            }
            stream.next_out = s_outbuf;
            stream.avail_out = BUF_SIZE;
        }

        if (status == Z_STREAM_END)
            break;
        else if (status != Z_OK) {
            printf("inflate() failed with status %i!\n", status);
            return EXIT_FAILURE;
        }

    }

    if (inflateEnd(&stream) != Z_OK) {
        printf("inflateEnd() failed!\n");
        return EXIT_FAILURE;
    }

    if (EOF == fclose(pOutfile)) {
        printf("Failed writing to output file!\n");
        return EXIT_FAILURE;
    }

    // printf("Total input bytes: %u\n", (mz_uint32)stream.total_in);
    // printf("Total output bytes: %u\n", (mz_uint32)stream.total_out);
    // printf("Success.\n");

    memcpy(speccyram,VIDEO::SaveRect,0x8000);

    return EXIT_SUCCESS;

}

void (*Tape::GetBlock)() = &Tape::TAP_GetBlock;

// Load tape file (.tap, .tzx)
void Tape::LoadTape(string mFile) {

    if (FileUtils::hasTAPextension(mFile)) {

        string keySel = mFile.substr(0,1);
        mFile.erase(0, 1);

        // if ( FileUtils::fileSize( ( FileUtils::MountPoint + FileUtils::TAP_Path + mFile ).c_str() ) > 0 ) {

            // Flashload .tap if needed
            if ((FileUtils::fileSize( ( FileUtils::MountPoint + FileUtils::TAP_Path + mFile ).c_str() ) > 0) && (keySel ==  "R")
                && (Config::flashload) && (Config::romSet != "ZX81+") && (Config::romSet != "48Kcs") && (Config::romSet != "128Kcs") && (Config::romSet != "TKcs")) {

                OSD::osdCenteredMsg(OSD_TAPE_FLASHLOAD[Config::lang], LEVEL_INFO, 0);

                uint8_t OSDprev = VIDEO::OSD;

                Tape::Stop();

                // Read and analyze tap file
                Tape::TAP_Open(mFile,FileUtils::MountPoint + FileUtils::TAP_Path);

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
                        VIDEO::Draw_OSD169 = Z80Ops::is2a3 ? VIDEO::MainScreen_OSD_2A3 : VIDEO::MainScreen_OSD;
                    else
                        VIDEO::Draw_OSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;

                    ESPectrum::TapeNameScroller = 0;

                }

            } else {

                OSD::osdCenteredMsg(OSD_TAPE_INSERT[Config::lang], LEVEL_INFO);

                Tape::Stop();

                // Read and analyze tap file
                Tape::TAP_Open(mFile,FileUtils::MountPoint + FileUtils::TAP_Path);

                ESPectrum::TapeNameScroller = 0;

            }

        // }

        // Tape::Stop();

        // // Read and analyze tap file
        // Tape::TAP_Open(mFile,FileUtils::MountPoint + FileUtils::TAP_Path);

        // ESPectrum::TapeNameScroller = 0;

    } else if (FileUtils::hasTZXextension(mFile)) {

        string keySel = mFile.substr(0,1);
        mFile.erase(0, 1);

        Tape::Stop();

        // Read and analyze tzx file
        Tape::TZX_Open(mFile,FileUtils::MountPoint + FileUtils::TAP_Path);

        ESPectrum::TapeNameScroller = 0;

        // printf("%s loaded.\n",mFile.c_str());

    }

}

void Tape::Init() {

    tape = NULL;
    tapeFileType = TAPE_FTYPE_EMPTY;

}

void Tape::Eject() {

    Tape::Stop();

    if (tape != NULL) {
        fclose(tape);
        tape = NULL;
    }

    tapeFileType = TAPE_FTYPE_EMPTY;

    tapeFileName = "none";
    tapeFilePath = "none";

}

void Tape::TAP_setBlockTimings() {

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

    tapeSyncLen *= tapeCompensation;
    tapeSync1Len *= tapeCompensation;
    tapeSync2Len *= tapeCompensation;
    tapeBit0PulseLen *= tapeCompensation;
    tapeBit1PulseLen *= tapeCompensation;
    tapeBlkPauseLen *= tapeCompensation;

}

void Tape::TAP_getBlockData() {

    fseek(tape,0,SEEK_END);
    tapeFileSize = ftell(tape);
    rewind(tape);

    Tape::TapeListing.clear(); // Clear TapeListing vector
    std::vector<TapeBlock>().swap(TapeListing); // free memory

    int tapeListIndex=0;
    int tapeContentIndex=0;
    int tapeBlkLen=0;
    TapeBlock block;
    if ( tapeFileSize > 0 ) {

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
    }

    tapeCurBlock = 0;
    tapeNumBlocks = tapeListIndex;

}

void Tape::TAP_Open(string name, string path) {

    Tape::Stop();

    if (tape != NULL) {
        fclose(tape);
        tape = NULL;
    }

    string fname = path + name;

    printf("Fname: %s\n", fname.c_str());

    tapeIsReadOnly = access(fname.c_str(), W_OK);
    tape = fopen(fname.c_str(), tapeIsReadOnly == 0 ? "rb+" : "rb");
    if (tape == NULL) {
        OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
        return;
    }

    tapeFilePath = path;
    tapeFileName = name;

    TAP_getBlockData();

    rewind(tape);

    tapeFileType = TAPE_FTYPE_TAP;

    TAP_setBlockTimings();

}

uint32_t Tape::CalcTapBlockPos(int block) {
    if ( TapeListing.empty()) return 0;

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
    char buf[52];
    char fname[11];

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

    snprintf(buf, sizeof(buf), "%04d %s  %10s           % 6d\n", Blocknum + 1, blktype.c_str(), fname, tapeBlkLen);

    return buf;

}

TapeBlock::BlockType Tape::getBlockType(int Blocknum) {

    int tapeContentIndex = 0;
    int tapeBlkLen = 0;

    tapeContentIndex = Tape::CalcTapBlockPos(Blocknum);

    // Analyze .tap file
    tapeBlkLen = (readByteFile(Tape::tape) | (readByteFile(Tape::tape) << 8));

    // Read the flag byte from the block.
    // If the last block is a fragmented data block, there is no flag byte, so set the flag to 255
    // to indicate a data block.
    uint8_t flagByte = 255;
    if (tapeContentIndex + 2 < Tape::tapeFileSize) {
        flagByte = readByteFile(Tape::tape);
    }

    // Process the block depending on if it is a header or a data block.
    // Block type 0 should be a header block, but it happens that headerless blocks also
    // have block type 0, so we need to check the block length as well.
    if (flagByte == 0 && tapeBlkLen == 19) { // This is a header.
        // Get the block type.
        uint8_t blocktype = readByteFile(Tape::tape);
        switch (blocktype) {
            case 0: return TapeBlock::Program_header;
            case 1: return TapeBlock::Number_array_header;
            case 2: return TapeBlock::Character_array_header;
            case 3: return TapeBlock::Code_header;
        }
        return TapeBlock::Unassigned;
    }

    return TapeBlock::Data_block;

}

void Tape::Play() {

    if (tape == NULL) {
        OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
        return;
    }

    if ( TapeListing.empty()) return;

    if (VIDEO::OSD) VIDEO::OSD = 1;

    // Prepare current block to play
    switch(tapeFileType) {
        case TAPE_FTYPE_TAP:
            tapePlayOffset = CalcTapBlockPos(tapeCurBlock);
            GetBlock = &TAP_GetBlock;
            break;
        case TAPE_FTYPE_TZX:
            tapePlayOffset = CalcTZXBlockPos(tapeCurBlock);
            GetBlock = &TZX_GetBlock;
            break;
    }

    // Init tape vars
    tapeEarBit = TAPEHIGH;
    tapeBitMask=0x80;
    tapeLastByteUsedBits = 8;
    tapeEndBitMask=0x80;
    tapeBlockLen = 0;
    tapebufByteCount = 0;
    GDBEnd = false;

    // Get block data
    tapeCurByte = readByteFile(tape);
    if (!feof(tape) && !ferror(tape)) { // check for empty tap
    // if ( tapeCurByte > 0 ) { // check for empty tap

        GetBlock();

        // Start loading
        Tape::tapeStatus=TAPE_LOADING;
        tapeStart=CPU::global_tstates + CPU::tstates;
    } else {
        Tape::tapeStatus=TAPE_STOPPED;
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
    tapeBlockLen += (tapeCurByte | (readByteFile(tape) << 8)) + 2;
    tapeCurByte = readByteFile(tape);
    tapebufByteCount += 2;

    // Set sync phase values
    tapePhase=TAPE_PHASE_SYNC;
    tapeNext=tapeSyncLen;
    if (tapeCurByte) tapeHdrPulses=tapeHdrShort; else tapeHdrPulses=tapeHdrLong;

}

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
            case TAPE_PHASE_CSW:
                tapeEarBit ^= 1;
                if (CSW_CompressionType == 1) { // RLE
                    CSW_PulseLenght = readByteFile(tape);
                    tapebufByteCount++;
                    if (tapebufByteCount == tapeBlockLen) {
                        tapeCurByte = CSW_PulseLenght;
                        if (tapeBlkPauseLen == 0) {
                            tapeCurBlock++;
                            GetBlock();
                        } else {
                            tapePhase=TAPE_PHASE_TAIL;
                            tapeNext = TAPE_PHASE_TAIL_LEN;
                        }
                        break;
                    }
                    if (CSW_PulseLenght == 0) {
                        CSW_PulseLenght = readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
                        tapebufByteCount += 4;
                    }
                    tapeNext = CSW_SampleRate * CSW_PulseLenght;
                } else { // Z-RLE
                    CSW_PulseLenght = readByteFile(cswBlock);
                    if (feof(cswBlock)) {
                        fclose(cswBlock);
                        cswBlock = NULL;
                        tapeCurByte = readByteFile(tape);
                        if (tapeBlkPauseLen == 0) {
                            tapeCurBlock++;
                            GetBlock();
                        } else {
                            tapePhase=TAPE_PHASE_TAIL;
                            tapeNext = TAPE_PHASE_TAIL_LEN;
                        }
                        break;
                    }
                    if (CSW_PulseLenght == 0) {
                        CSW_PulseLenght = readByteFile(cswBlock) | (readByteFile(cswBlock) << 8) | (readByteFile(cswBlock) << 16) | (readByteFile(cswBlock) << 24);
                    }
                    tapeNext = CSW_SampleRate * CSW_PulseLenght;
                }
                break;
            case TAPE_PHASE_GDB_PILOTSYNC:

                // Get next pulse lenght from current symbol
                if (++curGDBPulse < npp)
                    tapeNext = SymDefTable[GDBsymbol].PulseLenghts[curGDBPulse];

                if (tapeNext == 0 || curGDBPulse == npp) {

                    // printf("curGDBPulse: %d, npp: %d\n",(int)curGDBPulse,(int)npp);

                    // Next repetition
                    if (--tapeHdrPulses == 0) {

                        // Get next symbol in PRLE
                        curGDBSymbol++;

                        if (curGDBSymbol < totp) { // If not end of PRLE

                            // Read pulse data
                            GDBsymbol = readByteFile(tape); // Read Symbol to be represented from PRLE

                            // Get symbol flags
                            switch (SymDefTable[GDBsymbol].SymbolFlags) {
                                case 0:
                                    tapeEarBit ^= 1;
                                    break;
                                case 1:
                                    break;
                                case 2:
                                    tapeEarBit = TAPELOW;
                                    break;
                                case 3:
                                    tapeEarBit = TAPEHIGH;
                                    break;
                            }

                            // Get first pulse lenght from array of pulse lenghts
                            tapeNext = SymDefTable[GDBsymbol].PulseLenghts[0];

                            // Get number of repetitions from PRLE[0]
                            tapeHdrPulses = readByteFile(tape) | (readByteFile(tape) << 8); // Number of repetitions of symbol

                            curGDBPulse = 0;

                            tapebufByteCount += 3;

                        } else {

                            // End of PRLE

                            // Free SymDefTable
                            for (int i = 0; i < asp; i++)
                                delete[] SymDefTable[i].PulseLenghts;
                            delete[] SymDefTable;

                            // End of pilotsync. Is there data stream ?
                            if (totd > 0) {

                                // printf("\nPULSES (DATA)\n");

                                // Allocate memory for the array of pointers to struct Symdef
                                SymDefTable = new Symdef[asd];

                                // Allocate memory for each row
                                for (int i = 0; i < asd; i++) {
                                    // Initialize each element in the row
                                    SymDefTable[i].SymbolFlags = readByteFile(tape);
                                    tapebufByteCount += 1;
                                    SymDefTable[i].PulseLenghts = new uint16_t[npd];
                                    for(int j = 0; j < npd; j++) {
                                        SymDefTable[i].PulseLenghts[j] = readByteFile(tape) | (readByteFile(tape) << 8);
                                        SymDefTable[i].PulseLenghts[j] *= tapeCompensation; // Apply tape compensation if needed
                                        tapebufByteCount += 2;
                                    }

                                }

                                // printf("-----------------------\n");
                                // printf("Data Sync Symbol Table\n");
                                // printf("Asd: %d, Npd: %d\n",asd,npd);
                                // printf("-----------------------\n");
                                // for (int i = 0; i < asd; i++) {
                                //     printf("%d: %d; ",i,(int)SymDefTable[i].SymbolFlags);
                                //     for (int j = 0; j < npd; j++) {
                                //         printf("%d,",(int)SymDefTable[i].PulseLenghts[j]);
                                //     }
                                //     printf("\n");
                                // }
                                // printf("-----------------------\n");

                                // printf("END DATA SYMBOL TABLE GDB -> tapeCurByte: %d, Tape pos: %d, Tapebbc: %d\n", tapeCurByte,(int)(ftell(tape)),tapebufByteCount);

                                curGDBSymbol = 0;
                                curGDBPulse = 0;
                                curBit = 7;

                                // Read data stream first symbol
                                GDBsymbol = 0;

                                tapeCurByte = readByteFile(tape);
                                tapebufByteCount += 1;

                                // printf("tapeCurByte: %d, nb:%d\n", (int)tapeCurByte,(int)nb);

                                for (int i = nb; i > 0; i--) {
                                    GDBsymbol <<= 1;
                                    GDBsymbol |= ((tapeCurByte >> (curBit)) & 0x01);
                                    if (curBit == 0) {
                                        tapeCurByte = readByteFile(tape);
                                        tapebufByteCount += 1;
                                        curBit = 7;
                                    } else
                                        curBit--;
                                }

                                // Get symbol flags
                                switch (SymDefTable[GDBsymbol].SymbolFlags) {
                                case 0:
                                    tapeEarBit ^= 1;
                                    break;
                                case 1:
                                    break;
                                case 2:
                                    tapeEarBit = TAPELOW;
                                    break;
                                case 3:
                                    tapeEarBit = TAPEHIGH;
                                    break;
                                }

                                // Get first pulse lenght from array of pulse lenghts
                                tapeNext = SymDefTable[GDBsymbol].PulseLenghts[0];

                                tapePhase = TAPE_PHASE_GDB_DATA;

                                // printf("PULSE%d %d Flags: %d\n",tapeEarBit,tapeNext,(int)SymDefTable[GDBsymbol].SymbolFlags);

                                // printf("Curbit: %d, GDBSymbol: %d, Flags: %d, tapeNext: %d\n",(int)curBit,(int)GDBsymbol,(int)(SymDefTable[GDBsymbol].SymbolFlags & 0x3),(int)tapeNext);

                            } else {

                                tapeCurByte = readByteFile(tape);
                                tapeEarBit ^= 1;
                                tapePhase=TAPE_PHASE_TAIL_GDB;
                                tapeNext = TAPE_PHASE_TAIL_LEN_GDB;

                            }

                        }

                    } else {

                        // Modify tapeearbit according to symbol flags
                        switch (SymDefTable[GDBsymbol].SymbolFlags) {
                            case 0:
                                tapeEarBit ^= 1;
                                break;
                            case 1:
                                break;
                            case 2:
                                tapeEarBit = TAPELOW;
                                break;
                            case 3:
                                tapeEarBit = TAPEHIGH;
                                break;
                        }

                        tapeNext = SymDefTable[GDBsymbol].PulseLenghts[0];

                        curGDBPulse = 0;

                    }

                } else {

                    tapeEarBit ^= 1;

                }

                break;

            case TAPE_PHASE_GDB_DATA:

                // Get next pulse lenght from current symbol
                if (++curGDBPulse < npd) tapeNext = SymDefTable[GDBsymbol].PulseLenghts[curGDBPulse];

                if (curGDBPulse == npd || tapeNext == 0) {

                    // Get next symbol in data stream
                    curGDBSymbol++;

                    if (curGDBSymbol < totd) { // If not end of data stream read next symbol

                        GDBsymbol = 0;

                        // printf("tapeCurByte: %d, NB: %d, ", tapeCurByte,nb);

                        for (int i = nb; i > 0; i--) {
                            GDBsymbol <<= 1;
                            GDBsymbol |= ((tapeCurByte >> (curBit)) & 0x01);
                            if (curBit == 0) {
                                tapeCurByte = readByteFile(tape);
                                tapebufByteCount += 1;
                                curBit = 7;
                            } else
                                curBit--;
                        }

                        // Get symbol flags
                        switch (SymDefTable[GDBsymbol].SymbolFlags) {
                            case 0:
                                tapeEarBit ^= 1;
                                break;
                            case 1:
                                break;
                            case 2:
                                tapeEarBit = TAPELOW;
                                break;
                            case 3:
                                tapeEarBit = TAPEHIGH;
                                break;
                        }

                        // Get first pulse lenght from array of pulse lenghts
                        tapeNext = SymDefTable[GDBsymbol].PulseLenghts[0];

                        curGDBPulse = 0;

                    } else {

                        // Needed Adjustment
                        tapebufByteCount--;

                        // printf("END DATA GDB -> tapeCurByte: %d, Tape pos: %d, Tapebbc: %d, TapeBlockLen: %d\n", tapeCurByte,(int)(ftell(tape)),tapebufByteCount, tapeBlockLen);

                        // Free SymDefTable
                        for (int i = 0; i < asd; i++)
                            delete[] SymDefTable[i].PulseLenghts;
                        delete[] SymDefTable;

                        if (tapeBlkPauseLen == 0) {

                            if (tapeCurByte == 0x13) tapeEarBit ^= 1; // This is needed for Basil, maybe for others (next block == Pulse sequence)

                            GDBEnd = true; // Provisional: add special end to GDB data blocks with pause 0

                            tapeCurBlock++;
                            GetBlock();

                        } else {

                            GDBEnd = false; // Provisional: add special end to GDB data blocks with pause 0

                            tapeEarBit ^= 1;
                            tapePhase=TAPE_PHASE_TAIL_GDB;
                            tapeNext = TAPE_PHASE_TAIL_LEN_GDB;

                        }

                    }
                } else
                    tapeEarBit ^= 1;

                break;

            case TAPE_PHASE_TAIL_GDB:
                tapeEarBit = TAPELOW;
                tapePhase=TAPE_PHASE_PAUSE_GDB;
                tapeNext=tapeBlkPauseLen;
                break;

            case TAPE_PHASE_PAUSE_GDB:
                tapeEarBit = TAPEHIGH;
                tapeCurBlock++;
                GetBlock();
                break;

            case TAPE_PHASE_DRB:
                tapeBitMask = (tapeBitMask >> 1) | (tapeBitMask << 7);
                if (tapeBitMask == tapeEndBitMask) {
                    tapeCurByte = readByteFile(tape);
                    tapebufByteCount++;
                    if (tapebufByteCount == tapeBlockLen) {
                        if (tapeBlkPauseLen == 0) {
                            tapeCurBlock++;
                            GetBlock();
                        } else {
                            tapePhase=TAPE_PHASE_TAIL;
                            tapeNext = TAPE_PHASE_TAIL_LEN;
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
                    tapeEarBit = tapeCurByte & tapeBitMask ? TAPEHIGH : TAPELOW;
                } else {
                    tapeEarBit = tapeCurByte & tapeBitMask ? TAPEHIGH : TAPELOW;
                }
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
                    tapeCurByte = readByteFile(tape);
                    tapeCurBlock++;
                    GetBlock();
                }
                break;
            case TAPE_PHASE_PULSESEQ:
                tapeEarBit ^= 1;
                if (--tapeHdrPulses == 0) {
                    tapeCurByte = readByteFile(tape);
                    tapeCurBlock++;
                    GetBlock();
                } else {
                    tapeNext=(readByteFile(tape) | (readByteFile(tape) << 8));
                    tapebufByteCount += 2;
                }
                break;
            case TAPE_PHASE_END:
                tapeEarBit = TAPEHIGH;
                tapeCurBlock = 0;
                Stop();
                rewind(tape);
                tapeNext = 0xFFFFFFFF;
                break;
            case TAPE_PHASE_TAIL:
                tapeEarBit = TAPELOW;
                tapePhase=TAPE_PHASE_PAUSE;
                tapeNext=tapeBlkPauseLen;
                break;
            case TAPE_PHASE_PAUSE:
                tapeEarBit = TAPEHIGH;
                tapeCurBlock++;
                GetBlock();
            }
        } while (tapeCurrent >= tapeNext);

        // More precision just for DRB and CSW. Makes some loaders work but bigger TAIL_LEN also does and seems better solution.
        // if (tapePhase == TAPE_PHASE_DRB || tapePhase == TAPE_PHASE_CSW)
            tapeStart = CPU::global_tstates + CPU::tstates - tapeCurrent;
        // else
        //     tapeStart = CPU::global_tstates + CPU::tstates;

    }
}

void Tape::Save() {

	FILE *fichero = Tape::tape;
    unsigned char xxor,salir_s;
	uint8_t dato;
	int longitud;

    // Tape::Stop();
    // if (tape != NULL) {
    //     fclose(tape);
    //     tape = NULL;
    // }

    // fichero = fopen(tapeSaveName.c_str(), "ab");
    // if (fichero == NULL) {
    //     OSD::osdCenteredMsg(OSD_TAPE_SAVE_ERR, LEVEL_ERROR);
    //     return;
    // }

    fseek(fichero,0,SEEK_END);

	xxor=0;

	longitud=(int)(Z80::getRegDE());
	longitud+=2;

	dato=(uint8_t)(longitud%256);
    fwrite(&dato,sizeof(uint8_t),1,fichero);
	dato=(uint8_t)(longitud/256);
    fwrite(&dato,sizeof(uint8_t),1,fichero); // file length

    dato = Z80::getRegA(); // flag
	fwrite(&dato,sizeof(uint8_t),1,fichero);

	xxor^=Z80::getRegA();

	salir_s = 0;
	do {
	 	if (Z80::getRegDE() == 0)
	 		salir_s = 2;
	 	if (!salir_s) {
            dato = MemESP::readbyte(Z80::getRegIX());
            fwrite(&dato,sizeof(uint8_t),1,fichero);
            xxor^=dato;
            Z80::setRegIX(Z80::getRegIX() + 1);
            Z80::setRegDE(Z80::getRegDE() - 1);
        }
    } while (!salir_s);
    fwrite(&xxor,sizeof(unsigned char),1,fichero);
    Z80::setRegIX(Z80::getRegIX() + 2);

    TAP_getBlockData();

    // fclose(fichero);

    // Tape::TAP_Open( tapeFileName );

    // Tape::TAP_Open( tapeSaveName );


}

bool Tape::FlashLoad() {

    if (tape == NULL) {

        string fname = FileUtils::MountPoint + FileUtils::TAP_Path + tapeFileName;

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

void Tape::selectBlockToggle(int block) {
    auto it = std::find(Tape::selectedBlocks.begin(), Tape::selectedBlocks.end(), block);

    if (it != Tape::selectedBlocks.end()) {
        // Eliminar el bloque si está presente
        Tape::selectedBlocks.erase(it);
    } else {
        // Agregar el bloque si no está presente
        Tape::selectedBlocks.push_back(block);
    }
}

bool Tape::isSelectedBlock(int block) {
    return std::find(Tape::selectedBlocks.begin(), Tape::selectedBlocks.end(), block) != Tape::selectedBlocks.end();
}

void Tape::removeSelectedBlocks() {

    string tempDir = FileUtils::createTmpDir();
    if ( tempDir == "" ) {
        return;
    }

    char buffer[2048];

    string filenameTemp = tempDir + "/temp.tap";

    FILE* tempFile = fopen(filenameTemp.c_str(), "wb");
    if (!tempFile) {
        printf("Error al crear el archivo temporal.\n");
        return;
    }

    string filename = tapeFilePath + tapeFileName;

    int blockIndex = 0;
    int tapeBlkLen = 0;
    long off = 0;
    while (true) {
        fseek(tape, off, SEEK_SET);

        uint16_t encodedlen;

        // Leer la longitud del bloque
        size_t bytesRead = fread(&encodedlen, sizeof(encodedlen), 1, tape);
        if ( bytesRead != 1 ) break;

        uint8_t l = (( uint8_t * ) &encodedlen)[0];
        uint8_t h = (( uint8_t * ) &encodedlen)[1];

        // Analyze .tap file
        tapeBlkLen = (l | (h << 8));

        // Verificar si el bloque actual está en el vector selected
        if (std::find(selectedBlocks.begin(), selectedBlocks.end(), blockIndex) == selectedBlocks.end()) {
            // Escribir la longitud del bloque
            fwrite(&encodedlen, sizeof(encodedlen), 1, tempFile);

            // Leer la data del bloque, limitando a sizeof(buffer)
            size_t totalBytesRead = 0;
            while (totalBytesRead < tapeBlkLen) {
                size_t bytesToRead = std::min(sizeof(buffer), tapeBlkLen - totalBytesRead);
                size_t bytesRead = fread(buffer, 1, bytesToRead, tape);
                if (bytesRead != bytesToRead) {
                    printf("Error al leer el bloque del archivo.\n");
                    break; // Salir del bucle interno si hay un error de lectura
                }

                // Escribir los datos leídos en el archivo temporal
                fwrite(buffer, 1, bytesRead, tempFile);
                totalBytesRead += bytesRead;
            }

        }

        off += tapeBlkLen + sizeof(encodedlen);

        blockIndex++;
    }

    fclose(tempFile);

    Tape::Stop();

    if (tape != NULL) {
        fclose(tape);
        tape = NULL;
    }

    // Reemplazar el archivo original con el archivo temporal
    std::remove(filename.c_str());
    std::rename(filenameTemp.c_str(), filename.c_str());

    // Read and analyze tap file
    Tape::TAP_Open(tapeFileName, tapeFilePath);

    selectedBlocks.clear();

}

void Tape::moveSelectedBlocks(int targetPosition) {

    char buffer[2048];

    // Crear un archivo temporal para la salida
    string tempDir = FileUtils::createTmpDir();
    if (tempDir == "") {
        return;
    }

    string outputFilename = tempDir + "/temp.tap";

    FILE* outputFile = fopen(outputFilename.c_str(), "wb");
    if (!outputFile) {
        printf("Error al crear el archivo de salida temporal.\n");
        return;
    }

    string filename = tapeFilePath + tapeFileName;

    int blockIndex = 0;
    int tapeBlkLen = 0;
    long off = 0;

    uint16_t encodedlen;

    // Definir la lambda writeBlock
    auto writeBlock = [&](uint16_t encodedlen, FILE* inputFile, FILE* outputFile) {
        fwrite(&encodedlen, sizeof(encodedlen), 1, outputFile);
        size_t totalBytesRead = 0;
        while (totalBytesRead < tapeBlkLen) {
            size_t bytesToRead = std::min(sizeof(buffer), static_cast<size_t>(tapeBlkLen - totalBytesRead));
            size_t bytesRead = fread(buffer, 1, bytesToRead, inputFile);
            if (bytesRead != bytesToRead) {
                printf("Error al leer el bloque del archivo.\n");
                break;
            }
            fwrite(buffer, 1, bytesRead, outputFile);
            totalBytesRead += bytesRead;
        }
    };

    std::vector<long> selectedBlocksOffsets;
    std::vector<long> remainsBlocksOffsets;

    rewind(tape);

    while (true) {
        fseek(tape, off, SEEK_SET);

        bool block_is_selected = ( std::find(selectedBlocks.begin(), selectedBlocks.end(), blockIndex) != selectedBlocks.end() );

        size_t bytesRead = fread(&encodedlen, sizeof(encodedlen), 1, tape);
        if (bytesRead != 1) break;

        uint8_t l = ((uint8_t*)&encodedlen)[0];
        uint8_t h = ((uint8_t*)&encodedlen)[1];
        tapeBlkLen = (l | (h << 8));

        if ( !block_is_selected && blockIndex < targetPosition ) {
            writeBlock(encodedlen, tape, outputFile);
        } else if ( block_is_selected ) {
            selectedBlocksOffsets.push_back(off);
        } else {
            remainsBlocksOffsets.push_back(off);
        }

        off += tapeBlkLen + sizeof(encodedlen);
        blockIndex++;
    }

    selectedBlocksOffsets.insert(selectedBlocksOffsets.end(), remainsBlocksOffsets.begin(), remainsBlocksOffsets.end());

    for (int off : selectedBlocksOffsets) {
        fseek(tape, off, SEEK_SET);

        size_t bytesRead = fread(&encodedlen, sizeof(encodedlen), 1, tape);
        if (bytesRead != 1) break;

        uint8_t l = ((uint8_t*)&encodedlen)[0];
        uint8_t h = ((uint8_t*)&encodedlen)[1];
        tapeBlkLen = (l | (h << 8));

        writeBlock(encodedlen, tape, outputFile);
    }

    fclose(outputFile);

    Tape::Stop();

    if (tape != NULL) {
        fclose(tape);
        tape = NULL;
    }

    std::remove(filename.c_str());
    std::rename(outputFilename.c_str(), filename.c_str());

    // Read and analyze tap file
    Tape::TAP_Open(tapeFileName,tapeFilePath);

    selectedBlocks.clear();
}

string Tape::getBlockName(int block) {

    // Read header name
    char fname[11] = { 0 };
    fseek( tape, CalcTapBlockPos(block) + 3, SEEK_SET );
    uint8_t blocktype = readByteFile(Tape::tape);
    if (blocktype <= TapeBlock::Code_header) {
        fread( fname, 1, 10, tape );
        string ret = (char *) fname;
        rtrim(ret);
        return ret;
    }

    return "";

}

void Tape::renameBlock(int block, string new_name) {

    char fname[11];
    uint8_t header[18]; // header + checksum (without encoded block len and flag)

    TapeBlock::BlockType blocktype = getBlockType(block);
    switch( blocktype ) {
        case TapeBlock::Program_header:
        case TapeBlock::Number_array_header:
        case TapeBlock::Character_array_header:
        case TapeBlock::Code_header: {

            long blockNameOff = CalcTapBlockPos(block) + 3; // size + flag

            // Read header
            fseek( tape, blockNameOff, SEEK_SET );
            fread( header, 1, sizeof( header ), tape );

            // Replace name
            sprintf(fname, "%-10.10s", new_name.c_str());
            memmove(&header[1], fname, sizeof(fname)-1);

            // Calculate checksum
            uint8_t checksum = 0;
            for(int i = 0; i < sizeof( header ) - 1; checksum ^= header[i++] );
            header[17] = checksum;

            // write header
            fseek( tape, blockNameOff, SEEK_SET );
            fwrite(header, 1, sizeof(header), tape);
            break;
        }
    }
}
