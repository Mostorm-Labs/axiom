#!/usr/bin/env python3
"""Fetch and verify the locked hosted Linux semantic toolchain asset."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import urllib.request

from semantic_sdk import ROOT, SdkError, verify_archive


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path, default=ROOT / "semantic-toolchain.lock.json")
    parser.add_argument("--target", default="linux-x86_64")
    parser.add_argument("--destination", type=Path, default=ROOT / ".deps" / "protobuf")
    args = parser.parse_args()
    lock = json.loads(args.lock.read_text(encoding="utf-8"))
    if lock.get("format") != "axiom-semantic-toolchain-lock-v1":
        raise SdkError("semantic toolchain lock format is unsupported")
    if args.target != lock.get("target"):
        raise SdkError(f"semantic toolchain target mismatch: {args.target}")
    asset = lock.get("asset")
    tag = lock.get("releaseTag")
    repository = lock.get("repository")
    expected_sha = lock.get("sha256")
    sdk_id = lock.get("sdkId")
    if not all(isinstance(value, str) and value for value in (asset, tag, repository, expected_sha, sdk_id)):
        raise SdkError("semantic toolchain lock is incomplete")
    if asset != f"semantic-toolchain-linux-x86_64-{sdk_id}.zip" or \
       len(expected_sha) != 64 or any(char not in "0123456789abcdef" for char in expected_sha) or \
       len(sdk_id) != 64 or any(char not in "0123456789abcdef" for char in sdk_id):
        raise SdkError("semantic toolchain lock has an invalid asset or digest")
    base_url = os.environ.get("CANVAS_SEMANTIC_SDK_BASE_URL", "").rstrip("/")
    if base_url:
        url = f"{base_url}/{tag}/{asset}"
    else:
        url = f"https://github.com/{repository}/releases/download/{tag}/{asset}"
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="axiom-semantic-fetch-", dir=args.destination.parent) as directory:
        archive = Path(directory) / asset
        urllib.request.urlretrieve(url, archive)
        actual_sha = sha256(archive)
        if actual_sha != expected_sha:
            raise SdkError(f"semantic toolchain SHA-256 mismatch: expected {expected_sha}, got {actual_sha}")
        verify_archive(archive, args.destination, json.loads((ROOT / "deps.lock.json").read_text()), sdk_id)
    print(json.dumps({"sdkId": sdk_id, "asset": asset, "sha256": expected_sha, "url": url}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
