import sys
import struct

def parse_msmptbl(filename):
    entry_size = 28
    with open(filename, 'rb') as f:
        data = f.read()

    num_entries = len(data) // entry_size

    for i in range(num_entries):
        entry = data[i*entry_size:(i+1)*entry_size]

        # Parse fields
        name = entry[0:16].rstrip(b'\x00').decode('ascii', errors='ignore')
        unk_x = struct.unpack('<I', entry[16:20])[0]
        unk_y = struct.unpack('<I', entry[20:24])[0]
        # flags = struct.unpack('<I', entry[24:28])[0]

        if name:
            print(f'Partition {i=} {name=} {unk_x=} {unk_y=}')

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} msmptbl.bin")
        sys.exit(1)

    parse_msmptbl(sys.argv[1])
