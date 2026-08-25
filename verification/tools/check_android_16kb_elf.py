#!/usr/bin/env python3
"""Check that every Android ELF LOAD segment is aligned for 16 KB pages."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def find_readelf(explicit: str | None) -> str:
    candidates = [explicit, os.environ.get("LLVM_READELF"), shutil.which("llvm-readelf"), shutil.which("readelf")]
    for candidate in candidates:
        if candidate and Path(candidate).is_file() or candidate and shutil.which(candidate):
            return candidate
    ndk_root = os.environ.get("ANDROID_NDK_ROOT") or os.environ.get("ANDROID_NDK_HOME")
    if ndk_root:
        for path in Path(ndk_root).glob("toolchains/llvm/prebuilt/*/bin/llvm-readelf"):
            return str(path)
    raise RuntimeError("llvm-readelf/readelf not found; set LLVM_READELF or ANDROID_NDK_ROOT")


def load_alignments(readelf: str, elf: Path) -> list[int]:
    output = subprocess.run(
        [readelf, "-lW", str(elf)], check=True, text=True, capture_output=True
    ).stdout
    alignments: list[int] = []
    for line in output.splitlines():
        fields = line.split()
        if fields and fields[0] == "LOAD" and len(fields) >= 8:
            alignments.append(int(fields[-1], 0))
    if not alignments:
        raise RuntimeError(f"{elf}: no ELF LOAD segments found")
    return alignments


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--readelf")
    args = parser.parse_args(argv)
    elf = args.elf.resolve(strict=True)
    try:
        readelf = find_readelf(args.readelf)
        alignments = load_alignments(readelf, elf)
        if any(alignment < 0x4000 or alignment % 0x4000 != 0 for alignment in alignments):
            raise RuntimeError(
                f"{elf}: LOAD segment alignment is not 16 KB compatible: "
                + ", ".join(hex(value) for value in alignments)
            )
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as exc:
        print(f"android 16 KB ELF check failed: {exc}", file=sys.stderr)
        return 1
    print(f"android 16 KB ELF check passed: {elf} LOAD alignments="
          + ",".join(hex(value) for value in alignments))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
