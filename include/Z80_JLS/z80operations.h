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
    // Fetch opcode from RAM
    static uint8_t (*fetchOpcode)();
    static uint8_t fetchOpcode_2A3();
    static uint8_t fetchOpcode_std();

    // Read byte from RAM
    static uint8_t (*peek8)(uint16_t address);
    static uint8_t peek8_2A3(uint16_t address);
    static uint8_t peek8_std(uint16_t address);

    // Write byte to RAM
    static void (*poke8)(uint16_t address, uint8_t value);
    static void poke8_2A3(uint16_t address, uint8_t value);
    static void poke8_std(uint16_t address, uint8_t value);

    // Read word from RAM
    static uint16_t (*peek16)(uint16_t address);
    static uint16_t peek16_2A3(uint16_t adddress);
    static uint16_t peek16_std(uint16_t adddress);

    // Write word to RAM
    static void (*poke16)(uint16_t address, RegisterPair word);
    static void poke16_2A3(uint16_t address, RegisterPair word);
    static void poke16_std(uint16_t address, RegisterPair word);

    // Put an address on bus lasting 'tstates' cycles
    static void (*addressOnBus)(uint16_t address, int32_t wstates);
    static void addressOnBus_2A3(uint16_t address, int32_t wstates);
    static void addressOnBus_std(uint16_t address, int32_t wstates);

    // Callback to know when the INT signal is active
    static bool isActiveINT(void);

    static bool is48;
    static bool is128;
    static bool isPentagon;
    static bool is2a3;

};

#endif // Z80OPERATIONS_H
