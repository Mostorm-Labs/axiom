#!/usr/bin/env python3
"""Materialize the authority-reviewed GT-G1-02 codec golden seed.

This is verification-only code.  It implements generic protobuf wire
primitives and the human-reviewed catalog; it never imports or executes
``runtime/semantic`` or any production canonical writer.
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
CATALOG = "verification/fixture-author/codec_golden_authoring_v1.json"
AUTHORITY_PATHS = [
    "docs/notion/authority/04-semantic-schema/04-reference-idl/canonical-codec-golden-authority-closure-v0.1.md",
    "docs/notion/authority/10-verification/canonical-codec-golden-fixture-authoring-set-v0.1.md",
]


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _varint(value: int) -> bytes:
    if value < 0:
        raise ValueError("varint cannot encode a negative value")
    output = bytearray()
    while value > 0x7F:
        output.append((value & 0x7F) | 0x80)
        value >>= 7
    output.append(value)
    return bytes(output)


def _key(field: int, wire_type: int) -> bytes:
    return _varint((field << 3) | wire_type)


def _bytes(field: int, value: bytes) -> bytes:
    return _key(field, 2) + _varint(len(value)) + value


def _fixed64(field: int, tagged: str) -> bytes:
    bits = int(tagged.removeprefix("f64:"), 16)
    return _key(field, 1) + struct.pack("<Q", bits)


def _fixed32(field: int, tagged: str) -> bytes:
    bits = int(tagged.removeprefix("f32:"), 16)
    return _key(field, 5) + struct.pack("<I", bits)


def _message(field: int, value: bytes) -> bytes:
    return _bytes(field, value)


def _encode_projection(root_type: str, value: dict[str, Any]) -> bytes:
    """Encode only the authority-closed types with generic wire primitives."""
    if root_type in {"Id128", "OrderKey"}:
        return _bytes(1, bytes.fromhex(value["value"]))
    if root_type == "Vec2":
        return _fixed64(1, value["x"]) + _fixed64(2, value["y"])
    if root_type == "Transform2D":
        return b"".join(_fixed64(index, value[name]) for index, name in enumerate(("a", "b", "c", "d", "tx", "ty"), 1))
    if root_type == "PropertyValue":
        return _fixed32(2, value["f32Value"])
    if root_type == "ColorValue":
        return b"".join(_fixed32(index, value[name]) for index, name in enumerate(("r", "g", "b", "a"), 1))
    if root_type == "Placement":
        output = bytearray()
        if "parentId" in value:
            output += _message(1, _bytes(1, bytes.fromhex(value["parentId"])))
        output += _message(2, _bytes(1, bytes.fromhex(value["orderKey"])))
        return bytes(output)
    if root_type == "DashPattern":
        packed = b"".join(struct.pack("<Q", int(item.removeprefix("f64:"), 16)) for item in value["segments"])
        return _bytes(1, packed) + _fixed64(2, value["offset"])
    if root_type == "DocumentSnapshot":
        document_id = _bytes(1, bytes.fromhex(value["documentId"]))
        return _message(1, document_id) + _key(2, 0) + _varint(value["schemaVersion"])
    raise ValueError(f"unsupported closed fixture root type: {root_type}")


def _apply_recipe(base: bytes, operations: list[dict[str, Any]]) -> bytes:
    output = bytearray(base)
    for operation in operations:
        name = operation["op"]
        if name == "SET_BYTE":
            output[operation["offset"]] = int(operation["valueHex"], 16)
        elif name == "REPLACE_RANGE":
            offset, length = operation["offset"], operation["length"]
            output[offset:offset + length] = bytes.fromhex(operation["valueHex"])
        elif name == "INSERT_HEX":
            output[operation["offset"]:operation["offset"]] = bytes.fromhex(operation["valueHex"])
        elif name == "APPEND_HEX":
            output.extend(bytes.fromhex(operation["valueHex"]))
        elif name == "TRUNCATE":
            del output[operation["length"]:]
        else:
            raise ValueError(f"unsupported authority recipe primitive: {name}")
    return bytes(output)


def _git_commit(root: Path) -> str:
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, check=True, capture_output=True, text=True)
    return result.stdout.strip()


def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def load_authoring_catalog(root: Path = ROOT) -> list[dict[str, Any]]:
    catalog = json.loads((root / CATALOG).read_text(encoding="utf-8"))
    return catalog["cases"]


def verify_independence(root: Path = ROOT) -> dict[str, Any]:
    source_path = root / "verification/fixture-author/compile_codec_golden.py"
    tree = ast.parse(source_path.read_text(encoding="utf-8"))
    imported_modules = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            imported_modules.extend(alias.name for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module:
            imported_modules.append(node.module)
    violations = [module for module in imported_modules if module == "runtime" or module.startswith("runtime.")]
    return {"ok": not violations, "violations": violations, "identity": "verification-only"}


def _case_metadata(case: dict[str, Any], positive: bool) -> dict[str, Any]:
    expected: dict[str, Any] = {"outcome": "ACCEPTED" if positive else "REJECTED"}
    if positive or "canonicalCase" in case or "canonicalHex" in case:
        expected["canonicalArtifact"] = "expected/canonical.pb"
    if not positive:
        expected.update(case["outcome"])
    return {
        "formatVersion": 1,
        "id": case["id"],
        "status": "SPEC_REQUIREMENT",
        "authorityRefs": [f"Canonical Codec Golden Authority Closure v0.1#{case['id']}"],
        "entrypoint": "ENCODE" if positive else "DECODE",
        "requiredCapabilities": ["CANONICAL_ENCODE", "STRICT_WIRE_PREFLIGHT"],
        "input": {"artifact": "authoring/input.projection.json" if positive else "input/value.pb"},
        "expected": expected,
        "blockedByOpenPolicy": False,
    }


def materialize(root: Path, output_root: Path, compiler_commit: str | None = None) -> dict[str, Any]:
    cases = load_authoring_catalog(root)
    compiler_commit = compiler_commit or _git_commit(root)
    generated: dict[str, bytes] = {}
    entries: list[dict[str, Any]] = []
    for case in cases:
        positive = case["id"].startswith("BG-") and not case["id"].startswith("BG-N")
        directory = output_root / "wire/bg" / f"{case['id']}-{case['slug']}"
        directory.mkdir(parents=True, exist_ok=True)
        _write_json(directory / "case.json", _case_metadata(case, positive))
        if positive:
            projection = {"format": "axiom-verification-projection-v1", "formatVersion": 1, "rootType": case["rootType"], "form": "CANONICAL", "value": case["projection"]}
            _write_json(directory / "authoring/input.projection.json", projection)
            canonical = _encode_projection(case["rootType"], case["projection"])
            if canonical.hex() != case["canonicalHex"]:
                raise ValueError(f"{case['id']} fixture compiler does not reproduce authority hex")
            (directory / "expected").mkdir(exist_ok=True)
            (directory / "expected/canonical.pb").write_bytes(canonical)
            generated[case["id"]] = canonical
        else:
            recipe = {"formatVersion": 1, "baseCase": case["baseCase"], "baseArtifact": "expected/canonical.pb", "operations": case["recipe"]}
            _write_json(directory / "authoring/wire.recipe.json", recipe)
            input_bytes = _apply_recipe(generated[case["baseCase"]], case["recipe"])
            (directory / "input").mkdir(exist_ok=True)
            (directory / "input/value.pb").write_bytes(input_bytes)
            if "canonicalCase" in case or "canonicalHex" in case:
                canonical = generated.get(case.get("canonicalCase", ""), bytes.fromhex(case.get("canonicalHex", "")))
                (directory / "expected").mkdir(exist_ok=True)
                (directory / "expected/canonical.pb").write_bytes(canonical)
            _write_json(directory / "expected/outcome.json", case["outcome"])
        files = []
        for artifact in sorted(path for path in directory.rglob("*") if path.is_file() and path.name != "provenance.json"):
            relative = artifact.relative_to(directory).as_posix()
            files.append({"path": relative, "sha256": _sha256(artifact.read_bytes()), "bytes": artifact.stat().st_size})
        provenance = {
            "formatVersion": 1,
            "caseId": case["id"],
            "authority": {"semantic": "Canonical Codec Golden Authority Closure v0.1", "verification": "Canonical Codec Golden Fixture Authoring Set v0.1"},
            "sources": [CATALOG, *AUTHORITY_PATHS],
            "artifacts": files,
            "fixtureCompiler": {"identity": "verification-only", "sourceCommit": compiler_commit},
        }
        _write_json(directory / "provenance.json", provenance)
        entries.append({"id": case["id"], "path": (Path("wire/bg") / directory.name).as_posix(), "provenanceSha256": _sha256((directory / "provenance.json").read_bytes())})
    digest = hashlib.sha256()
    for entry in entries:
        digest.update(f"{entry['id']}\0{entry['path']}\0{entry['provenanceSha256']}\n".encode("utf-8"))
    manifest = {"formatVersion": 1, "fixtureCompiler": "verification-only", "cases": entries, "inventorySha256": digest.hexdigest()}
    _write_json(output_root / "fixture-manifest.json", manifest)
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
        print(json.dumps(result, indent=2))
        return 0 if result["ok"] else 1
    if args.output_root is None:
        parser.error("--output-root is required unless --verify-independence is set")
    print(json.dumps(materialize(root, args.output_root, args.source_commit), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
