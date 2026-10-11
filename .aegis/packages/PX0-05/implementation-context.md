# PX0-05 - Existing Capability Migration

PX0-05 starts from the accepted PX0-04 Workbench result.

- repository: `github:Mostorm-Labs/axiom`
- accepted PX0-04 code result: `ee00d39e44885c66a4d2f0fa584f5d0844c1eb32`
- accepted PX0-04 evidence head / task anchor: `4a35e8fc041e90040d05fe8e8d55766fa8014806`
- approved Spec: `docs/superpowers/specs/2026-10-10-px0-debug-control-plane-baseline-design.md`
- approved Plan: `docs/superpowers/plans/2026-10-10-px0-debug-control-plane-baseline-implementation.md`

## Current flow

PX0-04 established the stable shell:

```text
DebugWorkbench
  Header
  Sidebar: Dashboard / Control / Inspect / Runtime / Performance / Scenarios
  Content: PanelRegistry contributions
  Footer: bounded Activity
```

But all useful capability is still inside one transitional contribution:

```text
Control / Tools
  -> "Transitional legacy controls"
     -> legacy ##debug_tabs
        Overview
        Canvas / Selection
        RuntimeFacade
        Brush / Eraser
        Input
        Surface
        Diagnostics
```

PX0-05 removes that compatibility content body and migrates the already-authorized capability into final built-in workspace slots. It does not redesign Workbench/PanelRegistry.

## Target built-ins

```text
Dashboard
  Runtime Identity
  Current Product State
  Health
  Live Performance
  Recent Activity

Control
  Tools
  Ink
  View
  History
  Feature (Unsupported successor slot)

Inspect
  Selection
  Object (Unsupported)
  Interaction
  Relationship (Unsupported)

Runtime
  Document
  Scene (Unsupported)
  ARC
  Render
  Surface
  Resources (Unsupported)
  Feature Diagnostics (Unsupported in current DebugSnapshot)
  Advanced Actions

Performance
  Live Metrics
  Input / ARC
  Rendering
  Memory & Cache (Unsupported)
  Trace (Unsupported)
  GPU Timing (Unsupported)
  Feature Metrics (Unsupported)

Scenarios
  Smoke:
    Reset Canvas - Unsupported
    Empty Canvas - Unsupported
    Ink Baseline - Unsupported
```

## Product control/readback

Do not use legacy `toolId` as semantic Pan/Selection identity. Current product state already exposes separate `brushId/brushRevision`, `eraserId` and selection state. Use those for persistent UI selection/readback.

Product actions:

```text
Selection mode -> router.setSelectionMode
Object/Partial eraser -> router.setEraser
Vector/Marker/Chalk/Membrane -> router.setBrush
Undo/Redo -> router.undo/redo
Pan by -> router.panBy
Zoom At -> router.zoomAt
Fit Content -> router.fitToContent
Fit Selection -> router.fitToSelection
Fit Primary -> router.fitPrimaryObject
```

Brush parameter rows remain Unsupported. Do not expose CanvasControl ink-option mutation in PX0-05.

Camera display reads product camera state and Axiom camera generation when available. Pan/Zoom input values are drafts in DebugUiSessionState only. No Reset View or 100%.

## Engineering controls

Panels still receive only DebugControlRouter, never owner pointers. Add read-only Router owner-presence methods so UI can avoid presenting a bound-owner action as available when the owner is absent.

Current PX0-05 actionable engineering controls:

```text
Runtime / Surface
  Platform Default
  CPU Reference
  GPU Default
  -> requires PlatformDebug owner + usable canonical generation

Runtime / Advanced Actions
  Force Full Redraw
  -> requires AxiomDebug owner

Performance / Live Metrics
  Reset Rolling Metrics
  -> requires AxiomDebug owner
```

The rest of the Axiom command family remains visible Unsupported. This is deliberate: current Authority forbids inventing overlay names, memory units/ranges, background-raster readback and unsupported cache/scene semantics.

## Platform reality

Windows currently binds AxiomDebugControl and PlatformDebugControl. Web currently binds neither engineering owner while still providing diagnostics. This is why UI availability must use Router owner presence rather than diagnostic availability alone.

Do not modify Windows/Web production host composition here. PX0-06 owns host convergence.

## Scenarios

No sanctioned PX0 fixture/reset boundary exists in the frozen baseline. Therefore Reset Canvas, Empty Canvas and Ink Baseline are labels/status only and must cause zero control/owner calls.

## First incomplete action

Run repository/package/binding preflights and ImplementationDesignPreflight. Then update the focused Workbench/Router/structural oracles and observe the required RED before adding panel production code. Continue through the full frozen closure contract unless an explicit terminal blocker is encountered.


## P31 reconciliation after blocked P32

The first P32 attempt correctly failed closed on two independent blockers.

1. `TASK_PACKAGE_DEFECT`: the v0.1 package froze `windows_debug_ui_selection_contract_test.py` as unchanged even though its first test still required the PX0-04 legacy `ImGui::BeginTabBar`, `Canvas / Selection`, and `RuntimeFacade` tabs that PX0-05 V1 requires removing. v0.2 authorizes migrating only that obsolete Debug UI structural assertion to the final Control/Inspect panel architecture. Its Windows selection-pointer and terminal-clear/facade-synchronization assertions remain required.
2. `DEPENDENCY_BLOCKER`: the locked native build hits a pre-existing Runtime/Render include-boundary failure resolving `canvas/scene/scene_types.hpp`. That boundary is outside PX0-05 mutation scope and is handled by separate task `PX0-BUILD-01`.

The executor's current PX0-05 implementation exists only in a local worktree and has not been durably materialized. It is therefore **not** an accepted `resume_cursor`. Preserve it physically, but do not treat it as trusted completed work until a later P33 reconciliation compares it against the corrected current package after PX0-BUILD-01 closes.

This v0.2 package is intentionally `BLOCKED_DEPENDENCY` and MUST NOT be used as a P32/P33 execution handoff. After PX0-BUILD-01 produces an exact accepted result, materialize a new current PX0-05 package binding that exact dependency, then reconcile the existing local worktree as `ANCHOR_DESCENDANT_WITHOUT_CURSOR` and preserve only conforming valid changes.
