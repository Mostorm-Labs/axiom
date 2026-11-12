# G5 independent CPU Worker development checkpoint

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
