///////////////////////////////////////////////////////////////////////////////
//
// ZX-ESPectrum-IDF - Sinclair ZX Spectrum emulator for ESP32 / IDF
//
// Copyright (c) 2023 Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
// https://github.com/EremusOne/ZX-ESPectrum-IDF
//
// Based on ZX-ESPectrum-Wiimote
// Copyright (c) 2020, 2022 David Crespo [dcrespo3d]
// https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
//
// Based on previous work by Ramón Martinez and Jorge Fuertes
// https://github.com/rampa069/ZX-ESPectrum
//
// Original project by Pete Todd
// https://github.com/retrogubbins/paseVGA
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

#include <stdio.h>
#include <string>

#include "ESPectrum.h"
#include "FileSNA.h"
#include "FileZ80.h"
#include "Config.h"
#include "FileUtils.h"
#include "OSDMain.h"
#include "Ports.h"
#include "MemESP.h"
#include "roms.h"
#include "CPU.h"
#include "Video.h"
#include "messages.h"
#include "AySound.h"
#include "Tape.h"
#include "Z80_JLS/z80.h"
#include "pwm_audio.h"
#include "fabgl.h"

#ifndef ESP32_SDL2_WRAPPER
#include "ZXKeyb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "soc/timer_group_struct.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#endif

// #include <stdint.h>
// #include <string.h>
// #include <stdbool.h>
// #include "driver/uart.h"

// #include "esp_bt.h"
// #include "nvs_flash.h"
// #include "esp_bt_device.h"
// #include "esp_gap_ble_api.h"
// #include "esp_gattc_api.h"
// #include "esp_gatt_defs.h"
// #include "esp_bt_main.h"
// #include "esp_system.h"
// #include "esp_gatt_common_api.h"
// #include "esp_log.h"
// #define GATTC_TAG                   "GATTC_SPP_DEMO"

using namespace std;

// works, but not needed for now
#pragma GCC optimize ("O3")

//=======================================================================================
// KEYBOARD
//=======================================================================================
fabgl::PS2Controller ESPectrum::PS2Controller;

//=======================================================================================
// AUDIO
//=======================================================================================
uint8_t ESPectrum::audioBuffer[ESP_AUDIO_SAMPLES_48] = { 0 };
uint8_t ESPectrum::overSamplebuf[ESP_AUDIO_OVERSAMPLES_48] = { 0 };
// uint8_t ESPectrum::SamplebufAY[ESP_AUDIO_SAMPLES_48] = { 0 };
signed char ESPectrum::aud_volume = ESP_DEFAULT_VOLUME;
uint32_t ESPectrum::audbufcnt = 0;
uint32_t ESPectrum::faudbufcnt = 0;
uint32_t ESPectrum::audbufcntAY = 0;
uint32_t ESPectrum::faudbufcntAY = 0;
int ESPectrum::lastaudioBit = 0;
int ESPectrum::faudioBit = 0;
int ESPectrum::samplesPerFrame = ESP_AUDIO_SAMPLES_48;
int ESPectrum::overSamplesPerFrame = ESP_AUDIO_OVERSAMPLES_48;
bool ESPectrum::AY_emu = false;
int ESPectrum::Audio_freq = ESP_AUDIO_FREQ_48;
// bool ESPectrum::Audio_restart = false;

QueueHandle_t audioTaskQueue;
TaskHandle_t audioTaskHandle;
uint8_t *param;

//=======================================================================================
// ARDUINO FUNCTIONS
//=======================================================================================

#ifndef ESP32_SDL2_WRAPPER
#define NOP() asm volatile ("nop")
#else
#define NOP() {for(int i=0;i<1000;i++){}}
#endif

int64_t IRAM_ATTR micros()
{
    // return (int64_t) (esp_timer_get_time());
    return esp_timer_get_time();    
}

unsigned long IRAM_ATTR millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

// inline void IRAM_ATTR delay(uint32_t ms)
// {
//     vTaskDelay(ms / portTICK_PERIOD_MS);
// }

void IRAM_ATTR delayMicroseconds(int64_t us)
{
    int64_t m = micros();
    if(us){
        int64_t e = (m + us);
        if(m > e){ //overflow
            while(micros() > e){
                NOP();
            }
        }
        while(micros() < e){
            NOP();
        }
    }
}

//=======================================================================================
// TIMING
//=======================================================================================

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
  printf("Heap caps get free size: %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  printf("=========================================================================\n\n");
#endif
}

//=======================================================================================
// SETUP
//=======================================================================================
// TaskHandle_t ESPectrum::loopTaskHandle;

void ESPectrum::setup() 
{

    //=======================================================================================
    // KEYBOARD
    //=======================================================================================

    PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
    PS2Controller.keyboard()->setScancodeSet(2); // IBM PC AT

    if (Config::slog_on) {
        showMemInfo("Keyboard started");
    }

    //=======================================================================================
    // PHYSICAL KEYBOARD (SINCLAIR 8 + 5 MEMBRANE KEYBOARD)
    //=======================================================================================

    #ifdef ZXKEYB
    ZXKeyb::setup();
    #endif

    //=======================================================================================
    // FILESYSTEM
    //=======================================================================================
    FileUtils::initFileSystem();
    Config::load();

    #ifndef ESP32_SDL2_WRAPPER

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
    // BOOTKEYS: Read keyboard for 500 ms. checking boot keys
    //=======================================================================================

    std:string b = "00";
    std::string s;
    for (int i=0; i<2000; i++) {
        s = bootKeyboard();
        if (s!="") {
            
            if (s.length()==2) 
                b = s; 
            else {
                b[0] = b[1];
                b[1] = s[0];
            }

            bool chgRes = true;

            if (b=="1Q" || b=="Q1") {
                Config::aspect_16_9=false;
                Config::videomode=0;
            } else
            if (b=="1W" || b=="W1") {
                Config::aspect_16_9=true;
                Config::videomode=0;
            } else
            if (b=="2Q" || b=="Q2") {
                Config::aspect_16_9=false;
                Config::videomode=1;
            } else
            if (b=="2W" || b=="W2") {
                Config::aspect_16_9=true;
                Config::videomode=1;
            } else
            if (b=="3Q" || b=="Q3") {
                Config::aspect_16_9=false;
                Config::videomode=2;
            } else
            if (b=="3W" || b=="W3") {
                Config::aspect_16_9=true;
                Config::videomode=2;
            } else chgRes = false;

            if (chgRes) {
                Config::ram_file="none";                
                Config::save();
                printf("%s\n", b.c_str());
                break;
            }

        }

        delayMicroseconds(250);

    }

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

    MemESP::modeSP3 = 0;
    MemESP::romSP3 = 0;
    MemESP::romInUse = 0;
    MemESP::bankLatch = 0;
    MemESP::videoLatch = 0;
    MemESP::romLatch = 0;

    MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[1] = (unsigned char *)MemESP::ram[5];
    MemESP::ramCurrent[2] = (unsigned char *)MemESP::ram[2];
    MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = false;

    if (Config::getArch() == "48K") MemESP::pagingLock = 1; else MemESP::pagingLock = 0;

    if (Config::slog_on) showMemInfo("RAM Initialized");

    //=======================================================================================
    // VIDEO
    //=======================================================================================

    VIDEO::Init();

    // Active graphic bank pointer
    VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;

    if (Config::slog_on) showMemInfo("VGA started");

    //=======================================================================================
    // AUDIO
    //=======================================================================================

    // Set samples per frame and AY_emu flag depending on arch
    if (Config::getArch() == "48K") {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_48;
        samplesPerFrame=ESP_AUDIO_SAMPLES_48; 
        AY_emu = Config::AY48;
        Audio_freq = ESP_AUDIO_FREQ_48;
    } else {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_128;
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        AY_emu = true;        
        Audio_freq = ESP_AUDIO_FREQ_128;
    }

    ESPoffset = 0;

    // Create Audio task
    audioTaskQueue = xQueueCreate(1, sizeof(uint8_t *));
    // Latest parameter = Core. In ESPIF, main task runs on core 0 by default. In Arduino, loop() runs on core 1.

    // xTaskCreatePinnedToCore(&ESPectrum::audioTask, "audioTask", 2048, NULL, configMAX_PRIORITIES - 1, &audioTaskHandle, 1);

    xTaskCreatePinnedToCore(&ESPectrum::audioTask, "audioTask", 1536, NULL, configMAX_PRIORITIES - 1, &audioTaskHandle, 1);

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

    // Init Z80
    CPU::setup();

    // Set Ports starting values
    for (int i = 0; i < 128; i++) Ports::port[i] = 0x1F;
    if (Config::joystick) Ports::port[0x1f] = 0; // Kempston

    // Set emulation loop sync target
    target = CPU::microsPerFrame();

    // Load romset
    Config::requestMachine(Config::getArch(), Config::getRomSet(), true);

    // Load snapshot if present in Config::ram_file
    if (Config::ram_file != NO_RAM_FILE) {
        
        if (FileUtils::hasSNAextension(Config::ram_file))
            FileSNA::load(Config::ram_file);        
        else if (FileUtils::hasZ80extension(Config::ram_file))
            FileZ80::load(Config::ram_file);

        Config::last_ram_file = Config::ram_file;

        // ESP host reset
        #ifndef SNAPSHOT_LOAD_LAST
        Config::ram_file = NO_RAM_FILE;
        Config::save();
        #endif

    }

    if (Config::slog_on) showMemInfo("ZX-ESPectrum-IDF setup finished.");

    // Create loop task
    // xTaskCreatePinnedToCore(&ESPectrum::loop, "loopTask", 4096, NULL, 1, &loopTaskHandle, 0);

}

//=======================================================================================
// RESET
//=======================================================================================
void ESPectrum::reset()
{

    // Ports
    for (int i = 0; i < 128; i++) Ports::port[i] = 0x1F;
    if (Config::joystick) Ports::port[0x1f] = 0; // Kempston

    // Memory
    MemESP::bankLatch = 0;
    MemESP::videoLatch = 0;
    MemESP::romLatch = 0;

    if (Config::getArch() == "48K") MemESP::pagingLock = 1; else MemESP::pagingLock = 0;

    MemESP::modeSP3 = 0;
    MemESP::romSP3 = 0;
    MemESP::romInUse = 0;

    MemESP::ramCurrent[0] = (unsigned char *)MemESP::rom[MemESP::romInUse];
    MemESP::ramCurrent[1] = (unsigned char *)MemESP::ram[5];
    MemESP::ramCurrent[2] = (unsigned char *)MemESP::ram[2];
    MemESP::ramCurrent[3] = (unsigned char *)MemESP::ram[MemESP::bankLatch];

    MemESP::ramContended[0] = false;
    MemESP::ramContended[1] = true;
    MemESP::ramContended[2] = false;
    MemESP::ramContended[3] = false;

    VIDEO::Reset();

    Tape::tapeFileName = "none";
    Tape::tapeStatus = TAPE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    // Empty audio buffers
    for (int i=0;i<ESP_AUDIO_OVERSAMPLES_48;i++) overSamplebuf[i]=0;
    for (int i=0;i<ESP_AUDIO_SAMPLES_48;i++) {
        audioBuffer[i]=0;
        AySound::SamplebufAY[i]=0;
    }
    lastaudioBit=0;

    // Set samples per frame and AY_emu flag depending on arch
    if (Config::getArch() == "48K") {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_48;
        samplesPerFrame=ESP_AUDIO_SAMPLES_48; 
        AY_emu = Config::AY48;
        Audio_freq = ESP_AUDIO_FREQ_48;
    } else {
        overSamplesPerFrame=ESP_AUDIO_OVERSAMPLES_128;
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        AY_emu = true;        
        Audio_freq = ESP_AUDIO_FREQ_128;
    }

    ESPoffset = 0;

    pwm_audio_stop();
    pwm_audio_set_param(Audio_freq,LEDC_TIMER_8_BIT,1);
    pwm_audio_start();
    pwm_audio_set_volume(aud_volume);

    // Reset AY emulation
    AySound::init();
    AySound::set_sound_format(Audio_freq,1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    // Emu loop sync target
    target = CPU::microsPerFrame();

    CPU::reset();

}

//=======================================================================================
// ROM SWITCHING
//=======================================================================================
void ESPectrum::loadRom(string arch, string romset) {

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

//=======================================================================================
// KEYBOARD / KEMPSTON
//=======================================================================================
bool IRAM_ATTR ESPectrum::readKbd(fabgl::VirtualKeyItem *Nextkey) {
    
    bool r = PS2Controller.keyboard()->getNextVirtualKey(Nextkey);
    // Global keys
    if (Nextkey->down) {
        if (Nextkey->vk == fabgl::VK_PRINTSCREEN) { // Capture framebuffer to BMP file in SD Card (thx @dcrespo3d!)
            pwm_audio_stop();
            CaptureToBmp();
            pwm_audio_start();
            r = false;
        }
        #ifdef DEV_STUFF 
        else if (Nextkey->vk == fabgl::VK_GRAVEACCENT) { // Show mem info
            multi_heap_info_t info;
            heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task
            printf("=========================================================================\n");
            printf("Total currently free in all non-continues blocks: %d\n", info.total_free_bytes);
            printf("Minimum free ever: %d\n", info.minimum_free_bytes);
            printf("Largest continues block to allocate big array: %d\n", info.largest_free_block);
            printf("Heap caps get free size: %d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

            printf("=========================================================================\n\n");
            heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);

            printf("=========================================================================\n");
            heap_caps_print_heap_info(MALLOC_CAP_8BIT);            
            
            printf("=========================================================================\n");
            heap_caps_print_heap_info(MALLOC_CAP_32BIT);                        

            printf("=========================================================================\n");
            heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

            printf("=========================================================================\n");
            heap_caps_print_heap_info(MALLOC_CAP_DMA);            

            printf("=========================================================================\n");
            heap_caps_print_heap_info(MALLOC_CAP_EXEC);            

            printf("=========================================================================\n");
            heap_caps_print_heap_info(MALLOC_CAP_IRAM_8BIT);            
            
            printf("=========================================================================\n");
            heap_caps_dump_all();

            printf("=========================================================================\n");
            UBaseType_t wm;
            wm = uxTaskGetStackHighWaterMark(audioTaskHandle);
            printf("Audio Task Stack HWM: %u\n", wm);
            // wm = uxTaskGetStackHighWaterMark(loopTaskHandle);
            // printf("Loop Task Stack HWM: %u\n", wm);
            wm = uxTaskGetStackHighWaterMark(VIDEO::videoTaskHandle);
            printf("Video Task Stack HWM: %u\n", wm);

            r = false;
        }    
        #endif
    }

    return r;
}

uint8_t ESPectrum::PS2cols[8] = { 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f };    
    
#ifdef ZXKEYB
static int zxDelay = 0;
#endif

void IRAM_ATTR ESPectrum::processKeyboard() {

    auto Kbd = PS2Controller.keyboard();
    fabgl::VirtualKeyItem NextKey;
    fabgl::VirtualKey KeytoESP;
    bool Kdown;
    bool r = false;

    while (Kbd->virtualKeyAvailable()) {

        r = readKbd(&NextKey);

        if (r) {

            KeytoESP = NextKey.vk;
            Kdown = NextKey.down;

            if ((Kdown) && (((KeytoESP >= fabgl::VK_F1) && (KeytoESP <= fabgl::VK_F12)) || (KeytoESP == fabgl::VK_PAUSE))) {
                OSD::do_OSD(KeytoESP);
                return;
            }

            // Joystick emulation
            if (Config::joystick) {
                // Kempston
                Ports::port[0x1f] = 0;
                bitWrite(Ports::port[0x1f], 0, Kbd->isVKDown(fabgl::VK_RIGHT) || Kbd->isVKDown(fabgl::VK_KP_RIGHT));
                bitWrite(Ports::port[0x1f], 1, Kbd->isVKDown(fabgl::VK_LEFT) || Kbd->isVKDown(fabgl::VK_KP_LEFT));
                bitWrite(Ports::port[0x1f], 2, Kbd->isVKDown(fabgl::VK_DOWN) || Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER));
                bitWrite(Ports::port[0x1f], 3, Kbd->isVKDown(fabgl::VK_UP) || Kbd->isVKDown(fabgl::VK_KP_UP));
                bitWrite(Ports::port[0x1f], 4, Kbd->isVKDown(fabgl::VK_RALT));
            }

            // Check keyboard status and map it to Spectrum Ports
            
            bitWrite(PS2cols[0], 0, (!Kbd->isVKDown(fabgl::VK_LSHIFT)) 
                                &   (!Kbd->isVKDown(fabgl::VK_RSHIFT))
                                &   (!Kbd->isVKDown(fabgl::VK_BACKSPACE)) // Backspace

                                // Cursor joystick (Shift part)
                                & ( (Config::joystick) | (!Kbd->isVKDown(fabgl::VK_LEFT)) & (!Kbd->isVKDown(fabgl::VK_RIGHT))
                                &   (!Kbd->isVKDown(fabgl::VK_UP)) & (!Kbd->isVKDown(fabgl::VK_DOWN))
                                &   (!Kbd->isVKDown(fabgl::VK_KP_LEFT)) & (!Kbd->isVKDown(fabgl::VK_KP_RIGHT))
                                &   (!Kbd->isVKDown(fabgl::VK_KP_UP)) & (!Kbd->isVKDown(fabgl::VK_KP_DOWN))
                                &   (!Kbd->isVKDown(fabgl::VK_KP_CENTER)) )

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

            bitWrite(PS2cols[3], 0, (!Kbd->isVKDown(fabgl::VK_1)) & (!Kbd->isVKDown(fabgl::VK_EXCLAIM)));
            bitWrite(PS2cols[3], 1, (!Kbd->isVKDown(fabgl::VK_2)) & (!Kbd->isVKDown(fabgl::VK_AT)));
            bitWrite(PS2cols[3], 2, (!Kbd->isVKDown(fabgl::VK_3)) & (!Kbd->isVKDown(fabgl::VK_HASH)));
            bitWrite(PS2cols[3], 3, (!Kbd->isVKDown(fabgl::VK_4)) & (!Kbd->isVKDown(fabgl::VK_DOLLAR)));
            bitWrite(PS2cols[3], 4, (!Kbd->isVKDown(fabgl::VK_5)) & (!Kbd->isVKDown(fabgl::VK_PERCENT))
                & ((Config::joystick) | (!Kbd->isVKDown(fabgl::VK_LEFT)) & (!Kbd->isVKDown(fabgl::VK_KP_LEFT))));

            bitWrite(PS2cols[4], 0, (!Kbd->isVKDown(fabgl::VK_0)) & (!Kbd->isVKDown(fabgl::VK_RIGHTPAREN))
                                &   (!Kbd->isVKDown(fabgl::VK_BACKSPACE))
                                &   ((Config::joystick) | (!Kbd->isVKDown(fabgl::VK_RALT))));
            bitWrite(PS2cols[4], 1, !Kbd->isVKDown(fabgl::VK_9) & (!Kbd->isVKDown(fabgl::VK_LEFTPAREN)));
            bitWrite(PS2cols[4], 2, (!Kbd->isVKDown(fabgl::VK_8)) & (!Kbd->isVKDown(fabgl::VK_ASTERISK))
                                &   ((Config::joystick) | (!Kbd->isVKDown(fabgl::VK_RIGHT)) & (!Kbd->isVKDown(fabgl::VK_KP_RIGHT))));
            bitWrite(PS2cols[4], 3, (!Kbd->isVKDown(fabgl::VK_7)) & (!Kbd->isVKDown(fabgl::VK_AMPERSAND))
                                &   ((Config::joystick) | (!Kbd->isVKDown(fabgl::VK_UP)) & (!Kbd->isVKDown(fabgl::VK_KP_UP))));
            bitWrite(PS2cols[4], 4, (!Kbd->isVKDown(fabgl::VK_6)) & (!Kbd->isVKDown(fabgl::VK_CARET))
                                &   ((Config::joystick) | (!Kbd->isVKDown(fabgl::VK_DOWN)) & (!Kbd->isVKDown(fabgl::VK_KP_DOWN)) & (!Kbd->isVKDown(fabgl::VK_KP_CENTER))));

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

    #ifdef ZXKEYB
    
    if (zxDelay > 0)
        zxDelay--;
    else
        // Process physical keyboard
        ZXKeyb::process();

    // Detect and process physical kbd menu key combinations
    // CS+SS+N -> FN Keys
    // F11 -> CS+SS+Q, F12 -> CS+SS+W
    // TO DO: Add delay after special key so keys as vol up vol down or toggles like F8 doesn't get re-pressed too fast.
    if ((!bitRead(ZXKeyb::ZXcols[0],0)) && (!bitRead(ZXKeyb::ZXcols[7],1))) {

        zxDelay = 15;

        if (!bitRead(ZXKeyb::ZXcols[3],0)) {
            OSD::do_OSD(fabgl::VK_F1);
        } else
        if (!bitRead(ZXKeyb::ZXcols[3],1)) {
            OSD::do_OSD(fabgl::VK_F2);
        } else
        if (!bitRead(ZXKeyb::ZXcols[3],2)) {
            OSD::do_OSD(fabgl::VK_F3);
        } else
        if (!bitRead(ZXKeyb::ZXcols[3],3)) {
            OSD::do_OSD(fabgl::VK_F4);
        } else
        if (!bitRead(ZXKeyb::ZXcols[3],4)) {
            OSD::do_OSD(fabgl::VK_F5);
        } else
        if (!bitRead(ZXKeyb::ZXcols[4],4)) {
            OSD::do_OSD(fabgl::VK_F6);
        } else
        if (!bitRead(ZXKeyb::ZXcols[4],3)) {
            OSD::do_OSD(fabgl::VK_F7);
        } else
        if (!bitRead(ZXKeyb::ZXcols[4],2)) {
            OSD::do_OSD(fabgl::VK_F8);
        } else
        if (!bitRead(ZXKeyb::ZXcols[4],1)) {
            OSD::do_OSD(fabgl::VK_F9);
        } else
        if (!bitRead(ZXKeyb::ZXcols[4],0)) {
            OSD::do_OSD(fabgl::VK_F10);
        } else
        if (!bitRead(ZXKeyb::ZXcols[2],0)) {
            CaptureToBmp();
        } else
        if (!bitRead(ZXKeyb::ZXcols[2],1)) {
            OSD::do_OSD(fabgl::VK_F12);
        } else
            zxDelay = 0;

        if (zxDelay) {
            // Set all keys as not pressed
            for (uint8_t i = 0; i < 8; i++) ZXKeyb::ZXcols[i] = 0x1f;
            return;
        }
    
    }

    // Combine both keyboards
    for (uint8_t rowidx = 0; rowidx < 8; rowidx++) {
        Ports::port[rowidx] = PS2cols[rowidx] & ZXKeyb::ZXcols[rowidx];
    }
    
    #else

    if (r) {
        for (uint8_t rowidx = 0; rowidx < 8; rowidx++) {
            Ports::port[rowidx] = PS2cols[rowidx];
        }
    }
    #endif

}

std::string ESPectrum::bootKeyboard() {

    auto Kbd = PS2Controller.keyboard();
    fabgl::VirtualKeyItem NextKey;
    bool r = false;

    #ifdef ZXKEYB

    // Process physical keyboard
    ZXKeyb::process();
    // Detect and process physical kbd menu key combinations

    if (!bitRead(ZXKeyb::ZXcols[3], 0)) { // 1
        if (!bitRead(ZXKeyb::ZXcols[2], 0)) { // Q
            return "1Q";
        } else 
        if (!bitRead(ZXKeyb::ZXcols[2], 1)) { // W
            return "1W";
        }
    } else
    if (!bitRead(ZXKeyb::ZXcols[3], 1)) { // 2
        if (!bitRead(ZXKeyb::ZXcols[2], 0)) { // Q
            return "2Q";
        } else 
        if (!bitRead(ZXKeyb::ZXcols[2], 1)) { // W
            return "2W";
        }
    } else
    if (!bitRead(ZXKeyb::ZXcols[3], 2)) { // 3
        if (!bitRead(ZXKeyb::ZXcols[2], 0)) { // Q
            return "3Q";
        } else 
        if (!bitRead(ZXKeyb::ZXcols[2], 1)) { // W
            return "3W";
        }
    }

    #endif

    while (Kbd->virtualKeyAvailable()) {

        r = Kbd->getNextVirtualKey(&NextKey);
        if (r) {

            // Check keyboard status
            if (PS2Controller.keyboard()->isVKDown(fabgl::VK_1)) {
                return "1";
            }

            // Check keyboard status
            if (PS2Controller.keyboard()->isVKDown(fabgl::VK_2)) {
                return "2";
            }

            // Check keyboard status
            if (PS2Controller.keyboard()->isVKDown(fabgl::VK_3)) {
                return "3";
            }

            if (PS2Controller.keyboard()->isVKDown(fabgl::VK_Q) || PS2Controller.keyboard()->isVKDown(fabgl::VK_q)) {
                return "Q";
            }

            if (PS2Controller.keyboard()->isVKDown(fabgl::VK_W) || PS2Controller.keyboard()->isVKDown(fabgl::VK_w)) {
                return "W";
            }

        }

    }

    return "";

}

// static int bmax = 0;

//=======================================================================================
// AUDIO
//=======================================================================================
void IRAM_ATTR ESPectrum::audioTask(void *unused) {

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
    pac.ringbuf_len        = /* 1024 * 8;*/ 2560;

    pwm_audio_init(&pac);
    pwm_audio_set_param(Audio_freq,LEDC_TIMER_8_BIT,1);
    pwm_audio_start();
    pwm_audio_set_volume(aud_volume);

    for (;;) {

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        pwm_audio_write(ESPectrum::audbuffertosend, samplesPerFrame, &written, 5 / portTICK_PERIOD_MS);

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        // Finish fill of oversampled audio buffers
        if (faudbufcnt < overSamplesPerFrame) 
            for (int i=faudbufcnt; i < overSamplesPerFrame;i++) overSamplebuf[i] = faudioBit;
        
        // Downsample beeper (median) and mix AY channels to output buffer
        int beeper;
        
        if (Z80Ops::is48) {

            if (AY_emu) {
                if (faudbufcntAY < ESP_AUDIO_SAMPLES_48)
                    AySound::gen_sound(ESP_AUDIO_SAMPLES_48 - faudbufcntAY , faudbufcntAY);
            }

            int n = 0;
            for (int i=0;i<ESP_AUDIO_OVERSAMPLES_48; i += 7) {    
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

            if (faudbufcntAY < ESP_AUDIO_SAMPLES_128)
                AySound::gen_sound(ESP_AUDIO_SAMPLES_128 - faudbufcntAY , faudbufcntAY);

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
        }
    }
}

void ESPectrum::audioFrameStart() {
    
    xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

    audbufcnt = 0;
    audbufcntAY = 0;    

}

void IRAM_ATTR ESPectrum::BeeperGetSample(int Audiobit) {
        // Audio buffer generation (oversample)
        uint32_t audbufpos = Z80Ops::is48 ? CPU::tstates >> 4 : CPU::tstates / 19;
        if (audbufpos != audbufcnt) {
            for (int i=audbufcnt;i<audbufpos;i++) overSamplebuf[i] = lastaudioBit;
            audbufcnt = audbufpos;
        }
}

void IRAM_ATTR ESPectrum::AYGetSample() {
    // AY buffer generation
    uint32_t audbufpos = CPU::tstates / (Z80Ops::is48 ? 112 : 114);
    if (audbufpos != audbufcntAY) {
        AySound::gen_sound(audbufpos - audbufcntAY , audbufcntAY);
        audbufcntAY = audbufpos;
    }
}

void ESPectrum::audioFrameEnd() {
    
    faudbufcnt = audbufcnt;
    faudioBit = lastaudioBit;

    faudbufcntAY = audbufcntAY;

    xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

}

//=======================================================================================
// MAIN LOOP
//=======================================================================================

int ESPectrum::sync_cnt = 0;
uint8_t *ESPectrum::audbuffertosend = ESPectrum::audioBuffer;

volatile bool ESPectrum::vsync = false;

// void IRAM_ATTR ESPectrum::loop(void *unused) {
void IRAM_ATTR ESPectrum::loop() {    

static char linea1[] = "CPU: 00000 / IDL: 00000 ";
static char linea2[] = "FPS:000.00 / FND:000.00 ";    
static double totalseconds = 0;
static double totalsecondsnodelay = 0;
int64_t ts_start, elapsed;
int64_t idle;

// int ESPmedian = 0;

// For Testing/Profiling: Start with stats on
// if (Config::aspect_16_9) 
//     VIDEO::DrawOSD169 = VIDEO::MainScreen_OSD;
// else
//     VIDEO::DrawOSD43  = VIDEO::BottomBorder_OSD;
// VIDEO::OSD = true;

// FILE *f = fopen("/sd/c/audioout.raw", "wb");
// if (f==NULL)
// {
//     printf("Error opening file for write.\n");
// }
// uint32_t fpart = 0;

// Simulate keypress, for testing
// bitWrite(Ports::port[5], 4, 0);

for(;;) {

    ts_start = micros();

    audioFrameStart();

    CPU::loop();

    audioFrameEnd();

    processKeyboard();

    // if (fpart!=1001) fpart++;
    // if (fpart<1000) {
    //     uint8_t* buffer = audioBuffer;
    //     fwrite(&buffer[0],samplesPerFrame,1,f);
    // } else {
    //     if (fpart==1000) {
    //         fclose(f);
    //         printf("Audio dumped!\n");
    //     }            
    // }

    // Draw stats, if activated, every 32 frames
    if (((CPU::framecnt & 31) == 0) && (VIDEO::OSD)) OSD::drawStats(linea1,linea2); 

    // Flashing flag change
    if (!(VIDEO::flash_ctr++ & 0x0f)) VIDEO::flashing ^= 0x80;

    // OSD calcs
    totalsecondsnodelay += micros() - ts_start;
    if (totalseconds >= 1000000) {

        if (elapsed < 100000) {
    
            #ifdef LOG_DEBUG_TIMING
            printf("===========================================================================\n");
            printf("[CPU] elapsed: %u; idle: %d\n", elapsed, idle);
            printf("[Audio] Volume: %d\n", aud_volume);
            printf("[Framecnt] %u; [Seconds] %f; [FPS] %f; [FPS (no delay)] %f\n", CPU::framecnt, totalseconds / 1000000, CPU::framecnt / (totalseconds / 1000000), CPU::framecnt / (totalsecondsnodelay / 1000000));
            // printf("[ESPoffset] %d\n", ESPoffset);
            showMemInfo();
            #endif

            // printf("[Framecnt] %u; [Seconds] %f; [FPS] %f; [FPS (no delay)] %f\n", CPU::framecnt, totalseconds / 1000000, CPU::framecnt / (totalseconds / 1000000), CPU::framecnt / (totalsecondsnodelay / 1000000));

            sprintf((char *)linea1,"CPU: %05d / IDL: %05d ", (int)(elapsed), (int)(idle));
            // sprintf((char *)linea1,"CPU: %05d / BMX: %05d ", (int)(elapsed), bmax);
            // sprintf((char *)linea1,"CPU: %05d / OFF: %05d ", (int)(elapsed), (int)(ESPmedian/50));
            sprintf((char *)linea2,"FPS:%6.2f / FND:%6.2f ", CPU::framecnt / (totalseconds / 1000000), CPU::framecnt / (totalsecondsnodelay / 1000000));    
        }

        totalseconds = 0;
        totalsecondsnodelay = 0;
        CPU::framecnt = 0;

        // ESPmedian = 0;

    }
    
    #ifdef VIDEO_FRAME_TIMING    
   
    elapsed = micros() - ts_start;
    idle = target - elapsed - ESPoffset;
    
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

    totalseconds += micros() - ts_start;

}

}
