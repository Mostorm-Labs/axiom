# Cross-platform Parity + Platform Conformance Matrix v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81d19ebacc7546070191
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — G3 Platform Gate Contract

## Scope

Extends canonical corpus to real Web/Windows/Android/Apple Platform Host, surface lifecycle, metrics/DPI, foreground/background, suspend/resume, surface/device loss and bridge/input observable parity. Freezes observable proof, not physical implementation identity or backend winner.

## G3 proof model

Same canonical fixture + same platform scenario meaning → platform implementations → same required observable semantics + platform realization metadata → platform conformance evidence.

Must prove canonical semantic parity; Host/Canvas/Document/Surface/GPU/App lifecycle separation; metrics/surface generations do not mutate semantic truth and stale generations do not publish; surface/device loss preserves canonical continuity and derived recovery; public facade/DataBridge/input observable semantics remain invariant across bridge realization.

## Equality classes

- P0 Semantic Exact: exact verification projection; blocking Spec Requirement.
- P1 Observable Contract Exact/Partial-order: normalized lifecycle/bridge/generation trace; Accepted upstream→Spec, Current Direction→Freeze Candidate.
- P2 Structural/Presentation Correctness: ownership/viewport/visible structure; blocking where upstream Accepted.
- P3 Platform Realization Observation: backend/topology metadata; OPEN/Experimental.

Cross-platform pixel bit-exact is excluded.

## Target families

Web WASM required; Windows native C++ + RN host required; Android native C++ `.so` + RN/native host required; iOS/iPadOS Apple family required. macOS is observation/reference unless product scope promotes it. Web baseline has no separate Arc plane; native Arc is optional acceleration where capable.

## Scenario topology

One semantic golden fixture is referenced, not copied four times. PlatformScenario + PlatformProfile → Platform Driver → native/product test host + Axiom → PlatformObservation (semantic snapshots, lifecycle/surface/bridge traces, capability metadata) → comparator → PlatformConformanceResult → G3 aggregator.

## Normalized observables

Lifecycle vocabulary includes CanvasCreated, HostAttached/Detached, DocumentAttached/Detached, SurfaceBound/Unavailable/Rebound, MetricsChanged, AppBackground/Foreground, suspend/resume, DeviceLost/Recovery, ArcPreviewDegraded/Recovered and sync lifecycle observations. Verification-only additions include facade/DataBridge call issued/completed, LocalCanonicalEventPublished, InputBatchDelivered, canonical/preview submitted/presented, CanonicalVisibleAcknowledged, StaleGenerationRejected, destroy lifecycle and CallbackDispatched.

## Core invariants

- same canonical fixture → exact-equivalent Semantic Document;
- Host Attach and Document Attach are orthogonal;
- resize/DPI/orientation/visibility do not mutate semantic document;
- stale metrics/surface generation cannot publish/present;
- surface loss affects presentation only; rebind uses new generation and recovery/full redraw;
- device loss destroys GPU-derived state only; recovery rebuilds from canonical/RuntimeScene;
- background/suspend is not destroy/detach/delete;
- Arc canonical and preview render targets are separate where Arc enabled; Arc loss falls back without canonical corruption;
- preview clear only after corresponding canonical content is known visible/presented;
- UI/input callback does not synchronously wait on GPU present, JS, Storage or Cloud;
- bridge no-echo/non-reentrancy and stale callback rejection follow runtime/platform authority.

## 07-15 qualification

PlatformQualified proof is capability/profile-qualified, not platform-name-qualified. `PresentSubmitted`, Approximate evidence, stale SurfaceGeneration/MetricsGeneration, missing/stale coverage or non-visible lifecycle state cannot produce `CanonicalVisible(token)`. Qualified PresentedFeedback plus valid coverage in a later frame is required. Platform harness records facts; Render Core remains canonical coverage authority.
