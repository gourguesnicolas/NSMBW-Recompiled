#!/usr/bin/env python3
"""Port the community's PAL v1 symbols to SMNE01 revision 2 and merge them into a function map.

Two public files from the NSMBW-Community organisation are read (nothing from the game):

- NSMBW-Maps/address-map.txt   address ranges that map PAL v1 (P1) addresses to every other version
- NSMBW-Decomp/syms.txt        `name=0xADDRESS` for PAL v1

The map for a version is written relative to the version it extends, so the offsets are composed:
P1 -> E1 -> E2 for SMNE01 revision 2.

Usage:
  port_symbols.py --fetch DIR                       download both files into DIR
  port_symbols.py DIR MAP.txt                       add the ported addresses (committed form)
  port_symbols.py DIR MAP.txt --with-names OUT.txt  also write the names, to OUT.txt (local use)

The committed MAP.txt keeps addresses only (`sub_XXXXXXXX`): the community's names are their work and
neither repository declares a licence. A ported address not yet in MAP.txt is added; names already
in MAP.txt (written by hand) are kept. --with-names leaves MAP.txt untouched and writes a second map
carrying the community names, for readable crash reports on your own machine. Do not commit it.
"""
import re
import sys
import urllib.request
from pathlib import Path

BASE = "https://raw.githubusercontent.com/NSMBW-Community"
FILES = {
    "address-map.txt": f"{BASE}/NSMBW-Maps/master/address-map.txt",
    "syms.txt": f"{BASE}/NSMBW-Decomp/master/syms.txt",
}
CHAIN = ["E1", "E2"]  # P1 is the base: identity


def fetch(directory):
    Path(directory).mkdir(parents=True, exist_ok=True)
    for name, url in FILES.items():
        with urllib.request.urlopen(url, timeout=60) as r:
            (Path(directory) / name).write_bytes(r.read())
        print(f"fetched {name}")


def parse_address_map(path):
    versions, current = {}, None
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        if m := re.fullmatch(r"\[(\w+)\]", line):
            current = versions.setdefault(m.group(1), [])
        elif m := re.match(r"([0-9a-f]{8})-(?:([0-9a-f]{8})|\*):\s*([+-]0x[0-9a-f]+)", line):
            hi = int(m.group(2), 16) if m.group(2) else 0xFFFFFFFF
            current.append((int(m.group(1), 16), hi, int(m.group(3), 16)))
    return versions


def port(versions, address):
    for version in CHAIN:
        for lo, hi, offset in versions[version]:
            if lo <= address <= hi:
                address += offset
                break
        else:
            return None
    return address


def parse_symbols(path):
    symbols = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        name, _, address = line.strip().partition("=")
        if address:
            symbols.setdefault(int(address, 16), name)  # first name wins for aliases
    return symbols


def clean(name):
    """Keep the map one token per line and safe to emit as a C string or comment."""
    return re.sub(r"[^A-Za-z0-9_$.]", "_", name)


def read_map(path):
    header, entries = [], {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if m := re.match(r"([0-9A-Fa-f]{8})\s+(\S+)", line):
            entries[int(m.group(1), 16)] = m.group(2)
        elif line.startswith("#"):
            header.append(line)
    return header, entries


def main():
    args = sys.argv[1:]
    if len(args) == 2 and args[0] == "--fetch":
        return fetch(args[1])
    if len(args) < 2:
        sys.exit(__doc__)
    directory, map_path = Path(args[0]), Path(args[1])
    names_out = Path(args[args.index("--with-names") + 1]) if "--with-names" in args else None
    versions = parse_address_map(directory / "address-map.txt")
    symbols = parse_symbols(directory / "syms.txt")
    header, entries = read_map(map_path)

    ported = dict(entries)
    named = added = unmapped = 0
    for p1_address, name in symbols.items():
        address = port(versions, p1_address)
        if address is None:
            unmapped += 1
            continue
        if address not in entries:
            entries[address] = f"sub_{address:08X}"
            added += 1
        current = ported.get(address)
        if current is None or current.startswith("sub_"):
            ported[address] = clean(name)
            named += current is not None

    def write(path, table):
        lines = header + [f"{a:08X} {n}" for a, n in sorted(table.items())]
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    if names_out:
        # Community names on top of every start in the committed map.
        for address, name in entries.items():
            ported.setdefault(address, name)
        write(names_out, ported)
        print(f"wrote {len(ported)} named entries to {names_out} (do not commit)")
    else:
        write(map_path, entries)
    print(f"{len(symbols)} symbols: {added} new starts, {unmapped} unmapped; "
          f"map has {len(entries)} entries")


if __name__ == "__main__":
    main()
