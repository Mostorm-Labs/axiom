# 07-03 Operation → Semantic Document Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81dc9f65ec33c8109492
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Canonical Apply Data Flow

## Canonical write invariant

SemanticDocument is the only canonical semantic truth. Local editing, Undo/Redo, Remote, Replay and Import all mutate it only through OperationEngine/apply. Scene, Renderer, Arc and Data Runtime cannot bypass this boundary.

## Authoritative pipeline

`Operation bytes / typed Operation → Decode/typed view → Common Wire normalize/canonicalize → Envelope Validation → Payload Validation → OperationId Idempotency Gate → Reference/Kind/Invariant Validation → Prepare Apply Plan → Atomic Apply → post-state SemanticGeneration → ChangeSet → source-dependent publication`.

Typed local Operations and encoded external Operations converge on one semantic validation/apply engine.

## Source context

Source metadata controls side effects but is not canonical payload. Local sources publish successful commits. Restore/Replay/Remote never echo into the local stream.

## Idempotency

OperationId is identity:

- new ID + valid payload → apply;
- same ID + canonically equal payload → `AlreadyApplied`, no mutation/generation/ChangeSet/history/local emit;
- same ID + different canonical payload → protocol corruption/collision.

## Validation

Payload validation covers structural/schema/hard-limit correctness. Reference/invariant validation reads current Document state and validates the **resulting state**, including whole-batch hierarchy/cycle/reference constraints.

Cascade Connector Delete is closed: hierarchy closure is extended to a fixed-point connector cascade closure; Atomic Apply deletes the final resolved set and ChangeSet reports actual deleted IDs.

## Prepare Apply Plan

Prepare is validate-before-mutate staging. It may resolve targets, normalize values, compute delete/reference closure, staged hierarchy, replacements, before-images/history inputs and mutation classification. It may not expose half-applied Document state, trigger Outbox/local publication or wait on renderer/cloud.

## Atomicity

Each canonical Operation is whole-op atomic. If one target in a payload is invalid, no prefix becomes observable canonical state. This is Operation atomicity, not a global multi-operation Transaction.

## Post-commit publication

Successful Applied commits advance canonical runtime generation and produce a post-commit ChangeSet. Scene/internal publication and local DataBridge publication occur only after the canonical commit boundary; external sources remain no-echo.

07-11 supplies the precise SemanticGeneration/ChangeSet/ApplyBatchResult integration contract and CanonicalCommitStamp reconciliation.