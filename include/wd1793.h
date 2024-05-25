/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

WD1793 EMULATION, adapted from Mark Woodmass SpecEmu's implementation

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

#pragma once

#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <cstring>

using namespace std;

#define MAX_TRACKS 83

#define LOW_RAM // Define if you need a smaller, less RAM consuming and a little bit slower Databuffer

// status register values
const unsigned char STATUS_BUSY = 1;
const unsigned char STATUS_DRQ = 2;
const unsigned char STATUS_INDEX = 2; // all Type 1 commands
const unsigned char STATUS_TRACK0 = 4;
const unsigned char STATUS_LOSTDATA = 4; // all Type 2+3 commands
const unsigned char STATUS_WRITEPROTECT = 64;

// system register values
const unsigned char SYSTEM_INTRQ = 128;
const unsigned char SYSTEM_DRQ = 64;

enum class WDCommandType { 
    WD_IDLE,
    WD_RESTORE,
    WD_SEEK,
    WD_READSECTOR,
    WD_READTRACK,
    WD_READADDRESS,
    WD_WRITESECTOR,
    WD_WRITETRACK,
    WD_FORCEINTERRUPT
};

enum class WDStatusType {
    TYPE_1,
    TYPE_2,
    TYPE_3,
    TYPE_4
};

class WDDrive {

    public:

        bool Available;
        unsigned char Cyls;
        unsigned char Heads;
        unsigned char Sectors;
        unsigned short SectorSize;
        FILE *DiskFile;
        bool IsSCLFile;
        int sclDataOffset;
        string FName;

};

class WD1793 {

    public:

        // 4 drive units
        WDDrive Drive[4];

        unsigned char TrackReg;
        unsigned char SectorReg;
        unsigned char CommandReg;
        unsigned char StatusReg;
        unsigned char DataReg;
        unsigned char SystemReg;

        bool DriveReady;
        bool HeadLoaded;
        unsigned char CurrentUnit;
        unsigned char CurrentHead;
        unsigned char CurrentSector;    // current sector the head is over
        WDCommandType CurrentCmd;
        WDStatusType StatusType;

        #ifdef LOW_RAM
            unsigned char DataBuffer[256];
        #else
            unsigned char DataBuffer[4096];
        #endif

        unsigned short DataPosn;
        unsigned short DataCount;
        unsigned short SectorCount;    // number of sectors being read/written by current FDC command
        bool OverRunTest;
        unsigned char OverRunCount;

        unsigned char lastCmd;
        unsigned char statusReadCount = 0;

        bool sclConverted;
        unsigned char Track0[2304]; // Store SCL translated track0

        void Init();
        void ShutDown();
        void EnterIdle();
        void EjectDisks();
        void EjectDisk(unsigned char UnitNum);
        bool InsertDisk(unsigned char UnitNum,  string Filename); 
        void SCLtoTRD(unsigned char* track0, unsigned char UnitNum);
        bool DiskInserted(unsigned char UnitNum);
        void SetDRQ();
        void ClearDRQ();
        void SetINTRQ();
        void ExecuteCommand(unsigned char wdCmd);
        
        #ifdef LOW_RAM
        void ReadRawOneSectorData(unsigned char UnitNum,  unsigned char C,  unsigned char H,  unsigned char S);
        void WriteRawOneSectorData(unsigned char UnitNum,  unsigned char C,  unsigned char H,  unsigned char S);        
        #else
        void ReadRawSectorData(unsigned char UnitNum,  unsigned char C,  unsigned char H,  unsigned char S,  unsigned char Count);
        void WriteRawSectorData(unsigned char UnitNum,  unsigned char C,  unsigned char H,  unsigned char S,  unsigned char Count);
        #endif

        unsigned char ReadSystemReg(); 
        unsigned char ReadStatusReg(); 
        unsigned char ReadTrackReg(); 
        unsigned char ReadSectorReg(); 
        unsigned char ReadDataReg(); 
        void WriteSystemReg(unsigned char newData);
        void WriteCommandReg(unsigned char wdCmd);
        void WriteTrackReg(unsigned char newTrack); 
        void WriteSectorReg(unsigned char newSector); 
        void WriteDataReg(unsigned char newData); 

};
