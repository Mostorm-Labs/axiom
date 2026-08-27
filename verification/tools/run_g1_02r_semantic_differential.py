#!/usr/bin/env python3
"""Compare GT-G1-02R independent leaf goldens with production observations."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
EXPECTED_IDS = (
    "RTW-001", "RTW-002", "RTW-003", "RTW-004", "RTW-005", "RTW-006", "RTW-007", "RTW-008", "RTW-009",
    "STW-001", "STW-002", "STW-003", "STW-004", "STW-005", "STW-006", "STW-007",
    "RTW-N01", "RTW-N02", "STW-N01", "STW-N02", "STW-N03", "STW-N04", "STW-N05",
)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _cases(corpus: Path) -> tuple[dict[str, Path], list[str]]:
    manifest = corpus / "g1-02r-fixture-manifest.json"
    if not manifest.is_file():
        return {}, ["GT-G1-02R fixture manifest is missing"]
    entries = json.loads(manifest.read_text(encoding="utf-8")).get("cases", [])
    ids = tuple(entry.get("id") for entry in entries)
    reasons: list[str] = []
    if ids != EXPECTED_IDS:
        reasons.append("GT-G1-02R fixture case IDs do not match the authority-reviewed catalog")
    result: dict[str, Path] = {}
    for entry in entries:
        case_id = entry.get("id", "")
        directory = corpus / entry.get("path", "")
        result[case_id] = directory
        provenance = directory / "provenance.json"
        if not directory.is_dir() or not provenance.is_file():
            reasons.append(f"{case_id} fixture directory or provenance is missing")
        elif entry.get("provenanceSha256") != _sha256(provenance):
            reasons.append(f"{case_id} provenance does not match manifest")
    return result, reasons


def _root_type(directory: Path, cases: dict[str, Path]) -> str:
    projection = directory / "authoring/input.projection.json"
    if projection.is_file():
        return json.loads(projection.read_text(encoding="utf-8"))["rootType"]
    recipe = json.loads((directory / "authoring/wire.recipe.json").read_text(encoding="utf-8"))
    return _root_type(cases[recipe["baseCase"]], cases)


def _expected(case_id: str, directory: Path) -> dict[str, Any]:
    metadata = json.loads((directory / "case.json").read_text(encoding="utf-8"))["expected"]
    if not case_id.startswith(("RTW-N", "STW-N")):
        return {
            "accepted": True,
            "canonicality": "CANONICAL",
            "stage": "CANONICAL_ENCODE",
            "category": "",
            "canonicalHex": (directory / "expected/canonical.pb").read_bytes().hex(),
            "semanticProjection": json.loads((directory / "expected/semantic.projection.json").read_text(encoding="utf-8")),
        }
    outcome = json.loads((directory / "expected/outcome.json").read_text(encoding="utf-8"))
    return {key: outcome[key] for key in ("accepted", "stage", "category")}


def _observe(probe: Path, root_type: str, input_path: Path) -> dict[str, Any]:
    result = subprocess.run(
        [str(probe), "--root-type", root_type, "--input", str(input_path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"production golden probe failed ({result.returncode}): {result.stderr.strip()}")
    return json.loads(result.stdout)


def first_divergence(case_id: str, expected: dict[str, Any], actual: dict[str, Any]) -> dict[str, Any] | None:
    for key in ("accepted", "canonicality", "stage", "category"):
        if key in expected and expected[key] != actual.get(key):
            return {"caseId": case_id, "dimension": "outcome", "field": key, "expected": expected[key], "actual": actual.get(key)}
    if "canonicalHex" in expected and expected["canonicalHex"] != actual.get("canonicalHex"):
        return {"caseId": case_id, "dimension": "wire", "expected": expected["canonicalHex"], "actual": actual.get("canonicalHex")}
    if "semanticProjection" in expected and expected["semanticProjection"] != actual.get("semanticProjection"):
        return {"caseId": case_id, "dimension": "semanticProjection", "expected": expected["semanticProjection"], "actual": actual.get("semanticProjection")}
    return None


def run_differential(root: Path = ROOT, probe: Path | None = None, corpus: Path | None = None) -> dict[str, Any]:
    root = root.resolve()
    corpus = (corpus or root / "verification/corpus/semantic/v1").resolve()
    cases, reasons = _cases(corpus)
    if probe is None or not probe.is_file():
        reasons.append("production SemanticCodec golden probe is missing")
    if reasons:
        return {
            "format": "axiom-gt-g1-02r-semantic-differential-v1",
            "taskId": "GT-G1-02R",
            "sourceCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
            "caseCount": len(cases),
            "passed": False,
            "failed": [],
            "firstDivergence": None,
            "status": "BLOCKED",
            "blockingReasons": reasons,
            "cases": [],
        }
    observations: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    for case_id in EXPECTED_IDS:
        directory = cases[case_id]
        expected = _expected(case_id, directory)
        input_path = directory / ("expected/canonical.pb" if "canonicalHex" in expected else "input/value.pb")
        actual = _observe(probe, _root_type(directory, cases), input_path)
        divergence = first_divergence(case_id, expected, actual)
        wire_match = "canonicalHex" not in expected or expected["canonicalHex"] == actual.get("canonicalHex")
        semantic_match = "semanticProjection" not in expected or expected["semanticProjection"] == actual.get("semanticProjection")
        outcome_match = divergence is None or divergence["dimension"] not in {"outcome"}
        observation = {
            "caseId": case_id,
            "wireMatch": wire_match,
            "semanticMatch": semantic_match,
            "outcomeMatch": outcome_match,
            "expected": expected,
            "observed": actual,
            "pass": divergence is None,
        }
        observations.append(observation)
        if divergence is not None:
            failures.append(divergence)
    return {
        "format": "axiom-gt-g1-02r-semantic-differential-v1",
        "taskId": "GT-G1-02R",
        "sourceCommit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "caseCount": len(observations),
        "passed": not failures,
        "failed": failures,
        "firstDivergence": failures[0] if failures else None,
        "status": "PASS" if not failures else "FAIL",
        "blockingReasons": [],
        "cases": observations,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--corpus", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = run_differential(args.root, args.probe, args.corpus)
    encoded = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    else:
        print(encoded, end="")
    return 0 if result["status"] == "PASS" else 2 if result["status"] == "BLOCKED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
