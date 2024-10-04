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

#ifndef ESPECTRUM_MESSAGES_ES_h
#define ESPECTRUM_MESSAGES_ES_h

#define ERR_FS_EXT_FAIL_ES "\xAD" "Almacenamiento externo no disponible!"

#define OSD_MSGDIALOG_YES_ES "  S\xA1  "
#define OSD_MSGDIALOG_NO_ES "  No  "

#define OSD_PAUSE_ES "--=[EN PAUSA]=--"

#define POKE_ERR_ADDR1_ES "Direcci\xA2n debe estar entre 16384 y 65535"
#define POKE_ERR_ADDR2_ES "Direcci\xA2n debe ser menor que 16384"
#define POKE_ERR_VALUE_ES "Valor debe ser menor que 256"

#define OSD_FAT32_INVALIDCHAR_ES "Car\xA0" "cter no v\xA0lido en FAT32"

#define OSD_TAPE_SAVE_ES "Comando SAVE"

#define OSD_TAPE_SAVE_EXIST_ES "El fichero ya existe \xA8Sobreescribir?"

#define OSD_PSNA_SAVE_ES "Guardar snapshot"

#define OSD_PSNA_EXISTS_ES "\xA8Sobreescribir ranura?"

#define OSD_TAPE_SELECT_ERR_ES "Fichero de cinta no seleccionado"

#define OSD_FILE_INDEXING_ES "Indexando"
#define OSD_FILE_INDEXING_ES_1 "   Ordenando   "
#define OSD_FILE_INDEXING_ES_2 "Grabando \xA1ndice"
#define OSD_FILE_INDEXING_ES_3 "   Limpiando   "

#define OSD_FIRMW_UPDATE_ES "Actualizar firmware"

#define OSD_DLG_SURE_ES "\xA8" "Desea continuar?"

#define OSD_DLG_JOYSAVE_ES "\xA8Guardar cambios?"

#define OSD_DLG_JOYDISCARD_ES "\xA8" "Descartar cambios?"

#define OSD_DLG_SETJOYMAPDEFAULTS_ES "\xA8" "Cargar mapeo por defecto?"

#define OSD_FIRMW_ES "Actualizando firmware"

#define OSD_FIRMW_BEGIN_ES "Borrando partici\xA2n de destino."

#define OSD_FIRMW_WRITE_ES "   Grabando nuevo firmware.   "

#define OSD_FIRMW_END_ES "  Completado. Reiniciando.   "

#define OSD_NOFIRMW_ERR_ES "Firmware no encontrado."

#define OSD_FIRMW_ERR_ES "Error actualizando firmware."

#define MENU_ROM_TITLE_ES "Elija ROM"

#define OSD_ROM_ERR_ES "Error flasheando ROM."

#define OSD_NOROMFILE_ERR_ES "Custom ROM no encontrada."

#define OSD_ROM_ES "Flashear ROM Custom"

#define OSD_ROM_BEGIN_ES "Preparando espacio en flash."

#define OSD_ROM_WRITE_ES "    Grabando ROM custom.    "

#define MENU_SNA_TITLE_ES "Elija snapshot"

#define MENU_TAP_TITLE_ES "Elija fichero de cinta"

#define MENU_DSK_TITLE_ES "Elija disco"

#define MENU_ESP_LOAD_TITLE_ES "Elija snapshot de ESPectrum"

#define MENU_ESP_SAVE_TITLE_ES "Guardar snapshot de ESPectrum"

#define MENU_DELETE_TAP_BLOCKS_ES "Borrar selecci\xA2n"

#define MENU_DELETE_CURRENT_TAP_BLOCK_ES "Borrar bloque"

#define OSD_BLOCK_SELECT_ERR_ES "Ning\xA3n bloque seleccionado"

#define OSD_BLOCK_TYPE_ERR_ES "Tipo de bloque inv\xA0lido"

#define MENU_DELETE_CURRENT_FILE_ES "Borrar archivo"

#define OSD_READONLY_FILE_WARN_ES "Archivo de solo lectura"

#define OSD_TAPE_FLASHLOAD_ES "Carga rapida de cinta"
#define OSD_TAPE_INSERT_ES "Cinta insertada"
#define OSD_TAPE_EJECT_ES "Cinta expulsada"

#define TRDOS_RESET_ERR_ES "Error en reset a TR-DOS. Active Betadisk."

#define MENU_SNA_ES \
    "Men\xA3 snapshots\n"\
    "Cargar (SNA,Z80,SP,P) \t(F2) >\n"\
    "Cargar snapshot\t(F3) >\n"\
    "Guardar snapshot\t(F4) >\n"

#define MENU_TAPE_ES \
    "Casete\n"\
    "Elegir (TAP,TZX) \t(F5) >\n"\
    "Play/Stop\t(F6)  \n"\
    "Expulsar cinta\t(SF6) \n"\
    "Navegador cinta\t(F7)  \n"\
	"Modo reproductor\t>\n"

#define MENU_BETADISK_ES \
    "Unidades\n"\
    "Unidad A\t>\n"\
    "Unidad B\t>\n"\
    "Unidad C\t>\n"\
    "Unidad D\t>\n"

#define MENU_BETADRIVE_ES \
    "Unidad#\n"\
    "Insertar disco\t>\n"\
    "Expulsar disco\n"

#define MENU_MAIN_ES \
    "Snapshots\t>\n"\
    "Casete\t>\n"\
    "Betadisk\t>\n"\
    "Modelo\t>\n"\
    "Resetear\t>\n"\
    "Opciones\t>\n"\
    "Ayuda\n"\
    "Acerca de\n"

#define MENU_OPTIONS_ES \
    "Men\xA3 opciones\n"\
    "Almacenamiento\t>\n"\
    "Modelo preferido\t>\n"\
    "ROM preferida\t>\n"\
    "Joystick\t>\n"\
    "Emulaci\xA2n joystick\t>\n"\
    "Pantalla\t>\n"\
	"Actualizar\t>\n"\
    "Otros\t>\n"\
    "Idioma\t>\n"

#define MENU_UPDATE_ES \
    "Actualizar\n"\
	"Firmware\n"\
	"ROM Custom 48K\n"\
	"ROM Custom 128k\n"\
	"ROM Custom TK\n"		

#define MENU_VIDEO_ES \
    "Pantalla\n"\
    "Tipo render\t>\n"\
	"Resoluci\xA2n\t>\n"\
    "Scanlines\t>\n"

#define MENU_RENDER_ES \
    "Tipo render\n"\
    "Est\xA0ndar\t[S]\n"\
    "Efecto nieve\t[A]\n"

#define MENU_ASPECT_ES \
    "Resoluci\xA2n\n"\
    "320x240 (4:3)\t[4]\n"\
    "360x200 (16:9)\t[1]\n"

#define MENU_RESET_ES \
    "Resetear\n"\
    "Reset parcial\n"\
	"Reset a TR-DOS \t(CF11)\n"\
    "Reset completo\t(F11)\n"\
    "Resetear ESP32\t(F12)\n"

#define MENU_PERSIST_SAVE_ES \
    "Guardar snapshot\n"

#define MENU_PERSIST_LOAD_ES \
    "Cargar snapshot\n"

#define MENU_STORAGE_ES "Almacenamiento\n"\
    "Betadisk\t>\n"\
    "Carga r\xA0pida cinta\t>\n"\
    "Timings ROM R.G.\t>\n"	

#define MENU_YESNO_ES "S\xA1\t[Y]\n"\
    "No\t[N]\n"

#define MENU_OTHER_ES "Otros\n"\
    "AY en 48K\t>\n"\
    "Timing ULA\t>\n"\
    "48K Issue 2\t>\n"\
	"ULA TK\t>\n"\
    "Segundo disp. PS/2\t>\n"	

#define MENU_KBD2NDPS2_ES "Dispositivo\n"\
    "Nada\t[N]\n"\
    "Teclado\t[K]\n"

#define MENU_ALUTIMING_ES "Timing ULA\n"\
    "Early\t[E]\n"\
    "Late\t[L]\n"


#define MENU_ARCH_ES "Elija modelo\n"

#define MENU_ROMS48_ES "Elija ROM\n"\
	"48K\n"\
    "48K Espa\xA4ol\n"\
    "Custom\n"

#define MENU_ROMS128_ES "Elija ROM\n"\
	"128K\n"\
    "128K Espa\xA4ol\n"\
	"+2\n"\
    "+2 Espa\xA4ol\n"\
    "ZX81+\n"\
    "Custom\n"

#define MENU_ROMSTK_ES "Elija ROM\n"\
    "v1 Espa\xA4ol\n"\
    "v1 Portugu\x82s\n"\
    "v2 Espa\xA4ol\n"\
    "v2 Portugu\x82s\n"\
    "v3 Espa\xA4ol (R.G.)\n"\
    "v3 Portugu\x82s (R.G.)\n"\
    "v3 Ingl\x82s (R.G.)\n"\
    "Custom\n"

#define MENU_ROMSTK95_ES "Elija ROM\n"\
    "Espa\xA4ol\n"\
    "Portugu\x82s\n"

#define MENU_ROMS48_PREF_ES	"48K\t[48K]\n"\
    "48K Espa\xA4ol\t[48Kes]\n"\
    "Custom\t[48Kcs]\n"

#define MENU_ROMSTK90X_PREF_ES "v1 Espa\xA4ol\t[v1es ]\n"\
    "v1 Portugu\x82s\t[v1pt ]\n"\
    "v2 Espa\xA4ol\t[v2es ]\n"\
    "v2 Portugu\x82s\t[v2pt ]\n"\
    "v3 Espa\xA4ol (R.G.)\t[v3es ]\n"\
    "v3 Portugu\x82s (R.G.)\t[v3pt ]\n"\
    "v3 Ingl\x82s (R.G.)\t[v3en ]\n"\
    "Custom\t[TKcs ]\n"    	

#define MENU_ROMSTK95_PREF_ES "Espa\xA4ol\t[95es ]\n"\
    "Portugu\x82s\t[95pt ]\n"

#define MENU_ROMS128_PREF_ES "128K\t[128K]\n"\
    "128K Espa\xA4ol\t[128Kes]\n"\
	"+2\t[+2]\n"\
    "+2 Espa\xA4ol\t[+2es]\n"\
    "ZX81+\t[ZX81+]\n"\
    "Custom\t[128Kcs]\n"

#define MENU_INTERFACE_LANG_ES "Idioma\n"\
    "Ingl\x82s\t[ ]\n"\
    "Espa\xA4ol\t[ ]\n"\
	"Portugu\x82s\t[ ]\n"

#define MENU_JOY_ES "Menu Joystick\n"

#define MENU_DEFJOY_ES "Definir\n"

#define MENU_JOYPS2_ES "Emulaci\xA2n Joystick\n" "Tipo joystick\t>\n" "Joy en teclas de cursor\t>\n" "TAB como disparo 1\t>\n"

#define DLG_TITLE_INPUTPOK_ES "A\xA4" "adir Poke"

#endif // ESPECTRUM_MESSAGES_ES_h
