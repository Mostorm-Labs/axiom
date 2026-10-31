# Axiom repository instructions for AI agents

These instructions apply to the entire repository. Read them before planning or
changing implementation, public APIs, builds, dependencies, CI, or workflows.

## Architecture and public API boundaries

### Understand ownership before coding

Before implementation, inspect the affected modules and identify the existing state
owner, command/commit path, dependency direction, and public entry point. State the
intended scope and API impact in the implementation plan or progress update. Extend
the existing owner instead of adding a shortcut around it.

Use `docs/notion/manifest.yaml` to locate current authority. For SDK work, also read
the relevant contracts in `docs/planning/sdk/js-v1-contract.md`,
`docs/planning/sdk/windows-consumer-v1.md`, and
`docs/planning/sdk/shell-axis-preview-contract.md`. Distinguish current authority,
draft/proposed documents, historical designs, implementation reality, and evidence.
An old diagram or an existing internal method does not by itself authorize a new
public API. This file guides implementation; it does not redefine semantic schemas
or freeze a new ABI.

### Module responsibilities

| Area | Responsibility and boundary |
| --- | --- |
| `sdk/web`, `sdk/windows` | Consumer entry points, transport types, validation, and platform integration. Delegate editing behavior to shared Runtime owners. |
| `sdk/common` | Private Session decoding and adaptation. Do not create another document, selection, or history owner here. |
| `runtime/composition`, `runtime/control` | Runtime assembly, ownership, command admission, execution coordination, and receipts. Keep concrete composition internals private. |
| `runtime/input`, `runtime/interaction`, `runtime/ink`, `runtime/text` | Shared input, editing sessions, tool behavior, and text behavior. Platform adapters feed these existing paths. |
| `runtime/semantic` | Canonical document state, Operations, validation, atomic apply, and codecs. Canonical mutations follow the existing OperationEngine path. |
| `runtime/scene`, `runtime/render` | Derived scene, spatial data, rendering, and presentation. These are not alternate canonical document owners. |
| `runtime/foundation`, `schema` | Shared contracts/value types and versioned formats. A header being under `include/` does not make it a consumer SDK contract. |
| `apps`, `debug_ui`, `verification` | Hosts, demos, adapters, and verification. Some platform adapters are reused by the SDK; inspect actual dependencies before moving or deleting code. |

The usual consumer path is Shell -> SDK facade/transport -> private Session/adapters
-> shared Runtime owners -> OperationEngine -> canonical document, with derived
scene/render state for presentation. High-frequency input and IME use their existing
adapter paths; do not force them through a new parallel command implementation.
Keep product navigation/account/UI in the Shell and storage/network/synchronization
in the external Data/Axis owner. Runtime commit, local durability, presentation, and
cloud acknowledgement are distinct states.

### Keep consumer contracts separate from implementation

- Web consumers use the root exports in `sdk/web/axiom.mjs` and the types in
  `sdk/web/axiom.d.mts`. Keep the WASM module, HEAP access, bridge exports, and native
  wrappers behind the facade; do not return them as public business APIs or require
  private deep imports.
- Windows consumers use `sdk/windows/include/axiom/api.h`: a C/POD ABI with opaque
  handles and explicit buffer ownership. Do not pass internal C++ classes, STL
  objects, exceptions, or cross-CRT allocation ownership across the DLL boundary.
  `axiom.hpp` is a caller-side wrapper; its application-owned C++ values do not make
  internal C++ Runtime interfaces part of the DLL ABI.
- The source developer profile, including `RuntimeComposition` and its abstract
  facade, is separate from the ordinary JS/DLL consumer contract. Keep its Pimpl
  boundary and keep `AXIOM_INSTALL_DEVELOPER_COMPONENTS` opt-in. Do not copy its
  broader semantic C++ types into the consumer SDK or enable qualification seams
  for normal consumers.
- Public value DTOs, ObjectId/DocumentId, opaque tokens, read-only state snapshots,
  serialized Operation/Snapshot/resource bytes, and documented host surface handles
  are valid contracts. Specify ownership and lifetime for every buffer or handle.
- Do not expose `RuntimeCompositionCore`, `CanvasEditingRuntime`, mutable object
  stores, selection/history owners, scene/compiler internals, or Skia/GPU/tile
  objects through public fields, pointers, references, accessors, or debug commands.
  Do not duplicate canonical state, editing rules, history, or identity allocation
  in an SDK, demo, UI, or platform wrapper.

### Implement and verify the actual boundary

For each new or changed public capability:

1. Reuse the owning Runtime path and expose the smallest capability needed by the
   consumer. If the public contract has a gap, describe and address it explicitly;
   do not reach into private state to work around it.
2. Define request/result DTOs, validation, ID/token widths, version compatibility,
   buffer copy/borrow/release rules, lifetime, thread/reentrancy behavior, and
   error/receipt semantics. Preserve full identity/generation stamps and lossless
   integer transport. Do not equate admission with commit or persistence.
3. Update the matching public declarations, bindings, contract documentation, and
   relevant external-consumer verification. Cover invalid, stale, or foreign
   handles/tokens when the changed capability uses them.
4. Inspect JS package exports, DLL export symbols, CMake install rules, and release
   packaging as well as source headers. Normal consumer packages must not install
   private Runtime headers or standalone internal/dependency archives, or expose
   test/qualification interfaces.
5. Run checks appropriate to the affected path. The runtime boundary checker is
   useful but does not cover every current SDK entry point; use real exported SDK
   calls and package/consumer checks when those boundaries change. Web verification
   must use matching JS declarations/wrapper, WASM, and resources from the build.

For the current local development workflow, use the Web/WASM SDK demo as the primary
interactive verification path. Shared Runtime behavior belongs in the shared owner;
a Web pass alone does not establish another platform's ABI or packaging correctness.

## Dependency policy: consume published SDKs first

Axiom has accepted, locked prebuilt dependencies. Ordinary development and CI must
consume those published packages instead of rebuilding equivalent dependencies from
source.

### Semantic SDK v2

`semantic-sdk.lock.json` on `main` is the accepted Semantic SDK v2 consumer authority.
For normal development, tests, and new workflows, resolve it through the repository
entry point:

```bash
python tools/setup_build_environment.py --semantic --status
```

When the lightweight core dependencies are also needed, use:

```bash
python tools/setup_build_environment.py \
  --core \
  --semantic \
  --facts-output out/build-environment.json \
  --status
```

For cross builds, select the final runtime ABI explicitly, for example:

```bash
python tools/setup_build_environment.py \
  --semantic \
  --target android-arm64-v8a \
  --facts-output out/build-environment.json \
  --status
```

The resolver selects a native host `protoc` plus the locked target runtime and
materializes them in the persistent SDK Store. Reuse the emitted environment facts,
especially `AXIOM_PROTOC`, `AXIOM_SEMANTIC_RUNTIME_ROOT`, and `CMAKE_PREFIX_PATH`.
For CMake consumers, prefer `tools.semantic.cmake_consumer.consumer_cmake_arguments`
instead of inventing package-search logic.

Authoritative usage details are in `docs/semantic-sdk-store.md`.

### Forbidden by default

Unless the task explicitly owns Semantic SDK production/release or historical
reproduction, do **not**:

- run `tools/bootstrap_deps.py --semantic-codec`;
- create or populate `.deps/protobuf`;
- rebuild Protobuf, Abseil, utf8_range, or `protoc` just to satisfy a consumer build;
- install Homebrew/Chocolatey/system Protobuf as an ad-hoc replacement for the locked
  SDK;
- vendor another copy of those packages into a workflow or feature branch;
- change `semantic-sdk.lock.json` as part of unrelated feature work.

If the locked SDK cannot be resolved, treat that as a resolver/Store/authority problem
and investigate it. Do not silently fall back to a source rebuild.

### CI and new workflow rule

New workflows that need Semantic must use the same locked consumer path as local
development. The normal pattern is:

1. check out the repository;
2. install the platform toolchain required by the target (for example NDK or
   Emscripten when cross-compiling);
3. run `tools/setup_build_environment.py --core --semantic --target <target>` and
   capture its facts or GitHub environment output;
4. configure CMake with the emitted target runtime and host `protoc`;
5. build/test the project without rebuilding the Semantic dependency stack.

SDK resolution does not replace the OS/compiler toolchain. Installing a target
compiler is allowed; rebuilding the already-published Semantic SDK is not.

When validating cache/Store behavior, use a second `--offline` resolution to prove the
locked materialized packages are sufficient.

### Skia

Ordinary development and CI must use the accepted prebuilt Skia SDK supply chain and
`r1-full-skia-sdk.lock.json`. Do not run a fresh Skia GN/Ninja build unless the task
explicitly owns Skia SDK production/release. See
`docs/architecture/SKIA_SDK_SUPPLY_CHAIN.md`.

## Local caches

`<worktree>/.deps` remains the interface for lightweight/core source dependencies and
may be shared across worktrees with `tools/link_shared_deps.py`. Semantic SDK v2 uses
its separate persistent SDK Store; `.deps/protobuf` is a legacy path and must not be
reintroduced.

Before adding any dependency-install step, first check the repository lock files,
`docs/semantic-sdk-store.md`, and the existing setup scripts. Prefer reuse of accepted
artifacts over downloading or compiling a new copy.

## SDK versioning and publication

Follow `sdk/VERSIONING.md` and the user's `x.y.z` iteration rule: `z` for small
improvements/fixes, `y` for small feature iterations (reset `z`), and `x` for large
feature iterations (reset `y` and `z`). A release containing multiple levels uses
the highest one. Use numeric versions without prerelease suffixes. Keep the version
in `sdk/version.json`, Web package/wrapper/types, all platform SDKs and Demo
manifests identical. Publish immutable `vx.y.z` GitHub tags and matching
`@mostorm/axiom@x.y.z` npm packages with the `latest` distribution tag. The initial
release requested in this session is `0.0.1`. Version labels do not replace the
separate API/ABI/data compatibility contracts or platform verification.

A tag becomes immutable once its SDK is published as a GitHub Release or npm
version. To repair an unpublished candidate created by the current release task,
first cancel its run, preserve its exact revision/evidence, and confirm neither
publication exists. Updating that owned candidate requires an exact old-ref lease
and a complete new release verification run. Never rewrite published tags/assets.
