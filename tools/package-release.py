"""Put the release build together as dist/<name>/ and dist/<name>.zip.

The zip holds the executable, the SDK runtime DLLs, README, release notes and license texts.
No game files: players supply their own disc image or extracted disc.

Usage (after building nfsc with the win-amd64-release preset):
    python tools/package-release.py [--version 0.76]
"""

import argparse
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

DEFAULT_VERSION = "0.76"

ROOT = Path(__file__).resolve().parent.parent
BUILD_TREE = ROOT / "nfsc" / "out" / "build" / "win-amd64"
BUILD = BUILD_TREE / "Release"
SDK = ROOT / "sdk" / "rexglue-sdk"

# nfsc.exe and every DLL the build copies next to it (the SDK modules, SDL3, Tracy).
LICENSE_NAME = re.compile(r"^(LICENSE|LICENCE|COPYING)", re.IGNORECASE)
# Licenses that aren't at the top of their folder.
EXTRA_LICENSES = {
    "libmspack.txt": "thirdparty/libmspack/libmspack/COPYING.LIB",
}


def fail(message: str) -> None:
    sys.exit(f"package-release: {message}")


def sdk_thirdparty_folders() -> list[Path]:
    # The folders of the SDK version that is checked out. A checkout updated from an older SDK
    # keeps the folders of submodules that were removed since; their libraries aren't built in.
    listing = subprocess.run(
        ["git", "-c", "safe.directory=*", "ls-tree", "--name-only", "HEAD", "thirdparty/"],
        cwd=SDK, capture_output=True, text=True, check=True).stdout
    return [SDK / line for line in listing.splitlines() if (SDK / line).is_dir()]


def vma_license() -> str:
    # SDK v0.11 builds in the Vulkan Memory Allocator header from the Vulkan SDK; its license is
    # the comment at the top of the header.
    cache = (SDK / "out" / "build" / "win-amd64" / "CMakeCache.txt").read_text(
        encoding="utf-8", errors="replace")
    match = re.search(r"^REXGLUE_VMA_INCLUDE_DIR:PATH=(.*)$", cache, re.MULTILINE)
    header = Path(match.group(1).strip()) / "vk_mem_alloc.h" if match else None
    if not header or not header.is_file():
        fail("vk_mem_alloc.h not found through the SDK's REXGLUE_VMA_INCLUDE_DIR")
    lines = []
    for line in header.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.startswith("//"):
            break
        lines.append(line[3:] if line.startswith("// ") else line[2:])
    text = "\n".join(lines).strip()
    if "Permission is hereby granted" not in text:
        fail(f"no license text at the top of {header}")
    return "Vulkan Memory Allocator\n\n" + text + "\n"


def tlsf_license() -> str:
    # TLSF (the SDK's guest memory allocator) has its BSD license in the first comment of tlsf.h.
    header = SDK / "thirdparty" / "tlsf" / "tlsf.h"
    text = header.read_text(encoding="utf-8", errors="replace")
    start, end = text.find("/*"), text.find("*/")
    if start < 0 or end < 0 or "Redistribution and use" not in text[start:end]:
        fail(f"no license text at the top of {header}")
    lines = [line.removeprefix("**").removeprefix(" ").rstrip()
             for line in text[start + 2:end].splitlines()]
    return "\n".join(lines).strip() + "\n"


def check_optimized() -> None:
    # A failed configure once left the release build at -O0 (see the ROADMAP); refuse that.
    cache = (BUILD_TREE / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^CMAKE_CXX_FLAGS_RELEASE:STRING=(.*)$", cache, re.MULTILINE)
    if not match or "-O3" not in match.group(1):
        fail("the release build is not optimized (CMAKE_CXX_FLAGS_RELEASE has no -O3)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", default=DEFAULT_VERSION)
    version = parser.parse_args().version

    check_optimized()
    notes = ROOT / "docs" / f"release-notes-v{version}.md"
    if not notes.is_file():
        fail(f"no release notes at {notes}")

    name = f"NFS-Carbon-CE-Recompiled-v{version}"
    out = ROOT / "dist" / name
    if out.exists():
        shutil.rmtree(out)
    (out / "licenses").mkdir(parents=True)

    binaries = [BUILD / "nfsc.exe"] + sorted(BUILD.glob("*.dll"))
    for binary in binaries:
        shutil.copy2(binary, out / binary.name)
    shutil.copy2(ROOT / "README.md", out / "README.md")
    shutil.copy2(ROOT / "LICENSE", out / "LICENSE.txt")
    shutil.copy2(notes, out / "RELEASE-NOTES.md")

    # License texts: the SDK and the third-party libraries it builds in.
    shutil.copy2(SDK / "LICENSE", out / "licenses" / "rexglue-sdk.txt")
    for folder in sdk_thirdparty_folders():
        licenses = sorted(f for f in folder.iterdir() if f.is_file() and LICENSE_NAME.match(f.name))
        if licenses:
            shutil.copy2(licenses[0], out / "licenses" / f"{folder.name}.txt")
    for target, source in EXTRA_LICENSES.items():
        path = SDK / source
        if not path.is_file():
            fail(f"license text not found: {path}")
        shutil.copy2(path, out / "licenses" / target)
    (out / "licenses" / "vulkan-memory-allocator.txt").write_text(vma_license(), encoding="utf-8")
    (out / "licenses" / "tlsf.txt").write_text(tlsf_license(), encoding="utf-8")

    zip_path = out.parent / f"{name}.zip"  # not with_suffix: the version has dots
    zip_path.unlink(missing_ok=True)
    files = sorted(f for f in out.rglob("*") if f.is_file())
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for file in files:
            archive.write(file, file.relative_to(out).as_posix())
    size = sum(f.stat().st_size for f in files) / (1024 * 1024)
    print(f"{len(files)} files, {size:.1f} MB -> {zip_path}")


if __name__ == "__main__":
    main()
