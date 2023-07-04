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
#include "Ports.h"

uint8_t ZXKeyb::ZXcols[8];
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

    // set all keys as not pressed
    if (ZXKeyb::Exists) for (uint8_t i = 0; i < 8; i++) ZXcols[i] = 0x1f;

}

// row order depends on actual row association with address lines, see
// https://www.1000bit.it/support/manuali/sinclair/zxspectrum/sm/matrix.gif
static uint8_t roworder[8] = {
    0xDF,   // % 11011111
    0xFB,   // % 11111011
    0xFD,   // % 11111101
    0xFE,   // % 11111110
    0xF7,   // % 11110111
    0xEF,   // % 11101111
    0xBF,   // % 10111111
    0x7F    // % 01111111
};

void ZXKeyb::process()
{
    // traverse row indices
    for (uint8_t rowidx = 0; rowidx < 8; rowidx++)
    {
        // first of all, take row pattern from table
        uint8_t row_pattern = roworder[rowidx];
        // put row pattern to 8-tap membrane connector
        // (using shift register)
        putRows(row_pattern);

        // read column pattern from 5-tap membrane connector
        // uint8_t cols = getCols();
        // // write column to port array at given row index
        // Ports::port[rowidx] = cols;

        // read column pattern from 5-tap membrane connector
        ZXcols[rowidx] = getCols();

    }
}

// This function puts a row pattern
// into the membrane keyboard, for selecting a given row.
// Selection logic is active low, a 0 bit will select the row.
// A shift register is used for this task,
// so we'll need 3 output pins instead of 8.
void ZXKeyb::putRows(uint8_t row_pattern)
{
    // NOTICE: many usleeps have been commented out.
    // If keyboard readings are erratic,
    // maybe they should be recovered.

    // disable load pin, keep previous output
    gpio_set_level((gpio_num_t)SR_LOAD, 0);
    // usleep(1);

    // traverse bits in byte
    for (uint8_t i = 0; i < 8; i++) {
        // clock falling edge
        gpio_set_level((gpio_num_t)SR_CLK, 0);
        // usleep(1);

        // put row bit to shift register serial input
        gpio_set_level((gpio_num_t)SR_DATA, row_pattern & 0x80);

        // just to be safe, wait just before rising edge
        delayMicroseconds(1);
        // usleep(1); 

        // rising edge occurs here
        gpio_set_level((gpio_num_t)SR_CLK, 1);
        // usleep(1);

        // shift row bit pattern
        row_pattern <<= 1;
    }

    // enable load pin, update output
    gpio_set_level((gpio_num_t)SR_LOAD, 1);

    // this sleep is MANDATORY, do NOT remove it
    // or else (first column bits read will be wrong)
    delayMicroseconds(1);
    // usleep(1);
}

// This function reads all 5 columns from the
// corresponding GPIO pins and concatenates them
// into the lowest 5 bits of a byte.
uint8_t ZXKeyb::getCols()
{
    uint8_t cols = 0;
    cols |= gpio_get_level((gpio_num_t)KM_COL_4);
    cols <<= 1;
    cols |= gpio_get_level((gpio_num_t)KM_COL_3);
    cols <<= 1;
    cols |= gpio_get_level((gpio_num_t)KM_COL_2);
    cols <<= 1;
    cols |= gpio_get_level((gpio_num_t)KM_COL_1);
    cols <<= 1;
    cols |= gpio_get_level((gpio_num_t)KM_COL_0);
    
    // cols |= 0xE0;
    
    return cols;
}
