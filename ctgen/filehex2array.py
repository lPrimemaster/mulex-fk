import sys


def produce_hex_array(input):
    output = ''
    for i in range(len(input) // 2):
        output += '0x'
        output += input[2 * i] + input[2 * i + 1]
        output += ', '
    return output


if __name__ == '__main__':
    if (len(sys.argv) == 2):
        print(produce_hex_array(sys.argv[1]))
