import sys
import os

def write_header(header_filename, headername):
    with open(header_filename, 'w') as f:
        macro = headername.upper() + "_FONT_H"
        f.write(f"#ifndef {macro}\n")
        f.write(f"#define {macro}\n\n")

        f.write("#ifdef __cplusplus\n")
        f.write('extern "C" {\n')
        f.write("#endif\n\n")

        f.write(f"extern const unsigned char * const {headername}_font_data;\n")
        f.write(f"extern const unsigned int {headername}_font_size;\n\n")

        f.write("#ifdef __cplusplus\n")
        f.write('}\n')
        f.write("#endif\n\n")

        f.write(f"#endif // {macro}\n")

def write_source(data, source_filename, headername, lastput):
    with open(source_filename, 'w') as f:
        f.write(f'#include "{lastput}.h"\n\n')
        f.write(f"const unsigned char {headername}_font_data_array[] = {{\n")
        for i, byte in enumerate(data):
            f.write(f"0x{byte:02x}, ")
            if (i + 1) % 12 == 0:
                f.write("\n")
        f.write("\n};\n\n")
        f.write(f"const unsigned char * const {headername}_font_data = {headername}_font_data_array;\n")
        f.write(f"const unsigned int {headername}_font_size = {len(data)};\n")

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input.ttf> <output_basename> <headername>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_basename = sys.argv[2]
    headername = sys.argv[3]

    try:
        with open(input_file, 'rb') as f:
            data = f.read()
    except IOError:
        print("Error opening input file.")
        sys.exit(1)

    header_file = output_basename + ".h"
    source_file = output_basename + ".c"

    write_header(header_file, headername)

    lastput = os.path.splitext(os.path.basename(output_basename))[0]
    write_source(data, source_file, headername, lastput)

    print(f"Generated: {header_file}, {source_file}")

if __name__ == "__main__":
    main()
