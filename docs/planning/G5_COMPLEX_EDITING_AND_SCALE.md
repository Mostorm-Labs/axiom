# G5 fourth slice: complex editing and bounded scale observations

Repository github.com/Mostorm-Labs/axiom, controlled branch
codex/geometry-navigation-integration. Anchor6cce9d93f8433d568f5383da3e65dd2608ed7cbe
must be an ancestor. User authorizes step4 in this chat. This is engineering
verification of existing capabilities, not WP-G5-00/P23 fixture acceptance,
FC-12/13 qualification, a new product SLO, or official G5/P34 PASS.

Current owners/contracts: docs/notion/manifest.yaml and its current architecture
baseline; sdk/js-v1-contract.md, windows-consumer-v1.md and
shell-axis-preview-contract.md under docs/planning; existing Semantic SnapshotCodec,
OperationEngine, RuntimeComposition restore/capture, CanvasEditingRuntime editing,
RuntimeScene/Scene and Render/ResourceBudgetManager contracts. Existing performance
profiles mark1M as engineering-only and do not accept large-canvas thresholds.
This package does not replace those profiles or imply their acceptance.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: keep4217 and its installed SDK immutable; record anchor source/artifact
  identities before mutation. Normal SDK API/export/dependency/schema unchanged.
- required: root Web SDK mixed edit harness for affine stroke transform, partial
  erasure, sticky text, table text and image/resource replacement. Use existing
  public calls/DOM input only. No private HEAP, deep Runtime accessor, alternate
  document/selection/history/geometry owner, or new business API.
- required: per edit observe admission/application distinction, actual generation/
  operation identity, eventual presented semantic generation, committed-content
  export pixels, undo/redo roundtrip where the existing capability supports it,
  subsequent cold/warm navigation, coarse post-input receipt, no mutation during
  navigation, source/cache counters and clean close. Export/capture outside timed
  intervals; live navigation pixels and committed exports have distinct scopes.
- required: tiers1000,10000,100000,1000000. Object-count axis sparse/dense shapes;
  point-count axis approved compact strokes. Record actual objects,total canonical
  points, viewport point-centre oracle, runtime source candidate points when
  observed, DPR, snapshot/artifact identity, memory/cache accounting and timings.
  Shape-only point counts are0; input samples are not canonical point counts.
- required: native fixture generator/inspector uses pinned Semantic SnapshotCodec,
  then real root SDK restore. Native Scene/index and point projection observations
  are separately labelled components, never substitutes for root SDK rendering.
  Fixtures are verification inputs, not a second production mutation path.
- required: bounded isolated workers per tier; preserve partial stage/exit and
  resource/time/validation rejection. Native aggregate RSS cap2GiB and180s; browser
  owning process family aggregate RSS cap3GiB and180s. Explicit cap/timeout/missing
  fixture/authority rejection -> BLOCKED_EVIDENCE for the affected stage/tier.
  Never shrink counts silently, skip a declared tier without a record, or promote
  an index-only result to full SDK PASS. Reaching all bounded attempt outcomes is
  engineering-run completion, not successful1M qualification.
- required: DPR1 and2 at baseline1K root cases; primary scale progression DPR2.
  Keep existing frame/query/GPU/memory admission policy. Add only private opt-in
  budget/candidate diagnostics if needed, preserving OFF builds without logging.
- required oracles: strict profile/result validation (axis/counts, unavailable
  metrics, caps, partial-stage outcomes); actual native snapshot roundtrip and
  grid/index-vs-independent linear query/point-centre oracle; shared cache/input/
  snapshot/public SDK regression and source/artifact SHA guard. Test meaningful
  new behavior RED/GREEN before implementation. No tests that merely mirror code.
- required execution: all five edit cases, all tier attempts on both axes, sparse
  and dense object component cases, root restore/nav when a valid fixture and
  bounded environment permit; relevant native/Node/Python tests, full configured
  native/Node/workspace checks with exact known-failure comparison, boundary and
  normal install payload checks. No timing measurements during builds.
- required evidence: new verification/evidence/performance/g5-complex-scale/
  package/source/artifact hashes, raw observations and every failure/timeout/cap,
  per-stage results and findings. Preserve old evidence and Gate records.
- implementation fixes: only implementation-owned bugs in the affected existing
  owner revealed by these frozen cases; reproduce before fixing. Missing semantic
  authority/public capability routes to its owner; never bypass accepted limits.
- optional: hosted CI/physical input/platform runs. No physical/Windows ABI claim
  inferred from native harness/Chrome. No dependency or third-party SDK rebuild,
  descriptor/semantic lock change, public API, release, or renderer replacement.
- terminal_success: tooling/tests complete; bounded declared cases all have honest
  durable outcomes and required correctness oracles pass on completed stages.
  Return READY_FOR_CONTROL_REVIEW with per-tier BLOCKED_EVIDENCE findings; no
  throughput/latency/scale acceptance verdict from observational measurements.
- terminal_blockers: repository/source divergence, unavailable locked artifacts,
  authority contradiction, unresolvable new correctness failure, or inability to
  record/contain workers safely. Do not stop at just a plan or an unrun harness.
