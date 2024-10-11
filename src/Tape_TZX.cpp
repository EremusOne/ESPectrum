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

using namespace std;

#include "Tape.h"
#include "FileUtils.h"
#include "OSDMain.h"
#include "Config.h"
#include "messages.h"
#include "Z80_JLS/z80.h"

void Tape::TZX_BlockLen(TZXBlock &blockdata) {

    uint32_t tapeBlkLen;

    uint8_t tzx_blk_type = readByteFile(tape);

    switch (tzx_blk_type) {

        case 0x10: // Standard Speed Data

            fseek(tape,2,SEEK_CUR); // Jump pause len data
            tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8);
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen += 0x4; // Add pause len and block len bytes

            break;

        case 0x11: // Turbo Speed Data

            fseek(tape,0xf,SEEK_CUR); // Jump block data
            tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16);
            fseek(tape,tapeBlkLen,SEEK_CUR);

            tapeBlkLen +=  0x12; // Add block data bytes lenght

            break;

        case 0x12: // Pure Tone

            fseek(tape,0x4,SEEK_CUR); // Jump block data
            tapeBlkLen = 0x4;

            break;

        case 0x13: // Pulse sequence

            tapeBlkLen = readByteFile(tape) << 1;
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen += 0x01;

            break;

        case 0x14: // Pure Data

            fseek(tape,0x7,SEEK_CUR); // Jump block data
            tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16);
            fseek(tape,tapeBlkLen,SEEK_CUR);

            tapeBlkLen +=  0x0a;

            break;

        case 0x15: // Direct recording

            fseek(tape,0x5,SEEK_CUR); // Jump block data
            tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16);
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen +=  0x08;

            break;

        case 0x18: // CSW Recording

            tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
            fseek(tape,0x5,SEEK_CUR);

            if (readByteFile(tape) == 2) // Z-RLE
                tzx_blk_type = 0xF8; // Use this code to signal Z-RLE 0x18 block

            fseek(tape,tapeBlkLen - 0x6,SEEK_CUR);
            tapeBlkLen +=  0x04;

            // tapeBlkLen = -1; // For disable it until implementation

            break;

        case 0x19: // Generalized Data Block

            tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen +=  0x04;

            // tapeBlkLen = -1; // For disable it until implementation

            break;

        case 0x20: // Pause or Stop the tape

            blockdata.PauseLenght = readByteFile(tape) | (readByteFile(tape) << 8);
            tapeBlkLen = 0x2;

            break;

        case 0x21:

            tapeBlkLen = readByteFile(tape);
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen += 0x1;

            break;

        case 0x22:
        case 0x25:
        case 0x27:

            tapeBlkLen = 0;

            break;

        case 0x23: // Jump to block
        case 0x24: // Loop start

            fseek(tape,0x2,SEEK_CUR);
            tapeBlkLen = 0x2;

            break;

        case 0x26:

            tapeBlkLen=(readByteFile(tape) | (readByteFile(tape) << 8)) << 1;
            fseek(tape,tapeBlkLen,SEEK_CUR);

            tapeBlkLen +=  0x02;

            break;

        case 0x28:

            tapeBlkLen= readByteFile(tape) | (readByteFile(tape) << 8);
            fseek(tape,tapeBlkLen,SEEK_CUR);

            tapeBlkLen +=  0x02;

            break;

        case 0x2A:

            fseek(tape,0x4,SEEK_CUR);
            tapeBlkLen = 0x4;

            break;

        case 0x2B:

            fseek(tape,0x5,SEEK_CUR);
            tapeBlkLen = 0x5;

            break;

        case 0x30:

            tapeBlkLen = readByteFile(tape);
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen += 0x1;

            break;

        case 0x31:

            fseek(tape,1,SEEK_CUR);
            tapeBlkLen = readByteFile(tape);
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen += 0x2;

            break;

        case 0x32:

            tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8);
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen += 0x2;

            break;

        case 0x33:

            tapeBlkLen=readByteFile(tape) * 0x3;
            fseek(tape,tapeBlkLen,SEEK_CUR);
            tapeBlkLen += 0x1;

            break;

        case 0x35:

            fseek(tape,0x10,SEEK_CUR);
            tapeBlkLen=readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
            fseek(tape,tapeBlkLen,SEEK_CUR);

            tapeBlkLen += 0x14;

            break;

        case 0x5a:

            fseek(tape,0x9,SEEK_CUR);
            tapeBlkLen = 0x9;

            break;

        default:

            tapeBlkLen = -1;

    }

    blockdata.BlockType = tzx_blk_type;
    blockdata.BlockLenght = tapeBlkLen + 1;

}

string Tape::tzxBlockReadData(int Blocknum) {

    int tapeContentIndex = 0;
    int tapeBlkLen = 0;
    string blktype;
    char buf[48];
    char fname[10];

    TZXBlock TZXblock;

    tapeContentIndex = Tape::CalcTZXBlockPos(Blocknum);
    TZX_BlockLen(TZXblock);
    tapeBlkLen = TZXblock.BlockLenght;

    switch(TZXblock.BlockType) {
        case 0x10:
            blktype = "Standard     ";
            tapeBlkLen -= 0x4 + 1;
            break;
        case 0x11:
            blktype = "Turbo        ";
            tapeBlkLen -= 0x12 + 1;
            break;
        case 0x12:
            blktype = "Pure Tone       ";
            tapeBlkLen = -1;
            break;
        case 0x13:
            blktype = "Pulse sequence  ";
            tapeBlkLen = -1;
            break;
        case 0x14:
            blktype = "Pure Data    ";
            tapeBlkLen -= 0x0a + 1;
            break;
        case 0x15:
            blktype = "DRB          ";
            tapeBlkLen -= 0x08 + 1;
            break;
        case 0x18:
            blktype = "CSW (RLE)    ";
            tapeBlkLen = -1;
            break;
        case 0x19:
            blktype = "GDB          ";
            tapeBlkLen = -1;
            break;
        case 0x20:
            if (TZXblock.PauseLenght == 0) {
                blktype = "Stop the tape";
                tapeBlkLen = -1;
            } else {
                blktype = "Pause (ms.)  ";
                tapeBlkLen = TZXblock.PauseLenght;
            }
            break;
        case 0x21:
            blktype = "Group start";
            tapeBlkLen = -1;
            break;
        case 0x22:
            blktype = "Group end";
            tapeBlkLen = -1;
            break;
        case 0x23:
            blktype = "Jump to block";
            tapeBlkLen = -1;
            break;
        case 0x24:
            blktype = "Loop start";
            tapeBlkLen = -1;
            break;
        case 0x25:
            blktype = "Loop end";
            tapeBlkLen = -1;
            break;
        case 0x26:
            blktype = "Call sequence";
            tapeBlkLen = -1;
            break;
        case 0x27:
            blktype = "Return from sequence";
            tapeBlkLen = -1;
            break;
        case 0x28:
            blktype = "Select block";
            tapeBlkLen = -1;
            break;
        case 0x2a:
            blktype = "Stop if in 48K mode";
            tapeBlkLen = -1;
            break;
        case 0x2b:
            blktype = "Set signal level";
            tapeBlkLen = -1;
            break;
        case 0x30:
            blktype = "Text";
            tapeBlkLen = -1;
            break;
        case 0x31:
            blktype = "Message block";
            tapeBlkLen = -1;
            break;
        case 0x32:
            blktype = "Archive info";
            tapeBlkLen = -1;
            break;
        case 0x33:
            blktype = "Hardware type";
            tapeBlkLen = -1;
            break;
        case 0x35:
            blktype = "Custom info";
            tapeBlkLen = -1;
            break;
        case 0x5a:
            blktype = "Glue block";
            tapeBlkLen = -1;
            break;
        case 0xF8:
            blktype = "CSW (Z-RLE)  ";
            tapeBlkLen = -1;
            break;
        default:
            blktype = "Unknown";
            tapeBlkLen = -1;
    }

    fname[0] = '\0';

    if (tapeBlkLen >= 0)
        snprintf(buf, sizeof(buf), "%04d %s %10s % 6d\n", Blocknum + 1, blktype.c_str(), fname, tapeBlkLen);
    else
        snprintf(buf, sizeof(buf), "%04d %s\n", Blocknum + 1, blktype.c_str());

    return buf;

}

void Tape::TZX_Open(string name, string path) {

    if (tape != NULL) {
        fclose(tape);
        tape = NULL;
        tapeFileType = TAPE_FTYPE_EMPTY;
    }

    if (cswBlock != NULL) {
        fclose(cswBlock);
        cswBlock = NULL;
    }

    FileUtils::deleteFilesWithExtension(FileUtils::MountPoint.c_str(),".tmp");

    string fname = path + name;

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

    // ERRONEA -> tzxheader[8] puede no contener \0 y eso provocar que el strcmp falle
    // char tzxheader[8];
    // fread(&tzxheader, 8, 1, tape);
    // if (strcmp(tzxheader,"ZXTape!\x1a") != 0) {

    // VALIDA -> strncmp se limita a comparar los caracteres indicados. Ademas el orden de los parametros en fread es mas correcto (lee 8 elementos de 1 byte).
    char tzxheader[8];
    fread(&tzxheader, 1, 8, tape);
    if (strncmp(tzxheader,"ZXTape!\x1a", 8) != 0) {

    // VALIDA -> Declarando asi tzxheader nos aseguramos que tzxheader[8] sea \0. Ademas el orden de los parametros en fread es mas correcto (lee 8 elementos de 1 byte).
    // char tzxheader[9] = { 0 };
    // fread(&tzxheader, 1, 8, tape);
    // if (strcmp(tzxheader,"ZXTape!\x1a") != 0) {

    // // VALIDA -> fgets lee n -1 caracteres y añade un \0 a tzxheader[8].
    // char tzxheader[9];
    // // printf("8 -> %d\n",tzxheader[8]);
    // fgets(tzxheader,9,tape);
    // // printf("8 -> %d\n",tzxheader[8]);
    // if (strcmp(tzxheader,"ZXTape!\x1a") != 0) {

        OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
        fclose(tape);
        tape = NULL;
        tapeFileType = TAPE_FTYPE_EMPTY;
        return;
    }

    tapeFilePath = path;
    tapeFileName = name;

    fseek(tape, 2, SEEK_CUR); // Jump TZX version bytes
    // printf("TZX version: %d.%d\n",(int)readByteFile(tape),(int)readByteFile(tape));

    Tape::TapeListing.clear(); // Clear TapeListing vector
    std::vector<TapeBlock>().swap(TapeListing); // free memory

    int tapeListIndex=0;
    int tapeContentIndex=10;
    uint32_t tapeBlkLen=0;

    TapeBlock block;
    TZXBlock TZXblock;

    do {

        TZX_BlockLen(TZXblock);
        tapeBlkLen = TZXblock.BlockLenght;

        // tapeBlkLen = TZX_BlockLen();

        if (tapeBlkLen == 0) {

            Tape::TapeListing.clear(); // Clear TapeListing vector
            std::vector<TapeBlock>().swap(TapeListing); // free memory

            OSD::osdCenteredMsg("Block type not supported.", LEVEL_ERROR);
            fclose(tape);
            tape = NULL;
            tapeFilePath="none";
            tapeFileName="none";
            tapeFileType = TAPE_FTYPE_EMPTY;
            return;
        }

        // If block is CSW Z-RLE decompress it to SD card
        if (TZXblock.BlockType == 0xf8 ) {
            printf("Inflating CSW Z-RLE block!\n");
            fseek(tape,tapeContentIndex + 0x0e + 1,SEEK_SET);
            inflateCSW(tapeListIndex, tapeContentIndex + 0x0e + 1,TZXblock.BlockLenght - 0x0e - 1);
            TZXblock.BlockType = 0x18;
        }

        if ((tapeListIndex & (TAPE_LISTING_DIV - 1)) == 0) {
            block.StartPosition = tapeContentIndex;
            TapeListing.push_back(block);
        }

        tapeListIndex++;

        tapeContentIndex += tapeBlkLen;

    } while(tapeContentIndex < tapeFileSize);

    tapeCurBlock = 0;
    tapeNumBlocks = tapeListIndex;

    tapeFileType = TAPE_FTYPE_TZX;

    rewind(tape);

}

uint32_t Tape::CalcTZXBlockPos(int block) {

    TZXBlock TZXblock;

    int TapeBlockRest = block & (TAPE_LISTING_DIV -1);
    int CurrentPos = TapeListing[block / TAPE_LISTING_DIV].StartPosition;

    fseek(tape,CurrentPos,SEEK_SET);

    while (TapeBlockRest-- != 0) {
        TZX_BlockLen(TZXblock);
        CurrentPos += TZXblock.BlockLenght;
        // uint32_t tapeBlkLen = TZX_BlockLen();
        // CurrentPos += tapeBlkLen;
    }

    return CurrentPos;

}

void Tape::TZX_GetBlock() {

    int tapeData;
    short jumpDistance;
    char cswFileName[16]; // Nombre del archivo descomprimido

    for (;;) {

        if (tapeCurBlock >= tapeNumBlocks) {
            if (GDBEnd) {
                tapeEarBit ^= 1;
                tapePhase = TAPE_PHASE_END;
                // tapeNext = 7000;
                tapeNext = TAPE_PHASE_TAIL_LEN;
            } else {
                tapeCurBlock = 0;
                Stop();
                rewind(tape);
                tapeNext = 0xFFFFFFFF;
            }
            return;

            // //HACK: Sometimes a tape might have it's last tail pulse missing.
            // //In case it's the last block in the tape, it's best to flip the tape bit
            // //a last time to ensure that the process is terminated properly.
            // if (blockCounter == PZXFile.blocks.Count - 1) {
            //     currentBit = -1;
            //     edgeDuration = (3500 * 2);
            //     return true;
            // }

            // sorry, I do have a special end-of-pzx block heh, wrote this code ages ago, but still no go for you lol
            // [17:10]
            // I added a special block extending the current pulse level (whatever it is for the current file) by 1710 tstates, before going low, but not doing that doesn't affect Hollywood which still works (editado)
            // [17:10]
            // but glad you fixed it!

            // ; extend the final pulse before ending tape playback - allows for DATA blocks with TAIL pulses of 0...

        }

        switch (tapeCurByte) {

            case 0x10: // Standard Speed Data

                // printf("TZX block: %d, ID 0x10 - Standard Speed Data Block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

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

                tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500;
                tapeBlockLen += (readByteFile(tape) | (readByteFile(tape) << 8)) + 4 + 1;
                tapebufByteCount += 4 + 1;

                tapeBitMask=0x80;
                tapeLastByteUsedBits = 8;
                tapeEndBitMask = 0x80;

                tapeCurByte = readByteFile(tape);
                if (tapeCurByte & 0x80) tapeHdrPulses=tapeHdrShort; else tapeHdrPulses=tapeHdrLong;

                tapeSyncLen *= tapeCompensation;
                tapeSync1Len *= tapeCompensation;
                tapeSync2Len *= tapeCompensation;
                tapeBit0PulseLen *= tapeCompensation;
                tapeBit1PulseLen *= tapeCompensation;
                tapeBlkPauseLen *= tapeCompensation;

                tapePhase = TAPE_PHASE_SYNC;
                tapeNext = tapeSyncLen;

                return;

            case 0x11: // Turbo Speed Data

                // printf("TZX block: %d, ID 0x11 - Turbo Speed Data Block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeSyncLen = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeSync1Len = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeSync2Len = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeBit0PulseLen = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeBit1PulseLen = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeHdrPulses = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeLastByteUsedBits = readByteFile(tape);
                tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500;
                tapeBlockLen += (readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16)) + 0x12 + 1;
                tapebufByteCount += 0x12 + 1;

                tapeBitMask=0x80;
                tapeEndBitMask = 0x80;
                if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

                tapeCurByte = readByteFile(tape);

                tapeSyncLen *= tapeCompensation;
                tapeSync1Len *= tapeCompensation;
                tapeSync2Len *= tapeCompensation;
                tapeBit0PulseLen *= tapeCompensation;
                tapeBit1PulseLen *= tapeCompensation;
                tapeBlkPauseLen *= tapeCompensation;

                tapePhase=TAPE_PHASE_SYNC;
                tapeNext = tapeSyncLen;

                return;

            case 0x12: // Pure Tone

                // printf("TZX block: %d, ID 0x12 - Pure Tone Block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeSyncLen = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeHdrPulses = readByteFile(tape) | (readByteFile(tape) << 8);

                tapeBlockLen += 0x4 + 1;
                tapebufByteCount += 0x4 + 1;

                tapeSyncLen *= tapeCompensation;

                tapePhase = TAPE_PHASE_PURETONE;
                tapeNext = tapeSyncLen;

                return;

            case 0x13: // Pulse sequence

                // printf("TZX block: %d, ID 0x13 - Pulse Sequence Block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeHdrPulses = readByteFile(tape);
                tapeSyncLen=readByteFile(tape) | (readByteFile(tape) << 8);

                tapeBlockLen += (tapeHdrPulses << 1) + 1 + 1;
                tapebufByteCount += 0x3 + 1;

                tapeSyncLen *= tapeCompensation;

                tapePhase = TAPE_PHASE_PULSESEQ;
                tapeNext = tapeSyncLen;

                return;

            case 0x14: // Pure Data

                // printf("TZX block: %d, ID 0x14 - Pure Data Block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeBit0PulseLen = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeBit1PulseLen = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeLastByteUsedBits=readByteFile(tape);
                tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500;

                tapeBlockLen += (readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16)) + 0x0a + 1;
                tapebufByteCount += 0x0a + 1;

                // tapePulseCount=0;
                tapeBitMask=0x80;
                // tapeBitPulseCount=0;
                tapeEndBitMask = 0x80;
                if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

                tapeCurByte = readByteFile(tape);

                tapeBit0PulseLen *= tapeCompensation;
                tapeBit1PulseLen *= tapeCompensation;
                tapeBlkPauseLen *= tapeCompensation;

                tapePhase=TAPE_PHASE_DATA1;
                tapeNext = tapeCurByte & tapeBitMask ? tapeBit1PulseLen : tapeBit0PulseLen;

                return;

            case 0x15: // Direct recording

                // printf("TZX block: %d, ID 0x15 - Direct recording Block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeSyncLen = readByteFile(tape) | (readByteFile(tape) << 8);
                tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500;
                tapeLastByteUsedBits = readByteFile(tape);

                tapeBlockLen += (readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16)) + 0x08 + 1;
                tapebufByteCount += 0x08 + 1;

                // tapePulseCount=0;
                tapeBitMask=0x80;
                // tapeBitPulseCount=0;
                tapeEndBitMask = 0x80;
                if (((tapebufByteCount + 1) == tapeBlockLen) && (tapeLastByteUsedBits < 8)) tapeEndBitMask >>= tapeLastByteUsedBits;

                tapeCurByte = readByteFile(tape);
                tapeEarBit = tapeCurByte & tapeBitMask ? TAPEHIGH : TAPELOW;

                tapeSyncLen *= tapeCompensation;
                tapeBlkPauseLen *= tapeCompensation;

                tapePhase=TAPE_PHASE_DRB;
                tapeNext = tapeSyncLen;

                return;

            case 0x18: // CSW recording

                // printf("TZX block: %d, ID 0x18 - CSW recording Block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeData = readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
                tapeBlockLen += tapeData + 0x04 + 1;
                tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500;
                CSW_SampleRate = 3500000 / (readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16)); // Sample rate (hz) converted to t-states per sample
                CSW_CompressionType = readByteFile(tape);
                CSW_StoredPulses = readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);

                tapebufByteCount += 0x0e + 1;

                CSW_SampleRate *= tapeCompensation;
                tapeBlkPauseLen *= tapeCompensation;

                if (CSW_CompressionType == 0x2) { // Z-RLE compression

                    // printf("Reading CSW Z-RLE block from SD Card!\n");
                    // printf(MOUNT_POINT_SD "/.csw%04d.tmp\n",tapeCurBlock);

                    // Open csw file
                    sprintf(cswFileName,MOUNT_POINT_SD "/.csw%04d.tmp",tapeCurBlock);
                    cswBlock = fopen(cswFileName, "rb");
                    if (!cswBlock) {
                        printf("Failed opening csw block!\n");
                        tapeCurBlock = 0;
                        Stop();
                        fclose(tape);
                        tape = NULL;
                        tapeFileName="none";
                        tapeFileType = TAPE_FTYPE_EMPTY;
                        return;
                    }
                    CSW_PulseLenght = readByteFile(cswBlock);
                    if (CSW_PulseLenght == 0) {
                        CSW_PulseLenght = readByteFile(cswBlock) | (readByteFile(cswBlock) << 8) | (readByteFile(cswBlock) << 16) | (readByteFile(cswBlock) << 24);
                    }
                    fseek(tape,tapeData - 0x0a,SEEK_CUR);
                    tapebufByteCount = tapeBlockLen;
                } else {
                    CSW_PulseLenght = readByteFile(tape);
                    if (CSW_PulseLenght == 0) {
                        CSW_PulseLenght = readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
                        tapebufByteCount += 4;
                    }
                }

                tapePhase=TAPE_PHASE_CSW;
                tapeNext = CSW_SampleRate * CSW_PulseLenght;

                return;

            case 0x19:

                // printf("\n\n------------------------------------------------------------------------------------------------------------------------\nTZX block: %d, ID 0x19 - Generalized Data Block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeData = readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);

                tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500;

                totp = readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
                npp = readByteFile(tape);
                asp = readByteFile(tape);
                if (asp == 0) asp = 256;

                totd = readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
                npd = readByteFile(tape);
                asd = readByteFile(tape);
                if (asd == 0) asd = 256;

                tapebufByteCount += 18;

                // Calc number of bits -> nb = ceil(log2(asd))
                nb = 0;
                for (int i = asd; i > 0; i >>=1, nb++);
                if ((asd & (asd - 1)) == 0) nb--;

                tapeBlkPauseLen *= tapeCompensation; // Apply TZX compensation

                // Populate Pilot and Sync definition table
                if (totp > 0) {

                    // Allocate memory for the array of pointers to struct Symdef
                    SymDefTable = new Symdef[asp];

                    // Allocate memory for each row
                    for (int i = 0; i < asp; i++) {
                        // Initialize each element in the row
                        SymDefTable[i].SymbolFlags = readByteFile(tape);
                        tapebufByteCount += 1;
                        SymDefTable[i].PulseLenghts = new uint16_t[npp];
                        for(int j = 0; j < npp; j++) {
                            SymDefTable[i].PulseLenghts[j] = readByteFile(tape) | (readByteFile(tape) << 8);
                            SymDefTable[i].PulseLenghts[j] *= tapeCompensation; // Apply TZX compensation
                            tapebufByteCount += 2;
                        }

                    }

                    // printf("-----------------------\n");
                    // printf("Pilot Sync Symbol Table\n");
                    // printf("-----------------------\n");
                    // for (int i = 0; i < asp; i++) {
                    //     printf("%d: %d; ",i,(int)SymDefTable[i].SymbolFlags);
                    //     for (int j = 0; j < npp; j++) {
                    //         printf("%d,",(int)SymDefTable[i].PulseLenghts[j]);
                    //     }
                    //     printf("\n");
                    // }
                    // printf("-----------------------\n");

                    // printf("\nPULSES (PILOT SYNC)\n");

                    // Read pulse data
                    GDBsymbol = readByteFile(tape); // Read Symbol to be represented from PRLE[0]

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

                    // printf("PULSE%d %d %d Flags: %d\n",tapeEarBit,tapeNext,tapeHdrPulses,(int)SymDefTable[GDBsymbol].SymbolFlags);

                    curGDBSymbol = 0;
                    curGDBPulse = 0;

                    tapebufByteCount += 3;

                    tapePhase=TAPE_PHASE_GDB_PILOTSYNC;

                } else if (totd > 0) {

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
                            SymDefTable[i].PulseLenghts[j] *= tapeCompensation; // Apply TZX compensation
                            tapebufByteCount += 2;
                        }

                    }

                    // printf("-- NO GDB PILOT !! ---\n");
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

                    curGDBSymbol = 0;
                    curGDBPulse = 0;
                    curBit = 7;

                    // Read data stream first symbol
                    GDBsymbol = 0;

                    tapeCurByte = readByteFile(tape);
                    tapebufByteCount += 1;

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

                }

                // Provisional GDB in development
                tapeBlockLen += tapeData + 4 + 1;
                tapebufByteCount += 1;

                return;

            case 0x20:

                // printf("TZX block: %d, ID 0x20 - Pause (silence) or 'Stop the Tape' command, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeBlkPauseLen = (readByteFile(tape) | (readByteFile(tape) << 8)) * 3500;

                if (tapeBlkPauseLen == 0) {

                    Tape::Stop();

                    if (tapeCurBlock < (tapeNumBlocks - 1)) {
                        tapeCurBlock++;
                    } else {
                        tapeCurBlock = 0;
                        tapeEarBit = TAPELOW;
                        rewind(tape);
                    }

                } else {

                    tapeBlkPauseLen *= tapeCompensation; // Apply TZX compensation

                    tapeBlockLen += 2 + 1;
                    tapebufByteCount += 2 + 1;

                    tapeEarBit = TAPELOW;
                    tapePhase=TAPE_PHASE_PAUSE;
                    tapeNext=tapeBlkPauseLen;

                    tapeCurByte = readByteFile(tape);

                }

                return;

            case 0x21:

                // printf("TZX block: %d, ID 0x21 - Group Start, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeData = readByteFile(tape);
                fseek(tape,tapeData,SEEK_CUR);
                tapeData += 0x01 + 1;

                tapeBlockLen += tapeData;
                tapebufByteCount += tapeData;

                break;

            case 0x22:

                // printf("TZX block: %d, ID 0x22 - Group End, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

                tapeBlockLen++;
                tapebufByteCount++;

                break;

            case 0x23:

                // printf("TZX block: %d, ID 0x23 - Jump to block, Tape position: %d\n",tapeCurBlock + 1, tapebufByteCount);

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

                fseek(tape,CalcTZXBlockPos(tapeCurBlock),SEEK_SET);

                tapeCurByte = readByteFile(tape);

                continue;

            case 0x24:

                // printf("TZX block: ID 0x24 - Loop Start\n");

                nLoops=readByteFile(tape) | (readByteFile(tape) << 8);
                loopStart = tapeCurBlock;
                loop_first = true;

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

                if (nLoops > 0) {
                    tapeCurBlock = loopStart;
                    fseek(tape,CalcTZXBlockPos(tapeCurBlock + 1),SEEK_SET);
                } else {
                    tapeBlockLen = loop_tapeBlockLen;
                    tapebufByteCount = loop_tapebufByteCount;
                }

                break;

            case 0x26: // Call Sequence

                // printf("TZX block: ID 0x26- Call sequence\n");

                if (callSeq == 0) {
                    callSeq = readByteFile(tape) | (readByteFile(tape) << 8);
                    tapeBlockLen += (callSeq << 1) + 1;
                    tapebufByteCount += (callSeq << 1) + 1;
                } else {
                    fseek(tape, (callSeq << 1) + 2 ,SEEK_CUR);
                }

                jumpDistance = readByteFile(tape) | (readByteFile(tape) << 8);

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

                fseek(tape,CalcTZXBlockPos(tapeCurBlock),SEEK_SET);

                tapeCurByte = readByteFile(tape);

                continue;

            case 0x27: // Return from Sequence

                // printf("TZX block: ID 0x27- Return from sequence\n");

                tapeCurBlock = callBlock;

                if (callSeq == 0) tapeCurBlock++;

                tapeBlockLen++;
                tapebufByteCount++;

                fseek(tape,CalcTZXBlockPos(tapeCurBlock),SEEK_SET);

                tapeCurByte = readByteFile(tape);

                continue;

            case 0x28: // Select Block

                // printf("TZX block: ID 0x28- Select Block\n");

                // NOTE: Selection dialog not implemented. Now, this just jumps to next block.

                tapeData = readByteFile(tape) | (readByteFile(tape) << 8);
                fseek(tape,tapeData,SEEK_CUR);
                tapeData += 0x02 + 1;

                tapeBlockLen += tapeData;
                tapebufByteCount += tapeData;

                break;

            case 0x2A:

                // printf("TZX block: ID 0x2A - Stop the tape if in 48K mode\n");

                fseek(tape,4,SEEK_CUR);

                if (Z80Ops::is48) {

                    Tape::Stop();

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

                // printf("TZX block: ID 0x2b - Set Signal Level\n");

                fseek(tape,4,SEEK_CUR);
                tapeEarBit = readByteFile(tape);

                tapeBlockLen += 5 + 1;
                tapebufByteCount += 5 + 1;

                break;

            case 0x30:

                // printf("TZX block: ID 0x30 - Text Description\n");

                tapeData = readByteFile(tape);
                fseek(tape,tapeData,SEEK_CUR);
                tapeData += 0x01 + 1;

                tapeBlockLen += tapeData;
                tapebufByteCount += tapeData;

                break;

            case 0x31:

                // printf("TZX block: ID 0x31 - Message block\n");

                fseek(tape,1,SEEK_CUR);
                tapeData = readByteFile(tape);
                fseek(tape,tapeData,SEEK_CUR);
                tapeData += 0x02 + 1;

                tapeBlockLen += tapeData;
                tapebufByteCount += tapeData;

                break;

            case 0x32:

                // printf("TZX block: ID 0x32 - Archive info\n");

                tapeData=readByteFile(tape) | (readByteFile(tape) << 8);
                fseek(tape,tapeData,SEEK_CUR);
                tapeData += 0x02 + 1;

                tapeBlockLen += tapeData;
                tapebufByteCount += tapeData;

                break;

            case 0x33:

                // printf("TZX block: ID 0x33 - Hardware type\n");

                tapeData = readByteFile(tape) * 3;
                fseek(tape,tapeData,SEEK_CUR);
                tapeData += 0x01 + 1;

                tapeBlockLen += tapeData;
                tapebufByteCount += tapeData;

                break;

            case 0x35:

                // printf("TZX block: ID 0x35 - Custom info\n");

                fseek(tape,0x10,SEEK_CUR);
                tapeData = readByteFile(tape) | (readByteFile(tape) << 8) | (readByteFile(tape) << 16) | (readByteFile(tape) << 24);
                fseek(tape,tapeData,SEEK_CUR);
                tapeData += 0x14 + 1;

                tapeBlockLen += tapeData;
                tapebufByteCount += tapeData;

                break;

            case 0x5a:

                // printf("TZX block: ID 0x5a - Glue Block\n");
                fseek(tape,0x9,SEEK_CUR);

                tapeBlockLen += 9 + 1;
                tapebufByteCount += 9 + 1;

                break;

        }

        tapeCurBlock++;
        tapeCurByte = readByteFile(tape);

    }

}
