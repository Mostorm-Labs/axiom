# Live erasure → input feedback diagnosis — 2026-11-13

Package: `.aegis/packages/G5-live-erase-input-feedback-r1/package.json`, SHA256
`20465f205a71f162055b3354f869e638d98ed6025f03b6cb934ff3a0b0cdd113`.
Outcome: **finite no reproduction with actual pending-draw coverage**. The user
problem remains open. This package does not change production behavior, rebuild
SDKs, run another20-minute idle or claim performance/G5/P34 PASS.

Both required isolated headed-Chrome windows completed: exact ordinary4333 SDK and
the retained IF diagnostic SDK, DPR1, CSS822×900, camera .34/(120,260), Fixed40,
mouse source1 and default round-handdrawn revision4 unchanged configuration.
Each window used three160-move local erasures, then a64-move stroke and32-move drag
without a settled wait, settled controls, and60-second no-poll pause controls.
The user4333 page/document and its served SDK were not accessed or replaced.

## What was exercised

Each erasure was non-noop:40 targets,40 new swept masks,160 segments per mask.
Each run therefore added120 masks /19200 segments to the existing80 masks /2560
segments. All original40 body/property/placement values and historical masks
remain byte-identical under offline field comparisons; only the actually selected
original object's pose changes. Actual Operations supply the new mask geometry
and object IDs; final snapshots contain those exact values.

The next stroke Down arrived at generation2 while erase serial3 was waiting,
with a separate active create intent. Both runs admitted it; move16/move32 ROIs
contain visible amber pixels, and its AddStroke commits through the existing owner.
Diagnostic display order is admitted brush4 → sealed erase3 → sealed brush4.
The three strokes and three transforms plus the erasures give9 Operations,
final43 objects / generation9. Each stroke's persisted64 knots agree with its
Operation. This rules out a deterministic failure of this exact pending-draw
sequence at the sampled points, not every possible handoff failure.

All three drags in each run selected original object
`80202ea91a27b2511dd5fd34cf6c40a5`. Actual poses, pending selection bounds while Down
and real canonical target ROIs are retained. Requested displacement (+80,+30)
produces snapping-adjusted actual displacements, approximately (+81.9143,+30),
(+80,+24.7001), (+80,+30). Raw pointer displacement is not asserted as an exact
transform or performance pairing. By the first drag Down, erase work had already
finished. The **erase-pending drag branch was not exercised**; no extra load or
reordered attempt was invented to hit it.

Both60-second pauses completed with no observed RAF, hover/resize trigger or GPU
write during the interval. Subsequent draw/drag worked at sampled points. This
is not another20-minute qualification and does not establish physical OS focus.

## Evidence limits

Actual root errors, `ok:false` input/frame results and critical-event ring loss
were zero. The diagnostic repeat-aggregation map dropped3682 telemetry records;
ordinary dropped0. Input/display/error and frame-summary rings remain complete
within their caps, but missing detailed preview/fence/coverage records cannot be
inferred from the aggregates. A successful result's static `INPUT_REJECTED` or
`FRAME_REJECTED` code field is not itself a rejection.

The ROIs are finite samples at move16/32, not continuous visible-frame proof or
first-photon measurement. Same-position opaque strokes overlap; settled black
pixel differences alone do not prove a new stroke. Distinct Operations, geometry,
object counts and amber while Down are combined. Target readback preserves
READ/DRAW/PBO/PACK state and checks matching context/target dimensions; it does not
call `gl.finish` or consume `getError`. Thus the GL error queue is unqualified.
No new full-scene CPU oracle or AA qualification is supplied.

The engineering Fixed40 fixture differs from the unavailable user22-object
content. A repeated erasure followed immediately by pending draw is now covered;
pending drag, a natural failing occurrence and the complete display-retirement
chain remain missing. No causal root or production repair is established.

## Changes, checks and finite ledger

Only new `live_erase_feedback_*` verification files, their tests, this document
and the new result/out namespace were written. All54 retained source files,
42ordinary+42diagnostic SDK files and old IF evidence retain their exact hashes.
The protected4331/4332/4333 listeners retain PIDs15544/40475/82528.
No production edit, SDK build, commit, push or cleanup was performed.
Four new Node guard/waveform/readback tests and three Python offline-analysis
checks pass. Existing IF tests are retained and reused by their exact identity;
there is no new native/GPU test run.

Two browser starts charge106 seconds each:212/720 seconds,2/3 starts,0 builds.
The unused corrective slot is not permission to search for failure. Both owned
browser/process families and HTTP servers were cleaned up. Exact references are
in `.aegis/results/G5-live-erase-input-feedback-r1/result.json` and its manifests.

## Minimal next proposal

Keep the defect open and preserve a natural failure from the user's document,
with contact/pending serial, mask revision, display identity and actual surface
pixels at that boundary. If separately authorized, isolate first drag while erase
is still pending before the long64-move draw drains it. Do not automatically repeat
idle, amplify load, add threads or patch preview retirement without causal evidence.
