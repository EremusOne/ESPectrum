/*
	Author: bitluni 2019
	License: 
	Creative Commons Attribution ShareAlike 4.0
	https://creativecommons.org/licenses/by-sa/4.0/
	
	For further details check out: 
		https://youtube.com/bitlunislab
		https://github.com/bitluni
		http://bitluni.net
*/
#pragma once
#include "VGA.h"
#include "../Graphics/GraphicsR2G2B2A2.h"

class VGA6BitI : public VGA, public GraphicsR2G2B2A2
{
  public:
	VGA6BitI()	//8 bit based modes only work with I2S1
		: VGA(1)
	{
		interruptStaticChild = &VGA6BitI::interrupt;
	}

	bool init(const Mode &mode,
			  const int R0Pin, const int R1Pin,
			  const int G0Pin, const int G1Pin,
			  const int B0Pin, const int B1Pin,
			  const int hsyncPin, const int vsyncPin, const int clockPin = -1)
	{
		int pinMap[8] = {
			R0Pin, R1Pin,
			G0Pin, G1Pin,
			B0Pin, B1Pin,
			hsyncPin, vsyncPin
		};
		return VGA::init(mode, pinMap, 8, clockPin);
	}

	bool init(const Mode &mode, const int *redPins, const int *greenPins, const int *bluePins, const int hsyncPin, const int vsyncPin, const int clockPin = -1)
	{
		int pinMap[8];
		for (int i = 0; i < 2; i++)
		{
			pinMap[i] = redPins[i];
			pinMap[i + 2] = greenPins[i];
			pinMap[i + 4] = bluePins[i];
		}
		pinMap[6] = hsyncPin;
		pinMap[7] = vsyncPin;			return VGA::init(mode, pinMap, 8, clockPin);
	}

	bool init(const Mode &mode, const PinConfig &pinConfig)
	{
		int pins[8];
		pinConfig.fill6Bit(pins);
		return VGA::init(mode, pins, 8, pinConfig.clock);
	}

	virtual void initSyncBits()
	{
		hsyncBitI = mode.hSyncPolarity ? 0x40 : 0;
		vsyncBitI = mode.vSyncPolarity ? 0x80 : 0;
		hsyncBit = hsyncBitI ^ 0x40;
		vsyncBit = vsyncBitI ^ 0x80;
	}

	virtual long syncBits(bool hSync, bool vSync)
	{
		return ((hSync ? hsyncBit : hsyncBitI) | (vSync ? vsyncBit : vsyncBitI)) * 0x1010101;
	}

	virtual int bytesPerSample() const
	{
		return 1;
	}

	virtual float pixelAspect() const
	{
		return 1;
	}

	virtual void propagateResolution(const int xres, const int yres)
	{
		setResolution(xres, yres);
	}

	virtual void show(bool vSync = false)
	{
		if (!frameBufferCount)
			return;
		if (vSync)
		{
			vSyncPassed = false;
			while (!vSyncPassed)
				vTaskDelay(0);
		}
		Graphics::show(vSync);
	}

  protected:
	bool useInterrupt()
	{ 
		return true; 
	};

	static void interrupt(void *arg);

	static void interruptPixelLine(int y, unsigned long *pixels, unsigned long syncBits, void *arg);
};
