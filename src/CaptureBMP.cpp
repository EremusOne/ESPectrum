/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023, 2024 Víctor Iborra [Eremus] and 2023 David Crespo [dcrespo3d]
https://github.com/EremusOne/ZX-ESPectrum-IDF

Based on ZX-ESPectrum-Wiimote
Copyright (c) 2020, 2022 David Crespo [dcrespo3d]
https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote

Based on previous work by Ramón Martinez and Jorge Fuertes
https://github.com/rampa069/ZX-ESPectrum

Original project by Pete Todd
https://github.com/retrogubbins/paseVGA

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

To Contact the dev team you can write to zxespectrum@gmail.com or 
visit https://zxespectrum.speccy.org/contacto

*/

#include "stdio.h"
#include "CaptureBMP.h"
#include "Video.h"
#include "FileUtils.h"
#include "messages.h"
#include "Config.h"
#include "OSDMain.h"
#include "esp_vfs.h"

void CaptureToBmp()
{

    if (!FileUtils::isSDReady()) return;

    char filename[] = "ESP00000.bmp";

    unsigned char bmp_header2[BMP_HEADER2_SIZE] = { 
        0xaa,0xaa,0xaa,0xaa,0xbb,
        0xbb,0xbb,0xbb,0x01,0x00,
        0x08,0x00,0x00,0x00,0x00,
        0x00,0xcc,0xcc,0xcc,0xcc
    };

    // framebuffer size
    int w = VIDEO::vga.xres;
    int h = OSD::scrH;

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

    // Create dir if it doesn't exist
    struct stat stat_buf;
    if (stat(scrdir.c_str(), &stat_buf) != 0) {
        if (mkdir(scrdir.c_str(),0775) != 0) {
            delete[] linebuf;
            printf("Capture BMP: problem creating capture dir\n");
            return;
        }
    }
    
    DIR* dir = opendir(scrdir.c_str());
    if (dir == NULL) {
        delete[] linebuf;
        printf("Capture BMP: problem accessing capture dir\n");
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
    FILE* pf = fopen(fullfn.c_str(), "wb");
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
        uint32_t* src = (uint32_t*)VIDEO::vga.frameBuffer[y];
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
