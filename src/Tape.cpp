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

FILE *Tape::tape;
string Tape::tapeFileName = "none";
string Tape::tapeSaveName = "none";
uint8_t Tape::tapeStatus = TAPE_STOPPED;
uint8_t Tape::SaveStatus = SAVE_STOPPED;
uint8_t Tape::romLoading = false;
uint8_t Tape::tapeEarBit;
std::vector<TapeBlock> Tape::TapeListing;
uint16_t Tape::tapeCurBlock;
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
   
    FILE *file;

    file = fopen(name.c_str(), "rb");
    if (file == NULL) {
        OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
        return;
    }

    fseek(file,0,SEEK_END);
    long tSize = ftell(file);
    rewind(file);
    if (tSize == 0) return;
    
    tapeFileName = name;

    Tape::TapeListing.clear(); // Clear TapeListing vector
    std::vector<TapeBlock>().swap(TapeListing); // free memory

    int tapeListIndex=0;
    int tapeContentIndex=0;
    int tapeBlkLen=0;
    TapeBlock block;
    do {

        // Analyze .tap file
        tapeBlkLen=(readByteFile(file) | (readByteFile(file) << 8));

        // printf("Analyzing block %d\n",tapeListIndex);
        // printf("    Block Len: %d\n",tapeBlockLen - 2);        

        // Read the flag byte from the block.
        // If the last block is a fragmented data block, there is no flag byte, so set the flag to 255
        // to indicate a data block.
        uint8_t flagByte;
        if (tapeContentIndex + 2 < tSize) {
            flagByte = readByteFile(file);
        } else {
            flagByte = 255;
        }

        // Process the block depending on if it is a header or a data block.
        // Block type 0 should be a header block, but it happens that headerless blocks also
        // have block type 0, so we need to check the block length as well.
        if (flagByte == 0 && tapeBlkLen == 19) { // This is a header.

            // Get the block type.
            TapeBlock::BlockType dataBlockType;
            uint8_t blocktype = readByteFile(file);
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
                block.FileName[i] = readByteFile(file);
            }
            block.FileName[10]='\0';

            fseek(file,6,SEEK_CUR);

            // Get the checksum.
            uint8_t checksum = readByteFile(file);
        
            block.Type = dataBlockType;
            block.Index = tapeListIndex;
            block.IsHeaderless = false;
            block.Checksum = checksum;
            block.BlockLength = 19;
            block.BlockTypeNum = 0;
            block.StartPosition = tapeContentIndex;

            TapeListing.push_back(block);

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

            // If the preceeding block is a data block, this is a headerless block.
            bool isHeaderless;
            if (tapeListIndex > 0 && TapeListing[tapeListIndex - 1].Type == TapeBlock::BlockType::Data_block) {
                isHeaderless = true;
            } else {
                isHeaderless = false;
            }

            fseek(file,contentLength,SEEK_CUR);

            // Get the checksum.
            uint8_t checksum = readByteFile(file);

            block.FileName[0]='\0';
            block.Type = TapeBlock::BlockType::Data_block;
            block.Index = tapeListIndex;
            block.IsHeaderless = isHeaderless;
            block.Checksum = checksum;
            block.BlockLength = tapeBlkLen;
            block.BlockTypeNum = flagByte;
            block.StartPosition = tapeContentIndex;

            TapeListing.push_back(block);

        }

        tapeListIndex++;
        
        tapeContentIndex += tapeBlkLen + 2;

    } while(tapeContentIndex < tSize);

    tapeCurBlock = 0;

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

    fclose(file);

}

void Tape::TAP_Play()
{
   
    // TO DO: Use ram buffer (tapeReadBuf) to speed up loading
   
    switch (Tape::tapeStatus) {
    case TAPE_STOPPED:

        tape = fopen(Tape::tapeFileName.c_str(), "rb");
        if (tape == NULL)
        {
            OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
            return;
        }
        fseek(tape,0,SEEK_END);
        tapeFileSize = ftell(tape);
        rewind (tape);

        // Move to selected block position
        fseek(tape,TapeListing[Tape::tapeCurBlock].StartPosition,SEEK_SET);
        tapePlayOffset = TapeListing[Tape::tapeCurBlock].StartPosition;

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
    fclose(tape);
    tape = NULL;
}

void IRAM_ATTR Tape::TAP_Read()
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

    // if (tapeSaveName == "none") {
    //     OSD::osdCenteredMsg(OSD_TAPE_SAVE_ERR, LEVEL_ERROR);
    //     return;
    // }

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

    FILE *flashtape;

    flashtape = fopen(Tape::tapeFileName.c_str(), "rb");
    if (flashtape == NULL) {
        Z80::setCarryFlag(false);
        return true;
    }

    // Move to selected block position
    fseek(flashtape,TapeListing[Tape::tapeCurBlock].StartPosition,SEEK_SET);

    uint16_t blockLen=(readByteFile(flashtape) | (readByteFile(flashtape) <<8));
    uint8_t tapeFlag = readByteFile(flashtape);

    // printf("BlockLen: %d. Flag: %d\n",blockLen,tapeFlag);

    // printf("%u\n",Z80::getRegA());
    // printf("%u\n",Z80::getRegAx());    

    if (Z80::getRegAx() != (tapeFlag & 0xff)) {
        // printf("No coincide el flag\n");
        fclose(flashtape);
        Z80::setFlags(0x00);
        Z80::setRegA(Z80::getRegAx() ^ tapeFlag);
        if (tapeCurBlock < (TapeListing.size() - 1)) {
            tapeCurBlock++;
            return true;
        } else {
            tapeCurBlock = 0;
            return false;
        }
    }

    // La paridad incluye el byte de flag
    Z80::setRegA(tapeFlag);

    int count = 0;
    uint8_t data;
    int addr = Z80::getRegIX();    // Address start
    int nBytes = Z80::getRegDE();  // Lenght
    // printf("Addr: %d. nBytes: %d\n",addr,nBytes);
    while ((count < nBytes) && (count < blockLen - 1)) {
        data = readByteFile(flashtape);
        MemESP::writebyte(addr,data);
        Z80::Xor(data);
        addr = (addr + 1) & 0xffff;
        count++;
    }

    // printf("Count: %d. nBytes: %d\n",count,nBytes);

    // Se cargarán los bytes pedidos en DE
    if (count == nBytes) {
        Z80::Xor(readByteFile(flashtape)); // Byte de paridad
        Z80::Cp(0x01);
    } else if (count < nBytes) {
        // Hay menos bytes en la cinta de los indicados en DE
        // En ese caso habrá dado un error de timeout en LD-SAMPLE (0x05ED)
        // que se señaliza con CARRY==reset & ZERO==set
        Z80::setFlags(0x50); // when B==0xFF, then INC B, B=0x00, F=0x50
    }

    Z80::setRegIX(addr);
    Z80::setRegDE(nBytes - count);

    if (tapeCurBlock < (TapeListing.size() - 1)) tapeCurBlock++; else tapeCurBlock = 0;

    fclose(flashtape);

    return true;

}

// bool Tape::FlashLoad2() {

// //             // The return value, representing the index of the TapeBlock which was flash loaded.
// //             int lastBlockIndex = -1;

// //             // If we encounter a block with no data to load (for example a text block), just skip ahead.
// //             if (tapeManager.NextBlock != null && tapeManager.NextBlock.BlockContent == null)
// //             {
// //                 return lastBlockIndex;
// //             }

// //             // The LD BYTES ROM routine is intercepted at an early stage just before the edge detection is started.
// //             // Check that the tape position is at the end of a block and that there is a following block to flash load.
// //             if (z80.PC == 0x056A && tapeManager.NextBlock != null && tapeManager.CurrentTapePosition > tapeManager.NextBlock.StartPosition - 10)
// //             {

// //                 // The target address for the data is stored in IX.
// //                 int dataTargetParameter = 256 * z80.I1 + z80.X;
// //                 // The block length (number of bytes) is stored in DE.
// //                 int dataLengthParameter = 256 * z80.D + z80.E;
// //                 // The flag byte is stored in A'.
// //                 int flagByte = z80.APrime;

//                 int dataTargetParameter = Z80::getRegIX();
//                 int dataLengthParameter = Z80::getRegDE();
//                 int flagByte = Z80::getRegA();

//                 printf("dataTargetParameter: %04X\n",dataTargetParameter);
//                 printf("dataLengthParameter: %04X\n",dataLengthParameter);
//                 printf("flagByte: %02X\n",flagByte);


// //                 // Check for various errors:
// //                 // Is there a mismatch between the block type and the flag byte in the A' register?
// //                 if (tapeManager.NextBlock.BlockTypeNum != flagByte && dataLengthParameter > 0)
// //                 {
// //                     // Don't load the block, but reset all flags.
// //                     z80.CarryFlag = 0;
// //                     z80.SignFlag = 0;
// //                     z80.ZeroFlag = 0;
// //                     z80.HalfCarryFlag = 0;
// //                     z80.Parity_OverflowFlag = 0;
// //                     z80.SubtractFlag = 0;
// //                     z80.F3Flag = 0;
// //                     z80.F5Flag = 0;

// //                     // The A register is updated by XOR:ing the flag byte with the block byte read from the file.
// //                     z80.A = flagByte ^ tapeManager.NextBlock.BlockTypeNum;
// //                 }

//                 if (Tape::TapeListing[tapeCurBlock].BlockTypeNum != flagByte && dataLengthParameter > 0) {
//                     Z80::setFlags(0x00);
//                     Z80::setRegA(flagByte ^ Tape::TapeListing[tapeCurBlock].BlockTypeNum);
//                 }

// //                 else
// //                 // Is the expected number of bytes larger than the actual length of the block?
// //                 if (dataLengthParameter > tapeManager.NextBlock.BlockContent.Length)
// //                 {
// //                     // If the DE register indicates a too long data length, the loader will fail after
// //                     // loading the block and it expects one more byte.
// //                     memory.WriteDataBlock(tapeManager.NextBlock.BlockContent, dataTargetParameter);

// //                     // When a new edge is not found, set flags carry = 0 and zero = 1.
// //                     z80.CarryFlag = 0;
// //                     z80.ZeroFlag = 1;

// //                     // The other flags are set by the last INC B (from 0xFF) at 0x05ED.
// //                     z80.HalfCarryFlag = 1;
// //                     z80.SignFlag = 0;
// //                     z80.Parity_OverflowFlag = 0;
// //                     z80.SubtractFlag = 0;
// //                     z80.F3Flag = 0;
// //                     z80.F5Flag = 0;

// //                     // Check that we're not dealing with a data fragment (in which case IX and DE are intact).
// //                     if (tapeManager.NextBlock.BlockContent.Length >= 2)
// //                     {
// //                         z80.I1 = (dataTargetParameter + tapeManager.NextBlock.BlockContent.Length + 1) / 256;
// //                         z80.X = (dataTargetParameter + tapeManager.NextBlock.BlockContent.Length + 1) - 256 * z80.I1;
// //                         z80.D = (dataLengthParameter - (tapeManager.NextBlock.BlockContent.Length + 1)) / 256;
// //                         z80.E = (dataLengthParameter - (tapeManager.NextBlock.BlockContent.Length + 1)) - 256 * z80.D;
// //                     }

// //                     z80.A = 0;
// //                 }

//                 else if (dataLengthParameter > Tape::TapeListing[tapeCurBlock].BlockLength) {

//                 }

//                 // else

//                 // Is the expected number of bytes smaller than the length of the block?
//                 // if (dataLengthParameter < tapeManager.NextBlock.BlockContent.Length)
// //                 {
// //                     memory.WriteDataBlock(tapeManager.NextBlock.BlockContent, dataTargetParameter);

// //                     // When calculating the checksum for the loaded data, there are two different cases,
// //                     // either the block length parameter equals zero, in which case there is no parity
// //                     // calculated and the checksum contains the flag byte.
// //                     // Otherwise, the checksum is calculated in the usual way but only for the number
// //                     // of bytes specified in the data length parameter + 1.
// //                     int calculatedCheckSum;
// //                     if (dataLengthParameter == 0)
// //                     {
// //                         calculatedCheckSum = flagByte;
// //                     }
// //                     else
// //                     {
// //                         calculatedCheckSum = flagByte;

// //                         // The checksum is calculated by XOR:ing each byte of data with the flag byte.
// //                         for (int i = 0; i < dataLengthParameter + 1; i++)
// //                         {
// //                             calculatedCheckSum ^= tapeManager.NextBlock.BlockContent[i];
// //                         }
// //                     }

// //                     // Update the flags and the A register.
// //                     UpdateFlags(calculatedCheckSum);

// //                     // Update the IX and DE registers.
// //                     z80.I1 = (dataTargetParameter + dataLengthParameter) / 256;
// //                     z80.X = (dataTargetParameter + dataLengthParameter) - 256 * z80.I1;
// //                     z80.D = 0;
// //                     z80.E = 0;
// //                 }
// //                 else

// //                 // Is the expected number of bytes equal to zero?
// //                 if (dataLengthParameter == 0)
// //                 {
// //                     // There is no parity check, so the checksum contains the block type value.
// //                     int calculatedCheckSum = 0xFF;

// //                     // Update the flags and the A register.
// //                     UpdateFlags(calculatedCheckSum);
// //                 }
//                  else
// //                 // Flash load the block and update IX, DE and AF.
//                  {
// //                     int lastBytePos = memory.WriteDataBlock(tapeManager.NextBlock.BlockContent, dataTargetParameter);

//                         tape = fopen(Tape::tapeFileName.c_str(), "rb");
//                         if (tape == NULL)
//                         {
//                             OSD::osdCenteredMsg(OSD_TAPE_LOAD_ERR, LEVEL_ERROR);
//                             return false;
//                         }
    
//                         // Move to selected block position
//                         fseek(tape,TapeListing[Tape::tapeCurBlock].StartPosition + 3,SEEK_SET);

//                         int count = 0;
//                         uint8_t data;
//                         printf("Addr: %d. nBytes: %d\n",dataTargetParameter,dataLengthParameter);
//                         while (count < dataLengthParameter) {
//                             data = readByteFile(tape);
//                             MemESP::writebyte(dataTargetParameter,data);
//                             Z80::Xor(data);                            
//                             dataTargetParameter = (dataTargetParameter + 1) & 0xffff;
//                             count++;
//                         }

//                         Z80::Xor(readByteFile(tape)); // Byte de paridad
//                         Z80::Cp(0x01);


//                         Z80::setRegIX(dataTargetParameter);
//                         Z80::setRegDE(0);
//                         tapeCurBlock++;

//                     fclose(tape);
//                     return true;

// //                     // After loading the entire block, including the last checksum byte, the current checksum will be 0.
// //                     // (the checksum being byte 0 XOR byte 1 XOR ... byte n).

// //                     // Update the flags and the A register.
// //                     UpdateFlags(calculatedCheckSum);


// //                     // Set IX to the same value as if the block had been loaded by the ROM routine.
// //                     z80.I1 = lastBytePos / 256;
// //                     z80.X = lastBytePos - 256 * z80.I1;

// //                     // Set DE to 0.
// //                     z80.D = 0;
// //                     z80.E = 0;
// //                 }

// //                 // Keep track of the index of the last loaded tape block. This information
// //                 // can be used to rewind the tape to the start position of the next block
// //                 // after an auto pause.
// //                 lastBlockIndex = tapeManager.NextBlock.Index;

// //                 // Skip forward to the end of the block which was just flash loaded into RAM.
// //                 tapeManager.GoToEndOfBlock(tapeManager.NextBlock.Index);

// //                 // Skip to the end of the LD BYTES ROM routine (actually a RET, so it doesn't really matter which RET instruction we point to here).
// //                 z80.PC = 0x05E2;

//                }

// //             return lastBlockIndex;

// //             // Update the flags after loading a block of data.
// //             void UpdateFlags(int checkSum)
// //             {
// //                 // The flags are set by the CP 0x01 operation at 0x05E0, where A = the current checksum.
// //                 int flagTest = checkSum - 1;
// //                 z80.CarryFlag = BitOps.GetBit(flagTest, 8);
// //                 z80.SignFlag = Flags.SignFlag(flagTest);
// //                 z80.ZeroFlag = Flags.ZeroFlag(flagTest);
// //                 z80.HalfCarryFlag = Flags.HalfCarryFlagSub8(checkSum, 1, z80.CarryFlag);
// //                 z80.Parity_OverflowFlag = Flags.OverflowFlagSub8(checkSum, 1, flagTest);
// //                 z80.SubtractFlag = 1;
// //                 z80.F3Flag = 0;
// //                 z80.F5Flag = 0;

// //                 z80.A = checkSum;
// //             }
// //         }
// //     }
// // }

// }

// // public boolean flashLoad(Memory memory) {

// //         if (idxHeader >= nOffsetBlocks) {
// //             // cpu.setCarryFlag(false);
// //             return false;
// //         }

// //         tapePos = offsetBlocks[idxHeader];
// //         blockLen = readInt(tapeBuffer, tapePos, 2);
// //         // System.out.println(String.format("tapePos: %X. blockLen: %X", tapePos, blockLen));
// //         tapePos += 2;

// //         // ¿Coincide el flag? (está en el registro A)
// //         if (cpu.getRegA() != (tapeBuffer[tapePos] & 0xff)) {
// //             cpu.xor(tapeBuffer[tapePos]);
// //             cpu.setCarryFlag(false);
// //             idxHeader++;
// //             return true;
// //         }

// //         // La paridad incluye el byte de flag
// //         cpu.setRegA(tapeBuffer[tapePos]);

// //         int count = 0;
// //         int addr = cpu.getRegIX();    // Address start
// //         int nBytes = cpu.getRegDE();  // Lenght
// //         while (count < nBytes && count < blockLen - 1) {
// //             memory.writeByte(addr, tapeBuffer[tapePos + count + 1]);
// //             cpu.xor(tapeBuffer[tapePos + count + 1]);
// //             addr = (addr + 1) & 0xffff;
// //             count++;
// //         }

// //         // Se cargarán los bytes pedidos en DE
// //         if (count == nBytes) {
// //             cpu.xor(tapeBuffer[tapePos + count + 1]); // Byte de paridad
// //             cpu.cp(0x01);
// //         }

// //         // Hay menos bytes en la cinta de los indicados en DE
// //         // En ese caso habrá dado un error de timeout en LD-SAMPLE (0x05ED)
// //         // que se señaliza con CARRY==reset & ZERO==set
// //         if (count < nBytes) {
// //             cpu.setFlags(0x50); // when B==0xFF, then INC B, B=0x00, F=0x50
// //         }

// //         cpu.setRegIX(addr);
// //         cpu.setRegDE(nBytes - count);
// //         idxHeader++;
// //         fireTapeBlockChanged(idxHeader);

// // //        System.out.println(String.format("Salida -> IX: %04X DE: %04X AF: %04X",
// // //            cpu.getRegIX(), cpu.getRegDE(), cpu.getRegAF()));
// //         return true;
// // }

// // private void fireTapeBlockChanged(final int block) {
// //         blockListeners.forEach(listener -> {
// //             listener.blockChanged(block);
// //         });
// // }

// //      saveTrap = settings.getTapeSettings().isEnableSaveTraps();
// //         z80.setBreakpoint(0x04D0, saveTrap);

// //         loadTrap = settings.getTapeSettings().isEnableLoadTraps();
// //         z80.setBreakpoint(0x0556, loadTrap);

// //         flashload = settings.getTapeSettings().isFlashLoad();


// //     case 0x04D0:
// //                 // SA_BYTES routine in Spectrum ROM at 0x04D0
// //                 // SA_BYTES starts at 0x04C2, but the +3 ROM don't enter
// //                 // to SA_BYTES by his start address.
// //                 if (saveTrap && memory.isSpectrumRom() && tape.isTapeReady()) {
// //                     if (tape.saveTapeBlock(memory)) {
// //                         return 0xC9; // RET opcode
// //                     }
// //                 }
// //                 break;
// //             case 0x0556:
// //                 // LD_BYTES routine in Spectrum ROM at address 0x0556
// //                 if (loadTrap && memory.isSpectrumRom() && tape.isTapeReady()) {
// //                     if (flashload && tape.flashLoad(memory)) {
// //                         invalidateScreen(true); // thanks Andrew Owen
// //                         return 0xC9; // RET opcode
// //                     } else {
// //                         tape.play(false);
// //                     }
// //                 }
// //                 break;
// //         }


// void Spectrum::trapLdStart() {

//     // ZF = 1 means treat the flag byte as a normal byte. This is
//     // indicated by setting the number of flag bytes to zero.
//     uint16_t flagByte = (z80.af_.b.l & FLAG_Z) ? 1 : 0;

//     // If either there are no flag bytes or the expected flag matches the
//     // block's flag, we signal flag ok. Expected flag is in A'.
//     bool flagOk = tape.foundTapBlock(z80.af_.b.h) || flagByte;

//     // CF = 1 means LOAD, CF = 0 means VERIFY.
//     bool verify = !(z80.af_.b.l & FLAG_C);

//     if (flagOk) {
//         // Get parameters from CPU registers
//         uint16_t address = z80.ix.w;
//         uint16_t bytes = z80.de.w;

//         uint16_t block = tape.getBlockLength() + flagByte - 1;  // Include parity
//         uint16_t offset = 3 - flagByte;
//         uint8_t parity = flagByte ? 0 : tape.getBlockByte(2);

//         if (verify) {
//             while (bytes && block) {
//                 uint8_t byte = tape.getBlockByte(offset++);
//                 uint8_t mem = readMemory(address++);
//                 block--;
//                 bytes--;
//                 parity ^= byte;
//                 if (byte != mem) break;
//             }
//         } else {
//             while (bytes && block) {
//                 uint8_t byte = tape.getBlockByte(offset++);
//                 writeMemory(address++, byte);
//                 block--;
//                 bytes--;
//                 parity ^= byte;
//             }
//         }

//         if (block) {
//             parity ^= tape.getBlockByte(offset);
//         }

//         if (!bytes && block && !parity) {
//             z80.af.b.l |= FLAG_C;
//         } else {
//             z80.af.b.l &= ~FLAG_C;
//             if (!block) z80.af.b.l |= FLAG_Z;
//         }

//         z80.hl.b.h = parity;
//         z80.ix.w = address;
//         z80.de.w = bytes;
//     }

//     // Advance tape
//     tape.nextTapBlock();

//     // Force RET
//     z80.decode(0xC9);
//     z80.startInstruction();

//     if (tape.tapPointer == 0) {
//         tape.rewind();
//     }
// }
