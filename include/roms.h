#ifndef ROMS_H
 #define ROMS_H

 #include <stddef.h>
 //#include "roms/romDiag48K.h"
 #include "roms/romSinclair48K.h"
 #include "roms/romSE48K.h"
 //#include "roms/romRamTester48K.h"
  
 #include "roms/romPlus2A128K.h"
 // #include "roms/romPlus3128K.h"
 // #include "roms/romPlus3E128K.h"
 #include "roms/romSinclair128K.h"
 
 #define max_list_rom_48 2
 
 #define max_list_rom_128 2
 
 //roms 48K 
 //Titulos
 static const char * gb_list_roms_48k_title[max_list_rom_48]={
  "SINCLAIR",
 /* "DIAG",*/
  "SE"/*,
  "TESTRAM128"*/
 };
 
 //Datos 48K 1 ROM en slot 0
 static const unsigned char * gb_list_roms_48k_data[max_list_rom_48]={
  gb_rom_0_sinclair_48k,
  /*gb_rom_0_diag_48k,*/
  gb_rom_0_SE_48k/*,
  gb_rom_0_ramtester_48k*/
 };

 //Datos 128K
 static const char * gb_list_roms_128k_title[max_list_rom_128]={
  "SINCLAIR",
  "PLUS2A"/*,
  "PLUS3",
  "PLUS3E"*/
 };

  //Datos 128K 4 roms en 4 slots
 static const unsigned char * gb_list_roms_128k_data[max_list_rom_128][4]={
  gb_rom_0_sinclair_128k,gb_rom_1_sinclair_128k,gb_rom_0_sinclair_128k,gb_rom_1_sinclair_128k,
  gb_rom_0_plus2a_128k,gb_rom_1_plus2a_128k,gb_rom_0_plus2a_128k,gb_rom_1_plus2a_128k/*,
  gb_rom_0_plus3_128k,gb_rom_1_plus3_128k,gb_rom_2_plus3_128k,gb_rom_3_plus3_128k,
  gb_rom_0_plus3E_128k,gb_rom_1_plus3E_128k,gb_rom_2_plus3E_128k,gb_rom_3_plus3E_128k
  */};

#endif