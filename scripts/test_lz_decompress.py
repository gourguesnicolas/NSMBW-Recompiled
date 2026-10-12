#!/usr/bin/env python3
"""Self-test for lz_decompress.py on hand-built synthetic streams (no game data)."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from lz_decompress import decompress

def test_literals_type10():
    # 4 literals, flags byte 0x00
    assert decompress(bytes([0x10, 4, 0, 0, 0x00]) + b"ABCD") == b"ABCD"

def test_backref_type10():
    # "AB" then copy 4 bytes from distance 2 -> "ABABAB"
    stream = bytes([0x10, 6, 0, 0, 0b00100000]) + b"AB" + bytes([(4 - 3) << 4 | 0, 2 - 1])
    assert decompress(stream) == b"ABABAB"

def test_backref_type11_short():
    # type 0x11, short form (indicator 2..15): length = ind + 1
    stream = bytes([0x11, 6, 0, 0, 0b00100000]) + b"AB" + bytes([(4 - 1) << 4 | 0, 2 - 1])
    assert decompress(stream) == b"ABABAB"

def test_backref_type11_medium():
    # indicator 0: length = ((b0&0xF)<<4 | b1>>4) + 0x11
    n = 0x11 + 3
    stream = bytes([0x11, 1 + n, 0, 0, 0b01000000]) + b"Z" + bytes([0x00, 0x30, 0x00])
    assert decompress(stream) == b"Z" * (1 + n)

if __name__ == "__main__":
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            fn(); print("ok", name)
