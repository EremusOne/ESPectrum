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

VGA::VGA(const int i2sIndex) : I2S(i2sIndex) {}

bool VGA::init(int mode, const int *pinMap, const int bitCount, const int clockPin) {
	
	this->mode = mode;
	int xres = vidmodes[mode][vmodeproperties::hRes];
	int yres = vidmodes[mode][vmodeproperties::vRes] / vidmodes[mode][vmodeproperties::vDiv];
	initSyncBits();
	propagateResolution(xres, yres);
	allocateLineBuffers();
	initParallelOutputMode(pinMap, mode, bitCount, clockPin);

	startTX();

	return true;

}

void VGA::allocateLineBuffers() {} 

// ///////////////////////////////////////////////////
// OLD FUNCTION
// ///////////////////////////////////////////////////
// void VGA::allocateLineBuffers(void **frameBuffer) {

// 	dmaBufferDescriptorCount = (vidmodes[mode][vmodeproperties::vFront] + vidmodes[mode][vmodeproperties::vSync] + vidmodes[mode][vmodeproperties::vBack] + vidmodes[mode][vmodeproperties::vRes]) * 2;

// 	int inactiveSamples = (vidmodes[mode][vmodeproperties::hFront] + vidmodes[mode][vmodeproperties::hSync] + vidmodes[mode][vmodeproperties::hBack] + 3) & 0xfffffffc;
// 	void *vSyncInactiveBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples, true);
// 	void *vSyncActiveBuffer = DMABufferDescriptor::allocateBuffer(vidmodes[mode][vmodeproperties::hRes], true);
// 	void *inactiveBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples, true);
// 	void *blankActiveBuffer = DMABufferDescriptor::allocateBuffer(vidmodes[mode][vmodeproperties::hRes], true);

// 	for (int i = 0; i < inactiveSamples; i++) {
// 		if (i >= (vidmodes[mode][vmodeproperties::hFront] - CenterH) && i < (vidmodes[mode][vmodeproperties::hFront] - CenterH + vidmodes[mode][vmodeproperties::hSync])) {
// 			((unsigned char *)vSyncInactiveBuffer)[i ^ 2] = hsyncBit | vsyncBit;
// 			((unsigned char *)inactiveBuffer)[i ^ 2] = hsyncBit | vsyncBitI;
// 		} else {
// 			((unsigned char *)vSyncInactiveBuffer)[i ^ 2] = hsyncBitI | vsyncBit;
// 			((unsigned char *)inactiveBuffer)[i ^ 2] = hsyncBitI | vsyncBitI;
// 		}
// 	}

// 	for (int i = 0; i < vidmodes[mode][vmodeproperties::hRes]; i++) {
// 		((unsigned char *)vSyncActiveBuffer)[i ^ 2] = hsyncBitI | vsyncBit;
// 		((unsigned char *)blankActiveBuffer)[i ^ 2] = hsyncBitI | vsyncBitI;
// 	}

// 	dmaBufferDescriptors = DMABufferDescriptor::allocateDescriptors(dmaBufferDescriptorCount);
// 	for (int i = 0; i < dmaBufferDescriptorCount; i++)
// 		dmaBufferDescriptors[i].next(dmaBufferDescriptors[(i + 1) % dmaBufferDescriptorCount]);

// 	int d = 0;
// 	for (int i = 0; i < (vidmodes[mode][vmodeproperties::vFront] - CenterV); i++) {
// 		dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
// 		dmaBufferDescriptors[d++].setBuffer(blankActiveBuffer, vidmodes[mode][vmodeproperties::hRes]);
// 	}
// 	for (int i = 0; i < vidmodes[mode][vmodeproperties::vSync]; i++) {
// 		dmaBufferDescriptors[d++].setBuffer(vSyncInactiveBuffer, inactiveSamples);
// 		dmaBufferDescriptors[d++].setBuffer(vSyncActiveBuffer, vidmodes[mode][vmodeproperties::hRes]);
// 	}
// 	for (int i = 0; i < (vidmodes[mode][vmodeproperties::vBack] + CenterV); i++) {
// 		dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
// 		dmaBufferDescriptors[d++].setBuffer(blankActiveBuffer, vidmodes[mode][vmodeproperties::hRes]);
// 	}

// 	if ((vidmodes[mode][vmodeproperties::vRes] == 480 || vidmodes[mode][vmodeproperties::vRes] == 400) && vidmodes[mode][vmodeproperties::vDiv] == 1) { // 480 and 400 lines modes for scanline simulation
// 		for (int i = 0; i < vidmodes[mode][vmodeproperties::vRes]; i++) {
// 			dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
// 			// We use black framebuffer in odd lines
// 			dmaBufferDescriptors[d++].setBuffer(frameBuffer[i & 0x01 ? vidmodes[mode][vmodeproperties::vRes] >> 1 : i >> 1], vidmodes[mode][vmodeproperties::hRes]);
// 		}
// 	} else {
// 		for (int i = 0; i < vidmodes[mode][vmodeproperties::vRes]; i++) {
// 			dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
// 			dmaBufferDescriptors[d++].setBuffer(frameBuffer[i / vidmodes[mode][vmodeproperties::vDiv]], vidmodes[mode][vmodeproperties::hRes]);
// 		}
// 	}

// 	// printf("buffer descriptors count: %d\n",d);

// }

void VGA::allocateLineBuffers(void **frameBuffer) {

	int dmaBufferDescCount = (vidmodes[mode][vmodeproperties::vRes] << 1) + vidmodes[mode][vmodeproperties::vFront] + vidmodes[mode][vmodeproperties::vBack] + vidmodes[mode][vmodeproperties::vSync];

	int inactiveSamples = (vidmodes[mode][vmodeproperties::hFront] + vidmodes[mode][vmodeproperties::hSync] + vidmodes[mode][vmodeproperties::hBack] + 3) & 0xfffffffc;
	void *inactiveBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples, true);
	void *vSyncBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples + vidmodes[mode][vmodeproperties::hRes], true);
	void *blankBuffer = DMABufferDescriptor::allocateBuffer(inactiveSamples + vidmodes[mode][vmodeproperties::hRes], true);	

	for (int i = 0; i < inactiveSamples; i++) {
		if (i >= (vidmodes[mode][vmodeproperties::hFront] - CenterH) && i < (vidmodes[mode][vmodeproperties::hFront] - CenterH + vidmodes[mode][vmodeproperties::hSync])) {
			((unsigned char *)inactiveBuffer)[i ^ 2] = hsyncBit | vsyncBitI;
		} else {
			((unsigned char *)inactiveBuffer)[i ^ 2] = hsyncBitI | vsyncBitI;
		}
	}

	for (int i = 0; i < inactiveSamples; i++) {
		if (i >= (vidmodes[mode][vmodeproperties::hFront] - CenterH) && i < (vidmodes[mode][vmodeproperties::hFront] - CenterH + vidmodes[mode][vmodeproperties::hSync])) {
			((unsigned char *)vSyncBuffer)[i ^ 2] = hsyncBit | vsyncBit;
			((unsigned char *)blankBuffer)[i ^ 2] = hsyncBit | vsyncBitI;			
		} else {
			((unsigned char *)vSyncBuffer)[i ^ 2] = hsyncBitI | vsyncBit;
			((unsigned char *)blankBuffer)[i ^ 2] = hsyncBitI | vsyncBitI;			
		}
	}
	for (int i = inactiveSamples; i < (inactiveSamples + vidmodes[mode][vmodeproperties::hRes]); i++)	{
		((unsigned char *)vSyncBuffer)[i ^ 2] = hsyncBitI | vsyncBit;
		((unsigned char *)blankBuffer)[i ^ 2] = hsyncBitI | vsyncBitI;		
	}

	// Allocate descriptors for all scanlines and populate .next value
	dmaBufferDescriptors = DMABufferDescriptor::allocateDescriptors(dmaBufferDescCount);
	for (int i = 0; i < dmaBufferDescCount; i++)
		dmaBufferDescriptors[i].next(dmaBufferDescriptors[(i + 1) % dmaBufferDescCount]);

	// Assign scanlines buffers to descriptors
	int d = 0;
	for (int i = 0; i < (vidmodes[mode][vmodeproperties::vFront] - CenterV); i++) {
		dmaBufferDescriptors[d++].setBuffer(blankBuffer, inactiveSamples + vidmodes[mode][vmodeproperties::hRes]);
	}

	for (int i = 0; i < vidmodes[mode][vmodeproperties::vSync]; i++) {
		dmaBufferDescriptors[d++].setBuffer(vSyncBuffer, inactiveSamples + vidmodes[mode][vmodeproperties::hRes]);
	}

	for (int i = 0; i < (vidmodes[mode][vmodeproperties::vBack] + CenterV); i++) {
		dmaBufferDescriptors[d++].setBuffer(blankBuffer, inactiveSamples + vidmodes[mode][vmodeproperties::hRes]);
	}

	if ((vidmodes[mode][vmodeproperties::vRes] == 480 || vidmodes[mode][vmodeproperties::vRes] == 400) && vidmodes[mode][vmodeproperties::vDiv] == 1) { // 480 and 400 lines modes for scanline simulation
		for (int i = 0; i < vidmodes[mode][vmodeproperties::vRes]; i++) {
			dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
			// We use black framebuffer in odd lines
			dmaBufferDescriptors[d++].setBuffer(frameBuffer[i & 0x01 ? vidmodes[mode][vmodeproperties::vRes] >> 1 : i >> 1], vidmodes[mode][vmodeproperties::hRes]);
		}
	} else {
		for (int i = 0; i < vidmodes[mode][vmodeproperties::vRes]; i++) {
			dmaBufferDescriptors[d++].setBuffer(inactiveBuffer, inactiveSamples);
			dmaBufferDescriptors[d++].setBuffer(frameBuffer[i / vidmodes[mode][vmodeproperties::vDiv]], vidmodes[mode][vmodeproperties::hRes]);
		}
	}

}
