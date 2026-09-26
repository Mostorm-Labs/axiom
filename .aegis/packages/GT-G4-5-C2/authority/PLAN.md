# P30 implementation plan v0.4

Role: implementation planning. Authority: P12/P13 CONTRACT + P14/P16 + P20.
Objective: first complete Vector pipeline through persisted replay and platforms.
Non-goals: additional brush effects, migration tool and P34 decision.
Required analysis/output: dependency-ordered vertical slices and frozen closure.
Quality gate: P31 content bindings and completeness checks. Handoff: P32 later.

1. Reconcile repository/anchor, validate locks, record design preflight. Freeze
   production baseline and observe behavioral RED before production mutation.
2. Package JSON parser, parameter resolver and constrained stage compiler; V11,
   PLAN and immutable snapshot tests. Use explicit option presence, no zero sentinel.
3. New common session and R1 node; incremental output, prediction and bounded
   reliable handoff; R1, PREDICT, HANDOFF and PERF acceptance.
4. Materialize schema/registry carriers together with semantic codec/normalizer,
   validators, operation builder, snapshot, history, scene and outline rendering.
   Wire negatives, V12 save/reload/undo/redo and actual nonempty Skia observation.
5. Route Web/Android/Windows and Brush Lab through common engine; remove old
   authoring runtime, primitive tricks and obsolete presets/tests. Preserve V1
   read-only decoder/renderer and C0/C1/R01. Platform builds and legacy tests.
6. Required regressions and evidence materialization. Return precise revision and
   observations READY_FOR_CONTROL_REVIEW; implementation never declares P34 PASS.

No slice is a terminal return while later executable obligations remain. A real
missing authority/environment condition is an explicit blocker; do not redefine
semantics or weaken acceptance to pass. SDKs must use AGENTS.md accepted consumer
paths; no source rebuild of Protobuf/Skia and no unrelated lock changes.
