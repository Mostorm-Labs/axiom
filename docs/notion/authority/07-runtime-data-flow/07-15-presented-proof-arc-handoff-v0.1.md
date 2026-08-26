# 07-15 Presented Proof / CanonicalVisible / Arc Handoff ABI Integration Contract v0.1

> Source page: https://app.notion.com/p/3c44c57a590c815f94fcc97756051977
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze

## Core invariants

`PresentSubmitted != Presented != CanonicalVisible(token)`.

Present/swap return, GPU completion, vsync/rAF or transaction submission do not alone prove user-visible presentation. PlatformQualified evidence is profile/capability-qualified, not platform-name-qualified. Approximate/None evidence never releases Arc preview.

Proof is scoped to Canvas presentation stream + FrameId + SurfaceGeneration + MetricsGeneration. Stale feedback from destroyed/old generations is discarded.

Platform Host transports presentation facts; Axiom Render Core owns canonical coverage evaluation. Arc only consumes opaque CanonicalVisible tokens.

## PresentedFeedback common contract

Logical common feedback carries CanvasHandle, frame_id, surface_generation, metrics_generation, PresentOutcome, PresentEvidenceKind and optional presented timestamp. Platform adapter must correlate platform-private submission identity back to the original Axiom frame/generations; it may not relabel late callbacks as the current frame.

## FramePresentationRecord

Runtime-private correlation binds Canvas, FrameId, SurfaceGeneration, MetricsGeneration, targetSceneGeneration, submission time, platform-private correlation and FrameCoverageManifest reference.

## FrameCoverageManifest

For each handoff-capable submitted frame, Render Core retains immutable evidence of which ArcCanonicalTokens are actually eligible and why. Diagnostic reasons include stale canonical source, dirty old tile reuse, transient-only source, resource policy failure, surface/metrics mismatch and uncovered required region.

If coverage evidence is missing/evicted, behavior is conservative no-release and request a later frame.

## HandoffRequirement

Runtime-private token requirement includes Canvas/stream identity, minimum semantic generation, canonical visual coverage predicate, resource coverage policy and lifecycle/cancellation state.

Resource policy is either `CanonicalFallbackAllowed` or `MaterializedResourceRequired`; placeholder eligibility is token-specific, not a global guess.

## Eligibility

CanonicalVisible(T) requires: token pending; matching canvas/frame; Presented outcome; PlatformQualified evidence; feedback generations match the recorded frame and live surface/metrics generations; live presentation visibility is Visible; FrameCoverageManifest exists; token coverage is eligible.

Otherwise token remains pending or is explicitly cancelled by lifecycle policy.

## Arc ABI

`ArcCanonicalToken` is opaque session/process-local identity and must not alias FrameId, SemanticGeneration, CommitStamp or GPU fence. `canonical_visible(session, token)` is non-blocking and idempotent/harmless for duplicates; stale/unknown token cannot clear another preview. Acceptance of clear request does not mean clear frame itself was presented.

One qualified frame may satisfy multiple independent tokens.

## OPEN

Exact Windows/Android/Apple/Web presentation proof primitives, surface topology, timestamp normalization, multi-display/VRR/mirroring and future Web separate preview plane belong to 08 platform qualification.