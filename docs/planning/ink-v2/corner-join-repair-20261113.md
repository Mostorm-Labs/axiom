# V4 corner join correctness repair — 2026-11-13

Package `.aegis/packages/G5-corner-join-repair-r1/package.json`, SHA256
`2c0be8759ee010adff6d072bf032f11ff08d2bd4bde216db573b5f2538933682`.

The existing v4 compiler added a round disk at a negative adjacent-direction dot
but midpoint smoothing did not reach that point. A sparse U-shaped fixture had
an approximately26.2-world-unit gap between that disk and the ribbon. Actual old
production output failed the independent node/neck/support oracle before any
production edit. Its PNG and nonzero RED result remain preserved.

The shared `CompactContourCompiler` now flushes its pending incoming ribbon to
the actual corner's incoming support, emits a bridge with the corner center as
both controls, and restarts the outgoing side at its captured outgoing support.
The original round/marker-ellipse node remains and joins both arms. The bridge
is inside that convex node; no enlarged disk, moved corner, input densification,
pressure change, renderer-only concealment or new semantic primitive is used.
The compiler retains the same O(1) copyable state. Prefix append, tail forks,
seal, replay, prepared geometry and exact selection/erase share that compiler.

## Compatibility

This is an explicitly authorized correction of algorithm4's disconnected join,
not a new evaluator. Old v4 sharp-corner render/query derivations change when
redrawn. Centerline, effective pressure, config, seed and encoded payload do not
change. Earlier algorithm1/2/3 and material algorithms5/6 interpretation and PF
reference/goldens are unchanged. No schema, SDK public API, dependency lock or
package identity is changed. Old evidence and built4333 assets remain historical
and are not relabeled or overwritten. Local version stays0.4.3.0.

Non-sharp straight and smooth source output is compared against a standalone
compiler built from the actual saved preimage:40604 bytes are exactly equal,
SHA256 `46f398551910be0337e9ff3c2057f8559ede68d1971dfc1503cfa2218acb5489`.

## Bounded preparation and shared consumers

An effective knot emits at most two patches and one internal node. Sixteen total
taper cuts, one carried patch and finishing context give conservative saturating
bounds `maxPatches(N)=2N+35`, `maxCaps(N)=N+18`. Full staging, prepared paths,
32-knot job scratch and256-knot checkpoint scratch reserve those capacities
before expansion; leases and entry/global budgets stay with their existing owner.
32-input-knot compilation and32-path-unit preparation advances are unchanged.
The8192-every-knot-turn test completes within a2MiB entry at approximately495184
charged bytes via checkpoints, rejects a tiny hard budget and releases all leases.
The stored-path realization is exercised separately on the small fixed U.

The higher honest estimate can select checkpoint storage for a source previously
admitted as full paths. A self-intersection regression exposed3 channels differing
by1 despite1841 identical component paths: checkpoints interleaved ribbon/cap
rendering per block. Global ribbon and cap primitive ranges now preserve the full
helper's all-ribbons-then-all-caps order. This is a narrow replay integration fix;
the original strict byte-equality regression now passes without a wider tolerance.
Additional replay work is reported honestly; this is not a performance acceptance.

## Verification

The independent shape oracle checks actual node coverage, both raw-arm necks,
exposed support bounds and raster connected components. Archived old geometry,
deletion-only old geometry and an oversized disk are negative controls. It does
not assert `caps.empty()`. Marker checks transform points into its ellipse domain
and test .25/.05 flatness, true neck coverage and convex support. Mirrors,
near90/near180/exact180, adjacent turns, duplicates, dense arms, variable/zero
pressure and taper parity are covered. Strict global C1 is not asserted at a
reversal; ordinary smooth spans retain their tangent-continuity check, and a
non-C1 connection must be within an actual round node.

Full versus chunk1/2/31/32/255/256 and copied-checkpoint rendering is checked with
the corner placed just before/on/after relevant cut positions. Real BrushSession
single/batched and predicted-tail replacement retain equal sealed knots; actual
stable-prefix/tail preview alpha agrees with sealed geometry on the same scalar
coverage grid. An initial test compared global preview with independently tiled
CPU AA and failed; logs are retained. Its corrected comparison first verifies
exact contour components, then the same full scalar grid with the preexisting
2-channel tolerance. It does not change preview production behavior or claim
all tiled CPU AA edges equal.

Across the scoped runs45 distinct related CTests pass, including material/
pressure/quality/work/Retina, prepared/canonical/Metal pixels/working set,
checkpoint/leases, exact query/mask, display handoff and GPU recovery. Runtime
boundary checker passes120 contract files. Initial failures and their resolutions
are preserved. No full-project suite or G5/performance/P34 PASS is claimed.
Prebuilt Skia static-library duplicate-link warnings remain in build logs;
no third-party dependency was rebuilt.

## Actual Web evidence and Demo

One isolated ordinary root-SDK Chrome window used DPR2/CSS900×650, default mouse
simulated pressure and a marker guard with captured angle .7 / flatness .25.
Round and marker preview while Down, canonical output, actual corrected-neck
selection, local erase, Undo/Redo and snapshot restore were exercised. The five
Operation kinds are AddStroke/AddStroke/AddEraseMasks/RemoveEraseMasks/
AddEraseMasks. Original spine/config/pressure/seed remain equal through history;
full equals Undo and erased equals Redo/restore object/mask values exactly.

Actual first adequately wide sharp-corner31×31-device ROIs have one ink component
for round and marker. The sparse high-speed round input becomes extremely thin
later, so the complete line cannot be judged using a binary-alpha connectivity
threshold. Native independent geometry establishes the shape contract; Web
supplies actual finite corner ROIs and shared-runtime/payload/history evidence.

A focused two-stroke native SnapshotCodec→Scene→null-cache Direct CPU rendering
is compared with saved Web full/Redo/restored pixels. A fixed1-device-pixel
geometric boundary band comes from the input contours/masks, not candidate pixel
differences. Non-edge channel differences above the existing2 threshold are zero.
Redo versus restored differs in6 channel values (maximum30), all in that band;
full RGBA equality is explicitly not claimed. GL readback checks actual target and
state restoration without `gl.finish` or consuming `getError`; error-queue
qualification remains outside that witness. This is not a new40-object/image302
oracle, physical input qualification or latency measurement.

The window consumed11 charged seconds of480 and one of two possible starts.
No corrective window was needed. Its owned browser/server processes were cleaned
up. The retained developer Demo is <http://127.0.0.1:4334/example.html>; its41
non-HTML HTTP assets match the exact sampled42-file SDK manifest, and the HTML
response adds only the candidate label. On-disk SDK assets are unchanged after
sampling. Old4331/4332/4333 listeners and user documents are untouched.

4334 is labeled “v4转角圆点修复候选；闲置/长尾问题未闭环”. The idle/input failure
investigations, postWorker tails, global image302 qualification and physical
platform evidence remain separate. No commit, push, tag, release or merge is
performed. Exact source/build/test/pixel/cleanup references are under
`.aegis/results/G5-corner-join-repair-r1/` for independent control review.
