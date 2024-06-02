import sys

def write_header(data, filename):
    with open(filename, 'w') as f:
        f.write("#ifndef FONT_H\n")
        f.write("#define FONT_H\n\n")
        f.write("const unsigned char _mainfont_data[] = {")
        for i, byte in enumerate(data):
            f.write(hex(byte) + ",")
            if (i + 1) % 12 == 0:
                f.write("\n")
        f.write("\n};\n")
        f.write("const unsigned int _mainfont_size = {};\n\n".format(len(data)))
        f.write("#endif // FONT_H\n")

def main():
    if len(sys.argv) != 3:
        print("Usage: {} <input.ttf> <output.h>".format(sys.argv[0]))
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    try:
        with open(input_file, 'rb') as f:
            data = f.read()
    except IOError:
        print("Error opening input file.")
        sys.exit(1)

    write_header(data, output_file)

if __name__ == "__main__":
    main()
