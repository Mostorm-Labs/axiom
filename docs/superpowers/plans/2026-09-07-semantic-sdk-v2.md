# Axiom Semantic SDK v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish and consume a permanent cross-platform Semantic SDK v2 Release Set before GT-G1-08, while introducing a generic Axiom SDK Store/resolver that future SDK families can reuse without changing existing Semantic or Skia authority.

**Architecture:** Split the work into a generic SDK-infrastructure layer and a Semantic provider. The generic layer owns Store discovery, archive caching, mirror/Release transport, SHA verification, atomic materialization, provider composition, offline behavior, host detection, and diagnostics; the Semantic provider owns its v2 profile, host/runtime identities, release index/lock, producer, consumer contract, and exported build roots. Semantic host `protoc` and target Protobuf/Abseil runtimes are independent artifacts inside one immutable complete Release Set; consumers resolve only the host tool and target runtime they need.

**Tech Stack:** Python 3.12, GitHub Actions, CMake 3.30+, Ninja, C++20, Protobuf 36.0, Abseil 20250512.1, Windows LLVM 22.1.8/MSVC ABI, Android NDK 27.2.12479018 API 26, Emscripten 6.0.6, Xcode/macOS/iOS toolchains, Python `unittest`, GitHub CLI for immutable Release publication.

**Spec:** `docs/superpowers/specs/2026-09-07-semantic-sdk-v2-design.md`

## Global Constraints

- GT-G1-07 historical evidence, source revision, workflow evidence contract, and historical Semantic v1 Release assets stay unchanged.
- Existing Skia locks, profiles, SDK identities, producer workflows, Release tags, and Release assets stay unchanged.
- Semantic v2 publishes exactly three host tools: `linux-x86_64`, `windows-x64`, `macos-universal`.
- Semantic v2 publishes exactly nine target runtimes: `linux-x86_64`, `windows-x64-msvc-static`, `macos-arm64`, `macos-x64`, `ios-arm64`, `ios-simulator-arm64`, `android-arm64-v8a`, `android-x86_64`, `web-wasm32`.
- Host `protoc` and target runtime are separate authority records and separate materializations.
- Windows runtime is MSVC-compatible, C++20, static-library, static-CRT (`/MT`) compatible.
- Android runtime uses the repository-pinned NDK `27.2.12479018` and API level `26`.
- Web runtime uses the repository-pinned Emscripten `6.0.6` / LLVM `22.1.8` toolchain contract and no pthread requirement.
- iOS uses the existing Axiom/Skia platform contract of deployment target `17.0`; macOS native runtimes record the exact Xcode SDK/compiler identity used to produce them and do not invent a new product minimum in this change.
- Ordinary consumers never run `tools/bootstrap_deps.py --semantic-codec` or any equivalent Protobuf/Abseil source build.
- Store, mirror, Actions cache, and filesystem location are never dependency authority; lock/index/identity/SHA validation remains mandatory.
- Missing or invalid consumer artifacts fail closed; `--offline` never falls back to source build.
- Adding a future SDK family is additive: no existing Semantic or Skia identity, lock format, Release asset, materialization, or exported contract may change merely because a new provider exists.
- Existing lightweight source dependencies under `.deps` remain out of scope except where current semantic tests need `--core`.
- The first v2 implementation may retain `semantic-toolchain.lock.json` as historical v1 metadata. Once `semantic-sdk.lock.json` exists, new consumer paths must prefer v2 and must not consume `.deps/protobuf`.

---

## File Structure

### Generic SDK infrastructure

- Create `tools/sdk/__init__.py` — package marker only.
- Create `tools/sdk/model.py` — shared immutable request/artifact/materialization dataclasses and host detection.
- Create `tools/sdk/archive.py` — SHA-256, safe ZIP extraction, deterministic ZIP writer, canonical JSON bytes.
- Create `tools/sdk/store.py` — Store-root discovery, archive/package/release-set paths, atomic materialization, local validation hooks.
- Create `tools/sdk/transport.py` — Store/mirror/GitHub Release source selection and download without family semantics.
- Create `tools/sdk/resolver.py` — provider protocol, provider registry, resolution/materialization orchestration, status facts.
- Create `tools/sdk/tests/test_store.py` — archive/store/atomic-install/offline tests.
- Create `tools/sdk/tests/test_resolver.py` — provider composition, mirror priority, diagnostics tests.
- Create `tools/sdk/tests/test_additive_provider.py` — proves a second provider can be added without changing the first provider contract.

### Semantic v2 provider and producer

- Create `tools/semantic/profile-v2.json` — 3 host + 9 runtime matrix and ABI/build contract versions.
- Create `tools/semantic/contract.py` — profile/lock/index validators and `hostToolId`, `runtimeId`, `releaseSetId` canonical identity functions.
- Create `tools/semantic/package_host.py` — deterministic repackaging/verification of official Protobuf host binaries.
- Create `tools/semantic/build_runtime.py` — source-build planner/executor for the nine target runtime ABIs; producer-only.
- Create `tools/semantic/package_runtime.py` — deterministic runtime manifest/archive/verification.
- Create `tools/semantic/aggregate.py` — complete release-set aggregation, identity collision checks, index/checksum generation.
- Create `tools/semantic/reuse_release.py` — reuse unchanged byte-identical host/runtime assets from the currently trusted v2 release.
- Create `tools/semantic/publish_release.py` — immutable/idempotent GitHub Release publication.
- Create `tools/semantic/provider.py` — v2 lock/index consumer adapter that maps host+target to generic `ArtifactRef`s and exports Semantic build roots.
- Create `tools/semantic/smoke/CMakeLists.txt`, `tools/semantic/smoke/smoke.proto`, `tools/semantic/smoke/main.cpp` — source-free package consumer probe used by producer cells.
- Create focused tests under `tools/semantic/tests/` for profile/identity, host package, runtime package/build plan, aggregation/reuse, provider/lock behavior.

### Existing files to migrate

- Modify `deps.lock.json` — add the official Protobuf 36.0 Windows x64 `protoc` asset identity only; do not change existing dependency versions.
- Modify `tools/setup_build_environment.py` — become generic-provider backed, cross-platform, Store-based, v2-preferred resolver while retaining lightweight `--core` bootstrap.
- Modify `tools/semantic_fetch.py` — become a compatibility CLI over the v2 Semantic provider; retain v1 behavior only when no v2 lock exists.
- Modify `tools/update_semantic_lock.py` — generate/validate `axiom-semantic-sdk-lock-v2` from an already-published index.
- Retain `tools/semantic_sdk.py` for v1 historical verification compatibility; do not extend it into v2.
- Modify `runtime/semantic/CMakeLists.txt` — require `protobuf::libprotobuf` from target runtime and `AXIOM_PROTOC` as a separate host executable.
- Modify `verification/tests/test_build_environment.py`, `verification/tests/test_semantic_lock.py`, `verification/tests/test_ci_trigger_boundaries.py`; add new v2-specific verification tests where noted below.

### GitHub Actions

- Create `.github/workflows/semantic-sdk-producer-contract.yml` — PR classifier/orchestrator.
- Create `.github/workflows/semantic-sdk-producer.yml` — reusable host/runtime producer matrix and optional complete aggregation.
- Create `.github/workflows/semantic-sdk-consumer-validation.yml` — reusable native + cross-target source-free consumer matrix.
- Create `.github/workflows/semantic-sdk-release.yml` — main-only manual promotion, immutable publish, attestation, generated lock PR.
- Modify `.github/workflows/build-environment-contract.yml` — validate v2 Store consumer path when `semantic-sdk.lock.json` is present and invoke representative cross-platform checks.
- Modify `.github/workflows/g1-semantic-codec.yml` — use v2 runtime root + host protoc and stop reading `.deps/protobuf` evidence.
- Retire `.github/workflows/semantic-toolchain-producer.yml` only after the v2 producer/release path is merged and the first v2 Release exists; until then it remains the historical v1 producer workflow.

---

## Shared Interfaces

The generic layer introduced in Tasks 1-2 uses these stable Python types:

```python
from dataclasses import dataclass
from pathlib import Path
from typing import Literal, Mapping, Protocol

SourceKind = Literal["store", "mirror", "github"]

@dataclass(frozen=True)
class HostPlatform:
    os: Literal["linux", "windows", "macos"]
    arch: str
    key: str

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
    mirror: str | None
    offline: bool

@dataclass(frozen=True)
class ProviderPlan:
    family: str
    artifacts: tuple[ArtifactRef, ...]
    metadata: Mapping[str, object]

class SdkProvider(Protocol):
    family: str
    def resolve(self, request: ResolveRequest) -> ProviderPlan: ...
    def install(self, ref: ArtifactRef, archive: Path, staging_root: Path) -> None: ...
    def validate(self, ref: ArtifactRef, materialized_root: Path) -> None: ...
    def environment(self, plan: ProviderPlan, materialized: tuple[MaterializedArtifact, ...]) -> dict[str, str]: ...
```

Semantic v2 exports:

```text
AXIOM_SEMANTIC_HOST_ROOT
AXIOM_PROTOC
AXIOM_SEMANTIC_RUNTIME_ROOT
CMAKE_PREFIX_PATH=<AXIOM_SEMANTIC_RUNTIME_ROOT>   # compatibility/CMake adapter
AXIOM_SEMANTIC_SDK_ROOT=<AXIOM_SEMANTIC_RUNTIME_ROOT>  # temporary compatibility alias
```

`AXIOM_PROTOC` is passed to CMake explicitly with `-DAXIOM_PROTOC="$AXIOM_PROTOC"`.

---

### Task 1: Build the generic archive and Store primitives

**Files:**
- Create: `tools/sdk/__init__.py`
- Create: `tools/sdk/model.py`
- Create: `tools/sdk/archive.py`
- Create: `tools/sdk/store.py`
- Create: `tools/sdk/tests/test_store.py`

**Interfaces:**
- Produces: `HostPlatform`, `ArtifactRef`, `MaterializedArtifact`, `detect_host_platform()`, `canonical_bytes()`, `file_sha256()`, `create_deterministic_zip()`, `safe_extract_zip()`, `default_store_root()`, `SdkStore`.
- Consumed by: every later Semantic consumer/producer task.

- [ ] **Step 1: Write failing tests for host detection, platform-native Store roots, content paths, and atomic install**

Add tests equivalent to:

```python
from pathlib import Path
import tempfile
import unittest

from tools.sdk.model import ArtifactRef, detect_host_platform
from tools.sdk.store import SdkStore


class SdkStoreTest(unittest.TestCase):
    def test_content_paths_are_family_and_identity_scoped(self):
        with tempfile.TemporaryDirectory() as directory:
            store = SdkStore(Path(directory))
            ref = ArtifactRef(
                family="semantic", kind="runtime", key="linux-x86_64",
                identity="a" * 64, repository="Mostorm-Labs/axiom",
                release_tag="semantic-sdk-v2-aaaaaaaaaaaaaaaa",
                asset="semantic-runtime-linux-x86_64.zip", sha256="b" * 64,
            )
            self.assertEqual(
                store.package_path(ref),
                Path(directory) / "packages/semantic/runtime" / ("a" * 64),
            )
            self.assertEqual(
                store.archive_path(ref),
                Path(directory) / "archives/sha256" / ("b" * 64) / ref.asset,
            )

    def test_failed_install_never_replaces_valid_materialization(self):
        # Seed a valid package, make the installer raise, then assert the old
        # materialization still exists byte-for-byte.
        ...
```

Use concrete temporary files for the second test; do not mock filesystem rename semantics.

- [ ] **Step 2: Run the Store test module and verify red state**

Run:

```bash
python3 -m unittest tools.sdk.tests.test_store -v
```

Expected: import failure because `tools.sdk` does not exist.

- [ ] **Step 3: Implement the shared dataclasses and host detection**

`detect_host_platform()` must normalize only supported developer hosts:

```python
("Linux", "x86_64" | "amd64") -> HostPlatform("linux", "x86_64", "linux-x86_64")
("Windows", "amd64" | "x86_64") -> HostPlatform("windows", "x64", "windows-x64")
("Darwin", "arm64" | "aarch64") -> HostPlatform("macos", "arm64", "macos-arm64")
("Darwin", "x86_64" | "amd64") -> HostPlatform("macos", "x64", "macos-x64")
```

Unsupported hosts raise `RuntimeError` with both original system and machine values.

- [ ] **Step 4: Implement deterministic ZIP and safe extraction helpers**

Use fixed ZIP time `(1980, 1, 1, 0, 0, 0)`, sorted POSIX paths, explicit file modes, `allow_nan=False`, and reject absolute/`..` archive members. Do not reuse `tools/semantic_sdk.py` internals; v1 must remain isolated.

- [ ] **Step 5: Implement `SdkStore` atomic materialization**

`SdkStore` must create generic layout:

```text
archives/sha256/<asset-sha>/<asset>
packages/<family>/<kind>/<identity>/
release-sets/<family>/<release-set-id>/index.json
```

Installation sequence is `validate existing -> create sibling staging dir -> provider install -> provider validate -> rename staging into final -> remove old backup only after success`.

- [ ] **Step 6: Run unit tests and whitespace check**

Run:

```bash
python3 -m unittest tools.sdk.tests.test_store -v
git diff --check
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add tools/sdk
git commit -m "feat: add generic Axiom SDK store primitives"
```

---

### Task 2: Add generic transport, provider composition, offline mode, and additive-provider proof

**Files:**
- Create: `tools/sdk/transport.py`
- Create: `tools/sdk/resolver.py`
- Create: `tools/sdk/tests/test_resolver.py`
- Create: `tools/sdk/tests/test_additive_provider.py`

**Interfaces:**
- Consumes: Task 1 types/Store.
- Produces: `ResolveRequest`, `ProviderPlan`, `SdkProvider`, `download_artifact()`, `resolve_provider()`.

- [ ] **Step 1: Write failing source-priority tests**

Cover exactly these behaviors:

```text
valid Store package -> no mirror/GitHub access
Store miss + filesystem mirror hit -> source=mirror, network_used=false
Store miss + HTTPS mirror hit -> source=mirror, network_used=true
Store/mirror miss -> locked GitHub Release URL
--offline + Store hit -> success
--offline + Store miss -> fail before transport
wrong downloaded SHA -> fail before provider install
```

Use a local `file://` or filesystem mirror and a temporary HTTP server only where a network flag is required.

- [ ] **Step 2: Write the additive-provider regression test**

Create two fake providers in the test. Resolve provider A, register provider B, resolve A again, and assert A's artifact refs, environment, Store path, and bytes are identical. Then resolve B and assert it uses `packages/fake-b/...` without touching A.

- [ ] **Step 3: Run tests and verify red state**

```bash
python3 -m unittest tools.sdk.tests.test_resolver tools.sdk.tests.test_additive_provider -v
```

Expected: FAIL because transport/resolver are not implemented.

- [ ] **Step 4: Implement transport priority and SHA enforcement**

`download_artifact()` receives only an `ArtifactRef`, destination archive path, mirror string, and offline flag. A mirror may be a filesystem directory, `file://` URI, or HTTP(S) base. It must always verify `ref.sha256` after transfer.

GitHub fallback URL is exactly:

```python
f"https://github.com/{ref.repository}/releases/download/{ref.release_tag}/{ref.asset}"
```

- [ ] **Step 5: Implement `resolve_provider()` orchestration**

For every artifact in `ProviderPlan.artifacts`:

1. validate exact existing package;
2. if absent and offline: raise;
3. ensure exact archive exists/download it;
4. verify SHA;
5. atomically install using provider hook;
6. validate installed package;
7. return `MaterializedArtifact` facts.

Return machine-readable facts containing family, key, identity, source, root, and network usage.

- [ ] **Step 6: Run generic SDK tests**

```bash
python3 -m unittest discover -s tools/sdk/tests -v
git diff --check
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add tools/sdk
git commit -m "feat: add generic SDK resolver transport"
```

---

### Task 3: Define the Semantic v2 profile, identities, index, and v2 lock contract

**Files:**
- Create: `tools/semantic/profile-v2.json`
- Create: `tools/semantic/contract.py`
- Create: `tools/semantic/tests/test_contract_v2.py`
- Modify: `deps.lock.json`
- Modify: `tools/semantic/tests/test_producer_change_classifier.py` only for the new locked Windows protoc key regression if needed; producer classification changes themselves land in Task 9.

**Interfaces:**
- Produces: `HOST_KEYS`, `RUNTIME_KEYS`, `load_profile()`, `validate_profile()`, `make_host_tool_identity()`, `make_runtime_identity()`, `make_release_set_identity()`, `validate_index()`, `validate_v2_lock()`.

- [ ] **Step 1: Resolve and lock the official Protobuf 36.0 Windows host asset**

Run:

```bash
gh api repos/protocolbuffers/protobuf/releases/tags/v36.0 \
  --jq '.assets[] | select(.name=="protoc-36.0-win64.zip") | [.browser_download_url,.digest] | @tsv'
```

Require a GitHub `sha256:` digest. Add the resulting URL/digest to `deps.lock.json` under `dependencies.protobuf.protoc_assets.windows-x64`. Do not alter the Linux or Darwin entries.

- [ ] **Step 2: Write failing profile/identity tests**

Tests must assert:

```python
self.assertEqual(set(profile["hostTools"]), {
    "linux-x86_64", "windows-x64", "macos-universal",
})
self.assertEqual(set(profile["runtimes"]), {
    "linux-x86_64", "windows-x64-msvc-static", "macos-arm64",
    "macos-x64", "ios-arm64", "ios-simulator-arm64",
    "android-arm64-v8a", "android-x86_64", "web-wasm32",
})
```

Also prove:

- install path/timestamp/release tag do not affect IDs;
- Protobuf source change changes every runtime ID and every host ID whose upstream asset changes;
- Abseil change changes runtimes but not host tools;
- Windows runtime metadata encodes static CRT and C++20;
- release-set identity is non-self-referential and changes if any selected asset SHA changes;
- a `runtimeId` cannot be accepted with two different archive SHA values in one trusted history check.

- [ ] **Step 3: Run tests and verify red state**

```bash
python3 -m unittest tools.semantic.tests.test_contract_v2 -v
```

- [ ] **Step 4: Create `profile-v2.json`**

The profile must encode explicit runner/build intent without family coupling. Use these current repository values:

```text
host linux-x86_64      upstream key linux-x86_64, runner ubuntu-24.04
host windows-x64       upstream key windows-x64, runner windows-2025
host macos-universal   upstream key darwin-universal, runner macos-15
runtime linux          ubuntu-24.04, static, C++20
runtime windows        windows-2025, LLVM 22.1.8, MSVC ABI, static CRT, static libs, C++20
runtime macOS arm64    macos-15, macosx, arm64, static, C++20
runtime macOS x64      macos-15, macosx, x86_64, static, C++20
runtime iOS device     macos-15, iphoneos, arm64, deployment 17.0
runtime iOS simulator  macos-15, iphonesimulator, arm64, deployment 17.0
runtime Android arm64  ubuntu-24.04, NDK 27.2.12479018, API 26
runtime Android x86_64 ubuntu-24.04, NDK 27.2.12479018, API 26
runtime Web wasm32     ubuntu-24.04, Emscripten 6.0.6, LLVM 22.1.8, pthread false
```

Set independent integer `hostContractVersion: 1` and `runtimeContractVersion: 1` fields so a producer behavior change that changes bytes/consumer contract has an explicit identity input.

- [ ] **Step 5: Implement canonical identity and validation functions**

Use `tools.sdk.archive.canonical_bytes`. `hostToolId` identity includes upstream asset SHA, Protobuf version, host key, and host contract version. `runtimeId` includes Protobuf/Abseil source SHAs, target profile entry, actual toolchain record, and runtime contract version. `releaseSetId` includes dependency authority plus sorted `{key, identity, sha256}` records and excludes itself/tag/URL/time.

- [ ] **Step 6: Run profile/identity tests plus Skia classifier regression**

```bash
python3 -m unittest \
  tools.semantic.tests.test_contract_v2 \
  tools.skia.tests.test_change_classifier -v
```

Expected: PASS and the added Windows `protoc_assets` entry must not trigger a Skia producer classification.

- [ ] **Step 7: Commit**

```bash
git add deps.lock.json tools/semantic/profile-v2.json tools/semantic/contract.py tools/semantic/tests/test_contract_v2.py tools/semantic/tests/test_producer_change_classifier.py
git commit -m "feat: define Semantic SDK v2 authority"
```

---

### Task 4: Package and verify the three host `protoc` assets

**Files:**
- Create: `tools/semantic/package_host.py`
- Create: `tools/semantic/tests/test_package_host_v2.py`

**Interfaces:**
- Consumes: Task 3 profile/identity functions and Task 1 deterministic ZIP utilities.
- Produces CLI: `python3 tools/semantic/package_host.py --host-key KEY --output DIR`.
- Produces archive name: `axiom-semantic-protoc-<host-key>-<hostToolId>.zip`.

- [ ] **Step 1: Write failing deterministic package tests**

Use a fake upstream Protobuf archive containing `bin/protoc` (or `bin/protoc.exe`) plus `include/google/protobuf/descriptor.proto`. Package it twice and assert identical bytes. Verify manifest fields, path traversal rejection, required include tree, executable mode on POSIX, and host identity recomputation.

- [ ] **Step 2: Run test and verify red state**

```bash
python3 -m unittest tools.semantic.tests.test_package_host_v2 -v
```

- [ ] **Step 3: Implement host archive creation**

Manifest format is `axiom-semantic-host-tool-v2`; payload is under `package/`; root contains `manifest.json`. The package contains only the upstream host tool files required for protoc execution/well-known proto imports plus Axiom metadata/licenses. Do not copy target libraries into host assets.

- [ ] **Step 4: Implement host verification**

Verifier checks manifest format, canonical `hostToolId`, exact file set/hash/mode, expected host key, upstream digest, and `bin/protoc`/`bin/protoc.exe` presence.

- [ ] **Step 5: Add an executable-version probe mode**

`--probe` executes packaged protoc after extraction and requires output exactly compatible with `libprotoc 36.0`. On macOS also run `lipo -info` or `file` in workflow to prove the upstream universal binary includes both arm64 and x86_64 slices.

- [ ] **Step 6: Run tests**

```bash
python3 -m unittest tools.semantic.tests.test_package_host_v2 -v
git diff --check
```

- [ ] **Step 7: Commit**

```bash
git add tools/semantic/package_host.py tools/semantic/tests/test_package_host_v2.py
git commit -m "feat: package Semantic host protoc assets"
```

---

### Task 5: Build, package, and source-free smoke the nine target runtimes

**Files:**
- Create: `tools/semantic/build_runtime.py`
- Create: `tools/semantic/package_runtime.py`
- Create: `tools/semantic/smoke/CMakeLists.txt`
- Create: `tools/semantic/smoke/smoke.proto`
- Create: `tools/semantic/smoke/main.cpp`
- Create: `tools/semantic/tests/test_runtime_build_plan.py`
- Create: `tools/semantic/tests/test_package_runtime_v2.py`

**Interfaces:**
- CLI producer build: `python3 tools/semantic/build_runtime.py --target KEY --install-root PATH --toolchain-json PATH`.
- CLI package: `python3 tools/semantic/package_runtime.py --target KEY --root PATH --toolchain-json PATH --output DIR`.
- Archive: `axiom-semantic-runtime-<target>-<runtimeId>.zip`.

- [ ] **Step 1: Write failing build-plan tests without compiling third-party source**

Pure unit tests inspect generated CMake arguments. Assert all targets set:

```text
-Dprotobuf_BUILD_TESTS=OFF
-Dprotobuf_BUILD_CONFORMANCE=OFF
-Dprotobuf_BUILD_EXAMPLES=OFF
-Dprotobuf_BUILD_LIBPROTOC=OFF
-Dprotobuf_BUILD_PROTOC_BINARIES=OFF
-Dprotobuf_ABSL_PROVIDER=package
-Dprotobuf_BUILD_SHARED_LIBS=OFF
-DCMAKE_CXX_STANDARD=20
```

And target-specific contracts:

```text
windows -> CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded, clang-cl/MSVC environment
macos-* -> CMAKE_OSX_ARCHITECTURES=arm64 or x86_64
ios-arm64 -> CMAKE_SYSTEM_NAME=iOS, iphoneos, arm64, deployment 17.0
ios-simulator-arm64 -> iOS simulator sysroot, arm64, deployment 17.0
android-* -> NDK toolchain, ABI, android-26
web-wasm32 -> emcmake/em++ toolchain, wasm32, no pthread requirement
```

- [ ] **Step 2: Write failing runtime package tests**

Fake install roots must contain:

```text
include/google/protobuf/*.h
lib/cmake/protobuf/protobuf-config.cmake
lib/cmake/absl/abslConfig.cmake
lib/cmake/utf8_range/utf8_range-config.cmake
at least one libprotobuf static library (.a or .lib)
licenses/Protobuf.txt
licenses/Abseil.txt
licenses/utf8_range.txt
```

Package twice, assert byte identity, verify `runtimeId`, ABI metadata, and reject any `bin/protoc` in runtime assets.

- [ ] **Step 3: Run tests and verify red state**

```bash
python3 -m unittest \
  tools.semantic.tests.test_runtime_build_plan \
  tools.semantic.tests.test_package_runtime_v2 -v
```

- [ ] **Step 4: Implement the producer-only runtime build executor**

Build Abseil first into one staging prefix, then Protobuf against that prefix. Download source archives only from `deps.lock.json` and verify their SHA-256 before extraction. Extract licenses into the install prefix. Do not call this script from consumer/resolver code.

- [ ] **Step 5: Implement runtime packaging/verification**

Manifest format `axiom-semantic-runtime-v2`; payload under `package/`; identity uses Task 3 `make_runtime_identity`. Enforce file hashes/modes, required CMake config files, static-library presence, target key, and absence of host executables.

- [ ] **Step 6: Create a source-free smoke project**

`smoke.proto` contains one minimal message. `main.cpp` constructs/serializes/parses the generated message and returns nonzero on mismatch. CMake receives `AXIOM_PROTOC` and runtime prefix, generates C++, links `protobuf::libprotobuf`, and never looks for `protobuf::protoc`.

- [ ] **Step 7: Add platform smoke modes**

Native Linux/Windows/macOS run the executable. Web runs generated JS/WASM under Node. iOS/Android cross cells must at least configure/compile/link the smoke project; Android emulator and iOS simulator execution may be additive but artifact validity cannot depend on device availability.

- [ ] **Step 8: Run pure unit tests**

```bash
python3 -m unittest \
  tools.semantic.tests.test_runtime_build_plan \
  tools.semantic.tests.test_package_runtime_v2 -v
git diff --check
```

- [ ] **Step 9: Commit**

```bash
git add tools/semantic/build_runtime.py tools/semantic/package_runtime.py tools/semantic/smoke tools/semantic/tests/test_runtime_build_plan.py tools/semantic/tests/test_package_runtime_v2.py
git commit -m "feat: build Semantic target runtime packages"
```

---

### Task 6: Aggregate complete Release Sets, reuse unchanged assets, publish immutably, and generate v2 locks

**Files:**
- Create: `tools/semantic/aggregate.py`
- Create: `tools/semantic/reuse_release.py`
- Create: `tools/semantic/publish_release.py`
- Modify: `tools/update_semantic_lock.py`
- Create: `tools/semantic/tests/test_aggregate_v2.py`
- Create: `tools/semantic/tests/test_reuse_release_v2.py`
- Modify: `verification/tests/test_semantic_lock.py`

**Interfaces:**
- Aggregate CLI: `python3 tools/semantic/aggregate.py --assets DIR --output DIR`.
- Publish CLI mirrors `tools/skia/publish_release.py` semantics.
- Lock generator reads an already-generated/published `semantic-sdk-index.json` and writes `semantic-sdk.lock.json`.

- [ ] **Step 1: Write failing complete-set aggregation tests**

Create 12 tiny valid fake archives and assert aggregation emits:

```text
semantic-sdk-index.json
SHA256SUMS
release.json
3 host archives
9 runtime archives
```

Assert missing one key fails, duplicate key fails, and `release.json.tag` is `semantic-sdk-v2-` plus the first 16 hex chars of `releaseSetId`.

- [ ] **Step 2: Write the identity-collision regression**

Given a prior trusted index where `runtimeId == "a" * 64` maps to SHA X, aggregation of a new candidate with the same runtimeId and SHA Y must fail with an identity-collision error instructing the producer to increment the relevant contract version. Same rule applies to `hostToolId`.

- [ ] **Step 3: Write immutable publication tests around command construction**

Mock `gh` calls. Existing tag with exact asset set and byte-identical downloads succeeds; target commit mismatch, missing asset, extra asset, or byte difference fails. No command may use `gh release upload --clobber`.

- [ ] **Step 4: Run tests and verify red state**

```bash
python3 -m unittest \
  tools.semantic.tests.test_aggregate_v2 \
  tools.semantic.tests.test_reuse_release_v2 \
  verification.tests.test_semantic_lock -v
```

- [ ] **Step 5: Implement aggregation and reuse**

Aggregation verifies every archive before copying it. `reuse_release.py` may reuse only an asset whose current locked v2 index has the same content identity and whose downloaded bytes match the indexed SHA. If no v2 lock exists, return a clean cache miss; first publication therefore builds all 12 assets.

- [ ] **Step 6: Implement v2 lock generation**

`tools/update_semantic_lock.py` now accepts:

```text
--index semantic-sdk-index.json
--tag semantic-sdk-v2-<derived tag>
--output semantic-sdk.lock.json
```

It validates the index, computes exact file SHA, verifies the tag matches `releaseSetId`, and writes only:

```json
{
  "format": "axiom-semantic-sdk-lock-v2",
  "repository": "Mostorm-Labs/axiom",
  "releaseTag": "semantic-sdk-v2-...",
  "releaseSetId": "...",
  "indexAsset": "semantic-sdk-index.json",
  "indexSha256": "..."
}
```

- [ ] **Step 7: Implement immutable publication**

Follow the existing Skia publication pattern but for exactly 14 release assets: 12 ZIPs + `semantic-sdk-index.json` + `SHA256SUMS`. `release.json` is workflow transport metadata and is not a Release asset.

- [ ] **Step 8: Run tests**

```bash
python3 -m unittest \
  tools.semantic.tests.test_aggregate_v2 \
  tools.semantic.tests.test_reuse_release_v2 \
  verification.tests.test_semantic_lock -v
git diff --check
```

- [ ] **Step 9: Commit**

```bash
git add tools/semantic/aggregate.py tools/semantic/reuse_release.py tools/semantic/publish_release.py tools/update_semantic_lock.py tools/semantic/tests/test_aggregate_v2.py tools/semantic/tests/test_reuse_release_v2.py verification/tests/test_semantic_lock.py
git commit -m "feat: add Semantic SDK v2 release set authority"
```

---

### Task 7: Implement the Semantic provider and move local consumption to the shared SDK Store

**Files:**
- Create: `tools/semantic/provider.py`
- Create: `tools/semantic/tests/test_provider_v2.py`
- Modify: `tools/setup_build_environment.py`
- Modify: `tools/semantic_fetch.py`
- Modify: `verification/tests/test_build_environment.py`

**Interfaces:**
- Consumes: generic resolver/Store plus `semantic-sdk.lock.json`/index.
- Produces: cross-platform Semantic `ProviderPlan`, Store materializations, environment/facts, `--offline`, `--status`, `AXIOM_SDK_STORE`, `AXIOM_SDK_MIRROR` support.

- [ ] **Step 1: Write failing provider-selection tests**

Test these exact pairings:

```text
linux-x86_64 host + linux-x86_64 target -> linux host tool + linux runtime
windows-x64 host + native -> windows host tool + windows-x64-msvc-static runtime
macos-arm64 host + native -> macos-universal host tool + macos-arm64 runtime
macos-x64 host + native -> macos-universal host tool + macos-x64 runtime
macos-arm64 host + ios-arm64 -> macos-universal + ios-arm64
linux-x86_64 host + android-x86_64 -> linux host tool + android-x86_64
linux-x86_64 host + web-wasm32 -> linux host tool + web-wasm32
```

Unsupported host-target pairs fail before any download.

- [ ] **Step 2: Extend build-environment tests for Store behavior**

Use a temporary Store and fake v2 provider/index. Assert second invocation performs zero downloads, produces the same roots, and does not create `.deps/protobuf`.

Also assert Windows executable path uses `bin/protoc.exe`; POSIX uses `bin/protoc`.

- [ ] **Step 3: Run tests and verify red state**

```bash
python3 -m unittest \
  tools.semantic.tests.test_provider_v2 \
  verification.tests.test_build_environment -v
```

- [ ] **Step 4: Implement provider lock/index resolution**

When `semantic-sdk.lock.json` exists, fetch/cache/verify the exact indexed `semantic-sdk-index.json`, validate `indexSha256` and `releaseSetId`, select exactly two artifacts, and return generic `ArtifactRef`s.

If v2 lock is absent, existing v1 fetch remains available solely for transition/historical use.

- [ ] **Step 5: Refactor `setup_build_environment.py`**

CLI changes:

```text
--target defaults to native
--store PATH overrides AXIOM_SDK_STORE/default
--mirror VALUE overrides AXIOM_SDK_MIRROR
--offline forbids transport
--status prints human-readable resolved facts
```

Default Store roots:

```text
Windows: %LOCALAPPDATA%\Axiom\sdk
macOS:   ~/Library/Application Support/Axiom/sdk
Linux:   $XDG_DATA_HOME/axiom/sdk or ~/.local/share/axiom/sdk
```

Keep `--core` calling only `bootstrap_deps.py --core`.

- [ ] **Step 6: Export the stable v2 environment**

Facts and GitHub env include:

```text
AXIOM_SEMANTIC_HOST_ROOT
AXIOM_PROTOC
AXIOM_SEMANTIC_RUNTIME_ROOT
AXIOM_SEMANTIC_SDK_ROOT=<runtime root>
CMAKE_PREFIX_PATH=<runtime root>
```

Do not export per-workflow `Protobuf_DIR`, `absl_DIR`, or `utf8_range_DIR` as the new authority contract.

- [ ] **Step 7: Make `semantic_fetch.py` a compatibility wrapper**

The v2 mode delegates to `SemanticProvider` + generic Store and prints one JSON facts line. Preserve old v1 CLI only when explicitly passed a v1 lock or when the repository has no v2 lock during transition.

- [ ] **Step 8: Run consumer unit suite**

```bash
python3 -m unittest \
  tools.sdk.tests.test_store \
  tools.sdk.tests.test_resolver \
  tools.sdk.tests.test_additive_provider \
  tools.semantic.tests.test_provider_v2 \
  verification.tests.test_build_environment \
  verification.tests.test_semantic_lock -v
git diff --check
```

- [ ] **Step 9: Commit**

```bash
git add tools/semantic/provider.py tools/semantic/tests/test_provider_v2.py tools/setup_build_environment.py tools/semantic_fetch.py verification/tests/test_build_environment.py
git commit -m "feat: resolve Semantic SDKs through shared store"
```

---

### Task 8: Separate host `protoc` from target Protobuf in Axiom CMake

**Files:**
- Modify: `runtime/semantic/CMakeLists.txt`
- Create: `verification/tests/test_semantic_cmake_contract.py`
- Modify: `.github/workflows/g1-semantic-codec.yml` only for command-line CMake variables/evidence path; broad workflow migration is completed in Task 11.

**Interfaces:**
- Consumes: `AXIOM_PROTOC` file path and target runtime prefix.
- Produces: CMake no longer requires `protobuf::protoc` target.

- [ ] **Step 1: Write the failing static CMake contract test**

Assert the CMake file contains a cache/file-path contract for `AXIOM_PROTOC`, references `protobuf::libprotobuf`, and does not contain `$<TARGET_FILE:protobuf::protoc>` or a fatal requirement for `protobuf::protoc`.

- [ ] **Step 2: Run the test and verify it fails**

```bash
python3 -m unittest verification.tests.test_semantic_cmake_contract -v
```

- [ ] **Step 3: Modify the CMake contract**

Use concrete logic:

```cmake
find_package(Protobuf CONFIG REQUIRED)
if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "Semantic runtime requires protobuf::libprotobuf")
endif()
set(AXIOM_PROTOC "" CACHE FILEPATH "Host protoc executable for Axiom semantic code generation")
if(NOT AXIOM_PROTOC OR NOT EXISTS "${AXIOM_PROTOC}")
  message(FATAL_ERROR "Semantic code generation requires a valid AXIOM_PROTOC host executable")
endif()
```

The custom command executes `"${AXIOM_PROTOC}"`; `DEPENDS` includes the executable path plus proto files, not `protobuf::protoc`.

- [ ] **Step 4: Update hosted Linux G1 configure command**

Use:

```bash
-DCMAKE_PREFIX_PATH="$AXIOM_SEMANTIC_RUNTIME_ROOT" \
-DAXIOM_PROTOC="$AXIOM_PROTOC"
```

Do not read target protoc from the CMake package.

- [ ] **Step 5: Run CMake contract tests plus current Linux semantic build using the existing consumer path**

Until the v2 lock exists, the compatibility aliases must keep the Linux build green:

```bash
python3 -m unittest verification.tests.test_semantic_cmake_contract verification.tests.test_build_environment -v
python3 tools/setup_build_environment.py --core --semantic --target linux-x86_64 --github-env /tmp/axiom-env --facts-output /tmp/axiom-facts.json
```

Then configure/build with the environment values emitted by the helper.

- [ ] **Step 6: Commit**

```bash
git add runtime/semantic/CMakeLists.txt verification/tests/test_semantic_cmake_contract.py .github/workflows/g1-semantic-codec.yml
git commit -m "refactor: separate Semantic host protoc from target runtime"
```

---

### Task 9: Replace the boolean producer classifier with host/runtime invalidation sets

**Files:**
- Modify: `tools/semantic/classify_producer_changes.py`
- Modify: `tools/semantic/tests/test_producer_change_classifier.py`

**Interfaces:**
- Produces JSON `{mode, host_tools, runtimes, reason}` where mode is `none`, `partial`, or `full`.

- [ ] **Step 1: Rewrite tests to cover every accepted invalidation rule**

Required cases:

```text
protobuf version/source -> all 3 hosts + all 9 runtimes
protobuf linux host-asset digest only -> linux host tool; runtimes unchanged
protobuf Windows host-asset digest only -> windows host tool; runtimes unchanged
abseil source/version -> 0 hosts + all 9 runtimes
runtime build/package code -> all runtimes
host package code -> all host tools
Windows-specific profile change -> windows runtime only
Android profile change -> two Android runtimes only
Web profile change -> web runtime only
Apple runtime profile change -> affected Apple runtimes only
consumer/provider/store/fetch/lock change -> no producer build
ordinary runtime/semantic business source -> no dependency producer build
malformed diff -> conservative full
```

- [ ] **Step 2: Run classifier tests and verify red state**

```bash
python3 -m unittest tools.semantic.tests.test_producer_change_classifier -v
```

- [ ] **Step 3: Implement family-aware classification**

The classifier must compare decoded `deps.lock.json` subtrees and, when `profile-v2.json` changes, compare host/runtime entries by key. It must never infer platform scope from filename substring when decoded contract comparison is available.

- [ ] **Step 4: Add matrix expansion helper**

A `--matrix` mode converts selected keys to GitHub matrix records containing `key`, `kind`, `os`, and `family`. Keep host and runtime matrices separate so workflows can package host tools without source-building runtimes.

- [ ] **Step 5: Run semantic and Skia classifiers together**

```bash
python3 -m unittest \
  tools.semantic.tests.test_producer_change_classifier \
  tools.skia.tests.test_change_classifier -v
```

Expected: Semantic changes do not start Skia; Skia changes do not start Semantic unless shared dependency authority truly overlaps.

- [ ] **Step 6: Commit**

```bash
git add tools/semantic/classify_producer_changes.py tools/semantic/tests/test_producer_change_classifier.py
git commit -m "feat: classify Semantic SDK producer cells"
```

---

### Task 10: Add PR qualification and reusable cross-platform producer workflows

**Files:**
- Create: `.github/workflows/semantic-sdk-producer-contract.yml`
- Create: `.github/workflows/semantic-sdk-producer.yml`
- Create: `verification/tests/test_semantic_sdk_workflows.py`
- Modify: `verification/tests/test_ci_trigger_boundaries.py`

**Interfaces:**
- `semantic-sdk-producer.yml` is `workflow_call` with JSON `host_tools`, JSON `runtimes`, and boolean `aggregate` inputs.
- Producer uploads one Actions artifact per cell and `semantic-sdk-release-set` only when `aggregate=true`.

- [ ] **Step 1: Write failing static workflow tests**

Assert:

- PR contract never contains `gh release create`;
- producer has separate host/runtime jobs;
- runtime job calls `build_runtime.py`, package twice, compares bytes, verifies, and source-free smokes;
- host job packages twice, compares bytes, verifies, and runs host probe;
- existing trusted v2 asset reuse occurs before source build;
- aggregate job requires all 12 keys;
- ordinary `runtime/semantic/**` changes do not trigger dependency producer jobs;
- build-environment consumer workflows still never call `--semantic-codec`.

- [ ] **Step 2: Run tests and verify red state**

```bash
python3 -m unittest verification.tests.test_semantic_sdk_workflows verification.tests.test_ci_trigger_boundaries -v
```

- [ ] **Step 3: Implement PR classifier/orchestrator**

Use `fetch-depth: 0`, run classifier unit tests, compute exact base/head lock/profile comparisons, and invoke reusable producer only when selected cells are non-empty.

- [ ] **Step 4: Implement host matrix jobs**

Use `ubuntu-24.04`, `windows-2025`, and `macos-15` according to profile. Each cell obtains exact upstream binary, computes expected identity, attempts trusted release reuse, otherwise packages twice. Probe `protoc --version`; on macOS verify universal slices.

- [ ] **Step 5: Implement runtime matrix jobs**

Install only the required pinned producer toolchain for the cell:

```text
Windows -> MSVC dev environment + pinned LLVM 22.1.8
Android -> NDK 27.2.12479018 / CMake
Web -> pinned Emscripten 6.0.6
Apple -> runner Xcode toolchain recorded in toolchain.json
Linux -> ubuntu-24.04 compiler/CMake/Ninja recorded in toolchain.json
```

Attempt reuse after computing toolchain/runtime identity. On miss, source-build, package twice, compare, verify, source-free smoke.

- [ ] **Step 6: Implement optional aggregate job**

Download all selected/reused complete-matrix artifacts and call `tools/semantic/aggregate.py`. `aggregate=true` is allowed only when all 3 host and 9 runtime keys are requested.

- [ ] **Step 7: Run static workflow tests**

```bash
python3 -m unittest verification.tests.test_semantic_sdk_workflows verification.tests.test_ci_trigger_boundaries -v
git diff --check
```

- [ ] **Step 8: Commit**

```bash
git add .github/workflows/semantic-sdk-producer-contract.yml .github/workflows/semantic-sdk-producer.yml verification/tests/test_semantic_sdk_workflows.py verification/tests/test_ci_trigger_boundaries.py
git commit -m "ci: add cross-platform Semantic SDK producer"
```

---

### Task 11: Add reusable source-free consumer qualification and migrate ordinary Semantic CI

**Files:**
- Create: `.github/workflows/semantic-sdk-consumer-validation.yml`
- Modify: `.github/workflows/build-environment-contract.yml`
- Modify: `.github/workflows/g1-semantic-codec.yml`
- Modify: `verification/tests/test_ci_trigger_boundaries.py`
- Modify: `verification/tests/test_build_environment.py`

**Interfaces:**
- Consumer validation accepts no source SDK inputs; it always starts from repository v2 lock + resolver.

- [ ] **Step 1: Add static tests for the consumer matrix**

Require native cells for Linux, Windows, macOS and cross cells for iOS device, iOS simulator, Android arm64, Android x86_64, Web wasm32. Every cell must invoke `tools/setup_build_environment.py --semantic` and must not invoke semantic source bootstrap.

- [ ] **Step 2: Implement native consumer jobs**

For Linux/Windows/macOS:

1. checkout;
2. materialize `--core --semantic` using the v2 lock;
3. configure Axiom Semantic with `CMAKE_PREFIX_PATH=$AXIOM_SEMANTIC_RUNTIME_ROOT` and `AXIOM_PROTOC`;
4. build;
5. run CTest;
6. run `setup_build_environment.py --status`;
7. invoke setup a second time and assert facts report Store hits and no download.

- [ ] **Step 3: Implement cross-target consumer jobs**

Use the platform toolchain, resolve the host tool + requested runtime, set `BUILD_TESTING=OFF`, configure Axiom with `CANVAS_BUILD_SEMANTIC=ON` and `CANVAS_SEMANTIC_ENABLE_PROTOBUF=ON`, and build `canvas_runtime_semantic`. Cross cells must not clone/build Protobuf or Abseil.

- [ ] **Step 4: Migrate G1 semantic evidence away from `.deps/protobuf`**

Replace the old marker copy with evidence from resolver facts plus exact installed host/runtime manifests:

```text
evidence-ci/g1-semantic-codec/build-environment.json
evidence-ci/g1-semantic-codec/semantic-host-manifest.json
evidence-ci/g1-semantic-codec/semantic-runtime-manifest.json
```

Keep all existing descriptor/golden/differential semantics unchanged.

- [ ] **Step 5: Update Build Environment Contract**

The existing historical Skia consumer job stays unchanged. Semantic job consumes v2 when `semantic-sdk.lock.json` exists. Add the reusable consumer-validation workflow as the cross-platform authority proof rather than duplicating matrix logic inline.

- [ ] **Step 6: Run static/unit suite**

```bash
python3 -m unittest \
  verification.tests.test_build_environment \
  verification.tests.test_ci_trigger_boundaries \
  verification.tests.test_semantic_sdk_workflows \
  verification.tests.test_semantic_cmake_contract -v
git diff --check
```

- [ ] **Step 7: Commit**

```bash
git add .github/workflows/semantic-sdk-consumer-validation.yml .github/workflows/build-environment-contract.yml .github/workflows/g1-semantic-codec.yml verification/tests/test_ci_trigger_boundaries.py verification/tests/test_build_environment.py
git commit -m "ci: qualify Semantic SDK v2 consumers"
```

---

### Task 12: Add main-only immutable release promotion and automatic lock PR

**Files:**
- Create: `.github/workflows/semantic-sdk-release.yml`
- Modify: `verification/tests/test_semantic_sdk_workflows.py`

**Interfaces:**
- Manual `workflow_dispatch` on `main` only.
- Calls complete producer matrix with `aggregate=true`.
- Publishes immutable Release.
- Opens or updates branch `automation/semantic-sdk-lock-<releaseSetId-prefix>` containing `semantic-sdk.lock.json` only.

- [ ] **Step 1: Add failing release-workflow tests**

Assert the workflow:

- rejects any ref other than `refs/heads/main`;
- requests full 3+9 matrix;
- downloads `semantic-sdk-release-set`;
- attests all 12 ZIPs plus index;
- calls `tools/semantic/publish_release.py`;
- generates v2 lock from the published index;
- creates a lock-only branch/PR;
- never commits the lock directly to `main`;
- grants `contents: write`, `pull-requests: write`, `id-token: write`, `attestations: write` only to the publish/promotion job.

- [ ] **Step 2: Implement the release workflow**

After immutable publication, create the lock branch from current `main`, write `semantic-sdk.lock.json`, commit with:

```text
chore: advance Semantic SDK lock
```

Push branch and create PR title:

```text
chore: advance Semantic SDK to <releaseSetId-prefix>
```

If the branch/PR already exists for the same ID, verify the file is byte-identical and succeed idempotently.

- [ ] **Step 3: Make first-release behavior explicit**

When no v2 lock exists, reusable producer cells all miss and build all 12 artifacts. The lock PR adds `semantic-sdk.lock.json`; it does not delete or rewrite `semantic-toolchain.lock.json`.

- [ ] **Step 4: Run workflow contract tests**

```bash
python3 -m unittest verification.tests.test_semantic_sdk_workflows -v
git diff --check
```

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/semantic-sdk-release.yml verification/tests/test_semantic_sdk_workflows.py
git commit -m "ci: publish immutable Semantic SDK v2 releases"
```

---

### Task 13: Full pre-merge verification of implementation branch

**Files:**
- No new source files unless verification exposes a defect in an earlier task.

**Interfaces:**
- Proves repository-side implementation before promotion.

- [ ] **Step 1: Run all generic/semantic SDK Python tests**

```bash
python3 -m unittest discover -s tools/sdk/tests -v
python3 -m unittest discover -s tools/semantic/tests -v
python3 -m unittest \
  verification.tests.test_build_environment \
  verification.tests.test_semantic_sdk \
  verification.tests.test_semantic_lock \
  verification.tests.test_semantic_cmake_contract \
  verification.tests.test_semantic_sdk_workflows \
  verification.tests.test_ci_trigger_boundaries -v
```

Expected: PASS.

- [ ] **Step 2: Prove v1 compatibility before v2 lock exists**

Run the current Linux semantic consumer with `semantic-toolchain.lock.json` and verify it still builds after the CMake host/target separation. This proves the implementation PR can merge before first v2 publication.

- [ ] **Step 3: Prove generic additive-provider isolation**

Run:

```bash
python3 -m unittest tools.sdk.tests.test_additive_provider -v
```

Expected: PASS with no Semantic/Skia file mutation.

- [ ] **Step 4: Verify protected historical assets/configuration are unchanged**

Run:

```bash
git diff main...HEAD -- r1-full-skia-sdk.lock.json tools/skia .github/workflows/skia-sdk-r1-full-producer.yml .github/workflows/r1-full-release.yml
```

Expected: empty diff for all protected Skia authority files.

Also confirm no GT-G1-07 exact-source/evidence path was modified solely for this SDK migration.

- [ ] **Step 5: Verify no consumer source-bootstrap regression**

```bash
grep -R "bootstrap_deps.py --semantic-codec" .github/workflows tools/setup_build_environment.py tools/semantic_fetch.py
```

Expected after v2 workflows are added: only the historical v1 producer workflow may contain that command; ordinary consumer workflows/resolver code must not.

- [ ] **Step 6: Run whitespace check**

```bash
git diff --check
```

- [ ] **Step 7: Commit any verification-only repair, otherwise do not create an empty commit**

If verification required a repair, commit the minimal repaired files with a focused message and rerun the affected test command.

---

### Task 14: Merge implementation, publish the first complete Semantic SDK v2 Release, and accept it through the lock PR

**Files/Remote artifacts:**
- GitHub Release created by `.github/workflows/semantic-sdk-release.yml`
- Generated PR adds `semantic-sdk.lock.json`
- Existing `semantic-toolchain.lock.json` remains historical v1 metadata

**Interfaces:**
- This task establishes the actual consumer authority needed by GT-G1-08.

- [ ] **Step 1: Merge the implementation PR to `main` only after producer-contract CI is green**

Required green checks include generic unit tests, Semantic producer qualification for every affected cell, build-environment/CI boundary contracts, and existing unrelated required repository checks.

- [ ] **Step 2: Dispatch `Semantic SDK Release` from `main`**

Use the workflow UI/CLI against the exact merged `main` commit. Do not dispatch from the design/feature branch.

- [ ] **Step 3: Verify the published Release is complete**

The Release tag must match `semantic-sdk-v2-<releaseSetId[:16]>` and contain exactly:

```text
3 axiom-semantic-protoc-*.zip
9 axiom-semantic-runtime-*.zip
semantic-sdk-index.json
SHA256SUMS
```

Verify all assets are permanent GitHub Release assets, not merely Actions artifacts.

- [ ] **Step 4: Verify the generated lock PR is lock-only and exact**

The PR adds `semantic-sdk.lock.json` whose `releaseTag`, `releaseSetId`, and `indexSha256` match the published Release index. It must not modify runtime/schema/Skia/GT-G1-07 files.

- [ ] **Step 5: Let the lock PR run v2 consumer qualification**

Required evidence:

```text
Linux native semantic build/test source-free
Windows native semantic build/test source-free
macOS native semantic build/test source-free
iOS device compile/link source-free
iOS simulator compile/link source-free
Android arm64 compile/link source-free
Android x86_64 compile/link source-free
Web wasm32 compile/link + Node/browser smoke source-free
second-resolution Store-hit proof
historical Skia consumer proof unchanged
G1 Semantic Codec green with v2 host/runtime split
```

- [ ] **Step 6: Merge the lock PR**

After merge, `main` consumer authority is `semantic-sdk.lock.json`. New Semantic setup must use the shared SDK Store and must not materialize `.deps/protobuf`.

- [ ] **Step 7: Re-run/verify post-merge `main` checks**

Confirm Build Environment Contract, Semantic SDK Consumer Validation, G1 Semantic Codec, CI Boundary Contract, and any required branch checks are green on the lock merge commit.

- [ ] **Step 8: Validate developer experience on at least one local Windows or macOS machine**

Run:

```text
python tools/setup_build_environment.py --semantic --status
```

Then run it again. First run may download two immutable assets; second run must report Store hits and no network. No symlink/junction/admin privilege and no Protobuf/Abseil source build is allowed.

- [ ] **Step 9: Record GT-G1-08 readiness**

GT-G1-08 may start only when `main` contains the accepted v2 lock and the post-merge checks above are green. Its branch base must be that latest `main` revision.

---

## Final Acceptance Mapping

The implementation is complete only when all spec requirements map to evidence as follows:

1. Permanent immutable Release assets -> Task 12 + Task 14 Release inspection.
2. Host/target split -> Tasks 3-5 + Task 8 CMake contract.
3. Full 3+9 platform matrix -> Tasks 3, 10, 11, 14.
4. Generic shared Store -> Tasks 1, 2, 7.
5. Single v2 release-set lock/index -> Tasks 3, 6, 12, 14.
6. Additive future SDK family -> Task 2 synthetic second-provider test.
7. Content-addressed asset reuse -> Tasks 1, 6, 10.
8. No identity collision under same `hostToolId`/`runtimeId` -> Task 6 regression.
9. Windows static CRT/MSVC ABI -> Tasks 3, 5, 10, 11.
10. iOS/Android/Web cross-compilation -> Tasks 5, 10, 11.
11. No repo-local `.deps/protobuf` for v2 -> Tasks 7, 11, 14.
12. Offline/mirror fail-closed behavior -> Tasks 2, 7.
13. CI/local same resolver and authority -> Tasks 7, 11.
14. Producer invalidation boundaries -> Task 9 + PR producer contract.
15. GT-G1-07 unchanged -> Tasks 13-14 verification.
16. Skia authority unchanged -> Tasks 13-14 verification.
17. Main-only promotion and lock PR separation -> Task 12.
18. GT-G1-08 starts on accepted v2 baseline -> Task 14.

## Execution Order and Review Gates

Tasks 1-2 establish generic infrastructure and can be reviewed independently. Tasks 3-6 establish Semantic producer authority without changing ordinary consumer behavior. Tasks 7-8 introduce the consumer/CMake boundary while retaining v1 transition compatibility. Tasks 9-12 wire CI and publication. Task 13 is the implementation-branch gate. Task 14 is the remote Release/lock-promotion gate and is required before GT-G1-08.

Do not collapse Task 14 into the implementation PR: producer authority must first exist on `main`, then publish an immutable Release, then consumer authority advances through the generated lock PR.
