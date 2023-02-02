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

#include "FileUtils.h"
#include "CPU.h"
#include "MemESP.h"
#include "ESPectrum.h"
#include "hardpins.h"
#include "messages.h"
#include "OSDMain.h"
// #include "sort.h"
#include "FileUtils.h"
#include "Config.h"
#include "roms.h"
#include "esp_vfs.h"

#include "esp_spiffs.h"

#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO GPIO_NUM_2
#define PIN_NUM_MOSI GPIO_NUM_12
#define PIN_NUM_CLK  GPIO_NUM_14
#define PIN_NUM_CS   GPIO_NUM_13

sdmmc_card_t *FileUtils::card;

void FileUtils::unmountSDCard() {
    // Unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT_SD, card);
    // //deinitialize the bus after all devices are removed
    spi_bus_free(SPI2_HOST);
}

// Globals
void FileUtils::initFileSystem() {

    // Initialize SPIFFS file system
    esp_vfs_spiffs_conf_t config = {
        .base_path = MOUNT_POINT_SPIFFS,
        .partition_label = NULL,
        .max_files = 1,
        .format_if_mount_failed = false,
    };
    if (esp_vfs_spiffs_register(&config) != ESP_OK) {
        return;
    }

    // Initialize SD Card
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH1);
    if (ret != ESP_OK) {
        printf("SD Card init: Failed to initialize bus.\n");
        return;
    }

    sdspi_device_config_t slot_config =  {
    .host_id   = SDSPI_DEFAULT_HOST,
    .gpio_cs   = GPIO_NUM_13,
    .gpio_cd   = SDSPI_SLOT_NO_CD,
    .gpio_wp   = SDSPI_SLOT_NO_WP,
    .gpio_int  = GPIO_NUM_NC, \
    };
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT_SD, &host, &slot_config, &mount_config, &FileUtils::card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount filesystem.\n");
            return;
        } else {
            printf("Failed to initialize the card.\n");
            return;
        }
        return;
    }
    vTaskDelay(2);

}

// String FileUtils::getAllFilesFrom(const String path) {
//     KB_INT_STOP;
//     File root = THE_FS.open("/");
//     File file = root.openNextFile();
//     String listing;

//     while (file) {
//         file = root.openNextFile();
//         String filename = file.name();
//         if (filename.startsWith(path) && !filename.startsWith(path + "/.")) {
//             listing.concat(filename.substring(path.length() + 1));
//             listing.concat("\n");
//         }
//     }
//     vTaskDelay(2);
//     KB_INT_START;
//     return listing;
// }

// void FileUtils::listAllFiles() {
//     KB_INT_STOP;
//     File root = THE_FS.open("/");
//     Serial.println("fs opened");
//     File file = root.openNextFile();
//     Serial.println("fs openednextfile");

//     while (file) {
//         Serial.print("FILE: ");
//         Serial.println(file.name());
//         file = root.openNextFile();
//     }
//     vTaskDelay(2);
//     KB_INT_START;
// }

// void FileUtils::sanitizeFilename(String filename)
// {
//     filename.replace("\n", " ");
//     filename.trim();
// }

// File FileUtils::safeOpenFileRead(String filename)
// {
//     sanitizeFilename(filename);
//     File f;
//     if (Config::slog_on)
//         Serial.printf("%s '%s'\n", MSG_LOADING, filename.c_str());
//     if (!THE_FS.exists(filename.c_str())) {
//         KB_INT_START;
//         OSD::errorHalt((String)ERR_READ_FILE + "\n" + filename);
//     }
//     f = THE_FS.open(filename.c_str(), FILE_READ);
//     vTaskDelay(2);

//     return f;
// }

string FileUtils::getFileEntriesFromDir(string path) {

    string filelist;

    // printf("Getting entries from: '%s'\n", path.c_str());

    DIR* dir = opendir(path.c_str());
    if (dir == NULL) {
        // OSD::errorHalt(ERR_DIR_OPEN + "\n" + path).cstr());
    }

    struct dirent* de = readdir(dir);
    
    if (!de) {

        printf("No entries found!\n");

    } else {

        while (true) {
            
        //    printf("Found file: %s\n", de->d_name);
            
            string filename = de->d_name;

            if (filename.compare(0,1,".") == 0) {
        //        printf("HIDDEN\n");
            } else if (filename.substr(filename.size()-4) == ".txt") {
        //        printf("IGNORING TXT\n");
            } else if (filename.substr(filename.size()-4) == ".TXT") {
        //        printf("IGNORING TXT\n");
            } else {
        //        printf("ADDING\n");
                filelist += filename + "\n";
            }
            
            de = readdir(dir);
            if (!de) break;
        
        }

    }

    // printf(filelist.c_str());

    closedir(dir);

    return filelist;

}

bool FileUtils::hasSNAextension(string filename)
{
    
    if (filename.substr(filename.size()-4,4) == ".sna") return true;
    if (filename.substr(filename.size()-4,4) == ".SNA") return true;

    return false;

}

bool FileUtils::hasZ80extension(string filename)
{

    if (filename.substr(filename.size()-4,4) == ".z80") return true;
    if (filename.substr(filename.size()-4,4) == ".Z80") return true;

    return false;

}

// uint16_t FileUtils::countFileEntriesFromDir(String path) {
//     String entries = getFileEntriesFromDir(path);
//     unsigned short count = 0;
//     for (unsigned short i = 0; i < entries.length(); i++) {
//         if (entries.charAt(i) == ASCII_NL) {
//             count++;
//         }
//     }
//     return count;
// }

void FileUtils::loadRom(string arch, string romset) {

    if (arch == "48K") {
        for (int i=0;i < max_list_rom_48; i++) {
            if (romset.find(gb_list_roms_48k_title[i]) != string::npos) {
                MemESP::rom[0] = (uint8_t *) gb_list_roms_48k_data[i];
                break;
            }
        }
    } else {
        for (int i=0;i < max_list_rom_128; i++) {
            if (romset.find(gb_list_roms_128k_title[i]) != string::npos) {
                MemESP::rom[0] = (uint8_t *) gb_list_roms_128k_data[i][0];
                MemESP::rom[1] = (uint8_t *) gb_list_roms_128k_data[i][1];
                MemESP::rom[2] = (uint8_t *) gb_list_roms_128k_data[i][2];
                MemESP::rom[3] = (uint8_t *) gb_list_roms_128k_data[i][3];
                break;
            }
        }
    }

}

// Get all sna files sorted alphabetically
string FileUtils::getSortedFileList(string fileDir)
{

    
    return getFileEntriesFromDir(fileDir);
    
    // // get string of unsorted filenames, separated by newlines
    // string entries = getFileEntriesFromDir(fileDir);

    // // count filenames (they always end at newline)
    // unsigned short count = 0;
    // for (unsigned short i = 0; i < entries.length(); i++) {
    //     if (entries.at(i) == ASCII_NL) {
    //         count++;
    //     }
    // }

    // // array of filenames
    // String* filenames = (String*)malloc(count * sizeof(String));
    // // memory must be initialized to avoid crash on assign
    // memset(filenames, 0, count * sizeof(String));

    // // copy filenames from string to array
    // unsigned short ich = 0;
    // unsigned short ifn = 0;
    // for (unsigned short i = 0; i < entries.length(); i++) {
    //     if (entries.charAt(i) == ASCII_NL) {
    //         filenames[ifn++] = entries.substring(ich, i);
    //         ich = i + 1;
    //     }
    // }

    // // sort array
    // sortArray(filenames, count);

    // // string of sorted filenames
    // String sortedEntries = "";

    // // copy filenames from array to string
    // for (unsigned short i = 0; i < count; i++) {
    //     sortedEntries += filenames[i] + '\n';
    // }

    // return sortedEntries;
}
