# Axiom Semantic SDK v2 Cross-Platform Release + Shared SDK Store Design

Date: 2026-09-07
Status: Proposed / design-approved in chat, implementation not yet started
Scope: Semantic Protobuf/Abseil host tooling, target runtimes, immutable release-set publication, lock/index authority, shared local SDK materialization, build-environment resolution, CI qualification, and developer consumption

## 1. Purpose

Axiom already has a working Linux-hosted semantic dependency producer/consumer path, but it is still shaped as a single Linux toolchain archive. That model is good enough for hosted Linux semantic CI, but it is not a complete dependency product for Windows/macOS development or for Android/iOS/Web cross-compilation.

This design upgrades the semantic dependency supply chain into a formal cross-platform SDK product with five properties:

1. Semantic dependencies are published as permanent immutable GitHub Release assets, not only short-lived GitHub Actions artifacts.
2. Host code-generation tools are separated from target runtime libraries so native and cross builds use the same model.
3. A complete release set covers Linux, Windows, macOS, iOS, Android, and Web targets.
4. Developer machines and CI materialize immutable SDK assets into a persistent shared Axiom SDK Store instead of relying on repo-local `.deps/protobuf` or manual symlinks.
5. A single repository lock selects one immutable release-set index; the index selects platform-specific content-addressed assets.

The immediate operational goal is to have this model published and consumable before GT-G1-08 begins. GT-G1-07 historical evidence remains untouched.

## 2. Current repository constraints

The current implementation has four structural limitations that v2 must remove.

### 2.1 Single-host identity

`tools/semantic_sdk.py` currently defines the semantic SDK identity around a fixed `linux/x86_64` target and packages a runnable `protoc` together with the installed Protobuf/Abseil runtime.

### 2.2 Single-asset lock

`semantic-toolchain.lock.json` identifies one Linux asset, one SDK ID, one release tag, and one SHA-256. It cannot express multiple host tools and target runtimes.

### 2.3 Linux-only build-environment resolver

`tools/setup_build_environment.py` currently rejects targets other than `linux-x86_64` and installs the semantic toolchain to `.deps/protobuf`.

### 2.4 Host and target are coupled in CMake

`runtime/semantic/CMakeLists.txt` currently requires the same Protobuf package to provide both `protobuf::libprotobuf` and `protobuf::protoc`, then executes `$<TARGET_FILE:protobuf::protoc>` for code generation.

That is valid for a native build where host and target match. It is not a sound cross-compilation model. `protoc` must run on the build host, while `libprotobuf` and Abseil must match the final target ABI.

## 3. Architectural decision

Semantic SDK v2 separates host tools from target runtimes while publishing them as one atomic immutable release set.

```text
                       deps.lock.json
                              |
                              v
                    Semantic SDK Producer
                              |
              +---------------+---------------+
              |                               |
              v                               v
         Host tool assets                Target runtime assets
     Linux / Windows / macOS       Linux / Windows / macOS / iOS /
                                  Android / Web
              |                               |
              +---------------+---------------+
                              |
                              v
                  semantic-sdk-index.json
                              |
                              v
                  Immutable GitHub Release
                              |
                              v
                   semantic-sdk.lock.json
                              |
                              v
                 Build Environment Resolver
                              |
                              v
                    Shared Axiom SDK Store
                              |
                 +------------+------------+
                 |                         |
                 v                         v
             AXIOM_PROTOC       AXIOM_SEMANTIC_RUNTIME_ROOT
                 |                         |
                 +------------+------------+
                              |
                              v
                            CMake
```

The release set is the authoritative published unit. Individual host/runtime assets are independently content-addressed so unchanged assets can be reused across release sets without rebuilding or redownloading.

## 4. Platform matrix

### 4.1 Host tool assets

Semantic v2 publishes three host `protoc` assets:

| Host key | Purpose |
| --- | --- |
| `linux-x86_64` | Linux x64 developer/CI hosts |
| `windows-x64` | Windows x64 developer/CI hosts |
| `macos-universal` | macOS Apple Silicon and Intel hosts |

These assets contain the runnable code generator and its manifest/identity metadata. They do not define the target runtime ABI.

Protobuf 36.0 already publishes official binaries for these hosts. Axiom should lock the exact upstream asset identities and repackage them deterministically as Axiom host-tool assets rather than compiling `protoc` from source for normal consumers.

### 4.2 Target runtime assets

Semantic v2 publishes these target runtime assets:

| Runtime key | ABI / toolchain intent |
| --- | --- |
| `linux-x86_64` | Linux x64 native runtime |
| `windows-x64-msvc-static` | MSVC-compatible ABI, static CRT, C++20 |
| `macos-arm64` | macOS arm64 runtime |
| `macos-x64` | macOS x64 runtime |
| `ios-arm64` | iPhoneOS arm64 runtime |
| `ios-simulator-arm64` | iOS Simulator arm64 runtime |
| `android-arm64-v8a` | Android arm64-v8a runtime |
| `android-x86_64` | Android x86_64 runtime |
| `web-wasm32` | Emscripten wasm32 runtime |

Runtime assets contain the target-side consumer contract: headers, static libraries, required CMake package metadata, license/provenance metadata, and manifest identity. They do not need to contain a target-native `protoc` executable.

### 4.3 Cross-compilation pairing

The resolver chooses host and target independently.

Examples:

```text
macOS arm64 host -> iOS arm64 target
  host tool: macos-universal
  runtime:   ios-arm64

Linux x64 host -> Android x86_64 target
  host tool: linux-x86_64
  runtime:   android-x86_64

Windows x64 host -> Windows x64 target
  host tool: windows-x64
  runtime:   windows-x64-msvc-static
```

This host/target split is a hard compatibility rule of Semantic SDK v2.

## 5. Identity model

Semantic v2 uses three distinct identities.

Every identity is computed from a canonical identity payload that does not contain the ID being computed. The computed ID is then stored beside that payload and verified by recomputation. IDs must never be defined as hashes of serialized objects containing themselves.

### 5.1 `hostToolId`

`hostToolId` answers: which exact host-runnable `protoc` package is this?

Its canonical identity payload must be derived from authority inputs that materially define the host tool, including at least:

- Protobuf version;
- upstream host asset identity and SHA-256;
- host platform/architecture key;
- Axiom host-tool packaging contract version;
- deterministic packaging recipe inputs that materially affect bytes or consumer behavior.

Unrelated repository source changes must not change `hostToolId`.

### 5.2 `runtimeId`

`runtimeId` answers: which exact target Protobuf/Abseil runtime ABI is this?

Its canonical identity payload must include at least:

- Protobuf source version and SHA-256;
- Abseil source version and SHA-256;
- target platform/architecture;
- target toolchain identity;
- ABI contract such as static/shared library mode, CRT contract, deployment target/API level, C++ standard, and sanitizer/runtime policy where relevant;
- semantic runtime build/package contract version.

Unrelated Axiom runtime/business source changes must not change `runtimeId`.

### 5.3 `releaseSetId`

`releaseSetId` answers: which complete Semantic SDK platform set is currently being published and accepted as one release?

It is the SHA-256 of a canonical release-set identity payload containing the dependency authority plus the sorted selected host-tool/runtime identity-and-digest records. The identity payload explicitly excludes `releaseSetId` itself, the release tag, URLs, timestamps, attestations, and other publication metadata. The computed `releaseSetId` is then written into the final index and can be recomputed by consumers from its identity-bearing payload.

If any selected asset identity/digest changes, the `releaseSetId` changes. A new release set may reuse unchanged `hostToolId` or `runtimeId` values from an earlier release.

## 6. Release index and repository lock

### 6.1 Repository lock

The repository will migrate from the single-platform `semantic-toolchain.lock.json` model to a v2 release-set lock, for example:

```json
{
  "format": "axiom-semantic-sdk-lock-v2",
  "repository": "Mostorm-Labs/axiom",
  "releaseTag": "semantic-sdk-v2-<releaseSetId-prefix>",
  "releaseSetId": "<full-release-set-id>",
  "indexAsset": "semantic-sdk-index.json",
  "indexSha256": "<sha256>"
}
```

The lock intentionally does not duplicate all platform asset records. It pins one immutable release index by release tag, release-set identity, and exact serialized index digest.

### 6.2 Release index

`semantic-sdk-index.json` is the canonical complete release description. It contains:

- format/schema version;
- `releaseSetId`;
- the canonical release-set identity payload or fields sufficient to reconstruct it exactly;
- dependency authority versions/digests;
- `hostTools` map keyed by supported host;
- `runtimes` map keyed by supported target runtime;
- each asset file name, SHA-256, content identity, and compatibility metadata;
- optional provenance/attestation references where useful.

Conceptually:

```json
{
  "format": "axiom-semantic-sdk-index-v2",
  "releaseSetId": "...",
  "dependencies": {
    "protobuf": "36.0",
    "abseil": "20250512.1"
  },
  "hostTools": {
    "windows-x64": {
      "hostToolId": "...",
      "asset": "axiom-semantic-protoc-windows-x64-....zip",
      "sha256": "..."
    }
  },
  "runtimes": {
    "windows-x64-msvc-static": {
      "runtimeId": "...",
      "asset": "axiom-semantic-runtime-windows-x64-msvc-static-....zip",
      "sha256": "...",
      "abi": {
        "compiler": "msvc-compatible",
        "crt": "static",
        "cxxStandard": 20
      }
    }
  }
}
```

The canonical release-set identity payload determines `releaseSetId`; the exact final serialized index bytes are independently pinned by `indexSha256`. This avoids self-referential identity while still making both semantic identity and exact file bytes verifiable.

## 7. Immutable GitHub Release contract

A formal Semantic SDK release is tagged as:

```text
semantic-sdk-v2-<releaseSetId[:16]>
```

The release contains the complete supported matrix:

- `semantic-sdk-index.json`;
- `SHA256SUMS`;
- three host-tool archives;
- nine target-runtime archives;
- GitHub provenance attestations for the index and SDK archives.

Release assets are permanent dependency artifacts. GitHub Actions artifacts remain temporary intra-workflow transport only.

### 7.1 Immutability

Published tags/assets must never be overwritten, deleted and recreated under the same identity, or silently replaced.

If a publication workflow encounters an existing tag, it may succeed only after proving the existing release/index/assets are byte-identical to the requested publication.

### 7.2 Complete release set

Every formal Semantic SDK v2 release contains a complete matrix even if only one target changed. Unchanged assets may be reused from a prior trusted release without rebuilding. For a new complete Release, reuse means staging the exact previously verified bytes and attaching those byte-identical archives to the new Release; it does not mean rebuilding identity-equivalent binaries or leaving the new Release dependent on mutable external state.

Therefore:

```text
release is complete
build is incremental
```

## 8. Producer and qualification architecture

### 8.1 PR qualification

Producer-authority changes on a pull request run classification and qualification but do not publish a GitHub Release.

The classifier determines which host/runtime identities are affected. Only those cells rebuild. Unchanged cells can be represented by known trusted asset identities for qualification/aggregation logic.

### 8.2 Main release workflow

After producer-authority changes merge to `main`, a formal Semantic SDK release workflow:

1. validates it is running from `main`;
2. computes planned host/runtime identities;
3. reuses trusted byte-identical immutable assets whose identity is unchanged;
4. builds/packages/verifies changed cells;
5. aggregates the complete `semantic-sdk-index.json` and `SHA256SUMS`;
6. verifies determinism and complete matrix coverage;
7. publishes the immutable GitHub Release;
8. generates a repository lock update;
9. opens a lock-only consumer PR rather than silently advancing consumer authority on `main`.

This preserves a two-phase authority boundary:

```text
Producer authority -> immutable published Release
Consumer authority -> repository lock explicitly accepts one Release Set
```

### 8.3 Trigger ownership

Producer rebuilds are driven by dependency/toolchain/package authority, not ordinary Axiom semantic business logic.

Expected invalidation rules include:

- Protobuf version/source change -> all host tools and all runtimes;
- Abseil version/source change -> all runtimes, host tools unchanged;
- Windows toolchain/CRT/ABI change -> Windows runtime only;
- Android NDK/API change -> Android runtimes only;
- Emscripten/toolchain change -> Web runtime only;
- Apple deployment/Xcode contract change -> affected Apple runtimes;
- host-tool packaging contract change -> affected host-tool assets;
- runtime packaging/build contract change -> affected runtimes;
- ordinary Axiom semantic/runtime source changes -> zero producer builds.

Static CI contract tests should enforce these boundaries.

## 9. Platform build and qualification expectations

### 9.1 Host tools

Each host-tool package must at minimum prove:

- exact upstream asset digest/version;
- deterministic Axiom packaging;
- manifest/file hash integrity;
- executable mode/format validity;
- `protoc --version` matches the locked dependency version.

### 9.2 Linux runtime

Build and source-free consumer qualification run on pinned Ubuntu/toolchain identity. Qualification includes configure, compile, link, and runtime smoke where applicable.

### 9.3 Windows runtime

The Windows runtime uses an MSVC-compatible ABI and static CRT contract aligned with Axiom's existing Windows Skia linkage direction. The contract must record compiler/toolchain identity, `/MT`-equivalent CRT semantics, C++20, and relevant iterator/debug ABI constraints.

Qualification includes source-free configure, compile, link, and native runtime smoke.

### 9.4 macOS runtimes

macOS arm64 and x64 are separate target runtime identities. Qualification proves correct architecture, package configuration, static linking, and native smoke for the matching runner/toolchain.

### 9.5 iOS runtimes

`ios-arm64` and `ios-simulator-arm64` are distinct runtime identities. Qualification includes source-free configure/compile/link. Simulator runtime smoke is required where practical; device runtime execution is not required for the SDK producer itself.

### 9.6 Android runtimes

Android arm64-v8a and x86_64 are built against the pinned Android NDK/API contract. Qualification includes source-free configure/compile/link. x86_64 emulator smoke may be used for release qualification where reliable, but artifact validity must not depend on an unavailable emulator.

### 9.7 Web runtime

Web uses the pinned Emscripten toolchain and wasm32 ABI. Qualification includes source-free configure/compile/link plus an executable smoke under Node or browser automation where appropriate.

## 10. Shared Axiom SDK Store

Semantic v2 removes the repo-local `.deps/protobuf` directory from the formal semantic dependency contract.

### 10.1 Purpose

The Axiom SDK Store is a persistent local materialization of immutable release assets. It is not authority and it is not a correctness-bypassing cache.

Authority remains:

```text
repository lock
-> immutable release index
-> asset SHA / hostToolId / runtimeId
```

The Store is merely a verified local copy of those authority artifacts.

### 10.2 Default locations

Default user-scoped locations should be platform-native:

- Windows: `%LOCALAPPDATA%\Axiom\sdk`
- macOS: `~/Library/Application Support/Axiom/sdk`
- Linux: `$XDG_DATA_HOME/axiom/sdk`, falling back to `~/.local/share/axiom/sdk`

Developers may override the root with:

```text
AXIOM_SDK_STORE=<path>
```

This supports an existing shared dependency directory, a fast local disk, or a mounted shared volume without per-worktree symlinks.

### 10.3 Content-addressed layout

The semantic portion of the store is keyed by immutable identities, conceptually:

```text
<Axiom SDK Store>/
  semantic/
    archives/
    release-sets/<releaseSetId>/semantic-sdk-index.json
    host-tools/<hostToolId>/...
    runtimes/<runtimeId>/...
```

The exact directory spelling may evolve, but identity-based separation is mandatory.

### 10.4 Materialization behavior

For a requested host/target pair the resolver:

1. reads the repository lock;
2. fetches/verifies the locked release index if not already materialized;
3. selects the required host tool and target runtime;
4. checks the SDK Store for the exact `hostToolId` and `runtimeId`;
5. validates local manifests/identity before use;
6. downloads only missing assets;
7. verifies SHA-256, manifest identity, file set, modes, and consumer contract;
8. atomically installs them into the Store;
9. exports stable build-environment paths.

A second worktree using the same identities performs no clone, source build, or network download.

## 11. Source resolution, offline mode, and mirrors

The resolver uses this source priority:

```text
1. verified Axiom SDK Store materialization
2. configured Axiom SDK mirror
3. locked GitHub Release
```

A mirror is transport only. It never replaces lock/index/asset identity verification.

Supported configuration should include:

```text
AXIOM_SDK_STORE
AXIOM_SDK_MIRROR
```

and an explicit `--offline` mode.

`--offline` accepts only already-valid local Store materializations. Missing assets fail closed. It must never trigger a source bootstrap fallback.

## 12. Build Environment Resolver contract

`tools/setup_build_environment.py` evolves from Linux semantic setup into the shared Axiom Build Environment Resolver.

### 12.1 Native usage

For native development:

```text
python tools/setup_build_environment.py --semantic
```

The resolver auto-detects the host and native target.

Examples:

```text
Windows host -> windows-x64-msvc-static runtime
macOS arm64 host -> macos-arm64 runtime
Linux x64 host -> linux-x86_64 runtime
```

### 12.2 Cross-target usage

For cross builds the user specifies the final program target:

```text
python tools/setup_build_environment.py --semantic --target ios-arm64
python tools/setup_build_environment.py --semantic --target android-arm64-v8a
python tools/setup_build_environment.py --semantic --target web-wasm32
```

`--target` always means final target runtime. Host detection remains independent.

### 12.3 Stable exported variables

The stable semantic build-environment contract is:

```text
AXIOM_SEMANTIC_HOST_ROOT
AXIOM_PROTOC
AXIOM_SEMANTIC_RUNTIME_ROOT
```

The resolver may derive CMake adapter variables such as:

```text
CMAKE_PREFIX_PATH=<AXIOM_SEMANTIC_RUNTIME_ROOT>
```

but `CMAKE_PREFIX_PATH` is not the semantic dependency identity and should not become the long-term Axiom API.

Compatibility variables may remain temporarily where migration requires them, but new workflows should consume the stable semantic roots.

### 12.4 Diagnostics

The resolver should expose a human-readable status mode, for example:

```text
python tools/setup_build_environment.py --status
```

The output should report:

- detected host;
- requested/resolved target;
- release tag and `releaseSetId`;
- selected `hostToolId` and path/source;
- selected `runtimeId` and ABI/path/source;
- whether network was used;
- whether each artifact came from Store, mirror, or GitHub Release.

Machine-readable facts remain available for CI/evidence.

## 13. CMake host/target separation

The Semantic CMake consumer contract must stop treating `protobuf::protoc` as part of the target runtime package.

### 13.1 Target runtime requirement

CMake continues to locate the target Protobuf package through the selected runtime root and requires `protobuf::libprotobuf`.

### 13.2 Host tool requirement

Code generation uses the separately resolved host executable:

```text
AXIOM_PROTOC
```

Conceptually:

```cmake
find_package(Protobuf CONFIG REQUIRED)
if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "Semantic runtime requires protobuf::libprotobuf")
endif()
if(NOT DEFINED AXIOM_PROTOC)
  message(FATAL_ERROR "Semantic code generation requires AXIOM_PROTOC")
endif()
```

The generated-source custom command executes `${AXIOM_PROTOC}`, not `$<TARGET_FILE:protobuf::protoc>`.

This split is required for all cross-compilation targets.

## 14. Relationship to `.deps`

This design does not delete or redesign every existing dependency path before GT-G1-08.

The intended boundary is:

```text
.deps            = repo-local lightweight/source dependencies still owned by current bootstrap paths
Axiom SDK Store  = heavyweight immutable binary SDK products
```

Semantic exits `.deps/protobuf` as a formal dependency path in v2.

`googletest`, `nlohmann-json`, `xxhash`, and other current lightweight dependencies may remain under `.deps` until separately migrated.

## 15. Relationship to Skia

Skia is not migrated in this Semantic SDK v2 implementation.

However, the Axiom SDK Store is deliberately designed as the common future materialization layer for immutable SDK products. Skia already has a mature target-matrix, immutable-release consumer model, so a later change may materialize Skia SDK assets into the same store without redesigning the Skia producer.

Long-term the store may contain:

```text
Axiom SDK Store
  semantic/
  skia/
  toolchains/
```

This future convergence must preserve existing Skia locks, release identities, and consumer verification semantics.

## 16. Migration and compatibility

### 16.1 GT-G1-07

GT-G1-07 historical evidence remains unchanged. Its existing semantic dependency evidence is not reopened or rematerialized solely for this migration.

The existing historical Semantic Toolchain v1 Release remains immutable and available for evidence reproduction.

### 16.2 GT-G1-08+

Before GT-G1-08 begins, `main` should have:

1. Semantic SDK v2 producer/index/store/resolver implementation;
2. the first complete v2 release published;
3. a v2 repository lock selecting that release set;
4. Linux/Windows/macOS native consumer qualification;
5. source-free cross-target qualification for iOS/Android/Web;
6. ordinary semantic CI using v2 instead of source-building Protobuf/Abseil.

GT-G1-08 and later semantic tasks inherit the new build-environment authority.

### 16.3 Historical v1 assets

Existing v1 release assets are never overwritten or deleted. v2 is a new publication family with new lock/index formats and identities.

## 17. Failure handling

Semantic v2 is fail-closed.

Consumer resolution fails when any of the following occurs:

- unsupported or malformed repository lock;
- locked release/index cannot be resolved;
- index digest or `releaseSetId` mismatch;
- host or target is unsupported;
- selected asset digest/identity mismatch;
- local Store manifest/file hashes are invalid;
- runtime ABI metadata is incompatible with the requested target;
- required CMake consumer files are missing;
- host `protoc` is missing, non-runnable, or version-incompatible;
- offline mode lacks a valid local materialization;
- a consumer attempts an unauthorized source fallback.

A missing Release asset never causes automatic Protobuf/Abseil source compilation in ordinary consumer mode.

## 18. Testing and acceptance requirements

Implementation is accepted only when all of the following are proven:

1. A complete Semantic SDK v2 release exists with the three host tools, nine target runtimes, index, checksums, and provenance metadata.
2. Release publication is immutable and idempotent for byte-identical existing releases.
3. The repository v2 lock pins exactly one release index by tag, `releaseSetId`, and SHA-256.
4. Host/runtime assets are independently content-addressed and unchanged identities can be reused across release sets.
5. Windows, macOS, and Linux native semantic builds consume v2 assets source-free.
6. iOS, Android, and Web semantic target builds consume a host `protoc` plus the correct target runtime source-free.
7. Semantic CMake no longer requires target `protobuf::protoc` and uses the separately resolved `AXIOM_PROTOC`.
8. The resolver uses the shared SDK Store without repo-local symlinks.
9. A second worktree with an already-populated Store performs no semantic source clone/build/download.
10. Missing assets download only the required immutable host/runtime packages, not the entire matrix.
11. `--offline` succeeds with a valid Store and fails closed when required materialization is absent.
12. Mirror transport is still checked against lock/index/asset authority.
13. CI and local developer flows use the same resolver, lock, index, and verification logic.
14. Producer invalidation tests prove ordinary semantic source changes do not rebuild dependency SDKs.
15. GT-G1-07 historical evidence/assets are unchanged.
16. Existing Skia locks/releases are unchanged by this work.

## 19. Expected developer workflow

After v2 lands, a new worktree should need only the build-environment resolver.

Native example:

```text
git clone ...
cd axiom
python tools/setup_build_environment.py --semantic
cmake ...
cmake --build ...
```

Cross-target example:

```text
python tools/setup_build_environment.py --semantic --target android-arm64-v8a
```

If the selected identities are already in the shared Store, setup is a local verification/resolution step. If they are missing, only the required immutable Release assets are downloaded and installed.

Developers no longer need to create `.deps/protobuf` symlinks, know the shared dependency folder layout, or rebuild Protobuf/Abseil merely because they changed worktrees or moved to Windows/macOS.

## 20. Non-goals

This work does not:

- modify Axiom semantic operation/runtime behavior;
- redesign schema semantics;
- rework GT-G1-07 evidence;
- delete historical Semantic v1 assets;
- overwrite any existing Release asset;
- migrate every lightweight `.deps` dependency;
- migrate Skia into the shared Store in the same change;
- introduce vcpkg, Conan, or another package manager as dependency authority;
- allow cache/mirror/store contents to bypass lock/index/manifest verification.

## 21. Acceptance boundary before GT-G1-08

This design is considered operationally complete before GT-G1-08 only when Axiom has a published, immutable, complete Semantic SDK v2 Release Set; `main` is locked to it through the v2 index/lock authority; Linux, Windows, and macOS native consumers plus iOS/Android/Web cross-target consumers are proven source-free; and local development uses the persistent shared Axiom SDK Store rather than `.deps/protobuf` symlinks or repeated third-party source builds.
