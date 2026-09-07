"""Family-neutral orchestration; providers interpret, never fetch, their index."""
from __future__ import annotations

from pathlib import Path
from typing import Protocol

from .model import (ArtifactRef, MaterializedArtifact, OfflineError, ProviderPlan,
                    ReleaseIndexRef, ResolveRequest, SdkError, validate_namespace)
from .store import SdkStore
from .transport import ensure_release_file


class SdkProvider(Protocol):
    family: str

    def index_ref(self, request: ResolveRequest) -> ReleaseIndexRef | None: ...
    def plan(self, request: ResolveRequest, index_path: Path | None) -> ProviderPlan: ...
    def install(self, ref: ArtifactRef, archive: Path, staging_root: Path) -> None: ...
    def validate(self, ref: ArtifactRef, materialized_root: Path) -> None: ...
    def environment(self, plan: ProviderPlan,
                    materialized: tuple[MaterializedArtifact, ...]) -> dict[str, str]: ...


def resolve_provider(provider: SdkProvider, request: ResolveRequest) -> dict[str, object]:
    family = validate_namespace(provider.family)
    store = SdkStore(request.store_root)
    try:
        index_ref = provider.index_ref(request)
        index_path = None
        index_fact = None
        network_used = False
        if index_ref is not None:
            if index_ref.family != family:
                raise SdkError("provider returned a foreign family index")
            index_path = store.release_index_path(index_ref)
            with store.lock(f"index:{family}:{index_ref.identity}:{index_ref.asset}"):
                resolved = ensure_release_file(index_ref, index_path, mirror=request.mirror, offline=request.offline)
            network_used = resolved.network_used
            index_fact = {"identity": index_ref.identity, "sha256": index_ref.sha256,
                          "root": str(index_path), "source": resolved.source,
                          "networkUsed": resolved.network_used}
        plan = provider.plan(request, index_path)
        if plan.family != family or any(ref.family != family for ref in plan.artifacts):
            raise SdkError("provider plan crosses a family namespace")
        keys = [(ref.kind, ref.key) for ref in plan.artifacts]
        if len(set(keys)) != len(keys):
            raise SdkError("provider plan contains duplicate artifact keys")
        materialized = []
        artifact_facts = []
        for ref in plan.artifacts:
            root = store.package_path(ref)
            source, network = "store", False
            if root.exists():
                # A bad installed package is not silently repaired by consumers.
                provider.validate(ref, root)
            else:
                if request.offline:
                    raise OfflineError(f"package is not materialized: {ref.key}")
                archive = store.archive_path(ref)
                with store.lock(f"archive:{ref.sha256}:{ref.asset}"):
                    transfer = ensure_release_file(ref, archive, mirror=request.mirror)
                source, network = transfer.source, transfer.network_used
                root = store.materialize_package(
                    ref, archive,
                    lambda archive, staging, ref=ref: provider.install(ref, archive, staging),
                    lambda root, ref=ref: provider.validate(ref, root),
                )
            materialized.append(MaterializedArtifact(ref, root, source, network))
            artifact_facts.append({"family": family, "kind": ref.kind, "key": ref.key,
                                   "identity": ref.identity, "sha256": ref.sha256,
                                   "root": str(root), "source": source, "networkUsed": network})
            network_used |= network
        environment = provider.environment(plan, tuple(materialized))
        if not isinstance(environment, dict) or any(not isinstance(k, str) or not isinstance(v, str)
                                                     for k, v in environment.items()):
            raise SdkError("provider environment must map string names to string values")
        return {"environment": environment,
                "facts": {"format": "axiom-sdk-resolution-v1", "family": family,
                          "host": request.host.key, "target": request.target,
                          "index": index_fact, "artifacts": artifact_facts,
                          "networkUsed": network_used, "metadata": dict(plan.metadata)}}
    except SdkError as error:
        raise type(error)(f"{family}: {error}") from error
    except (OSError, ValueError, RuntimeError) as error:
        raise SdkError(f"{family}: resolution failed: {error}") from error


class ProviderRegistry:
    """Explicit registration of trusted repository code, not downloaded plugins."""
    def __init__(self) -> None:
        self._providers: dict[str, SdkProvider] = {}

    def register(self, provider: SdkProvider) -> None:
        family = validate_namespace(provider.family)
        if family in self._providers:
            raise SdkError(f"SDK provider already registered: {family}")
        self._providers[family] = provider

    def resolve(self, family: str, request: ResolveRequest) -> dict[str, object]:
        if family not in self._providers:
            raise SdkError(f"unknown SDK family: {family}")
        return resolve_provider(self._providers[family], request)
