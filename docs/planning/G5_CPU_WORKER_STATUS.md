# G5 independent CPU Worker development checkpoint

## Canonical body material experiment · 2026-11-13

New experimental Demo: http://127.0.0.1:4333/example.html (local0.4.3.0,
diagnosticsOFF, privateCPUWorkerON). Existing4331/4332 remain untouched.
One optional completed unmasked body-material artifact reuses coverage across
mask-only changes. Native eight-copy coverage128→16, subsequent append/remove/
replace/reorder0newsteps, independentpixels andleases/identitychecks pass.

Fresh ordinary DPR4 observations are limited: AB15.312→15.001s, C13.438→11.010s,
but post-last-Worker10.364→10.141s remains. Append-immediatezoom8.839→4.186s;
RAF p95 rose9.1→30.4ms, so this is not a general smoothness claim. Erase-zoom
8.984→8.866s is essentially unchanged. Primary original replay/pixels qualify;
mixed known302-image baseline guard has zero new differences, while global
image qualification remains open. No G5/P34/performance promotion follows.
See `G5_CANONICAL_CONVERGENCE_20261113.md` and the new exact result namespace.

## Render material reuse · 2026-11-13

The scoped dirty candidate preserves complete canonical material for live v4
feedback and shares equivalent completed isolated material between copies.
Cold transient preparation uses the existing cooperative allowance. Native
one/eight-copy work drops from16/128 to16/16 coverage steps; warm erasure adds
zero new coverage. Original semantic mutations, masks, brush/AA and all budget
limits remain unchanged.

Five finite Web windows observed DPR4 AB28.293→13.736s and C17.481→11.566s.
The mixed49-object immediate zoom-out rows remain approximately9s, with no
material improvement. The post-Worker C convergence wait remains about10.6s.
DPR1 candidate measured AB10.233s/C5.458s, with substantial input-time variation.
These are scoped observations, not an overall performance PASS. Final primary
snapshots/Operations and independent CPU reference pixels agree. Corrected
mixed-scene references completed after an initial10s driver misconfiguration,
but both baseline and candidate show the same4325 non-AA mismatches. That
original-reference proof gap blocksRR3; no4333 candidate is served. Existing4331/4332 demos remain unchanged; no new default is
promoted. See `G5_POSTWORKER_RENDER_REUSE_20261113.md` and the exact result root
`.aegis/results/G5-postworker-render-reuse-r1/`.

## CPUW7 postWorker experiment · 2026-11-13

The existing [4331 Demo](http://127.0.0.1:4331/example.html) remains the
default comparison baseline and its assets/user document are untouched.
[4332](http://127.0.0.1:4332/example.html) is an experimental comparison build,
not a broadly faster replacement. Both are local `0.4.3.0` builds.

Actual DPR4 attribution found repeated checkpoint compilation inside prepared
contour drawing. A 32-primitive raster step could repeatedly compile the same
immutable 256-knot checkpoint. The repair retains only the last checkpoint's
output in the existing charged owner-thread scratch; exact visits invalidate
that witness. Original primitive order, source, full canonical masks, blend/AA
and quotas remain unchanged. A structural test reduced compiled inputs from
10,783 to 7,199 with identical pixels, and verified draw → exact → same-draw
scratch invalidation.

Four finite Web windows preserved the complete A7200 → immediate B8 → pan/zoom
→ AB capture → Undo/capture → Redo/capture → C1 sequence. Ordinary paired DPR4
results were mixed: AB Up-to-drain increased from 27.879s to 38.528s, while C
decreased from 27.575s to 18.764s. Input duration also varied materially
(234.030s baseline, 222.420s candidate); no observer-overhead or overall
performance PASS is claimed. Candidate DPR1 measured AB 10.593s and C 6.175s
without a fresh DPR1 baseline. Both final-C snapshots match original
OperationEngine replay byte for byte and their independent CPU pixel references
pass the unchanged AA rule.

Stamp was measured, not optimized: diagnostic C spent about 1.357s on stamp
preparation, about 9.593s in transient rendering and 6.373s in canonical frame
calls. Nested owner/canonical records are separate accounting views, and
opportunity gaps are not all CPU time. Remaining submission/render/resource
waiting is open. This experiment demonstrates one removed repeated-work chain;
it does not close the user-visible long tail, G5, P34 or platform qualification.
The evidence lives in `.aegis/results/G5-CPUW7-postworker-tail-r1/`.

## CPUW6b local exact union candidate · 2026-11-13

The current candidate Demo is http://127.0.0.1:4331/example.html (local
`0.4.3.0`, diagnostics OFF, private pending CPU capability ON). The previous
4330 Demo and user document remain untouched. The sections below this addition
are historical checkpoint notes; current ordered editing capabilities are also
described in `G5_CPU_WORKER_FUNCTIONAL_CANDIDATE_20261112.md`.

Render now separates exact Hit/Miss decisions from complete union paths. It
validates the complete immutable mask and body, indexes conservative bounds, and
requests original-order pieces of at most 32 segments from the existing Worker.
Piece results carry their own type/range/full-input witness and cannot enter the
complete-path cache. Canonical AddEraseMasks still stores the entire original
object-local geometry and the renderer clears the original segments. Later
erasing and selection consume local historical preparation with resumable work.
Whole-path compatibility tests and the original sequential reference are retained.

The same native Fixed40 A7200+B1 case reduced drain from 38.377s to 2.560s and
kernel computation from 35.757s to 83.839ms. The matched real Web DPR1 burst
reduced Up-to-drain from 45.680s to 10.311s. Four finite browser rows completed;
14 captured snapshots match independent original OperationEngine replay byte for
byte, and 14 CPU reference pixel comparisons satisfy the existing AA rule.

Remaining delays are material: DPR1 paced follow-up erasing took 7.128s; DPR4
primary drain took 30.907s and its follow-up took 71.849s. Those waits are not
independently attributed to stamp versus render/resource work. Scene stamp is
unchanged. This is scoped union improvement, not complete tail/performance or
G5/P34 PASS. Native B has one segment; Web B has eight.

Evidence is bound under `.aegis/results/G5-CPUW6b-local-exact-union-r1/`. The
four sampled rows retain their original source/SDK freeze. The final Demo adds
a narrow compatibility correction for legal histories above 8192 segments and
terminal selection-job retirement above 512 consumers; its native RED/GREEN and
the relationship to sampled artifacts are recorded separately. The browser rows
were not rerun after that correction. No commit, push, tag or release was made.

Current development branch: `codex/g5-cpu-worker`.

This checkpoint preserves the shared C++ exact-path implementation compiled to
an isolated CPU-only WASM Worker. JavaScript transports jobs and results; the
existing CanvasEditingRuntime owns pending erase intents and commit order, and
OperationEngine remains the only canonical mutation owner.

Implemented candidate capabilities:

- Partial-erase Up freezes confirmed world samples and ends the contact before
  expensive path computation. Subsequent partial erasing can be admitted.
- The existing Runtime queue confirms and atomically commits erases in order.
- Render publishes fully validated finished paths and reuses them for subsequent
  historical queries. Full geometry comparison, bounded leases and critical pins
  preserve correctness across cache and presentation transitions.
- The internal `AXIOM_CPU_PENDING_ERASE=ON` build connects the Worker. The default
  is OFF. Navigation has a pending-feedback path, but its full browser acceptance
  is incomplete.

The complete background-computation plan is not finished. Creation and dragging
of shape, sticky, table and image objects during pending erase are not queued
with immediate feedback yet. The candidate explicitly rejects competing canonical
mutations as Busy while the erase queue is pending. This restriction does not
establish the requested all-object interaction behavior.

## Measured limitations

The frozen CWH browser run retained the original exact path semantics:

| Case | Ordinary build | Candidate build |
| --- | ---: | ---: |
| 1600-segment Up | 3880.9 ms | 0.4 ms |
| 1600-segment Up to settled canonical | 5028.7 ms | 7178.6 ms |
| 7200-segment Up to settled canonical | Not sampled | 52202.7 ms |

The candidate completed both erases and qualified presentation in the 7200 case,
with zero main-thread new-query and historical full unions. About 44.7 seconds
were still spent constructing 16 distinct local query paths in the Worker;
canonical commit work also remains synchronous. Moving computation off the main
thread did not reduce that total calculation work.

The cold snapshot check exposed an existing codec boundary: the resulting
snapshot encoded to 35,456,391 bytes, but decoding rejected it against the
33,554,432-byte limit. This checkpoint does not change Semantic limits or schemas.

The run is BLOCKED. Pending feedback/navigation, actual browser fault recovery,
strict independent pixel qualification and complete snapshot/Undo/Redo closure
remain outstanding. Native and focused SDK checks are scoped evidence and do not
make the overall performance or interaction milestone pass.

## Local candidate demo

The user-facing candidate is a copy of the exact tested candidate JS, WASM,
Worker and resources; only its HTML title distinguishes it. It uses the existing
SDK Demo and public root exports. No SDK release, version change or new tag is
part of this checkpoint. The publication record is under
`.aegis/results/G5-CPUW-publication-20261112-r1/`.

Rebuild with the accepted locked dependency environment and
`-DAXIOM_CPU_PENDING_ERASE=ON -DAXIOM_RENDER_MEMORY_PROFILE=DESKTOP`.
The candidate path-cache resident upper bound uses the existing profile's
`min(maxEntryBytes, maxBytes / 4)` (64 MiB on desktop). Default and explicitly
requested 8 MiB paths keep their original limit. The synchronous construction
and optional draw-plan limits remain 8 MiB; the fixed Worker heap remains 64 MiB
and the shared desktop budget remains 512 MiB. The earlier CW1/CW2 wording in
`sdk/web/README.md` describes those slices before this CWH resident-policy change.

Frozen source, SDK, numerical and failure records remain in their original
namespaces. Compact text evidence and their complete manifests are published;
bulk SDK assets, raw pixels, binary job buffers and local fixtures stay on disk
at hash-bound paths. No historical failure report is rewritten to success.
