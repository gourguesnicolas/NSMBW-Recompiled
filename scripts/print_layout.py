#!/usr/bin/env python3
"""Print layout facts (sections, entry point, SDA bases, REL headers) from main.dol and REL files.

Prints addresses and sizes only, never game bytes. Usage: print_layout.py main.dol d_basesNP.rel ...
"""
import struct, sys
from pathlib import Path

def u32(b, o): return struct.unpack_from(">I", b, o)[0]

def dol(path):
    d = Path(path).read_bytes()
    print(f"== {Path(path).name}  size=0x{len(d):X}")
    secs = []
    for i in range(18):
        off, addr, size = u32(d, i*4), u32(d, 0x48 + i*4), u32(d, 0x90 + i*4)
        if size:
            kind = "text" if i < 7 else "data"
            secs.append((kind, i if i < 7 else i-7, off, addr, size))
            print(f"  {kind}{i if i < 7 else i-7}: file 0x{off:06X} addr 0x{addr:08X} size 0x{size:06X} end 0x{addr+size:08X}")
    bss, bss_size, entry = u32(d, 0xD8), u32(d, 0xDC), u32(d, 0xE0)
    print(f"  bss: addr 0x{bss:08X} size 0x{bss_size:06X} end 0x{bss+bss_size:08X}")
    print(f"  entry: 0x{entry:08X}")
    hi = max(max(a+s for _,_,_,a,s in secs), bss+bss_size)
    print(f"  highest address used by the DOL: 0x{hi:08X}")
    def read(addr):
        for _,_,off,a,s in secs:
            if a <= addr < a+s: return u32(d, off + addr - a)
        raise KeyError(hex(addr))
    def disasm(addr, n):
        regs = {}
        for k in range(n):
            ins = read(addr + 4*k); op = ins >> 26
            rd, ra, imm = (ins >> 21) & 31, (ins >> 16) & 31, ins & 0xFFFF
            if op == 18:
                li = ins & 0x03FFFFFC
                if li & 0x02000000: li -= 0x04000000
                tgt = (li if ins & 2 else addr + 4*k + li) & 0xFFFFFFFF
                yield addr+4*k, "bl" if ins & 1 else "b", tgt
            elif op == 15 and ra == 0:
                regs[rd] = imm << 16; yield addr+4*k, f"lis r{rd}", regs[rd]
            elif op == 24 and ra == rd:
                regs[ra] = regs.get(ra, 0) | imm; yield addr+4*k, f"ori r{ra}", (regs[ra])
            elif op == 14 and ra == rd:
                s = imm - 0x10000 if imm & 0x8000 else imm
                regs[rd] = (regs.get(rd, 0) + s) & 0xFFFFFFFF; yield addr+4*k, f"addi r{rd}", regs[rd]
            elif ins == 0x4E800020:
                yield addr+4*k, "blr", None; return
    first_bl = next((t for _, m, t in disasm(entry, 8) if m == "bl"), None)
    print(f"  first bl from entry -> 0x{first_bl:08X} (expected __init_registers)")
    for a, m, v in disasm(first_bl, 64):
        if m.startswith(("lis", "ori", "addi")) and m.split()[1] in ("r1", "r2", "r13"):
            print(f"    0x{a:08X} {m:10s} -> r={v:#010x}" if v is not None else f"    {m}")
    return entry

def rel(path):
    d = Path(path).read_bytes()
    mid, nsec, sec_off = u32(d, 0), u32(d, 0xC), u32(d, 0x10)
    ver, bss = u32(d, 0x1C), u32(d, 0x20)
    rel_off, imp_off, imp_size = u32(d, 0x24), u32(d, 0x28), u32(d, 0x2C)
    pro_s, epi_s, unr_s, bss_s = d[0x30], d[0x31], d[0x32], d[0x33]
    pro, epi, unr = u32(d, 0x34), u32(d, 0x38), u32(d, 0x3C)
    align = u32(d, 0x40) if ver >= 2 else None
    bss_align = u32(d, 0x44) if ver >= 2 else None
    fix = u32(d, 0x48) if ver >= 3 else None
    print(f"== {Path(path).name}  size=0x{len(d):X}  module id {mid}  version {ver}  sections {nsec}  bss 0x{bss:X}")
    print(f"  prolog s{pro_s}+0x{pro:X}  epilog s{epi_s}+0x{epi:X}  unresolved s{unr_s}+0x{unr:X}  align {align}  bssAlign {bss_align}  fixSize 0x{fix:X}" if fix is not None else "")
    for i in range(nsec):
        off_x, size = u32(d, sec_off + 8*i), u32(d, sec_off + 8*i + 4)
        if size:
            print(f"  s{i}: {'exec' if off_x & 1 else '    '} file 0x{off_x & ~1:06X} size 0x{size:06X}{'  (bss)' if (off_x & ~1)==0 else ''}")
    imps = [(u32(d, imp_off + 8*i), u32(d, imp_off + 8*i + 4)) for i in range(imp_size // 8)]
    print("  imports from modules:", ", ".join(str(m) for m,_ in imps))

if __name__ == "__main__":
    for p in sys.argv[1:]:
        (dol if p.endswith(".dol") else rel)(p)
