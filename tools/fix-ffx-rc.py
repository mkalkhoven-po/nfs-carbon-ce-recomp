"""Convert the FidelityFX SDK's UTF-16 resource script to UTF-8.

llvm-rc (via the clang preprocessor) cannot read UTF-16 sources, so building the
SDK with REXGLUE_ENABLE_FIDELITYFX fails on ffx_api_dll.rc. Run this once after
the SDK's CMake configure has fetched FidelityFX.

Usage: python tools/fix-ffx-rc.py
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
RC = ROOT / "sdk/rexglue-sdk/out/build/win-amd64/_deps/fidelityfx-src/ffx-api/src/resource/ffx_api_dll.rc"

if not RC.exists():
    sys.exit(f"not found (configure the SDK with FidelityFX first): {RC}")
raw = RC.read_bytes()
if raw[:2] != b"\xff\xfe":
    print("already UTF-8, nothing to do")
else:
    RC.write_bytes(raw.decode("utf-16").encode("utf-8"))
    print(f"converted {RC.name} to UTF-8")
