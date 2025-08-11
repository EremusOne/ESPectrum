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

#include "ZXKeyb.h"
#include "ESPectrum.h"
#include "AudioIn.h"

// uint8_t ZXKeyb::ZXcols[8] = { 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf };
uint8_t ZXKeyb::ZXcols[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
uint8_t ZXKeyb::Exists = 0;

void ZXKeyb::setup() {

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

}

void ZXKeyb::check() {

    ZXKeyb::setup();

    // Check if membrane keyboard is present
    putRows(0xFF);
    bool gpi34 = true, gpi39 = true;
    for (int i=0; i < 20; i++) { // Check GPI34 and GPI39 value for ~1 ms. to ensure we do not get false positives from active real audio input signal
        if (!gpio_get_level((gpio_num_t)KM_COL_1)) gpi34 = false;
        if (!gpio_get_level((gpio_num_t)KM_COL_4)) gpi39 = false;
        delayMicroseconds(50);
    }
    bool gpi35 = gpio_get_level((gpio_num_t)KM_COL_2);

    // printf("GPI34: %d, GPI35: %d, GPI39: %d\n", gpi34, gpi35, gpi39);

    if (gpi34 && gpi35 && gpi39) Exists = 1;

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

    if (Config::AudioInMode && Config::AudioInGPIO == KM_COL_0) {
        // printf("Block col 0\n");
        cols |= 1;
    } else
        cols |= gpio_get_level((gpio_num_t)KM_COL_0);

    // cols |= 0xa0;   // Keep bits 5,7 up
    cols |= 0xe0;   // Keep bits 5,6,7 up

    return cols;

}

// void ZXKeyb::ZXKbdRead() {

//     ZXKbdRead(ZXKDBREAD_MODEINTERACTIVE);

// }

void ZXKeyb::ZXKbdRead(uint8_t mode) {

    #define REPDEL 140 // As in real ZX Spectrum (700 ms.) if this function is called every 5 ms.
    #define REPPER 20 // As in real ZX Spectrum (100 ms.) if this function is called every 5 ms.

    static int zxDel = REPDEL;
    static int lastzxK = fabgl::VK_NONE;
    static bool lastSSstatus = bitRead(ZXcols[7], 1);
    static bool lastCSstatus = bitRead(ZXcols[0], 0);
    bool capsshift = false;

    process();

    fabgl::VirtualKey injectKey = fabgl::VK_NONE;

    if (mode == ZXKDBREAD_MODEINTERACTIVE && bitRead(ZXcols[7], 1)) { // Not Symbol Shift pressed ?

        capsshift = ZXKBD_CS;

        if (capsshift) {
            if (ZXKBD_ENTER) injectKey = fabgl::VK_JOY1C; // CS + ENTER -> SPACE / SELECT
            else if (ZXKBD_0) injectKey = fabgl::VK_BACKSPACE; // CS + 0 -> BACKSPACE
            else if (ZXKBD_SPACE) injectKey = fabgl::VK_ESCAPE; // CS + BREAK/SPACE -> ESCAPE
            else if (ZXKBD_7) injectKey = fabgl::VK_UP; // 7 -> UP
            else if (ZXKBD_6) injectKey = fabgl::VK_DOWN; // 6 -> DOWN
            else if (ZXKBD_5) injectKey = fabgl::VK_LEFT; // 5 -> PGUP
            else if (ZXKBD_8) injectKey = fabgl::VK_RIGHT; // 8 -> PGDOWN
            else if (ZXKBD_N) injectKey = fabgl::VK_N; // N -> NEW
            else if (ZXKBD_M) injectKey = fabgl::VK_M; // M -> MOVE
            else if (ZXKBD_F) injectKey = fabgl::VK_F; // F -> FIND
            else if (ZXKBD_U) injectKey = fabgl::VK_U; // U -> BUSCAR
            else if (ZXKBD_P) injectKey = fabgl::VK_P; // P -> PROCURAR
            else if (ZXKBD_R) injectKey = fabgl::VK_R; // R -> RENAME
            else if (ZXKBD_D) injectKey = fabgl::VK_D; // D -> DELETE
            else if (ZXKBD_B) injectKey = fabgl::VK_B; // B -> BORRAR
        } else {
            if (ZXKBD_7) injectKey = fabgl::VK_UP; // 7 -> UP
            else if (ZXKBD_6) injectKey = fabgl::VK_DOWN; // 6 -> DOWN
            else if (ZXKBD_ENTER) injectKey = fabgl::VK_RETURN; // ENTER
            else if (ZXKBD_0) injectKey = fabgl::VK_RETURN; // 0 -> ENTER
            else if (ZXKBD_SPACE) injectKey = fabgl::VK_ESCAPE; // BREAK/SPACE -> ESCAPE
            else if (ZXKBD_9) injectKey = fabgl::VK_ESCAPE; // 9 -> ESCAPE
            else if (ZXKBD_5) injectKey = fabgl::VK_LEFT; // 5 -> PGUP
            else if (ZXKBD_8) injectKey = fabgl::VK_RIGHT; // 8 -> PGDOWN
            else if (ZXKBD_B) injectKey = fabgl::VK_PRINTSCREEN; // B -> BMP CAPTURE
            // else if (ZXKBD_P) injectKey = fabgl::VK_PAUSE; // P -> PAUSE
        }

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
            else if (ZXKBD_CS && !bitRead(ZXcols[4], 0)) injectKey = fabgl::VK_BACKSPACE; // CS + 0 -> BACKSPACE
            else if (ZXKBD_CS && !bitRead(ZXcols[7], 0) && bitRead(ZXcols[7], 1)) injectKey = fabgl::VK_ESCAPE; // CS + SPACE -> ESCAPE

        }

        if (injectKey == fabgl::VK_NONE) {

            if (ZXKBD_Z) injectKey = ZXKBD_CS ? fabgl::VK_Z : fabgl::VK_z;
            else if (ZXKBD_SPACE) injectKey = fabgl::VK_SPACE;
            else if (ZXKBD_X) injectKey = ZXKBD_CS ? fabgl::VK_X : fabgl::VK_x;
            else if (ZXKBD_C) injectKey = ZXKBD_CS ? fabgl::VK_C : fabgl::VK_c;
            else if (ZXKBD_V) injectKey = ZXKBD_CS ? fabgl::VK_V : fabgl::VK_v;

            else if (ZXKBD_A) injectKey = ZXKBD_CS ? fabgl::VK_A : fabgl::VK_a;
            else if (ZXKBD_S) injectKey = ZXKBD_CS ? fabgl::VK_S : fabgl::VK_s;
            else if (ZXKBD_D) injectKey = ZXKBD_CS ? fabgl::VK_D : fabgl::VK_d;
            else if (ZXKBD_F) injectKey = ZXKBD_CS ? fabgl::VK_F : fabgl::VK_f;
            else if (ZXKBD_G) injectKey = ZXKBD_CS ? fabgl::VK_G : fabgl::VK_g;

            else if (ZXKBD_Q) injectKey = ZXKBD_CS ? fabgl::VK_Q : fabgl::VK_q;
            else if (ZXKBD_W) injectKey = ZXKBD_CS ? fabgl::VK_W : fabgl::VK_w;
            else if (ZXKBD_E) injectKey = ZXKBD_CS ? fabgl::VK_E : fabgl::VK_e;
            else if (ZXKBD_R) injectKey = ZXKBD_CS ? fabgl::VK_R : fabgl::VK_r;
            else if (ZXKBD_T) injectKey = ZXKBD_CS ? fabgl::VK_T : fabgl::VK_t;

            else if (ZXKBD_P) injectKey = ZXKBD_CS ? fabgl::VK_P : fabgl::VK_p;
            else if (ZXKBD_O) injectKey = ZXKBD_CS ? fabgl::VK_O : fabgl::VK_o;
            else if (ZXKBD_I) injectKey = ZXKBD_CS ? fabgl::VK_I : fabgl::VK_i;
            else if (ZXKBD_U) injectKey = ZXKBD_CS ? fabgl::VK_U : fabgl::VK_u;
            else if (ZXKBD_Y) injectKey = ZXKBD_CS ? fabgl::VK_Y : fabgl::VK_y;

            else if (ZXKBD_L) injectKey = ZXKBD_CS ? fabgl::VK_L : fabgl::VK_l;
            else if (ZXKBD_K) injectKey = ZXKBD_CS ? fabgl::VK_K : fabgl::VK_k;
            else if (ZXKBD_J) injectKey = ZXKBD_CS ? fabgl::VK_J : fabgl::VK_j;
            else if (ZXKBD_H) injectKey = ZXKBD_CS ? fabgl::VK_H : fabgl::VK_h;

            else if (ZXKBD_M) injectKey = ZXKBD_CS ? fabgl::VK_M : fabgl::VK_m;
            else if (ZXKBD_N) injectKey = ZXKBD_CS ? fabgl::VK_N : fabgl::VK_n;
            else if (ZXKBD_B) injectKey = ZXKBD_CS ? fabgl::VK_B : fabgl::VK_b;

            else if (ZXKBD_1) injectKey = fabgl::VK_1;
            else if (ZXKBD_2) injectKey = fabgl::VK_2;
            else if (ZXKBD_3) injectKey = fabgl::VK_3;
            else if (ZXKBD_4) injectKey = fabgl::VK_4;
            else if (ZXKBD_5) injectKey = fabgl::VK_5;

            else if (ZXKBD_0) injectKey = fabgl::VK_0;
            else if (ZXKBD_9) injectKey = fabgl::VK_9;
            else if (ZXKBD_8) injectKey = fabgl::VK_8;
            else if (ZXKBD_7) injectKey = fabgl::VK_7;
            else if (ZXKBD_6) injectKey = fabgl::VK_6;

        }

    }

    if (injectKey != fabgl::VK_NONE) {
        if (zxDel == 0) {
            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(injectKey,  true, false, false, false, false, capsshift);
            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(injectKey, false, false, false, false, false, capsshift);
            zxDel = lastzxK == injectKey ? REPPER : REPDEL;
            lastzxK = injectKey;
        }
    } else {
        zxDel = 0;
        lastzxK = fabgl::VK_NONE;
    }

    if (zxDel > 0) zxDel--;

}
