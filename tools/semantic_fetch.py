#!/usr/bin/env python3
"""Semantic fetch compatibility CLI: prefer v2 Store, retain explicit v1 consumption."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import sys
import urllib.request

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.semantic_sdk import ROOT, verify_archive
from tools.sdk.model import OfflineError, SdkError, detect_host_platform
from tools.semantic.contract import LOCK_FORMAT, read_json
from tools.semantic.provider import select_lock_path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fetch_v1(lock_path: Path, target: str, destination: Path, *, offline: bool = False) -> dict:
    lock = read_json(lock_path)
    target = "linux-x86_64" if target == "native" else target
    if lock.get("format") != "axiom-semantic-toolchain-lock-v1":
        raise SdkError("semantic toolchain lock format is unsupported")
    if target != lock.get("target"):
        raise SdkError(f"semantic toolchain target mismatch: {target}")
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
    if offline:
        raise SdkError("semantic: --offline requires already-materialized v2 SDKs")
    if detect_host_platform().key != "linux-x86_64":
        raise SdkError("semantic: historical v1 is Linux-only; this host requires a v2 lock")
    base_url = os.environ.get("CANVAS_SEMANTIC_SDK_BASE_URL", "").rstrip("/")
    if base_url:
        url = f"{base_url}/{tag}/{asset}"
    else:
        url = f"https://github.com/{repository}/releases/download/{tag}/{asset}"
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="axiom-semantic-fetch-", dir=destination.parent) as directory:
        archive = Path(directory) / asset
        urllib.request.urlretrieve(url, archive)
        actual_sha = sha256(archive)
        if actual_sha != expected_sha:
            raise SdkError(f"semantic toolchain SHA-256 mismatch: expected {expected_sha}, got {actual_sha}")
        verify_archive(
            archive,
            destination,
            json.loads((ROOT / "deps.lock.json").read_text()),
            sdk_id,
            enforce_current_recipe=False,
        )
    return {"sdkId": sdk_id, "asset": asset, "sha256": expected_sha, "url": url}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path)
    parser.add_argument("--target", default="native")
    parser.add_argument("--destination", type=Path, help="Historical v1 only; v2 exports shared Store roots")
    parser.add_argument("--store", type=Path)
    parser.add_argument("--mirror")
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--status", action="store_true")
    args = parser.parse_args()
    lock_path = select_lock_path(ROOT, args.lock)
    lock = read_json(lock_path)
    if lock.get("format") == LOCK_FORMAT:
        if args.destination is not None:
            raise SdkError("semantic: v2 does not use --destination; use --store and exported roots")
        from tools.setup_build_environment import setup_environment, write_status
        facts = setup_environment(core=False, semantic=True, target=args.target, lock_path=lock_path,
                                  store_root=args.store, mirror=args.mirror, offline=args.offline)
        if args.status:
            write_status(facts)
        result = {"environment": facts["environment"], "facts": facts["semantic"]["resolution"]}
    else:
        if args.store is not None or args.mirror is not None:
            raise SdkError("semantic: --store/--mirror require v2 authority")
        result = fetch_v1(lock_path, args.target, args.destination or ROOT / ".deps/protobuf", offline=args.offline)
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except OfflineError as error:
        print(f"semantic: offline resolution failed: {error}", file=sys.stderr)
        raise SystemExit(1)
    except (SdkError, OSError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
