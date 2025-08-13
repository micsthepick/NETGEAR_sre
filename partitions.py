print('dev:"name"')
with open('partitions_from_part_dev.txt', 'r') as f:
    for i, line in enumerate(f):
        out = line.strip()
        #if out.startswith('vol_'):
        #    out = out.replace('vol_', '')
        print(f'mtd{i}:"{out}"')
