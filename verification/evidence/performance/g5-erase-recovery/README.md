# Partial erase recovery and local exact-hit work

Status: **READY_FOR_CONTROL_REVIEW** for this bounded local repair. No official
G5/P34, release, physical-device, universal latency or CPU-percent PASS is issued.

Runtime still owns gesture sessions, OperationEngine and canonical history.
Session and DOM are transport collectors; shared Render owns exact Skia hit
geometry. No public SDK API/ABI, version, semantic schema/codec/descriptor,
dependency lock, Brush algorithm, persistent geometry cache or budget changed.

## Faults and repair

1. A fresh Up could retire ingress, fail the erase semantic commit, yet leave
Session/dispatch contact maps alive. DOM forgot its record. The next Down then
reported DUPLICATE_CONTACT. Fresh rejected terminal control now cancels/retires
only its exact Runtime key, clears transport/baseline metadata and still returns
the original failure. A rejected Move explicitly cancels its last accepted
contact before capture release. Stale terminal timestamps/generations and other
contacts remain isolated. The Web facade schedules derived cleanup on rejection.
2. The old exact-hit path subtracted every historical mask from full stroke
components before asking whether the new eraser intersected them. The erase-only
query now intersects one component with the new mask first, lazily builds old
masks once per query, subtracts them from this local overlap, and stops on a
proven visible hit. Disjoint finite nonempty bounds skip PathOps conservatively.
This uses exact set equivalence `(component - oldMasks) ∩ newMask =
(component ∩ newMask) - oldMasks`. Full visible-selection geometry is unchanged.

A publication-capacity-one rejection reproduces the contact split with real
OperationEngine/SDK code; it is a controlled fault, not a claim that the user's
unsaved 4260 document had a full publication queue. That document and its first
rejection were not inspected or changed. Actual triggering geometry may differ.

The old seeded dense corpus also committed an extra operation on an identical
repeat erase. Every candidate treated that repeat as no-op. Six ordinary erase
commits retain exactly the same canonical snapshot bytes and exported pixels.

## Frozen identities and method

- Task anchor: 797345f113c1e25fd80261cbc55bec2b7d8830d8.
- Task/test package: 518b2730; `docs/planning/G5_ERASE_RECOVERY_2026_11_07.json`.
- Production code and clean ON/OFF artifacts: b47781cc.
- Final verification runner: bd30481f. These verification-only commits did not
  change the implementation or require a different production binary.
- Old Web artifacts: clean 5ab41d5f, production-identical to the task anchor.
- Native comparator isolates only old selection/erase geometry from 797345f1;
  all other linked objects match the candidate. Full compile/link args, source
  and binary hashes are retained. Original discovery native timings had no saved
  old-binary hash and are diagnostic only.
- Installed SDKs retain version 0.4.3, 36 files, root-only exports and no private
  headers/archives. All wrappers/types/WASM/resources are hashed in packages.json.
- Six streamed strokes × 1500 raw samples, then six × 60-point overlapping sweeps.
  Web uses4 samples/real RAF and 4ms sample timestamps; native synchronous inputs
  use 10ms. Do not compare absolute native/Web latency as equivalent platforms.
- Only the isolated test browser's WebCrypto entropy is fixed (xorshift32 seed
  0x12345678), so public document snapshots are comparable. Production entropy
  and identity allocation are unchanged.
- Each worker owns a fresh browser/process family with 180s/3GiB browser or 2GiB
  native containment. All final latency samples ran after native builds/tests.
  Process-family RSS can double-count shared pages; it is not unique physical RAM.
- Optional/shared Render limits remain 32/96MiB; step/query/GPU/time limits stay
  unchanged. Recorded diagnostic shared occupancy peaks are 23.48MiB in dense
  DPR2 navigation and 75.58MiB in mixed post-edit DPR4 navigation.

The first unseeded pilot overlapped regressions and missed queued onError
microtasks. Another unseeded run reached the old redundant no-op operation.
They are preserved as exploratory evidence, not final measurements. An early
whole-component clipping attempt worsened first-erase cost and was withdrawn;
its code/timing is also retained as diagnostic. Final code uses per-component,
lazy local intersection, not that withdrawn variant.

## Measured outcome

Native geometry-only comparison: sixth Up 3508.5ms
→ 242.4ms; all six commits accepted and contacts0.
The frozen local native last-Up<250ms and worst-Up<500ms checks passed.

|Web case|Sixth synchronous Up|Up through fixture settle|
|---|---:|---:|
|candidate-off-dpr1|197.3ms|481.6ms|
|candidate-off-dpr4|201.9ms|1076.8ms|
|candidate-off-dpr2|199.0ms|614.5ms|
|candidate-on-dpr2|198.9ms|598.3ms|
|baseline-off-dpr2|3764.3ms|4164.7ms|
|candidate-off-dpr2-repeat|199.6ms|614.9ms|

Settle includes canonical convergence and the fixture's extra RAF opportunities;
it is not just pointer dispatch or a hardware scanout receipt. These are small,
local engineering observations, not statistical or universal performance claims.

Five candidate dense workers passed rejection/no-mutation,contact retirement,
same-pointer restart and next commit; repeat-no-op;Undo/Redo;blur/capture cancel;
and post-erase wheel document/selection/camera checks. Baseline reproduced
INPUT_REJECTED followed by DUPLICATE_CONTACT and a stranded contact.

Across baseline/candidate,ON/OFF,repeatedDPR2 andDPR1/4,15 canonical checkpoints
per pair have identical exported PNG and snapshot-byte hashes. Deduplicated,
compressed snapshots and a path/hash index are under raw/snapshots and
raw/snapshot-index.json. Successful semantic state was not approximated.

The existing real public-SDK capacity fixture passed 16 long strokes, 12 successive
sweeps,actual browser mouse/capture path,transient-only feedback,preview/commit
pixel equality,Undo/Redo,cancel,RAF quiescence and two-contact pan/zoom. Its
navigation P95 was46.7ms pan and34.3ms zoom in this observation.

An additional rejected-erase pixel fixture proved identical snapshot,export and
white-composited public-canvas pixels after rejection;activeContacts0;same-pointer
retry and visible commit;Undo restoration. It uses public SDK APIs and browser
mouse input. Its out-of-tree fixture source/hash is retained under method/.

The unchanged mixed post-edit runner passed eight edits and 12 navigation phases
on old/new DPR4. Export and navigation pixels match. Selected idle-pan content
submission gap was 17.3ms before and 17.4ms after. This is a single local guard,
not statistical nonregression or physical Mac trackpad qualification.

## Regression and scope

Three focused native tests, 39 G5 Node tests,the pointer collector suite and 119
Runtime boundary checks passed. Full relink retains the same four existing
Semantic enum-switch compile failures. Full CTest is 1012/1078, with the same 66
failed/not-built names and no new failures;the complete lists/logs are retained.
No exclusions or authority/fixture/descriptor rewrites were used.

Task Manager GPU-memory stability alone does not diagnose GPU usage or a leak.
This repair removes measured CPU PathOps work and input-lifecycle retention;
it does not promise zero CPU while drawing,navigating or converging canonical
frames. Long-stroke document/history memory and other cold paths retain their
existing owners and remain separate from this bounded repair.

## Reproduce

Use accepted locked Semantic and Skia SDKs and existing host/Web CMake caches.
Build logs are under logs/,binary/source/package hashes in execution-inputs.json
and packages.json. Root-SDK runners are `verification/runners/g5/dense_erase.mjs`,
`dense_erase_native.py`,the existing post_edit_navigation runner and
`verification/sdk_js/verify_partial_erase_capacity.mjs`. The execution script
under logs/ records arguments and containment. Rebuilding must regenerate the
complete matching SDK;do not mix wrappers and WASM across revisions.

Run `python3.12 verify_evidence.py` from this directory to verify the stored
SHA256 manifest,paired canonical bytes and recorded closure checks offline.
The runnable fixed Demo is http://127.0.0.1:4261/example.html;4260/4242 were
preserved. Per-browser documents are independent;the smoke document does not
alter the user's page or prefill a newly opened Demo.
