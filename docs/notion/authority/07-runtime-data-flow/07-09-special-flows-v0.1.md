# 07-09 Special Flows Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c817c9d82f742542a572d
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Special Flow Conformance / Stress-Test Contract

## Purpose

This document stress-tests 07-01..08 against special flows. It does not create a ninth main flow, second canonical truth, second persistence/sync path or global Transaction.

## Meta-invariants

Every Document-changing behavior resolves to existing canonical Operations through OperationEngine. Sessions/IME/snap/route/eraser preview remain transient. Each Operation is atomic; multi-op user intents are not global Transactions. Remote/Replay/Recovery are no-echo. Presentation, durability and sync remain independent. Derived route/bounds/spatial/tile/GPU/surface state remains disposable.

## Stress-test outcomes

- Undo/Redo: new compensating canonical Operations; no history rewind.
- Delete subtree: resolved descendant + fixed-point connector cascade closure.
- Group/Ungroup: existing InsertObjects/SetPlacements/DeleteObjects; multi-op orchestration handled by 07-14.
- RichText/IME: composition transient; committed RichTextDelta atomic; merge/typing history policy remains specialized OPEN.
- Partial erase: SplitStrokes/AddEraseMasks according to existing semantic strategy.
- Connector endpoint drag: route/snap transient, final relation canonical.
- Transform+Snap: snap is transient; final SetTransforms canonical.
- Missing Blob: canonical ResourceId remains valid; placeholder/materialization path from 07-13.
- Surface/device loss: no semantic Operation; derived presentation rebuild.
- Background/foreground: lifecycle itself creates no semantic Operation; document/presentation/sync axes suspend independently.
- Crash/reconnect: Local Recovery Closure + idempotent remote re-drive.
- Duplicate OperationId: AlreadyApplied or collision; no second mutation/change/history/echo.
- Remote vs active session: remote applies first, then 07-14 ChangeSet×footprint conflict handling.

## Undo/Redo

Undo/Redo uses history before-image/semantic inverse to generate normal local compensating Operations, which then follow ordinary ChangeSet/render and DataBridge/persistence/outbox paths. UndoIntent identity stays local-only.

## Multi-operation intent

07-14 resolves the framework: staged preflight, short bounded same-document non-interleaving segment, stop-on-reject and canonical compensation for an already-Applied prefix. No hidden rollback or Cloud/crash atomicity is implied.

## Resource/lifecycle/presentation resolutions

07-13 closes missing-resource semantics and Saved/Synced resource closure. 07-14 closes interruption baseline: never auto-commit transient state. 07-15 closes Presented/CanonicalVisible proof and token-specific resource coverage policy.

## Remaining OPEN

Only owner-specific merge/rebase/collaboration semantics, typing history policy and physical platform/resource realization remain; the special flows do not require a new data-flow architecture.