# PX0-04 - Workbench Shell + Panel Registry

PX0-04 starts from the accepted PX0-03 control-plane result.

- repository: `github:Mostorm-Labs/axiom`
- accepted code result: `172ef085cc68b66258e535779b3df15cc0bbadbd`
- accepted evidence head / task anchor: `19840c78064ac7b73bf59daa64ddb45559b93db0`
- approved Spec: `docs/superpowers/specs/2026-10-10-px0-debug-control-plane-baseline-design.md`
- approved Plan: `docs/superpowers/plans/2026-10-10-px0-debug-control-plane-baseline-implementation.md`

## Current reality

PX0-03 already provides the correct control and observation infrastructure:

```text
providers -> DebugSnapshotAssembler -> structured DebugSnapshot
panels -> DebugControlRouter -> RuntimeFacade / AxiomDebugControl / PlatformDebugControl
router transitions -> bounded DebugActivityLog
```

The UI shell is still transitional:

```text
buildImGuiPanels()
  -> fixed 410 x 560 top-level ImGui window
  -> legacy tab bar
  -> controller.cpp also contains ImGuiSkiaRenderer

controller.hpp
  -> old DebugPanel taxonomy

panels.hpp
  -> old PanelCapability taxonomy

panel_model
  -> legacy describe() mapping
```

Windows and Web still call the common `buildImGuiPanels` entrypoint directly. Their host convergence to a composed `DebugController` is PX0-06, not this task.

## Target architecture

PX0-04 adds:

```text
WorkspaceId
PanelSlot
PanelContribution
PanelRegistry
DebugUiSessionState
DebugWorkbench
ImGuiSkiaRenderer (separate translation unit)
DebugController (common composition/orchestration)
```

Exactly six workspaces:

```text
Dashboard
Control
Inspect
Runtime
Performance
Scenarios
```

The panel invariant is:

```text
Panel = DebugSnapshot -> UI -> Intent
```

A panel receives only `DebugPanelContext{snapshot, controls, ui}`.

## Transitional visual integration

PX0-04 must make the shell visible without stealing PX0-05/06 scope.

Current Windows/Web callers keep their existing common `buildImGuiPanels(snapshot, selectedTool, router)` call site. Refactor that compatibility entrypoint so it renders the new Workbench shell.

For the transitional content, explicitly register one legacy compatibility contribution in `Control / Tools`. Move the existing panel body into that contribution without changing its control semantics. It may keep its internal legacy tab grouping until PX0-05.

Other workspaces may render an explicit not-yet-migrated/empty state. The Workbench header and Activity footer are common shell chrome, not final capability migration.

Do not create a process-global registry/session singleton. For this temporary entrypoint, a per-frame explicit registry plus selected-workspace persistence in the active ImGui context storage is acceptable. The durable/persistent `DebugController` owns a real `PanelRegistry` and `DebugUiSessionState`.

PX0-05 replaces the legacy compatibility contribution with the real per-workspace capability panels. PX0-06 makes Windows/Web hosts own/use only the common controller boundary.

## Controller frame

```text
snapshot = assembler.capture(sources)
router.beginFrame(snapshot)
router.refreshReceipts()
snapshot.activity = activity.snapshot()
store one immutable snapshot
workbench.render(snapshot, router, ui, registry)
```

Do not re-read diagnostics after `refreshReceipts`.

## Renderer move

Move `ImGuiSkiaRenderer` out of controller without algorithm changes. Preserve the existing A8 atlas, normalized UV shader matrix, white paint and `SkBlendMode::kModulate`.

## Old taxonomy reconciliation

`panels.hpp`, `PanelCapability`, `DebugPanel`, and `PanelState` are not allowed to remain as a second navigation taxonomy.

`DebugPanelModel` may remain only as a temporary product-control helper if required by the existing regression target. Its `describe()` API and old panel mapping must be removed.

## First incomplete action

Run repository/package/binding preflight and ImplementationDesignPreflight. Then add the PX0PanelRegistry and PX0Workbench RED tests before production mutation. Continue through the whole frozen closure contract unless an explicit terminal blocker is found.
