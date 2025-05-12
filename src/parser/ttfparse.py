import sys

def write_header(data, filename, headername):
    with open(filename, 'w') as f:
        f.write("#ifndef "+ headername.upper() +"_FONT_H\n")
        f.write("#define "+ headername.upper() +"_FONT_H\n\n")
        f.write("const unsigned char " + headername + "_font_data[] = {")
        for i, byte in enumerate(data):
            f.write(hex(byte) + ",")
            if (i + 1) % 12 == 0:
                f.write("\n")
        f.write("\n};\n")
        f.write("const unsigned int " + headername + "_font_size = {};\n\n".format(len(data)))
        f.write("#endif // FONT_H\n")

def main():
    if len(sys.argv) != 4:
        print("Usage: {} <input.ttf> <output.h> <headername>".format(sys.argv[0]))
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    headername = sys.argv[3]

    try:
        with open(input_file, 'rb') as f:
            data = f.read()
    except IOError:
        print("Error opening input file.")
        sys.exit(1)

    write_header(data, output_file, headername)

if __name__ == "__main__":
    main()
