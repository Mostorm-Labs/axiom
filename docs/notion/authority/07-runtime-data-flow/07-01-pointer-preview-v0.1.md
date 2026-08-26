# 07-01 Pointer → Preview Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c8110be4ddd4d078be6e7
> Snapshot date: 2026-08-24
> Source status: Current Direction — Pointer → Preview Runtime Data Flow

## Boundary

Physical input enters Arc/platform acquisition, becomes ordered `PointerSampleBatch`, enters Axiom `InputRouter` and an `InteractionSession`, then Axiom owns smoothing/pressure/brush/prediction semantics and emits immutable preview updates to Arc retained preview presentation.

Arc owns acquisition/presentation, not a second ink algorithm. Pointer/preview hot path must not traverse React state, Shared Data Runtime, Snapshot, Outbox or Cloud.

## Input facts

A conceptual PointerSample includes pointer identity, sequence, monotonic sample timestamp, explicit coordinate space, pressure with availability semantics, phase/type and optional device axes. Sample time must not be replaced by callback or processing time. Missing capability must remain distinguishable from a real zero value.

`PointerSampleBatch` is a transport/scheduling unit, not a semantic event or commit unit. It preserves logical sample order, does not lose terminal events, and each sample still enters interaction semantics.

## Session boundary

`InteractionSession` holds transient state such as start context, viewport/selection/tool snapshot, pointer tracking, candidate geometry, preview revision and snap/hover state. Cancel before canonical apply discards transient state with no Document side effect.

## Prediction

Prediction/rollback authority belongs to Axiom InkEngine. Arc only presents Axiom-produced preview state. Native predicted events, if used, are input hints and cannot become an independent canonical/prediction authority.

## Preview ABI boundary

Axiom → Arc is narrow typed immutable updates. Do not expose Skia object graphs, SemanticDocument/RuntimeScene pointers, arbitrary DisplayLists or long-lived raw GPU resource/fence pointers.

`preview_end` means no more normal gesture updates; it does **not** mean immediate clear.

## Handoff

Required order:

terminal sample → finalize semantic candidate → preview_end/keep visible → canonical Operation apply → ChangeSet/render → canonical content Presented/Visible → `canonical_visible(token)` → Arc clear.

Pointer-up must never directly clear preview.

## Web

Web may use a thin PointerEvent→WASM adapter and may lack the native Arc preview lifecycle. This changes physical realization, not Operation/Document authority.

## Remaining OPEN

Sequence scope/epoch, exact batching/flush/drop policy, predicted-event representation, scheduler coordination and exact Arc ABI numeric/layout choices remain owner-specific OPENs.