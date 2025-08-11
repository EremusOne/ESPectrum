/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023-2025 Víctor Iborra [Eremus] and 2023 David Crespo [dcrespo3d]
https://github.com/EremusOne/ESPectrum

Based on ZX-ESPectrum-Wiimote
Copyright (c) 2020-2022 David Crespo [dcrespo3d]
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

To Contact the dev team you can write to zxespectrum@gmail.com

*/

#include <string>
#include <algorithm>
#include <sys/stat.h>
#include "errno.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"

using namespace std;

#include "OSDMain.h"
#include "FileUtils.h"
#include "ESPConfig.h"
#include "ESPectrum.h"
#include "cpuESP.h"
#include "Video.h"
#include "messages.h"
#include <math.h>
#include "ZXKeyb.h"
#include "pwm_audio.h"
#include "Z80_JLS/z80.h"
#include "Tape.h"

FILE *dirfile;
unsigned int OSD::elements;
unsigned int OSD::fdSearchElements;
unsigned int OSD::ndirs;
int8_t OSD::fdScrollPos;
int OSD::timeStartScroll;
int OSD::timeScroll;
uint8_t OSD::fdCursorFlash;
bool OSD::fdSearchRefresh;

void OSD::restoreBackbufferData(bool force) {
    if ( !SaveRectpos ) return;
    if (menu_saverect || force) {
//    printf("--- OSD::restoreBackbufferData %d\n", SaveRectpos);

        // Restaurar datos del backbuffer utilizando RLE o bloques sin comprimir
        uint32_t j = VIDEO::SaveRect[SaveRectpos - 1];  // Obtener la dirección de inicio del bloque

        SaveRectpos = j;

//        printf("OSD::restoreBackbufferData j=%d\n", j);

        uint16_t x = VIDEO::SaveRect[j] >> 16;
        uint16_t y = VIDEO::SaveRect[j++] & 0xffff;
        uint16_t w = VIDEO::SaveRect[j] >> 16;
        uint16_t h = VIDEO::SaveRect[j++] & 0xffff;

//        printf("OSD::restoreBackbufferData x=%hd y=%hd w=%hd h=%hd\n", x, y, w, h);

        uint32_t *backbuffer32 = nullptr;
        for (uint32_t m = y; m < y + h; m++) {
            backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
            for (uint32_t x_off = x >> 2; x_off < ((x + w) >> 2) + 1;) {
                uint32_t run_length = VIDEO::SaveRect[j++];
                if (run_length & 0x80000000) {  // Bloque comprimido
                    run_length &= 0x7FFFFFFF;  // Limpiar el bit más alto
                    uint32_t value = VIDEO::SaveRect[j++];
                    for (int k = 0; k < run_length; k++) {
                        backbuffer32[x_off++] = value;
                    }
                } else {  // Bloque sin comprimir
                    for (int k = 0; k < run_length; k++) {
                        backbuffer32[x_off++] = VIDEO::SaveRect[j++];
                    }
                }
            }
        }
//        printf("OSD::restoreBackbufferData exit %d\n", SaveRectpos);
//        if ( !force )
        menu_saverect = false;
    }
}

void OSD::saveBackbufferData(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool force) {

    if ( force || menu_saverect ) {
//        printf("OSD::saveBackbufferData x=%hd y=%hd w=%hd h=%hd pos=%d\n", x, y, w, h, SaveRectpos);
        // Guardar datos del backbuffer utilizando RLE o bloques sin comprimir
        uint32_t start_pos = SaveRectpos;

        VIDEO::SaveRect[SaveRectpos++] = ( x << 16 ) | y;
        VIDEO::SaveRect[SaveRectpos++] = ( w << 16 ) | h;

        for (uint32_t m = y; m < y + h; m++) {
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
            uint32_t n_start = x >> 2;
            uint32_t current_value = backbuffer32[n_start];
            bool raw_mode = true;
            uint32_t count_pos = SaveRectpos;

            VIDEO::SaveRect[SaveRectpos++] = 1; // Contador a 1
            VIDEO::SaveRect[SaveRectpos++] = current_value;

            for (uint32_t n = n_start + 1; n < ((x + w) >> 2) + 1; n++) {
                if (backbuffer32[n] == current_value) {
                    if ( raw_mode ) {
                        if ( VIDEO::SaveRect[count_pos] != 1 ) {
                            // descarto el ultimo
                            VIDEO::SaveRect[count_pos]--;
                        } else {
                            SaveRectpos--;
                        }
                        count_pos = SaveRectpos - 1;
                        VIDEO::SaveRect[count_pos] = 0x80000001;

                        VIDEO::SaveRect[SaveRectpos++] = current_value;
                        raw_mode = false;
                    }
                    VIDEO::SaveRect[count_pos]++;
                } else {
                    current_value = backbuffer32[n];
                    if ( !raw_mode ) {
                        count_pos = SaveRectpos++;
                        VIDEO::SaveRect[count_pos] = 0; // count a 0
                    }
                    VIDEO::SaveRect[count_pos]++;
                    VIDEO::SaveRect[SaveRectpos++] = current_value;
                    raw_mode = true;
                }
            }
        }

        // Guardar la dirección de inicio del bloque
        VIDEO::SaveRect[SaveRectpos++] = start_pos;
//        printf("OSD::saveBackbufferData exit %d sp: %d\n", SaveRectpos, start_pos);
    }
}

void OSD::saveBackbufferData(bool force) {
    OSD::saveBackbufferData(x, y, w, h, force);
}

// Función para convertir una cadena de dígitos en un número
// se agrega esta funcion porque atoul crashea si no hay digitos en el buffer
unsigned long getLong(char *buffer) {
    unsigned long result = 0;
    char * p = buffer;

    while (p && isdigit(*p)) {
        result = result * 10 + (*p - '0');
        ++p;
    }

    return result;
}

// Run a new file menu
string OSD::fileDialog(string &fdir, string title, uint8_t ftype, uint8_t mfcols, uint8_t mfrows, uint8_t actions) {

    // struct stat stat_buf;
    long dirfilesize;
    bool reIndex;
    int hfocus;

    // Columns and Rows
    cols = mfcols;
    mf_rows = mfrows + (Config::aspect_16_9 ? 0 : 1);

    // CRT Overscan compensation
    if (Config::videomode == 2) {
        x = 18;
        if (menu_level == 0) {
            if (Config::arch[0] == 'T' && Config::ALUTK == 2) {
                y = 4;
            } else {
                y = 12;
            }
        }
    } else {
        x = 0;
        if (menu_level == 0) y = 0;
    }

    // Position
    if (menu_level == 0) {
        x += (Config::aspect_16_9 ? 24 : 8);
        y += (Config::aspect_16_9 ? 4 : 8);
    } else {
        x += (Config::aspect_16_9 ? 24 : 4) + (48 * menu_level);
        y += (Config::aspect_16_9 ? 8 : 8) + (4 * (menu_level - 1));
    }

    // Size
    // w = (cols * OSD_FONT_W) + 2;
    // h = ((mf_rows + 1) * OSD_FONT_H) + 2;
    // // Check window boundaries
    // if ( x + mfcols * OSD_FONT_W > (Config::aspect_16_9 ? 24 : 4) + 52 * OSD_FONT_W ) x = (Config::aspect_16_9 ? 24 : 4) + ( 51 - mfcols ) * OSD_FONT_W;
    // if ( y + mfrows > (Config::aspect_16_9 ? 200 : 240) - 2 * OSD_FONT_H ) y = (Config::aspect_16_9 ? 200 : 240) - ( mfrows + 2 ) * OSD_FONT_H;

    // Adjust dialog size if needed
    w = (cols * OSD_FONT_W) + 2;
    printf("X: %d w: %d Cols: %d scrW: %d\n",x,w,cols,scrW);
    while ( x + w >= OSD::scrW - OSD_FONT_W) {
        cols--;
        w = (cols * OSD_FONT_W) + 2;
        printf("X: %d w: %d Cols: %d scrW: %d\n",x,w,cols,scrW);
    };

    h = ((mf_rows + 1) * OSD_FONT_H) + 2;
    printf("Y: %d h: %d mf_rows: %d scrH: %d\n",y,h,mf_rows,scrH);
    while ( y + h >= OSD::scrH - OSD_FONT_H) {
        mf_rows--;
        h = ((mf_rows + 1) * OSD_FONT_H) + 2;
        printf("Y: %d h: %d mf_rows: %d scrH: %d\n",y,h,mf_rows,scrH);
    };

    // Adjust begin_row & focus in case of values doesn't fit in current dialog size
    // printf("Focus: %d, Begin_row: %d, mf_rows: %d\n",(int) FileUtils::fileTypes[ftype].focus,(int) FileUtils::fileTypes[ftype].begin_row,(int) mf_rows);
    if (FileUtils::fileTypes[ftype].focus > mf_rows - 1) {
        FileUtils::fileTypes[ftype].begin_row += FileUtils::fileTypes[ftype].focus - (mf_rows - 1);
        FileUtils::fileTypes[ftype].focus = mf_rows - 1;
    } else
    if (FileUtils::fileTypes[ftype].focus + (FileUtils::fileTypes[ftype].begin_row - 2) < mf_rows) {
        FileUtils::fileTypes[ftype].focus += FileUtils::fileTypes[ftype].begin_row - 2;
        FileUtils::fileTypes[ftype].begin_row = 2;
    }

    // menu = title + "\n" + fdir + "\n";
    menu = title + "\n" + ( fdir.length() == 1 ? fdir : fdir.substr(0,fdir.length()-1)) + "\n";
    WindowDraw(); // Draw menu outline
    fd_PrintRow(1, IS_INFO);    // Path

reset:

    // Draw blank rows
    uint8_t row = 2;
    for (; row < mf_rows; row++) {
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        menuAt(row, 0);
        VIDEO::vga.print(std::string(cols, ' ').c_str());
    }

    // Compose shortcut help
    string StatusBar = " ";
    if (actions & FILEDIALOG_NEW) {
        StatusBar += FILE_DLG_NEW[Config::lang];
        StatusBar += " ";
    }

    if (actions & FILEDIALOG_MKDIR) {
        StatusBar += FILE_DLG_MKDIR[Config::lang];
        StatusBar += " ";
    }

    StatusBar += FILE_DLG_FIND[Config::lang];

    if (actions & FILEDIALOG_RENAME) {
        StatusBar += " ";
        StatusBar += FILE_DLG_RENAME[Config::lang];
    }

    if (actions & FILEDIALOG_DELETE) {
        StatusBar += " ";
        StatusBar += FILE_DLG_DELETE[Config::lang];
    }

    if (cols > (StatusBar.length() + 11 + 2)) { // 11 from elements counter + 2 from borders
        StatusBar += std::string(cols - StatusBar.length() - 12, ' ');
    } else {
        StatusBar = std::string(cols - 12, ' ');
    }

    // Print status bar
    menuAt(row, 0);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));

    if (FileUtils::fileTypes[ftype].fdMode)
        VIDEO::vga.print(std::string(cols, ' ').c_str());
    else {
        for (int i=0; i<StatusBar.length(); i++) {
            if (StatusBar[i] == '&') {
                VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(5, 0));
                VIDEO::vga.print(StatusBar[++i]);
                VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
            } else {
                VIDEO::vga.print(StatusBar[i]);
            }
        }
        VIDEO::vga.print(std::string(12, ' ').c_str());
    }

    // fdSearchRefresh = true;

    while(1) {

        // ESPectrum::showMemInfo("file dialog: before checking dir");

        fdCursorFlash = 0;

        reIndex = false;
        string filedir = FileUtils::MountPoint + fdir;

        std::vector<std::string> filexts;
        size_t pos = 0;
        string ss = FileUtils::fileTypes[ftype].fileExts;
        while ((pos = ss.find(",")) != std::string::npos) {
            filexts.push_back(ss.substr(0, pos));
            ss.erase(0, pos + 1);
        }
        filexts.push_back(ss.substr(0));

        unsigned long hash = 0; // Name checksum variables

        // Get Dir Stats
        int result = FileUtils::getDirStats(filedir, filexts, &hash, &elements, &ndirs);

        filexts.clear(); // Clear vector
        std::vector<std::string>().swap(filexts); // free memory

        if ( result == -1 ) {

            printf("Error opening %s\n",filedir.c_str());

            FileUtils::unmountSDCard();

            OSD::restoreBackbufferData();

            click();
            return "";

        }

        // Open dir file for read
        printf("Checking existence of index file %s\n",(filedir + FileUtils::fileTypes[ftype].indexFilename).c_str());
        dirfile = fopen((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), "r");
        if (dirfile == NULL) {
            printf("No dir file found: reindexing\n");
            reIndex = true;
        } else {
            // stat((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), &stat_buf);
            fseek(dirfile,0,SEEK_END);
            dirfilesize = ftell(dirfile);

            fseek(dirfile, dirfilesize - 20, SEEK_SET);

            char fhash[21];
            memset( fhash, '\0', sizeof(fhash));
            fgets(fhash, sizeof(fhash), dirfile);
            // printf("File Hash: %s\n",fhash);

            // If calc hash and file hash are different refresh dir index
            if ( getLong(fhash) != hash ||
                dirfilesize - 20 != FILENAMELEN * ( ndirs+elements + ( filedir != ( FileUtils::MountPoint + "/" ) ? 1 : 0 ) ) ) {
                reIndex = true;
            }
        }

        // ESPectrum::showMemInfo("file dialog: after checking dir");

        // Force reindex (for testing)
        // reIndex = ESPectrum::ESPtestvar ? true : reIndex;

        // There was no index or hashes are different: reIndex
        if (reIndex) {

            // ESPectrum::showMemInfo("file dialog: before reindex");

            if ( dirfile ) {
                fclose(dirfile);
                dirfile = nullptr;
            }

// #if 1
//             multi_heap_info_t info;
//             size_t ram_consumption;

//             heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task

//             printf("\n=======================================================\n");
//             printf("ORDENANDO CARPETA\n");
//             printf("=======================================================\n");
//             printf("\nTotal free bytes          : %d\n", info.total_free_bytes);
//             printf("Minimum free ever         : %d\n", info.minimum_free_bytes);

//             size_t minimum_before = info.minimum_free_bytes;

//             uint32_t time_start = esp_timer_get_time();
// #endif

            FileUtils::DirToFile(filedir, ftype, hash, ndirs + elements ); // Prepare filelist

// #if 1
//             uint32_t time_elapsed = esp_timer_get_time() - time_start;

//             heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task

//             printf("TIEMPO DE ORDENACION      : %6.2f segundos\n", (float)time_elapsed / 1000000);
//             printf("Total free bytes  despues : %d\n", info.total_free_bytes);
//             printf("Minimum free ever despues : %d\n", info.minimum_free_bytes);
//             printf("Consumo RAM               : %d\n", minimum_before - info.minimum_free_bytes);

//             printf("\n=======================================================\n");
// #endif
            // stat((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), &stat_buf);

            dirfile = fopen((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), "r");
            if (dirfile == NULL) {
                printf("Error opening index file\n");

                FileUtils::unmountSDCard();

                OSD::restoreBackbufferData();
                click();

                return "";
            }

            fseek(dirfile,0,SEEK_END);
            dirfilesize = ftell(dirfile);

            // Reset position
            FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;

            // ESPectrum::showMemInfo("file dialog: after reindex");

        }

        if (FileUtils::fileTypes[ftype].fdMode) {

            // Clean Status Bar
            menuAt(row, 0);
            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
            VIDEO::vga.print(std::string(StatusBar.length(), ' ').c_str());

            // Recalc items number
            long prevpos = ftell(dirfile);

            unsigned int foundcount = 0;
            fdSearchElements = 0;
            rewind(dirfile);
            char buf[FILENAMELEN+1];
            string search = FileUtils::fileTypes[ftype].fileSearch;
            std::transform(search.begin(), search.end(), search.begin(), ::toupper);
            while(1) {
                fgets(buf, sizeof(buf), dirfile);
                if (feof(dirfile)) break;
                if (buf[0] == ASCII_SPC) {
                    foundcount++;
                    // printf("%s",buf);
                }else {
                    char *p = buf; while(*p) *p++ = toupper(*p);
                    char *pch = strstr(buf, search.c_str());
                    if (pch != NULL) {
                        foundcount++;
                        fdSearchElements++;
                        // printf("%s",buf);
                    }
                }
            }

            if (foundcount) {
                // Redraw rows
                real_rows = foundcount + 2; // Add 2 for title and status bar
                virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
                last_begin_row = last_focus = 0;
                // FileUtils::fileTypes[ftype].focus = 2;
                // FileUtils::fileTypes[ftype].begin_row = 2;
                // fd_Redraw(title, fdir, ftype);
            } else {
                fseek(dirfile,prevpos,SEEK_SET);
            }

            fdSearchRefresh = false;

        } else {

            // real_rows = (stat_buf.st_size / FILENAMELEN) + 2; // Add 2 for title and status bar
            real_rows = (dirfilesize / FILENAMELEN) + 2; // Add 2 for title and status bar
            virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
            // printf("Real rows: %d; st_size: %d; Virtual rows: %d\n",real_rows,stat_buf.st_size,virtual_rows);

            last_begin_row = last_focus = 0;

            fdSearchElements = elements;

        }

        // printf("Focus: %d, Begin_row: %d, real_rows: %d, mf_rows: %d\n",(int) FileUtils::fileTypes[ftype].focus,(int) FileUtils::fileTypes[ftype].begin_row,(int) real_rows, (int) mf_rows);
        if ((real_rows > mf_rows) && ((FileUtils::fileTypes[ftype].begin_row + mf_rows - 2) > real_rows)) {
            FileUtils::fileTypes[ftype].focus += (FileUtils::fileTypes[ftype].begin_row + mf_rows - 2) - real_rows;
            FileUtils::fileTypes[ftype].begin_row = real_rows - (mf_rows - 2);
            // printf("Focus: %d, BeginRow: %d\n",FileUtils::fileTypes[ftype].focus,FileUtils::fileTypes[ftype].begin_row);
        }

        fd_Redraw(title, fdir, ftype); // Draw content

        // Focus line scroll position
        fdScrollPos = 0;
        timeStartScroll = 0;
        timeScroll = 0;
        hfocus = 0;
        uint8_t hwide = (real_rows > virtual_rows ? cols - 3 : cols - 2);
        int timeStartScrollMax = hwide >= 46 ? 150 : 200;

        fabgl::VirtualKeyItem Menukey;
        while (1) {

            if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

            ESPectrum::readKbdJoy();

            // Process external keyboard
            if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {

                timeStartScroll = 0;
                timeScroll = 0;
                fdScrollPos = 0;

                // Print elements
                VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));

                unsigned int elem = FileUtils::fileTypes[ftype].fdMode ? fdSearchElements : elements;
                if (elem) {
                    // menuAt(mf_rows, cols - (real_rows > virtual_rows ? 13 : 12));
                    menuAt(mf_rows, cols - 12);
                    char elements_txt[13];
                    int nitem = (FileUtils::fileTypes[ftype].begin_row + FileUtils::fileTypes[ftype].focus ) - (4 + ndirs) + (fdir.length() == 1);
                    snprintf(elements_txt, sizeof(elements_txt), "%d/%d ", nitem > 0 ? nitem : 0 , elem);
                    VIDEO::vga.print(std::string(12 - strlen(elements_txt), ' ').c_str());
                    VIDEO::vga.print(elements_txt);
                } else {
                    menuAt(mf_rows, cols - 12);
                    VIDEO::vga.print(std::string(12,' ').c_str());
                }

                if (ESPectrum::readKbd(&Menukey)) {

                    if (!Menukey.down) continue;

                    // Search first ocurrence of letter if we're not on that letter yet
                    if ((!Menukey.SHIFT) && (((Menukey.vk >= fabgl::VK_a) && (Menukey.vk <= fabgl::VK_Z)) || Menukey.vk == fabgl::VK_SPACE || ((Menukey.vk >= fabgl::VK_0) && (Menukey.vk <= fabgl::VK_9)))) {

                        int fsearch;
                        if (Menukey.vk==fabgl::VK_SPACE && FileUtils::fileTypes[ftype].fdMode)
                            fsearch = ASCII_SPC;
                        else if (Menukey.vk<=fabgl::VK_9)
                            fsearch = Menukey.vk + 46;
                        else if (Menukey.vk<=fabgl::VK_z)
                            fsearch = Menukey.vk + 75;
                        else if (Menukey.vk<=fabgl::VK_Z)
                            fsearch = Menukey.vk + 17;

                        if (FileUtils::fileTypes[ftype].fdMode) {

                            if (FileUtils::fileTypes[ftype].fileSearch.length() < MAXSEARCHLEN) {
                                FileUtils::fileTypes[ftype].fileSearch += char(fsearch);
                                fdSearchRefresh = true;
                                click();
                            }

                        } else {

                            string rowSearch = rowGet(menu,FileUtils::fileTypes[ftype].focus);
                            uint8_t letra = rowSearch[0];
                            uint8_t atDir = 0;
                            if (letra == ASCII_SPC) {
                                letra = rowSearch[1];
                                atDir = 1;
                            }
                            if (toupper(letra) >= toupper(char(fsearch))) atDir ^= 1;

                            // Seek first ocurrence of letter/number
                            long prevpos = ftell(dirfile);
                            char buf[FILENAMELEN+1];
                            int cnt = 0;
                            int pass = 0;
                            fseek(dirfile,0,SEEK_SET);
                            while(1) {
                                fgets(buf, sizeof(buf), dirfile);
                                // printf("%c %d\n",buf[0],int(buf[0]));
                                if (atDir) {
                                    if ((buf[0] == ASCII_SPC) && (toupper(buf[1]) == toupper(char(fsearch)))) break;
                                } else {
                                    if (toupper(buf[0]) == toupper(char(fsearch))) break;
                                }
                                cnt++;
                                if(feof(dirfile)) {
                                    if (pass) break;
                                    fseek(dirfile,0,SEEK_SET);
                                    cnt = 0;
                                    pass = 1;
                                    atDir ^= 1;
                                }
                            }
                            // printf("Cnt: %d Letra: %d\n",cnt,int(letra));
                            if (!feof(dirfile)) {
                                last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                                last_focus = FileUtils::fileTypes[ftype].focus;
                                if (real_rows > virtual_rows) {
                                    int m = cnt + virtual_rows - real_rows;
                                    if (m > 0) {
                                        FileUtils::fileTypes[ftype].focus = m + 2;
                                        FileUtils::fileTypes[ftype].begin_row = cnt - m + 2;
                                    } else {
                                        FileUtils::fileTypes[ftype].focus = 2;
                                        FileUtils::fileTypes[ftype].begin_row = cnt + 2;
                                    }
                                } else {
                                    FileUtils::fileTypes[ftype].focus = cnt + 2;
                                    FileUtils::fileTypes[ftype].begin_row = 2;
                                }
                                // printf("Real rows: %d; Virtual rows: %d\n",real_rows,virtual_rows);
                                // printf("Focus: %d, Begin_row: %d\n",(int) FileUtils::fileTypes[ftype].focus,(int) FileUtils::fileTypes[ftype].begin_row);
                                fd_Redraw(title,fdir,ftype);
                                click();
                            } else
                                fseek(dirfile,prevpos,SEEK_SET);

                        }

                    } else if ((Menukey.SHIFT && (Menukey.vk == FileDlgKeys[Config::lang][0] || Menukey.vk == FileDlgKeys[Config::lang][1])) && (!FileUtils::fileTypes[ftype].fdMode) && (actions & FILEDIALOG_NEW)) {

                        ////////////////////////////////////////////////////////////////////////////////////////////////////
                        // New file
                        ////////////////////////////////////////////////////////////////////////////////////////////////////

                        uint8_t disk_res = DLG_YES;
                        if (ftype == DISK_DSKFILE) {
                            disk_res = msgDialog("New TRD disk","Choose number of tracks and sides",MSGDIALOG_CUSTOM,OSD_MSGDIALOG_BETADISKNEW[Config::lang],1);
                        }

                        if (disk_res != DLG_CANCEL) {

                            // Clean status bar
                            menuAt(mf_rows, 0);
                            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            VIDEO::vga.print(std::string(StatusBar.length(), ' ').c_str());

                            string new_file = "";

                            while (1) {

                                new_file = OSD::input( 1, mf_rows, FILE_DLG_INPUT[Config::lang], new_file, cols - 19 , 123, zxColor(7,1), zxColor(5,0), true );

                                trim(new_file);

                                if ( new_file == "" ) break;

                                uint8_t res = DLG_YES;
                                struct stat stat_buf;
                                if (ftype == DISK_TAPFILE) {
                                    new_file = "N" + new_file + ".tap";
                                } else
                                if (ftype == DISK_DSKFILE) {
                                    if (stat((FileUtils::MountPoint + FileUtils::DSK_Path + new_file + ".trd").c_str(), &stat_buf) == 0)
                                        res = OSD::msgDialog(OSD_TAPE_SAVE_EXIST[Config::lang],OSD_DLG_SURE[Config::lang], MSGDIALOG_YESNO);
                                    if (res == DLG_YES) new_file = char(disk_res + 48) + new_file + ".trd";
                                } else
                                if (ftype == DISK_ESPFILE) {
                                    if (stat((FileUtils::MountPoint + FileUtils::ESP_Path + new_file + ".esp").c_str(), &stat_buf) == 0)
                                        res = OSD::msgDialog(OSD_PSNA_SAVE[Config::lang], OSD_PSNA_EXISTS[Config::lang], MSGDIALOG_YESNO);
                                    if (res == DLG_YES) new_file = "N" + new_file + ".esp";
                                }

                                if (res == DLG_YES) {
                                    fclose(dirfile);
                                    dirfile = NULL;
                                    return new_file;
                                }

                            }

                            // Restore status bar
                            menuAt(mf_rows, 0);
                            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            for (int i=0; i<StatusBar.length(); i++) {
                                if (StatusBar[i] == '&') {
                                    VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(5, 0));
                                    VIDEO::vga.print(StatusBar[++i]);
                                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                                } else
                                    VIDEO::vga.print(StatusBar[i]);
                            }

                        }

                    } else if ((Menukey.SHIFT && (Menukey.vk == FileDlgKeys[Config::lang][2] || Menukey.vk == FileDlgKeys[Config::lang][3])) && (!FileUtils::fileTypes[ftype].fdMode) && (actions & FILEDIALOG_MKDIR)) {

                        ////////////////////////////////////////////////////////////////////////////////////////////////////
                        // New dir (Mkdir)
                        ////////////////////////////////////////////////////////////////////////////////////////////////////

                        // Clean status bar
                        menuAt(mf_rows, 0);
                        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                        VIDEO::vga.print(std::string(StatusBar.length(), ' ').c_str());

                        string new_file = "";

                        while (1) {

                            new_file = OSD::input( 1, mf_rows, FILE_DLG_INPUT[Config::lang], new_file, cols - 19 , 123, zxColor(7,1), zxColor(5,0), true );

                            trim(new_file);

                            if ( new_file == "" ) break;


                            // Create dir if it doesn't exist
                            string dir = FileUtils::MountPoint + fdir + new_file;

                            struct stat stat_buf;
                            if (stat(dir.c_str(), &stat_buf) == 0) {
                                // Mensaje directorio existe
                                OSD::osdCenteredMsg(OSD_DIREXISTS_WARN[Config::lang], LEVEL_WARN);
                                continue;
                            }

                            if (mkdir(dir.c_str(),0775) != 0) {
                                // Mensaje error creacion directorio
                                string err = OSD_FILEERROR_WARN[Config::lang];
                                err += strerror(errno);
                                OSD::osdCenteredMsg(err.c_str(), LEVEL_WARN);
                                break;
                            }

                            fclose(dirfile);
                            dirfile = NULL;
                            goto reset;

                        }

                        // Restore status bar
                        menuAt(mf_rows, 0);
                        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                        for (int i=0; i<StatusBar.length(); i++) {
                            if (StatusBar[i] == '&') {
                                VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(5, 0));
                                VIDEO::vga.print(StatusBar[++i]);
                                VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            } else
                                VIDEO::vga.print(StatusBar[i]);
                        }

                    } else if (Menukey.SHIFT && (Menukey.vk == FileDlgKeys[Config::lang][4] || Menukey.vk == FileDlgKeys[Config::lang][5])) {

                        ////////////////////////////////////////////////////////////////////////////////////////////////////
                        // Find
                        ////////////////////////////////////////////////////////////////////////////////////////////////////

                        FileUtils::fileTypes[ftype].fdMode ^= 1;

                        if (FileUtils::fileTypes[ftype].fdMode) {

                            // Clean status bar
                            menuAt(mf_rows, 0);
                            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            // VIDEO::vga.print( ftype == DISK_TAPFILE ? "            " "                      " : "                      " );
                            // VIDEO::vga.print(std::string(cols, ' ').c_str());
                            VIDEO::vga.print(std::string(StatusBar.length(), ' ').c_str());

                            fdCursorFlash = 63;

                            // menuAt(mfrows + (Config::aspect_letterbox ? 0 : 1), 1);
                            // VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            // VIDEO::vga.print(Config::lang ? "B\xA3sq: " : "Find: ");
                            // VIDEO::vga.print(FileUtils::fileTypes[ftype].fileSearch.c_str());
                            // VIDEO::vga.setTextColor(zxColor(5, 0), zxColor(7, 1));
                            // VIDEO::vga.print("K");
                            // VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            // VIDEO::vga.print(std::string(16 - FileUtils::fileTypes[ftype].fileSearch.size(), ' ').c_str());

                            fdSearchRefresh = FileUtils::fileTypes[ftype].fileSearch != "";

                        } else {

                            // Restore status bar
                            menuAt(mf_rows, 0);
                            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            for (int i=0; i<StatusBar.length(); i++) {
                                if (StatusBar[i] == '&') {
                                    VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(5, 0));
                                    VIDEO::vga.print(StatusBar[++i]);
                                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                                } else
                                    VIDEO::vga.print(StatusBar[i]);
                            }
                            // VIDEO::vga.print(StatusBar.c_str());

                            if (FileUtils::fileTypes[ftype].fileSearch != "") {
                                real_rows = (dirfilesize / FILENAMELEN) + 2; // Add 2 for title and status bar
                                virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
                                last_begin_row = last_focus = 0;
                                FileUtils::fileTypes[ftype].focus = 2;
                                FileUtils::fileTypes[ftype].begin_row = 2;
                                fd_Redraw(title, fdir, ftype);
                            }

                        }

                        click();

                    } else if ((Menukey.SHIFT && (Menukey.vk == FileDlgKeys[Config::lang][6] || Menukey.vk == FileDlgKeys[Config::lang][7])) && (!FileUtils::fileTypes[ftype].fdMode) && (actions & FILEDIALOG_RENAME)) {

                        ////////////////////////////////////////////////////////////////////////////////////////////////////
                        // Rename
                        ////////////////////////////////////////////////////////////////////////////////////////////////////

                        string fname = rowGet(menu,FileUtils::fileTypes[ftype].focus);

                        if (fname[0] != ASCII_SPC) {

                            fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_NORMAL);

                            rtrim(fname);
                            string filext = fname.substr(fname.length()-4);
                            string newname = fname.substr(0, fname.length() - 4);

                            while (1) {

                                newname = OSD::input( 1, FileUtils::fileTypes[ftype].focus, "", newname, cols - (real_rows > virtual_rows ? 4 : 3), 123, zxColor(0,0), zxColor(7,0), true );

                                trim(newname);

                                if ( newname == "" || fname == (newname + filext))  break;

                                if (rename((FileUtils::MountPoint + fdir + fname).c_str(),(FileUtils::MountPoint + fdir + newname + filext).c_str()) == ERR_OK) {

                                    fclose(dirfile);
                                    dirfile = NULL;
                                    goto reset;

                                } else {

                                    if (errno == EEXIST) {

                                        OSD::osdCenteredMsg(OSD_FILEEXISTS_WARN[Config::lang], LEVEL_WARN);

                                    } else {

                                        string err = OSD_FILEERROR_WARN[Config::lang];
                                        err += strerror(errno);
                                        OSD::osdCenteredMsg(err.c_str(), LEVEL_WARN);

                                        break;

                                    }
                                }

                            }

                            fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED); // Redraw row

                        } else { // Rename dir

                            rtrim(fname);
                            string newname = fname.substr(1);
                            fname = newname;

                            if (fname.compare(" ..") != 0) {

                                fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_NORMAL);

                                while (1) {

                                    newname = OSD::input( 1, FileUtils::fileTypes[ftype].focus, "", newname, cols - (real_rows > virtual_rows ? 10 : 9), 123, zxColor(0,0), zxColor(7,0), true );

                                    trim(newname);

                                    if ( newname == "" || fname == newname)  break;

                                    if (rename((FileUtils::MountPoint + fdir + fname).c_str(),(FileUtils::MountPoint + fdir + newname).c_str()) == ERR_OK) {

                                        fclose(dirfile);
                                        dirfile = NULL;
                                        goto reset;

                                    } else {

                                        if (errno == EEXIST) {

                                            OSD::osdCenteredMsg(OSD_DIREXISTS_WARN[Config::lang], LEVEL_WARN);

                                        } else {

                                            string err = OSD_FILEERROR_WARN[Config::lang];
                                            err += strerror(errno);
                                            OSD::osdCenteredMsg(err.c_str(), LEVEL_WARN);

                                            break;

                                        }
                                    }

                                }

                                fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED); // Redraw row

                            }

                        }

                    } else if ((Menukey.SHIFT && (Menukey.vk == FileDlgKeys[Config::lang][8] || Menukey.vk == FileDlgKeys[Config::lang][9])) &&  (!FileUtils::fileTypes[ftype].fdMode) && (actions & FILEDIALOG_DELETE)) {

                        ////////////////////////////////////////////////////////////////////////////////////////////////////
                        // Delete
                        ////////////////////////////////////////////////////////////////////////////////////////////////////

                        filedir = rowGet(menu,FileUtils::fileTypes[ftype].focus);
                        // printf("%s\n",filedir.c_str());
                        if (filedir[0] != ASCII_SPC) {
                            click();
                            rtrim(filedir);
                            if ( !access(( FileUtils::MountPoint + fdir + filedir ).c_str(), W_OK) ) {
                                string title = MENU_DELETE_CURRENT_FILE[Config::lang];
                                string msg = OSD_DLG_SURE[Config::lang];
                                uint8_t res = msgDialog(title,msg,MSGDIALOG_YESNO);
                                menu_saverect = true;

                                if (res == DLG_YES) {

                                    if (ftype == DISK_TAPFILE) {
                                        printf("File selected: -->%s<--\n", (FileUtils::MountPoint + fdir + filedir).c_str());
                                        printf("File inserted: -->%s<--\n", (Tape::tapeFilePath + Tape::tapeFileName).c_str());
                                        if ( (FileUtils::MountPoint + fdir + filedir) == (Tape::tapeFilePath + Tape::tapeFileName) ) {
                                            printf("Ejecting tape before deleting it\n");
                                            Tape::Eject();
                                        };
                                    } else if (ftype == DISK_DSKFILE) {
                                        // Eject disk if is inserted
                                        if ( ESPectrum::fdd.disk[ESPectrum::fdd.diskS] && ((FileUtils::MountPoint + fdir + filedir) == ESPectrum::fdd.disk[ESPectrum::fdd.diskS]->fname) ) {
                                            printf("Ejecting disk before deleting it\n");
                                            wdDiskEject(&ESPectrum::fdd,ESPectrum::fdd.diskS);
                                        };
                                    }

                                    remove(( FileUtils::MountPoint + fdir + filedir ).c_str());
                                    fd_Redraw(title, fdir, ftype);
                                    menu_saverect = true;
                                    fclose(dirfile);
                                    dirfile = NULL;
                                    goto reset;
                                }
                            } else {
                                OSD::osdCenteredMsg(OSD_READONLY_FILE_WARN[Config::lang], LEVEL_WARN);
                            }
                            click();

                        } else {

                            // Rmdir

                            trim(filedir);

                            if (filedir.compare("..") != 0) {

                                click();

                                uint8_t res = msgDialog(MENU_DELETE_CURRENT_DIR[Config::lang],OSD_DLG_SURE[Config::lang],MSGDIALOG_YESNO);
                                if (res == DLG_YES) {

                                    // trim(filedir);

                                    // Check if dir is empty (or there are only ESPectrum index files)
                                    struct dirent *de;
                                    DIR *dr = opendir((FileUtils::MountPoint + fdir + filedir).c_str());
                                    if (dr != NULL) {

                                        const char dot[] = ".";
                                        while ((de = readdir(dr)) != NULL) {
                                            // printf("%s\n", de->d_name);
                                            if (de->d_type == DT_DIR) res = 0;
                                            if (de->d_name[0] != dot[0]) res = 0;
                                        }

                                        closedir(dr);

                                        if (res) {

                                            DIR *dr1 = opendir((FileUtils::MountPoint + fdir + filedir).c_str());
                                            while ((de = readdir(dr1)) != NULL) {
                                                // printf("DOT FILE %s\n", de->d_name);
                                                remove((FileUtils::MountPoint + fdir + filedir + "/" + de->d_name).c_str());
                                            }
                                            closedir(dr1);

                                            if (rmdir((FileUtils::MountPoint + fdir + filedir).c_str()) == 0) {
                                                fclose(dirfile);
                                                dirfile = NULL;
                                                goto reset;
                                            }

                                        }
                                    }

                                    if (!res) {
                                        OSD::osdCenteredMsg(OSD_DIRNOTEMPTY_WARN[Config::lang], LEVEL_WARN);
                                    } else {
                                        string err = OSD_FILEERROR_WARN[Config::lang];
                                        err += strerror(errno);
                                        OSD::osdCenteredMsg(err.c_str(), LEVEL_WARN);
                                    }

                                }

                            }

                        }
                    } else if (Menukey.vk == fabgl::VK_UP || Menukey.vk == fabgl::VK_JOY1UP || Menukey.vk == fabgl::VK_JOY2UP) {
                        if (FileUtils::fileTypes[ftype].focus == 2 && FileUtils::fileTypes[ftype].begin_row > 2) {
                            last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                            FileUtils::fileTypes[ftype].begin_row--;
                            fd_Redraw(title, fdir, ftype);
                        } else if (FileUtils::fileTypes[ftype].focus > 2) {
                            last_focus = FileUtils::fileTypes[ftype].focus;
                            if (hfocus > 0) {
                                FileUtils::fileTypes[ftype].focus--;
                                fd_Redraw(title, fdir, ftype);
                                hfocus = 0;
                            } else {
                                fd_PrintRow(FileUtils::fileTypes[ftype].focus--, IS_NORMAL);
                                fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
                            }
                            // printf("Focus: %d, Lastfocus: %d\n",FileUtils::fileTypes[ftype].focus,(int) last_focus);
                        }
                        click();
                    } else if (Menukey.vk == fabgl::VK_DOWN || Menukey.vk == fabgl::VK_JOY1DOWN || Menukey.vk == fabgl::VK_JOY2DOWN) {
                        if (FileUtils::fileTypes[ftype].focus == virtual_rows - 1 && FileUtils::fileTypes[ftype].begin_row + virtual_rows - 2 < real_rows) {
                            last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                            FileUtils::fileTypes[ftype].begin_row++;
                            fd_Redraw(title, fdir, ftype);
                        } else if (FileUtils::fileTypes[ftype].focus < virtual_rows - 1) {
                            last_focus = FileUtils::fileTypes[ftype].focus;
                            if (hfocus > 0) {
                                FileUtils::fileTypes[ftype].focus++;
                                fd_Redraw(title, fdir, ftype);
                                hfocus = 0;
                            } else {
                                fd_PrintRow(FileUtils::fileTypes[ftype].focus++, IS_NORMAL);
                                fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
                            }
                            // printf("Focus: %d, Lastfocus: %d\n",FileUtils::fileTypes[ftype].focus,(int) last_focus);
                        }
                        click();
                    } else if (Menukey.vk == fabgl::VK_PAGEUP || Menukey.vk == fabgl::VK_LEFT || Menukey.vk == fabgl::VK_JOY1LEFT || Menukey.vk == fabgl::VK_JOY2LEFT) {
                        if (FileUtils::fileTypes[ftype].begin_row > virtual_rows) {
                            FileUtils::fileTypes[ftype].focus = 2;
                            FileUtils::fileTypes[ftype].begin_row -= virtual_rows - 2;
                        } else {
                            if (FileUtils::fileTypes[ftype].begin_row == 2 && FileUtils::fileTypes[ftype].focus == 2) {
                                if (fdir != "/") {
                                    fclose(dirfile);
                                    dirfile = NULL;
                                    fdir.pop_back();
                                    string prevdir = fdir.substr(fdir.find_last_of("/") + 1, fdir.length());
                                    fdir = fdir.substr(0,fdir.find_last_of("/") + 1);
                                    FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;
                                    click();
                                    break;
                                } else {
                                    if (menu_level > 0) {
                                        // Exit if we press left and we're on top of list
                                        OSD::restoreBackbufferData();
                                        fclose(dirfile);
                                        dirfile = NULL;
                                        click();
                                        return "";
                                    }
                                }
                            } else {
                                FileUtils::fileTypes[ftype].focus = 2;
                                FileUtils::fileTypes[ftype].begin_row = 2;
                            }
                        }
                        fd_Redraw(title, fdir, ftype);
                        click();
                    } else if (Menukey.vk == fabgl::VK_PAGEDOWN || Menukey.vk == fabgl::VK_RIGHT || Menukey.vk == fabgl::VK_JOY1RIGHT || Menukey.vk == fabgl::VK_JOY2RIGHT) {
                        if (real_rows - FileUtils::fileTypes[ftype].begin_row  - virtual_rows > virtual_rows) {
                            FileUtils::fileTypes[ftype].focus = 2;
                            FileUtils::fileTypes[ftype].begin_row += virtual_rows - 2;
                        } else {
                            FileUtils::fileTypes[ftype].focus = virtual_rows - 1;
                            FileUtils::fileTypes[ftype].begin_row = real_rows - virtual_rows + 2;
                        }
                        fd_Redraw(title, fdir, ftype);
                        click();
                    } else if (Menukey.vk == fabgl::VK_HOME) {
                        last_focus = FileUtils::fileTypes[ftype].focus;
                        last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                        FileUtils::fileTypes[ftype].focus = 2;
                        FileUtils::fileTypes[ftype].begin_row = 2;
                        fd_Redraw(title, fdir, ftype);
                        click();
                    } else if (Menukey.vk == fabgl::VK_END) {
                        last_focus = FileUtils::fileTypes[ftype].focus;
                        last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                        FileUtils::fileTypes[ftype].focus = virtual_rows - 1;
                        FileUtils::fileTypes[ftype].begin_row = real_rows - virtual_rows + 2;
                        // printf("Focus: %d, Lastfocus: %d\n",FileUtils::fileTypes[ftype].focus,(int) last_focus);
                        fd_Redraw(title, fdir, ftype);
                        click();
                    } else if (Menukey.vk == fabgl::VK_BACKSPACE) {
                        if (FileUtils::fileTypes[ftype].fdMode) {
                            if (FileUtils::fileTypes[ftype].fileSearch.length()) {
                                FileUtils::fileTypes[ftype].fileSearch.pop_back();
                                fdSearchRefresh = true;
                                click();
                            }
                        } else {
                            if (fdir != "/") {

                                fclose(dirfile);
                                dirfile = NULL;

                                fdir.pop_back();
                                string prevdir = fdir.substr(fdir.find_last_of("/") + 1, fdir.length());
                                printf("Prevdir: %s\n",prevdir.c_str());
                                fdir = fdir.substr(0,fdir.find_last_of("/") + 1);




                                FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;
                                // printf("Fdir: %s\n",fdir.c_str());

                                click();

                                break;

                            }
                        }
                    } else if (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C) {

                        fclose(dirfile);
                        dirfile = NULL;

                        filedir = rowGet(menu,FileUtils::fileTypes[ftype].focus);
                        // printf("%s\n",filedir.c_str());
                        if (filedir[0] == ASCII_SPC) {
                            if (filedir[1] == ASCII_SPC) {
                                fdir.pop_back();
                                fdir = fdir.substr(0,fdir.find_last_of("/") + 1);
                            } else {
                                filedir.erase(0,1);
                                trim(filedir);
                                fdir = fdir + filedir + "/";
                            }
                            FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;
                            // printf("Fdir: %s\n",fdir.c_str());
                            break;
                        } else {

                            OSD::restoreBackbufferData();

                            rtrim(filedir);
                            click();

                            if ((Menukey.SHIFT && Menukey.vk == fabgl::VK_RETURN) || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C)
                                return "S" + filedir;
                            else
                                return "R" + filedir;

                        }

                    } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {

                        OSD::restoreBackbufferData();

                        fclose(dirfile);
                        dirfile = NULL;
                        click();
                        return "";
                    }
                }

            } else {

                if (timeStartScroll < timeStartScrollMax) {
                    timeStartScroll++;
                    if (timeStartScroll == timeStartScrollMax) {
                        // Draw filename helper if wide > 43 so filename fits in max. three lines
                        if (hwide >= 46) {
                            hfocus = FileUtils::fileTypes[ftype].focus;
                            string fname = rowGet(menu,hfocus);
                            if(fname[0] != ASCII_SPC) {
                                trim(fname);

                                // Get file size
                                struct stat stat_buf;
                                stat((FileUtils::MountPoint + fdir + fname).c_str(), &stat_buf);
                                long fsize = stat_buf.st_size;
                                string fsize_str = to_string(fsize);

                                fname = "Name " + fname;
                                // if (fname.length() > hwide) {
                                    // Draw window
                                    int hlines = (fname.length() / hwide) + 3;
                                    hfocus = hfocus < (mfrows - hlines) ? hfocus + 1 : hfocus - (hlines + 1);
                                    int infox = x + 4;
                                    int infoy = y + ( hfocus * OSD_FONT_H) + 4;
                                    int infoh = hlines * OSD_FONT_H;
                                    int infow = (hwide + 1) * OSD_FONT_W;
                                    VIDEO::vga.rect(infox, infoy, infow, infoh, zxColor(0, 0));
                                    VIDEO::vga.fillRect(infox + 1, infoy + 1, infow - 2, infoh - 2,zxColor(5, 0));

                                    // printf("Focus: %d, Mfrows: %d, hlines: %d, filesize: %s\n",hfocus,mfrows,hlines,fsize_str.c_str());

                                    string fpart;

                                    VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(5, 0));
                                    menuAt(hfocus + 1, 1);
                                    VIDEO::vga.print( "Type ");
                                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                                    fpart = FileUtils::getLCaseExt(fname);
                                    std::transform(fpart.begin(), fpart.end(), fpart.begin(), ::toupper);
                                    fpart += " file";
                                    VIDEO::vga.print( fpart.c_str() );

                                    VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(5, 0));
                                    VIDEO::vga.print( " Size ");
                                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                                    fpart = fsize_str; // + " bytes";
                                    VIDEO::vga.print( fpart.c_str() );

                                    VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(5, 0));
                                    fpart = fname.substr(0,5);
                                    menuAt(hfocus + 2, 1);
                                    VIDEO::vga.print( fpart.c_str() );

                                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                                    fpart = fname.substr(5,hwide - 5);
                                    menuAt(hfocus + 2, 6);
                                    VIDEO::vga.print( fpart.c_str() );

                                    if (fname.length() > hwide) {
                                        fpart = fname.substr(hwide, hwide);
                                        menuAt(hfocus + 3, 1);
                                        VIDEO::vga.print( fpart.c_str() );
                                    }
                                    if (fname.length() > (hwide << 1)) {
                                        fpart = fname.substr(hwide << 1, hwide);
                                        menuAt(hfocus + 4, 1);
                                        VIDEO::vga.print( fpart.c_str() );
                                    }
                                // }
                            }
                        }
                    }
                }

            }

            // Scroll focused line if signaled
            if (hfocus == 0 && timeStartScroll == timeStartScrollMax) {
                timeScroll++;
                if (timeScroll == 50) {
                    fdScrollPos++;
                    fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
                    timeScroll = 0;
                }
            }

            if (FileUtils::fileTypes[ftype].fdMode) {

                if ((++fdCursorFlash & 0xf) == 0) {
                    menuAt(mf_rows, 1);
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                    string findprompt = FILE_DLG_FINDPROMPT[Config::lang];
                    for (int i=0; i<findprompt.length(); i++) {
                        if (findprompt[i] == '&') {
                            VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(5, 0));
                            VIDEO::vga.print(findprompt[++i]);
                            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                        } else
                            VIDEO::vga.print(findprompt[i]);
                    }

                    int ss_siz = FileUtils::fileTypes[ftype].fileSearch.size();
                    int max_siz = cols - 20;
                    if (max_siz > MAXSEARCHLEN) max_siz = MAXSEARCHLEN;
                    if (ss_siz < max_siz)
                        VIDEO::vga.print(FileUtils::fileTypes[ftype].fileSearch.c_str());
                    else
                        VIDEO::vga.print(FileUtils::fileTypes[ftype].fileSearch.substr(ss_siz - max_siz).c_str());


                    if (fdCursorFlash > 63) {
                        VIDEO::vga.setTextColor(zxColor(5, 0), zxColor(7, 1));
                        if (fdCursorFlash == 128) fdCursorFlash = 0;
                    }
                    VIDEO::vga.print("L");
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));

                    if (ss_siz < max_siz) VIDEO::vga.print(std::string(max_siz - ss_siz, ' ').c_str());

                }

                if (fdSearchRefresh) {

                    // Recalc items number
                    long prevpos = ftell(dirfile);

                    unsigned int foundcount = 0;
                    fdSearchElements = 0;
                    rewind(dirfile);
                    char buf[FILENAMELEN+1];
                    string search = FileUtils::fileTypes[ftype].fileSearch;
                    std::transform(search.begin(), search.end(), search.begin(), ::toupper);
                    while(1) {
                        fgets(buf, sizeof(buf), dirfile);
                        if (feof(dirfile)) break;
                        if (buf[0] == ASCII_SPC) {
                                foundcount++;
                                // printf("%s",buf);
                        }else {
                            char *p = buf; while(*p) *p++ = toupper(*p);
                            char *pch = strstr(buf, search.c_str());
                            if (pch != NULL) {
                                foundcount++;
                                fdSearchElements++;
                                // printf("%s",buf);
                            }
                        }
                    }

                    if (foundcount) {
                        // Redraw rows
                        real_rows = foundcount + 2; // Add 2 for title and status bar
                        virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
                        last_begin_row = last_focus = 0;
                        FileUtils::fileTypes[ftype].focus = 2;
                        FileUtils::fileTypes[ftype].begin_row = 2;
                        fd_Redraw(title, fdir, ftype);
                    } else {
                        fseek(dirfile,prevpos,SEEK_SET);
                    }

                    fdSearchRefresh = false;
                }

            } /*else {

                if (cols > (Config::lang ? 32 : 28) + 14) {

                    menuAt(mfrows + (Config::aspect_16_9 ? 0 : 1), 1);
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                    if ( ftype == DISK_TAPFILE ) { // Dirty hack
                        VIDEO::vga.print(Config::lang ? "F2 Nuevo | " : "F2 New | " );
                    }
                    VIDEO::vga.print(Config::lang ? "F3 Buscar | F8 Borrar" : "F3 Find | F8 Delete" );

                }

            }*/

            vTaskDelay(5 / portTICK_PERIOD_MS);

        }

    }

}

// Redraw inside rows
void OSD::fd_Redraw(string title, string fdir, uint8_t ftype) {

    if ((FileUtils::fileTypes[ftype].focus != last_focus) || (FileUtils::fileTypes[ftype].begin_row != last_begin_row)) {

        // printf("fd_Redraw\n");

        // Read bunch of rows
        menu = title + "\n" + ( fdir.length() == 1 ? fdir : fdir.substr(0,fdir.length()-1)) + "\n";
        char buf[FILENAMELEN+1];
        if (FileUtils::fileTypes[ftype].fdMode == 0 || FileUtils::fileTypes[ftype].fileSearch == "") {
            fseek(dirfile, (FileUtils::fileTypes[ftype].begin_row - 2) * FILENAMELEN, SEEK_SET);
            for (int i = 2; i < virtual_rows; i++) {
                fgets(buf, sizeof(buf), dirfile);
                if (feof(dirfile)) break;
                menu += buf;
            }
        } else {
            rewind(dirfile);
            int i = 2;
            int count = 2;
            string search = FileUtils::fileTypes[ftype].fileSearch;
            std::transform(search.begin(), search.end(), search.begin(), ::toupper);
            char upperbuf[FILENAMELEN+1];
            while (1) {
                fgets(buf, sizeof(buf), dirfile);
                if (feof(dirfile)) break;
                if (buf[0] == ASCII_SPC) {
                    if (i >= FileUtils::fileTypes[ftype].begin_row) {
                        menu += buf;
                        if (++count == virtual_rows) break;
                    }
                    i++;
                } else {
                    for(int i=0; buf[i]; i++) upperbuf[i] = toupper(buf[i]);
                    char *pch = strstr(upperbuf, search.c_str());
                    if (pch != NULL) {
                        if (i >= FileUtils::fileTypes[ftype].begin_row) {
                            menu += buf;
                            if (++count == virtual_rows) break;
                        }
                        i++;
                    }
                }
            }
        }

        fd_PrintRow(1, IS_INFO); // Print status bar

        uint8_t row = 2;
        for (; row < virtual_rows; row++) {
            if (row == FileUtils::fileTypes[ftype].focus) {
                fd_PrintRow(row, IS_FOCUSED);
            } else {
                fd_PrintRow(row, IS_NORMAL);
            }
        }

        if (real_rows > virtual_rows) {
            menuScrollBar(FileUtils::fileTypes[ftype].begin_row);
        } else {
            for (; row < mf_rows; row++) {
                VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                menuAt(row, 0);
                VIDEO::vga.print(std::string(cols, ' ').c_str());
            }
        }

        last_focus = FileUtils::fileTypes[ftype].focus;
        last_begin_row = FileUtils::fileTypes[ftype].begin_row;
    }

}

// Print a virtual row
void OSD::fd_PrintRow(uint8_t virtual_row_num, uint8_t line_type) {

    uint8_t margin;

    string line = rowGet(menu, virtual_row_num);

    bool isDir = (line[0] == ASCII_SPC);

    trim(line);

    switch (line_type) {
    case IS_TITLE:
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
        margin = 2;
        break;
    case IS_INFO:
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    case IS_FOCUSED:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    default:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
    }

    menuAt(virtual_row_num, 0);

    VIDEO::vga.print(" ");

    if (isDir) {

        // Directory
        if (line.length() <= cols - margin - 6)
            line = line + std::string(cols - margin - line.length(), ' ');
        else
            if (line_type == IS_FOCUSED) {
                line = line.substr(fdScrollPos);
                if (line.length() <= cols - margin - 6) {
                    fdScrollPos = -1;
                    timeStartScroll = 0;
                }
            }

        line = line.substr(0,cols - margin - 6) + " <DIR>";

    } else {

        if (line.length() <= cols - margin) {
            line = line + std::string(cols - margin - line.length(), ' ');
            line = line.substr(0, cols - margin);
        } else {
            if (line_type == IS_INFO) {
                // printf("%s %d\n",line.c_str(),line.length() - (cols - margin));
                line = ".." + line.substr(line.length() - (cols - margin) + 2);
                // printf("%s\n",line.c_str());
            } else {
                if (line_type == IS_FOCUSED) {
                    line = line.substr(fdScrollPos);
                    if (line.length() <= cols - margin) {
                        fdScrollPos = -1;
                        timeStartScroll = 0;
                    }
                }
                line = line.substr(0, cols - margin);
            }
        }

    }

    VIDEO::vga.print(line.c_str());

    VIDEO::vga.print(" ");

}
