#!/usr/bin/env python3
"""Build, verify, and install the single hosted semantic-toolchain asset."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import stat
import tempfile
import urllib.request
import zipfile
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
FORMAT = "axiom-semantic-toolchain-sdk-v1"
IDENTITY_FORMAT = "axiom-semantic-toolchain-identity-v1"
RECIPE_VERSION = 1
FIXED_ZIP_TIME = (1980, 1, 1, 0, 0, 0)


class SdkError(RuntimeError):
    """Raised when a semantic SDK is malformed or does not match the lock."""


def canonical_bytes(value: Any) -> bytes:
    return json.dumps(
        value, ensure_ascii=True, sort_keys=True, separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def recipe_sha256() -> str:
    """Hash the producer recipe without making its checkout path part of identity."""
    digest = hashlib.sha256()
    for relative in ("tools/bootstrap_deps.py", "tools/semantic_sdk.py"):
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update((ROOT / relative).read_bytes().replace(b"\r\n", b"\n"))
        digest.update(b"\0")
    return digest.hexdigest()


def load_lock(path: Path = ROOT / "deps.lock.json") -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or "dependencies" not in value:
        raise SdkError("dependency lock is not an object with dependencies")
    return value


def make_identity(lock: dict[str, Any], toolchain: dict[str, Any]) -> tuple[dict[str, Any], str]:
    dependencies = lock["dependencies"]
    protobuf = dependencies["protobuf"]
    abseil = dependencies["abseil"]
    identity = {
        "format": IDENTITY_FORMAT,
        "target": {"os": "linux", "arch": "x86_64"},
        "protobuf": {
            "version": protobuf["version"],
            "edition": protobuf["edition"],
            "sourceSha256": protobuf["source_sha256"],
            "protocAssetSha256": protobuf["protoc_assets"]["linux-x86_64"]["sha256"],
        },
        "abseil": {
            "version": abseil["version"],
            "sourceSha256": abseil["source_sha256"],
        },
        "toolchain": {
            str(key): value for key, value in sorted(toolchain.items())
            if key != "installPath"
        },
        "recipe": {
            "version": RECIPE_VERSION,
            "sha256": recipe_sha256(),
            "cmakeBuildType": "Release",
            "cxxStandard": 20,
            "sharedLibraries": False,
            "tests": False,
        },
    }
    return identity, sha256_bytes(canonical_bytes(identity))


def _relative_files(root: Path) -> list[Path]:
    if not root.is_dir():
        raise SdkError(f"toolchain root is not a directory: {root}")
    result: list[Path] = []
    for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix()):
        if path.is_dir():
            continue
        if path.is_symlink():
            resolved = path.resolve()
            if root.resolve() not in resolved.parents:
                raise SdkError(f"toolchain symlink escapes root: {path}")
        if not path.is_file():
            raise SdkError(f"toolchain contains unsupported entry: {path}")
        result.append(path.relative_to(root))
    if not result:
        raise SdkError("toolchain root is empty")
    return result


def make_manifest(root: Path, identity: dict[str, Any], sdk_id: str) -> dict[str, Any]:
    files = []
    for relative in _relative_files(root):
        path = root / relative
        files.append({
            "path": relative.as_posix(),
            "sha256": file_sha256(path),
            "size": path.stat().st_size,
            "mode": stat.S_IMODE(path.stat().st_mode),
        })
    return {
        "format": FORMAT,
        "schemaVersion": 1,
        "sdkId": sdk_id,
        "identity": identity,
        "files": files,
    }


def create_archive(root: Path, manifest: dict[str, Any], output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".partial")
    if temporary.exists():
        temporary.unlink()
    manifest_bytes = json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True).encode("utf-8") + b"\n"
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        manifest_info = zipfile.ZipInfo("manifest.json", FIXED_ZIP_TIME)
        manifest_info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(manifest_info, manifest_bytes)
        for entry in manifest["files"]:
            relative = Path(entry["path"])
            source = root / relative
            info = zipfile.ZipInfo(f"toolchain/{relative.as_posix()}", FIXED_ZIP_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = (stat.S_IMODE(source.stat().st_mode) | stat.S_IFREG) << 16
            archive.writestr(info, source.read_bytes())
    temporary.replace(output)


def _safe_member(name: str) -> Path:
    relative = Path(name)
    if not name or relative.is_absolute() or ".." in relative.parts:
        raise SdkError(f"archive contains unsafe path: {name!r}")
    return relative


def _validate_manifest(manifest: Any, lock: dict[str, Any], expected_sdk_id: str) -> dict[str, Any]:
    if not isinstance(manifest, dict) or manifest.get("format") != FORMAT or manifest.get("schemaVersion") != 1:
        raise SdkError("unsupported semantic SDK manifest")
    identity = manifest.get("identity")
    if not isinstance(identity, dict):
        raise SdkError("semantic SDK identity is missing")
    if manifest.get("sdkId") != expected_sdk_id:
        raise SdkError("semantic SDK ID does not match the locked SDK")
    toolchain = identity.get("toolchain")
    if not isinstance(toolchain, dict):
        raise SdkError("semantic SDK toolchain identity is missing")
    _, actual_sdk_id = make_identity(lock, toolchain)
    if actual_sdk_id != expected_sdk_id:
        raise SdkError("semantic SDK identity does not match deps.lock.json")
    files = manifest.get("files")
    if not isinstance(files, list) or not files:
        raise SdkError("semantic SDK manifest has no files")
    paths = [item.get("path") for item in files if isinstance(item, dict)]
    if len(paths) != len(files) or len(paths) != len(set(paths)) or any(not isinstance(path, str) for path in paths):
        raise SdkError("semantic SDK manifest file list is invalid")
    required_paths = {
        ".canvas-semantic-toolchain.json",
        "bin/protoc",
        "lib/cmake/protobuf/protobuf-config.cmake",
        "lib/cmake/absl/abslConfig.cmake",
        "lib/cmake/utf8_range/utf8_range-config.cmake",
    }
    if not required_paths.issubset(set(paths)) or not any(
        path.startswith("lib/") and path.endswith(".a") for path in paths
    ):
        raise SdkError("semantic SDK is missing the required consumer contract")
    protoc_entry = next(item for item in files if item["path"] == "bin/protoc")
    if not protoc_entry.get("mode", 0) & 0o111:
        raise SdkError("semantic SDK protoc is not executable")
    return manifest


def verify_archive(archive_path: Path, destination: Path, lock: dict[str, Any], expected_sdk_id: str) -> dict[str, Any]:
    if not archive_path.is_file():
        raise SdkError(f"semantic SDK archive is missing: {archive_path}")
    with zipfile.ZipFile(archive_path) as archive:
        members = archive.infolist()
        for member in members:
            _safe_member(member.filename)
            if member.filename == "manifest.json":
                continue
            if not member.filename.startswith("toolchain/") or member.is_dir():
                raise SdkError(f"semantic SDK archive has unexpected member: {member.filename}")
        try:
            manifest = json.loads(archive.read("manifest.json"))
        except (KeyError, json.JSONDecodeError) as error:
            raise SdkError("semantic SDK archive has no valid manifest") from error
        manifest = _validate_manifest(manifest, lock, expected_sdk_id)
        expected_names = {"manifest.json", *(f"toolchain/{item['path']}" for item in manifest["files"])}
        actual_names = {member.filename for member in members}
        if actual_names != expected_names:
            raise SdkError("semantic SDK archive does not match manifest file set")
        temporary = Path(tempfile.mkdtemp(prefix="axiom-semantic-sdk-", dir=destination.parent))
        try:
            for item in manifest["files"]:
                target = temporary / item["path"]
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(f"toolchain/{item['path']}"))
                mode = archive.getinfo(f"toolchain/{item['path']}").external_attr >> 16 & 0o777
                if mode != item.get("mode"):
                    raise SdkError(f"semantic SDK file mode mismatch: {item['path']}")
                if mode:
                    target.chmod(mode)
                if target.stat().st_size != item["size"] or file_sha256(target) != item["sha256"]:
                    raise SdkError(f"semantic SDK file hash mismatch: {item['path']}")
            marker = json.loads((temporary / ".canvas-semantic-toolchain.json").read_text(encoding="utf-8"))
            dependencies = lock["dependencies"]
            if marker.get("format") != "canvas-semantic-toolchain-v1" or \
               marker.get("protobuf_version") != dependencies["protobuf"]["version"] or \
               marker.get("protobuf_edition") != dependencies["protobuf"]["edition"] or \
               marker.get("protobuf_source_sha256") != dependencies["protobuf"]["source_sha256"] or \
               marker.get("protobuf_asset_key") != "linux-x86_64" or \
               marker.get("protobuf_asset_sha256") != dependencies["protobuf"]["protoc_assets"]["linux-x86_64"]["sha256"] or \
               marker.get("abseil_version") != dependencies["abseil"]["version"] or \
               marker.get("abseil_source_sha256") != dependencies["abseil"]["source_sha256"]:
                raise SdkError("semantic SDK marker does not match deps.lock.json")
            destination.parent.mkdir(parents=True, exist_ok=True)
            backup = destination.with_name(destination.name + ".previous")
            if backup.exists():
                shutil.rmtree(backup)
            if destination.exists():
                destination.rename(backup)
            temporary.rename(destination)
            if backup.exists():
                shutil.rmtree(backup)
            return manifest
        except Exception:
            shutil.rmtree(temporary, ignore_errors=True)
            raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    package = subparsers.add_parser("package")
    package.add_argument("--root", type=Path, required=True)
    package.add_argument("--output", type=Path, required=True)
    package.add_argument("--toolchain-json", type=Path, required=True)
    verify = subparsers.add_parser("verify")
    verify.add_argument("--archive", type=Path, required=True)
    verify.add_argument("--destination", type=Path, required=True)
    verify.add_argument("--sdk-id", required=True)
    args = parser.parse_args()
    lock = load_lock()
    if args.command == "package":
        toolchain = json.loads(args.toolchain_json.read_text(encoding="utf-8"))
        identity, sdk_id = make_identity(lock, toolchain)
        manifest = make_manifest(args.root, identity, sdk_id)
        create_archive(args.root, manifest, args.output)
        print(json.dumps({"sdkId": sdk_id, "archive": str(args.output)}, sort_keys=True))
    else:
        manifest = verify_archive(args.archive, args.destination, lock, args.sdk_id)
        print(json.dumps({"sdkId": manifest["sdkId"], "destination": str(args.destination)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
