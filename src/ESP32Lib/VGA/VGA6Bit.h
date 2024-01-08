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
#include "../Graphics/GraphicsR2G2B2S2Swapped.h"

class VGA6Bit : public VGA, public GraphicsR2G2B2S2Swapped
{
  public:

	bool VGA6Bit_useinterrupt;

	VGA6Bit() //8 bit based modes only work with I2S1
		: VGA(1)
	{
		interruptStaticChild = &VGA6Bit::interrupt;
	}

	// bool init(const Mode &mode, const int *redPins, const int *greenPins, const int *bluePins, const int hsyncPin, const int vsyncPin, const int clockPin = -1)
	bool init(int mode, const int *redPins, const int *greenPins, const int *bluePins, const int hsyncPin, const int vsyncPin, const int clockPin = -1)	
	{
		int pinMap[8];
		for (int i = 0; i < 2; i++)
		{
			pinMap[i] = redPins[i];
			pinMap[i + 2] = greenPins[i];
			pinMap[i + 4] = bluePins[i];
		}
		pinMap[6] = hsyncPin;
		pinMap[7] = vsyncPin;			
		return VGA::init(mode, pinMap, 8, clockPin);
	}

	virtual void initSyncBits()
	{
		hsyncBitI = vidmodes[mode][vmodeproperties::hSyncPolarity] ? 0x40 : 0;
		vsyncBitI = vidmodes[mode][vmodeproperties::vSyncPolarity] ? 0x80 : 0;
		hsyncBit = hsyncBitI ^ 0x40;
		vsyncBit = vsyncBitI ^ 0x80;
		SBits = hsyncBitI | vsyncBitI;
	}
		
	virtual long syncBits(bool hSync, bool vSync)
	{
		return ((hSync ? hsyncBit : hsyncBitI) | (vSync ? vsyncBit : vsyncBitI)) * 0x1010101;
	}

	virtual float pixelAspect() const
	{
		return 1;
	}

	virtual void propagateResolution(const int xres, const int yres)
	{
		setResolution(xres, yres);
	}

	void *vSyncInactiveBuffer;
	void *vSyncActiveBuffer;
	void *inactiveBuffer;
	void *blankActiveBuffer;

	virtual Color **allocateFrameBuffer()
	{
		return (Color **)DMABufferDescriptor::allocateDMABufferArray(yres, vidmodes[mode][vmodeproperties::hRes], true, syncBits(false, false));
	}

	virtual void allocateLineBuffers()
	{
		VGA::allocateLineBuffers((void **)frameBuffers[0]);
	}

	virtual void show(bool vSync = false)
	{
		if (!frameBufferCount)
			return;
		if (vSync)
		{
			//TODO read the I2S docs to find out
		}
		Graphics::show(vSync);
		if(dmaBufferDescriptors)
		for (int i = 0; i < yres * vidmodes[mode][vmodeproperties::vDiv]; i++)
			dmaBufferDescriptors[(vidmodes[mode][vmodeproperties::vFront] + vidmodes[mode][vmodeproperties::vSync] + vidmodes[mode][vmodeproperties::vBack] + i) * 2 + 1].setBuffer(frontBuffer[i / vidmodes[mode][vmodeproperties::vDiv]], vidmodes[mode][vmodeproperties::hRes]);
	}

	virtual void scroll(int dy, Color color)
	{
		Graphics::scroll(dy, color);
		if (frameBufferCount == 1)
			show();
	}

  protected:

	bool useInterrupt()
	{ 
		return VGA6Bit_useinterrupt;
	};

	static void interrupt(void *arg);

};
