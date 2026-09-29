#!/usr/bin/env python3
"""Embed small binary brush resources as C++ byte arrays."""

from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit("usage: embed_binary.py OUTPUT SYMBOL INPUT")
    output = Path(sys.argv[1])
    symbol = sys.argv[2]
    data = Path(sys.argv[3]).read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="ascii") as stream:
        stream.write("#pragma once\n#include <cstddef>\n#include <cstdint>\n\n")
        stream.write(f"inline constexpr std::uint8_t {symbol}[] = {{\n")
        for index in range(0, len(data), 16):
            values = ", ".join(f"0x{value:02x}" for value in data[index:index + 16])
            stream.write(f"    {values},\n")
        stream.write("};\n")
        stream.write(f"inline constexpr std::size_t {symbol}Size = sizeof({symbol});\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
