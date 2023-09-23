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

#ifndef ROMS_H
 #define ROMS_H

//  #include <stddef.h>
 //#include "roms/romDiag48K.h"
 #include "roms/romSinclair48K.h"
 //#include "roms/romSE48K.h"
 //#include "roms/romRamTester48K.h"
  
 //#include "roms/romPlus2A128K.h"
 // #include "roms/romPlus3128K.h"
 // #include "roms/romPlus3E128K.h"
 #include "roms/romSinclair128K.h"
 //#include "roms/romPlus2128K.h"
//  #include "roms/romZX81.edition3.h"

#include "roms/trdos.h"
#include "roms/rompentagon128k.h"

//  #define max_list_rom_48 1
 
//  #define max_list_rom_128 1
 
//  //roms 48K 
//  //Titulos
//  static const char * gb_list_roms_48k_title[max_list_rom_48]={
//   "SINCLAIR",
//  /* "DIAG",
//   "SE",
//   "TESTRAM128"*/
//  };
 
//  //Datos 48K 1 ROM en slot 0
//  static const unsigned char * gb_list_roms_48k_data[max_list_rom_48]={
//   gb_rom_0_sinclair_48k,
//   /*gb_rom_0_diag_48k,
//   gb_rom_0_SE_48k,
//   gb_rom_0_ramtester_48k*/
//  };

//  //Datos 128K
//  static const char * gb_list_roms_128k_title[max_list_rom_128]={
//   "SINCLAIR"/*,
//   "PLUS2",
//   "PLUS3",
//   "PLUS3E"*/
//  };


//  //Datos 128K 4 roms en 4 slots
//  static const unsigned char * gb_list_roms_128k_data[max_list_rom_128][4]={
//   gb_rom_0_sinclair_128k,gb_rom_1_sinclair_128k,gb_rom_0_sinclair_128k,gb_rom_1_sinclair_128k
//  };

//   //Datos 128K 4 roms en 4 slots
//  static const unsigned char * gb_list_roms_128k_data[max_list_rom_128][4]={
//   gb_rom_0_sinclair_128k,gb_rom_1_sinclair_128k,gb_rom_0_sinclair_128k,gb_rom_1_sinclair_128k,
//   gb_rom_0_plus2_128k,gb_rom_1_plus2_128k,gb_rom_0_plus2_128k,gb_rom_1_plus2_128k,
//   gb_rom_0_plus3_128k,gb_rom_1_plus3_128k,gb_rom_2_plus3_128k,gb_rom_3_plus3_128k,
//   gb_rom_0_plus3E_128k,gb_rom_1_plus3E_128k,gb_rom_2_plus3E_128k,gb_rom_3_plus3E_128k
//   };

#endif