import sys
import os

def write_header(filename):
    with open(filename, 'w') as f:
        f.write("#ifndef MAINFONT_FONT_H\n")
        f.write("#define MAINFONT_FONT_H\n\n")
        f.write("#ifdef __cplusplus\n")
        f.write('extern "C" {\n')
        f.write("#endif\n\n")

        f.write("extern const unsigned char * const _mainfont_data;\n")
        f.write("extern const unsigned int _mainfont_size;\n\n")

        f.write("#ifdef __cplusplus\n")
        f.write('}\n')
        f.write("#endif\n\n")

        f.write("#endif // MAINFONT_FONT_H\n")

def write_source(data, filename):
    with open(filename, 'w') as f:
        f.write('#include "main_font.h"\n\n')
        f.write("const unsigned char _mainfont_data_array[] = {\n")
        for i, byte in enumerate(data):
            f.write(f"0x{byte:02x}, ")
            if (i + 1) % 12 == 0:
                f.write("\n")
        f.write("\n};\n\n")
        f.write("const unsigned char * const _mainfont_data = _mainfont_data_array;\n")
        f.write(f"const unsigned int _mainfont_size = {len(data)};\n")

def main():
    if len(sys.argv) != 3:
        print("Usage: {} <input.ttf> <output>".format(sys.argv[0]))
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    header_file = output_file + ".h"
    source_file = output_file + ".c"

    try:
        with open(input_file, 'rb') as f:
            data = f.read()
    except IOError:
        print("Error opening input file.")
        sys.exit(1)

    write_header(header_file)
    write_source(data, source_file)
    print(f"Generated: {header_file}, {source_file}")

if __name__ == "__main__":
    main()
