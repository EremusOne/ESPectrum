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

#include "hardpins.h"
#include "FileUtils.h"
#include "CPU.h"
#include "Tape.h"
#include "Ports.h"
#include "OSDMain.h"
#include "messages.h"
#include "Z80_JLS/z80.h"

#define TAPE_LISTING_DIV 16

FILE *Tape::tape;
string Tape::tapeFileName = "none";
string Tape::tapeSaveName = "none";
uint8_t Tape::tapeStatus = TAPE_STOPPED;
uint8_t Tape::SaveStatus = SAVE_STOPPED;
uint8_t Tape::romLoading = false;
uint8_t Tape::tapeEarBit;
std::vector<TapeBlock> Tape::TapeListing;
uint16_t Tape::tapeCurBlock;
uint16_t Tape::tapeNumBlocks;
uint32_t Tape::tapebufByteCount;
uint32_t Tape::tapePlayOffset;
size_t Tape::tapeFileSize;

static uint8_t tapeCurByte;
static uint8_t tapePhase;
static uint64_t tapeStart;
static uint32_t tapePulseCount;
static uint16_t tapeBitPulseLen;   
static uint8_t tapeBitPulseCount;     
static uint16_t tapeHdrPulses;
static uint32_t tapeBlockLen;
static uint8_t tapeBitMask;

// static uint8_t tapeReadBuf[4096] = { 0 };

void Tape::Init()
{
    tape = NULL;
}

void Tape::Open(string name) {
   
    if (tape != NULL) {
        fclose(tape);
        tape = NULL;
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
                // block.FileName[i] = readByteFile(file);
                uint8_t tst = readByteFile(tape);
            }
            // block.FileName[10]='\0';

            fseek(tape,6,SEEK_CUR);

            // Get the checksum.
            uint8_t checksum = readByteFile(tape);
        
            // block.Type = dataBlockType;
            // block.Index = tapeListIndex;
            // block.IsHeaderless = false;
            // block.Checksum = checksum;
            // block.BlockLength = 19;
            // block.BlockTypeNum = 0;
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

            // // If the preceeding block is a data block, this is a headerless block.
            // bool isHeaderless;
            // if (tapeListIndex > 0 && TapeListing[tapeListIndex - 1].Type == TapeBlock::BlockType::Data_block) {
            //     isHeaderless = true;
            // } else {
            //     isHeaderless = false;
            // }

            fseek(tape,contentLength,SEEK_CUR);

            // Get the checksum.
            uint8_t checksum = readByteFile(tape);

            // block.FileName[0]='\0';
            // block.Type = TapeBlock::BlockType::Data_block;
            // block.Index = tapeListIndex;
            // block.IsHeaderless = isHeaderless;
            // block.Checksum = checksum;
            // block.BlockLength = tapeBlkLen;
            // block.BlockTypeNum = flagByte;
            if ((tapeListIndex & (TAPE_LISTING_DIV - 1)) == 0) {
                block.StartPosition = tapeContentIndex;
                TapeListing.push_back(block);
            }

        }

        tapeListIndex++;
        
        tapeContentIndex += tapeBlkLen + 2;

    } while(tapeContentIndex < tapeFileSize);
    
    // } while((tapeListIndex < 1000) && (tapeContentIndex < tapeFileSize));    

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

// void Tape::readBlockData(int BlockIndex) {

//     // Analyze .tap file
//     tapeBlockLen=(readByteFile(tape) | (readByteFile(tape) << 8));

//     // printf("Analyzing block %d\n",tapeListIndex);
//     // printf("    Block Len: %d\n",tapeBlockLen - 2);        

//     // Read the flag byte from the block.
//     // If the last block is a fragmented data block, there is no flag byte, so set the flag to 255
//     // to indicate a data block.
//     uint8_t flagByte;
//     if (tapeContentIndex + 2 < tapeFileSize) {
//         flagByte = readByteFile(file);
//     } else {
//         flagByte = 255;
//     }

//     // Process the block depending on if it is a header or a data block.
//     // Block type 0 should be a header block, but it happens that headerless blocks also
//     // have block type 0, so we need to check the block length as well.
//     if (flagByte == 0 && tapeBlkLen == 19) { // This is a header.

//         // Get the block type.
//         TapeBlock::BlockType dataBlockType;
//         uint8_t blocktype = readByteFile(file);
//         switch (blocktype)
//         {
//             case 0:
//                 dataBlockType = TapeBlock::BlockType::Program_header;
//                 break;
//             case 1:
//                 dataBlockType = TapeBlock::BlockType::Number_array_header;
//                 break;
//             case 2:
//                 dataBlockType = TapeBlock::BlockType::Character_array_header;
//                 break;
//             case 3:
//                 dataBlockType = TapeBlock::BlockType::Code_header;
//                 break;
//             default:
//                 dataBlockType = TapeBlock::BlockType::Unassigned;
//                 break;
//         }

//         // Get the filename.
//         for (int i = 0; i < 10; i++) {
//             // block.FileName[i] = readByteFile(file);
//             uint8_t tst = readByteFile(file);
//         }
//         // block.FileName[10]='\0';

//         fseek(file,6,SEEK_CUR);

//         // Get the checksum.
//         uint8_t checksum = readByteFile(file);
    
//         // block.Type = dataBlockType;
//         // block.Index = tapeListIndex;
//         // block.IsHeaderless = false;
//         // block.Checksum = checksum;
//         // block.BlockLength = 19;
//         // block.BlockTypeNum = 0;
//         if ((tapeListIndex & 0x07) == 0) {
//             block.StartPosition = tapeContentIndex;
//             TapeListing.push_back(block);
//         }

//     } else {

//         // Get the block content length.
//         int contentLength;
//         int contentOffset;
//         if (tapeBlkLen >= 2) {
//             // Normally the content length equals the block length minus two
//             // (the flag byte and the checksum are not included in the content).
//             contentLength = tapeBlkLen - 2;
//             // The content is found at an offset of 3 (two byte block size + one flag byte).
//             contentOffset = 3;
//         } else {
//             // Fragmented data doesn't have a flag byte or a checksum.
//             contentLength = tapeBlkLen;
//             // The content is found at an offset of 2 (two byte block size).
//             contentOffset = 2;
//         }

//         // // If the preceeding block is a data block, this is a headerless block.
//         // bool isHeaderless;
//         // if (tapeListIndex > 0 && TapeListing[tapeListIndex - 1].Type == TapeBlock::BlockType::Data_block) {
//         //     isHeaderless = true;
//         // } else {
//         //     isHeaderless = false;
//         // }

//         fseek(file,contentLength,SEEK_CUR);

//         // Get the checksum.
//         uint8_t checksum = readByteFile(file);

//         // block.FileName[0]='\0';
//         // block.Type = TapeBlock::BlockType::Data_block;
//         // block.Index = tapeListIndex;
//         // block.IsHeaderless = isHeaderless;
//         // block.Checksum = checksum;
//         // block.BlockLength = tapeBlkLen;
//         // block.BlockTypeNum = flagByte;
//         if ((tapeListIndex & 0x07) == 0) {
//             block.StartPosition = tapeContentIndex;
//             TapeListing.push_back(block);
//         }

//     }

// }


void Tape::TAP_Play()
{
   
    // TO DO: Use ram buffer (tapeReadBuf) to speed up loading
   
    switch (Tape::tapeStatus) {
    case TAPE_STOPPED:

        // tape = fopen(Tape::tapeFileName.c_str(), "rb");
        if (tape == NULL)
        {
            OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
            return;
        }

        // fseek(tape,0,SEEK_END);
        // tapeFileSize = ftell(tape);
        // rewind (tape);

        // Move to selected block position
        // fseek(tape,TapeListing[Tape::tapeCurBlock].StartPosition,SEEK_SET);
        // tapePlayOffset = TapeListing[Tape::tapeCurBlock].StartPosition;
        tapePlayOffset = CalcTapBlockPos(tapeCurBlock);

        tapePhase=TAPE_PHASE_SYNC;
        tapePulseCount=0;
        tapeEarBit=0;
        tapeBitMask=0x80;
        tapeBitPulseCount=0;
        tapeBitPulseLen=TAPE_BIT0_PULSELEN;
        tapeBlockLen=(readByteFile(tape) | (readByteFile(tape) <<8)) + 2;
        tapeCurByte = readByteFile(tape);
        if (tapeCurByte) tapeHdrPulses=TAPE_HDR_SHORT; else tapeHdrPulses=TAPE_HDR_LONG;
        tapebufByteCount=2;
        tapeStart=CPU::global_tstates + CPU::tstates;
        Tape::tapeStatus=TAPE_LOADING;
        break;

    case TAPE_LOADING:
        TAP_Stop();
        break;

    // case TAPE_PAUSED:
    //     tapeStart=CPU::global_tstates + CPU::tstates;        
    //     Tape::tapeStatus=TAPE_LOADING;
    }
}

void Tape::TAP_Stop()
{
    tapeStatus=TAPE_STOPPED;
    // fclose(tape);
    // tape = NULL;
}

IRAM_ATTR void Tape::TAP_Read()
{
    uint64_t tapeCurrent = (CPU::global_tstates + CPU::tstates) - tapeStart;
    
    switch (tapePhase) {
    case TAPE_PHASE_SYNC:
        if (tapeCurrent > TAPE_SYNC_LEN) {
            tapeStart=CPU::global_tstates + CPU::tstates;
            tapeEarBit ^= 1;
            tapePulseCount++;
            if (tapePulseCount>tapeHdrPulses) {
                tapePulseCount=0;
                tapePhase=TAPE_PHASE_SYNC1;
            }
        }
        break;
    case TAPE_PHASE_SYNC1:
        if (tapeCurrent > TAPE_SYNC1_LEN) {
            tapeStart=CPU::global_tstates + CPU::tstates;
            tapeEarBit ^= 1;
            tapePhase=TAPE_PHASE_SYNC2;
        }
        break;
    case TAPE_PHASE_SYNC2:
        if (tapeCurrent > TAPE_SYNC2_LEN) {
            tapeStart=CPU::global_tstates + CPU::tstates;
            tapeEarBit ^= 1;
            if (tapeCurByte & tapeBitMask) tapeBitPulseLen=TAPE_BIT1_PULSELEN; else tapeBitPulseLen=TAPE_BIT0_PULSELEN;            
            tapePhase=TAPE_PHASE_DATA;
        }
        break;
    case TAPE_PHASE_DATA:
        if (tapeCurrent > tapeBitPulseLen) {
            tapeStart=CPU::global_tstates + CPU::tstates;
            tapeEarBit ^= 1;
            tapeBitPulseCount++;
            if (tapeBitPulseCount==2) {
                tapeBitPulseCount=0;
                tapeBitMask = (tapeBitMask >>1 | tapeBitMask <<7);
                if (tapeBitMask==0x80) {
                    tapeCurByte = readByteFile(tape);
                    tapebufByteCount++;
                    if (tapebufByteCount == tapeBlockLen) {
                        tapePhase=TAPE_PHASE_PAUSE;
                        tapeCurBlock++;
                        tapeEarBit=0;
                        break;
                    }
                }
                if (tapeCurByte & tapeBitMask) tapeBitPulseLen=TAPE_BIT1_PULSELEN; else tapeBitPulseLen=TAPE_BIT0_PULSELEN;
            }
        }
        break;
    case TAPE_PHASE_PAUSE:
        if ((tapebufByteCount + tapePlayOffset) < tapeFileSize) {
            if (tapeCurrent > TAPE_BLK_PAUSELEN) {
                tapeStart=CPU::global_tstates + CPU::tstates;
                tapePulseCount=0;
                tapePhase=TAPE_PHASE_SYNC;
                tapeBlockLen+=(tapeCurByte | readByteFile(tape) <<8)+ 2;
                tapebufByteCount+=2;
                tapeCurByte=readByteFile(tape);
                if (tapeCurByte) tapeHdrPulses=TAPE_HDR_SHORT; else tapeHdrPulses=TAPE_HDR_LONG;
            }
        } else {
            tapeCurBlock=0;
            TAP_Stop();
        }
    } 

}

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
