///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
//
// BMP CAPTURE
//
// Copyright (c) 2023 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d
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

#include "stdio.h"
#include "CaptureBMP.h"
#include "Video.h"
#include "FileUtils.h"
#include "messages.h"
#include "config.h"
#include "esp_vfs.h"

void CaptureToBmp()
{

    char filename[] = "ESP00000.bmp";

    unsigned char bmp_header2[BMP_HEADER2_SIZE] = { 
        0xaa,0xaa,0xaa,0xaa,0xbb,
        0xbb,0xbb,0xbb,0x01,0x00,
        0x08,0x00,0x00,0x00,0x00,
        0x00,0xcc,0xcc,0xcc,0xcc
    };

    // framebuffer size
    int w = VIDEO::vga.xres;
    int h = VIDEO::vga.yres;

    // number of uint32_t words
    int count = w >> 2;

    // allocate line  buffer
    uint32_t *linebuf = new uint32_t[count];
    if (NULL == linebuf) {
        printf("Capture BMP: unable to allocate line buffer\n");
        return;
    }

    // Get next filename (BMP + 5 digits, 0 padded sequential)
    string filelist;
    string scrdir = (string) MOUNT_POINT_SD + DISK_SCR_DIR;

    DIR* dir = opendir(scrdir.c_str());
    if (dir == NULL) {
        printf("Capture BMP: problem accessing SCR dir\n");
        return;
    }
    int bmpnumber = 0;
    struct dirent* de = readdir(dir);
    if (de) {
        while (true) {
            string fname = de->d_name;
            if ((fname.substr(0,3) == "ESP") && (fname.substr(8,4) == ".bmp")) {
                int fnum = stoi(fname.substr(3,5));
                if (fnum > bmpnumber) bmpnumber = fnum;
            }
            de = readdir(dir);
            if (!de) break;
        }
    }
    closedir(dir);

    bmpnumber++;

    if (Config::slog_on) printf("BMP number -> %.5d\n",bmpnumber);

    sprintf((char *)filename,"ESP%.5d.bmp",bmpnumber);    
        
    // Full filename. Save only to SD.
    std::string fullfn = (string) MOUNT_POINT_SD + DISK_SCR_DIR + "/" + filename;

    // open file for writing
    FILE* pf = fopen(fullfn.c_str(), "w");
    if (NULL == pf) {
        delete[] linebuf;
        printf("Capture BMP: unable to open file %s for writing\n", fullfn.c_str());
        return;
    }

    // printf("CaptureBMP: capturing %d x %d to %s...\n", w, h, fullfn.c_str());

    // put width, height and size values in header
    uint32_t* biWidth     = (uint32_t*)(&bmp_header2[0]);
    uint32_t* biHeight    = (uint32_t*)(&bmp_header2[4]);
    uint32_t* biSizeImage = (uint32_t*)(&bmp_header2[16]);
    *biWidth = w;
    *biHeight = h;
    *biSizeImage = w * h;

    // write header 1
    fwrite(bmp_header1, BMP_HEADER1_SIZE, 1, pf);

    // write header 2
    fwrite(bmp_header2, BMP_HEADER2_SIZE, 1, pf);

    // write header 3
    fwrite(bmp_header3, BMP_HEADER3_SIZE, 1, pf);

    // process every scanline in reverse order (BMP is topdown)
    for (int y = h - 1; y >= 0; y--) {
        uint32_t* src = (uint32_t*)VIDEO::vga.backBuffer[y];
        uint32_t* dst = linebuf;
        // process every uint32 in scanline
        for (int i = 0; i < count; i++) {
            uint32_t srcval = *src++;
            uint32_t dstval = 0;
            // swap every uint32
            dstval |= ((srcval & 0xFFFF0000) >> 16);
            dstval |= ((srcval & 0x0000FFFF) << 16);
            *dst++ = dstval;
        }
        // write line to file
        fwrite(linebuf, sizeof(uint32_t), count, pf);
    }

    // cleanup
    fclose(pf);

    delete[] linebuf;

    // printf("Capture BMP: done\n");
}
