#!/usr/bin/env python3
"""Fail-closed proof that production geometry weights are YAML-generated."""

from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECTION = ROOT / "schema/axiom/v1/canonical/geometry_accounting_v1.yaml"
HEADER = ROOT / "runtime/semantic/include/canvas/semantic/geometry_accounting_v1.hpp"
VALIDATOR = ROOT / "runtime/semantic/src/validator.cpp"
GENERATOR = ROOT / "tools/generate_geometry_accounting_projection.py"


def _load_generator():
    spec = importlib.util.spec_from_file_location("geometry_projection_generator", GENERATOR)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load geometry projection generator")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def validate(root: Path = ROOT, generated_header: Path | None = None) -> list[str]:
    root = root.resolve()
    projection = root / PROJECTION.relative_to(ROOT)
    header = generated_header or root / HEADER.relative_to(ROOT)
    generator = _load_generator()
    failures: list[str] = []
    try:
        data = generator.load_yaml(projection)
        expected = generator.render(data)
    except (OSError, RuntimeError, KeyError, TypeError) as error:
        return [f"projection parse/generation failed: {error}"]
    if not header.is_file():
        return [f"missing production projection header: {header}"]
    actual = header.read_text(encoding="utf-8")
    if actual != expected:
        failures.append("production geometry projection header is not the deterministic YAML output")

    validator_source = (root / VALIDATOR.relative_to(ROOT)).read_text(encoding="utf-8")
    for constant in (
        "kLimit", "kMoveToPoint", "kLineToEnd", "kQuadTo", "kCubicTo",
        "kStrokeSample", "kDabInstance", "kEraseCubicSegment",
    ):
        if f"geometry_accounting_v1::{constant}" not in validator_source:
            failures.append(f"validator does not consume generated production constant: {constant}")

    validator_spec = importlib.util.spec_from_file_location(
        "geometry_projection_validator", root / "verification/tools/validate_geometry_accounting_projection.py"
    )
    if validator_spec is None or validator_spec.loader is None:
        return failures + ["cannot load geometry projection validator"]
    validator = importlib.util.module_from_spec(validator_spec)
    validator_spec.loader.exec_module(validator)
    failures.extend(f"projection validation: {failure}" for failure in validator.validate(root))

    carriers = data.get("carriers", {})
    expected_weights = {
        "kLimit": data.get("limit", {}).get("value"),
        "kMoveToPoint": carriers.get("MoveTo.point"),
        "kLineToEnd": carriers.get("LineTo.end"),
        "kQuadTo": carriers.get("QuadTo.control", 0) + carriers.get("QuadTo.end", 0),
        "kCubicTo": carriers.get("CubicTo.control1", 0) + carriers.get("CubicTo.control2", 0) + carriers.get("CubicTo.end", 0),
        "kStrokeSample": carriers.get("StrokeSample.position"),
        "kDabInstance": sum(carriers.get(key, 0) for key in ("DabInstance.center", "DabInstance.size", "DabInstance.rotation", "DabInstance.opacity")),
        "kEraseCubicSegment": 2 * carriers.get("EraseKnot.position", 0) + 2 * carriers.get("EraseKnot.radius", 0) + carriers.get("EraseCubicSegment.control1", 0) + carriers.get("EraseCubicSegment.control2", 0),
    }
    for name, value in expected_weights.items():
        marker = f"{name} = {value}U;"
        if marker not in actual:
            failures.append(f"production mapping drift: {marker}")

    required_operations = {
        "InsertObjects", "DeleteObjects", "RestoreObjects", "SetPlacements", "SetTransforms",
        "PatchProperties", "SetObjectSize", "SetVectorPathGeometry", "SetImageContent", "AddStroke",
        "SplitStrokes", "AddEraseMasks", "RemoveEraseMasks", "EditRichText", "SetConnectorContent",
    }
    if set(data.get("operations", {})) != required_operations:
        failures.append("operation aggregation inventory drift")
    if data.get("ownership", {}).get("normative_exact_validation") != "A3":
        failures.append("A3 ownership marker drift")
    if data.get("ownership", {}).get("early_raw_wire_rejection_when_provable") != "A2":
        failures.append("A2 ownership marker drift")
    if data.get("source_authority", {}).get("notion_page_id") != "3c94c57a-590c-81e1-aea7-eae7ea8a8c88":
        failures.append("human authority identity drift")
    if data.get("version") != 1 or data.get("status") != "frozen":
        failures.append("projection version/status drift")
    return failures


def main() -> int:
    failures = validate()
    if failures:
        print("\n".join(failures))
        return 1
    print("geometry production binding: PASS (YAML-generated C++ projection matches frozen carriers and operations)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
