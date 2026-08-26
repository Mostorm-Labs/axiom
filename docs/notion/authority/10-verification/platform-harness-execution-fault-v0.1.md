# Platform Harness Execution Protocol + Fault Hook Contract v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81c3b5b4ca44c86863ae
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — G3 Harness Execution / Deterministic Fault Contract

## Boundary

Verification harness/test-hook semantics only. No Product public API additions. `LateEventFence` is distinct from Document CloseFence, bindingGeneration, surfaceGeneration and GPU fence.

## Core decisions

Shared Runner is execution-protocol authority. Action dispatch, completion and platform observable event are separate facts. Destroy correctness cannot use quiet-time/sleep heuristic; it uses source registration, fence trigger, controlled drain and post-trigger rejection evidence. Seed fault injection uses deterministic verification hooks; real OS/driver fault is supplemental evidence. In-process and out-of-process transports share logical semantics and never transmit real native handles/pointers.

## Topology

ScenarioRunner owns CommandSequencer, CompletionRegistry, EventRecorder, SourceLeaseRegistry, FaultCoordinator, LateEventFenceManager and ArtifactWriter. Adapter receives logical action and reports receipt/completion/event/source/fault facts.

## Session

Session config carries protocol version, run/scenario/profile/session identity, strictly increasing non-zero sessionEpoch and required capabilities. Session states NEW→OPEN→CLOSING→CLOSED. Only OPEN accepts new action/source leases; CLOSED rejects any old epoch event/completion/lease. Process reuse must not reuse epoch. Late-fence scenarios complete fence protocol before teardown.

## Envelope

Verification wire envelope carries protocol/version/messageId/messageType/session identity/payload. Message types include HELLO/open/close session, action request/receipt/completion, event draft, source lease open/close, source attempt, fault/fence status, capture and adapter error. Runner owns commandSeq; inbound protocol receives runner ingressSeq; eventSeq is a separate EventRecorder namespace.

## Action/completion

ActionReceipt reports DISPATCHED, COMPLETED_SYNC, NOT_SUPPORTED or ADAPTER_ERROR. DISPATCHED with required wait returns a one-shot CompletionToken. Token is session-local opaque identity, not Product RequestId/FrameId/CanonicalToken/GPU fence, and has exactly one terminal completion. Unknown/duplicate/mismatched completion is protocol invalid.

## EventDraft

Adapter reports session/epoch/source lease/event/source/step/canvas and relevant surface/metrics/target facts; Shared EventRecorder alone assigns eventSeq. Async drafts require source lease. Old destroyed canvas/fenced scope cannot forward valid event. Raw stale OS/driver callbacks dropped by lifetime guard do not become PlatformEvent.

## SourceLease / SourceAttempt

Observable async sources are explicitly leased and closed so the runner can prove completeness. A source attempt records a late/stale attempt even when correctly rejected before publish. This is essential for destroy/stale-work proof and avoids equating absence of event with correctness.

## Fault hooks

FaultHandle has explicit deterministic lifecycle for activation/clear/pulse and status reporting. Surface/device loss injection must enter the production normalized recovery path rather than reimplement recovery in test code. Present-completion hold/release and stale-generation probes are verification-only seams.

## LateEventFence

Fence arms against a defined runtime scope, closes only when registered sources/completions are drained/settled and then proves post-trigger stale source attempts cannot become valid observable publish. Quiet-time alone is never sufficient.

## Presentation proof integration

Harness must preserve Canvas/Frame/SurfaceGeneration/MetricsGeneration correlation for presented feedback and be able to inject/observe stale-generation or held completion paths. Approximate/submitted evidence cannot be upgraded to PlatformQualified by the adapter.
