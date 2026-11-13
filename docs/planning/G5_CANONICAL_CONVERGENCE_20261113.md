# Canonical convergence: complete body material reuse

This scoped experiment extends the existing SkiaRenderCache with one optional,
most-recent complete unmasked body-material crop. It does not change Brush,
canonical masks, OperationEngine, input/History order, image sampling, SDK API,
threads, budgets, preparation quanta or the 100ms navigation quiet period.
The prior RR and MQ implementations and historical results remain valid.

Two diagnostic windows distinguished useful canonical progress from repeated
work. Primary C had 437 canonical attempts, 162334 coverage steps and 5.391M
checkpoint replay inputs, with only two preparation resets. Mixed append and
erase used about 435K coverage steps and 20.607M replay inputs; prepared draw
accounted for approximately 3.48s of each canonical diagnostic. Selection scans
and validation were secondary in those mixed rows. These are nested inclusive/
exclusive CPU/API observations, not additive owner/canonical totals or GPU
hardware timings. Diagnostic instrumentation is off in ordinary builds.

Complete masked material already used a full body/pose/permanent-mask identity.
Consequently different masks on otherwise identical strokes could rerasterize
the same body. The new optional artifact retains complete colored body pixels
before any mask. Its identity pins the validated prepared body/config/seed,
properties, exact pose, full support crop, target/device-grid/metrics/context
and resource basis. A hit copies Src into the original full scratch, then
applies every original permanent mask. Per-object z-order and opacity remain
unchanged; the material shader and opacity are not evaluated twice. The source
retention excludes old masks. Only finished pixels enter this artifact.

The same ResourceBudgetManager owns its pixel and metadata leases. Existing
optional bytes, entry limits and mandatory working surfaces keep priority;
allocation denial drops the optional body first and honors GPU retirement.
The cache holds only one last-pose artifact, so rotating poses or large working
sets can limit benefit. This does not add another cache hierarchy or budget.

Actual renderer RED/GREEN: eight same-pose bodies with different masks used
128 coverage steps before and 16 after. Subsequent append, removal, replacement
and genuine two-mask reorder used zero new body coverage and matched independent
original non-AA pixels. New tests assert real body residency/hits, pose/seed/
camera/resource invalidation, source removal, charged in-flight GPU release and
zero bytes after clear/completion. The affected native suite passed 27 tests,
Worker/guard Node checks passed 18, and all 120 runtime boundary files passed.

Four fresh ordinary DPR4 windows preserved the original primary and two-row
mixed immediate-zoom-out inputs, captures and Undo/Redo order. Primary AB
Up-to-drain was 15.312s baseline /15.001s candidate. C was 13.438s /11.010s, but
the interval after the last Worker result was 10.364s /10.141s; most of that
canonical tail remains. Input duration varied from 7.960s to13.172s. These are
single-pair observations, not a general performance qualification.

Append then immediate zoom-out improved from 8.839s to4.186s Up-to-settled,
with canonical current-camera feedback from8.648s to3.994s after first wheel.
SDK RAF callbacks fell515→235, but p95 callback duration rose9.1→30.4ms
(max48.7→49.9ms), so shorter convergence does not prove smoother motion.
Partial erase then zoom-out remained essentially unchanged:8.984s→8.866s.
Current-camera onFrame after all six wheels is distinct from earliest retained/
LOD feedback. No physical display-latency claim is made.

Original sequential replay, all five primary Operations/four snapshots and
pure-v4 final-C CPU pixels match. All twelve mixed feedback/final/redo checks
pass the narrowly frozen known-baseline regression guard: no new strict pixel
differences, no changed old302 image pixels beyond two channels, and unchanged
image dependencies. Global CPU image qualification remains open at302 pixels;
this guard does not rewrite RR3/MQ or declare all-image correctness.

The exact sampled ordinary SDK is served at
http://127.0.0.1:4333/example.html with an experimental label and explicit image/
remaining-tail limitations. Only the served HTML response adds the label; the
sampled SDK assets are unchanged. Old4331/4332 and user documents are untouched.
No commit, push, main merge, G5/P34/performance/release PASS or default promotion
is part of this package. Exact evidence is in
`.aegis/results/G5-canonical-convergence-latency-r1/`.
