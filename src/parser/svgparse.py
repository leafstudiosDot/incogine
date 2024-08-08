import sys
import os
import cairosvg
import base64

def parse(svg_file, output_header):
    image_name = os.path.splitext(os.path.basename(svg_file))[0].replace(' ', '_')
    
    png_file = image_name + ".png"
    cairosvg.svg2png(url=svg_file, write_to=png_file)
    
    with open(png_file, "rb") as f:
        png_data = f.read()

    array_content = ', '.join(f'0x{byte:02x}' for byte in png_data)

    with open(output_header, "w") as f:
        f.write("#ifndef PNG_IMAGE_H\n")
        f.write("#define PNG_IMAGE_H\n\n")
        f.write(f"const unsigned char {image_name}_data[] = {{\n    {array_content}\n}};\n\n")
        f.write(f"const unsigned int {image_name}_size = {len(png_data)};\n")
        f.write("#endif // PNG_IMAGE_H\n")

    os.remove(png_file)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 svgparse.py <input.svg> <output.h>")
        sys.exit(1)
    
    svg_file = sys.argv[1]
    output_header = sys.argv[2]
    
    parse(svg_file, output_header)
