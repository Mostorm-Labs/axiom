# Axiom

Axiom is a cross-platform **Visual Document Runtime** built with C++20 and
Skia Ganesh. It owns the semantic Document, Operation-only editing path,
EditorSession, RichText, InkEngine, SceneCompiler, RuntimeScene and canonical
rendering. The Product Layer owns navigation, Page collections and product
workflow. The external Shared Data Runtime only provides the approved
repository/storage/sync custody for those product concepts; neither layer
duplicates Canvas semantics or the native input/render hot path.

The current product targets are Web, Windows, Android, iOS and iPadOS. Web uses
React/TypeScript + WASM/WebGL; Windows uses React Native for Windows (RNW) with
a Native Canvas/Overlay Host; Android, iOS and iPadOS use React Native Shells
with native Canvas data paths. macOS native productization is deferred and is
accessed through Web, while the shared Runtime keeps a core/Metal conformance
harness. ChromiumOS reuses Web and Headless remains a reference/test target.

The sole promotion order is `AR-0 → G0 → G1 → G2 → G3 → G4 → G5 → G6 → G7 →
G8 → G9 → R5-B`. Existing POC/RF/R1～R5 labels remain evidence and delivery
work packages mapped onto that order; they are not a competing route. POC-03's
historical Windows Integrated D3D12 performance failure and POC-02/POC-06
physical latency gates remain visible until their own evidence closes.

## Fixed architecture baseline

- Product targets: Web, Windows RNW, Android RN, iOS/iPadOS RN receive complete
  product, device, performance, release, and support gates.
- Web reference/product shell: React/TypeScript + WASM + WebGL.
- Windows product shell: React Native for Windows + Native Canvas/Overlay Host;
  local screen annotation uses the native overlay path.
- Android reference/product shell: React Native + native `CanvasView` + JNI. Pen input and canvas
  rendering never pass through React Native JS.
- iOS/iPadOS product shell: React Native + native Canvas/ObjC++/Metal data path;
  Pencil, IME and rendering do not pass through RN JS.
- macOS: deferred native Shell; Web is the product entry point and a core/Metal
  conformance harness remains available.
- ChromiumOS reuses the Web target. Headless is a V1 test/reference utility,
  not yet a supported public server or batch-rendering product API.
- Runtime: C++20 modules for RuntimeFacade, InputRouter, Document, Operations,
  EditorSession, RichText, InkEngine, Geometry, Layout, HitTest, SceneCompiler,
  shared RuntimeScene, per-view FrameState, FrameBuilder, FrameGraph,
  Compositor, RendererBackend, FrameInvalidationSink, TileCache,
  ResourceBudgetCoordinator and Resources. Persistence, Offline/Sync and
  Collaboration are external data ports coordinated by Shared Data Runtime.
- Renderer: Skia Ganesh for v1; Graphite/WebGPU is a future backend.
- Surfaces: platform adapters own native window/surface/context lifecycles and
  provide generation-bound RenderTargets; RendererBackend does not own them.
- Ink: canonical document rendering and low-latency preview are separate paths
  connected through one shared Preview Model and `FastInkBridge`.
- Determinism: canonical numeric storage/encoding, deterministic clock/random,
  semantic ChangeSets, and cross-platform replay are explicit contracts.
- Recovery: an immutable DocumentSnapshot plus committed Operation continuation
  deterministically restores a target frontier; snapshots never bypass the
  normal Operation path for editing or undo/redo.
- Scheduling: the Runtime emits revision-bound frame invalidations; platform
  schedulers own VSync/present, while bounded input queues preserve confirmed
  samples independently of render cadence.

## Documents

- [Axiom architecture review workspace (Draft)](docs/architecture/review/README.md)
- [Project framework](docs/PROJECT_FRAMEWORK.md)
- [System architecture](docs/architecture/SYSTEM_ARCHITECTURE.md)
- [Notion v0.3 / repository gap audit](docs/architecture/review/NOTION_V03_REPOSITORY_GAP_AUDIT.md)
- [G0～G9 implementation and verification route](docs/planning/AXIOM_GATES_AND_STAGES.md)
- [AR-0 reconciliation report](docs/planning/AR0_RECONCILIATION_REPORT.md)
- [Gate task tracker](docs/planning/GATE_TASK_TRACKER.md)
- [R1～R5 milestone status](docs/planning/R_MILESTONE_STATUS.md)
- [RF-01 Scene rendering foundation](docs/architecture/RF01_SCENE_RENDERING_FOUNDATION.md)
- [Runtime Public C API contract](docs/api/RUNTIME_C_API_CONTRACT.md)
- [Canvas C++ / C ABI style](docs/CPP_STYLE.md)
- [Staged delivery plan](docs/planning/STAGED_DELIVERY_PLAN.md)
- [Verification strategy](docs/quality/VERIFICATION_STRATEGY.md)
- [Vibe architecture findings](docs/research/VIBE_ARCHITECTURE_FINDINGS.md)
- [Architecture decisions](docs/adr/README.md)
- [POC-01 implementation](pocs/shared_engine/README.md)
- [POC-02 Ink Engine implementation](pocs/ink_engine/README.md)
- [POC-03 100K Scene implementation](pocs/large_scene/README.md)
- [Semantic SDK Store usage](docs/semantic-sdk-store.md)
- [Prebuilt Skia SDK supply chain](docs/architecture/SKIA_SDK_SUPPLY_CHAIN.md)

The immutable POC profiles remain reproducible. R1 productization adds the
`r1-full-v1` Skia profile with eight targets and explicit `release`, `debug`,
and `asan` variants; it is produced and published separately, so ordinary
Canvas CI downloads SDK assets and never runs Skia GN/Ninja.

## Dependency consumption policy

Axiom now has an accepted Semantic SDK v2 lock on `main`. Ordinary development,
tests, and CI must **consume the locked published SDK** instead of rebuilding
Protobuf/Abseil/utf8_range/protoc from source.

The normal entry point is:

```bash
python3 tools/setup_build_environment.py --semantic --status
```

When the lightweight core dependencies are also needed, use:

```bash
python3 tools/setup_build_environment.py \
  --core \
  --semantic \
  --facts-output out/build-environment.json \
  --status
```

For cross builds, select the final runtime ABI explicitly, for example:

```bash
python3 tools/setup_build_environment.py \
  --semantic \
  --target android-arm64-v8a \
  --facts-output out/build-environment.json \
  --status
```

The resolver reads `semantic-sdk.lock.json`, selects a native host `protoc` plus
the locked target runtime, verifies the release identities/digests, and places
them in the persistent SDK Store. Later worktrees and builds reuse the same
materialized packages instead of recompiling them.

Do **not** use `tools/bootstrap_deps.py --semantic-codec`, create
`.deps/protobuf`, or install an ad-hoc system/Homebrew/Chocolatey Protobuf for
ordinary development or CI. Those paths are only for an explicitly scoped SDK
production/release task or historical reproduction. A failure to resolve the
locked SDK should be treated as a resolver/Store/authority problem, not as a
reason to silently fall back to a source rebuild.

New GitHub Actions workflows must follow the same rule: resolve Semantic through
`tools/setup_build_environment.py`, consume the emitted `AXIOM_PROTOC` and
`AXIOM_SEMANTIC_RUNTIME_ROOT`/`CMAKE_PREFIX_PATH`, and keep the target compiler
or OS SDK setup separate from Semantic dependency production. See
[`AGENTS.md`](AGENTS.md) for the repository-wide AI/automation guardrail and
[`docs/semantic-sdk-store.md`](docs/semantic-sdk-store.md) for exact Store,
offline, target, and CMake usage.

## Shared local dependency cache

The repository keeps its lightweight/core dependency interface at
`<worktree>/.deps`, while those source checkouts can be shared by multiple
worktrees. Semantic SDK v2 is different: it uses the persistent SDK Store and
must not recreate the legacy `.deps/protobuf` tree.

On macOS, if you want to share the lightweight/core `.deps` cache, connect the
current worktree and then resolve the normal build environment:

```bash
shared_deps=/Users/qing/Desktop/sources/git/deps/axiom/darwin-arm64
python3 tools/link_shared_deps.py --shared-root "$shared_deps"
python3 tools/setup_build_environment.py \
  --core \
  --semantic \
  --facts-output out/build-environment.json \
  --status
python3 tools/link_shared_deps.py --shared-root "$shared_deps" --check
```

For another worktree, run the same `link_shared_deps.py` command using the same
cache path. The script fails closed if `.deps` is a real directory or points at
a different cache; move an existing disposable `.deps` directory aside
explicitly before connecting it. The equivalent configuration can be supplied
through `AXIOM_SHARED_DEPS`.

The lightweight/core cache remains keyed by `deps.lock.json`. The Semantic SDK
Store is independently governed by `semantic-sdk.lock.json`, including its
release tag, release-set identity, index digest, and package manifests. Compiled
or materialized dependencies are platform/architecture-specific, so do not copy
a runtime package across incompatible targets. Keep build directories separate
per worktree/configuration; reuse only the dependency/cache layers intended for
sharing.

## Current sequence

`AR-0` reconciles the Notion v0.3 direction with repository evidence. G0～G3
establish the semantic kernel, RuntimeScene and basic canonical canvas; G4/G5
close interaction, Ink/Arc and large-canvas performance; G6 adds RichText,
complex objects, controlled ExternalSurface and platform lifecycle; G7/G8 add
local durability, offline/sync and recovery; G9 produces the integrated product
gate, followed by R5-B release hardening.

Arc/FastInk is a product requirement, but its presentation backend is isolated
from canonical mutation. Any Arc failure must automatically fall back to
Canonical-only rendering without dropping confirmed input, changing the digest
or blocking save/recovery. POC-05's controlled Overlay evidence is a G6 input;
its private scene bridge is not the product ABI. See the [consolidated
report](docs/evidence/poc05/consolidated-validation-20260820.md) for the scoped
historical evidence.
