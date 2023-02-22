///////////////////////////////////////////////////////////////////////////////
//
// z80cpp - Z80 emulator core
//
// Copyright (c) 2017, 2018, 2019, 2020 jsanchezv - https://github.com/jsanchezv
//
// Heretic optimizations and minor adaptations
// Copyright (c) 2021 dcrespo3d - https://github.com/dcrespo3d
//

#ifndef Z80OPERATIONS_H
#define Z80OPERATIONS_H

#include <stdint.h>
#include "esp_attr.h"

// Class grouping callbacks for Z80 operations
// methods declared here should be defined elsewhere... for example, in CPU.cpp

class Z80Ops
{
public:
    /* Read opcode from RAM */
    static uint8_t IRAM_ATTR fetchOpcode(uint16_t address);

    /* Read/Write byte from/to RAM */
    static uint8_t IRAM_ATTR peek8(uint16_t address);
    static void IRAM_ATTR poke8(uint16_t address, uint8_t value);

    /* Read/Write word from/to RAM */
    static uint16_t IRAM_ATTR peek16(uint16_t adddress);
    static void IRAM_ATTR poke16(uint16_t address, RegisterPair word);

    /* In/Out byte from/to IO Bus */
    static uint8_t IRAM_ATTR inPort(uint16_t port);
    static void IRAM_ATTR outPort(uint16_t port, uint8_t value);

    /* Put an address on bus lasting 'tstates' cycles */
    static void IRAM_ATTR addressOnBus(uint16_t address, int32_t wstates);

    /* Clocks needed for processing INT and NMI */
    static void IRAM_ATTR interruptHandlingTime(int32_t wstates);

    /* Callback to know when the INT signal is active */
    static bool IRAM_ATTR isActiveINT(void);

    /* Signal HALT in tstates */
    static void IRAM_ATTR signalHalt();

};

#endif // Z80OPERATIONS_H
