#!/usr/bin/env python3
"""Axiom semantic conformance coordinator bootstrap and artifact validator."""
from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import re
import sys

from google.protobuf import descriptor_pb2, descriptor_pool
from google.protobuf.descriptor import FieldDescriptor
from jsonschema import Draft202012Validator
from jsonschema.exceptions import SchemaError, ValidationError

ROOT = Path(__file__).resolve().parents[2]
REPO = ROOT.parent
GOLDEN = ROOT / "golden" / "v1"
SCHEMAS = ROOT / "schemas"
DEFAULT_DESCRIPTOR = REPO / "schema" / "axiom" / "v1" / "descriptor" / "axiom_v1.descriptor.pb"

STAGES = {"DECODE", "NORMALIZE", "VALIDATE", "APPLY", "PROJECTION", "ENCODE", "REPLAY", "ORDER_KEY_COMPARE", "ORDER_KEY_ALLOCATE", "HARNESS"}
REQUIREMENT_STATUSES = {"SPEC_REQUIREMENT", "FREEZE_CANDIDATE", "BENCHMARK_TARGET", "EXPERIMENTAL_TARGET", "OPEN"}
IMPLEMENTATION_KINDS = {"CPP_NATIVE", "WASM", "TS_REFERENCE"}
F32_TAG = re.compile(r"^f32:[0-9a-f]{8}$")
F64_TAG = re.compile(r"^f64:[0-9a-f]{16}$")
I64_TAG = re.compile(r"^i64:-?(?:0|[1-9][0-9]*)$")
U64_TAG = re.compile(r"^u64:[0-9a-f]{16}$")
HEX_TAG = re.compile(r"^hex:(?:[0-9a-f]{2})*$")
ID128_TAG = re.compile(r"^id128:[0-9a-f]{32}$")


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def seed_suite():
    return load_json(GOLDEN / "suites" / "seed-v0.1.json")


def validate_structure(schema_name: str, value) -> None:
    schema = load_json(SCHEMAS / f"{schema_name}.schema.json")
    try:
        Draft202012Validator.check_schema(schema)
        Draft202012Validator(schema).validate(value)
    except (SchemaError, ValidationError) as exc:
        path = "$" + "".join(f"[{part!r}]" if isinstance(part, str) else f"[{part}]" for part in getattr(exc, "absolute_path", []))
        raise ValueError(f"{schema_name} JSON Schema validation failed at {path}: {exc.message}") from exc


def validate_schema_inventory() -> None:
    required = {"corpus.schema.json", "suite.schema.json", "case.schema.json", "projection.schema.json", "observation.schema.json", "result.schema.json", "run.schema.json"}
    missing = sorted(name for name in required if not (SCHEMAS / name).is_file())
    if missing:
        raise ValueError(f"missing verification schemas: {missing}")
    for name in sorted(required):
        schema = load_json(SCHEMAS / name)
        if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            raise ValueError(f"{name}: must use JSON Schema Draft 2020-12")
        if not schema.get("$id", "").startswith("urn:auditoryworks:axiom:verification:"):
            raise ValueError(f"{name}: invalid verification schema $id")
        try:
            Draft202012Validator.check_schema(schema)
        except SchemaError as exc:
            raise ValueError(f"{name}: invalid Draft 2020-12 schema: {exc.message}") from exc


def _descriptor_path() -> Path:
    return Path(os.environ.get("AXIOM_V1_DESCRIPTOR", DEFAULT_DESCRIPTOR))


def load_descriptor_pool() -> descriptor_pool.DescriptorPool:
    path = _descriptor_path()
    if not path.is_file():
        raise ValueError(f"frozen Axiom V1 descriptor evidence not found: {path}")
    fds = descriptor_pb2.FileDescriptorSet()
    fds.ParseFromString(path.read_bytes())
    pool = descriptor_pool.DescriptorPool()
    pending = list(fds.file)
    while pending:
        progressed = False
        for file_proto in pending[:]:
            try:
                pool.Add(file_proto)
            except Exception:
                continue
            pending.remove(file_proto); progressed = True
        if not progressed:
            raise ValueError(f"unable to load descriptor dependencies: {[f.name for f in pending]}")
    return pool


def _json_name(field: FieldDescriptor) -> str:
    return field.json_name


def _validate_scalar(field: FieldDescriptor, value, path: str, form: str) -> None:
    t = field.type
    if t == FieldDescriptor.TYPE_BOOL:
        if type(value) is not bool: raise ValueError(f"{path}: expected boolean")
    elif t in {FieldDescriptor.TYPE_INT32, FieldDescriptor.TYPE_SINT32, FieldDescriptor.TYPE_SFIXED32}:
        if type(value) is not int or not -(2**31) <= value < 2**31: raise ValueError(f"{path}: expected int32")
    elif t in {FieldDescriptor.TYPE_UINT32, FieldDescriptor.TYPE_FIXED32}:
        if type(value) is not int or not 0 <= value < 2**32: raise ValueError(f"{path}: expected uint32")
    elif t in {FieldDescriptor.TYPE_INT64, FieldDescriptor.TYPE_SINT64, FieldDescriptor.TYPE_SFIXED64}:
        if form == "CANONICAL":
            if not isinstance(value, str) or not I64_TAG.fullmatch(value): raise ValueError(f"{path}: expected tagged i64 scalar")
        elif type(value) is not int: raise ValueError(f"{path}: expected int64")
    elif t in {FieldDescriptor.TYPE_UINT64, FieldDescriptor.TYPE_FIXED64}:
        if form == "CANONICAL":
            if not isinstance(value, str) or not U64_TAG.fullmatch(value): raise ValueError(f"{path}: expected tagged u64 scalar")
        elif type(value) is not int or value < 0: raise ValueError(f"{path}: expected uint64")
    elif t == FieldDescriptor.TYPE_FLOAT:
        if form == "CANONICAL":
            if not isinstance(value, str) or not F32_TAG.fullmatch(value): raise ValueError(f"{path}: expected tagged f32 scalar")
        elif type(value) not in {int, float} or not math.isfinite(value): raise ValueError(f"{path}: expected floating-point")
    elif t == FieldDescriptor.TYPE_DOUBLE:
        if form == "CANONICAL":
            if not isinstance(value, str) or not F64_TAG.fullmatch(value): raise ValueError(f"{path}: expected tagged f64 scalar")
        elif type(value) not in {int, float} or not math.isfinite(value): raise ValueError(f"{path}: expected floating-point")
    elif t == FieldDescriptor.TYPE_STRING:
        if not isinstance(value, str): raise ValueError(f"{path}: expected string")
    elif t == FieldDescriptor.TYPE_BYTES:
        if form == "CANONICAL":
            if not isinstance(value, str) or not HEX_TAG.fullmatch(value): raise ValueError(f"{path}: expected hex tagged scalar")
        elif not isinstance(value, str): raise ValueError(f"{path}: expected bytes representation string")
    elif t == FieldDescriptor.TYPE_ENUM:
        if not isinstance(value, str) or value not in field.enum_type.values_by_name: raise ValueError(f"{path}: expected enum name from {field.enum_type.full_name}")
    else:
        raise ValueError(f"{path}: unsupported scalar descriptor type {t}")


def _validate_message(desc, value, path: str, form: str) -> None:
    # Verification projection special scalars remain IDL-owned messages on wire,
    # but use frozen tagged strings in semantic projection form.
    if desc.full_name == "auditoryworks.axiom.v1.Id128":
        if not isinstance(value, str) or not ID128_TAG.fullmatch(value): raise ValueError(f"{path}: expected id128 tagged scalar")
        return
    if desc.full_name == "auditoryworks.axiom.v1.OrderKey":
        if not isinstance(value, str) or not HEX_TAG.fullmatch(value): raise ValueError(f"{path}: expected hex tagged scalar")
        return
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected object for {desc.full_name}")
    fields = {_json_name(field): field for field in desc.fields}
    unknown = sorted(set(value) - set(fields))
    if unknown: raise ValueError(f"{path}: unknown field(s) for {desc.full_name}: {unknown}")
    for oneof in desc.oneofs:
        present = [_json_name(field) for field in oneof.fields if _json_name(field) in value]
        if len(present) > 1: raise ValueError(f"{path}: oneof {oneof.name} has multiple members: {present}")
    for name, item in value.items():
        field = fields[name]; field_path = f"{path}.{name}"
        if field.is_repeated:
            if not isinstance(item, list): raise ValueError(f"{field_path}: expected array")
            for index, child in enumerate(item): _validate_field_value(field, child, f"{field_path}[{index}]", form)
        else:
            _validate_field_value(field, item, field_path, form)


def _validate_field_value(field: FieldDescriptor, value, path: str, form: str) -> None:
    if field.type == FieldDescriptor.TYPE_MESSAGE:
        _validate_message(field.message_type, value, path, form)
    else:
        _validate_scalar(field, value, path, form)


def validate_projection_against_idl(projection: dict) -> None:
    validate_structure("projection", projection)
    if projection["semanticSchemaVersion"] != 1:
        raise ValueError("projection: semanticSchemaVersion must be 1 for Axiom V1")
    pool = load_descriptor_pool()
    root_type = projection["rootType"]
    try:
        desc = pool.FindMessageTypeByName(root_type)
    except KeyError as exc:
        raise ValueError(f"projection rootType {root_type!r} not found in frozen Axiom V1 IDL") from exc
    _validate_message(desc, projection["value"], "$", projection["form"])


def _require_keys(value: dict, required: set[str], *, label: str) -> None:
    missing = required - set(value)
    if missing: raise ValueError(f"{label}: missing required fields: {sorted(missing)}")


def _reject_unknown(value: dict, allowed: set[str], *, label: str) -> None:
    unknown = set(value) - allowed
    if unknown: raise ValueError(f"{label}: unknown top-level fields: {sorted(unknown)}")


def validate_case_semantics(case: dict) -> None:
    validate_structure("case", case)
    allowed = {"formatVersion", "id", "title", "status", "category", "authorityRefs", "entrypoint", "requiredCapabilities", "input", "expected", "capture", "blockedByOpenPolicy", "notes"}
    _reject_unknown(case, allowed, label="case"); _require_keys(case, allowed, label="case")
    if case["status"] not in REQUIREMENT_STATUSES: raise ValueError("case: invalid requirement status")
    if case["blockedByOpenPolicy"] and (case["status"] != "OPEN" or case["expected"].get("outcome") != "UNSPECIFIED"):
        raise ValueError("OPEN case blocked by policy must use status=OPEN and expected outcome=UNSPECIFIED")


def validate_observation_semantics(obs: dict) -> None:
    validate_structure("observation", obs)
    allowed = {"format", "formatVersion", "runnerProtocolVersion", "vectorId", "implementationId", "implementationKind", "execution", "terminal", "stages", "artifacts", "checkpoints", "diagnostics"}
    _reject_unknown(obs, allowed, label="observation"); _require_keys(obs, allowed, label="observation")
    stage_names = [stage["stage"] for stage in obs["stages"]]
    if len(stage_names) != len(set(stage_names)): raise ValueError("observation: duplicate stage")
    indices = [checkpoint["operationIndex"] for checkpoint in obs["checkpoints"]]
    if any(a >= b for a, b in zip(indices, indices[1:])): raise ValueError("observation: checkpoints must be strictly ascending by operationIndex")


def validate_divergence_semantics(divergence: dict) -> None:
    basis = divergence.get("basis"); observed = divergence.get("observed", [])
    if basis == "GOLDEN":
        reference = divergence.get("reference")
        if not reference or reference.get("source") != "GOLDEN": raise ValueError("GOLDEN divergence requires GOLDEN reference")
    elif basis == "CROSS_IMPLEMENTATION":
        if "reference" in divergence: raise ValueError("CROSS_IMPLEMENTATION divergence reference must be absent")
        if len(observed) < 2: raise ValueError("CROSS_IMPLEMENTATION divergence requires at least two observed operands")
    else: raise ValueError("divergence: invalid basis")
    if ("operationIndex" in divergence) != ("operationId" in divergence): raise ValueError("divergence: operationIndex and operationId must appear together")


def validate_result_semantics(result: dict) -> None:
    validate_structure("result", result)
    allowed = {"format", "formatVersion", "vectorId", "requirementStatus", "result", "participants", "comparison", "divergence", "diagnostics"}
    _reject_unknown(result, allowed, label="result"); _require_keys(result, allowed, label="result")
    status = result["result"]; divergence = result["divergence"]
    if divergence is not None: validate_divergence_semantics(divergence)
    if status == "PASS" and divergence is not None: raise ValueError("PASS requires divergence=null")
    if status == "FAIL_GOLDEN_MISMATCH" and (divergence is None or divergence.get("basis") != "GOLDEN"): raise ValueError("FAIL_GOLDEN_MISMATCH requires GOLDEN divergence")
    if status == "OBSERVED_DIVERGENCE_OPEN" and (divergence is None or divergence.get("basis") != "CROSS_IMPLEMENTATION"): raise ValueError("OBSERVED_DIVERGENCE_OPEN requires CROSS_IMPLEMENTATION divergence")
    if status in {"BLOCKED_OPEN", "OBSERVED_AGREEMENT_OPEN"} and divergence is not None: raise ValueError(f"{status} requires divergence=null")
    participations = {p.get("participation") for p in result["participants"]}
    if status == "FAIL_CAPABILITY_MISSING" and not participations.intersection({"NOT_CAPABLE", "MISSING"}): raise ValueError("FAIL_CAPABILITY_MISSING requires NOT_CAPABLE or MISSING participant")
    if status == "FAIL_IMPLEMENTATION_ERROR" and "IMPLEMENTATION_ERROR" not in participations: raise ValueError("FAIL_IMPLEMENTATION_ERROR requires IMPLEMENTATION_ERROR participant")
    for participant in result["participants"]:
        if participant.get("participation") == "RAN" and "observation" not in participant: raise ValueError("RAN participant requires observation")
        if participant.get("participation") != "RAN" and "observation" in participant: raise ValueError("non-RAN participant must omit observation")


def validate_metadata() -> None:
    validate_schema_inventory()
    corpus = load_json(GOLDEN / "corpus.json"); suite = seed_suite()
    validate_structure("corpus", corpus); validate_structure("suite", suite)
    assert corpus["formatVersion"] == 1 and corpus["semanticSchemaVersion"] == 1 and corpus["requiredRunnerProtocolVersion"] == 1
    cases = suite["cases"]
    assert suite["id"] == "seed-v0.1" and len(cases) == 60 and len(set(cases)) == 60
    assert suite["implementationPolicy"] == {"cpp": "REQUIRED", "wasm": "REQUIRED", "ts": "REQUIRED_WHEN_CAPABLE"}


def main() -> int:
    parser = argparse.ArgumentParser(prog="axiom-conformance"); sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("validate-corpus"); list_p = sub.add_parser("list"); list_p.add_argument("--suite", default="seed-v0.1"); run_p = sub.add_parser("run"); run_p.add_argument("--suite", default="seed-v0.1")
    args = parser.parse_args(); validate_metadata()
    if args.command == "validate-corpus": print("corpus + JSON Schema Draft 2020-12 contracts: OK"); return 0
    if args.command == "list":
        if args.suite != "seed-v0.1": raise SystemExit(f"unknown bootstrap suite: {args.suite}")
        for case_id in seed_suite()["cases"]: print(case_id)
        return 0
    if args.command == "run":
        print("BLOCKED: semantic adapters/fixtures are not materialized yet; bootstrap refuses to fabricate conformance observations.", file=sys.stderr); return 3
    return 2


if __name__ == "__main__": raise SystemExit(main())
