///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum - ZX Spectrum emulator for ESP32
//
// Copyright (c) 2020, 2021 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez, Jorge Fuertes and many others
// https://github.com/rampa069/ZX-ESPectrum
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

#ifdef USE_AY_SOUND

//static SoundGenerator _soundGenerator;

SquareWaveformGenerator AySound::_channel[3];

// Registers
uint8_t AySound::finePitchChannelA = 0xFF;
uint8_t AySound::coarsePitchChannelA = 0xFF;
uint8_t AySound::finePitchChannelB = 0xFF;
uint8_t AySound::coarsePitchChannelB = 0xFF;
uint8_t AySound::finePitchChannelC = 0xFF;
uint8_t AySound::coarsePitchChannelC = 0xFF;
uint8_t AySound::noisePitch = 0xFF;
uint8_t AySound::mixer = 0xFF;
uint8_t AySound::volumeChannelA = 0xFF;
uint8_t AySound::volumeChannelB = 0xFF;
uint8_t AySound::volumeChannelC = 0xFF;
uint8_t AySound::envelopeFineDuration = 0xFF;
uint8_t AySound::envelopeCoarseDuration = 0xFF;
uint8_t AySound::envelopeShape = 0xFF;
uint8_t AySound::ioPortA = 0xFF;

// Status
uint8_t AySound::selectedRegister = 0xFF;
uint8_t AySound::channelVolume[3] = { 0xFF, 0xFF, 0xFF };
uint16_t AySound::channelFrequency[3] = { 0xFFFF, 0xFFFF, 0xFFFF };

const unsigned char volume[] = {
	0, 8, 17, 25, 34, 42, 51, 59, 68, 76, 85, 93, 102, 110, 119, 127
};

void AySound::initialize()
{
    //_soundGenerator.setVolume(126);
    //_soundGenerator.play(true);
	for (int8_t channel = 0; channel < 3; channel++)
	{
        //_soundGenerator.attach(&_channel[channel]);
        _channel[channel].enable(true);
        _channel[channel].setVolume(0);
	}
}

void AySound::enable()
{
    //_soundGenerator.play(true);
}

void AySound::disable()
{
    //_soundGenerator.play(false);
}

// Reference frequency is calculated like this:
// for a AY-3-8912 running at 4MHz, reference frequency is 125000 Hz.
// But on a Spectrum 128K, it runs at 3.5469MHz, so the
// frequency is 125000 * 3.5469 / 4 = 110840
#define FREQUENCY_REFERENCE 110840

inline int fixPitch(int p)
{
	int fixed = (p > 0) ? (FREQUENCY_REFERENCE / p) : 0;
	if (fixed > 15000) fixed = 15000;
	return fixed;
}

void AySound::update()
{
	uint16_t oldChannelFrequency[3];
	uint8_t oldChannelVolume[3];

	for (uint8_t channel = 0; channel < 3; channel++)
	{
		uint8_t chVolume = channelVolume[channel];
		oldChannelFrequency[channel] = chVolume ? channelFrequency[channel] : 0xFFFF;
		oldChannelVolume[channel] = chVolume;
	}

	int16_t pitch;

	switch (selectedRegister)
	{
	case 0:
	case 1:
		pitch = ((coarsePitchChannelA << 8) | finePitchChannelA) & 0x0FFF;
		channelFrequency[0] = pitch;
		break;
	case 2:
	case 3:
		pitch = ((coarsePitchChannelB << 8) | finePitchChannelB) & 0x0FFF;
		channelFrequency[1] = pitch;
		break;
	case 4:
	case 5:
		pitch = ((coarsePitchChannelC << 8) | finePitchChannelC) & 0x0FFF;
		channelFrequency[2] = pitch;
		break;
	case 6:
		// noisePitch - ignored for now
		break;
	case 7:
		channelVolume[0] = (mixer & 0x01) ? 0 : volume[volumeChannelA & 0x0F];
		channelVolume[1] = (mixer & 0x02) ? 0 : volume[volumeChannelB & 0x0F];
		channelVolume[2] = (mixer & 0x04) ? 0 : volume[volumeChannelC & 0x0F];
		break;
	case 8:
		channelVolume[0] = (mixer & 0x01) ? 0 : volume[volumeChannelA & 0x0F];
		break;
	case 9:
		channelVolume[1] = (mixer & 0x02) ? 0 : volume[volumeChannelB & 0x0F];
		break;
	case 10:
		channelVolume[2] = (mixer & 0x04) ? 0 : volume[volumeChannelC & 0x0F];
		break;
	case 11:
		// envelopeFineDuration - ignored for now
		break;
	case 12:
		// envelopeCoarseDuration - ignored for now
		break;
	case 13:
		// envelopeShape - ignored for now
		break;
	case 14:
		// ioPortA - ignored for now
		break;
	}

	for (int8_t channel = 0; channel < 3; channel++)
	{
		if (channelVolume[channel] != oldChannelVolume[channel])
		{
            _channel[channel].setVolume(channelVolume[channel]);
		}

		if (channelVolume[channel] == 0)
		{
            _channel[channel].setVolume(0);

			continue;
		}

		if (channelFrequency[channel] != oldChannelFrequency[channel])
		{
            _channel[channel].setFrequency(fixPitch(channelFrequency[channel]));
		}
	}
}

uint8_t AySound::getRegisterData()
{
	switch (selectedRegister)
	{
	case 0:
		return finePitchChannelA;
	case 1:
		return coarsePitchChannelA;
	case 2:
		return finePitchChannelB;
	case 3:
		return coarsePitchChannelB;
	case 4:
		return finePitchChannelC;
	case 5:
		return coarsePitchChannelC;
	case 6:
		return noisePitch;
	case 7:
		return mixer;
	case 8:
		return volumeChannelA;
	case 9:
		return volumeChannelB;
	case 10:
		return volumeChannelC;
	case 11:
		return envelopeFineDuration;
	case 12:
		return envelopeCoarseDuration;
	case 13:
		return envelopeShape;
	case 14:
		return ioPortA;
	default:
		return 0;
	}
}

void AySound::selectRegister(uint8_t registerNumber)
{
	selectedRegister = registerNumber;
}

void AySound::setRegisterData(uint8_t data)
{
	switch (selectedRegister)
	{
        case 0:
            finePitchChannelA = data;
            break;
        case 1:
            coarsePitchChannelA = data;
            break;
        case 2:
            finePitchChannelB = data;
            break;
        case 3:
            coarsePitchChannelB = data;
            break;
        case 4:
            finePitchChannelC = data;
            break;
        case 5:
            coarsePitchChannelC = data;
            break;
        case 6:
            noisePitch = data;
            break;
        case 7:
            mixer = data;
            break;
        case 8:
            volumeChannelA = data;
            break;
        case 9:
            volumeChannelB = data;
            break;
        case 10:
            volumeChannelC = data;
            break;
        case 11:
            envelopeFineDuration = data;
            break;
        case 12:
            envelopeCoarseDuration = data;
            break;
        case 13:
            envelopeShape = data;
            break;
        case 14:
            ioPortA = data;
            break;
        default:
            // invalid register - do nothing
            return;
    }

    update();
}

void AySound::reset()
{
	finePitchChannelA = 0xFF;
	coarsePitchChannelA = 0xFF;
	finePitchChannelB = 0xFF;
	coarsePitchChannelB = 0xFF;
	finePitchChannelC = 0xFF;
	coarsePitchChannelC = 0xFF;
	noisePitch = 0xFF;
	mixer = 0xFF;
	volumeChannelA = 0xFF;
	volumeChannelB = 0xFF;
	volumeChannelC = 0xFF;
	envelopeFineDuration = 0xFF;
	envelopeCoarseDuration = 0xFF;
	envelopeShape = 0xFF;
	ioPortA = 0xFF;

	// Status
	selectedRegister = 0xFF;
    for (int channel = 0; channel < 3; channel++)
    {
	    channelVolume[channel] = 0xFF;
	    channelFrequency[channel] = 0xFFFF;
        _channel[channel].setVolume(0);
    }
}

#endif
