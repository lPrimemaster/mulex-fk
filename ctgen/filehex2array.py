import sys


def produce_hex_array(input):
    with open(input, 'rb') as f:
        data = f.read()

    hex_array = [f'0x{b:02X}' for b in data]
    return ', '.join(hex_array)


if __name__ == '__main__':
    if (len(sys.argv) == 2):
        print(produce_hex_array(sys.argv[1]))
