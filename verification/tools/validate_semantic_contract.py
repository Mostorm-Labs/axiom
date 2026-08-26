#!/usr/bin/env python3
"""Validate the machine-readable GT-G1-02 semantic contract inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


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
    print(f"semantic contract: descriptor={actual}; seed_cases={len(cases)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
