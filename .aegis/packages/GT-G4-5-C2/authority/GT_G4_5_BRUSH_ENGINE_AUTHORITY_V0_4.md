# G4.5 Brush Engine v0.4 — Current scoped design

Role: P14/P16 architecture. Authority: user-approved replacement of all former
brush authoring/runtime design, with V1 read-only compatibility (2026-09-24).
Objective: one configured common engine from Ink routing to preview and commit.
Non-goals: Dab/Shape/Grain/WetMix execution, ZIP importer, arbitrary DAG editor,
D0 UI framework, Arc backend selection or general renderer redesign.
Required analysis/output: ownership, state transitions, bounded handoff,
retirement map and P31 scope below. Quality gate: P20 verification design.
Handoff: P20 → P21/P23 → P30/P31; this turn stops before P32.

Semantic authority is [the complete P12/P13 contract](brush-engine-v0.4/CONTRACT.md)
and its schema/examples. This document does not override its fields/versions.

## Owners and data flow

BrushPackage + versioned defaults + overrides → immutable ResolvedBrushState →
compiled BrushExecutionPlan → BrushSession → BrushPreviewDelta / BrushCommitIntent.
Common InkPlayground composition owns the engine, plan catalog and session map.
Platforms normalize input and present output. C1 → C0 → Brush runs ordered on one
logical session executor through BrushPreviewDeltaReady with no mandatory queue
hop. Arc receives typed output after that point. It never selects brush options.
Runtime Core owns AddStroke construction and SemanticDocument mutation.
Render Core consumes stored canonical outline; bounds/hit geometry also derives
from outline. Minimal support for new content is required in scene and Skia,
including submit(FramePlan)/observation(); cache/tile/scheduler redesign is excluded.

Perfect Freehand is only versioned node (2,1), with the precise adapter mapping
in CONTRACT.md. Old runtime class names/API and family dispatch need not survive.
R01 source/fixture/license assets remain reference assets, not a second engine.

## Lifecycle and failures

prepare resolves/compiles once outside append, validates schema/resources and
reserves session/commit capacity. begin freezes snapshot; no external catalog or
caller-owned mutable plan pointer may be observed afterward. Configuration edits
apply to next stroke. begin fails without creating preview if capacity exhausted.

append takes a confirmed ordered batch plus a complete replacement predicted
suffix. Confirmed sequences strictly increase; predicted sequence numbers are
local to prediction, never constrain later confirmed samples. Validate whole
batch before state mutation. Every new batch replaces prior prediction, even
when its predicted suffix is empty. Clone/checkpoint confirmed evaluator state;
prediction never advances confirmed state or seed. Invalid batch leaves revision,
confirmed state and preview unchanged. Concurrent appends serialize by session.

seal uses only confirmed samples and last=true, drops prediction, freezes final
outline and retains final preview. Empty strokes cancel with no operation.
One immutable intent per session is retained until acknowledged by Runtime Core;
retry reuses operation ID/bytes. Transient failure keeps intent and preview.
Permanent failure is surfaced, with explicit discard required; never silently
retire or report success. cancel is valid before seal, produces reliable cancel
control and zero intent. After successful canonical apply, undo is an Operation.

CanonicalVisible receipt matches document epoch, stroke ID, session generation,
operation ID and canonical commit revision. Late, duplicate or mismatched receipt
is ignored. A matching receipt retires only that preview. Session reuse increments
generation; revision counter alone is insufficient. Surface loss cancels unsealed
sessions; sealed intents remain owned by Runtime Core and are not discarded.

## Preview stability and performance contract

A sample count is not an outline stable-prefix count. Node declares stable
geometry by segment/side identity and revision. Deltas carry base_revision,
new_revision, immutable prefix references and a replaced geometry suffix (or
whole geometry while unstable). Consumer detects a missed base and receives a
current immutable snapshot; never applies delta to the wrong revision.

R1 first-ten-point pressure bootstrap, two-point synthetic expansion, end-noise
and seal finalization can invalidate earlier geometry. Before stability is proven,
report zero stable prefix. No fixed tail length is asserted for arbitrary data.
Allow one final O(n) reference evaluation at seal. append uses incremental state,
never calls full-history one-shot evaluator. Structural performance gate is only
for the frozen well-spaced workload below, not a universal O(1) promise.

Session memory O(n) for canonical samples/output is allowed. Preview mailbox has
one replaceable latest immutable geometry view per active session (max 64), plus
one reserved terminal control slot per session. Queue *revision count* is bounded;
geometry bytes still scale with stroke length and must not be copied on every
move. Up to 64 admitted/unacknowledged sessions, one reserved commit slot each.
New begin rejects BUSY when no slot; existing seal/terminal cannot be dropped.
Latest-wins coalesces complete views or rebased deltas, not unrebased patches.
No GPU/canonical/persistence wait on append. Tail buffers and counters expose
actual evaluations, copied historical scalars, pending views and retained bytes.

Performance fixture: confirmed samples (x=8*i, y=0, pressure=0.5), device source,
size=16, thinning=smoothing=streamline=0.5, zero taper, batch size 1, n=512/2048.
After first 16 samples, max evaluated/copied historical scalar work for 2048 is
<=1.25 times 512; full-history one-shot calls during append=0; pending views<=64.
Measure seal separately. <=1 ms p95 is corroborative on named hardware only.

## Required retirement at P32 completion

| Existing surface | Required final disposition |
| --- | --- |
| programmable_brush.hpp/.cpp, BrushDefinition/Compiler/Program/Runtime, BrushFamily | Remove authoring classes/dispatch and product references; replace with new package/plan/session API |
| brush_engine.hpp/.cpp stub and InkEngine brush shortcut | Remove stub; retain input/session routing through common engine adapter |
| canonical_brush_commit.hpp/.cpp | Replace old descriptor synthesis with new immutable intent→AddStroke adapter |
| Web/Android bridge BrushRuntime/session tables | Remove; invoke common composition after C0 routing |
| Windows raw pointer→preview geometry | Remove; consume common typed preview |
| skia_brush_renderer fake shape/grain ID rendering | Remove new-authoring consumers; draw new outline with snapshot paint |
| Brush Lab, toolbar family selection, old seven brush presets | Replace with actual JSON Vector package; unavailable capabilities not presented as working presets |
| old family/primitive tests | Replace authoring assertions; preserve applicable input and read-only V1 data regressions |
| semantic V1 decoder/validator and canonical renderer | Preserve existing read-only document behavior, no new V1 writer UI |
| R01 source/fixtures/license; C0/C1 routing | Preserve; R01 only node adapter/oracle, C0/C1 behavior unchanged |
| old design/package docs and review records | Historical only, linked by supersession; no old design constrains new authoring |

Read-only compatibility is permanent for this scope: do not schedule deletion of
V1 decode/render just because new pipeline tests pass. No runtime compatibility
facade should recreate old authoring APIs. Temporary migration wrappers must be
removed before package completion. No production changes occur at P31.
