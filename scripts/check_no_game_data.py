#!/usr/bin/env python3
"""Fail if the repository tracks anything that looks like game data.

Checks every file tracked by git for: forbidden extensions, forbidden
directories (translator output), known container magic numbers, and size.
This is a safety net for docs/LEGAL.md, not a replacement for it.
"""
import subprocess
import sys
from pathlib import Path

FORBIDDEN_EXTENSIONS = {
    ".dol", ".rel", ".lz", ".lh", ".iso", ".gcm", ".gcz", ".ciso", ".wbfs",
    ".wia", ".rvz", ".nkit", ".wad", ".tgc", ".arc", ".szs", ".u8", ".carc",
    ".brres", ".brsar", ".brstm", ".breff", ".breft", ".brlyt", ".brlan",
    ".brfnt", ".bmg", ".thp", ".tpl", ".gci", ".dff", ".pul",
}

# Translator output must never be committed.
FORBIDDEN_DIRECTORIES = ("generated/", "Assets/", "assets/", "PulsarPacks/", "Dumps/", "dumps/")

# (offset, magic bytes, description)
MAGIC_NUMBERS = [
    (0x18, bytes.fromhex("5D1C9EA3"), "Wii disc image"),
    (0x1C, bytes.fromhex("C2339F3D"), "GameCube disc image"),
    (0x00, b"WBFS", "WBFS disc image"),
    (0x00, b"RVZ\x01", "RVZ disc image"),
    (0x00, b"WIA\x01", "WIA disc image"),
    (0x00, b"CISO", "CISO disc image"),
    (0x00, bytes.fromhex("01C00BB1"), "GCZ disc image"),
    (0x00, bytes.fromhex("55AA382D"), "U8 archive"),
    (0x00, b"Yaz0", "Yaz0-compressed game file"),
    (0x00, b"Yay0", "Yay0-compressed game file"),
    (0x00, b"RSAR", "sound archive"),
    (0x00, b"RSTM", "audio stream"),
    (0x00, b"bres", "model/texture archive"),
    (0x00, b"RLYT", "layout file"),
    (0x00, b"RFNT", "font file"),
]

MAX_FILE_BYTES = 25 * 1024 * 1024

# Paths that are allowed to trip a rule. Keep this list short and explained.
ALLOWLIST: dict[str, str] = {}


def tracked_files() -> list[str]:
    out = subprocess.run(
        ["git", "ls-files", "-z"], check=True, capture_output=True
    ).stdout
    return [p for p in out.decode("utf-8", "surrogateescape").split("\0") if p]


def check(path: str) -> list[str]:
    problems = []
    p = Path(path)
    if p.suffix.lower() in FORBIDDEN_EXTENSIONS:
        problems.append(f"forbidden extension '{p.suffix}'")
    if path.startswith(FORBIDDEN_DIRECTORIES):
        problems.append("inside a directory reserved for game data or translator output")
    if not p.is_file() or p.is_symlink():
        return problems
    size = p.stat().st_size
    if size > MAX_FILE_BYTES:
        problems.append(f"larger than {MAX_FILE_BYTES // (1024 * 1024)} MiB ({size} bytes)")
    with p.open("rb") as handle:
        head = handle.read(0x20)
    for offset, magic, description in MAGIC_NUMBERS:
        if head[offset:offset + len(magic)] == magic:
            problems.append(f"looks like a {description}")
    return problems


def main() -> int:
    failures = 0
    files = tracked_files()
    for path in files:
        if path in ALLOWLIST:
            continue
        for problem in check(path):
            failures += 1
            print(f"::error file={path}::{path}: {problem}")
    if failures:
        print(f"\n{failures} problem(s) found. See docs/LEGAL.md: game data and translator "
              "output must never be committed.")
        return 1
    print(f"OK: {len(files)} tracked files, no game data found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
