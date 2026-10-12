#!/usr/bin/env python3
"""Inventory of the hardware-facing and SDK code a game executes, for the runtime HLE (issue #14).

Reads main.dol and a function map (the committed MAP.txt is enough) and writes a Markdown report:

1. Address bands: where the game, the SDK and the C library sit in the DOL.
2. Functions that touch memory-mapped hardware, per device. With the runtime's native hooks off
   these run as translated guest code, so each one needs an HLE, an emulated register, or a port.
3. The SDK entry points game code calls, ranked by number of call sites.
4. Which of them are already located in docs/nsmbw/hle-hooks.md.

Addresses and counts only, never game bytes.

Usage:
  sdk_inventory.py main.dol MAP.txt docs/nsmbw/hle-hooks.md > docs/nsmbw/sdk-inventory.md
  sdk_inventory.py main.dol MAP.txt docs/nsmbw/hle-hooks.md --names named.txt > local.md

--names takes a map carrying names (see scripts/port_symbols.py --with-names) and adds them to the
report. Use it for your own reading only: the community's names are not committed (issue #6).
"""
import bisect
import collections
import re
import struct
import sys
from pathlib import Path

# Approximate: set from the boundaries of named game classes (nw4r snd, nw4r ut) and the first and last
# functions that touch SDK state. Correct them here if a band is wrong; the report follows.
BANDS = [
    ("startup (crt0, init)", 0x80004000, 0x80006780),
    ("game and engine code", 0x80006780, 0x8019F000),
    ("SDK: OS, VI, GX, DVD, AX, NAND, WPAD, KPAD", 0x8019F000, 0x80229000),
    ("engine libraries (nw4r, EGG)", 0x80229000, 0x802DC000),
    ("C library and compiler runtime", 0x802DC000, 0x802EA000),
]
SDK_BAND = 2

DEVICES = [
    (0xCC000000, 0xCC000FFF, "CP (command processor)"), (0xCC001000, 0xCC001FFF, "PE (pixel engine)"),
    (0xCC002000, 0xCC002FFF, "VI (video interface)"), (0xCC003000, 0xCC003FFF, "PI (processor interface)"),
    (0xCC004000, 0xCC004FFF, "MI (memory interface)"), (0xCC005000, 0xCC005FFF, "DSP"),
    (0xCC006000, 0xCC0063FF, "DI (disc)"), (0xCC006400, 0xCC0067FF, "SI (serial)"),
    (0xCC006800, 0xCC006BFF, "EXI"), (0xCC006C00, 0xCC006FFF, "AI (audio)"),
    (0xCC008000, 0xCC008FFF, "GX write-gather FIFO"),
    (0xCD000000, 0xCD0000FF, "IPC (IOS)"), (0xCD800000, 0xCD800FFF, "Hollywood registers"),
    (0xCD806000, 0xCD8067FF, "EXI/SI (Wii)"),
]
LIS_MMIO = {0xCC00, 0xCC01, 0xCC02, 0xCD00, 0xCD01, 0xCD80, 0xCD81}
LOAD_STORE = set(range(32, 56))


def u32(b, o): return struct.unpack_from(">I", b, o)[0]
def sx(v): return v - 0x10000 if v & 0x8000 else v


class Dol:
    def __init__(self, data):
        self.data = data
        self.text = [(u32(data, i * 4), u32(data, 0x48 + i * 4), u32(data, 0x90 + i * 4)) for i in range(7)]
        self.text = [t for t in self.text if t[2]]

    def words(self):
        for off, addr, size in self.text:
            for i in range(size // 4):
                yield addr + i * 4, u32(self.data, off + i * 4)


def device(ea):
    for lo, hi, name in DEVICES:
        if lo <= ea <= hi:
            return name
    return None


def band(address):
    for i, (_n, lo, hi) in enumerate(BANDS):
        if lo <= address < hi:
            return i
    return None


def hle_table(path):
    """Rows of docs/nsmbw/hle-hooks.md: SMNE01 address -> SDK function name."""
    names = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 4 and re.fullmatch(r"`0x[0-9A-Fa-f]{8}`", cells[2]):
            name = re.sub(r"\s*\(.*", "", cells[0]).strip("` ")
            names[int(cells[2].strip("`"), 16)] = name
    return names


def read_names(path):
    names = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        m = re.match(r"([0-9A-Fa-f]{8})\s+(\S+)", line)
        if m and not m.group(2).startswith("sub_"):
            names[int(m.group(1), 16)] = m.group(2)
    return names


def main():
    args = sys.argv[1:]
    local_names = {}
    if "--names" in args:
        i = args.index("--names")
        local_names = read_names(args[i + 1])
        del args[i:i + 2]
    if len(args) != 3:
        sys.exit(__doc__)
    sys.argv = [sys.argv[0]] + args
    dol = Dol(Path(sys.argv[1]).read_bytes())
    starts = sorted(int(m.group(1), 16) for line in Path(sys.argv[2]).read_text(encoding="utf-8").splitlines()
                    if (m := re.match(r"([0-9A-Fa-f]{8})\s", line)))
    startset = set(starts)
    known = hle_table(sys.argv[3])
    located_count = len(known)
    for address, name in local_names.items():
        known.setdefault(address, name)

    def owner(address):
        return starts[bisect.bisect_right(starts, address) - 1]

    # Direct calls (bl) and tail calls (b to another function's first instruction).
    calls = collections.defaultdict(list)  # callee -> caller functions
    mmio = collections.defaultdict(set)    # function -> devices
    words = list(dol.words())
    index = {a: i for i, (a, _w) in enumerate(words)}
    for i, (address, word) in enumerate(words):
        opcode = word >> 26
        if opcode == 18:
            li = word & 0x03FFFFFC
            li -= 0x04000000 if li & 0x02000000 else 0
            target = (li if word & 2 else address + li) & 0xFFFFFFFF
            if target in startset and (word & 1 or owner(address) != target):
                calls[target].append(owner(address))
        elif opcode == 15 and (word & 0xFFFF) in LIS_MMIO:
            register, base = (word >> 21) & 31, (word & 0xFFFF) << 16
            for j in range(1, 12):
                if i + j >= len(words):
                    break
                nxt = words[i + j][1]
                if (nxt >> 16) & 31 == register and nxt >> 26 in LOAD_STORE | {14}:
                    dev = device((base + sx(nxt & 0xFFFF)) & 0xFFFFFFFF)
                    if dev:
                        mmio[owner(address)].add(dev)
                if (nxt >> 21) & 31 == register and nxt >> 26 in (14, 15, 24, 32):
                    break

    out = []
    w = out.append
    w("# SDK and hardware inventory (SMNE01 rev 2)\n")
    w("Generated by `scripts/sdk_inventory.py` from `main.dol` and `projects/nsmbw-usa/MAP.txt`. "
      "Addresses and counts only. Direct calls only: calls through function pointers and vtables are "
      "not in the counts, so every number is a lower bound.\n")

    w("## Address bands\n")
    w("Approximate: the boundaries come from the named game classes on either side. They are set in "
      "`BANDS` in the script.\n")
    w("| Band | Range | Function starts |")
    w("| --- | --- | --- |")
    for name, lo, hi in BANDS:
        n = sum(1 for s in starts if lo <= s < hi)
        w(f"| {name} | `0x{lo:08X}` - `0x{hi:08X}` | {n:,} |")
    w("")

    per_device = collections.defaultdict(list)
    for f, ds in mmio.items():
        for d in ds:
            per_device[d].append(f)
    w("## Functions that touch hardware registers\n")
    w(f"{len(mmio)} functions reference a memory-mapped hardware register. Running as translated "
      "code, each one reads or writes a host address the runtime has to answer. `In SDK` is the number "
      "inside the SDK band; the rest sit in game or engine code, which writes the GX FIFO inline.\n")
    w("| Device | Functions | In SDK band | Lowest | Highest |")
    w("| --- | --- | --- | --- | --- |")
    for dev, fs in sorted(per_device.items(), key=lambda x: -len(x[1])):
        fs = sorted(fs)
        in_sdk = sum(1 for f in fs if band(f) == SDK_BAND)
        w(f"| {dev} | {len(fs)} | {in_sdk} | `0x{fs[0]:08X}` | `0x{fs[-1]:08X}` |")
    w("")
    outside = sorted(f for f, ds in mmio.items() if band(f) != SDK_BAND)
    w(f"Outside the SDK band, {len(outside)} functions touch hardware directly "
      f"(`GX write-gather FIFO` in almost every case). They are inlined GX calls in game and engine code: "
      "the runtime has to answer FIFO writes wherever they come from, not only inside `GX*` functions.\n")

    api = {t: callers for t, callers in calls.items() if band(t) == SDK_BAND}
    used = {t: [c for c in cs if band(c) != SDK_BAND] for t, cs in api.items()}
    used = {t: cs for t, cs in used.items() if cs}
    sdk_total = sum(1 for s in starts if band(s) == SDK_BAND)
    w("## SDK entry points called from game code\n")
    w(f"The SDK band holds {sdk_total:,} function starts. Game and engine code calls {len(used):,} of them "
      "directly; the rest are reached from inside the SDK, through pointers, or not at all.\n")
    w("| SDK function | Direct call sites | Distinct callers | Touches | Known name |")
    w("| --- | --- | --- | --- | --- |")
    for target, callers in sorted(used.items(), key=lambda x: -len(x[1]))[:60]:
        touches = ", ".join(sorted(mmio.get(target, []))) or ""
        name = known.get(target, "")
        w(f"| `0x{target:08X}` | {len(callers)} | {len(set(callers))} | {touches} | {name} |")
    w("")

    fifo_in_sdk = sum(1 for f in per_device.get("GX write-gather FIFO", []) if band(f) == SDK_BAND)
    hle_located = [a for a in hle_table(sys.argv[3]) if a in used]
    w("## Coverage against the located hooks\n")
    w(f"`docs/nsmbw/hle-hooks.md` places {located_count} functions at SMNE01 addresses; "
      f"{len(hle_located)} of them are called directly from game code. The runtime registers 583 hooks, "
      f"so most of the SDK surface above has no known counterpart yet. The GX library is the largest "
      f"group: {fifo_in_sdk} functions inside the SDK band write the FIFO, and the game calls dozens of "
      "the state setters, matrix functions and texture loaders directly.\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
