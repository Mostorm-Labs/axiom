# Mixed scene preparation and exact-demand reuse

Repository github.com/Mostorm-Labs/axiom; branch codex/g5-settle-timing has been
fast-forwarded and pushed to origin/main@5254b087468db9e91e04f35c705f77f63040e103.
This exact ancestor is the baseline for the user's authorized items1-3. Current
architecture is located through docs/notion/manifest.yaml; G5 v0.3 engineering
scope is consumed as bound in G5_WORLD_NAVIGATION_TILES.md. Existing public SDK,
canonical codecs and BrushSession outputs remain authority, not this document.

Owners: CanvasEditingRuntime owns derived exact demand and presentation state;
Scene/DirectReferenceSource supplies ordered immutable records; SkiaRenderCache
owns per-object preparation/residency. Composition schedules frame opportunities;
providers own acquire/present. OperationEngine remains the sole mutation path.

Current breaks: prepare() globally falls back on any non-compact source,
transformed/masked stroke, capacity count or oversized crop; renderFrame skips
preparation when editing chrome is present. Every retry reconstructs visibility,
source commands, encoded bytes/digest and FramePlan before resuming coverage.

Design: qualify preparation per entry, preserve traversal and exact Direct for
unsupported entries, and retain supported entries under the existing budget.
Pending offscreen work never changes/presents the target. Keep immutable exact
demand across retry using its original, monotonic frame identity; do not mutate
frame-stamped canonical bytes to match a fabricated new identity. Invalidate on
document/scene, camera, metrics/surface/provider, resource or view-adaptation
change. Restrict reuse to ordinary canonical state; active editing overrides,
active text/crop/creation/container previews use their established paths. Idle
selection chrome can be regenerated over a retained demand; new input and tool
transitions invalidate it before reuse.
No canonical receipt is published before the final complete provider submission.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required1: baseline at exact main with100/500/1000 strokes plus rectangle,
  text and image; zoom/pan, drag and continued ink workloads; numeric frame gap,
  callback/settle, preparation/fallback and residency counters. Record native
  versus browser evidence distinctly; preserve matching before artifacts.
- required2: mixed scene no longer disables preparation of eligible compact
  entries; local Direct fallback for unsupported/oversized/overbudget entries;
  preserve order, isolation, alpha, erase, resource and transform behavior.
- required3: retries reuse immutable exact-demand source/plan; no repeated
  visibility/command copying/encode/digest/plan build on unchanged retry.
  Superseding content/camera/provider/resources must rebuild and reject stale work.
- tests: fail-first mixed scene pending/no-present, Direct pixel equality,
  overbudget/masks/transform/chrome, content/order/resource/lifecycle changes;
  real Runtime repeated attempt work counts, supersession and final receipt;
  G5 suite, root configured CTest with named historical failures, Node/boundary/
  workspace, matching Web SDK and real WebGL parity/ink/handoff/navigation.
- evidence: before/after workload data and source/artifact hashes, findings and
  runnable separate matching Web demo. Measurements are not product SLOs or
  physical input-to-photon. No required hosted CI or physical-platform evidence.
- forbidden: brush algorithm/material/asset changes, semantic/SDK/dependency lock
  changes or dependency rebuild, public debug seams, alternate editing owner,
  incomplete coverage or approximate CanonicalVisible, rewriting older evidence.
- terminal:1-3 implemented with regressions/evidence/demo, or precise authority/
  environment/new-regression blocker. No Gate/P34/platform PASS. No automatic
  merge to main or SDK publication. Baseline branch sync was explicitly authorized.

Measured implementation refinement: the1000-stroke mixed fixture showed that
local Direct budget fallback still accumulated expensive compact work at final
submission. Capacity-denied compact sources now stream into one bounded full-frame
offscreen target in original order with the shared scene mapping. A completed
frame replaces the target only after full coverage; partial work never presents.
Caches remain optional and bounded; no cache-cap increase or semantic change.

Verification status: native focused G5RenderCache/G5Navigation and actual Chrome
WebGL private probe are required checks. The full root CTest remains diagnostic;
its failures include G3/FC02 and qualification suites. Exact-main attribution
for failures absent from the older diagnostic remains unresolved; do not call
all of them pre-existing. Platform
physical-device evidence is outside this host/WebGL run and remains separate.

Mixed-scene drag terminal measurement also reproduced synchronous capacity fallback
in ordinary deferred canonical frames. The existing deferred owner now uses the
same cooperative prepare policy for every canonical opportunity, while immediate
native callers preserve their synchronous contract. Composition retry remains
admission rather than commit/presentation; unchanged idle demands retain identity.
