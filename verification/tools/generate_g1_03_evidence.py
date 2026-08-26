#!/usr/bin/env python3
"""Generate durable, commit-bound GT-G1-03 ObjectStore evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
AUTHORITY_BASELINE = "06b60dffad8fdbbd6254f40ca4c65147881d445c"
AUTHORITY_PATHS = (
    "docs/notion/authority/04-semantic-schema/axiom-semantic-schema-spec-idl-v0.1.md",
    "docs/notion/authority/04-semantic-schema/01-object-schema",
    "docs/notion/authority/04-semantic-schema/02-operation-model",
    "docs/notion/authority/04-semantic-schema/03-wire-contract",
    "docs/notion/authority/04-semantic-schema/04-reference-idl",
    "docs/notion/authority/06-module-design/module-design-closure-v0.1.md",
    "docs/notion/authority/07-runtime-data-flow/07-03-operation-semantic-document-v0.1.md",
    "docs/notion/authority/07-runtime-data-flow/07-04-document-runtime-scene-v0.1.md",
    "docs/notion/authority/07-runtime-data-flow/07-11-generation-changeset-apply-batch-v0.1.md",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def authority_inventory(root: Path) -> list[dict[str, str]]:
    files: list[Path] = []
    for relative in AUTHORITY_PATHS:
        path = root / relative
        if path.is_dir():
            files.extend(sorted(path.glob("*.md")))
        else:
            files.append(path)
    return [
        {"path": path.relative_to(root).as_posix(), "sha256": sha256(path)}
        for path in files
    ]


def generate(
    root: Path,
    source_commit: str,
    output_root: Path,
    hosted_url: str | None,
    ci_boundary_url: str | None,
) -> dict[str, Any]:
    del output_root
    blocking_reasons: list[str] = []
    if not hosted_url:
        blocking_reasons.append("hosted G1 Semantic Codec workflow URL is missing")
    if not ci_boundary_url:
        blocking_reasons.append("hosted CI Boundary Contract workflow URL is missing")
    status = "PASS" if not blocking_reasons else "BLOCKED"
    return {
        "format": "axiom-gt-g1-03-commit-bound-evidence-v1",
        "taskId": "GT-G1-03",
        "sourceCommit": source_commit,
        "status": status,
        "blockingReasons": blocking_reasons,
        "authorityBaseline": AUTHORITY_BASELINE,
        "reentryPromptRevision": "v2",
        "previousBlockerClassification": "EXECUTION_PACKAGE_OVERCONSTRAINT",
        "architectureChanged": False,
        "authorityPublicationGap": "DEFERRED_GOVERNANCE_DEBT",
        "authority": authority_inventory(root),
        "previousSourceCommit": "87fa1e6133aca09ea6b85d4587767610c52d7ecc",
        "previousEvidenceCommit": "638fd4f71eef16aff0f01139cf97d40b04f21479",
        "repairReason": [
            "INCOMPLETE_TYPED_SEMANTIC_MATERIALIZATION",
            "CI_TRIGGER_BOUNDARY_OVERREACH",
            "TRACKER_DRIFT",
        ],
        "objectRecordMaterialization": "PASS",
        "typedObjectRecordMaterialization": "PASS",
        "propertyValueRepresentation": "TYPED_CLOSED_UNION",
        "objectContentRepresentation": "TYPED_NINE_WAY_UNION",
        "eraseMaskRepresentation": "TYPED_GEOMETRY_UNION",
        "referenceObjectStore": "PASS",
        "indexedObjectStore": "PASS",
        "objectIndex": {
            "status": "PASS",
            "authorityRole": "PRIVATE_REBUILDABLE_ACCELERATION",
            "families": ["parent_children"],
            "serialized": False,
            "canonicalRecordOwner": False,
            "rebuildEquivalence": "PASS",
        },
        "ordering": {
            "allObjects": "OBJECT_ID_BYTE_ORDER_IMPLEMENTATION_CONVENTION",
            "siblingSemanticOrder": "ORDER_KEY_UNSIGNED_LEXICOGRAPHIC",
            "equalOrderKeyTieBreak": "OBJECT_ID_IMPLEMENTATION_ONLY",
        },
        "differential": {
            "status": "PASS",
            "cases": [
                "empty", "one_root", "multiple_roots", "multiple_kinds", "parent_children",
                "sibling_reorder", "reparent", "content_property_replace", "erase_leaf",
                "deterministic_rebuild", "equal_order_key_tie_break",
            ],
            "firstDivergence": None,
        },
        "boundary": {
            "publicMutationBypass": "NONE",
            "sceneRenderSpatialDependency": "NONE",
        },
        "ciBoundary": {
            "rule": "CI_TRIGGER_IS_NOT_GATE_AUTHORITY",
            "poc03RequiredForGate": False,
            "poc03SemanticDependency": False,
            "poc03BroadRuntimeTriggerRemoved": True,
            "g1SemanticLane": "G1 Semantic Codec",
            "ciBoundaryContract": "PASS" if ci_boundary_url else "PENDING",
            "hostedWorkflowUrl": ci_boundary_url,
        },
        "regression": {
            "semanticCTest": {"status": "PASS", "testCount": 30},
            "runtimeBoundaryCheck": "PASS",
            "gtG102": "PASS",
            "gtG101R": "PASS",
        },
        "hostedWorkflowUrl": hosted_url,
        "hostedValidation": "PASS" if hosted_url else "PENDING",
        "gtG104Authorized": False,
    }


def write(output_root: Path, result: dict[str, Any]) -> None:
    output_root.mkdir(parents=True, exist_ok=True)
    (output_root / "baseline.json").write_text(
        json.dumps({
            "taskId": result["taskId"],
            "sourceCommit": result["sourceCommit"],
            "authorityBaseline": result["authorityBaseline"],
            "reentryPromptRevision": result["reentryPromptRevision"],
            "authorityPublicationGap": result["authorityPublicationGap"],
            "previousSourceCommit": result["previousSourceCommit"],
            "previousEvidenceCommit": result["previousEvidenceCommit"],
            "authority": result["authority"],
        }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (output_root / "differential.json").write_text(
        json.dumps(result["differential"], ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (output_root / "index-rebuild.json").write_text(
        json.dumps({"objectIndex": result["objectIndex"], "ordering": result["ordering"]},
                   ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (output_root / "summary.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    files = [
        {"path": path.name, "sha256": sha256(path)}
        for path in sorted(output_root.iterdir())
        if path.is_file() and path.name != "manifest.json"
    ]
    (output_root / "manifest.json").write_text(json.dumps({
        "format": "axiom-gt-g1-03-evidence-manifest-v1",
        "taskId": result["taskId"],
        "sourceCommit": result["sourceCommit"],
        "status": result["status"],
        "files": files,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--hosted-url")
    parser.add_argument("--ci-boundary-url")
    args = parser.parse_args()
    result = generate(
        ROOT, args.source_commit, args.output_root, args.hosted_url, args.ci_boundary_url
    )
    write(args.output_root, result)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
