##############################################################################/
##
## ZX-ESPectrum - ZX Spectrum emulator for ESP32
## ESPectrum Bitmap Format (EBF) converter
## 
## Copyright (c) 2023 David Crespo [dcrespo3d]
## https://github.com/dcrespo3d
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to
## deal in the Software without restriction, including without limitation the
## rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
## sell copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
## 
## The above copyright notice and this permission notice shall be included in
## all copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
## FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
## DEALINGS IN THE SOFTWARE.

import sys
from PIL import Image
## https://pillow.readthedocs.io/en/stable/

MAGIC_EBF8 = 'EBF8'
MAGIC_EBF4 = 'EBF4'

# EBF Format description

# EBF8 (Native ESPectrum pixelformat, HVBBGGRR)
# [4 bytes] magic: "EBF8"
# [2 bytes] image width  W (little endian)
# [2 bytes] image height H (little endian)
# [N bytes] image data in native pixelformat: N = W * H

# EBF4 (4bpp indices into palette)
# [4 bytes] magic: "EBF4"
# [2 bytes] image width  W (little endian)
# [2 bytes] image height H (little endian)
# [16 bytes] palette with 16 entries in native ESPectrum pixelformat, HVBBGGRR
# [N bytes] image indices in 16bpp. N = floor(W/2) * H
# NOTE: when W is an odd value, an additional index 15 (the last one) will be padded into last byte of scanline

# from video.h: some colors for 6 bit mode
#                             //   BB GGRR 
#define BRI_BLACK   0xC0      // 1100 0000
#define BRI_BLUE    0xF0      // 1111 0000
#define BRI_RED     0xC3      // 1100 0011
#define BRI_GREEN   0xCC      // 1100 1100
#define BRI_WHITE   0xFF      // 1111 1111

def rgb222_from_rgb888(rgb888tuple):
    # a tuple is (r,g,b) with r,g,b between 0 and 255
    r, g, b = rgb888tuple
    r = r >> 6
    g = g >> 6
    b = b >> 6
    # set high two bits and compose others as pattern says (MSB HVBBGGRR LSB)
    rgb222 = 0xC0 | r | (g << 2) | (b << 4)
    return rgb222

def rgb888_from_rgb222(rgb222int):
    r2 =  rgb222int & 0x03
    g2 = (rgb222int & 0x0C) >> 2
    b2 = (rgb222int & 0x30) >> 4
    r8 = r2 | (r2 << 2) | (r2 << 4) | (r2 << 6)
    g8 = g2 | (g2 << 2) | (g2 << 4) | (g2 << 6)
    b8 = b2 | (b2 << 2) | (b2 << 4) | (b2 << 6)
    return (r8, g8, b8)

def iarr_from_str(magic_str):
    res = []
    for ch in magic_str:
        res.append(ord(ch))
    return res

def str_from_iarr(iarr):
    s = ''
    for val in iarr:
        s += chr(val)
    return s

def iarr_from_u16(u16):
    lo =  u16 & 0x00FF
    hi = (u16 & 0xFF00) >> 8
    return [lo, hi]     # little endian

def u16_from_iarr(iarr):
    lo = iarr[0]
    hi = iarr[1]
    return lo + (hi << 8)

def ebf8arr_from_pngfile(ifn):
    # print('opening ' + ifn)
    img = Image.open(ifn)
    img = img.convert('RGB')
    w, h = img.size
    # print('image size:', img.size)

    ebf8arr = []
    ebf8arr += iarr_from_str(MAGIC_EBF8)
    ebf8arr += iarr_from_u16(w)
    ebf8arr += iarr_from_u16(h)
    for y in range(h):
        for x in range(w):
            pixel888 = img.getpixel((x,y))
            pixel222 = rgb222_from_rgb888(pixel888)
            # print(pixel888, hex(pixel222))
            ebf8arr.append(pixel222)
    return ebf8arr

def ebf8arr_to_pngfile(ebf8arr, ofn):
    magic_str = str_from_iarr(ebf8arr[0:4])
    if magic_str != MAGIC_EBF8:
        raise Exception('ebf8: expected magic EBF8, found ' + magic_str);

    w = u16_from_iarr(ebf8arr[4:6])
    h = u16_from_iarr(ebf8arr[6:8])
    pixdata = ebf8arr[8:]
    img = Image.new('RGB', (w,h))
    idx = 0
    for y in range(h):
        for x in range(w):
            pixel222 = pixdata[idx]
            idx += 1
            pixel888 = rgb888_from_rgb222(pixel222)
            img.putpixel((x,y), pixel888)
    img.save(ofn)

def ebf4arr_from_ebf8arr(ebf8arr):
    magic_str = str_from_iarr(ebf8arr[0:4])
    w = u16_from_iarr(ebf8arr[4:6])
    h = u16_from_iarr(ebf8arr[6:8])
    pixdata = ebf8arr[8:]
    ebf4arr = []
    ebf4arr += iarr_from_str(MAGIC_EBF4)
    ebf4arr += iarr_from_u16(w)
    ebf4arr += iarr_from_u16(h)

    # table: for a given pixel value, get its palette index
    idx4pix = {}
    # table: for a given palette index, get its pixel value
    pix4idx = {}
    idxcount = 0

    # for indexing into bitmap
    bmpidx = 0

    # create palette
    # traverse scanlines, then pixels in scanline
    for y in range(h):
        for x in range(w):
            # pixel bitamp value
            pixel222 = pixdata[bmpidx]
            bmpidx += 1
            # annotate into tables if not present
            if not pixel222 in idx4pix:
                idx4pix[pixel222] = idxcount
                pix4idx[idxcount] = pixel222
                idxcount += 1

    if idxcount > 16:
        raise Exception('EBF4: unique colors must NOT exceed 16, found ' + str(idxcount))
    
    # convert palette table into palette array, padding up to 16 entries
    palette = []
    for palidx in range(16):
        if palidx in pix4idx:
            palette.append(pix4idx[palidx])
        else:
            palette.append(0xF3) # unused palette entries default to purple for error detection
    # print(palette)

    # add palette to data array
    ebf4arr += palette 
    
    # xpad: x coordinate for padding, -1 if not applyable
    xpad = -1
    if w & 1: xpad = w - 1

    # temporary array of palette index values
    # which will be compacted to 2 4bpp values per byte, ex: 0F 03 07 04 -> F3 74
    tmpdata = []

    # populate tmp data, each bye a looked up index into palette, for given pixel value
    palidx = 0
    bmpidx = 0
    for y in range(h):
        for x in range(w):
            pixel222 = pixdata[bmpidx]
            bmpidx += 1
            palidx = idx4pix[pixel222]
            tmpdata.append(palidx)
            if x == xpad:
                tmpdata.append(15)  # last index
    # print(tmpdata)

    # array for compacted data
    compdata = []

    # compact two nibbles into one byte
    compval = 0
    for i in range(len(tmpdata)):
        if not i & 1:
            compval = tmpdata[i] << 4
        else:
            compval |= tmpdata[i]
            compdata.append(compval)
    # print(compdata)

    # add bitmap to data array
    ebf4arr += compdata

    return ebf4arr

def ebf4arr_to_pngfile(ebf4arr, ofn):
    magic_str = str_from_iarr(ebf4arr[0:4])
    if magic_str != MAGIC_EBF4:
        raise Exception('ebf4: expected magic EBF4, found ' + magic_str);

    w = u16_from_iarr(ebf4arr[4:6])
    h = u16_from_iarr(ebf4arr[6:8])
    palette = ebf4arr[8:24]
    pixdata = ebf4arr[24:]

    # f*ck around and find out (uncommenting this)
    # palette.reverse()

    # create lookup table for palette
    pix4idx = {}
    for i in range(16):
        pix4idx[i] = palette[i]

    # uncompact data
    uncompdata = []
    for compval in pixdata:
        hinib = (compval & 0xF0) >> 4
        lonib = (compval & 0x0F)
        uncompdata.append(hinib)
        uncompdata.append(lonib)
    # print(uncompdata)

    stride = w
    if (w & 1): stride += 1

    img = Image.new('RGB', (w,h))
    for y in range(h):
        for x in range(w):
            bmpidx = x + y * stride
            palidx = uncompdata[bmpidx]
            pixel222 = pix4idx[palidx]
            pixel888 = rgb888_from_rgb222(pixel222)
            img.putpixel((x,y), pixel888)
    img.save(ofn)

def pngfile_to_ebf8file(ifn, ofn):
    print('converting ' + ifn + ' to ' + ofn + '...')
    ebf8arr = ebf8arr_from_pngfile(ifn)
    f = open(ofn, 'wb')
    f.write(bytes(ebf8arr))
    f.close()

def pngfile_to_ebf4file(ifn, ofn):
    print('converting ' + ifn + ' to ' + ofn + '...')
    ebf8arr = ebf8arr_from_pngfile(ifn)
    ebf4arr = ebf4arr_from_ebf8arr(ebf8arr)
    f = open(ofn, 'wb')
    f.write(bytes(ebf4arr))
    f.close()

def ebf8file_to_pngfile(ifn, ofn):
    print('converting ' + ifn + ' to ' + ofn + '...')
    f = open(ifn, 'rb')
    ebf8arr = list(f.read())
    f.close()
    ebf8arr_to_pngfile(ebf8arr, ofn)
    
def ebf4file_to_pngfile(ifn, ofn):
    print('converting ' + ifn + ' to ' + ofn + '...')
    f = open(ifn, 'rb')
    ebf4arr = list(f.read())
    f.close()
    ebf4arr_to_pngfile(ebf4arr, ofn)
    

TEST_DECODE = True

usage = '''EBF-convert, converts PNG to EBF (ESPectrum Bitmap Format)
usage:
    python EBF-convert.py input-file.png
'''

if __name__ == '__main__':
    if (len(sys.argv) != 2):
        print(usage)
        exit(-1)
    
    ifn = sys.argv[1]
    if not ifn.endswith('.png'):
        print('expected lowercase png extension, please try again')
        exit(-1)

    print('='*80)
    print('Creating EBF8 and EBF4 versions of input file:')

    ofn8 = ifn.replace('.png', '.ebf8')
    pngfile_to_ebf8file(ifn, ofn8);

    ofn4 = ifn.replace('.png', '.ebf4')
    pngfile_to_ebf4file(ifn, ofn4)    

    if TEST_DECODE:
        print('='*80)
        print('Conversion check: converting EBF8 and EBF4 back to PNG:')
        ebf8file_to_pngfile(ofn8, ofn8+'.png')
        ebf4file_to_pngfile(ofn4, ofn4+'.png')
