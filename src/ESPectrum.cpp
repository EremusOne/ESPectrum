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

#include <stdio.h>
#include <string>

#include "ESPectrum.h"
#include "Snapshot.h"
#include "ESPConfig.h"
#include "FileUtils.h"
#include "OSDMain.h"
#include "Ports.h"
#include "MemESP.h"
#include "cpuESP.h"
#include "Video.h"
#include "messages.h"
#include "AySound.h"
#include "Tape.h"
#include "Z80_JLS/z80.h"
#include "pwm_audio.h"
#include "fabgl.h"
#include "roms.h"
#include "AudioIn.h"

#include "ZXKeyb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "soc/timer_group_struct.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_efuse.h"
#include "soc/efuse_reg.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

using namespace std;

//=======================================================================================
// KEYBOARD
//=======================================================================================
fabgl::PS2Controller ESPectrum::PS2Controller;
bool ESPectrum::ps2kbd = false;
bool ESPectrum::ps2kbd2 = false;
bool ESPectrum::ps2mouse = false;

//=======================================================================================
// AUDIO
//=======================================================================================
uint8_t ESPectrum::audioBuffer[ESP_AUDIO_SAMPLES_PENTAGON] = { 0 };
uint8_t ESPectrum::SamplebufCOVOX[ESP_AUDIO_SAMPLES_PENTAGON] = { 0 };
uint32_t ESPectrum::audbufcntCOVOX = 0;
uint32_t ESPectrum::faudbufcntCOVOX = 0;
uint8_t ESPectrum::covoxData[4];
uint16_t ESPectrum::covoxMix;
uint16_t ESPectrum::fcovoxMix;
uint32_t* ESPectrum::overSamplebuf;
signed char ESPectrum::aud_volume;
uint32_t ESPectrum::audbufcnt = 0;
uint32_t ESPectrum::audbufcntover = 0;
// uint32_t ESPectrum::faudbufcnt = 0;
uint32_t ESPectrum::faudbufcntover = 0;
uint32_t ESPectrum::audbufcntAY = 0;
uint32_t ESPectrum::faudbufcntAY = 0;
int ESPectrum::lastaudioBit = 0;
int ESPectrum::flastaudioBit = 0;
static int faudioBitBuf = 0;
static unsigned char faudioBitbufCount = 0;
int ESPectrum::samplesPerFrame;
bool ESPectrum::AY_emu = false;
int ESPectrum::Audio_freq[4];
unsigned char ESPectrum::audioSampleDivider;
unsigned char ESPectrum::audioAYDivider;
unsigned char ESPectrum::audioCOVOXDivider;
unsigned char ESPectrum::audioOverSampleDivider;
int ESPectrum::audioBitBuf = 0;
unsigned char ESPectrum::audioBitbufCount = 0;
QueueHandle_t audioTaskQueue;
TaskHandle_t ESPectrum::audioTaskHandle;
uint8_t *param;
uint8_t ESPectrum::aud_active_sources = 0;

//=======================================================================================
// TAPE OSD
//=======================================================================================

int ESPectrum::TapeNameScroller = 0;

//=======================================================================================
// BETADISK
//=======================================================================================

bool ESPectrum::trdos = false;

rvmWD1793 ESPectrum::fdd;

//=======================================================================================
// RESET BUTTON
//=======================================================================================

bool ESPectrum::rst_button = false;

//=======================================================================================
// ARDUINO FUNCTIONS
//=======================================================================================

#define NOP() asm volatile ("nop")

// IRAM_ATTR unsigned long millis()
// {
//     return (unsigned long) (esp_timer_get_time() / 1000ULL);
// }

IRAM_ATTR void delayMicroseconds(int64_t us)
{
    int64_t m = esp_timer_get_time();
    if(us){
        int64_t e = (m + us);
        if(m > e){ //overflow
            while(esp_timer_get_time() > e){
                NOP();
            }
        }
        while(esp_timer_get_time() < e){
            NOP();
        }
    }
}

//=======================================================================================
// TIMING / SYNC
//=======================================================================================

double ESPectrum::totalseconds = 0;
double ESPectrum::totalsecondsnodelay = 0;
int64_t ESPectrum::target[4];
int ESPectrum::sync_cnt = 0;
volatile bool ESPectrum::vsync = false;
int64_t ESPectrum::ts_start;
int64_t ESPectrum::elapsed;
int64_t ESPectrum::idle;
uint8_t ESPectrum::ESP_delay = 1; // EMULATION SPEED: 0-> MAX. SPEED (NO SOUND), 1-> 100% SPEED, 2-> 125% SPEED, 3-> 150% SPEED
int ESPectrum::ESPoffset = 0;

//=======================================================================================
// LOGGING / TESTING
//=======================================================================================

int ESPectrum::ESPtestvar = 0;
int ESPectrum::ESPtestvar1 = 0;
int ESPectrum::ESPtestvar2 = 0;

#define START_MSG_DURATION 20

void ShowStartMsg() {

    fabgl::VirtualKeyItem Nextkey;

    VIDEO::vga.clear(zxColor(7,0));

    OSD::drawOSD(false);

    int pos_x, pos_y;

    if (Config::videomode == 2) {
        pos_x = 82;
        if (Config::arch[0] == 'T' && Config::ALUTK == 2) {
            VIDEO::vga.fillRect( 56, 24, 240,50,zxColor(0, 0));
            pos_y = 35;
        } else {
            VIDEO::vga.fillRect( 56, 48,240,50,zxColor(0, 0));
            pos_y = 59;
        }
    } else {
        VIDEO::vga.fillRect(Config::aspect_16_9 ? 60 : 40,Config::aspect_16_9 ? 12 : 32,240,50,zxColor(0, 0));
        pos_x = Config::aspect_16_9 ? 86 : 66;
        pos_y = Config::aspect_16_9 ? 23 : 43;
    }

    // Decode Logo in EBF8 format
    uint8_t *logo = (uint8_t *)ESPectrum_logo;
    int logo_w = (logo[5] << 8) + logo[4]; // Get Width
    int logo_h = (logo[7] << 8) + logo[6]; // Get Height
    logo+=8; // Skip header
    for (int i=0; i < logo_h; i++)
        for(int n=0; n<logo_w; n++)
            VIDEO::vga.dotFast(pos_x + n,pos_y + i,logo[n+(i*logo_w)]);

    OSD::osdAt(7, 1);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 0));
    VIDEO::vga.print(StartMsg[Config::lang]);

    VIDEO::vga.setTextColor(zxColor(16,0), zxColor(1, 0));
    OSD::osdAt(9, 1);
    VIDEO::vga.print("ESP");

    switch (Config::lang) {
        case 0: OSD::osdAt(7, 25);
                VIDEO::vga.print("ESP");
                OSD::osdAt(13, 13);
                VIDEO::vga.print("ESP");
                OSD::osdAt(17, 4);
                break;
        case 1: OSD::osdAt(7, 28);
                VIDEO::vga.print("ESP");
                OSD::osdAt(13, 13);
                VIDEO::vga.print("ESP");
                OSD::osdAt(17, 4);
                break;
        case 2: OSD::osdAt(7, 27);
                VIDEO::vga.print("ESP");
                OSD::osdAt(13, 18);
                VIDEO::vga.print("ESP");
                OSD::osdAt(17, 15);
    }

    VIDEO::vga.setTextColor(zxColor(3, 1), zxColor(1, 0));
    VIDEO::vga.print("patreon.com/ESPectrum");

    char msg[38];
    for (int i=START_MSG_DURATION; i >= 0; i--) {
        OSD::osdAt(19, 1);
        // sprintf(msg,Config::lang ? "Este mensaje se cerrar" "\xA0" " en %02d segundos" : "This message will close in %02d seconds",i);
        sprintf(msg,STARTMSG_CLOSE[Config::lang],i);
        VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));
        VIDEO::vga.print(msg);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    VIDEO::vga.clear(zxColor(7,0));

    // Disable StartMsg
    Config::StartMsg = false;
    Config::save("StartMsg");

}

void ESPectrum::showMemInfo(const char* caption) {

    string textout;

    // // Get chip information
    // esp_chip_info_t chip_info;
    // esp_chip_info(&chip_info);

    printf(" ------------------------------------------------------------\n");
    printf(" Memory info - %s \n", caption);
    // printf(" Hardware info - %s \n", caption);
    printf(" ------------------------------------------------------------\n");

    // // Chip models for ESP32
    // textout = " Chip model    : ";
    // uint32_t chip_ver = esp_efuse_get_pkg_ver();
    // uint32_t pkg_ver = chip_ver & 0x7;
    // switch (pkg_ver) {
    //     case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ6 :
    //         if (chip_info.revision == 3)
    //             textout += "ESP32-D0WDQ6-V3";
    //         else
    //             textout += "ESP32-D0WDQ6";
    //         break;
    //     case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ5 :
    //         if (chip_info.revision == 3)
    //             textout += "ESP32-D0WD-V3";
    //         else
    //             textout += "ESP32-D0WD";
    //         break;
    //     case EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5 :
    //         textout += "ESP32-D2WD";
    //         break;
    //     case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2 :
    //         textout += "ESP32-PICO-D2";
    //         break;
    //     case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4 :
    //         textout += "ESP32-PICO-D4";
    //         break;
    //     case EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302 :
    //         textout += "ESP32-PICO-V3-02";
    //         break;
    //     case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDR2V3 :
    //          textout += "ESP32-D0WDR2-V3";
    //         break;
    //     default:
    //         textout += "Unknown";
    // }
    // textout += "\n";
    // printf(textout.c_str());

    // textout = " Chip cores    : " + to_string(chip_info.cores) + "\n";
    // printf(textout.c_str());

    // textout = " Chip revision : " + to_string(chip_info.revision) + "\n";
    // printf(textout.c_str());

    // textout = " Flash size    : " + to_string(spi_flash_get_chip_size() / (1024 * 1024)) + (chip_info.features & CHIP_FEATURE_EMB_FLASH ? "MB embedded" : "MB external") + "\n";
    // printf(textout.c_str());

    multi_heap_info_t info;

    // heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    // uint32_t psramsize = (info.total_free_bytes + info.total_allocated_bytes) >> 10;
    // textout = " PSRAM size    : " + ( psramsize == 0 ? "N/A or disabled" : to_string(psramsize) + " MB") + "\n";
    // printf(textout.c_str());

    // textout = " IDF Version   : " + (string)(esp_get_idf_version()) + "\n";
    // printf(textout.c_str());

    // printf("\n Memory info\n");
    // printf(" ------------------------------------------------------------\n");

    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task
    textout = " Total free bytes         : " + to_string(info.total_free_bytes) + "\n";
    printf(textout.c_str());

    textout = " Minimum free ever        : " + to_string(info.minimum_free_bytes) + "\n";
    printf(textout.c_str());

    // textout = " Largest free block       : " + to_string(info.largest_free_block) + "\n";
    // printf(textout.c_str());

    // textout = " Free (MALLOC_CAP_32BIT)  : " + to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT)) + "\n";
    // printf(textout.c_str());

    // UBaseType_t wm;
    // wm = uxTaskGetStackHighWaterMark(NULL);
    // textout = " Main  Task Stack HWM     : " + to_string(wm) + "\n";
    // printf(textout.c_str());

    // wm = uxTaskGetStackHighWaterMark(ESPectrum::audioTaskHandle);
    // textout = " Audio Task Stack HWM     : " + to_string(wm) + "\n";
    // printf(textout.c_str());

    // wm = uxTaskGetStackHighWaterMark(VIDEO::videoTaskHandle);
    // textout = " Video Task Stack HWM     : " + (Config::videomode ? to_string(wm) : "N/A") + "\n";
    // printf(textout.c_str());
    printf("\n ------------------------------------------------------------\n\n");

}

//=======================================================================================
// BOOT KEYBOARD
//=======================================================================================
void ESPectrum::bootKeyboard(int timeout) {

    int i = 0;
    string s = "99";

    // printf("Boot kbd!\n");

    for (; i < timeout; i++) {

        if (ZXKeyb::Exists) {

            // Process physical keyboard
            ZXKeyb::process();

            // Detect and process physical kbd menu key combinations
            if (ZXKBD_0) s[0]='0';
            else if (ZXKBD_1) s[0]='1';
            else if (ZXKBD_2) s[0]='2';
            else if (ZXKBD_3) s[0]='3';

            if (ZXKBD_Q) s[1]='Q';
            else if (ZXKBD_W) s[1]='W';
            else if (ZXKBD_Z) s[1]='Z';


        }

        if (ps2kbd) {

            auto Kbd = PS2Controller.keyboard();
            fabgl::VirtualKeyItem NextKey;

            while (Kbd->virtualKeyAvailable()) {

                bool r = Kbd->getNextVirtualKey(&NextKey);

                if (r && NextKey.down) {

                    // Check keyboard status
                    switch (NextKey.vk) {
                        case fabgl::VK_0:
                            s[0] = '0';
                            break;
                        case fabgl::VK_1:
                            s[0] = '1';
                            break;
                        case fabgl::VK_2:
                            s[0] = '2';
                            break;
                        case fabgl::VK_3:
                            s[0] = '3';
                            break;
                        case fabgl::VK_Q:
                        case fabgl::VK_q:
                            s[1] = 'Q';
                            break;
                        case fabgl::VK_W:
                        case fabgl::VK_w:
                            s[1] = 'W';
                            break;
                        case fabgl::VK_Z:
                        case fabgl::VK_z:
                            s[1] = 'Z';
                            break;
                    }

                }

            }
        }

        if (s.find('9') == std::string::npos) break;

        delayMicroseconds(1000);

    }

    // printf("Boot kbd end!\n");

    if (i < timeout) {

        // printf("Boot keys detected: %s\n", s.c_str());

        if (s[0] == '0') {

            if (s[1] == 'Z') {
                nvs_flash_erase();
                nvs_flash_init();
                OSD::esp_hard_reset();
            }

        } else {

            bool asp169 = false;
            uint8_t vidmode = (s[0] == '1') ? 0 : (s[0] == '2') ? 1 : 2;
            if (vidmode != 2) asp169 = (s[1] == 'Q') ? false : true;

            if (Config::videomode != vidmode || Config::aspect_16_9 != asp169) {
                Config::videomode = vidmode;
                Config::aspect_16_9 = asp169;
                Config::ram_file="none";
                Config::save("videomode");
                Config::save("asp169");
                Config::save("ram");
                OSD::esp_hard_reset();
            }

        }

    }

}

//=======================================================================================
// SETUP
//=======================================================================================

// __NOINIT_ATTR char noinit_ramfile[1024];

void ESPectrum::setup() {

    ts_start = esp_timer_get_time();

    // esp_reset_reason_t reason = esp_reset_reason();
    // if (reason != ESP_RST_WDT) {
    //     for (int i=0; i < sizeof(noinit_ramfile); i++) noinit_ramfile[i] = ' ';
    // }

    // for (int i=0; i < sizeof(noinit_ramfile); i++) {
    //     printf("%c",noinit_ramfile[i]);
    // }
    // printf("\n");

    // if (Config::slog_on) {
    //     printf("------------------------------------\n");
    //     printf("| ESPectrum: booting               |\n");
    //     printf("------------------------------------\n");
    //     showMemInfo();
    // }

    //=======================================================================================
    // LOAD CONFIG
    //=======================================================================================

    Config::load();

    if (Config::StartMsg) {
        // Get PSRAM size and set it in Config
        multi_heap_info_t info; heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
        Config::psram_size = (info.total_free_bytes + info.total_allocated_bytes) >> 10;
        // printf("PSRAM size: %d\n", Config::psram_size);
        ZXKeyb::check(); // Check if ZX Keyboard is connected
        if (ZXKeyb::Exists) {
            Config::Board = Config::psram_size ? BOARD_ESPECTRUM_PSRAM : BOARD_ESPECTRUM_NOPSRAM;
            Config::ps2_dev2 = 1; // Default second PS/2 device to PS/2 Keyboard (for ppl testing or checking board without rubber keys)
        } else {
            // Get chip information
            esp_chip_info_t chip_info;
            esp_chip_info(&chip_info);
            Config::Board = chip_info.revision < 3 ? BOARD_LILYGO : BOARD_OLIMEX;
        }
        Config::save(); // Firmware updated or reflashed: save all config data
    }

    // Set arch if there's no snapshot to load
    if (Config::ram_file == NO_RAM_FILE) {

        if (Config::pref_arch.substr(Config::pref_arch.length()-1) == "R") {

            Config::pref_arch.pop_back();
            Config::save("pref_arch");

        } else {

            if (Config::pref_arch != "Last") Config::arch = Config::pref_arch;

            if (Config::arch == "48K") {
                if (Config::pref_romSet_48 != "Last")
                    Config::romSet = Config::pref_romSet_48;
                else
                    Config::romSet = Config::romSet48;
            } else if (Config::arch == "128K") {
                if (Config::pref_romSet_128 != "Last")
                    Config::romSet = Config::pref_romSet_128;
                else
                    Config::romSet = Config::romSet128;
            } else if (Config::arch == "+2A") {
                if (Config::pref_romSet_2A != "Last")
                    Config::romSet = Config::pref_romSet_2A;
                else
                    Config::romSet = Config::romSet2A;
            } else if (Config::arch == "TK90X") {
                if (Config::pref_romSet_90X != "Last")
                    Config::romSet = Config::pref_romSet_90X;
                else
                    Config::romSet = Config::romSetTK90X;
            } else if (Config::arch == "TK95") {
                if (Config::pref_romSet_95 != "Last")
                    Config::romSet = Config::pref_romSet_95;
                else
                    Config::romSet = Config::romSetTK95;
            } else Config::romSet = "Pentagon";

            // printf("Arch: %s, Romset: %s\n",Config::arch.c_str(), Config::romSet.c_str());

        }

    }

    //=======================================================================================
    // INIT PS/2 KEYBOARD
    //=======================================================================================
    if (Config::Board >= BOARD_ESPECTRUM_NOPSRAM) {

        ZXKeyb::setup();
        ZXKeyb::Exists = 1;

        switch (Config::ps2_dev2) {
        case 0:
            PS2Controller.begin(PS2Preset::zxKeyb, KbdMode::CreateVirtualKeysQueue);
            break;
        case 1:
            PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
            ps2kbd = true; // ESPjoy doesn't respond to isKeyboardAvailable so we assume it's there
            break;
        case 2:
            PS2Controller.begin(PS2Preset::MousePort0, KbdMode::CreateVirtualKeysQueue);
            ps2mouse = PS2Controller.mouse()->isMouseAvailable();
            if (!ps2mouse) {
                printf("2nd PS/2 device (Kbd/ESPjoy) not detected\n");
                Config::ps2_dev2 = 4;
                Config::save("PS2Dev2");
                OSD::esp_hard_reset();
            }
            break;
        case 3:
        case 4:
            PS2Controller.begin(PS2Preset::zxKeyb, KbdMode::CreateVirtualKeysQueue);
            Config::ps2_dev2 -= 2;
            Config::save("PS2Dev2");
            break;
        }

    } else {

        switch (Config::ps2_dev2) {
        case 0:
            PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
            ps2kbd = true;
            break;
        case 1:
            PS2Controller.begin(PS2Preset::KeyboardPort0_KeybJoystickPort1, KbdMode::CreateVirtualKeysQueue);
            ps2kbd  = true;
            ps2kbd2 = true; // ESPjoy doesn't respond to isKeyboardAvailable so we assume it's there
            break;
        case 2:
            PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::CreateVirtualKeysQueue);
            ps2kbd = true;
            ps2mouse = PS2Controller.mouse()->isMouseAvailable();
            if (!ps2mouse) {
                printf("2nd PS/2 device (mouse) not detected\n");
                Config::ps2_dev2 = 4;
                Config::save("PS2Dev2");
                OSD::esp_hard_reset();
            }
            break;
        case 3:
        case 4:
            PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
            ps2kbd = true;
            Config::ps2_dev2 -= 2;
            Config::save("PS2Dev2");
            break;
        }

    }

    if (ps2mouse) {
        printf("Mouse detected\n");
        PS2Controller.mouse()->setResolution(Config::mousedpi);
        PS2Controller.mouse()->setSampleRate(100);
        PS2Controller.mouse()->setScaling(1);
    }

    // Set Scroll Lock Led as current CursorAsJoy value
    if(ps2kbd)
        PS2Controller.keyboard()->setLEDs(false, false, Config::CursorAsJoy);
    if(ps2kbd2)
        PS2Controller.keybjoystick()->setLEDs(false, false, Config::CursorAsJoy);

    // Set TAB and GRAVEACCENT behaviour
    if (Config::TABasfire1) {
        ESPectrum::VK_ESPECTRUM_FIRE1 = fabgl::VK_TAB;
        ESPectrum::VK_ESPECTRUM_FIRE2 = fabgl::VK_GRAVEACCENT;
        ESPectrum::VK_ESPECTRUM_TAB = fabgl::VK_NONE;
        ESPectrum::VK_ESPECTRUM_GRAVEACCENT = fabgl::VK_NONE;
    } else {
        ESPectrum::VK_ESPECTRUM_FIRE1 = fabgl::VK_NONE;
        ESPectrum::VK_ESPECTRUM_FIRE2 = fabgl::VK_NONE;
        ESPectrum::VK_ESPECTRUM_TAB = fabgl::VK_TAB;
        ESPectrum::VK_ESPECTRUM_GRAVEACCENT = fabgl::VK_GRAVEACCENT;
    }

    if (Config::slog_on) {
        showMemInfo("Keyboard started");
    }

    // Set reset button GPIO as input
    if (Config::rst_button) gpio_set_direction((gpio_num_t)Config::rst_button, (gpio_mode_t)GPIO_MODE_INPUT);

    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Config::esp32rev = chip_info.revision;

    if (Config::slog_on) {

        printf("\n");
        printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
                CONFIG_IDF_TARGET,
                chip_info.cores,
                (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
        printf("silicon revision %d, ", chip_info.revision);
        printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
                (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
        printf("IDF Version: %s\n",esp_get_idf_version());
        printf("\n");

        if (Config::slog_on) printf("Executing on core: %u\n", xPortGetCoreID());

        showMemInfo();

    }

    //=======================================================================================
    // MEMORY SETUP
    //=======================================================================================

    MemESP::Init();

    // Load romset
    Config::requestMachine(Config::arch, Config::romSet);

    MemESP::Reset();

    if (Config::slog_on) showMemInfo("RAM Initialized");

    //=======================================================================================
    // VIDEO
    //=======================================================================================

    VIDEO::Init();
    VIDEO::Reset();

    if (Config::slog_on) showMemInfo("VGA started");

    if (Config::StartMsg) ShowStartMsg(); // Show welcome message

    //=======================================================================================
    // AUDIO
    //=======================================================================================

    overSamplebuf = (uint32_t *) heap_caps_calloc(ESP_AUDIO_SAMPLES_PENTAGON, sizeof(uint32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);

    if (overSamplebuf == NULL) printf("Can't allocate oversamplebuffer\n");

    // Set samples per frame and AY_emu flag depending on arch
    if (Config::arch == "48K") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_48;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_48;
        audioAYDivider = ESP_AUDIO_AY_DIV_48;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_48;
        AY_emu = Config::AY48;
        Audio_freq[0] = ESP_AUDIO_FREQ_48;
        Audio_freq[1] = ESP_AUDIO_FREQ_48;
        Audio_freq[2] = ESP_AUDIO_FREQ_48_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_48_150SPEED;
    } else if (Config::arch == "TK90X" || Config::arch == "TK95") {

        switch (Config::ALUTK) {
        case 0:
            samplesPerFrame=ESP_AUDIO_SAMPLES_48;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_48;
            audioAYDivider = ESP_AUDIO_AY_DIV_48;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_48;
            Audio_freq[0] = ESP_AUDIO_FREQ_48;
            Audio_freq[1] = ESP_AUDIO_FREQ_48;
            Audio_freq[2] = ESP_AUDIO_FREQ_48_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_48_150SPEED;
            break;
        case 1:
            samplesPerFrame=ESP_AUDIO_SAMPLES_TK_50;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_TK_50;
            audioAYDivider = ESP_AUDIO_AY_DIV_TK_50;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_TK_50;
            Audio_freq[0] = ESP_AUDIO_FREQ_TK_50;
            Audio_freq[1] = ESP_AUDIO_FREQ_TK_50;
            Audio_freq[2] = ESP_AUDIO_FREQ_TK_50_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_TK_50_150SPEED;
            break;
        case 2:
            samplesPerFrame=ESP_AUDIO_SAMPLES_TK_60;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_TK_60;
            audioAYDivider = ESP_AUDIO_AY_DIV_TK_60;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_TK_60;
            Audio_freq[0] = ESP_AUDIO_FREQ_TK_60;
            Audio_freq[1] = ESP_AUDIO_FREQ_TK_60;
            Audio_freq[2] = ESP_AUDIO_FREQ_TK_60_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_TK_60_150SPEED;
        }

        AY_emu = Config::AY48;

    } else if (Config::arch == "128K") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_128;
        audioAYDivider = ESP_AUDIO_AY_DIV_128;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_128;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_128;
        Audio_freq[1] = ESP_AUDIO_FREQ_128;
        Audio_freq[2] = ESP_AUDIO_FREQ_128_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_128_150SPEED;
    } else if (Config::arch == "+2A") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_128;
        audioAYDivider = ESP_AUDIO_AY_DIV_128;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_128;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_128;
        Audio_freq[1] = ESP_AUDIO_FREQ_128;
        Audio_freq[2] = ESP_AUDIO_FREQ_128_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_128_150SPEED;
    } else if (Config::arch == "Pentagon") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_PENTAGON;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_PENTAGON;
        audioAYDivider = ESP_AUDIO_AY_DIV_PENTAGON;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_PENTAGON;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_PENTAGON;
        Audio_freq[1] = ESP_AUDIO_FREQ_PENTAGON;
        Audio_freq[2] = ESP_AUDIO_FREQ_PENTAGON_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_PENTAGON_150SPEED;
    }

    // Create Audio task
    audioTaskQueue = xQueueCreate(1, sizeof(uint8_t *));
    // Latest parameter = Core. In ESPIDF, main task runs on core 0 by default. In Arduino, loop() runs on core 1.
    xTaskCreatePinnedToCore(&ESPectrum::audioTask, "audioTask", 2048 /* 1024 /* 1536 */, NULL, configMAX_PRIORITIES - 1, &audioTaskHandle, 1);

    audioCOVOXDivider = audioAYDivider;
    covoxData[0] = covoxData[1] = covoxData[2] = covoxData[3] = 0;

    if (Config::EarBoost) {
        AY_emu = false; // Disable AY emulation if tape player mode is set
        ESPectrum::aud_volume = ESP_VOLUME_MAX;
    } else
        ESPectrum::aud_volume = Config::volume;

    ESPoffset = 0;

    // AY Sound
    AySound::init();
    AySound::set_sound_format(Audio_freq[ESP_delay],1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    aud_active_sources = (Config::Covox & 0x01) | (AY_emu << 1);

    // Init tape
    Tape::Init();
    Tape::tapeFileName = "none";
    Tape::tapeStatus = TAPE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    if (Z80Ops::is128 || Z80Ops::is2a3) { // Apply pulse length compensation for 128K
        Tape::tapeCompensation = FACTOR128K;
    } else if ((Config::arch=="TK90X" || Config::arch == "TK95") && Config::ALUTK > 0) { // Apply pulse length compensation for Microdigital ALU
        Tape::tapeCompensation = FACTORALUTK;
    } else
        Tape::tapeCompensation = 1;

    // Init CPU
    Z80::create();

    // Set Ports starting values
    for (int i = 0; i < 128; i++) Ports::port[i] = 0xFF; // 0xBF;
    Ports::port254 = 0;
    Ports::LastOutTo1FFD = 0;
    if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0; // Kempston
    if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff; // Fuller

    ESPectrum::joystick1 = Config::joystick1;
    ESPectrum::joystick2 = Config::joystick2;

    // Read joystick default definition
    for (int n = 0; n < 24; n++)
        ESPectrum::JoyVKTranslation[n] = (fabgl::VirtualKey) Config::joydef[n];

    rvmWD1793Reset(&fdd);

    // Reset cpu
    CPU::reset();

    // Init audio input
    AudioIn::Init();

    // Load snapshot if present in Config::ram_file
    if (Config::ram_file != NO_RAM_FILE) {

        FileUtils::initFileSystem();

        printf("------------------------------------\n");
        printf("LOAD SNAPSHOT BECAUSE ARCH CHANGED OR RAM FILE SET\n");
        printf("RAM file: %s\n", Config::ram_file.c_str());

        int dirlev;

        FileUtils::SNA_Path = Config::SNA_Path;
        printf("SNA Path: %s\n", FileUtils::SNA_Path.c_str());
        dirlev = FileUtils::SNA_Path != "/" ? count(FileUtils::SNA_Path.begin(), FileUtils::SNA_Path.end(), '/') - 1 : 0;

        printf("Dirlev SNA: %d\n", dirlev);
        FileUtils::fileTypes[DISK_SNAFILE].dirLevel = dirlev;
        for (int i=0; i < dirlev; i++) {
            FileUtils::fileTypes[DISK_SNAFILE].begin_row[i] = 2;
            FileUtils::fileTypes[DISK_SNAFILE].focus[i] = 2;
            printf("Init SNA level %d\n", i);
        }
        FileUtils::fileTypes[DISK_SNAFILE].begin_row[dirlev] = Config::SNA_begin_row;
        FileUtils::fileTypes[DISK_SNAFILE].focus[dirlev] = Config::SNA_focus;
        FileUtils::fileTypes[DISK_SNAFILE].fdMode = Config::SNA_fdMode;
        FileUtils::fileTypes[DISK_SNAFILE].fileSearch = Config::SNA_fileSearch;

        FileUtils::TAP_Path = Config::TAP_Path;
        dirlev = FileUtils::TAP_Path != "/" ? count(FileUtils::TAP_Path.begin(), FileUtils::TAP_Path.end(), '/') - 1 : 0;
        FileUtils::fileTypes[DISK_TAPFILE].dirLevel = dirlev;
        for (int i=0; i < dirlev; i++) {
            FileUtils::fileTypes[DISK_TAPFILE].begin_row[i] = 2;
            FileUtils::fileTypes[DISK_TAPFILE].focus[i] = 2;
        }
        FileUtils::fileTypes[DISK_TAPFILE].begin_row[dirlev] = Config::TAP_begin_row;
        FileUtils::fileTypes[DISK_TAPFILE].focus[dirlev] = Config::TAP_focus;
        FileUtils::fileTypes[DISK_TAPFILE].fdMode = Config::TAP_fdMode;
        FileUtils::fileTypes[DISK_TAPFILE].fileSearch = Config::TAP_fileSearch;

        FileUtils::DSK_Path = Config::DSK_Path;
        dirlev = FileUtils::DSK_Path != "/" ? count(FileUtils::DSK_Path.begin(), FileUtils::DSK_Path.end(), '/') - 1 : 0;
        FileUtils::fileTypes[DISK_DSKFILE].dirLevel = dirlev;
        for (int i=0; i < dirlev; i++) {
            FileUtils::fileTypes[DISK_DSKFILE].begin_row[i] = 2;
            FileUtils::fileTypes[DISK_DSKFILE].focus[i] = 2;
        }
        FileUtils::fileTypes[DISK_DSKFILE].begin_row[dirlev] = Config::DSK_begin_row;
        FileUtils::fileTypes[DISK_DSKFILE].focus[dirlev] = Config::DSK_focus;
        FileUtils::fileTypes[DISK_DSKFILE].fdMode = Config::DSK_fdMode;
        FileUtils::fileTypes[DISK_DSKFILE].fileSearch = Config::DSK_fileSearch;

        FileUtils::ESP_Path = Config::ESP_Path;
        dirlev = FileUtils::ESP_Path != "/" ? count(FileUtils::ESP_Path.begin(), FileUtils::ESP_Path.end(), '/') - 1 : 0;
        for (int i=0; i < dirlev; i++) {
            FileUtils::fileTypes[DISK_ESPFILE].begin_row[i] = 2;
            FileUtils::fileTypes[DISK_ESPFILE].focus[i] = 2;
        }
        FileUtils::fileTypes[DISK_ESPFILE].dirLevel = dirlev;
        FileUtils::fileTypes[DISK_ESPFILE].begin_row[dirlev] = Config::ESP_begin_row;
        FileUtils::fileTypes[DISK_ESPFILE].focus[dirlev] = Config::ESP_focus;
        FileUtils::fileTypes[DISK_ESPFILE].fdMode = Config::ESP_fdMode;
        FileUtils::fileTypes[DISK_ESPFILE].fileSearch = Config::ESP_fileSearch;

        printf("------------------------------------\n");

        LoadSnapshot(Config::ram_file,"","",0xff);

        Config::last_ram_file = Config::ram_file;
        Config::ram_file = NO_RAM_FILE;
        Config::save("ram");

    }

    if (Config::slog_on) showMemInfo("Setup finished.");

    // printf("Waiting boot keys\n");
    bootKeyboard(5); // BOOTKEYS: Read keyboard checking boot keys
    // printf("End Waiting boot keys\n");

    if (ps2kbd && PS2Controller.keyboard()->isKeyboardAvailable()) PS2Controller.keyboard()->setScancodeSet(2);
    if (ps2kbd2 && PS2Controller.keybjoystick()->isKeyboardAvailable()) PS2Controller.keybjoystick()->setScancodeSet(2);

    // Start audio input calculating values for current machine
    AudioIn::Start();
    if (Config::AudioInMode) AudioIn::Play();

    double boottime = esp_timer_get_time() - ts_start;
    printf("Boot time: %6.2f\n", boottime / 1000000);

    // Load mouse test
    // LoadSnapshot("/sd/Tests/mouse.z80","","",0xff);

}

//=======================================================================================
// RESET
//=======================================================================================
void ESPectrum::reset() {

    AudioIn::Pause();

    switch (Config::DiskCtrl) {
        case 0:
        case 1:
        case 2:
            MemESP::rom[4] = (uint8_t *) rom_trdos_503;
            break;
        case 3:
        case 4:
            MemESP::rom[4] = (uint8_t *) rom_trdos_505D;
    }

    // Ports
    for (int i = 0; i < 128; i++) Ports::port[i] = 0xFF; // 0xBF;
    Ports::port254 = 0;
    Ports::LastOutTo1FFD = 0;
    if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0; // Kempston
    if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff; // Fuller

    ESPectrum::joystick1 = Config::joystick1;
    ESPectrum::joystick2 = Config::joystick2;

    // Read joystick default definition
    for (int n = 0; n < 24; n++)
        ESPectrum::JoyVKTranslation[n] = (fabgl::VirtualKey) Config::joydef[n];

    MemESP::Reset(); // Reset Memory

    VIDEO::Reset();

    // Reinit disk controller
    if (Config::DiskCtrl != 0 || Z80Ops::isPentagon) {
        rvmWD1793Reset(&fdd);
    }

    Tape::tapeStatus = TAPE_STOPPED;
    Tape::tapePhase = TAPE_PHASE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    if (Z80Ops::is128 || Z80Ops::is2a3) { // Apply pulse length compensation for 128K
        Tape::tapeCompensation = FACTOR128K;
    } else if ((Config::arch=="TK90X" || Config::arch == "TK95") && Config::ALUTK > 0) { // Apply pulse length compensation for Microdigital ALU
        Tape::tapeCompensation = FACTORALUTK;
    } else
        Tape::tapeCompensation = 1;

    // Set block timings if there's a tape loaded and is a .tap
    if (Tape::tape != NULL && Tape::tapeFileType == TAPE_FTYPE_TAP) {
        Tape::TAP_setBlockTimings();
    }

    // Empty audio buffers
    for (int i=0;i<ESP_AUDIO_SAMPLES_PENTAGON;i++) {
        overSamplebuf[i]=0;
        audioBuffer[i]=0;
        AySound::SamplebufAY[i]=0;
    }
    lastaudioBit=0;

    // Set samples per frame and AY_emu flag depending on arch
    int prevAudio_freq = Audio_freq[ESP_delay];
    if (Config::arch == "48K") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_48;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_48;
        audioAYDivider = ESP_AUDIO_AY_DIV_48;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_48;
        AY_emu = Config::AY48;
        Audio_freq[0] = ESP_AUDIO_FREQ_48;
        Audio_freq[1] = ESP_AUDIO_FREQ_48;
        Audio_freq[2] = ESP_AUDIO_FREQ_48_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_48_150SPEED;
    } else if (Config::arch == "TK90X" || Config::arch == "TK95") {

        switch (Config::ALUTK) {
        case 0:
            samplesPerFrame=ESP_AUDIO_SAMPLES_48;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_48;
            audioAYDivider = ESP_AUDIO_AY_DIV_48;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_48;
            Audio_freq[0] = ESP_AUDIO_FREQ_48;
            Audio_freq[1] = ESP_AUDIO_FREQ_48;
            Audio_freq[2] = ESP_AUDIO_FREQ_48_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_48_150SPEED;
            break;
        case 1:
            samplesPerFrame=ESP_AUDIO_SAMPLES_TK_50;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_TK_50;
            audioAYDivider = ESP_AUDIO_AY_DIV_TK_50;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_TK_50;
            Audio_freq[0] = ESP_AUDIO_FREQ_TK_50;
            Audio_freq[1] = ESP_AUDIO_FREQ_TK_50;
            Audio_freq[2] = ESP_AUDIO_FREQ_TK_50_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_TK_50_150SPEED;
            break;
        case 2:
            samplesPerFrame=ESP_AUDIO_SAMPLES_TK_60;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_TK_60;
            audioAYDivider = ESP_AUDIO_AY_DIV_TK_60;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_TK_60;
            Audio_freq[0] = ESP_AUDIO_FREQ_TK_60;
            Audio_freq[1] = ESP_AUDIO_FREQ_TK_60;
            Audio_freq[2] = ESP_AUDIO_FREQ_TK_60_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_TK_60_150SPEED;
        }

        AY_emu = Config::AY48;

    } else if (Config::arch == "128K") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_128;
        audioAYDivider = ESP_AUDIO_AY_DIV_128;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_128;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_128;
        Audio_freq[1] = ESP_AUDIO_FREQ_128;
        Audio_freq[2] = ESP_AUDIO_FREQ_128_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_128_150SPEED;
    } else if (Config::arch == "+2A") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_128;
        audioAYDivider = ESP_AUDIO_AY_DIV_128;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_128;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_128;
        Audio_freq[1] = ESP_AUDIO_FREQ_128;
        Audio_freq[2] = ESP_AUDIO_FREQ_128_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_128_150SPEED;
    } else if (Config::arch == "Pentagon") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_PENTAGON;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_PENTAGON;
        audioAYDivider = ESP_AUDIO_AY_DIV_PENTAGON;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_PENTAGON;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_PENTAGON;
        Audio_freq[1] = ESP_AUDIO_FREQ_PENTAGON;
        Audio_freq[2] = ESP_AUDIO_FREQ_PENTAGON_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_PENTAGON_150SPEED;
    }

    audioCOVOXDivider = audioAYDivider;
    covoxData[0] = covoxData[1] = covoxData[2] = covoxData[3] = 0;

    if (Config::EarBoost) AY_emu = false; // Disable AY emulation if tape player mode is set

    ESPoffset = 0;

    // Readjust output pwmaudio frequency if needed
    if (prevAudio_freq != Audio_freq[ESP_delay]) {
        // printf("Resetting pwmaudio to freq: %d\n",Audio_freq);
        esp_err_t res;
        res = pwm_audio_set_sample_rate(Audio_freq[ESP_delay]);
        if (res != ESP_OK) {
            printf("Can't set sample rate\n");
        }
    }

    // Reset AY emulation
    AySound::init();
    AySound::set_sound_format(Audio_freq[ESP_delay],1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    aud_active_sources = (Config::Covox & 0x01) | (AY_emu << 1);

    CPU::reset();

    // Restart Audio Input recalculating new values for current machine
    AudioIn::Start();
    AudioIn::Resume();

}

//=======================================================================================
// KEYBOARD / KEMPSTON
//=======================================================================================
IRAM_ATTR bool ESPectrum::readKbd(fabgl::VirtualKeyItem *Nextkey) {

    bool r = PS2Controller.keyboard()->getNextVirtualKey(Nextkey);
    // Global keys
    if (Nextkey->down) {
        if (Nextkey->vk == fabgl::VK_PRINTSCREEN) { // Capture framebuffer to BMP file in SD Card (thx @dcrespo3d!)
            CaptureToBmp();
            r = false;
        } else
        if (Nextkey->vk == fabgl::VK_SCROLLLOCK) { // Change CursorAsJoy setting
            Config::CursorAsJoy = !Config::CursorAsJoy;
            if(ps2kbd)
                PS2Controller.keyboard()->setLEDs(false,false,Config::CursorAsJoy);
            if(ps2kbd2)
                PS2Controller.keybjoystick()->setLEDs(false, false, Config::CursorAsJoy);
            Config::save("CursorAsJoy");
            r = false;
        }
    }

    return r;
}

//
// Read second ps/2 port and inject on first queue
//
IRAM_ATTR void ESPectrum::readKbdJoy() {

    if (ps2kbd2) {

        fabgl::VirtualKeyItem NextKey;
        auto KbdJoy = PS2Controller.keybjoystick();

        while (KbdJoy->virtualKeyAvailable()) {
            PS2Controller.keybjoystick()->getNextVirtualKey(&NextKey);
            ESPectrum::PS2Controller.keyboard()->injectVirtualKey(NextKey.vk, NextKey.down, false);
            // printf("NextKey: %d, %d\n",NextKey.vk,NextKey.down);
        }

    }

}

uint8_t ESPectrum::joystick1;
uint8_t ESPectrum::joystick2;
fabgl::VirtualKey ESPectrum::JoyVKTranslation[24];
//     fabgl::VK_FULLER_LEFT, // Left
//     fabgl::VK_FULLER_RIGHT, // Right
//     fabgl::VK_FULLER_UP, // Up
//     fabgl::VK_FULLER_DOWN, // Down
//     fabgl::VK_S, // Start
//     fabgl::VK_M, // Mode
//     fabgl::VK_FULLER_FIRE, // A
//     fabgl::VK_9, // B
//     fabgl::VK_SPACE, // C
//     fabgl::VK_X, // X
//     fabgl::VK_Y, // Y
//     fabgl::VK_Z, // Z

fabgl::VirtualKey ESPectrum::VK_ESPECTRUM_FIRE1 = fabgl::VK_NONE;
fabgl::VirtualKey ESPectrum::VK_ESPECTRUM_FIRE2 = fabgl::VK_NONE;
fabgl::VirtualKey ESPectrum::VK_ESPECTRUM_TAB = fabgl::VK_TAB;
fabgl::VirtualKey ESPectrum::VK_ESPECTRUM_GRAVEACCENT = fabgl::VK_GRAVEACCENT;

int ESPectrum::zxDelay = 0;

int32_t ESPectrum::mouseX = 0;
int32_t ESPectrum::mouseY = 0;
bool ESPectrum::mouseButtonL = 0;
bool ESPectrum::mouseButtonR = 0;

// static uint8_t PS2cols[8] = { 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf };
static uint8_t PS2cols[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

IRAM_ATTR void ESPectrum::processKeyboard() {

    bool r = false;

    // while (Mouse->availableStatus()) {

    //     mstat = Mouse->getNextStatus();

    //     printf("%d %d %d %d % d %d\n",mstat.X,mstat.Y,mstat.buttons.left,mstat.buttons.middle,mstat.buttons.right,mstat.wheelDelta);

    // }

    // MouseDelta delta;

    // while (Mouse->deltaAvailable()) {

    //      if (Mouse->getNextDelta(&delta)) {

    //         ESPectrum::mouseX = (ESPectrum::mouseX + delta.deltaX) & 0xff;
    //         ESPectrum::mouseY = (ESPectrum::mouseY + delta.deltaY) & 0xff;
    //         ESPectrum::mouseButtonL = delta.buttons.left;
    //         ESPectrum::mouseButtonR = delta.buttons.right;

    //      } else break;

    // }

    readKbdJoy();

    if (ps2kbd) {

        auto Kbd = PS2Controller.keyboard();
        // auto Mouse = PS2Controller.mouse();
        fabgl::VirtualKeyItem NextKey;
        fabgl::VirtualKey KeytoESP;
        bool Kdown;
        bool j[10] = { true, true, true, true, true, true, true, true, true, true };
        bool jShift = true;

        while (Kbd->virtualKeyAvailable()) {

            r = readKbd(&NextKey);

            if (r) {

                KeytoESP = NextKey.vk;
                Kdown = NextKey.down;

                // printf("KeytoESP: %d, Kdown: %d\n",KeytoESP,Kdown);

                if (KeytoESP >= fabgl::VK_JOY1LEFT && KeytoESP <= fabgl::VK_JOY2Z) {
                    fabgl::VirtualKey jk = JoyVKTranslation[KeytoESP - fabgl::VK_JOY1LEFT];
                    if (jk != fabgl::VK_NONE)
                        ESPectrum::PS2Controller.keyboard()->injectVirtualKey(jk, Kdown, false);
                    // printf("KeytoESP: %d, JoyVKTranslation: %d\n",KeytoESP,jk);
                    continue;
                }

                if ((Kdown) && ((KeytoESP >= fabgl::VK_F1 && KeytoESP <= fabgl::VK_F12) || KeytoESP == fabgl::VK_PAUSE || KeytoESP == fabgl::VK_VOLUMEUP || KeytoESP == fabgl::VK_VOLUMEDOWN || KeytoESP == fabgl::VK_VOLUMEMUTE)) {

                    int64_t osd_start = esp_timer_get_time();

                    OSD::do_OSD(KeytoESP, Kbd->isVKDown(fabgl::VK_LCTRL) || Kbd->isVKDown(fabgl::VK_RCTRL), Kbd->isVKDown(fabgl::VK_LSHIFT) || Kbd->isVKDown(fabgl::VK_RSHIFT));

                    Kbd->emptyVirtualKeyQueue();

                    // Inject F1, RETURN and ESC keys release for not generating a key press
                    Kbd->injectVirtualKey(fabgl::VK_F1, false, false);
                    Kbd->injectVirtualKey(fabgl::VK_RETURN, false, false);
                    Kbd->injectVirtualKey(fabgl::VK_ESCAPE, false, false);

                    if (ps2kbd2) {

                        PS2Controller.keybjoystick()->emptyVirtualKeyQueue();

                        // Inject all joystick actions release
                        for (int n = 248; n <= 271; n++)
                            // printf("Injecting: %d\n",n);
                            PS2Controller.keybjoystick()->injectVirtualKey((fabgl::VirtualKey) n, false, false);

                    }

                    // Set all zx keys as not pressed
                    for (uint8_t i = 0; i < 8; i++) ZXKeyb::ZXcols[i] = 0xff; // 0xbf;
                    zxDelay = 15;

                    // totalseconds = 0;
                    // totalsecondsnodelay = 0;
                    // VIDEO::framecnt = 0;

                    // Refresh border
                    VIDEO::brdnextframe = true;

                    AudioIn::Resume();

                    ESPectrum::ts_start += esp_timer_get_time() - osd_start;

                    return;

                }

                // Reset keys
                if (Kdown && NextKey.LALT) {
                    if (NextKey.CTRL) {
                        if (KeytoESP == fabgl::VK_DELETE) {
                            // printf("Ctrl + Alt + Supr!\n");
                            // ESP host reset
                            Config::ram_file = NO_RAM_FILE;
                            Config::save("ram");
                            OSD::esp_hard_reset();
                        } else if (KeytoESP == fabgl::VK_BACKSPACE) {
                            // printf("Ctrl + Alt + backSpace!\n");
                            // Hard
                            if (Config::ram_file != NO_RAM_FILE) {
                                Config::ram_file = NO_RAM_FILE;
                            }
                            Config::last_ram_file = NO_RAM_FILE;
                            ESPectrum::reset();
                            return;
                        }
                    } else if (KeytoESP == fabgl::VK_BACKSPACE) {
                        // printf("Alt + backSpace!\n");
                        // Soft reset
                        if (Config::last_ram_file != NO_RAM_FILE) {
                            LoadSnapshot(Config::last_ram_file,"","",0xff);
                            Config::ram_file = Config::last_ram_file;
                        } else ESPectrum::reset();
                        return;
                    }
                }

                if (ESPectrum::joystick1 == JOY_KEMPSTON || ESPectrum::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0;
                if (ESPectrum::joystick1 == JOY_FULLER || ESPectrum::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff;

                if (ESPectrum::joystick1 == JOY_KEMPSTON || ESPectrum::joystick2 == JOY_KEMPSTON) {

                    for (int i = fabgl::VK_KEMPSTON_RIGHT; i <= fabgl::VK_KEMPSTON_ALTFIRE; i++)
                        if (Kbd->isVKDown((fabgl::VirtualKey) i)) {
                            // printf("Kempston: %d\n",i);
                            bitWrite(Ports::port[0x1f], i - fabgl::VK_KEMPSTON_RIGHT, 1);
                        }

                }

                if (ESPectrum::joystick1 == JOY_FULLER || ESPectrum::joystick2 == JOY_FULLER) {

                    // Fuller
                    if (Kbd->isVKDown(fabgl::VK_FULLER_RIGHT)) {
                        bitWrite(Ports::port[0x7f], 3, 0);
                    }

                    if (Kbd->isVKDown(fabgl::VK_FULLER_LEFT)) {
                        bitWrite(Ports::port[0x7f], 2, 0);
                    }

                    if (Kbd->isVKDown(fabgl::VK_FULLER_DOWN)) {
                        bitWrite(Ports::port[0x7f], 1, 0);
                    }

                    if (Kbd->isVKDown(fabgl::VK_FULLER_UP)) {
                        bitWrite(Ports::port[0x7f], 0, 0);
                    }

                    if (Kbd->isVKDown(fabgl::VK_FULLER_FIRE)) {
                        bitWrite(Ports::port[0x7f], 7, 0);
                    }

                }

                jShift = !(Kbd->isVKDown(fabgl::VK_LSHIFT) || Kbd->isVKDown(fabgl::VK_RSHIFT));

                if (Config::CursorAsJoy) {

                    // Kempston Joystick emulation
                    if (Config::joyPS2 == JOYPS2_KEMPSTON) {

                        if (Kbd->isVKDown(fabgl::VK_RIGHT)) {
                            j[8] = jShift;
                            bitWrite(Ports::port[0x1f], 0, j[8]);
                        }

                        if (Kbd->isVKDown(fabgl::VK_LEFT)) {
                            j[5] = jShift;
                            bitWrite(Ports::port[0x1f], 1, j[5]);
                        }

                        if (Kbd->isVKDown(fabgl::VK_DOWN)) {
                            j[6] = jShift;
                            bitWrite(Ports::port[0x1f], 2, j[6]);
                        }

                        if (Kbd->isVKDown(fabgl::VK_UP)) {
                            j[7] = jShift;
                            bitWrite(Ports::port[0x1f], 3, j[7]);
                        }

                    // Fuller Joystick emulation
                    } else if (Config::joyPS2 == JOYPS2_FULLER) {

                        if (Kbd->isVKDown(fabgl::VK_RIGHT)) {
                            j[8] = jShift;
                            bitWrite(Ports::port[0x7f], 3, !j[8]);
                        }

                        if (Kbd->isVKDown(fabgl::VK_LEFT)) {
                            j[5] = jShift;
                            bitWrite(Ports::port[0x7f], 2, !j[5]);
                        }

                        if (Kbd->isVKDown(fabgl::VK_DOWN)) {
                            j[6] = jShift;
                            bitWrite(Ports::port[0x7f], 1, !j[6]);
                        }

                        if (Kbd->isVKDown(fabgl::VK_UP)) {
                            j[7] = jShift;
                            bitWrite(Ports::port[0x7f], 0, !j[7]);
                        }

                    } else if (Config::joyPS2 == JOYPS2_CURSOR) {

                        j[5] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                        j[8] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                        j[7] =  !Kbd->isVKDown(fabgl::VK_UP);
                        j[6] =  !Kbd->isVKDown(fabgl::VK_DOWN);

                    } else if (Config::joyPS2 == JOYPS2_SINCLAIR1) { // Right Sinclair

                        if (jShift) {
                            j[9] =  !Kbd->isVKDown(fabgl::VK_UP);
                            j[8] =  !Kbd->isVKDown(fabgl::VK_DOWN);
                            j[7] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                            j[6] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                        } else {
                            j[5] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                            j[8] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                            j[7] =  !Kbd->isVKDown(fabgl::VK_UP);
                            j[6] =  !Kbd->isVKDown(fabgl::VK_DOWN);
                        }

                    } else if (Config::joyPS2 == JOYPS2_SINCLAIR2) { // Left Sinclair

                        if (jShift) {
                            j[4] =  !Kbd->isVKDown(fabgl::VK_UP);
                            j[3] =  !Kbd->isVKDown(fabgl::VK_DOWN);
                            j[2] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                            j[1] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                        } else {
                            j[5] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                            j[8] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                            j[7] =  !Kbd->isVKDown(fabgl::VK_UP);
                            j[6] =  !Kbd->isVKDown(fabgl::VK_DOWN);
                        }

                    }

                } else {

                    // Cursor Keys
                    if (Kbd->isVKDown(fabgl::VK_RIGHT)) {
                        jShift = false;
                        j[8] = jShift;
                    }

                    if (Kbd->isVKDown(fabgl::VK_LEFT)) {
                        jShift = false;
                        j[5] = jShift;
                    }

                    if (Kbd->isVKDown(fabgl::VK_DOWN)) {
                        jShift = false;
                        j[6] = jShift;
                    }

                    if (Kbd->isVKDown(fabgl::VK_UP)) {
                        jShift = false;
                        j[7] = jShift;
                    }

                }

                // Keypad PS/2 Joystick emulation
                if (Config::joyPS2 == JOYPS2_KEMPSTON) {

                    if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                        bitWrite(Ports::port[0x1f], 0, 1);
                    }

                    if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                        bitWrite(Ports::port[0x1f], 1, 1);
                    }

                    if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                        bitWrite(Ports::port[0x1f], 2, 1);
                    }

                    if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                        bitWrite(Ports::port[0x1f], 3, 1);
                    }

                    if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECTRUM_FIRE1)) {
                        bitWrite(Ports::port[0x1f], 4, 1);
                    }

                    if (Kbd->isVKDown(fabgl::VK_SLASH) || /*Kbd->isVKDown(fabgl::VK_QUESTION) ||*/Kbd->isVKDown(fabgl::VK_RGUI) || Kbd->isVKDown(fabgl::VK_APPLICATION) || Kbd->isVKDown(VK_ESPECTRUM_FIRE2)) {
                        bitWrite(Ports::port[0x1f], 5, 1);
                    }

                } else if (Config::joyPS2 == JOYPS2_FULLER) {

                    if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                        bitWrite(Ports::port[0x7f], 3, 0);
                    }

                    if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                        bitWrite(Ports::port[0x7f], 2, 0);
                    }

                    if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                        bitWrite(Ports::port[0x7f], 1, 0);
                    }

                    if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                        bitWrite(Ports::port[0x7f], 0, 0);
                    }

                    if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECTRUM_FIRE1)) {
                        bitWrite(Ports::port[0x7f], 7, 0);
                    }

                } else if (Config::joyPS2 == JOYPS2_CURSOR) {

                    if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                        jShift = true;
                        j[5] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                        jShift = true;
                        j[8] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                        jShift = true;
                        j[7] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                        jShift = true;
                        j[6] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECTRUM_FIRE1)) {
                        jShift = true;
                        j[0] = false;
                    };

                } else if (Config::joyPS2 == JOYPS2_SINCLAIR1) { // Right Sinclair

                    if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                        jShift = true;
                        j[6] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                        jShift = true;
                        j[7] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                        jShift = true;
                        j[9] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                        jShift = true;
                        j[8] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECTRUM_FIRE1)) {
                        jShift = true;
                        j[0] = false;
                    };

                } else if (Config::joyPS2 == JOYPS2_SINCLAIR2) { // Left Sinclair

                    if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                        jShift = true;
                        j[1] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                        jShift = true;
                        j[2] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                        jShift = true;
                        j[4] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                        jShift = true;
                        j[3] = false;
                    };

                    if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECTRUM_FIRE1)) {
                        jShift = true;
                        j[5] = false;
                    };

                }

                // Check keyboard status and map it to Spectrum Ports

                bitWrite(PS2cols[0], 0, (jShift)
                    & (!Kbd->isVKDown(fabgl::VK_BACKSPACE))
                    & (!Kbd->isVKDown(fabgl::VK_CAPSLOCK)) // Caps lock
                    &   (!Kbd->isVKDown(VK_ESPECTRUM_GRAVEACCENT)) // Edit
                    &   (!Kbd->isVKDown(VK_ESPECTRUM_TAB)) // Extended mode
                    &   (!Kbd->isVKDown(fabgl::VK_ESCAPE)) // Break
                    ); // CAPS SHIFT
                bitWrite(PS2cols[0], 1, (!Kbd->isVKDown(fabgl::VK_Z)) & (!Kbd->isVKDown(fabgl::VK_z)));
                bitWrite(PS2cols[0], 2, (!Kbd->isVKDown(fabgl::VK_X)) & (!Kbd->isVKDown(fabgl::VK_x)));
                bitWrite(PS2cols[0], 3, (!Kbd->isVKDown(fabgl::VK_C)) & (!Kbd->isVKDown(fabgl::VK_c)));
                bitWrite(PS2cols[0], 4, (!Kbd->isVKDown(fabgl::VK_V)) & (!Kbd->isVKDown(fabgl::VK_v)));

                bitWrite(PS2cols[1], 0, (!Kbd->isVKDown(fabgl::VK_A)) & (!Kbd->isVKDown(fabgl::VK_a)));
                bitWrite(PS2cols[1], 1, (!Kbd->isVKDown(fabgl::VK_S)) & (!Kbd->isVKDown(fabgl::VK_s)));
                bitWrite(PS2cols[1], 2, (!Kbd->isVKDown(fabgl::VK_D)) & (!Kbd->isVKDown(fabgl::VK_d)));
                bitWrite(PS2cols[1], 3, (!Kbd->isVKDown(fabgl::VK_F)) & (!Kbd->isVKDown(fabgl::VK_f)));
                bitWrite(PS2cols[1], 4, (!Kbd->isVKDown(fabgl::VK_G)) & (!Kbd->isVKDown(fabgl::VK_g)));

                bitWrite(PS2cols[2], 0, (!Kbd->isVKDown(fabgl::VK_Q)) & (!Kbd->isVKDown(fabgl::VK_q)));
                bitWrite(PS2cols[2], 1, (!Kbd->isVKDown(fabgl::VK_W)) & (!Kbd->isVKDown(fabgl::VK_w)));
                bitWrite(PS2cols[2], 2, (!Kbd->isVKDown(fabgl::VK_E)) & (!Kbd->isVKDown(fabgl::VK_e)));
                bitWrite(PS2cols[2], 3, (!Kbd->isVKDown(fabgl::VK_R)) & (!Kbd->isVKDown(fabgl::VK_r)));
                bitWrite(PS2cols[2], 4, (!Kbd->isVKDown(fabgl::VK_T)) & (!Kbd->isVKDown(fabgl::VK_t)));

                bitWrite(PS2cols[3], 0, (!Kbd->isVKDown(fabgl::VK_1)) & (!Kbd->isVKDown(fabgl::VK_EXCLAIM))
                                    &   (!Kbd->isVKDown(VK_ESPECTRUM_GRAVEACCENT)) // Edit
                                    & (j[1]));
                bitWrite(PS2cols[3], 1, (!Kbd->isVKDown(fabgl::VK_2)) & (!Kbd->isVKDown(fabgl::VK_AT))
                                    &   (!Kbd->isVKDown(fabgl::VK_CAPSLOCK)) // Caps lock
                                    & (j[2])
                                    );
                bitWrite(PS2cols[3], 2, (!Kbd->isVKDown(fabgl::VK_3)) & (!Kbd->isVKDown(fabgl::VK_HASH)) & (j[3]));
                bitWrite(PS2cols[3], 3, (!Kbd->isVKDown(fabgl::VK_4)) & (!Kbd->isVKDown(fabgl::VK_DOLLAR)) & (j[4]));
                bitWrite(PS2cols[3], 4, (!Kbd->isVKDown(fabgl::VK_5)) & (!Kbd->isVKDown(fabgl::VK_PERCENT)) & (j[5]));

                bitWrite(PS2cols[4], 0, (!Kbd->isVKDown(fabgl::VK_0)) & (!Kbd->isVKDown(fabgl::VK_RIGHTPAREN)) & (!Kbd->isVKDown(fabgl::VK_BACKSPACE)) & (j[0]));
                bitWrite(PS2cols[4], 1, !Kbd->isVKDown(fabgl::VK_9) & (!Kbd->isVKDown(fabgl::VK_LEFTPAREN)) & (j[9]));
                bitWrite(PS2cols[4], 2, (!Kbd->isVKDown(fabgl::VK_8)) & (!Kbd->isVKDown(fabgl::VK_ASTERISK)) & (j[8]));
                bitWrite(PS2cols[4], 3, (!Kbd->isVKDown(fabgl::VK_7)) & (!Kbd->isVKDown(fabgl::VK_AMPERSAND)) & (j[7]));
                bitWrite(PS2cols[4], 4, (!Kbd->isVKDown(fabgl::VK_6)) & (!Kbd->isVKDown(fabgl::VK_CARET)) & (j[6]));

                bitWrite(PS2cols[5], 0, (!Kbd->isVKDown(fabgl::VK_P)) & (!Kbd->isVKDown(fabgl::VK_p))
                                    &   (!Kbd->isVKDown(fabgl::VK_QUOTE)) // Double quote
                                    );
                bitWrite(PS2cols[5], 1, (!Kbd->isVKDown(fabgl::VK_O)) & (!Kbd->isVKDown(fabgl::VK_o))
                                    &   (!Kbd->isVKDown(fabgl::VK_SEMICOLON)) // Semicolon
                                    );
                bitWrite(PS2cols[5], 2, (!Kbd->isVKDown(fabgl::VK_I)) & (!Kbd->isVKDown(fabgl::VK_i)));
                bitWrite(PS2cols[5], 3, (!Kbd->isVKDown(fabgl::VK_U)) & (!Kbd->isVKDown(fabgl::VK_u)));
                bitWrite(PS2cols[5], 4, (!Kbd->isVKDown(fabgl::VK_Y)) & (!Kbd->isVKDown(fabgl::VK_y)));

                bitWrite(PS2cols[6], 0, !Kbd->isVKDown(fabgl::VK_RETURN));
                bitWrite(PS2cols[6], 1, (!Kbd->isVKDown(fabgl::VK_L)) & (!Kbd->isVKDown(fabgl::VK_l)));
                bitWrite(PS2cols[6], 2, (!Kbd->isVKDown(fabgl::VK_K)) & (!Kbd->isVKDown(fabgl::VK_k)));
                bitWrite(PS2cols[6], 3, (!Kbd->isVKDown(fabgl::VK_J)) & (!Kbd->isVKDown(fabgl::VK_j)));
                bitWrite(PS2cols[6], 4, (!Kbd->isVKDown(fabgl::VK_H)) & (!Kbd->isVKDown(fabgl::VK_h)));

                bitWrite(PS2cols[7], 0, (!Kbd->isVKDown(fabgl::VK_SPACE))
                                &   (!Kbd->isVKDown(fabgl::VK_ESCAPE)) // Break
                );
                bitWrite(PS2cols[7], 1, (!Kbd->isVKDown(fabgl::VK_LCTRL)) // SYMBOL SHIFT
                                    &   (!Kbd->isVKDown(fabgl::VK_RCTRL))
                                    &   (!Kbd->isVKDown(fabgl::VK_COMMA)) // Comma
                                    &   (!Kbd->isVKDown(fabgl::VK_PERIOD)) // Period
                                    &   (!Kbd->isVKDown(fabgl::VK_SEMICOLON)) // Semicolon
                                    &   (!Kbd->isVKDown(fabgl::VK_QUOTE)) // Double quote
                                    &   (!Kbd->isVKDown(VK_ESPECTRUM_TAB)) // Extended mode
                                    ); // SYMBOL SHIFT
                bitWrite(PS2cols[7], 2, (!Kbd->isVKDown(fabgl::VK_M)) & (!Kbd->isVKDown(fabgl::VK_m))
                                    &   (!Kbd->isVKDown(fabgl::VK_PERIOD)) // Period
                                    );
                bitWrite(PS2cols[7], 3, (!Kbd->isVKDown(fabgl::VK_N)) & (!Kbd->isVKDown(fabgl::VK_n))
                                    &   (!Kbd->isVKDown(fabgl::VK_COMMA)) // Comma
                                    );
                bitWrite(PS2cols[7], 4, (!Kbd->isVKDown(fabgl::VK_B)) & (!Kbd->isVKDown(fabgl::VK_b)));

            }

        }

    }

    if (ZXKeyb::Exists) { // START - ZXKeyb Exists

        // Detect and process physical kbd menu key combinations
        if (ZXKBD_SS) {

            zxDelay = 15;

            int64_t osd_start = esp_timer_get_time();

            if (ZXKBD_CS) {

                if (ZXKBD_1) OSD::do_OSD(fabgl::VK_F1,0,0);
                else if (ZXKBD_2) OSD::do_OSD(fabgl::VK_F2,0,0);
                else if (ZXKBD_3) OSD::do_OSD(fabgl::VK_F3,0,0);
                else if (ZXKBD_4) OSD::do_OSD(fabgl::VK_F4,0,0);
                else if (ZXKBD_5) OSD::do_OSD(fabgl::VK_F5,0,0);
                else if (ZXKBD_6) OSD::do_OSD(fabgl::VK_F6,0,0);
                else if (ZXKBD_7) OSD::do_OSD(fabgl::VK_F7,0,0);
                else if (ZXKBD_8) OSD::do_OSD(fabgl::VK_F8,0,0);
                else if (ZXKBD_9) OSD::do_OSD(fabgl::VK_F9,0,0);
                else if (ZXKBD_0) OSD::do_OSD(fabgl::VK_F10,0,0);
                else if (ZXKBD_Q) OSD::do_OSD(fabgl::VK_F11,0,0);
                else if (ZXKBD_W) OSD::do_OSD(fabgl::VK_F12,0,0);
                else if (ZXKBD_P) OSD::do_OSD(fabgl::VK_PAUSE,0,0); // P -> Pause
                else if (ZXKBD_I) OSD::do_OSD(fabgl::VK_F1,0,true); // I -> Info
                else if (ZXKBD_E) OSD::do_OSD(fabgl::VK_F5,0,true); // E -> Eject tape
                else if (ZXKBD_U) OSD::do_OSD(fabgl::VK_F9,0,true); // U -> Uart test
                else if (ZXKBD_R) OSD::do_OSD(fabgl::VK_F11,true,0); // R -> Reset to TR-DOS
                else if (ZXKBD_T) OSD::do_OSD(fabgl::VK_F4,0,true); // T -> Tape browser
                else if (ZXKBD_B) CaptureToBmp(); // B -> BMP capture
                else if (ZXKBD_O) OSD::do_OSD(fabgl::VK_F9,true,0); // O -> Poke
                else if (ZXKBD_N) OSD::do_OSD(fabgl::VK_F10,true,0); // N -> NMI
                else if (ZXKBD_K) OSD::do_OSD(fabgl::VK_F1,true,0); // K -> Help / Kbd layout
                else if (ZXKBD_L) OSD::do_OSD(fabgl::VK_F2, 0, true); // L -> Audio in toggle
                else if (ZXKBD_S) OSD::do_OSD(fabgl::VK_F3,true,0); // S -> 2nd PS/2 device
                else if (ZXKBD_D) OSD::do_OSD(fabgl::VK_F4,true,0); // D -> Mouse DPI
                else if (ZXKBD_F) OSD::do_OSD(fabgl::VK_F2,true,0); // F -> Fast (Turbo) mode
                else if (ZXKBD_Z) OSD::do_OSD(fabgl::VK_F5,true,0); // Z -> CenterH
                else if (ZXKBD_X) OSD::do_OSD(fabgl::VK_F6,true,0); // X -> CenterH
                else if (ZXKBD_C) OSD::do_OSD(fabgl::VK_F7,true,0); // C -> CenterV
                else if (ZXKBD_V) OSD::do_OSD(fabgl::VK_F8,true,0); // V -> CenterV
                else zxDelay = 0;

            } else {

                if (Config::AudioInMode && Config::AudioInGPIO == KM_COL_0 && ZXKBD_L)  // L -> Audio in toggle
                    OSD::do_OSD(fabgl::VK_F2, 0, true);
                else
                    zxDelay = 0;

            }

            if (zxDelay) {

                // Set all keys as not pressed
                for (uint8_t i = 0; i < 8; i++) ZXKeyb::ZXcols[i] = 0xff; // 0xbf;

                // Refresh border
                VIDEO::brdnextframe = true;

                AudioIn::Resume();

                ESPectrum::ts_start += esp_timer_get_time() - osd_start;

                return;
            }

        }

        // Combine both keyboards
        for (uint8_t rowidx = 0; rowidx < 8; rowidx++) {
            Ports::port[rowidx] = PS2cols[rowidx] & ZXKeyb::ZXcols[rowidx];
        }

    } else {

        if (r) {
            for (uint8_t rowidx = 0; rowidx < 8; rowidx++) {
                Ports::port[rowidx] = PS2cols[rowidx];
            }
        }

    }

    if (Config::rst_button) {

        // printf("RST button GPIO %d\n",Config::rst_button);

        // RESET BUTTON
        static bool rst_state = true;
        static int rst_framecount = 0;
        if (gpio_get_level((gpio_num_t)Config::rst_button) == 0) {

            rst_framecount = rst_state ? 0 : rst_framecount + 1;

            if (rst_framecount == 100) {  // Action on long button press
                OSD::do_OSD(fabgl::VK_F12,0,0);
            }

            rst_state = false;

        } else {
            if ((rst_state == false) && (rst_framecount >= 5)) {

                int64_t osd_start = esp_timer_get_time();

                switch(Config::rst_btn_action) {
                    case RST_ACTION_RESET:
                        OSD::do_OSD(fabgl::VK_F11,0,0);
                        break;
                    case RST_ACTION_NMI:
                        Z80::triggerNMI();
                        break;
                    case RST_ACTION_STATS:
                        OSD::do_OSD(fabgl::VK_F8,0,0);
                        break;
                    case RST_ACTION_MENU:
                        ESPectrum::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_F1, true, false);
                        break;
                }

                // Refresh border
                VIDEO::brdnextframe = true;

                ESPectrum::ts_start += esp_timer_get_time() - osd_start;

            }
            rst_state = true;
        }

    }

}

//=======================================================================================
// AUDIO
//=======================================================================================

// static int64_t task2start, task2elapsed;

IRAM_ATTR void ESPectrum::audioTask(void *unused) {

    size_t written;

    // PWM Audio Init
    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_8_BIT;
    pac.gpio_num_left      = SPEAKER_PIN;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = -1;
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 3072; /* 4096; */

    pwm_audio_init(&pac);
    pwm_audio_set_param(Audio_freq[ESP_delay],LEDC_TIMER_8_BIT,1);
    pwm_audio_start();
    pwm_audio_set_volume(aud_volume);

    for (;;) {

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        if (ESP_delay) pwm_audio_write(audioBuffer, samplesPerFrame, &written, 5 / portTICK_PERIOD_MS);

        // task2start = esp_timer_get_time();

        // Process ZX Keyboard
        if (ZXKeyb::Exists) {

            if (zxDelay > 0)

                zxDelay--;

            else {

                // Process physical keyboard
                ZXKeyb::process();

            }

        }

        // task2elapsed = esp_timer_get_time() - task2start;

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        // task2start = esp_timer_get_time();

        if (ESP_delay) {

            // Finish fill of beeper oversampled audio buffers
            overSamplebuf[faudbufcntover++] = faudioBitBuf + (flastaudioBit * (audioSampleDivider - faudioBitbufCount));
            faudioBitBuf = flastaudioBit * audioSampleDivider;
            for(;faudbufcntover < samplesPerFrame;)
                overSamplebuf[faudbufcntover++] = faudioBitBuf;

            switch (aud_active_sources) {
            case 0:
                // Beeper only
                for (int i = 0; i < samplesPerFrame; i++)
                    audioBuffer[i] = (overSamplebuf[i] / audioSampleDivider);
                break;
            case 1:
                // Beeper + Covox
                for (;faudbufcntCOVOX < samplesPerFrame;)
                    SamplebufCOVOX[faudbufcntCOVOX++] = fcovoxMix;

                for (int i = 0; i < samplesPerFrame; i++) {
                    int beeper = (overSamplebuf[i] / audioSampleDivider) + SamplebufCOVOX[i];
                    audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
                }

                break;
            case 2:
                // Beeper + AY
                if (faudbufcntAY < samplesPerFrame)
                    AySound::gen_sound(samplesPerFrame - faudbufcntAY , faudbufcntAY);

                for (int i = 0; i < samplesPerFrame; i++) {
                    int beeper = (overSamplebuf[i] / audioSampleDivider) + AySound::SamplebufAY[i];
                    audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
                }

                break;
            case 3:
                // Beeper + AY + Covox
                for (;faudbufcntCOVOX < samplesPerFrame;)
                    SamplebufCOVOX[faudbufcntCOVOX++] = fcovoxMix;

                if (faudbufcntAY < samplesPerFrame)
                    AySound::gen_sound(samplesPerFrame - faudbufcntAY , faudbufcntAY);

                for (int i = 0; i < samplesPerFrame; i++) {
                    int beeper = (overSamplebuf[i] / audioSampleDivider) + AySound::SamplebufAY[i] + SamplebufCOVOX[i];
                    audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
                }

                break;
            }

        }

        // task2elapsed = esp_timer_get_time() - task2start;

    }
}

//=======================================================================================
// Beeper audiobuffer generation (oversample)
//=======================================================================================
IRAM_ATTR void ESPectrum::BeeperGetSample() {
    uint32_t audbufpos = CPU::tstates / audioOverSampleDivider;
    for (;audbufcnt < audbufpos; audbufcnt++) {
        audioBitBuf += lastaudioBit;
        if(++audioBitbufCount == audioSampleDivider) {
            overSamplebuf[audbufcntover++] = audioBitBuf;
            audioBitBuf = 0;
            audioBitbufCount = 0;
        }
    }
}

//=======================================================================================
// AY audiobuffer generation (oversample)
//=======================================================================================
IRAM_ATTR void ESPectrum::AYGetSample() {
    uint32_t audbufpos = CPU::tstates / audioAYDivider;
    if (audbufpos > audbufcntAY) {
        AySound::gen_sound(audbufpos - audbufcntAY, audbufcntAY);
        audbufcntAY = audbufpos;
    }
}

//=======================================================================================
// COVOX audiobuffer generation
//=======================================================================================
IRAM_ATTR void ESPectrum::COVOXGetSample() {
    uint32_t audbufpos = CPU::tstates / audioCOVOXDivider;
    if (audbufpos > audbufcntCOVOX) {
        covoxMix = (covoxData[0] + covoxData[1] + covoxData[2] + covoxData[3]) >> 2;
        for (int i=audbufcntCOVOX;i<audbufpos;i++)
            SamplebufCOVOX[i] = covoxMix;
        audbufcntCOVOX = audbufpos;
    }
}

//=======================================================================================
// MAIN LOOP
//=======================================================================================

IRAM_ATTR void ESPectrum::loop() {

static uint8_t OSDbgcolor = 1;

for(;;) {

    ts_start = esp_timer_get_time();

    // Send audioBuffer to pwmaudio
    xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

    CPU::loop();

    // Copy audio buffer vars for finish buffer code
    faudbufcntover = audbufcntover;
    faudioBitBuf = audioBitBuf;
    faudioBitbufCount = audioBitbufCount;
    faudbufcntAY = audbufcntAY;
    faudbufcntCOVOX = audbufcntCOVOX;
    flastaudioBit = lastaudioBit;
    fcovoxMix = covoxMix;

    xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

    AudioIn::PrepareFrame();

    processKeyboard();

    // Update stats every 50 frames
    if (VIDEO::OSD && VIDEO::framecnt >= 50) {

        if (VIDEO::OSD & 0x04) {

            // printf("Vol. OSD out -> Framecnt: %d\n", VIDEO::framecnt);

            if (VIDEO::framecnt >= 100) {

                // Save selected volume if not in tape player mode
                if (!Config::EarBoost) {
                    Config::volume = aud_volume;
                    Config::save("volume");
                }

                VIDEO::OSD &= 0xfb;

                if (VIDEO::OSD == 0) {

                    if (Config::aspect_16_9)
                        VIDEO::Draw_OSD169 = Z80Ops::is2a3 ? VIDEO::MainScreen_2A3 : VIDEO::MainScreen;
                    else
                        VIDEO::Draw_OSD43 = Z80Ops::isPentagon ? VIDEO::BottomBorder_Pentagon :  VIDEO::BottomBorder;

                    VIDEO::brdnextframe = true;

                }

            }

        }

        if ((VIDEO::OSD & 0x04) == 0) {

            if (VIDEO::OSD == 1 && Tape::tapeStatus==TAPE_LOADING) {

                snprintf(OSD::stats_lin1, sizeof(OSD::stats_lin1), " %-12s %04d/%04d ", Tape::tapeFileName.substr(0 + ESPectrum::TapeNameScroller, 12).c_str(), Tape::tapeCurBlock + 1, Tape::tapeNumBlocks);

                float percent = (float)((Tape::tapebufByteCount + Tape::tapePlayOffset) * 100) / (float)Tape::tapeFileSize;
                snprintf(OSD::stats_lin2, sizeof(OSD::stats_lin2), " %05.2f%% %07d%s%07d ", percent, Tape::tapebufByteCount + Tape::tapePlayOffset, "/" , Tape::tapeFileSize);

                if ((++ESPectrum::TapeNameScroller + 12) > Tape::tapeFileName.length()) ESPectrum::TapeNameScroller = 0;

                OSD::drawStats();

            } else if (VIDEO::OSD == 2) {

                snprintf(OSD::stats_lin1, sizeof(OSD::stats_lin1), "CPU: %05d  IDL: %05d  ", /*(int)(task2elapsed)*/ (int)(ESPectrum::elapsed), (int)(ESPectrum::idle));
                snprintf(OSD::stats_lin2, sizeof(OSD::stats_lin2), "FPS:%6.2f  FND:%6.2f  ", VIDEO::framecnt / (ESPectrum::totalseconds / 1000000), VIDEO::framecnt / (ESPectrum::totalsecondsnodelay / 1000000));

                // snprintf(OSD::stats_lin2, sizeof(OSD::stats_lin2), "SMP: %05d  BAS: %05d  ", AudioIn::sample_index , (int)((AudioIn::Basepos + CPU::tstates) * AudioIn::Factor) % AudioIn::Bufsize);

                OSD::drawStats();

            }

            totalseconds = 0;
            totalsecondsnodelay = 0;
            VIDEO::framecnt = 0;

        }

        OSDbgcolor = ESP_delay;

    }

    // Flashing flag change
    if (!(VIDEO::flash_ctr++ & 0x0f)) VIDEO::flashing ^= 0x80;

    // Draw fdd led if CPU OSD active
    if (VIDEO::OSD == 2 && (VIDEO::framecnt & 1)) {
        if (ESPectrum::fdd.led)
            VIDEO::vga.fillRect(VIDEO::osdstartX + 137 , VIDEO::brdlin_osdstart + 2, 5, 4, zxColor( ESPectrum::fdd.led == 1 ? 4 : 2, 1));
        else
            VIDEO::vga.fillRect(VIDEO::osdstartX + 137 , VIDEO::brdlin_osdstart + 2, 5, 4, zxColor( OSDbgcolor, 0));
    }

    #ifdef ESPECTRUM_PSRAM
    #ifdef TIME_MACHINE
    // Time machine
    if (Config::psram_size && Config::TimeMachine) MemESP::Tm_DoTimeMachine();
    #endif
    #endif

    elapsed = esp_timer_get_time() - ts_start;
    idle = target[ESP_delay] - elapsed - ESPoffset;

    totalsecondsnodelay += elapsed;

    if (!ESP_delay) {
        totalseconds += elapsed;
        continue;
    }

    if(Config::videomode && !Config::EarBoost && !Config::MicBoost && AudioIn::Status != AUDIOIN_PLAY && ESP_delay == 1) {

        // if (sync_cnt++ == 0) {

        //     if (idle > 0)
        //         delayMicroseconds(idle);

        // } else {

        //     // Audio sync (once every 128 frames ~ 2,5 seconds)
        //     if (sync_cnt & 0x80) {
        //         ESPoffset = 128 - pwm_audio_rbstats();
        //         sync_cnt = 0;
        //     }

            // Wait for vertical sync
            for (;;)
                if (vsync) {
                    vsync = false;
                    break;
                }

            // printf("Vsync!\n");

        // }

    } else {

        if (idle > 0)
            delayMicroseconds(idle);
        // else
        //     printf("Elapsed: %d\n",(int)elapsed);

        // Audio sync
        if (++sync_cnt & 0x10) {
            ESPoffset = 128 - pwm_audio_rbstats();
            sync_cnt = 0;
        }

    }

    totalseconds += esp_timer_get_time() - ts_start;
    // printf("Totalsecond: %f\n",double(esp_timer_get_time() - ts_start));

}

}
