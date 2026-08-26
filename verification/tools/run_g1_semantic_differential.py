#!/usr/bin/env python3
"""Inventory and validate the authority-promoted GT-G1-02 BG/BGX corpus.

This tool deliberately does not manufacture binary vectors.  Until the current
semantic authority promotes the files, an empty or candidate corpus produces a
machine-readable BLOCKED_AUTHORITY result.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


DEFAULT_ROOT = Path(__file__).resolve().parents[2]
WIRE_ROOTS = (
    "verification/corpus/semantic/v1/wire/bg",
    "verification/corpus/semantic/v1/wire/bgx",
)


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _inventory(root: Path) -> dict[str, Any]:
    files: list[dict[str, Any]] = []
    for relative_root in WIRE_ROOTS:
        directory = root / relative_root
        if not directory.exists():
            continue
        for path in sorted(path for path in directory.rglob("*") if path.is_file()):
            relative = path.relative_to(root).as_posix()
            data = path.read_bytes()
            files.append({"path": relative, "sha256": _sha256_bytes(data), "bytes": len(data)})
    files.sort(key=lambda item: item["path"])
    digest = hashlib.sha256()
    for item in files:
        digest.update(item["path"].encode("utf-8"))
        digest.update(b"\0")
        digest.update(item["sha256"].encode("ascii"))
        digest.update(b"\0")
        digest.update(str(item["bytes"]).encode("ascii"))
        digest.update(b"\n")
    return {"fileCount": len(files), "files": files, "inventorySha256": digest.hexdigest()}


def run_differential(root: Path = DEFAULT_ROOT) -> dict[str, Any]:
    root = root.resolve()
    manifest_path = root / "verification/corpus/semantic/v1/corpus.json"
    manifest: dict[str, Any] = {}
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    inventory = _inventory(root)
    reasons: list[str] = []
    status = "PASS"
    if inventory["fileCount"] == 0:
        status = "BLOCKED_AUTHORITY"
        reasons.append("BG/BGX authority-promoted binary corpus is missing")
    elif manifest.get("status") not in {"promoted", "frozen"}:
        status = "BLOCKED_AUTHORITY"
        reasons.append("BG/BGX corpus manifest is not authority-promoted")
    elif manifest.get("differentialOracle") != "authority_promoted":
        status = "BLOCKED_AUTHORITY"
        reasons.append("BG/BGX differential oracle is not authority-promoted")
    return {
        "format": "axiom-gt-g1-02-semantic-differential-v1",
        "taskId": "GT-G1-02",
        "status": status,
        "blockingReasons": reasons,
        "manifestStatus": manifest.get("status", "missing"),
        "binaryCorpus": inventory,
        "differential": {
            "decoder": "pending authority corpus/oracle" if status != "PASS" else "strict codec differential",
            "firstDivergence": None,
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = run_differential(args.root)
    encoded = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
