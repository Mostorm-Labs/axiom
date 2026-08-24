#!/usr/bin/env python3
"""MR-10-04 platform verification artifact validators.

This module validates verification-only Platform contracts. It must not turn
08 Platform OPEN physical realization into a semantic winner.
"""
from __future__ import annotations

import re
from pathlib import Path

from jsonschema import Draft202012Validator
from jsonschema.exceptions import SchemaError, ValidationError

ROOT = Path(__file__).resolve().parents[2]
SCHEMAS = ROOT / "schemas"

PLATFORM_SCHEMA_NAMES = (
    "platform-suite",
    "platform-scenario",
    "platform-profile",
    "platform-trace",
    "platform-observation",
    "platform-result",
)
U64_TAG = re.compile(r"^u64:[0-9a-f]{16}$")
_NATIVE_HANDLE_HINT = re.compile(
    r"(?:^0x[0-9a-f]+$|\bHWND\b|ANativeWindow|CALayer|CAMetalLayer|SkSurface|MTLTexture|ID3D\w*)",
    re.IGNORECASE,
)


def _load_json(path: Path):
    import json
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def validate_platform_schema_inventory() -> None:
    missing = [name for name in PLATFORM_SCHEMA_NAMES if not (SCHEMAS / f"{name}.schema.json").is_file()]
    if missing:
        raise ValueError(f"missing platform verification schemas: {missing}")
    for name in PLATFORM_SCHEMA_NAMES:
        schema = _load_json(SCHEMAS / f"{name}.schema.json")
        if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            raise ValueError(f"{name}: must use JSON Schema Draft 2020-12")
        if schema.get("$id") != f"urn:auditoryworks:axiom:verification:{name.removeprefix('platform-') if False else name}:v1":
            expected = {
                "platform-suite": "urn:auditoryworks:axiom:verification:platform-suite:v1",
                "platform-scenario": "urn:auditoryworks:axiom:verification:platform-scenario:v1",
                "platform-profile": "urn:auditoryworks:axiom:verification:platform-profile:v1",
                "platform-trace": "urn:auditoryworks:axiom:verification:platform-trace:v1",
                "platform-observation": "urn:auditoryworks:axiom:verification:platform-observation:v1",
                "platform-result": "urn:auditoryworks:axiom:verification:platform-result:v1",
            }[name]
            if schema.get("$id") != expected:
                raise ValueError(f"{name}: unexpected schema $id {schema.get('$id')!r}")
        try:
            Draft202012Validator.check_schema(schema)
        except SchemaError as exc:
            raise ValueError(f"{name}: invalid Draft 2020-12 schema: {exc.message}") from exc


def validate_platform_structure(schema_name: str, value) -> None:
    schema = _load_json(SCHEMAS / f"{schema_name}.schema.json")
    try:
        Draft202012Validator.check_schema(schema)
        Draft202012Validator(schema).validate(value)
    except (SchemaError, ValidationError) as exc:
        path = "$" + "".join(
            f"[{part!r}]" if isinstance(part, str) else f"[{part}]"
            for part in getattr(exc, "absolute_path", [])
        )
        raise ValueError(f"{schema_name} JSON Schema validation failed at {path}: {exc.message}") from exc


def _require_unique(values, label: str) -> None:
    if len(values) != len(set(values)):
        raise ValueError(f"{label}: duplicate value")


def _u64_value(tag: str) -> int:
    if not isinstance(tag, str) or not U64_TAG.fullmatch(tag):
        raise ValueError(f"invalid exact u64 tag: {tag!r}")
    return int(tag[4:], 16)


def validate_platform_suite_semantics(suite: dict) -> None:
    validate_platform_structure("platform-suite", suite)
    _require_unique(suite["scenarios"], "platform suite scenarios")


def validate_platform_scenario_semantics(scenario: dict) -> None:
    validate_platform_structure("platform-scenario", scenario)
    _require_unique(scenario["requirementIds"], "platform scenario requirementIds")
    _require_unique([step["stepId"] for step in scenario["steps"]], "platform scenario duplicate stepId")

    expected = scenario["expected"]
    ids = []
    for collection in ("requiredEvents", "forbiddenEvents", "partialOrder", "stateAssertions", "openObservations"):
        ids.extend(item["id"] for item in expected[collection])
    _require_unique(ids, "platform scenario assertion ids")

    correctness_oracles = sum(len(expected[name]) for name in ("requiredEvents", "forbiddenEvents", "partialOrder", "stateAssertions"))
    if scenario["requirementStatus"] in {"SPEC_REQUIREMENT", "FREEZE_CANDIDATE"} and correctness_oracles == 0:
        raise ValueError("platform scenario: Spec/Freeze correctness scenario requires at least one oracle")


def validate_platform_profile_semantics(profile: dict) -> None:
    validate_platform_structure("platform-profile", profile)
    _require_unique(profile["capabilities"], "platform profile capabilities")
    # Deliberately do not enum-freeze realization values: 08 still owns physical winners.


def validate_platform_trace_semantics(trace: dict) -> None:
    validate_platform_structure("platform-trace", trace)
    seqs = [_u64_value(event["eventSeq"]) for event in trace["events"]]
    if len(seqs) != len(set(seqs)) or any(a >= b for a, b in zip(seqs, seqs[1:])):
        raise ValueError("platform trace: eventSeq must be globally unique and strictly ascending within a trace")

    for event in trace["events"]:
        kind = event["event"]
        if kind in {"SURFACE_BOUND", "SURFACE_REBOUND"}:
            if "surfaceGeneration" not in event:
                raise ValueError(f"platform trace {kind}: surfaceGeneration is required")
            if "target" not in event:
                raise ValueError(f"platform trace {kind}: target is required")
        elif kind == "METRICS_CHANGED":
            if "metricsGeneration" not in event or "metrics" not in event:
                raise ValueError("platform trace METRICS_CHANGED: metricsGeneration + metrics are required")
        elif kind in {"DEVICE_LOST", "DEVICE_RECOVERED"}:
            if "deviceGeneration" not in event:
                raise ValueError(f"platform trace {kind}: deviceGeneration is required")
        elif kind in {"DATA_BRIDGE_APPLY_ISSUED", "DATA_BRIDGE_APPLY_COMPLETED"}:
            if not (event.get("callId") or event.get("correlationId")):
                raise ValueError(f"platform trace {kind}: callId/correlationId is required")
        elif kind == "CALLBACK_DISPATCHED" and "callbackId" not in event:
            raise ValueError("platform trace CALLBACK_DISPATCHED: callbackId is required")
        elif kind == "INPUT_BATCH_DELIVERED" and "batchId" not in event:
            raise ValueError("platform trace INPUT_BATCH_DELIVERED: batchId is required")
        elif kind == "STALE_GENERATION_REJECTED" and "generationKind" not in event:
            raise ValueError("platform trace STALE_GENERATION_REJECTED: generationKind is required")


def validate_platform_observation_semantics(observation: dict, scenario: dict, profile: dict) -> None:
    validate_platform_structure("platform-observation", observation)
    validate_platform_scenario_semantics(scenario)
    validate_platform_profile_semantics(profile)

    if observation["scenarioId"] != scenario["id"]:
        raise ValueError("platform observation: scenarioId does not match scenario")
    if observation["profileId"] != profile["profileId"] or observation["platformFamily"] != profile["platformFamily"]:
        raise ValueError("platform observation: profile identity does not match profile")

    artifacts = observation["artifacts"]
    capture = scenario["capture"]
    for requested, artifact_name in (
        (capture["lifecycleTrace"], "lifecycleTrace"),
        (capture["surfaceTrace"], "surfaceTrace"),
        (capture["bridgeTrace"], "bridgeTrace"),
    ):
        if requested and artifact_name not in artifacts:
            raise ValueError(f"platform observation: requested {artifact_name} artifact is missing")

    for binding in observation["targetBindings"]:
        tag = binding["bindingTag"]
        if _NATIVE_HANDLE_HINT.search(tag):
            raise ValueError("platform observation: bindingTag must be verification-opaque and must not contain a native handle")


def validate_platform_result_semantics(result: dict, scenario: dict) -> None:
    validate_platform_structure("platform-result", result)
    validate_platform_scenario_semantics(scenario)

    if result["scenarioId"] != scenario["id"] or result["requirementStatus"] != scenario["requirementStatus"]:
        raise ValueError("platform result: scenario identity/status mismatch")
    if scenario["requirementStatus"] == "OPEN" and result["result"] == "PASS":
        raise ValueError("platform result: OPEN scenario must not produce semantic PASS")

    required_not_capable = any(
        p["policy"] == "REQUIRED" and p["participation"] in {"NOT_CAPABLE", "MISSING"}
        for p in result["participants"]
    )
    if required_not_capable and result["result"] != "FAIL_CAPABILITY_MISSING":
        raise ValueError("platform result: REQUIRED NOT_CAPABLE/MISSING participant requires FAIL_CAPABILITY_MISSING")

    if result["result"] == "FAIL_CAPABILITY_MISSING" and not required_not_capable:
        raise ValueError("platform result: FAIL_CAPABILITY_MISSING requires a REQUIRED NOT_CAPABLE/MISSING participant")
