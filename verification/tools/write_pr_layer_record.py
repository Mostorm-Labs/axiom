#!/usr/bin/env python3
"""Write a provider-neutral PR layer record bound to an evidence file or tree."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def evidence_digest(path: Path) -> str:
    digest = hashlib.sha256()
    files = sorted(item for item in ([path] if path.is_file() else path.rglob("*")) if item.is_file())
    if not files:
        raise ValueError("evidence input is empty")
    for item in files:
        relative = item.name if path.is_file() else item.relative_to(path).as_posix()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(item.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--layer", choices=("schema", "protocol", "semantic", "platform"), required=True)
    parser.add_argument("--subject", required=True)
    parser.add_argument("--status", choices=("PASS", "PASS_WITH_OBSERVATIONS", "FAIL", "INVALID_EVIDENCE", "BLOCKED_AUTHORITY"), required=True)
    parser.add_argument("--attempt", type=int, default=1)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    record = {
        "format": "axiom-pr-layer-record-v1",
        "formatVersion": 1,
        "layer": args.layer,
        "subject": args.subject,
        "attempt": args.attempt,
        "status": args.status,
        "evidenceSha256": evidence_digest(args.evidence),
        "diagnostics": [],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
