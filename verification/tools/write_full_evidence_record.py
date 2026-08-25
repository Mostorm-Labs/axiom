#!/usr/bin/env python3
"""Write one provider-neutral GT-G0-15 evidence record."""
import argparse
import hashlib
import json
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--subject", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--corpus-sha256", required=True)
    parser.add_argument("--runner-version", required=True)
    parser.add_argument("--runtime-version", required=True)
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--status", choices=["PASS", "PASS_WITH_OBSERVATIONS", "FAIL", "INVALID_EVIDENCE", "BLOCKED_AUTHORITY"], default="PASS")
    parser.add_argument("--reality", choices=["HOSTED", "PHYSICAL", "EMULATOR", "SIMULATOR", "NOT_APPLICABLE"], default="HOSTED")
    parser.add_argument("--platform-family")
    parser.add_argument("--profile-id")
    parser.add_argument("--category", choices=["PREREQUISITE", "PROFILE"], default="PROFILE")
    return parser.parse_args()


def tree_hash(path: Path) -> str:
    digest = hashlib.sha256()
    if path.is_file():
        digest.update(path.read_bytes())
    elif path.is_dir():
        for child in sorted(item for item in path.rglob("*") if item.is_file()):
            digest.update(str(child.relative_to(path)).replace("\\", "/").encode())
            digest.update(b"\0")
            digest.update(child.read_bytes())
    else:
        digest.update(b"missing")
    return digest.hexdigest()


def main():
    args = parse_args()
    evidence = Path(args.evidence)
    record = {
        "format": "axiom-platform-evidence-record-v1",
        "formatVersion": 1,
        "subject": args.subject,
        "category": args.category,
        "platformFamily": args.platform_family,
        "profileId": args.profile_id,
        "sourceCommit": args.source_commit,
        "corpusSha256": args.corpus_sha256,
        "runnerVersion": args.runner_version,
        "runtimeVersion": args.runtime_version,
        "reality": args.reality,
        "status": args.status,
        "evidenceSha256": tree_hash(evidence),
        "pgStatuses": [{"group": group, "status": args.status} for group in ["PG-01", "PG-02", "PG-03", "PG-04", "PG-05", "PG-06"]],
        "diagnostics": [],
        "environment": {"evidencePath": str(evidence)},
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
