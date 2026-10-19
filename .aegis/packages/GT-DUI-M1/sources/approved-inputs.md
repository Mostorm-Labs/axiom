# GT-DUI-M1 approved control-plane input projection

Revision: dui-m1-control-2026-10-10-v1.
This is a task-scoped, explicitly condensed execution projection of the reviewed
Notion inputs, not a verbatim export or a hash of an entire remote Notion page.
Its exact UTF-8 bytes are bound by authority.lock.json. Upstream provenance:

- Debug UI Contract v0.1, sections 22.2-22.3 and 23:
  https://app.notion.com/p/3eb4c57a590c81a1a53ee2a673f78c13
- DUI-M1 Implementation Design & Plan v0.1, sections 1-9:
  https://app.notion.com/p/3f54c57a590c8109bdaed4754d9accaa
  Retrieved after targeted review at page version observed 2026-10-10T03:24:07.005Z.
- Public Runtime Facade v0.2 and the narrow Facade Closure remain superior
  ownership constraints; their relevant rules are materialized below, not
  permission to implement the whole future SDK:
  https://app.notion.com/p/3c84c57a590c81b7a5d8d15997502eaf
  https://app.notion.com/p/3f44c57a590c813e9546e6cfbc9f27ac
- User control instruction on 2026-10-10 approves the two-track continuation.
  Background is syncable document data; implementing the whole synchronization
  workflow now is NOT required. This corrects the earlier pre-G5 network demand.

## Authorized delivery
M1-01: choose among already mounted target views; truthful, revisioned state and
full target identity. No new window-management product is required.
M1-02: Ink, Select, Eraser and Pan through the existing interaction owners.
M1-03: available brush preset catalog, exact profile/revision and resource errors.
Brush styles are presets, not additional platform-specific ToolKinds.
M1-04: ordinary ink size/color/opacity and eraser mode/diameter are production
controls. They are not gated on experimental BrushAuthoring.
M1-05: pan, anchored and numeric zoom, 50/100/200 percent presets, Fit Content,
Selection, Object and finite WorldRect using the accepted camera/Fit policies.
M1-06: existing Undo/Redo, actual availability and structured request outcomes.
M1-07: build-generated Runtime version or development label, commit/dirty state,
platform/configuration; API/schema/brush/Skia/text versions remain distinct.
Windows, Web and Android must use the same product semantics and actual common
control route. Declarations, mock values and mandatory Unsupported rows are not
code completion. Physical observations remain separately recorded.

## Ownership and safety
RuntimeFacade::submitProductControl is the sole product intent lane. Extend it
with typed value payloads, not arbitrary string setters or mutable pointers.
Value types live in runtime/foundation; the small runtime/control implementation
may depend on foundation, input, interaction and ink, never the inverse.
Platform wrappers schedule the owner and publish results; they do not reimplement
preset interpretation, tool state, semantic mutation or camera authority.
Canonical mutation remains Operation-only. Control selection/options/camera are
view/editor-local and create no canonical Operation or history entry. Drawing
and Undo/Redo continue through their existing canonical owner.
Input samples do not travel through ImGui or the ordinary JS control/event lane.
SemanticGeneration is not a handle-lifetime token. Validate runtimeEpoch, viewId,
viewEpoch and attachmentEpoch at admission and again before applying a request.
Full ObjectId is 128 bits. Never truncate it for Fit Object or selection identity.

## Typed state and application policy
Use CanvasTargetKey, CanvasToolKind, BrushPresetRef and tagged OptionPatch<T>
(Keep, Set(value), ClearOverride). InkOptionsPatch has size, color and opacity.
EraserOptionsPatch has optional mode and diameterLogicalPx. CanvasControlPayload
contains SelectTool, SelectPreset, InkOptionsPatch, EraserOptionsPatch, PanBy,
ZoomAt, SetZoom, Fit, Undo and Redo. Fit carries a full object or finite rect.
An absent SetZoom anchor means logical viewport center. Positive PanBy delta
moves content in that logical direction; translate legacy wheel signs once.
Snapshot includes the target, controlRevision, effective tool/options, preset,
viewport/camera facts, selection summary, capabilities and last receipt.
Enqueue is bounded (default FIFO capacity 256); owner-safe-point application and
immutable snapshot publication are separate. No UI call waits for rendering,
Present, filesystem or network. Refresh snapshots after real input/history/view
changes too. Multiple clients use an owner-allocated request sequence or an
explicit client+request identity, never colliding local counters.
Receipt storage is bounded; old requests return Expired and are never executed
again. Queued is not Applied; Applied is not Presented, Durable or Synced.
Errors distinguish InvalidValue, Busy, UnknownPreset, ResourceUnavailable,
StaleTarget, PermissionDenied, QueueFull, Unsupported and InternalFailure.
Pending requests terminate on target destruction; stale replies cannot alter the
currently selected UI target.

Tool/mode/preset switching requires quiescence; Busy changes nothing. Validate and
load a preset before atomically switching tool and preset. Preserve explicit
user overrides across preset changes; ClearOverride restores preset defaults.
Default size/color/opacity changes apply only to sessions starting afterwards;
active sessions and committed objects retain their captured configuration.
Eraser diameter is captured by each new eraser session. A mixed mode+diameter
request is wholly rejected on Busy. Size uses existing BrushPackage content
units; eraser diameter uses logical view pixels with camera conversion, not DPR.
Finite positive size/diameter, finite RGBA/opacity in [0,1], and representable
conversions are required. Zero opacity is valid, not a clear-override sentinel.
Color is straight normalized RGBA; color alpha and independent opacity each
contribute once. Preview, canonical and replay consume the same captured state.
Retain vector AND dab paths. Per-contour presentation paint may be extended;
canonical BrushExecutionSnapshot already captures the ordinary options.
Diagnostic tint is explicit and separate; normal controls show actual ink color.
Pan follows common capture/session routing and never creates a stroke.
Camera changes preserve shared Scene/canonical state and existing gesture policy;
no implicit cancellation of active ink to make a camera command easier.

## Explicit non-goals and successor boundary
No background schema/rendering, PNG exporter, object inspector/list, network
Sync, complete Data Runtime, Brush Studio, TextEdit/IME, G5 optimizer, new public
C ABI, dependency SDK rebuilding, or historical evidence/Gate rewriting.
Background line/color remain shared, no-Undo, export-included document semantics
for M2. M2 implements schema/Operation/snapshot/replay/history/publication seams;
real transport and full persistence/sync infrastructure stay with G7/G8 and are
not a blocker for M1 or the pre-G5 baseline. Do not downgrade background to UI-only.
M1 execution may start from the reviewed development anchor before combined
physical qualification; this is task-specific approval, not G4.7 Gate PASS.
Return code/automation and runnable three-platform bundles to CONTROL_REVIEW.
Do not wait for human physical testing in P32 or claim physical/G4/G5 PASS.
