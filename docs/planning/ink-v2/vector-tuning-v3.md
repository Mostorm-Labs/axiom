# Shared round/marker tuning revision 3

User authorization: implement shared perfect-freehand-style controls and remove
release-end bulges. Repository github.com/Mostorm-Labs/axiom, execution branch
codex/sdk-shell-axis-bridge; starting anchor 89cbd51e1930c8fbd95637614190d7de17aa975c
(ancestor relation). This additive implementation binding extends compact-stroke-v1;
it does not replace historical brush/reference authority or qualification evidence.

Ownership: existing Input -> CanvasEditingRuntime -> BrushSession/CompactBrushSession
-> BrushCommitAdapter -> OperationEngine, with shared Skia preview/canonical coverage.
SDK Session only validates/adapts owned config values. No new public entry point.

Config version 2, algorithm 3, signal 2 are limited to round and compact marker.
Package revision 3 selects them; revisions 1/2 retain original interpretation and
wire encoding. New protobuf config tags 22..30 carry thinning, streamline,
smoothing, easing, pressure model, start/end taper, start/end cap. Unknown versions
and incomplete version-2 configs reject. Old readers reject config version 2.
Persist fitted compact centerline/effective pressure plus config, never expanded
outline/dabs. Replay neither filters nor simulates pressure again.

Thinning [-1,1]; streamline/smoothing [0,1]. Easing: 1 linear, 2 quadratic-in,
3 quadratic-out, 4 smoothstep. Pressure model: 1 timed-speed (existing target/filter),
2 distance proxy using the pinned reference pressure recurrence. The distance recurrence consumes filtered point spacing; raw stationary/release facts never manufacture pressure. Streamline uses
reference interpolation t=.15+.85*(1-streamline). Smoothing blends raw controls
with midpoint quadratic fitting (0 raw, 1 full midpoint); it is a common centerline
control, not a claim of identical perfect-freehand outline-point filtering.
Taper is world arc length [0,4096], with smoothstep envelope. Cap flags control
untapered endpoint clipping. Stationary observations do not raise effective
pressure; simulated terminal observations cannot raise it. Real pressure remains
explicit and capability checked. Defaults: handdrawn 16/.99/.13/.61/linear,
distance model, end taper 16; regular thinning 0/taper 0; marker thinning .5/taper 0.

Preview keeps a replaceable distance-bounded tail for end taper, immutable prefix
chunks, and chunk arc-length offsets. Shared coverage consumes the same distances
and config in preview, canonical, export, replay. Predictive evaluation forks
confirmed state; no global-history per-append evaluation is introduced.

EXECUTION_CLOSURE_CONTRACT:
- Required: Runtime tuning; versioned codec/package identity; SDK declarations,
  decoder/readback and demo; regression for release bulge, width, smoothing,
  filtering, caps/taper, preview parity, operation/snapshot/history persistence.
- Forbidden: dependency/descriptor locks, historical goldens, other brush material
  redesign, SDK publication, performance architecture rewrite, platform geometry.
- Required blocking checks: focused InkV2/semantic/SDK/render CTests, full configured
  CTest run (report existing failures by name), JS/TS contract checks, matching
  WASM SDK build, external real-browser SDK verification, git diff --check.
- Hosted verification: none required. Physical touch/Windows ABI qualification and
  user visual review are corroborative, not replaceable by automated Chrome input.
- Terminal success: checks completed, local matching Web demo available, results
  recorded; user review remains explicit. No Gate/P34/release PASS claim.
- Terminal blockers: authority conflict, locked SDK/toolchain unavailable,
  reproducible unresolved required-test failure. Continue until terminal state.

Geometry v3 coverage uses device-pixel analytic edge coverage for subpixel hairlines.
The geometry shader is bound by `compact-material-v3/manifest.json`; the existing
material-v2 source and identity remain unchanged. Smoothing interpolates position
and effective pressure together.
