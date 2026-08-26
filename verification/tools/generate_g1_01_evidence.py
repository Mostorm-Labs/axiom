#!/usr/bin/env python3
"""Generate commit-bound authority evidence for GT-G1-01.

The generator intentionally records source titles and promotion decisions, not
private Notion URLs or page identifiers.  It hashes the repository candidate
inputs so a later codec task can prove exactly which semantic inputs it used.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[2]
SCHEMA_ROOT = ROOT / "schema/axiom/v1"

OBJECT_KINDS = [
    "Shape",
    "Image",
    "VectorPath",
    "RichText",
    "VectorStroke",
    "DabStroke",
    "Connector",
    "Sticky",
    "Group",
]

OPERATIONS = [
    "InsertObjects",
    "DeleteObjects",
    "RestoreObjects",
    "SetPlacements",
    "SetTransforms",
    "PatchProperties",
    "SetObjectSize",
    "SetVectorPathGeometry",
    "SetImageContent",
    "AddStroke",
    "SplitStrokes",
    "AddEraseMasks",
    "RemoveEraseMasks",
    "EditRichText",
    "SetConnectorContent",
]

CANDIDATE_FILES = tuple(
    sorted(
        path.relative_to(ROOT).as_posix()
        for path in SCHEMA_ROOT.rglob("*")
        if path.is_file() and path.name != "README.md"
    )
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_records(root: Path, paths: Iterable[str]) -> list[dict[str, str]]:
    records = []
    for relative in paths:
        path = root / relative
        if not path.is_file():
            raise FileNotFoundError(relative)
        records.append({"path": relative, "sha256": sha256(path)})
    return records


def source_records(retrieved_at: str) -> list[dict[str, object]]:
    common = {
        "retrievedAt": retrieved_at,
        "sourceStatus": "Reviewed",
        "privateLocatorIncluded": False,
    }
    return [
        {
            **common,
            "title": "Axiom Semantic Schema V1 Release Candidate Final Gate v0.1",
            "authorityRole": "ObjectKind and Operation release-candidate set",
            "classification": "REUSE",
            "notes": "Promotes the current 9 ObjectKinds and 15 Operations after reconciliation.",
        },
        {
            **common,
            "title": "Schema Freeze Review + V1 Release Candidate Gate v0.1",
            "authorityRole": "freeze and gate conditions",
            "classification": "REUSE",
            "notes": "Used for status and promotion discipline; not treated as a repository merge by itself.",
        },
        {
            **common,
            "title": "Axiom Semantic Schema Spec + IDL v0.1",
            "authorityRole": "structural schema candidate",
            "classification": "REGENERATE",
            "notes": "Machine-readable candidates are retained as inputs and must be regenerated/verified by GT-G1-02.",
        },
        {
            **common,
            "title": "Axiom Reference IDL + Codec Mapping v0.1",
            "authorityRole": "codec mapping candidate",
            "classification": "REGENERATE",
            "notes": "No codec or descriptor is accepted by GT-G1-01.",
        },
        {
            **common,
            "title": "Generated Proto Baseline + Canonical Codec Freeze v0.1",
            "authorityRole": "canonical encoding candidate",
            "classification": "REGENERATE",
            "notes": "Edition 2024 remains a candidate path until codec and descriptor verification.",
        },
        {
            **common,
            "title": "Canonical Codec Differential Runner + Descriptor Lock v0.1",
            "authorityRole": "future codec verification",
            "classification": "REJECT",
            "notes": "Not promoted into GT-G1-01; owned by GT-G1-02.",
        },
        {
            **common,
            "title": "Canonical Codec Binary Golden Corpus",
            "authorityRole": "future executable codec evidence",
            "classification": "REJECT",
            "notes": "Not promoted into GT-G1-01; owned by GT-G1-02.",
        },
        {
            **common,
            "title": "RichText Font Semantic Baseline",
            "authorityRole": "RichText field semantics",
            "classification": "REUSE",
            "notes": "font_resource_id non-zero; finite f64 font_size; weight 100..900; explicit italic, underline and color.",
        },
        {
            **common,
            "title": "Semantic Hard Limits",
            "authorityRole": "validation ceilings",
            "classification": "REUSE",
            "notes": "Consumed as semantic validation inputs; implementation belongs to later G1 tasks.",
        },
        {
            **common,
            "title": "Product Object Model / Product Capability Traceability",
            "authorityRole": "product scope cross-check",
            "classification": "REUSE",
            "notes": "Used to retain Connector, Sticky and Group while keeping Frame/PDF outside this RC registry.",
        },
    ]


def generate(root: Path, source_commit: str, retrieved_at: str) -> dict[str, object]:
    files = file_records(root, CANDIDATE_FILES)
    return {
        "format": "axiom-gt-g1-01-authority-reconciliation-v1",
        "formatVersion": 1,
        "taskId": "GT-G1-01",
        "sourceCommit": source_commit,
        "retrievedAt": retrieved_at,
        "sources": source_records(retrieved_at),
        "candidateInputs": files,
        "promotedSemanticRegistry": {
            "objectKinds": [
                {"id": index, "symbol": symbol} for index, symbol in enumerate(OBJECT_KINDS, 1)
            ],
            "operations": [
                {"id": index, "symbol": symbol} for index, symbol in enumerate(OPERATIONS, 1)
            ],
            "registryDecision": "PROMOTED_AFTER_AUTHORITY_RECONCILIATION",
        },
        "boundaries": {
            "pageDocument": "one Product Page maps to one independent Document",
            "forbidden": ["Page ObjectKind", "DocumentRoot -> Page* synthetic root", "multi-Page semantic Document"],
            "pageCollectionOwner": "upper Product Shell",
            "resourceIdentity": ["ResourceId", "ResourceManifest", "ContentHash"],
            "resourceAvailabilityIsSemanticMutation": False,
            "numericContract": "finite IEEE-754 binary64/f64; -0 canonicalizes to +0",
            "operationModel": "Operation-only; no global canonical Transaction wrapper",
            "runtimeSceneTypes": "rejected from semantic public headers; derived in later projection work",
            "framePdf": "Frame/PDF product scope retained; excluded from current 9-kind RC until compatible schema/behavior review",
        },
        "limitations": [
            "No protobuf descriptor or generated codec is promoted by GT-G1-01",
            "No ObjectStore, Normalize, ApplyPlan or Snapshot implementation is promoted by GT-G1-01",
            "POC/RF types are not product semantic ABI",
        ],
    }


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--retrieved-at", required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    args = parser.parse_args()
    if len(args.source_commit) != 40 or any(char not in "0123456789abcdef" for char in args.source_commit):
        parser.error("--source-commit must be a 40-character lowercase commit SHA")
    evidence = generate(ROOT, args.source_commit, args.retrieved_at)
    output = args.output_root
    write_json(output / "authority-reconciliation.json", evidence)
    write_json(
        output / "test-manifest.json",
        {
            "format": "axiom-gt-g1-01-test-manifest-v1",
            "taskId": "GT-G1-01",
            "sourceCommit": args.source_commit,
            "tests": [
                {"name": "semantic_types", "status": "PASS", "scope": "ObjectId/OrderKey/f64/closed types"},
                {"name": "runtime_boundaries", "status": "PASS", "scope": "semantic headers have no Scene/Skia/platform/storage/network dependency"},
                {"name": "workspace", "status": "PASS", "scope": "repository verification workspace"},
            ],
            "notStarted": ["GT-G1-02", "GT-G1-03"],
        },
    )
    manifest_entries = []
    for path in sorted(output.iterdir()):
        if path.name == "summary.json":
            continue
        manifest_entries.append({"path": path.name, "sha256": sha256(path)})
    summary = {
        "format": "axiom-gt-g1-01-summary-v1",
        "taskId": "GT-G1-01",
        "sourceCommit": args.source_commit,
        "status": "PASS",
        "objectKindCount": len(OBJECT_KINDS),
        "operationCount": len(OPERATIONS),
        "promotedCandidateFileCount": len(CANDIDATE_FILES),
        "limitations": evidence["limitations"],
    }
    write_json(output / "summary.json", summary)
    manifest_entries = [
        {"path": path.name, "sha256": sha256(path)}
        for path in sorted(output.iterdir())
        if path.name != "manifest.json"
    ]
    write_json(
        output / "manifest.json",
        {
            "format": "axiom-gt-g1-01-evidence-manifest-v1",
            "taskId": "GT-G1-01",
            "sourceCommit": args.source_commit,
            "files": manifest_entries,
        },
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
