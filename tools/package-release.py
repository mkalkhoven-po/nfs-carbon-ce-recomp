"""Put the release build together as dist/<name>/ and dist/<name>.zip.

The zip holds the executable, the SDK runtime DLLs, README, release notes and license texts.
No game files: players supply their own disc image or extracted disc.

Usage (after building nfsc with the win-amd64-release preset):
    python tools/package-release.py [--version 0.752]
"""

import argparse
import re
import shutil
import sys
import zipfile
from pathlib import Path

DEFAULT_VERSION = "0.752"

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "nfsc" / "out" / "build" / "win-amd64-release"
SDK = ROOT / "sdk" / "rexglue-sdk"

BINARIES = ["nfsc.exe", "rexruntime.dll", "rexgpu-xenos.dll", "amd_fidelityfx_vk.dll"]
LICENSE_NAME = re.compile(r"^(LICENSE|LICENCE|COPYING)", re.IGNORECASE)
# Licenses that aren't at the top of their folder.
EXTRA_LICENSES = {
    "libmspack.txt": "thirdparty/libmspack/libmspack/COPYING.LIB",
    "fidelityfx.md": "out/build/win-amd64/_deps/fidelityfx-src/docs/license.md",
}


def fail(message: str) -> None:
    sys.exit(f"package-release: {message}")


def check_optimized() -> None:
    # A failed configure once left the release build at -O0 (see the ROADMAP); refuse that.
    cache = (BUILD / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
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

    for binary in BINARIES:
        shutil.copy2(BUILD / binary, out / binary)
    shutil.copy2(ROOT / "README.md", out / "README.md")
    shutil.copy2(ROOT / "LICENSE", out / "LICENSE.txt")
    shutil.copy2(notes, out / "RELEASE-NOTES.md")

    # License texts: the SDK and the third-party libraries it builds in.
    shutil.copy2(SDK / "LICENSE", out / "licenses" / "rexglue-sdk.txt")
    for folder in sorted(p for p in (SDK / "thirdparty").iterdir() if p.is_dir()):
        licenses = sorted(f for f in folder.iterdir() if f.is_file() and LICENSE_NAME.match(f.name))
        if licenses:
            shutil.copy2(licenses[0], out / "licenses" / f"{folder.name}.txt")
    for target, source in EXTRA_LICENSES.items():
        path = SDK / source
        if not path.is_file():
            fail(f"license text not found: {path}")
        shutil.copy2(path, out / "licenses" / target)

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
