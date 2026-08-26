# 07-05 RuntimeScene → Render → Presented Runtime Data Flow v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81a494d0e8e1fe772bcc
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Render / Present / Handoff Data Flow

## Presentation vocabulary

Never collapse:

`CanonicalCommitted != SceneReady != FramePrepared != RenderSubmitted != PresentSubmitted != Presented != CanonicalVisible(token) != PreviewClearedPresented`.

GPU completion also does not prove OS/browser presentation.

## Shared vs per-view state

RuntimeScene is shared derived document execution state. FrameState/ViewQuery is per canvas/view/frame and carries viewport/camera, surface/metrics generations, scale, deadline, damage/quality policy and pending handoff requirements. Viewport/surface state does not enter shared RuntimeScene.

## Render flow

RuntimeScene + FrameState → Spatial visibility/cull → Tile coverage and scheduler → valid RenderGroup/Tile/direct/live render sources → FrameGraph → Skia/GPU → Axiom canonical surface → platform present → presentation feedback → Render Core coverage evaluation → `CanonicalVisible(token)` → Arc clear.

## Visibility and cache rules

Camera changes alter ViewQuery/Tile coverage, not semantic Document state. SpatialIndex may return false positives but not false negatives; render order is resolved separately.

Tile priority is Visible > Near > Prefetch > Background. Dirty invisible tiles remain dirty-but-lazy. Tile keys must not include global Document revision.

RenderGroup/Tile caches are derived optimization. Cache miss changes performance, never correctness. Transform-only reuse is allowed only when dependencies remain valid.

## Handoff correctness

A presented frame may legally reuse old but still-valid local cache generations. However, a dirty old tile reused under deadline cannot satisfy a handoff token whose required canonical contribution lies in that region.

Therefore `Presented` and `CanonicalVisible(token)` are distinct. Arc preview clears only after a platform-qualified presented frame also satisfies Render Core's canonical coverage predicate.

07-15 freezes the proof/correlation/coverage contract.

## Lifecycle

Surface/device loss invalidates derived presentation state and in-flight generations, never canonical Document state. Late feedback from stale surface/metrics generations cannot satisfy current handoff.

## OPEN

Exact swapchain/layer APIs, FrameGraph struct, in-flight depth, Tile sizes/scale buckets and physical platform proof mechanisms remain platform/benchmark decisions.