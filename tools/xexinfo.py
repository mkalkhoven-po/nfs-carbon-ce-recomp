"""Dump the XEX2 header of an Xbox 360 executable.

Usage: python tools/xexinfo.py <default.xex>
"""
import struct
import sys

HEADER_NAMES = {
    0x000002FF: "RESOURCE_INFO",
    0x000003FF: "FILE_FORMAT_INFO",
    0x000005FF: "DELTA_PATCH_DESCRIPTOR",
    0x00000405: "BASE_REFERENCE",
    0x000080FF: "BOUNDING_PATH",
    0x00008105: "DEVICE_ID",
    0x00010001: "ORIGINAL_BASE_ADDRESS",
    0x00010100: "ENTRY_POINT",
    0x00010201: "IMAGE_BASE_ADDRESS",
    0x000103FF: "IMPORT_LIBRARIES",
    0x00018002: "CHECKSUM_TIMESTAMP",
    0x00018102: "ENABLED_FOR_CALLCAP",
    0x00018200: "ENABLED_FOR_FASTCAP",
    0x000183FF: "ORIGINAL_PE_NAME",
    0x000200FF: "STATIC_LIBRARIES",
    0x00020104: "TLS_INFO",
    0x00020200: "DEFAULT_STACK_SIZE",
    0x00020301: "DEFAULT_FILESYSTEM_CACHE_SIZE",
    0x00020401: "DEFAULT_HEAP_SIZE",
    0x00028002: "PAGE_HEAP_SIZE_AND_FLAGS",
    0x00030000: "SYSTEM_FLAGS",
    0x00040006: "EXECUTION_INFO",
    0x00040201: "TITLE_WORKSPACE_SIZE",
    0x00040310: "GAME_RATINGS",
    0x00040404: "LAN_KEY",
    0x000405FF: "XBOX360_LOGO",
    0x000406FF: "MULTIDISC_MEDIA_IDS",
    0x000407FF: "ALTERNATE_TITLE_IDS",
    0x00040801: "ADDITIONAL_TITLE_MEMORY",
    0x00E10402: "EXPORTS_BY_NAME",
}
ENCRYPTION = {0: "none", 1: "normal (AES)"}
COMPRESSION = {0: "none", 1: "basic", 2: "normal (LZX)", 3: "delta"}


def u16(d, o):
    return struct.unpack_from(">H", d, o)[0]


def u32(d, o):
    return struct.unpack_from(">I", d, o)[0]


def cstr(d, o):
    return d[o:d.index(b"\0", o)].decode("ascii", "replace")


def main(path):
    d = open(path, "rb").read()
    if d[:4] != b"XEX2":
        sys.exit("not an XEX2 file")
    module_flags, pe_offset, _, security_offset, count = struct.unpack_from(">IIIII", d, 4)
    print(f"module flags      0x{module_flags:08X}")
    print(f"PE data offset    0x{pe_offset:X}")

    headers = {}
    for i in range(count):
        key, value = struct.unpack_from(">II", d, 0x18 + i * 8)
        headers[key] = value

    print("\noptional headers:")
    for key, value in headers.items():
        print(f"  {key:08X} {HEADER_NAMES.get(key, '?'):<30} 0x{value:08X}")

    if 0x000003FF in headers:
        o = headers[0x000003FF]
        enc, comp = u16(d, o + 4), u16(d, o + 6)
        print(f"\nencryption        {ENCRYPTION.get(enc, enc)}")
        print(f"compression       {COMPRESSION.get(comp, comp)}")

    if 0x000183FF in headers:
        print(f"original PE name  {cstr(d, headers[0x000183FF] + 4)}")

    if 0x00040006 in headers:
        o = headers[0x00040006]
        media_id, version, base_version, title_id = struct.unpack_from(">IIII", d, o)
        disc, discs = d[o + 18], d[o + 19]

        def ver(v):
            return f"{v >> 28}.{(v >> 24) & 0xF}.{(v >> 8) & 0xFFFF}.{v & 0xFF}"

        print(f"title id          {title_id:08X}")
        print(f"media id          {media_id:08X}")
        print(f"version           {ver(version)} (base {ver(base_version)})")
        print(f"disc              {disc}/{discs}")

    s = security_offset
    image_size, image_flags, load_address = u32(d, s + 4), u32(d, s + 0x10C), u32(d, s + 0x110)
    region, pages = u32(d, s + 0x178), u32(d, s + 0x180)
    print(f"\nload address      0x{load_address:08X}")
    print(f"image size        0x{image_size:X} ({image_size / 1048576:.1f} MiB)")
    print(f"image flags       0x{image_flags:08X}")
    print(f"region            0x{region:08X}")
    print(f"page descriptors  {pages}")
    for key in (0x00010100, 0x00010201, 0x00020200, 0x00020401):
        if key in headers:
            print(f"{HEADER_NAMES[key].lower():<17} 0x{headers[key]:08X}")

    if 0x000200FF in headers:
        o = headers[0x000200FF]
        size = u32(d, o)
        print("\nstatic libraries (XDK build):")
        for e in range(o + 4, o + size, 16):
            name = d[e:e + 8].rstrip(b"\0").decode("ascii", "replace")
            major, minor, build, qfe = struct.unpack_from(">HHHH", d, e + 8)
            print(f"  {name:<10} {major}.{minor}.{build}.{qfe & 0xFF}")

    if 0x000103FF in headers:
        o = headers[0x000103FF]
        size, strings_size, string_count = struct.unpack_from(">III", d, o)
        names, p = [], o + 12
        while len(names) < string_count:
            names.append(cstr(d, p))
            p += len(names[-1]) + 1
            p += (-p) & 3
        p = o + 12 + strings_size
        print("\nimport libraries:")
        total = 0
        while p < o + size:
            lib_size = u32(d, p)
            version = u32(d, p + 0x1C)
            name_index, record_count = u16(d, p + 0x24), u16(d, p + 0x26)
            v = f"{version >> 28}.{(version >> 24) & 0xF}.{(version >> 8) & 0xFFFF}.{version & 0xFF}"
            print(f"  {names[name_index]:<16} v{v:<14} {record_count} import records")
            total += record_count
            p += lib_size
        print(f"  (total {total} records; functions use two records each, variables one)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
