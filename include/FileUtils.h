///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum - ZX Spectrum emulator for ESP32
//
// Copyright (c) 2020, 2021 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez, Jorge Fuertes and many others
// https://github.com/rampa069/ZX-ESPectrum
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

#ifndef FileUtils_h
#define FileUtils_h

#include <stdio.h>
#include <inttypes.h>
#include <string>
#include "sdmmc_cmd.h"

using namespace std;

#include "MemESP.h"

// Defines
#define ASCII_NL 10
#define ON true
#define OFF false

class FileUtils
{
public:
    static void initFileSystem();
    static void unmountSDCard();
    // static String         getAllFilesFrom(const String path);
    // static void           listAllFiles();
    // static void           sanitizeFilename(String filename); // in-place
    // static File IRAM_ATTR safeOpenFileRead(String filename);
    static string getFileEntriesFromDir(string path);
    // static uint16_t       countFileEntriesFromDir(String path);
    static string getSortedFileList(string fileDir);
    static bool hasSNAextension(string filename);
    static bool hasZ80extension(string filename);

    static string MountPoint;

private:
    friend class Config;
    static sdmmc_card_t *card;    
};

#define MOUNT_POINT_SPIFFS "/data"
#define MOUNT_POINT_SD "/sd"

// Use internal spiffs first
#define DISK_BOOT_FILENAME "/data/boot.cfg"
#define DISK_ROM_DIR "/rom"
#define DISK_SNA_DIR "/sna"
#define DISK_TAP_DIR "/tap"
#define DISK_PSNA_FILE "/persist/persist"

#define NO_RAM_FILE "none"
#define SNA_48K_SIZE 49179
#define SNA_128K_SIZE1 131103
#define SNA_128K_SIZE2 147487

// inline utility functions for uniform access to file/memory
// and making it easy to to implement SNA/Z80 functions

static inline uint8_t readByteFile(FILE *f)
{
    uint8_t result;

    if (fread(&result, 1, 1, f) != 1) {
        return -1;
    }

    return result;
}

static inline uint16_t readWordFileLE(FILE *f)
{
    uint8_t lo = readByteFile(f);
    uint8_t hi = readByteFile(f);
    return lo | (hi << 8);
}

static inline uint16_t readWordFileBE(FILE *f)
{
    uint8_t hi = readByteFile(f);
    uint8_t lo = readByteFile(f);
    return lo | (hi << 8);
}

static inline size_t readBlockFile(FILE *f, uint8_t* dstBuffer, size_t size)
{
    return fread(dstBuffer, 0x4000, 1, f);
}

static inline void writeByteFile(uint8_t value, FILE *f)
{
    fwrite(&value,1,1,f);
}

static inline void writeWordFileLE(uint16_t value, FILE *f)
{
    uint8_t lo =  value       & 0xFF;
    uint8_t hi = (value >> 8) & 0xFF;
    fwrite(&lo,1,1,f);
    fwrite(&hi,1,1,f);
}

// static inline void writeWordFileBE(uint16_t value, File f)
// {
//     uint8_t hi = (value >> 8) & 0xFF;
//     uint8_t lo =  value       & 0xFF;
//     f.write(hi);
//     f.write(lo);
// }

// static inline size_t writeBlockFile(uint8_t* srcBuffer, File f, size_t size)
// {
//     return f.write(srcBuffer, size);
// }

// static inline uint8_t readByteMem(uint8_t*& ptr)
// {
//     uint8_t value = *ptr++;
//     return value;
// }

// static inline uint16_t readWordMemLE(uint8_t*& ptr)
// {
//     uint8_t lo = *ptr++;
//     uint8_t hi = *ptr++;
//     return lo | (hi << 8);
// }

// static inline uint16_t readWordMemBE(uint8_t*& ptr)
// {
//     uint8_t hi = *ptr++;
//     uint8_t lo = *ptr++;
//     return lo | (hi << 8);
// }

// static inline size_t readBlockMem(uint8_t*& srcBuffer, uint8_t* dstBuffer, size_t size)
// {
//     memcpy(dstBuffer, srcBuffer, size);
//     srcBuffer += size;
//     return size;
// }

// static inline void writeByteMem(uint8_t value, uint8_t*& ptr)
// {
//     *ptr++ = value;
// }

// static inline void writeWordMemLE(uint16_t value, uint8_t*& ptr)
// {
//     uint8_t lo =  value       & 0xFF;
//     uint8_t hi = (value >> 8) & 0xFF;
//     *ptr++ = lo;
//     *ptr++ = hi;
// }

// static inline void writeWordMemBE(uint16_t value, uint8_t*& ptr)
// {
//     uint8_t hi = (value >> 8) & 0xFF;
//     uint8_t lo =  value       & 0xFF;
//     *ptr++ = hi;
//     *ptr++ = lo;
// }

// static inline size_t writeBlockMem(uint8_t* srcBuffer, uint8_t*& dstBuffer, size_t size)
// {
//     memcpy(dstBuffer, srcBuffer, size);
//     dstBuffer += size;
//     return size;
// }

#endif // FileUtils_h