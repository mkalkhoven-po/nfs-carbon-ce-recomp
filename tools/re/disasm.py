"""Print the PPC disassembly of a recompiled function from the generated C++.

The generated code carries one comment per guest instruction; labels and
instruction counting give the guest address of each line.

Usage: python tools/re/disasm.py <generated_dir> <sub_XXXXXXXX|0xADDR> [--max N]
"""
import argparse
import pathlib
import re


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gen_dir")
    ap.add_argument("func")
    ap.add_argument("--max", type=int, default=400)
    args = ap.parse_args()

    name = args.func if args.func.startswith("sub_") else f"sub_{int(args.func, 16):08X}"
    base = int(name[4:], 16)
    header = f"DEFINE_REX_FUNC({name})"
    for path in sorted(pathlib.Path(args.gen_dir).glob("*_recomp.*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        start = text.find(header)
        if start < 0:
            continue
        end = text.find("\nDEFINE_REX_FUNC(", start + len(header))
        body = text[start:end if end > 0 else len(text)]
        addr, shown = base, 0
        for line in body.splitlines():
            m = re.match(r"^loc_([0-9A-F]{8}):", line)
            if m:
                addr = int(m.group(1), 16)
                print(f"loc_{addr:08X}:")
                continue
            if line.startswith("\t// "):
                print(f"  {addr:08X}  {line[4:]}")
                addr += 4
                shown += 1
                if shown >= args.max:
                    print("  ...")
                    return
        return
    print(f"{name} not found")


if __name__ == "__main__":
    main()
