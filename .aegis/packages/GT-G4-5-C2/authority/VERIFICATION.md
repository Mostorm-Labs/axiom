# P20 Brush Engine verification v0.4

Role: verification design. Authority: CONTRACT.md and architecture v0.4.
Objective: detect wrong creation, lost commits, replay drift and duplicate owners.
Non-goals: claiming executed evidence, P34 PASS or hardware timing guarantees.
Required analysis/output: failure modes, independent oracles, frozen acceptance.
Quality gate: all blocking obligations below have explicit observation and scope.
Handoff: P21/P23 and P31; tests below are P32 deliverables, not currently existing.

C2-V11: resolve examples and field mutations; observe full normalized snapshot
bytes. Absent fields match explicit defaults; zero/false preserved; -0==+0.
Change each semantic field to another valid value: snapshot bytes differ.
Metadata/key order/whitespace changes preserve bytes. Invalid/unknown fields,
versions, duplicates, nonfinite values, pressure policies and unavailable stages
reject before begin. Delete catalog after prepare; execution unchanged.
Failure: silent semantic drift. Existing old FNV identity is insufficient.
Evidence: resolution.json. Target: C2V11Resolution.

C2-PLAN: inspect activated tuples, node calls, allocation and resource reads.
Default/on/auto required nodes produce [1,2,3]; required off rejects. Neutral auto
inactive stages perform zero calls/loads/state allocations. Unsupported on and
arbitrary edges reject; input/output dependency closure is validated before begin.
Failure: wasted work or missing provider. Evidence: plan.json. Target: C2Plan.

C2-R1: run frozen R01 corpus; for subset representable by profile 1 (zero taper)
map options exactly, compare point/outline counts and order, coordinate error
<=1e-6 points and <=1e-5 outline, pressure <=1e-9. Preserve full independent R01
suite unchanged including unsupported taper cases as reference-only coverage.
Every prefix with last=false and final last=true is compared; chunk partitions
[1], [2], [7], [all] yield equivalent sealed output. Known limitations unchanged.
Failure: wrong node mapping or chunk dependence. External pinned fixtures detect
shared runtime mistakes. Evidence: r1.json. Target: C2VectorNode + existing
G45PerfectFreehandReference. Never regenerate expected output from code under test.

C2-V12: live create → AddStroke canonical bytes → fresh document with no catalog,
session or evaluator → decode/replay → snapshot encode/reload. Compare exact
BrushStrokeRecord, ObjectRecord and scene projection bytes. Undo/redo Restore
must recover same record and paint. Build a nonempty FramePlan, call actual
SkiaHeadlessBackend::submit(plan), require accepted and observation.has_value;
256x256 identity camera, sRGB RGBA8888 premultiplied, white background, AA on.
Same-host live/reload RGBA byte equality and derived digest equality required;
at least one nonwhite pixel; traversedKinds contains VectorStroke. Different
hosts are not required bit-identical: per-channel max error<=2 and changed pixel
fraction<=0.01 is corroborative only. Mutating saved outline/paint must change
render; missing new branch must fail rather than fallback to line renderer.
Failure: empty-image false pass or unrecoverable document. Evidence: replay.json
(including exact bytes and nonwhite pixel count). Target: C2V12Reconstruction.

C2-WIRE: examples and negative mutations at SemanticCodec.decodeProtobufOperation,
object validator, operation apply and snapshot bootstrap. Test (5,2)/tag10 versus
(5,1)/tag5, operation payload 1/2, snapshot 1/2; Insert/Restore mixed lists; old
reader from task-anchor revision rejects all new carriers without mutation.
Unknown node/profile, missing required false/zero, mixed branch/version, absent
device pressure, duplicate scalar fields and malformed counts reject. Sample
pressure absent versus zero are distinct canonical encodings. Store/ledger/scene
projections identical before/after every rejection. Exact duplicate op is no-op;
same ID/different bytes follows existing conflict semantics.
Failure: destructive forward fallback/non-atomic apply. Evidence: wire.json.
Target: C2WireCompatibility plus existing semantic conformance suite.

C2-PREDICT: confirmed sequences [1,2], predicted [3,4], then confirmed [3] with
empty prediction succeeds and removes old prediction. Prediction far away from
confirmed changes preview but not sealed bytes. Invalid batch is atomic. Cancel
leaves no intent; begin/config mutation cannot change frozen snapshot. Two
sessions with repeated pointer ID but different generation remain isolated.
Failure: prediction leakage or rejected real input. Evidence: prediction.json.
Target: C2Prediction.

C2-HANDOFF: stall presentation for 1000 revisions across 64 sessions; bounds
in architecture hold, consumed latest geometry equals full observer geometry.
Stall canonical consumer; all 64 seals retained exactly once; 65th begin BUSY.
Inject transient apply failure then retry same op, permanent failure, stale and
matching receipts, cancellation and surface loss. Verify no lost intent, no early
retirement, no synchronous GPU/semantic wait in append, no session reordering.
Failure: lost user stroke/deadlock. Evidence: handoff.json. Target: C2Handoff.

C2-PERF: run exact architecture 512/2048 workload; count evaluations AND history
copies, do not use only append count. Assert structural bounds and zero full-
history one-shot calls. p95 data is optional and cannot replace structural oracle.
Failure: O(n²) hot path or unbounded queue. Evidence: performance.json.
Target: C2Complexity.

C2-LEGACY: open frozen V1 vector/dab fixtures, compare decoded data and old
canonical rendering to task-anchor baseline; mutation/toolbar/undo/redo/autosave
commands disabled for V1-containing documents, originals remain byte-preserved.
Create a fresh document and draw: only (5,2), no old family writer reachable.
Failure: destroyed old work or covert legacy authoring. Evidence: legacy.json.
Target: C2LegacyReadonly.

C2-PLATFORM: compile common host, Web and Android bridges and Windows host using
accepted SDKs; test C0/C1 routed traces with shared common host observer. Grep
production paths for obsolete classes/family dispatch, inspect remaining uses
(reference-only/history are allowed). Each platform emits new snapshot/output
from common controller. All three builds required; physical device runs optional.
Failure: uncompiled migration or platform duplicate engine. Evidence: platforms.json
with compiler/target/command/exit status; target C2PlatformOwnership + platform
builds. Provider/location optional, successful target compilation is not optional.

C2-REGRESSION: run existing C0/C1/ink routing, semantic, scene, render/headless and
R01 tests; replace ONLY assertions tied to explicitly retired authoring behavior.
Do not delete failed V1 read/operation/input tests. git diff --check and mutation
scope validation required. Evidence: regressions.json. Target: existing CTest suite.

All eleven artifacts above are blocking: each closes the named otherwise
uncovered failure mode; JSON parsing/hashes alone cover none of runtime semantics.
RED precondition for V11/V12/platform ownership: before production mutation,
run the new behavior probes against task-anchor code through explicit legacy
adapter; observe missing snapshot roundtrip/new wire rejection/duplicate owners.
Missing test binary alone is not behavioral RED. Preserve raw command/output.
Tests can be implemented during P32; acceptance and expected values are frozen
here. Evidence includes input fixture hashes, result commit, executable hash,
command, exit code and observations; summaries cannot substitute for raw results.
