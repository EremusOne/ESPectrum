/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023-2025 Víctor Iborra [Eremus] and 2023 David Crespo [dcrespo3d]
https://github.com/EremusOne/ESPectrum

Based on ZX-ESPectrum-Wiimote
Copyright (c) 2020-2022 David Crespo [dcrespo3d]
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

To Contact the dev team you can write to zxespectrum@gmail.com

*/

#ifndef ESPECTRUM_MESSAGES_ES_h
#define ESPECTRUM_MESSAGES_ES_h

#define ERR_FS_EXT_FAIL_ES "\xAD" "Almacenamiento externo no disponible!"

#define OSD_MSGDIALOG_YESNO_ES "  S\xA1  \n  No  \n"

#define OSD_PAUSE_ES "--=[EN PAUSA]=--"

#define POKE_ERR_ADDR1_ES "Direcci\xA2n debe estar entre 16384 y 65535"
#define POKE_ERR_ADDR2_ES "Direcci\xA2n debe ser menor que 16384"
#define POKE_ERR_VALUE_ES "Valor debe ser menor que 256"

#define OSD_FAT32_INVALIDCHAR_ES "Car\xA0" "cter no v\xA0lido en FAT32"

#define OSD_TAPE_SAVE_ES "Comando SAVE"

#define OSD_TAPE_SAVE_EXIST_ES "El fichero ya existe \xA8Sobreescribir?"

#define OSD_PSNA_SAVE_ES "Guardar snapshot ESP"

#define OSD_PSNA_EXISTS_ES "\xA8Sobreescribir?"

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

#define MENU_ESP_LOAD_TITLE_ES "Elija snap ESP"

#define MENU_ESP_SAVE_TITLE_ES "Guardar snap ESP"

#define MENU_DELETE_TAP_BLOCKS_ES "Borrar selecci\xA2n"

#define MENU_DELETE_CURRENT_TAP_BLOCK_ES "Borrar bloque"

#define OSD_BLOCK_SELECT_ERR_ES "Ning\xA3n bloque seleccionado"

#define OSD_BLOCK_TYPE_ERR_ES "Tipo de bloque inv\xA0lido"

#define MENU_DELETE_CURRENT_FILE_ES "Borrar archivo"
#define MENU_DELETE_CURRENT_DIR_ES "Borrar directorio"

#define OSD_READONLY_FILE_WARN_ES "Archivo de solo lectura"
#define OSD_FILEEXISTS_WARN_ES "El archivo ya existe"
#define OSD_FILEERROR_WARN_ES "Error de archivo: "

#define OSD_DIREXISTS_WARN_ES "El directorio ya existe"
#define OSD_DIRNOTEMPTY_WARN_ES "Directorio no vacio"

#define OSD_TAPE_FLASHLOAD_ES "Carga rapida de cinta"
#define OSD_TAPE_INSERT_ES "Cinta insertada"
#define OSD_TAPE_EJECT_ES "Cinta expulsada"

#define OSD_DISK_EJECT_ES "Disco expulsado"

#define TRDOS_RESET_ERR_ES "Betadisk desactivado o no disponible"

#define MENU_SNA_ES \
    "Men\xA3 snapshots\n"\
    "Cargar (SNA,Z80,SP,P) \t(F2) >\n"\
    "Cargar snapshot ESP\t(F6) >\n"\
    "Guardar snapshot ESP\t(F7)  \n"

#define MENU_TAPE_ES \
    "Casete\n"\
    "Elegir (TAP,TZX) \t(F4) >\n"\
    "Play/Stop\t(F5)  \n"\
    "Expulsar cinta\t(SF5)  \n"\
    "Navegador cinta\t(SF4)  \n"\
	"Amplificaci\xA2n\t>\n"\
    "Entrada audio\t>\n"\
    "H/W entrada audio\t>\n"

#define MENU_AMP_ES "Amplificaci\xA2n\n"\
    "EAR\t[1]\n"\
    "MIC\t[2]\n"\
    "Ambos\t[3]\n"\
    "Ninguna\t[4]\n"

#define MENU_AUDIOIN_ESPECTRUM_ES "Entrada EAR (GPIO03)\t[03]\n"\
    "USB-A 1     (GPIO32)\t[32]\n"\
    "USB-A 1     (GPIO33)\t[33]\n"\
    "USB-A 2     (GPIO16)\t[16]\n"\
    "USB-A 2     (GPIO17)\t[17]\n"

#define MENU_AUDIOIN_ESPECTRUM_PSRAM_ES "Entrada EAR (GPIO03)\t[03]\n"\
    "USB-A       (GPIO32)\t[32]\n"\
    "USB-A       (GPIO33)\t[33]\n"

#define MENU_AUDIOIN_LILYGO_ES "Segundo PS/2 (GPIO26)\t[26]\n"\
    "Segundo PS/2 (GPIO27)\t[27]\n"\
    "Pad interno  (GPI34)\t[34]\n"\
    "Pad interno  (GPI39)\t[39]\n"

#define MENU_AUDIOIN_OLIMEX_ES "Segundo PS/2 (GPIO26)\t[26]\n"\
	"Segundo PS/2 (GPIO27)\t[27]\n"\
	"Access Bus 6 (GPI34)\t[34]\n"\
	"Access Bus 8 (GPI39)\t[39]\n"

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
    "Opciones\t>\n"\
    "Resetear\t>\n"\
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
    "Otros\t>\n"\
	"Actualizar\t>\n"\
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

#define MENU_RESET_HWB_ES "Bot\xA2n hardware\t>\n"

#define MENU_RESET_BUTTON_ES \
    "Bot\xA2n hardware\n"\
    "GPI34\t[34]\n"\
    "GPI36\t[36]\n"\
    "GPI39\t[39]\n"\
    "No presente\t[00]\n"

#define MENU_PERSIST_SAVE_ES \
    "Guardar snapshot\n"

#define MENU_PERSIST_LOAD_ES \
    "Cargar snapshot\n"

#define MENU_STORAGE_ES "Almacenamiento\n"\
    "Betadisk\t>\n"\
    "Carga r\xA0pida cinta\t>\n"\
    "Timings ROM R.G.\t>\n"\
    "Indice r\xA0pido TAP\t>\n"

#define MENU_YESNO_ES "S\xA1\t[Y]\n"\
    "No\t[N]\n"

#define MENU_DISKCTRL_ES "Betadisk\n"\
    "TR-DOS 5.03\t[1]\n"\
    "TR-DOS 5.03  (Modo r\xA0pido)\t[2]\n"\
    "TR-DOS 5.05D\t[3]\n"\
    "TR-DOS 5.05D (Modo r\xA0pido)\t[4]\n"\
    "Desactivado\t[5]\n"

#define MENU_OTHER_ES "Otros\n"\
    "AY en 48K\t>\n"\
    "Covox\t>\n"\
    "Timing ULA\t>\n"\
    "48K Issue 2\t>\n"\
	"ULA TK\t>\n"\
    "Segundo disp. PS/2\t>\n"\
    "Rat\xA2n\t>\n"

#define MENU_KBD2NDPS2_ES "Dispositivo\n"\
    "Nada\t[N]\n"\
    "Teclado / Adapt. DB9\t[K]\n"\
    "Rat\xA2n\t[M]\n"

#define MENU_MOUSE_ES "Rat\xA2n\n"\
    "No\t[N]\n"\
    "Kempston\t[K]\n"

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
