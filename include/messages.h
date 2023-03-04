///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum - ZX Spectrum emulator for ESP32
//
// Copyright (c) 2020, 2021 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez, Jorge Fuertes and many others
// https://github.com/rampa069/ZX-ESPectrum
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#ifndef ESPECTRUM_MESSAGES_h
#define ESPECTRUM_MESSAGES_h

// General
#define MSG_LOADING_SNA "Loading SNA file"
#define MSG_LOADING_Z80 "Loading Z80 file"
#define MSG_SAVE_CONFIG "Saving config file"
#define MSG_VGA_INIT "Initializing VGA"

// Error
#define ERROR_TITLE "  !!!   ERROR - CLIVE MEDITATION   !!!  "
#define ERROR_BOTTOM "  Sir Clive is smoking in the Rolls...  "
#define ERR_READ_FILE "Cannot read file!"
#define ERR_BANK_FAIL "Failed to allocate RAM bank"
#define ERR_FS_INT_FAIL "Cannot mount internal storage!"
#define ERR_FS_EXT_FAIL "Cannot mount external storage!"
#define ERR_DIR_OPEN "Cannot open directory!"

// OSD
#define OSD_TITLE  "  ZX-ESPectrum-IDF - powered by ESP32   "
#define OSD_BOTTOM " SCIENCE LEADS TO PROGRESS    v1.0beta3 "

#define OSD_PAUSE_EN " --=[PAUSED]=-- "
#define OSD_PAUSE_ES "--=[EN PAUSA]=--"
static const char *OSD_PAUSE[2] = { OSD_PAUSE_EN,OSD_PAUSE_ES };

#define OSD_PSNA_NOT_AVAIL "No Persist Snapshot Available"
#define OSD_PSNA_LOADING "Loading Persist Snapshot..."
#define OSD_PSNA_SAVING "Saving Persist Snapshot..."
#define OSD_PSNA_SAVE_WARN "Disk error. Trying slow mode, be patient"
#define OSD_PSNA_SAVE_ERR "ERROR Saving Persist Snapshot"
#define OSD_PSNA_LOADED  "  Persist Snapshot Loaded  "
#define OSD_PSNA_LOAD_ERR "ERROR Loading Persist Snapshot"
#define OSD_PSNA_SAVED  "  Persist Snapshot Saved  "
#define OSD_TAPE_LOAD_ERR "ERROR Loading TAP file"

#define OSD_TAPE_SELECT_ERR_EN "Please select TAP file first"
#define OSD_TAPE_SELECT_ERR_ES "Por favor elija primero un archivo TAP"
static const char *OSD_TAPE_SELECT_ERR[2] = { OSD_TAPE_SELECT_ERR_EN,OSD_TAPE_SELECT_ERR_ES };

#define MENU_SNA_TITLE_EN "Select Snapshot"
#define MENU_SNA_TITLE_ES "Elija snapshot"
static const char *MENU_SNA_TITLE[2] = { MENU_SNA_TITLE_EN,MENU_SNA_TITLE_ES };

#define MENU_TAP_TITLE_EN "Select TAP file"
#define MENU_TAP_TITLE_ES "Elija fichero TAP"
static const char *MENU_TAP_TITLE[2] = { MENU_TAP_TITLE_EN,MENU_TAP_TITLE_ES };

#define MENU_SNA_EN \
    "Snapshot menu\n"\
    "Load (SNA,Z80) \t[F2] >\n"\
    "Load snapshot \t[F3] >\n"\
    "Save snapshot \t[F4] >\n"
#define MENU_SNA_ES \
    "Menu snapshots\n"\
    "Cargar (SNA,Z80) \t[F2] >\n"\
    "Cargar snapshot \t[F3] >\n"\
    "Guardar snapshot \t[F4] >\n"
static const char *MENU_SNA[2] = { MENU_SNA_EN,MENU_SNA_ES };

#define MENU_TAPE_EN \
    "Tape menu\n"\
    "Select TAP  \t[F5] >\n"\
    "Play/Pause  \t[F6]  \n"\
    "Stop  \t[F7]  \n"
#define MENU_TAPE_ES \
    "Casete\n"\
    "Elegir TAP  \t[F5] >\n"\
    "Play/Pausa  \t[F6]  \n"\
    "Stop  \t[F7]  \n"
static const char *MENU_TAPE[2] = { MENU_TAPE_EN,MENU_TAPE_ES };

#define MENU_MAIN_EN /*"Main Menu\n"*/ \
    "Snapshot\t>\n"\
    "Tape\t>\n"\
    "Reset\t>\n"\
    "Options\t>\n"\
    "About\n"
#define MENU_MAIN_ES /*"Menu principal\n"*/ \
    "Snapshots\t>\n"\
    "Casete\t>\n"\
    "Resetear\t>\n"\
    "Opciones\t>\n"\
    "Acerca de\n"
static const char *MENU_MAIN[2] = { MENU_MAIN_EN,MENU_MAIN_ES };

#define MENU_OPTIONS_EN \
    "Options menu\n"\
    "Storage\t>\n"\
    "Machine\t>\n"\
    "Aspect ratio\t>\n"\
    "Language\t>\n"\
    "Other\t>\n"
#define MENU_OPTIONS_ES \
    "Menu opciones\n"\
    "Almacenamiento\t>\n"\
    "Modelo\t>\n"\
    "Rel. aspecto\t>\n"\
    "Idioma\t>\n"\
    "Otros\t>\n"
static const char *MENU_OPTIONS[2] = { MENU_OPTIONS_EN,MENU_OPTIONS_ES };

#define MENU_ASPECT_EN \
    "Aspect Ratio\n"\
    "4:3\t[4]\n"\
    "16:9\t[1]\n"
#define MENU_ASPECT_ES \
    "Rel. aspecto\n"\
    "4:3\t[4]\n"\
    "16:9\t[1]\n"
static const char *MENU_ASPECT[2] = { MENU_ASPECT_EN, MENU_ASPECT_ES };

#define MENU_RESET_EN \
    "Reset Menu\n"\
    "Soft reset\n"\
    "Hard reset\n"\
    "ESP32 reset\t[F12]\n"
#define MENU_RESET_ES \
    "Resetear\n"\
    "Reset parcial\n"\
    "Reset completo\n"\
    "Resetear ESP32\t[F12]\n"
static const char *MENU_RESET[2] = { MENU_RESET_EN, MENU_RESET_ES };

#define MENU_PERSIST_EN \
    "Slot 1\n"\
    "Slot 2\n"\
    "Slot 3\n"\
    "Slot 4\n"\
    "Slot 5\n"
#define MENU_PERSIST_ES \
    "Ranura 1\n"\
    "Ranura 2\n"\
    "Ranura 3\n"\
    "Ranura 4\n"\
    "Ranura 5\n"
#define MENU_PERSIST_SAVE_EN \
    "Save snapshot\n" MENU_PERSIST_EN
#define MENU_PERSIST_SAVE_ES \
    "Guardar snapshot\n" MENU_PERSIST_ES
static const char *MENU_PERSIST_SAVE[2] = { MENU_PERSIST_SAVE_EN, MENU_PERSIST_SAVE_ES };

#define MENU_PERSIST_LOAD_EN \
    "Load snapshot\n" MENU_PERSIST_EN
#define MENU_PERSIST_LOAD_ES \
    "Cargar snapshot\n" MENU_PERSIST_ES
static const char *MENU_PERSIST_LOAD[2] = { MENU_PERSIST_LOAD_EN, MENU_PERSIST_LOAD_ES };

#define MENU_STORAGE_EN "Storage\n"\
    "Internal\t[I]\n"\
    "SD Card\t[S]\n"\
    "Refresh directories\n"
#define MENU_STORAGE_ES "Almacenamiento\n"\
    "Interno\t[I]\n"\
    "Tarjeta SD\t[S]\n"\
    "Refrescar directorios\n"
static const char *MENU_STORAGE[2] = { MENU_STORAGE_EN, MENU_STORAGE_ES };

#define MENU_OTHER_EN "Other\n"\
    "AY on 48K\t>\n"
#define MENU_OTHER_ES "Otros\n"\
    "AY en 48K\t>\n"
static const char *MENU_OTHER[2] = { MENU_OTHER_EN, MENU_OTHER_ES };

#define MENU_AY48_EN "AY on 48K\n"\
    "Yes\t[Y]\n"\
    "No\t[N]\n"
#define MENU_AY48_ES "AY en 48K\n"\
    "Si\t[Y]\n"\
    "No\t[N]\n"
static const char *MENU_AY48[2] = { MENU_AY48_EN, MENU_AY48_ES };

#define MENU_ARCH_EN "Select machine\n"\
    "ZX Spectrum 48K\n"\
    "ZX Spectrum 128K\n"
#define MENU_ARCH_ES "Elija modelo\n"\
    "ZX Spectrum 48K\n"\
    "ZX Spectrum 128K\n"
static const char *MENU_ARCH[2] = { MENU_ARCH_EN, MENU_ARCH_ES };

#define MENU_ROMSET48_EN "Select ROM\n"\
    "SINCLAIR\n"\
    "SE\n"
#define MENU_ROMSET48_ES "Elija ROM\n"\
    "SINCLAIR\n"\
    "SE\n"
static const char *MENU_ROMSET48[2] = { MENU_ROMSET48_EN, MENU_ROMSET48_ES };

#define MENU_ROMSET128_EN "Select ROM\n"\
    "SINCLAIR\n"\
    "PLUS2\n"
#define MENU_ROMSET128_ES "Elija ROM\n"\
    "SINCLAIR\n"\
    "PLUS2\n"
static const char *MENU_ROMSET128[2] = { MENU_ROMSET128_EN, MENU_ROMSET128_ES };

#define MENU_LANGUAGE_EN "Language\n"\
    "Interface\t>\n"\
    "Keyboard\t>\n"
#define MENU_LANGUAGE_ES "Idioma\n"\
    "Interfaz\t>\n"\
    "Teclado\t>\n"
static const char *MENU_LANGUAGE[2] = { MENU_LANGUAGE_EN, MENU_LANGUAGE_ES };

#define MENU_INTERFACE_LANG_EN "Language\n"\
    "English\t[ ]\n"\
    "Spanish\t[ ]\n"
#define MENU_INTERFACE_LANG_ES "Idioma\n"\
    "Ingles\t[ ]\n"\
    "Espanol\t[ ]\n"
static const char *MENU_INTERFACE_LANG[2] = { MENU_INTERFACE_LANG_EN, MENU_INTERFACE_LANG_ES };

#define MENU_KBD_LAYOUT_EN "Select language\n"\
    "US English\t[US]\n"\
    "Spanish\t[ES]\n"\
    "German\t[DE]\n"\
    "French\t[FR]\n"\
    "UK British\t[UK]\n"
#define MENU_KBD_LAYOUT_ES "Elija idioma\n"\
    "Ingles EEUU\t[US]\n"\
    "Espanol\t[ES]\n"\
    "Aleman\t[DE]\n"\
    "Frances\t[FR]\n"\
    "Ingles GB\t[UK]\n"
static const char *MENU_KBD_LAYOUT[2] = { MENU_KBD_LAYOUT_EN, MENU_KBD_LAYOUT_ES };

#define OSD_ABOUT_EN \
    " (C)2023 Victor Iborra AKA Eremus\n"\
    " https://github.com/eremusOne\n"\    
    "\n"\
    " Based on ZX-ESPectrum-Wiimote\n"\
    " (C)2020-2023 David Crespo\n"\
    " https://github.com/dcrespo3d\n"\
    " https://youtube.com/davidprograma\n"\
    "\n"\
    " Original (C) 2019 Rampa & Queru\n"\
    " https://github.com/rampa069\n"\
    "\n"\
    " Z80 emulation by JL Sanchez\n"\        
    " VGA driver by BitLuni\n"\    
    " PS2 driver by Fabrizio di Vittorio\n"\
    " Greetings to http://retrowiki.es and\n"\
    " his people (Hi ackerman!) for the\n"\
    " support and inspiration.\n"    
#define OSD_ABOUT_ES \
    " (C)2023 Victor Iborra AKA Eremus\n"\
    " https://github.com/eremusOne\n"\   
    "\n"\
    " Basado en ZX-ESPectrum-Wiimote\n"\
    " (C)2020-2023 David Crespo\n"\
    " https://github.com/dcrespo3d\n"\
    " https://youtube.com/davidprograma\n"\
    "\n"\
    " Original (C) 2019 Rampa & Queru\n"\
    " https://github.com/rampa069\n"\
    "\n"\
    " Emulacion Z80 por JL Sanchez\n"\        
    " Driver VGA por BitLuni\n"\    
    " Driver PS2 por Fabrizio di Vittorio\n"\
    " Saludos a http://retrowiki.es y\n"\
    " su gente (Hola ackerman!) por su\n"\
    " ayuda e inspiracion.\n"
static const char *OSD_ABOUT[2] = { OSD_ABOUT_EN, OSD_ABOUT_ES };

#endif // ESPECTRUM_MESSAGES_h
