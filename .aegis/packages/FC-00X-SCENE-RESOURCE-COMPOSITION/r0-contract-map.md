# R0 Contract → Source → Owner → Consumer → Oracle map

| Contract | Source / port | Owner | Consumer | Oracle |
|---|---|---|---|---|
| Canonical semantic state and operation clock | existing `semantic::IndexedObjectStore`, `OperationEngine`, `CanonicalCommitClock` | existing editor/semantic owners | composition input/control/operation ingress | `r3-runtime-composition.json` and protected interaction regressions |
| Incremental Scene query/spatial | `ISemanticSceneCompiler`, `SceneBinding`, `Scene`, `IncrementalRuntimeCoordinator` | Scene and coordinator | query, hit, draw list, reference render | `r1-scene-derivation.json` |
| Full reference/inspection projection | `FullSceneCompiler`, `scene::RuntimeSceneProjection`, `RuntimeScene` | coordinator-owned derived projection | inspection/reference render | `r1-scene-derivation.json` |
| Resource bytes and trusted binding | `ResourceProvider`, `ImageResourceResolver`, `RichTextLayoutService`, `runtime/composition` `ResourceBindingManifest` typed port | composition owner pump | image/text/brush derived refresh | `r2-resource-port.json` |
| Frame identity and retirement | `RuntimeFrameCoordinator`, `SurfaceLifecycle`, `PresentationTracker` | Runtime frame owner; platform supplies execution evidence | Canonical and Preview surfaces | `r3-runtime-composition.json`, `runtime-isolation.json` |
| Input/control/close | `RuntimeAppBinding`, `CanvasEditingRuntime`, control/facade lane | Runtime owners; Host supplies facts | Windows/Web Minimal and optional Debug UI | `r4-consumer-equivalence.json` |

## Dependency direction

`canvas_runtime_foundation <- semantic <- text <- scene <- render <- interaction/editor <- minimal_runtime_host`.
The composition target may depend on existing owner libraries and platform adapters, but no low-level target may depend on `apps/*`, Playground, Debug UI, or the composition target. Minimal core links composition plus platform adapters only. Debug UI remains a sibling optional target.

## Lifecycle and failure order

`created -> bound -> ready -> lost -> recovering -> stopping -> quiescent -> closed`.
At `ready`, owner-pump ingress is accepted; at `lost`, normal input and present are rejected or made stale; at `stopping`, all mutable ingress is rejected and pending receipts are cancelled/stale; at `closed`, late callbacks are ignored by invalidated tokens. Binding failure rolls back all partial bindings. Any Scene/resource/frame prepare failure keeps the last complete publication and records bounded retry/dirty state. Close invalidates tokens, cancels sessions/requests, asks the platform to drain, consumes quiescence, detaches surfaces/providers, then releases owners.

## R1/R2/R3/R4 boundary decisions

- Incremental Scene is the only query/spatial publication path; FullSceneCompiler is reference/inspection materialization. Both consume the same semantic records and preserve complete ObjectId and variable-length OrderKey.
- Resource interaction is pull plus notify only. The document/resource binding supplies expected hash/kind/decode metadata; provider bytes cannot self-authorize. Resource refresh changes derived Scene/frame revisions but not Document generation, Operation or Undo.
- Runtime is single-owner/manual-poll. The Host owns OS thread/lock/GPU affinity and platform execution feedback. No callback retains a raw Host/Surface pointer after close.
- Windows and Web Minimal consumers use one composition entry. Minimal core does not link Playground or Debug UI. Android build/APK/physical evidence is deferred.


## Exact composition mutation boundary

The Document ResourceManifest is the intended canonical metadata authority under ADR-0013, but is absent at the current anchor and must be resolved by P12/P13 before R2; the new internal composition is authorized only under `runtime/composition/CMakeLists.txt`, `runtime/composition/include/canvas/runtime/**`, `runtime/composition/src/**`, and `runtime/composition/tests/**`. The RED oracle and focused CTest target must use these exact paths.


## R2 authority stop

`r2-authority-binding.json` must prove a typed accepted Document ResourceManifest source. At the current anchor the oracle is expected to return exit 2 / `BLOCKED_AUTHORITY`; provider bytes, Host metadata, or a composition-owned second manifest are not valid substitutes.
