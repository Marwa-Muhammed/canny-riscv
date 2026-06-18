#!/usr/bin/env python3
"""
Convert a raw grayscale image into a C/C++ header containing a byte array
plus its width/height, so it can be compiled directly into main.cpp instead
of being read with fopen() at runtime.

This exists because riscv64-unknown-elf-g++ (bare-metal Newlib) has no
working file-open syscalls under QEMU -- confirmed directly with a minimal
fopen() test that failed even on a file proven to exist at the given path.

Output defines (default array name EMBEDDED_IMAGE, matches main.cpp):
    EMBEDDED_IMAGE_WIDTH
    EMBEDDED_IMAGE_HEIGHT
    EMBEDDED_IMAGE_LEN
    EMBEDDED_IMAGE[]   (uint8_t array)

Usage: raw_to_header.py <input.raw> <output.h> <width> <height> [array_name]
"""
import sys


def raw_to_header(raw_path, header_path, width, height, array_name="EMBEDDED_IMAGE"):
    with open(raw_path, "rb") as f:
        data = f.read()

    expected = width * height
    if len(data) != expected:
        print(f"WARNING: {raw_path} is {len(data)} bytes, but "
              f"{width}x{height} = {expected} bytes expected. "
              f"main.cpp's sanity check will catch this at runtime too.")

    with open(header_path, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <cstdint>\n\n")
        f.write(f"// Auto-generated from {raw_path}. Do not edit by hand.\n")
        f.write(f"// Regenerate: python3 scripts/raw_to_header.py {raw_path} "
                f"{header_path} {width} {height} {array_name}\n\n")
        f.write(f"static const unsigned int {array_name}_WIDTH  = {width}u;\n")
        f.write(f"static const unsigned int {array_name}_HEIGHT = {height}u;\n")
        f.write(f"static const unsigned int {array_name}_LEN    = {len(data)}u;\n\n")
        f.write(f"static const uint8_t {array_name}[] = {{\n")
        for i in range(0, len(data), 16):
            chunk = data[i:i + 16]
            line = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"    {line},\n")
        f.write("};\n")

    return len(data)


if __name__ == "__main__":
    if len(sys.argv) < 5:
        print(f"Usage: {sys.argv[0]} <input.raw> <output.h> <width> <height> [array_name]")
        sys.exit(1)
    raw_path, header_path = sys.argv[1], sys.argv[2]
    width, height = int(sys.argv[3]), int(sys.argv[4])
    array_name = sys.argv[5] if len(sys.argv) > 5 else "EMBEDDED_IMAGE"
    n = raw_to_header(raw_path, header_path, width, height, array_name)
    print(f"Wrote {header_path}: {array_name}[{n} bytes], "
          f"{array_name}_WIDTH={width}, {array_name}_HEIGHT={height}")
