///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
//
// AY SOUND EMULATION
//
// Based on libayemu by:
// Sashnov Alexander <sashnov@ngs.ru> and Roman Scherbakov <v_soft@nm.ru> 
//
// Copyright (c) 2023 Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
// https://github.com/EremusOne/ZX-ESPectrum-IDF
//
// Based on ZX-ESPectrum-Wiimote
// Copyright (c) 2020, 2022 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez and Jorge Fuertes
// https://github.com/rampa069/ZX-ESPectrum
//
// Original project by Pete Todd
// https://github.com/retrogubbins/paseVGA
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#ifndef AySound_h
#define AySound_h

#include "hardconfig.h"
#include "ESPectrum.h"
#include <inttypes.h>
#include <stddef.h>

// typedef unsigned char ayemu_ay_reg_frame_t[14];

/** Types of stereo.
    The codes of stereo types used for generage sound. */
typedef enum
{
    AYEMU_MONO = 0,
    AYEMU_ABC,
    AYEMU_ACB,
    AYEMU_BAC,
    AYEMU_BCA,
    AYEMU_CAB,
    AYEMU_CBA,
    AYEMU_STEREO_CUSTOM = 255
} ayemu_stereo_t;

/** Sound chip type.
    Constant for identify used chip for emulation */
typedef enum {
    AYEMU_AY,            /**< default AY chip (lion17 for now) */
    AYEMU_YM,            /**< default YM chip (lion17 for now) */
    AYEMU_AY_LION17,     /**< emulate AY with Lion17 table */
    AYEMU_YM_LION17,     /**< emulate YM with Lion17 table */
    AYEMU_AY_KAY,        /**< emulate AY with HACKER KAY table */
    AYEMU_YM_KAY,        /**< emulate YM with HACKER KAY table */
    AYEMU_AY_LOG,        /**< emulate AY with logariphmic table */
    AYEMU_YM_LOG,        /**< emulate YM with logariphmic table */
    AYEMU_AY_CUSTOM,     /**< use AY with custom table. */
    AYEMU_YM_CUSTOM      /**< use YM with custom table. */
} ayemu_chip_t;

// Registers
// regs[0] = finePitchChannelA;
// regs[1] = coarsePitchChannelA;
// regs[2] = finePitchChannelB;
// regs[3] = coarsePitchChannelB;
// regs[4] = finePitchChannelC;
// regs[5] = coarsePitchChannelC;
// regs[6] = noisePitch;
// regs[7] = mixer;
// regs[8] = volumeChannelA;
// regs[9] = volumeChannelB;
// regs[10] = volumeChannelC;
// regs[11] = envelopeFineDuration;
// regs[12] = envelopeCoarseDuration;
// regs[13] = envelopeShape;
// regs[14] = ioPortA;
// regs[15] = ioPortB;

/** parsed by #ayemu_set_regs() AY registers data \internal */
typedef struct
{
    int tone_a;       /**< R0, R1 */
    int tone_b;       /**< R2, R3 */    
    int tone_c;       /**< R4, R5 */
    int noise;        /**< R6 */
    int R7_tone_a;    /**< R7 bit 0 */
    int R7_tone_b;    /**< R7 bit 1 */
    int R7_tone_c;    /**< R7 bit 2 */
    int R7_noise_a;   /**< R7 bit 3 */
    int R7_noise_b;   /**< R7 bit 4 */
    int R7_noise_c;   /**< R7 bit 5 */
    int vol_a;        /**< R8 bits 3-0 */
    int vol_b;        /**< R9 bits 3-0 */
    int vol_c;        /**< R10 bits 3-0 */
    int env_a;        /**< R8 bit 4 */
    int env_b;        /**< R9 bit 4 */
    int env_c;        /**< R10 bit 4 */
    int env_freq;     /**< R11, R12 */
    int env_style;    /**< R13 */
}
ayemu_regdata_t;

/** Output sound format \internal */
typedef struct
{
  int freq;           /**< sound freq */
  int channels;       /**< channels (1-mono, 2-stereo) */
  int bpc;            /**< bits (8 or 16) */
}
ayemu_sndfmt_t;

class AySound
{
public:

    static void updToneA();
    static void updToneB();
    static void updToneC();
    static void updNoisePitch();
    static void updMixer();
    static void updVolA();
    static void updVolB();
    static void updVolC();
    static void updEnvFreq();
    static void updEnvType();

    static void reset();
    static uint8_t getRegisterData();
    static void selectRegister(uint8_t data);
    static void setRegisterData(uint8_t data);

    static void init();
    static int set_chip_type(ayemu_chip_t chip, int *custom_table);
    static void set_chip_freq(int chipfreq);
    static int set_stereo(ayemu_stereo_t stereo, int *custom_eq);
    static int set_sound_format(int freq, int chans, int bits);
    static void prepare_generation();
    static void gen_sound(unsigned char *buff, size_t bufsize, int bufpos);

    static void(*updateReg[14])();

private:

    /* emulator settings */
    static int table[32];                   /**< table of volumes for chip */
    static ayemu_chip_t type;               /**< general chip type (\b AYEMU_AY or \b AYEMU_YM) */
    static int ChipFreq;                    /**< chip emulator frequency */
    static int eq[6];                       /**< volumes for channels.
                                            Array contains 6 elements: 
                                            A left, A right, B left, B right, C left and C right;
                                            range -100...100 */
    static ayemu_regdata_t ayregs;          /**< parsed registers data */
    static ayemu_sndfmt_t sndfmt;           /**< output sound format */

    // flags
    static int default_chip_flag;           /**< =1 after init, resets in #ayemu_set_chip_type() */
    static int default_stereo_flag;         /**< =1 after init, resets in #ayemu_set_stereo() */
    static int default_sound_format_flag;   /**< =1 after init, resets in #ayemu_set_sound_format() */
    static int dirty;                       /**< dirty flag. Sets if any emulator properties changed */

    static int bit_a;                       /**< state of channel A generator */
    static int bit_b;                       /**< state of channel B generator */
    static int bit_c;                       /**< state of channel C generator */
    static int bit_n;                       /**< current generator state */
    static int period_n;                    // Noise period 
    static int cnt_a;                       /**< back counter of A */
    static int cnt_b;                       /**< back counter of B */
    static int cnt_c;                       /**< back counter of C */
    static int cnt_n;                       /**< back counter of noise generator */
    static int cnt_e;                       /**< back counter of envelop generator */
    static int ChipTacts_per_outcount;      /**< chip's counts per one sound signal count */
    static int Amp_Global;                  /**< scale factor for amplitude */
    static int vols[6][32];                 /**< stereo type (channel volumes) and chip table.
                                            This cache calculated by #table and #eq  */
    static int EnvNum;                      /**< number of current envilopment (0...15) */
    static int env_pos;                     /**< current position in envelop (0...127) */
    static int Cur_Seed;                    /**< random numbers counter */

    static uint8_t regs[14];
    static uint8_t selectedRegister;

};

#endif // AySound_h
