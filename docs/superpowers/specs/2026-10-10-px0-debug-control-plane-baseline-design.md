# PX0 Debug Control Plane Baseline Design

Date: 2026-10-10  
Status: Approved / written-spec review passed  
Stage family: Aegis P14-P18 architecture design  
Repository baseline: `Mostorm-Labs/axiom@9760a8ad910aaa1d1232c9cf24723e4d311d9816` (`codex/g4-7-development`)  
Scope: Pre-G5 Debug Control Plane baseline for the pre-productization track

## 1. Purpose

PX0 establishes a long-lived Axiom engineering control plane before PX1 Shape, PX2 RichText editing, PX3 Shape+Text composition, PX4 Connector, PX5 Snap, PX6 Image, PX7 Product UI Projection, PX8 Mixed Whiteboard Closure, and G5 performance work continue.

The goal is not to make the current Debug UI cosmetically richer. The goal is to turn the existing common ImGui + Skia Debug UI, RuntimeFacade, diagnostics providers, debug controls, input-capture gate, Surface controls, and telemetry into a stable engineering workbench with explicit ownership and extension contracts.

PX0 MUST make later capabilities additive:

```text
feature runtime behavior
    -> product-safe RuntimeFacade contract
    -> diagnostics projection
    -> Debug UI contribution
    -> scenario / evidence
```

A later feature MUST NOT require redesigning the Debug UI top-level information architecture, platform host, snapshot ownership, or command-routing model.

## 2. Authority and implementation-reality basis

This document is a proposed design authority for the PX0 scope only. It does not supersede Axiom semantic, Operation, RuntimeScene, Arc, Product Shell, Platform Host, Surface, or public Runtime ABI authority.

Current upstream authority preserved by this design includes:

- `SemanticDocument` remains the sole canonical Canvas truth.
- Operation remains the only canonical mutation unit.
- RuntimeScene, selection, viewport, Arc preview, Surface state, caches, and GPU resources remain derived/non-canonical.
- Product Shell drives Axiom through public/product-safe contracts and does not access RuntimeScene, SpatialIndex, Tile, Skia, or raw GPU ownership.
- Platform Host remains the composition root without regaining subsystem semantic authority.
- Arc owns native low-latency input acquisition / transient preview presentation; Axiom owns interaction semantics including connector and snap decisions.
- Stable Runtime C ABI is not changed by PX0 unless separately authorized.

Implementation reality at the baseline revision includes:

- `RuntimeFacade : RuntimeDiagnostics`.
- a flat `RuntimeStateSnapshot`;
- a flat `DebugSnapshot` plus global `Capability[]`;
- separate `IAxiomDiagnostics`, `IArcDiagnostics`, `IPlatformDiagnostics`, and `ITelemetry`;
- `AxiomDebugControl` and `PlatformDebugControl`;
- a generic `debug_ui::DebugCommand` / `BoundedCommandQueue`;
- two overlapping panel abstractions: `PanelCapability` and `DebugPanel`;
- a common ImGui + Skia renderer;
- Windows and Web Debug UI hosts;
- platform-specific DebugSnapshot assembly;
- product controls for brush, eraser, selection mode, camera, Undo, and Redo;
- Windows/Web application-level temporary tool controls in addition to Debug UI controls.

Repository code is implementation reality, not independent design authority. PX0 uses that reality as the migration starting point while preserving current upstream authority.

## 3. Success criteria

PX0 succeeds when the Debug UI becomes a stable workbench that:

1. preserves all currently useful Brush/Eraser, selection, history, Surface, Arc, telemetry, and diagnostics behavior;
2. routes product behavior only through product-safe RuntimeFacade contracts;
3. routes engineering-only mutation only through explicit debug-control owners;
4. exposes internal diagnostics through read-only provider contracts;
5. presents all panels from one bounded immutable structured snapshot;
6. assembles snapshots in common code rather than Windows/Web host code;
7. separates Debug UI local presentation state from Runtime state;
8. allows a feature to contribute Control and Inspect content without changing the workbench core;
9. keeps Windows/Web platform hosts thin and presentation-focused;
10. provides deterministic contract tests plus Windows/Web physical validation without entering Shape, RichText-editing, Connector, Snap, Image, or Product UI implementation.

## 4. Non-goals

PX0 does not implement:

- Shape authoring;
- TextEditSession, caret/selection editing, IME composition, or RichText authoring;
- Connector semantics or routing;
- Snap semantics;
- Image import/product placement;
- final Product Shell toolbar/context UI;
- ImGui docking or a multi-window debugger;
- a full RuntimeScene tree browser;
- raw SemanticDocument editing;
- a full Operation timeline;
- a GPU profiler;
- a full Brush parameter laboratory;
- Android/iOS Debug UI physical host parity as a PX0 blocking criterion;
- changes to canonical document semantics or stable public Runtime C ABI.

The workbench MUST preserve extension slots for these future capabilities without implementing them.

## 5. Layer 1 — Workbench information architecture

PX0 freezes six top-level workspaces:

```text
Dashboard
Control
Inspect
Runtime
Performance
Scenarios
```

Later feature work MUST NOT add new top-level workspaces without a new architecture decision. Feature growth is absorbed through stable workspace slots.

### 5.1 Dashboard

Dashboard answers: "Is the current Runtime in the state I expect?"

It contains bounded summaries of:

- Runtime/document/view/surface identity;
- current product state;
- subsystem health;
- core live performance;
- recent Debug Control Plane activity.

It is not a raw diagnostic dump.

### 5.2 Control

Control answers: "What product behavior do I want the Runtime to perform?"

Stable sections are:

```text
Tools
Ink
View
History
Feature
```

The Feature section is context-sensitive and is the primary extension slot for Shape, RichText, Connector, Snap, Image, and later product capabilities.

Tool identity and feature preset identity MUST remain distinct. For example, Pen is a tool while Vector Solid / Marker Flat / Chalk Grain / Membrane are Ink selections. Eraser is a tool while Object / Partial are eraser modes.

### 5.3 Inspect

Inspect answers: "What object, interaction, or relationship does the Runtime currently report?"

Stable sections are:

```text
Selection
Object
Interaction
Relationship
```

Inspect is read-only with respect to Runtime internals. Any user-triggered modification from an Inspector control is still a product or debug intent routed through the correct owner API.

### 5.4 Runtime

Runtime hosts engineering state grouped by owner/domain rather than by feature UI:

```text
Document
Scene
ARC
Render
Surface
Resources
Feature Diagnostics
Advanced Actions
```

Advanced Actions contains engineering-only Runtime controls such as force redraw, scene recompile, cache eviction, background-raster pause, and memory-budget experiments when supported. Metrics-reset controls are presented under Performance / Metrics while still routing to their engineering owner.

### 5.5 Performance

Performance hosts:

```text
Live Metrics
Input / ARC
Rendering
Memory & Cache
Trace
GPU Timing
Feature Metrics
```

Unavailable capabilities remain visible as unavailable/unsupported engineering capability, rather than disappearing or being misrepresented as implemented.

### 5.6 Scenarios

Scenarios is the stable qualification/evidence workspace.

PX0 establishes the workspace, registration contract, and bounded scenario runner. It MUST NOT create a canonical-state bypass merely to populate the UI.

At PX0, scenario entries may invoke only already-authorized public/test-fixture boundaries. "Reset", "Empty Canvas", and "Ink Baseline" are permitted only when they can be implemented through such sanctioned boundaries. If baseline reconciliation finds no safe existing boundary, the entry is shown as unsupported rather than introducing a direct Document/Scene mutation path.

Future work may register PX1 Shape Grid, PX2 RichText Samples, PX3 Shape+Text, PX4 Connector Network, PX5 Snap Playground, PX6 Image Board, PX8 Mixed Whiteboard, and G5 stress workloads.

## 5.7 PX0 control-surface contract

PX0 makes the control inventory explicit so later work does not confuse "a value is visible" with "a value is safely mutable".

Every control is classified as one of:

- **Product control** — a user-visible behavior that must route through RuntimeFacade or a typed product capability owned by the Runtime product control plane.
- **Engineering control** — a debug-only mutation that must route through AxiomDebugControl, PlatformDebugControl, or another explicit engineering owner.
- **UI-local control** — navigation/filter/presentation state owned only by DebugUiSessionState.
- **Successor feature control** — a reserved extension slot. PX0 may display Unsupported/Not Implemented, but MUST NOT invent the feature's product semantics.

### 5.7.1 Control-state presentation

Every mutable control MUST have an explicit UI state:

```text
Available
DisabledByContext
Pending
Applied
Rejected/Failed
Unsupported
Unavailable
StaleGeneration
Expired/QueueFull        # when the owner contract can report these states
```

The Debug UI may normalize these for presentation, but the raw owner receipt/state remains inspectable.

Rules:

1. a control is **Unsupported** when the owner/build does not implement it;
2. a control is **Unavailable** when it is supported but cannot currently execute;
3. a control is **DisabledByContext** when product context makes the action invalid, such as Redo with no redo entry or Fit Selection with no selection;
4. a queued action is **Pending** until the owning contract reports a terminal state;
5. stale-generation/expired/queue-full failures are displayed explicitly and MUST NOT silently retry through a bypass path;
6. persistent settings and modes MUST be read back from Runtime/diagnostics after submission; local widget state is never treated as canonical truth;
7. one-shot engineering actions may be represented by receipt/activity without a persistent readback value;
8. controls MUST NOT become enabled merely because a Panel can construct a request. Availability comes from owner capability/context.

### 5.7.2 Product control baseline

The following product-safe controls are part of the PX0 workbench baseline.

| Area | Control | Runtime owner/path | PX0 behavior |
| --- | --- | --- | --- |
| Tool | active tool selection | RuntimeFacade product control | show the currently resolved tool model; only expose tool choices actually supported by the Runtime |
| Ink | Vector Solid | RuntimeFacade::setBrush | preserve existing brush choice |
| Ink | Marker Flat | RuntimeFacade::setBrush | preserve existing brush choice |
| Ink | Chalk Grain | RuntimeFacade::setBrush | preserve existing brush choice |
| Ink | Membrane | RuntimeFacade::setBrush | preserve existing brush choice |
| Ink | brush revision | RuntimeFacade::setBrush + RuntimeState readback | always display resolved revision; only make revision selectable when the Runtime intentionally exposes more than one supported revision |
| Ink | size / opacity / other brush parameters | future product-safe brush parameter contract | do not create a DebugControl-only mutation; show Unsupported until a product-safe contract exists |
| Eraser | Object Eraser | RuntimeFacade::setEraser | preserve existing mode |
| Eraser | Partial Eraser | RuntimeFacade::setEraser | preserve existing mode |
| Selection | selection mode on/off | RuntimeFacade::setSelectionMode | preserve current selection-mode behavior and read back the resolved mode |
| History | Undo | RuntimeFacade::undo | enabled only when `canUndo` |
| History | Redo | RuntimeFacade::redo | enabled only when `canRedo` |
| View | Pan | RuntimeFacade camera control | relative camera translation through the existing product camera path |
| View | Zoom At | RuntimeFacade camera control | zoom by scale delta around an explicit/derived viewport anchor |
| View | Fit Content | RuntimeFacade camera control | fit the current document/content bounds |
| View | Fit Selection | RuntimeFacade camera control | enabled only when a valid selection exists |
| View | Fit Primary Object | RuntimeFacade camera control | uses the current primary selection/object identity when valid |
| Future Feature | Shape/Text/Connector/Snap/Image controls | typed successor product capability | reserved extension slot only; not implemented by PX0 |

PX0 MUST NOT make brush preset changes retroactively mutate already-committed strokes unless that behavior is separately part of the product contract.

Selection control in PX0 is limited to product-safe selection mode/state plus camera fit actions. Existing direct-on-canvas selection/transform interaction remains exercised through the real Canvas interaction path; PX0 does not duplicate it as ad hoc button/property mutation when no corresponding product-safe control contract exists.

### 5.7.3 View / Camera control baseline

View/Camera receives an explicit contract because it is both a real product behavior and an important engineering diagnostic surface.

The Control / View panel MUST expose or display:

```text
Camera State
- scale
- translation X
- translation Y
- view generation
- camera generation when diagnostics provides it

Camera Commands
- Pan
- Zoom At
- Fit Content
- Fit Selection
- Fit Primary Object
```

#### Pan

Pan is a relative camera operation.

The Debug UI may offer directional nudge buttons and/or delta-X / delta-Y inputs, but it MUST submit the same product camera operation used by other product surfaces. It MUST NOT directly edit viewport internals.

Absolute X/Y text editing is not part of PX0 unless a separately authorized absolute-camera product contract already exists at PX0-00 reconciliation.

#### Zoom At

Zoom uses a scale delta and an anchor.

The panel MUST expose the resolved scale and the anchor used for a submitted Zoom At action. A convenience zoom-in/zoom-out control may choose the current viewport center only when that center is available from the common host/view contract; otherwise the panel uses explicit anchor input.

The Debug UI MUST NOT invent a platform-specific zoom rule that differs from Canvas interaction behavior.

#### Fit Content

Fit Content uses the existing RuntimeFacade fit-to-content path and is distinct from any future "Reset View" command.

#### Fit Selection

Fit Selection is disabled when there is no valid selection. It MUST NOT silently fall back to Fit Content.

#### Fit Primary Object

Fit Primary Object uses the currently projected primary selected object when one exists. It is disabled otherwise. PX0 does not require a raw editable ObjectId field.

#### Reset View / 100%

PX0 does **not** define Reset View as a product semantic.

In particular:

```text
Reset View != Fit Content
100% zoom != automatically Fit Content
```

If the product later requires "100% at default center" or another canonical reset-camera behavior, it receives an explicit product contract rather than being inferred inside Debug UI.

### 5.7.4 Surface engineering controls

Surface mode is an engineering control and does not belong to Product UI semantics.

Required PX0 Surface controls:

| Control | Owner | Required presentation |
| --- | --- | --- |
| Platform Default | PlatformDebugControl | request + resolved mode + generation + receipt |
| CPU Reference | PlatformDebugControl | request + resolved mode + generation + receipt |
| GPU Default | PlatformDebugControl | request + resolved mode + generation + receipt |

The canonical Canvas Surface is the required PX0 target because it is already used by qualification. A separate Debug Controller Surface target may be surfaced when the platform provider genuinely supports it, but is not a PX0 closure requirement.

Surface panel state includes, when available:

```text
requested mode
resolved mode
canonical surface generation
preview surface generation
width / height
device pixel ratio
surface available
present count
lost count
last control receipt
```

A stale generation, queue-full, expired, unavailable, unsupported, or failed Surface request is surfaced directly; the UI MUST NOT apply an alternate platform shortcut.

### 5.7.5 Runtime engineering controls

The current AxiomDebugControl command family is represented explicitly under Runtime / Advanced Actions or, for metric-specific actions, Performance / Metrics.

| Control | UI location | Interaction model |
| --- | --- | --- |
| Set Overlay Flags | Runtime / Advanced Actions | persistent engineering setting; enable only when named/defined overlay semantics and owner support are available |
| Force Full Redraw | Runtime / Advanced Actions | one-shot action |
| Force Scene Recompile | Runtime / Advanced Actions | one-shot action |
| Evict Tile Cache | Runtime / Advanced Actions | one-shot action; Unsupported is valid before G5 cache implementation |
| Evict Raster Cache | Runtime / Advanced Actions | one-shot action; Unsupported is valid before G5 cache implementation |
| Pause Background Raster | Runtime / Advanced Actions | persistent mode only when readback/diagnostics can report the resolved state; otherwise use explicit command/result presentation rather than a misleading local toggle |
| Runtime Memory Budget | Runtime / Advanced Actions | numeric engineering input only when the owner exposes support and unit/range semantics |
| Reset Rolling Metrics | Performance / Metrics | one-shot engineering action; does not reset canonical/runtime product state |

PX0 does not invent overlay-flag names, memory units/ranges, or cache semantics that are absent from Current Authority/implementation contract. Unsupported controls remain visible as such where useful for engineering discoverability.

### 5.7.6 Performance and telemetry controls

Performance data is primarily read-only. PX0 control behavior is deliberately narrow.

Allowed controls include:

```text
Reset Rolling Metrics        -> engineering owner
Metric view/filter selection -> DebugUiSessionState only
Activity/trace filtering     -> DebugUiSessionState only
```

Trace and GPU Timing remain explicit capability sections. If they are not implemented, they render Unsupported. PX0 MUST NOT add a fake Enable Trace or GPU Timing toggle without an owner contract.

### 5.7.7 Scenario controls

Scenarios may expose:

```text
Select Scenario
Run
Cancel                    # when the runner is asynchronous
Reset to sanctioned fixture/baseline
Capture/Export evidence   # only through an already-authorized evidence path
Status / last result
```

Scenario execution MUST use RuntimeFacade, real input/interaction paths, or an already-authorized test-fixture boundary. It MUST NOT write SemanticDocument, RuntimeScene, Selection, or renderer state directly.

If Reset Canvas / Empty Canvas / Ink Baseline cannot be expressed through an authorized existing boundary at PX0-00, those controls are marked Unsupported instead of introducing a new semantic bypass.

Evidence export remains governed by the existing Gate/evidence contracts; PX0 does not redefine an evidence artifact merely because Scenarios provides a button.

### 5.7.8 UI-local and host-presentation controls

The following controls are non-Runtime presentation state. Most belong to DebugUiSessionState; Debug UI show/hide may remain owned by the platform host because it controls overlay/canvas presentation. None of them reach Runtime semantic owners:

```text
workspace navigation
section expand/collapse
search/filter
metric view selection
activity auto-scroll
local activity-filter selection
workbench show/hide presentation state (host/session presentation only)
```

A local "clear visible activity" action, if provided, only clears the Debug UI's bounded presentation log; it does not clear canonical history, telemetry owned by Runtime, or evidence artifacts.

### 5.7.9 Control/readback rule

For any persistent product or engineering mode, the Debug UI follows:

```text
user intent
-> owner request
-> receipt
-> new owner/runtime snapshot
-> UI reflects resolved state
```

It MUST NOT use:

```text
user intent
-> mutate local checkbox/value
-> assume Runtime matched it
```

This rule applies especially to Brush/Eraser mode, Selection mode, Camera, Surface mode, background-raster pause, and future Shape/RichText/Connector property controls.

## 6. Existing capability migration

Existing engineering capability is migrated rather than discarded.

| Existing capability | PX0 destination |
| --- | --- |
| Overview capability/status list | Dashboard / Health |
| Input status | Inspect / Interaction and Performance / Input |
| Canvas status | Runtime / Document + Scene |
| Arc Preview status | Runtime / ARC |
| Surface controls/status | Runtime / Surface |
| Brush controls | Control / Ink |
| Telemetry | Performance |
| Inspection placeholder | Inspect |
| Vector/Marker/Chalk/Membrane | Control / Ink |
| Object/Partial Eraser | Control / Tools + Eraser settings |
| Selection mode/state | Control / Tools + Inspect / Selection |
| Undo/Redo | Control / History |
| CPU reference / GPU default / platform default | Runtime / Surface |
| Force full redraw | Runtime / Advanced Actions |
| Reset rolling metrics | Performance / Metrics |
| product/surface request result | common bounded Activity Log |
| generation/revision counters | Dashboard + owner-specific Runtime sections |

No existing capability is allowed to retain a parallel mutation path solely because it originated in a platform toolbar or older Debug UI panel.

## 7. Layer 2 — Control, diagnostics, and snapshot ownership

PX0 freezes four primary concepts:

| Component | Responsibility | May mutate Runtime? | Product UI consumer? |
| --- | --- | ---: | ---: |
| RuntimeFacade | product command + product-safe state | yes | yes |
| Diagnostics | internal read-only observation | no | no |
| DebugControl | engineering-only mutation | yes | no |
| DebugSnapshot | bounded immutable Debug UI projection | no | no |

### 7.1 RuntimeFacade is the product control plane

PX0 MUST remove the inheritance coupling:

```cpp
RuntimeFacade : RuntimeDiagnostics
```

The target contract is conceptually:

```cpp
class RuntimeFacade {
public:
    virtual RuntimeStateSnapshot readRuntimeState() const noexcept = 0;
    virtual ProductControlReceipt
    submitProductControl(const ProductControlRequest&) noexcept = 0;
};
```

The same concrete Runtime owner may implement both RuntimeFacade and IAxiomDiagnostics, but consumers receive separate interface capabilities.

Product Shell obtains RuntimeFacade only. Debug composition may receive RuntimeFacade plus diagnostics/debug-control interfaces.

This is an internal C++ contract cleanup. PX0 MUST NOT alter the stable Runtime C ABI as an incidental consequence.

### 7.2 Product state is structured

`RuntimeStateSnapshot` MUST stop growing as a top-level flat field bag. It becomes a product projection grouped by stable concerns, conceptually:

```text
RuntimeStateSnapshot
├ Identity
├ Tool
├ Camera
├ History
└ Selection
```

PX0 migrates existing fields only. Feature-specific product state is added later through typed feature contracts/projections rather than by appending unrelated fields to a single generic request/snapshot.

Product-safe state may include data such as current tool, brush/eraser selection, selection count, primary selection kind/identity summary, camera state, and Undo/Redo availability.

It MUST NOT expose RuntimeScene pointers, SkSurface/Skia objects, SpatialIndex internals, GPU resources, or renderer-owned objects.

### 7.3 Diagnostics are read-only and owner-oriented

PX0 retains owner seams:

```text
IAxiomDiagnostics
IArcDiagnostics
IPlatformDiagnostics
ITelemetry
```

Axiom diagnostics are organized by stable diagnostic domains rather than UI panels:

```text
Identity
Document
Input
Interaction
Scene
Render
Resource
Feature Diagnostics
```

Owner truth remains explicit:

- document/interaction/selection/scene state -> Axiom;
- Arc pointer/preview/handoff state -> Arc;
- Surface dimensions/generations/loss/present state -> Platform;
- sampled latency/frame counters -> Telemetry.

Diagnostics MUST NOT expose mutation APIs.

### 7.4 Feature diagnostics extension seam

Feature diagnostics MUST NOT turn IAxiomDiagnostics into another flat bag.

PX0 establishes a typed built-in feature diagnostics aggregate. Conceptually:

```text
FeatureDiagnosticsSnapshot
├ Ink
├ Shape
├ RichText
├ Connector
├ Snap
└ Image
```

Each section carries availability and bounded state.

PX0 implements only already-existing capability. Future feature WPs populate their own sections.

Adding a future built-in feature may extend the typed feature aggregate and register a panel contribution, but MUST NOT require new platform-specific snapshot assembly or new workbench routing.

### 7.5 DebugControl is engineering-only

The boundary is normative:

```text
product behavior -> RuntimeFacade
engineering experiment/fault/control -> DebugControl
```

Examples:

- Set Brush / Set Eraser / Selection / Camera / Undo / Redo -> RuntimeFacade.
- future Shape/Text/Connector product actions -> product-safe typed RuntimeFacade capability.
- CPU/GPU Surface experiment -> PlatformDebugControl.
- Force full redraw / scene recompile / cache eviction / background-raster pause / memory budget / metric reset -> AxiomDebugControl or the appropriate engineering owner.

A product behavior MUST NOT be routed through DebugControl merely because its first UI is Debug UI.

### 7.6 Generic debug_ui::DebugCommand is retired

The existing generic `debug_ui::DebugCommand` / `BoundedCommandQueue` duplicates owner-specific control paths.

PX0 rules:

1. no new `DebugCommandKind` may be added;
2. existing users are migrated to RuntimeFacade, AxiomDebugControl, or PlatformDebugControl;
3. the generic queue is retired once no required migration user remains.

This does not remove owner-side bounded command queues that protect owner-thread/safe-point mutation.

### 7.7 Structured immutable DebugSnapshot

The flat DebugSnapshot is replaced by an immutable envelope with domain sections, conceptually:

```text
DebugSnapshot
├ Stamp
├ Coherence
├ Product projection
├ Axiom diagnostics
├ Arc diagnostics
├ Platform diagnostics
├ Telemetry
└ Activity
```

Snapshot structure follows data ownership, not UI layout. Dashboard, Runtime, Performance, and Inspect select from the same snapshot.

Snapshot content MUST remain bounded, copyable, and predictable. Full scene/document dumps do not belong in the live snapshot.

### 7.8 Section-local availability

The global `Capability[]` bitset is replaced by section-local availability:

```text
Available
Degraded
Unavailable
Unsupported
Error
```

Semantics are:

- Available: supported and currently usable;
- Degraded: supported but partial/stale/reduced;
- Unavailable: supported but not currently usable;
- Unsupported: not implemented for this owner/platform/build;
- Error: supported path failed.

This distinction is required so future Trace/GPU timing/feature sections do not conflate "not implemented" with "temporarily unavailable".

### 7.9 Snapshot coherence

Debug observation MUST not stall the Runtime to obtain a global lock.

PX0 uses best-effort immutable reads with identity validation:

```text
read leading identity
-> read Product/Axiom/Arc/Platform/Telemetry
-> read trailing identity
-> compare
```

The assembler may retry once when identity changed during capture. If it still changes, the snapshot remains displayable but is marked mixed/stale.

Minimum coherence states:

```text
Coherent
MixedGeneration
Stale
```

Panels MUST not present mixed generations as a single authoritative state without indication.

### 7.10 Common DebugSnapshotAssembler

Windows and Web MUST stop defining independent snapshot schema/assembly functions.

A common `DebugSnapshotAssembler` owns:

- provider reads;
- section availability;
- coherence validation;
- bounded aggregation;
- activity snapshot attachment.

Platform hosts only supply concrete providers and platform presentation.

### 7.11 Activity log

PX0 adds a bounded Debug Control Plane activity log.

It records Debug UI-originated request/receipt transitions, including:

- request ID;
- owner/source: Product, RuntimeDebug, PlatformDebug;
- action identity;
- current receipt state;
- relevant generation/frame identity.

It is not a canonical Operation log.

The log is bounded and does not become Gate evidence by itself.

For asynchronous engineering controls, the router/controller refreshes terminal receipts from the owning control contract. If a current owner contract lacks a receipt lookup required to observe queued completion, PX0 may add a read-only bounded receipt query to that debug-control interface. Such a query is engineering-only and must not change product semantics.

## 8. Layer 3 — Debug UI module and extension architecture

### 8.1 Dependency direction

The existing dependency direction is preserved:

```text
runtime/foundation
        ^
     debug_ui
        ^
application / platform host
```

Runtime modules MUST NOT depend on `debug_ui` or ImGui.

### 8.2 Target package structure

PX0 converges the Debug UI toward:

```text
debug_ui/
├ include/canvas/debug_ui/
│  ├ controller.hpp
│  ├ workbench.hpp
│  ├ workspace.hpp
│  ├ panel.hpp
│  ├ panel_registry.hpp
│  ├ snapshot.hpp
│  ├ snapshot_assembler.hpp
│  ├ control_router.hpp
│  ├ activity_log.hpp
│  ├ ui_session_state.hpp
│  ├ input_capture.hpp
│  ├ imgui_skia_renderer.hpp
│  └ windows_host.hpp
├ src/
│  ├ controller.cpp
│  ├ workbench.cpp
│  ├ panel_registry.cpp
│  ├ snapshot.cpp
│  ├ snapshot_assembler.cpp
│  ├ control_router.cpp
│  ├ activity_log.cpp
│  ├ imgui_skia_renderer.cpp
│  ├ windows_host.cpp
│  └ panels/
│     ├ dashboard.cpp
│     ├ control.cpp
│     ├ inspect.cpp
│     ├ runtime.cpp
│     ├ performance.cpp
│     ├ scenarios.cpp
│     └ features/
│        ├ ink.cpp
│        ├ shape.cpp
│        ├ rich_text.cpp
│        ├ connector.cpp
│        ├ snap.cpp
│        └ image.cpp
└ tests/
```

PX0 does not implement the future feature files' behavior. The structure freezes their intended extension location.

### 8.3 DebugController

DebugController becomes orchestration rather than a large panel renderer.

It composes:

```text
DebugSnapshotAssembler
DebugControlRouter
DebugActivityLog
PanelRegistry
DebugUiSessionState
DebugWorkbench
```

Its frame flow is:

```text
capture snapshot
-> refresh observable outstanding receipts
-> render workbench
-> panels emit intents
-> route intents to owners
-> update activity
```

DebugController does not own Shape geometry, RichText layout, Connector routing, Arc rendering, Surface creation, or semantic mutation.

### 8.4 Platform hosts become thin

Windows/Web Debug UI hosts own platform realization only:

- overlay/canvas/window lifetime;
- ImGui platform input;
- Debug UI Skia surface;
- show/hide;
- resize/reposition;
- render scheduling;
- invocation of DebugController frame.

They MUST NOT understand Brush semantics, Connector semantics, RichText semantics, Runtime generation schema, or snapshot assembly.

Android/iOS may adopt the same controller/workbench later without changing the common contract.

### 8.5 DebugUiSessionState

UI-local state is separate from Runtime/diagnostic state.

Examples:

- selected workspace;
- collapsed/expanded sections;
- search/filter strings;
- activity auto-scroll;
- chosen metric view.

Changing DebugUiSessionState MUST NOT alter Document generation, Runtime state, history, or canonical operations.

### 8.6 Panel contract

All panels receive only:

```text
const DebugSnapshot&
DebugControlRouter&
DebugUiSessionState&
```

Conceptually:

```cpp
struct DebugPanelContext {
    const DebugSnapshot& snapshot;
    DebugControlRouter& controls;
    DebugUiSessionState& ui;
};
```

A panel MUST NOT receive or fetch:

- SemanticDocument*;
- RuntimeScene*;
- SpatialIndex*;
- SkSurface*;
- InkPlaygroundHost*;
- diagnostics provider pointers;
- RuntimeFacade/AxiomDebugControl/PlatformDebugControl pointers directly.

The invariant is:

```text
Panel = Snapshot -> UI -> Intent
```

### 8.7 Workspace and panel contribution model

The two overlapping current abstractions `PanelCapability` and `DebugPanel` are replaced by:

- `WorkspaceId` for the six top-level workspaces;
- stable `PanelSlot` values inside those workspaces;
- explicit panel contributions registered at composition time.

Built-in feature registration is explicit, for example:

```text
registerInkDebugPanels(registry)
registerShapeDebugPanels(registry)
registerRichTextDebugPanels(registry)
registerConnectorDebugPanels(registry)
```

PX0 MUST NOT introduce static-global registration, reflection, or dynamic plugin discovery.

### 8.8 Stable extension slots

Stable slots include:

```text
Control / Feature
Inspect / Object
Inspect / Interaction
Inspect / Relationship
Runtime / Feature Diagnostics
Performance / Feature Metrics
Scenarios / Feature Scenario
```

A feature may contribute to multiple slots.

Examples:

Shape:
- Control / Feature: kind/style/defaults;
- Inspect / Object: bounds/transform;
- Inspect / Interaction: create/resize state.

RichText:
- Control / Feature: font/size/alignment;
- Inspect / Object: layout constraints;
- Inspect / Interaction: session/caret/selection/composition;
- Runtime / Feature: shaping/font resolution.

Connector:
- Control / Feature: line/arrow/routing defaults;
- Inspect / Object: connector projection;
- Inspect / Interaction: endpoint drag/candidate;
- Inspect / Relationship: source/target attachment;
- Runtime / Feature: resolved geometry.

Snap:
- Control / Feature only if a product-safe on/off/default exists;
- primarily Inspect / Interaction and Runtime / Feature diagnostics.

### 8.9 Feature product-control rule

The existing generic `ProductControlRequest` MUST NOT become another flat feature bag by adding Shape, text, connector, and snap-specific fields indefinitely.

Core Tool/View/History controls may remain on the existing core request path.

Feature-specific product behavior MUST use typed product capability contracts owned by RuntimeFacade's product control plane. Exact Shape/RichText/Connector request schemas are intentionally owned by PX1/PX2/PX4 and are not invented by PX0.

This is an explicit scope boundary, not an unresolved design decision.

### 8.10 DebugControlRouter

Panels do not construct owner requests directly.

`DebugControlRouter` owns Debug UI adaptation concerns:

- request ID allocation;
- generation fencing;
- deadlines;
- owner selection;
- receipt normalization;
- activity recording.

Conceptually:

```text
Panel intent
    -> DebugControlRouter
        -> RuntimeFacade
        -> AxiomDebugControl
        -> PlatformDebugControl
```

The router is Debug UI infrastructure only. Product UI does not depend on it and calls product-safe RuntimeFacade/Shell contracts directly.

The router MUST NOT contain feature business logic.

### 8.11 Workbench layout contract

PX0 uses one engineering-workbench layout, not docking.

Conceptually:

```text
+----------------------------------------------------+
| Header: runtime/doc/coherence/frame health         |
+--------------+-------------------------------------+
| Dashboard    |                                     |
| Control      |                                     |
| Inspect      |        active workspace             |
| Runtime      |                                     |
| Performance  |                                     |
| Scenarios    |                                     |
+--------------+-------------------------------------+
| Activity: latest request/receipt                   |
+----------------------------------------------------+
```

The current fixed `410 x 560` panel is no longer the semantic layout contract.

Hosts provide available geometry. Common Workbench code owns minimum/preferred dimensions and sidebar/content layout so Windows and Web do not maintain separate UI structure.

## 9. Runtime-side contract organization

PX0 may reorganize internal headers without introducing new public libraries solely for aesthetics.

The intended contract grouping is conceptually:

```text
runtime/foundation/include/canvas/runtime/
├ runtime_facade.hpp
├ runtime_state.hpp
├ product_control.hpp
├ diagnostics.hpp
└ diagnostics/
   ├ core.hpp
   ├ feature.hpp
   ├ shape.hpp
   ├ rich_text.hpp
   ├ connector.hpp
   └ snap.hpp
```

A new CMake library is not required. Keeping these contracts under `canvas_runtime_foundation` is acceptable and preferred unless implementation evidence shows a real dependency-cycle reason to split targets.

## 10. Platform and temporary-toolbar convergence

During PX0, Windows native toolbar, Web HTML toolbar, and common Debug UI may all remain visible for qualification.

Their mutation flow MUST converge:

```text
Windows temporary toolbar ----\\
Web temporary toolbar ---------+--> RuntimeFacade
Debug UI ----------------------/
```

They MUST NOT retain host-internal parallel mutation semantics.

PX0 does not remove all temporary product controls. Their final retirement belongs to PX7 Product UI Projection or an explicit successor package.

## 11. Runtime data flow

### 11.1 Product mutation

```text
Product UI ----------------------\\
                                  +--> RuntimeFacade -> Runtime owners
Debug UI -> DebugControlRouter --/
```

A product action has one semantic mutation path regardless of which UI triggered it.

### 11.2 Engineering mutation

```text
Debug UI
  -> DebugControlRouter
      -> AxiomDebugControl
      -> PlatformDebugControl
```

Owner safe-point/queue semantics remain responsible for avoiding synchronous heavyweight Runtime mutation from an ImGui callback.

### 11.3 Observation

```text
Runtime   -> IAxiomDiagnostics ------\\
Arc       -> IArcDiagnostics ---------\\
Platform  -> IPlatformDiagnostics ----+--> DebugSnapshotAssembler
Telemetry -> ITelemetry --------------/
Activity ----------------------------/
                                              |
                                              v
                                         DebugSnapshot
                                              |
                                              v
                                          all panels
```

Panels never query providers independently.

## 12. Failure and degraded-state behavior

PX0 must remain usable when one provider is unavailable.

Rules:

- one unavailable diagnostic provider MUST NOT prevent other sections from rendering;
- unsupported capability is labeled Unsupported, not Error;
- stale/mixed snapshots remain inspectable with a coherence warning;
- owner queue full/stale-generation/expired/failed receipts are retained in Activity;
- Debug UI control failure MUST NOT silently fall back to a direct owner/internal mutation;
- Surface loss MUST NOT invalidate canonical document truth;
- Debug UI hidden state MUST not intercept Canvas input;
- Debug UI input capture MUST remain sequence-latched so input does not leak to the Canvas mid-sequence;
- a failed Debug UI render/panel must not block canonical Canvas input/rendering.

## 13. Verification strategy

PX0 verification is architectural and behavioral.

### 13.1 Contract tests

Required contract coverage includes:

- RuntimeFacade no longer requires RuntimeDiagnostics inheritance;
- Product Shell-compatible consumers can compile with RuntimeFacade only;
- diagnostics are read-only;
- panel context exposes no owner/internal pointer;
- global Capability[] is no longer required for section availability;
- common assembler produces bounded snapshots;
- coherence classification is deterministic;
- generic `DebugCommandKind` receives no new uses and is retired when migration completes;
- DebugControlRouter routes product/debug/platform intents to the correct owner;
- Activity Log is bounded;
- a feature contribution can register Control + Inspect content without modifying Workbench core.

### 13.2 Platform contract tests

Windows/Web must prove:

- host owns presentation/input realization only;
- snapshot assembly is common;
- current controls still reach the same product/debug owner contracts;
- Debug UI hidden state does not consume Canvas input;
- Debug UI owned pointer/keyboard sequences do not leak into Canvas;
- Surface mode control preserves generation fencing and failure reporting.

### 13.3 Physical validation

PX0 closure requires physical Windows and Web validation for the common workbench because both already host the current Debug UI.

Physical validation covers:

- show/hide;
- workspace navigation;
- Brush/Eraser switching;
- selection mode;
- Undo/Redo;
- Camera Pan, Zoom At, Fit Content, Fit Selection, and Fit Primary Object with correct context disabling and state readback;
- Surface mode controls where supported;
- current Arc/Surface/Telemetry state visibility;
- Activity updates;
- Canvas input isolation.

Android/iOS physical Debug UI parity is successor work unless a separate Current Authority makes it blocking.

## 14. Work-package decomposition

PX0 implementation is split into narrow packages.

### PX0-00 — Baseline Reconciliation

Read-only reconciliation of:

- actual execution branch/revision;
- current RuntimeFacade contract;
- current Debug UI implementation;
- Windows/Web host integration;
- any newly-landed G4.7 RichText or diagnostics changes;
- existing authority conflicts.

No functional code change is authorized.

### PX0-01 — Contract Boundary Cleanup

- decouple RuntimeFacade from RuntimeDiagnostics;
- structure existing product state;
- preserve product behavior;
- preserve stable public Runtime C ABI;
- establish diagnostics/feature diagnostics ownership.

No Shape/RichText/Connector behavior.

### PX0-02 — Structured Snapshot + Assembler

- domain snapshot sections;
- section-local availability;
- coherence;
- common assembler;
- migrate Windows/Web away from independent assembly.

No UI redesign beyond compatibility required for migration.

### PX0-03 — Control Router + Activity

- central Debug UI request sequencing/fencing/deadline handling;
- owner routing;
- bounded Activity Log;
- terminal receipt observation;
- stop extending generic DebugCommand.

No new feature product capability.

### PX0-04 — Workbench Shell + Panel Registry

- six workspaces;
- header/sidebar/content/footer;
- WorkspaceId / PanelSlot / explicit registry;
- DebugUiSessionState;
- extension proof.

Does not migrate all feature content yet.

### PX0-05 — Existing Capability Migration

Migrate existing:

- Brush/Eraser;
- selection;
- View/Camera controls: Pan, Zoom At, Fit Content, Fit Selection, Fit Primary Object, plus camera state/readback; and History controls;
- Arc;
- Surface;
- telemetry;
- Runtime debug actions;
- health and activity display.

No new Shape/RichText/Connector/Snap/Image behavior.

### PX0-06 — Host Convergence + Temporary Toolbar Cleanup

- thin Windows/Web Debug UI hosts;
- common controller/assembler;
- temporary Windows/Web toolbar mutation through RuntimeFacade;
- preserve existing qualification utility.

Does not implement final Product UI.

### PX0-07 — Closure / Evidence

- contract tests;
- extension proof;
- Windows/Web physical validation;
- architecture-boundary checks;
- closure evidence compilation.

No PX1 implementation begins inside PX0-07.

## 15. PX0 closure criteria

PX0 is ready for Gate review only when all are true:

### Architecture

- Product mutation flows only through product-safe RuntimeFacade contracts.
- Engineering mutation flows only through explicit DebugControl owners.
- Diagnostics are read-only.
- Panels cannot directly access Runtime owners/internals.
- Windows/Web do not define separate live snapshot schemas.

### Snapshot

- snapshot is structured by owner/domain;
- availability is section-local;
- coherence is represented;
- live snapshot size is bounded;
- assembler is common cross-platform code.

### Workbench

- Dashboard, Control, Inspect, Runtime, Performance, Scenarios exist;
- current Brush/Eraser/selection/history/View-Camera/Surface/Arc/telemetry capabilities are preserved and reorganized;
- recent control activity is observable.

### Platform

- Windows physical path passes;
- Web physical path passes;
- Debug UI hidden input does not affect Canvas;
- Debug UI-owned input cannot leak into Canvas.

### Extension proof

A compile/test-only example contribution can register content into at least Control / Feature and Inspect / Object or Interaction without modifying Workbench core or platform hosts.

## 16. Explicit invariants for successor features

PX1 Shape, PX2 RichText, PX4 Connector, PX5 Snap, and PX6 Image inherit the following invariants:

1. product behavior uses RuntimeFacade product capability, never DebugCommand;
2. internal observation uses typed diagnostics;
3. Debug UI renders from DebugSnapshot only;
4. feature panels emit intents through DebugControlRouter;
5. feature code does not add a top-level workspace;
6. feature code does not make platform hosts understand feature semantics;
7. feature additions do not expose RuntimeScene/Skia/GPU internals to Product Shell;
8. feature diagnostics remain bounded;
9. scenario actions may not bypass canonical Operation/interaction authority.

## 17. Expected PX1 handoff shape

When PX0 is closed, PX1 Shape should be able to proceed mechanically:

```text
Shape product capability
    -> RuntimeFacade typed product contract

Shape internal observation
    -> ShapeDiagnosticsSnapshot

Canvas interaction
    -> existing Axiom interaction/semantic owners

Debug contribution
    -> Control / Feature
    -> Inspect / Object
    -> Inspect / Interaction

Qualification
    -> Shape scenario/evidence
```

PX1 should not need to redesign Dashboard, Workbench navigation, snapshot assembly, Windows host, or Web host.

The same pattern applies to PX2 RichText and PX4 Connector.

## 18. Acceptance boundary

This design is accepted when the team agrees that PX0 is a control-plane architecture convergence stage, not a feature stage: it preserves existing Debug UI capability, separates Product/Diagnostics/Debug ownership, centralizes snapshot/control infrastructure, establishes a stable engineering workbench and feature-extension contract, and leaves Shape/RichText/Connector product semantics to their successor work packages.
