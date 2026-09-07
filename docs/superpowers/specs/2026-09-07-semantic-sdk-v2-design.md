# Axiom Semantic SDK v2 Cross-Platform Release + Shared SDK Store Design

Date: 2026-09-07
Status: Proposed / design-approved in chat, implementation not yet started
Scope: Semantic Protobuf/Abseil host tooling, target runtimes, immutable release-set publication, lock/index authority, generic shared SDK materialization, build-environment resolution, CI qualification, developer consumption, and future SDK-family extensibility.

## 1. Purpose

Axiom already has a working Linux-hosted semantic dependency producer/consumer path, but it is still shaped as a single Linux toolchain archive. That model is sufficient for hosted Linux semantic CI, but it is not a complete dependency product for Windows/macOS development or Android/iOS/Web cross-compilation.

This design upgrades the semantic dependency supply chain into a formal cross-platform SDK product with six properties:

1. Semantic dependencies are published as permanent immutable GitHub Release assets, not only short-lived GitHub Actions artifacts.
2. Host code-generation tools are separated from target runtime libraries so native and cross builds use the same model.
3. A complete release set covers Linux, Windows, macOS, iOS, Android, and Web targets.
4. Developer machines and CI materialize immutable SDK assets into a persistent shared Axiom SDK Store instead of relying on repo-local `.deps/protobuf` or manual symlinks.
5. A single repository lock selects one immutable release-set index; the index selects platform-specific content-addressed assets.
6. The Store/resolver layer is generic Axiom infrastructure. Semantic is the first provider using it, not the owner of the infrastructure, so future SDK families can be added without changing existing Semantic or Skia authority.

The immediate operational goal is to have this model published and consumable before GT-G1-08 begins. GT-G1-07 historical evidence remains untouched.

## 2. Current repository constraints

### 2.1 Single-host identity

`tools/semantic_sdk.py` currently defines semantic SDK identity around a fixed `linux/x86_64` target and packages a runnable `protoc` together with the installed Protobuf/Abseil runtime.

### 2.2 Single-asset lock

`semantic-toolchain.lock.json` identifies one Linux asset, one SDK ID, one release tag, and one SHA-256. It cannot express multiple host tools and target runtimes.

### 2.3 Linux-only build-environment resolver

`tools/setup_build_environment.py` currently rejects targets other than `linux-x86_64` and installs the semantic toolchain to `.deps/protobuf`.

### 2.4 Host and target are coupled in CMake

`runtime/semantic/CMakeLists.txt` currently requires the same Protobuf package to provide both `protobuf::libprotobuf` and `protobuf::protoc`, then executes `$<TARGET_FILE:protobuf::protoc>` for code generation.

That is valid only for a native build where host and target match. `protoc` must run on the build host, while `libprotobuf` and Abseil must match the final target ABI.

### 2.5 Dependency infrastructure is still family-specific

Current fetching/materialization logic is organized around individual dependency families. If that pattern were extended directly, adding PDF, media, toolchain, or another SDK family would require more family-specific branches in the resolver and repeated implementations of Store/fetch/verify/offline/mirror behavior.

Semantic SDK v2 must avoid becoming a new special case of the same problem.

## 3. Architectural decision

Semantic SDK v2 separates host tools from target runtimes while publishing them as one atomic immutable release set.

At the same time, the local Store, immutable materialization, transport selection, verification, and generic host/target resolution mechanics are owned by **Axiom SDK Infrastructure**, outside the Semantic provider.

```text
                    dependency / producer authority
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
                 Axiom SDK Infrastructure
              +---------------+----------------+
              |               |                |
          Resolver Core   Verify/Transport   Shared Store
              |                                |
              +---------------+----------------+
                              |
                              v
                    Semantic Provider
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

The Semantic release set is the authoritative published unit for Semantic. The generic Store is not authority. Individual host/runtime assets remain content-addressed so unchanged assets can be reused across release sets without rebuilding or redownloading.

## 4. Platform matrix

### 4.1 Host tool assets

Semantic v2 publishes three host `protoc` assets:

| Host key | Purpose |
| --- | --- |
| `linux-x86_64` | Linux x64 developer/CI hosts |
| `windows-x64` | Windows x64 developer/CI hosts |
| `macos-universal` | macOS Apple Silicon and Intel hosts |

These assets contain the runnable code generator and manifest/identity metadata. They do not define the target runtime ABI.

Protobuf 36.0 already publishes official binaries for these hosts. Axiom should lock exact upstream asset identities and repackage them deterministically as Axiom host-tool assets rather than compiling `protoc` from source for normal consumers.

### 4.2 Target runtime assets

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

Runtime assets contain headers, static libraries, required CMake package metadata, license/provenance metadata, and manifest identity. They do not need a target-native `protoc` executable.

### 4.3 Cross-compilation pairing

The resolver chooses host and target independently.

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

This split is a hard compatibility rule.

## 5. Identity model

Every identity is computed from a canonical identity payload that does not contain the ID being computed. The computed ID is stored beside that payload and verified by recomputation.

### 5.1 `hostToolId`

Answers: which exact host-runnable `protoc` package is this?

Its canonical identity payload includes at least:

- Protobuf version;
- upstream host asset identity and SHA-256;
- host platform/architecture key;
- Axiom host-tool packaging contract version;
- deterministic packaging recipe inputs that materially affect bytes or consumer behavior.

Unrelated repository source changes must not change `hostToolId`.

### 5.2 `runtimeId`

Answers: which exact target Protobuf/Abseil runtime ABI is this?

Its canonical identity payload includes at least:

- Protobuf source version and SHA-256;
- Abseil source version and SHA-256;
- target platform/architecture;
- target toolchain identity;
- ABI contract, including static/shared mode, CRT contract, deployment target/API level, C++ standard, and sanitizer/runtime policy where relevant;
- Semantic runtime build/package contract version.

Unrelated Axiom runtime/business source changes must not change `runtimeId`.

### 5.3 `releaseSetId`

Answers: which complete Semantic SDK platform set is currently being published and accepted as one release?

It is the SHA-256 of a canonical release-set identity payload containing dependency authority plus the sorted selected host-tool/runtime identity-and-digest records. The payload excludes `releaseSetId` itself, release tag, URLs, timestamps, attestations, and other publication metadata.

If any selected asset identity/digest changes, `releaseSetId` changes. A new release set may reuse unchanged `hostToolId` or `runtimeId` values from an earlier release.

### 5.4 Family identity isolation

Semantic identities are owned only by the Semantic family. Adding another SDK family must not alter:

- `hostToolId` inputs;
- `runtimeId` inputs;
- `releaseSetId` inputs;
- existing Semantic release tags or assets;
- existing Semantic lock/index formats or accepted release-set identity.

Generic Store metadata must never become an implicit input to a family content identity unless that metadata materially changes the package bytes or consumer contract.

## 6. Release index and repository lock

### 6.1 Repository lock

The repository migrates to a v2 release-set lock, for example:

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

The lock pins one immutable release index by release tag, release-set identity, and exact serialized index digest.

### 6.2 Release index

`semantic-sdk-index.json` contains:

- format/schema version;
- `releaseSetId`;
- fields sufficient to reconstruct the canonical release-set identity payload;
- dependency authority versions/digests;
- `hostTools` map keyed by supported host;
- `runtimes` map keyed by supported target runtime;
- asset file name, SHA-256, content identity, compatibility metadata;
- optional provenance/attestation references.

The canonical release-set payload determines `releaseSetId`; exact final serialized index bytes are independently pinned by `indexSha256`.

## 7. Immutable GitHub Release contract

A formal Semantic SDK release is tagged:

```text
semantic-sdk-v2-<releaseSetId[:16]>
```

It contains the complete supported matrix:

- `semantic-sdk-index.json`;
- `SHA256SUMS`;
- three host-tool archives;
- nine target-runtime archives;
- provenance attestations for the index and SDK archives.

Release assets are permanent dependency artifacts. Actions artifacts are temporary intra-workflow transport only.

### 7.1 Immutability

Published tags/assets must never be overwritten, deleted and recreated under the same identity, or silently replaced. Existing-tag publication succeeds only after proving byte identity.

### 7.2 Complete release set

Every formal Semantic SDK v2 release contains a complete matrix even if only one target changed. Unchanged assets may be reused from a prior trusted release by staging the exact verified bytes and attaching those byte-identical archives to the new release.

```text
release is complete
build is incremental
```

## 8. Producer and qualification architecture

### 8.1 PR qualification

Producer-authority changes on a pull request run classification and qualification but do not publish a GitHub Release. Only affected host/runtime identities rebuild.

### 8.2 Main release workflow

After producer-authority changes merge to `main`, the release workflow:

1. validates it is running from `main`;
2. computes planned host/runtime identities;
3. reuses trusted byte-identical immutable assets whose identity is unchanged;
4. builds/packages/verifies changed cells;
5. aggregates the complete index and checksums;
6. verifies determinism and complete matrix coverage;
7. publishes the immutable GitHub Release;
8. generates the repository lock update;
9. opens a lock-only consumer PR instead of silently advancing consumer authority.

```text
Producer authority -> immutable published Release
Consumer authority -> repository lock explicitly accepts one Release Set
```

### 8.3 Trigger ownership

Expected invalidation rules:

- Protobuf version/source change -> all host tools and all runtimes;
- Abseil version/source change -> all runtimes, host tools unchanged;
- Windows toolchain/CRT/ABI change -> Windows runtime only;
- Android NDK/API change -> Android runtimes only;
- Emscripten/toolchain change -> Web runtime only;
- Apple deployment/Xcode contract change -> affected Apple runtimes;
- host-tool packaging contract change -> affected host-tool assets;
- runtime packaging/build contract change -> affected runtimes;
- ordinary Axiom semantic/runtime source changes -> zero producer builds.

Static CI contract tests enforce these boundaries.

## 9. Platform build and qualification expectations

### 9.1 Host tools

Each host-tool package proves exact upstream digest/version, deterministic packaging, manifest/file integrity, executable validity, and matching `protoc --version`.

### 9.2 Linux runtime

Pinned Ubuntu/toolchain; source-free configure, compile, link, and runtime smoke where applicable.

### 9.3 Windows runtime

MSVC-compatible ABI, static CRT, C++20, aligned with the existing Windows Skia linkage direction. Qualification includes source-free configure, compile, link, and native smoke.

### 9.4 macOS runtimes

Separate arm64/x64 identities; source-free package/configuration/static-link qualification and native smoke on matching runners where available.

### 9.5 iOS runtimes

`ios-arm64` and `ios-simulator-arm64` remain distinct. Qualification includes source-free configure/compile/link; simulator smoke where practical.

### 9.6 Android runtimes

arm64-v8a and x86_64 built against pinned NDK/API. Qualification includes source-free configure/compile/link; emulator smoke may supplement but is not required for artifact validity.

### 9.7 Web runtime

Pinned Emscripten wasm32 ABI; source-free configure/compile/link plus executable smoke under Node or browser automation where appropriate.

## 10. Generic Axiom SDK Infrastructure

The Axiom SDK Store and resolver are **generic infrastructure**, not Semantic-owned implementation details.

### 10.1 Ownership

The generic layer owns only mechanisms common to SDK families:

- Store root discovery and override;
- immutable archive cache/materialization;
- SHA-256 and manifest verification primitives;
- atomic install;
- source priority (Store/mirror/Release);
- offline behavior;
- host/target capability discovery;
- generic status/diagnostic reporting;
- family namespace isolation;
- optional garbage collection of unreferenced materializations.

It does not know that Protobuf, Skia, PDFium, FFmpeg, or any other dependency has a particular semantic meaning.

### 10.2 SDK family providers

Each family owns its own authority and build adapter, for example:

```text
Semantic Provider
  owns semantic-sdk.lock.json
  owns semantic-sdk-index.json interpretation
  exports AXIOM_PROTOC
  exports AXIOM_SEMANTIC_RUNTIME_ROOT

Skia Provider (future Store migration)
  keeps existing Skia locks/releases/identity semantics
  exports AXIOM_SKIA_SDK_ROOT

Future PDF Provider
  owns its own lock/index/release family
  exports AXIOM_PDF_SDK_ROOT
```

Providers may use the generic resolver and Store but may not mutate another provider's lock, identity, or materialized package contract.

### 10.3 Additive-extension rule

**Adding a new SDK family MUST be additive.** It must not require changing existing SDK identities, immutable release assets, lock formats, Store materializations, or stable consumer contracts.

A new family may add:

- its own provider;
- its own lock and index schemas;
- its own producer/qualification workflow;
- its own build adapter and exported roots;
- its own namespaced Store materializations.

It must reuse generic Store/fetch/verify/materialization infrastructure instead of adding another parallel dependency manager.

### 10.4 Store namespace and content layout

Conceptually:

```text
<Axiom SDK Store>/
  archives/
    sha256/<digest>/...

  packages/
    semantic/
      host-tools/<hostToolId>/...
      runtimes/<runtimeId>/...
    skia/
      runtimes/<skiaRuntimeId>/...
    toolchains/
      <family>/<toolchainId>/...
    pdf/
      ...
    media/
      ...

  release-sets/
    semantic/<releaseSetId>/...
    skia/<releaseIdentity>/...
    ...
```

Exact spelling may evolve, but family namespace separation and immutable identity-based materialization are mandatory.

### 10.5 Family admission criteria

Not every dependency becomes an SDK family. A dependency is a strong candidate when it is heavyweight, cross-platform ABI-sensitive, slow/expensive to build, reusable across worktrees/projects, and benefits from source-free immutable consumption.

Small, fast, source-friendly dependencies should remain under ordinary source/package-manager ownership.

Transitive implementation dependencies should remain encapsulated by their owning SDK where possible. For example, libraries consumed only as part of the Skia build must not automatically become separate Axiom SDK families.

## 11. Shared Axiom SDK Store

Semantic v2 removes repo-local `.deps/protobuf` from the formal semantic dependency contract.

### 11.1 Purpose

The Store is a persistent local materialization of immutable release assets. It is not authority and never bypasses correctness verification.

Semantic authority remains:

```text
semantic repository lock
-> immutable semantic release index
-> asset SHA / hostToolId / runtimeId
```

### 11.2 Default locations

- Windows: `%LOCALAPPDATA%\Axiom\sdk`
- macOS: `~/Library/Application Support/Axiom/sdk`
- Linux: `$XDG_DATA_HOME/axiom/sdk`, falling back to `~/.local/share/axiom/sdk`

Override:

```text
AXIOM_SDK_STORE=<path>
```

### 11.3 Materialization behavior

For requested Semantic host/target pairing the generic resolver plus Semantic provider:

1. reads the Semantic repository lock;
2. fetches/verifies the locked release index if needed;
3. selects host tool and target runtime;
4. checks the Store for exact identities;
5. validates local manifests/identity;
6. downloads only missing assets;
7. verifies SHA-256, manifest, file set, modes, and consumer contract;
8. atomically installs them;
9. exports stable build-environment paths.

A second worktree with the same identities performs no clone, source build, or network download.

## 12. Source resolution, offline mode, and mirrors

Source priority:

```text
1. verified Axiom SDK Store materialization
2. configured Axiom SDK mirror
3. locked GitHub Release
```

Mirror is transport only and never replaces lock/index/asset verification.

Supported configuration:

```text
AXIOM_SDK_STORE
AXIOM_SDK_MIRROR
```

`--offline` accepts only already-valid local Store materializations. Missing assets fail closed and never trigger source bootstrap fallback.

## 13. Build Environment Resolver contract

`tools/setup_build_environment.py` evolves from Linux semantic setup into the shared Axiom Build Environment Resolver.

The command surface may expose family selectors such as `--semantic` and later `--skia`, but the resolver core must dispatch through provider contracts rather than hard-code family-specific fetch/materialization logic throughout the core.

### 13.1 Native usage

```text
python tools/setup_build_environment.py --semantic
```

Auto-detect host and native target.

### 13.2 Cross-target usage

```text
python tools/setup_build_environment.py --semantic --target ios-arm64
python tools/setup_build_environment.py --semantic --target android-arm64-v8a
python tools/setup_build_environment.py --semantic --target web-wasm32
```

`--target` always means final target runtime; host detection is independent.

### 13.3 Stable Semantic exports

```text
AXIOM_SEMANTIC_HOST_ROOT
AXIOM_PROTOC
AXIOM_SEMANTIC_RUNTIME_ROOT
```

CMake adapter variables such as `CMAKE_PREFIX_PATH` may be derived, but are not Semantic identity or long-term Axiom API.

### 13.4 Diagnostics

`--status` should report detected host, target, family release identity, selected asset identities, ABI/path/source, network use, and Store/mirror/Release origin. Output must remain capable of reporting multiple SDK families without changing the already-stable fields of existing providers.

## 14. CMake host/target separation

CMake must stop treating `protobuf::protoc` as part of the target runtime package.

Target Protobuf is located through the selected runtime root and must provide `protobuf::libprotobuf`. Code generation uses separately resolved `AXIOM_PROTOC`.

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

Generated-source commands execute `${AXIOM_PROTOC}`, not `$<TARGET_FILE:protobuf::protoc>`.

## 15. Relationship to `.deps`

```text
.deps            = repo-local lightweight/source dependencies still owned by current bootstrap paths
Axiom SDK Store  = heavyweight immutable binary SDK/toolchain products
```

Semantic exits `.deps/protobuf` in v2. `googletest`, `nlohmann-json`, `xxhash`, and similar lightweight dependencies may remain under `.deps` until separately justified.

## 16. Relationship to Skia and future SDK families

Skia is not migrated in this Semantic SDK v2 implementation.

Skia already has mature immutable release/target-matrix authority. A later consumer-only migration may materialize those existing Skia assets through the generic Store while preserving existing locks, release identities, producer behavior, and verification semantics.

Likely future Store candidates include heavyweight toolchain packages and, if product scope requires them, PDF or media runtimes. Their admission is a separate design/authority decision; this spec does not pre-authorize a specific PDF/media dependency.

The following categories should generally stay outside standalone SDK families unless future evidence changes the boundary:

- lightweight source dependencies;
- npm/package-manager dependencies;
- OS/system SDKs such as DirectX/Metal/Xcode/Windows SDK (resolver may verify capability/version, not republish them);
- implementation-only transitive libraries already encapsulated inside Skia or another owning SDK.

## 17. Migration and compatibility

### 17.1 GT-G1-07

GT-G1-07 historical evidence remains unchanged. Existing Semantic Toolchain v1 Release remains immutable and available for evidence reproduction.

### 17.2 GT-G1-08+

Before GT-G1-08 begins, `main` should have:

1. generic SDK Infrastructure sufficient for Semantic provider use;
2. Semantic SDK v2 producer/index/store/resolver integration;
3. the first complete v2 release published;
4. a v2 repository lock selecting that release set;
5. Linux/Windows/macOS native consumer qualification;
6. source-free iOS/Android/Web target qualification;
7. ordinary semantic CI using v2 instead of source-building Protobuf/Abseil.

### 17.3 Historical assets

Existing Semantic v1 and Skia release assets are never overwritten or deleted. v2 is a new Semantic publication family.

## 18. Failure handling

Semantic v2 is fail-closed. Resolution fails on malformed/unsupported locks, unresolved locked release/index, digest or identity mismatch, unsupported host/target, invalid Store materialization, ABI mismatch, missing consumer files, unusable/incompatible host `protoc`, missing offline materialization, or unauthorized source fallback.

A missing Release asset never causes automatic Protobuf/Abseil source compilation in ordinary consumer mode.

Generic resolver/provider errors must identify the failing family so one broken future family cannot masquerade as a Semantic or Skia integrity failure.

## 19. Testing and acceptance requirements

Implementation is accepted only when all of the following are proven:

1. A complete Semantic SDK v2 release exists with three host tools, nine target runtimes, index, checksums, and provenance metadata.
2. Release publication is immutable and idempotent for byte-identical existing releases.
3. The repository v2 lock pins exactly one Semantic release index by tag, `releaseSetId`, and SHA-256.
4. Host/runtime assets are independently content-addressed and unchanged identities can be reused.
5. Windows, macOS, and Linux native semantic builds consume v2 assets source-free.
6. iOS, Android, and Web builds consume a host `protoc` plus the correct target runtime source-free.
7. Semantic CMake uses separately resolved `AXIOM_PROTOC`.
8. The resolver uses the shared SDK Store without repo-local Semantic symlinks.
9. A second worktree with a populated Store performs no Semantic source clone/build/download.
10. Missing assets download only required immutable host/runtime packages.
11. `--offline` succeeds with valid Store state and fails closed otherwise.
12. Mirror transport remains checked against authority.
13. CI and local development share resolver/lock/index/verification logic.
14. Producer invalidation tests prove ordinary Axiom source changes do not rebuild dependency SDKs.
15. GT-G1-07 historical evidence/assets are unchanged.
16. Existing Skia locks/releases are unchanged.
17. Generic Store/resolver tests prove SDK family namespaces cannot overwrite or reinterpret one another.
18. Adding a synthetic second provider in tests is additive: it does not change Semantic identities, Semantic lock parsing, Semantic Store paths, or Semantic consumer exports.
19. Generic resolver core has no dependency-specific branch that is required merely to recognize future package content; family-specific semantics are isolated behind provider contracts.
20. Existing family materializations remain valid after another family is added.

## 20. Expected developer workflow

Native:

```text
git clone ...
cd axiom
python tools/setup_build_environment.py --semantic
cmake ...
cmake --build ...
```

Cross-target:

```text
python tools/setup_build_environment.py --semantic --target android-arm64-v8a
```

Future additive usage may look like:

```text
python tools/setup_build_environment.py --semantic --skia --target windows-x64
```

That future syntax does not imply Skia migration is part of this implementation.

If selected identities are already in the shared Store, setup is local verification/resolution. Developers no longer need `.deps/protobuf` symlinks, manual shared-folder layout knowledge, or repeat Protobuf/Abseil builds when changing worktrees/platforms.

## 21. Non-goals

This work does not:

- modify Axiom semantic operation/runtime behavior;
- redesign schema semantics;
- rework GT-G1-07 evidence;
- delete or overwrite historical Semantic assets;
- migrate Skia into the shared Store in the same change;
- alter existing Skia locks, release identities, or producer behavior;
- migrate every lightweight `.deps` dependency;
- create a general-purpose replacement for vcpkg/Conan;
- require every dependency to become an Axiom SDK family;
- publish OS/vendor SDKs that should remain system-installed;
- choose a PDF/media/diagnostics dependency in advance;
- allow cache/mirror/store contents to bypass lock/index/manifest verification.

## 22. Acceptance boundary before GT-G1-08

This design is operationally complete before GT-G1-08 only when Axiom has:

- a generic SDK Store/resolver layer whose ownership is outside the Semantic provider;
- a published, immutable, complete Semantic SDK v2 Release Set;
- `main` locked to it through v2 index/lock authority;
- Linux, Windows, and macOS native consumers plus iOS/Android/Web cross-target consumers proven source-free;
- local development using the persistent shared Axiom SDK Store rather than `.deps/protobuf` symlinks or repeated third-party source builds;
- contract tests proving a future SDK family can be added additively without changing existing Semantic identities/contracts or existing Skia authority.

The long-term architectural rule established by this work is:

> **SDK families own their dependency authority; Axiom SDK Infrastructure owns only generic resolution/materialization mechanisms. New SDK families are additive and must not rewrite the authority of existing families.**
