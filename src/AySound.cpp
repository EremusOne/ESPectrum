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

#include "AySound.h"
#include "hardconfig.h"
#include "ESPectrum.h"

#pragma GCC optimize ("O3")

/* emulator settings */
int AySound::table[32];                 /**< table of volumes for chip */
ayemu_chip_t AySound::type;             /**< general chip type (\b AYEMU_AY or \b AYEMU_YM) */
int AySound::ChipFreq;                  /**< chip emulator frequency */
int AySound::eq[6];                     /**< volumes for channels.
                                        Array contains 6 elements: 
                                        A left, A right, B left, B right, C left and C right;
                                        range -100...100 */
ayemu_regdata_t AySound::ayregs;        /**< parsed registers data */
ayemu_sndfmt_t AySound::sndfmt;         /**< output sound format */

/* flags */
int AySound::default_chip_flag;         /**< =1 after init, resets in #ayemu_set_chip_type() */
int AySound::default_stereo_flag;       /**< =1 after init, resets in #ayemu_set_stereo() */
int AySound::default_sound_format_flag; /**< =1 after init, resets in #ayemu_set_sound_format() */
int AySound::dirty;                     /**< dirty flag. Sets if any emulator properties changed */

int AySound::bit_a;                     /**< state of channel A generator */
int AySound::bit_b;                     /**< state of channel B generator */
int AySound::bit_c;                     /**< state of channel C generator */
int AySound::bit_n;                     /**< current generator state */
int AySound::period_n;                  // Noise period 
int AySound::cnt_a;                     /**< back counter of A */
int AySound::cnt_b;                     /**< back counter of B */
int AySound::cnt_c;                     /**< back counter of C */
int AySound::cnt_n;                     /**< back counter of noise generator */
int AySound::cnt_e;                     /**< back counter of envelop generator */
int AySound::ChipTacts_per_outcount;    /**< chip's counts per one sound signal count */
int AySound::Amp_Global;                /**< scale factor for amplitude */
int AySound::vols[6][32];               /**< stereo type (channel volumes) and chip table.
                                        This cache calculated by #table and #eq    */
int AySound::EnvNum;                    /**< number of current envilopment (0...15) */
int AySound::env_pos;                   /**< current position in envelop (0...127) */
int AySound::Cur_Seed;                  /**< random numbers counter */

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
uint8_t AySound::regs[15]; /* = { 0xFF }; */

void (*AySound::updateReg[15])();

// Status
uint8_t AySound::selectedRegister; /* = 0xFF; */

/* Max amplitude value for stereo signal for avoiding for possible
        following SSRC for clipping */
// #define AYEMU_MAX_AMP 24575

#define AYEMU_MAX_AMP 40000; // 158
#define AYEMU_DEFAULT_CHIP_FREQ 1773400

/* sound chip volume envelops (will calculated by gen_env()) */
static int bEnvGenInit = 0;
static int Envelope [16][128];

/* AY volume table (c) by V_Soft and Lion 17 */
static int Lion17_AY_table [16] = {
    0, 513, 828, 1239, 1923, 3238, 4926, 9110,
    10344, 17876, 24682, 30442, 38844, 47270, 56402, 65535
};

/* AY volume table (C) Rampa 2023 */
static int Rampa_AY_table [16] = {
    0, 521, 735, 1039, 1467, 2072, 2927, 4135,
    5841, 8250, 11654, 16462, 23253, 32845, 46395, 65535
};

// Borrowed from SoftSpectrum48 source code:
// Values according to: http://forum.tslabs.info/viewtopic.php?f=6&t=539
// AmplitudeFactors = { 0.0f, 0.01f, 0.014f, 0.021f, 0.031f, 0.046f, 0.064f, 
// 0.107f, 0.127f, 0.205f, 0.292f, 0.373f, 0.493f, 0.635f, 0.806f, 1.0f }
static int SoftSpec48_AY_table [16] = {
    0, 655, 917, 1376, 2032, 3015, 4194, 7012,
    8323, 13435, 19136, 24445, 32309, 41615, 52821, 65535
};

/* make chip hardware envelop tables.
        Will execute once before first use. */
static void gen_env()
{
    int env;
    int pos;
    int hold;
    int dir;
    int vol;

    for (env = 0; env < 16; env++) {
        hold = 0;
        dir = (env & 4)?    1 : -1;
        vol = (env & 4)? -1 : 32;
        for (pos = 0; pos < 128; pos++) {
            if (!hold) {
                vol += dir;
                if (vol < 0 || vol >= 32) {
                    if ( env & 8 ) {
                        if ( env & 2 ) dir = -dir;
                        vol = (dir > 0 ) ? 0: 31;
                        if ( env & 1 ) {
                            hold = 1;
                            vol = ( dir > 0 ) ? 31 : 0;
                        }
                    } else {
                        vol = 0;
                        hold = 1;
                    }
                }
            }
            Envelope[env][pos] = vol;
        }
    }

    bEnvGenInit = 1;

}

void AySound::init()
{

    default_chip_flag = 1;
    ChipFreq = AYEMU_DEFAULT_CHIP_FREQ;
    default_stereo_flag = 1;
    default_sound_format_flag = 1;
    dirty = 1;
    
    updateReg[0] = &updToneA;
    updateReg[1] = &updToneA;
    updateReg[2] = &updToneB;
    updateReg[3] = &updToneB;
    updateReg[4] = &updToneC;
    updateReg[5] = &updToneC;
    updateReg[6] = &updNoisePitch;
    updateReg[7] = &updMixer;
    updateReg[8] = &updVolA;
    updateReg[9] = &updVolB;
    updateReg[10] = &updVolC;
    updateReg[11] = &updEnvFreq;
    updateReg[12] = &updEnvFreq;
    updateReg[13] = &updEnvType;
    updateReg[14] = &update;

    ayreset();

}

void AySound::ayreset()
{
    
    cnt_a = cnt_b = cnt_c = cnt_n = cnt_e = 0;
    bit_a = bit_b = bit_c = bit_n = 0;
    env_pos = EnvNum = 0;

    // Cur_Seed = 0xffff;

    // Cur_Seed = 0x1ffff;

    period_n = 1;
    Cur_Seed = 1;

    prepare_generation();

}

/** Set chip type. */
int AySound::set_chip_type(ayemu_chip_t type, int *custom_table)
{

    // if (!(type == AYEMU_AY_CUSTOM || type == AYEMU_YM_CUSTOM) && custom_table != NULL) {
    //     // ayemu_err = "For non-custom chip type 'custom_table' param must be NULL";
    //     return 0;
    // }

    // switch(type) {
    // case AYEMU_AY:
    // case AYEMU_AY_LION17:
    int n;
    for (n = 0; n < 32; n++)
        table[n] = Rampa_AY_table[n / 2];
    type = AYEMU_AY;
    // break;
    // case AYEMU_YM:
    // case AYEMU_YM_LION17:
    //     set_table_ym(ay, Lion17_YM_table);
    //     break;
    // case AYEMU_AY_KAY:
    //     set_table_ay(ay, KAY_AY_table);
    //     break;
    // case AYEMU_YM_KAY:
    //     set_table_ym(ay, KAY_YM_table);
    //     break;
    // case AYEMU_AY_CUSTOM:
    //     set_table_ay(ay, custom_table);
    //     break;
    // case AYEMU_YM_CUSTOM:
    //     set_table_ym(ay, custom_table);
    //     break;
    // default:
        // ayemu_err = "Incorrect chip type";
        // return 0;
    // }

    default_chip_flag = 0;
    dirty = 1;
    return 1;

}

/** Set chip frequency. */
void AySound::set_chip_freq(int chipfreq)
{

    ChipFreq = chipfreq;
    dirty = 1;

}

/*! Set output sound format
 * \arg \c ay - pointer to ayemu_t structure
 * \arg \c freq - sound freq (44100 for example)
 * \arg \c chans - number of channels (1-mono, 2-stereo)
 * \arg \c bits - now supported only 16 and 8.
 * \retval \b 1 on success, \b 0 if error occure
 */
int AySound::set_sound_format(int freq, int chans, int bits)
{
//     if (!check_magic(ay))
//         return 0;

    // if (!(bits == 16 || bits == 8)) {
    //     // ayemu_err = "Incorrect bits value";
    //     return 0;
    // }
    // else if (!(chans == 1 || chans == 2)) {
    //     // ayemu_err = "Incorrect number of channels";
    //     return 0;
    // }
    // else if (freq < 50) {
    //     // ayemu_err = "Incorrect output sound freq";
    //     return 0;
    // }
    // else {
        sndfmt.freq = freq;
        sndfmt.channels = chans;
        sndfmt.bpc = bits;
    // }

    default_sound_format_flag = 0;
    dirty = 1;
    return 1;
}

/*! Set amplitude factor for each of channels (A,B anc C, tone and noise).
 * Factor's value must be from (-100) to 100.
 * \arg ay - pointer to ayemu_t structure
 * \arg stereo_type - type of stereo
 * \arg custom_eq - NULL or pointer to custom table of mixer layout.
 * \retval 1 if OK, 0 if error occures.
 */
int AySound::set_stereo(ayemu_stereo_t stereo_type, int *custom_eq)
{
    int i;
    int chip;

    // if (stereo_type != AYEMU_STEREO_CUSTOM && custom_eq != NULL) {
    //     // ayemu_err = "Stereo type not custom, 'custom_eq' parametr must be NULL";
    //     return 0;
    // }

    // chip = (type == AYEMU_AY)? 0 : 1;

    // switch(stereo_type) {
    // case AYEMU_MONO:
    // case AYEMU_ABC:
    // case AYEMU_ACB:
    // case AYEMU_BAC:
    // case AYEMU_BCA:
    // case AYEMU_CAB:
    // case AYEMU_CBA:
        for (i = 0 ; i < 6 ; i++)
            // ay->eq[i] = default_layout[chip][stereo_type][i];
            eq[i] = 100;
        // break;
    // case AYEMU_STEREO_CUSTOM:
    //     for (i = 0 ; i < 6 ; i++)
    //         eq[i] = custom_eq[i];
    //     break;
    // default:
    //     // ayemu_err = "Incorrect stereo type";
    //     return 0;
    // }

    default_stereo_flag = 0;
    dirty = 1;
    return 1;
}

// #define WARN_IF_REGISTER_GREAT_THAN(r,m) \
// if (*(sregs + r) > m) \
//        printf("ayemu_set_regs: warning: possible bad register data- R%d > %d\n", r, m)

// // Assign values for AY registers.
// // You must pass array of char [14] to this function
// void AySound::set_regs(ayemu_ay_reg_frame_t sregs)
// {

//     if (verbose) {
//         WARN_IF_REGISTER_GREAT_THAN(1,15);
//         WARN_IF_REGISTER_GREAT_THAN(3,15);
//         WARN_IF_REGISTER_GREAT_THAN(5,15);
//         WARN_IF_REGISTER_GREAT_THAN(8,31);
//         WARN_IF_REGISTER_GREAT_THAN(9,31);
//         WARN_IF_REGISTER_GREAT_THAN(10,31);
//     }

//     ayregs.tone_a    = sregs[0] + ((sregs[1]&0x0f) << 8);
//     ayregs.tone_b    = sregs[2] + ((sregs[3]&0x0f) << 8);
//     ayregs.tone_c    = sregs[4] + ((sregs[5]&0x0f) << 8);

//     ayregs.noise = sregs[6] & 0x1f;

//     ayregs.R7_tone_a    = ! (sregs[7] & 0x01);
//     ayregs.R7_tone_b    = ! (sregs[7] & 0x02);
//     ayregs.R7_tone_c    = ! (sregs[7] & 0x04);

//     ayregs.R7_noise_a = ! (sregs[7] & 0x08);
//     ayregs.R7_noise_b = ! (sregs[7] & 0x10);
//     ayregs.R7_noise_c = ! (sregs[7] & 0x20);

//     ayregs.vol_a = sregs[8]    & 0x0f;
//     ayregs.vol_b = sregs[9]    & 0x0f;
//     ayregs.vol_c = sregs[10] & 0x0f;
//     ayregs.env_a = sregs[8]    & 0x10;
//     ayregs.env_b = sregs[9]    & 0x10;
//     ayregs.env_c = sregs[10] & 0x10;
//     ayregs.env_freq = sregs[11] + (sregs[12] << 8);

//     if (sregs[13] != 0xff) { // R13 = 255 means continue current envelop
//         int new_style = sregs[13] & 0x0f;
//         if (ayregs.env_style != new_style) {
//             ayregs.env_style = new_style;
//             env_pos = cnt_e = 0;
//         }
//     }

// }

void AySound::prepare_generation()
{
    int vol, max_l, max_r;

    if (!dirty) return;

    if (!bEnvGenInit) gen_env();

    if (default_chip_flag) set_chip_type(AYEMU_AY, NULL);

    if (default_stereo_flag) set_stereo(AYEMU_ABC, NULL);

    if (default_sound_format_flag) set_sound_format(44100, 2, 16);

    ChipTacts_per_outcount = ChipFreq / sndfmt.freq / 8;

    {    // GenVols
        int n, m;
        int vol;
        for (n = 0; n < 32; n++) {
            vol = table[n];
            for (m=0; m < 6; m++)
                vols[m][n] = (int) (((double) vol * eq[m]) / 100);
        }
    }

    max_l = vols[0][31] + vols[2][31] + vols[4][31];
    max_r = vols[1][31] + vols[3][31] + vols[5][31];
    vol = (max_l > max_r) ? max_l : max_r;    // =157283 on all defaults
    Amp_Global = ChipTacts_per_outcount * vol / AYEMU_MAX_AMP;

    dirty = 0;
}

//
// Generate sound.
// Fill sound buffer with current register data
//
void AySound::gen_sound(unsigned char *buff, size_t sound_bufsize, int bufpos)
{
    int mix_l, mix_r;
    int tmpvol;
    int m;
    int snd_numcount;
    unsigned char *sound_buf = buff;

    sound_buf += bufpos;

    snd_numcount = sound_bufsize / (sndfmt.channels * (sndfmt.bpc >> 3));
    while (snd_numcount-- > 0) {
        mix_l = mix_r = 0;
        
        for (m = 0 ; m < ChipTacts_per_outcount ; m++) {
            if (++cnt_a >= ayregs.tone_a) {
                cnt_a = 0;
                bit_a = !bit_a;
            }
            if (++cnt_b >= ayregs.tone_b) {
                cnt_b = 0;
                bit_b = !bit_b;
            }
            if (++cnt_c >= ayregs.tone_c) {
                cnt_c = 0;
                bit_c = !bit_c;
            }

            // /* GenNoise (c) Hacker KAY & Sergey Bulba */
            // if (++cnt_n >= (ayregs.noise * 2)) {

            //     cnt_n = 0;

            //     Cur_Seed = (Cur_Seed * 2 + 1) ^ \
            //         (((Cur_Seed >> 16) ^ (Cur_Seed >> 13)) & 1); 
            //     bit_n = ((Cur_Seed >> 16) & 1);

            // }

            // if (++cnt_n >= (ayregs.noise * 2)) {

            //     cnt_n = 0;

            //     // The algorithm is explained in the Fuse source:
            //     // The Random Number Generator of the 8910 is a 17-bit shift
            //     // register. The input to the shift register is bit0 XOR bit3
            //     // (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips.
            //     // The following is a fast way to compute bit17 = bit0^bit3
            //     // Instead of doing all the logic operations, we only check
            //     // bit0, relying on the fact that after three shifts of the
            //     // register, what now is bit3 will become bit0, and will
            //     // invert, if necessary, bit14, which previously was bit17
            //     if ((Cur_Seed & 1) == 1)
            //     {
            //         Cur_Seed ^= 0x24000;
            //     }
            //     Cur_Seed >>= 1;
            //     bit_n = Cur_Seed & 1;

            // }
          
            // Code borrowed from JSpeccy sources
            if (++cnt_n >= period_n) {

                cnt_n = 0;
                 // Changes to R6 take effect only when internal counter reaches 0
                period_n = ayregs.noise;
                if (period_n == 0) {
                    period_n = 1;
                }
                period_n <<= 1;

                // Code borrowed from MAME sources
                /* Is noise output going to change? */
                if (((Cur_Seed + 1) & 0x02) != 0) { /* (bit0^bit1)? */
                    bit_n = !bit_n;
                }

                /* The Random Number Generator of the 8910 is a 17-bit shift */
                /* register. The input to the shift register is bit0 XOR bit3 */
                /* (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips. */

                /* The following is a fast way to compute bit17 = bit0^bit3. */
                /* Instead of doing all the logic operations, we only check */
                /* bit0, relying on the fact that after three shifts of the */
                /* register, what now is bit3 will become bit0, and will */
                /* invert, if necessary, bit14, which previously was bit17. */
                if ((Cur_Seed & 0x01) != 0) {
                    Cur_Seed ^= 0x24000; /* This version is called the "Galois configuration". */
                }
                Cur_Seed >>= 1;
                // End of Code borrowed from MAME sources
            }
            // End of Code borrowed from JSpeccy sources

            if (++cnt_e >= ayregs.env_freq) {
                cnt_e = 0;
                if (++env_pos > 127)
                    env_pos = 64;
            }

            #define ENVVOL Envelope[ayregs.env_style][env_pos]

            if ((bit_a | !ayregs.R7_tone_a) & (bit_n | !ayregs.R7_noise_a)) {
                tmpvol = (ayregs.env_a) ? ENVVOL : ayregs.vol_a * 2 + 1;
                mix_l += vols[0][tmpvol];
                mix_r += vols[1][tmpvol];
            }
            
            if ((bit_b | !ayregs.R7_tone_b) & (bit_n | !ayregs.R7_noise_b)) {
                tmpvol = (ayregs.env_b) ? ENVVOL :  ayregs.vol_b * 2 + 1;
                mix_l += vols[2][tmpvol];
                mix_r += vols[3][tmpvol];
            }
            
            if ((bit_c | !ayregs.R7_tone_c) & (bit_n | !ayregs.R7_noise_c)) {
                tmpvol = (ayregs.env_c) ? ENVVOL : ayregs.vol_c * 2 + 1;
                mix_l += vols[4][tmpvol];
                mix_r += vols[5][tmpvol];
            }            
        } /* end for (m=0; ...) */
        
        mix_l /= Amp_Global;
        // mix_r /= Amp_Global;
        
        // if (ay->sndfmt.bpc == 8) {
            mix_l = (mix_l >> 8) /*| 128*/; /* 8 bit sound */
            // mix_r = (mix_r >> 8) /*| 128*/;
            *sound_buf++ = mix_l;
            // if (ay->sndfmt.channels != 1)
            //     *sound_buf++ = mix_r;
        // } else {
        //     *sound_buf++ = mix_l & 0x00FF; /* 16 bit sound */
        //     *sound_buf++ = (mix_l >> 8);
        //     if (ay->sndfmt.channels != 1) {
        //         *sound_buf++ = mix_r & 0x00FF;
        //         *sound_buf++ = (mix_r >> 8);
        //     }
        // }
    }

}

void AySound::updToneA() {
    ayregs.tone_a = regs[0] + ((regs[1] & 0x0f) << 8);
}

void AySound::updToneB() {
    ayregs.tone_b = regs[2] + ((regs[3] & 0x0f) << 8);
}

void AySound::updToneC() {
    ayregs.tone_c = regs[4] + ((regs[5] & 0x0f) << 8);
}

void AySound::updNoisePitch() {
    ayregs.noise = regs[6] & 0x1f;
}

void AySound::updMixer() {
    ayregs.R7_tone_a    = !(regs[7] & 0x01);
    ayregs.R7_tone_b    = !(regs[7] & 0x02);
    ayregs.R7_tone_c    = !(regs[7] & 0x04);

    ayregs.R7_noise_a = !(regs[7] & 0x08);
    ayregs.R7_noise_b = !(regs[7] & 0x10);
    ayregs.R7_noise_c = !(regs[7] & 0x20);
}

void AySound::updVolA() {
    ayregs.vol_a = regs[8]    & 0x0f;
    ayregs.env_a = regs[8]    & 0x10;
}

void AySound::updVolB() {
    ayregs.vol_b = regs[9]    & 0x0f;
    ayregs.env_b = regs[9]    & 0x10;
}

void AySound::updVolC() {
    ayregs.vol_c = regs[10] & 0x0f;
    ayregs.env_c = regs[10] & 0x10;
}

void AySound::updEnvFreq() {
    ayregs.env_freq = regs[11] + (regs[12] << 8);
}

void AySound::updEnvType() {
    if (regs[13] != 0xff) { // R13 = 255 means continue current envelop

        // int new_style = regs[13] & 0x0f;
        // if (ayregs.env_style != new_style) {
            // ayregs.env_style = new_style;
            // env_pos = cnt_e = 0;
        // }

        ayregs.env_style = regs[13] & 0x0f;
        env_pos = cnt_e = 0;

    }
}

void AySound::update() {
    return;
}

uint8_t AySound::getRegisterData()
{

    if (selectedRegister < 15) return regs[selectedRegister];

    return 0;

}

void AySound::selectRegister(uint8_t registerNumber)
{
    selectedRegister = registerNumber;
}

void AySound::setRegisterData(uint8_t data)
{
    
    if (selectedRegister < 15) {
        regs[selectedRegister] = data;    
        updateReg[selectedRegister]();
    }

}

void AySound::reset()
{

    ayreset();

    // for (int i=0;i<15;i++) regs[i] = 0xFF;
    for (int i=0;i<15;i++) regs[i] = 0;
   
    // // Vol = 0
    // regs[8]=0;
    // regs[9]=0;
    // regs[10]=0;

    selectedRegister = 0xFF;

    for(int i=0; i<15; i++) updateReg[i]();

}
