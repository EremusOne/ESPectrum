/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

WD1793 EMULATION, adapted from Mark Woodmass SpecEmu's implementation

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
#include <inttypes.h>
#include <string>
#include <cstring>

using namespace std;

#include "wd1793.h"

// #pragma GCC optimize("O3")

void WD1793::Init() {

    for( int i = 0 ; i < 4; i++ ) {

        if (Drive[i].DiskFile != NULL) {
            fclose(Drive[i].DiskFile);
            Drive[i].DiskFile = NULL;
        }

        Drive[i].IsSCLFile = false;
        Drive[i].FName.clear();

        sclConverted=false;

    }

    EnterIdle();

}

void WD1793::EnterIdle() {

  CurrentCmd = WDCommandType::WD_IDLE;
  StatusReg &= ~STATUS_BUSY;
  SystemReg &= ~SYSTEM_DRQ;
  DataPosn = 0;
  DataCount = 0;

}

void WD1793::SetDRQ() {
  StatusReg |= STATUS_DRQ;
  SystemReg |= SYSTEM_DRQ;
}

void WD1793::ClearDRQ() {
  StatusReg &= ~STATUS_DRQ;
  SystemReg &= ~SYSTEM_DRQ;
}

void WD1793::SetINTRQ() {
  SystemReg |= SYSTEM_INTRQ;
}

void WD1793::ExecuteCommand(unsigned char wdCmd) {

    SystemReg &= ~SYSTEM_INTRQ;

    if( (wdCmd & 0xF0) == 0xD0 ) {
        // Force Interrupt
        CurrentCmd = WDCommandType::WD_FORCEINTERRUPT;
        StatusType = WDStatusType::TYPE_4;
        if((wdCmd & 15) != 0 ) SetINTRQ();
        EnterIdle();

        // printf("    Command: Force Interrupt, Input value: %d\n",(int)wdCmd);

    }

    // if ((StatusReg & STATUS_BUSY) != 0) return;

    // set drive ready status bit
    StatusReg = ~((unsigned char)DriveReady) << 7;
    if(!DriveReady) {
        SetINTRQ();
        EnterIdle();
        return;
    }

    switch( ( wdCmd >> 5 ) & 7 ) {

        case 0: // Restore/Seek

            StatusType = WDStatusType::TYPE_1;
            if(  (wdCmd & 16) == 0 ) {
                // printf("    Command: Restore, Input value: %d\n",(int)wdCmd);                              
                CurrentCmd = WDCommandType::WD_RESTORE;
                TrackReg = 0;
            } else {
                // printf("    Command: Seek, Input value: %d\n",(int)wdCmd);                              
                CurrentCmd = WDCommandType::WD_SEEK;
                TrackReg = DataReg;
                if(TrackReg > (MAX_TRACKS - 1)) TrackReg = MAX_TRACKS - 1;
            }

            if(  TrackReg == 0 )
                StatusReg |= STATUS_TRACK0;
            else
                StatusReg &= ~STATUS_TRACK0;

            HeadLoaded = bool( ( wdCmd >> 3 ) & 1 );

            SetINTRQ();
            EnterIdle();

            break;

        case 1: // Step

            // printf("    Execute command: Step\n");
            StatusType = WDStatusType::TYPE_1;
            SetINTRQ();
            EnterIdle();
            break;

        case 2: // Step-In

            // printf("    Execute command: Step In\n");
            StatusType = WDStatusType::TYPE_1;
            SetINTRQ();
            EnterIdle();
            break;

        case 3: // Step-Out
            
            // printf("    Execute command: Step Out\n");
            StatusType = WDStatusType::TYPE_1;
            SetINTRQ();
            EnterIdle();
            break;

        case 4: // Read Sector(s)

            // printf("    Execute command: Read sector\n");

            CurrentCmd = WDCommandType::WD_READSECTOR;
            StatusType = WDStatusType::TYPE_2;
            if((wdCmd & 16) == 0)
                SectorCount = 1;
            else
                SectorCount = Drive[CurrentUnit].Sectors - SectorReg + 1;

            #ifdef LOW_RAM

            ReadRawOneSectorData(CurrentUnit, TrackReg, CurrentHead, SectorReg - 1);
            DataCount = Drive[CurrentUnit].SectorSize * SectorCount;

            #else

            // read raw sector data, setting relevant data transfer parameters
            ReadRawSectorData(CurrentUnit, TrackReg, CurrentHead, SectorReg - 1, SectorCount);

            #endif

            OverRunTest = true;
            OverRunCount = 64;
            StatusReg |= STATUS_BUSY;
            SetDRQ();
            break;

        case 5: // Write Sector(s)

            // printf("    Execute command: Write sector\n");

            CurrentCmd = WDCommandType::WD_WRITESECTOR;
            StatusType = WDStatusType::TYPE_2;

            if((wdCmd & 16) == 0 )
                SectorCount = 1;
            else
                SectorCount = Drive[CurrentUnit].Sectors - SectorReg + 1;

            // prepare write buffer data transfer parameters
            DataPosn = 0;
            DataCount = Drive[CurrentUnit].SectorSize * SectorCount;

            StatusReg |= STATUS_BUSY;
            SetDRQ();
            break;

        case 6: // Read Address

            // printf("    Execute command: Read address\n");

            CurrentCmd = WDCommandType::WD_READADDRESS;
            StatusType = WDStatusType::TYPE_3;
            HeadLoaded = true;
            // move to the next sector the head is over
            CurrentSector = (CurrentSector + 1) % Drive[CurrentUnit].Sectors;
            DataBuffer[0] = TrackReg;
            DataBuffer[1] = CurrentHead;
            DataBuffer[2] = CurrentSector;
            DataBuffer[3] = 1; // 256 bytes/sector
            DataBuffer[4] = 0; // crc1
            DataBuffer[5] = 0; // crc2
            DataPosn = 0;
            DataCount = 6;

            OverRunTest = true;
            OverRunCount = 64;
            SectorReg = TrackReg;
            StatusReg |= STATUS_BUSY;
            SetDRQ();
            break;

        case 7: // Read Track/Write Track

            StatusType = WDStatusType::TYPE_3;
            if((wdCmd & 16) == 0 ) {

                // printf("    Execute command: Read track\n");

                CurrentCmd = WDCommandType::WD_READTRACK;
                SetINTRQ();
                EnterIdle();

            } else {
                
                // printf("    Execute command: Write track\n");

                CurrentCmd = WDCommandType::WD_WRITETRACK;
                SetINTRQ();

                EnterIdle();

            }
            break;

    }

}

#ifdef LOW_RAM

// read one sector from CHS on specified unit into Drive's sector data buffer
void WD1793::ReadRawOneSectorData(unsigned char UnitNum,  unsigned char C,  unsigned char H,  unsigned char S) {

    if ((Drive[UnitNum].IsSCLFile) && (C == 0) && (H == 0)) {

        // Create track0 from SCL file if not already done
        if (!sclConverted) {
            // printf("Converting SCL track0 of drive %d\n",UnitNUm);
            SCLtoTRD(Track0,UnitNum);
            sclConverted = true;
        }

        // SCL disk -> Read sector to cache from created Track0
        if (S < 9) {
            int seekptr = S << 8;
            for (int i=0; i < 256; i++) DataBuffer[i] = Track0[seekptr + i];
            // printf("<WD1793 - ReadRawOneSectorData> Read SCL track0. S: %d\n", (int)S);
        } else {
            for (int i=0; i < 256; i++) DataBuffer[i] = 0;
            // printf("<WD1793 - ReadRawOneSectorData> Read and zero fill SCL track0. S: %d\n", (int)S);
        }

    } else {

        int seekptr = ( ( C * Drive[UnitNum].Heads + H ) * Drive[UnitNum].Sectors * Drive[UnitNum].SectorSize ) + ( S * Drive[UnitNum].SectorSize );
        // printf("SeekPtr no Offset: %d\n", seekptr);
        seekptr += Drive[UnitNum].sclDataOffset;
        // printf("SeekPtr Offset: %d\n",seekptr);        
        fseek(Drive[UnitNum].DiskFile, seekptr, SEEK_SET);
        fread(DataBuffer,1,Drive[UnitNum].SectorSize,Drive[UnitNum].DiskFile);

        // printf("<WD1793 - ReadRawOneSectorData> No SCL track0. Unit %d, seekptr %d\n", (int)UnitNum, seekptr);

    }

    DataPosn = 0;

}

// write one sector at CHS on specified unit from Drive's sector data buffer
void WD1793::WriteRawOneSectorData(unsigned char UnitNum,  unsigned char C,  unsigned char H,  unsigned char S) {

    if (!(Drive[UnitNum].IsSCLFile)) {
        int seekptr = ( ( C * Drive[UnitNum].Heads + H ) * Drive[UnitNum].Sectors * Drive[UnitNum].SectorSize ) + ( S * Drive[UnitNum].SectorSize );
        fseek(Drive[UnitNum].DiskFile,seekptr,SEEK_SET);
        fwrite(DataBuffer,1,Drive[UnitNum].SectorSize, Drive[UnitNum].DiskFile);
    }

}

#else

// read Count sectors from CHS on specified unit into Drive's sector data buffer
void WD1793::ReadRawSectorData(unsigned char UnitNum,  unsigned char C,  unsigned char H,  unsigned char S,  unsigned char Count) {

    if ((Drive[UnitNum].IsSCLFile) && (C == 0) && (H == 0)) {

        // Create track0 from SCL file if not already done
        if (!sclConverted) {
            // printf("Converting SCL track0 of drive %d\n",UnitNUm);
            SCLtoTRD(Track0,UnitNum);
            sclConverted = true;
        }

        // SCL disk -> Read sectors to cache from created Track0
        unsigned char n = 0;
        for(int n=0; n < Count; n++) {
            if (S < 9) {
                int seekptr = S << 8;
                for (int i=0; i < 256; i++) DataBuffer[(n << 8) + i] = Track0[seekptr + i];
            } else {
                for (int i=0; i < 256; i++) DataBuffer[(n << 8) + i] = 0;
            }
            S++;
        }

    } else {

        int seekptr = ( ( C * Drive[UnitNum].Heads + H ) * Drive[UnitNum].Sectors * Drive[UnitNum].SectorSize ) + ( S * Drive[UnitNum].SectorSize );
        seekptr += Drive[UnitNum].sclDataOffset;
        fseek(Drive[UnitNum].DiskFile, seekptr, SEEK_SET);
        fread(DataBuffer,1,Drive[UnitNum].SectorSize * Count,Drive[UnitNum].DiskFile);
        // printf("<WD1793 - ReadRawSectorData> seekptr %d, Count %d\n", seekptr, (int)Count);

    }

    DataPosn = 0;
    DataCount = Drive[ UnitNum ].SectorSize * Count;

}

// write Count sectors at CHS on specified unit from Drive's sector data buffer
void WD1793::WriteRawSectorData(unsigned char UnitNum,  unsigned char C,  unsigned char H,  unsigned char S,  unsigned char Count) {

    if (!(Drive[UnitNum].IsSCLFile)) {
        int seekptr = ( ( C * Drive[UnitNum].Heads + H ) * Drive[UnitNum].Sectors * Drive[UnitNum].SectorSize ) + ( S * Drive[UnitNum].SectorSize );
        fseek(Drive[UnitNum].DiskFile,seekptr,SEEK_SET);
        fwrite(DataBuffer,1,Drive[UnitNum].SectorSize * Count, Drive[UnitNum].DiskFile);
    }

}

#endif

void WD1793::ShutDown() {
  EjectDisks();
}

void WD1793::EjectDisks() {

    for( unsigned char i = 0 ; i < 4; i++ ) EjectDisk(i);

}

void WD1793::EjectDisk(unsigned char UnitNum) 
{

    if(Drive[UnitNum].Available) {

        if (Drive[UnitNum].DiskFile != NULL) {
            fclose(Drive[UnitNum].DiskFile);
            Drive[UnitNum].DiskFile = NULL;
        }
        Drive[UnitNum].Available = false;

        if (UnitNum == CurrentUnit) sclConverted = false;

    }

}

bool WD1793::InsertDisk(unsigned char UnitNum, string Filename) {       

    uint8_t diskType;   
    char magic[9];     

    // close any open disk in this unit
    EjectDisk(UnitNum);

    Drive[UnitNum].DiskFile = fopen(Filename.c_str(), "r+b");
    if (Drive[UnitNum].DiskFile != NULL) {

        Drive[UnitNum].Available = true;
        Drive[UnitNum].Heads = 1;
        Drive[UnitNum].Sectors = 16;
        Drive[UnitNum].SectorSize = 256;
        Drive[UnitNum].FName = Filename;

        fgets(magic,9, Drive[UnitNum].DiskFile);
        // printf("%s\n",magic);
        
        if (strcmp(magic,"SINCLAIR") == 0) {
            // SCL file
            // printf("SCL disk loaded\n");
            Drive[UnitNum].IsSCLFile=true;
            diskType = 0x16;
            // SCLtoTRD(Track0,UnitNum);

        } else {

            Drive[UnitNum].IsSCLFile=false;
            Drive[UnitNum].sclDataOffset = 0;

            fseek(Drive[UnitNum].DiskFile,2048 + 227,SEEK_SET);    
            fread(&diskType,1,1,Drive[UnitNum].DiskFile);

        }

        switch(diskType) {
            case 0x16:
                Drive[UnitNum].Cyls = 80;
                Drive[UnitNum].Heads = 2; //80 tracks, double sided
                break;
            case 0x17:
                Drive[UnitNum].Cyls = 40;
                Drive[UnitNum].Heads = 2; //40 tracks, double sided
                break;
            case 0x18:
                Drive[UnitNum].Cyls = 80;
                Drive[UnitNum].Heads = 1; //80 tracks, single sided
                break;
            case 0x19:
                Drive[UnitNum].Cyls = 40;
                Drive[UnitNum].Heads = 1; //40 tracks, single sided
                break;
            default:
                EjectDisk(UnitNum);
        }

        // printf("Disk inserted! Disktype: %d\n",(int) diskType);

    }

    return Drive[UnitNum].Available;

}

bool WD1793::DiskInserted(unsigned char UnitNum) {   
    return Drive[UnitNum].Available;
}

unsigned char WD1793::ReadStatusReg() {

    unsigned char result;

    if (StatusType == WDStatusType::TYPE_1)
        if(HeadLoaded && DriveReady) StatusReg ^= STATUS_INDEX;

    result = StatusReg;

    SystemReg &= ~SYSTEM_INTRQ;

    if(!DriveReady) result = 128; // no disk in drive

    // (Woodster) FIXME: toggle index mark pulse for Seek command
    if (lastCmd == 24) {
        statusReadCount++;
        if (statusReadCount % 32 == 0) {
            result ^= 2;
        }
    }

    return result;

}

unsigned char WD1793::ReadTrackReg() {

    unsigned char result;

    result = TrackReg;
    
    return result;

}

void WD1793::WriteTrackReg(unsigned char newTrack) {

    TrackReg = newTrack;

}

unsigned char WD1793::ReadSectorReg() {
    
    unsigned char result;
    result = SectorReg;
    return result;

}

void WD1793::WriteSectorReg(unsigned char newSector) {
    
    SectorReg = newSector;

}

unsigned char WD1793::ReadDataReg() {
    
    unsigned char result;

    result = DataReg;

    switch( CurrentCmd ) {

            case WDCommandType::WD_IDLE:

                result = DataReg;
                break;

            case WDCommandType::WD_READSECTOR:

                DataReg = DataBuffer[DataPosn];
                result = DataReg;
                DataPosn++;
                DataCount--;

                if(DataCount == 0) {
                    // data transfer finished
                    ClearDRQ();
                    SetINTRQ();
                    EnterIdle();
                    return result;
                }

                OverRunTest = true;
                OverRunCount = 64;

                if((DataCount % Drive[CurrentUnit].SectorSize) == 0 ) {
                    // moving to next sector in multi-sector transfer
                    SectorReg++;
                    // printf("Multi sector SectorReg++\n");
                    #ifdef LOW_RAM
                    ReadRawOneSectorData(CurrentUnit, TrackReg, CurrentHead, SectorReg - 1);
                    #endif
                }

                break;

            case WDCommandType::WD_READADDRESS:

                DataReg = DataBuffer[DataPosn];
                result = DataReg;
                DataPosn++;
                DataCount--;
                
                if(DataCount == 0) {
                    // data transfer finished
                    ClearDRQ();
                    SetINTRQ();
                    EnterIdle();
                    return result;
                }
                
                OverRunTest = true;
                OverRunCount = 64;

    }
    
    return result;

}

void WD1793::WriteDataReg(unsigned char newData) {

    DataReg = newData;

    if(CurrentCmd == WDCommandType::WD_WRITESECTOR) {
        DataBuffer[DataPosn] = DataReg;
        DataPosn++;
        DataCount--;
        if(DataCount == 0) {
            // data transfer finished
            #ifdef LOW_RAM
            WriteRawOneSectorData(CurrentUnit, TrackReg, CurrentHead, SectorReg - 1);
            #else
            WriteRawSectorData(CurrentUnit, TrackReg, CurrentHead, SectorReg - 1, SectorCount);
            #endif
            ClearDRQ();
            SetINTRQ();
            EnterIdle();
            return;
        }

        #ifdef LOW_RAM
        if(DataPosn == 256) {
            // flush buffer
            WriteRawOneSectorData(CurrentUnit, TrackReg, CurrentHead, SectorReg - 1);
            // moving to next sector in multi-sector transfer
            SectorReg++;
            DataPosn=0;
        }
        #endif

    }

}

unsigned char WD1793::ReadSystemReg() {
    
unsigned char result;

if(OverRunTest) {
    OverRunCount--;
    if(OverRunCount == 0) {
        if (CurrentCmd==WDCommandType::WD_READSECTOR || CurrentCmd==WDCommandType::WD_READTRACK || CurrentCmd==WDCommandType::WD_READADDRESS) {
            StatusReg |= STATUS_LOSTDATA;
            ClearDRQ();
            SetINTRQ();
            EnterIdle();
        }
    }
}

result = SystemReg;

return result;

}

void WD1793::WriteSystemReg(unsigned char newData) {

    // SystemReg = newData;
    if (CurrentUnit != (newData & 3)) {
        CurrentUnit = newData & 3;
        sclConverted = false;
    }
    CurrentHead = ( ( newData ^ 16 ) >> 4 ) & 1;
    DriveReady = Drive[CurrentUnit].Available;

}

void WD1793::WriteCommandReg(unsigned char wdCmd) {

  lastCmd = wdCmd;
  ExecuteCommand(wdCmd);

}

void WD1793::SCLtoTRD(unsigned char* track0, unsigned char UnitNum) {

    uint8_t numberOfFiles;

    // Reset track 0 info
    memset(track0,0,2304);

    fseek(Drive[UnitNum].DiskFile,8,SEEK_SET);
    fread(&numberOfFiles,1,1,Drive[UnitNum].DiskFile);

    // printf("Number of files: %d\n",(int)numberOfFiles);

    char diskNameArray[9]="        ";
    string fname = Drive[UnitNum].FName.substr(Drive[UnitNum].FName.find_last_of("/") + 1);
    int len = fname.find(".");
    fname.copy(diskNameArray,len > 8 ? 8 : len);
    // printf("fname: %s, diskNameArray: %s\n",fname.c_str(), diskNameArray);

    // Populate FAT.
    int startSector = 0;
    int startTrack = 1; // Since Track 0 is reserved for FAT and Disk Specification.

    uint8_t data;
    for (int i = 0; i < numberOfFiles; i++) {

        int n = i << 4;

        for (int j = 0; j < 13; j++) {
            fread(&data,1,1,Drive[UnitNum].DiskFile);
            track0[n + j] = data;
        }
        
        fread(&data,1,1,Drive[UnitNum].DiskFile); // Filelenght
        track0[n + 13] = data;

        // printf("File #: %d, Filelenght: %d\n",i + 1,(int)data);

        track0[n + 14] = (uint8_t)startSector;
        track0[n + 15] = (uint8_t)startTrack;

        int newStartTrack = (startTrack * 16 + startSector + data) / 16;
        startSector = (startTrack * 16 + startSector + data) - 16 * newStartTrack;
        startTrack = newStartTrack;
    
    }

    // Populate Disk Specification.
    track0[2273] = (uint8_t)startSector;
    track0[2274] = (uint8_t)startTrack;
    track0[2275] = 22; // Disk Type
    track0[2276] = (uint8_t)numberOfFiles; // File Count
    uint16_t freeSectors = 2560 - (startTrack << 4) + startSector;
    // printf("Free Sectors: %d\n",freeSectors);
    track0[2277] = freeSectors & 0x00ff;
    track0[2278] = freeSectors >> 8;
    track0[2279] = 0x10; // TR-DOS ID

    for (int i = 0; i < 9; i++) track0[2282 + i] = 0x20;

    // Store the image file name in the disk label section of the Disk Specification.
    for (int i = 0; i < 8; i++) track0[2293 + i] = diskNameArray[i];

    Drive[UnitNum].sclDataOffset =  (9 + (numberOfFiles * 14)) - 4096;
    
}
