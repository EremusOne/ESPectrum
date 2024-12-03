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

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <algorithm>
#include "FileUtils.h"
#include "Config.h"
#include "cpuESP.h"
#include "MemESP.h"
#include "ESPectrum.h"
#include "hardpins.h"
#include "messages.h"
#include "OSDMain.h"
#include "roms.h"
#include "Video.h"
#include "ZXKeyb.h"

#include "esp_vfs.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

using namespace std;

string FileUtils::MountPoint = MOUNT_POINT_SD; // Start with SD
bool FileUtils::SDReady = false;
sdmmc_card_t *FileUtils::card;

string FileUtils::SNA_Path = "/"; // Current path on the SD
string FileUtils::TAP_Path = "/"; // Current path on the SD
string FileUtils::DSK_Path = "/"; // Current path on the SD
string FileUtils::ROM_Path = "/"; // Current path on the SD
string FileUtils::ESP_Path = "/.p/"; // Current path on the SD

DISK_FTYPE FileUtils::fileTypes[5] = {
    {"sna,z80,sp,p",".s",2,2,0,""},
    {"tap,tzx,",".t",2,2,0,""},
    {"trd,scl",".d",2,2,0,""},
    {"rom",".r",2,2,0,""},
    {"esp",".e",2,2,0,""}
};

string toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr;
}

// get extension in lowercase
string FileUtils::getLCaseExt(const string& filename) {
    size_t dotPos = filename.rfind('.'); // find the last dot position
    if (dotPos == string::npos) {
        return ""; // dot position don't found
    }

    // get the substring after dot
    string extension = filename.substr(dotPos + 1);

    // convert extension to lowercase
//    for (char& c : extension) {
//        c = ::tolower(static_cast<unsigned char>(c));
//    }

//    return extension;

    return toLower( extension );

}

size_t FileUtils::fileSize(const char * mFile) {
    struct stat stat_buf;
    if ( !mFile ) return -1;
    int status = stat(mFile, &stat_buf);
    if ( status == -1 || ! ( stat_buf.st_mode & S_IFREG ) ) return -1;
    return stat_buf.st_size;
}

void FileUtils::initFileSystem() {

    // Try to mount SD card on LILYGO TTGO VGA32 Board or ESPectrum Board
    if (!SDReady) SDReady = mountSDCard(PIN_NUM_MISO_LILYGO_ESPECTRUM,PIN_NUM_MOSI_LILYGO_ESPECTRUM,PIN_NUM_CLK_LILYGO_ESPECTRUM,PIN_NUM_CS_LILYGO_ESPECTRUM);

    // Try to mount SD card on Olimex ESP32-SBC-FABGL Board
    if ((!ZXKeyb::Exists) && (!SDReady)) SDReady = mountSDCard(PIN_NUM_MISO_SBCFABGL,PIN_NUM_MOSI_SBCFABGL,PIN_NUM_CLK_SBCFABGL,PIN_NUM_CS_SBCFABGL);

}

bool FileUtils::mountSDCard(int PIN_MISO, int PIN_MOSI, int PIN_CLK, int PIN_CS) {

    // Init SD Card
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH1);
    if (ret != ESP_OK) {
        printf("SD Card init: Failed to initialize bus.\n");
        vTaskDelay(20 / portTICK_PERIOD_MS);
        return false;
    }

    vTaskDelay(20 / portTICK_PERIOD_MS);

    sdspi_device_config_t slot_config =  {
    .host_id   = SDSPI_DEFAULT_HOST,
    .gpio_cs   = GPIO_NUM_13,
    .gpio_cd   = SDSPI_SLOT_NO_CD,
    .gpio_wp   = SDSPI_SLOT_NO_WP,
    .gpio_int  = GPIO_NUM_NC, \
    };
    slot_config.gpio_cs = (gpio_num_t) PIN_CS;
    slot_config.host_id = SPI2_HOST;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT_SD, &host, &slot_config, &mount_config, &FileUtils::card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount filesystem.\n");
        } else {
            printf("Failed to initialize the card.\n");
        }
        spi_bus_free(SPI2_HOST);
        vTaskDelay(20 / portTICK_PERIOD_MS);
        return false;
    }

    // This seems to fix problems when video framebuffer is too big (400x300 i.e.)
    host.max_freq_khz = SDCARD_HOST_MAXFREQ;
    host.set_card_clk(host.slot, SDCARD_HOST_MAXFREQ);

    printf("SD Host max freq: %d\n",host.max_freq_khz);

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    vTaskDelay(20 / portTICK_PERIOD_MS);

    return true;

}

void FileUtils::unmountSDCard() {
    // Unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT_SD, card);
    // //deinitialize the bus after all devices are removed
    spi_bus_free(SPI2_HOST);

    SDReady = false;
}

bool FileUtils::isMountedSDCard() {
    // Dirty SDCard mount detection
    DIR* dir = opendir("/sd");
    if ( !dir ) return false;
    struct dirent* de = readdir(dir);
    if ( !de && ( errno == EIO || errno == EBADF ) ) {
        closedir(dir);
        return false;
    }
    closedir(dir);
    return true;
}

bool FileUtils::isSDReady() {

    if ( FileUtils::SDReady && !FileUtils::isMountedSDCard() ) {
        FileUtils::unmountSDCard();
    }

    if ( !FileUtils::SDReady ) {
        FileUtils::initFileSystem();
        if ( !FileUtils::SDReady ) {
            OSD::osdCenteredMsg(ERR_FS_EXT_FAIL[Config::lang], LEVEL_ERROR);
            return false;
        }
    }

    return true;

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

// string FileUtils::getFileEntriesFromDir(string path) {

//     string filelist;

//     // printf("Getting entries from: '%s'\n", path.c_str());

//     DIR* dir = opendir(path.c_str());
//     if (dir == NULL) {
//         // OSD::errorHalt(ERR_DIR_OPEN + "\n" + path).cstr());
//     }

//     struct dirent* de = readdir(dir);

//     if (!de) {

//         printf("No entries found!\n");

//     } else {

//         int cnt = 0;
//         while (true) {

//             printf("Found file: %s\n", de->d_name);

//             string filename = de->d_name;

//             // printf("readdir filename -> %s\n", filename.c_str());

//             if (filename.compare(0,1,".") == 0) {
//         //        printf("HIDDEN\n");
//             } else if (filename.substr(filename.size()-4) == ".txt") {
//         //        printf("IGNORING TXT\n");
//             } else if (filename.substr(filename.size()-4) == ".TXT") {
//         //        printf("IGNORING TXT\n");
//             } else {
//         //        printf("ADDING\n");
//                 filelist += filename + "\n";
//                 cnt++;
//             }

//             de = readdir(dir);
//             if ((!de) || (cnt == 20)) break;

//         }

//     }

//     // printf(filelist.c_str());

//     closedir(dir);

//     return filelist;

// }

int FileUtils::getDirStats(const string& filedir, const vector<string>& filexts, unsigned long* hash, unsigned int* elements, unsigned int* ndirs) {
    *hash = 0; // Name checksum variable
    unsigned long high;
    DIR* dir;
    struct dirent* de;

    *elements = 0;
    *ndirs = 0;

    string fdir = filedir.substr(0, filedir.length() - 1);
    if ((dir = opendir(fdir.c_str())) != nullptr) {
        while ((de = readdir(dir)) != nullptr) {
            string fname = de->d_name;
            if (de->d_type == DT_REG || de->d_type == DT_DIR) {
                if (fname.compare(0, 1, ".") != 0) {
                    if ((de->d_type == DT_DIR) || (std::find(filexts.begin(), filexts.end(), FileUtils::getLCaseExt(fname)) != filexts.end())) {
                        // Calculate name checksum
                        for (int i = 0; i < fname.length(); i++) {
                            *hash = (*hash << 4) + fname[i];
                            if (high = *hash & 0xF0000000) *hash ^= high >> 24;
                            *hash &= ~high;
                        }
                        if (de->d_type == DT_REG)
                            (*elements)++; // Count elements in dir
                        else if (de->d_type == DT_DIR)
                            (*ndirs)++;
                    }
                }
            }
        }
        closedir(dir);
        return 0;
    }

    return -1;
}

string FileUtils::getResolvedPath(const string& path) {
    char *resolved_path = NULL;
    if ((resolved_path = realpath(path.c_str(), resolved_path)) == NULL) {
        printf("Error resolving path\n");
        return "";
    }
    string ret = string(resolved_path);
    free(resolved_path);
    return ret;
}

string FileUtils::createTmpDir() {
    string tempDir = MountPoint + "/.tmp";

    // Verificar si el directorio ya existía
    struct stat info;

    // Crear el directorio si no existe
    if (!(stat(tempDir.c_str(), &info) == 0 && (info.st_mode & S_IFDIR))) {
        if (mkdir(tempDir.c_str(), 0755) != 0) {
            printf( "TMP directory creation failed\n" );
            return "";
        }
    }

    return tempDir;
}

void FileUtils::DirToFile(string fpath, uint8_t ftype, unsigned long hash, unsigned int item_count) {
    FILE* fin = nullptr;
    FILE* fout = nullptr;
    char line[FILENAMELEN + 1];
    string fname1 = "";
    string fname2 = "";
    string fnameLastSaved = "";

    // printf("\nJust after entering dirtofile");
    // ESPectrum::showMemInfo();
    // printf("\n");

    // Populate filexts with valid filename extensions
    std::vector<std::string> filexts;
    size_t pos = 0;
    string ss = fileTypes[ftype].fileExts;
    while ((pos = ss.find(",")) != string::npos) {
        // printf("%s , ",ss.substr(0,pos).c_str());
        filexts.push_back(ss.substr(0, pos));
        ss.erase(0, pos + 1);
    }
    // printf("%s , ",ss.substr(0).c_str());
    filexts.push_back(ss.substr(0));
    // printf("\n");

    // Remove previous dir file
    remove((fpath + fileTypes[ftype].indexFilename).c_str());

    string fdir = fpath.substr(0, fpath.length() - 1);
    DIR* dir = opendir(fdir.c_str());
    if (dir == NULL) {
        printf("Error opening %s\n", fpath.c_str());
        return;
    }

    OSD::progressDialog(OSD_FILE_INDEXING[Config::lang],OSD_FILE_INDEXING_1[Config::lang],0,0);

    int items_processed = 0;
    struct dirent* de;

    OSD::elements = 0;
    OSD::ndirs = 0;

    bool readFile1 = false, readFile2 = true;
    bool eof1 = true, eof2 = false;
    bool holdFile2 = false;

    int n = 1;

    if (fpath != ( MountPoint + "/" ) ) {
        fname1 = "  ..";
        eof1 = false;
    }

    // printf("\nBefore checking tempdir");
    // ESPectrum::showMemInfo();
    // printf("\n");

    string tempDir = FileUtils::createTmpDir();
    if ( tempDir == "" ) {
        closedir(dir);
        // Close progress dialog
        OSD::progressDialog("","",0,2);
        return;
    }

    // printf("\nAfter checking tempdir");
    // ESPectrum::showMemInfo();
    // printf("\n");


    int bufferSize;
    if (Config::videomode < 2) {
        bufferSize = item_count > DIR_CACHE_SIZE ? DIR_CACHE_SIZE : item_count;  // Size of buffer to read and sort
    } else {
        bufferSize = item_count > DIR_CACHE_SIZE_OVERSCAN ? DIR_CACHE_SIZE_OVERSCAN : item_count;  // Size of buffer to read and sort
    }
    std::vector<std::string> buffer;

    int iterations = 0;

    // printf("\nBefore while");
    // ESPectrum::showMemInfo();
    // printf("\n");

    while ( !eof2 || ( fin && !feof(fin)) ) {
        fnameLastSaved = "";

        holdFile2 = false;

        iterations++;

        fout = fopen((/*fpath*/ tempDir + "/" + fileTypes[ftype].indexFilename + ".tmp." + std::to_string(n&1)).c_str(), "wb");
        if ( !fout ) {
            if ( fin ) fclose( fin );
            closedir( dir );
            // Close progress dialog
            OSD::progressDialog("","",0,2);
            return;
        }

        if (setvbuf(fout, NULL, _IOFBF, 1024) != 0) {
            printf("setvbuf failed\n");
        }

        while (1) {

            if ( readFile1 ) {
                if ( !fin || feof( fin ) ) eof1 = true;
                if ( !eof1 ) {
                    size_t res = fread( line, sizeof(char), FILENAMELEN, fin);
                    if ( !res || feof( fin ) ) {
                        eof1 = true;
                    } else {
                        line[FILENAMELEN-1] = '\0';
                        fname1.assign(line);
                    }
                }
                readFile1 = false;
            }

            if ( readFile2 ) {

                if (buffer.empty()) { // Fill buffer with directory entries

                    // buffer.clear();

                    if ( bufferSize ) {

                        // printf("\nBefore buffer fill -> ");
                        // ESPectrum::showMemInfo();
                        // printf("\n");

                        while ( buffer.size() < bufferSize && (de = readdir(dir)) != nullptr ) {
                            if (de->d_name[0] != '.') {
                                string fname = de->d_name;
                                if (de->d_type == DT_DIR) {
                                    buffer.push_back( " " + fname );
                                    OSD::ndirs++;
                                } else if (de->d_type == DT_REG && std::find(filexts.begin(), filexts.end(), getLCaseExt(fname)) != filexts.end()) {
                                    buffer.push_back( fname );
                                    OSD::elements++;
                                }
                            }
                        }

                        // printf("Buffer size: %d\n",buffer.size());
                        // printf("Before buffer sort -> ");
                        // ESPectrum::showMemInfo();
                        // printf("\n");

                        // Sort buffer loaded with processed directory entries
                        sort(buffer.begin(), buffer.end(), [](const string& a, const string& b) {
                            return ::toLower(a) < toLower(b);
                        });

                    } else {

                        eof2 = true;
                        readFile2 = false;

                    }

                }

                if (!buffer.empty()) {

                    fname2 = buffer.front();

                    buffer.erase(buffer.begin()); // Remove first element from buffer

                    items_processed++;

                    OSD::progressDialog("","",(float) 100 / ((float) item_count / (float) items_processed),1);

                } else {
                    if ( !de ) eof2 = true;
                }

                readFile2 = false;
                holdFile2 = false;
            }

            string fnameToSave = "";

            if ( eof1 ) {
                if ( eof2 || holdFile2 || strcasecmp(fnameLastSaved.c_str(), fname2.c_str()) > 0 ) {
                    break;
                }
                fnameToSave = fname2;
                readFile2 = true;
            } else
            // eof2 || fname1 < fname2
            // si fname2 > fname1 entonces grabar fname1, ya que fname2 esta ordenado y no puede venir uno menor en este grupo
            if ( eof2 || strcasecmp(fname1.c_str(), fname2.c_str()) < 0 ) {
                fnameToSave = fname1;
                readFile1 = true;
            } else
            if ( strcasecmp(fname1.c_str(), fname2.c_str()) > 0 && strcasecmp(fnameLastSaved.c_str(), fname2.c_str()) > 0 ) {
                // fname1 > fname2 && last > fname2
                holdFile2 = true;
                fnameToSave = fname1;
                readFile1 = true;
            } else {
                if ( strcasecmp(fnameLastSaved.c_str(), fname2.c_str()) > 0 ) {
                    break;
                }
                fnameToSave = fname2;
                readFile2 = true;
            }

            if ( fnameToSave != "" ) {
                string sw;
                if ( fnameToSave.length() > FILENAMELEN - 1 )   sw = fnameToSave.substr(0,FILENAMELEN - 1) + "\n";
                else                                            sw = fnameToSave + string(FILENAMELEN - 1 - fnameToSave.size(), ' ') + "\n";
                fwrite(sw.c_str(), sizeof(char), sw.length(), fout);
                fnameLastSaved = fnameToSave;
            }
        }

        if ( fin ) {
            fclose(fin);
            fin = nullptr;
        }

        fclose(fout);

        if ( eof1 && eof2 ) break;

        fin = fopen((/*fpath*/ tempDir + "/" + fileTypes[ftype].indexFilename + ".tmp." + std::to_string(n&1)).c_str(), "rb");
        if ( !fin ) {
            buffer.clear(); // Clear vector
            std::vector<std::string>().swap(buffer); // free memory

            filexts.clear(); // Clear vector
            std::vector<std::string>().swap(filexts); // free memory

            closedir( dir );
            // Close progress dialog
            OSD::progressDialog("","",0,2);
            return;
        }

        if (setvbuf(fin, NULL, _IOFBF, 512) != 0) {
            printf("setvbuf failed\n");
        }

        eof1 = false;
        readFile1 = true;

        n++;
    }

    buffer.clear(); // Clear vector
    std::vector<std::string>().swap(buffer); // free memory

    filexts.clear(); // Clear vector
    std::vector<std::string>().swap(filexts); // free memory

    if ( fin ) fclose(fin);
    closedir(dir);

    rename((/*fpath*/ tempDir + "/" + fileTypes[ftype].indexFilename + ".tmp." + std::to_string(n&1)).c_str(), (fpath + fileTypes[ftype].indexFilename).c_str());

    // Add directory hash to last line of file
    fout = fopen((fpath + fileTypes[ftype].indexFilename).c_str(), "a");
    if ( !fout ) {
        // Close progress dialog
        OSD::progressDialog("","",0,2);
        return;
    }

//    char bhash[21]; // Reserva espacio para el número más un carácter nulo
//    int length = snprintf(bhash, sizeof(bhash), "%020lu", hash); // Formatea el número con longitud fija
//    fputs(bhash, fout); // Escribe la cadena formateada en el archivo
    fprintf( fout, "%020lu", hash );
    fclose(fout);

    if ( n ) {

        OSD::progressDialog(OSD_FILE_INDEXING[Config::lang],OSD_FILE_INDEXING_3[Config::lang],0,1);
        remove((/*fpath*/ tempDir + "/" + fileTypes[ftype].indexFilename + ".tmp." + std::to_string((n-1)&1)).c_str());

        OSD::progressDialog("","",(float) 100, 1);
    }

    // Close progress dialog
    OSD::progressDialog("","",0,2);

    printf("total iterations %d\n", iterations );

}

bool FileUtils::hasExtension(string filename, string extension) {
    return ( getLCaseExt(filename) == toLower(extension) );
}

bool FileUtils::hasSNAextension(string filename) {
    return ( getLCaseExt(filename) == "sna" );
}

bool FileUtils::hasZ80extension(string filename) {
    return ( getLCaseExt(filename) == "z80" );
}

bool FileUtils::hasPextension(string filename) {
    return ( getLCaseExt(filename) == "p" );
}

bool FileUtils::hasTAPextension(string filename) {
    return ( getLCaseExt(filename) == "tap" );
}

bool FileUtils::hasTZXextension(string filename) {
    return ( getLCaseExt(filename) == "tzx" );
}

void FileUtils::deleteFilesWithExtension(const char *folder_path, const char *extension) {

    DIR *dir;
    struct dirent *entry;
    dir = opendir(folder_path);

    if (dir == NULL) {
        // perror("Unable to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            if (strstr(entry->d_name, extension) != NULL) {
                char file_path[512];
                snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, entry->d_name);
                if (remove(file_path) == 0) {
                    printf("Deleted file: %s\n", entry->d_name);
                } else {
                    printf("Failed to delete file: %s\n", entry->d_name);
                }
            }
        }
    }

    closedir(dir);

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

// // Get all sna files sorted alphabetically
// string FileUtils::getSortedFileList(string fileDir)
// {

//     // get string of unsorted filenames, separated by newlines
//     string entries = getFileEntriesFromDir(fileDir);

//     // count filenames (they always end at newline)
//     int count = 0;
//     for (int i = 0; i < entries.length(); i++) {
//         if (entries.at(i) == ASCII_NL) {
//             count++;
//         }
//     }

//     std::vector<std::string> filenames;
//     filenames.reserve(count);

//     // Copy filenames from string to vector
//     string fname = "";
//     for (int i = 0; i < entries.length(); i++) {
//         if (entries.at(i) == ASCII_NL) {
//             filenames.push_back(fname.c_str());
//             fname = "";
//         } else fname += entries.at(i);
//     }

//     // Sort vector
//     sort(filenames.begin(),filenames.end());

//     // Copy back filenames from vector to string
//     string sortedEntries = "";
//     for (int i = 0; i < count; i++) {
//         sortedEntries += filenames[i] + '\n';
//     }

//     return sortedEntries;

// }
