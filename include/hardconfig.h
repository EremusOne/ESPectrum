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
// Testing code
//
// Activate sections of code for testing / profiling
///////////////////////////////////////////////////////////////////////////////

// #define TESTING_CODE

///////////////////////////////////////////////////////////////////////////////
// RAM info key
//
// Activate VK_GRAVEACCENT for logging ram info to console
///////////////////////////////////////////////////////////////////////////////

// #define RAM_INFO_KEY

#endif // ESPectrum_config_h
