import sys
import re

def text_to_bin(input_file, output_file):
    with open(input_file, "r") as f:
        # Leer el contenido del archivo
        content = f.read()

    # Extraer los números hexadecimales usando una expresión regular
    hex_numbers = re.findall(r'0x([0-9A-Fa-f]+)', content)

    with open(output_file, "wb") as f:
        for hex_number in hex_numbers:
            # Convertir cada número hexadecimal a un byte y escribirlo en el archivo
            f.write(int(hex_number, 16).to_bytes(1, byteorder='big'))

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Uso: python text_to_bin.py input_file.txt output_file.bin")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    text_to_bin(input_file, output_file)
    print(f"Archivo {output_file} creado con éxito.")

