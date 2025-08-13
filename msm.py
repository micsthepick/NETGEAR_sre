import sys
import struct

def parse_msmptbl(data):
    entry_size = 28
    num_entries = len(data) // entry_size

    print('dev:"name"')
    for i in range(num_entries):
        entry = data[i*entry_size:(i+1)*entry_size]
        name = entry[0:16].rstrip(b'\x00').decode('ascii', errors='ignore')
        if name:
            print(f'mtd{i}:"{name}"')

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] != '-':
        with open(sys.argv[1], 'rb') as f:
            data = f.read()
    else:
        data = sys.stdin.buffer.read()

    parse_msmptbl(data)
