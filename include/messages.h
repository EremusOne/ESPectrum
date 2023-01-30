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
#define OSD_TITLE  "  ZX-ESPectrum - ESP32 - VGA - Wiimote  "
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
#define MENU_TAP_SELECTED \
    "How to load TAP\n"\
    "Type LOAD \"\"\n"\
    "F6 Play/Pause tape\n"\
    "F7 Stop tape\n"
#define MENU_MAIN \
    "Main Menu\n"\
    "Load SNA or Z80\n"\
    "Select TAP file\n"\
    "Select ROM\n"\
    "Load state (F2)\n"\
    "Save state (F3)\n"\
    "Aspect Ratio...\n"\
    "Reset\n"\
    "About\n"
#define MENU_ASPECT_169 \
    "Aspect Ratio\n"\
    "16:9 (current)\n"\
    "4:3  (will reset)\n"
#define MENU_ASPECT_43 \
    "Aspect Ratio\n"\
    "4:3  (current)\n"\
    "16:9 (will reset)\n"
#define MENU_RESET \
    "Reset Menu\n"\
    "Soft reset\n"\
    "Hard reset\n"\
    "ESP host reset\n"\
    "Cancel\n"
#define MENU_PERSIST \
    "Slot 1\n"\
    "Slot 2\n"\
    "Slot 3\n"\
    "Slot 4\n"\
    "Slot 5\n"\
    "Cancel\n"
#define MENU_PERSIST_SAVE \
    "Persist Save\n" MENU_PERSIST
#define MENU_PERSIST_LOAD \
    "Persist Load\n" MENU_PERSIST    
#define MENU_DEMO "Demo mode\nOFF\n 1 minute\n 3 minutes\n 5 minutes\n15 minutes\n30 minutes\n 1 hour\n"
#define MENU_ARCH "Select Arch\n"\
    "48K\n"\
    "128K\n"
#define MENU_ROMSET48 "Select Rom Set\n"\
    "SINCLAIR\n"\
    "SE\n"\
    "DIAG\n"
#define MENU_ROMSET128 "Select Rom Set\n"\
    "SINCLAIR\n"\
    "PLUS2A\n"
#define OSD_HELP \
    "Developed in 2019 by Rampa & Queru\n"\
    "Modified  in 2020 by DCrespo\n"\
    "for adding Wiimote[W] as input device,\n"\
    "keeping PS/2 keyboard[K] support.\n" \
    "Modified in 2022 by Eremus.\n" \
    "for adding tape and multicolour support.\n\n" \
    "    [K]F1       [W]Home   for main menu\n"\
    "    [K]Cursors  [W]DPad   to move.\n"\
    "    [K]Enter    [W]A/1/2  to select.\n"\
    "    [K]ESC/F1   [W]Home   to exit.\n\n"\
    "For syncing Wiimote, press buttons 1 & 2\n\n"\
    "Kempston joystick is emulated using\n"\
    "cursor keys and AltGr for fire button.\n"
#define MENU_TEST getTestMenu(200)

#endif // ESPECTRUM_MESSAGES_h
