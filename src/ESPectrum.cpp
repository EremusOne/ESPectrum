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

#include <stdio.h>
#include <string>

#include "ESPectrum.h"
#include "FileSNA.h"
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "soc/timer_group_struct.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#endif

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
signed char ESPectrum::aud_volume = -8;
uint32_t ESPectrum::audbufcnt = 0;
uint32_t ESPectrum::faudbufcnt = 0;
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

// int ESPectrum::ESPoffset = 0; // Testing

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
void ESPectrum::setup() 
{
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
    // VIDEO
    //=======================================================================================

    VIDEO::Init();

    if (Config::slog_on) showMemInfo("VGA started");

    //=======================================================================================
    // MEMORY SETUP
    //=======================================================================================

    MemESP::ram5 = staticMemPage0;
    MemESP::ram0 = staticMemPage1;
    MemESP::ram2 = staticMemPage2;

    // MemESP::ram1 = (unsigned char *) heap_caps_malloc(0x4000,MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // MemESP::ram3 = (unsigned char *) heap_caps_malloc(0x4000,MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // MemESP::ram4 = (unsigned char *) heap_caps_malloc(0x4000,MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // MemESP::ram6 = (unsigned char *) heap_caps_malloc(0x4000,MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // MemESP::ram7 = (unsigned char *) heap_caps_malloc(0x4000,MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

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

    if (Config::slog_on) showMemInfo("RAM Initialized");

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

    // Create Audio task
    audioTaskQueue = xQueueCreate(1, sizeof(uint8_t *));
    // Latest parameter = Core. In ESPIF, main task runs on core 0 by default. In Arduino, loop() runs on core 1.
    xTaskCreatePinnedToCore(&ESPectrum::audioTask, "audioTask", 1024, NULL, 5, &audioTaskHandle, 1);

    // AY Sound
    AySound::init();
    AySound::set_sound_format(Audio_freq,1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    
    AySound::reset();

    //=======================================================================================
    // KEYBOARD
    //=======================================================================================

    PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
    PS2Controller.keyboard()->setScancodeSet(2); // IBM PC AT

    string cfgLayout = Config::kbd_layout;

    if(cfgLayout == "ES") 
            PS2Controller.keyboard()->setLayout(&fabgl::SpanishLayout);                
    else if(cfgLayout == "UK") 
            PS2Controller.keyboard()->setLayout(&fabgl::UKLayout);                
    else if(cfgLayout == "DE") 
            PS2Controller.keyboard()->setLayout(&fabgl::GermanLayout);                
    else if(cfgLayout == "FR") 
            PS2Controller.keyboard()->setLayout(&fabgl::FrenchLayout);            
    else 
            PS2Controller.keyboard()->setLayout(&fabgl::USLayout);

    if (Config::slog_on) {
        showMemInfo("Keyboard started");
    }

    // printf("Kbd layout: %s\n",Config::kbd_layout.c_str());

    // Init tape
    Tape::Init();

    // START Z80
    CPU::setup();

    // Ports
    for (int i = 0; i < 128; i++) Ports::port[i] = 0x1F;
    if (Config::joystick) Ports::port[0x1f] = 0; // Kempston

    MemESP::bankLatch = 0;
    MemESP::videoLatch = 0;
    MemESP::romLatch = 0;

    if (Config::getArch() == "48K") MemESP::pagingLock = 1; else MemESP::pagingLock = 0;

    MemESP::modeSP3 = 0;
    MemESP::romSP3 = 0;
    MemESP::romInUse = 0;

    // Active graphic bank pointer
    VIDEO::grmem = MemESP::videoLatch ? MemESP::ram7 : MemESP::ram5;

    Tape::tapeFileName = "none";
    Tape::tapeStatus = TAPE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    // Video sync
    target = CPU::microsPerFrame();

    Config::requestMachine(Config::getArch(), Config::getRomSet(), true);

    #ifdef SNAPSHOT_LOAD_LAST

    if (Config::ram_file != NO_RAM_FILE) {
        OSD::changeSnapshot(Config::ram_file);
    }

    #endif // SNAPSHOT_LOAD_LAST

    if (Config::slog_on) showMemInfo("ZX-ESPectrum-IDF setup finished.");

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

    pwm_audio_stop();

    // Empty audio buffers
    for (int i=0;i<ESP_AUDIO_OVERSAMPLES_48;i++) overSamplebuf[i]=0;
    for (int i=0;i<ESP_AUDIO_SAMPLES_48;i++) {
        audioBuffer[i]=0;
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

    pwm_audio_set_param(Audio_freq,LEDC_TIMER_8_BIT,1);

    // Reset AY emulation
    AySound::init();
    AySound::set_sound_format(Audio_freq,1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    pwm_audio_start();
    
    // ESPectrum::Audio_restart = true;

    // Video sync
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
            CaptureToBmp();
            r = false;
        }
        #ifdef DEV_STUFF 
        else if (Nextkey->vk == fabgl::VK_DEGREE) { // Show mem info
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
            printf("Stack HWM: %u\n", wm);
            
            r = false;
        }    
        #endif
    }

    return r;
}

void IRAM_ATTR ESPectrum::processKeyboard() {
    
    fabgl::VirtualKeyItem NextKey;
    fabgl::VirtualKey KeytoESP;
    bool Kdown;

    while (PS2Controller.keyboard()->virtualKeyAvailable()) {

        bool r = readKbd(&NextKey);

        if (r) {
            KeytoESP = NextKey.vk;
            Kdown = NextKey.down;

            if ((Kdown) && (((KeytoESP >= fabgl::VK_F1) && (KeytoESP <= fabgl::VK_F12)) || (KeytoESP == fabgl::VK_PAUSE))) {
                // vTaskDelay(50 / portTICK_PERIOD_MS); // Generous delay to let audio task to flush or finish processing buffer if needed
                vTaskSuspend(audioTaskHandle);
                OSD::do_OSD(KeytoESP);
                vTaskResume(audioTaskHandle);
                continue;
            }

            if (KeytoESP == fabgl::VK_RETURN) {
                bitWrite(Ports::port[6], 0, !Kdown); // ENTER
                continue;
            }

            if (KeytoESP == fabgl::VK_LCTRL) {
                bitWrite(Ports::port[0], 0, !Kdown); // CAPS SHIFT                
                continue;
            }

            if (KeytoESP == fabgl::VK_RCTRL) {
                bitWrite(Ports::port[7], 1, !Kdown); // SYMBOL SHIFT
                continue;
            }

            if (KeytoESP == fabgl::VK_SPACE) {
                bitWrite(Ports::port[7], 0, !Kdown); // SPACE
                continue;
            }

            if (KeytoESP == fabgl::VK_BACKSPACE) {
                bitWrite(Ports::port[0], 0, !Kdown); // CAPS SHIFT
                bitWrite(Ports::port[4], 0, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_COMMA) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[7], 3, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_PERIOD) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[7], 2, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_PLUS) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[6], 2, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_MINUS) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[6], 3, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_QUOTE) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[4], 3, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_QUOTEDBL) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[5], 0, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_LEFTPAREN) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[4], 2, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_RIGHTPAREN) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[4], 1, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_EQUALS) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[6], 1, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_COLON) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[0], 1, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_SEMICOLON) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[5], 1, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_SLASH) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[0], 4, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_EXCLAIM) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[3], 0, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_AT) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[3], 1, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_HASH) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[3], 2, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_DOLLAR) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[3], 3, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_PERCENT) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[3], 4, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_AMPERSAND) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[4], 4, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_UNDERSCORE) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[4], 0, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_LESS) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[2], 3, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_GREATER) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[2], 4, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_ASTERISK) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[7], 4, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_QUESTION) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[0], 3, !Kdown);
                continue;
            }

            if (KeytoESP == fabgl::VK_POUND) {
                bitWrite(Ports::port[7], 1, !Kdown);
                bitWrite(Ports::port[0], 2, !Kdown);
                continue;
            }

            // Joystick emulation
            if (Config::joystick) {

                // Kempston
                Ports::port[0x1f] = 0;

                bitWrite(Ports::port[0x1f], 0, PS2Controller.keyboard()->isVKDown(fabgl::VK_RIGHT) || PS2Controller.keyboard()->isVKDown(fabgl::VK_KP_RIGHT));
                bitWrite(Ports::port[0x1f], 1, PS2Controller.keyboard()->isVKDown(fabgl::VK_LEFT) || PS2Controller.keyboard()->isVKDown(fabgl::VK_KP_LEFT));
                bitWrite(Ports::port[0x1f], 2, PS2Controller.keyboard()->isVKDown(fabgl::VK_DOWN) || PS2Controller.keyboard()->isVKDown(fabgl::VK_KP_DOWN) || PS2Controller.keyboard()->isVKDown(fabgl::VK_KP_CENTER));
                bitWrite(Ports::port[0x1f], 3, PS2Controller.keyboard()->isVKDown(fabgl::VK_UP) || PS2Controller.keyboard()->isVKDown(fabgl::VK_KP_UP));
                bitWrite(Ports::port[0x1f], 4, PS2Controller.keyboard()->isVKDown(fabgl::VK_RALT));
        
            } else {

                // Cursor
                if (KeytoESP == fabgl::VK_DOWN || KeytoESP == fabgl::VK_KP_CENTER || KeytoESP == fabgl::VK_KP_DOWN) {
                    bitWrite(Ports::port[4], 4, !Kdown);
                    continue;
                }

                if (KeytoESP == fabgl::VK_UP || KeytoESP == fabgl::VK_KP_UP) {
                    bitWrite(Ports::port[4], 3, !Kdown);
                    continue;
                }

                if (KeytoESP == fabgl::VK_LEFT || KeytoESP == fabgl::VK_KP_LEFT) {
                    bitWrite(Ports::port[3], 4, !Kdown);
                    continue;
                }

                if (KeytoESP == fabgl::VK_RIGHT || KeytoESP == fabgl::VK_KP_RIGHT) {
                    bitWrite(Ports::port[4], 2, !Kdown);
                    continue;
                }

                if (KeytoESP == fabgl::VK_RALT) {
                    bitWrite(Ports::port[4], 0, !Kdown);
                    continue;
                }

            }

            // TO DO: CONVERT ALL THIS STUFF TO A TABLE WITH KEY -> PORT,BITS RELATIONSHIPS
            
            //bitWrite(Ports::port[0], 0, !PS2Controller.keyboard()->isVKDown(fabgl::VK_LCTRL)); // CAPS SHIFT
            bitWrite(Ports::port[0], 1, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_Z)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_z)));
            bitWrite(Ports::port[0], 2, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_X)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_x)));
            bitWrite(Ports::port[0], 3, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_C)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_c)));
            bitWrite(Ports::port[0], 4, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_V)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_v)));

            bitWrite(Ports::port[1], 0, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_A)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_a)));    
            bitWrite(Ports::port[1], 1, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_S)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_s)));
            bitWrite(Ports::port[1], 2, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_D)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_d)));
            bitWrite(Ports::port[1], 3, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_F)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_f)));
            bitWrite(Ports::port[1], 4, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_G)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_g)));

            bitWrite(Ports::port[2], 0, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_Q)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_q)));    
            bitWrite(Ports::port[2], 1, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_W)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_w)));
            bitWrite(Ports::port[2], 2, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_E)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_e)));
            bitWrite(Ports::port[2], 3, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_R)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_r)));
            bitWrite(Ports::port[2], 4, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_T)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_t)));

            bitWrite(Ports::port[3], 0, !PS2Controller.keyboard()->isVKDown(fabgl::VK_1));
            bitWrite(Ports::port[3], 1, !PS2Controller.keyboard()->isVKDown(fabgl::VK_2));
            bitWrite(Ports::port[3], 2, !PS2Controller.keyboard()->isVKDown(fabgl::VK_3));
            bitWrite(Ports::port[3], 3, !PS2Controller.keyboard()->isVKDown(fabgl::VK_4));
            bitWrite(Ports::port[3], 4, !PS2Controller.keyboard()->isVKDown(fabgl::VK_5));

            bitWrite(Ports::port[4], 0, !PS2Controller.keyboard()->isVKDown(fabgl::VK_0));
            bitWrite(Ports::port[4], 1, !PS2Controller.keyboard()->isVKDown(fabgl::VK_9));
            bitWrite(Ports::port[4], 2, !PS2Controller.keyboard()->isVKDown(fabgl::VK_8));
            bitWrite(Ports::port[4], 3, !PS2Controller.keyboard()->isVKDown(fabgl::VK_7));
            bitWrite(Ports::port[4], 4, !PS2Controller.keyboard()->isVKDown(fabgl::VK_6));

            bitWrite(Ports::port[5], 0, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_P)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_p)));
            bitWrite(Ports::port[5], 1, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_O)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_o)));
            bitWrite(Ports::port[5], 2, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_I)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_i)));
            bitWrite(Ports::port[5], 3, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_U)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_u)));
            bitWrite(Ports::port[5], 4, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_Y)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_y)));

            // bitWrite(Ports::port[6], 0, !PS2Controller.keyboard()->isVKDown(fabgl::VK_RETURN));
            bitWrite(Ports::port[6], 1, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_L)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_l)));
            bitWrite(Ports::port[6], 2, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_K)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_k)));
            bitWrite(Ports::port[6], 3, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_J)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_j)));
            bitWrite(Ports::port[6], 4, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_H)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_h)));

            // bitWrite(Ports::port[7], 0, !PS2Controller.keyboard()->isVKDown(fabgl::VK_SPACE));
            // bitWrite(Ports::port[7], 1, !PS2Controller.keyboard()->isVKDown(fabgl::VK_RCTRL)); // SYMBOL SHIFT
            bitWrite(Ports::port[7], 2, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_M)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_m)));
            bitWrite(Ports::port[7], 3, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_N)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_n)));
            bitWrite(Ports::port[7], 4, (!PS2Controller.keyboard()->isVKDown(fabgl::VK_B)) & (!PS2Controller.keyboard()->isVKDown(fabgl::VK_b)));

        }

    }

}

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
    pac.ringbuf_len        = 1024 * 8;

    // for (;;) {
    
    pwm_audio_init(&pac);
    pwm_audio_set_param(Audio_freq,LEDC_TIMER_8_BIT,1);
    pwm_audio_start();
    pwm_audio_set_volume(aud_volume);

    for (;;) {

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        // // Deinit audio to reinit with changed parameters
        // if (ESPectrum::Audio_restart) {
        //     ESPectrum::Audio_restart = false;
        //     pwm_audio_stop();
        //     pwm_audio_deinit();
        //     break;
        // }
        
        pwm_audio_write(audioBuffer, samplesPerFrame, &written, portMAX_DELAY);

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        int i = 0;
        
        // Finish fill of oversampled audio buffer
        if (faudbufcnt < overSamplesPerFrame) {
            int signal = faudioBit ? 127: 0;
            for (i=faudbufcnt; i < overSamplesPerFrame;i++) overSamplebuf[i] = signal;
        }
        
        // Downsample beeper (median) and mix AY channels to output buffer
        int beeper;
        
        if (Z80Ops::is48) {

            if (AY_emu) {
                
                // AySound::update(); // TO DO: This should be done reading a buffer of AY orders built during frame

                AySound::gen_sound(audioBuffer, ESP_AUDIO_SAMPLES_48, 0);

            }

            int n = 0;
            for (int i=0;i<overSamplesPerFrame; i += 7) {    
                // Downsample (median)
                beeper  =  overSamplebuf[i];
                beeper +=  overSamplebuf[i+1];
                beeper +=  overSamplebuf[i+2];
                beeper +=  overSamplebuf[i+3];
                beeper +=  overSamplebuf[i+4];
                beeper +=  overSamplebuf[i+5];
                beeper +=  overSamplebuf[i+6];

                beeper = AY_emu ? (beeper / 7) + audioBuffer[n] : beeper / 7;

                audioBuffer[n] = beeper > 255 ? 255 : beeper; // Clamp

                n++;

            }

        } else {

            // AySound::update(); // TO DO: This should be done reading a buffer of AY orders built during frame

            AySound::gen_sound(audioBuffer, ESP_AUDIO_SAMPLES_128, 0);
            
            int n = 0;
            for (int i=0;i<overSamplesPerFrame; i += 6) {

                // Downsample (median)
                beeper  =  overSamplebuf[i];
                beeper +=  overSamplebuf[i+1];
                beeper +=  overSamplebuf[i+2];
                beeper +=  overSamplebuf[i+3];
                beeper +=  overSamplebuf[i+4];
                beeper +=  overSamplebuf[i+5];

                beeper = (beeper / 6) + audioBuffer[n];

                audioBuffer[n] = beeper > 255 ? 255 : beeper; // Clamp

                n++;

            }

        }

    }

    // }

}

void ESPectrum::audioFrameStart() {
    
    xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

    audbufcnt = 0;

}

void IRAM_ATTR ESPectrum::audioGetSample(int Audiobit) {

    if (Audiobit != lastaudioBit) {
        // Audio buffer generation (oversample)
        uint32_t audbufpos = Z80Ops::is48 ? CPU::tstates >> 4 : CPU::tstates / 19;
        int signal = lastaudioBit ? 127: 0;
        for (int i=audbufcnt;i<audbufpos;i++) {
            overSamplebuf[i] = signal;
        }
        audbufcnt = audbufpos;
        lastaudioBit = Audiobit;
    }

}

void ESPectrum::audioFrameEnd() {
    
    faudbufcnt = audbufcnt;
    faudioBit = lastaudioBit;

    xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

}

//=======================================================================================
// MAIN LOOP
//=======================================================================================

void IRAM_ATTR ESPectrum::loop() {

static char linea1[] = "CPU: 00000 / IDL: 00000 ";
static char linea2[] = "FPS:000.00 / FND:000.00 ";    
static double totalseconds = 0;
static double totalsecondsnodelay = 0;
int64_t ts_start, elapsed;
int64_t idle;

// For Testing/Profiling: Start with stats on
// VIDEO::LineDraw = LINEDRAW_FPS;
// VIDEO::BottomDraw = BOTTOMBORDER_FPS;

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
    if (!(VIDEO::flash_ctr++ & 0x0f)) VIDEO::flashing ^= 0b10000000;

    processKeyboard();
    
    elapsed = micros() - ts_start;
    idle = target - elapsed;
    if (idle < 0) idle = 0;

    #ifdef VIDEO_FRAME_TIMING
    totalseconds += idle;
    #endif
    
    totalseconds += elapsed;
    totalsecondsnodelay += elapsed;
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

            sprintf((char *)linea1,"CPU: %05d / IDL: %05d ", (int)(elapsed), (int)(idle));
            sprintf((char *)linea2,"FPS:%6.2f / FND:%6.2f ", CPU::framecnt / (totalseconds / 1000000), CPU::framecnt / (totalsecondsnodelay / 1000000));    

        }

        totalseconds = 0;
        totalsecondsnodelay = 0;
        CPU::framecnt = 0;

    }
    
    #ifdef VIDEO_FRAME_TIMING    
    elapsed = micros() - ts_start;
    idle = target - elapsed;
    if (!Z80Ops::is48) idle -= 90;
    if (idle > 0) delayMicroseconds(idle);
    #endif

}

}
