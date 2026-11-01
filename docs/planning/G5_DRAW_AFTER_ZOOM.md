# Draw immediately after navigation

Repository github.com/Mostorm-Labs/axiom, ancestor anchor
04b3f56cc35811c177f3f11d700ed51861bc7025; scheduling dirty changes preserved.
User reports immediate drawing after pinch-in stalls and authorizes diagnosis/fix.
Consumes G5_WORLD_NAVIGATION_TILES.md and G5_NAVIGATION_SCHEDULING.md; no new
semantic, brush, public SDK, provider or CanonicalVisible authority.

Observed break: serviceNavigationPresentation rejects previewActive and clears
navigationExactPreparation_; Composition then synchronously renders the entire
viewport canonical before preview. Actual Chrome corpus64 measured ~1.65s first
preview despite ~1ms pointer processing.

Owner: existing CanvasEditingRuntime owns active BrushSession and canonical
invalidation. Composition consumes its frame scheduling disposition; Render
continues existing resumable preparation. No platform stroke/drawing state.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: live brush input pauses viewport-only exact reconstruction when the
  semantic/scene/target source is unchanged; preview is still captured/submitted.
- required: post-navigation exact work remains cooperative through preview,
  seal or cancel; new scene is authoritative, never acknowledge partial work.
- required: semantic seal/ARC receipt/cancel/loss rules remain unchanged; no fake
  completion, no cropped fallback, no algorithm/material/schema/SDK-lock change.
- tests: fail-first real Runtime down-after-zoom and down-during-settle, confirmed
  sample/commit, cancel/no mutation, exact retry/convergence, generation/loss;
  G5 suite, actual WebGL renderer, matching SDK handoff/navigation/browser, Node
  facade and boundaries. Compare final pixels and operation/document counts.
- evidence: 64 revision3 strokes, synthetic pinch-in then drawing at 0/150ms;
  pointer callback, preview submission delay, frame callback/gap, artifacts/source
  hashes. Metrics are not physical input-to-photon. Preserve 4191 drawing page.
- hosted verification: none required. Existing full-root historical20 failures
  remain separate; no new P34 or physical Android/Windows/trackpad PASS.
- terminal: regression closed with evidence and separate Web demo, or exact
  authority/oracle/locked dependency/environment blocker; no release/push needed.
