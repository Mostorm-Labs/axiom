# 07-02 Pointer / Intent → Canonical Operation Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81d487ced53f9be0a9c2
> Snapshot date: 2026-08-24
> Source status: Current Direction

## Core boundary

`InteractionSession` is transient. `Operation` is the canonical semantic mutation primitive. Preview changes, pointer moves, snap guides, route previews, IME intermediate composition and hover are not Operations.

An Operation is constructed only when Axiom has a final deterministic serializable semantic result. `Preview changed != semantic result finalized != Operation constructed != Operation validated != Operation applied`.

## Session mapping

- StrokeSession → final StrokeRecord → AddStroke.
- TransformSession → final transforms → SetTransforms.
- TextEditSession → committed RichTextDelta → EditRichText.
- ConnectorSession → final connector semantic content → SetConnectorContent/defined related op.
- EraserSession → DeleteObjects / SplitStrokes / AddEraseMasks according to existing semantic authority.

## Operation construction

Before construction, runtime-only pointer/view/snap/prediction state is resolved away. SessionId, previewRevision, viewport, selection, snap target, sync state, retry count, server revision and UndoIntent/HistoryGroup identity do not enter the Operation envelope.

## One intent may produce multiple Operations

Group/Ungroup and other coherent intents may emit 1..N independent canonical Operations. This does not restore a global Transaction. UndoIntent/HistoryGroup is local editor-history grouping, not a wire/sync transaction or cross-operation atomic container.

07-14 resolves the framework: staged semantic preflight → short same-document non-interleaving commit segment → stop-on-reject; if an Applied prefix exists, containment uses new canonical compensating Operations, never hidden rollback. Cross-crash/cross-client intent atomicity remains OPEN.

## Cancel vs Undo

Cancel before canonical apply discards transient state and emits no Operation/local event/Outbox work. Undo after apply generates new compensating canonical Operations and does not rewind a Document pointer or delete committed history.

## Apply boundary

07-02 ends at canonical Operation representation. Decode/normalize/validation/idempotency/prepare/atomic apply/ChangeSet belong to 07-03. `Operation constructed != semantic commit succeeded`.

## Source classes

Local interaction/command/Undo/Redo/AI/import can publish after successful local apply. Restore/Replay/Remote use the same semantic engine but are no-echo sources.

## Remaining OPEN

Typing/history coalescing and session-specific concurrent merge/rebase semantics remain outside this document; 07-14 provides the safe integration framework.