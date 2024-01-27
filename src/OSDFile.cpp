/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023 Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
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

#include <string>
#include <algorithm>
#include <sys/stat.h>
#include "errno.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"

using namespace std;

#include "OSDMain.h"
#include "FileUtils.h"
#include "Config.h"
#include "ESPectrum.h"
#include "CPU.h"
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

// Run a new file menu
string OSD::fileDialog(string &fdir, string title, uint8_t ftype, uint8_t mfcols, uint8_t mfrows) {

    // struct stat stat_buf;
    long dirfilesize;    
    bool reIndex;
  
    // Position
    if (menu_level == 0) {
        x = (Config::aspect_16_9 ? 24 : 4);
        y = (Config::aspect_16_9 ? 4 : 4);
    } else {
        x = (Config::aspect_16_9 ? 24 : 8) + (60 * menu_level);
        y = 8 + (16 * menu_level);
    }

    // Columns and Rows
    cols = mfcols;
    mf_rows = mfrows + (Config::aspect_16_9 ? 0 : 1);

    // printf("Focus: %d, Begin_row: %d, mf_rows: %d\n",(int) FileUtils::fileTypes[ftype].focus,(int) FileUtils::fileTypes[ftype].begin_row,(int) mf_rows);
    if (FileUtils::fileTypes[ftype].focus > mf_rows - 1) {
        FileUtils::fileTypes[ftype].begin_row += FileUtils::fileTypes[ftype].focus - (mf_rows - 1);
        FileUtils::fileTypes[ftype].focus = mf_rows - 1;
    }

    // Size
    w = (cols * OSD_FONT_W) + 2;
    h = ((mf_rows + 1) * OSD_FONT_H) + 2;
    
    // menu = title + "\n" + fdir + "\n";
    menu = title + "\n" + ( fdir.length() == 1 ? fdir : fdir.substr(0,fdir.length()-1)) + "\n";
    WindowDraw(); // Draw menu outline
    fd_PrintRow(1, IS_INFO);    // Path

    // Draw blank rows
    uint8_t row = 2;
    for (; row < mf_rows; row++) {
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        menuAt(row, 0);
        VIDEO::vga.print(std::string(cols, ' ').c_str());
    }

    // Print status bar
    menuAt(row, 0);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
    VIDEO::vga.print(std::string(cols, ' ').c_str());    

    // char fsessid[32];

    // fdSearchRefresh = true;

    while(1) {

        fdCursorFlash = 0;

        reIndex = false;
        string filedir = FileUtils::MountPoint + fdir;

        // // Get / Create sessid
        // bool sessid_ok = false;
        // if (stat((filedir + ".sessid").c_str(), &stat_buf) == 0) {
        //     dirfile = fopen((filedir + ".sessid").c_str(), "r");
        //     fgets(fsessid, sizeof(fsessid), dirfile);
        //     printf("FSessId: %s Sessid: %u\n",fsessid,ESPectrum::sessid);
        //     if (stoul(fsessid) == ESPectrum::sessid) sessid_ok = true;
        // }

        // if (!sessid_ok) {
        //     dirfile = fopen((filedir + ".sessid").c_str(), "w");
        //     fputs(to_string(ESPectrum::sessid).c_str(),dirfile);
        // }

        // fclose(dirfile);

        // Open dir file for read
        dirfile = fopen((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), "r");
        if (dirfile == NULL) {

            // printf("No dir file found: reindexing\n");
            reIndex = true;

        } else {

            // stat((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), &stat_buf);
            fseek(dirfile,0,SEEK_END);
            dirfilesize = ftell(dirfile);
            
            // if (!sessid_ok) {

                // Read dir hash from file
                // fseek(dirfile, (stat_buf.st_size >> 5) << 5,SEEK_SET);
                fseek(dirfile, (dirfilesize >> 6) << 6,SEEK_SET);                

                char fhash[32];
                fgets(fhash, sizeof(fhash), dirfile);
                // printf("File Hash: %s\n",fhash);

                // Count dir items and calc hash
                DIR *dir;
                struct dirent* de;

                std::vector<std::string> filexts;
                size_t pos = 0;
                string ss = FileUtils::fileTypes[ftype].fileExts;
                while ((pos = ss.find(",")) != std::string::npos) {
                    filexts.push_back(ss.substr(0, pos));
                    ss.erase(0, pos + 1);
                }
                filexts.push_back(ss.substr(0));

                unsigned long hash = 0, high; // Name checksum variables

                string fdir = filedir.substr(0,filedir.length() - 1);
                if ((dir = opendir(fdir.c_str())) != nullptr) {
                    
                    elements = 0;
                    ndirs = 0;
                    while ((de = readdir(dir)) != nullptr) {
                        string fname = de->d_name;
                        if (de->d_type == DT_REG || de->d_type == DT_DIR) {
                            if (fname.compare(0,1,".") != 0) {
                                // printf("Fname: %s Fname size: %d\n",fname.c_str(),fname.size());
                                if ((de->d_type == DT_DIR) || ((fname.size() > 3) && (std::find(filexts.begin(),filexts.end(),fname.substr(fname.size()-4)) != filexts.end()))) {
                                    // Calculate name checksum
                                    for (int i = 0; i < fname.length(); i++) {
                                        hash = (hash << 4) + fname[i];
                                        if (high = hash & 0xF0000000) hash ^= high >> 24;
                                        hash &= ~high;
                                    }
                                    if (de->d_type == DT_REG) 
                                        elements++; // Count elements in dir
                                    else if (de->d_type == DT_DIR)
                                            ndirs++;
                                }
                            }
                        }
                    }

                    // printf("Hashcode : %lu\n",hash);
                    
                    closedir(dir);

                } else {

                    printf("Error opening %s\n",filedir.c_str());
                    return "";

                }

                filexts.clear(); // Clear vector
                std::vector<std::string>().swap(filexts); // free memory   

                // If calc hash and file hash are different refresh dir index
                if (stoul(fhash) != hash) {
                    fclose(dirfile);
                    reIndex = true;
                }

            // }

        }

        // // Force reindex (for testing)
        // fclose(dirfile);
        // reIndex = true;

        // There was no index or hashes are different: reIndex
        if (reIndex) {

            FileUtils::DirToFile(filedir, ftype); // Prepare filelist

            // stat((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), &stat_buf);

            dirfile = fopen((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), "r");
            if (dirfile == NULL) {
                printf("Error opening index file\n");
                return "";
            }

            fseek(dirfile,0,SEEK_END);
            dirfilesize = ftell(dirfile);

            // Reset position
            FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;

        }

        if (FileUtils::fileTypes[ftype].fdMode) {

                // Recalc items number
                long prevpos = ftell(dirfile);

                unsigned int foundcount = 0;
                fdSearchElements = 0;
                rewind(dirfile);
                char buf[128];
                char upperbuf[128];
                string search = FileUtils::fileTypes[ftype].fileSearch;
                std::transform(search.begin(), search.end(), search.begin(), ::toupper);
                while(1) {
                    fgets(buf, sizeof(buf), dirfile);
                    if (feof(dirfile)) break;
                    if (buf[0] == ASCII_SPC) {
                            foundcount++;
                            // printf("%s",buf);
                    }else {
                        for(int i=0;i<strlen(buf);i++) upperbuf[i] = toupper(buf[i]);
                        char *pch = strstr(upperbuf, search.c_str());
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

            // real_rows = (stat_buf.st_size / 64) + 2; // Add 2 for title and status bar
            real_rows = (dirfilesize / 64) + 2; // Add 2 for title and status bar        
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
                    menuAt(mfrows + (Config::aspect_16_9 ? 0 : 1), cols - (real_rows > virtual_rows ? 13 : 12));
                    char elements_txt[13];
                    int nitem = (FileUtils::fileTypes[ftype].begin_row + FileUtils::fileTypes[ftype].focus ) - (4 + ndirs) + (fdir.length() == 1);
                    snprintf(elements_txt, sizeof(elements_txt), "%d/%d ", nitem > 0 ? nitem : 0 , elem);
                    VIDEO::vga.print(std::string(12 - strlen(elements_txt), ' ').c_str());
                    VIDEO::vga.print(elements_txt);
                } else {
                    menuAt(mfrows + (Config::aspect_16_9 ? 0 : 1), cols - 13);
                    VIDEO::vga.print("             ");
                }

                if (ESPectrum::readKbd(&Menukey)) {

                    if (!Menukey.down) continue;

                    // Search first ocurrence of letter if we're not on that letter yet
                    if (((Menukey.vk >= fabgl::VK_a) && (Menukey.vk <= fabgl::VK_Z)) || ((Menukey.vk >= fabgl::VK_0) && (Menukey.vk <= fabgl::VK_9))) {

                        int fsearch;
                        if (Menukey.vk<=fabgl::VK_9)
                            fsearch = Menukey.vk + 46;
                        else if (Menukey.vk<=fabgl::VK_z)
                            fsearch = Menukey.vk + 75;
                        else if (Menukey.vk<=fabgl::VK_Z)
                            fsearch = Menukey.vk + 17;

                        if (FileUtils::fileTypes[ftype].fdMode) {

                            if (FileUtils::fileTypes[ftype].fileSearch.length()<10) {
                                FileUtils::fileTypes[ftype].fileSearch += char(fsearch);
                                fdSearchRefresh = true;
                                click();
                            }

                        } else {
                            uint8_t letra = rowGet(menu,FileUtils::fileTypes[ftype].focus).at(0);
                            // printf("%d %d\n",(int)letra,fsearch);
                            if (letra != fsearch) { 
                                // Seek first ocurrence of letter/number
                                long prevpos = ftell(dirfile);
                                char buf[128];
                                int cnt = 0;
                                fseek(dirfile,0,SEEK_SET);
                                while(!feof(dirfile)) {
                                    fgets(buf, sizeof(buf), dirfile);
                                    // printf("%c %d\n",buf[0],int(buf[0]));
                                    if (buf[0] == char(fsearch)) break;
                                    cnt++;
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
                        }

                    } else if (Menukey.vk == fabgl::VK_F3) {

                        FileUtils::fileTypes[ftype].fdMode ^= 1;

                        if (FileUtils::fileTypes[ftype].fdMode) {

                            fdCursorFlash = 63;

                            // menuAt(mfrows + (Config::aspect_16_9 ? 0 : 1), 1);
                            // VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            // VIDEO::vga.print(Config::lang ? "Busq: " : "Find: ");
                            // VIDEO::vga.print(FileUtils::fileTypes[ftype].fileSearch.c_str());
                            // VIDEO::vga.setTextColor(zxColor(5, 0), zxColor(7, 1));
                            // VIDEO::vga.print("K");
                            // VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            // VIDEO::vga.print(std::string(10 - FileUtils::fileTypes[ftype].fileSearch.size(), ' ').c_str());

                            fdSearchRefresh = FileUtils::fileTypes[ftype].fileSearch != "";

                        } else {

                            menuAt(mfrows + (Config::aspect_16_9 ? 0 : 1), 1);
                            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            VIDEO::vga.print("      " "          ");

                            if (FileUtils::fileTypes[ftype].fileSearch != "") {
                                // FileUtils::fileTypes[ftype].fileSearch="";
                                real_rows = (dirfilesize / 64) + 2; // Add 2 for title and status bar        
                                virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
                                last_begin_row = last_focus = 0;
                                FileUtils::fileTypes[ftype].focus = 2;
                                FileUtils::fileTypes[ftype].begin_row = 2;
                                fd_Redraw(title, fdir, ftype);
                            }

                        }

                        click();

                    } else if (Menukey.vk == fabgl::VK_UP || Menukey.vk == fabgl::VK_JOY1UP || Menukey.vk == fabgl::VK_JOY2UP) {
                        if (FileUtils::fileTypes[ftype].focus == 2 && FileUtils::fileTypes[ftype].begin_row > 2) {
                            last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                            FileUtils::fileTypes[ftype].begin_row--;
                            fd_Redraw(title, fdir, ftype);
                        } else if (FileUtils::fileTypes[ftype].focus > 2) {
                            last_focus = FileUtils::fileTypes[ftype].focus;
                            fd_PrintRow(FileUtils::fileTypes[ftype].focus--, IS_NORMAL);
                            fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
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
                            fd_PrintRow(FileUtils::fileTypes[ftype].focus++, IS_NORMAL);
                            fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
                            // printf("Focus: %d, Lastfocus: %d\n",FileUtils::fileTypes[ftype].focus,(int) last_focus);
                        }
                        click();
                    } else if (Menukey.vk == fabgl::VK_PAGEUP || Menukey.vk == fabgl::VK_LEFT || Menukey.vk == fabgl::VK_JOY1LEFT || Menukey.vk == fabgl::VK_JOY2LEFT) {
                        if (FileUtils::fileTypes[ftype].begin_row > virtual_rows) {
                            FileUtils::fileTypes[ftype].focus = 2;
                            FileUtils::fileTypes[ftype].begin_row -= virtual_rows - 2;
                        } else {
                            FileUtils::fileTypes[ftype].focus = 2;
                            FileUtils::fileTypes[ftype].begin_row = 2;
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
                                fdir = fdir.substr(0,fdir.find_last_of("/") + 1);

                                FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;
                                // printf("Fdir: %s\n",fdir.c_str());

                                click();
                                
                                break;

                            }       
                        }                  
                    } else if (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_SPACE || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C) {

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

                            if (menu_saverect) {
                                // Restore backbuffer data
                                int j = SaveRectpos - (((w >> 2) + 1) * h);
                                SaveRectpos = j - 4;
                                for (int  m = y; m < y + h; m++) {
                                    uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
                                    for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                                        backbuffer32[n] = VIDEO::SaveRect[j];
                                        j++;
                                    }
                                }
                                menu_saverect = false;                                
                            }

                            rtrim(filedir);
                            click();
                            return (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B ? "R" : "S") + filedir;
                        }

                    } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {

                        // Restore backbuffer data
                        if (menu_saverect) {
                            int j = SaveRectpos - (((w >> 2) + 1) * h);
                            SaveRectpos = j - 4;
                            for (int  m = y; m < y + h; m++) {
                                uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
                                for (int n = x >> 2; n < ((x + w) >> 2) + 1; n++) {
                                    backbuffer32[n] = VIDEO::SaveRect[j];
                                    j++;
                                }
                            }
                            menu_saverect = false;
                        }

                        fclose(dirfile);
                        dirfile = NULL;
                        click();
                        return "";
                    }
                }

            } else {

                if (timeStartScroll < 200) timeStartScroll++;

            }

            // TO DO: SCROLL FOCUSED LINE IF SIGNALED
            if (timeStartScroll == 200) {
                timeScroll++;
                if (timeScroll == 50) {  
                    fdScrollPos++;
                    fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
                    timeScroll = 0;
                }
            }

            if (FileUtils::fileTypes[ftype].fdMode) {

                if ((++fdCursorFlash & 0xf) == 0) {

                    menuAt(mfrows + (Config::aspect_16_9 ? 0 : 1), 1);
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                    VIDEO::vga.print(Config::lang ? "Busq: " : "Find: ");
                    VIDEO::vga.print(FileUtils::fileTypes[ftype].fileSearch.c_str());
                    if (fdCursorFlash > 63) {
                        VIDEO::vga.setTextColor(zxColor(5, 0), zxColor(7, 1));
                        if (fdCursorFlash == 128) fdCursorFlash = 0;
                    }
                    VIDEO::vga.print("K");
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                    VIDEO::vga.print(std::string(10 - FileUtils::fileTypes[ftype].fileSearch.size(), ' ').c_str());
                }

                if (fdSearchRefresh) {

                    // Recalc items number
                    long prevpos = ftell(dirfile);

                    unsigned int foundcount = 0;
                    fdSearchElements = 0;
                    rewind(dirfile);
                    char buf[128];
                    char upperbuf[128];
                    string search = FileUtils::fileTypes[ftype].fileSearch;
                    std::transform(search.begin(), search.end(), search.begin(), ::toupper);
                    while(1) {
                        fgets(buf, sizeof(buf), dirfile);
                        if (feof(dirfile)) break;
                        if (buf[0] == ASCII_SPC) {
                                foundcount++;
                                // printf("%s",buf);
                        }else {
                            for(int i=0;i<strlen(buf);i++) upperbuf[i] = toupper(buf[i]);
                            char *pch = strstr(upperbuf, search.c_str());
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

            }

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
        char buf[128];
        if (FileUtils::fileTypes[ftype].fdMode == 0 || FileUtils::fileTypes[ftype].fileSearch == "") {
            fseek(dirfile, (FileUtils::fileTypes[ftype].begin_row - 2) * 64,SEEK_SET);
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
            char upperbuf[128];
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
                    for(int i=0;i<strlen(buf);i++) upperbuf[i] = toupper(buf[i]);
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
