#!/usr/bin/env python3
"""
Decode the hex-dump blocks printed by main.cpp's dump_image_hex() -- captured
from QEMU's stdout -- back into real .raw binary files.

Looks for blocks of exactly this form (as printed by main.cpp):

    === DUMP_START name=<name> width=<w> height=<h> ===
    <hex bytes, wrapped>
    === DUMP_END ===

Usage: decode_dump.py <qemu_output.log> [out_dir]
"""
import sys
import re
import os

PATTERN = re.compile(
    r"=== DUMP_START name=(\S+) width=(\d+) height=(\d+) ===\n(.*?)\n=== DUMP_END ===",
    re.DOTALL,
)


def decode(log_path, out_dir="."):
    with open(log_path, "r") as f:
        content = f.read()

    matches = PATTERN.findall(content)
    if not matches:
        print("No dump blocks found in log. Did the program actually run "
              "(check for the 'Embedded image: WxH' line), and was stdout "
              "redirected to this file?")
        return

    os.makedirs(out_dir, exist_ok=True)
    for name, width_s, height_s, hexblock in matches:
        width, height = int(width_s), int(height_s)
        hexstr = re.sub(r"\s+", "", hexblock)
        data = bytes.fromhex(hexstr)
        expected = width * height
        if len(data) != expected:
            print(f"WARNING: {name}: decoded {len(data)} bytes, "
                  f"expected {expected} ({width}x{height})")
        out_path = os.path.join(out_dir, f"{name}.raw")
        with open(out_path, "wb") as out:
            out.write(data)
        print(f"Wrote {out_path} ({len(data)} bytes, {width}x{height})")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <qemu_output.log> [out_dir]")
        sys.exit(1)
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "."
    decode(sys.argv[1], out_dir)
