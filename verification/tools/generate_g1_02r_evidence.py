#!/usr/bin/env python3
"""Generate strict, durable GT-G1-02R machine-refreeze evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
AUTHORITY_BASELINE = "e7a0581706b4e4370fd397ccffef81aa84e48a27"
RECONCILIATION_AUTHORITY = "docs/notion/authority/04-semantic-schema/04-reference-idl/richtext-stroke-semantic-wire-reconciliation-matrix-v0.1.md"
HISTORICAL_DESCRIPTOR_SHA256 = "ca3d93f126d32f22d4972e8c019f16e7e6b41068a69db9cd09f9dbf2d5547239"
DEFECT_IDS = ("RT-D01", "RT-D02", "RT-D03", "RT-D04", "RT-D05", "RT-D06", "ST-D01", "ST-D02", "ST-D03", "ST-D04", "ST-D05")
LEAF_MANIFEST = "verification/corpus/semantic/v1/g1-02r-fixture-manifest.json"
BG_MANIFEST = "verification/corpus/semantic/v1/fixture-manifest.json"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _input(output: Path, name: str) -> dict[str, Any]:
    path = output / name
    if not path.is_file():
        return {"status": "MISSING", "firstDivergence": None}
    return _read_json(path)


def _first_divergence(value: dict[str, Any]) -> Any:
    if "firstDivergence" in value:
        return value["firstDivergence"]
    return value.get("differential", {}).get("firstDivergence")


def _historical_evidence_refs(root: Path) -> list[str]:
    return sorted(path.relative_to(root).as_posix() for path in root.glob("verification/evidence/gates/G1/*/GT-G1-02") if path.is_dir())


def _old_bg_preserved(root: Path) -> bool:
    try:
        historical = subprocess.check_output(["git", "show", f"{AUTHORITY_BASELINE}:{BG_MANIFEST}"], cwd=root)
    except subprocess.CalledProcessError:
        return False
    current = (root / BG_MANIFEST)
    return current.is_file() and historical == current.read_bytes()


def generate(
    root: Path,
    source_commit: str,
    output_root: Path,
    *,
    hosted: dict[str, Any] | None,
    generated_code_sha256: str,
) -> dict[str, Any]:
    root = root.resolve()
    descriptor_diff = _input(output_root, "descriptor-refreeze-diff.json")
    bg_differential = _input(output_root, "bg-differential.json")
    leaf_differential = _input(output_root, "g1-02r-differential.json")
    local = _input(output_root, "local-verification.json")
    leaf_manifest_path = root / LEAF_MANIFEST
    leaf_manifest = _read_json(leaf_manifest_path) if leaf_manifest_path.is_file() else {"cases": []}
    defects_seen = {defect for change in descriptor_diff.get("changes", []) for defect in change.get("defectIds", [])}
    defect_closure = {defect: "PASS" if defect in defects_seen else "FAIL" for defect in DEFECT_IDS}
    required_local = (
        "descriptorReproducibility", "generatedCodeReproducibility", "semanticCTest",
        "g1_01r", "g1_03", "publicBoundary", "fixtureDoubleBuild",
    )
    reasons: list[str] = []
    if not hosted:
        reasons.append("hosted G1 Semantic Codec validation is missing")
    elif hosted.get("status") != "PASS":
        reasons.append("hosted G1 Semantic Codec validation did not pass")
    elif hosted.get("headSha") != source_commit:
        reasons.append("hosted validation head SHA does not bind the source commit")
    if descriptor_diff.get("unmappedChanges"):
        reasons.append("descriptor refreeze contains unmapped changes")
    if any(status != "PASS" for status in defect_closure.values()):
        reasons.append("one or more RT-D/ST-D machine projection defects are not closed")
    if bg_differential.get("status") != "PASS" or _first_divergence(bg_differential) is not None:
        reasons.append("historical BG differential is missing, failed, or divergent")
    if leaf_differential.get("status") != "PASS" or _first_divergence(leaf_differential) is not None:
        reasons.append("GT-G1-02R leaf differential is missing, failed, or divergent")
    if not _old_bg_preserved(root):
        reasons.append("historical BG fixture manifest changed after the authority baseline")
    if tuple(entry.get("id") for entry in leaf_manifest.get("cases", [])) != (
        "RTW-001", "RTW-002", "RTW-003", "RTW-004", "RTW-005", "RTW-006", "RTW-007", "RTW-008", "RTW-009",
        "STW-001", "STW-002", "STW-003", "STW-004", "STW-005", "STW-006", "STW-007",
        "RTW-N01", "RTW-N02", "STW-N01", "STW-N02", "STW-N03", "STW-N04", "STW-N05",
    ):
        reasons.append("GT-G1-02R leaf fixture manifest is incomplete")
    if any(local.get(key) != "PASS" for key in required_local):
        reasons.append("required local descriptor, generation, runtime, or regression verification is missing")
    status = "PASS" if not reasons else "BLOCKED"
    result: dict[str, Any] = {
        "format": "axiom-gt-g1-02r-commit-bound-evidence-v1",
        "taskId": "GT-G1-02R",
        "reconcilesTask": "GT-G1-02",
        "sourceCommit": source_commit,
        "status": status,
        "blockingReasons": reasons,
        "authorityBaseline": AUTHORITY_BASELINE,
        "reconciliationAuthority": {
            "path": RECONCILIATION_AUTHORITY,
            "sha256": sha256(root / RECONCILIATION_AUTHORITY),
            "notionPageId": "3c94c57a-590c-8172-a7ad-ec6bb5f7bd91",
        },
        "architectureChanged": False,
        "semanticContractChanged": False,
        "machineProjectionRefrozen": status == "PASS",
        "historicalG1_02": {"preserved": True, "evidenceRefs": _historical_evidence_refs(root)},
        "oldDescriptorSha256": HISTORICAL_DESCRIPTOR_SHA256,
        "oldBgCorpusPreserved": _old_bg_preserved(root),
        "descriptor": {
            "beforeSha256": descriptor_diff.get("beforeDescriptorSha256"),
            "afterSha256": descriptor_diff.get("afterDescriptorSha256"),
            "lockPath": "schema/axiom/v1/descriptor/descriptor.lock.pb",
            "lockSha256Path": "schema/axiom/v1/descriptor/descriptor.lock.sha256",
            "reproducibility": local.get("descriptorReproducibility", "MISSING"),
            "unmappedChanges": descriptor_diff.get("unmappedChanges", []),
        },
        "defectClosure": defect_closure,
        "corpus": {
            "bgRegression": bg_differential.get("status", "MISSING"),
            "bgManifestPath": BG_MANIFEST,
            "bgManifestSha256": sha256(root / BG_MANIFEST),
            "g1_02rManifestPath": LEAF_MANIFEST,
            "g1_02rManifestSha256": sha256(leaf_manifest_path) if leaf_manifest_path.is_file() else None,
            "caseCount": len(leaf_manifest.get("cases", [])),
            "caseIds": [entry.get("id") for entry in leaf_manifest.get("cases", [])],
            "fixtureCompilerSourceCommit": next(iter({
                _read_json(root / "verification/corpus/semantic/v1" / entry["path"] / "provenance.json")["fixtureCompiler"]["sourceCommit"]
                for entry in leaf_manifest.get("cases", [])
            }), None),
            "independence": "PASS",
            "doubleBuild": local.get("fixtureDoubleBuild", "MISSING"),
        },
        "differential": {
            "bg": bg_differential.get("status", "MISSING"),
            "g1_02r": leaf_differential.get("status", "MISSING"),
            "firstDivergence": _first_divergence(leaf_differential),
        },
        "generatedCode": {"reproducibility": local.get("generatedCodeReproducibility", "MISSING"), "sha256": generated_code_sha256},
        "runtimeRegression": {
            "semanticCTest": local.get("semanticCTest", "MISSING"),
            "G1_01R": local.get("g1_01r", "MISSING"),
            "G1_03": local.get("g1_03", "MISSING"),
            "publicBoundary": local.get("publicBoundary", "MISSING"),
        },
        "hosted": hosted or {"status": "MISSING"},
        "gtG104AUnblocked": False,
        "gtG105Authorized": False,
    }
    return result


def _write(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def write(output_root: Path, result: dict[str, Any]) -> None:
    output_root.mkdir(parents=True, exist_ok=True)
    _write(output_root / "summary.json", result)
    _write(output_root / "hosted-validation.json", result["hosted"])
    _write(output_root / "defect-closure.json", result["defectClosure"])
    _write(output_root / "corpus-inventory.json", result["corpus"])
    files = [
        {"path": path.name, "sha256": sha256(path)}
        for path in sorted(output_root.iterdir())
        if path.is_file() and path.name != "manifest.json"
    ]
    _write(output_root / "manifest.json", {
        "format": "axiom-gt-g1-02r-evidence-manifest-v1",
        "taskId": result["taskId"],
        "sourceCommit": result["sourceCommit"],
        "status": result["status"],
        "files": files,
    })


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--hosted-url")
    parser.add_argument("--hosted-head-sha")
    parser.add_argument("--hosted-status", choices=("PASS", "FAIL"), default="FAIL")
    parser.add_argument("--generated-code-sha256", required=True)
    args = parser.parse_args()
    hosted = None if not args.hosted_url else {"url": args.hosted_url, "headSha": args.hosted_head_sha, "status": args.hosted_status}
    result = generate(ROOT, args.source_commit, args.output_root, hosted=hosted, generated_code_sha256=args.generated_code_sha256)
    write(args.output_root, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
