#!/usr/bin/env python3
"""Independently validate the authority-derived geometry boundary oracle."""

from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / "verification/fixture-author/geometry_accounting_boundary_v1.json"


def independent_units(construction: dict) -> int:
    path = construction["path"]
    return (
        path["move"]
        + path["line"]
        + 2 * path["quad"]
        + 3 * path["cubic"]
        + 3 * construction["dabs"]
        + 6 * construction.get("eraseSegments", 0)
    )


def verify_independence(root: Path = ROOT) -> dict:
    tree = ast.parse((root / "verification/fixture-author/compile_geometry_accounting_boundary.py").read_text())
    imported = [alias.name for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names]
    imported += [node.module for node in ast.walk(tree) if isinstance(node, ast.ImportFrom) and node.module]
    violations = [name for name in imported if name == "runtime" or name.startswith("runtime.")]
    return {"ok": not violations, "violations": violations, "identity": "verification-only-independent-oracle"}


def validate(root: Path = ROOT) -> list[str]:
    catalog = json.loads((root / "verification/fixture-author/geometry_accounting_boundary_v1.json").read_text())
    failures = []
    if catalog.get("limit") != 2_000_000:
        failures.append("geometry limit must be 2000000")
    for case in catalog.get("cases", []):
        actual = independent_units(case["construction"])
        if actual != case["expectedUnits"]:
            failures.append(f"{case['id']}: expectedUnits mismatch ({actual} != {case['expectedUnits']})")
        if case["expected"] == "REJECTED" and case.get("error") != "GEOMETRY_LIMIT_EXCEEDED":
            failures.append(f"{case['id']}: missing geometry-limit rejection category")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--verify-independence", action="store_true")
    args = parser.parse_args()
    result = verify_independence(args.root) if args.verify_independence else {"failures": validate(args.root)}
    print(json.dumps(result, indent=2, sort_keys=True))
    if args.verify_independence:
        return 0 if result["ok"] else 1
    return 0 if not result["failures"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
