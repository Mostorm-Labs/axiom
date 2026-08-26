# Axiom Interaction / Behavior Model v0.1

> Source: Notion `Axiom Interaction / Behavior Model v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81a2acdad26855d666f5
> Snapshot date: 2026-08-24
> Source status: Draft for Review — Interaction / Session Boundary Candidate
> Repository status: proposed

## Purpose

This layer sits between Product Object Model and Semantic Schema/Operation. It standardizes how continuous interactions exist without creating semi-canonical state machines per tool.

Canonical flow:

`Physical/Product Input → Command/Intent → Interaction Session → Transient State/Preview → Resolve Final Semantic Result → Canonical Operation → Semantic Document → ChangeSet → RuntimeScene/Render`

Cancellation discards transient state and produces no canonical side effect.

## Core invariants

- Session is not canonical state.
- Operations express final semantic mutation, not pointer moves, hover, prediction, route preview, snap guides, or drag frames.
- One user intention may resolve to one or more Operations without restoring a global `Transaction → operations[]` canonical layer.
- Undo grouping is Editor history/intention metadata, not wire truth.
- Cancel returns to the pre-session canonical Document.
- Preview may be approximate; commit must be deterministic and headless-replayable.

## Common session lifecycle

`Idle → begin → Active → Previewing/Resolving → commit/cancel → Idle`

Conceptual session state may include local-only `sessionId`, start-context snapshots, transient state, preview output, commit policy, undo-intent grouping metadata, and lifecycle. `sessionId`, preview revision, active pointer, hover target, and current snap target never enter canonical Operations.

## Session → Operation mapping

| Session / behavior | Transient state | Commit result | Canonical Operation |
| --- | --- | --- | --- |
| StrokeSession | samples, prediction, preview geometry | canonical StrokeRecord | `AddStroke` |
| EraserSession — Stroke | sweep, hit candidates, temporary hidden set | deleted IDs | `DeleteObjects` |
| EraserSession — Partial | sweep, affected segments/mask preview | split result or erase masks | `SplitStrokes` / `AddEraseMasks` |
| TransformSession | delta transform, snap constraints, preview transforms | final transforms | `SetTransforms` |
| Reorder/Reparent | drop target, insertion guide | final Placement | `SetPlacements` |
| TextEditSession | caret, selection, IME composition, overlay state | committed RichText delta | `EditRichText` |
| ConnectorCreateSession | endpoint hover, route preview, snap | Connector object | `InsertObjects` after schema closure |
| ConnectorEndpointSession | endpoint drag/anchor/route preview | final endpoint relation | connector mutation contract |
| Snap | candidates/guides | no independent commit | host Session's final Operation |
| ViewportInteraction | camera gesture state | ViewportState | local only |

## StrokeSession

`PointerSampleBatch → StrokeSession/InkEngine → PreviewStrokeUpdate + Canonical Stroke`.

Axiom owns brush semantics, smoothing, pressure mapping, prediction policy, rollback, and canonicalization. Arc owns low-latency input acquisition and transient preview presentation, not a second ink algorithm.

The stable StrokeId allocated at stroke begin becomes the final ObjectId. Raw platform events, predicted tail, preview revision, Arc primitives, smoothing buffers, and frame-local sample batches remain transient.

Pointer-up finalizes renderer-neutral StrokeRecord and commits through `AddStroke`. Preview clears only after corresponding canonical content is visibly presented; exact acknowledgement/fence ABI remains an implementation detail.

## EraserSession

V1 modes:

- Stroke Erase
- Partial Erase

Stroke erase resolves affected Stroke IDs and commits `DeleteObjects`. Partial erase resolves to `SplitStrokes` or `AddEraseMasks` depending on the canonical brush strategy. Tile/GPU masks are never erase truth.

Locked Ink is not modified by default; hidden Ink is excluded from hit testing. Product eraser diameter is screen-space and converted to world geometry.

## TransformSession

Begin captures the target identity set, original transforms, selection bounds, active handle/mode, pivot, viewport snapshot, and modifier policy. The target set remains stable through the drag.

Update computes transient transforms through constraints/Snap and renders them without emitting `SetTransforms` each frame. Commit emits final transforms once. Cancel restores runtime preview with no canonical Document mutation.

Attached Connector routing may recompute as derived runtime state during the transform; canonical connector path need not be written every frame.

## TextEditSession

Canonical RichText belongs to Axiom; IME/DOM/native editor overlay belongs to the controlled platform adapter.

Caret, selection, focus, composition range/text, and overlay state are transient. Platform composition events resolve to committed `RichTextDelta`, then `EditRichText`. Intermediate composition strings are not Operations.

## Connector / Snap / Viewport

Connector creation and endpoint editing are Interaction Sessions. Route preview and snap candidates remain transient; only the final semantic relation is canonical. Snap never creates its own Operation. Viewport interaction is local View state and does not enter the Semantic Document.

## Migration note

This is an implementation-facing snapshot of the Notion authority candidate. Promote to `frozen` only after the corresponding upstream/downstream authority review is complete.