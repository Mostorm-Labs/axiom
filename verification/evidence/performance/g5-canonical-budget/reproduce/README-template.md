# Canonical budget fallback and capacity comparison

Result: READY_FOR_CONTROL_REVIEW for this local Render slice. No official Gate,
P34, physical-platform or formal100K/1M PASS is issued.

Implementation: `1cf3f46e8d1176de4fed14e697aa6ae65c40e860`.
Paired installed SDK baseline: `10f3ecd86f60f3dfffb407644af3921817dfcc58`.
Repository/branch: Mostorm-Labs/axiom, codex/geometry-navigation-integration.
The initial frozen task package is at exact commit cd0bce0e;its five local input
hashes remain unchanged. The later package-entry edit resolves its initial
materialization locator and does not change scope,oracles or completion criteria.

The optional cache limit and shared derived-resource hard limit are now independent.
Defaults remain32/96MiB. Private source-build CMake options
`AXIOM_RENDER_CACHE_MIB` and `AXIOM_RENDER_SHARED_MIB` create a64/192MiB experiment;
there is no new JS/DLL API,SDK release or platform budget selector.

Mandatory composite/scratch buffers use the same shared manager and existing
GPU-retirement leases. They no longer inherit the optional32MiB image-size gate.
Preparation uses the existing CompactStrokeRasterJob and unchanged device grid,
geometry,AA,order,erase and material mapping. Critical admission denial yields;
structurally impossible work-buffer capacity rejects explicitly. Pending output
is never presented or acknowledged as a canonical frame. Expected coverage quota
denial now preserves the job and yields without throwing across WASM. Hybrid
correction already copied into the coverage target survives snapshot retries.
Optional cache/picture denial continues to use existing bounded checkpoints.

## Results

Same636x923 CSS mixed corpus:five2048-sample scribbles,two shapes,image,table,
sticky and selection. Each isolated Chrome run covers selection interruption,
two warm cycles,1000ms foreground idle and5s CDP frozen/active. DPR2/3/4 have
all three profiles;an additional DPR4 diagnostics-OFF run qualifies the normal
consumer path. Builds and tests were stopped before timing. Browser workers are
bounded to180s/3GiB and record cleanup. All fifteen phase comparisons have
identical final framebuffer hashes across the three profiles;OFF pixels also
match. Document,operation,selection,camera and presentation checks pass. These
are synthetic root SDK inputs,not physical trackpad input-to-photon measurements.

| DPR4 restored exact work | Old32/96 | Fixed32/96 | Fixed64/192 |
| --- | ---: | ---: | ---: |
| Canonical completion wait | 7408ms | 1429.2ms | 1593.1ms |
| Longest exact owner call | 7268.7ms | 13.2ms | 17.1ms |
| CPU coverage total | 7169.2ms | 570.2ms | 671.2ms |
| Exact attempts | 1 | 80 | 90 |

The default's six observed coverage-quota retries now complete instead of
terminating the SDK frame loop. Expanded capacity had no such retries,but did
not reduce this cold wait further. DPR4 low-density source time fraction after
freeze improves from approximately94.6% at fixed32/96 to72.9% at64/192. This is
a source-density proxy,not an optical blur score or a physical GPU-memory reading.
Maximum shared charge observed at navigation opportunities rises from about
66.7MiB to144.1MiB. Canonical preparation peaks are not continuously sampled by
that navigation counter. Process-family RSS is retained separately;it includes
shared-page double counting and cannot isolate GPU residency.

Keep32/96 as the default.64/192 remains a reproducible experiment. Cold exact
wait and navigation detail are still open. DPR3 cold wait is1181.4->1296.1ms
in this ordered pair;do not promote the local results to latency equivalence.
Next work should inspect remaining exact CPU/checkpoint and scheduling costs,
then supply/retain navigation detail under explicit resource admission. Formal
scale acceptance remains deferred.

## Verification and failed candidates

The112-test native set passes107 with exactly the existing five failures:
canvas_rf01_full_rebuild,canvas_fc02_shape_label_scene,G3HeadlessCanvasDemo,
FC00XRuntimeComposition and SketchSemantic. Eight actual Chrome GPU modes pass:
oversized,frontier,quality,affine,background,contour,replay and exact-replay.
The31 G5 Node tests and118 boundary contracts pass. Three matching installed
packages contain36 files each,without private headers or archives. Source and
Semantic/Skia locks,Brush algorithms,SDK version0.3.9 and old Gate records remain
unchanged outside the explicitly scoped Render implementation and task records.

`exploratory/g5-budget-exploratory-6407` retains a real implementation defect:
the first cooperative candidate escaped on coverage quota denial after continuous
DPR4 navigation. It is not an environment failure or a qualifying result. The
native RED oracle reproduces this branch before3fe40c3f fixes it. The final root
run exercises six such retries and reaches canonical completion without errors.
`exploratory/g5-budget-exploratory-build-race` retains an invalid private probe
started before WASM linking finished;its mismatched JS/WASM compile failure is
excluded. Only fully completed matching builds enter the qualifying evidence.

`comparison.json`,`assessment.json`,`off-corroboration.json` and `result.json`
are mechanically derived. `source-inputs.json`,`consumer-artifacts.json`,raw
compressed observations and `SHA256.json` bind exact inputs/artifacts. Run
`python3.12 verification/evidence/performance/g5-canonical-budget/verify_evidence.py`
to verify this envelope. For reproduction,check out the implementation revision,
consume the locked prebuilt dependencies,configure/install the normal SDK with
the CMake profiles above,and run `checks/g5-budget-final-matrix.py` from repository
root. The private GPU runner and native target/test lists are preserved in checks.

Interactive installs are immutable local staging paths:
4229 serves fixed default32/96 diagnostics-OFF;4230 serves experimental64/192
diagnostics-ON.4226 and its document remain untouched. Instrumented4230 is useful
for correctness/visual comparison;it is not an OFF-versus-OFF latency experiment.
