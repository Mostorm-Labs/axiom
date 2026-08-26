#!/usr/bin/env python3
"""Create the committed lock for an immutable semantic-toolchain release."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--asset", required=True)
    parser.add_argument("--sdk-id", required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--output", type=Path, default=Path("semantic-toolchain.lock.json"))
    args = parser.parse_args()
    if len(args.sdk_id) != 64 or any(char not in "0123456789abcdef" for char in args.sdk_id):
        raise SystemExit("sdk-id must be a lowercase SHA-256 hex digest")
    if len(args.sha256) != 64 or any(char not in "0123456789abcdef" for char in args.sha256):
        raise SystemExit("sha256 must be a lowercase SHA-256 hex digest")
    if args.asset != f"semantic-toolchain-linux-x86_64-{args.sdk_id}.zip":
        raise SystemExit("asset name must include the locked sdk-id")
    value = {
        "format": "axiom-semantic-toolchain-lock-v1",
        "repository": "Mostorm-Labs/axiom",
        "releaseTag": args.tag,
        "target": "linux-x86_64",
        "asset": args.asset,
        "sdkId": args.sdk_id,
        "sha256": args.sha256,
    }
    args.output.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
