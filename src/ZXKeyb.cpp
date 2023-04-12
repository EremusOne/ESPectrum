#include "ZXKeyb.h"
#include "ESPectrum.h"
#include "Ports.h"

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

    // set all keys as not pressed
    for (uint8_t i = 0; i < 8; i++)
        Ports::port[i] = 1;
}

static uint8_t roworder[8] = {
    0xDF,   // % 11011111 - should be $FE % 11111110
    0xFB,   // % 11111011 - should be $FD % 11111101
    0xFD,   // % 11111101 - should be $FB % 11111011
    0xFE,   // % 11111110 - should be $F7 % 11110111
    0xF7,   // % 11110111 - should be $EF % 11101111
    0xEF,   // % 11101111 - should be $DF % 11011111
    0xBF,   // % 10111111 - should be $BF % 10111111
    0x7F    // % 01111111 - should be $7F % 01111111
};

void ZXKeyb::process()
{
    for (uint8_t rowidx = 0; rowidx < 8; rowidx++)
    {
        uint8_t rows = roworder[rowidx];
        putRows(rows);
        uint8_t cols = getCols();
        Ports::port[rowidx] = cols;
    }
}

void ZXKeyb::putRows(uint8_t rows)
{
    // disable load pin, keep previous output
    gpio_set_level((gpio_num_t)SR_LOAD, 0);
    usleep(1);

    // traverse bits in byte
    for (uint8_t i = 0; i < 8; i++) {
        gpio_set_level((gpio_num_t)SR_CLK, 0);
        usleep(1);
        gpio_set_level((gpio_num_t)SR_DATA, rows & 0x80);
        usleep(1);
        gpio_set_level((gpio_num_t)SR_CLK, 1);
        usleep(1);
        rows <<= 1;
    }

    // enable load pin, update output
    gpio_set_level((gpio_num_t)SR_LOAD, 1);
    usleep(1);
}

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
    cols |= 0xE0;
    return cols;
}
