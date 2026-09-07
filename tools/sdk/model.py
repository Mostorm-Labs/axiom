"""Shared SDK value objects; dependency-specific identity stays in providers."""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import platform
import re
from typing import Literal, Mapping


class SdkError(RuntimeError):
    """An SDK request cannot be satisfied without violating its contract."""


class IntegrityError(SdkError):
    """Bytes or installed content differ from the pinned artifact."""


class OfflineError(SdkError):
    """An offline request lacks an already-valid local materialization."""


SourceKind = Literal["store", "mirror", "github"]
_TOKEN = re.compile(r"[a-z0-9][a-z0-9._-]*\Z")
_DIGEST = re.compile(r"[0-9a-f]{64}\Z")
_RESERVED = re.compile(r"(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?\Z", re.I)


def validate_component(value: str) -> str:
    if (not isinstance(value, str) or not value or value in {".", ".."}
            or value[-1:] in {" ", "."} or any(ord(c) < 32 for c in value)
            or any(c in '/\\:<>"|?*' for c in value) or _RESERVED.fullmatch(value)):
        raise SdkError(f"unsafe path component: {value!r}")
    return value


def validate_namespace(value: str) -> str:
    validate_component(value)
    if not _TOKEN.fullmatch(value):
        raise SdkError(f"invalid SDK namespace: {value!r}")
    return value


def validate_digest(value: str) -> str:
    if not isinstance(value, str) or not _DIGEST.fullmatch(value):
        raise SdkError(f"expected lowercase SHA-256 digest: {value!r}")
    return value


def _validate_ref(family: str, identity: str, repository: str, tag: str,
                  asset: str, digest: str) -> None:
    validate_namespace(family)
    validate_digest(identity)
    validate_digest(digest)
    validate_component(asset)
    if not isinstance(repository, str) or len(repository.split("/")) != 2:
        raise SdkError(f"invalid repository: {repository!r}")
    for part in repository.split("/"):
        validate_component(part)
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", part):
            raise SdkError(f"invalid repository: {repository!r}")
    if not isinstance(tag, str) or not tag or any(ord(c) < 33 for c in tag):
        raise SdkError(f"invalid release tag: {tag!r}")
    for component in tag.split("/"):
        validate_component(component)


@dataclass(frozen=True)
class HostPlatform:
    os: Literal["linux", "windows", "macos"]
    arch: str
    key: str


def detect_host_platform(system: str | None = None, machine: str | None = None) -> HostPlatform:
    system = platform.system() if system is None else system
    machine = platform.machine() if machine is None else machine
    arch = machine.lower()
    if system.lower() == "linux" and arch in {"x86_64", "amd64"}:
        return HostPlatform("linux", "x86_64", "linux-x86_64")
    if system.lower() == "windows" and arch in {"x86_64", "amd64"}:
        return HostPlatform("windows", "x64", "windows-x64")
    if system.lower() == "darwin":
        if arch in {"arm64", "aarch64"}:
            return HostPlatform("macos", "arm64", "macos-arm64")
        if arch in {"x86_64", "amd64"}:
            return HostPlatform("macos", "x64", "macos-x64")
    raise SdkError(f"unsupported SDK host: {system}/{machine}")


@dataclass(frozen=True)
class ReleaseIndexRef:
    family: str
    identity: str
    repository: str
    release_tag: str
    asset: str
    sha256: str

    def __post_init__(self) -> None:
        _validate_ref(self.family, self.identity, self.repository, self.release_tag, self.asset, self.sha256)


@dataclass(frozen=True)
class ArtifactRef:
    family: str
    kind: str
    key: str
    identity: str
    repository: str
    release_tag: str
    asset: str
    sha256: str

    def __post_init__(self) -> None:
        _validate_ref(self.family, self.identity, self.repository, self.release_tag, self.asset, self.sha256)
        validate_namespace(self.kind)
        validate_namespace(self.key)


@dataclass(frozen=True)
class MaterializedArtifact:
    ref: ArtifactRef
    root: Path
    source: SourceKind
    network_used: bool


@dataclass(frozen=True)
class ResolveRequest:
    repo_root: Path
    host: HostPlatform
    target: str
    store_root: Path
    mirror: str | None = None
    offline: bool = False


@dataclass(frozen=True)
class ProviderPlan:
    family: str
    artifacts: tuple[ArtifactRef, ...]
    metadata: Mapping[str, object]
