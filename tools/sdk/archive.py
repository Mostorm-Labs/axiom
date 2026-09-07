"""Deterministic archives and strict cross-platform path validation."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import stat
import tempfile
from typing import Any, Mapping
import zipfile

from .model import IntegrityError, SdkError, validate_component

FIXED_ZIP_TIME = (1980, 1, 1, 0, 0, 0)


def canonical_bytes(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=True, sort_keys=True,
                      separators=(",", ":"), allow_nan=False).encode("utf-8")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_sha256(path: Path, expected: str) -> None:
    if not path.is_file() or path.is_symlink() or file_sha256(path) != expected:
        raise IntegrityError(f"SHA-256 mismatch or missing file: {path}")


def _parts(name: str) -> tuple[str, ...]:
    # Apply Windows rules even on POSIX, before filesystem normalization.
    if not name or name.startswith("/") or "\\" in name:
        raise SdkError(f"unsafe archive path: {name!r}")
    parts = tuple(name.split("/"))
    for part in parts:
        validate_component(part)
    return parts


def _check_members(members: list[zipfile.ZipInfo], max_bytes: int, max_files: int) -> None:
    if len(members) > max_files or sum(m.file_size for m in members) > max_bytes:
        raise SdkError("archive exceeds extraction limit")
    explicit: set[str] = set()
    nodes: dict[str, tuple[str, bool]] = {}
    for member in members:
        name = member.filename[:-1] if member.is_dir() else member.filename
        parts = _parts(name)
        mode = member.external_attr >> 16
        kind = stat.S_IFMT(mode)
        if kind not in {0, stat.S_IFREG, stat.S_IFDIR} or (mode & 0o7000):
            raise SdkError(f"unsupported archive entry: {member.filename}")
        if kind == stat.S_IFDIR and not member.is_dir():
            raise SdkError(f"inconsistent archive entry: {member.filename}")
        if name.casefold() in explicit:
            raise SdkError(f"duplicate archive member: {name}")
        explicit.add(name.casefold())
        for length in range(1, len(parts) + 1):
            prefix = "/".join(parts[:length])
            is_dir = length < len(parts) or member.is_dir()
            prior = nodes.get(prefix.casefold())
            if prior is not None and prior != (prefix, is_dir):
                raise SdkError(f"archive path collision: {prefix}")
            nodes[prefix.casefold()] = (prefix, is_dir)


def safe_extract_zip(archive: Path, destination: Path, *, max_bytes: int = 2 * 1024**3,
                     max_files: int = 100_000) -> None:
    """Extract into an empty staging directory; never use extractall()."""
    if destination.is_symlink() or (destination.exists() and (not destination.is_dir() or any(destination.iterdir()))):
        raise SdkError(f"extraction destination must be an empty directory: {destination}")
    try:
        with zipfile.ZipFile(archive) as source:
            members = source.infolist()
            _check_members(members, max_bytes, max_files)
            destination.mkdir(parents=True, exist_ok=True)
            for member in members:
                path = destination.joinpath(*PurePosixPath(member.filename).parts)
                if member.is_dir():
                    path.mkdir(parents=True, exist_ok=True)
                    continue
                path.parent.mkdir(parents=True, exist_ok=True)
                with source.open(member) as incoming, path.open("xb") as outgoing:
                    shutil.copyfileobj(incoming, outgoing, 1024 * 1024)
                if path.stat().st_size != member.file_size:
                    raise IntegrityError(f"archive size mismatch: {member.filename}")
                mode = member.external_attr >> 16 & 0o777
                path.chmod(mode or 0o644)
    except (zipfile.BadZipFile, NotImplementedError) as error:
        raise IntegrityError(f"invalid SDK ZIP: {archive}") from error


def create_deterministic_zip(root: Path, output: Path, *, modes: Mapping[str, int] | None = None) -> None:
    root = root.resolve()
    if not root.is_dir() or output.resolve().is_relative_to(root):
        raise SdkError("archive root must exist and output must be outside it")
    entries: list[tuple[zipfile.ZipInfo, Path]] = []
    for path in sorted(root.rglob("*"), key=lambda p: p.relative_to(root).as_posix()):
        if path.is_symlink():
            raise SdkError(f"package input contains symlink: {path}")
        if path.is_dir():
            continue
        if not path.is_file():
            raise SdkError(f"unsupported package input: {path}")
        name = path.relative_to(root).as_posix()
        _parts(name)
        mode = modes[name] if modes is not None else (0o755 if path.stat().st_mode & 0o111 else 0o644)
        if mode not in {0o644, 0o755}:
            raise SdkError(f"unsupported package mode: {mode:o}")
        info = zipfile.ZipInfo(name, FIXED_ZIP_TIME)
        info.create_system = 3
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = (stat.S_IFREG | mode) << 16
        entries.append((info, path))
    _check_members([info for info, _ in entries], 2 * 1024**3, 100_000)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".sdk-zip-", delete=False) as temporary:
        staging = Path(temporary.name)
    try:
        with zipfile.ZipFile(staging, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for info, path in entries:
                with path.open("rb") as source, archive.open(info, "w", force_zip64=True) as target:
                    shutil.copyfileobj(source, target, 1024 * 1024)
        staging.replace(output)
    finally:
        staging.unlink(missing_ok=True)
