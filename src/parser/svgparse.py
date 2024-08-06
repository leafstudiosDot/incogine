import os
import sys

def svg_to_hex_array(svg_file):
    with open(svg_file, 'rb') as f:
        content = f.read()

    hex_array = ', '.join(f'0x{byte:02x}' for byte in content)
    size = len(content)
    return hex_array, size

def generate_c_header(svg_file):
    file_name = os.path.splitext(os.path.basename(svg_file))[0]
    hex_array, size = svg_to_hex_array(svg_file)

    header_content = f"""
        #ifndef {file_name.upper()}_DATA_H
        #define {file_name.upper()}_DATA_H

        const unsigned char {file_name}_data[] = {{
            {hex_array}
        }};

        const unsigned int {file_name}_size = {size};

        #endif // {file_name.upper()}_DATA_H
        """

    header_file = f"{file_name}_data.h"
    with open(header_file, 'w') as f:
        f.write(header_content)

    print(f"Generated {header_file}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 svgparse.py <svg_files>")
        sys.exit(1)

    svg_files = sys.argv[1:]
    for svg_file in svg_files:
        generate_c_header(svg_file)
