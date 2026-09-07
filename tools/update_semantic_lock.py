#!/usr/bin/env python3
"""Generate a v2 Release Set lock; retain the explicit historical v1 CLI."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.sdk.model import SdkError
from tools.semantic.contract import parse_json_bytes, validate_index, validate_v2_lock


def make_v2_lock(index_path: Path, tag: str) -> dict:
    data = index_path.read_bytes()
    index = validate_index(parse_json_bytes(data))
    expected = "semantic-sdk-v2-" + index["releaseSetId"][:16]
    if tag != expected:
        raise SdkError("semantic: supplied tag differs from the releaseSetId-bound tag")
    return validate_v2_lock({"format": "axiom-semantic-sdk-lock-v2", "repository": "Mostorm-Labs/axiom",
                            "releaseTag": expected, "releaseSetId": index["releaseSetId"],
                            "indexAsset": "semantic-sdk-index.json", "indexSha256": hashlib.sha256(data).hexdigest()})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--index", type=Path)
    parser.add_argument("--asset")
    parser.add_argument("--sdk-id")
    parser.add_argument("--sha256")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.index is not None:
        if any(value is not None for value in (args.asset, args.sdk_id, args.sha256)):
            parser.error("--index cannot be combined with v1 asset/sdk-id/sha256 arguments")
        value = make_v2_lock(args.index, args.tag)
        output = args.output or Path("semantic-sdk.lock.json")
    else:
        if any(value is None for value in (args.asset, args.sdk_id, args.sha256)):
            parser.error("v1 mode requires --asset, --sdk-id and --sha256")
        if len(args.sdk_id) != 64 or any(char not in "0123456789abcdef" for char in args.sdk_id):
            raise SystemExit("sdk-id must be a lowercase SHA-256 hex digest")
        if len(args.sha256) != 64 or any(char not in "0123456789abcdef" for char in args.sha256):
            raise SystemExit("sha256 must be a lowercase SHA-256 hex digest")
        if args.asset != f"semantic-toolchain-linux-x86_64-{args.sdk_id}.zip":
            raise SystemExit("asset name must include the locked sdk-id")
        value = {"format": "axiom-semantic-toolchain-lock-v1", "repository": "Mostorm-Labs/axiom",
                 "releaseTag": args.tag, "target": "linux-x86_64", "asset": args.asset,
                 "sdkId": args.sdk_id, "sha256": args.sha256}
        output = args.output or Path("semantic-toolchain.lock.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".semantic-lock-", delete=False) as stream:
        temporary = Path(stream.name)
        stream.write((json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8"))
        stream.flush()
        os.fsync(stream.fileno())
    try:
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
