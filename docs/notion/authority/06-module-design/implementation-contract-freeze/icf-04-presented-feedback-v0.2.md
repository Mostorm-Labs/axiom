# ICF-04 PresentedFeedback Contract v0.2

> Status: **Current Proposed Freeze — PresentedFeedback / CanonicalVisible Implementation Contract**
>
> Notion source: `3c84c57a-590c-81e7-b376-d66279c4be84`
>
> Supersedes: ICF-04 v0.1.

## Change reason

v0.2 preserves `AxiomPresentedFeedbackV1` layout and enum values but updates the implementation-facing CanonicalVisible predicate to include **live MetricsGeneration** and **live visibility** checks already required by 07-15.

## Data flow

```text
Axiom Render Core
-> FrameId + FrameCoverageManifest
-> canonical presentation target
-> platform/compositor evidence
-> PresentedFeedback
-> PresentationTracker
-> live generation + visibility + coverage validation
-> CanonicalVisible(ArcCanonicalToken)
-> Arc clear
```

Host transports facts. Render Core owns coverage interpretation. Arc consumes an opaque token.

## Stable logical feedback fields

```text
canvas
frame_id
surface_generation
metrics_generation
outcome
evidence_kind
presented_time_ns
```

PresentOutcome: PRESENTED=1, DROPPED=2, FAILED=3, UNKNOWN=4.

PresentEvidenceKind: PLATFORM_QUALIFIED=1, APPROXIMATE=2, NONE=3.

`PlatformQualified` proves a platform presentation fact, not coverage by itself. `Approximate` may support pacing/diagnostics but never clears preview.

## CanonicalVisible eligibility

For pending token T, submitted frame record F and feedback:

```text
Eligible(T,F,feedback) iff
  T is Pending
  AND feedback.canvas == T.canvas
  AND feedback.frameId == F.frameId
  AND feedback.outcome == Presented
  AND feedback.evidence == PlatformQualified
  AND feedback.surfaceGeneration == F.surfaceGeneration
  AND F.surfaceGeneration == LiveSurfaceGeneration
  AND feedback.metricsGeneration == F.metricsGeneration
  AND F.metricsGeneration == LiveMetricsGeneration
  AND live presentation visibility == Visible
  AND FrameCoverageManifest(F) exists
  AND Manifest(F).HandoffCoverage[T].eligible == true
```

If any condition is false, the token remains Pending unless an explicit lifecycle rule cancels it. Timeout/heuristic clear is forbidden.

## Stale evidence rules

- resize/DPI/orientation may make a just-presented frame stale if live MetricsGeneration advanced;
- Hidden/Occluded cannot establish user-visible canonical coverage;
- old SurfaceGeneration or old MetricsGeneration feedback is ignored;
- feedback after RenderView destruction is ignored;
- duplicate feedback/token acknowledgement is idempotent.

## Thread/lifetime

Platform callbacks only enqueue/forward non-blockingly. CanonicalVisible evaluation occurs on the appropriate ordered render/control domain; OS compositor callbacks must not synchronously invoke Arc. Missing coverage manifest means conservative no-release.

## Required conformance

Test stale live metrics, Hidden/Occluded no-clear, fresh Visible clear-once, old surface/metrics feedback ignored, destroyed-view feedback ignored, duplicate idempotency and Approximate never-clear.
