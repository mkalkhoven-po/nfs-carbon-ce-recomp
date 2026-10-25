"""Find direct callers (bl / b) of guest functions.

Usage: python tools/re/callers.py <image.bin> <init.cpp> <addr> [<addr> ...]
"""
import bisect
import re
import struct
import sys

BASE = 0x82000000
CODE_START, CODE_END = 0x82110000, 0x82B35A64


def main():
    image, init_cpp, *targets = sys.argv[1:]
    data = open(image, "rb").read()
    targets = {int(t, 16) for t in targets}
    starts = sorted(int(m.group(1), 16) for m in
                    re.finditer(r"\{ 0x(8[0-9A-F]{7}), ", open(init_cpp).read()))
    for site in range(CODE_START, CODE_END, 4):
        ins = struct.unpack_from(">I", data, site - BASE)[0]
        if ins >> 26 != 18 or ins & 2:  # b/bl with relative target only
            continue
        off = ins & 0x03FFFFFC
        if off & 0x02000000:
            off -= 0x04000000
        target = (site + off) & 0xFFFFFFFF
        if target in targets:
            func = starts[bisect.bisect_right(starts, site) - 1]
            kind = "bl" if ins & 1 else "b "
            print(f"{target:08X} <- {kind} {site:08X} in sub_{func:08X}")


if __name__ == "__main__":
    main()
