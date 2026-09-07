"""Semantic adapter over generic SDK resolution; no downloads or source builds here."""
from __future__ import annotations

from pathlib import Path

from tools.sdk.archive import verify_sha256
from tools.sdk.model import (ArtifactRef, HostPlatform, IntegrityError, MaterializedArtifact,
                             ProviderPlan, ReleaseIndexRef, ResolveRequest, SdkError)
from tools.semantic.contract import read_json, validate_index, validate_v2_lock
from tools.semantic.package_host import install_host, validate_host
from tools.semantic.package_runtime import install_runtime, validate_runtime

_NATIVE = {'linux-x86_64': ('linux', 'x86_64', 'linux-x86_64'),
           'windows-x64': ('windows', 'x64', 'windows-x64-msvc-static'),
           'macos-arm64': ('macos', 'arm64', 'macos-arm64'),
           'macos-x64': ('macos', 'x64', 'macos-x64')}
_CROSS = {'android-arm64-v8a', 'android-x86_64', 'web-wasm32'}
_APPLE = {'macos-arm64', 'macos-x64', 'ios-arm64', 'ios-simulator-arm64'}


def select_keys(host: HostPlatform, target: str) -> tuple[str, str]:
    """Host keys name executable platforms; target keys name final library ABIs."""
    native = _NATIVE.get(host.key)
    if native is None or (host.os, host.arch) != native[:2]:
        raise SdkError(f'semantic: unsupported host: {host}')
    target = native[2] if target == 'native' else target
    allowed = _CROSS | (_APPLE if host.os == 'macos' else {native[2]})
    if target not in allowed:
        raise SdkError(f'semantic: unsupported host/target pair: {host.key} -> {target}')
    return ('macos-universal' if host.os == 'macos' else host.key), target


def select_lock_path(repo_root: Path, explicit: Path | None = None) -> Path:
    """Only absent v2 authority allows transition to v1; invalid v2 never does."""
    if explicit is not None:
        return explicit.expanduser().absolute()
    v2 = repo_root / 'semantic-sdk.lock.json'
    if v2.exists() or v2.is_symlink():
        validate_v2_lock(read_json(v2))  # a v1-format file here is not a transition
        return v2
    return repo_root / 'semantic-toolchain.lock.json'


class SemanticProvider:
    family = 'semantic'

    def __init__(self, lock_path: Path | None = None):
        self.lock_path = lock_path
        # Only records from a verified index may drive installation/validation.
        self._records: dict[ArtifactRef, dict] = {}

    def _lock(self, request: ResolveRequest) -> dict:
        path = self.lock_path if self.lock_path is not None else request.repo_root / 'semantic-sdk.lock.json'
        return validate_v2_lock(read_json(path))

    def index_ref(self, request: ResolveRequest) -> ReleaseIndexRef:
        select_keys(request.host, request.target)  # reject even before index transport
        lock = self._lock(request)
        return ReleaseIndexRef(self.family, lock['releaseSetId'], lock['repository'],
                               lock['releaseTag'], lock['indexAsset'], lock['indexSha256'])

    def plan(self, request: ResolveRequest, index_path: Path | None) -> ProviderPlan:
        host_key, target = select_keys(request.host, request.target)
        lock = self._lock(request)
        if index_path is None:
            raise SdkError('semantic: a verified release index is required')
        # Recheck against the lock snapshot used here, including direct protocol callers.
        verify_sha256(index_path, lock['indexSha256'])
        index = validate_index(read_json(index_path))
        if index['releaseSetId'] != lock['releaseSetId']:
            raise IntegrityError('semantic: index releaseSetId differs from its lock')
        refs = []
        for section, kind, key, id_field in (
                ('hostTools', 'host-tools', host_key, 'hostToolId'),
                ('runtimes', 'runtimes', target, 'runtimeId')):
            record = index[section][key]
            ref = ArtifactRef(self.family, kind, key, record[id_field], lock['repository'],
                              lock['releaseTag'], record['asset'], record['sha256'])
            if ref in self._records and self._records[ref] != record:
                raise IntegrityError('semantic: conflicting trusted records for the same artifact')
            self._records[ref] = record
            refs.append(ref)
        return ProviderPlan(self.family, tuple(refs), {
            'releaseSetId': lock['releaseSetId'], 'releaseTag': lock['releaseTag'],
            'indexSha256': lock['indexSha256'], 'hostTool': host_key, 'target': target,
            'protobufVersion': index['dependencies']['protobuf']['version'],
            'abseilVersion': index['dependencies']['abseil']['version'],
            'abi': index['runtimes'][target]['identity']['target']['abi'],
            'toolchain': index['runtimes'][target]['identity']['toolchain'],
        })

    def _record(self, ref: ArtifactRef) -> dict:
        if ref not in self._records:
            raise SdkError('semantic: artifact was not selected by a verified provider plan')
        return self._records[ref]

    def install(self, ref: ArtifactRef, archive: Path, staging_root: Path) -> None:
        record = self._record(ref)
        (install_host if ref.kind == 'host-tools' else install_runtime)(archive, staging_root, record)

    def validate(self, ref: ArtifactRef, materialized_root: Path) -> None:
        record = self._record(ref)
        (validate_host if ref.kind == 'host-tools' else validate_runtime)(materialized_root, record)

    def environment(self, plan: ProviderPlan, materialized: tuple[MaterializedArtifact, ...]) -> dict[str, str]:
        if (plan.family != self.family or len(materialized) != 2
                or tuple(item.ref for item in materialized) != plan.artifacts):
            raise SdkError('semantic: materialized pair differs from provider plan')
        host, runtime = (item.root.resolve(strict=True) for item in materialized)
        executable = self._record(plan.artifacts[0])['identity']['executable']
        return {'AXIOM_SEMANTIC_HOST_ROOT': str(host),
                'AXIOM_PROTOC': str((host / executable).resolve(strict=True)),
                'AXIOM_SEMANTIC_RUNTIME_ROOT': str(runtime),
                'AXIOM_SEMANTIC_SDK_ROOT': str(runtime),
                'CMAKE_PREFIX_PATH': str(runtime)}
