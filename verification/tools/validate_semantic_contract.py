#!/usr/bin/env python3
"""Validate the machine-readable GT-G1-02 semantic contract inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PROTO_ROOT = ROOT / "schema/axiom/v1/proto"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    root = args.root.resolve()
    expected = (root / "schema/axiom/v1/descriptor/descriptor.lock.sha256").read_text().strip()
    actual = sha256(root / "schema/axiom/v1/descriptor/descriptor.lock.pb")
    if expected != actual:
        raise SystemExit(f"descriptor hash mismatch: {actual} != {expected}")
    seed = json.loads((root / "verification/corpus/semantic/v1/suites/seed-v0.1.json").read_text())
    cases = seed.get("cases", [])
    expected_ids = [f"g1-seed-v0.1-{index:03d}" for index in range(60)]
    if len(cases) != 60 or [item["stable_id"] for item in cases] != expected_ids:
        raise SystemExit("semantic seed identity mismatch")
    protoc = root / ".deps/protoc-36.0/bin/protoc"
    if not protoc.exists():
        raise SystemExit("protoc 36.0 is not available; hosted toolchain validation is required")
    with tempfile.TemporaryDirectory() as directory:
        generated = Path(directory) / "descriptor.pb"
        sources = sorted(PROTO_ROOT.rglob("*.proto"))
        command = [str(protoc), f"--proto_path={PROTO_ROOT}", "--include_imports", f"--descriptor_set_out={generated}", *[str(path.relative_to(PROTO_ROOT)) for path in sources]]
        subprocess.run(command, cwd=PROTO_ROOT, check=True)
        if sha256(generated) != actual:
            raise SystemExit("descriptor is not reproducible")
    print(f"semantic contract: descriptor={actual}; seed_cases={len(cases)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
