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

This package resumes interrupted work through P33. Resolve repository/package identity first, inspect the preserved local PX0-05 worktree, classify it against the task anchor as `ANCHOR_DESCENDANT_WITHOUT_CURSOR`, and reconcile its existing diff against the v0.3 authorized scope. Preserve conforming work, migrate the obsolete Windows selection structural oracle if still incomplete, establish a reconciled continuation point, then resume at the first incomplete frozen obligation. Do not replay already-valid panel migration work.


## P31 reconciliation after blocked P32

The first P32 attempt correctly failed closed on two independent blockers.

1. `TASK_PACKAGE_DEFECT`: the v0.1 package froze `windows_debug_ui_selection_contract_test.py` as unchanged even though its first test still required the PX0-04 legacy `ImGui::BeginTabBar`, `Canvas / Selection`, and `RuntimeFacade` tabs that PX0-05 V1 requires removing. v0.2 authorizes migrating only that obsolete Debug UI structural assertion to the final Control/Inspect panel architecture. Its Windows selection-pointer and terminal-clear/facade-synchronization assertions remain required.
2. `DEPENDENCY_BLOCKER`: the locked native build hits a pre-existing Runtime/Render include-boundary failure resolving `canvas/scene/scene_types.hpp`. That boundary is outside PX0-05 mutation scope and is handled by separate task `PX0-BUILD-01`.

The executor's current PX0-05 implementation exists only in a local worktree and has not been durably materialized. It is therefore **not** an accepted `resume_cursor`. Preserve it physically, but do not treat it as trusted completed work until a later P33 reconciliation compares it against the corrected current package after PX0-BUILD-01 closes.

This v0.2 package is intentionally `BLOCKED_DEPENDENCY` and MUST NOT be used as a P32/P33 execution handoff. After PX0-BUILD-01 produces an exact accepted result, materialize a new current PX0-05 package binding that exact dependency, then reconcile the existing local worktree as `ANCHOR_DESCENDANT_WITHOUT_CURSOR` and preserve only conforming valid changes.


## P31 v0.3 build-environment resolution

PX0-BUILD-01 ended with an accepted `BLOCKED_ENVIRONMENT` diagnosis rather than a repository repair.

Exact diagnostic identities:

- source/result revision: `beaccca42b3587b19daf981b93587fd6322f2d26`
- reviewer-accessible evidence head: `def8f6deebc54af72e109fb9ad0875fbbf4949e4`

The clean PX0-04-equivalent configuration used `CANVAS_BUILD_RF01=ON` and `CANVAS_BUILD_POC01=OFF`; `canvas_runtime_render` built successfully and its compile commands contained both `runtime/scene/include` and `runtime/text/include`. The earlier PX0-05 cache used `CANVAS_BUILD_RF01=OFF`, which omitted the Scene/Text targets and produced the misleading `scene_types.hpp` failure.

Therefore:

- there is no Runtime/Render CMake repair dependency;
- PX0-05 MUST use a fresh native build tree;
- native verification MUST set `CANVAS_BUILD_RF01=ON` and `CANVAS_BUILD_POC01=OFF` and otherwise follow the accepted locked clang-cl + Skia profile;
- the earlier PX0-05 native cache MUST NOT be reused;
- if the missing-header failure returns under the exact clean v0.3 profile, fail closed as `BLOCKED_ENVIRONMENT` rather than widening Runtime scope.

## P33 resume posture

The prior PX0-05 implementation remains local-only at:

`C:/Users/Chado/.codex/worktrees/px0-05-existing-capability-migration-clean`

It is candidate work, not accepted Gate evidence and not an accepted resume cursor. The P33 executor must inspect and reconcile it against this v0.3 package. Candidate work reported from the interrupted execution includes the six workspace registrations, legacy-panel removal, snapshot-backed control readback, owner-presence handling, camera drafts, Surface/Axiom actions, Unsupported placeholders, focused Workbench/Router evidence, the new PX0-05 structural contract, and real ImGui widget activation. Preserve only what repository inspection proves conforms to v0.3.

The selection structural oracle correction from v0.2 remains mandatory: the first test in `windows_debug_ui_selection_contract_test.py` must validate the final Control/Inspect architecture rather than requiring `BeginTabBar`, `Canvas / Selection`, or a `RuntimeFacade` tab. Its pointer-path and terminal-clear/facade synchronization assertions remain unchanged.


## P31 v0.4 package correction

P34/P35 independently reviewed the P33 v0.3 result and classified the remaining blocker as a `TASK_PACKAGE_DEFECT`, not a PX0-05 production implementation defect.

Accepted continuation point:

- implementation result: `9c5e3587358ad982ddab56ca2369cd285e330c5d`
- evidence head: `4b7ca8856a2c94b5fcdd2569009a9be669405955`
- P34/P35 materialization: `6f703b60cbcc7ea3587b946d5c2962bf1e8db59b`

Repository reality proved both the failing `debug_ui/tests/runtime_contract_test.cpp` blob and the `ProductControlReceipt` definition were unchanged from the PX0-05 task anchor through the v0.3 result. The frozen clang-cl `/W4 /WX` profile rejects this existing test fixture because `RuntimeProbe::submitProductControl` omits the existing optional `ProductControlReceipt::control` aggregate field.

v0.4 therefore adds exactly one new source authorization:

`debug_ui/tests/runtime_contract_test.cpp`

The only allowed source repair is to fully initialize `ProductControlReceipt`, preserving `control == std::nullopt` for this non-canvas product receipt. Do not modify RuntimeFacade, ProductControlReceipt, warning policy, any panel code, or any platform host.

## P33 v0.4 resume

The v0.3 implementation result `9c5e3587358ad982ddab56ca2369cd285e330c5d` is now the accepted resume cursor. All six-workspace capability migration work is preserved and must not be replayed.

Expected execution position:

`DESCENDANT_CURSOR`

The execution branch may contain evidence/control/package descendants after the cursor. Inspect only the delta after `9c5e3587...`, verify that it is control/evidence/package materialization, apply the single test-file repair, and rerun the frozen closure evidence.

The native profile remains the clean RF01=ON / POC01=OFF locked Ninja Release clang-cl + Skia profile established in v0.3. A fresh build tree is required.
