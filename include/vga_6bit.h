#ifndef VGA_6BIT_H_FILE
#define VGA_6BIT_H_FILE

#include <cstdint>

//hfront hsync hback pixels vfront vsync vback lines divy pixelclock hpolaritynegative vpolaritynegative

const int VgaMode_vga_mode_200x150[12]={
 6,18,32,200, 1,2,22,600,4,9000000, 0,0
};

const int VgaMode_vga_mode_320x200[12]={
 8,48,24,320,12,2,35,400,2,12587500,1,0
};

// ModeLine "320x240" 12.60 320 328 376 400 240 245 246 262 -HSync -VSync DoubleScan
const int VgaMode_vga_mode_320x240[12]={ 
 8,48,24,320,11,2,31,480,2,12587500,1,1
};

const int VgaMode_vga_mode_360x200[12]={ 
 8,54,28,360,11,2,32,400,2,14161000,1,0
};

const int VgaMode_vga_mode_400x300[12]={
 12,36,64,400,1,2,22,600,2,18000000,0,0
};

void vga_init(const unsigned char *pin_map, const int *mode, bool double_buffered);

void vga_swap_buffers(bool wait_vsync = true);
void vga_clear_screen(uint8_t color);
unsigned char **vga_get_framebuffer();
unsigned char vga_get_sync_bits();
int vga_get_xres();
int vga_get_yres();

unsigned char vga_get_vsync_inv_bit(void);
unsigned char vga_get_hsync_inv_bit(void);

void SetVideoInterrupt(unsigned char auxState);

#endif
