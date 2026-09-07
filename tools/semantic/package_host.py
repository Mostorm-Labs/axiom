#!/usr/bin/env python3
"""Repackage pinned official protoc binaries; never compile a target runtime."""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.parse

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes, safe_extract_zip, verify_sha256
from tools.sdk.model import ArtifactRef, IntegrityError, SdkError, detect_host_platform
from tools.sdk.transport import ensure_release_file
from tools.semantic.contract import DEFAULT_PROFILE, ROOT, HOST_KEYS, load_profile, make_host_tool_identity, read_json
from tools.semantic.package_common import create_package, install_payload, validate_payload
from tools.semantic.sources import source_archive, source_license


def validate_host(root: Path, record: dict) -> dict:
    manifest = validate_payload(root, record, host=True)
    identity = manifest["identity"]
    if identity.get("key") not in HOST_KEYS or identity.get("format") != "axiom-semantic-host-identity-v2":
        raise IntegrityError("semantic: invalid host identity")
    executable = "bin/protoc.exe" if identity["key"] == "windows-x64" else "bin/protoc"
    required = {executable, "include/google/protobuf/descriptor.proto", "licenses/Protobuf.txt"}
    files = {entry["path"]: entry for entry in manifest["files"]}
    if not required <= files.keys() or any(path.startswith("lib/") for path in files):
        raise IntegrityError("semantic: incomplete host consumer contract")
    if files[executable]["mode"] != 0o755 or identity.get("executable") != executable:
        raise IntegrityError("semantic: host executable contract mismatch")
    return manifest


def install_host(archive: Path, destination: Path, record: dict) -> dict:
    install_payload(archive, destination, record, host=True)
    return validate_host(destination, record)


def verify_host_archive(archive: Path, record: dict) -> dict:
    with tempfile.TemporaryDirectory(prefix="semantic-host-verify-") as directory:
        return install_host(archive, Path(directory), record)


def package_host(lock: dict, profile: dict, key: str, upstream: Path,
                 license_file: Path, output: Path) -> dict:
    identity, _ = make_host_tool_identity(lock, profile, key)
    verify_sha256(upstream, identity["upstreamSha256"])
    with tempfile.TemporaryDirectory(prefix="semantic-protoc-") as directory:
        temporary = Path(directory)
        unpacked = temporary / "upstream"
        safe_extract_zip(upstream, unpacked)
        executable = identity["executable"]
        if not (unpacked / executable).is_file() or not (unpacked / "include/google/protobuf/descriptor.proto").is_file():
            raise IntegrityError("semantic: official protoc archive lacks required host contract")
        payload = temporary / "payload"
        (payload / "bin").mkdir(parents=True)
        shutil.copyfile(unpacked / executable, payload / executable)
        shutil.copytree(unpacked / "include", payload / "include")
        (payload / "licenses").mkdir()
        shutil.copyfile(license_file, payload / "licenses/Protobuf.txt")
        # Preserve upstream notices without treating unrelated binaries as SDK inputs.
        for path in unpacked.iterdir():
            if path.is_file():
                shutil.copyfile(path, payload / "licenses" / ("upstream-" + path.name))
        record = create_package(payload, identity, host=True, output=output)
    verify_host_archive(output / record["asset"], record)
    return record


def probe_host(archive: Path, record: dict) -> None:
    actual = detect_host_platform()
    expected = "macos-universal" if actual.os == "macos" else actual.key
    if record["identity"]["key"] != expected:
        raise SdkError("semantic: host probe cannot execute a different host's binary")
    with tempfile.TemporaryDirectory(prefix="semantic-host-probe-") as directory:
        root = Path(directory)
        install_host(archive, root, record)
        executable = root / record["identity"]["executable"]
        output = subprocess.check_output([str(executable), "--version"], text=True).strip()
        if output != "libprotoc " + record["identity"]["protobufVersion"]:
            raise SdkError(f"semantic: protoc version mismatch: {output}")
        if actual.os == "macos":
            slices = subprocess.check_output(["lipo", "-archs", str(executable)], text=True).split()
            if not {"arm64", "x86_64"} <= set(slices):
                raise SdkError("semantic: protoc is missing universal macOS slices")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-key", choices=HOST_KEYS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    parser.add_argument("--deps-lock", type=Path, default=ROOT / "deps.lock.json")
    parser.add_argument("--cache", type=Path, default=ROOT / "out/semantic-sdk/upstream")
    parser.add_argument("--upstream-archive", type=Path)
    parser.add_argument("--protobuf-source-archive", type=Path)
    parser.add_argument("--probe", action="store_true")
    args = parser.parse_args()
    lock, profile = read_json(args.deps_lock), load_profile(args.profile)
    identity, identifier = make_host_tool_identity(lock, profile, args.host_key)
    protobuf = lock["dependencies"]["protobuf"]
    upstream_data = protobuf["protoc_assets"][profile["hostTools"][args.host_key]["upstreamKey"]]
    upstream_name = Path(urllib.parse.urlsplit(upstream_data["url"]).path).name
    upstream = args.upstream_archive or args.cache / upstream_data["sha256"] / upstream_name
    if args.upstream_archive is None:
        ref = ArtifactRef("semantic", "host-tools", args.host_key, identifier,
                          "protocolbuffers/protobuf", "v" + protobuf["version"], upstream_name, upstream_data["sha256"])
        ensure_release_file(ref, upstream)
    sources = args.protobuf_source_archive or source_archive("protobuf", lock, args.cache)
    verify_sha256(sources, protobuf["source_sha256"])
    with tempfile.TemporaryDirectory(prefix="semantic-host-license-") as directory:
        license_file = source_license(sources, f"protobuf-{protobuf['version']}/LICENSE", Path(directory) / "LICENSE")
        record = package_host(lock, profile, args.host_key, upstream, license_file, args.output)
    if args.probe:
        probe_host(args.output / record["asset"], record)
    (args.output / "record.json").write_bytes(canonical_bytes(record) + b"\n")
    print(canonical_bytes(record).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
