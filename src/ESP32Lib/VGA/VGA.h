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

#include "../I2S/I2S.h"

enum vmodeproperties {
	hFront,	hSync, hBack, hRes,	vFront,	vSync, vBack, vRes,	vDiv, hSyncPolarity, vSyncPolarity,	r1sdm0,	r1sdm1,	r1sdm2,	r1odiv,	r0sdm2,	r0odiv
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VGA STANDARD MODES (IN STANDARD MODE TIMING DEPENDS ON PWM_AUDIO SO ALL MACHINES SHARE SAME VIDEO MODES)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define VgaMode_320x240 { 8, 48, 24, 320, 10, 2, 33, 480, 2, 1, 1, 47,84,7,7,6,6 } // 31469 / 59.94
#define VgaMode_320x240_scanlines { 8, 48, 24, 320, 10, 2, 33, 480, 1, 1, 1, 47,84,7,7,6,6 } // Same as above but without double scanlines

#define VgaMode_360x200 { 9, 54, 27, 360, 13, 2, 34, 400, 2, 1, 0, 249,83,7,6,6,5 } // 31466.7 / 70.082
#define VgaMode_360x200_scanlines { 9, 54, 27, 360, 13, 2, 34, 400, 1, 1, 0, 249,83,7,6,6,5 } // Same as above but without double scanlines

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VGA MODES WITH SAME FREQUENCY AS REAL MACHINES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 48K 50hz VGA
#define VgaMode_320x240_50_48 { 8, 42, 50, 320, 27, 6, 53, 720, 3, 1, 1, 0,44,6,4,8,5 } // 40365 / 50.0801282
#define VgaMode_320x240_50_48_scanlines { 8, 32, 40, 320, 3, 10, 16, 480, 1, 1, 0, 59,45,5,7,5,7 } // 25491 / 50.0801282
#define VgaMode_360x200_50_48 { 18, 36, 54, 360, 31, 3, 33, 600, 3, 1, 1, 51,97,5,4,7,5 } // 33403 / 50.0801282
#define VgaMode_360x200_50_48_scanlines { 8, 32, 40, 360, 3, 10, 6, 400, 1, 1, 0, 151,59,5,8,8,11 } // 20984 / 50.0801282

// TK 50hz
#define VgaMode_320x240_50_TK { 8, 42, 50, 320, 27, 6, 53, 720, 3, 1, 1, 142, 53, 6, 4, 8, 5 } // 40513 / 50.2638854
#define VgaMode_320x240_50_TK_scanlines { 8, 32, 40, 320, 3, 10, 16, 480, 1, 1, 0, 205,71,8,10,5,7 }  // 25584 / 50.2638854
#define VgaMode_360x200_50_TK { 18, 36, 54, 360, 31, 3, 33, 600, 3, 1, 1, 174,251,6,5,7,5 } // 33526 / 50.2638854
#define VgaMode_360x200_50_TK_scanlines { 9, 54, 27, 360, 35, 12, 2, 400, 1, 1, 0, 222,47,8,10,5,7 } // 22568 / 50.2638854

// TK 60hz
#define VgaMode_320x240_60_TK { 8, 48, 24, 320, 10, 2, 33, 480, 2, 1, 1, 83,14,6,6,6,6 }  // 31425 / 59.856887
#define VgaMode_320x240_60_TK_scanlines { 8, 48, 24, 320, 10, 2, 33, 480, 1, 1, 1, 83,14,6,6,6,6 }  // 31425 / 59.856887
#define VgaMode_360x200_60_TK { 18, 36, 54, 360, 31, 3, 33, 600, 3, 1, 1, 164,87,5,3,7,4 } // 39925 / 59.856887
#define VgaMode_360x200_60_TK_scanlines { 9, 54, 27, 360, 35, 12, 2, 400, 1, 1, 0, 22,24, 8,8,8,8 } // 26876 / 59.856887

// 128K 50hz VGA
#define VgaMode_320x240_50_128 { 8, 42, 50, 320, 27, 6, 53, 720, 3, 1, 1, 224,40,6,4,8,5 } // 40316 / 50.020008
#define VgaMode_320x240_50_128_scanlines { 8, 32, 40, 320, 3, 10, 16, 480, 1, 1, 0, 105,42,5,7,5,7 } // 25460 / 50.020008
#define VgaMode_360x200_50_128 { 18, 36, 54, 360, 31, 3, 33, 600, 3, 1, 1, 194,125,8,6,7,5 } // 33363 / 50.020008
#define VgaMode_360x200_50_128_scanlines { 8, 32, 40, 360, 3, 10, 6, 400, 1, 1, 0, 231,16,7,10,8,11 } // 20958 / 50.020008

// PENTAGON 128K 50hz VGA
#define VgaMode_320x240_50_PENTAGON { 16, 32, 48, 320, 1, 3, 17, 720, 3, 1, 0, 143, 10, 8, 6, 5, 4 } // 36182 / 48.828125 / MSV: 1792
#define VgaMode_320x240_50_PENTAGON_scanlines { 16, 32, 48, 320, 1, 3, 17, 480, 1, 1, 0, 174, 40, 5, 7, 5, 7 } // 24463 / 48.828125
#define VgaMode_360x200_50_PENTAGON {  8, 40, 48, 360, 1, 3, 14, 600, 3, 1, 0,  87, 98, 8, 7, 7, 6 } // 30176 / 48.828125 / MSV: 1792
#define VgaMode_360x200_50_PENTAGON_scanlines {  8, 40, 48, 360, 1, 3, 14, 400, 1, 1, 0,  220, 60, 6, 9, 8, 11 } // 20410 / 48.828125

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TV MODES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define CrtMode_352x272_TV_48 { 22, 32, 42, 352, 18, 3, 19, 272, 1, 1, 1, 0,128,6,13,8,15 }  // 7 MHZ / 15625 / 50.0801282
#define CrtMode_352x272_TV_TK50 { 22, 32, 42, 352, 18, 3, 19, 272, 1, 1, 1, 148,241,7,15,8,15 } // 15682 / 50.2638854
#define CrtMode_352x224_TV_TK60 { 22, 32, 42, 352, 18, 3, 19, 224, 1, 1, 1, 244,8,8,15,8,15 } // 15802 / 59.856887 /
#define CrtMode_352x272_TV_128 { 22, 32, 42, 352, 18, 3, 19, 272, 1, 1, 1, 194,47,7,14,8,15 } // 15606 / 50.020008
#define CrtMode_352x272_TV_PENTAGON { 22, 32, 42, 352, 18, 3, 19, 272, 1, 1, 1, 133,235,6,14,7,14 } // 15234 / 48.828125

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const unsigned short int vidmodes[29][17]={
	VgaMode_320x240, VgaMode_320x240_scanlines, VgaMode_360x200, VgaMode_360x200_scanlines,
	VgaMode_320x240_50_48, VgaMode_320x240_50_48_scanlines, VgaMode_360x200_50_48,  VgaMode_360x200_50_48_scanlines,
	VgaMode_320x240_50_TK, VgaMode_320x240_50_TK_scanlines, VgaMode_360x200_50_TK,  VgaMode_360x200_50_TK_scanlines,
	VgaMode_320x240_60_TK, VgaMode_320x240_60_TK_scanlines, VgaMode_360x200_60_TK,  VgaMode_360x200_60_TK_scanlines,
	VgaMode_320x240_50_128, VgaMode_320x240_50_128_scanlines, VgaMode_360x200_50_128, VgaMode_360x200_50_128_scanlines,
	VgaMode_320x240_50_PENTAGON , VgaMode_320x240_50_PENTAGON_scanlines, VgaMode_360x200_50_PENTAGON, VgaMode_360x200_50_PENTAGON_scanlines,
	CrtMode_352x272_TV_48 ,	CrtMode_352x272_TV_TK50 , CrtMode_352x224_TV_TK60 ,	CrtMode_352x272_TV_128,	CrtMode_352x272_TV_PENTAGON
};

class VGA : public I2S {

  public:

	VGA(const int i2sIndex = 0);

	bool init(int mode, const int *pinMap, const int bitCount, const int clockPin = -1);

	int mode;

	int CenterH = 0;
	int CenterV = 0;

  protected:

	virtual void initSyncBits() = 0;
	virtual long syncBits(bool h, bool v) = 0;

	long vsyncBit;
	long hsyncBit;
	long vsyncBitI;
	long hsyncBitI;

	virtual void allocateLineBuffers();
	virtual void allocateLineBuffers(void **frameBuffer);
	virtual void propagateResolution(const int xres, const int yres) = 0;

};
