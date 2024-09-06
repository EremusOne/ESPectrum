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
    putRows(0xFF);
    Exists = gpio_get_level((gpio_num_t)KM_COL_1) && gpio_get_level((gpio_num_t)KM_COL_2) && gpio_get_level((gpio_num_t)KM_COL_4);

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

void ZXKeyb::ZXKbdRead() {

    ZXKbdRead(ZXKDBREAD_MODEINTERACTIVE);

}

void ZXKeyb::ZXKbdRead(uint8_t mode) {

    #define REPDEL 140 // As in real ZX Spectrum (700 ms.) if this function is called every 5 ms. 
    #define REPPER 20 // As in real ZX Spectrum (100 ms.) if this function is called every 5 ms. 

    static int zxDel = REPDEL;
    static int lastzxK = fabgl::VK_NONE;
    static bool lastSSstatus = bitRead(ZXcols[7], 1);
    static bool lastCSstatus = bitRead(ZXcols[0], 0);

    process();

    fabgl::VirtualKey injectKey = fabgl::VK_NONE;

    if (mode == ZXKDBREAD_MODEINTERACTIVE && bitRead(ZXcols[7], 1)) { // Not Symbol Shift pressed ?

        if (!bitRead(ZXcols[4], 3)) injectKey = fabgl::VK_UP; // 7 -> UP
        else if (!bitRead(ZXcols[4], 4)) injectKey = fabgl::VK_DOWN; // 6 -> DOWN
        else if ((!bitRead(ZXcols[0], 0)) && (!bitRead(ZXcols[6], 0))) injectKey = fabgl::VK_JOY1C; // CS + ENTER -> SPACE / SELECT
        else if (!bitRead(ZXcols[6], 0)) injectKey = fabgl::VK_RETURN; // ENTER
        else if ((!bitRead(ZXcols[0], 0)) && (!bitRead(ZXcols[4], 0))) injectKey = fabgl::VK_BACKSPACE; // CS + 0 -> BACKSPACE
        else if (!bitRead(ZXcols[4], 0)) injectKey = fabgl::VK_RETURN; // 0 -> ENTER
        else if ((!bitRead(ZXcols[7], 0)) || (!bitRead(ZXcols[4], 1))) injectKey = fabgl::VK_ESCAPE; // BREAK -> ESCAPE
        else if (!bitRead(ZXcols[3], 4)) injectKey = fabgl::VK_LEFT; // 5 -> PGUP
        else if (!bitRead(ZXcols[4], 2)) injectKey = fabgl::VK_RIGHT; // 8 -> PGDOWN
        else if (!bitRead(ZXcols[7], 4)) injectKey = fabgl::VK_PRINTSCREEN; // B -> BMP CAPTURE
        else if (!bitRead(ZXcols[5], 0)) injectKey = fabgl::VK_PAUSE; // P -> PAUSE
        else if (!bitRead(ZXcols[7], 3)) injectKey = fabgl::VK_F2; // N -> NUEVO / RENOMBRAR
        else if (!bitRead(ZXcols[7], 2)) injectKey = fabgl::VK_F6; // M -> MOVE / MOVER        
        else if (!bitRead(ZXcols[1], 2)) injectKey = fabgl::VK_F8; // D -> DELETE / BORRAR                
        else if (!bitRead(ZXcols[1], 3)) injectKey = fabgl::VK_F3; // F -> FIND / BUSQUEDA

    } else {

        if (mode != ZXKDBREAD_MODEINTERACTIVE) {

            bool curSSstatus = bitRead(ZXcols[7], 1);
            if (lastSSstatus != curSSstatus) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_LCTRL, !curSSstatus, false);
                lastSSstatus = curSSstatus;
            }
            
            bool curCSstatus = bitRead(ZXcols[0], 0);
            if (lastCSstatus != curCSstatus) {
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_LSHIFT, !curCSstatus, false);
                lastCSstatus = curCSstatus;
            }

            if (!bitRead(ZXcols[6], 0) && bitRead(ZXcols[7], 1)) injectKey = fabgl::VK_RETURN; // ENTER
            else if ((!bitRead(ZXcols[0], 0)) && (!bitRead(ZXcols[4], 0))) injectKey = fabgl::VK_BACKSPACE; // CS + 0 -> BACKSPACE        
            else if (!bitRead(ZXcols[0], 0) && !bitRead(ZXcols[7], 0) && bitRead(ZXcols[7], 1)) injectKey = fabgl::VK_ESCAPE; // CS + SPACE -> ESCAPE

        }

        if (injectKey == fabgl::VK_NONE) {


            if (!bitRead(ZXcols[0], 1)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_Z : fabgl::VK_z;
            else if (!bitRead(ZXcols[7], 0)) injectKey = fabgl::VK_SPACE;
            else if (!bitRead(ZXcols[0], 2)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_X : fabgl::VK_x;
            else if (!bitRead(ZXcols[0], 3)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_C : fabgl::VK_c;
            else if (!bitRead(ZXcols[0], 4)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_V : fabgl::VK_v;

            else if (!bitRead(ZXcols[1], 0)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_A : fabgl::VK_a;
            else if (!bitRead(ZXcols[1], 1)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_S : fabgl::VK_s;
            else if (!bitRead(ZXcols[1], 2)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_D : fabgl::VK_d;
            else if (!bitRead(ZXcols[1], 3)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_F : fabgl::VK_f;
            else if (!bitRead(ZXcols[1], 4)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_G : fabgl::VK_g;

            else if (!bitRead(ZXcols[2], 0)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_Q : fabgl::VK_q;
            else if (!bitRead(ZXcols[2], 1)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_W : fabgl::VK_w;
            else if (!bitRead(ZXcols[2], 2)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_E : fabgl::VK_e;
            else if (!bitRead(ZXcols[2], 3)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_R : fabgl::VK_r;
            else if (!bitRead(ZXcols[2], 4)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_T : fabgl::VK_t;

            else if (!bitRead(ZXcols[5], 0)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_P : fabgl::VK_p;
            else if (!bitRead(ZXcols[5], 1)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_O : fabgl::VK_o;
            else if (!bitRead(ZXcols[5], 2)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_I : fabgl::VK_i;
            else if (!bitRead(ZXcols[5], 3)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_U : fabgl::VK_u;
            else if (!bitRead(ZXcols[5], 4)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_Y : fabgl::VK_y;

            else if (!bitRead(ZXcols[6], 1)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_L : fabgl::VK_l;
            else if (!bitRead(ZXcols[6], 2)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_K : fabgl::VK_k;
            else if (!bitRead(ZXcols[6], 3)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_J : fabgl::VK_j;
            else if (!bitRead(ZXcols[6], 4)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_H : fabgl::VK_h;

            else if (!bitRead(ZXcols[7], 2)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_M : fabgl::VK_m;
            else if (!bitRead(ZXcols[7], 3)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_N : fabgl::VK_n;
            else if (!bitRead(ZXcols[7], 4)) injectKey = !bitRead(ZXcols[0], 0) ? fabgl::VK_B : fabgl::VK_b;

            else if (!bitRead(ZXcols[3], 0)) injectKey = fabgl::VK_1;
            else if (!bitRead(ZXcols[3], 1)) injectKey = fabgl::VK_2;
            else if (!bitRead(ZXcols[3], 2)) injectKey = fabgl::VK_3;
            else if (!bitRead(ZXcols[3], 3)) injectKey = fabgl::VK_4;
            else if (!bitRead(ZXcols[3], 4)) injectKey = fabgl::VK_5;                        

            else if (!bitRead(ZXcols[4], 0)) injectKey = fabgl::VK_0;
            else if (!bitRead(ZXcols[4], 1)) injectKey = fabgl::VK_9;
            else if (!bitRead(ZXcols[4], 2)) injectKey = fabgl::VK_8;
            else if (!bitRead(ZXcols[4], 3)) injectKey = fabgl::VK_7;
            else if (!bitRead(ZXcols[4], 4)) injectKey = fabgl::VK_6;

        }

    }

    if (injectKey != fabgl::VK_NONE) {
        if (zxDel == 0) {
            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(injectKey, true, false);
            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(injectKey, false, false);
            zxDel = lastzxK == injectKey ? REPPER : REPDEL;
            lastzxK = injectKey;
        }
    } else {
        zxDel = 0;
        lastzxK = fabgl::VK_NONE;
    }

    if (zxDel > 0) zxDel--;

}
