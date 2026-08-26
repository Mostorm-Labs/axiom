#!/usr/bin/env python3
"""Compare production SemanticCodec observations to authority golden fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Any


DEFAULT_ROOT = Path(__file__).resolve().parents[2]
CORPUS = "verification/corpus/semantic/v1"
AUTHORITY_CASE_IDS = (
    "BG-001", "BG-002", "BG-003", "BG-004", "BG-005", "BG-006", "BG-007", "BG-008", "BG-009", "BG-010",
    "BG-N01", "BG-N02", "BG-N03", "BG-N04", "BG-N05", "BG-N06", "BG-N07", "BG-N08",
)


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _inventory(root: Path) -> dict[str, Any]:
    files: list[dict[str, Any]] = []
    for relative_root in ("wire/bg", "wire/bgx"):
        directory = root / CORPUS / relative_root
        if directory.exists():
            for path in sorted(path for path in directory.rglob("*") if path.is_file()):
                relative = path.relative_to(root).as_posix()
                data = path.read_bytes()
                files.append({"path": relative, "sha256": _sha256_bytes(data), "bytes": len(data)})
    digest = hashlib.sha256()
    for item in files:
        digest.update(item["path"].encode("utf-8")); digest.update(b"\0")
        digest.update(item["sha256"].encode("ascii")); digest.update(b"\0")
        digest.update(str(item["bytes"]).encode("ascii")); digest.update(b"\n")
    return {"fileCount": len(files), "files": files, "inventorySha256": digest.hexdigest()}


def _fixture_cases(root: Path) -> tuple[dict[str, Path], list[str]]:
    manifest_path = root / CORPUS / "fixture-manifest.json"
    if not manifest_path.exists():
        return {}, ["BG/BGX authority fixture manifest is missing; exactly 18 BG cases are required"]
    entries = json.loads(manifest_path.read_text(encoding="utf-8")).get("cases", [])
    cases = {entry.get("id"): root / CORPUS / entry.get("path", "") for entry in entries}
    reasons: list[str] = []
    if len(entries) != len(AUTHORITY_CASE_IDS) or tuple(cases) != AUTHORITY_CASE_IDS:
        reasons.append("authority fixture set must contain exactly 18 ordered BG cases")
    for case_id in AUTHORITY_CASE_IDS:
        directory = cases.get(case_id)
        if directory is None or not directory.is_dir():
            reasons.append(f"{case_id} fixture directory is missing")
            continue
        if not (directory / "case.json").is_file() or not (directory / "provenance.json").is_file():
            reasons.append(f"{case_id} case metadata or provenance is missing")
        else:
            entry = next(item for item in entries if item.get("id") == case_id)
            if entry.get("provenanceSha256") != _sha256_bytes((directory / "provenance.json").read_bytes()):
                reasons.append(f"{case_id} provenance hash does not match fixture manifest")
            if json.loads((directory / "case.json").read_text(encoding="utf-8")).get("id") != case_id:
                reasons.append(f"{case_id} case metadata identity does not match fixture manifest")
        required = ("authoring/wire.recipe.json", "input/value.pb", "expected/outcome.json") if case_id.startswith("BG-N") else ("authoring/input.projection.json", "expected/canonical.pb")
        for relative in required:
            if not (directory / relative).is_file(): reasons.append(f"{case_id} is missing {relative}")
    return cases, reasons


def _root_type(case_directory: Path, cases: dict[str, Path]) -> str:
    projection = case_directory / "authoring/input.projection.json"
    if projection.exists(): return json.loads(projection.read_text(encoding="utf-8"))["rootType"]
    recipe = json.loads((case_directory / "authoring/wire.recipe.json").read_text(encoding="utf-8"))
    return _root_type(cases[recipe["baseCase"]], cases)


def _expected(case_id: str, directory: Path) -> dict[str, Any]:
    if not case_id.startswith("BG-N"):
        return {"accepted": True, "canonicality": "CANONICAL", "stage": "CANONICAL_ENCODE", "category": "", "canonicalHex": (directory / "expected/canonical.pb").read_bytes().hex()}
    outcome = json.loads((directory / "expected/outcome.json").read_text(encoding="utf-8"))
    expected: dict[str, Any] = {"category": outcome.get("category", outcome.get("strictCategory", ""))}
    if "strictCategory" in outcome:
        expected.update({"accepted": False, "stage": outcome["stage"], "category": outcome["strictCategory"]})
        return expected
    if "canonicality" in outcome:
        expected.update({"accepted": True, "canonicality": outcome["canonicality"], "stage": outcome["stage"], "category": ""})
        canonical = directory / "expected/canonical.pb"
        if canonical.exists(): expected["canonicalHex"] = canonical.read_bytes().hex()
    else:
        expected.update({"accepted": False, "stage": outcome["stage"]})
    return expected


def _observe(probe: Path, root_type: str, input_path: Path) -> dict[str, Any]:
    completed = subprocess.run([str(probe), "--root-type", root_type, "--input", str(input_path)], check=False, capture_output=True, text=True)
    if completed.returncode != 0: raise RuntimeError(f"production probe failed ({completed.returncode}): {completed.stderr.strip()}")
    return json.loads(completed.stdout)


def _first_difference(case_id: str, expected: dict[str, Any], actual: dict[str, Any]) -> dict[str, Any] | None:
    for key, expected_value in expected.items():
        if actual.get(key) != expected_value: return {"caseId": case_id, "field": key, "expected": expected_value, "actual": actual.get(key)}
    return None


def run_differential(root: Path = DEFAULT_ROOT, probe: Path | None = None) -> dict[str, Any]:
    root = root.resolve()
    manifest_path = root / CORPUS / "corpus.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    inventory = _inventory(root)
    cases, reasons = _fixture_cases(root)
    if manifest.get("status") not in {"promoted", "frozen"}: reasons.append("BG corpus manifest is not authority-promoted")
    if manifest.get("differentialOracle") != "authority_promoted": reasons.append("BG differential oracle is not authority-promoted")
    if not cases: reasons.append("BG authority-promoted binary corpus is missing")
    if reasons:
        return {"format": "axiom-gt-g1-02-semantic-differential-v2", "taskId": "GT-G1-02", "status": "BLOCKED_AUTHORITY", "blockingReasons": reasons, "manifestStatus": manifest.get("status", "missing"), "binaryCorpus": inventory, "differential": {"decoder": "not-run", "firstDivergence": None, "observations": []}}
    if probe is None or not probe.is_file():
        return {"format": "axiom-gt-g1-02-semantic-differential-v2", "taskId": "GT-G1-02", "status": "BLOCKED_RUNTIME", "blockingReasons": ["production SemanticCodec golden probe is missing"], "manifestStatus": manifest.get("status", "missing"), "binaryCorpus": inventory, "differential": {"decoder": "not-run", "firstDivergence": None, "observations": []}}
    observations: list[dict[str, Any]] = []; first_divergence: dict[str, Any] | None = None
    for case_id in AUTHORITY_CASE_IDS:
        directory = cases[case_id]
        input_path = directory / ("expected/canonical.pb" if not case_id.startswith("BG-N") else "input/value.pb")
        expected = _expected(case_id, directory)
        actual = _observe(probe, _root_type(directory, cases), input_path)
        divergence = _first_difference(case_id, expected, actual)
        observations.append({"caseId": case_id, "expected": expected, "actual": actual, "pass": divergence is None})
        if first_divergence is None and divergence is not None: first_divergence = divergence
    return {"format": "axiom-gt-g1-02-semantic-differential-v2", "taskId": "GT-G1-02", "status": "PASS" if first_divergence is None else "FAIL", "blockingReasons": [], "manifestStatus": manifest.get("status", "missing"), "binaryCorpus": inventory, "differential": {"decoder": "production SemanticCodec golden probe", "firstDivergence": first_divergence, "observations": observations}}


def main() -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("--root", type=Path, default=DEFAULT_ROOT); parser.add_argument("--probe", type=Path); parser.add_argument("--output", type=Path); args = parser.parse_args()
    result = run_differential(args.root, args.probe); encoded = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output: args.output.parent.mkdir(parents=True, exist_ok=True); args.output.write_text(encoded, encoding="utf-8")
    else: print(encoded, end="")
    return 0 if result["status"] == "PASS" else 2 if result["status"].startswith("BLOCKED") else 1


if __name__ == "__main__": raise SystemExit(main())
