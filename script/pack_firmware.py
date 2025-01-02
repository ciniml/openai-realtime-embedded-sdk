#!/usr/bin/env python
# Pack ESP32 firmware binary for burn with M5Burner

import re
import pathlib
import sys

target_pattern = re.compile(r'^(0x[0-9a-fA-F]{1,8})\s+([\w\.\/-]+)')

targets = []
with open('build/flash_args') as f:
    for line in iter(f.readline, ''):
        m = target_pattern.match(line)
        if m:
            start_address = int(m.group(1), 16)
            path = m.group(2)
            targets.append((start_address, path))

targets.sort(key=lambda x: x[0])
print(targets)

output_path = sys.argv[1] if len(sys.argv) > 1 else 'firmware.bin'

with open(output_path, 'wb') as f:
    current_address = 0
    for start_address, path in targets:
        if current_address < start_address:
            data = b'\xff' * (start_address - current_address)
            bytes_written = 0
            while bytes_written < len(data):
                bytes_written += f.write(data[bytes_written:])

        current_address = start_address
        bin_path = pathlib.Path('build').joinpath(path)
        with open(bin_path, 'rb') as g:
            while True:
                data = g.read()
                bytes_read = len(data)
                if bytes_read == 0:
                    break
                bytes_written = 0
                while bytes_written < bytes_read:
                    bytes_written += f.write(data[bytes_written:])
                current_address += bytes_read