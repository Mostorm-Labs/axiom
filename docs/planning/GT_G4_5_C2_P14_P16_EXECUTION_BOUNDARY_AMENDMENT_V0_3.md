# GT-G4-5-C2 P14/P16 Execution Boundary Amendment v0.3

Status: **CONTROL-ACCEPTED FOR P31 REGENERATION**.

## Execution contract

The existing single `BrushRuntime` remains the only engine. The first profile
is VectorReferenceV1, implemented by the pinned R01 Perfect Freehand evaluator
adapter. `BrushPackage` is authoring/configuration data; resolution produces a
self-contained `ResolvedBrushState`; plan compilation produces a disposable
`BrushExecutionPlan`; the stroke session consumes the plan and emits preview and
confirmed commit outputs.

Lifecycle is `prepare → begin → append(batch) → seal/cancel`. Resolution and
plan compilation occur before begin. `off` stages are absent from dependency
closure and load no state/resources; `auto` stages activate only when declared
dependencies/outputs require them; `on` stages remain semantically active.
Missing providers, cycles, invalid versions and required-but-unready resources
reject before begin; no silent degraded brush is selected.

The plan is deterministic and topologically ordered. Confirmed input is the
only source for `BrushCommitIntent`; predicted input may update a replaceable
preview tail but never canonical state. Preview and canonical paths consume the
same resolved state, plan recipe, node versions, resources and seed.

## Migration boundary

Common composition owns Brush begin/append/seal/cancel and session mapping.
Web/Android retain only platform input normalization and presentation realization;
Windows consumes common preview output. Arc does not resolve BrushPackage or
choose evaluator options. Existing V1 brushes continue through their existing
interpreter. No persistence, sync, RuntimeScene, tile, Skia redesign, or WetMix
implementation is part of this package.

Shape/Grain are inactive for the initial Vector profile. WetMix is explicitly
unsupported and cannot be activated by capability or renderer fallback.

