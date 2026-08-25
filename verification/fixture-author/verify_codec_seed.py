#!/usr/bin/env python3
"""Verification-only byte oracle checker for BG-001..010.

This tool intentionally does not import or link the production Axiom codec.
It checks checked-in DERIVED_GENERATED binary truth against the human-reviewed
hex review surface from the frozen codec seed authority.
"""
from __future__ import annotations

import json
from pathlib import Path

VERIFICATION = Path(__file__).resolve().parents[1]
GOLDEN = VERIFICATION / "golden" / "v1"
SUITE = GOLDEN / "suites" / "codec-binary-seed-v0.1.json"


def main() -> int:
    suite = json.loads(SUITE.read_text(encoding="utf-8"))
    cases = suite["cases"]
    assert suite["id"] == "codec-binary-seed-v0.1"
    assert len(cases) == 10
    assert [c["id"] for c in cases] == [f"BG-{i:03d}" for i in range(1, 11)]

    for case in cases:
        path = GOLDEN / case["path"]
        actual = path.read_bytes()
        expected = bytes.fromhex(case["expectedHex"])
        assert actual == expected, (
            f"{case['id']} byte drift: actual={actual.hex()} expected={expected.hex()}"
        )
        print(f"{case['id']} {case['type']}: {len(actual)} bytes OK {actual.hex()}")

    print("codec binary seed: 10/10 exact bytes OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
