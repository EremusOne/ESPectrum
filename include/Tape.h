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

#ifndef Tape_h
#define Tape_h

#include <inttypes.h>
#include <vector>
#include <string>

using namespace std;

// Tape status definitions
#define TAPE_STOPPED 0
#define TAPE_LOADING 1
#define TAPE_PAUSED 2

// Saving status
#define SAVE_STOPPED 0
#define TAPE_SAVING 1

// Tape phases
#define TAPE_PHASE_SYNC 1
#define TAPE_PHASE_SYNC1 2
#define TAPE_PHASE_SYNC2 3
#define TAPE_PHASE_DATA 4
#define TAPE_PHASE_PAUSE 5

// Tape sync phases lenght in microseconds
#define TAPE_SYNC_LEN 2168 // 620 microseconds for 2168 tStates (48K)
#define TAPE_SYNC1_LEN 667 // 190 microseconds for 667 tStates (48K)
#define TAPE_SYNC2_LEN 735 // 210 microseconds for 735 tStates (48K)

#define TAPE_HDR_LONG 8063   // Header sync lenght in pulses
#define TAPE_HDR_SHORT 3223  // Data sync lenght in pulses

#define TAPE_BIT0_PULSELEN 855 // tstates = 244 ms, lenght of pulse for bit 0
#define TAPE_BIT1_PULSELEN 1710 // tstates = 488 ms, lenght of pulse for bit 1

//#define TAPE_BLK_PAUSELEN 3500000UL // 1 second of pause between blocks
#define TAPE_BLK_PAUSELEN 1750000UL // 1/2 second of pause between blocks
//#define TAPE_BLK_PAUSELEN 875000UL // 1/4 second of pause between blocks

class TapeBlock
{
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

class Tape
{
public:

    // Tape
    static FILE *tape;
    static string tapeFileName;
    static string tapeSaveName;
    static uint8_t tapeEarBit;
    static uint8_t tapeStatus;
    static uint8_t SaveStatus;
    static uint8_t romLoading;
    static uint16_t tapeCurBlock;  
    static uint16_t tapeNumBlocks;  
    static uint32_t tapebufByteCount;
    static uint32_t tapePlayOffset;    
    static size_t tapeFileSize;
 
    static std::vector<TapeBlock> TapeListing;

    static void Init();
    static void Open(string name);
    static void TAP_Play();
    static void TAP_Stop();    
    static void TAP_Read();
    static bool FlashLoad();
    static void Save();
    static uint32_t CalcTapBlockPos(int block);

};

#endif
