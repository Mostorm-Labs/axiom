#!/usr/bin/env python3
"""Check the proposed target layouts; no Runtime/SDK linkage is exercised."""
import argparse
import json
from pathlib import Path


EXPECTED = {
    "object_id_size": 16,
    "token_size": 16,
    "excluded_size": 20,
    "excluded_reason_offset": 16,
    "projection_size": 80,
    "projection_token_offset": 8,
    "projection_total_offset": 56,
    "value_size": 96,
    "value_dash_pointer_offset": 80,
    "value_dash_count_offset": 88,
    "receipt_size": 48,
    "receipt_counts_offset": 24,
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    args = parser.parse_args()
    for label, path, pointer_size in (("windows-x64", args.native, 8), ("wasm32", args.wasm, 4)):
        actual = json.loads(path.read_text(encoding="utf-8"))
        expected = dict(EXPECTED, pointer_size=pointer_size)
        if actual != expected:
            raise SystemExit(f"{label}: layout mismatch: expected {expected}, actual {actual}")
        print(f"{label}: candidate layout matches proposed values; pointer width={pointer_size}")
    print("DRAFT ONLY: compile/layout does not prove public SDK linkage, Runtime behavior or acceptance")


if __name__ == "__main__":
    main()
