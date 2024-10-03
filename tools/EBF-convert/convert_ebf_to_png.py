import sys
from PIL import Image

MAGIC_EBF8 = 'EBF8'
MAGIC_EBF4 = 'EBF4'

def rgb222_from_rgb888(rgb888tuple):
    r, g, b = rgb888tuple
    r = r >> 6
    g = g >> 6
    b = b >> 6
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

def ebf8file_to_pngfile(ifn, ofn):
    with open(ifn, 'rb') as f:
        ebf8arr = list(f.read())
    
    magic_str = ''.join([chr(x) for x in ebf8arr[0:4]])
    if magic_str != MAGIC_EBF8:
        raise Exception('Expected magic EBF8, found ' + magic_str)

    w = int.from_bytes(ebf8arr[4:6], 'little')
    h = int.from_bytes(ebf8arr[6:8], 'little')
    pixdata = ebf8arr[8:]

    img = Image.new('RGB', (w, h))
    idx = 0
    for y in range(h):
        for x in range(w):
            pixel222 = pixdata[idx]
            idx += 1
            pixel888 = rgb888_from_rgb222(pixel222)
            img.putpixel((x, y), pixel888)

    img.save(ofn)
    print(f'Converted {ifn} to {ofn}.')

def ebf4file_to_pngfile(ifn, ofn):
    with open(ifn, 'rb') as f:
        ebf4arr = list(f.read())
    
    magic_str = ''.join([chr(x) for x in ebf4arr[0:4]])
    if magic_str != MAGIC_EBF4:
        raise Exception('Expected magic EBF4, found ' + magic_str)

    w = int.from_bytes(ebf4arr[4:6], 'little')
    h = int.from_bytes(ebf4arr[6:8], 'little')
    palette = ebf4arr[8:24]
    pixdata = ebf4arr[24:]

    img = Image.new('RGB', (w, h))
    uncompdata = []
    
    # Uncompact data
    for compval in pixdata:
        hinib = (compval & 0xF0) >> 4
        lonib = (compval & 0x0F)
        uncompdata.append(hinib)
        uncompdata.append(lonib)

    stride = w
    if (w & 1):
        stride += 1

    for y in range(h):
        for x in range(w):
            bmpidx = x + y * stride
            palidx = uncompdata[bmpidx]
            pixel222 = palette[palidx]
            pixel888 = rgb888_from_rgb222(pixel222)
            img.putpixel((x, y), pixel888)

    img.save(ofn)
    print(f'Converted {ifn} to {ofn}.')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: python convert_ebf_to_png.py input-file.ebf8 output-file.png')
        print('Or: python convert_ebf_to_png.py input-file.ebf4 output-file.png')
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    if input_file.endswith('.ebf8'):
        ebf8file_to_pngfile(input_file, output_file)
    elif input_file.endswith('.ebf4'):
        ebf4file_to_pngfile(input_file, output_file)
    else:
        print('File must be either .ebf8 or .ebf4')

