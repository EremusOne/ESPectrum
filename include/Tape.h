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

#ifndef Tape_h
#define Tape_h

#include <inttypes.h>
#include <vector>
#include <string>

using namespace std;

// Tape file types
#define TAPE_FTYPE_EMPTY 0
#define TAPE_FTYPE_TAP 1
#define TAPE_FTYPE_TZX 2

// Tape status definitions
#define TAPE_STOPPED 0
#define TAPE_LOADING 1

// Saving status
#define SAVE_STOPPED 0
#define TAPE_SAVING 1

// Tape phases
#define TAPE_PHASE_STOPPED 0
#define TAPE_PHASE_SYNC 1
#define TAPE_PHASE_SYNC1 2
#define TAPE_PHASE_SYNC2 3
#define TAPE_PHASE_DRB 4
#define TAPE_PHASE_PAUSE 5
#define TAPE_PHASE_TAIL 6
#define TAPE_PHASE_DATA1 7
#define TAPE_PHASE_DATA2 8
#define TAPE_PHASE_PURETONE 9
#define TAPE_PHASE_PULSESEQ 10

// Tape sync phases lenght in microseconds
#define TAPE_SYNC_LEN 2168 // 620 microseconds for 2168 tStates (48K)
#define TAPE_SYNC1_LEN 667 // 190 microseconds for 667 tStates (48K)
#define TAPE_SYNC2_LEN 735 // 210 microseconds for 735 tStates (48K)

#define TAPE_HDR_LONG 8063   // Header sync lenght in pulses
#define TAPE_HDR_SHORT 3223  // Data sync lenght in pulses

#define TAPE_BIT0_PULSELEN 855 // tstates = 244 ms, lenght of pulse for bit 0
#define TAPE_BIT1_PULSELEN 1710 // tstates = 488 ms, lenght of pulse for bit 1

#define TAPE_PHASE_TAIL_LEN 3500

#define TAPE_BLK_PAUSELEN 3500000UL // 1000 ms. of pause between blocks

// Tape sync phases lenght in microseconds for Rodolfo Guerra ROMs
#define TAPE_SYNC_LEN_RG 1408 // 620 microseconds for 2168 tStates (48K)
#define TAPE_SYNC1_LEN_RG 397 // 190 microseconds for 667 tStates (48K)
#define TAPE_SYNC2_LEN_RG 317 // 210 microseconds for 735 tStates (48K)

#define TAPE_HDR_LONG_RG 4835   // Header sync lenght in pulses
#define TAPE_HDR_SHORT_RG 1930  // Data sync lenght in pulses

#define TAPE_BIT0_PULSELEN_RG 325 // tstates = 244 ms, lenght of pulse for bit 0
#define TAPE_BIT1_PULSELEN_RG 649 // tstates = 488 ms, lenght of pulse for bit 1

#define TAPE_BLK_PAUSELEN_RG 1113000UL // 318 ms.

#define TAPE_LISTING_DIV 16

struct TZXBlock
{
    uint8_t BlockType;   
    char FileName[11];
    uint16_t PauseLenght;
    uint32_t BlockLenght;
};

class TapeBlock {
public:
    enum BlockType {
        Program_header,
        Number_array_header,
        Character_array_header,
        Code_header,
        Data_block,
        Info,
        Unassigned
    };
    // uint8_t Index;          // Index (position) of the tape block in the tape file.
    // BlockType Type;         // Type of tape block (enum).
    // char FileName[11];      // File name in header block.
    // bool IsHeaderless;      // Set to true for data blocks without a header.
    // uint8_t Checksum;       // Header checksum byte.
    // uint8_t BlockTypeNum;   // Block type, 0x00 = header; 0xFF = data block.
    uint32_t StartPosition; // Start point of this block?
    // uint16_t BlockLength;
};

class Tape {
public:

    // Tape
    static FILE *tape;
    static string tapeFileName;
    static string tapeSaveName;
    static int tapeFileType;
    static uint8_t tapeEarBit;
    static uint8_t tapeStatus;
    static uint8_t SaveStatus;
    static uint8_t romLoading;
    static int tapeCurBlock;  
    static int tapeNumBlocks;  
    static uint32_t tapebufByteCount;
    static uint32_t tapePlayOffset;    
    static size_t tapeFileSize;
 
    static uint8_t tapePhase;    

    static std::vector<TapeBlock> TapeListing;

    static void Init();
    static void LoadTape(string mFile);
    static void Play();
    static void Stop();
    static void Read();
    static bool FlashLoad();
    static void Save();

    static uint32_t CalcTapBlockPos(int block);
    static uint32_t CalcTZXBlockPos(int block);    
    static string tapeBlockReadData(int Blocknum);
    static string tzxBlockReadData(int Blocknum);    

private:

    static void (*GetBlock)();

    static void TAP_Open(string name);
    static void TAP_GetBlock();    
    static void TZX_Open(string name);
    static void TZX_GetBlock();    
    static void TZX_BlockLen(TZXBlock &blockdata);

    // Tape timing values
    static uint16_t tapeSyncLen;
    static uint16_t tapeSync1Len;
    static uint16_t tapeSync2Len;
    static uint16_t tapeBit0PulseLen; // lenght of pulse for bit 0
    static uint16_t tapeBit1PulseLen; // lenght of pulse for bit 1
    static uint16_t tapeHdrLong;  // Header sync lenght in pulses
    static uint16_t tapeHdrShort; // Data sync lenght in pulses
    static uint32_t tapeBlkPauseLen; 
    static uint8_t tapeLastByteUsedBits;
    static uint8_t tapeEndBitMask;
    static uint32_t tapeNext;

    static uint8_t tapeCurByte;
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
    static bool loop_first;

    static uint16_t callSeq;
    static int callBlock;
    // short jumpDistance;

};

#endif
