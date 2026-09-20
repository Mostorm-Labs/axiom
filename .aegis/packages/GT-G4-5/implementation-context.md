# GT-G4-5 implementation context

## Repository identity and starting point

- Repository: `Mostorm-Labs/axiom`
- Canonical branch: `main`
- Task anchor: `e5d0f418816b85a652f457a8d2235130c65f0a52`
- G4.4 Gate revision: `eb4ca06e2bb44471f413234377b1cfb4e1e76bdf`
- G4.4 integration: PR #131, merged as the task anchor above
- Execution branch: `codex/g4-5-programmable-brush`

The older project-state cache predates the G4.4 integration. It is not authority for this package's Git identity. The exact GitHub integration occurrence, Gate revision and task anchor above control execution.

## Existing seams to preserve and extend

- `runtime/ink/include/canvas/ink/ink_engine.hpp` currently has a minimal runtime-local descriptor and keyed multi-stroke sessions.
- `runtime/ink/include/canvas/ink/preview_model.hpp` and `arc_preview_bridge.hpp` already support keyed preview state.
- Canonical brush and stroke authority remains in `runtime/semantic/include/canvas/semantic/object_content.hpp` and its validator.
- Existing FinePen, Highlighter and TexturedMarker wire/golden fixtures under `verification/corpus/semantic/v1/wire/g1-02r/brush-stroke/` must remain valid.
- Arc already owns preview/presentation primitives; extend it only for renderer-ready temporal transient consumption.

## Required implementation sequence

1. G4.5-00: reconcile current repository seams and record gaps without rewriting historical evidence.
2. G4.5-01: introduce renderer/platform-neutral BrushDefinition and capability profiles.
3. G4.5-02: introduce immutable/versioned BrushProgram IR and deterministic compiler.
4. G4.5-03: implement the single BrushRuntime evaluator with separate commit/preview artifacts.
5. G4.5-04: implement ResourceId-based shape/grain binding and explicit failure/degraded behavior.
6. G4.5-05: implement deterministic domain-separated dynamics/random channels.
7. G4.5-06: cover six persistent V1 brush experiences through the common pipeline.
8. G4.5-07: implement Laser as an Arc temporal transient with zero canonical mutation.
9. G4.5-08: add Brush Lab, independent golden corpus and workload counters.
10. G4.5-09: close automated regressions and emit exact-build qualification bundles for Windows, Web and Android.

Stop at `READY_FOR_PHYSICAL_VERIFICATION`. G4.5-10 is a successor physical Gate and is the only stage that may support final G4.5 PASS.

## Design preflight and ownership boundary

- BrushDefinition and BrushProgram belong to the ink/runtime authoring layer, not SemanticDocument.
- BrushRuntime owns brush semantics. Arc and render consume render-ready output and never reinterpret the program.
- Persistent commit output must continue using the frozen canonical StrokeRecord alternatives.
- Preview and Laser transient state are derived/runtime state and cannot become canonical truth.
- Resource bindings are stable logical IDs; backend handles stay below the render/resource boundary.
- Capability variance is data. A platform may reject or mark a capability unavailable but cannot silently substitute different semantics.

## Dependency policy

Use the locked published Semantic SDK and Skia SDK. Resolve them through repository setup tools and lock files. Do not rebuild the dependency stacks from source or modify their locks for this task.

## Physical-validation cadence

Do not interrupt G4.5-00 through G4.5-09 for repeated device checkpoints. Produce complete qualification builds and manifests, then execute the unified G4.5-10 checkpoint. Pressure and tilt checks apply only to device profiles that actually support them; otherwise record `NOT_APPLICABLE` or `NOT_EXERCISED` without claiming that capability passed.
