# UR1d result: BLOCKED

The frozen subject reached an explicit implementation blocker. U1D-04 physical
context loss is conservatively classified as unknown abandonment in actual WebGL.
No source, driver, SDK, profile, resource or frozen predicate was changed after
sampling; all four authorized workers are closed and no retry is available.

## Actual defect

`WebGlSurfaceProvider::Impl::service()` queries context attributes before checking
loss. The pinned Emscripten implementation returns INVALID_TARGET both for a
deleted registry handle and for a live lost context whose getContextAttributes()
returns null. Both real canonical and preview loss consequently emit
unknown-abandon / physical_loss=false. The unknown path stays fail-closed; this
does not fabricate successful GPU completion, but cannot satisfy frozen U1D-04
and may retain unresolved charges until owner close if loss overlaps GPU work.
The proposal to check live registry separately is unimplemented in this subject.

## Completed implementation and proof

Existing renderer/cache/source/budget owners remain. Actual GPU submit outcome is
resolved before provider present; early-return scopes fence offscreen work.
Navigation retains feedback before cold source query and opens a subsequent
recording segment without another frame allowance. Queue service coalesces each
qualified retained completion-state lifetime per service pass, is non-reentrant,
and runs before admission. Later observed epochs wait for ordered retirement;
staging shrink and logical eviction do not prematurely grant quota. Unknown
abandon, physical-loss cleanup, unused work, submit failure, display invalidation
and owner close have distinct observations. Callbacks retain quota/state only.

Existing resource-service forwarding carries resource-only idle retry and a
private surface lifecycle hint. Resize/loss/rebind clear device pixels while
valid world-body jobs retain progress. Explicit owner clear still purges source
metadata. Temporary navigation acquire failure preserves the existing owner.
No Input/Operation/History/Scene/Brush/ARC/UR2 semantic path was changed.

56 frozen native tests pass against the final build, including DPR1/2/4 actual
Metal and the C2 original 4263 one-step calls / 513 builds / legacy compile 0.
Existing 36 G5 Node + 4 new + 15 SDK controller/transport checks pass (55);
21 existing Python + 5 new pass (26); 120 boundary contracts pass.
Behavioral REDs retain repeated polling, premature shrink, actual failed submit
after success, and navigation-owner discard on temporary acquire denial. One
incorrect denial-fixture setup is preserved as invalid setup, not a mechanism RED.

Core qualification and normal workers are OBSERVED; recovery normal is OBSERVED;
recovery qualification is PRODUCT_FAILURE_OBSERVED solely at physical-loss facts.
All 40 fresh cold captures and 8 roundtrips qualify against the unchanged original
CPU/null-cache oracle and accepted AA boundary; snapshots, canonical identity,
resource binding and current presentation recover without semantic mutation.
Both qualified recovery documents end at 91 admitted / 91 ordered retired,
queued 0, needs_service false; no critical observation loss. No claim is made for
loss overlapping a still-held real Web GPU epoch beyond the separate native proof.

## Accounting, packaging and limits

Four starts charge ceil(actual): 49 + 48 + 16 + 14 = 127 seconds. Historical
54 starts/2107 seconds remains unchanged: cumulative 58/2234, cap 8370, remaining
6136 seconds; no remaining worker IDs. All guards complete normally and owned
process families clean up. Ordinary SDK is diagnostics/timing OFF; private
qualification engine is ON, with the consumer install guard retained. Both have
36 matching facade/resource files, developer components OFF. Defaults remain
32/96 MiB, 8 MiB/512 entries, aggregate allowance, 1024 epoch cap and nav2 guard.

Fresh functional normal S2 erase phase records ~277.9ms content gap and ~277.2ms
maximum handler. This is unpaired observation, not an improvement or SLO PASS.
Exact Up-to-matching-CanonicalVisible timestamp remains null. Web proof is DPR2;
native proof covers DPR1/2/4. Overall UR1/G5/P34/performance PASS is not issued.

Normal Demo: http://127.0.0.1:4288/example.html (retained). No UR1d commit/push;
UR2 and old artifacts/ports are preserved. Repair/reverification requires a new
control package/window; old rows cannot be relabeled as evidence for a repair.
The immediate blocked notice and its successful control delivery remain intact.

CTest show-only refreshes its Temporary/LastTest.log; full 56-test status and the
working-set raw extract were already retained. The final two resource executables
were additionally captured directly, unchanged source/binaries, to keep durable raw
transition output. This adds no browser/driver/native implementation change.
Initial archive assembly accidentally included its still-growing stdout log; final
materialization excludes that log and preserves the initial archives separately.
