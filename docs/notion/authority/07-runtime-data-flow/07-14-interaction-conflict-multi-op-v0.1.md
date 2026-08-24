# 07-14 Interaction Conflict / Multi-Operation Intent Integration Contract v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81028a5ff4a974c9240d
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze

## Active session has no canonical veto

An active Interaction/Transform/Text session cannot block, reject or reorder an order-qualified remote/other-source canonical Operation. Canonical apply happens first; post-commit ChangeSet then informs transient session conflict handling.

Conflict actions modify only transient state: Continue, ReResolve, Rebase or Cancel. They do not secretly mutate SemanticDocument. OperationEngine remains final current-state validation authority.

## Generation is only a trigger

`semanticGenerationAtBegin != currentGeneration` means canonical state changed, not necessarily that the session conflicts. Conflict is determined by intersecting post-commit ChangeSet with an InteractionDependencyFootprint describing the canonical facts the intent depends on.

## Dependency footprint

Logical local control metadata may include base generation, target ObjectIds, semantic components read (placement/hierarchy/transform/properties/content/erase/relation), related parent/attachment dependencies and dynamic query dependencies. Exact representation remains open.

## Session actions

- Continue: dependency-disjoint change.
- ReResolve: preserve raw user intent/input but resolve against current canonical state.
- Rebase: only when a session-specific deterministic intent-preserving rule exists.
- Cancel: target/base invalid or safe mapping cannot be proven.

No silent rebase is allowed without semantic authority.

## Multi-operation coherent intent

No global Transaction is introduced. A coherent local intent may use:

1. provisional local UndoIntent/context;
2. staged semantic preflight using OperationEngine validation logic, not a duplicate validator;
3. short bounded same-document non-interleaving IntentCommitSegment;
4. each Operation commits independently with its own generation/stamp/ChangeSet/publication;
5. stop forward sequence on reject;
6. if an Applied prefix exists, contain failure with new canonical compensating Operations;
7. finalize a user-visible HistoryGroup only after required forward Operations succeed; zero-apply or fully compensated failure is not a successful user Undo item.

This provides no cross-crash, cross-client or Cloud atomicity.

## Lifecycle interruption

Background/suspend/detach/destroy must not auto-commit the last preview, unfinished transform/eraser or IME composition. If an Operation has already entered atomic ordered apply it may complete; otherwise uncommitted transient state cancels/ends.

## Session baselines

Target deletion generally cancels. Eraser may prune/re-resolve. Transform target/ancestor placement/transform change requires explicit rebase authority or cancel. Text same-object change makes EditorProjection stale and requires resync; uncommitted composition is not auto-committed. Exact collaboration merge semantics remain open.

## OPEN

Transform/RichText/Connector concurrent merge/rebase semantics, durable UndoIntent storage and cross-client coherent-intent semantics belong to collaboration/history/protocol work.