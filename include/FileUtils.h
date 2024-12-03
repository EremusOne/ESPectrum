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

#ifndef FileUtils_h
#define FileUtils_h

#include <stdio.h>
#include <inttypes.h>
#include <string>
#include <vector>
#include "sdmmc_cmd.h"

using namespace std;

#include "MemESP.h"

// Defines
#define ASCII_NL 10

#define DISK_SNAFILE 0
#define DISK_TAPFILE 1
#define DISK_DSKFILE 2
#define DISK_ROMFILE 3
#define DISK_ESPFILE 4

struct DISK_FTYPE {
    string fileExts;
    string indexFilename;
    int begin_row;
    int focus;
    uint8_t fdMode;
    string fileSearch;
};

class FileUtils
{
public:
    static string getLCaseExt(const string& filename);

    static size_t fileSize(const char * mFile);

    static void initFileSystem();
    static bool mountSDCard(int PIN_MISO, int PIN_MOSI, int PIN_CLK, int PIN_CS);
    static void unmountSDCard();

    static bool isMountedSDCard();
    static bool isSDReady();

    // static String         getAllFilesFrom(const String path);
    // static void           listAllFiles();
    // static void           sanitizeFilename(String filename); // in-place
    // static File           safeOpenFileRead(String filename);
    // static string getFileEntriesFromDir(string path);
    static int getDirStats(const string& filedir, const vector<string>& filexts, unsigned long* hash, unsigned int* elements, unsigned int* ndirs);
    static void DirToFile(string Dir, uint8_t ftype /*string fileExts*/, unsigned long hash, unsigned int item_count);
//    static void Mergefiles(string fpath, uint8_t ftype, int chunk_cnt);
    // static uint16_t       countFileEntriesFromDir(String path);
    // static string getSortedFileList(string fileDir);
    static bool hasExtension(string filename, string extension);
    static bool hasSNAextension(string filename);
    static bool hasZ80extension(string filename);
    static bool hasPextension(string filename);
    static bool hasTAPextension(string filename);
    static bool hasTZXextension(string filename);

    static void deleteFilesWithExtension(const char *folder_path, const char *extension);

    static string getResolvedPath(const string& path);
    static string createTmpDir();

    static string MountPoint;
    static bool SDReady;

    static string SNA_Path; // Current SNA path on the SD
    static string TAP_Path; // Current TAP path on the SD
    static string DSK_Path; // Current DSK path on the SD
    static string ROM_Path; // Current ROM path on the SD
    static string ESP_Path; // Current ROM path on the SD

    static DISK_FTYPE fileTypes[5];

private:
    friend class Config;
    static sdmmc_card_t *card;
};

#define MOUNT_POINT_SD "/sd"
#define DISK_SCR_DIR "/.c"
#define DISK_PSNA_DIR "/.p"
#define DISK_PSNA_FILE "persist"

#define NO_RAM_FILE "none"

#define SNA_48K_SIZE 49179
#define SNA_48K_WITH_ROM_SIZE 65563
#define SNA_128K_SIZE1 131103
#define SNA_128K_SIZE2 147487
#define SNA_2A3_SIZE1 131121

#ifdef ESPECTRUM_PSRAM

// Experimental values for PSRAM
#define DIR_CACHE_SIZE 256
#define DIR_CACHE_SIZE_OVERSCAN 256
#define FILENAMELEN 128

#else

// Values for no PSRAM
#define DIR_CACHE_SIZE 64
#define DIR_CACHE_SIZE_OVERSCAN 32
#define FILENAMELEN 128

#endif

#define SDCARD_HOST_MAXFREQ 19000

// inline utility functions for uniform access to file/memory
// and making it easy to to implement SNA/Z80 functions

static inline uint8_t readByteFile(FILE *f) {
    uint8_t result;

    if (fread(&result, 1, 1, f) != 1)
        return -1;

    return result;
}

static inline uint16_t readWordFileLE(FILE *f) {
    uint8_t lo = readByteFile(f);
    uint8_t hi = readByteFile(f);
    return lo | (hi << 8);
}

static inline uint16_t readWordFileBE(FILE *f) {
    uint8_t hi = readByteFile(f);
    uint8_t lo = readByteFile(f);
    return lo | (hi << 8);
}

static inline size_t readBlockFile(FILE *f, uint8_t* dstBuffer, size_t size) {
    return fread(dstBuffer, 0x4000, 1, f);
}

static inline void writeByteFile(uint8_t value, FILE *f) {
    fwrite(&value,1,1,f);
}

static inline void writeWordFileLE(uint16_t value, FILE *f) {
    uint8_t lo =  value       & 0xFF;
    uint8_t hi = (value >> 8) & 0xFF;
    fwrite(&lo,1,1,f);
    fwrite(&hi,1,1,f);
}

// static inline void writeWordFileBE(uint16_t value, File f) {
//     uint8_t hi = (value >> 8) & 0xFF;
//     uint8_t lo =  value       & 0xFF;
//     f.write(hi);
//     f.write(lo);
// }

// static inline size_t writeBlockFile(uint8_t* srcBuffer, File f, size_t size) {
//     return f.write(srcBuffer, size);
// }

// static inline uint8_t readByteMem(uint8_t*& ptr) {
//     uint8_t value = *ptr++;
//     return value;
// }

// static inline uint16_t readWordMemLE(uint8_t*& ptr) {
//     uint8_t lo = *ptr++;
//     uint8_t hi = *ptr++;
//     return lo | (hi << 8);
// }

// static inline uint16_t readWordMemBE(uint8_t*& ptr) {
//     uint8_t hi = *ptr++;
//     uint8_t lo = *ptr++;
//     return lo | (hi << 8);
// }

// static inline size_t readBlockMem(uint8_t*& srcBuffer, uint8_t* dstBuffer, size_t size) {
//     memcpy(dstBuffer, srcBuffer, size);
//     srcBuffer += size;
//     return size;
// }

// static inline void writeByteMem(uint8_t value, uint8_t*& ptr) {
//     *ptr++ = value;
// }

// static inline void writeWordMemLE(uint16_t value, uint8_t*& ptr) {
//     uint8_t lo =  value       & 0xFF;
//     uint8_t hi = (value >> 8) & 0xFF;
//     *ptr++ = lo;
//     *ptr++ = hi;
// }

// static inline void writeWordMemBE(uint16_t value, uint8_t*& ptr) {
//     uint8_t hi = (value >> 8) & 0xFF;
//     uint8_t lo =  value       & 0xFF;
//     *ptr++ = hi;
//     *ptr++ = lo;
// }

// static inline size_t writeBlockMem(uint8_t* srcBuffer, uint8_t*& dstBuffer, size_t size) {
//     memcpy(dstBuffer, srcBuffer, size);
//     dstBuffer += size;
//     return size;
// }

#endif // FileUtils_h
