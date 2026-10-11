# PX0-06 - Host Convergence + Temporary Toolbar Cleanup

PX0-06 starts after accepted PX0-05 closure.

- repository: `github:Mostorm-Labs/axiom`
- accepted PX0-05 code: `1e21856316ea93de3a1782f5cbab3d0291d81ded`
- PX0-05 evidence head: `b855df91de34ef66ec876885c2c665ca9f820af7`
- PX0-05 P34 closure / task anchor: `9d771820f25b98b6922663eed53f58abc0088a8c`

## Current Windows reality

`State` still owns:

```text
RuntimeFacade + diagnostics providers + Axiom/Platform debug owners
DebugActivityLog
DebugControlRouter
DebugSnapshotAssembler
DebugSnapshot channel
WindowsDebugUiHost
```

`captureDebugSnapshot(State&)` still assembles/polls/publishes a snapshot in Windows main.

`WindowsDebugUiHost` still has setters/members for RuntimeFacade, diagnostics, telemetry, debug owners, router and a snapshot-refresh callback. Its render path still calls `buildImGuiPanels(...)` directly.

The Windows temporary toolbar helper `selectWindowsTool` still mutates `InkPlaygroundHost` directly. The concrete `WindowsRuntimeFacade` also applies Host mutations, so PX0-06 must separate the toolbar adapter from the owner-side Host application path to avoid recursion.

Target:

```text
Windows composition root
  RuntimeFacade / diagnostics / debug owners
             |
       DebugController
             |
  WindowsDebugUiHost
  (ImGui/Skia/input/presentation only)
```

The native toolbar remains visible:

```text
toolbar command
  -> RuntimeFacade setBrush / setEraser / setSelectionMode
  -> concrete RuntimeFacade applies Host mutation
  -> Runtime state/readback updates button presentation
```

## Current Web reality

`DebugUiState` still owns:

```text
WebRuntimeFacade
DebugSnapshotAssembler
DebugActivityLog
DebugControlRouter
platform/telemetry/arc providers
selectedTool presentation state
```

`captureDebugSnapshot(...)` and `buildImGuiPanels(...)` remain Web-local Debug UI orchestration.

The exported temporary toolbar functions still call `Host::selectBrushProfile` / `Host::selectTool` directly.

Target:

```text
per-host Web RuntimeFacade product adapter
        |                    |
temporary toolbar      DebugController
                             |
                     DebugUiState platform
                     realization/rendering
```

The RuntimeFacade adapter must not exist only as a side effect of Debug UI visibility; the HTML toolbar is a product-control path and must continue to work when the Debug UI canvas is hidden.

## Preserved behavior

- Windows native toolbar stays visible.
- Web `brushBar/toolPalette` stays visible.
- Windows/Web Debug UI show/hide/input mapping stays.
- Windows shared InputCaptureGate stays.
- Web debug canvas and WebGL provider stay.
- Same-dispatch redraw after a Debug UI product-control click stays.
- G4.7 text qualification menus/actions remain qualification-only behavior.
- PX0-05 panel capability inventory is frozen.
- Physical qualification remains PX0-07.

## First incomplete action

Run repository/package/binding preflight and ImplementationDesignPreflight. Update Windows/Web structural contracts first and observe the required RED. Then execute Task 13 and Task 14 through all frozen focused/native/Web build evidence. Do not return after Windows-only convergence; both platform paths are one PX0-06 closure package.
