"""Print duration and stream info of ASF/WMV files.

Usage: python tools/asfinfo.py <file.wmv> [...]
"""
import struct
import sys
import uuid

HEADER = uuid.UUID("75B22630-668E-11CF-A6D9-00AA0062CE6C")
FILE_PROPERTIES = uuid.UUID("8CABDCA1-A947-11CF-8EE4-00C00C205365")
STREAM_PROPERTIES = uuid.UUID("B7DC0791-A9B7-11CF-8EE6-00C00C205365")
AUDIO_MEDIA = uuid.UUID("F8699E40-5B4D-11CF-A8FD-00805F5C442B")
VIDEO_MEDIA = uuid.UUID("BC19EFC0-5B4D-11CF-A8FD-00805F5C442B")


def guid(data, offset):
    return uuid.UUID(bytes_le=bytes(data[offset:offset + 16]))


def info(path):
    with open(path, "rb") as f:
        data = f.read(1 << 16)
    if guid(data, 0) != HEADER:
        return f"{path}: not an ASF file"
    count = struct.unpack_from("<I", data, 24)[0]
    offset, streams, duration = 30, [], None
    for _ in range(count):
        object_id = guid(data, offset)
        size = struct.unpack_from("<Q", data, offset + 16)[0]
        if object_id == FILE_PROPERTIES:
            play_duration, _, preroll = struct.unpack_from("<QQQ", data, offset + 64)
            duration = play_duration / 1e7 - preroll / 1000
        elif object_id == STREAM_PROPERTIES:
            kind = guid(data, offset + 24)
            streams.append("audio" if kind == AUDIO_MEDIA else "video" if kind == VIDEO_MEDIA else "other")
        offset += size
    return f"{duration:7.2f} s  {'+'.join(streams):12s} {path}"


if __name__ == "__main__":
    for p in sys.argv[1:]:
        print(info(p))
