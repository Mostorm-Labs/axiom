#!/usr/bin/env python3
"""P31 RED oracle for the Scene/resource/composition boundary.

The oracle is intentionally structural and read-only.  At the package anchor no
shared composition target or typed pull+notify port exists; a successful result
means the verification design is stale and P32 must stop.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--expect", choices=("baseline-red", "implemented"), required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    candidates = [
        root / "runtime/composition/include/canvas/runtime/runtime_composition.hpp",
        root / "runtime/composition/src/runtime_composition.cpp",
        root / "runtime/foundation/include/canvas/runtime/resource_port.hpp",
    ]
    present = [str(p.relative_to(root)) for p in candidates if p.exists()]
    if args.expect == "baseline-red":
        if present:
            print("VERIFICATION_DESIGN_DEFECT: composition boundary already exists:", ", ".join(present))
            return 2
        print("RED: no shared composition target or typed pull+notify resource port at baseline")
        return 1
    if len(present) != len(candidates):
        print("FAIL: implemented oracle missing:", ", ".join(str(p.relative_to(root)) for p in candidates if not p.exists()))
        return 1
    print("PASS: shared composition boundary and resource port are present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
