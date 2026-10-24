"""Minimal XDVDFS (Xbox / Xbox 360 disc) reader.

Usage:
  python tools/xdvdfs.py list    <image.iso>
  python tools/xdvdfs.py extract <image.iso> <out_dir> [path ...]

Without paths, `extract` extracts the whole game partition.
"""
import os
import struct
import sys

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"
# Game partition offsets: raw/extracted, XGD2 (360), XGD1 (Xbox), XGD3 (360)
PARTITION_OFFSETS = (0, 0xFD90000, 0x18300000, 0x2080000)
ATTR_DIRECTORY = 0x10


class Image:
    def __init__(self, path):
        self.f = open(path, "rb")
        for base in PARTITION_OFFSETS:
            self.f.seek(base + 32 * SECTOR)
            if self.f.read(20) == MAGIC:
                self.base = base
                break
        else:
            raise ValueError("no XDVDFS volume found")
        self.f.seek(self.base + 32 * SECTOR + 20)
        self.root_sector, self.root_size = struct.unpack("<II", self.f.read(8))

    def read(self, sector, size):
        self.f.seek(self.base + sector * SECTOR)
        return self.f.read(size)

    def walk(self, sector=None, size=None, prefix=""):
        """Yield (path, sector, size, is_dir) for every entry, depth first."""
        if sector is None:
            sector, size = self.root_sector, self.root_size
        if size == 0:
            return
        table = self.read(sector, size)
        stack, seen = [0], set()
        while stack:
            off = stack.pop()
            if off in seen or off + 14 > len(table):
                continue
            seen.add(off)
            left, right, start, length, attr, name_len = struct.unpack_from("<HHIIBB", table, off)
            if left == 0xFFFF and right == 0xFFFF:
                continue
            name = table[off + 14:off + 14 + name_len].decode("ascii", "replace")
            path = prefix + name
            is_dir = bool(attr & ATTR_DIRECTORY)
            yield path, start, length, is_dir
            if is_dir:
                yield from self.walk(start, length, path + "/")
            if right:
                stack.append(right * 4)
            if left:
                stack.append(left * 4)

    def extract(self, sector, size, dest):
        os.makedirs(os.path.dirname(dest) or ".", exist_ok=True)
        self.f.seek(self.base + sector * SECTOR)
        with open(dest, "wb") as out:
            remaining = size
            while remaining:
                chunk = self.f.read(min(remaining, 1 << 22))
                if not chunk:
                    raise IOError(f"unexpected end of image while reading {dest}")
                out.write(chunk)
                remaining -= len(chunk)


def main(argv):
    if len(argv) < 2 or argv[0] not in ("list", "extract"):
        print(__doc__)
        return 1
    img = Image(argv[1])
    entries = sorted(img.walk(), key=lambda e: e[0].lower())
    if argv[0] == "list":
        print(f"partition at 0x{img.base:X}")
        for path, _, size, is_dir in entries:
            print(f"{'<DIR>' if is_dir else size:>12}  {path}")
        return 0

    out_dir = argv[2]
    wanted = {p.replace("\\", "/").lower() for p in argv[3:]}
    for path, sector, size, is_dir in entries:
        if is_dir or (wanted and path.lower() not in wanted):
            continue
        img.extract(sector, size, os.path.join(out_dir, path))
        print(f"extracted {path} ({size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
