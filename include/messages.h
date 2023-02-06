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
#define MSG_LOADING "Loading file"
#define MSG_LOADING_SNA "Loading SNA file"
#define MSG_LOADING_Z80 "Loading Z80 file"
#define MSG_SAVE_CONFIG "Saving config file"
#define MSG_CHIP_SETUP "Chip setup"
#define MSG_VGA_INIT "Initializing VGA"
#define MSG_FREE_HEAP_BEFORE "Free heap before "
#define MSG_FREE_HEAP_AFTER "Free heap after "
#define MSG_Z80_RESET "Reseting Z80 CPU"
#define MSG_EXEC_ON_CORE "Executing on core #"
#define ULA_ON "ULA ON"
#define ULA_OFF "ULA OFF"

// WiFi
#define MSG_WIFI_CONN_BEGIN "Connecting to WiFi"

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
#define OSD_BOTTOM "      SCIENCE  LEADS  TO  PROGRESS      "
#define OSD_ON "OSD ON"
#define OSD_OFF "OSD OFF"
#define OSD_DEMO_MODE_ON "Demo Mode ON"
#define OSD_DEMO_MODE_OFF "Demo Mode OFF"
#define OSD_PAUSE "--=[PAUSED]=--"

#define OSD_QSNA_NOT_AVAIL "No Quick Snapshot Available"
#define OSD_QSNA_LOADING "Loading Quick Snapshot..."
#define OSD_QSNA_SAVING "Saving Quick Snapshot..."
#define OSD_QSNA_SAVE_ERR "ERROR Saving Quick Snapshot"
#define OSD_QSNA_LOADED "Quick Snapshot Loaded"
#define OSD_QSNA_LOAD_ERR "ERROR Loading Quick Snapshot"
#define OSD_QSNA_SAVED "Quick Snapshot Saved"

#define OSD_PSNA_NOT_AVAIL "No Persist Snapshot Available"
#define OSD_PSNA_LOADING "Loading Persist Snapshot..."
#define OSD_PSNA_SAVING "Saving Persist Snapshot..."
#define OSD_PSNA_SAVE_WARN "Disk error. Trying slow mode, be patient"
#define OSD_PSNA_SAVE_ERR "ERROR Saving Persist Snapshot"
#define OSD_PSNA_LOADED  "  Persist Snapshot Loaded  "
#define OSD_PSNA_LOAD_ERR "ERROR Loading Persist Snapshot"
#define OSD_PSNA_SAVED  "  Persist Snapshot Saved  "

#define OSD_TAPE_LOAD_ERR "ERROR Loading TAP file"
#define OSD_TAPE_SELECT_ERR "Please select TAP file first"

#define MENU_SNA_TITLE "Select Snapshot"
#define MENU_TAP_TITLE "Select TAP file"
#define MENU_SNA \
    "Snapshot menu\n"\
    "Load (SNA,Z80) \t[F2] >\n"\
    "Load snapshot \t[F3]  \n"\
    "Save snapshot \t[F4]  \n"
#define MENU_TAPE \
    "Tape menu\n"\
    "Select TAP  \t[F5] >\n"\
    "Play/Pause  \t[F6]  \n"\
    "Stop  \t[F7]  \n"
#define MENU_MAIN \
    "Main Menu\n"\
    "Snapshot\t>\n"\
    "Tape\t>\n"\
    "Reset\t>\n"\
    "Options\t>\n"\
    "About\n"
#define MENU_OPTIONS \
    "Options menu\n"\
    "Storage\t>\n"\
    "ROM\t>\n"\
    "Aspect ratio\t>\n"
#define MENU_ASPECT_169 \
    "Aspect Ratio\n"\
    "4:3\t[ ]\n"\
    "16:9\t[*]\n"
#define MENU_ASPECT_43 \
    "Aspect Ratio\n"\
    "4:3\t[*]\n"\
    "16:9\t[ ]\n"
#define MENU_RESET \
    "Reset Menu\n"\
    "Soft reset\n"\
    "Hard reset\n"\
    "ESP32 reset\t[F12]\n"
#define MENU_PERSIST \
    "Slot 1\n"\
    "Slot 2\n"\
    "Slot 3\n"\
    "Slot 4\n"\
    "Slot 5\n"
#define MENU_PERSIST_SAVE \
    "Persist Save\n" MENU_PERSIST
#define MENU_PERSIST_LOAD \
    "Persist Load\n" MENU_PERSIST    
#define MENU_STORAGE_SD "Select storage\n"\
    "Internal\t[ ]\n"\
    "SD Card\t[*]\n"
#define MENU_STORAGE_INTERNAL "Select storage\n"\
    "Internal\t[*]\n"\
    "SD Card\t[ ]\n"
#define MENU_ARCH "Select Arch\n"\
    "48K\t>\n"\
    "128K\t>\n"
#define MENU_ROMSET48 "Select Rom Set\n"\
    "SINCLAIR\n"\
    "SE\n"/*\
    "DIAG\n"*/
#define MENU_ROMSET128 "Select Rom Set\n"\
    "SINCLAIR\n"\
    "PLUS2A\n"
#define OSD_HELP \
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

#endif // ESPECTRUM_MESSAGES_h
