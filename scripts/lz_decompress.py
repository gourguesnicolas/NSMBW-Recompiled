#!/usr/bin/env python3
"""Decompress Nintendo LZ77 (types 0x10 and 0x11), as used for .rel.LZ files."""
import sys

def decompress(src: bytes) -> bytes:
    kind = src[0]
    size = src[1] | src[2] << 8 | src[3] << 16
    pos = 4
    if size == 0:
        size = int.from_bytes(src[4:8], "little"); pos = 8
    if kind not in (0x10, 0x11):
        raise ValueError(f"unsupported LZ type 0x{kind:02x}")
    out = bytearray()
    while len(out) < size:
        flags = src[pos]; pos += 1
        for bit in range(7, -1, -1):
            if len(out) >= size:
                break
            if not (flags >> bit) & 1:
                out.append(src[pos]); pos += 1
                continue
            if kind == 0x10:
                b0, b1 = src[pos], src[pos + 1]; pos += 2
                length = (b0 >> 4) + 3
                disp = ((b0 & 0xF) << 8 | b1) + 1
            else:
                b0 = src[pos]; ind = b0 >> 4
                if ind == 0:
                    b1, b2 = src[pos + 1], src[pos + 2]; pos += 3
                    length = ((b0 & 0xF) << 4 | b1 >> 4) + 0x11
                    disp = ((b1 & 0xF) << 8 | b2) + 1
                elif ind == 1:
                    b1, b2, b3 = src[pos + 1], src[pos + 2], src[pos + 3]; pos += 4
                    length = ((b0 & 0xF) << 12 | b1 << 4 | b2 >> 4) + 0x111
                    disp = ((b2 & 0xF) << 8 | b3) + 1
                else:
                    b1 = src[pos + 1]; pos += 2
                    length = ind + 1
                    disp = ((b0 & 0xF) << 8 | b1) + 1
            if disp > len(out):
                raise ValueError("back-reference before start of output")
            for _ in range(length):
                out.append(out[-disp])
    if len(out) != size:
        raise ValueError("size mismatch")
    return bytes(out)

if __name__ == "__main__":
    data = open(sys.argv[1], "rb").read()
    print(f"type 0x{data[0]:02x}", file=sys.stderr)
    open(sys.argv[2], "wb").write(decompress(data))
