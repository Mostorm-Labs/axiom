# Axiom repository instructions for AI agents

These instructions apply to the entire repository. Read them before changing build,
dependency, CI, or workflow code.

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
