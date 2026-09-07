# Axiom Build Environment Authority + CI Dependency Architecture v0.1

Date: 2026-09-07
Status: Proposed / design-approved in chat, implementation not yet started
Scope: CI dependency acquisition, build-environment identity, producer/consumer boundaries, and workflow convergence

## 1. Purpose

Axiom CI currently spends significant hosted-runner time rebuilding immutable third-party dependencies, most notably the semantic Protobuf/Abseil toolchain, before it can compile and test Axiom itself. The repository already contains the main building blocks for a producer/consumer model, but ordinary G1 exact-source workflows still perform source bootstrap directly.

This design changes the responsibility of bootstrap:

- source bootstrap remains a producer/developer recovery mechanism;
- ordinary consumer CI restores or fetches immutable dependency artifacts;
- immutable locks and artifact digests become the dependency authority;
- workflow-local knowledge of individual package directories is reduced;
- Skia stays on its existing immutable SDK/release path and is not rebuilt or republished by this work.

The goal is faster, less drift-prone, and more reproducible CI without changing Axiom runtime semantics.

## 2. Non-goals

This work does not:

- modify Axiom semantic/runtime/render behavior;
- change Skia source, Skia profiles, Skia producer workflows, Skia release assets, or Skia SDK identities;
- rebuild or republish historical Skia assets;
- introduce a new package manager for C++ dependencies;
- require a fully hermetic cross-platform container system in v0.1;
- make caches authoritative evidence.

## 3. Current repository reality

The repository already contains two distinct dependency supply-chain patterns.

### 3.1 Skia

Skia already follows the desired consumer model:

1. a lock records a historical release tag and per-target immutable asset identity;
2. `tools/skia/fetch.py` derives the Release URL from the lock;
3. the downloaded archive is checked for size, SHA-256, SDK identity, manifest identity, target/toolchain/profile compatibility, and file hashes;
4. the verified SDK is atomically installed below `.deps/skia-sdk`;
5. ordinary consumers do not need Skia source, GN, or a producer build.

The R1 Full lock is `r1-full-skia-sdk.lock.json`, currently identifying `skia-sdk-r1-full-v1-54c1999dc79d094d` and its target/variant assets.

### 3.2 Semantic toolchain

The repository also has a semantic producer/consumer chain:

- `semantic-toolchain.lock.json`
- `tools/semantic_fetch.py`
- `tools/semantic_sdk.py`
- `.github/workflows/semantic-toolchain-producer.yml`

The producer builds the pinned semantic toolchain once, packages a deterministic archive, verifies it, and can publish an immutable prerelease. The lock identifies the hosted Linux artifact with its release tag, SDK ID, and SHA-256.

However, G1 exact-source workflows such as `g1-06-exact-source.yml` still call `tools/bootstrap_deps.py --core --semantic-codec`, rebuilding Protobuf/Abseil on every run and then wiring `Protobuf_DIR`, `absl_DIR`, and `utf8_range_DIR` manually.

This is the primary optimization and drift problem addressed by this design.

## 4. Architectural decision

Axiom will use a producer/consumer dependency architecture.

```text
Dependency source / producer recipe changes
                 |
                 v
          Producer workflow
                 |
        source bootstrap once
                 |
      qualify + deterministic package
                 |
        immutable release asset
                 |
          lock + digest authority
                 |
        -------------------------
                 |
                 v
          Consumer CI workflow
                 |
      fetch/restore locked artifact
                 |
       verify identity + digest
                 |
        expose canonical prefix
                 |
          configure / build
                 |
             test/evidence
```

Ordinary exact-source CI must build Axiom from exact Axiom source, but it does not need to rebuild immutable third-party dependencies from source when a verified locked artifact already exists.

## 5. Authority model

Three inputs are distinct and should be recorded separately.

### 5.1 Source Authority

The exact Axiom repository revision being tested.

Examples:

- `sourceRef`
- `taskAnchor`
- branch/ref identity required by the Gate

### 5.2 Build Environment Authority

The immutable dependency/toolchain identities consumed by the build.

For v0.1 this consists of repository locks plus verified artifact identity, including at least:

- lock path and lock format/version;
- release tag;
- asset name;
- SDK/artifact ID;
- SHA-256;
- target/platform;
- producer manifest/toolchain metadata when the artifact contains it.

### 5.3 Cache

Cache is only a transport/performance optimization.

A cache hit may avoid network download or repeated extraction, but a cache entry is accepted only after the same authority checks required for a downloaded artifact. Cache presence or key identity never replaces lock/digest verification.

## 6. Frozen Skia compatibility contract

This work has a hard compatibility boundary around Skia.

### 6.1 MUST NOT change

The CI modernization MUST NOT:

- modify `r1-full-skia-sdk.lock.json` as part of this optimization;
- modify existing Skia SDK identities;
- rebuild Skia merely because consumer CI runs;
- republish or overwrite an existing historical Skia Release asset;
- replace the existing Skia producer/consumer architecture;
- create a source fallback when a locked Skia artifact is unavailable or invalid.

### 6.2 MUST preserve historical resolution

The new/shared CI setup MUST continue to support the current Skia consumer contract:

```text
lock
  -> repository + historical tag + asset
  -> GitHub Release or configured mirror
  -> size/SHA verification
  -> manifest/SDK identity verification
  -> atomic install
```

A historical asset remains valid because its lock identity matches, not because it was produced by the current source revision.

### 6.3 Fail-closed behavior

If a locked Skia asset is missing, has the wrong size/digest, has incompatible manifest identity, or cannot be resolved from the locked tag, CI must fail closed. It must not rebuild Skia silently.

### 6.4 Compatibility regression check

This migration must include a source-free Skia compatibility check proving that the current lock still resolves to the same historical tag/assets and passes the existing strict verification path. Existing Skia tooling should be reused rather than duplicated.

## 7. Semantic toolchain consumer contract

The semantic toolchain is the first dependency family migrated away from per-run source bootstrap.

### 7.1 Primary source

Ordinary hosted Linux consumer CI should use:

```text
semantic-toolchain.lock.json
        +
tools/semantic_fetch.py
```

The fetcher already:

- validates lock shape;
- derives the historical Release URL or configured mirror URL;
- downloads the locked archive;
- verifies SHA-256;
- verifies the SDK archive contract;
- installs to `.deps/protobuf` by default.

### 7.2 No ordinary source bootstrap

A normal G1 consumer workflow must not run:

```text
python3 tools/bootstrap_deps.py --semantic-codec
```

unless it is explicitly acting as the semantic dependency producer or a documented developer/recovery path.

### 7.3 Core lightweight dependencies

`googletest`, `nlohmann/json`, and `xxHash` are lightweight relative to the semantic toolchain. v0.1 may continue to bootstrap these if needed, but their acquisition should be kept separate from the semantic SDK so that `--core` does not force a semantic source rebuild.

A later optimization may cache or package these by `deps.lock.json` hash. That is not required to realize the main CI improvement.

## 8. Canonical consumer environment

Workflow files should not each own package-specific path wiring.

The consumer setup must expose a small canonical contract. For the semantic SDK, the preferred contract is a single prefix/root plus derived paths, for example:

```text
AXIOM_SEMANTIC_SDK_ROOT=<workspace>/.deps/protobuf
CMAKE_PREFIX_PATH=<workspace>/.deps/protobuf
```

If current CMake package behavior still requires explicit package variables, one shared setup layer may derive and export:

```text
Protobuf_DIR=<root>/lib/cmake/protobuf
absl_DIR=<root>/lib/cmake/absl
utf8_range_DIR=<root>/lib/cmake/utf8_range
```

but individual G1 workflow files should not recalculate these paths independently.

The migration should prefer `CMAKE_PREFIX_PATH` if it is sufficient and tested. Explicit package variables remain acceptable behind the shared setup contract where necessary.

## 9. Shared setup boundary

The repository should have one CI-facing build-environment setup boundary instead of repeated workflow-local dependency logic.

v0.1 may implement this as a repository script or composite action. The stable behavior matters more than the YAML mechanism.

The setup boundary should:

1. identify host target/platform;
2. read the relevant dependency locks;
3. fetch or validate installed immutable artifacts;
4. optionally use cache as a transport optimization;
5. export canonical dependency roots/prefixes;
6. produce machine-readable environment facts for evidence/debugging;
7. fail closed on lock/artifact mismatch.

It must not trigger producer builds.

## 10. Workflow migration

Migration is incremental and evidence-preserving.

### Phase 1 — Semantic exact-source path

Migrate the active G1 exact-source consumer pattern first:

- replace semantic source bootstrap with locked semantic SDK fetch;
- keep exact-source repository identity checks unchanged;
- keep protobuf-on and protobuf-off build/test matrices unchanged;
- preserve existing test selection and evidence generation;
- record semantic SDK identity in CI facts where practical.

`g1-06-exact-source.yml` is the reference example because it currently contains the duplicated bootstrap and `*_DIR` wiring pattern. Newer G1 workflows should use the shared consumer contract rather than copy it.

### Phase 2 — Shared consumer setup

After the first path proves the contract, extract the environment acquisition/wiring into the shared setup boundary and migrate other G1 semantic workflows that duplicate the same pattern.

### Phase 3 — Optional cache

Add cache only after artifact identity verification is stable. Cache keys should incorporate at least platform/architecture and the relevant lock digest. Restored content must still be verified.

### Phase 4 — Compiler cache (optional follow-up)

`ccache`/`sccache` may be enabled as a separate performance optimization. Compiler cache outputs are not dependency authority or Gate evidence by themselves.

## 11. Exact-source semantics

Using a historical immutable dependency asset does not weaken exact-source CI.

Exact-source means the tested Axiom source revision is exact. Third-party dependencies are separate governed build inputs. A stronger evidence statement is:

```text
Exact Axiom Source
+ Exact Dependency Lock/Artifact
+ Exact Test Contract
= reproducible Gate input set
```

Rebuilding Protobuf/Abseil from source on every Axiom commit is neither necessary nor inherently more authoritative if the dependency recipe did not change.

## 12. Producer invalidation rules

A dependency producer should run because dependency authority changed, not because ordinary product source changed.

Semantic producer inputs include the dependency source/lock/packaging recipe and producer workflow itself. Existing producer classification should remain the owner of that decision.

Consumer changes must not cause Skia producer rebuilds. Consumer-only CI changes should remain source-free with respect to Skia.

## 13. Failure handling

Consumer CI fails closed when any of the following occurs:

- lock is malformed or unsupported;
- target does not match lock;
- Release asset cannot be resolved;
- downloaded size/digest does not match;
- SDK manifest/identity/file set verification fails;
- canonical package prefix cannot satisfy required CMake packages;
- a workflow attempts an unauthorized source fallback.

A failure should report which authority input failed: source identity, dependency lock, artifact identity, environment wiring, configure, build, or tests.

## 14. Verification requirements

Implementation is complete only when all of the following are demonstrated:

1. Semantic consumer CI fetches the currently locked semantic artifact instead of building Protobuf/Abseil from source.
2. Protobuf-enabled Axiom configure/build succeeds using the shared/canonical dependency contract.
3. Protobuf-disabled Axiom configure/build remains unaffected.
4. Existing focused and full semantic tests remain unchanged in meaning and pass.
5. Existing exact-source identity checks remain intact.
6. Skia locks and historical assets are unchanged.
7. A source-free Skia compatibility check resolves and strictly verifies at least the representative locked R1 Full consumer path using existing tooling.
8. There is no silent dependency source fallback on artifact failure.
9. CI logs/evidence expose the consumed semantic artifact identity sufficiently to diagnose and reproduce the build.
10. The hosted CI path shows the expensive semantic bootstrap build step has disappeared from ordinary consumer runs.

## 15. Expected performance effect

The dominant improvement is removal of repeated semantic toolchain compilation from ordinary runs. Current observed G1 behavior spends several minutes building Abseil/Protobuf before Axiom compilation. After migration this work becomes download/verification/extraction, with optional cache reducing that further.

No performance claim is made for Skia because Skia is already consumed as a prebuilt locked SDK and should remain unchanged.

## 16. Scope of repository changes

Expected implementation changes are limited to CI/build-environment surfaces such as:

- `.github/workflows/*` consumer workflows;
- a small shared CI/build-environment helper or composite action;
- CI contract/verification tests;
- CI evidence metadata where needed.

Runtime semantic/render source changes are out of scope.

Skia producer/profile/lock/release changes are out of scope.

## 17. Acceptance boundary

This architecture is accepted when ordinary semantic G1 consumer CI no longer compiles the semantic third-party toolchain from source, uses the locked immutable semantic artifact instead, preserves all Axiom test/evidence semantics, and demonstrably preserves the existing source-free locked Skia consumer path without modifying Skia authority.
