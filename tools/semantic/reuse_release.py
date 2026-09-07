#!/usr/bin/env python3
"""Stage unchanged package bytes only from the currently accepted v2 Release."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import sys
import tempfile

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import ArtifactRef, ReleaseIndexRef, IntegrityError, SdkError, validate_digest
from tools.sdk.store import SdkStore, default_store_root
from tools.sdk.transport import ensure_release_file
from tools.semantic.contract import ROOT, HOST_KEYS, RUNTIME_KEYS, read_json, validate_index, validate_v2_lock
from tools.semantic.package_host import verify_host_archive
from tools.semantic.package_runtime import verify_runtime_archive


def load_accepted_index(lock_path: Path, *, store_root: Path | None = None,
                        mirror: str | None = None, offline: bool = False) -> tuple[dict, dict] | None:
    # Absence is the only authority-level clean miss. Corruption is never a miss.
    if not lock_path.exists() and not lock_path.is_symlink():
        return None
    lock = validate_v2_lock(read_json(lock_path))
    store = SdkStore(store_root if store_root is not None else default_store_root())
    ref = ReleaseIndexRef("semantic", lock["releaseSetId"], lock["repository"], lock["releaseTag"],
                          lock["indexAsset"], lock["indexSha256"])
    with store.lock("release-index:semantic:" + ref.identity):
        result = ensure_release_file(ref, store.release_index_path(ref),
                                     mirror=mirror if mirror is not None else os.environ.get("AXIOM_SDK_MIRROR"),
                                     offline=offline)
        index = validate_index(read_json(result.path))
    if index["releaseSetId"] != lock["releaseSetId"]:
        raise IntegrityError("semantic: accepted index releaseSetId differs from its lock")
    return lock, index


def reuse_asset(lock_path: Path, kind: str, key: str, identifier: str, output: Path, *,
                store_root: Path | None = None, mirror: str | None = None,
                offline: bool = False) -> dict | None:
    validate_digest(identifier)
    if kind not in {"hostTools", "runtimes"} or key not in (HOST_KEYS if kind == "hostTools" else RUNTIME_KEYS):
        raise SdkError("semantic: unsupported reuse kind/key")
    accepted = load_accepted_index(lock_path, store_root=store_root, mirror=mirror, offline=offline)
    if accepted is None:
        return None
    lock, index = accepted
    record = index[kind][key]
    field, package_kind, verify = (("hostToolId", "host-tools", verify_host_archive) if kind == "hostTools"
                                   else ("runtimeId", "runtimes", verify_runtime_archive))
    if record[field] != identifier:
        return None
    ref = ArtifactRef("semantic", package_kind, key, identifier, lock["repository"], lock["releaseTag"],
                      record["asset"], record["sha256"])
    store = SdkStore(store_root if store_root is not None else default_store_root())
    output = output.absolute()
    with store.lock("archive:" + ref.sha256):
        result = ensure_release_file(ref, store.archive_path(ref),
                                     mirror=mirror if mirror is not None else os.environ.get("AXIOM_SDK_MIRROR"),
                                     offline=offline)
        verify(result.path, record)
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".semantic-reuse-", dir=output.parent) as temporary:
            staging = Path(temporary) / "package"
            staging.mkdir()
            shutil.copyfile(result.path, staging / ref.asset)
            (staging / "record.json").write_bytes(canonical_bytes(record) + b"\n")
            verify(staging / ref.asset, record)
            if output.exists() or output.is_symlink():
                if output.is_symlink() or not output.is_dir() or {p.name for p in output.iterdir()} != {ref.asset, "record.json"}:
                    raise IntegrityError("semantic: reuse output differs; refusing replacement")
                if any(p.is_symlink() for p in output.iterdir()) or read_json(output / "record.json") != record:
                    raise IntegrityError("semantic: reuse output identity differs")
                verify(output / ref.asset, record)
            else:
                staging.rename(output)
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lock", type=Path, default=ROOT / "semantic-sdk.lock.json")
    parser.add_argument("--kind", choices=("hostTools", "runtimes"), required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--identity", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--store", type=Path)
    parser.add_argument("--mirror")
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    record = reuse_asset(args.lock, args.kind, args.key, args.identity, args.output,
                         store_root=args.store, mirror=args.mirror, offline=args.offline)
    print(canonical_bytes({"status": "reused" if record is not None else "miss", "record": record}).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
