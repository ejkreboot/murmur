#!/usr/bin/env python3
import struct
from pathlib import Path

SRC = Path("images/logo_low_res.bmp")
OUT = Path("firmware/logo_bitmap.h")


def main() -> None:
    b = SRC.read_bytes()
    if b[:2] != b"BM":
        raise SystemExit("not a BMP")

    off = struct.unpack_from("<I", b, 10)[0]
    width, height = struct.unpack_from("<ii", b, 18)
    bpp = struct.unpack_from("<H", b, 28)[0]
    if bpp != 32:
        raise SystemExit(f"expected 32bpp, got {bpp}")

    h = abs(height)
    w = width
    top_down = height < 0
    row_bytes = w * 4

    packed = bytearray()
    for y in range(h):
        sy = y if top_down else (h - 1 - y)
        base = off + sy * row_bytes
        acc = 0
        bit = 7
        for x in range(w):
            bb, gg, rr, _aa = b[base + 4 * x : base + 4 * x + 4]
            luma = (rr * 30 + gg * 59 + bb * 11) // 100
            on = luma >= 128
            if on:
                acc |= 1 << bit
            bit -= 1
            if bit < 0:
                packed.append(acc)
                acc = 0
                bit = 7
        if bit != 7:
            packed.append(acc)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8") as f:
        f.write("#pragma once\n")
        f.write("#include <Arduino.h>\n\n")
        f.write(f"static const uint16_t MURMUR_LOGO_WIDTH = {w};\n")
        f.write(f"static const uint16_t MURMUR_LOGO_HEIGHT = {h};\n")
        f.write("static const uint8_t MURMUR_LOGO_BITMAP[] PROGMEM = {\n")
        for i, v in enumerate(packed):
            if i % 12 == 0:
                f.write("  ")
            f.write(f"0x{v:02X}")
            if i != len(packed) - 1:
                f.write(", ")
            if i % 12 == 11:
                f.write("\n")
        if len(packed) % 12 != 0:
            f.write("\n")
        f.write("};\n")

    print(f"Wrote {OUT} ({len(packed)} bytes)")


if __name__ == "__main__":
    main()
