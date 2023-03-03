///////////////////////////////////////////////////////////////////////////////
//
// z80cpp - Z80 emulator core
//
// Copyright (c) 2017, 2018, 2019, 2020 jsanchezv - https://github.com/jsanchezv
//
// Heretic optimizations and minor adaptations
// Copyright (c) 2021 dcrespo3d - https://github.com/dcrespo3d
//

// Converted to C++ from Java at
//... https://github.com/jsanchezv/Z80Core
//... commit c4f267e3564fa89bd88fd2d1d322f4d6b0069dbd
//... GPL 3
//... v1.0.0 (13/02/2017)
//    quick & dirty conversion by dddddd (AKA deesix)

#include "Z80_JLS/z80.h"

// #pragma GCC optimize ("Ofast")
#pragma GCC optimize ("O2")

///////////////////////////////////////////////////////////////////////////////
// miembros estáticos

uint8_t Z80::opCode;
uint8_t Z80::prefixOpcode = { 0x00 };
bool Z80::execDone;
uint8_t Z80::regA;
uint8_t Z80::sz5h3pnFlags;
bool Z80::carryFlag;
RegisterPair Z80::regBC, Z80::regBCx, Z80::regDE, Z80::regDEx, Z80::regHL, Z80::regHLx;
bool Z80::flagQ;
bool Z80::lastFlagQ;
RegisterPair Z80::regAFx;
RegisterPair Z80::regPC;
RegisterPair Z80::regIX;
RegisterPair Z80::regIY;
RegisterPair Z80::regSP;
uint8_t Z80::regI;
uint8_t Z80::regR;
bool Z80::regRbit7;
bool Z80::ffIFF1 = false;
bool Z80::ffIFF2 = false;
bool Z80::pendingEI = false;
bool Z80::activeNMI = false;
Z80::IntMode Z80::modeINT = Z80::IntMode::IM0;
bool Z80::halted = false;
bool Z80::pinReset = false;
RegisterPair Z80::memptr;
uint8_t Z80::sz53n_addTable[256];
uint8_t Z80::sz53pn_addTable[256];
uint8_t Z80::sz53n_subTable[256];
uint8_t Z80::sz53pn_subTable[256];

///////////////////////////////////////////////////////////////////////////////

// Constructor de la clase
void Z80::create() {

    bool evenBits;

    for (uint32_t idx = 0; idx < 256; idx++) {
		sz53n_addTable[idx] = 0;
		sz53pn_addTable[idx] = 0;
		sz53n_subTable[idx] = 0;
		sz53pn_subTable[idx] = 0;

		if (idx > 0x7f) {
            sz53n_addTable[idx] |= SIGN_MASK;
        }

        evenBits = true;
        for (uint8_t mask = 0x01; mask != 0; mask <<= 1) {
            if ((idx & mask) != 0) {
                evenBits = !evenBits;
            }
        }

        sz53n_addTable[idx] |= (idx & FLAG_53_MASK);
        sz53n_subTable[idx] = sz53n_addTable[idx] | ADDSUB_MASK;

        if (evenBits) {
            sz53pn_addTable[idx] = sz53n_addTable[idx] | PARITY_MASK;
            sz53pn_subTable[idx] = sz53n_subTable[idx] | PARITY_MASK;
        } else {
            sz53pn_addTable[idx] = sz53n_addTable[idx];
            sz53pn_subTable[idx] = sz53n_subTable[idx];
        }
    }

    sz53n_addTable[0] |= ZERO_MASK;
    sz53pn_addTable[0] |= ZERO_MASK;
    sz53n_subTable[0] |= ZERO_MASK;
    sz53pn_subTable[0] |= ZERO_MASK;

    execDone = false;
    reset();
}

void Z80::destroy(void)
{
}

RegisterPair Z80::getPairIR(void) {
    RegisterPair IR;
    IR.byte8.hi = regI;
    IR.byte8.lo = regR & 0x7f;
    if (regRbit7) {
        IR.byte8.lo |= SIGN_MASK;
    }
    return IR;
}

void Z80::setAddSubFlag(bool state) {
    if (state) {
        sz5h3pnFlags |= ADDSUB_MASK;
    } else {
        sz5h3pnFlags &= ~ADDSUB_MASK;
    }
}

void Z80::setParOverFlag(bool state) {
    if (state) {
        sz5h3pnFlags |= PARITY_MASK;
    } else {
        sz5h3pnFlags &= ~PARITY_MASK;
    }
}

void Z80::setBit3Fag(bool state) {
    if (state) {
        sz5h3pnFlags |= BIT3_MASK;
    } else {
        sz5h3pnFlags &= ~BIT3_MASK;
    }
}

void Z80::setHalfCarryFlag(bool state) {
    if (state) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    } else {
        sz5h3pnFlags &= ~HALFCARRY_MASK;
    }
}

void Z80::setBit5Flag(bool state) {
    if (state) {
        sz5h3pnFlags |= BIT5_MASK;
    } else {
        sz5h3pnFlags &= ~BIT5_MASK;
    }
}

void Z80::setZeroFlag(bool state) {
    if (state) {
        sz5h3pnFlags |= ZERO_MASK;
    } else {
        sz5h3pnFlags &= ~ZERO_MASK;
    }
}

void Z80::setSignFlag(bool state) {
    if (state) {
        sz5h3pnFlags |= SIGN_MASK;
    } else {
        sz5h3pnFlags &= ~SIGN_MASK;
    }
}

// Reset
/* Según el documento de Sean Young, que se encuentra en
 * [http://www.myquest.com/z80undocumented], la mejor manera de emular el
 * reset es poniendo PC, IFF1, IFF2, R e IM0 a 0 y todos los demás registros
 * a 0xFFFF.
 *
 * 29/05/2011: cuando la CPU recibe alimentación por primera vez, los
 *             registros PC e IR se inicializan a cero y el resto a 0xFF.
 *             Si se produce un reset a través de la patilla correspondiente,
 *             los registros PC e IR se inicializan a 0 y el resto se preservan.
 *             En cualquier caso, todo parece depender bastante del modelo
 *             concreto de Z80, así que se escoge el comportamiento del
 *             modelo Zilog Z8400APS. Z80A CPU.
 *             http://www.worldofspectrum.org/forums/showthread.php?t=34574
 */
void Z80::reset(void) {
    if (pinReset) {
        pinReset = false;
    } else {
        regA = 0xff;
        
        setFlags(0xfd); // The only one flag reset at cold start is the add/sub flag

        REG_AFx = 0xffff;
        REG_BC = REG_BCx = 0xffff;
        REG_DE = REG_DEx = 0xffff;
        REG_HL = REG_HLx = 0xffff;

        REG_IX = REG_IY = 0xffff;

        REG_SP = 0xffff;

        REG_WZ = 0xffff;
    }

    REG_PC = 0;
    regI = regR = 0;
    regRbit7 = false;
    ffIFF1 = false;
    ffIFF2 = false;
    pendingEI = false;
    activeNMI = false;
    halted = false;
    setIM(IntMode::IM0);
    lastFlagQ = false;
    prefixOpcode = 0x00;
}

// Rota a la izquierda el valor del argumento
// El bit 0 y el flag C toman el valor del bit 7 antes de la operación
void Z80::rlc(uint8_t &oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    if (carryFlag) {
        oper8 |= CARRY_MASK;
    }
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}
// Rota a la izquierda el valor del argumento
// El bit 7 va al carry flag
// El bit 0 toma el valor del flag C antes de la operación
void Z80::rl(uint8_t &oper8) {
    bool carry = carryFlag;
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    if (carry) {
        oper8 |= CARRY_MASK;
    }
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la izquierda el valor del argumento
// El bit 7 va al carry flag
// El bit 0 toma el valor 0
void Z80::sla(uint8_t &oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la izquierda el valor del argumento (como sla salvo por el bit 0)
// El bit 7 va al carry flag
// El bit 0 toma el valor 1
// Instrucción indocumentada
void Z80::sll(uint8_t &oper8) {
    carryFlag = (oper8 > 0x7f);
    oper8 <<= 1;
    oper8 |= CARRY_MASK;
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la derecha el valor del argumento
// El bit 7 y el flag C toman el valor del bit 0 antes de la operación
void Z80::rrc(uint8_t &oper8) {
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 >>= 1;
    if (carryFlag) {
        oper8 |= SIGN_MASK;
    }
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la derecha el valor del argumento
// El bit 0 va al carry flag
// El bit 7 toma el valor del flag C antes de la operación
void Z80::rr(uint8_t &oper8) {
    bool carry = carryFlag;
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 >>= 1;
    if (carry) {
        oper8 |= SIGN_MASK;
    }
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la derecha 1 bit el valor del argumento
// El bit 0 pasa al carry.
// El bit 7 conserva el valor que tenga
void Z80::sra(uint8_t &oper8) {
    uint8_t sign = oper8 & SIGN_MASK;
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 = (oper8 >> 1) | sign;
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

// Rota a la derecha 1 bit el valor del argumento
// El bit 0 pasa al carry.
// El bit 7 toma el valor 0
void Z80::srl(uint8_t &oper8) {
    carryFlag = (oper8 & CARRY_MASK) != 0;
    oper8 >>= 1;
    sz5h3pnFlags = sz53pn_addTable[oper8];
    flagQ = true;
}

/*
 * Half-carry flag:
 *
 * FLAG = (A ^ B ^ RESULT) & 0x10  for any operation
 *
 * Overflow flag:
 *
 * FLAG = ~(A ^ B) & (B ^ RESULT) & 0x80 for addition [ADD/ADC]
 * FLAG = (A ^ B) & (A ^ RESULT) &0x80 for subtraction [SUB/SBC]
 *
 * For INC/DEC, you can use following simplifications:
 *
 * INC:
 * H_FLAG = (RESULT & 0x0F) == 0x00
 * V_FLAG = RESULT == 0x80
 *
 * DEC:
 * H_FLAG = (RESULT & 0x0F) == 0x0F
 * V_FLAG = RESULT == 0x7F
 */
// Incrementa un valor de 8 bits modificando los flags oportunos
void Z80::inc8(uint8_t &oper8) {
    oper8++;

    sz5h3pnFlags = sz53n_addTable[oper8];

    if ((oper8 & 0x0f) == 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (oper8 == 0x80) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    flagQ = true;
    return;
}

// Decrementa un valor de 8 bits modificando los flags oportunos
void Z80::dec8(uint8_t &oper8) {
    oper8--;

    sz5h3pnFlags = sz53n_subTable[oper8];

    if ((oper8 & 0x0f) == 0x0f) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (oper8 == 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    flagQ = true;
    return;
}

// Suma de 8 bits afectando a los flags
void Z80::add(uint8_t oper8) {
    uint16_t res = regA + oper8;

    carryFlag = res > 0xff;
    res &= 0xff;
    sz5h3pnFlags = sz53n_addTable[res];

    /* El módulo 16 del resultado será menor que el módulo 16 del registro A
     * si ha habido HalfCarry. Sucede lo mismo para todos los métodos suma
     * SIN carry */
    if ((res & 0x0f) < (regA & 0x0f)) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ ~oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    regA = res;
    flagQ = true;
}

// Suma con acarreo de 8 bits
void Z80::adc(uint8_t oper8) {
    uint16_t res = regA + oper8;

    if (carryFlag) {
        res++;
    }

    carryFlag = res > 0xff;
    res &= 0xff;
    sz5h3pnFlags = sz53n_addTable[res];

    if (((regA ^ oper8 ^ res) & 0x10) != 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ ~oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    regA = res;
    flagQ = true;
}

// Suma dos operandos de 16 bits sin carry afectando a los flags
void Z80::add16(RegisterPair &reg16, uint16_t oper16) {
    uint32_t tmp = oper16 + reg16.word;

    REG_WZ = reg16.word + 1;
    carryFlag = tmp > 0xffff;
    reg16.word = tmp;
    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | ((reg16.word >> 8) & FLAG_53_MASK);

    if ((reg16.word & 0x0fff) < (oper16 & 0x0fff)) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    flagQ = true;
    return;
}

// Suma con acarreo de 16 bits
void Z80::adc16(uint16_t reg16) {
    uint16_t tmpHL = REG_HL;
    REG_WZ = REG_HL + 1;

    uint32_t res = REG_HL + reg16;
    if (carryFlag) {
        res++;
    }

    carryFlag = res > 0xffff;
    res &= 0xffff;
    REG_HL = (uint16_t) res;

    sz5h3pnFlags = sz53n_addTable[REG_H];
    if (res != 0) {
        sz5h3pnFlags &= ~ZERO_MASK;
    }

    if (((res ^ tmpHL ^ reg16) & 0x1000) != 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((tmpHL ^ ~reg16) & (tmpHL ^ res)) > 0x7fff) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    flagQ = true;
}

// Resta de 8 bits
void Z80::sub(uint8_t oper8) {
    int16_t res = regA - oper8;

    carryFlag = res < 0;
    res &= 0xff;
    sz5h3pnFlags = sz53n_subTable[res];

    /* El módulo 16 del resultado será mayor que el módulo 16 del registro A
     * si ha habido HalfCarry. Sucede lo mismo para todos los métodos resta
     * SIN carry, incluido cp */
    if ((res & 0x0f) > (regA & 0x0f)) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    regA = res;
    flagQ = true;
}

// Resta con acarreo de 8 bits
void Z80::sbc(uint8_t oper8) {
    int16_t res = regA - oper8;

    if (carryFlag) {
        res--;
    }

    carryFlag = res < 0;
    res &= 0xff;
    sz5h3pnFlags = sz53n_subTable[res];

    if (((regA ^ oper8 ^ res) & 0x10) != 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    regA = res;
    flagQ = true;
}

// Resta con acarreo de 16 bits
void Z80::sbc16(uint16_t reg16) {
    uint16_t tmpHL = REG_HL;
    REG_WZ = REG_HL + 1;

    int32_t res = REG_HL - reg16;
    if (carryFlag) {
        res--;
    }

    carryFlag = res < 0;
    res &= 0xffff;
    REG_HL = (uint16_t) res;

    sz5h3pnFlags = sz53n_subTable[REG_H];
    if (res != 0) {
        sz5h3pnFlags &= ~ZERO_MASK;
    }

    if (((res ^ tmpHL ^ reg16) & 0x1000) != 0) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((tmpHL ^ reg16) & (tmpHL ^ res)) > 0x7fff) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }
    flagQ = true;
}

// Operación AND lógica
void Z80::and_(uint8_t oper8) {
    regA &= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA] | HALFCARRY_MASK;
    flagQ = true;
}

// Operación XOR lógica
void Z80::xor_(uint8_t oper8) {
    regA ^= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA];
    flagQ = true;
}

// Operación OR lógica
void Z80::or_(uint8_t oper8) {
    regA |= oper8;
    carryFlag = false;
    sz5h3pnFlags = sz53pn_addTable[regA];
    flagQ = true;
}

// Operación de comparación con el registro A
// es como SUB, pero solo afecta a los flags
// Los flags SIGN y ZERO se calculan a partir del resultado
// Los flags 3 y 5 se copian desde el operando (sigh!)
void Z80::cp(uint8_t oper8) {
    int16_t res = regA - oper8;

    carryFlag = res < 0;
    res &= 0xff;

    sz5h3pnFlags = (sz53n_addTable[oper8] & FLAG_53_MASK)
            | // No necesito preservar H, pero está a 0 en la tabla de todas formas
            (sz53n_subTable[res] & FLAG_SZHN_MASK);

    if ((res & 0x0f) > (regA & 0x0f)) {
        sz5h3pnFlags |= HALFCARRY_MASK;
    }

    if (((regA ^ oper8) & (regA ^ res)) > 0x7f) {
        sz5h3pnFlags |= OVERFLOW_MASK;
    }

    flagQ = true;
}

// DAA
void Z80::daa(void) {
    uint8_t suma = 0;
    bool carry = carryFlag;

    if ((sz5h3pnFlags & HALFCARRY_MASK) != 0 || (regA & 0x0f) > 0x09) {
        suma = 6;
    }

    if (carry || (regA > 0x99)) {
        suma |= 0x60;
    }

    if (regA > 0x99) {
        carry = true;
    }

    if ((sz5h3pnFlags & ADDSUB_MASK) != 0) {
        sub(suma);
        sz5h3pnFlags = (sz5h3pnFlags & HALFCARRY_MASK) | sz53pn_subTable[regA];
    } else {
        add(suma);
        sz5h3pnFlags = (sz5h3pnFlags & HALFCARRY_MASK) | sz53pn_addTable[regA];
    }

    carryFlag = carry;
    // Los add/sub ya ponen el resto de los flags
    flagQ = true;
}

// POP
uint16_t Z80::pop(void) {
    uint16_t word = Z80Ops::peek16(REG_SP);
    REG_SP = REG_SP + 2;
    return word;
}

// PUSH
void Z80::push(uint16_t word) {
    Z80Ops::poke8(--REG_SP, word >> 8);
    Z80Ops::poke8(--REG_SP, word);
}

// LDI
void Z80::ldi(void) {
    uint8_t work8 = Z80Ops::peek8(REG_HL);
    Z80Ops::poke8(REG_DE, work8);
    Z80Ops::addressOnBus(REG_DE, 2);
    REG_HL++;
    REG_DE++;
    REG_BC--;
    work8 += regA;

    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZ_MASK) | (work8 & BIT3_MASK);

    if ((work8 & ADDSUB_MASK) != 0) {
        sz5h3pnFlags |= BIT5_MASK;
    }

    if (REG_BC != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    }
    flagQ = true;
}

// LDD
void Z80::ldd(void) {
    uint8_t work8 = Z80Ops::peek8(REG_HL);
    Z80Ops::poke8(REG_DE, work8);
    Z80Ops::addressOnBus(REG_DE, 2);
    REG_HL--;
    REG_DE--;
    REG_BC--;
    work8 += regA;

    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZ_MASK) | (work8 & BIT3_MASK);

    if ((work8 & ADDSUB_MASK) != 0) {
        sz5h3pnFlags |= BIT5_MASK;
    }

    if (REG_BC != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    }
    flagQ = true;
}

// CPI
void Z80::cpi(void) {
    uint8_t memHL = Z80Ops::peek8(REG_HL);
    bool carry = carryFlag; // lo guardo porque cp lo toca
    cp(memHL);
    carryFlag = carry;
    Z80Ops::addressOnBus(REG_HL, 5);
    REG_HL++;
    REG_BC--;
    memHL = regA - memHL - ((sz5h3pnFlags & HALFCARRY_MASK) != 0 ? 1 : 0);
    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHN_MASK) | (memHL & BIT3_MASK);

    if ((memHL & ADDSUB_MASK) != 0) {
        sz5h3pnFlags |= BIT5_MASK;
    }

    if (REG_BC != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    }

    REG_WZ++;
    flagQ = true;
}

// CPD
void Z80::cpd(void) {
    uint8_t memHL = Z80Ops::peek8(REG_HL);
    bool carry = carryFlag; // lo guardo porque cp lo toca
    cp(memHL);
    carryFlag = carry;
    Z80Ops::addressOnBus(REG_HL, 5);
    REG_HL--;
    REG_BC--;
    memHL = regA - memHL - ((sz5h3pnFlags & HALFCARRY_MASK) != 0 ? 1 : 0);
    sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHN_MASK) | (memHL & BIT3_MASK);

    if ((memHL & ADDSUB_MASK) != 0) {
        sz5h3pnFlags |= BIT5_MASK;
    }

    if (REG_BC != 0) {
        sz5h3pnFlags |= PARITY_MASK;
    }

    REG_WZ--;
    flagQ = true;
}

// INI
void Z80::ini(void) {
    REG_WZ = REG_BC;
    Z80Ops::addressOnBus(getPairIR().word, 1);
    uint8_t work8 = Z80Ops::inPort(REG_WZ++);
    Z80Ops::poke8(REG_HL, work8);

    REG_B--;
    REG_HL++;

    sz5h3pnFlags = sz53pn_addTable[REG_B];
    if (work8 > 0x7f) {
        sz5h3pnFlags |= ADDSUB_MASK;
    }

    carryFlag = false;
    uint16_t tmp = work8 + REG_C + 1;
    if (tmp > 0xff) {
        sz5h3pnFlags |= HALFCARRY_MASK;
        carryFlag = true;
    }

    if ((sz53pn_addTable[((tmp & 0x07) ^ REG_B)]
            & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    } else {
        sz5h3pnFlags &= ~PARITY_MASK;
    }
    flagQ = true;
}

// IND
void Z80::ind(void) {
    REG_WZ = REG_BC;
    Z80Ops::addressOnBus(getPairIR().word, 1);
    uint8_t work8 = Z80Ops::inPort(REG_WZ--);
    Z80Ops::poke8(REG_HL, work8);

    REG_B--;
    REG_HL--;

    sz5h3pnFlags = sz53pn_addTable[REG_B];
    if (work8 > 0x7f) {
        sz5h3pnFlags |= ADDSUB_MASK;
    }

    carryFlag = false;
    uint16_t tmp = work8 + (REG_C - 1);
    if (tmp > 0xff) {
        sz5h3pnFlags |= HALFCARRY_MASK;
        carryFlag = true;
    }

    if ((sz53pn_addTable[((tmp & 0x07) ^ REG_B)]
            & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    } else {
        sz5h3pnFlags &= ~PARITY_MASK;
    }
    flagQ = true;
}

// OUTI
void Z80::outi(void) {

    Z80Ops::addressOnBus(getPairIR().word, 1);

    REG_B--;
    REG_WZ = REG_BC;

    uint8_t work8 = Z80Ops::peek8(REG_HL);
    Z80Ops::outPort(REG_WZ++, work8);

    REG_HL++;

    carryFlag = false;
    if (work8 > 0x7f) {
        sz5h3pnFlags = sz53n_subTable[REG_B];
    } else {
        sz5h3pnFlags = sz53n_addTable[REG_B];
    }

    if ((REG_L + work8) > 0xff) {
        sz5h3pnFlags |= HALFCARRY_MASK;
        carryFlag = true;
    }

    if ((sz53pn_addTable[(((REG_L + work8) & 0x07) ^ REG_B)]
            & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    }
    flagQ = true;
}

// OUTD
void Z80::outd(void) {

    Z80Ops::addressOnBus(getPairIR().word, 1);

    REG_B--;
    REG_WZ = REG_BC;

    uint8_t work8 = Z80Ops::peek8(REG_HL);
    Z80Ops::outPort(REG_WZ--, work8);

    REG_HL--;

    carryFlag = false;
    if (work8 > 0x7f) {
        sz5h3pnFlags = sz53n_subTable[REG_B];
    } else {
        sz5h3pnFlags = sz53n_addTable[REG_B];
    }

    if ((REG_L + work8) > 0xff) {
        sz5h3pnFlags |= HALFCARRY_MASK;
        carryFlag = true;
    }

    if ((sz53pn_addTable[(((REG_L + work8) & 0x07) ^ REG_B)]
            & PARITY_MASK) == PARITY_MASK) {
        sz5h3pnFlags |= PARITY_MASK;
    }
    flagQ = true;
}

// Pone a 1 el Flag Z si el bit b del registro
// r es igual a 0
/*
 * En contra de lo que afirma el Z80-Undocumented, los bits 3 y 5 toman
 * SIEMPRE el valor de los bits correspondientes del valor a comparar para
 * las instrucciones BIT n,r. Para BIT n,(HL) toman el valor del registro
 * escondido (REG_WZ), y para las BIT n, (IX/IY+n) toman el valor de los
 * bits superiores de la dirección indicada por IX/IY+n.
 *
 * 04/12/08 Confirmado el comentario anterior:
 *          http://scratchpad.wikia.com/wiki/Z80
 */
void Z80::bitTest(uint8_t mask, uint8_t reg) {
    bool zeroFlag = (mask & reg) == 0;

    sz5h3pnFlags = (sz53n_addTable[reg] & ~FLAG_SZP_MASK) | HALFCARRY_MASK;

    if (zeroFlag) {
        sz5h3pnFlags |= (PARITY_MASK | ZERO_MASK);
    }

    if (mask == SIGN_MASK && !zeroFlag) {
        sz5h3pnFlags |= SIGN_MASK;
    }
    flagQ = true;
}

//Interrupción
/* Desglose de la interrupción, según el modo:
 * IM0:
 *      M1: 7 T-Estados -> reconocer INT y decSP
 *      M2: 3 T-Estados -> escribir byte alto y decSP
 *      M3: 3 T-Estados -> escribir byte bajo y salto a N
 * IM1:
 *      M1: 7 T-Estados -> reconocer INT y decSP
 *      M2: 3 T-Estados -> escribir byte alto PC y decSP
 *      M3: 3 T-Estados -> escribir byte bajo PC y PC=0x0038
 * IM2:
 *      M1: 7 T-Estados -> reconocer INT y decSP
 *      M2: 3 T-Estados -> escribir byte alto y decSP
 *      M3: 3 T-Estados -> escribir byte bajo
 *      M4: 3 T-Estados -> leer byte bajo del vector de INT
 *      M5: 3 T-Estados -> leer byte alto y saltar a la rutina de INT
 */
void Z80::interrupt(void) {
    
    lastFlagQ = false;
    
    // Si estaba en un HALT esperando una INT, lo saca de la espera
    if (halted) {
        halted = false;
        REG_PC++;
    }

    Z80Ops::interruptHandlingTime(7);

    regR++;

    ffIFF1 = ffIFF2 = false;
    push(REG_PC); // el push añadirá 6 t-estados (+contended si toca)
    if (modeINT == IntMode::IM2) {
        REG_PC = Z80Ops::peek16((regI << 8) | 0xff); // +6 t-estados
    } else {
        REG_PC = 0x0038;
    }
    REG_WZ = REG_PC;
}

//Interrupción NMI, no utilizado por ahora
/* Desglose de ciclos de máquina y T-Estados
 * M1: 5 T-Estados -> extraer opcode (pá ná, es tontería) y decSP
 * M2: 3 T-Estados -> escribe byte alto de PC y decSP
 * M3: 3 T-Estados -> escribe byte bajo de PC y PC=0x0066
 */
void Z80::nmi(void) {
    // Esta lectura consigue dos cosas:
    //      1.- La lectura del opcode del M1 que se descarta
    //      2.- Si estaba en un HALT esperando una INT, lo saca de la espera
    Z80Ops::fetchOpcode(REG_PC);
    Z80Ops::interruptHandlingTime(1);
    if (halted) {
        halted = false;
        REG_PC++;
    }
    regR++;
    ffIFF1 = false;
    push(REG_PC); // 3+3 t-estados + contended si procede
    REG_PC = REG_WZ = 0x0066;
}

void Z80::checkINT(void) {

    // Ahora se comprueba si está activada la señal INT
    if (ffIFF1 && !pendingEI && Z80Ops::isActiveINT()) {
        interrupt();
    }

}

void Z80::incRegR(uint8_t inc) {

    regR += inc;

}

void Z80::execute(void) {

    opCode = Z80Ops::fetchOpcode(REG_PC);
    regR++;

#ifdef WITH_BREAKPOINT_SUPPORT
    if (breakpointEnabled && prefixOpcode == 0) {
        opCode = Z80Ops::breakpoint(REG_PC, opCode);
    }
#endif
    
    REG_PC++;

    // El prefijo 0xCB no cuenta para esta guerra.
    // En CBxx todas las xx producen un código válido
    // de instrucción, incluyendo CBCB.
    switch (prefixOpcode) {
        case 0x00:
            flagQ = pendingEI = false;
            decodeOpcode(opCode);
            break;
        case 0xDD:
            prefixOpcode = 0;
            decodeDDFD(opCode, regIX);
            break;
        case 0xED:
            prefixOpcode = 0;
            decodeED(opCode);
            break;
        case 0xFD:
            prefixOpcode = 0;
            decodeDDFD(opCode, regIY);
            break;
        default:
            return;
    }

    if (prefixOpcode != 0) return;

    lastFlagQ = flagQ;

#ifdef WITH_EXEC_DONE
    if (execDone) {
        Z80Ops::execDone();
    }
#endif
    
    // // Primero se comprueba NMI
    // // Si se activa NMI no se comprueba INT porque la siguiente
    // // instrucción debe ser la de 0x0066.
    // if (activeNMI) {
    //     activeNMI = false;
    //     lastFlagQ = false;
    //     nmi();
    //     return;
    // }

    // Ahora se comprueba si está activada la señal INT
    if (ffIFF1 && !pendingEI && Z80Ops::isActiveINT()) {
        interrupt();
    }

}

void Z80::decodeOpcode(uint8_t opCode) {

    switch (opCode) {
        case 0x00:
        { /* NOP */
            break;
        }
        case 0x01:
        { /* LD BC,nn */
            REG_BC = Z80Ops::peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x02:
        { /* LD (BC),A */
            Z80Ops::poke8(REG_BC, regA);
            REG_W = regA;
            REG_Z = REG_C + 1;
            //REG_WZ = (regA << 8) | (REG_C + 1);
            break;
        }
        case 0x03:
        { /* INC BC */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_BC++;
            break;
        }
        case 0x04:
        { /* INC B */
            inc8(REG_B);
            break;
        }
        case 0x05:
        { /* DEC B */
            dec8(REG_B);
            break;
        }
        case 0x06:
        { /* LD B,n */
            REG_B = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x07:
        { /* RLCA */
            carryFlag = (regA > 0x7f);
            regA <<= 1;
            if (carryFlag) {
                regA |= CARRY_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            flagQ = true;
            break;
        }
        case 0x08:
        { /* EX AF,AF' */
            uint8_t work8 = regA;
            regA = REG_Ax;
            REG_Ax = work8;

            work8 = getFlags();
            setFlags(REG_Fx);
            REG_Fx = work8;
            break;
        }
        case 0x09:
        { /* ADD HL,BC */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            add16(regHL, REG_BC);
            break;
        }
        case 0x0A:
        { /* LD A,(BC) */
            regA = Z80Ops::peek8(REG_BC);
            REG_WZ = REG_BC + 1;
            break;
        }
        case 0x0B:
        { /* DEC BC */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_BC--;
            break;
        }
        case 0x0C:
        { /* INC C */
            inc8(REG_C);
            break;
        }
        case 0x0D:
        { /* DEC C */
            dec8(REG_C);
            break;
        }
        case 0x0E:
        { /* LD C,n */
            REG_C = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x0F:
        { /* RRCA */
            carryFlag = (regA & CARRY_MASK) != 0;
            regA >>= 1;
            if (carryFlag) {
                regA |= SIGN_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            flagQ = true;
            break;
        }
        case 0x10:
        { /* DJNZ e */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            int8_t offset = Z80Ops::peek8(REG_PC);
            if (--REG_B != 0) {
                Z80Ops::addressOnBus(REG_PC, 5);
                REG_PC = REG_WZ = REG_PC + offset + 1;
            } else {
                REG_PC++;
            }
            break;
        }
        case 0x11:
        { /* LD DE,nn */
            REG_DE = Z80Ops::peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x12:
        { /* LD (DE),A */
            Z80Ops::poke8(REG_DE, regA);
            REG_W = regA;
            REG_Z = REG_E + 1;
            //REG_WZ = (regA << 8) | (REG_E + 1);
            break;
        }
        case 0x13:
        { /* INC DE */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_DE++;
            break;
        }
        case 0x14:
        { /* INC D */
            inc8(REG_D);
            break;
        }
        case 0x15:
        { /* DEC D */
            dec8(REG_D);
            break;
        }
        case 0x16:
        { /* LD D,n */
            REG_D = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x17:
        { /* RLA */
            bool oldCarry = carryFlag;
            carryFlag = regA > 0x7f;
            regA <<= 1;
            if (oldCarry) {
                regA |= CARRY_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            flagQ = true;
            break;
        }
        case 0x18:
        { /* JR e */
            int8_t offset = Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC = REG_WZ = REG_PC + offset + 1;
            break;
        }
        case 0x19:
        { /* ADD HL,DE */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            add16(regHL, REG_DE);
            break;
        }
        case 0x1A:
        { /* LD A,(DE) */
            regA = Z80Ops::peek8(REG_DE);
            REG_WZ = REG_DE + 1;
            break;
        }
        case 0x1B:
        { /* DEC DE */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_DE--;
            break;
        }
        case 0x1C:
        { /* INC E */
            inc8(REG_E);
            break;
        }
        case 0x1D:
        { /* DEC E */
            dec8(REG_E);
            break;
        }
        case 0x1E:
        { /* LD E,n */
            REG_E = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x1F:
        { /* RRA */
            bool oldCarry = carryFlag;
            carryFlag = (regA & CARRY_MASK) != 0;
            regA >>= 1;
            if (oldCarry) {
                regA |= SIGN_MASK;
            }
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
            flagQ = true;
            break;
        }
        case 0x20:
        { /* JR NZ,e */
            int8_t offset = Z80Ops::peek8(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                Z80Ops::addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            break;
        }
        case 0x21:
        { /* LD HL,nn */
            REG_HL = Z80Ops::peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x22:
        { /* LD (nn),HL */
            REG_WZ = Z80Ops::peek16(REG_PC);
            Z80Ops::poke16(REG_WZ, regHL);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x23:
        { /* INC HL */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_HL++;
            break;
        }
        case 0x24:
        { /* INC H */
            inc8(REG_H);
            break;
        }
        case 0x25:
        { /* DEC H */
            dec8(REG_H);
            break;
        }
        case 0x26:
        { /* LD H,n */
            REG_H = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x27:
        { /* DAA */
            daa();
            break;
        }
        case 0x28:
        { /* JR Z,e */
            int8_t offset = Z80Ops::peek8(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                Z80Ops::addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            break;
        }
        case 0x29:
        { /* ADD HL,HL */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            add16(regHL, REG_HL);
            break;
        }
        case 0x2A:
        { /* LD HL,(nn) */
            REG_WZ = Z80Ops::peek16(REG_PC);
            REG_HL = Z80Ops::peek16(REG_WZ);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x2B:
        { /* DEC HL */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_HL--;
            break;
        }
        case 0x2C:
        { /* INC L */
            inc8(REG_L);
            break;
        }
        case 0x2D:
        { /* DEC L */
            dec8(REG_L);
            break;
        }
        case 0x2E:
        { /* LD L,n */
            REG_L = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x2F:
        { /* CPL */
            regA ^= 0xff;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | HALFCARRY_MASK
                    | (regA & FLAG_53_MASK) | ADDSUB_MASK;
            flagQ = true;
            break;
        }
        case 0x30:
        { /* JR NC,e */
            int8_t offset = Z80Ops::peek8(REG_PC);
            if (!carryFlag) {
                Z80Ops::addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            break;
        }
        case 0x31:
        { /* LD SP,nn */
            REG_SP = Z80Ops::peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x32:
        { /* LD (nn),A */
            REG_WZ = Z80Ops::peek16(REG_PC);
            Z80Ops::poke8(REG_WZ, regA);
            REG_WZ = (regA << 8) | ((REG_WZ + 1) & 0xff);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x33:
        { /* INC SP */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_SP++;
            break;
        }
        case 0x34:
        { /* INC (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            inc8(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x35:
        { /* DEC (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            dec8(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x36:
        { /* LD (HL),n */
            Z80Ops::poke8(REG_HL, Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        }
        case 0x37:
        { /* SCF */
            uint8_t regQ = lastFlagQ ? sz5h3pnFlags : 0;
            carryFlag = true;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (((regQ ^ sz5h3pnFlags) | regA) & FLAG_53_MASK);
            flagQ = true;
            break;
        }
        case 0x38:
        { /* JR C,e */
            int8_t offset = Z80Ops::peek8(REG_PC);
            if (carryFlag) {
                Z80Ops::addressOnBus(REG_PC, 5);
                REG_PC += offset;
                REG_WZ = REG_PC + 1;
            }
            REG_PC++;
            break;
        }
        case 0x39:
        { /* ADD HL,SP */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            add16(regHL, REG_SP);
            break;
        }
        case 0x3A:
        { /* LD A,(nn) */
            REG_WZ = Z80Ops::peek16(REG_PC);
            regA = Z80Ops::peek8(REG_WZ);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x3B:
        { /* DEC SP */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_SP--;
            break;
        }
        case 0x3C:
        { /* INC A */
            inc8(regA);
            break;
        }
        case 0x3D:
        { /* DEC A */
            dec8(regA);
            break;
        }
        case 0x3E:
        { /* LD A,n */
            regA = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x3F:
        { /* CCF */
            uint8_t regQ = lastFlagQ ? sz5h3pnFlags : 0;
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (((regQ ^ sz5h3pnFlags) | regA) & FLAG_53_MASK);
            if (carryFlag) {
                sz5h3pnFlags |= HALFCARRY_MASK;
            }
            carryFlag = !carryFlag;
            flagQ = true;
            break;
        }
//      case 0x40: {     /* LD B,B */
//           break;
//    }
        case 0x41:
        { /* LD B,C */
            REG_B = REG_C;
            break;
        }
        case 0x42:
        { /* LD B,D */
            REG_B = REG_D;
            break;
        }
        case 0x43:
        { /* LD B,E */
            REG_B = REG_E;
            break;
        }
        case 0x44:
        { /* LD B,H */
            REG_B = REG_H;
            break;
        }
        case 0x45:
        { /* LD B,L */
            REG_B = REG_L;
            break;
        }
        case 0x46:
        { /* LD B,(HL) */
            REG_B = Z80Ops::peek8(REG_HL);
            break;
        }
        case 0x47:
        { /* LD B,A */
            REG_B = regA;
            break;
        }
        case 0x48:
        { /* LD C,B */
            REG_C = REG_B;
            break;
        }
//        case 0x49: {     /* LD C,C */
//            break;
//        }
        case 0x4A:
        { /* LD C,D */
            REG_C = REG_D;
            break;
        }
        case 0x4B:
        { /* LD C,E */
            REG_C = REG_E;
            break;
        }
        case 0x4C:
        { /* LD C,H */
            REG_C = REG_H;
            break;
        }
        case 0x4D:
        { /* LD C,L */
            REG_C = REG_L;
            break;
        }
        case 0x4E:
        { /* LD C,(HL) */
            REG_C = Z80Ops::peek8(REG_HL);
            break;
        }
        case 0x4F:
        { /* LD C,A */
            REG_C = regA;
            break;
        }
        case 0x50:
        { /* LD D,B */
            REG_D = REG_B;
            break;
        }
        case 0x51:
        { /* LD D,C */
            REG_D = REG_C;
            break;
        }
//            case 0x52: {     /* LD D,D */
//                break;
//            }
        case 0x53:
        { /* LD D,E */
            REG_D = REG_E;
            break;
        }
        case 0x54:
        { /* LD D,H */
            REG_D = REG_H;
            break;
        }
        case 0x55:
        { /* LD D,L */
            REG_D = REG_L;
            break;
        }
        case 0x56:
        { /* LD D,(HL) */
            REG_D = Z80Ops::peek8(REG_HL);
            break;
        }
        case 0x57:
        { /* LD D,A */
            REG_D = regA;
            break;
        }
        case 0x58:
        { /* LD E,B */
            REG_E = REG_B;
            break;
        }
        case 0x59:
        { /* LD E,C */
            REG_E = REG_C;
            break;
        }
        case 0x5A:
        { /* LD E,D */
            REG_E = REG_D;
            break;
        }
//            case 0x5B: {     /* LD E,E */
//                break;
//            }
        case 0x5C:
        { /* LD E,H */
            REG_E = REG_H;
            break;
        }
        case 0x5D:
        { /* LD E,L */
            REG_E = REG_L;
            break;
        }
        case 0x5E:
        { /* LD E,(HL) */
            REG_E = Z80Ops::peek8(REG_HL);
            break;
        }
        case 0x5F:
        { /* LD E,A */
            REG_E = regA;
            break;
        }
        case 0x60:
        { /* LD H,B */
            REG_H = REG_B;
            break;
        }
        case 0x61:
        { /* LD H,C */
            REG_H = REG_C;
            break;
        }
        case 0x62:
        { /* LD H,D */
            REG_H = REG_D;
            break;
        }
        case 0x63:
        { /* LD H,E */
            REG_H = REG_E;
            break;
        }
//            case 0x64: {     /* LD H,H */
//                break;
//            }
        case 0x65:
        { /* LD H,L */
            REG_H = REG_L;
            break;
        }
        case 0x66:
        { /* LD H,(HL) */
            REG_H = Z80Ops::peek8(REG_HL);
            break;
        }
        case 0x67:
        { /* LD H,A */
            REG_H = regA;
            break;
        }
        case 0x68:
        { /* LD L,B */
            REG_L = REG_B;
            break;
        }
        case 0x69:
        { /* LD L,C */
            REG_L = REG_C;
            break;
        }
        case 0x6A:
        { /* LD L,D */
            REG_L = REG_D;
            break;
        }
        case 0x6B:
        { /* LD L,E */
            REG_L = REG_E;
            break;
        }
        case 0x6C:
        { /* LD L,H */
            REG_L = REG_H;
            break;
        }
//            case 0x6D: {     /* LD L,L */
//                break;
//            }
        case 0x6E:
        { /* LD L,(HL) */
            REG_L = Z80Ops::peek8(REG_HL);
            break;
        }
        case 0x6F:
        { /* LD L,A */
            REG_L = regA;
            break;
        }
        case 0x70:
        { /* LD (HL),B */
            Z80Ops::poke8(REG_HL, REG_B);
            break;
        }
        case 0x71:
        { /* LD (HL),C */
            Z80Ops::poke8(REG_HL, REG_C);
            break;
        }
        case 0x72:
        { /* LD (HL),D */
            Z80Ops::poke8(REG_HL, REG_D);
            break;
        }
        case 0x73:
        { /* LD (HL),E */
            Z80Ops::poke8(REG_HL, REG_E);
            break;
        }
        case 0x74:
        { /* LD (HL),H */
            Z80Ops::poke8(REG_HL, REG_H);
            break;
        }
        case 0x75:
        { /* LD (HL),L */
            Z80Ops::poke8(REG_HL, REG_L);
            break;
        }
        case 0x76:
        { /* HALT */
            REG_PC--;
            // Signal HALT to CPU Loop
            Z80Ops::signalHalt();
            halted = true;
            break;
        }
        case 0x77:
        { /* LD (HL),A */
            Z80Ops::poke8(REG_HL, regA);
            break;
        }
        case 0x78:
        { /* LD A,B */
            regA = REG_B;
            break;
        }
        case 0x79:
        { /* LD A,C */
            regA = REG_C;
            break;
        }
        case 0x7A:
        { /* LD A,D */
            regA = REG_D;
            break;
        }
        case 0x7B:
        { /* LD A,E */
            regA = REG_E;
            break;
        }
        case 0x7C:
        { /* LD A,H */
            regA = REG_H;
            break;
        }
        case 0x7D:
        { /* LD A,L */
            regA = REG_L;
            break;
        }
        case 0x7E:
        { /* LD A,(HL) */
            regA = Z80Ops::peek8(REG_HL);
            break;
        }
//            case 0x7F: {     /* LD A,A */
//                break;
//            }
        case 0x80:
        { /* ADD A,B */
            add(REG_B);
            break;
        }
        case 0x81:
        { /* ADD A,C */
            add(REG_C);
            break;
        }
        case 0x82:
        { /* ADD A,D */
            add(REG_D);
            break;
        }
        case 0x83:
        { /* ADD A,E */
            add(REG_E);
            break;
        }
        case 0x84:
        { /* ADD A,H */
            add(REG_H);
            break;
        }
        case 0x85:
        { /* ADD A,L */
            add(REG_L);
            break;
        }
        case 0x86:
        { /* ADD A,(HL) */
            add(Z80Ops::peek8(REG_HL));
            break;
        }
        case 0x87:
        { /* ADD A,A */
            add(regA);
            break;
        }
        case 0x88:
        { /* ADC A,B */
            adc(REG_B);
            break;
        }
        case 0x89:
        { /* ADC A,C */
            adc(REG_C);
            break;
        }
        case 0x8A:
        { /* ADC A,D */
            adc(REG_D);
            break;
        }
        case 0x8B:
        { /* ADC A,E */
            adc(REG_E);
            break;
        }
        case 0x8C:
        { /* ADC A,H */
            adc(REG_H);
            break;
        }
        case 0x8D:
        { /* ADC A,L */
            adc(REG_L);
            break;
        }
        case 0x8E:
        { /* ADC A,(HL) */
            adc(Z80Ops::peek8(REG_HL));
            break;
        }
        case 0x8F:
        { /* ADC A,A */
            adc(regA);
            break;
        }
        case 0x90:
        { /* SUB B */
            sub(REG_B);
            break;
        }
        case 0x91:
        { /* SUB C */
            sub(REG_C);
            break;
        }
        case 0x92:
        { /* SUB D */
            sub(REG_D);
            break;
        }
        case 0x93:
        { /* SUB E */
            sub(REG_E);
            break;
        }
        case 0x94:
        { /* SUB H */
            sub(REG_H);
            break;
        }
        case 0x95:
        { /* SUB L */
            sub(REG_L);
            break;
        }
        case 0x96:
        { /* SUB (HL) */
            sub(Z80Ops::peek8(REG_HL));
            break;
        }
        case 0x97:
        { /* SUB A */
            sub(regA);
            break;
        }
        case 0x98:
        { /* SBC A,B */
            sbc(REG_B);
            break;
        }
        case 0x99:
        { /* SBC A,C */
            sbc(REG_C);
            break;
        }
        case 0x9A:
        { /* SBC A,D */
            sbc(REG_D);
            break;
        }
        case 0x9B:
        { /* SBC A,E */
            sbc(REG_E);
            break;
        }
        case 0x9C:
        { /* SBC A,H */
            sbc(REG_H);
            break;
        }
        case 0x9D:
        { /* SBC A,L */
            sbc(REG_L);
            break;
        }
        case 0x9E:
        { /* SBC A,(HL) */
            sbc(Z80Ops::peek8(REG_HL));
            break;
        }
        case 0x9F:
        { /* SBC A,A */
            sbc(regA);
            break;
        }
        case 0xA0:
        { /* AND B */
            and_(REG_B);
            break;
        }
        case 0xA1:
        { /* AND C */
            and_(REG_C);
            break;
        }
        case 0xA2:
        { /* AND D */
            and_(REG_D);
            break;
        }
        case 0xA3:
        { /* AND E */
            and_(REG_E);
            break;
        }
        case 0xA4:
        { /* AND H */
            and_(REG_H);
            break;
        }
        case 0xA5:
        { /* AND L */
            and_(REG_L);
            break;
        }
        case 0xA6:
        { /* AND (HL) */
            and_(Z80Ops::peek8(REG_HL));
            break;
        }
        case 0xA7:
        { /* AND A */
            and_(regA);
            break;
        }
        case 0xA8:
        { /* XOR B */
            xor_(REG_B);
            break;
        }
        case 0xA9:
        { /* XOR C */
            xor_(REG_C);
            break;
        }
        case 0xAA:
        { /* XOR D */
            xor_(REG_D);
            break;
        }
        case 0xAB:
        { /* XOR E */
            xor_(REG_E);
            break;
        }
        case 0xAC:
        { /* XOR H */
            xor_(REG_H);
            break;
        }
        case 0xAD:
        { /* XOR L */
            xor_(REG_L);
            break;
        }
        case 0xAE:
        { /* XOR (HL) */
            xor_(Z80Ops::peek8(REG_HL));
            break;
        }
        case 0xAF:
        { /* XOR A */
            xor_(regA);
            break;
        }
        case 0xB0:
        { /* OR B */
            or_(REG_B);
            break;
        }
        case 0xB1:
        { /* OR C */
            or_(REG_C);
            break;
        }
        case 0xB2:
        { /* OR D */
            or_(REG_D);
            break;
        }
        case 0xB3:
        { /* OR E */
            or_(REG_E);
            break;
        }
        case 0xB4:
        { /* OR H */
            or_(REG_H);
            break;
        }
        case 0xB5:
        { /* OR L */
            or_(REG_L);
            break;
        }
        case 0xB6:
        { /* OR (HL) */
            or_(Z80Ops::peek8(REG_HL));
            break;
        }
        case 0xB7:
        { /* OR A */
            or_(regA);
            break;
        }
        case 0xB8:
        { /* CP B */
            cp(REG_B);
            break;
        }
        case 0xB9:
        { /* CP C */
            cp(REG_C);
            break;
        }
        case 0xBA:
        { /* CP D */
            cp(REG_D);
            break;
        }
        case 0xBB:
        { /* CP E */
            cp(REG_E);
            break;
        }
        case 0xBC:
        { /* CP H */
            cp(REG_H);
            break;
        }
        case 0xBD:
        { /* CP L */
            cp(REG_L);
            break;
        }
        case 0xBE:
        { /* CP (HL) */
            cp(Z80Ops::peek8(REG_HL));
            break;
        }
        case 0xBF:
        { /* CP A */
            cp(regA);
            break;
        }
        case 0xC0:
        { /* RET NZ */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_WZ = pop();
            }
            break;
        }
        case 0xC1:
        { /* POP BC */
            REG_BC = pop();
            break;
        }
        case 0xC2:
        { /* JP NZ,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xC3:
        { /* JP nn */
            REG_WZ = REG_PC = Z80Ops::peek16(REG_PC);
            break;
        }
        case 0xC4:
        { /* CALL NZ,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                Z80Ops::addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xC5:
        { /* PUSH BC */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_BC);
            break;
        }
        case 0xC6:
        { /* ADD A,n */
            add(Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        }
        case 0xC7:
        { /* RST 00H */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x00;
            break;
        }
        case 0xC8:
        { /* RET Z */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                REG_PC = REG_WZ = pop();
            }
            break;
        }
        case 0xC9:
        { /* RET */
            REG_PC = REG_WZ = pop();
            break;
        }
        case 0xCA:
        { /* JP Z,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xCB:
        { /* Subconjunto de instrucciones */
            decodeCB();
            break;
        }
        case 0xCC:
        { /* CALL Z,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                Z80Ops::addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xCD:
        { /* CALL nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            Z80Ops::addressOnBus(REG_PC + 1, 1);
            push(REG_PC + 2);
            REG_PC = REG_WZ;
            break;
        }
        case 0xCE:
        { /* ADC A,n */
            adc(Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        }
        case 0xCF:
        { /* RST 08H */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x08;
            break;
        }
        case 0xD0:
        { /* RET NC */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            if (!carryFlag) {
                REG_PC = REG_WZ = pop();
            }
            break;
        }
        case 0xD1:
        { /* POP DE */
            REG_DE = pop();
            break;
        }
        case 0xD2:
        { /* JP NC,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if (!carryFlag) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xD3:
        { /* OUT (n),A */
            uint8_t work8 = Z80Ops::peek8(REG_PC);
            REG_PC++;
            REG_WZ = regA << 8;
            Z80Ops::outPort(REG_WZ | work8, regA);
            REG_WZ |= (work8 + 1);
            break;
        }
        case 0xD4:
        { /* CALL NC,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if (!carryFlag) {
                Z80Ops::addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xD5:
        { /* PUSH DE */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_DE);
            break;
        }
        case 0xD6:
        { /* SUB n */
            sub(Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        }
        case 0xD7:
        { /* RST 10H */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x10;
            break;
        }
        case 0xD8:
        { /* RET C */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            if (carryFlag) {
                REG_PC = REG_WZ = pop();
            }
            break;
        }
        case 0xD9:
        { /* EXX */
            uint16_t tmp;
            tmp = REG_BC;
            REG_BC = REG_BCx;
            REG_BCx = tmp;

            tmp = REG_DE;
            REG_DE = REG_DEx;
            REG_DEx = tmp;

            tmp = REG_HL;
            REG_HL = REG_HLx;
            REG_HLx = tmp;
            break;
        }
        case 0xDA:
        { /* JP C,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if (carryFlag) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xDB:
        { /* IN A,(n) */
            REG_W = regA;
            REG_Z = Z80Ops::peek8(REG_PC);
            //REG_WZ = (regA << 8) | Z80Ops::peek8(REG_PC);
            REG_PC++;
            regA = Z80Ops::inPort(REG_WZ);
            REG_WZ++;
            break;
        }
        case 0xDC:
        { /* CALL C,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if (carryFlag) {
                Z80Ops::addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xDD:
        { /* Subconjunto de instrucciones */
            opCode = Z80Ops::fetchOpcode(REG_PC++);
            regR++;
            decodeDDFD(opCode, regIX);
            break;
        }
        case 0xDE:
        { /* SBC A,n */
            sbc(Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        }
        case 0xDF:
        { /* RST 18H */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x18;
            break;
        }
        case 0xE0: /* RET PO */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                REG_PC = REG_WZ = pop();
            }
            break;
        case 0xE1: /* POP HL */
            REG_HL = pop();
            break;
        case 0xE2: /* JP PO,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xE3:
        { /* EX (SP),HL */
            // Instrucción de ejecución sutil.
            RegisterPair work = regHL;
            REG_HL = Z80Ops::peek16(REG_SP);
            Z80Ops::addressOnBus(REG_SP + 1, 1);
            // No se usa poke16 porque el Z80 escribe los bytes AL REVES
            Z80Ops::poke8(REG_SP + 1, work.byte8.hi);
            Z80Ops::poke8(REG_SP, work.byte8.lo);
            Z80Ops::addressOnBus(REG_SP, 2);
            REG_WZ = REG_HL;
            break;
        }
        case 0xE4: /* CALL PO,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                Z80Ops::addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xE5: /* PUSH HL */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_HL);
            break;
        case 0xE6: /* AND n */
            and_(Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        case 0xE7: /* RST 20H */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x20;
            break;
        case 0xE8: /* RET PE */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                REG_PC = REG_WZ = pop();
            }
            break;
        case 0xE9: /* JP (HL) */
            REG_PC = REG_HL;
            break;
        case 0xEA: /* JP PE,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xEB:
        { /* EX DE,HL */
            uint16_t tmp = REG_HL;
            REG_HL = REG_DE;
            REG_DE = tmp;
            break;
        }
        case 0xEC: /* CALL PE,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                Z80Ops::addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xED: /*Subconjunto de instrucciones*/
            opCode = Z80Ops::fetchOpcode(REG_PC++);
            regR++;
            decodeED(opCode);
            break;
        case 0xEE: /* XOR n */
            xor_(Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        case 0xEF: /* RST 28H */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x28;
            break;
        case 0xF0: /* RET P */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            if (sz5h3pnFlags < SIGN_MASK) {
                REG_PC = REG_WZ = pop();
            }
            break;
        case 0xF1: /* POP AF */
            setRegAF(pop());
            break;
        case 0xF2: /* JP P,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if (sz5h3pnFlags < SIGN_MASK) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xF3: /* DI */
            ffIFF1 = ffIFF2 = false;
            break;
        case 0xF4: /* CALL P,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if (sz5h3pnFlags < SIGN_MASK) {
                Z80Ops::addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xF5: /* PUSH AF */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(getRegAF());
            break;
        case 0xF6: /* OR n */
            or_(Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        case 0xF7: /* RST 30H */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x30;
            break;
        case 0xF8: /* RET M */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            if (sz5h3pnFlags > 0x7f) {
                REG_PC = REG_WZ = pop();
            }
            break;
        case 0xF9: /* LD SP,HL */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_SP = REG_HL;
            break;
        case 0xFA: /* JP M,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if (sz5h3pnFlags > 0x7f) {
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xFB: /* EI */
            ffIFF1 = ffIFF2 = true;
            pendingEI = true;
            break;
        case 0xFC: /* CALL M,nn */
            REG_WZ = Z80Ops::peek16(REG_PC);
            if (sz5h3pnFlags > 0x7f) {
                Z80Ops::addressOnBus(REG_PC + 1, 1);
                push(REG_PC + 2);
                REG_PC = REG_WZ;
                break;
            }
            REG_PC = REG_PC + 2;
            break;
        case 0xFD: /* Subconjunto de instrucciones */
            opCode = Z80Ops::fetchOpcode(REG_PC++);
            regR++;
            decodeDDFD(opCode, regIY);
            break;
        case 0xFE: /* CP n */
            cp(Z80Ops::peek8(REG_PC));
            REG_PC++;
            break;
        case 0xFF: /* RST 38H */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(REG_PC);
            REG_PC = REG_WZ = 0x38;
    } /* del switch( codigo ) */
}

//Subconjunto de instrucciones 0xCB

void Z80::decodeCB(void) {
    uint8_t opCode = Z80Ops::fetchOpcode(REG_PC++);
    regR++;
    switch (opCode) {
        case 0x00:
        { /* RLC B */
            rlc(REG_B);
            break;
        }
        case 0x01:
        { /* RLC C */
            rlc(REG_C);
            break;
        }
        case 0x02:
        { /* RLC D */
            rlc(REG_D);
            break;
        }
        case 0x03:
        { /* RLC E */
            rlc(REG_E);
            break;
        }
        case 0x04:
        { /* RLC H */
            rlc(REG_H);
            break;
        }
        case 0x05:
        { /* RLC L */
            rlc(REG_L);
            break;
        }
        case 0x06:
        { /* RLC (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            rlc(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x07:
        { /* RLC A */
            rlc(regA);
            break;
        }
        case 0x08:
        { /* RRC B */
            rrc(REG_B);
            break;
        }
        case 0x09:
        { /* RRC C */
            rrc(REG_C);
            break;
        }
        case 0x0A:
        { /* RRC D */
            rrc(REG_D);
            break;
        }
        case 0x0B:
        { /* RRC E */
            rrc(REG_E);
            break;
        }
        case 0x0C:
        { /* RRC H */
            rrc(REG_H);
            break;
        }
        case 0x0D:
        { /* RRC L */
            rrc(REG_L);
            break;
        }
        case 0x0E:
        { /* RRC (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            rrc(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x0F:
        { /* RRC A */
            rrc(regA);
            break;
        }
        case 0x10:
        { /* RL B */
            rl(REG_B);
            break;
        }
        case 0x11:
        { /* RL C */
            rl(REG_C);
            break;
        }
        case 0x12:
        { /* RL D */
            rl(REG_D);
            break;
        }
        case 0x13:
        { /* RL E */
            rl(REG_E);
            break;
        }
        case 0x14:
        { /* RL H */
            rl(REG_H);
            break;
        }
        case 0x15:
        { /* RL L */
            rl(REG_L);
            break;
        }
        case 0x16:
        { /* RL (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            rl(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x17:
        { /* RL A */
            rl(regA);
            break;
        }
        case 0x18:
        { /* RR B */
            rr(REG_B);
            break;
        }
        case 0x19:
        { /* RR C */
            rr(REG_C);
            break;
        }
        case 0x1A:
        { /* RR D */
            rr(REG_D);
            break;
        }
        case 0x1B:
        { /* RR E */
            rr(REG_E);
            break;
        }
        case 0x1C:
        { /*RR H*/
            rr(REG_H);
            break;
        }
        case 0x1D:
        { /* RR L */
            rr(REG_L);
            break;
        }
        case 0x1E:
        { /* RR (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            rr(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x1F:
        { /* RR A */
            rr(regA);
            break;
        }
        case 0x20:
        { /* SLA B */
            sla(REG_B);
            break;
        }
        case 0x21:
        { /* SLA C */
            sla(REG_C);
            break;
        }
        case 0x22:
        { /* SLA D */
            sla(REG_D);
            break;
        }
        case 0x23:
        { /* SLA E */
            sla(REG_E);
            break;
        }
        case 0x24:
        { /* SLA H */
            sla(REG_H);
            break;
        }
        case 0x25:
        { /* SLA L */
            sla(REG_L);
            break;
        }
        case 0x26:
        { /* SLA (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            sla(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x27:
        { /* SLA A */
            sla(regA);
            break;
        }
        case 0x28:
        { /* SRA B */
            sra(REG_B);
            break;
        }
        case 0x29:
        { /* SRA C */
            sra(REG_C);
            break;
        }
        case 0x2A:
        { /* SRA D */
            sra(REG_D);
            break;
        }
        case 0x2B:
        { /* SRA E */
            sra(REG_E);
            break;
        }
        case 0x2C:
        { /* SRA H */
            sra(REG_H);
            break;
        }
        case 0x2D:
        { /* SRA L */
            sra(REG_L);
            break;
        }
        case 0x2E:
        { /* SRA (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            sra(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x2F:
        { /* SRA A */
            sra(regA);
            break;
        }
        case 0x30:
        { /* SLL B */
            sll(REG_B);
            break;
        }
        case 0x31:
        { /* SLL C */
            sll(REG_C);
            break;
        }
        case 0x32:
        { /* SLL D */
            sll(REG_D);
            break;
        }
        case 0x33:
        { /* SLL E */
            sll(REG_E);
            break;
        }
        case 0x34:
        { /* SLL H */
            sll(REG_H);
            break;
        }
        case 0x35:
        { /* SLL L */
            sll(REG_L);
            break;
        }
        case 0x36:
        { /* SLL (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            sll(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x37:
        { /* SLL A */
            sll(regA);
            break;
        }
        case 0x38:
        { /* SRL B */
            srl(REG_B);
            break;
        }
        case 0x39:
        { /* SRL C */
            srl(REG_C);
            break;
        }
        case 0x3A:
        { /* SRL D */
            srl(REG_D);
            break;
        }
        case 0x3B:
        { /* SRL E */
            srl(REG_E);
            break;
        }
        case 0x3C:
        { /* SRL H */
            srl(REG_H);
            break;
        }
        case 0x3D:
        { /* SRL L */
            srl(REG_L);
            break;
        }
        case 0x3E:
        { /* SRL (HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL);
            srl(work8);
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x3F:
        { /* SRL A */
            srl(regA);
            break;
        }
        case 0x40:
        { /* BIT 0,B */
            bitTest(0x01, REG_B);
            break;
        }
        case 0x41:
        { /* BIT 0,C */
            bitTest(0x01, REG_C);
            break;
        }
        case 0x42:
        { /* BIT 0,D */
            bitTest(0x01, REG_D);
            break;
        }
        case 0x43:
        { /* BIT 0,E */
            bitTest(0x01, REG_E);
            break;
        }
        case 0x44:
        { /* BIT 0,H */
            bitTest(0x01, REG_H);
            break;
        }
        case 0x45:
        { /* BIT 0,L */
            bitTest(0x01, REG_L);
            break;
        }
        case 0x46:
        { /* BIT 0,(HL) */
            bitTest(0x01, Z80Ops::peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80Ops::addressOnBus(REG_HL, 1);
            break;
        }
        case 0x47:
        { /* BIT 0,A */
            bitTest(0x01, regA);
            break;
        }
        case 0x48:
        { /* BIT 1,B */
            bitTest(0x02, REG_B);
            break;
        }
        case 0x49:
        { /* BIT 1,C */
            bitTest(0x02, REG_C);
            break;
        }
        case 0x4A:
        { /* BIT 1,D */
            bitTest(0x02, REG_D);
            break;
        }
        case 0x4B:
        { /* BIT 1,E */
            bitTest(0x02, REG_E);
            break;
        }
        case 0x4C:
        { /* BIT 1,H */
            bitTest(0x02, REG_H);
            break;
        }
        case 0x4D:
        { /* BIT 1,L */
            bitTest(0x02, REG_L);
            break;
        }
        case 0x4E:
        { /* BIT 1,(HL) */
            bitTest(0x02, Z80Ops::peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80Ops::addressOnBus(REG_HL, 1);
            break;
        }
        case 0x4F:
        { /* BIT 1,A */
            bitTest(0x02, regA);
            break;
        }
        case 0x50:
        { /* BIT 2,B */
            bitTest(0x04, REG_B);
            break;
        }
        case 0x51:
        { /* BIT 2,C */
            bitTest(0x04, REG_C);
            break;
        }
        case 0x52:
        { /* BIT 2,D */
            bitTest(0x04, REG_D);
            break;
        }
        case 0x53:
        { /* BIT 2,E */
            bitTest(0x04, REG_E);
            break;
        }
        case 0x54:
        { /* BIT 2,H */
            bitTest(0x04, REG_H);
            break;
        }
        case 0x55:
        { /* BIT 2,L */
            bitTest(0x04, REG_L);
            break;
        }
        case 0x56:
        { /* BIT 2,(HL) */
            bitTest(0x04, Z80Ops::peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80Ops::addressOnBus(REG_HL, 1);
            break;
        }
        case 0x57:
        { /* BIT 2,A */
            bitTest(0x04, regA);
            break;
        }
        case 0x58:
        { /* BIT 3,B */
            bitTest(0x08, REG_B);
            break;
        }
        case 0x59:
        { /* BIT 3,C */
            bitTest(0x08, REG_C);
            break;
        }
        case 0x5A:
        { /* BIT 3,D */
            bitTest(0x08, REG_D);
            break;
        }
        case 0x5B:
        { /* BIT 3,E */
            bitTest(0x08, REG_E);
            break;
        }
        case 0x5C:
        { /* BIT 3,H */
            bitTest(0x08, REG_H);
            break;
        }
        case 0x5D:
        { /* BIT 3,L */
            bitTest(0x08, REG_L);
            break;
        }
        case 0x5E:
        { /* BIT 3,(HL) */
            bitTest(0x08, Z80Ops::peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80Ops::addressOnBus(REG_HL, 1);
            break;
        }
        case 0x5F:
        { /* BIT 3,A */
            bitTest(0x08, regA);
            break;
        }
        case 0x60:
        { /* BIT 4,B */
            bitTest(0x10, REG_B);
            break;
        }
        case 0x61:
        { /* BIT 4,C */
            bitTest(0x10, REG_C);
            break;
        }
        case 0x62:
        { /* BIT 4,D */
            bitTest(0x10, REG_D);
            break;
        }
        case 0x63:
        { /* BIT 4,E */
            bitTest(0x10, REG_E);
            break;
        }
        case 0x64:
        { /* BIT 4,H */
            bitTest(0x10, REG_H);
            break;
        }
        case 0x65:
        { /* BIT 4,L */
            bitTest(0x10, REG_L);
            break;
        }
        case 0x66:
        { /* BIT 4,(HL) */
            bitTest(0x10, Z80Ops::peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80Ops::addressOnBus(REG_HL, 1);
            break;
        }
        case 0x67:
        { /* BIT 4,A */
            bitTest(0x10, regA);
            break;
        }
        case 0x68:
        { /* BIT 5,B */
            bitTest(0x20, REG_B);
            break;
        }
        case 0x69:
        { /* BIT 5,C */
            bitTest(0x20, REG_C);
            break;
        }
        case 0x6A:
        { /* BIT 5,D */
            bitTest(0x20, REG_D);
            break;
        }
        case 0x6B:
        { /* BIT 5,E */
            bitTest(0x20, REG_E);
            break;
        }
        case 0x6C:
        { /* BIT 5,H */
            bitTest(0x20, REG_H);
            break;
        }
        case 0x6D:
        { /* BIT 5,L */
            bitTest(0x20, REG_L);
            break;
        }
        case 0x6E:
        { /* BIT 5,(HL) */
            bitTest(0x20, Z80Ops::peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80Ops::addressOnBus(REG_HL, 1);
            break;
        }
        case 0x6F:
        { /* BIT 5,A */
            bitTest(0x20, regA);
            break;
        }
        case 0x70:
        { /* BIT 6,B */
            bitTest(0x40, REG_B);
            break;
        }
        case 0x71:
        { /* BIT 6,C */
            bitTest(0x40, REG_C);
            break;
        }
        case 0x72:
        { /* BIT 6,D */
            bitTest(0x40, REG_D);
            break;
        }
        case 0x73:
        { /* BIT 6,E */
            bitTest(0x40, REG_E);
            break;
        }
        case 0x74:
        { /* BIT 6,H */
            bitTest(0x40, REG_H);
            break;
        }
        case 0x75:
        { /* BIT 6,L */
            bitTest(0x40, REG_L);
            break;
        }
        case 0x76:
        { /* BIT 6,(HL) */
            bitTest(0x40, Z80Ops::peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80Ops::addressOnBus(REG_HL, 1);
            break;
        }
        case 0x77:
        { /* BIT 6,A */
            bitTest(0x40, regA);
            break;
        }
        case 0x78:
        { /* BIT 7,B */
            bitTest(0x80, REG_B);
            break;
        }
        case 0x79:
        { /* BIT 7,C */
            bitTest(0x80, REG_C);
            break;
        }
        case 0x7A:
        { /* BIT 7,D */
            bitTest(0x80, REG_D);
            break;
        }
        case 0x7B:
        { /* BIT 7,E */
            bitTest(0x80, REG_E);
            break;
        }
        case 0x7C:
        { /* BIT 7,H */
            bitTest(0x80, REG_H);
            break;
        }
        case 0x7D:
        { /* BIT 7,L */
            bitTest(0x80, REG_L);
            break;
        }
        case 0x7E:
        { /* BIT 7,(HL) */
            bitTest(0x80, Z80Ops::peek8(REG_HL));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK) | (REG_W & FLAG_53_MASK);
            Z80Ops::addressOnBus(REG_HL, 1);
            break;
        }
        case 0x7F:
        { /* BIT 7,A */
            bitTest(0x80, regA);
            break;
        }
        case 0x80:
        { /* RES 0,B */
            REG_B &= 0xFE;
            break;
        }
        case 0x81:
        { /* RES 0,C */
            REG_C &= 0xFE;
            break;
        }
        case 0x82:
        { /* RES 0,D */
            REG_D &= 0xFE;
            break;
        }
        case 0x83:
        { /* RES 0,E */
            REG_E &= 0xFE;
            break;
        }
        case 0x84:
        { /* RES 0,H */
            REG_H &= 0xFE;
            break;
        }
        case 0x85:
        { /* RES 0,L */
            REG_L &= 0xFE;
            break;
        }
        case 0x86:
        { /* RES 0,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) & 0xFE;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x87:
        { /* RES 0,A */
            regA &= 0xFE;
            break;
        }
        case 0x88:
        { /* RES 1,B */
            REG_B &= 0xFD;
            break;
        }
        case 0x89:
        { /* RES 1,C */
            REG_C &= 0xFD;
            break;
        }
        case 0x8A:
        { /* RES 1,D */
            REG_D &= 0xFD;
            break;
        }
        case 0x8B:
        { /* RES 1,E */
            REG_E &= 0xFD;
            break;
        }
        case 0x8C:
        { /* RES 1,H */
            REG_H &= 0xFD;
            break;
        }
        case 0x8D:
        { /* RES 1,L */
            REG_L &= 0xFD;
            break;
        }
        case 0x8E:
        { /* RES 1,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) & 0xFD;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x8F:
        { /* RES 1,A */
            regA &= 0xFD;
            break;
        }
        case 0x90:
        { /* RES 2,B */
            REG_B &= 0xFB;
            break;
        }
        case 0x91:
        { /* RES 2,C */
            REG_C &= 0xFB;
            break;
        }
        case 0x92:
        { /* RES 2,D */
            REG_D &= 0xFB;
            break;
        }
        case 0x93:
        { /* RES 2,E */
            REG_E &= 0xFB;
            break;
        }
        case 0x94:
        { /* RES 2,H */
            REG_H &= 0xFB;
            break;
        }
        case 0x95:
        { /* RES 2,L */
            REG_L &= 0xFB;
            break;
        }
        case 0x96:
        { /* RES 2,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) & 0xFB;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x97:
        { /* RES 2,A */
            regA &= 0xFB;
            break;
        }
        case 0x98:
        { /* RES 3,B */
            REG_B &= 0xF7;
            break;
        }
        case 0x99:
        { /* RES 3,C */
            REG_C &= 0xF7;
            break;
        }
        case 0x9A:
        { /* RES 3,D */
            REG_D &= 0xF7;
            break;
        }
        case 0x9B:
        { /* RES 3,E */
            REG_E &= 0xF7;
            break;
        }
        case 0x9C:
        { /* RES 3,H */
            REG_H &= 0xF7;
            break;
        }
        case 0x9D:
        { /* RES 3,L */
            REG_L &= 0xF7;
            break;
        }
        case 0x9E:
        { /* RES 3,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) & 0xF7;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0x9F:
        { /* RES 3,A */
            regA &= 0xF7;
            break;
        }
        case 0xA0:
        { /* RES 4,B */
            REG_B &= 0xEF;
            break;
        }
        case 0xA1:
        { /* RES 4,C */
            REG_C &= 0xEF;
            break;
        }
        case 0xA2:
        { /* RES 4,D */
            REG_D &= 0xEF;
            break;
        }
        case 0xA3:
        { /* RES 4,E */
            REG_E &= 0xEF;
            break;
        }
        case 0xA4:
        { /* RES 4,H */
            REG_H &= 0xEF;
            break;
        }
        case 0xA5:
        { /* RES 4,L */
            REG_L &= 0xEF;
            break;
        }
        case 0xA6:
        { /* RES 4,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) & 0xEF;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xA7:
        { /* RES 4,A */
            regA &= 0xEF;
            break;
        }
        case 0xA8:
        { /* RES 5,B */
            REG_B &= 0xDF;
            break;
        }
        case 0xA9:
        { /* RES 5,C */
            REG_C &= 0xDF;
            break;
        }
        case 0xAA:
        { /* RES 5,D */
            REG_D &= 0xDF;
            break;
        }
        case 0xAB:
        { /* RES 5,E */
            REG_E &= 0xDF;
            break;
        }
        case 0xAC:
        { /* RES 5,H */
            REG_H &= 0xDF;
            break;
        }
        case 0xAD:
        { /* RES 5,L */
            REG_L &= 0xDF;
            break;
        }
        case 0xAE:
        { /* RES 5,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) & 0xDF;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xAF:
        { /* RES 5,A */
            regA &= 0xDF;
            break;
        }
        case 0xB0:
        { /* RES 6,B */
            REG_B &= 0xBF;
            break;
        }
        case 0xB1:
        { /* RES 6,C */
            REG_C &= 0xBF;
            break;
        }
        case 0xB2:
        { /* RES 6,D */
            REG_D &= 0xBF;
            break;
        }
        case 0xB3:
        { /* RES 6,E */
            REG_E &= 0xBF;
            break;
        }
        case 0xB4:
        { /* RES 6,H */
            REG_H &= 0xBF;
            break;
        }
        case 0xB5:
        { /* RES 6,L */
            REG_L &= 0xBF;
            break;
        }
        case 0xB6:
        { /* RES 6,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) & 0xBF;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xB7:
        { /* RES 6,A */
            regA &= 0xBF;
            break;
        }
        case 0xB8:
        { /* RES 7,B */
            REG_B &= 0x7F;
            break;
        }
        case 0xB9:
        { /* RES 7,C */
            REG_C &= 0x7F;
            break;
        }
        case 0xBA:
        { /* RES 7,D */
            REG_D &= 0x7F;
            break;
        }
        case 0xBB:
        { /* RES 7,E */
            REG_E &= 0x7F;
            break;
        }
        case 0xBC:
        { /* RES 7,H */
            REG_H &= 0x7F;
            break;
        }
        case 0xBD:
        { /* RES 7,L */
            REG_L &= 0x7F;
            break;
        }
        case 0xBE:
        { /* RES 7,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) & 0x7F;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xBF:
        { /* RES 7,A */
            regA &= 0x7F;
            break;
        }
        case 0xC0:
        { /* SET 0,B */
            REG_B |= 0x01;
            break;
        }
        case 0xC1:
        { /* SET 0,C */
            REG_C |= 0x01;
            break;
        }
        case 0xC2:
        { /* SET 0,D */
            REG_D |= 0x01;
            break;
        }
        case 0xC3:
        { /* SET 0,E */
            REG_E |= 0x01;
            break;
        }
        case 0xC4:
        { /* SET 0,H */
            REG_H |= 0x01;
            break;
        }
        case 0xC5:
        { /* SET 0,L */
            REG_L |= 0x01;
            break;
        }
        case 0xC6:
        { /* SET 0,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) | 0x01;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xC7:
        { /* SET 0,A */
            regA |= 0x01;
            break;
        }
        case 0xC8:
        { /* SET 1,B */
            REG_B |= 0x02;
            break;
        }
        case 0xC9:
        { /* SET 1,C */
            REG_C |= 0x02;
            break;
        }
        case 0xCA:
        { /* SET 1,D */
            REG_D |= 0x02;
            break;
        }
        case 0xCB:
        { /* SET 1,E */
            REG_E |= 0x02;
            break;
        }
        case 0xCC:
        { /* SET 1,H */
            REG_H |= 0x02;
            break;
        }
        case 0xCD:
        { /* SET 1,L */
            REG_L |= 0x02;
            break;
        }
        case 0xCE:
        { /* SET 1,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) | 0x02;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xCF:
        { /* SET 1,A */
            regA |= 0x02;
            break;
        }
        case 0xD0:
        { /* SET 2,B */
            REG_B |= 0x04;
            break;
        }
        case 0xD1:
        { /* SET 2,C */
            REG_C |= 0x04;
            break;
        }
        case 0xD2:
        { /* SET 2,D */
            REG_D |= 0x04;
            break;
        }
        case 0xD3:
        { /* SET 2,E */
            REG_E |= 0x04;
            break;
        }
        case 0xD4:
        { /* SET 2,H */
            REG_H |= 0x04;
            break;
        }
        case 0xD5:
        { /* SET 2,L */
            REG_L |= 0x04;
            break;
        }
        case 0xD6:
        { /* SET 2,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) | 0x04;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xD7:
        { /* SET 2,A */
            regA |= 0x04;
            break;
        }
        case 0xD8:
        { /* SET 3,B */
            REG_B |= 0x08;
            break;
        }
        case 0xD9:
        { /* SET 3,C */
            REG_C |= 0x08;
            break;
        }
        case 0xDA:
        { /* SET 3,D */
            REG_D |= 0x08;
            break;
        }
        case 0xDB:
        { /* SET 3,E */
            REG_E |= 0x08;
            break;
        }
        case 0xDC:
        { /* SET 3,H */
            REG_H |= 0x08;
            break;
        }
        case 0xDD:
        { /* SET 3,L */
            REG_L |= 0x08;
            break;
        }
        case 0xDE:
        { /* SET 3,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) | 0x08;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xDF:
        { /* SET 3,A */
            regA |= 0x08;
            break;
        }
        case 0xE0:
        { /* SET 4,B */
            REG_B |= 0x10;
            break;
        }
        case 0xE1:
        { /* SET 4,C */
            REG_C |= 0x10;
            break;
        }
        case 0xE2:
        { /* SET 4,D */
            REG_D |= 0x10;
            break;
        }
        case 0xE3:
        { /* SET 4,E */
            REG_E |= 0x10;
            break;
        }
        case 0xE4:
        { /* SET 4,H */
            REG_H |= 0x10;
            break;
        }
        case 0xE5:
        { /* SET 4,L */
            REG_L |= 0x10;
            break;
        }
        case 0xE6:
        { /* SET 4,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) | 0x10;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xE7:
        { /* SET 4,A */
            regA |= 0x10;
            break;
        }
        case 0xE8:
        { /* SET 5,B */
            REG_B |= 0x20;
            break;
        }
        case 0xE9:
        { /* SET 5,C */
            REG_C |= 0x20;
            break;
        }
        case 0xEA:
        { /* SET 5,D */
            REG_D |= 0x20;
            break;
        }
        case 0xEB:
        { /* SET 5,E */
            REG_E |= 0x20;
            break;
        }
        case 0xEC:
        { /* SET 5,H */
            REG_H |= 0x20;
            break;
        }
        case 0xED:
        { /* SET 5,L */
            REG_L |= 0x20;
            break;
        }
        case 0xEE:
        { /* SET 5,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) | 0x20;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xEF:
        { /* SET 5,A */
            regA |= 0x20;
            break;
        }
        case 0xF0:
        { /* SET 6,B */
            REG_B |= 0x40;
            break;
        }
        case 0xF1:
        { /* SET 6,C */
            REG_C |= 0x40;
            break;
        }
        case 0xF2:
        { /* SET 6,D */
            REG_D |= 0x40;
            break;
        }
        case 0xF3:
        { /* SET 6,E */
            REG_E |= 0x40;
            break;
        }
        case 0xF4:
        { /* SET 6,H */
            REG_H |= 0x40;
            break;
        }
        case 0xF5:
        { /* SET 6,L */
            REG_L |= 0x40;
            break;
        }
        case 0xF6:
        { /* SET 6,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) | 0x40;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xF7:
        { /* SET 6,A */
            regA |= 0x40;
            break;
        }
        case 0xF8:
        { /* SET 7,B */
            REG_B |= 0x80;
            break;
        }
        case 0xF9:
        { /* SET 7,C */
            REG_C |= 0x80;
            break;
        }
        case 0xFA:
        { /* SET 7,D */
            REG_D |= 0x80;
            break;
        }
        case 0xFB:
        { /* SET 7,E */
            REG_E |= 0x80;
            break;
        }
        case 0xFC:
        { /* SET 7,H */
            REG_H |= 0x80;
            break;
        }
        case 0xFD:
        { /* SET 7,L */
            REG_L |= 0x80;
            break;
        }
        case 0xFE:
        { /* SET 7,(HL) */
            uint8_t work8 = Z80Ops::peek8(REG_HL) | 0x80;
            Z80Ops::addressOnBus(REG_HL, 1);
            Z80Ops::poke8(REG_HL, work8);
            break;
        }
        case 0xFF:
        { /* SET 7,A */
            regA |= 0x80;
            break;
        }
        default:
        {
            break;
        }
    }
}

//Subconjunto de instrucciones 0xDD / 0xFD
/*
 * Hay que tener en cuenta el manejo de secuencias códigos DD/FD que no
 * hacen nada. Según el apartado 3.7 del documento
 * [http://www.myquest.nl/z80undocumented/z80-documented-v0.91.pdf]
 * secuencias de códigos como FD DD 00 21 00 10 NOP NOP NOP LD HL,1000h
 * activan IY con el primer FD, IX con el segundo DD y vuelven al
 * registro HL con el código NOP. Es decir, si detrás del código DD/FD no
 * viene una instrucción que maneje el registro HL, el código DD/FD
 * "se olvida" y hay que procesar la instrucción como si nunca se
 * hubiera visto el prefijo (salvo por los 4 t-estados que ha costado).
 * Naturalmente, en una serie repetida de DDFD no hay que comprobar las
 * interrupciones entre cada prefijo.
 */
void Z80::decodeDDFD(uint8_t opCode, RegisterPair& regIXY) {
    switch (opCode) {
        case 0x09:
        { /* ADD IX,BC */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            add16(regIXY, REG_BC);
            break;
        }
        case 0x19:
        { /* ADD IX,DE */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            add16(regIXY, REG_DE);
            break;
        }
        case 0x21:
        { /* LD IX,nn */
            regIXY.word = Z80Ops::peek16(REG_PC);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x22:
        { /* LD (nn),IX */
            REG_WZ = Z80Ops::peek16(REG_PC);
            Z80Ops::poke16(REG_WZ++, regIXY);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x23:
        { /* INC IX */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            regIXY.word++;
            break;
        }
        case 0x24:
        { /* INC IXh */
            inc8(regIXY.byte8.hi);
            break;
        }
        case 0x25:
        { /* DEC IXh */
            dec8(regIXY.byte8.hi);
            break;
        }
        case 0x26:
        { /* LD IXh,n */
            regIXY.byte8.hi = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x29:
        { /* ADD IX,IX */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            add16(regIXY, regIXY.word);
            break;
        }
        case 0x2A:
        { /* LD IX,(nn) */
            REG_WZ = Z80Ops::peek16(REG_PC);
            regIXY.word = Z80Ops::peek16(REG_WZ++);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x2B:
        { /* DEC IX */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            regIXY.word--;
            break;
        }
        case 0x2C:
        { /* INC IXl */
            inc8(regIXY.byte8.lo);
            break;
        }
        case 0x2D:
        { /* DEC IXl */
            dec8(regIXY.byte8.lo);
            break;
        }
        case 0x2E:
        { /* LD IXl,n */
            regIXY.byte8.lo = Z80Ops::peek8(REG_PC);
            REG_PC++;
            break;
        }
        case 0x34:
        { /* INC (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            uint8_t work8 = Z80Ops::peek8(REG_WZ);
            Z80Ops::addressOnBus(REG_WZ, 1);
            inc8(work8);
            Z80Ops::poke8(REG_WZ, work8);
            break;
        }
        case 0x35:
        { /* DEC (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            uint8_t work8 = Z80Ops::peek8(REG_WZ);
            Z80Ops::addressOnBus(REG_WZ, 1);
            dec8(work8);
            Z80Ops::poke8(REG_WZ, work8);
            break;
        }
        case 0x36:
        { /* LD (IX+d),n */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            REG_PC++;
            uint8_t work8 = Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 2);
            REG_PC++;
            Z80Ops::poke8(REG_WZ, work8);
            break;
        }
        case 0x39:
        { /* ADD IX,SP */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            add16(regIXY, REG_SP);
            break;
        }
        case 0x44:
        { /* LD B,IXh */
            REG_B = regIXY.byte8.hi;
            break;
        }
        case 0x45:
        { /* LD B,IXl */
            REG_B = regIXY.byte8.lo;
            break;
        }
        case 0x46:
        { /* LD B,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_B = Z80Ops::peek8(REG_WZ);
            break;
        }
        case 0x4C:
        { /* LD C,IXh */
            REG_C = regIXY.byte8.hi;
            break;
        }
        case 0x4D:
        { /* LD C,IXl */
            REG_C = regIXY.byte8.lo;
            break;
        }
        case 0x4E:
        { /* LD C,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_C = Z80Ops::peek8(REG_WZ);
            break;
        }
        case 0x54:
        { /* LD D,IXh */
            REG_D = regIXY.byte8.hi;
            break;
        }
        case 0x55:
        { /* LD D,IXl */
            REG_D = regIXY.byte8.lo;
            break;
        }
        case 0x56:
        { /* LD D,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_D = Z80Ops::peek8(REG_WZ);
            break;
        }
        case 0x5C:
        { /* LD E,IXh */
            REG_E = regIXY.byte8.hi;
            break;
        }
        case 0x5D:
        { /* LD E,IXl */
            REG_E = regIXY.byte8.lo;
            break;
        }
        case 0x5E:
        { /* LD E,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_E = Z80Ops::peek8(REG_WZ);
            break;
        }
        case 0x60:
        { /* LD IXh,B */
            regIXY.byte8.hi = REG_B;
            break;
        }
        case 0x61:
        { /* LD IXh,C */
            regIXY.byte8.hi = REG_C;
            break;
        }
        case 0x62:
        { /* LD IXh,D */
            regIXY.byte8.hi = REG_D;
            break;
        }
        case 0x63:
        { /* LD IXh,E */
            regIXY.byte8.hi = REG_E;
            break;
        }
        case 0x64:
        { /* LD IXh,IXh */
            break;
        }
        case 0x65:
        { /* LD IXh,IXl */
            regIXY.byte8.hi = regIXY.byte8.lo;
            break;
        }
        case 0x66:
        { /* LD H,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_H = Z80Ops::peek8(REG_WZ);
            break;
        }
        case 0x67:
        { /* LD IXh,A */
            regIXY.byte8.hi = regA;
            break;
        }
        case 0x68:
        { /* LD IXl,B */
            regIXY.byte8.lo = REG_B;
            break;
        }
        case 0x69:
        { /* LD IXl,C */
            regIXY.byte8.lo = REG_C;
            break;
        }
        case 0x6A:
        { /* LD IXl,D */
            regIXY.byte8.lo = REG_D;
            break;
        }
        case 0x6B:
        { /* LD IXl,E */
            regIXY.byte8.lo = REG_E;
            break;
        }
        case 0x6C:
        { /* LD IXl,IXh */
            regIXY.byte8.lo = regIXY.byte8.hi;
            break;
        }
        case 0x6D:
        { /* LD IXl,IXl */
            break;
        }
        case 0x6E:
        { /* LD L,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            REG_L = Z80Ops::peek8(REG_WZ);
            break;
        }
        case 0x6F:
        { /* LD IXl,A */
            regIXY.byte8.lo = regA;
            break;
        }
        case 0x70:
        { /* LD (IX+d),B */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80Ops::poke8(REG_WZ, REG_B);
            break;
        }
        case 0x71:
        { /* LD (IX+d),C */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80Ops::poke8(REG_WZ, REG_C);
            break;
        }
        case 0x72:
        { /* LD (IX+d),D */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80Ops::poke8(REG_WZ, REG_D);
            break;
        }
        case 0x73:
        { /* LD (IX+d),E */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80Ops::poke8(REG_WZ, REG_E);
            break;
        }
        case 0x74:
        { /* LD (IX+d),H */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80Ops::poke8(REG_WZ, REG_H);
            break;
        }
        case 0x75:
        { /* LD (IX+d),L */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80Ops::poke8(REG_WZ, REG_L);
            break;
        }
        case 0x77:
        { /* LD (IX+d),A */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            Z80Ops::poke8(REG_WZ, regA);
            break;
        }
        case 0x7C:
        { /* LD A,IXh */
            regA = regIXY.byte8.hi;
            break;
        }
        case 0x7D:
        { /* LD A,IXl */
            regA = regIXY.byte8.lo;
            break;
        }
        case 0x7E:
        { /* LD A,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            regA = Z80Ops::peek8(REG_WZ);
            break;
        }
        case 0x84:
        { /* ADD A,IXh */
            add(regIXY.byte8.hi);
            break;
        }
        case 0x85:
        { /* ADD A,IXl */
            add(regIXY.byte8.lo);
            break;
        }
        case 0x86:
        { /* ADD A,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            add(Z80Ops::peek8(REG_WZ));
            break;
        }
        case 0x8C:
        { /* ADC A,IXh */
            adc(regIXY.byte8.hi);
            break;
        }
        case 0x8D:
        { /* ADC A,IXl */
            adc(regIXY.byte8.lo);
            break;
        }
        case 0x8E:
        { /* ADC A,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            adc(Z80Ops::peek8(REG_WZ));
            break;
        }
        case 0x94:
        { /* SUB IXh */
            sub(regIXY.byte8.hi);
            break;
        }
        case 0x95:
        { /* SUB IXl */
            sub(regIXY.byte8.lo);
            break;
        }
        case 0x96:
        { /* SUB (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            sub(Z80Ops::peek8(REG_WZ));
            break;
        }
        case 0x9C:
        { /* SBC A,IXh */
            sbc(regIXY.byte8.hi);
            break;
        }
        case 0x9D:
        { /* SBC A,IXl */
            sbc(regIXY.byte8.lo);
            break;
        }
        case 0x9E:
        { /* SBC A,(IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            sbc(Z80Ops::peek8(REG_WZ));
            break;
        }
        case 0xA4:
        { /* AND IXh */
            and_(regIXY.byte8.hi);
            break;
        }
        case 0xA5:
        { /* AND IXl */
            and_(regIXY.byte8.lo);
            break;
        }
        case 0xA6:
        { /* AND (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            and_(Z80Ops::peek8(REG_WZ));
            break;
        }
        case 0xAC:
        { /* XOR IXh */
            xor_(regIXY.byte8.hi);
            break;
        }
        case 0xAD:
        { /* XOR IXl */
            xor_(regIXY.byte8.lo);
            break;
        }
        case 0xAE:
        { /* XOR (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            xor_(Z80Ops::peek8(REG_WZ));
            break;
        }
        case 0xB4:
        { /* OR IXh */
            or_(regIXY.byte8.hi);
            break;
        }
        case 0xB5:
        { /* OR IXl */
            or_(regIXY.byte8.lo);
            break;
        }
        case 0xB6:
        { /* OR (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            or_(Z80Ops::peek8(REG_WZ));
            break;
        }
        case 0xBC:
        { /* CP IXh */
            cp(regIXY.byte8.hi);
            break;
        }
        case 0xBD:
        { /* CP IXl */
            cp(regIXY.byte8.lo);
            break;
        }
        case 0xBE:
        { /* CP (IX+d) */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 5);
            REG_PC++;
            cp(Z80Ops::peek8(REG_WZ));
            break;
        }
        case 0xCB:
        { /* Subconjunto de instrucciones */
            REG_WZ = regIXY.word + (int8_t) Z80Ops::peek8(REG_PC);
            REG_PC++;
            opCode = Z80Ops::peek8(REG_PC);
            Z80Ops::addressOnBus(REG_PC, 2);
            REG_PC++;
            decodeDDFDCB(opCode, REG_WZ);
            break;
        }
        case 0xDD:
            prefixOpcode = 0xDD;
            break;
        case 0xE1:
        { /* POP IX */
            regIXY.word = pop();
            break;
        }
        case 0xE3:
        { /* EX (SP),IX */
            // Instrucción de ejecución sutil como pocas... atento al dato.
            RegisterPair work16 = regIXY;
            regIXY.word = Z80Ops::peek16(REG_SP);
            Z80Ops::addressOnBus(REG_SP + 1, 1);
            // I can't call to poke16 from here because the Z80 do the writes in inverted order
            // Same for EX (SP), HL
            Z80Ops::poke8(REG_SP + 1, work16.byte8.hi);
            Z80Ops::poke8(REG_SP, work16.byte8.lo);
            Z80Ops::addressOnBus(REG_SP, 2);
            REG_WZ = regIXY.word;
            break;
        }
        case 0xE5:
        { /* PUSH IX */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            push(regIXY.word);
            break;
        }
        case 0xE9:
        { /* JP (IX) */
            REG_PC = regIXY.word;
            break;
        }
        case 0xED:
        {
            prefixOpcode = 0xED;
            break;
        }
        case 0xF9:
        { /* LD SP,IX */
            Z80Ops::addressOnBus(getPairIR().word, 2);
            REG_SP = regIXY.word;
            break;
        }
        case 0xFD:
        {
            prefixOpcode = 0xFD;
            break;
        }
        default:
        {
            // Detrás de un DD/FD o varios en secuencia venía un código
            // que no correspondía con una instrucción que involucra a
            // IX o IY. Se trata como si fuera un código normal.
            // Sin esto, además de emular mal, falla el test
            // ld <bcdexya>,<bcdexya> de ZEXALL.
#ifdef WITH_BREAKPOINT_SUPPORT
            if (breakpointEnabled && prefixOpcode == 0) {
                opCode = Z80Ops::breakpoint(REG_PC, opCode);
            }
#endif
            decodeOpcode(opCode);
            break;
        }
    }
}

// Subconjunto de instrucciones 0xDDCB
void Z80::decodeDDFDCB(uint8_t opCode, uint16_t address) {

    switch (opCode) {
        case 0x00: /* RLC (IX+d),B */
        case 0x01: /* RLC (IX+d),C */
        case 0x02: /* RLC (IX+d),D */
        case 0x03: /* RLC (IX+d),E */
        case 0x04: /* RLC (IX+d),H */
        case 0x05: /* RLC (IX+d),L */
        case 0x06: /* RLC (IX+d)   */
        case 0x07: /* RLC (IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address);
            rlc(work8);
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x08: /* RRC (IX+d),B */
        case 0x09: /* RRC (IX+d),C */
        case 0x0A: /* RRC (IX+d),D */
        case 0x0B: /* RRC (IX+d),E */
        case 0x0C: /* RRC (IX+d),H */
        case 0x0D: /* RRC (IX+d),L */
        case 0x0E: /* RRC (IX+d)   */
        case 0x0F: /* RRC (IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address);
            rrc(work8);
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x10: /* RL (IX+d),B */
        case 0x11: /* RL (IX+d),C */
        case 0x12: /* RL (IX+d),D */
        case 0x13: /* RL (IX+d),E */
        case 0x14: /* RL (IX+d),H */
        case 0x15: /* RL (IX+d),L */
        case 0x16: /* RL (IX+d)   */
        case 0x17: /* RL (IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address);
            rl(work8);
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x18: /* RR (IX+d),B */
        case 0x19: /* RR (IX+d),C */
        case 0x1A: /* RR (IX+d),D */
        case 0x1B: /* RR (IX+d),E */
        case 0x1C: /* RR (IX+d),H */
        case 0x1D: /* RR (IX+d),L */
        case 0x1E: /* RR (IX+d)   */
        case 0x1F: /* RR (IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address);
            rr(work8);
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x20: /* SLA (IX+d),B */
        case 0x21: /* SLA (IX+d),C */
        case 0x22: /* SLA (IX+d),D */
        case 0x23: /* SLA (IX+d),E */
        case 0x24: /* SLA (IX+d),H */
        case 0x25: /* SLA (IX+d),L */
        case 0x26: /* SLA (IX+d)   */
        case 0x27: /* SLA (IX+d),A */
        {
             uint8_t work8 = Z80Ops::peek8(address);
             sla(work8);
             Z80Ops::addressOnBus(address, 1);
             Z80Ops::poke8(address, work8);
             copyToRegister(opCode, work8);
            break;
        }
        case 0x28: /* SRA (IX+d),B */
        case 0x29: /* SRA (IX+d),C */
        case 0x2A: /* SRA (IX+d),D */
        case 0x2B: /* SRA (IX+d),E */
        case 0x2C: /* SRA (IX+d),H */
        case 0x2D: /* SRA (IX+d),L */
        case 0x2E: /* SRA (IX+d)   */
        case 0x2F: /* SRA (IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address);
            sra(work8);
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x30: /* SLL (IX+d),B */
        case 0x31: /* SLL (IX+d),C */
        case 0x32: /* SLL (IX+d),D */
        case 0x33: /* SLL (IX+d),E */
        case 0x34: /* SLL (IX+d),H */
        case 0x35: /* SLL (IX+d),L */
        case 0x36: /* SLL (IX+d)   */
        case 0x37: /* SLL (IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address);
            sll(work8);
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x38: /* SRL (IX+d),B */
        case 0x39: /* SRL (IX+d),C */
        case 0x3A: /* SRL (IX+d),D */
        case 0x3B: /* SRL (IX+d),E */
        case 0x3C: /* SRL (IX+d),H */
        case 0x3D: /* SRL (IX+d),L */
        case 0x3E: /* SRL (IX+d)   */
        case 0x3F: /* SRL (IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address);
            srl(work8);
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        { /* BIT 0,(IX+d) */
            bitTest(0x01, Z80Ops::peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80Ops::addressOnBus(address, 1);
            break;
        }
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x4F:
        { /* BIT 1,(IX+d) */
            bitTest(0x02, Z80Ops::peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80Ops::addressOnBus(address, 1);
            break;
        }
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
        { /* BIT 2,(IX+d) */
            bitTest(0x04, Z80Ops::peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80Ops::addressOnBus(address, 1);
            break;
        }
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x5F:
        { /* BIT 3,(IX+d) */
            bitTest(0x08, Z80Ops::peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80Ops::addressOnBus(address, 1);
            break;
        }
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x66:
        case 0x67:
        { /* BIT 4,(IX+d) */
            bitTest(0x10, Z80Ops::peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80Ops::addressOnBus(address, 1);
            break;
        }
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F:
        { /* BIT 5,(IX+d) */
            bitTest(0x20, Z80Ops::peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80Ops::addressOnBus(address, 1);
            break;
        }
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77:
        { /* BIT 6,(IX+d) */
            bitTest(0x40, Z80Ops::peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80Ops::addressOnBus(address, 1);
            break;
        }
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
        { /* BIT 7,(IX+d) */
            bitTest(0x80, Z80Ops::peek8(address));
            sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                    | ((address >> 8) & FLAG_53_MASK);
            Z80Ops::addressOnBus(address, 1);
            break;
        }
        case 0x80: /* RES 0,(IX+d),B */
        case 0x81: /* RES 0,(IX+d),C */
        case 0x82: /* RES 0,(IX+d),D */
        case 0x83: /* RES 0,(IX+d),E */
        case 0x84: /* RES 0,(IX+d),H */
        case 0x85: /* RES 0,(IX+d),L */
        case 0x86: /* RES 0,(IX+d)   */
        case 0x87: /* RES 0,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) & 0xFE;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x88: /* RES 1,(IX+d),B */
        case 0x89: /* RES 1,(IX+d),C */
        case 0x8A: /* RES 1,(IX+d),D */
        case 0x8B: /* RES 1,(IX+d),E */
        case 0x8C: /* RES 1,(IX+d),H */
        case 0x8D: /* RES 1,(IX+d),L */
        case 0x8E: /* RES 1,(IX+d)   */
        case 0x8F: /* RES 1,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) & 0xFD;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x90: /* RES 2,(IX+d),B */
        case 0x91: /* RES 2,(IX+d),C */
        case 0x92: /* RES 2,(IX+d),D */
        case 0x93: /* RES 2,(IX+d),E */
        case 0x94: /* RES 2,(IX+d),H */
        case 0x95: /* RES 2,(IX+d),L */
        case 0x96: /* RES 2,(IX+d)   */
        case 0x97: /* RES 2,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) & 0xFB;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0x98: /* RES 3,(IX+d),B */
        case 0x99: /* RES 3,(IX+d),C */
        case 0x9A: /* RES 3,(IX+d),D */
        case 0x9B: /* RES 3,(IX+d),E */
        case 0x9C: /* RES 3,(IX+d),H */
        case 0x9D: /* RES 3,(IX+d),L */
        case 0x9E: /* RES 3,(IX+d)   */
        case 0x9F: /* RES 3,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) & 0xF7;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xA0: /* RES 4,(IX+d),B */
        case 0xA1: /* RES 4,(IX+d),C */
        case 0xA2: /* RES 4,(IX+d),D */
        case 0xA3: /* RES 4,(IX+d),E */
        case 0xA4: /* RES 4,(IX+d),H */
        case 0xA5: /* RES 4,(IX+d),L */
        case 0xA6: /* RES 4,(IX+d)   */
        case 0xA7: /* RES 4,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) & 0xEF;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xA8: /* RES 5,(IX+d),B */
        case 0xA9: /* RES 5,(IX+d),C */
        case 0xAA: /* RES 5,(IX+d),D */
        case 0xAB: /* RES 5,(IX+d),E */
        case 0xAC: /* RES 5,(IX+d),H */
        case 0xAD: /* RES 5,(IX+d),L */
        case 0xAE: /* RES 5,(IX+d)   */
        case 0xAF: /* RES 5,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) & 0xDF;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xB0: /* RES 6,(IX+d),B */
        case 0xB1: /* RES 6,(IX+d),C */
        case 0xB2: /* RES 6,(IX+d),D */
        case 0xB3: /* RES 6,(IX+d),E */
        case 0xB4: /* RES 6,(IX+d),H */
        case 0xB5: /* RES 6,(IX+d),L */
        case 0xB6: /* RES 6,(IX+d)   */
        case 0xB7: /* RES 6,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) & 0xBF;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xB8: /* RES 7,(IX+d),B */
        case 0xB9: /* RES 7,(IX+d),C */
        case 0xBA: /* RES 7,(IX+d),D */
        case 0xBB: /* RES 7,(IX+d),E */
        case 0xBC: /* RES 7,(IX+d),H */
        case 0xBD: /* RES 7,(IX+d),L */
        case 0xBE: /* RES 7,(IX+d)   */
        case 0xBF: /* RES 7,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) & 0x7F;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xC0: /* SET 0,(IX+d),B */
        case 0xC1: /* SET 0,(IX+d),C */
        case 0xC2: /* SET 0,(IX+d),D */
        case 0xC3: /* SET 0,(IX+d),E */
        case 0xC4: /* SET 0,(IX+d),H */
        case 0xC5: /* SET 0,(IX+d),L */
        case 0xC6: /* SET 0,(IX+d)   */
        case 0xC7: /* SET 0,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) | 0x01;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xC8: /* SET 1,(IX+d),B */
        case 0xC9: /* SET 1,(IX+d),C */
        case 0xCA: /* SET 1,(IX+d),D */
        case 0xCB: /* SET 1,(IX+d),E */
        case 0xCC: /* SET 1,(IX+d),H */
        case 0xCD: /* SET 1,(IX+d),L */
        case 0xCE: /* SET 1,(IX+d)   */
        case 0xCF: /* SET 1,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) | 0x02;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xD0: /* SET 2,(IX+d),B */
        case 0xD1: /* SET 2,(IX+d),C */
        case 0xD2: /* SET 2,(IX+d),D */
        case 0xD3: /* SET 2,(IX+d),E */
        case 0xD4: /* SET 2,(IX+d),H */
        case 0xD5: /* SET 2,(IX+d),L */
        case 0xD6: /* SET 2,(IX+d)   */
        case 0xD7: /* SET 2,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) | 0x04;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xD8: /* SET 3,(IX+d),B */
        case 0xD9: /* SET 3,(IX+d),C */
        case 0xDA: /* SET 3,(IX+d),D */
        case 0xDB: /* SET 3,(IX+d),E */
        case 0xDC: /* SET 3,(IX+d),H */
        case 0xDD: /* SET 3,(IX+d),L */
        case 0xDE: /* SET 3,(IX+d)   */
        case 0xDF: /* SET 3,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) | 0x08;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xE0: /* SET 4,(IX+d),B */
        case 0xE1: /* SET 4,(IX+d),C */
        case 0xE2: /* SET 4,(IX+d),D */
        case 0xE3: /* SET 4,(IX+d),E */
        case 0xE4: /* SET 4,(IX+d),H */
        case 0xE5: /* SET 4,(IX+d),L */
        case 0xE6: /* SET 4,(IX+d)   */
        case 0xE7: /* SET 4,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) | 0x10;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xE8: /* SET 5,(IX+d),B */
        case 0xE9: /* SET 5,(IX+d),C */
        case 0xEA: /* SET 5,(IX+d),D */
        case 0xEB: /* SET 5,(IX+d),E */
        case 0xEC: /* SET 5,(IX+d),H */
        case 0xED: /* SET 5,(IX+d),L */
        case 0xEE: /* SET 5,(IX+d)   */
        case 0xEF: /* SET 5,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) | 0x20;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xF0: /* SET 6,(IX+d),B */
        case 0xF1: /* SET 6,(IX+d),C */
        case 0xF2: /* SET 6,(IX+d),D */
        case 0xF3: /* SET 6,(IX+d),E */
        case 0xF4: /* SET 6,(IX+d),H */
        case 0xF5: /* SET 6,(IX+d),L */
        case 0xF6: /* SET 6,(IX+d)   */
        case 0xF7: /* SET 6,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) | 0x40;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
        case 0xF8: /* SET 7,(IX+d),B */
        case 0xF9: /* SET 7,(IX+d),C */
        case 0xFA: /* SET 7,(IX+d),D */
        case 0xFB: /* SET 7,(IX+d),E */
        case 0xFC: /* SET 7,(IX+d),H */
        case 0xFD: /* SET 7,(IX+d),L */
        case 0xFE: /* SET 7,(IX+d)   */
        case 0xFF: /* SET 7,(IX+d),A */
        {
            uint8_t work8 = Z80Ops::peek8(address) | 0x80;
            Z80Ops::addressOnBus(address, 1);
            Z80Ops::poke8(address, work8);
            copyToRegister(opCode, work8);
            break;
        }
    }
}

//Subconjunto de instrucciones 0xED

void Z80::decodeED(uint8_t opCode) {
    switch (opCode) {
        case 0x40:
        { /* IN B,(C) */
            REG_WZ = REG_BC;
            REG_B = Z80Ops::inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_B];
            flagQ = true;
            break;
        }
        case 0x41:
        { /* OUT (C),B */
            REG_WZ = REG_BC;
            Z80Ops::outPort(REG_WZ, REG_B);
            REG_WZ++;
            break;
        }
        case 0x42:
        { /* SBC HL,BC */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            sbc16(REG_BC);
            break;
        }
        case 0x43:
        { /* LD (nn),BC */
            REG_WZ = Z80Ops::peek16(REG_PC);
            Z80Ops::poke16(REG_WZ, regBC);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x44:
        case 0x4C:
        case 0x54:
        case 0x5C:
        case 0x64:
        case 0x6C:
        case 0x74:
        case 0x7C:
        { /* NEG */
            uint8_t aux = regA;
            regA = 0;
            carryFlag = false;
            sbc(aux);
            break;
        }
        case 0x45:
        case 0x4D: /* RETI */
        case 0x55:
        case 0x5D:
        case 0x65:
        case 0x6D:
        case 0x75:
        case 0x7D:
        { /* RETN */
            ffIFF1 = ffIFF2;
            REG_PC = REG_WZ = pop();
            break;
        }
        case 0x46:
        case 0x4E:
        case 0x66:
        case 0x6E:
        { /* IM 0 */
            modeINT = IntMode::IM0;
            break;
        }
        case 0x47:
        { /* LD I,A */
            /*
             * El par IR se pone en el bus de direcciones *antes*
             * de poner A en el registro I. Detalle importante.
             */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            regI = regA;
            break;
        }
        case 0x48:
        { /* IN C,(C) */
            REG_WZ = REG_BC;
            REG_C = Z80Ops::inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_C];
            flagQ = true;
            break;
        }
        case 0x49:
        { /* OUT (C),C */
            REG_WZ = REG_BC;
            Z80Ops::outPort(REG_WZ, REG_C);
            REG_WZ++;
            break;
        }
        case 0x4A:
        { /* ADC HL,BC */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            adc16(REG_BC);
            break;
        }
        case 0x4B:
        { /* LD BC,(nn) */
            REG_WZ = Z80Ops::peek16(REG_PC);
            REG_BC = Z80Ops::peek16(REG_WZ);
            REG_WZ++;
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x4F:
        { /* LD R,A */
            /*
             * El par IR se pone en el bus de direcciones *antes*
             * de poner A en el registro R. Detalle importante.
             */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            setRegR(regA);
            break;
        }
        case 0x50:
        { /* IN D,(C) */
            REG_WZ = REG_BC;
            REG_D = Z80Ops::inPort(REG_WZ);
            REG_WZ++;
            sz5h3pnFlags = sz53pn_addTable[REG_D];
            flagQ = true;
            break;
        }
        case 0x51:
        { /* OUT (C),D */
            REG_WZ = REG_BC;
            Z80Ops::outPort(REG_WZ++, REG_D);
            break;
        }
        case 0x52:
        { /* SBC HL,DE */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            sbc16(REG_DE);
            break;
        }
        case 0x53:
        { /* LD (nn),DE */
            REG_WZ = Z80Ops::peek16(REG_PC);
            Z80Ops::poke16(REG_WZ++, regDE);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x56:
        case 0x76:
        { /* IM 1 */
            modeINT = IntMode::IM1;
            break;
        }
        case 0x57:
        { /* LD A,I */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            regA = regI;
            sz5h3pnFlags = sz53n_addTable[regA];
            if (ffIFF2 && !Z80Ops::isActiveINT()) {
                sz5h3pnFlags |= PARITY_MASK;
            }
            flagQ = true;
            break;
        }
        case 0x58:
        { /* IN E,(C) */
            REG_WZ = REG_BC;
            REG_E = Z80Ops::inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_E];
            flagQ = true;
            break;
        }
        case 0x59:
        { /* OUT (C),E */
            REG_WZ = REG_BC;
            Z80Ops::outPort(REG_WZ++, REG_E);
            break;
        }
        case 0x5A:
        { /* ADC HL,DE */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            adc16(REG_DE);
            break;
        }
        case 0x5B:
        { /* LD DE,(nn) */
            REG_WZ = Z80Ops::peek16(REG_PC);
            REG_DE = Z80Ops::peek16(REG_WZ++);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x5E:
        case 0x7E:
        { /* IM 2 */
            modeINT = IntMode::IM2;
            break;
        }
        case 0x5F:
        { /* LD A,R */
            Z80Ops::addressOnBus(getPairIR().word, 1);
            regA = getRegR();
            sz5h3pnFlags = sz53n_addTable[regA];
            if (ffIFF2 && !Z80Ops::isActiveINT()) {
                sz5h3pnFlags |= PARITY_MASK;
            }
            flagQ = true;
            break;
        }
        case 0x60:
        { /* IN H,(C) */
            REG_WZ = REG_BC;
            REG_H = Z80Ops::inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_H];
            flagQ = true;
            break;
        }
        case 0x61:
        { /* OUT (C),H */
            REG_WZ = REG_BC;
            Z80Ops::outPort(REG_WZ++, REG_H);
            break;
        }
        case 0x62:
        { /* SBC HL,HL */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            sbc16(REG_HL);
            break;
        }
        case 0x63:
        { /* LD (nn),HL */
            REG_WZ = Z80Ops::peek16(REG_PC);
            Z80Ops::poke16(REG_WZ++, regHL);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x67:
        { /* RRD */
            // A = A7 A6 A5 A4 (HL)3 (HL)2 (HL)1 (HL)0
            // (HL) = A3 A2 A1 A0 (HL)7 (HL)6 (HL)5 (HL)4
            // Los bits 3,2,1 y 0 de (HL) se copian a los bits 3,2,1 y 0 de A.
            // Los 4 bits bajos que había en A se copian a los bits 7,6,5 y 4 de (HL).
            // Los 4 bits altos que había en (HL) se copian a los 4 bits bajos de (HL)
            // Los 4 bits superiores de A no se tocan. ¡p'habernos matao!
            uint8_t aux = regA << 4;
            REG_WZ = REG_HL;
            uint16_t memHL = Z80Ops::peek8(REG_WZ);
            regA = (regA & 0xf0) | (memHL & 0x0f);
            Z80Ops::addressOnBus(REG_WZ, 4);
            Z80Ops::poke8(REG_WZ++, (memHL >> 4) | aux);
            sz5h3pnFlags = sz53pn_addTable[regA];
            flagQ = true;
            break;
        }
        case 0x68:
        { /* IN L,(C) */
            REG_WZ = REG_BC;
            REG_L = Z80Ops::inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[REG_L];
            flagQ = true;
            break;
        }
        case 0x69:
        { /* OUT (C),L */
            REG_WZ = REG_BC;
            Z80Ops::outPort(REG_WZ++, REG_L);
            break;
        }
        case 0x6A:
        { /* ADC HL,HL */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            adc16(REG_HL);
            break;
        }
        case 0x6B:
        { /* LD HL,(nn) */
            REG_WZ = Z80Ops::peek16(REG_PC);
            REG_HL = Z80Ops::peek16(REG_WZ++);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x6F:
        { /* RLD */
            // A = A7 A6 A5 A4 (HL)7 (HL)6 (HL)5 (HL)4
            // (HL) = (HL)3 (HL)2 (HL)1 (HL)0 A3 A2 A1 A0
            // Los 4 bits bajos que había en (HL) se copian a los bits altos de (HL).
            // Los 4 bits altos que había en (HL) se copian a los 4 bits bajos de A
            // Los bits 3,2,1 y 0 de A se copian a los bits 3,2,1 y 0 de (HL).
            // Los 4 bits superiores de A no se tocan. ¡p'habernos matao!
            uint8_t aux = regA & 0x0f;
            REG_WZ = REG_HL;
            uint16_t memHL = Z80Ops::peek8(REG_WZ);
            regA = (regA & 0xf0) | (memHL >> 4);
            Z80Ops::addressOnBus(REG_WZ, 4);
            Z80Ops::poke8(REG_WZ++, (memHL << 4) | aux);
            sz5h3pnFlags = sz53pn_addTable[regA];
            flagQ = true;
            break;
        }
        case 0x70:
        { /* IN (C) */
            REG_WZ = REG_BC;
            uint8_t inPort = Z80Ops::inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[inPort];
            flagQ = true;
            break;
        }
        case 0x71:
        { /* OUT (C),0 */
            REG_WZ = REG_BC;
            Z80Ops::outPort(REG_WZ++, 0x00);
            break;
        }
        case 0x72:
        { /* SBC HL,SP */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            sbc16(REG_SP);
            break;
        }
        case 0x73:
        { /* LD (nn),SP */
            REG_WZ = Z80Ops::peek16(REG_PC);
            Z80Ops::poke16(REG_WZ++, regSP);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0x78:
        { /* IN A,(C) */
            REG_WZ = REG_BC;
            regA = Z80Ops::inPort(REG_WZ++);
            sz5h3pnFlags = sz53pn_addTable[regA];
            flagQ = true;
            break;
        }
        case 0x79:
        { /* OUT (C),A */
            REG_WZ = REG_BC;
            Z80Ops::outPort(REG_WZ++, regA);
            break;
        }
        case 0x7A:
        { /* ADC HL,SP */
            Z80Ops::addressOnBus(getPairIR().word, 7);
            adc16(REG_SP);
            break;
        }
        case 0x7B:
        { /* LD SP,(nn) */
            REG_WZ = Z80Ops::peek16(REG_PC);
            REG_SP = Z80Ops::peek16(REG_WZ++);
            REG_PC = REG_PC + 2;
            break;
        }
        case 0xA0:
        { /* LDI */
            ldi();
            break;
        }
        case 0xA1:
        { /* CPI */
            cpi();
            break;
        }
        case 0xA2:
        { /* INI */
            ini();
            break;
        }
        case 0xA3:
        { /* OUTI */
            outi();
            break;
        }
        case 0xA8:
        { /* LDD */
            ldd();
            break;
        }
        case 0xA9:
        { /* CPD */
            cpd();
            break;
        }
        case 0xAA:
        { /* IND */
            ind();
            break;
        }
        case 0xAB:
        { /* OUTD */
            outd();
            break;
        }
        case 0xB0:
        { /* LDIR */
            ldi();
            if (REG_BC != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80Ops::addressOnBus(REG_DE - 1, 5);
            }
            break;
        }
        case 0xB1:
        { /* CPIR */
            cpi();
            if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK
                    && (sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80Ops::addressOnBus(REG_HL - 1, 5);
            }
            break;
        }
        case 0xB2:
        { /* INIR */
            ini();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                Z80Ops::addressOnBus(REG_HL - 1, 5);
            }
            break;
        }
        case 0xB3:
        { /* OTIR */
            outi();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                Z80Ops::addressOnBus(REG_BC, 5);
            }
            break;
        }
        case 0xB8:
        { /* LDDR */
            ldd();
            if (REG_BC != 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80Ops::addressOnBus(REG_DE + 1, 5);
            }
            break;
        }
        case 0xB9:
        { /* CPDR */
            cpd();
            if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK
                    && (sz5h3pnFlags & ZERO_MASK) == 0) {
                REG_PC = REG_PC - 2;
                REG_WZ = REG_PC + 1;
                Z80Ops::addressOnBus(REG_HL + 1, 5);
            }
            break;
        }
        case 0xBA:
        { /* INDR */
            ind();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                Z80Ops::addressOnBus(REG_HL + 1, 5);
            }
            break;
        }
        case 0xBB:
        { /* OTDR */
            outd();
            if (REG_B != 0) {
                REG_PC = REG_PC - 2;
                Z80Ops::addressOnBus(REG_BC, 5);
            }
            break;
        }
        case 0xDD:
            prefixOpcode = 0xDD;
            break;
        case 0xED:
            prefixOpcode = 0xED;
            break;
        case 0xFD:
            prefixOpcode = 0xFD;
            break;
        default:
        {
            break;
        }
    }
}

void Z80::copyToRegister(uint8_t opCode, uint8_t value)
{
    switch (opCode & 0x07)
    {
        case 0x00:
            REG_B = value;
            break;
        case 0x01:
            REG_C = value;
            break;
        case 0x02:
            REG_D = value;
            break;
        case 0x03:
            REG_E = value;
            break;
        case 0x04:
            REG_H = value;
            break;
        case 0x05:
            REG_L = value;
            break;
        case 0x07:
            regA = value;
        default:
            break;
    }
}
