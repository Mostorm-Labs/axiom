# Masked navigation: bounded result and next design review

Status: **BLOCKED_ENDPOINT**, not a Gate decision. This is a proposed follow-up,
not new semantic authority or a silently authorized second rendering pipeline.

The fixed public-root-SDK corpus has 7200 confirmed samples (4ms timestamps),
15 duplicates offset by [1,1], two25point erasures at2x, a2second quiet wait,
and two identical zoom-out/pan/re-entry phases. Ordinary navigation before erase
has a roughly68ms maximum content-submit gap. The original masked cold/hot gaps
are roughly882/870ms. The clean diagnostic candidate measured683/595ms; the
ordinary candidate measured678/602ms with identical settled GPU readback hashes. It
preserves document/selection/commit snapshots, makes actual GPU contour
submissions, and avoids Axiom's CPU coarse-mask path for admitted masked v4
sources. The300ms target is not met. Headless Chrome on this Apple M2 is an
engineering reproduction, not physical trackpad or Vibe evidence.

## What is now isolated

- `SkiaNavigationTiles` cancelled useful in-flight work when coarse parents
  changed; the new native RED/GREEN covers visible versus genuinely offscreen
  work. Candidate cold cancellation/discard totals became zero.
- Masked v4 coarse preparation was still CPU raster even after canonical GPU
  coverage was introduced. The candidate uses existing pinned prepared contours
  and GPU SrcOver, with no CPU edge correction. Raster/unprepared/explicit CPU
  policy keeps its existing fallback. GPU submissions remain separately capped;
  the2ms owner allowance and32/96MiB budgets are unchanged.
- These improvements do not eliminate missing-coverage gating. `NavigationCache`
  matches the complete Scene identity, so a local mask mutation disqualifies an
  old wide-view plane. Tile revalidation compares complete source entries; any
  mask difference invalidates the whole affected tile. Composition waits for
  complete viewport coverage. The trace repeatedly pays to reconstruct
  unaffected pixels before a first useful present.
- Repeated CPU exports/History in the large diagnostic worker exceeded its3GiB
  process-family RSS bound. Those partial runs remain blocked. The latency
  sampler now avoids exports/History, retains snapshots and tests quiescence;
  independent renderer oracles and Runtime regressions own pixel/History proof.
  The Render resource ledger is not a physical Chrome/driver-memory cap.

## Proposed next change: certify unchanged pixel regions

Owner: shared Runtime Scene/Render pipeline. No JS/DLL API or canonical state
owner changes. Do not simply mark an old Scene image valid for a new Scene.

1. Prove that ordered sources, content, affine/clip/material/resource identity and
   common mask geometry are unchanged. Compare exact immutable representations,
   not an incomplete legacy digest. An object insertion/removal/order/resource or
   unsupported effect change retains full invalidation.
2. Derive conservative world damage for added/removed/changed erase masks through
   existing validated geometry/bounds helpers, including object affine, AA and
   sampling gutter. Failed proof retains full invalidation. Do not introduce a
   new hit/Brush evaluator or approximate canonical mask.
3. Carry valid subregions of an old wide plane/tile into the new Scene only
   outside certified damage. Newly accepted canonical pixels cover damage;
   compose disjoint regions without clearing unrendered holes. Keep fixed region
   metadata, bounded admission and complete generation/provider/resource stamps.
4. Preserve GPU retirement and existing32/96MiB admission. If subregion metadata
   or resources cannot be admitted, use current bounded regeneration.
5. Assess source-coverage reuse after local validity is proven. Identical body
   geometry may be shared, but affine/device-grid/material/seed/mask validity and
   lifetime must be explicit; a cached image is not a new geometry authority.

## Required RED and correctness proof before implementation

- Two permanent erasures invalidate pixels inside their damage but preserve
  qualified old pixels outside it; no stale ink leaks through erased regions.
- Mask addition/removal/replacement, Undo/Redo, transforms, translucent overlap,
  background/image composition, resource changes and tile/region exhaustion.
- Reject foreign Scene/provider/generation certificates and failed bounds proof.
- Root-SDK cold/hot zoom-out after settled erasures; actual content-submit gaps,
  first-coverage time, source replay, CPU/GPU work, exact completion, cache bytes,
  snapshots and receipts. Final canonical pixels remain unchanged; permitted GPU
  edge AA is checked independently against geometric boundary bands.
- Same-source ordinary/diagnostic artifacts and physical trackpad follow-up.

Do not expand per-frame CPU time or runtime memory to disguise missing coverage.
Do not claim the current user-visible stall is closed from lower CPU work alone.
