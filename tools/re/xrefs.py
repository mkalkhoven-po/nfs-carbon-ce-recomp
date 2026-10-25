"""Find code that references guest addresses (lis + addi/ori/load/store pairs).

Usage: python tools/re/xrefs.py <image.bin> <init.cpp> <addr> [<addr> ...]
  init.cpp: generated nfsc_init.cpp, used to map code addresses to functions.
"""
import bisect
import re
import struct
import sys

BASE = 0x82000000
CODE_START, CODE_END = 0x82110000, 0x82B35A64
# D-form opcodes whose rA + SIMM forms an address: addi, lwz, lwzu, lbz, lbzu,
# stw, stwu, stb, stbu, lhz, lhzu, lha, lhau, sth, sthu, lmw, stmw, lfs, lfsu,
# lfd, lfdu, stfs, stfsu, stfd, stfdu
ADDR_OPS = {14, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
            48, 49, 50, 51, 52, 53, 54, 55}


def sext16(v):
    return v - 0x10000 if v & 0x8000 else v


def scan(data):
    """Yield (site, target) for every resolved lis-based address."""
    lis = {}  # reg -> (value, site)
    for site in range(CODE_START, CODE_END, 4):
        ins = struct.unpack_from(">I", data, site - BASE)[0]
        op, rd, ra, imm = ins >> 26, (ins >> 21) & 31, (ins >> 16) & 31, ins & 0xFFFF
        if op == 15 and ra == 0:  # lis rd, imm
            lis[rd] = ((imm << 16) & 0xFFFFFFFF, site)
            continue
        if op in ADDR_OPS and ra in lis and site - lis[ra][1] <= 64:
            yield site, (lis[ra][0] + sext16(imm)) & 0xFFFFFFFF
            if op == 14 and rd == ra:
                lis.pop(ra)
        elif op == 24 and rd in lis and site - lis[rd][1] <= 64:  # ori ra, rs, imm (rs=rd field)
            yield site, lis[rd][0] | imm
        # Branches end the tracking window.
        if op in (16, 18, 19):
            lis.clear()


def functions(init_cpp):
    starts = sorted(int(m.group(1), 16) for m in
                    re.finditer(r"\{ 0x(8[0-9A-F]{7}), sub_|\{ 0x(8[0-9A-F]{7}), ", open(init_cpp).read())
                    if m.group(1))
    return starts


def main():
    image, init_cpp, *targets = sys.argv[1:]
    data = open(image, "rb").read()
    targets = {int(t, 16) for t in targets}
    starts = functions(init_cpp)
    for site, target in scan(data):
        if target in targets:
            i = bisect.bisect_right(starts, site) - 1
            func = starts[i] if i >= 0 else 0
            print(f"{target:08X} <- {site:08X} in sub_{func:08X}")


if __name__ == "__main__":
    main()
