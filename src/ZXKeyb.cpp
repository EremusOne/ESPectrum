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

#include "ZXKeyb.h"
#include "ESPectrum.h"

uint8_t ZXKeyb::ZXcols[8] = { 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf };
bool ZXKeyb::Exists;

void ZXKeyb::setup()
{
    // setup shift register pins as outputs
    gpio_set_direction((gpio_num_t)SR_CLK, (gpio_mode_t)GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)SR_LOAD, (gpio_mode_t)GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)SR_DATA, (gpio_mode_t)GPIO_MODE_OUTPUT);

    // setup columns as inputs
    gpio_set_direction((gpio_num_t)KM_COL_0, (gpio_mode_t)GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)KM_COL_1, (gpio_mode_t)GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)KM_COL_2, (gpio_mode_t)GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)KM_COL_3, (gpio_mode_t)GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)KM_COL_4, (gpio_mode_t)GPIO_MODE_INPUT);

    // Check if membrane keyboard is present
    ZXKeyb::putRows(0xFF);
    ZXKeyb::Exists = gpio_get_level((gpio_num_t)KM_COL_1) && gpio_get_level((gpio_num_t)KM_COL_2) && gpio_get_level((gpio_num_t)KM_COL_4);

}

void ZXKeyb::process() {

    // Put row pattern to 8-tap membrane connector (using shift register)
    // and read column pattern from 5-tap membrane connector.
    // row order depends on actual row association with address lines, see
    // https://www.1000bit.it/support/manuali/sinclair/zxspectrum/sm/matrix.gif
    putRows(0b11011111); ZXcols[0] = getCols();
    putRows(0b11111011); ZXcols[1] = getCols();
    putRows(0b11111101); ZXcols[2] = getCols();
    putRows(0b11111110); ZXcols[3] = getCols();
    putRows(0b11110111); ZXcols[4] = getCols();
    putRows(0b11101111); ZXcols[5] = getCols();
    putRows(0b10111111); ZXcols[6] = getCols();    
    putRows(0b01111111); ZXcols[7] = getCols();                        

}

// This function puts a row pattern into the membrane keyboard, for selecting a given row.
// Selection logic is active low, a 0 bit will select the row.
// A shift register is used for this task, so we'll need 3 output pins instead of 8.
void ZXKeyb::putRows(uint8_t row_pattern) {
    
    // NOTICE: many delays have been commented out.
    // If keyboard readings are erratic, maybe they should be recovered.

    gpio_set_level((gpio_num_t)SR_LOAD, 0); // disable load pin, keep previous output
    // delayMicroseconds(1);

    for (uint8_t i = 0; i < 8; i++) {   // traverse bits in byte

        gpio_set_level((gpio_num_t)SR_CLK, 0);  // clock falling edge
        // delayMicroseconds(1);

        gpio_set_level((gpio_num_t)SR_DATA, row_pattern & 0x80);    // put row bit to shift register serial input

        delayMicroseconds(1);   // just to be safe, wait just before rising edge

        gpio_set_level((gpio_num_t)SR_CLK, 1);  // rising edge occurs here
        // delayMicroseconds(1);

        row_pattern <<= 1;  // shift row bit pattern

    }

    gpio_set_level((gpio_num_t)SR_LOAD, 1); // enable load pin, update output

    // this sleep is MANDATORY, do NOT remove it
    // or else (first column bits read will be wrong)
    delayMicroseconds(1);

}

// This function reads all 5 columns from the corresponding GPIO pins and concatenates them
// into the lowest 5 bits of a byte.
uint8_t ZXKeyb::getCols() {

    uint8_t cols = gpio_get_level((gpio_num_t)KM_COL_4) << 1;
    cols |= gpio_get_level((gpio_num_t)KM_COL_3); cols <<= 1;
    cols |= gpio_get_level((gpio_num_t)KM_COL_2); cols <<= 1;
    cols |= gpio_get_level((gpio_num_t)KM_COL_1); cols <<= 1;
    cols |= gpio_get_level((gpio_num_t)KM_COL_0);   
    
    cols |= 0xa0;   // Keep bits 5,7 up
    
    return cols;

}
