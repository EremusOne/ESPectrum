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

#include "../hardconfig.h"

#include <stdint.h>

// Class grouping callbacks for Z80 operations
// methods declared here should be defined elsewhere... for example, in CPU.cpp

class Z80Ops
{
public:
    /* Read opcode from RAM */
    static uint8_t fetchOpcode(uint16_t address);

    /* Read/Write byte from/to RAM */
    static uint8_t peek8(uint16_t address);
    static void poke8(uint16_t address, uint8_t value);

    /* Read/Write word from/to RAM */
    static uint16_t peek16(uint16_t adddress);
    static void poke16(uint16_t address, RegisterPair word);

    /* In/Out byte from/to IO Bus */
    static uint8_t inPort(uint16_t port);
    static void outPort(uint16_t port, uint8_t value);

    /* Put an address on bus lasting 'tstates' cycles */
    static void addressOnBus(uint16_t address, int32_t wstates);

    /* Clocks needed for processing INT and NMI */
    static void interruptHandlingTime(int32_t wstates);

    /* Callback to know when the INT signal is active */
    static bool isActiveINT(void);

    /* Add tStates and do ALU_video and audio buffer capture */
    static void addTstates(int32_t tstatestoadd, bool dovideo);

    /* Signal HALT in tstates */
    static void signalHalt();

};

#endif // Z80OPERATIONS_H
