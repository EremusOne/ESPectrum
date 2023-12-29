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

#include <stdio.h>
#include <string>

#include "ESPectrum.h"
#include "Snapshot.h"
#include "Config.h"
#include "FileUtils.h"
#include "OSDMain.h"
#include "Ports.h"
#include "MemESP.h"
#include "CPU.h"
#include "Video.h"
#include "messages.h"
#include "AySound.h"
#include "Tape.h"
#include "Z80_JLS/z80.h"
#include "pwm_audio.h"
#include "fabgl.h"
#include "wd1793.h"

#ifndef ESP32_SDL2_WRAPPER
#include "ZXKeyb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "soc/timer_group_struct.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
// #include "bootloader_random.h"
#endif

using namespace std;

// #pragma GCC optimize("O3")

//=======================================================================================
// KEYBOARD
//=======================================================================================
fabgl::PS2Controller ESPectrum::PS2Controller;
bool ESPectrum::ps2kbd2 = false;

//=======================================================================================
// AUDIO
//=======================================================================================
uint8_t ESPectrum::audioBuffer[ESP_AUDIO_SAMPLES_PENTAGON] = { 0 };
uint8_t ESPectrum::overSamplebuf[ESP_AUDIO_OVERSAMPLES_PENTAGON] = { 0 };
signed char ESPectrum::aud_volume = ESP_DEFAULT_VOLUME;
uint32_t ESPectrum::audbufcnt = 0;
uint32_t ESPectrum::faudbufcnt = 0;
uint32_t ESPectrum::audbufcntAY = 0;
uint32_t ESPectrum::faudbufcntAY = 0;
int ESPectrum::lastaudioBit = 0;
int ESPectrum::faudioBit = 0;
int ESPectrum::samplesPerFrame;
int ESPectrum::overSamplesPerFrame;
bool ESPectrum::AY_emu = false;
int ESPectrum::Audio_freq;
int ESPectrum::TapeNameScroller = 0;
// bool ESPectrum::Audio_restart = false;

QueueHandle_t audioTaskQueue;
TaskHandle_t ESPectrum::audioTaskHandle;
uint8_t *param;

//=======================================================================================
// BETADISK
//=======================================================================================

bool ESPectrum::trdos = false;
WD1793 ESPectrum::Betadisk;

//=======================================================================================
// ARDUINO FUNCTIONS
//=======================================================================================

#ifndef ESP32_SDL2_WRAPPER
#define NOP() asm volatile ("nop")
#else
#define NOP() {for(int i=0;i<1000;i++){}}
#endif

// IRAM_ATTR int64_t micros()
// {
//     return esp_timer_get_time();    
// }

IRAM_ATTR unsigned long millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

// inline void delay(uint32_t ms)
// {
//     vTaskDelay(ms / portTICK_PERIOD_MS);
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
// TIMING
//=======================================================================================

static double totalseconds = 0;
static double totalsecondsnodelay = 0;

int64_t ESPectrum::target;

//=======================================================================================
// LOGGING / TESTING
//=======================================================================================

int ESPectrum::ESPoffset = 0;

void showMemInfo(const char* caption = "ZX-ESPectrum-IDF") {

#ifndef ESP32_SDL2_WRAPPER

multi_heap_info_t info;

heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task
printf("=========================================================================\n");
printf(" %s - Mem info:\n",caption);
printf("-------------------------------------------------------------------------\n");
printf("Total currently free in all non-continues blocks: %d\n", info.total_free_bytes);
printf("Minimum free ever: %d\n", info.minimum_free_bytes);
printf("Largest continues block to allocate big array: %d\n", info.largest_free_block);
printf("Heap caps get free size (MALLOC_CAP_8BIT): %d\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
printf("Heap caps get free size (MALLOC_CAP_32BIT): %d\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
printf("Heap caps get free size (MALLOC_CAP_INTERNAL): %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
printf("=========================================================================\n\n");

// heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);

// printf("=========================================================================\n");
// heap_caps_print_heap_info(MALLOC_CAP_8BIT);            

// printf("=========================================================================\n");
// heap_caps_print_heap_info(MALLOC_CAP_32BIT);                        

// printf("=========================================================================\n");
// heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

// printf("=========================================================================\n");
// heap_caps_print_heap_info(MALLOC_CAP_DMA);            

// printf("=========================================================================\n");
// heap_caps_print_heap_info(MALLOC_CAP_EXEC);            

// printf("=========================================================================\n");
// heap_caps_print_heap_info(MALLOC_CAP_IRAM_8BIT);            

// printf("=========================================================================\n");
// heap_caps_dump_all();

// printf("=========================================================================\n");

// UBaseType_t wm;
// wm = uxTaskGetStackHighWaterMark(audioTaskHandle);
// printf("Audio Task Stack HWM: %u\n", wm);
// // wm = uxTaskGetStackHighWaterMark(loopTaskHandle);
// // printf("Loop Task Stack HWM: %u\n", wm);
// wm = uxTaskGetStackHighWaterMark(VIDEO::videoTaskHandle);
// printf("Video Task Stack HWM: %u\n", wm);

#endif

}

//=======================================================================================
// BOOT KEYBOARD
//=======================================================================================
void ESPectrum::bootKeyboard() {

    auto Kbd = PS2Controller.keyboard();
    fabgl::VirtualKeyItem NextKey;
    int i = 0;
    string s = "00";

    // printf("Boot kbd!\n");

    for (; i < 200; i++) {

        if (ZXKeyb::Exists) {

            // Process physical keyboard
            ZXKeyb::process();
            
            // Detect and process physical kbd menu key combinations
            if (!bitRead(ZXKeyb::ZXcols[3], 0)) { // 1
                s[0]='1';
            } else
            if (!bitRead(ZXKeyb::ZXcols[3], 1)) { // 2
                s[0]='2';
            } else
            if (!bitRead(ZXKeyb::ZXcols[3], 2)) { // 3
                s[0]='3';
            }

            if (!bitRead(ZXKeyb::ZXcols[2], 0)) { // Q
                s[1]='Q';
            } else 
            if (!bitRead(ZXKeyb::ZXcols[2], 1)) { // W
                s[1]='W';
            }

        }

        while (Kbd->virtualKeyAvailable()) {

            bool r = Kbd->getNextVirtualKey(&NextKey);

            if (r && NextKey.down) {

                // Check keyboard status
                switch (NextKey.vk) {
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
                }

            }

        }

        if (s.find('0') == std::string::npos) break;

        delayMicroseconds(1000);

    }

    // printf("Boot kbd end!\n");

    if (i < 200) {
        Config::videomode = (s[0] == '1') ? 0 : (s[0] == '2') ? 1 : 2;
        Config::aspect_16_9 = (s[1] == 'Q') ? false : true;
        Config::ram_file="none";
        Config::save();
        // printf("%s\n", s.c_str());
    }

}

//=======================================================================================
// SETUP
//=======================================================================================
// TaskHandle_t ESPectrum::loopTaskHandle;

// uint32_t ESPectrum::sessid;

void ESPectrum::setup() 
{

    // //=======================================================================================
    // // GENERATE SESSION ID
    // //=======================================================================================
    // bootloader_random_enable();
    // sessid = esp_random();
    // printf("SESSION ID: %08X\n",(unsigned int)sessid);
    // bootloader_random_disable();    

    #ifndef ESP32_SDL2_WRAPPER

    if (Config::slog_on) {

        printf("------------------------------------\n");
        printf("| ESPectrum: booting               |\n");        
        printf("------------------------------------\n");    

        showMemInfo();

    }

    #endif

    //=======================================================================================
    // PHYSICAL KEYBOARD (SINCLAIR 8 + 5 MEMBRANE KEYBOARD)
    //=======================================================================================

    ZXKeyb::setup();
   
    //=======================================================================================
    // LOAD CONFIG
    //=======================================================================================
    
    Config::load();

    //=======================================================================================
    // INIT PS/2 KEYBOARD
    //=======================================================================================

    ESPectrum::ps2kbd2 = (Config::ps2_dev2 != 0);
    
    if (ZXKeyb::Exists) {
        PS2Controller.begin(ps2kbd2 ? PS2Preset::KeyboardPort0 : PS2Preset::zxKeyb, KbdMode::CreateVirtualKeysQueue);
    } else {
        PS2Controller.begin(ps2kbd2 ? PS2Preset::KeyboardPort0_KeybJoystickPort1 : PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
    }

    ps2kbd2 &= !ZXKeyb::Exists;

    // Set Scroll Lock Led as current CursorAsJoy value
    PS2Controller.keyboard()->setLEDs(false, false, Config::CursorAsJoy);
    if(ps2kbd2)
        PS2Controller.keybjoystick()->setLEDs(false, false, Config::CursorAsJoy);

    #ifndef ESP32_SDL2_WRAPPER

    if (Config::slog_on) {
        showMemInfo("Keyboard started");
    }

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
    
    #endif

    //=======================================================================================
    // BOOTKEYS: Read keyboard for 200 ms. checking boot keys
    //=======================================================================================

    // printf("Waiting boot keys\n");
    bootKeyboard();
    // printf("End Waiting boot keys\n");

    //=======================================================================================
    // MEMORY SETUP
    //=======================================================================================

    MemESP::ram5 = staticMemPage0;
    MemESP::ram0 = staticMemPage1;
    MemESP::ram2 = staticMemPage2;

    MemESP::ram1 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT);
    MemESP::ram3 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT);
    MemESP::ram4 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT);
    MemESP::ram6 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT);
    MemESP::ram7 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT);

    // MemESP::ram1 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    // MemESP::ram3 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    // MemESP::ram4 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    // MemESP::ram6 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    // MemESP::ram7 = (unsigned char *) heap_caps_malloc(0x4000, MALLOC_CAP_8BIT);

    if (Config::slog_on) {
        if (MemESP::ram1 == NULL) printf("ERROR! Unable to allocate ram1\n");        
        if (MemESP::ram3 == NULL) printf("ERROR! Unable to allocate ram3\n");        
        if (MemESP::ram4 == NULL) printf("ERROR! Unable to allocate ram4\n");        
        if (MemESP::ram6 == NULL) printf("ERROR! Unable to allocate ram6\n");
        if (MemESP::ram7 == NULL) printf("ERROR! Unable to allocate ram7\n");
    }

    MemESP::ram[0] = MemESP::ram0; MemESP::ram[1] = MemESP::ram1;
    MemESP::ram[2] = MemESP::ram2; MemESP::ram[3] = MemESP::ram3;
    MemESP::ram[4] = MemESP::ram4; MemESP::ram[5] = MemESP::ram5;
    MemESP::ram[6] = MemESP::ram6; MemESP::ram[7] = MemESP::ram7;

    MemESP::romInUse = 0;
    MemESP::bankLatch = 0;
    MemESP::videoLatch = 0;
    MemESP::romLatch = 0;

    MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[1] = (unsigned char *)MemESP::ram[5];
    MemESP::ramCurrent[2] = (unsigned char *)MemESP::ram[2];
    MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = Config::getArch() == "Pentagon" ? false : true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = false;

    if (Config::getArch() == "48K") MemESP::pagingLock = 1; else MemESP::pagingLock = 0;

    if (Config::slog_on) showMemInfo("RAM Initialized");

    //=======================================================================================
    // VIDEO
    //=======================================================================================

    VIDEO::Init();
    VIDEO::Reset();
    
    if (Config::slog_on) showMemInfo("VGA started");

    //=======================================================================================
    // INIT FILESYSTEM
    //=======================================================================================
    
    FileUtils::initFileSystem();

    //=======================================================================================
    // AUDIO
    //=======================================================================================

    // Set samples per frame and AY_emu flag depending on arch
    if (Config::getArch() == "48K") {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_48;
        samplesPerFrame=ESP_AUDIO_SAMPLES_48; 
        AY_emu = Config::AY48;
        Audio_freq = ESP_AUDIO_FREQ_48;
    } else if (Config::getArch() == "128K") {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_128;
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        AY_emu = true;        
        Audio_freq = ESP_AUDIO_FREQ_128;
    } else if (Config::getArch() == "Pentagon") {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_PENTAGON;
        samplesPerFrame=ESP_AUDIO_SAMPLES_PENTAGON;
        AY_emu = true;        
        Audio_freq = ESP_AUDIO_FREQ_PENTAGON;
    }

    ESPoffset = 0;

    // Create Audio task
    audioTaskQueue = xQueueCreate(1, sizeof(uint8_t *));
    // Latest parameter = Core. In ESPIF, main task runs on core 0 by default. In Arduino, loop() runs on core 1.
    xTaskCreatePinnedToCore(&ESPectrum::audioTask, "audioTask", /* 1024 */ 1536, NULL, configMAX_PRIORITIES - 1, &audioTaskHandle, 1);

    // AY Sound
    AySound::init();
    AySound::set_sound_format(Audio_freq,1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    // Init tape
    Tape::Init();
    Tape::tapeFileName = "none";
    Tape::tapeStatus = TAPE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    // Init CPU
    Z80::create();

    // Set Ports starting values
    for (int i = 0; i < 128; i++) Ports::port[i] = 0xBF;
    if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0; // Kempston
    if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff; // Fuller

    // Read joystick default definition
    for (int n = 0; n < 24; n++)
        ESPectrum::JoyVKTranslation[n] = (fabgl::VirtualKey) Config::joydef[n];

    // Init disk controller
    Betadisk.Init();

    // Load romset
    Config::requestMachine(Config::getArch(), Config::getRomSet());

    // Reset cpu
    CPU::reset();

    // Load snapshot if present in Config::ram_file
    if (Config::ram_file != NO_RAM_FILE) {

        FileUtils::SNA_Path = Config::SNA_Path;
        FileUtils::fileTypes[DISK_SNAFILE].begin_row = Config::SNA_begin_row;
        FileUtils::fileTypes[DISK_SNAFILE].focus = Config::SNA_focus;
        FileUtils::fileTypes[DISK_SNAFILE].fdMode = Config::SNA_fdMode;
        FileUtils::fileTypes[DISK_SNAFILE].fileSearch = Config::SNA_fileSearch;

        FileUtils::TAP_Path = Config::TAP_Path;
        FileUtils::fileTypes[DISK_TAPFILE].begin_row = Config::TAP_begin_row;
        FileUtils::fileTypes[DISK_TAPFILE].focus = Config::TAP_focus;
        FileUtils::fileTypes[DISK_TAPFILE].fdMode = Config::TAP_fdMode;
        FileUtils::fileTypes[DISK_TAPFILE].fileSearch = Config::TAP_fileSearch;

        FileUtils::DSK_Path = Config::DSK_Path;
        FileUtils::fileTypes[DISK_DSKFILE].begin_row = Config::DSK_begin_row;
        FileUtils::fileTypes[DISK_DSKFILE].focus = Config::DSK_focus;
        FileUtils::fileTypes[DISK_DSKFILE].fdMode = Config::DSK_fdMode;
        FileUtils::fileTypes[DISK_DSKFILE].fileSearch = Config::DSK_fileSearch;

        LoadSnapshot(Config::ram_file,"");

        Config::last_ram_file = Config::ram_file;
        #ifndef SNAPSHOT_LOAD_LAST
        Config::ram_file = NO_RAM_FILE;
        Config::save("ram");
        #endif

    }

    if (Config::slog_on) showMemInfo("ZX-ESPectrum-IDF setup finished.");

    // Create loop function as task: it doesn't seem better than calling from main.cpp and increases RAM consumption (4096 bytes for stack).
    // xTaskCreatePinnedToCore(&ESPectrum::loop, "loopTask", 4096, NULL, 1, &loopTaskHandle, 0);

}

//=======================================================================================
// RESET
//=======================================================================================
void ESPectrum::reset()
{

    // Ports
    for (int i = 0; i < 128; i++) Ports::port[i] = 0xBF;
    if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0; // Kempston
    if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff; // Fuller

    // Read joystick default definition
    for (int n = 0; n < 24; n++)
        ESPectrum::JoyVKTranslation[n] = (fabgl::VirtualKey) Config::joydef[n];

    // Memory
    MemESP::bankLatch = 0;
    MemESP::videoLatch = 0;
    MemESP::romLatch = 0;

    string arch = Config::getArch();

    if (arch == "48K") MemESP::pagingLock = 1; else MemESP::pagingLock = 0;

    MemESP::romInUse = 0;

    MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[1] = (unsigned char *)MemESP::ram[5];
    MemESP::ramCurrent[2] = (unsigned char *)MemESP::ram[2];
    MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = arch == "Pentagon" ? false : true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = false;

    VIDEO::Reset();

    // Reinit disk controller
    // Betadisk.ShutDown();
    // Betadisk.Init();
    Betadisk.EnterIdle();

    Tape::tapeFileName = "none";
    if (Tape::tape != NULL) {
        fclose(Tape::tape);
        Tape::tape = NULL;
    }
    Tape::tapeStatus = TAPE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    // Empty audio buffers
    for (int i=0;i<ESP_AUDIO_OVERSAMPLES_PENTAGON;i++) overSamplebuf[i]=0;
    for (int i=0;i<ESP_AUDIO_SAMPLES_PENTAGON;i++) {
        audioBuffer[i]=0;
        AySound::SamplebufAY[i]=0;
    }
    lastaudioBit=0;

    // Set samples per frame and AY_emu flag depending on arch
    int prevAudio_freq = Audio_freq;
    if (arch == "48K") {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_48;
        samplesPerFrame=ESP_AUDIO_SAMPLES_48; 
        AY_emu = Config::AY48;
        Audio_freq = ESP_AUDIO_FREQ_48;
    } else if (arch == "128K") {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_128;
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        AY_emu = true;        
        Audio_freq = ESP_AUDIO_FREQ_128;
    } else if (arch == "Pentagon") {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_PENTAGON;
        samplesPerFrame=ESP_AUDIO_SAMPLES_PENTAGON;
        AY_emu = true;        
        Audio_freq = ESP_AUDIO_FREQ_PENTAGON;
    }

    ESPoffset = 0;

    // Readjust output pwmaudio frequency if needed
    if (prevAudio_freq != Audio_freq) {
        
        // printf("Resetting pwmaudio to freq: %d\n",Audio_freq);

        esp_err_t res;
        res = pwm_audio_set_sample_rate(Audio_freq);
        if (res != ESP_OK) {
            printf("Can't set sample rate\n");
        }

        // pwm_audio_stop();
        // delay(100); // Maybe this fix random sound lost ?
        // pwm_audio_set_param(Audio_freq,LEDC_TIMER_8_BIT,1);
        // pwm_audio_start();
        // pwm_audio_set_volume(aud_volume);

    }
    
    // Reset AY emulation
    AySound::init();
    AySound::set_sound_format(Audio_freq,1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    CPU::reset();

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
        }

    }

}

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

IRAM_ATTR void ESPectrum::processKeyboard() {
// void ESPectrum::processKeyboard() {    

    static uint8_t PS2cols[8] = { 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf };    
    static int zxDelay = 0;
    auto Kbd = PS2Controller.keyboard();
    fabgl::VirtualKeyItem NextKey;
    fabgl::VirtualKey KeytoESP;
    bool Kdown;
    bool r = false;
    bool j[10] = { true, true, true, true, true, true, true, true, true, true };
    // bool j1 =  true;
    // bool j2 = true;
    // bool j3 = true;
    // bool j4 = true;
    // bool j5 = true;
    // bool j6 = true;
    // bool j7 = true;
    // bool j8 = true;
    // bool j9 = true;
    // bool j0 = true;
    bool jShift = true;

    readKbdJoy();

    while (Kbd->virtualKeyAvailable()) {

        r = readKbd(&NextKey);

        if (r) {

            KeytoESP = NextKey.vk;
            Kdown = NextKey.down;
          
            if (KeytoESP >= fabgl::VK_JOY1LEFT && KeytoESP <= fabgl::VK_JOY2Z) {
                // printf("KeytoESP: %d\n",KeytoESP);
                ESPectrum::PS2Controller.keyboard()->injectVirtualKey(JoyVKTranslation[KeytoESP - 248], Kdown, false);
                continue;
            }

            if ((Kdown) && ((KeytoESP >= fabgl::VK_F1 && KeytoESP <= fabgl::VK_F12) || KeytoESP == fabgl::VK_PAUSE)) {

                OSD::do_OSD(KeytoESP, Kbd->isVKDown(fabgl::VK_LCTRL) || Kbd->isVKDown(fabgl::VK_RCTRL));

                Kbd->emptyVirtualKeyQueue();
                
                // Set all zx keys as not pressed
                for (uint8_t i = 0; i < 8; i++) ZXKeyb::ZXcols[i] = 0xbf;
                zxDelay = 15;
                
                totalseconds = 0;
                totalsecondsnodelay = 0;
                VIDEO::framecnt = 0;

                return;

            }

            if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0;
            if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff;

            if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON) {

                for (int i = fabgl::VK_KEMPSTON_RIGHT; i <= fabgl::VK_KEMPSTON_ALTFIRE; i++)
                    if (Kbd->isVKDown((fabgl::VirtualKey) i))
                        bitWrite(Ports::port[0x1f], i - fabgl::VK_KEMPSTON_RIGHT, 1);

            }

            if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER) {

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

                if (Kbd->isVKDown(fabgl::VK_RALT)) {
                    bitWrite(Ports::port[0x1f], 4, 1);
                }

                if (Kbd->isVKDown(fabgl::VK_SLASH) || Kbd->isVKDown(fabgl::VK_QUESTION) || Kbd->isVKDown(fabgl::VK_RGUI) || Kbd->isVKDown(fabgl::VK_APPLICATION) ) {
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

                if (Kbd->isVKDown(fabgl::VK_RALT)) {
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

                if (Kbd->isVKDown(fabgl::VK_RALT)) {
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

                if (Kbd->isVKDown(fabgl::VK_RALT)) {
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

                if (Kbd->isVKDown(fabgl::VK_RALT)) {
                    jShift = true;
                    j[5] = false;
                };

            }

            // Check keyboard status and map it to Spectrum Ports
            
            bitWrite(PS2cols[0], 0, (jShift) & (!Kbd->isVKDown(fabgl::VK_BACKSPACE))); // CAPS SHIFT
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

            bitWrite(PS2cols[3], 0, (!Kbd->isVKDown(fabgl::VK_1)) & (!Kbd->isVKDown(fabgl::VK_EXCLAIM)) & (j[1]));
            bitWrite(PS2cols[3], 1, (!Kbd->isVKDown(fabgl::VK_2)) & (!Kbd->isVKDown(fabgl::VK_AT)) & (j[2]));
            bitWrite(PS2cols[3], 2, (!Kbd->isVKDown(fabgl::VK_3)) & (!Kbd->isVKDown(fabgl::VK_HASH)) & (j[3]));
            bitWrite(PS2cols[3], 3, (!Kbd->isVKDown(fabgl::VK_4)) & (!Kbd->isVKDown(fabgl::VK_DOLLAR)) & (j[4]));
            bitWrite(PS2cols[3], 4, (!Kbd->isVKDown(fabgl::VK_5)) & (!Kbd->isVKDown(fabgl::VK_PERCENT)) & (j[5]));

            bitWrite(PS2cols[4], 0, (!Kbd->isVKDown(fabgl::VK_0)) & (!Kbd->isVKDown(fabgl::VK_RIGHTPAREN)) & (!Kbd->isVKDown(fabgl::VK_BACKSPACE)) & (j[0]));
            bitWrite(PS2cols[4], 1, !Kbd->isVKDown(fabgl::VK_9) & (!Kbd->isVKDown(fabgl::VK_LEFTPAREN)) & (j[9]));
            bitWrite(PS2cols[4], 2, (!Kbd->isVKDown(fabgl::VK_8)) & (!Kbd->isVKDown(fabgl::VK_ASTERISK)) & (j[8]));
            bitWrite(PS2cols[4], 3, (!Kbd->isVKDown(fabgl::VK_7)) & (!Kbd->isVKDown(fabgl::VK_AMPERSAND)) & (j[7]));
            bitWrite(PS2cols[4], 4, (!Kbd->isVKDown(fabgl::VK_6)) & (!Kbd->isVKDown(fabgl::VK_CARET)) & (j[6]));

            bitWrite(PS2cols[5], 0, (!Kbd->isVKDown(fabgl::VK_P)) & (!Kbd->isVKDown(fabgl::VK_p)));
            bitWrite(PS2cols[5], 1, (!Kbd->isVKDown(fabgl::VK_O)) & (!Kbd->isVKDown(fabgl::VK_o)));
            bitWrite(PS2cols[5], 2, (!Kbd->isVKDown(fabgl::VK_I)) & (!Kbd->isVKDown(fabgl::VK_i)));
            bitWrite(PS2cols[5], 3, (!Kbd->isVKDown(fabgl::VK_U)) & (!Kbd->isVKDown(fabgl::VK_u)));
            bitWrite(PS2cols[5], 4, (!Kbd->isVKDown(fabgl::VK_Y)) & (!Kbd->isVKDown(fabgl::VK_y)));

            bitWrite(PS2cols[6], 0, !Kbd->isVKDown(fabgl::VK_RETURN));
            bitWrite(PS2cols[6], 1, (!Kbd->isVKDown(fabgl::VK_L)) & (!Kbd->isVKDown(fabgl::VK_l)));
            bitWrite(PS2cols[6], 2, (!Kbd->isVKDown(fabgl::VK_K)) & (!Kbd->isVKDown(fabgl::VK_k)));
            bitWrite(PS2cols[6], 3, (!Kbd->isVKDown(fabgl::VK_J)) & (!Kbd->isVKDown(fabgl::VK_j)));
            bitWrite(PS2cols[6], 4, (!Kbd->isVKDown(fabgl::VK_H)) & (!Kbd->isVKDown(fabgl::VK_h)));

            bitWrite(PS2cols[7], 0, !Kbd->isVKDown(fabgl::VK_SPACE));
            bitWrite(PS2cols[7], 1, (!Kbd->isVKDown(fabgl::VK_LCTRL)) // SYMBOL SHIFT
                                &   (!Kbd->isVKDown(fabgl::VK_RCTRL))
                                &   (!Kbd->isVKDown(fabgl::VK_COMMA)) // Comma
                                &   (!Kbd->isVKDown(fabgl::VK_PERIOD)) // Period
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

    if (ZXKeyb::Exists) { // START - ZXKeyb Exists

        if (zxDelay > 0)
            zxDelay--;
        else
            // Process physical keyboard
            ZXKeyb::process();

        // Detect and process physical kbd menu key combinations
        // CS+SS+<1..0> -> F1..F10 Keys, CS+SS+Q -> F11, CS+SS+W -> F12, CS+SS+S -> Capture screen
        if ((!bitRead(ZXKeyb::ZXcols[0],0)) && (!bitRead(ZXKeyb::ZXcols[7],1))) {

            zxDelay = 15;

            if (!bitRead(ZXKeyb::ZXcols[3],0)) {
                OSD::do_OSD(fabgl::VK_F1,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[3],1)) {
                OSD::do_OSD(fabgl::VK_F2,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[3],2)) {
                OSD::do_OSD(fabgl::VK_F3,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[3],3)) {
                OSD::do_OSD(fabgl::VK_F4,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[3],4)) {
                OSD::do_OSD(fabgl::VK_F5,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[4],4)) {
                OSD::do_OSD(fabgl::VK_F6,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[4],3)) {
                OSD::do_OSD(fabgl::VK_F7,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[4],2)) {
                OSD::do_OSD(fabgl::VK_F8,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[4],1)) {
                OSD::do_OSD(fabgl::VK_F9,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[4],0)) {
                OSD::do_OSD(fabgl::VK_F10,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[2],0)) {
                OSD::do_OSD(fabgl::VK_F11,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[2],1)) {
                OSD::do_OSD(fabgl::VK_F12,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[5],0)) { // P -> Pause
                OSD::do_OSD(fabgl::VK_PAUSE,0);
            } else
            if (!bitRead(ZXKeyb::ZXcols[5],2)) { // I -> Info
                OSD::do_OSD(fabgl::VK_F1,true);
            } else
            if (!bitRead(ZXKeyb::ZXcols[1],1)) { // S -> Screen capture
                CaptureToBmp();
            } else
            if (!bitRead(ZXKeyb::ZXcols[0],1)) { // Z -> CenterH
                if (Config::CenterH > -16) Config::CenterH--;
                Config::save("CenterH");
                OSD::osdCenteredMsg("Horiz. center: " + to_string(Config::CenterH), LEVEL_INFO, 375);
            } else
            if (!bitRead(ZXKeyb::ZXcols[0],2)) { // X -> CenterH
                if (Config::CenterH < 16) Config::CenterH++;
                Config::save("CenterH");
                OSD::osdCenteredMsg("Horiz. center: " + to_string(Config::CenterH), LEVEL_INFO, 375);
            } else
            if (!bitRead(ZXKeyb::ZXcols[0],3)) { // C -> CenterV
                if (Config::CenterV > -16) Config::CenterV--;
                Config::save("CenterV");
                OSD::osdCenteredMsg("Vert. center: " + to_string(Config::CenterV), LEVEL_INFO, 375);
            } else
            if (!bitRead(ZXKeyb::ZXcols[0],4)) { // V -> CenterV
                if (Config::CenterV < 16) Config::CenterV++;
                Config::save("CenterV");
                OSD::osdCenteredMsg("Vert. center: " + to_string(Config::CenterV), LEVEL_INFO, 375);
            } else
                zxDelay = 0;

            if (zxDelay) {
                // Set all keys as not pressed
                for (uint8_t i = 0; i < 8; i++) ZXKeyb::ZXcols[i] = 0xbf;
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

}

// static int bmax = 0;

// static int sndalive = 0;

//=======================================================================================
// AUDIO
//=======================================================================================
IRAM_ATTR void ESPectrum::audioTask(void *unused) {

    size_t written;

    // esp_err_t err;

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
    pac.ringbuf_len        = /* 1024 * 8;*/ /*2560;*/ /* 2880; */ 1024 * 4;

    pwm_audio_init(&pac);
    pwm_audio_set_param(Audio_freq,LEDC_TIMER_8_BIT,1);
    pwm_audio_start();
    pwm_audio_set_volume(aud_volume);

    for (;;) {

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        /* err = */ pwm_audio_write(ESPectrum::audioBuffer, samplesPerFrame, &written, 5 / portTICK_PERIOD_MS);

        // if (err == ESP_FAIL) printf("Audio fail!\n");

        // sndalive++;

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        // Finish fill of oversampled audio buffers
        if (faudbufcnt) {
            if (faudbufcnt < overSamplesPerFrame)
                for (int i=faudbufcnt; i < overSamplesPerFrame;i++) overSamplebuf[i] = faudioBit;
        }
        
        // Downsample beeper (median) and mix AY channels to output buffer
        int beeper;
        
        if (Z80Ops::is128) {

            if (faudbufcntAY < ESP_AUDIO_SAMPLES_128)
                AySound::gen_sound(ESP_AUDIO_SAMPLES_128 - faudbufcntAY , faudbufcntAY);

            if (faudbufcnt) {
                int n = 0;
                for (int i=0;i<ESP_AUDIO_OVERSAMPLES_128; i += 6) {
                    // Downsample (median)
                    beeper  =  overSamplebuf[i];
                    beeper +=  overSamplebuf[i+1];
                    beeper +=  overSamplebuf[i+2];
                    beeper +=  overSamplebuf[i+3];
                    beeper +=  overSamplebuf[i+4];
                    beeper +=  overSamplebuf[i+5];

                    beeper =  (beeper / 6) + AySound::SamplebufAY[n];
                    // if (bmax < SamplebufAY[n]) bmax = SamplebufAY[n];
                    audioBuffer[n++] = beeper > 255 ? 255 : beeper; // Clamp
                }
            } else {
                for (int i = 0; i < ESP_AUDIO_SAMPLES_128; i++) {
                    beeper = faudioBit + AySound::SamplebufAY[i];
                    audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
                }
            }

        } else {

            if (AY_emu) {
                if (faudbufcntAY < samplesPerFrame)
                    AySound::gen_sound(samplesPerFrame - faudbufcntAY , faudbufcntAY);
            }

            if (faudbufcnt) {
                int n = 0;
                for (int i=0;i < overSamplesPerFrame; i += 7) {
                    // Downsample (median)
                    beeper  =  overSamplebuf[i];
                    beeper +=  overSamplebuf[i+1];
                    beeper +=  overSamplebuf[i+2];
                    beeper +=  overSamplebuf[i+3];
                    beeper +=  overSamplebuf[i+4];
                    beeper +=  overSamplebuf[i+5];
                    beeper +=  overSamplebuf[i+6];

                    beeper = AY_emu ? (beeper / 7) + AySound::SamplebufAY[n] : beeper / 7;
                    // if (bmax < SamplebufAY[n]) bmax = SamplebufAY[n];
                    audioBuffer[n++] = beeper > 255 ? 255 : beeper; // Clamp

                }
            } else {
                for (int i = 0; i < samplesPerFrame; i++) {
                    beeper = AY_emu ? faudioBit + AySound::SamplebufAY[i] : faudioBit;
                    audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
                }
            }

        }
    }
}

IRAM_ATTR void ESPectrum::BeeperGetSample() {
    // Beeper audiobuffer generation (oversample)
    uint32_t audbufpos = Z80Ops::is128 ? CPU::tstates / 19 : CPU::tstates >> 4;
    for (;audbufcnt < audbufpos; audbufcnt++) overSamplebuf[audbufcnt] = lastaudioBit;
}

IRAM_ATTR void ESPectrum::AYGetSample() {
    // AY audiobuffer generation (oversample)
    uint32_t audbufpos = CPU::tstates / (Z80Ops::is128 ? 114 : 112);
    if (audbufpos > audbufcntAY) {
        AySound::gen_sound(audbufpos - audbufcntAY, audbufcntAY);
        audbufcntAY = audbufpos;
    }
}

//=======================================================================================
// MAIN LOOP
//=======================================================================================

int ESPectrum::sync_cnt = 0;
volatile bool ESPectrum::vsync = false;

IRAM_ATTR void ESPectrum::loop() {    

int64_t ts_start, elapsed;
int64_t idle;

// int ESPmedian = 0;

for(;;) {

    ts_start = esp_timer_get_time();

    // Send audioBuffer to pwmaudio
    xQueueSend(audioTaskQueue, &param, portMAX_DELAY);
    audbufcnt = 0;
    audbufcntAY = 0;    

    CPU::loop();

    // Process audio buffer
    faudbufcnt = audbufcnt;
    faudioBit = lastaudioBit;
    faudbufcntAY = audbufcntAY;
    xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

    processKeyboard();

    // Update stats every 50 frames
    if (VIDEO::framecnt == 1 && VIDEO::OSD) OSD::drawStats();

    // Flashing flag change
    if (!(VIDEO::flash_ctr++ & 0x0f)) VIDEO::flashing ^= 0x80;

    // OSD calcs
    if (VIDEO::framecnt) {
        
        totalsecondsnodelay += esp_timer_get_time() - ts_start;
        
        if (totalseconds >= 1000000) {

            if (elapsed < 100000) {
        
                // printf("Tstates: %u, RegPC: %u\n",CPU::tstates,Z80::getRegPC());

                #ifdef LOG_DEBUG_TIMING
                printf("===========================================================================\n");
                printf("[CPU] elapsed: %u; idle: %d\n", elapsed, idle);
                printf("[Audio] Volume: %d\n", aud_volume);
                printf("[Framecnt] %u; [Seconds] %f; [FPS] %f; [FPS (no delay)] %f\n", CPU::framecnt, totalseconds / 1000000, CPU::framecnt / (totalseconds / 1000000), CPU::framecnt / (totalsecondsnodelay / 1000000));
                // printf("[ESPoffset] %d\n", ESPoffset);
                showMemInfo();
                #endif

                #ifdef TESTING_CODE

                // printf("[Framecnt] %u; [Seconds] %f; [FPS] %f; [FPS (no delay)] %f\n", CPU::framecnt, totalseconds / 1000000, CPU::framecnt / (totalseconds / 1000000), CPU::framecnt / (totalsecondsnodelay / 1000000));

                // showMemInfo();
                
                snprintf(linea1, sizeof(linea1), "CPU: %05d / IDL: %05d ", (int)(elapsed), (int)(idle));
                // snprintf(linea1, sizeof(linea1), "CPU: %05d / TGT: %05d ", (int)elapsed, (int)target);
                // snprintf(linea1, sizeof(linea1), "CPU: %05d / BMX: %05d ", (int)(elapsed), bmax);
                // snprintf(linea1, sizeof(linea1), "CPU: %05d / OFF: %05d ", (int)(elapsed), (int)(ESPmedian/50));

                snprintf(linea2, sizeof(linea2), "FPS:%6.2f / FND:%6.2f ", CPU::framecnt / (totalseconds / 1000000), CPU::framecnt / (totalsecondsnodelay / 1000000));

                #else

                if (Tape::tapeStatus==TAPE_LOADING) {

                    snprintf(OSD::stats_lin1, sizeof(OSD::stats_lin1), " %-12s %04d/%04d ", Tape::tapeFileName.substr(0 + TapeNameScroller, 12).c_str(), Tape::tapeCurBlock + 1, Tape::tapeNumBlocks);

                    float percent = (float)((Tape::tapebufByteCount + Tape::tapePlayOffset) * 100) / (float)Tape::tapeFileSize;
                    snprintf(OSD::stats_lin2, sizeof(OSD::stats_lin2), " %05.2f%% %07d%s%07d ", percent, Tape::tapebufByteCount + Tape::tapePlayOffset, "/" , Tape::tapeFileSize);

                    if ((++TapeNameScroller + 12) > Tape::tapeFileName.length()) TapeNameScroller = 0;

                } else {

                    snprintf(OSD::stats_lin1, sizeof(OSD::stats_lin1), "CPU: %05d / IDL: %05d ", (int)(elapsed), (int)(idle));
                    
                    // Audio tests
                    // snprintf(OSD::stats_lin1, sizeof(OSD::stats_lin1), "CPU: %05d / SND: %05d ", (int)(elapsed), (int)pwm_audio_rbstats());
                    // snprintf(OSD::stats_lin1, sizeof(OSD::stats_lin1), "CPU: %05d / SND: %05d ", (int)(elapsed), (int)pwm_audio_pwmcount());                    
                    
                    snprintf(OSD::stats_lin2, sizeof(OSD::stats_lin2), "FPS:%6.2f / FND:%6.2f ", VIDEO::framecnt / (totalseconds / 1000000), VIDEO::framecnt / (totalsecondsnodelay / 1000000));

                }

                #endif
            }

            totalseconds = 0;
            totalsecondsnodelay = 0;
            VIDEO::framecnt = 0;

            // ESPmedian = 0;

        }
        
        elapsed = esp_timer_get_time() - ts_start;
        idle = target - elapsed - ESPoffset;

        #ifdef VIDEO_FRAME_TIMING    

        if(Config::videomode) {

            if (sync_cnt++ == 0) {
                if (idle > 0) { 
                    delayMicroseconds(idle);
                }
            } else {

                // Audio sync (once every 250 frames ~ 2,5 seconds)
                if (sync_cnt++ == 250) {
                    ESPoffset = 128 - pwm_audio_rbstats();
                    sync_cnt = 0;
                }

                // Wait for vertical sync
                for (;;) {
                    if (vsync) break;
                }

            }

        } else {

            if (idle > 0) { 
                delayMicroseconds(idle);
            }

            // Audio sync
            if (sync_cnt++ & 0x0f) {
                ESPoffset = 128 - pwm_audio_rbstats();
                sync_cnt = 0;
            }

            // ESPmedian += ESPoffset;

        }
        
        #endif

        totalseconds += esp_timer_get_time() - ts_start;

    }

}

}
