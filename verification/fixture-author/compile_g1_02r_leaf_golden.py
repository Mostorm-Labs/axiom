#!/usr/bin/env python3
"""Materialize GT-G1-02R RichText / Stroke leaf wire goldens.

This is a verification-only author.  It accepts reviewed semantic projections
and deterministic legacy-wire recipes, then writes generic protobuf wire
bytes.  It never imports or invokes the production SemanticCodec, generated
DTOs, or a production canonical writer.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
CATALOG = "verification/fixture-author/g1_02r_leaf_authoring_v1.json"
AUTHORITY_PATHS = (
    "docs/notion/authority/04-semantic-schema/04-reference-idl/richtext-stroke-semantic-wire-reconciliation-matrix-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/richtext-wire-schema-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/brush-stroke-wire-schema-v0.1.md",
    "docs/notion/authority/10-verification/golden-authoring-fixture-pipeline-v0.1.md",
)


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _varint(value: int) -> bytes:
    if value < 0:
        raise ValueError("varint cannot encode a negative value")
    result = bytearray()
    while value > 0x7F:
        result.append((value & 0x7F) | 0x80)
        value >>= 7
    result.append(value)
    return bytes(result)


def _key(field: int, wire: int) -> bytes:
    return _varint((field << 3) | wire)


def _uint(field: int, value: int) -> bytes:
    return _key(field, 0) + _varint(value)


def _f64(field: int, value: float) -> bytes:
    return _key(field, 1) + struct.pack("<d", value)


def _fixed64(field: int, value: int) -> bytes:
    return _key(field, 1) + struct.pack("<Q", value)


def _f32(field: int, value: float) -> bytes:
    return _key(field, 5) + struct.pack("<f", value)


def _message(field: int, value: bytes) -> bytes:
    return _key(field, 2) + _varint(len(value)) + value


def _string(field: int, value: str) -> bytes:
    return _message(field, value.encode("utf-8"))


def _id(value: str) -> bytes:
    raw = bytes.fromhex(value)
    if len(raw) != 16:
        raise ValueError("fixture Id128 values must contain exactly 16 bytes")
    return _message(1, raw)


def _vec(value: dict[str, Any]) -> bytes:
    return _f64(1, float(value["x"])) + _f64(2, float(value["y"]))


def _color(value: dict[str, Any]) -> bytes:
    return b"".join(_f32(field, float(value[name])) for field, name in enumerate(("r", "g", "b", "a"), 1))


def _text_style(value: dict[str, Any]) -> bytes:
    result = bytearray()
    if "fontResourceId" in value:
        result += _message(1, _id(value["fontResourceId"]))
    if "fontSize" in value:
        result += _f64(2, float(value["fontSize"]))
    if "weight" in value:
        result += _uint(3, int(value["weight"]))
    if "italic" in value:
        result += _uint(4, int(bool(value["italic"])))
    if "underline" in value:
        result += _uint(5, int(bool(value["underline"])))
    if "color" in value:
        result += _message(6, _color(value["color"]))
    return bytes(result)


def _paragraph_style(value: dict[str, Any]) -> bytes:
    return (
        _uint(1, int(value["alignment"]))
        + _f64(2, float(value["lineHeight"]))
        + _f64(3, float(value["spacingBefore"]))
        + _f64(4, float(value["spacingAfter"]))
    )


def _rich_step(value: dict[str, Any]) -> bytes:
    kind = value["kind"]
    if kind == "InsertText":
        body = _message(1, _id(value["paragraphId"])) + _uint(2, int(value["scalarOffset"])) + _string(3, value["text"]) + _message(4, _text_style(value["style"]))
        return _message(1, body)
    if kind == "DeleteText":
        body = _message(1, _id(value["paragraphId"])) + _uint(2, int(value["startScalar"])) + _uint(3, int(value["scalarCount"]))
        return _message(2, body)
    if kind == "SplitParagraph":
        body = _message(1, _id(value["paragraphId"])) + _uint(2, int(value["scalarOffset"])) + _message(3, _id(value["newParagraphId"]))
        return _message(3, body)
    if kind == "MergeParagraph":
        body = _message(1, _id(value["firstParagraphId"])) + _message(2, _id(value["secondParagraphId"]))
        return _message(4, body)
    if kind == "SetInlineStyle":
        body = _message(1, _id(value["paragraphId"])) + _uint(2, int(value["startScalar"])) + _uint(3, int(value["scalarCount"])) + _message(4, _text_style(value["style"]))
        return _message(5, body)
    if kind == "SetParagraphStyle":
        body = _message(1, _id(value["paragraphId"])) + _message(2, _paragraph_style(value["style"]))
        return _message(6, body)
    raise ValueError(f"unsupported RichText step kind: {kind}")


def _rich_delta(value: dict[str, Any]) -> bytes:
    result = bytearray(_uint(1, int(value["deltaVersion"])))
    for step in value["steps"]:
        result += _message(2, _rich_step(step))
    return bytes(result)


def _paragraph(value: dict[str, Any]) -> bytes:
    result = bytearray(_message(1, _id(value["paragraphId"])) + _message(2, _paragraph_style(value["style"])))
    for run in value["runs"]:
        result += _message(3, _string(1, run["text"]) + _message(2, _text_style(run["style"])))
    return bytes(result)


def _rich_document(value: dict[str, Any]) -> bytes:
    return b"".join(_message(1, _paragraph(paragraph)) for paragraph in value["paragraphs"])


def _curve(value: list[dict[str, Any]]) -> bytes:
    return b"".join(_message(1, _f32(1, float(point["x"])) + _f32(2, float(point["y"]))) for point in value)


def _pressure(value: dict[str, Any]) -> bytes:
    result = bytearray(_uint(1, int(bool(value["enabled"]))))
    if "sizeCurve" in value:
        result += _message(2, _curve(value["sizeCurve"]))
    if "opacityCurve" in value:
        result += _message(3, _curve(value["opacityCurve"]))
    return bytes(result)


def _brush(value: dict[str, Any]) -> bytes:
    result = bytearray()
    numeric = (("brushFamilyId", 1), ("brushVersion", 2))
    for name, field in numeric:
        if name in value:
            result += _uint(field, int(value[name]))
    if "color" in value:
        result += _message(3, _color(value["color"]))
    if "nominalSize" in value:
        result += _f64(4, float(value["nominalSize"]))
    if "opacity" in value:
        result += _f32(5, float(value["opacity"]))
    if "pressure" in value:
        result += _message(6, _pressure(value["pressure"]))
    if "tilt" in value:
        tilt = value["tilt"]
        result += _message(7, _uint(1, int(bool(tilt["enabled"]))) + _f32(2, float(tilt["sizeInfluence"])) + _f32(3, float(tilt["angleInfluence"])))
    if "smoothing" in value:
        result += _message(8, _f32(1, float(value["smoothing"]["amount"])))
    if "spacing" in value:
        result += _message(9, _f32(1, float(value["spacing"]["normalizedSpacing"])))
    if "textureResourceId" in value:
        result += _message(10, _id(value["textureResourceId"]))
    if "blendMode" in value:
        result += _uint(11, int(value["blendMode"]))
    return bytes(result)


def _stroke(value: dict[str, Any]) -> bytes:
    result = bytearray()
    if "brush" in value:
        result += _message(1, _brush(value["brush"]))
    result += _fixed64(2, int(value["deterministicSeed"]))
    if "vector" in value:
        samples = bytearray()
        for sample in value["vector"]["samples"]:
            sample_bytes = _message(1, _vec(sample["position"])) + _f32(2, float(sample["pressure"])) + _message(3, _vec(sample["tilt"]))
            samples += _message(1, sample_bytes)
        result += _message(3, bytes(samples))
    elif "dab" in value:
        dabs = bytearray()
        for dab in value["dab"]["dabs"]:
            dab_bytes = _message(1, _vec(dab["center"])) + _f64(2, float(dab["size"])) + _f32(3, float(dab["rotation"])) + _f32(4, float(dab["opacity"]))
            dabs += _message(1, dab_bytes)
        result += _message(4, bytes(dabs))
    else:
        raise ValueError("StrokeRecord fixture needs vector or dab data")
    return bytes(result)


def _encode(root_type: str, projection: dict[str, Any]) -> bytes:
    if root_type == "ParagraphStyle":
        return _paragraph_style(projection)
    if root_type == "RichTextDelta":
        return _rich_delta(projection)
    if root_type == "RichTextDocument":
        return _rich_document(projection)
    if root_type == "PressureMapping":
        return _pressure(projection)
    if root_type == "BrushDescriptor":
        return _brush(projection)
    if root_type == "StrokeRecord":
        return _stroke(projection)
    raise ValueError(f"unsupported GT-G1-02R leaf fixture root type: {root_type}")


def _apply_recipe(base: bytes, operations: list[dict[str, Any]]) -> bytes:
    result = bytearray(base)
    for operation in operations:
        if operation["op"] == "SET_BYTE":
            result[int(operation["offset"])] = int(operation["valueHex"], 16)
        elif operation["op"] == "REPLACE_RANGE":
            offset, length = int(operation["offset"]), int(operation["length"])
            result[offset:offset + length] = bytes.fromhex(operation["valueHex"])
        elif operation["op"] == "INSERT_HEX":
            offset = int(operation["offset"])
            result[offset:offset] = bytes.fromhex(operation["valueHex"])
        elif operation["op"] == "APPEND_HEX":
            result.extend(bytes.fromhex(operation["valueHex"]))
        elif operation["op"] == "TRUNCATE":
            del result[int(operation["length"]):]
        else:
            raise ValueError(f"unsupported reviewed wire recipe primitive: {operation['op']}")
    return bytes(result)


def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _git_commit(root: Path) -> str:
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()


def load_authoring_catalog(root: Path = ROOT) -> list[dict[str, Any]]:
    return json.loads((root / CATALOG).read_text(encoding="utf-8"))["cases"]


def verify_independence(root: Path = ROOT) -> dict[str, Any]:
    source = root / "verification/fixture-author/compile_g1_02r_leaf_golden.py"
    tree = ast.parse(source.read_text(encoding="utf-8"))
    modules: list[str] = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            modules.extend(alias.name for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module:
            modules.append(node.module)
    violations = [module for module in modules if module == "runtime" or module.startswith("runtime.")]
    return {"ok": not violations, "violations": violations, "identity": "verification-only-generic-wire-author"}


def _case_metadata(case: dict[str, Any], positive: bool) -> dict[str, Any]:
    expected: dict[str, Any] = {"outcome": "ACCEPTED" if positive else "REJECTED"}
    if positive:
        expected.update({"canonicalArtifact": "expected/canonical.pb", "semanticProjectionArtifact": "expected/semantic.projection.json"})
    else:
        expected.update(case["outcome"])
    return {
        "formatVersion": 1,
        "id": case["id"],
        "status": "SPEC_REQUIREMENT",
        "authorityRefs": ["RichText + Stroke Semantic / Wire Reconciliation Matrix v0.1", "Golden Corpus Authoring Rules + Fixture Generation Pipeline v0.1"],
        "entrypoint": "ENCODE" if positive else "DECODE",
        "requiredCapabilities": ["CANONICAL_ENCODE", "STRICT_WIRE_PREFLIGHT", "SEMANTIC_PROJECTION"],
        "input": {"artifact": "authoring/input.projection.json" if positive else "input/value.pb"},
        "expected": expected,
        "blockedByOpenPolicy": False,
    }


def _case_directory(case: dict[str, Any]) -> Path:
    family = "richtext" if case["id"].startswith("RTW") else "brush-stroke"
    return Path("wire/g1-02r") / family / f"{case['id']}-{case['slug']}"


def materialize(root: Path, output_root: Path, compiler_commit: str | None = None) -> dict[str, Any]:
    cases = load_authoring_catalog(root)
    compiler_commit = compiler_commit or _git_commit(root)
    generated: dict[str, bytes] = {}
    entries: list[dict[str, Any]] = []
    for case in cases:
        positive = "projection" in case
        directory = output_root / _case_directory(case)
        directory.mkdir(parents=True, exist_ok=True)
        _write_json(directory / "case.json", _case_metadata(case, positive))
        if positive:
            projection = {"format": "axiom-verification-projection-v1", "formatVersion": 1, "rootType": case["rootType"], "form": "CANONICAL", "value": case["projection"]}
            _write_json(directory / "authoring/input.projection.json", projection)
            canonical = _encode(case["rootType"], case["projection"])
            if case["id"] == "STW-007":
                for sample in case["fixed64Samples"]:
                    encoded = _fixed64(2, int(sample))
                    if len(encoded) != 9 or encoded[0] != 0x11:
                        raise ValueError("STW-007 fixed64 sample did not preserve fixed-width carrier")
            (directory / "expected").mkdir(exist_ok=True)
            (directory / "expected/canonical.pb").write_bytes(canonical)
            _write_json(directory / "expected/semantic.projection.json", case["semanticProjection"])
            generated[case["id"]] = canonical
        else:
            base = generated.get(case["baseCase"])
            if base is None:
                raise ValueError(f"{case['id']} references a positive base case that has not been materialized")
            _write_json(directory / "authoring/wire.recipe.json", {"formatVersion": 1, "baseCase": case["baseCase"], "baseArtifact": "expected/canonical.pb", "operations": case["recipe"]})
            (directory / "input").mkdir(exist_ok=True)
            (directory / "input/value.pb").write_bytes(_apply_recipe(base, case["recipe"]))
            _write_json(directory / "expected/outcome.json", case["outcome"])
        artifacts = []
        for artifact in sorted(path for path in directory.rglob("*") if path.is_file() and path.name != "provenance.json"):
            artifacts.append({"path": artifact.relative_to(directory).as_posix(), "sha256": _sha256(artifact.read_bytes()), "bytes": artifact.stat().st_size})
        provenance = {
            "formatVersion": 1,
            "caseId": case["id"],
            "authority": {"semantic": "RichText + Stroke Semantic / Wire Reconciliation Matrix v0.1", "verification": "Golden Corpus Authoring Rules + Fixture Generation Pipeline v0.1"},
            "sources": [CATALOG, *AUTHORITY_PATHS],
            "artifacts": artifacts,
            "fixtureCompiler": {"identity": "verification-only-generic-wire-author", "sourceCommit": compiler_commit},
        }
        _write_json(directory / "provenance.json", provenance)
        entries.append({"id": case["id"], "path": _case_directory(case).as_posix(), "provenanceSha256": _sha256((directory / "provenance.json").read_bytes())})
    digest = hashlib.sha256()
    for entry in entries:
        digest.update(f"{entry['id']}\0{entry['path']}\0{entry['provenanceSha256']}\n".encode("utf-8"))
    manifest = {
        "format": "axiom-g1-02r-leaf-fixture-manifest-v1",
        "fixtureCompiler": "verification-only-generic-wire-author",
        "cases": entries,
        "inventorySha256": digest.hexdigest(),
    }
    _write_json(output_root / "g1-02r-fixture-manifest.json", manifest)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--verify-independence", action="store_true")
    parser.add_argument("--source-commit")
    args = parser.parse_args()
    root = args.root.resolve()
    if args.verify_independence:
        result = verify_independence(root)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0 if result["ok"] else 1
    if args.output_root is None:
        parser.error("--output-root is required unless --verify-independence is set")
    print(json.dumps(materialize(root, args.output_root, args.source_commit), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
