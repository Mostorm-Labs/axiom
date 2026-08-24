#!/usr/bin/env python3
"""Axiom semantic conformance coordinator bootstrap.

MR-10-01 adds fail-closed validation for verification artifact contracts while
keeping semantic adapter execution blocked until implementations/fixtures are
materialized.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "golden" / "v1"
SCHEMAS = ROOT / "schemas"

STAGES = {
    "DECODE", "NORMALIZE", "VALIDATE", "APPLY", "PROJECTION", "ENCODE",
    "REPLAY", "ORDER_KEY_COMPARE", "ORDER_KEY_ALLOCATE", "HARNESS",
}
REQUIREMENT_STATUSES = {
    "SPEC_REQUIREMENT", "FREEZE_CANDIDATE", "BENCHMARK_TARGET",
    "EXPERIMENTAL_TARGET", "OPEN",
}
IMPLEMENTATION_KINDS = {"CPP_NATIVE", "WASM", "TS_REFERENCE"}


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def seed_suite():
    return load_json(GOLDEN / "suites" / "seed-v0.1.json")


def _require_keys(value: dict, required: set[str], *, label: str) -> None:
    missing = required - set(value)
    if missing:
        raise ValueError(f"{label}: missing required fields: {sorted(missing)}")


def _reject_unknown(value: dict, allowed: set[str], *, label: str) -> None:
    unknown = set(value) - allowed
    if unknown:
        raise ValueError(f"{label}: unknown top-level fields: {sorted(unknown)}")


def validate_schema_inventory() -> None:
    required = {
        "corpus.schema.json", "suite.schema.json", "case.schema.json",
        "projection.schema.json", "observation.schema.json",
        "result.schema.json", "run.schema.json",
    }
    missing = sorted(name for name in required if not (SCHEMAS / name).is_file())
    if missing:
        raise ValueError(f"missing verification schemas: {missing}")
    for name in sorted(required):
        schema = load_json(SCHEMAS / name)
        if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            raise ValueError(f"{name}: must use JSON Schema Draft 2020-12")
        if not schema.get("$id", "").startswith("urn:auditoryworks:axiom:verification:"):
            raise ValueError(f"{name}: invalid verification schema $id")


def validate_case_semantics(case: dict) -> None:
    allowed = {
        "formatVersion", "id", "title", "status", "category", "authorityRefs",
        "entrypoint", "requiredCapabilities", "input", "expected", "capture",
        "blockedByOpenPolicy", "notes",
    }
    _reject_unknown(case, allowed, label="case")
    _require_keys(case, allowed, label="case")
    if case["formatVersion"] != 1:
        raise ValueError("case: formatVersion must be 1")
    if case["status"] not in REQUIREMENT_STATUSES:
        raise ValueError("case: invalid requirement status")
    if case["blockedByOpenPolicy"]:
        if case["status"] != "OPEN" or case["expected"].get("outcome") != "UNSPECIFIED":
            raise ValueError("OPEN case blocked by policy must use status=OPEN and expected outcome=UNSPECIFIED")


def validate_observation_semantics(obs: dict) -> None:
    allowed = {
        "format", "formatVersion", "runnerProtocolVersion", "vectorId",
        "implementationId", "implementationKind", "execution", "terminal",
        "stages", "artifacts", "checkpoints", "diagnostics",
    }
    _reject_unknown(obs, allowed, label="observation")
    _require_keys(obs, allowed, label="observation")
    if obs["format"] != "axiom-conformance-observation-v1" or obs["formatVersion"] != 1:
        raise ValueError("observation: invalid format/version")
    if obs["implementationKind"] not in IMPLEMENTATION_KINDS:
        raise ValueError("observation: invalid implementationKind")
    stage_names = [stage["stage"] for stage in obs["stages"]]
    if len(stage_names) != len(set(stage_names)):
        raise ValueError("observation: duplicate stage")
    if any(stage not in STAGES for stage in stage_names):
        raise ValueError("observation: unknown stage")
    indices = [checkpoint["operationIndex"] for checkpoint in obs["checkpoints"]]
    if any(a >= b for a, b in zip(indices, indices[1:])):
        raise ValueError("observation: checkpoints must be strictly ascending by operationIndex")


def validate_divergence_semantics(divergence: dict) -> None:
    basis = divergence.get("basis")
    observed = divergence.get("observed", [])
    if basis == "GOLDEN":
        reference = divergence.get("reference")
        if not reference or reference.get("source") != "GOLDEN":
            raise ValueError("GOLDEN divergence requires GOLDEN reference")
    elif basis == "CROSS_IMPLEMENTATION":
        if "reference" in divergence:
            raise ValueError("CROSS_IMPLEMENTATION divergence reference must be absent")
        if len(observed) < 2:
            raise ValueError("CROSS_IMPLEMENTATION divergence requires at least two observed operands")
    else:
        raise ValueError("divergence: invalid basis")
    has_index = "operationIndex" in divergence
    has_id = "operationId" in divergence
    if has_index != has_id:
        raise ValueError("divergence: operationIndex and operationId must appear together")


def validate_result_semantics(result: dict) -> None:
    allowed = {
        "format", "formatVersion", "vectorId", "requirementStatus", "result",
        "participants", "comparison", "divergence", "diagnostics",
    }
    _reject_unknown(result, allowed, label="result")
    _require_keys(result, allowed, label="result")
    if result["format"] != "axiom-conformance-result-v1" or result["formatVersion"] != 1:
        raise ValueError("result: invalid format/version")
    if result["requirementStatus"] not in REQUIREMENT_STATUSES:
        raise ValueError("result: invalid requirement status")
    status = result["result"]
    divergence = result["divergence"]
    if divergence is not None:
        validate_divergence_semantics(divergence)
    if status == "PASS" and divergence is not None:
        raise ValueError("PASS requires divergence=null")
    if status == "FAIL_GOLDEN_MISMATCH" and (divergence is None or divergence.get("basis") != "GOLDEN"):
        raise ValueError("FAIL_GOLDEN_MISMATCH requires GOLDEN divergence")
    if status == "OBSERVED_DIVERGENCE_OPEN" and (divergence is None or divergence.get("basis") != "CROSS_IMPLEMENTATION"):
        raise ValueError("OBSERVED_DIVERGENCE_OPEN requires CROSS_IMPLEMENTATION divergence")
    if status in {"BLOCKED_OPEN", "OBSERVED_AGREEMENT_OPEN"} and divergence is not None:
        raise ValueError(f"{status} requires divergence=null")
    participations = {p.get("participation") for p in result["participants"]}
    if status == "FAIL_CAPABILITY_MISSING" and not participations.intersection({"NOT_CAPABLE", "MISSING"}):
        raise ValueError("FAIL_CAPABILITY_MISSING requires NOT_CAPABLE or MISSING participant")
    if status == "FAIL_IMPLEMENTATION_ERROR" and "IMPLEMENTATION_ERROR" not in participations:
        raise ValueError("FAIL_IMPLEMENTATION_ERROR requires IMPLEMENTATION_ERROR participant")
    for participant in result["participants"]:
        if participant.get("participation") == "RAN" and "observation" not in participant:
            raise ValueError("RAN participant requires observation")
        if participant.get("participation") != "RAN" and "observation" in participant:
            raise ValueError("non-RAN participant must omit observation")


def validate_metadata() -> None:
    validate_schema_inventory()
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
        print("corpus metadata + MR-10-01 schema inventory: OK")
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
