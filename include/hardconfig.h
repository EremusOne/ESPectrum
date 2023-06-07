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
///////////////////////////////////////////////////////////////////////////////

// #define LOG_DEBUG_TIMING

///////////////////////////////////////////////////////////////////////////////
// TO DO: Tape traps
//
// define TAPE_TRAPS for emulator trapping LOAD and SAVE rom code. In some 
// titles it could help with right loading or auto-stoping tape in right places.
// (If a .tap doesn't load with tape load active try disabling it)
///////////////////////////////////////////////////////////////////////////////

// #define TAPE_TRAPS // STILL NOT WORKING!

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
///////////////////////////////////////////////////////////////////////////////

#define SNAPSHOT_LOAD_FORCE_ARCH

///////////////////////////////////////////////////////////////////////////////
// Snapshot remember behaviour
//
// define SNAPSHOT_LOAD_LAST if you want the last snapshot name to be saved
// to persistent config, thus remembering snapshot when powering off,
// so that snapshot is loaded on next power on.
//
// This affects only to snapshots on "Load Snapshot to RAM",
// not to persistent snapshots.
///////////////////////////////////////////////////////////////////////////////

// #define SNAPSHOT_LOAD_LAST

///////////////////////////////////////////////////////////////////////////////
// Testing code
//
// Activate sections of code for testing / profiling
///////////////////////////////////////////////////////////////////////////////

// #define TESTING_CODE

#endif // ESPectrum_config_h
