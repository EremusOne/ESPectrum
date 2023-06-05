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
#include "VGA.h"
#include "hardconfig.h"

// VIDEO MODES:
// hfront hsync hback pixels vfront vsync vback lines divy pixelclock hpolaritynegative vpolaritynegative

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VGA STANDARD MODES (TIMING USES PWM_AUDIO SO 48K MODES = 128K MODES)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// (PREVIOUS)
// const Mode VGA::MODE320x240(8, 48, 24, 320, 11, 2, 31, 480, 2, 12587500, 1, 1, 41,84,7,7,0,0,6,6);  // 31469 / 60.05
// const Mode VGA::MODE360x200(8, 54, 28, 360, 11, 2, 32, 400, 2, 14161000, 1, 0, 44,84,7,6,0,0,6,5);  // 31469 / 70.72

const Mode VGA::MODE320x240(8, 48, 24, 320, 10, 2, 33, 480, 2, 12587500, 1, 1, 41,84,7,7,0,0,6,6);  // 31469 / 59.94 INDUSTRY STANDARD 
const Mode VGA::MODE360x200(9, 54, 27, 360, 12, 2, 35, 400, 2, 14160000, 1, 0, 121,233,5,5,0,0,6,5);  // 31467 / 70.08 TAKEN FROM FABGL

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VGA 50HZ MODES:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 48K

// This is a test using divy = 2, it works in one of my monitors but hFreq is quite low so it must be not quite compatible
// const Mode VGA::MODE320x240_50_48(8, 48, 24, 320, 11, 2, 31, 480, 2, 10496768, 1, 1, 228,139,7,9,0,0,8,9);  // 26242 / 50.08

// 48K 50hz VGA, SYNC @ 19968 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
const Mode VGA::MODE320x240_50_48(8, 42, 50, 320, 27, 6, 53, 720, 3, 16953124, 1, 1, 0,44,6,4,0,0,8,5);  // 40365 / 50.0801282
const Mode VGA::MODE360x200_50_48(18, 36, 54, 360, 31, 3, 33, 600, 3, 15632812, 1, 1, 51,97,5,4,0,0,7,5);  // 33403 / 50.0801282

// 128K

// This is a test of 128K mode using divy = 2, it works in one of my monitors but hFreq is quite low so it could be not quite compatible
// const Mode VGA::MODE320x240_50_128(8, 48, 24, 320, 11, 2, 31, 480, 2, 10484193, 1, 1, 143,111,5,7,0,0,8,9);  // 26210 / 50.020008

// 128K 50hz VGA, SYNC @ 19992 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
const Mode VGA::MODE320x240_50_128(8, 42, 50, 320, 27, 6, 53, 720, 3, 16932773, 1, 1, 224,40,6,4,0,0,8,5);  // 40316 / 50.020008
const Mode VGA::MODE360x200_50_128(18, 36, 54, 360, 31, 3, 33, 600, 3, 15614045, 1, 1, 194,125,8,6,0,0,7,5);  // 33363 / 50.020008

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TV MODES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 48K

// PREVIOUS
// const Mode VGA::MODE320x240_TV_48(38, 32, 58, 320, 28, 3, 41, 240, 1, 7000000, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.08
// const Mode VGA::MODE360x200_TV_48(18, 32, 38, 360, 48, 3, 61, 200, 1, 7000000, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.08

// ¡PENDING FINE TUNE SYNC TESTS! -> 48K 50hz CRT, SYNC @ 19968 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
const Mode VGA::MODE320x240_TV_48(38, 32, 58, 320, 28, 3, 41, 240, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.0801282
const Mode VGA::MODE360x200_TV_48(18, 32, 38, 360, 48, 3, 61, 200, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.0801282

// 128K

// PREVIOUS
// const Mode VGA::MODE320x240_TV_128(42, 32, 62, 320, 28, 3, 40, 240, 1, 7093800, 1, 1, 107,238,5,12,0,0,8,15);  // 15557 / 50.02
// const Mode VGA::MODE360x200_TV_128(22, 32, 42, 360, 48, 3, 60, 200, 1, 7093800, 1, 1, 107,238,5,12,0,0,8,15);  // 15557 / 50.02

// ¡PENDING FINE TUNE SYNC TESTS! -> 128K 50hz CRT, SYNC @ 19992 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
const Mode VGA::MODE320x240_TV_128(42, 32, 62, 320, 28, 3, 40, 240, 1, 7093637, 1, 1, 245,163,6,13,0,0,8,15);  // 15556 / 50.020008
const Mode VGA::MODE360x200_TV_128(22, 32, 42, 360, 48, 3, 60, 200, 1, 7093637, 1, 1, 245,163,6,13,0,0,8,15);  // 15556 / 50.020008

const Mode VGA::videomodes[3][2][2]={
	{ {MODE320x240, MODE360x200}, {MODE320x240, MODE360x200} },
	{ {MODE320x240_50_48, MODE360x200_50_48}, {MODE320x240_50_128, MODE360x200_50_128} },
	{ {MODE320x240_TV_48, MODE360x200_TV_48}, {MODE320x240_TV_128, MODE360x200_TV_128} }
};				

VGA::VGA(const int i2sIndex)
	: I2S(i2sIndex)
{
	lineBufferCount = 8;
	dmaBufferDescriptors = 0;
}

bool VGA::init(const Mode &mode, const int *pinMap, const int bitCount, const int clockPin)
{
	this->mode = mode;
	int xres = mode.hRes;
	int yres = mode.vRes / mode.vDiv;
	initSyncBits();
	propagateResolution(xres, yres);
	this->vsyncPin = vsyncPin;
	this->hsyncPin = hsyncPin;
	totalLines = mode.linesPerField();
	// printf("totallines: %d\n",totalLines);
	allocateLineBuffers();
	currentLine = 0;
	vSyncPassed = false;
	// initParallelOutputMode(pinMap, mode.pixelClock, bitCount, clockPin);
	initPrecalcParallelOutputMode(pinMap, mode, bitCount, clockPin);

	startTX();
	return true;
}

void VGA::setLineBufferCount(int lineBufferCount)
{
	this->lineBufferCount = lineBufferCount;
}

void VGA::allocateLineBuffers()
{
	allocateLineBuffers(lineBufferCount);
}

/// simple ringbuffer of blocks of size bytes each
void VGA::allocateLineBuffers(const int lines)
{
	dmaBufferDescriptorCount = lines;
	dmaBufferDescriptors = DMABufferDescriptor::allocateDescriptors(dmaBufferDescriptorCount);
	int bytes = (mode.hFront + mode.hSync + mode.hBack + mode.hRes) * bytesPerSample();
	for (int i = 0; i < dmaBufferDescriptorCount; i++)
	{
		dmaBufferDescriptors[i].setBuffer(DMABufferDescriptor::allocateBuffer(bytes, true), bytes); //front porch + hsync + back porch + pixels
		if (i)
			dmaBufferDescriptors[i - 1].next(dmaBufferDescriptors[i]);
	}
	dmaBufferDescriptors[dmaBufferDescriptorCount - 1].next(dmaBufferDescriptors[0]);
}

///complete ringbuffer from frame
void VGA::allocateLineBuffers(void **frameBuffer)
{
	dmaBufferDescriptorCount = totalLines * 2;
	int inactiveSamples = (mode.hFront + mode.hSync + mode.hBack + 3) & 0xfffffffc;
	vSyncInactiveBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples * bytesPerSample(), true);
	vSyncActiveBuffer = DMABufferDescriptor::allocateBuffer(mode.hRes * bytesPerSample(), true);
	inactiveBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples * bytesPerSample(), true);
	blankActiveBuffer = DMABufferDescriptor::allocateBuffer(mode.hRes * bytesPerSample(), true);
	if(bytesPerSample() == 1)
	{
		for (int i = 0; i < inactiveSamples; i++)
		{
			if (i >= mode.hFront && i < mode.hFront + mode.hSync)
			{
				((unsigned char *)vSyncInactiveBuffer)[i ^ 2] = hsyncBit | vsyncBit;
				((unsigned char *)inactiveBuffer)[i ^ 2] = hsyncBit | vsyncBitI;
			}
			else
			{
				((unsigned char *)vSyncInactiveBuffer)[i ^ 2] = hsyncBitI | vsyncBit;
				((unsigned char *)inactiveBuffer)[i ^ 2] = hsyncBitI | vsyncBitI;
			}
		}
		for (int i = 0; i < mode.hRes; i++)
		{
			((unsigned char *)vSyncActiveBuffer)[i ^ 2] = hsyncBitI | vsyncBit;
			((unsigned char *)blankActiveBuffer)[i ^ 2] = hsyncBitI | vsyncBitI;
		}
	}
	else if(bytesPerSample() == 2)
	{
		for (int i = 0; i < inactiveSamples; i++)
		{
			if (i >= mode.hFront && i < mode.hFront + mode.hSync)
			{
				((unsigned short *)vSyncInactiveBuffer)[i ^ 1] = hsyncBit | vsyncBit;
				((unsigned short *)inactiveBuffer)[i ^ 1] = hsyncBit | vsyncBitI;
			}
			else
			{
				((unsigned short *)vSyncInactiveBuffer)[i ^ 1] = hsyncBitI | vsyncBit;
				((unsigned short *)inactiveBuffer)[i ^ 1] = hsyncBitI | vsyncBitI;
			}
		}
		for (int i = 0; i < mode.hRes; i++)
		{
			((unsigned short *)vSyncActiveBuffer)[i ^ 1] = hsyncBitI | vsyncBit;
			((unsigned short *)blankActiveBuffer)[i ^ 1] = hsyncBitI | vsyncBitI;
		}
	}

	dmaBufferDescriptors = DMABufferDescriptor::allocateDescriptors(dmaBufferDescriptorCount);
	for (int i = 0; i < dmaBufferDescriptorCount; i++)
		dmaBufferDescriptors[i].next(dmaBufferDescriptors[(i + 1) % dmaBufferDescriptorCount]);
	int d = 0;
	for (int i = 0; i < mode.vFront; i++)
	{
		dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples * bytesPerSample());
		dmaBufferDescriptors[d++].setBuffer(blankActiveBuffer, mode.hRes * bytesPerSample());
	}
	for (int i = 0; i < mode.vSync; i++)
	{
		dmaBufferDescriptors[d++].setBuffer(vSyncInactiveBuffer, inactiveSamples * bytesPerSample());
		dmaBufferDescriptors[d++].setBuffer(vSyncActiveBuffer, mode.hRes * bytesPerSample());
	}
	for (int i = 0; i < mode.vBack; i++)
	{
		dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples * bytesPerSample());
		dmaBufferDescriptors[d++].setBuffer(blankActiveBuffer, mode.hRes * bytesPerSample());
	}
	for (int i = 0; i < mode.vRes; i++)
	{
		dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples * bytesPerSample());
		dmaBufferDescriptors[d++].setBuffer(frameBuffer[i / mode.vDiv], mode.hRes * bytesPerSample());
	}
	// printf("buffer descriptors count: %d\n",d);
}

void VGA::vSync()
{
	vSyncPassed = true;
}

void VGA::interrupt()
{
	unsigned long *signal = (unsigned long *)dmaBufferDescriptors[dmaBufferDescriptorActive].buffer();
	unsigned long *pixels = &((unsigned long *)dmaBufferDescriptors[dmaBufferDescriptorActive].buffer())[(mode.hSync + mode.hBack) / 2];
	unsigned long base, baseh;
	if (currentLine >= mode.vFront && currentLine < mode.vFront + mode.vSync)
	{
		baseh = (vsyncBit | hsyncBit) | ((vsyncBit | hsyncBit) << 16);
		base = (vsyncBit | hsyncBitI) | ((vsyncBit | hsyncBitI) << 16);
	}
	else
	{
		baseh = (vsyncBitI | hsyncBit) | ((vsyncBitI | hsyncBit) << 16);
		base = (vsyncBitI | hsyncBitI) | ((vsyncBitI | hsyncBitI) << 16);
	}
	for (int i = 0; i < mode.hSync / 2; i++)
		signal[i] = baseh;
	for (int i = mode.hSync / 2; i < (mode.hSync + mode.hBack) / 2; i++)
		signal[i] = base;

	int y = (currentLine - mode.vFront - mode.vSync - mode.vBack) / mode.vDiv;
	if (y >= 0 && y < mode.vRes)
		interruptPixelLine(y, pixels, base);
	else
		for (int i = 0; i < mode.hRes / 2; i++)
		{
			pixels[i] = base | (base << 16);
		}
	for (int i = 0; i < mode.hFront / 2; i++)
		signal[i + (mode.hSync + mode.hBack + mode.hRes) / 2] = base;
	currentLine = (currentLine + 1) % totalLines;
	dmaBufferDescriptorActive = (dmaBufferDescriptorActive + 1) % dmaBufferDescriptorCount;
	if (currentLine == 0)
		vSync();
}

void VGA::interruptPixelLine(int y, unsigned long *pixels, unsigned long syncBits)
{
}
