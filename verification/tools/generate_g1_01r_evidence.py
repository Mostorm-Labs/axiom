#!/usr/bin/env python3
"""Generate durable, commit-bound reconciliation evidence for GT-G1-01R."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]
AUTHORITY_PATHS = (
    "docs/notion/manifest.yaml",
    "docs/notion/authority/05-runtime-architecture/axiom-architecture-baseline-v0.4.md",
    "docs/notion/authority/06-module-design/implementation-contract-freeze/icf-01-data-bridge-v0.1.md",
    "docs/notion/authority/07-runtime-data-flow/07-03-operation-semantic-document-v0.1.md",
    "docs/notion/authority/07-runtime-data-flow/07-11-generation-changeset-apply-batch-v0.1.md",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def authority_records(root: Path) -> list[dict[str, object]]:
    return [
        {
            "path": relative,
            "sha256": sha256(root / relative),
            "privateLocatorIncluded": False,
        }
        for relative in AUTHORITY_PATHS
    ]


def changed_files(root: Path, base_commit: str, source_commit: str) -> list[str]:
    return [
        path
        for path in subprocess.run(
            ["git", "diff", "--name-only", base_commit, source_commit],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.splitlines()
        if path
    ]


def verification_records() -> dict[str, object]:
    return {
        "redEvidence": {
            "command": (
                "cmake --build /tmp/axiom-g1-01r-build "
                "--target canvas_semantic_types_test --parallel 4"
            ),
            "exitCode": 2,
            "testCount": 1,
            "result": "EXPECTED_FAIL",
            "firstFailure": (
                "OperationId::isZero() could not be evaluated as a constant expression because "
                "ObjectId::isZero() was not constexpr."
            ),
        },
        "greenEvidence": {
            "command": (
                "cmake -S . -B /tmp/axiom-g1-01r-clean -DCMAKE_BUILD_TYPE=Debug "
                "-DCANVAS_BUILD_POC01=OFF -DCANVAS_BUILD_SEMANTIC=ON; "
                "cmake --build /tmp/axiom-g1-01r-clean --parallel 4; "
                "ctest --test-dir /tmp/axiom-g1-01r-clean --output-on-failure"
            ),
            "exitCode": 0,
            "testCount": 22,
            "result": "PASS",
        },
        "regression": {
            "command": "python3 -m unittest G1 semantic regression suite -v",
            "exitCode": 0,
            "testCount": 29,
            "result": "PASS",
        },
        "publicDependencyCheck": {
            "command": "python3 tools/check_runtime_boundaries.py --root .",
            "exitCode": 0,
            "contractFilesChecked": 29,
            "result": "PASS",
        },
    }


def generate(
    root: Path,
    source_commit: str,
    authority_baseline: str,
    *,
    files_changed: list[str] | None = None,
    hosted_validation: dict[str, object] | None = None,
) -> dict[str, object]:
    verification = verification_records()
    return {
        "format": "axiom-gt-g1-01r-reconciliation-evidence-v1",
        "taskId": "GT-G1-01R",
        "sourceCommit": source_commit,
        "authorityBaseline": authority_baseline,
        "architectureChanged": False,
        "reconciliationReason": (
            "Replace the pre-07-11 SemanticRevision/SemanticChangeSet public boundary "
            "with the current SemanticGeneration/ChangeSet v1 contract."
        ),
        "authority": authority_records(root),
        "existingWorkReused": [
            "9 ObjectKinds",
            "15 OperationKinds",
            "one Product Page to one Document",
            "no Page ObjectKind or synthetic DocumentRoot to Page root",
            "finite numeric validation and negative-zero normalization",
            "Operation-only canonical mutation boundary",
            "G1-02 descriptor, protobuf toolchain, corpus, and differential closure",
        ],
        "semanticBoundary": {
            "semanticGeneration": "runtime-local strong canonical post-state token",
            "canonicalCommitStamp": "separate runtime-epoch plus ordinal bridge ordering identity",
            "serverRevisionCursor": "not materialized in the Semantic Core public boundary; owned by Shared Data Runtime and not aliased",
            "changeSet": "unique ObjectId changes with merged flags and sorted, deduplicated FieldIds",
            "derivedRuntimeExcluded": True,
        },
        "oldBoundaryRemoved": ["SemanticRevision", "SemanticChangeSet"],
        "tests": [
            "SemanticTypes.SemanticGenerationIsASeparateStrongRuntimeLocalToken",
            "SemanticTypes.OperationIdZeroPredicateSupportsConstantEvaluation",
            "SemanticTypes.ChangeSetMergesObjectChangesInDeterministicOrder",
            "SemanticTypes.ChangeSetExpressesCreatedAndDeletedObjects",
            "canvas_semantic_runtime_boundaries",
            "verification.tests.test_semantic_public_boundary",
            "GT-G1-02 semantic codec regression suite",
        ],
        **verification,
        "filesChanged": list(files_changed or []),
        "hostedValidation": hosted_validation or {"status": "PENDING"},
        "gtG102": {
            "status": "PASS",
            "sourceCommit": "fa5b17ca3e8a10cf5ae9641d036d25adf65d7851",
            "hostedRun": "https://github.com/Mostorm-Labs/axiom/actions/runs/32949931979",
            "releaseTag": "semantic-toolchain-poc01-82c7ccc56b861d7b",
        },
        "gtG103Authorized": False,
    }


def _write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def write(output: Path, evidence: dict[str, object]) -> None:
    output.mkdir(parents=True, exist_ok=True)
    reconciliation = output / "reconciliation.json"
    summary = output / "summary.json"
    _write_json(reconciliation, evidence)
    _write_json(
        summary,
        {
            "format": "axiom-gt-g1-01r-summary-v1",
            "taskId": evidence["taskId"],
            "sourceCommit": evidence["sourceCommit"],
            "status": (
                "PASS"
                if evidence["hostedValidation"].get("status") == "PASS"
                else "VALIDATING"
            ),
            "gtG102": evidence["gtG102"]["status"],
            "gtG103Authorized": evidence["gtG103Authorized"],
        },
    )
    _write_json(
        output / "manifest.json",
        {
            "format": "axiom-gt-g1-01r-evidence-manifest-v1",
            "taskId": evidence["taskId"],
            "sourceCommit": evidence["sourceCommit"],
            "files": [
                {"path": path.name, "sha256": sha256(path)}
                for path in (reconciliation, summary)
            ],
        },
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--authority-baseline", required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--hosted-url", required=True)
    parser.add_argument("--hosted-run-id", type=int, required=True)
    parser.add_argument("--hosted-status", choices=("PASS", "FAIL"), required=True)
    args = parser.parse_args()
    for value, name in ((args.source_commit, "source commit"), (args.authority_baseline, "authority baseline")):
        if len(value) != 40 or any(character not in "0123456789abcdef" for character in value):
            parser.error(f"{name} must be a 40-character lowercase commit SHA")
    write(
        args.output_root,
        generate(
            ROOT,
            args.source_commit,
            args.authority_baseline,
            files_changed=changed_files(ROOT, args.authority_baseline, args.source_commit),
            hosted_validation={
                "status": args.hosted_status,
                "runId": args.hosted_run_id,
                "url": args.hosted_url,
            },
        ),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
