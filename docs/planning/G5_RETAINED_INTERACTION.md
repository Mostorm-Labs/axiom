# Retained ordered interaction over 1000 sources

Engineering slice authorized in this chat, repository github.com/Mostorm-Labs/axiom,
anchor ancestor 5254b087468db9e91e04f35c705f77f63040e103. Preserve the preceding
mixed-scene preparation changes. Authority located through docs/notion/manifest.yaml;
G5 v0.3 scope as bound by G5_WORLD_NAVIGATION_TILES.md. This local package is an
implementation record, not an upstream Gate or new semantic authority.

Already solved: deferred latest-input presentation and per-object reuse for warm
small scenes. Remaining break: a 1000-stroke scene exceeds per-object capacity and
transient full traversal repeatedly rasterizes evicted, unchanged ink.

Owners: CanvasEditingRuntime keeps editing/transform and commit/receipt ownership;
SkiaRenderCache keeps bounded derived content and ordered composition. Providers
keep target lifecycle/present. Borrow retained content/transform separation and
priority admission mechanisms; no third-party implementation is copied.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: retain eligible contiguous compact-content runs while preparing exact
  frames; reuse them in original traversal order for approximate editing. Keep
  background and front/back segments distinct, preserve shared Skia mapping,
  erase/isolation and active object position. Canonical remains exact Direct or
  exact per-object/full-frame composition; retained transparent segments may not
  qualify CanonicalVisible.
- required: count and bytes bounded by existing resource budget, no capacity bump;
  camera, clip, source/content/order/resource/provider/generation changes reject
  stale segments. Unsupported/admission failure preserves safe original rendering.
- tests/oracles: fail-first 1000 sources with tiny object capacity, reuse without
  background raster, exact opaque-prefix pixels, bounded transparent-segment
  quantization and exact final Direct parity; changed content/order/target,
  creation/erase, Runtime latest input/cancel/terminal and receipt isolation.
- blocking verification: native G5 suite, real Chrome WebGL render cache probe,
  matching SDK navigation/handoff/ink, Node/boundary/workspace checks; before/after
  100/500/1000 real-browser drag traces, final pixel identity, source/artifact SHA,
  separate runnable demo. Full root CTest diagnostic with named failures retained.
- hosted: none; physical devices outside this slice, no platform/P34 PASS.
- forbidden: Brush/material/schema/dependency lock/SDK public API changes,
  approximate canonical receipt, shared preview/canonical surface, raising budgets
  to hide repeated work, modification of older evidence, auto-commit/publish.
- terminal: bounded implementation, checks and honest evidence/demo; otherwise
  precise authority, environment, oracle or newly unresolved regression blocker.
  Local durable files are the result boundary for this engineering slice.

Implementation applicability refinement: initial run capture is limited to identity,
unmasked compact commands whose existing shared mapping guarantees independent
isolation. Masked/transformed/unsupported entries remain exact local fallback.
Do not apply transparent segment rounding to canonical frames. Run construction
piggybacks on existing exact materialization; it does not start synchronous extra
coverage work on the first pointer event.
