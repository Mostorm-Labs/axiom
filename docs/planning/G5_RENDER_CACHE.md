# Render Damage / RenderGroup / RasterCache slice

Repository: GitHub Mostorm-Labs/axiom. User authorization: proceed with
Damage tracking → RenderGroup/RasterCache in this chat. Anchor (ancestor):
64be2cc86da72e535bb7ae38a4ca9d45dfcefc52. Preserve the existing dirty Scene
locality delta; its exact input is `out/g5-render-cache/basis-source.json`.

Authority: G5 v0.3, WP-G5-08..11,
https://app.notion.com/p/3f64c57a590c816e8e8ae42f396a3ff2 .
Formal G5 prerequisite acceptance remains separate; this bounded implementation
does not issue G5/P34 acceptance or select a benchmark winner as new authority.

Owner/data flow: OperationEngine → Scene publication → immutable reference
entries → Render-owned contribution damage and RenderGroup validity → Skia
cache source or permanent Direct fallback → existing provider/present/receipt.
Scene's RF-01 journal is compatibility-isolated as a Scene change journal;
it does not make cache/view/raster policy. No consumer SDK exports/ABI changes.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: separate render damage ownership; generation-bound old/new visual
  contributions; bounded conservative damage; separate content/transform/clip/
  effect/order/resource revisions; disposable bounded Skia-private raster cache;
  direct fallback; preserve full frame validation, provider generation and receipt.
- initial eligibility: compact stroke render groups with equivalent SrcOver
  isolation. Other commands and uncertain effects/masks use Direct. Metadata
  supports all reference entries. No giant texture is required for huge groups.
- required_tests: fail-first repeat-frame raster work, cache-on/off exact pixels,
  overlap/delete/reorder/transform/viewport/clip/background/resource invalidation,
  cache clear/budget/huge fallback, stale frame and failed present, damage old/new
  bounds and bounded merge, group revision separation, host production integration.
- required_verification: native component suite; G2/G3/Ink regressions; full CTest
  failure comparison; Python/Node workspace regressions; runtime boundary checker;
  matching root Web SDK browser smoke and repeated accumulation measurements.
- blocking_evidence: source/artifact SHA256, exact pixels, actual raster/cache
  counters, regression failures and measured timings. Hosted/physical evidence
  is corroborative; never infer Android/Windows/physical PASS from host/browser.
- forbidden: semantic/brush output or SDK locks, new canonical owner, platform
  geometry/cache policy, Tile/Scheduler, stale source as canonical-visible proof.
- success: required correctness/locality checks and reviewer-readable repository
  result/evidence; report any retained compatibility and whole-repository failures.
- blockers: authority conflict, pixel/oracle mismatch, dependency unavailable,
  unresolved production regression, source/artifact drift, unavailable browser.

Design preflight: Render owns the new disposable metadata and representation;
prepare/validation precedes draw; provider present remains the only submission
acceptance seam. Cached content never creates a receipt. Clearing/eviction is
semantically invisible. No unresolved semantic decisions or upstream lock edits.
Cache budget values are replaceable implementation limits, not Product SLOs.
