# PX0-03 - Control Router + Bounded Activity

PX0-03 begins from the accepted PX0-02 result:

- repository: `github:Mostorm-Labs/axiom`
- task anchor: `0eab5dea1570f9b8ae7535aea0eef6b1135061a7`
- approved Spec: `docs/superpowers/specs/2026-10-10-px0-debug-control-plane-baseline-design.md`
- approved Plan: `docs/superpowers/plans/2026-10-10-px0-debug-control-plane-baseline-implementation.md`

## Current implementation reality

PX0-02 already established the common structured observation path:

```text
RuntimeFacade / IAxiomDiagnostics / IArcDiagnostics / IPlatformDiagnostics / ITelemetry
                                  |
                                  v
                        DebugSnapshotAssembler
                                  |
                                  v
                         structured DebugSnapshot
```

Control flow is still transitional:

```text
current buildImGuiPanels
  |-- Selection / Undo / Redo ------> RuntimeFacade directly
  |-- Brush / Eraser / Pan ---------> RuntimeFacade::submitCanvasControl directly
  |-- Surface mode -----------------> PlatformDebugControl directly
  |
  +-- generic DebugController::submit -> BoundedCommandQueue
       (duplicate infrastructure; not the owner-safe-point queues)
```

Windows also has a platform-local DebugActivitySource exposing only the latest Product/Surface receipts. Web has no activity source. AxiomDebugControl already has receipt(requestId). PlatformDebugControl does not yet expose receipt lookup even though BoundedSurfaceModeQueue retains receipts.

## Target flow

```text
legacy panels
      |
      v
DebugControlRouter
  |----------------> RuntimeFacade            product intents
  |----------------> AxiomDebugControl        engineering intents
  +----------------> PlatformDebugControl     Surface intents
      |
      +---- request IDs / generation fences / deadline
      +---- normalized receipt states
      +---- pending engineering receipt refresh
      v
DebugActivityLog (bounded 64)
      |
      v
DebugActivitySource
      |
DebugSnapshotAssembler -> DebugSnapshot.activity
```

The fixed tab layout remains. PX0-04, not this package, introduces Workbench/Workspace/PanelRegistry.

## Activity compatibility

PX0-02 introduced:

```cpp
struct DebugActivitySnapshot {
  optional<ProductControlReceipt> productControl;
  optional<SurfaceModeReceipt> surfaceControl;
};
```

PX0-03 may extend this structure with bounded normalized `entries` while preserving those latest typed receipts so the current RuntimeFacade/Surface rows do not need a layout rewrite before PX0-04/PX0-05.

`DebugActivityLog` implements `DebugActivitySource`. It records only Debug UI-originated control-plane transitions. It is not the product Operation log and not evidence storage.

## Router frame context

`beginFrame(snapshot)` captures the immutable request context for the frame.

- request IDs are router-local, monotonically increasing, non-zero;
- queued owner deadline = `snapshot.stamp.sequence + 120`;
- Axiom request expected generations = snapshot runtime/document generations;
- Surface expected generation = Platform canonicalSurfaceGeneration when that section exists, otherwise snapshot surfaceGeneration;
- Activity generation/frame fields come from the current frame snapshot.

`refreshReceipts()` polls only outstanding queued Axiom/Surface receipts. It must not re-read any diagnostics provider.

## Existing panel migration

Do not redesign the tabs.

Migrate:

- Selection checkbox -> `router.setSelectionMode`
- Undo / Redo -> router
- Vector/Marker/Chalk/Membrane -> `router.setBrush(1..4, revision)`
- Object/Partial Eraser -> `router.setEraser(1/2)`
- existing Pan item -> `router.setTool(static_cast<uint32_t>(CanvasToolKind::kPan))`; the router submits typed CanvasControl through RuntimeFacade and does not know the legacy 4107 presentation ID
- Surface buttons -> `router.setCanonicalSurfaceMode`

Do not add new Axiom buttons solely because the router now supports AxiomDebugControl; PX0-05 owns capability migration/presentation.

## Windows/Web composition

Each Debug UI composition owns one `DebugActivityLog` and one `DebugControlRouter`.

Capture/publish flow:

```text
snapshot = assembler.capture(sources including &activityLog)
router.beginFrame(snapshot)
router.refreshReceipts()
snapshot.activity = activityLog.snapshot()
publish/render snapshot
```

Do not perform another assembler/provider capture merely to refresh receipts.

Windows may retain legacy Runtime/diagnostic/debug-owner setter members on WindowsDebugUiHost until PX0-06, but `buildImGuiPanels` itself must receive only the router for mutation.

Web has product RuntimeFacade but currently no AxiomDebugControl or PlatformDebugControl. Construct its router with null engineering owners. Unsupported/unavailable engineering attempts must remain explicit rather than falling back to host mutation.

## Generic queue retirement

Delete only:

- `debug_ui/include/canvas/debug_ui/command.hpp`
- `debug_ui/src/command.cpp`

Remove the corresponding DebugController queue API/tests/CMake source.

Preserve:

- `debug_ui/include/canvas/debug_ui/debug_command_queue.hpp`
- `debug_ui/include/canvas/debug_ui/surface_debug_queue.hpp`

Those are owner-side safe-point queues and are not the generic UI queue being retired.

## First incomplete action

Run repository/package/anchor/binding preflights and the ImplementationDesignPreflight. Then add the focused PX0ControlRouter RED target before changing production code. Continue through the full closure contract unless a terminal blocker is found.
