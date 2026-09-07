"""Shared Semantic host/runtime package envelope, not a second SDK resolver."""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import stat
import tempfile
import zipfile

from tools.sdk.archive import canonical_bytes, create_deterministic_zip, file_sha256, safe_extract_zip, verify_sha256
from tools.sdk.model import IntegrityError, SdkError, validate_component, validate_digest
from tools.semantic.contract import parse_json_bytes


def file_entries(root: Path, *, executable: str | None = None) -> list[dict]:
    files = []
    for path in sorted(root.rglob("*"), key=lambda p: p.relative_to(root).as_posix()):
        if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
            raise IntegrityError(f"semantic: package contains a link: {path}")
        if path.is_dir():
            continue
        if not path.is_file():
            raise IntegrityError(f"semantic: unsupported package entry: {path}")
        relative = path.relative_to(root).as_posix()
        for part in relative.split("/"):
            validate_component(part)
        if relative == "manifest.json":
            raise IntegrityError("semantic: payload may not shadow its manifest")
        files.append({"path": relative, "sha256": file_sha256(path), "size": path.stat().st_size,
                      "mode": 0o755 if relative == executable else 0o644})
    if not files:
        raise IntegrityError("semantic: empty package payload")
    return files


def create_package(root: Path, identity: dict, *, host: bool, output: Path) -> dict:
    identifier = hashlib.sha256(canonical_bytes(identity)).hexdigest()
    id_field = "hostToolId" if host else "runtimeId"
    prefix = "protoc" if host else "runtime"
    asset_name = f"axiom-semantic-{prefix}-{identity['key']}-{identifier}.zip"
    manifest = {"format": "axiom-semantic-host-tool-v2" if host else "axiom-semantic-runtime-v2",
                "schemaVersion": 2, id_field: identifier, "identity": identity,
                "files": file_entries(root, executable=identity.get("executable") if host else None)}
    manifest_bytes = canonical_bytes(manifest) + b"\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="semantic-package-", dir=output.parent) as directory:
        temporary = Path(directory)
        envelope = temporary / "envelope"
        shutil.copytree(root, envelope / "package")
        (envelope / "manifest.json").write_bytes(manifest_bytes)
        modes = {"manifest.json": 0o644, **{f"package/{item['path']}": item["mode"] for item in manifest["files"]}}
        archive = temporary / asset_name
        create_deterministic_zip(envelope, archive, modes=modes)
        record = {id_field: identifier, "identity": identity, "asset": asset_name,
                  "sha256": file_sha256(archive), "manifestSha256": hashlib.sha256(manifest_bytes).hexdigest(),
                  "size": archive.stat().st_size}
        output.mkdir(parents=True, exist_ok=True)
        target = output / asset_name
        if target.exists():
            verify_sha256(target, record["sha256"])
        else:
            archive.replace(target)
    return record


def read_archive_manifest(archive: Path) -> dict:
    try:
        with zipfile.ZipFile(archive) as source:
            candidates = [info for info in source.infolist() if info.filename == "manifest.json"]
            if len(candidates) != 1 or candidates[0].file_size > 8 * 1024**2:
                raise IntegrityError("semantic: missing, duplicate, or oversized manifest")
            return parse_json_bytes(source.read(candidates[0]))
    except zipfile.BadZipFile as error:
        raise IntegrityError("semantic: invalid package ZIP") from error


def _manifest(root: Path, record: dict, host: bool) -> dict:
    verify_sha256(root / "manifest.json", record["manifestSha256"])
    value = parse_json_bytes((root / "manifest.json").read_bytes())
    id_field = "hostToolId" if host else "runtimeId"
    expected_format = "axiom-semantic-host-tool-v2" if host else "axiom-semantic-runtime-v2"
    if set(value) != {"format", "schemaVersion", id_field, "identity", "files"} or value["format"] != expected_format or value["schemaVersion"] != 2:
        raise IntegrityError("semantic: incorrect package manifest format")
    if value[id_field] != record[id_field] or value["identity"] != record["identity"]:
        raise IntegrityError("semantic: manifest identity mismatch")
    if hashlib.sha256(canonical_bytes(value["identity"])).hexdigest() != record[id_field]:
        raise IntegrityError("semantic: noncanonical package identity")
    files = value["files"]
    if not isinstance(files, list) or not files:
        raise IntegrityError("semantic: manifest has no payload files")
    seen = set()
    for entry in files:
        if not isinstance(entry, dict) or set(entry) != {"path", "size", "sha256", "mode"}:
            raise IntegrityError("semantic: invalid file entry")
        path = entry["path"]
        if not isinstance(path, str) or path == "manifest.json":
            raise IntegrityError("semantic: invalid payload path")
        for part in path.split("/"):
            validate_component(part)
        if path.casefold() in seen:
            raise IntegrityError("semantic: duplicate payload path")
        seen.add(path.casefold())
        validate_digest(entry["sha256"])
        if type(entry["size"]) is not int or entry["size"] < 0 or entry["mode"] not in {0o644, 0o755}:
            raise IntegrityError("semantic: invalid payload size or mode")
    return value


def validate_payload(root: Path, record: dict, *, host: bool) -> dict:
    manifest = _manifest(root, record, host)
    paths = {entry["path"] for entry in manifest["files"]}
    actual = set()
    for path in root.rglob("*"):
        if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
            raise IntegrityError("semantic: materialized payload contains a link")
        if path.is_dir():
            continue
        if not path.is_file():
            raise IntegrityError("semantic: unsupported materialized payload entry")
        actual.add(path.relative_to(root).as_posix())
    if actual != paths | {"manifest.json"}:
        raise IntegrityError("semantic: payload file set does not match manifest")
    for entry in manifest["files"]:
        path = root.joinpath(*entry["path"].split("/"))
        verify_sha256(path, entry["sha256"])
        if path.stat().st_size != entry["size"]:
            raise IntegrityError(f"semantic: payload size mismatch: {entry['path']}")
        if os.name != "nt" and stat.S_IMODE(path.stat().st_mode) != entry["mode"]:
            raise IntegrityError(f"semantic: payload mode mismatch: {entry['path']}")
    return manifest


def install_payload(archive: Path, destination: Path, record: dict, *, host: bool) -> dict:
    verify_sha256(archive, record["sha256"])
    if archive.stat().st_size != record["size"]:
        raise IntegrityError("semantic: package archive size mismatch")
    safe_extract_zip(archive, destination)
    if {path.name for path in destination.iterdir()} != {"manifest.json", "package"}:
        raise IntegrityError("semantic: incorrect archive envelope")
    manifest = _manifest(destination, record, host)
    with zipfile.ZipFile(archive) as source:
        expected = {"manifest.json", *("package/" + entry["path"] for entry in manifest["files"])}
        if set(source.namelist()) != expected:
            raise IntegrityError("semantic: archive file set does not match manifest")
        for entry in manifest["files"]:
            mode = source.getinfo("package/" + entry["path"]).external_attr >> 16 & 0o777
            if mode != entry["mode"]:
                raise IntegrityError("semantic: archive file mode mismatch")
    package = destination / "package"
    for path in package.iterdir():
        target = destination / path.name
        if target.exists():
            raise IntegrityError("semantic: payload shadows metadata")
        path.rename(target)
    package.rmdir()
    return validate_payload(destination, record, host=host)
