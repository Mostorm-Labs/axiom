#!/usr/bin/env python3
"""Axiom semantic conformance coordinator bootstrap.

This bootstrap validates/discovers corpus metadata and intentionally does not
pretend to execute semantic adapters before they exist.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
GOLDEN = ROOT / "golden" / "v1"


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def seed_suite():
    return load_json(GOLDEN / "suites" / "seed-v0.1.json")


def validate_metadata() -> None:
    corpus = load_json(GOLDEN / "corpus.json")
    suite = seed_suite()
    assert corpus["formatVersion"] == 1
    assert corpus["semanticSchemaVersion"] == 1
    assert corpus["requiredRunnerProtocolVersion"] == 1
    cases = suite["cases"]
    assert suite["id"] == "seed-v0.1"
    assert len(cases) == 60, f"seed-v0.1 must contain exactly 60 cases, got {len(cases)}"
    assert len(set(cases)) == 60, "seed-v0.1 case IDs must be unique"
    assert suite["implementationPolicy"] == {
        "cpp": "REQUIRED",
        "wasm": "REQUIRED",
        "ts": "REQUIRED_WHEN_CAPABLE",
    }


def main() -> int:
    parser = argparse.ArgumentParser(prog="axiom-conformance")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("validate-corpus")
    list_p = sub.add_parser("list")
    list_p.add_argument("--suite", default="seed-v0.1")
    run_p = sub.add_parser("run")
    run_p.add_argument("--suite", default="seed-v0.1")
    args = parser.parse_args()

    validate_metadata()
    if args.command == "validate-corpus":
        print("corpus metadata: OK")
        return 0
    if args.command == "list":
        if args.suite != "seed-v0.1":
            raise SystemExit(f"unknown bootstrap suite: {args.suite}")
        for case_id in seed_suite()["cases"]:
            print(case_id)
        return 0
    if args.command == "run":
        print(
            "BLOCKED: semantic adapters/fixtures are not materialized yet; "
            "bootstrap refuses to fabricate conformance observations.",
            file=sys.stderr,
        )
        return 3
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
