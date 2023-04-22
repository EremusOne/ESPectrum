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

int AySound::default_sound_format_flag; /**< =1 after init, resets in #ayemu_set_sound_format() */
int AySound::dirty;                     /**< dirty flag. Sets if any emulator properties changed */

int AySound::bit_a;                     /**< state of channel A generator */
int AySound::bit_b;                     /**< state of channel B generator */
int AySound::bit_c;                     /**< state of channel C generator */
int AySound::bit_n;                     /**< current generator state */
int AySound::cnt_a;                     /**< back counter of A */
int AySound::cnt_b;                     /**< back counter of B */
int AySound::cnt_c;                     /**< back counter of C */
int AySound::cnt_n;                     /**< back counter of noise generator */
int AySound::cnt_e;                     /**< back counter of envelop generator */
int AySound::ChipTacts_per_outcount;    /**< chip's counts per one sound signal count */
int AySound::EnvNum;                    /**< number of current envilopment (0...15) */
int AySound::env_pos;                   /**< current position in envelop (0...127) */
uint32_t AySound::lfsr;                  /**< random numbers counter */

uint8_t AySound::regs[14];

void (*AySound::updateReg[14])() = {
    &updToneA,&updToneA,&updToneB,&updToneB,&updToneC,
    &updToneC,&updNoisePitch,&updMixer,&updVolA,&updVolB,
    &updVolC,&updEnvFreq,&updEnvFreq,&updEnvType
};

// Status
uint8_t AySound::selectedRegister;

// #define AYEMU_MAX_AMP 40000; // This results in output values between 0-158

#define AYEMU_MAX_AMP 140 //140 // This results in output values between 0-158

#define AYEMU_DEFAULT_CHIP_FREQ 1773400

/* sound chip volume envelops (will calculated by gen_env()) */
static int bEnvGenInit = 0;
static int Envelope [16][64];
static int Envelope_overflow_masks [16];

/* Make chip hardware envelop tables.
   Will execute once before first use. 
        
    Code Borrowed from CLK emulator.        
*/
static void gen_env()
{
    int env;
    int pos;
 
 	for(int env = 0; env < 16; env++) {
		for(int pos = 0; pos < 64; pos++) {
			switch(env) {
				case 0: case 1: case 2: case 3: case 9:
					/* Envelope: \____ */
					Envelope[env][pos] = (pos < 32) ? (pos ^0x1f) : 0;
					Envelope_overflow_masks [env] = 0x3f;
				break;
				case 4: case 5: case 6: case 7: case 15:
					/* Envelope: /____ */
					Envelope[env][pos] = (pos < 32) ? pos : 0;
					Envelope_overflow_masks [env] = 0x3f;
				break;

				case 8:
					/* Envelope: \\\\\\\\ */
					Envelope[env][pos] = (pos & 0x1f) ^ 0x1f;
					Envelope_overflow_masks [env] = 0x00;
				break;
				case 12:
					/* Envelope: //////// */
					Envelope[env][pos] = (pos & 0x1f);
					Envelope_overflow_masks [env] = 0x00;
				break;

				case 10:
					/* Envelope: \/\/\/\/ */
					Envelope[env][pos] = (pos & 0x1f) ^ ((pos < 32) ? 0x1f : 0x0);
					Envelope_overflow_masks [env] = 0x00;
				break;
				case 14:
					/* Envelope: /\/\/\/\ */
					Envelope[env][pos] = (pos & 0x1f) ^ ((pos < 32) ? 0x0 : 0x1f);
					Envelope_overflow_masks [env] = 0x00;
				break;

				case 11:
					/* Envelope: \------	(if - is high) */
					Envelope[env][pos] = (pos < 32) ? (pos^0x1f) : 0x1f;
					Envelope_overflow_masks [env] = 0x3f;
				break;
				case 13:
					/* Envelope: /------- */
					Envelope[env][pos] = (pos < 32) ? pos : 0x1f;
					Envelope_overflow_masks[env] = 0x3f;
				break;
			}
		}
	}

    bEnvGenInit = 1;

}

void AySound::init()
{

    ChipFreq = AYEMU_DEFAULT_CHIP_FREQ;
    default_sound_format_flag = 1;
    dirty = 1;
    
    cnt_a = cnt_b = cnt_c = cnt_n = cnt_e = 0;
    bit_a = bit_b = bit_c = bit_n = 0;
    env_pos = EnvNum = 0;

    lfsr = 0x1;

    if (!bEnvGenInit) gen_env();

}

void AySound::generateVolumeTable()
{

    const float max_volume = float (AYEMU_MAX_AMP / 3.0f);	// As there are three channels.
    float root_two = 1.414213562373095f;
    
    for(int v = 0; v < 32; v++) {
		table[v] = int(max_volume / powf(root_two, float(v ^ 0x1f) / 3.18f));
	}
    // Tie level 0 to silence.
	for(int v = 31; v >= 0; --v) {
		table[v] -= table[0];
	}

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
    int i;
    int chip;

    for (i = 0 ; i < 6 ; i++) eq[i] = 100;

    default_stereo_flag = 0;
    dirty = 1;
    return 1;
}

void AySound::prepare_generation()
{
    if (!dirty) return;
    generateVolumeTable();
    if (default_sound_format_flag) set_sound_format(44100, 2, 16);
    ChipTacts_per_outcount = ChipFreq / sndfmt.freq / 8;
    dirty = 0;
}

//
// Generate sound.
// Fill sound buffer with current register data
//
void AySound::gen_sound(unsigned char *buff, size_t sound_bufsize, int bufpos)
{

    uint16_t mix_l;
    uint16_t tmpvol;
    unsigned char *sound_buf = buff;
    
    sound_buf += bufpos;

    int snd_numcount = sound_bufsize / (sndfmt.channels * (sndfmt.bpc >> 3));
    while (snd_numcount-- > 0) {

        mix_l = 0;
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

            if(++cnt_n >= ayregs.noise<<1)
            {
                cnt_n = 0;
                lfsr = (lfsr >> 1) ^ ((lfsr & 1) ? 0x14000 : 0);
                bit_n = lfsr & 1;
            }        

            if (++cnt_e >= ayregs.env_period) {
                cnt_e = 0;
                if (++env_pos == 64)
                    env_pos = Envelope_overflow_masks [ayregs.env_style];
                ;
            }

            #define ENVVOL Envelope[ayregs.env_style][env_pos |=1]
            
            uint8_t chan_a=(bit_a | !ayregs.R7_tone_a) & (bit_n | !ayregs.R7_noise_a);
            uint8_t chan_b=(bit_b | !ayregs.R7_tone_b) & (bit_n | !ayregs.R7_noise_b);
            uint8_t chan_c=(bit_c | !ayregs.R7_tone_c) & (bit_n | !ayregs.R7_noise_c);

            tmpvol= chan_a ? ayregs.env_a ? ENVVOL : ayregs.vol_a * 2 + 1 : 0;
            mix_l  +=table[tmpvol];
            
            tmpvol= chan_b ? ayregs.env_b ? ENVVOL : ayregs.vol_b * 2 + 1 : 0;
            mix_l  +=table[tmpvol];

            tmpvol= chan_c ? ayregs.env_c ? ENVVOL : ayregs.vol_c * 2 + 1 : 0;
            mix_l  +=table[tmpvol];
            
        } 
        if (regs[7]==0x3f)
            mix_l = mix_l >>2;

        mix_l = mix_l / ChipTacts_per_outcount;
        *sound_buf++ = (uint8_t) mix_l;

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
    ayregs.env_period = regs[11] + (regs[12] << 8);
}

void AySound::updEnvType() {
    
    ayregs.env_style = regs[13] & 0x0f;
    env_pos = cnt_e = 0;

}

uint8_t AySound::getRegisterData()
{

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
      case 0x0b: return ayregs.env_period & 0x00ff;
      case 0x0c: return ayregs.env_period >> 8;
      case 0x0d: return ayregs.env_style & 0x0f;
      default: return 0xff;
    }
    

}

void AySound::selectRegister(uint8_t registerNumber)
{
    selectedRegister = registerNumber;
}

void AySound::setRegisterData(uint8_t data)
{
    
    if (selectedRegister < 14) {
        regs[selectedRegister] = data;
        updateReg[selectedRegister]();
    } 
}

void AySound::reset()
{

    cnt_a = cnt_b = cnt_c = cnt_n = cnt_e = 0;
    bit_a = bit_b = bit_c = bit_n = 0;
    env_pos = EnvNum = 0;

    lfsr = 0x1;

    prepare_generation();

    for (int i=0;i<14;i++) regs[i] = 0;

    selectedRegister = 0xFF;

    for(int i=0; i<14; i++) updateReg[i]();

}
