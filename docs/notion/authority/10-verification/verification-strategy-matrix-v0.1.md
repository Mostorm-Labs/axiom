# Verification Strategy + Verification Matrix v0.1

> Source page: https://app.notion.com/p/3c44c57a590c8178b629e305a4b2fe93
> Snapshot date: 2026-08-24
> Source status: Draft / Freeze Candidate — Verification Architecture

## Authority / scope

00–09 define what Axiom should be; 10 Verification defines what evidence is sufficient to prove those contracts. Verification does not redesign Runtime. Every requirement remains classified as `Spec Requirement`, `Freeze Candidate`, `Benchmark Target`, `Experimental Target`, or `OPEN`; benchmark suggestions are not Product SLOs.

## Evidence model

Architecture/spec requirement → verification requirement → test/benchmark/fault injection → evidence artifact → release gate/observation.

Five evidence layers: Static Conformance; Semantic Conformance; Runtime Correctness; System Resilience; Performance Characterization. L1–L4 may be pass/fail gates. L5 is measurement/regression unless an upstream authority already freezes a numeric requirement.

## Core invariants

- Semantic Document is the correctness oracle before RuntimeScene/Tile/GPU/screenshot.
- Same canonical Snapshot + Operation Stream must produce canonical-equivalent semantic state across conforming implementations.
- Validate before mutate: rejected whole-op validation must also prove state unchanged.
- Derived state is rebuildable from canonical state.
- Every optimization should have a simple reference mode where practical: spatial↔linear scan, incremental bounds↔full recompute, incremental scene compile↔full compile, tile↔non-tiled reference, cache↔cache-disabled render.

## Verification matrix

`VER-01` functional capability trace; `VER-02` semantic correctness; `VER-03` determinism; `VER-04` codec; `VER-05` cross-language; `VER-06` replay; `VER-07` cross-platform parity; `VER-08` rendering correctness; `VER-09` spatial correctness; `VER-10` Arc latency; `VER-11` runtime performance; `VER-12` memory/resource; `VER-13` cold start/restore; `VER-14` large document; `VER-15` persistence; `VER-16` recovery; `VER-17` sync failure; `VER-18` Blob failure; `VER-19` device loss; `VER-20` surface lifecycle.

Spec requirements include determinism/replay/platform parity/spatial correctness/device loss/surface lifecycle. Semantic/codec/cross-language/render/persistence/recovery/blob are Freeze Candidate where upstream is not fully frozen. Performance/latency/memory/cold-start are benchmark targets; large-document is experimental/benchmark; sync may remain OPEN where policy is not defined.

## Functional trace

Capability → Object → Behavior/Command → Operation → Runtime Capability → Owner Module → Platform → VerificationCase. Each capability should record requirement status, semantic/interaction/platform/failure/performance tests as applicable, evidence artifact and release gate.

## Equality classes

- Semantic Exact: document state, hierarchy, Placement, Transform, properties/content, erase semantic result, RichText semantic state, Connector endpoints.
- Runtime Exact/Deterministic: explicitly deterministic Bounds/hit-test/selection/canonical stroke geometry/spatial query sets.
- Visual Tolerance: glyph raster, AA, GPU blending precision and backend/font rasterization are not cross-platform bit-exact by default.

## Scale / benchmark contract

Standard object counts: `1K / 10K / 50K / 100K / 500K / 1M`. History-scale benchmark fixes viewport, visible count, dirty area, interaction and distribution while increasing total document size. Record write/frame/hit-test timing, candidate count, objects touched, bounds updates, spatial nodes visited and tiles/render-groups touched. Establish curves/regression baseline first; do not invent Product SLO percentages.

Harness must allow configurable spatial backend, tile size, chunk size, cache budget, viewport, object distribution, visible count, dirty area, zoom/pan, stroke input, device capability and render backend.

## Arc latency vocabulary

T0 hardware/OS timestamp; T1 Arc received; T2 Axiom received; T3 StrokeSession processed; T4 preview update; T5 preview submitted; T6 preview presented; T7 canonical committed; T8 canonical render submitted; T9 canonical presented; T10 preview clear requested; T11 preview cleared/presented.

Metrics: InputToPreview=T6-T0; InputToCommit=T7-T0; CommitToPresent=T9-T7; PreviewHandoff=T11-T7. Record p50/p95/p99/max/sample count/device metadata and support Arc enabled/disabled/inline-preview comparisons.

## Memory / restore / resilience

Memory accounts include Semantic Document, indexes, RuntimeScene, SpatialIndex, geometry/stroke chunks, render/tile/raster/image/text/GPU caches, Arc preview and total RSS/working set. Verify pressure→eviction→rebuild and device loss→resource recreate.

Restore stages include open requested, metadata, snapshot bytes/decode, Semantic Document restore, OpLog replay, indexes, initial viewport query, first contentful canvas frame, Local Ready, Interactive Ready, reconnect/catch-up and Cloud Ready. Local Ready and Cloud Ready are independently measurable.

Persistence/recovery fault injection covers operation append/snapshot crash, truncated OpLog, corrupt snapshot, missing Blob, disk full, DB write failure, stale cursor, duplicate operation and migration failure. Core oracle: no silently corrupted canonical state; outcomes must be explicit recovery/fallback/retryable/unrecoverable categories.

## 07 final-closure intake

10 must turn 07 correctness contracts into executable evidence without redefining runtime semantics. Priority groups include semantic apply/idempotency, identity namespace separation, incremental/full RuntimeScene equivalence, remote persist-first crash windows, LocalRecoveryClosure, external no-echo, missing-resource closure, active-session conflict/multi-op compensation, PresentedFeedback/stale-generation/canonical coverage, lifecycle late-event rejection and performance candidates.
