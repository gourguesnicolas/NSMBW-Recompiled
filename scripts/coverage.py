#!/usr/bin/env python3
"""Report how much of the REL code the translator reached, and where the walk must be seeded.

Function starts the call-graph walk from __start cannot see on its own come from three places:

- REL relocation tables, which list every call and code pointer crossing a section or module;
- main.dol data sections, which have no relocations but hold the same kind of pointers
  (virtual tables, callbacks);
- the `stwu r1,-N(r1)` + `mflr r0` prologue that opens almost every non-leaf function.

Prints addresses and counts only, never game bytes.

Usage:
  coverage.py <manifest.yml> <generated/functions> [--emit-map FILE]

--emit-map writes every function start found, as `<hex address> <name>` lines, the format of the
manifest's translation.function_map. Names already in the current map are kept.
"""
import re
import struct
import sys
from pathlib import Path

R_PPC_ADDR32, R_PPC_ADDR16_LO, R_PPC_ADDR16_HI, R_PPC_ADDR16_HA = 1, 4, 5, 6
R_PPC_REL24, R_PPC_REL14 = 10, 11
R_DOLPHIN_NOP, R_DOLPHIN_SECTION, R_DOLPHIN_END = 201, 202, 203


def u8(b, o): return b[o]
def u16(b, o): return struct.unpack_from(">H", b, o)[0]
def u32(b, o): return struct.unpack_from(">I", b, o)[0]


def parse_manifest(path):
    """Read the dol path and the rels list. The manifest is simple enough for a line scan."""
    text = Path(path).read_text(encoding="utf-8")
    root = Path(path).parent / re.search(r"^workspace_root:\s*(\S+)", text, re.M).group(1)
    dol = re.search(r"dol:\s*\n\s*path:\s*(\S+)", text).group(1)
    rels = []
    for block in re.split(r"\n\s*- path:", text.split("rels:")[1].split("translation:")[0])[0:]:
        m = re.match(r"\s*(\S+)", block)
        load = re.search(r"load_address:\s*(0x[0-9A-Fa-f]+)", block)
        bss = re.search(r"bss_address:\s*(0x[0-9A-Fa-f]+)", block)
        if m and load:
            rels.append((m.group(1), int(load.group(1), 16), int(bss.group(1), 16) if bss else None))
    fmap = re.search(r"function_map:\s*^\s*path:\s*(\S+)", text, re.M)
    return ((root / dol).resolve(), [((root / p).resolve(), l, b) for p, l, b in rels],
            (root / fmap.group(1)).resolve() if fmap else None)


def dol_sections(data):
    secs = []
    for i in range(18):
        off, addr, size = u32(data, i * 4), u32(data, 0x48 + i * 4), u32(data, 0x90 + i * 4)
        if size:
            secs.append((addr, size, i < 7))  # first seven sections are text
    return secs


class Rel:
    def __init__(self, path, load, bss):
        d = Path(path).read_bytes()
        self.name, self.load, self.data = Path(path).stem, load, d
        self.id = u32(d, 0)
        n, sec_off = u32(d, 0xC), u32(d, 0x10)
        self.imp_off, self.imp_size = u32(d, 0x28), u32(d, 0x2C)
        self.sections = []  # (address, size, executable) per section index
        for i in range(n):
            off, size = u32(d, sec_off + i * 8), u32(d, sec_off + i * 8 + 4)
            exe, off = off & 1, off & ~3
            if size == 0:
                self.sections.append((0, 0, False))
            elif off == 0:  # BSS
                self.sections.append((bss if bss is not None else 0, size, False))
            else:
                self.sections.append((load + off, size, bool(exe)))

    def relocations(self):
        """Yield (target_module, target_section, addend, type, source_address)."""
        d = self.data
        for i in range(self.imp_size // 8):
            module, rel_off = u32(d, self.imp_off + i * 8), u32(d, self.imp_off + i * 8 + 4)
            pos, offset, section = rel_off, 0, 0
            while True:
                skip, kind, sec, addend = u16(d, pos), u8(d, pos + 2), u8(d, pos + 3), u32(d, pos + 4)
                pos += 8
                if kind == R_DOLPHIN_END:
                    break
                if kind == R_DOLPHIN_SECTION:
                    section, offset = sec, 0
                    continue
                offset += skip
                if kind == R_DOLPHIN_NOP:
                    continue
                src = self.sections[section][0] + offset if section < len(self.sections) else 0
                yield module, sec, addend, kind, src


STWU_R1, BLR, BCTR = 0x9421, 0x4E800020, 0x4E800420


class Code:
    """Read instruction words of executable sections, to tell function starts from jump labels."""

    def __init__(self, dol_data, rels):
        self.dol, self.rels = dol_data, rels
        self.dol_secs = [(u32(dol_data, i * 4), a, n) for i in range(7)
                         for a, n in [(u32(dol_data, 0x48 + i * 4), u32(dol_data, 0x90 + i * 4))] if n]

    def executable_ranges(self):
        for off, addr, size in self.dol_secs:
            yield "main.dol", addr, size, self.dol, off
        for r in self.rels.values():
            for s, n, exe in r.sections:
                if exe:
                    yield r.name, s, n, r.data, s - r.load

    def word(self, a):
        for off, addr, size in self.dol_secs:
            if addr <= a < addr + size:
                return u32(self.dol, off + a - addr)
        for r in self.rels.values():
            for s, n, exe in r.sections:
                if exe and s <= a < s + n:
                    return u32(r.data, a - r.load)
        return None

    def looks_like_function_start(self, a):
        """Pointer relocations also hit jump-table labels inside functions. A real start either
        opens with `stwu r1,...` or follows padding, a return, an indirect jump or a plain branch."""
        w, prev = self.word(a), self.word(a - 4)
        if w is None:
            return False
        if w >> 16 == STWU_R1 or prev is None:
            return True
        return prev == 0 or prev in (BLR, BCTR) or (prev >> 26 == 18 and not prev & 1)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    emit = sys.argv[sys.argv.index("--emit-map") + 1] if "--emit-map" in sys.argv else None
    if emit:
        args.remove(emit)
    if len(args) != 2:
        sys.exit(__doc__)
    manifest, functions_dir = args

    dol_path, rel_specs, map_path = parse_manifest(manifest)
    dol_secs = dol_sections(dol_path.read_bytes())
    rels = {r.id: r for r in (Rel(p, l, b) for p, l, b in rel_specs)}

    def in_dol_text(a):
        return any(s <= a < s + n and exe for s, n, exe in dol_secs)

    def owner(a):
        if in_dol_text(a):
            return "main.dol"
        for r in rels.values():
            if any(s <= a < s + n and exe for s, n, exe in r.sections):
                return r.name
        return None

    translated = {int(m.group(1), 16) for f in Path(functions_dir).iterdir()
                  if (m := re.match(r"func_([0-9A-Fa-f]{8})\.", f.name))}

    code = Code(dol_path.read_bytes(), rels)
    rejected = set()
    candidates = {}  # address -> set of relocation kinds that point at it
    for r in rels.values():
        for module, sec, addend, kind, _src in r.relocations():
            if module == 0:
                target = addend  # main.dol: absolute address, section field unused
            elif module in rels:
                t = rels[module]
                if sec >= len(t.sections):
                    continue
                target = t.sections[sec][0] + addend
            else:
                continue
            if kind not in (R_PPC_REL24, R_PPC_ADDR32) or not owner(target) or target % 4:
                continue
            if kind == R_PPC_ADDR32 and not code.looks_like_function_start(target):
                rejected.add(target)
                continue
            candidates.setdefault(target, set()).add("call" if kind == R_PPC_REL24 else "ptr")
    rejected -= set(candidates)

    # main.dol data sections: pointers into executable code, filtered like the REL pointers.
    dol_data = dol_path.read_bytes()
    for i in range(7, 18):
        off, addr, size = u32(dol_data, i * 4), u32(dol_data, 0x48 + i * 4), u32(dol_data, 0x90 + i * 4)
        for pos in range(off, off + size - 3, 4):
            target = u32(dol_data, pos)
            if (0x80000000 <= target < 0x81800000 and target % 4 == 0 and owner(target)
                    and code.looks_like_function_start(target)):
                candidates.setdefault(target, set()).add("ptr")

    # Prologue scan: an `stwu r1` immediately followed by `mflr r0` opens a function.
    for _name, base, size, data, off in code.executable_ranges():
        for a in range(0, size - 4, 4):
            if u32(data, off + a) >> 16 == STWU_R1 and u32(data, off + a + 4) == 0x7C0802A6:
                candidates.setdefault(base + a, set()).add("prologue")

    print(f"{'code owner':<16}{'candidates':>11}{'translated':>11}{'missing':>9}  (translated in total)")
    missing = {}
    for name in ["main.dol"] + [r.name for r in rels.values()]:
        c = {a for a in candidates if owner(a) == name}
        miss = sorted(c - translated)
        missing[name] = miss
        total = sum(1 for a in translated if owner(a) == name)
        print(f"{name:<16}{len(c):>11}{len(c) - len(miss):>11}{len(miss):>9}  ({total})")

    print(f"pointer targets rejected as jump-table labels or mid-function: {len(rejected)}")

    if emit:
        lines = ["# Function starts for SMNE01 revision 2: addresses and names only, no game data.",
                 "# Generated by scripts/coverage.py --emit-map; hand-written names are kept.",
                 "# Format: <hex address> <name>."]
        names = {}
        if map_path and map_path.exists():
            for line in map_path.read_text(encoding="utf-8").splitlines():
                m = re.match(r"([0-9A-Fa-f]{8})\s+(\S+)", line)
                if m and not m.group(2).startswith("sub_"):
                    names[int(m.group(1), 16)] = m.group(2)
        starts = sorted(set(candidates) | set(names))
        lines += [f"{a:08X} {names.get(a, f'sub_{a:08X}')}" for a in starts]
        Path(emit).write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"wrote {len(starts)} function starts to {emit}")


if __name__ == "__main__":
    main()
