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

#ifndef FileSNA_h
#define FileSNA_h

#include <inttypes.h>
#include <string>

using namespace std;

class FileSNA
{
public:
    static bool load(string sna_fn);

    // // save snapshot in SNA format to disk using specified filename.
    // // this function tries to save using pages, 
    // static bool save(string sna_fn);
    // // using this function you can choose whether to write pages block by block, or byte by byte.
    // static bool save(string sna_fn, bool blockMode);

    // static bool loadQuick();
    // static bool saveQuick();

    // static bool loadFromMem(uint8_t* srcBuffer, uint32_t size);
    // static bool saveToMem(uint8_t* dstBuffer, uint32_t size);

    // static bool loadQuick48();
    // static bool saveQuick48();

    // static bool isQuickAvailable();
    // static bool isPersistAvailable(string filename);
};

#endif