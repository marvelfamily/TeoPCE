#!/usr/bin/env python3
"""Build optional catalog-bound whole-cheat-group availability (TPQ1)."""
import argparse
from pathlib import Path
import struct
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import prepare_cheat_groups as groups
HEADER = struct.Struct('<4sBBHII')
ROW = struct.Struct('<IHH')

def build_index(catalog, launch, packages):
    records, generation = groups.launch.parse_catalog(catalog)
    identities = groups._launch_rows(launch, records, generation)
    known = {f"{row['key']:08X}.tcg" for row in records}
    if set(packages) - known:
        raise ValueError('unknown package key')
    output = bytearray(HEADER.pack(b'TPQ1', 1, ROW.size, len(records), generation, 0))
    for row in records:
        available = held = 0
        data = packages.get(f"{row['key']:08X}.tcg")
        if data is not None:
            if row['kind'] not in (1, 2) or not groups.launch.launchable(row):
                raise ValueError('unsupported package media')
            decoded = groups.codec.decode_package(data, row['key'], *identities[row['path']])
            for group in decoded:
                if group['reason']:
                    held += 1
                else:
                    if row['kind'] == 2 and any(flags != 2 for address, value, flags in group['codes']):
                        raise ValueError('CD groups must use supported RAM writes')
                    available += 1
        output.extend(ROW.pack(row['key'], available, held))
    return bytes(output)

def parse_index(data, catalog):
    records, generation = groups.launch.parse_catalog(catalog)
    if len(data) != HEADER.size + len(records) * ROW.size or HEADER.unpack_from(data) != (b'TPQ1', 1, ROW.size, len(records), generation, 0):
        raise ValueError('cheat index header or length mismatch')
    result = []
    for row in records:
        key, available, held = ROW.unpack_from(data, HEADER.size + row['index'] * ROW.size)
        if key != row['key'] or available + held > 1024:
            raise ValueError('cheat index key or count mismatch')
        result.append((available, held))
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('catalog', 'launch', 'packages', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    data = build_index(args.catalog.read_bytes(), args.launch.read_bytes(), {p.name:p.read_bytes() for p in args.packages.glob('*.tcg')})
    parse_index(data, args.catalog.read_bytes())
    with args.output.open('xb') as stream:
        stream.write(data)
if __name__ == '__main__':
    main()
