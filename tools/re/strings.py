"""List printable ASCII strings in a dumped guest image, with guest addresses.

Usage: python tools/re/strings.py <image.bin> [regex] [--base 0x82000000] [--min 4]
"""
import argparse
import re


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("pattern", nargs="?", default=None)
    ap.add_argument("--base", type=lambda v: int(v, 0), default=0x82000000)
    ap.add_argument("--min", type=int, default=4)
    args = ap.parse_args()

    data = open(args.image, "rb").read()
    rx = re.compile(args.pattern, re.I) if args.pattern else None
    for m in re.finditer(rb"[\x20-\x7e]{%d,}" % args.min, data):
        s = m.group().decode("ascii")
        if rx is None or rx.search(s):
            print(f"{args.base + m.start():08X}  {s}")


if __name__ == "__main__":
    main()
