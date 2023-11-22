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

#ifndef Z80CPP_H
#define Z80CPP_H

#include <stdint.h>

#define Z80CPP_IS_LITTLE_ENDIAN 1

/* Union allowing a register pair to be accessed as bytes or as a word */
typedef union {
#ifdef Z80CPP_IS_LITTLE_ENDIAN
	struct {
		uint8_t lo, hi;
	} byte8;
#else
    struct {
        uint8_t hi, lo; 
    } byte8;
#endif
    uint16_t word;
} RegisterPair;

#include "z80operations.h"

#define REG_B   regBC.byte8.hi
#define REG_C   regBC.byte8.lo
#define REG_BC  regBC.word
#define REG_Bx  regBCx.byte8.hi
#define REG_Cx  regBCx.byte8.lo
#define REG_BCx regBCx.word

#define REG_D   regDE.byte8.hi
#define REG_E   regDE.byte8.lo
#define REG_DE  regDE.word
#define REG_Dx  regDEx.byte8.hi
#define REG_Ex  regDEx.byte8.lo
#define REG_DEx regDEx.word

#define REG_H   regHL.byte8.hi
#define REG_L   regHL.byte8.lo
#define REG_HL  regHL.word
#define REG_Hx  regHLx.byte8.hi
#define REG_Lx  regHLx.byte8.lo
#define REG_HLx regHLx.word

#define REG_IXh regIX.byte8.hi
#define REG_IXl regIX.byte8.lo
#define REG_IX  regIX.word

#define REG_IYh regIY.byte8.hi
#define REG_IYl regIY.byte8.lo
#define REG_IY  regIY.word

#define REG_Ax  regAFx.byte8.hi
#define REG_Fx  regAFx.byte8.lo
#define REG_AFx regAFx.word

#define REG_PCh regPC.byte8.hi
#define REG_PCl regPC.byte8.lo
#define REG_PC  regPC.word

#define REG_S   regSP.byte8.hi
#define REG_P   regSP.byte8.lo
#define REG_SP  regSP.word

#define REG_W   memptr.byte8.hi
#define REG_Z   memptr.byte8.lo
#define REG_WZ  memptr.word

class Z80 {
public:
    // Modos de interrupción
    enum IntMode {
        IM0, IM1, IM2
    };
private:
    // Código de instrucción a ejecutar
    // Poner esta variable como local produce peor rendimiento
    // ZEXALL test: (local) 1:54 vs 1:47 (visitante)
    static uint8_t opCode;
    // Se está ejecutando una instrucción prefijada con DD, ED o FD
    // Los valores permitidos son [0x00, 0xDD, 0xED, 0xFD]
    // El prefijo 0xCB queda al margen porque, detrás de 0xCB, siempre
    // viene un código de instrucción válido, tanto si delante va un
    // 0xDD o 0xFD como si no.
    static uint8_t prefixOpcode;
    // Subsistema de notificaciones
    static bool execDone;
    // Posiciones de los flags
    const static uint8_t CARRY_MASK = 0x01;
    const static uint8_t ADDSUB_MASK = 0x02;
    const static uint8_t PARITY_MASK = 0x04;
    const static uint8_t OVERFLOW_MASK = 0x04; // alias de PARITY_MASK
    const static uint8_t BIT3_MASK = 0x08;
    const static uint8_t HALFCARRY_MASK = 0x10;
    const static uint8_t BIT5_MASK = 0x20;
    const static uint8_t ZERO_MASK = 0x40;
    const static uint8_t SIGN_MASK = 0x80;
    // Máscaras de conveniencia
    const static uint8_t FLAG_53_MASK = BIT5_MASK | BIT3_MASK;
    const static uint8_t FLAG_SZ_MASK = SIGN_MASK | ZERO_MASK;
    const static uint8_t FLAG_SZHN_MASK = FLAG_SZ_MASK | HALFCARRY_MASK | ADDSUB_MASK;
    const static uint8_t FLAG_SZP_MASK = FLAG_SZ_MASK | PARITY_MASK;
    const static uint8_t FLAG_SZHP_MASK = FLAG_SZP_MASK | HALFCARRY_MASK;
    // Acumulador y resto de registros de 8 bits
    static uint8_t regA;
    // Flags sIGN, zERO, 5, hALFCARRY, 3, pARITY y ADDSUB (n)
    static uint8_t sz5h3pnFlags;
    // El flag Carry es el único que se trata aparte
    static bool carryFlag;
    // Registros principales y alternativos
    static RegisterPair regBC, regBCx, regDE, regDEx, regHL, regHLx;
    /* Flags para indicar la modificación del registro F en la instrucción actual
     * y en la anterior.
     * Son necesarios para emular el comportamiento de los bits 3 y 5 del
     * registro F con las instrucciones CCF/SCF.
     *
     * http://www.worldofspectrum.org/forums/showthread.php?t=41834
     * http://www.worldofspectrum.org/forums/showthread.php?t=41704
     *
     * Thanks to Patrik Rak for his tests and investigations.
     */
    static bool flagQ, lastFlagQ;

    // Acumulador alternativo y flags -- 8 bits
    static RegisterPair regAFx;

    // Registros de propósito específico
    // *PC -- Program Counter -- 16 bits*
    static RegisterPair regPC;
    // *IX -- Registro de índice -- 16 bits*
    static RegisterPair regIX;
    // *IY -- Registro de índice -- 16 bits*
    static RegisterPair regIY;
    // *SP -- Stack Pointer -- 16 bits*
    static RegisterPair regSP;
    // *I -- Vector de interrupción -- 8 bits*
    static uint8_t regI;
    // *R -- Refresco de memoria -- 7 bits*
    static uint8_t regR;
    // *R7 -- Refresco de memoria -- 1 bit* (bit superior de R)
    static bool regRbit7;
    //Flip-flops de interrupción
    static bool ffIFF1;
    static bool ffIFF2;
    // EI solo habilita las interrupciones DESPUES de ejecutar la
    // siguiente instrucción (excepto si la siguiente instrucción es un EI...)
    static bool pendingEI;
    // Estado de la línea NMI
    static bool activeNMI;
    // Modo de interrupción
    static IntMode modeINT;
    // halted == true cuando la CPU está ejecutando un HALT (28/03/2010)
    static bool halted;
    // pinReset == true, se ha producido un reset a través de la patilla
    static bool pinReset;
    /*
     * Registro interno que usa la CPU de la siguiente forma
     *
     * ADD HL,xx      = Valor del registro H antes de la suma
     * LD r,(IX/IY+d) = Byte superior de la suma de IX/IY+d
     * JR d           = Byte superior de la dirección de destino del salto
     *
     * 04/12/2008     No se vayan todavía, aún hay más. Con lo que se ha
     *                implementado hasta ahora parece que funciona. El resto de
     *                la historia está contada en:
     *                http://zx.pk.ru/attachment.php?attachmentid=2989
     *
     * 25/09/2009     Se ha completado la emulación de MEMPTR. A señalar que
     *                no se puede comprobar si MEMPTR se ha emulado bien hasta
     *                que no se emula el comportamiento del registro en las
     *                instrucciones CPI y CPD. Sin ello, todos los tests de
     *                z80tests.tap fallarán aunque se haya emulado bien al
     *                registro en TODAS las otras instrucciones.
     *                Shit yourself, little parrot.
     */

    static RegisterPair memptr;
    // I and R registers
    static inline RegisterPair getPairIR(void);

    /* Algunos flags se precalculan para un tratamiento más rápido
     * Concretamente, SIGN, ZERO, los bits 3, 5, PARITY y ADDSUB:
     * sz53n_addTable tiene el ADDSUB flag a 0 y paridad sin calcular
     * sz53pn_addTable tiene el ADDSUB flag a 0 y paridad calculada
     * sz53n_subTable tiene el ADDSUB flag a 1 y paridad sin calcular
     * sz53pn_subTable tiene el ADDSUB flag a 1 y paridad calculada
     * El resto de bits están a 0 en las cuatro tablas lo que es
     * importante para muchas operaciones que ponen ciertos flags a 0 por real
     * decreto. Si lo ponen a 1 por el mismo método basta con hacer un OR con
     * la máscara correspondiente.
     */
    static uint8_t sz53n_addTable[256];
    static uint8_t sz53pn_addTable[256];
    static uint8_t sz53n_subTable[256];
    static uint8_t sz53pn_subTable[256];

    // Un true en una dirección indica que se debe notificar que se va a
    // ejecutar la instrucción que está en esa direción.
#ifdef WITH_BREAKPOINT_SUPPORT
    static bool breakpointEnabled {false};
#endif
    static void copyToRegister(uint8_t value);

public:
    // Constructor de la clase
    static void create();
    static void destroy();

    // Acceso a registros de 8 bits
    // Access to 8-bit registers
    static uint8_t getRegA(void) { return regA; }
    static void setRegA(uint8_t value) { regA = value; }

    static uint8_t getRegB(void) { return REG_B; }
    static void setRegB(uint8_t value) { REG_B = value; }

    static uint8_t getRegC(void) { return REG_C; }
    static void setRegC(uint8_t value) { REG_C = value; }

    static uint8_t getRegD(void) { return REG_D; }
    static void setRegD(uint8_t value) { REG_D = value; }

    static uint8_t getRegE(void) { return REG_E; }
    static void setRegE(uint8_t value) { REG_E = value; }

    static uint8_t getRegH(void) { return REG_H; }
    static void setRegH(uint8_t value) { REG_H = value; }

    static uint8_t getRegL(void) { return REG_L; }
    static void setRegL(uint8_t value) { REG_L = value; }

    // Acceso a registros alternativos de 8 bits
    // Access to alternate 8-bit registers
    static uint8_t getRegAx(void) { return REG_Ax; }
    static void setRegAx(uint8_t value) { REG_Ax = value; }

    static uint8_t getRegFx(void) { return REG_Fx; }
    static void setRegFx(uint8_t value) { REG_Fx = value; }

    static uint8_t getRegBx(void) { return REG_Bx; }
    static void setRegBx(uint8_t value) { REG_Bx = value; }

    static uint8_t getRegCx(void) { return REG_Cx; }
    static void setRegCx(uint8_t value) { REG_Cx = value; }

    static uint8_t getRegDx(void) { return REG_Dx; }
    static void setRegDx(uint8_t value) { REG_Dx = value; }

    static uint8_t getRegEx(void) { return REG_Ex; }
    static void setRegEx(uint8_t value) { REG_Ex = value; }

    static uint8_t getRegHx(void) { return REG_Hx; }
    static void setRegHx(uint8_t value) { REG_Hx = value; }

    static uint8_t getRegLx(void) { return REG_Lx; }
    static void setRegLx(uint8_t value) { REG_Lx = value; }

    // Acceso a registros de 16 bits
    // Access to registers pairs
    static uint16_t getRegAF(void) { return (regA << 8) | (carryFlag ? sz5h3pnFlags | CARRY_MASK : sz5h3pnFlags); }
    static void setRegAF(uint16_t word) { regA = word >> 8; sz5h3pnFlags = word & 0xfe; carryFlag = (word & CARRY_MASK) != 0; }

    static uint16_t getRegAFx(void) { return REG_AFx; }
    static void setRegAFx(uint16_t word) { REG_AFx = word; }

    static uint16_t getRegBC(void) { return REG_BC; }
    static void setRegBC(uint16_t word) { REG_BC = word; }

    static uint16_t getRegBCx(void) { return REG_BCx; }
    static void setRegBCx(uint16_t word) { REG_BCx = word; }

    static uint16_t getRegDE(void) { return REG_DE; }
    static void setRegDE(uint16_t word) { REG_DE = word; }

    static uint16_t getRegDEx(void) { return REG_DEx; }
    static void setRegDEx(uint16_t word) { REG_DEx = word; }

    static uint16_t getRegHL(void) { return REG_HL; }
    static void setRegHL(uint16_t word) { REG_HL = word; }

    static uint16_t getRegHLx(void) { return REG_HLx; }
    static void setRegHLx(uint16_t word) { REG_HLx = word; }

    // Acceso a registros de propósito específico
    // Access to special purpose registers
    static uint16_t getRegPC(void) { return REG_PC; }
    static void setRegPC(uint16_t address) { REG_PC = address; }

    static uint16_t getRegSP(void) { return REG_SP; }
    static void setRegSP(uint16_t word) { REG_SP = word; }

    static uint16_t getRegIX(void) { return REG_IX; }
    static void setRegIX(uint16_t word) { REG_IX = word; }

    static uint16_t getRegIY(void) { return REG_IY; }
    static void setRegIY(uint16_t word) { REG_IY = word; }

    static uint8_t getRegI(void) { return regI; }
    static void setRegI(uint8_t value) { regI = value; }

    static uint8_t getRegR(void) { return regRbit7 ? regR | SIGN_MASK : regR & 0x7f; }
    static void setRegR(uint8_t value) { regR = value & 0x7f; regRbit7 = (value > 0x7f); }

    // Acceso al registro oculto MEMPTR
    // Hidden register MEMPTR (known as WZ at Zilog doc?)
    static uint16_t getMemPtr(void) { return REG_WZ; }
    static void setMemPtr(uint16_t word) { REG_WZ = word; }

    // Acceso a los flags uno a uno
    // Access to single flags from F register
    static bool isCarryFlag(void) { return carryFlag; }
    static void setCarryFlag(bool state) { carryFlag = state; }

    static bool isAddSubFlag(void) { return (sz5h3pnFlags & ADDSUB_MASK) != 0; }
    static void setAddSubFlag(bool state);

    static bool isParOverFlag(void) { return (sz5h3pnFlags & PARITY_MASK) != 0; }
    static void setParOverFlag(bool state);

    /* Undocumented flag */
    static bool isBit3Flag(void) { return (sz5h3pnFlags & BIT3_MASK) != 0; }
    static void setBit3Fag(bool state);

    static bool isHalfCarryFlag(void) { return (sz5h3pnFlags & HALFCARRY_MASK) != 0; }
    static void setHalfCarryFlag(bool state);

    /* Undocumented flag */
    static bool isBit5Flag(void) { return (sz5h3pnFlags & BIT5_MASK) != 0; }
    static void setBit5Flag(bool state);

    static bool isZeroFlag(void) { return (sz5h3pnFlags & ZERO_MASK) != 0; }
    static void setZeroFlag(bool state);

    static bool isSignFlag(void) { return sz5h3pnFlags >= SIGN_MASK; }
    static void setSignFlag(bool state);

    // Acceso a los flags F
    // Access to F register
    static uint8_t getFlags(void) { return carryFlag ? sz5h3pnFlags | CARRY_MASK : sz5h3pnFlags; }
    static void setFlags(uint8_t regF) { sz5h3pnFlags = regF & 0xfe; carryFlag = (regF & CARRY_MASK) != 0; }

    // Acceso a los flip-flops de interrupción
    // Interrupt flip-flops
    static bool isIFF1(void) { return ffIFF1; }
    static void setIFF1(bool state) { ffIFF1 = state; }

    static bool isIFF2(void) { return ffIFF2; }
    static void setIFF2(bool state) { ffIFF2 = state; }

    static bool isNMI(void) { return activeNMI; }
    static void setNMI(bool nmi) { activeNMI = nmi; }

    // /NMI is negative level triggered.
    static void triggerNMI(void) { activeNMI = true; }

    //Acceso al modo de interrupción
    // Maskable interrupt mode
    static IntMode getIM(void) { return modeINT; }
    static void setIM(IntMode mode) { modeINT = mode; }

    static bool isHalted(void) { return halted; }
    static void setHalted(bool state) { halted = state; }

    // Reset requested by /RESET signal (not power-on)
    static void setPinReset(void) { pinReset = true; }

    static bool isPendingEI(void) { return pendingEI; }
    static void setPendingEI(bool state) { pendingEI = state; }

    // Reset
    static void reset(void);

    // Execute one instruction
    static void execute();
    static void exec_nocheck();

    // Check INT
    static void checkINT(void);

    static void incRegR(uint8_t inc);

    static void Xor(uint8_t oper8);

    static void Cp(uint8_t oper8);    

#ifdef WITH_BREAKPOINT_SUPPORT
    static bool isBreakpoint(void) { return breakpointEnabled; }
    static void setBreakpoint(bool state) { breakpointEnabled = state; }
#endif

#ifdef WITH_EXEC_DONE
    static void setExecDone(bool status) { execDone = status; }
#endif

private:
    // Rota a la izquierda el valor del argumento
    static inline void rlc(uint8_t &oper8);

    // Rota a la izquierda el valor del argumento
    static inline void rl(uint8_t &oper8);

    // Rota a la izquierda el valor del argumento
    static inline void sla(uint8_t &oper8);

    // Rota a la izquierda el valor del argumento (como sla salvo por el bit 0)
    static inline void sll(uint8_t &oper8);

    // Rota a la derecha el valor del argumento
    static inline void rrc(uint8_t &oper8);

    // Rota a la derecha el valor del argumento
    static inline void rr(uint8_t &oper8);

    // Rota a la derecha 1 bit el valor del argumento
    static inline void sra(uint8_t &oper8);

    // Rota a la derecha 1 bit el valor del argumento
    static inline void srl(uint8_t &oper8);

    // Incrementa un valor de 8 bits modificando los flags oportunos
    static inline void inc8(uint8_t &oper8);

    // Decrementa un valor de 8 bits modificando los flags oportunos
    static inline void dec8(uint8_t &oper8);

    // Suma de 8 bits afectando a los flags
    static inline void add(uint8_t oper8);

    // Suma con acarreo de 8 bits
    static inline void adc(uint8_t oper8);

    // Suma dos operandos de 16 bits sin carry afectando a los flags
    static inline void add16(RegisterPair &reg16, uint16_t oper16);

    // Suma con acarreo de 16 bits
    static inline void adc16(uint16_t reg16);

    // Resta de 8 bits
    static inline void sub(uint8_t oper8);

    // Resta con acarreo de 8 bits
    static inline void sbc(uint8_t oper8);

    // Resta con acarreo de 16 bits
    static inline void sbc16(uint16_t reg16);

    // Operación AND lógica
    // Simple 'and' is C++ reserved keyword
    static inline void and_(uint8_t oper8);

    // Operación XOR lógica
    // Simple 'xor' is C++ reserved keyword
    static inline void xor_(uint8_t oper8);

    // Operación OR lógica
    // Simple 'or' is C++ reserved keyword
    static inline void or_(uint8_t oper8);

    // Operación de comparación con el registro A
    // es como SUB, pero solo afecta a los flags
    // Los flags SIGN y ZERO se calculan a partir del resultado
    // Los flags 3 y 5 se copian desde el operando (sigh!)
    static inline void cp(uint8_t oper8);

    // DAA
    static inline void daa(void);

    // POP
    static inline uint16_t pop(void);

    // PUSH
    static inline void push(uint16_t word);

    // LDI
    static void ldi(void);

    // LDD
    static void ldd(void);

    // CPI
    static void cpi(void);

    // CPD
    static void cpd(void);

    // INI
    static void ini(void);

    // IND
    static void ind(void);

    // OUTI
    static void outi(void);

    // OUTD
    static void outd(void);

    static void SetAbortedINxR_OTxRFlags();

    // BIT n,r
    static inline void bitTest(uint8_t mask, uint8_t reg);

    //Interrupción
    static void interrupt(void);

    //Interrupción NMI
    static void nmi(void);

    // Decode main opcodes
    // static void decodeOpcode();

    // Subconjunto de instrucciones 0xCB
    // decode CBXX opcodes
    // static void decodeCB(void);

    //Subconjunto de instrucciones 0xDD / 0xFD
    // Decode DD/FD opcodes
    static void decodeDDFD(RegisterPair& regIXY);

    // Subconjunto de instrucciones 0xDD / 0xFD 0xCB
    // Decode DD / FD CB opcodes
    static void decodeDDFDCB(uint16_t address);

    //Subconjunto de instrucciones 0xED
    // Decode EDXX opcodes
    static void decodeED(void);

    static void (*dcOpcode[256])();
    static void (*dcCB[256])();

    static void decodeOpcode00(void);
    static void decodeOpcode01(void);
    static void decodeOpcode02(void);
    static void decodeOpcode03(void);
    static void decodeOpcode04(void);
    static void decodeOpcode05(void);
    static void decodeOpcode06(void);                        
    static void decodeOpcode07(void);
    static void decodeOpcode08(void);
    static void decodeOpcode09(void);
    static void decodeOpcode0a(void);
    static void decodeOpcode0b(void);                    
    static void decodeOpcode0c(void);                    
    static void decodeOpcode0d(void);                    
    static void decodeOpcode0e(void);                    
    static void decodeOpcode0f(void);                    

    static void decodeOpcode10(void);
    static void decodeOpcode11(void);
    static void decodeOpcode12(void);
    static void decodeOpcode13(void);
    static void decodeOpcode14(void);
    static void decodeOpcode15(void);
    static void decodeOpcode16(void);                        
    static void decodeOpcode17(void);
    static void decodeOpcode18(void);
    static void decodeOpcode19(void);
    static void decodeOpcode1a(void);
    static void decodeOpcode1b(void);                    
    static void decodeOpcode1c(void);                    
    static void decodeOpcode1d(void);                    
    static void decodeOpcode1e(void);                    
    static void decodeOpcode1f(void);                    

    static void decodeOpcode20(void);
    static void decodeOpcode21(void);
    static void decodeOpcode22(void);
    static void decodeOpcode23(void);
    static void decodeOpcode24(void);
    static void decodeOpcode25(void);
    static void decodeOpcode26(void);                        
    static void decodeOpcode27(void);
    static void decodeOpcode28(void);
    static void decodeOpcode29(void);
    static void decodeOpcode2a(void);
    static void decodeOpcode2b(void);                    
    static void decodeOpcode2c(void);                    
    static void decodeOpcode2d(void);                    
    static void decodeOpcode2e(void);                    
    static void decodeOpcode2f(void);                    

    static void decodeOpcode30(void);
    static void decodeOpcode31(void);
    static void decodeOpcode32(void);
    static void decodeOpcode33(void);
    static void decodeOpcode34(void);
    static void decodeOpcode35(void);
    static void decodeOpcode36(void);                        
    static void decodeOpcode37(void);
    static void decodeOpcode38(void);
    static void decodeOpcode39(void);
    static void decodeOpcode3a(void);
    static void decodeOpcode3b(void);                    
    static void decodeOpcode3c(void);                    
    static void decodeOpcode3d(void);                    
    static void decodeOpcode3e(void);                    
    static void decodeOpcode3f(void);                    

    static void decodeOpcode40(void);
    static void decodeOpcode41(void);
    static void decodeOpcode42(void);
    static void decodeOpcode43(void);
    static void decodeOpcode44(void);
    static void decodeOpcode45(void);
    static void decodeOpcode46(void);                        
    static void decodeOpcode47(void);
    static void decodeOpcode48(void);
    static void decodeOpcode49(void);
    static void decodeOpcode4a(void);
    static void decodeOpcode4b(void);                    
    static void decodeOpcode4c(void);                    
    static void decodeOpcode4d(void);                    
    static void decodeOpcode4e(void);                    
    static void decodeOpcode4f(void);                    

    static void decodeOpcode50(void);
    static void decodeOpcode51(void);
    static void decodeOpcode52(void);
    static void decodeOpcode53(void);
    static void decodeOpcode54(void);
    static void decodeOpcode55(void);
    static void decodeOpcode56(void);                        
    static void decodeOpcode57(void);
    static void decodeOpcode58(void);
    static void decodeOpcode59(void);
    static void decodeOpcode5a(void);
    static void decodeOpcode5b(void);                    
    static void decodeOpcode5c(void);                    
    static void decodeOpcode5d(void);                    
    static void decodeOpcode5e(void);                    
    static void decodeOpcode5f(void);                    

    static void decodeOpcode60(void);
    static void decodeOpcode61(void);
    static void decodeOpcode62(void);
    static void decodeOpcode63(void);
    static void decodeOpcode64(void);
    static void decodeOpcode65(void);
    static void decodeOpcode66(void);                        
    static void decodeOpcode67(void);
    static void decodeOpcode68(void);
    static void decodeOpcode69(void);
    static void decodeOpcode6a(void);
    static void decodeOpcode6b(void);                    
    static void decodeOpcode6c(void);                    
    static void decodeOpcode6d(void);                    
    static void decodeOpcode6e(void);                    
    static void decodeOpcode6f(void);                    

    static void decodeOpcode70(void);
    static void decodeOpcode71(void);
    static void decodeOpcode72(void);
    static void decodeOpcode73(void);
    static void decodeOpcode74(void);
    static void decodeOpcode75(void);
    static void decodeOpcode76(void);                        
    static void decodeOpcode77(void);
    static void decodeOpcode78(void);
    static void decodeOpcode79(void);
    static void decodeOpcode7a(void);
    static void decodeOpcode7b(void);                    
    static void decodeOpcode7c(void);                    
    static void decodeOpcode7d(void);                    
    static void decodeOpcode7e(void);                    
    static void decodeOpcode7f(void);                    

    static void decodeOpcode80(void);
    static void decodeOpcode81(void);
    static void decodeOpcode82(void);
    static void decodeOpcode83(void);
    static void decodeOpcode84(void);
    static void decodeOpcode85(void);
    static void decodeOpcode86(void);                        
    static void decodeOpcode87(void);
    static void decodeOpcode88(void);
    static void decodeOpcode89(void);
    static void decodeOpcode8a(void);
    static void decodeOpcode8b(void);                    
    static void decodeOpcode8c(void);                    
    static void decodeOpcode8d(void);                    
    static void decodeOpcode8e(void);                    
    static void decodeOpcode8f(void);                    

    static void decodeOpcode90(void);
    static void decodeOpcode91(void);
    static void decodeOpcode92(void);
    static void decodeOpcode93(void);
    static void decodeOpcode94(void);
    static void decodeOpcode95(void);
    static void decodeOpcode96(void);                        
    static void decodeOpcode97(void);
    static void decodeOpcode98(void);
    static void decodeOpcode99(void);
    static void decodeOpcode9a(void);
    static void decodeOpcode9b(void);                    
    static void decodeOpcode9c(void);                    
    static void decodeOpcode9d(void);                    
    static void decodeOpcode9e(void);                    
    static void decodeOpcode9f(void);                    

    static void decodeOpcodea0(void);
    static void decodeOpcodea1(void);
    static void decodeOpcodea2(void);
    static void decodeOpcodea3(void);
    static void decodeOpcodea4(void);
    static void decodeOpcodea5(void);
    static void decodeOpcodea6(void);                        
    static void decodeOpcodea7(void);
    static void decodeOpcodea8(void);
    static void decodeOpcodea9(void);
    static void decodeOpcodeaa(void);
    static void decodeOpcodeab(void);                    
    static void decodeOpcodeac(void);                    
    static void decodeOpcodead(void);                    
    static void decodeOpcodeae(void);                    
    static void decodeOpcodeaf(void);                    

    static void decodeOpcodeb0(void);
    static void decodeOpcodeb1(void);
    static void decodeOpcodeb2(void);
    static void decodeOpcodeb3(void);
    static void decodeOpcodeb4(void);
    static void decodeOpcodeb5(void);
    static void decodeOpcodeb6(void);                        
    static void decodeOpcodeb7(void);
    static void decodeOpcodeb8(void);
    static void decodeOpcodeb9(void);
    static void decodeOpcodeba(void);
    static void decodeOpcodebb(void);                    
    static void decodeOpcodebc(void);                    
    static void decodeOpcodebd(void);                    
    static void decodeOpcodebe(void);                    

    static void decodeOpcodebf(void); // Used for LOAD TRAP

    static void decodeOpcodec0(void);
    static void decodeOpcodec1(void);
    static void decodeOpcodec2(void);
    static void decodeOpcodec3(void);
    static void decodeOpcodec4(void);
    static void decodeOpcodec5(void);
    static void decodeOpcodec6(void);                        
    static void decodeOpcodec7(void);
    static void decodeOpcodec8(void);
    static void decodeOpcodec9(void);
    static void decodeOpcodeca(void);
    static void decodeOpcodecb(void);                    
    static void decodeOpcodecc(void);                    
    static void decodeOpcodecd(void);                    
    static void decodeOpcodece(void);                    
    static void decodeOpcodecf(void);                    

    static void decodeOpcoded0(void);
    static void decodeOpcoded1(void);
    static void decodeOpcoded2(void);
    static void decodeOpcoded3(void);
    static void decodeOpcoded4(void);
    static void decodeOpcoded5(void);
    static void decodeOpcoded6(void);                        
    static void decodeOpcoded7(void);
    static void decodeOpcoded8(void);
    static void decodeOpcoded9(void);
    static void decodeOpcodeda(void);
    static void decodeOpcodedb(void);                    
    static void decodeOpcodedc(void);                    
    static void decodeOpcodedd(void);                    
    static void decodeOpcodede(void);                    
    static void decodeOpcodedf(void);                    

    static void decodeOpcodee0(void);
    static void decodeOpcodee1(void);
    static void decodeOpcodee2(void);
    static void decodeOpcodee3(void);
    static void decodeOpcodee4(void);
    static void decodeOpcodee5(void);
    static void decodeOpcodee6(void);                        
    static void decodeOpcodee7(void);
    static void decodeOpcodee8(void);
    static void decodeOpcodee9(void);
    static void decodeOpcodeea(void);
    static void decodeOpcodeeb(void);                    
    static void decodeOpcodeec(void);                    
    static void decodeOpcodeed(void);                    
    static void decodeOpcodeee(void);                    
    static void decodeOpcodeef(void);                    

    static void decodeOpcodef0(void);
    static void decodeOpcodef1(void);
    static void decodeOpcodef2(void);
    static void decodeOpcodef3(void);
    static void decodeOpcodef4(void);
    static void decodeOpcodef5(void);
    static void decodeOpcodef6(void);                        
    static void decodeOpcodef7(void);
    static void decodeOpcodef8(void);
    static void decodeOpcodef9(void);
    static void decodeOpcodefa(void);
    static void decodeOpcodefb(void);                    
    static void decodeOpcodefc(void);                    
    static void decodeOpcodefd(void);                    
    static void decodeOpcodefe(void);                    
    static void decodeOpcodeff(void);                    

    static void dcCB00(void);
    static void dcCB01(void);
    static void dcCB02(void);
    static void dcCB03(void);
    static void dcCB04(void);
    static void dcCB05(void);
    static void dcCB06(void);                        
    static void dcCB07(void);
    static void dcCB08(void);
    static void dcCB09(void);
    static void dcCB0A(void);
    static void dcCB0B(void);                    
    static void dcCB0C(void);                    
    static void dcCB0D(void);                    
    static void dcCB0E(void);                    
    static void dcCB0F(void);                    

    static void dcCB10(void);
    static void dcCB11(void);
    static void dcCB12(void);
    static void dcCB13(void);
    static void dcCB14(void);
    static void dcCB15(void);
    static void dcCB16(void);                        
    static void dcCB17(void);
    static void dcCB18(void);
    static void dcCB19(void);
    static void dcCB1A(void);
    static void dcCB1B(void);                    
    static void dcCB1C(void);                    
    static void dcCB1D(void);                    
    static void dcCB1E(void);                    
    static void dcCB1F(void);                    

    static void dcCB20(void);
    static void dcCB21(void);
    static void dcCB22(void);
    static void dcCB23(void);
    static void dcCB24(void);
    static void dcCB25(void);
    static void dcCB26(void);                        
    static void dcCB27(void);
    static void dcCB28(void);
    static void dcCB29(void);
    static void dcCB2A(void);
    static void dcCB2B(void);                    
    static void dcCB2C(void);                    
    static void dcCB2D(void);                    
    static void dcCB2E(void);                    
    static void dcCB2F(void);                    

    static void dcCB30(void);
    static void dcCB31(void);
    static void dcCB32(void);
    static void dcCB33(void);
    static void dcCB34(void);
    static void dcCB35(void);
    static void dcCB36(void);                        
    static void dcCB37(void);
    static void dcCB38(void);
    static void dcCB39(void);
    static void dcCB3A(void);
    static void dcCB3B(void);                    
    static void dcCB3C(void);                    
    static void dcCB3D(void);                    
    static void dcCB3E(void);                    
    static void dcCB3F(void);                    

    static void dcCB40(void);
    static void dcCB41(void);
    static void dcCB42(void);
    static void dcCB43(void);
    static void dcCB44(void);
    static void dcCB45(void);
    static void dcCB46(void);                        
    static void dcCB47(void);
    static void dcCB48(void);
    static void dcCB49(void);
    static void dcCB4A(void);
    static void dcCB4B(void);                    
    static void dcCB4C(void);                    
    static void dcCB4D(void);                    
    static void dcCB4E(void);                    
    static void dcCB4F(void);                    

    static void dcCB50(void);
    static void dcCB51(void);
    static void dcCB52(void);
    static void dcCB53(void);
    static void dcCB54(void);
    static void dcCB55(void);
    static void dcCB56(void);                        
    static void dcCB57(void);
    static void dcCB58(void);
    static void dcCB59(void);
    static void dcCB5A(void);
    static void dcCB5B(void);                    
    static void dcCB5C(void);                    
    static void dcCB5D(void);                    
    static void dcCB5E(void);                    
    static void dcCB5F(void);                    

    static void dcCB60(void);
    static void dcCB61(void);
    static void dcCB62(void);
    static void dcCB63(void);
    static void dcCB64(void);
    static void dcCB65(void);
    static void dcCB66(void);                        
    static void dcCB67(void);
    static void dcCB68(void);
    static void dcCB69(void);
    static void dcCB6A(void);
    static void dcCB6B(void);                    
    static void dcCB6C(void);                    
    static void dcCB6D(void);                    
    static void dcCB6E(void);                    
    static void dcCB6F(void);                    

    static void dcCB70(void);
    static void dcCB71(void);
    static void dcCB72(void);
    static void dcCB73(void);
    static void dcCB74(void);
    static void dcCB75(void);
    static void dcCB76(void);                        
    static void dcCB77(void);
    static void dcCB78(void);
    static void dcCB79(void);
    static void dcCB7A(void);
    static void dcCB7B(void);                    
    static void dcCB7C(void);                    
    static void dcCB7D(void);                    
    static void dcCB7E(void);                    
    static void dcCB7F(void);                    

    static void dcCB80(void);
    static void dcCB81(void);
    static void dcCB82(void);
    static void dcCB83(void);
    static void dcCB84(void);
    static void dcCB85(void);
    static void dcCB86(void);                        
    static void dcCB87(void);
    static void dcCB88(void);
    static void dcCB89(void);
    static void dcCB8A(void);
    static void dcCB8B(void);                    
    static void dcCB8C(void);                    
    static void dcCB8D(void);                    
    static void dcCB8E(void);                    
    static void dcCB8F(void);                    

    static void dcCB90(void);
    static void dcCB91(void);
    static void dcCB92(void);
    static void dcCB93(void);
    static void dcCB94(void);
    static void dcCB95(void);
    static void dcCB96(void);                        
    static void dcCB97(void);
    static void dcCB98(void);
    static void dcCB99(void);
    static void dcCB9A(void);
    static void dcCB9B(void);                    
    static void dcCB9C(void);                    
    static void dcCB9D(void);                    
    static void dcCB9E(void);                    
    static void dcCB9F(void);                    

    static void dcCBA0(void);
    static void dcCBA1(void);
    static void dcCBA2(void);
    static void dcCBA3(void);
    static void dcCBA4(void);
    static void dcCBA5(void);
    static void dcCBA6(void);                        
    static void dcCBA7(void);
    static void dcCBA8(void);
    static void dcCBA9(void);
    static void dcCBAA(void);
    static void dcCBAB(void);                    
    static void dcCBAC(void);                    
    static void dcCBAD(void);                    
    static void dcCBAE(void);                    
    static void dcCBAF(void);                    

    static void dcCBB0(void);
    static void dcCBB1(void);
    static void dcCBB2(void);
    static void dcCBB3(void);
    static void dcCBB4(void);
    static void dcCBB5(void);
    static void dcCBB6(void);                        
    static void dcCBB7(void);
    static void dcCBB8(void);
    static void dcCBB9(void);
    static void dcCBBA(void);
    static void dcCBBB(void);                    
    static void dcCBBC(void);                    
    static void dcCBBD(void);                    
    static void dcCBBE(void);                    
    static void dcCBBF(void);                    

    static void dcCBC0(void);
    static void dcCBC1(void);
    static void dcCBC2(void);
    static void dcCBC3(void);
    static void dcCBC4(void);
    static void dcCBC5(void);
    static void dcCBC6(void);                        
    static void dcCBC7(void);
    static void dcCBC8(void);
    static void dcCBC9(void);
    static void dcCBCA(void);
    static void dcCBCB(void);                    
    static void dcCBCC(void);                    
    static void dcCBCD(void);                    
    static void dcCBCE(void);                    
    static void dcCBCF(void);                    

    static void dcCBD0(void);
    static void dcCBD1(void);
    static void dcCBD2(void);
    static void dcCBD3(void);
    static void dcCBD4(void);
    static void dcCBD5(void);
    static void dcCBD6(void);                        
    static void dcCBD7(void);
    static void dcCBD8(void);
    static void dcCBD9(void);
    static void dcCBDA(void);
    static void dcCBDB(void);                    
    static void dcCBDC(void);                    
    static void dcCBDD(void);                    
    static void dcCBDE(void);                    
    static void dcCBDF(void);                    

    static void dcCBE0(void);
    static void dcCBE1(void);
    static void dcCBE2(void);
    static void dcCBE3(void);
    static void dcCBE4(void);
    static void dcCBE5(void);
    static void dcCBE6(void);                        
    static void dcCBE7(void);
    static void dcCBE8(void);
    static void dcCBE9(void);
    static void dcCBEA(void);
    static void dcCBEB(void);                    
    static void dcCBEC(void);                    
    static void dcCBED(void);                    
    static void dcCBEE(void);                    
    static void dcCBEF(void);                    

    static void dcCBF0(void);
    static void dcCBF1(void);
    static void dcCBF2(void);
    static void dcCBF3(void);
    static void dcCBF4(void);
    static void dcCBF5(void);
    static void dcCBF6(void);                        
    static void dcCBF7(void);
    static void dcCBF8(void);
    static void dcCBF9(void);
    static void dcCBFA(void);
    static void dcCBFB(void);                    
    static void dcCBFC(void);                    
    static void dcCBFD(void);                    
    static void dcCBFE(void);                    
    static void dcCBFF(void);                    

    static void check_trdos();                 
    static void check_trdos_unpage();                 
};

#endif // Z80CPP_H
