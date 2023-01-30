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

///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum - ZX Spectrum emulator for ESP32 microcontroller
//
// hardconfig.h
// hardcoded configuration file (@compile time) for ZX-ESPectrum
// created by David Crespo on 19/11/2020
//
///////////////////////////////////////////////////////////////////////////////

#ifndef ESPectrum_hardconfig_h
#define ESPectrum_hardconfig_h

///////////////////////////////////////////////////////////////////////////////
// Frame timing switch
//
// #define VIDEO_FRAME_TIMING to let the emu add a delay at the end of
// every frame in order to get the right duration in microsecs/frame
///////////////////////////////////////////////////////////////////////////////

#define VIDEO_FRAME_TIMING

///////////////////////////////////////////////////////////////////////////////
// Video output switch
//
// #define NOVIDEO for disabling video output.
// Useful for Testing & CPU / Video timing
///////////////////////////////////////////////////////////////////////////////

// #define NO_VIDEO

///////////////////////////////////////////////////////////////////////////////
// LOG_DEBUG_TIMING generates simple timing log messages to console every second.
#define LOG_DEBUG_TIMING

///////////////////////////////////////////////////////////////////////////////
// Resolution, border and centering
//
// Total number of pixels drawn affects video task timing,
// so don't be tempted to draw borders to fill screen, it would ruin timing!
//
// BOR_W and BOR_H are the actual border pixels drawn outside of image.
// OFF_X and OFF_Y are used for centering, use with caution;
// you could write off the buffer and crash the emulator.
// 
// Update: these are specified separately for the two supported aspect ratios.
///////////////////////////////////////////////////////////////////////////////
#define BOR_W_16_9 44
#define BOR_H_16_9 4
#define OFF_X_16_9 1
#define OFF_Y_16_9 0

// if you can't center the image in your screen,
// set some offset, (ex: OFF_X = _20_)
// use a smaller border (ex: BOR_W = 32 == 52 - _20_)
// then change OFF_X for software centering (0 < OFF_X < 40) (40 == 2 * _20_)
#define BOR_W_4_3 40
#define BOR_H_4_3 24
#define OFF_X_4_3 0
#define OFF_Y_4_3 0

///////////////////////////////////////////////////////////////////////////////
// Storage mode
//
// define ONLY one of these
// USE_INT_FLASH for internal flash storage
// USE_SD_CARD for external SD card
///////////////////////////////////////////////////////////////////////////////

#define USE_INT_FLASH 1
// #define USE_SD_CARD 1

// check: only one must be defined
#if defined(USE_INT_FLASH) && defined(USE_SD_CARD)
#error "Only one of (USE_INT_FLASH, USE_SD_CARD) must be defined"
#endif
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// PS/2 Keyboard
//
// define PS2_KEYB_PRESENT if you want PS/2 Keyboard to be enabled.
//
// also, if you happen to have a pesky keyboard which won't initialize
// until it receives an echo message, also define PS2_KEYB_FORCE_INIT
#define PS2_KEYB_PRESENT
#define PS2_KEYB_FORCE_INIT

// define NONE, ONE or BOTH of this
// PS2_ARROWKEYS_AS_CURSOR will use arrow keys 
// PS2_ARROWKEYS_AS_KEMPSTON will use arrow keys as kempston joystick
// directions, with AltGr a fire button.
// #define PS2_ARROWKEYS_AS_CURSOR
#define PS2_ARROWKEYS_AS_KEMPSTON

// for using convenience keys, define just ONE of
// PS2_CONVENIENCE_KEYS_EN for english keyboard: [backspace][,][.][/][;][´][-][=]
// PS2_CONVENIENCE_KEYS_EN for SPANISH keyboard: [backspace][,][.][-][´][+]
//#define PS2_CONVENIENCE_KEYS_EN
#define PS2_CONVENIENCE_KEYS_ES

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Wiimote Support
//
// define WIIMOTE_PRESENT if you want Wiimote to be supported
// also, you should define wiimote button mappings for each game.
//

//#define WIIMOTE_PRESENT
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Audio I/O
//
// define SPEAKER_PRESENT if you want the speaker to be present.
// define EAR_PRESENT if you want the ear input port to be present.
// define MIC_PRESENT if you want the mic output port to be present.
// 

#define SPEAKER_PRESENT
// #define EAR_PRESENT
// #define MIC_PRESENT
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Audio I/O
//
// define USE_AY_SOUND if you want to use AY-3-891X emulation thru FabGL.
// 

#define USE_AY_SOUND
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Audio I/O
//
// define AUDIO_MIX_CLAMP to use clamping on mix of beeper and AY sound data
// 

#define AUDIO_MIX_CLAMP
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Tape traps beta (Experimental)
//
// define TAPE_TRAPS for emulator trapping LOAD and SAVE rom code. In some 
// titles it could help with right loading or auto-stoping tape in right places.
// (If a .tap doesn't load with tape load active try disabling it)

//#define TAPE_TRAPS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Snapshot loading behaviour
//
// define SNAPSHOT_LOAD_FORCE_ARCH if you want the architecture present
// in the snapshot file to be forcibly set as the current emulated machine.
// 
// if current machine is 48K and a 128K snapshot is loaded,
// arch will be switched to 128K always (with SINCLAIR romset as the default).
//
// but if machine is in 128K mode and a 48K snapshot is loaded,
// arch will be switched to 48K only if SNAPSHOT_LOAD_FORCE_ARCH is defined.

#define SNAPSHOT_LOAD_FORCE_ARCH
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Snapshot remember behaviour
//
// define SNAPSHOT_LOAD_LAST if you want the last snapshot name to be saved
// to persistent config, thus remembering snapshot when powering off,
// so that snapshot is loaded on next power on.
//
// This affects only to snapshots on "Load Snapshot to RAM",
// not to persistent snapshots.

#define SNAPSHOT_LOAD_LAST
///////////////////////////////////////////////////////////////////////////////

#endif // ESPectrum_config_h
