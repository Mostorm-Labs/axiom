# Navigation continuity repair execution contract

Repository: GitHub `Mostorm-Labs/axiom`. User-authorized repair, anchor
`51f93640` (ancestor), controlled branch `codex/geometry-navigation-integration`.
Consumes the existing G5 render, Runtime camera, root SDK and resource ownership
contracts, plus the source-bound lifecycle reproduction at that anchor. This is
an implementation slice, not new semantic authority or Gate acceptance.

Owner flow: root SDK DOM facts -> shared Runtime camera/demand -> Render-owned
cooperative world coverage/refinement -> Skia/provider feedback. Exact canonical
preparation and qualified completion remain independent. No public API impact.

Execution closure:

- Prioritize bounded complete coarse viewport coverage before fine tiles. Keep
  useful coarse coverage resident while it supplies feedback. Prewarm/fine jobs
  must yield priority when a new visible coverage frontier is missing.
- Reconcile the v4 prepared-geometry admission bound for long strokes using a
  budgeted staging pass of the pinned contour compiler, then actual contour
  counts for path admission. Keep per-entry/global limits, leases and denial
  fallback. No new Brush geometry or altered contour pairing.
- Keep full-viewport coverage checks, immutable source stamps, stale-generation
  rejection, erase/material behavior, GPU backpressure, cooperative time/step
  limits, exact pixels and canonical receipt rules.
- Bound prepared-contour work at primitive checkpoints and separately limit GPU
  contour submissions per opportunity. CPU time does not measure GPU debt.
  Coarse v4 coverage masks may use bounded Skia raster memory within the existing
  tile lease, followed by an implicit GPU upload; material composition and final
  presentation remain on the registered GPU provider. Other algorithms retain
  their existing coverage realization. This does not claim zero internal copies.
- No SDK/semantic ABI, descriptor/SDK lock, third-party build, old Gate record,
  user-page document mutation, worker pool or platform renderer change.

Blocking oracles: RED then GREEN deterministic coarse-first coverage/progress
and long-v4 admission tests; existing cache/coverage/material/budget/lifecycle
and Runtime navigation regressions; real WebGL2 pixel checks; matching rebuilt
root SDK ON/OFF wheel selected/unselected mixed-scene repeated/idle/recovery
measurements. Check actual canonical framebuffer submissions and unchanged
document/selection/operation, not RAF alone. Reuse the locked SDK artifacts.
Submission observation includes default-framebuffer color clears and color blits
as well as draw calls. Compare both versions with the same corrected observer.
GPU parity reads each completed test target before drawing the other reference:
two identical complete reference streams queued together on the shared Ganesh
context themselves differ at an AA boundary. Keep strict byte equality and an
independent path/matrix/paint stream oracle; do not introduce production waits.

Required local evidence: exact source/build/package SHA256, raw before/after
measurements, work counters, pixel comparison and regression logs. Require a
material submission-gap reduction in the frozen reproduced wheel corpus without
relaxing correctness or introducing synchronous waits. Retain every tail. This
does not freeze a universal physical latency SLO. Longer foreground idle is a
required corroborating scenario; real trackpad/OS sleep remains separate.

Run the full configured native/verification commands; distinguish reproduced
pre-existing failures/tooling blockers from new failures. Hosted CI is optional.
Local committed evidence is the durable review boundary; no push/release required.

Continue through implementation, relevant regressions, browser reverification and
durable materialization. Terminal blockers: authority/verification conflict,
unavailable locked artifact/compiler/browser, source drift, broken correctness,
new high-impact regression, or failure to improve the frozen reproduction.
Implementation returns reviewable results, never P34 PASS.

Design preflight: state/commit ownership stays unchanged; preparation leases are
reserved before temporary/persistent allocation and released on denial/abort.
Coverage publication requires the complete current viewport. Fine/prewarm work
is disposable and preemptible. Unresolved semantic decisions: none; no authority,
verification or package-scope conflict identified at entry.
