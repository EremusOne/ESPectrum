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

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // VGA STANDARD MODES (TIMING USES PWM_AUDIO SO 48K MODES = 128K MODES)
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// const Mode VGA::MODE320x240(8, 48, 24, 320, 10, 2, 33, 480, 2, 12587500, 1, 1, 41,84,7,7,0,0,6,6);  // 31469 / 59.94 INDUSTRY STANDARD 
// // const Mode VGA::MODE360x200(9, 54, 27, 360, 12, 2, 35, 400, 2, 14160000, 1, 0, 121,233,5,5,0,0,6,5);  // 31467 / 70.08 TAKEN FROM FABGL
// // const Mode VGA::MODE360x200(9, 54, 27, 360, 12, 2, 35, 400, 2, 14163704, 1, 0, 186,84,7,6,0,0,6,5);  // 31475 / 70.1
// const Mode VGA::MODE360x200(9, 54, 27, 360, 12, 2, 35, 400, 2, 14161078, 1, 0, 48,84,7,6,0,0,6,5);  // 31469 / 70.087 TAKEN FROM SAMSUNG S22C450 MANUAL

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // VGA 50HZ MODES:
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // 48K

// // 48K 50hz VGA, SYNC @ 19968 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
// const Mode VGA::MODE320x240_50_48(8, 42, 50, 320, 27, 6, 53, 720, 3, 16953124, 1, 1, 0,44,6,4,0,0,8,5);  // 40365 / 50.0801282
// const Mode VGA::MODE360x200_50_48(18, 36, 54, 360, 31, 3, 33, 600, 3, 15632812, 1, 1, 51,97,5,4,0,0,7,5);  // 33403 / 50.0801282

// // 128K

// // 128K 50hz VGA, SYNC @ 19992 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
// const Mode VGA::MODE320x240_50_128(8, 42, 50, 320, 27, 6, 53, 720, 3, 16932773, 1, 1, 224,40,6,4,0,0,8,5);  // 40316 / 50.020008
// const Mode VGA::MODE360x200_50_128(18, 36, 54, 360, 31, 3, 33, 600, 3, 15614045, 1, 1, 194,125,8,6,0,0,7,5);  // 33363 / 50.020008

// // PENTAGON 128K

// // PENTAGON 128K 50hz VGA, SYNC @ 20480 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
// const Mode VGA::MODE320x240_50_PENTAGON(8, 42, 50, 320, 27, 6, 53, 720, 3, 16529296, 1, 1, 13,146,7,5,0,0,6,4);  // 39355 / 48.838125
// const Mode VGA::MODE360x200_50_PENTAGON(18, 36, 54, 360, 31, 3, 33, 600, 3, 15241992, 1, 1, 143,49,8,6,0,0,5,4);  // 32568 / 48.838125

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// // TV MODES
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // 48K

// // ¡PENDING FINE TUNE SYNC TESTS! -> 48K 50hz CRT, SYNC @ 19968 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
// // const Mode VGA::MODE320x240_TV_48(38, 32, 58, 320, 28, 3, 41, 240, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.0801282
// // const Mode VGA::MODE360x200_TV_48(18, 32, 38, 360, 48, 3, 61, 200, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.0801282

// // Front and Back porches adjusted to fit with other models
// const Mode VGA::MODE320x240_TV_48(42, 32, 62, 320, 28, 3, 40, 240, 1, 7102163, 1, 1, 59,167,6,13,0,0,6,12);  // 15575 / 50.0801282
// const Mode VGA::MODE360x200_TV_48(22, 32, 42, 360, 48, 3, 60, 200, 1, 7102163, 1, 1, 59,167,6,13,0,0,6,12);  // 15575 / 50.0801282

// // 128K

// // ¡PENDING FINE TUNE SYNC TESTS! -> 128K 50hz CRT, SYNC @ 19992 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
// const Mode VGA::MODE320x240_TV_128(42, 32, 62, 320, 28, 3, 40, 240, 1, 7093637, 1, 1, 245,163,6,13,0,0,8,15);  // 15556 / 50.020008
// const Mode VGA::MODE360x200_TV_128(22, 32, 42, 360, 48, 3, 60, 200, 1, 7093637, 1, 1, 245,163,6,13,0,0,8,15);  // 15556 / 50.020008

// // PENTAGON 128K

// // ¡PENDING FINE TUNE SYNC TESTS! -> PENTAGON 128K 50hz CRT, SYNC @ 20480 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
// const Mode VGA::MODE320x240_TV_PENTAGON(42, 32, 62, 320, 28, 3, 40, 240, 1, 6924609, 1, 1, 151,197,7,15,0,0,5,11);  // 15186 / 48.838125
// const Mode VGA::MODE360x200_TV_PENTAGON(22, 32, 42, 360, 48, 3, 60, 200, 1, 6924609, 1, 1, 151,197,7,15,0,0,5,11);  // 15186 / 48.838125

// const Mode VGA::videomodes[3][3][2] = {
// 	{ {MODE320x240, MODE360x200}, {MODE320x240, MODE360x200}, {MODE320x240, MODE360x200}},
// 	{ {MODE320x240_50_48, MODE360x200_50_48}, {MODE320x240_50_128, MODE360x200_50_128}, {MODE320x240_50_PENTAGON, MODE360x200_50_PENTAGON}},
// 	{ {MODE320x240_TV_48, MODE360x200_TV_48}, {MODE320x240_TV_128, MODE360x200_TV_128}, {MODE320x240_TV_PENTAGON, MODE360x200_TV_PENTAGON}}
// };				

VGA::VGA(const int i2sIndex) : I2S(i2sIndex) {

}

// bool VGA::init(const Mode &mode, const int *pinMap, const int bitCount, const int clockPin) {
// 	this->mode = mode;
// 	int xres = vidmodes[mode][vmodeproperties::hRes];
// 	int yres = mode.vRes / mode.vDiv;
// 	initSyncBits();
// 	propagateResolution(xres, yres);
// 	allocateLineBuffers();
// 	currentLine = 0;
// 	vSyncPassed = false;
// 	initParallelOutputMode(pinMap, mode, bitCount, clockPin);

// 	startTX();
// 	return true;
// }

bool VGA::init(int mode, const int *pinMap, const int bitCount, const int clockPin) {
	this->mode = mode;
	int xres = vidmodes[mode][vmodeproperties::hRes];
	int yres = vidmodes[mode][vmodeproperties::vRes] / vidmodes[mode][vmodeproperties::vDiv];
	initSyncBits();
	propagateResolution(xres, yres);
	allocateLineBuffers();
	currentLine = 0;
	vSyncPassed = false;
	initParallelOutputMode(pinMap, mode, bitCount, clockPin);

	startTX();
	return true;
}

void VGA::allocateLineBuffers() {} 

// complete ringbuffer from frame
void VGA::allocateLineBuffers(void **frameBuffer) {

	// dmaBufferDescriptorCount = mode.linesPerField() * 2;
	dmaBufferDescriptorCount = (vidmodes[mode][vmodeproperties::vFront] + vidmodes[mode][vmodeproperties::vSync] + vidmodes[mode][vmodeproperties::vBack] + vidmodes[mode][vmodeproperties::vRes]) * 2;

	int inactiveSamples = (vidmodes[mode][vmodeproperties::hFront] + vidmodes[mode][vmodeproperties::hSync] + vidmodes[mode][vmodeproperties::hBack] + 3) & 0xfffffffc;
	vSyncInactiveBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples, true);
	vSyncActiveBuffer = DMABufferDescriptor::allocateBuffer(vidmodes[mode][vmodeproperties::hRes], true);
	inactiveBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples, true);
	blankActiveBuffer = DMABufferDescriptor::allocateBuffer(vidmodes[mode][vmodeproperties::hRes], true);

	for (int i = 0; i < inactiveSamples; i++)
	{
		if (i >= (vidmodes[mode][vmodeproperties::hFront] - CenterH) && i < (vidmodes[mode][vmodeproperties::hFront] - CenterH + vidmodes[mode][vmodeproperties::hSync]))
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
	for (int i = 0; i < vidmodes[mode][vmodeproperties::hRes]; i++)
	{
		((unsigned char *)vSyncActiveBuffer)[i ^ 2] = hsyncBitI | vsyncBit;
		((unsigned char *)blankActiveBuffer)[i ^ 2] = hsyncBitI | vsyncBitI;
	}

	dmaBufferDescriptors = DMABufferDescriptor::allocateDescriptors(dmaBufferDescriptorCount);
	for (int i = 0; i < dmaBufferDescriptorCount; i++)
		dmaBufferDescriptors[i].next(dmaBufferDescriptors[(i + 1) % dmaBufferDescriptorCount]);
	int d = 0;
	for (int i = 0; i < (vidmodes[mode][vmodeproperties::vFront] - CenterV); i++)
	{
		dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
		dmaBufferDescriptors[d++].setBuffer(blankActiveBuffer, vidmodes[mode][vmodeproperties::hRes]);
	}
	for (int i = 0; i < vidmodes[mode][vmodeproperties::vSync]; i++)
	{
		dmaBufferDescriptors[d++].setBuffer(vSyncInactiveBuffer, inactiveSamples);
		dmaBufferDescriptors[d++].setBuffer(vSyncActiveBuffer, vidmodes[mode][vmodeproperties::hRes]);
	}
	for (int i = 0; i < (vidmodes[mode][vmodeproperties::vBack] + CenterV); i++)
	{
		dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
		dmaBufferDescriptors[d++].setBuffer(blankActiveBuffer, vidmodes[mode][vmodeproperties::hRes]);
	}
	for (int i = 0; i < vidmodes[mode][vmodeproperties::vRes]; i++)
	{
		dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
		dmaBufferDescriptors[d++].setBuffer(frameBuffer[i / vidmodes[mode][vmodeproperties::vDiv]], vidmodes[mode][vmodeproperties::hRes]);
	}
	
	// printf("buffer descriptors count: %d\n",d);

}
