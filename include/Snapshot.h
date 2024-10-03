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

#ifndef Snapshot_h
#define Snapshot_h

#include <stdio.h>
#include <inttypes.h>
#include <string>

using namespace std;

bool LoadSnapshot(string filename, string force_arch, string force_romset, uint8_t force_ALU);

class FileSNA
{
public:
    static bool load(string sna_fn, string force_arch, string force_romset, uint8_t force_ALU);
    static bool save(string sna_fn);
    static bool save(string sna_fn, bool blockMode);
    static bool isPersistAvailable(string filename);
};

class FileZ80
{
public:
    static bool load(string z80_fn);
    static void loader48();    
    static void loader128();  
    static bool keepArch;      
private:
    static void loadCompressedMemData(FILE *f, uint16_t dataLen, uint16_t memStart, uint16_t memlen);
    static void loadCompressedMemPage(FILE *f, uint16_t dataLen, uint8_t* memPage, uint16_t memlen);
};

class FileSP
{
public: 
    static bool load(string sp_fn);
};

class FileP
{
public:
    static bool load(string p_fn);
};

#endif