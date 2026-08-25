#!/usr/bin/env python3
"""Generate the G0 semantic bootstrap summary from a CTest JUnit report."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]
SOURCES = (
    "pocs/shared_engine/src/document.cpp",
    "pocs/shared_engine/src/operations.cpp",
    "pocs/shared_engine/src/scene_compiler.cpp",
    "pocs/shared_engine/tests/conformance_test.cpp",
    "pocs/shared_engine/tests/document_test.cpp",
    "pocs/shared_engine/tests/operations_test.cpp",
    "pocs/shared_engine/tests/scene_compiler_test.cpp",
)


def source_digest(root: Path) -> str:
    digest = hashlib.sha256()
    for relative in SOURCES:
        path = root / relative
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def generate(root: Path, junit: Path, source_commit: str) -> dict:
    suite = ET.parse(junit).getroot()
    cases = sorted({case.attrib["name"] for case in suite.iter("testcase")})
    if not cases:
        raise ValueError("semantic bootstrap JUnit contains no test cases")
    failures = list(suite.iter("failure")) + list(suite.iter("error"))
    return {
        "format": "axiom-semantic-bootstrap-summary-v1",
        "formatVersion": 1,
        "authority": "GT-G0-14_BOOTSTRAP_ONLY",
        "scope": "POC01_HOST_CORE_OPERATION_REPLAY_DIGEST_PROJECTION",
        "sourceCommit": source_commit,
        "status": "FAIL" if failures else "PASS",
        "tests": cases,
        "corpusSha256": source_digest(root),
        "limitations": [
            "NOT_G1_SEMANTIC_ACCEPTANCE",
            "NO_PRODUCT_SEMANTIC_KERNEL",
            "NO_60_CASE_SEMANTIC_CORPUS",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--source-commit")
    args = parser.parse_args()
    source_commit = args.source_commit or subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    summary = generate(ROOT, args.junit, source_commit)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0 if summary["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
