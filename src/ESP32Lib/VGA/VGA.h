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
	hFront,
	hSync,
	hBack,
	hRes,
	vFront,
	vSync,
	vBack,
	vRes,
	vDiv,
	hSyncPolarity,
	vSyncPolarity,
	r1sdm0,
	r1sdm1,
	r1sdm2,
	r1odiv,
	r0sdm2,
	r0odiv
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VGA STANDARD MODES (IN STANDARD MODE TIMING DEPENDS ON PWM_AUDIO SO ALL MACHINES SHARE SAME VIDEO MODES)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define VgaMode_320x240 { 8, 48, 24, 320, 10, 2, 33, 480, 2, 1, 1, 47,84,7,7,6,6 } // 31469 / 59.94 INDUSTRY STANDARD
#define VgaMode_320x240_scanlines { 8, 48, 24, 320, 10, 2, 33, 480, 1, 1, 1, 47,84,7,7,6,6 } // 31469 / 59.94 INDUSTRY STANDARD
#define VgaMode_360x200 { 9, 54, 27, 360, 12, 2, 35, 400, 2, 1, 0, 48,84,7,6,6,5 } // 31469 / 70.087 TAKEN FROM SAMSUNG S22C450 MANUAL
#define VgaMode_360x200_scanlines { 9, 54, 27, 360, 12, 2, 35, 400, 1, 1, 0, 48,84,7,6,6,5 } // 31469 / 70.087 TAKEN FROM SAMSUNG S22C450 MANUAL

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VGA MODES WITH SAME FREQUENCY AS REAL MACHINES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 48K 50hz VGA, SYNC @ 19968 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
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

// 128K 50hz VGA, SYNC @ 19992 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
#define VgaMode_320x240_50_128 { 8, 42, 50, 320, 27, 6, 53, 720, 3, 1, 1, 224,40,6,4,8,5 } // 40316 / 50.020008
#define VgaMode_320x240_50_128_scanlines { 8, 32, 40, 320, 3, 10, 16, 480, 1, 1, 0, 105,42,5,7,5,7 } // 25460 / 50.020008
#define VgaMode_360x200_50_128 { 18, 36, 54, 360, 31, 3, 33, 600, 3, 1, 1, 194,125,8,6,7,5 } // 33363 / 50.020008
#define VgaMode_360x200_50_128_scanlines { 8, 32, 40, 360, 3, 10, 6, 400, 1, 1, 0, 231,16,7,10,8,11 } // 20958 / 50.020008

// PENTAGON 128K 50hz VGA, SYNC @ 20480 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
#define VgaMode_320x240_50_PENTAGON { 8, 42, 50, 320, 27, 6, 53, 720, 3, 1, 1, 13,146,7,5,6,4 } // 39355 / 48.838125
#define VgaMode_320x240_50_PENTAGON_scanlines { 8, 32, 40, 320, 3, 10, 16, 480, 1, 1, 0, 160,238,7,10,5,7 } // 24859 / 48.838125
#define VgaMode_360x200_50_PENTAGON { 18, 36, 54, 360, 31, 3, 33, 600, 3, 1, 1, 143,49,8,6,5,4 } // 32568 / 48.838125
#define VgaMode_360x200_50_PENTAGON_scanlines { 8, 32, 40, 360, 3, 10, 6, 400, 1, 1, 0, 120,231,5,9,5,8 } // 20463 / 48.838125

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TV MODES
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// const Mode VGA::MODE320x240_TV_48(38, 32, 58, 320, 28, 3, 41, 240, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.0801282
// const Mode VGA::MODE360x270_TV_48(18, 32, 38, 360, 13, 3, 26, 270, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.0801282
// const Mode VGA::MODE320x240_TV_128(42, 32, 62, 320, 28, 3, 40, 240, 1, 7093637, 1, 1, 245,163,6,13,0,0,8,15);  // 15556 / 50.020008
// const Mode VGA::MODE360x270_TV_128(22, 32, 42, 360, 13, 3, 25, 270, 1, 7093637, 1, 1, 245,163,6,13,0,0,8,15);  // 15556 / 50.020008

// const Mode VGA::MODE320x240_TV_48(18, 32, 38, 360, 13, 3, 26, 270, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.0801282
// const Mode VGA::MODE320x240_TV_48(22, 32, 42, 352, 16, 3, 29, 264, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.0801282

// TRAIDOS DE LA RAMA OVERSCAN
// const Mode VGA::MODE320x240_TV_48(22, 32, 42, 352, 14, 3, 31, 264, 1, 6999999, 1, 1, 0,128,6,13,0,0,8,15);  // 15625 / 50.08
// const Mode VGA::MODE320x240_TV_128(26, 32, 46, 352, 14, 3, 30, 264, 1, 7093637, 1, 1, 92,238,5,12,0,0,8,15);  // 15556 / 50.02

// 48K

// ¡PENDING FINE TUNE SYNC TESTS! -> 48K 50hz CRT, SYNC @ 19968 micros, DAPR AUDIO LAG TEST -> CLEAN TONE

#define VgaMode_320x240_TV_48 { 42, 32, 62, 320, 28, 3, 40, 240, 1, 1, 1, 59,167,6,13,6,12 } // 15575 / 50.0801282
#define VgaMode_360x200_TV_48 { 22, 32, 42, 360, 48, 3, 60, 200, 1, 1, 1, 59,167,6,13,6,12 } // 15575 / 50.0801282

// 128K

// ¡PENDING FINE TUNE SYNC TESTS! -> 128K 50hz CRT, SYNC @ 19992 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
#define VgaMode_320x240_TV_128 { 42, 32, 62, 320, 28, 3, 40, 240, 1, 1, 1, 245,163,6,13,8,15 } // 15556 / 50.020008
#define VgaMode_360x200_TV_128 { 22, 32, 42, 360, 48, 3, 60, 200, 1, 1, 1, 245,163,6,13,8,15 } // 15556 / 50.020008

// PENTAGON 128K

// ¡PENDING FINE TUNE SYNC TESTS! -> PENTAGON 128K 50hz CRT, SYNC @ 20480 micros, DAPR AUDIO LAG TEST -> CLEAN TONE
#define VgaMode_320x240_TV_PENTAGON { 42, 32, 62, 320, 28, 3, 40, 240, 1, 1, 1, 151,197,7,15,5,11 } // 15186 / 48.838125
#define VgaMode_360x200_TV_PENTAGON { 22, 32, 42, 360, 48, 3, 60, 200, 1, 1, 1, 151,197,7,15,5,11 } // 15186 / 48.838125

// OVERSCAN CRT MODES / FOR FUTURE USE
#define CrtMode_352x272_TV_48 { 22, 32, 42, 352, 18, 3, 19, 272, 1, 1, 1, 0,128,6,13,8,15 }  // 7 MHZ / 15625 / 50.0801282
// #define CrtMode_352x264_TV_48 { 22, 32, 42, 352, 22, 3, 23, 264, 1, 1, 1, 0,128,6,13,8,15 }  // 7 MHZ / 15625 / 50.0801282 / 312 VLINES (PAL)

#define CrtMode_352x272_TV_TK50 { 22, 32, 42, 352, 18, 3, 19, 272, 1, 1, 1, 148,241,7,15,8,15 } // 15682 / 50.2638854 

#define CrtMode_352x224_TV_TK60 { 22, 32, 42, 352, 18, 3, 19, 224, 1, 1, 1, 244,8,8,15,8,15 } // 15802 / 59.856887 / 
// #define CrtMode_352x220_TV_TK60 { 22, 32, 42, 352, 19, 3, 21, 220, 1, 1, 1, 25,43,5,11,8,15 } // 15742 / 59.856887 / 263 VLINES (NTSC)

#define CrtMode_352x272_TV_128 { 22, 32, 42, 352, 18, 3, 19, 272, 1, 1, 1, 194,47,7,14,8,15 } // 15606 / 50.020008

#define CrtMode_352x272_TV_PENTAGON { 22, 32, 42, 352, 18, 3, 19, 272, 1, 1, 1, 217,154,7,15,7,14 } // 15237 / 48.838125

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const unsigned short int vidmodes[29][17]={ 
	VgaMode_320x240, VgaMode_320x240_scanlines, VgaMode_360x200, VgaMode_360x200_scanlines,

	VgaMode_320x240_50_48, VgaMode_320x240_50_48_scanlines, VgaMode_360x200_50_48,  VgaMode_360x200_50_48_scanlines,
	VgaMode_320x240_50_TK, VgaMode_320x240_50_TK_scanlines, VgaMode_360x200_50_TK,  VgaMode_360x200_50_TK_scanlines,
	VgaMode_320x240_60_TK, VgaMode_320x240_60_TK_scanlines, VgaMode_360x200_60_TK,  VgaMode_360x200_60_TK_scanlines,
	VgaMode_320x240_50_128, VgaMode_320x240_50_128_scanlines, VgaMode_360x200_50_128, VgaMode_360x200_50_128_scanlines,
	VgaMode_320x240_50_PENTAGON , VgaMode_320x240_50_PENTAGON_scanlines, VgaMode_360x200_50_PENTAGON, VgaMode_360x200_50_PENTAGON_scanlines,

	// Overscan
	CrtMode_352x272_TV_48 , 
	CrtMode_352x272_TV_TK50 , 
	CrtMode_352x224_TV_TK60 , 
	CrtMode_352x272_TV_128, 
	CrtMode_352x272_TV_PENTAGON
	
	// VgaMode_320x240_TV_48, VgaMode_360x200_TV_48, 
	// VgaMode_320x240_TV_128, VgaMode_360x200_TV_128, 
	// VgaMode_320x240_TV_PENTAGON , VgaMode_360x200_TV_PENTAGON		

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
