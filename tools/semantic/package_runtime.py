#!/usr/bin/env python3
"""Package relocatable target libraries and CMake metadata, never host tools."""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys
import tempfile

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes
from tools.sdk.model import IntegrityError, SdkError
from tools.semantic.contract import ROOT, DEFAULT_PROFILE, RUNTIME_KEYS, load_profile, make_runtime_identity, read_json
from tools.semantic.package_common import create_package, install_payload, validate_payload

REQUIRED_FILES = {
    "include/google/protobuf/message.h",
    "lib/cmake/protobuf/protobuf-config.cmake",
    "lib/cmake/absl/abslConfig.cmake",
    "lib/cmake/utf8_range/utf8_range-config.cmake",
    "licenses/Protobuf.txt", "licenses/Abseil.txt", "licenses/utf8_range.txt",
}


def _check_contract(root: Path, key: str) -> None:
    files = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()}
    if not REQUIRED_FILES <= files or any(path.startswith("bin/") for path in files):
        raise IntegrityError("semantic: incomplete target runtime consumer contract or unexpected host tool")
    extension = ".lib" if key.startswith("windows") else ".a"
    if not any(Path(path).name in {"libprotobuf" + extension, "protobuf" + extension} for path in files):
        raise IntegrityError("semantic: runtime is missing its target Protobuf static library")


def validate_runtime(root: Path, record: dict) -> dict:
    manifest = validate_payload(root, record, host=False)
    identity = manifest["identity"]
    if identity.get("key") not in RUNTIME_KEYS or identity.get("format") != "axiom-semantic-runtime-identity-v2":
        raise IntegrityError("semantic: incorrect runtime identity")
    _check_contract(root, identity["key"])
    return manifest


def install_runtime(archive: Path, destination: Path, record: dict) -> dict:
    install_payload(archive, destination, record, host=False)
    return validate_runtime(destination, record)


def verify_runtime_archive(archive: Path, record: dict) -> dict:
    with tempfile.TemporaryDirectory(prefix="semantic-runtime-verify-") as directory:
        return install_runtime(archive, Path(directory), record)


def package_runtime(lock: dict, profile: dict, key: str, root: Path, toolchain: dict,
                    output: Path) -> dict:
    identity, _ = make_runtime_identity(lock, profile, key, toolchain)
    _check_contract(root, key)
    prefixes = {spelling for path in (root, root.absolute(), root.resolve())
                for spelling in (str(path), path.as_posix())}
    prefixes |= {str(value) for name, value in toolchain.items() if name in {"buildPath", "sourcePath", "installPath"}}
    for path in (root / "lib/cmake").rglob("*.cmake"):
        text = path.read_text(encoding="utf-8")
        if any(prefix and prefix in text for prefix in prefixes):
            raise IntegrityError(f"semantic: CMake package is not relocatable: {path.name}")
    with tempfile.TemporaryDirectory(prefix="semantic-runtime-") as directory:
        payload = Path(directory) / "payload"
        for folder in ("include", "licenses", "lib/cmake"):
            shutil.copytree(root / folder, payload / folder)
        extension = ".lib" if key.startswith("windows") else ".a"
        for path in (root / "lib").glob("*" + extension):
            if path.is_symlink():
                raise IntegrityError("semantic: runtime library may not be a symlink")
            shutil.copyfile(path, payload / "lib" / path.name)
        record = create_package(payload, identity, host=False, output=output)
    verify_runtime_archive(output / record["asset"], record)
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=RUNTIME_KEYS, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--toolchain-json", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    parser.add_argument("--deps-lock", type=Path, default=ROOT / "deps.lock.json")
    args = parser.parse_args()
    record = package_runtime(read_json(args.deps_lock), load_profile(args.profile), args.target,
                             args.root, read_json(args.toolchain_json), args.output)
    (args.output / "record.json").write_bytes(canonical_bytes(record) + b"\n")
    print(canonical_bytes(record).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
