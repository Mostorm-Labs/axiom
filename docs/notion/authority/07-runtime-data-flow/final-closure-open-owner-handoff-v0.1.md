# 07 Runtime Data Flow Final Closure / OPEN-to-Owner Handoff v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81db854bf3ab3dbb26fd
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Final Closure / OPEN-to-Owner Handoff

## Closure verdict

07-00 through 07-15 are architecture/data-flow implementation-complete. No remaining blocker requires redefining the runtime data-flow trunk, canonical authority, Operation atomicity, no-echo, remote persist-first, Local Recovery Closure, resource separation or Presented/CanonicalVisible handoff.

This does **not** upgrade every child page's original status and does not mean platform/storage/collaboration/verification physical choices are closed.

## Final invariants

- SemanticDocument is the only canonical truth; semantic mutation only through OperationEngine.
- Each Operation is whole-op atomic; batch/UndoIntent/IntentCommitSegment is not global Transaction.
- Only successful local-source apply enters local stream; Remote/Replay/Recovery are no-echo.
- OperationId, SemanticGeneration, CanonicalCommitStamp, RecoverySequence, server cursor, UndoIntent, ArcCanonicalToken, FrameId, SurfaceGeneration and BlobRef do not alias.
- ChangeSet is post-commit impact information; RuntimeScene/bounds/spatial/tile/GPU state is derived/rebuildable.
- `PresentSubmitted != Presented != CanonicalVisible(token)`; platform transports evidence, Render Core decides coverage, Arc consumes opaque token.
- `Committed != LocalDurable != CloudSynced`; presentation and storage/cloud do not synchronously wait on each other.
- Remote dispatch requires DurableInbound + OrderReady; canonical remote apply implies durable inbound evidence.
- LOCAL READY reconciles recovery-required/order-ready inbound; never-OrderReady gap-blocked inbound does not block local readiness.
- `BlobRef identity != local bytes != integrity != materialization != CloudAvailable`; resource readiness never advances semantic generation.
- Active InteractionSession has no canonical veto; ChangeSet×dependency-footprint drives transient conflict handling and OperationEngine remains final validator.
- Lifecycle interruption never auto-commits transient state; surface/device loss never rolls back Document.

## Final end-to-end contract

Pointer/Command/External Operation → interaction/semantic resolve → canonical Operation → decode/validate/atomic apply → CanonicalCommitRecord (generation + commit stamp + ChangeSet) → RuntimeScene/bounds/spatial → FrameState/render sources/coverage manifest → PresentSubmitted → platform PresentedFeedback → CanonicalVisible(token) → Arc clear.

A local commit independently forks to DataBridge → LocalDurable → Outbox → CloudSynced. Remote bytes enter through DurableInbound + OrderReady. Open/restart enters through Snapshot + continuation + inbound evidence → LocalRecoveryClosure.

## Status-drift corrections incorporated

07-14 resolves earlier OPFLOW multi-op framework gaps. Cascade Connector Delete closes old connector-delete OPEN. 07-11 closes SemanticGeneration/ChangeSet integration. 07-15 closes occlusion/resource handoff ambiguity. 07-13 closes missing-resource semantics. 07-14 closes lifecycle no-auto-commit baseline.

## OPEN → 08 Platform

Pointer timestamp/sequence/batching/predicted-event physical realization; scheduler coordination; frame queue/in-flight policy; cross-surface composition; FrameGraph-to-platform execution; Windows/Android/Apple/Web presentation proof; surface/device/background callback ordering; Text/IME physical adapter and lifecycle.

Platform experiments remain OPEN/Current Direction until POC evidence; 07 closure does not upgrade physical candidates to Accepted.

## OPEN → Collaboration / Sync Semantics

Same-target Transform/Placement merge, RichText CRDT/OT and active IME mapping, Connector concurrent semantics, Undo against remote changes, cross-client coherent-intent semantics, server ordering model, remote rejection/quarantine/rebootstrap, large-gap snapshot authority/pending-Outbox reconciliation, outbound server rejection/conflict and Blob cloud protocol.

These decisions may not violate 07's persist-first, atomicity, no-echo, identity separation or safe session fallback.

## OPEN → 10 Verification

Verification must provide conformance/evidence for semantic apply/idempotency, incremental/full Scene equivalence, persist-first/recovery crash windows, no-echo, frontier advancement, resource closure, lifecycle interruption and Presented/CanonicalVisible stale-generation/platform-proof behavior.

## Codex intake rule

07 is now a complete local architecture input. Implementation agents may choose physical details only where the relevant OPEN owner permits them; contradictions with the frozen/proposed-freeze invariants are architecture blockers, not implementation discretion.