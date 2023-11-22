/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

AY SOUND EMULATION, based on libayemu by:
Sashnov Alexander <sashnov@ngs.ru> and Roman Scherbakov <v_soft@nm.ru> 

Copyright (c) 2023 Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
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

#include "AySound.h"
#include "hardconfig.h"
#include "ESPectrum.h"

// #pragma GCC optimize("O3")

/* emulator settings */
int AySound::table[32];                 /**< table of volumes for chip */
ayemu_chip_t AySound::type;             /**< general chip type (\b AYEMU_AY or \b AYEMU_YM) */
int AySound::ChipFreq;                  /**< chip emulator frequency */
// int AySound::eq[6];                     /**< volumes for channels.
                                        // Array contains 6 elements: 
                                        // A left, A right, B left, B right, C left and C right;
                                        // range -100...100 */
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

// int AySound::vols[6][32];               /**< stereo type (channel volumes) and chip table.
//                                         This cache calculated by #table and #eq    */

// int AySound::vols[32];

int AySound::EnvNum;                    /**< number of current envilopment (0...15) */
int AySound::env_pos;                   /**< current position in envelop (0...127) */
int AySound::Cur_Seed;                  /**< random numbers counter */

uint8_t AySound::regs[16];

uint8_t AySound::SamplebufAY[ESP_AUDIO_SAMPLES_PENTAGON] = { 0 };

void (*AySound::updateReg[16])() = {
    &updToneA,&updToneA,&updToneB,&updToneB,&updToneC,
    &updToneC,&updNoisePitch,&updMixer,&updVolA,&updVolB,
    &updVolC,&updEnvFreq,&updEnvFreq,&updEnvType,&updIOPortA,&updIOPortB
};

// Status
uint8_t AySound::selectedRegister;

#define AYEMU_MAX_AMP 140 // This results in output values between 0-158

#define AYEMU_DEFAULT_CHIP_FREQ 1773400

/* sound chip volume envelops (will calculated by gen_env()) */
static int bEnvGenInit = 0;
static uint8_t Envelope [16][128];

/* AY volume table (c) by V_Soft and Lion 17 */
// static int Lion17_AY_table [16] = {
//     0, 513, 828, 1239, 1923, 3238, 4926, 9110,
//     10344, 17876, 24682, 30442, 38844, 47270, 56402, 65535
// };

// /* AY volume table (C) Rampa 2023 */
// static int Rampa_AY_table [16] = {
//     0, 521, 735, 1039, 1467, 2072, 2927, 4135,
//     5841, 8250, 11654, 16462, 23253, 32845, 46395, 65535
// };

DRAM_ATTR static const uint8_t Rampa_AY_table[16] = {0,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31};

// Borrowed from SoftSpectrum48 source code:
// Values according to: http://forum.tslabs.info/viewtopic.php?f=6&t=539
// AmplitudeFactors = { 0.0f, 0.01f, 0.014f, 0.021f, 0.031f, 0.046f, 0.064f, 
// 0.107f, 0.127f, 0.205f, 0.292f, 0.373f, 0.493f, 0.635f, 0.806f, 1.0f }
// static int SoftSpec48_AY_table [16] = {
//     0, 655, 917, 1376, 2032, 3015, 4194, 7012,
//     8323, 13435, 19136, 24445, 32309, 41615, 52821, 65535
// };

/* make chip hardware envelop tables.
        Will execute once before first use. */
static void gen_env()
{
    int env;
    int pos;
    int hold;
    int dir;
    int vol;

    // printf("Gen Env:\n");
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
            Envelope[env][pos] = (uint8_t) vol;
            // printf("%d ",vol);
        }
        // printf("\n");
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
    
    cnt_a = cnt_b = cnt_c = cnt_n = cnt_e = 0;
    bit_a = bit_b = bit_c = bit_n = 0;
    env_pos = EnvNum = 0;

    /* GenNoise (c) Hacker KAY & Sergey Bulba */
    Cur_Seed = 0xffff;

    // /* GenNoise (c) SoftSpectrum 48 */
    // Cur_Seed = 0x1ffff;

    // /* GenNoise (c) JSpeccy */
    // period_n = 1;
    // Cur_Seed = 1;

    if (!bEnvGenInit) gen_env();

}

/** Set chip type. */
int AySound::set_chip_type(ayemu_chip_t type, int *custom_table)
{

    const float max_volume = float (AYEMU_MAX_AMP / 3.0f);	// As there are three channels.
    float root_two = 1.414213562373095f;
    
    for(int v = 0; v < 32; v++) {
		table[v] = int(max_volume / powf(root_two, float(v ^ 0x1f) / 3.18f));
	}
    // Tie level 0 to silence.
	for(int v = 31; v >= 0; --v)
        table[v] -= table[0];

    // for (int n = 0; n < 32; n++)
    //     // table[n] = Lion17_AY_table[n / 2];
    //     table[n] = Rampa_AY_table[n / 2];
    //     // table[n] = SoftSpec48_AY_table[n / 2];
    
    type = AYEMU_AY;

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
    sndfmt.freq = freq;
    sndfmt.channels = chans;
    sndfmt.bpc = bits;

    default_sound_format_flag = 0;
    dirty = 1;
    return 1;
}

//  Set amplitude factor for each of channels (A,B anc C, tone and noise).
//  Factor's value must be from (-100) to 100.
//  \arg ay - pointer to ayemu_t structure
//  \arg stereo_type - type of stereo
//  \arg custom_eq - NULL or pointer to custom table of mixer layout.
//  \retval 1 if OK, 0 if error occures.
//
int AySound::set_stereo(ayemu_stereo_t stereo_type, int *custom_eq)
{
    // int i;
    // int chip;

    // for (i = 0 ; i < 6 ; i++) eq[i] = 100;

    default_stereo_flag = 0;
    dirty = 1;
    return 1;
}

void AySound::prepare_generation()
{

    // int vol;

    if (!dirty) return;

    if (default_chip_flag) set_chip_type(AYEMU_AY, NULL);

    if (default_stereo_flag) set_stereo(AYEMU_ABC, NULL);

    if (default_sound_format_flag) set_sound_format(44100, 2, 16);

    ChipTacts_per_outcount = ChipFreq / sndfmt.freq / 8;

    // for (int n = 0; n < 32; n++) {
    //     // vol = table[n];
    //     for (int m=0; m < 6; m++)
    //         // vols[m][n] = (int) (((double) vol * eq[m]) / 100);
    //         vols[m][n] = table[n];
    // }

    // int max_l = vols[0][31] + vols[2][31] + vols[4][31];
    // int max_r = vols[1][31] + vols[3][31] + vols[5][31];

    // vol = (max_l > max_r) ? max_l : max_r;
    // Amp_Global = ChipTacts_per_outcount * vol / AYEMU_MAX_AMP;

    Amp_Global = ChipTacts_per_outcount * (table[31] * 3) / AYEMU_MAX_AMP;

    dirty = 0;
}

//
// Generate sound.
// Fill sound buffer with current register data
//
IRAM_ATTR void AySound::gen_sound(int sound_bufsize, int bufpos)
{

    int tmpvol;
    uint8_t *sound_buf = SamplebufAY + bufpos;

    // int snd_numcount = sound_bufsize / (sndfmt.channels * (sndfmt.bpc >> 3));
    // while (snd_numcount-- > 0) {
    while (sound_bufsize-- > 0) {        

        int mix_l = 0;
        
        for (int m = 0 ; m < ChipTacts_per_outcount ; m++) {

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

            /* GenNoise (c) Hacker KAY & Sergey Bulba */
            if (++cnt_n >= (ayregs.noise * 2)) {
                cnt_n = 0;
                Cur_Seed = (Cur_Seed * 2 + 1) ^ \
                    (((Cur_Seed >> 16) ^ (Cur_Seed >> 13)) & 1); 
                bit_n = ((Cur_Seed >> 16) & 1);
            }
            /* End of GenNoise (c) Hacker KAY & Sergey Bulba */

            // /* GenNoise (c) SoftSpectrum 48 */
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
            // /* End of GenNoise (c) SoftSpectrum 48 */

            // // /* GenNoise (c) JSpeccy */
            // if (++cnt_n >= period_n) {

            //     cnt_n = 0;
            //      // Changes to R6 take effect only when internal counter reaches 0
            //     period_n = ayregs.noise;
            //     if (period_n == 0) {
            //         period_n = 1;
            //     }
            //     period_n <<= 1;

            //     // Code borrowed from MAME sources
            //     /* Is noise output going to change? */
            //     if (((Cur_Seed + 1) & 0x02) != 0) { /* (bit0^bit1)? */
            //         bit_n = !bit_n;
            //     }

            //     /* The Random Number Generator of the 8910 is a 17-bit shift */
            //     /* register. The input to the shift register is bit0 XOR bit3 */
            //     /* (bit0 is the output). This was verified on AY-3-8910 and YM2149 chips. */
            //     /* The following is a fast way to compute bit17 = bit0^bit3. */
            //     /* Instead of doing all the logic operations, we only check */
            //     /* bit0, relying on the fact that after three shifts of the */
            //     /* register, what now is bit3 will become bit0, and will */
            //     /* invert, if necessary, bit14, which previously was bit17. */
            //     if ((Cur_Seed & 0x01) != 0) {
            //         Cur_Seed ^= 0x24000; /* This version is called the "Galois configuration". */
            //     }
            //     Cur_Seed >>= 1;
            //     // End of Code borrowed from MAME sources
            // }
            // // /* End of GenNoise (c) JSpeccy */            

            if (++cnt_e >= ayregs.env_freq) {
                cnt_e = 0;
                if (++env_pos > 127)
                    env_pos = 64;
            }

            #define ENVVOL Envelope[ayregs.env_style][env_pos]

            if ((bit_a | !ayregs.R7_tone_a) & (bit_n | !ayregs.R7_noise_a)) {
                tmpvol = (ayregs.env_a) ? ENVVOL : Rampa_AY_table[ayregs.vol_a];
                mix_l += table[tmpvol];
            }

            if ((bit_b | !ayregs.R7_tone_b) & (bit_n | !ayregs.R7_noise_b)) {
                tmpvol = (ayregs.env_b) ? ENVVOL : Rampa_AY_table[ayregs.vol_b];
                mix_l += table[tmpvol];
            }
            
            if ((bit_c | !ayregs.R7_tone_c) & (bit_n | !ayregs.R7_noise_c)) {
                tmpvol = (ayregs.env_c) ? ENVVOL : Rampa_AY_table[ayregs.vol_c];
                mix_l += table[tmpvol];
            }            

        }
        
        *sound_buf++ = mix_l / Amp_Global;

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
    ayregs.R7_tone_a = !(regs[7] & 0x01);
    ayregs.R7_tone_b = !(regs[7] & 0x02);
    ayregs.R7_tone_c = !(regs[7] & 0x04);

    ayregs.R7_noise_a = !(regs[7] & 0x08);
    ayregs.R7_noise_b = !(regs[7] & 0x10);
    ayregs.R7_noise_c = !(regs[7] & 0x20);
}

void AySound::updVolA() {
    ayregs.vol_a = regs[8] & 0x0f;
    ayregs.env_a = regs[8] & 0x10;
}

void AySound::updVolB() {
    ayregs.vol_b = regs[9] & 0x0f;
    ayregs.env_b = regs[9] & 0x10;
}

void AySound::updVolC() {
    ayregs.vol_c = regs[10] & 0x0f;
    ayregs.env_c = regs[10] & 0x10;
}

void AySound::updEnvFreq() {
    ayregs.env_freq = regs[11] + (regs[12] << 8);
}

void AySound::updEnvType() {
    
    // This shouldn't happen on AY
    // if (regs[13] == 0xff) { // R13 = 255 means continue current envelop
    //     // printf("ENV TYPE 0xff!\n");
    //     return;
    // }

    ayregs.env_style = regs[13] & 0x0f;
    env_pos = cnt_e = 0;

}

void AySound::updIOPortA() {
    ayregs.IOPortA = regs[14] & 0xff;
}

void AySound::updIOPortB() {
    ayregs.IOPortB = regs[15] & 0xff;
}

uint8_t AySound::getRegisterData()
{

    if ((selectedRegister >= 14) && ((regs[7] >> (selectedRegister - 8)) & 1) == 0) {
        // printf("getAYRegister %d: %02X\n", selectedRegister, 0xFF);
        return 0xFF;
    }

    switch(selectedRegister) {
      case 0x00: return ayregs.tone_a & 0xff;
      case 0x01: return (ayregs.tone_a >> 8) & 0x0f;
      case 0x02: return ayregs.tone_b & 0xff;
      case 0x03: return (ayregs.tone_b >> 8) & 0x0f;
      case 0x04: return ayregs.tone_c & 0xff;
      case 0x05: return (ayregs.tone_c >> 8) & 0x0f;
      case 0x06: return ayregs.noise & 0x1f;
      case 0x07: return regs[7];
      case 0x08: return ayregs.env_a + ayregs.vol_a;
      case 0x09: return ayregs.env_b + ayregs.vol_b;
      case 0x0a: return ayregs.env_c + ayregs.vol_c;
      case 0x0b: return ayregs.env_freq & 0x00ff;
      case 0x0c: return ayregs.env_freq >> 8;
      case 0x0d: return ayregs.env_style & 0x0f;
      case 0x0e: return ayregs.IOPortA;
      case 0x0f: return ayregs.IOPortB;
    }
    
    return 0;

}

void AySound::selectRegister(uint8_t registerNumber)
{
    selectedRegister = registerNumber;
}

void AySound::setRegisterData(uint8_t data)
{

    if (selectedRegister < 16) {
        regs[selectedRegister] = data;
        updateReg[selectedRegister]();
    }

}

void AySound::reset()
{

    cnt_a = cnt_b = cnt_c = cnt_n = cnt_e = 0;
    bit_a = bit_b = bit_c = bit_n = 0;
    env_pos = EnvNum = 0;

    /* GenNoise (c) Hacker KAY & Sergey Bulba */
    Cur_Seed = 0xffff;

    // // /* GenNoise (c) SoftSpectrum 48 */
    // // Cur_Seed = 0x1ffff;

    // /* GenNoise (c) JSpeccy */
    // period_n = 1;
    // Cur_Seed = 1;

    prepare_generation();

    for (int i=0;i<16;i++) regs[i] = 0; // All registers are set to 0
    
    regs[7] = 0xff; // Mixer register

    selectedRegister = 0xff;

    for(int i=0; i < 16; i++) updateReg[i](); // Update all registers

}
