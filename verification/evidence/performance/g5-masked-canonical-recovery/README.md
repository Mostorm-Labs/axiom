# Masked v4 canonical recovery after duplicated dense ink

Status: **READY_FOR_CONTROL_REVIEW** for this bounded Render repair. This is not
official G5/P34,release,physical Mac input,competitor-equivalence or full UI-latency
acceptance. Remaining delays are explicitly listed below.

## Reproduction and diagnosis

The user draws roughly 20 seconds with default handdrawn round ink at changing
speed,duplicates it 15 times,partially erases,immediately pinches/pans,and erases
again. Earlier tests either used six independent strokes or a simpler fixed-width
wave and waited for quiescence before navigation. They did not qualify this chain.

The new fixture uses 4800 raw samples,four per real RAF,4ms sample timestamps,a
variable-speed self-crossing curve,the actual default round-handdrawn revision4,
15 public duplicateSelection calls,two overlapping25-point erasures and48 immediate
wheel zoom/pan events after each Up. It does not wait or export between Up and
navigation. Exact receipt,quiescence,capture and History checks follow each phase.
The captured default config has size16,thinning0.99,smoothing0.61,streamline0.13;
no alternate simplified brush is substituted. This is synthetic engineering input,
not the user's actual unsaved document or a physical trackpad qualification.

The diagnostic and subsequent unprofiled baseline showed about 6.8 seconds of
canonical CPU coverage after the second erase. prepare() classified every permanent
mask entry as Direct. Direct drawing inside prepare() then processed one entire
compact body synchronously,including every256-pixel coverage tile,without using
the existing resumable v4 contour-record and indexed replay path. A coverage-step
or time allowance could not interrupt this individual Direct entry.

The first native oracle failed with `masked v4 bypassed cooperative coverage and
synchronously presented`. This is a deterministic behavioral RED,not a noisy
millisecond threshold. A close-only defect in the first verification runner is
preserved separately as exploratory evidence;final workers close the module owner
correctly and complete the whole sequence.

## Implementation and ownership

Only `runtime/render/src/skia_render_cache.cpp` changes production behavior:

- Admit algorithm4 compact bodies with permanent masks to existing cooperative
  preparation. Other masked algorithms/non-compact command contracts stay unchanged.
- Complete the pinned body evaluator offscreen,then apply the original mask once
  under the original world/object/device transform and full viewport clip.
- Use isolated scratch for isolated content;retain the existing ordered composite
  semantics for transformed standalone ink. Honor export isolateEraseMasks.
- Store/compose/present only completed content. Existing cache revisions,mask
  invalidation,source/target generations,resource leases and budgets remain owners.

There is no new geometry algorithm,renderer,cache owner,SDK capability,canonical
state,semantic operation,codec,descriptor or dependency build. Optional/shared
Render budgets remain 32/96MiB and original step/time/GPU ceilings are unchanged.

A separate private test-target fix adds -fno-rtti to the Web oracle to match the
accepted prebuilt Skia SDK. The first private link failure and fixed build are
archived. Consumer targets/package exports and third-party SDKs are unchanged.

## Identity and measurement

- Trusted anchor:137e70af294107c4de23a62051d103be30d14e20.
- Task/oracle commit:82789358;runner closure correction:bf51c46a.
- Production Render change:5cb9a102.
- Clean matching Web artifact revision:82291be6;the additional delta is private
  test-target RTTI configuration. ON/OFF both carry this exact build revision.
- Final metric compiler/verification revision:d78488a6. It changes observation
  aggregation only,with another full DPR2 sequential worker. All original raw
  traces,source-before/after inventories and artifact hashes remain intact.
- Baseline installed artifacts:b47781cc,whose production is identical to 137e70af.
- Packages retain SDK 0.4.3, 36 files,root-only exports and no private headers or
  standalone archives. Full file hashes are in packages.json.

The isolated browser fixes WebCrypto entropy with xorshift32 seed0x12345678;
production identity entropy is unchanged. Before/after canonical bytes can
therefore be compared directly. Source/artifact identity is stable within every
worker. Native builds,full regressions and Web builds finished before final
latency sampling. Profiling discovery samples are separate from final unprofiled
measurements. Browser workers use 180s/3GiB process-family containment;all owned
processes were cleaned. Summed process RSS can double-count shared pages.

CPU coverage totals use the **inclusive** parent scope once,including nested
contour compile/draw/replay. Exclusive coverage alone would omit most of the new
cooperative work. The older raw timing field records exclusive CPU;summary.json
recompiles inclusive totals from the preserved stage ledger. The corrected runner
and three RED/GREEN metric tests preserve this definition. OFF diagnostics are
unavailable,represented as null rather than measured zero CPU. Browser CPU time
is CDP TaskDuration delta,not Chrome Task Manager CPU percentage.

## Results

Second erase,default DPR2:

|Metric|Before|After,final verification|
|---|---:|---:|
|Canonical CPU coverage,children included|6807.4ms|754.1ms|
|All canonical attempt time|7062.2ms|1044.6ms|
|Longest canonical attempt|176.7ms|13.4ms|
|Browser CPU time,erase plus immediate navigation|8279.1ms|2168.6ms|

|Case,second erase|Coverage CPU inclusive|All canonical attempts|Whole-phase browser CPU|
|---|---:|---:|---:|
|baseline-on-dpr2|6807.4ms|7062.2ms|8279.1ms|
|candidate-on-dpr2-total|754.1ms|1044.6ms|2168.6ms|
|candidate-on-dpr1|759.6ms|1045.6ms|1668.7ms|
|candidate-on-dpr4|914.4ms|1246.9ms|8167.3ms|
|candidate-on-dpr2|751.2ms|1044.1ms|2184.4ms|
|candidate-on-dpr2-repeat|748.3ms|1048.7ms|2178.5ms|

The frozen local criteria were coverageCPU<1500ms,longestcanonicalattempt<500ms,
and recovery/quiescence<2000ms after the last navigation event. All measured ON
cases met these. OFF independently passed state,receipt,History and recovery;it
has no CPU-stage diagnostic proof. These small samples are engineering evidence,
not statistical or universal performance/CPU claims.

Five canonical checkpoints per before/after case—initial,each erase,Undo,Redo—
have identical snapshot bytes and PNG export hashes. Same-DPR2 final compositor
hashes also match exactly across old/new,ON/OFF and repeats. DPR1/4 live framebuffer
hashes differ in pixel dimensions;their world-bounds exports and canonical bytes
match. Private actual Chrome WebGL/Ganesh oracle comparisons additionally passed
Direct pixel tolerance2 under affine transforms,background/transparent isolation,
small-cache streaming,mask changes and stale surface rejection.

## Remaining observed latency

This is a substantial canonical repair,not the complete Vibe-like interaction
result. The same default-DPR2 trace still has about 0.9 seconds in the first erase
feedback RAF;DPR4 showed about 3.3 seconds. This cold source/material warm-up is not
removed by the cooperative canonical change. Existing CPU100 peaks can therefore
still occur briefly. No CPU35-percent ceiling or short-time-zero guarantee is made.

Immediate wheel navigation still waits for the new Scene's first canonical
submission. serviceNavigationPresentation requires the retained exact source's
Scene generation/read token to match current Scene and no pending canonical
handoff. Consequently,recorded first content response to the immediate navigation
was about 7.1 seconds before and 1.65 seconds after. It is not a 150ms continuity pass.
A lower canonical attempt cost must not be mistaken for visible navigation feedback.

The next two targets are body/prepared-source reuse for duplicated ink and mask
changes under existing budgets,and Runtime-owned new-Scene navigation eligibility
with correct generation/receipt binding. Current service code and receipt rules
were not weakened here. In particular,no old-Scene image is relabeled current and
no navigation feedback qualifies CanonicalVisible. Normal scale acceptance stays
separate from these ordinary-scene gaps.

## Verification and use

- Native masked preparation oracle:24 combinations,strict one-step yield/no early
  present,Direct parity,mask update/supersession,small cache and stale generation.
- Actual Chrome WebGL/Ganesh private oracle:PASS using locked prebuilt Skia.
- Full native CTest:1013/1079,the same 66 existing failed/not-built names;no new
  failures. The same four Semantic switch compile failures remain enumerated.
- 42 G5 Node tests,pointer collector and 119 Runtime boundary checks passed.
- Six candidate sequential workers cover default/diagnosticDPR2,repeat,corrected
  metric aggregation,DPR1 andDPR4. DPR4 completed in 177.8 seconds within the 180s
  bound,so this is not a broad-scale throughput qualification.
- Existing dense erase,16longstroke/12sweep capacity,Undo/Redo,cancel,failed-Up
  pixel/contact recovery and8-edit/12-phase mixed DPR4 navigation regressions passed.

Run `python3.12 verify_evidence.py` here for offline manifest,raw-stage aggregate,
paired-byte and recorded closure verification. Deduplicated compressed snapshots
are mapped by raw/snapshot-index.json. Logs include exact build/test/worker
commands;installed artifacts are identified by complete hashes and rebuild inputs.

Fixed default Demo:http://127.0.0.1:4262/example.html.4260/4261 are preserved.
The separate UI smoke draws,duplicates 15 times,erases 12 times and zooms/pans through
actual Demo controls without changing the user's browser documents.
