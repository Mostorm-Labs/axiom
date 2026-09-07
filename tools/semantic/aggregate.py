#!/usr/bin/env python3
"""Verify and atomically stage a complete, deterministic Semantic Release Set."""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys
import tempfile

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import IntegrityError, SdkError, validate_component
from tools.semantic.contract import (
    ROOT, DEFAULT_PROFILE, HOST_KEYS, RUNTIME_KEYS, INDEX_FORMAT,
    dependency_authority, load_profile, make_host_tool_identity,
    make_runtime_identity, make_release_set_identity, read_json, validate_index,
)
from tools.semantic.package_host import verify_host_archive
from tools.semantic.package_runtime import verify_runtime_archive

INDEX_ASSET = "semantic-sdk-index.json"
SUMS_ASSET = "SHA256SUMS"


def check_identity_collisions(prior: dict, candidate: dict) -> None:
    """A previously accepted identity must never acquire different archive bytes."""
    for kind, field in (("hostTools", "hostToolId"), ("runtimes", "runtimeId")):
        known = {record[field]: record["sha256"] for record in prior[kind].values()}
        for record in candidate[kind].values():
            if record[field] in known and known[record[field]] != record["sha256"]:
                raise IntegrityError(
                    f"semantic: identity-collision for {field} {record[field]}; "
                    "increment the corresponding producer contract version before publishing"
                )


def release_metadata(index: dict, index_path: Path) -> dict:
    return {"format": "axiom-semantic-release-v2", "releaseSetId": index["releaseSetId"],
            "tag": "semantic-sdk-v2-" + index["releaseSetId"][:16],
            "indexSha256": file_sha256(index_path),
            "assets": sorted([INDEX_ASSET, SUMS_ASSET, *(
                record["asset"] for kind in ("hostTools", "runtimes")
                for record in index[kind].values())])}


def checksum_bytes(directory: Path, index: dict) -> bytes:
    names = sorted([INDEX_ASSET, *(r["asset"] for kind in ("hostTools", "runtimes")
                                  for r in index[kind].values())])
    # SHA256SUMS does not hash itself. release.json is transport metadata only.
    return "".join(f"{file_sha256(directory / name)}  {name}\n" for name in names).encode("utf-8")


def verify_release_directory(directory: Path) -> dict:
    """Validate metadata, exact file set and every payload before any publication."""
    index = validate_index(read_json(directory / INDEX_ASSET))
    expected = release_metadata(index, directory / INDEX_ASSET)
    actual = {path.name for path in directory.iterdir()}
    if actual != set(expected["assets"]) | {"release.json"}:
        raise IntegrityError("semantic: release asset set must contain exactly 14 assets and release.json")
    for path in directory.iterdir():
        if path.is_symlink() or not path.is_file():
            raise IntegrityError("semantic: release assets must be regular files, not links")
    for kind, verify in (("hostTools", verify_host_archive), ("runtimes", verify_runtime_archive)):
        for record in index[kind].values():
            verify(directory / record["asset"], record)
    if (directory / SUMS_ASSET).read_bytes() != checksum_bytes(directory, index):
        raise IntegrityError("semantic: SHA256SUMS differs from the complete release set")
    if read_json(directory / "release.json") != expected:
        raise IntegrityError("semantic: release.json differs from the index authority")
    return expected


def aggregate(assets: Path, output: Path, *, deps_lock: Path = ROOT / "deps.lock.json",
              profile_path: Path = DEFAULT_PROFILE, previous_index: dict | None = None) -> dict:
    assets, output = assets.resolve(), output.absolute()
    if not assets.is_dir() or output == assets or assets in output.parents or output in assets.parents:
        raise SdkError("semantic: input and output must be separate, non-overlapping directories")
    lock, profile = read_json(deps_lock), load_profile(profile_path)
    hosts, runtimes, sources = {}, {}, {}
    for record_path in sorted(assets.rglob("record.json")):
        record = read_json(record_path)
        identity = record.get("identity", {})
        key = identity.get("key")
        is_host = "hostToolId" in record
        records, keys = (hosts, HOST_KEYS) if is_host else (runtimes, RUNTIME_KEYS)
        if key not in keys:
            raise IntegrityError(f"semantic: unsupported package key: {key}")
        if key in records:
            raise IntegrityError(f"semantic: duplicate package key: {key}")
        expected, _ = (make_host_tool_identity(lock, profile, key) if is_host else
                       make_runtime_identity(lock, profile, key, identity.get("toolchain", {})))
        if identity != expected:
            raise IntegrityError(f"semantic: package differs from selected producer authority: {key}")
        archive = record_path.parent / validate_component(record.get("asset"))
        if record_path.is_symlink() or archive.is_symlink() or not archive.is_file():
            raise IntegrityError("semantic: missing or linked candidate archive/record")
        sources[record["asset"]] = archive
        records[key] = record
    dependencies = dependency_authority(lock)
    _, identifier = make_release_set_identity(dependencies, hosts, runtimes)
    index = validate_index({"format": INDEX_FORMAT, "releaseSetId": identifier,
                            "dependencies": dependencies, "hostTools": hosts, "runtimes": runtimes})
    if previous_index is not None:
        check_identity_collisions(validate_index(previous_index), index)
    for kind, verify in (("hostTools", verify_host_archive), ("runtimes", verify_runtime_archive)):
        for record in index[kind].values():
            verify(sources[record["asset"]], record)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".semantic-release-", dir=output.parent) as temporary:
        staging = Path(temporary) / "release"
        staging.mkdir()
        for name, archive in sources.items():
            shutil.copyfile(archive, staging / name)
        (staging / INDEX_ASSET).write_bytes(canonical_bytes(index) + b"\n")
        (staging / SUMS_ASSET).write_bytes(checksum_bytes(staging, index))
        release = release_metadata(index, staging / INDEX_ASSET)
        (staging / "release.json").write_bytes(canonical_bytes(release) + b"\n")
        verify_release_directory(staging)
        if output.exists() or output.is_symlink():
            if output.is_symlink() or verify_release_directory(output) != release:
                raise IntegrityError("semantic: existing release directory differs; refusing replacement")
            for name in [*release["assets"], "release.json"]:
                if file_sha256(output / name) != file_sha256(staging / name):
                    raise IntegrityError("semantic: existing release directory is not byte-identical")
        else:
            staging.rename(output)
    return release


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--deps-lock", type=Path, default=ROOT / "deps.lock.json")
    parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    parser.add_argument("--accepted-lock", type=Path, default=ROOT / "semantic-sdk.lock.json")
    parser.add_argument("--store", type=Path)
    parser.add_argument("--mirror")
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    from tools.semantic.reuse_release import load_accepted_index
    prior = load_accepted_index(args.accepted_lock, store_root=args.store, mirror=args.mirror, offline=args.offline)
    release = aggregate(args.assets, args.output, deps_lock=args.deps_lock,
                        profile_path=args.profile, previous_index=prior[1] if prior else None)
    print(canonical_bytes(release).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
