# FC-00X Scene / Resource / Runtime Composition — P31 context

## Execution identity

- Repository: `github:Mostorm-Labs/axiom`
- Execution branch: `codex/fc00x-scene-resource-composition-p31-v2`
- Task anchor: `0e9ba85f94d62e6e3939d5d1397b1c681c1b1a2a` (`ancestor`)
- Preparation input: `D:/axreview/fc00x-20261016/scene-resources-composition-handoff-v0.3.txt`
- Preparation SHA-256: `94ed26bb7dafadb2ab2eec46df2e40ef1b7fb52c09ba89ff75f803095d2314fd`
- This package is P30/P31 materialization only. It authorizes no production mutation until an independent Control Review accepts the exact package commit. The exact accepted P12/P13 Authority dependency is bound below; this does not authorize third-package P32.

## Baseline reconciliation

The latest fetched `origin/codex/fc00x-interaction-internalization-p31` is `0e9ba85f94d62e6e3939d5d1397b1c681c1b1a2a`, a descendant of the reviewed `fa489abdd429e189f29f10187a91080d579f8df6`. The descendant delta is limited to runtime control-state cleanup in `runtime/control/src/canvas_control_service.cpp` and its protected tests. No third-package implementation exists and no accepted continuation cursor exists.

The post-246cd1d0 review remains an upstream dependency. F01-F08 (keyed cancellation, source resurrection/watermark, arbitration cleanup, failed-session rollback, creation cancellation repaint, control target fencing, style-leaf preservation, and truthful frame qualification) must be re-audited at P32 against the accepted implementation revision. They are not silently included in this package. If an upstream failure remains, return `BLOCKED_IMPLEMENTATION` or `BLOCKED_AUTHORITY` according to the frozen classification before changing Scene/resource/composition code.

## Current flow and owners

`semantic::SemanticReadView` is the canonical read source. `SceneBinding` and `IncrementalRuntimeCoordinator` own Scene/RuntimeScene prepare and atomic publication. `Scene` owns query, hit, draw-list, spatial and damage state. `RuntimeSceneProjection` is an inspection/reference projection, not a second Document. `FullSceneCompiler` is the full materialization path; the existing `ISemanticSceneCompiler` path remains the incremental/query path. `RichTextLayoutService` owns immutable text snapshots and consumes an explicit `LayoutContext`. `ResourceProvider` supplies immutable bytes; image and text consumers own decode/layout caches. Document ResourceManifest is the intended trusted Image/Font/Brush metadata source under ADR-0013; attach/bind may copy entries into an internal ResourceBindingSnapshot only after that source is resolvable. At the current anchor, SemanticSnapshot/ObjectRecord/protobuf snapshot expose no ResourceManifest representation. The accepted P12/P13 Authority is now bound, but P32 must verify the bound implementation before R2 production mutation. Host/provider supplies bytes only; Brush cross-checks canonical `BrushResourceBinding`. `RuntimeFrameCoordinator` owns frame identity, stale/retry decisions, and preview retirement qualification; platform providers own acquire/present/GPU facts. Existing `CanvasEditingRuntime` owns interaction/session/history and remains the one canonical mutation owner.

The new composition layer is a narrow owner-side assembly above these components. It is a separate `runtime/composition` target with explicit include/src/test scope; it is not a public ABI. It may own per-instance bindings and queues, but it must not duplicate Document, Selection, Session, History, RuntimeScene, or canonical write state. It must not call Playground to perform generic business behavior.

## Frozen R0 decisions

1. **Owner and clock:** one Runtime instance owns one Document/View binding, existing `CanvasEditingRuntime` owns interaction/session/history, `Scene` + `IncrementalRuntimeCoordinator` own Scene publication, `RuntimeFrameCoordinator` owns frame qualification, and the host owns OS thread/lock/GPU affinity. Document/operation IDs and commit clocks use existing allocation contracts. No new canonical clock or public ABI field is introduced.
2. **Scene derivations:** the incremental `ISemanticSceneCompiler -> CompiledSceneSnapshot/CompiledSceneDelta -> Scene` path remains the query/spatial path. `FullSceneCompiler -> scene::RuntimeSceneProjection` remains the full reference/inspection path and is adapted to the existing `RuntimeScene` projection only through the coordinator. Shared geometry, identity, order, dependency and invalidation rules come from existing semantic/scene helpers. No ID truncation or OrderKey truncation is permitted. Publication is atomic through the existing coordinator gate; a failed prepare keeps the previous complete publication.
3. **Resource interaction:** choose **pull + notify** as the single model. The Runtime requests a provider snapshot at an owner safe point and receives an explicit invalidation/notify event when the provider epoch changes. External code supplies bytes/results; Runtime never performs file/network access. A typed internal binding token carries document binding, provider epoch, resource version, expected hash and request lifetime. Resource refresh is not a Document Operation and does not change Semantic generation; it does create a Scene/resource-derived revision and frame demand when a dependent decode/layout publication changes.
4. **Instance lifecycle:** `create/open -> attach/bind -> ready -> lost/recover/rebind -> stopping/quiescent -> detach/close`. Input/control/operation/resource/frame calls are accepted only in the states recorded by the execution contract. Stop first invalidates tokens and cancels owner work, then asks the platform to drain; platform feedback is consumed by owner pump before detach. No callback retains a raw Host/Surface pointer after close.
5. **Concurrency:** Runtime is single-owner/manual-poll. External workers may produce owned immutable bytes or platform feedback, but only the owner pump mutates provider caches, derived scenes, frame state or readback. No synchronous callback re-enters Runtime while a lock is held; notifications are copied into a bounded, coalescing queue that retains the latest state for each binding.
6. **Compatibility:** existing RuntimeFacade, RuntimeAppBinding, SceneBinding, FullSceneCompiler, RuntimeSceneProjection, ImageResourceResolver and RichTextLayoutService remain compatibility consumers. A new composition target may wrap them internally, but no stable public C ABI, dynamic registry, or SDK is created. Playground remains an explicitly supported comparison consumer until R4 migration evidence proves the Minimal consumers equivalent.

## R1-R4 slices

- **R1:** implement the two existing Semantic-to-Scene adapters and a shared identity/order/invalidations mapping. Preserve all currently supported object/content kinds, erase masks, placement, transforms, properties, dependencies and layout/resource references. Publish Full/Delta/Recovery through the existing gate.
- **R2:** implement the pull+notify resource port and typed binding/version checks. Distinguish bytes accepted, decode/layout ready, failed, missing, evicted and provider-replaced. Refresh dependent image/text/brush derivations at owner safe point, publish atomically, mark frame dirty, and never create Operation/Undo.
- **R3:** implement per-instance internal composition and the complete lifecycle/close protocol. Expose narrow input/control/operation/resource/frame/query/readback entry points with real consumers and receipts. Keep Canonical and Preview roles independent.
- **R4:** migrate Windows first, then Web debug-on and debug-off, to the same composition entry point. Deliver a minimal Windows executable and a reproducible Web HTML/JS/WASM/service entry without linking Playground or Debug UI into the minimal core. Android build/APK/physical evidence stays deferred.

## Required preflight before production mutation

P32 must record `implementation-design-preflight.json` with exact current and target owner graphs, file/target mapping, lifecycle table, failure/rollback order, oracle bindings and no unresolved decisions. P32 must run the two frozen preconditions: upstream F01-F08 audit and an old-boundary oracle proving the new composition target/port does not exist at the starting revision. If an old oracle unexpectedly passes, classify `VERIFICATION_DESIGN_DEFECT` and stop.

## Protected work and non-goals

Do not modify `.aegis/authorities.json`, `.aegis/gates.json`, `.aegis/evidence.json`, `.aegis/integrations.json`, prior package/result trees, semantic schema/golden, Operation/Object/Field IDs, docs/api normative ABI, or product feature semantics. Do not revive POC-06/Arc logic, create a dynamic registry, add a second editor engine, add implicit file/network I/O, rebuild Semantic/Skia SDKs, perform G5 optimization, or implement Android delivery.
## R0 contract map

| Contract | Source/port | Owner | Consumer | Oracle/evidence |
|---|---|---|---|---|
| Canonical semantic state and operation clock | existing `semantic::IndexedObjectStore`, `OperationEngine`, `CanonicalCommitClock` | existing `CanvasEditingRuntime` / semantic owners | composition input/control/operation ingress | `r3-runtime-composition.json`, protected interaction regressions |
| Incremental Scene query/spatial | `ISemanticSceneCompiler`, `SceneBinding`, `Scene`, `IncrementalRuntimeCoordinator` | Scene + coordinator | query/hit/draw/reference render | `r1-scene-derivation.json` |
| Full reference/inspection projection | `FullSceneCompiler`, `scene::RuntimeSceneProjection`, `RuntimeScene` | coordinator-owned derived projection | inspection/reference render | `r1-scene-derivation.json` |
| Resource bytes and trusted binding | existing `ResourceProvider`, `ImageResourceResolver`, `RichTextLayoutService` plus new internal typed port | composition owner pump | image/text/brush derived refresh | `r2-resource-port.json` |
| Frame identity and retirement | `RuntimeFrameCoordinator`, `SurfaceLifecycle`, `PresentationTracker` | Runtime frame owner; platform supplies execution evidence | Canonical/Preview surfaces | `r3-runtime-composition.json`, `runtime-isolation.json` |
| Input/control/close | existing RuntimeAppBinding, CanvasEditingRuntime and control/facade lanes | Runtime owners; Host supplies facts | Windows/Web Minimal and optional Debug UI | `r4-consumer-equivalence.json` |

## R0 dependency direction and target graph

`canvas_runtime_foundation <- semantic <- text <- scene <- render <- interaction/editor <- minimal_runtime_host`.
The composition target may depend on the existing owner libraries and platform adapter targets, but no low-level runtime target may depend on `apps/*`, Playground, Debug UI, or the composition target. Minimal core links the composition plus platform adapters only. Debug UI is a sibling optional executable/library. Windows and Web platform targets supply input, resource bytes, surface metrics and execution feedback; they do not implement Session, Scene compilation, resource refresh or frame retirement.

## R0 lifecycle matrix

| State | Accepted calls | Rejected calls | Pending receipt disposition |
|---|---|---|---|
| `created` | `open` | input/control/resource/frame/query | none; open failure closes instance |
| `bound` | `attach/bind`, resource registration, surface registration | editing/query/readback until bind succeeds | bind failure rolls back all partial bindings |
| `ready` | input/control/operation/resource notify/owner pump/frame/query/readback | duplicate attach; direct mutation bypass | receipts are owner-queued and version fenced |
| `lost` | resource notify, rebind/recover, close, owner pump | normal input/frame present | pending frame receipts become stale/retry; no canonical mutation |
| `recovering` | owner pump, rebind completion, close | new attach or direct resource mutation | old tokens rejected; successful recovery republishes atomically |
| `stopping` | drain/feedback, owner pump, close | new input/control/operation/resource/frame | pending work cancelled or explicit stale/closed receipt |
| `quiescent` | detach/close/read-only diagnostic | input/control/operation/resource/frame | no borrowed callback or pending owner work |
| `closed` | idempotent close/read-only terminal status | all mutable calls | late platform/resource completion ignored by binding token |

## R0 failure and recovery order

1. Validate instance state and binding token at the owner queue boundary.
2. Copy external bytes/feedback into owned immutable work; never call external code while holding an owner lock.
3. Prepare all affected Scene/RuntimeScene/resource/frame derivatives.
4. On any failure, keep the last complete published state, retain a bounded retry/dirty marker, and emit a typed receipt; never publish a partial bounds/spatial/layout/resource result.
5. On close, stop acceptance, invalidate tokens, cancel sessions/requests, ask platform to drain, consume quiescence, detach surfaces/providers, then release owners. Late results are ignored.

## R1 conversion boundaries

The incremental path is the only query/spatial publication path. The full path is the reference/inspection path. Both read the same semantic records and use existing geometry, dependency, placement, transform, property, erase-mask and order helpers. `RenderPayloadRef` and `HitGeometryRef` remain instance/generation-scoped derived handles; full ObjectId and variable-length OrderKey are preserved. A `kind_version` change is not used as a substitute for content/palette/resource/derived invalidation. Recovery may full-rebuild but must publish through the same gate.

## R2 resource state and trusted metadata

The sole interaction model is pull+notify. A provider snapshot is requested only at an owner safe point. A trusted manifest/descriptor owned by the document/resource binding supplies expected hash, kind and decode/layout version; provider bytes never self-authorize their own hash. Internal token fields are `document_binding`, `provider_epoch`, `resource_version`, `expected_sha256`, `request_id` and `lifetime_generation`. States are `missing`, `bytes_validated`, `decode_ready`, `layout_ready`, `failed`, `evicted`, `retryable` and `provider_replaced`. Bytes accepted is distinct from decode/layout ready. Shared immutable bytes may be reused by content hash, but mutable decoded/layout caches remain instance/provider scoped.

## R3 composition entrance and platform split

The internal entrance must have real implementations and readback for `create/open`, `attach/bind`, input/control/operation/resource ingress, `ownerPump`, frame request/feedback, copy snapshot/query, and `stop/quiesce/detach/close`. Runtime owns ordering, state, token validity, publication and receipts. Platform owns OS thread/lock/GPU affinity, native resource decode where applicable, surface acquire/present/context and execution feedback. Runtime never waits on an owner lock for a callback that must return to that owner.

## R4 Minimal consumer contract

The Windows executable and Web HTML/JS/WASM/service entry use the same composition entry. The core target does not link `axiom_ink_playground_common`, `canvas_debug_ui`, or Playground/Debug UI source. Playground remains a same-revision comparison consumer. Web debug-on and debug-off must both operate; Android is explicitly deferred. Real tests cover create/select/modify/cancel/Undo/Redo/query/readback, missing-to-ready resource refresh, close with late completion, resize/loss/rebind, and two-instance isolation.

## Resource metadata authority

`runtime/composition/include/canvas/runtime/resource_binding.hpp` defines an internal ResourceBindingSnapshot copied from the accepted Document ResourceManifest at attach/open. ResourceManifest is the canonical semantic source described by ADR-0013; its expected hash, kind, revision, length and decode/layout metadata are not supplied or authorized by provider bytes. Existing `semantic::BrushResourceBinding` is cross-checked against the manifest. ImageContent and TextStyle ResourceId values alone are insufficient to authorize bytes; without a matching manifest entry the resource remains missing/unverified and cannot become Ready. The accepted semantic implementation exposes the manifest at the bound revision; R2 must consume it rather than adding schema fields. This internal snapshot adds no ObjectKind, FieldId, OperationKind, protobuf fields or public ABI.

## Composition target

The implementation target is `runtime/composition/` with `CMakeLists.txt`, public-internal headers under `include/canvas/runtime/`, sources under `src/`, and focused tests under `tests/`. It may depend on existing foundation, semantic, text, scene, render, interaction/editor and control targets; no lower target may depend back on composition or on apps/debug UI.

## Current R2 authority blocker

At the bound implementation revision `4bef9d6e222d156a79a8358f7d88908a48e39f67` (materialized at `601304425870ceb5715ae34a6da3b6ef1dfdd7e5`), `SemanticSnapshot` carries the typed `ResourceManifest`, `resource_manifest.hpp` defines its canonical product types, and `snapshot.proto` serializes the manifest. The upstream P12/P13 Authority is resolved and R2 must bind this source before resource-port production mutation.

## P12/P13 modeling input

`p12-p13-resource-manifest-authority-input.md` records the rules and historical implementation gap. The accepted P12/P13 Authority is bound by exact revision `057464f9fab2db4b67c032134cce3f5f417a64e0`; R2 is unblocked by the accepted typed product implementation at revision 4bef9d6e222d156a79a8358f7d88908a48e39f67.

## Exact upstream Authority binding

- Authority: `fc00x-resource-manifest-p12-p13-v0.4`
- Accepted revision: `057464f9fab2db4b67c032134cce3f5f417a64e0`
- Package: `repo:.aegis/packages/FC-00X-RESOURCE-MANIFEST-P12-P13/package.json@057464f9fab2db4b67c032134cce3f5f417a64e0`
- Package SHA-256: `f8b3ff32b604d19be1f93d37e3f275c64569b199e72cdb05aab204a1e60401d5`
- Acceptance: `repo:.aegis/results/FC-00X-RESOURCE-MANIFEST-P12-P13/p23-authority-acceptance-v0.4.json@f5e0cfbc6b324b9fcabe77280d9363b10b1cf680`
- Acceptance SHA-256: `b77ec8ad1c0da295d6920a98a82efe34cf0bc4104b1aa2fc8913e1203f28b8b1`
- Product implementation at anchor: present at 4bef9d6e222d156a79a8358f7d88908a48e39f67 (materialized 601304425870ceb5715ae34a6da3b6ef1dfdd7e5); this is an execution precondition, not a license to add semantic fields in this package.
