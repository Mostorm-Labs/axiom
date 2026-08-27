#!/usr/bin/env python3
"""Validate the machine-readable GT-G1-02 semantic contract inputs."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
G1_02R_BASELINE = "e7a0581706b4e4370fd397ccffef81aa84e48a27"
G1_02R_AUTHORITY_PATHS = (
    "docs/notion/manifest.yaml",
    "docs/notion/authority/04-semantic-schema/04-reference-idl/richtext-stroke-semantic-wire-reconciliation-matrix-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/richtext-wire-schema-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/brush-stroke-wire-schema-v0.1.md",
    "docs/notion/authority/10-verification/golden-authoring-fixture-pipeline-v0.1.md",
)
G1_02R_CASE_IDS = (
    "RTW-001", "RTW-002", "RTW-003", "RTW-004", "RTW-005", "RTW-006", "RTW-007", "RTW-008", "RTW-009",
    "STW-001", "STW-002", "STW-003", "STW-004", "STW-005", "STW-006", "STW-007",
    "RTW-N01", "RTW-N02", "STW-N01", "STW-N02", "STW-N03", "STW-N04", "STW-N05",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resolve_pinned_protoc(root: Path, requested: Path | None = None) -> Path:
    candidates = []
    if requested is not None:
        candidates.append(requested if requested.is_absolute() else root / requested)
    candidates.extend((
        root / ".deps/protobuf/bin/protoc",
        root / ".deps/protoc-36.0/bin/protoc",
    ))
    path_from_env = shutil.which("protoc")
    if path_from_env:
        candidates.append(Path(path_from_env))
    for candidate in candidates:
        if not candidate.exists():
            continue
        try:
            version = subprocess.check_output(
                [str(candidate), "--version"], text=True, stderr=subprocess.STDOUT
            ).strip()
        except (OSError, subprocess.CalledProcessError):
            continue
        if version == "libprotoc 36.0":
            return candidate
    raise SystemExit("pinned protoc 36.0 is not available; hosted toolchain validation is required")


def _load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load validator helper: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _manifest_valid(root: Path, relative: str, expected_ids: tuple[str, ...]) -> bool:
    manifest_path = root / relative
    if not manifest_path.is_file():
        return False
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = manifest.get("cases", [])
    if tuple(item.get("id") for item in entries) != expected_ids:
        return False
    corpus_root = manifest_path.parent
    for entry in entries:
        directory = corpus_root / entry.get("path", "")
        provenance = directory / "provenance.json"
        case = directory / "case.json"
        if not provenance.is_file() or not case.is_file():
            return False
        if sha256(provenance) != entry.get("provenanceSha256"):
            return False
        if json.loads(case.read_text(encoding="utf-8")).get("id") != entry.get("id"):
            return False
    return True


def validate_g1_02r(root: Path) -> dict[str, object]:
    """Validate the current GT-G1-02R machine-refreeze inputs.

    This is deliberately structural.  It proves that the descriptor, current
    authority paths and checked-in independent corpus are mutually coherent;
    the production differential is executed by its own runner.
    """
    root = root.resolve()
    descriptor_tool = _load_module(
        "g1_02r_descriptor_refreeze_diff",
        root / "verification/tools/descriptor_refreeze_diff.py",
    )
    before = subprocess.check_output(
        ["git", "show", f"{G1_02R_BASELINE}:schema/axiom/v1/descriptor/descriptor.lock.pb"], cwd=root
    )
    after = (root / "schema/axiom/v1/descriptor/descriptor.lock.pb").read_bytes()
    descriptor = descriptor_tool.compare(before, after, authority_baseline=G1_02R_BASELINE)
    compiler = _load_module(
        "g1_02r_leaf_fixture_author",
        root / "verification/fixture-author/compile_g1_02r_leaf_golden.py",
    )
    leaf_manifest = root / "verification/corpus/semantic/v1/g1-02r-fixture-manifest.json"
    leaf = json.loads(leaf_manifest.read_text(encoding="utf-8")) if leaf_manifest.is_file() else {"cases": []}
    return {
        "descriptor": {
            "beforeSha256": descriptor["beforeDescriptorSha256"],
            "afterSha256": descriptor["afterDescriptorSha256"],
            "unmappedChanges": descriptor["unmappedChanges"],
            "outerRegistryPreserved": descriptor["outerRegistryPreserved"],
        },
        "historicalBgManifestValid": _manifest_valid(
            root,
            "verification/corpus/semantic/v1/fixture-manifest.json",
            tuple([f"BG-{index:03d}" for index in range(1, 11)] + [f"BG-N{index:02d}" for index in range(1, 9)]),
        ),
        "leafCorpus": {
            "manifestPath": "verification/corpus/semantic/v1/g1-02r-fixture-manifest.json",
            "manifestSha256": sha256(leaf_manifest) if leaf_manifest.is_file() else None,
            "caseCount": len(leaf.get("cases", [])),
            "caseIds": [entry.get("id") for entry in leaf.get("cases", [])],
            "manifestValid": _manifest_valid(root, "verification/corpus/semantic/v1/g1-02r-fixture-manifest.json", G1_02R_CASE_IDS),
            "fixtureAuthorIndependent": compiler.verify_independence(root)["ok"],
        },
        "authorityPathsValid": all((root / path).is_file() for path in G1_02R_AUTHORITY_PATHS),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--protoc", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    proto_root = root / "schema/axiom/v1/proto"
    expected = (root / "schema/axiom/v1/descriptor/descriptor.lock.sha256").read_text().strip()
    actual = sha256(root / "schema/axiom/v1/descriptor/descriptor.lock.pb")
    if expected != actual:
        raise SystemExit(f"descriptor hash mismatch: {actual} != {expected}")
    seed = json.loads((root / "verification/corpus/semantic/v1/suites/seed-v0.1.json").read_text())
    cases = seed.get("cases", [])
    expected_ids = [f"g1-seed-v0.1-{index:03d}" for index in range(60)]
    if len(cases) != 60 or [item["stable_id"] for item in cases] != expected_ids:
        raise SystemExit("semantic seed identity mismatch")
    protoc = resolve_pinned_protoc(root, args.protoc)
    with tempfile.TemporaryDirectory() as directory:
        generated = Path(directory) / "descriptor.pb"
        sources = sorted(proto_root.rglob("*.proto"))
        command = [str(protoc), f"--proto_path={proto_root}", "--include_imports", f"--descriptor_set_out={generated}", *[str(path.relative_to(proto_root)) for path in sources]]
        subprocess.run(command, cwd=proto_root, check=True)
        if sha256(generated) != actual:
            raise SystemExit("descriptor is not reproducible")
    g1_02r = validate_g1_02r(root)
    if (
        g1_02r["descriptor"]["unmappedChanges"]
        or not all(g1_02r["descriptor"]["outerRegistryPreserved"].values())
        or not g1_02r["historicalBgManifestValid"]
        or not g1_02r["leafCorpus"]["manifestValid"]
        or not g1_02r["leafCorpus"]["fixtureAuthorIndependent"]
        or not g1_02r["authorityPathsValid"]
    ):
        raise SystemExit("GT-G1-02R semantic machine-refreeze contract is invalid")
    print(
        f"semantic contract: descriptor={actual}; seed_cases={len(cases)}; "
        f"g1_02r_cases={g1_02r['leafCorpus']['caseCount']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
